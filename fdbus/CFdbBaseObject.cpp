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

#include <common_base/CFdbBaseObject.h>
#include <common_base/CBaseEndpoint.h>
#include <common_base/CFdbMessage.h>
#include <common_base/CBaseWorker.h>
#include <common_base/CFdbSession.h>
#include <common_base/CFdbContext.h>
#include <utils/CFdbIfMessageHeader.h>
#include <server/CFdbIfNameServer.h>
#include "CFdbWatchdog.h"
#include <utils/Log.h>
#include <string.h>

using namespace std::placeholders;

CFdbBaseObject::CFdbBaseObject(const char *name, CBaseWorker *worker, EFdbEndpointRole role)
    : mEndpoint(0)
    , mFlag(0)
    , mWatchdog(0)
    , mWorker(worker)
    , mObjId(FDB_INVALID_ID)
    , mRole(role)
    , mSid(FDB_INVALID_ID)
    , mRegIdAllocator(0)
{
    if (name)
    {
        mName = name;
    }
}

CFdbBaseObject::~CFdbBaseObject()
{
    if (mWatchdog)
    {
        delete mWatchdog;
    }
}

bool CFdbBaseObject::invoke(FdbSessionId_t receiver
                          , FdbMsgCode_t code
                          , IFdbMsgBuilder &data
                          , int32_t timeout)
{
    auto msg = new CBaseMessage(code, this, receiver);
    if (!msg->serialize(data, this))
    {
        delete msg;
        return false;
    }
    return msg->invoke(timeout);
}

bool CFdbBaseObject::invoke(FdbSessionId_t receiver
                            , CFdbMessage *msg
                            , IFdbMsgBuilder &data
                            , int32_t timeout)
{
    msg->setDestination(this, receiver);
    if (!msg->serialize(data, this))
    {
        delete msg;
        return false;
    }
    msg->enableTimeStamp(timeStampEnabled());
    return msg->invoke(timeout);
}

bool CFdbBaseObject::invoke(FdbMsgCode_t code
                            , IFdbMsgBuilder &data
                            , int32_t timeout)
{
    return invoke(FDB_INVALID_ID, code, data, timeout);
}

bool CFdbBaseObject::invoke(CFdbMessage *msg
                            , IFdbMsgBuilder &data
                            , int32_t timeout)
{
    return invoke(FDB_INVALID_ID, msg, data, timeout);
}

bool CFdbBaseObject::invoke(FdbSessionId_t receiver
                            , CBaseJob::Ptr &msg_ref
                            , IFdbMsgBuilder &data
                            , int32_t timeout)
{
    auto msg = castToMessage<CFdbMessage *>(msg_ref);
    msg->setDestination(this, receiver);
    if (!msg->serialize(data, this))
    {
       return false;
    }
    msg->enableTimeStamp(timeStampEnabled());
    return msg->invoke(msg_ref, timeout);
}

bool CFdbBaseObject::invoke(CBaseJob::Ptr &msg_ref
                            , IFdbMsgBuilder &data
                            , int32_t timeout)
{
    return invoke(FDB_INVALID_ID, msg_ref, data, timeout);
}

bool CFdbBaseObject::send(FdbSessionId_t receiver
                          , FdbMsgCode_t code
                          , IFdbMsgBuilder &data
                          , EFdbQOS qos)
{
    auto msg = new CBaseMessage(code, this, receiver, qos);
    if (!msg->serialize(data, this))
    {
        delete msg;
        return false;
    }
    return msg->send();
}

bool CFdbBaseObject::send(FdbMsgCode_t code, IFdbMsgBuilder &data, EFdbQOS qos)
{
    return send(FDB_INVALID_ID, code, data, qos);
}

bool CFdbBaseObject::send(FdbSessionId_t receiver
                         , FdbMsgCode_t code
                         , const void *buffer
                         , int32_t size
                         , EFdbQOS qos
                         , const char *log_data)
{
    auto msg = new CBaseMessage(code, this, receiver, qos);
    msg->setLogData(log_data);
    if (!msg->serialize(buffer, size, this))
    {
        delete msg;
        return false;
    }
    return msg->send();
}

bool CFdbBaseObject::send(FdbMsgCode_t code
                         , const void *buffer
                         , int32_t size
                         , EFdbQOS qos
                         , const char *log_data)
{
    return send(FDB_INVALID_ID, code, buffer, size, qos, log_data);
}

bool CFdbBaseObject::publish(FdbMsgCode_t code, IFdbMsgBuilder &data, const char *topic,
                             bool force_update, EFdbQOS qos)
{
    auto msg = new CBaseMessage(code, this, FDB_INVALID_ID, qos);
    if (!msg->serialize(data, this))
    {
        delete msg;
        return false;
    }
    if (topic)
    {
        msg->topic(topic);
    }
    msg->forceUpdate(force_update);
    return msg->publish();
}
 
bool CFdbBaseObject::publish(FdbMsgCode_t code, const void *buffer, int32_t size, const char *topic,
                             bool force_update, EFdbQOS qos, const char *log_data)
{
    auto msg = new CBaseMessage(code, this, FDB_INVALID_ID, qos);
    msg->setLogData(log_data);
    if (!msg->serialize(buffer, size, this))
    {
        delete msg;
        return false;
    }
    if (topic)
    {
        msg->topic(topic);
    }
    msg->forceUpdate(force_update);
    return msg->publish();
}

bool CFdbBaseObject::publishNoQueue(FdbMsgCode_t code, const char *topic, const void *buffer, int32_t size,
                                    const char *log_data, bool force_update, EFdbQOS qos)
{
    CBaseMessage msg(code, this, FDB_INVALID_ID, qos);
    msg.expectReply(false);
    msg.forceUpdate(force_update);
    msg.setLogData(log_data);
    if (!msg.serialize(buffer, size, this))
    {
        return false;
    }
    if (topic)
    {
        msg.topic(topic);
    }
    msg.type(FDB_MT_PUBLISH);

    auto session = mEndpoint->preferredPeer();
    if (session)
    {
        return session->sendMessage(&msg);
    }
    return false;
}

bool CFdbBaseObject::publishNoQueue(FdbMsgCode_t code, const char *topic, const void *buffer,
                                 int32_t size, CFdbSession *session)
{
    CBaseMessage msg(code, this);
    if (topic)
    {
        msg.topic(topic);
    }
    msg.expectReply(false);
    if (!msg.serialize(buffer, size, this))
    {
        return false;
    }
    msg.type(FDB_MT_PUBLISH);

    return session->sendMessage(&msg);
}

void CFdbBaseObject::publishCachedEvents(CFdbSession *session)
{
    for (auto it_events = mEventCache.begin(); it_events != mEventCache.end(); ++it_events)
    {
        auto event_code = it_events->first;
        auto &events = it_events->second;
        for (auto it_data = events.begin(); it_data != events.end(); ++it_data)
        {
            auto &filter = it_data->first;
            auto &data = it_data->second;
            publishNoQueue(event_code, filter.c_str(), data.mBuffer, data.mSize, session);
        }
    }
}

bool CFdbBaseObject::get(FdbMsgCode_t code, const char *topic, int32_t timeout)
{
    auto msg = new CBaseMessage(code, this, FDB_INVALID_ID);
    if (!msg->serialize(0, 0, this))
    {
        delete msg;
        return false;
    }
    msg->topic(topic);
    return msg->get(timeout);
}

bool CFdbBaseObject::get(CFdbMessage *msg, const char *topic, int32_t timeout)
{
    msg->setDestination(this, FDB_INVALID_ID);
    if (!msg->serialize(0, 0, this))
    {
        delete msg;
        return false;
    }
    msg->topic(topic);
    msg->enableTimeStamp(timeStampEnabled());
    return msg->get(timeout);
}

bool CFdbBaseObject::get(CBaseJob::Ptr &msg_ref, const char *topic, int32_t timeout)
{
    auto msg = castToMessage<CFdbMessage *>(msg_ref);
    msg->setDestination(this, FDB_INVALID_ID);
    if (!msg->serialize(0, 0, this))
    {
        return false;
    }
    msg->topic(topic);
    msg->enableTimeStamp(timeStampEnabled());
    return msg->get(msg_ref, timeout);
}

bool CFdbBaseObject::sendLog(FdbMsgCode_t code, IFdbMsgBuilder &data)
{
    auto msg = new CBaseMessage(code, this);
    if (!msg->serialize(data))
    {
        delete msg;
        return false;
    }
    return msg->send();
}

bool CFdbBaseObject::sendLogNoQueue(FdbMsgCode_t code, IFdbMsgBuilder &data)
{
    CBaseMessage msg(code, this, FDB_INVALID_ID);
    msg.expectReply(false);
    if (!msg.serialize(data))
    {
        return false;
    }
    auto session = mEndpoint->preferredPeer();
    if (session)
    {
        return session->sendMessage(&msg);
    }
    return false;
}

bool CFdbBaseObject::broadcast(FdbMsgCode_t code
                               , IFdbMsgBuilder &data
                               , const char *filter
                               , EFdbQOS qos)
{
    auto msg = new CFdbMessage(code, this, filter, FDB_INVALID_ID, FDB_INVALID_ID, qos);
    if (!msg->serialize(data, this))
    {
        delete msg;
        return false;
    }
    return msg->broadcast();
}

bool CFdbBaseObject::broadcast(FdbMsgCode_t code
                              , const void *buffer
                              , int32_t size
                              , const char *filter
                              , EFdbQOS qos
                              , const char *log_data)
{
    auto msg = new CFdbMessage(code, this, filter, FDB_INVALID_ID, FDB_INVALID_ID, qos);
    msg->setLogData(log_data);
    if (!msg->serialize(buffer, size, this))
    {
        delete msg;
        return false;
    }
    return msg->broadcast();
}

void CFdbBaseObject::broadcastLogNoQueue(FdbMsgCode_t code, const uint8_t *data, int32_t size,
                                         const char *filter)
{
    CFdbMessage msg(code, this, filter, FDB_INVALID_ID, FDB_INVALID_ID);
    if (!msg.serialize(data, size, this))
    {
        return;
    }
    msg.enableLog(false);

    broadcast(&msg);
}

void CFdbBaseObject::broadcastNoQueue(FdbMsgCode_t code, const uint8_t *data, int32_t size,
                                      const char *filter, bool force_update, EFdbQOS qos)
{
    CFdbMessage msg(code, this, filter, FDB_INVALID_ID, FDB_INVALID_ID, qos);
    if (!msg.serialize(data, size, this))
    {
        return;
    }
    msg.forceUpdate(force_update);

    broadcast(&msg);
}

bool CFdbBaseObject::unsubscribe(CFdbMsgSubscribeList &msg_list)
{
    auto msg = new CBaseMessage(FDB_INVALID_ID, this);
    msg->type(FDB_MT_SUBSCRIBE_REQ);
    CFdbParcelableBuilder builder(msg_list);
    if (!msg->serialize(builder, this))
    {
        delete msg;
        return false;
    }
    return msg->unsubscribe();
}

bool CFdbBaseObject::unsubscribe()
{
    CFdbMsgSubscribeList msg_list;
    return unsubscribe(msg_list);
}

void CFdbBaseObject::migrateToWorker(CBaseJob::Ptr &msg_ref, tRemoteCallback callback, CBaseWorker *worker)
{
    if (!worker)
    {
        worker = mWorker;
    }
    if (worker)
    {
        if (enableMigrate())
        {
            auto msg = castToMessage<CBaseMessage *>(msg_ref);
            msg->setCallable(std::bind(callback, this, _1));
            if (!worker->sendAsync(msg_ref))
            {
            }
        }
    }
    else
    {
        try
        {
            (this->*callback)(msg_ref);
        }
        catch (...)
        {
        }
    }
}

class COnOnlineJob : public CMethodJob<CFdbBaseObject>
{
public:
    COnOnlineJob(CFdbBaseObject *object, FdbSessionId_t sid, bool first_or_last, bool online)
        : CMethodJob<CFdbBaseObject>(object, &CFdbBaseObject::callOnOnline, JOB_FORCE_RUN)
        , mSid(sid)
        , mFirstOrLast(first_or_last)
        , mOnline(online)
    {}

    FdbSessionId_t mSid;
    bool mFirstOrLast;
    bool mOnline;
};

void CFdbBaseObject::callOnOnline(CBaseWorker *worker, CMethodJob<CFdbBaseObject> *job, CBaseJob::Ptr &ref)
{
    auto the_job = fdb_dynamic_cast_if_available<COnOnlineJob *>(job);
    if (the_job)
    {
        callOnline(the_job->mSid, the_job->mFirstOrLast, the_job->mOnline);
    }
}

void CFdbBaseObject::migrateToWorker(FdbSessionId_t sid, bool first_or_last, bool online, CBaseWorker *worker)
{
    if (!worker)
    {
        worker = mWorker;
    }
    if (worker)
    {
        if (enableMigrate())
        {
            worker->sendAsync(new COnOnlineJob(this, sid, first_or_last, online));
        }
    }
    else
    {
        callOnline(sid, first_or_last, online);
    }
}

void CFdbBaseObject::callSubscribe(CBaseJob::Ptr &msg_ref)
{
    try // catch exception to avoid missing of auto-reply
    {
        onSubscribe(msg_ref);
    }
    catch (...)
    {
    }
    CFdbMessage::autoReply(msg_ref, NFdbBase::FDB_ST_AUTO_REPLY_OK, "Automatically reply to subscribe request.");
}

void CFdbBaseObject::callBroadcast(CBaseJob::Ptr &msg_ref)
{
    onBroadcast(msg_ref);
}

void CFdbBaseObject::callInvoke(CBaseJob::Ptr &msg_ref)
{
    try // catch exception to avoid missing of auto-reply
    {
        onInvoke(msg_ref);
    }
    catch (...)
    {
    }
    CFdbMessage::autoReply(msg_ref, NFdbBase::FDB_ST_AUTO_REPLY_OK, "Automatically reply to request.");
}

void CFdbBaseObject::callOnline(FdbSessionId_t sid, bool first_or_last, bool online)
{
    try
    {
        if (online)
        {
            if (!isPrimary() && (mRole == FDB_OBJECT_ROLE_CLIENT) && !fdbValidFdbId(mSid))
            {
                mSid = sid;
            }
            onOnline(sid, first_or_last);
        }
        else
        {
            onOffline(sid, first_or_last);
            if (!isPrimary() && (mSid == sid))
            {
                mSid = FDB_INVALID_ID;
            }
        }
    }
    catch (...)
    {
    }
}

void CFdbBaseObject::notifyOnline(CFdbSession *session, bool is_first)
{
    if (isPrimary())
    {
        mEndpoint->updateSessionInfo(session);
    }
    if (mWatchdog)
    {
        mWatchdog->addDog(session);
    }
    else if (mFlag & FDB_OBJ_ENABLE_WATCHDOG)
    {
        createWatchdog();
    }
    migrateToWorker(session->sid(), is_first, true);
}

void CFdbBaseObject::notifyOffline(CFdbSession *session, bool is_last)
{
    if (mWatchdog)
    {
        mWatchdog->removeDog(session);
    }
    migrateToWorker(session->sid(), is_last, false);
}

void CFdbBaseObject::callReply(CBaseJob::Ptr &msg_ref)
{
    onReply(msg_ref);
}

void CFdbBaseObject::callReturnEvent(CBaseJob::Ptr &msg_ref)
{
    onGetEvent(msg_ref);
}

void CFdbBaseObject::callStatus(CBaseJob::Ptr &msg_ref)
{
    auto fdb_msg = castToMessage<CFdbMessage *>(msg_ref);
    int32_t error_code;
    std::string description;
    if (!fdb_msg->decodeStatus(error_code, description))
    {
        return;
    }

    onStatus(msg_ref, error_code, description.c_str());
}

const CFdbBaseObject::CEventData *CFdbBaseObject::getCachedEventData(FdbMsgCode_t msg_code,
                                                                     const char *filter)
{
    auto it_filters = mEventCache.find(msg_code);
    if (it_filters != mEventCache.end())
    {
        auto &filters = it_filters->second;
        auto it_filter = filters.find(filter);
        if (it_filter != filters.end())
        {
            auto &data = it_filter->second;
            return &data;
        }
    }
    return 0;
}

void CFdbBaseObject::broadcastCached(CBaseJob::Ptr &msg_ref)
{
    auto msg = castToMessage<CFdbMessage *>(msg_ref);
    auto session = FDB_CONTEXT->getSession(msg->session());
    if (!session)
    {
        return;
    }
    const CFdbMsgSubscribeItem *sub_item;
    /* iterate all message id subscribed */
    FDB_BEGIN_FOREACH_SIGNAL(msg, sub_item)
    {
        FdbMsgCode_t msg_code = sub_item->msg_code();
        const char *topic = "";
        if (sub_item->has_filter())
        {
            topic = sub_item->filter().c_str();
        }
        if (topic[0] == '\0')
        {
            auto it_filters = mEventCache.find(msg_code);
            if (it_filters != mEventCache.end())
            {
                auto &filters = it_filters->second;
                for (auto it_data = filters.begin(); it_data != filters.end(); ++it_data)
                {
                    auto &cached_topic = it_data->first;
                    auto &cached_data = it_data->second;
                    CFdbMessage broadcast_msg(msg_code, msg, cached_topic.c_str());
                    if (broadcast_msg.serialize(cached_data.mBuffer, cached_data.mSize, this))
                    {
                        broadcast_msg.forceUpdate(true);
                        broadcast(&broadcast_msg, session);
                    }
                }
            }
        }
        else
        {
            auto cached_data = getCachedEventData(msg_code, topic);
            if (cached_data)
            {
                CFdbMessage broadcast_msg(msg_code, msg, topic);
                if (broadcast_msg.serialize(cached_data->mBuffer, cached_data->mSize, this))
                {
                    broadcast_msg.forceUpdate(true);
                    broadcast(&broadcast_msg, session);
                }
            }
        }
    }
    FDB_END_FOREACH_SIGNAL()
}

void CFdbBaseObject::doSubscribe(CBaseJob::Ptr &msg_ref)
{
    if (mFlag & FDB_OBJ_ENABLE_EVENT_CACHE)
    {
        try // catch exception to avoid missing of auto-reply
        {
            /* broadcast current value of event/filter pair */
            broadcastCached(msg_ref);
        }
        catch (...)
        {
        }

        CFdbMessage::autoReply(msg_ref, NFdbBase::FDB_ST_AUTO_REPLY_OK, "Automatically reply to subscribe request.");
        return;
    }

    migrateToWorker(msg_ref, &CFdbBaseObject::callSubscribe);
}

void CFdbBaseObject::doBroadcast(CBaseJob::Ptr &msg_ref)
{
    migrateToWorker(msg_ref, &CFdbBaseObject::callBroadcast);
}

void CFdbBaseObject::doGetEvent(CBaseJob::Ptr &msg_ref)
{
    auto msg = castToMessage<CBaseMessage *>(msg_ref);
    if (mFlag & FDB_OBJ_ENABLE_EVENT_CACHE)
    {
        auto cached_data = getCachedEventData(msg->code(), msg->topic().c_str());
        if (cached_data)
        {
            msg->replyEventCache(msg_ref, cached_data->mBuffer, cached_data->mSize);
        }
        else
        {
            msg->statusf(msg_ref, NFdbBase::FDB_ST_NON_EXIST, "Event %d topic %s doesn't exists!",
                         msg->code(), msg->topic().c_str());
        }
    }
    else
    {
        msg->status(msg_ref, NFdbBase::FDB_ST_NON_EXIST, "Event cache is not enabled!");
    }
}

void CFdbBaseObject::doInvoke(CBaseJob::Ptr &msg_ref)
{
    migrateToWorker(msg_ref, &CFdbBaseObject::callInvoke);
}

void CFdbBaseObject::doReply(CBaseJob::Ptr &msg_ref)
{
    migrateToWorker(msg_ref, &CFdbBaseObject::callReply);
}

void CFdbBaseObject::doReturnEvent(CBaseJob::Ptr &msg_ref)
{
    migrateToWorker(msg_ref, &CFdbBaseObject::callReturnEvent);
}

void CFdbBaseObject::doStatus(CBaseJob::Ptr &msg_ref)
{
    migrateToWorker(msg_ref, &CFdbBaseObject::callStatus);
}

void CFdbBaseObject::doPublish(CBaseJob::Ptr &msg_ref)
{
    if (mFlag & FDB_OBJ_ENABLE_EVENT_ROUTE)
    {
        try // catch exception to avoid missing of auto-reply
        {
            onPublish(msg_ref);
        }
        catch (...)
        {
        }
    }
    // check if auto-reply is required
    CFdbMessage::autoReply(msg_ref, NFdbBase::FDB_ST_AUTO_REPLY_OK, "Automatically reply to publish request.");
}

bool CFdbBaseObject::invoke(FdbSessionId_t receiver
                           , FdbMsgCode_t code
                           , const void *buffer
                           , int32_t size
                           , int32_t timeout
                           , const char *log_data)
{
    auto msg = new CBaseMessage(code, this, receiver);
    msg->setLogData(log_data);
    if (!msg->serialize(buffer, size, this))
    {
        delete msg;
        return false;
    }
    return msg->invoke(timeout);
}

bool CFdbBaseObject::invoke(FdbMsgCode_t code
                           , const void *buffer
                           , int32_t size
                           , int32_t timeout
                           , const char *log_data)
{
    return invoke(FDB_INVALID_ID, code, buffer, size, timeout, log_data);
}


bool CFdbBaseObject::invoke(FdbSessionId_t receiver
                           , CFdbMessage *msg
                           , const void *buffer
                           , int32_t size
                           , int32_t timeout)
{
    msg->setDestination(this, receiver);
    if (!msg->serialize(buffer, size, this))
    {
        delete msg;
        return false;
    }
    msg->enableTimeStamp(timeStampEnabled());
    return msg->invoke(timeout);
}

bool CFdbBaseObject::invoke(CFdbMessage *msg
                           , const void *buffer
                           , int32_t size
                           , int32_t timeout)
{
    return invoke(FDB_INVALID_ID, msg, buffer, size, timeout);
}

bool CFdbBaseObject::invoke(FdbSessionId_t receiver
                           , CBaseJob::Ptr &msg_ref
                           , const void *buffer
                           , int32_t size
                           , int32_t timeout)
{
    auto msg = castToMessage<CFdbMessage *>(msg_ref);
    msg->setDestination(this, receiver);
    if (!msg->serialize(buffer, size, this))
    {
       return false;
    }
    msg->enableTimeStamp(timeStampEnabled());
    return msg->invoke(msg_ref, timeout);
}

bool CFdbBaseObject::invoke(CBaseJob::Ptr &msg_ref
                           , const void *buffer
                           , int32_t size
                           , int32_t timeout)
{
    return invoke(FDB_INVALID_ID, msg_ref, buffer, size, timeout);
}

bool CFdbBaseObject::subscribe(CFdbMsgSubscribeList &msg_list
                              , int32_t timeout)
{
    auto msg = new CBaseMessage(FDB_INVALID_ID, this);
    msg->type(FDB_MT_SUBSCRIBE_REQ);
    CFdbParcelableBuilder builder(msg_list);
    if (!msg->serialize(builder, this))
    {
        delete msg;
        return false;
    }
    return msg->subscribe(timeout);
}

bool CFdbBaseObject::subscribe(CFdbMsgSubscribeList &msg_list
                              , CFdbMessage *msg
                              , int32_t timeout)
{
    msg->type(FDB_MT_SUBSCRIBE_REQ);
    msg->setDestination(this);
    CFdbParcelableBuilder builder(msg_list);
    if (!msg->serialize(builder, this))
    {
        delete msg;
        return false;
    }
    msg->enableTimeStamp(timeStampEnabled());
    return msg->subscribe(timeout);
}

bool CFdbBaseObject::subscribeSync(CFdbMsgSubscribeList &msg_list
                                  , int32_t timeout)
{
    auto msg = new CBaseMessage(FDB_INVALID_ID, this);
    msg->type(FDB_MT_SUBSCRIBE_REQ);
    msg->setDestination(this);
    CFdbParcelableBuilder builder(msg_list);
    if (!msg->serialize(builder, this))
    {
        delete msg;
        return false;
    }
    CBaseJob::Ptr msg_ref(msg);
    if (!msg->subscribe(msg_ref, timeout))
    {
        return false;
    }
    if (msg->isError())
    {
        return false;
    }
    if (worker())
    {
        worker()->flush();
    }

    return true;
}

bool CFdbBaseObject::update(CFdbMsgTriggerList &msg_list
                            , int32_t timeout)
{
    auto msg = new CBaseMessage(FDB_INVALID_ID, this);
    msg->type(FDB_MT_SUBSCRIBE_REQ);
    CFdbParcelableBuilder builder(msg_list);
    if (!msg->serialize(builder, this))
    {
        delete msg;
        return false;
    }
    return msg->update(timeout);
}

bool CFdbBaseObject::update(CFdbMsgTriggerList &msg_list
                            , CFdbMessage *msg
                            , int32_t timeout)
{
    msg->type(FDB_MT_SUBSCRIBE_REQ);
    msg->setDestination(this);
    CFdbParcelableBuilder builder(msg_list);
    if (!msg->serialize(builder, this))
    {
        delete msg;
        return false;
    }
    msg->enableTimeStamp(timeStampEnabled());
    return msg->update(timeout);
}

bool CFdbBaseObject::updateSync(CFdbMsgTriggerList &msg_list
                                , int32_t timeout)
{
    auto msg = new CBaseMessage(FDB_INVALID_ID, this);
    msg->type(FDB_MT_SUBSCRIBE_REQ);
    msg->setDestination(this);
    CFdbParcelableBuilder builder(msg_list);
    if (!msg->serialize(builder, this))
    {
        delete msg;
        return false;
    }
    CBaseJob::Ptr msg_ref(msg);
    if (!msg->update(msg_ref, timeout))
    {
        return false;
    }
    if (msg->isError())
    {
        return false;
    }
    if (worker())
    {
        worker()->flush();
    }

    return true;
}

void CFdbBaseObject::addNotifyItem(CFdbMsgSubscribeList &msg_list
                                  , FdbMsgCode_t msg_code
                                  , const char *filter)
{
    auto item = msg_list.add_subscribe_tbl();
    item->set_msg_code(msg_code);
    if (filter)
    {
        item->set_filter(filter);
    }
}

void CFdbBaseObject::addNotifyGroup(CFdbMsgSubscribeList &msg_list
                                    , FdbEventGroup_t event_group
                                    , const char *filter)
{
    addNotifyItem(msg_list, fdbMakeEventGroup(event_group), filter);
}

void CFdbBaseObject::addUpdateItem(CFdbMsgSubscribeList &msg_list
                                  , FdbMsgCode_t msg_code
                                  , const char *filter)
{
    auto item = msg_list.add_subscribe_tbl();
    item->set_msg_code(msg_code);
    if (filter)
    {
        item->set_filter(filter);
    }
    item->set_type(FDB_SUB_TYPE_ON_REQUEST);
}

void CFdbBaseObject::addUpdateGroup(CFdbMsgSubscribeList &msg_list
                                    , FdbEventGroup_t event_group
                                    , const char *filter)
{
    addUpdateItem(msg_list, fdbMakeEventGroup(event_group), filter);
}

void CFdbBaseObject::addTriggerItem(CFdbMsgTriggerList &msg_list
                                     , FdbMsgCode_t msg_code
                                     , const char *filter)
{
    addNotifyItem(msg_list, msg_code, filter);
}

 void CFdbBaseObject::addTriggerGroup(CFdbMsgTriggerList &msg_list
                                      , FdbEventGroup_t event_group
                                      , const char *filter)
{
    addNotifyGroup(msg_list, event_group, filter);
}

void CFdbBaseObject::subscribe(CFdbSession *session,
                               FdbMsgCode_t msg,
                               FdbObjectId_t obj_id,
                               const char *filter,
                               CFdbSubscribeType type)
{
    CEventSubscribeHandle &subscribe_handle = fdbIsGroup(msg) ?
                                              mGroupSubscribeHandle : mEventSubscribeHandle;
    subscribe_handle.subscribe(session, msg, obj_id, filter, type);
}

void CFdbBaseObject::unsubscribe(CFdbSession *session,
                                 FdbMsgCode_t msg,
                                 FdbObjectId_t obj_id,
                                 const char *filter)
{
    CEventSubscribeHandle &subscribe_handle = fdbIsGroup(msg) ?
                                              mGroupSubscribeHandle : mEventSubscribeHandle;
    subscribe_handle.unsubscribe(session, msg, obj_id, filter);
}

void CFdbBaseObject::unsubscribe(CFdbSession *session)
{
    mGroupSubscribeHandle.unsubscribe(session);
    mEventSubscribeHandle.unsubscribe(session);
}

void CFdbBaseObject::unsubscribe(FdbObjectId_t obj_id)
{
    mGroupSubscribeHandle.unsubscribe(obj_id);
    mEventSubscribeHandle.unsubscribe(obj_id);
}

bool CFdbBaseObject::updateEventCache(CFdbMessage *msg)
{
    if (mFlag & FDB_OBJ_ENABLE_EVENT_CACHE)
    {
        // update cached event data
        auto &cached_event = mEventCache[msg->code()][msg->topic()];
        auto updated = cached_event.setEventCache(msg->getPayloadBuffer(), msg->getPayloadSize());
        if (!updated)
        {
            if (!cached_event.mAlwaysUpdate && !msg->isForceUpdate())
            {
                return false;
            }
        }
    }
    return true;
}

void CFdbBaseObject::broadcast(CFdbMessage *msg)
{
    if (updateEventCache(msg))
    {
        mEventSubscribeHandle.broadcast(msg, msg->code());
        mGroupSubscribeHandle.broadcast(msg, fdbMakeGroup(msg->code()));
    }
}

bool CFdbBaseObject::broadcast(CFdbMessage *msg, CFdbSession *session)
{
    if (updateEventCache(msg))
    {
        if (!mEventSubscribeHandle.broadcast(msg, session, msg->code()))
        {
            return mGroupSubscribeHandle.broadcast(msg, session, fdbMakeGroup(msg->code()));
        }
    }
    return false;
}

void CFdbBaseObject::getSubscribeTable(tFdbSubscribeMsgTbl &table)
{
    mEventSubscribeHandle.getSubscribeTable(table);
    mGroupSubscribeHandle.getSubscribeTable(table);
}

void CFdbBaseObject::getSubscribeTable(FdbMsgCode_t code, tFdbFilterSets &filters)
{
    mEventSubscribeHandle.getSubscribeTable(code, filters);
    mGroupSubscribeHandle.getSubscribeTable(fdbMakeGroup(code), filters);
}

void CFdbBaseObject::getSubscribeTable(FdbMsgCode_t code, CFdbSession *session,
                                        tFdbFilterSets &filter_tbl)
{
    mEventSubscribeHandle.getSubscribeTable(code, session, filter_tbl);
    mGroupSubscribeHandle.getSubscribeTable(fdbMakeGroup(code), session, filter_tbl);
}

void CFdbBaseObject::getSubscribeTable(FdbMsgCode_t code, const char *filter,
                                       tSubscribedSessionSets &session_tbl)
{
    mEventSubscribeHandle.getSubscribeTable(code, filter, session_tbl);
    mGroupSubscribeHandle.getSubscribeTable(fdbMakeGroup(code), filter, session_tbl);
}

FdbObjectId_t CFdbBaseObject::addToEndpoint(CBaseEndpoint *endpoint, FdbObjectId_t obj_id)
{
    mEndpoint = endpoint;
    mObjId = obj_id;
    obj_id = endpoint->addObject(this);
    if (fdbValidFdbId(obj_id))
    {
        LOG_I("CFdbBaseObject: Object %d is created.\n", obj_id);
        registered(true);
    }
    return mObjId;
}

void CFdbBaseObject::removeFromEndpoint()
{
    if (mEndpoint)
    {
        mEndpoint->removeObject(this);
        registered(false);
        mEndpoint = 0;
    }
}

FdbEndpointId_t CFdbBaseObject::epid() const
{
    return mEndpoint ? mEndpoint->epid() : FDB_INVALID_ID;
}


FdbObjectId_t CFdbBaseObject::doBind(CBaseEndpoint *endpoint, FdbObjectId_t obj_id)
{
    mRole = FDB_OBJECT_ROLE_SERVER;
    return addToEndpoint(endpoint, obj_id);
}

FdbObjectId_t CFdbBaseObject::doConnect(CBaseEndpoint *endpoint, FdbObjectId_t obj_id)
{
    mRole = FDB_OBJECT_ROLE_CLIENT;
    return addToEndpoint(endpoint, obj_id);
}

void CFdbBaseObject::doUnbind()
{
    removeFromEndpoint();
}

void CFdbBaseObject::doDisconnect()
{
    removeFromEndpoint();
}

//================================== Bind ==========================================
class CBindObjectJob : public CMethodJob<CFdbBaseObject>
{
public:
    CBindObjectJob(CFdbBaseObject *object, CBaseEndpoint *endpoint, FdbObjectId_t &oid)
        : CMethodJob<CFdbBaseObject>(object, &CFdbBaseObject::callBindObject, JOB_FORCE_RUN)
        , mEndpoint(endpoint)
        , mOid(oid)
    {
    }

    CBaseEndpoint *mEndpoint;
    FdbObjectId_t &mOid;
};

void CFdbBaseObject::callBindObject(CBaseWorker *worker, CMethodJob<CFdbBaseObject> *job, CBaseJob::Ptr &ref)
{
    auto the_job = fdb_dynamic_cast_if_available<CBindObjectJob *>(job);
    if (the_job)
    {
        the_job->mOid = doBind(the_job->mEndpoint, the_job->mOid);
    }
}

FdbObjectId_t CFdbBaseObject::bind(CBaseEndpoint *endpoint, FdbObjectId_t oid)
{
    if (registered())
    {
        oid = mObjId;
    }
    else
    {
        CFdbContext::getInstance()->sendSyncEndeavor(new CBindObjectJob(this, endpoint, oid), 0, true);
    }
    return oid;
}

//================================== Connect ==========================================
class CConnectObjectJob : public CMethodJob<CFdbBaseObject>
{
public:
    CConnectObjectJob(CFdbBaseObject *object, CBaseEndpoint *endpoint, FdbObjectId_t &oid)
        : CMethodJob<CFdbBaseObject>(object, &CFdbBaseObject::callConnectObject, JOB_FORCE_RUN)
        , mEndpoint(endpoint)
        , mOid(oid)
    {
    }

    CBaseEndpoint *mEndpoint;
    FdbObjectId_t &mOid;
};

void CFdbBaseObject::callConnectObject(CBaseWorker *worker, CMethodJob<CFdbBaseObject> *job, CBaseJob::Ptr &ref)
{
    auto the_job = fdb_dynamic_cast_if_available<CConnectObjectJob *>(job);
    if (the_job)
    {
        the_job->mOid = doConnect(the_job->mEndpoint, the_job->mOid);
    }
}

FdbObjectId_t CFdbBaseObject::connect(CBaseEndpoint *endpoint, FdbObjectId_t oid)
{
    if (registered())
    {
        oid = mObjId;
    }
    else
    {
        CFdbContext::getInstance()->sendSyncEndeavor(new CConnectObjectJob(this, endpoint, oid));
    }
    return oid;
}

//================================== Unbind ==========================================
class CUnbindObjectJob : public CMethodJob<CFdbBaseObject>
{
public:
    CUnbindObjectJob(CFdbBaseObject *object)
        : CMethodJob<CFdbBaseObject>(object, &CFdbBaseObject::callUnbindObject, JOB_FORCE_RUN)
    {
    }
};

void CFdbBaseObject::callUnbindObject(CBaseWorker *worker, CMethodJob<CFdbBaseObject> *job, CBaseJob::Ptr &ref)
{
     doUnbind();
}

void CFdbBaseObject::unbind()
{
    if (registered())
    {
        CFdbContext::getInstance()->sendSyncEndeavor(new CUnbindObjectJob(this), 0, true);
        // From now on, there will be no jobs migrated to worker thread. Applying a
        // flush to worker thread to ensure no one refers to the object.
    }
}

//================================== Disconnect ==========================================
class CDisconnectObjectJob : public CMethodJob<CFdbBaseObject>
{
public:
    CDisconnectObjectJob(CFdbBaseObject *object)
        : CMethodJob<CFdbBaseObject>(object, &CFdbBaseObject::callDisconnectObject, JOB_FORCE_RUN)
    {
    }
};

void CFdbBaseObject::callDisconnectObject(CBaseWorker *worker, CMethodJob<CFdbBaseObject> *job, CBaseJob::Ptr &ref)
{
     doDisconnect();
}

void CFdbBaseObject::disconnect()
{
    if (registered())
    {
        unsubscribe();
        CFdbContext::getInstance()->sendSyncEndeavor(new CDisconnectObjectJob(this), 0, true);
        // From now on, there will be no jobs migrated to worker thread. Applying a
        // flush to worker thread to ensure no one refers to the object.
    }
}

bool CFdbBaseObject::broadcast(FdbSessionId_t sid
                              , FdbObjectId_t obj_id
                              , FdbMsgCode_t code
                              , IFdbMsgBuilder &data
                              , const char *filter)
{
    auto msg = new CFdbMessage(code, this, filter, sid, obj_id);
    if (!msg->serialize(data, this))
    {
        delete msg;
        return false;
    }
    return msg->broadcast();
}

bool CFdbBaseObject::broadcast(FdbSessionId_t sid
                      , FdbObjectId_t obj_id
                      , FdbMsgCode_t code
                      , const void *buffer
                      , int32_t size
                      , const char *filter
                      , const char *log_data)
{
    auto msg = new CFdbMessage(code, this, filter, sid, obj_id);
    msg->setLogData(log_data);
    if (!msg->serialize(buffer, size, this))
    {
        delete msg;
        return false;
    }
    return msg->broadcast();
}

void CFdbBaseObject::initEventCache(FdbMsgCode_t event
                                    , const char *topic
                                    , IFdbMsgBuilder &data
                                    , bool always_update)
{
    if (!topic)
    {
        topic = "";
    }
    auto &cached_event = mEventCache[event][topic];
    cached_event.mAlwaysUpdate = always_update;
    int32_t size = data.build();
    if (size < 0)
    {
        return;
    }
    cached_event.setEventCache(0, size);
    data.toBuffer(cached_event.mBuffer, size);
}
                
void CFdbBaseObject::initEventCache(FdbMsgCode_t event
                                    , const char *topic
                                    , const void *buffer
                                    , int32_t size
                                    , bool always_update)
{
    if (!topic)
    {
        topic = "";
    }
    auto &cached_event = mEventCache[event][topic];
    cached_event.mAlwaysUpdate = always_update;
    cached_event.setEventCache((const uint8_t *)buffer, size);
}

bool CFdbBaseObject::invokeSideband(FdbMsgCode_t code
                                  , IFdbMsgBuilder &data
                                  , int32_t timeout)
{
    auto msg = new CBaseMessage(code, this, FDB_INVALID_ID);
    if (!msg->serialize(data, this))
    {
        delete msg;
        return false;
    }
    return msg->invokeSideband(timeout);
}

bool CFdbBaseObject::invokeSideband(FdbMsgCode_t code
                                  , const void *buffer
                                  , int32_t size
                                  , int32_t timeout)
{
    auto msg = new CBaseMessage(code, this, FDB_INVALID_ID);
    if (!msg->serialize(buffer, size, this))
    {
        delete msg;
        return false;
    }
    return msg->invokeSideband(timeout);
}

bool CFdbBaseObject::sendSidebandNoQueue(CFdbMessage &msg, bool expect_reply)
{
    msg.expectReply(expect_reply);
    msg.type(FDB_MT_SIDEBAND_REQUEST);
    auto session = msg.getSession();
    if (session)
    {
        return session->sendMessage(&msg);
    }
    return false;
}

bool CFdbBaseObject::sendSideband(FdbSessionId_t receiver, FdbMsgCode_t code, IFdbMsgBuilder &data)
{
    CFdbMessage msg(code, this, receiver);
    if (!msg.serialize(data, this))
    {
        return false;
    }
    return sendSidebandNoQueue(msg, false);
}

bool CFdbBaseObject::sendSideband(FdbMsgCode_t code, IFdbMsgBuilder &data)
{
    return sendSideband(FDB_INVALID_ID, code, data);
}

bool CFdbBaseObject::sendSideband(FdbSessionId_t receiver, FdbMsgCode_t code,
                                  const void *buffer, int32_t size)
{
    CFdbMessage msg(code, this, receiver);
    if (!msg.serialize(buffer, size, this))
    {
        return false;
    }
    return sendSidebandNoQueue(msg, false);
}

bool CFdbBaseObject::sendSideband(FdbMsgCode_t code, const void *buffer, int32_t size)
{
    return sendSideband(FDB_INVALID_ID, code, buffer, size);
}

CFdbBaseObject::CEventData::CEventData()
    : mBuffer(0)
    , mSize(0)
    , mAlwaysUpdate(false)
{
}

CFdbBaseObject::CEventData::~CEventData()
{
    if (mBuffer)
    {
        delete[] mBuffer;
        mBuffer = 0;
    }
}

bool CFdbBaseObject::CEventData::setEventCache(const uint8_t *buffer, int32_t size)
{
    if (size == mSize)
    {
        if (mBuffer)
        {
            if (buffer)
            {
                if (!memcmp(mBuffer, buffer, size))
                {
                    return false;
                }
            }
        }
        else if (size)
        {
            mBuffer = new uint8_t[size];
        }
    }
    else
    {
        if (mBuffer)
        {
            delete[] mBuffer;
            mBuffer = 0;
        }

        if (size)
        {
            mBuffer = new uint8_t[size];
        }
        mSize = size;
    }

    if (size && buffer)
    {
        memcpy(mBuffer, buffer, size);
    }
    return true;
}

void CFdbBaseObject::CEventData::replaceEventCache(uint8_t *buffer, int32_t size)
{
    if (mBuffer)
    {
        delete[] mBuffer;
        mBuffer = 0;
    }
    mBuffer = buffer;
    mSize = size;
}

void CFdbBaseObject::onSidebandInvoke(CBaseJob::Ptr &msg_ref)
{
    auto msg = castToMessage<CFdbMessage *>(msg_ref);
    switch (msg->code())
    {
        case FDB_SIDEBAND_QUERY_EVT_CACHE:
        {
            NFdbBase::FdbMsgEventCache msg_cache;
            for (auto it_filters = mEventCache.begin(); it_filters != mEventCache.end(); ++it_filters)
            {
                auto event_code = it_filters->first;
                auto &filters = it_filters->second;
                for (auto it_data = filters.begin(); it_data != filters.end(); ++it_data)
                {
                    auto &filter = it_data->first;
                    auto &data = it_data->second;
                    auto cache_item = msg_cache.add_cache();
                    cache_item->set_event(event_code);
                    cache_item->set_topic(filter.c_str());
                    cache_item->set_size(data.mSize);
                }
            }
            CFdbParcelableBuilder builder(msg_cache);
            msg->replySideband(msg_ref, builder);
        }
        break;
        case FDB_SIDEBAND_KICK_WATCHDOG:
        {
            msg->forceRun(true);
            onKickDog(msg_ref);
        }
        break;
        case FDB_SIDEBAND_FEED_WATCHDOG:
            if (mWatchdog)
            {
                auto session = FDB_CONTEXT->getSession(msg->session());
                if (session)
                {
                    mWatchdog->feedDog(session);
                }
            }
        break;
        default:
        break;
    }
}

void CFdbBaseObject::onPublish(CBaseJob::Ptr &msg_ref)
{
    auto msg = castToMessage<CBaseMessage *>(msg_ref);
    broadcastNoQueue(msg->code(), msg->getPayloadBuffer(), msg->getPayloadSize(),
                     msg->topic().c_str(), msg->isForceUpdate(), msg->qos());
}

class CPrepareDestroyJob : public CMethodJob<CFdbBaseObject>
{
public:
    CPrepareDestroyJob(CFdbBaseObject *object)
        : CMethodJob<CFdbBaseObject>(object, &CFdbBaseObject::callPrepareDestroy, JOB_FORCE_RUN)
    {
    }
};

void CFdbBaseObject::callPrepareDestroy(CBaseWorker *worker, CMethodJob<CFdbBaseObject> *job, CBaseJob::Ptr &ref)
{
    enableMigrate(false);
}

void CFdbBaseObject::prepareDestroy()
{
    /*
     * Why not just call callPrepareDestroy()? Supposing the following case:
     * CFdbContext is executing the following logic:
     * 1. check if migration flag is set;
     * 2. if set, migrate callback to worker thread.
     * between 1 and 2, call callPrepareDestroy() followed by flush(). It might
     * be possible that the job to flush is in front of the job for migration.
     */
    CFdbContext::getInstance()->sendSyncEndeavor(new CPrepareDestroyJob(this), 0, true);
    if (mWorker)
    {
        // Make sure no pending remote callback is queued
        mWorker->flush();
    }
}

#define FDB_MSG_TYPE_AFC_SUBSCRIBE (FDB_MSG_TYPE_SYSTEM - 1)
#define FDB_MSG_TYPE_AFC_INVOKE (FDB_MSG_TYPE_SYSTEM - 2)

class CAFCSubscribeMsg : public CFdbMessage
{
public:
    CAFCSubscribeMsg(CFdbEventDispatcher::tRegistryHandleTbl *reg_handle)
        : CFdbMessage()
    {
        if (reg_handle)
        {
            mRegHandle = *reg_handle;
        }
    }
    CAFCSubscribeMsg(NFdbBase::CFdbMessageHeader &head
                      , CFdbMsgPrefix &prefix
                      , uint8_t *buffer
                      , FdbSessionId_t sid
                      , CFdbEventDispatcher::tRegistryHandleTbl &reg_handle)
        : CFdbMessage(head, prefix, buffer, sid)
        , mRegHandle(reg_handle)
    {
    }
    FdbMessageType_t getTypeId()
    {
        return FDB_MSG_TYPE_AFC_SUBSCRIBE;
    }

    CFdbEventDispatcher::tRegistryHandleTbl mRegHandle;
protected:
    CFdbMessage *clone(NFdbBase::CFdbMessageHeader &head
                      , CFdbMsgPrefix &prefix
                      , uint8_t *buffer
                      , FdbSessionId_t sid)
    {
        return new CAFCSubscribeMsg(head, prefix, buffer, sid, mRegHandle);
    }
};

class CAFCInvokeMsg : public CFdbMessage
{
public:
    CAFCInvokeMsg(FdbMsgCode_t code, CFdbBaseObject::tInvokeCallbackFn &reply_callback,
                  CBaseWorker *worker = 0)
        : CFdbMessage(code)
        , mReplyCallback(reply_callback)
        , mWorker(worker)
    {
    }
    FdbMessageType_t getTypeId()
    {
        return FDB_MSG_TYPE_AFC_INVOKE;
    }
    CFdbBaseObject::tInvokeCallbackFn mReplyCallback;
    CBaseWorker *mWorker;
};

class CDispOnOnlineJob : public CMethodJob<CFdbBaseObject>
{
public:
    CDispOnOnlineJob(CFdbBaseObject *object, FdbSessionId_t sid, bool first_or_last, bool online,
                 CFdbBaseObject::tConnCallbackFn &callback)
        : CMethodJob<CFdbBaseObject>(object, &CFdbBaseObject::callDispOnOnline, JOB_FORCE_RUN)
        , mSid(sid)
        , mFirstOrLast(first_or_last)
        , mOnline(online)
        , mCallback(callback)
    {}

    FdbSessionId_t mSid;
    bool mFirstOrLast;
    bool mOnline;
    CFdbBaseObject::tConnCallbackFn mCallback;
};

void CFdbBaseObject::callDispOnOnline(CBaseWorker *worker, CMethodJob<CFdbBaseObject> *job, CBaseJob::Ptr &ref)
{
    auto the_job = fdb_dynamic_cast_if_available<CDispOnOnlineJob *>(job);
    if (the_job)
    {
        the_job->mCallback(this, the_job->mSid, the_job->mOnline, the_job->mFirstOrLast);
    }
}

void CFdbBaseObject::migrateToWorker(FdbSessionId_t sid, bool first_or_last, bool online,
                                     tConnCallbackFn &callback, CBaseWorker *worker)
{
    if (!callback)
    {
        return;
    }
    if (!worker || worker->isSelf())
    {
        try
        {
            callback(this, sid, online, first_or_last);
        }
        catch (...)
        {
        }
    }
    else
    {
        worker->sendAsync(new CDispOnOnlineJob(this, sid, first_or_last, online, callback));
    }
}

CFdbBaseObject::tRegEntryId CFdbBaseObject::registerConnNotification(tConnCallbackFn callback,
                                                                     CBaseWorker *worker)
{
    if (!callback)
    {
        return FDB_INVALID_ID;
    }
    CFdbBaseObject::tRegEntryId id = mRegIdAllocator++;
    auto &item = mConnCallbackTbl[id];
    item.mCallback = callback;
    item.mWorker = worker;

    if (!isPrimary())
    {
        return id;
    }

    bool is_first = true;
    auto &containers = mEndpoint->getContainer();
    for (auto socket_it = containers.begin(); socket_it != containers.end(); ++socket_it)
    {
        auto container = socket_it->second;
        if (!container->mConnectedSessionTable.empty())
        {
            // get a snapshot of the table to avoid modification of the table in callback
            auto tbl = container->mConnectedSessionTable;
            for (auto session_it = tbl.begin(); session_it != tbl.end(); ++session_it)
            {
                auto session = *session_it;
                if (!authentication(session))
                {
                    continue;
                }
                migrateToWorker(session->sid(), is_first, true, callback, worker);
                is_first = false;
            }
        }
    }
    return id;
}

bool CFdbBaseObject::subscribeEvents(const CFdbEventDispatcher::tEvtHandleTbl &events,
                                     CFdbEventDispatcher::tRegistryHandleTbl *reg_handle)
{
    CFdbMsgSubscribeList subscribe_list;
    for (auto it = events.begin(); it != events.end(); ++it)
    {
        addNotifyItem(subscribe_list, it->mCode, it->mTopic.c_str());
    }
    return subscribe(subscribe_list, new CAFCSubscribeMsg(reg_handle));
}

bool CFdbBaseObject::registerEventHandle(const CFdbEventDispatcher::CEvtHandleTbl &evt_tbl,
                                         CFdbEventDispatcher::tRegistryHandleTbl *reg_handle)
{
    CFdbEventDispatcher::tRegistryHandleTbl handle_tbl;
    mEvtDispather.registerCallback(evt_tbl, &handle_tbl);
    if (reg_handle)
    {
        reg_handle->insert(reg_handle->end(), handle_tbl.begin(), handle_tbl.end());
    }

    if (mEndpoint->connected())
    {
        subscribeEvents(evt_tbl.getEvtHandleTbl(), &handle_tbl);
    }
    return true;
}

bool CFdbBaseObject::registerMsgHandle(const CFdbMsgDispatcher::CMsgHandleTbl &msg_tbl)
{
    return mMsgDispather.registerCallback(msg_tbl);
}

void CFdbBaseObject::onBroadcast(CBaseJob::Ptr &msg_ref)
{
    const CFdbEventDispatcher::tRegistryHandleTbl *registered_evt_tbl = 0;
    auto *msg = castToMessage<CBaseMessage *>(msg_ref);
    if (msg->isInitialResponse() && (msg->getTypeId() == FDB_MSG_TYPE_AFC_SUBSCRIBE))
    {
        auto afc_msg = castToMessage<CAFCSubscribeMsg *>(msg_ref);
        registered_evt_tbl = &afc_msg->mRegHandle;
    }
    mEvtDispather.processMessage(msg_ref, this, registered_evt_tbl);
}

void CFdbBaseObject::onReply(CBaseJob::Ptr &msg_ref)
{
    auto *msg = castToMessage<CBaseMessage *>(msg_ref);
    if (msg->getTypeId() == FDB_MSG_TYPE_AFC_INVOKE)
    {
        auto afc_msg = castToMessage<CAFCInvokeMsg *>(msg_ref);
        fdbMigrateCallback(msg_ref, msg, afc_msg->mReplyCallback, afc_msg->mWorker, this);
    }
}

void CFdbBaseObject::onOnline(FdbSessionId_t sid, bool is_first)
{
    CFdbEventDispatcher::tEvtHandleTbl events;
    mEvtDispather.dumpEvents(events);
    subscribeEvents(events, 0);

    for (auto it = mConnCallbackTbl.begin(); it != mConnCallbackTbl.end(); ++it)
    {
        migrateToWorker(sid, is_first, true, it->second.mCallback, it->second.mWorker);
    }
}
void CFdbBaseObject::onOffline(FdbSessionId_t sid, bool is_last)
{
    for (auto it = mConnCallbackTbl.begin(); it != mConnCallbackTbl.end(); ++it)
    {
        migrateToWorker(sid, is_last, false, it->second.mCallback, it->second.mWorker);
    }
}

void CFdbBaseObject::onInvoke(CBaseJob::Ptr &msg_ref)
{
    auto *msg = castToMessage<CBaseMessage *>(msg_ref);
    msg->setPostProcessing([](CBaseJob::Ptr &msg_ref)
        {
            CFdbMessage::autoReply(msg_ref, NFdbBase::FDB_ST_AUTO_REPLY_OK, "Automatically reply to request.");
        });
    mMsgDispather.processMessage(msg_ref, this);
}

bool CFdbBaseObject::invoke(FdbMsgCode_t code, IFdbMsgBuilder &data
                           , CFdbBaseObject::tInvokeCallbackFn callback
                           , CBaseWorker *worker, int32_t timeout)
{
    auto invoke_msg = new CAFCInvokeMsg(code, callback, worker);
    return CFdbBaseObject::invoke(invoke_msg, data, timeout);
}

bool CFdbBaseObject::invoke(FdbMsgCode_t code, CFdbBaseObject::tInvokeCallbackFn callback
                           , const void *buffer, int32_t size, CBaseWorker *worker
                           , int32_t timeout, const char *log_info)
{
    auto invoke_msg = new CAFCInvokeMsg(code, callback, worker);
    invoke_msg->setLogData(log_info);
    return CFdbBaseObject::invoke(invoke_msg, buffer, size, timeout);
}

bool CFdbBaseObject::kickDog(CFdbSession *session)
{
    CFdbMessage msg(FDB_SIDEBAND_KICK_WATCHDOG, this);
    msg.expectReply(false);
    msg.enableLog(false);
    msg.type(FDB_MT_SIDEBAND_REQUEST);
    msg.serialize(0, 0, this);
    return session->sendMessage(&msg);
}

void CFdbBaseObject::onKickDog(CBaseJob::Ptr &msg_ref)
{
    CFdbMessage::feedDogNoQueue(msg_ref);
}

void CFdbBaseObject::onBark(CFdbSession *session)
{
    LOG_F("CFdbBaseObject: NAME %s, PID %d: NO RESPONSE!!!\n", session->getEndpointName().c_str(),
                                                               session->pid());
}

enum FdbWdogAction
{
    FDB_WDOG_START,
    FDB_WDOG_STOP,
    FDB_WDOG_REMOVE
};

class CWatchdogJob : public CMethodJob<CFdbBaseObject>
{
public:
    CWatchdogJob(CFdbBaseObject *obj, FdbWdogAction action, int32_t interval = 0, int32_t max_retries = 0)
        : CMethodJob<CFdbBaseObject>(obj, &CFdbBaseObject::callWatchdogAction, JOB_FORCE_RUN)
        , mAction(action)
        , mInterval(interval)
        , mMaxRetries(max_retries)
    {}
    FdbWdogAction mAction;
    int32_t mInterval;
    int32_t mMaxRetries;
};

void CFdbBaseObject::createWatchdog(int32_t interval, int32_t max_retries)
{
    if (!mWatchdog)
    {
        mWatchdog = new CFdbWatchdog(this, interval, max_retries);
        auto &containers = mEndpoint->getContainer();
        for (auto socket_it = containers.begin(); socket_it != containers.end(); ++socket_it)
        {
            auto container = socket_it->second;
            if (!container->mConnectedSessionTable.empty())
            {
                // get a snapshot of the table to avoid modification of the table in callback
                auto tbl = container->mConnectedSessionTable;
                for (auto session_it = tbl.begin(); session_it != tbl.end(); ++session_it)
                {
                    auto session = *session_it;
                    mWatchdog->addDog(session);
                }
            }
        }
    }

    mWatchdog->start();
}

void CFdbBaseObject::callWatchdogAction(CBaseWorker *worker, CMethodJob<CFdbBaseObject> *job,
                                            CBaseJob::Ptr &ref)
{
    auto the_job = fdb_dynamic_cast_if_available<CWatchdogJob *>(job);
    if (the_job->mAction == FDB_WDOG_START)
    {
        createWatchdog(the_job->mInterval, the_job->mMaxRetries);
        mFlag |= FDB_OBJ_ENABLE_WATCHDOG;
    }
    else if (the_job->mAction == FDB_WDOG_STOP)
    {
        if (mWatchdog)
        {
            mWatchdog->stop();
        }
    }
    else if (the_job->mAction == FDB_WDOG_REMOVE)
    {
        if (mWatchdog)
        {
            delete mWatchdog;
            mWatchdog = 0;
            mFlag &= ~FDB_OBJ_ENABLE_WATCHDOG;
        }
    }
}

void CFdbBaseObject::startWatchdog(int32_t interval, int32_t max_retries)
{
    auto job = new CWatchdogJob(this, FDB_WDOG_START, interval, max_retries);
    FDB_CONTEXT->sendAsync(job);
}

void CFdbBaseObject::stopWatchdog()
{
    auto job = new CWatchdogJob(this, FDB_WDOG_STOP);
    FDB_CONTEXT->sendAsync(job);
}

void CFdbBaseObject::removeWatchdog()
{
    auto job = new CWatchdogJob(this, FDB_WDOG_REMOVE);
    FDB_CONTEXT->sendAsync(job);
}

void CFdbBaseObject::getDroppedProcesses(CFdbMsgProcessList &process_list)
{
    if (mWatchdog)
    {
        mWatchdog->getDroppedProcesses(process_list);
    }
}

