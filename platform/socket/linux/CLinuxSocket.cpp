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
#include <common_base/CBaseSocketFactory.h>

CTCPTransportSocket::CTCPTransportSocket(sckt::TCPSocket *imp, EFdbSocketType type)
    : mSocketImp(imp)
{
    mCred.pid = imp->pid;
    mCred.gid = imp->gid;
    mCred.uid = imp->uid;
    mConn.mPeerIp = imp->peer_ip;
    mConn.mPeerPort = imp->peer_port;

    // For TCP socket, address of CTCPTransportSocket is the different from CLinuxServerSocket
    // So you SHOULD get address either from session only!!!
    mConn.mSelfAddress.mType = type;
    mConn.mSelfAddress.mAddr = imp->self_ip;
    mConn.mSelfAddress.mPort = imp->self_port;
}

CTCPTransportSocket::~CTCPTransportSocket()
{
    if (mSocketImp)
    {
        delete mSocketImp;
    }
}

int32_t CTCPTransportSocket::send(const uint8_t *data, int32_t size)
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

int32_t CTCPTransportSocket::recv(uint8_t *data, int32_t size)
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

int CTCPTransportSocket::getFd()
{
    if (mSocketImp)
    {
        return mSocketImp->getNativeSocket();
    }
    return -1;
}

CLinuxClientSocket::CLinuxClientSocket(CFdbSocketAddr &addr)
    : CClientSocketImp(addr)
{
}

CLinuxClientSocket::~CLinuxClientSocket()
{
}

CSocketImp *CLinuxClientSocket::connect()
{
    CSocketImp *ret = 0;
    try
    {
        sckt::TCPSocket *sckt_imp = 0;
        if (mConn.mSelfAddress.mType == FDB_SOCKET_TCP)
        {
            if (mConn.mSelfAddress.mAddr.empty())
            {
                mConn.mSelfAddress.mAddr = "127.0.0.1";
            }
            sckt::IPAddress address(mConn.mSelfAddress.mAddr.c_str(), (sckt::u16)mConn.mSelfAddress.mPort);
            sckt_imp = new sckt::TCPSocket(address);
        }
#ifndef __WIN32__
        else if (mConn.mSelfAddress.mType == FDB_SOCKET_IPC)
        {
            sckt::IPAddress address(mConn.mSelfAddress.mAddr.c_str());
            sckt_imp = new sckt::TCPSocket(address);
        }
#endif

        if (sckt_imp)
        {
            ret = new CTCPTransportSocket(sckt_imp, mConn.mSelfAddress.mType);
            CFdbSocketConnInfo &conn_info = const_cast<CFdbSocketConnInfo &>(ret->getConnectionInfo());
            if (mConn.mSelfAddress.mType == FDB_SOCKET_IPC)
            {
                // For IPC socket, address of CTCPTransportSocket is the same as CLinuxServerSocket
                // So you can get address either from container or session
                conn_info.mSelfAddress = mConn.mSelfAddress;
            }
            else
            {
                CBaseSocketFactory::buildUrl(conn_info.mSelfAddress.mUrl,
                                             conn_info.mSelfAddress.mAddr.c_str(),
                                             conn_info.mSelfAddress.mPort);
            }
        }
    }
    catch (...)
    {
        ret = 0;
    }
    return ret;
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
            if (mConn.mSelfAddress.mType == FDB_SOCKET_TCP)
            {
                sckt::IPAddress address(mConn.mSelfAddress.mAddr.c_str(), (sckt::u16)mConn.mSelfAddress.mPort);
                mServerSocketImp = new sckt::TCPServerSocket(address);
                mConn.mSelfAddress.mPort = mServerSocketImp->self_port; // in case port number is allocated dynamically...
            }
#ifndef __WIN32__
            else if (mConn.mSelfAddress.mType == FDB_SOCKET_IPC)
            {
                sckt::IPAddress address(mConn.mSelfAddress.mAddr.c_str());
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
            ret = new CTCPTransportSocket(sock_imp, mConn.mSelfAddress.mType);
            CFdbSocketConnInfo &conn_info = const_cast<CFdbSocketConnInfo &>(ret->getConnectionInfo());
            if (mConn.mSelfAddress.mType == FDB_SOCKET_IPC)
            {
                // For IPC socket, address of CTCPTransportSocket is the same as CLinuxServerSocket
                // So you can get address either from container or session
                conn_info.mSelfAddress = mConn.mSelfAddress;
            }
            else
            {
                CBaseSocketFactory::buildUrl(conn_info.mSelfAddress.mUrl,
                                             conn_info.mSelfAddress.mAddr.c_str(),
                                             conn_info.mSelfAddress.mPort);
            }
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

CUDPTransportSocket::CUDPTransportSocket(sckt::UDPSocket *imp)
    : mSocketImp(imp)
{
    mConn.mSelfAddress.mType = FDB_SOCKET_UDP;
}

CUDPTransportSocket::~CUDPTransportSocket()
{
    if (mSocketImp)
    {
        delete mSocketImp;
    }
}

int32_t CUDPTransportSocket::send(const uint8_t *data, int32_t size, const CFdbSocketAddr &dest_addr)
{
    int32_t ret = -1;
    if (mSocketImp)
    {
        try
        {
            sckt::IPAddress address(dest_addr.mAddr.c_str(), (sckt::u16)dest_addr.mPort);
            ret = mSocketImp->Send(data, size, address);
        }
        catch(...)
        {
            ret = -1;
        }
    }
    return ret;
}

int32_t CUDPTransportSocket::recv(uint8_t *data, int32_t size)
{
    int32_t ret = -1;
    if (mSocketImp)
    {
        try
        {
            sckt::IPAddress sender_ip;
            ret = mSocketImp->Recv(data, size, sender_ip);
        }
        catch(...)
        {
            ret = -1;
        }
    }
    return ret;
}

int CUDPTransportSocket::getFd()
{
    if (mSocketImp)
    {
        return mSocketImp->getNativeSocket();
    }
    return -1;
}

CLinuxUDPSocket::CLinuxUDPSocket(CFdbSocketAddr &addr)
    : CUDPSocketImp(addr)
{
}

CSocketImp *CLinuxUDPSocket::bind()
{
    CSocketImp *ret = 0;
    try
    {
        sckt::UDPSocket *sckt_imp = 0;
        if (mConn.mSelfAddress.mType == FDB_SOCKET_UDP)
        {
            if (mConn.mSelfAddress.mAddr.empty())
            {
                mConn.mSelfAddress.mAddr = "127.0.0.1";
            }
            sckt::IPAddress address(mConn.mSelfAddress.mAddr.c_str(), (sckt::u16)mConn.mSelfAddress.mPort);
            sckt_imp = new sckt::UDPSocket();
            sckt_imp->Open(address);
        }

        if (sckt_imp)
        {
            ret = new CUDPTransportSocket(sckt_imp);
            CFdbSocketConnInfo &conn_info = const_cast<CFdbSocketConnInfo &>(ret->getConnectionInfo());
            // For UDP socket, address of CUDPTransportSocket is the same as CLinuxUDPSocket
            // So you can get address either from container or session
            conn_info.mSelfAddress = mConn.mSelfAddress;
        }
    }
    catch (...)
    {
        ret = 0;
    }
    return ret;
}

