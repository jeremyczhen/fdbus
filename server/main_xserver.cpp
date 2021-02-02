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

#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <common_base/fdbus.h>

#define XCLT_TEST_SINGLE_DIRECTION 0
#define XCLT_TEST_BI_DIRECTION     1

class CXServer;
static CBaseWorker *fdb_statistic_worker;

class CStatisticTimer : public CMethodLoopTimer<CXServer>
{
public:
    CStatisticTimer(CXServer *server);
};

class CXServer : public CBaseServer
{
public:
    CXServer(const char *name, CBaseWorker *worker = 0)
        : CBaseServer(name, worker)
    {
        mTimer = new CStatisticTimer(this);
        mTimer->attach(fdb_statistic_worker, false);
        enableUDP(true);
        enableAysncRead(true);
        enableAysncWrite(true);
    }
    void doStatistic(CMethodLoopTimer<CXServer> *timer)
    {
        static bool title_printed = false;
        if (!title_printed)
        {
            printf("%12s       %12s     %8s     %8s %8s\n",
                   "Avg Data Rate", "Inst Data Rate", "Avg Trans", "Inst Trans", "Pending Rep");
            title_printed = true;
        }
        uint64_t interval_s = mIntervalNanoTimer.snapshotSeconds();
        uint64_t total_s = mTotalNanoTimer.snapshotSeconds();
        if (interval_s <= 0) interval_s = 1;
        if (total_s <= 0) total_s = 1;

        uint64_t avg_data_rate = mTotalBytesReceived / total_s;
        uint64_t inst_data_rate = mIntervalBytesReceived / interval_s;
        uint64_t avg_trans_rate = mTotalRequest / total_s;
        uint64_t inst_trans_rate = mIntervalRequest / interval_s;

        printf("%12u B/s %12u B/s %8u Req/s %8u Req/s %8u\n",
                (uint32_t)avg_data_rate, (uint32_t)inst_data_rate,
                (uint32_t)avg_trans_rate, (uint32_t)inst_trans_rate,
                FDB_CONTEXT->jobQueueSize());
        
        resetInterval();
    }
protected:
    void onOnline(FdbSessionId_t sid, bool is_first)
    {
        if (is_first)
        {
            resetTotal();
            mTimer->enable();
        }
    }
    void onOffline(FdbSessionId_t sid, bool is_last)
    {
        if (is_last)
        {
            mTimer->disable();
        }
    }
    void onInvoke(CBaseJob::Ptr &msg_ref)
    {
        auto msg = castToMessage<CBaseMessage *>(msg_ref);
        switch (msg->code())
        {
            case XCLT_TEST_BI_DIRECTION:
            {
                incrementReceived(msg->getPayloadSize());
                auto buffer = msg->getPayloadBuffer();
                auto size = msg->getPayloadSize();
                auto to_be_release = msg->ownBuffer();
                msg->reply(msg_ref, buffer, size);
                msg->releaseBuffer(to_be_release);
            }
            break;
            case XCLT_TEST_SINGLE_DIRECTION:
                incrementReceived(msg->getPayloadSize());
                broadcast(XCLT_TEST_SINGLE_DIRECTION, msg->getPayloadBuffer(), msg->getPayloadSize(),
                          "", FDB_QOS_BEST_EFFORTS);
            break;
        }
    }
private:
    CNanoTimer mTotalNanoTimer;
    CNanoTimer mIntervalNanoTimer;
    uint64_t mTotalBytesReceived;
    uint64_t mTotalRequest;
    uint64_t mIntervalBytesReceived;
    uint64_t mIntervalRequest;
    CStatisticTimer *mTimer;

    void resetTotal()
    {
        mTotalNanoTimer.start();
        mTotalBytesReceived = 0;
        mTotalRequest = 0;

        resetInterval();
    }

    void resetInterval()
    {
        mIntervalNanoTimer.start();
        mIntervalBytesReceived = 0;
        mIntervalRequest = 0;
    }

    void incrementReceived(uint32_t size)
    {
        mTotalBytesReceived += size;
        mIntervalBytesReceived += size;
        mTotalRequest++;
        mIntervalRequest++;
    }
};
CStatisticTimer::CStatisticTimer(CXServer *server)
    : CMethodLoopTimer<CXServer>(1000, true, server, &CXServer::doStatistic)
{}

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
    FDB_CONTEXT->enableLogger(false);
    /* start fdbus context thread */
    FDB_CONTEXT->start();

    fdb_statistic_worker = new CBaseWorker();
    fdb_statistic_worker->start();

    auto server = new CXServer(FDB_XTEST_NAME);
    server->bind();

    /* convert main thread into worker */
    CBaseWorker background_worker;
    background_worker.start(FDB_WORKER_EXE_IN_PLACE);
}

