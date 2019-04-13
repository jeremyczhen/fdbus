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

#ifndef _CBASELOOPTIMER_H_
#define _CBASELOOPTIMER_H_

#include "CSysLoopTimer.h"

class CBaseWorker;
class CBaseLoopTimer : public CSysLoopTimer
{
public:
    /*
     * Create a Loop timer
     *
     * @iparam interval: interval of the timer in ms
     * @iparam repeat: true - the timer is cyclic; false - one shot
     * @return None
     */
    CBaseLoopTimer(int32_t interval, bool repeat = false);

    ~CBaseLoopTimer();

    /*
     * Enable the timer: the timer is running with the context of
     * worker thread
     * Note:
     * 1. Once you call this function when timer is running, expiration time
     * will be reset to "now + interval".
     * 2. cyclic/one-shot property of the timer is not changed.
     * @iparam interval: optionally set interval of the timer; unchanged
     *     if set to -1;
     *
     * @return true: success
     * @note attach() should be called before calling this function
     */
    bool enable(int32_t interval = -1);

    /*
     * Disable the timer: the timer is still on the worker but doesn't work;
     * You can enable it with enable(), enableRepeat() or enableOneShot()
     *
     * @return true: success
     * @note attach() should be called before calling this function
     */
    bool disable();

    /*
     * Set timer to be cyclic. Timer will be enabled if already disabled
     * Note:
     * Once you call this function when timer is running, expiration time
     * will be reset to "now + interval".
     *
     * @iparam interval: optionally set interval of the timer; unchanged
     *     if set to -1;
     * @iparam init_value: optionally set initial value of timer; if <= 0:
     *     the same as "interval".
     * @return true: success
     * @note attach() should be called before calling this function
     */
    bool enableRepeat(int32_t interval = -1, int32_t init_value = -1);

    /*
     * Set timer to be one-shot. Timer will be enabled if already disabled
     * Note:
     * Once you call this function when timer is running, expiration time
     * will be reset to "now + interval".
     * @iparam interval: optionally set interval of the timer; unchanged
     *     if set to -1;
     *
     * @return true: success
     * @note attach() should be called before calling this function
     */
    bool enableOneShot(int32_t interval = -1);

    /*
     * Run the timer at specified worker. Once an timer is attached with
     * a worker, callback of the timer will run in context of thread of
     * the worker
     *
     * @iparam worker: the worker where the timer is running.
     * @iparam enb: initially enable/disable the timer
     * @return true: success
     * @note attach() should be called before calling this function
     */
    bool attach(CBaseWorker *worker, bool enb = true);

    /*
     * Obtain worker the timer is attached
     *
     * @return the worker
     */
    CBaseWorker *worker()
    {
        return mWorker;
    }
private:
    friend class CBaseWorker;
    CBaseWorker *mWorker;
};
#endif
