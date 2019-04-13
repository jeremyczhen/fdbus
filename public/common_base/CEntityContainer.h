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

#ifndef _CENTITYCONTAINER_H_
#define _CENTITYCONTAINER_H_

#include <map>
#define FDB_FIRST_ENTRY_ID 0

template<typename IDX, typename EP>
class CEntityContainer
{
public:
    typedef std::map<IDX, EP> EntryContainer_t;
    CEntityContainer()
        : mEntryAllocator(FDB_FIRST_ENTRY_ID)
        , mValidIt(false)
    {}
    virtual ~CEntityContainer()
    {}
    IDX allocateEntityId()
    {
        return mEntryAllocator++;
    }
    IDX allocateUniqueEntityId()
    {
        return mUniqueEntryAllocator++;
    }

    int32_t insertEntry(IDX index, EP &ep)
    {
        typename EntryContainer_t::iterator it = mEntryContainer.find(index);
        if (it == mEntryContainer.end())
        {
            mEntryContainer[index] = ep;
            mValidIt = false;
            return 0;
        }
        return -1;
    }

    typename EntryContainer_t::iterator retrieveEntry(IDX index, EP &entry)
    {
        typename EntryContainer_t::iterator it = mEntryContainer.find(index);
        if (it != mEntryContainer.end())
        {
            entry = it->second;
        }
        return it;
    }

    EP &retrieveEntry(IDX index, typename EntryContainer_t::iterator &it, bool &found)
    {
        static EP dummy;
        it = mEntryContainer.find(index);
        if (it == mEntryContainer.end())
        {
            found = false;
            return dummy;
        }
        else
        {
            found = true;
            return it->second;
        }
    }

    void deleteEntry(IDX index)
    {
        typename EntryContainer_t::iterator it = mEntryContainer.find(index);
        if (it != mEntryContainer.end())
        {
            mEntryContainer.erase(it);
            mValidIt = false;
        }
    }
    void deleteEntry(typename EntryContainer_t::iterator &it)
    {
        if (it != mEntryContainer.end())
        {
            mEntryContainer.erase(it);
            mValidIt = false;
        }
    }
    EntryContainer_t &getContainer()
    {
        return mEntryContainer;
    }
private:
    IDX mEntryAllocator;
    static IDX mUniqueEntryAllocator;
    EntryContainer_t mEntryContainer;
    bool mValidIt;
};

#endif
