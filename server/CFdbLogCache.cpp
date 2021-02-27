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

#include "CFdbLogCache.h"
#include <common_base/CFdbSession.h>
#include <common_base/CBaseEndpoint.h>
#include <common_base/CLogProducer.h>

CFdbLogCache::CDataItem::CDataItem(const uint8_t *log_data, int32_t size, FdbEventCode_t code, uint32_t id)
    : mLogData(((size > 0) && log_data) ? new uint8_t[size] : 0)
    , mSize(size)
    , mCode(code)
    , mId(id)
{
    if (mLogData && log_data)
    {
        memcpy(mLogData, log_data, size);
    }
}

CFdbLogCache::CDataItem::~CDataItem()
{
    if (mLogData)
    {
        delete[] mLogData;
    }
}

void CFdbLogCache::removeAll()
{
    while (!mCache.empty())
    {
        auto head = mCache.front();
        mCache.pop_front();
        delete head;
    }
    mFull = false;
    mCacheSize = 0;
}

CFdbLogCache::~CFdbLogCache()
{
    removeAll();
}

void CFdbLogCache::popOne()
{
    if (mCache.empty())
    {
        return;
    }
    auto head = mCache.front();
    mCacheSize -= head->mSize + sizeof(CDataItem);
    if (mCacheSize < 0)
    {
        removeAll();
    }

    if (mCache.empty())
    {
        mCacheSize = 0;
    }
    else
    {
        mCache.pop_front();
        delete head;
    }
}

void CFdbLogCache::push(const uint8_t *log_data, int32_t size, FdbEventCode_t code)
{
    if (!mMaxSize || (size <= 0))
    {
        return;
    }
    if (mStopIfFull)
    {
        if ((mCacheSize + size + (int32_t)sizeof(CDataItem)) > mMaxSize)
        {
            mFull = true;
            return;
        }
    }
    else
    {
        while ((mCacheSize + size + (int32_t)sizeof(CDataItem)) > mMaxSize)
        {
            mFull = true;
            popOne();
            if (mCache.empty())
            {
                break;
            }
        }
    }

    mCacheSize += size + sizeof(CDataItem);
    mCache.push_back(new CDataItem(log_data, size, code, mDataId++));
}

void CFdbLogCache::dump(CLogProducer *log_producer)
{
    while (!mCache.empty())
    {
        auto item = mCache.front();
        log_producer->sendLog(item->mCode, item->mLogData, item->mSize);
        mCache.pop_front();
        delete item;
    }
    mCacheSize = 0;
    mDataId = 0;
    mFull = 0;
}

void CFdbLogCache::dump(CFdbBaseObject *object, CFdbSession *session, int32_t size)
{
    if (!size)
    {
        return;
    }
    int32_t size_sent = 0;
    for (auto it = mCache.begin(); it != mCache.end(); ++it)
    {
        CFdbMessage msg((*it)->mCode, object, 0, FDB_INVALID_ID, FDB_INVALID_ID, FDB_QOS_RELIABLE);
        if (!msg.serialize((*it)->mLogData, (*it)->mSize, object))
        {
            continue;
        }
        object->broadcast(&msg, session);

        size_sent += (*it)->mSize;
        if ((size > 0) && (size_sent > size))
        {
            break;
        }
    }
}

void CFdbLogCache::resize(int32_t max_size)
{
    if ((max_size < 0) || (max_size == mMaxSize))
    {
        return;
    }
    while (mCacheSize > max_size)
    {
        popOne();
        if (mCache.empty())
        {
            break;
        }
    }
    mMaxSize = max_size;
}

