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

#ifndef _CFDBLOGCACHE_H_
#define _CFDBLOGCACHE_H_

#include <deque>
#include <common_base/common_defs.h>

class CFdbSession;
class CFdbBaseObject;

class CFdbLogCache
{
public:
    CFdbLogCache(int32_t max_size)
        : mMaxSize(max_size)
        , mCacheSize(0)
        , mDataId(0)
    {
    }
    void push(uint8_t *log_data, int32_t size, FdbEventCode_t code);
    void dump(CFdbBaseObject *object, CFdbSession *session, int32_t size);
    void resize(int32_t max_size);

private:
    struct CDataItem
    {
        uint8_t *mLogData;
        int32_t mSize;
        FdbEventCode_t mCode;
        uint32_t mId;
        CDataItem(uint8_t *log_data, int32_t size, FdbEventCode_t code, uint32_t id);
        ~CDataItem();
    };
    typedef std::deque<CDataItem *> tCache;
    tCache mCache;
    int32_t mMaxSize;
    int32_t mCacheSize;
    uint32_t mDataId;

    void popOne();
};

#endif
