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

#include <common_base/CFdbContext.h>
#include <common_base/CBaseServer.h>
#include <common_base/fdb_option_parser.h>
#include <common_base/fdb_log_trace.h>
#include <utils/Log.h>
#include <stdlib.h>
#include <map>
#include <string>
#include <vector>
#include <iostream>
#include "CLogPrinter.h"
#include <common_base/CFdbIfMessageHeader.h>

static int32_t fdb_disable_request = 0;
static int32_t fdb_disable_reply = 0;
static int32_t fdb_disable_broadcast = 0;
static int32_t fdb_disable_subscribe = 0;
static int32_t fdb_disable_output = 0;
static int32_t fdb_disable_global_logger = 0;
// 0: no raw data; < 0: all raw data; > 0: actrual size of raw data
static int32_t fdb_raw_data_clipping_size = 0;
static int32_t fdb_debug_trace_level = FDB_LL_INFO;
static int32_t fdb_disable_global_trace = 0;
static std::vector<std::string> fdb_log_host_white_list;
static std::vector<std::string> fdb_log_endpoint_white_list;
static std::vector<std::string> fdb_log_busname_white_list;
static std::vector<std::string> fdb_trace_host_white_list;
static std::vector<std::string> fdb_trace_tag_white_list;

static void fdb_populate_white_list(const char *filter_str, std::vector<std::string> &white_list)
{
    if (!filter_str)
    {
        return;
    }
    uint32_t num_filters = 0;
    char **filters = strsplit(filter_str, ",", &num_filters);
    for (uint32_t i = 0; i < num_filters; ++i)
    {
        white_list.push_back(filters[i]);
    }
    endstrsplit(filters, num_filters);
}

static void fdb_populate_white_list_cmd(CFdbParcelableArray<std::string> &out_filter
                                      , const std::vector<std::string> &white_list)
{
    for (auto it = white_list.begin(); it != white_list.end(); ++it)
    {
        std::string *filter = out_filter.Add();
        filter->assign(it->c_str());
    }
}

void fdb_dump_white_list_cmd(CFdbParcelableArray<std::string> &in_filter
                           , std::vector<std::string> &white_list)
{
    white_list.clear();
    for (auto it = in_filter.pool().begin(); it != in_filter.pool().end(); ++it)
    {
        white_list.push_back(*it);
    }
}

class CLogServer : public CBaseServer
{
public:
    CLogServer()
        : CBaseServer(FDB_LOG_SERVER_NAME)
    {
    }
protected:
    void onInvoke(CBaseJob::Ptr &msg_ref)
    {
        auto msg = castToMessage<CFdbMessage *>(msg_ref);
        switch (msg->code())
        {
            case NFdbBase::REQ_FDBUS_LOG:
            {
                if (!fdb_disable_output)
                {
                    CFdbSimpleDeserializer deserializer(msg->getPayloadBuffer(), msg->getPayloadSize());
                    mLogPrinter.outputFdbLog(deserializer, msg);
                }
                if (!mLoggerClientTbl.empty())
                {
                    broadcastLogNoQueue(NFdbBase::NTF_FDBUS_LOG, msg->getPayloadBuffer(), msg->getPayloadSize(), 0);
                }
            }
            break;
            case NFdbBase::REQ_LOGGER_CONFIG:
            {
                NFdbBase::FdbMsgLogConfig in_config;
                CFdbParcelableParser parser(in_config);
                if (!msg->deserialize(parser))
                {
                    LOG_E("CLogServer: Unable to deserialize message!\n");
                    return;
                }
                fdb_disable_global_logger = !in_config.global_enable();
                fdb_disable_request = !in_config.enable_request();
                fdb_disable_reply = !in_config.enable_reply();
                fdb_disable_broadcast = !in_config.enable_broadcast();
                fdb_disable_subscribe = !in_config.enable_subscribe();
                fdb_raw_data_clipping_size = in_config.raw_data_clipping_size();
                fdb_dump_white_list_cmd(in_config.host_white_list(), fdb_log_host_white_list);
                fdb_dump_white_list_cmd(in_config.endpoint_white_list(), fdb_log_endpoint_white_list);
                fdb_dump_white_list_cmd(in_config.busname_white_list(), fdb_log_busname_white_list);

                NFdbBase::FdbMsgLogConfig out_config;
                fillLoggerConfigs(out_config);
                CFdbParcelableBuilder builder(out_config);
                broadcast(NFdbBase::NTF_LOGGER_CONFIG, builder);
                msg->reply(msg_ref);
            }
            break;
            case NFdbBase::REQ_TRACE_LOG:
            {
                if (!fdb_disable_output)
                {
                    CFdbSimpleDeserializer deserializer(msg->getPayloadBuffer(), msg->getPayloadSize());
                    mLogPrinter.outputTraceLog(deserializer, msg);
                }
                if (!mLoggerClientTbl.empty())
                {
                    broadcastLogNoQueue(NFdbBase::NTF_TRACE_LOG, msg->getPayloadBuffer(), msg->getPayloadSize(), 0);
                }
            }
            break;
            case NFdbBase::REQ_TRACE_CONFIG:
            {
                NFdbBase::FdbTraceConfig in_config;
                CFdbParcelableParser parser(in_config);
                if (!msg->deserialize(parser))
                {
                    LOG_E("CLogServer: Unable to deserialize message!\n");
                    return;
                }
                fdb_disable_global_trace = !in_config.global_enable();
                fdb_debug_trace_level = in_config.log_level();
                fdb_dump_white_list_cmd(in_config.host_white_list(), fdb_trace_host_white_list);
                fdb_dump_white_list_cmd(in_config.tag_white_list(), fdb_trace_tag_white_list);

                NFdbBase::FdbTraceConfig out_config;
                fillTraceConfigs(out_config);
                CFdbParcelableBuilder builder(out_config);
                broadcast(NFdbBase::NTF_TRACE_CONFIG, builder);
                msg->reply(msg_ref);
            }
            break;
            default:
            break;
        }
    }
    
    void onSubscribe(CBaseJob::Ptr &msg_ref)
    {
        auto msg = castToMessage<CFdbMessage *>(msg_ref);
        const CFdbMsgSubscribeItem *sub_item;
        FDB_BEGIN_FOREACH_SIGNAL(msg, sub_item)
        {
            switch (sub_item->msg_code())
            {
                case NFdbBase::NTF_LOGGER_CONFIG:
                {
                    NFdbBase::FdbMsgLogConfig cfg;
                    fillLoggerConfigs(cfg);
                    CFdbParcelableBuilder builder(cfg);
                    msg->broadcast(NFdbBase::NTF_LOGGER_CONFIG, builder);
                }
                break;
                case NFdbBase::NTF_TRACE_CONFIG:
                {
                    NFdbBase::FdbTraceConfig cfg;
                    fillTraceConfigs(cfg);
                    CFdbParcelableBuilder builder(cfg);
                    msg->broadcast(NFdbBase::NTF_TRACE_CONFIG, builder);
                }
                break;
                case NFdbBase::NTF_FDBUS_LOG:
                {
                    bool is_first = mLoggerClientTbl.empty();
                    auto it = mLoggerClientTbl.find(msg->session());
                    if (it == mLoggerClientTbl.end())
                    {
                        mLoggerClientTbl[msg->session()] = new CLogClientCfg();
                    }
                    if (is_first)
                    {
                        NFdbBase::FdbMsgLogConfig cfg;
                        fillLoggerConfigs(cfg);
                        CFdbParcelableBuilder builder(cfg);
                        broadcast(NFdbBase::NTF_LOGGER_CONFIG, builder);
                    }
                }
                break;
                case NFdbBase::NTF_TRACE_LOG:
                {
                    bool is_first = mTraceClientTbl.empty();
                    auto it = mTraceClientTbl.find(msg->session());
                    if (it == mTraceClientTbl.end())
                    {
                        mTraceClientTbl[msg->session()] = new CTraceClientCfg();
                    }
                    if (is_first)
                    {
                        NFdbBase::FdbTraceConfig cfg;
                        fillTraceConfigs(cfg);
                        CFdbParcelableBuilder builder(cfg);
                        broadcast(NFdbBase::NTF_TRACE_CONFIG, builder);
                    }
                }
                break;
            }
        }
        FDB_END_FOREACH_SIGNAL()
    }

    void onOnline(FdbSessionId_t sid, bool is_first)
    {
    }

    void onOffline(FdbSessionId_t sid, bool is_last)
    {
        {
        auto it = mLoggerClientTbl.find(sid);
        if (it != mLoggerClientTbl.end())
        {
            delete it->second;
            mLoggerClientTbl.erase(it);
            if (mLoggerClientTbl.empty())
            {
                NFdbBase::FdbMsgLogConfig cfg;
                fillLoggerConfigs(cfg);
                CFdbParcelableBuilder builder(cfg);
                broadcast(NFdbBase::NTF_LOGGER_CONFIG, builder);
            }
        }
        }
        {
        auto it = mTraceClientTbl.find(sid);
        if (it != mTraceClientTbl.end())
        {
            delete it->second;
            mTraceClientTbl.erase(it);
            if (mTraceClientTbl.empty())
            {
                NFdbBase::FdbTraceConfig cfg;
                fillTraceConfigs(cfg);
                CFdbParcelableBuilder builder(cfg);
                broadcast(NFdbBase::NTF_TRACE_CONFIG, builder);
            }
        }
        }
    }
private:
    struct CLogClientCfg
    {
        CLogClientCfg( )
            : mUnused(false)
        {}
        bool mUnused;
    };
    typedef std::map<FdbSessionId_t, CLogClientCfg *> LoggerClientTbl_t;
    
    LoggerClientTbl_t mLoggerClientTbl;

    struct CTraceClientCfg
    {
        CTraceClientCfg( )
            : mUnused(false)
        {}
        bool mUnused;
    };
    typedef std::map<FdbSessionId_t, CTraceClientCfg *> TraceClientTbl_t;
    
    TraceClientTbl_t mTraceClientTbl;

    CLogPrinter mLogPrinter;

    bool checkLogEnabled(bool global_disable, bool no_client_connected)
    {
        bool cfg_enable;
        if (global_disable)
        {
            cfg_enable = false;
        }
        else if (fdb_disable_output && no_client_connected)
        {
            cfg_enable = false;
        }
        else
        {
            cfg_enable = true;
        }
        return cfg_enable;
    }

    void fillLoggerConfigs(NFdbBase::FdbMsgLogConfig &config)
    {
        config.set_global_enable(checkLogEnabled(fdb_disable_global_logger,
                                                 mLoggerClientTbl.empty()));
        config.set_enable_request(!fdb_disable_request);
        config.set_enable_reply(!fdb_disable_reply);
        config.set_enable_broadcast(!fdb_disable_broadcast);
        config.set_enable_subscribe(!fdb_disable_subscribe);
        config.set_raw_data_clipping_size(fdb_raw_data_clipping_size);
        fdb_populate_white_list_cmd(config.host_white_list(), fdb_log_host_white_list);
        fdb_populate_white_list_cmd(config.endpoint_white_list(), fdb_log_endpoint_white_list);
        fdb_populate_white_list_cmd(config.busname_white_list(), fdb_log_busname_white_list);
    }

    void fillTraceConfigs(NFdbBase::FdbTraceConfig &config)
    {
        config.set_global_enable(checkLogEnabled(fdb_disable_global_trace,
                                                 mTraceClientTbl.empty()));
        config.set_log_level((EFdbLogLevel)fdb_debug_trace_level);
        fdb_populate_white_list_cmd(config.host_white_list(), fdb_trace_host_white_list);
        fdb_populate_white_list_cmd(config.tag_white_list(), fdb_trace_tag_white_list);
    }
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
    int32_t help = 0;
    char *log_host_filters = 0;
    char *log_endpoint_filters = 0;
    char *log_busname_filters = 0;
    char *trace_host_filters = 0;
    char *trace_tag_filters = 0;

    const struct fdb_option core_options[] = {
        { FDB_OPTION_BOOLEAN, "request", 'q', &fdb_disable_request },
        { FDB_OPTION_BOOLEAN, "reply", 'p', &fdb_disable_reply },
        { FDB_OPTION_BOOLEAN, "broadcast", 'b', &fdb_disable_broadcast },
        { FDB_OPTION_BOOLEAN, "subscribe", 's', &fdb_disable_subscribe },
        { FDB_OPTION_BOOLEAN, "no_fdbus", 'f', &fdb_disable_global_logger },
        { FDB_OPTION_BOOLEAN, "output", 'o', &fdb_disable_output },
        { FDB_OPTION_INTEGER, "clip", 'c', &fdb_raw_data_clipping_size },
        { FDB_OPTION_STRING, "log_endpoint", 'e', &log_endpoint_filters },
            { FDB_OPTION_STRING, "log_busname", 'n', &log_busname_filters },
        { FDB_OPTION_STRING, "log_hosts", 'm', &log_host_filters },
        { FDB_OPTION_INTEGER, "level", 'l', &fdb_debug_trace_level },
        { FDB_OPTION_BOOLEAN, "no_trace", 'd', &fdb_disable_global_trace },
        { FDB_OPTION_STRING, "trace_tags", 't', &trace_tag_filters },
        { FDB_OPTION_STRING, "trace_hosts", 'M', &trace_host_filters },
        { FDB_OPTION_BOOLEAN, "help", 'h', &help }
    };

    fdb_parse_options(core_options, ARRAY_LENGTH(core_options), &argc, argv);
    if (help)
    {
        std::cout << "FDBus version " << FDB_VERSION_MAJOR << "."
                                      << FDB_VERSION_MINOR << "."
                                      << FDB_VERSION_BUILD << std::endl;
        std::cout << "Usage: logsvc[ -q][ -p][ -b][ -s][ -f][ -o][ -c clipping_size][ -e ep1,ep2...][ -m host1,host2...][ -l][ -d][ -t tag1,tag2][ -M host1,host2...][ -h]" << std::endl;
        std::cout << "Start log server." << std::endl;
        std::cout << "    ==== Options for fdbus monitor ====" << std::endl;
        std::cout << "    -q: disable logging fdbus request" << std::endl;
        std::cout << "    -p: disable logging fdbus response" << std::endl;
        std::cout << "    -b: disable logging fdbus broadcast" << std::endl;
        std::cout << "    -s: disable logging fdbus subscribe" << std::endl;
        std::cout << "    -f: disable logging all fdbus message" << std::endl;
        std::cout << "    -o: disable terminal output" << std::endl;
        std::cout << "    -c: specify size of raw data to be clipped" << std::endl;
        std::cout << "    -e: specify a list of endpoints separated by ',' as white list for fdbus logging" << std::endl;
        std::cout << "    -n: specify a list of bus name separated by ',' as white list for fdbus logging" << std::endl;
        std::cout << "    -m: specify a list of host names separated by ',' as white list for fdbus logging" << std::endl;
        std::cout << "    ==== Options for debug trace   ====" << std::endl;
        std::cout << "    -l: specify debug trace level: 0-verbose 1-debug 2-info 3-warning 4-error 5-fatal 6-silent" << std::endl;
        std::cout << "    -d: disable all debug trace log" << std::endl;
        std::cout << "    -t: specify a list of tags separated by ',' as white list for debug trace" << std::endl;
        std::cout << "    -M: specify a list of host names separated by ',' as white list for debug trace" << std::endl;
        std::cout << "    -h: print help" << std::endl;
        std::cout << "    ==== fdbus monitor log format: ====" << std::endl;
        std::cout << "    [F]" << std::endl;
        std::cout << "    [pid of message sender]" << std::endl;
        std::cout << "    [host name of message sender]" << std::endl;
        std::cout << "    [message sender name -> message receiver name]" << std::endl;
        std::cout << "    [service name of fdbus]" << std::endl;
        std::cout << "    [object id]" << std::endl;
        std::cout << "    [message type]" << std::endl;
        std::cout << "    [message code]" << std::endl;
        std::cout << "    [serial number]" << std::endl;
        std::cout << "    [size of payload]" << std::endl;
        std::cout << "    [time stamp]" << std::endl;
        std::cout << "    message details" << std::endl;
        std::cout << "    ==== debug trace log format: ====" << std::endl;
        std::cout << "    [D]" << std::endl;
        std::cout << "    [level: V-verbose; D-Debug; I-Information; W-Warning; E-Error; F-Fatal; S-Silence]" << std::endl;
        std::cout << "    [tag]" << std::endl;
        std::cout << "    [pid of log generator]" << std::endl;
        std::cout << "    [host name of log generator]" << std::endl;
        std::cout << "    [time stamp]" << std::endl;
        std::cout << "    log information" << std::endl;
        return 0;
    }

    fdb_populate_white_list(log_host_filters, fdb_log_host_white_list);
    fdb_populate_white_list(log_endpoint_filters, fdb_log_endpoint_white_list);
    fdb_populate_white_list(log_busname_filters, fdb_log_busname_white_list);
    fdb_populate_white_list(trace_host_filters, fdb_trace_host_white_list);
    fdb_populate_white_list(trace_tag_filters, fdb_trace_tag_white_list);

    FDB_CONTEXT->enableLogger(false);
    FDB_CONTEXT->init();
    
    CLogServer log_server;
    log_server.bind();
    FDB_CONTEXT->start(FDB_WORKER_EXE_IN_PLACE);
    return 0;
}

