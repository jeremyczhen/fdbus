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
 
#ifndef __CFDBUSSECURITYCONFIG_H__
#define __CFDBUSSECURITYCONFIG_H__

#include <string>
#include <common_base/common_defs.h>

class CFdbusSecurityConfig
{
public:
    static int32_t tokenLength()
    {
        return mTokenLength;
    }
    static int32_t getNrSecurityLevel()
    {
        return mNrSecurityLevel;
    }

    static void *readFile(const char *file_name);
    static void importSecurity();
private:
    static int32_t mTokenLength;
    static int32_t mNrSecurityLevel;
    static void parseSecurityConfig(const char *json_str, std::string &err_msg);
};

#endif

