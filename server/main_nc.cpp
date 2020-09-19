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

#include <string>
#include <vector>
#include <iostream>
#include <common_base/CBaseServer.h>
#include <common_base/CBaseClient.h>
#include <common_base/fdb_option_parser.h>
#include <common_base/CFdbContext.h>
#include <common_base/CFdbSession.h>

class CNotificationCenter;
class CNotificationCenterProxy : public CBaseClient
{
public:
    CNotificationCenterProxy(const char *server_name, CNotificationCenter *ns);
protected:
    void onOnline(FdbSessionId_t sid, bool is_first);
private:
    CNotificationCenter *mNs;
};

class CNotificationCenter : public CBaseServer
{
public:
    CNotificationCenter(const char *name = FDB_NOTIFICATION_CENTER_NAME)
        : CBaseServer(name ? name : FDB_NOTIFICATION_CENTER_NAME)
    {
        enableEventCache(true);
    }
    void addPeer(const char *server_name, const char *peer_name)
    {
        CBaseClient *peer = new CNotificationCenterProxy(server_name, this);
        mPeerTbl.push_back(peer);

        std::string peer_url(FDB_URL_SVC);
        peer_url += peer_name;
        peer->connect(peer_url.c_str());
    }
    ~CNotificationCenter()
    {
        for (auto it = mPeerTbl.begin(); it != mPeerTbl.end(); ++it)
        {
            (*it)->prepareDestroy();
            delete *it;
        }
    }
    void syncEventPool(FdbSessionId_t sid)
    {
        auto session = FDB_CONTEXT->getSession(sid);
        if (session)
        {
            publishCachedEvents(session);
        }
    }
protected:
    void onInvoke(CBaseJob::Ptr &msg_ref)
    {
        auto msg = castToMessage<CBaseMessage *>(msg_ref);
        broadcastNoQueue(msg->code(), msg->getPayloadBuffer(), msg->getPayloadSize(),
                         msg->topic().c_str(), msg->isForceUpdate());

        auto session = FDB_CONTEXT->getSession(msg->session());
        for (auto it = mPeerTbl.begin(); it != mPeerTbl.end(); ++it)
        {
            /* avoid back and forth between NCs */
            if (session->senderName().compare((*it)->nsName()))
            {
                (*it)->publishNoQueue(msg->code()
                                      , msg->topic().c_str()
                                      , msg->getPayloadBuffer()
                                      , msg->getPayloadSize()
                                      , 0
                                      , msg->isForceUpdate());
            }
        }
    }
private:
    typedef std::vector<CBaseClient *> tPeerTbl;
    tPeerTbl mPeerTbl;
};

CNotificationCenterProxy::CNotificationCenterProxy(const char *server_name, CNotificationCenter *ns)
    : CBaseClient(server_name)
    , mNs(ns)
{
    enableReconnect(true);
}

void CNotificationCenterProxy::onOnline(FdbSessionId_t sid, bool is_first)
{
    if (is_first)
    {
        mNs->syncEventPool(sid);
    }
}

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
    char *server_name = 0;
    char *peers = 0;
    const struct fdb_option core_options[] = {
            { FDB_OPTION_STRING, "server_name", 'n', &server_name },
            { FDB_OPTION_STRING, "peers", 'p', &peers },
            { FDB_OPTION_BOOLEAN, "help", 'h', &help }
    };

    fdb_parse_options(core_options, ARRAY_LENGTH(core_options), &argc, argv);
    if (help)
    {
        std::cout << "FDBus version " << FDB_VERSION_MAJOR << "."
                                      << FDB_VERSION_MINOR << "."
                                      << FDB_VERSION_BUILD << std::endl;
        std::cout << "Usage: ntfcenter[ -n service name][ -p peer_name_1[,peer_name_2][,...]]" << std::endl;
        std::cout << "FDBus Notification Center" << std::endl;
        std::cout << "    -n: FDBus service name for notification center" << std::endl;
        std::cout << "    -p: Server name of connected notification centers, separated by ','" << std::endl;
        return 0;
    }

    uint32_t num_peers = 0;
    char **peers_array = peers ? strsplit(peers, ",", &num_peers) : 0;

    FDB_CONTEXT->init();
    
    CNotificationCenter nc(server_name);
    for (uint32_t i = 0; i < num_peers; ++i)
    {
        nc.addPeer(server_name, peers_array[i]);
    }
    nc.bind();
    FDB_CONTEXT->start(FDB_WORKER_EXE_IN_PLACE);
    return 0;
}

