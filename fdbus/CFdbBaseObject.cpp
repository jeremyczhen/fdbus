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
#include <utils/Log.h>
#include <string.h>

using namespace std::placeholders;

CFdbBaseObject::CFdbBaseObject(const char *name, CBaseWorker *worker, EFdbEndpointRole role)
    : mEndpoint(0)
    , mFlag(0)
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
                          , bool fast)
{
    auto msg = new CBaseMessage(code, this, receiver, fast);
    if (!msg->serialize(data, this))
    {
        delete msg;
        return false;
    }
    return msg->send();
}

bool CFdbBaseObject::send(FdbMsgCode_t code, IFdbMsgBuilder &data, bool fast)
{
    return send(FDB_INVALID_ID, code, data, fast);
}

bool CFdbBaseObject::send(FdbSessionId_t receiver
                         , FdbMsgCode_t code
                         , const void *buffer
                         , int32_t size
                         , bool fast
                         , const char *log_data)
{
    auto msg = new CBaseMessage(code, this, receiver, fast);
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
                         , bool fast
                         , const char *log_data)
{
    return send(FDB_INVALID_ID, code, buffer, size, fast, log_data);
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
    msg->setEventGet(true);
    return msg->invoke(timeout);
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
    msg->setEventGet(true);
    msg->enableTimeStamp(timeStampEnabled());
    return msg->invoke(timeout);
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
    msg->setEventGet(true);
    msg->enableTimeStamp(timeStampEnabled());
    return msg->invoke(msg_ref, timeout);
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
    CBaseMessage msg(code, this, FDB_INVALID_ID, true);
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
                               , bool fast)
{
    auto msg = new CFdbMessage(code, this, filter, FDB_INVALID_ID, FDB_INVALID_ID, fast);
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
                              , bool fast
                              , const char *log_data)
{
    auto msg = new CFdbMessage(code, this, filter, FDB_INVALID_ID, FDB_INVALID_ID, fast);
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
    CFdbMessage msg(code, this, filter, FDB_INVALID_ID, FDB_INVALID_ID, true);
    if (!msg.serialize(data, size, this))
    {
        return;
    }
    msg.enableLog(false);

    broadcast(&msg);
}

void CFdbBaseObject::broadcastNoQueue(FdbMsgCode_t code, const uint8_t *data, int32_t size,
                                      const char *filter, bool force_update, bool fast)
{
    CFdbMessage msg(code, this, filter, FDB_INVALID_ID, FDB_INVALID_ID, fast);
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

void CFdbBaseObject::callSubscribe(CBaseJob::Ptr &msg_ref)
{
    onSubscribe(msg_ref);
    CFdbMessage::autoReply(msg_ref, NFdbBase::FDB_ST_AUTO_REPLY_OK, "Automatically reply to subscribe request.");
}

bool CFdbBaseObject::migrateOnSubscribeToWorker(CBaseJob::Ptr &msg_ref, CBaseWorker *worker)
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
            msg->setCallable(std::bind(&CFdbBaseObject::callSubscribe, this, _1));
            if (!worker->sendAsync(msg_ref))
            {
                LOG_E("CFdbBaseObject: Unable to migrate onsubscribe to worker!\n");
            }
        }
        return true;
    }
    return false;
}

void CFdbBaseObject::callBroadcast(CBaseJob::Ptr &msg_ref)
{
    onBroadcast(msg_ref);
}

bool CFdbBaseObject::migrateOnBroadcastToWorker(CBaseJob::Ptr &msg_ref, CBaseWorker *worker)
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
            msg->setCallable(std::bind(&CFdbBaseObject::callBroadcast, this, _1));
            if (!worker->sendAsync(msg_ref))
            {
                LOG_E("CFdbBaseObject: Unable to migrate onbroadcast to worker!\n");
            }
        }
        return true;
    }
    return false;
}

void CFdbBaseObject::callInvoke(CBaseJob::Ptr &msg_ref)
{
    onInvoke(msg_ref);
    CFdbMessage::autoReply(msg_ref, NFdbBase::FDB_ST_AUTO_REPLY_OK, "Automatically reply to request.");
}

bool CFdbBaseObject::migrateOnInvokeToWorker(CBaseJob::Ptr &msg_ref, CBaseWorker *worker)
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
            msg->setCallable(std::bind(&CFdbBaseObject::callInvoke, this, _1));
            if (!worker->sendAsync(msg_ref))
            {
                LOG_E("CFdbBaseObject: Unable to migrate oninvoke to worker!\n");
            }
        }
        return true;
    }
    return false;
}

class COnOfflineJob : public CMethodJob<CFdbBaseObject>
{
public:
    COnOfflineJob(CFdbBaseObject *object, FdbSessionId_t sid, bool is_last)
        : CMethodJob<CFdbBaseObject>(object, &CFdbBaseObject::callOnOffline, JOB_FORCE_RUN)
        , mSid(sid)
        , mIsLast(is_last)
    {}

    FdbSessionId_t mSid;
    bool mIsLast;
};

void CFdbBaseObject::callOnOffline(CBaseWorker *worker, CMethodJob<CFdbBaseObject> *job, CBaseJob::Ptr &ref)
{
    COnOfflineJob *the_job = fdb_dynamic_cast_if_available<COnOfflineJob *>(job);
    if (the_job)
    {
        onOffline(the_job->mSid, the_job->mIsLast);
        if (!isPrimary() && (mSid == the_job->mSid))
        {
            mSid = FDB_INVALID_ID;
        }
    }
}

bool CFdbBaseObject::migrateOnOfflineToWorker(FdbSessionId_t sid, bool is_last, CBaseWorker *worker)
{
    if (!worker)
    {
        worker = mWorker;
    }
    if (worker)
    {
        if (enableMigrate())
        {
            if (!worker->sendAsync(new COnOfflineJob(this, sid, is_last)))
            {
                LOG_E("CFdbBaseObject: Unable to migrate onoffline to worker!\n");
            }
        }
        return true;
    }
    return false;
}

void CFdbBaseObject::notifyOffline(CFdbSession *session, bool is_last)
{
    if (!migrateOnOfflineToWorker(session->sid(), is_last))
    {
        onOffline(session->sid(), is_last);
        if (!isPrimary() && (mSid == session->sid()))
        {
            mSid = FDB_INVALID_ID;
        }
    }
}

class COnOnlineJob : public CMethodJob<CFdbBaseObject>
{
public:
    COnOnlineJob(CFdbBaseObject *object, FdbSessionId_t sid, bool is_first)
        : CMethodJob<CFdbBaseObject>(object, &CFdbBaseObject::callOnOnline, JOB_FORCE_RUN)
        , mSid(sid)
        , mIsFirst(is_first)
    {}

    FdbSessionId_t mSid;
    bool mIsFirst;
};

void CFdbBaseObject::callOnOnline(CBaseWorker *worker, CMethodJob<CFdbBaseObject> *job, CBaseJob::Ptr &ref)
{
    auto the_job = fdb_dynamic_cast_if_available<COnOnlineJob *>(job);
    if (the_job)
    {
        if (!isPrimary() && (mRole == FDB_OBJECT_ROLE_CLIENT) && !fdbValidFdbId(mSid))
        {
            mSid = the_job->mSid;
        }
        onOnline(the_job->mSid, the_job->mIsFirst);
    }
}

bool CFdbBaseObject::migrateOnOnlineToWorker(FdbSessionId_t sid, bool is_first, CBaseWorker *worker)
{
    if (!worker)
    {
        worker = mWorker;
    }
    if (worker)
    {
        if (enableMigrate())
        {
            if (!worker->sendAsync(new COnOnlineJob(this, sid, is_first)))
            {
                LOG_E("CFdbBaseObject: Unable to migrate ononline to worker!\n");
            }
        }
        return true;
    }
    return false;
}

void CFdbBaseObject::notifyOnline(CFdbSession *session, bool is_first)
{
    if (isPrimary())
    {
        mEndpoint->updateSessionInfo(session);
    }

    if (!migrateOnOnlineToWorker(session->sid(), is_first))
    {
        if (!isPrimary() && (mRole == FDB_OBJECT_ROLE_CLIENT) && !fdbValidFdbId(mSid))
        {
            mSid = session->sid();
        }
        onOnline(session->sid(), is_first);
    }
}

void CFdbBaseObject::callReply(CBaseJob::Ptr &msg_ref)
{
    onReply(msg_ref);
}

bool CFdbBaseObject::migrateOnReplyToWorker(CBaseJob::Ptr &msg_ref, CBaseWorker *worker)
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
            msg->setCallable(std::bind(&CFdbBaseObject::callReply, this, _1));
            if (!worker->sendAsync(msg_ref))
            {
                LOG_E("CFdbBaseObject: Unable to migrate onreply to worker!\n");
            }
        }
        return true;
    }
    return false;
}

void CFdbBaseObject::callGetEvent(CBaseJob::Ptr &msg_ref)
{
    onGetEvent(msg_ref);
}

bool CFdbBaseObject::migrateGetEventToWorker(CBaseJob::Ptr &msg_ref, CBaseWorker *worker)
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
            msg->setCallable(std::bind(&CFdbBaseObject::callGetEvent, this, _1));
            if (!worker->sendAsync(msg_ref))
            {
                LOG_E("CFdbBaseObject: Unable to migrate onreply to worker!\n");
            }
        }
        return true;
    }
    return false;
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

bool CFdbBaseObject::migrateOnStatusToWorker(CBaseJob::Ptr &msg_ref, CBaseWorker *worker)
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
            msg->setCallable(std::bind(&CFdbBaseObject::callStatus, this, _1));
            if (!worker->sendAsync(msg_ref))
            {
                LOG_E("CFdbBaseObject: Unable to migrate onstatus to worker!\n");
            }
        }
        return true;
    }
    return false;
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
        /* broadcast current value of event/filter pair */
        broadcastCached(msg_ref);
        CFdbMessage::autoReply(msg_ref, NFdbBase::FDB_ST_AUTO_REPLY_OK, "Automatically reply to subscribe request.");
        return;
    }

    if (!migrateOnSubscribeToWorker(msg_ref))
    {
        callSubscribe(msg_ref);
    }
}

void CFdbBaseObject::doBroadcast(CBaseJob::Ptr &msg_ref)
{
    if (!migrateOnBroadcastToWorker(msg_ref))
    {
        callBroadcast(msg_ref);
    }
}

void CFdbBaseObject::doInvoke(CBaseJob::Ptr &msg_ref)
{
    if (mFlag & FDB_OBJ_ENABLE_EVENT_CACHE)
    {
        /* reply current value for 'get' request */
        auto msg = castToMessage<CBaseMessage *>(msg_ref);
        if (msg->isEventGet())
        {
            if (!msg->expectReply())
            {
                msg->status(msg_ref, NFdbBase::FDB_ST_BAD_PARAMETER, "Intend to get event but reply is not expected!");
                return;
            }
            /*
             * We come here because client intends to retrieve current value of event/topic pair.
             * It acts as a 'get' command.
             */
            auto cached_data = getCachedEventData(msg->code(), msg->topic().c_str());
            if (cached_data)
            {
                msg->replyNoQueue(msg_ref, cached_data->mBuffer, cached_data->mSize);
            }
            else
            {
                msg->statusf(msg_ref, NFdbBase::FDB_ST_NON_EXIST, "Event %d topic %s doesn't exists!",
                             msg->code(), msg->topic().c_str());
            }
            return;
        }
    }

    if (!migrateOnInvokeToWorker(msg_ref))
    {
        callInvoke(msg_ref);
    }
}

void CFdbBaseObject::doReply(CBaseJob::Ptr &msg_ref)
{
    if (!migrateOnReplyToWorker(msg_ref))
    {
        callReply(msg_ref);
    }
}

void CFdbBaseObject::doGetEvent(CBaseJob::Ptr &msg_ref)
{
    if (!migrateGetEventToWorker(msg_ref))
    {
        callGetEvent(msg_ref);
    }
}

void CFdbBaseObject::doStatus(CBaseJob::Ptr &msg_ref)
{
    if (!migrateOnStatusToWorker(msg_ref))
    {
        callStatus(msg_ref);
    }
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

bool CFdbBaseObject::sendSideband(FdbSessionId_t receiver, FdbMsgCode_t code, IFdbMsgBuilder &data)
{
    auto msg = new CBaseMessage(code, this, receiver);
    if (!msg->serialize(data, this))
    {
        delete msg;
        return false;
    }
    return msg->sendSideband();
}

bool CFdbBaseObject::sendSideband(FdbMsgCode_t code, IFdbMsgBuilder &data)
{
    return sendSideband(FDB_INVALID_ID, code, data);
}

bool CFdbBaseObject::sendSideband(FdbSessionId_t receiver, FdbMsgCode_t code,
                                  const void *buffer, int32_t size)
{
    auto msg = new CBaseMessage(code, this, receiver);
    if (!msg->serialize(buffer, size, this))
    {
        delete msg;
        return false;
    }
    return msg->sendSideband();
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
        default:
        break;
    }
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

#define FDB_MSG_TYPE_AFC_SUBSCRIBE (FDB_MSG_TYPE_SYSTEM + 1)
#define FDB_MSG_TYPE_AFC_INVOKE (FDB_MSG_TYPE_SYSTEM + 2)

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

CFdbBaseObject::tRegEntryId CFdbBaseObject::registerConnNotification(tConnCallbackFn callback,
                                                                     CBaseWorker *worker)
{
    CFdbBaseObject::tRegEntryId id = mRegIdAllocator++;
    auto &item = mConnCallbackTbl[id];
    item.mCallback = callback;
    item.mWorker = worker;
    if (mEndpoint->connected())
    {
        callback(this, FDB_INVALID_ID, true);
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
        afc_msg->mReplyCallback(msg_ref, this);
    }
}

void CFdbBaseObject::onOnline(FdbSessionId_t sid, bool is_first)
{
    CFdbEventDispatcher::tEvtHandleTbl events;
    mEvtDispather.dumpEvents(events);
    subscribeEvents(events, 0);

    for (auto it = mConnCallbackTbl.begin(); it != mConnCallbackTbl.end(); ++it)
    {
        (it->second.mCallback)(this, sid, true);
    }
}
void CFdbBaseObject::onOffline(FdbSessionId_t sid, bool is_last)
{
    for (auto it = mConnCallbackTbl.begin(); it != mConnCallbackTbl.end(); ++it)
    {
        (it->second.mCallback)(this, sid, false);
    }
}
void CFdbBaseObject::onInvoke(CBaseJob::Ptr &msg_ref)
{
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

