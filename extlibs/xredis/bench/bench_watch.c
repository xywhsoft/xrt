#include <xredis.h>
#include "../tests/support/watch_peer.h"
static int watchCompare(const void* Left, const void* Right)
{ double A = *(const double*)Left, B = *(const double*)Right; return (A > B) - (A < B); }
int main(int argc, char** argv)
{
    size_t Count = argc > 1 ? (size_t)strtoull(argv[1], NULL, 10) : 1000;
    if (!Count || Count > 1000000) return 2;
    double* Samples = xrtCalloc(Count, sizeof(double)); if (!Samples) return 1;
    rediswatchpeer Peer; redisWatchPeerStart(&Peer, W_NORMAL, TX_NORMAL, false);
    xredisclient* Client = redisTransactionPeerConnect(&Peer.Tx, NULL); redisWatchPeerReady(Client);
    xbytesview Key = XRT_BYTES_LITERAL("watch-key"), Echo[] = {XRT_BYTES_LITERAL("ECHO"), XRT_BYTES_LITERAL("watch\0binary")};
    xredistransactioncommand Commands[] = {{Echo, 2}, {Echo, 2}}; double Start = xrtTimer();
    for (size_t Index = 0; Index < Count; ++Index) {
        double Began = xrtTimer(); xfuture *Started = NULL, *Read = NULL, *Commit = NULL;
        redisPeerRequire(xredisClientWatch(Client, &Key, 1, NULL, &Started) == XREDIS_ADMISSION_ACCEPTED, "watch benchmark admission");
        const xrediswatchresult* Result = xrtFutureValue(Started); redisPeerRequire(Result && Result->Watch, "watch benchmark readiness");
        redisPeerRequire(xredisWatchCall(Result->Watch, Echo, 2, 5000, &Read) == XREDIS_ADMISSION_ACCEPTED &&
            ((const xredisrequestresult*)xrtFutureValue(Read))->Status == XREDIS_REQUEST_OK, "watch benchmark read");
        redisPeerRequire(xredisWatchCommit(Result->Watch, Commands, 2, 5000, &Commit) == XREDIS_ADMISSION_ACCEPTED &&
            ((const xredistransactionresult*)xrtFutureValue(Commit))->Execution == XREDIS_TRANSACTION_EXECUTED, "watch benchmark EXEC");
        xfuture* Closed = xredisWatchClosed(Result->Watch);
        redisPeerRequire(xrtFutureWaitFor(Closed, 10000) == XWAIT_OK && ((const xrediswatchclosedresult*)xrtFutureValue(Closed))->Reusable,
            "watch benchmark physical retirement");
        xrtFutureDestroy(Started); xrtFutureDestroy(Read); xrtFutureDestroy(Commit); xrtFutureDestroy(Closed);
        Samples[Index] = (xrtTimer() - Began) * 1000000;
    }
    double Elapsed = xrtTimer() - Start; qsort(Samples, Count, sizeof(double), watchCompare);
    printf("scopes_per_sec: %.9f\nscope_p50_us: %.9f\nscope_p99_us: %.9f\ncompleted: %zu\n", (double)Count / Elapsed,
        Samples[(Count * 50u + 99u) / 100u - 1u], Samples[(Count * 99u + 99u) / 100u - 1u], Count);
    puts("workload: sequential WATCH / binary scoped read / two-command staged EXEC / physical Closed; two-worker local C fixture; full observer release included");
    redisTransactionPeerStop(&Peer.Tx); xrtFree(Samples); return 0;
}
