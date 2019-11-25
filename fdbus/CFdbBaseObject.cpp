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
#include <idl-gen/common.base.MessageHeader.pb.h>
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

bool CFdbBaseObject::send(FdbSessionId_t receiver
                         , FdbMsgCode_t code
                         , const void *buffer
                         , int32_t size
                         , EFdbMessageEncoding enc
                         , const char *log_data)
{
    CBaseMessage *msg = new CBaseMessage(code, this, receiver, enc);
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
                         , EFdbMessageEncoding enc
                         , const char *log_data)
{
    return send(FDB_INVALID_ID, code, buffer, size, enc, log_data);
}

bool CFdbBaseObject::sendLog(FdbMsgCode_t code, IFdbMsgBuilder &data)
{
    CBaseMessage *msg = new CBaseMessage(code, this);
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
                              , const char *filter
                              , const void *buffer
                              , int32_t size
                              , EFdbMessageEncoding enc
                              , const char *log_data)
{
    CBaseMessage *msg = new CFdbBroadcastMsg(code, this, filter, FDB_INVALID_ID, FDB_INVALID_ID, enc);
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
    CFdbBroadcastMsg msg(code, this, 0, FDB_INVALID_ID, FDB_INVALID_ID, FDB_MSG_ENC_SIMPLE);
    if (!msg.serialize(log_data, log_size))
    {
        return false;
    }
    return msg.broadcastLogNoQueue();
}

bool CFdbBaseObject::unsubscribe(CFdbMsgSubscribeList &msg_list)
{
    CBaseMessage *msg = new CBaseMessage(FDB_INVALID_ID, this);
    if (!msg->serialize(msg_list, this))
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
        , mMsgRef(msg_ref)
    {
    }
    CBaseJob::Ptr mMsgRef;
};

void CFdbBaseObject::callOnSubscribe(CBaseWorker *worker, CMethodJob<CFdbBaseObject> *job, CBaseJob::Ptr &ref)
{
    COnSubscribeJob *the_job = dynamic_cast<COnSubscribeJob *>(job);
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
    {}

    CBaseJob::Ptr mMsgRef;
};

void CFdbBaseObject::callOnBroadcast(CBaseWorker *worker, CMethodJob<CFdbBaseObject> *job, CBaseJob::Ptr &ref)
{
    COnBroadcastJob *the_job = dynamic_cast<COnBroadcastJob *>(job);
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
    {}

    CBaseJob::Ptr mMsgRef;
};

void CFdbBaseObject::callOnInvoke(CBaseWorker *worker, CMethodJob<CFdbBaseObject> *job, CBaseJob::Ptr &ref)
{
    COnInvokeJob *the_job = dynamic_cast<COnInvokeJob *>(job);
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
    COnOfflineJob *the_job = dynamic_cast<COnOfflineJob *>(job);
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
    COnOnlineJob *the_job = dynamic_cast<COnOnlineJob *>(job);
    if (the_job)
    {
        if (!isPrimary() && (mRole == FDB_OBJECT_ROLE_CLIENT) && !isValidFdbId(mSid))
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
    if (!migrateOnOnlineToWorker(session->sid(), is_first))
    {
        if (!isPrimary() && (mRole == FDB_OBJECT_ROLE_CLIENT) && !isValidFdbId(mSid))
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
    COnReplyJob *the_job = dynamic_cast<COnReplyJob *>(job);
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
    COnStatusJob *the_job = dynamic_cast<COnStatusJob *>(job);
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
    CFdbMessage *fdb_msg = castToMessage<CFdbMessage *>(msg_ref);

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
                           , EFdbMessageEncoding enc
                           , const char *log_data)
{
    CBaseMessage *msg = new CBaseMessage(code, this, receiver, enc);
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
                           , EFdbMessageEncoding enc
                           , const char *log_data)
{
    return invoke(FDB_INVALID_ID, code, buffer, size, timeout, enc, log_data);
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
    CFdbMessage *msg = castToMessage<CFdbMessage *>(msg_ref);
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
    CBaseMessage *msg = new CBaseMessage(FDB_INVALID_ID, this);
    if (!msg->serialize(msg_list, this))
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
    if (!msg->serialize(msg_list, this))
    {
        delete msg;
        return false;
    }
    return msg->subscribe(timeout);
}

bool CFdbBaseObject::subscribe(CBaseJob::Ptr &msg_ref
                              , CFdbMsgSubscribeList &msg_list
                              , int32_t timeout)
{
    CFdbMessage *msg = castToMessage<CFdbMessage *>(msg_ref);
    if (msg)
    {
        msg->setDestination(this);
        if (!msg->serialize(msg_list, this))
        {
            return false;
        }
        return msg->subscribe(msg_ref, timeout);
    }
    else
    {
        msg = new CBaseMessage(FDB_INVALID_ID, this);
        if (!msg->serialize(msg_list, this))
        {
            delete msg;
            return false;
        }
        
        CBaseJob::Ptr ref(msg);
        bool ret = msg->subscribe(ref, timeout);
        msg_ref = ref;
        return ret;
    }
}

bool CFdbBaseObject::update(CFdbMsgSubscribeList &msg_list
                            , int32_t timeout)
{
    CBaseMessage *msg = new CBaseMessage(FDB_INVALID_ID, this);
    if (!msg->serialize(msg_list, this))
    {
        delete msg;
        return false;
    }
    return msg->update(timeout);
}

bool CFdbBaseObject::update(CFdbMsgSubscribeList &msg_list
                            , CFdbMessage *msg
                            , int32_t timeout)
{
    msg->setDestination(this);
    if (!msg->serialize(msg_list, this))
    {
        delete msg;
        return false;
    }
    return msg->update(timeout);
}

bool CFdbBaseObject::update(CBaseJob::Ptr &msg_ref
                            , CFdbMsgSubscribeList &msg_list
                            , int32_t timeout)
{
    CFdbMessage *msg = castToMessage<CFdbMessage *>(msg_ref);
    if (msg)
    {
        msg->setDestination(this);
        if (!msg->serialize(msg_list, this))
        {
            return false;
        }
        return msg->update(msg_ref, timeout);
    }
    else
    {
        msg = new CBaseMessage(FDB_INVALID_ID, this);
        if (!msg->serialize(msg_list, this))
        {
            delete msg;
            return false;
        }
        
        CBaseJob::Ptr ref(msg);
        bool ret = msg->update(ref, timeout);
        msg_ref = ref;
        return ret;
    }
}

void CFdbBaseObject::addNotifyItem(CFdbMsgSubscribeList &msg_list
                                  , FdbMsgCode_t msg_code
                                  , const char *filter)
{
    CFdbMsgSubscribeItem item;
    item.set_msg_code(msg_code);
    if (filter)
    {
        item.set_filter(filter);
    }
    msg_list.addItem(item);
}

void CFdbBaseObject::addUpdateItem(CFdbMsgSubscribeList &msg_list
                                  , FdbMsgCode_t msg_code
                                  , const char *filter)
{
    CFdbMsgSubscribeItem item;
    item.set_msg_code(msg_code);
    if (filter)
    {
        item.set_filter(filter);
    }
    item.set_type(FDB_SUB_TYPE_ON_REQUEST);
    msg_list.addItem(item);
}

void CFdbBaseObject::addManualTrigger(CFdbMsgTriggerList &msg_list
                                     , FdbMsgCode_t msg_code
                                     , const char *filter)
{
    addNotifyItem(msg_list, msg_code, filter);
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
    mSessionSubscribeTable[msg][session][obj_id][filter] = type;
}

void CFdbBaseObject::unsubscribe(CFdbSession *session,
                                 FdbMsgCode_t msg,
                                 FdbObjectId_t obj_id,
                                 const char *filter)
{
    SubscribeTable_t::iterator it_sessions = mSessionSubscribeTable.find(msg);
    if (it_sessions != mSessionSubscribeTable.end())
    {
        SessionTable_t &sessions = it_sessions->second;
        SessionTable_t::iterator it_objects = sessions.find(session);
        if (it_objects != sessions.end())
        {
            ObjectTable_t &objects = it_objects->second;
            ObjectTable_t::iterator it_filters = objects.find(obj_id);
            if (it_filters != objects.end())
            {
                if (filter)
                {
                    FilterTable_t &filters = it_filters->second;
                    FilterTable_t::iterator it_type = filters.find(filter);
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
            mSessionSubscribeTable.erase(it_sessions);
        }
    }
}

void CFdbBaseObject::unsubscribe(CFdbSession *session)
{
    for (SubscribeTable_t::iterator it_sessions = mSessionSubscribeTable.begin();
            it_sessions != mSessionSubscribeTable.end();)
    {
        SubscribeTable_t::iterator the_it_sessions = it_sessions;
        ++it_sessions;

        SessionTable_t &sessions = the_it_sessions->second;
        SessionTable_t::iterator it_objects = sessions.find(session);
        if (it_objects != sessions.end())
        {
            sessions.erase(it_objects);
        }
        if (sessions.empty())
        {
            mSessionSubscribeTable.erase(the_it_sessions);
        }
    }
}

void CFdbBaseObject::unsubscribe(FdbObjectId_t obj_id)
{
    for (SubscribeTable_t::iterator it_sessions = mSessionSubscribeTable.begin();
            it_sessions != mSessionSubscribeTable.end();)
    {
        SubscribeTable_t::iterator the_it_sessions = it_sessions;
        ++it_sessions;

        SessionTable_t &sessions = the_it_sessions->second;
        for (SessionTable_t::iterator it_objects = sessions.begin();
                it_objects != sessions.end();)
        {
            SessionTable_t::iterator the_it_objects = it_objects;
            ++it_objects;

            ObjectTable_t &objects = the_it_objects->second;
            ObjectTable_t::iterator it_filters = objects.find(obj_id);
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
            mSessionSubscribeTable.erase(the_it_sessions);
        }
    }
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

void CFdbBaseObject::broadcast(CFdbMessage *msg)
{
    SubscribeTable_t::iterator it_sessions = mSessionSubscribeTable.find(msg->code());
    if (it_sessions != mSessionSubscribeTable.end())
    {
        const char *filter = msg->getFilter();
        if (!filter)
        {
            filter = "";
        }
        SessionTable_t &sessions = it_sessions->second;
        for (SessionTable_t::iterator it_objects = sessions.begin();
                it_objects != sessions.end(); ++it_objects)
        {
            CFdbSession *session = it_objects->first;
            ObjectTable_t &objects = it_objects->second;
            for (ObjectTable_t::iterator it_filters = objects.begin();
                    it_filters != objects.end(); ++it_filters)
            {
                FdbObjectId_t object_id = it_filters->first;
                msg->updateObjectId(object_id); // send to the specific object.
                FilterTable_t &filters = it_filters->second;
                FilterTable_t::iterator it_type = filters.find(filter);
                if (it_type == filters.end())
                {
                    /*
                     * If filter doesn't match, check who registers filter "".
                     * It represents any filter.
                     */
                    if (filter[0] != '\0')
                    {
                        FilterTable_t::iterator it_type = filters.find("");
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

bool CFdbBaseObject::broadcast(CFdbMessage *msg, CFdbSession *session)
{
    bool sent = false;
    SubscribeTable_t::iterator it_sessions = mSessionSubscribeTable.find(msg->code());
    if (it_sessions != mSessionSubscribeTable.end())
    {
        SessionTable_t &sessions = it_sessions->second;
        SessionTable_t::iterator it_objects = sessions.find(session);
        if (it_objects != sessions.end())
        {
            ObjectTable_t &objects = it_objects->second;
            ObjectTable_t::iterator it_filters = objects.find(msg->objectId());
            if (it_filters != objects.end())
            {
                const char *filter = msg->getFilter();
                if (!filter)
                {
                    filter = "";
                }
                FilterTable_t &filters = it_filters->second;
                FilterTable_t::iterator it_type = filters.find(filter);
                if (it_type == filters.end())
                {
                    /*
                     * If filter doesn't match, check who registers filter "".
                     * It represents any filter.
                     */
                    if (filter[0] != '\0')
                    {
                        FilterTable_t::iterator it_type = filters.find("");
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
    return sent;
}

void CFdbBaseObject::getSubscribeTable(SessionTable_t &sessions, tFdbFilterSets &filter_tbl)
{
    for (SessionTable_t::iterator it_objects = sessions.begin();
            it_objects != sessions.end(); ++it_objects)
    {
        ObjectTable_t &objects = it_objects->second;
        for (ObjectTable_t::iterator it_filters = objects.begin();
                it_filters != objects.end(); ++it_filters)
        {
            FilterTable_t &filters = it_filters->second;
            for (FilterTable_t::iterator it_type = filters.begin();
                    it_type != filters.end(); ++it_type)
            {
                if (it_type->second == FDB_SUB_TYPE_NORMAL)
                {
                    filter_tbl.insert(it_type->first);
                }
            }
        }
    }
}

void CFdbBaseObject::getSubscribeTable(tFdbSubscribeMsgTbl &table)
{
    for (SubscribeTable_t::iterator it_sessions = mSessionSubscribeTable.begin();
            it_sessions != mSessionSubscribeTable.end(); ++it_sessions)
    {
        tFdbFilterSets &filter_table = table[it_sessions->first];
        SessionTable_t &sessions = it_sessions->second;
        getSubscribeTable(sessions, filter_table);
    }
}

void CFdbBaseObject::getSubscribeTable(FdbMsgCode_t code, tFdbFilterSets &filters)
{
    SubscribeTable_t::iterator it_sessions = mSessionSubscribeTable.find(code);
    if (it_sessions != mSessionSubscribeTable.end())
    {
        SessionTable_t &sessions = it_sessions->second;
        getSubscribeTable(sessions, filters);
    }
}

void CFdbBaseObject::getSubscribeTable(FdbMsgCode_t code, CFdbSession *session,
                                        tFdbFilterSets &filter_tbl)
{
    SubscribeTable_t::iterator it_sessions = mSessionSubscribeTable.find(code);
    if (it_sessions != mSessionSubscribeTable.end())
    {
        SessionTable_t &sessions = it_sessions->second;
        SessionTable_t::iterator it_objects = sessions.find(session);
        if (it_objects != sessions.end())
        {
            ObjectTable_t &objects = it_objects->second;
            for (ObjectTable_t::iterator it_filters = objects.begin();
                    it_filters != objects.end(); ++it_filters)
            {
                FilterTable_t &filters = it_filters->second;
                for (FilterTable_t::iterator it_type = filters.begin();
                        it_type != filters.end(); ++it_type)
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

void CFdbBaseObject::getSubscribeTable(FdbMsgCode_t code, const char *filter,
                                       tSubscribedSessionSets &session_tbl)
{
    SubscribeTable_t::iterator it_sessions = mSessionSubscribeTable.find(code);
    if (it_sessions != mSessionSubscribeTable.end())
    {
        if (!filter)
        {
            filter = "";
        }
        SessionTable_t &sessions = it_sessions->second;
        for (SessionTable_t::iterator it_objects = sessions.begin();
                it_objects != sessions.end(); ++it_objects)
        {
            CFdbSession *session = it_objects->first;
            ObjectTable_t &objects = it_objects->second;
            for (ObjectTable_t::iterator it_filters = objects.begin();
                    it_filters != objects.end(); ++it_filters)
            {
                FilterTable_t &filters = it_filters->second;
                FilterTable_t::iterator it_type = filters.find(filter);
                if (it_type == filters.end())
                {
                    /*
                     * If filter doesn't match, check who registers filter "".
                     * It represents any filter.
                     */
                    if (filter[0] != '\0')
                    {
                        FilterTable_t::iterator it_type = filters.find("");
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

FdbObjectId_t CFdbBaseObject::addToEndpoint(CBaseEndpoint *endpoint, FdbObjectId_t obj_id)
{
    mEndpoint = endpoint;
    mObjId = obj_id;
    obj_id = endpoint->addObject(this);
    if (isValidFdbId(obj_id))
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
    CBindObjectJob *the_job = dynamic_cast<CBindObjectJob *>(job);
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
    CConnectObjectJob *the_job = dynamic_cast<CConnectObjectJob *>(job);
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
                      , const char *filter
                      , const void *buffer
                      , int32_t size
                      , const char *log_data)
{
    CBaseMessage *msg = new CFdbBroadcastMsg(code, this, filter, sid, obj_id);
    msg->setLogData(log_data);
    if (!msg->serialize(buffer, size, this))
    {
        delete msg;
        return false;
    }
    return msg->broadcast();
}

