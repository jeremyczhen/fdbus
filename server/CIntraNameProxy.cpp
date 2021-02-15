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

#include "CIntraNameProxy.h"
#include <common_base/CFdbContext.h>
#include <common_base/CFdbMessage.h>
#include <common_base/CBaseServer.h>
#include <common_base/CBaseSocketFactory.h>
#include <utils/CNsConfig.h>
#include <utils/Log.h>
#include <fdbus/CFdbWatchdog.h>

#define FDB_MSG_TYPE_NSP_SUBSCRIBE (FDB_MSG_TYPE_SYSTEM - 1)

class CNSProxyMsg : public CFdbMessage
{
public:
    CNSProxyMsg(FdbContextId_t ctx_id)
        : CFdbMessage()
        , mCtxId(ctx_id)
    {
    }
    CNSProxyMsg(FdbMsgCode_t code, FdbContextId_t ctx_id)
        : CFdbMessage(code)
        , mCtxId(ctx_id)
    {
    }
    CNSProxyMsg(NFdbBase::CFdbMessageHeader &head
                      , CFdbMsgPrefix &prefix
                      , uint8_t *buffer
                      , FdbSessionId_t sid
                      , FdbContextId_t ctx_id)
        : CFdbMessage(head, prefix, buffer, sid)
        , mCtxId(ctx_id)
    {
    }
    FdbMessageType_t getTypeId()
    {
        return FDB_MSG_TYPE_NSP_SUBSCRIBE;
    }

    FdbContextId_t mCtxId;
protected:
    CFdbMessage *clone(NFdbBase::CFdbMessageHeader &head
                      , CFdbMsgPrefix &prefix
                      , uint8_t *buffer
                      , FdbSessionId_t sid)
    {
        return new CNSProxyMsg(head, prefix, buffer, sid, mCtxId);
    }
};

CIntraNameProxy::CIntraNameProxy()
    : mConnectTimer(this)
    , mNotificationCenter(this)
    , mEnableReconnectToNS(true)
    , mNsWatchdogListener(0)
{
    mName  = std::to_string(CBaseThread::getPid());
    mName += "-nsproxy(local)";
    worker(mContext);
    mConnectTimer.attach(mContext, false);
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

void CIntraNameProxy::addServiceListener(const char *svc_name, CFdbBaseContext *context)
{
    if (connected())
    {
        CFdbMsgSubscribeList subscribe_list;
        addNotifyItem(subscribe_list, NFdbBase::NTF_SERVICE_ONLINE, svc_name);
        subscribe(subscribe_list, new CNSProxyMsg(context->ctxId()));
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

void CIntraNameProxy::registerService(const char *svc_name, CFdbBaseContext *context)
{
    if (!connected())
    {
        return;
    }
    CFdbMessage *msg = new CNSProxyMsg(NFdbBase::REQ_ALLOC_SERVICE_ADDRESS, context->ctxId());
    NFdbBase::FdbMsgServerName msg_svc_name;
    msg_svc_name.set_name(svc_name);
    CFdbParcelableBuilder builder(msg_svc_name);
    invoke(msg, builder);
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

void CIntraNameProxy::doConnectToServer(CFdbBaseContext *context, NFdbBase::FdbMsgAddressList &msg_addr_list,
                                        bool is_init_response)
{
    auto svc_name = msg_addr_list.service_name().c_str();
    const std::string &host_name = msg_addr_list.host_name();
    bool is_offline = msg_addr_list.address_list().empty();
    int32_t success_count = 0;
    int32_t failure_count = 0;
    
    auto &container = context->getEndpoints().getContainer();
    for (auto ep_it = container.begin(); ep_it != container.end(); ++ep_it)
    {
        if (ep_it->second->role() != FDB_OBJECT_ROLE_CLIENT)
        {
            continue;
        }
        if (ep_it->second->nsName().compare(svc_name))
        {
            continue;
        }
        auto client = fdb_dynamic_cast_if_available<CBaseClient *>(ep_it->second);
        if (!client)
        {
            LOG_E("CIntraNameProxy: Fail to convert to CBaseEndpoint!\n");
            continue;
        }

        if (is_offline)
        {
            if (client->hostConnected(host_name.c_str()))
            {
                // only disconnect the connected server (host name should match)
                client->doDisconnect();
                LOG_E("CIntraNameProxy Client %s is disconnected by %s!\n", svc_name, host_name.c_str());
            }
            else if (client->connected())
            {
                LOG_I("CIntraNameProxy: Client %s ignore disconnect from %s.\n", svc_name, host_name.c_str());
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
                        LOG_E("CIntraNameProxy: Server: %s, address %s is connected.\n",
                                svc_name, it->tcp_ipc_url().c_str());
                        // only connect to the first url of the same server.
                        break;
                    }
                    else
                    {
                        LOG_E("CIntraNameProxy: Fail to connect to %s!\n", it->tcp_ipc_url().c_str());
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
        if (!is_init_response)
        {   // client starts before server and at least one fails to bind UDP: In this case
            // we should send requests of the same number as failures in the hope that we
            // can get enough response with new UDP port ID
            nr_request = failure_count;
        }
        else if (!(is_init_response && success_count))
        {   // client starts after server and none success to bind UDP: in this case only
            // one request is needed since there are several other requesting on-going
            nr_request = 1;
        }
        for (int32_t i = 0; i < nr_request; ++i)
        {
            LOG_E("CIntraNameProxy: Server: %s: requesting next UDP...\n", svc_name);
            addServiceListener(svc_name, context);
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
            FdbContextId_t ctx_id = FDB_INVALID_ID;
            if (msg->isInitialResponse() && (msg->getTypeId() == FDB_MSG_TYPE_NSP_SUBSCRIBE))
            {
                auto nsp_msg = castToMessage<CNSProxyMsg *>(msg_ref);
                ctx_id = nsp_msg->mCtxId;
            }
            connectToServer(msg, ctx_id);
        }
        break;
        case NFdbBase::NTF_MORE_ADDRESS:
        {
            bindAddress(msg, FDB_INVALID_ID);
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
        case NFdbBase::NTF_WATCHDOG:
        {
            if (!mNsWatchdogListener)
            {
                return;
            }
            CFdbMsgProcessList msg_process_list;
            CFdbParcelableParser parser(msg_process_list);
            if (!msg->deserialize(parser))
            {
                return;
            }
            auto &process_list = msg_process_list.process_list();
            tNsWatchdogList output_process_list;
            for (auto it = process_list.vpool().begin(); it != process_list.vpool().end(); ++it)
            {
                CNsWatchdogItem item(it->client_name().c_str(), it->pid());
                output_process_list.push_back(std::move(item));
            }
            mNsWatchdogListener(output_process_list);
        }
        break;
        default:
        break;
    }
}

void CIntraNameProxy::doBindAddress(CFdbBaseContext *context, NFdbBase::FdbMsgAddressList &msg_addr_list,
                                    bool force_rebind)
{
    auto svc_name = msg_addr_list.service_name().c_str();
    NFdbBase::FdbMsgAddrBindResults bound_list;

    bound_list.service_name(msg_addr_list.service_name());
    auto &container = context->getEndpoints().getContainer();
    for (auto ep_it = container.begin(); ep_it != container.end(); ++ep_it)
    {
        if (ep_it->second->role() != FDB_OBJECT_ROLE_SERVER)
        {
            continue;
        }
        if (ep_it->second->nsName().compare(svc_name))
        {
            continue;
        }
        auto server = fdb_dynamic_cast_if_available<CBaseServer *>(ep_it->second);
        if (!server)
        {
            LOG_E("CIntraNameProxy: Fail to convert to CIntraNameProxy!\n");
            continue;
        }

        if (msg_addr_list.has_token_list() && server->importTokens(msg_addr_list.token_list().tokens()))
        {
            server->updateSecurityLevel();
        }

        auto &addr_list = msg_addr_list.address_list();
        if (force_rebind)
        {
            server->doUnbind();
        }
        for (auto it = addr_list.vpool().begin(); it != addr_list.vpool().end(); ++it)
        {
            auto *addr_status = bound_list.add_address_list();
            const std::string &tcp_ipc_url = it->tcp_ipc_url();
            addr_status->request_address(tcp_ipc_url);
            int32_t udp_port = FDB_INET_PORT_INVALID;
            if (it->has_udp_port())
            {
                udp_port = it->udp_port();
            }
            CServerSocket *sk = server->doBind(tcp_ipc_url.c_str(), udp_port);
            if (!sk)
            {
                continue;
            }
            int32_t bound_port = FDB_INET_PORT_INVALID;
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
                        LOG_W("CIntraNameProxy: Server: %s, port %d is intended but get %d.\n",
                               svc_name, it->tcp_port(), info.mAddress->mPort);
                    }
                    CBaseSocketFactory::buildUrl(url, it->tcp_ipc_address().c_str(), info.mAddress->mPort);
                    addr_status->bind_address(url);
                    char_url = url.c_str();
                }
                if (server->UDPEnabled())
                {
                    CFdbSocketInfo socket_info;
                    if (sk->getUDPSocketInfo(socket_info) && FDB_VALID_PORT(socket_info.mAddress->mPort))
                    {
                        bound_port = socket_info.mAddress->mPort;
                    }
                }
            }
            else
            {
                addr_status->bind_address(tcp_ipc_url);
            }
            addr_status->set_udp_port(bound_port);

            if (FDB_VALID_PORT(bound_port))
            {
                LOG_I("CIntraNameProxy: Server: %s, address %s UDP %d is bound.\n",
                      svc_name, char_url, bound_port);
            }
            else
            {
                LOG_I("CIntraNameProxy: Server: %s, address %s is bound.\n",
                      svc_name, char_url);
            }
        }
    }

    CFdbParcelableBuilder builder(bound_list);
    send(NFdbBase::REQ_REGISTER_SERVICE, builder);
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
            auto nsp_msg = castToMessage<CNSProxyMsg *>(msg_ref);
            bindAddress(msg, nsp_msg->mCtxId);
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
    if (mNsWatchdogListener)
    {
        addNotifyItem(subscribe_list, NFdbBase::NTF_WATCHDOG);
    }
    subscribe(subscribe_list);
    
    queryServiceAddress();
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

class CRegisterWatchdogJob : public CBaseJob
{
public:
    CRegisterWatchdogJob(tNsWatchdogListenerFn &watchdog_listener)
        : CBaseJob(JOB_FORCE_RUN)
        , mWatchdogListener(watchdog_listener)
    {

    }
protected:
    void run(CBaseWorker *worker, Ptr &ref)
    {
        auto proxy = FDB_CONTEXT->getNameProxy();
        if (proxy)
        {
            proxy->doRegisterNsWatchdogListener(mWatchdogListener);
        }
    }
private:
    tNsWatchdogListenerFn mWatchdogListener;
};

void CIntraNameProxy::doRegisterNsWatchdogListener(tNsWatchdogListenerFn &watchdog_listener)
{
    mNsWatchdogListener = watchdog_listener;
    if (connected())
    {
        CFdbMsgSubscribeList subscribe_list;
        addNotifyItem(subscribe_list, NFdbBase::NTF_WATCHDOG);
        subscribe(subscribe_list);
    }
}

void CIntraNameProxy::registerNsWatchdogListener(tNsWatchdogListenerFn &watchdog_listener)
{
    if (watchdog_listener)
    {
        mContext->sendAsync(new CRegisterWatchdogJob(watchdog_listener));
    }
}

class CConnectToServerJob : public CMethodJob<CIntraNameProxy>
{
public:
    CConnectToServerJob(CIntraNameProxy *object, bool is_init_response)
        : CMethodJob<CIntraNameProxy>(object, &CIntraNameProxy::callConnectToServer, JOB_FORCE_RUN)
        , mIsInitResponse(is_init_response)
    {}

    NFdbBase::FdbMsgAddressList mAddressList;
    bool mIsInitResponse;
};

void CIntraNameProxy::callConnectToServer(CBaseWorker *worker,
            CMethodJob<CIntraNameProxy> *job, CBaseJob::Ptr &ref)
{
    auto the_job = fdb_dynamic_cast_if_available<CConnectToServerJob *>(job);
    auto *context = fdb_dynamic_cast_if_available<CFdbBaseContext *>(worker);
    doConnectToServer(context, the_job->mAddressList, the_job->mIsInitResponse);
}

void CIntraNameProxy::connectToServer(CFdbMessage *msg, FdbContextId_t ctx_id)
{
    auto session = msg->getSession();
    bool is_init_response = msg->isInitialResponse();
    // CIntraNameProxy should run at CFdbContext, not CFdbBaseContext
    auto default_context = fdb_dynamic_cast_if_available<CFdbContext *>(mContext);
    auto &container = default_context->getContexts().getContainer();
    for (auto it = container.begin(); it != container.end(); ++it)
    {
        if (fdbValidFdbId(ctx_id) && (ctx_id != it->first))
        {
            continue;
        }
        auto job = new CConnectToServerJob(this, is_init_response);
        CFdbParcelableParser parser(job->mAddressList);
        if (!msg->deserialize(parser))
        {
            return;
        }

        replaceSourceUrl(job->mAddressList, session);
        it->second->sendAsyncEndeavor(job);
    }
}

class CBindAddressJob : public CMethodJob<CIntraNameProxy>
{
public:
    CBindAddressJob(CIntraNameProxy *object, bool force_rebind)
        : CMethodJob<CIntraNameProxy>(object, &CIntraNameProxy::callBindAddress, JOB_FORCE_RUN)
        , mForceRebind(force_rebind)
    {}

    NFdbBase::FdbMsgAddressList mAddressList;
    bool mForceRebind;
};

void CIntraNameProxy::callBindAddress(CBaseWorker *worker,
            CMethodJob<CIntraNameProxy> *job, CBaseJob::Ptr &ref)
{
    auto the_job = fdb_dynamic_cast_if_available<CBindAddressJob *>(job);
    auto *context = fdb_dynamic_cast_if_available<CFdbBaseContext *>(worker);
    doBindAddress(context, the_job->mAddressList, the_job->mForceRebind);
}

void CIntraNameProxy::bindAddress(CFdbMessage *msg, FdbContextId_t ctx_id)
{
    bool force_rebind = fdbValidFdbId(ctx_id) ? true : false;
    // CIntraNameProxy should run at CFdbContext, not CFdbBaseContext
    auto default_context = fdb_dynamic_cast_if_available<CFdbContext *>(mContext);
    auto &container = default_context->getContexts().getContainer();
    for (auto it = container.begin(); it != container.end(); ++it)
    {
        if (fdbValidFdbId(ctx_id) && (ctx_id != it->first))
        {
            continue;
        }
        auto job = new CBindAddressJob(this, force_rebind);
        CFdbParcelableParser parser(job->mAddressList);
        if (!msg->deserialize(parser))
        {
            return;
        }

        it->second->sendAsyncEndeavor(job);
    }
}

class CQueryServiceAddressJob : public CMethodJob<CIntraNameProxy>
{
public:
    CQueryServiceAddressJob(CIntraNameProxy *object)
        : CMethodJob<CIntraNameProxy>(object, &CIntraNameProxy::callQueryServiceAddress, JOB_FORCE_RUN)
    {}
};

void CIntraNameProxy::callQueryServiceAddress(CBaseWorker *worker,
            CMethodJob<CIntraNameProxy> *job, CBaseJob::Ptr &ref)
{
    auto *context = fdb_dynamic_cast_if_available<CFdbBaseContext *>(worker);
    auto &container = context->getEndpoints().getContainer();
    for (auto it = container.begin(); it != container.end(); ++it)
    {
        auto endpoint = it->second;
        endpoint->requestServiceAddress();
    }
}

void CIntraNameProxy::queryServiceAddress()
{
    // CIntraNameProxy should run at CFdbContext, not CFdbBaseContext
    auto default_context = fdb_dynamic_cast_if_available<CFdbContext *>(mContext);
    auto &container = default_context->getContexts().getContainer();
    for (auto it = container.begin(); it != container.end(); ++it)
    {
        it->second->sendAsyncEndeavor(new CQueryServiceAddressJob(this));
    }
}

