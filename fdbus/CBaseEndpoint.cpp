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

#include <common_base/CBaseEndpoint.h>
#include <common_base/CFdbContext.h>
#include <common_base/CFdbSession.h>
#include <common_base/CFdbMessage.h>
#include <common_base/CIntraNameProxy.h>
#include <utils/Log.h>

CBaseEndpoint::CBaseEndpoint(const char *name, CBaseWorker *worker, EFdbEndpointRole role)
    : CFdbBaseObject(name, worker, role)
    , mSessionCnt(0)
    , mSnAllocator(1)
    , mEpid(FDB_INVALID_ID)
{
    mObjId = FDB_OBJECT_MAIN;
    mEndpoint = this;
    registerSelf();
}

CBaseEndpoint::~CBaseEndpoint()
{
    destroySelf(false);
}

void CBaseEndpoint::prepareDestroy()
{
    CFdbBaseObject::prepareDestroy();
    destroySelf(true);
}

void CBaseEndpoint::addSocket(CFdbSessionContainer *container)
{
    insertEntry(container->skid(), container);
}

void CBaseEndpoint::deleteSocket(FdbSocketId_t skid)
{
    if (fdbValidFdbId(skid))
    {
        CFdbSessionContainer *container = 0;
        (void)retrieveEntry(skid, container);
        if (container)
        {
            delete container; // also deleted from socket container
        }
    }
    else
    {
        EntryContainer_t &containers = getContainer();
        while (!containers.empty())
        {
            auto it = containers.begin();
            delete it->second; // also deleted from socket container
        }
    }
}

void CBaseEndpoint::getDefaultSvcUrl(std::string &svc_url)
{
    svc_url = FDB_URL_SVC;
    if (mNsName.empty())
    {
        svc_url += mName;
    }
    else
    {
        svc_url += mNsName;
    }
}

CFdbSession *CBaseEndpoint::preferredPeer()
{
    CFdbSession *session = 0;
    if (fdbValidFdbId(mSid))
    {
        session = CFdbContext::getInstance()->getSession(mSid);
    }
    if (session)
    {
        return session;
    }
    auto &containers = getContainer();
    auto it = containers.begin();
    if (it == containers.end())
    {
        session = 0;
    }
    else
    {
        auto sessions = it->second;
        session = sessions->getDefaultSession();
    }

    return session;
}

void CBaseEndpoint::checkAutoRemove()
{
    if (autoRemove() && !getSessionCount())
    {
        delete this;
    }
}

CFdbSessionContainer *CBaseEndpoint::getSocketByUrl(const char *url)
{
    auto &containers = getContainer();
    for (auto it = containers.begin(); it != containers.end(); ++it)
    {
        auto container = it->second;
        CFdbSocketInfo info;
        container->getSocketInfo(info);
        if (!info.mAddress->mUrl.compare(url))
        {
            return container;
        }
    }

    return 0;
}

void CBaseEndpoint::getUrlList(std::vector<std::string> &url_list)
{
    auto &containers = getContainer();
    for (auto it = containers.begin(); it != containers.end(); ++it)
    {
        auto container = it->second;
        CFdbSocketInfo info;
        container->getSocketInfo(info);
        url_list.push_back(info.mAddress->mUrl);
    }
}

FdbObjectId_t CBaseEndpoint::addObject(CFdbBaseObject *obj)
{
    auto obj_id = obj->objId();
    if (obj_id == FDB_OBJECT_MAIN)
    {
        return FDB_INVALID_ID;
    }

    if (!fdbValidFdbId(obj_id))
    {
        do
        {
            obj_id = mObjectContainer.allocateEntityId();
        } while (obj_id == FDB_OBJECT_MAIN);
    }

    if (obj->role() == FDB_OBJECT_ROLE_SERVER)
    {
        auto o = findObject(obj_id, true);
        if (o)
        {
            if (o == obj)
            {
                return obj_id;
            }
            else
            {
                LOG_E("CBaseEndpoint: server object %d already exist!\n", obj_id);
                return FDB_INVALID_ID;
            }
        }
    }

    obj_id = FDB_OBJECT_MAKE_ID(mSnAllocator++, obj_id);
    obj->objId(obj_id);
    if (mObjectContainer.insertEntry(obj_id, obj))
    {
        return FDB_INVALID_ID;
    }

    obj->enableMigrate(true);
    bool is_first = true;
    auto &containers = getContainer();
    for (auto socket_it = containers.begin(); socket_it != containers.end(); ++socket_it)
    {
        auto container = socket_it->second;
        if (!container->mConnectedSessionTable.empty())
        {
            // get a snapshot of the table to avoid modification of the table in callback
            auto tbl = container->mConnectedSessionTable;
            for (auto session_it = tbl.begin(); session_it != tbl.end(); ++session_it)
            {
                auto session = *session_it;
                CFdbSessionInfo info;
                session->getSessionInfo(info);

                if (!obj->authentication(info))
                {
                    continue;
                }
                obj->notifyOnline(session, is_first);
                is_first = false;
            }
        }
    }

    return obj_id;
}

void CBaseEndpoint::removeObject(CFdbBaseObject *obj)
{
    auto is_last = false;
    auto session_cnt = mSessionCnt;

    auto &containers = getContainer();
    for (auto socket_it = containers.begin(); socket_it != containers.end(); ++socket_it)
    {
        auto container = socket_it->second;
        if (!container->mConnectedSessionTable.empty())
        {
            // get a snapshot of the table to avoid modification of the table in callback
            auto tbl = container->mConnectedSessionTable;
            for (auto session_it = tbl.begin(); session_it != tbl.end(); ++session_it)
            {
                if (session_cnt-- == 1)
                {
                    is_last = true;
                }
            
                obj->notifyOffline(*session_it, is_last);
            }
        }
    }
    
    mObjectContainer.deleteEntry(obj->objId());
    // obj->objId(FDB_INVALID_ID);
    obj->enableMigrate(false);
}

void CBaseEndpoint::unsubscribeSession(CFdbSession *session)
{
    auto &object_tbl = mObjectContainer.getContainer();
    for (auto it = object_tbl.begin(); it != object_tbl.end(); ++it)
    {
        auto object = it->second;
        object->unsubscribe(session);
    }
    
    unsubscribe(session);
}

class CKickOutSessionJob : public CBaseJob
{
public:
    CKickOutSessionJob(FdbSessionId_t sid)
        : CBaseJob(JOB_FORCE_RUN)
        , mSid(sid)
    {
    }
protected:
    void run(CBaseWorker *worker, Ptr &ref)
    {
        CFdbContext::getInstance()->deleteSession(mSid);
    }
private:
    FdbSessionId_t mSid;
};

void CBaseEndpoint::kickOut(FdbSessionId_t sid)
{
    CFdbContext::getInstance()->sendAsyncEndeavor(new CKickOutSessionJob(sid));
}

CFdbBaseObject *CBaseEndpoint::getObject(CFdbMessage *msg, bool server_only)
{
    auto obj_id = msg->objectId();
    if (obj_id == FDB_OBJECT_MAIN)
    {
        return this;
    }
    
    CFdbBaseObject *object = 0;
    bool tried_to_create = false;

    while (1)
    {
        object = findObject(obj_id, server_only);
        if (object || tried_to_create)
        {
            break;
        }
        
        onCreateObject(this, msg);
        tried_to_create = true;
    }

    return object;
}

bool CBaseEndpoint::addConnectedSession(CFdbSessionContainer *socket, CFdbSession *session)
{
    CFdbSessionInfo info;
    session->getSessionInfo(info);
    if (!authentication(info))
    {
        return false;
    }

    bool is_first = !mSessionCnt;

    socket->addSession(session);
    mSessionCnt++;
    
    auto &object_tbl = mObjectContainer.getContainer();
    if (!object_tbl.empty())
    {
        // get a snapshot of the table to avoid modification of the table in callback
        auto object_tbl = mObjectContainer.getContainer();
        for (auto it = object_tbl.begin(); it != object_tbl.end(); ++it)
        {
            auto object = it->second;
            if (!object->authentication(info))
            {
                continue;
            }
            object->notifyOnline(session, is_first);
        }
    }

    notifyOnline(session, is_first);
    
    return true;
}

void CBaseEndpoint::deleteConnectedSession(CFdbSession *session)
{
    bool is_last = (mSessionCnt == 1);
    
    session->container()->removeSession(session);
    if (mSessionCnt > 0)
    {
        mSessionCnt--;
    }
    else
    {
        LOG_E("CBaseEndpoint: session count < 0 for object %s!\n", mName.c_str());
    }

    notifyOffline(session, is_last);
    
    auto &object_tbl = mObjectContainer.getContainer();
    if (!object_tbl.empty())
    {
        // get a snapshot of the table to avoid modification of the table in callback
        auto object_tbl = mObjectContainer.getContainer();
        for (auto it = object_tbl.begin(); it != object_tbl.end(); ++it)
        {
            CFdbBaseObject *object = it->second;
            object->notifyOffline(session, is_last);
        }
    }
}

bool CBaseEndpoint::hostIp(std::string &host_ip, CFdbSession *session)
{
    if (!session)
    {
        session = preferredPeer();
    }
    return session ? session->hostIp(host_ip) : false;
}

bool CBaseEndpoint::peerIp(std::string &peer_ip, CFdbSession *session)
{
    if (!session)
    {
        session = preferredPeer();
    }
    return session ? session->peerIp(peer_ip) : false;
}

CFdbBaseObject *CBaseEndpoint::findObject(FdbObjectId_t obj_id, bool server_only)
{
    auto &object_tbl = mObjectContainer.getContainer();
    for (auto it = object_tbl.begin(); it != object_tbl.end(); ++it)
    {
        auto object = it->second;
        if (server_only)
        {
            if ((object->role() == FDB_OBJECT_ROLE_SERVER) &&
                (FDB_OBJECT_GET_CLASS(obj_id) == 
                    FDB_OBJECT_GET_CLASS(object->objId())))
            {
                return object;
            }
        }
        else if (obj_id == object->objId())
        {
            return object;
        }
    }
    return 0;
}

//================================== register ==========================================
class CRegisterJob : public CMethodJob<CBaseEndpoint>
{
public:
    CRegisterJob(CBaseEndpoint *object, FdbEndpointId_t &epid)
        : CMethodJob<CBaseEndpoint>(object, &CBaseEndpoint::callRegisterEndpoint, JOB_FORCE_RUN)
        , mEpid(epid)
    {
    }

    FdbEndpointId_t &mEpid;
};

void CBaseEndpoint::callRegisterEndpoint(CBaseWorker *worker,
                CMethodJob<CBaseEndpoint> *job, CBaseJob::Ptr &ref)
{
    auto the_job = fdb_dynamic_cast_if_available<CRegisterJob *>(job);
    if (the_job)
    {
        the_job->mEpid = CFdbContext::getInstance()->registerEndpoint(this);
        registered(true);
    }
}

FdbEndpointId_t CBaseEndpoint::registerSelf()
{
    auto epid = mEpid;
    if (!registered())
    {
        CFdbContext::getInstance()->sendSyncEndeavor(new CRegisterJob(this, epid));
        if (fdbValidFdbId(epid))
        {
            registered(true);
        }
    }
    return epid;
}

//================================== unregister ==========================================
class CDestroyJob : public CMethodJob<CBaseEndpoint>
{
public:
    CDestroyJob(CBaseEndpoint *object, bool prepare)
        : CMethodJob<CBaseEndpoint>(object, &CBaseEndpoint::callDestroyEndpoint, JOB_FORCE_RUN)
        , mPrepare(prepare)
    {
    }
    bool mPrepare;
};

void CBaseEndpoint::callDestroyEndpoint(CBaseWorker *worker,
            CMethodJob<CBaseEndpoint> *job, CBaseJob::Ptr &ref)
{
    auto the_job = fdb_dynamic_cast_if_available<CDestroyJob *>(job);
    if (the_job->mPrepare)
    {
        CFdbContext::getInstance()->unregisterEndpoint(this);
    }
    else
    {
        auto &object_tbl = mObjectContainer.getContainer();
        while (!object_tbl.empty())
        {
            auto it = object_tbl.begin();
            CFdbBaseObject *object = it->second;
            removeObject(object);
        }

        deleteSocket();
        // ensure if prepare is not called, it still can be unregistered.
        CFdbContext::getInstance()->unregisterEndpoint(this);
    }
}

void CBaseEndpoint::destroySelf(bool prepare)
{
    if (registered())
    {
        CFdbContext::getInstance()->sendSyncEndeavor(new CDestroyJob(this, prepare));
        registered(false);
    }
}

bool CBaseEndpoint::importTokens(const CFdbParcelableArray<std::string> &in_tokens)
{
    auto need_update = false;
    
    if (in_tokens.size() != (uint32_t)mTokens.size())
    {
        need_update = true;
    }
    else
    {
        auto it = mTokens.begin();
        auto in_it = in_tokens.pool().begin();
         for (; in_it != in_tokens.pool().end(); ++in_it, ++it)
        {
            if (it->compare(*in_it))
            {
                need_update = true;
                break;
            }
        }
    }
    
    if (need_update)
    {
        mTokens.clear();
        for (auto in_it = in_tokens.pool().begin(); in_it != in_tokens.pool().end(); ++in_it)
        {
            mTokens.push_back(*in_it);
        }
    }
    return need_update;
}

int32_t CBaseEndpoint::checkSecurityLevel(const char *token)
{
    return CFdbToken::checkSecurityLevel(mTokens, token);
}

// given token in session, update security level
void CBaseEndpoint::updateSecurityLevel()
{
    auto &containers = getContainer();
    for (auto socket_it = containers.begin(); socket_it != containers.end(); ++socket_it)
    {
        auto container = socket_it->second;
        auto &tbl = container->mConnectedSessionTable;
        for (auto session_it = tbl.begin(); session_it != tbl.end(); ++session_it)
        {
            auto session = *session_it;
            session->securityLevel(checkSecurityLevel(session->token().c_str()));
        }
    }
}

bool CBaseEndpoint::requestServiceAddress(const char *server_name)
{
    if (role() == FDB_OBJECT_ROLE_NS_SERVER)
    {
        return false;
    }
    if (server_name)
    {
        mNsName = server_name;
    }
    if (mNsName.empty())
    {
        // server name is not ready: this might happen when name server
        // is connected but bind() or connect() is not called.
        return false;
    }

    auto name_proxy = FDB_CONTEXT->getNameProxy();
    if (!name_proxy)
    {
        return false;
    }

    if (role() == FDB_OBJECT_ROLE_SERVER)
    {
        name_proxy->registerService(mNsName.c_str());
        name_proxy->addAddressListener(mNsName.c_str());
    }
    else
    {
        name_proxy->addServiceListener(mNsName.c_str(), FDB_INVALID_ID);
    }
    return true;
}

bool CBaseEndpoint::releaseServiceAddress()
{
    if (role() == FDB_OBJECT_ROLE_NS_SERVER)
    {
        return false;
    }
    if (mNsName.empty())
    {
        // server name is not ready: this might happen when name server
        // is connected but bind() or connect() is not called.
        return false;
    }

    auto name_proxy = FDB_CONTEXT->getNameProxy();
    if (!name_proxy)
    {
        return false;
    }

    if (role() == FDB_OBJECT_ROLE_SERVER)
    {
        name_proxy->removeAddressListener(mNsName.c_str());
        name_proxy->unregisterService(mNsName.c_str());
    }
    else
    {
        name_proxy->removeServiceListener(mNsName.c_str());
    }
    return true;
}
