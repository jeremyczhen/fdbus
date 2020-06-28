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
#include <time.h>
#include <sys/types.h>
#include <sys/timeb.h>
#include <common_base/CBaseSysDep.h>

#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif

uint64_t sysdep_getsystemtime_milli()
{
    __timeb64 tm;
    _ftime64_s(&tm);
    return tm.millitm + (tm.time * 1000U);
}

uint64_t sysdep_getsystemtime_nano()
{
    //TODO: use another call to get accuracy nano time!!!
    return sysdep_getsystemtime_milli() * 1000000;
}

int32_t sysdep_gettimeofday(struct timeval *tv)
{
    FILETIME ft;
    unsigned __int64 tmpres = 0;
    static int tzflag;

    if (0 != tv)
    {
        GetSystemTimeAsFileTime(&ft);

        tmpres |= ft.dwHighDateTime;
        tmpres <<= 32;
        tmpres |= ft.dwLowDateTime;

        /*converting file time to unix epoch*/
        tmpres /= 10;  /*convert into microseconds*/
        tmpres -= DELTA_EPOCH_IN_MICROSECS;
        tv->tv_sec = (long)(tmpres / 1000000UL);
        tv->tv_usec = (long)(tmpres % 1000000UL);
    }

    return 0;
}


void sysdep_sleep
(
    uint32_t msecTimeout
)
{
    Sleep(static_cast<DWORD>(msecTimeout));
}

void sysdep_usleep
(
    uint32_t microsecTimeout
)
{
    Sleep(static_cast<DWORD>(microsecTimeout / 1000));
}

int poll(pollfd *fds, int n_fds, int timeout_milliseconds)
{
    fd_set read_set, write_set, err_set;
    int max_fd = 0;
    int i;
    int ready;

    FD_ZERO (&read_set);
    FD_ZERO (&write_set);
    FD_ZERO (&err_set);

    for (i = 0; i < n_fds; i++)
    {
        pollfd *fdp = &fds[i];

        if (fdp->events & POLLIN)
        {
            FD_SET (fdp->fd, &read_set);
        }

        if (fdp->events & POLLOUT)
        {
            FD_SET (fdp->fd, &write_set);
        }

        FD_SET (fdp->fd, &err_set);

        if ((int)fdp->fd > max_fd)
        {
            max_fd = (int)fdp->fd;
        }
    }

    struct timeval *ptv = 0;
    struct timeval tv;
    if (timeout_milliseconds >= 0)
    {
        tv.tv_sec = timeout_milliseconds / 1000;
        tv.tv_usec = (timeout_milliseconds % 1000) * 1000;
        ptv = &tv;
    }

    ready = select (max_fd + 1, &read_set, &write_set, &err_set, ptv);

    if (ready > 0)
    {
        for (i = 0; i < n_fds; i++)
        {
            pollfd *fdp = &fds[i];

            fdp->revents = 0;

            if (FD_ISSET (fdp->fd, &read_set))
            {
                fdp->revents |= POLLIN;
            }

            if (FD_ISSET (fdp->fd, &write_set))
            {
                fdp->revents |= POLLOUT;
            }

            if (FD_ISSET (fdp->fd, &err_set))
            {
                fdp->revents |= POLLERR;
            }
        }
    }
    return ready;
}

void sysdep_gethostname(char *name, int32_t size)
{
    gethostname(name, size);
}

