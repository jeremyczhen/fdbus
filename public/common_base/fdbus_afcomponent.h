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

#ifndef __FDBUS_AFCOMPONENT_H__
#define __FDBUS_AFCOMPONENT_H__

#include "common_defs.h"
#include "fdbus_c_common.h"

#ifdef __cplusplus
extern "C"
{
#endif

struct fdb_client_tag;
struct fdb_server_tag;

typedef void (*fdb_connection_fn_t)(FdbSessionId_t sid, fdb_bool_t is_online,
                                    fdb_bool_t is_first, void *user_data);
typedef void (*fdb_event_broadcast_fn_t)(FdbSessionId_t sid,
                                         FdbMsgCode_t msg_code,
                                         const uint8_t *msg_data,
                                         int32_t data_size,
                                         const char *topic,
                                         void *user_data);
typedef void (*fdb_message_invoke_fn_t)(FdbSessionId_t sid,
                                       FdbMsgCode_t msg_code,
                                       const uint8_t *msg_data,
                                       int32_t data_size,
                                       void *reply_handle,
                                       void *user_data);

typedef struct fdb_event_handle_tag
{
    FdbMsgCode_t event;
    const char *topic;
    fdb_event_broadcast_fn_t fn;
    void *user_data;
}fdb_event_handle_t;

typedef struct fdb_message_handle_tag
{
    FdbMsgCode_t msg;
    fdb_message_invoke_fn_t fn;
    void *user_data;
}fdb_message_handle_t;

LIB_EXPORT
void *fdb_create_afcomponent(const char *name);
LIB_EXPORT
struct fdb_client_tag *fdb_afcomponent_query_service(void *component_handle,
                                            const char *bus_name,
                                            const fdb_event_handle_t *handle_tbl,
                                            int32_t tbl_size,
                                            fdb_connection_fn_t connection_fn,
                                            void *client_user_data
                                           );
LIB_EXPORT
struct fdb_server_tag *fdb_afcomponent_offer_service(void *component_handle,
                                            const char *bus_name,
                                            const fdb_message_handle_t *handle_tbl,
                                            int32_t tbl_size,
                                            fdb_connection_fn_t connection_fn,
                                            void *server_user_data
                                           );

#ifdef __cplusplus
}
#endif

#endif
