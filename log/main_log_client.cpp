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

#include "fdb_log_config.h"
#include <common_base/CFdbContext.h>
#include <common_base/CBaseClient.h>
#include <common_base/fdb_log_trace.h>
#include <common_base/CLogProducer.h>
#include <utils/Log.h>
#include <stdlib.h>
#include <iostream>
#include "CLogPrinter.h"

class CLogClient : public CBaseClient
{
public:
    CLogClient()
        : CBaseClient(FDB_LOG_SERVER_NAME)
        , mLoggerCfgReplied(false)
        , mTraceCfgReplied(false)
    {
    }
protected:
    void onBroadcast(CBaseJob::Ptr &msg_ref)
    {
        CFdbMessage *msg = castToMessage<CFdbMessage *>(msg_ref);
        switch (msg->code())
        {
            case NFdbBase::NTF_FDBUS_LOG:
            {
                CFdbSimpleDeserializer deserializer(msg->getPayloadBuffer(), msg->getPayloadSize());
                mLogPrinter.outputFdbLog(deserializer, msg, std::cout);
            }
            break;
            case NFdbBase::NTF_TRACE_LOG:
            {
                CFdbSimpleDeserializer deserializer(msg->getPayloadBuffer(), msg->getPayloadSize());
                mLogPrinter.outputTraceLog(deserializer, msg, std::cout);
            }
            break;
            default:
            break;
        }
    }
    
    void onOnline(FdbSessionId_t sid, bool is_first)
    {
        if (gFdbLogConfig.fdb_config_mode == CFG_MODE_ABSOLUTE)
        {
            NFdbBase::FdbMsgLogConfig msg_cfg;
            fdb_fill_logger_config(msg_cfg, !gFdbLogConfig.fdb_disable_global_logger);
            {
            CFdbParcelableBuilder builder(msg_cfg);
            invoke(NFdbBase::REQ_SET_LOGGER_CONFIG, builder);
            }

            NFdbBase::FdbTraceConfig trace_cfg;
            fdb_fill_trace_config(trace_cfg, !gFdbLogConfig.fdb_disable_global_trace);
            {
            CFdbParcelableBuilder builder(trace_cfg);
            invoke(NFdbBase::REQ_SET_TRACE_CONFIG, builder);
            }
        }
        else if ((gFdbLogConfig.fdb_config_mode == CFG_MODE_INCREMENTAL) || gFdbLogConfig.fdb_info_mode)
        {
            invoke(NFdbBase::REQ_GET_LOGGER_CONFIG);
            invoke(NFdbBase::REQ_GET_TRACE_CONFIG);
        }
        else
        {
            //CFdbMsgSubscribeList subscribe_list;
            //addNotifyItem(subscribe_list, NFdbBase::NTF_FDBUS_LOG);
            //addNotifyItem(subscribe_list, NFdbBase::NTF_TRACE_LOG);
            //subscribe(subscribe_list);
            NFdbBase::FdbLogStart log_start;
            log_start.set_cache_size(gFdbLogConfig.fdb_cache_size);
            CFdbParcelableBuilder builder(log_start);
            send(NFdbBase::REQ_LOG_START, builder);
        }
    }

    void onReply(CBaseJob::Ptr &msg_ref)
    {
        auto msg = castToMessage<CFdbMessage *>(msg_ref);
        if (msg->isStatus())
        {
            if (msg->isError())
            {
                int32_t id;
                std::string reason;
                msg->decodeStatus(id, reason);
                LOG_I("CLogClient: status is received: msg code: %d, id: %d, reason: %s\n", msg->code(), id, reason.c_str());
            }
            exit(0);
        }
        switch (msg->code())
        {
            case NFdbBase::REQ_SET_LOGGER_CONFIG:
            case NFdbBase::REQ_GET_LOGGER_CONFIG:
            {
                NFdbBase::FdbMsgLogConfig in_config;
                CFdbParcelableParser parser(in_config);

                if (!msg->deserialize(parser))
                {
                    LOG_E("CLogServer: Unable to deserialize message!\n");
                    mLoggerCfgReplied = true;
                    Exit();
                    return;
                }

                if ((msg->code() == NFdbBase::REQ_GET_LOGGER_CONFIG) &&
                    (gFdbLogConfig.fdb_config_mode == CFG_MODE_INCREMENTAL))
                {
                    if (fdb_update_logger_config(in_config, !gFdbLogConfig.fdb_disable_global_logger))
                    {
                        CFdbParcelableBuilder builder(in_config);
                        invoke(NFdbBase::REQ_SET_LOGGER_CONFIG, builder);
                        break;
                    }
                }
                fdb_dump_logger_config(in_config);

                mLoggerCfgReplied = true;
                Exit();
            }
            break;
            case NFdbBase::REQ_SET_TRACE_CONFIG:
            case NFdbBase::REQ_GET_TRACE_CONFIG:
            {
                NFdbBase::FdbTraceConfig in_config;
                CFdbParcelableParser parser(in_config);
                if (!msg->deserialize(parser))
                {
                    LOG_E("CLogServer: Unable to deserialize message!\n");
                    mLoggerCfgReplied = true;
                    Exit();
                    return;
                }
                if ((msg->code() == NFdbBase::REQ_GET_TRACE_CONFIG) &&
                    (gFdbLogConfig.fdb_config_mode == CFG_MODE_INCREMENTAL))
                {
                    if (fdb_update_trace_config(in_config, !gFdbLogConfig.fdb_disable_global_logger))
                    {
                        CFdbParcelableBuilder builder(in_config);
                        invoke(NFdbBase::REQ_SET_TRACE_CONFIG, builder);
                        break;
                    }
                }
                fdb_dump_trace_config(in_config);

                mTraceCfgReplied = true;
                Exit();
            }
            break;
            default:
            break;
        }
    }
private:
    void Exit()
    {
        if (mLoggerCfgReplied && mTraceCfgReplied)
        {
            std::cout << "==== Configuration Items ====" << std::endl;
            fdb_print_configuration();
            exit(0);
        }
    }

    CLogPrinter mLogPrinter;
    bool mLoggerCfgReplied;
    bool mTraceCfgReplied;
};

int main(int argc, char **argv)
{
#ifdef __WIN32__
    WORD wVersionRequested;
    WSADATA wsaData;
    int err;

    /* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
    wVersionRequested = MAKEWORD(2, 2);

    err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0)
    {
        /* Tell the user that we could not find a usable */
        /* Winsock DLL.                                  */
        printf("WSAStartup failed with error: %d\n", err);
        return 1;
    }
#endif

    if (!initLogConfig(argc, argv, true))
    {
        return 0;
    }

    FDB_CONTEXT->enableLogger(false);
    FDB_CONTEXT->init();
    
    CLogClient log_client;
    log_client.enableReconnect(true);
    log_client.connect();
    FDB_CONTEXT->start(FDB_WORKER_EXE_IN_PLACE);
    return 0;
}

