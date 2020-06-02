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
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <common_base/cJSON/cJSON.h>
#include <utils/Log.h>
#include "CFdbusSecurityConfig.h"

int32_t CFdbusSecurityConfig::mTokenLength = FDB_CFG_TOKEN_LENGTH;
int32_t CFdbusSecurityConfig::mNrSecurityLevel = FDB_CFG_NR_SECURE_LEVEL;

void *CFdbusSecurityConfig::readFile(const char *file_name)
{
    struct stat statbuf;
    if (stat(file_name, &statbuf) < 0)
    {
        return 0;
    }
    FILE *fp = fopen(file_name, "rb");
    if (fp != NULL)
    {
        void *buffer = malloc(statbuf.st_size);
        uint32_t size = fread(buffer, 1, statbuf.st_size, fp);
        if (size < (uint32_t)statbuf.st_size)
        {
            free(buffer);
            fclose(fp);
            return 0;
        }
        fclose(fp);
        return buffer;
    }
    return 0;
}

void CFdbusSecurityConfig::parseSecurityConfig(const char *json_str,
                                               std::string &err_msg)
{
    auto cfg_root = cJSON_Parse(json_str);
    if (cfg_root != NULL)
    {
        if (cJSON_IsObject(cfg_root))
        {
            auto nr_sec_level = cJSON_GetObjectItem(cfg_root, "number_of_secure_levels");
            if (nr_sec_level != NULL)
            {
                if (cJSON_IsNumber(nr_sec_level))
                {
                    int32_t nr_sec_level_val = nr_sec_level->valueint;
                    if ((nr_sec_level_val <= 0) || (nr_sec_level_val >= 32))
                    {
                        nr_sec_level_val = FDB_CFG_NR_SECURE_LEVEL;
                    }
                    mNrSecurityLevel = nr_sec_level_val;
                }
                else
                {
                    err_msg = "not number - 1";
                }
            }

            auto token_length = cJSON_GetObjectItem(cfg_root, "token_length");
            if (token_length != NULL)
            {
                if (cJSON_IsNumber(token_length))
                {
                    int32_t token_length_val = token_length->valueint;
                    if ((token_length_val < 8) || (token_length_val > 512))
                    {
                        token_length_val = FDB_CFG_TOKEN_LENGTH;
                    }
                    mTokenLength = token_length_val;
                }
                else
                {
                    err_msg = "not number - 2";
                }
            }
        }
    }

    if (cfg_root)
    {
        cJSON_Delete(cfg_root);
    }
}

void CFdbusSecurityConfig::importSecurity()
{
    const char *file_name = FDB_CFG_CONFIG_PATH
                            "/fdbus"
                            FDB_CFG_CONFIG_FILE_SUFFIX;
    void *buffer = readFile(file_name);
    if (buffer != NULL)
    {
        std::string err_msg;
        parseSecurityConfig((char *)buffer, err_msg);
        if (!err_msg.empty())
        {
            LOG_E("CServerSecurityConfig: error when parsing %s: %s\n",
                    file_name, err_msg.c_str());
        }
        free(buffer);
    }
}

#else
#include "CFdbusSecurityConfig.h"
int32_t CFdbusSecurityConfig::mTokenLength = FDB_CFG_TOKEN_LENGTH;
int32_t CFdbusSecurityConfig::mNrSecurityLevel = FDB_CFG_NR_SECURE_LEVEL;

void *CFdbusSecurityConfig::readFile(const char *file_name)
{
    return 0;
}

void CFdbusSecurityConfig::importSecurity()
{

}

#endif

