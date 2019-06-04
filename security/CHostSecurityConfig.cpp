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
#include <common_base/CFdbToken.h>
#include <security/cJSON/cJSON.h>
#include <utils/Log.h>
#include <security/CFdbusSecurityConfig.h>
#include "CHostSecurityConfig.h"

void CHostSecurityConfig::addPermission(const void *json_handle, CHostSecCfg &cfg,
                                        int32_t level, EFdbusCredType type, std::string &err_msg)
{
    const cJSON *item_id = (const cJSON *)json_handle;
    if (cJSON_IsArray(item_id))
    {
        for (int k = 0; k < cJSON_GetArraySize(item_id); ++k)
        {
            cJSON *id = cJSON_GetArrayItem(item_id, k);
            if (cJSON_IsString(id))
            {
                if (type == FDB_PERM_CRED_MAC)
                {}
                else if (type == FDB_PERM_CRED_IP)
                {}
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
    cJSON *cfg_root = cJSON_Parse(json_str);
    if (cfg_root != NULL)
    {
        if (cJSON_IsObject(cfg_root))
        {
            cJSON *item_host = cJSON_GetObjectItem(cfg_root, "host");
            if (item_host != NULL)
            {
                if (cJSON_IsObject(item_host))
                {
                    for (int32_t i = 0; i < cJSON_GetArraySize(item_host); ++i)
                    {
                        cJSON *host = cJSON_GetArrayItem(item_host, i);
                        if (host)
                        {
                            if (cJSON_IsArray(host))
                            {
                                CPermission &host_perm = mHostSecLevelTbl[host->string];
                                for (int32_t j = 0; j < cJSON_GetArraySize(host); ++j)
                                {
                                    cJSON *sec_level = cJSON_GetArrayItem(host, j);
                                    if (cJSON_IsObject(sec_level))
                                    {
                                        cJSON *level = cJSON_GetObjectItem(sec_level, "level");
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
                                                cJSON *item_mac = cJSON_GetObjectItem(sec_level, "mac");
                                                if (item_mac)
                                                {
                                                    addPermission(item_mac, host_perm.mMacAddr,
                                                              level_val, FDB_PERM_CRED_MAC, err_msg);
                                                }
                                                cJSON *item_ip = cJSON_GetObjectItem(sec_level, "ip");
                                                if (item_ip)
                                                {
                                                    addPermission(item_ip, host_perm.mIpAddr,
                                                              level_val, FDB_PERM_CRED_IP, err_msg);
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
    if (buffer != NULL)
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

int32_t CHostSecurityConfig::getMacSecurityLevel(const char *mac, const CHostSecCfg &sec_cfg)
{
    const tCredSecLevelTbl &level_tbl = sec_cfg.mCredSecLevelTbl;
    tCredSecLevelTbl::const_iterator level_it = level_tbl.find(mac);
    return level_it == level_tbl.end() ? sec_cfg.mDefaultLevel : level_it->second;
}

int32_t CHostSecurityConfig::getIpSecurityLevel(const char *ip, const CHostSecCfg &sec_cfg)
{
    const tCredSecLevelTbl &level_tbl = sec_cfg.mCredSecLevelTbl;
    tCredSecLevelTbl::const_iterator level_it = level_tbl.find(ip);
    return level_it == level_tbl.end() ? sec_cfg.mDefaultLevel : level_it->second;
}

int32_t CHostSecurityConfig::getSecurityLevel(const char *host_name, const char *cred,
                                                EFdbusCredType type)
{
    int32_t security_level = FDB_SECURITY_LEVEL_NONE;
    tHostSecLevelTbl::const_iterator perm_it = mHostSecLevelTbl.find(host_name);
    if (perm_it != mHostSecLevelTbl.end())
    {
        if (type == FDB_PERM_CRED_MAC)
        {
            security_level = getMacSecurityLevel(cred, perm_it->second.mMacAddr);
        }
        else if (type == FDB_PERM_CRED_IP)
        {
            security_level = getIpSecurityLevel(cred, perm_it->second.mIpAddr);
        }
    }

    return security_level;
}

#else
#include "CHostSecurityConfig.h"
#include <security/CFdbusSecurityConfig.h>

void CHostSecurityConfig::importSecurity()
{
}

int32_t CHostSecurityConfig::getSecurityLevel(const char *host_name, const char *cred,
                                                EFdbusCredType type)
{
    return CFdbusSecurityConfig::getNrSecurityLevel() - 1;
}

#endif

