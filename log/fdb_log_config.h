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

#ifndef _FDB_LOG_CONFIG_H_
#define _FDB_LOG_CONFIG_H_

#include <common_base/CFdbSimpleSerializer.h>
#include <vector>
#include <string>

namespace NFdbBase
{
    class FdbMsgLogConfig;
    class FdbTraceConfig;
}

enum EFdbCfgMode
{
    CFG_MODE_INVALID = -1,
    CFG_MODE_NONE,
    CFG_MODE_INCREMENTAL,
    CFG_MODE_ABSOLUTE
};

struct CFdbLogParams
{
    int32_t fdb_disable_request;
    bool fdb_disable_request_configured;

    int32_t fdb_disable_reply;
    bool fdb_disable_reply_configured;

    int32_t fdb_disable_broadcast;
    bool fdb_disable_broadcast_configured;

    int32_t fdb_disable_subscribe;
    bool fdb_disable_subscribe_configured;

    int32_t fdb_disable_global_logger;
    bool fdb_disable_global_logger_configured;

    // 0: no raw data; < 0: all raw data; > 0: actrual size of raw data
    int32_t fdb_raw_data_clipping_size;
    bool fdb_raw_data_clipping_size_configured;

    int32_t fdb_debug_trace_level;
    bool fdb_debug_trace_level_configured;

    int32_t fdb_disable_global_trace;
    bool fdb_disable_global_trace_configured;

    int32_t fdb_cache_size; // initial cache size: 0kB
    bool fdb_cache_size_configured;

    std::vector<std::string> fdb_log_host_white_list;
    std::vector<std::string> fdb_log_endpoint_white_list;
    std::vector<std::string> fdb_log_busname_white_list;
    std::vector<std::string> fdb_trace_host_white_list;
    std::vector<std::string> fdb_trace_tag_white_list;

    int32_t fdb_reverse_endpoint_name;
    bool fdb_reverse_endpoint_name_configured;

    int32_t fdb_reverse_bus_name;
    bool fdb_reverse_bus_name_configured;

    int32_t fdb_reverse_tag;
    bool fdb_reverse_tag_configured;

    std::string fdb_log_path;
    bool fdb_log_path_configured;

    int32_t fdb_max_log_storage_size;
    bool fdb_max_log_storage_size_configured;

    int32_t fdb_max_log_file_size;
    bool fdb_max_log_file_size_configured;

    // for logclt
    int32_t fdb_disable_std_output;

    // for logviewer
    EFdbCfgMode fdb_config_mode;
    int32_t fdb_info_mode;
};

bool initLogConfig(int argc, char **argv, bool is_viewer);
void fdb_print_configuration();
void fdb_fill_logger_config(NFdbBase::FdbMsgLogConfig &config, bool global_enabled);
void fdb_fill_trace_config(NFdbBase::FdbTraceConfig &config, bool global_enabled);

void fdb_dump_logger_config(NFdbBase::FdbMsgLogConfig &config);
void fdb_dump_trace_config(NFdbBase::FdbTraceConfig &config);

bool fdb_update_logger_config(NFdbBase::FdbMsgLogConfig &config, bool global_enabled);
bool fdb_update_trace_config(NFdbBase::FdbTraceConfig &config, bool global_enabled);

extern CFdbLogParams gFdbLogConfig;

#endif

