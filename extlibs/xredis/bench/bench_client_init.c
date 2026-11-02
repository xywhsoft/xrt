#include <xredis.h>
#include "../tests/support/client_init_peer.h"

static int compareInit(const void* Left, const void* Right)
{ double A = *(const double*)Left, B = *(const double*)Right; return (A > B) - (A < B); }

int main(int argc, char** argv)
{
	size_t Count = argc > 1 ? (size_t)strtoull(argv[1], NULL, 10) : 256; if (Count == 0 || Count > 4096) return 1;
	double* Latency = (double*)malloc(Count * sizeof(double)); if (Latency == NULL) return 2;
	const redisinitpeerstep Steps[] = {
		{{XRT_BYTES_LITERAL("HELLO"), XRT_BYTES_LITERAL("3"), XRT_BYTES_LITERAL("AUTH"), XRT_BYTES_LITERAL("default"), XRT_BYTES_LITERAL("fixture")}, 5,
		 XRT_BYTES_LITERAL(REDIS_INIT_HELLO_REPLY), false, false},
		{{XRT_BYTES_LITERAL("SELECT"), XRT_BYTES_LITERAL("0")}, 2, XRT_BYTES_LITERAL("+OK\r\n"), false, false}};
	double Start = xrtTimer();
	for (size_t Index = 0; Index < Count; ++Index) {
		redisinitpeer Peer; redisInitPeerStart(&Peer, Steps, 2); xredisclientinitconfig Init; xredisClientInitConfigInit(&Init);
		Init.Flags = XREDIS_INIT_AUTH | XREDIS_INIT_SELECT; Init.Password = (xbytesview)XRT_BYTES_LITERAL("fixture");
		xredisclient* Client = NULL; double Submitted = xrtTimer();
		redisPeerRequire(xredisClientConnectInit(Peer.Peer.Engine, &Peer.Peer.Address, 0, NULL, &Init, &Client), "bench initialization");
		xfuture* Ready = xredisClientReady(Client);
		redisPeerRequire(xrtFutureWaitFor(Ready, 10000) == XWAIT_OK, "bench readiness wait");
		Latency[Index] = (xrtTimer() - Submitted) * 1000000.0;
		const xredisreadyresult* Result = (const xredisreadyresult*)xrtFutureValue(Ready);
		redisPeerRequire(Result != NULL && Result->Status == XREDIS_READY_OK && Result->Negotiated && Result->Hello != NULL, "bench HELLO outcome");
		xfuture* Closed = xredisClientClosed(Client);
		redisPeerRequire(xredisClientClose(Client) && xrtFutureWaitFor(Closed, 10000) == XWAIT_OK, "bench setup retirement");
		xredisclientinfo Info = {0}; Info.Size = sizeof(Info); xredisClientInfo(Client, &Info);
		redisPeerRequire(Info.InitBytes == 0, "bench credential wire retained");
		xredisClientRelease(Client); xrtFutureDestroy(Closed); redisPeerStop(&Peer.Peer); xrtFutureDestroy(Ready);
	}
	double Seconds = xrtTimer() - Start; qsort(Latency, Count, sizeof(double), compareInit);
	/* Lifecycle throughput includes peer/engine creation and teardown. Handshake
	 * samples include encoding, worker/TCP wait and HELLO/SELECT observation. */
	printf("lifecycle_per_sec: %.3f\n", (double)Count / Seconds);
	printf("handshake_p50_us: %.3f\n", Latency[(Count - 1u) * 50u / 100u]);
	printf("handshake_p95_us: %.3f\n", Latency[(Count - 1u) * 95u / 100u]);
	printf("handshake_p99_us: %.3f\n", Latency[(Count - 1u) * 99u / 100u]);
	printf("handshake_p999_us: %.3f\n", Latency[(Count - 1u) * 999u / 1000u]); free(Latency); return 0;
}
