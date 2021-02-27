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

#ifndef _CBASESYSDEP_HPP_
#define _CBASESYSDEP_HPP_

#ifdef __WIN32__
#pragma warning(disable:4250)
#define WIN32_LEAN_AND_MEAN
#include <stdint.h>
#include <process.h>
#include <windows.h>
#include <winsock2.h>
#include <io.h>
#include <time.h>

#define	LIB_EXPORT __declspec(dllexport)

#ifdef __cplusplus
extern "C"
{
#endif

int poll(struct pollfd *fds, int n_fds, int timeout_milliseconds);

#ifdef __cplusplus
}
#endif

#define strerror_r(_e, _b, _s) strncpy(_b, strerror(_e), _s)

typedef HANDLE CBASE_tMutexHnd;
typedef HANDLE CBASE_tSemHnd;
typedef HANDLE CBASE_tThreadHnd;
typedef DWORD CBASE_tProcId;

#define closePollFd(_fd)   do {  \
    shutdown(_fd, SD_BOTH);  \
    closesocket(_fd);   \
}while (0)

#else
#include <stdint.h>
#include <errno.h>
//#include <sys/poll.h>
#include <poll.h>
#include <sys/poll.h>
#include <unistd.h>
//#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>

#define LIB_EXPORT

typedef pthread_mutex_t CBASE_tMutexHnd;
typedef sem_t CBASE_tSemHnd;
typedef pthread_t CBASE_tThreadHnd;
typedef pid_t CBASE_tProcId;

#ifndef PRIu64
#define PRIu64 "llu"
#endif

#define closePollFd(_fd)    close(_fd)

#endif

#ifdef __WIN32__
struct timezone
{
    int  tz_minuteswest; /* minutes W of Greenwich */
    int  tz_dsttime;       /* type of dst correction */
};
#endif

#ifndef ARRAY_LENGTH
#define ARRAY_LENGTH(_a) (int32_t)((sizeof (_a) / sizeof (_a)[0]))
#endif

#ifdef __cplusplus
extern "C"
{
#endif

void sysdep_sleep(uint32_t msecTimeout);
void sysdep_usleep(uint32_t microsecTimeout);
// Returns time in msec since system started.
uint64_t sysdep_getsystemtime_milli();
uint64_t sysdep_getsystemtime_nano();
int32_t sysdep_gettimeofday(struct timeval *tv);
void sysdep_gethostname(char *name, int32_t size);
void sysdep_gettimestamp(char *stime, int32_t size, int32_t need_millisec, int32_t format);

#ifdef __cplusplus
}
#endif

#endif
