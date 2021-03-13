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
#include <utils/CFdbIfMessageHeader.h>
#include <common_base/CApiSecurityConfig.h>
#include "CIntraNameProxy.h"
#include <utils/Log.h>

CBaseEndpoint::CBaseEndpoint(const char *name, CBaseWorker *worker, CFdbBaseContext *context,
                             EFdbEndpointRole role)
    : CFdbBaseObject(name, worker, context, role)
    , mSessionCnt(0)
    , mSnAllocator(1)
    , mEpid(FDB_INVALID_ID)
    , mEventRouter(this)
{
    autoRemove(false);
    mContext = context ? context : FDB_CONTEXT;
    mContext->init(); // will not multi-initialization
    enableBlockingMode(false);
    mObjId = FDB_OBJECT_MAIN;
    mEndpoint = this;
    registerSelf();
}

CBaseEndpoint::~CBaseEndpoint()
{
    if (!mSessionContainer.getContainer().empty())
    {
        LOG_E("~CBaseEndpoint: Unable to destroy context since there are active sessions!\n");
    }
    destroySelf(false);
}

void CBaseEndpoint::prepareDestroy()
{
    autoRemove(false);
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
    auto &containers = getContainer();
    auto it = containers.begin();
    if (it != containers.end())
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

    if (mObjectContainer.findEntry(obj))
    {
        return obj_id;
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
                if (!obj->authentication(session))
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
    if (!mObjectContainer.findEntry(obj))
    {
        return;
    }

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

    obj->enableMigrate(false);
    if (obj->mWorker)
    {
        obj->mWorker->flush();
    }
    mObjectContainer.deleteEntry(obj->objId());
    if (obj->autoRemove())
    {
        delete obj;
    }
    // obj->objId(FDB_INVALID_ID);
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

class CKickOutSessionJob : public CMethodJob<CBaseEndpoint>
{
public:
    CKickOutSessionJob(CBaseEndpoint *object, FdbSessionId_t sid)
        : CMethodJob<CBaseEndpoint>(object, &CBaseEndpoint::callKickOutSession, JOB_FORCE_RUN)
        , mSid(sid)
    {
    }

    FdbSessionId_t mSid;
};

void CBaseEndpoint::callKickOutSession(CBaseWorker *worker,
                CMethodJob<CBaseEndpoint> *job, CBaseJob::Ptr &ref)
{
    auto the_job = fdb_dynamic_cast_if_available<CKickOutSessionJob *>(job);
    deleteSession(the_job->mSid);
}

void CBaseEndpoint::kickOut(FdbSessionId_t sid)
{
    mContext->sendAsyncEndeavor(new CKickOutSessionJob(this, sid));
}

CFdbBaseObject *CBaseEndpoint::getObject(CFdbMessage *msg, bool server_only)
{
    auto obj_id = msg->objectId();
    if (obj_id == FDB_OBJECT_MAIN)
    {
        msg->context(mContext);
        return this;
    }
    
    CFdbBaseObject *object = 0;
    bool tried_to_create = false;

    while (1)
    {
        object = findObject(obj_id, server_only);
        if (object)
        {
            msg->context(object->endpoint()->context());
            break;
        }
        else if (tried_to_create)
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
    if (!authentication(session))
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
            if (!object->authentication(session))
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
        the_job->mEpid = mContext->registerEndpoint(this);
    }
}

FdbEndpointId_t CBaseEndpoint::registerSelf()
{
    FdbEndpointId_t epid = FDB_INVALID_ID;
    mContext->sendSyncEndeavor(new CRegisterJob(this, epid));
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
        auto &object_tbl = mObjectContainer.getContainer();
        while (!object_tbl.empty())
        {
            auto it = object_tbl.begin();
            CFdbBaseObject *object = it->second;
            removeObject(object);
        }

        mContext->unregisterEndpoint(this);
    }
    else
    {
        deleteSocket();
        // ensure if prepare is not called, it still can be unregistered.
        mContext->unregisterEndpoint(this);
    }
}

void CBaseEndpoint::destroySelf(bool prepare)
{
    mContext->sendSyncEndeavor(new CDestroyJob(this, prepare));
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
        setNsName(server_name);
    }
    if (mNsName.empty())
    {
        // server name is not ready: this might happen when name server
        // is connected but bind() or connect() is not called.
        return false;
    }
    server_name = mNsName.c_str();

    auto name_proxy = FDB_CONTEXT->getNameProxy();
    if (!name_proxy)
    {
        return false;
    }

    if (role() == FDB_OBJECT_ROLE_SERVER)
    {
        if (mContext->serverAlreadyRegistered(this))
        {
            LOG_E("CBaseEndpoint: service %s exists and fails to bind again!\n", server_name);
            return false;
        }
        name_proxy->registerService(server_name, this);
        name_proxy->addAddressListener(server_name);
    }
    else
    {
        name_proxy->listenOnService(server_name, this);
    }
    mEventRouter.connectPeers();
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

void CBaseEndpoint::onSidebandInvoke(CBaseJob::Ptr &msg_ref)
{
    auto msg = castToMessage<CFdbMessage *>(msg_ref);
    switch (msg->code())
    {
        case FDB_SIDEBAND_SESSION_INFO:
        {
            NFdbBase::FdbSessionInfo sinfo;
            CFdbParcelableParser parser(sinfo);
            if (!msg->deserialize(parser))
            {
                return;
            }
            auto session = msg->getSession();
            if (!session)
            {
                return;
            }
            session->senderName(sinfo.sender_name().c_str());
            session->pid((CBASE_tProcId)sinfo.pid());
            std::string peer_ip;
            int32_t udp_port = FDB_INET_PORT_INVALID;
            if (sinfo.has_udp_port())
            {
                udp_port = sinfo.udp_port();
            }
            if (FDB_VALID_PORT(udp_port) && session->peerIp(peer_ip))
            {
                CFdbSocketAddr &udp_addr = const_cast<CFdbSocketAddr &>(session->getPeerUDPAddress());
                udp_addr.mAddr = peer_ip;
                udp_addr.mPort = udp_port;
            }
        }
        break;
        default:
            CFdbBaseObject::onSidebandInvoke(msg_ref);
        break;
    }
}


void CBaseEndpoint::updateSessionInfo(CFdbSession *session)
{
    if ((role() != FDB_OBJECT_ROLE_SERVER) && (role() != FDB_OBJECT_ROLE_CLIENT))
    {
        return;
    }

    CFdbSessionInfo sinfo_connected;
    session->getSessionInfo(sinfo_connected);

    int32_t udp_port = FDB_INET_PORT_INVALID;
    if (sinfo_connected.mContainerSocket.mAddress->mType == FDB_SOCKET_TCP)
    {
        if (role() == FDB_OBJECT_ROLE_CLIENT)
        {
            // for client, self IP address is unknown until session is connected.
            // for server, UDP is created upon binding because IP address of UDP
            //  is the same as that of TCP socket
            session->container()->bindUDPSocket(sinfo_connected.mConn->mSelfAddress.mAddr.c_str());
        }

        CFdbSocketInfo socket_info;
        if (session->container()->getUDPSocketInfo(socket_info) &&
            FDB_VALID_PORT(socket_info.mAddress->mPort))
        {
            udp_port = socket_info.mAddress->mPort;
        }
    }
    NFdbBase::FdbSessionInfo sinfo_sent;
    sinfo_sent.set_sender_name(mName.c_str());
    sinfo_sent.set_pid((uint32_t)CBaseThread::getPid());
    if (FDB_VALID_PORT(udp_port))
    {
        sinfo_sent.set_udp_port(udp_port);
    }
    CFdbParcelableBuilder builder(sinfo_sent);
    if (role() == FDB_OBJECT_ROLE_CLIENT)
    {
        sendSideband(FDB_SIDEBAND_SESSION_INFO, builder);
    }
    else
    {
        sendSideband(session->sid(), FDB_SIDEBAND_SESSION_INFO, builder);
    }
}

CFdbSession *CBaseEndpoint::connected(const CFdbSocketAddr &addr)
{
    auto &containers = getContainer();
    for (auto it = containers.begin(); it != containers.end(); ++it)
    {
        auto container = it->second;
        auto session = container->connected(addr);
        if (session)
        {
            return session;
        }
    }
    return 0;
}

CFdbSession *CBaseEndpoint::bound(const CFdbSocketAddr &addr)
{
    auto &containers = getContainer();
    for (auto it = containers.begin(); it != containers.end(); ++it)
    {
        auto container = it->second;
        auto session = container->bound(addr);
        if (session)
        {
            return session;
        }
    }
    return 0;
}

bool CBaseEndpoint::onMessageAuthentication(CFdbMessage *msg, CFdbSession *session)
{
    const CApiSecurityConfig *sec_cfg = getApiSecurityConfig();
    if (sec_cfg)
    {
        auto required_security_level = sec_cfg->getMessageSecLevel(msg->code());
        return session->securityLevel() >= required_security_level;
    }
    return true;
}
bool CBaseEndpoint::onEventAuthentication(CFdbMessage *msg, CFdbSession *session)
{
    const CApiSecurityConfig *sec_cfg = getApiSecurityConfig();
    if (sec_cfg)
    {
        auto required_security_level = sec_cfg->getEventSecLevel(msg->code());
        return session->securityLevel() >= required_security_level;
    }
    return true;
}

bool CBaseEndpoint::onMessageAuthentication(CFdbMessage *msg)
{
    const CApiSecurityConfig *sec_cfg = getApiSecurityConfig();
    if (sec_cfg && !msg->token().empty())
    {
        auto required_security_level = sec_cfg->getMessageSecLevel(msg->code());
        auto client_sec_level = checkSecurityLevel(msg->token().c_str());
        return client_sec_level >= required_security_level;
    }
    return true;
}
bool CBaseEndpoint::onEventAuthentication(CFdbMessage *msg)
{
    const CApiSecurityConfig *sec_cfg = getApiSecurityConfig();
    if (sec_cfg && !msg->token().empty())
    {
        auto required_security_level = sec_cfg->getEventSecLevel(msg->code());
        auto client_sec_level = checkSecurityLevel(msg->token().c_str());
        return client_sec_level >= required_security_level;
    }
    return true;
}

void CBaseEndpoint::onPublish(CBaseJob::Ptr &msg_ref)
{
    CFdbBaseObject::onPublish(msg_ref);
    mEventRouter.routeMessage(msg_ref);
}

void CBaseEndpoint::registerSession(CFdbSession *session)
{
    auto sid = mSessionContainer.allocateEntityId();
    session->sid(sid);
    mSessionContainer.insertEntry(sid, session);
}

CFdbSession *CBaseEndpoint::getSession(FdbSessionId_t session_id)
{
    CFdbSession *session = 0;
    mSessionContainer.retrieveEntry(session_id, session);
    return session;
}

void CBaseEndpoint::unregisterSession(FdbSessionId_t session_id)
{
    CFdbSession *session = 0;
    auto it = mSessionContainer.retrieveEntry(session_id, session);
    if (session)
    {
        mSessionContainer.deleteEntry(it);
    }
}

void CBaseEndpoint::deleteSession(FdbSessionId_t session_id)
{
    CFdbSession *session = 0;
    (void)mSessionContainer.retrieveEntry(session_id, session);
    if (session)
    {
        delete session;
    }
}

void CBaseEndpoint::deleteSession(CFdbSessionContainer *container)
{
    auto &session_tbl = mSessionContainer.getContainer();
    for (auto it = session_tbl.begin(); it != session_tbl.end();)
    {
        CFdbSession *session = it->second;
        ++it;
        if (session->container() == container)
        {
            delete session;
        }
    }
}

