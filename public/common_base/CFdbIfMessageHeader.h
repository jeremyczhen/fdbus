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
#include "common_defs.h"

namespace NFdbBase {
enum FdbMessageLoggerCode
{
    REQ_FDBUS_LOG               = 0,
    REQ_TRACE_LOG               = 1,

    REQ_LOGGER_CONFIG           = 2,
    REQ_TRACE_CONFIG            = 3,

    NTF_LOGGER_CONFIG           = 4,
    NTF_TRACE_CONFIG            = 5,

    NTF_FDBUS_LOG               = 6,
    NTF_TRACE_LOG               = 7,
} ;

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

    void serialize(CFdbSimpleSerializer &serializer) const
    {
        serializer << (uint8_t)mType
                   << mSn
                   << mCode
                   << mFlag
                   << mObjId
                   << mPayloadSize
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
    }

    void deserialize(CFdbSimpleDeserializer &deserializer)
    {
        uint8_t msg_type;
        deserializer >> msg_type
                     >> mSn
                     >> mCode
                     >> mFlag
                     >> mObjId
                     >> mPayloadSize
                     >> mOptions;
        mType = (EFdbMessageType)msg_type;
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
    uint8_t mOptions;
        static const uint8_t mMaskHeadFilter = 1 << 1;
        static const uint8_t mMaskSenderArriveTime = 1 << 2;
        static const uint8_t mMaskReplyTime = 1 << 3;
    
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

class FdbMsgLogConfig : public IFdbParcelable
{
public:
    bool global_enable() const
    {
        return mGlobalEnable;
    }
    void set_global_enable(bool enb)
    {
        mGlobalEnable = enb;
    }
    bool enable_request() const
    {
        return mEnableRequest;
    }
    void set_enable_request(bool enb)
    {
        mEnableRequest = enb;
    }
    bool enable_reply() const
    {
        return mEnableReply;
    }
    void set_enable_reply(bool enb)
    {
        mEnableReply = enb;
    }
    bool enable_broadcast() const
    {
        return mEnableBroadcast;
    }
    void set_enable_broadcast(bool enb)
    {
        mEnableBroadcast = enb;
    }
    bool enable_subscribe() const
    {
        return mEnableSubscribe;
    }
    void set_enable_subscribe(bool enb)
    {
        mEnableSubscribe = enb;
    }
    int32_t raw_data_clipping_size() const
    {
        return mRawDataClippingSize;
    }
    void set_raw_data_clipping_size(int32_t size)
    {
        mRawDataClippingSize = size;
    }
    CFdbParcelableArray<std::string> &host_white_list()
    {
        return mHostWhiteList;
    }
    void add_host_white_list(const std::string &name)
    {
        mHostWhiteList.Add(name);
    }
    void add_host_white_list(const char *name)
    {
        mHostWhiteList.Add(name);
    }
    CFdbParcelableArray<std::string> &endpoint_white_list()
    {
        return mEndpointWhiteList;
    }
    void add_endpoint_white_list(const std::string &name)
    {
        mEndpointWhiteList.Add(name);
    }
    void add_endpoint_white_list(const char *name)
    {
        mEndpointWhiteList.Add(name);
    }
    CFdbParcelableArray<std::string> &busname_white_list()
    {
        return mBusnameWhiteList;
    }
    void add_busname_white_list(const std::string &name)
    {
        mBusnameWhiteList.Add(name);
    }
    void add_busname_white_list(const char *name)
    {
        mBusnameWhiteList.Add(name);
    }

    void serialize(CFdbSimpleSerializer &serializer) const
    {
        serializer << mGlobalEnable
                   << mEnableRequest
                   << mEnableReply
                   << mEnableBroadcast
                   << mEnableSubscribe
                   << mRawDataClippingSize
                   << mHostWhiteList
                   << mEndpointWhiteList
                   << mBusnameWhiteList;
    }
    void deserialize(CFdbSimpleDeserializer &deserializer)
    {
        deserializer >> mGlobalEnable
                     >> mEnableRequest
                     >> mEnableReply
                     >> mEnableBroadcast
                     >> mEnableSubscribe
                     >> mRawDataClippingSize
                     >> mHostWhiteList
                     >> mEndpointWhiteList
                     >> mBusnameWhiteList;
    }
private:
    bool mGlobalEnable;
    bool mEnableRequest;
    bool mEnableReply;
    bool mEnableBroadcast;
    bool mEnableSubscribe;
    int32_t mRawDataClippingSize;
    CFdbParcelableArray<std::string> mHostWhiteList;
    CFdbParcelableArray<std::string> mEndpointWhiteList;
    CFdbParcelableArray<std::string> mBusnameWhiteList;
};

class FdbTraceConfig : public IFdbParcelable
{
public:
    bool global_enable() const
    {
        return mGlobalEnable;
    }
    void set_global_enable(bool enb)
    {
        mGlobalEnable = enb;
    }
    EFdbLogLevel log_level() const
    {
        return mLogLevel;
    }
    void set_log_level(EFdbLogLevel level)
    {
        mLogLevel = level;
    }
    CFdbParcelableArray<std::string> &host_white_list()
    {
        return mHostWhiteList;
    }
    void add_host_white_list(const std::string &name)
    {
        mHostWhiteList.Add(name);
    }
    void add_host_white_list(const char *name)
    {
        mHostWhiteList.Add(name);
    }
    CFdbParcelableArray<std::string> &tag_white_list()
    {
        return mTagWhiteList;
    }
    void add_tag_white_list(const std::string &name)
    {
        mTagWhiteList.Add(name);
    }
    void add_tag_white_list(const char *name)
    {
        mTagWhiteList.Add(name);
    }
    CFdbParcelableArray<std::string> &busname_white_list()
    {
        return mBusnameWhiteList;
    }
    void add_busname_white_list(const std::string &name)
    {
        mBusnameWhiteList.Add(name);
    }
    void add_busname_white_list(const char *name)
    {
        mBusnameWhiteList.Add(name);
    }

    void serialize(CFdbSimpleSerializer &serializer) const
    {
        serializer << mGlobalEnable
                   << (uint8_t)mLogLevel
                   << mHostWhiteList
                   << mTagWhiteList
                   << mBusnameWhiteList;
    }
    void deserialize(CFdbSimpleDeserializer &deserializer)
    {
        uint8_t level;
        deserializer >> mGlobalEnable
                     >> level
                     >> mHostWhiteList
                     >> mTagWhiteList
                     >> mBusnameWhiteList;
        mLogLevel = (EFdbLogLevel)level;
    }
private:
    bool mGlobalEnable;
    EFdbLogLevel mLogLevel;
    CFdbParcelableArray<std::string> mHostWhiteList;
    CFdbParcelableArray<std::string> mTagWhiteList;
    CFdbParcelableArray<std::string> mBusnameWhiteList;
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
    const std::string &sender_name() const
    {
        return mSenderName;
    }
    void set_sender_name(const char *name)
    {
        mSenderName = name;
    }
    void serialize(CFdbSimpleSerializer &serializer) const
    {
        serializer << mSenderName;
    }
    void deserialize(CFdbSimpleDeserializer &deserializer)
    {
        deserializer >> mSenderName;
    }
private:
    std::string mSenderName;
};
}

#endif

