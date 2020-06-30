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
#include <sstream>

#define FDB_SCRATCH_CACHE_SIZE 2048 
#define FDB_BYTEARRAY_PRINT_SIZE 16
typedef uint16_t fdb_string_len_t;
typedef uint16_t fdb_struct_arr_len_t;
typedef int32_t fdb_byte_arr_len_t;

class CFdbSimpleSerializer;
class CFdbSimpleDeserializer;
class IFdbParcelable
{
public:
    virtual ~IFdbParcelable()
    {}
    
    virtual void serialize(CFdbSimpleSerializer &serializer) const = 0;
    virtual void deserialize(CFdbSimpleDeserializer &deserializer) = 0;
    virtual std::ostringstream &format(std::ostringstream &stream) const
    {
        stream << "{";
        toString(stream);
        stream << "}";
        return stream;
    }
protected:
    virtual void toString(std::ostringstream &stream) const
    {
        stream << "null";
    }
};

class CFdbSimpleSerializer
{
public:
    CFdbSimpleSerializer();
    ~CFdbSimpleSerializer();
#define FDB_OPERATOR_IN(_T) \
    friend CFdbSimpleSerializer& operator<<(CFdbSimpleSerializer &serializer, _T data) \
    { \
        serializer.serializeScalar((const uint8_t *)&data, data); \
        return serializer; \
    }
    FDB_OPERATOR_IN(int8_t)
    FDB_OPERATOR_IN(uint8_t)
    FDB_OPERATOR_IN(int16_t)
    FDB_OPERATOR_IN(uint16_t)
    FDB_OPERATOR_IN(int32_t)
    FDB_OPERATOR_IN(uint32_t)
    FDB_OPERATOR_IN(int64_t)
    FDB_OPERATOR_IN(uint64_t)

    friend CFdbSimpleSerializer& operator<<(CFdbSimpleSerializer &serializer, bool data)
    {
        uint8_t value = data ? 1 : 0;
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
    void addString(const char *string, fdb_string_len_t str_len);

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

#define FDB_OPERATOR_OUT(_T) \
    friend CFdbSimpleDeserializer& operator>>(CFdbSimpleDeserializer& deserializer, _T &data) \
    { \
        deserializer.deserializeScalar(data); \
        return deserializer; \
    }
    FDB_OPERATOR_OUT(int8_t)
    FDB_OPERATOR_OUT(uint8_t)
    FDB_OPERATOR_OUT(int16_t)
    FDB_OPERATOR_OUT(uint16_t)
    FDB_OPERATOR_OUT(int32_t)
    FDB_OPERATOR_OUT(uint32_t)
    FDB_OPERATOR_OUT(int64_t)
    FDB_OPERATOR_OUT(uint64_t)

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

// for array of bool
class abool : public IFdbParcelable
{
public:
    abool &operator=(bool value)
    {
        mValue = value;
        return *this;
    }
    bool operator()() const
    {
        return mValue;
    }
    void serialize(CFdbSimpleSerializer &serializer) const
    {
        serializer << (uint8_t)(mValue ? 1 : 0);
    }
    void deserialize(CFdbSimpleDeserializer &deserializer)
    {
        uint8_t value = 0;
        deserializer >> value;
        mValue = !!value;
    }
    std::ostringstream &format(std::ostringstream &stream) const
    {
        toString(stream);
        return stream;
    }
protected:
    void toString(std::ostringstream &stream) const
    {
        stream << (mValue ? "true" : "false");
    }
private:
    bool mValue;
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
        serializer << (fdb_struct_arr_len_t)mPool.size();
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
        
        fdb_struct_arr_len_t size = 0;
        deserializer >> size;
        mPool.resize(size);
        for (fdb_struct_arr_len_t i = 0; i < size; ++i)
        {
            if (deserializer.error())
            {
                return;
            }
            T &value = mPool[i];
            deserializer >> value;
        }
    }

    std::ostringstream &format(std::ostringstream &stream) const
    {
        stream << "[";
        toString(stream);
        stream << "]";
        return stream;
    }

protected:
    tPool mPool;
};

template<typename T>
class CFdbParcelableArray : public CFdbRepeatedParcelable<T>
{
protected:
    void toString(std::ostringstream &stream) const
    {
        for (typename CFdbRepeatedParcelable<T>::tPool::const_iterator it = CFdbRepeatedParcelable<T>::mPool.begin();
                  it != CFdbRepeatedParcelable<T>::mPool.end(); ++it)
        {
            it->format(stream) << ",";
        }
    }
};

template<>
class CFdbParcelableArray<int8_t> : public CFdbRepeatedParcelable<int8_t>
{
protected:
    void toString(std::ostringstream &stream) const
    {
        for (typename CFdbRepeatedParcelable<int8_t>::tPool::const_iterator it = CFdbRepeatedParcelable<int8_t>::mPool.begin();
                  it != CFdbRepeatedParcelable<int8_t>::mPool.end(); ++it)
        {
            stream << (signed)*it << ",";
        }
    }
};

template<>
class CFdbParcelableArray<uint8_t> : public CFdbRepeatedParcelable<uint8_t>
{
protected:
    void toString(std::ostringstream &stream) const
    {
        for (typename CFdbRepeatedParcelable<uint8_t>::tPool::const_iterator it = CFdbRepeatedParcelable<uint8_t>::mPool.begin();
                  it != CFdbRepeatedParcelable<uint8_t>::mPool.end(); ++it)
        {
            stream << (unsigned)*it << ",";
        }
    }
};

#define CFDBPARCELABLEARRAY(_T) \
template<> \
class CFdbParcelableArray<_T> : public CFdbRepeatedParcelable<_T> \
{ \
protected: \
    void toString(std::ostringstream &stream) const \
    { \
        for (typename CFdbRepeatedParcelable<_T>::tPool::const_iterator it = CFdbRepeatedParcelable<_T>::mPool.begin(); \
                  it != CFdbRepeatedParcelable<_T>::mPool.end(); ++it) \
        { \
            stream << *it << ","; \
        } \
    } \
};

CFDBPARCELABLEARRAY(int16_t)
CFDBPARCELABLEARRAY(uint16_t)
CFDBPARCELABLEARRAY(int32_t)
CFDBPARCELABLEARRAY(uint32_t)
CFDBPARCELABLEARRAY(int64_t)
CFDBPARCELABLEARRAY(uint64_t)

template<>
class CFdbParcelableArray<std::string> : public CFdbRepeatedParcelable<std::string>
{
public:
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

protected:
    void toString(std::ostringstream &stream) const
    {
        for (typename CFdbRepeatedParcelable<std::string>::tPool::const_iterator it = CFdbRepeatedParcelable<std::string>::mPool.begin();
                  it != CFdbRepeatedParcelable<std::string>::mPool.end(); ++it)
        {
            stream << *it << ",";
        }
    }
};

template <int32_t SIZE>
class CFdbByteArray : public IFdbParcelable
{
public:
    CFdbByteArray(int32_t size = SIZE)
        : mSize((size < SIZE) ? size : SIZE)
    {
    }

    int32_t size() const
    {
        return (int32_t)sizeof(mBuffer);
    }

    void size(int32_t size)
    {
        mSize = size;
    }

    const uint8_t *buffer() const
    {
        return mBuffer;
    }

    uint8_t *vbuffer()
    {
        return mBuffer;
    }

    void serialize(CFdbSimpleSerializer &serializer) const
    {
        serializer << (fdb_byte_arr_len_t)mSize;
        serializer.addRawData(mBuffer, mSize);
    }

    void deserialize(CFdbSimpleDeserializer &deserializer)
    {
        if (deserializer.error())
        {
            return;
        }
        fdb_byte_arr_len_t size = 0;
        deserializer >> size;
        if (size <= SIZE)
        {
            mSize = size;
            deserializer.retrieveRawData(mBuffer, size);
        }
        else
        {
            deserializer.error(true);
        }
    }

    std::ostringstream &format(std::ostringstream &stream) const
    {
        int32_t psize = mSize;
        if (psize > FDB_BYTEARRAY_PRINT_SIZE)
        {
            stream << mSize << "[";
            psize = FDB_BYTEARRAY_PRINT_SIZE;
        }
        else
        {
            stream << "[";
        }
        for (int32_t i = 0; i < psize; ++i)
        {
            stream << (unsigned)mBuffer[i] << ",";
        }
        stream << "]";
        return stream;
    }

private:
    int32_t mSize;
    uint8_t mBuffer[SIZE];
};
    
class CFdbByteArrayExt : public IFdbParcelable
{
public:
    CFdbByteArrayExt()
        : mSize(0)
        , mBuffer(0)
        , mVBuffer(0)
    {}

    CFdbByteArrayExt(int32_t size, const uint8_t *buffer)
        : mSize(size)
        , mBuffer(buffer)
        , mVBuffer(0)
    {}

    ~CFdbByteArrayExt()
    {
        if (mVBuffer)
        {
            delete mVBuffer;
        }
    }

    int32_t size() const
    {
        return mSize;
    }

    uint8_t *vbuffer(bool remove = false)
    {
        uint8_t *buffer = mVBuffer;
        if (remove)
        {
            mVBuffer = 0;
        }
        return buffer;
    }

    const uint8_t *buffer() const
    {
        return mBuffer;
    }
    
    void serialize(CFdbSimpleSerializer &serializer) const
    {
        if (!mSize || mBuffer)
        {
            serializer << (fdb_byte_arr_len_t)mSize;
            if (mBuffer)
            {
                serializer.addRawData(mBuffer, mSize);
            }
        }
    }

    void deserialize(CFdbSimpleDeserializer &deserializer)
    {
        if (deserializer.error())
        {
            return;
        }
        fdb_byte_arr_len_t size = 0;
        deserializer >> size;
        if (!size)
        {
            return;
        }

        if (!mVBuffer)
        {
            try
            {
                mVBuffer = new uint8_t[size];
                mSize = size;
            }
            catch (...)
            {
                deserializer.error(true);
                return;
            }
        }
        deserializer.retrieveRawData(mVBuffer, size);
    }

    std::ostringstream &format(std::ostringstream &stream) const
    {
        int32_t psize = mSize;
        if (psize > FDB_BYTEARRAY_PRINT_SIZE)
        {
            stream << mSize << "[";
            psize = FDB_BYTEARRAY_PRINT_SIZE;
        }
        else
        {
            stream << "[";
        }
        const uint8_t *buffer = mBuffer ? mBuffer : mVBuffer;
        if (buffer)
        {
            for (int32_t i = 0; i < psize; ++i)
            {
                stream << (unsigned)buffer[i] << ",";
            }
        }
        stream << "]";
        return stream;
    }

private:
    int32_t mSize;
    const uint8_t *mBuffer;
    uint8_t *mVBuffer;
};

#endif

