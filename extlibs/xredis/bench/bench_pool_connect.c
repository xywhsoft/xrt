#include <xredis.h>
#include "../tests/support/pool_connect_peer.h"
static int pcBenchCompare(const void* Left, const void* Right)
{ double A = *(const double*)Left, B = *(const double*)Right; return (A > B) - (A < B); }
static xfuture* pcBenchAcquire(xredispool* Pool)
{
    xfuture* Future = NULL; redisPeerRequire(xredisPoolAcquire(Pool, 5000, &Future) == XREDIS_ADMISSION_ACCEPTED &&
        ((const xredisleaseresult*)xrtFutureValue(Future))->Lease, "benchmark automatic acquire"); return Future;
}
static void pcBenchReturn(xfuture* Future, bool Abort)
{
    xredislease* Lease = ((const xredisleaseresult*)xrtFutureValue(Future))->Lease; xfuture* Closed = xredisLeaseClosed(Lease);
    if (Abort) xredisLeaseAbort(Lease); else xredisLeaseReturn(Lease);
    redisPeerRequire(xrtFutureWaitFor(Closed, 10000) == XWAIT_OK && ((const xredisleaseclosedresult*)xrtFutureValue(Closed))->Reusable != Abort, "benchmark physical boundary");
    xrtFutureDestroy(Closed); xrtFutureDestroy(Future);
}
int main(int argc, char** argv)
{
    size_t Count = argc > 1 ? (size_t)strtoull(argv[1], NULL, 10) : 1000;
    unsigned Mode = argc > 2 ? (unsigned)strtoul(argv[2], NULL, 10) : 0; bool Repair = Mode == 1, Fixed = Mode == 2;
    if (!Count || Count > 1000000) return 2;
    double* Samples = xrtCalloc(Count, sizeof(double)); if (!Samples) return 1;
    redisconnectpeer Peer; redisConnectPeerStart(&Peer); redisconnectfactory Factory; xredispoolconnectconfig Config;
    redisConnectFactoryConfig(&Factory, &Peer, &Config); Config.RetryMin = Config.RetryMax = 1; xredispool* Pool = NULL;
    if (Fixed) {
        xredisclient* Client = NULL;
        redisPeerRequire(redisConnectFactory(Peer.Engine, 0, 1, 5000, &Factory, &Client), "benchmark matched fixed member");
        xfuture* Ready = xredisClientReady(Client);
        redisPeerRequire(xrtFutureWaitFor(Ready, 10000) == XWAIT_OK && ((const xredisreadyresult*)xrtFutureValue(Ready))->Status == XREDIS_READY_OK &&
            xredisPoolCreate(Peer.Engine, 0, &Client, 1, NULL, &Pool) == XREDIS_ADMISSION_ACCEPTED, "benchmark matched fixed pool");
        xrtFutureDestroy(Ready); xredisClientRelease(Client);
    } else redisPeerRequire(xredisPoolConnect(Peer.Engine, 0, 1, NULL, &Config, &Pool) == XREDIS_ADMISSION_ACCEPTED, "benchmark automatic pool");
    pcBenchReturn(pcBenchAcquire(Pool), false); xbytesview Ping = XRT_BYTES_LITERAL("PING"); double Started = xrtTimer();
    for (size_t Index = 0; Index < Count; ++Index) {
        double Began = xrtTimer(); xfuture* Acquired = pcBenchAcquire(Pool); xfuture* Call = NULL;
        xredislease* Lease = ((const xredisleaseresult*)xrtFutureValue(Acquired))->Lease;
        redisPeerRequire(xredisLeaseCall(Lease, &Ping, 1, 5000, &Call) == XREDIS_ADMISSION_ACCEPTED &&
            ((const xredisrequestresult*)xrtFutureValue(Call))->Status == XREDIS_REQUEST_OK, "benchmark native PING");
        xrtFutureDestroy(Call); pcBenchReturn(Acquired, Repair);
        if (Repair) pcBenchReturn(pcBenchAcquire(Pool), false); /* Includes retry, TCP/setup/readiness and safe reuse. */
        Samples[Index] = (xrtTimer() - Began) * 1000000.0;
    }
    double Seconds = xrtTimer() - Started; qsort(Samples, Count, sizeof(double), pcBenchCompare);
    if (!Fixed) {
        xredispoolconnectinfo Info = {0}; Info.Size = sizeof(Info); xredisPoolConnectInfo(Pool, &Info);
        redisPeerRequire(Info.Connected == (Repair ? Count + 1u : 1u), "benchmark generation count");
    }
    printf("cycles_per_sec: %.9f\ncycle_p50_us: %.9f\ncycle_p99_us: %.9f\ncompleted: %zu\n", (double)Count / Seconds,
        Samples[(Count * 50u + 99u) / 100u - 1u], Samples[(Count * 99u + 99u) / 100u - 1u], Count);
    puts(Repair ? "workload: automatic pool; acquire/PING/Abort/physical close/1ms retry/TCP HELLO/reacquire/Return; two-worker local C peer; startup excluded" :
        Fixed ? "workload: matched fixed pool with connector compiled; warm acquire/PING/physical Return/observer release; identical peer, input and workers; startup excluded" :
        "workload: automatic pool; warm acquire/PING/physical Return/observer release; two-worker local C peer; startup excluded");
    xfuture* Closed = xredisPoolClosed(Pool); xredisPoolClose(Pool); redisPeerRequire(xrtFutureWaitFor(Closed, 10000) == XWAIT_OK, "benchmark pool close");
    if (Fixed) redisConnectFactoryDrop(&Factory);
    xredisPoolRelease(Pool); xrtFutureDestroy(Closed); redisConnectPeerStop(&Peer); xrtFree(Samples); return 0;
}
