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

#include "CBaseFdWatch.h"
#include "common_defs.h"
#include "CFdbMessage.h"
#include "CEntityContainer.h"
#include "CSocketImp.h"
#include "CFdbSessionContainer.h"

struct CFdbSessionInfo
{
    CFdbSocketInfo mSocketInfo;
    CFdbSocketCredentials const *mCred;
    CFdbSocketConnInfo const *mConn;
};

class CFdbSessionContainer;
class CFdbMessageHeader;
class CFdbSession : public CBaseFdWatch
{
public:
    CFdbSession(FdbSessionId_t sid, CFdbSessionContainer *container, CSocketImp *socket);
    virtual ~CFdbSession();

    bool sendMessage(const uint8_t *buffer, int32_t size);
    bool sendMessage(CBaseJob::Ptr &ref);
    bool sendMessage(CFdbMessage *msg);
    FdbSessionId_t sid()
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
    bool internalError()
    {
        return mInternalError;
    }
    void internalError(bool active)
    {
        mInternalError = active;
    }
    void securityLevel(int32_t level);
    int32_t securityLevel()
    {
        return mSecurityLevel;
    }
    void token(const char *token);
    std::string &token()
    {
        return mToken;
    }
    bool hostIp(std::string &host_ip);
    bool peerIp(std::string &host_ip);

    bool receiveData(uint8_t *buf, int32_t size);

    const std::string &getEndpointName() const;
    void terminateMessage(CBaseJob::Ptr &job, int32_t status, const char *reason);
    void terminateMessage(FdbMsgSn_t msg, int32_t status, const char *reason = 0);
    void getSessionInfo(CFdbSessionInfo &info);
    CFdbMessage *peepPendingMessage(FdbMsgSn_t sn);
protected:
    void onInput(bool &io_error);
    void onError();
    void onHup();
private:
    typedef CEntityContainer<FdbMsgSn_t, CBaseJob::Ptr> PendingMsgTable_t;

    void doRequest(CFdbMessageHeader &head, CFdbMessage::CFdbMsgPrefix &prefix, uint8_t *buffer);
    void doResponse(CFdbMessageHeader &head, CFdbMessage::CFdbMsgPrefix &prefix, uint8_t *buffer);
    void doBroadcast(CFdbMessageHeader &head, CFdbMessage::CFdbMsgPrefix &prefix, uint8_t *buffer);
    void doSubscribeReq(CFdbMessageHeader &head, CFdbMessage::CFdbMsgPrefix &prefix, uint8_t *buffer, bool subscribe);
    void doUpdate(CFdbMessageHeader &head, CFdbMessage::CFdbMsgPrefix &prefix, uint8_t *buffer);

    PendingMsgTable_t mPendingMsgTable;
    FdbSessionId_t mSid;
    CFdbSessionContainer *mContainer;
    CSocketImp *mSocket;
    bool mInternalError;
    int32_t mSecurityLevel;
    std::string mToken;
};

#endif
