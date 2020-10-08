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
 
#include <stdio.h>
#include "CAddressAllocator.h"
#include <utils/CNsConfig.h>
#include <string.h>

FdbServerType IAddressAllocator::getSvcType(const char *svc_name)
{
    if (!strcmp(svc_name, CNsConfig::getNameServerName()))
    {
        return FDB_SVC_NAME_SERVER;
    }
    else if (!strcmp(svc_name, CNsConfig::getHostServerName()))
    {
        return FDB_SVC_HOST_SERVER;
    }
    else
    {
        return FDB_SVC_USER;
    }
}

CIPCAddressAllocator::CIPCAddressAllocator()
    : mSocketId(0)
{
}

void CIPCAddressAllocator::allocate(CFdbSocketAddr &sckt_addr, FdbServerType svc_type)
{
    if (svc_type == FDB_SVC_NAME_SERVER)
    {
        sckt_addr.mAddr = CNsConfig::getNameServerIPCPath();
        sckt_addr.mUrl = CNsConfig::getNameServerIPCUrl();
    }
    else if (svc_type == FDB_SVC_HOST_SERVER)
    {
        sckt_addr.mAddr = CNsConfig::getHostServerIPCPath();
        sckt_addr.mUrl = CNsConfig::getHostServerIPCUrl();
    }
    else
    {
        uint32_t id = mSocketId++;
        char id_string[64];
        sprintf(id_string, "%u", id);
        sckt_addr.mAddr = CNsConfig::getIPCPathBase();
        sckt_addr.mAddr += id_string;
        sckt_addr.mUrl = CNsConfig::getIPCUrlBase();
        sckt_addr.mUrl += id_string;
    }

    sckt_addr.mPort = 0;
    sckt_addr.mType = FDB_SOCKET_IPC;
}

void CIPCAddressAllocator::reset()
{
    mSocketId = 0;
}

CTCPAddressAllocator::CTCPAddressAllocator()
    : mMinPort(CNsConfig::getTCPPortMin())
    , mMaxPort(CNsConfig::getTCPPortMax())
    , mPort(mMinPort)
{
}

void CTCPAddressAllocator::allocate(CFdbSocketAddr &sckt_addr, FdbServerType svc_type)
{
    int32_t port;
    if (svc_type == FDB_SVC_NAME_SERVER)
    {
        port = CNsConfig::getIntNameServerTCPPort();
    }
    else if (svc_type == FDB_SVC_HOST_SERVER)
    {
        port = CNsConfig::getIntHostServerTCPPort();
    }
    else
    {
#ifdef CFG_ALLOC_PORT_BY_SYSTEM
        port = FDB_INET_PORT_AUTO;
#else
        port = mPort++;
        if (mPort > mMaxPort)
        {
            mPort = mMinPort;
        }
#endif
    }

    char port_string[64];
    sprintf(port_string, "%u", port);
    
    sckt_addr.mPort = port;
    sckt_addr.mType = FDB_SOCKET_TCP;
    sckt_addr.mAddr = mInterfaceIp;
    sckt_addr.mUrl = FDB_URL_TCP;
    sckt_addr.mUrl = sckt_addr.mUrl + mInterfaceIp + ":" + port_string;
}

void CTCPAddressAllocator::reset()
{
    mPort = mMinPort;
}

void CTCPAddressAllocator::setInterfaceIp(const char *ip_addr)
{
    if (mInterfaceIp.empty())
    {
        mInterfaceIp = ip_addr;
    }
}
CUDPPortAllocator::CUDPPortAllocator()
    : mMinPort(CNsConfig::getTCPPortMin())
    , mMaxPort(CNsConfig::getTCPPortMax())
    , mPort(mMinPort)
{
}

int32_t CUDPPortAllocator::allocate()
{
    int32_t port;
#ifdef CFG_ALLOC_PORT_BY_USER
    port = mPort++;
    if (mPort > mMaxPort)
    {
        mPort = mMinPort;
    }
#else
    port = FDB_INET_PORT_AUTO;
#endif
    return port;
}

