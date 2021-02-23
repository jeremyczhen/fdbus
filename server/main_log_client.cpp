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
#include <common_base/CBaseClient.h>
#include <common_base/fdb_option_parser.h>
#include <common_base/fdb_log_trace.h>
#include <common_base/CLogProducer.h>
#include <utils/Log.h>
#include <stdlib.h>
#include <vector>
#include <iostream>
#include "CLogPrinter.h"

static int32_t fdb_disable_request = 0;
static int32_t fdb_disable_reply = 0;
static int32_t fdb_disable_broadcast = 0;
static int32_t fdb_disable_subscribe = 0;
static int32_t fdb_disable_global_logger = 0;
// 0: no raw data; < 0: all raw data; > 0: actrual size of raw data
static int32_t fdb_raw_data_clipping_size = 0;
static int32_t fdb_config_mode = 0;
static int32_t fdb_info_mode = 0;
static int32_t fdb_debug_trace_level = (int32_t)FDB_LL_INFO;
static int32_t fdb_disable_global_trace = 0;
static int32_t fdb_cache_size = -1;
static std::vector<std::string> fdb_log_host_white_list;
static std::vector<std::string> fdb_log_endpoint_white_list;
static std::vector<std::string> fdb_log_busname_white_list;
static std::vector<std::string> fdb_trace_host_white_list;
static std::vector<std::string> fdb_trace_tag_white_list;
static bool fdb_reverse_endpoint_name = false;
static bool fdb_reverse_bus_name = false;
static bool fdb_reverse_tag = false;

static void fdb_print_whitelist(const std::vector<std::string> &white_list)
{
    if (white_list.empty())
    {
        std::cout << "    all enabled" << std::endl;
    }
    for (std::vector<std::string>::const_iterator it = white_list.begin(); it != white_list.end(); ++it)
    {
        std::cout << "    >" << *it << std::endl;
    }
}

static void fdb_print_configuration()
{
    const char *level_name[] = {
        "Verbose",
        "Debug",
        "Information",
        "Warning",
        "Error",
        "Fatal",
        "Silent"
    };
    std::cout << "request:             " << (fdb_disable_request ? "disabled" : "enabled") << std::endl;
    std::cout << "reply:               " << (fdb_disable_reply ? "disabled" : "enabled") << std::endl;
    std::cout << "broadcast:           " << (fdb_disable_broadcast ? "disabled" : "enabled") << std::endl;
    std::cout << "subscribe:           " << (fdb_disable_subscribe ? "disabled" : "enabled") << std::endl;
    std::cout << "fdbus log:           " << (fdb_disable_global_logger ? "disabled" : "enabled") << std::endl;
    std::cout << "raw data clipping:   " << fdb_raw_data_clipping_size << std::endl;
    std::cout << "debug trace level:   " << level_name[fdb_debug_trace_level] << std::endl;
    std::cout << "debug trace Log:     " << (fdb_disable_global_trace ? "disabled" : "enabled") << std::endl;
    std::cout << "exclusive endpoints: " << (fdb_reverse_endpoint_name ? "true" : "false") << std::endl;
    std::cout << "exclusive bus names: " << (fdb_reverse_bus_name ? "true" : "false") << std::endl;
    std::cout << "exclusive tags:      " << (fdb_reverse_tag ? "true" : "false") << std::endl;
    std::cout << "log cache size:      " << fdb_cache_size << "kB" << std::endl;

    std::cout << "fdbus log host white list:" << std::endl;
    fdb_print_whitelist(fdb_log_host_white_list);
    std::cout << "fdbus log endpoint white list:" << std::endl;
    fdb_print_whitelist(fdb_log_endpoint_white_list);
    std::cout << "fdbus log busname white list:" << std::endl;
    fdb_print_whitelist(fdb_log_busname_white_list);

    std::cout << "fdbus debug trace host white list:" << std::endl;
    fdb_print_whitelist(fdb_trace_host_white_list);
    std::cout << "fdbus debug trace tag white list:" << std::endl;
    fdb_print_whitelist(fdb_trace_tag_white_list);
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
        if (fdb_config_mode)
        {
            NFdbBase::FdbMsgLogConfig msg_cfg;
            fillLoggerConfigs(msg_cfg);
            {
            CFdbParcelableBuilder builder(msg_cfg);
            invoke(NFdbBase::REQ_SET_LOGGER_CONFIG, builder);
            }

            NFdbBase::FdbTraceConfig trace_cfg;
            fillTraceConfigs(trace_cfg);
            {
            CFdbParcelableBuilder builder(trace_cfg);
            invoke(NFdbBase::REQ_SET_TRACE_CONFIG, builder);
            }
        }
        else if (fdb_info_mode)
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
            log_start.set_cache_size(fdb_cache_size);
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
                mLoggerCfgReplied = true;
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
                fdb_reverse_endpoint_name = in_config.reverse_endpoint_name();
                fdb_reverse_bus_name = in_config.reverse_bus_name();
                Exit();
            }
            break;
            case NFdbBase::REQ_SET_TRACE_CONFIG:
            case NFdbBase::REQ_GET_TRACE_CONFIG:
            {
                mTraceCfgReplied = true;
                NFdbBase::FdbTraceConfig in_config;
                CFdbParcelableParser parser(in_config);
                if (!msg->deserialize(parser))
                {
                    LOG_E("CLogServer: Unable to deserialize message!\n");
                    return;
                }
                fdb_disable_global_trace = !in_config.global_enable();
                fdb_debug_trace_level = in_config.log_level();
                fdb_cache_size = in_config.cache_size();
                fdb_dump_white_list_cmd(in_config.host_white_list(), fdb_trace_host_white_list);
                fdb_dump_white_list_cmd(in_config.tag_white_list(), fdb_trace_tag_white_list);
                fdb_reverse_tag = in_config.reverse_tag();
                Exit();
            }
            break;
            default:
            break;
        }
    }
private:
    void fillLoggerConfigs(NFdbBase::FdbMsgLogConfig &config)
    {
        config.set_global_enable(!fdb_disable_global_logger);
        config.set_enable_request(!fdb_disable_request);
        config.set_enable_reply(!fdb_disable_reply);
        config.set_enable_broadcast(!fdb_disable_broadcast);
        config.set_enable_subscribe(!fdb_disable_subscribe);
        config.set_raw_data_clipping_size(fdb_raw_data_clipping_size);
        fdb_populate_white_list_cmd(config.host_white_list(), fdb_log_host_white_list);
        fdb_populate_white_list_cmd(config.endpoint_white_list(), fdb_log_endpoint_white_list);
        fdb_populate_white_list_cmd(config.busname_white_list(), fdb_log_busname_white_list);
        config.set_reverse_endpoint_name(fdb_reverse_endpoint_name);
        config.set_reverse_bus_name(fdb_reverse_bus_name);
    }

    void fillTraceConfigs(NFdbBase::FdbTraceConfig &config)
    {
        config.set_log_level((EFdbLogLevel)fdb_debug_trace_level);
        config.set_global_enable(!fdb_disable_global_trace);
        config.set_cache_size(fdb_cache_size);
        fdb_populate_white_list_cmd(config.host_white_list(), fdb_trace_host_white_list);
        fdb_populate_white_list_cmd(config.tag_white_list(), fdb_trace_tag_white_list);
        config.set_reverse_tag(fdb_reverse_tag);
    }

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
    int32_t help = 0;
    char *log_host_filters = 0;
    char *log_endpoint_filters = 0;
    char *log_busname_filters = 0;
    char *trace_host_filters = 0;
    char *trace_tag_filters = 0;
    char *reverse_selections = 0;
    const struct fdb_option core_options[] = {
        { FDB_OPTION_BOOLEAN, "request", 'q', &fdb_disable_request },
        { FDB_OPTION_BOOLEAN, "reply", 'p', &fdb_disable_reply },
        { FDB_OPTION_BOOLEAN, "broadcast", 'b', &fdb_disable_broadcast },
        { FDB_OPTION_BOOLEAN, "subscribe", 's', &fdb_disable_subscribe },
        { FDB_OPTION_BOOLEAN, "no_fdbus", 'f', &fdb_disable_global_logger },
        { FDB_OPTION_INTEGER, "clip", 'c', &fdb_raw_data_clipping_size },
        { FDB_OPTION_BOOLEAN, "cfg_mode", 'x', &fdb_config_mode },
        { FDB_OPTION_BOOLEAN, "info_mode", 'i', &fdb_info_mode },
        { FDB_OPTION_STRING, "log_endpoint", 'e', &log_endpoint_filters },
        { FDB_OPTION_STRING, "log_busname", 'n', &log_busname_filters },
        { FDB_OPTION_STRING, "log_hosts", 'm', &log_host_filters },
        { FDB_OPTION_INTEGER, "level", 'l', &fdb_debug_trace_level },
        { FDB_OPTION_BOOLEAN, "no_trace", 'd', &fdb_disable_global_trace },
        { FDB_OPTION_STRING, "trace_tags", 't', &trace_tag_filters },
        { FDB_OPTION_STRING, "trace_hosts", 'M', &trace_host_filters },
        { FDB_OPTION_STRING, "reverse_selection", 'r', &reverse_selections },
        { FDB_OPTION_INTEGER, "initial_cache_size", 'g', &fdb_cache_size },
        { FDB_OPTION_BOOLEAN, "help", 'h', &help }
    };

    if (fdb_debug_trace_level >= FDB_LL_MAX)
    {
        fdb_debug_trace_level = FDB_LL_SILENT;
    }

    fdb_parse_options(core_options, ARRAY_LENGTH(core_options), &argc, argv);
    if (help)
    {
        std::cout << "FDBus - Fast Distributed Bus" << std::endl;
        std::cout << "    SDK version " << FDB_DEF_TO_STR(FDB_VERSION_MAJOR) "."
                                           FDB_DEF_TO_STR(FDB_VERSION_MINOR) "."
                                           FDB_DEF_TO_STR(FDB_VERSION_BUILD) << std::endl;
        std::cout << "    LIB version " << CFdbContext::getFdbLibVersion() << std::endl;
        std::cout << "Usage: logviewer -x[ -q][ -p][ -b][ -s][ -f][ -c clipping_size][ -r e[,n[,t]][ -e ep1,ep2...][ -m host1,host2...][ -l][ -d][ -t tag1,tag2][ -M host1,host2...][ -h]" << std::endl;
        std::cout << "           Configure log server or" << std::endl;
        std::cout << "       logviewer -i" << std::endl;
        std::cout << "           Retrieve config info from log server or" << std::endl;
        std::cout << "       logviewer" << std::endl;
        std::cout << "           Start log client." << std::endl;
        std::cout << "    ==== Options for fdbus monitor ====" << std::endl;
        std::cout << "    -q: disable logging fdbus request" << std::endl;
        std::cout << "    -p: disable logging fdbus response" << std::endl;
        std::cout << "    -b: disable logging fdbus broadcast" << std::endl;
        std::cout << "    -s: disable logging fdbus subscribe" << std::endl;
        std::cout << "    -f: disable logging all fdbus message" << std::endl;
        std::cout << "    -c: specify size of raw data to be clipped for fdbus logging" << std::endl;
        std::cout << "    -e: specify a list of endpoints separated by ',' as white list for fdbus logging" << std::endl;
        std::cout << "    -m: specify a list of host names separated by ',' as white list for fdbus logging" << std::endl;
        std::cout << "    ==== Options for debug trace ====" << std::endl;
        std::cout << "    -l: specify debug trace level: 0-verbose 1-debug 2-info 3-warning 4-error 5-fatal 6-silent" << std::endl;
        std::cout << "    -d: disable all debug trace log" << std::endl;
        std::cout << "    -t: specify a list of tags separated by ',' as white list for debug trace" << std::endl;
        std::cout << "    -M: specify a list of host names separated by ',' as white list for debug trace" << std::endl;
        std::cout << "    ==== Other options ====" << std::endl;
        std::cout << "    -r: reverse selection of white list and can be 'e', 'n', or 't' separated by ','" << std::endl;
        std::cout << "        'e': reverse selection of endpoints specified by '-e'" << std::endl;
        std::cout << "        'n': reverse selection of bus names specified by '-n'" << std::endl;
        std::cout << "        't': reverse selection of tags specified by '-t'" << std::endl;
        std::cout << "    -g: specify size of cached logs before showing instance logs" << std::endl;
        std::cout << "        -1: all cached logs are retrieved" << std::endl;
        std::cout << "         0: don't show cached logs (default)" << std::endl;
        std::cout << "        >0: size of cached data in kB before showing instance logs" << std::endl;
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

    if (reverse_selections)
    {
        uint32_t num_reverse_selections = 0;
        char **reverse_array = strsplit(reverse_selections, ",", &num_reverse_selections);
        for (uint32_t i = 0; i < num_reverse_selections; ++i)
        {
            if (reverse_array[i][0] == 'e')
            {
                fdb_reverse_endpoint_name = true;
            }
            else if (reverse_array[i][0] == 'n')
            {
                fdb_reverse_bus_name = true;
            }
            else if (reverse_array[i][0] == 't')
            {
                fdb_reverse_tag = true;
            }
        }
    }

    FDB_CONTEXT->enableLogger(false);
    FDB_CONTEXT->init();
    
    CLogClient log_client;
    log_client.enableReconnect(true);
    log_client.connect();
    FDB_CONTEXT->start(FDB_WORKER_EXE_IN_PLACE);
    return 0;
}

