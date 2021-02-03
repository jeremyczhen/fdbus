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

#include <common_base/CFdbSimpleSerializer.h>

#define FDB_SER_BUFFER_BLOCK_SIZE 1024
static const uint16_t fdb_endian_check_word = 0xaa55;
static const uint8_t *fdb_endian_check_ptr = (uint8_t *)&fdb_endian_check_word;
#define fdb_is_little_endian() (*fdb_endian_check_ptr == 0x55)

CFdbSimpleSerializer::CFdbSimpleSerializer()
    : mBuffer(mScratchCache)
    , mTotalSize(FDB_SCRATCH_CACHE_SIZE)
    , mPos(0)
{
}

CFdbSimpleSerializer::~CFdbSimpleSerializer()
{
    reset();
}

CFdbSimpleSerializer& operator<<(CFdbSimpleSerializer &serializer, const std::string& data)
{
    serializer.addString(data.c_str(), (fdb_string_len_t)data.size());
    return serializer;
}

CFdbSimpleSerializer& operator<<(CFdbSimpleSerializer &serializer, const char *data)
{
    serializer.addString(data, (fdb_string_len_t)strlen(data));
    return serializer;
}

CFdbSimpleSerializer& operator<<(CFdbSimpleSerializer &serializer, const IFdbParcelable &data)
{
    data.serialize(serializer);
    return serializer;
}

CFdbSimpleSerializer& operator<<(CFdbSimpleSerializer &serializer, const IFdbParcelable *data)
{
    data->serialize(serializer);
    return serializer;
}

int32_t CFdbSimpleSerializer::toBuffer(uint8_t *buffer, int32_t size)
{
    if (size > (int32_t)mPos)
    {
        size = mPos;
    }
    
    memcpy(buffer, mBuffer, size);
    return size;
}

void CFdbSimpleSerializer::reset()
{
    mTotalSize = FDB_SCRATCH_CACHE_SIZE;
    mPos = 0;
    if (mBuffer && (mBuffer != mScratchCache))
    {
        free(mBuffer);
        mBuffer = mScratchCache;
    }
}

void CFdbSimpleSerializer::addString(const char *string, fdb_string_len_t str_len)
{
    fdb_string_len_t l = str_len + 1;
    *this << l;
    addRawData((const uint8_t *)string, l);
}

void CFdbSimpleSerializer::addMemory(uint32_t size)
{
    uint32_t new_pos = mPos + size;
    if (mBuffer == mScratchCache)
    {
        if (new_pos > FDB_SCRATCH_CACHE_SIZE)
        {
            mTotalSize += size + FDB_SER_BUFFER_BLOCK_SIZE;
            mBuffer = (uint8_t *)malloc(mTotalSize);
            memcpy(mBuffer, mScratchCache, mPos);
        }
        return;
    }

    uint32_t old_total = mTotalSize;
    if (new_pos > mTotalSize)
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
    if (mTotalSize > old_total)
    {
        mBuffer = (uint8_t*)realloc(mBuffer, mTotalSize);
    }
}

void CFdbSimpleSerializer::addBasicType(const uint8_t *p_data, int32_t size)
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

void CFdbSimpleSerializer::addRawData(const uint8_t *p_data, int32_t size)
{
    addMemory(size);
    for (int32_t i = 0; i < size; ++i)
    {
        mBuffer[mPos + i] = p_data[i];
    }
    mPos += size;
}

CFdbSimpleDeserializer::CFdbSimpleDeserializer(const uint8_t *buffer, int32_t size)
    : mBuffer(buffer)
    , mSize(size)
    , mPos(0)
{
    mError = mBuffer ? false : true;
}

void CFdbSimpleDeserializer::reset(const uint8_t *buffer, int32_t size)
{
    mBuffer = buffer;
    mSize = size;
    mPos = 0;
    mError = mBuffer ? false : true;
}

CFdbSimpleDeserializer& operator>>(CFdbSimpleDeserializer &deserializer, std::string& data)
{
    fdb_string_len_t len = 0;
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

CFdbSimpleDeserializer& operator>>(CFdbSimpleDeserializer &deserializer, IFdbParcelable &data)
{
    data.deserialize(deserializer);
    return deserializer;
}

CFdbSimpleDeserializer& operator>>(CFdbSimpleDeserializer &deserializer, IFdbParcelable *data)
{
    data->deserialize(deserializer);
    return deserializer;
}

void CFdbSimpleDeserializer::retrieveBasicData(uint8_t *p_data, int32_t size)
{
    if (fdb_is_little_endian())
    {
        for (int32_t i = 0; i < size; ++i)
        {
            p_data[i] = mBuffer[mPos + i];
        }
    }
    else
    {
        for (int32_t i = 0; i < size; ++i)
        {
            p_data[i] = mBuffer[mPos + size - 1 - i];
        }
    }
    mPos += size;
}

bool CFdbSimpleDeserializer::retrieveRawData(uint8_t *p_data, int32_t size)
{
    if (mSize && ((mPos + size) > mSize))
    {
        return false;
    }

    memcpy(p_data, mBuffer + mPos, size);
    mPos += size;
    return true;
}

