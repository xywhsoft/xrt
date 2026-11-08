#include <xredis.h>
#include "../tests/support/transaction_peer.h"
static int transactionCompare(const void* Left, const void* Right)
{ double A = *(const double*)Left, B = *(const double*)Right; return (A > B) - (A < B); }
int main(int argc, char** argv)
{
    size_t Count = argc > 1 ? (size_t)strtoull(argv[1], NULL, 10) : 1000;
    if (!Count || Count > 1000000) return 2;
    double* Samples = xrtCalloc(Count, sizeof(double)); if (!Samples) return 1;
    redistransactionpeer Peer; redisTransactionPeerStart(&Peer, TX_NORMAL, false);
    xredisclient* Client = redisTransactionPeerConnect(&Peer, NULL); xfuture* Ready = xredisClientReady(Client);
    redisPeerRequire(xrtFutureWaitFor(Ready, 10000) == XWAIT_OK && ((const xredisreadyresult*)xrtFutureValue(Ready))->Status == XREDIS_READY_OK, "benchmark readiness");
    xrtFutureDestroy(Ready); xbytesview Body = XRT_BYTES_LITERAL("BODY"); xredistransactioncommand Commands[] = {{&Body, 1}, {&Body, 1}};
    double Start = xrtTimer();
    for (size_t Index = 0; Index < Count; ++Index) {
        double Began = xrtTimer(); xfuture* Future = NULL;
        redisPeerRequire(xredisClientTransaction(Client, Commands, 2, NULL, 5000, &Future) == XREDIS_ADMISSION_ACCEPTED, "benchmark transaction admission");
        const xredistransactionresult* Result = xrtFutureValue(Future);
        redisPeerRequire(Result && Result->Request.Status == XREDIS_REQUEST_OK && Result->Execution == XREDIS_TRANSACTION_EXECUTED && Result->Queued == 2, "benchmark transaction result");
        xrtFutureDestroy(Future); Samples[Index] = (xrtTimer() - Began) * 1000000;
    }
    double Elapsed = xrtTimer() - Start; qsort(Samples, Count, sizeof(double), transactionCompare);
    printf("transactions_per_sec: %.9f\ntransaction_p50_us: %.9f\ntransaction_p99_us: %.9f\ncompleted: %zu\n",
        (double)Count / Elapsed, Samples[(Count * 50u + 99u) / 100u - 1u], Samples[(Count * 99u + 99u) / 100u - 1u], Count);
    puts("workload: sequential two-command staged MULTI/queue/EXEC; two-worker local C fixture; client API entry through result release");
    redisTransactionPeerStop(&Peer); xrtFree(Samples); return 0;
}
