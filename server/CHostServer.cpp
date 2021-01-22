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

#include "CHostServer.h"
#include <common_base/CFdbContext.h>
#include <common_base/CFdbMessage.h>
#include <common_base/CBaseSocketFactory.h>
#include <common_base/CFdbSession.h>
#include <security/CFdbusSecurityConfig.h>
#include <utils/CNsConfig.h>
#include <utils/Log.h>
#include <common_base/CFdbIfNameServer.h>

CHostServer::CHostServer()
    : CBaseServer(CNsConfig::getHostServerName())
    , mHeartBeatTimer(this)
{
    mHostSecurity.importSecurity();
    enableTcpBlockingMode(true);
    enableIpcBlockingMode(true);

    mMsgHdl.registerCallback(NFdbBase::REQ_REGISTER_HOST, &CHostServer::onRegisterHostReq);
    mMsgHdl.registerCallback(NFdbBase::REQ_UNREGISTER_HOST, &CHostServer::onUnregisterHostReq);
    mMsgHdl.registerCallback(NFdbBase::REQ_QUERY_HOST, &CHostServer::onQueryHostReq);
    mMsgHdl.registerCallback(NFdbBase::REQ_HEARTBEAT_OK, &CHostServer::onHeartbeatOk);
    mMsgHdl.registerCallback(NFdbBase::REQ_HOST_READY, &CHostServer::onHostReady);

    mSubscribeHdl.registerCallback(NFdbBase::NTF_HOST_ONLINE, &CHostServer::onHostOnlineReg);
    mHeartBeatTimer.attach(FDB_CONTEXT, false);
}

CHostServer::~CHostServer()
{
}

void CHostServer::onSubscribe(CBaseJob::Ptr &msg_ref)
    {
    auto msg = castToMessage<CFdbMessage *>(msg_ref);
    const CFdbMsgSubscribeItem *sub_item;
    FDB_BEGIN_FOREACH_SIGNAL(msg, sub_item)
    {
        mSubscribeHdl.processMessage(this, msg, sub_item, sub_item->msg_code());
    }
    FDB_END_FOREACH_SIGNAL()
}

void CHostServer::onInvoke(CBaseJob::Ptr &msg_ref)
{
    mMsgHdl.processMessage(this, msg_ref);
}

void CHostServer::onOffline(FdbSessionId_t sid, bool is_last)
{
    auto it = mHostTbl.find(sid);
    if (it != mHostTbl.end())
    {
        auto &info = it->second;
        LOG_I("CHostServer: host is dropped: name: %s, ip: %s, ns: %s\n", info.mHostName.c_str(),
                                                                          info.mIpAddress.c_str(),
                                                                          info.mNsUrl.c_str());
        broadcastSingleHost(sid, false, info);
        mHostTbl.erase(it);
        if (mHostTbl.size() == 0)
        {
            mHeartBeatTimer.disable();
        }
    }
}

void CHostServer::onRegisterHostReq(CBaseJob::Ptr &msg_ref)
{
    auto msg = castToMessage<CFdbMessage *>(msg_ref);
    NFdbBase::FdbMsgHostAddress host_addr;
    CFdbParcelableParser parser(host_addr);
    if (!msg->deserialize(parser))
    {
        msg->status(msg_ref, NFdbBase::FDB_ST_MSG_DECODE_FAIL);
        return;
    }

    //TODO: check host cred and deny unauthorized host from being connected

    auto ip_addr = host_addr.ip_address().c_str();
    auto host_name = ip_addr;
    if (!host_addr.host_name().empty())
    {
        host_name = host_addr.host_name().c_str();
    }
    std::string str_ns_url;
    const char *ns_url;
    if (host_addr.ns_url().empty())
    {
        CBaseSocketFactory::buildUrl(str_ns_url, ip_addr, CNsConfig::getNameServerTcpPort());
        ns_url = str_ns_url.c_str();
    }
    else
    {
        ns_url = host_addr.ns_url().c_str();
    }
    LOG_I("HostServer: host is registered: name: %s; ip: %s, ns: %s\n.", host_name, ip_addr, ns_url);
    for (auto it = mHostTbl.begin(); it != mHostTbl.end(); ++it)
    {
        if (it->second.mIpAddress == ip_addr)
        {
            msg->status(msg_ref, NFdbBase::FDB_ST_ALREADY_EXIST);
            return;
        }
    }

    auto &info = mHostTbl[msg->session()];
    info.mHostName = host_name;
    info.mIpAddress = ip_addr;
    info.mNsUrl = ns_url;
    info.mHbCount = 0;
    info.mReady = false;
    info.mAuthorized = false;
    if (host_addr.has_cred() && !host_addr.cred().empty())
    {
        auto cred = mHostSecurity.getCred(host_name);
        if (cred && !cred->compare(host_addr.cred()))
        {
            info.mAuthorized = true;
        }
    }
    CFdbToken::allocateToken(info.mTokens);

    NFdbBase::FdbMsgHostRegisterAck ack;
    populateTokens(info.mTokens, ack);
    CFdbParcelableBuilder builder(ack);
    msg->reply(msg_ref, builder);

    if (mHostTbl.size() == 1)
    {
        mHeartBeatTimer.enable();
    }
}

void CHostServer::onHostReady(CBaseJob::Ptr &msg_ref)
{
    auto msg = castToMessage<CFdbMessage *>(msg_ref);

    //TODO: check host cred and deny unauthorized host from being connected

    auto it = mHostTbl.find(msg->session());
    if (it != mHostTbl.end())
    {
        CHostInfo &info = it->second;
        info.mReady = true;
        broadcastSingleHost(msg->session(), true, info);
    }
}

// give token from 'this_host' to 'that_host' so that 'that_host' can connect with 'this_host'
void CHostServer::addToken(const CHostInfo &this_host,
                          const CHostInfo &that_host,
                          NFdbBase::FdbMsgHostAddress &host_addr)
{
    int32_t security_level;
    if (that_host.mAuthorized)
    {
         security_level =  mHostSecurity.getSecurityLevel(this_host.mHostName.c_str(), that_host.mHostName.c_str());
    }
    else
    {
        security_level = FDB_SECURITY_LEVEL_NONE;
    }

    const char *token = 0;
    if ((security_level >= 0) && (security_level < (int32_t)this_host.mTokens.size()))
    {
        token = this_host.mTokens[security_level].c_str();
    }
    host_addr.token_list().clear_tokens();
    host_addr.token_list().set_crypto_algorithm(NFdbBase::CRYPTO_NONE);
    if (token)
    {
        host_addr.token_list().add_tokens(token);
    }
}

void CHostServer::broadcastSingleHost(FdbSessionId_t sid, bool online, CHostInfo &info)
{
    NFdbBase::FdbMsgHostAddressList addr_list;
    auto addr = addr_list.add_address_list();
    addr->set_host_name(info.mHostName);
    addr->set_ip_address(info.mIpAddress);
    addr->set_ns_url(online ? info.mNsUrl : ""); // ns_url being empty means offline

    if (online)
    {
        tSubscribedSessionSets sessions;
        getSubscribeTable(NFdbBase::NTF_HOST_ONLINE, 0, sessions);
        for (auto session_it = sessions.begin(); session_it != sessions.end(); ++session_it)
        {
            CFdbSession *session = *session_it;
            auto info_it = mHostTbl.find(session->sid());
            if (info_it != mHostTbl.end()) 
            {
                addToken(info, info_it->second, *addr);
            }
            CFdbParcelableBuilder builder(addr_list);
            broadcast(session->sid(), FDB_OBJECT_MAIN, NFdbBase::NTF_HOST_ONLINE, builder);
        }
    }
    else
    {
        CFdbParcelableBuilder builder(addr_list);
        broadcast(NFdbBase::NTF_HOST_ONLINE, builder);
    }
}

void CHostServer::onUnregisterHostReq(CBaseJob::Ptr &msg_ref)
{
    auto msg = castToMessage<CFdbMessage *>(msg_ref);
    NFdbBase::FdbMsgHostAddress host_addr;
    CFdbParcelableParser parser(host_addr);
    if (!msg->deserialize(parser))
    {
        msg->status(msg_ref, NFdbBase::FDB_ST_MSG_DECODE_FAIL);
        return;
    }
    auto ip_addr = host_addr.ip_address().c_str();

    for (auto it = mHostTbl.begin(); it != mHostTbl.end(); ++it)
    {
        if (it->second.mIpAddress == ip_addr)
        {
            CHostInfo &info = it->second;
            LOG_I("CHostServer: host is unregistered: name: %s, ip: %s, ns: %s\n",
                    info.mHostName.c_str(), info.mIpAddress.c_str(), info.mNsUrl.c_str());
            broadcastSingleHost(msg->session(), false, info);
            mHostTbl.erase(it);
            return;
        }
    }
    msg->status(msg_ref, NFdbBase::FDB_ST_NON_EXIST);
}

void CHostServer::onQueryHostReq(CBaseJob::Ptr &msg_ref)
{
    auto msg = castToMessage<CFdbMessage *>(msg_ref);
    msg->status(msg_ref, NFdbBase::FDB_ST_NOT_IMPLEMENTED, "onQueryHostReq() is not implemented!");
}

void CHostServer::onHeartbeatOk(CBaseJob::Ptr &msg_ref)
{
    auto msg = castToMessage<CFdbMessage *>(msg_ref);
    auto it = mHostTbl.find(msg->session());

    if (it != mHostTbl.end())
    {
        CHostInfo &info = it->second;
        info.mHbCount = 0;
    }
}

void CHostServer::onHostOnlineReg(CFdbMessage *msg, const CFdbMsgSubscribeItem *sub_item)
{
    NFdbBase::FdbMsgHostAddressList addr_list;
    auto session = FDB_CONTEXT->getSession(msg->session());
    for (auto it = mHostTbl.begin(); it != mHostTbl.end(); ++it)
    {
        auto &info = it->second;
        if (info.mReady)
        {
            auto addr = addr_list.add_address_list();
            addr->set_ip_address(info.mIpAddress);
            addr->set_ns_url(info.mNsUrl); // ns_url being empty means offline
            addr->set_host_name(info.mHostName);

            auto info_it = mHostTbl.find(session->sid());
            if (info_it != mHostTbl.end()) 
            {
                addToken(info_it->second, info, *addr);
            }
        }
    }
    if (!addr_list.address_list().empty())
    {
        CFdbParcelableBuilder builder(addr_list);
        msg->broadcast(NFdbBase::NTF_HOST_ONLINE, builder);
    }
}

void CHostServer::broadcastHeartBeat(CMethodLoopTimer<CHostServer> *timer)
{
    for (auto it = mHostTbl.begin(); it != mHostTbl.end();)
    {
        auto the_it = it++;
        auto &info = the_it->second;
        if (++info.mHbCount >= CNsConfig::getHeartBeatRetryNr())
        {
            // will trigger offline callback which do everything for me.
            LOG_E("Host %s is kicked out due to HB!\n", info.mHostName.c_str());
            kickOut(the_it->first);
        }
    }
    broadcast(NFdbBase::NTF_HEART_BEAT);
}

void CHostServer::populateTokens(const CFdbToken::tTokenList &tokens,
                                 NFdbBase::FdbMsgHostRegisterAck &list)
{
    list.token_list().clear_tokens();
    list.token_list().set_crypto_algorithm(NFdbBase::CRYPTO_NONE);
    for (CFdbToken::tTokenList::const_iterator it = tokens.begin(); it != tokens.end(); ++it)
    {
        list.token_list().add_tokens(*it);
    }
}

