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

/*-----------------------------------------------------------------------------
 * INCLUDES AND NAMESPACE
 *---------------------------------------------------------------------------*/
#include <common_base/CBaseWorker.h>
#include <common_base/CBaseLoopTimer.h>
#include <common_base/CBaseFdWatch.h>
#include <utils/Log.h>
#include <common_base/CFdEventLoop.h>
#include <common_base/CThreadEventLoop.h>

/*-----------------------------------------------------------------------------
 * CLASS IMPLEMENTATIONS
 *---------------------------------------------------------------------------*/
CBaseJob::CBaseJob(uint32_t flag)
    : mFlag(flag)
    , mSyncLock(false)
    , mSyncReq(0)
{
}

CBaseJob::~CBaseJob()
{
}

void CBaseJob::wakeup(Ptr &ref)
{
    // Why +1? because the job must be referred locally.
    // Warning: ref count of the job can not be changed
    // during the sync waiting!!!
    if (ref.use_count() == (mSyncReq->mInitSharedCnt + 1))
    {
        mSyncReq->mSem.post();
    }
}

void CBaseJob::terminate(Ptr &ref)
{
    if (mSyncReq)
    {
        CAutoLock _l(mSyncLock);
        if (mSyncReq)
        {
            wakeup(ref);
        }
    }
}

CBaseLoopTimer::CBaseLoopTimer(int32_t interval, bool repeat)
    : CSysLoopTimer(interval, repeat)
    , mWorker(0)
{

}

CBaseLoopTimer::~CBaseLoopTimer()
{
    attach(0);
}

class CConfigTimerJob : public CBaseJob
{
public:
    CConfigTimerJob(CSysLoopTimer *timer, CSysLoopTimer::EEnable enb,
                    CSysLoopTimer::EEnable rpt, int32_t interval, int32_t init_value)
        : CBaseJob(JOB_FORCE_RUN)
        , mTimer(timer)
        , mEnable(enb)
        , mRepeat(rpt)
        , mInterval(interval)
        , mInitValue(init_value)
    {}
    void run(CBaseWorker *worker, Ptr &ref)
    {
        mTimer->config(mEnable, mRepeat, mInterval, mInitValue);
    }
private:
    CSysLoopTimer *mTimer;
    CSysLoopTimer::EEnable mEnable;
    CSysLoopTimer::EEnable mRepeat;
    int32_t mInterval;
    int32_t mInitValue;
};

bool CBaseLoopTimer::enable(int32_t interval)
{
    if (mWorker)
    {
        if (mWorker->isSelf())
        {
            CSysLoopTimer::config(ENABLE, DONT_CARE, interval, -1);
        }
        else
        {
            mWorker->sendAsync(new CConfigTimerJob(this, CSysLoopTimer::ENABLE,
                                                   CSysLoopTimer::DONT_CARE, interval, -1));

        }
    }
    else
    {
        LOG_E("CBaseWorker: Timer is not attached with a worker!\n");
        return false;
    }
    return true;
}

bool CBaseLoopTimer::disable()
{
    if (mWorker)
    {
        if (mWorker->isSelf())
        {
            CSysLoopTimer::config(DISABLE, DONT_CARE, -1, -1);
        }
        else
        {
            mWorker->sendAsync(new CConfigTimerJob(this,
                                                   CSysLoopTimer::DISABLE,
                                                   CSysLoopTimer::DONT_CARE,
                                                   -1, -1));
        }
    }
    else
    {
        LOG_E("CBaseWorker: Timer is not attached with a worker!\n");
        return false;
    }
    return true;
}

bool CBaseLoopTimer::enableRepeat(int32_t interval, int32_t init_value)
{
    if (mWorker)
    {
        if (mWorker->isSelf())
        {
            CSysLoopTimer::config(ENABLE, ENABLE, interval, init_value);
        }
        else
        {
            mWorker->sendAsync(new CConfigTimerJob(this,
                                                   CSysLoopTimer::ENABLE,
                                                   CSysLoopTimer::ENABLE,
                                                   interval, init_value));
        }
    }
    else
    {
        LOG_E("CBaseWorker: Timer is not attached with a worker!\n");
        return false;
    }
    return true;
}

bool CBaseLoopTimer::enableOneShot(int32_t interval)
{
    if (mWorker)
    {
        if (mWorker->isSelf())
        {
            CSysLoopTimer::config(ENABLE, DISABLE, interval, -1);
        }
        else
        {
            mWorker->sendAsync(new CConfigTimerJob(this,
                                                   CSysLoopTimer::ENABLE,
                                                   CSysLoopTimer::DISABLE,
                                                   interval, -1));
        }
    }
    else
    {
        LOG_E("CBaseWorker: Timer is not attached with a worker!\n");
        return false;
    }
    return true;
}

class CAttachTimerJob : public CBaseJob
{
public:
    CAttachTimerJob(CSysLoopTimer *timer, bool attach, bool enb)
        : CBaseJob(JOB_FORCE_RUN)
        , mTimer(timer)
        , mAttach(attach)
        , mEnable(enb)
    {
    };
    void run(CBaseWorker *worker, Ptr &ref)
    {
        if (mAttach)
        {
            worker->getLoop()->addTimer(mTimer, mEnable);
        }
        else
        {
            worker->getLoop()->removeTimer(mTimer);
        }
    }
private:
    CSysLoopTimer *mTimer;
    bool mAttach;
    bool mEnable;
};
bool CBaseLoopTimer::attach(CBaseWorker *worker, bool enb)
{
    if (mWorker)
    {
        if (mWorker == worker)
        {
            return true;
        }
        else
        {
            if (mWorker->isSelf())
            {
                mWorker->getLoop()->removeTimer(this);
            }
            else
            {
                if (!mWorker->sendSync(new CAttachTimerJob(this, false, enb), 0, true))
                {
                    return false;
                }
            }
            mWorker = 0;
        }
    }

    if (worker)
    {
        mWorker = worker;
        if (mWorker->isSelf())
        {
            mWorker->getLoop()->addTimer(this, enb);
        }
        else
        {
            if (!worker->sendSync(new CAttachTimerJob(this, true, enb), 0, true))
            {
                mWorker = 0;
                return false;
            }
        }
    }
    return true;
}

CBaseFdWatch::CBaseFdWatch(int fd, int32_t flags)
    : CSysFdWatch(fd, flags)
    , mWorker(0)
{

}

CBaseFdWatch::~CBaseFdWatch()
{
    attach(0);
}

class CEnableWatchJob : public CBaseJob
{
public:
    CEnableWatchJob(CSysFdWatch *watch, bool enable)
        : CBaseJob(JOB_FORCE_RUN)
        , mWatch(watch)
        , mEnable(enable)
    {}
    void run(CBaseWorker *worker, Ptr &ref)
    {
        mWatch->enable(mEnable);
    }
private:
    CSysFdWatch *mWatch;
    bool mEnable;
};
bool CBaseFdWatch::enable()
{
    if (mWorker)
    {
        if (mWorker->isSelf())
        {
            CSysFdWatch::enable(true);
        }
        else
        {
            mWorker->sendAsync(new CEnableWatchJob(this, true));
        }
    }
    else
    {
        LOG_E("CBaseWorker: Watch is not attached with a worker!\n");
        return false;
    }
    return true;
}

bool CBaseFdWatch::disable()
{
    if (mWorker)
    {
        if (mWorker->isSelf())
        {
            CSysFdWatch::enable(false);
        }
        else
        {
            mWorker->sendAsync(new CEnableWatchJob(this, false));
        }
    }
    else
    {
        LOG_E("CBaseWorker: Watch is not attached with a worker!\n");
        return false;
    }
    return true;
}

class CAttachWatchJob : public CBaseJob
{
public:
    CAttachWatchJob(CSysFdWatch *watch, bool attach, bool enb, CFdEventLoop *loop)
        : CBaseJob(JOB_FORCE_RUN)
        , mWatch(watch)
        , mAttach(attach)
        , mEnable(enb)
        , mLoop(loop)
    {
    };
    void run(CBaseWorker *worker, Ptr &ref)
    {
        if (mAttach)
        {
            mLoop->addWatch(mWatch, mEnable);
        }
        else
        {
            mLoop->removeWatch(mWatch);
        }
    }
private:
    CSysFdWatch *mWatch;
    bool mAttach;
    bool mEnable;
    CFdEventLoop *mLoop;
};
bool CBaseFdWatch::attach(CBaseWorker *worker, bool enb)
{
    if (mWorker)
    {
        if (mWorker == worker)
        {
            return true;
        }
        else
        {
            CFdEventLoop *loop = dynamic_cast<CFdEventLoop *>(mWorker->getLoop());
            if (!loop)
            {
                LOG_E("CBaseWorker: Trying to attach watch but fdloop is not enabled!\n");
                return false;
            }
            if (mWorker->isSelf())
            {
                loop->removeWatch(this);
            }
            else
            {
                if (!mWorker->sendSync(new CAttachWatchJob(this, false, enb, loop), 0, true))
                {
                    return false;
                }
            }
            mWorker = 0;
        }
    }

    if (worker)
    {
        mWorker = worker;
        CFdEventLoop *loop = dynamic_cast<CFdEventLoop *>(mWorker->getLoop());
        if (!loop)
        {
            LOG_E("CBaseWorker: Trying to attach watch but fdloop is not enabled!\n");
            mWorker = 0;
            return false;
        }

        if (mWorker->isSelf())
        {
            loop->addWatch(this, enb);
        }
        else
        {
            if (!mWorker->sendSync(new CAttachWatchJob(this, true, enb, loop), 0, true))
            {
                mWorker = 0;
                return false;
            }
        }
    }

    return true;
}

class CWatchFlagsJob : public CBaseJob
{
public:
    CWatchFlagsJob(CSysFdWatch *watch, int32_t flgs)
        : CBaseJob(JOB_FORCE_RUN)
        , mWatch(watch)
        , mFlags(flgs)
    {
    }
    void run(CBaseWorker *worker, Ptr &ref)
    {
        mWatch->flags(mFlags);
    }
private:
    CSysFdWatch *mWatch;
    int32_t mFlags;
};

bool CBaseFdWatch::flags(int32_t flgs)
{
    if (mWorker)
    {
        if (mWorker->isSelf())
        {
            CSysFdWatch::flags(flgs);
        }
        else
        {
            mWorker->sendAsync(new CWatchFlagsJob(this, flgs));
        }
    }
    else
    {
        LOG_E("CBaseWorker: Watch is not attached with a worker!\n");
        return false;
    }
    return true;
}

CBaseWorker::CJobQueue::CJobQueue(uint32_t max_size)
        : mMaxSize(max_size)
        , mCurrentSize(0)
        , mDiscardCnt(0)
{
}

bool CBaseWorker::CJobQueue::enqueue(CBaseJob::Ptr &job)
{
    bool ret = false;
    
    if (!mMaxSize || (mCurrentSize < mMaxSize))
    {
        CAutoLock _l(mMutex);
        if (!mMaxSize || (mCurrentSize < mMaxSize))
        {
            mJobQueue.push_back(job);
            mCurrentSize++;
            ret = true;
        }
    }
    
    return ret;
}

void CBaseWorker::CJobQueue::dumpJobs(tJobContainer &job_queue)
{
    if (!mJobQueue.empty())
    {
        CAutoLock _l(mMutex);
        job_queue = mJobQueue;
        mJobQueue.clear();
        mCurrentSize = 0;
    }
}

void CBaseWorker::CJobQueue::discardJobs()
{
    CAutoLock _l(mMutex);
    mDiscardCnt++;
}

void CBaseWorker::CJobQueue::pickupJobs()
{
    if (mDiscardCnt > 0)
    {
        CAutoLock _l(mMutex);
        if (mDiscardCnt > 0)
        {
            mDiscardCnt--;
        }
    }
}

bool CBaseWorker::CJobQueue::jobDiscarded()
{
    return mDiscardCnt > 0;
}

void CBaseWorker::CJobQueue::jobQueueSize(uint32_t size)
{
    mMaxSize = size;
}

uint32_t CBaseWorker::CJobQueue::jobQueueSize()
{
    return mMaxSize;
}

CBaseWorker::CBaseWorker(const char* thread_name, uint32_t normal_queue_size, uint32_t urgent_queue_size)
    : CBaseThread(thread_name)
    , mExitCode(0)
    , mEventLoop(0)
    , mNormalJobQueue(normal_queue_size)
    , mUrgentJobQueue(urgent_queue_size)
{
}

CBaseWorker::~CBaseWorker()
{
    if (mEventLoop)
    {
        delete mEventLoop;
    }
}

bool CBaseWorker::init(uint32_t flag)
{
    if (!mEventLoop)
    {
        if (flag & FDB_WORKER_ENABLE_FD_LOOP)
        {
            mEventLoop = new CFdEventLoop();
        }
        else
        {
            mEventLoop = new CThreadEventLoop();
        }
        if (mEventLoop->init(this))
        {
            return asyncReady();
        }
        else
        {
            LOG_E("CBaseWorker: fail to initialize event loop!\n");
            return false;
        }
    }
    return true;
}

bool CBaseWorker::start(uint32_t flag)
{
    if (started())
    {
        return false;
    }

    return init(flag) ? CBaseThread::start(flag) : false;
}

void CBaseWorker::run()
{
    try
    {
        if (!tearup())
        {
            return;
        }
    }
    catch (...)
    {
        LOG_E("CBaseWorker: exception caught in worker tearup!\n");
        return;
    }

    mExitCode = 0;
    while (!mExitCode)
    {
        try
        {
            mEventLoop->dispatch();
        }
        catch (...)
        {
            LOG_E("CBaseWorker: exception caught in worker main loop!\n");
        }
    }

    try
    {
        teardown();
    }
    catch (...)
    {
        LOG_E("CBaseWorker: exception caught in worker teardown!\n");
    }
}

void CBaseWorker::processNotifyWatch(bool &io_error)
{
    if (!mEventLoop->acknowledge())
    {
        LOG_E("CBaseWorker: Error reading from notify FD!\n");
        io_error = true;
    }
    else
    {
        processJobQueue();
    }
}

void CBaseWorker::runOneJob(tJobContainer::iterator &it, bool run_job)
{
    if ((*it)->sync())
    {
        if ((*it)->mSyncReq)
        {
            /*
             * Running with mSyncLock locked so that when when timer
             * expires at sendSync(), it waits until run() returns.
             * success() will be set to true to indicate that run() has
             * been executed successfully.
             */
            CAutoLock _l((*it)->mSyncLock);
            if ((*it)->mSyncReq)
            {
                if (run_job)
                {
                    (*it)->success(true);
                    (*it)->run(this, *it);
                }
                (*it)->wakeup(*it);
            }
        }
    }
    else
    {
        if (run_job)
        {
            (*it)->success(true);
            (*it)->run(this, *it);
        }
        (*it)->terminate(*it);
    }
}

void CBaseWorker::processUrgentJobQueue()
{
    tJobContainer jobs;

    mUrgentJobQueue.dumpJobs(jobs);
    for (tJobContainer::iterator it = jobs.begin(); it != jobs.end(); ++it)
    {
        (*it)->urgent(true);
        bool run_job = !mUrgentJobQueue.jobDiscarded() || (*it)->forceRun();
        runOneJob(it, run_job);
    }
}

void CBaseWorker::processJobQueue()
{
    tJobContainer jobs;

    mNormalJobQueue.dumpJobs(jobs);
    for (tJobContainer::iterator it = jobs.begin(); it != jobs.end(); ++it)
    {
        processUrgentJobQueue();
        (*it)->urgent(false);
        bool run_job = (!mNormalJobQueue.jobDiscarded() || (*it)->forceRun()) && !mExitCode;
        runOneJob(it, run_job);
    }

    processUrgentJobQueue();
}

bool CBaseWorker::send(CBaseJob::Ptr &job, bool urgent)
{
    bool ret;
    
    if (mExitCode)
    {
        return false;
    }

    if (urgent)
    {
        ret = mUrgentJobQueue.enqueue(job);
    }
    else
    {
        ret = mNormalJobQueue.enqueue(job);
    }

    return ret ? mEventLoop->notify() : false;
}

bool CBaseWorker::sendAsync(CBaseJob::Ptr &job, bool urgent)
{
    job->sync(false);
    return send(job, urgent);
}

bool CBaseWorker::sendAsyncEndeavor(CBaseJob::Ptr &job, bool urgent)
{
    if (isSelf())
    {
        // This is a shortcut and break the queue order. DO NOT invoke synchronously
        // in the same worker the job is intended to run!!!
        job->run(this, job);
        return true;
    }
    return sendAsync(job, urgent);
}

bool CBaseWorker::sendAsync(CBaseJob *job, bool urgent)
{
    CBaseJob::Ptr j(job);
    return sendAsync(j, urgent);
}

bool CBaseWorker::sendAsyncEndeavor(CBaseJob *job, bool urgent)
{
    CBaseJob::Ptr j(job);
    return sendAsyncEndeavor(j, urgent);
}

bool CBaseWorker::sendSync(CBaseJob::Ptr &job, int32_t milliseconds, bool urgent)
{
    if (job->mSyncReq)
    {
        return false;
    }

    // now we can assure the job is not in any worker queue
    CBaseJob::CSyncRequest sync_req(job.use_count());
    job->mSyncLock.init();
    job->sync(true);
    job->success(false);
    job->mSyncReq = &sync_req;
    if (!send(job, urgent))
    {
        return false;
    }

    if (!sync_req.mSem.wait(milliseconds))
    {
        // the job might still in worker queue
        CAutoLock _l(job->mSyncLock);
        job->mSyncReq = 0;
        return job->success();
    }

    // now we can assure the job is not in any worker queue
    job->mSyncReq = 0;
    return job->success();
}

bool CBaseWorker::sendSyncEndeavor(CBaseJob::Ptr &job, int32_t milliseconds, bool urgent)
{
    if (isSelf())
    {
        // This is a shortcut and break the queue order. DO NOT invoke synchronously
        // in the same worker the job is intended to run!!!
        job->success(true);
        job->run(this, job);
        return job->success();
    }
    return sendSync(job, milliseconds, urgent);

}

bool CBaseWorker::sendSync(CBaseJob *job, int32_t milliseconds, bool urgent)
{
    CBaseJob::Ptr j(job);
    return sendSync(j, milliseconds, urgent);
}

bool CBaseWorker::sendSyncEndeavor(CBaseJob *job, int32_t milliseconds, bool urgent)
{
    CBaseJob::Ptr j(job);
    return sendSyncEndeavor(j, milliseconds, urgent);
}

class CExitRequestJob : public CBaseJob
{
public:
    CExitRequestJob(int32_t exit_code)
        : CBaseJob(JOB_FORCE_RUN)
        , mExitCode(exit_code)
    {
    }
protected:
    virtual void run(CBaseWorker *worker, Ptr &ref)
    {
        worker->mExitCode = mExitCode;
    }
private:
    int32_t mExitCode;
};

void CBaseWorker::exit(int32_t exit_code)
{
    sendAsync(new CExitRequestJob(exit_code));
}

bool CBaseWorker::flush(int32_t milliseconds, bool urgent)
{
    if (isSelf())
    {
        return false;
    }

    return sendSync(new CBaseJob(), milliseconds, urgent);
}

void CBaseWorker::doExit(int32_t exit_code)
{
    mExitCode = exit_code;
}

void CBaseWorker::updateDiscardStatus(bool discard, bool urgent)
{
    CJobQueue &job_queue = urgent ? mUrgentJobQueue : mNormalJobQueue;
    
    if (discard)
    {
        job_queue.discardJobs();
    }
    else
    {
        job_queue.pickupJobs();
    }
}

class CUnlockJobQueueJob : public CBaseJob
{
public:
    CUnlockJobQueueJob()
        : CBaseJob(JOB_FORCE_RUN)
    {
    }
protected:
    void run(CBaseWorker *worker, Ptr &ref)
    {
        worker->updateDiscardStatus(false, urgent());
    }
};

void CBaseWorker::discardJobs(bool urgent)
{
    CBaseJob *job = new CUnlockJobQueueJob();
    
    updateDiscardStatus(true, urgent);
    if (!sendAsync(job, urgent))
    {
        updateDiscardStatus(false, urgent);
    }
}

void CBaseWorker::discardQueuedJobs(bool urgent)
{
    discardJobs(urgent);
}

