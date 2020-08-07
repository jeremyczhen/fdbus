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
#include "CServerSecurityConfig.h"
#include <sys/types.h>
#ifdef __WIN32__
/* refer to https://github.com/tronkko/dirent */
#include <platform/win/dirent.h>
#else
#include <dirent.h>
#include <grp.h>
#include <pwd.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <common_base/cJSON/cJSON.h>
#include <utils/Log.h>
#include <common_base/CFdbToken.h>
#include <security/CFdbusSecurityConfig.h>

#ifdef __WIN32__
struct group
{
    uint32_t gr_gid;
};

const static struct group fake_group = {0};

const struct group *getgrnam(const char *grp_name)
{
    return &fake_group;
}

typedef uint32_t gid_t;
struct passwd
{
    const char *pw_name;
    uint32_t pw_gid;
    uint32_t pw_uid;
};

const static struct passwd fake_passwd = {"unknown", 0, 0};

const struct passwd *getpwuid(uint32_t uid)
{
    return &fake_passwd;
}

const struct passwd *getpwnam(const char *name)
{
    return &fake_passwd;
}

int32_t getgrouplist(const char *user, gid_t group,
                        gid_t *groups, int *ngroups)
{
    *ngroups = 0;
    return -1;
}

#endif

#define FDB_MAX_GROUP 128

void CServerSecurityConfig::addPermission(const void *json_handle, CServerSecCfg &cfg,
                              int32_t level, EFdbusCredType type, std::string &err_msg)
{
    const auto item_cred = (const cJSON *)json_handle;
    if (cJSON_IsArray(item_cred))
    {
        for (int j = 0; j < cJSON_GetArraySize(item_cred); ++j)
        {
            auto item_id = cJSON_GetArrayItem(item_cred, j);
            if (item_id)
            {
                uint32_t id;
                bool err = false;
                if (cJSON_IsString(item_id))
                {
                    if (type == FDB_PERM_CRED_GID)
                    {
                        const struct group *grp_info;
                        grp_info = getgrnam(item_id->valuestring);
                        if (grp_info)
                        {
                            id = (uint32_t)grp_info->gr_gid;
                        }
                        else
                        {
                            err = true;
                        }
                    }
                    else if (type == FDB_PERM_CRED_UID)
                    {
                        const struct passwd *pw = getpwnam(item_id->valuestring);
                        if (pw)
                        {
                            id = pw->pw_uid;
                        }
                        else
                        {
                            err = true;
                        }
                    }
                    else
                    {
                        return;
                    }
                    if (err)
                    {
                        if (type == FDB_PERM_CRED_GID)
                        {
                            err_msg += "group ";
                        }
                        else if (type == FDB_PERM_CRED_UID)
                        {
                            err_msg += "user ";
                        }
                        err_msg += item_id->valuestring;
                        err_msg += " is not found.";
                        return;
                    }
                }
                else if (cJSON_IsNumber(item_id))
                {
                    id = item_id->valueint;
                }
                else
                {
                    err_msg = "id should be number or string.";
                    return;
                }
                cfg.mCredSecLevelTbl[id] = level;
            }
        }
    }
    else if (cJSON_IsNull(item_cred))
    {
        cfg.mDefaultLevel = level;
    }
    else
    {
        err_msg = "'gid' or 'uid' should be array or null.";
        return;
    }
}

void CServerSecurityConfig::parseServerSecurityConfig(const char *svc_name,
                                const char *json_str, std::string &err_msg)
{
    auto cfg_root = cJSON_Parse(json_str);
    if (cfg_root != NULL)
    {
        if (cJSON_IsObject(cfg_root))
        {
            auto item_perm = cJSON_GetObjectItem(cfg_root, "permission");
            if (item_perm != NULL)
            {
                if (cJSON_IsArray(item_perm))
                {
                    CPermissions &perm = mServerSecLevelTbl[svc_name];
                    for (int i = 0; i < cJSON_GetArraySize(item_perm); ++i)
                    {
                        auto sec_level = cJSON_GetArrayItem(item_perm, i);
                        if (sec_level)
                        {
                            if (cJSON_IsObject(sec_level))
                            {
                                auto item_level = cJSON_GetObjectItem(sec_level, "level");
                                if (item_level)
                                {
                                    if (cJSON_IsNumber(item_level))
                                    {
                                        int32_t level = (int32_t)item_level->valueint;
                                        if (level < 0)
                                        {
                                            level = FDB_SECURITY_LEVEL_NONE;
                                        }
                                        if (level > CFdbusSecurityConfig::getNrSecurityLevel())
                                        {
                                            level = CFdbusSecurityConfig::getNrSecurityLevel();
                                        }
                                        auto item_gid = cJSON_GetObjectItem(sec_level, "gid");
                                        if (item_gid)
                                        {
                                            addPermission(item_gid, perm.mGid, level, 
                                                            FDB_PERM_CRED_GID, err_msg);
                                        }
                                        auto item_uid = cJSON_GetObjectItem(sec_level, "uid");
                                        if (item_uid)
                                        {
                                            addPermission(item_uid, perm.mUid, level,
                                                            FDB_PERM_CRED_UID, err_msg);
                                        }
                                    }
                                    else
                                    {
                                        err_msg = "'level' should be number.";
                                        goto _quit;
                                    }
                                }
                                else
                                {
                                    err_msg = "'level' is not found.";
                                    goto _quit;
                                }
                            }
                            else
                            {
                                err_msg = "not object - 2.";
                                goto _quit;
                            }
                        }
                    }
                }
                else
                {
                    err_msg = "not array - 1.";
                    goto _quit;
                }
            }
        }
        else
        {
            err_msg = "not object - 1.";
            goto _quit;
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

void CServerSecurityConfig::parseServerConfigFile(const char *path,
                            const char *cfg_file_name, std::string &err_msg)
{
    int32_t name_len = (int32_t)strlen(cfg_file_name);
    int32_t suffix_len = (int32_t)strlen(FDB_CFG_CONFIG_FILE_SUFFIX);
    if (name_len <= suffix_len)
    {
        return;
    }
    if (strcmp(cfg_file_name + name_len - suffix_len, FDB_CFG_CONFIG_FILE_SUFFIX))
    {
        return;
    }
    std::string full_path = path;
    full_path += "/";
    full_path += cfg_file_name;

    void *buffer = CFdbusSecurityConfig::readFile(full_path.c_str());
    if (buffer != NULL)
    {
        char *svc_name = strdup(cfg_file_name);
        svc_name[name_len - suffix_len] = '\0';
        parseServerSecurityConfig(svc_name, (const char *)buffer, err_msg);
#if 0
        if (!err_msg.empty())
        {
            tServerSecLevelTbl::iterator it = mServerSecLevelTbl.find(svc_name);
            if (it != mServerSecLevelTbl.end())
            {
                mServerSecLevelTbl.erase(it);
            }
        }
#endif
    
        free(buffer);
        free(svc_name);
    }
    else
    {
        err_msg = "unable to open ";
        err_msg += full_path;
    }
}

void CServerSecurityConfig::importServerSecurity(const char *config_path)
{
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir(config_path)) != NULL)
    {
        while ((ent = readdir(dir)) != NULL)
        {
            if (ent->d_type != DT_REG)
            {
                continue;
            }
            std::string err_msg;
            parseServerConfigFile(config_path, ent->d_name, err_msg);
            if (!err_msg.empty())
            {
                LOG_E("CServerSecurityConfig: error when parsing %s/%s: %s\n",
                        config_path, ent->d_name, err_msg.c_str());
            }
        }
        closedir(dir);
    }
}

void CServerSecurityConfig::importSecurity()
{
    if (!mServerSecLevelTbl.empty())
    {
        return;
    }
    CFdbusSecurityConfig::importSecurity();

    importServerSecurity(FDB_CFG_CONFIG_PATH "/server");
}

void CServerSecurityConfig::getOwnGroups(uint32_t uid, tGroupTbl &group_list)
{
    const struct passwd *pw;

    pw = getpwuid(uid);
    if (pw == NULL)
    {
        return;
    }

    gid_t *groups = (gid_t *)malloc(FDB_MAX_GROUP * sizeof (gid_t));
    int32_t ngroups = FDB_MAX_GROUP;
    if (getgrouplist(pw->pw_name, pw->pw_gid, groups, &ngroups) == -1)
    {
        free(groups);
        return;
    }
    for (int32_t i = 0; i < ngroups; ++i)
    {
        group_list.push_back(groups[i]);
    }

    free(groups);
}

int32_t CServerSecurityConfig::getGroupSecurityLevel(uint32_t uid, const CServerSecCfg &sec_cfg)
{
    tGroupTbl grp_list;
    getOwnGroups(uid, grp_list);
    if (grp_list.empty())
    {
        return sec_cfg.mDefaultLevel;
    }
    const tCredSecLevelTbl &level_tbl = sec_cfg.mCredSecLevelTbl;
    int32_t max_security_level = FDB_SECURITY_LEVEL_NONE;
    bool found = false;
    if (!level_tbl.empty())
    {
        for (tGroupTbl::const_iterator it = grp_list.begin();
                it != grp_list.end(); ++it)
        {
            tCredSecLevelTbl::const_iterator level_it = level_tbl.find(*it);
            if (level_it != level_tbl.end())
            {
                found = true;
                if (level_it->second > max_security_level)
                {
                    max_security_level = level_it->second;
                }
            }
        }
    }
    return found ? max_security_level : sec_cfg.mDefaultLevel;
}

int32_t CServerSecurityConfig::getUserSecurityLevel(uint32_t uid, const CServerSecCfg &sec_cfg)
{
    const tCredSecLevelTbl &level_tbl = sec_cfg.mCredSecLevelTbl;
    tCredSecLevelTbl::const_iterator level_it = level_tbl.find(uid);
    return level_it == level_tbl.end() ? sec_cfg.mDefaultLevel : level_it->second;
}

int32_t CServerSecurityConfig::getSecurityLevel(const char *svc_name, uint32_t uid,
                                                EFdbusCredType type)
{
    int32_t security_level = FDB_SECURITY_LEVEL_NONE;

    tServerSecLevelTbl::const_iterator perm_it = mServerSecLevelTbl.find(svc_name);
    if (perm_it != mServerSecLevelTbl.end())
    {
        int32_t level_gid = getGroupSecurityLevel(uid, perm_it->second.mGid);
        int32_t level_uid = getUserSecurityLevel(uid, perm_it->second.mUid);
        security_level = (level_gid > level_uid) ? level_gid : level_uid;
    }

    return security_level;
}

#else
#include "CServerSecurityConfig.h"
#include <security/CFdbusSecurityConfig.h>

void CServerSecurityConfig::importSecurity()
{
}

int32_t CServerSecurityConfig::getSecurityLevel(const char *svc_name, uint32_t uid,
                                                EFdbusCredType type)
{
    return CFdbusSecurityConfig::getNrSecurityLevel() - 1;
}
#endif
