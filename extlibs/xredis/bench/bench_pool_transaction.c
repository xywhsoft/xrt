#include <xredis.h>
#include "../tests/support/pool_sessions_peer.h"
static int poolTransactionCompare(const void* A, const void* B)
{ double L = *(const double*)A, R = *(const double*)B; return (L > R) - (L < R); }
int main(int argc, char** argv)
{
    size_t Count = argc > 1 ? (size_t)strtoull(argv[1], NULL, 10) : 1000; if (!Count || Count > 1000000) return 2;
    double* Samples = xrtCalloc(Count, sizeof(double)); if (!Samples) return 1;
    redispoolsessionspeer Peer; redisPoolSessionsPeerStart(&Peer, W_NORMAL, TX_NORMAL, false, NULL);
    xbytesview Body = XRT_BYTES_LITERAL("BODY"); xredistransactioncommand Commands[] = {{&Body, 1}, {&Body, 1}}; double Started = xrtTimer();
    for (size_t Index = 0; Index < Count; ++Index) {
        double Began = xrtTimer(); xfuture *Acquired = NULL, *Transaction = NULL;
        redisPeerRequire(xredisPoolAcquire(Peer.Pool, 5000, &Acquired) == XREDIS_ADMISSION_ACCEPTED, "benchmark pooled acquire");
        const xredisleaseresult* Lease = xrtFutureValue(Acquired); redisPeerRequire(Lease && Lease->Lease, "benchmark lease result");
        redisPeerRequire(xredisLeaseTransaction(Lease->Lease, Commands, 2, NULL, 5000, &Transaction) == XREDIS_ADMISSION_ACCEPTED &&
            ((const xredistransactionresult*)xrtFutureValue(Transaction))->Execution == XREDIS_TRANSACTION_EXECUTED, "benchmark native EXEC");
        xfuture* Returned = xredisLeaseClosed(Lease->Lease); xredisLeaseReturn(Lease->Lease);
        redisPeerRequire(xrtFutureWaitFor(Returned, 10000) == XWAIT_OK && ((const xredisleaseclosedresult*)xrtFutureValue(Returned))->Reusable, "benchmark physical return");
        xrtFutureDestroy(Acquired); xrtFutureDestroy(Transaction); xrtFutureDestroy(Returned); Samples[Index] = (xrtTimer() - Began) * 1000000;
    }
    double Seconds = xrtTimer() - Started; qsort(Samples, Count, sizeof(double), poolTransactionCompare);
    printf("cycles_per_sec: %.9f\ncycle_p50_us: %.9f\ncycle_p99_us: %.9f\ncompleted: %zu\n", (double)Count / Seconds,
        Samples[(Count * 50u + 99u) / 100u - 1u], Samples[(Count * 99u + 99u) / 100u - 1u], Count);
    puts("workload: warm lease acquire / native two-command staged transaction / physical Return / observer release; TCP/pool construction excluded; client worker0 and pool worker1");
    redisPoolSessionsPeerStop(&Peer); xrtFree(Samples); return 0;
}
