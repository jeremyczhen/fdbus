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

#ifndef __CFDINTERFACETOKEN_H__
#define __CFDINTERFACETOKEN_H__

#include <common_base/CFdbSimpleSerializer.h>

namespace NFdbBase {
enum FdbCryptoAlgorithm
{
    CRYPTO_NONE = 1,
    CRYPTO_AES128 = 2,
    CRYPTO_AES192 = 3,
    CRYPTO_AES256 = 4,
    CRYPTO_RSA1024 = 5,
    CRYPTO_RSA2048 = 6,
    CRYPTO_RSA3072 = 7,
    CRYPTO_ECC160 = 8,
    CRYPTO_ECC224 = 9,
    CRYPTO_ECC256 = 10,
};

class FdbMsgTokens : public IFdbParcelable
{
public:
    void add_tokens(const std::string &token)
    {
        mTokens.Add(token);
    }
    void add_tokens(const char *token)
    {
        mTokens.Add(token);
    }
    void clear_tokens()
    {
        mTokens.clear();
    }
    CFdbParcelableArray<std::string> &tokens()
    {
        return mTokens;
    }
    void set_crypto_algorithm(FdbCryptoAlgorithm algorithm)
    {
        mCryptoAlgorithm = algorithm;
    }
    FdbCryptoAlgorithm crypto_algorithm()
    {
        return mCryptoAlgorithm;
    }
    void serialize(CFdbSimpleSerializer &serializer) const
    {
        serializer << mTokens
                   << (uint8_t)mCryptoAlgorithm;
    }
    void deserialize(CFdbSimpleDeserializer &deserializer)
    {
        uint8_t algorithm;
        deserializer >> mTokens
                     >> algorithm;
        mCryptoAlgorithm = (FdbCryptoAlgorithm)algorithm;
    }
    
private:
    CFdbParcelableArray<std::string> mTokens;
    FdbCryptoAlgorithm mCryptoAlgorithm;
};
}

#endif

