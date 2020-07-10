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

#ifndef __IFDBSIMPLEMSGBUILDER_H__ 
#define __IFDBSIMPLEMSGBUILDER_H__

#include <string>
#include "IFdbMsgBuilder.h"
#include "CFdbSimpleSerializer.h"

template <typename T>
class CFdbBaseSimpleMsgBuilder : public IFdbMsgBuilder
{
public:
    CFdbBaseSimpleMsgBuilder(T message)
        : mMessage(message)
    {}
    int32_t build()
    {
        mSerializer << mMessage;
        return mSerializer.bufferSize();
    }
    bool toBuffer(uint8_t *buffer, int32_t size)
    {
        mSerializer.toBuffer(buffer, size);
        return true;
    }
    CFdbSimpleSerializer &serializer()
    {
        return mSerializer;
    }
    
protected:
    CFdbSimpleSerializer mSerializer;
    T mMessage;
};

template <typename T>
class CFdbSimpleMsgBuilder : public CFdbBaseSimpleMsgBuilder<T> 
{
public:
    CFdbSimpleMsgBuilder(T message)
        : CFdbBaseSimpleMsgBuilder<T>(message)
    {}
    bool toString(std::string &msg_txt) const
    {
        std::ostringstream stream;
        (void)CFdbBaseSimpleMsgBuilder<T>::mMessage.format(stream);
        msg_txt.assign(stream.str());
        return true;
    }
};

typedef CFdbSimpleMsgBuilder<const IFdbParcelable &> CFdbParcelableBuilder;

#define CFDBSIMPLEMSGBUILDER(_T) \
template <> \
class CFdbSimpleMsgBuilder<_T> : public CFdbBaseSimpleMsgBuilder<_T> \
{ \
public: \
    CFdbSimpleMsgBuilder(_T message) \
        : CFdbBaseSimpleMsgBuilder<_T>(message) \
    {} \
    bool toString(std::string &msg_txt) const \
    { \
        std::ostringstream stream; \
        stream << CFdbBaseSimpleMsgBuilder<_T>::mMessage; \
        msg_txt.assign(stream.str()); \
        return true; \
    } \
};

CFDBSIMPLEMSGBUILDER(int8_t)
CFDBSIMPLEMSGBUILDER(uint8_t)
CFDBSIMPLEMSGBUILDER(int16_t)
CFDBSIMPLEMSGBUILDER(uint16_t)
CFDBSIMPLEMSGBUILDER(int32_t)
CFDBSIMPLEMSGBUILDER(uint32_t)
CFDBSIMPLEMSGBUILDER(int64_t);
CFDBSIMPLEMSGBUILDER(uint64_t);
CFDBSIMPLEMSGBUILDER(std::string);

template <typename T>
class CFdbSimpleMsgParser : public IFdbMsgParser
{
public:
    CFdbSimpleMsgParser(T message)
        : mMessage(message)
    {}
    bool parse(const uint8_t *buffer, int32_t size)
    {
        mDeserializer.reset(buffer, size);
        mDeserializer >> mMessage;
        return !mDeserializer.error();
    }
    CFdbSimpleDeserializer &deserializer()
    {
        return mDeserializer;
    };

protected:
    CFdbSimpleDeserializer mDeserializer;
    T mMessage;
};

typedef CFdbSimpleMsgParser<IFdbParcelable &> CFdbParcelableParser;

#endif
