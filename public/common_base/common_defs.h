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

#include <iostream>
#include <stdint.h>

#define FDB_VERSION_MAJOR 3
#define FDB_VERSION_MINOR 0
#define FDB_VERSION_BUILD 1

#define FDB_INVALID_ID (~0)
typedef uint16_t FdbEndpointId_t;
typedef int32_t FdbSessionId_t;
typedef int32_t FdbSocketId_t;
typedef int32_t FdbMsgCode_t;
typedef uint32_t FdbMsgSn_t;
typedef FdbMsgCode_t FdbEventCode_t;
typedef uint32_t FdbObjectId_t;

#define FDB_NAME_SERVER_NAME "__NameServer__"
#define FDB_HOST_SERVER_NAME "__HostServer__"
#define FDB_LOG_SERVER_NAME "__LogServer__"

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

#define FDB_SECURITY_LEVEL_NONE -1

template <typename T>
bool isValidFdbId(T id)
{
    return id != (T)FDB_INVALID_ID;
}

#define assert_true(cond) \
    if (!(cond)) \
    { \
        std::cout << "Die at line " << __LINE__ << " of file " << __FILE__ << std::endl; \
        *((char *)0) = 0; \
    }

#ifndef Num_Elems
#define Num_Elems(_arr_) sizeof(_arr_)/sizeof(_arr_[0])
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

#endif
