#include <xredis.h>
#include "../tests/support/pool_peer.h"
#include <inttypes.h>
static int poolPipelineCompare(const void* Left, const void* Right)
{ double A = *(const double*)Left, B = *(const double*)Right; return (A > B) - (A < B); }
int main(int argc, char** argv)
{
    size_t Count = argc > 1 ? (size_t)strtoull(argv[1], NULL, 10) : 100000;
    size_t Depth = argc > 2 ? (size_t)strtoull(argv[2], NULL, 10) : 16;
    bool Pooled = argc > 3 && !strcmp(argv[3], "1");
    if (!Count || Count > 1000000 || !Depth || Depth > 128) return 1;
    double* Latencies = malloc(Count * sizeof(double)); if (!Latencies) return 2;
    redispoolpeer Peer; redisPoolPeerStart(&Peer, 1); xredisclient* Client = Peer.Clients[0]; xredispool* Pool = NULL;
    xfuture* Acquired = NULL; xredislease* Lease = NULL;
    if (Pooled) {
        xredispoolconfig Config; xredisPoolConfigInit(&Config); Config.LeaseTimeout = 120000;
        redisPeerRequire(xredisPoolCreate(Peer.Peers[0].Engine, 0, &Client, 1, &Config, &Pool) == XREDIS_ADMISSION_ACCEPTED &&
            xredisPoolAcquire(Pool, 5000, &Acquired) == XREDIS_ADMISSION_ACCEPTED, "pipeline benchmark lease");
        const xredisleaseresult* Result = xrtFutureValue(Acquired); redisPeerRequire(Result && Result->Lease, "pipeline benchmark lease result"); Lease = Result->Lease;
    }
    xbytesview Ping = XRT_BYTES_LITERAL("PING"); xfuture* Ready = NULL;
    redisPeerRequire((Pooled ? xredisLeaseCall(Lease, &Ping, 1, 5000, &Ready) : xredisClientCall(Client, &Ping, 1, 5000, &Ready)) == XREDIS_ADMISSION_ACCEPTED,
        "pipeline benchmark warmup"); xrtFutureDestroy(Ready);
    uint64 Checksum = 0; double Start = xrtTimer();
    for (size_t Offset = 0; Offset < Count; Offset += Depth) {
        xfuture* Futures[128]; double Submitted[128]; size_t Batch = Count - Offset < Depth ? Count - Offset : Depth;
        for (size_t Index = 0; Index < Batch; ++Index) {
            char Number[32]; int Bytes = snprintf(Number, sizeof(Number), "%zu", Offset + Index);
            xbytesview Args[] = {XRT_BYTES_LITERAL("ID"), {(cbytes)Number, (size_t)Bytes}}; Submitted[Index] = xrtTimer();
            xredisadmission Admission = Pooled ? xredisLeaseCallAsync(Lease, Args, 2, 5000, &Futures[Index]) : xredisClientCallAsync(Client, Args, 2, 5000, &Futures[Index]);
            redisPeerRequire(Admission == XREDIS_ADMISSION_ACCEPTED, "pipeline benchmark native admission");
        }
        for (size_t Index = 0; Index < Batch; ++Index) {
            redisPeerRequire(xrtFutureWaitFor(Futures[Index], 10000) == XWAIT_OK, "pipeline benchmark completion");
            Latencies[Offset + Index] = (xrtTimer() - Submitted[Index]) * 1000000.0;
            const xredisrequestresult* Result = xrtFutureValue(Futures[Index]); xredisreplyvalue Value;
            redisPeerRequire(Result && Result->Status == XREDIS_REQUEST_OK && xredisReplyGet(Result->Reply, 0, &Value) &&
                Value.Integer == (int64)(Offset + Index), "pipeline benchmark correlation"); Checksum += (uint64)Value.Integer; xrtFutureDestroy(Futures[Index]);
        }
    }
    double Seconds = xrtTimer() - Start; qsort(Latencies, Count, sizeof(double), poolPipelineCompare);
    printf("requests_per_sec: %.3f\np50_us: %.3f\np95_us: %.3f\np99_us: %.3f\np999_us: %.3f\nchecksum: %" PRIu64 "\n", (double)Count / Seconds,
        Latencies[(Count - 1u) * 50u / 100u], Latencies[(Count - 1u) * 95u / 100u], Latencies[(Count - 1u) * 99u / 100u], Latencies[(Count - 1u) * 999u / 1000u], Checksum);
    puts("workload: identical managed ID pipeline, two-worker local C peer; API entry through result observation; lease checkout/return excluded and measured separately");
    if (Pool) {
        xfuture* Returned = xredisLeaseClosed(Lease); xredisLeaseReturn(Lease);
        redisPeerRequire(xrtFutureWaitFor(Returned, 10000) == XWAIT_OK && ((const xredisleaseclosedresult*)xrtFutureValue(Returned))->Reusable, "pipeline benchmark return");
        xfuture* Closed = xredisPoolClosed(Pool); xredisPoolClose(Pool); redisPeerRequire(xrtFutureWaitFor(Closed, 10000) == XWAIT_OK, "pipeline benchmark pool close");
        xredisPoolRelease(Pool); xrtFutureDestroy(Returned); xrtFutureDestroy(Closed); xrtFutureDestroy(Acquired);
    }
    redisPoolPeerStop(&Peer); free(Latencies); return 0;
}
