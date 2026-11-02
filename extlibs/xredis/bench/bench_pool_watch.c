#include <xredis.h>
#include "../tests/support/pool_sessions_peer.h"
static int poolWatchCompare(const void* A, const void* B)
{ double L = *(const double*)A, R = *(const double*)B; return (L > R) - (L < R); }
int main(int argc, char** argv)
{
    size_t Count = argc > 1 ? (size_t)strtoull(argv[1], NULL, 10) : 1000; if (!Count || Count > 1000000) return 2;
    double* Samples = xrtCalloc(Count, sizeof(double)); if (!Samples) return 1;
    redispoolsessionspeer Peer; redisPoolSessionsPeerStart(&Peer, W_NORMAL, TX_NORMAL, false, NULL);
    xbytesview Key = XRT_BYTES_LITERAL("bench-watch"), Echo[] = {XRT_BYTES_LITERAL("ECHO"), XRT_BYTES_LITERAL("watch\0read")};
    xredistransactioncommand Commands[] = {{Echo, 2}, {Echo, 2}}; double Started = xrtTimer();
    for (size_t Index = 0; Index < Count; ++Index) {
        double Began = xrtTimer(); xfuture *Acquired = NULL, *ScopeFuture = NULL, *Read = NULL, *Transaction = NULL;
        redisPeerRequire(xredisPoolAcquire(Peer.Pool, 5000, &Acquired) == XREDIS_ADMISSION_ACCEPTED, "benchmark watch lease");
        const xredisleaseresult* Lease = xrtFutureValue(Acquired); redisPeerRequire(Lease && Lease->Lease &&
            xredisLeaseWatch(Lease->Lease, &Key, 1, NULL, &ScopeFuture) == XREDIS_ADMISSION_ACCEPTED, "benchmark nested start");
        const xrediswatchresult* Scope = xrtFutureValue(ScopeFuture); redisPeerRequire(Scope && Scope->Request.Status == XREDIS_REQUEST_OK, "benchmark WATCH ready");
        redisPeerRequire(xredisWatchCall(Scope->Watch, Echo, 2, 5000, &Read) == XREDIS_ADMISSION_ACCEPTED &&
            ((const xredisrequestresult*)xrtFutureValue(Read))->Status == XREDIS_REQUEST_OK, "benchmark nested read");
        redisPeerRequire(xredisWatchCommit(Scope->Watch, Commands, 2, 5000, &Transaction) == XREDIS_ADMISSION_ACCEPTED &&
            ((const xredistransactionresult*)xrtFutureValue(Transaction))->Execution == XREDIS_TRANSACTION_EXECUTED, "benchmark nested EXEC");
        xfuture* ScopeClosed = xredisWatchClosed(Scope->Watch); redisPeerRequire(xrtFutureWaitFor(ScopeClosed, 10000) == XWAIT_OK, "benchmark child barrier");
        xfuture* Returned = xredisLeaseClosed(Lease->Lease); xredisLeaseReturn(Lease->Lease);
        redisPeerRequire(xrtFutureWaitFor(Returned, 10000) == XWAIT_OK && ((const xredisleaseclosedresult*)xrtFutureValue(Returned))->Reusable, "benchmark parent barrier");
        xrtFutureDestroy(Acquired); xrtFutureDestroy(ScopeFuture); xrtFutureDestroy(Read); xrtFutureDestroy(Transaction); xrtFutureDestroy(ScopeClosed); xrtFutureDestroy(Returned);
        Samples[Index] = (xrtTimer() - Began) * 1000000;
    }
    double Seconds = xrtTimer() - Started; qsort(Samples, Count, sizeof(double), poolWatchCompare);
    printf("cycles_per_sec: %.9f\ncycle_p50_us: %.9f\ncycle_p99_us: %.9f\ncompleted: %zu\n", (double)Count / Seconds,
        Samples[(Count * 50u + 99u) / 100u - 1u], Samples[(Count * 99u + 99u) / 100u - 1u], Count);
    puts("workload: warm lease acquire / WATCH / binary read / two-command EXEC / child Closed / physical Return / observer release; TCP/pool construction excluded; different client/pool workers");
    redisPoolSessionsPeerStop(&Peer); xrtFree(Samples); return 0;
}
