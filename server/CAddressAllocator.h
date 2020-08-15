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
#ifndef __CADDRESSALLOCATOR_H__
#define __CADDRESSALLOCATOR_H__

#include <string>
#include <common_base/CSocketImp.h>

enum FdbServerType
{
    FDB_SVC_NAME_SERVER,
    FDB_SVC_HOST_SERVER,
    FDB_SVC_LOG_SERVER,
    FDB_SVC_USER
};

class IAddressAllocator
{
public:
    virtual void allocate(CFdbSocketAddr &sckt_addr, FdbServerType svc_type) = 0;
    virtual void reset() = 0;
    virtual ~IAddressAllocator() {}
    static FdbServerType getSvcType(const char *svc_name);
};

class CIpcAddressAllocator : public IAddressAllocator
{
public:
    CIpcAddressAllocator();
    void allocate(CFdbSocketAddr &sckt_addr, FdbServerType svc_type);
    void reset();

private:
    uint32_t mSocketId;
};

class CTcpAddressAllocator : public IAddressAllocator
{
public:
    CTcpAddressAllocator();
    void allocate(CFdbSocketAddr &sckt_addr, FdbServerType svc_type);
    void reset();
    void setInterfaceIp(const char *ip_addr);

private:
    int32_t mMinPort;
    int32_t mMaxPort;
    int32_t mPort;
    std::string mInterfaceIp;
};

#endif
