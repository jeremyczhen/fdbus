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

#include "CFdbProtoMsgBuilder.h"
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/text_format.h>
#define FDB_LOG_TAG "FDB_TEST_CLIENT"
#include <common_base/fdb_log_trace.h>

CFdbProtoMsgBuilder::CFdbProtoMsgBuilder(const CFdbProtoMessage &message)
    : mMessage(message)
{
}

int32_t CFdbProtoMsgBuilder::build()
{
    return mMessage.ByteSize();
}

void CFdbProtoMsgBuilder::toBuffer(uint8_t *buffer, int32_t size)
{
    try
    {
        google::protobuf::io::ArrayOutputStream aos(buffer, size);
        google::protobuf::io::CodedOutputStream coded_output(&aos);
        if (!mMessage.SerializeToCodedStream(&coded_output))
        {
            FDB_LOG_E("CFdbProtoMsgBuilder: Unable to serialize message!\n");
        }
    }
    catch (...)
    {
        FDB_LOG_E("CFdbProtoMsgBuilder: Unable to serialize message!\n");
    }
}

bool CFdbProtoMsgBuilder::toString(std::string *msg_txt)
{
    bool ret = true;
    try
    {
        const ::google::protobuf::Message &full_msg =
                    dynamic_cast<const ::google::protobuf::Message &>(mMessage);
        try
        {
            if (!google::protobuf::TextFormat::PrintToString(full_msg, msg_txt))
            {
                ret = false;
            }
        }
        catch (...)
        {
            ret = false;
        }
    }
    catch (std::bad_cast exp)
    {
        msg_txt->assign("Lite version does not support logging. You can remove \"option optimize_for = LITE_RUNTIME;\" from .proto file.\n");
    }

    return ret;
}

CFdbProtoMsgParser::CFdbProtoMsgParser(CFdbProtoMessage &message)
    : mMessage(message)
{
}

bool CFdbProtoMsgParser::parse(const uint8_t *buffer, int32_t size)
{
    bool ret;
    try
    {
        //Assign ArrayInputStream with enough memory
        google::protobuf::io::ArrayInputStream ais(buffer, size);
        google::protobuf::io::CodedInputStream coded_input(&ais);
        google::protobuf::io::CodedInputStream::Limit msgLimit = coded_input.PushLimit(size);
        // De-Serialize
        ret = mMessage.ParseFromCodedStream(&coded_input);
        //Once the embedded message has been parsed, PopLimit() is called to undo the limit
        coded_input.PopLimit(msgLimit);
    }
    catch (...)
    {
        ret = false;
        FDB_LOG_E("CFdbProtoMsgParser: Unable to deserialize message!\n");
    }
    
    return ret;
}

