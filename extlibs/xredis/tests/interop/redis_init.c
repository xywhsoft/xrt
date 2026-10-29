#if defined(INTEROP_TLS_AUTH)
#define INTEROP_TLS
#define XREDIS_MODULE_XREDIS_TLS_AUTH
#elif defined(INTEROP_TLS)
#define XREDIS_MODULE_XREDIS_TLS
#elif defined(INTEROP_DIAL)
#define XREDIS_MODULE_XREDIS_DIAL
#else
#define XREDIS_MODULE_XREDIS_CLIENT
#endif
#if defined(INTEROP_TRANSACTION)
#define XREDIS_MODULE_XREDIS_TRANSACTION
#endif
#if defined(INTEROP_WATCH)
#define XREDIS_MODULE_XREDIS_WATCH
#endif
#include <xredis.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Only run against the disposable service started by run_redis_init.py.
 * These are public synthetic fixture credentials, never application secrets. */
#define INTEROP_PASSWORD "xredis-fixture-password"
#define INTEROP_USERNAME "xredis-fixture-user"
#if defined(INTEROP_DIAL) || defined(INTEROP_TLS)
static xnetresolver* InteropResolver;
#endif
#if defined(INTEROP_TLS)
static xredisclienttlsconfig InteropTLS;
#endif
#if defined(INTEROP_TLS_AUTH)
static bool interopAuthConnect(xnetengine*, const xnetaddr*, const xredisclientinitconfig*, const xredisclienttlsconfig*, xredisclient**);
#endif
static bool interopConnect(xnetengine* Engine, const xnetaddr* Address, const xredisclientinitconfig* Init, xredisclient** Client)
{
#if defined(INTEROP_TLS_AUTH)
	return interopAuthConnect(Engine, Address, Init, &InteropTLS, Client);
#elif defined(INTEROP_TLS)
	return xredisClientTlsDial(Engine, InteropResolver, XRT_STR_LITERAL("localhost"), Address->Port, 0, NULL, Init, &InteropTLS, Client);
#elif defined(INTEROP_DIAL)
	return xredisClientDial(Engine, InteropResolver, XRT_STR_LITERAL("localhost"), Address->Port, 0, NULL, Init, NULL, Client);
#else
	return xredisClientConnectInit(Engine, Address, 0, NULL, Init, Client);
#endif
}
static void interopRequire(bool Condition, cstr Message)
{ if (!Condition) { fprintf(stderr, "[FAIL] Redis interoperability: %s\n", Message); exit(1); } }
#if defined(INTEROP_TLS_AUTH)
#include "tls_auth_support.h"
#endif

static const void* interopWait(xfuture* Future)
{
	interopRequire(Future != NULL && xrtFutureWaitFor(Future, 10000) == XWAIT_OK, "future wait"); xfutureresult Value;
	interopRequire(xrtFutureResult(Future, &Value) && Value.State == XFUTURE_RESOLVED && Value.Value != NULL, "typed outcome"); return Value.Value;
}

static void interopClose(xredisclient* Client)
{
	xfuture* Closed = xredisClientClosed(Client); interopRequire(xredisClientClose(Client) && xrtFutureWaitFor(Closed, 10000) == XWAIT_OK, "physical close");
	xredisclientinfo Info = {0}; Info.Size = sizeof(Info);
	interopRequire(xredisClientInfo(Client, &Info) && Info.InitBytes == 0 && Info.Requests == 0 && Info.Accepted == Info.Completed, "wire/requests retained at close");
	xredisClientRelease(Client); xrtFutureDestroy(Closed);
}

static xfuture* interopCall(xredisclient* Client, const xbytesview* Args, size_t Count)
{
	xfuture* Future = NULL;
	interopRequire(xredisClientCall(Client, Args, Count, 5000, &Future) == XREDIS_ADMISSION_ACCEPTED, "call admission");
	const xredisrequestresult* Value = (const xredisrequestresult*)interopWait(Future);
	interopRequire(Value->Status == XREDIS_REQUEST_OK && Value->Delivery == XREDIS_REPLIED, "server command result"); return Future;
}

static xredisreplyvalue interopValue(xfuture* Future)
{
	const xredisrequestresult* Value = (const xredisrequestresult*)interopWait(Future);
	xredisreplyvalue Root; xredisreplyinfo Info = {0}; Info.Size = sizeof(Info);
	interopRequire(xredisReplyInfo(Value->Reply, &Info) && xredisReplyGet(Value->Reply, Info.Root, &Root), "root reply"); return Root;
}

static void interopOptions(xredisclientinitconfig* Init, int Scenario)
{
	xredisClientInitConfigInit(Init); Init->Flags = XREDIS_INIT_SETNAME | XREDIS_INIT_SELECT;
	Init->Name = (xbytesview)XRT_BYTES_LITERAL("xredis-interop"); Init->Database = 3;
	if (Scenario != 0) { Init->Flags |= XREDIS_INIT_AUTH; Init->Password = (xbytesview)XRT_BYTES_LITERAL(INTEROP_PASSWORD); }
	if (Scenario == 2) { Init->Flags |= XREDIS_INIT_USERNAME; Init->Username = (xbytesview)XRT_BYTES_LITERAL(INTEROP_USERNAME); }
}

static xfuture* interopSuccess(xnetengine* Engine, const xnetaddr* Address, int Scenario, xredisinitmode Mode)
{
	xredisclientinitconfig Init; interopOptions(&Init, Scenario); Init.Mode = Mode; xredisclient* Client = NULL;
#if defined(INTEROP_TLS_AUTH)
	uint32 Signs = xrtAtomic32Load(&InteropSigns, XMEMORY_ACQUIRE), Queries = xrtAtomic32Load(&InteropQueries, XMEMORY_ACQUIRE);
#endif
	interopRequire(interopConnect(Engine, Address, &Init, &Client), "managed connection");
	xfuture* ReadyFuture = xredisClientReady(Client); const xredisreadyresult* Ready = (const xredisreadyresult*)interopWait(ReadyFuture);
	if (Ready->Status != XREDIS_READY_OK)
		fprintf(stderr, "readiness failure: mode=%d status=%d step=%d protocol=%d cause=%s\n",
			(int)Mode, (int)Ready->Status, (int)Ready->Step, (int)Ready->Protocol,
			Ready->Cause == NULL ? "none" : xrtErrorMessage(Ready->Cause));
	interopRequire(Ready->Status == XREDIS_READY_OK && Ready->Protocol == (Mode == XREDIS_INIT_RESP2 ? XREDIS_RESP2 : XREDIS_RESP3) &&
		!Ready->Downgraded && Ready->Negotiated == (Mode != XREDIS_INIT_RESP2) && (Ready->Hello != NULL) == Ready->Negotiated, "negotiated readiness");
#if defined(INTEROP_TLS_AUTH)
	interopRequire(xrtAtomic32Load(&InteropSigns, XMEMORY_ACQUIRE) == Signs + 1u &&
		xrtAtomic32Load(&InteropQueries, XMEMORY_ACQUIRE) == Queries + 1u, "accepted peer signs exactly once");
#endif
	xbytesview GetName[] = {XRT_BYTES_LITERAL("CLIENT"), XRT_BYTES_LITERAL("GETNAME")};
	xfuture* Future = interopCall(Client, GetName, 2); xredisreplyvalue Value = interopValue(Future);
	interopRequire(Value.Type == XREDIS_REPLY_BULK_STRING && Value.Data.Size == Init.Name.Size && memcmp(Value.Data.Data, Init.Name.Data, Init.Name.Size) == 0,
		"configured client name not visible to Redis"); xrtFutureDestroy(Future);
	xbytesview Info[] = {XRT_BYTES_LITERAL("CLIENT"), XRT_BYTES_LITERAL("INFO")}; Future = interopCall(Client, Info, 2); Value = interopValue(Future);
	bool Database = false;
	for (size_t Index = 0; Index + 6u <= Value.Data.Size; ++Index) if (memcmp(Value.Data.Data + Index, " db=3 ", 6) == 0) Database = true;
	interopRequire(Database, "configured database not visible to Redis"); xrtFutureDestroy(Future);
	xbytesview Set[] = {XRT_BYTES_LITERAL("SET"), XRT_BYTES_LITERAL("key\0binary"), XRT_BYTES_LITERAL("v\0\r\n\xff")};
	Future = interopCall(Client, Set, 3); xrtFutureDestroy(Future);
	xbytesview Get[] = {XRT_BYTES_LITERAL("GET"), Set[1]}; Future = interopCall(Client, Get, 2); Value = interopValue(Future);
	interopRequire(Value.Data.Size == Set[2].Size && memcmp(Value.Data.Data, Set[2].Data, Set[2].Size) == 0, "binary SET/GET"); xrtFutureDestroy(Future);
	xbytesview Ping = XRT_BYTES_LITERAL("PING"); xfuture* Pipeline[32];
	for (size_t Index = 0; Index < 32; ++Index)
		interopRequire(xredisClientCallAsync(Client, &Ping, 1, 5000, &Pipeline[Index]) == XREDIS_ADMISSION_ACCEPTED, "real pipeline admission");
	for (size_t Index = 0; Index < 32; ++Index) {
		const xredisrequestresult* Result = (const xredisrequestresult*)interopWait(Pipeline[Index]);
		interopRequire(Result->Status == XREDIS_REQUEST_OK, "real pipeline correlation"); xrtFutureDestroy(Pipeline[Index]);
	}
	interopClose(Client); return ReadyFuture;
}

#if defined(INTEROP_TRANSACTION)
#include "transaction_support.h"
#endif
#if defined(INTEROP_WATCH)
#include "watch_support.h"
#endif

static void interopFailure(xnetengine* Engine, const xnetaddr* Address, int Scenario)
{
	for (size_t Trial = 0; Trial < (Scenario == 0 ? 2u : 6u); ++Trial) {
#if defined(INTEROP_TLS_AUTH)
		uint32 Signs = xrtAtomic32Load(&InteropSigns, XMEMORY_ACQUIRE), Queries = xrtAtomic32Load(&InteropQueries, XMEMORY_ACQUIRE);
#endif
		xredisclientinitconfig Init; interopOptions(&Init, Scenario);
		xredisreadystatus Expected = XREDIS_READY_SERVER_ERROR; xredisinitstep Step = XREDIS_INIT_HELLO;
		if (Trial == 0) Init.Name = (xbytesview)XRT_BYTES_LITERAL("invalid name");
		if (Trial == 1) { Init.Database = UINT32_MAX; Step = XREDIS_INIT_DATABASE; }
		if (Trial >= 2) {
			Expected = XREDIS_READY_AUTH_FAILED; Init.Mode = Trial == 3 ? XREDIS_INIT_RESP2 : XREDIS_INIT_AUTO;
			if (Init.Mode == XREDIS_INIT_RESP2) Step = XREDIS_INIT_AUTHENTICATE;
			Init.Password = (xbytesview)XRT_BYTES_LITERAL("wrong-fixture-password");
			if (Trial == 4) { Init.Flags |= XREDIS_INIT_USERNAME; Init.Username = (xbytesview)XRT_BYTES_LITERAL("missing-fixture-user"); }
			if (Trial == 5) {
				Init.Flags &= ~(XREDIS_INIT_AUTH | XREDIS_INIT_USERNAME); Init.Password = (xbytesview){NULL, 0}; Init.Username = (xbytesview){NULL, 0};
			}
		}
		xredisclient* Client = NULL; interopRequire(interopConnect(Engine, Address, &Init, &Client), "failure connection admission");
		xfuture* Future = xredisClientReady(Client); const xredisreadyresult* Ready = (const xredisreadyresult*)interopWait(Future);
		interopRequire(Ready->Status == Expected && Ready->Step == Step && !Ready->Downgraded && Ready->Hello == NULL, "real initialization failure/downgrade");
#if defined(INTEROP_TLS_AUTH)
		interopRequire(xrtAtomic32Load(&InteropSigns, XMEMORY_ACQUIRE) == Signs + 1u &&
			xrtAtomic32Load(&InteropQueries, XMEMORY_ACQUIRE) == Queries + 1u, "Redis rejection does not replay TLS signing");
#endif
		if (Ready->Cause != NULL) interopRequire(strcmp(xrtErrorMessage(Ready->Cause), "connection initialization failed") == 0, "redacted error");
		interopClose(Client); xrtFutureDestroy(Future);
	}
}

#if !defined(INTEROP_TLS)
static void interopRaw(xnetengine* Engine, const xnetaddr* Address, int Scenario)
{
	xredisclient* Client = NULL; interopRequire(xredisClientConnect(Engine, Address, 0, NULL, &Client), "raw connection");
	xfuture* ReadyFuture = xredisClientReady(Client); const xredisreadyresult* Ready = (const xredisreadyresult*)interopWait(ReadyFuture);
	interopRequire(Ready->Status == XREDIS_READY_OK && Ready->Protocol == XREDIS_PROTOCOL_UNKNOWN && !Ready->Negotiated, "raw transport readiness");
	xbytesview Args[5] = {XRT_BYTES_LITERAL("HELLO"), XRT_BYTES_LITERAL("3"), XRT_BYTES_LITERAL("AUTH"),
		Scenario == 2 ? (xbytesview)XRT_BYTES_LITERAL(INTEROP_USERNAME) : (xbytesview)XRT_BYTES_LITERAL("default"), XRT_BYTES_LITERAL(INTEROP_PASSWORD)};
	xfuture* Hello = interopCall(Client, Args, Scenario == 0 ? 2 : 5); xredisreplyvalue Root = interopValue(Hello);
	interopRequire(Root.Type == XREDIS_REPLY_MAP, "raw manual HELLO control"); xrtFutureDestroy(Hello); interopClose(Client); xrtFutureDestroy(ReadyFuture);
}
#else
static xtlsverifier* interopTrust(cstr Path)
{
	FILE* File = fopen(Path, "rb"); interopRequire(File != NULL, "open fixture trust");
	unsigned char Data[65536]; size_t Size = fread(Data, 1, sizeof(Data), File);
	interopRequire(Size != 0 && Size < sizeof(Data) && !ferror(File) && fclose(File) == 0, "read bounded trust PEM");
	xx509store* Store = xrtX509StoreCreate(); interopRequire(Store != NULL && xrtX509StoreAddData(Store, (xbytesview){Data, Size}, NULL), "trust import");
	xtlsverifierconfig Config; xrtTlsVerifierConfigInit(&Config); Config.Store = Store; xtlsverifylimits Limits; xrtTlsVerifyLimitsInit(&Limits);
	xtlsverifier* Verifier = xrtTlsVerifierCreateLimited(&Config, &Limits); xrtX509StoreFree(Store);
	interopRequire(Verifier != NULL, "limited default verifier"); return Verifier;
}
static void interopTlsRejected(xnetengine* Engine, const xnetaddr* Address, xtlsverifier* Wrong)
{
	for (size_t Trial = 0; Trial < 2; ++Trial) {
		xredisclienttlsconfig Bad = InteropTLS;
		if (Trial == 0) Bad.Session.VerifyName = (xstrview)XRT_STR_LITERAL("wrong.fixture"); else Bad.Session.Verifier = Wrong;
#if defined(INTEROP_TLS_AUTH)
		uint32 Before = xrtAtomic32Load(&InteropSigns, XMEMORY_ACQUIRE);
		xredisclient* Client = NULL; interopRequire(interopAuthConnect(Engine, Address, NULL, &Bad, &Client), "rejected auth TLS admission");
#else
		xredisclient* Client = NULL; interopRequire(xredisClientTlsDial(Engine, InteropResolver, XRT_STR_LITERAL("localhost"), Address->Port,
			0, NULL, NULL, &Bad, &Client), "rejected TLS admission");
#endif
		xfuture* Ready = xredisClientReady(Client), *Request = NULL; xbytesview Ping = XRT_BYTES_LITERAL("PING");
		interopRequire(xredisClientCallAsync(Client, &Ping, 1, 5000, &Request) == XREDIS_ADMISSION_ACCEPTED, "queued rejected TLS request");
		const xredisreadyresult* State = (const xredisreadyresult*)interopWait(Ready);
		const xredisrequestresult* Result = (const xredisrequestresult*)interopWait(Request);
		interopRequire(State->Status == XREDIS_READY_DISCONNECTED && State->Hello == NULL && State->Cause != NULL && Result->Delivery == XREDIS_NOT_SENT,
			"untrusted/wrong-name certificate accepted or leaked business");
#if defined(INTEROP_TLS_AUTH)
		interopRequire(xrtAtomic32Load(&InteropSigns, XMEMORY_ACQUIRE) == Before, "bad server invoked private client signer");
#endif
		interopClose(Client); xrtFutureDestroy(Ready); xrtFutureDestroy(Request);
	}
}
#endif

#if defined(INTEROP_TLS_AUTH)
static void interopAuthRejected(xnetengine* Engine, const xnetaddr* Address, xtlsidentity* Wrong)
{
	for (unsigned Mode = 0; Mode < 4u; Mode++) {
		uint32 Signs = xrtAtomic32Load(&InteropSigns, XMEMORY_ACQUIRE), Queries = xrtAtomic32Load(&InteropQueries, XMEMORY_ACQUIRE);
		xtlsidentity* Original = InteropIdentity; InteropMissing = Mode == 0u; InteropCorrupt = Mode == 2u;
		InteropMemory = Mode == 3u;
		if (Mode == 1u) InteropIdentity = Wrong;
		xredisclient* Client = NULL; interopRequire(interopConnect(Engine, Address, NULL, &Client), "bad client authentication admission");
		xfuture* Ready = xredisClientReady(Client), *Request = NULL; xbytesview Ping = XRT_BYTES_LITERAL("PING");
		interopRequire(xredisClientCallAsync(Client, &Ping, 1, 5000, &Request) == XREDIS_ADMISSION_ACCEPTED, "bad client queued business");
		const xredisreadyresult* State = interopWait(Ready); const xredisrequestresult* Result = interopWait(Request);
		interopRequire(State->Status == (Mode == 3u ? XREDIS_READY_MEMORY : XREDIS_READY_DISCONNECTED) && !State->Hello && Result->Delivery == XREDIS_NOT_SENT,
			"missing/untrusted/corrupt client proof published managed readiness or sent business");
		uint32 Calls = xrtAtomic32Load(&InteropSigns, XMEMORY_ACQUIRE) - Signs;
		interopRequire(Calls <= 1u && (Mode != 0u || !Calls) && (Mode < 2u || Calls == 1u) &&
			xrtAtomic32Load(&InteropQueries, XMEMORY_ACQUIRE) == Queries + Calls, "failed proof cannot repeat a signing call");
		interopClose(Client); xrtFutureDestroy(Ready); xrtFutureDestroy(Request);
		InteropIdentity = Original; InteropMissing = InteropCorrupt = InteropMemory = false;
	}
}
#endif

int main(int argc, char** argv)
{
	if (argc !=
#if defined(INTEROP_TLS_AUTH)
		10
#elif defined(INTEROP_TLS)
		6
#else
		3
#endif
	) return 2;
	char* End = NULL; unsigned long Port = strtoul(argv[1], &End, 10); if (*End != 0 || Port == 0 || Port > UINT16_MAX) return 2;
	int Scenario = atoi(argv[2]); if (Scenario < 0 || Scenario > 2) return 2;
#if defined(INTEROP_TLS)
	int Version = atoi(argv[5]); if (Version != 12 && Version != 13) return 2;
	xtlsverifier* Trust = interopTrust(argv[3]), *Wrong = interopTrust(argv[4]);
	xtlspolicy Policy; xrtTlsPolicyInit(&Policy); xtlsversion Versions[] = {Version == 12 ? XTLS_VERSION_12 : XTLS_VERSION_13};
	xtlscipher Ciphers[] = {Version == 12 ? XTLS_ECDHE_RSA_AES_128_GCM_SHA256 : XTLS_AES_128_GCM_SHA256};
	xtlssignature Signatures[] = {XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256
#if defined(INTEROP_TLS_AUTH)
		, XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256, XTLS_SIGNATURE_ECDSA_SECP384R1_SHA384, XTLS_SIGNATURE_ED25519
#endif
	};
	Policy.Versions = Versions; Policy.VersionCount = 1; Policy.Ciphers = Ciphers; Policy.CipherCount = 1;
	Policy.Signatures = Signatures; Policy.SignatureCount = sizeof(Signatures) / sizeof(Signatures[0]);
	xtlscontextconfig ContextConfig; xrtTlsContextConfigInit(&ContextConfig); ContextConfig.Policy = &Policy;
	xtlscontext* Context = xrtTlsContextCreate(&ContextConfig);
	if (Context == NULL) fprintf(stderr, "TLS context policy: %s\n", xrtErrorMessage(xrtGetError()));
	interopRequire(Context != NULL, "TLS version policy");
	xredisClientTlsConfigInit(&InteropTLS); InteropTLS.Session.Context = Context; InteropTLS.Session.Verifier = Trust;
#if defined(INTEROP_TLS_AUTH)
	InteropIdentity = xredisExampleAuthIdentity(argv[6], argv[7]); xtlsidentity* WrongIdentity = xredisExampleAuthIdentity(argv[8], argv[9]);
	interopRequire(InteropIdentity && WrongIdentity, "bounded runtime client identity/key import");
#endif
#endif
	xnetengineconfig Config; xrtNetEngineConfigInit(&Config); Config.Workers = 2;
	xnetengine* Engine = xrtNetEngineCreate(&Config); interopRequire(Engine != NULL && xrtNetEngineStart(Engine), "engine start");
#if defined(INTEROP_DIAL) || defined(INTEROP_TLS)
	InteropResolver = xrtNetResolverCreate(NULL); interopRequire(InteropResolver != NULL, "application Resolver");
#if defined(INTEROP_TLS)
	printf("connection_mode: verified TLS 1.%d / localhost / default chain and identity verifier\n", Version - 10);
#else
	puts("connection_mode: hostname localhost / shared system Resolver");
#endif
#endif
	xnetaddr Address; interopRequire(xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, (uint16)Port), "endpoint");
	xfuture* Results[3]; Results[0] = interopSuccess(Engine, &Address, Scenario, XREDIS_INIT_RESP3);
	Results[1] = interopSuccess(Engine, &Address, Scenario, XREDIS_INIT_RESP2); Results[2] = interopSuccess(Engine, &Address, Scenario, XREDIS_INIT_AUTO);
#if defined(INTEROP_TRANSACTION)
	interopTransactions(Engine, &Address, Scenario);
#endif
#if defined(INTEROP_WATCH)
	interopWatches(Engine, &Address, Scenario);
#endif
	interopFailure(Engine, &Address, Scenario);
#if defined(INTEROP_TLS)
	interopTlsRejected(Engine, &Address, Wrong);
#if defined(INTEROP_TLS_AUTH)
	interopAuthRejected(Engine, &Address, WrongIdentity);
#endif
#else
	interopRaw(Engine, &Address, Scenario);
#endif
#if defined(INTEROP_DIAL) || defined(INTEROP_TLS)
	interopRequire(xrtNetResolverDestroy(InteropResolver), "shared Resolver join");
#endif
	interopRequire(xrtNetEngineStop(Engine) && xrtNetEngineDestroy(Engine), "engine teardown");
#if defined(INTEROP_TLS)
	xrtTlsVerifierRelease(Trust); xrtTlsVerifierRelease(Wrong); xrtTlsContextRelease(Context);
#if defined(INTEROP_TLS_AUTH)
	xrtTlsIdentityRelease(InteropIdentity); xrtTlsIdentityRelease(WrongIdentity);
	uint32 Signs = xrtAtomic32Load(&InteropSigns, XMEMORY_ACQUIRE), Queries = xrtAtomic32Load(&InteropQueries, XMEMORY_ACQUIRE);
	interopRequire(Signs >= 3u && Signs == Queries && xrtAtomic32Load(&InteropCreated, XMEMORY_ACQUIRE) == xrtAtomic32Load(&InteropDropped, XMEMORY_ACQUIRE),
		"real proofs/sign-once counts or signer retirement");
	printf("client_auth: nonnull-sign-calls=%u queries=%u released-identities=%u\n", Signs, Queries, xrtAtomic32Load(&InteropDropped, XMEMORY_ACQUIRE));
#endif
#endif
	for (size_t Index = 0; Index < 3; ++Index) {
		const xredisreadyresult* Ready = (const xredisreadyresult*)interopWait(Results[Index]);
		interopRequire(Ready->Status == XREDIS_READY_OK && Ready->Step == XREDIS_INIT_DONE, "ready metadata survived engine teardown"); xrtFutureDestroy(Results[Index]);
	}
	printf("[PASS] real Redis initialization scenario=%d: protocol/auth/name/database/binary/pipeline/failure/teardown\n", Scenario); return 0;
}
