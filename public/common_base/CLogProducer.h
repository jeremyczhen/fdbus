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

#ifndef __CLOGPRODUCER_H__
#define __CLOGPRODUCER_H__
#include <string>
#include <set>
#include "CBaseClient.h"
#include "CFdbMessage.h"
#include "CFdbSimpleSerializer.h"
#include <mutex>

class CBaseEndpoint;
namespace NFdbBase {
enum FdbMessageLoggerCode
{
    REQ_FDBUS_LOG               = 0,
    REQ_TRACE_LOG               = 1,

    REQ_SET_LOGGER_CONFIG       = 2,
    REQ_SET_TRACE_CONFIG        = 3,

    REQ_GET_LOGGER_CONFIG       = 4,
    REQ_GET_TRACE_CONFIG        = 5,

    NTF_LOGGER_CONFIG           = 6,
    NTF_TRACE_CONFIG            = 7,

    NTF_FDBUS_LOG               = 8,
    NTF_TRACE_LOG               = 9,
} ;

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
    bool reverse_endpoint_name() const
    {
        return mReverseEndpointName;
    }
    void set_reverse_endpoint_name(bool reverse)
    {
        mReverseEndpointName = reverse;
    }
    bool reverse_bus_name() const
    {
        return mReverseBusName;
    }
    void set_reverse_bus_name(bool reverse)
    {
        mReverseBusName = reverse;
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
                   << mBusnameWhiteList
                   << mReverseEndpointName
                   << mReverseBusName;
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
                     >> mBusnameWhiteList
                     >> mReverseEndpointName
                     >> mReverseBusName;
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
    bool mReverseEndpointName;
    bool mReverseBusName;
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
    bool reverse_tag() const
    {
        return mReverseTag;
    }
    void set_reverse_tag(bool reverse)
    {
        mReverseTag = reverse;
    }

    void serialize(CFdbSimpleSerializer &serializer) const
    {
        serializer << mGlobalEnable
                   << (uint8_t)mLogLevel
                   << mHostWhiteList
                   << mTagWhiteList
                   << mBusnameWhiteList
                   << mReverseTag;
    }
    void deserialize(CFdbSimpleDeserializer &deserializer)
    {
        uint8_t level;
        deserializer >> mGlobalEnable
                     >> level
                     >> mHostWhiteList
                     >> mTagWhiteList
                     >> mBusnameWhiteList
                     >> mReverseTag;
        mLogLevel = (EFdbLogLevel)level;
    }
private:
    bool mGlobalEnable;
    EFdbLogLevel mLogLevel;
    CFdbParcelableArray<std::string> mHostWhiteList;
    CFdbParcelableArray<std::string> mTagWhiteList;
    CFdbParcelableArray<std::string> mBusnameWhiteList;
    bool mReverseTag;
};
}

class CLogProducer : public CBaseClient
{
public:
    CLogProducer();
    void logMessage(CFdbMessage *msg, const char *sender_name, CBaseEndpoint *endpoint);
    bool checkLogTraceEnabled(EFdbLogLevel log_level, const char *tag);
    void logTrace(EFdbLogLevel log_level, const char *tag, const char *info);

    bool checkLogEnabled(EFdbMessageType type,
                         const char *sender_name,
                         const CBaseEndpoint *endpoint,
                         bool lock = true);
    static const int32_t mMaxTraceLogSize = 4096;
protected:
    void onBroadcast(CBaseJob::Ptr &msg_ref);
    void onOnline(FdbSessionId_t sid, bool is_first);
    void onOffline(FdbSessionId_t sid, bool is_last);
private:
    typedef std::set<std::string> tFilterTbl;
    
    const char *getReceiverName(EFdbMessageType type,
                                const char *sender_name,
                                const CBaseEndpoint *endpoint);
    
    bool checkLogEnabledGlobally();
    bool checkLogEnabledByMessageType(EFdbMessageType type);
    bool checkLogEnabledByEndpoint(const char *sender, const char *receiver, const char *busname);
    
    CBASE_tProcId mPid;
    bool mLoggerDisableGlobal;
    bool mDisableRequest;
    bool mDisableReply;
    bool mDisableBroadcast;
    bool mDisableSubscribe;
    int32_t mRawDataClippingSize;
    EFdbLogLevel mLogLevel;
    bool mTraceDisableGlobal;

    tFilterTbl mLogEndpointWhiteList;
    tFilterTbl mLogBusnameWhiteList;
    bool mLogHostEnabled;

    tFilterTbl mTraceTagWhiteList;
    bool mTraceHostEnabled;

    bool mReverseEndpoints;
    bool mReverseBusNames;
    bool mReverseTags;

    std::mutex mTraceLock;

    bool checkHostEnabled(const CFdbParcelableArray<std::string> &host_tbl);
    void populateWhiteList(const CFdbParcelableArray<std::string> &in_filter
                         , tFilterTbl &white_list);
};
#endif
