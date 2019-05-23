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
 
#ifndef __CSERVERSECURITYCONFIG_H__
#define __CSERVERSECURITYCONFIG_H__

#include <map>
#include <set>
#include <string>
#include <vector>
#include <common_base/common_defs.h>

class CServerSecurityConfig
{
public:
    void importSecurity();
    int32_t getSecurityLevel(const char *svc_name, uint32_t uid,
                                EFdbusCredType type = FDB_PERM_CRED_AUTO);
    
private:
    typedef std::vector<uint32_t> tGroupTbl;
    typedef std::map< uint32_t, int32_t> tCredSecLevelTbl; // gid/uid->sec level
    struct CServerSecCfg
    {
        int32_t mDefaultLevel;
        tCredSecLevelTbl mCredSecLevelTbl;
        CServerSecCfg()
            : mDefaultLevel(FDB_SECURITY_LEVEL_NONE)
        {}
    };
    struct CPermissions
    {
        CServerSecCfg mGid;
        CServerSecCfg mUid;
    };
    typedef std::map< std::string, CPermissions> tServerSecLevelTbl;
    tServerSecLevelTbl mServerSecLevelTbl;

    void parseServerSecurityConfig(const char *svc_name, const char *json_str, std::string &err_msg);
    void parseServerConfigFile(const char *path, const char *cfg_file_name, std::string &err_msg);
    void importServerSecurity(const char *config_path);
    void getOwnGroups(uint32_t uid, tGroupTbl &group_list);
    void addPermission(const void *json_handle, CServerSecCfg &cfg, int32_t level,
                       EFdbusCredType type, std::string &err_msg);
    int32_t getGroupSecurityLevel(uint32_t uid, const CServerSecCfg &sec_cfg);
    int32_t getUserSecurityLevel(uint32_t uid, const CServerSecCfg &sec_cfg);
};

#endif
