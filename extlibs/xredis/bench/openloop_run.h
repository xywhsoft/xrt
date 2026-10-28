#ifndef XREDIS_BENCH_OPENLOOP_RUN_H
#define XREDIS_BENCH_OPENLOOP_RUN_H
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "openloop_schedule.h"
#include "../tests/support/client_peer.h"
#include <inttypes.h>
#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#else
#include <dirent.h>
#include <sys/resource.h>
#include <time.h>
#endif

typedef struct redisopenslot {
	xfuture* Future;
	xfuturewatch Watch;
	xatomic32 Done;
	uint64 Observed;
	size_t Id;
	bool Active;
} redisopenslot;
typedef struct redisopenresult {
	redisopencounts Counts;
	double Elapsed, Retire, Cpu;
	double All[4], Success[4], Lag[4];
	uint64 Malloc, Calloc, Realloc, MemDup, BlockAlloc, BackingAlloc, BackingBytes;
	size_t HarnessBytes, RequestsPeak, CommandBytesPeak;
	uint64 RssPeak, HandlesBefore, HandlesAfter;
	uint32 PeerCommands;
	bool StatsCompiled;
	char Backend[32];
} redisopenresult;
typedef struct redisopenpeer {
	redispeer Peer;
	redispeerhook Hook;
	bool Hold;
} redisopenpeer;

static uint64 redisOpenNow(void)
{ return (uint64)(xrtTimer() * (double)REDIS_OPEN_NS); }

static double redisOpenCpu(void)
{
#if defined(_WIN32)
	FILETIME Created, Exited, Kernel, User; ULARGE_INTEGER K, U;
	redisPeerRequire(GetProcessTimes(GetCurrentProcess(), &Created, &Exited, &Kernel, &User) != 0, "process CPU snapshot");
	K.LowPart = Kernel.dwLowDateTime; K.HighPart = Kernel.dwHighDateTime;
	U.LowPart = User.dwLowDateTime; U.HighPart = User.dwHighDateTime;
	return (double)(K.QuadPart + U.QuadPart) / 10000000.0;
#else
	struct timespec Time; redisPeerRequire(clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &Time) == 0, "process CPU snapshot");
	return (double)Time.tv_sec + (double)Time.tv_nsec / (double)REDIS_OPEN_NS;
#endif
}

static uint64 redisOpenHandles(void)
{
#if defined(_WIN32)
	DWORD Count; redisPeerRequire(GetProcessHandleCount(GetCurrentProcess(), &Count) != 0, "process handle snapshot"); return Count;
#else
	DIR* Directory = opendir("/proc/self/fd"); redisPeerRequire(Directory != NULL, "process fd snapshot");
	uint64 Count = 0; struct dirent* Entry;
	while ((Entry = readdir(Directory)) != NULL) if (Entry->d_name[0] != '.') ++Count;
	closedir(Directory); return Count; /* Includes this scan's own descriptor on both snapshots. */
#endif
}

static uint64 redisOpenRss(void)
{
#if defined(_WIN32)
	PROCESS_MEMORY_COUNTERS Memory; memset(&Memory, 0, sizeof(Memory)); Memory.cb = sizeof(Memory);
	redisPeerRequire(K32GetProcessMemoryInfo(GetCurrentProcess(), &Memory, sizeof(Memory)) != 0, "process memory snapshot"); return Memory.PeakWorkingSetSize;
#else
	struct rusage Usage; redisPeerRequire(getrusage(RUSAGE_SELF, &Usage) == 0, "process memory snapshot"); return (uint64)Usage.ru_maxrss * 1024u;
#endif
}

static void redisOpenNotify(ptr Data)
{ ((redisopenslot*)Data)->Observed = redisOpenNow(); }

static void redisOpenReleased(ptr Data)
{
	/* Last access to reusable slot/watch storage. Publishing in Notify would
	 * race the core's post-notification watch accounting and slot reuse. */
	xrtAtomic32Store(&((redisopenslot*)Data)->Done, 1, XMEMORY_RELEASE);
}

static bool redisOpenPeerCommand(redispeer* Peer, xredisreply* Reply, ptr Data)
{
	redisopenpeer* Context = (redisopenpeer*)Data; xredisreplyvalue Root, Command, Version;
	redisPeerRequire(xredisReplyGet(Reply, 0, &Root) && xredisReplyGet(Reply, Root.FirstChild, &Command), "open-loop command root");
	if (redisPeerName(Command, "HELLO")) {
		redisPeerRequire(Root.ChildCount == 2 && xredisReplyGet(Reply, Command.Next, &Version) && redisPeerName(Version, "3"), "open-loop HELLO");
		const xbytesview Response = XRT_BYTES_LITERAL("%3\r\n+server\r\n+redis\r\n+version\r\n+fixture\r\n+proto\r\n:3\r\n");
		redisPeerSend(Peer, Response.Data, Response.Size); return true;
	}
	if (Context->Hold && redisPeerName(Command, "ECHO")) Peer->Blocked = true;
	return false;
}

static int redisOpenCompare(const void* Left, const void* Right)
{ uint64 A = *(const uint64*)Left, B = *(const uint64*)Right; return (A > B) - (A < B); }

static void redisOpenPercentiles(uint64* Samples, size_t Count, double* Output)
{
	static const unsigned Percentiles[] = {500, 950, 990, 999};
	if (Count != 0) qsort(Samples, Count, sizeof(*Samples), redisOpenCompare);
	for (size_t Index = 0; Index < 4; ++Index) Output[Index] = Count == 0 ? 0.0 : (double)Samples[redisOpenQuantile(Count, Percentiles[Index])] / 1000.0;
}

static void redisOpenObserve(redisopenslot* Slot, const redisopenconfig* Config,
	redisopenrecord* Records, redisopencounts* Counts)
{
	xfutureresult Value; bool Valid = xrtFutureResult(Slot->Future, &Value) && Value.State == XFUTURE_RESOLVED && Value.Value != NULL;
	xredisrequeststatus Status = XREDIS_REQUEST_PROTOCOL;
	if (Valid) {
		const xredisrequestresult* Result = (const xredisrequestresult*)Value.Value; Status = Result->Status;
		if (Status == XREDIS_REQUEST_OK) {
			xredisreplyinfo Info = {0}; Info.Size = sizeof(Info); xredisreplyvalue Root;
			Valid = Result->Delivery == XREDIS_REPLIED && xredisReplyInfo(Result->Reply, &Info) && xredisReplyGet(Result->Reply, Info.Root, &Root) &&
				Root.Type == XREDIS_REPLY_BULK_STRING && !(Root.Flags & XREDIS_REPLY_NULL_VALUE) && Root.Data.Size == Config->Payload;
			for (size_t Byte = 0; Valid && Byte < Config->Payload; ++Byte) Valid = Root.Data.Data[Byte] == redisOpenByte(Slot->Id, Byte);
		}
	}
	redisPeerRequire(redisOpenFinish(&Records[Slot->Id], Slot->Observed, REDIS_OPEN_ACCEPTED,
		Status, Valid, (uint64)Config->Timeout * 1000000u, Counts), "open-loop accepted ledger");
	xrtFutureDestroy(Slot->Future); Slot->Future = NULL; Slot->Active = false;
}

static bool redisOpenRun(const redisopenconfig* Config, redisopenresult* Output)
{
	if (!redisOpenConfigValid(Config) || Output == NULL) return false;
#if !defined(XRT_FEATURE_MEMORY_STATS)
	if (Config->Stats) return false;
#endif
	*Output = (redisopenresult){0}; Output->HandlesBefore = redisOpenHandles();
	redisopenrecord* Records = (redisopenrecord*)calloc(Config->Count, sizeof(*Records));
	redisopenslot* Slots = (redisopenslot*)calloc(Config->Slots, sizeof(*Slots));
	size_t* Free = (size_t*)malloc(Config->Slots * sizeof(*Free));
	unsigned char* Payload = (unsigned char*)malloc(Config->Payload);
	uint64* Samples = (uint64*)malloc(Config->Count * sizeof(*Samples));
	redisPeerRequire(Records != NULL && Slots != NULL && Free != NULL && Payload != NULL && Samples != NULL, "bounded harness storage");
	Output->HarnessBytes = Config->Count * (sizeof(*Records) + sizeof(*Samples)) + Config->Slots * (sizeof(*Slots) + sizeof(*Free)) + Config->Payload;
	for (size_t Index = 0; Index < Config->Slots; ++Index) Free[Index] = Index;
	redisopenpeer Endpoint; memset(&Endpoint, 0, sizeof(Endpoint)); Endpoint.Hold = Config->Mode == 2;
	if (Config->Port == 0) {
		redisPeerStart(&Endpoint.Peer); Endpoint.Hook = (redispeerhook){redisOpenPeerCommand, &Endpoint};
		xrtAtomicPtrStore(&Endpoint.Peer.Hook, &Endpoint.Hook, XMEMORY_RELEASE);
	} else {
		xnetengineconfig Engine; xrtNetEngineConfigInit(&Engine); Engine.Workers = 1;
		Endpoint.Peer.Engine = xrtNetEngineCreate(&Engine);
		redisPeerRequire(Endpoint.Peer.Engine != NULL && xrtNetEngineStart(Endpoint.Peer.Engine) &&
			xrtNetAddrLoopback(&Endpoint.Peer.Address, XNET_FAMILY_IPV4, (uint16)Config->Port), "external loopback engine");
	}
	xnetportbackendinfo Backend; redisPeerRequire(xrtNetEngineBackendInfo(Endpoint.Peer.Engine, 0, &Backend), "open-loop backend");
	snprintf(Output->Backend, sizeof(Output->Backend), "%s", Backend.Name);
	xredisclientconfig ClientConfig; xredisClientConfigInit(&ClientConfig);
	ClientConfig.MaxRequests = Config->Slots; if (ClientConfig.MaxInFlight > Config->Slots) ClientConfig.MaxInFlight = Config->Slots;
	xredisclient* Client = NULL;
	redisPeerRequire(xredisClientConnectInit(Endpoint.Peer.Engine, &Endpoint.Peer.Address, 0, &ClientConfig, NULL, &Client), "open-loop connect");
	xfuture* Ready = xredisClientReady(Client); redisPeerRequire(xrtFutureWaitFor(Ready, 10000) == XWAIT_OK, "open-loop readiness");
	const xredisreadyresult* State = (const xredisreadyresult*)xrtFutureValue(Ready);
	redisPeerRequire(State != NULL && State->Status == XREDIS_READY_OK && State->Protocol == XREDIS_RESP3, "open-loop HELLO outcome"); xrtFutureDestroy(Ready);
	xbytesview Warm = XRT_BYTES_LITERAL("PING"); xfuture* WarmFuture = NULL;
	redisPeerRequire(xredisClientCall(Client, &Warm, 1, 5000, &WarmFuture) == XREDIS_ADMISSION_ACCEPTED &&
		xrtFutureValue(WarmFuture) != NULL &&
		((const xredisrequestresult*)xrtFutureValue(WarmFuture))->Status == XREDIS_REQUEST_OK, "open-loop warmup"); xrtFutureDestroy(WarmFuture);
	xredisclientinfo WarmInfo = {0}; WarmInfo.Size = sizeof(WarmInfo); uint64 WarmLimit = redisOpenNow() + 10u * REDIS_OPEN_NS;
	do {
		redisPeerRequire(xredisClientInfo(Client, &WarmInfo) && redisOpenNow() < WarmLimit, "warm physical request retirement");
		if (WarmInfo.Requests != 0 || WarmInfo.InitBytes != 0) xrtThreadYield();
	} while (WarmInfo.Requests != 0 || WarmInfo.InitBytes != 0);
#if defined(XRT_FEATURE_MEMORY_STATS)
	bool WasEnabled = xrtMemStatsEnabled(); xrtMemStatsEnable(Config->Stats); Output->StatsCompiled = true;
	if (Config->Stats) xrtMemStatsReset();
#endif
	xredisclientinfo Info = {0}; Info.Size = sizeof(Info); uint64 Timeout = (uint64)Config->Timeout * 1000000u;
	double CpuStart = redisOpenCpu(); uint64 Start = redisOpenNow(), Horizon = 0;
	redisPeerRequire(redisOpenDue(Start, Config->Count - 1u, Config->Rate, &Horizon), "arrival horizon");
	redisPeerRequire(Horizon <= UINT64_MAX - Timeout - 10u * REDIS_OPEN_NS, "bounded horizon arithmetic");
	uint64 Limit = Horizon + Timeout + 10u * REDIS_OPEN_NS;
	if (Config->Mode == 1) xrtSleep(Config->Stall);
	size_t Next = 0, Active = 0, Available = Config->Slots;
	while (Next < Config->Count || Active != 0) {
		uint64 Now = redisOpenNow(); redisPeerRequire(Now < Limit, "bounded open-loop drain deadline");
		for (size_t Index = 0; Index < Config->Slots; ++Index) if (Slots[Index].Active && xrtAtomic32Load(&Slots[Index].Done, XMEMORY_ACQUIRE)) {
			redisOpenObserve(&Slots[Index], Config, Records, &Output->Counts); Free[Available++] = Index; --Active;
		}
		for (size_t Work = 0; Next < Config->Count && Work < 128; ++Work) {
			uint64 Due = 0; redisPeerRequire(redisOpenDue(Start, Next, Config->Rate, &Due), "fixed arrival timestamp"); Now = redisOpenNow();
			if (Now < Due) break;
			redisopenrecord* Record = &Records[Next]; Record->Due = Due; Record->Submitted = Now;
			redisopenoutcome Outcome = REDIS_OPEN_PENDING;
			if (Now - Due >= Timeout) Outcome = REDIS_OPEN_EXPIRED;
			else if (Available == 0) Outcome = REDIS_OPEN_CAPACITY;
			else {
				redisopenslot* Slot = &Slots[Free[--Available]]; Slot->Id = Next; Slot->Active = true; Slot->Observed = 0;
				xrtAtomic32Store(&Slot->Done, 0, XMEMORY_RELAXED);
				for (size_t Byte = 0; Byte < Config->Payload; ++Byte) Payload[Byte] = redisOpenByte(Next, Byte);
				xbytesview Args[] = {XRT_BYTES_LITERAL("ECHO"), {Payload, Config->Payload}};
				uint64 At = redisOpenNow(); Record->Submitted = At;
				if (At - Due >= Timeout) { Outcome = REDIS_OPEN_EXPIRED; Slot->Active = false; Free[Available++] = (size_t)(Slot - Slots); }
				else {
					int64 Remaining = (int64)((Timeout - (At - Due)) / 1000000u);
					xredisadmission Admission = xredisClientCallAsync(Client, Args, 2, Remaining, &Slot->Future);
					if (Admission == XREDIS_ADMISSION_ACCEPTED) {
						++Active; redisPeerRequire(xrtFutureWatchInit(&Slot->Watch, redisOpenNotify, redisOpenReleased, Slot), "open-loop watch init");
						xfuturewatchresult Watch = xrtFutureWatchAdd(Slot->Future, &Slot->Watch);
						redisPeerRequire(Watch != XFUTURE_WATCH_ERROR, "open-loop watch registration");
						if (Watch == XFUTURE_WATCH_READY) { redisOpenNotify(Slot); redisOpenReleased(Slot); }
					} else {
						Outcome = Admission == XREDIS_ADMISSION_OVERLOADED ? REDIS_OPEN_OVERLOADED :
							Admission == XREDIS_ADMISSION_CLOSED ? REDIS_OPEN_CLOSED : REDIS_OPEN_ERROR;
						Slot->Active = false; Free[Available++] = (size_t)(Slot - Slots);
					}
				}
			}
			if (Outcome != REDIS_OPEN_PENDING)
				redisPeerRequire(redisOpenFinish(Record, redisOpenNow(), Outcome, XREDIS_REQUEST_OK, true, Timeout, &Output->Counts), "open-loop rejected ledger");
			++Next;
		}
		xredisClientInfo(Client, &Info);
		if (Info.Requests > Output->RequestsPeak) Output->RequestsPeak = Info.Requests;
		if (Info.CommandBytes > Output->CommandBytesPeak) Output->CommandBytesPeak = Info.CommandBytes;
		xrtThreadYield();
	}
	uint64 LogicalEnd = redisOpenNow(); Output->Elapsed = (double)(LogicalEnd - Start) / (double)REDIS_OPEN_NS;
	xfuture* Closed = xredisClientClosed(Client);
	redisPeerRequire(xredisClientClose(Client) && xrtFutureWaitFor(Closed, 10000) == XWAIT_OK, "open-loop physical close");
	xredisClientInfo(Client, &Info); redisPeerRequire(Info.Requests == 0 && Info.CommandBytes == 0 && Info.InitBytes == 0 && Info.Accepted == Info.Completed, "open-loop physical ownership");
	xredisClientRelease(Client); xrtFutureDestroy(Closed);
	if (Config->Port == 0) { redisPeerStop(&Endpoint.Peer); Output->PeerCommands = xrtAtomic32Load(&Endpoint.Peer.Commands, XMEMORY_ACQUIRE); }
	else redisPeerRequire(xrtNetEngineStop(Endpoint.Peer.Engine) && xrtNetEngineDestroy(Endpoint.Peer.Engine), "external engine retirement");
	Output->Retire = (double)(redisOpenNow() - LogicalEnd) / (double)REDIS_OPEN_NS; Output->Cpu = redisOpenCpu() - CpuStart;
#if defined(XRT_FEATURE_MEMORY_STATS)
	if (Config->Stats) {
		xmemstats Stats; xrtMemStatsGet(&Stats); Output->Malloc = Stats.MallocCalls; Output->Calloc = Stats.CallocCalls;
		Output->Realloc = Stats.ReallocCalls; Output->MemDup = Stats.MemDupCalls; Output->BlockAlloc = Stats.BlockAllocCalls;
		Output->BackingAlloc = Stats.BackingAllocCalls; Output->BackingBytes = Stats.BackingAllocBytes;
	}
	xrtMemStatsEnable(WasEnabled);
#endif
	redisPeerRequire(Output->Counts.Total == Config->Count && Output->Counts.Accepted + Output->Counts.Outcomes[REDIS_OPEN_CAPACITY] +
		Output->Counts.Outcomes[REDIS_OPEN_OVERLOADED] + Output->Counts.Outcomes[REDIS_OPEN_CLOSED] + Output->Counts.Outcomes[REDIS_OPEN_ERROR] +
		Output->Counts.Outcomes[REDIS_OPEN_EXPIRED] == Config->Count, "open-loop no omitted offers");
	for (size_t Index = 0; Index < Config->Count; ++Index) { redisPeerRequire(Records[Index].Finished != 0, "missing scheduled offer"); Samples[Index] = Records[Index].Finished - Records[Index].Due; }
	redisOpenPercentiles(Samples, Config->Count, Output->All);
	for (size_t Index = 0; Index < Config->Count; ++Index) Samples[Index] = Records[Index].Submitted - Records[Index].Due;
	redisOpenPercentiles(Samples, Config->Count, Output->Lag);
	size_t Successes = 0;
	for (size_t Index = 0; Index < Config->Count; ++Index) if (Records[Index].Outcome == REDIS_OPEN_ACCEPTED && Records[Index].Valid && Records[Index].Status == XREDIS_REQUEST_OK)
		Samples[Successes++] = Records[Index].Finished - Records[Index].Due;
	redisOpenPercentiles(Samples, Successes, Output->Success);
	Output->RssPeak = redisOpenRss(); free(Samples); free(Payload); free(Free); free(Slots); free(Records);
	Output->HandlesAfter = redisOpenHandles(); return Output->Counts.Corrupt == 0 && (Config->Mode != 0 || Output->Counts.Successful != 0);
}
#endif
