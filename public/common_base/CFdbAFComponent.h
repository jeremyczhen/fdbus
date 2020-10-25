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

#ifndef __CFDBAFCOMPONENT_H__
#define __CFDBAFCOMPONENT_H__

#include <string>
#include <vector>
#include "CFdbMsgDispatcher.h"
#include "CBaseJob.h"
#include "CFdbBaseObject.h"

class CBaseClient;
class CBaseServer;

class CFdbAFComponent
{
public:
    CFdbAFComponent(const char *name = "anonymous");

    // create FDBus client, connect to server and register callbacks
    // If the client is already created (connected), do not create (or connect) again
    // @bus_name - FDBus server name to be connected
    // @evt_tbl - list of callbacks called when associated event:topic is received
    // @connect_callback - callback called when server is connected/disconnected
    CBaseClient *queryService(const char *bus_name,
                               const CFdbEventDispatcher::CEvtHandleTbl &evt_tbl,
                               CFdbBaseObject::tConnCallbackFn connect_callback = 0);

    // create FDBus server, bind bus name, and register callbacks
    // If the server is already created (bound), do not create (or bind) again
    // @bus_name - FDBus server name to be bound
    // @msg_tbl - list of callbacks called when associated method is invoked
    // @connect_callback - callback called when client is connected/disconnected
    CBaseServer *offerService(const char *bus_name,
                               const CFdbMsgDispatcher::CMsgHandleTbl &msg_tbl,
                               CFdbBaseObject::tConnCallbackFn connect_callback = 0);

    CFdbEventDispatcher::tRegistryHandleTbl &getEventRegistryTbl()
    {
        return mEventRegistryTbl;
    }
    const std::string &name() const
    {
        return mName;
    }
    void name(const char *name)
    {
        if (name)
        {
            mName = name;
        }
    }

protected:
    std::string mName;
private:
    typedef std::vector<CFdbBaseObject::tRegEntryId> tConnHandleTbl;
    CFdbEventDispatcher::tRegistryHandleTbl mEventRegistryTbl;
    tConnHandleTbl mConnHandleTbl;

    void callQueryService(CBaseWorker *worker, CMethodJob<CFdbAFComponent> *job, CBaseJob::Ptr &ref);
    void callOfferService(CBaseWorker *worker, CMethodJob<CFdbAFComponent> *job, CBaseJob::Ptr &ref);

    friend class COfferServiceJob;
    friend class CQueryServiceJob;
};

#endif
