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
        auto &callback = it->mCallback;
        if (mRegistryTbl.find(code) != mRegistryTbl.end())
        {
            LOG_E("CFdbMsgDispatcher: handle for method %d is aready registered.\n", code);
            ret = false;
            continue;
        }
        mRegistryTbl[code] = callback;
    }
    return ret;
}

bool CFdbMsgDispatcher::processMessage(CBaseJob::Ptr &msg_ref, CFdbAFServer *server)
{
    CFdbMessage *msg = castToMessage<CFdbMessage *>(msg_ref);
    tRegistryTbl::iterator it = mRegistryTbl.find(msg->code());
    if (it == mRegistryTbl.end())
    {
        return false;
    }
    (it->second)(msg_ref, server);
    return true;
}

bool CFdbMsgDispatcher::CMsgHandleTbl::add(FdbMsgCode_t code,
                                CFdbMsgDispatcher::tMsgCallbackFn callback)
{
    mTable.resize(mTable.size() + 1);
    auto &item = mTable.back();
    item.mCode = code;
    item.mCallback = callback;
    return true;
}

void CFdbEventDispatcher::registerCallback(const CEvtHandleTbl &evt_tbl,
                                           tRegistryHandleTbl *registered_evt_tbl)
{
    for (auto it = evt_tbl.mTable.begin(); it != evt_tbl.mTable.end(); ++it)
    {
        auto code = it->mCode;
        auto &callback = it->mCallback;
        auto &topic = it->mTopic;
        CFdbEventDispatcher::tRegEntryId id = mRegIdAllocator++;
        mRegistryTbl[code][topic][id] = callback;
        if (registered_evt_tbl)
        {
            registered_evt_tbl->push_back(id);
        }
    }
}

bool CFdbEventDispatcher::processMessage(CBaseJob::Ptr &msg_ref, CFdbAFClient *client,
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
            auto &callbacks = it_callbacks->second;
            if (registered_evt_tbl)
            {
                for (auto it_callback = callbacks.begin(); it_callback != callbacks.end(); ++it_callback)
                {
                    auto reg_id = it_callback->first;
                    auto it_reg_id = std::find(registered_evt_tbl->begin(), registered_evt_tbl->end(), reg_id);
                    if (it_reg_id != registered_evt_tbl->end())
                    {
                        (it_callback->second)(msg_ref, client);
                    }
                }
            }
            else
            {
                for (auto it_callback = callbacks.begin(); it_callback != callbacks.end(); ++it_callback)
                {
                    (it_callback->second)(msg_ref, client);
                }
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

bool CFdbEventDispatcher::CEvtHandleTbl::add(FdbMsgCode_t code,
                        CFdbEventDispatcher::tEvtCallbackFn callback, const char *topic)
{

    mTable.resize(mTable.size() + 1);
    auto &item = mTable.back();
    item.mCode = code;
    item.mCallback = callback;
    if (topic)
    {
        item.mTopic = topic;
    }
    return true;
}
