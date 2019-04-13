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

#include <common_base/CBaseMutexLock.h>

CBaseMutexLock::CBaseMutexLock()
    : mInitialized(false)
{
    init();
}

CBaseMutexLock::CBaseMutexLock(bool do_init)
    : mInitialized(false)
{
    if (do_init)
    {
        init();
    }
}

CBaseMutexLock::~CBaseMutexLock()
{
    if (mInitialized)
    {
        (void)CloseHandle(mMutex);
    }
}

bool CBaseMutexLock::init()
{
    if (mInitialized)
    {
        return true;
    }
    mInitialized = true;

    mMutex = CreateMutex(0, FALSE, 0);
    return mMutex ? true : false;
}

bool CBaseMutexLock::lock()
{
    if (!mInitialized)
    {
        return false;
    }

    DWORD waitResult = WaitForSingleObject(mMutex, INFINITE);
    return (WAIT_OBJECT_0 == waitResult) ? true : false;

    if ( WAIT_OBJECT_0 != waitResult )
    {
        //throw LockError(waitResult);
    }
}

bool CBaseMutexLock::unlock()
{
    if (!mInitialized)
    {
        return false;
    }

    return !!ReleaseMutex(mMutex);
}
