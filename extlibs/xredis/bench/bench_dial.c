#include <xredis.h>
#include "../tests/support/dial_peer.h"

static int compareDial(const void* A, const void* B)
{ double Left = *(const double*)A, Right = *(const double*)B; return (Left > Right) - (Left < Right); }
static double dialPercentile(const double* Sorted, size_t Count, size_t PerMille)
{ return Sorted[(Count * PerMille + 999u) / 1000u - 1u]; }
static bool dialNumber(cstr Input, size_t* Output)
{
	if (*Input == 0) return false;
	size_t Value = 0;
	for (; *Input != 0; ++Input) {
		if (*Input < '0' || *Input > '9' || Value > (SIZE_MAX - (size_t)(*Input - '0')) / 10u) return false;
		Value = Value * 10u + (size_t)(*Input - '0');
	}
	*Output = Value; return true;
}

int main(int argc, char** argv)
{
	size_t Count = 256, Mode = 1, Stats = 0;
	if (argc > 4 || (argc > 1 && !dialNumber(argv[1], &Count)) || (argc > 2 && !dialNumber(argv[2], &Mode)) ||
		(argc > 3 && !dialNumber(argv[3], &Stats)) || Count == 0 || Count > 4096 || Mode > 2 || Stats > 1) return 1;
#if defined(XRT_FEATURE_MEMORY_STATS)
	bool Previous = xrtMemStatsEnabled(); xrtMemStatsEnable(Stats != 0); if (Stats) xrtMemStatsReset();
#else
	if (Stats) return 1;
#endif
	double* Latency = (double*)malloc(Count * sizeof(double)); if (Latency == NULL) return 2;
	uint64 Lookups = 0; double Begin = xrtTimer();
	for (size_t Index = 0; Index < Count; ++Index) {
		redisdialpeer Peer; redisDialPeerStart(&Peer, 0, 0, 0);
		if (Index == 0) {
			xnetportbackendinfo Backend; redisPeerRequire(xrtNetEngineBackendInfo(Peer.Init.Peer.Engine, 0, &Backend), "Dial backend snapshot");
			printf("network_backend: %s\n", Backend.Name);
		}
		xnetaddrlist* Addresses = Mode == 2 ? xrtNetAddrListCreate(&Peer.Init.Peer.Address, 1) : NULL;
		if (Mode == 2) redisPeerRequire(Addresses != NULL, "Dial benchmark endpoint list");
		xredisclient* Client = NULL; double Submitted = xrtTimer(); bool Admitted;
		if (Mode == 0) Admitted = xredisClientConnectInit(Peer.Init.Peer.Engine, &Peer.Init.Peer.Address, 0, NULL, NULL, &Client);
		else if (Mode == 1) Admitted = xredisClientDial(Peer.Init.Peer.Engine, Peer.Resolver, XRT_STR_LITERAL("redis.fixture"), Peer.Init.Peer.Address.Port,
			0, NULL, NULL, NULL, &Client);
		else Admitted = xredisClientDialAddresses(Peer.Init.Peer.Engine, Addresses, 0, NULL, NULL, NULL, &Client);
		redisPeerRequire(Admitted, "cold benchmark admission"); xrtNetAddrListDestroy(Addresses);
		xfuture* Ready = redisDialReady(Client); Latency[Index] = (xrtTimer() - Submitted) * 1000000.0;
		const xredisreadyresult* Result = (const xredisreadyresult*)xrtFutureValue(Ready);
		redisPeerRequire(Result != NULL && Result->Status == XREDIS_READY_OK && Result->Negotiated && Result->Hello != NULL, "cold benchmark setup result");
		redisDialClientClose(Client); redisDialPeerStop(&Peer); xrtFutureDestroy(Ready);
		Lookups += xrtAtomic32Load(&Peer.Lookups, XMEMORY_ACQUIRE);
	}
	double Seconds = xrtTimer() - Begin; redisPeerRequire(Lookups == (Mode == 1 ? Count : 0), "unexpected cold query count");
	qsort(Latency, Count, sizeof(double), compareDial);
	printf("connection_mode: %s\n", Mode == 0 ? "numeric_in_dial_binary" : Mode == 1 ? "hostname_fixture" : "prepared_endpoints");
	printf("arrival_model: serial_cold\nworkers: 2\nsamples: %zu\nlookups: %llu\n", Count, (unsigned long long)Lookups);
	printf("lifecycle_per_sec: %.3f\n", (double)Count / Seconds);
	printf("handshake_p50_us: %.3f\n", dialPercentile(Latency, Count, 500));
	printf("handshake_p95_us: %.3f\n", dialPercentile(Latency, Count, 950));
	printf("handshake_p99_us: %.3f\n", dialPercentile(Latency, Count, 990));
	printf("handshake_p999_us: %.3f\n", dialPercentile(Latency, Count, 999));
#if defined(XRT_FEATURE_MEMORY_STATS)
	printf("memory_stats: %s\n", Stats ? "enabled_process_window" : "disabled");
	if (Stats) {
		xmemstats Memory; xrtMemStatsGet(&Memory);
		printf("window_malloc_calls: %llu\nwindow_calloc_calls: %llu\nwindow_realloc_calls: %llu\nwindow_block_alloc_calls: %llu\nwindow_backing_alloc_calls: %llu\nwindow_backing_alloc_bytes: %llu\n",
			(unsigned long long)Memory.MallocCalls, (unsigned long long)Memory.CallocCalls, (unsigned long long)Memory.ReallocCalls,
			(unsigned long long)Memory.BlockAllocCalls, (unsigned long long)Memory.BackingAllocCalls, (unsigned long long)Memory.BackingAllocBytes);
	}
	xrtMemStatsEnable(Previous);
#else
	puts("memory_stats: not_compiled");
#endif
	free(Latency); return 0;
}
