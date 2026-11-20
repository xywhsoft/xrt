#ifndef XREDIS_BENCH_OPENLOOP_RUN_H
#define XREDIS_BENCH_OPENLOOP_RUN_H
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#if defined(_WIN32) && defined(REDIS_OPENLOOP_HIREDIS)
#include <winsock2.h>
#endif
#include "openloop_schedule.h"
#if !defined(REDIS_OPENLOOP_HIREDIS)
#include "../tests/support/client_peer.h"
#endif
#if defined(REDIS_OPENLOOP_TLS)
#include "../tests/support/tls_peer.h"
#endif
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#else
#include <dirent.h>
#include <sys/resource.h>
#include <time.h>
#endif

typedef struct redisopenresult {
	redisopencounts Counts;
	double Elapsed, Retire, Cpu, GeneratorCpu;
	double All[4], Success[4], Lag[4], Response[4];
	uint64 Malloc, Calloc, Realloc, MemDup, BlockAlloc, BackingAlloc, BackingBytes;
	size_t HarnessBytes, RequestsPeak, CommandBytesPeak, WindowPeak;
	uint64 RssPeak, HandlesBefore, HandlesAfter;
	size_t AcceptedGets, AcceptedSets, SuccessfulGets;
	uint32 PeerCommands;
	bool StatsCompiled;
	char Backend[32];
} redisopenresult;

static void redisOpenRequire(bool Condition, cstr Message)
{
	if (!Condition) {
		fprintf(stderr, "open-loop: %s\n", Message);
		exit(EXIT_FAILURE);
	}
}

static uint64 redisOpenNow(void)
{ return (uint64)(xrtTimer() * (double)REDIS_OPEN_NS); }

static double redisOpenCpu(bool Generator)
{
#if defined(_WIN32)
	FILETIME Created, Exited, Kernel, User; ULARGE_INTEGER K, U;
	BOOL Ok = Generator ? GetThreadTimes(GetCurrentThread(), &Created, &Exited, &Kernel, &User) :
		GetProcessTimes(GetCurrentProcess(), &Created, &Exited, &Kernel, &User);
	redisOpenRequire(Ok != 0, "CPU snapshot");
	K.LowPart = Kernel.dwLowDateTime; K.HighPart = Kernel.dwHighDateTime;
	U.LowPart = User.dwLowDateTime; U.HighPart = User.dwHighDateTime;
	return (double)(K.QuadPart + U.QuadPart) / 10000000.0;
#else
	struct timespec Time;
	redisOpenRequire(clock_gettime(Generator ? CLOCK_THREAD_CPUTIME_ID : CLOCK_PROCESS_CPUTIME_ID, &Time) == 0, "CPU snapshot");
	return (double)Time.tv_sec + (double)Time.tv_nsec / (double)REDIS_OPEN_NS;
#endif
}

static uint64 redisOpenHandles(void)
{
#if defined(_WIN32)
	DWORD Count; redisOpenRequire(GetProcessHandleCount(GetCurrentProcess(), &Count) != 0, "process handle snapshot"); return Count;
#else
	DIR* Directory = opendir("/proc/self/fd"); redisOpenRequire(Directory != NULL, "process fd snapshot");
	uint64 Count = 0; struct dirent* Entry;
	while ((Entry = readdir(Directory)) != NULL) if (Entry->d_name[0] != '.') ++Count;
	closedir(Directory); return Count; /* Includes this scan's own descriptor on both snapshots. */
#endif
}

static uint64 redisOpenRss(void)
{
#if defined(_WIN32)
	PROCESS_MEMORY_COUNTERS Memory; memset(&Memory, 0, sizeof(Memory)); Memory.cb = sizeof(Memory);
	redisOpenRequire(K32GetProcessMemoryInfo(GetCurrentProcess(), &Memory, sizeof(Memory)) != 0, "process memory snapshot"); return Memory.PeakWorkingSetSize;
#else
	struct rusage Usage; redisOpenRequire(getrusage(RUSAGE_SELF, &Usage) == 0, "process memory snapshot"); return (uint64)Usage.ru_maxrss * 1024u;
#endif
}

static int redisOpenCompare(const void* Left, const void* Right)
{ uint64 A = *(const uint64*)Left, B = *(const uint64*)Right; return (A > B) - (A < B); }

static void redisOpenPercentiles(uint64* Samples, size_t Count, double* Output)
{
	static const unsigned Percentiles[] = {500, 950, 990, 999};
	if (Count != 0) qsort(Samples, Count, sizeof(*Samples), redisOpenCompare);
	for (size_t Index = 0; Index < 4; ++Index) Output[Index] = Count == 0 ? 0.0 : (double)Samples[redisOpenQuantile(Count, Percentiles[Index])] / 1000.0;
}

static const xbytesview RedisOpenReadKey = XRT_BYTES_LITERAL("xrt:reference:\0read");
static const xbytesview RedisOpenWriteKey = XRT_BYTES_LITERAL("xrt:reference:\0write");

static void redisOpenPayload(unsigned char* Payload, size_t Size, size_t Id)
{
	for (size_t Byte = 0; Byte < Size; ++Byte) Payload[Byte] = redisOpenByte(Id, Byte);
}

static size_t redisOpenCommand(const redisopenconfig* Config, size_t Id,
	unsigned char* Payload, xbytesview Args[3])
{
	if (Config->Mixed && !redisOpenWrite(Config, Id)) {
		Args[0] = (xbytesview)XRT_BYTES_LITERAL("GET");
		Args[1] = RedisOpenReadKey;
		return 2;
	}
	redisOpenPayload(Payload, Config->Payload, Id);
	xbytesview Value = {Payload, Config->Payload};
	if (Config->Mixed) {
		Args[0] = (xbytesview)XRT_BYTES_LITERAL("SET");
		Args[1] = RedisOpenWriteKey;
		Args[2] = Value;
		return 3;
	}
	Args[0] = (xbytesview)XRT_BYTES_LITERAL("ECHO");
	Args[1] = Value;
	return 2;
}

static bool redisOpenBinary(xbytesview Reply, size_t Size, size_t Id)
{
	if (Reply.Size != Size) return false;
	for (size_t Byte = 0; Byte < Size; ++Byte)
		if (Reply.Data[Byte] != redisOpenByte(Id, Byte)) return false;
	return true;
}

static void redisOpenSummarize(const redisopenconfig* Config, redisopenresult* Output,
	const redisopenrecord* Records, uint64* Samples)
{
	redisOpenRequire(Output->Counts.Total == Config->Count, "open-loop no omitted offers");
	for (size_t Index = 0; Index < Config->Count; ++Index) {
		redisOpenRequire(Records[Index].Finished != 0, "missing scheduled offer");
		Samples[Index] = Records[Index].Finished - Records[Index].Due;
	}
	redisOpenPercentiles(Samples, Config->Count, Output->All);
	for (size_t Index = 0; Index < Config->Count; ++Index) Samples[Index] = Records[Index].Submitted - Records[Index].Due;
	redisOpenPercentiles(Samples, Config->Count, Output->Lag);
	size_t Successes = 0;
	for (size_t Index = 0; Index < Config->Count; ++Index) if (Records[Index].Outcome == REDIS_OPEN_ACCEPTED) {
		bool Set = redisOpenWrite(Config, Index);
		if (Config->Mixed) {
			if (Set) ++Output->AcceptedSets;
			else ++Output->AcceptedGets;
		}
		if (Records[Index].Valid && Records[Index].Status == XREDIS_REQUEST_OK) {
			if (Config->Mixed && !Set) ++Output->SuccessfulGets;
			Samples[Successes++] = Records[Index].Finished - Records[Index].Due;
		}
	}
	redisOpenRequire(Successes == Output->Counts.Successful, "open-loop success ledger");
	redisOpenPercentiles(Samples, Successes, Output->Success);
	Successes = 0;
	for (size_t Index = 0; Index < Config->Count; ++Index) {
		const redisopenrecord* Record = &Records[Index];
		if (Record->Outcome == REDIS_OPEN_ACCEPTED && Record->Valid && Record->Status == XREDIS_REQUEST_OK)
			Samples[Successes++] = Record->Finished - Record->Submitted;
	}
	redisOpenPercentiles(Samples, Successes, Output->Response);
	Output->RssPeak = redisOpenRss();
}

#if defined(REDIS_OPENLOOP_HIREDIS)
#include "openloop_hiredis.h"
#else
typedef struct redisopenslot {
	xfuture* Future;
	xfuturewatch Watch;
	xatomic32 Done;
	uint64 Observed;
	size_t Id;
	bool Active;
} redisopenslot;
typedef struct redisopenpeer {
	redispeer Peer;
	redispeerhook Hook;
	bool Hold;
} redisopenpeer;

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

static void redisOpenObserve(redisopenslot* Slot, const redisopenconfig* Config,
	redisopenrecord* Records, redisopencounts* Counts)
{
	xfutureresult Value; bool Valid = xrtFutureResult(Slot->Future, &Value) && Value.State == XFUTURE_RESOLVED && Value.Value != NULL;
	xredisrequeststatus Status = XREDIS_REQUEST_PROTOCOL;
	if (Valid) {
		const xredisrequestresult* Result = (const xredisrequestresult*)Value.Value; Status = Result->Status;
		if (Status == XREDIS_REQUEST_OK) {
			xredisreplyinfo Info = {0}; Info.Size = sizeof(Info); xredisreplyvalue Root;
			Valid = Result->Delivery == XREDIS_REPLIED && xredisReplyInfo(Result->Reply, &Info) && xredisReplyGet(Result->Reply, Info.Root, &Root);
			if (Valid && redisOpenWrite(Config, Slot->Id))
				Valid = Root.Type == XREDIS_REPLY_SIMPLE_STRING && Root.Data.Size == 2 && memcmp(Root.Data.Data, "OK", 2) == 0;
			else if (Valid)
				Valid = Root.Type == XREDIS_REPLY_BULK_STRING && !(Root.Flags & XREDIS_REPLY_NULL_VALUE) &&
					redisOpenBinary(Root.Data, Config->Payload, Config->Mixed ? 0 : Slot->Id);
		}
	}
	redisPeerRequire(redisOpenFinish(&Records[Slot->Id], Slot->Observed, REDIS_OPEN_ACCEPTED,
		Status, Valid, (uint64)Config->Timeout * 1000000u, Counts), "open-loop accepted ledger");
	xrtFutureDestroy(Slot->Future); Slot->Future = NULL; Slot->Active = false;
}

static void redisOpenWarmCommand(xredisclient* Client, const xbytesview* Args, size_t Count, xbytesview Expected)
{
	xfuture* Future = NULL;
	redisPeerRequire(xredisClientCall(Client, Args, Count, 5000, &Future) == XREDIS_ADMISSION_ACCEPTED,
		"open-loop warmup admission");
	const xredisrequestresult* Result = xrtFutureValue(Future);
	xredisreplyinfo Info = {.Size = sizeof(Info)};
	xredisreplyvalue Value;
	redisPeerRequire(Result && Result->Status == XREDIS_REQUEST_OK && Result->Delivery == XREDIS_REPLIED &&
		xredisReplyInfo(Result->Reply, &Info) && xredisReplyGet(Result->Reply, Info.Root, &Value) &&
		Value.Type == XREDIS_REPLY_SIMPLE_STRING && Value.Data.Size == Expected.Size &&
		memcmp(Value.Data.Data, Expected.Data, Expected.Size) == 0, "open-loop warmup reply");
	xrtFutureDestroy(Future);
}

static bool redisOpenRun(const redisopenconfig* Config, redisopenresult* Output)
{
	if (!redisOpenConfigValid(Config) || Output == NULL) return false;
#if defined(REDIS_OPENLOOP_TLS)
	if (Config->Port != 0) return false; /* External TLS requires explicit trust; never fall back to plaintext. */
#endif
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
	const unsigned Connections = redisOpenConnections(Config);
	xredisclient* Clients[REDIS_OPEN_MAX_CONNECTIONS] = {0};
#if defined(REDIS_OPENLOOP_TLS)
	redistlspeer Credentials; memset(&Credentials, 0, sizeof(Credentials)); redisTlsPeerCredentials(&Credentials, &Endpoint.Peer, false, false);
	xredisclienttlsconfig TLS; redisTlsPeerConfig(&Credentials, &TLS);
#endif

	for (unsigned Index = 0; Index < Connections; ++Index) {
		xredisclient** Client = &Clients[Index];
#if defined(REDIS_OPENLOOP_TLS)
		redisPeerRequire(xredisClientTlsConnect(Endpoint.Peer.Engine, &Endpoint.Peer.Address, 0, &ClientConfig, NULL, &TLS, Client), "open-loop TLS connect");
#else
		redisPeerRequire(xredisClientConnectInit(Endpoint.Peer.Engine, &Endpoint.Peer.Address, 0, &ClientConfig, NULL, Client), "open-loop connect");
#endif
		xfuture* Ready = xredisClientReady(*Client); redisPeerRequire(xrtFutureWaitFor(Ready, 10000) == XWAIT_OK, "open-loop readiness");
		const xredisreadyresult* State = (const xredisreadyresult*)xrtFutureValue(Ready);
		redisPeerRequire(State != NULL && State->Status == XREDIS_READY_OK && State->Protocol == XREDIS_RESP3, "open-loop HELLO outcome");
		xrtFutureDestroy(Ready);
		xbytesview Warm = XRT_BYTES_LITERAL("PING");
		redisOpenWarmCommand(*Client, &Warm, 1, (xbytesview)XRT_BYTES_LITERAL("PONG"));
		if (Config->Mixed && Index == 0) {
			redisOpenPayload(Payload, Config->Payload, 0);
			xbytesview Seed[] = {XRT_BYTES_LITERAL("SET"), RedisOpenReadKey, {Payload, Config->Payload}};
			redisOpenWarmCommand(*Client, Seed, 3, (xbytesview)XRT_BYTES_LITERAL("OK"));
		}
		xredisclientinfo WarmInfo = {.Size = sizeof(WarmInfo)};
		uint64 WarmLimit = redisOpenNow() + 10u * REDIS_OPEN_NS;
		do {
			redisPeerRequire(xredisClientInfo(*Client, &WarmInfo) && redisOpenNow() < WarmLimit, "warm physical request retirement");
			if (WarmInfo.Requests || WarmInfo.InitBytes) xrtThreadYield();
		} while (WarmInfo.Requests || WarmInfo.InitBytes);
	}
#if defined(XRT_FEATURE_MEMORY_STATS)
	bool WasEnabled = xrtMemStatsEnabled(); xrtMemStatsEnable(Config->Stats); Output->StatsCompiled = true;
	if (Config->Stats) xrtMemStatsReset();
#endif
	xredisclientinfo Info = {0}; Info.Size = sizeof(Info); uint64 Timeout = (uint64)Config->Timeout * 1000000u;
	double CpuStart = redisOpenCpu(false), GeneratorStart = redisOpenCpu(true);
	uint64 Start = redisOpenNow(), Horizon = 0;
	redisPeerRequire(redisOpenDue(Start, Config->Count - 1u, Config->Rate, &Horizon), "arrival horizon");
	redisPeerRequire(Horizon <= UINT64_MAX - Timeout - 10u * REDIS_OPEN_NS, "bounded horizon arithmetic");
	uint64 Limit = Horizon + Timeout + 10u * REDIS_OPEN_NS;
	if (Config->Mode == 1) xrtSleep(Config->Stall);
	size_t Next = 0, Active = 0, Available = Config->Slots;
	uint64 SampleAt = Start;
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
				xbytesview Args[3];
				size_t ArgCount = redisOpenCommand(Config, Next, Payload, Args);
				uint64 At = redisOpenNow(); Record->Submitted = At;
				if (At - Due >= Timeout) { Outcome = REDIS_OPEN_EXPIRED; Slot->Active = false; Free[Available++] = (size_t)(Slot - Slots); }
				else {
					int64 Remaining = (int64)((Timeout - (At - Due)) / 1000000u);
					xredisadmission Admission = xredisClientCallAsync(Clients[Next % Connections], Args, ArgCount, Remaining, &Slot->Future);
					if (Admission == XREDIS_ADMISSION_ACCEPTED) {
						if (++Active > Output->WindowPeak) Output->WindowPeak = Active;
						redisPeerRequire(xrtFutureWatchInit(&Slot->Watch, redisOpenNotify, redisOpenReleased, Slot), "open-loop watch init");
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
		/* Each snapshot takes the same gate as the worker. Bound probe traffic
		 * independently of the unconditional offer and completion ledgers. */
		if (Now >= SampleAt) {
			SampleAt = Now + 1000000u;
			size_t Requests = 0, Bytes = 0;
			for (unsigned Index = 0; Index < Connections; ++Index) {
				redisPeerRequire(xredisClientInfo(Clients[Index], &Info), "open-loop client counters");
				Requests += Info.Requests;
				Bytes += Info.CommandBytes;
			}
			if (Requests > Output->RequestsPeak) Output->RequestsPeak = Requests;
			if (Bytes > Output->CommandBytesPeak) Output->CommandBytesPeak = Bytes;
		}
		xrtThreadYield();
	}
	uint64 LogicalEnd = redisOpenNow(); Output->Elapsed = (double)(LogicalEnd - Start) / (double)REDIS_OPEN_NS;
	for (unsigned Index = 0; Index < Connections; ++Index) {
		xredisclient* Client = Clients[Index];
		xfuture* Closed = xredisClientClosed(Client);
		redisPeerRequire(xredisClientClose(Client) && xrtFutureWaitFor(Closed, 10000) == XWAIT_OK, "open-loop physical close");
		redisPeerRequire(xredisClientInfo(Client, &Info) && Info.Requests == 0 && Info.CommandBytes == 0 &&
			Info.InitBytes == 0 && Info.Accepted == Info.Completed, "open-loop physical ownership");
		xredisClientRelease(Client);
		xrtFutureDestroy(Closed);
	}
	if (Config->Port == 0) { redisPeerStop(&Endpoint.Peer); Output->PeerCommands = xrtAtomic32Load(&Endpoint.Peer.Commands, XMEMORY_ACQUIRE); }
	else redisPeerRequire(xrtNetEngineStop(Endpoint.Peer.Engine) && xrtNetEngineDestroy(Endpoint.Peer.Engine), "external engine retirement");
#if defined(REDIS_OPENLOOP_TLS)
	redisTlsPeerCredentialsDrop(&Credentials);
#endif
	Output->Retire = (double)(redisOpenNow() - LogicalEnd) / (double)REDIS_OPEN_NS;
	Output->GeneratorCpu = redisOpenCpu(true) - GeneratorStart;
	Output->Cpu = redisOpenCpu(false) - CpuStart;
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
	redisOpenSummarize(Config, Output, Records, Samples);
	free(Samples); free(Payload); free(Free); free(Slots); free(Records);
	Output->HandlesAfter = redisOpenHandles(); return Output->Counts.Corrupt == 0 && (Config->Mode != 0 || Output->Counts.Successful != 0);
}
#endif
#endif
