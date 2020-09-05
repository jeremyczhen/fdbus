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
#include <common_base/CBaseClient.h>
#include <common_base/CFdbIfNameServer.h>
#include <iostream>
#include <stdlib.h>
#include <common_base/fdb_option_parser.h>
#include <utils/Log.h>
#include <common_base/CFdbSimpleMsgBuilder.h>

static int32_t ls_verbose = 0;
static int32_t ls_follow = 0;

class CNameServerProxy : public CBaseClient
{
public:
    CNameServerProxy()
        : CBaseClient(FDB_NAME_SERVER_NAME)
    {

    }
    ~CNameServerProxy()
    {
        disconnect();
    }
protected:
    void onReply(CBaseJob::Ptr &msg_ref)
    {
        auto msg = castToMessage<CFdbMessage *>(msg_ref);
        if (msg->isStatus())
        {
            if (msg->isError())
            {
                int32_t id;
                std::string reason;
                msg->decodeStatus(id, reason);
                LOG_I("CNameServerProxy: status is received: msg code: %d, id: %d, reason: %s\n", msg->code(), id, reason.c_str());
                quit();
            }
            return;
        }
        
        switch (msg->code())
        {
            case NFdbBase::REQ_QUERY_SERVICE:
            {
                NFdbBase::FdbMsgServiceTable svc_tbl;
                CFdbParcelableParser parser(svc_tbl);
                if (!msg->deserialize(parser))
                {
                    LOG_E("CNameServerProxy: unable to decode NFdbBase::FdbMsgServiceTable.\n");
                    quit();
                }
                const char *prev_ip = "";
                const char *prev_host = "";
                auto &svc_list = svc_tbl.service_tbl();
                for (auto svc_it = svc_list.vpool().begin(); svc_it != svc_list.vpool().end(); ++svc_it)
                {
                    auto &host_addr = svc_it->host_addr();
                    auto &service_addr = svc_it->service_addr();
                    const char *location = service_addr.is_local() ? "(local)" : "(remote)";

                    if (host_addr.ip_address().compare(prev_ip) || host_addr.host_name().compare(prev_host))
                    {
                        std::cout << "[" << host_addr.host_name() << location << "]"
                                  << " - IP: " << host_addr.ip_address()
                                  << ", URL: " << host_addr.ns_url()
                                  << std::endl;
                        prev_ip = host_addr.ip_address().c_str();
                        prev_host = host_addr.host_name().c_str();
                    }
                    std::cout << "    [" << service_addr.service_name() << "]" << std::endl;
                    auto &addr_list = service_addr.address_list();
                    for (auto addr_it = addr_list.pool().begin();
                            addr_it != addr_list.pool().end(); ++addr_it)

                    {
                        std::cout << "        > " << *addr_it << std::endl;
                    }
                }
            }
            break;
            default:
            break;
        }
        if (!ls_follow)
        {
            quit();
        }
    }

    void onBroadcast(CBaseJob::Ptr &msg_ref)
    {
        auto msg = castToMessage<CFdbMessage *>(msg_ref);
        switch (msg->code())
        {
            case NFdbBase::NTF_SERVICE_ONLINE_MONITOR:
            {
                NFdbBase::FdbMsgAddressList msg_addr_list;
                CFdbParcelableParser parser(msg_addr_list);
                if (!msg->deserialize(parser))
                {
                    LOG_E("CNameServerProxy: unable to decode NFdbBase::FdbMsgAddressList.\n");
                    return;
                }
                const char *location = msg_addr_list.is_local() ? "(local)" : "(remote)";
                
                if (msg_addr_list.address_list().empty())
                {
                    std::cout << "[" << msg_addr_list.service_name()
                              << "]@" << msg_addr_list.host_name() << location
                              << " - Dropped" << std::endl;
                }
                else
                {
                    std::cout << "[" << msg_addr_list.service_name()
                              << "]@" << msg_addr_list.host_name() << location
                              << " - Online" << std::endl;
                    const CFdbParcelableArray<std::string> &addr_list = msg_addr_list.address_list();
                    for (CFdbParcelableArray<std::string>::tPool::const_iterator it = addr_list.pool().begin();
                            it != addr_list.pool().end(); ++it)
                    {
                        std::cout << "    > " << *it << std::endl;
                    }
                }
                
                if (ls_verbose)
                {
                    invoke(NFdbBase::REQ_QUERY_SERVICE);
                }
            }
            break;
            default:
            break;
        }
    }

    void onOnline(FdbSessionId_t sid, bool is_first)
    {
        if (ls_follow)
        {
            CFdbMsgSubscribeList subscribe_list;
            addNotifyItem(subscribe_list, NFdbBase::NTF_SERVICE_ONLINE_MONITOR);
            subscribe(subscribe_list);
        }
        else
        {
            invoke(NFdbBase::REQ_QUERY_SERVICE);
        }
    }
    
private:
    void quit()
    {
        exit(0);
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
    int32_t help = 0;
	const struct fdb_option core_options[] = {
            { FDB_OPTION_BOOLEAN, "follow", 'f', &ls_follow },
            { FDB_OPTION_BOOLEAN, "verbose", 'v', &ls_verbose },
            { FDB_OPTION_BOOLEAN, "help", 'h', &help }
    };

    fdb_parse_options(core_options, ARRAY_LENGTH(core_options), &argc, argv);
    if (help)
    {
        std::cout << "FDBus version " << FDB_VERSION_MAJOR << "."
                                      << FDB_VERSION_MINOR << "."
                                      << FDB_VERSION_BUILD << std::endl;
        std::cout << "Usage: lssvc[ -f][ -v]" << std::endl;
        std::cout << "List name of all services" << std::endl;
        std::cout << "    -f: keep monitoring service name" << std::endl;
        std::cout << "    -v: verbose mode" << std::endl;
        return 0;
    }

    FDB_CONTEXT->enableLogger(false);
    FDB_CONTEXT->init();
    
    CNameServerProxy nsp;
    nsp.connect();
    FDB_CONTEXT->start(FDB_WORKER_EXE_IN_PLACE);
    return 0;
}

