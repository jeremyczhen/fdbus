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
#include <stdlib.h>     /* srand, rand */
#include <common_base/CFdbToken.h>
#include <common_base/CBaseSysDep.h>
#include <security/CFdbusSecurityConfig.h>

void CFdbToken::allocateToken(tTokenList &tokens, bool encrypt)
{
    tokens.clear();
    for (int32_t i = 0; i < CFdbusSecurityConfig::getNrSecurityLevel(); ++i)
    {
        std::string rand_string;
        while (1)
        {
            randomString(rand_string);

            bool same_token_found = false;
            for (uint32_t j = 0; j < tokens.size(); ++j)
            {
                if (!rand_string.compare(tokens[j]))
                {
                    same_token_found = true;
                    break;
                }
            }
            if (!same_token_found)
            {
                break;
            }
            rand_string.clear();
        }
        tokens.push_back(rand_string);
    }
}

void CFdbToken::allocateToken(std::string &token, bool encrypt)
{
    randomString(token);
}

int32_t CFdbToken::checkSecurityLevel(tTokenList &tokens, const char *token)
{
    int32_t security_level = FDB_SECURITY_LEVEL_NONE;
    if (token && (token[0] != '\0'))
    {
        for (int32_t i = 0; i < (int32_t)tokens.size(); ++i)
        {
            if (!tokens[i].compare(token))
            {
                security_level = i;
                break;
            }
        }
    }
    return security_level;
}

void CFdbToken::encryptToken(const char *raw_token, std::string &crypt_token)
{

}

void CFdbToken::decryptToken(std::string &raw_token, const char *crypt_token)
{

}

void CFdbToken::encryptToken(const tTokenList &raw_tokens, tTokenList &crypt_tokens)
{

}

void CFdbToken::decryptToken(const tTokenList &raw_tokens, tTokenList &crypt_tokens)
{

}

void CFdbToken::randomString(std::string &sstring)
{
    static const char alphanum[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";

    srand((uint32_t)sysdep_getsystemtime_milli());
    for (int i = 0; i < CFdbusSecurityConfig::tokenLength(); ++i) {
        sstring += alphanum[rand() % (sizeof(alphanum) - 1)];
    }
}

#else
#include <common_base/CFdbToken.h>

void CFdbToken::allocateToken(tTokenList &tokens, bool encrypt)
{
}

void CFdbToken::allocateToken(std::string &token, bool encrypt)
{
}

int32_t CFdbToken::checkSecurityLevel(tTokenList &tokens, const char *token)
{
    return FDB_SECURITY_LEVEL_NONE;
}

void CFdbToken::encryptToken(const char *raw_token, std::string &crypt_token)
{

}

void CFdbToken::decryptToken(std::string &raw_token, const char *crypt_token)
{

}

void CFdbToken::encryptToken(const tTokenList &raw_tokens, tTokenList &crypt_tokens)
{

}

void CFdbToken::decryptToken(const tTokenList &raw_tokens, tTokenList &crypt_tokens)
{

}

#endif

