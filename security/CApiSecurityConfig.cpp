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

#ifdef CFG_FDBUS_SECURITY
#include <string.h>
#include <stdlib.h>
#include <common_base/cJSON/cJSON.h>
#include <security/CFdbusSecurityConfig.h>
#include <common_base/CApiSecurityConfig.h>
#include <utils/Log.h>

void CApiSecurityConfig::parseApiConfig(const void *json_handle, CApiSecLevelTbl &cfg, std::string &err_msg)
{
    const cJSON *json = (const cJSON *)json_handle;
    if (cJSON_IsArray(json))
    {
        for (int i = 0; i < cJSON_GetArraySize(json); ++i)
        {
            cJSON *sec_level = cJSON_GetArrayItem(json, i);
            if (sec_level)
            {
                if (cJSON_IsObject(sec_level))
                {
                    cJSON *item_level = cJSON_GetObjectItem(sec_level, "level");
                    if (item_level)
                    {
                        if (cJSON_IsNumber(item_level))
                        {
                            int32_t level = (int32_t)item_level->valueint;
                            if (level < 0)
                            {
                                level = FDB_SECURITY_LEVEL_NONE;
                            }
                            cJSON *item_default = cJSON_GetObjectItem(sec_level, "default");
                            if (item_default)
                            {
                                if (cJSON_IsTrue(item_default))
                                {
                                    cfg.mDefaultLevel = level;
                                    continue;
                                }
                            }
                            else
                            {
                                err_msg = "default should be true or false.";
                                return;
                            }
                            cJSON *item_group = cJSON_GetObjectItem(sec_level, "group");
                            if (cJSON_IsNumber(item_group))
                            {
                                auto group = fdbmakeEventGroup(item_group->valueint);
                                cfg.mApiSecRangeTbl.push_back({group, group, level});
                                continue;
                            }
                            cJSON *item_from = cJSON_GetObjectItem(sec_level, "from");
                            if (item_from)
                            {
                                if (cJSON_IsNumber(item_from))
                                {
                                    cJSON *item_to = cJSON_GetObjectItem(sec_level, "to");
                                    if (item_to)
                                    {
                                        if (cJSON_IsNumber(item_to))
                                        {
                                            if (item_from->valueint <= item_to->valueint)
                                            {
                                                cfg.mApiSecRangeTbl.push_back(
                                                    {item_from->valueint,
                                                     item_to->valueint,
                                                     level}
                                                );
                                            }
                                            else
                                            {
                                                err_msg = "'from' < 'to'.";
                                                return;
                                            }
                                        }
                                        else
                                        {
                                            err_msg = "'to' should be number.";
                                            return;
                                        }
                                    }
                                    else
                                    {
                                        err_msg = "'to' is not found.";
                                        return;
                                    }
                                }
                                else
                                {
                                    err_msg = "'from' should be string or number.";
                                    return;
                                }
                            }
                            else
                            {
                                err_msg = "'from' is not found.";
                                return;
                            }
                        }
                        else
                        {
                            err_msg = "'level' should be number.";
                            return;
                        }
                    }
                    else
                    {
                        err_msg = "'level' is not found.";
                        return;
                    }
                }
                else
                {
                    err_msg = "not object - 1.";
                    return;
                }
            }
        }
    }
    else
    {
        err_msg = "not array - 1.";
    }
}

void CApiSecurityConfig::parseSecurityConfig(const char *json_str, std::string &err_msg)
{
    auto cfg_root = cJSON_Parse(json_str);
    if (cfg_root)
    {
        if (cJSON_IsObject(cfg_root))
        {
            auto message = cJSON_GetObjectItem(cfg_root, "message");
            if (message == NULL)
            {
                err_msg = "'message' is not found!";
                goto _quit;
            }
            parseApiConfig(message, mMessageSecLevelTbl, err_msg);
            if (!err_msg.empty())
            {
                goto _quit;
            }
            
            auto event = cJSON_GetObjectItem(cfg_root, "event");
            if (event == NULL)
            {
                err_msg = "'event' is not found!";
                goto _quit;
            }
            parseApiConfig(event, mEventSecLevelTbl, err_msg);
        }
    }
    else
    {
        err_msg = "unable to parse json string.";
    }
    
_quit:
    if (cfg_root)
    {
        cJSON_Delete(cfg_root);
    }
}

void CApiSecurityConfig::importSecLevel(const char *svc_name)
{
    if (!mMessageSecLevelTbl.mApiSecRangeTbl.empty() ||
           !mEventSecLevelTbl.mApiSecRangeTbl.empty())
    {
        return;
    }
    std::string cfg_file = FDB_CFG_CONFIG_PATH "/server/";
    cfg_file += svc_name;
    cfg_file += FDB_CFG_CONFIG_FILE_SUFFIX;
    void *buffer = CFdbusSecurityConfig::readFile(cfg_file.c_str());
    if (buffer)
    {
        std::string err_msg;
        parseSecurityConfig((char *)buffer, err_msg);
        if (!err_msg.empty())
        {
            mMessageSecLevelTbl.mApiSecRangeTbl.clear();
            mEventSecLevelTbl.mApiSecRangeTbl.clear();
            LOG_E("CApiSecurityConfig: error when parsing %s: %s\n",
                        cfg_file.c_str(), err_msg.c_str());
        }
        free(buffer);
    }
}

int32_t CApiSecurityConfig::getSecLevel(FdbMsgCode_t msg_code, CApiSecLevelTbl &tbl)
{
    for (tApiSecRangeTbl::const_iterator it = tbl.mApiSecRangeTbl.begin();
            it != tbl.mApiSecRangeTbl.end(); ++it)
    {
        const CSecLevelRange &range = *it;
        if ((msg_code >= range.mBegin) && (msg_code <= range.mEnd))
        {
            return range.mSecLevel;
        }
    }
    return tbl.mDefaultLevel;
}

int32_t CApiSecurityConfig::getMessageSecLevel(FdbMsgCode_t msg_code)
{
    return getSecLevel(msg_code, mMessageSecLevelTbl);
}

int32_t CApiSecurityConfig::getEventSecLevel(FdbMsgCode_t msg_code)
{
    return getSecLevel(msg_code, mEventSecLevelTbl);
}

#else
#include <common_base/CApiSecurityConfig.h>

void CApiSecurityConfig::importSecLevel(const char *svc_name)
{
}

int32_t CApiSecurityConfig::getMessageSecLevel(FdbMsgCode_t msg_code)
{
    return FDB_SECURITY_LEVEL_NONE;
}
int32_t CApiSecurityConfig::getEventSecLevel(FdbMsgCode_t msg_code)
{
    return FDB_SECURITY_LEVEL_NONE;
}

#endif

