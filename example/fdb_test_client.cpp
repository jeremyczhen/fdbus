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

#define FDB_INVOKE_SYNC 1

static CBaseWorker main_worker;

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

class CMediaClient;

/*
 * print statistic of message:
 * client                   server
 *  |---------c2s------------>|
 *  |                         |---\
 *  |                         |   s2r
 *  |                         |<--/
 *  |<--------r2c-------------|
 *  total = c2s + s2r + r2c 
 */
void printMetadata(FdbObjectId_t obj_id, const CFdbMsgMetadata &metadata)
{
    uint64_t time_c2s;
    uint64_t time_s2r;
    uint64_t time_r2c;
    uint64_t time_total;
    CFdbMessage::parseTimestamp(metadata, time_c2s, time_s2r, time_r2c, time_total);
#ifdef __WIN32__
    FDB_LOG_I("OBJ %d , client->server: %llu, arrive->reply: %llu, reply->receive: %llu, total: %llu\n",
                obj_id, time_c2s, time_s2r, time_r2c, time_total);
#else
    FDB_LOG_I("OBJ %d , client->server: %lu, arrive->reply: %lu, reply->receive: %lu, total: %lu\n",
                obj_id, time_c2s, time_s2r, time_r2c, time_total);
#endif
}

/* a timer sending request to server periodically */
class CInvokeTimer : public CMethodLoopTimer<CMediaClient>
{
public:
    CInvokeTimer(CMediaClient *client);
};

class CMediaClient : public CBaseClient
{
public:
    CMediaClient(const char *name, CBaseWorker *worker = 0)
        : CBaseClient(name, worker)
    {
        /* create the request timer, attach to a worker thread, but do not start it */
        mTimer = new CInvokeTimer(this);
        mTimer->attach(&main_worker, false);
    }

    /* called periodically in timer to get song information */
    void callServer(CMethodLoopTimer<CMediaClient> *timer)
    {
        NFdbExample::SongId song_id;
        song_id.set_id(1234);
        CFdbProtoMsgBuilder builder(song_id);

#if defined(FDB_INVOKE_SYNC)
        /* this version of invoke() is synchronous: once called, it blocks until server call reply() */
        CBaseJob::Ptr ref(new CBaseMessage(REQ_METADATA));
        invoke(ref, builder); /* onInvoke() will be called at server */

        /* we return from invoke(); must get reply from server. Check it. */
        auto msg = castToMessage<CBaseMessage *>(ref);
        /* print performance statistics */
        CFdbMsgMetadata md;
        msg->metadata(md);
        printMetadata(objId(), md);

        if (msg->isStatus())
        {
            /* Unable to get intended reply from server... Check what happen. */
            int32_t id;
            std::string reason;
            if (!msg->decodeStatus(id, reason))
            {
                FDB_LOG_E("onReply: fail to decode status!\n");
                return;
            }
            FDB_LOG_I("sync reply: status is received: msg code: %d, id: %d, reason: %s\n", msg->code(), id, reason.c_str());
            return;
        }

        /*
         * recover protocol buffer from incoming package
         * it should match the type replied from server 
         */
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
            FDB_LOG_I("sync reply - artist: %s, album: %s, genre: %s, title: %s, file name: %s, folder name: %s, elapse time: %d\n",
                        artist, album, genre, title, file_name, folder_name, elapse_time);
        }
        else
        {
            FDB_LOG_I("Error! Unable to decode message!!!\n");
        }
#else
        /*
         * This is asynchronous verison of invoke(); It returns immediately without blocking
         * Reply from server will be received from onReply() callback
         */
        invoke(REQ_METADATA, builder);
#endif

        invoke(REQ_RAWDATA);

        /*
         * trigger update manually; onBroadcast() will be called followed by
         * onStatus().
         */
        CFdbMsgTriggerList update_list;
        addTriggerItem(update_list, NTF_MANUAL_UPDATE);
        update(update_list);
    }

protected:
    /* called when connected to the server */
    void onOnline(FdbSessionId_t sid, bool is_first)
    {
        FDB_LOG_I("client session online: %d\n", sid);
        if (is_first)
        {
            mTimer->enable(); /* enable timer to invoke the server periodically */

            /* add all events to subscribe list */
            CFdbMsgSubscribeList subscribe_list;
            addNotifyItem(subscribe_list, NTF_ELAPSE_TIME, "my_filter");
            addNotifyItem(subscribe_list, NTF_ELAPSE_TIME, "raw_buffer");
            addNotifyItem(subscribe_list, NTF_CJSON_TEST);
            addNotifyGroup(subscribe_list, MEDIA_GROUP_1);
            /*
             * register NTF_MANUAL_UPDATE for manual update: it will not
             * update unless update() is called
             */
            addUpdateItem(subscribe_list, NTF_MANUAL_UPDATE);
            /* subscribe them, leading to onSubscribe() to be called at server */
            subscribe(subscribe_list);
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
        auto msg = castToMessage<CBaseMessage *>(msg_ref);
        FDB_LOG_I("Broadcast is received: %d; filter: %s\n", msg->code(), msg->topic().c_str());

        switch (msg->code())
        {
            case NTF_ELAPSE_TIME:
            {
                std::string filter(msg->topic());
                if (!filter.compare("my_filter"))
                {
                    /*
                     * recover protocol buffer from incoming package
                     * it should match the type broadcasted from server 
                     */
                    NFdbExample::ElapseTime et;
                    CFdbProtoMsgParser parser(et);
                    if (msg->deserialize(parser))
                    {
                        FDB_LOG_I("elapse time is received: hour: %d, minute: %d, second: %d\n",
                                    et.hour(), et.minute(), et.second());
                    }
                    else
                    {
                        FDB_LOG_E("Unable to decode NFdbExample::ElapseTime!\n");
                    }
                }
                else if (!filter.compare("raw_buffer"))
                {
                    // const char *buffer = (char *)msg->getPayloadBuffer();
                    int32_t size = msg->getPayloadSize();
                    FDB_LOG_I("Broadcast of raw buffer is received: size: %d\n", size);
                }
            }
            break;
            case NTF_MANUAL_UPDATE:
            {
                FDB_LOG_I("Manual update is received!\n");
            }
            break;
            case NTF_CJSON_TEST:
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
                        FDB_LOG_I("Broadcast of JSON is received: birthday: %d, id: %d, name: %s\n", birthday, id, name);
                    }
                }
                else
                {
                    FDB_LOG_E("Broadcast of JSON is received: with error!\n");
                }
            }
            break;
            case NTF_GROUP_TEST1:
            case NTF_GROUP_TEST2:
            case NTF_GROUP_TEST3:
                FDB_LOG_I("Broadcast of group message %d is received. Init: %d\n", fdbEventCode(msg->code()), msg->isInitialResponse());
            break;
            default:
            break;
        }
    }

    /* called when client call asynchronous version of invoke() and reply() is called at server */
    void onReply(CBaseJob::Ptr &msg_ref)
    {
        auto msg = castToMessage<CBaseMessage *>(msg_ref);
        FDB_LOG_I("response is receieved. sn: %d\n", msg->sn());
        /* print performance statistics */
        CFdbMsgMetadata md;
        msg->metadata(md);
        printMetadata(objId(), md);

        switch (msg->code())
        {
            case REQ_METADATA:
            {
                if (msg->isStatus())
                {
                    /* Unable to get intended reply from server... Check what happen. */
                    if (msg->isError())
                    {
                        int32_t error_code;
                        std::string reason;
                        if (!msg->decodeStatus(error_code, reason))
                        {
                            FDB_LOG_E("onReply: fail to decode status!\n");
                            return;
                        }
                        FDB_LOG_I("onReply(): status is received: msg code: %d, error_code: %d, reason: %s\n",
                              msg->code(), error_code, reason.c_str());
                    }
                    return;
                }
                /*
                 * recover protocol buffer from incoming package
                 * it should match the type replied from server 
                 */
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
                    FDB_LOG_I("async reply - artist: %s, album: %s, genre: %s, title: %s, file name: %s, folder name: %s, elapse time: %d\n",
                                artist, album, genre, title, file_name, folder_name, elapse_time);
                }
                else
                {
                    FDB_LOG_I("Error! Unable to decode message!!!\n");
                }
            }
            break;
            case REQ_RAWDATA:
            {
                if (msg->isStatus())
                {
                    int32_t error_code;
                    std::string reason;
                    if (!msg->decodeStatus(error_code, reason))
                    {
                        FDB_LOG_E("onReply: fail to decode status!\n");
                        return;
                    }
                    FDB_LOG_I("onReply(): status is received: msg code: %d, error_code: %d, reason: %s\n",
                          msg->code(), error_code, reason.c_str());
                    return;
                }
                
                CFdbParcelableArray<CPerson> persions;
                CFdbParcelableParser parser(persions);
                if (!msg->deserialize(parser))
                {
                    FDB_LOG_E("onReply: fail to decode from simple parser!\n");
                    return;
                }
                std::ostringstream stream;
                (void)persions.format(stream);
                FDB_LOG_I("onReply: %s\n", stream.str().c_str());
#if 0
                for (auto pit = persions.pool().begin(); pit != persions.pool().end(); ++pit)
                {
                    FDB_LOG_I("name: %s; age: %d; salary: %d; address: %s.\n",
                                pit->mName.c_str(), pit->mAge, pit->mSalary, pit->mAddress.c_str());
                    for (auto cit = pit->mCars.pool().begin(); cit != pit->mCars.pool().end(); ++cit)
                    {
                        FDB_LOG_I("    brand: %s, model: %s, price: %d.\n",
                                cit->mBrand.c_str(), cit->mModel.c_str(), cit->mPrice);
                    }
                }
#endif
            }
            break;
            default:
            break;
        }
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
    CInvokeTimer *mTimer;
};

/* create a timer: interval is 1000ms; cyclically; when timeout, call CMediaClient::callServer() */
CInvokeTimer::CInvokeTimer(CMediaClient *client)
    : CMethodLoopTimer<CMediaClient>(1000, true, client, &CMediaClient::callServer)
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

    /* create clients and connect to address: svc://service_name */
    for (int i = 1; i < argc; ++i)
    {
        std::string server_name = argv[i];
        std::string url(FDB_URL_SVC);
        url += server_name;
        server_name += "_client";
        auto client = new CMediaClient(server_name.c_str(), worker_ptr);
        
        client->enableReconnect(true);
        client->connect(url.c_str());
    }

    /* convert main thread into worker */
    CBaseWorker background_worker;
    background_worker.start(FDB_WORKER_EXE_IN_PLACE);
}
