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
#include "IFdbMsgBuilder.h"
#include <vector>
#include <common_base/CFdbSimpleMsgBuilder.h>

class CFdbMsgSubscribeItem : public IFdbParcelable
{
public:
    CFdbMsgSubscribeItem()
        : mOptions(0)
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
        return !!(mOptions & mMaskFilter);
    }
    const std::string &filter() const
    {
        return mFilter;
    }
    void set_filter(const char *filter)
    {
        mFilter.assign(filter);
        mOptions |= mMaskFilter;
    }
    bool has_type() const
    {
        return !!(mOptions & mMaskType);
    }
    CFdbSubscribeType type() const
    {
        return mType;
    }
    void set_type(CFdbSubscribeType type)
    {
        mType = type;
        mOptions |= mMaskType;
    }

    void serialize(CFdbSimpleSerializer &serializer) const
    {
        serializer << mCode << mOptions;
        if (mOptions & mMaskFilter)
        {
            serializer << mFilter;
        }
        if (mOptions & mMaskType)
        {
            serializer << (uint8_t)mType;
        }
    }
    void deserialize(CFdbSimpleDeserializer &deserializer)
    {
        deserializer >> mCode >> mOptions;
        if (mOptions & mMaskFilter)
        {
            deserializer >> mFilter;
        }
        if (mOptions & mMaskType)
        {
            deserializer >> (uint8_t &)mType;
        }
    }
protected:
    void toString(std::ostringstream &stream) const
    {
        stream << "event : " << mCode
               << ", topic : " << mFilter;
    }
    
private:
    int32_t mCode;
    std::string mFilter;
    CFdbSubscribeType mType;
    uint8_t mOptions;
        static const uint8_t mMaskFilter = 1 << 0;
        static const uint8_t mMaskType = 1 << 1;
};

class CFdbMsgTable : public IFdbParcelable
{
public:
    CFdbParcelableArray<CFdbMsgSubscribeItem> &subscribe_tbl()
    {
        return mSubscribeTbl;
    }
    CFdbMsgSubscribeItem *add_subscribe_tbl()
    {
        return mSubscribeTbl.Add();
    }

    void serialize(CFdbSimpleSerializer &serializer) const
    {
        serializer << mSubscribeTbl;
    }
    void deserialize(CFdbSimpleDeserializer &deserializer)
    {
        deserializer >> mSubscribeTbl;
    }
protected:
    void toString(std::ostringstream &stream) const
    {
        stream << "mName:"; mSubscribeTbl.format(stream);
    }
private:
    CFdbParcelableArray<CFdbMsgSubscribeItem> mSubscribeTbl;
};

#endif

