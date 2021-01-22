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
 
#include <stdio.h>
#include "CNameServer.h"
#include <common_base/CFdbContext.h>
#include <common_base/CFdbMessage.h>
#include <common_base/CBaseSocketFactory.h>
#include <common_base/CFdbSession.h>
#include <common_base/CFdbIfMessageHeader.h>
#include <common_base/CFdbIfNameServer.h>
#include <security/CFdbusSecurityConfig.h>
#include "CHostProxy.h"
#include <utils/CNsConfig.h>
#include <utils/Log.h>

CNameServer::CNameServer()
    : CBaseServer(CNsConfig::getNameServerName())
    , mHostProxy(0)
{
    mNsName = CNsConfig::getNameServerName();
    mServerSecruity.importSecurity();
    role(FDB_OBJECT_ROLE_NS_SERVER);
    enableTcpBlockingMode(true);
    enableIpcBlockingMode(true);
    
    mMsgHdl.registerCallback(NFdbBase::REQ_ALLOC_SERVICE_ADDRESS, &CNameServer::onAllocServiceAddressReq);
    mMsgHdl.registerCallback(NFdbBase::REQ_REGISTER_SERVICE, &CNameServer::onRegisterServiceReq);
    mMsgHdl.registerCallback(NFdbBase::REQ_UNREGISTER_SERVICE, &CNameServer::onUnegisterServiceReq);

    mMsgHdl.registerCallback(NFdbBase::REQ_QUERY_SERVICE, &CNameServer::onQueryServiceReq);
    mMsgHdl.registerCallback(NFdbBase::REQ_QUERY_SERVICE_INTER_MACHINE, &CNameServer::onQueryServiceInterMachineReq);

    mMsgHdl.registerCallback(NFdbBase::REQ_QUERY_HOST_LOCAL, &CNameServer::onQueryHostReq);

    mSubscribeHdl.registerCallback(NFdbBase::NTF_SERVICE_ONLINE, &CNameServer::onServiceOnlineReg);
    mSubscribeHdl.registerCallback(NFdbBase::NTF_SERVICE_ONLINE_INTER_MACHINE, &CNameServer::onServiceOnlineReg);
    mSubscribeHdl.registerCallback(NFdbBase::NTF_SERVICE_ONLINE_MONITOR, &CNameServer::onServiceOnlineReg);
    mSubscribeHdl.registerCallback(NFdbBase::NTF_SERVICE_ONLINE_MONITOR_INTER_MACHINE, &CNameServer::onServiceOnlineReg);
    mSubscribeHdl.registerCallback(NFdbBase::NTF_HOST_ONLINE_LOCAL, &CNameServer::onHostOnlineReg);

    mSubscribeHdl.registerCallback(NFdbBase::NTF_HOST_INFO, &CNameServer::onHostInfoReg);

#ifdef __WIN32__
    mLocalAllocator.setInterfaceIp(FDB_LOCAL_HOST);
#endif
}

CNameServer::~CNameServer()
{
    if (mHostProxy)
    {
        mHostProxy->prepareDestroy();
        delete mHostProxy;
    }
}

void CNameServer::onSubscribe(CBaseJob::Ptr &msg_ref)
{
    auto msg = castToMessage<CFdbMessage *>(msg_ref);
    const CFdbMsgSubscribeItem *sub_item;
    FDB_BEGIN_FOREACH_SIGNAL(msg, sub_item)
    {
        mSubscribeHdl.processMessage(this, msg, sub_item, sub_item->msg_code());
    }
    FDB_END_FOREACH_SIGNAL()
}

void CNameServer::onInvoke(CBaseJob::Ptr &msg_ref)
{
    mMsgHdl.processMessage(this, msg_ref);
}

void CNameServer::populateAddrList(const tAddressDescTbl &addr_tbl,
                                   NFdbBase::FdbMsgAddressList &list, EFdbSocketType type)
{
    for (auto it = addr_tbl.begin(); it != addr_tbl.end(); ++it)
    {
        if ((type == FDB_SOCKET_MAX) || (type == it->mAddress.mType))
        {
            if (it->mStatus == CFdbAddressDesc::ADDR_BOUND)
            {
                list.add_address_list(it->mAddress.mUrl);
            }
        }
    }
}

void CNameServer::setHostInfo(CFdbSession *session, NFdbBase::FdbMsgServiceInfo *msg_svc_info, const char *svc_name)
{
    std::string host_ip;
    if (!mHostProxy->hostIp(host_ip, session))
    {
        host_ip = FDB_IP_ALL_INTERFACE;
    }
    std::string ns_url;
    CBaseSocketFactory::buildUrl(ns_url, host_ip.c_str(), CNsConfig::getNameServerTcpPort());
    msg_svc_info->host_addr().set_ip_address(host_ip);
    msg_svc_info->host_addr().set_ns_url(ns_url);
    msg_svc_info->host_addr().set_host_name(mHostProxy->hostName());
}

void CNameServer::populateServerTable(CFdbSession *session, NFdbBase::FdbMsgServiceTable &svc_tbl, bool is_local)
{
    if (mRegistryTbl.empty())
    {
        auto msg_svc_info = svc_tbl.add_service_tbl();
        setHostInfo(session, msg_svc_info, 0);
        msg_svc_info->service_addr().set_service_name("");
        msg_svc_info->service_addr().set_host_name("");
        msg_svc_info->service_addr().set_is_local(is_local);
    }
    else
    {
        for (auto it = mRegistryTbl.begin(); it != mRegistryTbl.end(); ++it)
        {
            auto msg_svc_info = svc_tbl.add_service_tbl();
            setHostInfo(session, msg_svc_info, it->first.c_str());
            populateAddrList(it->second.mAddrTbl, msg_svc_info->service_addr(), FDB_SOCKET_MAX);
            msg_svc_info->service_addr().set_service_name(it->first);
            msg_svc_info->service_addr().set_host_name(mHostProxy->hostName());
            msg_svc_info->service_addr().set_is_local(is_local);
        }
    }
}

bool CNameServer::addressRegistered(const tAddressDescTbl &addr_list,
                                    CFdbSocketAddr &sckt_addr)
{
    for (auto it = addr_list.begin(); it != addr_list.end(); ++it)
    {
        auto &addr_desc = *it;
        // Only one interface is allowed for each service.
        if ((addr_desc.mAddress.mType == sckt_addr.mType) &&
             !addr_desc.mAddress.mAddr.compare(sckt_addr.mAddr))
        {
            return true;
        }
    }
    return false;
}

void CNameServer::addOneServiceAddress(const std::string &svc_name,
                                       CSvcRegistryEntry &addr_tbl,
                                       EFdbSocketType skt_type,
                                       NFdbBase::FdbMsgAddressList *msg_addr_list)
{
    tSocketAddrTbl sckt_addr_tbl;
    allocateAddress(skt_type, svc_name.c_str(), sckt_addr_tbl);
    for (auto it = sckt_addr_tbl.begin(); it != sckt_addr_tbl.end(); ++it)
    {
        if (addressRegistered(addr_tbl.mAddrTbl, *it))
        {
            continue;
        }

        addr_tbl.mAddrTbl.resize(addr_tbl.mAddrTbl.size() + 1);
        auto &desc = addr_tbl.mAddrTbl.back();
        desc.mAddress = *it;
        desc.mStatus = CFdbAddressDesc::ADDR_PENDING;
        if (msg_addr_list)
        {
            msg_addr_list->add_address_list(desc.mAddress.mUrl);
        }
    }
}

bool CNameServer::addServiceAddress(const std::string &svc_name,
                                    CSvcRegistryEntry &addr_tbl,
                                    EFdbSocketType skt_type,
                                    NFdbBase::FdbMsgAddressList *msg_addr_list)
{
    if (msg_addr_list)
    {
        msg_addr_list->set_service_name(svc_name);
        msg_addr_list->set_host_name((mHostProxy->hostName()));
        msg_addr_list->set_is_local(true);
    }

    if (skt_type == FDB_SOCKET_IPC)
    {
        addOneServiceAddress(svc_name, addr_tbl, FDB_SOCKET_IPC, msg_addr_list);
    }
    addOneServiceAddress(svc_name, addr_tbl, FDB_SOCKET_TCP, msg_addr_list);

    return msg_addr_list ? !msg_addr_list->address_list().empty() : false;
}

bool CNameServer::addServiceAddress(const std::string &svc_name,
                                    FdbSessionId_t sid,
                                    EFdbSocketType skt_type,
                                    NFdbBase::FdbMsgAddressList *msg_addr_list)
{
    auto &addr_tbl = mRegistryTbl[svc_name];
    CFdbToken::allocateToken(addr_tbl.mTokens);
    if (msg_addr_list)
    {
        populateTokens(addr_tbl.mTokens, *msg_addr_list);
    }
    addr_tbl.mSid = sid;

    return addServiceAddress(svc_name, addr_tbl, skt_type, msg_addr_list);
}

void CNameServer::onAllocServiceAddressReq(CBaseJob::Ptr &msg_ref)
{
    auto msg = castToMessage<CFdbMessage *>(msg_ref);
    NFdbBase::FdbMsgServerName svc_name;
    auto sid = msg->session();
    CFdbParcelableParser parser(svc_name);
    if (!msg->deserialize(parser))
    {
        msg->status(msg_ref, NFdbBase::FDB_ST_MSG_DECODE_FAIL);
        return;
    }

    auto it = mRegistryTbl.find(svc_name.name());
    if (it != mRegistryTbl.end())
    {
        msg->status(msg_ref, NFdbBase::FDB_ST_ALREADY_EXIST);
        return;
    }

    NFdbBase::FdbMsgAddressList reply_addr_list;
    if (!addServiceAddress(svc_name.name(), sid, getSocketType(sid), &reply_addr_list))
    {
        msg->status(msg_ref, NFdbBase::FDB_ST_NOT_AVAILABLE);
        return;
    }

    CFdbParcelableBuilder builder(reply_addr_list);
    msg->reply(msg_ref, builder);
}

void CNameServer::buildSpecificTcpAddress(CFdbSession *session,
                                          int32_t port,
                                          std::string &out_url)
{
    if (!session)
    {
        return;
    }
    /*
     * name server should not send FDB_IP_ALL_INTERFACE to client.
     * Instead specific IP address should be used. But a host has
     * several interfaces(IP address). Which one should be sent?
     * Here IP address of the interface connecting to name server
     * is used as IP address of the server.
     */
    CFdbSessionInfo sinfo;
    session->getSessionInfo(sinfo);
    auto session_addr = sinfo.mSocketInfo.mAddress;
    if (session_addr->mType != FDB_SOCKET_IPC)
    {
        // The same port number but a specific IP address
        CBaseSocketFactory::buildUrl(out_url, sinfo.mConn->mSelfIp.c_str(), port);
    }
}

void CNameServer::populateTokensRemote(const CFdbToken::tTokenList &tokens,
                                       NFdbBase::FdbMsgAddressList &addr_list,
                                       CFdbSession *session)
{
    addr_list.token_list().clear_tokens();
    addr_list.token_list().set_crypto_algorithm(NFdbBase::CRYPTO_NONE);
    // send token matching security level to local clients
    int32_t security_level = session->securityLevel();
    if ((security_level >= 0) && (int32_t)tokens.size())
    {
        if (security_level >= (int32_t)tokens.size())
        {
            security_level = (int32_t)tokens.size() - 1; 
        }
        // only give the tokens matching secure level of the host
        for (int32_t i = 0; i <= security_level; ++i) 
        {
            addr_list.token_list().add_tokens(tokens[i].c_str());
        }    
    }    
}

void CNameServer::broadcastSvcAddrRemote(const CFdbToken::tTokenList &tokens,
                                         NFdbBase::FdbMsgAddressList &addr_list,
                                         CFdbMessage *msg)
{
    auto svc_name = addr_list.service_name().c_str();
    auto session = FDB_CONTEXT->getSession(msg->session());
    if (session)
    {    
        populateTokensRemote(tokens, addr_list, session);
        CFdbParcelableBuilder builder(addr_list);
        msg->broadcast(NFdbBase::NTF_SERVICE_ONLINE_INTER_MACHINE, builder, svc_name);
        addr_list.token_list().clear_tokens();
    }
}

void CNameServer::broadcastSvcAddrRemote(const CFdbToken::tTokenList &tokens,
                                         NFdbBase::FdbMsgAddressList &addr_list,
                                         CFdbSession *session)
{
    auto svc_name = addr_list.service_name().c_str();
    populateTokensRemote(tokens, addr_list, session);
    CFdbParcelableBuilder builder(addr_list);
    broadcast(session->sid(), FDB_OBJECT_MAIN, NFdbBase::NTF_SERVICE_ONLINE_INTER_MACHINE,
              builder, svc_name);
    addr_list.token_list().clear_tokens();
}

void CNameServer::broadcastSvcAddrRemote(const CFdbToken::tTokenList &tokens,
                                         NFdbBase::FdbMsgAddressList &addr_list)
{
    tSubscribedSessionSets sessions;
    auto svc_name = addr_list.service_name().c_str();
    getSubscribeTable(NFdbBase::NTF_SERVICE_ONLINE_INTER_MACHINE, svc_name, sessions);
    for (tSubscribedSessionSets::iterator it = sessions.begin(); it != sessions.end(); ++it)
    {
        CFdbSession *session = *it;
        broadcastSvcAddrRemote(tokens, addr_list, session);
    }
}

void CNameServer::populateTokensLocal(const CFdbToken::tTokenList &tokens,
                                      NFdbBase::FdbMsgAddressList &addr_list,
                                      CFdbSession *session)
{
    auto svc_name = addr_list.service_name().c_str();
    addr_list.token_list().clear_tokens();
    addr_list.token_list().set_crypto_algorithm(NFdbBase::CRYPTO_NONE);
    // send token matching security level to local clients
    int32_t security_level = getSecurityLevel(session, svc_name);
    if ((security_level >= 0) && (int32_t)tokens.size())
    {
        if (security_level >= (int32_t)tokens.size())
        {
            security_level = (int32_t)tokens.size() - 1; 
        }
        addr_list.token_list().add_tokens(tokens[security_level].c_str());
    }    
}

void CNameServer::broadcastSvcAddrLocal(const CFdbToken::tTokenList &tokens,
                                        NFdbBase::FdbMsgAddressList &addr_list,
                                        CFdbMessage *msg)
{
    auto svc_name = addr_list.service_name().c_str();
    auto session = FDB_CONTEXT->getSession(msg->session());
    if (session)
    {    
        populateTokensLocal(tokens, addr_list, session);
        CFdbParcelableBuilder builder(addr_list);
        msg->broadcast(NFdbBase::NTF_SERVICE_ONLINE, builder, svc_name);
        addr_list.token_list().clear_tokens();
    }
}

void CNameServer::broadcastSvcAddrLocal(const CFdbToken::tTokenList &tokens,
                                        NFdbBase::FdbMsgAddressList &addr_list,
                                        CFdbSession *session)
{
    auto svc_name = addr_list.service_name().c_str();
    populateTokensLocal(tokens, addr_list, session);
    CFdbParcelableBuilder builder(addr_list);
    broadcast(session->sid(), FDB_OBJECT_MAIN, NFdbBase::NTF_SERVICE_ONLINE,
              builder, svc_name);
    addr_list.token_list().clear_tokens();
}

void CNameServer::broadcastSvcAddrLocal(const CFdbToken::tTokenList &tokens,
                                        NFdbBase::FdbMsgAddressList &addr_list)
{
    tSubscribedSessionSets sessions;
    auto svc_name = addr_list.service_name().c_str();
    getSubscribeTable(NFdbBase::NTF_SERVICE_ONLINE, svc_name, sessions);
    for (auto it = sessions.begin(); it != sessions.end(); ++it)
    {
        auto session = *it;
        broadcastSvcAddrLocal(tokens, addr_list, session);
    }
}

void CNameServer::onRegisterServiceReq(CBaseJob::Ptr &msg_ref)
{
    auto msg = castToMessage<CFdbMessage *>(msg_ref);
    NFdbBase::FdbMsgAddrBindResults addr_list;
    CFdbParcelableParser parser(addr_list);
    if (!msg->deserialize(parser))
    {
        msg->status(msg_ref, NFdbBase::FDB_ST_MSG_DECODE_FAIL);
        return;
    }
    const std::string &svc_name = addr_list.service_name();

    auto reg_it = mRegistryTbl.find(svc_name);
#if 1
    if (reg_it == mRegistryTbl.end())
    {
        msg->status(msg_ref, NFdbBase::FDB_ST_NON_EXIST);
        return;
    }
#else
    if (reg_it == mRegistryTbl.end())
    {
        CSvcRegistryEntry &addr_tbl = mRegistryTbl[addr_list.service_name()];
        reg_it = mRegistryTbl.find(svc_name);
    }
#endif

    auto &addr_tbl = reg_it->second;
    addr_tbl.mSid = msg->session();
    tAddressDescTbl new_addr_tbl;
    NFdbBase::FdbMsgAddressList broadcast_ipc_addr_list;
    broadcast_ipc_addr_list.set_service_name(svc_name);
    broadcast_ipc_addr_list.set_host_name(mHostProxy->hostName());
    
    NFdbBase::FdbMsgAddressList broadcast_tcp_addr_list;
    broadcast_tcp_addr_list.set_service_name(svc_name);
    broadcast_tcp_addr_list.set_host_name(mHostProxy->hostName());

    NFdbBase::FdbMsgAddressList broadcast_all_addr_list;
    broadcast_all_addr_list.set_service_name(svc_name);
    broadcast_all_addr_list.set_host_name(mHostProxy->hostName());

    bool is_host_server = svc_name == CNsConfig::getHostServerName();
    bool specific_hs_ip_found = false;
    std::string hs_ipc_url;
    std::string hs_tcp_url;
    
    auto &addrs = addr_list.address_list();
    for (auto msg_it = addrs.pool().begin(); msg_it != addrs.pool().end(); ++msg_it)
    {
        CFdbAddressDesc *desc = 0;
        tAddressDescTbl::iterator desc_it = addr_tbl.mAddrTbl.end();
        for (auto addr_it = addr_tbl.mAddrTbl.begin();
                addr_it != addr_tbl.mAddrTbl.end(); ++addr_it)
        {
            auto &addr_in_tbl = addr_it->mAddress;
            if (!msg_it->request_address().compare(addr_in_tbl.mUrl))
            {
                desc = &(*addr_it);
                if (msg_it->bind_address().empty())
                {
                    desc_it = addr_it;
                }
                else {
                    desc->mStatus = CFdbAddressDesc::ADDR_BOUND;
                    if ((addr_in_tbl.mType != FDB_SOCKET_IPC) && 
                            msg_it->bind_address().compare(addr_in_tbl.mUrl))
                    {
                        CFdbSocketAddr addr;
                        if (CBaseSocketFactory::parseUrl(msg_it->bind_address().c_str(), addr))
                        {
                            desc->mAddress = addr;
                        }
                    }
                }
                break;
            }
        }

        if (!desc)
        {
            new_addr_tbl.resize(new_addr_tbl.size() + 1);
            desc = &new_addr_tbl.back();
            if (CBaseSocketFactory::parseUrl(msg_it->bind_address().c_str(), desc->mAddress))
            {
                desc->mStatus = CFdbAddressDesc::ADDR_BOUND;
            }
            else
            {
                new_addr_tbl.pop_back();
                desc = 0;
            }
        }

        if (desc)
        {
            if (desc->mStatus != CFdbAddressDesc::ADDR_BOUND)
            {
                if (!reconnectToAddress(desc, svc_name.c_str()))
                {
                    if (desc_it != addr_tbl.mAddrTbl.end())
                    {
                        addr_tbl.mAddrTbl.erase(desc_it);
                    }
                }
                continue;
            }

            if (desc->mAddress.mType == FDB_SOCKET_IPC)
            {
                broadcast_ipc_addr_list.add_address_list(desc->mAddress.mUrl);
                if (is_host_server && hs_ipc_url.empty())
                {
                    hs_ipc_url = desc->mAddress.mUrl;
                }
            }
            else
            {
                broadcast_tcp_addr_list.add_address_list(desc->mAddress.mUrl);
                if (is_host_server && !specific_hs_ip_found)
                {
                    if (desc->mAddress.mAddr == FDB_IP_ALL_INTERFACE)
                    {
                        if (hs_tcp_url.empty())
                        {
                            buildSpecificTcpAddress(FDB_CONTEXT->getSession(msg->session()),
                                                    desc->mAddress.mPort, hs_tcp_url);
                        }
                    }
                    else
                    {
                        hs_tcp_url = desc->mAddress.mUrl;
                        specific_hs_ip_found = true;
                    }
                }
            }
            broadcast_all_addr_list.add_address_list(desc->mAddress.mUrl);
        }
    }
    if (!new_addr_tbl.empty())
    {
        addr_tbl.mAddrTbl.insert(addr_tbl.mAddrTbl.end(), new_addr_tbl.begin(), new_addr_tbl.end());
    }

    if (addr_tbl.mAddrTbl.empty())
    {
        mRegistryTbl.erase(reg_it);
        LOG_I("CNameServer: Service %s: registry fails.\n", svc_name.c_str());
        return;
    }
    else
    {
        LOG_I("CNameServer: Registry request of service %s is processed.\n", svc_name.c_str());
    }
    
    if (is_host_server)
    {
        std::string hs_url;
        /* perfer ipc over tcp for host server connection */
        if (!hs_ipc_url.empty())
        {
            hs_url = hs_ipc_url;
        }
        else if (!hs_tcp_url.empty())
        {
            hs_url = hs_tcp_url;
        }
        else
        {
            addr_tbl.mAddrTbl.resize(addr_tbl.mAddrTbl.size() + 1);
            auto &desc = addr_tbl.mAddrTbl.back();
#ifdef __WIN32__
            allocateAddress(mLocalAllocator, FDB_SVC_HOST_SERVER, desc.mAddress);
#else
            allocateAddress(mIpcAllocator, FDB_SVC_HOST_SERVER, desc.mAddress);
#endif
        }
        connectToHostServer(hs_url.c_str(), true);
    }

    if (!broadcast_all_addr_list.address_list().empty())
    {
        broadcast_all_addr_list.token_list().clear_tokens(); // never broadcast token to monitors!!!
        broadcast_all_addr_list.token_list().set_crypto_algorithm(NFdbBase::CRYPTO_NONE);
        broadcast_all_addr_list.set_is_local(true);
        {
        CFdbParcelableBuilder builder(broadcast_all_addr_list);
        broadcast(NFdbBase::NTF_SERVICE_ONLINE_MONITOR,
                  builder, svc_name.c_str());
        }
        broadcast_all_addr_list.set_is_local(false);
        {
        CFdbParcelableBuilder builder(broadcast_all_addr_list);
        broadcast(NFdbBase::NTF_SERVICE_ONLINE_MONITOR_INTER_MACHINE,
                  builder, svc_name.c_str());
        }
    }
    if (broadcast_ipc_addr_list.address_list().empty())
    {
        /* If ipc is bound or connecting, no longer bind to tcp */
        bool ipc_bound = false;
        for (auto addr_it = addr_tbl.mAddrTbl.begin();
                addr_it != addr_tbl.mAddrTbl.end(); ++addr_it)
        {
            if ((addr_it->mAddress.mType == FDB_SOCKET_IPC) &&
                    (addr_it->mStatus != CFdbAddressDesc::ADDR_FREE))
            {
                ipc_bound = true;
                break;
            }
        }
        if (!ipc_bound)
        {
            broadcast_tcp_addr_list.set_is_local(true);
            broadcastSvcAddrLocal(reg_it->second.mTokens, broadcast_tcp_addr_list);
        }
    }
    else
    {
        broadcast_ipc_addr_list.set_is_local(true);
        broadcastSvcAddrLocal(reg_it->second.mTokens, broadcast_ipc_addr_list);
    }
    
    if (!broadcast_tcp_addr_list.address_list().empty())
    {
        broadcast_tcp_addr_list.set_is_local(false);
        // send all tokens to the local name server, which will send appropiate
        // token to local clients according to security level
        broadcastSvcAddrRemote(reg_it->second.mTokens, broadcast_tcp_addr_list);
    }
}

bool CNameServer::reconnectToAddress(CFdbAddressDesc *addr_desc, const char *svc_name)
{
    if (addr_desc->reconnect_cnt >= CNsConfig::getAddressBindRetryCnt())
    {
        LOG_E("CNameServer: Service %s: fail to bind address.\n", svc_name);
        addr_desc->reconnect_cnt = 0;
        return false;
    }
    addr_desc->reconnect_cnt++;

    // allocate another address
    IAddressAllocator *allocator;
    if (addr_desc->mAddress.mType == FDB_SOCKET_IPC)
    {
#ifdef __WIN32__
        addr_desc->reconnect_cnt = 0;
        return false;
#else
        allocator = &mIpcAllocator;
#endif
    }
    else
    {
        auto it = mTcpAllocators.find(addr_desc->mAddress.mAddr);
        if (it == mTcpAllocators.end())
        {   // We come here if the address is not allocated by NS or HS fail to bind
            LOG_E("CNameServer: Service %s: unable to allocate address for IP %s\n", svc_name,
                  addr_desc->mAddress.mAddr.c_str());
            addr_desc->reconnect_cnt = 0;
            return false;
        }
        allocator = &it->second;
    }
    CFdbSocketAddr sckt_addr;
    bool ret = allocateAddress(*allocator, IAddressAllocator::getSvcType(svc_name), sckt_addr);
    if (ret && CBaseSocketFactory::parseUrl(sckt_addr.mUrl.c_str(), addr_desc->mAddress))
    {
        // replace failed address with a new one
        NFdbBase::FdbMsgAddressList addr_list;
        addr_list.add_address_list(sckt_addr.mUrl.c_str());
        addr_list.set_service_name(svc_name);
        addr_list.set_host_name((mHostProxy->hostName()));
        addr_list.set_is_local(true);
        CFdbParcelableBuilder builder(addr_list);
        broadcast(NFdbBase::NTF_MORE_ADDRESS, builder, svc_name);
        LOG_E("CNameServer: Service %s: fail to bind address and retry...\n", svc_name);
        return true;
    }

    addr_desc->reconnect_cnt = 0;
    return false;
}

void CNameServer::notifyRemoteNameServerDrop(const char *host_name)
{
    NFdbBase::FdbMsgAddressList broadcast_addr_list;
    auto svc_name = CNsConfig::getNameServerName();

    broadcast_addr_list.set_service_name(svc_name);
    broadcast_addr_list.set_host_name(host_name);

    broadcast_addr_list.set_is_local(false);
    CFdbParcelableBuilder builder(broadcast_addr_list);
    broadcast(NFdbBase::NTF_SERVICE_ONLINE_MONITOR, builder, svc_name);
}

void CNameServer::onHostOnline(bool online)
{
    if (!online)
    {
        return;
    }

    createTcpAllocator();
    for (auto it = mRegistryTbl.begin(); it != mRegistryTbl.end(); ++it)
    {
        if (!it->first.compare(CNsConfig::getHostServerName()))
        {
            continue;
        }
        NFdbBase::FdbMsgAddressList net_addr_list;
        if (addServiceAddress(it->first, it->second, FDB_SOCKET_TCP, &net_addr_list))
        {
            if (it->first.compare(name()))
            {
                CFdbParcelableBuilder builder(net_addr_list);
                broadcast(NFdbBase::NTF_MORE_ADDRESS, builder, it->first.c_str());
            }
            else
            {
                bindNsAddress(it->second.mAddrTbl);
            }
        }
    }
}

void CNameServer::removeService(tRegistryTbl::iterator &reg_it)
{
    auto svc_name = reg_it->first.c_str();

    LOG_I("CNameServer: Service %s is unregistered.\n", svc_name);
    NFdbBase::FdbMsgAddressList broadcast_addr_list;
    broadcast_addr_list.set_service_name(svc_name);
    broadcast_addr_list.set_host_name(mHostProxy->hostName());

    broadcast_addr_list.set_is_local(true);
    broadcastSvcAddrLocal(reg_it->second.mTokens, broadcast_addr_list);
    broadcast_addr_list.token_list().clear_tokens(); // never broadcast token to monitors!!!
    broadcast_addr_list.token_list().set_crypto_algorithm(NFdbBase::CRYPTO_NONE);
    {
    CFdbParcelableBuilder builder(broadcast_addr_list);
    broadcast(NFdbBase::NTF_SERVICE_ONLINE_MONITOR, builder, svc_name);
    }

    broadcast_addr_list.set_is_local(false);
    // send all tokens to the local name server, which will send appropiate
    // token to local clients according to security level
    broadcastSvcAddrRemote(reg_it->second.mTokens, broadcast_addr_list);
    broadcast_addr_list.token_list().clear_tokens(); // never broadcast token to monitors!!!
    broadcast_addr_list.token_list().set_crypto_algorithm(NFdbBase::CRYPTO_NONE);
    {
    CFdbParcelableBuilder builder(broadcast_addr_list);
    broadcast(NFdbBase::NTF_SERVICE_ONLINE_MONITOR_INTER_MACHINE, builder, svc_name);
    }

    mRegistryTbl.erase(reg_it);
}

void CNameServer::onUnegisterServiceReq(CBaseJob::Ptr &msg_ref)
{
    auto msg = castToMessage<CFdbMessage *>(msg_ref);
    NFdbBase::FdbMsgServerName msg_svc_name;
    CFdbParcelableParser parser(msg_svc_name);
    if (!msg->deserialize(parser))
    {
        msg->status(msg_ref, NFdbBase::FDB_ST_MSG_DECODE_FAIL);
        return;
    }

    auto svc_name = msg_svc_name.name().c_str();
    auto it = mRegistryTbl.find(svc_name);
    if (it != mRegistryTbl.end())
    {
        removeService(it);
    }
}

void CNameServer::onQueryServiceReq(CBaseJob::Ptr &msg_ref)
{
    mHostProxy->queryServiceReq(msg_ref);
}

void CNameServer::onQueryServiceInterMachineReq(CBaseJob::Ptr &msg_ref)
{
    auto msg = castToMessage<CFdbMessage *>(msg_ref);
    NFdbBase::FdbMsgServiceTable svc_tbl;
    auto session = FDB_CONTEXT->getSession(msg->session());
    populateServerTable(session, svc_tbl, false);
    CFdbParcelableBuilder builder(svc_tbl);
    msg->reply(msg_ref, builder);
}

void CNameServer::onQueryHostReq(CBaseJob::Ptr &msg_ref)
{
    auto msg = castToMessage<CFdbMessage *>(msg_ref);
    NFdbBase::FdbMsgHostAddressList host_tbl;
    mHostProxy->getHostTbl(host_tbl);
    CFdbParcelableBuilder builder(host_tbl);
    msg->reply(msg_ref, builder);
}

void CNameServer::broadServiceAddress(tRegistryTbl::iterator &reg_it, CFdbMessage *msg,
                                        FdbMsgCode_t msg_code)
{
    if ((msg_code == NFdbBase::NTF_SERVICE_ONLINE_INTER_MACHINE) && (reg_it->first == name()))
    {
        /* never broadcast name server to remote for normal request! */
        return;
    }
    NFdbBase::FdbMsgAddressList addr_list;
    EFdbSocketType skt_type;
    if ((msg_code == NFdbBase::NTF_SERVICE_ONLINE_MONITOR) ||
            (msg_code == NFdbBase::NTF_SERVICE_ONLINE_MONITOR_INTER_MACHINE))
    {
        skt_type = FDB_SOCKET_MAX;
    }
    else
    {
        skt_type = getSocketType(msg->session());
    }

    auto &addr_tbl = reg_it->second;
    populateAddrList(addr_tbl.mAddrTbl, addr_list, skt_type);
    if ((skt_type == FDB_SOCKET_IPC) && addr_list.address_list().empty())
    {   /* for request from local, fallback to TCP connection */
        populateAddrList(addr_tbl.mAddrTbl, addr_list, FDB_SOCKET_MAX);
    }

    if (addr_list.address_list().empty())
    {
        return;
    }
    
    addr_list.set_service_name(reg_it->first);
    addr_list.set_host_name(mHostProxy->hostName());
    if ((msg_code == NFdbBase::NTF_SERVICE_ONLINE_INTER_MACHINE) ||
        (msg_code == NFdbBase::NTF_SERVICE_ONLINE_MONITOR_INTER_MACHINE))
    {
        addr_list.set_is_local(false);
    }
    else
    {
        addr_list.set_is_local(true);
    }

    if (msg_code == NFdbBase::NTF_SERVICE_ONLINE)
    {
        broadcastSvcAddrLocal(reg_it->second.mTokens, addr_list, msg);
    }
    else if (msg_code == NFdbBase::NTF_SERVICE_ONLINE_INTER_MACHINE)
    {
        broadcastSvcAddrRemote(reg_it->second.mTokens, addr_list, msg);
    }
    else
    {
        CFdbParcelableBuilder builder(addr_list);
        msg->broadcast(msg_code, builder, reg_it->first.c_str());
    }
}

void CNameServer::onServiceOnlineReg(CFdbMessage *msg, const CFdbMsgSubscribeItem *sub_item)
{
    auto msg_code = sub_item->msg_code();
    auto svc_name = sub_item->has_filter() ? sub_item->filter().c_str() : "";
    bool service_specified = svc_name[0] != '\0';

    if (service_specified)
    {
        auto it = mRegistryTbl.find(svc_name);
        if (it != mRegistryTbl.end())
        {
            broadServiceAddress(it, msg, msg_code);
        }
    }
    else
    {
        for (auto it = mRegistryTbl.begin(); it != mRegistryTbl.end(); ++it)
        {
            broadServiceAddress(it, msg, msg_code);
        }
    }

    if ((msg_code == NFdbBase::NTF_SERVICE_ONLINE_INTER_MACHINE) ||
            (msg_code == NFdbBase::NTF_SERVICE_ONLINE_MONITOR_INTER_MACHINE))
    {
        LOG_I("CNameServer: Registry of %s is received from remote.\n",
                service_specified ? svc_name : "all services");
    }
    else if (name() != svc_name)
    {
        mHostProxy->forwardServiceListener(msg_code, svc_name, msg->session());
    }
}

void CNameServer::onHostOnlineReg(CFdbMessage *msg, const CFdbMsgSubscribeItem *sub_item)
{
    NFdbBase::FdbMsgHostAddressList host_tbl;
    mHostProxy->getHostTbl(host_tbl);
    CFdbParcelableBuilder builder(host_tbl);
    msg->broadcast(sub_item->msg_code(), builder);
}

void CNameServer::onHostInfoReg(CFdbMessage *msg, const CFdbMsgSubscribeItem *sub_item)
{
    NFdbBase::FdbMsgHostInfo msg_host_info;
    msg_host_info.set_name(mHostProxy->hostName());
    CFdbParcelableBuilder builder(msg_host_info);
    msg->broadcast(sub_item->msg_code(), builder);
}

CNameServer::CFdbAddressDesc *CNameServer::findAddress(EFdbSocketType type, const char *url)
{
    for (auto it = mRegistryTbl.begin(); it != mRegistryTbl.end(); ++it)
    {
        auto &desc_tbl = it->second;
        for (auto desc_it = desc_tbl.mAddrTbl.begin();
                desc_it != desc_tbl.mAddrTbl.end(); ++desc_it)
        {
            if ((type == FDB_SOCKET_MAX) || (desc_it->mAddress.mType == type))
            {
                if (desc_it->mAddress.mUrl == url)
                {
                    return &(*desc_it);
                }
            }
        }
    }
    return 0;
}

bool CNameServer::allocateAddress(IAddressAllocator &allocator, FdbServerType svc_type,
                                  CFdbSocketAddr &sckt_addr)
{
    int32_t retries =  (int32_t)mRegistryTbl.size() + 8; // 8 is just come to me...
    while (--retries > 0)
    {
        allocator.allocate(sckt_addr, svc_type);
        if (!findAddress(sckt_addr.mType, sckt_addr.mUrl.c_str()))
        {
            return true;
        }
        if (svc_type != FDB_SVC_USER)
        {
            LOG_E("NameServer: address %s conflict!\n", sckt_addr.mUrl.c_str());
            return false;
        }
        if ((sckt_addr.mType == FDB_SOCKET_TCP) && (sckt_addr.mPort == FDB_SYSTEM_PORT))
        {
            return true;
        }
    }

    LOG_E("NameServer: Fatal Error! allocator goes out of range!\n");
    return false;
}

void CNameServer::createTcpAllocator()
{
    // get IP address of connected HS. For Windows, IP of both remote HS and
    // local HS can be retrieved. But for local HS FDB_LOCAL_HOST might be
    // returned and should be skipped. For local NS of xNX since connection
    // is UDS IP address can not be retrieved.
    std::string host_ip;
    if (mHostProxy->hostIp(host_ip))
    {
        if (!host_ip.compare(FDB_LOCAL_HOST))
        {
            host_ip = "";
        }
    }
    if (!host_ip.empty())
    {   // add ip address of HS if connection with HS is via TCP
        // this means interface of HS will always binded
        auto &allocator = mTcpAllocators[host_ip];
        allocator.setInterfaceIp(host_ip.c_str());
    }

    for (auto it = mIpInterfaces.begin(); it != mIpInterfaces.end(); ++it)
    {
        auto &allocator = mTcpAllocators[*it];
        allocator.setInterfaceIp(it->c_str());
    }

    if (!mNameInterfaces.empty())
    {
        CBaseSocketFactory::tIpAddressTbl interfaces;
        CBaseSocketFactory::getIpAddress(interfaces);
        for (auto it = mNameInterfaces.begin(); it != mNameInterfaces.end(); ++it)
        {
            auto if_pair = interfaces.find(*it);
            if (if_pair == interfaces.end())
            {
                LOG_E("CNameServer: interface %s doesn't exist and is skipped.\n", it->c_str());
            }
            else
            {
                auto &allocator = mTcpAllocators[if_pair->second];
                allocator.setInterfaceIp(if_pair->second.c_str());
            }
        }
    }

    if (mTcpAllocators.empty())
    {   // for local HS of xNX, we come here if interface is not specified
        auto &allocator = mTcpAllocators[FDB_IP_ALL_INTERFACE];
        allocator.setInterfaceIp(FDB_IP_ALL_INTERFACE);
    }
}

void CNameServer::allocateTcpAddress(FdbServerType svc_type, tSocketAddrTbl &sckt_addr_tbl)
{
    for (auto it = mTcpAllocators.begin(); it != mTcpAllocators.end(); ++it)
    {
        sckt_addr_tbl.resize(sckt_addr_tbl.size() + 1);
        if (!allocateAddress(it->second, svc_type, sckt_addr_tbl.back()))
        {
            sckt_addr_tbl.pop_back();
        }
    }

#ifdef __WIN32__
    // For windows, always allocate from local host
    sckt_addr_tbl.resize(sckt_addr_tbl.size() + 1);
    if (!allocateAddress(mLocalAllocator, svc_type, sckt_addr_tbl.back()))
    {
        sckt_addr_tbl.pop_back();
    }
#endif
}

void CNameServer::allocateTcpAddress(const std::string &svc_name, tSocketAddrTbl &sckt_addr_tbl)
{
    auto svc_type = IAddressAllocator::getSvcType(svc_name.c_str());

    if (mTcpAllocators.empty())
    {
        if (mHostProxy->connected())
        {
            // It is not possible to come here. But anyway have to do something
            // to avoid failure
            auto &allocator = mTcpAllocators[FDB_IP_ALL_INTERFACE];
            allocator.setInterfaceIp(FDB_IP_ALL_INTERFACE);
        }
        else
        {
            if (svc_type == FDB_SVC_HOST_SERVER)
            {
                createTcpAllocator();
                allocateTcpAddress(svc_type, sckt_addr_tbl);
                // This is not grace but workable since address of host server does
                // not depends on context
                mTcpAllocators.clear();
            }
#ifdef __WIN32__
            if (svc_type == FDB_SVC_HOST_SERVER)
            {   // for windows, we still need to allocate local address for other services.
                return;
            }
#else
            // Don't allocate TCP address until HS is online: onHostOnline()->createTcpAllocator()
            return;
#endif
        }
    }

    allocateTcpAddress(svc_type, sckt_addr_tbl);
}

void CNameServer::allocateIpcAddress(const std::string &svc_name, tSocketAddrTbl &sckt_addr_tbl)
{
#ifndef __WIN32__
    sckt_addr_tbl.resize(sckt_addr_tbl.size() + 1);
    if (!allocateAddress(mIpcAllocator, IAddressAllocator::getSvcType(svc_name.c_str()), sckt_addr_tbl.back()))
    {
        sckt_addr_tbl.pop_back();
    }
#endif
}

void CNameServer::allocateAddress(EFdbSocketType sckt_type, const std::string &svc_name, tSocketAddrTbl &sckt_addr_tbl)
{
    (sckt_type == FDB_SOCKET_IPC) ? allocateIpcAddress(svc_name, sckt_addr_tbl) :
                                    allocateTcpAddress(svc_name, sckt_addr_tbl);
}

EFdbSocketType CNameServer::getSocketType(FdbSessionId_t sid)
{
    auto session = FDB_CONTEXT->getSession(sid);
    if (session)
    {
        CFdbSessionInfo sinfo;
        session->getSessionInfo(sinfo);
        return sinfo.mSocketInfo.mAddress->mType;
    }
    else
    {
        return FDB_SOCKET_TCP;
    }
}

void CNameServer::onOffline(FdbSessionId_t sid, bool is_last)
{
    for (auto it = mRegistryTbl.begin(); it != mRegistryTbl.end();)
    {
        auto &addr_tbl = it->second;
        auto cur_it = it;
        ++it;
        if (addr_tbl.mSid == sid)
        {
            removeService(cur_it);
        }
    }
}

bool CNameServer::bindNsAddress(tAddressDescTbl &addr_tbl)
{
    bool success = true;
    for (auto it = addr_tbl.begin(); it != addr_tbl.end(); ++it)
    {
        if (it->mStatus == CFdbAddressDesc::ADDR_BOUND)
        {
            continue;
        }
        auto url = it->mAddress.mUrl.c_str();
        auto sk = doBind(url);
        if (sk)
        {
            it->mStatus = CFdbAddressDesc::ADDR_BOUND;
        }
        else
        {
            LOG_E("NameServer: Fail to bind address: %s!\n", url);
            success = false;
        }
        
    }
    return success;
}

bool CNameServer::online(const char *hs_url, const char *hs_name,
                         char **interface_ips, uint32_t num_interface_ips,
                         char **interface_names, uint32_t num_interface_names)
{
    if (interface_ips && num_interface_ips)
    {
        for (uint32_t i = 0; i < num_interface_ips; ++i)
        {
            mIpInterfaces.insert(interface_ips[i]);
        }
    }

    if (interface_names && num_interface_names)
    {
        for (uint32_t i = 0; i < num_interface_names; ++i)
        {
            mNameInterfaces.insert(interface_names[i]);
        }
    }

    mHostProxy = new CHostProxy(this, hs_name);
    auto ns_name = name().c_str();
    addServiceAddress(ns_name, FDB_INVALID_ID, FDB_SOCKET_IPC, 0);
    auto it = mRegistryTbl.find(ns_name);
    if (it == mRegistryTbl.end())
    {
        LOG_E("Opps! Something wrong in Name Server!\n");
        return false;
    }

    if (!bindNsAddress(it->second.mAddrTbl))
    {
        return false;
    }

    connectToHostServer(hs_url, hs_url ? false : true);
    return true;
}

void CNameServer::connectToHostServer(const char *hs_url, bool is_local)
{
    mHostProxy->local(is_local);
    if (hs_url)
    {
        mHostProxy->setHostUrl(hs_url);
        mHostProxy->connectToHostServer();
    }
}

const std::string CNameServer::getNsTcpUrl(const char *ip_addr)
{
    std::string ns_url;
    std::string host_ip;
    if (ip_addr)
    {
        host_ip = ip_addr;
    }
    else
    {
        mHostProxy->hostIp(host_ip);
    }
    if (!host_ip.empty())
    {
        CBaseSocketFactory::buildUrl(ns_url, host_ip.c_str(), CNsConfig::getNameServerTcpPort());
    }
    return ns_url;
}

int32_t CNameServer::getSecurityLevel(CFdbSession *session, const char *svc_name)
{
    CFdbSessionInfo session_info;
    session->getSessionInfo(session_info);
    return mServerSecruity.getSecurityLevel(svc_name, session_info.mCred->uid);
}

void CNameServer::populateTokens(const CFdbToken::tTokenList &tokens,
                                 NFdbBase::FdbMsgAddressList &list)
{
    list.token_list().clear_tokens();
    list.token_list().set_crypto_algorithm(NFdbBase::CRYPTO_NONE);
    for (auto it = tokens.begin(); it != tokens.end(); ++it)
    {
        list.token_list().add_tokens(*it);
    }
}

void CNameServer::dumpTokens(CFdbToken::tTokenList &tokens,
                             NFdbBase::FdbMsgAddressList &list)
{
    const auto &t = list.token_list().tokens();
    for (auto it = t.pool().begin(); it != t.pool().end(); ++it)
    {
        tokens.push_back(*it);
    }
}

#if 0
void CNameServer::onHostIpObtained(std::string &ip_addr)
{
    if (ip_addr.empty())
    {
        return;
    }

    for (tRegistryTbl::iterator svc_it = mRegistryTbl.begin();
        svc_it != mRegistryTbl.end(); ++svc_it)
    {
        const std::string &svc_name = svc_it->first;
        CSvcRegistryEntry &addr_tbl = svc_it->second;
        NFdbBase::FdbMsgAddressList broadcast_addr_list;

        for (tAddressDescTbl::iterator addr_it = addr_tbl.mAddrTbl.begin();
            addr_it != addr_tbl.mAddrTbl.end(); ++addr_it)
        {
            CFdbAddressDesc *desc = *addr_it;
            if (ip_addr == desc->mAddress.mAddr)
            {
                broadcast_addr_list.add_address_list(desc->mAddress.mUrl);
            }
        }

        if (broadcast_addr_list.address_list().empty())
        {
            addServiceAddress(svc_name, FDB_INVALID_ID, FDB_SOCKET_TCP, &broadcast_addr_list);
        }
        else
        {
            broadcast_addr_list.set_service_name(svc_name);
            broadcast_addr_list.set_host_name((mHostProxy->hostName()));
            broadcast_addr_list.set_is_local(true);
        }
        
        if (svc_name != name())
        {
            if (!broadcast_addr_list.address_list().empty())
            {
                broadcast(NFdbBase::NTF_MORE_ADDRESS, broadcast_addr_list, svc_name.c_str());
            }
        }
        else
        {
            bindNsAddress(addr_tbl.mAddrTbl);
        }
    }
    mHostProxy->connectToHostServer();
}
#endif
