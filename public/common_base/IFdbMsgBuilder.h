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

#ifndef __IFDBSERIALIZER_H__
#define __IFDBSERIALIZER_H__

#include <string>

class IFdbMsgBuilder
{
public:
    virtual int32_t build()
    {
        return -1;
    }
    virtual bool toBuffer(uint8_t *buffer, int32_t size)
    {
        return true;
    }
    virtual bool toString(std::string &msg_txt) const
    {
        return true;
    }
    virtual ~IFdbMsgBuilder()
    {
    }
};

class IFdbMsgParser
{
public:
    virtual bool parse(const uint8_t *buffer, int32_t size)
    {
        return true;
    }
    virtual ~IFdbMsgParser()
    {
    }
};
    
#endif

