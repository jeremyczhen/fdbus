/*
 * Copyright (C) 2015   Jeremy Chen jeremy_cz@yahoo.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <common_base/CIntraNameProxy.h>
#include <common_base/CFdbContext.h>
#include <common_base/CFdbMessage.h>
#include <common_base/CBaseServer.h>
#include <common_base/CBaseSocketFactory.h>
#include <idl-gen/common.base.MessageHeader.pb.h>
#include "CNsConfig.h"
#include <utils/Log.h>

bool CIntraNameProxy::connectToNameServer()
{
    if (connected())
    {
        return true;
    }
    const char *url;
#ifdef __WIN32__
    std::string std_url;
    CBaseSocketFactory::buildUrl(std_url,
                                 FDB_SOCKET_TCP,
                                 FDB_LOCAL_HOST,
                                 CNsConfig::getNameServerTcpPort());
    url = std_url.c_str();
#else
    url = CNsConfig::getNameServerIpcPath();
#endif
    // timer will stop if connected since onOnline() will be called
    // upon success
    doConnect(url);
    if (!connected())
    {
        mConnectTimer.fire();
        return false;
    }

    return true;
}

CIntraNameProxy::CConnectTimer::CConnectTimer(CIntraNameProxy *proxy)
    : CMethodLoopTimer<CIntraNameProxy>(CNsConfig::getNsReconnectInterval(), false,
                                   proxy, &CIntraNameProxy::onConnectTimer)
{
}

void CIntraNameProxy::CConnectTimer::fire()
{
    enableOneShot(CNsConfig::getNsReconnectInterval());
}


CIntraNameProxy::CIntraNameProxy()
    : mConnectTimer(this)
    , mNotificationCenter(this)
    , mEnableReconnectToNS(true)
{
    mConnectTimer.attach(FDB_CONTEXT, false);
}

void CIntraNameProxy::addServiceListener(const char *svc_name, FdbSessionId_t subscriber)
{
    if (connected())
    {
        subscribeListener(NFdbBase::NTF_SERVICE_ONLINE, svc_name);
    }
}

void CIntraNameProxy::removeServiceListener(const char *svc_name)
{
    if (connected())
    {
        unsubscribeListener(NFdbBase::NTF_SERVICE_ONLINE, svc_name);
    }
}

void CIntraNameProxy::addAddressListener(const char *svc_name)
{
    if (connected())
    {
        subscribeListener(NFdbBase::NTF_MORE_ADDRESS, svc_name);
    }
}

void CIntraNameProxy::registerService(const char *svc_name)
{
    if (!connected())
    {
        return;
    }
    NFdbBase::FdbMsgServerName msg_svc_name;
    msg_svc_name.set_name(svc_name);
    invoke(NFdbBase::REQ_ALLOC_SERVICE_ADDRESS, msg_svc_name);
}

void CIntraNameProxy::registerService(const char *svc_name, std::vector<std::string> &addr_tbl)
{
    if (!connected() || addr_tbl.empty())
    {
        return;
    }

    NFdbBase::FdbMsgAddressList addr_list;
    addr_list.set_service_name(svc_name);
    addr_list.set_host_name("");
    addr_list.set_is_local(true);
    for (std::vector<std::string>::iterator it = addr_tbl.begin(); it != addr_tbl.end(); ++it)
    {
        addr_list.add_address_list(*it);
    }
    
    send(NFdbBase::REQ_REGISTER_SERVICE, addr_list);
}

void CIntraNameProxy::unregisterService(const char *svc_name)
{
    if (!connected())
    {
        return;
    }
    NFdbBase::FdbMsgServerName msg_svc_name;
    msg_svc_name.set_name(svc_name);
    send(NFdbBase::REQ_UNREGISTER_SERVICE, msg_svc_name);
}

void CIntraNameProxy::processClientOnline(CFdbMessage *msg, NFdbBase::FdbMsgAddressList &msg_addr_list, bool force_reconnect)
{
    const char *svc_name = msg_addr_list.service_name().c_str();
    const std::string &host_name = msg_addr_list.host_name();
    std::vector<CBaseEndpoint *> endpoints;
    bool is_offline = msg_addr_list.address_list().empty();
    
    FDB_CONTEXT->findEndpoint(svc_name, endpoints, false);
    for (std::vector<CBaseEndpoint *>::iterator ep_it = endpoints.begin();
            ep_it != endpoints.end(); ++ep_it)
    {
        CBaseClient *client = dynamic_cast<CBaseClient *>(*ep_it);
        if (!client)
        {
            LOG_E("CIntraNameProxy: Session %d: Fail to convert to CBaseEndpoint!\n", msg->session());
            continue;
        }

        if (client->mNsConnStatus == DISCONNECTED)
        {
            continue;
        }
        std::string &connected_host = client->mConnectedHost;

        if (is_offline)
        {
            if (host_name == connected_host)
            {
                // only disconnect the connected server (host name should match)
                client->doDisconnect();
                LOG_E("CIntraNameProxy: Session %d: Client %s is disconnected by %s!\n",
                        msg->session(), svc_name, host_name.c_str());
                connected_host.clear();
            }
            else
            {
                if (connected_host.empty())
                {
                    if (client->connected())
                    {
                        // This should never happen!!!
                        client->doDisconnect();
                        LOG_E("CIntraNameProxy: Session %d: Client %s: host name is cleared but still connected!",
                               msg->session(), svc_name);
                    }
                }
                else
                {
                    LOG_I("CIntraNameProxy: Session %d: Client %s ignore disconnect from %s.\n",
                            msg->session(), svc_name, host_name.c_str());
                }
            }
        }
        else
        {
            if (client->connectionEnabled(msg_addr_list))
            {
                if (force_reconnect)
                {
                    client->doDisconnect();
                    connected_host.clear();
                }

                if (msg_addr_list.has_token_list() && client->importTokens(msg_addr_list.token_list().tokens()))
                {
                    client->updateSecurityLevel();
                    LOG_I("CIntraNameProxy: tokens of client %s are updated.\n", client->name().c_str());
                }

                client->local(msg_addr_list.is_local());

                replaceSourceUrl(msg_addr_list, FDB_CONTEXT->getSession(msg->session()));
                const ::google::protobuf::RepeatedPtrField< ::std::string> &addr_list =
                    msg_addr_list.address_list();
                for (::google::protobuf::RepeatedPtrField< ::std::string>::const_iterator it = addr_list.begin();
                        it != addr_list.end(); ++it)
                {
                    if (!client->doConnect(it->c_str()))
                    {
                        LOG_E("CIntraNameProxy: Session %d: Fail to connect to %s!\n", msg->session(), it->c_str());
                        //TODO: do something for me!
                    }
                    else
                    {
                        LOG_E("CIntraNameProxy: Session %d, Server: %s, address %s is connected.\n",
                                msg->session(), svc_name, it->c_str());
                        client->mNsConnStatus = CONNECTED;
                        // only connect to the first url of the same server.
                        break;
                    }
                }
                if (client->connected())
                {
                    connected_host = host_name;
                }
            }
        }
    }
}

void CIntraNameProxy::onBroadcast(CBaseJob::Ptr &msg_ref)
{
    CFdbMessage *msg = castToMessage<CFdbMessage *>(msg_ref);
    switch (msg->code())
    {
        case NFdbBase::NTF_SERVICE_ONLINE:
        {
            NFdbBase::FdbMsgAddressList msg_addr_list;
            if (!msg->deserialize(msg_addr_list))
            {
                return;
            }
            processClientOnline(msg, msg_addr_list, !msg->isInitialResponse());
        }
        break;
        case NFdbBase::NTF_MORE_ADDRESS:
        {
            NFdbBase::FdbMsgAddressList msg_addr_list;
            if (!msg->deserialize(msg_addr_list))
            {
                return;
            }
            processServiceOnline(msg, msg_addr_list);
        }
        break;
        case NFdbBase::NTF_HOST_INFO:
        {
            NFdbBase::FdbMsgHostInfo msg_host_info;
            if (!msg->deserialize(msg_host_info))
            {
                return;
            }
            mHostName = msg_host_info.name();
            CHostNameReady name_ready(mHostName);
            mNotificationCenter.notify(name_ready);
        }
        break;
        default:
        break;
    }
}

void CIntraNameProxy::processServiceOnline(CFdbMessage *msg, NFdbBase::FdbMsgAddressList &msg_addr_list)
{
    const char *svc_name = msg_addr_list.service_name().c_str();
    std::vector<CBaseEndpoint *> endpoints;
    NFdbBase::FdbMsgAddressList bound_list;

    bound_list.set_service_name(msg_addr_list.service_name());
    bound_list.set_host_name(msg_addr_list.host_name());
    bound_list.set_is_local(msg_addr_list.is_local());
    FDB_CONTEXT->findEndpoint(svc_name, endpoints, true);
    for (std::vector<CBaseEndpoint *>::iterator ep_it = endpoints.begin();
            ep_it != endpoints.end(); ++ep_it)
    {
        CBaseServer *server = dynamic_cast<CBaseServer *>(*ep_it);
        if (!server)
        {
            LOG_E("CIntraNameProxy: session %d: Fail to convert to CIntraNameProxy!\n", msg->session());
            continue;
        }
        if (server->mNsConnStatus == DISCONNECTED)
        {
            continue;
        }
        if (msg_addr_list.has_token_list() && server->importTokens(msg_addr_list.token_list().tokens()))
        {
            server->updateSecurityLevel();
            LOG_I("CIntraNameProxy: tokens of server %s are updated.\n", server->name().c_str());
        }

        int32_t retries = CNsConfig::getAddressBindRetryNr();
        const ::google::protobuf::RepeatedPtrField< ::std::string> &addr_list =
            msg_addr_list.address_list();
        for (::google::protobuf::RepeatedPtrField< ::std::string>::const_iterator it = addr_list.begin();
                it != addr_list.end(); ++it)
        {
            do
            {
                CServerSocket *sk = server->doBind(it->c_str());
                if (sk)
                {
                    CFdbSocketInfo info;
                    CFdbSocketAddr addr;
                    std::string url;
                    const char *char_url = it->c_str();
                    sk->getSocketInfo(info);
                    CBaseSocketFactory::parseUrl(it->c_str(), addr);
                    if (info.mAddress->mType == FDB_SOCKET_TCP)
                    {
                        if (addr.mPort == info.mAddress->mPort)
                        {
                            bound_list.add_address_list(*it);
                        }
                        else
                        {
                            if (addr.mPort)
                            {
                                LOG_W("CIntraNameProxy: session %d: Server: %s, port %d is intended but get %d.\n",
                                        msg->session(), svc_name, addr.mPort, info.mAddress->mPort);
                            }
                            CBaseSocketFactory::buildUrl(url, FDB_SOCKET_TCP,
                                        addr.mAddr.c_str(), info.mAddress->mPort);
                            bound_list.add_address_list(url);
                            char_url = url.c_str();
                        }
                    }
                    else
                    {
                        bound_list.add_address_list(*it);
                    }

                    LOG_I("CIntraNameProxy: session %d: Server: %s, address %s is bound.\n",
                            msg->session(), svc_name, char_url);
                    server->mNsConnStatus = CONNECTED;
                    break;
                }
                else
                {
#if 0
                    LOG_E("CIntraNameProxy: session %d: Fail to bind to %s! Reconnecting...\n", msg->session(), it->c_str());
#endif
                    sysdep_sleep(CNsConfig::getAddressBindRetryInterval());
                }
            } while (--retries > 0);
        }
    }

    send(msg->session(), NFdbBase::REQ_REGISTER_SERVICE, bound_list);
}

void CIntraNameProxy::onReply(CBaseJob::Ptr &msg_ref)
{
    CFdbMessage *msg = castToMessage<CFdbMessage *>(msg_ref);
    if (msg->isStatus())
    {
        if (msg->isError())
        {
            int32_t id;
            std::string reason;
            msg->decodeStatus(id, reason);
            LOG_I("CIntraNameProxy: status is received: msg code: %d, id: %d, reason: %s\n",
                    msg->code(), id, reason.c_str());
        }

        return;
    }

    switch (msg->code())
    {
        case NFdbBase::REQ_ALLOC_SERVICE_ADDRESS:
        {
            NFdbBase::FdbMsgAddressList msg_addr_list;
            if (!msg->deserialize(msg_addr_list))
            {
                LOG_E("CIntraNameProxy: unable to decode message for REQ_ALLOC_SERVICE_ADDRESS!\n");
                return;
            }
            processServiceOnline(msg, msg_addr_list);
        }
        break;
        default:
        break;
    }
}

void CIntraNameProxy::onConnectTimer(CMethodLoopTimer<CIntraNameProxy> *timer)
{
#if 0
    LOG_E("CIntraNameProxy: Reconnecting to name server...\n");
#endif
    if (mEnableReconnectToNS)
    {
        connectToNameServer();
    }
}

void CIntraNameProxy::onOnline(FdbSessionId_t sid, bool is_first)
{
    mConnectTimer.disable();
    
    CFdbMsgSubscribeList subscribe_list;
    addNotifyItem(subscribe_list, NFdbBase::NTF_HOST_INFO);
    subscribe(subscribe_list);
    
    FDB_CONTEXT->reconnectOnNsConnected(true);
}

void CIntraNameProxy::onOffline(FdbSessionId_t sid, bool is_last)
{
    if (mEnableReconnectToNS)
    {
        mConnectTimer.fire();
        FDB_CONTEXT->reconnectOnNsConnected(false);
    }
}

void CIntraNameProxy::registerHostNameReadyNotify(CBaseNotification<CHostNameReady> *notification)
{
    CBaseNotification<CHostNameReady>::Ptr ntf(notification);
    mNotificationCenter.subscribe(ntf);
}

void CIntraNameProxy::disconnect()
{
    mEnableReconnectToNS = false;
    CBaseClient::disconnect();
}

