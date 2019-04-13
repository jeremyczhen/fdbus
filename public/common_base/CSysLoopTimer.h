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

#ifndef _CSYSLOOPTIMER_H_
#define _CSYSLOOPTIMER_H_

#include <cstdint>

class CBaseEventLoop;
class CSysLoopTimer
{
public:
    enum EEnable
    {
        ENABLE,
        DISABLE,
        DONT_CARE
    };

    /*
     * Create a timer running at event loop
     *
     * @iparam interval: interval of the timer in ms
     * @iparam repeat: true - cyclic; false - one shot
     */
    CSysLoopTimer(int32_t interval, bool repeat);

    virtual ~CSysLoopTimer();

    /*
     * Get interval of timer
     */
    int32_t interval()
    {
        return mInterval;
    }

    /*
     * get enable status of the timer.
     */
    bool enable()
    {
        return mEnable;
    }

    /*
     * enable the timer. Internally used only!
     */
    void enable(bool enb, uint64_t time_start = 0, int32_t init_value = 0);

    /*
     * configure the timer. Internally used only!
     */
    void config(EEnable enb, EEnable rpt, int32_t interval, int32_t init_value);

    bool repeat()
    {
        return mRepeat;
    }
protected:
    /*-----------------------------------------------------------------------------
     * The virtual function should be implemented by subclass
     *---------------------------------------------------------------------------*/
    /*
     * The function will be called upon expiration of the timer
     */
    virtual void run() {}

private:
    void repeat(bool rep)
    {
        mRepeat = rep;
    }

    uint64_t expiration()
    {
        return mExpiration;
    }

    void eventloop(CBaseEventLoop *loop)
    {
        mEventLoop = loop;
    }

    CBaseEventLoop *eventloop()
    {
        return mEventLoop;
    }

    int32_t mInterval;
    bool mRepeat;
    bool mEnable;
    uint64_t mExpiration;
    CBaseEventLoop *mEventLoop;
    friend class CBaseEventLoop;
    friend class CFdEventLoop;
};

#endif
