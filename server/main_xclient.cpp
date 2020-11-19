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
#include <stdio.h>
#include <iostream>
#include <common_base/fdbus.h>
#include <mutex>
#include <list>

#define XCLT_TEST_SINGLE_DIRECTION 0
#define XCLT_TEST_BI_DIRECTION     1
#define XCLT_INIT_SKIP_COUNT       16

class CXTestJob : public CBaseJob
{
public:
    CXTestJob()
    {}
protected:
    void run(CBaseWorker* worker, Ptr& ref);
};

class CUDPSenderTimer : public CNanoTimer
{
public:
    CUDPSenderTimer()
        : mSn(0)
    {}
    CUDPSenderTimer(uint32_t sn)
        : mSn(sn)
    {}
    uint32_t mSn;
};

class CXClient;
static CXClient *fdb_xtest_client;
static CBaseWorker *fdb_worker_A;
static CBaseWorker *fdb_worker_B;
static CBaseWorker *fdb_statistic_worker;
static bool fdb_stop_job = false;

static uint32_t fdb_burst_size = 16;
static bool fdb_udp_test = false;
static uint32_t fdb_block_size = 1024;
static uint32_t fdb_delay = 0;
static bool fdb_sync_invoke = false;
static std::mutex fdb_ts_mutex;
static std::list<CUDPSenderTimer *> fdb_timestamps;
static int32_t fdb_init_skip_count = XCLT_INIT_SKIP_COUNT;

class CStatisticTimer : public CMethodLoopTimer<CXClient>
{
public:
    CStatisticTimer(CXClient *client);
};

class CXClient : public CBaseClient
{
public:
    CXClient(const char *name, CBaseWorker *worker = 0)
        : CBaseClient(name, worker)
        , mFailureCount(0)
    {
        /* create the request timer, attach to a worker thread, but do not start it */
        mTimer = new CStatisticTimer(this);
        mTimer->attach(fdb_statistic_worker, false);
        mBuffer = (uint8_t *)malloc(fdb_block_size);
        memset(mBuffer, 0, fdb_block_size);
        resetTotal();
        enableUDP(true);
        enableTimeStamp(true);
    }
    void doStatistic(CMethodLoopTimer<CXClient> *timer)
    {
        static bool title_printed = false;
        if (!title_printed)
        {
            printf("  |%12s| |%12s|    |%8s|   |%8s||%8s||%6s/%-6s||%10s||%10s|\n",
                   "Avg Data Rate", "Inst Data Rate", "Avg Trans", "Inst Trans", "Pending", "Total", "Failure", "Avg Delay", "Max Delay");
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
        uint64_t pending_req = 0;
        if (fdb_udp_test)
        {
            pending_req = (uint64_t)fdb_timestamps.size();
        }
        else if (mTotalRequest > mTotalReply)
        {
            pending_req = mTotalRequest - mTotalReply;
        }
        uint64_t avg_delay = mTotalReply ? mTotalDelay / mTotalReply : 0;
        
        printf("%12u B/s %12u B/s %8u Req/s %8u Req/s %8u %12u/%-4u %8u us %8u us\n",
                (uint32_t)avg_data_rate, (uint32_t)inst_data_rate, (uint32_t)avg_trans_rate,
                (uint32_t)inst_trans_rate, (uint32_t)pending_req, (uint32_t)mTotalRequest,
                (uint32_t)mFailureCount, (uint32_t)avg_delay, (uint32_t)mMaxDelay);
        resetInterval();
    }
    void sendData()
    {
        static uint32_t sn = 0;

        if (fdb_init_skip_count)
        {
            send(XCLT_TEST_SINGLE_DIRECTION, mBuffer, fdb_block_size, true);
            return;
        }

        auto timer = new CUDPSenderTimer(sn);
        timer->start();
        // shoud be pushed to list before sending!!!
        {
        std::lock_guard<std::mutex> _l(fdb_ts_mutex);
        fdb_timestamps.push_back(timer);
        }

        incrementSend(fdb_block_size);
        uint8_t *ptr = (uint8_t *)mBuffer;
        *(ptr++) = (uint8_t)((sn >> 0) & 0xff);
        *(ptr++) = (uint8_t)((sn >> 8) & 0xff);
        *(ptr++) = (uint8_t)((sn >> 16) & 0xff);
        *(ptr++) = (uint8_t)((sn >> 24) & 0xff);
        send(XCLT_TEST_SINGLE_DIRECTION, mBuffer, fdb_block_size, true);
        sn++;
    }
    void invokeMethod()
    {
        incrementSend(fdb_block_size);
        if (fdb_sync_invoke)
        {
            CBaseJob::Ptr ref(new CBaseMessage(XCLT_TEST_BI_DIRECTION));
            invoke(ref, mBuffer, fdb_block_size);
            handleReply(ref);
        }
        else
        {
            invoke(XCLT_TEST_BI_DIRECTION, mBuffer, fdb_block_size);
        }
    }
protected:
    /* called when connected to the server */
    void onOnline(FdbSessionId_t sid, bool is_first)
    {
        resetTotal();
        mTimer->enable();
        fdb_stop_job = false;
        fdb_worker_A->sendAsync(new CXTestJob());
        CFdbMsgSubscribeList sub_list;
        addNotifyItem(sub_list, XCLT_TEST_SINGLE_DIRECTION);
        subscribe(sub_list);
    }
    void onOffline(FdbSessionId_t sid, bool is_last)
    {
        mTimer->disable();
        fdb_stop_job = true;
    }
    void onReply(CBaseJob::Ptr &msg_ref)
    {
        handleReply(msg_ref);
    }
    void onBroadcast(CBaseJob::Ptr &msg_ref)
    {
        if (fdb_init_skip_count)
        {
            fdb_init_skip_count--;
            return;
        }
        auto msg = castToMessage<CFdbMessage *>(msg_ref);
        switch (msg->code())
        {
            case XCLT_TEST_SINGLE_DIRECTION:
            {
                uint8_t *ptr = (uint8_t *)(msg->getPayloadBuffer());
                uint32_t sn = 0;
                sn |= (uint32_t)(*(ptr + 0) << 0);
                sn |= (uint32_t)(*(ptr + 1) << 8);
                sn |= (uint32_t)(*(ptr + 2) << 16);
                sn |= (uint32_t)(*(ptr + 3) << 24);

                CUDPSenderTimer *timer = 0;
                {
                std::lock_guard<std::mutex> _l(fdb_ts_mutex);
                for (auto it = fdb_timestamps.begin(); it != fdb_timestamps.end();)
                {
                    if ((*it)->mSn < sn)
                    {
                        mFailureCount++;
                        ++it;
                        delete fdb_timestamps.front();
                        fdb_timestamps.pop_front();
                    }
                    else if ((*it)->mSn == sn)
                    {
                        timer = fdb_timestamps.front();
                        fdb_timestamps.pop_front();
                        break;
                    }
                    else
                    {
                        break;
                    }
                }
                }
                incrementReceive(msg->getPayloadSize());
                if (timer)
                {
                    getdownDelay(timer->snapshotMicroseconds());
                    delete timer;
                }
                else
                {
                }
            }
            break;
        }
    }
private:
    CNanoTimer mTotalNanoTimer;
    CNanoTimer mIntervalNanoTimer;
    uint64_t mTotalBytesSent;
    uint64_t mTotalBytesReceived;
    uint64_t mTotalRequest;
    uint64_t mTotalReply;
    
    uint64_t mIntervalBytesSent;
    uint64_t mIntervalBytesReceived;
    uint64_t mIntervalRequest;
    uint64_t mIntervalReply;

    uint64_t mMaxDelay;
    uint64_t mMinDelay;
    uint64_t mTotalDelay;
    uint64_t mIntervalDelay;

    uint8_t *mBuffer;
    uint64_t mFailureCount;

    CStatisticTimer *mTimer;

    void resetTotal()
    {
        mTotalNanoTimer.start();
        mTotalBytesSent = 0;
        mTotalBytesReceived = 0;
        mTotalRequest = 0;
        mTotalReply = 0;

        resetInterval();

        mMaxDelay = 0;
        mMinDelay = (uint64_t)~0;
        mTotalDelay = 0;

        fdb_init_skip_count = XCLT_INIT_SKIP_COUNT;
        std::lock_guard<std::mutex> _l(fdb_ts_mutex);
        fdb_timestamps.clear();
    }

    void resetInterval()
    {
        mIntervalNanoTimer.start();
        mIntervalBytesSent = 0;
        mIntervalBytesReceived = 0;
        mIntervalRequest = 0;
        mIntervalReply = 0;
        mIntervalDelay = 0;
    }

    void incrementSend(uint32_t size)
    {
        if (fdb_init_skip_count)
        {
            return;
        }
        mTotalBytesSent += size;
        mIntervalBytesSent += size;
        mTotalRequest++;
        mIntervalRequest++;
    }

    void incrementReceive(uint32_t size)
    {
        if (fdb_init_skip_count)
        {
            return;
        }
        mTotalBytesReceived += size;
        mIntervalBytesReceived += size;
        mTotalReply++;
        mIntervalReply++;
    }

    void getdownDelay(uint64_t delay)
    {
        if (fdb_init_skip_count)
        {
            return;
        }
        if (delay > mMaxDelay)
        {
            mMaxDelay = delay;
        }
        if (delay < mMinDelay)
        {
            mMinDelay = delay;
        }
        mTotalDelay += delay;
        mIntervalDelay += delay;
    }

    void handleReply(CBaseJob::Ptr &msg_ref)
    {
        if (fdb_init_skip_count)
        {
            fdb_init_skip_count--;
            return;
        }
        auto msg = castToMessage<CBaseMessage *>(msg_ref);
        const CFdbMsgMetadata *md = msg->metadata();
        switch (msg->code())
        {
            case XCLT_TEST_BI_DIRECTION:
            {
                if (msg->isStatus())
                {
                    /* Unable to get intended reply from server... Check what happen. */
                    if (msg->isError())
                    {
                        int32_t error_code;
                        std::string reason;
                        if (!msg->decodeStatus(error_code, reason))
                        {
                            std::cout << "onReply: fail to decode status!\n" << std::endl;
                            return;
                        }
                        std::cout << "onReply(): status is received: msg code: " << msg->code()
                                  << ", error_code: " << error_code
                                  << ", reason: " << reason
                                  << std::endl;
                        mFailureCount++;
                    }
                    return;
                }
                incrementReceive(msg->getPayloadSize());
                getdownDelay((md->mReceiveTime - md->mSendTime) / 1000);
            }
            break;
            default:
            break;
        }
    }
};

CStatisticTimer::CStatisticTimer(CXClient *client)
    : CMethodLoopTimer<CXClient>(1000, true, client, &CXClient::doStatistic)
{}

void CXTestJob::run(CBaseWorker *worker, Ptr &ref)
{
    if (fdb_stop_job)
    {
        return;
    }
    for (uint32_t i = 0; i < fdb_burst_size; ++i)
    {
        if (fdb_udp_test)
        {
            fdb_xtest_client->sendData();
        }
        else
        {
            fdb_xtest_client->invokeMethod();
        }
    }
    if (fdb_delay)
    {
        sysdep_usleep(fdb_delay);
    }

    CBaseWorker *peer_worker;
    if (fdb_worker_A->isSelf())
    {
        peer_worker = fdb_worker_B;
    }
    else
    {
        peer_worker = fdb_worker_A;
    }

    // trigger another thread to send immediately meanwhile
    peer_worker->sendAsync(new CXTestJob());
    // wait until message sent above is really consumed
    FDB_CONTEXT->flush();
}

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
    int32_t help = 0;
    uint32_t burst_size = 16;
    int32_t udp_test = 0;
    uint32_t block_size = 1024;
    uint32_t delay = 0;
    int32_t sync_invoke = 0;
    const struct fdb_option core_options[] = {
        { FDB_OPTION_INTEGER, "block_size", 'b', &block_size},
        { FDB_OPTION_INTEGER, "burst_size", 's', &burst_size},
        { FDB_OPTION_INTEGER, "delay", 'd', &delay},
        { FDB_OPTION_BOOLEAN, "udp_test", 'u', &udp_test},
        { FDB_OPTION_BOOLEAN, "sync", 'y', &sync_invoke},
        { FDB_OPTION_BOOLEAN, "help", 'h', &help}
    };
    fdb_parse_options(core_options, ARRAY_LENGTH(core_options), &argc, argv);

    fdb_burst_size = burst_size;
    fdb_udp_test = !!udp_test;
    fdb_block_size = block_size;
    fdb_delay = delay;
    fdb_sync_invoke = !!sync_invoke;

    std::cout << "block size: " << fdb_block_size
              << ", burst size: " << fdb_burst_size
              << ", UDP test: " << (fdb_udp_test ? "true" : "false")
              << ", delay: " << fdb_delay
              << ", sync: " << (fdb_sync_invoke ? "true" : "false")
              << std::endl;

    if (help)
    {
        std::cout << "FDBus - Fast Distributed Bus" << std::endl;
        std::cout << "    SDK version " << FDB_DEF_TO_STR(FDB_VERSION_MAJOR) "."
                                           FDB_DEF_TO_STR(FDB_VERSION_MINOR) "."
                                           FDB_DEF_TO_STR(FDB_VERSION_BUILD) << std::endl;
        std::cout << "    LIB version " << CFdbContext::getFdbLibVersion() << std::endl;
        std::cout << "Usage: fdbxclient[ -b block size][ -s burst size][-d delay][ -u]" << std::endl;
        std::cout << "    -b block size: specify size of date sent for each request" << std::endl;
        std::cout << "    -s burst size: specify how many requests are sent in batch for a burst" << std::endl;
        std::cout << "    -d delay: specify delay between two bursts in micro second" << std::endl;
        std::cout << "    -u: if set, UDP is tested; otherwise TCP/UDS will be tested" << std::endl;
        std::cout << "    -y: if set, TCP test with synchronous API; otherwise asynchronous API will be called" << std::endl;
        exit(0);
    }

    FDB_CONTEXT->enableLogger(false);
    /* start fdbus context thread */
    FDB_CONTEXT->start();

    fdb_worker_A = new CBaseWorker();
    fdb_worker_B = new CBaseWorker();
    fdb_statistic_worker = new CBaseWorker();
    fdb_worker_A->start();
    fdb_worker_B->start();
    fdb_statistic_worker->start();
    fdb_xtest_client = new CXClient(FDB_XTEST_NAME);

    fdb_xtest_client->connect();

    /* convert main thread into worker */
    CBaseWorker background_worker;
    background_worker.start(FDB_WORKER_EXE_IN_PLACE);
}

