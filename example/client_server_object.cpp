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

#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <vector>
#define FDB_LOG_TAG "FDBTEST"
#include <common_base/fdbus.h>
#define OBJ_FROM_SERVER_TO_CLIENT 1

/*
 * 该文件由描述文件workspace/pb_idl/common.base.Example.proto自动生成。
 * 项目所有的接口描述文件都统一放在该目录下，在使用时包含进来。
 */
#include <idl-gen/common.base.Example.pb.h>

#if 1
CBaseNotificationCenter<void *> nc;
CBaseNotification<void *> nf;

class ntf_test
{
public:
    void process(CMethodNotification<void *, ntf_test> *ntf, void * &data)
    {
    }
};

ntf_test aaa;
CMethodNotification<void *, ntf_test> m_ntf(&aaa, &ntf_test::process);
CGenericMethodNotification<ntf_test> m_ntf1(&aaa, &ntf_test::process);
#endif

#if __WIN32__
// Need to link with Ws2_32.lib
#pragma comment(lib, "ws2_32.lib")
#endif

/*
 * 基本概念：
 * server vs. socket：
 * 一个Server包含多个socket，即一个server可以bind到(侦听)多个地址；
 * socket vs. session：
 * 每个socket包含多个session，因为server的accept()会返回多个文件描述符(fd)。
 * session：
 * 一个session代表一个client-server的连接; 消息都是在session中传递。
 *
 * client vs. socket：
 * 一个client包含多个socket，即client可以connect到多个地址（server必须已经
 * 在侦听这些地址）；
 * 实际应用中，每个client只会连接一个地址。
 * socket vs. session：
 * 每个socket包含一个session，因为client只有一个fd。
 *
 * client和server统称为endpoint，用FdbEndpointId_t表示；
 * session用FdbSessionId_t表示；
 * socket用FdbSocketId_t表示。
 */

/*
 * 定义worker线程，运行定时器，用于client定时向server发消息
 * 或server定时向client广播消息
 */
static CBaseWorker main_worker;
static CBaseWorker mediaplayer_worker("mediaplayer_worker");

/*
 * 定义自己的消息ID。
 * 同一个server的所有的请求消息和广播消息都用整型id唯一标识。
 * 最好定义在server提供的proto文件里，方便client引用。
 */
enum EMessageId
{
    REQ_METADATA,
    REQ_RAWDATA,
    REQ_CREATE_MEDIAPLAYER,
    NTF_ELAPSE_TIME,
    NTF_MEDIAPLAYER_CREATED,
    NTF_MANUAL_UPDATE
};

/*
 * 定义自己的消息体，可以存放一次交互的上下文（context）。
 * 如果没有该需求，代码中也可以直接使用CBaseMessage或CFdbMessage。
 */
class CMyMessage : public CBaseMessage 
{
public:
    CMyMessage(FdbMsgCode_t code = FDB_INVALID_ID)
        : CBaseMessage(code)
    {
        memset(my_buf, 0, sizeof(my_buf));
    }
private:
    uint8_t my_buf[1024 * 1024];
};

void printMetadata(FdbObjectId_t obj_id, const CFdbMsgMetadata &metadata)
{
    uint64_t time_c2s;
    uint64_t time_s2r;
    uint64_t time_r2c;
    uint64_t time_total;
    CFdbMessage::parseTimestamp(metadata, time_c2s, time_s2r, time_r2c, time_total);
    FDB_LOG_I("OBJ %d, client->server: %" PRIu64 ", arrive->reply: %" PRIu64 ", reply->receive: %" PRIu64 ", total: %" PRIu64 "\n",
                obj_id, time_c2s, time_s2r, time_r2c, time_total);
}

/*
 * 定义一个定时器，用于周期性向client广播数据。
 */
template <typename T>
class CMyServer;
static std::vector<CMyServer<CFdbBaseObject> *> my_server_objects;
template <typename T>
class CBroadcastTimer : public CMethodLoopTimer<CMyServer<T> > 
{
public:
    /*
     * 100代表定时间隔是100毫秒；true表示是周期性timer；否则便
     * 是一次触发的timer。
     */
    CBroadcastTimer(CMyServer<T> *server)
        : CMethodLoopTimer<CMyServer<T> >(1500, true, server, &CMyServer<T>::broadcastElapseTime)
    {}
};

/*
 * 定义一个server的实现。所有的交互都是在回调函数中完成的。具体有
 * 哪些回调函数需要实现可以参考CBaseEndpoint类的onXXX()函数。
 */
template <class T>
class CMyServer : public T
{
public:
    CMyServer(const char *name, CBaseWorker *worker = 0)
        : T(name, worker)
    {
        /*
         * 创建定时器广播消息；attach()将timer与main_worker绑定：定时器
         * 的回调函数将运行在该worker对应的线程上。false表示暂时不启动
         * 定时器。
         */
        mTimer = new CBroadcastTimer<T>(this);
        mTimer->attach(&main_worker, false);
    }
    /*
     * Timer的回调函数，用于周期性向client广播数据
     */
    void broadcastElapseTime(CMethodLoopTimer<CMyServer<T> > *timer)
    {
        /*
         * 这里假设server向client广播播放时间。首先准备好protocol buffer
         * 格式的数据NFdbExample::ElapseTime
         */
        NFdbExample::ElapseTime et;
        et.set_hour(1);
        et.set_minute(10);
        et.set_second(35);
        /*
         * 然后将数据广播给所有注册了NTF_ELAPSE_TIME的clients。
         * 为了更精细地控制广播的颗粒度，还增加了一可选的个字符串。在现在的
         * 例子中，只有用户注册NTF_ELAPSE_TIME时附带了my_filter才能收到该广播。
         */
        this->broadcast(NTF_ELAPSE_TIME, et, "my_filter");
        char raw_data[1920];
        memset(raw_data, '=', sizeof(raw_data));
        raw_data[1919] = '\0';
        this->broadcast(NTF_ELAPSE_TIME, "raw_buffer", raw_data, 1920);
        this->broadcast(NTF_ELAPSE_TIME);
    }
protected:
    /*
     * 当有client连接进来时被调用；sid代表和client的一个连接。
     * is_first表示是否第一个client接入。
     */
    void onOnline(FdbSessionId_t sid, bool is_first)
    {
        FDB_LOG_I("OBJ %d server session up: %d\n", this->objId(), sid);
        if (is_first)
        {
            FDB_LOG_I("timer enabled\n");
            // 第一个client来连接时，启动定时器
            if (!this->isPrimary())
            {
                mTimer->enable();
            }
        }
    }
    /*
     * 当有client断开连接是被调用；sid代表和client的一个连接。
     * is_last表示是否最后一个client断开连接。
     */
    void onOffline(FdbSessionId_t sid, bool is_last)
    {
        FDB_LOG_I("OBJ %d server session shutdown: %d\n", this->objId(), sid);
        if (is_last)
        {
            // 最后一个client断开连接时关闭定时器
            mTimer->disable();
        }
    }
    /*
     * 当client调用invoke()或send()时被调用。msg_ref包含收到的消息和数据。
     */
    void onInvoke(CBaseJob::Ptr &msg_ref)
    {
        /*
         * 首先转换成CBaseMessage。onInvoke()里只能转换成该类型。
         */
        CBaseMessage *msg = castToMessage<CBaseMessage *>(msg_ref);
        /*
         * 接下来就要case by case地处理收到的请求了。msg->code()表示消息对应的码。
         *
         * 为了让代码结构更加美观及高效，可以使用类CFdbMessageHandle
         * (在CBaseEndpoint.h)中。具体实现可以参考name_server/CNameServer.cpp。
         */
        static int32_t elapse_time = 0;
        switch (msg->code())
        {
            case REQ_METADATA:
            {
                /*
                 * 首先解析对方传过来的数据，格式是已知的protocol buffer
                 * 结构NFdbExample::SongId。msg->deserialize()解析收到的串行数据
                 * 并转换成指定的格式。
                 */
                NFdbExample::SongId song_id;
                if (!msg->deserialize(song_id))
                {
                    // 格式解析失败，返回错误状态给对方。
                    msg->status(msg_ref, NFdbBase::FDB_ST_MSG_DECODE_FAIL, "Fail to decode request!");
                    return;
                }

                FDB_LOG_I("OBJ %d requested song id is: %d\n", this->objId(), song_id.id());
                /*
                 * 接下来构筑返回数据，格式同样为protocol buffer结构。
                 */
                NFdbExample::NowPlayingDetails rep_msg;
#if 0
                rep_msg.set_artist("刘德华");
                rep_msg.set_album("十年华语金曲");
                rep_msg.set_genre("乡村");
                rep_msg.set_title("忘情水");
#else
                rep_msg.set_artist("Liu Dehua");
                rep_msg.set_album("Ten Year's Golden Song");
                rep_msg.set_genre("County");
                rep_msg.set_title("Wang Qing Shui");
#endif
                rep_msg.set_file_name("Lau Dewa");
                rep_msg.set_elapse_time(elapse_time++);
                // 将结果返回给对方；处理结束。
                msg->reply(msg_ref, rep_msg);
            }
            break;
            case REQ_RAWDATA:
            {
                // const char *buffer = (char *)msg->getPayloadBuffer();
                int32_t size = msg->getPayloadSize();
                FDB_LOG_I("OBJ %d Invoke of raw buffer is received: size: %d\n", this->objId(), size);
                msg->status(msg_ref, NFdbBase::FDB_ST_OK, "REQ_RAWDATA is processed successfully!");
            }
            break;
            case REQ_CREATE_MEDIAPLAYER:
            {
                #if 0
                for (int32_t i = 0; i < 5; ++i)
                {
                    CMyServer<CFdbBaseObject> *obj = new CMyServer<CFdbBaseObject>("mediaplayer", &mediaplayer_worker);
                    FdbObjectId_t obj_id = obj->bind(dynamic_cast<CBaseEndpoint *>(this));
                    NFdbExample::FdbMsgObjectInfo obj_info;
                    obj_info.set_obj_id(obj_id);
                    this->broadcast(NTF_MEDIAPLAYER_CREATED, obj_info);
                    my_server_objects.push_back(obj);
                }
                #endif
            }
            break;
            default:
            break;
        }
    }
    /*
     * client注册每一条消息通知时，server的onSubscribe()都会被调用到。一个强制
     * 要求是，在该函数中，server都要通过broadcast()将所注册消息的当前值返回给
     * client，从而client在注册消息的同时就可以自动收到消息的当前值。
     */
    void onSubscribe(CBaseJob::Ptr &msg_ref)
    {
        CFdbMessage *msg = castToMessage<CFdbMessage *>(msg_ref);
        const CFdbMsgSubscribeItem *sub_item;
        FDB_BEGIN_FOREACH_SIGNAL(msg, sub_item)
        {
            FdbMsgCode_t msg_code = sub_item->msg_code();
            const char *filter = "";
            if (sub_item->has_filter())
            {
                filter = sub_item->filter().c_str();
            }
            FdbSessionId_t sid = msg->session();

            FDB_LOG_I("OBJ %d message %d, filter %s of session %d is registered!\n", this->objId(), msg_code, filter, sid);

            /*
             * 将当前状态主动报告给前来注册的client。
             *
             * 接下来需要case-by-case地处理；同样为了美观及高效，可以使用类
             * CFdbSubscribeHandle(在CBaseEndpoint.h中)。具体例子可以参考
             * name_server/CNameServer.cpp。
             */
            switch (msg_code)
            {
                case NTF_ELAPSE_TIME:
                {
                    // Warning! filter might be NULL!
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
                break;
                case NTF_MANUAL_UPDATE:
                {
                    msg->broadcast(NTF_MANUAL_UPDATE);
                }
                default:
                break;
            }

        }
        FDB_END_FOREACH_SIGNAL()
    }

    void onCreateObject(CBaseEndpoint *endpoint, CFdbMessage *msg)
    {
        FDB_LOG_I("Create object %d because message %d is received.\n", msg->objectId(), msg->code());
        CMyServer<CFdbBaseObject> *obj = new CMyServer<CFdbBaseObject>("mediaplayer", &mediaplayer_worker);
        obj->bind(dynamic_cast<CBaseEndpoint *>(this), msg->objectId());
    }
private:
    CBroadcastTimer<T> *mTimer;
};

template <typename T>
class CMyClient;
static std::vector<CMyClient<CFdbBaseObject> *> my_client_objects;
/*
 * 定义一个定时器，用于周期性向server发送消息或请求。
 */
template <typename T>
class CInvokeTimer : public CMethodLoopTimer<CMyClient<T> >
{
public:
    /*
     * 100代表定时间隔是100毫秒；true表示是周期性timer；否则便
     * 是一次触发的timer。
     */
    CInvokeTimer(CMyClient<T> *client)
        : CMethodLoopTimer<CMyClient<T> >(1000, true, client, &CMyClient<T>::callServer)
    {}
};

/*
 * 定义一个client的实现。所有的交互都是在回调函数中完成的。具体有
 * 哪些回调函数需要实现可以参考CBaseEndpoint类的onXXX()函数。
 */
template <typename T>
class CMyClient : public T
{
public:
    CMyClient(const char *name, CBaseWorker *worker = 0)
        : T(name, worker)
    {
        /*
         * 创建定时器周期性发起调用；attach()将timer与main_worker绑定：定时器
         * 的回调函数将运行在该worker对应的线程上。false表示暂时不启动
         * 定时器。
         */
        mTimer = new CInvokeTimer<T>(this);
        mTimer->attach(&main_worker, false);
    }
    /*
     * Timer的回调函数，用于周期性调用server的方法
     */
    void callServer(CMethodLoopTimer<CMyClient<T> > *timer)
    {
        /*
         * 创建并准备好要发送给server的protocol buffer格式的数据
         */
        NFdbExample::SongId song_id;
        song_id.set_id(1234);

        /*
         * 接下来将数据发送给server。同步或异步调用使用invoke()函数
         * 如果是同步调用，需要创建消息体的引用ref，并将该引用传给
         * invoke()。该引用可以防止msg被系统自动释放掉，从而在
         * invoke()结束后可以使用msg获得返回值。
         *
         * 如果是异步调用，则无需创建ref，直接调用invoke的
         * 不带引用的版本。异步调用的返回将会触发onReply()调用，
         * 在该函数中可以处理返回值。
         *
         * 如果不需要返回值，可以调用send()。
         *
         * client_session_id是client和server之间的一个链接，也就是请求
         * 发送的目的地址。
         */
        CBaseJob::Ptr ref(new CMyMessage(REQ_METADATA));
        this->invoke(ref, song_id);
        CMyMessage *msg = castToMessage<CMyMessage *>(ref);
        CFdbMsgMetadata md;
        msg->metadata(md);
        printMetadata(this->objId(), md);
        /*
         * 由于是同步调用，msg->invoke()会一直阻塞直到server调用reply()。
         * 接下来就可以处理server reply回来的数据了。
         *
         * 首先要看一下是正常的数据返回还是由于异常或其它原因造成的状态
         * 返回。server可能通过reply()返回正常数据，还可以通过status()返回
         * 失败。如果不是正常数据返回，需要在此处理。
         */
        if (msg->isStatus())
        {
            /*
             * 状态返回统一的格式：错误码+原因。这和linux命令返回是一致的
             */
            int32_t id;
            std::string reason;
            if (!msg->decodeStatus(id, reason))
            {
                FDB_LOG_E("onReply: fail to decode status!\n");
                return;
            }
            if (msg->isError())
            {
                FDB_LOG_I("sync reply: status is received: msg code: %d, id: %d, reason: %s\n", msg->code(), id, reason.c_str());
            }
            return;
        }
        /*
         * OK，现在可以处理调用的返回值了。首先你是知道返回的数据类型是
         * NFdbExample::NowPlayingDetails。这是个protocol buffer格式。
         * msg->deserialize()将收到的串行数据解析成protocol buffer结构，
         * 然后就可以一个一个域处理了。
         * 如果格式不匹配会返回false，就要检查一下是什么原因了。
         */
        NFdbExample::NowPlayingDetails now_playing;
        if (msg->deserialize(now_playing))
        {
            const char *artist = now_playing.artist().c_str();
            const char *album = now_playing.album().c_str();
            const char *genre = now_playing.genre().c_str();
            const char *title = now_playing.title().c_str();
            const char *file_name = "";
            if (now_playing.has_file_name())
            {
                file_name = now_playing.file_name().c_str();
            }
            const char *folder_name = "";
            if (now_playing.has_folder_name())
            {
                folder_name = now_playing.folder_name().c_str();
            }
            int32_t elapse_time = now_playing.elapse_time();
            FDB_LOG_I("OBJ %d, artist: %s, album: %s, genre: %s, title: %s, file name: %s, folder name: %s, elapse time: %d\n",
                        this->objId(), artist, album, genre, title, file_name, folder_name, elapse_time);
        }
        else
        {
            FDB_LOG_I("OBJ %d Error! Unable to decode message!!!\n", this->objId());
        }

        std::string raw_buffer("raw buffer test for invoke()!");
        this->invoke(REQ_RAWDATA, raw_buffer.c_str(), raw_buffer.length() + 1);

        /*
         * trigger update manually; onBroadcast() will be called followed by
         * onStatus().
         */
        CFdbMsgTriggerList update_list;
        this->addManualTrigger(update_list, NTF_MANUAL_UPDATE);
        this->update(update_list);
    }

protected:
    /*
     * 当client与server连上时被调用。由于client只会创建一条session，
     * is_first可以不用检查。
     */
    void onOnline(FdbSessionId_t sid, bool is_first)
    {
        FDB_LOG_I("OBJ %d client session online: %d\n", this->objId(), sid);
        if (!this->isPrimary())
        {
            this->setDefaultSession(sid);
        }
        if (is_first)
        {
            /*
             * 既然server已经上线，就要注册消息通知。注册消息时使用protocol buffer
             * 结构NFdbBase::FdbMsgSubscribe。add_msg_code_list()可以添加多个
             * 通知；add_filter_list()可以添加多个filter，与msg_code_list()消息的
             * 位置一一对应。如果没有filter则不调用add_filter_list()。
             */
            CFdbMsgSubscribeList subscribe_list;
            this->addNotifyItem(subscribe_list, NTF_ELAPSE_TIME, "my_filter");
            this->addNotifyItem(subscribe_list, NTF_ELAPSE_TIME, "raw_buffer");
            this->addNotifyItem(subscribe_list, NTF_ELAPSE_TIME);
            this->addNotifyItem(subscribe_list, NTF_MEDIAPLAYER_CREATED);
            /*
             * register NTF_MANUAL_UPDATE for manual update: it will not
             * update unless update() is called
             */
	    this->addUpdateItem(subscribe_list, NTF_MANUAL_UPDATE);
            this->subscribe(subscribe_list);

            if (this->isPrimary())
            {
#if 0
                this->invoke(REQ_CREATE_MEDIAPLAYER);
                CMyClient<CFdbBaseObject> *obj = new CMyClient<CFdbBaseObject>("mediaplayer1", &mediaplayer_worker);
                obj->connect(dynamic_cast<CBaseEndpoint *>(this), 1);

                obj = new CMyClient<CFdbBaseObject>("mediaplayer1", &mediaplayer_worker);
                obj->connect(dynamic_cast<CBaseEndpoint *>(this), 2);
#endif
            }
            else
            {
                //启动timer周期性调用server的某个方法。
                mTimer->enable();
            }
        }
    }
    /*
     * client断开连接时被调用。由于client只有一条session，is_last可以
     * 不检查。
     */
    void onOffline(FdbSessionId_t sid, bool is_last)
    {
        FDB_LOG_I("OBJ %d client session shutdown: %d\n", this->objId(), sid);
        if (is_last)
        {
            //停止工作
            mTimer->disable();
        }
    }
    /*
     * server通过broadcast()广播消息，且该消息（及filter）已经注册
     * 时被调用到。
     */
    void onBroadcast(CBaseJob::Ptr &msg_ref)
    {
        //转换成所需要的类型，onBroadcast()里只能转成CBaseMessage类型。
        CBaseMessage *msg = castToMessage<CBaseMessage *>(msg_ref);
        FDB_LOG_I("OBJ %d Broadcast is received: %d; filter: %s\n", this->objId(), msg->code(), msg->getFilter());
        //取得并打印时间戳
        //CFdbMsgMetadata md;
        //msg->metadata(md);
        //printMetadata(md);
        /*
         * 接下来就要case by case地处理收到的消息广播。msg->code()表示消息对应的码。
         *
         * 为了让代码结构更加美观及高效，可以使用类CFdbMessageHandle
         * (在CBaseEndpoint.h)中。具体实现可以参考name_server/CNameServer.cpp。
         */
        switch (msg->code())
        {
            case NTF_ELAPSE_TIME:
            {
                /*
                 * 双方约定NTF_ELAPSE_TIME对应的类型是protocol buffer结构NFdbExample::ElapseTime
                 */
                std::string filter(msg->getFilter());
                if (!filter.compare("my_filter"))
                {
                    NFdbExample::ElapseTime et;
                    if (msg->deserialize(et))
                    {
                        FDB_LOG_I("OBJ %d elapse time is received: hour: %d, minute: %d, second: %d\n",
                                    this->objId(), et.hour(), et.minute(), et.second());
                    }
                    else
                    {
                        FDB_LOG_E("Unable to decode NFdbExample::ElapseTime!\n");
                    }
                }
                else if (!filter.compare("raw_buffer"))
                {
                    if (msg->getDataEncoding() != FDB_MSG_ENC_PROTOBUF)
                    {
                        FDB_LOG_E("OBJ %d Error! Raw data is expected but protobuf is received.\n", this->objId());
                        return;
                    }
                    // const char *buffer = (char *)msg->getPayloadBuffer();
                    int32_t size = msg->getPayloadSize();
                    //std::cout << "OBJ " << this->objId() << " Broadcast of raw buffer is received: size: " << size << ", data: " << buffer << std::endl;
                    FDB_LOG_I("OBJ %d Broadcast of raw buffer is received: size: %d\n", this->objId(), size);
                }
            }
            break;
            case NTF_MEDIAPLAYER_CREATED:
            {
                NFdbExample::FdbMsgObjectInfo obj_info;
                msg->deserialize(obj_info);
                CMyClient<CFdbBaseObject> *obj = new CMyClient<CFdbBaseObject>("mediaplayer", &mediaplayer_worker);
                obj->connect(dynamic_cast<CBaseEndpoint *>(this), obj_info.obj_id());
                my_client_objects.push_back(obj);
            }
            break;
            case NTF_MANUAL_UPDATE:
            {
                FDB_LOG_I("Manual update is received!\n");
            }
            default:
            break;
        }
    }
    /*
     * 当server调用invoke()或send()时被调用。msg_ref包含收到的消息和数据。
     * 通常只有client向server发起请求，server不会请求client。但实际上client和server
     * 是对等的：都可以向对方发起请求，广播消息，收到对应的回调函数等。所以client
     * 和server统称endpoint。只是通常不会这么用。
     * 但有一个例外：当server收到onSubscribe()时，要向指定的client发送所注册消息(
     * 调用send()方法)。此时可以直接调用onBroadcast()即可。
     */
    void onInvoke(CBaseJob::Ptr &msg_ref)
    {
    }
    /*
     * 当client发起异步调用，且server调用CFdbMessage::reply()或
     * CFdbMessage::status()时被调用。msg_ref包含收到的消息和数据。
     * 对于同步调用，由于可以在调用返回后得到返回数据，onReply()将不被调用。
     */
    void onReply(CBaseJob::Ptr &msg_ref)
    {
        CBaseMessage *msg = castToMessage<CBaseMessage *>(msg_ref);
        FDB_LOG_I("OBJ %d response is receieved. sn: %d\n", this->objId(), msg->sn());
        // 打印时间戳
        CFdbMsgMetadata md;
        msg->metadata(md);
        printMetadata(this->objId(), md);
        /*
         * OK，现在可以case by case地处理所有返回值；详见callServer()
         *
         * 为了让代码结构更加美观及高效，可以使用类CFdbMessageHandle
         * (在CBaseEndpoint.h)中。具体实现可以参考name_server/CNameServer.cpp。
         */
        switch (msg->code())
        {
            case REQ_METADATA:
            {
                /*
                 * 以下详见callServer()
                 */
                if (msg->isStatus())
                {
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
                NFdbExample::NowPlayingDetails now_playing;
                if (msg->deserialize(now_playing))
                {
                    const char *artist = now_playing.artist().c_str();
                    const char *album = now_playing.album().c_str();
                    const char *genre = now_playing.genre().c_str();
                    const char *title = now_playing.title().c_str();
                    const char *file_name = "";
                    if (now_playing.has_file_name())
                    {
                        file_name = now_playing.file_name().c_str();
                    }
                    const char *folder_name = "";
                    if (now_playing.has_folder_name())
                    {
                        folder_name = now_playing.folder_name().c_str();
                    }
                    int32_t elapse_time = now_playing.elapse_time();
                    FDB_LOG_I("OBJ %d artist: %s, album: %s, genre: %s, title: %s, file name: %s, folder name: %s, elapse time: %d\n",
                                this->objId(), artist, album, genre, title, file_name, folder_name, elapse_time);
                }
                else
                {
                    FDB_LOG_I("OBJ %d Error! Unable to decode message!!!\n", this->objId());
                }
            }
            break;
            case REQ_RAWDATA:
            {
                /*
                 * 以下详见callServer()
                 */
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
            }
            break;
            default:
            break;
        }
    }
    // 暂不考虑实现该调用
    void onStatus(CBaseJob::Ptr &msg_ref
                        , int32_t error_code
                        , const char *description)
    {
        CBaseMessage *msg = castToMessage<CBaseMessage *>(msg_ref);
        if (msg->isSubscribe())
        {
            if (msg->isError())
            {
            }
            else
            {
                FDB_LOG_I("OBJ %d subscribe is ok! sn: %d is received.\n", this->objId(), msg->sn());
            }
        }
        FDB_LOG_I("OBJ %d Reason: %s\n", this->objId(), description);
    }
    void onCreateObject(CBaseEndpoint *endpoint, CFdbMessage *msg)
    {
        FDB_LOG_I("Create object %d because message %d is received.\n", msg->objectId(), msg->code());
        CMyServer<CFdbBaseObject> *obj = new CMyServer<CFdbBaseObject>("mediaplayer", &mediaplayer_worker);
        obj->bind(dynamic_cast<CBaseEndpoint *>(this), msg->objectId());
    }
protected:
    CInvokeTimer<T> *mTimer;
};

class CObjTestTimer : public CBaseLoopTimer
{
public:
    CObjTestTimer()
        : CBaseLoopTimer(1000, true)
        , mCount(0)
    {}
protected:
    void run()
    {
        mCount++;
        if (mCount%2 == 0){
            worker()->setThreadName("");
        }else{
            const char* nullp = 0;
            worker()->setThreadName(nullp);
        }
        
    #if 0
        for (std::vector<CMyClient<CFdbBaseObject> *>::iterator it = my_client_objects.begin();
                it != my_client_objects.end(); ++it)
        {
            CMyClient<CFdbBaseObject> *obj = *it;
            obj->disconnect();
        }
        for (std::vector<CMyServer<CFdbBaseObject> *>::iterator it = my_server_objects.begin();
                it != my_server_objects.end(); ++it)
        {
            CMyServer<CFdbBaseObject> *obj = *it;
            obj->unbind();
        }
    #endif
    }
    int mCount;
};

static void printUsage(void)
{
    std::cout << "ERROR:" << std::endl;
    std::cout << "argv 1:  1 - server; 0 - client" << std::endl;
    std::cout << "argv 2:  service name" << std::endl;
    std::cout << "Should run this test with arguments like these:" << std::endl;
    std::cout << "if you run server : ./fdbobjtest 1 testserver" << std::endl;
    std::cout << "if you run client : ./fdbobjtest 0 testserver" << std::endl;
}


int main(int argc, char **argv)
{
    //shared_ptr_test();
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
    //FDBUS必要的初始化并启动通信线程
    FDB_CONTEXT->start();
    //启动worker运行前面定义的timer，并作为client/server的工作线程。
    main_worker.start();
    mediaplayer_worker.start();
    /*
     * worker_ptr将作为参数用于创建client或server (endpoint)。当设为0时，endpoint
     * 的回调函数onXXX()运行在通信线程里；当设为main_worker时，onXXX()运行在
     * main_worker里。这是一个非常有用的工作模式，可以防止通信线程被阻塞。
     */
    CBaseWorker *worker_ptr = &main_worker;
    //CBaseWorker *worker_ptr = 0;

    /*
     * 参数1：1 - server； 0 - client
     * 参数2：service的名字
     *
     * 本例子使用name server做地址解析，url格式为svc://server_name。支持的协议包括：
     * - svc:
     * svc://server_name - 指定要连接或创建的服务名；前提是name server必须已经启动
     * - tcp：
     * tcp://ip_address:port_number - 指定tcp连接的ip地址和端口号；不需要name server
     * - ipc:
     * ipc://path - 指定unix域socket的路径名；不需要name server
     */
    if(argc < 3)
    {
        printUsage();
        return -1;
    }
    int32_t is_server = atoi(argv[1]);
    CObjTestTimer obj_test_timer;
    obj_test_timer.attach(&main_worker, true);

    if (is_server)
    {
        // 创建并注册server
        for (int i = 2; i < argc; ++i)
        {
            std::string server_name = argv[i];
            std::string url(FDB_URL_SVC);
            url += server_name;
            server_name += "_server";
            CMyServer<CBaseServer> *server = new CMyServer<CBaseServer>(server_name.c_str(), worker_ptr);

#ifdef OBJ_FROM_SERVER_TO_CLIENT
            for (int j = 0; j < 5; ++j)
            {
                char obj_id[64];
                sprintf(obj_id, "obj%u", j);
                std::string obj_name = server_name + obj_id;
                CMyClient<CFdbBaseObject> *obj = new CMyClient<CFdbBaseObject>(obj_name.c_str(), &mediaplayer_worker);
                obj->connect(server, 1000);
            }
#endif

            server->bind(url.c_str());
        }
        CBaseWorker background_worker;
        background_worker.start(FDB_WORKER_EXE_IN_PLACE);
    }
    else
    {
        // 创建client并连接Server
        CMyClient<CBaseClient> *client = 0;
        for (int i = 2; i < argc; ++i)
        {
            std::string server_name = argv[i];
            std::string url(FDB_URL_SVC);
            url += server_name;
            server_name += "_client";
            client = new CMyClient<CBaseClient>(server_name.c_str(), worker_ptr);
            
#ifndef OBJ_FROM_SERVER_TO_CLIENT
            for (int j = 0; j < 5; ++j)
            {
                char obj_id[64];
                sprintf(obj_id, "obj%u", j);
                std::string obj_name = server_name + obj_id;
                CMyClient<CFdbBaseObject> *obj = new CMyClient<CFdbBaseObject>(obj_name.c_str(), &mediaplayer_worker);
                obj->connect(client, 1000);
            }
#endif
            
            client->enableReconnect(true);
            client->connect(url.c_str());
        }
#if 0
        sysdep_sleep(1000);
        client->disconnect();
        main_worker.flush();
        main_worker.exit();
        main_worker.join();
        FDB_CONTEXT->destroy();
        return 0;
#endif
        CBaseWorker background_worker;
        background_worker.start(FDB_WORKER_EXE_IN_PLACE);
    }

    return 0;
}

