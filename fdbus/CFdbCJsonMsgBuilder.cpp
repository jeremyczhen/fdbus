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

#include <string.h>
#include <stdlib.h>
#include <common_base/CFdbCJsonMsgBuilder.h>
#define FDB_LOG_TAG "FDB_TEST_CLIENT"
#include <common_base/fdb_log_trace.h>
#include <common_base/cJSON/cJSON.h>

CFdbCJsonMsgBuilder::CFdbCJsonMsgBuilder(const cJSON *message)
    : mMessage(message)
    , mJson(0)
    , mSize(0)
{
}

int32_t CFdbCJsonMsgBuilder::build()
{
    mJson = cJSON_PrintUnformatted(mMessage);
    mSize = (int32_t)strlen(mJson) + 1; // take EOS into consideration
    return mSize;
}

bool CFdbCJsonMsgBuilder::toBuffer(uint8_t *buffer, int32_t size)
{
    int32_t s = size;
    if (size > mSize)
    {
        s = mSize;
    }
    if (mJson)
    {
        strncpy((char *)buffer, mJson, s);
    }
    return true;
}

bool CFdbCJsonMsgBuilder::toString(std::string &msg_txt) const
{
    char *str = cJSON_Print(mMessage);
    msg_txt = str;
    cJSON_free(str);
    return true;
}

CFdbCJsonMsgBuilder::~CFdbCJsonMsgBuilder()
{
    if (mJson)
    {
        cJSON_free((void *)mJson);
    }
}

CFdbCJsonMsgParser::CFdbCJsonMsgParser()
    : mMessage(0)
{
}

CFdbCJsonMsgParser::~CFdbCJsonMsgParser()
{
    if (mMessage)
    {
        cJSON_Delete(mMessage);
    }
}

cJSON *CFdbCJsonMsgParser::retrieve()
{
    return mMessage;
}

cJSON *CFdbCJsonMsgParser::detach()
{
    cJSON *msg = mMessage;
    mMessage = 0;
    return msg;
}

bool CFdbCJsonMsgParser::parse(const uint8_t *buffer, int32_t size)
{
    mMessage = cJSON_Parse((const char *)buffer);
    return mMessage != 0;
}

