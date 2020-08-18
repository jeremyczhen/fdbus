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

#ifndef _CHOSTSERVER_H_
#define _CHOSTSERVER_H_
#include <map>
#include <string>
#include <vector>
#include <common_base/CBaseServer.h>
#include <common_base/CMethodLoopTimer.h>
#include <utils/CNsConfig.h>
#include <security/CHostSecurityConfig.h>

namespace NFdbBase {
    class FdbMsgHostRegisterAck;
    class FdbMsgHostAddress;
}
class CFdbMessage;
class CHostServer : public CBaseServer
{
public:
    CHostServer();
    ~CHostServer();

protected:
    void onSubscribe(CBaseJob::Ptr &msg_ref);
    void onInvoke(CBaseJob::Ptr &msg_ref);
    void onOffline(FdbSessionId_t sid, bool is_last);
private:
    struct CHostInfo
    {
        std::string mHostName;
        std::string mIpAddress;
        std::string mNsUrl;
        int32_t mHbCount;
        bool mReady;
        bool mAuthorized;
        CFdbToken::tTokenList mTokens;
    };
    typedef std::map<FdbSessionId_t, CHostInfo> tHostTbl;
    tHostTbl mHostTbl;
    CFdbMessageHandle<CHostServer> mMsgHdl;
    CFdbSubscribeHandle<CHostServer> mSubscribeHdl;

    void onRegisterHostReq(CBaseJob::Ptr &msg_ref);
    void onUnregisterHostReq(CBaseJob::Ptr &msg_ref);
    void onQueryHostReq(CBaseJob::Ptr &msg_ref);
    void onHeartbeatOk(CBaseJob::Ptr &msg_ref);
    void onHostReady(CBaseJob::Ptr &msg_ref);

    void onHostOnlineReg(CFdbMessage *msg, const CFdbMsgSubscribeItem *sub_item);

    void broadcastSingleHost(FdbSessionId_t sid, bool online, CHostInfo &info);

    class CHeartBeatTimer : public CMethodLoopTimer<CHostServer>
    {
    public:
        CHeartBeatTimer(CHostServer *proxy)
            : CMethodLoopTimer<CHostServer>(CNsConfig::getHeartBeatInterval(), true,
                                            proxy, &CHostServer::broadcastHeartBeat)
        {
        }
    };
    CHeartBeatTimer mHeartBeatTimer;
    void broadcastHeartBeat(CMethodLoopTimer<CHostServer> *timer);
    CHostSecurityConfig mHostSecurity;

    void populateTokens(const CFdbToken::tTokenList &tokens,
                        NFdbBase::FdbMsgHostRegisterAck &list);
    void addToken(const CHostInfo &this_host, const CHostInfo &that_host,
                    NFdbBase::FdbMsgHostAddress &host_addr);
};

#endif
