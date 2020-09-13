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

#include <string>
#include <common_base/CBaseServer.h>
#include <common_base/CFdbMessage.h>
#include <common_base/CLogProducer.h>
#include <common_base/CFdbContext.h>
#include <common_base/fdbus_server.h>
#include <common_base/CFdbMessage.h>
#define FDB_LOG_TAG "FDB_C"
#include <common_base/fdb_log_trace.h>
#include <vector>

class CCServer : public CBaseServer
{
public:
    CCServer(const char *name, fdb_server_t *server);
    ~CCServer();
protected:
    void onOnline(FdbSessionId_t sid, bool is_first);
    void onOffline(FdbSessionId_t sid, bool is_last);
    void onInvoke(CBaseJob::Ptr &msg_ref);
    void onSubscribe(CBaseJob::Ptr &msg_ref);
private:
    fdb_server_t *mServer;
};

CCServer::CCServer(const char *name, fdb_server_t *server)
    : CBaseServer(name)
    , mServer(server)
{
}

CCServer::~CCServer()
{
}

void CCServer::onOnline(FdbSessionId_t sid, bool is_first)
{
    if (!mServer || !mServer->on_online_func)
    {
        return;
    }
    mServer->on_online_func(mServer, sid, is_first);
}

void CCServer::onOffline(FdbSessionId_t sid, bool is_last)
{
    if (!mServer || !mServer->on_offline_func)
    {
        return;
    }
    mServer->on_offline_func(mServer, sid, is_last);
}

void CCServer::onInvoke(CBaseJob::Ptr &msg_ref)
{
    if (!mServer || !mServer->on_invoke_func)
    {
        return;
    }

    auto fdb_msg = castToMessage<CFdbMessage *>(msg_ref);
    if (fdb_msg)
    {
        auto reply_handle = new CBaseJob::Ptr(msg_ref);
        mServer->on_invoke_func(mServer,
                                fdb_msg->session(),
                                fdb_msg->code(),
                                fdb_msg->getPayloadBuffer(),
                                fdb_msg->getPayloadSize(),
                                reply_handle);
    }
}

void CCServer::onSubscribe(CBaseJob::Ptr &msg_ref)
{
    if (!mServer || !mServer->on_subscribe_func)
    {
        return;
    }

    auto fdb_msg = castToMessage<CFdbMessage *>(msg_ref);
    std::vector<fdb_subscribe_item_t> event_array;
    std::vector<FdbMsgCode_t> event_code_array;
    std::vector<std::string> topic_array;
    const CFdbMsgSubscribeItem *sub_item;
    FDB_BEGIN_FOREACH_SIGNAL(fdb_msg, sub_item)
    {
        auto msg_code = sub_item->msg_code();
        event_code_array.push_back(msg_code);
        topic_array.push_back(sub_item->filter());
    }
    FDB_END_FOREACH_SIGNAL()
    
    for (uint32_t i = 0; i < event_code_array.size(); ++i)
    {
        event_array.push_back({event_code_array[i], topic_array[i].c_str()});
    }
    
    if (event_array.size())
    {
        auto reply_handle = new CBaseJob::Ptr(msg_ref);
        mServer->on_subscribe_func(mServer,
                                   event_array.data(),
                                   (int32_t)event_array.size(),
                                   reply_handle);
    }
}

fdb_server_t *fdb_server_create(const char *name, void *user_data)
{
    auto c_server = new fdb_server_t();
    memset(c_server, 0, sizeof(fdb_server_t));
    c_server->user_data = user_data;
    auto fdb_server = new CCServer(name, c_server);
    c_server->native_handle = fdb_server;
    return c_server;
}

void *fdb_server_get_user_data(fdb_server_t *handle)
{
    return handle ? handle->user_data : 0;
}

void fdb_server_register_event_handle(fdb_server_t *handle,
                                      fdb_server_online_fn_t on_online,
                                      fdb_server_offline_fn_t on_offline,
                                      fdb_server_invoke_fn_t on_invoke,
                                      fdb_server_subscribe_fn_t on_subscribe)
{
    handle->on_online_func = on_online;
    handle->on_offline_func = on_offline;
    handle->on_invoke_func = on_invoke;
    handle->on_subscribe_func = on_subscribe;
}

void fdb_server_destroy(fdb_server_t *handle)
{
    if (!handle || !handle->native_handle)
    {
        return;
    }

    auto fdb_server = (CCServer *)handle->native_handle;
    fdb_server->prepareDestroy();
    delete fdb_server;
    delete handle;
}

fdb_bool_t fdb_server_bind(fdb_server_t *handle, const char *url)
{
    if (!handle || !handle->native_handle)
    {
        return fdb_false;
    }

    auto fdb_server = (CCServer *)handle->native_handle;
    fdb_server->bind(url);
    return fdb_true;
}

fdb_bool_t fdb_server_unbind(fdb_server_t *handle)
{
    if (!handle || !handle->native_handle)
    {
        return fdb_false;
    }

    auto fdb_server = (CCServer *)handle->native_handle;
    fdb_server->unbind();
    return fdb_true;
}

fdb_bool_t fdb_server_broadcast(fdb_server_t *handle,
                                FdbMsgCode_t msg_code,
                                const char *topic,
                                const uint8_t *msg_data,
                                int32_t data_size,
                                const char *log_data)
{
    if (!handle || !handle->native_handle)
    {
        return fdb_false;
    }

    auto fdb_server = (CCServer *)handle->native_handle;

    return fdb_server->broadcast(msg_code,
                                 msg_data,
                                 data_size,
                                 topic,
                                 log_data);
}

fdb_bool_t fdb_message_reply(void *reply_handle,
                             const uint8_t *msg_data,
                             int32_t data_size,
                             const char *log_data)
{
    if (!reply_handle)
    {
        return fdb_false;
    }

    auto msg_ref = (CBaseJob::Ptr *)reply_handle;
    auto msg = castToMessage<CFdbMessage *>(*msg_ref);
    if (msg)
    {
        return msg->reply(*msg_ref, msg_data, data_size, log_data);
    }
    return fdb_false;
}

fdb_bool_t fdb_message_broadcast(void *reply_handle,
                                 FdbMsgCode_t msg_code,
                                 const char *topic,
                                 const uint8_t *msg_data,
                                 int32_t data_size,
                                 const char *log_data)
{
    if (!reply_handle)
    {
        return fdb_false;
    }

    auto msg_ref = (CBaseJob::Ptr *)reply_handle;
    auto msg = castToMessage<CFdbMessage *>(*msg_ref);
    if (msg)
    {
        return msg->broadcast(msg_code, msg_data, data_size, topic, log_data);
    }
    return fdb_false;
}

void fdb_message_destroy(void *reply_handle)
{
    if (!reply_handle)
    {
        return;
    }
    auto msg_ref = (CBaseJob::Ptr *)reply_handle;
    msg_ref->reset();
    delete msg_ref;
}
void fdb_server_enable_event_cache(fdb_server_t *handle, fdb_bool_t enable)
{
    if (!handle || !handle->native_handle)
    {
        return;
    }

    auto fdb_server = (CCServer *)handle->native_handle;
    fdb_server->enableEventCache(enable);
}

void fdb_server_init_event_cache(fdb_server_t *handle,
                                 FdbMsgCode_t event,
                                 const char *topic,
                                 const uint8_t *event_data,
                                 int32_t data_size,
                                 fdb_bool_t always_update)
{
    if (!handle || !handle->native_handle)
    {
        return;
    }

    auto fdb_server = (CCServer *)handle->native_handle;
    fdb_server->initEventCache(event, topic, event_data, data_size, always_update);
}


