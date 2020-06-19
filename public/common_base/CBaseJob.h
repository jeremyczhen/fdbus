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

#ifndef _CBASEJOB_H_
#define _CBASEJOB_H_

#include <cstdint>
#include <memory>
#include <mutex>
#include "CBaseSemaphore.h"
#include "common_defs.h"

class CBaseWorker;
class CBaseJob
{
#define JOB_FORCE_RUN           (1 << 0)
#define JOB_IS_URGENT           (1 << 1)
#define JOB_IS_SYNC             (1 << 2)
#define JOB_RUN_SUCCESS         (1 << 3)
public:
    typedef std::shared_ptr<CBaseJob> Ptr;
    CBaseJob(uint32_t flag = 0);
    virtual ~CBaseJob();
    void terminate(Ptr &ref);
    void forceRun(bool force)
    {
        if (force)
        {
            mFlag |= JOB_FORCE_RUN;
        }
        else
        {
            mFlag &= ~JOB_FORCE_RUN;
        }
    }

    bool forceRun() const
    {
        return !!(mFlag & JOB_FORCE_RUN);
    }

    bool urgent() const
    {
        return !!(mFlag & JOB_IS_URGENT);
    }

    bool sync() const
    {
        return !!(mFlag & JOB_IS_SYNC);
    }

    bool success() const
    {
        return !!(mFlag & JOB_RUN_SUCCESS);
    }

protected:
    /*-----------------------------------------------------------------------------
     * The virtual function should be implemented by subclass
     *---------------------------------------------------------------------------*/
    /*
     * this is the function implementing task of the job
     *
     * @iparam worker: the worker(thread) where the job is running
     * @return None
     */
    virtual void run(CBaseWorker *worker, Ptr &ref) {}

private:
    struct CSyncRequest
    {
        CSyncRequest(long int init_shared_cnt)
            : mInitSharedCnt(init_shared_cnt)
            , mSem(0)
        {
        }

        long int mInitSharedCnt;
        CBaseSemaphore mSem;
    };

    void urgent(bool active)
    {
        if (active)
        {
            mFlag |= JOB_IS_URGENT;
        }
        else
        {
            mFlag &= ~JOB_IS_URGENT;
        }
    }

    void sync(bool active)
    {
        if (active)
        {
            mFlag |= JOB_IS_SYNC;
        }
        else
        {
            mFlag &= ~JOB_IS_SYNC;
        }
    }

    void success(bool active)
    {
        if (active)
        {
            mFlag |= JOB_RUN_SUCCESS;
        }
        else
        {
            mFlag &= ~JOB_RUN_SUCCESS;
        }
    }

    void wakeup(Ptr &ref);

    int32_t mFlag;
    std::mutex mSyncLock;
    CSyncRequest *mSyncReq;
    friend class CBaseWorker;
};

template <typename T>
T castToMessage(CBaseJob::Ptr &job_ref)
{
    return fdb_dynamic_cast_if_available<T>(job_ref.get());
}

#endif

