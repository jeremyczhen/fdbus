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
 
#ifndef __CHOSTSECURITYCONFIG_H__
#define __CHOSTSECURITYCONFIG_H__

#include <map>
#include <string>
#include <common_base/common_defs.h>

class CHostSecurityConfig
{
public:
    void importSecurity();
    int32_t getSecurityLevel(const char *host_name, const char *cred,
                             EFdbusCredType type = FDB_PERM_CRED_AUTO);

private:
    typedef std::map< std::string, int32_t> tCredSecLevelTbl; // host cred->sec level
    struct CHostSecCfg
    {
        int32_t mDefaultLevel;
        tCredSecLevelTbl mCredSecLevelTbl;
    };
    struct CPermission
    {
        CHostSecCfg mMacAddr;
        CHostSecCfg mIpAddr;
    };
    typedef std::map< std::string, CPermission> tHostSecLevelTbl;
    tHostSecLevelTbl mHostSecLevelTbl;

    void parseHostSecurityConfig(const char *json_str, std::string &err_msg);
    void addPermission(const void *json_handle, CHostSecCfg &cfg,
                        int32_t level, EFdbusCredType type, std::string &err_msg);
    int32_t getMacSecurityLevel(const char *mac, const CHostSecCfg &sec_cfg);
    int32_t getIpSecurityLevel(const char *ip, const CHostSecCfg &sec_cfg);
};

#endif
