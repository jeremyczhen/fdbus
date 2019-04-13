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

#include <common_base/CBaseSemaphore.h>
#include <common_base/CThreadEventLoop.h>
#include <common_base/CBaseWorker.h>

CThreadEventLoop::CThreadEventLoop()
    : mSemaphore(0)
    , mWorker(0)
{
}

CThreadEventLoop::~CThreadEventLoop()
{
    if (mSemaphore)
    {
        delete mSemaphore;
    }
}

void CThreadEventLoop::dispatch()
{
    int32_t wait_time = getMostRecentTime();
    if (wait_time == 0)
    {
        wait_time = 1;
    }
    mSemaphore->wait(wait_time);
    processTimers();
    bool io_error;
    mWorker->processNotifyWatch(io_error);
}

bool CThreadEventLoop::notify()
{
    return mSemaphore->post();
}

bool CThreadEventLoop::acknowledge()
{
    return true;
}

bool CThreadEventLoop::init(CBaseWorker *worker)
{
    if (!mSemaphore)
    {
        mSemaphore = new CBaseSemaphore(0);
        mWorker = worker;
    }
    return true;
}

