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
#define FDB_LOG_TAG "FDB_JNI"
#include <common_base/fdb_log_trace.h>

JNIEXPORT jboolean JNICALL Java_ipc_fdbus_FdbusMessage_fdb_1reply
                            (JNIEnv *env,
                            jobject,
                            jlong handle,
                            jbyteArray pb_data,
                            jstring log_data)
{
    auto msg_ref = (CBaseJob::Ptr *)handle;
    if (!msg_ref)
    {
        return false;
    }
    
    auto msg = castToMessage<CFdbMessage *>(*msg_ref);
    if (!msg)
    {
        FDB_LOG_E("Java_FdbusMessage_fdb_1reply: msg pointer is corrupted!\n");
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

    bool ret = msg->reply(*msg_ref, (const void *)c_array, len_arr, c_log_data);
    if (c_log_data)
    {
        env->ReleaseStringUTFChars(log_data, c_log_data);
    }
    if (c_array)
    {
        env->ReleaseByteArrayElements(pb_data, c_array, 0);
    }
    
    return ret;
}

JNIEXPORT jboolean JNICALL Java_ipc_fdbus_FdbusMessage_fdb_1broadcast
                            (JNIEnv *env,
                             jobject,
                             jlong handle,
                             jint msg_code,
                             jstring filter,
                             jbyteArray pb_data,
                             jstring log_data)
{
    auto msg_ref = (CBaseJob::Ptr *)handle;
    if (!msg_ref)
    {
        return false;
    }
    
    auto msg = castToMessage<CFdbMessage *>(*msg_ref);
    if (!msg)
    {
        FDB_LOG_E("Java_FdbusMessage_fdb_1broadcast: msg pointer is corrupted!\n");
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

    const char *c_filter = 0;
    if (filter)
    {
        c_filter = env->GetStringUTFChars(filter, 0);
    }
    
    bool ret = msg->broadcast(msg_code, (const void *)c_array, len_arr, c_filter, c_log_data);
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


JNIEXPORT void JNICALL Java_ipc_fdbus_FdbusMessage_fdb_1destroy
  (JNIEnv *env, jobject, jlong handle)
{
    auto msg_ref = (CBaseJob::Ptr *)handle;
    if (!msg_ref)
    {
        return;
    }
    msg_ref->reset();
    delete msg_ref;
}
  
JNIEXPORT jboolean JNICALL Java_ipc_fdbus_FdbusMessage_fdb_1log_1enabled
  (JNIEnv *env, jobject, jlong handle)
{
    auto msg_ref = (CBaseJob::Ptr *)handle;
    if (!msg_ref)
    {
        return false;
    }
    
    auto msg = castToMessage<CFdbMessage *>(*msg_ref);
    if (!msg)
    {
        FDB_LOG_E("Java_ipc_fdbus_FdbusMessage_fdb_1log_1enabled: msg pointer is corrupted!\n");
        return false;
    }

    return msg->isLogEnabled();
}

static const JNINativeMethod gFdbusMessageMethods[] = {
    {(char *)"fdb_reply",
             (char *)"(J[BLjava/lang/String;)Z",
             (void*) Java_ipc_fdbus_FdbusMessage_fdb_1reply},
    {(char *)"fdb_broadcast",
             (char *)"(JILjava/lang/String;[BLjava/lang/String;)Z",
             (void*) Java_ipc_fdbus_FdbusMessage_fdb_1broadcast},
    {(char *)"fdb_destroy",
             (char *)"(J)V",
             (void*) Java_ipc_fdbus_FdbusMessage_fdb_1destroy},
    {(char *)"fdb_log_enabled",
             (char *)"(J)Z",
             (void*) Java_ipc_fdbus_FdbusMessage_fdb_1log_1enabled},
};
  
int register_fdbus_message(JNIEnv *env)
{
    return CGlobalParam::jniRegisterNativeMethods(env,
                         "ipc/fdbus/FdbusMessage",
                         gFdbusMessageMethods,
                         Fdb_Num_Elems(gFdbusMessageMethods));
}

