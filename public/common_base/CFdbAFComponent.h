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
#include <map>
#include <functional>
#include "CFdbMsgDispatcher.h"
#include "CMethodJob.h"
#include "CFdbBaseObject.h"
#include "CBaseWorker.h"

class CBaseClient;
class CBaseServer;
class CBaseWorker;
class CFdbBaseContext;
class CBaseEndpoint;

class CFdbAFComponent
{
public:
    typedef std::function<void(CBaseEndpoint *)> tOnCreateFn;
    CBaseClient *registerClient(const char *bus_name, const char *endpoint_name = 0, tOnCreateFn on_create = 0);
    CBaseServer *registerServer(const char *bus_name, const char *endpoint_name = 0, tOnCreateFn on_create = 0);

    // base class of APP FW component
    // @name - name of the component for debug purpose
    // worker - the thread context where callback is executed; if not specified
    //     callback will be called at FDBus context thread
    CFdbAFComponent(const char *name = "anonymous", CBaseWorker *worker = 0, CFdbBaseContext *context = 0);
    virtual ~CFdbAFComponent()
    {}

    // create FDBus client, connect to server and register callbacks
    // If the client is already created (connected), do not create (or connect) again
    // @bus_name - FDBus server name to be connected
    // @evt_tbl - tabe of handles called when associated event:topic is received
    // @connect_callback - handle called when server is connected/disconnected
    CBaseClient *queryService(const char *bus_name,
                               const CFdbEventDispatcher::CEvtHandleTbl &evt_tbl,
                               CFdbBaseObject::tConnCallbackFn connect_callback = 0);

    // create FDBus server, bind bus name, and register callbacks
    // If the server is already created (bound), do not create (or bind) again
    // @bus_name - FDBus server name to be bound
    // @msg_tbl - table of handles called when associated method is invoked
    // @connect_callback - handle called when client is connected/disconnected
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

    CBaseWorker *worker() const
    {
        return mWorker;
    }

    // the method is called by server to register message handle
    // hdl_table - table of message handle
    // code - message code
    // callback  - handle called when invoke() is called by clients
    bool addMsgHandle(CFdbMsgDispatcher::CMsgHandleTbl &hdl_table, FdbMsgCode_t code,
                      tDispatcherCallbackFn callback);
    // the method is called by client to register event handle
    // hdl_table - table of message handle
    // code - message code
    // callback  - handle called when broadcast() is called by server 
    // topic - topic of the event
    bool addEvtHandle(CFdbEventDispatcher::CEvtHandleTbl &hdl_table, FdbMsgCode_t code,
                      tDispatcherCallbackFn callback, const char *topic = 0);

    // This method is wrapper of CFdbBaseObject::invoke[9] but the worker is taken
    // from mWorker()
    bool invoke(CFdbBaseObject *obj
                , FdbMsgCode_t code
                , IFdbMsgBuilder &data
                , CFdbBaseObject::tInvokeCallbackFn callback
                , int32_t timeout = 0);

    // This method is wrapper of CFdbBaseObject::invoke[10] but the worker is taken
    // from mWorker()
    bool invoke(CFdbBaseObject *obj
                , FdbMsgCode_t code
                , CFdbBaseObject::tInvokeCallbackFn callback
                , const void *buffer = 0
                , int32_t size = 0
                , int32_t timeout = 0
                , const char *log_info = 0);

protected:
    std::string mName;
    CBaseWorker *mWorker;
    virtual CBaseClient *createClient();
    virtual CBaseServer *createServer();

private:
    typedef std::map<std::string, CBaseClient *> tAFClientTbl;
    typedef std::map<std::string, CBaseServer *> tAFServerTbl;
    typedef std::vector<CFdbBaseObject::tRegEntryId> tConnHandleTbl;
    CFdbEventDispatcher::tRegistryHandleTbl mEventRegistryTbl;
    tConnHandleTbl mConnHandleTbl;
    CFdbBaseContext *mContext;

    tAFClientTbl mClientTbl;
    tAFServerTbl mServerTbl;

    CBaseClient *findClient(const char *bus_name);
    CBaseServer *findService(const char *bus_name);
    bool registerClient(const char *bus_name, CBaseClient *client);
    bool registerService(const char *bus_name, CBaseServer *server);
 
    void callRegisterEndpoint(CBaseWorker *worker, CMethodJob<CFdbAFComponent> *job, CBaseJob::Ptr &ref);
    void callQueryService(CBaseWorker *worker, CMethodJob<CFdbAFComponent> *job, CBaseJob::Ptr &ref);
    void callOfferService(CBaseWorker *worker, CMethodJob<CFdbAFComponent> *job, CBaseJob::Ptr &ref);

    friend class COfferServiceJob;
    friend class CQueryServiceJob;
    friend class CRegisterEndpointJob;
};

#endif
