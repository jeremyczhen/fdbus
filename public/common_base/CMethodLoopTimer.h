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

#ifndef _CMETHODLOOPTIMER_H_
#define _CMETHODLOOPTIMER_H_

#include <cstdint>
#include "CBaseLoopTimer.h"

/*
 * The timer allows executing callback in the context of given object
 * rather than the context of the job itself.
 */
template <class C>
class CMethodLoopTimer : public CBaseLoopTimer
{
public:
    /*
     * This is the type you can use to define the subclass. For example:
     * class CMyTimer : public CMethodLoopTimer<CMyClass>
     * {
     * public:
     *     CMyTimer(CMyClass *class, CMethodLoopTimer<CMyClass>::M method, ...)
     * };
     *
     * @iparam timer: the timer the method is processing
     */
    typedef C classType;
    typedef void (C::*M)(CMethodLoopTimer *timer);

    /*
     * Create CMethodLoopTimer
     *
     * @iparam interval: interval of the timer
     * @iparam repeat: true - cyclic timer; false - one shot timer
     * @iparam instance: instance of object
     * @iparam method: method of the instance to run
     * @return  None
     */
    CMethodLoopTimer(int32_t interval, bool repeat, C *instance, M method)
        : CBaseLoopTimer(interval, repeat)
        , mInstance(instance)
        , mMethod(method)
    {
    }

private:
    void run()
    {
        if (mInstance && mMethod)
        {
            (mInstance->*mMethod)(this);
        }
    }
    C *mInstance;
    M mMethod;
};

#endif
