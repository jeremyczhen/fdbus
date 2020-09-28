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

#include <stdlib.h>
#include <string.h>
#include <common_base/CBaseClient.h>
#include <common_base/CFdbMessage.h>
#include <common_base/CLogProducer.h>
#include <common_base/CFdbContext.h>
#include <common_base/fdbus_client.h>
#define FDB_LOG_TAG "FDB_CLT"
#include <common_base/fdb_log_trace.h>

#define FDB_MSG_TYPE_C_INVOKE (FDB_MSG_TYPE_SYSTEM + 1)

class CCClient : public CBaseClient
{
public:
    CCClient(const char *name, fdb_client_t *c_handle);
    ~CCClient();
protected:
    void onOnline(FdbSessionId_t sid, bool is_first);
    void onOffline(FdbSessionId_t sid, bool is_last);
    void onReply(CBaseJob::Ptr &msg_ref);
    void onGetEvent(CBaseJob::Ptr &msg_ref);
    void onBroadcast(CBaseJob::Ptr &msg_ref);
private:
    fdb_client_t *mClient;
};

class CCInvokeMsg : public CBaseMessage
{
public:
    CCInvokeMsg(FdbMsgCode_t code, void *user_data)
        : CBaseMessage(code)
        , mUserData(user_data)
    {
    }
    FdbMessageType_t getTypeId()
    {
        return FDB_MSG_TYPE_C_INVOKE;
    }
    void *mUserData;
};

CCClient::CCClient(const char *name, fdb_client_t *c_handle)
    : CBaseClient(name)
    , mClient(c_handle)
{
    enableReconnect(true);
}

CCClient::~CCClient()
{
}

void CCClient::onOnline(FdbSessionId_t sid, bool is_first)
{
    if (!mClient || !mClient->on_online_func)
    {
        return;
    }
    
    mClient->on_online_func(mClient, sid);
}

void CCClient::onOffline(FdbSessionId_t sid, bool is_last)
{
    if (!mClient || !mClient->on_offline_func)
    {
        return;
    }
    mClient->on_offline_func(mClient, sid);
}

void CCClient::onReply(CBaseJob::Ptr &msg_ref)
{
    if (!mClient || !mClient->on_reply_func)
    {
        return;
    }

    auto *fdb_msg = castToMessage<CFdbMessage *>(msg_ref);
    int32_t error_code = NFdbBase::FDB_ST_OK;
    if (fdb_msg->isStatus())
    {
        std::string reason;
        if (!fdb_msg->decodeStatus(error_code, reason))
        {
            FDB_LOG_E("onReply: fail to decode status!\n");
            error_code = NFdbBase::FDB_ST_MSG_DECODE_FAIL;
        }
    }

    CCInvokeMsg *c_msg = 0;
    if (fdb_msg->getTypeId() == FDB_MSG_TYPE_C_INVOKE)
    {
        c_msg = castToMessage<CCInvokeMsg *>(msg_ref);
    }

    mClient->on_reply_func(mClient,
                           fdb_msg->session(),
                           fdb_msg->code(),
                           fdb_msg->getPayloadBuffer(),
                           fdb_msg->getPayloadSize(),
                           error_code,
                           c_msg ? c_msg->mUserData : 0);
}

void CCClient::onGetEvent(CBaseJob::Ptr &msg_ref)
{
    if (!mClient || !mClient->on_reply_func)
    {
        return;
    }

    auto *fdb_msg = castToMessage<CFdbMessage *>(msg_ref);
    int32_t error_code = NFdbBase::FDB_ST_OK;
    if (fdb_msg->isStatus())
    {
        std::string reason;
        if (!fdb_msg->decodeStatus(error_code, reason))
        {
            FDB_LOG_E("onReply: fail to decode status!\n");
            error_code = NFdbBase::FDB_ST_MSG_DECODE_FAIL;
        }
    }

    CCInvokeMsg *c_msg = 0;
    if (fdb_msg->getTypeId() == FDB_MSG_TYPE_C_INVOKE)
    {
        c_msg = castToMessage<CCInvokeMsg *>(msg_ref);
    }

    mClient->on_get_event_func(mClient,
                               fdb_msg->session(),
                               fdb_msg->code(),
                               fdb_msg->topic().c_str(),
                               fdb_msg->getPayloadBuffer(),
                               fdb_msg->getPayloadSize(),
                               error_code,
                               c_msg ? c_msg->mUserData : 0);
}

void CCClient::onBroadcast(CBaseJob::Ptr &msg_ref)
{
    if (!mClient || !mClient->on_broadcast_func)
    {
        return;
    }
    auto *fdb_msg = castToMessage<CBaseMessage *>(msg_ref);
    if (fdb_msg)
    {
        mClient->on_broadcast_func(mClient,
                                   fdb_msg->session(),
                                   fdb_msg->code(),
                                   fdb_msg->getPayloadBuffer(),
                                   fdb_msg->getPayloadSize(),
                                   fdb_msg->topic().c_str());
    }
}

fdb_client_t *fdb_client_create(const char *name, void *user_data)
{
    auto c_client = new fdb_client_t();
    memset(c_client, 0, sizeof(fdb_client_t));
    c_client->user_data = user_data;
    auto fdb_client = new CCClient(name, c_client);
    c_client->native_handle = fdb_client;
    return c_client;
}

void *fdb_client_get_user_data(fdb_client_t *handle)
{
    return handle ? handle->user_data : 0;
}

void fdb_client_register_event_handle(fdb_client_t *handle,
                                      fdb_client_online_fn_t on_online,
                                      fdb_client_offline_fn_t on_offline,
                                      fdb_client_reply_fn_t on_reply,
                                      fdb_client_get_event_fn_t on_get_event,
                                      fdb_client_broadcast_fn_t on_broadcast)
{
    if (!handle)
    {
        return;
    }
    handle->on_online_func = on_online;
    handle->on_offline_func = on_offline;
    handle->on_reply_func = on_reply;
    handle->on_get_event_func = on_get_event;
    handle->on_broadcast_func = on_broadcast;
}

void fdb_client_destroy(fdb_client_t *handle)
{
    if (!handle || !handle->native_handle)
    {
        return;
    }
    auto fdb_client = (CCClient *)handle->native_handle;
    fdb_client->prepareDestroy();
    delete fdb_client;
    delete handle;
}

fdb_bool_t fdb_client_connect(fdb_client_t *handle, const char *url)
{
    if (!handle || !handle->native_handle)
    {
        return fdb_false;
    }
    
    auto fdb_client = (CCClient *)handle->native_handle;
    fdb_client->connect(url);
    return fdb_true;
}

fdb_bool_t fdb_client_disconnect(fdb_client_t *handle)
{
    if (!handle || !handle->native_handle)
    {
        return fdb_false;
    }
    
    auto fdb_client = (CCClient *)handle->native_handle;
    fdb_client->disconnect();
    return fdb_true;
}

fdb_bool_t fdb_client_invoke_async(fdb_client_t *handle,
                                   FdbMsgCode_t msg_code,
                                   const uint8_t *msg_data,
                                   int32_t data_size,
                                   int32_t timeout,
                                   void *user_data,
                                   const char *log_data)
{
    if (!handle || !handle->native_handle)
    {
        return fdb_false;
    }
    
    auto fdb_client = (CCClient *)handle->native_handle;
    
    auto fdb_msg = new CCInvokeMsg(msg_code, user_data);
    fdb_msg->setLogData(log_data);
    return fdb_client->invoke(fdb_msg, msg_data, data_size, timeout);
}

fdb_bool_t fdb_client_invoke_sync(fdb_client_t *handle,
                                  FdbMsgCode_t msg_code,
                                  const uint8_t *msg_data,
                                  int32_t data_size,
                                  int32_t timeout,
                                  const char *log_data,
                                  fdb_message_t *ret_msg)
{
    if (ret_msg)
    {
        ret_msg->status = NFdbBase::FDB_ST_UNKNOWN;
        ret_msg->msg_buffer = 0;
    }
    if (!handle || !handle->native_handle)
    {
        return fdb_false;
    }
    
    auto fdb_client = (CCClient *)handle->native_handle;
    
    auto fdb_msg = new CBaseMessage(msg_code);
    fdb_msg->setLogData(log_data);
    CBaseJob::Ptr ref(fdb_msg);
    if (!fdb_client->invoke(ref, msg_data, data_size, timeout))
    {
        FDB_LOG_E("fdb_client_invoke_sync: unable to call method\n");
        return fdb_false;
    }

    int32_t error_code = NFdbBase::FDB_ST_OK;
    if (fdb_msg->isStatus())
    {
        std::string reason;
        if (!fdb_msg->decodeStatus(error_code, reason))
        {
            FDB_LOG_E("onReply: fail to decode status!\n");
            error_code = NFdbBase::FDB_ST_MSG_DECODE_FAIL;
        }
    }

    if (ret_msg)
    {
        ret_msg->sid = fdb_msg->session();
        ret_msg->msg_code = fdb_msg->code();
        ret_msg->msg_data = fdb_msg->getPayloadBuffer();
        // avoid buffer from being released.
        ret_msg->msg_buffer = fdb_msg->ownBuffer();
        ret_msg->data_size = fdb_msg->getPayloadSize();
        ret_msg->status = error_code;
    }

    return fdb_true;
}

void fdb_client_release_return_msg(fdb_message_t *ret_msg)
{
    if (ret_msg)
    {
        if (ret_msg->topic)
        {
            free(ret_msg->topic);
        }
        if (ret_msg->msg_buffer)
        {
            delete[] (uint8_t *)ret_msg->msg_buffer;
        }
    }
}
                                  
fdb_bool_t fdb_client_send(fdb_client_t *handle,
                           FdbMsgCode_t msg_code,
                           const uint8_t *msg_data,
                           int32_t data_size,
                           const char *log_data)
{
    if (!handle || !handle->native_handle)
    {
        return fdb_false;
    }
    
    auto fdb_client = (CCClient *)handle->native_handle;
    
    return fdb_client->send(msg_code, msg_data, data_size, log_data);
}

fdb_bool_t fdb_client_subscribe(fdb_client_t *handle,
                                const fdb_subscribe_item_t *sub_items,
                                int32_t nr_items)
{
    if (!handle || !handle->native_handle)
    {
        return fdb_false;
    }
    
    auto fdb_client = (CCClient *)handle->native_handle;

    if (!nr_items || !sub_items)
    {
        return fdb_false;
    }

    CFdbMsgSubscribeList subscribe_list;
    for (int32_t i = 0; i < nr_items; ++i)
    {
        fdb_client->addNotifyItem(subscribe_list,
                                  sub_items[i].event_code,
                                  sub_items[i].topic);
    }
    return fdb_client->subscribe(subscribe_list);
}

fdb_bool_t fdb_client_unsubscribe(fdb_client_t *handle,
                                  const fdb_subscribe_item_t *sub_items,
                                  int32_t nr_items)
{
    if (!handle || !handle->native_handle)
    {
        return fdb_false;
    }
    
    auto fdb_client = (CCClient *)handle->native_handle;

    if (!nr_items || !sub_items)
    {
        return fdb_false;
    }

    CFdbMsgSubscribeList subscribe_list;
    for (int32_t i = 0; i < nr_items; ++i)
    {
        fdb_client->addNotifyItem(subscribe_list,
                                  sub_items[i].event_code,
                                  sub_items[i].topic);
    }
    return fdb_client->unsubscribe(subscribe_list);
}

fdb_bool_t fdb_client_publish(fdb_client_t *handle,
                              FdbMsgCode_t event,
                              const char *topic,
                              const uint8_t *event_data,
                              int32_t data_size,
                              const char *log_data,
                              fdb_bool_t always_update)
{
    if (!handle || !handle->native_handle)
    {
        return fdb_false;
    }
    
    auto fdb_client = (CCClient *)handle->native_handle;
    
    return fdb_client->publish(event, event_data, data_size, topic, always_update, log_data);
}

fdb_bool_t fdb_client_get_event_async(fdb_client_t *handle,
                                      FdbMsgCode_t event,
                                      const char *topic,
                                      int32_t timeout,
                                      void *user_data)
{
    if (!handle || !handle->native_handle)
    {
        return fdb_false;
    }
    
    auto fdb_client = (CCClient *)handle->native_handle;
    
    auto fdb_msg = new CCInvokeMsg(event, user_data);
    return fdb_client->get(fdb_msg, topic, timeout);
}

fdb_bool_t fdb_client_get_event_sync(fdb_client_t *handle,
                                     FdbMsgCode_t event,
                                     const char *topic,
                                     int32_t timeout,
                                     fdb_message_t *ret_msg)
{
    if (ret_msg)
    {
        ret_msg->status = NFdbBase::FDB_ST_UNKNOWN;
        ret_msg->msg_buffer = 0;
        ret_msg->topic = 0;
    }
    if (!handle || !handle->native_handle)
    {
        return fdb_false;
    }
    
    auto fdb_client = (CCClient *)handle->native_handle;
    
    auto fdb_msg = new CBaseMessage(event);
    CBaseJob::Ptr ref(fdb_msg);
    if (!fdb_client->get(ref, topic, timeout))
    {
        FDB_LOG_E("fdb_client_get_event_sync: unable to call method\n");
        return fdb_false;
    }
    
    int32_t error_code = NFdbBase::FDB_ST_OK;
    if (fdb_msg->isStatus())
    {
        std::string reason;
        if (!fdb_msg->decodeStatus(error_code, reason))
        {
            FDB_LOG_E("onReply: fail to decode status!\n");
            error_code = NFdbBase::FDB_ST_MSG_DECODE_FAIL;
        }
    }
    
    if (ret_msg)
    {
        ret_msg->sid = fdb_msg->session();
        ret_msg->msg_code = fdb_msg->code();
        ret_msg->topic = strdup(fdb_msg->topic().c_str());
        ret_msg->msg_data = fdb_msg->getPayloadBuffer();
        // avoid buffer from being released.
        ret_msg->msg_buffer = fdb_msg->ownBuffer();
        ret_msg->data_size = fdb_msg->getPayloadSize();
        ret_msg->status = error_code;
    }
    
    return fdb_true;
}

