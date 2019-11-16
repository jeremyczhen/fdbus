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
#include "CFdbMessageHeader.h"

#define FDB_SEND_RETRIES 65535
#define FDB_RECV_RETRIES 65535

CFdbSession::CFdbSession(FdbSessionId_t sid, CFdbSessionContainer *container, CSocketImp *socket)
    : CBaseFdWatch(socket->getFd(), POLLIN | POLLHUP | POLLERR)
    , mSid(sid)
    , mContainer(container)
    , mSocket(socket)
    , mInternalError(false)
    , mSecurityLevel(FDB_SECURITY_LEVEL_NONE)
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
        mSocket = 0;
    }
    descriptor(0);

    mContainer->callSessionDestroyHook(this);
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
        if (msg->isLogEnabled())
        {
            CLogProducer *logger = CFdbContext::getInstance()->getLogger();
            if (logger)
            {
                logger->logMessage(msg, mContainer->owner());
            }
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
        msg->setErrorMsg(FDB_MT_UNKNOWN, NFdbBase::FDB_ST_UNABLE_TO_SEND,
                         "Fail when sending message!");
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

    CFdbMessageHeaderParser head(head_start, prefix.mHeadLength);
    if (head.parse() < 0)
    {
        LOG_E("CFdbSession: Session %d: Unable to deserialize message head!\n", mSid);
        delete[] whole_buf;
        io_error = true;
        return;
    }

    switch (head.type())
    {
        case FDB_MT_REQUEST:
        case FDB_MT_SIDEBAND_REQUEST:
            doRequest(head, prefix, whole_buf);
            break;
        case FDB_MT_REPLY:
        case FDB_MT_SIDEBAND_REPLY:
            doResponse(head, prefix, whole_buf);
            break;
        case FDB_MT_BROADCAST:
            doBroadcast(head, prefix, whole_buf);
            break;
        case FDB_MT_STATUS:
            doResponse(head, prefix, whole_buf);
            break;
        case FDB_MT_SUBSCRIBE_REQ:
            if (head.code() == FDB_CODE_SUBSCRIBE)
            {
                doSubscribeReq(head, prefix, whole_buf, true);
            }
            else if (head.code() == FDB_CODE_UNSUBSCRIBE)
            {
                doSubscribeReq(head, prefix, whole_buf, false);
            }
            else if (head.code() == FDB_CODE_UPDATE)
            {
                doUpdate(head, prefix, whole_buf);
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

void CFdbSession::doRequest(CFdbMessageHeader &head,
                            CFdbMessage::CFdbMsgPrefix &prefix, uint8_t *buffer)
{
    CFdbMessage *msg = new CFdbMessage(head, prefix, buffer, mSid);
    CFdbBaseObject *object = mContainer->owner()->getObject(msg, true);
    CBaseJob::Ptr msg_ref(msg);

    msg->type(FDB_MT_REPLY);
    msg->checkLogEnabled(mContainer->owner(), false);
    if (object)
    {
        NFdbBase::FdbMsgStatusCode status_code = NFdbBase::FDB_ST_AUTO_REPLY_OK;
        const char *status_msg = "Automatically reply to request.";
        msg->decodeDebugInfo(head, this);
        if (msg->type() == FDB_MT_SIDEBAND_REQUEST)
        {
            object->onSidebandInvoke(msg_ref);
        }
        else
        {
            if (object->onMessageAuthentication(msg, this))
            {
                object->doInvoke(msg_ref);
            }
            else
            {
                status_code = NFdbBase::FDB_ST_AUTHENTICATION_FAIL;
                status_msg = "Authentication failed!";
            }
        }
        // check if auto-reply is required
        msg->autoReply(this, msg_ref, status_code, status_msg);
    }
    else
    {
        msg->sendStatus(this, NFdbBase::FDB_ST_OBJECT_NOT_FOUND, "Object is not found.");
    }
}

void CFdbSession::doResponse(CFdbMessageHeader &head,
                             CFdbMessage::CFdbMsgPrefix &prefix, uint8_t *buffer)
{
    bool found;
    PendingMsgTable_t::EntryContainer_t::iterator it;
    CBaseJob::Ptr &msg_ref = mPendingMsgTable.retrieveEntry(head.serial_number(), it, found);
    if (found)
    {
        CFdbMessage *msg = castToMessage<CFdbMessage *>(msg_ref);
        FdbObjectId_t object_id = head.object_id();
        if (msg->objectId() != object_id)
        {
            LOG_E("CFdbSession: object id of response %d does not match that in request: %d\n",
                    object_id, msg->objectId());
            terminateMessage(msg_ref, NFdbBase::FDB_ST_OBJECT_NOT_FOUND, "Object ID does not match.");
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
                if (head.type() == FDB_MT_REPLY)
                {
                    object->doReply(msg_ref);
                }
                else if (head.type() == FDB_MT_SIDEBAND_REQUEST)
                {
                    object->onSidebandReply(msg_ref);
                }
                else if (head.type() == FDB_MT_STATUS)
                {
                    if (msg->mType == FDB_MT_REQUEST)
                    {
                        object->doReply(msg_ref);
                    }
                    else if (msg->mType == FDB_MT_SIDEBAND_REQUEST)
                    {
                        object->onSidebandReply(msg_ref);
                    }
                    else
                    {
                        object->doStatus(msg_ref);
                    }
                }
            }
        }
        else
        {
            delete[] buffer;
        }

        msg_ref->terminate(msg_ref);
        mPendingMsgTable.deleteEntry(it);
    }
}

void CFdbSession::doBroadcast(CFdbMessageHeader &head,
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

void CFdbSession::doSubscribeReq(CFdbMessageHeader &head,
                                 CFdbMessage::CFdbMsgPrefix &prefix,
                                 uint8_t *buffer, bool subscribe)
{
    FdbObjectId_t object_id = head.object_id();
    CFdbMessage *msg = new CFdbMessage(head, prefix, buffer, mSid);
    CFdbBaseObject *object = mContainer->owner()->getObject(msg, true);
    CBaseJob::Ptr msg_ref(msg);
    
    int32_t error_code;
    const char *error_msg;
    if (object)
    {
        // correct the type so that checkLogEnabled() can get correct
        // sender name and receiver name
        msg->type(FDB_MT_BROADCAST);
        msg->checkLogEnabled(mContainer->owner(), false);
        msg->decodeDebugInfo(head, this);
        const CFdbMsgSubscribeItem *sub_item;
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
                // if fail on authentication, unable to 
                if (object->onEventAuthentication(msg, this))
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
    msg->type(FDB_MT_STATUS); // correct the type
    msg->checkLogEnabled(mContainer->owner(), false);
    msg->sendStatus(this, error_code, error_msg);
}

void CFdbSession::doUpdate(CFdbMessageHeader &head,
                           CFdbMessage::CFdbMsgPrefix &prefix,
                           uint8_t *buffer)
{
    CFdbMessage *msg = new CFdbMessage(head, prefix, buffer, mSid);
    CFdbBaseObject *object = mContainer->owner()->getObject(msg, true);
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
    CFdbMessage *msg = castToMessage<CFdbMessage *>(job);
    if (msg)
    {
        msg->setErrorMsg(FDB_MT_UNKNOWN, status, reason);
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
    if (sinfo.mSocketInfo.mAddress->mType == FDB_SOCKET_IPC)
    {
        return false;
    }
    host_ip = sinfo.mConn->mSelfIp;
    return true;
}

bool CFdbSession::peerIp(std::string &host_ip)
{
    CFdbSessionInfo sinfo;
    getSessionInfo(sinfo);
    if (sinfo.mSocketInfo.mAddress->mType == FDB_SOCKET_IPC)
    {
        return false;
    }
    host_ip = sinfo.mConn->mPeerIp;
    return true;

}
