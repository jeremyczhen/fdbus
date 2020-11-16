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

#include "CFdbWatchdog.h"
#include <common_base/CFdbBaseObject.h>
#include <common_base/CFdbContext.h>
#include <common_base/CFdbSession.h>

CFdbWatchdog::CFdbWatchdog(CFdbBaseObject *obj, int32_t interval, int32_t max_retries)
                           : CBaseLoopTimer(interval ? interval : FDB_WATCHDOG_INTERVAL, true)
                           , mObject(obj)
                           , mMaxRetries(max_retries ? max_retries : FDB_WATCHDOG_RETRIES)
{
    attach(FDB_CONTEXT);
}

void CFdbWatchdog::addDog(CFdbSession *session)
{
    CFdbSessionInfo info;
    session->getSessionInfo(info);
    if (info.mContainerSocket.mAddress->mType != FDB_SOCKET_IPC)
    {
        // Only local endpoints can be monitored.
#ifdef __WIN32__
        if (info.mContainerSocket.mAddress->mAddr.compare(FDB_LOCAL_HOST))
        {
            return;
        }
#else
        return;
#endif
    }
    auto &target = mDogs[session->sid()];
    target.reset();
}

void CFdbWatchdog::removeDog(CFdbSession *session)
{
    auto it = mDogs.find(session->sid());
    if (it != mDogs.end())
    {
        it->second.mDropped = true;
        mObject->onBark(session);
        mDogs.erase(it);
    }
}

void CFdbWatchdog::feedDog(CFdbSession *session)
{
    auto it = mDogs.find(session->sid());
    if (it != mDogs.end())
    {
        it->second.reset(mMaxRetries);
    }
}

void CFdbWatchdog::run()
{
    for (auto it = mDogs.begin(); it != mDogs.end();)
    {
        auto the_it = it;
        ++it;
        auto sid = the_it->first;
        auto &dog = the_it->second;
        auto session = FDB_CONTEXT->getSession(sid);
        if (!session)
        {
            mDogs.erase(the_it);
            continue;
        }
        if (dog.mRetries-- <= 0)
        {
            dog.reset();
            dog.mDropped = true;
            mObject->onBark(session);
        }
        else
        {
            mObject->kickDog(session);
        }
    }
}

void CFdbWatchdog::start(int32_t interval, int32_t max_retries)
{
    if (max_retries)
    {
        mMaxRetries = max_retries;
    }
    enable(interval);
}

void CFdbWatchdog::stop()
{
    disable();
}

void CFdbWatchdog::getDroppedProcesses(CFdbMsgProcessList &process_list)
{
    for (auto it = mDogs.begin(); it != mDogs.end(); ++it)
    {
        auto sid = it->first;
        auto &dog = it->second;
        if (dog.mDropped)
        {
            auto session = FDB_CONTEXT->getSession(sid);
            if (session)
            {
                auto process = process_list.add_process_list();
                process->set_client_name(session->senderName().c_str());
                process->set_pid((uint32_t)(session->pid()));
            }
        }
    }
}

