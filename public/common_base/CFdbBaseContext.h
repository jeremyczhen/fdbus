
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

#ifndef _CFDBBASECONTEXT_H_
#define _CFDBBASECONTEXT_H_

#include "common_defs.h"
#include "CEntityContainer.h"
#include "CBaseWorker.h"

class CBaseEndpoint;

class CFdbBaseContext : public CBaseWorker
{
public:
    typedef CEntityContainer<FdbEndpointId_t, CBaseEndpoint *> tEndpointContainer;

    CFdbBaseContext(const char *worker_name = "FDBusContext");
    virtual ~CFdbBaseContext();
    bool start(uint32_t flag = FDB_WORKER_ENABLE_FD_LOOP);
    bool init();

    CBaseEndpoint *getEndpoint(FdbEndpointId_t server_id);
    FdbEndpointId_t registerEndpoint(CBaseEndpoint *endpoint);
    void unregisterEndpoint(CBaseEndpoint *endpoint);
    bool serverAlreadyRegistered(const char *bus_name);
    bool serverAlreadyRegistered(CBaseEndpoint *endpoint);
    tEndpointContainer &getEndpoints()
    {
        return mEndpointContainer;
    }
    FdbContextId_t ctxId() const
    {
        return mCtxId;
    }
    void ctxId(FdbContextId_t id)
    {
        mCtxId = id;
    }

protected:
    FdbContextId_t mCtxId;
    bool tearup();
    void teardown();

private:
    tEndpointContainer mEndpointContainer;
};

#endif
