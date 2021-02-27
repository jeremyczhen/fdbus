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

#ifndef __CFDBCJSONBUILDER_H__
#define __CFDBCJSONBUILDER_H__ 

#include <common_base/IFdbMsgBuilder.h>
struct cJSON;

class CFdbCJsonMsgBuilder : public IFdbMsgBuilder
{
public:
    CFdbCJsonMsgBuilder(const cJSON *message);
    ~CFdbCJsonMsgBuilder();
    int32_t build();
    bool toBuffer(uint8_t *buffer, int32_t size);
    bool toString(std::string &msg_txt) const;
    int32_t bufferSize()
    {
        return mSize;
    }
    const uint8_t *buffer()
    {
        return (const uint8_t *)mJson;
    }
    
private:
    const cJSON *mMessage;
    const char *mJson;
    int32_t mSize;
};

class CFdbCJsonMsgParser : public IFdbMsgParser
{
public:
    CFdbCJsonMsgParser();
    ~CFdbCJsonMsgParser();
    bool parse(const uint8_t *buffer, int32_t size);
    cJSON *retrieve();
    cJSON *detach();

private:
    cJSON *mMessage;
};

#endif
