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

#ifndef _CBASEENDPOINT_H_
#define _CBASEENDPOINT_H_

#include <string>
#include <vector>
#include "common_defs.h"
#include "CEntityContainer.h"
#include "CFdbBaseObject.h"
#include "CMethodJob.h"
#include "CFdbToken.h"
#include "CFdbSimpleSerializer.h"

class CBaseWorker;
class CFdbSessionContainer;
class CFdbMessage;

class CBaseEndpoint : public CEntityContainer<FdbSocketId_t, CFdbSessionContainer *>
                    , public CFdbBaseObject
{
public:
    CBaseEndpoint(const char *name, CBaseWorker *worker = 0, EFdbEndpointRole role = FDB_OBJECT_ROLE_UNKNOWN);
    ~CBaseEndpoint();

    const std::string &nsName() const
    {
        return mNsName;
    }

    /*
     * Kick out a session (usually called by server to kick out a client)
     */
    void kickOut(FdbSessionId_t sid);
    bool hostIp(std::string &host_ip, CFdbSession *session = 0);
    bool peerIp(std::string &peer_ip, CFdbSession *session = 0);
	
    uint32_t getSessionCount()
    {
        return mSessionCnt;
    }

    bool connected()
    {
        return !!mSessionCnt;
    }

    FdbEndpointId_t epid() const
    {
        return mEpid;
    }

    void prepareDestroy();

protected:
    std::string mNsName;

    void deleteSocket(FdbSocketId_t skid = FDB_INVALID_ID);
    void addSocket(CFdbSessionContainer *container);
    void getDefaultSvcUrl(std::string &url);
    void epid(FdbEndpointId_t epid)
    {
        mEpid = epid;
    }

    bool requestServiceAddress(const char *server_name = 0);
    bool releaseServiceAddress();
    void onSidebandInvoke(CBaseJob::Ptr &msg_ref)
    {
        CFdbBaseObject::onSidebandInvoke(msg_ref);
    }

private:
    typedef CEntityContainer<FdbObjectId_t, CFdbBaseObject *> tObjectContainer;
    
    CFdbSession *preferredPeer();
    void checkAutoRemove();
    CFdbSessionContainer *getSocketByUrl(const char *url);
    void getUrlList(std::vector<std::string> &url_list);
    FdbObjectId_t addObject(CFdbBaseObject *obj);
    void removeObject(CFdbBaseObject *obj);
    CFdbBaseObject *getObject(CFdbMessage *msg, bool server_only);
    void unsubscribeSession(CFdbSession *session);
    bool addConnectedSession(CFdbSessionContainer *socket, CFdbSession *session);
    void deleteConnectedSession(CFdbSession *session);

    FdbEndpointId_t registerSelf();
    void destroySelf(bool prepare);
    void callRegisterEndpoint(CBaseWorker *worker, CMethodJob<CBaseEndpoint> *job, CBaseJob::Ptr &ref);
    void callDestroyEndpoint(CBaseWorker *worker, CMethodJob<CBaseEndpoint> *job, CBaseJob::Ptr &ref);
    CFdbBaseObject *findObject(FdbObjectId_t obj_id, bool server_only);
    bool importTokens(const CFdbParcelableArray<std::string> &in_tokens);
    int32_t checkSecurityLevel(const char *token);
    void updateSecurityLevel();

    tObjectContainer mObjectContainer;

    uint32_t mSessionCnt;
    FdbObjectId_t mSnAllocator;
    CFdbToken::tTokenList mTokens;
    FdbEndpointId_t mEpid;

    friend class CFdbSession;
    friend class CFdbMessage;
    friend class CFdbContext;
    friend class CBaseServer;
    friend class CBaseClient;
    friend class CIntraNameProxy;
    friend class CHostProxy;
    friend class CFdbBaseObject;
    friend class CServerSocket;
    friend class CRegisterJob;
    friend class CDestroyJob;
};

#endif
