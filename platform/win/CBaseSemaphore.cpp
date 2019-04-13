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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <limits.h>
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
        (void)CloseHandle(mSem);
    }
}

bool CBaseSemaphore::init(uint32_t initialCount)
{
    if (mInitialized)
    {
        return true;
    }
    mInitialized = true;

    mSem = CreateSemaphore(0, initialCount, LONG_MAX, 0);
    return mSem ? true : false;
}

bool CBaseSemaphore::post()
{
    if (!mInitialized)
    {
        return false;
    }

    return !!ReleaseSemaphore(mSem, 1, 0);
}


bool CBaseSemaphore::wait(int32_t milliseconds)
{
    if (!mInitialized)
    {
        return false;
    }

    DWORD wait_time = (milliseconds <= 0) ? INFINITE :  milliseconds;
    DWORD waitResult = WaitForSingleObject(mSem, wait_time);
    return (WAIT_OBJECT_0 == waitResult) ? true : false;
}
