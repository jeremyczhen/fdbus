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
    SOCKET temp;
    SOCKET socket1 = INVALID_FD;
    SOCKET socket2 = INVALID_FD;
    struct sockaddr_in saddr;
    int len;
    u_long arg;
    fd_set read_set;
    fd_set write_set;
    struct timeval tv;

    temp = socket(AF_INET, SOCK_STREAM, 0);
    if (temp == INVALID_SOCKET)
    {
        goto out0;
    }

    arg = 1;
    if (ioctlsocket(temp, FIONBIO, &arg) == SOCKET_ERROR)
    {
        goto out0;
    }

    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = 0;
    saddr.sin_addr.s_addr = htonl (INADDR_LOOPBACK);

    if (bind (temp, (struct sockaddr *)&saddr, sizeof (saddr)))
    {
        goto out0;
    }

    if (listen (temp, 1) == SOCKET_ERROR)
    {
        goto out0;
    }

    len = sizeof (saddr);
    if (getsockname(temp, (struct sockaddr *)&saddr, &len))
    {
        goto out0;
    }

    socket1 = socket(AF_INET, SOCK_STREAM, 0);
    if (socket1 == INVALID_SOCKET)
    {
        goto out0;
    }

    arg = 1;
    if (ioctlsocket(socket1, FIONBIO, &arg) == SOCKET_ERROR)
    {
        goto out1;
    }

    if (connect(socket1, (struct sockaddr *)&saddr, len) != SOCKET_ERROR ||
            WSAGetLastError () != WSAEWOULDBLOCK)
    {
        goto out1;
    }

    FD_ZERO (&read_set);
    FD_SET (temp, &read_set);

    tv.tv_sec = 0;
    tv.tv_usec = 0;

    if (select(0, &read_set, 0, 0, 0) == SOCKET_ERROR)
    {
        goto out1;
    }

    socket2 = accept(temp, (struct sockaddr *) &saddr, &len);
    if (socket2 == INVALID_SOCKET)
    {
        goto out1;
    }

    FD_ZERO(&write_set);
    FD_SET(socket1, &write_set);

    tv.tv_sec = 0;
    tv.tv_usec = 0;

    if ( select(0, NULL, &write_set, 0, 0) == SOCKET_ERROR )
    {
        goto out2;
    }

    // NOTE: socket1 was already set to non-blocking earlier
    if ( blockOnRead )
    {
        arg = 0;
        if ( ioctlsocket(socket1, FIONBIO, &arg) == SOCKET_ERROR )
        {
            goto out2;
        }
    }

    if ( blockOnWrite )
    {
        arg = 0;
        if ( ioctlsocket (socket2, FIONBIO, &arg) == SOCKET_ERROR )
        {
            goto out2;
        }
    }
    else
    {
        arg = 1;
        if ( ioctlsocket (socket2, FIONBIO, &arg) == SOCKET_ERROR )
        {
            goto out2;
        }
    }

    mReadFd = socket1;
    mWriteFd = socket2;

    closesocket(temp);

    return true;

out2:
    closesocket(socket2);
out1:
    closesocket(socket1);
out0:
    closesocket (temp);

    return false;
}


bool CBasePipe::close()
{
    bool closed(true);

    if ( INVALID_FD != mReadFd )
    {
        if ( 0 != closesocket(mReadFd) )
        {
            closed = false;
        }
        mReadFd = INVALID_FD;
    }

    if ( INVALID_FD != mWriteFd )
    {
        if ( 0 != closesocket(mWriteFd) )
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
    int32_t ret = recv(mReadFd, static_cast<char *>(buf), nBytes, 0);
    return ((ret == SOCKET_ERROR) ? -1 : ret);
}


int32_t CBasePipe::write
(
    const void *buf,
    uint32_t    nBytes
)
{
    int32_t ret = send(mWriteFd, static_cast<const char *>(buf), nBytes, 0);
    return ((ret == SOCKET_ERROR) ? -1 : ret);
}

