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

#include <common_base/CFdbSessionContainer.h>
#include <common_base/CFdbContext.h>
#include <common_base/CBaseEndpoint.h>
#include <utils/Log.h>
#include <algorithm>

CFdbSessionContainer::CFdbSessionContainer(FdbSocketId_t skid
        , CBaseEndpoint *owner)
    : mSkid(skid)
    , mOwner(owner)
    , mEnableSessionDestroyHook(true)
{
}

CFdbSessionContainer::~CFdbSessionContainer()
{
    CFdbContext::getInstance()->deleteSession(this);
    if (!mConnectedSessionTable.empty())
    {
        LOG_E("CFdbSessionContainer: Untracked sessions are found!!!\n");
    }

    CFdbSessionContainer *self = 0;
    auto it = mOwner->retrieveEntry(mSkid, self);
    if (self)
    {
        mOwner->deleteEntry(it);
    }
}

CFdbSession *CFdbSessionContainer::getDefaultSession()
{
    auto it = mConnectedSessionTable.begin();
    return (it == mConnectedSessionTable.end()) ? 0 : *it;
}

void CFdbSessionContainer::addSession(CFdbSession *session)
{
    for (auto it = mConnectedSessionTable.begin();
            it != mConnectedSessionTable.end(); ++it)
    {
        if (*it == session)
        {
            return;
        }
    }

    mConnectedSessionTable.push_back(session);
}

void CFdbSessionContainer::removeSession(CFdbSession *session)
{
    fdb_remove_value_from_container(mConnectedSessionTable, session);
}

void CFdbSessionContainer::callSessionDestroyHook(CFdbSession *session)
{
    if (mEnableSessionDestroyHook)
    {
        onSessionDeleted(session);
    }
}

