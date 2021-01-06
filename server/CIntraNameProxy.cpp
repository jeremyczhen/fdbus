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
#include <utils/CNsConfig.h>
#include <utils/Log.h>

CIntraNameProxy::CIntraNameProxy()
    : mConnectTimer(this)
    , mNotificationCenter(this)
    , mEnableReconnectToNS(true)
{
    mName  = std::to_string(CBaseThread::getPid());
    mName += "(local)";
    worker(FDB_CONTEXT);
    mConnectTimer.attach(FDB_CONTEXT, false);
}

bool CIntraNameProxy::connectToNameServer()
{
    if (connected())
    {
        return true;
    }
    const char *url;
#ifdef __WIN32__
    std::string std_url;
    CBaseSocketFactory::buildUrl(std_url, FDB_LOCAL_HOST,
                                 CNsConfig::getNameServerTcpPort());
    url = std_url.c_str();
#else
    url = CNsConfig::getNameServerIpcUrl();
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

void CIntraNameProxy::removeAddressListener(const char *svc_name)
{
    if (connected())
    {
        unsubscribeListener(NFdbBase::NTF_MORE_ADDRESS, svc_name);
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
    CFdbParcelableBuilder builder(msg_svc_name);
    invoke(NFdbBase::REQ_ALLOC_SERVICE_ADDRESS, builder);
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
    for (auto it = addr_tbl.begin(); it != addr_tbl.end(); ++it)
    {
        addr_list.add_address_list(*it);
    }

    CFdbParcelableBuilder builder(addr_list);
    send(NFdbBase::REQ_REGISTER_SERVICE, builder);
}

void CIntraNameProxy::unregisterService(const char *svc_name)
{
    if (!connected())
    {
        return;
    }
    NFdbBase::FdbMsgServerName msg_svc_name;
    msg_svc_name.set_name(svc_name);
    CFdbParcelableBuilder builder(msg_svc_name);
    send(NFdbBase::REQ_UNREGISTER_SERVICE, builder);
}

void CIntraNameProxy::processClientOnline(CFdbMessage *msg, NFdbBase::FdbMsgAddressList &msg_addr_list, bool force_reconnect)
{
    auto svc_name = msg_addr_list.service_name().c_str();
    const std::string &host_name = msg_addr_list.host_name();
    std::vector<CBaseEndpoint *> endpoints;
    bool is_offline = msg_addr_list.address_list().empty();
    
    FDB_CONTEXT->findEndpoint(svc_name, endpoints, false);
    for (auto ep_it = endpoints.begin(); ep_it != endpoints.end(); ++ep_it)
    {
        auto client = fdb_dynamic_cast_if_available<CBaseClient *>(*ep_it);
        if (!client)
        {
            LOG_E("CIntraNameProxy: Session %d: Fail to convert to CBaseEndpoint!\n", msg->session());
            continue;
        }

        if (is_offline)
        {
            if (client->hostConnected(host_name.c_str()))
            {
                // only disconnect the connected server (host name should match)
                client->doDisconnect();
                LOG_E("CIntraNameProxy: Session %d: Client %s is disconnected by %s!\n",
                        msg->session(), svc_name, host_name.c_str());
            }
            else if (client->connected())
            {
                LOG_I("CIntraNameProxy: Session %d: Client %s ignore disconnect from %s.\n",
                        msg->session(), svc_name, host_name.c_str());
            }
        }
        else
        {
            if (client->connectionEnabled(msg_addr_list))
            {
                if (force_reconnect)
                {
                    client->doDisconnect();
                }

                if (msg_addr_list.has_token_list() && client->importTokens(msg_addr_list.token_list().tokens()))
                {
                    client->updateSecurityLevel();
                    LOG_I("CIntraNameProxy: tokens of client %s are updated.\n", client->name().c_str());
                }

                client->local(msg_addr_list.is_local());

                replaceSourceUrl(msg_addr_list, FDB_CONTEXT->getSession(msg->session()));
                const auto &addr_list = msg_addr_list.address_list();
                for (auto it = addr_list.pool().begin(); it != addr_list.pool().end(); ++it)
                {
                    if (!client->doConnect(it->c_str(), host_name.c_str()))
                    {
                        LOG_E("CIntraNameProxy: Session %d: Fail to connect to %s!\n", msg->session(), it->c_str());
                        //TODO: do something for me!
                    }
                    else
                    {
                        LOG_E("CIntraNameProxy: Session %d, Server: %s, address %s is connected.\n",
                                msg->session(), svc_name, it->c_str());
                        // only connect to the first url of the same server.
                        break;
                    }
                }
            }
        }
    }
}

void CIntraNameProxy::onBroadcast(CBaseJob::Ptr &msg_ref)
{
    auto msg = castToMessage<CFdbMessage *>(msg_ref);
    switch (msg->code())
    {
        case NFdbBase::NTF_SERVICE_ONLINE:
        {
            NFdbBase::FdbMsgAddressList msg_addr_list;
            CFdbParcelableParser parser(msg_addr_list);
            if (!msg->deserialize(parser))
            {
                return;
            }
            processClientOnline(msg, msg_addr_list, true);
        }
        break;
        case NFdbBase::NTF_MORE_ADDRESS:
        {
            NFdbBase::FdbMsgAddressList msg_addr_list;
            CFdbParcelableParser parser(msg_addr_list);
            if (!msg->deserialize(parser))
            {
                return;
            }
            processServiceOnline(msg, msg_addr_list, false);
        }
        break;
        case NFdbBase::NTF_HOST_INFO:
        {
            NFdbBase::FdbMsgHostInfo msg_host_info;
            CFdbParcelableParser parser(msg_host_info);
            if (!msg->deserialize(parser))
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

void CIntraNameProxy::processServiceOnline(CFdbMessage *msg, NFdbBase::FdbMsgAddressList &msg_addr_list, bool force_reconnect)
{
    auto svc_name = msg_addr_list.service_name().c_str();
    std::vector<CBaseEndpoint *> endpoints;
    NFdbBase::FdbMsgAddrBindResults bound_list;

    bound_list.service_name(msg_addr_list.service_name());
    FDB_CONTEXT->findEndpoint(svc_name, endpoints, true);
    for (auto ep_it = endpoints.begin(); ep_it != endpoints.end(); ++ep_it)
    {
        auto server = fdb_dynamic_cast_if_available<CBaseServer *>(*ep_it);
        if (!server)
        {
            LOG_E("CIntraNameProxy: session %d: Fail to convert to CIntraNameProxy!\n", msg->session());
            continue;
        }

        if (msg_addr_list.has_token_list() && server->importTokens(msg_addr_list.token_list().tokens()))
        {
            server->updateSecurityLevel();
            LOG_I("CIntraNameProxy: tokens of server %s are updated.\n", server->name().c_str());
        }

        const auto &addr_list = msg_addr_list.address_list();
        if (force_reconnect)
        {
            server->doUnbind();
        }
        for (auto it = addr_list.pool().begin(); it != addr_list.pool().end(); ++it)
        {
            auto *addr_status = bound_list.add_address_list();
            addr_status->request_address(*it);
            CServerSocket *sk = server->doBind(it->c_str());
            if (!sk)
            {
                continue;
            }
            CFdbSocketInfo info;
            CFdbSocketAddr addr;
            std::string url;
            auto char_url = it->c_str();
            sk->getSocketInfo(info);
            CBaseSocketFactory::parseUrl(it->c_str(), addr);
            if (info.mAddress->mType == FDB_SOCKET_TCP)
            {
                if (addr.mPort == info.mAddress->mPort)
                {
                    addr_status->bind_address(*it);
                }
                else
                {
                    if (addr.mPort)
                    {
                        LOG_W("CIntraNameProxy: session %d: Server: %s, port %d is intended but get %d.\n",
                                msg->session(), svc_name, addr.mPort, info.mAddress->mPort);
                    }
                    CBaseSocketFactory::buildUrl(url, addr.mAddr.c_str(), info.mAddress->mPort);
                    addr_status->bind_address(url);
                    char_url = url.c_str();
                }
            }
            else
            {
                addr_status->bind_address(*it);
            }

            LOG_I("CIntraNameProxy: session %d: Server: %s, address %s is bound.\n",
                    msg->session(), svc_name, char_url);
        }
    }

    CFdbParcelableBuilder builder(bound_list);
    send(msg->session(), NFdbBase::REQ_REGISTER_SERVICE, builder);
}

void CIntraNameProxy::onReply(CBaseJob::Ptr &msg_ref)
{
    auto msg = castToMessage<CFdbMessage *>(msg_ref);
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
            CFdbParcelableParser parser(msg_addr_list);
            if (!msg->deserialize(parser))
            {
                LOG_E("CIntraNameProxy: unable to decode message for REQ_ALLOC_SERVICE_ADDRESS!\n");
                return;
            }
            processServiceOnline(msg, msg_addr_list, true);
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
    
    FDB_CONTEXT->reconnectOnNsConnected();
}

void CIntraNameProxy::onOffline(FdbSessionId_t sid, bool is_last)
{
    if (mEnableReconnectToNS)
    {
        mConnectTimer.fire();
    }
}

void CIntraNameProxy::registerHostNameReadyNotify(CBaseNotification<CHostNameReady> *notification)
{
    CBaseNotification<CHostNameReady>::Ptr ntf(notification);
    mNotificationCenter.subscribe(ntf);
}

