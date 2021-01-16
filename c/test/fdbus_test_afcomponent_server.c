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

static void fdb_message_handle(FdbSessionId_t sid,
                               FdbMsgCode_t msg_code,
                               const uint8_t *msg_data,
                               int32_t data_size,
                               void *reply_handle,
                               void *user_data)
{
    int32_t i;

    FDB_LOG_D("on invoke: sid: %d, code: %d, size: %d\n", sid, msg_code, data_size);
    for (i = 0; i < data_size; ++i)
    {
        FDB_LOG_D("    data received: %d, user data: %s\n", msg_data[i], (char*)user_data);
    }

    uint8_t buffer[7];
    for (i = 0; i < Fdb_Num_Elems(buffer); ++i)
    {
        buffer[i] = i;
    }
    fdb_message_reply(reply_handle, buffer, Fdb_Num_Elems(buffer), 0);
}


static void fdb_connection_handle(FdbSessionId_t sid, fdb_bool_t is_online,
                                    fdb_bool_t is_first, void *user_data)
{
    FDB_LOG_D("connection status changed- sid: %d, is_online: %d, is_first: %d, user dat: %s\n",
                sid, is_online, is_first, (char*)user_data);
}

int main(int argc, char **argv)
{
    int32_t i;

    fdb_start();
    fdb_message_handle_t handle[] = {
            {FDB_TEST_MESSAGE_ID_1, fdb_message_handle, "say hello 1"},
            {FDB_TEST_MESSAGE_ID_2, fdb_message_handle, "say hello 2"},
            {FDB_TEST_MESSAGE_ID_3, fdb_message_handle, "say hello 3"},
            {FDB_TEST_MESSAGE_ID_4, fdb_message_handle, "say hello 4"},
        };
    fdb_server_t **server_array = (fdb_server_t **)malloc(sizeof(fdb_server_t *) * argc);
    for (i = 0; i < (argc - 1); ++i)
    {
        char *bus_name = argv[i + 1];
        void *component = fdb_create_afcomponent(bus_name);
        server_array[i] = fdb_afcomponent_offer_service((void *)component,
                                                        bus_name,
                                                        handle,
                                                        Fdb_Num_Elems(handle),
                                                        fdb_connection_handle,
                                                        "say 'hi'!");
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

