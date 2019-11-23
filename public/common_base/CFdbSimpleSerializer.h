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

#define FDB_SCRATCH_CACHE_SIZE 2048 
typedef uint16_t fdb_ser_strlen_t;

class CFdbSimpleSerializer
{
public:
    CFdbSimpleSerializer();
    ~CFdbSimpleSerializer();
    template<typename T>
    friend CFdbSimpleSerializer& operator<<(CFdbSimpleSerializer &serializer, const T& data)
    {
        serializer.addBasicType((const uint8_t *)&data, sizeof(T));
        return serializer;
    }

    friend CFdbSimpleSerializer& operator<<(CFdbSimpleSerializer &serializer, const std::string& data);
    friend CFdbSimpleSerializer& operator<<(CFdbSimpleSerializer &serializer, std::string& data);
    friend CFdbSimpleSerializer& operator<<(CFdbSimpleSerializer &serializer, const char *data);
    friend CFdbSimpleSerializer& operator<<(CFdbSimpleSerializer &serializer, char *data);

    int32_t toBuffer(uint8_t *buffer, int32_t size);
    int32_t bufferSize() const
    {
        return mPos;
    }
    void reset();
    void addRawData(const uint8_t *p_data, int32_t size);
    void addString(const char *string, fdb_ser_strlen_t str_len);

private:
    uint8_t *mBuffer;
    uint32_t mTotalSize;
    uint32_t mPos;
    uint8_t mScratchCache[FDB_SCRATCH_CACHE_SIZE];
    void addMemory(uint32_t size);
    void addBasicType(const uint8_t *p_data, int32_t size);
};

class CFdbSimpleDeserializer
{
public:
    CFdbSimpleDeserializer(const uint8_t *buffer = 0, int32_t size = 0);
    void reset(const uint8_t *buffer, int32_t size = 0);
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
        deserializer.retrieveBasicData((uint8_t *)&data, size);
        return deserializer;
    }

    friend CFdbSimpleDeserializer& operator>>(CFdbSimpleDeserializer &deserializer, std::string& data);

    bool error() const
    {
        return mError;
    }

    void error(bool status)
    {
        mError = status;
    }

    int32_t index() const
    {
        return mPos;
    }

    const uint8_t *pos() const
    {
        return mBuffer ? mBuffer + mPos : 0;
    }
    
    bool retrieveRawData(uint8_t *p_data, int32_t size);

private:
    const uint8_t *mBuffer;
    int32_t mSize;
    int32_t mPos;
    bool mError;

    void retrieveBasicData(uint8_t *p_data, int32_t size);
};

#endif

