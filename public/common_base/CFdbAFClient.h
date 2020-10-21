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

#ifndef __CFDBAFCLIENT_H__
#define __CFDBAFCLIENT_H__
#include "CBaseClient.h"
#include "CFdbMsgDispatcher.h"
#include "common_defs.h"
#include "CBaseJob.h"

#include <map>
#include <functional>

class CFdbAFComponent;

class CFdbAFClient : public CBaseClient
{
public:
    typedef uint32_t tRegEntryId;
    typedef std::function<void(CFdbAFClient *, bool online)> tConnCallbackFn;
    typedef std::function<void(CBaseJob::Ptr &, CFdbAFClient *)> tInvokeCallbackFn;
    typedef std::map<tRegEntryId, tConnCallbackFn> tConnCallbackTbl;

    CFdbAFClient(const char *name, CBaseWorker *worker)
        : CBaseClient(name, worker)
        , mRegIdAllocator(0)
    {
    }

    // APPFW version of async invoke: it is similiar to CFdbBaseObject::invoke[2.1] except
    // that it accepts a callback which will be triggered when server replies
    bool invoke(FdbMsgCode_t code
                , IFdbMsgBuilder &data
                , tInvokeCallbackFn callback
                , int32_t timeout = 0);
    // APPFW version of async invoke: it is similiar to CFdbBaseObject::invoke[6] except
    // that it accepts a callback which will be triggered when server replies
    bool invoke(FdbMsgCode_t code
                , tInvokeCallbackFn callback
                , const void *buffer = 0
                , int32_t size = 0
                , int32_t timeout = 0
                , const char *log_info = 0);

protected:
    void onOnline(FdbSessionId_t sid, bool is_first);
    void onOffline(FdbSessionId_t sid, bool is_last);
    void onBroadcast(CBaseJob::Ptr &msg_ref);
    void onReply(CBaseJob::Ptr &msg_ref);

private:
    CFdbEventDispatcher mEvtDispather;
    tConnCallbackTbl mConnCallbackTbl;
    tRegEntryId mRegIdAllocator;

    tRegEntryId registerConnNotification(tConnCallbackFn callback);
    bool registerEventHandle(const CFdbEventDispatcher::CEvtHandleTbl &evt_tbl,
                             CFdbAFComponent *component);
    bool subscribeEvents(const CFdbEventDispatcher::tEvtHandleTbl &events,
                         CFdbAFComponent *component);

friend class CFdbAFComponent;
};

#endif
