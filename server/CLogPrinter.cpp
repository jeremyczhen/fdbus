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

#include "CLogPrinter.h"
#include <common_base/CNanoTimer.h>
#include <common_base/CBaseSysDep.h>
#include <common_base/CFdbSimpleMsgBuilder.h>
#include <utils/Log.h>
#include <iostream>
#include <stdlib.h>
#include <string>

CLogPrinter::CLogPrinter(uint32_t mode)
    : mPrintMode(mode)
{
}

void CLogPrinter::outputFdbLog(CFdbSimpleDeserializer &deserializer, CFdbMessage *log_msg)
{
    uint32_t pid;
    std::string host_name;
    std::string sender;
    std::string receiver;
    std::string busname;
    std::string topic;
    uint8_t msg_type;
    FdbMsgCode_t msg_code;
    uint64_t timestamp;
    int32_t payload_size;
    FdbMsgSn_t msg_sn;
    FdbObjectId_t obj_id;
    bool is_string;
    deserializer >> pid
                 >> host_name
                 >> sender
                 >> receiver
                 >> busname
                 >> msg_type
                 >> msg_code
                 >> topic
                 >> timestamp
                 >> payload_size
                 >> msg_sn
                 >> obj_id
                 >> is_string;

    std::cout << "[F][" << pid << "@" << host_name << "]["
              << sender << "->" << receiver << "]["
              << busname << "]["
              << obj_id << "]["
              << CFdbMessage::getMsgTypeName((EFdbMessageType)msg_type) << "]["
              << msg_code << "]["
              << topic << "]["
              << msg_sn << "]["
              << payload_size << "]["
              << timestamp << "]{"
              << std::endl;
    if (deserializer.error())
    {
        return;
    }
    if (is_string)
    {
        fdb_string_len_t str_len = 0;
        deserializer >> str_len;
        if (str_len)
        {
            std::cout << (const char *)deserializer.pos() << "}" << std::endl;
        }
    }
    else
    {
        int32_t log_size;
        deserializer >> log_size;
        std::cout << "No log! Data size: " << log_size << std::endl << "}" << std::endl;
    }
}

void CLogPrinter::outputTraceLog(CFdbSimpleDeserializer &deserializer, CFdbMessage *trace_msg)
{
    const char *level_name[] = {
        "[V]",
        "[D]",
        "[I]",
        "[W]",
        "[E]",
        "[F]",
        "[S]"
    };
    uint32_t pid;
    std::string tag;
    std::string host_name;
    uint64_t timestamp;
    uint8_t log_level;
    deserializer >> pid
                 >> tag
                 >> host_name
                 >> timestamp
                 >> log_level;
    if (log_level >= FDB_LL_MAX)
    {
        return;
    }

    fdb_string_len_t str_len = 0;
    deserializer >> str_len;
    const char *trace_log = str_len ? (const char *)deserializer.pos() : "\n";

    std::cout << "[D]" << level_name[log_level] << "["
              << tag << "-" << pid << "-" << host_name << "]["
              << timestamp << "] "
              << trace_log;
}
