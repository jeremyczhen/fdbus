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
#include <common_base/CFdbMessage.h>
#include <common_base/CFdbContext.h>
#include <common_base/CLogProducer.h>
#define FDB_LOG_TAG "FDB_JNI"
#include <common_base/fdb_log_trace.h>

#define FDB_JNI_VERSION            JNI_VERSION_1_4

JavaVM* CGlobalParam::mJvm = 0;

jmethodID CFdbusClientParam::mOnOnline = 0;
jmethodID CFdbusClientParam::mOnOffline = 0;
jmethodID CFdbusClientParam::mOnReply = 0;
jmethodID CFdbusClientParam::mOnGetEvent = 0;
jmethodID CFdbusClientParam::mOnBroadcast = 0;

jmethodID CFdbusServerParam::mOnOnline = 0;
jmethodID CFdbusServerParam::mOnOffline = 0;
jmethodID CFdbusServerParam::mOnInvoke = 0;
jmethodID CFdbusServerParam::mOnSubscribe = 0;

jfieldID CFdbusSubscribeItemParam::mCode = 0;
jfieldID CFdbusSubscribeItemParam::mTopic = 0;
jclass CFdbusSubscribeItemParam::mClass = 0;

jclass CFdbusMessageParam::mClass = 0;

bool CGlobalParam::init(JNIEnv *env)
{
    FDB_CONTEXT->start();
    if (mJvm)
    {
        return true;
    }
    env->GetJavaVM(&mJvm);
    if (mJvm)
    {
        return true;
    }
    else
    {
        FDB_LOG_E("CGlobalParam::init: fail to create JVM!\n");
        return false;
    }
}

JNIEnv *CGlobalParam::obtainJniEnv()
{
    JNIEnv *env = 0;
    if (mJvm)
    {
        int getEnvStat = mJvm->GetEnv((void **)&env, FDB_JNI_VERSION);
        if (getEnvStat == JNI_EDETACHED)
        {
#ifdef FDB_CFG_KEEP_ENV_TYPE
            if (mJvm->AttachCurrentThread(&env, 0) != 0)
#else
            if (mJvm->AttachCurrentThread((void **)&env, 0) != 0)
#endif
            {
                FDB_LOG_E("obtainJniEnv: fail to attach!\n");
            }
        } else if (getEnvStat == JNI_OK)
        {
        }
        else if (getEnvStat == JNI_EVERSION)
        {
            FDB_LOG_E("obtainJniEnv: version not supported!\n");
        }
    }

    if (!env)
    {
        FDB_LOG_E("obtainJniEnv: fail to get JVM env!\n");
    }

    return env;
}

void CGlobalParam::releaseJniEnv(JNIEnv *env)
{
    if (env && env->ExceptionCheck())
    {
        env->ExceptionDescribe();
    }

    mJvm->DetachCurrentThread();
}

jbyteArray CGlobalParam::createRawPayloadBuffer(JNIEnv *env, const CFdbMessage *msg)
{
    int32_t len = msg->getPayloadSize();
    jbyteArray payload = 0;
    if (len)
    {
        payload =env->NewByteArray(len);
        if (payload)
        {
            env->SetByteArrayRegion(payload, 0, len, (jbyte *)msg->getPayloadBuffer());
        }
        else
        {
            FDB_LOG_E("createRawPayloadBuffer: fail to create payload buffer!\n");
        }
    }
    return payload;
}

int CGlobalParam::jniRegisterNativeMethods(JNIEnv* env,
                                           const char* className,
                                           const JNINativeMethod* gMethods,
                                           int numMethods)
{
    jclass clz = env->FindClass(className);
    if (clz)
    {
        if (env->RegisterNatives(clz, gMethods, numMethods) >= 0)
        {
            return 0;
        }
        FDB_LOG_E("RegisterNatives failed for '%s': unable to register. aborting...\n", className);
        return -1;
    }
    FDB_LOG_E("RegisterNatives failed for '%s': class not found. aborting...\n", className);
    return -1;
}

bool CFdbusClientParam::init(JNIEnv *env, jclass clazz)
{
    bool ret = false;
    
    mOnOnline = env->GetMethodID(clazz, "callbackOnline", "(I)V");
    if (!mOnOnline)
    {
        FDB_LOG_E("CFdbusClientParam::init: fail to get method mOnOnline!\n");
        goto _quit;
    }
    mOnOffline = env->GetMethodID(clazz, "callbackOffline", "(I)V");
    if (!mOnOffline)
    {
        FDB_LOG_E("CFdbusClientParam::init: fail to get method mOnOffline!\n");
        goto _quit;
    }
    mOnReply = env->GetMethodID(clazz, "callbackReply", "(II[BILjava/lang/Object;)V");
    if (!mOnReply)
    {
        FDB_LOG_E("CFdbusClientParam::init: fail to get method mOnReply!\n");
        goto _quit;
    }
    mOnGetEvent = env->GetMethodID(clazz, "callbackGetEvent", "(IILjava/lang/String;[BILjava/lang/Object;)V");
    if (!mOnGetEvent)
    {
        FDB_LOG_E("CFdbusClientParam::init: fail to get method mOnGetEvent!\n");
        goto _quit;
    }
    mOnBroadcast = env->GetMethodID(clazz, "callbackBroadcast", "(IILjava/lang/String;[B)V");
    if (!mOnBroadcast)
    {
        FDB_LOG_E("CFdbusClientParam::init: fail to get method mOnBroadcast!\n");
        goto _quit;
    }
    ret = true;
    
_quit:
    return ret;
}

bool CFdbusServerParam::init(JNIEnv *env, jclass clazz)
{
    bool ret = false;
    mOnOnline = env->GetMethodID(clazz, "callbackOnline", "(IZ)V");
    if (!mOnOnline)
    {
        FDB_LOG_E("CFdbusServerParam::init: fail to get method mOnOnline!\n");
        goto _quit;
    }
    mOnOffline = env->GetMethodID(clazz, "callbackOffline", "(IZ)V");
    if (!mOnOffline)
    {
        FDB_LOG_E("CFdbusServerParam::init: fail to get method mOnOffline!\n");
        goto _quit;
    }
    mOnInvoke = env->GetMethodID(clazz, "callbackInvoke", "(II[BJ)V");
    if (!mOnInvoke)
    {
        FDB_LOG_E("CFdbusServerParam::init: fail to get method mOnInvoke!\n");
        goto _quit;
    }
    mOnSubscribe = env->GetMethodID(clazz, "callbackSubscribe", "(IJLjava/util/ArrayList;)V");
    if (!mOnSubscribe)
    {
        FDB_LOG_E("CFdbusServerParam::init: fail to get method mOnSubscribe!\n");
        goto _quit;
    }
    ret = true;
    
_quit:
    return ret;
}

bool CFdbusSubscribeItemParam::init(JNIEnv *env, jclass &clazz)
{
    bool ret = false;
    
    mCode = env->GetFieldID(clazz, "mCode", "I");
    if (!mCode)
    {
        FDB_LOG_E("CFdbusSubscribeItemParam::init: fail to get field mCode!\n");
        goto _quit;
    }
    mTopic = env->GetFieldID(clazz, "mTopic", "Ljava/lang/String;");
    if (!mTopic)
    {
        FDB_LOG_E("CFdbusSubscribeItemParam::init: fail to get field mTopic!\n");
        goto _quit;
    }

    mClass = reinterpret_cast<jclass>(env->NewGlobalRef(clazz));

    ret = true;
    
_quit:
    return ret;
}

bool CFdbusMessageParam::init(JNIEnv *env, jclass &clazz)
{
    mClass = reinterpret_cast<jclass>(env->NewGlobalRef(clazz));
    return true;
}

JNIEXPORT void JNICALL Java_ipc_fdbus_Fdbus_fdb_1init
                          (JNIEnv * env,
                           jobject,
                           jclass server_class,
                           jclass client_class,
                           jclass sub_item_class,
                           jclass fdb_msg_class)
{
    CGlobalParam::init(env);
    CFdbusServerParam::init(env, server_class);
    CFdbusClientParam::init(env, client_class);
    CFdbusSubscribeItemParam::init(env, sub_item_class);
    CFdbusMessageParam::init(env, fdb_msg_class);
}

JNIEXPORT void JNICALL Java_ipc_fdbus_Fdbus_fdb_1log_1trace
  (JNIEnv *env, jclass, jstring tag, jint level, jstring data)
{
    const char *c_tag = 0;
    if (tag)
    {
        c_tag = env->GetStringUTFChars(tag, 0);
    }
    
    const char *c_data = "";
    if (data)
    {
        c_data = env->GetStringUTFChars(data, 0);
    }
    
    CLogProducer *logger = FDB_CONTEXT->getLogger();
    if (logger)
    {
        logger->logTrace((EFdbLogLevel)level, c_tag, c_data);
    }
}

static const JNINativeMethod gFdbusGlobalMethods[] = {
    {(char *)"fdb_init",
             (char *)"(Ljava/lang/Class;Ljava/lang/Class;Ljava/lang/Class;Ljava/lang/Class;)V",
             (void*) Java_ipc_fdbus_Fdbus_fdb_1init},
    {(char *)"fdb_log_trace",
             (char *)"(Ljava/lang/String;ILjava/lang/String;)V",
             (void*) Java_ipc_fdbus_Fdbus_fdb_1log_1trace},
};
  
int register_fdbus_global(JNIEnv *env)
{
    return CGlobalParam::jniRegisterNativeMethods(env,
                         "ipc/fdbus/Fdbus",
                         gFdbusGlobalMethods,
                         Fdb_Num_Elems(gFdbusGlobalMethods));
}
extern int register_fdbus_client(JNIEnv *env);
extern int register_fdbus_server(JNIEnv *env);
extern int register_fdbus_message(JNIEnv *env);
jint JNI_OnLoad(JavaVM* vm, void* /* reserved */)
{
    CGlobalParam::mJvm = vm;
    JNIEnv *env = CGlobalParam::obtainJniEnv();
    if (env)
    {
        register_fdbus_global(env);
        register_fdbus_client(env);
        register_fdbus_server(env);
        register_fdbus_message(env);
    }
    return FDB_JNI_VERSION;
}

