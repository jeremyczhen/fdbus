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

CIpcAddressAllocator::CIpcAddressAllocator()
    : mSocketId(0)
{
}

void CIpcAddressAllocator::allocate(CFdbSocketAddr &sckt_addr, FdbServerType svc_type)
{
    if (svc_type == FDB_SVC_NAME_SERVER)
    {
        sckt_addr.mAddr = CNsConfig::getNameServerIpcPath();
        sckt_addr.mUrl = CNsConfig::getNameServerIpcUrl();
    }
    else if (svc_type == FDB_SVC_HOST_SERVER)
    {
        sckt_addr.mAddr = CNsConfig::getHostServerIpcPath();
        sckt_addr.mUrl = CNsConfig::getHostServerIpcUrl();
    }
    else
    {
        uint32_t id = mSocketId++;
        char id_string[64];
        sprintf(id_string, "%u", id);
        sckt_addr.mAddr = CNsConfig::getIpcPathBase();
        sckt_addr.mAddr += id_string;
        sckt_addr.mUrl = CNsConfig::getIpcUrlBase();
        sckt_addr.mUrl += id_string;
    }

    sckt_addr.mPort = 0;
    sckt_addr.mType = FDB_SOCKET_IPC;
}

void CIpcAddressAllocator::reset()
{
    mSocketId = 0;
}

CTcpAddressAllocator::CTcpAddressAllocator()
    : mMinPort(CNsConfig::getTcpPortMin())
    , mMaxPort(CNsConfig::getTcpPortMax())
    , mPort(mMinPort)
{
}

void CTcpAddressAllocator::allocate(CFdbSocketAddr &sckt_addr, FdbServerType svc_type)
{
    int32_t port = -1;
    if (svc_type == FDB_SVC_NAME_SERVER)
    {
        port = CNsConfig::getIntNameServerTcpPort();
    }
    else if (svc_type == FDB_SVC_HOST_SERVER)
    {
        port = CNsConfig::getIntHostServerTcpPort();
    }
    else
    {
#ifdef CFG_ALLOC_PORT_BY_SYSTEM
        port = FDB_SYSTEM_PORT;
#else
        port = mPort++;
        if (mPort > mMaxPort)
        {
            mPort = mMinPort;
        }
#endif
    }

    if (port == -1)
    {
        return;
    }

    char port_string[64];
    sprintf(port_string, "%u", port);
    
    sckt_addr.mPort = port;
    sckt_addr.mType = FDB_SOCKET_TCP;
    sckt_addr.mAddr = mInterfaceIp;
    sckt_addr.mUrl = FDB_URL_TCP;
    sckt_addr.mUrl = sckt_addr.mUrl + mInterfaceIp + ":" + port_string;
}

void CTcpAddressAllocator::reset()
{
    mPort = mMinPort;
}

void CTcpAddressAllocator::setInterfaceIp(const char *ip_addr)
{
    if (mInterfaceIp.empty())
    {
        mInterfaceIp = ip_addr;
    }
}
