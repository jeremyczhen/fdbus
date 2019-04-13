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
#include "CHostServer.h"

#if __WIN32__
// Need to link with Ws2_32.lib
#pragma comment(lib, "ws2_32.lib")
#endif

int main(int argc, char **argv)
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
    FDB_CONTEXT->enableLogger(false);
    FDB_CONTEXT->init();
    CHostServer *hs = new CHostServer();
    if (!hs)
    {
        return 0;
    }

    hs->bind();
    FDB_CONTEXT->start(FDB_WORKER_EXE_IN_PLACE);
    return 0;
}
