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

#ifndef _CFDBCONTEXT_H_
#define _CFDBCONTEXT_H_

#include <vector>
#include <mutex>
#include "common_defs.h"
#include "CEntityContainer.h"
#include "CBaseWorker.h"
#include "CMethodJob.h"
#include "CFdbSessionContainer.h"

#define FDB_CONTEXT CFdbContext::getInstance()

class CBaseClient;
class CBaseServer;
class CFdbSession;
class CIntraNameProxy;
class CLogProducer;

class CFdbContext : public CBaseWorker
{
public:
    static CFdbContext *getInstance();
    bool start(uint32_t flag = FDB_WORKER_ENABLE_FD_LOOP);
    bool init();

    bool destroy();

    void findEndpoint(const char *name
                      , std::vector<CBaseEndpoint *> &skt_tbl
                      , bool is_server);

    CBaseEndpoint *getEndpoint(FdbEndpointId_t server_id);
    void registerSession(CFdbSession *session);
    CFdbSession *getSession(FdbSessionId_t session_id);
    void unregisterSession(FdbSessionId_t session_id);
    void deleteSession(FdbSessionId_t session_id);
    void deleteSession(CFdbSessionContainer *container);
    FdbEndpointId_t registerEndpoint(CBaseEndpoint *endpoint);
    void unregisterEndpoint(CBaseEndpoint *endpoint);
    CIntraNameProxy *getNameProxy();
    void reconnectOnNsConnected();
    void enableNameProxy(bool enable);
    void enableLogger(bool enable);
    CLogProducer *getLogger();

protected:
    bool asyncReady();
    
private:
    typedef CEntityContainer<FdbEndpointId_t, CBaseEndpoint *> tEndpointContainer;
    typedef CEntityContainer<FdbSessionId_t, CFdbSession *> tSessionContainer;

    tEndpointContainer mEndpointContainer;
    tSessionContainer mSessionContainer;
    CIntraNameProxy *mNameProxy;
    CLogProducer *mLogger;
    static std::mutex mSingletonLock;
    static CFdbContext *mInstance;

    bool mEnableNameProxy;
    bool mEnableLogger;

    CFdbContext();
    ~CFdbContext() {}
};

#endif
