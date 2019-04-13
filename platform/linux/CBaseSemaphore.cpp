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

CBaseSemaphore::CBaseSemaphore(uint32_t initialCount)
    : mInitialized(false)
{
    init(initialCount);
}

CBaseSemaphore::CBaseSemaphore()
    : mInitialized(false)
{
}

CBaseSemaphore::~CBaseSemaphore()
{
    if (mInitialized)
    {
        (void)sem_destroy(&mSem);
    }
}

bool CBaseSemaphore::init(uint32_t initialCount)
{
    if (mInitialized)
    {
        return true;
    }
    mInitialized = true;
    
    return sem_init(&mSem, 0, initialCount) ? false : true;
}

bool CBaseSemaphore::post()
{
    if (!mInitialized)
    {
        return false;
    }

    return !sem_post(&mSem);
}


bool CBaseSemaphore::wait(int32_t milliseconds)
{
    if (!mInitialized)
    {
        return false;
    }

    bool ret = true;
    if (milliseconds > 0)
    {
        int s;
        struct timespec ts;
        if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
        {
            ret = false;
        }
        else
        {
            ts.tv_sec += milliseconds / 1000;
            ts.tv_nsec += (milliseconds % 1000) * 1000000;
            if (ts.tv_nsec >= 1000000000)
            {
                ts.tv_nsec -= 1000000000;
                ts.tv_sec++;
            }

            while ((s = sem_timedwait(&mSem, &ts)) == -1 && errno == EINTR)
            {
                continue;
            }
            if (s == -1)
            {
                if (errno == ETIMEDOUT)
                {
                    //TODO: process timeout
                }
                else
                {
                    //TODO: process error
                }
                ret = false;
            }
        }
    }
    else
    {
        ret = !sem_wait(&mSem);
    }
    return ret;
}

