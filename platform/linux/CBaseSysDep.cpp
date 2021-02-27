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

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <common_base/CBaseSysDep.h>
#include <sys/utsname.h>
#include <string.h>
#include <stdio.h>

uint64_t sysdep_getsystemtime_milli()
{
    struct timespec now;
    uint64_t msecTime(0U);

    if ( -1 != clock_gettime(CLOCK_MONOTONIC, &now) )
    {
        msecTime = (now.tv_sec * 1000U) + (now.tv_nsec / 1000000U);
    }

    return msecTime;
}

uint64_t sysdep_getsystemtime_nano()
{
    struct timespec now;
    uint64_t msecTime(0U);

    if ( -1 != clock_gettime(CLOCK_MONOTONIC, &now) )
    {
        msecTime = ((uint64_t)now.tv_sec * 1000000000U) + (uint64_t)now.tv_nsec;
    }

    return msecTime;
}

int32_t sysdep_gettimeofday(struct timeval *tv, struct timezone *tz)
{
    return gettimeofday(tv, tz);
}

void sysdep_sleep
(
    uint32_t msecTimeout
)
{
    static_cast<void>(usleep(static_cast<useconds_t>(msecTimeout * 1000U)));
}

void sysdep_usleep
(
    uint32_t microsecTimeout
)
{
    static_cast<void>(usleep(static_cast<useconds_t>(microsecTimeout)));
}

void sysdep_gethostname(char *name, int32_t size)
{
    struct utsname sysinfo;
    if( uname(&sysinfo) >= 0)
    {
        strncpy(name, sysinfo.nodename, size);
    }
}

void sysdep_gettimestamp(char *stime, int32_t size, int32_t need_millisec, int32_t format)
{
    const char *date_format_str;
    const char *ms_format_str;
    time_t now_time;
    time(&now_time);
    if (format == 0)
    {
        date_format_str = "%F %H:%M:%S";
        ms_format_str = ":%03u";
    }
    else
    {
        date_format_str = "%F_%H-%M-%S";
        ms_format_str = "-%03u";
    }

    strftime(stime, size, date_format_str, localtime(&now_time));
    if (need_millisec)
    {
        struct timespec now;

        if ( -1 != clock_gettime(CLOCK_MONOTONIC, &now) )
        {
            uint32_t ms = (now.tv_nsec / 1000000U);
            if (ms >= 1000)
            {
                ms = 999;
            }
            char ms_buf[64];
            snprintf(ms_buf, sizeof(ms_buf), ms_format_str, ms);
            strncat(stime, ms_buf, size);
        }
    }
}

