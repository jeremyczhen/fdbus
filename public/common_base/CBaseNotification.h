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

#ifndef _CBASENOTIFICATION_
#define _CBASENOTIFICATION_

#include <memory>
#include "common_defs.h"

class CBaseWorker;

template<typename T>
class CBaseNotification
{
public:
    typedef std::shared_ptr<CBaseNotification> Ptr;
    CBaseNotification(FdbEventCode_t event, CBaseWorker *worker = 0)
        : mEvent(event)
        , mWorker(worker)
    {
    }

    CBaseNotification(CBaseWorker *worker = 0)
        : mEvent(FDB_INVALID_ID)
        , mWorker(worker)
    {
    }
    
    virtual ~CBaseNotification()
    {
    }

    FdbEventCode_t event()
    {
        return mEvent;
    }
    void event(FdbEventCode_t evt)
    {
        mEvent = evt;
    }

    void worker(CBaseWorker *wker)
    {
        mWorker = wker;
    }

    CBaseWorker *worker()
    {
        return mWorker;
    }
    
protected:
    /*
     * run() will be executed once the specified event happens.
     */
    virtual void run(T &data) {}
    
private:
    FdbEventCode_t mEvent;
    CBaseWorker *mWorker;

    template<typename>
    friend class CBaseNotificationCenter;
};

typedef CBaseNotification<void *> CGenericNotification;
#endif

