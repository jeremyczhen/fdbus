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

#include <stdlib.h>
#include <common_base/CFdbAppFramework.h>
#include <common_base/CFdbContext.h>

CFdbAPPFramework *CFdbAPPFramework::mInstance = 0;

CFdbAPPFramework::CFdbAPPFramework()
{
    FDB_CONTEXT->start();
    const char *app_name = getenv("FDB_CONFIG_APP_NAME");
    mName = app_name ? app_name : "anonymous-app";
}

CBaseClient *CFdbAPPFramework::findClient(const char *bus_name)
{
    if (!bus_name)
    {
        return 0;
    }
    auto it = mClientTbl.find(bus_name);
    if (it == mClientTbl.end())
    {
        return 0;
    }
    return it->second;
}

bool CFdbAPPFramework::registerClient(const char *bus_name, CBaseClient *client)
{
    if (!bus_name || !client)
    {
        return false;
    }
    if (mClientTbl.find(bus_name) != mClientTbl.end())
    {
        return false;
    }
    mClientTbl[bus_name] = client;
    return true;
}

CBaseServer *CFdbAPPFramework::findService(const char *bus_name)
{
    if (!bus_name)
    {
        return 0;
    }
    auto it = mServerTbl.find(bus_name);
    if (it == mServerTbl.end())
    {
        return 0;
    }
    return it->second;
}
bool CFdbAPPFramework::registerService(const char *bus_name, CBaseServer *server)
{
    if (!bus_name || !server)
    {
        return false;
    }
    if (mServerTbl.find(bus_name) != mServerTbl.end())
    {
        return false;
    }
    mServerTbl[bus_name] = server;
    return true;
}

