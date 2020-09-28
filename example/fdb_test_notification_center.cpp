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

#define FDB_LOG_TAG "FDB_TEST_CLIENT"
#include <common_base/fdbus.h>
#include FDB_IDL_EXAMPLE_H
#include <common_base/CFdbProtoMsgBuilder.h>
#include "CFdbIfPerson.h"
#include <common_base/cJSON/cJSON.h>
#include <common_base/CFdbCJsonMsgBuilder.h>
#include <iostream>

#define FDB_INVOKE_SYNC 0

static CBaseWorker main_worker;
static bool is_publisher = true;
static int32_t event_id_start = 0;

/* Define message ID; should be the same as server. */
enum EGroupId
{
    MEDIA_GROUP_MASTER,
    MEDIA_GROUP_1
};

enum EMessageId
{
    REQ_METADATA,
    REQ_RAWDATA,
    REQ_CREATE_MEDIAPLAYER,
    NTF_ELAPSE_TIME,
    NTF_MEDIAPLAYER_CREATED,
    NTF_MANUAL_UPDATE,

    NTF_CJSON_TEST = 128,
    NTF_GROUP_TEST1 = fdbMakeEventCode(MEDIA_GROUP_1, 0),
    NTF_GROUP_TEST2 = fdbMakeEventCode(MEDIA_GROUP_1, 1),
    NTF_GROUP_TEST3 = fdbMakeEventCode(MEDIA_GROUP_1, 2)
};

class CNotificationCenterClient;
/* a timer sending request to server periodically */
class CInvokeTimer : public CMethodLoopTimer<CNotificationCenterClient>
{
public:
    CInvokeTimer(CNotificationCenterClient *client);
};

class CNotificationCenterClient : public CBaseClient
{
public:
    CNotificationCenterClient(const char *name, CBaseWorker *worker = 0)
        : CBaseClient(name, worker)
    {
        /* create the request timer, attach to a worker thread, but do not start it */
        mTimer = new CInvokeTimer(this);
        mTimer->attach(&main_worker, false);
    }

    /* called periodically in timer to get song information */
    void callServer(CMethodLoopTimer<CNotificationCenterClient> *timer)
    {
        if (is_publisher)
        {
            {
            NFdbExample::SongId data;
            data.set_id(1234);
            CFdbProtoMsgBuilder builder(data);
            publish(REQ_RAWDATA + event_id_start, builder, "topic 1", true);
            }
            
            {
            static int32_t elapse_time = 0;
            NFdbExample::NowPlayingDetails data;
            data.set_artist("Liu Dehua");
            data.set_album("Ten Year's Golden Song");
            data.set_genre("County");
            data.set_title("Wang Qing Shui");
            data.set_file_name("Lau Dewa");
            data.set_elapse_time(elapse_time++);
            CFdbProtoMsgBuilder builder(data);
            publish(REQ_METADATA + event_id_start, builder, "topic 2");
            }
            
            {
            cJSON *data = cJSON_CreateObject();
            cJSON_AddNumberToObject(data, "birthday", 19900101);
            cJSON_AddNumberToObject(data, "id", 1);
            cJSON_AddStringToObject(data, "name", "Sofia");
            CFdbCJsonMsgBuilder builder(data);
            publish(NTF_CJSON_TEST + event_id_start, builder, "topic 3");
            cJSON_Delete(data);
            }
            publish(REQ_CREATE_MEDIAPLAYER + event_id_start, 0, 0, 0, true);
        }
        else
        {
            get(REQ_RAWDATA + event_id_start, "topic 1");
            get(REQ_METADATA + event_id_start, "topic 2");
            get(NTF_CJSON_TEST + event_id_start, "topic 3");
        }
    }

    void testInitBroadcast()
    {
        enableEventCache(true);
        broadcast(1234, 0, 0, "broadcast_topic");
        CBaseWorker background_worker;
        background_worker.start(FDB_WORKER_EXE_IN_PLACE);
    }

protected:
    /* called when connected to the server */
    void onOnline(FdbSessionId_t sid, bool is_first)
    {
        FDB_LOG_I("client session online: %d\n", sid);
        if (is_first)
        {
            mTimer->enable(); /* enable timer to invoke the server periodically */
            if (!is_publisher)
            {
                /* add all events to subscribe list */
                CFdbMsgSubscribeList subscribe_list;
                addNotifyItem(subscribe_list, REQ_RAWDATA + event_id_start, "topic 1");
                addNotifyItem(subscribe_list, REQ_METADATA + event_id_start, "topic 2");
                addNotifyItem(subscribe_list, NTF_CJSON_TEST + event_id_start, "topic 3");
                /* subscribe them, leading to onSubscribe() to be called at server */
                subscribeSync(subscribe_list);
            }
        }
    }

    /* called when disconnected from server */
    void onOffline(FdbSessionId_t sid, bool is_last)
    {
        FDB_LOG_I("client session shutdown: %d\n", sid);
        if (is_last)
        {
            mTimer->disable(); /* disable the timer */
        }
    }

    /* called when events broadcasted from server is received */
    void onBroadcast(CBaseJob::Ptr &msg_ref)
    {
        processEvent(msg_ref, "Notification");
    }

    void onGetEvent(CBaseJob::Ptr &msg_ref)
    {
        auto msg = castToMessage<CBaseMessage *>(msg_ref);
        /*
         * We get here to process reply to event retrieving, i.e., reply to get()
         */
        if (msg->isStatus())
        {
            int32_t id;
            std::string reason;
            if (!msg->decodeStatus(id, reason))
            {
                FDB_LOG_E("GetEvent: fail to decode status!\n");
                return;
            }
            /* Check if something is wrong... */
            FDB_LOG_E("GetEvent error: msg code: %d, id: %d, reason: %s\n", msg->code(), id, reason.c_str());
            return;
        }
        processEvent(msg_ref, "GetEvent");
    }

    void onReply(CBaseJob::Ptr &msg_ref)
    {
        auto msg = castToMessage<CBaseMessage *>(msg_ref);
        FDB_LOG_I("Reply to invoke is received: %d, %s!\n", msg->code(), msg->topic().c_str());
    }

    /* check if something happen... */
    void onStatus(CBaseJob::Ptr &msg_ref
                        , int32_t error_code
                        , const char *description)
    {
        auto msg = castToMessage<CBaseMessage *>(msg_ref);
        if (msg->isSubscribe())
        {
            if (msg->isError())
            {
            }
            else
            {
                FDB_LOG_I("subscribe is ok! sn: %d is received.\n", msg->sn());
            }
        }
        FDB_LOG_I("Reason: %s\n", description);
    }

private:
    void processEvent(CBaseJob::Ptr &msg_ref, const char *type)
    {
        auto msg = castToMessage<CBaseMessage *>(msg_ref);
        if (msg->code() == (REQ_RAWDATA + event_id_start))
        {
            NFdbExample::SongId song_id;
            CFdbProtoMsgParser parser(song_id);
            if (msg->deserialize(parser))
            {
                FDB_LOG_I("%s is received: event: %d, topic: %s, id: %d\n", type, msg->code(), msg->topic().c_str(), song_id.id());
            }
            else
            {
                FDB_LOG_E("%s parse error! code: %d, topic: %s\n", type, msg->code(), msg->topic().c_str());
            }
        }
        else if (msg->code() == (REQ_METADATA + event_id_start))
        {
            NFdbExample::NowPlayingDetails now_playing;
            CFdbProtoMsgParser parser(now_playing);
            if (msg->deserialize(parser))
            {
                auto artist = now_playing.artist().c_str();
                auto album = now_playing.album().c_str();
                auto genre = now_playing.genre().c_str();
                auto title = now_playing.title().c_str();
                auto file_name = "";
                if (now_playing.has_file_name())
                {
                    file_name = now_playing.file_name().c_str();
                }
                auto folder_name = "";
                if (now_playing.has_folder_name())
                {
                    folder_name = now_playing.folder_name().c_str();
                }
                int32_t elapse_time = now_playing.elapse_time();
                FDB_LOG_I("%s is received: event:"
                          " %d, topic: %s, artist: %s, album: %s, genre: %s, title: %s,"
                          " file name: %s, folder name: %s, elapse time: %d\n",
                          type, msg->code(), msg->topic().c_str(), artist, album, genre, title, file_name, folder_name, elapse_time);
            }
            else
            {
                FDB_LOG_I("%s parse error! code: %d, topic: %s\n", type, msg->code(), msg->topic().c_str());
            }
        }
        else if (msg->code() == (NTF_CJSON_TEST + event_id_start))
        {
            CFdbCJsonMsgParser parser;
            if (msg->deserialize(parser))
            {
                cJSON *f = parser.retrieve();
                int birthday = 0;
                int id = 0;
                const char *name = 0;
                if (cJSON_IsObject(f))
                {
                    {
                    cJSON *item = cJSON_GetObjectItem(f, "birthday");
                    if (item && cJSON_IsNumber(item))
                    {
                        birthday = item->valueint;
                    }
                    }{
                    cJSON *item = cJSON_GetObjectItem(f, "id");
                    if (item && cJSON_IsNumber(item))
                    {
                        id = item->valueint;
                    }
                    }{
                    cJSON *item = cJSON_GetObjectItem(f, "name");
                    if (item && cJSON_IsString(item))
                    {
                        name = item->valuestring;
                    }
                    }
                    FDB_LOG_I("%s is received: code: %d, topic: %s, birthday: %d, id: %d, name: %s\n",
                              type, msg->code(), msg->topic().c_str(),  birthday, id, name);
                }
            }
            else
            {
                FDB_LOG_E("%s parse error: JSON is received with error!\n", type);
            }
        }
    }
    CInvokeTimer *mTimer;
};

/* create a timer: interval is 1000ms; cyclically; when timeout, call CNotificationCenterClient::callServer() */
CInvokeTimer::CInvokeTimer(CNotificationCenterClient *client)
    : CMethodLoopTimer<CNotificationCenterClient>(1000, true, client, &CNotificationCenterClient::callServer)
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
    if(argc < 3)
    {
        std::cout << "Usage: fdbntfcentertest 0/1 server-name[ start_event]" << std::endl;
        std::cout << "       0: subscriber; 1: publisher" << std::endl;
        return -1;
    }

    is_publisher = !!atoi(argv[1]);
    if (argc >= 4)
    {
        event_id_start = atoi(argv[3]);
    }
    /* start fdbus context thread */
    FDB_CONTEXT->start();
    CBaseWorker *worker_ptr = &main_worker;
    /* start worker thread */
    worker_ptr->start();

    std::string server_name = argv[2];
    std::string url(FDB_URL_SVC);
    url += server_name;
    server_name += "_client";
    auto client = new CNotificationCenterClient(server_name.c_str(), worker_ptr);
    client->enableReconnect(true);
    client->connect(url.c_str());

    /* convert main thread into worker */
    CBaseWorker background_worker;
    background_worker.start(FDB_WORKER_EXE_IN_PLACE);
}
