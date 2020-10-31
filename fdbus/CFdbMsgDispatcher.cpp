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

#include <algorithm>
#include <utils/Log.h>
#include <common_base/CFdbMsgDispatcher.h>

bool CFdbMsgDispatcher::registerCallback(const CMsgHandleTbl &msg_tbl)
{
    bool ret = true;
    for (auto it = msg_tbl.mTable.begin(); it != msg_tbl.mTable.end(); ++it)
    {
        auto code = it->mCode;
        if (mRegistryTbl.find(code) != mRegistryTbl.end())
        {
            LOG_E("CFdbMsgDispatcher: handle for method %d is aready registered.\n", code);
            ret = false;
            continue;
        }
        mRegistryTbl[code] = *it;
    }
    return ret;
}

bool CFdbMsgDispatcher::processMessage(CBaseJob::Ptr &msg_ref, CFdbBaseObject *obj)
{
    CFdbMessage *msg = castToMessage<CFdbMessage *>(msg_ref);
    tRegistryTbl::iterator it = mRegistryTbl.find(msg->code());
    if (it == mRegistryTbl.end())
    {
        return false;
    }
<<<<<<< HEAD
    fdbMigrateCallback(msg_ref, msg, it->second.mCallback, it->second.mWorker, obj);
    return true;
}

bool CFdbMsgDispatcher::CMsgHandleTbl::add(FdbMsgCode_t code, tDispatcherCallbackFn callback,
=======
    (it->second)(msg_ref, obj);
    return true;
}

bool CFdbMsgDispatcher::CMsgHandleTbl::add(FdbMsgCode_t code,
                                CFdbMsgDispatcher::tMsgCallbackFn callback,
>>>>>>> 20359d5abe63437dcfb1b9a4008bd293a0998b63
                                CBaseWorker *worker)
{
    if (!callback)
    {
        return false;
    }
    mTable.resize(mTable.size() + 1);
    auto &item = mTable.back();
    item.mCode = code;
    item.mCallback = callback;
    item.mWorker = worker;
    return true;
}

void CFdbEventDispatcher::registerCallback(const CEvtHandleTbl &evt_tbl,
                                           tRegistryHandleTbl *registered_evt_tbl)
{
    for (auto it = evt_tbl.mTable.begin(); it != evt_tbl.mTable.end(); ++it)
    {
        auto code = it->mCode;
        auto &topic = it->mTopic;
        CFdbEventDispatcher::tRegEntryId id = mRegIdAllocator++;
        mRegistryTbl[code][topic][id] = *it;
        if (registered_evt_tbl)
        {
            registered_evt_tbl->push_back(id);
        }
    }
}

bool CFdbEventDispatcher::processMessage(CBaseJob::Ptr &msg_ref, CFdbBaseObject *obj,
                                         const tRegistryHandleTbl *registered_evt_tbl)
{
    CFdbMessage *msg = castToMessage<CFdbMessage *>(msg_ref);
    auto it_topics = mRegistryTbl.find(msg->code());
    if (it_topics != mRegistryTbl.end())
    {
        auto &topics = it_topics->second;
        auto it_callbacks = topics.find(msg->topic());
        if (it_callbacks != topics.end())
        {
            tEvtHandlePtrTbl handles_to_invoke;
            auto &callbacks = it_callbacks->second;
            if (registered_evt_tbl)
            {
                for (auto it_callback = callbacks.begin(); it_callback != callbacks.end(); ++it_callback)
                {
                    auto reg_id = it_callback->first;
                    auto it_reg_id = std::find(registered_evt_tbl->begin(), registered_evt_tbl->end(), reg_id);
                    if (it_reg_id != registered_evt_tbl->end())
                    {
<<<<<<< HEAD
                        handles_to_invoke.push_back(&(it_callback->second));
=======
                        (it_callback->second)(msg_ref, obj);
>>>>>>> 20359d5abe63437dcfb1b9a4008bd293a0998b63
                    }
                }
            }
            else
            {
                for (auto it_callback = callbacks.begin(); it_callback != callbacks.end(); ++it_callback)
                {
<<<<<<< HEAD
                    handles_to_invoke.push_back(&(it_callback->second));
                }
            }
            if (handles_to_invoke.size() < 1)
            {
                return true;
            }

            auto handle = handles_to_invoke.front();
            if (handles_to_invoke.size() == 1)
            {
                fdbMigrateCallback(msg_ref, msg, handle->mCallback, handle->mWorker, obj);
                return true;
            }

            auto next_msg = new CFdbMessage(msg);
            CFdbMessage *cur_msg;
            fdbMigrateCallback(msg_ref, msg, handle->mCallback, handle->mWorker, obj);
            for (auto it_callback = handles_to_invoke.begin() + 1; it_callback != handles_to_invoke.end();)
            {
                cur_msg = next_msg;
                auto cur_it = it_callback++;
                if (it_callback != handles_to_invoke.end())
                {
                    next_msg = new CFdbMessage(msg);
=======
                    (it_callback->second)(msg_ref, obj);
>>>>>>> 20359d5abe63437dcfb1b9a4008bd293a0998b63
                }
                CBaseJob::Ptr cur_msg_ref(cur_msg);
                fdbMigrateCallback(cur_msg_ref, cur_msg, (*cur_it)->mCallback, (*cur_it)->mWorker, obj);
            }
        }
    }
    return true;
}

void CFdbEventDispatcher::dumpEvents(tEvtHandleTbl &event_table)
{
    for (auto it_topics = mRegistryTbl.begin(); it_topics != mRegistryTbl.end(); ++it_topics)
    {
        FdbMsgCode_t code = it_topics->first;
        auto &topics = it_topics->second;
        for (auto it_callbacks = topics.begin(); it_callbacks != topics.end(); ++it_callbacks)
        {
            const char *topic = it_callbacks->first.c_str();
            event_table.resize(event_table.size() + 1);
            auto &item = event_table.back();
            item.mCode = code;
            item.mTopic = topic;
        }
    }
}

<<<<<<< HEAD
bool CFdbEventDispatcher::CEvtHandleTbl::add(FdbMsgCode_t code, tDispatcherCallbackFn callback,
=======
bool CFdbEventDispatcher::CEvtHandleTbl::add(FdbMsgCode_t code,
                                             CFdbEventDispatcher::tEvtCallbackFn callback,
>>>>>>> 20359d5abe63437dcfb1b9a4008bd293a0998b63
                                             CBaseWorker *worker, const char *topic)
{
    if (!callback)
    {
        return false;
    }
    mTable.resize(mTable.size() + 1);
    auto &item = mTable.back();
    item.mCode = code;
    item.mCallback = callback;
    if (topic)
    {
        item.mTopic = topic;
    }
    item.mWorker = worker;
    return true;
}

