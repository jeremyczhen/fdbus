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
#include <idl-gen/common.base.MessageHeader.pb.h>
#include <security/CFdbusSecurityConfig.h>
#include "CHostProxy.h"
#include "CNsConfig.h"
#include <utils/Log.h>

CNameServer::CNameServer()
    : CBaseServer(CNsConfig::getNameServerName())
    , mTcpPortAllocator(CNsConfig::getTcpPortMin())
    , mIpcAllocator(0)
    , mHostProxy(0)
{
#ifdef CFG_ALLOC_PORT_BY_SYSTEM
    mNsPort = FDB_SYSTEM_PORT;
#else
    mNsPort = CNsConfig::getIntNameServerTcpPort();
#endif
    mServerSecruity.importSecurity();
    role(FDB_OBJECT_ROLE_NS_SERVER);
    
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
}

CNameServer::~CNameServer()
{
    if (mHostProxy)
    {
        delete mHostProxy;
    }

    for (tRegistryTbl::iterator it = mRegistryTbl.begin(); it != mRegistryTbl.end(); ++it)
    {
        CSvcRegistryEntry &desc_tbl = it->second;
        for (tAddressDescTbl::iterator desc_it = desc_tbl.mAddrTbl.begin();
                desc_it != desc_tbl.mAddrTbl.end(); ++desc_it)
        {
            delete *desc_it;
        }
    }
}

void CNameServer::onSubscribe(CBaseJob::Ptr &msg_ref)
{
    CFdbMessage *msg = castToMessage<CFdbMessage *>(msg_ref);
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
    for (tAddressDescTbl::const_iterator it = addr_tbl.begin(); it != addr_tbl.end(); ++it)
    {
        if ((type == FDB_SOCKET_MAX) || (type == (*it)->mAddress.mType))
        {
            if ((*it)->mStatus == CFdbAddressDesc::ADDR_BOUND)
            {
                list.add_address_list((*it)->mAddress.mUrl);
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
    CBaseSocketFactory::buildUrl(ns_url, FDB_SOCKET_TCP, host_ip.c_str(), mNsPort);
    msg_svc_info->mutable_host_addr()->set_ip_address(host_ip);
    msg_svc_info->mutable_host_addr()->set_ns_url(ns_url);
    msg_svc_info->mutable_host_addr()->set_host_name(mHostProxy->hostName());
    msg_svc_info->mutable_service_addr()->set_service_name(svc_name ? svc_name : "");
}

void CNameServer::populateServerTable(CFdbSession *session, NFdbBase::FdbMsgServiceTable &svc_tbl, bool is_local)
{
    if (mRegistryTbl.empty())
    {
        NFdbBase::FdbMsgServiceInfo *msg_svc_info = svc_tbl.add_service_tbl();
        setHostInfo(session, msg_svc_info, 0);
        msg_svc_info->mutable_service_addr()->set_service_name("");
        msg_svc_info->mutable_service_addr()->set_host_name("");
        msg_svc_info->mutable_service_addr()->set_is_local(is_local);
    }
    else
    {
        for (tRegistryTbl::iterator it = mRegistryTbl.begin(); it != mRegistryTbl.end(); ++it)
        {
            NFdbBase::FdbMsgServiceInfo *msg_svc_info = svc_tbl.add_service_tbl();
            setHostInfo(session, msg_svc_info, it->first.c_str());
            populateAddrList(it->second.mAddrTbl, *msg_svc_info->mutable_service_addr(), FDB_SOCKET_MAX);
            msg_svc_info->mutable_service_addr()->set_service_name(it->first);
            msg_svc_info->mutable_service_addr()->set_host_name(mHostProxy->hostName());
            msg_svc_info->mutable_service_addr()->set_is_local(is_local);
        }
    }
}

void CNameServer::addServiceAddress(const std::string &svc_name,
                                    FdbSessionId_t sid,
                                    EFdbSocketType skt_type,
                                    NFdbBase::FdbMsgAddressList *msg_addr_list)
{
    CSvcRegistryEntry &addr_tbl = mRegistryTbl[svc_name];
    CFdbToken::allocateToken(addr_tbl.mTokens);
    if (msg_addr_list)
    {
        populateTokens(addr_tbl.mTokens, *msg_addr_list);
    }

    addr_tbl.mSid = sid;
    if (msg_addr_list)
    {
        msg_addr_list->set_service_name(svc_name);
        msg_addr_list->set_host_name((mHostProxy->hostName()));
        msg_addr_list->set_is_local(true);
    }
    
    CFdbAddressDesc *desc;
    std::string url;
    if (skt_type == FDB_SOCKET_IPC)
    {
        if (allocateIpcAddress(svc_name, url))
        {
            desc = createAddrDesc(url.c_str());
            desc->mStatus = CFdbAddressDesc::ADDR_PENDING;
            addr_tbl.mAddrTbl.push_back(desc);
            if (msg_addr_list)
            {
                msg_addr_list->add_address_list(desc->mAddress.mUrl);
            }
        }
    }
    if (allocateTcpAddress(svc_name, url))
    {
        desc = createAddrDesc(url.c_str());
        desc->mStatus = CFdbAddressDesc::ADDR_PENDING;
        addr_tbl.mAddrTbl.push_back(desc);
        if (msg_addr_list)
        {
            msg_addr_list->add_address_list(desc->mAddress.mUrl);
        }
    }
}

void CNameServer::onAllocServiceAddressReq(CBaseJob::Ptr &msg_ref)
{
    CFdbMessage *msg = castToMessage<CFdbMessage *>(msg_ref);
    NFdbBase::FdbMsgServerName svc_name;
    FdbSessionId_t sid = msg->session();
    
    if (!msg->deserialize(svc_name))
    {
        msg->status(msg_ref, NFdbBase::FDB_ST_MSG_DECODE_FAIL);
        return;
    }

    tRegistryTbl::iterator it = mRegistryTbl.find(svc_name.name());
    if (it != mRegistryTbl.end())
    {
        msg->status(msg_ref, NFdbBase::FDB_ST_ALREADY_EXIST);
        return;
    }

    NFdbBase::FdbMsgAddressList reply_addr_list;
    addServiceAddress(svc_name.name(), sid, getSocketType(sid), &reply_addr_list);
    if (reply_addr_list.address_list().empty())
    {
        msg->status(msg_ref, NFdbBase::FDB_ST_NOT_AVAILABLE);
        return;
    }
    
    msg->reply(msg_ref, reply_addr_list);
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
    CFdbSocketAddr const *session_addr = sinfo.mSocketInfo.mAddress;
    if (session_addr->mType == FDB_SOCKET_IPC)
    {
        /*
         * This should not happen: the client uses unix domain
         * socket to connect with name server for address of
         * a server, but name server ask it to connect with the
         * server via TCP interface.
         */
        LOG_E("CNameServer: Unable to determine service address!\n");
    }
    else
    {
        // The same port number but a specific IP address
        CBaseSocketFactory::buildUrl(out_url, FDB_SOCKET_TCP,
                                    sinfo.mConn->mSelfIp.c_str(),
                                    port);
    }
}

void CNameServer::populateTokensRemote(const CFdbToken::tTokenList &tokens,
                                       NFdbBase::FdbMsgAddressList &addr_list,
                                       CFdbSession *session)
{
    addr_list.mutable_token_list()->clear_tokens();
    addr_list.mutable_token_list()->set_crypto_algorithm(NFdbBase::CRYPTO_NONE);
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
            addr_list.mutable_token_list()->add_tokens(tokens[i].c_str());
        }    
    }    
}

void CNameServer::broadcastSvcAddrRemote(const CFdbToken::tTokenList &tokens,
                                         NFdbBase::FdbMsgAddressList &addr_list,
                                         CFdbMessage *msg)
{
    const char *svc_name = addr_list.service_name().c_str();
    CFdbSession *session = FDB_CONTEXT->getSession(msg->session());
    if (session)
    {    
        populateTokensRemote(tokens, addr_list, session);
        msg->broadcast(NFdbBase::NTF_SERVICE_ONLINE_INTER_MACHINE, addr_list, svc_name);
        addr_list.mutable_token_list()->clear_tokens();
    }
}

void CNameServer::broadcastSvcAddrRemote(const CFdbToken::tTokenList &tokens,
                                         NFdbBase::FdbMsgAddressList &addr_list,
                                         CFdbSession *session)
{
    const char *svc_name = addr_list.service_name().c_str();
    populateTokensRemote(tokens, addr_list, session);
    broadcast(session->sid(), FDB_OBJECT_MAIN, NFdbBase::NTF_SERVICE_ONLINE_INTER_MACHINE,
              addr_list, svc_name);
    addr_list.mutable_token_list()->clear_tokens();
}

void CNameServer::broadcastSvcAddrRemote(const CFdbToken::tTokenList &tokens,
                                         NFdbBase::FdbMsgAddressList &addr_list)
{
    tSubscribedSessionSets sessions;
    const char *svc_name = addr_list.service_name().c_str();
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
    const char *svc_name = addr_list.service_name().c_str();
    addr_list.mutable_token_list()->clear_tokens();
    addr_list.mutable_token_list()->set_crypto_algorithm(NFdbBase::CRYPTO_NONE);
    // send token matching security level to local clients
    int32_t security_level = getSecurityLevel(session, svc_name);
    if ((security_level >= 0) && (int32_t)tokens.size())
    {
        if (security_level >= (int32_t)tokens.size())
        {
            security_level = (int32_t)tokens.size() - 1; 
        }
        addr_list.mutable_token_list()->add_tokens(tokens[security_level].c_str());
    }    
}

void CNameServer::broadcastSvcAddrLocal(const CFdbToken::tTokenList &tokens,
                                        NFdbBase::FdbMsgAddressList &addr_list,
                                        CFdbMessage *msg)
{
    const char *svc_name = addr_list.service_name().c_str();
    CFdbSession *session = FDB_CONTEXT->getSession(msg->session());
    if (session)
    {    
        populateTokensLocal(tokens, addr_list, session);
        msg->broadcast(NFdbBase::NTF_SERVICE_ONLINE, addr_list, svc_name);
        addr_list.mutable_token_list()->clear_tokens();
    }
}

void CNameServer::broadcastSvcAddrLocal(const CFdbToken::tTokenList &tokens,
                                        NFdbBase::FdbMsgAddressList &addr_list,
                                        CFdbSession *session)
{
    const char *svc_name = addr_list.service_name().c_str();
    populateTokensLocal(tokens, addr_list, session);
    broadcast(session->sid(), FDB_OBJECT_MAIN, NFdbBase::NTF_SERVICE_ONLINE,
              addr_list, svc_name);
    addr_list.mutable_token_list()->clear_tokens();
}

void CNameServer::broadcastSvcAddrLocal(const CFdbToken::tTokenList &tokens,
                                        NFdbBase::FdbMsgAddressList &addr_list)
{
    tSubscribedSessionSets sessions;
    const char *svc_name = addr_list.service_name().c_str();
    getSubscribeTable(NFdbBase::NTF_SERVICE_ONLINE, svc_name, sessions);
    for (tSubscribedSessionSets::iterator it = sessions.begin(); it != sessions.end(); ++it)
    {
        CFdbSession *session = *it;
        broadcastSvcAddrLocal(tokens, addr_list, session);
    }
}

void CNameServer::onRegisterServiceReq(CBaseJob::Ptr &msg_ref)
{
    CFdbMessage *msg = castToMessage<CFdbMessage *>(msg_ref);
    NFdbBase::FdbMsgAddressList addr_list;
    if (!msg->deserialize(addr_list))
    {
        msg->status(msg_ref, NFdbBase::FDB_ST_MSG_DECODE_FAIL);
        return;
    }
    const std::string &svc_name = addr_list.service_name();

    tRegistryTbl::iterator reg_it = mRegistryTbl.find(svc_name);
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

    CSvcRegistryEntry &addr_tbl = reg_it->second;
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
    
    const ::google::protobuf::RepeatedPtrField< ::std::string> &addrs =
        addr_list.address_list();
    for (::google::protobuf::RepeatedPtrField< ::std::string>::const_iterator msg_it = addrs.begin();
            msg_it != addrs.end(); ++msg_it)
    {
        CFdbAddressDesc *desc = 0;
        for (tAddressDescTbl::iterator addr_it = addr_tbl.mAddrTbl.begin();
                addr_it != addr_tbl.mAddrTbl.end(); ++addr_it)
        {
            CFdbSocketAddr &addr_in_tbl = (*addr_it)->mAddress;
            if (!msg_it->compare(addr_in_tbl.mUrl))
            {
                desc = *addr_it;
                desc->mStatus = CFdbAddressDesc::ADDR_BOUND;
                break;
            }
            else if ((addr_in_tbl.mType != FDB_SOCKET_IPC) && (addr_in_tbl.mPort == FDB_SYSTEM_PORT))
            {
                CFdbSocketAddr addr;
                if (CBaseSocketFactory::parseUrl(msg_it->c_str(), addr))
                {
                    if (!addr.mAddr.compare(addr_in_tbl.mAddr))
                    {
                        // get port number allocated from system: save it.
                        CBaseSocketFactory::updatePort(addr_in_tbl, addr.mPort);
                        desc = *addr_it;
                        desc->mStatus = CFdbAddressDesc::ADDR_BOUND;
                    }
                }
            }
        }

        if (!desc)
        {
            desc = createAddrDesc(msg_it->c_str());
            if (desc)
            {
                desc->mStatus = CFdbAddressDesc::ADDR_BOUND;
                new_addr_tbl.push_back(desc);
            }
        }
        
        if (desc)
        {
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
#ifdef __WIN32__
            CBaseSocketFactory::buildUrl(hs_url,
                                         FDB_SOCKET_TCP,
                                         FDB_LOCAL_HOST,
                                         CNsConfig::getHostServerTcpPort());
#else
            hs_url = CNsConfig::getHostServerIpcPath();
#endif
            addr_tbl.mAddrTbl.push_back(createAddrDesc(hs_url.c_str()));
        }
        connectToHostServer(hs_url.c_str(), true);
    }

    LOG_I("CNameServer: Service %s is registered.\n", svc_name.c_str());

    if (!broadcast_all_addr_list.address_list().empty())
    {
        broadcast_all_addr_list.mutable_token_list()->clear_tokens(); // never broadcast token to monitors!!!
        broadcast_all_addr_list.mutable_token_list()->set_crypto_algorithm(NFdbBase::CRYPTO_NONE);
        broadcast_all_addr_list.set_is_local(true);
        broadcast(NFdbBase::NTF_SERVICE_ONLINE_MONITOR,
                  broadcast_all_addr_list, svc_name.c_str());
        broadcast_all_addr_list.set_is_local(false);
        broadcast(NFdbBase::NTF_SERVICE_ONLINE_MONITOR_INTER_MACHINE,
                  broadcast_all_addr_list, svc_name.c_str());
    }
    if (broadcast_ipc_addr_list.address_list().empty())
    {
        /* If ipc is bound or connecting, no longer bind to tcp */
        bool ipc_bound = false;
        for (tAddressDescTbl::iterator addr_it = addr_tbl.mAddrTbl.begin();
                addr_it != addr_tbl.mAddrTbl.end(); ++addr_it)
        {
            if (((*addr_it)->mAddress.mType == FDB_SOCKET_IPC) &&
                    ((*addr_it)->mStatus != CFdbAddressDesc::ADDR_FREE))
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

    checkUnconnectedAddress(addr_tbl, svc_name.c_str());
}

void CNameServer::checkUnconnectedAddress(CSvcRegistryEntry &desc_tbl, const char *svc_name)
{
    NFdbBase::FdbMsgAddressList addr_list;
    bool failure_found = false;
    for (tAddressDescTbl::iterator desc_it = desc_tbl.mAddrTbl.begin();
           desc_it != desc_tbl.mAddrTbl.end(); ++desc_it)
    {
        CFdbAddressDesc *desc = *desc_it;
        if (desc->mStatus == CFdbAddressDesc::ADDR_BOUND)
        {
            continue;
        }
        if (desc->reconnect_cnt >= CNsConfig::getAddressBindRetryCnt())
        {
            continue;
        }
        desc->reconnect_cnt++;
        std::string url;
        bool ret;
        if (desc->mAddress.mType == FDB_SOCKET_IPC)
        {
            ret = allocateIpcAddress(svc_name, url);
        }
        else
        {
            ret = allocateTcpAddress(svc_name, url);
        }

        if (ret)
        {
            // replace failed address with a new one
            if (CBaseSocketFactory::parseUrl(url.c_str(), desc->mAddress))
            {
                addr_list.add_address_list(url.c_str());
                failure_found = true;
            }
        }
    }

    if (failure_found)
    {
        addr_list.set_service_name(svc_name);
        addr_list.set_host_name((mHostProxy->hostName()));
        addr_list.set_is_local(true);
        broadcast(NFdbBase::NTF_MORE_ADDRESS, addr_list, svc_name);
        LOG_E("CNameServer: Service %s: fail to bind address and retry...\n", svc_name);
    }
}

void CNameServer::notifyRemoteNameServerDrop(const char *host_name)
{
    NFdbBase::FdbMsgAddressList broadcast_addr_list;
    const char *svc_name = CNsConfig::getNameServerName();

    broadcast_addr_list.set_service_name(svc_name);
    broadcast_addr_list.set_host_name(host_name);

    broadcast_addr_list.set_is_local(false);
    broadcast(NFdbBase::NTF_SERVICE_ONLINE_MONITOR, broadcast_addr_list, svc_name);
}

void CNameServer::removeService(tRegistryTbl::iterator &reg_it)
{
    const char *svc_name = reg_it->first.c_str();
    CSvcRegistryEntry &desc_tbl = reg_it->second;

    LOG_I("CNameServer: Service %s is unregistered.\n", svc_name);
    NFdbBase::FdbMsgAddressList broadcast_addr_list;
    broadcast_addr_list.set_service_name(svc_name);
    broadcast_addr_list.set_host_name(mHostProxy->hostName());

    broadcast_addr_list.set_is_local(true);
    broadcastSvcAddrLocal(reg_it->second.mTokens, broadcast_addr_list);
    broadcast_addr_list.mutable_token_list()->clear_tokens(); // never broadcast token to monitors!!!
    broadcast_addr_list.mutable_token_list()->set_crypto_algorithm(NFdbBase::CRYPTO_NONE);
    broadcast(NFdbBase::NTF_SERVICE_ONLINE_MONITOR, broadcast_addr_list, svc_name);

    broadcast_addr_list.set_is_local(false);
    // send all tokens to the local name server, which will send appropiate
    // token to local clients according to security level
    broadcastSvcAddrRemote(reg_it->second.mTokens, broadcast_addr_list);
    broadcast_addr_list.mutable_token_list()->clear_tokens(); // never broadcast token to monitors!!!
    broadcast_addr_list.mutable_token_list()->set_crypto_algorithm(NFdbBase::CRYPTO_NONE);
    broadcast(NFdbBase::NTF_SERVICE_ONLINE_MONITOR_INTER_MACHINE, broadcast_addr_list, svc_name);
    
    for (tAddressDescTbl::iterator desc_it = desc_tbl.mAddrTbl.begin();
            desc_it != desc_tbl.mAddrTbl.end(); ++desc_it)
    {
        delete *desc_it;
    }
    mRegistryTbl.erase(reg_it);
}

void CNameServer::onUnegisterServiceReq(CBaseJob::Ptr &msg_ref)
{
    CFdbMessage *msg = castToMessage<CFdbMessage *>(msg_ref);
    NFdbBase::FdbMsgServerName msg_svc_name;
    if (!msg->deserialize(msg_svc_name))
    {
        msg->status(msg_ref, NFdbBase::FDB_ST_MSG_DECODE_FAIL);
        return;
    }

    const char *svc_name = msg_svc_name.name().c_str();
    tRegistryTbl::iterator it = mRegistryTbl.find(svc_name);
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
    CFdbMessage *msg = castToMessage<CFdbMessage *>(msg_ref);
    NFdbBase::FdbMsgServiceTable svc_tbl;
    CFdbSession *session = FDB_CONTEXT->getSession(msg->session());
    
    populateServerTable(session, svc_tbl, false);
    msg->reply(msg_ref, svc_tbl);
}

void CNameServer::onQueryHostReq(CBaseJob::Ptr &msg_ref)
{
    CFdbMessage *msg = castToMessage<CFdbMessage *>(msg_ref);
    NFdbBase::FdbMsgHostAddressList host_tbl;
    mHostProxy->getHostTbl(host_tbl);
    msg->reply(msg_ref, host_tbl);
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

    CSvcRegistryEntry &addr_tbl = reg_it->second;
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
        msg->broadcast(msg_code, addr_list, reg_it->first.c_str());
    }
}

void CNameServer::onServiceOnlineReg(CFdbMessage *msg, const CFdbMsgSubscribeItem *sub_item)
{
    FdbMsgCode_t msg_code = sub_item->msg_code();
    const char *svc_name = sub_item->has_filter() ? sub_item->filter().c_str() : "";
    bool service_specified = svc_name[0] != '\0';

    if (service_specified)
    {
        tRegistryTbl::iterator it = mRegistryTbl.find(svc_name);
        if (it != mRegistryTbl.end())
        {
            broadServiceAddress(it, msg, msg_code);
        }
    }
    else
    {
        for (tRegistryTbl::iterator it = mRegistryTbl.begin(); it != mRegistryTbl.end(); ++it)
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
    msg->broadcast(sub_item->msg_code(), host_tbl);
}

void CNameServer::onHostInfoReg(CFdbMessage *msg, const CFdbMsgSubscribeItem *sub_item)
{
    NFdbBase::FdbMsgHostInfo msg_host_info;
    msg_host_info.set_name(mHostProxy->hostName());
    msg->broadcast(sub_item->msg_code(), msg_host_info);
}

CNameServer::CFdbAddressDesc *CNameServer::findAddress(EFdbSocketType type, const char *url)
{
    for (tRegistryTbl::iterator it = mRegistryTbl.begin(); it != mRegistryTbl.end(); ++it)
    {
        CSvcRegistryEntry &desc_tbl = it->second;
        for (tAddressDescTbl::iterator desc_it = desc_tbl.mAddrTbl.begin();
                desc_it != desc_tbl.mAddrTbl.end(); ++desc_it)
        {
            CFdbAddressDesc *addr_desc = *desc_it;
            if ((type == FDB_SOCKET_MAX) || (addr_desc->mAddress.mType == type))
            {
                if (addr_desc->mAddress.mUrl == url)
                {
                    return addr_desc;
                }
            }
        }
    }
    return 0;
}

void CNameServer::buildUrl(const char *id, std::string &url)
{
    url = CNsConfig::getIpcPathBase();
    url += id;
}

void CNameServer::buildUrl(uint32_t id, std::string &url)
{
    char id_string[48];
    sprintf(id_string, "%u", id);
    buildUrl(id_string, url);
}

CNameServer::CFdbAddressDesc *CNameServer::createAddrDesc(const char *url)
{
    CFdbAddressDesc *desc = new CFdbAddressDesc();
    if (!CBaseSocketFactory::parseUrl(url, desc->mAddress))
    {
        delete desc;
        return 0;
    }
    return desc;
}

bool CNameServer::allocateTcpAddress(const std::string &svc_name, std::string &addr_url)
{
    const char *str_host_ip = 0;
    std::string host_ip;
#ifdef CFG_ALLOC_PORT_BY_SYSTEM
    bool alloc_port_by_system = true;
#else
    bool alloc_port_by_system = false;
#endif
    
    if (!mInterface.empty())
    {
        CBaseSocketFactory::getIpAddress(host_ip, mInterface.c_str());
        str_host_ip = host_ip.c_str();
    }
    if (!str_host_ip || (str_host_ip[0] == '\0'))
    {
        str_host_ip = FDB_IP_ALL_INTERFACE;
    }

    bool is_host_server = svc_name == CNsConfig::getHostServerName();
    bool is_name_server = svc_name == name();
    if (is_host_server || is_name_server || alloc_port_by_system)
    {
        int32_t port;
        if (is_host_server)
        {
            port = CNsConfig::getIntHostServerTcpPort();
        }
        else if (alloc_port_by_system)
        {
#ifdef __WIN32__ 
            // for windows, port of name server is fixed.
            port = is_name_server ? CNsConfig::getIntNameServerTcpPort() : FDB_SYSTEM_PORT;
#else
            port = FDB_SYSTEM_PORT;
#endif
        }
        else
        {
            port = CNsConfig::getIntNameServerTcpPort();
        }
        std::string url;
        CBaseSocketFactory::buildUrl(url, FDB_SOCKET_TCP, str_host_ip, port);
        if (alloc_port_by_system || !findAddress(FDB_SOCKET_TCP, url.c_str()))
        {
            addr_url = url;
            return true;
        }
        LOG_E("NameServer: TCP address conflict!\n");
    }
    else
    {
        uint32_t alloc_begin;
        uint32_t alloc_end;
        alloc_begin = mTcpPortAllocator++;
        alloc_end = CNsConfig::getTcpPortMax();
        if (mTcpPortAllocator > alloc_end)
        {
            mTcpPortAllocator = CNsConfig::getTcpPortMin();
        }
        
        for (uint32_t id = alloc_begin; id <= alloc_end; ++id)
        {
            std::string url;
            CBaseSocketFactory::buildUrl(url, FDB_SOCKET_TCP, str_host_ip, id);
            if (!findAddress(FDB_SOCKET_TCP, url.c_str()))
            {
                addr_url = url;
                return true;
            }
        }
        LOG_E("NameServer: Fatal Error! Port number out of range!\n");
    }

    return false;
}

bool CNameServer::allocateIpcAddress(const std::string &svc_name, std::string &addr_url)
{
    bool is_host_server = svc_name == CNsConfig::getHostServerName();
    bool is_name_server = svc_name == name();
    if (is_host_server || is_name_server)
    {
        const char *chr_url = is_host_server ? CNsConfig::getHostServerIpcPath() : CNsConfig::getNameServerIpcPath();
        if (!findAddress(FDB_SOCKET_IPC, chr_url))
        {
            addr_url = chr_url;
            return true;
        }
        LOG_E("NameServer: IPC address conflict!\n");
    }
    else
    {
        uint32_t alloc_begin;
        uint32_t alloc_end;
        alloc_begin = mIpcAllocator++;
        alloc_end = ~0;
        if (mIpcAllocator >= alloc_end)
        {
            mIpcAllocator = 0;
        }
        
        for (uint32_t id = alloc_begin; id <= alloc_end; ++id)
        {
            std::string url;
            buildUrl(id, url);
            if (!findAddress(FDB_SOCKET_IPC, url.c_str()))
            {
                addr_url = url;
                return true;
            }
        }
        LOG_E("NameServer: Fatal Error! IPC number out of range!\n");
    }

    return false;
}

EFdbSocketType CNameServer::getSocketType(FdbSessionId_t sid)
{
    CFdbSession *session = FDB_CONTEXT->getSession(sid);
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
    for (tRegistryTbl::iterator it = mRegistryTbl.begin(); it != mRegistryTbl.end();)
    {
        CSvcRegistryEntry &addr_tbl = it->second;
        tRegistryTbl::iterator cur_it = it;
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
    for (tAddressDescTbl::iterator it = addr_tbl.begin(); it != addr_tbl.end(); ++it)
    {
        const char *url = (*it)->mAddress.mUrl.c_str();
        CServerSocket *sk = doBind(url);
        if (sk)
        {
            if ((*it)->mAddress.mType != FDB_SOCKET_IPC)
            {
                CFdbSocketInfo info;
                sk->getSocketInfo(info);
                mNsPort = info.mAddress->mPort;
                CBaseSocketFactory::updatePort((*it)->mAddress, mNsPort);
            }
            (*it)->mStatus = CFdbAddressDesc::ADDR_BOUND;
        }
        else
        {
            LOG_E("NameServer: Fail to bind address: %s!\n", url);
            success = false;
        }
        
    }
    return success;
}

bool CNameServer::online(const char *hs_url, const char *hs_name, const char *interface_name)
{
    if (interface_name)
    {
        mInterface = interface_name;
    }
    mHostProxy = new CHostProxy(this, hs_name);
#ifdef __WIN32__
    EFdbSocketType skt_type = FDB_SOCKET_TCP;
#else
    EFdbSocketType skt_type = FDB_SOCKET_IPC;
#endif
    const char *ns_name = name().c_str();
    addServiceAddress(ns_name, FDB_INVALID_ID, skt_type, 0);
    tRegistryTbl::iterator it = mRegistryTbl.find(ns_name);
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
        if (mNsPort == FDB_SYSTEM_PORT)
        {
            LOG_E("CNameServer: getNsTcpUrl() is called before name server is bound to TCP address!\n");
        }
        else
        {
            CBaseSocketFactory::buildUrl(ns_url, FDB_SOCKET_TCP, host_ip.c_str(), mNsPort);
        }
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
    list.mutable_token_list()->clear_tokens();
    list.mutable_token_list()->set_crypto_algorithm(NFdbBase::CRYPTO_NONE);
    for (CFdbToken::tTokenList::const_iterator it = tokens.begin(); it != tokens.end(); ++it)
    {
        list.mutable_token_list()->add_tokens(*it);
    }
}

void CNameServer::dumpTokens(CFdbToken::tTokenList &tokens,
                             const NFdbBase::FdbMsgAddressList &list)
{
    const ::google::protobuf::RepeatedPtrField< ::std::string> &t =
        list.token_list().tokens();
    for (::google::protobuf::RepeatedPtrField< ::std::string>::const_iterator it = t.begin();
            it != t.end(); ++it)
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
