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
#include <common_base/CLogProducer.h>
#include <utils/Log.h>
#include <stdlib.h>
#include <map>
#include <string>
#include <vector>
#include <iostream>
#include "CLogPrinter.h"
#include "CFdbLogCache.h"
#include "CLogFileManager.h"

static int32_t fdb_disable_request = 0;
static int32_t fdb_disable_reply = 0;
static int32_t fdb_disable_broadcast = 0;
static int32_t fdb_disable_subscribe = 0;
static int32_t fdb_disable_std_output = 0;
static int32_t fdb_disable_global_logger = 0;
// 0: no raw data; < 0: all raw data; > 0: actrual size of raw data
static int32_t fdb_raw_data_clipping_size = 0;
static int32_t fdb_debug_trace_level = FDB_LL_INFO;
static int32_t fdb_disable_global_trace = 0;
static int32_t fdb_cache_size = 0; // initial cache size: 0kB
static std::vector<std::string> fdb_log_host_white_list;
static std::vector<std::string> fdb_log_endpoint_white_list;
static std::vector<std::string> fdb_log_busname_white_list;
static std::vector<std::string> fdb_trace_host_white_list;
static std::vector<std::string> fdb_trace_tag_white_list;
static bool fdb_reverse_endpoint_name = false;
static bool fdb_reverse_bus_name = false;
static bool fdb_reverse_tag = false;
static std::string fdb_log_path;
static int64_t fdb_max_log_storage_size = 0;
static int64_t fdb_max_file_size = 0;

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
        , mLogCache(fdb_cache_size * 1024)
        , mFileManager(fdb_log_path.c_str(), 0, fdb_max_log_storage_size * 1024, fdb_max_file_size * 1024)
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
                if (!fdb_disable_std_output || mFileManager.logEnabled())
                {
                    CFdbSimpleDeserializer deserializer(msg->getPayloadBuffer(), msg->getPayloadSize());
                    std::ostringstream ostream;
                    mLogPrinter.outputFdbLog(deserializer, msg, ostream);

                    if (!fdb_disable_std_output)
                    {
                        std::cout << ostream.str();
                    }
                    if (mFileManager.logEnabled())
                    {
                        mFileManager.store(ostream.str());
                    }
                }
                forwardLogData(NFdbBase::NTF_FDBUS_LOG, msg);
            }
            break;
            case NFdbBase::REQ_SET_LOGGER_CONFIG:
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
                fdb_reverse_endpoint_name = in_config.reverse_endpoint_name();
                fdb_reverse_bus_name = in_config.reverse_bus_name();

                NFdbBase::FdbMsgLogConfig out_config;
                fillLoggerConfigs(out_config);
                CFdbParcelableBuilder builder(out_config);
                broadcast(NFdbBase::NTF_LOGGER_CONFIG, builder);
                msg->reply(msg_ref, builder);
            }
            break;
            case NFdbBase::REQ_GET_LOGGER_CONFIG:
            {
                NFdbBase::FdbMsgLogConfig out_config;
                fillLoggerConfigs(out_config);
                CFdbParcelableBuilder builder(out_config);
                msg->reply(msg_ref, builder);
            }
            break;
            case NFdbBase::REQ_TRACE_LOG:
            {
                if (!fdb_disable_std_output || mFileManager.logEnabled())
                {
                    CFdbSimpleDeserializer deserializer(msg->getPayloadBuffer(), msg->getPayloadSize());
                    std::ostringstream ostream;
                    mLogPrinter.outputTraceLog(deserializer, msg, ostream);

                    if (!fdb_disable_std_output)
                    {
                        std::cout << ostream.str();
                    }
                    if (mFileManager.logEnabled())
                    {
                        mFileManager.store(ostream.str());
                    }
                }
                forwardLogData(NFdbBase::NTF_TRACE_LOG, msg);
            }
            break;
            case NFdbBase::REQ_SET_TRACE_CONFIG:
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
                fdb_reverse_tag = in_config.reverse_tag();
                fdb_cache_size = in_config.cache_size();

                mLogCache.resize(fdb_cache_size * 1024);

                NFdbBase::FdbTraceConfig out_config;
                fillTraceConfigs(out_config);
                CFdbParcelableBuilder builder(out_config);
                broadcast(NFdbBase::NTF_TRACE_CONFIG, builder);
                msg->reply(msg_ref, builder);
            }
            break;
            case NFdbBase::REQ_GET_TRACE_CONFIG:
            {
                NFdbBase::FdbTraceConfig out_config;
                fillTraceConfigs(out_config);
                CFdbParcelableBuilder builder(out_config);
                msg->reply(msg_ref, builder);
            }
            break;
            case NFdbBase::REQ_LOG_START:
            {
                NFdbBase::FdbLogStart log_start;
                CFdbParcelableParser parser(log_start);
                if (!msg->deserialize(parser))
                {
                    LOG_E("CLogServer: Unable to deserialize message!\n");
                    return;
                }
                auto session = msg->getSession();

                subscribe(session, NFdbBase::NTF_FDBUS_LOG, objId(), 0, FDB_SUB_TYPE_NORMAL);
                onSubscribeFDBusLogger(msg->session());

                subscribe(session, NFdbBase::NTF_TRACE_LOG, objId(), 0, FDB_SUB_TYPE_NORMAL);
                onSubscribeTraceLogger(msg->session());

                auto cache_size = log_start.cache_size();
                if (cache_size > 0)
                {
                    cache_size *= 1024;
                }
                mLogCache.dump(this, session, cache_size);
            }
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
                    onSubscribeFDBusLogger(msg->session());
                }
                break;
                case NFdbBase::NTF_TRACE_LOG:
                {
                    onSubscribeTraceLogger(msg->session());
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
    CFdbLogCache mLogCache;
    CLogFileManager mFileManager;

    bool checkLogEnabled(bool global_disable, bool no_client_connected)
    {
        if (global_disable)
        {
            return false; // if log is disabled globally, don't generate log message
        }
        else if (mLogCache.size() ||            // log cache is enabled
                 !fdb_disable_std_output ||     // showing at stdout
                 !no_client_connected ||        // any log viewer is connected
                 mFileManager.logEnabled())     // logging to file is enabled
        {
            return true;
        }
        else
        {
            return false; // don't generat any log message
        }
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
        config.set_reverse_endpoint_name(fdb_reverse_endpoint_name);
        config.set_reverse_bus_name(fdb_reverse_bus_name);
    }

    void fillTraceConfigs(NFdbBase::FdbTraceConfig &config)
    {
        config.set_global_enable(checkLogEnabled(fdb_disable_global_trace,
                                                 mTraceClientTbl.empty()));
        config.set_log_level((EFdbLogLevel)fdb_debug_trace_level);
        config.set_cache_size(fdb_cache_size);
        fdb_populate_white_list_cmd(config.host_white_list(), fdb_trace_host_white_list);
        fdb_populate_white_list_cmd(config.tag_white_list(), fdb_trace_tag_white_list);
        config.set_reverse_tag(fdb_reverse_tag);
    }

    void onSubscribeFDBusLogger(FdbSessionId_t sid)
    {
        bool is_first = mLoggerClientTbl.empty();
        auto it = mLoggerClientTbl.find(sid);
        if (it == mLoggerClientTbl.end())
        {
            mLoggerClientTbl[sid] = new CLogClientCfg();
        }
        if (is_first)
        {
            NFdbBase::FdbMsgLogConfig cfg;
            fillLoggerConfigs(cfg);
            CFdbParcelableBuilder builder(cfg);
            broadcast(NFdbBase::NTF_LOGGER_CONFIG, builder);
        }
    }

    void onSubscribeTraceLogger(FdbSessionId_t sid)
    {
        bool is_first = mTraceClientTbl.empty();
        auto it = mTraceClientTbl.find(sid);
        if (it == mTraceClientTbl.end())
        {
            mTraceClientTbl[sid] = new CTraceClientCfg();
        }
        if (is_first)
        {
            NFdbBase::FdbTraceConfig cfg;
            fillTraceConfigs(cfg);
            CFdbParcelableBuilder builder(cfg);
            broadcast(NFdbBase::NTF_TRACE_CONFIG, builder);
        }
    }

    void forwardLogData(FdbEventCode_t code, CFdbMessage *msg)
    {
        auto payload = (uint8_t *)msg->getPayloadBuffer();
        int32_t size = msg->getPayloadSize();
        mLogCache.push(payload, size, code);

        if (((code == NFdbBase::NTF_FDBUS_LOG) && !mLoggerClientTbl.empty()) ||
            ((code == NFdbBase::NTF_TRACE_LOG) && !mTraceClientTbl.empty()))
        {
            broadcastLogNoQueue(code, payload, size, 0);
        }
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
    char *reverse_selections = 0;
    char *log_storage_param = 0;
    const struct fdb_option core_options[] = {
        { FDB_OPTION_BOOLEAN, "request", 'q', &fdb_disable_request },
        { FDB_OPTION_BOOLEAN, "reply", 'p', &fdb_disable_reply },
        { FDB_OPTION_BOOLEAN, "broadcast", 'b', &fdb_disable_broadcast },
        { FDB_OPTION_BOOLEAN, "subscribe", 's', &fdb_disable_subscribe },
        { FDB_OPTION_BOOLEAN, "no_fdbus", 'f', &fdb_disable_global_logger },
        { FDB_OPTION_BOOLEAN, "output", 'o', &fdb_disable_std_output },
        { FDB_OPTION_INTEGER, "clip", 'c', &fdb_raw_data_clipping_size },
        { FDB_OPTION_STRING, "log_endpoint", 'e', &log_endpoint_filters },
        { FDB_OPTION_STRING, "log_busname", 'n', &log_busname_filters },
        { FDB_OPTION_STRING, "log_hosts", 'm', &log_host_filters },
        { FDB_OPTION_INTEGER, "level", 'l', &fdb_debug_trace_level },
        { FDB_OPTION_BOOLEAN, "no_trace", 'd', &fdb_disable_global_trace },
        { FDB_OPTION_STRING, "trace_tags", 't', &trace_tag_filters },
        { FDB_OPTION_STRING, "trace_hosts", 'M', &trace_host_filters },
        { FDB_OPTION_STRING, "reverse_selection", 'r', &reverse_selections },
        { FDB_OPTION_INTEGER, "cache_size", 'g', &fdb_cache_size },
        { FDB_OPTION_STRING, "log_storage", 'j', &log_storage_param},
        { FDB_OPTION_BOOLEAN, "help", 'h', &help }
    };

    fdb_parse_options(core_options, ARRAY_LENGTH(core_options), &argc, argv);
    if (help)
    {
        std::cout << "FDBus - Fast Distributed Bus" << std::endl;
        std::cout << "    SDK version " << FDB_DEF_TO_STR(FDB_VERSION_MAJOR) "."
                                           FDB_DEF_TO_STR(FDB_VERSION_MINOR) "."
                                           FDB_DEF_TO_STR(FDB_VERSION_BUILD) << std::endl;
        std::cout << "    LIB version " << CFdbContext::getFdbLibVersion() << std::endl;
        std::cout << "Usage: logsvc[ -q][ -p][ -b][ -s][ -f][ -o][ -c clipping_size][ -r e[,n[,t]][ -e ep1,ep2...][ -m host1,host2...][ -l][ -d][ -t tag1,tag2][ -M host1,host2...][ -h]" << std::endl;
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
        std::cout << "    -g: specify size of log cache in kB; no log cache if 0 is given (default)" << std::endl;
        std::cout << "    -j: specify log storage in format: 'path,max_storage_size,max_file_size'; the size is in unit of MB. " << std::endl;
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

    if (log_storage_param)
    {
        uint32_t num_log_storage_param = 0;
        char **log_storage_param_array = strsplit(log_storage_param, ",", &num_log_storage_param);
        if (num_log_storage_param >= 1)
        {
            fdb_log_path = log_storage_param_array[0];
        }
        if (num_log_storage_param >= 2)
        {
            fdb_max_log_storage_size = atoi(log_storage_param_array[1]);
        }
        if (num_log_storage_param >= 3)
        {
            fdb_max_file_size = atoi(log_storage_param_array[2]);
        }
    }

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
    
    CLogServer log_server;
    log_server.bind();
    FDB_CONTEXT->start(FDB_WORKER_EXE_IN_PLACE);
    return 0;
}

