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

#include <string.h>
#include <stdarg.h>
#include <common_base/CFdbMessage.h>
#include <common_base/CBaseEndpoint.h>
#include <common_base/CFdbContext.h>
#include <common_base/CNanoTimer.h>
#include <common_base/CFdbSession.h>
#include <common_base/CLogProducer.h>
#include <common_base/CFdbBaseObject.h>
#include <utils/Log.h>
#include <common_base/CFdbIfMessageHeader.h>

#define FDB_MSG_TX_SYNC         (1 << 0)
#define FDB_MSG_TX_NO_REPLY     (1 << 1)
#define FDB_MAX_STATUS_SIZE     1024

class CMessageTimer : public CBaseLoopTimer
{
public:
    CMessageTimer(int32_t interval)
        : CBaseLoopTimer(interval, false)
        , mSession(0)
        , mMsgSn(FDB_INVALID_ID)
    {}
protected:
    void run()
    {
        mSession->terminateMessage(mMsgSn, NFdbBase::FDB_ST_TIMEOUT, "Message is destroyed due to timeout.");
    }
private:
    CFdbSession *mSession;
    FdbMsgSn_t mMsgSn;
    friend class CFdbMessage;
};

CFdbMessage::CFdbMessage(FdbMsgCode_t code)
    : mType(FDB_MT_REQUEST)
    , mCode(code)
    , mSn(FDB_INVALID_ID)
    , mPayloadSize(0)
    , mHeadSize(0)
    , mOffset(0)
    , mSid(FDB_INVALID_ID)
    , mOid(FDB_INVALID_ID)
    , mBuffer(0)
    , mFlag(0)
    , mTimer(0)
    , mMigrateObject(0)
    , mMigrateFlag(0)
{
}

CFdbMessage::CFdbMessage(FdbMsgCode_t code, CFdbBaseObject *obj, FdbSessionId_t alt_receiver)
    : mType(FDB_MT_REQUEST)
    , mCode(code)
    , mSn(FDB_INVALID_ID)
    , mPayloadSize(0)
    , mHeadSize(0)
    , mOffset(0)
    , mBuffer(0)
    , mFlag(0)
    , mTimer(0)
    , mMigrateObject(0)
    , mMigrateFlag(0)
{
    setDestination(obj, alt_receiver);
}

CFdbMessage::CFdbMessage(FdbMsgCode_t code, CFdbMessage *msg, const char *filter)
    : mType(FDB_MT_BROADCAST)
    , mCode(code)
    , mSn(msg->mSn)
    , mPayloadSize(0)
    , mHeadSize(0)
    , mOffset(0)
    , mSid(msg->mSid)
    , mOid(msg->mOid)
    , mBuffer(0)
    , mFlag(0)
    , mTimer(0)
    , mMigrateObject(0)
    , mMigrateFlag(0)
{
    if (filter)
    {
        mFilter = filter;
    }
    mFlag |= msg->mFlag & (MSG_FLAG_MANUAL_UPDATE | MSG_FLAG_ENABLE_LOG);

}

CFdbMessage::CFdbMessage(NFdbBase::CFdbMessageHeader &head
                         , CFdbMsgPrefix &prefix
                         , uint8_t *buffer
                         , CFdbSession *session
                        )
    : mType(FDB_MT_REPLY)
    , mCode(head.code())
    , mSn(head.serial_number())
    , mPayloadSize(head.payload_size())
    , mHeadSize(prefix.mHeadLength)
    , mOffset(0)
    , mSid(session->sid())
    , mOid(head.object_id())
    , mBuffer(buffer)
    , mFlag((head.flag() & MSG_GLOBAL_FLAG_MASK) | MSG_FLAG_EXTERNAL_BUFFER)
    , mTimer(0)
    , mMigrateObject(0)
    , mMigrateFlag(0)
{
    if (head.has_broadcast_filter())
    {
        mFilter = head.broadcast_filter().c_str();
    }
};

CFdbMessage::CFdbMessage(FdbMsgCode_t code
                         , CFdbBaseObject *obj
                         , const char *filter
                         , FdbSessionId_t alt_sid
                         , FdbObjectId_t alt_oid)
    : mType(FDB_MT_BROADCAST)
    , mCode(code)
    , mSn(FDB_INVALID_ID)
    , mPayloadSize(0)
    , mHeadSize(0)
    , mOffset(0)
    , mBuffer(0)
    , mFlag(0)
    , mTimer(0)
    , mMigrateObject(0)
    , mMigrateFlag(0)
{
    setDestination(obj, FDB_INVALID_ID);
    if (filter)
    {
        mFilter = filter;
    }

    if (fdbValidFdbId(alt_sid))
    {
        mSid = alt_sid;
        mFlag &= ~MSG_FLAG_ENDPOINT;
    }
    else
    {
        mEpid = obj->epid();
        mFlag |= MSG_FLAG_ENDPOINT;
    }
    if (fdbValidFdbId(alt_oid))
    {
        mOid = alt_oid;
    }
}

CFdbMessage::~CFdbMessage()
{
    if (mTimer)
    {
        delete mTimer;
        mTimer = 0;
    }
    releaseBuffer();
    //LOG_I("Message %d is destroyed!\n", (int32_t)mSn);
}

void CFdbMessage::setDestination(CFdbBaseObject *obj, FdbSessionId_t alt_sid)
{
    auto sid = obj->getDefaultSession();
    if (fdbValidFdbId(alt_sid))
    {
        mSid = alt_sid;
        mFlag &= ~MSG_FLAG_ENDPOINT;
    }
    else if (fdbValidFdbId(sid))
    {
        mSid = sid;
        mFlag &= ~MSG_FLAG_ENDPOINT;
    }
    else
    {
        mEpid = obj->epid();
        mFlag |= MSG_FLAG_ENDPOINT;
    }
    mOid = obj->objId();
}

void CFdbMessage::run(CBaseWorker *worker, Ptr &ref)
{
    if (mMigrateObject)
    {
        // process callback migrated from context thread to worker therad
        auto object = mMigrateObject;
        auto flag = mMigrateFlag;
        mMigrateObject = 0;
        mMigrateFlag = 0;
        object->remoteCallback(ref, flag);
        return;
    }
    // process request from other threads to context thread
    switch (mType)
    {
        case FDB_MT_REQUEST:
        case FDB_MT_SIDEBAND_REQUEST:
            doRequest(ref);
            break;
        case FDB_MT_REPLY:
        case FDB_MT_SIDEBAND_REPLY:
            doReply(ref);
            break;
        case FDB_MT_BROADCAST:
            doBroadcast(ref);
            break;
        case FDB_MT_STATUS:
            doStatus(ref);
            break;
        case FDB_MT_SUBSCRIBE_REQ:
            if ((mCode == FDB_CODE_SUBSCRIBE) || (mCode == FDB_CODE_UPDATE))
			{
                doSubscribeReq(ref);
            }
            else if (mCode == FDB_CODE_UNSUBSCRIBE)
            {
                doUnsubscribeReq(ref);
            }
            break;
        default:
            LOG_E("CFdbMessage: Message %d: Unknown type!\n", (int32_t)mSn);
            break;
    }
}

bool CFdbMessage::feedback(CBaseJob::Ptr &msg_ref
                         , EFdbMessageType type)
{
    mType = type;
    mFlag |= MSG_FLAG_REPLIED;
    if (!CFdbContext::getInstance()->sendAsync(msg_ref))
    {
        mFlag &= ~MSG_FLAG_REPLIED;
        LOG_E("CFdbMessage: Fail to send message job to FDB_CONTEXT!\n");
        return false;
    }
    return true;
}

bool CFdbMessage::reply(CBaseJob::Ptr &msg_ref, IFdbMsgBuilder &data)
{
    auto fdb_msg = castToMessage<CFdbMessage *>(msg_ref);
    if (fdb_msg->mFlag & MSG_FLAG_NOREPLY_EXPECTED)
    {
        return false;
    }
    if (!fdb_msg->serialize(data))
    {
        return false;
    }
    return fdb_msg->feedback(msg_ref, FDB_MT_REPLY);
}

bool CFdbMessage::reply(CBaseJob::Ptr &msg_ref
                      , const void *buffer
                      , int32_t size
                      , const char *log_data)
{
    auto fdb_msg = castToMessage<CFdbMessage *>(msg_ref);
    if (fdb_msg->mFlag & MSG_FLAG_NOREPLY_EXPECTED)
    {
        return false;
    }
    if (!fdb_msg->serialize((uint8_t *)buffer, size))
    {
        return false;
    }
    fdb_msg->setLogData(log_data);
    
    fdb_msg->mType = FDB_MT_REPLY;
    fdb_msg->mFlag |= MSG_FLAG_REPLIED;
    if (!CFdbContext::getInstance()->sendAsync(msg_ref))
    {
        fdb_msg->mFlag &= ~MSG_FLAG_REPLIED;
        LOG_E("CFdbMessage: Fail to send reply job to FDB_CONTEXT!\n");
        return false;
    }
    return true;
}

bool CFdbMessage::replyNoQueue(CBaseJob::Ptr &msg_ref, const void *buffer, int32_t size)
{
    auto fdb_msg = castToMessage<CFdbMessage *>(msg_ref);
    if (fdb_msg->mFlag & MSG_FLAG_NOREPLY_EXPECTED)
    {
        return false;
    }
    if (!fdb_msg->serialize((uint8_t *)buffer, size))
    {
        return false;
    }
 
    fdb_msg->mType = FDB_MT_REPLY;
    fdb_msg->mFlag |= MSG_FLAG_REPLIED;
    auto session = fdb_msg->getSession();
    if (session)
    {
        session->sendMessage(fdb_msg);
    }
    return true;
}

bool CFdbMessage::status(CBaseJob::Ptr &msg_ref, int32_t error_code, const char *description)
{
    auto fdb_msg = castToMessage<CFdbMessage *>(msg_ref);
    if (fdb_msg->mFlag & MSG_FLAG_NOREPLY_EXPECTED)
    {
        return false;
    }
    fdb_msg->setErrorMsg(FDB_MT_STATUS, error_code, description);
    if (!CFdbContext::getInstance()->sendAsync(msg_ref))
    {
        LOG_E("CFdbMessage: Fail to send status job to FDB_CONTEXT!\n");
        return false;
    }
    return true;
}

bool CFdbMessage::statusf(CBaseJob::Ptr &msg_ref, int32_t error_code, ...)
{
    char description[FDB_MAX_STATUS_SIZE];
    description[0] = '\0';
    va_list args;
    va_start(args, error_code);
    const char *format = va_arg(args, const char *);
    vsnprintf(description, FDB_MAX_STATUS_SIZE, format, args);
    va_end(args);
    return status(msg_ref, error_code, description);
}

bool CFdbMessage::submit(CBaseJob::Ptr &msg_ref
                         , uint32_t tx_flag
                         , int32_t timeout)
{
    bool sync = !!(tx_flag & FDB_MSG_TX_SYNC);
    if (sync && FDB_CONTEXT->isSelf())
    {
        LOG_E("CFdbMessage: Cannot send sychronously from FDB_CONTEXT!\n");
        return false;
    }

    if (tx_flag & FDB_MSG_TX_NO_REPLY)
    {
        mFlag |= MSG_FLAG_NOREPLY_EXPECTED;
    }
    else
    {
        mFlag |= MSG_FLAG_AUTO_REPLY;
        if (sync)
        {
            mFlag |= MSG_FLAG_SYNC_REPLY;
        }
        if (timeout > 0)
        {
            mTimer = new CMessageTimer(timeout);
        }
    }

    bool ret;
    if (sync)
    {
        ret = CFdbContext::getInstance()->sendSync(msg_ref);
    }
    else
    {
        ret = CFdbContext::getInstance()->sendAsync(msg_ref);
    }
    if (!ret)
    {
        LOG_E("CFdbMessage: Fail to send job to FDB_CONTEXT!\n");
    }
    return ret;
}

bool CFdbMessage::invoke(CBaseJob::Ptr &msg_ref
                         , uint32_t tx_flag
                         , int32_t timeout)
{
    mType = FDB_MT_REQUEST;
    return submit(msg_ref, tx_flag, timeout);
}

bool CFdbMessage::invoke(int32_t timeout)
{
    CBaseJob::Ptr msg_ref(this);
    return invoke(msg_ref, 0, timeout);
}

bool CFdbMessage::invoke(CBaseJob::Ptr &msg_ref
                         , int32_t timeout)
{
    auto msg = castToMessage<CFdbMessage *>(msg_ref);
    return msg ? msg->invoke(msg_ref, FDB_MSG_TX_SYNC, timeout) : false;
}

bool CFdbMessage::send()
{
    CBaseJob::Ptr msg_ref(this);
    return invoke(msg_ref, FDB_MSG_TX_NO_REPLY, -1);
}

bool CFdbMessage::broadcast(FdbMsgCode_t code
                           , IFdbMsgBuilder &data
                           , const char *filter)
{
    auto msg = new CFdbMessage(code, this, filter);
    msg->mFlag |= mFlag & MSG_FLAG_ENABLE_LOG;
    if (!msg->serialize(data))
    {
        delete msg;
        return false;
    }
    msg->forceUpdate(true);
    return msg->broadcast();
}

bool CFdbMessage::broadcast(FdbMsgCode_t code
                           , const void *buffer
                           , int32_t size
                           , const char *filter
                           , const char *log_data)
{
    auto msg = new CFdbMessage(code, this, filter);
    if (!msg->serialize(buffer, size))
    {
        delete msg;
        return false;
    }
    msg->forceUpdate(true);
    msg->setLogData(log_data);
    return msg->broadcast();
}

bool CFdbMessage::broadcast()
{
   mType = FDB_MT_BROADCAST;
   if (!CFdbContext::getInstance()->sendAsync(this))
   {
       LOG_E("CFdbMessage: Fail to send broadcast job to FDB_CONTEXT!\n");
       return false;
   }
   return true;
}

bool CFdbMessage::subscribe(CBaseJob::Ptr &msg_ref
                            , uint32_t tx_flag
                            , FdbMsgCode_t subscribe_code
                            , int32_t timeout)
{
    mType = FDB_MT_SUBSCRIBE_REQ;
    mCode = subscribe_code;
    return submit(msg_ref, tx_flag, timeout);
}

bool CFdbMessage::subscribe(int32_t timeout)
{
    CBaseJob::Ptr msg_ref(this);
    return subscribe(msg_ref, 0, FDB_CODE_SUBSCRIBE, timeout);
}

bool CFdbMessage::subscribe(CBaseJob::Ptr &msg_ref, int32_t timeout)
{
    auto msg = castToMessage<CFdbMessage *>(msg_ref);
    return msg ? msg->subscribe(msg_ref, FDB_MSG_TX_SYNC, FDB_CODE_SUBSCRIBE, timeout) : false;
}

bool CFdbMessage::unsubscribe()
{
    CBaseJob::Ptr msg_ref(this);
    return subscribe(msg_ref, FDB_MSG_TX_NO_REPLY, FDB_CODE_UNSUBSCRIBE, 0);
}

bool CFdbMessage::update(int32_t timeout)
{
    CBaseJob::Ptr msg_ref(this);
    // actually subscribe nothing but just trigger a broadcast() from onBroadcast()
    return subscribe(msg_ref, 0, FDB_CODE_UPDATE, timeout);
}

bool CFdbMessage::update(CBaseJob::Ptr &msg_ref
                            , int32_t timeout)
{
    auto msg = castToMessage<CFdbMessage *>(msg_ref);
    // actually subscribe nothing but just trigger a broadcast() from onBroadcast()
    return msg ? msg->subscribe(msg_ref, FDB_MSG_TX_SYNC, FDB_CODE_UPDATE, timeout) : false;
}

bool CFdbMessage::buildHeader(CFdbSession *session)
{
    if (mFlag & MSG_FLAG_HEAD_OK)
    {
        return true;
    }
    NFdbBase::CFdbMessageHeader msg_hdr;
    msg_hdr.set_type(mType);
    msg_hdr.set_serial_number(mSn);
    msg_hdr.set_code(mCode);
    msg_hdr.set_flag(mFlag & MSG_GLOBAL_FLAG_MASK);
    msg_hdr.set_object_id(mOid);
    msg_hdr.set_payload_size(mPayloadSize);

    encodeDebugInfo(msg_hdr, session);

    auto filter = mFilter.c_str();
    if (filter[0] != '\0')
    {
        msg_hdr.set_broadcast_filter(filter);
    }

    CFdbParcelableBuilder builder(msg_hdr);
    int32_t head_size = builder.build();
    if ((head_size > mMaxHeadSize) || (head_size < 0))
    {
        LOG_E("CFdbMessage: Message %d of Session %d: Head is too long or error!\n", (int32_t)mCode, (int32_t)mSid);
        return false;
    }
    mHeadSize = head_size;
    int32_t head_offset = maxReservedSize() - head_size;
    int32_t prefix_offset = head_offset - mPrefixSize;
    mOffset = prefix_offset;

    if (!builder.toBuffer(mBuffer + head_offset, head_size))
    {
        return false;
    }

    // Update offset and head size according to actual head size
    CFdbMsgPrefix prefix(getRawDataSize(), mHeadSize);
    prefix.serialize(getRawBuffer());

    mFlag |= MSG_FLAG_HEAD_OK;
    return true;
}

void CFdbMessage::freeRawBuffer()
{
    if (mBuffer)
    {
        delete[] mBuffer;
        mBuffer = 0;
    }
}

bool CFdbMessage::allocCopyRawBuffer(const void *src, int32_t payload_size)
{
    int32_t total_size = maxReservedSize() + payload_size;
    uint8_t *buffer = new uint8_t[total_size];
    if (src)
    {
        memcpy(buffer +  maxReservedSize(), src, payload_size);
    }
    releaseBuffer();
    mBuffer = buffer;
    return true;
}

bool CFdbMessage::serialize(IFdbMsgBuilder &data, const CFdbBaseObject *object)
{
    mOffset = 0;
    mHeadSize = mMaxHeadSize;

    if (object)
    {
        checkLogEnabled(object);
    }
    
    mFlag |= MSG_FLAG_EXTERNAL_BUFFER;
    int32_t size = data.build();
    if (size < 0)
    {
        return false;
    }
    mPayloadSize = size;
    if (allocCopyRawBuffer(0, mPayloadSize))
    {
        if (!data.toBuffer(mBuffer + maxReservedSize(), mPayloadSize))
        {
            return false;
        }
    }

    if (object)
    {
        checkLogEnabled(object);
    }

    if (mFlag & MSG_FLAG_ENABLE_LOG)
    {
        auto logger = CFdbContext::getInstance()->getLogger();
        if (logger)
        {
            data.toString(mStringData);
        }
    }
    return true;
}

bool CFdbMessage::serialize(const void *buffer, int32_t size, const CFdbBaseObject *object)
{
    mOffset = 0;
    mHeadSize = mMaxHeadSize;

    if (object)
    {
        checkLogEnabled(object);
    }
    
    mFlag |= MSG_FLAG_EXTERNAL_BUFFER;
    mPayloadSize = size;
    return allocCopyRawBuffer(buffer, mPayloadSize);
}

void CFdbMessage::releaseBuffer()
{
    if (mFlag & MSG_FLAG_EXTERNAL_BUFFER)
    {
        freeRawBuffer();
    }
    else if (mBuffer)
    {
        delete[] mBuffer;
        mBuffer = 0;
    }
}

void CFdbMessage::replaceBuffer(uint8_t *buffer, int32_t payload_size,
                                int32_t head_size, int32_t offset)
{
    releaseBuffer();
    mBuffer = buffer;
    mPayloadSize = payload_size;
    mHeadSize = head_size;
    mOffset = offset;
}

void CFdbMessage::doRequest(Ptr &ref)
{
    bool success = true;
    const char *reason;
    auto session = getSession();
    if (session)
    {
        if (mFlag & MSG_FLAG_NOREPLY_EXPECTED)
        {
            success = session->sendMessage(this);
            reason = "error when sending message!";
        }
        else
        {
            if (session->sendMessage(ref) && mTimer)
            {
                mTimer->mSession = session;
                //TODO: store ref rather than sn for performance
                mTimer->mMsgSn = mSn;
                mTimer->attach(CFdbContext::getInstance());
            }
        }
    }
    else
    {
        success = false;
        reason = "Invalid sid!";
    }
    
    if (!success)
    {
        if (mFlag & MSG_FLAG_SYNC_REPLY)
        {
            setErrorMsg(FDB_MT_UNKNOWN, NFdbBase::FDB_ST_INVALID_ID, reason);
        }
        else
        {
            onAsyncError(ref, NFdbBase::FDB_ST_INVALID_ID, reason);
        }
    }
}


void CFdbMessage::doReply(Ptr &ref)
{
    if (!(mFlag & MSG_FLAG_NOREPLY_EXPECTED))
    {
        auto session = getSession();
        if (session)
        {
            session->sendMessage(this);
        }
    }
}

void CFdbMessage::doBroadcast(Ptr &ref)
{
    bool success = true;
    const char *reason = "";
    if (mFlag & MSG_FLAG_ENDPOINT)
    {
        auto endpoint = CFdbContext::getInstance()->getEndpoint(mEpid);
        if (endpoint)
        {
            auto object = endpoint->getObject(this, true);
            if (object)
            {
                // broadcast to all sessions of the object
                object->broadcast(this);
            }
            else
            {
                success = false;
                reason = "Invalid object id!";
            }
        }
        else
        {
            success = false;
            reason = "Invalid epid!";
        }
    }
    else
    {
        mFlag |= MSG_FLAG_INITIAL_RESPONSE; // mark as initial response
        auto session = CFdbContext::getInstance()->getSession(mSid);
        if (session)
        {
            auto object =
                    session->container()->owner()->getObject(this, true);
            if (object)
            {
                // Broadcast to specified session of object
                if (!object->broadcast(this, session))
                {
                    success = false;
                    reason = "Not subscribed or fail to send!";
                }
            }
            else
            {
                success = false;
                reason = "Invalid object id!";
            }
        }
        else
        {
            success = false;
            reason = "Invalid sid!";
        }
    }
    if (!success)
    {
        onAsyncError(ref, NFdbBase::FDB_ST_INVALID_ID, reason);
    }
}

void CFdbMessage::doStatus(Ptr &ref)
{
    doReply(ref);
}

void CFdbMessage::doSubscribeReq(Ptr &ref)
{
    doRequest(ref);
}

void CFdbMessage::doUnsubscribeReq(Ptr &ref)
{
    doRequest(ref);
}

void CFdbMessage::setErrorMsg(EFdbMessageType type, int32_t error_code, const char *description)
{
    if (type != FDB_MT_UNKNOWN)
    {
        mType = type;
    }

    if ((error_code < NFdbBase::FDB_ST_AUTO_REPLY_OK) || (error_code > NFdbBase::FDB_ST_OK))
    {
        mFlag |= MSG_FLAG_ERROR;
    }

    mFlag |= (MSG_FLAG_STATUS | MSG_FLAG_REPLIED);

    NFdbBase::FdbMsgErrorInfo error_info;
    error_info.set_error_code(error_code);
    if (description)
    {
        error_info.set_description(description);
    }
    CFdbParcelableBuilder builder(error_info);
    serialize(builder);
}

void CFdbMessage::sendStatus(CFdbSession *session, int32_t error_code, const char *description)
{
    if (!(mFlag & MSG_FLAG_NOREPLY_EXPECTED))
    {
        setErrorMsg(FDB_MT_STATUS, error_code, description);
        session->sendMessage(this);
    }
}

void CFdbMessage::sendAutoReply(CFdbSession *session, int32_t error_code, const char *description)
{
    if ((mFlag & (MSG_FLAG_AUTO_REPLY | MSG_FLAG_REPLIED)) == MSG_FLAG_AUTO_REPLY)
    {
        sendStatus(session, error_code, description);
    }
}

void CFdbMessage::autoReply(CFdbSession *session
                            , CBaseJob::Ptr &msg_ref
                            , int32_t error_code
                            , const char *description)
{
    auto fdb_msg = castToMessage<CFdbMessage *>(msg_ref);
    if (((fdb_msg->mFlag & (MSG_FLAG_AUTO_REPLY | MSG_FLAG_REPLIED)) == MSG_FLAG_AUTO_REPLY)
            && !(fdb_msg->mFlag & MSG_FLAG_NOREPLY_EXPECTED) && msg_ref.unique())
    {
        fdb_msg->sendStatus(session, error_code, description);
    }
}

void CFdbMessage::autoReply(CBaseJob::Ptr &msg_ref, int32_t error_code, const char *description)
{
    auto fdb_msg = castToMessage<CFdbMessage *>(msg_ref);
    if (((fdb_msg->mFlag & (MSG_FLAG_AUTO_REPLY | MSG_FLAG_REPLIED)) == MSG_FLAG_AUTO_REPLY)
            && !(fdb_msg->mFlag & MSG_FLAG_NOREPLY_EXPECTED) && msg_ref.unique())
    {
        auto fdb_msg = castToMessage<CFdbMessage *>(msg_ref);
        fdb_msg->setErrorMsg(FDB_MT_STATUS, error_code, description);
        CFdbContext::getInstance()->sendAsync(msg_ref);
    }
}

void CFdbMessage::parseTimestamp(const CFdbMsgMetadata &metadata
                               , uint64_t &client_to_server
                               , uint64_t &server_to_reply
                               , uint64_t &reply_to_client
                               , uint64_t &total)
{
    CNanoTimer timer;
    if (!metadata.mSendTime || !metadata.mArriveTime)
    {
        client_to_server = 0;
    }
    else
    {
        timer.start(metadata.mSendTime);
        client_to_server = timer.snapshotMicroseconds(metadata.mArriveTime);
    }

    if (!metadata.mArriveTime || !metadata.mReplyTime)
    {
        server_to_reply = 0;
    }
    else
    {
        timer.reset();
        timer.start(metadata.mArriveTime);
        server_to_reply = timer.snapshotMicroseconds(metadata.mReplyTime);
    }

    if (!metadata.mReplyTime || !metadata.mReceiveTime)
    {
        reply_to_client = 0;
    }
    else
    {
        timer.reset();
        timer.start(metadata.mReplyTime);
        reply_to_client = timer.snapshotMicroseconds(metadata.mReceiveTime);
    }

    if (!metadata.mSendTime || !metadata.mReceiveTime)
    {
        total = 0;
    }
    else
    {
        timer.reset();
        timer.start(metadata.mSendTime);
        total = timer.snapshotMicroseconds(metadata.mReceiveTime);
    }
}

bool CFdbMessage::decodeStatus(int32_t &error_code, std::string &description)
{
    NFdbBase::FdbMsgErrorInfo error_msg;
    CFdbParcelableParser parser(error_msg);
    bool ret = deserialize(parser);
    if (ret)
    {
        error_code = error_msg.error_code();
        description = error_msg.description();
    }
    return ret;
}

void CFdbMessage::update(NFdbBase::CFdbMessageHeader &head, CFdbMessage::CFdbMsgPrefix &prefix)
{
    mFlag = (mFlag & ~MSG_GLOBAL_FLAG_MASK) | (head.flag() & MSG_GLOBAL_FLAG_MASK);
    //mCode = head.code();
}

CFdbSession *CFdbMessage::getSession()
{
    CFdbSession *session;
    if (mFlag & MSG_FLAG_ENDPOINT)
    {
        auto endpoint = CFdbContext::getInstance()->getEndpoint(mEpid);
        session = endpoint ? endpoint->preferredPeer() : 0;
        if (session)
        {
            mFlag &= ~MSG_FLAG_ENDPOINT;
            mSid = session->sid();
        }
    }
    else
    {
        session = CFdbContext::getInstance()->getSession(mSid);
    }
    return session;
}

bool CFdbMessage::deserialize(IFdbMsgParser &payload) const
{
    if (!mBuffer || !mPayloadSize)
    {
        return false;
    }
    return payload.parse(mBuffer + mOffset + mPrefixSize + mHeadSize, mPayloadSize);
}

const char *CFdbMessage::getMsgTypeName(EFdbMessageType type)
{
    static const char *type_name[] = {"Unknown"
                                    , "Request"
                                    , "Reply"
                                    , "Subscribe"
                                    , "Broadcast"
                                    , "SidebandRequest"
                                    , "SidebandReply"
                                    , "Status"};
    if (type >= FDB_MT_MAX)
    {
        return 0;
    }
    return type_name[type];
}

void CFdbMessage::setLogData(const char *log_data)
{
    if (log_data)
    {
        mStringData = log_data;
    }
}

void CFdbMessage::checkLogEnabled(const CFdbBaseObject *object, bool lock)
{
    if (!(mFlag & MSG_FLAG_ENABLE_LOG))
    {
        CLogProducer *logger = CFdbContext::getInstance()->getLogger();
        if (logger && logger->checkLogEnabled(mType, 0, object->endpoint(), lock))
        {
            mFlag |= MSG_FLAG_ENABLE_LOG;
        }
    }
}

void CFdbMessage::encodeDebugInfo(NFdbBase::CFdbMessageHeader &msg_hdr, CFdbSession *session)
{
#if defined(CONFIG_FDB_MESSAGE_METADATA)
    switch (msg_hdr.type())
    {
        case FDB_MT_REPLY:
        case FDB_MT_STATUS:
            msg_hdr.set_send_or_arrive_time(mArriveTime);
            msg_hdr.set_reply_time(CNanoTimer::getNanoSecTimer());
            break;
        case FDB_MT_REQUEST:
        case FDB_MT_SUBSCRIBE_REQ:
        case FDB_MT_BROADCAST:
            mSendTime = CNanoTimer::getNanoSecTimer();
            msg_hdr.set_send_or_arrive_time(mSendTime);
            break;
        default:
            break;
    }
#endif
}

void CFdbMessage::decodeDebugInfo(NFdbBase::CFdbMessageHeader &msg_hdr, CFdbSession *session)
{
#if defined(CONFIG_FDB_MESSAGE_METADATA)
    switch (msg_hdr.type())
    {
        case FDB_MT_REPLY:
        case FDB_MT_STATUS:
            if (msg_hdr.has_send_or_arrive_time())
            {
                mArriveTime = msg_hdr.send_or_arrive_time();
            }
            if (msg_hdr.has_reply_time())
            {
                mReplyTime = msg_hdr.reply_time();
            }
            mReceiveTime = CNanoTimer::getNanoSecTimer();
            break;
        case FDB_MT_REQUEST:
        case FDB_MT_SUBSCRIBE_REQ:
        case FDB_MT_BROADCAST:
            mArriveTime = CNanoTimer::getNanoSecTimer();
            if (msg_hdr.has_send_or_arrive_time())
            {
                mSendTime = msg_hdr.send_or_arrive_time();
            }
            break;
        default:
            break;
    }
#endif
}

void CFdbMessage::metadata(CFdbMsgMetadata &metadata)
{
#if defined(CONFIG_FDB_MESSAGE_METADATA)
    metadata.mSendTime = mSendTime;
    metadata.mArriveTime = mArriveTime;
    metadata.mReplyTime = mReplyTime;
    metadata.mReceiveTime = mReceiveTime;
#else
    metadata.mSendTime = 0;
    metadata.mArriveTime = 0;
    metadata.mReplyTime = 0;
    metadata.mReceiveTime = 0;
#endif
}

bool CFdbMessage::invokeSideband(int32_t timeout)
{
    CBaseJob::Ptr msg_ref(this);
    mType = FDB_MT_SIDEBAND_REQUEST;
    return submit(msg_ref, 0, timeout);
}

bool CFdbMessage::sendSideband()
{
    CBaseJob::Ptr msg_ref(this);
    mType = FDB_MT_SIDEBAND_REQUEST;
    return submit(msg_ref, FDB_MSG_TX_NO_REPLY, -1);
}

bool CFdbMessage::replySideband(CBaseJob::Ptr &msg_ref, IFdbMsgBuilder &data)
{
    auto fdb_msg = castToMessage<CFdbMessage *>(msg_ref);
    if (fdb_msg->mFlag & MSG_FLAG_NOREPLY_EXPECTED)
    {
        return false;
    }
    if (!fdb_msg->serialize(data))
    {
        return false;
    }
    return fdb_msg->feedback(msg_ref, FDB_MT_SIDEBAND_REPLY);
}

bool CFdbMessage::replySideband(CBaseJob::Ptr &msg_ref, const void *buffer, int32_t size)
{
    auto fdb_msg = castToMessage<CFdbMessage *>(msg_ref);
    if (fdb_msg->mFlag & MSG_FLAG_NOREPLY_EXPECTED)
    {
        return false;
    }
    if (!fdb_msg->serialize((uint8_t *)buffer, size))
    {
        return false;
    }
    return fdb_msg->feedback(msg_ref, FDB_MT_SIDEBAND_REPLY);
}
