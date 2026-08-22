#include "../../../dev/bench/bench_common.h"

#include <xhttp.h>

#include "../../../dev/bench/network/bench_network_common.h"



#define BENCH_HTTP_BODY \
	"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz+-"



static const uint8 g_BenchHttpBody[] = BENCH_HTTP_BODY;
static const uint8 g_BenchHttpRawResponse[] =
	"HTTP/1.1 200 OK\r\n"
	"Content-Type: application/octet-stream\r\n"
	"Content-Length: 64\r\n"
	"\r\n"
	BENCH_HTTP_BODY;



/* HTTP 回环基准只在回调间共享原子失败计数。 */
typedef struct benchhttpcontext {
	xatomic32 Failures;
} benchhttpcontext;



/* 比较请求目标与静态零结尾路径。 */
static bool benchHttpTargetEqual(xstrview Target, cstr sPath)
{
	size_t iSize = strlen(sPath);

	return
		(Target.Size == iSize) &&
		((iSize == 0) || (memcmp(Target.Data, sPath, iSize) == 0));
}



/* 分别走结构化便利响应和预构建原始报文路径。 */
static void benchHttpRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	benchhttpcontext* pContext = (benchhttpcontext*)pData;
	xstrview Target = xrtHttpServerRequestTarget(pRequest);
	xnetresult Result;

	(void)pServer;
	if ( benchHttpTargetEqual(Target, "/reply") ) {
		Result = xrtHttpConnReply(
			pConnection,
			XHTTP_STATUS_OK,
			XRT_STR_LITERAL("application/octet-stream"),
			(xbytesview) {
				g_BenchHttpBody,
				sizeof(g_BenchHttpBody) - 1u
			}
		);
	} else if ( benchHttpTargetEqual(Target, "/raw") ) {
		Result = xrtHttpConnRespondRaw(
			pConnection,
			(xbytesview) {
				g_BenchHttpRawResponse,
				sizeof(g_BenchHttpRawResponse) - 1u
			},
			XHTTP_SERVER_RAW_KEEP_ALIVE
		);
	} else {
		Result = xrtHttpConnReply(
			pConnection,
			XHTTP_STATUS_NOT_FOUND,
			XRT_STR_LITERAL("text/plain"),
			XRT_BYTES_LITERAL("Not Found")
		);
	}
	if ( Result != XNET_RESULT_OK ) {
		(void)xrtAtomic32FetchAdd(
			&pContext->Failures,
			1,
			XMEMORY_RELEASE
		);
	}
}



/* 服务端运行时错误只计数，主线程在本轮结束时统一拒绝样本。 */
static void benchHttpError(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	benchhttpcontext* pContext = (benchhttpcontext*)pData;

	(void)pServer;
	(void)pConnection;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pContext->Failures,
		1,
		XMEMORY_RELEASE
	);
}



/* 阻塞执行一次 GET，并严格校验状态与 64 字节正文。 */
static bool benchHttpFetch(xhttpclient* pClient, xstrview Url)
{
	xhttpresult* pResult = xrtHttpClientGetSync(pClient, Url, NULL);
	const xhttpresponse* pResponse;
	xbytesview Body;
	bool bResult;

	if ( pResult == NULL ) {
		return false;
	}
	pResponse = xrtHttpResultResponse(pResult);
	Body = xrtHttpResponseBody(pResponse);
	bResult =
		(pResponse != NULL) &&
		(xrtHttpResponseStatus(pResponse) == XHTTP_STATUS_OK) &&
		(Body.Size == (sizeof(g_BenchHttpBody) - 1u)) &&
		(memcmp(Body.Data, g_BenchHttpBody, Body.Size) == 0);
	xrtHttpResultDestroy(pResult);
	return bResult;
}



/* 预热后测量一条持久连接上的顺序请求吞吐与尾延迟。 */
static bool benchHttpPath(
	xhttpclient* pClient,
	xstrview Url,
	uint32 iIterations,
	uint64* pLatencies,
	uint64* pElapsed
)
{
	xbenchtimer Timer;

	if (
		(pClient == NULL) ||
		(iIterations == 0) ||
		(pLatencies == NULL) ||
		(pElapsed == NULL) ||
		!benchHttpFetch(pClient, Url)
	) {
		return false;
	}
	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		uint64 iStarted = xbenchNowNs();

		if ( !benchHttpFetch(pClient, Url) ) {
			return false;
		}
		pLatencies[i] = xbenchNowNs() - iStarted;
	}
	xbenchTimerStop(&Timer);
	*pElapsed = xbenchTimerElapsedNs(&Timer);
	qsort(pLatencies, iIterations, sizeof(uint64), xbenchCompareU64);
	return *pElapsed != 0;
}



/* 在截止时间内等待 HTTP Server 进入关闭态。 */
static bool benchHttpWaitServerClosed(xhttpserver* pServer)
{
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(5000000));

	while ( xrtHttpServerState(pServer) != XHTTP_SERVER_CLOSED ) {
		if ( xrtDeadlineExpired(Deadline) ) {
			return false;
		}
		xrtThreadYield();
	}
	return true;
}



/* 测量 HTTP/1.1 keep-alive 下便利响应与 raw 响应的完整客户端/服务端路径。 */
static bool benchHttpLoopback(uint32 iIterations)
{
	benchhttpcontext Context;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpserverevents ServerEvents;
	xhttpclientconfig ClientConfig;
	xhttpclientstats ClientStats;
	xhttpserverstats ServerStats;
	xnetenginestats EngineStats;
	xnetengine* pEngine = NULL;
	xhttpserver* pServer = NULL;
	xhttpclient* pClient = NULL;
	xnetaddr Address;
	char sReplyUrl[128];
	char sRawUrl[128];
	uint64* pLatencies = NULL;
	uint64 iReplyElapsed = 0;
	uint64 iRawElapsed = 0;
	uint64 iExpectedRequests = ((uint64)iIterations + 1u) * 2u;
	int iReplyLength;
	int iRawLength;
	cstr sStage = "allocate";
	bool bResult = false;
	bool bClean = true;

	memset(&Context, 0, sizeof(Context));
	memset(&ClientStats, 0, sizeof(ClientStats));
	memset(&ServerStats, 0, sizeof(ServerStats));
	memset(&EngineStats, 0, sizeof(EngineStats));
	xrtAtomic32Init(&Context.Failures, 0);
	pLatencies = (uint64*)calloc(iIterations, sizeof(uint64));
	if ( pLatencies == NULL ) {
		goto cleanup;
	}

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
	ServerEvents.Request = benchHttpRequest;
	ServerEvents.Error = benchHttpError;
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
	iReplyLength = snprintf(
		sReplyUrl,
		sizeof(sReplyUrl),
		"http://127.0.0.1:%u/reply",
		(unsigned int)Address.Port
	);
	iRawLength = snprintf(
		sRawUrl,
		sizeof(sRawUrl),
		"http://127.0.0.1:%u/raw",
		(unsigned int)Address.Port
	);
	if (
		(iReplyLength <= 0) ||
		((size_t)iReplyLength >= sizeof(sReplyUrl)) ||
		(iRawLength <= 0) ||
		((size_t)iRawLength >= sizeof(sRawUrl))
	) {
		goto cleanup;
	}

	sStage = "reply";
	if ( !benchHttpPath(
		pClient,
		(xstrview) { sReplyUrl, (size_t)iReplyLength },
		iIterations,
		pLatencies,
		&iReplyElapsed
	) ) {
		goto cleanup;
	}
	xbenchPrintMetricDouble(
		"http_reply_requests_per_sec",
		xbenchSafeRate(iIterations, iReplyElapsed)
	);
	xbenchPrintMetricDouble(
		"http_reply_latency_p50_us",
		xbenchPercentileUs(pLatencies, iIterations, 0.50)
	);
	xbenchPrintMetricDouble(
		"http_reply_latency_p99_us",
		xbenchPercentileUs(pLatencies, iIterations, 0.99)
	);

	sStage = "raw";
	if ( !benchHttpPath(
		pClient,
		(xstrview) { sRawUrl, (size_t)iRawLength },
		iIterations,
		pLatencies,
		&iRawElapsed
	) ) {
		goto cleanup;
	}
	xbenchPrintMetricDouble(
		"http_raw_requests_per_sec",
		xbenchSafeRate(iIterations, iRawElapsed)
	);
	xbenchPrintMetricDouble(
		"http_raw_latency_p50_us",
		xbenchPercentileUs(pLatencies, iIterations, 0.50)
	);
	xbenchPrintMetricDouble(
		"http_raw_latency_p99_us",
		xbenchPercentileUs(pLatencies, iIterations, 0.99)
	);

	sStage = "stats";
	if (
		!xrtHttpClientStats(pClient, &ClientStats) ||
		!xrtHttpServerStats(pServer, &ServerStats) ||
		(xrtAtomic32Load(&Context.Failures, XMEMORY_ACQUIRE) != 0) ||
		(ClientStats.ActiveConnections != 0) ||
		(ClientStats.IdleConnections != 1) ||
		(ClientStats.ClosingConnections != 0) ||
		(ClientStats.WaitingCalls != 0) ||
		(ClientStats.RequestsStarted != iExpectedRequests) ||
		(ClientStats.RequestsCompleted != iExpectedRequests) ||
		(ClientStats.ConnectionsOpened != 1) ||
		(ClientStats.ConnectionsReused != (iExpectedRequests - 1u)) ||
		(ClientStats.PoolRejected != 0) ||
		(ServerStats.Accepted != 1) ||
		(ServerStats.Rejected != 0) ||
		(ServerStats.Requests != iExpectedRequests) ||
		(ServerStats.Responses != iExpectedRequests) ||
		(ServerStats.Upgraded != 0) ||
		(ServerStats.ProtocolErrors != 0) ||
		(ServerStats.Timeouts != 0) ||
		(ServerStats.Connections != 1)
	) {
		fprintf(
			stderr,
			"HTTP stats failures=%u client(active=%zu idle=%zu closing=%zu "
			"waiting=%zu started=%" PRIu64 " completed=%" PRIu64
			" opened=%" PRIu64 " reused=%" PRIu64 " closed=%" PRIu64
			" waits=%" PRIu64 " rejected=%" PRIu64 ") "
			"server(state=%d accepted=%" PRIu64 " rejected=%" PRIu64
			" requests=%" PRIu64 " responses=%" PRIu64
			" upgraded=%" PRIu64 " protocol=%" PRIu64
			" timeouts=%" PRIu64 " connections=%zu peak=%zu)\n",
			(unsigned int)xrtAtomic32Load(
				&Context.Failures,
				XMEMORY_ACQUIRE
			),
			ClientStats.ActiveConnections,
			ClientStats.IdleConnections,
			ClientStats.ClosingConnections,
			ClientStats.WaitingCalls,
			ClientStats.RequestsStarted,
			ClientStats.RequestsCompleted,
			ClientStats.ConnectionsOpened,
			ClientStats.ConnectionsReused,
			ClientStats.ConnectionsClosed,
			ClientStats.PoolWaits,
			ClientStats.PoolRejected,
			(int)ServerStats.State,
			ServerStats.Accepted,
			ServerStats.Rejected,
			ServerStats.Requests,
			ServerStats.Responses,
			ServerStats.Upgraded,
			ServerStats.ProtocolErrors,
			ServerStats.Timeouts,
			ServerStats.Connections,
			ServerStats.PeakConnections
		);
		goto cleanup;
	}
	xbenchPrintMetricU64("http_requests", iExpectedRequests);
	xbenchPrintMetricU64(
		"http_connections_opened",
		ClientStats.ConnectionsOpened
	);
	xbenchPrintMetricU64(
		"http_connections_reused",
		ClientStats.ConnectionsReused
	);
	sStage = "done";
	bResult = true;

cleanup:
	if ( !bResult ) {
		fprintf(stderr, "HTTP benchmark failed at %s", sStage);
		xbenchPrintCurrentError();
		fprintf(stderr, "\n");
	}
	xrtHttpClientDestroy(pClient);
	if ( pServer != NULL ) {
		if ( xrtHttpServerState(pServer) != XHTTP_SERVER_CLOSED ) {
			if ( bResult ) {
				bClean = xrtHttpServerDrain(pServer) && bClean;
			} else {
				bClean = xrtHttpServerAbort(pServer) && bClean;
			}
			bClean = benchHttpWaitServerClosed(pServer) && bClean;
		}
		if (
			!xrtHttpServerStats(pServer, &ServerStats) ||
			(ServerStats.Connections != 0)
		) {
			bClean = false;
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
	free(pLatencies);
	return bResult && bClean;
}



/* 运行 HTTP/1.1 当前 API 的持久连接回环基准。 */
int main(int argc, char** argv)
{
	uint32 iIterations = xbenchArgU32(argc, argv, 1, 5000u);

	if ( iIterations == 0 ) {
		fprintf(stderr, "invalid HTTP benchmark iterations.\n");
		return 1;
	}
	xbenchApplyCpuPinFromEnv();
	printf("xrt HTTP/1.1 loopback benchmark\n");
	return benchHttpLoopback(iIterations) ? 0 : 2;
}
