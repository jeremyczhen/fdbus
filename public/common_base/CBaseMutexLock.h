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

#ifndef _CBASEMUTEXLOCK_HPP_
#define _CBASEMUTEXLOCK_HPP_

#include "CBaseSysDep.h"

class CBaseMutexLock
{
public:
    CBaseMutexLock();
    CBaseMutexLock(bool do_init);
    virtual ~CBaseMutexLock();
    bool init();

    bool lock();
    bool unlock();
private:
    bool mInitialized;
    CBASE_tMutexHnd   mMutex;
};

class CAutoLock {
public:
    inline explicit CAutoLock(CBaseMutexLock& mutex) : mLock(mutex)  { mLock.lock(); }
    inline explicit CAutoLock(CBaseMutexLock* mutex) : mLock(*mutex) { mLock.lock(); }
    inline ~CAutoLock() { mLock.unlock(); }
private:
    CBaseMutexLock& mLock;
};

#endif /* Guard for MUTEXLOCK_HPP_ */
