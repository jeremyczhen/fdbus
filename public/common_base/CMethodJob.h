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

#ifndef _CMETHODJOB_H_
#define _CMETHODJOB_H_

#include "CBaseJob.h"

/*
 * The job allows executing callback in the context of given object
 * rather than the context of the job itself.
 */
template <class C>
class CMethodJob : public CBaseJob
{
public:
    /*
     * This is the type you can use to define the subclass. For example:
     * class CMyJob : public CMethodJob<CMyClass>
     * {
     * public:
     *     CMyJob(CMyClass *class, CMethodJob<CMyClass>::M method, ...)
     * };
     *
     * @iparam worker: the worker(thread) where the job is running
     * @iparam job: the job the method is processing
     */
    typedef C classType;
    typedef void (C::*M)(CBaseWorker *worker, CMethodJob *job, Ptr &ref);

    /*
     * Create CMethodJob
     *
     * @iparam instance: instance of any class
     * @iparam method: method of the class
     * @return  None
     */
    CMethodJob(C *instance, M method, uint32_t flag = 0)
        : CBaseJob(flag)
        , mInstance(instance)
        , mMethod(method)
    {
    }
private:
    void run(CBaseWorker *worker, Ptr &ref)
    {
        if (mInstance && mMethod)
        {
            (mInstance->*mMethod)(worker, this, ref);
        }
    }
    C *mInstance;
    M mMethod;
};

#endif

