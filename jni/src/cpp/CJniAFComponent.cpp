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
#include <common_base/CFdbAFComponent.h>
#define FDB_LOG_TAG "FDB_JNI"
#include <common_base/fdb_log_trace.h>

class CBaseClient;
class CBaseServer;

CBaseClient *FDB_createJniClient(const char *name);
CBaseServer *FDB_createJniServer(const char *name);

class CJniAFComponent : public CFdbAFComponent
{
public:
    CJniAFComponent(const char *name)
        : CFdbAFComponent(name)
    {
    }
protected:
    CBaseClient *createClient(const char *bus_name);
    CBaseServer *createServer(const char *bus_name);
};

CBaseClient *CJniAFComponent::createClient(const char *bus_name)
{
    return FDB_createJniClient(bus_name);
}

CBaseServer *CJniAFComponent::createServer(const char *bus_name)
{
    return FDB_createJniServer(bus_name);
}

JNIEXPORT jlong JNICALL Java_ipc_fdbus_FdbusAFComponent_fdb_1create 
  (JNIEnv *env, jobject thiz, jstring name)
{
    const char* c_name = 0;
    if (name)
    {
        c_name = env->GetStringUTFChars(name, 0);
    }

    auto component_name = c_name;
    if (!component_name)
    {
        component_name = "default component";
        FDB_LOG_W("Java_ipc_fdbus_FdbusAFComponent_fdb_1create: using %s as default name!\n", component_name);
    }
    jlong handle = (jlong) new CJniAFComponent(component_name);
    if (c_name)
    {
        env->ReleaseStringUTFChars(name, c_name);
    }
    return handle;
}

JNIEXPORT jlong JNICALL Java_ipc_fdbus_FdbusAFComponent_fdb_1query_1service 
  (JNIEnv *env, jobject thiz, jlong handle, jstring bus_name, jobject evt_handle_tbl, jobject conn_callback)
{
    auto component = (CFdbAFComponent *)handle;
    if (!component)
    {
        return false;
    }
    const char* c_name = 0;
    if (bus_name)
    {
        c_name = env->GetStringUTFChars(bus_name, 0);
    }
    if (!c_name)
    {
        return false;
    }

    CGlobalParam::tSubscriptionTbl sub_tbl;
    CGlobalParam::getSubscriptionList(env, evt_handle_tbl, sub_tbl);
    
    CFdbEventDispatcher::CEvtHandleTbl evt_tbl;
    for (uint32_t i = 0; i < (uint32_t)sub_tbl.size(); ++i)
    {
        jobject j_callback = sub_tbl[i].mCallback;
        if (!j_callback)
        {
            continue;
        }
        auto callback = sub_tbl[i].mCallback;
        component->addEvtHandle(evt_tbl, sub_tbl[i].mCode, [callback](CBaseJob::Ptr &msg_ref, CFdbBaseObject *obj)
            {
                CGlobalParam::callAction(callback, msg_ref, CGlobalParam::EVENT);
            },
            sub_tbl[i].mTopic.c_str()
        );
    }

    jlong native_handle = 0;
    if (conn_callback)
    {
        auto callback = env->NewGlobalRef(conn_callback);
        native_handle = (jlong)component->queryService(c_name, evt_tbl, [callback](
            CFdbBaseObject *obj, FdbSessionId_t sid, bool is_online, bool first_or_last) {
                CGlobalParam::callConnection(callback, is_online, first_or_last);
            }
        );
    }
    else
    {
        native_handle = (jlong)component->queryService(c_name, evt_tbl);
    }

    if (c_name)
    {
        env->ReleaseStringUTFChars(bus_name, c_name);
    }

    return native_handle;
}

JNIEXPORT jlong JNICALL Java_ipc_fdbus_FdbusAFComponent_fdb_1offer_1service
  (JNIEnv *env, jobject thiz, jlong handle, jstring bus_name, jobject msg_handle_tbl, jobject conn_callback)
{
    auto component = (CFdbAFComponent *)handle;
    if (!component)
    {
        return false;
    }
    const char* c_name = 0;
    if (bus_name)
    {
        c_name = env->GetStringUTFChars(bus_name, 0);
    }
    if (!c_name)
    {
        return false;
    }
    
    CGlobalParam::tSubscriptionTbl sub_tbl;
    CGlobalParam::getSubscriptionList(env, msg_handle_tbl, sub_tbl);

    CFdbMsgDispatcher::CMsgHandleTbl msg_tbl;
    for (uint32_t i = 0; i < (uint32_t)sub_tbl.size(); ++i)
    {
        jobject j_callback = sub_tbl[i].mCallback;
        if (!j_callback)
        {
            continue;
        }
        auto callback = sub_tbl[i].mCallback;
        component->addMsgHandle(msg_tbl, sub_tbl[i].mCode, [callback](CBaseJob::Ptr &msg_ref, CFdbBaseObject *obj)
            {
                CGlobalParam::callAction(callback, msg_ref, CGlobalParam::MESSAGE);
            }
        );
    }

    jlong native_handle = 0;
    if (conn_callback)
    {
        auto callback = env->NewGlobalRef(conn_callback);
        native_handle = (jlong)component->offerService(c_name, msg_tbl, [callback](
            CFdbBaseObject *obj, FdbSessionId_t sid, bool is_online, bool first_or_last) {
                CGlobalParam::callConnection(callback, is_online, first_or_last);
            }
        );
    }
    else
    {
        native_handle = (jlong)component->offerService(c_name, msg_tbl);
    }

    if (c_name)
    {
        env->ReleaseStringUTFChars(bus_name, c_name);
    }

    return native_handle;
}

static const JNINativeMethod gFdbusAFComponentMethods[] = {
    {(char *)"fdb_create",
             (char *)"(Ljava/lang/String;)J",
             (void*) Java_ipc_fdbus_FdbusAFComponent_fdb_1create},
    {(char *)"fdb_query_service",
             (char *)"(JLjava/lang/String;Ljava/util/ArrayList;Lipc/fdbus/FdbusAppListener$Connection;)J",
             (void*) Java_ipc_fdbus_FdbusAFComponent_fdb_1query_1service},
    {(char *)"fdb_offer_service",
             (char *)"(JLjava/lang/String;Ljava/util/ArrayList;Lipc/fdbus/FdbusAppListener$Connection;)J",
             (void*) Java_ipc_fdbus_FdbusAFComponent_fdb_1offer_1service},
};

int register_fdbus_afcomponent(JNIEnv *env)
{
    return CGlobalParam::jniRegisterNativeMethods(env,
                         "ipc/fdbus/FdbusAFComponent",
                         gFdbusAFComponentMethods,
                         Fdb_Num_Elems(gFdbusAFComponentMethods));
    return 0;
}

