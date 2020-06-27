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
        startTimer_ = 0;
    }

    void reset(void)
    {
        startTimer_ = 0;
    }

    static uint64_t getNanoSecTimer(void)
    {
        return sysdep_getsystemtime_nano();
    }

    void start(uint64_t start_time = INVALID_TIME)
    {
        startTimer_ = (start_time == INVALID_TIME) ? getNanoSecTimer() : start_time;
    }
    uint64_t snapshot(uint64_t cur_time = INVALID_TIME)
    {
        uint64_t end = (cur_time == INVALID_TIME) ? getNanoSecTimer() : cur_time;
        return end - startTimer_;
    }
    inline uint64_t snapshotSeconds(uint64_t cur_time = INVALID_TIME)
    {
        return snapshot(cur_time) / 1000000000;
    }
    inline uint64_t snapshotMilliseconds(uint64_t cur_time = INVALID_TIME)
    {
        return snapshot(cur_time) / 1000000;
    }
    inline uint64_t snapshotMicroseconds(uint64_t cur_time = INVALID_TIME)
    {
        return snapshot(cur_time) / 1000;
    }
    inline uint64_t snapshotNanoseconds(uint64_t cur_time = INVALID_TIME)
    {
        return snapshot(cur_time);
    }

private:
    uint64_t startTimer_;
};
#else
struct sNanoTimer
{
    uint64_t startTimer_;
};

static inline void createNanoTimer(struct sNanoTimer *timer)
{
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
static void snapshotNanoTimer(struct sNanoTimer *timer)
{
    //assert(startTimer_ > 0);
    uint64_t end = getNanoSecTimer();
    return end - timer->startTimer_;
}
static inline uint32_t snapshotSeconds(struct sNanoTimer *timer)
{
    return snapshotNanoTimer(timer) / 1000000000;
}
static inline uint32_t snapshotMilliseconds(struct sNanoTimer *timer)
{
    return snapshotNanoTimer(timer) / 1000000;
}
static inline uint32_t snapshotMicroseconds(struct sNanoTimer *timer)
{
    return snapshotNanoTimer(timer) / 1000;
}
static inline uint32_t snapshotNanoseconds(struct sNanoTimer *timer)
{
    return snapshotNanoTimer(timer);
}

#endif
#endif
