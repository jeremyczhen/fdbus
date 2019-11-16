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

#ifndef __CNANOTIMER_H__
#define __CNANOTIMER_H__

#ifdef  __cplusplus
#include "CBaseSysDep.h"
class CNanoTimer
{
public:
#define INVALID_TIME            (uint64_t)~0
    CNanoTimer()
    {
        totalTimer_ = 0;
        startTimer_ = 0;
    }

    void reset(void)
    {
        totalTimer_ = 0;
        startTimer_ = 0;
    }

    static uint64_t getNanoSecTimer(void)
    {
        return sysdep_getsystemtime_nano();
    }

    void startTimer(uint64_t start_time = INVALID_TIME)
    {
        startTimer_ = (start_time == INVALID_TIME) ? getNanoSecTimer() : start_time;
    }
    void stopTimer(uint64_t stop_time = INVALID_TIME)
    {
        uint64_t end = (stop_time == INVALID_TIME) ? getNanoSecTimer() : stop_time;
        totalTimer_ += end - startTimer_;
        startTimer_ = 0;
    }
    inline uint64_t getTotalSeconds()
    {
        return totalTimer_ / 1000000000;
    }
    inline uint64_t getTotalMilliseconds()
    {
        return totalTimer_ / 1000000;
    }
    inline uint64_t getTotalMicroseconds()
    {
        return totalTimer_ / 1000;
    }
    inline uint64_t getTotalNanoseconds()
    {
        return totalTimer_;
    }
    inline uint64_t getCurrentSeconds()
    {
        return (totalTimer_ + (startTimer_ > 0 ? getNanoSecTimer() - startTimer_ : 0)) / 1000000000;
    }

private:
    uint64_t startTimer_;
    uint64_t totalTimer_;
};
#else
struct sNanoTimer
{
    uint64_t startTimer_;
    uint64_t totalTimer_;
};

static inline void createNanoTimer(struct sNanoTimer *timer)
{
    timer->totalTimer_ = 0;
    timer->startTimer_ = 0;
}

static inline uint64_t getNanoSecTimer()
{
    struct timespec ts_ = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &ts_);
    return (uint64_t)ts_.tv_sec * 1000000000 +  (uint64_t)ts_.tv_nsec;
}

static void startNanoTimer(struct sNanoTimer *timer)
{
    timer->startTimer_ = getNanoSecTimer();
}
static void stopNanoTimer(struct sNanoTimer *timer)
{
    //assert(startTimer_ > 0);
    uint64_t end = getNanoSecTimer();
    timer->totalTimer_ += end - timer->startTimer_;
    timer->startTimer_ = 0;
}
static inline uint32_t getTotalSeconds(struct sNanoTimer *timer)
{
    return timer->totalTimer_ / 1000000000;
}
static inline uint32_t getTotalMilliseconds(struct sNanoTimer *timer)
{
    return timer->totalTimer_ / 1000000;
}
static inline uint32_t getTotalMicroseconds(struct sNanoTimer *timer)
{
    return timer->totalTimer_ / 1000;
}
static inline uint32_t getTotalNanoseconds(struct sNanoTimer *timer)
{
    return timer->totalTimer_;
}
static inline uint32_t getCurrentSeconds(struct sNanoTimer *timer)
{
    return (timer->totalTimer_ + (timer->startTimer_ > 0 ? getNanoSecTimer() - timer->startTimer_ : 0)) / 1000000000;
}

#endif
#endif
