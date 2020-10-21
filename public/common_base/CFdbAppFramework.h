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

#ifndef __CFDBAPPFRAMEWORK_H__
#define __CFDBAPPFRAMEWORK_H__

#include <string>
#include <map>
#include <functional>
#include "CBaseWorker.h"

class CFdbAFClient;
class CFdbAFServer;

class CFdbAPPFramework
{
public:
    typedef std::function<void(FdbSessionId_t sid, bool first_or_last)> tCallbackFn;
    static CFdbAPPFramework *getInstance()
    {
        if (!mInstance)
        {
            mInstance = new CFdbAPPFramework();
        }
        return mInstance;
    }

    CBaseWorker *defaultWorker()
    {
        return &mDefaultWorker;
    }

    CFdbAFClient *findAFClient(const char *bus_name);
    bool registerClient(const char *bus_name, CFdbAFClient *client);

    CFdbAFServer *findAFService(const char *bus_name);
    bool registerService(const char *bus_name, CFdbAFServer *server);
    const std::string &name()
    {
        return mName;
    }

private:
    typedef std::map<std::string, CFdbAFClient *> tAFClientTbl;
    typedef std::map<std::string, CFdbAFServer *> tAFServerTbl;
    std::string mName;

    static CFdbAPPFramework *mInstance;
    tAFClientTbl mClientTbl;
    tAFServerTbl mServerTbl;
    CBaseWorker mDefaultWorker;

    CFdbAPPFramework();
};

#endif
