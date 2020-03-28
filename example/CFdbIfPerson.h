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

#ifndef __CFDBIFPERSION_H__ 
#define __CFDBIFPERSION_H__

#include <common_base/fdbus.h>
class CCar : public IFdbParcelable
{
public:
    void serialize(CFdbSimpleSerializer &serializer) const
    {
        serializer << mBrand << mModel << mPrice;
    }
    void deserialize(CFdbSimpleDeserializer &deserializer)
    {
        deserializer >> mBrand >> mModel >> mPrice;
    }

    std::string mBrand;
    std::string mModel;
    int32_t mPrice;

protected:
    void toString(std::ostringstream &stream) const
    {
        stream << "mBrand:" << mBrand
               << ", mModel:" << mModel
               << ", mPrice:" << mPrice;
    }
};

class CPerson : public IFdbParcelable
{
public:
    void serialize(CFdbSimpleSerializer &serializer) const
    {
        serializer << mName << mAge << mCars << mAddress << mSalary << mPrivateInfo;
    }
    void deserialize(CFdbSimpleDeserializer &deserializer)
    {
        deserializer >> mName >> mAge >> mCars >> mAddress >> mSalary >> mPrivateInfo;
    }
    std::string mName;
    uint8_t mAge;
    int32_t mSalary;
    std::string mAddress;
    CFdbParcelableArray<CCar> mCars;
    CFdbParcelableArray<CFdbByteArray<20>> mPrivateInfo;

protected:
    void toString(std::ostringstream &stream) const
    {
        stream << "mName:" << mName
               << ", mAge:" << (unsigned)mAge
               << ", mSalary:" << mSalary
               << ", mCars:"; mCars.format(stream)
               << ", mAddress:" << mAddress
               << ", mPrivateInfo: "; mPrivateInfo.format(stream);
    }
};

#endif
