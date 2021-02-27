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

#include <common_base/CFdbMessage.h>
#include <common_base/CLogProducer.h>
#include <common_base/CBaseThread.h>
#include <common_base/CFdbContext.h>
#include <server/CIntraNameProxy.h>
#include <server/CFdbLogCache.h>
#include <common_base/CFdbSession.h>
#include <server/CLogPrinter.h>
#include <common_base/CFdbRawMsgBuilder.h>
#include <utils/CFdbIfMessageHeader.h>
#include <common_base/fdb_log_trace.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <inttypes.h>
#include <utils/Log.h>

using namespace std::placeholders;

const char *CLogProducer::mLogHead =
    "+==============================================================================+\n"
    "|                                 START LOGGING...                             |\n"
    "+==============================================================================+\n"
    ;
const char *CLogProducer::mBreakMark =
    "+==============================================================================+\n"
    "|                        WARNING: LOG MIGHT DROPPED!!!                         |\n"
    "+==============================================================================+\n"
    ;

EFdbLogLevel CLogProducer::mStaticLogLevel = FDB_LL_INFO;

CLogProducer::CLogProducer(int32_t log_cache_size)
    : CBaseClient(FDB_LOG_SERVER_NAME)
    , mPid(CBaseThread::getPid())
    , mLoggerDisableGlobal(false)
    , mDisableRequest(false)
    , mDisableReply(false)
    , mDisableBroadcast(false)
    , mDisableSubscribe(false)
    , mRawDataClippingSize(0)
    , mLogLevel(FDB_LL_INFO)
    , mTraceDisableGlobal(false)
    , mLogHostEnabled(true)
    , mTraceHostEnabled(true)
    , mReverseEndpoints(false)
    , mReverseBusNames(false)
    , mReverseTags(false)
    , mLogCache(log_cache_size ? new CFdbLogCache(log_cache_size, true) : 0)
{
    enableLog(false);
}

CLogProducer::~CLogProducer()
{
    if (mLogCache)
    {
        delete mLogCache;
    }
}

void CLogProducer::sendCheckpoint(const char *check_point)
{
    CBaseMessage msg(0, this);
    msg.enableLog(true);
    msg.setLogData(check_point);
    logFDBus(&msg, 0, this);
}

void CLogProducer::onOnline(FdbSessionId_t sid, bool is_first)
{
    CFdbMsgSubscribeList subscribe_list;
    addNotifyItem(subscribe_list, NFdbBase::NTF_LOGGER_CONFIG);
    addNotifyItem(subscribe_list, NFdbBase::NTF_TRACE_CONFIG);
    subscribe(subscribe_list);
    if (mLogCache)
    {
        sendCheckpoint(mLogHead);
        bool cache_full = mLogCache->full();
        mLogCache->dump(this);
        if (cache_full)
        {
            sendCheckpoint(mBreakMark);
        }
    }
}

void CLogProducer::onOffline(FdbSessionId_t sid, bool is_last)
{
}

void CLogProducer::onBroadcast(CBaseJob::Ptr &msg_ref)
{
    auto msg = castToMessage<CFdbMessage *>(msg_ref);
    switch (msg->code())
    {
        case NFdbBase::NTF_LOGGER_CONFIG:
        {
            NFdbBase::FdbMsgLogConfig cfg;
            CFdbParcelableParser parser(cfg);
            if (!msg->deserialize(parser))
            {
                LOG_E("CLogProducer: Unable to deserialize FdbMsgLogConfig!\n");
                return;
            }
            mLoggerDisableGlobal = !cfg.global_enable();
            mDisableRequest = !cfg.enable_request();
            mDisableReply = !cfg.enable_reply();
            mDisableBroadcast = !cfg.enable_broadcast();
            mDisableSubscribe = !cfg.enable_subscribe();
            mRawDataClippingSize = cfg.raw_data_clipping_size();
            mLogHostEnabled = checkHostEnabled(cfg.host_white_list());
            std::lock_guard<std::mutex> _l(mFdbusLogLock);
            populateWhiteList(cfg.endpoint_white_list(), mLogEndpointWhiteList);
            populateWhiteList(cfg.busname_white_list(), mLogBusnameWhiteList);
            mReverseEndpoints = cfg.reverse_endpoint_name();
            mReverseBusNames = cfg.reverse_bus_name();
        }
        break;
        case NFdbBase::NTF_TRACE_CONFIG:
        {
            NFdbBase::FdbTraceConfig cfg;
            CFdbParcelableParser parser(cfg);
            if (!msg->deserialize(parser))
            {
                LOG_E("CLogProducer: Unable to deserialize FdbTraceConfig!\n");
                return;
            }
            mLogLevel = (EFdbLogLevel)cfg.log_level();
            mTraceDisableGlobal = !cfg.global_enable();
            mTraceHostEnabled = checkHostEnabled(cfg.host_white_list());
            std::lock_guard<std::mutex> _l(mTraceLock);
            populateWhiteList(cfg.tag_white_list(), mTraceTagWhiteList);
            mReverseTags = cfg.reverse_tag();
        }
        break;
        default:
        break;
    }
}

bool CLogProducer::checkLogEnabledByMessageType(EFdbMessageType type)
{
    bool match = true;
    switch (type)
    {
        case FDB_MT_REQUEST:
        case FDB_MT_SIDEBAND_REQUEST:
        case FDB_MT_GET_EVENT:
        case FDB_MT_PUBLISH:
            if (mDisableRequest)
            {
                match = false;
            }
        break;
        case FDB_MT_REPLY:
        case FDB_MT_STATUS:
        case FDB_MT_SIDEBAND_REPLY:
        case FDB_MT_RETURN_EVENT:
            if (mDisableReply)
            {
                match = false;
            }
        break;
        case FDB_MT_BROADCAST:
            if (mDisableBroadcast)
            {
                match = false;
            }
        break;
        case FDB_MT_SUBSCRIBE_REQ:
            if (mDisableSubscribe)
            {
                match = false;
            }
        break;
        default:
            match = true;
        break;
    }
    
    return match;
}

bool CLogProducer::checkCacheEnabled()
{
    if (mLogCache)
    {
        if (!getSessionCount() && mLogCache->full())
        {
            return false;
        }
    }
    else
    {
        if (!getSessionCount())
        {
            return false;
        }
    }

    return true;
}

bool CLogProducer::checkLogEnabled(EFdbMessageType type,
                                   const char *receiver_name,
                                   CBaseEndpoint *endpoint)
{
    if (mLoggerDisableGlobal || !mLogHostEnabled || !checkCacheEnabled())
    {
        return false;
    }
    
    if (!checkLogEnabledByMessageType(type))
    {
        return false;
    }

    const char *sender_name = endpoint->name().c_str();
    const char *bus_name = endpoint->nsName().c_str();
    if (!mLogEndpointWhiteList.empty() || !mLogBusnameWhiteList.empty())
    {
        std::lock_guard<std::mutex> _l(mFdbusLogLock);
        if (!mLogEndpointWhiteList.empty())
        {
            if (!sender_name || (sender_name[0] == '\0'))
            {
                if (!receiver_name || (receiver_name[0] == '\0'))
                {
                    return false;
                }
                auto it_receiver = mLogEndpointWhiteList.find(receiver_name);
                bool exclude = it_receiver == mLogEndpointWhiteList.end();
                if (mReverseEndpoints ^ exclude)
                {
                    return false;
                }
            }
            else if (!receiver_name || (receiver_name[0] == '\0'))
            {
                if (!sender_name || (sender_name[0] == '\0'))
                {
                    return false;
                }
                auto it_sender = mLogEndpointWhiteList.find(sender_name);
                bool exclude = it_sender == mLogEndpointWhiteList.end();
                if (mReverseEndpoints ^ exclude)
                {
                    return false;
                }
            }
            else
            {
                auto it_sender = mLogEndpointWhiteList.find(sender_name);
                auto it_receiver = mLogEndpointWhiteList.find(receiver_name);
                bool exclude = ((it_sender == mLogEndpointWhiteList.end()) && (it_receiver == mLogEndpointWhiteList.end()));
                if (mReverseEndpoints ^ exclude)
                {
                    return false;
                }
            }
        }
        if (!mLogBusnameWhiteList.empty())
        {
            if (!bus_name || (bus_name[0] == '\0'))
            {
                return false;
            }
            auto it_bus_name = mLogBusnameWhiteList.find(bus_name);
            bool exclude = (it_bus_name == mLogBusnameWhiteList.end());
            if (mReverseBusNames ^ exclude)
            {
                return false;
            }
        }
    }

    return true;
}

void CLogProducer::logFDBus(CFdbMessage *msg, const char *receiver_name, CBaseEndpoint *endpoint,
                            CFdbRawMsgBuilder &builder)
{
    if (!msg->isLogEnabled())
    {
        return;
    }

    const char *sender_name = endpoint->name().empty() ? "None" :  endpoint->name().c_str();
    const char *bus_name = endpoint->nsName().empty() ? "None" :  endpoint->nsName().c_str();
    if (!receiver_name || (receiver_name[0] == '\0'))
    {
        receiver_name = "None";
    }

    auto proxy = FDB_CONTEXT->getNameProxy();
    char date_buf[64];
    sysdep_gettimestamp(date_buf, sizeof(date_buf), 1, 0);
    date_buf[63] = '\0';
    std::string date_str = date_buf;
    builder.serializer() << (uint32_t)mPid
                         << (proxy ? proxy->hostName().c_str() : "None")
                         << sender_name
                         << receiver_name
                         << bus_name
                         << (uint8_t)msg->type()
                         << msg->code()
                         << msg->topic()
                         << date_str
                         << msg->getPayloadSize()
                         << msg->sn()
                         << msg->objectId()
                         ;

    if (!msg->mStringData.empty())
    {
        builder.serializer() << true << msg->mStringData;
    }
    else
    {
        builder.serializer() << false;
        int32_t log_size = msg->getPayloadSize();
        if ((mRawDataClippingSize >= 0) && (mRawDataClippingSize < log_size))
        {
            log_size = mRawDataClippingSize;
        }
        builder.serializer() << log_size;
        builder.serializer().addRawData(msg->getPayloadBuffer(), log_size);
    }
}

void CLogProducer::logFDBus(CFdbMessage *msg, const char *receiver_name, CBaseEndpoint *endpoint)
{
    CFdbRawMsgBuilder builder;
    logFDBus(msg, receiver_name, endpoint, builder);
    sendLog(NFdbBase::REQ_FDBUS_LOG, builder);
}

bool CLogProducer::checkLogTraceEnabled(EFdbLogLevel log_level, const char *tag)
{
    if (mTraceDisableGlobal || !mTraceHostEnabled || !checkCacheEnabled()
        || (log_level < mLogLevel) || (log_level >= FDB_LL_SILENT))
    {
        return false;
    }

    if (!mTraceTagWhiteList.empty())
    {
        std::lock_guard<std::mutex> _l(mTraceLock);
        if (!mTraceTagWhiteList.empty())
        {
            auto it_tag = mTraceTagWhiteList.find(tag);
            bool exclude = (it_tag == mTraceTagWhiteList.end());
            if (mReverseTags ^ exclude)
            {
                return false;
            }
        }
    }

    return true;
}

void CLogProducer::logTrace(EFdbLogLevel log_level, const char *tag, const char *info)
{
    auto proxy = FDB_CONTEXT->getNameProxy();
    CFdbRawMsgBuilder builder;
    builder.serializer() << (uint32_t)mPid
                         << tag
                         << (proxy ? proxy->hostName().c_str() : "Unknown")
                         << sysdep_getsystemtime_milli()
                         << (uint8_t)log_level
                         << info;

    sendLog(NFdbBase::REQ_TRACE_LOG, builder);
}

void CLogProducer::printTrace(EFdbLogLevel log_level, const char *tag, const char *info)
{
    auto proxy = FDB_CONTEXT->getNameProxy();
    char date_buf[64];
    sysdep_gettimestamp(date_buf, sizeof(date_buf), 1, 0);
    date_buf[63] = '\0';
    CLogPrinter::TraceInfo trace_info;
    trace_info.mPid = CBaseThread::getPid();
    trace_info.mTag = tag;
    trace_info.mHostName = (proxy ? proxy->hostName().c_str() : "Unknown");
    trace_info.mTimeStamp = date_buf;
    trace_info.mLogLevel = log_level;
    trace_info.mData = info ? info : "\n";
    CLogPrinter::outputTraceLog(trace_info, std::cout);
}

bool CLogProducer::checkHostEnabled(const CFdbParcelableArray<std::string> &host_tbl)
{
    if (host_tbl.empty())
    {
        return true;
    }

    CIntraNameProxy *proxy = FDB_CONTEXT->getNameProxy();
    if (!proxy)
    {
        return true;
    }

    for (auto it = host_tbl.pool().begin(); it != host_tbl.pool().end(); ++it)
    {
        if (!it->compare(proxy->hostName()))
        {
            return true;
        }
    }

    return false;
}

void CLogProducer::populateWhiteList(const CFdbParcelableArray<std::string> &in_filter
                                   , tFilterTbl &white_list)
{
    white_list.clear();
    for (auto it = in_filter.pool().begin(); it != in_filter.pool().end(); ++it)
    {
        white_list.insert(*it);
    }
}

void CLogProducer::callSendLog(CBaseJob::Ptr &msg_ref)
{
    auto msg = castToMessage<CFdbMessage *>(msg_ref);
    if (connected())
    {
        msg->expectReply(false);
        auto session = mEndpoint->preferredPeer();
        if (session)
        {
            session->sendMessage(msg);
        }
    }
    else if (mLogCache)
    {
        mLogCache->push(msg->getPayloadBuffer(), msg->getPayloadSize(), msg->code());
    }
}

bool CLogProducer::sendLog(FdbMsgCode_t code, IFdbMsgBuilder &builder)
{
    uint8_t *buffer = const_cast<uint8_t *>(builder.buffer());
    int32_t size = builder.bufferSize();
    bool need_release = false;
    if (!buffer && size)
    {
        buffer = new uint8_t[size];
        builder.toBuffer(buffer, size);
        need_release = true;
    }

    bool ret = sendLog(code, buffer, size);

    if (need_release)
    {
        delete[] buffer;
    }

    return ret;
}

bool CLogProducer::sendLog(FdbMsgCode_t code, const uint8_t *buffer, int32_t size)
{
    if (FDB_CONTEXT->isSelf())
    {
        if (connected())
        {
            CBaseMessage msg(code, this);
            msg.expectReply(false);
            if (!msg.serialize(buffer, size))
            {
                return false;
            }
            auto session = mEndpoint->preferredPeer();
            if (session)
            {
                return session->sendMessage(&msg);
            }
        }
        else if (mLogCache)
        {
            mLogCache->push(buffer, size, code);
        }
        return true;
    }
    else
    {
        auto msg = new CBaseMessage(code, this);
        if (!msg->serialize(buffer, size))
        {
            delete msg;
            return false;
        }
        msg->setCallable(std::bind(&CLogProducer::callSendLog, this, _1));
        return FDB_CONTEXT->sendAsync(msg);
    }
}

#define FDB_DO_LOG(_level_, _tag_) do{ \
    CLogProducer *logger = FDB_CONTEXT->getLogger(); \
    if (!logger) \
    { \
        return; \
    } \
    if (!logger->checkLogTraceEnabled(_level_, _tag_))\
    {\
        return;\
    }\
    char info[logger->mMaxTraceLogSize];\
    info[0] = '\0'; \
    va_list args; \
    va_start(args, tag); \
    const char *format = va_arg(args, const char *); \
    vsnprintf(info, logger->mMaxTraceLogSize, format, args);\
    va_end(args);\
    logger->logTrace(_level_, tag, info);\
}while(0)

void fdb_log_debug(const char *tag, ...)
{
    FDB_DO_LOG(FDB_LL_DEBUG, tag);
}

void fdb_log_info(const char *tag, ...)
{
    FDB_DO_LOG(FDB_LL_INFO, tag);
}

void fdb_log_warning(const char *tag, ...)
{
    FDB_DO_LOG(FDB_LL_WARNING, tag);
}

void fdb_log_error(const char *tag, ...)
{
    FDB_DO_LOG(FDB_LL_ERROR, tag);
}

void fdb_log_fatal(const char *tag, ...)
{
    FDB_DO_LOG(FDB_LL_FATAL, tag);
}

#define FDB_PRINT_LOG(_level_, _tag_) do{ \
    if (_level_ < CLogProducer::staticLogLevel()) \
    { \
        return; \
    } \
    char info[CLogProducer::mMaxTraceLogSize];\
    info[0] = '\0'; \
    va_list args; \
    va_start(args, tag); \
    const char *format = va_arg(args, const char *); \
    vsnprintf(info, CLogProducer::mMaxTraceLogSize, format, args);\
    va_end(args);\
    CLogProducer::printTrace(_level_, tag, info);\
}while(0)

void fdb_print_debug(const char *tag, ...)
{
    FDB_PRINT_LOG(FDB_LL_DEBUG, tag);
}

void fdb_print_info(const char *tag, ...)
{
    FDB_PRINT_LOG(FDB_LL_INFO, tag);
}

void fdb_print_warning(const char *tag, ...)
{
    FDB_PRINT_LOG(FDB_LL_WARNING, tag);
}

void fdb_print_error(const char *tag, ...)
{
    FDB_PRINT_LOG(FDB_LL_ERROR, tag);
}

void fdb_print_fatal(const char *tag, ...)
{
    FDB_PRINT_LOG(FDB_LL_FATAL, tag);
}

