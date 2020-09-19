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

#include <vector>
#include "CHostProxy.h"
#include <common_base/CInterNameProxy.h>
#include <common_base/CFdbContext.h>
#include "CNameServer.h"
#include <common_base/CFdbIfMessageHeader.h>
#include <utils/Log.h>
#include <common_base/CBaseSocketFactory.h>
#include <common_base/CFdbSession.h>
#include <algorithm>

#define FDB_HOST_NAME_MAX 1024

CHostProxy::CHostProxy(CNameServer *ns, const char *host_name)
    : CBaseClient(0)
    , mNameServer(ns)
    , mConnectTimer(this)
{
    mNotifyHdl.registerCallback(NFdbBase::NTF_HOST_ONLINE, &CHostProxy::onHostOnlineNotify);
    mNotifyHdl.registerCallback(NFdbBase::NTF_HEART_BEAT, &CHostProxy::onHeartbeatOk);
    if (host_name)
    {
        mHostName = host_name;
    }
    else
    {
        char buffer[FDB_HOST_NAME_MAX];
        sysdep_gethostname(buffer, FDB_HOST_NAME_MAX);
        buffer[FDB_HOST_NAME_MAX - 1] = '\0';
        mHostName = buffer;
    }
    mName = mHostName;
    mConnectTimer.attach(FDB_CONTEXT, false);
}

void CHostProxy::isolate(tNameProxyTbl::iterator &it)
{
    auto ns_proxy = it->second;
    ns_proxy->doDisconnect();
    LOG_E("CHostProxy: Disconnected to NS of host %s.\n", ns_proxy->getHostIp().c_str());
    // eventually call deleteNameProxy()
    delete ns_proxy;
}

void CHostProxy::isolate()
{
    for (auto it = mNameProxyTbl.begin(); it != mNameProxyTbl.end();)
    {
        auto the_it = it;
        ++it;
        isolate(the_it);
    }
}

CHostProxy::~CHostProxy()
{
    isolate();
    doDisconnect();
}

bool CHostProxy::connectToHostServer()
{
    if (connected())
    {
        return true;
    }

    // timer will stop if connected since onOnline() will be called
    // upon success
    doConnect(mHostUrl.c_str());
    if (!connected())
    {
        mConnectTimer.startReconnectMode();
        return false;
    }

    return true;
}

void CHostProxy::onOnline(FdbSessionId_t sid, bool is_first)
{
    mConnectTimer.disable();

    subscribeListener(NFdbBase::NTF_HOST_ONLINE, true);
    subscribeListener(NFdbBase::NTF_HEART_BEAT, true);

    mNameServer->onHostOnline(true);
    hostOnline();

    LOG_I("CHostProxy: connected to host server: %s\n.", mHostUrl.c_str());
}

void CHostProxy::onOffline(FdbSessionId_t sid, bool is_last)
{
    mNameServer->onHostOnline(false);
    mConnectTimer.startReconnectMode();
    
    NFdbBase::FdbMsgHostAddressList host_list;
    getHostTbl(host_list);
    CFdbParcelableBuilder builder(host_list);
    mNameServer->broadcast(NFdbBase::NTF_HOST_ONLINE_LOCAL, builder);

    LOG_E("CHostProxy: Name Proxy: Fail to connect address: %s. Reconnecting...\n",
            mHostUrl.c_str());
}

void CHostProxy::subscribeListener(NFdbBase::FdbHsMsgCode code, bool sub)
{
    if (!connected())
    {
        return;
    }

    CFdbMsgSubscribeList subscribe_list;
    addNotifyItem(subscribe_list, code);
    if (sub)
    {
        subscribe(subscribe_list);
    }
    else
    {
        unsubscribe(subscribe_list);
    }
}

void CHostProxy::onReply(CBaseJob::Ptr &msg_ref)
{
    auto msg = castToMessage<CFdbMessage *>(msg_ref);
    if (msg->isStatus())
    {
        if (msg->isError())
        {
            int32_t id;
            std::string reason;
            msg->decodeStatus(id, reason);
            LOG_I("CHostProxy: onReply(): status is received: msg code: %d, id: %d, reason: %s\n", msg->code(), id, reason.c_str());
        }
    }

    switch (msg->code())
    {
        case NFdbBase::REQ_REGISTER_HOST:
        {
            NFdbBase::FdbMsgHostRegisterAck ack;
            CFdbParcelableParser parser(ack);
            if (!msg->deserialize(parser))
            {
                return;
            }
            if (ack.has_token_list() && mNameServer->importTokens(ack.token_list().tokens()))
            {
                mNameServer->updateSecurityLevel();
                LOG_I("CHostProxy: tokens of name server is updated.\n");
            }
            send(NFdbBase::REQ_HOST_READY);
        }
        break;
        default:
        break;
    }
}

void CHostProxy::onBroadcast(CBaseJob::Ptr &msg_ref)
{
    mNotifyHdl.processMessage(this, msg_ref);
}

void CHostProxy::onHostOnlineNotify(CBaseJob::Ptr &msg_ref)
{
    auto msg = castToMessage<CFdbMessage *>(msg_ref);
    NFdbBase::FdbMsgHostAddressList host_list;
    auto session = FDB_CONTEXT->getSession(msg->session());
    if (!session)
    {
        return; //never happen!!!
    }
    CFdbParcelableParser parser(host_list);
    if (!msg->deserialize(parser))
    {
        return;
    }

    std::string host_ip;
    hostIp(host_ip, session);

    tFdbFilterSets registered_service_tbl;
    mNameServer->getSubscribeTable(NFdbBase::NTF_SERVICE_ONLINE, registered_service_tbl);
    tFdbFilterSets monitored_service_tbl;
    mNameServer->getSubscribeTable(NFdbBase::NTF_SERVICE_ONLINE_MONITOR, monitored_service_tbl);
    CFdbParcelableBuilder builder(host_list);
    mNameServer->broadcast(NFdbBase::NTF_HOST_ONLINE_LOCAL, builder);
    auto &addr_list = host_list.address_list();
    std::vector<std::string> online_host_list;
    for (auto it = addr_list.vpool().begin(); it != addr_list.vpool().end(); ++it)
    {
        auto &addr = *it;
        std::string ns_url(addr.ns_url());
        std::string ip_address(addr.ip_address());
        if (addr.ip_address() == FDB_IP_ALL_INTERFACE)
        {
            if (!peerIp(ip_address, session))
            {
                continue;
            }
            if (!ns_url.empty())
            {
                CFdbSocketAddr addr;
                if (CBaseSocketFactory::parseUrl(ns_url.c_str(), addr))
                {
                    CBaseSocketFactory::buildUrl(ns_url, ip_address.c_str(), addr.mPort);
                }
                else
                {
                    continue;
                }
            }
        }

        if (!host_ip.empty() && (ip_address == host_ip))
        {
            // don't connect to self.
            continue;
        }

        auto np_it = mNameProxyTbl.find(ip_address);
        bool is_offline = ns_url.empty();
        if (is_offline)
        {
            if (np_it != mNameProxyTbl.end())
            {
                mNameServer->notifyRemoteNameServerDrop(np_it->second->getHostName().c_str());
                delete np_it->second;
            }
        }
        else
        {
            online_host_list.push_back(ip_address);
            if (np_it == mNameProxyTbl.end())
            {
                auto proxy = new CInterNameProxy(this, ip_address, ns_url, addr.host_name());
                proxy->enableReconnect(true);
                proxy->autoRemove(true);
                if (proxy->connectToNameServer())
                {
                    mNameProxyTbl[ip_address] = proxy;
                    if (addr.has_token_list() && proxy->importTokens(addr.token_list().tokens()))
                    {
                        proxy->updateSecurityLevel();
                        LOG_I("CHostProxy: tokens of %s is updated.\n", proxy->name().c_str());
                    }

                    for (auto filter_it = registered_service_tbl.begin();
                                filter_it != registered_service_tbl.end(); ++filter_it)
                    {
                        proxy->addServiceListener(filter_it->c_str(), FDB_INVALID_ID);
                        LOG_I("CHostProxy: registry of %s is forwarded due to host %s online.\n",
                              filter_it->c_str(), ip_address.c_str());
                    }
                    for (auto filter_it = monitored_service_tbl.begin();
                                filter_it != monitored_service_tbl.end(); ++filter_it)
                    {
                        proxy->addServiceMonitorListener(filter_it->c_str(), FDB_INVALID_ID);
                        LOG_I("CHostProxy: registry of %s is forwarded due to host %s online.\n",
                              filter_it->c_str(), ip_address.c_str());
                    }
                }
                else
                {
                    delete proxy;
                }
            }
        }
    }

    if (msg->isInitialResponse() && !online_host_list.empty())
    {
        for (auto np_it = mNameProxyTbl.begin(); np_it != mNameProxyTbl.end();)
        {
            auto the_np_it = np_it;
            ++np_it;
            auto proxy = the_np_it->second;
            if (std::find(online_host_list.begin(), online_host_list.end(), proxy->getHostIp()) ==
                    online_host_list.end())
            {
                isolate(the_np_it);
            }
        }
    }
}

void CHostProxy::onHeartbeatOk(CBaseJob::Ptr &msg_ref)
{
    send(NFdbBase::REQ_HEARTBEAT_OK);
    mConnectTimer.startHeartbeatMode();
}

void CHostProxy::hostOnline(FdbMsgCode_t code)
{
    std::string host_ip;
    if (!hostIp(host_ip))
    {
        host_ip = FDB_IP_ALL_INTERFACE;
    }
    if (host_ip == FDB_LOCAL_HOST)
    {
        host_ip = FDB_IP_ALL_INTERFACE;
    }

    NFdbBase::FdbMsgHostAddress host_addr;
    host_addr.set_ip_address(host_ip);
    host_addr.set_host_name(hostName());
    host_addr.set_ns_url(mNameServer->getNsTcpUrl(host_ip.c_str()));
    CFdbParcelableBuilder builder(host_addr);
    invoke(code, builder);
}

void CHostProxy::hostOnline()
{
    hostOnline(NFdbBase::REQ_REGISTER_HOST);
}

void CHostProxy::hostOffline()
{
    hostOnline(NFdbBase::REQ_UNREGISTER_HOST);
}

void CHostProxy::forwardServiceListener(FdbMsgCode_t msg_code
                                      , const char *service_name
                                      , FdbSessionId_t subscriber)
{
    std::string host_ip(FDB_IP_ALL_INTERFACE);
    for (auto it = mNameProxyTbl.begin(); it != mNameProxyTbl.end(); ++it)
    {
        if (host_ip == it->first)
        {
            continue;
        }
        auto ns_proxy = it->second;
        if (msg_code == NFdbBase::NTF_SERVICE_ONLINE_MONITOR)
        {
            ns_proxy->addServiceMonitorListener(service_name, subscriber);
        }
        else
        {
            ns_proxy->addServiceListener(service_name, subscriber);
        }
    }
}

void CHostProxy::recallServiceListener(FdbMsgCode_t msg_code, const char *service_name)
{
    std::string host_ip(FDB_IP_ALL_INTERFACE);
    for (auto it = mNameProxyTbl.begin(); it != mNameProxyTbl.end(); ++it)
    {
        if (host_ip == it->first)
        {
            continue;
        }
        auto ns_proxy = it->second;
        if (msg_code == NFdbBase::NTF_SERVICE_ONLINE_MONITOR)
        {
            ns_proxy->removeServiceMonitorListener(service_name);
        }
        else
        {
            ns_proxy->removeServiceListener(service_name);
        }
    }
}

void CHostProxy::deleteNameProxy(CInterNameProxy *proxy)
{
    auto it = mNameProxyTbl.find(proxy->getHostIp());
    if (it != mNameProxyTbl.end())
    {
        mNameServer->notifyRemoteNameServerDrop(it->second->getHostName().c_str());
        mNameProxyTbl.erase(it);
    }
}

void CHostProxy::onConnectTimer(CMethodLoopTimer<CHostProxy> *timer)
{
    switch (mConnectTimer.mMode)
    {
        case CConnectTimer::MODE_RECONNECT_TO_SERVER:
#if 0
            LOG_I("CHostProxy: Reconnecting to host server...\n");
#endif
            connectToHostServer();
        break;
        case CConnectTimer::MODE_HEARTBEAT:
            LOG_E("CHostProxy: Heartbeat timeout! Disconnecting to all hosts...\n");
            isolate();
            doDisconnect();
        break;
        default:
        break;
    }
}

void CHostProxy::queryServiceReq(CBaseJob::Ptr &msg_ref)
{
    auto msg = castToMessage<CFdbMessage *>(msg_ref);
    auto session = FDB_CONTEXT->getSession(msg->session());
    if (mNameProxyTbl.empty())
    {
        NFdbBase::FdbMsgServiceTable svc_tbl;
        mNameServer->populateServerTable(session, svc_tbl, true);
        CFdbParcelableBuilder builder(svc_tbl);
        msg->reply(msg_ref, builder);
        return;
    }
    
    auto pending_tbl = new CQueryServiceMsg::tPendingReqTbl;
    auto svc_tbl = new NFdbBase::FdbMsgServiceTable;
    mNameServer->populateServerTable(session, *svc_tbl, true);
    for (auto np_it = mNameProxyTbl.begin(); np_it != mNameProxyTbl.end(); ++np_it)
    {
        pending_tbl->push_back(np_it->first);
    }
    for (auto np_it = mNameProxyTbl.begin(); np_it != mNameProxyTbl.end(); ++np_it)
    {
        auto req = new CQueryServiceMsg(pending_tbl, svc_tbl, msg_ref, np_it->first, this);
        np_it->second->queryServiceTbl(req);
    }
}

void CHostProxy::finalizeServiceQuery(NFdbBase::FdbMsgServiceTable *svc_tbl, CQueryServiceMsg *query)
{
    if (svc_tbl)
    {
        const auto &src_pool = svc_tbl->service_tbl().pool();
        auto &dst_pool = query->mSvcTbl->service_tbl().vpool();
        dst_pool.insert(dst_pool.end(), src_pool.begin(), src_pool.end());
    }
    fdb_remove_value_from_container(*(query->mPendingReqTbl), query->mHostIp);
    
    for (auto it = query->mPendingReqTbl->begin(); it != query->mPendingReqTbl->end();)
    {
        auto the_it = it;
        ++it;
        if (mNameProxyTbl.find(*the_it) == mNameProxyTbl.end())
        {
            query->mPendingReqTbl->erase(the_it);
        }
    }

    if (query->mPendingReqTbl->empty())
    {
        auto msg = castToMessage<CFdbMessage *>(query->mReq);
        CFdbParcelableBuilder builder(*query->mSvcTbl);
        msg->reply(query->mReq, builder);
        delete query->mPendingReqTbl;
        delete query->mSvcTbl;
    }
}

void CHostProxy::getHostTbl(NFdbBase::FdbMsgHostAddressList &host_tbl)
{
    for (auto it = mNameProxyTbl.begin(); it != mNameProxyTbl.end(); ++it)
    {
        auto addr = host_tbl.add_address_list();
        addr->set_ip_address(it->second->getHostIp());
        addr->set_host_name(it->second->getHostName());
        addr->set_ns_url(it->second->getNsUrl());
    }

    auto addr = host_tbl.add_address_list();
    std::string host_ip;
    if (!hostIp(host_ip))
    {
        host_ip = FDB_IP_ALL_INTERFACE;
    }

    addr->set_ip_address(host_ip);
    addr->set_host_name(mHostName);
    addr->set_ns_url(mNameServer->getNsTcpUrl(host_ip.c_str()));
}

