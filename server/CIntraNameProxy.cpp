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
#include <common_base/CFdbSession.h>
#include <utils/CNsConfig.h>
#include <utils/Log.h>

CIntraNameProxy::CIntraNameProxy()
    : mConnectTimer(this)
    , mNotificationCenter(this)
    , mEnableReconnectToNS(true)
{
    mName  = std::to_string(CBaseThread::getPid());
    mName += "-nsproxy(local)";
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
                                 CNsConfig::getNameServerTCPPort());
    url = std_url.c_str();
#else
    url = CNsConfig::getNameServerIPCUrl();
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

void CIntraNameProxy::addServiceListener(const char *svc_name)
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

void CIntraNameProxy::processClientOnline(CFdbMessage *msg, NFdbBase::FdbMsgAddressList &msg_addr_list)
{
    auto svc_name = msg_addr_list.service_name().c_str();
    const std::string &host_name = msg_addr_list.host_name();
    std::vector<CBaseEndpoint *> endpoints;
    std::vector<CBaseEndpoint *> failures;
    bool is_offline = msg_addr_list.address_list().empty();
    int32_t success_count = 0;
    int32_t failure_count = 0;
    
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
                if (msg_addr_list.has_token_list() && client->importTokens(msg_addr_list.token_list().tokens()))
                {
                    client->updateSecurityLevel();
                    LOG_I("CIntraNameProxy: tokens of client %s are updated.\n", client->name().c_str());
                }

                client->local(msg_addr_list.is_local());

                // for client, size of addr_list is always 1, which is ensured by name server
                replaceSourceUrl(msg_addr_list, FDB_CONTEXT->getSession(msg->session()));
                auto &addr_list = msg_addr_list.address_list();
                for (auto it = addr_list.vpool().begin(); it != addr_list.vpool().end(); ++it)
                {
                    int32_t udp_port = FDB_INET_PORT_INVALID;
                    if (it->has_udp_port())
                    {
                        udp_port = it->udp_port();
                    }

                    auto session_container = client->doConnect(it->tcp_ipc_url().c_str(),
                                                               host_name.c_str(), udp_port);
                    if (session_container)
                    {
                        if (client->UDPEnabled() && (it->address_type() != FDB_SOCKET_IPC)
                            && (udp_port > FDB_INET_PORT_NOBIND))
                        {
                            CFdbSocketInfo socket_info;
                            if (!session_container->getUDPSocketInfo(socket_info) ||
                                (!FDB_VALID_PORT(socket_info.mAddress->mPort)))
                            {
                                failure_count++;
                            }
                            else
                            {
                                success_count++;
                            }
                        }
                        LOG_E("CIntraNameProxy: Session %d, Server: %s, address %s is connected.\n",
                                msg->session(), svc_name, it->tcp_ipc_url().c_str());
                        // only connect to the first url of the same server.
                        break;
                    }
                    else
                    {
                        LOG_E("CIntraNameProxy: Session %d: Fail to connect to %s!\n",
                                msg->session(), it->tcp_ipc_url().c_str());
                        //TODO: do something for me!
                    }
                }
            }
        }
    }
    if (!is_offline && failure_count)
    {
        // msg->isInitialResponse() true: client starts after server; UDP port will be
        //                                allocated for each client
        //                          false: client starts before server; only one UDP port
        //                                is allocated. Subsequent allocation shall be
        //                                triggered manually
        // if client starts before server, once server appears, only one event is
        // received by client process. If more than one client connects to the same
        // server in the same process, shall ask name server to allocate UDP port
        // for each client. A calling to addServiceListener() will lead to a
        // broadcast of server address with isInitialResponse() being true. Only
        // UDP port will be valid and TCP address will be discussed in subsequent broadcast.
        int32_t nr_request = 0;
        if (!msg->isInitialResponse())
        {   // client starts before server and at least one fails to bind UDP: In this case
            // we should send requests of the same number as failures in the hope that we
            // can get enough response with new UDP port ID
            nr_request = failure_count;
        }
        else if (!(msg->isInitialResponse() && success_count))
        {   // client starts after server and none success to bind UDP: in this case only
            // one request is needed since there are several other requesting on-going
            nr_request = 1;
        }
        for (int32_t i = 0; i < nr_request; ++i)
        {
            LOG_E("CIntraNameProxy: Session %d, Server: %s: requesting next UDP...\n",
                    msg->session(), svc_name);
            addServiceListener(svc_name);
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
            processClientOnline(msg, msg_addr_list);
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

        int32_t retries = CNsConfig::getAddressBindRetryNr();
        auto &addr_list = msg_addr_list.address_list();
        if (force_reconnect)
        {
            server->doUnbind();
        }
        for (auto it = addr_list.vpool().begin(); it != addr_list.vpool().end(); ++it)
        {
            auto *addr_status = bound_list.add_address_list();
            const std::string &tcp_ipc_url = it->tcp_ipc_url();
            addr_status->request_address(tcp_ipc_url);
            do
            {
                int32_t udp_port = FDB_INET_PORT_INVALID;
                if (it->has_udp_port())
                {
                    udp_port = it->udp_port();
                }
                CServerSocket *sk = server->doBind(tcp_ipc_url.c_str(), udp_port);
                if (sk)
                {
                    CFdbSocketInfo info;
                    std::string url;
                    auto char_url = tcp_ipc_url.c_str();
                    sk->getSocketInfo(info);
                    if (it->address_type() == FDB_SOCKET_TCP)
                    {
                        if (it->tcp_port() == info.mAddress->mPort)
                        {
                            addr_status->bind_address(tcp_ipc_url);
                        }
                        else
                        {
                            if (it->tcp_port())
                            {
                                LOG_W("CIntraNameProxy: session %d: Server: %s, port %d is intended but get %d.\n",
                                        msg->session(), svc_name, it->tcp_port(), info.mAddress->mPort);
                            }
                            CBaseSocketFactory::buildUrl(url, it->tcp_ipc_address().c_str(), info.mAddress->mPort);
                            addr_status->bind_address(url);
                            char_url = url.c_str();
                        }
                    }
                    else
                    {
                        addr_status->bind_address(tcp_ipc_url);
                    }

                    LOG_I("CIntraNameProxy: session %d: Server: %s, address %s is bound.\n",
                            msg->session(), svc_name, char_url);
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

