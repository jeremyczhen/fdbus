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
#include <stdlib.h>
#include <vector>
#include <string>

static const uint16_t fdb_endian_check_word = 0xaa55;
static const uint8_t *fdb_endian_check_ptr = (uint8_t *)&fdb_endian_check_word;
#define fdb_is_little_endian() (*fdb_endian_check_ptr == 0x55)
#define FDB_SER_BUFFER_BLOCK_SIZE 512
typedef uint16_t fdb_ser_strlen_t;

class CFdbSimpleSerializer
{
public:
    CFdbSimpleSerializer()
        : mBuffer(0)
        , mTotalSize(0)
        , mPos(0)
    {
    }
    template<typename T>
    friend CFdbSimpleSerializer& operator<<(CFdbSimpleSerializer &serializer, const T& data)
    {
        serializer.addBasicType((const uint8_t *)&data, sizeof(T));
        return serializer;
    }

    friend CFdbSimpleSerializer& operator<<(CFdbSimpleSerializer &serializer, const std::string& data)
    {
        serializer.serializeString(data.c_str(), (fdb_ser_strlen_t)data.size());
        return serializer;
    }
    friend CFdbSimpleSerializer& operator<<(CFdbSimpleSerializer &serializer, std::string& data)
    {
        return serializer << (const std::string&)data;
    }

    friend CFdbSimpleSerializer& operator<<(CFdbSimpleSerializer &serializer, const char *data)
    {
        serializer.serializeString(data, (fdb_ser_strlen_t)strlen(data));
        return serializer;
    }
    friend CFdbSimpleSerializer& operator<<(CFdbSimpleSerializer &serializer, char *data)
    {
        return serializer << (const char *)data;
    }

    int32_t toBuffer(uint8_t *buffer, int32_t size)
    {
        if (size > (int32_t)mPos)
        {
            size = mPos;
        }
        
        memcpy(buffer, mBuffer, size);
        return size;
    }
    int32_t bufferSize() const
    {
        return mPos;
    }

    void reset()
    {
        mTotalSize = 0;
        mPos = 0;
    }
    
private:
    uint8_t *mBuffer;
    uint32_t mTotalSize;
    uint32_t mPos;
    void serializeString(const char *string, fdb_ser_strlen_t str_len)
    {
        fdb_ser_strlen_t l = str_len + 1;
        *this << l;
        addRawData((const uint8_t *)string, l);
    }
    void addMemory(uint32_t size)
    {
        uint32_t old_size = mTotalSize;
        if ((mPos + size) > mTotalSize)
        {
            if (size < FDB_SER_BUFFER_BLOCK_SIZE)
            {
                mTotalSize += FDB_SER_BUFFER_BLOCK_SIZE;
            }
            else
            {
                mTotalSize += size + FDB_SER_BUFFER_BLOCK_SIZE;
            }
        }
        if (mTotalSize != old_size)
        {
            mBuffer = (uint8_t*)realloc(mBuffer, mTotalSize);
        }
    }
    void addBasicType(const uint8_t *p_data, int32_t size)
    {
        addMemory(size);
        if (fdb_is_little_endian())
        {
            for (int32_t i = 0; i < size; ++i)
            {
                mBuffer[mPos + i] = p_data[i];
            }
        }
        else
        {
            for (int32_t i = 0; i < size; ++i)
            {
                mBuffer[mPos + i] = p_data[size - 1 - i];
            }
        }
        mPos += size;
    }
    void addRawData(const uint8_t *p_data, int32_t size)
    {
        addMemory(size);
        for (int32_t i = 0; i < size; ++i)
        {
            mBuffer[mPos + i] = p_data[i];
        }
        mPos += size;
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
        
        int32_t size = (int32_t)sizeof(T);
        if (deserializer.mSize && ((deserializer.mPos + size) > deserializer.mSize))
        {
            deserializer.mError = true;
            return deserializer;
        }

        uint8_t *p_data = (uint8_t *)&data;
        if (fdb_is_little_endian())
        {
            for (int32_t i = 0; i < size; ++i)
            {
                p_data[i] = deserializer.mBuffer[deserializer.mPos + i];
            }
        }
        else
        {
            for (int32_t i = 0; i < size; ++i)
            {
                p_data[i] = deserializer.mBuffer[deserializer.mPos + size - 1 - i];
            }
        }
        deserializer.mPos += size;
        return deserializer;
    }

    friend CFdbSimpleDeserializer& operator>>(CFdbSimpleDeserializer &deserializer, std::string& data)
    {
        fdb_ser_strlen_t len = 0;
        deserializer >> len;
        if (deserializer.mError || !len)
        {
            return deserializer;
        }
        
        if (deserializer.mSize && ((deserializer.mPos + len) > deserializer.mSize))
        {
            deserializer.mError = true;
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

