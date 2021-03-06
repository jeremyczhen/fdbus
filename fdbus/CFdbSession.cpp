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

#include <common_base/CFdbSession.h>
#include <common_base/CFdbSessionContainer.h>
#include <common_base/CFdbContext.h>
#include <common_base/CBaseEndpoint.h>
#include <common_base/CLogProducer.h>
#include <common_base/CSocketImp.h>
#include <common_base/CFdbRawMsgBuilder.h>
#include <utils/Log.h>
#include <utils/CFdbIfMessageHeader.h>

#define FDB_SEND_RETRIES (1024 * 10)
#define FDB_SEND_DELAY 2
#define FDB_SEND_MAX_RECURSIVE 128

#define FDB_RECV_RETRIES FDB_SEND_RETRIES
#define FDB_RECV_DELAY FDB_SEND_DELAY

CFdbSession::CFdbSession(FdbSessionId_t sid, CFdbSessionContainer *container, CSocketImp *socket)
    : CBaseFdWatch(socket->getFd(), POLLIN | POLLHUP | POLLERR)
    , mSid(sid)
    , mContainer(container)
    , mSocket(socket)
    , mSecurityLevel(FDB_SECURITY_LEVEL_NONE)
    , mRecursiveDepth(0)
    , mPid(0)
    , mPayloadBuffer(0)
{
    mUDPAddr.mPort = FDB_INET_PORT_INVALID;
    mUDPAddr.mType = FDB_SOCKET_UDP;

    if (mContainer->owner()->enableAysncRead())
    {
        submitInput(mPrefixBuffer, sizeof(mPrefixBuffer), false);
    }
}

CFdbSession::~CFdbSession()
{
    auto &sn_generator = mPendingMsgTable.getContainer();
    while (!sn_generator.empty())
    {
        auto it = sn_generator.begin();
        terminateMessage(it->second, NFdbBase::FDB_ST_PEER_VANISH,
                         "Message is destroyed due to broken connection.");
        mPendingMsgTable.deleteEntry(it);
    }

    mContainer->owner()->deleteConnectedSession(this);
    mContainer->owner()->unsubscribeSession(this);
    mContainer->owner()->unregisterSession(mSid);

    if (mSocket)
    {
        delete mSocket;
        mSocket = 0;
    }
    descriptor(0);

    mContainer->callSessionDestroyHook(this);
}

bool CFdbSession::sendMessage(const uint8_t *buffer, int32_t size)
{
    if (fatalError() || !buffer)
    {
        return false;
    }

    int32_t cnt = 0;
    int32_t retries = FDB_SEND_RETRIES;
    mRecursiveDepth++;
    while (1)
    {
        cnt = mSocket->send((uint8_t *)buffer, size);
        if (cnt < 0)
        {
            break;
        }
        buffer += cnt;
        size -= cnt;
        retries--;
        if ((size <= 0) || (retries <= 0))
        {
            break;
        }
        if (mRecursiveDepth < FDB_SEND_MAX_RECURSIVE)
        {
            worker()->dispatchInput(FDB_SEND_DELAY >> 1);
            sysdep_sleep(FDB_SEND_DELAY >> 1);
        }
        else
        {
            // Sorry just slow down...
            sysdep_sleep(FDB_SEND_DELAY);
        }
    }
    mRecursiveDepth--;

    if ((cnt < 0) || (size > 0))
    {
        if (cnt < 0)
        {
            LOG_E("CFdbSession: process %d: fatal error or peer drops when writing %d bytes!\n", CBaseThread::getPid(), size);
        }
        else
        {
            LOG_E("CFdbSession: process %d: fail to write %d bytes!\n", CBaseThread::getPid(), size);
        }
        fatalError(true);
        return false;
    }

    return true;
}

bool CFdbSession::sendMessage(CFdbMessage *msg)
{
    if (!msg->buildHeader())
    {
        return false;
    }

    bool ret = true;
    if (mContainer->owner()->enableAysncWrite())
    {
        auto logger = FDB_CONTEXT->getLogger();
        if (logger && msg->isLogEnabled())
        {
            CFdbRawMsgBuilder builder;
            logger->logFDBus(msg, mSenderName.c_str(), mContainer->owner(), builder);
            uint8_t *buffer = const_cast<uint8_t *>(builder.buffer());
            int32_t size = builder.bufferSize();
            bool need_release = false;
            if (!buffer && size)
            {
                buffer = new uint8_t[size];
                builder.toBuffer(buffer, size);
                need_release = true;
            }
            submitOutput(msg->getRawBuffer(), msg->getRawDataSize(), buffer, size);
            if (need_release)
            {
                delete[] buffer;
            }
        }
        else
        {
            submitOutput(msg->getRawBuffer(), msg->getRawDataSize(), 0, 0);
        }
    }
    else
    {
        if (sendMessage(msg->getRawBuffer(), msg->getRawDataSize()))
        {
            if (msg->isLogEnabled())
            {
                auto logger = FDB_CONTEXT->getLogger();
                if (logger)
                {
                    logger->logFDBus(msg, mSenderName.c_str(), mContainer->owner());
                }
            }
        }
        else
        {
            ret = false;
        }
    }

    return ret;
}

bool CFdbSession::sendMessage(CBaseJob::Ptr &ref)
{
    auto msg = castToMessage<CFdbMessage *>(ref);
    if (!msg)
    {
        return false;
    }
    msg->sn(mPendingMsgTable.allocateEntityId());
    if (sendMessage(msg))
    {
        msg->replaceBuffer(0); // free buffer to save memory
        msg->clearLogData();
        mPendingMsgTable.insertEntry(msg->sn(), ref);
        return true;
    }
    else
    {
        msg->setStatusMsg(NFdbBase::FDB_ST_UNABLE_TO_SEND, "Fail when sending message!");
        if (!msg->sync())
        {
            mContainer->owner()->doReply(ref);
        }
        return false;
    }
}

bool CFdbSession::sendUDPMessage(CFdbMessage *msg)
{
    return mContainer->sendUDPmessage(msg, mUDPAddr);
}

bool CFdbSession::receiveData(uint8_t *buf, int32_t size)
{
    int32_t cnt = 0;
    int32_t retries = FDB_RECV_RETRIES;
    while (1)
    {
        cnt = mSocket->recv(buf, size);
        if (cnt < 0)
        {
            break;
        }
        buf += cnt;
        size -= cnt;
        retries--;
        if ((size <= 0) || (retries <= 0))
        {
            break;
        }
        sysdep_sleep(FDB_RECV_DELAY);
    }

    if ((cnt < 0) || (size > 0))
    {
        if (cnt < 0)
        {
            LOG_E("CFdbSession: process %d: fatal error or peer drops when reading %d bytes!\n", CBaseThread::getPid(), size);
        }
        else
        {
            LOG_E("CFdbSession: process %d: fail to read %d bytes!\n", CBaseThread::getPid(), size);
        }
        fatalError(true);
        return false;
    }

    return true;
}

void CFdbSession::parsePrefix(const uint8_t *data, int32_t size)
{
    if (size < 0)
    {
        return;
    }
    mPayloadBuffer = 0;
    mMsgPrefix.deserialize(mPrefixBuffer);
    int32_t data_size = mMsgPrefix.mTotalLength - CFdbMessage::mPrefixSize;
    if (data_size < 0)
    {
        fatalError(true);
        return;
    }
    /*
     * The leading CFdbMessage::mPrefixSize bytes are not used; just for
     * keeping uniform structure
     */
    try
    {
        mPayloadBuffer = new uint8_t[mMsgPrefix.mTotalLength];
    }
    catch (...)
    {
        LOG_E("CFdbSession: Session %d: Unable to allocate buffer of size %d!\n",
                mSid, mMsgPrefix.mTotalLength);
        fatalError(true);
    }
}

void CFdbSession::processPayload(const uint8_t *data, int32_t size)
{
    if (size < 0)
    {
        if (mPayloadBuffer)
        {
            delete[] mPayloadBuffer;
            mPayloadBuffer = 0;
        }
        return;
    }

    NFdbBase::CFdbMessageHeader head;
    CFdbParcelableParser parser(head);
    if (!parser.parse(data, mMsgPrefix.mHeadLength))
    {
        LOG_E("CFdbSession: Session %d: Unable to deserialize message head!\n", mSid);
        if (mPayloadBuffer)
        {
            delete[] mPayloadBuffer;
            mPayloadBuffer = 0;
        }
        fatalError(true);
        return;
    }

    switch (head.type())
    {
        case FDB_MT_REQUEST:
        case FDB_MT_SIDEBAND_REQUEST:
        case FDB_MT_GET_EVENT:
        case FDB_MT_PUBLISH:
            doRequest(head);
            break;
        case FDB_MT_RETURN_EVENT:
        case FDB_MT_REPLY:
        case FDB_MT_SIDEBAND_REPLY:
            doResponse(head);
            break;
        case FDB_MT_BROADCAST:
            doBroadcast(head);
            break;
        case FDB_MT_STATUS:
            doResponse(head);
            break;
        case FDB_MT_SUBSCRIBE_REQ:
            if (head.code() == FDB_CODE_SUBSCRIBE)
            {
                doSubscribeReq(head, true);
            }
            else if (head.code() == FDB_CODE_UNSUBSCRIBE)
            {
                doSubscribeReq(head, false);
            }
            else if (head.code() == FDB_CODE_UPDATE)
            {
                doUpdate(head);
            }
            break;
        default:
            LOG_E("CFdbSession: Message %d: Unknown type!\n", (int32_t)head.serial_number());
            delete[] mPayloadBuffer;
            mPayloadBuffer = 0;
            fatalError(true);
            break;
    }

    mPayloadBuffer = 0;
}

int32_t CFdbSession::writeStream(const uint8_t *data, int32_t size)
{
    return mSocket->send((uint8_t *)data, size);
}

int32_t CFdbSession::readStream(uint8_t *data, int32_t size)
{
    return mSocket->recv(data, size);
}

void CFdbSession::onInputReady(const uint8_t *data, int32_t size)
{
    if (data == mPrefixBuffer)
    {
        parsePrefix(data, size);
        if (mPayloadBuffer)
        {
            submitInput(mPayloadBuffer + CFdbMessage::mPrefixSize,
                        mMsgPrefix.mTotalLength - CFdbMessage::mPrefixSize, true);
        }
    }
    else
    {
        processPayload(data, size);
        submitInput(mPrefixBuffer, sizeof(mPrefixBuffer), true);
    }
}

void CFdbSession::onInput()
{
    if (receiveData(mPrefixBuffer, sizeof(mPrefixBuffer)))
    {
        parsePrefix(mPrefixBuffer, sizeof(mPrefixBuffer));
    }
    if (fatalError())
    {
        return;
    }

    if (receiveData(mPayloadBuffer + CFdbMessage::mPrefixSize,
                     mMsgPrefix.mTotalLength - CFdbMessage::mPrefixSize))
    {
        processPayload(mPayloadBuffer + CFdbMessage::mPrefixSize,
                       mMsgPrefix.mTotalLength - CFdbMessage::mPrefixSize);
    }
}

void CFdbSession::onError()
{
    onHup();
}

void CFdbSession::onHup()
{
    auto endpoint = mContainer->owner();
    delete this;
    endpoint->checkAutoRemove();
}

void CFdbSession::doRequest(NFdbBase::CFdbMessageHeader &head)
{
    auto msg = new CFdbMessage(head, this);
    auto object = mContainer->owner()->getObject(msg, true);
    CBaseJob::Ptr msg_ref(msg);

    if (object)
    {
        msg->checkLogEnabled(object);
        msg->decodeDebugInfo(head);
        switch (head.type())
        {
            case FDB_MT_REQUEST:
                if (mContainer->owner()->onMessageAuthentication(msg, this))
                {
                    object->doInvoke(msg_ref);
                }
                else
                {
                    msg->sendStatus(this, NFdbBase::FDB_ST_AUTHENTICATION_FAIL,
                                    "Authentication failed!");
                }
            break;
            case FDB_MT_SIDEBAND_REQUEST:
                try // catch exception to avoid missing of auto-reply
                {
                    object->onSidebandInvoke(msg_ref);
                }
                catch (...)
                {
                }
                // check if auto-reply is required
                msg->autoReply(this, msg_ref, NFdbBase::FDB_ST_AUTO_REPLY_OK,
                               "Automatically reply to request.");
            break;
            case FDB_MT_GET_EVENT:
                if (mContainer->owner()->onEventAuthentication(msg, this))
                {
                    object->doGetEvent(msg_ref);
                }
                else
                {
                    msg->sendStatus(this, NFdbBase::FDB_ST_AUTHENTICATION_FAIL,
                                    "Authentication failed!");
                }
            break;
            case FDB_MT_PUBLISH:
                if (mContainer->owner()->onEventAuthentication(msg, this))
                {
                    object->doPublish(msg_ref);
                }
                else
                {
                    msg->sendStatus(this, NFdbBase::FDB_ST_AUTHENTICATION_FAIL,
                                    "Authentication failed!");
                }
            break;
            default:
            break;
        }
    }
    else
    {
        msg->enableLog(true);
        msg->sendStatus(this, NFdbBase::FDB_ST_OBJECT_NOT_FOUND, "Object is not found.");
    }
}

void CFdbSession::doResponse(NFdbBase::CFdbMessageHeader &head)
{
    bool found;
    PendingMsgTable_t::EntryContainer_t::iterator it;
    CBaseJob::Ptr &msg_ref = mPendingMsgTable.retrieveEntry(head.serial_number(), it, found);
    if (found)
    {
        auto msg = castToMessage<CFdbMessage *>(msg_ref);
        auto object_id = head.object_id();
        if (msg->objectId() != object_id)
        {
            LOG_E("CFdbSession: object id of response %d does not match that in request: %d\n",
                    object_id, msg->objectId());
            terminateMessage(msg_ref, NFdbBase::FDB_ST_OBJECT_NOT_FOUND, "Object ID does not match.");
            mPendingMsgTable.deleteEntry(it);
            delete[] mPayloadBuffer;
            mPayloadBuffer = 0;
            return;
        }

        auto object = mContainer->owner()->getObject(msg, false);
        if (object)
        {
            msg->update(head, mMsgPrefix);
            msg->decodeDebugInfo(head);
            msg->replaceBuffer(mPayloadBuffer, head.payload_size(), mMsgPrefix.mHeadLength);
            if (!msg->sync())
            {
                switch (head.type())
                {
                    case FDB_MT_REPLY:
                        object->doReply(msg_ref);
                    break;
                    case FDB_MT_SIDEBAND_REPLY:
                        object->onSidebandReply(msg_ref);
                    break;
                    case FDB_MT_STATUS:
                        switch (msg->mType)
                        {
                            case FDB_MT_REQUEST:
                                object->doReply(msg_ref);
                            break;
                            case FDB_MT_SIDEBAND_REQUEST:
                                object->onSidebandReply(msg_ref);
                            break;
                            case FDB_MT_GET_EVENT:
                                object->doReturnEvent(msg_ref);
                            break;
                            case FDB_MT_STATUS:
                                object->doStatus(msg_ref);
                            break;
                            default:
                            break;
                        }
                    break;
                    case FDB_MT_RETURN_EVENT:
                        object->doReturnEvent(msg_ref);
                    break;
                    default:
                    break;
                }
            }
        }
        else
        {
            delete[] mPayloadBuffer;
        }

        msg_ref->terminate(msg_ref);
        mPendingMsgTable.deleteEntry(it);
    }
}

void CFdbSession::doBroadcast(NFdbBase::CFdbMessageHeader &head)
{
    CFdbMessage *msg = 0;
    if (head.flag() & MSG_FLAG_INITIAL_RESPONSE)
    {
        bool found;
        PendingMsgTable_t::EntryContainer_t::iterator it;
        CBaseJob::Ptr &msg_ref = mPendingMsgTable.retrieveEntry(head.serial_number(), it, found);
        if (found)
        {
            auto outgoing_msg = castToMessage<CFdbMessage *>(msg_ref);
            msg = outgoing_msg->clone(head, this);
        }
    }

    if (!msg)
    {
        msg = new CFdbMessage(head, this);
    }
    auto object = mContainer->owner()->getObject(msg, false);
    CBaseJob::Ptr msg_ref(msg);
    if (object)
    {
        msg->decodeDebugInfo(head);
        object->doBroadcast(msg_ref);
    }
}

void CFdbSession::doSubscribeReq(NFdbBase::CFdbMessageHeader &head, bool subscribe)
{
    auto object_id = head.object_id();
    auto msg = new CFdbMessage(head, this);
    auto object = mContainer->owner()->getObject(msg, true);
    CBaseJob::Ptr msg_ref(msg);
    
    int32_t error_code;
    const char *error_msg;
    if (object)
    {
        // correct the type so that checkLogEnabled() can get correct
        // sender name and receiver name
        msg->type(FDB_MT_BROADCAST);
        msg->checkLogEnabled(object);
        msg->decodeDebugInfo(head);
        const CFdbMsgSubscribeItem *sub_item;
        int32_t ret;
        bool unsubscribe_object = true;
        FDB_BEGIN_FOREACH_SIGNAL_WITH_RETURN(msg, sub_item, ret)
        {
            auto code = sub_item->msg_code();
            const char *filter = 0;
            if (sub_item->has_filter())
            {
                filter = sub_item->filter().c_str();
            }
            
            if (subscribe)
            {
                // if fail on authentication, unable to 
                if (mContainer->owner()->onEventAuthentication(msg, this))
                {
                    CFdbSubscribeType type = FDB_SUB_TYPE_NORMAL;
                    if (sub_item->has_type())
                    {
                        type = sub_item->type();
                    }
                    object->subscribe(this, code, object_id, filter, type);
                }
                else
                {
                    object->unsubscribe(this, code, object_id, filter);
                    LOG_E("CFdbSession: Session %d: Unable to subscribe obj: %d, id: %d, filter: %s. Fail in security check!\n",
                           mSid, object_id, code, filter);
                    if (ret >= 0)
                    {
                        ret = -2;
                    }
                }
            }
            else
            {
                object->unsubscribe(this, code, object_id, filter);
                unsubscribe_object = false;
            }
        }
        FDB_END_FOREACH_SIGNAL_WITH_RETURN()

        if (ret < 0)
        {
            if (ret == -2)
            {
                error_code = NFdbBase::FDB_ST_AUTHENTICATION_FAIL;
                error_msg = "Authentication failed for some events!";
            }
            else
            {
                error_code = NFdbBase::FDB_ST_MSG_DECODE_FAIL;
                error_msg = "Not valid CFdbMsgSubscribeList message!";
                LOG_E("CFdbSession: Session %d: Unable to deserialize subscribe message!\n", mSid);
            }
            goto _reply_status;
        }
        else if (subscribe)
        {
            object->doSubscribe(msg_ref);
        }
        else if (unsubscribe_object)
        {   
            object->unsubscribe(object_id); // unsubscribe the whole object
        }
    }
    else
    {
        error_code = NFdbBase::FDB_ST_OBJECT_NOT_FOUND;
        error_msg = "Object is not found.";
        goto _reply_status;
        
    }
    return;
    
_reply_status:
    msg->enableLog(true);
    msg->sendStatus(this, error_code, error_msg);
}

void CFdbSession::doUpdate(NFdbBase::CFdbMessageHeader &head)
{
    auto msg = new CFdbMessage(head, this);
    auto object = mContainer->owner()->getObject(msg, true);
    CBaseJob::Ptr msg_ref(msg);

    if (object)
    {
        // trigger a broadcast() from onSubscribe() to update peer status.
        msg->manualUpdate(true);
        object->doSubscribe(msg_ref);
    }
    else
    {
        msg->sendStatus(this, NFdbBase::FDB_ST_OBJECT_NOT_FOUND, "Object is not found.");
    }
}

const std::string &CFdbSession::getEndpointName() const
{
    return mContainer->owner()->name();
}

void CFdbSession::terminateMessage(CBaseJob::Ptr &job, int32_t status, const char *reason)
{
    auto msg = castToMessage<CFdbMessage *>(job);
    if (msg)
    {
        msg->setStatusMsg(status, reason);
        if (!msg->sync())
        {
            mContainer->owner()->doReply(job);
        }
        msg->terminate(job);
    }
}

void CFdbSession::terminateMessage(FdbMsgSn_t msg_sn, int32_t status, const char *reason)
{
    bool found;
    PendingMsgTable_t::EntryContainer_t::iterator it;
    CBaseJob::Ptr &job = mPendingMsgTable.retrieveEntry(msg_sn, it, found);
    if (found)
    {
        terminateMessage(job, status, reason);
        mPendingMsgTable.deleteEntry(it);
    }
}

void CFdbSession::getSessionInfo(CFdbSessionInfo &info)
{
        container()->getSocketInfo(info.mContainerSocket);
        info.mCred = &mSocket->getPeerCredentials();
        info.mConn = &mSocket->getConnectionInfo();
}

CFdbMessage *CFdbSession::peepPendingMessage(FdbMsgSn_t sn)
{
    bool found;
    
    PendingMsgTable_t::EntryContainer_t::iterator it;
    CBaseJob::Ptr &job = mPendingMsgTable.retrieveEntry(sn, it, found);
    return found ? castToMessage<CFdbMessage *>(job) : 0;
}

void CFdbSession::securityLevel(int32_t level)
{
    mSecurityLevel = level;
}

void CFdbSession::token(const char *token)
{
    mToken = token;
}

bool CFdbSession::hostIp(std::string &host_ip)
{
    CFdbSessionInfo sinfo;
    getSessionInfo(sinfo);
    if (sinfo.mContainerSocket.mAddress->mType == FDB_SOCKET_IPC)
    {
        return false;
    }
    host_ip = sinfo.mConn->mSelfAddress.mAddr;
    return true;
}

bool CFdbSession::peerIp(std::string &host_ip)
{
    CFdbSessionInfo sinfo;
    getSessionInfo(sinfo);
    if (sinfo.mContainerSocket.mAddress->mType == FDB_SOCKET_IPC)
    {
        return false;
    }
    host_ip = sinfo.mConn->mPeerIp;
    return true;

}

bool CFdbSession::connected(const CFdbSocketAddr &addr)
{
    bool already_connected = false;
    const CFdbSocketAddr &session_addr = mSocket->getAddress();
    const CFdbSocketConnInfo &conn_info = mSocket->getConnectionInfo();
    if (session_addr.mType == addr.mType)
    {
        if (session_addr.mType == FDB_SOCKET_IPC)
        {
            if (session_addr.mAddr == addr.mAddr)
            {
                already_connected = true;
            }
        }
        else
        {
            if ((conn_info.mPeerIp == addr.mAddr) &&
                (conn_info.mPeerPort == addr.mPort))
            {
                already_connected = true;
            }
        }
    }
    return already_connected;
}

bool CFdbSession::bound(const CFdbSocketAddr &addr)
{
    bool already_connected = false;
    const CFdbSocketAddr &session_addr = mSocket->getAddress();
    if (session_addr.mType == addr.mType)
    {
        if (session_addr.mType == FDB_SOCKET_IPC)
        {
            if (session_addr.mAddr == addr.mAddr)
            {
                already_connected = true;
            }
        }
        else
        {
            if ((session_addr.mAddr == addr.mAddr) &&
                (session_addr.mPort == addr.mPort))
            {
                already_connected = true;
            }
        }
    }
    return already_connected;
}

