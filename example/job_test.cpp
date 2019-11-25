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

#define FDB_LOG_TAG "JOB_TEST"
#include <common_base/fdbus.h>

static CBaseWorker sender;
static CBaseWorker receiver;

/* create a job which print message at worker thread */
class CGreetingJob : public CBaseJob
{
public:
    CGreetingJob(const char *greeting_word)
    {
        mGreetingWord = greeting_word;
    }
protected:
    /* callback called at worker thread */
    void run(CBaseWorker *worker, Ptr &ref)
    {
        std::cout << mGreetingWord << std::endl;
        FDB_LOG_I("Job is received.\n");
    }
private:
    std::string mGreetingWord;
};

/* a timer sending job at 200ms interval cyclically */
class CSenderTimer : public CBaseLoopTimer
{
public:
    CSenderTimer() : CBaseLoopTimer(200, true)
    {}
protected:
    /* called when timeout */
    void run()
    {
        /* send job to receiver worker thread asynchronously
         * it just throws the job to receiver and will not block
         */
        receiver.sendAsync(new CGreetingJob("hello, world!"));
        FDB_LOG_I("Job is sent.\n");
    }
};

int main(int argc, char **argv)
{
#ifdef __WIN32__
    WORD wVersionRequested;
    WSADATA wsaData;
    int err;

/* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
    wVersionRequested = MAKEWORD(2, 2);

    err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0) {
        /* Tell the user that we could not find a usable */
        /* Winsock DLL.                                  */
        printf("WSAStartup failed with error: %d\n", err);
        return 1;
    }
#endif
    /* start fdbus context thread */
    FDB_CONTEXT->start();
    sender.start(); /* start sender worker thread */
    receiver.start(); /* start receiver worker thread */

    /* create sender timer and attach it with sender worker thread */
    CSenderTimer *sender_timer = new CSenderTimer();
    sender_timer->attach(&sender, true);

    /* convert main thread into worker */
    CBaseWorker background_worker;
    background_worker.start(FDB_WORKER_EXE_IN_PLACE);
}
