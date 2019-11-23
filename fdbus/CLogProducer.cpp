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

#include <common_base/CFdbSession.h>
#include <common_base/CFdbMessage.h>
#include <common_base/CLogProducer.h>
#include <common_base/CBaseThread.h>
#include <common_base/CFdbContext.h>
#include <common_base/CIntraNameProxy.h>
#include <common_base/CFdbSimpleMsgBuilder.h>
#include <google/protobuf/text_format.h>
#include <idl-gen/common.base.MessageHeader.pb.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <utils/Log.h>

std::string CLogProducer::mTagName = "Unknown";

CLogProducer::CLogProducer()
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
{
}

void CLogProducer::onOnline(FdbSessionId_t sid, bool is_first)
{
    CFdbMsgSubscribeList subscribe_list;
    addNotifyItem(subscribe_list, NFdbBase::NTF_LOGGER_CONFIG);
    addNotifyItem(subscribe_list, NFdbBase::NTF_TRACE_CONFIG);
    subscribe(subscribe_list);
}

void CLogProducer::onOffline(FdbSessionId_t sid, bool is_last)
{
}

void CLogProducer::onBroadcast(CBaseJob::Ptr &msg_ref)
{
    CFdbMessage *msg = castToMessage<CFdbMessage *>(msg_ref);
    switch (msg->code())
    {
        case NFdbBase::NTF_LOGGER_CONFIG:
        {
            NFdbBase::FdbMsgLogConfig cfg;
            if (!msg->deserialize(cfg))
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
            CAutoLock _l(mTraceLock);
            populateWhiteList(cfg.endpoint_white_list(), mLogEndpointWhiteList);
            populateWhiteList(cfg.busname_white_list(), mLogBusnameWhiteList);
        }
        break;
        case NFdbBase::NTF_TRACE_CONFIG:
        {
            NFdbBase::FdbTraceConfig cfg;
            if (!msg->deserialize(cfg))
            {
                LOG_E("CLogProducer: Unable to deserialize FdbTraceConfig!\n");
                return;
            }
            mLogLevel = (EFdbLogLevel)cfg.log_level();
            mTraceDisableGlobal = !cfg.global_enable();
            mTraceHostEnabled = checkHostEnabled(cfg.host_white_list());
            CAutoLock _l(mTraceLock);
            populateWhiteList(cfg.tag_white_list(), mTraceTagWhiteList);
        }
        break;
        default:
        break;
    }
}

const char *CLogProducer::getReceiverName(EFdbMessageType type,
                                          const char *sender_name,
                                          const CBaseEndpoint *endpoint)
{
    const char *receiver;
    
    switch (type)
    {
        case FDB_MT_REQUEST:
        case FDB_MT_SUBSCRIBE_REQ:
        case FDB_MT_SIDEBAND_REQUEST:
            receiver = endpoint->nsName().c_str();
        break;
        default:
            if (!sender_name || (sender_name[0] == '\0'))
            {
                receiver = "__ANY__";
            }
            else
            {
                receiver = sender_name;
            }
        break;
    }

    return receiver;
}

bool CLogProducer::checkLogEnabledGlobally()
{
    return getSessionCount() && !mLoggerDisableGlobal && mLogHostEnabled;
}

bool CLogProducer::checkLogEnabledByMessageType(EFdbMessageType type)
{
    bool match = true;
    switch (type)
    {
        case FDB_MT_REQUEST:
        case FDB_MT_SIDEBAND_REQUEST:
            if (mDisableRequest)
            {
                match = false;
            }
        break;
        case FDB_MT_REPLY:
        case FDB_MT_STATUS:
        case FDB_MT_SIDEBAND_REPLY:
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

bool CLogProducer::checkLogEnabledByEndpoint(const char *sender, const char *receiver, const char *busname)
{
    if (!mLogEndpointWhiteList.empty())
    {
        if ((mLogEndpointWhiteList.find(sender) == mLogEndpointWhiteList.end()) &&
            (mLogEndpointWhiteList.find(receiver) == mLogEndpointWhiteList.end()))
        {
            return false;
        }
    }
    if (!mLogBusnameWhiteList.empty())
    {
        if (mLogBusnameWhiteList.find(busname) == mLogBusnameWhiteList.end())
        {
            return false;
        }
    }

    return true;
}

bool CLogProducer::checkLogEnabled(EFdbMessageType type,
                                    const char *sender_name,
                                    const CBaseEndpoint *endpoint,
                                    bool lock)
{
    if (!checkLogEnabledGlobally())
    {
        return false;
    }
    
    if (!checkLogEnabledByMessageType(type))
    {
        return false;
    }

    if (!endpoint)
    {
        return true;
    }

    const char *sender = endpoint->name().c_str();
    const char *receiver = getReceiverName(type, sender_name, endpoint);
    const char *busname = endpoint->nsName().c_str();
    
    if (lock)
    {
        CAutoLock _l(mTraceLock);
        return checkLogEnabledByEndpoint(sender, receiver, busname);
    }
    else
    {
        return checkLogEnabledByEndpoint(sender, receiver, busname);
    }
}

bool CLogProducer::checkLogEnabled(const CFdbMessage *msg, const CBaseEndpoint *endpoint, bool lock)
{
    return checkLogEnabled(msg->type(), msg->senderName().c_str(), endpoint, lock);
}

void CLogProducer::logMessage(CFdbMessage *msg, CBaseEndpoint *endpoint)
{
    if (!msg->isLogEnabled())
    {
        return;
    }
    
    const char *sender = endpoint->name().c_str();
    const char *receiver = getReceiverName(msg->type(), msg->senderName().c_str(), endpoint);
    const char *busname = endpoint->nsName().c_str();
    CIntraNameProxy *proxy = FDB_CONTEXT->getNameProxy();

    CFdbSimpleMsgBuilder builder;
    builder.serializer() << mPid
                         << (proxy ? proxy->hostName().c_str() : "Unknown")
                         << sender
                         << receiver
                         << busname
                         << msg->type()
                         << msg->code()
                         << sysdep_getsystemtime_milli()
                         << msg->getPayloadSize()
                         << msg->sn()
                         << msg->objectId()
                         ;

    if (msg->mStringData)
    {
        builder.serializer() << true << *(msg->mStringData);
        delete msg->mStringData;
        msg->mStringData = 0;
        sendLogNoQueue(NFdbBase::REQ_FDBUS_LOG, builder);
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
        sendLogNoQueue(NFdbBase::REQ_FDBUS_LOG, builder);
    }
}

void CLogProducer::logTrace(EFdbLogLevel log_level, const char *tag, const char *format, ...)
{
    if (!getSessionCount()
        || mTraceDisableGlobal
        || (log_level < mLogLevel)
        || (log_level >= FDB_LL_SILENT)
        || !mTraceHostEnabled)
    {
        return;
    }

    if (tag)
    {
        if (!mTraceTagWhiteList.empty())
        {
            CAutoLock _l(mTraceLock);
            if (!mTraceTagWhiteList.empty() && (mTraceTagWhiteList.find(tag) == mTraceTagWhiteList.end()))
            {
                return;
            }
        }
    }
    else
    {
        tag = "None";
    }

    CIntraNameProxy *proxy = FDB_CONTEXT->getNameProxy();
    CFdbSimpleMsgBuilder builder;
    builder.serializer() << mPid
                         << tag
                         << (proxy ? proxy->hostName().c_str() : "Unknown")
                         << sysdep_getsystemtime_milli()
                         << log_level
                         ;

    char buffer[mMaxTraceLogSize];
    buffer[0] = '\0';
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, mMaxTraceLogSize, format, args);
    va_end(args);
    builder.serializer() << buffer;

    sendLog(NFdbBase::REQ_TRACE_LOG, builder);
}

bool CLogProducer::printToString(std::string *str_msg, const CFdbBasePayload &pb_msg)
{
    bool ret = true;
    try
    {
        const ::google::protobuf::Message &full_msg = dynamic_cast<const ::google::protobuf::Message &>(pb_msg);
        try
        {
            if (!google::protobuf::TextFormat::PrintToString(full_msg, str_msg))
            {
                ret = false;
            }
        }
        catch (...)
        {
            ret = false;
        }
    }
    catch (std::bad_cast exp)
    {
        str_msg->assign("Lite version does not support logging. You can remove \"option optimize_for = LITE_RUNTIME;\" from .proto file.\n");
    }

    return ret;
}

bool CLogProducer::checkHostEnabled(const ::google::protobuf::RepeatedPtrField< ::std::string> &host_tbl)
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

    for (::google::protobuf::RepeatedPtrField< ::std::string>::const_iterator it = host_tbl.begin();
            it != host_tbl.end(); ++it)
    {
        if (!it->compare(proxy->hostName()))
        {
            return true;
        }
    }

    return false;
}

void CLogProducer::populateWhiteList(const ::google::protobuf::RepeatedPtrField< ::std::string> &in_filter
                                   , tFilterTbl &white_list)
{
    white_list.clear();
    for (::google::protobuf::RepeatedPtrField< ::std::string>::const_iterator it = in_filter.begin();
            it != in_filter.end(); ++it)
    {
        white_list.insert(*it);
    }
}

