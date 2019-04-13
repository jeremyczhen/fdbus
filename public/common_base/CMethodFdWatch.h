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

#ifndef _CMETHODFDWATCH_H_
#define _CMETHODFDWATCH_H_

#include <cstdint>
#include "CBaseFdWatch.h"

/*
 * The watch allows executing callback in the context of given object
 * rather than the context of the job itself.
 */
template <class C>
class CMethodFdWatch : public CBaseFdWatch
{
public:
    /*
     * This is the type you can use to define the subclass. For example:
     * class CMyWatch : public CMethodFdWatch<CMyClass>
     * {
     * public:
     *     CMyWatch(CMyClass *class, CMethodFdWatch<CMyClass>::M input, ...)
     * };
     *
     * @iparam watch: the watch running the method
     * @return: none
     */
    typedef C classType;
    typedef void (C::*M_normal)(CMethodFdWatch *watch, bool &internal);
    typedef void (C::*M_exception)(CMethodFdWatch *watch);

    /*
     * create a method watch
     *
     * @iparam fd: file descriptor to be watched
     * @iparam instance: instance of object
     * @iparam input: called when POLLIN is set after poll()
     * @iparam output: called when POLLOUT is set after poll()
     * @iparam error: called when POLLERR is set after poll()
     * @iparam hup: called when POLLHUP is set after poll()
     * @return  None
     */
    CMethodFdWatch(int fd
                   , C *instance
                   , M_normal input = 0
                   , M_normal output = 0
                   , M_exception error = 0
                   , M_exception hup = 0)
        : CBaseFdWatch(fd, 0)
        , mInstance(instance)
        , mInput(input)
        , mOutput(output)
        , mError(error)
        , mHup(hup)
    {
        int32_t flgs = 0;
        if (input)
        {
            flgs |= POLLIN;
        }
        if (output)
        {
            flgs |= POLLOUT;
        }
        if (error)
        {
            flgs |= POLLERR;
        }
        if (hup)
        {
            flgs |= POLLHUP;
        }
        CSysFdWatch::flags(flgs);
    }
private:
    void onInput(bool &io_error)
    {
        if (mInstance && mInput)
        {
            (mInstance->*mInput)(this, io_error);
        }
    }
    void onOutput(bool &io_error)
    {
        if (mInstance && mOutput)
        {
            (mInstance->*mOutput)(this, io_error);
        }
    }
    void onError()
    {
        if (mInstance && mError)
        {
            (mInstance->*mError)(this);
        }
    }
    void onHup()
    {
        if (mInstance && mHup)
        {
            (mInstance->*mHup)(this);
        }
    }
    C *mInstance;
    M_normal mInput;
    M_normal mOutput;
    M_exception mError;
    M_exception mHup;
};

#endif
