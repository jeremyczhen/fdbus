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

#include <common_base/CEventSubscribeHandle.h>
#include <common_base/CFdbSession.h>
#include <common_base/CFdbMessage.h>

void CEventSubscribeHandle::subscribe(CFdbSession *session,
                               FdbMsgCode_t msg,
                               FdbObjectId_t obj_id,
                               const char *filter,
                               CFdbSubscribeType type)
{
    if (!filter)
    {
        filter = "";
    }
    auto &subitem = mEventSubscribeTable[msg][session][obj_id][filter];
    subitem.mType = type;
}

void CEventSubscribeHandle::unsubscribe(CFdbSession *session,
                                 FdbMsgCode_t msg,
                                 FdbObjectId_t obj_id,
                                 const char *filter)
{
    SubscribeTable_t &subscribe_table = mEventSubscribeTable;
    
    auto it_sessions = subscribe_table.find(msg);
    if (it_sessions != subscribe_table.end())
    {
        auto &sessions = it_sessions->second;
        auto it_objects = sessions.find(session);
        if (it_objects != sessions.end())
        {
            auto &objects = it_objects->second;
            auto it_subitems = objects.find(obj_id);
            if (it_subitems != objects.end())
            {
                if (filter)
                {
                    auto &subitems = it_subitems->second;
                    auto it_subitem = subitems.find(filter);
                    if (it_subitem != subitems.end())
                    {
                        subitems.erase(it_subitem);
                    }
                    if (subitems.empty())
                    {
                        objects.erase(it_subitems);
                    }
                }
                else
                {
                    objects.erase(it_subitems);
                }
            }
            if (objects.empty())
            {
                sessions.erase(it_objects);
            }
        }
        if (sessions.empty())
        {
            subscribe_table.erase(it_sessions);
        }
    }
}

void CEventSubscribeHandle::unsubscribe(CFdbSession *session)
{
    SubscribeTable_t &subscribe_table = mEventSubscribeTable;

    for (auto it_sessions = subscribe_table.begin();
            it_sessions != subscribe_table.end();)
    {
        auto the_it_sessions = it_sessions;
        ++it_sessions;

        auto &sessions = the_it_sessions->second;
        auto it_objects = sessions.find(session);
        if (it_objects != sessions.end())
        {
            sessions.erase(it_objects);
        }
        if (sessions.empty())
        {
            subscribe_table.erase(the_it_sessions);
        }
    }
}

void CEventSubscribeHandle::unsubscribe(FdbObjectId_t obj_id)
{
    SubscribeTable_t &subscribe_table = mEventSubscribeTable;

    for (auto it_sessions = subscribe_table.begin();
            it_sessions != subscribe_table.end();)
    {
        auto the_it_sessions = it_sessions;
        ++it_sessions;

        auto &sessions = the_it_sessions->second;
        for (auto it_objects = sessions.begin();
                it_objects != sessions.end();)
        {
            auto the_it_objects = it_objects;
            ++it_objects;

            auto &objects = the_it_objects->second;
            auto it_subitems = objects.find(obj_id);
            if (it_subitems != objects.end())
            {
                objects.erase(it_subitems);
            }
            if (objects.empty())
            {
                sessions.erase(the_it_objects);
            }
        }
        if (sessions.empty())
        {
            subscribe_table.erase(the_it_sessions);
        }
    }
}

void CEventSubscribeHandle::broadcastOneMsg(CFdbSession *session,
                                     CFdbMessage *msg,
                                     CSubscribeItem &sub_item)
{
    if ((sub_item.mType == FDB_SUB_TYPE_NORMAL) || msg->manualUpdate())
    {
        if (!msg->preferUDP() || !session->sendUDPMessage(msg))
        {
            session->sendMessage(msg);
        }
    }
}

void CEventSubscribeHandle::broadcast(CFdbMessage *msg, FdbMsgCode_t event)
{
    SubscribeTable_t &subscribe_table = mEventSubscribeTable;

    auto it_sessions = subscribe_table.find(event);
    if (it_sessions != subscribe_table.end())
    {
        auto filter = msg->topic().c_str();
        auto &sessions = it_sessions->second;
        for (auto it_objects = sessions.begin();
                it_objects != sessions.end(); ++it_objects)
        {
            auto session = it_objects->first;
            auto &objects = it_objects->second;
            for (auto it_subitems = objects.begin();
                    it_subitems != objects.end(); ++it_subitems)
            {
                auto object_id = it_subitems->first;
                msg->updateObjectId(object_id); // send to the specific object.
                auto &subitems = it_subitems->second;
                auto it_subitem = subitems.find(filter);
                if (it_subitem != subitems.end())
                {
                    broadcastOneMsg(session, msg, it_subitem->second);
                }
                /*
                 * If filter doesn't match, check who registers filter "".
                 * It represents any filter.
                 */
                if (filter[0] != '\0')
                {
                    auto it_subitem = subitems.find("");
                    if (it_subitem != subitems.end())
                    {
                        broadcastOneMsg(session, msg, it_subitem->second);
                    }
                }
            }
        }
    }
}

bool CEventSubscribeHandle::broadcast(CFdbMessage *msg, CFdbSession *session, FdbMsgCode_t event)
{
    SubscribeTable_t &subscribe_table = mEventSubscribeTable;

    bool sent = false;
    auto it_sessions = subscribe_table.find(event);
    if (it_sessions != subscribe_table.end())
    {
        auto &sessions = it_sessions->second;
        auto it_objects = sessions.find(session);
        if (it_objects != sessions.end())
        {
            auto &objects = it_objects->second;
            auto it_subitems = objects.find(msg->objectId());
            if (it_subitems != objects.end())
            {
                auto filter = msg->topic().c_str();
                auto &subitems = it_subitems->second;
                auto it_subitem = subitems.find(filter);
                if (it_subitem != subitems.end())
                {
                    broadcastOneMsg(session, msg, it_subitem->second);
                    sent = true;
                }
                else if (filter[0] != '\0')
                {
                    auto it_subitem = subitems.find("");
                    if (it_subitem != subitems.end())
                    {
                        broadcastOneMsg(session, msg, it_subitem->second);
                        sent = true;
                    }
                }
            }
        }
    }
    return sent;
}

void CEventSubscribeHandle::getSubscribeTable(SessionTable_t &sessions, tFdbFilterSets &filter_tbl)
{
    for (auto it_objects = sessions.begin(); it_objects != sessions.end(); ++it_objects)
    {
        auto &objects = it_objects->second;
        for (auto it_subitems = objects.begin(); it_subitems != objects.end(); ++it_subitems)
        {
            auto &subitems = it_subitems->second;
            for (auto it_subitem = subitems.begin(); it_subitem != subitems.end(); ++it_subitem)
            {
                auto &subitem = it_subitem->second;
                if (subitem.mType == FDB_SUB_TYPE_NORMAL)
                {
                    filter_tbl.insert(it_subitem->first);
                }
            }
        }
    }
}

void CEventSubscribeHandle::getSubscribeTable(tFdbSubscribeMsgTbl &table)
{
    SubscribeTable_t &subscribe_table = mEventSubscribeTable;

    for (auto it_sessions = subscribe_table.begin();
            it_sessions != subscribe_table.end(); ++it_sessions)
    {
        auto &filter_table = table[it_sessions->first];
        auto &sessions = it_sessions->second;
        getSubscribeTable(sessions, filter_table);
    }
}

void CEventSubscribeHandle::getSubscribeTable(FdbMsgCode_t code, tFdbFilterSets &filters)
{
    SubscribeTable_t &subscribe_table = mEventSubscribeTable;

    auto it_sessions = subscribe_table.find(code);
    if (it_sessions != subscribe_table.end())
    {
        auto &sessions = it_sessions->second;
        getSubscribeTable(sessions, filters);
    }
}

void CEventSubscribeHandle::getSubscribeTable(FdbMsgCode_t code, CFdbSession *session,
                                              tFdbFilterSets &filter_tbl)
{
    SubscribeTable_t &subscribe_table = mEventSubscribeTable;

    auto it_sessions = subscribe_table.find(code);
    if (it_sessions != subscribe_table.end())
    {
        auto &sessions = it_sessions->second;
        auto it_objects = sessions.find(session);
        if (it_objects != sessions.end())
        {
            auto &objects = it_objects->second;
            for (auto it_subitems = objects.begin();
                    it_subitems != objects.end(); ++it_subitems)
            {
                auto &subitems = it_subitems->second;
                for (auto it_subitem = subitems.begin(); it_subitem != subitems.end(); ++it_subitem)
                {
                    auto &subitem = it_subitem->second;
                    if (subitem.mType == FDB_SUB_TYPE_NORMAL)
                    {
                        filter_tbl.insert(it_subitem->first);
                    }
                }
            }
        }
    }
}

void CEventSubscribeHandle::getSubscribeTable(FdbMsgCode_t code, const char *filter,
                                              tSubscribedSessionSets &session_tbl)
{
    SubscribeTable_t &subscribe_table = mEventSubscribeTable;

    auto it_sessions = subscribe_table.find(code);
    if (it_sessions != subscribe_table.end())
    {
        if (!filter)
        {
            filter = "";
        }
        auto &sessions = it_sessions->second;
        for (auto it_objects = sessions.begin();
                it_objects != sessions.end(); ++it_objects)
        {
            auto session = it_objects->first;
            auto &objects = it_objects->second;
            for (auto it_subitems = objects.begin();
                    it_subitems != objects.end(); ++it_subitems)
            {
                auto &subitems = it_subitems->second;
                auto it_subitem = subitems.find(filter);
                if (it_subitem == subitems.end())
                {
                    /*
                     * If filter doesn't match, check who registers filter "".
                     * It represents any filter.
                     */
                    if (filter[0] != '\0')
                    {
                        auto it_subitem = subitems.find("");
                        if (it_subitem != subitems.end())
                        {
                            auto &subitem = it_subitem->second;
                            if (subitem.mType == FDB_SUB_TYPE_NORMAL)
                            {
                                session_tbl.insert(session);
                                break;
                            }
                        }
                    }
                }
                else
                {
                    auto &subitem = it_subitem->second;
                    if (subitem.mType == FDB_SUB_TYPE_NORMAL)
                    {
                        session_tbl.insert(session);
                        break;
                    }
                }
            }
        }
    }
}

