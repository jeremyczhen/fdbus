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
    CFdbContext::getInstance()->sendSyncEndeavor(new CBindServerJob(this, &CBaseServer::cbBind, skid, url), 0, true);
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
    the_job->mSkId = doBind(url);
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


FdbSocketId_t CBaseServer::doBind(const char *url)
{
    CFdbSocketAddr addr;
    EFdbSocketType skt_type;
    const char *server_name;

    if (url)
    {
        if (!CBaseSocketFactory::getInstance()->parseUrl(url, addr))
        {
            return FDB_INVALID_ID;
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
        requestServiceAddress(server_name);
        return FDB_INVALID_ID;
    }

    CFdbSessionContainer *session_container = getSocketByUrl(url);
    if (session_container) /* If the address is already bound, do nothing */
    {
        return session_container->skid();
    }

    CServerSocketImp *server_imp = CBaseSocketFactory::getInstance()->createServerSocket(addr);
    if (server_imp)
    {
        FdbSocketId_t skid = allocateEntityId();
        CServerSocket *sk = new CServerSocket(this, skid, server_imp);
        addSocket(sk);
        if (sk->bind(CFdbContext::getInstance()))
        {
            return skid;
        }
        else
        {
            delete sk;
        }
    }
    return FDB_INVALID_ID;
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
    CFdbContext::getInstance()->sendSyncEndeavor(new CUnbindServerJob(this, &CBaseServer::cbUnbind, skid), 0, true);
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

