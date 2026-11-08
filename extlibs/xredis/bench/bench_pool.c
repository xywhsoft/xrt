#include <xredis.h>
#include "../tests/support/pool_peer.h"
static int poolCycleCompare(const void* Left, const void* Right)
{ double A = *(const double*)Left, B = *(const double*)Right; return (A > B) - (A < B); }
int main(int argc, char** argv)
{
    size_t Count = argc > 1 ? (size_t)strtoull(argv[1], NULL, 10) : 1000; if (!Count || Count > 1000000) return 2;
    double* Samples = xrtCalloc(Count, sizeof(double)); if (!Samples) return 1;
    redispoolpeer Peer; redisPoolPeerStart(&Peer, 1); xredispool* Pool = NULL;
    redisPeerRequire(xredisPoolCreate(Peer.Peers[0].Engine, 0, Peer.Clients, 1, NULL, &Pool) == XREDIS_ADMISSION_ACCEPTED, "cycle benchmark create");
    xbytesview Ping = XRT_BYTES_LITERAL("PING"); double Started = xrtTimer();
    for (size_t Index = 0; Index < Count; ++Index) {
        double Began = xrtTimer(); xfuture *Acquired = NULL, *Call = NULL;
        redisPeerRequire(xredisPoolAcquire(Pool, 5000, &Acquired) == XREDIS_ADMISSION_ACCEPTED, "cycle benchmark acquire");
        const xredisleaseresult* Result = xrtFutureValue(Acquired); redisPeerRequire(Result && Result->Lease, "cycle benchmark grant");
        redisPeerRequire(xredisLeaseCall(Result->Lease, &Ping, 1, 5000, &Call) == XREDIS_ADMISSION_ACCEPTED &&
            ((const xredisrequestresult*)xrtFutureValue(Call))->Status == XREDIS_REQUEST_OK, "cycle benchmark native request");
        xfuture* Returned = xredisLeaseClosed(Result->Lease); xredisLeaseReturn(Result->Lease);
        redisPeerRequire(xrtFutureWaitFor(Returned, 10000) == XWAIT_OK && ((const xredisleaseclosedresult*)xrtFutureValue(Returned))->Reusable, "cycle benchmark physical return");
        xrtFutureDestroy(Acquired); xrtFutureDestroy(Call); xrtFutureDestroy(Returned); Samples[Index] = (xrtTimer() - Began) * 1000000.0;
    }
    double Seconds = xrtTimer() - Started; qsort(Samples, Count, sizeof(double), poolCycleCompare);
    printf("leases_per_sec: %.9f\nlease_p50_us: %.9f\nlease_p99_us: %.9f\ncompleted: %zu\n", (double)Count / Seconds,
        Samples[(Count * 50u + 99u) / 100u - 1u], Samples[(Count * 99u + 99u) / 100u - 1u], Count);
    puts("workload: warm fixed-membership pool; acquire / PING / physical Return / observer release; two-worker local C peer; pool construction and TCP setup excluded");
    xfuture* Closed = xredisPoolClosed(Pool); xredisPoolClose(Pool); redisPeerRequire(xrtFutureWaitFor(Closed, 10000) == XWAIT_OK, "cycle benchmark close");
    xredisPoolRelease(Pool); redisPoolPeerStop(&Peer); xrtFutureDestroy(Closed); xrtFree(Samples); return 0;
}
