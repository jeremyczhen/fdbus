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
#include <common_base/fdb_option_parser.h>
#include <common_base/CLogProducer.h>
#include <common_base/CFdbContext.h>
#include <iostream>

CFdbLogParams gFdbLogConfig;

#define fdb_init_conifg(_member, _invalid_value) do { \
    gFdbLogConfig._member = _invalid_value; \
    gFdbLogConfig._member##_configured = false; \
} while (0)

#define fdb_check_config_updated(_member, _init_value) do { \
    if (gFdbLogConfig._member >= 0) \
    { \
        gFdbLogConfig._member##_configured = true; \
    } \
    else \
    { \
        gFdbLogConfig._member = _init_value; \
    } \
} while (0)

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

void fdb_print_configuration()
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
    std::cout << "request:              " << (gFdbLogConfig.fdb_disable_request ? "disabled" : "enabled") << std::endl;
    std::cout << "reply:                " << (gFdbLogConfig.fdb_disable_reply ? "disabled" : "enabled") << std::endl;
    std::cout << "broadcast:            " << (gFdbLogConfig.fdb_disable_broadcast ? "disabled" : "enabled") << std::endl;
    std::cout << "subscribe:            " << (gFdbLogConfig.fdb_disable_subscribe ? "disabled" : "enabled") << std::endl;
    std::cout << "fdbus log:            " << (gFdbLogConfig.fdb_disable_global_logger ? "disabled" : "enabled") << std::endl;
    std::cout << "raw data clipping:    " << gFdbLogConfig.fdb_raw_data_clipping_size << std::endl;
    std::cout << "debug trace level:    " << level_name[gFdbLogConfig.fdb_debug_trace_level] << std::endl;
    std::cout << "debug trace Log:      " << (gFdbLogConfig.fdb_disable_global_trace ? "disabled" : "enabled") << std::endl;
    std::cout << "exclusive endpoints:  " << (gFdbLogConfig.fdb_reverse_endpoint_name ? "true" : "false") << std::endl;
    std::cout << "exclusive bus names:  " << (gFdbLogConfig.fdb_reverse_bus_name ? "true" : "false") << std::endl;
    std::cout << "exclusive tags:       " << (gFdbLogConfig.fdb_reverse_tag ? "true" : "false") << std::endl;
    std::cout << "log cache size:       " << gFdbLogConfig.fdb_cache_size << "kB" << std::endl;
    std::cout << "log file path:        " << gFdbLogConfig.fdb_log_path << std::endl;
    std::cout << "log storage capacity: " << gFdbLogConfig.fdb_max_log_storage_size << "kB" << std::endl;
    std::cout << "single log file size: " << gFdbLogConfig.fdb_max_log_file_size << "kB" << std::endl;

    std::cout << "fdbus log host white list:" << std::endl;
    fdb_print_whitelist(gFdbLogConfig.fdb_log_host_white_list);
    std::cout << "fdbus log endpoint white list:" << std::endl;
    fdb_print_whitelist(gFdbLogConfig.fdb_log_endpoint_white_list);
    std::cout << "fdbus log busname white list:" << std::endl;
    fdb_print_whitelist(gFdbLogConfig.fdb_log_busname_white_list);

    std::cout << "fdbus debug trace host white list:" << std::endl;
    fdb_print_whitelist(gFdbLogConfig.fdb_trace_host_white_list);
    std::cout << "fdbus debug trace tag white list:" << std::endl;
    fdb_print_whitelist(gFdbLogConfig.fdb_trace_tag_white_list);
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

static void fdb_dump_white_list_cmd(const CFdbParcelableArray<std::string> &in_filter
                                    , std::vector<std::string> &white_list)
{
    white_list.clear();
    for (auto it = in_filter.pool().begin(); it != in_filter.pool().end(); ++it)
    {
        white_list.push_back(*it);
    }
}

void fdb_fill_logger_config(NFdbBase::FdbMsgLogConfig &config, bool global_enabled)
{
    config.set_global_enable(global_enabled);
    config.set_enable_request(!gFdbLogConfig.fdb_disable_request);
    config.set_enable_reply(!gFdbLogConfig.fdb_disable_reply);
    config.set_enable_broadcast(!gFdbLogConfig.fdb_disable_broadcast);
    config.set_enable_subscribe(!gFdbLogConfig.fdb_disable_subscribe);
    config.set_raw_data_clipping_size(gFdbLogConfig.fdb_raw_data_clipping_size);
    fdb_populate_white_list_cmd(config.host_white_list(), gFdbLogConfig.fdb_log_host_white_list);
    fdb_populate_white_list_cmd(config.endpoint_white_list(), gFdbLogConfig.fdb_log_endpoint_white_list);
    fdb_populate_white_list_cmd(config.busname_white_list(), gFdbLogConfig.fdb_log_busname_white_list);
    config.set_reverse_endpoint_name(gFdbLogConfig.fdb_reverse_endpoint_name);
    config.set_reverse_bus_name(gFdbLogConfig.fdb_reverse_bus_name);
}

void fdb_fill_trace_config(NFdbBase::FdbTraceConfig &config, bool global_enabled)
{
    config.set_global_enable(global_enabled);
    config.set_log_level((EFdbLogLevel)gFdbLogConfig.fdb_debug_trace_level);
    config.set_cache_size(gFdbLogConfig.fdb_cache_size);
    fdb_populate_white_list_cmd(config.host_white_list(), gFdbLogConfig.fdb_trace_host_white_list);
    fdb_populate_white_list_cmd(config.tag_white_list(), gFdbLogConfig.fdb_trace_tag_white_list);
    config.set_reverse_tag(gFdbLogConfig.fdb_reverse_tag);
    config.set_log_path(gFdbLogConfig.fdb_log_path.c_str());
    config.set_max_log_storage_size(gFdbLogConfig.fdb_max_log_storage_size);
    config.set_max_log_file_size(gFdbLogConfig.fdb_max_log_file_size);
}

void fdb_dump_logger_config(NFdbBase::FdbMsgLogConfig &config)
{
    gFdbLogConfig.fdb_disable_global_logger = !config.global_enable();
    gFdbLogConfig.fdb_disable_request = !config.enable_request();
    gFdbLogConfig.fdb_disable_reply = !config.enable_reply();
    gFdbLogConfig.fdb_disable_broadcast = !config.enable_broadcast();
    gFdbLogConfig.fdb_disable_subscribe = !config.enable_subscribe();
    gFdbLogConfig.fdb_raw_data_clipping_size = config.raw_data_clipping_size();
    fdb_dump_white_list_cmd(config.host_white_list(), gFdbLogConfig.fdb_log_host_white_list);
    fdb_dump_white_list_cmd(config.endpoint_white_list(), gFdbLogConfig.fdb_log_endpoint_white_list);
    fdb_dump_white_list_cmd(config.busname_white_list(), gFdbLogConfig.fdb_log_busname_white_list);
    gFdbLogConfig.fdb_reverse_endpoint_name = config.reverse_endpoint_name();
    gFdbLogConfig.fdb_reverse_bus_name = config.reverse_bus_name();
}

void fdb_dump_trace_config(NFdbBase::FdbTraceConfig &config)
{
    gFdbLogConfig.fdb_disable_global_trace = !config.global_enable();
    gFdbLogConfig.fdb_debug_trace_level = config.log_level();
    fdb_dump_white_list_cmd(config.host_white_list(), gFdbLogConfig.fdb_trace_host_white_list);
    fdb_dump_white_list_cmd(config.tag_white_list(), gFdbLogConfig.fdb_trace_tag_white_list);
    gFdbLogConfig.fdb_reverse_tag = config.reverse_tag();
    gFdbLogConfig.fdb_cache_size = config.cache_size();
    gFdbLogConfig.fdb_log_path = config.log_path();
    gFdbLogConfig.fdb_max_log_storage_size = config.max_log_storage_size();
    gFdbLogConfig.fdb_max_log_file_size = config.max_log_file_size();
}

bool fdb_update_logger_config(NFdbBase::FdbMsgLogConfig &config, bool global_enabled)
{
    bool updated = false;
    if (gFdbLogConfig.fdb_disable_global_logger_configured)
    {
        config.set_global_enable(global_enabled);
        updated = true;
    }

    if (gFdbLogConfig.fdb_disable_request_configured)
    {
        config.set_enable_request(!gFdbLogConfig.fdb_disable_request);
        updated = true;
    }
    if (gFdbLogConfig.fdb_disable_reply_configured)
    {
        config.set_enable_reply(!gFdbLogConfig.fdb_disable_reply);
        updated = true;
    }
    if (gFdbLogConfig.fdb_disable_broadcast_configured)
    {
        config.set_enable_broadcast(!gFdbLogConfig.fdb_disable_broadcast);
        updated = true;
    }
    if (gFdbLogConfig.fdb_disable_subscribe_configured)
    {
        config.set_enable_subscribe(!gFdbLogConfig.fdb_disable_subscribe);
        updated = true;
    }
    if (gFdbLogConfig.fdb_raw_data_clipping_size_configured)
    {
        config.set_raw_data_clipping_size(gFdbLogConfig.fdb_raw_data_clipping_size);
        updated = true;
    }
    if (!gFdbLogConfig.fdb_log_host_white_list.empty())
    {
        config.host_white_list().vpool().clear();
        fdb_populate_white_list_cmd(config.host_white_list(), gFdbLogConfig.fdb_log_host_white_list);
        updated = true;
    }
    if (!gFdbLogConfig.fdb_log_endpoint_white_list.empty())
    {
        config.endpoint_white_list().vpool().clear();
        fdb_populate_white_list_cmd(config.endpoint_white_list(), gFdbLogConfig.fdb_log_endpoint_white_list);
        updated = true;
    }
    if (!gFdbLogConfig.fdb_log_busname_white_list.empty())
    {
        config.busname_white_list().vpool().clear();
        fdb_populate_white_list_cmd(config.busname_white_list(), gFdbLogConfig.fdb_log_busname_white_list);
        updated = true;
    }
    if (gFdbLogConfig.fdb_reverse_endpoint_name_configured)
    {
        config.set_reverse_endpoint_name(gFdbLogConfig.fdb_reverse_endpoint_name);
        updated = true;
    }
    if (gFdbLogConfig.fdb_reverse_bus_name_configured)
    {
        config.set_reverse_bus_name(gFdbLogConfig.fdb_reverse_bus_name);
        updated = true;
    }
    return updated;
}

bool fdb_update_trace_config(NFdbBase::FdbTraceConfig &config, bool global_enabled)
{
    bool updated = false;
    if (gFdbLogConfig.fdb_disable_global_trace_configured)
    {
        config.set_global_enable(global_enabled);
        updated = true;
    }
    if (gFdbLogConfig.fdb_debug_trace_level_configured)
    {
        config.set_log_level((EFdbLogLevel)gFdbLogConfig.fdb_debug_trace_level);
        updated = true;
    }
    if (gFdbLogConfig.fdb_cache_size_configured)
    {
        config.set_cache_size(gFdbLogConfig.fdb_cache_size);
        updated = true;
    }
    if (!gFdbLogConfig.fdb_trace_host_white_list.empty())
    {
        config.host_white_list().vpool().clear();
        fdb_populate_white_list_cmd(config.host_white_list(), gFdbLogConfig.fdb_trace_host_white_list);
        updated = true;
    }
    if (!gFdbLogConfig.fdb_trace_tag_white_list.empty())
    {
        config.tag_white_list().vpool().clear();
        fdb_populate_white_list_cmd(config.tag_white_list(), gFdbLogConfig.fdb_trace_tag_white_list);
        updated = true;
    }
    if (gFdbLogConfig.fdb_reverse_tag_configured)
    {
        config.set_reverse_tag(gFdbLogConfig.fdb_reverse_tag);
        updated = true;
    }
    if (gFdbLogConfig.fdb_log_path_configured)
    {
        config.set_log_path(gFdbLogConfig.fdb_log_path.c_str());
        updated = true;
    }
    if (gFdbLogConfig.fdb_max_log_storage_size_configured)
    {
        config.set_max_log_storage_size(gFdbLogConfig.fdb_max_log_storage_size);
        updated = true;
    }
    if (gFdbLogConfig.fdb_max_log_file_size_configured)
    {
        config.set_max_log_file_size(gFdbLogConfig.fdb_max_log_file_size);
        updated = true;
    }
    return updated;
}

void printHelp(bool is_viewer)
{
    std::cout << "FDBus - Fast Distributed Bus" << std::endl;
    std::cout << "    SDK version " << FDB_DEF_TO_STR(FDB_VERSION_MAJOR) "."
                                       FDB_DEF_TO_STR(FDB_VERSION_MINOR) "."
                                       FDB_DEF_TO_STR(FDB_VERSION_BUILD) << std::endl;
    std::cout << "    LIB version " << CFdbContext::getFdbLibVersion() << std::endl;
    if (is_viewer)
    {
        std::cout << "Usage: logviewer -x[-a]";
    }
    else
    {
        std::cout << "Usage: logsvc";
    }
    std::cout << "[ -q][ -p][ -b][ -s][ -f][ -c clipping_size][ -r e[,n[,t]][ -e ep1,ep2...][ -m host1,host2...][ -l][ -d][ -t tag1,tag2][ -M host1,host2...][ -h]" << std::endl;
    if (is_viewer)
    {
        std::cout << "           Configure log server or" << std::endl;
        std::cout << "       logviewer -i" << std::endl;
        std::cout << "           Retrieve config info from log server or" << std::endl;
        std::cout << "       logviewer" << std::endl;
        std::cout << "           Start log client." << std::endl;
    }
    else
    {
        std::cout << "Start log server." << std::endl;
    }
    std::cout << "    ==== Options for fdbus monitor ====" << std::endl;
    std::cout << "    -q: disable logging fdbus request" << std::endl;
    std::cout << "    -p: disable logging fdbus response" << std::endl;
    std::cout << "    -b: disable logging fdbus broadcast" << std::endl;
    std::cout << "    -s: disable logging fdbus subscribe" << std::endl;
    std::cout << "    -f: disable logging all fdbus message" << std::endl;
    if (!is_viewer)
    {
        std::cout << "    -o: disable terminal output" << std::endl;
    }
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
    std::cout << "    -j: specify log storage in format: 'path,max_storage_size,max_file_size'; the size is in unit of MB." << std::endl;
    if (is_viewer)
    {
        std::cout << "    -x: configure log server in incremental mode: only update the specified items." << std::endl;
        std::cout << "    -a: configure log server in absolute mode: override all configures with specified or default value in log viewer." << std::endl;
    }
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
}

bool initLogConfig(int argc, char **argv, bool is_viewer)
{
    fdb_init_conifg(fdb_disable_request, -1);
    fdb_init_conifg(fdb_disable_reply, -1);
    fdb_init_conifg(fdb_disable_broadcast, -1);
    fdb_init_conifg(fdb_disable_subscribe, -1);
    fdb_init_conifg(fdb_disable_global_logger, -1);
    fdb_init_conifg(fdb_raw_data_clipping_size, -1);
    fdb_init_conifg(fdb_debug_trace_level, -1);
    fdb_init_conifg(fdb_disable_global_trace, -1);
    fdb_init_conifg(fdb_cache_size, -1);
    fdb_init_conifg(fdb_reverse_endpoint_name, false);
    fdb_init_conifg(fdb_reverse_bus_name, false);
    fdb_init_conifg(fdb_reverse_tag, false);
    fdb_init_conifg(fdb_log_path, "");
    fdb_init_conifg(fdb_max_log_storage_size, -1);
    fdb_init_conifg(fdb_max_log_file_size, -1);


    // for logsvc
    gFdbLogConfig.fdb_disable_std_output = 0;

    // for logviewer
    gFdbLogConfig.fdb_config_mode = CFG_MODE_NONE;
    gFdbLogConfig.fdb_info_mode = 0;

    int32_t help = 0;
    char *log_host_filters = 0;
    char *log_endpoint_filters = 0;
    char *log_busname_filters = 0;
    char *trace_host_filters = 0;
    char *trace_tag_filters = 0;
    char *reverse_selections = 0;
    char *log_storage_param = 0;
    int32_t increment_config_mode = 0;
    int32_t absolute_config_mode = 0;
    const struct fdb_option core_options[] = {
        { FDB_OPTION_BOOLEAN, "request", 'q', &gFdbLogConfig.fdb_disable_request },
        { FDB_OPTION_BOOLEAN, "reply", 'p', &gFdbLogConfig.fdb_disable_reply },
        { FDB_OPTION_BOOLEAN, "broadcast", 'b', &gFdbLogConfig.fdb_disable_broadcast },
        { FDB_OPTION_BOOLEAN, "subscribe", 's', &gFdbLogConfig.fdb_disable_subscribe },
        { FDB_OPTION_BOOLEAN, "no_fdbus", 'f', &gFdbLogConfig.fdb_disable_global_logger },
        { FDB_OPTION_INTEGER, "clip", 'c', &gFdbLogConfig.fdb_raw_data_clipping_size },
        { FDB_OPTION_STRING, "log_endpoint", 'e', &log_endpoint_filters },
        { FDB_OPTION_STRING, "log_busname", 'n', &log_busname_filters },
        { FDB_OPTION_STRING, "log_hosts", 'm', &log_host_filters },
        { FDB_OPTION_INTEGER, "level", 'l', &gFdbLogConfig.fdb_debug_trace_level },
        { FDB_OPTION_BOOLEAN, "no_trace", 'd', &gFdbLogConfig.fdb_disable_global_trace },
        { FDB_OPTION_STRING, "trace_tags", 't', &trace_tag_filters },
        { FDB_OPTION_STRING, "trace_hosts", 'M', &trace_host_filters },
        { FDB_OPTION_STRING, "reverse_selection", 'r', &reverse_selections },
        { FDB_OPTION_INTEGER, "initial_cache_size", 'g', &gFdbLogConfig.fdb_cache_size },
        { FDB_OPTION_STRING, "log_storage", 'j', &log_storage_param},

        // for logsvc
        { FDB_OPTION_BOOLEAN, "output", 'o', &gFdbLogConfig.fdb_disable_std_output },

        // for logviewer
        { FDB_OPTION_BOOLEAN, "inc_cfg_mode", 'x', &increment_config_mode },
        { FDB_OPTION_BOOLEAN, "abs_cfg_mode", 'a', &absolute_config_mode },
        { FDB_OPTION_BOOLEAN, "info_mode", 'i', &gFdbLogConfig.fdb_info_mode },

        { FDB_OPTION_BOOLEAN, "help", 'h', &help }
    };

    fdb_parse_options(core_options, ARRAY_LENGTH(core_options), &argc, argv);
    if (help)
    {
        printHelp(is_viewer);
        return false;
    }

    if (increment_config_mode)
    {
        gFdbLogConfig.fdb_config_mode = CFG_MODE_INCREMENTAL;
    }
    else if (absolute_config_mode)
    {
        gFdbLogConfig.fdb_config_mode = CFG_MODE_ABSOLUTE;
    }

    if (gFdbLogConfig.fdb_debug_trace_level >= FDB_LL_MAX)
    {
        gFdbLogConfig.fdb_debug_trace_level = FDB_LL_SILENT;
    }
    
    fdb_populate_white_list(log_host_filters, gFdbLogConfig.fdb_log_host_white_list);
    fdb_populate_white_list(log_endpoint_filters, gFdbLogConfig.fdb_log_endpoint_white_list);
    fdb_populate_white_list(log_busname_filters, gFdbLogConfig.fdb_log_busname_white_list);
    fdb_populate_white_list(trace_host_filters, gFdbLogConfig.fdb_trace_host_white_list);
    fdb_populate_white_list(trace_tag_filters, gFdbLogConfig.fdb_trace_tag_white_list);

    
    if (log_storage_param)
    {
        uint32_t num_log_storage_param = 0;
        char **log_storage_param_array = strsplit(log_storage_param, ",", &num_log_storage_param);
        if (num_log_storage_param >= 1)
        {
            gFdbLogConfig.fdb_log_path = log_storage_param_array[0];
            gFdbLogConfig.fdb_log_path_configured = true;
        }
        if (num_log_storage_param >= 2)
        {
            gFdbLogConfig.fdb_max_log_storage_size = atoi(log_storage_param_array[1]);
        }
        if (num_log_storage_param >= 3)
        {
            gFdbLogConfig.fdb_max_log_file_size = atoi(log_storage_param_array[2]);
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
                gFdbLogConfig.fdb_reverse_endpoint_name = true;
                gFdbLogConfig.fdb_reverse_endpoint_name_configured = true;
            }
            else if (reverse_array[i][0] == 'n')
            {
                gFdbLogConfig.fdb_reverse_bus_name = true;
                gFdbLogConfig.fdb_reverse_bus_name_configured = true;
            }
            else if (reverse_array[i][0] == 't')
            {
                gFdbLogConfig.fdb_reverse_tag = true;
                gFdbLogConfig.fdb_reverse_tag_configured = true;
            }
        }
    }

    fdb_check_config_updated(fdb_disable_request, 0);
    fdb_check_config_updated(fdb_disable_reply, 0);
    fdb_check_config_updated(fdb_disable_broadcast, 0);
    fdb_check_config_updated(fdb_disable_subscribe, 0);
    fdb_check_config_updated(fdb_disable_global_logger, 0);
    fdb_check_config_updated(fdb_raw_data_clipping_size, 0);
    fdb_check_config_updated(fdb_debug_trace_level, (int32_t)FDB_LL_INFO);
    fdb_check_config_updated(fdb_disable_global_trace, 0);
    fdb_check_config_updated(fdb_cache_size, 0);
    fdb_check_config_updated(fdb_max_log_storage_size, 0);
    fdb_check_config_updated(fdb_max_log_file_size, 0);

    return true;
}

