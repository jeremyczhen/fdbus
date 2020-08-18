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

#ifndef _CSYSFDWATCH_H_
#define _CSYSFDWATCH_H_

#include <cstdint>

class CFdEventLoop;
class CSysFdWatch
{
public:
    /*
     * Create FD watch
     *
     * @iparam fd - the file descriptor to watch
     *     Warning!!! the fd should be non-block when reading from onInput().
     *     If you are not sure, non-block it manully as follows:
     *     fcntl(fd, F_SETFL, O_NONBLOCK)
     * @iparam flag - or-ed by
     *     POLLIN
     *     POLLOUT
     *     POLLHUP
     *     POLLERR
     *  @return - NA
     */
    CSysFdWatch(int fd, int32_t flags);

    virtual ~CSysFdWatch();

    /*
     * query enable status.
     */
    bool enable()
    {
        return mEnable;
    }

    /*
     * enable the watch. Internally used only!!!
     */
    void enable(bool enb);

    /*
     * set polling flag of the watch. Internally used only!!!
     */
    int32_t flags()
    {
        return mFlags;
    }

    /*
     * query flag.
     */
    void flags(int32_t flgs)
    {
        mFlags = flgs;
    }

    /*
     * Get file descriptor of the watch.
     */
    int descriptor()
    {
        return mFd;
    }

    /*
     * Set file descriptor of the watch.
     */
    void descriptor(int fd)
    {
        mFd = fd;
    }

    bool fatalError() const
    {
        return mFatalError;
    }

    void fatalError(bool enb);

protected:
    /*-----------------------------------------------------------------------------
     * The virtual function should be implemented by subclass
     *---------------------------------------------------------------------------*/
    /*
     * callback invoked when POLLIN is set after poll()
     * You MUST read from fd as follows:
     *     read(descriptor(), data, size)
     * Otherwise the poll() will not be blocked.
     * Also, the read() should be non-block!!!
     */
    virtual void onInput(bool &io_error) {}

    /*
     * callback invoked when POLLOUT is set after poll()
     * You MUST write to fd as follows:
     *     write(descriptor(), data, size)
     * Otherwise the poll() will not be blocked.
     * Also, the write() should be non-block!!!
     */
    virtual void onOutput(bool &io_error) {}

    /*
     * callback invoked when POLLHUP is set after poll()
     */
    virtual void onHup()
    {
        enable(false);
    }

    /*
     * callback invoked when POLLERROR is set after poll()
     */
    virtual void onError()
    {
        fatalError(true);
    }

    virtual int32_t convertRetEvents(int32_t revents)
    {
        return revents;
    }

private:
    void eventloop(CFdEventLoop *loop)
    {
        mEventLoop = loop;
    }

    CFdEventLoop *eventloop()
    {
        return mEventLoop;
    }

    int mFd;
    int32_t mFlags;
    bool mEnable;
    bool mFatalError;
    CFdEventLoop *mEventLoop;
    
    friend class CFdEventLoop;
    friend class CNotifyFdWatch;
};

#endif
