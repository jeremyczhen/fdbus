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
    CBASE_tProcId pid;
    std::string host_name;
    std::string sender;
    std::string receiver;
    std::string busname;
    EFdbMessageType msg_type;
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
                 >> timestamp
                 >> payload_size
                 >> msg_sn
                 >> obj_id
                 >> is_string;

    std::cout << "[F][" << pid << "@" << host_name << "]["
              << sender << "->" << receiver << "]["
              << busname << "]["
              << obj_id << "]["
              << CFdbMessage::getMsgTypeName(msg_type) << "]["
              << msg_code << "]["
              << msg_sn << "]["
              << payload_size << "]["
              << timestamp << "]{"
              << std::endl;
    if (is_string)
    {
        std::cout << (char *)log_msg->getExtraBuffer() << "}"
                  << std::endl;
    }
    else
    {
        int32_t clipped_payload_size = 0;
        CFdbMessage *log = log_msg->parseFdbLog(log_msg->getExtraBuffer(), log_msg->getExtraDataSize());
        if (log)
        {
            clipped_payload_size = log->getPayloadSize();
        }
        else
        {
            clipped_payload_size = -1;
            LOG_E("CLogServer: Unable to decode log data!\n");
            return;
        }

        std::cout << "Raw data of size " << payload_size
                  << " is clipped to "
                  << clipped_payload_size << " bytes"
                  << std::endl << "}" << std::endl;
        log->ownBuffer();
        delete log;
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
    CBASE_tProcId pid;
    std::string tag;
    std::string host_name;
    uint64_t timestamp;
    EFdbLogLevel log_level;
    deserializer >> pid
                 >> tag
                 >> host_name
                 >> timestamp
                 >> log_level;
    if (log_level >= FDB_LL_MAX)
    {
        return;
    }

    std::cout << "[D]" << level_name[log_level] << "["
              << tag << "-" << pid << "-" << host_name << "]["
              << timestamp << "] "
              << trace_msg->getExtraBuffer();
}
