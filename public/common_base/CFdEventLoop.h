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

#ifndef _CFDEVENTLOOP_H_
#define _CFDEVENTLOOP_H_

#include <list>
#include <set>
#include "CBaseSysDep.h"
#include "CBaseEventLoop.h"
#include "CEventFd.h"

class CSysFdWatch;
class CNotifyFdWatch;
class CFdEventLoop : public CBaseEventLoop
{
public:
    CFdEventLoop();
    ~CFdEventLoop();

    void addWatch(CSysFdWatch *watch, bool enb);
    void removeWatch(CSysFdWatch *watch);
    void dispatch();
    bool notify();
    bool acknowledge();
    bool init(CBaseWorker *worker);
protected:
    typedef std::list< CSysFdWatch *> tCFdWatchList;
    typedef std::set<CSysFdWatch *> tWatchTbl;

    bool watchDestroyed(CSysFdWatch *watch);
    void addWatchToBlacklist(CSysFdWatch *watch);
    void uninstallWatches();
    void enableWatchBlackList(tWatchTbl *black_list)
    {
        mWatchBlackList = black_list;
    }

    pollfd *mFds;
    int32_t mFdsSize;
    tCFdWatchList mWatchList;
    tCFdWatchList mWatchWorkingList;
    CSysFdWatch **mWatches;
    tWatchTbl *mWatchBlackList;
private:
    CNotifyFdWatch *mNotifyWatch;
    CEventFd mEventFd;
    void processNotifyWatch(bool &io_error);
    int32_t buildFdArray();
    void processWatches(int32_t nr_watches);

    friend CSysFdWatch;
};

#endif
