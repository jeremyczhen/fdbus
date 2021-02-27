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

#include <mutex>
#include <functional>
#include "common_defs.h"
#include "CFdbBaseContext.h"
#include "CMethodJob.h"
#include "CEntityContainer.h"

#define FDB_CONTEXT CFdbContext::getInstance()
#define FDB_DEF_TO_STR1(R) #R
#define FDB_DEF_TO_STR(R) FDB_DEF_TO_STR1(R)

class CIntraNameProxy;
class CLogProducer;
class CBaseWorker;

struct CNsWatchdogItem
{
    std::string mClientName;
    CBASE_tProcId mPid;
    CNsWatchdogItem(const char *client_name, CBASE_tProcId pid)
        : mClientName(client_name)
        , mPid(pid)
    {}
};
typedef std::vector<CNsWatchdogItem> tNsWatchdogList;
typedef std::function<void(const tNsWatchdogList &)> tNsWatchdogListenerFn;

class CFdbContext : public CFdbBaseContext 
{
public:
    typedef CEntityContainer<FdbContextId_t, CFdbBaseContext *> tContextContainer;

    static CFdbContext *getInstance();
    bool destroy();

    CIntraNameProxy *getNameProxy();
    void enableNameProxy(bool enable);
    void enableLogger(bool enable);
    CLogProducer *getLogger();
    void registerNsWatchdogListener(tNsWatchdogListenerFn watchdog_listener);
    static const char *getFdbLibVersion();
    void registerContext(CFdbBaseContext *context);
    void unregisterContext(CFdbBaseContext *context);

    tContextContainer &getContexts()
    {
        return mContextContainer;
    }
    void enableLogCache(bool active)
    {
        mEnableLogCache = active;
    }
    void setLogCacheSize(int32_t size)
    {
        mLogCacheSize = size;
    }

protected:
    bool asyncReady();
    
private:
    void doRegisterContext(CFdbBaseContext *context);
    void doUnregisterContext(CFdbBaseContext *context);
    void callRegisterContext(CBaseWorker *worker, CMethodJob<CFdbContext> *job, CBaseJob::Ptr &ref);

    CIntraNameProxy *mNameProxy;
    CLogProducer *mLogger;
    static std::mutex mSingletonLock;
    static CFdbContext *mInstance;

    bool mEnableNameProxy;
    bool mEnableLogger;
    tContextContainer mContextContainer;
    bool mEnableLogCache;
    int32_t mLogCacheSize;

    CFdbContext();
    friend class CRegisterContextJob;
};

#endif
