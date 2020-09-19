#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <common_base/fdbus.h>

#define XCLT_TEST_SINGLE_DIRECTION 0
#define XCLT_TEST_BI_DIRECTION     1

class CXTestJob : public CBaseJob
{
public:
    CXTestJob()
    {}
protected:
    void run(CBaseWorker* worker, Ptr& ref);
};

class CXClient;
static CXClient *fdb_xtest_client;
static CBaseWorker *fdb_worker_A;
static CBaseWorker *fdb_worker_B;
static CBaseWorker* fdb_statistic_worker;
static bool fdb_stop_job = false;

static uint32_t fdb_burst_size = 16;
static bool fdb_bi_direction = true;
static uint32_t fdb_block_size = 1024;
static uint32_t fdb_delay = 0;
static bool fdb_sync_invoke = false;

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
    }
    void doStatistic(CMethodLoopTimer<CXClient> *timer)
    {
        static bool title_printed = false;
        if (!title_printed)
        {
            printf("%12s       %12s     %8s     %8s %8s %6s %10s  %10s\n",
                   "Avg Data Rate", "Inst Data Rate", "Avg Trans", "Inst Trans", "Pending Req", "Failure", "Avg Delay", "Max Delay");
            title_printed = true;
        }
        uint64_t interval_s = mIntervalNanoTimer.snapshotSeconds();
        uint64_t total_s = mTotalNanoTimer.snapshotSeconds();
        if (interval_s <= 0) interval_s = 1;
        if (total_s <= 0) total_s = 1;

        uint64_t avg_data_rate = mTotalBytesSent / total_s;
        uint64_t inst_data_rate = mIntervalBytesSent / interval_s;
        uint64_t avg_trans_rate = mTotalRequest / total_s;
        uint64_t inst_trans_rate = mIntervalRequest / interval_s;
        uint64_t pending_req = fdb_bi_direction ? ((mTotalRequest < mTotalReply) ? 0 : (mTotalRequest - mTotalReply)) : 0;
        uint64_t avg_delay = mTotalReply ? mTotalDelay / mTotalReply : 0;
        
        printf("%12u B/s %12u B/s %8u Req/s %8u Req/s %8u %6u %10u us %10u us\n",
                (uint32_t)avg_data_rate, (uint32_t)inst_data_rate, (uint32_t)avg_trans_rate,
                (uint32_t)inst_trans_rate, (uint32_t)pending_req, (uint32_t)mFailureCount,
                (uint32_t)avg_delay, (uint32_t)mMaxDelay);
        resetInterval();
    }
    void sendData()
    {
        send(XCLT_TEST_SINGLE_DIRECTION, mBuffer, fdb_block_size);
        incrementSend(fdb_block_size);
    }
    void invokeMethod()
    {
	if (fdb_sync_invoke)
	{
            CBaseJob::Ptr ref(new CBaseMessage(XCLT_TEST_BI_DIRECTION));
            invoke(ref);
	    handleReply(ref);
	}
	else
	{
            invoke(XCLT_TEST_BI_DIRECTION, mBuffer, fdb_block_size);
	}
        incrementSend(fdb_block_size);
    }
protected:
    /* called when connected to the server */
    void onOnline(FdbSessionId_t sid, bool is_first)
    {
        resetTotal();
        mTimer->enable();
        fdb_stop_job = false;
        fdb_worker_A->sendAsync(new CXTestJob());
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
        mTotalBytesSent += size;
        mIntervalBytesSent += size;
        mTotalRequest++;
        mIntervalRequest++;
    }

    void incrementReceive(uint32_t size)
    {
        mTotalBytesReceived += size;
        mIntervalBytesReceived += size;
        mTotalReply++;
        mIntervalReply++;
    }

    void getdownDelay(uint64_t delay)
    {
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
        auto msg = castToMessage<CBaseMessage *>(msg_ref);
        CFdbMsgMetadata md;
        msg->metadata(md);
        uint64_t c2s, s2r, r2c, total;
        msg->parseTimestamp(md, c2s, s2r, r2c, total);
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
                getdownDelay(total);
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
        if (fdb_bi_direction)
        {
            fdb_xtest_client->invokeMethod();
        }
        else
        {
            fdb_xtest_client->sendData();
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

    peer_worker->sendAsync(new CXTestJob());
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
    int32_t uni_direction = 0;
    uint32_t block_size = 1024;
    uint32_t delay = 0;
    int32_t sync_invoke = 0;
    const struct fdb_option core_options[] = {
        { FDB_OPTION_INTEGER, "block_size", 'b', &block_size},
        { FDB_OPTION_INTEGER, "burst_size", 's', &burst_size},
        { FDB_OPTION_INTEGER, "delay", 'd', &delay},
        { FDB_OPTION_BOOLEAN, "uni_direction", 'u', &uni_direction},
        { FDB_OPTION_BOOLEAN, "sync", 'y', &sync_invoke},
        { FDB_OPTION_BOOLEAN, "help", 'h', &help}
    };
    fdb_parse_options(core_options, ARRAY_LENGTH(core_options), &argc, argv);

    fdb_burst_size = burst_size;
    fdb_bi_direction = !uni_direction;
    fdb_block_size = block_size;
    fdb_delay = delay;
    fdb_sync_invoke = !!sync_invoke;

    std::cout << "block size: " << fdb_block_size
              << ", burst size: " << fdb_burst_size
              << ", bi-direction: " << fdb_bi_direction
              << ", delay: " << fdb_delay
              << ", sync: " << fdb_sync_invoke
              << std::endl;

    if (help)
    {
        std::cout << "FDBus version " << FDB_VERSION_MAJOR << "."
                                      << FDB_VERSION_MINOR << "."
                                      << FDB_VERSION_BUILD << std::endl;
        std::cout << "Usage: fdbxclient[ -b block size][ -s burst size][-d delay][ -u]" << std::endl;
        std::cout << "    -b block size: specify size of date sent for each request" << std::endl;
        std::cout << "    -s burst size: specify how many requests are sent in batch for a burst" << std::endl;
        std::cout << "    -d delay: specify delay between two bursts in micro second" << std::endl;
        std::cout << "    -u: if not specified, dual-way (request-reply) are tested; otherwise only test one way (request)" << std::endl;
        std::cout << "    -y: " << std::endl;
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

