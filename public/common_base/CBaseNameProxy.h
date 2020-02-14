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

#ifndef _CBASENAMEPROXY_H_
#define _CBASENAMEPROXY_H_

#include "common_defs.h"
#include "CBaseClient.h"
#include "CFdbIfNameServer.h"

class CBaseNameProxy : public CBaseClient
{
public:
    CBaseNameProxy();

    virtual bool connectToNameServer() {return false;}
    virtual void addServiceListener(const char *svc_name, FdbSessionId_t subscriber) {}
    virtual void removeServiceListener(const char *svc_name) {}
    virtual void addServiceMonitorListener(const char *svc_name, FdbSessionId_t subscriber) {}
    virtual void removeServiceMonitorListener(const char *svc_name) {}

protected:
    void subscribeListener(NFdbBase::FdbNsMsgCode code, const char *svc_name);
    void unsubscribeListener(NFdbBase::FdbNsMsgCode code, const char *svc_name);
    void replaceSourceUrl(NFdbBase::FdbMsgAddressList &msg_addr_list, CFdbSession *session);
};

#endif
