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

#define FDB_OPT_HAS_SENDER_NAME         (1 << 0)
#define FDB_OPT_HAS_HEAD_FILTER         (1 << 1)
#define FDB_OPT_HAS_SEND_ARRIVE_TIME    (1 << 2)
#define FDB_OPT_HAS_REPLY_TIME          (1 << 3)

class CFdbMessageHeader
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
        return !!(mOptions & FDB_OPT_HAS_SENDER_NAME);
    }
    const std::string &sender_name() const
    {
        return mSenderName;
    }
    void set_sender_name(const char *sender_name)
    {
        mSenderName = sender_name;
        mOptions |= FDB_OPT_HAS_SENDER_NAME;
    }
    bool has_broadcast_filter() const
    {
        return !!(mOptions & FDB_OPT_HAS_HEAD_FILTER);
    }
    const std::string &broadcast_filter() const
    {
        return mFilter;
    }
    void set_broadcast_filter(const char *filter)
    {
        mFilter.assign(filter);
        mOptions |= FDB_OPT_HAS_HEAD_FILTER;
    }
    bool has_send_or_arrive_time() const
    {
        return !!(mOptions & FDB_OPT_HAS_SEND_ARRIVE_TIME);
    }
    uint64_t send_or_arrive_time() const
    {
        return mSendArriveTime;
    }
    void set_send_or_arrive_time(uint64_t send_or_arrive_time)
    {
        mSendArriveTime = send_or_arrive_time;
        mOptions |= FDB_OPT_HAS_SEND_ARRIVE_TIME;
    }
    bool has_reply_time() const
    {
        return !!(mOptions & FDB_OPT_HAS_REPLY_TIME);
    }
    const uint64_t reply_time() const
    {
        return mReplyTime;
    }
    void set_reply_time(uint64_t reply_time)
    {
        mReplyTime = reply_time;
        mOptions |= FDB_OPT_HAS_REPLY_TIME;
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

    void serialize(CFdbSimpleSerializer &serializer)
    {
        serializer << mType << mSn << mCode << mFlag << mObjId << mPayloadSize << mOptions;
        if (mOptions & FDB_OPT_HAS_SENDER_NAME)
        {
            serializer << mSenderName;
        }
        if (mOptions & FDB_OPT_HAS_HEAD_FILTER)
        {
            serializer << mFilter;
        }
        if (mOptions & FDB_OPT_HAS_SEND_ARRIVE_TIME)
        {
            serializer << mSendArriveTime;
        }
        if (mOptions & FDB_OPT_HAS_REPLY_TIME)
        {
            serializer << mReplyTime;
        }
    }
    
    void deserialize(CFdbSimpleDeserializer &deserializer)
    {
        deserializer >> mType >> mSn >> mCode >> mFlag >> mObjId >> mPayloadSize >> mOptions;
        if (mOptions & FDB_OPT_HAS_SENDER_NAME)
        {
            deserializer >> mSenderName;
        }
        if (mOptions & FDB_OPT_HAS_HEAD_FILTER)
        {
            deserializer >> mFilter;
        }
        if (mOptions & FDB_OPT_HAS_SEND_ARRIVE_TIME)
        {
            deserializer >> mSendArriveTime;
        }
        if (mOptions & FDB_OPT_HAS_REPLY_TIME)
        {
            deserializer >> mReplyTime;
        }
    }
};

class CFdbMessageHeaderBuilder : public CFdbMessageHeader, public CFdbSimpleMsgBuilder
{
public:
    int32_t build()
    {
        serialize(mSerializer);
        return mSerializer.bufferSize();
    }
};

class CFdbMessageHeaderParser : public CFdbMessageHeader, public CFdbSimpleMsgParser
{
public:
    CFdbMessageHeaderParser(const uint8_t *buffer, int32_t size)
        : CFdbSimpleMsgParser(buffer, size)
    {
    }
    void prepare(uint8_t *buffer, int32_t size)
    {
        mDeserializer.reset(buffer, size);
    }
    int32_t parse()
    {
        deserialize(mDeserializer);
        return mDeserializer.error() ? -1 : mDeserializer.index();
    }
};

#endif

