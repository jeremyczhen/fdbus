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
#include <common_base/CFdbToken.h>
#include <common_base/cJSON/cJSON.h>
#include <utils/Log.h>
#include <security/CFdbusSecurityConfig.h>
#include "CHostSecurityConfig.h"

void CHostSecurityConfig::addPermission(const void *json_handle, CHostSecCfg &cfg,
                                        int32_t level, std::string &err_msg)
{
    const auto item_id = (const cJSON *)json_handle;
    if (cJSON_IsArray(item_id))
    {
        for (int k = 0; k < cJSON_GetArraySize(item_id); ++k)
        {
            auto id = cJSON_GetArrayItem(item_id, k);
            if (cJSON_IsString(id))
            {
                cfg.mCredSecLevelTbl[id->valuestring] = level;
            }
            else
            {
                err_msg = "not string - 1";
                return;
            }
        }
    }
    else if (cJSON_IsString(item_id))
    {
        if (!strcmp(item_id->valuestring, "default"))
        {
            cfg.mDefaultLevel = level;
        }
        else
        {
            err_msg = "if 'id' is string, only array or 'default' is allowed.";
            return;
        }
    }
    else
    {
        err_msg = "'id' should be array or string";
        return;
    }
}

void CHostSecurityConfig::parseHostSecurityConfig(const char *json_str, std::string &err_msg)
{
    auto cfg_root = cJSON_Parse(json_str);
    if (cfg_root)
    {
        if (cJSON_IsObject(cfg_root))
        {
            auto item_host = cJSON_GetObjectItem(cfg_root, "host");
            if (item_host)
            {
                if (cJSON_IsObject(item_host))
                {
                    for (int32_t i = 0; i < cJSON_GetArraySize(item_host); ++i)
                    {
                        auto host = cJSON_GetArrayItem(item_host, i);
                        if (cJSON_IsObject(host))
                        {
                            CHostSecCfg &host_perm = mHostSecLevelTbl[host->string];
                            host_perm.mDefaultLevel = FDB_SECURITY_LEVEL_NONE;
                            auto item_cred = cJSON_GetObjectItem(host, "cred");
                            if (item_cred)
                            {
                                if (cJSON_IsString(item_cred))
                                {
                                    host_perm.mCred = item_cred->valuestring;
                                }
                                else
                                {
                                    err_msg = "not string - 1";
                                    goto _quit;
                                }
                            }
                            auto item_perms = cJSON_GetObjectItem(host, "permission");
                            if (item_perms)
                            {
                                if (cJSON_IsArray(item_perms))
                                {
                                    for (int32_t j = 0; j < cJSON_GetArraySize(item_perms); ++j)
                                    {
                                        auto sec_level = cJSON_GetArrayItem(item_perms, j);
                                        if (cJSON_IsObject(sec_level))
                                        {
                                            auto level = cJSON_GetObjectItem(sec_level, "level");
                                            if (level)
                                            {
                                                if (cJSON_IsNumber(level))
                                                {
                                                    int32_t level_val = level->valueint;
                                                    if (level_val < 0)
                                                    {
                                                        level_val = FDB_SECURITY_LEVEL_NONE;
                                                    }
                                                    if (level_val > CFdbusSecurityConfig::getNrSecurityLevel())
                                                    {
                                                        level_val = CFdbusSecurityConfig::getNrSecurityLevel();
                                                    }
                                                    auto item_hosts = cJSON_GetObjectItem(sec_level, "hosts");
                                                    if (item_hosts)
                                                    {
                                                        addPermission(item_hosts, host_perm, level_val, err_msg);
                                                    }
                                                }
                                                else
                                                {
                                                    err_msg = "'level' is not number";
                                                    goto _quit;
                                                }
                                            }
                                            else
                                            {
                                                err_msg = "'level' is not found";
                                                goto _quit;
                                            }
                                        }
                                        else
                                        {
                                            err_msg = "not object - 1";
                                            goto _quit;
                                        }
                                    }
                                }
                                else
                                {
                                    err_msg = "not array - 1";
                                    goto _quit;
                                }
                            }
                        }
                        else
                        {
                            err_msg = "not object - 2";
                            goto _quit;
                        }
                    }
                }
                else
                {
                    err_msg = "not number - 1";
                    goto _quit;
                }
            }
        }
    }

_quit:
    if (cfg_root)
    {
        cJSON_Delete(cfg_root);
    }
}


void CHostSecurityConfig::importSecurity()
{
    if (!mHostSecLevelTbl.empty())
    {
        return;
    }
    CFdbusSecurityConfig::importSecurity();
    
    const char *config_file = FDB_CFG_CONFIG_PATH
                              "/host"
                              FDB_CFG_CONFIG_FILE_SUFFIX;
    void *buffer = CFdbusSecurityConfig::readFile(config_file);
    if (buffer)
    {
        std::string err_msg;
        parseHostSecurityConfig((char *)buffer, err_msg);
        if (!err_msg.empty())
        {
            LOG_E("CHostSecurityConfig: error when parsing %s: %s\n",
                    config_file, err_msg.c_str());
        }
        free(buffer);
    }
}

int32_t CHostSecurityConfig::getSecurityLevel(const char *this_host, const char *that_host) const
{
    auto perm_it = mHostSecLevelTbl.find(this_host);
    int32_t security_level = FDB_SECURITY_LEVEL_NONE;
    if (perm_it != mHostSecLevelTbl.end())
    {
        const auto &sec_cfg = perm_it->second;
        const auto &level_tbl = sec_cfg.mCredSecLevelTbl;
        auto level_it = level_tbl.find(that_host);
        security_level = level_it == level_tbl.end() ? sec_cfg.mDefaultLevel : level_it->second;
    }

    return security_level;
}

const std::string *CHostSecurityConfig::getCred(const char *host_name) const
{
    auto perm_it = mHostSecLevelTbl.find(host_name);
    if (perm_it != mHostSecLevelTbl.end())
    {
        return &perm_it->second.mCred;
    }
    return 0;
}

#else
#include "CHostSecurityConfig.h"
#include <security/CFdbusSecurityConfig.h>

void CHostSecurityConfig::importSecurity()
{
}

int32_t CHostSecurityConfig::getSecurityLevel(const char *this_host, const char *that_host) const
{
    return CFdbusSecurityConfig::getNrSecurityLevel() - 1;
}

const std::string *CHostSecurityConfig::getCred(const char *host_name) const
{
    return 0;
}

#endif

