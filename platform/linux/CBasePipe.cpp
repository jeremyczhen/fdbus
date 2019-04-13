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

#include <fcntl.h>
#include <unistd.h>
#include <common_base/CBasePipe.h>

CBasePipe::CBasePipe()
    : mReadFd(INVALID_FD)
    , mWriteFd(INVALID_FD)
{
}


CBasePipe::~CBasePipe()
{
    (void)close();
}


bool CBasePipe::open
(
    bool  blockOnRead,
    bool  blockOnWrite
)
{
    int32_t fds[2] = {INVALID_FD, INVALID_FD};
    bool status(false);

    int32_t rc = pipe(fds);
    if ( 0 == rc )
    {
        // If reads are non-blocking then ...
        if ( !blockOnRead )
        {
            rc = fcntl(fds[0], F_SETFL, O_NONBLOCK);
            if ( 0 > rc )
            {
                (void)::close(fds[0]);
                fds[0] = INVALID_FD;
            }
        }

        // if writes are non-blocking then ...
        if ( !blockOnWrite )
        {
            rc = fcntl(fds[1], F_SETFL, O_NONBLOCK);
            if ( 0 > rc )
            {
                (void)::close(fds[1]);
                fds[1] = INVALID_FD;
            }
        }

        if ( (INVALID_FD != fds[0]) && (INVALID_FD != fds[1]) )
        {
            status = true;
            mReadFd = fds[0];
            mWriteFd = fds[1];
        }
    }

    // Do necessary clean-up if there was a failure opening the pipe.
    if ( !status )
    {
        if ( INVALID_FD != fds[0] )
        {
            (void)::close(fds[0]);
        }

        if ( INVALID_FD != fds[1] )
        {
            (void)::close(fds[1]);
        }
    }

    return status;
}


bool CBasePipe::close()
{
    bool closed(true);

    if ( INVALID_FD != mReadFd )
    {
        if ( 0 != ::close(mReadFd) )
        {
            closed = false;
        }
        mReadFd = INVALID_FD;
    }

    if ( INVALID_FD != mWriteFd )
    {
        if ( 0 != ::close(mWriteFd) )
        {
            closed = false;
        }
        mWriteFd = INVALID_FD;
    }

    return closed;
}


int32_t CBasePipe::getReadFd() const
{
    return mReadFd;
}


int32_t CBasePipe::getWriteFd() const
{
    return mWriteFd;
}


int32_t CBasePipe::read
(
    void    *buf,
    uint32_t nBytes
)
{
    return (int32_t)::read(mReadFd, buf, nBytes);
}


int32_t CBasePipe::write
(
    const void *buf,
    uint32_t nBytes
)
{
    return (int32_t)::write(mWriteFd, buf, nBytes);
}

