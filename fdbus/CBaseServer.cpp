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

#include <common_base/CBaseServer.h>
#include <common_base/CFdbContext.h>
#include <common_base/CFdbSession.h>
#include <common_base/CBaseSocketFactory.h>
#include <common_base/CFdbIfMessageHeader.h>
#include <common_base/CFdbIfNameServer.h>
#include <utils/Log.h>

CServerSocket::CServerSocket(CBaseServer *owner
                             , FdbSocketId_t skid
                             , CServerSocketImp *socket)
    : CBaseFdWatch(-1, POLLIN)
    , CFdbSessionContainer(skid, owner)
    , mSocket(socket)
{
}

CServerSocket::~CServerSocket()
{
    unbind();
}

void CServerSocket::onInput(bool &io_error)
{
    bool blocking_mode = (mSocket->getAddress().mType == FDB_SOCKET_IPC) ?
                          mOwner->enableIpcBlockingMode() : mOwner->enableTcpBlockingMode();
    auto sock_imp = mSocket->accept(blocking_mode);
    if (sock_imp)
    {
        auto session = new CFdbSession(FDB_INVALID_ID, this, sock_imp);
        CFdbContext::getInstance()->registerSession(session);
        session->attach(worker());
        if (!mOwner->addConnectedSession(this, session))
        {
            delete session;
        }
    }
}

bool CServerSocket::bind(CBaseWorker *worker)
{
    int32_t retries = FDB_ADDRESS_BIND_RETRY_NR;
    do
    {
        if (mSocket->bind())
        {
            break;
        }

        sysdep_sleep(FDB_ADDRESS_BIND_RETRY_INTERVAL);
    } while (--retries > 0);

    if (retries > 0)
    {
        descriptor(mSocket->getFd());
        attach(worker);
        return true;
    }
    return false;
}

void CServerSocket::unbind()
{
    if (mSocket)
    {
        delete mSocket;
        mSocket = 0;
    }
}

void CServerSocket::getSocketInfo(CFdbSocketInfo &info)
{
    info.mAddress = &mSocket->getAddress();
}

CBaseServer::CBaseServer(const char *name, CBaseWorker *worker)
    : CBaseEndpoint(name, worker, FDB_OBJECT_ROLE_SERVER)
{
    enableTcpBlockingMode(true);
    enableIpcBlockingMode(false);
}

CBaseServer::~CBaseServer()
{
}

class CBindServerJob : public CMethodJob<CBaseServer>
{
public:
    CBindServerJob(CBaseServer *server, M method, FdbSocketId_t &skid, const char *url)
        : CMethodJob<CBaseServer>(server, method, JOB_FORCE_RUN)
        , mSkId(skid)
    {
        if (url)
        {
            mUrl = url;
        }
    }
    FdbSocketId_t &mSkId;
    std::string mUrl;
};
FdbSocketId_t CBaseServer::bind(const char *url)
{
    FdbSocketId_t skid = FDB_INVALID_ID;
    CFdbContext::getInstance()->sendSyncEndeavor(
        new CBindServerJob(this, &CBaseServer::cbBind, skid, url), 0, true);
    return skid;
}

void CBaseServer::cbBind(CBaseWorker *worker, CMethodJob<CBaseServer> *job, CBaseJob::Ptr &ref)
{
    auto the_job = fdb_dynamic_cast_if_available<CBindServerJob *>(job);
    if (!the_job)
    {
        return;
    }
    enableMigrate(true);

    std::string svc_url;
    const char *url;
    if (the_job->mUrl.empty())
    {
        getDefaultSvcUrl(svc_url);
        url = svc_url.c_str();
    }
    else
    {
        url = the_job->mUrl.c_str();
    }
    auto sk = doBind(url);
    if (sk)
    {
        the_job->mSkId = sk->skid();
    }
    else
    {
        the_job->mSkId = FDB_INVALID_ID;
    }
}


CServerSocket *CBaseServer::doBind(const char *url)
{
    CFdbSocketAddr addr;
    EFdbSocketType skt_type;
    const char *server_name;

    if (url)
    {
        if (!CBaseSocketFactory::parseUrl(url, addr))
        {
            return 0;
        }
        skt_type = addr.mType;
        server_name = addr.mAddr.c_str();
    }
    else
    {
        skt_type = FDB_SOCKET_SVC;
        server_name = 0;
    }
    

    if (skt_type == FDB_SOCKET_SVC)
    {
        mApiSecurity.importSecLevel(server_name);
        requestServiceAddress(server_name);
        return 0;
    }

    auto session_container = getSocketByUrl(url);
    if (session_container) /* If the address is already bound, do nothing */
    {
        return fdb_dynamic_cast_if_available<CServerSocket *>(session_container);
    }

    auto server_imp = CBaseSocketFactory::createServerSocket(addr);
    if (server_imp)
    {
        FdbSocketId_t skid = allocateEntityId();
        auto sk = new CServerSocket(this, skid, server_imp);
        addSocket(sk);
        if (sk->bind(CFdbContext::getInstance()))
        {
            return sk;
        }
        else
        {
            delete sk;
        }
    }
    return 0;
}

class CUnbindServerJob : public CMethodJob<CBaseServer>
{
public:
    CUnbindServerJob(CBaseServer *server, M method, FdbSocketId_t &skid)
        : CMethodJob<CBaseServer>(server, method, JOB_FORCE_RUN)
        , mSkId(skid)
    {}
    FdbSocketId_t mSkId;
};

void CBaseServer::cbUnbind(CBaseWorker *worker, CMethodJob<CBaseServer> *job, CBaseJob::Ptr &ref)
{
    auto the_job = fdb_dynamic_cast_if_available<CUnbindServerJob *>(job);
    if (!the_job)
    {
        return;
    }

    doUnbind(the_job->mSkId);
    if (!fdbValidFdbId(the_job->mSkId))
    {
        releaseServiceAddress();
        enableMigrate(false);
        // From now on, there will be no jobs migrated to worker thread. Applying a
        // flush to worker thread to ensure no one refers to the object.
    }
}

void CBaseServer::doUnbind(FdbSocketId_t skid)
{
    deleteSocket(skid);
}

void CBaseServer::unbind(FdbSocketId_t skid)
{
    CFdbContext::getInstance()->sendSyncEndeavor(
            new CUnbindServerJob(this, &CBaseServer::cbUnbind, skid), 0, true);
}

void CBaseServer::onSidebandInvoke(CBaseJob::Ptr &msg_ref)
{
    auto msg = castToMessage<CFdbMessage *>(msg_ref);
    auto session = FDB_CONTEXT->getSession(msg->session());
    if (!session)
    {
        return;
    }
    switch (msg->code())
    {
        case FDB_SIDEBAND_AUTH:
        {
            NFdbBase::FdbAuthentication authen;
            CFdbParcelableParser parser(authen);
            if (!msg->deserialize(parser))
            {
                msg->status(msg_ref, NFdbBase::FDB_ST_MSG_DECODE_FAIL);
                return;
            }
            
            int32_t security_level = FDB_SECURITY_LEVEL_NONE;
            auto token = "";
            if (authen.has_token_list() && !authen.token_list().tokens().empty())
            {
                const auto &tokens = authen.token_list().tokens();
                // only use the first token in case more than 1 tokens are received
                token = tokens.pool().begin()->c_str();
                security_level = checkSecurityLevel(token);
            }
            // update security level and token
            session->securityLevel(security_level);
            session->token(token);
            LOG_I("CBaseServer: security is set: session: %d, level: %d.\n",
                    msg->session(), security_level);
        }
        break;
        case FDB_SIDEBAND_SESSION_INFO:
        {
            NFdbBase::FdbSessionInfo sinfo;
            CFdbParcelableParser parser(sinfo);
            if (!msg->deserialize(parser))
            {
                msg->status(msg_ref, NFdbBase::FDB_ST_MSG_DECODE_FAIL);
                return;
            }
            session->senderName(sinfo.sender_name().c_str());
        }
        break;
        case FDB_SIDEBAND_QUERY_CLIENT:
        {
            NFdbBase::FdbMsgClientTable clt_tbl;
            clt_tbl.set_endpoint_name(name().c_str());
            clt_tbl.set_server_name(nsName().c_str());
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
                        CFdbSessionInfo sinfo;
                        session->getSessionInfo(sinfo);
                        auto cinfo = clt_tbl.add_client_tbl();
                        cinfo->set_peer_name(session->senderName().c_str());
                        std::string addr;
                        if (sinfo.mSocketInfo.mAddress->mType == FDB_SOCKET_IPC)
                        {
                            addr = sinfo.mSocketInfo.mAddress->mAddr.c_str();
                        }
                        else
                        {
                            addr = sinfo.mConn->mPeerIp + ":" +
                                   std::to_string(sinfo.mConn->mPeerPort);
                        }
                        cinfo->set_peer_address(addr.c_str());
                        cinfo->set_security_level(session->securityLevel());
                    }
                }
            }
            CFdbParcelableBuilder builder(clt_tbl);
            msg->replySideband(msg_ref, builder);
        }
        break;
        default:
            CBaseEndpoint::onSidebandInvoke(msg_ref);
        break;
    }
}

bool CBaseServer::onMessageAuthentication(CFdbMessage *msg, CFdbSession *session)
{
    auto security_level = mApiSecurity.getMessageSecLevel(msg->code());
    return session->securityLevel() >= security_level;
}

bool CBaseServer::onEventAuthentication(CFdbMessage *msg, CFdbSession *session)
{
    auto security_level = mApiSecurity.getEventSecLevel(msg->code());
    return session->securityLevel() >= security_level;
}

bool CBaseServer::publishNoQueue(FdbMsgCode_t code, const char *topic, const void *buffer,
                                 int32_t size, CFdbSession *session)
{
    CBaseMessage msg(code, this);
    if (topic)
    {
        msg.topic(topic);
    }
    msg.expectReply(false);
    if (!msg.serialize(buffer, size, this))
    {
        return false;
    }

    return session->sendMessage(&msg);
}

void CBaseServer::publishCachedEvents(CFdbSession *session)
{
    for (auto it_events = mEventCache.begin(); it_events != mEventCache.end(); ++it_events)
    {
        auto event_code = it_events->first;
        auto &events = it_events->second;
        for (auto it_data = events.begin(); it_data != events.end(); ++it_data)
        {
            auto &filter = it_data->first;
            auto &data = it_data->second;
            publishNoQueue(event_code, filter.c_str(), data.mBuffer, data.mSize, session);
        }
    }
}

void CBaseServer::prepareDestroy()
{
    unbind();
    CBaseEndpoint::prepareDestroy();
}

