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

#include <common_base/CFdbBaseContext.h>
#include <common_base/CFdbContext.h>
#include <common_base/CBaseEndpoint.h>
#include <common_base/CFdbSession.h>
#include <utils/Log.h>
#include <iostream>

CFdbBaseContext::CFdbBaseContext(const char *worker_name)
    : CBaseWorker(worker_name)
    , mCtxId(FDB_INVALID_ID)
{

}

bool CFdbBaseContext::start(uint32_t flag)
{
    return CBaseWorker::start(FDB_WORKER_ENABLE_FD_LOOP | flag);
}

bool CFdbBaseContext::tearup()
{
    FDB_CONTEXT->registerContext(this);
    return true;
}

void CFdbBaseContext::teardown()
{
    FDB_CONTEXT->unregisterContext(this);
}

bool CFdbBaseContext::init()
{
    return CBaseWorker::init(FDB_WORKER_ENABLE_FD_LOOP);
}

CFdbBaseContext::~CFdbBaseContext()
{
    if (!mEndpointContainer.getContainer().empty())
    {
        std::cout << "CFdbBaseContext: Unable to destroy context since there are active endpoint!" << std::endl;
    }
}

CBaseEndpoint *CFdbBaseContext::getEndpoint(FdbEndpointId_t endpoint_id)
{
    CBaseEndpoint *endpoint = 0;
    mEndpointContainer.retrieveEntry(endpoint_id, endpoint);
    return endpoint;
}

FdbEndpointId_t CFdbBaseContext::registerEndpoint(CBaseEndpoint *endpoint)
{
    auto id = endpoint->epid();
    if (mEndpointContainer.findEntry(endpoint))
    {
        return id;
    }
    if (fdbValidFdbId(id))
    {
        CBaseEndpoint *registered_endpoint = 0;
        mEndpointContainer.retrieveEntry(id, registered_endpoint);
        if (!registered_endpoint)
        {
            return FDB_INVALID_ID;
        }
    }
    else
    {
        id = mEndpointContainer.allocateEntityId();
        endpoint->epid(id);
        mEndpointContainer.insertEntry(id, endpoint);
        endpoint->enableMigrate(true);
    }
    return id;
}

void CFdbBaseContext::unregisterEndpoint(CBaseEndpoint *endpoint)
{
    CBaseEndpoint *self = 0;
    auto it = mEndpointContainer.retrieveEntry(endpoint->epid(), self);
    if (self)
    {
        endpoint->enableMigrate(false);
        endpoint->epid(FDB_INVALID_ID);
        mEndpointContainer.deleteEntry(it);
    }
}

bool CFdbBaseContext::serverAlreadyRegistered(const char *bus_name)
{
    if (!bus_name || (bus_name[0] == '\0'))
    {
        return false;
    }
        
    auto &container = mEndpointContainer.getContainer();
    for (auto it = container.begin(); it != container.end(); ++it)
    {
        if ((it->second->role() == FDB_OBJECT_ROLE_SERVER) &&
            !it->second->nsName().compare(bus_name))
        {
            return true;
        }
    }

    return false;
}

bool CFdbBaseContext::serverAlreadyRegistered(CBaseEndpoint *endpoint)
{
    auto &container = mEndpointContainer.getContainer();
    for (auto it = container.begin(); it != container.end(); ++it)
    {
        if ((it->second->role() == FDB_OBJECT_ROLE_SERVER) &&
            !it->second->nsName().compare(endpoint->nsName()) &&
            (it->second != endpoint))
        {
            return true;
        }
    }

    return false;
}

