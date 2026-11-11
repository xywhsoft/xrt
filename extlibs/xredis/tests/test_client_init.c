#include "test.h"
#include "support/client_init_peer.h"

#define INIT_B(Text) XRT_BYTES_LITERAL(Text)

static const void* initWait(xfuture* Future)
{
	testRequire(Future != NULL && xrtFutureWaitFor(Future, 10000) == XWAIT_OK, "initialization future wait");
	xfutureresult Result;
	testRequire(xrtFutureResult(Future, &Result) && Result.State == XFUTURE_RESOLVED && Result.Value != NULL,
		"initialization typed immutable outcome"); return Result.Value;
}

static xredisclient* initConnect(redisinitpeer* Peer, const xredisclientconfig* Config, const xredisclientinitconfig* Init)
{
	xredisclient* Client = NULL;
	testRequire(xredisClientConnectInit(Peer->Peer.Engine, &Peer->Peer.Address, 0, Config, Init, &Client), "managed connect admission");
	return Client;
}

static xfuture* initPing(xredisclient* Client, int64 Timeout)
{
	xbytesview Ping = INIT_B("PING"); xfuture* Future = NULL;
	testRequire(xredisClientCallAsync(Client, &Ping, 1, Timeout, &Future) == XREDIS_ADMISSION_ACCEPTED, "queued business admission"); return Future;
}

static void initClose(redisinitpeer* Peer, xredisclient* Client)
{
	xfuture* Closed = xredisClientClosed(Client); testRequire(xredisClientClose(Client), "managed close");
	testRequire(xrtFutureWaitFor(Closed, 10000) == XWAIT_OK, "managed physical close");
	xredisclientinfo Info = {0}; Info.Size = sizeof(Info);
	testRequire(xredisClientInfo(Client, &Info) && Info.State == XREDIS_CLIENT_CLOSED && Info.InitBytes == 0 &&
		Info.Requests == 0 && Info.CommandBytes == 0 && Info.Accepted == Info.Completed, "setup wire/request retirement barrier");
	xrtFutureDestroy(Closed); xredisClientRelease(Client); redisPeerStop(&Peer->Peer);
}

static void initExpectReady(xfuture* Future, xredisprotocol Protocol, bool Downgraded)
{
	const xredisreadyresult* Ready = (const xredisreadyresult*)initWait(Future);
	testRequire(Ready->Status == XREDIS_READY_OK && Ready->Step == XREDIS_INIT_DONE && Ready->Protocol == Protocol &&
		Ready->Downgraded == Downgraded && Ready->Negotiated == (Protocol == XREDIS_RESP3) && Ready->Cause == NULL,
		"readiness protocol/negotiation outcome");
	if (Protocol == XREDIS_RESP3) {
		xredisreplyvalue Root; xredisreplyinfo Info = {0}; Info.Size = sizeof(Info);
		testRequire(Ready->Hello != NULL && xredisReplyInfo(Ready->Hello, &Info) && xredisReplyGet(Ready->Hello, Info.Root, &Root) && Root.Type == XREDIS_REPLY_MAP,
			"owned HELLO metadata");
	} else testRequire(Ready->Hello == NULL, "legacy connection fabricated HELLO metadata");
}

static void initSuccess(void)
{
	for (size_t Trial = 0; Trial < 9; ++Trial) {
		xredisclientinitconfig Init; xredisClientInitConfigInit(&Init);
		unsigned char Username[] = {'a', 0, 'u'}, Password[] = {'p', 0, '\r', '\n', 0xff}, Name[] = {'n', 0, 'm'};
		const xbytesview User = INIT_B("a\0u"), Pass = INIT_B("p\0\r\n\xff"), ClientName = INIT_B("n\0m");
		bool Legacy = Trial >= 4, Downgraded = Trial >= 7, Full = Trial == 1 || Trial == 3 || Trial == 5 || Trial == 7;
		if (Trial == 2 || Trial == 3 || Downgraded) Init.Mode = XREDIS_INIT_AUTO;
		else if (Legacy) Init.Mode = XREDIS_INIT_RESP2;
		if (Full) {
			Init.Flags = XREDIS_INIT_AUTH | XREDIS_INIT_USERNAME | XREDIS_INIT_SETNAME | XREDIS_INIT_SELECT;
			Init.Username = (xbytesview){Username, sizeof(Username)}; Init.Password = (xbytesview){Password, sizeof(Password)};
			Init.Name = (xbytesview){Name, sizeof(Name)}; Init.Database = UINT32_MAX;
		} else if (Trial == 6) Init.Flags = XREDIS_INIT_AUTH | XREDIS_INIT_SETNAME | XREDIS_INIT_SELECT; /* Explicit empty/default/zero. */
		redisinitpeerstep Steps[5] = {0}; size_t Count = 0;
		if (Init.Mode != XREDIS_INIT_RESP2) {
			redisinitpeerstep* Step = &Steps[Count++]; Step->Arguments[Step->Count++] = (xbytesview)INIT_B("HELLO");
			Step->Arguments[Step->Count++] = (xbytesview)INIT_B("3");
			if (Full) {
				Step->Arguments[Step->Count++] = (xbytesview)INIT_B("AUTH"); Step->Arguments[Step->Count++] = User;
				Step->Arguments[Step->Count++] = Pass; Step->Arguments[Step->Count++] = (xbytesview)INIT_B("SETNAME");
				Step->Arguments[Step->Count++] = ClientName;
			}
			Step->Response = Downgraded ? (Trial == 7 ? (xbytesview)INIT_B("-ERR unknown command 'hello', with args beginning with: secret\r\n") :
				(xbytesview)INIT_B("-NOPROTO unsupported protocol version\r\n")) : (xbytesview)INIT_B(REDIS_INIT_HELLO_REPLY);
		}
		if (Legacy && (Init.Flags & XREDIS_INIT_AUTH)) {
			redisinitpeerstep* Step = &Steps[Count++]; Step->Arguments[Step->Count++] = (xbytesview)INIT_B("AUTH");
			if (Full) Step->Arguments[Step->Count++] = User;
			Step->Arguments[Step->Count++] = Full ? Pass : (xbytesview){NULL, 0}; Step->Response = (xbytesview)INIT_B("+OK\r\n");
		}
		if (Legacy && (Init.Flags & XREDIS_INIT_SETNAME))
			Steps[Count++] = (redisinitpeerstep){{INIT_B("CLIENT"), INIT_B("SETNAME"), Full ? ClientName : (xbytesview){NULL, 0}}, 3, INIT_B("+OK\r\n"), false, false};
		if (Init.Flags & XREDIS_INIT_SELECT)
			Steps[Count++] = (redisinitpeerstep){{INIT_B("SELECT"), Full ? (xbytesview)INIT_B("4294967295") : (xbytesview)INIT_B("0")}, 2, INIT_B("+OK\r\n"), false, false};
		redisinitpeer Peer; redisInitPeerStart(&Peer, Steps, Count);
		xredisclientconfig Config; xredisClientConfigInit(&Config); Config.ReadBytes = 1; Config.ReadFrames = 1; Config.WorkRequests = 1;
		if (Init.Mode == XREDIS_INIT_RESP2) Config.Replies.Reader.Protocol = XREDIS_RESP2;
		xredisclient* Client = initConnect(&Peer, &Config, Trial == 0 ? NULL : &Init);
		memset(Username, 0x5a, sizeof(Username)); memset(Password, 0x5a, sizeof(Password)); memset(Name, 0x5a, sizeof(Name));
		xfuture* Ready = xredisClientReady(Client), *Ping = initPing(Client, 5000);
		initExpectReady(Ready, Legacy ? XREDIS_RESP2 : XREDIS_RESP3, Downgraded);
		const xredisrequestresult* Result = (const xredisrequestresult*)initWait(Ping);
		testRequire(Result->Status == XREDIS_REQUEST_OK && Result->Delivery == XREDIS_REPLIED, "business escaped/consumed setup reply");
		redisPeerWait(&Peer.Peer.Commands, (uint32)(Count + 1u));
		testRequire(xrtAtomic32Load(&Peer.Peer.Commands, XMEMORY_ACQUIRE) == Count + 1u, "unexpected/repeated setup command");
		xredisclientinfo Info = {0}; Info.Size = sizeof(Info); xredisClientInfo(Client, &Info);
		testRequire(Info.State == XREDIS_CLIENT_OPEN && Info.Ready && Info.Protocol == (Legacy ? XREDIS_RESP2 : XREDIS_RESP3), "readiness info");
		xrtFutureDestroy(Ping); initClose(&Peer, Client);
		initExpectReady(Ready, Legacy ? XREDIS_RESP2 : XREDIS_RESP3, Downgraded); /* No engine/client retained. */
		xrtFutureDestroy(Ready);
	}
	/* HELLO supplies the default ACL username, including an explicit empty password. */
	const redisinitpeerstep Default[] = {{{INIT_B("HELLO"), INIT_B("3"), INIT_B("AUTH"), INIT_B("default"), {NULL, 0}, INIT_B("SETNAME"), {NULL, 0}}, 7,
		INIT_B("|1\r\n+trace\r\n:7\r\n" REDIS_INIT_HELLO_REPLY), false, false}};
	redisinitpeer Peer; redisInitPeerStart(&Peer, Default, 1); xredisclientinitconfig Init; xredisClientInitConfigInit(&Init);
	Init.Flags = XREDIS_INIT_AUTH | XREDIS_INIT_SETNAME; xredisclient* Client = initConnect(&Peer, NULL, &Init);
	xfuture* Ready = xredisClientReady(Client); initExpectReady(Ready, XREDIS_RESP3, false);
	const xredisreadyresult* Result = (const xredisreadyresult*)initWait(Ready); xredisreplyvalue Root; xredisreplyinfo ReplyInfo = {0}; ReplyInfo.Size = sizeof(ReplyInfo);
	testRequire(xredisReplyInfo(Result->Hello, &ReplyInfo) && xredisReplyGet(Result->Hello, ReplyInfo.Root, &Root) && Root.Attributes != XREDIS_REPLY_NONE, "HELLO attributes lost");
	initClose(&Peer, Client); xrtFutureDestroy(Ready);
}

static void initBarrier(void)
{
	const redisinitpeerstep Steps[] = {{{INIT_B("HELLO"), INIT_B("3")}, 2, INIT_B(">1\r\n+notice\r\n" REDIS_INIT_HELLO_REPLY), true, false}};
	redisinitpeer Peer; redisInitPeerStart(&Peer, Steps, 1); xredisclient* Client = initConnect(&Peer, NULL, NULL);
	xfuture* Ready = xredisClientReady(Client); redisInitPeerStepWait(&Peer, 1);
	testRequire(xrtFutureWaitFor(Ready, 0) == XWAIT_TIMEOUT, "initialization bypassed held response");
	xredisclientinfo Info = {0}; Info.Size = sizeof(Info); xredisClientInfo(Client, &Info);
	testRequire(!Info.Ready && Info.State == XREDIS_CLIENT_CONNECTING && Info.InitStep == XREDIS_INIT_HELLO && Info.InitBytes != 0, "setup barrier info");
	xfuture* Expired = initPing(Client, 30), *Cancelled = initPing(Client, 5000), *Live = initPing(Client, 5000);
	testRequire(xrtFutureCancel(Cancelled) && xrtFutureCancel(Ready), "request/observer cancellation");
	const xredisrequestresult* Result = (const xredisrequestresult*)initWait(Expired);
	testRequire(Result->Status == XREDIS_REQUEST_TIMEOUT && Result->Delivery == XREDIS_NOT_SENT, "setup wait excluded from request deadline");
	Result = (const xredisrequestresult*)initWait(Cancelled);
	testRequire(Result->Status == XREDIS_REQUEST_CANCELLED && Result->Delivery == XREDIS_NOT_SENT, "queued initialization cancellation");
	testRequire(xrtAtomic32Load(&Peer.Peer.Commands, XMEMORY_ACQUIRE) == 1 && xrtFutureWaitFor(Ready, 0) == XWAIT_TIMEOUT, "business sent before readiness");
	redisPeerReleaseBlocked(&Peer.Peer); initExpectReady(Ready, XREDIS_RESP3, false);
	Result = (const xredisrequestresult*)initWait(Live); testRequire(Result->Status == XREDIS_REQUEST_OK, "live queued request lost");
	xredisreply* Push = NULL; testRequire(xredisClientTakePush(Client, &Push), "setup push lost"); xredisReplyRelease(Push);
	xrtFutureDestroy(Expired); xrtFutureDestroy(Cancelled); xrtFutureDestroy(Live); xrtFutureDestroy(Ready); initClose(&Peer, Client);
}

static void initRejected(void)
{
	const struct {cstr Response; xredisreadystatus Status; xredisinitmode Mode;} Cases[] = {
		{"-ERR unknown command 'HELLO', with args beginning with: leaked-password\r\n", XREDIS_READY_UNSUPPORTED, XREDIS_INIT_RESP3},
		{"-NOPROTO unsupported protocol version\r\n", XREDIS_READY_UNSUPPORTED, XREDIS_INIT_RESP3},
		{"-WRONGPASS leaked-password\r\n", XREDIS_READY_AUTH_FAILED, XREDIS_INIT_AUTO},
		{"-NOAUTH Authentication required\r\n", XREDIS_READY_AUTH_FAILED, XREDIS_INIT_AUTO},
		{"-ERR invalid password\r\n", XREDIS_READY_AUTH_FAILED, XREDIS_INIT_AUTO},
		{"-ERR syntax error\r\n", XREDIS_READY_SERVER_ERROR, XREDIS_INIT_AUTO},
		{"-BUSY service unavailable\r\n", XREDIS_READY_SERVER_ERROR, XREDIS_INIT_AUTO},
		{"-ERR unknown command 'HELLOX'\r\n", XREDIS_READY_SERVER_ERROR, XREDIS_INIT_AUTO},
		{"-NOPROTOX ordinary error\r\n", XREDIS_READY_SERVER_ERROR, XREDIS_INIT_AUTO},
		{"!25\r\nWRONGPASS leaked-password\r\n", XREDIS_READY_AUTH_FAILED, XREDIS_INIT_AUTO},
		{"+OK\r\n", XREDIS_READY_PROTOCOL, XREDIS_INIT_RESP3},
		{"%3\r\n+server\r\n+redis\r\n+version\r\n+7\r\n+proto\r\n:2\r\n", XREDIS_READY_PROTOCOL, XREDIS_INIT_AUTO},
		{"%3\r\n+version\r\n+7\r\n+proto\r\n:3\r\n+version\r\n+8\r\n", XREDIS_READY_PROTOCOL, XREDIS_INIT_RESP3},
		{"%3\r\n+server\r\n$-1\r\n+version\r\n+7\r\n+proto\r\n:3\r\n", XREDIS_READY_PROTOCOL, XREDIS_INIT_RESP3},
		{"%3\r\n+server\r\n+redis\r\n+version\r\n+7\r\n+proto\r\n+3\r\n", XREDIS_READY_PROTOCOL, XREDIS_INIT_RESP3}
	};
	for (size_t Trial = 0; Trial < sizeof(Cases) / sizeof(Cases[0]); ++Trial) {
		redisinitpeerstep Step = {{INIT_B("HELLO"), INIT_B("3"), INIT_B("AUTH"), INIT_B("default"), INIT_B("synthetic")}, 5,
			{(cbytes)Cases[Trial].Response, strlen(Cases[Trial].Response)}, true, false};
		redisinitpeer Peer; redisInitPeerStart(&Peer, &Step, 1); xredisclientinitconfig Init; xredisClientInitConfigInit(&Init);
		Init.Mode = Cases[Trial].Mode; Init.Flags = XREDIS_INIT_AUTH; Init.Password = (xbytesview)INIT_B("synthetic");
		xredisclient* Client = initConnect(&Peer, NULL, &Init); xfuture* Ready = xredisClientReady(Client), *Ping = initPing(Client, 5000);
		redisInitPeerStepWait(&Peer, 1); redisPeerReleaseBlocked(&Peer.Peer);
		const xredisreadyresult* Result = (const xredisreadyresult*)initWait(Ready);
		testRequire(Result->Status == Cases[Trial].Status && Result->Step == XREDIS_INIT_HELLO && !Result->Downgraded && Result->Hello == NULL,
			"initialization error downgraded/auth/validation result");
		const xredisrequestresult* Request = (const xredisrequestresult*)initWait(Ping);
		testRequire(Request->Status == XREDIS_REQUEST_INITIALIZATION && Request->Delivery == XREDIS_NOT_SENT && Request->Reply == NULL,
			"failed setup sent business or lost failure category");
		if (Result->Cause != NULL) testRequire(strcmp(xrtErrorMessage(Result->Cause), "connection initialization failed") == 0, "credential leaked into diagnostic");
		testRequire(xrtAtomic32Load(&Peer.Peer.Commands, XMEMORY_ACQUIRE) == 1, "authentication failure retried/downgraded");
		xrtFutureDestroy(Ping); initClose(&Peer, Client); xrtFutureDestroy(Ready);
	}
	/* Every legacy step validates its own reply and stops the remaining script. */
	for (size_t StepIndex = 0; StepIndex < 3; ++StepIndex) {
		redisinitpeerstep Steps[] = {
			{{INIT_B("AUTH"), INIT_B("synthetic")}, 2, INIT_B("+OK\r\n"), false, false},
			{{INIT_B("CLIENT"), INIT_B("SETNAME"), INIT_B("fixture")}, 3, INIT_B("+OK\r\n"), false, false},
			{{INIT_B("SELECT"), INIT_B("9")}, 2, INIT_B("+OK\r\n"), false, false}};
		Steps[StepIndex].Response = StepIndex == 0 ? (xbytesview)INIT_B("-ERR invalid password\r\n") :
			StepIndex == 1 ? (xbytesview)INIT_B("$2\r\nOK\r\n") : (xbytesview)INIT_B("-ERR DB index is out of range\r\n");
		redisinitpeer Peer; redisInitPeerStart(&Peer, Steps, StepIndex + 1u); xredisclientinitconfig Init; xredisClientInitConfigInit(&Init);
		Init.Mode = XREDIS_INIT_RESP2; Init.Flags = XREDIS_INIT_AUTH | XREDIS_INIT_SETNAME | XREDIS_INIT_SELECT;
		Init.Password = (xbytesview)INIT_B("synthetic"); Init.Name = (xbytesview)INIT_B("fixture"); Init.Database = 9;
		xredisclient* Client = initConnect(&Peer, NULL, &Init); xfuture* Ready = xredisClientReady(Client);
		const xredisreadyresult* Result = (const xredisreadyresult*)initWait(Ready);
		testRequire(Result->Status == (StepIndex == 0 ? XREDIS_READY_AUTH_FAILED : StepIndex == 1 ? XREDIS_READY_PROTOCOL : XREDIS_READY_SERVER_ERROR) &&
			Result->Step == (xredisinitstep)(XREDIS_INIT_AUTHENTICATE + StepIndex), "legacy step failure correlation");
		redisPeerWait(&Peer.Peer.Commands, (uint32)(StepIndex + 1u));
		testRequire(xrtAtomic32Load(&Peer.Peer.Commands, XMEMORY_ACQUIRE) == StepIndex + 1u, "legacy step continued after failure");
		initClose(&Peer, Client); xrtFutureDestroy(Ready);
	}
	const cstr AuthErrors[] = {"-BUSY service unavailable\r\n", "-ERR unknown command 'AUTH'\r\n"};
	for (size_t Trial = 0; Trial < sizeof(AuthErrors) / sizeof(AuthErrors[0]); ++Trial) {
		const redisinitpeerstep Step = {{INIT_B("AUTH"), INIT_B("fixture")}, 2,
			{(cbytes)AuthErrors[Trial], strlen(AuthErrors[Trial])}, false, false};
		redisinitpeer Peer; redisInitPeerStart(&Peer, &Step, 1); xredisclientinitconfig Init; xredisClientInitConfigInit(&Init);
		Init.Mode = XREDIS_INIT_RESP2; Init.Flags = XREDIS_INIT_AUTH; Init.Password = (xbytesview)INIT_B("fixture");
		xredisclient* Client = initConnect(&Peer, NULL, &Init); xfuture* Ready = xredisClientReady(Client);
		const xredisreadyresult* Result = (const xredisreadyresult*)initWait(Ready);
		testRequire(Result->Status == XREDIS_READY_SERVER_ERROR && Result->Step == XREDIS_INIT_AUTHENTICATE && !Result->Downgraded,
			"generic AUTH command failure misclassified as rejected credentials");
		initClose(&Peer, Client); xrtFutureDestroy(Ready);
	}
}

typedef struct initStall {xatomic32 Entered, Release;} initStall;
static void initStallWorker(xnetworker* Worker, ptr Data)
{
	(void)Worker; initStall* Stall = (initStall*)Data; xrtAtomic32Store(&Stall->Entered, 1, XMEMORY_RELEASE);
	double End = xrtTimer() + 10.0;
	while (!xrtAtomic32Load(&Stall->Release, XMEMORY_ACQUIRE)) { testRequire(xrtTimer() < End, "injected stall expired"); xrtThreadYield(); }
}

static void initInterrupted(void)
{
	for (size_t Trial = 0; Trial < 5; ++Trial) {
		redisinitpeerstep Step = {{INIT_B("HELLO"), INIT_B("3")}, 2, {NULL, 0}, true, Trial == 2};
		redisinitpeer Peer; redisInitPeerStart(&Peer, &Step, 1); xredisclientinitconfig Init; xredisClientInitConfigInit(&Init);
		Init.Timeout = Trial == 0 ? 50 : Trial == 3 ? 0 : Trial == 4 ? 20 : XRT_WAIT_FOREVER;
		initStall Stall = {0};
		if (Trial == 4) {
			testRequire(xrtNetEnginePost(Peer.Peer.Engine, 0, initStallWorker, &Stall), "initialization stall post"); redisPeerWait(&Stall.Entered, 1);
		}
		xredisclient* Client = initConnect(&Peer, NULL, &Init); xfuture* Ready = xredisClientReady(Client);
		if (Trial == 1) { redisInitPeerStepWait(&Peer, 1); testRequire(xredisClientClose(Client), "close pending handshake"); }
		if (Trial == 4) {
			double End = xrtTimer() + 0.06; while (xrtTimer() < End) xrtThreadYield();
			xrtAtomic32Store(&Stall.Release, 1, XMEMORY_RELEASE);
		}
		const xredisreadyresult* Result = (const xredisreadyresult*)initWait(Ready);
		testRequire(Result->Status == (Trial == 1 ? XREDIS_READY_CLOSED : Trial == 2 ? XREDIS_READY_DISCONNECTED : XREDIS_READY_TIMEOUT), "setup interruption outcome");
		if (Trial >= 3) testRequire(xrtAtomic32Load(&Peer.Peer.Accepted, XMEMORY_ACQUIRE) == 0, "expired setup started TCP");
		initClose(&Peer, Client); xrtFutureDestroy(Ready);
	}
	/* SELECT shares the original setup deadline; success of HELLO does not reset it. */
	const redisinitpeerstep Steps[] = {
		{{INIT_B("HELLO"), INIT_B("3")}, 2, INIT_B(REDIS_INIT_HELLO_REPLY), false, false},
		{{INIT_B("SELECT"), INIT_B("0")}, 2, {NULL, 0}, true, false}};
	redisinitpeer Peer; redisInitPeerStart(&Peer, Steps, 2); xredisclientinitconfig Init; xredisClientInitConfigInit(&Init);
	Init.Timeout = 100; Init.Flags = XREDIS_INIT_SELECT; xredisclient* Client = initConnect(&Peer, NULL, &Init);
	xfuture* Ready = xredisClientReady(Client); const xredisreadyresult* Result = (const xredisreadyresult*)initWait(Ready);
	testRequire(Result->Status == XREDIS_READY_TIMEOUT && Result->Step == XREDIS_INIT_DATABASE && Result->Negotiated &&
		Result->Protocol == XREDIS_RESP3 && Result->Hello == NULL, "post-HELLO setup timeout/metadata lifetime");
	initClose(&Peer, Client); xrtFutureDestroy(Ready);
}

static void initInvalid(void)
{
	redisinitpeer Peer; redisInitPeerStart(&Peer, NULL, 0); xredisclientinitconfig Defaults; xredisClientInitConfigInit(&Defaults);
	for (size_t Trial = 0; Trial < 14; ++Trial) {
		xredisclientinitconfig Bad = Defaults; xredisclientconfig Config; xredisClientConfigInit(&Config);
		if (Trial == 0) Bad.Size = 0;
		if (Trial == 1) Bad.Mode = (xredisinitmode)1;
		if (Trial == 2) Bad.Flags = 0x80000000u;
		if (Trial == 3) Bad.Flags = XREDIS_INIT_USERNAME;
		if (Trial == 4) Bad.Password = (xbytesview)INIT_B("unused");
		if (Trial == 5) Bad.Username = (xbytesview)INIT_B("unused");
		if (Trial == 6) Bad.Name = (xbytesview)INIT_B("unused");
		if (Trial == 7) Bad.Database = 1;
		if (Trial == 8) Bad.Timeout = -2;
		if (Trial == 9) Bad.MaxWireBytes = 0;
		if (Trial == 10) Bad.MaxWireBytes = 1;
		if (Trial == 11) { Bad.Flags = XREDIS_INIT_AUTH; Bad.Password = (xbytesview){NULL, 1}; }
		if (Trial == 12) { Bad.Flags = XREDIS_INIT_AUTH; Bad.Password = (xbytesview){(cbytes)(uintptr_t)(UINTPTR_MAX - 2u), 9}; }
		if (Trial == 13) Config.Replies.Reader.Protocol = XREDIS_RESP2;
		xredisclient* Output = (xredisclient*)(uintptr_t)1;
		testRequire(!xredisClientConnectInit(Peer.Peer.Engine, &Peer.Peer.Address, 0, &Config, &Bad, &Output) &&
			Output == (xredisclient*)(uintptr_t)1, "invalid initialization changed output/admitted"); xrtClearError();
	}
#if !defined(XREDIS_FEATURE_XREDIS_ROLE_CLIENT)
    xredisclientinitconfig Off = Defaults; Off.Flags = 0x10u; xredisclient* OffOutput = (xredisclient*)(uintptr_t)1;
    testRequire(!xredisClientConnectInit(Peer.Peer.Engine, &Peer.Peer.Address, 0, NULL, &Off, &OffOutput) && OffOutput == (xredisclient*)(uintptr_t)1,
        "role flags stay invalid without the optional role-client feature"); xrtClearError();
#endif
	xredisclientinitconfig Init = Defaults;
	testRequire(!xredisClientConnectInit(Peer.Peer.Engine, &Peer.Peer.Address, 0, NULL, &Init, (xredisclient**)&Init), "initialization descriptor alias"); xrtClearError();
	xredisclient* Output = (xredisclient*)(uintptr_t)1; Init.Flags = XREDIS_INIT_AUTH; Init.Password = (xbytesview){(cbytes)&Output, sizeof(Output)};
	testRequire(!xredisClientConnectInit(Peer.Peer.Engine, &Peer.Peer.Address, 0, NULL, &Init, &Output) && Output == (xredisclient*)(uintptr_t)1,
		"credential/output alias"); xrtClearError(); redisPeerStop(&Peer.Peer);
	/* Exact aggregate setup cap, including the AUTO fallback command. */
	const redisinitpeerstep Step = {{INIT_B("HELLO"), INIT_B("3"), INIT_B("AUTH"), INIT_B("default"), INIT_B("p")}, 5, INIT_B(REDIS_INIT_HELLO_REPLY), false, false};
	xbytesview Auth[] = {INIT_B("AUTH"), INIT_B("p")}; size_t HelloBytes = 0, AuthBytes = 0;
	testRequire(xredisCommandSize(Step.Arguments, Step.Count, &HelloBytes) && xredisCommandSize(Auth, 2, &AuthBytes), "setup wire size");
	redisInitPeerStart(&Peer, &Step, 1); xredisClientInitConfigInit(&Init); Init.Mode = XREDIS_INIT_AUTO; Init.Flags = XREDIS_INIT_AUTH; Init.Password = (xbytesview)INIT_B("p");
	Init.MaxWireBytes = HelloBytes + AuthBytes - 1u; Output = (xredisclient*)(uintptr_t)1;
	testRequire(!xredisClientConnectInit(Peer.Peer.Engine, &Peer.Peer.Address, 0, NULL, &Init, &Output) && Output == (xredisclient*)(uintptr_t)1,
		"AUTO fallback omitted from total cap"); xrtClearError(); ++Init.MaxWireBytes;
	xredisclient* Client = initConnect(&Peer, NULL, &Init); xfuture* Ready = xredisClientReady(Client); initExpectReady(Ready, XREDIS_RESP3, false);
	const xbytesview Controls[][2] = {{INIT_B("hello"), INIT_B("3")}, {INIT_B("aUtH"), INIT_B("p")}, {INIT_B("SELECT"), INIT_B("0")}, {INIT_B("CLIENT"), INIT_B("sEtNaMe")}};
	for (size_t Index = 0; Index < 4; ++Index) {
		xfuture* Future = (xfuture*)(uintptr_t)1;
		testRequire(xredisClientCallAsync(Client, Controls[Index], 2, 1000, &Future) == XREDIS_ADMISSION_ERROR &&
			Future == (xfuture*)(uintptr_t)1 && xrtErrorCode(xrtGetError()) == XREDIS_CLIENT_COMMAND_MODE, "managed controls escaped initialization"); xrtClearError();
	}
	initClose(&Peer, Client); xrtFutureDestroy(Ready);
}

static void initAllocations(void)
{
	size_t Failures = 0, Successes = 0;
	const redisinitpeerstep Step = {{INIT_B("HELLO"), INIT_B("3"), INIT_B("AUTH"), INIT_B("default"), INIT_B("synthetic")}, 5, INIT_B(REDIS_INIT_HELLO_REPLY), false, false};
	for (size_t Limit = 1; Limit < 23000; Limit += 383) {
		redisinitpeer Peer; redisInitPeerStart(&Peer, &Step, 1); xredisclientinitconfig Init; xredisClientInitConfigInit(&Init);
		Init.Mode = XREDIS_INIT_AUTO; Init.Flags = XREDIS_INIT_AUTH; Init.Password = (xbytesview)INIT_B("synthetic");
		xallocationbudget* Budget = NULL; xallocationscope Scope = {0};
		testRequire(xrtAllocationBudgetCreate(Limit, &Budget) && xrtAllocationBudgetEnter(Budget, &Scope), "setup creation quota");
		xredisclient* Client = (xredisclient*)(uintptr_t)1;
		bool Success = xredisClientConnectInit(Peer.Peer.Engine, &Peer.Peer.Address, 0, NULL, &Init, &Client);
		if (Success) ++Successes; else { ++Failures; testRequire(Client == (xredisclient*)(uintptr_t)1, "OOM setup output ownership"); }
		xrtClearError(); testRequire(xrtAllocationBudgetLeave(&Scope), "setup quota exit");
		if (Success) {
			xfuture* Ready = xredisClientReady(Client); initExpectReady(Ready, XREDIS_RESP3, false); initClose(&Peer, Client); xrtFutureDestroy(Ready);
		} else redisPeerStop(&Peer.Peer);
		xallocationbudgetinfo Info = {0}; Info.Size = sizeof(Info);
		testRequire(xrtAllocationBudgetInfo(Budget, &Info) && Info.LiveBytes == 0 && Info.LiveCount == 0, "setup OOM/cold lifetime quota leak");
		xrtAllocationBudgetFree(Budget);
	}
	testRequire(Failures != 0 && Successes != 0, "setup creation quota failed to cross boundary");
}

typedef struct initWorkerQuota {xallocationbudget* Budget; xallocationscope Scope; xatomic32 Stage;} initWorkerQuota;
static void initWorkerEnter(xnetworker* Worker, ptr Data)
{
	(void)Worker; initWorkerQuota* Quota = (initWorkerQuota*)Data;
	testRequire(xrtAllocationBudgetEnter(Quota->Budget, &Quota->Scope), "setup worker quota enter"); xrtAtomic32Store(&Quota->Stage, 1, XMEMORY_RELEASE);
}
static void initWorkerLeave(xnetworker* Worker, ptr Data)
{
	(void)Worker; initWorkerQuota* Quota = (initWorkerQuota*)Data;
	xrtClearError(); testRequire(xrtAllocationBudgetLeave(&Quota->Scope), "setup worker quota leave"); xrtAtomic32Store(&Quota->Stage, 2, XMEMORY_RELEASE);
}
static void initWorkerOOM(void)
{
	for (size_t Trial = 0; Trial < 2; ++Trial) {
		const redisinitpeerstep Step = {{INIT_B("HELLO"), INIT_B("3")}, 2, INIT_B(REDIS_INIT_HELLO_REPLY), true, false};
		redisinitpeer Peer; redisInitPeerStart(&Peer, &Step, 1); initWorkerQuota Quota = {0};
		testRequire(xrtAllocationBudgetCreate(1, &Quota.Budget), "setup worker quota"); xredisclient* Client = NULL;
		if (Trial == 1) { Client = initConnect(&Peer, NULL, NULL); redisInitPeerStepWait(&Peer, 1); }
		testRequire(xrtNetEnginePost(Peer.Peer.Engine, 0, initWorkerEnter, &Quota), "setup worker quota post"); redisPeerWait(&Quota.Stage, 1);
		if (Trial == 0) Client = initConnect(&Peer, NULL, NULL); else redisPeerReleaseBlocked(&Peer.Peer);
		xfuture* Ready = xredisClientReady(Client); const xredisreadyresult* Result = (const xredisreadyresult*)initWait(Ready);
		testRequire(Result->Status == XREDIS_READY_MEMORY && Result->Hello == NULL, "setup worker/diagnostic OOM lost typed outcome");
		testRequire(xrtNetEnginePost(Peer.Peer.Engine, 0, initWorkerLeave, &Quota), "setup worker quota exit post"); redisPeerWait(&Quota.Stage, 2);
		initClose(&Peer, Client); xrtFutureDestroy(Ready); xallocationbudgetinfo Info = {0}; Info.Size = sizeof(Info);
		testRequire(xrtAllocationBudgetInfo(Quota.Budget, &Info) && Info.LiveBytes == 0 && Info.Rejections != 0, "setup worker quota coverage/leak");
		xrtAllocationBudgetFree(Quota.Budget);
	}
}

typedef struct initNotification {xredisclient* Client; bool Success; xfuture* Child; xatomic32 Done;} initNotification;
static void initNotified(ptr Data)
{
	initNotification* Note = (initNotification*)Data; xbytesview Ping = INIT_B("PING");
	if (Note->Success) Note->Child = initPing(Note->Client, 5000);
	else {
		xfuture* Output = (xfuture*)(uintptr_t)1;
		testRequire(xredisClientCallAsync(Note->Client, &Ping, 1, 5000, &Output) == XREDIS_ADMISSION_CLOSED && Output == (xfuture*)(uintptr_t)1,
			"failed readiness published before stopping admission");
	}
	testRequire(xredisClientClose(Note->Client), "readiness notification close reentry"); xrtAtomic32Store(&Note->Done, 1, XMEMORY_RELEASE);
}
static void initReentry(void)
{
	for (size_t Trial = 0; Trial < 2; ++Trial) {
		redisinitpeerstep Step = {{INIT_B("HELLO"), INIT_B("3")}, 2, Trial == 0 ? (xbytesview)INIT_B(REDIS_INIT_HELLO_REPLY) : (xbytesview)INIT_B("-WRONGPASS secret\r\n"), true, false};
		redisinitpeer Peer; redisInitPeerStart(&Peer, &Step, 1); xredisclient* Client = initConnect(&Peer, NULL, NULL);
		xfuture* Ready = xredisClientReady(Client); redisInitPeerStepWait(&Peer, 1); initNotification Note = {Client, Trial == 0, NULL, {0}}; xfuturewatch Watch;
		testRequire(xrtFutureWatchInit(&Watch, initNotified, NULL, &Note) && xrtFutureWatchAdd(Ready, &Watch) == XFUTURE_WATCH_PENDING, "ready notification registration");
		redisPeerReleaseBlocked(&Peer.Peer); redisPeerWait(&Note.Done, 1);
		if (Note.Success) {
			const xredisrequestresult* Child = (const xredisrequestresult*)initWait(Note.Child);
			testRequire(Child->Status == XREDIS_REQUEST_CLOSED && Child->Delivery == XREDIS_NOT_SENT, "ready notification sent after reentrant close"); xrtFutureDestroy(Note.Child);
		}
		initClose(&Peer, Client); xrtFutureDestroy(Ready);
	}
}

int main(void)
{
	initSuccess(); initBarrier(); initRejected(); initInterrupted(); initInvalid(); initAllocations(); initWorkerOOM(); initReentry();
	printf("[PASS] xredis managed setup: HELLO3/AUTH/ACL/name/database/RESP2/fallback/barrier/deadline/OOM/close/reentry\n"); return 0;
}
