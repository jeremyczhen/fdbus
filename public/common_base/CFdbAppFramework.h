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

class CBaseClient;
class CBaseServer;

class CFdbAPPFramework
{
public:
    static CFdbAPPFramework *getInstance()
    {
        if (!mInstance)
        {
            mInstance = new CFdbAPPFramework();
        }
        return mInstance;
    }
    CBaseClient *findClient(const char *bus_name);
    CBaseServer *findService(const char *bus_name);
    const std::string &name()
    {
        return mName;
    }
    void name(const char *n)
    {
        if (n)
        {
            mName = n;
        }
    }

private:
    typedef std::map<std::string, CBaseClient *> tAFClientTbl;
    typedef std::map<std::string, CBaseServer *> tAFServerTbl;
    std::string mName;

    static CFdbAPPFramework *mInstance;
    tAFClientTbl mClientTbl;
    tAFServerTbl mServerTbl;

    CFdbAPPFramework();
    bool registerClient(const char *bus_name, CBaseClient *client);
    bool registerService(const char *bus_name, CBaseServer *server);
friend class CFdbAFComponent;
};

#endif
