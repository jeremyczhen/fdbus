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

#ifndef _CLINUXSOCKET_H_
#define _CLINUXSOCKET_H_

#include <common_base/CSocketImp.h>
#include <platform/socket/sckt-0.5/sckt.hpp>

bool getLinuxIpAddress(std::map<std::string, std::string> &addr_tbl);

class CLinuxSocket : public CSocketImp
{
public:
    CLinuxSocket(sckt::TCPSocket *imp);
    ~CLinuxSocket();
    int32_t send(const uint8_t *data, int32_t size);
    int32_t recv(uint8_t *data, int32_t size);
    int getFd();
    CFdbSocketCredentials const &getPeerCredentials();
    CFdbSocketConnInfo const &getConnectionInfo();
private:
    //sckt::Socket *mSocketImp;
    sckt::TCPSocket *mSocketImp;
    CFdbSocketCredentials mCred;
    CFdbSocketConnInfo mConn;
};

class CLinuxClientSocket : public CClientSocketImp
{
public:
    CLinuxClientSocket(CFdbSocketAddr &addr);
    ~CLinuxClientSocket();
    CSocketImp *connect(bool block = false, int32_t ka_interval = 0, int32_t ka_retries = 0);
};

class CLinuxServerSocket : public CServerSocketImp
{
public:
    CLinuxServerSocket(CFdbSocketAddr &addr);
    ~CLinuxServerSocket();
    bool bind();
    CSocketImp *accept(bool block = false, int32_t ka_interval = 0, int32_t ka_retries = 0);
    int getFd();
private:
    sckt::TCPServerSocket *mServerSocketImp;
};

#endif
