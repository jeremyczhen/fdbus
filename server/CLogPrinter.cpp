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
#include <utils/Log.h>
#include <iostream>
#include <stdlib.h>
#include <idl-gen/common.base.MessageHeader.pb.h>

CLogPrinter::CLogPrinter(uint32_t mode)
    : mPrintMode(mode)
{
}

void CLogPrinter::outputFdbLog(const NFdbBase::FdbLogProducerData &log_info, CFdbMessage *log_msg)
{
    std::cout << "[F][" << log_info.logger_pid() << "@" << log_info.sender_host_name() << "]["
              << log_info.sender_name() << "->" << log_info.receiver_name() << "]["
              << log_info.service_name() << "]["
              << log_info.object_id() << "]["
              << CFdbMessage::getMsgTypeName(log_info.type()) << "]["
              << log_info.code() << "]["
              << log_info.serial_number() << "]["
              << log_info.msg_payload_size() << "]["
              << log_info.time_stamp() << "]{"
              << std::endl;
    if (log_info.is_string())
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

        std::cout << "Raw data of size " << log_info.msg_payload_size()
                  << " is clipped to "
                  << clipped_payload_size << " bytes"
                  << std::endl << "}" << std::endl;
        log->ownBuffer();
        delete log;
    }
}

void CLogPrinter::outputTraceLog(const NFdbBase::FdbTraceProducerData &trace_info, CFdbMessage *trace_msg)
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

    std::cout << "[D]" << level_name[trace_info.trace_level()] << "["
              << trace_info.tag() << "-" << trace_info.trace_pid() << "-" << trace_info.sender_host_name() << "]["
              << trace_info.time_stamp() << "] "
              << trace_msg->getExtraBuffer();
}
