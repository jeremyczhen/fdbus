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

#include <common_base/CFdbContext.h>
#include <stdio.h>
#define FDB_LOG_TAG "FDB_C"
#include <common_base/fdbus_clib.h>

#ifdef __WIN32__
// Need to link with Ws2_32.lib
#pragma comment(lib, "ws2_32.lib")
#endif

fdb_bool_t fdb_start()
{
#ifdef __WIN32__
    WORD wVersionRequested;
    WSADATA wsaData;
    int err;

    /* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
    wVersionRequested = MAKEWORD(2, 2);

    err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0)
    {
        /* Tell the user that we could not find a usable */
        /* Winsock DLL.                                  */
        printf("WSAStartup failed with error: %d\n", err);
        return 1;
    }
#endif
    return FDB_CONTEXT->start();
}

/* for python only; for c, please refer to fdb_log_trace.h */
void fdb_log_trace(EFdbLogLevel level, const char *tag, const char *log_data)
{
    switch (level)
    {
        case FDB_LL_DEBUG:
            FDB_TLOG_D(tag, "%s\n", log_data);
        break;
        case FDB_LL_INFO:
            FDB_TLOG_I(tag, "%s\n", log_data);
        break;
        case FDB_LL_WARNING:
            FDB_TLOG_W(tag, "%s\n", log_data);
        break;
        case FDB_LL_ERROR:
            FDB_TLOG_E(tag, "%s\n", log_data);
        break;
        case FDB_LL_FATAL:
            FDB_TLOG_F(tag, "%s\n", log_data);
        break;
        default:
        break;
    }
}
