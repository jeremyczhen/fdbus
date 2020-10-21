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

#include <iostream>
#define FDB_LOG_TAG "FDB_TEST_APPFW"
#include <common_base/fdbus.h>


using namespace std::placeholders;
/*
 * |----------------------------------------------------|       |---------|
 * |       APP Framework running at default worker      |       |         | 
 * |                                       +----------+ |       |         |
 * |                         +------------>| client 1 |-------->|         |
 * | +-------------+         |             +----------+ |       |         |
 * | | component 1 |-------->|             +----------+ |       |         |
 * | +-------------+         +------------>| client 2 |-------->|         |
 * |                         |             +----------+ |       |         |-----> UDS Sockets
 * |                         |             +----------+ |       |         |
 * |                         +------------>| client N |-------->|         |
 * |                         |             +----------+ |       | FDBus   |
 * |                         |             +----------+ |       | Context |
 * |                         +------------>| server 1 |-------->| Worker  |
 * | +-------------+         |             +----------+ |       |         |-----> INET Sockets
 * | | component 2 |-------->|             +----------+ |       |         |
 * | +-------------+         +------------>| server 2 |-------->|         |
 * |                         |             +----------+ |       |         |
 * |                         |             +----------+ |       |         |
 * |                         +------------>| server N |-------->|         |
 * |                                       +----------+ |       |         |
 * |----------------------------------------------------|       |---------|
 */

enum RadioEventId
{
    EVT_RADIO_1,
    EVT_RADIO_2,
    EVT_RADIO_3
};
enum RadioMessageId
{
    MSG_RADIO_1,
    MSG_RADIO_2,
    MSG_RADIO_3,
    MSG_RADIO_4
};

class MyComponent;

// define a timer to invoke (for client) or broadcast (for server) periodically
class MyTimer : public CBaseLoopTimer
{
public:
    MyTimer(MyComponent &comp);
protected:
    void run();
private:
    MyComponent &mComp;
};

// define a component which contains several fdbus clients and servers
class MyComponent : public CFdbAFComponent
{
public:
    MyComponent(const char *comp_name, int32_t method_start,
                char **radio_client_name, uint32_t nr_clients,
                char **radio_server_name, uint32_t nr_servers)
        : CFdbAFComponent(comp_name)
        , mRadioClient(0)
        , mRadioServer(0)
        , mTimer(*this)
        , mFirstMethodId(method_start)
    {
        if (radio_client_name && nr_clients)
        {
            for (uint32_t i = 0; i < nr_clients; ++i)
            {
                connectClient(radio_client_name[i]);
            }
        }
        if (radio_server_name && nr_servers)
        {
            for (uint32_t i = 0; i < nr_servers; ++i)
            {
                bindServer(radio_server_name[i]);
            }
        }

        mWorker.start();
        mTimer.attach(&mWorker, true);
    }


    void connectClient(const char *name)
    {
        // callback which will be called when server is online/offline
        CFdbAFClient::tConnCallbackFn cb = std::bind(&MyComponent::onRadioClientOnline,
                                                       this, _1, _2);
        // a list of callbacks that will be called when associated event is broadcasted
        CFdbEventDispatcher::CEvtHandleTbl evt_tbl;
        CFdbEventDispatcher::tEvtCallbackFn cb1 = std::bind(&MyComponent::onRadioClientBroadcast1,
                                                            this, _1, _2);
        CFdbEventDispatcher::tEvtCallbackFn cb2 = std::bind(&MyComponent::onRadioClientBroadcast2,
                                                            this, _1, _2);
        CFdbEventDispatcher::tEvtCallbackFn cb3 = std::bind(&MyComponent::onRadioClientBroadcast3,
                                                            this, _1, _2);
        evt_tbl.add(EVT_RADIO_1, cb1, "topic1");
        evt_tbl.add(EVT_RADIO_2, cb2, "topic1");
        evt_tbl.add(EVT_RADIO_3, cb3, "topic1");
        // connect to FDBus server: if the server is already connected, just register the
        // callbacks; otherwise connecting to the service indicated by 'name' followed by
        // registering the callbacks.
        mRadioClient = queryService(name, evt_tbl, cb);
    }

    void bindServer(const char *name)
    {
        // callback when a client is connected
        CFdbAFServer::tConnCallbackFn cb = std::bind(&MyComponent::onRadioServerOnline,
                                                      this, _1, _2, _3);
        // list of callbacks that will be called when associated method is invoked
        CFdbMsgDispatcher::CMsgHandleTbl msg_tbl;
        CFdbMsgDispatcher::tMsgCallbackFn cb1 = std::bind(&MyComponent::onRadioServerInvoke1,
                                                          this, _1, _2);
        CFdbMsgDispatcher::tMsgCallbackFn cb2 = std::bind(&MyComponent::onRadioServerInvoke2,
                                                          this, _1, _2);
        CFdbMsgDispatcher::tMsgCallbackFn cb3 = std::bind(&MyComponent::onRadioServerInvoke3,
                                                          this, _1, _2);
        msg_tbl.add(MSG_RADIO_1 + mFirstMethodId, cb1);
        msg_tbl.add(MSG_RADIO_2 + mFirstMethodId, cb2);
        msg_tbl.add(MSG_RADIO_3 + mFirstMethodId, cb3);
        // lambda is also supported
        msg_tbl.add(MSG_RADIO_4 + mFirstMethodId, [this](CBaseJob::Ptr &msg_ref, CFdbAFServer *server)
            {
                auto msg = castToMessage<CFdbMessage *>(msg_ref);
                FDB_LOG_I("component %s, endpoint %s: invoke 4 is received with code %d\n",
                this->name().c_str(), server->name().c_str(), msg->code());
                msg->reply(msg_ref);
            });
        // offer the service to FDBus: if the service is already registered at name server,
        // just register the callbacks; otherwise register the service (own the name) at
        // name server followed by registering the callbacks
        mRadioServer = offerService(name, msg_tbl, cb);
    }

    CFdbAFClient *client()
    {
        return mRadioClient;
    }
    CFdbAFServer *server()
    {
        return mRadioServer;
    }
    void onRadioClientReply1(CBaseJob::Ptr &msg_ref, CFdbAFClient *client)
    {
        auto msg = castToMessage<CFdbMessage *>(msg_ref);
        FDB_LOG_I("component %s, endpoint %s: reply 1 is received with code %d\n",
                    name().c_str(), client->name().c_str(), msg->code());
    }
    void onRadioClientReply2(CBaseJob::Ptr &msg_ref, CFdbAFClient *client)
    {
        auto msg = castToMessage<CFdbMessage *>(msg_ref);
        FDB_LOG_I("component %s, endpoint %s: reply 2 is received with code %d\n",
                    name().c_str(), client->name().c_str(), msg->code());
    }
    void onRadioClientReply3(CBaseJob::Ptr &msg_ref, CFdbAFClient *client)
    {
        auto msg = castToMessage<CFdbMessage *>(msg_ref);
        FDB_LOG_I("component %s, endpoint %s: reply 3 is received with code %d\n",
                    name().c_str(), client->name().c_str(), msg->code());
    }
    void onRadioServerInvoke1(CBaseJob::Ptr &msg_ref, CFdbAFServer *server)
    {
        auto msg = castToMessage<CFdbMessage *>(msg_ref);
        FDB_LOG_I("component %s, endpoint %s: invoke 1 is received with code %d\n",
                    name().c_str(), server->name().c_str(), msg->code());
        msg->reply(msg_ref);
    }
    void onRadioServerInvoke2(CBaseJob::Ptr &msg_ref, CFdbAFServer *server)
    {
        auto msg = castToMessage<CFdbMessage *>(msg_ref);
        FDB_LOG_I("component %s, endpoint %s: invoke 2 is received with code %d\n",
                    name().c_str(), server->name().c_str(), msg->code());
        msg->reply(msg_ref);
    }
    void onRadioServerInvoke3(CBaseJob::Ptr &msg_ref, CFdbAFServer *server)
    {
        auto msg = castToMessage<CFdbMessage *>(msg_ref);
        FDB_LOG_I("component %s, endpoint %s: invoke 3 is received with code %d\n",
                    name().c_str(), server->name().c_str(), msg->code());
        msg->reply(msg_ref);
    }
    int32_t firstMethodId() const
    {
        return mFirstMethodId;
    }
private:
    CFdbAFClient *mRadioClient;
    CFdbAFServer *mRadioServer;
    CBaseWorker mWorker;
    MyTimer mTimer;
    int32_t mFirstMethodId;

    void onRadioClientOnline(CFdbAFClient *client, bool online)
    {
        FDB_LOG_I("component %s, endpoint %s: client online %d\n",
                    name().c_str(), client->name().c_str(), online);
    }
    void onRadioServerOnline(CFdbAFServer *server, FdbSessionId_t sid, bool online)
    {
        FDB_LOG_I("component %s, endpoint %s: server online %d %d\n",
                    name().c_str(), server->name().c_str(), sid, online);
    }
    void onRadioClientBroadcast1(CBaseJob::Ptr &msg_ref, CFdbAFClient *client)
    {
        auto msg = castToMessage<CFdbMessage *>(msg_ref);
        FDB_LOG_I("component %s, endpoint %s: broadcast 1 is received with code %d\n",
                    name().c_str(), client->name().c_str(), msg->code());
    }
    void onRadioClientBroadcast2(CBaseJob::Ptr &msg_ref, CFdbAFClient *client)
    {
        auto msg = castToMessage<CFdbMessage *>(msg_ref);
        FDB_LOG_I("component %s, endpoint %s: broadcast 2 is received with code %d\n",
                    name().c_str(), client->name().c_str(), msg->code());
    }
    void onRadioClientBroadcast3(CBaseJob::Ptr &msg_ref, CFdbAFClient *client)
    {
        auto msg = castToMessage<CFdbMessage *>(msg_ref);
        FDB_LOG_I("component %s, endpoint %s: broadcast 3 is received with code %d\n",
                    name().c_str(), client->name().c_str(), msg->code());
    }
};

MyTimer::MyTimer(MyComponent &comp)
    : CBaseLoopTimer(1500, true)
    , mComp(comp)
{
}

void MyTimer::run()
{
    if (mComp.client() && mComp.client()->connected())
    {
        // invoke method call asynchronously: the callback will be called once associated
        // reply is received from server
        CFdbAFClient::tInvokeCallbackFn cb1 = std::bind(&MyComponent::onRadioClientReply1,
                                                        &mComp, _1, _2);
        CFdbAFClient::tInvokeCallbackFn cb2 = std::bind(&MyComponent::onRadioClientReply2,
                                                        &mComp, _1, _2);
        CFdbAFClient::tInvokeCallbackFn cb3 = std::bind(&MyComponent::onRadioClientReply3,
                                                        &mComp, _1, _2);
        mComp.client()->invoke(MSG_RADIO_1 + mComp.firstMethodId(), cb1);
        mComp.client()->invoke(MSG_RADIO_2 + mComp.firstMethodId(), cb2);
        mComp.client()->invoke(MSG_RADIO_3 + mComp.firstMethodId(), cb3);
        // lambda is also supported.
        auto *c = &mComp;
        mComp.client()->invoke(MSG_RADIO_4 + mComp.firstMethodId(), [c](CBaseJob::Ptr &msg_ref, CFdbAFClient *client)
            {
                auto msg = castToMessage<CFdbMessage *>(msg_ref);
                FDB_LOG_I("component %s, endpoint %s: reply 4 is received with code %d\n",
                            c->name().c_str(), client->name().c_str(), msg->code());
            });
    }
    else if (mComp.server() && mComp.server()->connected())
    {
        {
            mComp.server()->broadcast(EVT_RADIO_1, 0, 0, "topic1");
            mComp.server()->broadcast(EVT_RADIO_2, 0, 0, "topic1");
            mComp.server()->broadcast(EVT_RADIO_3, 0, 0, "topic1");
        }
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
    const char *component = 0;
    const char *client = 0;
    const char *server = 0;
    const struct fdb_option core_options[] = {
        { FDB_OPTION_STRING, "component name", 'n', &component },
        { FDB_OPTION_STRING, "client name", 'c', &client },
        { FDB_OPTION_STRING, "server name", 's', &server },
        { FDB_OPTION_BOOLEAN, "help", 'h', &help }
    };

    fdb_parse_options(core_options, ARRAY_LENGTH(core_options), &argc, argv);
    if (help)
    {
        std::cout << "Usage: fdbappfwtest -n component_name[,component_name...]"
                  << "[ -c client_name[,client_name...][ -s server_name[,server_name...]"
                  << std::endl;
        std::cout << "    -n: specify component names separated by ','" << std::endl;
        std::cout << "    -c: specify clients separated by ','" << std::endl;
        std::cout << "    -s: specify servers separated by ','" << std::endl;
        return 0;
    }

    uint32_t nr_components = 0;
    char **components = component ? strsplit(component, ",", &nr_components) : 0;
    uint32_t nr_clients = 0;
    char **clients = client ? strsplit(client, ",", &nr_clients) : 0;
    uint32_t nr_servers = 0;
    char **servers = server ? strsplit(server, ",", &nr_servers) : 0;

    int32_t first_method_id = 0;
    for (uint32_t i = 0; i < nr_components; ++i)
    {
        new MyComponent(components[i], first_method_id, clients, nr_clients, servers, nr_servers);
        first_method_id += 128;
    }

    /* convert main thread into worker */
    CBaseWorker background_worker;
    background_worker.start(FDB_WORKER_EXE_IN_PLACE);
    return 0;
}

