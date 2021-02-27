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

#include <string.h>
#include <common_base/fdbus_afcomponent.h>
#include <common_base/fdbus_client.h>
#include <common_base/fdbus_server.h>
#include <common_base/CFdbAFComponent.h>
class CBaseClient;
class CBaseServer;

CBaseClient *FDB_createCClient(const char *name);
CBaseServer *FDB_createCServer(const char *name);

class CPyAFComponent : public CFdbAFComponent
{
public:
    CPyAFComponent(const char *name)
        : CFdbAFComponent(name)
    {
    }
protected:
    CBaseClient *createClient();
    CBaseServer *createServer();
};

CBaseClient *CPyAFComponent::createClient()
{
    return FDB_createCClient(mName.c_str());
}

CBaseServer *CPyAFComponent::createServer()
{
    return FDB_createCServer(mName.c_str());
}

void *fdb_create_afcomponent(const char *name)
{
    return (void *)new CPyAFComponent(name);
}

fdb_client_t *fdb_afcomponent_query_service(void *component_handle,
                                            const char *bus_name,
                                            const fdb_event_handle_t *handle_tbl,
                                            int32_t tbl_size,
                                            fdb_connection_fn_t connection_fn,
                                            void *client_user_data
                                           )
{
    if (!component_handle)
    {
        return 0;
    }

    CFdbAFComponent *component = (CFdbAFComponent *)component_handle;
    CFdbEventDispatcher::CEvtHandleTbl evt_tbl;
    if (handle_tbl)
    {
        for (int32_t i = 0; i < tbl_size; ++i)
        {
            auto &item = handle_tbl[i];
            auto fn = item.fn;
            auto user_data = item.user_data;
            component->addEvtHandle(evt_tbl, item.event, [fn, user_data]
                (CBaseJob::Ptr &msg_ref, CFdbBaseObject *obj)
                {
                    auto *fdb_msg = castToMessage<CFdbMessage *>(msg_ref);
                    fn(fdb_msg->session(),
                       fdb_msg->code(),
                       fdb_msg->getPayloadBuffer(),
                       fdb_msg->getPayloadSize(),
                       fdb_msg->topic().c_str(),
                       user_data);
                },
                item.topic);
        }
    }

    CBaseClient *client;
    if (connection_fn)
    {
        client = component->queryService(bus_name, evt_tbl, [connection_fn, client_user_data]
            (CFdbBaseObject *obj, FdbSessionId_t sid, bool is_online, bool first_or_last)
            {
                connection_fn(sid, is_online, first_or_last, client_user_data);
            });
    }
    else
    {
        client = component->queryService(bus_name, evt_tbl);
    }

    return fdb_client_create_with_handle(client_user_data, (void *)client);
}

fdb_server_t *fdb_afcomponent_offer_service(void *component_handle,
                                            const char *bus_name,
                                            const fdb_message_handle_t *handle_tbl,
                                            int32_t tbl_size,
                                            fdb_connection_fn_t connection_fn,
                                            void *server_user_data
                                           )
{
    if (!component_handle)
    {
        return 0;
    }

    CFdbAFComponent *component = (CFdbAFComponent *)component_handle;
    CFdbMsgDispatcher::CMsgHandleTbl msg_tbl;
    if (handle_tbl)
    {
        for (int32_t i = 0; i < tbl_size; ++i)
        {
            auto &item = handle_tbl[i];
            auto fn = item.fn;
            auto user_data = item.user_data;
            component->addMsgHandle(msg_tbl, item.msg, [fn, user_data]
                (CBaseJob::Ptr &msg_ref, CFdbBaseObject *obj)
                {
                    auto *fdb_msg = castToMessage<CFdbMessage *>(msg_ref);
                    auto reply_handle = new CBaseJob::Ptr(msg_ref);
                    fn(fdb_msg->session(),
                       fdb_msg->code(),
                       fdb_msg->getPayloadBuffer(),
                       fdb_msg->getPayloadSize(),
                       (void *)reply_handle,
                       user_data);
                });
        }
    }

    CBaseServer *server;
    if (connection_fn)
    {
        server = component->offerService(bus_name, msg_tbl, [connection_fn, server_user_data]
            (CFdbBaseObject *obj, FdbSessionId_t sid, bool is_online, bool first_or_last)
            {
                connection_fn(sid, is_online, first_or_last, server_user_data);
            });
    }
    else
    {
        server = component->offerService(bus_name, msg_tbl);
    }

    return fdb_server_create_with_handle(server_user_data, (void *)server);
}

