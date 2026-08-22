#include "../../../dev/bench/bench_common.h"

#include <xws.h>
#include "../../../dev/bench/network/bench_network_common.h"



#define BENCH_WS_TIMEOUT UINT64_C(5000000)



/* 每个端点的消息暂存只由其所属网络 Worker 修改。 */
typedef struct benchwsendpoint {
	xatomic32 Messages;
	xatomic32 Closed;
	size_t Size;
	xwsopcode Opcode;
	xwsconnclose Close;
} benchwsendpoint;



/* 单会话基准共享不可变负载和跨 Worker 原子终态。 */
typedef struct benchwscontext {
	const uint8* Payload;
	size_t PayloadSize;
	benchwsendpoint Client;
	benchwsendpoint Server;
	xatomic32 Errors;
	xatomic32 Backpressure;
	xatomicptr ServerConnection;
	xwsconnevents Events;
} benchwscontext;



/* 按 Connection 角色取得只由对应 Worker 写入的端点状态。 */
static benchwsendpoint* benchWsEndpoint(
	benchwscontext* pContext,
	xwsconn* pConnection
)
{
	return (xrtWsConnRole(pConnection) == XWS_ROLE_CLIENT) ?
		&pContext->Client : &pContext->Server;
}



/* 发布一个可由主线程观察的 WebSocket 基准错误。 */
static void benchWsFail(benchwscontext* pContext)
{
	(void)xrtAtomic32FetchAdd(
		&pContext->Errors,
		1,
		XMEMORY_RELEASE
	);
}



/* 每条逻辑消息必须是新的 Binary 消息。 */
static void benchWsMessageBegin(
	xwsconn* pConnection,
	const xwsmessageinfo* pInfo,
	ptr pData
)
{
	benchwscontext* pContext = (benchwscontext*)pData;
	benchwsendpoint* pEndpoint = benchWsEndpoint(pContext, pConnection);

	if (
		(pInfo == NULL) ||
		(pInfo->Opcode != XWS_OPCODE_BINARY)
	) {
		benchWsFail(pContext);
		return;
	}
	pEndpoint->Opcode = (xwsopcode)pInfo->Opcode;
	pEndpoint->Size = 0;
}



/* 流式校验消息分块，不为回环负载分配额外缓冲。 */
static void benchWsMessageData(
	xwsconn* pConnection,
	xbytesview Data,
	ptr pData
)
{
	benchwscontext* pContext = (benchwscontext*)pData;
	benchwsendpoint* pEndpoint = benchWsEndpoint(pContext, pConnection);

	if (
		(pEndpoint->Size > pContext->PayloadSize) ||
		(Data.Size > (pContext->PayloadSize - pEndpoint->Size)) ||
		((Data.Size != 0) &&
		 (memcmp(
			Data.Data,
			pContext->Payload + pEndpoint->Size,
			Data.Size
		  ) != 0))
	) {
		benchWsFail(pContext);
		return;
	}
	pEndpoint->Size += Data.Size;
}



/* 服务端完整接收后回显，客户端完整接收后发布本轮完成。 */
static void benchWsMessageEnd(xwsconn* pConnection, ptr pData)
{
	benchwscontext* pContext = (benchwscontext*)pData;
	benchwsendpoint* pEndpoint = benchWsEndpoint(pContext, pConnection);

	if (
		(pEndpoint->Opcode != XWS_OPCODE_BINARY) ||
		(pEndpoint->Size != pContext->PayloadSize)
	) {
		benchWsFail(pContext);
		return;
	}
	if (
		(xrtWsConnRole(pConnection) == XWS_ROLE_SERVER) &&
		(xrtWsConnBinary(
			pConnection,
			(xbytesview) {
				pContext->Payload,
				pContext->PayloadSize
			}
		 ) != XNET_RESULT_OK)
	) {
		benchWsFail(pContext);
		return;
	}
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Messages,
		1,
		XMEMORY_RELEASE
	);
}



/* 顺序 ping-pong 不应触发发送侧背压。 */
static void benchWsBackpressure(
	xwsconn* pConnection,
	size_t iPending,
	ptr pData
)
{
	benchwscontext* pContext = (benchwscontext*)pData;

	(void)pConnection;
	(void)iPending;
	(void)xrtAtomic32FetchAdd(
		&pContext->Backpressure,
		1,
		XMEMORY_RELEASE
	);
}



/* WebSocket 运行时错误会使当前性能样本无效。 */
static void benchWsError(
	xwsconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	(void)pConnection;
	(void)pError;
	benchWsFail((benchwscontext*)pData);
}



/* 保存双端唯一关闭快照并发布关闭完成。 */
static void benchWsClose(
	xwsconn* pConnection,
	const xwsconnclose* pClose,
	ptr pData
)
{
	benchwscontext* pContext = (benchwscontext*)pData;
	benchwsendpoint* pEndpoint = benchWsEndpoint(pContext, pConnection);

	if ( pClose == NULL ) {
		benchWsFail(pContext);
		return;
	}
	pEndpoint->Close = *pClose;
	xrtAtomic32Store(&pEndpoint->Closed, 1, XMEMORY_RELEASE);
}



/* 服务端 Upgrade 成功后交付其拥有的 Connection 引用。 */
static void benchWsServerDone(
	xhttpconn* pHttp,
	xnetresult Result,
	xwsconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	benchwscontext* pContext = (benchwscontext*)pData;

	(void)pHttp;
	(void)pError;
	if (
		(Result != XNET_RESULT_OK) ||
		(pConnection == NULL)
	) {
		benchWsFail(pContext);
		return;
	}
	xrtAtomicPtrStore(
		&pContext->ServerConnection,
		pConnection,
		XMEMORY_RELEASE
	);
}



/* 把唯一 HTTP 请求升级为无扩展 WebSocket 会话。 */
static void benchWsRequest(
	xhttpserver* pServer,
	xhttpconn* pHttp,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	benchwscontext* pContext = (benchwscontext*)pData;
	xwsserverconfig Config;
	xstrview Target = xrtHttpServerRequestTarget(pRequest);

	(void)pServer;
	if (
		(Target.Size != 6u) ||
		(memcmp(Target.Data, "/bench", 6u) != 0)
	) {
		benchWsFail(pContext);
		return;
	}
	xrtWsServerConfigInit(&Config);
	Config.Connection.MessageLimit = pContext->PayloadSize;
	Config.Connection.FrameLimit = pContext->PayloadSize;
	if ( xrtWsUpgrade(
		pHttp,
		&Config,
		&pContext->Events,
		pContext,
		benchWsServerDone,
		pContext
	) != XNET_RESULT_OK ) {
		benchWsFail(pContext);
	}
}



/* HTTP 层错误会使 Upgrade 性能样本无效。 */
static void benchWsHttpError(
	xhttpserver* pServer,
	xhttpconn* pHttp,
	const xerror* pError,
	ptr pData
)
{
	(void)pServer;
	(void)pHttp;
	(void)pError;
	benchWsFail((benchwscontext*)pData);
}



/* 在截止时间内等待一个单调原子计数达到目标。 */
static bool benchWsWaitCount(const xatomic32* pValue, uint32 iExpected)
{
	xdeadline Deadline = xrtDeadlineAfter(BENCH_WS_TIMEOUT);

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		if ( xrtDeadlineExpired(Deadline) ) {
			return false;
		}
		xrtThreadYield();
	}
	return true;
}



/* 在截止时间内等待服务端 Upgrade 发布 Connection。 */
static xwsconn* benchWsWaitServerConnection(benchwscontext* pContext)
{
	xdeadline Deadline = xrtDeadlineAfter(BENCH_WS_TIMEOUT);
	xwsconn* pConnection;

	for ( ;; ) {
		pConnection = (xwsconn*)xrtAtomicPtrLoad(
			&pContext->ServerConnection,
			XMEMORY_ACQUIRE
		);
		if ( pConnection != NULL ) {
			return pConnection;
		}
		if ( xrtDeadlineExpired(Deadline) ) {
			return NULL;
		}
		xrtThreadYield();
	}
}



/* 等待 Future 成功终结，不借用其可选值。 */
static bool benchWsFutureResolved(xfuture* pFuture)
{
	return
		(pFuture != NULL) &&
		(xrtFutureWaitFor(pFuture, BENCH_WS_TIMEOUT) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_RESOLVED);
}



/* 在截止时间内等待 HTTP Server 进入关闭态。 */
static bool benchWsWaitServerClosed(xhttpserver* pServer)
{
	xdeadline Deadline = xrtDeadlineAfter(BENCH_WS_TIMEOUT);

	while ( xrtHttpServerState(pServer) != XHTTP_SERVER_CLOSED ) {
		if ( xrtDeadlineExpired(Deadline) ) {
			return false;
		}
		xrtThreadYield();
	}
	return true;
}



/* 测量一次 Upgrade 和单连接上的顺序二进制消息回环。 */
static bool benchWsLoopback(uint32 iIterations, size_t iPayloadSize)
{
	benchwscontext Context;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpserverevents ServerEvents;
	xhttpclientconfig ClientConfig;
	xwsclientconfig WsConfig;
	xhttpserverstats ServerStats;
	xnetenginestats EngineStats;
	xnetengine* pEngine = NULL;
	xhttpserver* pServer = NULL;
	xhttpclient* pClient = NULL;
	xwsopenresult* pOpen = NULL;
	xhttpresponse* pResponse = NULL;
	xwsconn* pClientConnection = NULL;
	xwsconn* pServerConnection = NULL;
	xfuture* pFuture = NULL;
	uint8* pPayload = NULL;
	uint64* pLatencies = NULL;
	xnetaddr Address;
	xbenchtimer SetupTimer;
	xbenchtimer RunTimer;
	uint64 iSetupElapsed = 0;
	uint64 iRunElapsed = 0;
	char sUrl[128];
	int iUrlLength;
	cstr sStage = "allocate";
	bool bResult = false;
	bool bClean = true;

	memset(&Context, 0, sizeof(Context));
	memset(&ServerStats, 0, sizeof(ServerStats));
	memset(&EngineStats, 0, sizeof(EngineStats));
	pPayload = (uint8*)malloc(iPayloadSize);
	pLatencies = (uint64*)calloc(iIterations, sizeof(uint64));
	if ( (pPayload == NULL) || (pLatencies == NULL) ) {
		goto cleanup;
	}
	for ( size_t i = 0; i < iPayloadSize; i++ ) {
		pPayload[i] = (uint8)((i * 17u) + 11u);
	}
	Context.Payload = pPayload;
	Context.PayloadSize = iPayloadSize;
	xrtAtomic32Init(&Context.Client.Messages, 0);
	xrtAtomic32Init(&Context.Client.Closed, 0);
	xrtAtomic32Init(&Context.Server.Messages, 0);
	xrtAtomic32Init(&Context.Server.Closed, 0);
	xrtAtomic32Init(&Context.Errors, 0);
	xrtAtomic32Init(&Context.Backpressure, 0);
	xrtAtomicPtrInit(&Context.ServerConnection, NULL);
	memset(&Context.Events, 0, sizeof(Context.Events));
	Context.Events.MessageBegin = benchWsMessageBegin;
	Context.Events.MessageData = benchWsMessageData;
	Context.Events.MessageEnd = benchWsMessageEnd;
	Context.Events.Backpressure = benchWsBackpressure;
	Context.Events.Error = benchWsError;
	Context.Events.Close = benchWsClose;

	sStage = "engine";
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if (
		(pEngine == NULL) ||
		!xrtNetEngineStart(pEngine) ||
		!xbenchPrintNetworkBackend(pEngine)
	) {
		goto cleanup;
	}

	sStage = "server";
	xrtHttpServerConfigInit(&ServerConfig);
	if ( !xrtNetAddrLoopback(
		&ServerConfig.Network.Listen.Address,
		XNET_FAMILY_IPV4,
		0
	) ) {
		goto cleanup;
	}
	xrtHttpServerEventsInit(&ServerEvents);
	ServerEvents.Request = benchWsRequest;
	ServerEvents.Error = benchWsHttpError;
	ServerEvents.Data = &Context;
	pServer = xrtHttpServerStart(pEngine, &ServerConfig, &ServerEvents);
	if (
		(pServer == NULL) ||
		!xrtHttpServerLocal(pServer, 0, &Address)
	) {
		goto cleanup;
	}

	sStage = "client";
	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Dial.MaxAttempts = 1;
	pClient = xrtHttpClientCreate(pEngine, &ClientConfig);
	if ( pClient == NULL ) {
		goto cleanup;
	}
	iUrlLength = snprintf(
		sUrl,
		sizeof(sUrl),
		"ws://127.0.0.1:%u/bench",
		(unsigned int)Address.Port
	);
	if (
		(iUrlLength <= 0) ||
		((size_t)iUrlLength >= sizeof(sUrl))
	) {
		goto cleanup;
	}

	sStage = "upgrade";
	xrtWsClientConfigInit(&WsConfig);
	WsConfig.Connection.MessageLimit = iPayloadSize;
	WsConfig.Connection.FrameLimit = iPayloadSize;
	xbenchTimerStart(&SetupTimer);
	pOpen = xrtWsConnectSync(
		pClient,
		(xstrview) { sUrl, (size_t)iUrlLength },
		&WsConfig,
		&Context.Events,
		&Context
	);
	if ( pOpen == NULL ) {
		goto cleanup;
	}
	pClientConnection = xrtWsOpenResultTakeConnection(pOpen);
	pResponse = xrtWsOpenResultTakeResponse(pOpen);
	pServerConnection = benchWsWaitServerConnection(&Context);
	xbenchTimerStop(&SetupTimer);
	iSetupElapsed = xbenchTimerElapsedNs(&SetupTimer);
	if (
		(pClientConnection == NULL) ||
		(pServerConnection == NULL) ||
		(pResponse == NULL) ||
		(xrtHttpResponseStatus(pResponse) != XHTTP_STATUS_SWITCHING_PROTOCOLS)
	) {
		goto cleanup;
	}

	sStage = "messages";
	xbenchTimerStart(&RunTimer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		uint64 iStarted = xbenchNowNs();

		pFuture = xrtWsConnBinaryAsync(
			pClientConnection,
			(xbytesview) { pPayload, iPayloadSize }
		);
		if (
			!benchWsFutureResolved(pFuture) ||
			!benchWsWaitCount(&Context.Client.Messages, i + 1u)
		) {
			goto cleanup;
		}
		xrtFutureDestroy(pFuture);
		pFuture = NULL;
		pLatencies[i] = xbenchNowNs() - iStarted;
	}
	xbenchTimerStop(&RunTimer);
	iRunElapsed = xbenchTimerElapsedNs(&RunTimer);
	qsort(pLatencies, iIterations, sizeof(uint64), xbenchCompareU64);
	if (
		(xrtAtomic32Load(&Context.Errors, XMEMORY_ACQUIRE) != 0) ||
		(xrtAtomic32Load(&Context.Backpressure, XMEMORY_ACQUIRE) != 0) ||
		(xrtAtomic32Load(
			&Context.Server.Messages,
			XMEMORY_ACQUIRE
		 ) != iIterations) ||
		(xrtWsConnPending(pClientConnection) != 0) ||
		(xrtWsConnPending(pServerConnection) != 0) ||
		(xrtWsConnAsyncBytes(pClientConnection) != 0) ||
		(xrtWsConnAsyncCount(pClientConnection) != 0)
	) {
		goto cleanup;
	}
	xbenchPrintMetricDouble(
		"websocket_upgrade_latency_us",
		((double)iSetupElapsed) / 1000.0
	);
	xbenchPrintMetricDouble(
		"websocket_messages_per_sec",
		xbenchSafeRate(iIterations, iRunElapsed)
	);
	xbenchPrintMetricDouble(
		"websocket_latency_p50_us",
		xbenchPercentileUs(pLatencies, iIterations, 0.50)
	);
	xbenchPrintMetricDouble(
		"websocket_latency_p99_us",
		xbenchPercentileUs(pLatencies, iIterations, 0.99)
	);

	sStage = "close";
	pFuture = xrtWsConnCloseAsync(
		pClientConnection,
		XWS_CLOSE_NORMAL,
		XRT_STR_LITERAL("bench")
	);
	if (
		!benchWsFutureResolved(pFuture) ||
		!benchWsWaitCount(&Context.Client.Closed, 1) ||
		!benchWsWaitCount(&Context.Server.Closed, 1)
	) {
		goto cleanup;
	}
	xrtFutureDestroy(pFuture);
	pFuture = NULL;
	if (
		((Context.Client.Close.Flags & XWS_CONN_CLOSE_CLEAN) == 0) ||
		((Context.Server.Close.Flags & XWS_CONN_CLOSE_CLEAN) == 0) ||
		(Context.Client.Close.LocalCode != XWS_CLOSE_NORMAL) ||
		(Context.Server.Close.RemoteCode != XWS_CLOSE_NORMAL)
	) {
		goto cleanup;
	}

	sStage = "stats";
	if (
		!xrtHttpServerStats(pServer, &ServerStats) ||
		(ServerStats.Accepted != 1) ||
		(ServerStats.Requests != 1) ||
		(ServerStats.Responses != 1) ||
		(ServerStats.Upgraded != 1) ||
		(ServerStats.ProtocolErrors != 0) ||
		(ServerStats.Timeouts != 0) ||
		(ServerStats.Connections != 0)
	) {
		goto cleanup;
	}
	sStage = "done";
	bResult = true;

cleanup:
	if ( !bResult ) {
		fprintf(stderr, "WebSocket benchmark failed at %s", sStage);
		xbenchPrintCurrentError();
		fprintf(stderr, "\n");
	}
	if ( pFuture != NULL ) {
		(void)xrtFutureCancel(pFuture);
		xrtFutureDestroy(pFuture);
	}
	if (
		(pClientConnection != NULL) &&
		(xrtWsConnState(pClientConnection) != XWS_CONN_CLOSED)
	) {
		bClean = xrtWsConnAbort(pClientConnection) && bClean;
	}
	if (
		(pServerConnection != NULL) &&
		(xrtWsConnState(pServerConnection) != XWS_CONN_CLOSED)
	) {
		bClean = xrtWsConnAbort(pServerConnection) && bClean;
	}
	xrtWsConnDestroy(pClientConnection);
	xrtWsConnDestroy(pServerConnection);
	xrtHttpResponseDestroy(pResponse);
	xrtWsOpenResultDestroy(pOpen);
	xrtHttpClientDestroy(pClient);
	if ( pServer != NULL ) {
		if ( xrtHttpServerState(pServer) != XHTTP_SERVER_CLOSED ) {
			if ( bResult ) {
				bClean = xrtHttpServerDrain(pServer) && bClean;
			} else {
				bClean = xrtHttpServerAbort(pServer) && bClean;
			}
			bClean = benchWsWaitServerClosed(pServer) && bClean;
		}
		xrtHttpServerDestroy(pServer);
	}
	if ( pEngine != NULL ) {
		if (
			!xrtNetEngineStats(pEngine, &EngineStats) ||
			(EngineStats.LiveObjects != 0) ||
			(EngineStats.WaitErrors != 0)
		) {
			bClean = false;
		}
		bClean = xrtNetEngineDestroy(pEngine) && bClean;
	}
	free(pPayload);
	free(pLatencies);
	return bResult && bClean;
}



/* 运行 WebSocket 当前 API 的 Upgrade、消息回环和关闭基准。 */
int main(int argc, char** argv)
{
	uint32 iIterations = xbenchArgU32(argc, argv, 1, 5000u);
	uint32 iPayloadSize = xbenchArgU32(argc, argv, 2, 64u);

	if (
		(iIterations == 0) ||
		(iPayloadSize == 0) ||
		(iPayloadSize > 65536u)
	) {
		fprintf(stderr, "invalid WebSocket benchmark arguments.\n");
		return 1;
	}
	xbenchApplyCpuPinFromEnv();
	printf("xrt WebSocket loopback benchmark\n");
	return benchWsLoopback(iIterations, iPayloadSize) ? 0 : 2;
}
