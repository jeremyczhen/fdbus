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
#define FDB_LOG_TAG "FDB-CCLT"
#include <common_base/fdbus_clib.h>
#include "fdbus_test_msg_ids.h"

static void on_online(fdb_client_t *self, FdbSessionId_t sid)
{
    fdb_subscribe_item_t items[] = {{FDB_TEST_EVENT_ID_1, "topic1"},
                                    {FDB_TEST_EVENT_ID_2, "topic2"},
                                    {FDB_TEST_EVENT_ID_3, "topic3"}};
    fdb_client_subscribe(self, items, Fdb_Num_Elems(items));
    FDB_LOG_D("on online: %d\n", sid);
}

static void on_offline(fdb_client_t *self, FdbSessionId_t sid)
{
    FDB_LOG_D("on offline: %d\n", sid);
}

static void on_reply(fdb_client_t *self,
                     FdbSessionId_t sid,
                     FdbMsgCode_t msg_code,
                     const uint8_t *msg_data,
                     int32_t data_size,
                     int32_t status,
                     void *user_data)
{
    int32_t i;

    FDB_LOG_D("on reply: sid: %d, code: %d, size: %d\n", sid, msg_code, data_size);
    for (i = 0; i < data_size; ++i)
    {
        FDB_LOG_D("    data received: %d\n", msg_data[i]);
    }
}

static void on_broadcast(fdb_client_t *self,
                         FdbSessionId_t sid,
                         FdbMsgCode_t msg_code,
                         const uint8_t *msg_data,
                         int32_t data_size,
                         const char *filter)
{
    int32_t i;

    FDB_LOG_D("on broadcast: sid: %d, code: %d, size: %d, filter: %s\n",
                sid, msg_code, data_size, filter);
    for (i = 0; i < data_size; ++i)
    {
        FDB_LOG_D("    data received: %d\n", msg_data[i]);
    }
}

int main(int argc, char **argv)
{
    int32_t i;

    fdb_start();
    fdb_client_t **client_array = (fdb_client_t **)malloc(sizeof(fdb_client_t *) * argc);
    for (i = 0; i < (argc - 1); ++i)
    {
        char url[1024];
        snprintf(url, sizeof(url), "svc://%s", argv[i + 1]);
        client_array[i] = fdb_client_create(argv[i + 1], 0);
        fdb_client_register_event_handle(client_array[i], on_online, on_offline, on_reply, 0, on_broadcast);
        fdb_client_connect(client_array[i], url);
    }

    uint8_t buffer[19];
    uint8_t count = 0;
    uint32_t msg_code = 0;
    while (1)
    {
        int32_t i;

        for (i = 0; i < Fdb_Num_Elems(buffer); ++i)
        {
            buffer[i] = count++;
        }
        for (i = 0; i < (argc - 1); ++i)
        {
            fdb_client_invoke_async(client_array[i], msg_code++, buffer, Fdb_Num_Elems(buffer), 0, 0, 0);
        }
        sysdep_sleep(111);
    }
}
