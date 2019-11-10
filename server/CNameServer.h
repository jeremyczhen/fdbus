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

#ifndef _CNAMESERVER_H_
#define _CNAMESERVER_H_

#include <list>
#include <map>
#include <string>
#include <set>
#include <vector>
#include <common_base/CBaseServer.h>
#include <idl-gen/common.base.NameServer.pb.h>
#include <common_base/CSocketImp.h>
#include <security/CServerSecurityConfig.h>

class CFdbMessage;
class CHostProxy;
class CFdbSession;
class CNameServer : public CBaseServer
{
private:
#define INVALID_ALLOC_ID        (~0)
    struct CFdbAddressDesc
    {
        CFdbAddressDesc()
        {
            mStatus = ADDR_FREE;
            reconnect_cnt = 0;
        }
        enum eAddressStatus
        {
            ADDR_FREE,
            ADDR_PENDING,
            ADDR_BOUND,
        };
        eAddressStatus mStatus;
        CFdbSocketAddr mAddress;
        int32_t reconnect_cnt;
    };

public:
    CNameServer();
    ~CNameServer();
    bool online(const char *hs_url = 0, const char *host_name = 0, const char *interface_name = 0);
    const std::string getNsTcpUrl(const char *ip_addr = 0);
    void populateServerTable(CFdbSession *session, NFdbBase::FdbMsgServiceTable &svc_tbl, bool is_local);

    void notifyRemoteNameServerDrop(const char *host_name);
protected:
    void onSubscribe(CBaseJob::Ptr &msg_ref);
    void onInvoke(CBaseJob::Ptr &msg_ref);
    void onOffline(FdbSessionId_t sid, bool is_last);
private:
    typedef std::list<CFdbAddressDesc *> tAddressDescTbl;
    struct CSvcRegistryEntry
    {
        FdbSessionId_t mSid;
        tAddressDescTbl mAddrTbl;
        CFdbToken::tTokenList mTokens;
    };
    typedef std::map<std::string, CSvcRegistryEntry> tRegistryTbl;

    tRegistryTbl mRegistryTbl;
    CFdbMessageHandle<CNameServer> mMsgHdl;
    CFdbSubscribeHandle<CNameServer> mSubscribeHdl;

    void populateAddrList(const tAddressDescTbl &addr_tbl,
                          NFdbBase::FdbMsgAddressList &list, EFdbSocketType type);

    void onAllocServiceAddressReq(CBaseJob::Ptr &msg_ref);
    void onRegisterServiceReq(CBaseJob::Ptr &msg_ref);
    void onUnegisterServiceReq(CBaseJob::Ptr &msg_ref);
    void onQueryServiceReq(CBaseJob::Ptr &msg_ref);
    void onQueryServiceInterMachineReq(CBaseJob::Ptr &msg_ref);
    void onQueryHostReq(CBaseJob::Ptr &msg_ref);
    
    void onServiceOnlineReg(CFdbMessage *msg, const CFdbMsgSubscribeItem *sub_item);
    void onHostOnlineReg(CFdbMessage *msg, const CFdbMsgSubscribeItem *sub_item);
    void onHostInfoReg(CFdbMessage *msg, const CFdbMsgSubscribeItem *sub_item);

    CFdbAddressDesc *findAddress(EFdbSocketType type, const char *url);
    bool allocateTcpAddress(const std::string &svc_name, std::string &addr_url);
    bool allocateIpcAddress(const std::string &svc_name, std::string &addr_url);

    void buildUrl(const char *id, std::string &url);
    void buildUrl(uint32_t id, std::string &url);

    EFdbSocketType getSocketType(FdbSessionId_t sid);
    void removeService(tRegistryTbl::iterator &it);
    CFdbAddressDesc *createAddrDesc(const char *url);
    void connectToHostServer(const char *hs_url, bool is_local);
    void addServiceAddress(const std::string &svc_name,
                            FdbSessionId_t sid,
                            EFdbSocketType skt_type,
                            NFdbBase::FdbMsgAddressList *msg_addr_list);
    void setHostInfo(CFdbSession *session, NFdbBase::FdbMsgServiceInfo *msg_svc_info, const char *svc_name);
    void broadServiceAddress(tRegistryTbl::iterator &reg_it, CFdbMessage *msg, FdbMsgCode_t msg_code);
    bool bindNsAddress(tAddressDescTbl &addr_tbl);
    void checkUnconnectedAddress(CSvcRegistryEntry &desc_tbl, const char *svc_name);
    void buildSpecificTcpAddress(CFdbSession *session, int32_t port, std::string &out_url);
    void populateTokens(const CFdbToken::tTokenList &tokens,
                        NFdbBase::FdbMsgAddressList &list);

    void populateTokensRemote(const CFdbToken::tTokenList &tokens,
                               NFdbBase::FdbMsgAddressList &addr_list,
                               CFdbSession *session);
    void broadcastSvcAddrRemote(const CFdbToken::tTokenList &tokens,
                                 NFdbBase::FdbMsgAddressList &addr_list,
                                 CFdbMessage *msg);
    void broadcastSvcAddrRemote(const CFdbToken::tTokenList &tokens,
                                 NFdbBase::FdbMsgAddressList &addr_list,
                                 CFdbSession *session);
    void broadcastSvcAddrRemote(const CFdbToken::tTokenList &tokens,
                                 NFdbBase::FdbMsgAddressList &addr_list);
    void populateTokensLocal(const CFdbToken::tTokenList &tokens,
                              NFdbBase::FdbMsgAddressList &addr_list,
                              CFdbSession *session);
    void broadcastSvcAddrLocal(const CFdbToken::tTokenList &tokens,
                                NFdbBase::FdbMsgAddressList &addr_list,
                                CFdbMessage *msg);
    void broadcastSvcAddrLocal(const CFdbToken::tTokenList &tokens,
                                NFdbBase::FdbMsgAddressList &addr_list,
                                CFdbSession *session);
    void broadcastSvcAddrLocal(const CFdbToken::tTokenList &tokens,
                                NFdbBase::FdbMsgAddressList &addr_list);

    int32_t getSecurityLevel(CFdbSession *session, const char *svc_name);

    void dumpTokens(CFdbToken::tTokenList &tokens,
                    const NFdbBase::FdbMsgAddressList &list);

    uint32_t mTcpPortAllocator;
    uint32_t mIpcAllocator;
    CHostProxy *mHostProxy;
    std::string mInterface;
    int32_t mNsPort;
    CServerSecurityConfig mServerSecruity;
    
    friend class CInterNameProxy;
};

#endif
