#include "test.h"
#include "support/client_peer.h"

static xredisclient* testClientConnect(redispeer* Peer, const xredisclientconfig* Config)
{
	xredisclient* Client = NULL;
	testRequire(xredisClientConnect(Peer->Engine, &Peer->Address, 0, Config, &Client), "client connect");
	Peer->Client = Client; return Client;
}

static const xredisrequestresult* testClientResult(xfuture* Future)
{
	testRequire(xrtFutureWaitFor(Future, 10000) == XWAIT_OK, "client result wait");
	xfutureresult Result;
	testRequire(xrtFutureResult(Future, &Result) && Result.State == XFUTURE_RESOLVED && Result.Value != NULL,
		"accepted request must have typed immutable outcome"); return (const xredisrequestresult*)Result.Value;
}

static xfuture* testClientSubmit(xredisclient* Client, cstr Name, cstr Number, int64 Timeout)
{
	xbytesview Arguments[2] = {{(cbytes)Name, strlen(Name)}, {(cbytes)Number, Number == NULL ? 0 : strlen(Number)}};
	xfuture* Future = NULL;
	testRequire(xredisClientCallAsync(Client, Arguments, Number == NULL ? 1 : 2, Timeout, &Future) == XREDIS_ADMISSION_ACCEPTED,
		"request admission"); return Future;
}

static void testClientClose(redispeer* Peer, xredisclient* Client)
{
	xfuture* Closed = xredisClientClosed(Client); testRequire(Closed != NULL && xredisClientClose(Client), "client close request");
	testRequire(xrtFutureWaitFor(Closed, 10000) == XWAIT_OK, "client physical close wait");
	xredisclientinfo Info = {0}; Info.Size = sizeof(Info);
	testRequire(xredisClientInfo(Client, &Info) && Info.State == XREDIS_CLIENT_CLOSED && Info.Requests == 0 && Info.CommandBytes == 0 &&
		Info.Accepted == Info.Completed, "client physical obligations not retired");
	testRequire(xredisClientClose(Client), "client idempotent close");
	xredisClientRelease(Client); xrtFutureDestroy(Closed); redisPeerStop(Peer);
}

static void testClientBasic(void)
{
	redispeer Peer; redisPeerStart(&Peer); xredisclientconfig Config; xredisClientConfigInit(&Config);
	Config.ReadBytes = 3; Config.ReadFrames = 1; Config.WorkRequests = 2; Config.MaxInFlight = 8;
	xredisclient* Client = testClientConnect(&Peer, &Config);
	xfuture* Ping = testClientSubmit(Client, "PING", NULL, 5000);
	const xredisrequestresult* Result = testClientResult(Ping);
	testRequire(Result->Status == XREDIS_REQUEST_OK && Result->Delivery == XREDIS_REPLIED && Result->Cause == NULL, "PING status");
	xredisreplyvalue Value; testRequire(xredisReplyGet(Result->Reply, 0, &Value) && Value.Data.Size == 4 && memcmp(Value.Data.Data, "PONG", 4) == 0,
		"PING response");
	xfuture* Futures[64];
	for (size_t Index = 0; Index < 64; ++Index) {
		char Text[32]; snprintf(Text, sizeof(Text), "%zu", Index); Futures[Index] = testClientSubmit(Client, "ID", Text, 5000); memset(Text, 0x5a, sizeof(Text));
	}
	for (size_t Index = 0; Index < 64; ++Index) {
		Result = testClientResult(Futures[Index]);
		testRequire(Result->Status == XREDIS_REQUEST_OK && xredisReplyGet(Result->Reply, 0, &Value) && Value.Integer == (int64)Index,
			"pipeline correlation"); xrtFutureDestroy(Futures[Index]);
	}
	unsigned char Payload[] = {'a', 0, 'b', '\r', '\n', 0xff};
	xbytesview Echo[] = {XRT_BYTES_LITERAL("ECHO"), {Payload, sizeof(Payload)}}; xfuture* Future = NULL;
	testRequire(xredisClientCall(Client, Echo, 2, 5000, &Future) == XREDIS_ADMISSION_ACCEPTED, "sync echo"); memset(Payload, 0, sizeof(Payload));
	Result = testClientResult(Future);
	testRequire(Result->Status == XREDIS_REQUEST_OK && xredisReplyGet(Result->Reply, 0, &Value) &&
		Value.Data.Size == 6 && memcmp(Value.Data.Data, "a\0b\r\n\xff", 6) == 0, "binary owned echo"); xrtFutureDestroy(Future);
	Future = testClientSubmit(Client, "ERROR", NULL, 5000); Result = testClientResult(Future);
	testRequire(Result->Status == XREDIS_REQUEST_SERVER_ERROR && Result->Delivery == XREDIS_REPLIED && Result->Reply != NULL, "server error distinct");
	xrtFutureDestroy(Future);
	Future = testClientSubmit(Client, "PUSH", NULL, 5000); Result = testClientResult(Future);
	testRequire(Result->Status == XREDIS_REQUEST_OK && xredisReplyGet(Result->Reply, 0, &Value) && Value.Data.Size == 5 &&
		memcmp(Value.Data.Data, "after", 5) == 0, "push shifted ordinary FIFO"); xrtFutureDestroy(Future);
	xredisreply* Push = NULL; testRequire(xredisClientTakePush(Client, &Push), "push queue empty");
	xredisreplyinfo Info = {0}; Info.Size = sizeof(Info); xredisReplyInfo(Push, &Info); xredisReplyGet(Push, Info.Root, &Value);
	testRequire(Value.Type == XREDIS_REPLY_PUSH && Value.Attributes == 0 && Value.ChildCount == 2, "push metadata");
	xredisReplyRelease(Push); Push = (xredisreply*)(uintptr_t)1;
	testRequire(!xredisClientTakePush(Client, &Push) && Push == (xredisreply*)(uintptr_t)1, "empty push mutated output");
	/* Retained results are independent from all network and engine lifetimes. */
	testClientClose(&Peer, Client); Result = testClientResult(Ping);
	testRequire(xredisReplyGet(Result->Reply, 0, &Value) && memcmp(Value.Data.Data, "PONG", 4) == 0, "result retained engine/client");
	xrtFutureDestroy(Ping);
}

static void testClientTimeoutCancel(void)
{
	redispeer Peer; redisPeerStart(&Peer); xredisclientconfig Config; xredisClientConfigInit(&Config);
	Config.MaxInFlight = 2; Config.DrainTimeout = 1000;
	xredisclient* Client = testClientConnect(&Peer, &Config);
	xfuture* Hold = testClientSubmit(Client, "HOLD", NULL, 200); redisPeerWait(&Peer.Commands, 1);
	xfuture* Sent = testClientSubmit(Client, "ID", "11", 5000); redisPeerWait(&Peer.Commands, 2);
	xfuture* Queued = testClientSubmit(Client, "ID", "22", 5000); testRequire(xrtFutureCancel(Queued), "cancel queued");
	const xredisrequestresult* Result = testClientResult(Queued);
	testRequire(Result->Status == XREDIS_REQUEST_CANCELLED && Result->Delivery == XREDIS_NOT_SENT, "queued cancel sent bytes");
	Result = testClientResult(Hold);
	testRequire(Result->Status == XREDIS_REQUEST_TIMEOUT && Result->Delivery == XREDIS_MAY_HAVE_EXECUTED, "sent timeout certainty");
	testRequire(xrtFutureCancel(Sent), "cancel sent"); Result = testClientResult(Sent);
	testRequire(Result->Status == XREDIS_REQUEST_CANCELLED && Result->Delivery == XREDIS_MAY_HAVE_EXECUTED, "sent cancel certainty");
	redisPeerReleaseBlocked(&Peer);
	xfuture* Following = testClientSubmit(Client, "ID", "33", 5000); Result = testClientResult(Following);
	xredisreplyvalue Value; testRequire(Result->Status == XREDIS_REQUEST_OK && xredisReplyGet(Result->Reply, 0, &Value) && Value.Integer == 33,
		"late replies shifted next request");
	xredisclientinfo Info = {0}; Info.Size = sizeof(Info); xredisClientInfo(Client, &Info);
	testRequire(Info.DiscardedReplies == 2 && xrtAtomic32Load(&Peer.Commands, XMEMORY_ACQUIRE) == 3, "tombstones/cancelled unsent count");
	xrtFutureDestroy(Hold); xrtFutureDestroy(Sent); xrtFutureDestroy(Queued); xrtFutureDestroy(Following);
	testClientClose(&Peer, Client);
}

static void testClientFailures(void)
{
	const cstr Names[] = {"MALFORMED", "PARTIAL", "DROP", "FLOOD"};
	for (size_t Trial = 0; Trial < 4; ++Trial) {
		redispeer Peer; redisPeerStart(&Peer); xredisclientconfig Config; xredisClientConfigInit(&Config);
		Config.MaxPushes = 1; Config.ReadBytes = 2; xredisclient* Client = testClientConnect(&Peer, &Config);
		xfuture* Future = testClientSubmit(Client, Names[Trial], NULL, 5000);
		const xredisrequestresult* Result = testClientResult(Future);
		testRequire(Result->Status == (Trial < 2 ? XREDIS_REQUEST_PROTOCOL : Trial == 2 ? XREDIS_REQUEST_DISCONNECTED : XREDIS_REQUEST_LIMIT) &&
			Result->Delivery == XREDIS_MAY_HAVE_EXECUTED && Result->Reply == NULL, "connection failure cause/certainty");
		testClientClose(&Peer, Client); xrtFutureDestroy(Future);
	}
	redispeer Peer; redisPeerStart(&Peer); xredisclientconfig Config; xredisClientConfigInit(&Config); Config.DrainTimeout = 100;
	xredisclient* Client = testClientConnect(&Peer, &Config); xfuture* Hold = testClientSubmit(Client, "HOLD", NULL, 200);
	redisPeerWait(&Peer.Commands, 1);
	xfuture* Next = testClientSubmit(Client, "PING", NULL, 5000); redisPeerWait(&Peer.Commands, 2);
	const xredisrequestresult* Result = testClientResult(Hold); testRequire(Result->Status == XREDIS_REQUEST_TIMEOUT, "grace original timeout");
	Result = testClientResult(Next);
	testRequire(Result->Status == XREDIS_REQUEST_DISCONNECTED && Result->Delivery == XREDIS_MAY_HAVE_EXECUTED,
		"another request's grace misreported this request's deadline");
	xrtFutureDestroy(Hold); xrtFutureDestroy(Next); testClientClose(&Peer, Client);
}

static void testClientAdmission(void)
{
	redispeer Peer; redisPeerStart(&Peer); xredisclientconfig Config; xredisClientConfigInit(&Config);
	Config.MaxRequests = 1; Config.MaxInFlight = 1; Config.DrainTimeout = 1000;
	xredisclient* Client = testClientConnect(&Peer, &Config);
	xfuture* Hold = testClientSubmit(Client, "HOLD", NULL, 200); redisPeerWait(&Peer.Commands, 1);
	xbytesview Ping = XRT_BYTES_LITERAL("PING"); xfuture* Output = (xfuture*)(uintptr_t)1;
	xrtSetErrorKind(XERR_EXISTS); const xerror* Previous = xrtGetError();
	testRequire(xredisClientCallAsync(Client, &Ping, 1, 1000, &Output) == XREDIS_ADMISSION_OVERLOADED &&
		Output == (xfuture*)(uintptr_t)1 && xrtGetError() == Previous, "count admission overwrote output/error");
	const xredisrequestresult* Result = testClientResult(Hold); testRequire(Result->Status == XREDIS_REQUEST_TIMEOUT, "admission hold timeout");
	testRequire(xredisClientCallAsync(Client, &Ping, 1, 1000, &Output) == XREDIS_ADMISSION_OVERLOADED,
		"abandoned wire slot did not count toward admission");
	redisPeerReleaseBlocked(&Peer);
	double End = xrtTimer() + 5.0; xredisclientinfo Info = {0}; Info.Size = sizeof(Info);
	do { xredisClientInfo(Client, &Info); testRequire(xrtTimer() < End, "admission retirement wait"); xrtThreadYield(); } while (Info.Requests != 0);
	xfuture* Zero = testClientSubmit(Client, "PING", NULL, 0); Result = testClientResult(Zero);
	testRequire(Result->Status == XREDIS_REQUEST_TIMEOUT && Result->Delivery == XREDIS_NOT_SENT &&
		xrtAtomic32Load(&Peer.Commands, XMEMORY_ACQUIRE) == 1, "zero deadline sent request"); xrtFutureDestroy(Zero);
	xrtFutureDestroy(Hold); xrtClearError(); testClientClose(&Peer, Client);
}

typedef struct testClientStall { xatomic32 Entered, Release; } testClientStall;

static void testClientStallWorker(xnetworker* Worker, ptr Data)
{
	(void)Worker; testClientStall* Stall = (testClientStall*)Data;
	xrtAtomic32Store(&Stall->Entered, 1, XMEMORY_RELEASE); double End = xrtTimer() + 10.0;
	while (!xrtAtomic32Load(&Stall->Release, XMEMORY_ACQUIRE)) {
		testRequire(xrtTimer() < End, "injected worker stall not released"); xrtThreadYield();
	}
}

static void testClientQueueDeadline(void)
{
	redispeer Peer; redisPeerStart(&Peer); testClientStall Stall = {0};
	testRequire(xrtNetEnginePost(Peer.Engine, 0, testClientStallWorker, &Stall), "stall injection"); redisPeerWait(&Stall.Entered, 1);
	xredisclient* Client = testClientConnect(&Peer, NULL); xfuture* Future = testClientSubmit(Client, "PING", NULL, 20);
	double End = xrtTimer() + 0.06; while (xrtTimer() < End) xrtThreadYield();
	xrtAtomic32Store(&Stall.Release, 1, XMEMORY_RELEASE);
	const xredisrequestresult* Result = testClientResult(Future);
	testRequire(Result->Status == XREDIS_REQUEST_TIMEOUT && Result->Delivery == XREDIS_NOT_SENT &&
		xrtAtomic32Load(&Peer.Commands, XMEMORY_ACQUIRE) == 0, "setup/worker queue excluded from deadline");
	xrtFutureDestroy(Future); testClientClose(&Peer, Client);
}

static void testClientSyncWorker(xnetworker* Worker, ptr Data)
{
	(void)Worker; redispeer* Peer = (redispeer*)Data; xbytesview Ping = XRT_BYTES_LITERAL("PING"); xfuture* Output = (xfuture*)(uintptr_t)1;
	testRequire(xredisClientCall(Peer->Client, &Ping, 1, 1000, &Output) == XREDIS_ADMISSION_ERROR &&
		Output == (xfuture*)(uintptr_t)1 && xrtErrorCode(xrtGetError()) == XREDIS_CLIENT_STATE, "sync facade allowed worker blocking");
	xrtClearError(); (void)xrtAtomic32FetchAdd(&Peer->SyncChecked, 1, XMEMORY_RELEASE);
}

static void testClientInvalid(void)
{
	redispeer Peer; redisPeerStart(&Peer); xredisclient* Client = testClientConnect(&Peer, NULL);
	xbytesview Ping = XRT_BYTES_LITERAL("PING"); xfuture* Output = (xfuture*)(uintptr_t)1;
	testRequire(xredisClientCallAsync(Client, &Ping, 1, -2, &Output) == XREDIS_ADMISSION_ERROR && Output == (xfuture*)(uintptr_t)1,
		"invalid timeout mutation");
	testRequire(xredisClientCallAsync(Client, &Ping, 1, 1000, (xfuture**)Client) == XREDIS_ADMISSION_ERROR, "client output alias");
	union {xbytesview Args[2]; xfuture* Future;} Alias; Alias.Args[0] = Ping;
	testRequire(xredisClientCallAsync(Client, Alias.Args, 1, 1000, &Alias.Future) == XREDIS_ADMISSION_ERROR, "argument/output alias");
	testRequire(xredisClientCall(Client, &Ping, 1, 1000, NULL) == XREDIS_ADMISSION_ERROR, "sync NULL output");
	const cstr Names[] = {"subscribe", "PSUBSCRIBE", "SSUBSCRIBE", "UNSUBSCRIBE", "PUNSUBSCRIBE", "SUNSUBSCRIBE", "MONITOR", "SYNC", "PSYNC"};
	for (size_t Index = 0; Index < sizeof(Names) / sizeof(Names[0]); ++Index) {
		xbytesview Argument = {(cbytes)Names[Index], strlen(Names[Index])};
		testRequire(xredisClientCallAsync(Client, &Argument, 1, 1000, &Output) == XREDIS_ADMISSION_ERROR &&
			xrtErrorCode(xrtGetError()) == XREDIS_CLIENT_COMMAND_MODE, "cardinality-changing mode accepted");
	}
	xbytesview ReplyOff[] = {XRT_BYTES_LITERAL("CLIENT"), XRT_BYTES_LITERAL("REPLY"), XRT_BYTES_LITERAL("OFF")};
	testRequire(xredisClientCallAsync(Client, ReplyOff, 3, 1000, &Output) == XREDIS_ADMISSION_ERROR, "CLIENT REPLY accepted");
	for (size_t Worker = 0; Worker < 2; ++Worker)
		testRequire(xrtNetEnginePost(Peer.Engine, Worker, testClientSyncWorker, &Peer), "sync guard probe post");
	redisPeerWait(&Peer.SyncChecked, 2); xrtClearError();
	xrtSetErrorKind(XERR_EXISTS); const xerror* Previous = xrtGetError();
	xfuture* Future = testClientSubmit(Client, "PING", NULL, 5000);
	testRequire(xrtGetError() == Previous && testClientResult(Future)->Status == XREDIS_REQUEST_OK, "valid admission changed caller error");
	xrtFutureDestroy(Future); xrtClearError();
	testRequire(xredisClientClose(Client), "invalid fixture close");
	testRequire(xredisClientCallAsync(Client, &Ping, 1, 1000, &Output) == XREDIS_ADMISSION_CLOSED && Output == (xfuture*)(uintptr_t)1,
		"closing admission output"); testClientClose(&Peer, Client);
}

typedef struct testClientProducer {xredisclient* Client; size_t Base;} testClientProducer;

static int32 testClientProduce(ptr Data)
{
	testClientProducer* Context = (testClientProducer*)Data;
	for (size_t Batch = 0; Batch < 12; ++Batch) {
		xfuture* Futures[8];
		for (size_t Index = 0; Index < 8; ++Index) {
			char Number[32]; snprintf(Number, sizeof(Number), "%zu", Context->Base + Batch * 8u + Index);
			Futures[Index] = testClientSubmit(Context->Client, "ID", Number, 5000);
			if ((Index % 3u) == 0) (void)xrtFutureCancel(Futures[Index]);
		}
		for (size_t Index = 0; Index < 8; ++Index) {
			const xredisrequestresult* Result = testClientResult(Futures[Index]);
			testRequire(Result->Status == XREDIS_REQUEST_OK || Result->Status == XREDIS_REQUEST_CANCELLED, "concurrent terminal status");
			if (Result->Status == XREDIS_REQUEST_OK) {
				xredisreplyvalue Value; testRequire(xredisReplyGet(Result->Reply, 0, &Value) &&
					Value.Integer == (int64)(Context->Base + Batch * 8u + Index), "concurrent response cross-talk");
			}
			xrtFutureDestroy(Futures[Index]);
		}
	}
	xredisClientRelease(Context->Client); return 0;
}

static void testClientThreads(void)
{
	redispeer Peer; redisPeerStart(&Peer); xredisclient* Client = testClientConnect(&Peer, NULL);
	testClientProducer Producers[4]; xthread* Threads[4];
	for (size_t Index = 0; Index < 4; ++Index) {
		Producers[Index] = (testClientProducer){Client, Index * 1000u}; testRequire(xredisClientRetain(Client), "producer client ref");
		Threads[Index] = xrtThreadCreate(testClientProduce, &Producers[Index], 0); testRequire(Threads[Index] != NULL, "producer thread");
	}
	for (size_t Index = 0; Index < 4; ++Index) {
		testRequire(xrtThreadWait(Threads[Index]) == XWAIT_OK && xrtThreadExitCode(Threads[Index]) == 0, "producer join"); xrtThreadDestroy(Threads[Index]);
	}
	testClientClose(&Peer, Client);
}

static void testClientAllocations(void)
{
	size_t Failed = 0, Created = 0;
	for (size_t Limit = 1; Limit < 18000; Limit += 509) {
		redispeer Peer; redisPeerStart(&Peer);
		xallocationbudget* Budget = NULL; xallocationscope Scope = {0};
		testRequire(xrtAllocationBudgetCreate(Limit, &Budget) && xrtAllocationBudgetEnter(Budget, &Scope), "client create quota");
		xredisclient* Client = (xredisclient*)(uintptr_t)1;
		bool Success = xredisClientConnect(Peer.Engine, &Peer.Address, 0, NULL, &Client);
		if (Success) ++Created;
		else { ++Failed; testRequire(Client == (xredisclient*)(uintptr_t)1, "failed create changed ownership"); }
		xrtClearError(); testRequire(xrtAllocationBudgetLeave(&Scope), "client quota leave");
		if (Success) testClientClose(&Peer, Client); else redisPeerStop(&Peer);
		xallocationbudgetinfo Info = {0}; Info.Size = sizeof(Info);
		testRequire(xrtAllocationBudgetInfo(Budget, &Info) && Info.LiveBytes == 0, "client creation failure/lifecycle quota leak");
		xrtAllocationBudgetFree(Budget);
	}
	testRequire(Failed != 0 && Created != 0, "client create quota did not cross failure boundary");
	redispeer Peer; redisPeerStart(&Peer); xredisclient* Client = testClientConnect(&Peer, NULL);
	xfuture* Warm = testClientSubmit(Client, "PING", NULL, 5000); testClientResult(Warm); xrtFutureDestroy(Warm);
	Failed = 0; Created = 0;
	for (size_t Limit = 1; Limit < 9000; Limit += 127) {
		xallocationbudget* Budget = NULL; xallocationscope Scope = {0};
		testRequire(xrtAllocationBudgetCreate(Limit, &Budget) && xrtAllocationBudgetEnter(Budget, &Scope), "request quota");
		xfuture* Future = (xfuture*)(uintptr_t)1; xbytesview Ping = XRT_BYTES_LITERAL("PING");
		xredisadmission Admission = xredisClientCallAsync(Client, &Ping, 1, 5000, &Future);
		if (Admission == XREDIS_ADMISSION_ACCEPTED) ++Created;
		else { ++Failed; testRequire(Admission == XREDIS_ADMISSION_ERROR && Future == (xfuture*)(uintptr_t)1,
			"OOM admission changed output/status"); }
		xrtClearError(); xrtAllocationBudgetLeave(&Scope);
		if (Admission == XREDIS_ADMISSION_ACCEPTED) {
			testRequire(testClientResult(Future)->Status == XREDIS_REQUEST_OK, "request quota corrupted successful operation"); xrtFutureDestroy(Future);
		}
		xredisclientinfo ClientInfo = {0}; ClientInfo.Size = sizeof(ClientInfo); double End = xrtTimer() + 5.0;
		do { xredisClientInfo(Client, &ClientInfo); testRequire(xrtTimer() < End, "quota request retirement"); xrtThreadYield(); } while (ClientInfo.Requests != 0);
		xallocationbudgetinfo Info = {0}; Info.Size = sizeof(Info);
		testRequire(xrtAllocationBudgetInfo(Budget, &Info), "request quota info");
		if (Info.LiveBytes != 0) fprintf(stderr, "request quota limit=%zu live=%zu count=%zu\n", Limit, Info.LiveBytes, Info.LiveCount);
		testRequire(Info.LiveBytes == 0, "request quota leaked"); xrtAllocationBudgetFree(Budget);
	}
	testRequire(Failed != 0 && Created != 0, "request quota did not cross failure boundary"); testClientClose(&Peer, Client);
}

static void testClientByteLimits(void)
{
	for (size_t Trial = 0; Trial < 2; ++Trial) {
		redispeer Peer; redisPeerStart(&Peer); xredisclientconfig Config; xredisClientConfigInit(&Config);
		Config.MaxCommandBytes = Trial == 0 ? 16 : 64; Config.MaxCommandBytesTotal = Trial == 0 ? 64 : 16;
		if (Trial != 0) Config.MaxCommandBytes = 16;
		xredisclient* Client = testClientConnect(&Peer, &Config);
		xbytesview Args[] = {XRT_BYTES_LITERAL("ECHO"), XRT_BYTES_LITERAL("too large")}; xfuture* Output = (xfuture*)(uintptr_t)1;
		testRequire(xredisClientCallAsync(Client, Args, 2, 5000, &Output) == XREDIS_ADMISSION_OVERLOADED &&
			Output == (xfuture*)(uintptr_t)1, "per-command byte cap ignored"); testClientClose(&Peer, Client);
	}
	redispeer Peer; redisPeerStart(&Peer); xredisclientconfig Config; xredisClientConfigInit(&Config);
	Config.MaxCommandBytes = 14; Config.MaxCommandBytesTotal = 14; Config.DrainTimeout = 1000;
	xredisclient* Client = testClientConnect(&Peer, &Config); xfuture* Hold = testClientSubmit(Client, "HOLD", NULL, 1000);
	redisPeerWait(&Peer.Commands, 1); xbytesview Ping = XRT_BYTES_LITERAL("PING"); xfuture* Output = (xfuture*)(uintptr_t)1;
	testRequire(xredisClientCallAsync(Client, &Ping, 1, 5000, &Output) == XREDIS_ADMISSION_OVERLOADED,
		"aggregate command byte cap ignored");
	testRequire(xrtFutureCancel(Hold), "byte-cap cancel"); testClientResult(Hold); xrtFutureDestroy(Hold);
	testClientClose(&Peer, Client);
}

typedef struct testClientWorkerQuota {xallocationbudget* Budget; xallocationscope Scope; xatomic32 Stage;} testClientWorkerQuota;

static void testClientEnterQuota(xnetworker* Worker, ptr Data)
{
	(void)Worker; testClientWorkerQuota* Context = (testClientWorkerQuota*)Data;
	testRequire(xrtAllocationBudgetEnter(Context->Budget, &Context->Scope), "worker quota enter");
	xrtAtomic32Store(&Context->Stage, 1, XMEMORY_RELEASE);
}

static void testClientLeaveQuota(xnetworker* Worker, ptr Data)
{
	(void)Worker; testClientWorkerQuota* Context = (testClientWorkerQuota*)Data;
	xrtClearError(); testRequire(xrtAllocationBudgetLeave(&Context->Scope), "worker quota leave");
	xrtAtomic32Store(&Context->Stage, 2, XMEMORY_RELEASE);
}

static void testClientWorkerOOM(void)
{
	redispeer Peer; redisPeerStart(&Peer); xredisclient* Client = testClientConnect(&Peer, NULL);
	xfuture* Warm = testClientSubmit(Client, "PING", NULL, 5000); testClientResult(Warm); xrtFutureDestroy(Warm);
	testClientWorkerQuota Context = {0}; testRequire(xrtAllocationBudgetCreate(1, &Context.Budget), "worker quota create");
	testRequire(xrtNetEnginePost(Peer.Engine, 0, testClientEnterQuota, &Context), "worker quota post"); redisPeerWait(&Context.Stage, 1);
	xfuture* Future = testClientSubmit(Client, "PING", NULL, XRT_WAIT_FOREVER);
	const xredisrequestresult* Result = testClientResult(Future);
	testRequire(Result->Status == XREDIS_REQUEST_MEMORY && Result->Reply == NULL, "worker/diagnostic OOM lost typed outcome");
	testRequire(xrtNetEnginePost(Peer.Engine, 0, testClientLeaveQuota, &Context), "worker quota exit post"); redisPeerWait(&Context.Stage, 2);
	xrtFutureDestroy(Future); testClientClose(&Peer, Client);
	xallocationbudgetinfo Info = {0}; Info.Size = sizeof(Info);
	testRequire(xrtAllocationBudgetInfo(Context.Budget, &Info) && Info.LiveBytes == 0 && Info.Rejections != 0, "worker OOM scope leak/coverage");
	xrtAllocationBudgetFree(Context.Budget);
}

typedef struct testClientNotify {xredisclient* Client; xfuture* Child; xatomic32 Done;} testClientNotify;

static void testClientNotifyClose(ptr Data)
{
	testClientNotify* Context = (testClientNotify*)Data; xredisclientinfo Info = {0}; Info.Size = sizeof(Info);
	testRequire(xredisClientInfo(Context->Client, &Info), "notification info reentry");
	Context->Child = testClientSubmit(Context->Client, "PING", NULL, 5000);
	testRequire(xredisClientClose(Context->Client), "notification close reentry");
	xrtAtomic32Store(&Context->Done, 1, XMEMORY_RELEASE);
}

static void testClientReentry(void)
{
	redispeer Peer; redisPeerStart(&Peer); xredisclient* Client = testClientConnect(&Peer, NULL);
	xfuture* Hold = testClientSubmit(Client, "HOLD", NULL, 5000); redisPeerWait(&Peer.Commands, 1);
	testClientNotify Context = {Client, NULL, {0}}; xfuturewatch Watch;
	testRequire(xrtFutureWatchInit(&Watch, testClientNotifyClose, NULL, &Context) &&
		xrtFutureWatchAdd(Hold, &Watch) == XFUTURE_WATCH_PENDING, "pending notification registration");
	redisPeerReleaseBlocked(&Peer); redisPeerWait(&Context.Done, 1);
	const xredisrequestresult* Result = testClientResult(Context.Child);
	testRequire(Result->Status == XREDIS_REQUEST_CLOSED && Result->Delivery == XREDIS_NOT_SENT, "reentry child not closed before send");
	xrtFutureDestroy(Context.Child); xrtFutureDestroy(Hold); testClientClose(&Peer, Client);
}

static void testClientEdges(void)
{
	for (size_t Trial = 0; Trial < 2; ++Trial) {
		redispeer Peer; redisPeerStart(&Peer); xredisclientconfig Config; xredisClientConfigInit(&Config);
		if (Trial == 1) Config.MaxPushBytes = 1;
		xredisclient* Client = testClientConnect(&Peer, &Config);
		xfuture* Future = testClientSubmit(Client, Trial == 0 ? "EXTRA" : "PUSH", NULL, 5000);
		const xredisrequestresult* Result = testClientResult(Future);
		testRequire(Result->Status == (Trial == 0 ? XREDIS_REQUEST_OK : XREDIS_REQUEST_LIMIT), "extra/push-byte outcome");
		xfuture* Closed = xredisClientClosed(Client);
		testRequire(xrtFutureWaitFor(Closed, 10000) == XWAIT_OK, "extra/push-byte automatic close"); xrtFutureDestroy(Closed);
		xredisclientinfo Info = {0}; Info.Size = sizeof(Info); xredisClientInfo(Client, &Info);
		testRequire(Info.CloseStatus == (Trial == 0 ? XREDIS_REQUEST_PROTOCOL : XREDIS_REQUEST_LIMIT), "extra/push-byte close cause");
		xrtFutureDestroy(Future); testClientClose(&Peer, Client);
	}
	redispeer Peer; redisPeerStart(&Peer); xredisclientconfig Config; xredisClientConfigInit(&Config);
	for (size_t Trial = 0; Trial < 8; ++Trial) {
		xredisclientconfig Bad = Config; xnetaddr Remote = Peer.Address; xredisclient* Output = (xredisclient*)(uintptr_t)1;
		if (Trial == 0) Bad.Size = 0;
		if (Trial == 1) Bad.MaxRequests = 0;
		if (Trial == 2) Bad.MaxInFlight = Bad.MaxRequests + 1u;
		if (Trial == 3) Bad.MaxCommandBytes = Bad.Stream.WriteLimit + 1u;
		if (Trial == 4) Bad.DrainTimeout = 0;
		if (Trial == 5) Bad.Replies.Reader.Size = 0;
		if (Trial == 6) Bad.MaxPushes = SIZE_MAX;
		if (Trial == 7) Remote.Port = 0;
		testRequire(!xredisClientConnect(Peer.Engine, &Remote, 0, &Bad, &Output) && Output == (xredisclient*)(uintptr_t)1,
			"invalid client config/endpoint changed output"); xrtClearError();
	}
	testRequire(xrtNetListenerClose(Peer.Listener), "refusal listener close"); redisPeerWait(&Peer.ListenerClosed, 1);
	xredisclient* Client = testClientConnect(&Peer, NULL); xfuture* Future = testClientSubmit(Client, "PING", NULL, 5000);
	const xredisrequestresult* Result = testClientResult(Future);
	testRequire(Result->Status == XREDIS_REQUEST_DISCONNECTED && Result->Delivery == XREDIS_NOT_SENT && Result->Reply == NULL,
		"connect failure uncertainty/replay"); xrtFutureDestroy(Future); testClientClose(&Peer, Client);
}

static void testClientBatchedClose(void)
{
	redispeer Peer; redisPeerStart(&Peer); xredisclientconfig Config; xredisClientConfigInit(&Config);
	Config.WorkRequests = 1; Config.MaxInFlight = 1;
	xredisclient* Client = testClientConnect(&Peer, &Config); xfuture* Futures[101];
	Futures[0] = testClientSubmit(Client, "HOLD", NULL, XRT_WAIT_FOREVER); redisPeerWait(&Peer.Commands, 1);
	for (size_t Index = 1; Index < 101; ++Index) Futures[Index] = testClientSubmit(Client, "PING", NULL, XRT_WAIT_FOREVER);
	testRequire(xredisClientClose(Client), "batched close request");
	for (size_t Index = 0; Index < 101; ++Index) {
		const xredisrequestresult* Result = testClientResult(Futures[Index]);
		testRequire(Result->Status == XREDIS_REQUEST_CLOSED && Result->Delivery == (Index == 0 ? XREDIS_MAY_HAVE_EXECUTED : XREDIS_NOT_SENT),
			"batched close lost outcome/physical duty"); xrtFutureDestroy(Futures[Index]);
	}
	testClientClose(&Peer, Client);
}

int main(void)
{
	testClientBasic(); testClientTimeoutCancel(); testClientFailures(); testClientAdmission();
	testClientQueueDeadline(); testClientInvalid(); testClientThreads();
	testClientAllocations(); testClientByteLimits();
	testClientWorkerOOM(); testClientReentry();
	testClientEdges();
	testClientBatchedClose();
	printf("[PASS] xredis client: TCP/async/sync/pipeline/binary/push/deadline/cancel/tombstone/grace/close\n"); return 0;
}
