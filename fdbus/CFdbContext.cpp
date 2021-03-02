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
#include "CIntraNameProxy.h"
#include <common_base/CLogProducer.h>
#include <utils/Log.h>

std::mutex CFdbContext::mSingletonLock;
CFdbContext *CFdbContext::mInstance = 0;

#define FDB_DEFAULT_LOG_CACHE_SIZE  8 * 1024 * 1024

CFdbContext::CFdbContext()
    : CFdbBaseContext("FDBusDefaultContext")
    , mNameProxy(0)
    , mLogger(0)
    , mEnableNameProxy(true)
    , mEnableLogger(true)
    , mEnableLogCache(true)
    , mLogCacheSize(0)
{

}

CFdbContext *CFdbContext::getInstance()
{
    if (!mInstance)
    {
        std::lock_guard<std::mutex> _l(mSingletonLock);
        if (!mInstance)
        {
            mInstance = new CFdbContext();
        }
    }
    return mInstance;
}

const char *CFdbContext::getFdbLibVersion()
{
    return FDB_DEF_TO_STR(FDB_VERSION_MAJOR) "." FDB_DEF_TO_STR(FDB_VERSION_MINOR) "." FDB_DEF_TO_STR(FDB_VERSION_BUILD);
}

bool CFdbContext::asyncReady()
{
    if (mEnableNameProxy)
    {
        auto name_proxy = new CIntraNameProxy();
        name_proxy->connectToNameServer();
        mNameProxy = name_proxy;
    }
    if (mEnableLogger)
    {
        int32_t cache_size = 0;
        if (mEnableLogCache)
        {
            cache_size = mLogCacheSize ? mLogCacheSize : FDB_DEFAULT_LOG_CACHE_SIZE;
        }
        auto logger = new CLogProducer(cache_size);
        std::string svc_url;
        logger->getDefaultSvcUrl(svc_url);
        logger->doConnect(svc_url.c_str());
        mLogger = logger;
    }
    return true;
}

bool CFdbContext::destroy()
{
    if (mNameProxy)
    {
        auto name_proxy = mNameProxy;
        mNameProxy = 0;
        name_proxy->enableNsMonitor(false);
        name_proxy->prepareDestroy();
        delete name_proxy;
    }
    if (mLogger)
    {
        auto logger = mLogger;
        mLogger = 0;
        logger->prepareDestroy();
        delete logger;
    }

    exit();
    join();
    delete this;
    return true;
}

CIntraNameProxy *CFdbContext::getNameProxy()
{
    return (mNameProxy && mNameProxy->connected()) ? mNameProxy : 0;
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

void CFdbContext::registerNsWatchdogListener(tNsWatchdogListenerFn watchdog_listener)
{
    if (mNameProxy)
    {
        mNameProxy->registerNsWatchdogListener(watchdog_listener);
    }
}

class CRegisterContextJob : public CMethodJob<CFdbContext>
{
public:
    CRegisterContextJob(CFdbContext *object, CFdbBaseContext *context, bool do_register)
        : CMethodJob<CFdbContext>(object, &CFdbContext::callRegisterContext, JOB_FORCE_RUN)
        , mContext(context)
        , mDoRegister(do_register)
    {}
    CFdbBaseContext *mContext;
    bool mDoRegister;
};

void CFdbContext::callRegisterContext(CBaseWorker *worker,
            CMethodJob<CFdbContext> *job, CBaseJob::Ptr &ref)
{
    auto the_job = fdb_dynamic_cast_if_available<CRegisterContextJob *>(job);
    if (the_job->mDoRegister)
    {
        doRegisterContext(the_job->mContext);
    }
    else
    {
        doUnregisterContext(the_job->mContext);
    }
}

void CFdbContext::doRegisterContext(CFdbBaseContext *context)
{
    auto id = mContextContainer.allocateEntityId();
    context->ctxId(id);
    mContextContainer.insertEntry(id, context);
}

void CFdbContext::doUnregisterContext(CFdbBaseContext *context)
{
    mContextContainer.deleteEntry(context->ctxId());
}

void CFdbContext::registerContext(CFdbBaseContext *context)
{
    sendSyncEndeavor(new CRegisterContextJob(this, context, true));
}

void CFdbContext::unregisterContext(CFdbBaseContext *context)
{
    sendSyncEndeavor(new CRegisterContextJob(this, context, false));
}

