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

#include <vector>
#include <utils/Log.h>
#include <cstdint>
#include <common_base/CBaseSysDep.h>
#include <common_base/CBaseEventLoop.h>
#include <common_base/CSysLoopTimer.h>
#include <algorithm>
#include <common_base/common_defs.h>

/*-----------------------------------------------------------------------------
 * CLASS IMPLEMENTATIONS
 *---------------------------------------------------------------------------*/
CSysLoopTimer::CSysLoopTimer(int32_t interval, bool rpt)
    : mInterval(interval)
    , mRepeat(rpt)
    , mEnable(false)
    , mExpiration(0)
    , mEventLoop(0)
{
}

CSysLoopTimer::~CSysLoopTimer()
{
}

void CSysLoopTimer::enable(bool enb, uint64_t time_start, int32_t init_value)
{
    if (mEnable)
    {
        fdb_remove_value_from_container(mEventLoop->mTimerWorkingList, this);
        mEnable = false;
    }

    if (enb)
    {
        if (time_start <= 0)
        {
            time_start = sysdep_getsystemtime_milli();
        }
        if (init_value > 0)
        {
            mExpiration = time_start + init_value;
        }
        else if (mInterval > 0)
        {
            mExpiration = time_start + mInterval;
        }
        else
        {
            LOG_E("CBaseEventLoop: Unable to enable timer since interval is invalid!\n");
            return;
        }
        auto ti = mEventLoop->mTimerWorkingList.begin();
        for (; ti != mEventLoop->mTimerWorkingList.end(); ++ti)
        {
            if ((*ti)->expiration() > mExpiration)
            {
                break;
            }
        }
        mEventLoop->mTimerWorkingList.insert(ti, this);
        mEnable = true;
    }
}

void CSysLoopTimer::config(EEnable enb, EEnable rpt, int32_t interval, int32_t init_value)
{
    if (rpt == ENABLE)
    {
        mRepeat = true;
    }
    else if (rpt == DISABLE)
    {
        mRepeat = false;
    }

    if (interval > 0)
    {
        mInterval = interval;
    }

    if (enb == ENABLE)
    {
        enable(true, 0, init_value);
    }
    else if (enb == DISABLE)
    {
        enable(false);
    }
}

CBaseEventLoop::CBaseEventLoop()
    : mTimerRecursiveCnt(0)
{
}

CBaseEventLoop::~CBaseEventLoop()
{
    uninstallTimers();
}

bool CBaseEventLoop::timerDestroyed(CSysLoopTimer *timer)
{
    if ((mTimerBlackList.find(timer) != mTimerBlackList.end()))
    {
        LOG_I("CBaseEventLoop: Timer is destroyed inside callback.\n");
        return true;
    }
    return false;
}

void CBaseEventLoop::addTimer(CSysLoopTimer *timer, bool enb)
{
    for (auto ti = mTimerList.begin(); ti != mTimerList.end(); ++ti)
    {
        if ((*ti) == timer)
        {
            return; // alredy added
        }
    }

    timer->eventloop(this);
    mTimerList.push_back(timer);
    timer->enable(enb);
}

void CBaseEventLoop::removeTimer(CSysLoopTimer *timer)
{
    addTimerToBlacklist(timer);
    timer->enable(false);
    fdb_remove_value_from_container(mTimerList, timer);
    timer->eventloop(0);
}

void CBaseEventLoop::addTimerToBlacklist(CSysLoopTimer *timer)
{
    mTimerBlackList.insert(timer);
}

void CBaseEventLoop::uninstallTimers()
{
    for (auto ti = mTimerList.begin(); ti != mTimerList.end();)
    {
        CSysLoopTimer *timer = *ti;
        ++ti;
        removeTimer(timer);
    }
}

int32_t CBaseEventLoop::getMostRecentTime()
{
    int32_t wait_time = -1; // wait forever
    auto ti = mTimerWorkingList.begin();
    if (ti != mTimerWorkingList.end())
    {
        uint64_t now_millis = sysdep_getsystemtime_milli();
        uint64_t min_expire = (*ti)->expiration();
        if (min_expire > now_millis)
        {
            wait_time = (int)(min_expire - now_millis); // wait until timeout
        }
        else
        {
            wait_time = 0; // no wait; expire immediately!
        }
    }
    return wait_time;
}

void CBaseEventLoop::beginTimerBlackList()
{
    if (!mTimerRecursiveCnt++)
    {
        mTimerBlackList.clear();
    }
}

void CBaseEventLoop::endTimerBlackList()
{
    if (mTimerRecursiveCnt)
    {
        mTimerRecursiveCnt--;
    }
    if (!mTimerRecursiveCnt)
    {
        mTimerBlackList.clear();
    }
}

void CBaseEventLoop::processTimers()
{
    uint64_t now_millis = sysdep_getsystemtime_milli();
    std::vector<CSysLoopTimer *> tos;
    for (auto ti = mTimerWorkingList.begin(); ti != mTimerWorkingList.end(); ++ti)
    {
        if (now_millis >= (*ti)->expiration())
        {
            tos.push_back(*ti);
        }
        else
        {
            break;
        }
    }
    if (!tos.empty())
    {
        beginTimerBlackList();
        for (auto ti = tos.begin(); ti != tos.end(); ++ti)
        {
            (*ti)->enable((*ti)->repeat(), now_millis);
        }

        for (auto ti = tos.begin(); ti != tos.end(); ++ti)
        {
            if (timerDestroyed((*ti)))
            {
                continue;
            }
            try
            {
                (*ti)->run();
            }
            catch (...)
            {
                LOG_E("CFdEventLoop: Exception received at line %d of file %s!\n", __LINE__, __FILE__);
                if (!timerDestroyed(*ti))
                {
                    removeTimer(*ti);
                }
            }
        }
        endTimerBlackList();
    }
}

void CBaseEventLoop::lock()
{
    mMutex.lock();
}

void CBaseEventLoop::unlock()
{
    mMutex.unlock();
}
