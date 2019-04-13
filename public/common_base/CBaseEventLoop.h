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

#ifndef _CBASEEVENTLOOP_H_
#define _CBASEEVENTLOOP_H_

#include <list>
#include <set>

class CSysLoopTimer;
class CBaseWorker;
class CBaseEventLoop
{
public:
    CBaseEventLoop();
    virtual ~CBaseEventLoop();
    /*
     * The functions are used internally only.
     */
    void addTimer(CSysLoopTimer *timer, bool enb);
    void removeTimer(CSysLoopTimer *timer);

    virtual void dispatch() = 0;
    virtual bool notify() = 0;
    virtual bool acknowledge() = 0;
    virtual bool init(CBaseWorker *worker) = 0;

protected:
    int32_t getMostRecentTime();
    void processTimers();
#define LOOP_DEFAULT_INTERVAL       20
private:
    typedef std::list< CSysLoopTimer *> tLoopTimerList;
    typedef std::set<CSysLoopTimer *> tTimerTbl;

    bool timerDestroyed(CSysLoopTimer *timer);
    void addTimerToBlacklist(CSysLoopTimer *timer);
    void uninstallTimers();
    void enableTimerBlackList(tTimerTbl *black_list)
    {
        mTimerBlackList = black_list;
    }
    

    tLoopTimerList mTimerList;
    tLoopTimerList mTimerWorkingList;
    tTimerTbl *mTimerBlackList;
    friend CSysLoopTimer;
};

#endif
