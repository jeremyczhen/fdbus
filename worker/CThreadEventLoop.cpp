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

#include <common_base/CThreadEventLoop.h>
#include <common_base/CBaseWorker.h>

CThreadEventLoop::CThreadEventLoop()
    : mWorker(0)
{
}

CThreadEventLoop::~CThreadEventLoop()
{
}

void CThreadEventLoop::dispatch()
{
    int32_t wait_time = getMostRecentTime();
    // initially mMutex is unlocked.
    if (wait_time < 0)
    {
        mMutex.lock();
        if (mWorker->jobQueued())
        {
            mWorker->processJobQueue(); // mutex will be unlocked
        }
        else
        {
            mWakeupSignal.wait(mMutex); // mutex will be locked 
            mWorker->processJobQueue(); // mutex will be unlocked
        }
    }
    else if (wait_time == 0)
    {
        processTimers();
    }
    {
        mMutex.lock();
        if (mWorker->jobQueued())
        {
            mWorker->processJobQueue(); // mutex will be unlocked 
        }
        else
        {
            std::cv_status status = mWakeupSignal.wait_for(mMutex,
                                        std::chrono::milliseconds(wait_time));
            if (status == std::cv_status::timeout)
            {
                mMutex.unlock();
                processTimers(); // timer should be processed unlocked
            }
            else
            {
                mWorker->processJobQueue(); // mutex will be unlocked
            }
        }
    }
}

bool CThreadEventLoop::notify()
{
    mWakeupSignal.notify_one();
    return true;
}

bool CThreadEventLoop::init(CBaseWorker *worker)
{
    mWorker = worker;
    return true;
}
