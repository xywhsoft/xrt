#include <xredis.h>
#include "../tests/support/client_peer.h"
#include <inttypes.h>

static int compareLatency(const void* Left, const void* Right)
{
	double A = *(const double*)Left, B = *(const double*)Right; return (A > B) - (A < B);
}

int main(int argc, char** argv)
{
	size_t Count = argc > 1 ? (size_t)strtoull(argv[1], NULL, 10) : 100000;
	size_t Depth = argc > 2 ? (size_t)strtoull(argv[2], NULL, 10) : 16;
	if (Count == 0 || Count > 1000000 || Depth == 0 || Depth > 128) return 1;
	double* Latencies = (double*)malloc(Count * sizeof(double)); if (Latencies == NULL) return 2;
	redispeer Peer; redisPeerStart(&Peer); xredisclient* Client = NULL;
	redisPeerRequire(xredisClientConnect(Peer.Engine, &Peer.Address, 0, NULL, &Client), "bench connect");
	xbytesview Warm = XRT_BYTES_LITERAL("PING"); xfuture* Ready = NULL;
	redisPeerRequire(xredisClientCall(Client, &Warm, 1, 5000, &Ready) == XREDIS_ADMISSION_ACCEPTED, "bench warmup"); xrtFutureDestroy(Ready);
	uint64 Checksum = 0; double Start = xrtTimer();
	for (size_t Offset = 0; Offset < Count; Offset += Depth) {
		xfuture* Futures[128]; double Submitted[128]; size_t Batch = Count - Offset < Depth ? Count - Offset : Depth;
		for (size_t Index = 0; Index < Batch; ++Index) {
			char Number[32]; int Bytes = snprintf(Number, sizeof(Number), "%zu", Offset + Index);
			xbytesview Args[] = {XRT_BYTES_LITERAL("ID"), {(cbytes)Number, (size_t)Bytes}};
			Submitted[Index] = xrtTimer();
			redisPeerRequire(xredisClientCallAsync(Client, Args, 2, 5000, &Futures[Index]) == XREDIS_ADMISSION_ACCEPTED, "bench admission");
		}
		for (size_t Index = 0; Index < Batch; ++Index) {
			redisPeerRequire(xrtFutureWaitFor(Futures[Index], 10000) == XWAIT_OK, "bench completion");
			Latencies[Offset + Index] = (xrtTimer() - Submitted[Index]) * 1000000.0;
			const xredisrequestresult* Result = (const xredisrequestresult*)xrtFutureValue(Futures[Index]); xredisreplyvalue Value;
			redisPeerRequire(Result != NULL && Result->Status == XREDIS_REQUEST_OK && xredisReplyGet(Result->Reply, 0, &Value) &&
				Value.Integer == (int64)(Offset + Index), "bench correlated result");
			Checksum += (uint64)Value.Integer; xrtFutureDestroy(Futures[Index]);
		}
	}
	double Seconds = xrtTimer() - Start; qsort(Latencies, Count, sizeof(double), compareLatency);
	printf("requests_per_sec: %.3f\n", (double)Count / Seconds);
	printf("p50_us: %.3f\n", Latencies[(Count - 1u) * 50u / 100u]);
	printf("p95_us: %.3f\n", Latencies[(Count - 1u) * 95u / 100u]);
	printf("p99_us: %.3f\n", Latencies[(Count - 1u) * 99u / 100u]);
	printf("p999_us: %.3f\n", Latencies[(Count - 1u) * 999u / 1000u]);
	printf("checksum: %" PRIu64 "\n", Checksum);
	xfuture* Closed = xredisClientClosed(Client);
	redisPeerRequire(xredisClientClose(Client) && xrtFutureWaitFor(Closed, 10000) == XWAIT_OK, "bench retire");
	xredisClientRelease(Client); xrtFutureDestroy(Closed); redisPeerStop(&Peer); free(Latencies); return 0;
}
