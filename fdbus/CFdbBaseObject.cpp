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
#include <common_base/CFdbIfMessageHeader.h>
#include <utils/Log.h>

CFdbBaseObject::CFdbBaseObject(const char *name, CBaseWorker *worker, EFdbEndpointRole role)
    : mEndpoint(0)
    , mWorker(worker)
    , mObjId(FDB_INVALID_ID)
    , mFlag(0)
    , mRole(role)
    , mSid(FDB_INVALID_ID)
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
                          , IFdbMsgBuilder &data)
{
    auto msg = new CBaseMessage(code, this, receiver);
    if (!msg->serialize(data, this))
    {
        delete msg;
        return false;
    }
    return msg->send();
}

bool CFdbBaseObject::send(FdbMsgCode_t code, IFdbMsgBuilder &data)
{
    return send(FDB_INVALID_ID, code, data);
}

bool CFdbBaseObject::send(FdbSessionId_t receiver
                         , FdbMsgCode_t code
                         , const void *buffer
                         , int32_t size
                         , const char *log_data)
{
    auto msg = new CBaseMessage(code, this, receiver);
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
                         , const char *log_data)
{
    return send(FDB_INVALID_ID, code, buffer, size, log_data);
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
    CBaseMessage msg(code, this);
    if (!msg.serialize(data))
    {
        return false;
    }
    return msg.sendLogNoQueue();
}

bool CFdbBaseObject::broadcast(FdbMsgCode_t code
                               , IFdbMsgBuilder &data
                               , const char *filter)
{
    auto msg = new CFdbBroadcastMsg(code, this, filter);
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
                              , const char *log_data)
{
    auto msg = new CFdbBroadcastMsg(code, this, filter, FDB_INVALID_ID, FDB_INVALID_ID);
    msg->setLogData(log_data);
    if (!msg->serialize(buffer, size, this))
    {
        delete msg;
        return false;
    }
    return msg->broadcast();
}

bool CFdbBaseObject::broadcastLogNoQueue(FdbMsgCode_t code, const uint8_t *log_data, int32_t log_size)
{
    CFdbBroadcastMsg msg(code, this, 0, FDB_INVALID_ID, FDB_INVALID_ID);
    if (!msg.serialize(log_data, log_size))
    {
        return false;
    }
    return msg.broadcastLogNoQueue();
}

bool CFdbBaseObject::unsubscribe(CFdbMsgSubscribeList &msg_list)
{
    auto msg = new CBaseMessage(FDB_INVALID_ID, this);
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

class COnSubscribeJob : public CMethodJob<CFdbBaseObject>
{
public:
    COnSubscribeJob(CFdbBaseObject *object, CBaseJob::Ptr &msg_ref)
        : CMethodJob<CFdbBaseObject>(object, &CFdbBaseObject::callOnSubscribe, JOB_FORCE_RUN)
    {
        mMsgRef.swap(msg_ref);
    }
    CBaseJob::Ptr mMsgRef;
};

void CFdbBaseObject::callOnSubscribe(CBaseWorker *worker, CMethodJob<CFdbBaseObject> *job, CBaseJob::Ptr &ref)
{
    auto the_job = fdb_dynamic_cast_if_available<COnSubscribeJob *>(job);
    if (the_job)
    {
        onSubscribe(the_job->mMsgRef);
        CFdbMessage::autoReply(the_job->mMsgRef, NFdbBase::FDB_ST_AUTO_REPLY_OK, "Automatically reply to subscribe request.");
    }
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
            if (!worker->sendAsync(new COnSubscribeJob(this, msg_ref)))
            {
                LOG_E("CFdbBaseObject: Unable to migrate onsubscribe to worker!\n");
            }
        }
        return true;
    }
    return false;
}

class COnBroadcastJob : public CMethodJob<CFdbBaseObject>
{
public:
    COnBroadcastJob(CFdbBaseObject *object, CBaseJob::Ptr &msg_ref)
        : CMethodJob<CFdbBaseObject>(object, &CFdbBaseObject::callOnBroadcast, JOB_FORCE_RUN)
        , mMsgRef(msg_ref)
    {
        mMsgRef.swap(msg_ref);
    }

    CBaseJob::Ptr mMsgRef;
};

void CFdbBaseObject::callOnBroadcast(CBaseWorker *worker, CMethodJob<CFdbBaseObject> *job, CBaseJob::Ptr &ref)
{
    auto the_job = fdb_dynamic_cast_if_available<COnBroadcastJob *>(job);
    if (the_job)
    {
        onBroadcast(the_job->mMsgRef);
    }
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
            if (!worker->sendAsync(new COnBroadcastJob(this, msg_ref)))
            {
                LOG_E("CFdbBaseObject: Unable to migrate onbroadcast to worker!\n");
            }
        }
        return true;
    }
    return false;
}

class COnInvokeJob : public CMethodJob<CFdbBaseObject>
{
public:
    COnInvokeJob(CFdbBaseObject *object, CBaseJob::Ptr &msg_ref)
        : CMethodJob<CFdbBaseObject>(object, &CFdbBaseObject::callOnInvoke, JOB_FORCE_RUN)
        , mMsgRef(msg_ref)
    {
        mMsgRef.swap(msg_ref);
    }

    CBaseJob::Ptr mMsgRef;
};

void CFdbBaseObject::callOnInvoke(CBaseWorker *worker, CMethodJob<CFdbBaseObject> *job, CBaseJob::Ptr &ref)
{
    auto the_job = fdb_dynamic_cast_if_available<COnInvokeJob *>(job);
    if (the_job)
    {
        onInvoke(the_job->mMsgRef);
        CFdbMessage::autoReply(the_job->mMsgRef, NFdbBase::FDB_ST_AUTO_REPLY_OK, "Automatically reply to request.");
    }
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
            if (!worker->sendAsync(new COnInvokeJob(this, msg_ref)))
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
    if (isPrimary() && (mRole == FDB_OBJECT_ROLE_CLIENT))
    {
        // send session info from client to primary object of server
        NFdbBase::FdbSessionInfo sinfo;
        sinfo.set_sender_name(mName.c_str());
        CFdbParcelableBuilder builder(sinfo);
        sendSideband(FDB_SIDEBAND_SESSION_INFO, builder);
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

class COnReplyJob : public CMethodJob<CFdbBaseObject>
{
public:
    COnReplyJob(CFdbBaseObject *object, CBaseJob::Ptr &msg_ref)
        : CMethodJob<CFdbBaseObject>(object, &CFdbBaseObject::callOnReply, JOB_FORCE_RUN)
        , mMsgRef(msg_ref)
    {}

    CBaseJob::Ptr mMsgRef;
};

void CFdbBaseObject::callOnReply(CBaseWorker *worker, CMethodJob<CFdbBaseObject> *job, CBaseJob::Ptr &ref)
{
    auto the_job = fdb_dynamic_cast_if_available<COnReplyJob *>(job);
    if (the_job)
    {
        onReply(the_job->mMsgRef);
    }
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
            if (!worker->sendAsync(new COnReplyJob(this, msg_ref)))
            {
                LOG_E("CFdbBaseObject: Unable to migrate onreply to worker!\n");
            }
        }
        return true;
    }
    return false;
}

class COnStatusJob : public CMethodJob<CFdbBaseObject>
{
public:
    COnStatusJob(CFdbBaseObject *object
                , CBaseJob::Ptr &msg_ref
                , int32_t error_code
                , const char *description)
        : CMethodJob<CFdbBaseObject>(object, &CFdbBaseObject::callOnStatus, JOB_FORCE_RUN)
        , mMsgRef(msg_ref)
        , mErrorCode(error_code)
        , mDescription(description)
    {}

    CBaseJob::Ptr mMsgRef;
    int32_t mErrorCode;
    std::string mDescription;
};

void CFdbBaseObject::callOnStatus(CBaseWorker *worker, CMethodJob<CFdbBaseObject> *job, CBaseJob::Ptr &ref)
{
    auto the_job = fdb_dynamic_cast_if_available<COnStatusJob *>(job);
    if (the_job)
    {
        onStatus(the_job->mMsgRef
                 , the_job->mErrorCode
                 , the_job->mDescription.c_str());
    }
}

bool CFdbBaseObject::migrateOnStatusToWorker(CBaseJob::Ptr &msg_ref
        , int32_t error_code
        , const char *description
        , CBaseWorker *worker)
{
    if (!worker)
    {
        worker = mWorker;
    }
    if (worker)
    {
        if (enableMigrate())
        {
            if (!worker->sendAsync(new COnStatusJob(this, msg_ref, error_code, description)))
            {
                LOG_E("CFdbBaseObject: Unable to migrate onstatus to worker!\n");
            }
        }
        return true;
    }
    return false;
}

void CFdbBaseObject::doSubscribe(CBaseJob::Ptr &msg_ref)
{
    if (!migrateOnSubscribeToWorker(msg_ref))
    {
        onSubscribe(msg_ref);
        CFdbMessage::autoReply(msg_ref, NFdbBase::FDB_ST_AUTO_REPLY_OK, "Automatically reply to subscribe request.");
    }
}

void CFdbBaseObject::doBroadcast(CBaseJob::Ptr &msg_ref)
{
    if (!migrateOnBroadcastToWorker(msg_ref))
    {
        onBroadcast(msg_ref);
    }
}

void CFdbBaseObject::doInvoke(CBaseJob::Ptr &msg_ref)
{
    if (!migrateOnInvokeToWorker(msg_ref))
    {
        onInvoke(msg_ref);
        CFdbMessage::autoReply(msg_ref, NFdbBase::FDB_ST_AUTO_REPLY_OK, "Automatically reply to request.");
    }
}

void CFdbBaseObject::doReply(CBaseJob::Ptr &msg_ref)
{
    if (!migrateOnReplyToWorker(msg_ref))
    {
        onReply(msg_ref);
    }
}

void CFdbBaseObject::doStatus(CBaseJob::Ptr &msg_ref)
{
    auto fdb_msg = castToMessage<CFdbMessage *>(msg_ref);

    int32_t error_code;
    std::string description;
    if (!fdb_msg->decodeStatus(error_code, description))
    {
        return;
    }

    if (!migrateOnStatusToWorker(msg_ref, error_code, description.c_str()))
    {
        onStatus(msg_ref, error_code, description.c_str());
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
    msg->setDestination(this);
    CFdbParcelableBuilder builder(msg_list);
    if (!msg->serialize(builder, this))
    {
        delete msg;
        return false;
    }
    return msg->subscribe(timeout);
}

bool CFdbBaseObject::subscribeSync(CFdbMsgSubscribeList &msg_list
                              , int32_t timeout)
{
    auto msg = new CBaseMessage(FDB_INVALID_ID, this);
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
    msg->setDestination(this);
    CFdbParcelableBuilder builder(msg_list);
    if (!msg->serialize(builder, this))
    {
        delete msg;
        return false;
    }
    return msg->update(timeout);
}

bool CFdbBaseObject::updateSync(CFdbMsgTriggerList &msg_list
                                , int32_t timeout)
{
    auto msg = new CBaseMessage(FDB_INVALID_ID, this);
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
    addNotifyItem(msg_list, fdbmakeEventGroup(event_group), filter);
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
    addUpdateItem(msg_list, fdbmakeEventGroup(event_group), filter);
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
    if (!filter)
    {
        filter = "";
    }
    SubscribeTable_t &subscribe_table = fdbIsGroup(msg) ? mGroupSubscribeTable : mEventSubscribeTable;
    subscribe_table[msg][session][obj_id][filter] = type;
}

void CFdbBaseObject::unsubscribe(CFdbSession *session,
                                 FdbMsgCode_t msg,
                                 FdbObjectId_t obj_id,
                                 const char *filter)
{
    SubscribeTable_t &subscribe_table = fdbIsGroup(msg) ? mGroupSubscribeTable : mEventSubscribeTable;
    
    auto it_sessions = subscribe_table.find(msg);
    if (it_sessions != subscribe_table.end())
    {
        auto &sessions = it_sessions->second;
        auto it_objects = sessions.find(session);
        if (it_objects != sessions.end())
        {
            auto &objects = it_objects->second;
            auto it_filters = objects.find(obj_id);
            if (it_filters != objects.end())
            {
                if (filter)
                {
                    auto &filters = it_filters->second;
                    auto it_type = filters.find(filter);
                    if (it_type != filters.end())
                    {
                        filters.erase(it_type);
                    }
                    if (filters.empty())
                    {
                        objects.erase(it_filters);
                    }
                }
                else
                {
                    objects.erase(it_filters);
                }
            }
            if (objects.empty())
            {
                sessions.erase(it_objects);
            }
        }
        if (sessions.empty())
        {
            subscribe_table.erase(it_sessions);
        }
    }
}

void CFdbBaseObject::unsubscribeSession(SubscribeTable_t &subscribe_table,
                                        CFdbSession *session)
{
    for (auto it_sessions = subscribe_table.begin();
            it_sessions != subscribe_table.end();)
    {
        auto the_it_sessions = it_sessions;
        ++it_sessions;

        auto &sessions = the_it_sessions->second;
        auto it_objects = sessions.find(session);
        if (it_objects != sessions.end())
        {
            sessions.erase(it_objects);
        }
        if (sessions.empty())
        {
            subscribe_table.erase(the_it_sessions);
        }
    }
}

void CFdbBaseObject::unsubscribe(CFdbSession *session)
{
    unsubscribeSession(mEventSubscribeTable, session);
    unsubscribeSession(mGroupSubscribeTable, session);
}

void CFdbBaseObject::unsubscribeObject(SubscribeTable_t &subscribe_table,
                                       FdbObjectId_t obj_id)
{
    for (auto it_sessions = subscribe_table.begin();
            it_sessions != subscribe_table.end();)
    {
        auto the_it_sessions = it_sessions;
        ++it_sessions;

        auto &sessions = the_it_sessions->second;
        for (auto it_objects = sessions.begin();
                it_objects != sessions.end();)
        {
            auto the_it_objects = it_objects;
            ++it_objects;

            auto &objects = the_it_objects->second;
            auto it_filters = objects.find(obj_id);
            if (it_filters != objects.end())
            {
                objects.erase(it_filters);
            }
            if (objects.empty())
            {
                sessions.erase(the_it_objects);
            }
        }
        if (sessions.empty())
        {
            subscribe_table.erase(the_it_sessions);
        }
    }
}

void CFdbBaseObject::unsubscribe(FdbObjectId_t obj_id)
{
    unsubscribeObject(mEventSubscribeTable, obj_id);
    unsubscribeObject(mGroupSubscribeTable, obj_id);
}

void CFdbBaseObject::broadcastOneMsg(CFdbSession *session,
                                     CFdbMessage *msg,
                                     CFdbSubscribeType type)
{
    if ((type == FDB_SUB_TYPE_NORMAL) || msg->manualUpdate())
    {
        session->sendMessage(msg);
    }
}

void CFdbBaseObject::broadcast(SubscribeTable_t &subscribe_table,
                               CFdbMessage *msg, FdbMsgCode_t event)
{
    auto it_sessions = subscribe_table.find(event);
    if (it_sessions != subscribe_table.end())
    {
        auto filter = msg->getFilter();
        if (!filter)
        {
            filter = "";
        }
        auto &sessions = it_sessions->second;
        for (auto it_objects = sessions.begin();
                it_objects != sessions.end(); ++it_objects)
        {
            auto session = it_objects->first;
            auto &objects = it_objects->second;
            for (auto it_filters = objects.begin();
                    it_filters != objects.end(); ++it_filters)
            {
                auto object_id = it_filters->first;
                msg->updateObjectId(object_id); // send to the specific object.
                auto &filters = it_filters->second;
                auto it_type = filters.find(filter);
                if (it_type == filters.end())
                {
                    /*
                     * If filter doesn't match, check who registers filter "".
                     * It represents any filter.
                     */
                    if (filter[0] != '\0')
                    {
                        auto it_type = filters.find("");
                        if (it_type != filters.end())
                        {
                            broadcastOneMsg(session, msg, it_type->second);
                        }
                    }
                }
                else
                {
                    broadcastOneMsg(session, msg, it_type->second);
                }
            }
        }
    }
}
                               
void CFdbBaseObject::broadcast(CFdbMessage *msg)
{
    broadcast(mEventSubscribeTable, msg, msg->code());
    broadcast(mGroupSubscribeTable, msg, fdbMakeGroup(msg->code()));
}

bool CFdbBaseObject::broadcast(SubscribeTable_t &subscribe_table, CFdbMessage *msg,
                               CFdbSession *session, FdbMsgCode_t event)
{
    bool sent = false;
    auto it_sessions = subscribe_table.find(event);
    if (it_sessions != subscribe_table.end())
    {
        auto &sessions = it_sessions->second;
        auto it_objects = sessions.find(session);
        if (it_objects != sessions.end())
        {
            auto &objects = it_objects->second;
            auto it_filters = objects.find(msg->objectId());
            if (it_filters != objects.end())
            {
                auto filter = msg->getFilter();
                if (!filter)
                {
                    filter = "";
                }
                auto &filters = it_filters->second;
                auto it_type = filters.find(filter);
                if (it_type == filters.end())
                {
                    /*
                     * If filter doesn't match, check who registers filter "".
                     * It represents any filter.
                     */
                    if (filter[0] != '\0')
                    {
                        auto it_type = filters.find("");
                        if (it_type != filters.end())
                        {
                            broadcastOneMsg(session, msg, it_type->second);
                            sent = true;
                        }
                    }
                }
                else
                {
                    broadcastOneMsg(session, msg, it_type->second);
                    sent = true;
                }
            }
        }
    }
    return sent;
}

bool CFdbBaseObject::broadcast(CFdbMessage *msg, CFdbSession *session)
{
    if (!broadcast(mEventSubscribeTable, msg, session, msg->code()))
    {
        return broadcast(mGroupSubscribeTable, msg, session, fdbMakeGroup(msg->code()));
    }
    return false;
}

void CFdbBaseObject::getSubscribeTable(SessionTable_t &sessions, tFdbFilterSets &filter_tbl)
{
    for (auto it_objects = sessions.begin(); it_objects != sessions.end(); ++it_objects)
    {
        auto &objects = it_objects->second;
        for (auto it_filters = objects.begin(); it_filters != objects.end(); ++it_filters)
        {
            auto &filters = it_filters->second;
            for (auto it_type = filters.begin(); it_type != filters.end(); ++it_type)
            {
                if (it_type->second == FDB_SUB_TYPE_NORMAL)
                {
                    filter_tbl.insert(it_type->first);
                }
            }
        }
    }
}

void CFdbBaseObject::getSubscribeTable(SubscribeTable_t &subscribe_table,
                                       tFdbSubscribeMsgTbl &table)
{
    for (auto it_sessions = subscribe_table.begin();
            it_sessions != subscribe_table.end(); ++it_sessions)
    {
        auto &filter_table = table[it_sessions->first];
        auto &sessions = it_sessions->second;
        getSubscribeTable(sessions, filter_table);
    }
}

void CFdbBaseObject::getSubscribeTable(tFdbSubscribeMsgTbl &table)
{
    getSubscribeTable(mEventSubscribeTable, table);
    getSubscribeTable(mGroupSubscribeTable, table);
}

void CFdbBaseObject::getSubscribeTable(SubscribeTable_t &subscribe_table, FdbMsgCode_t code,
                                       tFdbFilterSets &filters)
{
    auto it_sessions = subscribe_table.find(code);
    if (it_sessions != subscribe_table.end())
    {
        auto &sessions = it_sessions->second;
        getSubscribeTable(sessions, filters);
    }
}

void CFdbBaseObject::getSubscribeTable(FdbMsgCode_t code, tFdbFilterSets &filters)
{
    getSubscribeTable(mEventSubscribeTable, code, filters);
    getSubscribeTable(mGroupSubscribeTable, fdbMakeGroup(code), filters);
}

void CFdbBaseObject::getSubscribeTable(SubscribeTable_t &subscribe_table, FdbMsgCode_t code,
                                       CFdbSession *session, tFdbFilterSets &filter_tbl)
{
    auto it_sessions = subscribe_table.find(code);
    if (it_sessions != subscribe_table.end())
    {
        auto &sessions = it_sessions->second;
        auto it_objects = sessions.find(session);
        if (it_objects != sessions.end())
        {
            auto &objects = it_objects->second;
            for (auto it_filters = objects.begin();
                    it_filters != objects.end(); ++it_filters)
            {
                auto &filters = it_filters->second;
                for (auto it_type = filters.begin(); it_type != filters.end(); ++it_type)
                {
                    if (it_type->second == FDB_SUB_TYPE_NORMAL)
                    {
                        filter_tbl.insert(it_type->first);
                    }
                }
            }
        }
    }
}

void CFdbBaseObject::getSubscribeTable(FdbMsgCode_t code, CFdbSession *session,
                                        tFdbFilterSets &filter_tbl)
{
    getSubscribeTable(mEventSubscribeTable, code, session, filter_tbl);
    getSubscribeTable(mGroupSubscribeTable, fdbMakeGroup(code), session, filter_tbl);
}

void CFdbBaseObject::getSubscribeTable(SubscribeTable_t &subscribe_table, FdbMsgCode_t code,
                                       const char *filter, tSubscribedSessionSets &session_tbl)
{
    auto it_sessions = subscribe_table.find(code);
    if (it_sessions != subscribe_table.end())
    {
        if (!filter)
        {
            filter = "";
        }
        auto &sessions = it_sessions->second;
        for (auto it_objects = sessions.begin();
                it_objects != sessions.end(); ++it_objects)
        {
            auto session = it_objects->first;
            auto &objects = it_objects->second;
            for (auto it_filters = objects.begin();
                    it_filters != objects.end(); ++it_filters)
            {
                auto &filters = it_filters->second;
                auto it_type = filters.find(filter);
                if (it_type == filters.end())
                {
                    /*
                     * If filter doesn't match, check who registers filter "".
                     * It represents any filter.
                     */
                    if (filter[0] != '\0')
                    {
                        auto it_type = filters.find("");
                        if (it_type != filters.end() &&
                            (it_type->second == FDB_SUB_TYPE_NORMAL))
                        {
                            session_tbl.insert(session);
                            break;
                        }
                    }
                }
                else if (it_type->second == FDB_SUB_TYPE_NORMAL)
                {
                    session_tbl.insert(session);
                    break;
                }
            }
        }
    }
}

void CFdbBaseObject::getSubscribeTable(FdbMsgCode_t code, const char *filter,
                                       tSubscribedSessionSets &session_tbl)
{
    getSubscribeTable(mEventSubscribeTable, code, filter, session_tbl);
    getSubscribeTable(mGroupSubscribeTable, fdbMakeGroup(code), filter, session_tbl);
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
    auto msg = new CFdbBroadcastMsg(code, this, filter, sid, obj_id);
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
    auto msg = new CFdbBroadcastMsg(code, this, filter, sid, obj_id);
    msg->setLogData(log_data);
    if (!msg->serialize(buffer, size, this))
    {
        delete msg;
        return false;
    }
    return msg->broadcast();
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

bool CFdbBaseObject::sendSideband(FdbMsgCode_t code, IFdbMsgBuilder &data)
{
    auto msg = new CBaseMessage(code, this, FDB_INVALID_ID);
    if (!msg->serialize(data, this))
    {
        delete msg;
        return false;
    }
    return msg->sendSideband();
}

bool CFdbBaseObject::sendSideband(FdbMsgCode_t code, const void *buffer, int32_t size)
{
    auto msg = new CBaseMessage(code, this, FDB_INVALID_ID);
    if (!msg->serialize(buffer, size, this))
    {
        delete msg;
        return false;
    }
    return msg->sendSideband();
}
