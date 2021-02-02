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

#ifndef _CFDBSESSION_
#define _CFDBSESSION_

#include <string>
#include <common_base/CBaseFdWatch.h>
#include <common_base/common_defs.h>
//#include "CFdbMessage.h"
#include <common_base/CBaseJob.h>
#include <common_base/CEntityContainer.h>
#include <common_base/CFdbSessionContainer.h>
#include <common_base/CFdbMessage.h>

struct CFdbSessionInfo
{
    CFdbSocketInfo mContainerSocket;
    CFdbSocketCredentials const *mCred;
    CFdbSocketConnInfo const *mConn;
};

class CFdbSessionContainer;
class CSocketImp;
class CFdbMessage;

namespace NFdbBase {
    class CFdbMessageHeader;
}
class CFdbSession : public CBaseFdWatch
{
public:
    CFdbSession(FdbSessionId_t sid, CFdbSessionContainer *container, CSocketImp *socket);
    virtual ~CFdbSession();

    bool sendMessage(const uint8_t *buffer, int32_t size);
    bool sendMessage(CBaseJob::Ptr &ref);
    bool sendMessage(CFdbMessage *msg);
    bool sendUDPMessage(CFdbMessage *msg);
    FdbSessionId_t sid() const
    {
        return mSid;
    }
    void sid(FdbSessionId_t sid)
    {
        mSid = sid;
    }
    CFdbSessionContainer *container()
    {
        return mContainer;
    }
    void securityLevel(int32_t level);
    int32_t securityLevel() const
    {
        return mSecurityLevel;
    }
    void token(const char *token);
    std::string &token()
    {
        return mToken;
    }
    const std::string &senderName() const
    {
        return mSenderName;
    }
    void senderName(const char *name)
    {
        mSenderName = name;
    }
    CBASE_tProcId pid() const
    {
        return mPid;
    }
    void pid(CBASE_tProcId pid)
    {
        mPid = pid;
    }
    const CFdbSocketAddr &getPeerUDPAddress() const
    {
        return mUDPAddr;
    }
    bool hostIp(std::string &host_ip);
    bool peerIp(std::string &host_ip);

    const std::string &getEndpointName() const;
    void terminateMessage(CBaseJob::Ptr &job, int32_t status, const char *reason);
    void terminateMessage(FdbMsgSn_t msg, int32_t status, const char *reason = 0);
    void getSessionInfo(CFdbSessionInfo &info);
    CFdbMessage *peepPendingMessage(FdbMsgSn_t sn);

    bool connected(const CFdbSocketAddr &addr);
    bool bound(const CFdbSocketAddr &addr);
    CSocketImp *getSocket()
    {
        return mSocket;
    }
protected:
    void onInput();
    void onError();
    void onHup();
    void onInputReady(const uint8_t *data, int32_t size);
    int32_t writeStream(const uint8_t *data, int32_t size);
    int32_t readStream(uint8_t *data, int32_t size);
private:
    typedef CEntityContainer<FdbMsgSn_t, CBaseJob::Ptr> PendingMsgTable_t;

    void doRequest(NFdbBase::CFdbMessageHeader &head);
    void doResponse(NFdbBase::CFdbMessageHeader &head);
    void doBroadcast(NFdbBase::CFdbMessageHeader &head);
    void doSubscribeReq(NFdbBase::CFdbMessageHeader &head, bool subscribe);
    void doUpdate(NFdbBase::CFdbMessageHeader &head);
    void checkLogEnabled(CFdbMessage *msg);
    bool receiveData(uint8_t *buf, int32_t size);
    void parsePrefix(const uint8_t *data, int32_t size);
    void processPayload(const uint8_t *data, int32_t size);

    PendingMsgTable_t mPendingMsgTable;
    FdbSessionId_t mSid;
    CFdbSessionContainer *mContainer;
    CSocketImp *mSocket;
    int32_t mSecurityLevel;
    std::string mToken;
    std::string mSenderName;
    int32_t mRecursiveDepth;
    CFdbSocketAddr mUDPAddr;
    CBASE_tProcId mPid;
    uint8_t *mPayloadBuffer;
    uint8_t mPrefixBuffer[CFdbMessage::mPrefixSize];
    CFdbMsgPrefix mMsgPrefix;
};

#endif
