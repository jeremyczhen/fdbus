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

#ifndef _CHOSTPROXY_H_
#define _CHOSTPROXY_H_

#include <map>
#include <common_base/CBaseClient.h>
#include <common_base/CMethodLoopTimer.h>
#include <common_base/CFdbIfNameServer.h>
#include <utils/CNsConfig.h>

class CInterNameProxy;
class CNameServer;
class CQueryServiceMsg;
class CHostProxy : public CBaseClient
{
public:
    CHostProxy(CNameServer *ns, const char *host_name = "");
    ~CHostProxy();
    void setHostUrl(const char *url)
    {
        mHostUrl = url;
    }
    bool connectToHostServer();

    void forwardServiceListener(FdbMsgCode_t msg_code
                              , const char *service_name
                              , FdbSessionId_t subscriber);
    void recallServiceListener(FdbMsgCode_t msg_code, const char *service_name);
    CNameServer *nameServer()
    {
        return mNameServer;
    }
    void hostOffline();
    std::string &hostName() /* local */
    {
        return mHostName;
    }
    std::string &hostUrl()  /* remote */
    {
        return mHostUrl;
    }
    void deleteNameProxy(CInterNameProxy *proxy);
    void queryServiceReq(CBaseJob::Ptr &msg_ref);
    void finalizeServiceQuery(NFdbBase::FdbMsgServiceTable *svc_tbl, CQueryServiceMsg *query);

    void getHostTbl(NFdbBase::FdbMsgHostAddressList &host_tbl);
protected:
    void onReply(CBaseJob::Ptr &msg_ref);
    void onBroadcast(CBaseJob::Ptr &msg_ref);
    void onOnline(FdbSessionId_t sid, bool is_first);
    void onOffline(FdbSessionId_t sid, bool is_last);
private:
    typedef std::map<std::string, CInterNameProxy *> tNameProxyTbl;
    tNameProxyTbl mNameProxyTbl;
    CNameServer *mNameServer;
    CFdbMessageHandle<CHostProxy> mNotifyHdl;
    std::string mHostName;
    std::string mHostUrl;

    void subscribeListener(NFdbBase::FdbHsMsgCode code, bool sub);
    void onHostOnlineNotify(CBaseJob::Ptr &msg_ref);
    void onHeartbeatOk(CBaseJob::Ptr &msg_ref);
    void hostOnline();
    void hostOnline(FdbMsgCode_t code);
    void isolate();
    void isolate(tNameProxyTbl::iterator &it);

    class CConnectTimer : public CMethodLoopTimer<CHostProxy>
    {
    public:
        enum eWorkMode
        {
            MODE_RECONNECT_TO_SERVER,
            MODE_HEARTBEAT,
            MODE_IDLE
        };
        CConnectTimer(CHostProxy *proxy)
            : CMethodLoopTimer<CHostProxy>(CNsConfig::getHsReconnectInterval(), false,
                                           proxy, &CHostProxy::onConnectTimer)
            , mMode(MODE_IDLE)
        {
        }
        void startHeartbeatMode()
        {
            mMode = MODE_HEARTBEAT;
            enableOneShot(CNsConfig::getHeartBeatTimeout());
        }
        void startReconnectMode()
        {
            mMode = MODE_RECONNECT_TO_SERVER;
            enableOneShot(CNsConfig::getHsReconnectInterval());
        }
        eWorkMode mMode;
    };
    CConnectTimer mConnectTimer;

    void onConnectTimer(CMethodLoopTimer<CHostProxy> *timer);
    bool importTokens(const NFdbBase::FdbMsgHostAddress &host_addr,
                                        CBaseEndpoint *endpoint);
};

#endif
