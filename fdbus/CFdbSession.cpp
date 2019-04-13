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
#include <idl-gen/common.base.MessageHeader.pb.h>
#include <utils/Log.h>

#define FDB_SEND_RETRIES 65535
#define FDB_RECV_RETRIES 65535

CFdbSession::CFdbSession(FdbSessionId_t sid, CFdbSessionContainer *container, CSocketImp *socket)
    : CBaseFdWatch(socket->getFd(), POLLIN | POLLHUP | POLLERR)
    , mSid(sid)
    , mContainer(container)
    , mSocket(socket)
    , mInternalError(false)
{
}

CFdbSession::~CFdbSession()
{
#if 0
    LOG_I("CFdbSession: session %d is destroyed!\n", (int32_t)mSid);
#endif
    mContainer->owner()->deleteConnectedSession(this);
    mContainer->owner()->unsubscribeSession(this);
    CFdbContext::getInstance()->unregisterSession(mSid);

    PendingMsgTable_t::EntryContainer_t &sn_generator = mPendingMsgTable.getContainer();
    while (!sn_generator.empty())
    {
        PendingMsgTable_t::EntryContainer_t::iterator it = sn_generator.begin();
        terminateMessage(it->second, NFdbBase::FDB_ST_PEER_VANISH,
                         "Message is destroyed due to broken connection.");
        mPendingMsgTable.deleteEntry(it);
    }

    if (mSocket)
    {
        delete mSocket;
    }

    mContainer->onSessionDeleted(this);
}

bool CFdbSession::sendMessage(const uint8_t *buffer, int32_t size)
{
    int32_t retries = FDB_SEND_RETRIES;
    while ((size > 0) && (retries > 0))
    {
        int32_t cnt = mSocket->send((uint8_t *)buffer, size);
        if (cnt < 0)
        {
            return false;
        }
        buffer += cnt;
        size -= cnt;
        retries--;
    }
    return retries > 0;
}

bool CFdbSession::sendMessage(CFdbMessage *msg)
{
    if (!msg->buildHeader(this))
    {
        return false;
    }
    if (sendMessage(msg->getRawBuffer(), msg->getRawDataSize()))
    {
        CLogProducer *logger = CFdbContext::getInstance()->getLogger();
        if (logger)
        {
            logger->logMessage(msg, mContainer->owner());
        }
        return true;
    }
    return false;
}

bool CFdbSession::sendMessage(CBaseJob::Ptr &ref)
{
    CFdbMessage *msg = castToMessage<CFdbMessage *>(ref);
    if (!msg)
    {
        return false;
    }
    msg->sn(mPendingMsgTable.allocateEntityId());
    if (sendMessage(msg))
    {
        msg->replaceBuffer(0); // free buffer to save memory
        mPendingMsgTable.insertEntry(msg->sn(), ref);
        return true;
    }
    else
    {
        msg->setErrorMsg(NFdbBase::MT_UNKNOWN, NFdbBase::FDB_ST_UNABLE_TO_SEND, "Fail when sending message!");
        if (!msg->sync())
        {
            mContainer->owner()->doReply(ref);
        }
        return false;
    }
}

bool CFdbSession::receiveData(uint8_t *buf, int32_t size)
{
    int32_t retries = FDB_RECV_RETRIES;
    while ((size > 0) && (retries > 0))
    {
        int32_t cnt = mSocket->recv(buf, size);
        if (cnt < 0)
        {
            return false;
        }
        buf += cnt;
        size -= cnt;
        retries--;
    }
    return retries > 0;
}

void CFdbSession::onInput(bool &io_error)
{
    uint8_t hdr_buf[CFdbMessage::mPrefixSize];
    if (!receiveData(hdr_buf, sizeof(hdr_buf)))
    {
#if 0
        LOG_E("CFdbSession: Session %d: Unable to read prefix: %d!\n", mSid, (int)sizeof(hdr_buf));
#endif
        io_error = true;
        return;
    }

    CFdbMessage::CFdbMsgPrefix prefix(hdr_buf);

    int32_t data_size = prefix.mTotalLength - CFdbMessage::mPrefixSize;
    if (data_size < 0)
    {
        io_error = true;
        return;
    }
    /*
     * The leading CFdbMessage::mPrefixSize bytes are not used; just for
     * keeping uniform structure
     */
    uint8_t *whole_buf;
    try
    {
        whole_buf = new uint8_t[prefix.mTotalLength];
    }
    catch (...)
    {
        LOG_E("CFdbSession: Session %d: Unable to allocate buffer of size %d!\n",
                mSid, CFdbMessage::mPrefixSize + data_size);
        io_error = true;
        return;
    }
    uint8_t *head_start = whole_buf + CFdbMessage::mPrefixSize;
    if (!receiveData(head_start, data_size))
    {
        LOG_E("CFdbSession: Session %d: Unable to read message: %d!\n", mSid, data_size);
        delete[] whole_buf;
        io_error = true;
        return;
    }

    NFdbBase::FdbMessageHeader head;
    if (!CFdbMessage::deserializePb(head, head_start, prefix.mHeadLength))
    {
        LOG_E("CFdbSession: Session %d: Unable to deserialize message head!\n", mSid);
        delete[] whole_buf;
        io_error = true;
        return;
    }

    switch (head.type())
    {
        case NFdbBase::MT_REQUEST:
            doRequest(head, prefix, whole_buf);
            break;
        case NFdbBase::MT_REPLY:
            doResponse(head, prefix, whole_buf);
            break;
        case NFdbBase::MT_BROADCAST:
            doBroadcast(head, prefix, whole_buf);
            break;
        case NFdbBase::MT_STATUS:
            doResponse(head, prefix, whole_buf);
            break;
        case NFdbBase::MT_SUBSCRIBE_REQ:
            if (head.code() == FDB_CODE_SUBSCRIBE)
            {
                doSubscribeReq(head, prefix, whole_buf, true);
            }
            else if (head.code() == FDB_CODE_UNSUBSCRIBE)
            {
                doSubscribeReq(head, prefix, whole_buf, false);
            }
            break;
        default:
            LOG_E("CFdbSession: Message %d: Unknown type!\n", (int32_t)head.serial_number());
            delete[] whole_buf;
            io_error = true;
            break;
    }
}

void CFdbSession::onError()
{
    CBaseEndpoint *endpoint = mContainer->owner();
    mInternalError = true;
    delete this;
    endpoint->checkAutoRemove();
}

void CFdbSession::onHup()
{
    CBaseEndpoint *endpoint = mContainer->owner();
    mInternalError = false;
    delete this;
    endpoint->checkAutoRemove();
}

void CFdbSession::doRequest(NFdbBase::FdbMessageHeader &head, CFdbMessage::CFdbMsgPrefix &prefix, uint8_t *buffer)
{
    CFdbMessage *msg = (head.flag() & MSG_FLAG_DEBUG) ?
                       new CFdbDebugMsg(head, prefix, buffer, mSid) :
                       new CFdbMessage(head, prefix, buffer, mSid);
    CFdbBaseObject *object = mContainer->owner()->getObject(msg, true);
    CBaseJob::Ptr msg_ref(msg);
    if (object)
    {
        msg->decodeDebugInfo(head, this);
        object->doInvoke(msg_ref);
        // check if auto-reply is required
        msg->autoReply(this, msg_ref, NFdbBase::FDB_ST_AUTO_REPLY_OK, "Automatically reply to request.");
    }
    else
    {
        msg->sendStatus(this, NFdbBase::FDB_ST_OBJECT_NOT_FOUND, "Object is not found.");
    }
}

void CFdbSession::doResponse(NFdbBase::FdbMessageHeader &head, CFdbMessage::CFdbMsgPrefix &prefix, uint8_t *buffer)
{
    bool found;
    PendingMsgTable_t::EntryContainer_t::iterator it;
    CBaseJob::Ptr &job = mPendingMsgTable.retrieveEntry(head.serial_number(), it, found);
    if (found)
    {
        CFdbMessage *msg = castToMessage<CFdbMessage *>(job);
        FdbObjectId_t object_id = head.object_id();
        if (msg->objectId() != object_id)
        {
            LOG_E("CFdbSession: object id of response %d does not match that in request: %d\n",
                    object_id, msg->objectId());
            terminateMessage(job, NFdbBase::FDB_ST_OBJECT_NOT_FOUND, "Object ID does not match.");
            mPendingMsgTable.deleteEntry(it);
            delete[] buffer;
            return;
        }
        
        CFdbBaseObject *object = mContainer->owner()->getObject(msg, false);
        if (msg && object)
        {
            msg->update(head, prefix);
            msg->decodeDebugInfo(head, this);
            msg->replaceBuffer(buffer, head.payload_size(), prefix.mHeadLength);
            if (!msg->sync())
            {
                if (head.type() == NFdbBase::MT_REPLY)
                {
                    object->doReply(job);
                }
                else if (head.type() == NFdbBase::MT_STATUS)
                {
                    if (msg->mType == NFdbBase::MT_REQUEST)
                    {
                        object->doReply(job);
                    }
                    else
                    {
                        object->doStatus(job);
                    }
                }
            }
        }
        else
        {
            delete[] buffer;
        }

        job->terminate(job);
        mPendingMsgTable.deleteEntry(it);
    }
}

void CFdbSession::doBroadcast(NFdbBase::FdbMessageHeader &head,
                              CFdbMessage::CFdbMsgPrefix &prefix, uint8_t *buffer)
{
    const char *filter = "";
    if (head.has_broadcast_filter())
    {
        filter = head.broadcast_filter().c_str();
    }
    CFdbBroadcastMsg *msg = new CFdbBroadcastMsg(head, prefix, buffer, mSid, filter);
    CFdbBaseObject *object = mContainer->owner()->getObject(msg, false);
    CBaseJob::Ptr msg_ref(msg);
    if (object)
    {
        msg->decodeDebugInfo(head, this);
        object->doBroadcast(msg_ref);
    }
}

void CFdbSession::doSubscribeReq(NFdbBase::FdbMessageHeader &head,
                                 CFdbMessage::CFdbMsgPrefix &prefix, uint8_t *buffer, bool subscribe)
{
    FdbObjectId_t object_id = head.object_id();
    CFdbMessage *msg = (head.flag() & MSG_FLAG_DEBUG) ?
                       new CFdbDebugMsg(head, prefix, buffer, mSid) :
                       new CFdbMessage(head, prefix, buffer, mSid);
    CFdbBaseObject *object = mContainer->owner()->getObject(msg, true);
    CBaseJob::Ptr msg_ref(msg);

    if (object)
    {
        msg->decodeDebugInfo(head, this);
        const ::NFdbBase::FdbMsgSubscribeItem *sub_item;
        int32_t ret;
        bool unsubscribe_object = true;
        FDB_BEGIN_FOREACH_SIGNAL_WITH_RETURN(msg, sub_item, ret)
        {
            FdbMsgCode_t code = sub_item->msg_code();
            const char *filter = 0;
            if (sub_item->has_filter())
            {
                filter = sub_item->filter().c_str();
            }
            
            if (subscribe)
            {
                object->subscribe(this, code, object_id, filter);
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
            msg->sendStatus(this, NFdbBase::FDB_ST_MSG_DECODE_FAIL, "Not valid NFdbBase::FdbMsgSubscribe message!");
            LOG_E("CFdbSession: Session %d: Unable to deserialize subscribe message!\n", mSid);
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
        msg->sendStatus(this, NFdbBase::FDB_ST_OBJECT_NOT_FOUND, "Object is not found.");
    }
}

std::string &CFdbSession::getEndpointName()
{
    return mContainer->owner()->name();
}

void CFdbSession::terminateMessage(CBaseJob::Ptr &job, int32_t status, const char *reason)
{
    CFdbMessage *msg = castToMessage<CFdbMessage *>(job);
    if (msg)
    {
        msg->setErrorMsg(NFdbBase::MT_UNKNOWN, status, reason);
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
        container()->getSocketInfo(info.mSocketInfo);
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

