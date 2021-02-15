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

#include <utils/Log.h>
#include <string.h>
#include <common_base/CSysFdWatch.h>
#include <common_base/CFdEventLoop.h>
#include <common_base/CFdbRawMsgBuilder.h>
#include <common_base/CFdbBaseObject.h>
#include <common_base/CFdbContext.h>
#include <common_base/CLogProducer.h>

#define FDB_MAX_RECURSIVE_SIZE   	32

CSysFdWatch::CSysFdWatch(int fd, uint32_t flags)
    : mFd(fd)
    , mFlags(flags)
    , mEnable(false)
    , mFatalError(false)
    , mEventLoop(0)
    , mInputRecursiveDepth(0)
{}

CSysFdWatch::~CSysFdWatch()
{
    if (mFd > 0)
    {
        closePollFd(mFd);
    }

    clearOutputChunkList();
}

void CSysFdWatch::enable(bool enb)
{
    if (enb)
    {
        mFatalError = false;
    }
    mEventLoop->enableWatch(this, enb);
    mEnable = enb;
}

void CSysFdWatch::fatalError(bool enb)
{
    if (mFatalError != enb)
    {
        mEventLoop->rebuildPollFd();
    }
    mFatalError = enb;
}

void CSysFdWatch::updateFlags(uint32_t mask, uint32_t value)
{
    uint32_t flags = (mFlags & ~mask) | value;
    if (mFlags != flags)
    {
        mFlags = flags;
        mEventLoop->rebuildPollFd();
    }
}

CSysFdWatch::COutputDataChunk::COutputDataChunk(uint8_t *buffer, int32_t size,
                                                int32_t consumed, CFdbRawMsgBuilder *log_builder)
    : mSize(size)
    , mConsumed(consumed)
    , mLogBuffer(0)
    , mLogSize(0)
{
    mBuffer = new uint8_t[size];
    memcpy(mBuffer, buffer, size);

    if (log_builder)
    {
        mLogSize = log_builder->serializer().bufferSize();
        if (mLogSize)
        {
            mLogBuffer = new uint8_t[mLogSize];
            log_builder->toBuffer(mLogBuffer, mLogSize);
        }
    }
}

CSysFdWatch::COutputDataChunk::~COutputDataChunk()
{
    if (mLogBuffer)
    {
        delete[] mLogBuffer;
    }

    if (mBuffer)
    {
        delete[] mBuffer;
    }
}

void CSysFdWatch::submitInput(uint8_t *buffer, int32_t size, bool trigger_read)
{
    if (!buffer || fatalError())
    {
        return;
    }
    if ((mInputRecursiveDepth > FDB_MAX_RECURSIVE_SIZE) || !trigger_read)
    {
        mInputChunk.mBuffer = buffer;
        mInputChunk.mSize = size;
        mInputChunk.mConsumed = 0;
        return;
    }

    mInputRecursiveDepth++;
    try
    {
        auto consumed = readStream(buffer, size);
        if (consumed < 0)
        {
            fatalError(true);
            onInputReady(buffer, consumed); // consumed < 0 means error happens
        }
        else if (consumed < size)
        {
            mInputChunk.mBuffer = buffer;
            mInputChunk.mSize = size;
            mInputChunk.mConsumed = consumed;
        }
        else
        {
            onInputReady(buffer, size);
        }
    }
    catch (...)
    {
        LOG_E("CSysFdWatch: Exception received at line %d of file %s!\n", __LINE__, __FILE__);
    }
    mInputRecursiveDepth--;
}

void CSysFdWatch::submitOutput(uint8_t *buffer, int32_t size, CFdbRawMsgBuilder *log_builder)
{
    if (!buffer || !size || fatalError())
    {
        return;
    }
    if (mOutputChunkList.size())
    {
        mOutputChunkList.push_back(new COutputDataChunk(buffer, size, 0, log_builder));
    }
    else
    {
        auto consumed = writeStream(buffer, size);
        if (consumed < 0)
        {
            fatalError(true);
        }
        else if (consumed < size)
        {
            mOutputChunkList.push_back(new COutputDataChunk(buffer, size, consumed, log_builder));
            updateFlags(POLLOUT, POLLOUT);
        }
        else
        {
            if (log_builder && log_builder->serializer().bufferSize())
            {
                auto logger = FDB_CONTEXT->getLogger();
                if (logger)
                {
                    logger->sendLog(NFdbBase::REQ_FDBUS_LOG, *log_builder);
                }
            }
        }
    }
}

void CSysFdWatch::processInput()
{
    if (!mInputChunk.mBuffer)
    {
        onInput();
        return;
    }

    auto buffer = mInputChunk.mBuffer + mInputChunk.mConsumed;
    auto size = mInputChunk.mSize - mInputChunk.mConsumed;
    auto consumed = readStream(buffer, size);
    if (consumed < 0)
    {
        fatalError(true);
        onInputReady(mInputChunk.mBuffer, consumed); // consumed < size means error happens
    }
    else if (consumed < size)
    {
        mInputChunk.mConsumed += consumed;
    }
    else
    {
        onInputReady(mInputChunk.mBuffer, mInputChunk.mSize);
    }
}

void CSysFdWatch::clearOutputChunkList()
{
    while (!mOutputChunkList.empty())
    {
        auto it = mOutputChunkList.begin();
        delete *it;
        mOutputChunkList.pop_front();
    }
}

void CSysFdWatch::processOutput()
{
    if (mOutputChunkList.empty())
    {
        onOutput();
        return;
    }

    while (!mOutputChunkList.empty())
    {
        auto it = mOutputChunkList.begin();
        auto chunk = *it;
        auto buffer = chunk->mBuffer + chunk->mConsumed;
        auto size = chunk->mSize - chunk->mConsumed;
        auto consumed = writeStream(buffer, size);
        if (consumed < 0)
        {
            fatalError(true);
            clearOutputChunkList();
            break;
        }
        else if (consumed < size)
        {
            chunk->mConsumed += consumed;
            break;
        }
        else
        {
            if (chunk->mLogBuffer)
            {
                auto logger = FDB_CONTEXT->getLogger();
                if (logger)
                {
                    logger->sendLog(NFdbBase::REQ_FDBUS_LOG, chunk->mLogBuffer, chunk->mLogSize);
                }
            }
            delete chunk;
            mOutputChunkList.pop_front();
        }
    }

    if (mOutputChunkList.empty())
    {
        updateFlags(POLLOUT, 0);
    }
}

