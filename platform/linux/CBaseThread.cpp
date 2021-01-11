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

#include <errno.h>
#include <stdlib.h>
#include <common_base/CBaseThread.h>
#include <cstring>

// Min/max thread priority range for QNX
static const int32_t MIN_THREAD_PRIORITY = 8;
static const int32_t MAX_THREAD_PRIORITY = 22;

CBaseThread::CBaseThread(const char* thread_name)
    : mPriority(10)
    , mThreadName(thread_name ? thread_name : "")
{
    int32_t schedPolicy;
    struct sched_param schedParam;
    if (!pthread_getschedparam(pthread_self(), &schedPolicy, &schedParam))
    {
        mPriority = schedParam.sched_priority;
    }
    mInvalidTid = pthread_self();
    mThread = mInvalidTid;
}

CBaseThread::~CBaseThread()
{
    // join();
}


bool CBaseThread::start(uint32_t flag)
{
    if (started())
    {
        return false;
    }

    if (flag & FDB_WORKER_EXE_IN_PLACE)
    {
        {
            std::lock_guard<std::mutex> _l(mMutex);
            if (started())
            {
                return false;
            }
            mThread = pthread_self();
        }
        threadFunc(this);
        return true;
    }
    else
    {
        std::lock_guard<std::mutex> _l(mMutex);
        if (started())
        {
            return true;
        }
        bool ret = false;
        pthread_attr_t threadAttr;
        if (!pthread_attr_init(&threadAttr))
        {
            if (!pthread_attr_setdetachstate(&threadAttr, PTHREAD_CREATE_JOINABLE))
            {
                if (!pthread_create(&mThread, &threadAttr, threadFunc, this))
                {
                    applyPriority(mPriority);
                    name(mThreadName.c_str());
                    ret = true;
                }
            }
            // We no longer need the attribute structure
            (void)pthread_attr_destroy(&threadAttr);
        }
        return ret;
    }
}

bool CBaseThread::started()
{
    return !pthread_equal(mInvalidTid, mThread);
}

bool CBaseThread::join(uint32_t milliseconds)
{
    if (isSelf())
    {
        return false;
    }
    return !pthread_join(mThread, 0);
}

bool CBaseThread::isSelf() const
{
    return !!pthread_equal(mThread, pthread_self());
}

bool CBaseThread::priority(int32_t  level)
{
    bool status(false);

    if ( (level >= MIN_THREAD_PRIORITY) &&
            (level <= MAX_THREAD_PRIORITY) )
    {
        mPriority = level;
        status = applyPriority(mPriority);
    }

    return status;
}

bool CBaseThread::applyPriority(int32_t level)
{
    bool status(false);

    int32_t schedPolicy;
    struct sched_param schedParam;
    if (!pthread_getschedparam( mThread, &schedPolicy, &schedParam))
    {
        schedParam.sched_priority = level;
        if (!pthread_setschedparam(mThread, schedPolicy, &schedParam))
        {
            status = true;
        }
    }

    return status;
}

bool CBaseThread::name(const char* thread_name)
{
    if (thread_name == 0 || strlen(thread_name) == 0){
        return false;
    }

    if (mThread != mInvalidTid){
        size_t len = strlen(thread_name);

        const char* t_name = len <= 15 ? thread_name :
                                         thread_name+len - 15;
        // prctl(PR_SET_NAME, t_name);// set call thread name
        if(pthread_setname_np(mThread, t_name)){
            return false;
        }
        mThreadName = t_name;
        return true;   
    }else{
        return false;
    }
}

void *CBaseThread::threadFunc(void *d)
{
    CBaseThread *self = static_cast<CBaseThread *>(d);

    self->run();
    return 0;
}

CBASE_tProcId CBaseThread::getPid()
{
    return getpid();
}
