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

#ifndef _CSOCKETIMP_H_
#define _CSOCKETIMP_H_

#include <stdint.h>
#include <string>

enum EFdbSocketType
{
    FDB_SOCKET_TCP,
    FDB_SOCKET_UDP,
    FDB_SOCKET_IPC,
    FDB_SOCKET_SVC,
    FDB_SOCKET_MAX
};

struct CFdbSocketAddr
{
    std::string mUrl;
    EFdbSocketType mType;
    std::string mAddr;
    int32_t mPort;
};

struct CFdbSocketCredentials
{
    uint32_t pid;
    uint32_t gid;
    uint32_t uid;
};

struct CFdbSocketConnInfo
{
    std::string mPeerIp;
    int32_t mPeerPort;
    std::string mSelfIp;
    int32_t mSelfPort;
};

class CSocketImp
{
public:
    CSocketImp()
    {
    }

    virtual ~CSocketImp()
    {
    }

    virtual int32_t send(const uint8_t *data, int32_t size)
    {
        return -1;
    }

    virtual int32_t recv(uint8_t *data, int32_t size)
    {
        return -1;
    }

    virtual int getFd()
    {
        return -1;
    }

    virtual CFdbSocketCredentials const &getPeerCredentials() = 0;
    virtual CFdbSocketConnInfo const &getConnectionInfo() = 0;
};

class CClientSocketImp
{
public:
    CClientSocketImp(CFdbSocketAddr &addr)
    {
        mAddress = addr;
    }

    virtual ~CClientSocketImp()
    {
    }
    virtual CSocketImp *connect(bool block = false, int32_t ka_interval = 0, int32_t ka_retries = 0)
    {
        return 0;
    }
    virtual int getFd()
    {
        return -1;
    }

    CFdbSocketAddr const &getAddress()
    {
        return mAddress;
    }
protected:
    CFdbSocketAddr mAddress;
};

class CServerSocketImp
{
public:
    CServerSocketImp(CFdbSocketAddr &addr)
    {
        mAddress = addr;
    }

    virtual ~CServerSocketImp()
    {
    }

    virtual bool bind()
    {
        return false;
    }

    virtual CSocketImp *accept(bool block = false, int32_t ka_interval = 0, int32_t ka_retries = 0)
    {
        return 0;
    }

    virtual int getFd()
    {
        return -1;
    }
    CFdbSocketAddr const &getAddress()
    {
        return mAddress;
    }
protected:
    CFdbSocketAddr mAddress;
};
#endif
