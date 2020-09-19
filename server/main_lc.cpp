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
#include <stdlib.h>
#include <common_base/fdb_option_parser.h>
#include <utils/Log.h>
#include <common_base/CFdbSimpleMsgBuilder.h>

typedef std::set<std::string> FdbClientList_t;
static const char *fdb_endpoint_name = "org.fdbus.connection-fetcher";

class CConnectionFetcher : public CBaseClient
{
public:
    CConnectionFetcher(FdbClientList_t &list)
        : CBaseClient(fdb_endpoint_name)
        , mClientList(list)
    {

    }
    ~CConnectionFetcher()
    {
        disconnect();
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
                LOG_I("CConnectionFetcher: status is received: msg code: %d, id: %d, reason: %s\n", msg->code(), id, reason.c_str());
                quit();
            }
            return;
        }
        
        switch (msg->code())
        {
            case FDB_SIDEBAND_QUERY_CLIENT:
            {
                NFdbBase::FdbMsgClientTable client_tbl;
                CFdbParcelableParser parser(client_tbl);
                if (!msg->deserialize(parser))
                {
                    fprintf(stderr, "CConnectionFetcher: unable to decode NFdbBase::FdbMsgHostAddressList.\n");
                    quit();
                }
                auto iter = mClientList.find(client_tbl.server_name());
                if (iter == mClientList.end())
                {
                    fprintf(stderr, "CConnectionFetcher: unexpected service name: %s\n", client_tbl.server_name().c_str());
                    quit();
                }
                mClientList.erase(iter);
                printClients(client_tbl);
                if (mClientList.empty())
                {
                    quit();
                }
            }
            break;
            default:
            break;
        }
    }

    void onOnline(FdbSessionId_t sid, bool is_first)
    {
        invokeSideband(FDB_SIDEBAND_QUERY_CLIENT);
    }

private:
    void quit()
    {
        exit(0);
    }

    void printClients(NFdbBase::FdbMsgClientTable &client_tbl)
    {
        std::cout << "[" << client_tbl.server_name() << "]-"
                     "[" << client_tbl.endpoint_name() << "]" << std::endl;
        auto &client_list = client_tbl.client_tbl();
        for (auto it = client_list.vpool().begin(); it != client_list.vpool().end(); ++it)
        {
            auto &client_info = *it;
            if (!client_info.peer_name().compare(fdb_endpoint_name))
            {
                continue;
            }
            std::cout << "    " <<  client_info.peer_name() << "@"
                                << client_info.peer_address()
                                << ", security: " << client_info.security_level()
                                << std::endl;
        }
    }

    FdbClientList_t &mClientList;
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
    int32_t help = 0;
	const struct fdb_option core_options[] = {
        { FDB_OPTION_BOOLEAN, "help", 'h', &help }
    };

    fdb_parse_options(core_options, ARRAY_LENGTH(core_options), &argc, argv);
    if (help || (argc <= 1))
    {
        std::cout << "FDBus version " << FDB_VERSION_MAJOR << "."
                                      << FDB_VERSION_MINOR << "."
                                      << FDB_VERSION_BUILD << std::endl;
        std::cout << "Usage: lsclt service_name[ service_name ...]" << std::endl;
        std::cout << "List connected client of specified services" << std::endl;
        return 0;
    }
    
    FdbClientList_t client_tbl;
    for (int32_t i = 1; i < argc; ++i)
    {
        client_tbl.insert(argv[i]);
    }

    FDB_CONTEXT->enableLogger(false);
    FDB_CONTEXT->init();
 
    for (auto it = client_tbl.begin(); it != client_tbl.end(); ++it)
    {
        std::string server_addr;
        auto &server_name = *it;
        server_addr = FDB_URL_SVC;
        server_addr += server_name;
        auto fetcher = new CConnectionFetcher(client_tbl);
        fetcher->connect(server_addr.c_str());
    }
    
    FDB_CONTEXT->start(FDB_WORKER_EXE_IN_PLACE);
    return 0;
}

