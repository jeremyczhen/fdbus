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

#include <common_base/CFdbAFComponent.h>
#include <common_base/CMethodJob.h>
#include <common_base/CBaseClient.h>
#include <common_base/CBaseServer.h>
#include <common_base/CFdbContext.h>

CFdbAFComponent::CFdbAFComponent(const char *name, CBaseWorker *worker, CFdbBaseContext *context)
    : mName(name ? name : "Unknown-Component")
    , mWorker(worker)
{
    mContext = context ? context : FDB_CONTEXT;
}

CBaseClient *CFdbAFComponent::findClient(const char *bus_name)
{
    if (!bus_name)
    {
        return 0;
    }
    auto it = mClientTbl.find(bus_name);
    if (it == mClientTbl.end())
    {
        return 0;
    }
    return it->second;
}

bool CFdbAFComponent::registerClient(const char *bus_name, CBaseClient *client)
{
    std::string url(FDB_URL_SVC);
    url += bus_name;
    client->connect(url.c_str());
    mClientTbl[bus_name] = client;
    return true;
}

CBaseServer *CFdbAFComponent::findService(const char *bus_name)
{
    if (!bus_name)
    {
        return 0;
    }
    auto it = mServerTbl.find(bus_name);
    if (it == mServerTbl.end())
    {
        return 0;
    }
    return it->second;
}
bool CFdbAFComponent::registerService(const char *bus_name, CBaseServer *server)
{
    std::string url(FDB_URL_SVC);
    url += bus_name;
    server->bind(url.c_str());
    mServerTbl[bus_name] = server;
    return true;
}

class CRegisterEndpointJob : public CMethodJob<CFdbAFComponent>
{
public:
    CRegisterEndpointJob(CFdbAFComponent *appfw, const char *bus_name, const char *endpoint_name,
                         CFdbAFComponent::tOnCreateFn &on_create, bool is_server, CBaseEndpoint *&endpoint)
        : CMethodJob<CFdbAFComponent>(appfw, &CFdbAFComponent::callRegisterEndpoint, JOB_FORCE_RUN)
        , mBusName(bus_name)
        , mEndpointName(endpoint_name)
        , mOnCreate(on_create)
        , mIsServer(is_server)
        , mEndpoint(endpoint)
    {}
    const char *mBusName;
    const char *mEndpointName;
    CFdbAFComponent::tOnCreateFn &mOnCreate;
    bool mIsServer;
    CBaseEndpoint *&mEndpoint;
};

void CFdbAFComponent::callRegisterEndpoint(CBaseWorker *worker, CMethodJob<CFdbAFComponent> *job,
                                            CBaseJob::Ptr &ref)
{
    auto the_job = fdb_dynamic_cast_if_available<CRegisterEndpointJob *>(job);
    the_job->mEndpoint = 0;
    const char *endpoint_name = the_job->mEndpointName ? the_job->mEndpointName : mName.c_str();
    if (the_job->mIsServer)
    {
        auto server = findService(the_job->mBusName);
        if (!server)
        {
            server = new CBaseServer(endpoint_name);
            if (the_job->mOnCreate)
            {
                the_job->mOnCreate(server);
            }

            registerService(the_job->mBusName, server);
            the_job->mEndpoint = server;
        }
    }
    else
    {
        auto client = findClient(the_job->mBusName);
        if (!client)
        {
            client = new CBaseClient(endpoint_name);
            if (the_job->mOnCreate)
            {
                the_job->mOnCreate(client);
            }

            registerClient(the_job->mBusName, client);
            the_job->mEndpoint = client;
        }
    }
}

CBaseClient *CFdbAFComponent::registerClient(const char *bus_name, const char *endpoint_name,
                                              tOnCreateFn on_create)
{
    if (!bus_name)
    {
        return 0;
    }
    CBaseEndpoint *client = 0;
    auto job = new CRegisterEndpointJob(this, bus_name, endpoint_name, on_create, false, client);
    FDB_CONTEXT->sendSyncEndeavor(job);
    return fdb_dynamic_cast_if_available<CBaseClient *>(client);
}

CBaseServer *CFdbAFComponent::registerServer(const char *bus_name, const char *endpoint_name,
                                              tOnCreateFn on_create)
{
    if (!bus_name)
    {
        return 0;
    }
    CBaseEndpoint *server = 0;
    auto job = new CRegisterEndpointJob(this, bus_name, endpoint_name, on_create, true, server);
    FDB_CONTEXT->sendSyncEndeavor(job);
    return fdb_dynamic_cast_if_available<CBaseServer *>(server);
}

class CQueryServiceJob : public CMethodJob<CFdbAFComponent>
{
public:
    CQueryServiceJob(CFdbAFComponent *comp,
                     const char *bus_name,
                     const CFdbEventDispatcher::CEvtHandleTbl &evt_tbl,
                     CFdbBaseObject::tConnCallbackFn &connect_callback,
                     CBaseClient *&client)
        : CMethodJob<CFdbAFComponent>(comp, &CFdbAFComponent::callQueryService, JOB_FORCE_RUN)
        , mBusName(bus_name)
        , mEvtTbl(evt_tbl)
        , mConnCallback(connect_callback)
        , mClient(client)
    {
    }
    const char *mBusName;
    const CFdbEventDispatcher::CEvtHandleTbl &mEvtTbl;
    CFdbBaseObject::tConnCallbackFn &mConnCallback;
    CBaseClient *&mClient;
};

CBaseClient *CFdbAFComponent::createClient()
{
    return new CBaseClient(mName.c_str(), mWorker, mContext);
}

void CFdbAFComponent::callQueryService(CBaseWorker *worker, CMethodJob<CFdbAFComponent> *job, CBaseJob::Ptr &ref)
{
    auto the_job = fdb_dynamic_cast_if_available<CQueryServiceJob *>(job);
    auto client = findClient(the_job->mBusName);
    if (!client)
    {
        client = createClient();
        if (!client)
        {
            the_job->mClient = 0;
            return;
        }
        registerClient(the_job->mBusName, client);
    }
    auto handle = client->registerConnNotification(the_job->mConnCallback, mWorker);
    mConnHandleTbl.push_back(handle);
    client->registerEventHandle(the_job->mEvtTbl, &mEventRegistryTbl);
    the_job->mClient = client;
}

CBaseClient *CFdbAFComponent::queryService(const char *bus_name,
                                            const CFdbEventDispatcher::CEvtHandleTbl &evt_tbl,
                                            CFdbBaseObject::tConnCallbackFn connect_callback)
{
    if (!bus_name)
    {
        return 0;
    }
    CBaseClient *client = 0;
    auto job = new CQueryServiceJob(this, bus_name, evt_tbl, connect_callback, client);
    mContext->sendSyncEndeavor(job);
    return client;
}

bool CFdbAFComponent::addEvtHandle(CFdbEventDispatcher::CEvtHandleTbl &hdl_table, FdbMsgCode_t code,
                                   tDispatcherCallbackFn callback, const char *topic)
{
    return hdl_table.add(code, callback, mWorker, topic);
}

bool CFdbAFComponent::invoke(CFdbBaseObject *obj
                             , FdbMsgCode_t code
                             , IFdbMsgBuilder &data
                             , CFdbBaseObject::tInvokeCallbackFn callback
                             , int32_t timeout)
{
    return obj->invoke(code, data, callback, mWorker, timeout);
}

bool CFdbAFComponent::invoke(CFdbBaseObject *obj
                             , FdbMsgCode_t code
                             , CFdbBaseObject::tInvokeCallbackFn callback
                             , const void *buffer
                             , int32_t size
                             , int32_t timeout
                             , const char *log_info)
{
    return obj->invoke(code, callback, buffer, size, mWorker, timeout, log_info);
}

class COfferServiceJob : public CMethodJob<CFdbAFComponent>
{
public:
    COfferServiceJob(CFdbAFComponent *comp,
                     const char *bus_name,
                     const CFdbMsgDispatcher::CMsgHandleTbl &msg_tbl,
                     CFdbBaseObject::tConnCallbackFn &connect_callback,
                     CBaseServer *&server)
        : CMethodJob<CFdbAFComponent>(comp, &CFdbAFComponent::callOfferService, JOB_FORCE_RUN)
        , mBusName(bus_name)
        , mMsgTbl(msg_tbl)
        , mConnCallback(connect_callback)
        , mServer(server)
    {
    }
    const char *mBusName;
    const CFdbMsgDispatcher::CMsgHandleTbl &mMsgTbl;
    CFdbBaseObject::tConnCallbackFn &mConnCallback;
    CBaseServer *&mServer;
};

CBaseServer *CFdbAFComponent::createServer()
{
    return new CBaseServer(mName.c_str(), mWorker, mContext);
}

void CFdbAFComponent::callOfferService(CBaseWorker *worker, CMethodJob<CFdbAFComponent> *job, CBaseJob::Ptr &ref)
{
    auto the_job = fdb_dynamic_cast_if_available<COfferServiceJob *>(job);
    auto server = findService(the_job->mBusName);
    if (!server)
    {
        server = createServer();
        if (!server)
        {
            the_job->mServer = 0;
            return;
        }
        server->enableEventCache(true);
        registerService(the_job->mBusName, server);
    }
    auto handle = server->registerConnNotification(the_job->mConnCallback, mWorker);
    mConnHandleTbl.push_back(handle);
    server->registerMsgHandle(the_job->mMsgTbl);
    the_job->mServer = server;
}

CBaseServer *CFdbAFComponent::offerService(const char *bus_name,
                                            const CFdbMsgDispatcher::CMsgHandleTbl &msg_tbl,
                                            CFdbBaseObject::tConnCallbackFn connect_callback)
{
    if (!bus_name)
    {
        return 0;
    }
    CBaseServer *server = 0;
    auto job = new COfferServiceJob(this, bus_name, msg_tbl, connect_callback, server);
    mContext->sendSyncEndeavor(job);
    return server;
}

bool CFdbAFComponent::addMsgHandle(CFdbMsgDispatcher::CMsgHandleTbl &hdl_table, FdbMsgCode_t code,
                                   tDispatcherCallbackFn callback)
{
    return hdl_table.add(code, callback, mWorker);
}

