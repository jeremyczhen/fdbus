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

#include <stdlib.h>
#include <common_base/CFdbAppFramework.h>
#include <common_base/CBaseClient.h>
#include <common_base/CBaseServer.h>
#include <common_base/CFdbContext.h>
#include <common_base/CFdbContext.h>

CFdbAPPFramework *CFdbAPPFramework::mInstance = 0;

CFdbAPPFramework::CFdbAPPFramework()
{
    FDB_CONTEXT->start();
    const char *app_name = getenv("FDB_CONFIG_APP_NAME");
    mName = app_name ? app_name : "anonymous-app";
}

CBaseClient *CFdbAPPFramework::findClient(const char *bus_name)
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

bool CFdbAPPFramework::registerClient(const char *bus_name, CBaseClient *client)
{
    std::string url(FDB_URL_SVC);
    url += bus_name;
    client->connect(url.c_str());
    mClientTbl[bus_name] = client;
    return true;
}

CBaseServer *CFdbAPPFramework::findService(const char *bus_name)
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
bool CFdbAPPFramework::registerService(const char *bus_name, CBaseServer *server)
{
    std::string url(FDB_URL_SVC);
    url += bus_name;
    server->bind(url.c_str());
    mServerTbl[bus_name] = server;
    return true;
}

class CRegisterEndpointJob : public CMethodJob<CFdbAPPFramework>
{
public:
    CRegisterEndpointJob(CFdbAPPFramework *appfw, const char *bus_name, const char *endpoint_name,
                         CFdbAPPFramework::tOnCreateFn &on_create, bool is_server, CBaseEndpoint *&endpoint)
        : CMethodJob<CFdbAPPFramework>(appfw, &CFdbAPPFramework::callRegisterEndpoint, JOB_FORCE_RUN)
        , mBusName(bus_name)
        , mEndpointName(endpoint_name)
        , mOnCreate(on_create)
        , mIsServer(is_server)
        , mEndpoint(endpoint)
    {}
    const char *mBusName;
    const char *mEndpointName;
    CFdbAPPFramework::tOnCreateFn &mOnCreate;
    bool mIsServer;
    CBaseEndpoint *&mEndpoint;
};

void CFdbAPPFramework::callRegisterEndpoint(CBaseWorker *worker, CMethodJob<CFdbAPPFramework> *job,
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

CBaseClient *CFdbAPPFramework::registerClient(const char *bus_name, const char *endpoint_name,
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

CBaseServer *CFdbAPPFramework::registerServer(const char *bus_name, const char *endpoint_name,
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

