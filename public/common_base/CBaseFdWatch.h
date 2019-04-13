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

#ifndef _CBASEFDWATCH_
#define _CBASEFDWATCH_

#include "CSysFdWatch.h"

class CBaseWorker;
class CBaseFdWatch : public CSysFdWatch
{
public:

    /*
     * constructor
     *
     * @iparam fd: the file descriptor to watch
     * @flags: or-ed by:
     *     POLLIN
     *     POLLOUT
     *     POLLHUP
     *     POLLERR
     * @return None
     */
    CBaseFdWatch(int fd, int32_t flags);

    ~CBaseFdWatch();

    /*
     * Enable the watch: the watch is polling the file with the context of
     * worker thread
     *
     * @return true - success
     * @note attach() should be called before calling this function
     */
    bool enable();

    /*
     * Disable the watch: the watch is still on the worker but doesn't work
     *
     * @return true - success
     * @note attach() should be called before calling this function
     */
    bool disable();

    /*
     * Change polling flag of file descriptor
     *
     * @iparam flgs - new flag; see above
     * @return true - success
     * @note attach() should be called before calling this function
     */
    bool flags(int32_t flgs);

    /*
     * Run the watch at specified worker. Once an watch is attached with
     * a worker, the fd watched is polled at thread of the worker
     *
     * @iparam worker - the worker where the watch is running.
     * @iparam enb - initially enable/disable the timer
     * @return true - success
     * @note attach() should be called before calling this function
     */
    bool attach(CBaseWorker *worker, bool enb = true);

    /*
     * Obtain worker the watch is attached
     *
     * @return the worker
     */
    CBaseWorker *worker()
    {
        return mWorker;
    }
protected:
    CBaseWorker *mWorker;

    friend class CBaseWorker;
};

#endif
