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

#ifndef _CBASEPIPE_HPP_
#define _CBASEPIPE_HPP_

#include "CBaseSysDep.h"

class CBasePipe
{
public:
    // Invalid CBasePipe file descriptor
    static const int32_t INVALID_FD = -1;

    CBasePipe();
    ~CBasePipe();

    bool open(bool blockOnRead = false, bool blockOnWrite = true);
    bool close();

    int32_t getReadFd() const;
    int32_t getWriteFd() const;

    int32_t read(void *buf, uint32_t nBytes);
    int32_t write(const void *buf, uint32_t nBytes);

private:
    int32_t  mReadFd;
    int32_t  mWriteFd;
};

#endif /* Guard for PIPE_HPP_ */
