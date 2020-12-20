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
#include "CFdbIfMsgTokens.h"
#include <common_base/common_defs.h>

namespace NFdbBase {
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
    EFdbQOS qos() const
    {
        return mQOS;
    }
    void qos(EFdbQOS qos)
    {
        mQOS = qos;
    }
    bool has_broadcast_filter() const
    {
        return !!(mOptions & mMaskHeadFilter);
    }
    const std::string &broadcast_filter() const
    {
        return mFilter;
    }
    void set_broadcast_filter(const char *filter)
    {
        mFilter.assign(filter);
        mOptions |= mMaskHeadFilter;
    }
    bool has_send_or_arrive_time() const
    {
        return !!(mOptions & mMaskSenderArriveTime);
    }
    uint64_t send_or_arrive_time() const
    {
        return mSendArriveTime;
    }
    void set_send_or_arrive_time(uint64_t send_or_arrive_time)
    {
        mSendArriveTime = send_or_arrive_time;
        mOptions |= mMaskSenderArriveTime;
    }
    bool has_reply_time() const
    {
        return !!(mOptions & mMaskReplyTime);
    }
    uint64_t reply_time() const
    {
        return mReplyTime;
    }
    void set_reply_time(uint64_t reply_time)
    {
        mReplyTime = reply_time;
        mOptions |= mMaskReplyTime;
    }
    bool has_token() const
    {
        return !!(mOptions & mMaskToken);
    }
    const std::string &token() const
    {
        return mToken;
    }
    void set_token(const char *token)
    {
        mToken.assign(token);
        mOptions |= mMaskToken;
    }

    void serialize(CFdbSimpleSerializer &serializer) const
    {
        serializer << (uint8_t)mType
                   << mSn
                   << mCode
                   << mFlag
                   << mObjId
                   << mPayloadSize
                   << (uint8_t)mQOS
                   << mOptions;
        if (mOptions & mMaskHeadFilter)
        {
            serializer << mFilter;
        }
        if (mOptions & mMaskSenderArriveTime)
        {
            serializer << mSendArriveTime;
        }
        if (mOptions & mMaskReplyTime)
        {
            serializer << mReplyTime;
        }
        if (mOptions & mMaskToken)
        {
            serializer << mToken;
        }
    }

    void deserialize(CFdbSimpleDeserializer &deserializer)
    {
        uint8_t msg_type;
        uint8_t qos;
        deserializer >> msg_type
                     >> mSn
                     >> mCode
                     >> mFlag
                     >> mObjId
                     >> mPayloadSize
                     >> qos
                     >> mOptions;
        mType = (EFdbMessageType)msg_type;
        mQOS = (EFdbQOS)qos;
        if (mOptions & mMaskHeadFilter)
        {
            deserializer >> mFilter;
        }
        if (mOptions & mMaskSenderArriveTime)
        {
            deserializer >> mSendArriveTime;
        }
        if (mOptions & mMaskReplyTime)
        {
            deserializer >> mReplyTime;
        }
        if (mOptions & mMaskToken)
        {
            deserializer >> mToken;
        }
    }
    
private:
    EFdbMessageType mType;
    int32_t mSn;
    int32_t mCode;
    uint32_t mFlag;
    uint32_t mObjId;
    uint32_t mPayloadSize;
    std::string mFilter;
    uint64_t mSendArriveTime;
    uint64_t mReplyTime;
    std::string mToken;
    EFdbQOS mQOS;
    uint8_t mOptions;
        static const uint8_t mMaskHeadFilter = 1 << 1;
        static const uint8_t mMaskSenderArriveTime = 1 << 2;
        static const uint8_t mMaskReplyTime = 1 << 3;
        static const uint8_t mMaskToken = 1 << 4;
    
};

class FdbMsgErrorInfo : public IFdbParcelable
{
public:
    FdbMsgErrorInfo()
        : mOptions(0)
    {}
    int32_t error_code() const
    {
        return mErrorCode;
    }
    void set_error_code(int32_t error_code)
    {
        mErrorCode = error_code;
    }
    std::string &description()
    {
        return mDescription;
    }
    void set_description(const char *description)
    {
        mDescription = description;
        mOptions |= mMaskDescription;
    }
    void set_description(const std::string &description)
    {
        mDescription = description;
        mOptions |= mMaskDescription;
    }

    bool has_description() const
    {
        return !!(mOptions & mMaskDescription);
    }

    void serialize(CFdbSimpleSerializer &serializer) const
    {
        serializer << mErrorCode
                   << mOptions;
        if (mOptions & mMaskDescription)
        {
            serializer << mDescription;
        }
    }
    
    void deserialize(CFdbSimpleDeserializer &deserializer)
    {
        deserializer >> mErrorCode
                     >> mOptions;
        if (mOptions & mMaskDescription)
        {
            deserializer >> mDescription;
        }
    }
private:
    int32_t mErrorCode;
    std::string mDescription;
    uint8_t mOptions;
        static const uint8_t mMaskDescription = 1 << 0;
};

class FdbAuthentication : public IFdbParcelable
{
public:
    FdbAuthentication()
        : mOptions(0)
    {}
    FdbMsgTokens &token_list()
    {
        mOptions |= mMaskTokenList;
        return mTokenList;
    }
    bool has_token_list() const
    {
        return !!(mOptions & mMaskTokenList);
    }

    void serialize(CFdbSimpleSerializer &serializer) const
    {
        serializer << mOptions;
        if (mOptions & mMaskTokenList)
        {
            serializer << mTokenList;
        }
    }
    void deserialize(CFdbSimpleDeserializer &deserializer)
    {
        deserializer >> mOptions;
        if (mOptions & mMaskTokenList)
        {
            deserializer >> mTokenList;
        }
    }
private:
    FdbMsgTokens mTokenList;
    uint8_t mOptions;
        static const uint8_t mMaskTokenList = 1 << 0;
};

class FdbSessionInfo : public IFdbParcelable
{
public:
    FdbSessionInfo()
        : mOptions(0)
    {
    }
    const std::string &sender_name() const
    {
        return mSenderName;
    }
    void set_sender_name(const char *name)
    {
        mSenderName = name;
    }
    int32_t udp_port() const
    {
        return mUDPPort;
    }
    void set_udp_port(int32_t port)
    {
        mUDPPort = port;
        mOptions |= mMaskHasUDPPort;
    }
    bool has_udp_port() const
    {
        return !!(mOptions & mMaskHasUDPPort);
    }
    uint32_t pid() const
    {
        return mPid;
    }
    void set_pid(uint32_t pid)
    {
        mPid = pid;
    }
    void serialize(CFdbSimpleSerializer &serializer) const
    {
        serializer << mSenderName
                   << mPid
                   << mOptions;
        if (mOptions & mMaskHasUDPPort)
        {
            serializer << mUDPPort;
        }
    }
    void deserialize(CFdbSimpleDeserializer &deserializer)
    {
        deserializer >> mSenderName
                     >> mPid
                     >> mOptions;
        if (mOptions & mMaskHasUDPPort)
        {
            deserializer >> mUDPPort;
        }
    }
private:
    std::string mSenderName;
    int32_t mUDPPort;
    uint32_t mPid;
    uint8_t mOptions;
        static const uint8_t mMaskHasUDPPort = 1 << 0;
};
}

#endif

