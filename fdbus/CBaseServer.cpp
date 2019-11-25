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
#include <common_base/CIntraNameProxy.h>
#include <idl-gen/common.base.MessageHeader.pb.h>
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
    CSocketImp *sock_imp = mSocket->accept();
    if (sock_imp)
    {
        CFdbSession *session = new CFdbSession(FDB_INVALID_ID, this, sock_imp);
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
    if (mSocket->bind())
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
    CBindServerJob *the_job = dynamic_cast<CBindServerJob *>(job);
    if (!the_job)
    {
        return;
    }

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
    CServerSocket *sk = doBind(url);
    if (sk)
    {
        the_job->mSkId = sk->skid();
    }
    else
    {
        the_job->mSkId = FDB_INVALID_ID;
    }
}

bool CBaseServer::requestServiceAddress(const char *server_name)
{
    if (mNsConnStatus == CONNECTED)
    {
        return true;
    }

    if (mNsName.empty() && !server_name)
    {
        LOG_E("CBaseServer: Service name is not given!\n");
        return false;
    }
    if (server_name)
    {
        mNsName = server_name;
    }

    if (role() == FDB_OBJECT_ROLE_NS_SERVER)
    {
        LOG_E("CBaseServer: name server cannot bind to svc://service_name!\n");
        return false;
    }

    if (mNsConnStatus == LOST)
    {
        doUnbind();
    }
    
    mNsConnStatus = CONNECTING;
    CIntraNameProxy *name_proxy = FDB_CONTEXT->getNameProxy();
    if (!name_proxy)
    {
        return false;
    }

#if 0
    if (mNsConnStatus == LOST)
    {
        std::vector<std::string> url_list;
        getUrlList(url_list);
        name_proxy->registerService(mNsName.c_str(), url_list);
    }
    else
    {
        name_proxy->registerService(mNsName.c_str());
    }
#else
    name_proxy->registerService(mNsName.c_str());
#endif
    name_proxy->addAddressListener(mNsName.c_str());
    return true;
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

    CFdbSessionContainer *session_container = getSocketByUrl(url);
    if (session_container) /* If the address is already bound, do nothing */
    {
        return dynamic_cast<CServerSocket *>(session_container);
    }

    CServerSocketImp *server_imp = CBaseSocketFactory::createServerSocket(addr);
    if (server_imp)
    {
        FdbSocketId_t skid = allocateEntityId();
        CServerSocket *sk = new CServerSocket(this, skid, server_imp);
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
    CUnbindServerJob *the_job = dynamic_cast<CUnbindServerJob *>(job);
    if (!the_job)
    {
        return;
    }

    doUnbind(the_job->mSkId);
    if (!isValidFdbId(the_job->mSkId))
    {
        unregisterSelf();
        // From now on, there will be no jobs migrated to worker thread. Applying a
        // flush to worker thread to ensure no one refers to the object.
    }
}

void CBaseServer::doUnbind(FdbSocketId_t skid)
{
    deleteSocket(skid);
    if (!isValidFdbId(skid))
    {
        mNsConnStatus = UNCONNECTED;
    }
}

void CBaseServer::unbind(FdbSocketId_t skid)
{
    CFdbContext::getInstance()->sendSyncEndeavor(
            new CUnbindServerJob(this, &CBaseServer::cbUnbind, skid), 0, true);
}

void CBaseServer::reconnectToNs(bool connect)
{
    if (mNsConnStatus == DISCONNECTED)
    {
        return;
    }
    if (connect)
    {
        requestServiceAddress(0);
    }
    else
    {
        mNsConnStatus = LOST;
    }
}

void CBaseServer::onSidebandInvoke(CBaseJob::Ptr &msg_ref)
{
    CFdbMessage *msg = castToMessage<CFdbMessage *>(msg_ref);
    switch (msg->code())
    {
        case FDB_SIDEBAND_AUTH:
        {
            NFdbBase::FdbAuthentication authen;
            if (!msg->deserialize(authen))
            {
                msg->status(msg_ref, NFdbBase::FDB_ST_MSG_DECODE_FAIL);
                return;
            }
            
            CFdbSession *session = FDB_CONTEXT->getSession(msg->session());
            if (!session)
            {
                return;
            }
            int32_t security_level = FDB_SECURITY_LEVEL_NONE;
            const char *token = "";
            if (authen.has_token_list() && !authen.token_list().tokens().empty())
            {
                const ::google::protobuf::RepeatedPtrField< ::std::string> &tokens =
                    authen.token_list().tokens();
                // only use the first token in case more than 1 tokens are received
                token = tokens.begin()->c_str();
                security_level = checkSecurityLevel(token);
            }
            // update security level and token
            session->securityLevel(security_level);
            session->token(token);
            LOG_I("CBaseServer: security is set: session: %d, level: %d.\n",
                    msg->session(), security_level);
        }
        break;
        default:
        break;
    }
}

bool CBaseServer::onMessageAuthentication(CFdbMessage *msg, CFdbSession *session)
{
    int32_t security_level = mApiSecurity.getMessageSecLevel(msg->code());
    return session->securityLevel() >= security_level;
}

bool CBaseServer::onEventAuthentication(CFdbMessage *msg, CFdbSession *session)
{
    int32_t security_level = mApiSecurity.getEventSecLevel(msg->code());
    return session->securityLevel() >= security_level;
}

