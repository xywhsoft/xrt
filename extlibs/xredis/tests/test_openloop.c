#include "../bench/openloop_run.h"
#include "test.h"
#include <math.h>
#if defined(__linux__)
#include <sys/wait.h>
#include <unistd.h>
#endif

static void testOpenSchedule(void)
{
	uint64 Due = 0;
	testRequire(redisOpenDue(17, 0, 3, &Due) && Due == 17, "first planned arrival");
	testRequire(redisOpenDue(17, 1, 3, &Due) && Due == 333333350, "rational planned arrival");
	testRequire(redisOpenDue(17, 3, 3, &Due) && Due == 1000000017, "arrival schedule drift");
	for (size_t Index = 0; Index < 10000; ++Index) {
		testRequire(redisOpenDue(0, Index, 7000, &Due) && Due == (uint64)Index * REDIS_OPEN_NS / 7000, "absolute arrival oracle");
	}
	Due = 91;
	testRequire(!redisOpenDue(UINT64_MAX, 1, 1, &Due) && Due == 91 && !redisOpenDue(0, 1, 0, &Due) && Due == 91, "arrival failure output/overflow");
	testRequire(redisOpenQuantile(16, 999) == 15 && redisOpenQuantile(2, 500) == 0 && redisOpenQuantile(1, 500) == 0 &&
		redisOpenQuantile(1000, 999) == 998 && redisOpenQuantile(SIZE_MAX, 1000) == SIZE_MAX - 1u &&
		redisOpenQuantile(0, 500) == SIZE_MAX, "nearest-rank percentiles and tiny-sample maximum");
}

static void testOpenLedger(void)
{
	redisopencounts Counts = {0};
	for (size_t Index = 0; Index < 10000; ++Index) {
		redisopenrecord Record = {1000, 2000, 0, REDIS_OPEN_PENDING, XREDIS_REQUEST_OK, false};
		redisopenoutcome Outcome = Index % 2u == 0 ? REDIS_OPEN_ACCEPTED : REDIS_OPEN_CAPACITY;
		testRequire(redisOpenFinish(&Record, 5000, Outcome, XREDIS_REQUEST_OK, true, 3000, &Counts), "complete planned offer");
		redisopencounts Snapshot = Counts;
		testRequire(!redisOpenFinish(&Record, 6000, Outcome, XREDIS_REQUEST_OK, true, 3000, &Counts) &&
			memcmp(&Counts, &Snapshot, sizeof(Counts)) == 0, "double completion changed accounting");
	}
	testRequire(Counts.Total == 10000 && Counts.Accepted == 5000 && Counts.Successful == 5000 && Counts.Missed == 10000 &&
		Counts.Outcomes[REDIS_OPEN_CAPACITY] == 5000 && Counts.Status[XREDIS_REQUEST_OK] == 5000, "rejections omitted or lateness shifted");
	redisopenrecord Bad = {1000, 1100, 0, REDIS_OPEN_PENDING, XREDIS_REQUEST_OK, false}; redisopencounts Snapshot = Counts;
	testRequire(!redisOpenFinish(&Bad, 1050, REDIS_OPEN_ACCEPTED, XREDIS_REQUEST_OK, true, 3000, &Counts) && Bad.Finished == 0 &&
		memcmp(&Counts, &Snapshot, sizeof(Counts)) == 0, "invalid time accepted/mutated ledger");
	redisopenrecord Equal = {1000, 1000, 0, REDIS_OPEN_PENDING, XREDIS_REQUEST_OK, false};
	testRequire(redisOpenFinish(&Equal, 2000, REDIS_OPEN_ACCEPTED, XREDIS_REQUEST_OK, true, 1000, &Counts) && Counts.Missed == 10001,
		"intended deadline equality must expire");
	const unsigned char Prefix[] = {0xff, 0x0d, 0x0a, 0, 0, 0, 0, 0};
	for (size_t Index = 0; Index < sizeof(Prefix); ++Index) testRequire(redisOpenByte(0x000a0dff, Index) == Prefix[Index], "binary correlation ID corpus");
	testRequire(redisOpenByte(0, 8) == 24 && redisOpenByte(1, 8) == 41 && redisOpenByte(256, 8) == 25, "data-dependent tail corpus");
}

static void testOpenResponse(void)
{
	redisopenconfig Config = redisOpenDefaults(); Config.Count = 3;
	redisopenresult Result = {0};
	redisopenrecord Records[] = {{.Due = 1000, .Submitted = 4000},
		{.Due = 2000, .Submitted = 3000}, {.Due = 5000, .Submitted = 6000}};
	uint64 Samples[3];
	testRequire(redisOpenFinish(&Records[0], 8000, REDIS_OPEN_ACCEPTED, XREDIS_REQUEST_OK, true, 20000, &Result.Counts) &&
		redisOpenFinish(&Records[1], 5000, REDIS_OPEN_ACCEPTED, XREDIS_REQUEST_OK, true, 20000, &Result.Counts) &&
		redisOpenFinish(&Records[2], 16000, REDIS_OPEN_CAPACITY, XREDIS_REQUEST_OK, true, 20000, &Result.Counts),
		"response intervals test ledger");
	redisOpenSummarize(&Config, &Result, Records, Samples);
	testRequire(Result.Success[0] == 3 && Result.Success[2] == 7 && Result.All[2] == 11 &&
		Result.Response[0] == 2 && Result.Response[2] == 4, "response percentiles must use successful submission timestamps");
}

static void testOpenInvalid(void)
{
	redisopenconfig Default = redisOpenDefaults(); testRequire(redisOpenConfigValid(&Default), "default open-loop config");
	for (size_t Trial = 0; Trial < 17; ++Trial) {
		redisopenconfig Bad = Default;
		if (Trial == 0) Bad.Count = 0;
		if (Trial == 1) Bad.Count = REDIS_OPEN_MAX_COUNT + 1u;
		if (Trial == 2) Bad.Rate = 0;
		if (Trial == 3) Bad.Rate = REDIS_OPEN_NS + 1u;
		if (Trial == 4) Bad.Slots = 0;
		if (Trial == 5) Bad.Slots = REDIS_OPEN_MAX_SLOTS + 1u;
		if (Trial == 6) Bad.Payload = 7;
		if (Trial == 7) Bad.Payload = REDIS_OPEN_MAX_PAYLOAD + 1u;
		if (Trial == 8) Bad.Timeout = 0;
		if (Trial == 9) Bad.Timeout = 10001;
		if (Trial == 10) Bad.Mode = 3;
		if (Trial == 11) Bad.Stall = 1;
		if (Trial == 12) Bad.Port = UINT16_MAX + 1u;
		if (Trial == 13) Bad.Rate = 1;
		if (Trial == 14) Bad.Connections = REDIS_OPEN_MAX_CONNECTIONS + 1u;
		if (Trial == 15) Bad.Connections = 2;
		if (Trial == 16) Bad.Mixed = true;
		testRequire(!redisOpenConfigValid(&Bad), "invalid open-loop config accepted");
	}
}

static void testOpenMixed(void)
{
	redisopenconfig Config = redisOpenDefaults();
	Config.Port = 1;
	Config.Connections = 4;
	Config.Mixed = true;
	testRequire(redisOpenConfigValid(&Config), "valid external mixed workload");
	unsigned Gets[4] = {0}, Sets[4] = {0};
	unsigned char Payload[32];
	for (size_t Index = 0; Index < 32; ++Index) {
		xbytesview Args[3];
		size_t Count = redisOpenCommand(&Config, Index, Payload, Args);
		testRequire(Count == 2 || Count == 3, "mixed command arity");
		testRequire(Args[0].Size == 3 && Args[1].Size > 0 &&
			memchr(Args[1].Data, 0, Args[1].Size) != NULL, "binary key and command name");
		if (Count == 3) {
			testRequire(memcmp(Args[0].Data, "SET", 3) == 0 && Args[1].Data == RedisOpenWriteKey.Data &&
				redisOpenBinary(Args[2], sizeof(Payload), Index), "SET uses its own key and carries the offer payload");
			++Sets[Index % Config.Connections];
		} else {
			testRequire(memcmp(Args[0].Data, "GET", 3) == 0 && Args[1].Data == RedisOpenReadKey.Data,
				"GET reads the separate seed key");
			++Gets[Index % Config.Connections];
		}
	}
	for (size_t Index = 0; Index < 4; ++Index)
		testRequire(Gets[Index] == 6 && Sets[Index] == 2, "mix ratio per connection");
}

static void testOpenRuntime(void)
{
	redisopenconfig Config = redisOpenDefaults(); Config.Count = 32; Config.Rate = 2000; Config.Slots = 32; Config.Timeout = 5000;
	redisopenresult Result; testRequire(redisOpenRun(&Config, &Result), "open-loop normal runtime");
	testRequire(Result.Counts.Total == 32 && Result.Counts.Accepted == 32 && Result.Counts.Successful == 32 &&
		Result.Counts.Corrupt == 0 && Result.HarnessBytes != 0 && Result.PeerCommands == 34, "normal offer ledger/correlation");
	for (size_t Index = 0; Index < 4; ++Index)
		testRequire(Result.Response[Index] > 0 && Result.Response[Index] <= Result.Success[Index], "response interval exceeds offer latency");
	double GeneratorCpu = Result.GeneratorCpu.User + Result.GeneratorCpu.Kernel;
	double ProcessCpu = Result.Cpu.User + Result.Cpu.Kernel;
	testRequire(Result.GeneratorCpu.User >= 0 && Result.GeneratorCpu.Kernel >= 0 &&
		Result.Cpu.User >= 0 && Result.Cpu.Kernel >= 0 && isfinite(GeneratorCpu) &&
		isfinite(ProcessCpu) && GeneratorCpu <= ProcessCpu + 0.000001,
		"generator CPU outside the process window");
	Config.Count = 16; Config.Rate = 1000000; Config.Mode = 1; Config.Stall = 50; Config.Timeout = 10;
	testRequire(redisOpenRun(&Config, &Result) && Result.Counts.Outcomes[REDIS_OPEN_EXPIRED] == 16 && Result.Counts.Accepted == 0 &&
		Result.Counts.Missed == 16 && Result.Lag[0] >= 10000.0 && Result.Response[3] == 0 &&
		Result.PeerCommands == 2, "generator stall silently omitted/shifted offers");
	Config.Count = 32; Config.Rate = REDIS_OPEN_NS; Config.Slots = 4; Config.Mode = 2; Config.Stall = 0; Config.Timeout = 1000;
	testRequire(redisOpenRun(&Config, &Result) && Result.Counts.Total == 32 && Result.Counts.Accepted != 0 && Result.Counts.Successful == 0 &&
		Result.Counts.Status[XREDIS_REQUEST_TIMEOUT] == Result.Counts.Accepted && Result.Counts.Outcomes[REDIS_OPEN_CAPACITY] != 0,
		"held server failed to account bounded overload/timeout");
#if defined(XRT_FEATURE_MEMORY_STATS)
	Config = redisOpenDefaults(); Config.Count = 16; Config.Rate = 2000; Config.Slots = 16; Config.Timeout = 5000; Config.Stats = true;
	testRequire(redisOpenRun(&Config, &Result) && Result.Counts.Successful == 16 && Result.StatsCompiled && Result.Malloc != 0 && Result.BlockAlloc != 0,
		"instrumented process window failed to observe actual allocation work");
#endif
}

#if defined(__linux__)
static void testOpenRssAfterExec(void)
{
	const size_t Bytes = 128u * 1024u * 1024u;
	const long PageSize = sysconf(_SC_PAGESIZE);
	testRequire(PageSize > 0, "process memory page size");
	unsigned char* Allocation = malloc(Bytes);
	testRequire(Allocation != NULL, "allocate launcher pages");
	volatile unsigned char* Pages = Allocation;
	for (size_t Index = 0; Index < Bytes; Index += (size_t)PageSize)
		Pages[Index] = 1;
	pid_t Child = fork();
	testRequire(Child >= 0, "fork RSS launcher");
	if (Child == 0) {
		execl("/proc/self/exe", "test_openloop", "--check-rss-after-exec", (char*)NULL);
		_exit(127);
	}
	int Status = 0;
	testRequire(waitpid(Child, &Status, 0) == Child, "wait for RSS child");
	free(Allocation);
	testRequire(WIFEXITED(Status) && WEXITSTATUS(Status) == 0, "benchmark RSS included launcher pages");
}
#endif

int main(int argc, char** argv)
{
#if defined(__linux__)
	if (argc == 2 && strcmp(argv[1], "--check-rss-after-exec") == 0) {
		const uint64 Peak = redisOpenRss();
		return Peak == 0 || Peak >= 64u * 1024u * 1024u;
	}
	testOpenRssAfterExec();
#else
	(void)argc;
	(void)argv;
#endif
	testOpenSchedule(); testOpenLedger(); testOpenResponse(); testOpenInvalid(); testOpenMixed(); testOpenRuntime();
	puts("[PASS] xredis open-loop workload: absolute offers/no omission/all outcomes/binary/deadlines/overload/watch retirement/process counters"); return 0;
}
