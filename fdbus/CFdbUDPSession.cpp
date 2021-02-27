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

#include <utils/CFdbUDPSession.h>
#include <common_base/CFdbSessionContainer.h>
#include <common_base/CFdbContext.h>
#include <common_base/CBaseEndpoint.h>
#include <common_base/CLogProducer.h>
#include <common_base/CSocketImp.h>
#include <common_base/CFdbMessage.h>
#include <utils/Log.h>
#include <utils/CFdbIfMessageHeader.h>

#define FDB_UDP_RECEIVE_BUFFER_SIZE     (64 * 1024 - 1)

CFdbUDPSession::CFdbUDPSession(CFdbSessionContainer *container, CSocketImp *socket)
    : CBaseFdWatch(socket->getFd(), POLLIN | POLLHUP | POLLERR)
    , mContainer(container)
    , mSocket(socket)
{
}

CFdbUDPSession::~CFdbUDPSession()
{
    mContainer->mUDPSession = 0;
    if (mSocket)
    {
        delete mSocket;
        mSocket = 0;
    }
    descriptor(0);
}

bool CFdbUDPSession::sendMessage(const uint8_t *buffer, int32_t size, const CFdbSocketAddr &dest_addr)
{
    if (FDB_VALID_PORT(dest_addr.mPort) && !dest_addr.mAddr.empty())
    {
        auto sent = mSocket->send(buffer, size, dest_addr);
        return sent == size;
    }
    return false;
}

bool CFdbUDPSession::sendMessage(CFdbMessage *msg, const CFdbSocketAddr &dest_addr)
{
    if (!msg->buildHeader())
    {
        return false;
    }
    if (sendMessage(msg->getRawBuffer(), msg->getRawDataSize(), dest_addr))
    {
        if (msg->isLogEnabled())
        {
            auto logger = FDB_CONTEXT->getLogger();
            if (logger)
            {
                logger->logFDBus(msg, 0, mContainer->owner());
            }
        }
        return true;
    }
    return false;
}

int32_t CFdbUDPSession::receiveData(uint8_t *buf, int32_t size)
{
    return mSocket->recv(buf, size);
}

void CFdbUDPSession::onInput()
{
    uint8_t rx_buffer[FDB_UDP_RECEIVE_BUFFER_SIZE];
    int32_t rx_size = receiveData(rx_buffer, sizeof(rx_buffer));
    if (rx_size < CFdbMessage::mPrefixSize)
    {
        return;
    }

    CFdbMsgPrefix prefix(rx_buffer);
    if ((uint32_t)rx_size < prefix.mTotalLength)
    {
        return;
    }

    int32_t data_size = prefix.mTotalLength - CFdbMessage::mPrefixSize;
    if (data_size < 0)
    {
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
        LOG_E("CFdbUDPSession: Unable to allocate buffer of size %d!\n",
                CFdbMessage::mPrefixSize + data_size);
        fatalError(true);
        return;
    }
    uint8_t *head_start = whole_buf + CFdbMessage::mPrefixSize;
    memcpy(head_start, rx_buffer + CFdbMessage::mPrefixSize, data_size);

    NFdbBase::CFdbMessageHeader head;
    CFdbParcelableParser parser(head);
    if (!parser.parse(head_start, prefix.mHeadLength))
    {
        LOG_E("CFdbUDPSession: Unable to deserialize message head!\n");
        delete[] whole_buf;
        fatalError(true);
        return;
    }

    switch (head.type())
    {
        case FDB_MT_BROADCAST:
            doBroadcast(head, prefix, whole_buf);
        break;
        case FDB_MT_REQUEST:
        case FDB_MT_PUBLISH:
            doRequest(head, prefix, whole_buf);
        break;
        default:
            LOG_E("CFdbUDPSession: Message %d: Unknown type!\n", (int32_t)head.serial_number());
            delete[] whole_buf;
        break;
    }
}

void CFdbUDPSession::onError()
{
}

void CFdbUDPSession::onHup()
{
}

void CFdbUDPSession::doBroadcast(NFdbBase::CFdbMessageHeader &head,
                                 CFdbMsgPrefix &prefix, uint8_t *buffer)
{
    auto msg = new CFdbMessage(head, prefix, buffer, FDB_INVALID_ID);
    auto object = mContainer->owner()->getObject(msg, false);
    CBaseJob::Ptr msg_ref(msg);
    if (object)
    {
        msg->decodeDebugInfo(head);
        object->doBroadcast(msg_ref);
    }
}

void CFdbUDPSession::doRequest(NFdbBase::CFdbMessageHeader &head,
                               CFdbMsgPrefix &prefix, uint8_t *buffer)
{
    auto msg = new CFdbMessage(head, prefix, buffer, FDB_INVALID_ID);
    auto object = mContainer->owner()->getObject(msg, true);
    CBaseJob::Ptr msg_ref(msg);

    if (object)
    {
        msg->decodeDebugInfo(head);
        switch (head.type())
        {
            case FDB_MT_REQUEST:
                if (mContainer->owner()->onMessageAuthentication(msg))
                {
                    object->doInvoke(msg_ref);
                }
                else
                {
                }
            break;
            case FDB_MT_PUBLISH:
                if (mContainer->owner()->onEventAuthentication(msg))
                {
                    object->doPublish(msg_ref);
                }
                else
                {
                }
            break;
            default:
            break;
        }
    }
    else
    {
    }
}
