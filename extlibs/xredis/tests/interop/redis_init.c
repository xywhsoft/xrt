#define XREDIS_MODULE_XREDIS_CLIENT
#include <xredis.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Only run against the disposable service started by run_redis_init.py.
 * These are public synthetic fixture credentials, never application secrets. */
#define INTEROP_PASSWORD "xredis-fixture-password"
#define INTEROP_USERNAME "xredis-fixture-user"
static void interopRequire(bool Condition, cstr Message)
{ if (!Condition) { fprintf(stderr, "[FAIL] Redis interoperability: %s\n", Message); exit(1); } }

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
	interopRequire(xredisClientConnectInit(Engine, Address, 0, NULL, &Init, &Client), "managed connection");
	xfuture* ReadyFuture = xredisClientReady(Client); const xredisreadyresult* Ready = (const xredisreadyresult*)interopWait(ReadyFuture);
	interopRequire(Ready->Status == XREDIS_READY_OK && Ready->Protocol == (Mode == XREDIS_INIT_RESP2 ? XREDIS_RESP2 : XREDIS_RESP3) &&
		!Ready->Downgraded && Ready->Negotiated == (Mode != XREDIS_INIT_RESP2) && (Ready->Hello != NULL) == Ready->Negotiated, "negotiated readiness");
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

static void interopFailure(xnetengine* Engine, const xnetaddr* Address, int Scenario)
{
	for (size_t Trial = 0; Trial < (Scenario == 0 ? 2u : 6u); ++Trial) {
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
		xredisclient* Client = NULL; interopRequire(xredisClientConnectInit(Engine, Address, 0, NULL, &Init, &Client), "failure connection admission");
		xfuture* Future = xredisClientReady(Client); const xredisreadyresult* Ready = (const xredisreadyresult*)interopWait(Future);
		interopRequire(Ready->Status == Expected && Ready->Step == Step && !Ready->Downgraded && Ready->Hello == NULL, "real initialization failure/downgrade");
		if (Ready->Cause != NULL) interopRequire(strcmp(xrtErrorMessage(Ready->Cause), "connection initialization failed") == 0, "redacted error");
		interopClose(Client); xrtFutureDestroy(Future);
	}
}

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

int main(int argc, char** argv)
{
	if (argc != 3) return 2;
	char* End = NULL; unsigned long Port = strtoul(argv[1], &End, 10); if (*End != 0 || Port == 0 || Port > UINT16_MAX) return 2;
	int Scenario = atoi(argv[2]); if (Scenario < 0 || Scenario > 2) return 2;
	xnetengineconfig Config; xrtNetEngineConfigInit(&Config); Config.Workers = 2;
	xnetengine* Engine = xrtNetEngineCreate(&Config); interopRequire(Engine != NULL && xrtNetEngineStart(Engine), "engine start");
	xnetaddr Address; interopRequire(xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, (uint16)Port), "endpoint");
	xfuture* Results[3]; Results[0] = interopSuccess(Engine, &Address, Scenario, XREDIS_INIT_RESP3);
	Results[1] = interopSuccess(Engine, &Address, Scenario, XREDIS_INIT_RESP2); Results[2] = interopSuccess(Engine, &Address, Scenario, XREDIS_INIT_AUTO);
	interopFailure(Engine, &Address, Scenario); interopRaw(Engine, &Address, Scenario);
	interopRequire(xrtNetEngineStop(Engine) && xrtNetEngineDestroy(Engine), "engine teardown");
	for (size_t Index = 0; Index < 3; ++Index) {
		const xredisreadyresult* Ready = (const xredisreadyresult*)interopWait(Results[Index]);
		interopRequire(Ready->Status == XREDIS_READY_OK && Ready->Step == XREDIS_INIT_DONE, "ready metadata survived engine teardown"); xrtFutureDestroy(Results[Index]);
	}
	printf("[PASS] real Redis initialization scenario=%d: protocol/auth/name/database/binary/pipeline/failure/raw/teardown\n", Scenario); return 0;
}
