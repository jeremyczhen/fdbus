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

#ifndef _CINTERNAMEPROXY_H_
#define _CINTERNAMEPROXY_H_

#include <string>
#include <set>
#include "CBaseNameProxy.h"
#include "CFdbIfNameServer.h"

class CHostProxy;
class CQueryServiceMsg : public CFdbMessage
{
public:
    typedef std::list<std::string> tPendingReqTbl;
    
    CQueryServiceMsg(tPendingReqTbl *pending_tbl,
                     NFdbBase::FdbMsgServiceTable *svc_tbl,
                     CBaseJob::Ptr &req,
                     const std::string &host_ip,
                     CHostProxy *host_proxy);
    ~CQueryServiceMsg();
    
    tPendingReqTbl *mPendingReqTbl;
    NFdbBase::FdbMsgServiceTable *mSvcTbl;
    CBaseJob::Ptr mReq;
    std::string mHostIp;
    CHostProxy *mHostProxy;
protected:
    void onAsyncError(Ptr &ref, NFdbBase::FdbMsgStatusCode code, const char *reason);
};

class CInterNameProxy : public CBaseNameProxy
{
public:
    CInterNameProxy(CHostProxy *host_proxy,
                    const std::string &host_ip,
                    const std::string &ns_url,
                    const std::string &host_name);
    ~CInterNameProxy();
    void addServiceListener(const char *svc_name, FdbSessionId_t subscriber);
    void removeServiceListener(const char *svc_name);
    void addServiceMonitorListener(const char *svc_name, FdbSessionId_t subscriber);
    void removeServiceMonitorListener(const char *svc_name);
    const std::string &getHostIp() const
    {
        return mIpAddress;
    }
    bool connectToNameServer();
    void queryServiceTbl(CQueryServiceMsg *msg);

    const std::string &getNsUrl() const
    {
        return mNsUrl;
    }
    const std::string &getHostName() const
    {
        return mHostName;
    }
protected:
    void onBroadcast(CBaseJob::Ptr &msg_ref);
    void onReply(CBaseJob::Ptr &msg_ref);
    CHostProxy *mHostProxy;
    void subscribeListener(NFdbBase::FdbNsMsgCode code
                         , const char *svc_name
                         , FdbSessionId_t subscriber);
private:
    std::string mIpAddress;
    std::string mNsUrl;
    std::string mHostName;
};

#endif
