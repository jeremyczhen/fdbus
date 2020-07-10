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

#ifndef __IFDBPROTOMSGBUILDER_H__ 
#define __IFDBPROTOMSGBUILDER_H__

#include <common_base/IFdbMsgBuilder.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/text_format.h>

namespace google{namespace protobuf {class MessageLite;}}
typedef ::google::protobuf::MessageLite CFdbProtoMessage;

class CFdbProtoMsgBuilder : public IFdbMsgBuilder
{
public:
    CFdbProtoMsgBuilder(const CFdbProtoMessage &message)
        : mMessage(message)
    {
    }

    int32_t build()
    {
        return mMessage.ByteSize();
    }

    bool toBuffer(uint8_t *buffer, int32_t size)
    {
        try
        {
            google::protobuf::io::ArrayOutputStream aos(buffer, size);
            google::protobuf::io::CodedOutputStream coded_output(&aos);
            if (!mMessage.SerializeToCodedStream(&coded_output))
            {
                return false;
            }
        }
        catch (...)
        {
            return false;
        }

        return true;
    }

    bool toString(std::string &msg_txt) const
    {
#if defined(__cpp_rtti) || defined(FDBUS_PROTO_FULL_FEATURE)
        bool ret = true;
        try
        {
#if defined(__cpp_rtti)
            const auto &full_msg = dynamic_cast<const ::google::protobuf::Message &>(mMessage);
#elif defined(FDBUS_PROTO_FULL_FEATURE)
            const auto &full_msg = static_cast<const ::google::protobuf::Message &>(mMessage);
#endif
            try
            {
                if (!google::protobuf::TextFormat::PrintToString(full_msg, &msg_txt))
                {
                    ret = false;
                }
            }
            catch (...)
            {
                ret = false;
            }
        }
        catch (...)
        {
            msg_txt.assign("Lite version does not support logging. You can remove \"option optimize_for = LITE_RUNTIME;\" from .proto file.\n");
        }

        return ret;
#else
        return false;
#endif
    }
    
private:
    const CFdbProtoMessage &mMessage;
};

class CFdbProtoMsgParser : public IFdbMsgParser
{
public:
    CFdbProtoMsgParser(CFdbProtoMessage &message)
        : mMessage(message)
    {
    }

    bool parse(const uint8_t *buffer, int32_t size)
    {
        bool ret = true;
        try
        {
            //Assign ArrayInputStream with enough memory
            google::protobuf::io::ArrayInputStream ais(buffer, size);
            google::protobuf::io::CodedInputStream coded_input(&ais);
            auto msgLimit = coded_input.PushLimit(size);
            // De-Serialize
            ret = mMessage.ParseFromCodedStream(&coded_input);
            //Once the embedded message has been parsed, PopLimit() is called to undo the limit
            coded_input.PopLimit(msgLimit);
        }
        catch (...)
        {
            ret = false;
        }
 
        return ret;
    }

private:
    CFdbProtoMessage &mMessage;
};

#endif
