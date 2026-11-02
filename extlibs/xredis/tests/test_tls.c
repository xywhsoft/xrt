#include "test.h"
#include "support/tls_peer.h"

static const xredisreadyresult* tlsReady(xfuture* Future)
{
	testRequire(xrtFutureWaitFor(Future, 10000) == XWAIT_OK, "TLS readiness wait");
	const xredisreadyresult* Result = (const xredisreadyresult*)xrtFutureValue(Future); testRequire(Result != NULL, "TLS typed readiness"); return Result;
}
static const xredisrequestresult* tlsRequest(xfuture* Future)
{
	testRequire(xrtFutureWaitFor(Future, 10000) == XWAIT_OK, "TLS request wait");
	const xredisrequestresult* Result = (const xredisrequestresult*)xrtFutureValue(Future); testRequire(Result != NULL, "TLS typed request"); return Result;
}
static xredisclient* tlsConnect(redistlspeer* Peer, const xredisclientconfig* Config, const xredisclientinitconfig* Init, const xredisclienttlsconfig* TLS)
{
	xredisclient* Client = NULL;
	bool Success = xredisClientTlsDial(Peer->Dial.Init.Peer.Engine, Peer->Dial.Resolver, XRT_STR_LITERAL("redis.fixture"), Peer->Dial.Init.Peer.Address.Port,
		0, Config, Init, TLS, &Client);
	if (!Success) fprintf(stderr, "TLS factory kind=%d cause=%s\n", (int)xrtErrorKind(xrtGetError()), xrtErrorMessage(xrtGetError()));
	testRequire(Success, "TLS hostname admission"); return Client;
}
static void tlsBasic(void)
{
	for (size_t Mode = 0; Mode < 3; ++Mode) {
		redistlspeer Peer; redisTlsPeerStart(&Peer, false, false, Mode == 0 ? REDIS_DIAL_BLOCK : 0); xredisclienttlsconfig TLS; redisTlsPeerConfig(&Peer, &TLS);
		char Identity[] = "redis.fixture", Protocol[] = "r\0p"; xstrview ALPN = {Protocol, sizeof(Protocol) - 1u};
		TLS.Session.VerifyName = (xstrview){Identity, sizeof(Identity) - 1u}; TLS.Session.Protocols = &ALPN; TLS.Session.ProtocolCount = 1;
		xredisclientconfig Config; xredisClientConfigInit(&Config); Config.ReadBytes = 2; Config.ReadFrames = 1; Config.WorkRequests = 1; TLS.WriteBytes = 3;
		xredisclient* Client = NULL;
		if (Mode == 0) Client = tlsConnect(&Peer, &Config, NULL, &TLS);
		else if (Mode == 1) testRequire(xredisClientTlsConnect(Peer.Dial.Init.Peer.Engine, &Peer.Dial.Init.Peer.Address, 0, &Config, NULL, &TLS, &Client), "TLS numeric admission");
		else {
			xnetaddr Values[] = {Peer.Dial.RefusedAddress, Peer.Dial.Init.Peer.Address}; xnetaddrlist* Addresses = xrtNetAddrListCreate(Values, 2);
			testRequire(Addresses != NULL && xredisClientTlsDialAddresses(Peer.Dial.Init.Peer.Engine, Addresses, 0, &Config, NULL, &TLS, &Client), "TLS prepared fallback");
			xrtNetAddrListDestroy(Addresses);
		}
		memset(Identity, 0x5a, sizeof(Identity)); memset(Protocol, 0x5a, sizeof(Protocol)); memset(&TLS, 0, sizeof(TLS));
		xrtTlsVerifierRelease(Peer.Verifier); Peer.Verifier = NULL; /* Session retained it before asynchronous DNS. */
		xrtAtomic32Store(&Peer.Dial.Gate, 1, XMEMORY_RELEASE);
		xfuture* Ready = xredisClientReady(Client), *Replies[32];
		for (size_t Index = 0; Index < 32; ++Index) {
			xbytesview Args[] = {XRT_BYTES_LITERAL("ECHO"), XRT_BYTES_LITERAL("TLS\0\r\n")};
			testRequire(xredisClientCallAsync(Client, Args, 2, 5000, &Replies[Index]) == XREDIS_ADMISSION_ACCEPTED, "TLS queued pipeline");
		}
		const xredisreadyresult* State = tlsReady(Ready);
		if (State->Status != XREDIS_READY_OK) fprintf(stderr, "TLS ready status=%d cause=%s\n", (int)State->Status, State->Cause == NULL ? "none" : xrtErrorMessage(State->Cause));
		testRequire(State->Status == XREDIS_READY_OK && State->Negotiated && State->Hello != NULL, "TLS before HELLO readiness");
		for (size_t Index = 0; Index < 32; ++Index) {
			const xredisrequestresult* Result = tlsRequest(Replies[Index]); xredisreplyvalue Value;
			testRequire(Result->Status == XREDIS_REQUEST_OK && xredisReplyGet(Result->Reply, 0, &Value) && Value.Data.Size == 6 &&
				memcmp(Value.Data.Data, "TLS\0\r\n", 6) == 0, "TLS partial-write binary pipeline correlation");
		}
		redisDialClientClose(Client); redisTlsPeerStop(&Peer);
		testRequire(tlsReady(Ready)->Hello != NULL, "TLS retained ready metadata"); xrtFutureDestroy(Ready);
		for (size_t Index = 0; Index < 32; ++Index) xrtFutureDestroy(Replies[Index]);
	}
}

static void tlsFailures(void)
{
	for (size_t Trial = 0; Trial < 5; ++Trial) {
		redistlspeer Peer; redisTlsPeerStart(&Peer, Trial == 1 || Trial == 2, Trial == 0, Trial >= 3 ? REDIS_DIAL_BLOCK : 0);
		xredisclienttlsconfig TLS; redisTlsPeerConfig(&Peer, &TLS); xredisclientinitconfig Init; xredisClientInitConfigInit(&Init);
		if (Trial == 1) Init.Timeout = 30;
		if (Trial == 2) TLS.Stream.HandshakeTimeout = 30;
		xredisclient* Client = tlsConnect(&Peer, NULL, &Init, &TLS); xfuture* Ready = xredisClientReady(Client), *Ping = NULL;
		xbytesview Args = XRT_BYTES_LITERAL("PING"); testRequire(xredisClientCallAsync(Client, &Args, 1, 5000, &Ping) == XREDIS_ADMISSION_ACCEPTED, "TLS failed setup queued business");
		xfuture* Closed = NULL;
		if (Trial >= 3) {
			redisPeerWait(&Peer.Dial.Entered, 1); Closed = xredisClientClosed(Client); testRequire(xredisClientClose(Client), "TLS close blocked DNS");
			if (Trial == 4) { xredisClientRelease(Client); Client = NULL; }
		}
		const xredisreadyresult* State = tlsReady(Ready);
		testRequire(State->Status == (Trial == 0 ? XREDIS_READY_DISCONNECTED : Trial >= 3 ? XREDIS_READY_CLOSED : XREDIS_READY_TIMEOUT) && State->Hello == NULL,
			"TLS certificate/deadline/close status");
		testRequire(tlsRequest(Ping)->Delivery == XREDIS_NOT_SENT && xrtAtomic32Load(&Peer.Dial.Init.Peer.Commands, XMEMORY_ACQUIRE) == 0,
			"TLS failure leaked plaintext/business or silently downgraded");
		if (Client != NULL) redisDialClientClose(Client); else testRequire(xrtFutureWaitFor(Closed, 10000) == XWAIT_OK, "TLS runtime reference after caller release");
		if (Trial >= 3) testRequire(xrtAtomic32Load(&Peer.Dial.Exited, XMEMORY_ACQUIRE) == 0, "TLS physical close waited for uncancellable DNS");
		xrtFutureDestroy(Closed); xrtFutureDestroy(Ping); xrtFutureDestroy(Ready); redisTlsPeerStop(&Peer);
	}
}

typedef struct tlsGate {xatomic32 Entered, Release;} tlsGate;
static void tlsBlocked(xnetworker* Worker, ptr Data)
{
	(void)Worker; tlsGate* Gate = (tlsGate*)Data; xrtAtomic32Store(&Gate->Entered, 1, XMEMORY_RELEASE); double End = xrtTimer() + 10;
	while (!xrtAtomic32Load(&Gate->Release, XMEMORY_ACQUIRE)) { testRequire(xrtTimer() < End, "TLS worker gate not released"); xrtThreadYield(); }
}
static void tlsPartial(void)
{
	for (size_t Trial = 0; Trial < 3; ++Trial) {
		redistlspeer Peer; redisTlsPeerStart(&Peer, false, false, 0); xredisclienttlsconfig TLS; redisTlsPeerConfig(&Peer, &TLS); TLS.WriteBytes = 1;
		xredisclient* Client = tlsConnect(&Peer, NULL, NULL, &TLS); xfuture* Ready = xredisClientReady(Client);
		testRequire(tlsReady(Ready)->Status == XREDIS_READY_OK, "partial TLS ready");
		uint32 Before = xrtAtomic32Load(&Peer.Dial.Init.Peer.PlainBytes, XMEMORY_ACQUIRE);
		unsigned char Payload[16384]; memset(Payload, 'x', sizeof(Payload));
		xbytesview Echo[] = {XRT_BYTES_LITERAL("ECHO"), {Payload, sizeof(Payload)}}; xfuture* First = NULL, *Second = NULL;
		testRequire(xredisClientCallAsync(Client, Echo, 2, Trial == 1 ? 100 : 5000, &First) == XREDIS_ADMISSION_ACCEPTED, "partial TLS admission");
		redisPeerWait(&Peer.Dial.Init.Peer.PlainBytes, Before + 1u); tlsGate Gate = {0};
		testRequire(xrtNetEnginePost(Peer.Dial.Init.Peer.Engine, 0, tlsBlocked, &Gate), "partial TLS worker stall"); redisPeerWait(&Gate.Entered, 1);
		testRequire(xrtAtomic32Load(&Peer.Dial.Init.Peer.Commands, XMEMORY_ACQUIRE) == 1, "partial fixture reached whole command before stall");
		xbytesview Ping = XRT_BYTES_LITERAL("PING"); testRequire(xredisClientCallAsync(Client, &Ping, 1, 5000, &Second) == XREDIS_ADMISSION_ACCEPTED, "partial TLS FIFO successor");
		if (Trial == 0) testRequire(xrtFutureCancel(First), "partial TLS cancellation");
		else if (Trial == 1) { double End = xrtTimer() + 0.12; while (xrtTimer() < End) xrtThreadYield(); }
		else testRequire(xredisClientClose(Client), "partial TLS close");
		xrtAtomic32Store(&Gate.Release, 1, XMEMORY_RELEASE);
		const xredisrequestresult* Result = tlsRequest(First);
		testRequire(Result->Status == (Trial == 0 ? XREDIS_REQUEST_CANCELLED : Trial == 1 ? XREDIS_REQUEST_TIMEOUT : XREDIS_REQUEST_CLOSED) &&
			Result->Delivery == XREDIS_MAY_HAVE_EXECUTED, "partial TLS lost sent uncertainty");
		Result = tlsRequest(Second);
		testRequire(Result->Status == (Trial == 2 ? XREDIS_REQUEST_CLOSED : XREDIS_REQUEST_OK) && Result->Delivery == (Trial == 2 ? XREDIS_NOT_SENT : XREDIS_REPLIED),
			"partial TLS continuation/cancel tombstone broke following reply");
		if (Trial != 2) {
			xredisclientinfo Info = {0}; Info.Size = sizeof(Info); testRequire(xredisClientInfo(Client, &Info) && Info.DiscardedReplies == 1,
				"partial TLS late reply did not drain exactly once");
		}
		redisDialClientClose(Client); redisTlsPeerStop(&Peer); xrtFutureDestroy(Ready); xrtFutureDestroy(First); xrtFutureDestroy(Second);
	}
}

static void tlsAuthInitialization(void)
{
	for (size_t Trial = 0; Trial < 2; ++Trial) {
		redistlspeer Peer; redisTlsPeerStart(&Peer, false, false, 0);
		unsigned char Password[8192]; memset(Password, 'p', sizeof(Password)); Password[1] = 0;
		size_t Length = Trial == 0 ? 3u : sizeof(Password);
		const redisinitpeerstep Steps[] = {
			{{XRT_BYTES_LITERAL("HELLO"), XRT_BYTES_LITERAL("3"), XRT_BYTES_LITERAL("AUTH"), XRT_BYTES_LITERAL("default"), {Password, Length}},
			 5, XRT_BYTES_LITERAL(REDIS_INIT_HELLO_REPLY), false, false},
			{{XRT_BYTES_LITERAL("SELECT"), XRT_BYTES_LITERAL("2")}, 2, XRT_BYTES_LITERAL("+OK\r\n"), false, false}};
		/* Publish a fresh immutable script hook before any connection. */
		Peer.Dial.Init.Steps = Steps; Peer.Dial.Init.Count = 2;
		redispeerhook Hook = {redisInitPeerCommand, &Peer.Dial.Init}; xrtAtomicPtrStore(&Peer.Dial.Init.Peer.Hook, &Hook, XMEMORY_RELEASE);
		xredisclienttlsconfig TLS; redisTlsPeerConfig(&Peer, &TLS); TLS.WriteBytes = Trial == 0 ? 3 : 1;
		xredisclientinitconfig Init; xredisClientInitConfigInit(&Init); Init.Flags = XREDIS_INIT_AUTH | XREDIS_INIT_SELECT; Init.Password = (xbytesview){Password, Length}; Init.Database = 2;
		xredisclient* Client = tlsConnect(&Peer, NULL, &Init, &TLS); xfuture* Ready = xredisClientReady(Client);
		/* The script needs its expected bytes until observation. Snapshot source
		 * recycling is separately exercised by names/ALPN and client-init tests. */
		if (Trial == 0) {
			testRequire(tlsReady(Ready)->Status == XREDIS_READY_OK, "fragmented TLS AUTH/SELECT"); redisInitPeerStepWait(&Peer.Dial.Init, 2);
		} else {
			redisPeerWait(&Peer.Dial.Init.Peer.PlainBytes, 1); tlsGate Gate = {0};
			testRequire(xrtNetEnginePost(Peer.Dial.Init.Peer.Engine, 0, tlsBlocked, &Gate), "partial TLS AUTH worker stall"); redisPeerWait(&Gate.Entered, 1);
			testRequire(xrtFutureWaitFor(Ready, 0) == XWAIT_TIMEOUT && xredisClientClose(Client), "close incomplete TLS AUTH");
			xrtAtomic32Store(&Gate.Release, 1, XMEMORY_RELEASE); testRequire(tlsReady(Ready)->Status == XREDIS_READY_CLOSED, "partial TLS AUTH close readiness");
			testRequire(xrtAtomic32Load(&Peer.Dial.Init.StepsSeen, XMEMORY_ACQUIRE) == 0, "partial AUTH was replayed/completed after close");
		}
		redisDialClientClose(Client); redisTlsPeerStop(&Peer); xrtSecureZero(Password, sizeof(Password)); xrtFutureDestroy(Ready);
	}
}

static void tlsInvalid(void)
{
	redistlspeer Peer; redisTlsPeerStart(&Peer, false, false, 0); xredisclienttlsconfig Default; redisTlsPeerConfig(&Peer, &Default);
	for (size_t Trial = 0; Trial < 12; ++Trial) {
		xredisclienttlsconfig TLS = Default;
		if (Trial == 0) TLS.Size = 0;
		if (Trial == 1) TLS.WriteBytes = 0;
		if (Trial == 2) TLS.WriteBytes = 1024u * 1024u + 1u;
		if (Trial == 3) TLS.MaxMetadataBytes = 1;
		if (Trial == 4) TLS.MaxMetadataBytes = 1024u * 1024u + 1u;
		if (Trial == 5) TLS.Stream.HandshakeTimeout = -1;
		if (Trial == 6) TLS.Stream.CloseTimeout = -1;
		if (Trial == 7) TLS.Session.ProtocolCount = 257;
		if (Trial == 8) TLS.Session.Protocols = (const xstrview*)(uintptr_t)(UINTPTR_MAX - 1u), TLS.Session.ProtocolCount = 2;
		if (Trial == 9) TLS.Session.Verifier = NULL;
		if (Trial == 10) TLS.Session.VerifyName = (xstrview)XRT_STR_LITERAL("redis\0fixture");
		if (Trial == 11) TLS.Session.VerifyName = (xstrview){(cstr)(uintptr_t)(UINTPTR_MAX - 1u), 13};
		xredisclient* Client = (xredisclient*)(uintptr_t)1;
		testRequire(!xredisClientTlsDial(Peer.Dial.Init.Peer.Engine, Peer.Dial.Resolver, XRT_STR_LITERAL("redis.fixture"), Peer.Dial.Init.Peer.Address.Port,
			0, NULL, NULL, &TLS, &Client) && Client == (xredisclient*)(uintptr_t)1, "invalid TLS admitted/changed output"); xrtClearError();
	}
	redisTlsPeerStop(&Peer);
}

static void tlsAllocations(void)
{
	size_t Failed = 0, Created = 0;
	for (size_t Limit = 1; Limit < 128000; Limit += 4001) {
		redistlspeer Peer; redisTlsPeerStart(&Peer, false, false, 0); xredisclienttlsconfig TLS; redisTlsPeerConfig(&Peer, &TLS);
		xallocationbudget* Budget = NULL; xallocationscope Scope = {0};
		testRequire(xrtAllocationBudgetCreate(Limit, &Budget) && xrtAllocationBudgetEnter(Budget, &Scope), "TLS factory quota");
		xredisclient* Client = (xredisclient*)(uintptr_t)1;
		bool Success = xredisClientTlsDial(Peer.Dial.Init.Peer.Engine, Peer.Dial.Resolver, XRT_STR_LITERAL("redis.fixture"), Peer.Dial.Init.Peer.Address.Port, 0, NULL, NULL, &TLS, &Client);
		if (Success) ++Created; else { ++Failed; testRequire(Client == (xredisclient*)(uintptr_t)1, "TLS OOM changed output"); }
		xrtClearError(); testRequire(xrtAllocationBudgetLeave(&Scope), "TLS factory quota leave");
		if (Success) { xfuture* Ready = xredisClientReady(Client); testRequire(tlsReady(Ready)->Status == XREDIS_READY_OK, "TLS accepted quota result"); redisDialClientClose(Client); xrtFutureDestroy(Ready); }
		redisTlsPeerStop(&Peer); xallocationbudgetinfo Info = {0}; Info.Size = sizeof(Info);
		testRequire(xrtAllocationBudgetInfo(Budget, &Info) && Info.LiveBytes == 0 && Info.LiveCount == 0, "TLS factory OOM retained session/config/client"); xrtAllocationBudgetFree(Budget);
	}
	testRequire(Failed != 0 && Created != 0, "TLS quota did not cross creation threshold");
}

typedef struct tlsQuota {xallocationbudget* Budget; xallocationscope Scope; xatomic32 Stage;} tlsQuota;
static void tlsQuotaEnter(xnetworker* Worker, ptr Data)
{ (void)Worker; tlsQuota* Quota = (tlsQuota*)Data; testRequire(xrtAllocationBudgetEnter(Quota->Budget, &Quota->Scope), "TLS worker quota enter"); xrtAtomic32Store(&Quota->Stage, 1, XMEMORY_RELEASE); }
static void tlsQuotaLeave(xnetworker* Worker, ptr Data)
{ (void)Worker; tlsQuota* Quota = (tlsQuota*)Data; xrtClearError(); testRequire(xrtAllocationBudgetLeave(&Quota->Scope), "TLS worker quota leave"); xrtAtomic32Store(&Quota->Stage, 2, XMEMORY_RELEASE); }
static void tlsWorkerOOM(void)
{
	for (size_t Trial = 0; Trial < 2; ++Trial) {
		redistlspeer Peer; redisTlsPeerStart(&Peer, false, false, 0); xredisclienttlsconfig TLS; redisTlsPeerConfig(&Peer, &TLS);
		tlsQuota Quota = {0}; testRequire(xrtAllocationBudgetCreate(1, &Quota.Budget), "TLS worker quota create"); xredisclient* Client = NULL; xfuture* Ready = NULL, *Request = NULL;
		if (Trial == 1) { Client = tlsConnect(&Peer, NULL, NULL, &TLS); Ready = xredisClientReady(Client); testRequire(tlsReady(Ready)->Status == XREDIS_READY_OK, "TLS send OOM initial readiness"); }
		testRequire(xrtNetEnginePost(Peer.Dial.Init.Peer.Engine, 0, tlsQuotaEnter, &Quota), "TLS worker quota post"); redisPeerWait(&Quota.Stage, 1);
		if (Trial == 0) { Client = tlsConnect(&Peer, NULL, NULL, &TLS); Ready = xredisClientReady(Client); testRequire(tlsReady(Ready)->Status == XREDIS_READY_MEMORY, "TLS native start OOM result"); }
		else {
			xbytesview Ping = XRT_BYTES_LITERAL("PING"); testRequire(xredisClientCallAsync(Client, &Ping, 1, 5000, &Request) == XREDIS_ADMISSION_ACCEPTED, "TLS encryption OOM admission");
			const xredisrequestresult* Result = tlsRequest(Request);
			/* Reused cipher blocks can accept plaintext before queue metadata
			 * hits this quota. That is MAY_HAVE_EXECUTED, even if TCP sends none. */
			testRequire(Result->Status == XREDIS_REQUEST_MEMORY && Result->Reply == NULL &&
				(Result->Delivery == XREDIS_NOT_SENT || Result->Delivery == XREDIS_MAY_HAVE_EXECUTED), "TLS send OOM lost typed delivery uncertainty");
		}
		testRequire(xrtNetEnginePost(Peer.Dial.Init.Peer.Engine, 0, tlsQuotaLeave, &Quota), "TLS worker quota exit post"); redisPeerWait(&Quota.Stage, 2);
		redisDialClientClose(Client); redisTlsPeerStop(&Peer); xrtFutureDestroy(Ready); xrtFutureDestroy(Request);
		xallocationbudgetinfo Info = {0}; Info.Size = sizeof(Info); testRequire(xrtAllocationBudgetInfo(Quota.Budget, &Info) && Info.LiveBytes == 0 && Info.LiveCount == 0 && Info.Rejections != 0, "TLS worker OOM leak/coverage");
		xrtAllocationBudgetFree(Quota.Budget);
	}
}
int main(void)
{
	tlsBasic(); tlsFailures(); tlsPartial(); tlsAuthInitialization(); tlsInvalid(); tlsAllocations(); tlsWorkerOOM();
	puts("[PASS] xredis TLS: fixture/numeric/host/fallback/partial init/binary pipeline/cancel/deadline/close/verification/limits/OOM/lifetime"); return 0;
}
