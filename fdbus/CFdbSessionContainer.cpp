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

#include <common_base/CFdbSessionContainer.h>
#include <common_base/CFdbContext.h>
#include <common_base/CBaseEndpoint.h>
#include <common_base/CBaseSocketFactory.h>
#include <utils/CFdbUDPSession.h>
#include <common_base/CFdbSession.h>
#include <utils/Log.h>
#include <algorithm>

CFdbSessionContainer::CFdbSessionContainer(FdbSocketId_t skid , CBaseEndpoint *owner,
                                           CBaseSocket *tcp_socket, int32_t udp_port)
    : mSkid(skid)
    , mOwner(owner)
    , mSocket(tcp_socket)
    , mEnableSessionDestroyHook(true)
    , mUDPSocket(0)
    , mUDPSession(0)
    , mPendingUDPPort(udp_port)
{
}

CFdbSessionContainer::~CFdbSessionContainer()
{
    mOwner->context()->deleteSession(this);
    if (!mConnectedSessionTable.empty())
    {
        LOG_E("CFdbSessionContainer: Untracked sessions are found!!!\n");
    }

    CFdbSessionContainer *self = 0;
    auto it = mOwner->retrieveEntry(mSkid, self);
    if (self)
    {
        mOwner->deleteEntry(it);
    }
    if (mUDPSession)
    {
        delete mUDPSession;
        mUDPSession = 0;
    }
    if (mUDPSocket)
    {
        delete mUDPSocket;
        mUDPSocket = 0;
    }
    if (mSocket)
    {
        delete mSocket;
        mSocket = 0;
    }
}

CFdbSession *CFdbSessionContainer::getDefaultSession()
{
    auto it = mConnectedSessionTable.begin();
    return (it == mConnectedSessionTable.end()) ? 0 : *it;
}

void CFdbSessionContainer::addSession(CFdbSession *session)
{
    for (auto it = mConnectedSessionTable.begin();
            it != mConnectedSessionTable.end(); ++it)
    {
        if (*it == session)
        {
            return;
        }
    }

    mConnectedSessionTable.push_back(session);
}

void CFdbSessionContainer::removeSession(CFdbSession *session)
{
    fdb_remove_value_from_container(mConnectedSessionTable, session);
}

void CFdbSessionContainer::callSessionDestroyHook(CFdbSession *session)
{
    if (mEnableSessionDestroyHook)
    {
        onSessionDeleted(session);
    }
}

bool CFdbSessionContainer::getSocketInfo(CFdbSocketInfo &info)
{
    info.mAddress = &mSocket->getAddress();
    return true;
}

bool CFdbSessionContainer::bindUDPSocket(const char *ip_address, int32_t udp_port)
{
    if (!mOwner->UDPEnabled())
    {
        return false;
    }
    if (udp_port == FDB_INET_PORT_INVALID)
    {
        udp_port = mPendingUDPPort;
    }
    if (!mSocket || (udp_port == FDB_INET_PORT_INVALID))
    {
        return false;
    }
    const CFdbSocketAddr &tcp_addr = mSocket->getAddress();
    if (tcp_addr.mType == FDB_SOCKET_IPC)
    {
        return false;
    }

    if (mUDPSocket)
    {
        const CFdbSocketAddr &udp_addr = mUDPSocket->getAddress();
        if (udp_addr.mPort == udp_port)
        {
            return true;
        }
        if (mUDPSession)
        {
            delete mUDPSession;
            mUDPSession = 0;
        }
        delete mUDPSocket;
        mUDPSocket = 0;
    }

    CFdbSocketAddr udp_addr;
    if (ip_address)
    {
        udp_addr.mAddr = ip_address;
    }
    else
    {
        udp_addr.mAddr = tcp_addr.mAddr;
    }
    udp_addr.mType = FDB_SOCKET_UDP;
    udp_addr.mPort = udp_port;
    auto udp_socket = CBaseSocketFactory::createUDPSocket(udp_addr);
    if (udp_socket)
    {
        CSocketImp *socket_imp = 0;
        int32_t retries = FDB_ADDRESS_BIND_RETRY_NR;
        do {
            socket_imp = udp_socket->bind();
            if (socket_imp)
            {
                break;
            }

            sysdep_sleep(FDB_ADDRESS_BIND_RETRY_INTERVAL);
        } while (--retries > 0);

        if (socket_imp)
        {
            mUDPSocket = udp_socket;
            mUDPSession = new CFdbUDPSession(this, socket_imp);
            mUDPSession->attach(mOwner->context());

            const CFdbSocketAddr &newly_addr = socket_imp->getAddress();
            mPendingUDPPort = newly_addr.mPort;
            return true;
        }
        else
        {
            LOG_E("CFdbSessionContainer: fail to bind socket: udp://%s:%d\n",
                    udp_addr.mAddr.c_str(), udp_addr.mPort);
            delete udp_socket;
        }
    }
    return false;
}

bool CFdbSessionContainer::sendUDPmessage(CFdbMessage *msg, const CFdbSocketAddr &dest_addr)
{
    return mUDPSession ? mUDPSession->sendMessage(msg, dest_addr) : false;
}

bool CFdbSessionContainer::getUDPSocketInfo(CFdbSocketInfo &info)
{
    if (mUDPSession && mUDPSession->getSocket())
    {
        // can get from session as well
        info.mAddress = &mUDPSession->getSocket()->getAddress();
        return true;
    }
    return false;
}

CFdbSession *CFdbSessionContainer::connected(const CFdbSocketAddr &addr)
{
    for (auto it = mConnectedSessionTable.begin();
            it != mConnectedSessionTable.end(); ++it)
    {
        auto session = *it;
        if (session->connected(addr))
        {
            return session;
        }
    }
    return 0;
}

CFdbSession *CFdbSessionContainer::bound(const CFdbSocketAddr &addr)
{
    for (auto it = mConnectedSessionTable.begin();
            it != mConnectedSessionTable.end(); ++it)
    {
        auto session = *it;
        if (session->bound(addr))
        {
            return session;
        }
    }
    return 0;
}

