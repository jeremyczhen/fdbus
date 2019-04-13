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

#ifndef _CNSCONFIG_H_
#define _CNSCONFIG_H_

#include <stdint.h>
#include <common_base/common_defs.h>

class CNsConfig
{
public:
#if !defined(FDB_CFG_SOCKET_PATH)
#define FDB_CFG_SOCKET_PATH "/tmp"
#endif

#define NS_CFG_NR_HB_RETRIES            5
#define NS_CFG_HB_INTERVAL              1000
#define NS_CFG_HB_TIMEOUT               (NS_CFG_NR_HB_RETRIES * NS_CFG_HB_INTERVAL)
#define NS_CFG_HS_RECONNECT_INTERVAL    1500
#define NS_CFG_NS_RECONNECT_INTERVAL    500
#define NS_CFG_CHECK_IP_INTERVAL        500
#define NS_CFG_ADDRESS_BIND_RETRY_NR    5
#define NS_CFG_ADDRESS_BIND_RETRY_INTERVAL 200
#define NS_CFG_ADDRESS_BIND_RETRY_CNT   5

    static const char *getHostServerName()
    {
        return FDB_HOST_SERVER_NAME;
    }

    static const char *getNameServerName()
    {
        return FDB_NAME_SERVER_NAME;
    }

    static const char *getNameServerIpcPath()
    {
        return FDB_URL_IPC FDB_CFG_SOCKET_PATH "/" "fdb-ns";
    }

    static const char *getHostServerIpcPath()
    {
        return FDB_URL_IPC FDB_CFG_SOCKET_PATH "/" "fdb-hs";
    }

    static const char *getNameServerTcpPort()
    {
        return "60001";
    }

    static const char *getHostServerTcpPort()
    {
        return "60000";
    }

    static const char *getIpcPathBase()
    {
        return FDB_URL_IPC FDB_CFG_SOCKET_PATH "/" "fdb-ipc";
    }

    static int32_t getTcpPortMin()
    {
        return 60002;
    }

    static int32_t getTcpPortMax()
    {
        return 65000;
    }

    /* Number of un-acknowledged heartbeats before lose is detected */
    static int32_t getHeartBeatRetryNr()
    {
        return NS_CFG_NR_HB_RETRIES;
    }

    /* How long heartbeat is regarded as lost if un-acknowledged */
    static int32_t getHeartBeatTimeout()
    {
        return NS_CFG_HB_TIMEOUT + 500;
    }

    /* Interval between each heartbeat */
    static int32_t getHeartBeatInterval()
    {
        return NS_CFG_HB_INTERVAL;
    }

    static int32_t getHsReconnectInterval()
    {
        return NS_CFG_HS_RECONNECT_INTERVAL;
    }

    static int32_t getCheckIpInterval()
    {
        return NS_CFG_CHECK_IP_INTERVAL;
    }

    static int32_t getNsReconnectInterval()
    {
        return NS_CFG_NS_RECONNECT_INTERVAL;
    }

    static int32_t getAddressBindRetryNr()
    {
        return NS_CFG_ADDRESS_BIND_RETRY_NR;
    }

    static int32_t getAddressBindRetryInterval()
    {
        return NS_CFG_ADDRESS_BIND_RETRY_INTERVAL;
    }

    static int32_t getAddressBindRetryCnt()
    {
        return NS_CFG_ADDRESS_BIND_RETRY_CNT;
    }
};

#endif

