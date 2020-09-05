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
#include <stdio.h>
#define FDB_LOG_TAG "FDB-SVC"
#include <common_base/fdbus_clib.h>
#include "fdbus_test_msg_ids.h"

static void on_online(fdb_server_t *self, FdbSessionId_t sid, fdb_bool_t is_first)
{
    FDB_LOG_D("on online: %d, first: %s\n", sid, is_first ? "true" : "false");
}

static void on_offline(fdb_server_t *self, FdbSessionId_t sid, fdb_bool_t is_last)
{
    FDB_LOG_D("on offline: %d, last: %s\n", sid, is_last ? "true" : "false");
}

static void on_invoke(struct fdb_server_tag *self,
		      FdbSessionId_t sid,
		      FdbMsgCode_t msg_code,
		      const uint8_t *msg_data,
		      int32_t data_size,
		      void *reply_handle)
{
    int32_t i;

    FDB_LOG_D("on invoke: sid: %d, code: %d, size: %d\n", sid, msg_code, data_size);
    for (i = 0; i < data_size; ++i)
    {
        FDB_LOG_D("    data received: %d\n", msg_data[i]);
    }

    uint8_t buffer[7];
    for (i = 0; i < Fdb_Num_Elems(buffer); ++i)
    {
        buffer[i] = i;
    }
    fdb_message_reply(reply_handle, buffer, Fdb_Num_Elems(buffer), 0);
}

static void on_subscribe(struct fdb_server_tag *self,
		         const fdb_subscribe_item_t *sub_items,
		         int32_t nr_items, void *reply_handle)
{
    int32_t i;

    FDB_LOG_D("on subscribe: nr_items: %d\n", nr_items);
    uint8_t buffer[18];
    for (i = 0; i < Fdb_Num_Elems(buffer); ++i)
    {
        buffer[i] = i + 100;
    }
    for (i = 0; i < nr_items; ++i)
    {
        FDB_LOG_D("on subscribe: broadcast event %d with topic %s\n", sub_items[i].event_code, sub_items[i].topic);
        fdb_message_broadcast(reply_handle,
                              sub_items[i].event_code,
                              sub_items[i].topic,
                              buffer,
                              Fdb_Num_Elems(buffer),
                              0);
    }
}

int main(int argc, char **argv)
{
    int32_t i;

    fdb_start();
    fdb_server_t **server_array = (fdb_server_t **)malloc(sizeof(fdb_server_t *) * argc);
    for (i = 0; i < (argc - 1); ++i)
    {
        char url[1024];
        snprintf(url, sizeof(url), "svc://%s", argv[i + 1]);
        server_array[i] = fdb_server_create(argv[i + 1], 0);
        fdb_server_register_event_handle(server_array[i], on_online, on_offline, on_invoke, on_subscribe);
        fdb_server_bind(server_array[i], url);
    }

    uint8_t buffer[12];
    uint8_t count = 0;
    while (1)
    {
        for (i = 0; i < Fdb_Num_Elems(buffer); ++i)
        {
            buffer[i] = count++;
        }
        for (i = 0; i < (argc - 1); ++i)
        {
            fdb_server_broadcast(server_array[i], FDB_TEST_EVENT_ID_1, "topic1", buffer, Fdb_Num_Elems(buffer), 0);
            fdb_server_broadcast(server_array[i], FDB_TEST_EVENT_ID_2, "topic2", buffer, Fdb_Num_Elems(buffer), 0);
            fdb_server_broadcast(server_array[i], FDB_TEST_EVENT_ID_3, "topic3", buffer, Fdb_Num_Elems(buffer), 0);
        }
        sysdep_sleep(123);
    }
}
