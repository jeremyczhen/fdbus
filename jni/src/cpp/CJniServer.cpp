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
#include <ipc_fdbus_FdbusServer.h>
#include <common_base/CBaseServer.h>
#include <common_base/CFdbMessage.h>
#include <common_base/CLogProducer.h>
#include <common_base/CFdbContext.h>
#define FDB_LOG_TAG "FDB_JNI"
#include <common_base/fdb_log_trace.h>

class CJniServer : public CBaseServer
{
public:
    CJniServer(JNIEnv *env, const char *name, jobject &java_server);
    ~CJniServer();
protected:
    void onOnline(FdbSessionId_t sid, bool is_first);
    void onOffline(FdbSessionId_t sid, bool is_last);
    void onInvoke(CBaseJob::Ptr &msg_ref);
    void onSubscribe(CBaseJob::Ptr &msg_ref);
private:
    jobject mJavaServer;
};

CJniServer::CJniServer(JNIEnv *env, const char *name, jobject &java_server)
    : CBaseServer(name)
    , mJavaServer(env->NewGlobalRef(java_server))
{
}
    
CJniServer::~CJniServer()
{
    unbind();
    if (mJavaServer)
    {
        JNIEnv *env = CGlobalParam::obtainJniEnv();
        if (env)
        {
            env->DeleteGlobalRef(mJavaServer);
            mJavaServer = 0;
        }
    }
}

void CJniServer::onOnline(FdbSessionId_t sid, bool is_first)
{
    JNIEnv *env = CGlobalParam::obtainJniEnv();
    if (env)
    {
        env->CallVoidMethod(mJavaServer, CFdbusServerParam::mOnOnline, sid);
    }
    CGlobalParam::releaseJniEnv(env);
}

void CJniServer::onOffline(FdbSessionId_t sid, bool is_last)
{
    JNIEnv *env = CGlobalParam::obtainJniEnv();
    if (env)
    {
        env->CallVoidMethod(mJavaServer, CFdbusServerParam::mOnOffline, sid);
    }
    CGlobalParam::releaseJniEnv(env);
}

void CJniServer::onInvoke(CBaseJob::Ptr &msg_ref)
{
    JNIEnv *env = CGlobalParam::obtainJniEnv();
    if (env)
    {
        CFdbMessage *msg = castToMessage<CFdbMessage *>(msg_ref);
        if (msg)
        {
            CBaseJob::Ptr *msg_handle = new CBaseJob::Ptr(msg_ref);
            env->CallVoidMethod(mJavaServer,
                                CFdbusServerParam::mOnInvoke,
                                msg->session(),
                                msg->code(),
                                CGlobalParam::createRawPayloadBuffer(env, msg),
                                msg->getDataEncoding(),
                                msg_handle
                                );
        }
    }
    CGlobalParam::releaseJniEnv(env);
}

void CJniServer::onSubscribe(CBaseJob::Ptr &msg_ref)
{
    JNIEnv *env = CGlobalParam::obtainJniEnv();
    if (env)
    {
        jclass cls_arr_list = env->FindClass("java/util/ArrayList");
        if (!cls_arr_list)
        {
            FDB_LOG_E("onSubscribe: unable find java/util/ArrayList!\n");
            return;
        }
        jmethodID constructor = env->GetMethodID(cls_arr_list, "<init>", "()V");
        if (!constructor)
        {
            FDB_LOG_E("onSubscribe: unable to get constructor for ava/util/ArrayList!\n");
            return;
        }
        jobject obj_arr_list = env->NewObject(cls_arr_list, constructor, "");
        if (!obj_arr_list)
        {
            FDB_LOG_E("onSubscribe: unable to create object of ava/util/ArrayList!\n");
            return;
        }
        jmethodID arr_list_add = env->GetMethodID(cls_arr_list, "add" , "(Ljava/lang/Object;)Z");
        if (!arr_list_add)
        {
            FDB_LOG_E("onSubscribe: unable to find add method for ava/util/ArrayList!\n");
            return;
        }

        constructor = env->GetMethodID(
                                    CFdbusSubscribeItemParam::mClass,
                                    "<init>",
                                    "(ILjava/lang/String;)V");
        if (!constructor)
        {
            FDB_LOG_E("onSubscribe: unable to get constructor for subscribe item!\n");
            return;
        }
        
        CFdbMessage *msg = castToMessage<CFdbMessage *>(msg_ref);
        const CFdbMsgSubscribeItem *sub_item;
        /* iterate all message id subscribed */
        FDB_BEGIN_FOREACH_SIGNAL(msg, sub_item)
        {
            FdbMsgCode_t msg_code = sub_item->msg_code();
            const char *c_filter = "";
            if (sub_item->has_filter())
            {
                c_filter = sub_item->filter().c_str();
            }
            jstring filter = c_filter ? env->NewStringUTF(c_filter) : 0;
            jobject j_sub_item = env->NewObject(CFdbusSubscribeItemParam::mClass,
                                                constructor, msg_code, filter);
            if (!j_sub_item)
            {
                FDB_LOG_E("onSubscribe: unable to create subscribe item!\n");
                continue;
            }
            env->CallObjectMethod(obj_arr_list, arr_list_add, j_sub_item);
        }
        FDB_END_FOREACH_SIGNAL()

        CBaseJob::Ptr *msg_handle = new CBaseJob::Ptr(msg_ref);
        env->CallVoidMethod(mJavaServer,
                            CFdbusServerParam::mOnSubscribe,
                            msg->session(),
                            msg_handle,
                            obj_arr_list
                            );
    }
    CGlobalParam::releaseJniEnv(env);
}

JNIEXPORT jlong JNICALL Java_ipc_fdbus_FdbusServer_fdb_1create
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
        endpoint_name = "default server";
        FDB_LOG_W("Java_FdbusServer_fdb_1create: using %s as default name!\n", endpoint_name);
    }
    jlong handle = (jlong) new CJniServer(env, endpoint_name, thiz);
    if (c_name)
    {
        env->ReleaseStringUTFChars(name, c_name);
    }
    return handle;
}

JNIEXPORT void JNICALL Java_ipc_fdbus_FdbusServer_fdb_1destroy
  (JNIEnv *, jobject, jlong handle)
{
    CJniServer *server = (CJniServer *)handle;
    if (server)
    {
        delete server;
    }
}
 
JNIEXPORT jboolean JNICALL Java_ipc_fdbus_FdbusServer_fdb_1bind
  (JNIEnv *env, jobject, jlong handle, jstring url)
{
    bool ret = false;
    const char* c_url = env->GetStringUTFChars(url, 0);
    if (c_url)
    {
        CJniServer *server = (CJniServer *)handle;
        if (server)
        {
            server->bind(c_url);
            ret = true;
        }
        env->ReleaseStringUTFChars(url, c_url);
    }
    return ret;
}

JNIEXPORT jboolean JNICALL Java_ipc_fdbus_FdbusServer_fdb_1unbind
  (JNIEnv *, jobject, jlong handle)
{
    CJniServer *server = (CJniServer *)handle;
    if (server)
    {
        server->unbind();
        return true;
    }

    return false;
}

JNIEXPORT jboolean JNICALL Java_ipc_fdbus_FdbusServer_fdb_1broadcast
                          (JNIEnv *env,
                           jobject,
                           jlong handle,
                           jint msg_code,
                           jstring filter,
                           jbyteArray pb_data,
                           jint encoding,
                           jstring log_data)
{
    CJniServer *server = (CJniServer *)handle;
    if (!server)
    {
        return false;
    }

    if (encoding >= FDB_MSG_ENC_MAX)
    {
        FDB_LOG_E("Java_FdbusServer_fdb_1broadcast: encoding %d out of range!\n", encoding);
        return false;
    }
    
    const char *c_log_data = 0;
    if (log_data)
    {
        c_log_data = env->GetStringUTFChars(log_data, 0);
    }

    const char *c_filter = 0;
    if (filter)
    {
        c_filter = env->GetStringUTFChars(filter, 0);
    }

    jbyte *c_array = 0;
    int len_arr = 0;
    if (pb_data)
    {
        c_array = env->GetByteArrayElements(pb_data, 0);
        len_arr = env->GetArrayLength(pb_data);
    }

    bool ret = server->broadcast(msg_code, c_filter, c_array, len_arr, 
                             (EFdbMessageEncoding)encoding, c_log_data);

    if (c_log_data)
    {
        env->ReleaseStringUTFChars(log_data, c_log_data);
    }
    if (c_array)
    {
        env->ReleaseByteArrayElements(pb_data, c_array, 0);
    }
    if (c_filter)
    {
        env->ReleaseStringUTFChars(filter, c_filter);
    }

    return ret;
}

JNIEXPORT jstring JNICALL Java_ipc_fdbus_FdbusServer_fdb_1endpoint_1name
  (JNIEnv *env, jobject, jlong handle)
{
    CJniServer *server = (CJniServer *)handle;
    const char *name = "";
   
    if (server)
    {
        name = server->name().c_str();
    }
   
   return env->NewStringUTF(name);
}

JNIEXPORT jstring JNICALL Java_ipc_fdbus_FdbusServer_fdb_1bus_1name
  (JNIEnv *env, jobject, jlong handle)
{
    CJniServer *server = (CJniServer *)handle;
    const char *name = "";
 
    if (server)
    {
        name = server->nsName().c_str();
    }
 
    return env->NewStringUTF(name);
}

JNIEXPORT jboolean JNICALL Java_ipc_fdbus_FdbusServer_fdb_1log_1enabled
  (JNIEnv *env, jobject, jlong handle, jint msg_type)
{
    CJniServer *server = (CJniServer *)handle;
    if (server)
    {
        CLogProducer *logger = CFdbContext::getInstance()->getLogger();
        if (logger && logger->checkLogEnabled((EFdbMessageType)msg_type, 0, server))
        {
            return true;
        }
    }
    
    return false;
}

