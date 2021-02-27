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
#include "CFdbEventRouter.h"

class CBaseWorker;
class CFdbSessionContainer;
class CFdbMessage;
struct CFdbSocketAddr;
class CFdbSession;
class CApiSecurityConfig;
class CFdbBaseContext;

class CBaseEndpoint : public CEntityContainer<FdbSocketId_t, CFdbSessionContainer *>
                    , public CFdbBaseObject
{
public:
    typedef CEntityContainer<FdbObjectId_t, CFdbBaseObject *> tObjectContainer;

#define FDB_EP_RECONNECT_ENABLED        (1 << 9)
#define FDB_EP_RECONNECT_ACTIVATED      (1 << 10)
#define FDB_EP_ENABLE_UDP               (1 << 11)
#define FDB_EP_TCP_BLOCKING_MODE        (1 << 12)
#define FDB_EP_IPC_BLOCKING_MODE        (1 << 13)
#define FDB_EP_READ_ASYNC               (1 << 14)
#define FDB_EP_WRITE_ASYNC              (1 << 15)

    CBaseEndpoint(const char *name = 0, CBaseWorker *worker = 0, CFdbBaseContext *context = 0,
                  EFdbEndpointRole role = FDB_OBJECT_ROLE_UNKNOWN);
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

    void enableReconnect(bool active)
    {
        if (active)
        {
            mFlag |= FDB_EP_RECONNECT_ENABLED;
        }
        else
        {
            mFlag &= ~FDB_EP_RECONNECT_ENABLED;
        }
    }

    bool reconnectEnabled() const
    {
        return !!(mFlag & FDB_EP_RECONNECT_ENABLED);
    }

    void activateReconnect(bool active)
    {
        if (active)
        {
            mFlag |= FDB_EP_RECONNECT_ACTIVATED;
        }
        else
        {
            mFlag &= ~FDB_EP_RECONNECT_ACTIVATED;
        }
    }

    bool reconnectActivated() const
    {
        return !!(mFlag & FDB_EP_RECONNECT_ACTIVATED);
    }

    bool UDPEnabled() const
    {
        return !!(mFlag & FDB_EP_ENABLE_UDP);
    }

    void enableUDP(bool active)
    {
        if (active)
        {
            mFlag |= FDB_EP_ENABLE_UDP;
        }
        else
        {
            mFlag &= ~FDB_EP_ENABLE_UDP;
        }
    }

    void enableTcpBlockingMode(bool active)
    {
        if (active)
        {
            mFlag |= FDB_EP_TCP_BLOCKING_MODE;
        }
        else
        {
            mFlag &= ~FDB_EP_TCP_BLOCKING_MODE;
        }
    }

    bool enableTcpBlockingMode() const
    {
        return !!(mFlag & FDB_EP_TCP_BLOCKING_MODE);
    }

    void enableIpcBlockingMode(bool active)
    {
        if (active)
        {
            mFlag |= FDB_EP_IPC_BLOCKING_MODE;
        }
        else
        {
            mFlag &= ~FDB_EP_IPC_BLOCKING_MODE;
        }
    }

    bool enableIpcBlockingMode() const
    {
        return !!(mFlag & FDB_EP_IPC_BLOCKING_MODE);
    }

    void enableAysncWrite(bool active)
    {
        if (active)
        {
            mFlag |= FDB_EP_WRITE_ASYNC;
        }
        else
        {
            mFlag &= ~FDB_EP_WRITE_ASYNC;
        }
    }

    bool enableAysncWrite()
    {
        return !!(mFlag & FDB_EP_WRITE_ASYNC);
    }

    void enableAysncRead(bool active)
    {
        if (active)
        {
            mFlag |= FDB_EP_READ_ASYNC;
        }
        else
        {
            mFlag &= ~FDB_EP_READ_ASYNC;
        }
    }
    bool enableAysncRead()
    {
        return !!(mFlag & FDB_EP_READ_ASYNC);
    }

    void enableBlockingMode(bool active)
    {
        if (active)
        {
            mFlag |= FDB_EP_IPC_BLOCKING_MODE | FDB_EP_TCP_BLOCKING_MODE;
            mFlag &= ~(FDB_EP_WRITE_ASYNC | FDB_EP_READ_ASYNC);
        }
        else
        {
            mFlag &= ~(FDB_EP_IPC_BLOCKING_MODE | FDB_EP_TCP_BLOCKING_MODE);
            mFlag |= FDB_EP_WRITE_ASYNC | FDB_EP_READ_ASYNC;
        }
    }

    void prepareDestroy();

    void addPeerRouter(const char *peer_router_name)
    {
        mEventRouter.addPeer(peer_router_name);
    }

    CFdbBaseContext *context() const
    {
        return mContext;
    }

    tObjectContainer &getObjectContainer()
    {
        return mObjectContainer;
    }

protected:
    std::string mNsName;
    CFdbToken::tTokenList mTokens;

    void deleteSocket(FdbSocketId_t skid = FDB_INVALID_ID);
    void addSocket(CFdbSessionContainer *container);
    void getDefaultSvcUrl(std::string &url);
    void epid(FdbEndpointId_t epid)
    {
        mEpid = epid;
    }

    bool requestServiceAddress(const char *server_name = 0);
    bool releaseServiceAddress();
    void onSidebandInvoke(CBaseJob::Ptr &msg_ref);

    virtual bool onMessageAuthentication(CFdbMessage *msg, CFdbSession *session);
    virtual bool onEventAuthentication(CFdbMessage *msg, CFdbSession *session);

    virtual bool onMessageAuthentication(CFdbMessage *msg);
    virtual bool onEventAuthentication(CFdbMessage *msg);

    void setNsName(const char *name)
    {
        if (name)
        {
            mNsName = name;
            if (mName.empty())
            {
                mName = mNsName;
            }
        }
    }
    void onPublish(CBaseJob :: Ptr &msg_ref);
    CFdbBaseContext *mContext;

private:
    tObjectContainer mObjectContainer;

    uint32_t mSessionCnt;
    FdbObjectId_t mSnAllocator;
    FdbEndpointId_t mEpid;
    CFdbEventRouter mEventRouter;
    
    CFdbSession *preferredPeer();
    void checkAutoRemove();
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
    void updateSessionInfo(CFdbSession *session);
    CFdbSession *connected(const CFdbSocketAddr &addr);
    CFdbSession *bound(const CFdbSocketAddr &addr);
    virtual const CApiSecurityConfig *getApiSecurityConfig()
    {
        return 0;
    }

    friend class CFdbSession;
    friend class CFdbUDPSession;
    friend class CFdbMessage;
    friend class CFdbBaseContext;
    friend class CBaseServer;
    friend class CBaseClient;
    friend class CIntraNameProxy;
    friend class CHostProxy;
    friend class CFdbEventRouter;
    friend class CFdbBaseObject;
    friend class CServerSocket;
    friend class CRegisterJob;
    friend class CDestroyJob;
    friend class CLogProducer;
};

#endif
