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

CFdbLogCache::CDataItem::CDataItem(uint8_t *log_data, int32_t size, FdbEventCode_t code, uint32_t id)
    : mLogData(new uint8_t[size])
    , mSize(size)
    , mCode(code)
    , mId(id)
{
    memcpy(mLogData, log_data, size);
}

CFdbLogCache::CDataItem::~CDataItem()
{
    delete[] mLogData;
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
        while (!mCache.empty())
        {
            auto h = mCache.front();
            mCache.pop_front();
            delete h;
        }
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

void CFdbLogCache::push(uint8_t *log_data, int32_t size, FdbEventCode_t code)
{
    if (!mMaxSize || (size <= 0))
    {
        return;
    }
    while ((mCacheSize + size + (int32_t)sizeof(CDataItem)) > mMaxSize)
    {
        popOne();
        if (mCache.empty())
        {
            break;
        }
    }
    mCacheSize += size + sizeof(CDataItem);
    mCache.push_back(new CDataItem(log_data, size, code, mDataId++));
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

