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

#ifndef _COMMON_DEFS_H_
#define _COMMON_DEFS_H_

#include <stdint.h>

#define FDB_VERSION_MAJOR 4
#define FDB_VERSION_MINOR 3
#define FDB_VERSION_BUILD 0

#define FDB_INVALID_ID (~0)
typedef uint16_t FdbEndpointId_t;
typedef int32_t FdbSessionId_t;
typedef int32_t FdbSocketId_t;
typedef int32_t FdbMsgCode_t;
typedef uint32_t FdbMsgSn_t;
typedef FdbMsgCode_t FdbEventCode_t;
typedef uint32_t FdbObjectId_t;
typedef uint8_t FdbEventGroup_t;

#define FDB_NAME_SERVER_NAME            "org.fdbus.name-server"
#define FDB_HOST_SERVER_NAME            "org.fdbus.host-server"
#define FDB_LOG_SERVER_NAME             "org.fdbus.log-server"
#define FDB_NOTIFICATION_CENTER_NAME    "org.fdbus.notification-center"
#define FDB_XTEST_NAME                  "org.fdbus.xtest-server"

#define FDB_URL_TCP_IND "tcp"
#define FDB_URL_IPC_IND "ipc"
#define FDB_URL_SVC_IND "svc"
#define FDB_URL_UDP_IND "udp"

#define FDB_URL_TCP FDB_URL_TCP_IND "://"
#define FDB_URL_IPC FDB_URL_IPC_IND "://"
#define FDB_URL_SVC FDB_URL_SVC_IND "://"
#define FDB_URL_UDP FDB_URL_UDP_IND "://"

#define FDB_IP_ALL_INTERFACE "0"
#define FDB_SYSTEM_PORT 0

#define FDB_OBJECT_MAIN 0
#define FDB_OBJECT_SN_SHIFT 16
#define FDB_OBJECT_CLASS_MASK 0xFFFF
#define FDB_OBJECT_MAKE_ID(_sn, _class) (((_sn) << FDB_OBJECT_SN_SHIFT) | \
                                        ((_class) & FDB_OBJECT_CLASS_MASK))
#define FDB_OBJECT_GET_CLASS(_id) ((_id) & FDB_OBJECT_CLASS_MASK)

#define FDB_LOCAL_HOST "127.0.0.1"

#define FDB_SECURITY_LEVEL_NONE 0

#ifdef __cplusplus
template <typename T>
bool fdbValidFdbId(T id)
{
    return id != (T)FDB_INVALID_ID;
}
template<typename To, typename From>
inline To fdb_dynamic_cast_if_available(From from) {
#if defined(CONFIG_FDB_NO_RTTI) || !defined(__cpp_rtti)
  return static_cast<To>(from);
#else
  return dynamic_cast<To>(from);
#endif

#define fdb_remove_value_from_container(_container, _value) do { \
    auto it = std::find((_container).begin(), (_container).end(), _value); \
    if (it != (_container).end()) \
    { \
        (_container).erase(it); \
    } \
} while (0)

}
#endif

#ifndef Fdb_Num_Elems
#define Fdb_Num_Elems(_arr_) ((int32_t) (sizeof(_arr_) / sizeof((_arr_)[0])))
#endif

#if !defined(FDB_CFG_CONFIG_PATH)
#define FDB_CFG_CONFIG_PATH "/etc/fdbus"
#endif

#define FDB_CFG_CONFIG_FILE_SUFFIX ".fdb"

#if !defined(FDB_CFG_NR_SECURE_LEVEL)
#define FDB_CFG_NR_SECURE_LEVEL 4
#endif

#if !defined(FDB_CFG_TOKEN_LENGTH)
#define FDB_CFG_TOKEN_LENGTH 32
#endif

enum EFdbLogLevel
{
    FDB_LL_VERBOSE = 0,
    FDB_LL_DEBUG = 1,
    FDB_LL_INFO = 2,
    FDB_LL_WARNING = 3,
    FDB_LL_ERROR = 4,
    FDB_LL_FATAL = 5,
    FDB_LL_SILENT = 6,
    FDB_LL_MAX = 7
};

enum EFdbusCredType
{
    FDB_PERM_CRED_AUTO,
    FDB_PERM_CRED_GID,
    FDB_PERM_CRED_UID,
    FDB_PERM_CRED_MAC,
    FDB_PERM_CRED_IP
};

#define FDB_EVENT_GROUP_SHIFT 24
#define FDB_EVENT_GROUP_BITS 0xFF
#define FDB_DEFAULT_GROUP 0
#define FDB_EVENT_GROUP_MASK (FDB_EVENT_GROUP_BITS << FDB_EVENT_GROUP_SHIFT)
#define FDB_EVENT_ID_MASK (~FDB_EVENT_GROUP_MASK)
#define fdbMakeGroup(_event) ((FdbMsgCode_t)((_event) | FDB_EVENT_ID_MASK))

#define fdbMakeEventCode(_group, _event) ((FdbMsgCode_t)(((((_group) & FDB_EVENT_GROUP_BITS) << FDB_EVENT_GROUP_SHIFT) | \
                                         ((_event) & FDB_EVENT_ID_MASK))))
#define fdbmakeEventGroup(_group) fdbMakeEventCode(_group, FDB_EVENT_ID_MASK)

#define fdbIsGroup(_event) (((_event) & FDB_EVENT_ID_MASK) == FDB_EVENT_ID_MASK)
#define fdbEventGroup(_event) ((FdbEventGroup_t)((((uint32_t)(_event)) >> FDB_EVENT_GROUP_SHIFT) & FDB_EVENT_GROUP_BITS))
#define fdbEventCode(_event) ((FdbMsgCode_t)((_event) & FDB_EVENT_ID_MASK))

#define FDB_ADDRESS_CONNECT_RETRY_NR    5
#define FDB_ADDRESS_CONNECT_RETRY_INTERVAL 200

#define FDB_ADDRESS_BIND_RETRY_NR    5
#define FDB_ADDRESS_BIND_RETRY_INTERVAL 200

#endif
