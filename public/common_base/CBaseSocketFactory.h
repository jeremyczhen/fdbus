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

#ifndef _CBASESOCKETFACTORY_H_
#define _CBASESOCKETFACTORY_H_

#include <map>
#include <string>
#include "CSocketImp.h"

class CBaseSocketFactory
{
public:
    typedef std::map<std::string, std::string> tIpAddressTbl;
    static CClientSocketImp *createClientSocket(CFdbSocketAddr &addr);
    static CClientSocketImp *createClientSocket(const char *url);
    static CServerSocketImp *createServerSocket(CFdbSocketAddr &addr);
    static CServerSocketImp *createServerSocket(const char *url);
    static bool parseUrl(const char *url, CFdbSocketAddr &addr);
    static bool getIpAddress(tIpAddressTbl &addr_tbl);
    static bool getIpAddress(std::string &address, const char *if_name = 0);
    static void buildUrl(std::string &url, const char *ip_addr, const char *port);
    static void buildUrl(std::string &url, const char *ip_addr, int32_t port);
    static void buildUrl(std::string &url, uint32_t uds_id);
    static void buildUrl(std::string &url, const char *svc_name);
    static void updatePort(CFdbSocketAddr &addr, int32_t new_port);
private:
    static int32_t buildTcpAddress(const char *host_addr, CFdbSocketAddr &addr);
    static int32_t buildIpcAddress(const char *addr_str, CFdbSocketAddr &addr);
    static int32_t buildSvcAddress(const char *host_name, CFdbSocketAddr &addr);
};

#endif
