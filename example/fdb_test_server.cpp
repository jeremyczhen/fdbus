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

#define FDB_LOG_TAG "FDB_TEST_SERVER"
#include <common_base/fdbus.h>
#include <idl-gen/common.base.Example.pb.h>

/* Define message ID; should be the same as server. */
enum EMessageId
{
    REQ_METADATA,
    REQ_RAWDATA,
    REQ_CREATE_MEDIAPLAYER,
    NTF_ELAPSE_TIME,
    NTF_MEDIAPLAYER_CREATED,
    NTF_MANUAL_UPDATE
};

class CMediaServer;

static CBaseWorker main_worker;

/* a timer broadcasting now-playing information periodically */
class CBroadcastTimer : public CMethodLoopTimer<CMediaServer> 
{
public:
    CBroadcastTimer(CMediaServer *server);
};

/* demo code for media server; inherit from CBaseServer */
class CMediaServer : public CBaseServer
{
public:
    CMediaServer(const char *name, CBaseWorker *worker = 0)
        : CBaseServer(name, worker)
    {
        mTimer = new CBroadcastTimer(this);
        /*
         * attach the timer to worker to run the timer callback at the thread
         * 'false' means don't start the timer
         */
        mTimer->attach(&main_worker, false);
    }

    /* callback called by the timer to broad elapse time.*/
    void broadcastElapseTime(CMethodLoopTimer<CMediaServer> *timer)
    {
        /* fill up protocol buffer */
        NFdbExample::ElapseTime et;
        et.set_hour(1);
        et.set_minute(10);
        et.set_second(35);
        /* broadcast elapse time; "my_filter" have no mean but test */
        broadcast(NTF_ELAPSE_TIME, et, "my_filter");

        /* another test: broadcast raw data */
        char raw_data[256];
        memset(raw_data, '=', sizeof(raw_data));
        raw_data[255] = '\0';
        broadcast(NTF_ELAPSE_TIME, "raw_buffer", raw_data, 256);
    }
protected:
    void onOnline(FdbSessionId_t sid, bool is_first)
    {
        FDB_LOG_I("server session up: %d\n", sid);
        if (is_first)
        {
            /* enable broadcast timer when the first client is connected */
            FDB_LOG_I("timer enabled\n");
            mTimer->enable();
        }
    }
    void onOffline(FdbSessionId_t sid, bool is_last)
    {
        FDB_LOG_I("server session shutdown: %d\n", sid);
        if (is_last)
        {
            /* disable broadcast timer when the last client is disconnected */
            mTimer->disable();
        }
    }
    /* called when client calls invoke() */
    void onInvoke(CBaseJob::Ptr &msg_ref)
    {
        CBaseMessage *msg = castToMessage<CBaseMessage *>(msg_ref);

        static int32_t elapse_time = 0;
        switch (msg->code())
        {
            case REQ_METADATA:
            {
                /*
                 * recover protocol buffer from incoming package
                 * it should match the type sent from client
                 */
                NFdbExample::SongId song_id;
                if (!msg->deserialize(song_id))
                {
                    msg->status(msg_ref, NFdbBase::FDB_ST_MSG_DECODE_FAIL, "Fail to decode request!");
                    return;
                }

                FDB_LOG_I("requested song id is: %d\n", song_id.id());
                /* fill in protocol buffer and reply to client */
                NFdbExample::NowPlayingDetails rep_msg;
                rep_msg.set_artist("Liu Dehua");
                rep_msg.set_album("Ten Year's Golden Song");
                rep_msg.set_genre("County");
                rep_msg.set_title("Wang Qing Shui");
                rep_msg.set_file_name("Lau Dewa");
                rep_msg.set_elapse_time(elapse_time++);
                msg->reply(msg_ref, rep_msg);
            }
            break;
            case REQ_RAWDATA:
            {
                /* raw data is received from client */
                // const char *buffer = (char *)msg->getPayloadBuffer();
                int32_t size = msg->getPayloadSize();
                FDB_LOG_I("Invoke of raw buffer is received: size: %d\n", size);
                msg->status(msg_ref, NFdbBase::FDB_ST_OK, "REQ_RAWDATA is processed successfully!");
            }
            break;
            default:
            break;
        }
    }

    /* called when client call subscribe() to register message */
    void onSubscribe(CBaseJob::Ptr &msg_ref)
    {
        CFdbMessage *msg = castToMessage<CFdbMessage *>(msg_ref);
        const CFdbMsgSubscribeItem *sub_item;
        /* iterate all message id subscribed */
        FDB_BEGIN_FOREACH_SIGNAL(msg, sub_item)
        {
            FdbMsgCode_t msg_code = sub_item->msg_code();
            const char *filter = "";
            if (sub_item->has_filter())
            {
                filter = sub_item->filter().c_str();
            }
            FdbSessionId_t sid = msg->session();

            FDB_LOG_I("message %d, filter %s of session %d is registered!\n", msg_code, filter, sid);

            /* reply initial value to the client subscribing the message id */
            switch (msg_code)
            {
                case NTF_ELAPSE_TIME:
                {
                    std::string str_filter(filter);
                    if (!str_filter.compare("my_filter"))
                    {
                        NFdbExample::ElapseTime et;
                        et.set_hour(1);
                        et.set_minute(10);
                        et.set_second(35);
                        msg->broadcast(msg_code, et, filter);
                    }
                    else if (!str_filter.compare("raw_buffer"))
                    {
                        std::string raw_data = "raw buffer test for broadcast.";
                        msg->broadcast(NTF_ELAPSE_TIME, "raw_buffer", raw_data.c_str(), raw_data.length() + 1);
                    }
                }
                case NTF_MANUAL_UPDATE:
                {
                    msg->broadcast(NTF_MANUAL_UPDATE);
                }
                break;
                default:
                break;
            }
        }
        FDB_END_FOREACH_SIGNAL()
    }

private:
    CBroadcastTimer *mTimer;
};

/* create a timer: interval is 1500ms; cyclically; when timeout, call CMediaServer::broadcastElapseTime */
CBroadcastTimer::CBroadcastTimer(CMediaServer *server)
    : CMethodLoopTimer<CMediaServer>(1500, true, server, &CMediaServer::broadcastElapseTime)
{}

int main(int argc, char **argv)
{
#ifdef __WIN32__
    WORD wVersionRequested;
    WSADATA wsaData;
    int err;

/* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
    wVersionRequested = MAKEWORD(2, 2);

    err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0) {
        /* Tell the user that we could not find a usable */
        /* Winsock DLL.                                  */
        printf("WSAStartup failed with error: %d\n", err);
        return 1;
    }
#endif
    /* start fdbus context thread */
    FDB_CONTEXT->start();
    CBaseWorker *worker_ptr = &main_worker;
    /* start worker thread */
    worker_ptr->start();

    /* create servers and bind the address: svc://service_name */
    for (int i = 1; i < argc; ++i)
    {
        std::string server_name = argv[i];
        std::string url(FDB_URL_SVC);
        url += server_name;
        server_name += "_server";
        CMediaServer *server = new CMediaServer(server_name.c_str(), worker_ptr);
        server->bind(url.c_str());
    }

    /* convert main thread into worker */
    CBaseWorker background_worker;
    background_worker.start(FDB_WORKER_EXE_IN_PLACE);
}
