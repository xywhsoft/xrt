/* One continuous mixed workload against a private, empty loopback Redis. */
#include <xredis.h>
#include "../../bench/openloop_schedule.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

enum { MEMBERS = 2, BATCH = 8, PAYLOAD = 1024, DEADLINE_MS = 2000, TICK_SECONDS = 5, SAMPLES = 8192 };
static const xbytesview Channel = XRT_BYTES_LITERAL("xrt:soak:\0channel");

static void require(bool Ok, const char* Message)
{
	if (!Ok) {
		fprintf(stderr, "Redis mixed: %s\n", Message);
		exit(EXIT_FAILURE);
	}
}

#include "tls_probe.h"

typedef struct mixedendpoint {
	xnetaddr Address;
	redisprobetls Tls;
} mixedendpoint;

static uint64 argument(const char* Text, uint64 Min, uint64 Max)
{
	char* End;
	errno = 0;
	unsigned long long Value = strtoull(Text, &End, 10);
	if (*Text < '0' || *Text > '9' || *End || errno || Value < Min || Value > Max) exit(2);
	return (uint64)Value;
}

static const void* waitResult(xfuture* Future)
{
	require(Future != NULL && xrtFutureWaitFor(Future, DEADLINE_MS + 1000) == XWAIT_OK, "Future deadline");
	const void* Value = xrtFutureValue(Future);
	require(Value != NULL, "missing Future value");
	return Value;
}

static xredisreplyvalue resultRoot(const xredisrequestresult* Result)
{
	if (Result->Status != XREDIS_REQUEST_OK || Result->Delivery != XREDIS_REPLIED)
		fprintf(stderr, "request status=%d delivery=%d\n", (int)Result->Status, (int)Result->Delivery);
	require(Result->Status == XREDIS_REQUEST_OK && Result->Delivery == XREDIS_REPLIED, "request failed");
	xredisreplyinfo Info = {.Size = sizeof(Info)};
	xredisreplyvalue Root;
	require(xredisReplyInfo(Result->Reply, &Info) && xredisReplyGet(Result->Reply, Info.Root, &Root), "missing reply root");
	return Root;
}

static bool same(xbytesview Left, xbytesview Right)
{ return Left.Size == Right.Size && memcmp(Left.Data, Right.Data, Right.Size) == 0; }

static void verifyValue(xredisreplyvalue Root, xbytesview Expected)
{
	require(Root.Type == XREDIS_REPLY_BULK_STRING && !(Root.Flags & XREDIS_REPLY_NULL_VALUE) &&
		same(Root.Data, Expected), "binary GET mismatch");
}

static void verifySet(xredisreplyvalue Root)
{
	const xbytesview Ok = XRT_BYTES_LITERAL("OK");
	require(Root.Type == XREDIS_REPLY_SIMPLE_STRING && same(Root.Data, Ok), "SET ACK");
}

static bool connectMember(xnetengine* Engine, size_t Slot, uint64 Attempt, int64 Timeout, ptr Data, xredisclient** Out)
{
	(void)Attempt;
	const mixedendpoint* Endpoint = Data;
	xredisclientinitconfig Init;
	if (!xredisClientInitConfigInit(&Init)) return false;
	Init.Timeout = Timeout;
	return redisProbeConnect(&Endpoint->Tls, Engine, &Endpoint->Address, Slot, &Init, Out);
}

static xredisclient* connectDedicated(xnetengine* Engine, const mixedendpoint* Endpoint, uint64 Affinity)
{
	xredisclient* Client = NULL;
	require(redisProbeConnect(&Endpoint->Tls, Engine, &Endpoint->Address, Affinity, NULL, &Client),
		"dedicated client connect");
	xfuture* Ready = xredisClientReady(Client);
	const xredisreadyresult* Result = waitResult(Ready);
	require(Result->Status == XREDIS_READY_OK && Result->Protocol == XREDIS_RESP3, "dedicated HELLO 3");
	xrtFutureDestroy(Ready);
	return Client;
}

static void closeClient(xredisclient* Client)
{
	xfuture* Closed = xredisClientClosed(Client);
	require(Closed && xredisClientClose(Client) && xrtFutureWaitFor(Closed, DEADLINE_MS + 1000) == XWAIT_OK,
		"dedicated physical close");
	xredisclientinfo Info = {.Size = sizeof(Info)};
	require(xredisClientInfo(Client, &Info) && Info.State == XREDIS_CLIENT_CLOSED && !Info.Requests &&
		!Info.CommandBytes && !Info.InitBytes && !Info.Pushes && !Info.PushBytes && Info.Accepted == Info.Completed,
		"dedicated final request and push accounting");
	xrtFutureDestroy(Closed);
	xredisClientRelease(Client);
}

static void batch(xredispool* Pool, const xbytesview* Keys, const xbytesview* Values, bool Write)
{
	xfuture* Futures[BATCH] = {0};
	const xbytesview Set = XRT_BYTES_LITERAL("SET"), Get = XRT_BYTES_LITERAL("GET");
	for (size_t Index = 0; Index < BATCH; ++Index) {
		xbytesview Args[] = {Write ? Set : Get, Keys[Index], Values[Index]};
		require(xredisSharedPoolCallAsync(Pool, Args, Write ? 3 : 2, DEADLINE_MS, &Futures[Index]) ==
			XREDIS_ADMISSION_ACCEPTED, "batch admission");
	}
	for (size_t Index = 0; Index < BATCH; ++Index) {
		xredisreplyvalue Root = resultRoot(waitResult(Futures[Index]));
		if (Write) verifySet(Root);
		else verifyValue(Root, Values[Index]);
		xrtFutureDestroy(Futures[Index]);
	}
}

static void trackingRound(xredispool* Pool, xredisclient* Tracking, xbytesview Key, xbytesview Old, xbytesview New)
{
	xbytesview Read[] = {XRT_BYTES_LITERAL("GET"), Key};
	xfuture* Future = NULL;
	require(xredisClientTrackingCall(Tracking, Read, 2, DEADLINE_MS, &Future) == XREDIS_ADMISSION_ACCEPTED, "tracking GET admission");
	const xredistrackingresult* Observed = waitResult(Future);
	verifyValue(resultRoot(&Observed->Request), Old);
	uint64 Sequence = Observed->Sequence;
	require(Sequence != 0, "missing observed receive sequence");
	xrtFutureDestroy(Future);

	xbytesview Write[] = {XRT_BYTES_LITERAL("SET"), Key, New};
	Future = NULL;
	require(xredisSharedPoolCall(Pool, Write, 3, DEADLINE_MS, &Future) == XREDIS_ADMISSION_ACCEPTED, "tracking SET admission");
	verifySet(resultRoot(waitResult(Future)));
	xrtFutureDestroy(Future);

	xredistrackingpush Push;
	double Limit = xrtTimer() + (double)DEADLINE_MS / 1000.0;
	while (!xredisClientTrackingTake(Tracking, &Push)) {
		require(xrtTimer() < Limit, "invalidation deadline");
		xrtThreadYield();
	}
	xredistrackingevent Event;
	xredisreplyvalue Item;
	require(Push.Sequence > Sequence && xredisTrackingEventRead(Push.Reply, &Event) == XREDIS_TRACKING_EVENT &&
		Event.Kind == XREDIS_TRACKING_KEYS && Event.Keys == 1 &&
		xredisReplyGet(Push.Reply, Event.FirstKey, &Item) && same(Item.Data, Key), "invalidation identity and ordering");
	xredisReplyRelease(Push.Reply);
}

static void publication(xredispool* Pool, xredisclient* Subscriber, xbytesview Payload)
{
	xfuture* Notification = NULL;
	require(xredisSubscriberReadable(Subscriber, &Notification) == XREDIS_ADMISSION_ACCEPTED &&
		Notification != NULL && !xrtFutureDone(Notification), "empty subscriber queue before publish");
	xbytesview Args[] = {XRT_BYTES_LITERAL("PUBLISH"), Channel, Payload};
	xfuture* Future = NULL;
	require(xredisSharedPoolCall(Pool, Args, 3, DEADLINE_MS, &Future) == XREDIS_ADMISSION_ACCEPTED, "publish admission");
	xredisreplyvalue Root = resultRoot(waitResult(Future));
	require(Root.Type == XREDIS_REPLY_INTEGER && Root.Integer == 1, "one live recipient");
	xrtFutureDestroy(Future);
	require(xrtFutureWaitFor(Notification, DEADLINE_MS) == XWAIT_OK, "subscriber notification deadline");
	xrtFutureDestroy(Notification);

	xredisreply* Message = NULL;
	xredispubsubevent Event;
	require(xredisClientTakePush(Subscriber, &Message) && xredisPubSubEventRead(Message, &Event) == XREDIS_PUBSUB_EVENT &&
		Event.Kind == XREDIS_PUBSUB_MESSAGE && same(Event.Channel, Channel) && same(Event.Payload, Payload), "binary publication identity");
	xredisReplyRelease(Message);
}

static int compareSample(const void* Left, const void* Right)
{
	double A = *(const double*)Left, B = *(const double*)Right;
	return (A > B) - (A < B);
}

static double cpuSeconds(void)
{
#if defined(_WIN32)
	FILETIME Created, Exited, Kernel, User;
	require(GetProcessTimes(GetCurrentProcess(), &Created, &Exited, &Kernel, &User) != 0, "process CPU clock");
	uint64 Work = ((uint64)Kernel.dwHighDateTime << 32) + Kernel.dwLowDateTime;
	Work += ((uint64)User.dwHighDateTime << 32) + User.dwLowDateTime;
	return (double)Work / 10000000.0;
#else
	struct timespec Work;
	require(clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &Work) == 0, "process CPU clock");
	return (double)Work.tv_sec + (double)Work.tv_nsec / 1000000000.0;
#endif
}

static void tick(uint64 Rounds, double Elapsed, double Cpu, double* Latency, size_t Count)
{
	require(Count != 0, "no mixed workload progress");
	qsort(Latency, Count, sizeof(*Latency), compareSample);
	printf("{\"rounds\":%" PRIu64 ",\"seconds\":%.6f,\"cpu_seconds\":%.6f,\"samples\":%zu,"
		"\"round_p50_us\":%.3f,\"round_p95_us\":%.3f,\"round_p99_us\":%.3f,\"round_p999_us\":%.3f}\n",
		Rounds, Elapsed, Cpu, Count, Latency[redisOpenQuantile(Count, 500)], Latency[redisOpenQuantile(Count, 950)],
		Latency[redisOpenQuantile(Count, 990)], Latency[redisOpenQuantile(Count, 999)]);
	fflush(stdout);
}

int main(int Argc, char** Argv)
{
	if (Argc != 4 && Argc != 5) return 2;
	uint16 Port = (uint16)argument(Argv[1], 1, UINT16_MAX);
	double Duration = (double)argument(Argv[2], 1, 72u * 3600u + 3600u);
	int64 Period = (int64)argument(Argv[3], 2, 100);
	mixedendpoint Endpoint = {0};
	require(xrtNetAddrLoopback(&Endpoint.Address, XNET_FAMILY_IPV4, Port), "loopback address");
	redisProbeTlsInit(&Endpoint.Tls, Argc == 5 ? Argv[4] : NULL);
	xnetengineconfig EngineConfig;
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 2;
	xnetengine* Engine = xrtNetEngineCreate(&EngineConfig);
	require(Engine && xrtNetEngineStart(Engine), "start engine");

	xredispoolconnectconfig Connect;
	require(xredisPoolConnectConfigInit(&Connect), "pool factory defaults");
	Connect.Connect = connectMember;
	Connect.Data = &Endpoint;
	xredispool* Pool = NULL;
	require(xredisSharedPoolConnect(Engine, 0, MEMBERS, NULL, &Connect, &Pool) == XREDIS_ADMISSION_ACCEPTED, "start automatic shared pool");
	xredispoolinfo PoolInfo = {.Size = sizeof(PoolInfo)};
	double SetupLimit = xrtTimer() + 5;
	do {
		require(xredisPoolInfo(Pool, &PoolInfo) && xrtTimer() < SetupLimit, "all shared members ready");
		if (PoolInfo.Idle < MEMBERS) xrtSleep(1);
	} while (PoolInfo.Idle < MEMBERS);

	xredisclient* Tracking = connectDedicated(Engine, &Endpoint, 0);
	xredisclient* Subscriber = connectDedicated(Engine, &Endpoint, 1);
	xredistrackingconfig TrackingConfig;
	require(xredisTrackingConfigInit(&TrackingConfig) && xredisClientTrackingObserve(Tracking, 0) == XREDIS_ADMISSION_ACCEPTED,
		"install tracking observer");
	TrackingConfig.Enabled = true;
	xfuture* Setup = NULL;
	require(xredisClientTrackingConfigureAsync(Tracking, &TrackingConfig, DEADLINE_MS, &Setup) ==
		XREDIS_ADMISSION_ACCEPTED, "enable tracking");
	const xredistrackingresult* Tracked = waitResult(Setup);
	verifySet(resultRoot(&Tracked->Request));
	xrtFutureDestroy(Setup);
	Setup = NULL;
	require(xredisSubscriberStart(Subscriber, NULL) == XREDIS_ADMISSION_ACCEPTED &&
		xredisSubscriberCommand(Subscriber, XREDIS_PUBSUB_CMD_SUBSCRIBE, &Channel, 1, DEADLINE_MS, &Setup) ==
			XREDIS_ADMISSION_ACCEPTED, "subscribe");
	const xredissubscriberresult* Subscribed = waitResult(Setup);
	require(Subscribed->Request.Status == XREDIS_REQUEST_OK && Subscribed->Request.Delivery == XREDIS_REPLIED && Subscribed->Acknowledged == 1 &&
		Subscribed->Subscriptions == 1, "subscription confirmed");
	xrtFutureDestroy(Setup);

	unsigned char KeyData[BATCH][16], Data[BATCH][PAYLOAD], Updated[PAYLOAD];
	xbytesview Keys[BATCH], Values[BATCH];
	for (size_t Index = 0; Index < BATCH; ++Index) {
		memcpy(KeyData[Index], "xrt:soak:\0", 10);
		KeyData[Index][10] = (unsigned char)Index;
		Keys[Index] = (xbytesview){KeyData[Index], 11};
	}
	double Latency[SAMPLES], CpuStart = cpuSeconds(), Start = xrtTimer(), Last = Start, Now = Start;
	uint64 Rounds = 0;
	size_t Count = 0;
	while (Now - Start < Duration) {
		double RoundStart = xrtTimer();
		size_t Bytes = Rounds % 4 == 0 ? PAYLOAD : 32;
		size_t Track = (size_t)(Rounds % BATCH);
		for (size_t Index = 0; Index < BATCH; ++Index) {
			for (size_t Byte = 0; Byte < Bytes; ++Byte)
				Data[Index][Byte] = Byte == 0 ? (unsigned char)Index : redisOpenByte((size_t)Rounds, Byte - 1);
			Values[Index] = (xbytesview){Data[Index], Bytes};
		}
		memcpy(Updated, Data[Track], Bytes);
		Updated[Bytes - 1] ^= 0xff;
		xbytesview Value = {Updated, Bytes};
		batch(Pool, Keys, Values, true);
		batch(Pool, Keys, Values, false);
		trackingRound(Pool, Tracking, Keys[Track], Values[Track], Value);
		publication(Pool, Subscriber, Value);
		++Rounds;
		Now = xrtTimer();
		require(Count < SAMPLES, "bounded latency window");
		Latency[Count++] = (Now - RoundStart) * 1000000.0;
		if (Now - Last >= TICK_SECONDS || Now - Start >= Duration) {
			tick(Rounds, Now - Start, cpuSeconds() - CpuStart, Latency, Count);
			Last = Now;
			Count = 0;
		}
		if (Now - Start < Duration) xrtSleep(Period);
		Now = xrtTimer();
	}
	if (Count) tick(Rounds, Now - Start, cpuSeconds() - CpuStart, Latency, Count);

	xredissubscriberinfo SubInfo = {.Size = sizeof(SubInfo)};
	xredistrackinginfo TrackingInfo = {.Size = sizeof(TrackingInfo)};
	xredisclientinfo ClientInfo = {.Size = sizeof(ClientInfo)};
	require(xredisSubscriberInfo(Subscriber, &SubInfo) && SubInfo.Messages == Rounds &&
		xredisClientInfo(Subscriber, &ClientInfo) && !ClientInfo.Pushes &&
		xredisClientInfo(Tracking, &ClientInfo) && !ClientInfo.Pushes &&
		xredisClientTrackingInfo(Tracking, &TrackingInfo) && !TrackingInfo.Rejected, "final live delivery accounting");
	xredispoolconnectinfo ConnectInfo = {.Size = sizeof(ConnectInfo)};
	require(xredisPoolConnectInfo(Pool, &ConnectInfo) && ConnectInfo.Connected == MEMBERS, "unchanged pool connection generations");
	closeClient(Subscriber);
	closeClient(Tracking);
	xfuture* Closed = xredisPoolClosed(Pool);
	require(Closed && xredisPoolClose(Pool) && xrtFutureWaitFor(Closed, DEADLINE_MS + 1000) == XWAIT_OK &&
		xredisPoolInfo(Pool, &PoolInfo) && PoolInfo.State == XREDIS_POOL_CLOSED && !PoolInfo.Live &&
		!PoolInfo.Requests && !PoolInfo.CommandBytes, "pool physical close and resource ledger");
	xrtFutureDestroy(Closed);
	xredisPoolRelease(Pool);
	require(xrtNetEngineStop(Engine) && xrtNetEngineDestroy(Engine), "engine worker retirement");
	redisProbeTlsClose(&Endpoint.Tls);
	printf("{\"complete\":true,\"rounds\":%" PRIu64 ",\"seconds\":%.6f}\n", Rounds, Now - Start);
	return 0;
}
