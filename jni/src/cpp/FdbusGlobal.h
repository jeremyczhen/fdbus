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

#ifndef __FDBUS_GLOBAL__H__
#define __FDBUS_GLOBAL__H__
#include <jni.h>
#include <string>
#include <vector>
#include <common_base/common_defs.h>
#include <common_base/CBaseJob.h>

#ifdef JNIEXPORT
    #undef JNIEXPORT
#endif
#define JNIEXPORT static

#ifdef JNICALL
    #undef JNICALL
#endif
#define JNICALL

class CFdbMessage;

class CGlobalParam
{
public:
    enum ECallbakType
    {
        EVENT,      // callback on event (client in response to broadcast)
        MESSAGE,    // callback on message (server in response to invoke)
        REPLY       // callback on reply (client in response to reply)
    };
    struct CScriptionItem
    {
        int32_t mCode;
        std::string mTopic;
        jobject mCallback;
        CScriptionItem(int32_t code, const char *topic, jobject callback)
            : mCode(code)
            , mCallback(callback)
        {
            if (topic)
            {
                mTopic = topic;
            }
        }
    };
    typedef std::vector<CScriptionItem> tSubscriptionTbl;

    static JavaVM* mJvm;
    static bool init(JNIEnv *env);
    static JNIEnv *obtainJniEnv();
    static void releaseJniEnv(JNIEnv *env);
    static jbyteArray createRawPayloadBuffer(JNIEnv *env, const CFdbMessage *msg);
    static int32_t jniRegisterNativeMethods(JNIEnv* env,
                                    const char* className,
                                    const JNINativeMethod* gMethods,
                                    int32_t numMethods);
    static void getSubscriptionList(JNIEnv *env, jobject sub_items, tSubscriptionTbl &sub_tbl);
    static void callAction(jobject callback, CBaseJob::Ptr &msg_ref, ECallbakType type);
    static void callConnection(jobject callback, bool is_online, bool is_first);
};

class CFdbusClientParam
{
public:
    static jmethodID mOnOnline;
    static jmethodID mOnOffline;
    static jmethodID mOnReply;
    static jmethodID mOnGetEvent;
    static jmethodID mOnBroadcast;
    static bool init(JNIEnv *env, jclass clazz);
};

class CFdbusServerParam
{
public:
    static jmethodID mOnOnline;
    static jmethodID mOnOffline;
    static jmethodID mOnInvoke;
    static jmethodID mOnSubscribe;
    static bool init(JNIEnv *env, jclass clazz);
};

class CFdbusSubscribeItemParam
{
public:
    static jfieldID mCode;
    static jfieldID mTopic;
    static jfieldID mCallback;

    static bool init(JNIEnv *env, jclass &clazz);
    static jclass mClass;
};

class CFdbusMessageParam
{
public:
    static bool init(JNIEnv *env, jclass &clazz);
    static jclass mClass;
};

class CFdbusConnectionParam
{
public:
    static jmethodID mHandleConnection;
    static bool init(JNIEnv *env, jclass &clazz);
};

class CFdbusActionParam
{
public:
    static jmethodID mHandleMessage;
    static bool init(JNIEnv *env, jclass &clazz);
    static jclass mClass;
};

#endif
