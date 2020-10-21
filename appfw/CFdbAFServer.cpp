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

#include <common_base/CFdbAFServer.h>

CFdbAFServer::tRegEntryId CFdbAFServer::registerConnNotification(tConnCallbackFn callback)
{
    CFdbAFServer::tRegEntryId id = mRegIdAllocator++;
    mConnCallbackTbl[id] =callback;
    if (connected())
    {
        callback(this, FDB_INVALID_ID, true);
    }
    return id;
}
bool CFdbAFServer::registerMsgHandle(const CFdbMsgDispatcher::CMsgHandleTbl &msg_tbl)
{
    return mMsgDispather.registerCallback(msg_tbl);
}

void CFdbAFServer::onOnline(FdbSessionId_t sid, bool is_first)
{
    for (auto it = mConnCallbackTbl.begin(); it != mConnCallbackTbl.end(); ++it)
    {
        (it->second)(this, sid, true);
    }
}
void CFdbAFServer::onOffline(FdbSessionId_t sid, bool is_last)
{
    for (auto it = mConnCallbackTbl.begin(); it != mConnCallbackTbl.end(); ++it)
    {
        (it->second)(this, sid, false);
    }
}
void CFdbAFServer::onInvoke(CBaseJob::Ptr &msg_ref)
{
    mMsgDispather.processMessage(msg_ref, this);
}

