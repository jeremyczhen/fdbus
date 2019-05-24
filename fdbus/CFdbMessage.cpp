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
#include <common_base/CFdbMessage.h>
#include <common_base/CBaseEndpoint.h>
#include <common_base/CFdbContext.h>
#include <common_base/CNanoTimer.h>
#include <common_base/CFdbSession.h>
#include <common_base/CLogProducer.h>
#include <common_base/CFdbBaseObject.h>
#include <idl-gen/common.base.MessageHeader.pb.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/coded_stream.h>
#include <utils/Log.h>

#define FDB_MSG_TX_SYNC         (1 << 0)
#define FDB_MSG_TX_NO_REPLY     (1 << 1)

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
    : mType(NFdbBase::MT_UNKNOWN)
    , mCode(code)
    , mSn(FDB_INVALID_ID)
    , mPayloadSize(0)
    , mHeadSize(0)
    , mOffset(0)
    , mExtraSize(0)
    , mSid(FDB_INVALID_ID)
    , mOid(FDB_INVALID_ID)
    , mBuffer(0)
    , mFlag(0)
    , mTimer(0)
    , mStringData(0)
{
}

CFdbMessage::CFdbMessage(FdbMsgCode_t code, CFdbBaseObject *obj, FdbSessionId_t alt_receiver)
    : mType(NFdbBase::MT_UNKNOWN)
    , mCode(code)
    , mSn(FDB_INVALID_ID)
    , mPayloadSize(0)
    , mHeadSize(0)
    , mOffset(0)
    , mExtraSize(0)
    , mBuffer(0)
    , mFlag(0)
    , mTimer(0)
    , mStringData(0)
{
    setDestination(obj, alt_receiver);
}

CFdbMessage::CFdbMessage(FdbMsgCode_t code, CFdbMessage *msg)
    : mType(NFdbBase::MT_UNKNOWN)
    , mCode(code)
    , mSn(msg->mSn)
    , mPayloadSize(0)
    , mHeadSize(0)
    , mOffset(0)
    , mExtraSize(0)
    , mSid(msg->mSid)
    , mOid(msg->mOid)
    , mBuffer(0)
    , mFlag(0)
    , mTimer(0)
    , mSenderName(msg->mSenderName)
    , mStringData(0)
{
}

CFdbMessage::CFdbMessage(NFdbBase::FdbMessageHeader &head
                         , CFdbMsgPrefix &prefix
                         , uint8_t *buffer
                         , FdbSessionId_t sid
                        )
    : mType(head.type())
    , mCode(head.code())
    , mSn(head.serial_number())
    , mPayloadSize(head.payload_size())
    , mHeadSize(prefix.mHeadLength)
    , mOffset(0)
    , mExtraSize(prefix.mTotalLength - mPrefixSize - mHeadSize - mPayloadSize)
    , mSid(sid)
    , mOid(head.object_id())
    , mBuffer(buffer)
    , mFlag((head.flag() & MSG_GLOBAL_FLAG_MASK) | MSG_FLAG_EXTERNAL_BUFFER)
    , mTimer(0)
    , mStringData(0)
{
    if (mExtraSize < 0)
    {
        mExtraSize = 0;
        LOG_E("CFdbMessage: mExtraSize is less than 0: %d %d %d\n",
                prefix.mTotalLength, mHeadSize, mPayloadSize);
    }
    if (head.has_sender_name())
    {
        mSenderName = head.sender_name().c_str();
    }
};

CFdbMessage::~CFdbMessage()
{
    if (mTimer)
    {
        delete mTimer;
        mTimer = 0;
    }
    releaseBuffer();
    if (mStringData)
    {
        delete mStringData;
        mStringData = 0;
    }
    //LOG_I("Message %d is destroyed!\n", (int32_t)mSn);
}

void CFdbMessage::setDestination(CFdbBaseObject *obj, FdbSessionId_t alt_sid)
{
    FdbSessionId_t sid = obj->getDefaultSession();
    if (isValidFdbId(alt_sid))
    {
        mSid = alt_sid;
        mFlag &= ~MSG_FLAG_ENDPOINT;
    }
    else if (isValidFdbId(sid))
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
    mSenderName = obj->name();
}

void CFdbMessage::run(CBaseWorker *worker, Ptr &ref)
{
    switch (mType)
    {
        case NFdbBase::MT_REQUEST:
        case NFdbBase::MT_SIDEBAND_REQUEST:
            doRequest(ref);
            break;
        case NFdbBase::MT_REPLY:
        case NFdbBase::MT_SIDEBAND_REPLY:
            doReply(ref);
            break;
        case NFdbBase::MT_BROADCAST:
            doBroadcast(ref);
            break;
        case NFdbBase::MT_STATUS:
            doStatus(ref);
            break;
        case NFdbBase::MT_SUBSCRIBE_REQ:
            if (mCode == FDB_CODE_SUBSCRIBE)
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

void CFdbMessage::feedback(CBaseJob::Ptr &msg_ref, CFdbMsgPayload &payload, int32_t size)
{
    if (mFlag & MSG_FLAG_NOREPLY_EXPECTED)
    {
        return;
    }
    if (!serialize(payload, size))
    {
        return;
    }
    mFlag |= MSG_FLAG_REPLIED;
    if (!CFdbContext::getInstance()->sendAsyncEndeavor(msg_ref))
    {
        mFlag &= ~MSG_FLAG_REPLIED;
    }
}

void CFdbMessage::reply(CBaseJob::Ptr &msg_ref, const CFdbBasePayload &data)
{
    CFdbMessage *fdb_msg = castToMessage<CFdbMessage *>(msg_ref);
    CFdbMsgPayload payload;
    payload.mMessage = &data;
    fdb_msg->mType = NFdbBase::MT_REPLY;
    fdb_msg->feedback(msg_ref, payload, -1);
}

void CFdbMessage::reply(CBaseJob::Ptr &msg_ref, const void *buffer, int32_t size)
{
    CFdbMessage *fdb_msg = castToMessage<CFdbMessage *>(msg_ref);
    CFdbMsgPayload payload;
    payload.mBuffer = (uint8_t *)buffer;
    fdb_msg->mType = NFdbBase::MT_REPLY;
    fdb_msg->feedback(msg_ref, payload, size);
}

void CFdbMessage::status(CBaseJob::Ptr &msg_ref, int32_t error_code, const char *description)
{
    CFdbMessage *fdb_msg = castToMessage<CFdbMessage *>(msg_ref);
    if (fdb_msg->mFlag & MSG_FLAG_NOREPLY_EXPECTED)
    {
        return;
    }
    fdb_msg->setErrorMsg(NFdbBase::MT_STATUS, error_code, description);
    CFdbContext::getInstance()->sendAsyncEndeavor(msg_ref);
}

void CFdbMessage::submit(CBaseJob::Ptr &msg_ref
                         , CFdbMsgPayload &payload
                         , int32_t size
                         , uint32_t tx_flag
                         , int32_t timeout)
{
    bool sync = !!(tx_flag & FDB_MSG_TX_SYNC);
    if (sync && FDB_CONTEXT->isSelf())
    {
        setErrorMsg(NFdbBase::MT_UNKNOWN, NFdbBase::FDB_ST_DEAD_LOCK,
                    "Cannot send sychronously from FDB_CONTEXT!");
        return;
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

    if (!serialize(payload, size))
    {
        if (sync)
        {
            setErrorMsg(NFdbBase::MT_UNKNOWN, NFdbBase::FDB_ST_UNABLE_TO_SEND,
                        "Fail to serialize payload!");
        }
        else
        {
            onAsyncError(msg_ref, NFdbBase::FDB_ST_UNABLE_TO_SEND, "Fail to serialize payload!");
        }
        return;
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
        if (sync)
        {
            setErrorMsg(NFdbBase::MT_UNKNOWN, NFdbBase::FDB_ST_UNABLE_TO_SEND,
                        "Fail to send job to FDB_CONTEXT!");
        }
        else
        {
            onAsyncError(msg_ref, NFdbBase::FDB_ST_UNABLE_TO_SEND, "Fail to send job to FDB_CONTEXT!");
        }
    }
}

void CFdbMessage::invoke(CBaseJob::Ptr &msg_ref
                         , CFdbMsgPayload &payload
                         , int32_t size
                         , uint32_t tx_flag
                         , int32_t timeout)
{
    mType = NFdbBase::MT_REQUEST;
    submit(msg_ref, payload, size, tx_flag, timeout);
}

void CFdbMessage::invoke(const CFdbBasePayload &data
                         , int32_t timeout)
{
    CFdbMsgPayload payload;
    payload.mMessage = &data;
    CBaseJob::Ptr msg_ref(this);
    invoke(msg_ref, payload, -1, 0, timeout);
}

void CFdbMessage::invoke(CBaseJob::Ptr &msg_ref
                         , const CFdbBasePayload &data
                         , int32_t timeout)
{
    CFdbMessage *msg = castToMessage<CFdbMessage *>(msg_ref);
    if (msg)
    {
        CFdbMsgPayload payload;
        payload.mMessage = &data;
        msg->invoke(msg_ref, payload, -1, FDB_MSG_TX_SYNC, timeout);
    }
}

void CFdbMessage::invoke(const void *buffer
                         , int32_t size
                         , int32_t timeout)
{
    CFdbMsgPayload payload;
    payload.mBuffer = (uint8_t *)buffer;
    CBaseJob::Ptr msg_ref(this);
    invoke(msg_ref, payload, size, 0, timeout);
}

void CFdbMessage::invoke(CBaseJob::Ptr &msg_ref
                         , const void *buffer
                         , int32_t size
                         , int32_t timeout)
{
    CFdbMessage *msg = castToMessage<CFdbMessage *>(msg_ref);
    if (msg)
    {
        CFdbMsgPayload payload;
        payload.mBuffer = (uint8_t *)buffer;
        msg->invoke(msg_ref, payload, size, FDB_MSG_TX_SYNC, timeout);
    }
}
void CFdbMessage::send(const CFdbBasePayload &data)
{
    CFdbMsgPayload payload;
    payload.mMessage = &data;
    CBaseJob::Ptr msg_ref(this);
    invoke(msg_ref, payload, -1, FDB_MSG_TX_NO_REPLY, -1);
}

void CFdbMessage::send(const void *buffer
                       , int32_t size)
{
    CFdbMsgPayload payload;
    payload.mBuffer = (uint8_t *)buffer;
    CBaseJob::Ptr msg_ref(this);
    invoke(msg_ref, payload, size, FDB_MSG_TX_NO_REPLY, -1);
}

void CFdbMessage::sendLog(const CFdbBasePayload &data
                           , uint8_t *log_data
                           , int32_t size
                           , int32_t clipped_size
                           , bool send_as_job)
{
    CFdbMsgPayload payload;
    payload.mMessage = &data;
    mFlag |= MSG_FLAG_NOREPLY_EXPECTED;
    mType = NFdbBase::MT_REQUEST;

    if (clipped_size < 0)
    {
        mExtraSize = size;
    }
    else
    {
        CFdbMsgPrefix prefix;
        prefix.deserialize(log_data);
        int32_t payload_size = prefix.mTotalLength - prefix.mHeadLength - mPrefixSize;
        if (clipped_size == 0)
        {
            prefix.mTotalLength = mPrefixSize + prefix.mHeadLength;
            mExtraSize = prefix.mTotalLength;
            prefix.serialize(log_data);
        }
        else if (clipped_size < payload_size)
        {
            prefix.mTotalLength = mPrefixSize + prefix.mHeadLength + clipped_size;
            mExtraSize = prefix.mTotalLength;
            prefix.serialize(log_data);
        }
        else
        {
            mExtraSize = size;
        }
    }

    if (!serialize(payload, -1))
    {
        return;
    }

    if (log_data && size)
    {
        memcpy(getExtraBuffer(), log_data, mExtraSize);
    }

    if (send_as_job)
    {
        CFdbContext::getInstance()->sendAsync(this);
    }
    else
    {
        CFdbSession *session = getSession();
        if (session)
        {
            session->sendMessage(this);
        }
    }
}

void CFdbMessage::broadcastLog(const CFdbBasePayload &data
                                , const uint8_t *log_data
                                , int32_t size
                                , bool send_as_job)
{
    CFdbMsgPayload payload;
    payload.mMessage = &data;
    mType = NFdbBase::MT_BROADCAST;
    mExtraSize = size;
    if (!serialize(payload, -1))
    {
        return;
    }

    if (log_data && size)
    {
        memcpy(getExtraBuffer(), log_data, mExtraSize);
    }

    if (send_as_job)
    {
        CFdbContext::getInstance()->sendAsyncEndeavor(this);
    }
    else
    {
        CBaseEndpoint *endpoint = CFdbContext::getInstance()->getEndpoint(mEpid);
        if (endpoint)
        {
            // Broadcast per object!!!
            CFdbBaseObject *object = endpoint->getObject(this, false);
            if (object)
            {
                object->broadcast(this);
            }
        }
    }
}

CFdbMessage *CFdbMessage::parseFdbLog(uint8_t *buffer, int32_t size)
{
    if (!buffer || !size)
    {
        return 0;
    }

    CFdbMessage::CFdbMsgPrefix prefix;
    prefix.deserialize(buffer);
    
    NFdbBase::FdbMessageHeader head;
    if (!CFdbMessage::deserializePb(head,
                                    buffer + CFdbMessage::mPrefixSize,
                                    prefix.mHeadLength))
    {
        LOG_E("CFdbMessage: Unable to deserialize log header!\n");
        return 0;
    }
    int32_t payload_size = head.payload_size();
    int32_t extra_size = prefix.mTotalLength - mPrefixSize - prefix.mHeadLength - payload_size;
    if (extra_size < 0)
    {
        payload_size += extra_size;
        if (payload_size < 0)
        {
            return 0;
        }
        head.set_payload_size(payload_size);
    }
    return new CFdbMessage(head, prefix, buffer, session());
}

void CFdbMessage::yell(CFdbMsgPayload &payload, int32_t size)
{
    mType = NFdbBase::MT_BROADCAST;
    serialize(payload, size);
    CFdbContext::getInstance()->sendAsyncEndeavor(this);
}

void CFdbMessage::broadcast(const CFdbBasePayload &msg)
{
    CFdbMsgPayload payload;
    payload.mMessage = &msg;
    yell(payload, -1);
}

void CFdbMessage::broadcast(FdbMsgCode_t code
                           , const CFdbBasePayload &data
                           , const char *filter)
{
    CBaseMessage *msg = new CFdbBroadcastMsg(code, this, filter);
    msg->broadcast(data);
}

void CFdbMessage::broadcast(FdbMsgCode_t code
                           , const char *filter
                           , const void *buffer
                           , int32_t size)
{
    CBaseMessage *msg = new CFdbBroadcastMsg(code, this, filter);
    msg->broadcast(buffer, size);
}

void CFdbMessage::broadcast(const void *buffer
                            , int32_t size)
{
    CFdbMsgPayload payload;
    payload.mBuffer = (uint8_t *)buffer;
    yell(payload, size);
}

void CFdbMessage::subscribe(CBaseJob::Ptr &msg_ref
                            , NFdbBase::FdbMsgSubscribe &msg_list
                            , uint32_t tx_flag
                            , FdbMsgCode_t subscribe_code
                            , int32_t timeout)
{
    mType = NFdbBase::MT_SUBSCRIBE_REQ;
    mCode = subscribe_code;
    CFdbMsgPayload payload;
    payload.mMessage = &msg_list;
    submit(msg_ref, payload, -1, tx_flag, timeout);
}

void CFdbMessage::subscribe(NFdbBase::FdbMsgSubscribe &msg_list
                            , int32_t timeout)
{
    CBaseJob::Ptr msg_ref(this);
    subscribe(msg_ref, msg_list, 0, FDB_CODE_SUBSCRIBE, timeout);
}

void CFdbMessage::subscribe(CBaseJob::Ptr &msg_ref
                            , NFdbBase::FdbMsgSubscribe &msg_list
                            , int32_t timeout)
{
    CFdbMessage *msg = castToMessage<CFdbMessage *>(msg_ref);
    if (msg)
    {
        msg->subscribe(msg_ref, msg_list, FDB_MSG_TX_SYNC, FDB_CODE_SUBSCRIBE, timeout);
    }
}

void CFdbMessage::unsubscribe(NFdbBase::FdbMsgSubscribe &msg_list)
{
    CBaseJob::Ptr msg_ref(this);
    subscribe(msg_ref, msg_list, FDB_MSG_TX_NO_REPLY, FDB_CODE_UNSUBSCRIBE, 0);
}

bool CFdbMessage::buildHeader(CFdbSession *session)
{
    if (mFlag & MSG_FLAG_HEAD_OK)
    {
        return true;
    }
    NFdbBase::FdbMessageHeader msg_hdr;
    msg_hdr.set_type((NFdbBase::FdbMessageType)mType);
    msg_hdr.set_serial_number(mSn);
    msg_hdr.set_code(mCode);
    msg_hdr.set_flag(mFlag & MSG_GLOBAL_FLAG_MASK);
    msg_hdr.set_object_id(mOid);
    msg_hdr.set_payload_size(mPayloadSize);

    encodeDebugInfo(msg_hdr, session);
    if (mSenderName.empty())
    {
        msg_hdr.set_sender_name(session->getEndpointName());
    }
    else
    {
        msg_hdr.set_sender_name(mSenderName);
    }

    if (mType == NFdbBase::MT_BROADCAST)
    {
        const char *filter = getFilter();
        if (filter && (filter[0] != '\0'))
        {
            msg_hdr.set_broadcast_filter(filter);
        }
    }

    int32_t head_size = msg_hdr.ByteSize();
    if (head_size > mMaxHeadSize)
    {
        LOG_E("CFdbMessage: Message %d of Session %d: Head is too long!\n", (int32_t)mCode, (int32_t)mSid);
        return false;
    }
    mHeadSize = head_size;
    int32_t head_offset = maxReservedSize() - head_size;
    int32_t prefix_offset = head_offset - mPrefixSize;
    mOffset = prefix_offset;

    try
    {
        google::protobuf::io::ArrayOutputStream aos(mBuffer + head_offset, head_size);
        google::protobuf::io::CodedOutputStream coded_output(&aos);
        if (!msg_hdr.SerializeToCodedStream(&coded_output))
        {
            LOG_E("CFdbMessage: Unable to serialize head!\n");
            return false;
        }
    }
    catch (...)
    {
        LOG_E("CFdbMessage: Unable to serialize head!\n");
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

bool CFdbMessage::allocCopyRawBuffer(const uint8_t *src, int32_t payload_size)
{
    int32_t total_size = maxReservedSize() + payload_size + mExtraSize;
    mBuffer = new uint8_t[total_size];
    if (src)
    {
        memcpy(mBuffer +  maxReservedSize(), src, payload_size);
    }
    return true;
}

bool CFdbMessage::serialize(CFdbMsgPayload &payload, int32_t payload_size)
{
    mOffset = 0;
    mHeadSize = mMaxHeadSize;

    releaseBuffer();

    if (payload_size < 0)
    {
        if (payload.mMessage)
        {
            try
            {
                mFlag &= ~(MSG_FLAG_RAW_DATA | MSG_FLAG_EXTERNAL_BUFFER);
                mPayloadSize = payload.mMessage->ByteSize();
                mBuffer = new uint8_t[maxReservedSize() + mPayloadSize + mExtraSize];
                google::protobuf::io::ArrayOutputStream aos(
                    mBuffer + maxReservedSize(), mPayloadSize);
                google::protobuf::io::CodedOutputStream coded_output(&aos);
                if (!payload.mMessage->SerializeToCodedStream(&coded_output))
                {
                    LOG_E("CFdbMessage: Unable to serialize message!\n");
                    return false;
                }
            }
            catch (...)
            {
                LOG_E("CFdbMessage: Unable to serialize message!\n");
                return false;
            }

            CLogProducer *logger = CFdbContext::getInstance()->getLogger();
            if (logger)
            {
                logger->printToString(this, *payload.mMessage);
            }
        }
    }
    else
    {
        mFlag |= MSG_FLAG_RAW_DATA | MSG_FLAG_EXTERNAL_BUFFER;
        mPayloadSize = payload_size;
        return allocCopyRawBuffer(payload.mBuffer, mPayloadSize);
    }
    return true;
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
    CFdbSession *session = getSession();
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
            setErrorMsg(NFdbBase::MT_UNKNOWN, NFdbBase::FDB_ST_INVALID_ID, reason);
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
        CFdbSession *session = getSession();
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
        CBaseEndpoint *endpoint = CFdbContext::getInstance()->getEndpoint(mEpid);
        if (endpoint)
        {
            CFdbBaseObject *object = endpoint->getObject(this, false);
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
        mFlag |= MSG_FLAG_FORCE_BROADCAST; // broadcast anyway; jeremy
        mFlag |= MSG_FLAG_INITIAL_RESPONSE; // mark as initial response
        CFdbSession *session = CFdbContext::getInstance()->getSession(mSid);
        if (session)
        {
            //if (mFlag & MSG_FLAG_FORCE_BROADCAST)
            if (0)
            {
                success = session->sendMessage(this);
                reason = "error when sending message!";
            }
            else
            {
                CFdbBaseObject *object =
                        session->container()->owner()->getObject(this, false);
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

void CFdbMessage::setErrorMsg(NFdbBase::wrapper::FdbMessageType type, int32_t error_code, const char *description)
{
    if (type != NFdbBase::MT_UNKNOWN)
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
    CFdbMsgPayload payload;
    payload.mMessage = &error_info;
    serialize(payload, -1);
}

void CFdbMessage::sendStatus(CFdbSession *session, int32_t error_code, const char *description)
{
    if (!(mFlag & MSG_FLAG_NOREPLY_EXPECTED))
    {
        setErrorMsg(NFdbBase::MT_STATUS, error_code, description);
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
    CFdbMessage *fdb_msg = castToMessage<CFdbMessage *>(msg_ref);
    if (((fdb_msg->mFlag & (MSG_FLAG_AUTO_REPLY | MSG_FLAG_REPLIED)) == MSG_FLAG_AUTO_REPLY)
            && !(fdb_msg->mFlag & MSG_FLAG_NOREPLY_EXPECTED)
            && (msg_ref.use_count() == 1))
    {
        fdb_msg->sendStatus(session, error_code, description);
    }
}

void CFdbMessage::autoReply(CBaseJob::Ptr &msg_ref, int32_t error_code, const char *description)
{
    CFdbMessage *fdb_msg = castToMessage<CFdbMessage *>(msg_ref);
    if (((fdb_msg->mFlag & (MSG_FLAG_AUTO_REPLY | MSG_FLAG_REPLIED)) == MSG_FLAG_AUTO_REPLY)
            && !(fdb_msg->mFlag & MSG_FLAG_NOREPLY_EXPECTED)
            && (msg_ref.use_count() == 1))
    {
        CFdbMessage *fdb_msg = castToMessage<CFdbMessage *>(msg_ref);
        fdb_msg->setErrorMsg(NFdbBase::MT_STATUS, error_code, description);
        CFdbContext::getInstance()->sendAsyncEndeavor(msg_ref);
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
        timer.startTimer(metadata.mSendTime);
        timer.stopTimer(metadata.mArriveTime);
        client_to_server = timer.getTotalMicroseconds();
    }

    if (!metadata.mArriveTime || !metadata.mReplyTime)
    {
        server_to_reply = 0;
    }
    else
    {
        timer.reset();
        timer.startTimer(metadata.mArriveTime);
        timer.stopTimer(metadata.mReplyTime);
        server_to_reply = timer.getTotalMicroseconds();
    }

    if (!metadata.mReplyTime || !metadata.mReceiveTime)
    {
        reply_to_client = 0;
    }
    else
    {
        timer.reset();
        timer.startTimer(metadata.mReplyTime);
        timer.stopTimer(metadata.mReceiveTime);
        reply_to_client = timer.getTotalMicroseconds();
    }

    if (!metadata.mSendTime || !metadata.mReceiveTime)
    {
        total = 0;
    }
    else
    {
        timer.reset();
        timer.startTimer(metadata.mSendTime);
        timer.stopTimer(metadata.mReceiveTime);
        total = timer.getTotalMicroseconds();
    }
}

bool CFdbMessage::decodeStatus(int32_t &error_code, std::string &description)
{
    NFdbBase::FdbMsgErrorInfo error_msg;
    bool ret = deserialize(error_msg);
    if (ret)
    {
        error_code = error_msg.error_code();
        description = error_msg.description();
    }
    return ret;
}

void CFdbMessage::update(NFdbBase::FdbMessageHeader &head, CFdbMessage::CFdbMsgPrefix &prefix)
{
    mFlag = (mFlag & ~MSG_GLOBAL_FLAG_MASK) | (head.flag() & MSG_GLOBAL_FLAG_MASK);
    //mCode = head.code();
}

CFdbSession *CFdbMessage::getSession()
{
    CFdbSession *session;
    if (mFlag & MSG_FLAG_ENDPOINT)
    {
        CBaseEndpoint *endpoint = CFdbContext::getInstance()->getEndpoint(mEpid);
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

bool CFdbMessage::deserialize(CFdbBasePayload &payload) const
{
    if (!mBuffer || (mFlag & MSG_FLAG_RAW_DATA))
    {
        return false;
    }
    return deserializePb(payload
                         , mBuffer + mOffset + mPrefixSize + mHeadSize
                         , mPayloadSize);
}

bool CFdbMessage::deserializePb(CFdbBasePayload &payload, void *buffer, int32_t size)
{
    bool ret;
    if (!size)
    {
        ret = true;
    }
    else if (buffer)
    {
        try
        {
            //Assign ArrayInputStream with enough memory
            google::protobuf::io::ArrayInputStream ais(buffer, size);
            google::protobuf::io::CodedInputStream coded_input(&ais);
            google::protobuf::io::CodedInputStream::Limit msgLimit = coded_input.PushLimit(size);
            // De-Serialize
            ret = payload.ParseFromCodedStream(&coded_input);
            //Once the embedded message has been parsed, PopLimit() is called to undo the limit
            coded_input.PopLimit(msgLimit);
        }
        catch (...)
        {
            ret = false;
        }
    }
    else
    {
        ret = false;
    }
    
    return ret;
}

const char *CFdbMessage::getMsgTypeName(NFdbBase::wrapper::FdbMessageType type)
{
    static const char *type_name[] = {"Unknown"
                                    , "Request"
                                    , "Reply"
                                    , "Subscribe"
                                    , "Broadcast"
                                    , "SidebandRequest"
                                    , "SidebandReply"
                                    , "Status"};
    if (type >= NFdbBase::MT_MAX)
    {
        return 0;
    }
    return type_name[type];
}

bool CFdbMessage::isSubscribe()
{
    return (mType == NFdbBase::MT_SUBSCRIBE_REQ) && (mCode == FDB_CODE_SUBSCRIBE);
}

CFdbDebugMsg::CFdbDebugMsg(FdbMsgCode_t code)
    : CFdbMessage(code)
    , mSendTime(0)
    , mArriveTime(0)
    , mReplyTime(0)
    , mReceiveTime(0)
{
    mFlag |= MSG_FLAG_DEBUG;
}

CFdbDebugMsg::CFdbDebugMsg(FdbMsgCode_t code
                         , CFdbBaseObject *obj
                         , FdbSessionId_t alt_receiver)
    : CFdbMessage(code, obj, alt_receiver)
    , mSendTime(0)
    , mArriveTime(0)
    , mReplyTime(0)
    , mReceiveTime(0)
{
    mFlag |= MSG_FLAG_DEBUG;
}

CFdbDebugMsg::CFdbDebugMsg(NFdbBase::FdbMessageHeader &head
                           , CFdbMsgPrefix &prefix
                           , uint8_t *buffer
                           , FdbSessionId_t sid)
    : CFdbMessage(head, prefix, buffer, sid)
    , mSendTime(0)
    , mArriveTime(0)
    , mReplyTime(0)
    , mReceiveTime(0)
{
}

CFdbDebugMsg:: CFdbDebugMsg(FdbMsgCode_t code
                          , CFdbMessage *msg)
    : CFdbMessage(code, msg)
{
    mFlag |= MSG_FLAG_DEBUG;
}

void CFdbDebugMsg::encodeDebugInfo(NFdbBase::FdbMessageHeader &msg_hdr, CFdbSession *session)
{
    switch (msg_hdr.type())
    {
        case NFdbBase::MT_REPLY:
        case NFdbBase::MT_STATUS:
            msg_hdr.set_send_or_arrive_time(mArriveTime);
            msg_hdr.set_reply_time(CNanoTimer::getNanoSecTimer());
            break;
        case NFdbBase::MT_REQUEST:
        case NFdbBase::MT_SUBSCRIBE_REQ:
        case NFdbBase::MT_BROADCAST:
            mSendTime = CNanoTimer::getNanoSecTimer();
            msg_hdr.set_send_or_arrive_time(mSendTime);
            break;
        default:
            break;
    }
}

void CFdbDebugMsg::decodeDebugInfo(NFdbBase::FdbMessageHeader &msg_hdr, CFdbSession *session)
{
    switch (msg_hdr.type())
    {
        case NFdbBase::MT_REPLY:
        case NFdbBase::MT_STATUS:
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
        case NFdbBase::MT_REQUEST:
        case NFdbBase::MT_SUBSCRIBE_REQ:
        case NFdbBase::MT_BROADCAST:
            mArriveTime = CNanoTimer::getNanoSecTimer();
            if (msg_hdr.has_send_or_arrive_time())
            {
                mSendTime = msg_hdr.send_or_arrive_time();
            }
            break;
        default:
            break;
    }
}

void CFdbDebugMsg::metadata(CFdbMsgMetadata &metadata)
{
    metadata.mSendTime = mSendTime;
    metadata.mArriveTime = mArriveTime;
    metadata.mReplyTime = mReplyTime;
    metadata.mReceiveTime = mReceiveTime;
}

CFdbBroadcastMsg::CFdbBroadcastMsg(FdbMsgCode_t code
                                 , CFdbBaseObject *obj
                                 , const char *filter
                                 , FdbSessionId_t alt_sid
                                 , FdbObjectId_t alt_oid)
    : _CBaseMessage(code, obj)
{
    if (filter)
    {
        mFilter = filter;
    }

    if (isValidFdbId(alt_sid))
    {
        mSid = alt_sid;
        mFlag &= ~MSG_FLAG_ENDPOINT;
        mSenderName = obj->name();
    }
    else
    {
        mEpid = obj->epid();
        mFlag |= MSG_FLAG_ENDPOINT;
    }
    if (isValidFdbId(alt_oid))
    {
        mOid = alt_oid;
    }
}

CFdbBroadcastMsg::CFdbBroadcastMsg(FdbMsgCode_t code
                                 , CFdbMessage *msg
                                 , const char *filter)
    : _CBaseMessage(code, msg)
{
    if (filter)
    {
        mFilter = filter;
    }
}

void CFdbMessage::invokeSideband(const CFdbBasePayload &data
                               , int32_t timeout)
{
    CFdbMsgPayload payload;
    payload.mMessage = &data;
    CBaseJob::Ptr msg_ref(this);
    mType = NFdbBase::MT_SIDEBAND_REQUEST;
    submit(msg_ref, payload, -1, 0, timeout);
}

void CFdbMessage::sendSideband(const CFdbBasePayload &data)
{
    CFdbMsgPayload payload;
    payload.mMessage = &data;
    CBaseJob::Ptr msg_ref(this);
    mType = NFdbBase::MT_SIDEBAND_REQUEST;
    submit(msg_ref, payload, -1, FDB_MSG_TX_NO_REPLY, -1);
}


void CFdbMessage::replySideband(CBaseJob::Ptr &msg_ref,
                                       const CFdbBasePayload &data)
{
    CFdbMessage *fdb_msg = castToMessage<CFdbMessage *>(msg_ref);
    CFdbMsgPayload payload;
    payload.mMessage = &data;
    fdb_msg->mType = NFdbBase::MT_SIDEBAND_REPLY;
    fdb_msg->feedback(msg_ref, payload, -1);
}
