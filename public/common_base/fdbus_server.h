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
#include "fdbus_c_common.h"

#ifndef __FDBUS_SERVER_H__
#define __FDBUS_SERVER_H__

#ifdef __cplusplus
extern "C"
{
#endif

struct fdb_server_tag;
typedef void (*fdb_server_online_fn_t)(struct fdb_server_tag *self,
                                       FdbSessionId_t sid, fdb_bool_t is_first);
typedef void (*fdb_server_offline_fn_t)(struct fdb_server_tag *self,
                                        FdbSessionId_t sid, fdb_bool_t is_last);
typedef void (*fdb_server_invoke_fn_t)(struct fdb_server_tag *self,
                                       FdbSessionId_t sid,
                                       FdbMsgCode_t msg_code,
                                       const uint8_t *msg_data,
                                       int32_t data_size,
                                       void *reply_handle);
typedef void (*fdb_server_subscribe_fn_t)(struct fdb_server_tag *self,
                                          const fdb_subscribe_item_t *sub_items,
                                          int32_t nr_items, void *reply_handle);

typedef struct fdb_server_tag
{
    void *native_handle;
    void *user_data;
    fdb_server_online_fn_t on_online_func;
    fdb_server_offline_fn_t on_offline_func;
    fdb_server_invoke_fn_t on_invoke_func;
    fdb_server_subscribe_fn_t on_subscribe_func;
}fdb_server_t;

LIB_EXPORT
fdb_server_t *fdb_server_create(const char *name, void *user_data);
LIB_EXPORT
void *fdb_server_get_user_data(fdb_server_t *handle);
LIB_EXPORT
void fdb_server_register_event_handle(fdb_server_t *handle,
                                      fdb_server_online_fn_t on_online,
                                      fdb_server_offline_fn_t on_offline,
                                      fdb_server_invoke_fn_t on_invoke,
                                      fdb_server_subscribe_fn_t on_subscribe);

LIB_EXPORT
void fdb_server_destroy(fdb_server_t *handle);
LIB_EXPORT
fdb_bool_t fdb_server_bind(fdb_server_t *handle, const char *url);
LIB_EXPORT
fdb_bool_t fdb_server_unbind(fdb_server_t *handle);

LIB_EXPORT
void fdb_server_enable_event_cache(fdb_server_t *handle, fdb_bool_t enable);

LIB_EXPORT
fdb_bool_t fdb_server_broadcast(fdb_server_t *handle,
                                FdbMsgCode_t msg_code,
                                const char *topic,
                                const uint8_t *msg_data,
                                int32_t data_size,
                                const char *log_data);

LIB_EXPORT
fdb_bool_t fdb_message_reply(void *reply_handle,
                             const uint8_t *msg_data,
                             int32_t data_size,
                             const char *log_data);

LIB_EXPORT
fdb_bool_t fdb_message_broadcast(void *reply_handle,
                                 FdbMsgCode_t msg_code,
                                 const char *topic,
                                 const uint8_t *msg_data,
                                 int32_t data_size,
                                 const char *log_data);

LIB_EXPORT
void fdb_message_destroy(void *reply_handle);

LIB_EXPORT
void fdb_server_init_event_cache(fdb_server_t *handle,
                                 FdbMsgCode_t event,
                                 const char *topic,
                                 const uint8_t *event_data,
                                 int32_t data_size,
                                 fdb_bool_t always_update);

#ifdef __cplusplus
}
#endif
                          
#endif

