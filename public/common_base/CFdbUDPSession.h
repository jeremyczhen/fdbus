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

#ifndef __CFDBUDPSESSION_H__
#define __CFDBUDPSESSION_H__

#include <string>
#include "common_defs.h"
#include "CBaseFdWatch.h"

class CFdbSessionContainer;
class CSocketImp;
struct CFdbSocketAddr;
class CFdbMessage;
struct CFdbMsgPrefix;
namespace NFdbBase {
    class CFdbMessageHeader;
}

class CFdbUDPSession : public CBaseFdWatch
{
public:
    CFdbUDPSession(CFdbSessionContainer *container, CSocketImp *socket);
    virtual ~CFdbUDPSession();

    bool sendMessage(const uint8_t *buffer, int32_t size, const CFdbSocketAddr &dest_addr);
    bool sendMessage(CFdbMessage *msg, const CFdbSocketAddr &dest_addr);

    const std::string &senderName() const
    {
        return mSenderName;
    }
    void senderName(const char *name)
    {
        mSenderName = name;
    }

protected:
    void onInput(bool &io_error);
    void onError();
    void onHup();

private:
    CFdbSessionContainer *mContainer;
    CSocketImp *mSocket;
    std::string mSenderName;

    int32_t receiveData(uint8_t *buf, int32_t size);
    void doBroadcast(NFdbBase::CFdbMessageHeader &head, CFdbMsgPrefix &prefix, uint8_t *buffer);
};

#endif
