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
 
#ifndef __CFDBTOKEN_H__
#define __CFDBTOKEN_H__

#include <vector>
#include <string>
#include "common_defs.h"

class CFdbToken
{
public:
    typedef std::vector<std::string> tTokenList;
    static void allocateToken(tTokenList &tokens, bool encrypt = false);
    static void allocateToken(std::string &token, bool encrypt = false);
    static int32_t checkSecurityLevel(tTokenList &tokens, const char *token);
    static void encryptToken(const char *raw_token, std::string &crypt_token);
    static void decryptToken(std::string &raw_token, const char *crypt_token);
    static void encryptToken(const tTokenList &raw_tokens, tTokenList &crypt_tokens);
    static void decryptToken(const tTokenList &raw_tokens, tTokenList &crypt_tokens);
private:
    static void randomString(std::string &random);
};

#endif
