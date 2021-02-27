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

#ifndef __IFDBRAWMSGBUILDER_H__ 
#define __IFDBRAWMSGBUILDER_H__

#include "IFdbMsgBuilder.h"
#include "CFdbSimpleSerializer.h"

class CFdbRawMsgBuilder : public IFdbMsgBuilder
{
public:
    int32_t build()
    {
        return mSerializer.bufferSize();
    }
    const uint8_t *buffer()
    {
        return mSerializer.buffer();
    }
    int32_t bufferSize()
    {
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
};

class CFdbRawMsgParser : public IFdbMsgParser
{
public:
    CFdbRawMsgParser(const uint8_t *buffer = 0,
                        int32_t size = 0)
        : mDeserializer(buffer, size)
    {}
    bool parse(const uint8_t *buffer, int32_t size)
    {
        mDeserializer.reset(buffer, size);
        return true;
    }
    CFdbSimpleDeserializer &deserializer()
    {
        return mDeserializer;
    };

protected:
    CFdbSimpleDeserializer mDeserializer;
};

#endif
