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

#ifndef __CFDBWATCHDOG_H__
#define __CFDBWATCHDOG_H__

#include <map>
#include <string>
#include <common_base/CFdbSimpleMsgBuilder.h>
#include <common_base/CBaseLoopTimer.h>
#include <common_base/common_defs.h>

class CFdbBaseObject;
class CFdbSession;
class CBaseWorker;
class CFdbMsgProcessList;

class CFdbWatchdog : public CBaseLoopTimer
{
public:
    CFdbWatchdog(CFdbBaseObject *obj, int32_t interval = 0, int32_t max_retries = 0);
    void addDog(CFdbSession *session);
    void removeDog(CFdbSession *session);
    void feedDog(CFdbSession *session);
    void start(int32_t interval = 0, int32_t max_retries = 0);
    void stop();
    void getDroppedProcesses(CFdbMsgProcessList &process_list);
protected:
    void run();
private:
    struct CDog
    {
        int32_t mRetries;
        bool mDropped;
        void reset(int32_t retries = FDB_WATCHDOG_RETRIES)
        {
            mRetries = retries;
            mDropped = false;
        }
    };

    typedef std::map<FdbSessionId_t, CDog> tDogs;

    tDogs mDogs;
    CFdbBaseObject *mObject;
    int32_t mMaxRetries;
};

class CFdbMsgProcessInfo : public IFdbParcelable
{
public:
    std::string &client_name()
    {
        return mClientName;
    }
    void set_client_name(const char *name)
    {
        if (name)
        {
            mClientName = name;
        }
    }
    uint32_t pid() const
    {
        return mPid;
    }
    void set_pid(uint32_t pid)
    {
        mPid = pid;
    }
    void serialize(CFdbSimpleSerializer &serializer) const
    {
        serializer << mClientName
                   << mPid;
    }
    
    void deserialize(CFdbSimpleDeserializer &deserializer)
    {
        deserializer >> mClientName
                     >> mPid;
    }
private:
    std::string mClientName;
    uint32_t mPid;
};

class CFdbMsgProcessList : public IFdbParcelable
{
public:
    CFdbParcelableArray<CFdbMsgProcessInfo> &process_list()
    {
        return mProcessList;
    }
    CFdbMsgProcessInfo *add_process_list()
    {
        return mProcessList.Add();
    }
    void serialize(CFdbSimpleSerializer &serializer) const
    {
        serializer << mProcessList;
    }
    
    void deserialize(CFdbSimpleDeserializer &deserializer)
    {
        deserializer >> mProcessList;
    }
private:
    CFdbParcelableArray<CFdbMsgProcessInfo> mProcessList;
};

#endif
