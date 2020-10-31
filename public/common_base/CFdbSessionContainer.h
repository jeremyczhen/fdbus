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

#ifndef _CFDBSESSIONCONTAINER_H_
#define _CFDBSESSIONCONTAINER_H_

#include <string>
#include <list>
#include "common_defs.h"
#include "CSocketImp.h"

class CFdbSession;
class CBaseEndpoint;
class CFdbUDPSession;
class CFdbMessage;
struct CFdbSocketInfo
{
    CFdbSocketAddr const *mAddress;
};

class CFdbSessionContainer
{
public:
    CFdbSessionContainer(FdbSocketId_t skid, CBaseEndpoint *owner, CBaseSocket *tcp_socket,
                         int32_t udp_port = FDB_INET_PORT_INVALID);
    virtual ~CFdbSessionContainer();
    FdbSocketId_t skid()
    {
        return mSkid;
    }

    bool getSocketInfo(CFdbSocketInfo &info);

    CBaseEndpoint *owner()
    {
        return mOwner;
    }
    CFdbSession *getDefaultSession();

    void enableSessionDestroyHook(bool enable)
    {
        mEnableSessionDestroyHook = enable;
    }

    bool bindUDPSocket(const char *ip_address = 0, int32_t udp_port = FDB_INET_PORT_INVALID);
    bool sendUDPmessage(CFdbMessage *msg, const CFdbSocketAddr &dest_addr);
    bool getUDPSocketInfo(CFdbSocketInfo &info);

    CFdbSession *connected(const CFdbSocketAddr &addr);
    CFdbSession *bound(const CFdbSocketAddr &addr);

    void pendingUDPPort(int32_t udp_port)
    {
        mPendingUDPPort = udp_port;
    }
protected:
    FdbSocketId_t mSkid;
    virtual void onSessionDeleted(CFdbSession *session) {}
    CBaseEndpoint *mOwner;
    CBaseSocket *mSocket;
private:
    typedef std::list<CFdbSession *> ConnectedSessionTable_t;
    bool mEnableSessionDestroyHook;
    CBaseSocket *mUDPSocket;
    CFdbUDPSession *mUDPSession;
    int32_t mPendingUDPPort;

    ConnectedSessionTable_t mConnectedSessionTable;

    void addSession(CFdbSession *session);
    void removeSession(CFdbSession *session);
    void callSessionDestroyHook(CFdbSession *session);

    friend class CFdbSession;
    friend class CBaseEndpoint;
    friend class CFdbUDPSession;
    friend class CBaseServer;
    friend class CBaseClient;
    friend class CFdbBaseObject;
};

#endif
