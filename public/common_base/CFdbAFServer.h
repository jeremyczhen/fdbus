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

#ifndef __CFDBAFSERVER_H__
#define __CFDBAFSERVER_H__

#include "CBaseServer.h"
#include "CFdbMsgDispatcher.h"

class CFdbAFServer : public CBaseServer
{
public:
    typedef uint32_t tRegEntryId;
    typedef std::function<void(CFdbAFServer *, FdbSessionId_t, bool)> tConnCallbackFn;
    typedef std::map<tRegEntryId, tConnCallbackFn> tConnCallbackTbl;
    CFdbAFServer(const char *name, CBaseWorker *worker)
        : CBaseServer(name, worker)
        , mRegIdAllocator(0)
    {
        enableEventCache(true);
    }

protected:
    void onOnline(FdbSessionId_t sid, bool is_first);
    void onOffline(FdbSessionId_t sid, bool is_last);
    void onInvoke(CBaseJob::Ptr &msg_ref);

private:
    CFdbMsgDispatcher mMsgDispather;
    tConnCallbackTbl mConnCallbackTbl;
    tRegEntryId mRegIdAllocator;

    tRegEntryId registerConnNotification(tConnCallbackFn callback);
    bool registerMsgHandle(const CFdbMsgDispatcher::CMsgHandleTbl &msg_tbl);
friend class CFdbAFComponent;
};

#endif

