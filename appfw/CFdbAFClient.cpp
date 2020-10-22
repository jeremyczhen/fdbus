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
    
#include <common_base/CFdbAFClient.h>
#include <common_base/CFdbAFComponent.h>

#define FDB_MSG_TYPE_AFC_SUBSCRIBE (FDB_MSG_TYPE_SYSTEM + 1)
#define FDB_MSG_TYPE_AFC_INVOKE (FDB_MSG_TYPE_SYSTEM + 2)

class CAFCSubscribeMsg : public CFdbMessage
{
public:
    CAFCSubscribeMsg(CFdbAFComponent *appfw_handle)
        : CFdbMessage()
        , mAppFWHandle(appfw_handle)
    {
    }
    CAFCSubscribeMsg(NFdbBase::CFdbMessageHeader &head
                      , CFdbMsgPrefix &prefix
                      , uint8_t *buffer
                      , FdbSessionId_t sid
                      , CFdbAFComponent *appfw_handle)
        : CFdbMessage(head, prefix, buffer, sid)
        , mAppFWHandle(appfw_handle)
    {
    }
    FdbMessageType_t getTypeId()
    {
        return FDB_MSG_TYPE_AFC_SUBSCRIBE;
    }

    CFdbAFComponent *mAppFWHandle;
protected:
    CFdbMessage *clone(NFdbBase::CFdbMessageHeader &head
                      , CFdbMsgPrefix &prefix
                      , uint8_t *buffer
                      , FdbSessionId_t sid)
    {
        return new CAFCSubscribeMsg(head, prefix, buffer, sid, mAppFWHandle);
    }
};

class CAFCInvokeMsg : public CFdbMessage
{
public:
    CAFCInvokeMsg(FdbMsgCode_t code, CFdbAFClient::tInvokeCallbackFn &reply_callback)
        : CFdbMessage(code)
        , mReplyCallback(reply_callback)
    {
    }
    FdbMessageType_t getTypeId()
    {
        return FDB_MSG_TYPE_AFC_INVOKE;
    }
    CFdbAFClient::tInvokeCallbackFn mReplyCallback;
};

CFdbAFClient::tRegEntryId CFdbAFClient::registerConnNotification(CFdbAFClient::tConnCallbackFn callback)
{
    CFdbAFClient::tRegEntryId id = mRegIdAllocator++;
    mConnCallbackTbl[id] =callback;
    if (connected())
    {
        callback(this, true);
    }
    return id;
}

void CFdbAFClient::onOnline(FdbSessionId_t sid, bool is_first)
{
    CFdbEventDispatcher::tEvtHandleTbl events;
    mEvtDispather.dumpEvents(events);
    subscribeEvents(events, 0);
    
    for (auto it = mConnCallbackTbl.begin(); it != mConnCallbackTbl.end(); ++it)
    {
        (it->second)(this, true);
    }
}
void CFdbAFClient::onOffline(FdbSessionId_t sid, bool is_last)
{
    for (auto it = mConnCallbackTbl.begin(); it != mConnCallbackTbl.end(); ++it)
    {
        (it->second)(this, false);
    }
}
void CFdbAFClient::onBroadcast(CBaseJob::Ptr &msg_ref)
{
    const CFdbEventDispatcher::tRegistryHandleTbl *registered_evt_tbl = 0;
    auto *msg = castToMessage<CBaseMessage *>(msg_ref);
    if (msg->isInitialResponse() && (msg->getTypeId() == FDB_MSG_TYPE_AFC_SUBSCRIBE))
    {
        auto afc_msg = castToMessage<CAFCSubscribeMsg *>(msg_ref);
        if (afc_msg->mAppFWHandle)
        {
            registered_evt_tbl = &afc_msg->mAppFWHandle->getEventRegistryTbl();
        }
    }
    mEvtDispather.processMessage(msg_ref, this, registered_evt_tbl);
}
void CFdbAFClient::onReply(CBaseJob::Ptr &msg_ref)
{
    auto *msg = castToMessage<CBaseMessage *>(msg_ref);
    if (msg->getTypeId() == FDB_MSG_TYPE_AFC_INVOKE)
    {
        auto afc_msg = castToMessage<CAFCInvokeMsg *>(msg_ref);
        afc_msg->mReplyCallback(msg_ref, this);
    }
}

bool CFdbAFClient::subscribeEvents(const CFdbEventDispatcher::tEvtHandleTbl &events,
                                   CFdbAFComponent *component)
{
    CFdbMsgSubscribeList subscribe_list;
    for (auto it = events.begin(); it != events.end(); ++it)
    {
        addNotifyItem(subscribe_list, it->mCode, it->mTopic.c_str());
    }
    return subscribe(subscribe_list, new CAFCSubscribeMsg(component));
}

bool CFdbAFClient::registerEventHandle(const CFdbEventDispatcher::CEvtHandleTbl &evt_tbl,
                                       CFdbAFComponent *component)
{
    mEvtDispather.registerCallback(evt_tbl, &component->getEventRegistryTbl());

    if (connected())
    {
        subscribeEvents(evt_tbl.getEvtHandleTbl(), component);
    }
    return true;
}

bool CFdbAFClient::invoke(FdbMsgCode_t code, IFdbMsgBuilder &data
                          , CFdbAFClient::tInvokeCallbackFn callback, int32_t timeout)
{
    auto invoke_msg = new CAFCInvokeMsg(code, callback);
    return CFdbBaseObject::invoke(invoke_msg, data, timeout);
}

bool CFdbAFClient::invoke(FdbMsgCode_t code, CFdbAFClient::tInvokeCallbackFn callback
                          , const void *buffer, int32_t size, int32_t timeout, const char *log_info)
{
    auto invoke_msg = new CAFCInvokeMsg(code, callback);
    invoke_msg->setLogData(log_info);
    return CFdbBaseObject::invoke(invoke_msg, buffer, size, timeout);
}

