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
struct CFdbSocketInfo
{
    CFdbSocketAddr const *mAddress;
};

class CBaseEndpoint;
class CFdbSessionContainer
{
public:
    CFdbSessionContainer(FdbSocketId_t skid, CBaseEndpoint *owner);
    virtual ~CFdbSessionContainer();
    FdbSocketId_t skid()
    {
        return mSkid;
    }

    virtual void getSocketInfo(CFdbSocketInfo &info)
    {
    }

    CBaseEndpoint *owner()
    {
        return mOwner;
    }
    CFdbSession *getDefaultSession();

    void enableSessionDestroyHook(bool enable)
    {
        mEnableSessionDestroyHook = enable;
    }
protected:
    FdbSocketId_t mSkid;
    virtual void onSessionDeleted(CFdbSession *session) {}
    CBaseEndpoint *mOwner;
private:
    bool mEnableSessionDestroyHook;
    typedef std::list<CFdbSession *> ConnectedSessionTable_t;

    ConnectedSessionTable_t mConnectedSessionTable;

    void addSession(CFdbSession *session);
    void removeSession(CFdbSession *session);
    void callSessionDestroyHook(CFdbSession *session);

    friend class CBaseEndpoint;
    friend class CFdbSession;
    friend class CBaseServer;
};

#endif
