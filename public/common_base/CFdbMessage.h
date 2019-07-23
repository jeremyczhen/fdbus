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

#ifndef _CFDBMESSAGE_H_
#define _CFDBMESSAGE_H_

#include "common_defs.h"
#include "CBaseJob.h"
#include "CBaseLoopTimer.h"

#include <idl-gen/common.base.MessageHeader.pb.h>
#if defined(PROTOBUF_common_2ebase_2eMessageHeader_2eproto__INCLUDED)
#define CONFIG_INCLUDE_GEN_HEAD
#endif

#if !defined(CONFIG_INCLUDE_GEN_HEAD)
namespace google{namespace protobuf {class Message;}}
namespace google{namespace protobuf {class MessageLite;}}

namespace NFdbBase
{
    class FdbMessageHeader;
    class FdbMsgSubscribe;
    class FdbMsgSubscribeItem;
}
#endif

namespace NFdbBase
{
    enum FdbMsgStatusCode
    {
        FDB_ST_OK              = 0,
        FDB_ST_AUTO_REPLY_OK   = -11,
        FDB_ST_SUBSCRIBE_OK    = -12,
        FDB_ST_SUBSCRIBE_FAIL  = -13,
        FDB_ST_UNSUBSCRIBE_OK  = -14,
        FDB_ST_UNSUBSCRIBE_FAIL= -15,
        FDB_ST_TIMEOUT         = -16,
        FDB_ST_INVALID_ID      = -17,
        FDB_ST_PEER_VANISH     = -18,
        FDB_ST_DEAD_LOCK       = -19,
        FDB_ST_UNABLE_TO_SEND  = -20,

        FDB_ST_NON_EXIST       = -21,
        FDB_ST_ALREADY_EXIST   = -22,
        FDB_ST_MSG_DECODE_FAIL = -23,
        FDB_ST_BAD_PARAMETER   = -24,
        FDB_ST_NOT_AVAILABLE   = -25,
        FDB_ST_INTERNAL_FAIL   = -26,
        FDB_ST_OUT_OF_MEMORY   = -27,
        FDB_ST_NOT_IMPLEMENTED = -28,

        FDB_ST_OBJECT_NOT_FOUND= -29,
        FDB_ST_AUTHENTICATION_FAIL = -30
    };
}

namespace NFdbBase
{
    namespace wrapper
    {
        typedef int32_t FdbMessageType;
    }
}

enum EFdbSidebandMessage
{
    FDB_SIDEBAND_AUTH = 0,
    FDB_SIDEBAND_WATCHDOG = 1,
    FDB_SIDEBAND_SYSTEM_MAX = 4095,
    FDB_SIDEBAND_USER_MIN = FDB_SIDEBAND_SYSTEM_MAX + 1
};

typedef ::google::protobuf::MessageLite CFdbBasePayload;

struct CFdbMsgMetadata
{
    CFdbMsgMetadata()
        : mSendTime(0)
        , mArriveTime(0)
        , mReplyTime(0)
        , mReceiveTime(0)
    {
    }
    uint64_t mSendTime;     // the time when message is sent from client
    uint64_t mArriveTime;   // the time when message is arrived at server
    uint64_t mReplyTime;    // the time when message is replied by server
    uint64_t mReceiveTime;     // the time when message is received by client
};

#define FDB_BEGIN_FOREACH_SIGNAL_WITH_RETURN(_msg, _sub_item, _error)     do { \
    NFdbBase::FdbMsgSubscribe _subscribe_msg; \
    if (!_msg->deserialize(_subscribe_msg)) \
    { \
        _error = -1; \
    } \
    else \
    { \
        _error = 0; \
        const ::google::protobuf::RepeatedPtrField< ::NFdbBase::FdbMsgSubscribeItem> &_notify_list = \
            _subscribe_msg.notify_list(); \
        for (::google::protobuf::RepeatedPtrField< ::NFdbBase::FdbMsgSubscribeItem>::const_iterator it = _notify_list.begin(); \
                it != _notify_list.end(); ++it) \
        { \
            _sub_item = &(*it);

#define FDB_END_FOREACH_SIGNAL_WITH_RETURN() \
        } \
    } \
} while (0);

#define FDB_BEGIN_FOREACH_SIGNAL(_msg, _sub_item)     do { \
    int32_t _error; \
    FDB_BEGIN_FOREACH_SIGNAL_WITH_RETURN(_msg, _sub_item, _error)

#define FDB_END_FOREACH_SIGNAL() \
    FDB_END_FOREACH_SIGNAL_WITH_RETURN() \
    if (_error < 0) \
    { \
    } \
} while (0);

class CMessageTimer;
class CFdbSession;
class CFdbBaseObject;
class CFdbMessage : public CBaseJob
{
private:
#define FDB_CODE_SUBSCRIBE          0
#define FDB_CODE_UNSUBSCRIBE        1
#define FDB_CODE_UPDATE             2

#define MSG_LOCAL_FLAG_SHIFT        24
#define MSG_GLOBAL_FLAG_MASK        0xffffff

#define MSG_FLAG_NOREPLY_EXPECTED   (1 << 0)
#define MSG_FLAG_AUTO_REPLY         (1 << 1)
#define MSG_FLAG_SYNC_REPLY         (1 << 2)
#define MSG_FLAG_DEBUG              (1 << 3)
#define MSG_FLAG_RAW_DATA           (1 << 4)
#define MSG_FLAG_ERROR              (1 << 5)
#define MSG_FLAG_STATUS             (1 << 6)
#define MSG_FLAG_INITIAL_RESPONSE   (1 << 7)

#define MSG_FLAG_HEAD_OK            (1 << (MSG_LOCAL_FLAG_SHIFT + 0))
#define MSG_FLAG_ENDPOINT           (1 << (MSG_LOCAL_FLAG_SHIFT + 1))
#define MSG_FLAG_REPLIED            (1 << (MSG_LOCAL_FLAG_SHIFT + 2))
#define MSG_FLAG_EXTERNAL_BUFFER    (1 << (MSG_LOCAL_FLAG_SHIFT + 4))
#define MSG_FLAG_DO_NOT_LOG         (1 << (MSG_LOCAL_FLAG_SHIFT + 5))
#define MSG_FLAG_MANUAL_UPDATE      (1 << (MSG_LOCAL_FLAG_SHIFT + 5))
    
    struct CFdbMsgPrefix
    {
        CFdbMsgPrefix()
            : mTotalLength(0)
            , mHeadLength(0)
        {
        }

        CFdbMsgPrefix(uint32_t payload_length, uint32_t head_length)
            : mTotalLength(payload_length)
            , mHeadLength(head_length)
        {
        }

        void deserialize(const uint8_t *buffer)
        {
            mTotalLength = buffer[0] << 0;
            mTotalLength |= buffer[1] << 8;
            mTotalLength |= buffer[2] << 16;
            mTotalLength |= buffer[3] << 24;

            mHeadLength = buffer[4] << 0;
            mHeadLength |= buffer[5] << 8;
            mHeadLength |= buffer[6] << 16;
            mHeadLength |= buffer[7] << 24;
        }

        CFdbMsgPrefix(const uint8_t *buffer)
        {
            deserialize(buffer);
        }

        void serialize(uint8_t *buffer)
        {
            buffer[0] = (uint8_t)((mTotalLength >> 0) & 0xff);
            buffer[1] = (uint8_t)((mTotalLength >> 8) & 0xff);
            buffer[2] = (uint8_t)((mTotalLength >> 16) & 0xff);
            buffer[3] = (uint8_t)((mTotalLength >> 24) & 0xff);

            buffer[4] = (uint8_t)((mHeadLength >> 0) & 0xff);
            buffer[5] = (uint8_t)((mHeadLength >> 8) & 0xff);
            buffer[6] = (uint8_t)((mHeadLength >> 16) & 0xff);
            buffer[7] = (uint8_t)((mHeadLength >> 24) & 0xff);
        }
        uint32_t mTotalLength;
        uint32_t mHeadLength;
    };

    static const int32_t mPrefixSize = sizeof(CFdbMsgPrefix);
    static const int32_t mMaxHeadSize = 128;

public:
    CFdbMessage(FdbMsgCode_t code = FDB_INVALID_ID);
    virtual ~CFdbMessage();

    /*
     * reply[1]
     * Send reply to sender of the message
     * @iparam msg_ref: reference to the incoming message
     * @iparam data: message in protocol buffer
     */
    static void reply(CBaseJob::Ptr &msg_ref, const CFdbBasePayload &data);
    /*
     * reply[2]
     * Similiar to reply[1] but raw data is sent
     * @iparam buffer: buffer holding raw data
     * @iparam size: size of data in buffer
     */
    static void reply(CBaseJob::Ptr &msg_ref, const void *buffer = 0, int32_t size = 0);
    /*
     * Act as reply but just reply error_code and description to the sender.
     *      Usually used when no data but status should be replied to the sender
     * @iparam msg_ref: reference to the incoming message
     * @iparam error_code: error code and must be > 0
     * @iparam desctiption: optional description of the error
     */
    static void status(CBaseJob::Ptr &msg_ref, int32_t error_code, const char *description = 0);

    void broadcast(FdbMsgCode_t code
                   , const CFdbBasePayload &data
                   , const char *filter = 0);

    void broadcast(FdbMsgCode_t code
                   , const char *filter = 0
                   , const void *buffer = 0
                   , int32_t size = 0);
    /*
     * Deserialize received buffer into protocol buffer of particular type.
     * @return true - successfully decoded into protocol buffer
     *         false - unable to decode
     */
    bool deserialize(CFdbBasePayload &payload) const;
    /*
     *=========================================================
     * buffer structure:
     * +-----+-----+-------+--------------+----------+
     * |  1  |  2  |   3   |      4       |     5    |
     * +-----+-----+-------+--------------+----------+
     *
     * 1: offset; paddings
     * 2: prefix; 0 - 3 bytes: total length (2+3+4+5)
     *            4 - 7 bytes: length of head (3)
     * 3: head
     * 4: payload
     * 5: extra data
     */

    /*
     * Get size of payload data in buffer
     */
    int32_t getPayloadSize() const
    {
        return mPayloadSize;
    }

    int32_t getRawDataSize() const
    {
        return mPrefixSize + mHeadSize + mPayloadSize + mExtraSize;
    }

    int32_t getExtraDataSize() const
    {
        return mExtraSize;
    }

    /*
     * Get offset of payload from the beginning of the buffer
     * returned by ownBuffer().
     */
    int32_t getPayloadOffset() const
    {
        return mOffset + mPrefixSize + mHeadSize;
    }

    int32_t getExtraDataOffset() const
    {
        return getPayloadOffset() + mPayloadSize;
    }

    uint8_t *getRawBuffer() const
    {
        return mBuffer + mOffset;
    }

    /*
     * Get raw buffer holding received payload
     */
    uint8_t *getPayloadBuffer() const
    {
        return mBuffer ? (mBuffer + getPayloadOffset()) : 0;
    }

    uint8_t *getExtraBuffer() const
    {
        return mBuffer ? (mBuffer + getExtraDataOffset()) : 0;
    }

    /*
     * Own the buffer (so that user should release it manually)
     */
    void *ownBuffer()
    {
        void *buf = mBuffer;
        mBuffer = 0;
        return buf;
    }

    /*
     * Release the buffer obtained from ownBuffer().
     */
    static void releaseBuffer(uint8_t *buffer)
    {
        if (buffer)
        {
            delete[] buffer;
        }
    }

    bool isRawData() const
    {
        return !!(mFlag & MSG_FLAG_RAW_DATA);
    }

    /*
     * Get message code
     */
    FdbMsgCode_t code() const
    {
        return mCode;
    }

    /*
     * Get serial number of the message
     */
    FdbMsgSn_t sn() const
    {
        return mSn;
    }

    /*
     * Get broadcast filter
     */
    virtual const char *getFilter() const
    {
        return "";
    }

    /*
     * Get session ID of the message
     */
    FdbSessionId_t session() const
    {
        return mSid;
    }

    /*
     * in CBaseEndpoint::onReply(), if isStatus() return true, decode
     *      incoming message as error_code and description
     * @oparam error_code: the decoded error_code
     * @oparam description: description of the error
     */
    bool decodeStatus(int32_t &error_code, std::string &description);
    /*
     * in CBaseEndpoint::onReply(), check if the reply is caused by
     *      CBaseEndpoint::reply() (return false) or CBaseEndpoint::status
     *      (return true)
     */
    bool isStatus()
    {
        return !!(mFlag & MSG_FLAG_STATUS);
    }
    /*
     * in CBaseEndpoint::onReply() if isStatus() return true, or in
     *      CBaseEndpoint::onStatus(), check if the reply indicates
     *      a successful result of fail result.
     */
    bool isError()
    {
        return (mFlag & (MSG_FLAG_ERROR | MSG_FLAG_STATUS)) == (MSG_FLAG_ERROR | MSG_FLAG_STATUS);
    }
    /*
     * in CBaseEndpoint::onStatus(), check if the reply is automatical reply
     *      to CBaseEndpoint::subscribe().
     */
    bool isSubscribe();

    bool isInitialResponse()
    {
        return !!(mFlag & MSG_FLAG_INITIAL_RESPONSE);
    }

    /*
     * Retrieve metadata associated with the message. Valid only if CBaseMessage
     *      is defined as CFdbDebugMsg.
     * @oparam metadata: retrieved metadata
     *      mSendTime - the time when message is sent
     *      mArriveTime - the time when message arrives at receiver
     *      mReplyTime - the time when message is replied by receiver
     *      mReceiveTime - the time when reply is received by the sender
     */
    virtual void metadata(CFdbMsgMetadata &metadata)
    {
        metadata.mSendTime = 0;
        metadata.mArriveTime = 0;
        metadata.mReplyTime = 0;
        metadata.mReceiveTime = 0;
    }

    /*
     * Parse timestamp to time span.
     */
    static void parseTimestamp(const CFdbMsgMetadata &metadata
                             , uint64_t &client_to_server
                             , uint64_t &server_to_reply
                             , uint64_t &reply_to_client
                             , uint64_t &total);
    static uint32_t maxReservedSize()
    {
        return mPrefixSize + mMaxHeadSize;
    }

    FdbObjectId_t objectId()
    {
        return mOid;
    }

    std::string &senderName()
    {
        return mSenderName;
    }

    void senderName(const char *sender_name)
    {
        if (sender_name)
        {
            mSenderName = sender_name;
        }
    }

    NFdbBase::wrapper::FdbMessageType type() const
    {
        return mType;
    }

    static const char *getMsgTypeName(NFdbBase::wrapper::FdbMessageType type);

protected:
    virtual void encodeDebugInfo(NFdbBase::FdbMessageHeader &msg_hdr, CFdbSession *session)
    {}

    virtual void decodeDebugInfo(NFdbBase::FdbMessageHeader &msg_hdr, CFdbSession *session)
    {}
    virtual bool allocCopyRawBuffer(const uint8_t *src, int32_t payload_size);
    virtual void freeRawBuffer();
    virtual void onAsyncError(Ptr &ref, NFdbBase::FdbMsgStatusCode code, const char *reason) {}

private:
    CFdbMessage(NFdbBase::FdbMessageHeader &head
                , CFdbMsgPrefix &prefix
                , uint8_t *buffer
                , FdbSessionId_t sid);

    CFdbMessage(FdbMsgCode_t code
              , CFdbBaseObject *obj
              , FdbSessionId_t alt_receiver = FDB_INVALID_ID);

    CFdbMessage(FdbMsgCode_t code
              , CFdbMessage *msg);

    union CFdbMsgPayload
    {
        const CFdbBasePayload *mMessage;
        const uint8_t *mBuffer;
    };

    void invoke(const CFdbBasePayload &data
                , int32_t timeout = 0);
    static void invoke(CBaseJob::Ptr &msg_ref
                       , const CFdbBasePayload &data
                       , int32_t timeout = 0);
    void invoke(const void *buffer = 0
                , int32_t size = 0
                , int32_t timeout = 0);
    static void invoke(CBaseJob::Ptr &msg_ref
                       , const void *buffer = 0
                       , int32_t size = 0
                       , int32_t timeout = 0);
    void manualUpdate(bool active)
    {
        if (active)
        {
            mFlag |= MSG_FLAG_MANUAL_UPDATE;
        }
        else
        {
            mFlag &= ~MSG_FLAG_MANUAL_UPDATE;
        }
    }

    bool manualUpdate()
    {
        return !!(mFlag & MSG_FLAG_MANUAL_UPDATE);
    }

    void send(const CFdbBasePayload &data);
    void send(const void *buffer = 0
              , int32_t size = 0);

    void broadcast(const CFdbBasePayload &msg);
    void broadcast(const void *buffer = 0
                   , int32_t size = 0);

    void subscribe(NFdbBase::FdbMsgSubscribe &msg_list
                   , int32_t timeout = 0);
    static void subscribe(CBaseJob::Ptr &msg_ref
                          , NFdbBase::FdbMsgSubscribe &msg_list
                          , int32_t timeout = 0);
    
    void unsubscribe(NFdbBase::FdbMsgSubscribe &msg_list);
    
    void update(NFdbBase::FdbMsgSubscribe &msg_list, int32_t timeout = 0);
    static void update(CBaseJob::Ptr &msg_ref
                       , NFdbBase::FdbMsgSubscribe &msg_list
                       , int32_t timeout = 0);

    void run(CBaseWorker *worker, Ptr &ref);
    bool buildHeader(CFdbSession *session);
    bool serialize(CFdbMsgPayload &payload, int32_t payload_size);

    void submit(CBaseJob::Ptr &msg_ref
                , CFdbMsgPayload &payload
                , int32_t size
                , uint32_t tx_flag
                , int32_t timeout);
    void invoke(CBaseJob::Ptr &msg_ref
                , CFdbMsgPayload &payload
                , int32_t size
                , uint32_t tx_flag
                , int32_t timeout);
    void subscribe(CBaseJob::Ptr &msg_ref
                   , NFdbBase::FdbMsgSubscribe &msg_list
                   , uint32_t tx_flag
                   , FdbMsgCode_t subscribe_code
                   , int32_t timeout);
    void feedback(CBaseJob::Ptr &msg_ref, CFdbMsgPayload &payload, int32_t size);
    void yell(CFdbMsgPayload &payload, int32_t size);

    void doRequest(Ptr &ref);
    void doReply(Ptr &ref);
    void doBroadcast(Ptr &ref);
    void doStatus(Ptr &ref);
    void doSubscribeReq(Ptr &ref);
    void doUnsubscribeReq(Ptr &ref);

    static void autoReply(CBaseJob::Ptr &msg_ref, int32_t error_code, const char *description = 0);
    void setErrorMsg(NFdbBase::wrapper::FdbMessageType type, int32_t error_code, const char *description = 0);

    void sendStatus(CFdbSession *session, int32_t error_code, const char *description = 0);
    void sendAutoReply(CFdbSession *session, int32_t error_code, const char *description = 0);
    static void autoReply(CFdbSession *session, CBaseJob::Ptr &msg_ref, int32_t error_code, const char *description = 0);

    void releaseBuffer();
    void replaceBuffer(uint8_t *buffer, int32_t payload_size = 0, int32_t head_size = 0, int32_t offset = 0);

    void code(FdbMsgCode_t code)
    {
        mCode = code;
    }
    void sn(FdbMsgSn_t sn)
    {
        mSn = sn;
    }

    void session(FdbSessionId_t session)
    {
        mSid = session;
    }

    void type(NFdbBase::wrapper::FdbMessageType type)
    {
        mType = type;
    }

    static bool deserializePb(CFdbBasePayload &payload, void *buffer, int32_t size);

    bool sync()
    {
        return !!(mFlag & MSG_FLAG_SYNC_REPLY);
    }

    void update(NFdbBase::FdbMessageHeader &head, CFdbMessage::CFdbMsgPrefix &prefix);
    CFdbSession *getSession();

    void setDestination(CFdbBaseObject *obj, FdbSessionId_t alt_sid = FDB_INVALID_ID);

    void objectId(FdbObjectId_t object_id)
    {
        mOid = object_id;
    }

    void updateObjectId(FdbObjectId_t object_id)
    {
        if (mOid != object_id)
        {
            mFlag &= ~MSG_FLAG_HEAD_OK;
            mOid = object_id;
        }
    }

    void sendLog(const CFdbBasePayload &data
               , uint8_t *log_data
               , int32_t size
               , int32_t clipped_size
               , bool send_as_job);
    void broadcastLog(const CFdbBasePayload &data
                    , const uint8_t *log_data
                    , int32_t size
                    , bool send_as_johb);
    CFdbMessage *parseFdbLog(uint8_t *buffer, int32_t size);
    void invokeSideband(const CFdbBasePayload &data
                      , int32_t timeout = 0);
    void sendSideband(const CFdbBasePayload &data);
    static void replySideband(CBaseJob::Ptr &msg_ref,
                              const CFdbBasePayload &data);

    NFdbBase::wrapper::FdbMessageType mType;
    FdbMsgCode_t mCode;
    FdbMsgSn_t mSn;
    int32_t mPayloadSize;
    int32_t mHeadSize;
    int32_t mOffset;
    int32_t mExtraSize;
    union
    {
        FdbSessionId_t mSid;
        FdbEndpointId_t mEpid;
    };
    FdbObjectId_t mOid;
    uint8_t *mBuffer;
    uint32_t mFlag;
    CMessageTimer *mTimer;
    std::string mSenderName;
    std::string *mStringData;

    friend class CFdbSession;
    friend class CFdbBaseObject;
    friend class CFdbBroadcastMsg;
    friend class CFdbDebugMsg;
    friend class CLogProducer;
    friend class CLogPrinter;
    friend class CLogServer;
    friend class CLogClient;
};

class CFdbDebugMsg : public CFdbMessage
{
public:
    CFdbDebugMsg(FdbMsgCode_t code = FDB_INVALID_ID);
    
    void metadata(CFdbMsgMetadata &metadata);
protected:
    void encodeDebugInfo(NFdbBase::FdbMessageHeader &msg_hdr, CFdbSession *session);
    void decodeDebugInfo(NFdbBase::FdbMessageHeader &msg_hdr, CFdbSession *session);
private:
    CFdbDebugMsg(FdbMsgCode_t code
                , CFdbBaseObject *obj
                , FdbSessionId_t alt_receiver = FDB_INVALID_ID);
    CFdbDebugMsg(NFdbBase::FdbMessageHeader &head
                 , CFdbMsgPrefix &prefix
                 , uint8_t *buffer
                 , FdbSessionId_t sid);
    CFdbDebugMsg(FdbMsgCode_t code
              , CFdbMessage *msg);

    uint64_t mSendTime;     // the time when message is sent from client
    uint64_t mArriveTime;   // the time when message is arrived at server
    uint64_t mReplyTime;    // the time when message is replied by server
    uint64_t mReceiveTime;     // the time when message is received by client

    friend class CFdbSession;
    friend class CFdbBroadcastMsg;
    friend class CFdbBaseObject;
};

typedef CFdbMessage CBaseMessage;

#if defined(CONFIG_FDB_MESSAGE_METADATA)
typedef CFdbDebugMsg _CBaseMessage;
#else
typedef CFdbMessage _CBaseMessage;
#endif

class CFdbBroadcastMsg : public _CBaseMessage
{
public:
    CFdbBroadcastMsg(FdbMsgCode_t code
                     , CFdbBaseObject *obj
                     , const char *filter
                     , FdbSessionId_t alt_sid = FDB_INVALID_ID
                     , FdbObjectId_t alt_oid = FDB_INVALID_ID);

    CFdbBroadcastMsg(FdbMsgCode_t code
                     , CFdbMessage *msg
                     , const char *filter);
    
    const char *getFilter() const
    {
        return mFilter.c_str();
    }

protected:
    CFdbBroadcastMsg(NFdbBase::FdbMessageHeader &head
                     , CFdbMsgPrefix &prefix
                     , uint8_t *buffer
                     , FdbSessionId_t sid
                     , const char *filter)
        : _CBaseMessage(head, prefix, buffer, sid)
    {
        if (filter)
        {
            mFilter = filter;
        }
    }
private:
    std::string mFilter;
    friend class CFdbSession;
};

#endif
