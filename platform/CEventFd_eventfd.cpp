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

#include <errno.h>
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
    mEventFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (mEventFd > 0)
    {
        ret = true;
        fd = mEventFd;
    }
    return ret;
}

bool CEventFd::pickEvent()
{
    eventfd_t val;
    int err;
    do
    {
        err = eventfd_read(mEventFd, &val);
    }
    while ((err < 0) && (errno == EINTR));
    return (err >= 0) || (errno == EAGAIN);
}

bool CEventFd::triggerEvent()
{
    int err;
    do
    {
        err = eventfd_write(mEventFd, 1);
    }
    while ((err < 0) && (errno == EINTR));
    return err >= 0;
}
