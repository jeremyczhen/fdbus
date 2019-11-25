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
 * ���ļ��������ļ�workspace/pb_idl/common.base.Example.proto�Զ����ɡ�
 * ��Ŀ���еĽӿ������ļ���ͳһ���ڸ�Ŀ¼�£���ʹ��ʱ����������
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
 * �������
 * server vs. socket��
 * һ��Server�������socket����һ��server����bind��(����)�����ַ��
 * socket vs. session��
 * ÿ��socket�������session����Ϊserver��accept()�᷵�ض���ļ�������(fd)��
 * session��
 * һ��session����һ��client-server������; ��Ϣ������session�д��ݡ�
 *
 * client vs. socket��
 * һ��client�������socket����client����connect�������ַ��server�����Ѿ�
 * ��������Щ��ַ����
 * ʵ��Ӧ���У�ÿ��clientֻ������һ����ַ��
 * socket vs. session��
 * ÿ��socket����һ��session����Ϊclientֻ��һ��fd��
 *
 * client��serverͳ��Ϊendpoint����FdbEndpointId_t��ʾ��
 * session��FdbSessionId_t��ʾ��
 * socket��FdbSocketId_t��ʾ��
 */

/*
 * ����worker�̣߳����ж�ʱ��������client��ʱ��server����Ϣ
 * ��server��ʱ��client�㲥��Ϣ
 */
static CBaseWorker main_worker;
static CBaseWorker mediaplayer_worker("mediaplayer_worker");

/*
 * �����Լ�����ϢID��
 * ͬһ��server�����е�������Ϣ�͹㲥��Ϣ��������idΨһ��ʶ��
 * ��ö�����server�ṩ��proto�ļ������client���á�
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
 * �����Լ�����Ϣ�壬���Դ��һ�ν����������ģ�context����
 * ���û�и����󣬴�����Ҳ����ֱ��ʹ��CBaseMessage��CFdbMessage��
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
 * ����һ����ʱ����������������client�㲥���ݡ�
 */
template <typename T>
class CMyServer;
static std::vector<CMyServer<CFdbBaseObject> *> my_server_objects;
template <typename T>
class CBroadcastTimer : public CMethodLoopTimer<CMyServer<T> > 
{
public:
    /*
     * 100����ʱ�����100���룻true��ʾ��������timer�������
     * ��һ�δ�����timer��
     */
    CBroadcastTimer(CMyServer<T> *server)
        : CMethodLoopTimer<CMyServer<T> >(1500, true, server, &CMyServer<T>::broadcastElapseTime)
    {}
};

/*
 * ����һ��server��ʵ�֡����еĽ��������ڻص���������ɵġ�������
 * ��Щ�ص�������Ҫʵ�ֿ��Բο�CBaseEndpoint���onXXX()������
 */
template <class T>
class CMyServer : public T
{
public:
    CMyServer(const char *name, CBaseWorker *worker = 0)
        : T(name, worker)
    {
        /*
         * ������ʱ���㲥��Ϣ��attach()��timer��main_worker�󶨣���ʱ��
         * �Ļص������������ڸ�worker��Ӧ���߳��ϡ�false��ʾ��ʱ������
         * ��ʱ����
         */
        mTimer = new CBroadcastTimer<T>(this);
        mTimer->attach(&main_worker, false);
    }
    /*
     * Timer�Ļص�������������������client�㲥����
     */
    void broadcastElapseTime(CMethodLoopTimer<CMyServer<T> > *timer)
    {
        /*
         * �������server��client�㲥����ʱ�䡣����׼����protocol buffer
         * ��ʽ������NFdbExample::ElapseTime
         */
        NFdbExample::ElapseTime et;
        et.set_hour(1);
        et.set_minute(10);
        et.set_second(35);
        /*
         * Ȼ�����ݹ㲥������ע����NTF_ELAPSE_TIME��clients��
         * Ϊ�˸���ϸ�ؿ��ƹ㲥�Ŀ����ȣ���������һ��ѡ�ĸ��ַ����������ڵ�
         * �����У�ֻ���û�ע��NTF_ELAPSE_TIMEʱ������my_filter�����յ��ù㲥��
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
     * ����client���ӽ���ʱ�����ã�sid�����client��һ�����ӡ�
     * is_first��ʾ�Ƿ��һ��client���롣
     */
    void onOnline(FdbSessionId_t sid, bool is_first)
    {
        FDB_LOG_I("OBJ %d server session up: %d\n", this->objId(), sid);
        if (is_first)
        {
            FDB_LOG_I("timer enabled\n");
            // ��һ��client������ʱ��������ʱ��
            if (!this->isPrimary())
            {
                mTimer->enable();
            }
        }
    }
    /*
     * ����client�Ͽ������Ǳ����ã�sid�����client��һ�����ӡ�
     * is_last��ʾ�Ƿ����һ��client�Ͽ����ӡ�
     */
    void onOffline(FdbSessionId_t sid, bool is_last)
    {
        FDB_LOG_I("OBJ %d server session shutdown: %d\n", this->objId(), sid);
        if (is_last)
        {
            // ���һ��client�Ͽ�����ʱ�رն�ʱ��
            mTimer->disable();
        }
    }
    /*
     * ��client����invoke()��send()ʱ�����á�msg_ref�����յ�����Ϣ�����ݡ�
     */
    void onInvoke(CBaseJob::Ptr &msg_ref)
    {
        /*
         * ����ת����CBaseMessage��onInvoke()��ֻ��ת���ɸ����͡�
         */
        CBaseMessage *msg = castToMessage<CBaseMessage *>(msg_ref);
        /*
         * ��������Ҫcase by case�ش����յ��������ˡ�msg->code()��ʾ��Ϣ��Ӧ���롣
         *
         * Ϊ���ô���ṹ�������ۼ���Ч������ʹ����CFdbMessageHandle
         * (��CBaseEndpoint.h)�С�����ʵ�ֿ��Բο�name_server/CNameServer.cpp��
         */
        static int32_t elapse_time = 0;
        switch (msg->code())
        {
            case REQ_METADATA:
            {
                /*
                 * ���Ƚ����Է������������ݣ���ʽ����֪��protocol buffer
                 * �ṹNFdbExample::SongId��msg->deserialize()�����յ��Ĵ�������
                 * ��ת����ָ���ĸ�ʽ��
                 */
                NFdbExample::SongId song_id;
                if (!msg->deserialize(song_id))
                {
                    // ��ʽ����ʧ�ܣ����ش���״̬���Է���
                    msg->status(msg_ref, NFdbBase::FDB_ST_MSG_DECODE_FAIL, "Fail to decode request!");
                    return;
                }

                FDB_LOG_I("OBJ %d requested song id is: %d\n", this->objId(), song_id.id());
                /*
                 * �����������������ݣ���ʽͬ��Ϊprotocol buffer�ṹ��
                 */
                NFdbExample::NowPlayingDetails rep_msg;
#if 0
                rep_msg.set_artist("���»�");
                rep_msg.set_album("ʮ�껪�����");
                rep_msg.set_genre("���");
                rep_msg.set_title("����ˮ");
#else
                rep_msg.set_artist("Liu Dehua");
                rep_msg.set_album("Ten Year's Golden Song");
                rep_msg.set_genre("County");
                rep_msg.set_title("Wang Qing Shui");
#endif
                rep_msg.set_file_name("Lau Dewa");
                rep_msg.set_elapse_time(elapse_time++);
                // ��������ظ��Է������������
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
     * clientע��ÿһ����Ϣ֪ͨʱ��server��onSubscribe()���ᱻ���õ���һ��ǿ��
     * Ҫ���ǣ��ڸú����У�server��Ҫͨ��broadcast()����ע����Ϣ�ĵ�ǰֵ���ظ�
     * client���Ӷ�client��ע����Ϣ��ͬʱ�Ϳ����Զ��յ���Ϣ�ĵ�ǰֵ��
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
             * ����ǰ״̬���������ǰ��ע���client��
             *
             * ��������Ҫcase-by-case�ش���ͬ��Ϊ�����ۼ���Ч������ʹ����
             * CFdbSubscribeHandle(��CBaseEndpoint.h��)���������ӿ��Բο�
             * name_server/CNameServer.cpp��
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
 * ����һ����ʱ����������������server������Ϣ������
 */
template <typename T>
class CInvokeTimer : public CMethodLoopTimer<CMyClient<T> >
{
public:
    /*
     * 100����ʱ�����100���룻true��ʾ��������timer�������
     * ��һ�δ�����timer��
     */
    CInvokeTimer(CMyClient<T> *client)
        : CMethodLoopTimer<CMyClient<T> >(1000, true, client, &CMyClient<T>::callServer)
    {}
};

/*
 * ����һ��client��ʵ�֡����еĽ��������ڻص���������ɵġ�������
 * ��Щ�ص�������Ҫʵ�ֿ��Բο�CBaseEndpoint���onXXX()������
 */
template <typename T>
class CMyClient : public T
{
public:
    CMyClient(const char *name, CBaseWorker *worker = 0)
        : T(name, worker)
    {
        /*
         * ������ʱ�������Է�����ã�attach()��timer��main_worker�󶨣���ʱ��
         * �Ļص������������ڸ�worker��Ӧ���߳��ϡ�false��ʾ��ʱ������
         * ��ʱ����
         */
        mTimer = new CInvokeTimer<T>(this);
        mTimer->attach(&main_worker, false);
    }
    /*
     * Timer�Ļص����������������Ե���server�ķ���
     */
    void callServer(CMethodLoopTimer<CMyClient<T> > *timer)
    {
        /*
         * ������׼����Ҫ���͸�server��protocol buffer��ʽ������
         */
        NFdbExample::SongId song_id;
        song_id.set_id(1234);

        /*
         * �����������ݷ��͸�server��ͬ�����첽����ʹ��invoke()����
         * �����ͬ�����ã���Ҫ������Ϣ�������ref�����������ô���
         * invoke()�������ÿ��Է�ֹmsg��ϵͳ�Զ��ͷŵ����Ӷ���
         * invoke()���������ʹ��msg��÷���ֵ��
         *
         * ������첽���ã������贴��ref��ֱ�ӵ���invoke��
         * �������õİ汾���첽���õķ��ؽ��ᴥ��onReply()���ã�
         * �ڸú����п��Դ�����ֵ��
         *
         * �������Ҫ����ֵ�����Ե���send()��
         *
         * client_session_id��client��server֮���һ�����ӣ�Ҳ��������
         * ���͵�Ŀ�ĵ�ַ��
         */
        CBaseJob::Ptr ref(new CMyMessage(REQ_METADATA));
        this->invoke(ref, song_id);
        CMyMessage *msg = castToMessage<CMyMessage *>(ref);
        CFdbMsgMetadata md;
        msg->metadata(md);
        printMetadata(this->objId(), md);
        /*
         * ������ͬ�����ã�msg->invoke()��һֱ����ֱ��server����reply()��
         * �������Ϳ��Դ���server reply�����������ˡ�
         *
         * ����Ҫ��һ�������������ݷ��ػ��������쳣������ԭ����ɵ�״̬
         * ���ء�server����ͨ��reply()�����������ݣ�������ͨ��status()����
         * ʧ�ܡ���������������ݷ��أ���Ҫ�ڴ˴���
         */
        if (msg->isStatus())
        {
            /*
             * ״̬����ͳһ�ĸ�ʽ��������+ԭ�����linux�������һ�µ�
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
         * OK�����ڿ��Դ�����õķ���ֵ�ˡ���������֪�����ص�����������
         * NFdbExample::NowPlayingDetails�����Ǹ�protocol buffer��ʽ��
         * msg->deserialize()���յ��Ĵ������ݽ�����protocol buffer�ṹ��
         * Ȼ��Ϳ���һ��һ�������ˡ�
         * �����ʽ��ƥ��᷵��false����Ҫ���һ����ʲôԭ���ˡ�
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
     * ��client��server����ʱ�����á�����clientֻ�ᴴ��һ��session��
     * is_first���Բ��ü�顣
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
             * ��Ȼserver�Ѿ����ߣ���Ҫע����Ϣ֪ͨ��ע����Ϣʱʹ��protocol buffer
             * �ṹNFdbBase::FdbMsgSubscribe��add_msg_code_list()������Ӷ��
             * ֪ͨ��add_filter_list()������Ӷ��filter����msg_code_list()��Ϣ��
             * λ��һһ��Ӧ�����û��filter�򲻵���add_filter_list()��
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
                //����timer�����Ե���server��ĳ��������
                mTimer->enable();
            }
        }
    }
    /*
     * client�Ͽ�����ʱ�����á�����clientֻ��һ��session��is_last����
     * ����顣
     */
    void onOffline(FdbSessionId_t sid, bool is_last)
    {
        FDB_LOG_I("OBJ %d client session shutdown: %d\n", this->objId(), sid);
        if (is_last)
        {
            //ֹͣ����
            mTimer->disable();
        }
    }
    /*
     * serverͨ��broadcast()�㲥��Ϣ���Ҹ���Ϣ����filter���Ѿ�ע��
     * ʱ�����õ���
     */
    void onBroadcast(CBaseJob::Ptr &msg_ref)
    {
        //ת��������Ҫ�����ͣ�onBroadcast()��ֻ��ת��CBaseMessage���͡�
        CBaseMessage *msg = castToMessage<CBaseMessage *>(msg_ref);
        FDB_LOG_I("OBJ %d Broadcast is received: %d; filter: %s\n", this->objId(), msg->code(), msg->getFilter());
        //ȡ�ò���ӡʱ���
        //CFdbMsgMetadata md;
        //msg->metadata(md);
        //printMetadata(md);
        /*
         * ��������Ҫcase by case�ش����յ�����Ϣ�㲥��msg->code()��ʾ��Ϣ��Ӧ���롣
         *
         * Ϊ���ô���ṹ�������ۼ���Ч������ʹ����CFdbMessageHandle
         * (��CBaseEndpoint.h)�С�����ʵ�ֿ��Բο�name_server/CNameServer.cpp��
         */
        switch (msg->code())
        {
            case NTF_ELAPSE_TIME:
            {
                /*
                 * ˫��Լ��NTF_ELAPSE_TIME��Ӧ��������protocol buffer�ṹNFdbExample::ElapseTime
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
     * ��server����invoke()��send()ʱ�����á�msg_ref�����յ�����Ϣ�����ݡ�
     * ͨ��ֻ��client��server��������server��������client����ʵ����client��server
     * �ǶԵȵģ���������Է��������󣬹㲥��Ϣ���յ���Ӧ�Ļص������ȡ�����client
     * ��serverͳ��endpoint��ֻ��ͨ��������ô�á�
     * ����һ�����⣺��server�յ�onSubscribe()ʱ��Ҫ��ָ����client������ע����Ϣ(
     * ����send()����)����ʱ����ֱ�ӵ���onBroadcast()���ɡ�
     */
    void onInvoke(CBaseJob::Ptr &msg_ref)
    {
    }
    /*
     * ��client�����첽���ã���server����CFdbMessage::reply()��
     * CFdbMessage::status()ʱ�����á�msg_ref�����յ�����Ϣ�����ݡ�
     * ����ͬ�����ã����ڿ����ڵ��÷��غ�õ��������ݣ�onReply()���������á�
     */
    void onReply(CBaseJob::Ptr &msg_ref)
    {
        CBaseMessage *msg = castToMessage<CBaseMessage *>(msg_ref);
        FDB_LOG_I("OBJ %d response is receieved. sn: %d\n", this->objId(), msg->sn());
        // ��ӡʱ���
        CFdbMsgMetadata md;
        msg->metadata(md);
        printMetadata(this->objId(), md);
        /*
         * OK�����ڿ���case by case�ش������з���ֵ�����callServer()
         *
         * Ϊ���ô���ṹ�������ۼ���Ч������ʹ����CFdbMessageHandle
         * (��CBaseEndpoint.h)�С�����ʵ�ֿ��Բο�name_server/CNameServer.cpp��
         */
        switch (msg->code())
        {
            case REQ_METADATA:
            {
                /*
                 * �������callServer()
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
                 * �������callServer()
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
    // �ݲ�����ʵ�ָõ���
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
    //FDBUS��Ҫ�ĳ�ʼ��������ͨ���߳�
    FDB_CONTEXT->start();
    //����worker����ǰ�涨���timer������Ϊclient/server�Ĺ����̡߳�
    main_worker.start();
    mediaplayer_worker.start();
    /*
     * worker_ptr����Ϊ�������ڴ���client��server (endpoint)������Ϊ0ʱ��endpoint
     * �Ļص�����onXXX()������ͨ���߳������Ϊmain_workerʱ��onXXX()������
     * main_worker�����һ���ǳ����õĹ���ģʽ�����Է�ֹͨ���̱߳�������
     */
    CBaseWorker *worker_ptr = &main_worker;
    //CBaseWorker *worker_ptr = 0;

    /*
     * ����1��1 - server�� 0 - client
     * ����2��service������
     *
     * ������ʹ��name server����ַ������url��ʽΪsvc://server_name��֧�ֵ�Э�������
     * - svc:
     * svc://server_name - ָ��Ҫ���ӻ򴴽��ķ�������ǰ����name server�����Ѿ�����
     * - tcp��
     * tcp://ip_address:port_number - ָ��tcp���ӵ�ip��ַ�Ͷ˿ںţ�����Ҫname server
     * - ipc:
     * ipc://path - ָ��unix��socket��·����������Ҫname server
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
        // ������ע��server
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
        // ����client������Server
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

