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
#include "linux/CLinuxSocket.h"
#include <common_base/CBaseSocketFactory.h>
#include <common_base/common_defs.h>
#include <utils/CNsConfig.h>

CClientSocketImp *CBaseSocketFactory::createClientSocket(CFdbSocketAddr &addr)
{
    return new CLinuxClientSocket(addr);
}

CClientSocketImp *CBaseSocketFactory::createClientSocket(const char *url)
{
    CFdbSocketAddr addr;
    if (!parseUrl(url, addr))
    {
        return 0;
    }

    return createClientSocket(addr);
}

CServerSocketImp *CBaseSocketFactory::createServerSocket(CFdbSocketAddr &addr)
{
    return new CLinuxServerSocket(addr);
}

CServerSocketImp *CBaseSocketFactory::createServerSocket(const char *url)
{
    CFdbSocketAddr addr;
    if (!parseUrl(url, addr))
    {
        return 0;
    }

    return createServerSocket(addr);
}

bool CBaseSocketFactory::parseUrl(const char *url, CFdbSocketAddr &addr)
{
    if (!url)
    {
        return false;
    }
    std::string u (url);
    std::string::size_type pos = u.find ("://");
    if (pos == std::string::npos)
    {
        return false;
    }
    std::string protocol = u.substr (0, pos);
    std::string addr_str = u.substr (pos + 3);

    if (protocol.empty () || addr_str.empty ())
    {
        return false;
    }

    addr.mUrl = url;
    if (protocol == FDB_URL_TCP_IND)
    {
        addr.mType = FDB_SOCKET_TCP;
        if (buildTcpAddress(addr_str.c_str(), addr))
        {
            return false;
        }
    }
    else if (protocol == FDB_URL_IPC_IND)
    {
        addr.mType = FDB_SOCKET_IPC;
        if (buildIpcAddress(addr_str.c_str(), addr))
        {
            return false;
        }
    }
    else if (protocol == FDB_URL_SVC_IND)
    {
        addr.mType = FDB_SOCKET_SVC;
        if (buildSvcAddress(addr_str.c_str(), addr))
        {
            return false;
        }
    }
    else
    {
        return false;
    }

    return true;
}

int32_t CBaseSocketFactory::buildTcpAddress(const char *host_addr, CFdbSocketAddr &addr)
{
    const char *delimiter = strrchr (host_addr, ':');
    if (!delimiter)
    {
        return -1;
    }
    std::string addr_str(host_addr, delimiter - host_addr);
    std::string port_str(delimiter + 1);

    //  Remove square brackets around the address, if any, as used in IPv6
    if (addr_str.size () >= 2 && addr_str [0] == '[' &&
            addr_str [addr_str.size () - 1] == ']')
    {
        addr_str = addr_str.substr (1, addr_str.size () - 2);
    }

    //  Allow 0 specifically, to detect invalid port error in atoi if not
    uint16_t port;
    if (port_str == "*" || port_str == "0")
        //  Resolve wildcard to 0 to allow autoselection of port
    {
        port = 0;
    }
    else
    {
        //  Parse the port number (0 is not a valid port).
        port = (uint16_t) atoi (port_str.c_str ());
        if (port == 0)
        {
            return -1;
        }
    }
    addr.mAddr = addr_str;
    addr.mPort = port;
    return 0;
}

int32_t CBaseSocketFactory::buildIpcAddress(const char *addr_str, CFdbSocketAddr &addr)
{
    addr.mAddr = addr_str;
    addr.mPort = 0;
    return 0;
}

int32_t CBaseSocketFactory::buildSvcAddress(const char *host_name, CFdbSocketAddr &addr)
{
    addr.mAddr = host_name;
    return 0;
}

bool CBaseSocketFactory::getIpAddress(tIpAddressTbl &addr_tbl)
{
    return getLinuxIpAddress(addr_tbl);
}

bool CBaseSocketFactory::getIpAddress(std::string &address, const char *if_name)
{
    tIpAddressTbl addr_tbl;
    if (getLinuxIpAddress(addr_tbl))
    {
        tIpAddressTbl::iterator it;
        if (if_name && (if_name[0] != '\0'))
        {
            it = addr_tbl.find(if_name);
            if (it != addr_tbl.end())
            {
                address = it->second;
                return true;
            }
        }
        else
        {
            for (it = addr_tbl.begin(); it != addr_tbl.end(); ++it)
            {
                if (!it->second.compare("127.0.0.1"))
                {
                    continue;
                }
                address = it->second;
                return true;
            }
        }
    }

    return false;
}

void CBaseSocketFactory::buildUrl(std::string &url, const char *ip_addr, const char *port)
{
    url = FDB_URL_TCP;
    if (ip_addr)
    {
        url = url + ip_addr + ":" + port;
    }
    else
    {
        url = url + ":" + port;
    }
}

void CBaseSocketFactory::buildUrl(std::string &url, const char *ip_addr, int32_t port)
{
    char port_string[64];
    sprintf(port_string, "%u", port);
    buildUrl(url, ip_addr, port_string);
}

void CBaseSocketFactory::buildUrl(std::string &url, uint32_t uds_id)
{
    char uds_id_string[64];
    sprintf(uds_id_string, "%u", uds_id);

    url = CNsConfig::getIpcUrlBase();
    url += uds_id_string;
}

void CBaseSocketFactory::buildUrl(std::string &url, const char *svc_name)
{
    url = FDB_URL_SVC;
    url = url + ":" + svc_name;
}

void CBaseSocketFactory::updatePort(CFdbSocketAddr &addr, int32_t new_port)
{
    if (new_port != addr.mPort)
    {
        buildUrl(addr.mUrl, addr.mAddr.c_str(), new_port);
        addr.mPort = new_port;
    }
}
