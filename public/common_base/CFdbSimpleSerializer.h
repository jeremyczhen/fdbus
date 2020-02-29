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
typedef uint16_t fdb_ser_arrlen_t;

class CFdbSimpleSerializer;
class CFdbSimpleDeserializer;
class IFdbParcelable
{
public:
    virtual ~IFdbParcelable()
    {}
    
    virtual void serialize(CFdbSimpleSerializer &serializer) const = 0;
    virtual void deserialize(CFdbSimpleDeserializer &deserializer) = 0;
};

class CFdbSimpleSerializer
{
public:
    CFdbSimpleSerializer();
    ~CFdbSimpleSerializer();
    friend CFdbSimpleSerializer& operator<<(CFdbSimpleSerializer &serializer, int8_t data)
    {
        serializer.serializeScalar((const uint8_t *)&data, data);
        return serializer;
    }
    friend CFdbSimpleSerializer& operator<<(CFdbSimpleSerializer &serializer, uint8_t data)
    {
        serializer.serializeScalar((const uint8_t *)&data, data);
        return serializer;
    }
    friend CFdbSimpleSerializer& operator<<(CFdbSimpleSerializer &serializer, int16_t data)
    {
        serializer.serializeScalar((const uint8_t *)&data, data);
        return serializer;
    }
    friend CFdbSimpleSerializer& operator<<(CFdbSimpleSerializer &serializer, uint16_t data)
    {
        serializer.serializeScalar((const uint8_t *)&data, data);
        return serializer;
    }
    friend CFdbSimpleSerializer& operator<<(CFdbSimpleSerializer &serializer, int32_t data)
    {
        serializer.serializeScalar((const uint8_t *)&data, data);
        return serializer;
    }
    friend CFdbSimpleSerializer& operator<<(CFdbSimpleSerializer &serializer, uint32_t data)
    {
        serializer.serializeScalar((const uint8_t *)&data, data);
        return serializer;
    }
    friend CFdbSimpleSerializer& operator<<(CFdbSimpleSerializer &serializer, int64_t data)
    {
        serializer.serializeScalar((const uint8_t *)&data, data);
        return serializer;
    }
    friend CFdbSimpleSerializer& operator<<(CFdbSimpleSerializer &serializer, uint64_t data)
    {
        serializer.serializeScalar((const uint8_t *)&data, data);
        return serializer;
    }
    friend CFdbSimpleSerializer& operator<<(CFdbSimpleSerializer &serializer, bool data)
    {
        uint8_t value = data;
        serializer.serializeScalar((const uint8_t *)&value, value);
        return serializer;
    }

    friend CFdbSimpleSerializer& operator<<(CFdbSimpleSerializer &serializer, const std::string& data);
    friend CFdbSimpleSerializer& operator<<(CFdbSimpleSerializer &serializer, const char *data);

    friend CFdbSimpleSerializer& operator<<(CFdbSimpleSerializer &serializer, const IFdbParcelable &data);
    friend CFdbSimpleSerializer& operator<<(CFdbSimpleSerializer &serializer, const IFdbParcelable *data);

    int32_t toBuffer(uint8_t *buffer, int32_t size);
    uint8_t *buffer() const
    {
        return mBuffer;
    }
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
    template <typename T>
    void serializeScalar(const uint8_t *p_data, T data)
    {
        addBasicType(p_data, (int32_t)sizeof(T));
    }
    void addBasicType(const uint8_t *p_data, int32_t size);
};

class CFdbSimpleDeserializer
{
public:
    CFdbSimpleDeserializer(const uint8_t *buffer = 0, int32_t size = 0);
    void reset(const uint8_t *buffer, int32_t size = 0);

    friend CFdbSimpleDeserializer& operator>>(CFdbSimpleDeserializer& deserializer, int8_t &data)
    {
        deserializer.deserializeScalar(data);
        return deserializer;
    }
    friend CFdbSimpleDeserializer& operator>>(CFdbSimpleDeserializer& deserializer, uint8_t &data)
    {
        deserializer.deserializeScalar(data);
        return deserializer;
    }
    friend CFdbSimpleDeserializer& operator>>(CFdbSimpleDeserializer& deserializer, int16_t &data)
    {
        deserializer.deserializeScalar(data);
        return deserializer;
    }
    friend CFdbSimpleDeserializer& operator>>(CFdbSimpleDeserializer& deserializer, uint16_t &data)
    {
        deserializer.deserializeScalar(data);
        return deserializer;
    }
    friend CFdbSimpleDeserializer& operator>>(CFdbSimpleDeserializer& deserializer, int32_t &data)
    {
        deserializer.deserializeScalar(data);
        return deserializer;
    }
    friend CFdbSimpleDeserializer& operator>>(CFdbSimpleDeserializer& deserializer, uint32_t &data)
    {
        deserializer.deserializeScalar(data);
        return deserializer;
    }
    friend CFdbSimpleDeserializer& operator>>(CFdbSimpleDeserializer& deserializer, int64_t &data)
    {
        deserializer.deserializeScalar(data);
        return deserializer;
    }
    friend CFdbSimpleDeserializer& operator>>(CFdbSimpleDeserializer& deserializer, uint64_t &data)
    {
        deserializer.deserializeScalar(data);
        return deserializer;
    }
    friend CFdbSimpleDeserializer& operator>>(CFdbSimpleDeserializer& deserializer, bool &data)
    {
        uint8_t value = 0;
        deserializer.deserializeScalar(value);
        data = !!value;
        return deserializer;
    }

    friend CFdbSimpleDeserializer& operator>>(CFdbSimpleDeserializer &deserializer, std::string& data);
    friend CFdbSimpleDeserializer& operator>>(CFdbSimpleDeserializer &deserializer, IFdbParcelable &data);
    friend CFdbSimpleDeserializer& operator>>(CFdbSimpleDeserializer &deserializer, IFdbParcelable *data);

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
    
    template<typename T>
    void deserializeScalar(T &data)
    {
        if (mError)
        {
            return;
        }
        
        int32_t size = (int32_t)sizeof(T);
        if (mSize && ((mPos + size) > mSize))
        {
            mError = true;
            return;
        }
        retrieveBasicData((uint8_t *)&data, size);
    }
};

template<typename T>
class CFdbRepeatedParcelable : public IFdbParcelable
{
public:
    typedef T tData;
    typedef std::vector<tData> tPool;

    CFdbRepeatedParcelable()
    {}
    CFdbRepeatedParcelable(uint32_t size) : mPool(size)
    {}
    CFdbRepeatedParcelable(const tPool &x) : mPool(x)
    {}

    uint32_t size() const
    {
        return (uint32_t)mPool.size();
    }

    bool empty() const
    {
        return mPool.empty();
    }

    void clear()
    {
        mPool.clear();
    }

    void resize(uint32_t size)
    {
        mPool.resize((uint32_t)size);
    }

    void Add(const tData &element)
    {
        mPool.push_back(element);
    }

    tData *Add()
    {
        mPool.resize(mPool.size() + 1);
        return &(mPool.back());
    }

    const tPool &pool() const
    {
        return mPool;
    }

    tPool &vpool()
    {
        return mPool;
    }
    
    void serialize(CFdbSimpleSerializer &serializer) const
    {
        serializer << (fdb_ser_arrlen_t)mPool.size();
        for (typename tPool::const_iterator it = mPool.begin(); it != mPool.end(); ++it)
        {
            serializer << *it;
        }
    }

    void deserialize(CFdbSimpleDeserializer &deserializer)
    {
        if (deserializer.error())
        {
            return;
        }
        
        fdb_ser_arrlen_t size = 0;
        deserializer >> size;
        mPool.resize(size);
        for (fdb_ser_arrlen_t i = 0; i < size; ++i)
        {
            if (deserializer.error())
            {
                return;
            }
            T &value = mPool[i];
            deserializer >> value;
        }
    }

protected:
    tPool mPool;
};

template<typename T>
class CFdbParcelableArray : public CFdbRepeatedParcelable<T>
{
public:
    CFdbParcelableArray()
    {}
    CFdbParcelableArray(uint32_t size) : CFdbRepeatedParcelable<T>(size)
    {}
    CFdbParcelableArray(const typename CFdbRepeatedParcelable<T>::tPool &x) : CFdbRepeatedParcelable<T>(x)
    {}
};

template<>
class CFdbParcelableArray<std::string> : public CFdbRepeatedParcelable<std::string>
{
public:
    CFdbParcelableArray()
    {}
    CFdbParcelableArray(uint32_t size) : CFdbRepeatedParcelable<std::string>(size)
    {}
    CFdbParcelableArray(const typename CFdbRepeatedParcelable<std::string>::tPool &x) : CFdbRepeatedParcelable<std::string>(x)
    {}

    void Add(const std::string &element)
    {
        mPool.push_back(element);
    }

    std::string *Add()
    {
        mPool.resize(mPool.size() + 1);
        return &(mPool.back());
    }

    void Add(const char *element)
    {
        mPool.push_back(element);
    }
};

#endif

