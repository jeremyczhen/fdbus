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

#include <string>

#ifdef __WIN32__
#pragma warning(disable:4250)
#define WIN32_LEAN_AND_MEAN
#include <stdint.h>
#include <process.h>
#include <windows.h>
#include <winsock2.h>
#include <io.h>
#include <time.h>

#if 0
// Basic integer types
typedef unsigned char      uint8_t;
typedef short              int16_t;
typedef unsigned short     uint16_t;
typedef unsigned long      uint32_t;
typedef long               int32_t;
typedef __int64            int64_t;
typedef unsigned __int64   uint64_t;
#endif

#if 0
/** There is data to read */
#define POLLIN 0x0001
/** There is urgent data to read */
#define POLLPRI 0x0002
/** Writing now will not block */
#define POLLOUT 0x0004
/** Error condition */
#define POLLERR 0x0008
/** Hung up */
#define POLLHUP 0x0010
/** Invalid request: fd not open */
#define POLLNVAL 0x0020
#endif

#if 0
/**
 * A portable struct pollfd wrapper.
 */
typedef struct
{
    int fd; /**< File descriptor */
    short events; /**< Events to poll for */
    short revents; /**< Events that occurred */
} CBasePollFd, pollfd;
#endif

extern "C" int poll(pollfd *fds,
                    int n_fds,
                    int timeout_milliseconds);

#define strerror_r(_e, _b, _s) strncpy(_b, strerror(_e), _s)

typedef HANDLE CBASE_tMutexHnd;
typedef HANDLE CBASE_tSemHnd;
typedef HANDLE CBASE_tThreadHnd;
typedef DWORD CBASE_tProcId;

#ifndef PRIu64
#define PRIu64 "I64u"
#endif

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

bool sysdep_startup();
bool sysdep_shutdown();
void sysdep_sleep(uint32_t msecTimeout);

// Returns time in msec since system started.
uint64_t sysdep_getsystemtime_milli();
uint64_t sysdep_getsystemtime_nano();
int32_t sysdep_gettimeofday(struct timeval *tv);
void sysdep_gethostname(std::string &name);

#endif
