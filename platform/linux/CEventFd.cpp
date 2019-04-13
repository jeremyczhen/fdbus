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

#include <sys/eventfd.h>
#include <unistd.h>
#include <common_base/CEventFd.h>

CEventFd::CEventFd()
    : mEventFd(-1)
{
}

CEventFd::~CEventFd()
{
}

bool CEventFd::create(int &fd)
{
    bool ret = false;
    mEventFd = eventfd(0, EFD_NONBLOCK);
    if (mEventFd > 0)
    {
        ret = true;
        fd = mEventFd;
    }
    return ret;
}

bool CEventFd::pickEvent()
{
    uint64_t val;
    ssize_t s;

    s = read(mEventFd, &val, sizeof(uint64_t));
    return (s == sizeof(uint64_t)) ? true : false;
}

bool CEventFd::triggerEvent()
{
    uint64_t val = 1;
    ssize_t s;
    s = write(mEventFd, &val, sizeof(uint64_t));
    return (s == sizeof(uint64_t)) ? true : false;
}
