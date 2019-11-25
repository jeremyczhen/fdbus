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

#include <common_base/CInterNameProxy.h>
#include <common_base/CFdbContext.h>
#include <common_base/CFdbMessage.h>
#include <common_base/CFdbSession.h>
#include "CHostProxy.h"
#include "CNameServer.h"
#include <idl-gen/common.base.MessageHeader.pb.h>
#include <utils/Log.h>

class CServiceSubscribeMsg : public CFdbMessage
{
public:
    CServiceSubscribeMsg(FdbSessionId_t subscriber)
        : CFdbMessage()
        , mSubscriber(subscriber)
    {
    }
    FdbSessionId_t getSubscriber() const
    {
        return mSubscriber;
    }
private:
    FdbSessionId_t mSubscriber;
};

CQueryServiceMsg::CQueryServiceMsg(tPendingReqTbl *pending_tbl,
                                    NFdbBase::FdbMsgServiceTable *svc_tbl,
                                    CBaseJob::Ptr &req,
                                    const std::string &host_ip,
                                    CHostProxy *host_proxy)
    : CFdbMessage(NFdbBase::REQ_QUERY_SERVICE_INTER_MACHINE)
    , mPendingReqTbl(pending_tbl)
    , mSvcTbl(svc_tbl)
    , mReq(req)
    , mHostIp(host_ip)
    , mHostProxy(host_proxy)
{
}

CQueryServiceMsg::~CQueryServiceMsg()
{
}

void CQueryServiceMsg::onAsyncError(Ptr &ref, NFdbBase::FdbMsgStatusCode code, const char *reason)
{
    mHostProxy->finalizeServiceQuery(0, this);
}

CInterNameProxy::CInterNameProxy(CHostProxy *host_proxy,
                                 const std::string &host_ip,
                                 const std::string &ns_url,
                                 const std::string &host_name)
    : mHostProxy(host_proxy)
    , mIpAddress(host_ip)
    , mNsUrl(ns_url)
    , mHostName(host_name)
{
}

CInterNameProxy::~CInterNameProxy()
{
    mHostProxy->deleteNameProxy(this);
}

bool CInterNameProxy::connectToNameServer()
{
    if (connected())
    {
        return true;
    }
    if (mNsUrl.empty())
    {
        LOG_E("CInterNameProxy: Unable to connect to name server since URL is not set.\n");
        return false;
    }
    
    doConnect(mNsUrl.c_str());
    if (!connected())
    {
        LOG_E("CInterNameProxy: Unable to connect to name server %s.\n", mNsUrl.c_str());
        return false;
    }

    return true;
}

void CInterNameProxy::addServiceListener(const char *svc_name, FdbSessionId_t subscriber)
{
    if (connected())
    {
        subscribeListener(NFdbBase::NTF_SERVICE_ONLINE_INTER_MACHINE, svc_name, subscriber);
    }
}

void CInterNameProxy::removeServiceListener(const char *svc_name)
{
    if (connected())
    {
        unsubscribeListener(NFdbBase::NTF_SERVICE_ONLINE_INTER_MACHINE, svc_name);
    }
} 

void CInterNameProxy::addServiceMonitorListener(const char *svc_name, FdbSessionId_t subscriber)
{
    if (connected())
    {
        subscribeListener(NFdbBase::NTF_SERVICE_ONLINE_MONITOR_INTER_MACHINE, svc_name, subscriber);
    }
}

void CInterNameProxy::removeServiceMonitorListener(const char *svc_name)
{
    if (connected())
    {
        unsubscribeListener(NFdbBase::NTF_SERVICE_ONLINE_MONITOR_INTER_MACHINE, svc_name);
    }
}

void CInterNameProxy::onBroadcast(CBaseJob::Ptr &msg_ref)
{
    CFdbMessage *msg = castToMessage<CFdbMessage *>(msg_ref);
    CNameServer *name_server = mHostProxy->nameServer();
    switch (msg->code())
    {
        case NFdbBase::NTF_SERVICE_ONLINE_INTER_MACHINE:
        case NFdbBase::NTF_SERVICE_ONLINE_MONITOR_INTER_MACHINE:
        {
            NFdbBase::FdbMsgAddressList msg_addr_list;
            if (!msg->deserialize(msg_addr_list))
            {
                return;
            }

            FdbMsgCode_t code = (msg->code() == NFdbBase::NTF_SERVICE_ONLINE_INTER_MACHINE) ?
                                NFdbBase::NTF_SERVICE_ONLINE :
                                NFdbBase::NTF_SERVICE_ONLINE_MONITOR;
            tSubscribedSessionSets sessions;
            const char *svc_name = msg_addr_list.service_name().c_str();
            name_server->getSubscribeTable(code, svc_name, sessions);
            if (sessions.empty())
            {
                // if no client is connected to the server, unsubscrie it from
                // name server of other hosts.
                mHostProxy->recallServiceListener(msg->code(), svc_name);
                return;
            }

            // now show forward appear/disappear of the server to local clients
            CFdbSession *session = FDB_CONTEXT->getSession(msg->session());
            if (session)
            {
                replaceSourceUrl(msg_addr_list, session);
            }

            FdbSessionId_t subscriber = FDB_INVALID_ID;
            if (session)
            {
                CFdbMessage *out_going_msg = session->peepPendingMessage(msg->sn());
                if (out_going_msg)
                {
                    CServiceSubscribeMsg *svc_sub_msg =
                                    dynamic_cast<CServiceSubscribeMsg *>(out_going_msg);
                    if (svc_sub_msg)
                    {
                        subscriber = svc_sub_msg->getSubscriber();
                    }
                }
            }
            
            if (code == NFdbBase::NTF_SERVICE_ONLINE)
            {
                CFdbToken::tTokenList tokens;
                name_server->dumpTokens(tokens, msg_addr_list);
                bool broadcast_to_all = true;
                if (isValidFdbId(subscriber))
                {
                    CFdbSession *session = FDB_CONTEXT->getSession(subscriber);
                    if (session)
                    {
                        // send token matching security level to the client
                        name_server->broadcastSvcAddrLocal(tokens, msg_addr_list, session);
                        broadcast_to_all = false;
                    }
                }
                if (broadcast_to_all)
                {
                    name_server->broadcastSvcAddrLocal(tokens, msg_addr_list);
                }
            }
            else
            {
                // never broadcast token to monitors!!!
                msg_addr_list.mutable_token_list()->clear_tokens();
                msg_addr_list.mutable_token_list()->set_crypto_algorithm(NFdbBase::CRYPTO_NONE);
                name_server->broadcast(subscriber, FDB_OBJECT_MAIN, code,
                                msg_addr_list, msg_addr_list.service_name().c_str());
            }
        }
        break;
        default:
        break;
    }
}

void CInterNameProxy::queryServiceTbl(CQueryServiceMsg *msg)
{
    invoke(msg, 0, 0, 5000);
}

void CInterNameProxy::onReply(CBaseJob::Ptr &msg_ref)
{
    CQueryServiceMsg *msg = castToMessage<CQueryServiceMsg *>(msg_ref);
    if (!msg)
    {
        LOG_E("CInterNameProxy: reply message cannot be casted to CQueryServiceMsg!\n");
        return;
    }
    if (msg->isStatus())
    {
        if (msg->isError())
        {
            int32_t id;
            std::string reason;
            msg->decodeStatus(id, reason);
            LOG_I("CInterNameProxy: status is received: msg code: %d, id: %d, reason: %s\n", msg->code(), id, reason.c_str());
        }

        return;
    }

    switch (msg->code())
    {
        case NFdbBase::REQ_QUERY_SERVICE_INTER_MACHINE:
        {
            NFdbBase::FdbMsgServiceTable svc_tbl;
            if (!msg->deserialize(svc_tbl))
            {
                return;
            }
            ::google::protobuf::RepeatedPtrField< ::NFdbBase::FdbMsgServiceInfo> *svc_list =
                svc_tbl.mutable_service_tbl();
            for (::google::protobuf::RepeatedPtrField< ::NFdbBase::FdbMsgServiceInfo>::iterator it = svc_list->begin();
                    it != svc_list->end(); ++it)
            {
                replaceSourceUrl(*it->mutable_service_addr(),
                                 FDB_CONTEXT->getSession(msg->session()));
            }
            mHostProxy->finalizeServiceQuery(&svc_tbl, msg);
        }
        break;
        default:
        break;
    }
}

void CInterNameProxy::subscribeListener(NFdbBase::FdbNsMsgCode code
                                      , const char *svc_name
                                      , FdbSessionId_t subscriber)
{
    if (isValidFdbId(subscriber))
    {
        CFdbMsgSubscribeList subscribe_list;
        addNotifyItem(subscribe_list, code, svc_name);
        CFdbMessage *msg = new CServiceSubscribeMsg(subscriber);
        subscribe(subscribe_list, msg);
    }
    else
    {
        CBaseNameProxy::subscribeListener(code, svc_name);
    }
}

