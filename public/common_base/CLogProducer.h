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
#include "CBaseMutexLock.h"

class CBaseEndpoint;
namespace google
{
    namespace protobuf
    {
        template <typename Element> class RepeatedPtrField;
    }
}
class CLogProducer : public CBaseClient
{
public:
    CLogProducer();
    void logMessage(CFdbMessage *msg, CBaseEndpoint *endpoint);
    void logTrace(EFdbLogLevel log_level, const char *tag, const char *format, ...);

    bool checkLogEnabled(EFdbMessageType type,
                         const char *sender_name,
                         const CBaseEndpoint *endpoint,
                         bool lock = true);
    bool checkLogEnabled(const CFdbMessage *msg, const CBaseEndpoint *endpoint, bool lock = true);
    
    bool printToString(std::string *str_msg, const CFdbBasePayload &pb_msg);

    static std::string mTagName;
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
    
    uint32_t mFlag;
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
    CBaseMutexLock mTraceLock;

    static const int32_t mMaxTraceLogSize = 4096;
    bool checkHostEnabled(const ::google::protobuf::RepeatedPtrField< ::std::string> &host_tbl);
    void populateWhiteList(const ::google::protobuf::RepeatedPtrField< ::std::string> &in_filter
                         , tFilterTbl &white_list);
};
#endif
