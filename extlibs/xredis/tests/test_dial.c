#include "test.h"
#include "support/dial_peer.h"

static const xredisreadyresult* dialResult(xfuture* Future)
{
	xfutureresult Result;
	testRequire(xrtFutureWaitFor(Future, 10000) == XWAIT_OK && xrtFutureResult(Future, &Result) &&
		Result.State == XFUTURE_RESOLVED && Result.Value != NULL, "Dial immutable typed result"); return (const xredisreadyresult*)Result.Value;
}

static xfuture* dialPing(xredisclient* Client, int64 Timeout)
{
	xbytesview Args = XRT_BYTES_LITERAL("PING"); xfuture* Future = NULL;
	testRequire(xredisClientCallAsync(Client, &Args, 1, Timeout, &Future) == XREDIS_ADMISSION_ACCEPTED, "Dial queued request"); return Future;
}

static const xredisrequestresult* dialRequest(xfuture* Future)
{
	testRequire(xrtFutureWaitFor(Future, 10000) == XWAIT_OK, "Dial business wait");
	const xredisrequestresult* Result = (const xredisrequestresult*)xrtFutureValue(Future); testRequire(Result != NULL, "Dial business typed value"); return Result;
}

static xredisclient* dialConnect(redisdialpeer* Peer, const xredisclientinitconfig* Init, const xredisclientdialconfig* Dial)
{
	xredisclient* Client = NULL;
	testRequire(xredisClientDial(Peer->Init.Peer.Engine, Peer->Resolver, XRT_STR_LITERAL("redis.fixture"), Peer->Init.Peer.Address.Port,
		0, NULL, Init, Dial, &Client), "host Dial factory"); return Client;
}

static void dialBasic(void)
{
	for (size_t Trial = 0; Trial < 3; ++Trial) {
		redisdialpeer Peer; redisDialPeerStart(&Peer, Trial == 1 ? REDIS_DIAL_DUAL : 0, 0, 0); xredisclient* Client = NULL;
		xredisclientconfig Config; xredisClientConfigInit(&Config); Config.ReadBytes = 2; Config.WorkRequests = 1;
		char Host[] = "redis.fixture";
		if (Trial != 2) testRequire(xredisClientDial(Peer.Init.Peer.Engine, Peer.Resolver, (xstrview){Host, sizeof(Host) - 1u}, Peer.Init.Peer.Address.Port,
			0, &Config, NULL, NULL, &Client), "bounded host snapshot");
		else {
			xnetaddr Endpoints[] = {Peer.RefusedAddress, Peer.Init.Peer.Address};
			xnetaddrlist* Addresses = xrtNetAddrListCreate(Endpoints, 2);
			testRequire(Addresses != NULL && xredisClientDialAddresses(Peer.Init.Peer.Engine, Addresses, 0, &Config, NULL, NULL, &Client), "resolved endpoint fallback");
			xrtNetAddrListDestroy(Addresses); memset(Endpoints, 0, sizeof(Endpoints));
		}
		memset(Host, 0x5a, sizeof(Host)); xfuture* Ready = xredisClientReady(Client), *Ping = dialPing(Client, 5000);
		const xredisreadyresult* Result = dialResult(Ready);
		if (Result->Status != XREDIS_READY_OK) fprintf(stderr, "Dial trial=%zu status=%d step=%d protocol=%d cause=%s\n", Trial, (int)Result->Status,
			(int)Result->Step, (int)Result->Protocol, Result->Cause == NULL ? "none" : xrtErrorMessage(Result->Cause));
		testRequire(Result->Status == XREDIS_READY_OK && Result->Protocol == XREDIS_RESP3 && Result->Negotiated && Result->Hello != NULL,
			"Dial handshake/winning stream transfer");
		testRequire(dialRequest(Ping)->Status == XREDIS_REQUEST_OK, "queued command did not survive Dial"); xrtFutureDestroy(Ping);
		for (size_t Index = 0; Index < 32; ++Index) {
			char Number[24]; snprintf(Number, sizeof(Number), "%zu", Index);
			xbytesview Args[] = {XRT_BYTES_LITERAL("ID"), {(cbytes)Number, strlen(Number)}}; xfuture* Future = NULL;
			testRequire(xredisClientCall(Client, Args, 2, 5000, &Future) == XREDIS_ADMISSION_ACCEPTED, "Dial sync facade");
			xredisreplyvalue Value; const xredisrequestresult* Reply = dialRequest(Future);
			testRequire(Reply->Status == XREDIS_REQUEST_OK && xredisReplyGet(Reply->Reply, 0, &Value) && Value.Integer == (int64)Index, "Dial FIFO correlation");
			xrtFutureDestroy(Future);
		}
		testRequire(xrtAtomic32Load(&Peer.Lookups, XMEMORY_ACQUIRE) == (Trial == 2 ? 0u : 1u), "unexpected resolver work/replay");
		redisDialClientClose(Client); redisDialPeerStop(&Peer);
		testRequire(dialResult(Ready)->Hello != NULL, "Dial readiness survives client/engine/Resolver"); xrtFutureDestroy(Ready);
	}
}

static void dialQueue(void)
{
	redisdialpeer Peer; redisDialPeerStart(&Peer, REDIS_DIAL_BLOCK, 0, 0); xredisclient* Client = dialConnect(&Peer, NULL, NULL);
	redisPeerWait(&Peer.Entered, 1); xfuture* Ready = xredisClientReady(Client), *Expired = dialPing(Client, 20), *Cancel = dialPing(Client, 5000), *Live = dialPing(Client, 5000);
	testRequire(xrtFutureCancel(Cancel), "cancel while DNS pending");
	const xredisrequestresult* Result = dialRequest(Expired);
	testRequire(Result->Status == XREDIS_REQUEST_TIMEOUT && Result->Delivery == XREDIS_NOT_SENT, "DNS wait omitted from deadline");
	Result = dialRequest(Cancel); testRequire(Result->Status == XREDIS_REQUEST_CANCELLED && Result->Delivery == XREDIS_NOT_SENT, "DNS queued cancellation");
	testRequire(xrtFutureWaitFor(Ready, 0) == XWAIT_TIMEOUT && xrtAtomic32Load(&Peer.Init.Peer.Accepted, XMEMORY_ACQUIRE) == 0, "Dial readiness bypassed DNS barrier");
	xrtAtomic32Store(&Peer.Gate, 1, XMEMORY_RELEASE);
	testRequire(dialResult(Ready)->Status == XREDIS_READY_OK && dialRequest(Live)->Status == XREDIS_REQUEST_OK, "DNS release did not resume setup/request");
	xrtFutureDestroy(Expired); xrtFutureDestroy(Cancel); xrtFutureDestroy(Live); xrtFutureDestroy(Ready); redisDialClientClose(Client); redisDialPeerStop(&Peer);
}

static void dialInterrupted(void)
{
	for (size_t Trial = 0; Trial < 4; ++Trial) {
		redisdialpeer Peer; redisDialPeerStart(&Peer, REDIS_DIAL_BLOCK, 0, 0);
		xredisclientinitconfig Init; xredisClientInitConfigInit(&Init); Init.Timeout = Trial == 1 ? 30 : Trial == 2 ? 0 : 5000;
		xredisclientdialconfig Dial; xredisClientDialConfigInit(&Dial); if (Trial == 3) Dial.Timeout = 30;
		xredisclient* Client = dialConnect(&Peer, &Init, &Dial); xfuture* Ready = xredisClientReady(Client);
		if (Trial != 2) redisPeerWait(&Peer.Entered, 1);
		if (Trial == 0) testRequire(xredisClientClose(Client), "close blocked DNS");
		const xredisreadyresult* Result = dialResult(Ready);
		testRequire(Result->Status == (Trial == 0 ? XREDIS_READY_CLOSED : XREDIS_READY_TIMEOUT) && Result->Hello == NULL, "Dial cancel/overall/native deadline");
		redisDialClientClose(Client);
		/* Retire the engine while OS/custom lookup still runs. It cannot retain
		 * this client's callback data; the shared Resolver is joined separately. */
		redisPeerStop(&Peer.Init.Peer);
		testRequire(xrtNetSocketClose(Peer.Refused), "interrupted refusal socket close");
		if (Trial == 2) testRequire(xrtAtomic32Load(&Peer.Lookups, XMEMORY_ACQUIRE) == 0, "already-expired setup submitted DNS");
		xrtAtomic32Store(&Peer.Gate, 1, XMEMORY_RELEASE); testRequire(xrtNetResolverDestroy(Peer.Resolver), "late lookup/service retirement");
		testRequire(dialResult(Ready)->Status == (Trial == 0 ? XREDIS_READY_CLOSED : XREDIS_READY_TIMEOUT), "late lookup changed immutable outcome");
		xrtFutureDestroy(Ready);
	}
}

static void dialFailures(void)
{
	for (size_t Trial = 0; Trial < 4; ++Trial) {
		redisdialpeer Peer; redisDialPeerStart(&Peer, Trial == 0 ? REDIS_DIAL_MISSING : 0, 0, Trial == 1 ? 3 : 0);
		xredisclientdialconfig Dial; xredisClientDialConfigInit(&Dial); xredisclient* Client = NULL;
		if (Trial < 2) Client = dialConnect(&Peer, NULL, &Dial);
		else {
			xnetaddr Values[] = {Peer.RefusedAddress, Peer.Init.Peer.Address}; Dial.MaxAttempts = Trial == 2 ? 1 : 2;
			if (Trial == 3) Dial.Family = XNET_FAMILY_IPV6;
			xnetaddrlist* Addresses = xrtNetAddrListCreate(Values, 2);
			if (Trial == 3) {
				Client = (xredisclient*)(uintptr_t)1;
				testRequire(!xredisClientDialAddresses(Peer.Init.Peer.Engine, Addresses, 0, NULL, NULL, &Dial, &Client) && Client == (xredisclient*)(uintptr_t)1,
					"family filtering admitted an empty candidate set"); xrtClearError(); xrtNetAddrListDestroy(Addresses); redisDialPeerStop(&Peer); continue;
			}
			testRequire(xredisClientDialAddresses(Peer.Init.Peer.Engine, Addresses, 0, NULL, NULL, &Dial, &Client), "bounded failed candidate factory"); xrtNetAddrListDestroy(Addresses);
		}
		xfuture* Ready = redisDialReady(Client); const xredisreadyresult* Result = dialResult(Ready);
		testRequire(Result->Status == (Trial == 1 ? XREDIS_READY_LIMIT : XREDIS_READY_DISCONNECTED) && Result->Cause != NULL && Result->Hello == NULL,
			"DNS miss/host limit/candidate failure lost typed cause");
		testRequire(xrtAtomic32Load(&Peer.Init.Peer.Accepted, XMEMORY_ACQUIRE) == 0, "failed/capped Dial sent business or connected elsewhere");
		xrtFutureDestroy(Ready); redisDialClientClose(Client); redisDialPeerStop(&Peer);
	}
}

static void dialInvalid(void)
{
	redisdialpeer Peer; redisDialPeerStart(&Peer, 0, 0, 0); xredisclientdialconfig Default; xredisClientDialConfigInit(&Default);
	for (size_t Trial = 0; Trial < 12; ++Trial) {
		xredisclientdialconfig Bad = Default; xstrview Host = XRT_STR_LITERAL("redis.fixture"); uint16 Port = Peer.Init.Peer.Address.Port;
		if (Trial == 0) Bad.Size = 0;
		if (Trial == 1) Bad.Family = (xnetfamily)7;
		if (Trial == 2) Bad.Timeout = -2;
		if (Trial == 3) Bad.FallbackDelay = -1;
		if (Trial == 4) Bad.MaxAttempts = 0;
		if (Trial == 5) Bad.MaxAttempts = 65;
		if (Trial == 6) Bad.MaxHostBytes = 0;
		if (Trial == 7) Bad.MaxHostBytes = XREDIS_DIAL_HOST_MAX + 1u;
		if (Trial == 8) Host = (xstrview)XRT_STR_LITERAL("redis\0fixture");
		if (Trial == 9) Host = (xstrview){NULL, 1};
		if (Trial == 10) Host = (xstrview){(cstr)(uintptr_t)(UINTPTR_MAX - 2u), 9};
		if (Trial == 11) Port = 0;
		xredisclient* Output = (xredisclient*)(uintptr_t)1;
		testRequire(!xredisClientDial(Peer.Init.Peer.Engine, Peer.Resolver, Host, Port, 0, NULL, NULL, &Bad, &Output) && Output == (xredisclient*)(uintptr_t)1,
			"invalid Dial changed output/admitted"); xrtClearError();
	}
	xredisclient* Output = (xredisclient*)(uintptr_t)1;
	testRequire(!xredisClientDial(Peer.Init.Peer.Engine, Peer.Resolver, (xstrview){(cstr)&Output, sizeof(Output)}, Peer.Init.Peer.Address.Port,
		0, NULL, NULL, NULL, &Output) && Output == (xredisclient*)(uintptr_t)1, "host/output alias"); xrtClearError();
	testRequire(!xredisClientDial(Peer.Init.Peer.Engine, Peer.Resolver, XRT_STR_LITERAL("redis.fixture"), Peer.Init.Peer.Address.Port,
		0, NULL, NULL, &Default, (xredisclient**)&Default), "policy/output alias"); xrtClearError();
	for (size_t Trial = 0; Trial < 2; ++Trial) {
		xnetaddr Values[65]; for (size_t Index = 0; Index < 65; ++Index) { Values[Index] = Peer.Init.Peer.Address; Values[Index].Port = (uint16)(Index + 1u); }
		if (Trial == 1) Values[0].Port = 0;
		xnetaddrlist* Addresses = xrtNetAddrListCreate(Values, Trial == 0 ? 65 : 1);
		testRequire(Addresses != NULL && !xredisClientDialAddresses(Peer.Init.Peer.Engine, Addresses, 0, NULL, NULL, NULL, &Output) &&
			Output == (xredisclient*)(uintptr_t)1, "address count/port input bound"); xrtClearError(); xrtNetAddrListDestroy(Addresses);
	}
	redisDialPeerStop(&Peer);
}

static void dialOverload(void)
{
	redisdialpeer Peer; redisDialPeerStart(&Peer, REDIS_DIAL_BLOCK, 1, 0);
	xredisclient* First = dialConnect(&Peer, NULL, NULL); redisPeerWait(&Peer.Entered, 1);
	xredisclient* Second = dialConnect(&Peer, NULL, NULL); xfuture* Ready = redisDialReady(Second);
	testRequire(dialResult(Ready)->Status == XREDIS_READY_LIMIT, "Resolver subscriber bound lost by Dial");
	redisDialClientClose(Second); xrtFutureDestroy(Ready); redisDialClientClose(First);
	testRequire(xrtAtomic32Load(&Peer.Lookups, XMEMORY_ACQUIRE) == 1 && xrtAtomic32Load(&Peer.Exited, XMEMORY_ACQUIRE) == 0,
		"overload replayed a query or Close depended on blocked lookup"); redisDialPeerStop(&Peer);
}

typedef struct dialWorkerGate {xatomic32 Entered, Gate;} dialWorkerGate;
static void dialWorkerBlocked(xnetworker* Worker, ptr Data)
{
	(void)Worker; dialWorkerGate* Gate = (dialWorkerGate*)Data; xrtAtomic32Store(&Gate->Entered, 1, XMEMORY_RELEASE);
	double End = xrtTimer() + 10.0;
	while (!xrtAtomic32Load(&Gate->Gate, XMEMORY_ACQUIRE)) { testRequire(xrtTimer() < End, "Dial worker gate not released"); xrtThreadYield(); }
}
static void dialBeforeStart(void)
{
	for (size_t Trial = 0; Trial < 2; ++Trial) {
		redisdialpeer Peer; redisDialPeerStart(&Peer, 0, 0, 0); dialWorkerGate Gate = {0};
		testRequire(xrtNetEnginePost(Peer.Init.Peer.Engine, 0, dialWorkerBlocked, &Gate), "Dial worker stall post"); redisPeerWait(&Gate.Entered, 1);
		xredisclientinitconfig Init; xredisClientInitConfigInit(&Init); Init.Timeout = Trial == 0 ? 20 : 5000;
		xredisclient* Client = dialConnect(&Peer, &Init, NULL); xfuture* Ready = xredisClientReady(Client), *Request = dialPing(Client, 5000);
		if (Trial == 0) { double End = xrtTimer() + 0.03; while (xrtTimer() < End) xrtThreadYield(); }
		else testRequire(xredisClientClose(Client), "Dial close before worker startup");
		xrtAtomic32Store(&Gate.Gate, 1, XMEMORY_RELEASE);
		testRequire(dialResult(Ready)->Status == (Trial == 0 ? XREDIS_READY_TIMEOUT : XREDIS_READY_CLOSED) &&
			dialRequest(Request)->Delivery == XREDIS_NOT_SENT, "startup delay reset deadline/ignored close");
		redisDialClientClose(Client); testRequire(xrtAtomic32Load(&Peer.Lookups, XMEMORY_ACQUIRE) == 0, "closed/expired startup submitted DNS");
		xrtFutureDestroy(Request); xrtFutureDestroy(Ready); redisDialPeerStop(&Peer);
	}
}

typedef struct dialWorkerQuota {xallocationbudget* Budget; xallocationscope Scope; xatomic32 Stage;} dialWorkerQuota;
static void dialWorkerEnter(xnetworker* Worker, ptr Data)
{
	(void)Worker; dialWorkerQuota* Quota = (dialWorkerQuota*)Data;
	testRequire(xrtAllocationBudgetEnter(Quota->Budget, &Quota->Scope), "Dial worker quota enter"); xrtAtomic32Store(&Quota->Stage, 1, XMEMORY_RELEASE);
}
static void dialWorkerLeave(xnetworker* Worker, ptr Data)
{
	(void)Worker; dialWorkerQuota* Quota = (dialWorkerQuota*)Data;
	xrtClearError(); testRequire(xrtAllocationBudgetLeave(&Quota->Scope), "Dial worker quota leave"); xrtAtomic32Store(&Quota->Stage, 2, XMEMORY_RELEASE);
}
static void dialWorkerOOM(void)
{
	for (size_t Trial = 0; Trial < 2; ++Trial) {
		redisdialpeer Peer; redisDialPeerStart(&Peer, 0, 0, 0); dialWorkerQuota Quota = {0};
		testRequire(xrtAllocationBudgetCreate(1, &Quota.Budget) && xrtNetEnginePost(Peer.Init.Peer.Engine, 0, dialWorkerEnter, &Quota), "Dial worker budget admission");
		redisPeerWait(&Quota.Stage, 1); xredisclientinitconfig Init; xredisClientInitConfigInit(&Init);
		if (Trial == 1) Init.Timeout = XRT_WAIT_FOREVER; /* Reach native Dial allocation instead of failing the init timer. */
		xredisclient* Client = dialConnect(&Peer, &Init, NULL); xfuture* Ready = redisDialReady(Client);
		testRequire(dialResult(Ready)->Status == XREDIS_READY_MEMORY, "Dial worker allocation failure lost typed result");
		testRequire(xrtNetEnginePost(Peer.Init.Peer.Engine, 0, dialWorkerLeave, &Quota), "Dial worker quota exit post"); redisPeerWait(&Quota.Stage, 2);
		redisDialClientClose(Client); xrtFutureDestroy(Ready); redisDialPeerStop(&Peer);
		xallocationbudgetinfo Info = {0}; Info.Size = sizeof(Info);
		testRequire(xrtAllocationBudgetInfo(Quota.Budget, &Info) && Info.LiveBytes == 0 && Info.LiveCount == 0 && Info.Rejections != 0, "Dial worker OOM lifetime leak");
		xrtAllocationBudgetFree(Quota.Budget);
	}
}

typedef struct dialNotification {xredisclient* Client; xatomic32 Done;} dialNotification;
static void dialNotified(ptr Data)
{
	dialNotification* Note = (dialNotification*)Data; xredisclientinfo Info = {0}; Info.Size = sizeof(Info);
	testRequire(xredisClientInfo(Note->Client, &Info) && xredisClientClose(Note->Client), "Dial readiness Info/Close reentry");
	xrtAtomic32Store(&Note->Done, 1, XMEMORY_RELEASE);
}
static void dialHandshake(void)
{
	for (size_t Trial = 0; Trial < 3; ++Trial) {
		redisinitpeerstep Step = {{XRT_BYTES_LITERAL("HELLO"), XRT_BYTES_LITERAL("3")}, 2,
			Trial == 1 ? (xbytesview)XRT_BYTES_LITERAL("-WRONGPASS fixture\r\n") : (xbytesview)XRT_BYTES_LITERAL(REDIS_INIT_HELLO_REPLY), true, false};
		redisdialpeer Peer; redisDialPeerStartScript(&Peer, Trial == 2 ? 0 : REDIS_DIAL_BLOCK, 0, 0, &Step, Trial == 2 ? 0 : 1);
		xredisclientinitconfig Init; xredisClientInitConfigInit(&Init); if (Trial == 2) Init.Mode = XREDIS_INIT_RESP2;
		xredisclient* Client = dialConnect(&Peer, &Init, NULL); xfuture* Ready = xredisClientReady(Client);
		if (Trial != 2) {
			redisPeerWait(&Peer.Entered, 1); dialNotification Note = {Client, {0}}; xfuturewatch Watch;
			testRequire(xrtFutureWatchInit(&Watch, dialNotified, NULL, &Note) && xrtFutureWatchAdd(Ready, &Watch) == XFUTURE_WATCH_PENDING, "Dial readiness watch");
			xrtAtomic32Store(&Peer.Gate, 1, XMEMORY_RELEASE); redisInitPeerStepWait(&Peer.Init, 1); redisPeerReleaseBlocked(&Peer.Init.Peer);
			redisPeerWait(&Note.Done, 1); redisDialClientClose(Client);
		} else {
			testRequire(dialResult(Ready)->Protocol == XREDIS_RESP2 && !dialResult(Ready)->Negotiated, "Dial legacy initialization");
			xfuture* Ping = dialPing(Client, 5000); testRequire(dialRequest(Ping)->Status == XREDIS_REQUEST_OK, "legacy Dial business");
			xrtFutureDestroy(Ping); redisDialClientClose(Client);
		}
		const xredisreadyresult* Result = dialResult(Ready);
		testRequire(Result->Status == (Trial == 1 ? XREDIS_READY_AUTH_FAILED : XREDIS_READY_OK), "Dial setup failure/success immutable status");
		if (Trial == 1) testRequire(Result->Hello == NULL && Result->Cause != NULL && strcmp(xrtErrorMessage(Result->Cause), "connection initialization failed") == 0,
			"DNS path bypassed setup diagnostic redaction"); xrtFutureDestroy(Ready); redisDialPeerStop(&Peer);
	}
}

static void dialAllocations(void)
{
	size_t Failed = 0, Created = 0;
	for (size_t Limit = 1; Limit < 23000; Limit += 467) {
		redisdialpeer Peer; redisDialPeerStart(&Peer, 0, 0, 0); xallocationbudget* Budget = NULL; xallocationscope Scope = {0};
		testRequire(xrtAllocationBudgetCreate(Limit, &Budget) && xrtAllocationBudgetEnter(Budget, &Scope), "Dial factory quota");
		xredisclient* Client = (xredisclient*)(uintptr_t)1;
		bool Ok = xredisClientDial(Peer.Init.Peer.Engine, Peer.Resolver, XRT_STR_LITERAL("redis.fixture"), Peer.Init.Peer.Address.Port, 0, NULL, NULL, NULL, &Client);
		if (Ok) ++Created; else { ++Failed; testRequire(Client == (xredisclient*)(uintptr_t)1, "failed Dial consumed output"); }
		xrtClearError(); testRequire(xrtAllocationBudgetLeave(&Scope), "Dial factory quota leave");
		if (Ok) { xfuture* Ready = redisDialReady(Client); testRequire(dialResult(Ready)->Status == XREDIS_READY_OK, "quota success setup"); xrtFutureDestroy(Ready); redisDialClientClose(Client); }
		redisDialPeerStop(&Peer); xallocationbudgetinfo Info = {0}; Info.Size = sizeof(Info);
		testRequire(xrtAllocationBudgetInfo(Budget, &Info) && Info.LiveBytes == 0 && Info.LiveCount == 0, "Dial source/constructor lifetime quota leak"); xrtAllocationBudgetFree(Budget);
	}
	testRequire(Failed != 0 && Created != 0, "Dial factory quota did not cross boundary");
}

int main(void)
{
	dialBasic(); dialQueue(); dialInterrupted(); dialFailures(); dialInvalid(); dialOverload(); dialBeforeStart(); dialWorkerOOM(); dialHandshake(); dialAllocations();
	puts("[PASS] xredis Dial: host snapshot/async DNS/address fallback/init/deadline/cancel/close/limits/OOM/late resolver lifetime"); return 0;
}
