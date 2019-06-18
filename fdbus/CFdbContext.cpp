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

#include <common_base/CFdbContext.h>
#include <common_base/CFdbSession.h>
#include <common_base/CIntraNameProxy.h>
#include <common_base/CLogProducer.h>
#include <utils/Log.h>

// template<> FdbSessionId_t CFdbContext::tSessionContainer::mUniqueEntryAllocator = 0;

CFdbContext *CFdbContext::mInstance = 0;

CFdbContext::CFdbContext()
    : CBaseWorker("CFdbContext")
    , mNameProxy(0)
    , mLogger(0)
    , mEnableNameProxy(true)
    , mEnableLogger(true)
{

}

bool CFdbContext::start(uint32_t flag)
{
    return CBaseWorker::start(FDB_WORKER_ENABLE_FD_LOOP | flag);
}

bool CFdbContext::init()
{
    return CBaseWorker::init(FDB_WORKER_ENABLE_FD_LOOP);
}

bool CFdbContext::asyncReady()
{
    if (mEnableNameProxy)
    {
        mNameProxy = new CIntraNameProxy();
        mNameProxy->connectToNameServer();
    }
    if (mEnableLogger)
    {
        mLogger = new CLogProducer();
        std::string svc_url;
        mLogger->getDefaultSvcUrl(svc_url);
        mLogger->doConnect(svc_url.c_str());
    }
    return true;
}

bool CFdbContext::destroy()
{
    if (mNameProxy)
    {
        mNameProxy->disconnect();
        delete mNameProxy;
        mNameProxy = 0;
    }
    if (mLogger)
    {
        mLogger->disconnect();
        delete mLogger;
        mLogger = 0;
    }

    if (!mEndpointContainer.getContainer().empty())
    {
        LOG_E("CFdbContext: Unable to destroy context since there are active endpoint!\n");
        return false;
    }
    if (!mSessionContainer.getContainer().empty())
    {
        LOG_E("CFdbContext: Unable to destroy context since there are active sessions!\n");
        return false;
    }
    exit();
    join();
    delete this;
    mInstance = 0;
    return true;
}

CBaseEndpoint *CFdbContext::getEndpoint(FdbEndpointId_t endpoint_id)
{
    CBaseEndpoint *endpoint = 0;
    mEndpointContainer.retrieveEntry(endpoint_id, endpoint);
    return endpoint;
}

void CFdbContext::registerSession(CFdbSession *session)
{
    FdbSessionId_t sid = mSessionContainer.allocateEntityId();
    session->sid(sid);
    mSessionContainer.insertEntry(sid, session);
}

CFdbSession *CFdbContext::getSession(FdbSessionId_t session_id)
{
    CFdbSession *session = 0;
    mSessionContainer.retrieveEntry(session_id, session);
    return session;
}

void CFdbContext::unregisterSession(FdbSessionId_t session_id)
{
    CFdbSession *session = 0;
    tSessionContainer::EntryContainer_t::iterator it =
        mSessionContainer.retrieveEntry(session_id, session);
    if (session)
    {
        mSessionContainer.deleteEntry(it);
    }
}

void CFdbContext::deleteSession(FdbSessionId_t session_id)
{
    CFdbSession *session = 0;
    (void)mSessionContainer.retrieveEntry(session_id, session);
    if (session)
    {
        delete session;
    }
}

void CFdbContext::deleteSession(CFdbSessionContainer *container)
{
    tSessionContainer::EntryContainer_t &session_tbl = mSessionContainer.getContainer();
    tSessionContainer::EntryContainer_t::iterator it;
    for (it = session_tbl.begin(); it != session_tbl.end();)
    {
        CFdbSession *session = it->second;
        ++it;
        if (session->container() == container)
        {
            delete session;
        }
    }
}

FdbEndpointId_t CFdbContext::registerEndpoint(CBaseEndpoint *endpoint)
{
    FdbEndpointId_t id = endpoint->epid();
    if (!isValidFdbId(id))
    {
        id = mEndpointContainer.allocateEntityId();
        endpoint->epid(id);
        mEndpointContainer.insertEntry(id, endpoint);
        endpoint->enableMigrate(true);
    }
    return id;
}

void CFdbContext::unregisterEndpoint(CBaseEndpoint *endpoint)
{
    CBaseEndpoint *self = 0;
    tEndpointContainer::EntryContainer_t::iterator it =
        mEndpointContainer.retrieveEntry(endpoint->epid(), self);
    if (self)
    {
        endpoint->enableMigrate(false);
        endpoint->epid(FDB_INVALID_ID);
        mEndpointContainer.deleteEntry(it);
    }
}

void CFdbContext::findEndpoint(const char *name
                               , std::vector<CBaseEndpoint *> &ep_tbl
                               , bool is_server)
{
    tEndpointContainer::EntryContainer_t &container = mEndpointContainer.getContainer();
    tEndpointContainer::EntryContainer_t::iterator it;
    for (it = container.begin(); it != container.end(); ++it)
    {
        CBaseEndpoint *endpoint = it->second;
        bool found = false;

        if (is_server)
        {
            if (endpoint->role() == FDB_OBJECT_ROLE_SERVER)
            {
                found = true;
            }
        }
        else if (endpoint->role() == FDB_OBJECT_ROLE_CLIENT)
        {
            found = true;
        }

        if (!found)
        {
            continue;
        }

        if (!endpoint->nsName().compare(name))
        {
            ep_tbl.push_back(endpoint);
        }
    }
}

CIntraNameProxy *CFdbContext::getNameProxy()
{
    return (mNameProxy && mNameProxy->connected()) ? mNameProxy : 0;
}

void CFdbContext::reconnectOnNsConnected(bool connect)
{
    tEndpointContainer::EntryContainer_t &container = mEndpointContainer.getContainer();
    tEndpointContainer::EntryContainer_t::iterator it;
    for (it = container.begin(); it != container.end(); ++it)
    {
        CBaseEndpoint *endpoint = it->second;
        endpoint->reconnectToNs(connect);
    }
}

void CFdbContext::enableNameProxy(bool enable)
{
    mEnableNameProxy = enable;
}

void CFdbContext::enableLogger(bool enable)
{
    mEnableLogger = enable;
}

CLogProducer *CFdbContext::getLogger()
{
    return mLogger;
}

