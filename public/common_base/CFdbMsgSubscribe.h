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

#ifndef __CFDBMSGSUBSCRIBEITEM_H__
#define __CFDBMSGSUBSCRIBEITEM_H__

#include <string>
#include "CFdbSimpleSerializer.h"
#include "IFdbMsgSerializer.h"
#include <vector>

#define FDB_OPT_HAS_SUB_FILTER      (1 << 0)
#define FDB_OPT_HAS_TYPE            (1 << 1)

class CFdbMsgSubscribeItem
{
public:
    CFdbMsgSubscribeItem()
        : mOptions(false)
    {
    }
    int32_t msg_code() const
    {
        return mCode;
    }
    void set_msg_code(int32_t msg_code)
    {
        mCode = msg_code;
    }
    bool has_filter() const
    {
        return !!(mOptions & FDB_OPT_HAS_SUB_FILTER);
    }
    const std::string &filter() const
    {
        return mFilter;
    }
    void set_filter(const char *filter)
    {
        mFilter.assign(filter);
        mOptions |= FDB_OPT_HAS_SUB_FILTER;
    }
    bool has_type() const
    {
        return !!(mOptions & FDB_OPT_HAS_TYPE);
    }
    CFdbSubscribeType type() const
    {
        return mType;
    }
    void set_type(CFdbSubscribeType type)
    {
        mType = type;
        mOptions |= FDB_OPT_HAS_TYPE;
    }
    
private:
    int32_t mCode;
    std::string mFilter;
    CFdbSubscribeType mType;
    uint8_t mOptions;

    void serialize(CFdbSimpleSerializer &serializer)
    {
        serializer << mCode << mOptions;
        if (mOptions & FDB_OPT_HAS_SUB_FILTER)
        {
            serializer << mFilter;
        }
        if (mOptions & FDB_OPT_HAS_TYPE)
        {
            serializer << mType;
        }
    }
    void deserialize(CFdbSimpleDeserializer &deserializer)
    {
        deserializer >> mCode >> mOptions;
        if (mOptions & FDB_OPT_HAS_SUB_FILTER)
        {
            deserializer >> mFilter;
        }
        if (mOptions & FDB_OPT_HAS_TYPE)
        {
            deserializer >> mType;
        }
    }
    
    friend class CFdbMsgSubscribe;
};

class CFdbMsgSubscribe
{
public:
    typedef std::vector<CFdbMsgSubscribeItem> SubList_t;
    void addItem(CFdbMsgSubscribeItem &item)
    {
        mSubList.push_back(item);
    }
    SubList_t &getSubList()
    {
        return mSubList;
    }
    
protected:
    SubList_t mSubList;

    void serialize(CFdbSimpleSerializer &serializer)
    {
        uint32_t size = (uint32_t)mSubList.size();
        if (!size)
        {
            return;
        }
        serializer << size;
        for (SubList_t::iterator it = mSubList.begin(); it != mSubList.end(); ++it)
        {
            CFdbMsgSubscribeItem &item = *it;
            item.serialize(serializer);
        }
    }
    
    void deserialize(CFdbSimpleDeserializer &deserializer)
    {
        uint32_t size = 0;
        deserializer >> size;
        if (!size)
        {
            return;
        }
        try {
            mSubList.resize(size);
        } catch (...) {
            deserializer.error(true);
            return;
        }
        
        for (uint32_t i = 0; i < size; ++i)
        {
            mSubList[i].deserialize(deserializer);
        }
    }
};

class CFdbMsgSubscribeBuilder : public CFdbMsgSubscribe, public IFdbMsgBuilder
{
public:
    int32_t build()
    {
        serialize(mSerializer);
        return mSerializer.bufferSize();
    }
    void toBuffer(uint8_t *buffer, int32_t size)
    {
        mSerializer.toBuffer(buffer, size);
    }
private:
    CFdbSimpleSerializer mSerializer;
};

class CFdbMsgSubscribeParser : public CFdbMsgSubscribe, public IFdbMsgParser
{
public:
	CFdbMsgSubscribeParser(uint8_t *buffer = 0, int32_t size = 0)
        : mDeserializer(buffer, size)
    {}
    void prepare(uint8_t *buffer, int32_t size)
    {
        mDeserializer.reset(buffer, size);
    }
    int32_t parse()
    {
        deserialize(mDeserializer);
        return mDeserializer.error() ? -1 : mDeserializer.index();
    }
private:
    CFdbSimpleDeserializer mDeserializer;
};

#endif

