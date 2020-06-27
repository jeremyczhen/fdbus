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
            case NFdbBase::REQ_QUERY_HOST_LOCAL:
            {
                NFdbBase::FdbMsgHostAddressList host_list;
                CFdbParcelableParser parser(host_list);
                if (!msg->deserialize(parser))
                {
                    LOG_E("CNameServerProxy: unable to decode NFdbBase::FdbMsgHostAddressList.\n");
                }
                else
                {
                    printHosts(host_list, false);
                }
            }
            break;
            default:
            break;
        }

        quit();
    }
    
    void onBroadcast(CBaseJob::Ptr &msg_ref)
    {
        auto msg = castToMessage<CFdbMessage *>(msg_ref);
        switch (msg->code())
        {
            case NFdbBase::NTF_HOST_ONLINE_LOCAL:
            {
                NFdbBase::FdbMsgHostAddressList host_list;
                CFdbParcelableParser parser(host_list);
                if (!msg->deserialize(parser))
                {
                    LOG_E("CNameServerProxy: unable to decode NFdbBase::FdbMsgHostAddressList.\n");
                }
                else
                {
                    printHosts(host_list, false);
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
            addNotifyItem(subscribe_list, NFdbBase::NTF_HOST_ONLINE_LOCAL);
            subscribe(subscribe_list);
        }
        else
        {
            invoke(NFdbBase::REQ_QUERY_HOST_LOCAL);
        }
    }
    
private:
    void quit()
    {
        exit(0);
    }

    void printHosts(NFdbBase::FdbMsgHostAddressList &host_list, bool monitor)
    {
        auto &addr_list = host_list.address_list();
        for (auto it = addr_list.vpool().begin(); it != addr_list.vpool().end(); ++it)
        {
            auto &addr = *it;
            bool is_offline = addr.ns_url().empty();
            if (is_offline)
            {
                std::cout << "[" << addr.host_name() << "]"
                          << " - IP: " << addr.ip_address()
                          << " - Dropped"
                          << std::endl;
            }
            else
            {
                if (monitor)
                {
                    std::cout << "[" << addr.host_name() << "]"
                              << " - IP: " << addr.ip_address()
                              << " - Online"
                              << std::endl;
                }
                else
                {
                    std::cout << "[" << addr.host_name() << "]"
                              << " - IP: " << addr.ip_address()
                              << ", URL: " << addr.ns_url()
                              << std::endl;
                }
            }
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
    int32_t help = 0;
    const struct fdb_option core_options[] = {
        { FDB_OPTION_BOOLEAN, "follow", 'f', &ls_follow },
        { FDB_OPTION_BOOLEAN, "help", 'h', &help }
    };

    fdb_parse_options(core_options, ARRAY_LENGTH(core_options), &argc, argv);
    if (help)
    {
        std::cout << "FDBus version " << FDB_VERSION_MAJOR << "."
                                      << FDB_VERSION_MINOR << "."
                                      << FDB_VERSION_BUILD << std::endl;
        std::cout << "Usage: lshost[ -f]" << std::endl;
        std::cout << "List name of all hosts" << std::endl;
        std::cout << "    -f: keep monitoring hosts" << std::endl;
        return 0;
    }

    FDB_CONTEXT->enableLogger(false);
    FDB_CONTEXT->init();
    
    CNameServerProxy hsp;
    hsp.connect();
    FDB_CONTEXT->start(FDB_WORKER_EXE_IN_PLACE);
    return 0;
}

