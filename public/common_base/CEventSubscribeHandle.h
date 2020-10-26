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

#ifndef __CEVENTSUBSCRIBEHANDLE_H__
#define __CEVENTSUBSCRIBEHANDLE_H__

#include <map>
#include <set>
#include <string>
#include "common_defs.h"

class CFdbSession;
class CFdbMessage;

enum CFdbSubscribeType {
  FDB_SUB_TYPE_NORMAL = 0,
  FDB_SUB_TYPE_ON_REQUEST = 1
};

typedef std::set<std::string> tFdbFilterSets;
typedef std::map<FdbMsgCode_t, tFdbFilterSets> tFdbSubscribeMsgTbl;
typedef std::set<CFdbSession *> tSubscribedSessionSets;

class CEventSubscribeHandle
{
public:
    struct CSubscribeItem
    {
        CFdbSubscribeType mType;
    };
    typedef std::map<std::string, CSubscribeItem> SubItemTable_t;
    typedef std::map<FdbObjectId_t, SubItemTable_t> ObjectTable_t;
    typedef std::map<CFdbSession *, ObjectTable_t> SessionTable_t;
    typedef std::map<FdbMsgCode_t, SessionTable_t> SubscribeTable_t;

    void subscribe(CFdbSession *session, FdbMsgCode_t msg, FdbObjectId_t obj_id,
                   const char *filter, CFdbSubscribeType type);
    void unsubscribe(CFdbSession *session, FdbMsgCode_t msg, FdbObjectId_t obj_id,
                     const char *filter);
    void unsubscribe(CFdbSession *session);
    void unsubscribe(FdbObjectId_t obj_id);
    void broadcast(CFdbMessage *msg, FdbMsgCode_t event);
    bool broadcast(CFdbMessage *msg, CFdbSession *session, FdbMsgCode_t event);
    void getSubscribeTable(SessionTable_t &sessions, tFdbFilterSets &filter_tbl);
    void getSubscribeTable(tFdbSubscribeMsgTbl &table);
    void getSubscribeTable(FdbMsgCode_t code, tFdbFilterSets &filters);
    void getSubscribeTable(FdbMsgCode_t code, CFdbSession *session,
                           tFdbFilterSets &filter_tbl);
    void getSubscribeTable(FdbMsgCode_t code, const char *filter,
                           tSubscribedSessionSets &session_tbl);
private:
    SubscribeTable_t mEventSubscribeTable;
    void broadcastOneMsg(CFdbSession *session, CFdbMessage *msg,
                         CSubscribeItem &sub_item);
};

#endif

