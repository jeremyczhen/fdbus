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

#ifndef __CFDBMESSAGEHEADER_H__
#define __CFDBMESSAGEHEADER_H__

#include <string>
#include <common_base/CFdbSimpleMsgBuilder.h>

class CFdbMessageHeader : public IFdbParcelable
{
public:
    CFdbMessageHeader()
        : mOptions(0)
    {}
    EFdbMessageType type() const
    {
        return mType;
    }
    void set_type(EFdbMessageType type)
    {
        mType = type;
    }
    int32_t serial_number() const
    {
        return mSn;
    }
    void set_serial_number(int32_t sn)
    {
        mSn = sn;
    }
    int32_t code() const
    {
        return mCode;
    }
    void set_code(int32_t code)
    {
        mCode = code;
    }
    uint32_t flag() const
    {
        return mFlag;
    }
    void set_flag(uint32_t flag)
    {
        mFlag = flag;
    }
    uint32_t object_id() const
    {
        return mObjId;
    }
    void set_object_id(int32_t obj_id)
    {
        mObjId = obj_id;
    }
    uint32_t payload_size() const
    {
        return mPayloadSize;
    }
    void set_payload_size(uint32_t payload_size)
    {
        mPayloadSize = payload_size;
    }
    bool has_sender_name() const
    {
        return !!(mOptions & mOptHasSenderName);
    }
    const std::string &sender_name() const
    {
        return mSenderName;
    }
    void set_sender_name(const char *sender_name)
    {
        mSenderName = sender_name;
        mOptions |= mOptHasSenderName;
    }
    bool has_broadcast_filter() const
    {
        return !!(mOptions & mOptHasFilter);
    }
    const std::string &broadcast_filter() const
    {
        return mFilter;
    }
    void set_broadcast_filter(const char *filter)
    {
        mFilter.assign(filter);
        mOptions |= mOptHasFilter;
    }
    bool has_send_or_arrive_time() const
    {
        return !!(mOptions & mOptHasArriveTime);
    }
    uint64_t send_or_arrive_time() const
    {
        return mSendArriveTime;
    }
    void set_send_or_arrive_time(uint64_t send_or_arrive_time)
    {
        mSendArriveTime = send_or_arrive_time;
        mOptions |= mOptHasArriveTime;
    }
    bool has_reply_time() const
    {
        return !!(mOptions & mOptHasReplyTime);
    }
    uint64_t reply_time() const
    {
        return mReplyTime;
    }
    void set_reply_time(uint64_t reply_time)
    {
        mReplyTime = reply_time;
        mOptions |= mOptHasReplyTime;
    }

    void serialize(CFdbSimpleSerializer &serializer) const
    {
        serializer << mType << mSn << mCode << mFlag << mObjId << mPayloadSize << mOptions;
        if (mOptions & mOptHasSenderName)
        {
            serializer << mSenderName;
        }
        if (mOptions & mOptHasFilter)
        {
            serializer << mFilter;
        }
        if (mOptions & mOptHasArriveTime)
        {
            serializer << mSendArriveTime;
        }
        if (mOptions & mOptHasReplyTime)
        {
            serializer << mReplyTime;
        }
    }

    void deserialize(CFdbSimpleDeserializer &deserializer)
    {
        deserializer >> mType >> mSn >> mCode >> mFlag >> mObjId >> mPayloadSize >> mOptions;
        if (mOptions & mOptHasSenderName)
        {
            deserializer >> mSenderName;
        }
        if (mOptions & mOptHasFilter)
        {
            deserializer >> mFilter;
        }
        if (mOptions & mOptHasArriveTime)
        {
            deserializer >> mSendArriveTime;
        }
        if (mOptions & mOptHasReplyTime)
        {
            deserializer >> mReplyTime;
        }
    }
    
protected:
    EFdbMessageType mType;
    int32_t mSn;
    int32_t mCode;
    uint32_t mFlag;
    uint32_t mObjId;
    uint32_t mPayloadSize;
    std::string mSenderName;
    std::string mFilter;
    uint64_t mSendArriveTime;
    uint64_t mReplyTime;
    uint8_t mOptions;
        static const uint8_t mOptHasSenderName = (1 << 0);
        static const uint8_t mOptHasFilter = (1 << 1);
        static const uint8_t mOptHasArriveTime = (1 << 2);
        static const uint8_t mOptHasReplyTime = (1 << 3);
};

#endif

