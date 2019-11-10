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

#ifndef __CSIMPLESERIALIZER_H__
#define __CSIMPLESERIALIZER_H__
#include <string.h>
#include <vector>
#include <string>

class CFdbSimpleSerializer
{
public:
    template<typename T>
    friend CFdbSimpleSerializer& operator<<(CFdbSimpleSerializer &serializer, const T& data)
    {
        const uint8_t *p_data = (const uint8_t *)&data;
        serializer.mBuffer.insert(serializer.mBuffer.end(), p_data, p_data + sizeof(T));
        return serializer;
    }

    friend CFdbSimpleSerializer& operator<<(CFdbSimpleSerializer &serializer, const std::string& data)
    {
        serializeString(serializer, data.c_str(), (int32_t)data.size());
        return serializer;
    }
    friend CFdbSimpleSerializer& operator<<(CFdbSimpleSerializer &serializer, std::string& data)
    {
        return operator<<(serializer, (const std::string&)data);
    }

    friend CFdbSimpleSerializer& operator<<(CFdbSimpleSerializer &serializer, const char *data)
    {
        serializeString(serializer, data, (int32_t)strlen(data));
        return serializer;
    }
    friend CFdbSimpleSerializer& operator<<(CFdbSimpleSerializer &serializer, char *data)
    {
        return operator<<(serializer, (const uint8_t *)data);
    }

    int32_t toBuffer(uint8_t *buffer, int32_t size)
    {
        if (size > (int32_t)mBuffer.size())
        {
            size = (int32_t)mBuffer.size();
        }
        
        memcpy(buffer, mBuffer.data(), size);
        return size;
    }
    int32_t bufferSize() const
    {
        return (int32_t)mBuffer.size();
    }

    void reset()
    {
        mBuffer.clear();
    }
    
private:
    std::vector<uint8_t> mBuffer;
    friend void serializeString(CFdbSimpleSerializer &serializer, const char *string, int32_t len)
    {
        serializer.mBuffer.push_back(len + 1);
        serializer.mBuffer.insert(serializer.mBuffer.end(), string, string + len);
        serializer.mBuffer.push_back('\0');
    }
};

class CFdbSimpleDeserializer
{
public:
    CFdbSimpleDeserializer(const uint8_t *buffer = 0, int32_t size = 0)
        : mBuffer(buffer)
        , mSize(size)
        , mPos(0)
        , mError(false)
    {
    }
    void reset(const uint8_t *buffer, int32_t size = 0)
    {
        mBuffer = buffer;
        mSize = size;
        mPos = 0;
        mError = false;
    }
    template<typename T>
    friend CFdbSimpleDeserializer& operator>>(CFdbSimpleDeserializer& deserializer, T& data)
    {
        if (deserializer.mError)
        {
            return deserializer;
        }
        
        int32_t len = sizeof(T);
        if (deserializer.mSize && ((deserializer.mPos + len) > deserializer.mSize))
        {
            deserializer.mError = true;
            return deserializer;
        }

        uint8_t *p_data = (uint8_t *)&data;
        memcpy(p_data, deserializer.mBuffer + deserializer.mPos, len);
        deserializer.mPos += len;
        return deserializer;
    }

    friend CFdbSimpleDeserializer& operator>>(CFdbSimpleDeserializer &deserializer, std::string& data)
    {
        if (deserializer.mError)
        {
            return deserializer;
        }
        
        int32_t len = deserializer.mBuffer[deserializer.mPos];
        deserializer.mPos++;
        
        if (deserializer.mSize && ((deserializer.mPos + len) > deserializer.mSize))
        {
            deserializer.mError = true;
            return deserializer;
        }

        if (!len)
        {
            return deserializer;
        }

        if (deserializer.mBuffer[deserializer.mPos + len - 1] != '\0')
        {
            deserializer.mError = true;
            return deserializer;
        }
        data.assign((char *)(deserializer.mBuffer + deserializer.mPos));
        deserializer.mPos += len;
        return deserializer;
    }

    bool error() const
    {
        return mError;
    }

    void error(bool status)
    {
        mError = status;
    }

    int32_t size() const
    {
        return mPos;
    }

private:
    const uint8_t *mBuffer;
    int32_t mSize;
    int32_t mPos;
    bool mError;
};

#endif

