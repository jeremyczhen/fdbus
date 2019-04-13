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
    static CBaseSocketFactory *getInstance()
    {
        if (!mInstance)
        {
            mInstance = new CBaseSocketFactory();
        }
        return mInstance;
    }
    CClientSocketImp *createClientSocket(CFdbSocketAddr &addr);
    CClientSocketImp *createClientSocket(const char *url);
    CServerSocketImp *createServerSocket(CFdbSocketAddr &addr);
    CServerSocketImp *createServerSocket(const char *url);
    bool parseUrl(const char *url, CFdbSocketAddr &addr);
    static bool getIpAddress(tIpAddressTbl &addr_tbl);
    static bool getIpAddress(std::string &address, const char *if_name = 0);
    static void buildUrl(std::string &url, EFdbSocketType type, const char *ip_path_svc, const char *port = 0);
    static void buildUrl(std::string &url, EFdbSocketType type, const char *ip_path_svc, int32_t port);
private:
    int32_t buildTcpAddress(const char *host_addr, CFdbSocketAddr &addr);
    int32_t buildIpcAddress(const char *addr_str, CFdbSocketAddr &addr);
    int32_t buildSvcAddress(const char *host_name, CFdbSocketAddr &addr);
    CBaseSocketFactory()
    {}
    static CBaseSocketFactory *mInstance;
};

#endif
