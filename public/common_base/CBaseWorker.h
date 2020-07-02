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

#ifndef _CBASEWORKER_H_
#define _CBASEWORKER_H_

#include <vector>
#include "CBaseThread.h"
#include "CBaseJob.h"

/*
 * If set, the worker thread can run jobs, timers and watches;
 * Otherwise, the worker thread can run jobs, timeres; but watches are not
 * allowed
 */
#define FDB_WORKER_ENABLE_FD_LOOP   (1 << (FDB_BASE_WORKER_FLAG_SHIFT + 0))
#define FDB_WORKER_FLAG_SHIFT       (FDB_BASE_WORKER_FLAG_SHIFT + 1)

class CBaseEventLoop;
class CBaseWorker : public CBaseThread
{
public:
    CBaseWorker(const char* thread_name = "", uint32_t normal_queue_size = 0, uint32_t urgent_queue_size = 0);
    virtual ~CBaseWorker();

    /*
     * start work thread of the worker
     *
     * @iparam flag - can be none or or-ed by FDB_WORKER_EXE_IN_PLACE and
     *      FDB_WORKER_ENABLE_FD_LOOP
     * @return true - success; false - fail
     */
    bool start(uint32_t flag = FDB_WORKER_DEFAULT);

    /*
     * Initialize worker thread. It is used along with
     *      start(FDB_WORKER_EXE_IN_PLACE) to run event loop without
     *      creating new thread. In this case the event loop is running
     *      at the thread start() is called.
     * @iparam flag: can be none or FDB_WORKER_EXE_IN_PLACE
     * @return true - success; false - fail
     */
    bool init(uint32_t flag = FDB_WORKER_DEFAULT);

    /*
     * exit main loop of the work thread
     *
     * iparam exit_code: !0 - exit anyway
     */
    void exit(int32_t exit_code = 1);

    /*
     * send job to worker asynchronously; the job will be destroyed
     * by the worker automatically. To prevent this, call sendAsync(CBaseJob::Ptr &job)
     * version. The function is asynchronous, i.e., it returns immediately
     *
     * @iparam job: the job to sent
     * @iparam urgent: true - job is inserted at the tail of urgent queue;
     *                 false - job is inserted at tail of normal queue
     *         note that urgent queue has higher priority over normal queue
     * @return true - success; false - fail
     */
    bool sendAsync(CBaseJob *job, bool urgent = false);
    bool sendAsyncEndeavor(CBaseJob *job, bool urgent = false);

    /*
     * send job to worker asynchronously; the job will NOT be destroyed
     * by the worker automatically.
     *
     * @iparam job: the job to sent
     * @iparam urgent: true - job is inserted at the tail of urgent queue;
     *                 false - job is inserted at tail of normal queue
     *         note that urgent queue has higher priority over normal queue
     * @return true - success; false - fail
     */
    bool sendAsync(CBaseJob::Ptr &job, bool urgent = false);
    bool sendAsyncEndeavor(CBaseJob::Ptr &job, bool urgent = false);

    /*
     * send job to worker synchronously; the caller will be blocked
     * until the runnable is processed by the worker; the job will be deleted
     * by the worker automatically; To prevent this, call
     * sendSync(CBaseJob::Ptr &job, int32_t). If milliseconds is not 0, the call
     * will be blocked no longer than the specified time in millisecond.
     * The function is synchronous, i.e., it blocks until the job finishes
     *
     * @iparam job: the job to sent
     * @iparam milliseconds: timeout in milliseconds; 0: wait forever
     * @iparam urgent: true - job is inserted at the tail of urgent queue;
     *                 false - job is inserted at tail of normal queue
     *         note that urgent queue has higher priority over normal queue
     * @return true - success; false - fail (timeout or other reasons)
     */
    bool sendSync(CBaseJob *job, int32_t milliseconds = 0, bool urgent = false);
    bool sendSyncEndeavor(CBaseJob *job, int32_t milliseconds = 0, bool urgent = false);

    /*
     * send job to worker synchronously; the caller will be blocked
     * until the runnable is processed by the worker; the job will NOT be
     * deleted by the worker automatically. If milliseconds is not 0, the call
     * will be blocked no longer than the specified time in millisecond.
     * The function is synchronous, i.e., it blocks until the job finishes
     *
     * @iparam job: the job to sent
     * @iparam milliseconds: timeout in milliseconds; 0: wait forever
     * @iparam urgent: true - job is inserted at the tail of urgent queue;
     *                 false - job is inserted at tail of normal queue
     *         note that urgent queue has higher priority over normal queue
     *         during dispatch.
     * @return true - success; false - fail (timeout or other reasons)
     */
    bool sendSync(CBaseJob::Ptr &job, int32_t milliseconds = 0, bool urgent = false);
    bool sendSyncEndeavor(CBaseJob::Ptr &job, int32_t milliseconds = 0, bool urgent = false);

    /*
     * Return event loop of the worker. Don't call it for most of time.
     */
    CBaseEventLoop *getLoop() const
    {
        return mEventLoop;
    }

    /*
     * Send a job doing nothing to the work and wait until the job is
     *      processed by the worker. This is to make sure all jobs previously
     *      sent are all processed.
     */
    bool flush(int32_t milliseconds = 0, bool urgent = false);

    /*
     * Discard all jobs currently in normal/urgent job queue. Note that the job
     * current processing is not affected.
     */
    void discardQueuedJobs(bool urgent = false);

    /*
     * set max size of normal/urgent job queue. If jobs queued in normal job queue
     * have reached the specified size, jobs sent are dropped. The size of
     * 0 means unlimited.
     */
    void jobQueueSizeLimit(uint32_t size, bool urgent = false)
    {
        if (urgent)
        {
            mUrgentJobQueue.sizeLimit(size);
        }
        else
        {
            mNormalJobQueue.sizeLimit(size);
        }
    }
    uint32_t jobQueueSizeLimit(bool urgent = false) const
    {
        return urgent ? mUrgentJobQueue.sizeLimit() : mNormalJobQueue.sizeLimit();
    }

    uint32_t jobQueueSize(bool urgent = false) const
    {
        return urgent ? mUrgentJobQueue.size() : mNormalJobQueue.size();
    }

    void dispatchInput(int32_t timeout);

protected:
    /*
     * called after job queue is initialized but thread is not started. You can
     * send jobs asynchronously.
     */
    virtual bool asyncReady()
    {
        return true;
    }
    /*
     * called before entering main loop of worker thread. You can do preparation
     * that only do once, but should be done withing the context of the worker
     */
    virtual bool tearup()
    {
        return true;
    }

    /*
     * called after exiting main loop of worker thread. You can do clearup
     * that only do once, but should be done withing the context of the worker
     */
    virtual void teardown()
    {
    }

private:
    typedef std::vector< CBaseJob::Ptr > tJobContainer;
    class CJobQueue
    {
    public:
        CJobQueue(uint32_t max_size = 0);
        bool enqueue(CBaseJob::Ptr &job);
        void dumpJobs(tJobContainer &job_queue);
        void discardJobs();
        void pickupJobs();
        bool jobDiscarded();
        void sizeLimit(uint32_t size);
        uint32_t sizeLimit() const;
        uint32_t size() const;
        void eventLoop(CBaseEventLoop *event_loop)
        {
            mEventLoop = event_loop;
        }
        tJobContainer &jobQueue()
        {
            return mJobQueue;
        }
    private:
        uint32_t mMaxSize;
        int32_t mDiscardCnt;
        CBaseEventLoop *mEventLoop;
        tJobContainer mJobQueue;

        friend class CBaseWorker;
    };
    
    bool send(CBaseJob::Ptr &job, bool urgent);
    void run();
    void processJobQueue();
    void processUrgentJobs(tJobContainer &jobs);
    void processUrgentJobs();
    void doExit(int32_t exit_code = 1);
    void discardJobs(bool urgent);
    void updateDiscardStatus(bool discard, bool urgent);
    void runOneJob(tJobContainer::iterator &it, bool run_job);
    bool jobQueued() const;

    /*
     * exit code: 0 - don't exit
     *           !0 - exit
     */
    int32_t mExitCode;
    CBaseEventLoop *mEventLoop;

    CJobQueue mNormalJobQueue;
    CJobQueue mUrgentJobQueue;

    friend class CExitRequestJob;
    friend class CNotifyFdWatch;
    friend class CThreadEventLoop;
    friend class CUnlockJobQueueJob;
};

#endif
