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

#ifndef _CMETHODNOTIFICATION_
#define _CMETHODNOTIFICATION_

#include "CBaseNotification.h"

template <class T, class C>
class CMethodNotification : public CBaseNotification<T>
{
public:
    /*
     * This is the type you can use to define the subclass. For example:
     * class CMyNotificaton : public CMethodNotification<CMyNotificaton>
     * {
     * public:
     *     CMyNotificaton(CMyClass *class, CMethodNotification<CMyClass>::M method, ...)
     * };
     *
     * @iparam notification: the notification being processing
     * @iparam data: parameter of extra data
     */
    typedef T dataType;
    typedef C classType;
    typedef void (C::*M)(CMethodNotification *notification, T &data);

    /*
     * Create CMethodNotification
     *
     * @iparam instance: instance of any class
     * @iparam method: method of the class to be called once event happens
     * @iparam event: refer to CBaseNotification
     */
    CMethodNotification(C *instance, M method, FdbEventCode_t event, CBaseWorker *worker = 0)
        : CBaseNotification<T>(event, worker)
        , mInstance(instance)
        , mMethod(method)
    {
    }
    CMethodNotification(C *instance, M method, CBaseWorker *worker = 0)
        : CBaseNotification<T>(worker)
        , mInstance(instance)
        , mMethod(method)
    {
    }
private:
    void run(T &data)
    {
        if (mInstance && mMethod)
        {
            (mInstance->*mMethod)(this, data);
        }
    }
    C *mInstance;
    M mMethod;
};

#if 0
template <class C>
class CEventMethodNotification : public CMethodNotification<void *, C>
{
public:
    CEventMethodNotification(C *instance, M method, FdbEventCode_t event = FDB_INVALID_ID)
        : CMethodNotification<void *, C>(instance, method, event)
    {}
};
#endif
template <class C>
using CGenericMethodNotification = CMethodNotification<void *, C>;

#endif
