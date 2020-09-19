
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

#include <set>
#include <string>
#include <common_base/CFdbContext.h>
#include <common_base/CBaseClient.h>
#include <common_base/CFdbIfNameServer.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <utils/Log.h>
#include <common_base/CFdbSimpleMsgBuilder.h>

static const char *fdb_endpoint_name = "org.fdbus.event-fetcher";
static int32_t fdb_obj_id = FDB_OBJECT_MAIN;
static CBaseClient *fdb_event_fetcher = 0;

template <class T>
class CEventFetcher : public T
{
public:
    CEventFetcher(const char *name)
        : T(name)
    {

    }
    ~CEventFetcher()
    {
        this->disconnect();
    }

protected:
    void onSidebandReply(CBaseJob::Ptr &msg_ref)
    {
        auto msg = castToMessage<CFdbMessage *>(msg_ref);
        if (msg->isStatus())
        {
            if (msg->isError())
            {
                int32_t id;
                std::string reason;
                msg->decodeStatus(id, reason);
                LOG_I("CEventFetcher: status is received: msg code: %d, id: %d, reason: %s\n", msg->code(), id, reason.c_str());
            }
            quit();
            return;
        }
        
        switch (msg->code())
        {
            case FDB_SIDEBAND_QUERY_EVT_CACHE:
            {
                NFdbBase::FdbMsgEventCache event_tbl;
                CFdbParcelableParser parser(event_tbl);
                if (!msg->deserialize(parser))
                {
                    fprintf(stderr, "CEventFetcher: unable to decode NFdbBase::FdbMsgHostAddressList.\n");
                    quit();
                }
                printEvents(event_tbl);
                quit();
            }
            break;
            default:
            break;
        }
    }

    void onOnline(FdbSessionId_t sid, bool is_first)
    {
        if (this->isPrimary())
        {
            if (fdb_obj_id == FDB_OBJECT_MAIN)
            {
                this->invokeSideband(FDB_SIDEBAND_QUERY_EVT_CACHE);
            }
            else
            {
                CEventFetcher<CFdbBaseObject> *obj = new CEventFetcher<CFdbBaseObject>("fdbus.__fetcher_object__");
                obj->connect(fdb_event_fetcher, fdb_obj_id);
                obj->invokeSideband(FDB_SIDEBAND_QUERY_EVT_CACHE);
            }
        }
    }

private:
    void quit()
    {
        exit(0);
    }

    void printEvents(NFdbBase::FdbMsgEventCache &event_tbl)
    {
        auto &event_list = event_tbl.cache();
        printf("| %-10s | %-32s | %-10s |\n", "**EVENT**", "**TOPIC**", "**SIZE**");
        for (auto it = event_list.vpool().begin(); it != event_list.vpool().end(); ++it)
        {
            auto &event_info = *it;
            printf("| %-10d | %-32s | %-10d |\n", event_info.event(), event_info.topic().c_str(), event_info.size());
        }
    }
};

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
    if (argc <= 1)
    {
        std::cout << "FDBus version " << FDB_VERSION_MAJOR << "."
                                      << FDB_VERSION_MINOR << "."
                                      << FDB_VERSION_BUILD << std::endl;
        std::cout << "Usage: lsevt service_name[ object_id]" << std::endl;
        std::cout << "List cached events of specified servers" << std::endl;
        return 0;
    }

    char *server_name = argv[1];
    if (argc > 2)
    {
        fdb_obj_id = atoi(argv[2]);
    }
    
    FDB_CONTEXT->enableLogger(false);
    FDB_CONTEXT->init();
 
    std::string server_addr;
    server_addr = FDB_URL_SVC; 
    server_addr += server_name;
    fdb_event_fetcher = new CEventFetcher<CBaseClient>(fdb_endpoint_name);
    fdb_event_fetcher->connect(server_addr.c_str());
    
    FDB_CONTEXT->start(FDB_WORKER_EXE_IN_PLACE);
    return 0;
}

