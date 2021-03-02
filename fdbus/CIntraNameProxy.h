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

#ifndef _CINTRANAMEPROXY_H_
#define _CINTRANAMEPROXY_H_

#include <vector>
#include <string>
#include <common_base/CMethodLoopTimer.h>
#include <common_base/CNotificationCenter.h>
#include <common_base/CFdbContext.h>
#include <common_base/CMethodJob.h>
#include <utils/CBaseNameProxy.h>

class CFdbBaseContext;

class CIntraNameProxy : public CBaseNameProxy
{
public:
    struct CHostNameReady
    {
        CHostNameReady(std::string &host_name)
            : mHostName(host_name)
        {
        }
        std::string &mHostName;
    };
    CIntraNameProxy();
    void listenOnService(const char *svc_name, CBaseEndpoint *endpoint);
    void listenOnService(const char *svc_name, FdbContextId_t ctx_id, FdbEndpointId_t ep_id);
    void removeServiceListener(const char *svc_name);
    void addAddressListener(const char *svc_name);
    void removeAddressListener(const char *svc_name);
    void registerService(const char *svc_name, CBaseEndpoint *endpoint);
    void unregisterService(const char *svc_name);
    bool connectToNameServer();
    std::string &hostName()
    {
        return mHostName;
    }
    void registerHostNameReadyNotify(CBaseNotification<CHostNameReady> *notification);
    void enableNsMonitor(bool enb)
    {
        mEnableReconnectToNS = enb;
    }
    void registerNsWatchdogListener(tNsWatchdogListenerFn &watchdog_listener);
    
protected:
    void onReply(CBaseJob::Ptr &msg_ref);
    void onBroadcast(CBaseJob::Ptr &msg_ref);
    void onOnline(FdbSessionId_t sid, bool is_first);
    void onOffline(FdbSessionId_t sid, bool is_last);
    
private:
    std::string mHostName;
    class CConnectTimer : public CMethodLoopTimer<CIntraNameProxy>
    {
    public:
        CConnectTimer(CIntraNameProxy *proxy);
        void fire();
    };
    CConnectTimer mConnectTimer;
    class CHostNameNotificationCenter : public CBaseNotificationCenter<CHostNameReady>
    {
    public:
        CHostNameNotificationCenter(CIntraNameProxy *proxy)
            : CBaseNotificationCenter<CHostNameReady>()
            , mHostProxy(proxy)
        {
        }
    protected:
        bool onSubscribe(CBaseNotification<CHostNameReady>::Ptr &notification)
        {
            if (!mHostProxy->hostName().empty())
            {
                CHostNameReady name_ready(mHostProxy->hostName());
                notify(name_ready, notification);
            }
            return true;
        }
    private:
        CIntraNameProxy *mHostProxy;
    };
    CHostNameNotificationCenter mNotificationCenter;
    bool mEnableReconnectToNS;
    tNsWatchdogListenerFn mNsWatchdogListener;

    void onConnectTimer(CMethodLoopTimer<CIntraNameProxy> *timer);
    
    void doConnectToServer(CFdbBaseContext *context, FdbEndpointId_t ep_id,
                           NFdbBase::FdbMsgAddressList &msg_addr_list, bool is_init_response);
    void doBindAddress(CFdbBaseContext *context, FdbEndpointId_t ep_id,
                       NFdbBase::FdbMsgAddressList &msg_addr_list, bool force_rebind);
    void doRegisterNsWatchdogListener(tNsWatchdogListenerFn &watchdog_listener);
    void connectToServer(CFdbMessage *msg, FdbContextId_t ctx_id, FdbEndpointId_t ep_id);
    void bindAddress(CFdbMessage *msg, FdbContextId_t ctx_id, FdbEndpointId_t ep_id);
    void queryServiceAddress();
    void callConnectToServer(CBaseWorker *worker, CMethodJob<CIntraNameProxy> *job, CBaseJob::Ptr &ref);
    void callBindAddress(CBaseWorker *worker, CMethodJob<CIntraNameProxy> *job, CBaseJob::Ptr &ref);
    void callQueryServiceAddress(CBaseWorker *worker, CMethodJob<CIntraNameProxy> *job, CBaseJob::Ptr &ref);

    friend class CRegisterWatchdogJob;
    friend class CConnectToServerJob;
    friend class CBindAddressJob;
    friend class CQueryServiceAddressJob;
};

#endif
