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

#ifndef _CBASETHREAD_HPP_
#define _CBASETHREAD_HPP_

#include "CBaseSysDep.h"
#include <string>
#include <mutex>

/*
 *  * Default starting flag: create thread; doesn't supoort watch
 *   */
#define FDB_WORKER_DEFAULT            0
/*
 *  * If set, no worker thread will be created; the main loop is running at
 *   * the thread calling start()
 *    */
#define FDB_WORKER_EXE_IN_PLACE       (1 << 0)

#define FDB_BASE_WORKER_FLAG_SHIFT    1

class CBaseThread
{
public:
    CBaseThread(const char* thread_name = "");
    virtual ~CBaseThread();

    virtual bool start(uint32_t flag = FDB_WORKER_DEFAULT);
    bool started();
    bool join(uint32_t milliseconds = 0);
    bool isSelf() const;
    bool priority(int32_t level);
    int32_t priority() const
    {
        return mPriority;
    }
    bool name(const char* thread_name);
    const std::string &name() const
    {
        return mThreadName;
    }
    static CBASE_tProcId getPid(); 

protected:
    bool applyPriority(int32_t level);
    /*-----------------------------------------------------------------------------
     * The virtual function should be implemented by subclass
     *---------------------------------------------------------------------------*/
    /*
     * run main loop of the thread
     */
    virtual void run()
    {}

    CBASE_tThreadHnd mThread;
    std::mutex mMutex;

private:
    int32_t mPriority;
    std::string mThreadName;

#ifdef __WIN32__
    static unsigned int __stdcall threadFunc(void *d);
    DWORD mThreadId;
    DWORD mInvalidTid;
#else
    CBASE_tThreadHnd mInvalidTid;
    static void *threadFunc(void *d);
#endif
};

#endif /* Guard for THREAD_H_ */
