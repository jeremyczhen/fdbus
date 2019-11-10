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

#include "FdbusGlobal.h"
#include <idl-gen/ipc_fdbus_FdbusClient.h>
#include <common_base/CBaseClient.h>
#include <common_base/CFdbMessage.h>
#include <common_base/CLogProducer.h>
#include <common_base/CFdbContext.h>
#define FDB_LOG_TAG "FDB_JNI"
#include <common_base/fdb_log_trace.h>

class CJniClient : public CBaseClient
{
public:
    CJniClient(JNIEnv *env, const char *name, jobject java_client);
    ~CJniClient();
protected:
    void onOnline(FdbSessionId_t sid, bool is_first);
    void onOffline(FdbSessionId_t sid, bool is_last);
    void onReply(CBaseJob::Ptr &msg_ref);
    void onBroadcast(CBaseJob::Ptr &msg_ref);
private:
    jobject mJavaClient;
};

class CJniInvokeMsg : public CBaseMessage
{
public:
    CJniInvokeMsg(FdbMsgCode_t code, EFdbMessageEncoding enc, jobject user_data)
        : CBaseMessage(code, enc)
        , mUserData(user_data)
    {
    }
    jobject mUserData;
    ~CJniInvokeMsg()
    {
        if (mUserData)
        {
            JNIEnv *env = CGlobalParam::obtainJniEnv();
            if (env)
            {
                env->DeleteGlobalRef(mUserData);
                mUserData = 0;
            }
        }
    }
};

CJniClient::CJniClient(JNIEnv *env, const char *name, jobject java_client)
    : CBaseClient(name)
    , mJavaClient(env->NewGlobalRef(java_client))
{
}
    
CJniClient::~CJniClient()
{
    disconnect();
    if (mJavaClient)
    {
        JNIEnv *env = CGlobalParam::obtainJniEnv();
        if (env)
        {
            env->DeleteGlobalRef(mJavaClient);
            mJavaClient = 0;
        }
    }
}

void CJniClient::onOnline(FdbSessionId_t sid, bool is_first)
{
    JNIEnv *env = CGlobalParam::obtainJniEnv();
    if (env)
    {
        env->CallVoidMethod(mJavaClient, CFdbusClientParam::mOnOnline, sid);
    }
    CGlobalParam::releaseJniEnv(env);
}

void CJniClient::onOffline(FdbSessionId_t sid, bool is_last)
{
    JNIEnv *env = CGlobalParam::obtainJniEnv();
    if (env)
    {
        env->CallVoidMethod(mJavaClient, CFdbusClientParam::mOnOffline, sid);
    }
    CGlobalParam::releaseJniEnv(env);
}

void CJniClient::onReply(CBaseJob::Ptr &msg_ref)
{
    JNIEnv *env = CGlobalParam::obtainJniEnv();
    if (env)
    {
        CJniInvokeMsg *msg = castToMessage<CJniInvokeMsg *>(msg_ref);
        if (msg)
        {
            int32_t error_code = NFdbBase::FDB_ST_OK;
            if (msg->isStatus())
            {
                std::string reason;
                if (!msg->decodeStatus(error_code, reason))
                {
                    FDB_LOG_E("onReply: fail to decode status!\n");
                    error_code = NFdbBase::FDB_ST_MSG_DECODE_FAIL;
                }
            }
            
            jobject user_data = 0;
            if (msg->mUserData)
            {
                user_data = env->NewLocalRef(msg->mUserData);
                env->DeleteGlobalRef(msg->mUserData);
                msg->mUserData = 0;
            }
            
            env->CallVoidMethod(mJavaClient,
                                CFdbusClientParam::mOnReply,
                                msg->session(),
                                msg->code(),
                                CGlobalParam::createRawPayloadBuffer(env, msg),
                                msg->getDataEncoding(),
                                error_code,
                                user_data
                                );
        }
    }
    CGlobalParam::releaseJniEnv(env);
}

void CJniClient::onBroadcast(CBaseJob::Ptr &msg_ref)
{
    JNIEnv *env = CGlobalParam::obtainJniEnv();
    if (env)
    {
        CBaseMessage *msg = castToMessage<CBaseMessage *>(msg_ref);
        if (msg)
        {
            const char *c_filter = msg->getFilter();
            jstring filter = c_filter ? env->NewStringUTF(c_filter) : 0;
            env->CallVoidMethod(mJavaClient,
                                CFdbusClientParam::mOnBroadcast,
                                msg->session(),
                                msg->code(),
                                filter,
                                CGlobalParam::createRawPayloadBuffer(env, msg),
                                msg->getDataEncoding()
                                );
        }
    }
    CGlobalParam::releaseJniEnv(env);
}

JNIEXPORT jlong JNICALL Java_ipc_fdbus_FdbusClient_fdb_1create
  (JNIEnv *env, jobject thiz, jstring name)
{
    const char* c_name = 0;
    if (name)
    {
        c_name = env->GetStringUTFChars(name, 0);
    }
    
    const char *endpoint_name = c_name;
    if (!endpoint_name)
    {
        endpoint_name = "default client";
        FDB_LOG_W("Java_FdbusClient_fdb_1create: using %s as default name!\n", endpoint_name);
    }
    jlong handle = (jlong) new CJniClient(env, endpoint_name, thiz);
    if (c_name)
    {
        env->ReleaseStringUTFChars(name, c_name);
    }
    return handle;
}

JNIEXPORT void JNICALL Java_ipc_fdbus_FdbusClient_fdb_1destroy
  (JNIEnv *, jobject, jlong handle)
{
    CJniClient *client = (CJniClient *)handle;
    if (client)
    {
        delete client;
    }
}
 
JNIEXPORT jboolean JNICALL Java_ipc_fdbus_FdbusClient_fdb_1connect
  (JNIEnv *env, jobject, jlong handle, jstring url)
{
    bool ret = false;
    const char* c_url = env->GetStringUTFChars(url, 0);
    if (c_url)
    {
        CJniClient *client = (CJniClient *)handle;
        if (client)
        {
            client->connect(c_url);
            ret = true;
        }
        env->ReleaseStringUTFChars(url, c_url);
    }
    return ret;
}

JNIEXPORT jboolean JNICALL Java_ipc_fdbus_FdbusClient_fdb_1disconnect
  (JNIEnv *env, jobject, jlong handle)
{
    CJniClient *client = (CJniClient *)handle;
    if (client)
    {
        client->disconnect();
        return true;
    }
    return false;
}

JNIEXPORT jboolean JNICALL Java_ipc_fdbus_FdbusClient_fdb_1invoke_1async
                              (JNIEnv *env,
                              jobject,
                              jlong handle,
                              jint code,
                              jbyteArray pb_data,
                              jint encoding,
                              jstring log_data,
                              jobject user_data,
                              jint timeout)
{
    CJniClient *client = (CJniClient *)handle;
    if (!client)
    {
        return false;
    }
    
    if (encoding >= FDB_MSG_ENC_MAX)
    {
        FDB_LOG_E("Java_FdbusClient_fdb_1invoke_1async: encoding %d out of range!\n", encoding);
        return false;
    }
    
    jbyte *c_array = 0;
    int len_arr = 0;
    if (pb_data)
    {
        c_array = env->GetByteArrayElements(pb_data, 0);
        len_arr = env->GetArrayLength(pb_data);
    }
    
    CJniInvokeMsg *msg = new CJniInvokeMsg(code, (EFdbMessageEncoding)encoding,
                                           user_data ? env->NewGlobalRef(user_data) : 0);

    const char* c_log_data = 0;
    if (log_data)
    {
        c_log_data = env->GetStringUTFChars(log_data, 0);
    }
    
    if (c_log_data)
    {
        msg->setLogData(c_log_data);
        env->ReleaseStringUTFChars(log_data, c_log_data);
    }

    jboolean ret = client->invoke(msg, (const void *)c_array, len_arr, timeout);
    if (c_array)
    {
        env->ReleaseByteArrayElements(pb_data, c_array, 0);
    }
    
    return ret;
}

JNIEXPORT jobject JNICALL Java_ipc_fdbus_FdbusClient_fdb_1invoke_1sync
                              (JNIEnv *env,
                               jobject,
                               jlong handle,
                               jint code,
                               jbyteArray pb_data,
                               jint encoding,
                               jstring log_data,
                               jint timeout)
{
    
    CJniClient *client = (CJniClient *)handle;
    if (!client)
    {
        return 0;
    }

    if (encoding >= FDB_MSG_ENC_MAX)
    {
        FDB_LOG_E("Java_FdbusClient_fdb_1invoke_1async: encoding %d out of range!\n", encoding);
        return 0;
    }

    jbyte *c_array = 0;
    int len_arr = 0;
    if (pb_data)
    {
        c_array = env->GetByteArrayElements(pb_data, 0);
        len_arr = env->GetArrayLength(pb_data);
    }

    const char* c_log_data = 0;
    if (log_data)
    {
        c_log_data = env->GetStringUTFChars(log_data, 0);
    }

    CBaseMessage *invoke_msg = new CBaseMessage(code, (EFdbMessageEncoding)encoding);
    CBaseJob::Ptr ref(invoke_msg);

    if (c_log_data)
    {
        invoke_msg->setLogData(c_log_data);
        env->ReleaseStringUTFChars(log_data, c_log_data);
    }
    
    jboolean ret = client->invoke(ref, (const void *)c_array, len_arr, timeout);
    if (c_array)
    {
        env->ReleaseByteArrayElements(pb_data, c_array, 0);
    }
    if (!ret)
    {
        FDB_LOG_E("Java_ipc_fdbus_FdbusClient_fdb_1invoke_1sync: unable to call method: %d\n", ret);
        return 0;
    }

    int32_t error_code = NFdbBase::FDB_ST_OK;
    if (invoke_msg->isStatus())
    {
        std::string reason;
        if (!invoke_msg->decodeStatus(error_code, reason))
        {
            FDB_LOG_E("onReply: fail to decode status!\n");
            error_code = NFdbBase::FDB_ST_MSG_DECODE_FAIL;
        }
    }

    jmethodID constructor = env->GetMethodID(
                                    CFdbusMessageParam::mClass,
                                    "<init>",
                                    "(II[BII)V");
    if (!constructor)
    {
        FDB_LOG_E("Java_ipc_fdbus_FdbusClient_fdb_1invoke_1sync: unable to get constructor: %d\n", ret);
        return 0;
    }
    return env->NewObject(CFdbusMessageParam::mClass,
                                    constructor,
                                    invoke_msg->session(),
                                    code,
                                    CGlobalParam::createRawPayloadBuffer(env, invoke_msg),
                                    encoding,
                                    error_code);
}

JNIEXPORT jboolean JNICALL Java_ipc_fdbus_FdbusClient_fdb_1send
                              (JNIEnv *env,
                               jobject,
                               jlong handle,
                               jint code,
                               jbyteArray pb_data,
                               jint encoding,
                               jstring log_data)
{
    CJniClient *client = (CJniClient *)handle;
    if (!client)
    {
        return false;
    }
    
    if (encoding >= FDB_MSG_ENC_MAX)
    {
        FDB_LOG_E("Java_FdbusClient_fdb_1send: encoding %d out of range!\n", encoding);
        return false;
    }

    jbyte *c_array = 0;
    int len_arr = 0;
    if (pb_data)
    {
        c_array = env->GetByteArrayElements(pb_data, 0);
        len_arr = env->GetArrayLength(pb_data);
    }

    const char* c_log_data = 0;
    if (log_data)
    {
        c_log_data = env->GetStringUTFChars(log_data, 0);
    }

    jboolean ret = client->send((FdbMsgCode_t)code,
                        (const void *)c_array,
                        len_arr,
                        (EFdbMessageEncoding)encoding,
                        c_log_data);
    if (c_array)
    {
        env->ReleaseByteArrayElements(pb_data, c_array, 0);
    }
    if (c_log_data)
    {
        env->ReleaseStringUTFChars(log_data, c_log_data);
    }
    return ret;
}

static jint getSubscriptionList(JNIEnv *env,
                                jobject sub_items,
                                CJniClient *client,
                                CFdbMsgSubscribeList &subscribe_list)
{
    jint len = 0;
    jclass sub_item_cls = env->GetObjectClass(sub_items);
    if (sub_item_cls)
    {
        jmethodID arraylist_get = env->GetMethodID(sub_item_cls, "get", "(I)Ljava/lang/Object;");
        jmethodID arraylist_size = env->GetMethodID(sub_item_cls,"size","()I");
        len = env->CallIntMethod(sub_items, arraylist_size);
        for (int i = 0; i < len; ++i)
        {
            jobject sub_item_obj = env->CallObjectMethod(sub_items, arraylist_get, i);
            if (!sub_item_obj)
            {
                FDB_LOG_E("Java_FdbusClient_fdb_1subscribe: fail to get item at %d!\n", i);
                continue;
            }
    
            jint code= env->GetIntField(sub_item_obj , CFdbusSubscribeItemParam::mCode);
            jobject filter_obj = env->GetObjectField(sub_item_obj , CFdbusSubscribeItemParam::mTopic);
            const char* c_filter = 0;
            jstring filter = 0;
            if (filter_obj)
            {
                filter = reinterpret_cast<jstring>(filter_obj);
                c_filter = env->GetStringUTFChars(filter, 0);
            }
            client->addNotifyItem(subscribe_list, code, c_filter);
            if (c_filter)
            {
                env->ReleaseStringUTFChars(filter, c_filter);
            }
            if (filter_obj)
            {
                env->DeleteLocalRef(filter_obj);
            }
            env->DeleteLocalRef(sub_item_obj);
        }
    }
    
    return len;
}

JNIEXPORT jboolean JNICALL Java_ipc_fdbus_FdbusClient_fdb_1subscribe
  (JNIEnv *env, jobject, jlong handle, jobject sub_items)
{
    CJniClient *client = (CJniClient *)handle;
    if (!client)
    {
        return false;
    }

    CFdbMsgSubscribeList subscribe_list;
    jint len = getSubscriptionList(env, sub_items, client, subscribe_list);
    return len ? client->subscribe(subscribe_list) : false;
}

JNIEXPORT jboolean JNICALL Java_ipc_fdbus_FdbusClient_fdb_1unsubscribe
  (JNIEnv *env, jobject, jlong handle, jobject sub_items)
{
    CJniClient *client = (CJniClient *)handle;
    if (!client)
    {
        return false;
    }

    CFdbMsgSubscribeList subscribe_list;
    jint len = getSubscriptionList(env, sub_items, client, subscribe_list);
    return len ? client->unsubscribe(subscribe_list) : false;
}

JNIEXPORT jstring JNICALL Java_ipc_fdbus_FdbusClient_fdb_1endpoint_1name
  (JNIEnv *env, jobject, jlong handle)
{
    CJniClient *client = (CJniClient *)handle;
    const char *name = "";
    
    if (client)
    {
        name = client->name().c_str();
    }
    
    return env->NewStringUTF(name);
}

JNIEXPORT jstring JNICALL Java_ipc_fdbus_FdbusClient_fdb_1bus_1name
  (JNIEnv *env, jobject, jlong handle)
{
    CJniClient *client = (CJniClient *)handle;
    const char *name = "";
      
    if (client)
    {
        name = client->nsName().c_str();
    }
      
    return env->NewStringUTF(name);
}

JNIEXPORT jboolean JNICALL Java_ipc_fdbus_FdbusClient_fdb_1log_1enabled
  (JNIEnv *env, jobject, jlong handle, jint msg_type)
{
    CJniClient *client = (CJniClient *)handle;
    if (client)
    {
        CLogProducer *logger = CFdbContext::getInstance()->getLogger();
        if (logger && logger->checkLogEnabled((EFdbMessageType)msg_type, 0, client))
        {
            return true;
        }
    }

    return false;
}

