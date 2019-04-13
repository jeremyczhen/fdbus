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

#ifndef _CBASESEMAPHORE_HPP_
#define _CBASESEMAPHORE_HPP_

#include "CBaseSysDep.h"

class CBaseSemaphore
{
public:
    CBaseSemaphore(uint32_t initialCount);
    CBaseSemaphore();
    ~CBaseSemaphore();

    bool init(uint32_t initialCount);
    bool post();
    bool wait(int32_t milliseconds = 0);

private:
    bool mInitialized;
    CBASE_tSemHnd mSem;
};

#endif /* Guard for SEMAPHORE_HPP_ */
