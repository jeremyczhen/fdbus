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

#include <common_base/CEventFd.h>

CEventFd::CEventFd()
{
}

CEventFd::~CEventFd()
{
}

bool CEventFd::create(int &fd)
{
    bool ret = false;
    if (mPipe.open())
    {
        fd = mPipe.getReadFd();
        ret = true;
    }
    return ret;
}

bool CEventFd::pickEvent()
{
    uint8_t dummy;
    int32_t ret = mPipe.read(&dummy, 1);
    return (ret < 0) ? false : true;
}

bool CEventFd::triggerEvent()
{
    uint8_t dummy = 0;
    int32_t ret = mPipe.write(&dummy, 1);
    return (ret < 0) ? false : true;
}
