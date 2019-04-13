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

#include "CLinuxSocket.h"

CLinuxSocket::CLinuxSocket(sckt::TCPSocket *imp, bool del_imp)
    : mSocketImp(imp)
    , mDeleteImp(del_imp)
{
    mCred.pid = imp->pid;
    mCred.gid = imp->gid;
    mCred.uid = imp->uid;
    mConn.mPeerIp = imp->peer_ip;
    mConn.mPeerPort = imp->peer_port;
    mConn.mSelfIp = imp->self_ip;
    mConn.mSelfPort = imp->self_port;
}

CLinuxSocket::~CLinuxSocket()
{
    if (mSocketImp && mDeleteImp)
    {
        delete mSocketImp;
    }
}

int32_t CLinuxSocket::send(const uint8_t *data, int32_t size)
{
    int32_t ret = -1;
    if (mSocketImp)
    {
        try
        {
            ret = mSocketImp->Send(data, size);
        }
        catch(...)
        {
            ret = -1;
        }
    }
    return ret;
}

int32_t CLinuxSocket::recv(uint8_t *data, int32_t size)
{
    int32_t ret = -1;
    if (mSocketImp)
    {
        try
        {
            ret = mSocketImp->Recv(data, size);
        }
        catch(...)
        {
            ret = -1;
        }
    }
    return ret;
}

int CLinuxSocket::getFd()
{
    if (mSocketImp)
    {
        return mSocketImp->getNativeSocket();
    }
    return -1;
}

CFdbSocketCredentials const &CLinuxSocket::getPeerCredentials()
{
    return mCred;
}

CFdbSocketConnInfo const &CLinuxSocket::getConnectionInfo()
{
    return mConn;
}

CLinuxClientSocket::CLinuxClientSocket(CFdbSocketAddr &addr)
    : CClientSocketImp(addr)
    , mClientSocketImp(0)
{
}

CLinuxClientSocket::~CLinuxClientSocket()
{
    if (mClientSocketImp)
    {
        delete mClientSocketImp;
    }
}

CSocketImp *CLinuxClientSocket::connect()
{
    CSocketImp *ret = 0;
    try
    {
        if (mClientSocketImp)
        {
        }
        {
            if (mAddress.mType == FDB_SOCKET_TCP)
            {
                if (mAddress.mAddr.empty())
                {
                    mAddress.mAddr = "127.0.0.1";
                }
                sckt::IPAddress address(mAddress.mAddr.c_str(), (sckt::u16)mAddress.mPort);
                mClientSocketImp = new sckt::TCPSocket(address);
            }
#ifndef __WIN32__
            else if (mAddress.mType == FDB_SOCKET_IPC)
            {
                sckt::IPAddress address(mAddress.mAddr.c_str());
                mClientSocketImp = new sckt::TCPSocket(address);
            }
#endif
            else
            {
                return 0;
            }
        }
        ret = new CLinuxSocket(mClientSocketImp, false);
    }
    catch (...)
    {
        ret = 0;
    }
    return ret;
}

int CLinuxClientSocket::getFd()
{
    if (mClientSocketImp)
    {
        return mClientSocketImp->getNativeSocket();
    }
    return -1;
}

CLinuxServerSocket::CLinuxServerSocket(CFdbSocketAddr &addr)
    : CServerSocketImp(addr)
    , mServerSocketImp(0)
{
}

CLinuxServerSocket::~CLinuxServerSocket()
{
    if (mServerSocketImp)
    {
        delete mServerSocketImp;
    }
}

bool CLinuxServerSocket::bind()
{
    bool ret = false;
    try
    {
        if (mServerSocketImp)
        {
        }
        else
        {
            if (mAddress.mType == FDB_SOCKET_TCP)
            {
                sckt::IPAddress address(mAddress.mAddr.c_str(), (sckt::u16)mAddress.mPort);
                mServerSocketImp = new sckt::TCPServerSocket(address);
            }
#ifndef __WIN32__
            else if (mAddress.mType == FDB_SOCKET_IPC)
            {
                sckt::IPAddress address(mAddress.mAddr.c_str());
                mServerSocketImp = new sckt::TCPServerSocket(address);
            }
#endif
            else
            {
                return 0;
            }
        }
        ret = true;
    }
    catch (...)
    {
        ret = false;
    }
    return ret;

}

CSocketImp *CLinuxServerSocket::accept()
{
    CSocketImp *ret = 0;
    sckt::TCPSocket *sock_imp = 0;
    try
    {
        if (mServerSocketImp)
        {
            sock_imp = new sckt::TCPSocket();
            mServerSocketImp->Accept(*sock_imp);
            ret = new CLinuxSocket(sock_imp);
        }
    }
    catch (...)
    {
        if (sock_imp)
        {
            delete sock_imp;
        }
        ret = 0;
    }
    return ret;
}

int CLinuxServerSocket::getFd()
{
    if (mServerSocketImp)
    {
        return mServerSocketImp->getNativeSocket();
    }
    return -1;
}

bool getLinuxIpAddress(std::map<std::string, std::string> &addr_tbl)
{
    try
    {
        sckt::getIpAddress(addr_tbl);
    }
    catch (...)
    {
        return false;
    }
    return true;
}
