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
 
#ifndef __CAPISECURITYCONFIG_H__
#define __CAPISECURITYCONFIG_H__

#include <vector>
#include <string>
#include <common_base/common_defs.h>

class CApiSecurityConfig
{
public:
    void importSecLevel(const char *svc_name);
    int32_t getMessageSecLevel(FdbMsgCode_t msg_code);
    int32_t getEventSecLevel(FdbMsgCode_t msg_code);
private:
    struct CSecLevelRange
    {
        FdbMsgCode_t mBegin;
        FdbMsgCode_t mEnd;
        int32_t mSecLevel;
    };
    typedef std::vector< CSecLevelRange> tApiSecRangeTbl;
    struct CApiSecLevelTbl
    {
        int32_t mDefaultLevel;
        tApiSecRangeTbl mApiSecRangeTbl;
        CApiSecLevelTbl()
            : mDefaultLevel(FDB_SECURITY_LEVEL_NONE)
        {}
    };
    
    CApiSecLevelTbl mMessageSecLevelTbl;
    CApiSecLevelTbl mEventSecLevelTbl;

    int32_t getSecLevel(FdbMsgCode_t msg_code, CApiSecLevelTbl &tbl);
    void parseSecurityConfig(const char *json_str, std::string &err_msg);
    void parseApiConfig(const void *json_handle, CApiSecLevelTbl &cfg, std::string &err_msg);
};
#endif
