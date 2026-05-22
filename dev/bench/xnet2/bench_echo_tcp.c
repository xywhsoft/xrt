#include "bench_stream_server.h"

typedef struct {
	volatile long iOpenCount;
	volatile long iCloseCount;
	volatile long iErrorCount;
	volatile long iEchoBytes;
} __bench_echo_tcp_server_ctx;

typedef struct {
	volatile long iOpenCount;
	volatile long iCloseCount;
	volatile long iErrorCount;
	volatile long iDone;
	uint32_t iIterations;
	uint32_t iMessageSize;
	uint32_t iSent;
	uint32_t iRecv;
	uint32_t iPendingBytes;
	char* pPayload;
	uint64_t* arrLatencyNs;
	uint64_t iConnectStartNs;
	uint64_t iOpenNs;
	uint64_t iRunStartNs;
	uint64_t iCurrentSendNs;
	uint64_t iFirstSendNs;
	uint64_t iLastRecvNs;
} __bench_echo_tcp_client_ctx;

static int __benchEchoLatencyCmp(const void* pA, const void* pB)
{
	uint64_t a = *(const uint64_t*)pA;
	uint64_t b = *(const uint64_t*)pB;
	return (a > b) - (a < b);
}

static bool __benchEchoSendChain(xnetstream* pStream, xnetchain* pChain)
{
	xnetspan arrInline[64];
	xnetspan* pSpans = arrInline;
	uint32 iSpanCount = 0u;
	uint32 iWriteCount = 0u;
	bool bOk = false;

	if ( !pStream || !pChain ) return false;

	iSpanCount = xrtNetChainSpanCount(pChain);
	if ( iSpanCount == 0u ) return true;

	if ( iSpanCount > (uint32)(sizeof(arrInline) / sizeof(arrInline[0])) ) {
		pSpans = (xnetspan*)calloc(iSpanCount, sizeof(xnetspan));
		if ( !pSpans ) return false;
	}

	iWriteCount = xrtNetChainGetSpans(pChain, pSpans, iSpanCount);
	if ( iWriteCount > 0u ) {
		bOk = (xrtNetStreamSendVec(pStream, pSpans, iWriteCount) == XRT_NET_OK);
	}

	if ( pSpans != arrInline ) free(pSpans);
	return bOk;
}

static bool __benchEchoSendNext(xnetstream* pStream, __bench_echo_tcp_client_ctx* pCtx)
{
	uint64_t iNowNs;
	if ( !pStream || !pCtx || !pCtx->pPayload || pCtx->iSent >= pCtx->iIterations ) return false;
	iNowNs = xbenchNowNs();
	pCtx->iCurrentSendNs = iNowNs;
	if ( pCtx->iSent == 0u ) pCtx->iFirstSendNs = iNowNs;
	if ( xrtNetStreamSend(pStream, pCtx->pPayload, pCtx->iMessageSize) != XRT_NET_OK ) return false;
	pCtx->iSent++;
	return true;
}

static void __benchEchoServerOnOpen(ptr pOwner, xbenchstreamserver* pServer, xbenchstreamconn* pConn)
{
	__bench_echo_tcp_server_ctx* pCtx = (__bench_echo_tcp_server_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	if ( pCtx ) xbenchAtomicInc(&pCtx->iOpenCount);
}

static void __benchEchoServerOnRecv(ptr pOwner, xbenchstreamserver* pServer, xbenchstreamconn* pConn, xnetchain* pChain)
{
	__bench_echo_tcp_server_ctx* pCtx = (__bench_echo_tcp_server_ctx*)pOwner;
	size_t iBytes;
	(void)pServer;
	if ( !pConn || !pConn->pStream || !pChain ) return;
	iBytes = xrtNetChainBytes(pChain);
	if ( iBytes == 0u ) return;
	if ( __benchEchoSendChain(pConn->pStream, pChain) && pCtx ) {
		xbenchAtomicAdd(&pCtx->iEchoBytes, (long)iBytes);
	}
	xrtNetChainConsume(pChain, iBytes);
}

static void __benchEchoServerOnClose(ptr pOwner, xbenchstreamserver* pServer, xbenchstreamconn* pConn, xnet_result iReason)
{
	__bench_echo_tcp_server_ctx* pCtx = (__bench_echo_tcp_server_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	(void)iReason;
	if ( pCtx ) xbenchAtomicInc(&pCtx->iCloseCount);
}

static void __benchEchoServerOnError(ptr pOwner, xbenchstreamserver* pServer, xbenchstreamconn* pConn, int iSysErr)
{
	__bench_echo_tcp_server_ctx* pCtx = (__bench_echo_tcp_server_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	(void)iSysErr;
	if ( pCtx ) xbenchAtomicInc(&pCtx->iErrorCount);
}

static void __benchEchoClientOnOpen(ptr pOwner, xnetstream* pStream)
{
	__bench_echo_tcp_client_ctx* pCtx = (__bench_echo_tcp_client_ctx*)pOwner;
	if ( !pCtx || !pStream ) return;
	pCtx->iOpenNs = xbenchNowNs();
	if ( pCtx->iRunStartNs == 0u ) pCtx->iRunStartNs = pCtx->iOpenNs;
	xbenchAtomicInc(&pCtx->iOpenCount);
	(void)__benchEchoSendNext(pStream, pCtx);
}

static void __benchEchoClientOnRecv(ptr pOwner, xnetstream* pStream, xnetchain* pChain)
{
	__bench_echo_tcp_client_ctx* pCtx = (__bench_echo_tcp_client_ctx*)pOwner;
	size_t iBytes = xrtNetChainBytes(pChain);
	if ( !pCtx || !pStream || !pChain || iBytes == 0u ) return;
	xrtNetChainConsume(pChain, iBytes);
	pCtx->iPendingBytes += (uint32_t)iBytes;
	while ( pCtx->iPendingBytes >= pCtx->iMessageSize && pCtx->iRecv < pCtx->iIterations ) {
		uint64_t iNow = xbenchNowNs();
		pCtx->iPendingBytes -= pCtx->iMessageSize;
		pCtx->iLastRecvNs = iNow;
		pCtx->arrLatencyNs[pCtx->iRecv++] = iNow - pCtx->iCurrentSendNs;
		if ( pCtx->iRecv >= pCtx->iIterations ) {
			xbenchAtomicInc(&pCtx->iDone);
			xrtNetStreamClose(pStream, XNET_CLOSE_F_GRACEFUL);
			return;
		}
		if ( !__benchEchoSendNext(pStream, pCtx) ) {
			xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
			return;
		}
	}
}

static void __benchEchoClientOnClose(ptr pOwner, xnetstream* pStream, xnet_result iReason)
{
	__bench_echo_tcp_client_ctx* pCtx = (__bench_echo_tcp_client_ctx*)pOwner;
	(void)pStream;
	(void)iReason;
	if ( pCtx ) xbenchAtomicInc(&pCtx->iCloseCount);
}

static void __benchEchoClientOnError(ptr pOwner, xnetstream* pStream, int iSysErr)
{
	__bench_echo_tcp_client_ctx* pCtx = (__bench_echo_tcp_client_ctx*)pOwner;
	(void)pStream;
	(void)iSysErr;
	if ( pCtx && xbenchAtomicLoad(&pCtx->iDone) == 0 ) xbenchAtomicInc(&pCtx->iErrorCount);
}

static const xnetstreamevents* __benchEchoClientEvents(void)
{
	static const xnetstreamevents tEvents = {
		__benchEchoClientOnOpen,
		__benchEchoClientOnRecv,
		NULL,
		__benchEchoClientOnClose,
		__benchEchoClientOnError,
		NULL,
		NULL
	};
	return &tEvents;
}

int main(int argc, char** argv)
{
	uint32_t iIterations = xbenchArgU32(argc, argv, 1, 5000u);
	uint32_t iMessageSize = xbenchArgU32(argc, argv, 2, 64u);
	xnetengineconfig tEngineCfg;
	xbenchstreamserverconfig tSrvCfg;
	xbenchstreamserverevents tSrvEvents;
	__bench_echo_tcp_server_ctx tSrvCtx;
	__bench_echo_tcp_client_ctx tCliCtx;
	xbenchstreamserver* pServer = NULL;
	xnetengine* pServerEngine = NULL;
	xnetengine* pClientEngine = NULL;
	xnetstream* pClient = NULL;
	xnetconnectconfig tConnCfg;
	uint64_t iSetupElapsedNs = 0u;
	uint64_t iElapsedNs = 0u;
	uint64_t iSteadyNs = 0u;
	double fP50 = 0.0;
	double fP95 = 0.0;
	double fP99 = 0.0;
	bool bCompleted = false;
	int iExitCode = 0;

	printf("xnet2 bench_echo_tcp\n");
	printf("iterations=%u message_size=%u\n", (unsigned)iIterations, (unsigned)iMessageSize);

	if ( !xbenchNetInit() ) return 1;
	memset(&tSrvCtx, 0, sizeof(tSrvCtx));
	memset(&tCliCtx, 0, sizeof(tCliCtx));
	memset(&tSrvEvents, 0, sizeof(tSrvEvents));
	tSrvEvents.OnOpen = __benchEchoServerOnOpen;
	tSrvEvents.OnRecv = __benchEchoServerOnRecv;
	tSrvEvents.OnClose = __benchEchoServerOnClose;
	tSrvEvents.OnError = __benchEchoServerOnError;

	tCliCtx.iIterations = iIterations;
	tCliCtx.iMessageSize = iMessageSize;
	tCliCtx.pPayload = (char*)malloc(iMessageSize);
	tCliCtx.arrLatencyNs = (uint64_t*)calloc(iIterations, sizeof(uint64_t));
	if ( !tCliCtx.pPayload || !tCliCtx.arrLatencyNs ) goto cleanup;
	memset(tCliCtx.pPayload, 'E', iMessageSize);

	xrtNetEngineConfigInit(&tEngineCfg);
	tEngineCfg.iWorkerCount = 1u;
	pServerEngine = xrtNetEngineCreate(&tEngineCfg);
	pClientEngine = xrtNetEngineCreate(&tEngineCfg);
	if ( !pServerEngine || !pClientEngine ) goto cleanup;
	if ( xrtNetEngineStart(pServerEngine) != XRT_NET_OK ) goto cleanup;
	if ( xrtNetEngineStart(pClientEngine) != XRT_NET_OK ) goto cleanup;

	xbenchStreamServerConfigInit(&tSrvCfg);
	(void)xrtNetAddrParse(&tSrvCfg.tListenCfg.tBindAddr, "127.0.0.1", 0);
	tSrvCfg.tListenCfg.iFlags |= XNET_LISTEN_F_REUSE_ADDR | XNET_LISTEN_F_NO_DELAY;
	pServer = xbenchStreamServerCreate(pServerEngine, &tSrvCfg, &tSrvEvents, &tSrvCtx);
	if ( !pServer ) goto cleanup;
	if ( xbenchStreamServerStart(pServer) != XRT_NET_OK ) goto cleanup;

	xrtNetConnectConfigInit(&tConnCfg);
	tConnCfg.sHost = "127.0.0.1";
	tConnCfg.iPort = xbenchStreamServerBoundPort(pServer);
	tConnCfg.iFlags |= XNET_CONNECT_F_NO_DELAY;
	pClient = xrtNetStreamCreate(pClientEngine, __benchEchoClientEvents(), &tCliCtx);
	if ( !pClient ) goto cleanup;

	tCliCtx.iConnectStartNs = xbenchNowNs();
	if ( xrtNetStreamConnect(pClient, &tConnCfg) != XRT_NET_OK ) goto cleanup;
	if ( !xbenchWaitMin(&tCliCtx.iOpenCount, 1, 10000u) ) goto cleanup;
	if ( tCliCtx.iOpenNs > tCliCtx.iConnectStartNs ) {
		iSetupElapsedNs = tCliCtx.iOpenNs - tCliCtx.iConnectStartNs;
	}
	bCompleted = xbenchWaitMin(&tCliCtx.iDone, 1, 10000u);
	if ( tCliCtx.iLastRecvNs > tCliCtx.iRunStartNs && tCliCtx.iRunStartNs > 0u ) {
		iElapsedNs = tCliCtx.iLastRecvNs - tCliCtx.iRunStartNs;
	}

	if ( !bCompleted || tCliCtx.iRecv != iIterations ) {
		printf("incomplete_run: recv=%u expected=%u client_errors=%ld server_errors=%ld\n",
			(unsigned)tCliCtx.iRecv,
			(unsigned)iIterations,
			xbenchAtomicLoad(&tCliCtx.iErrorCount),
			xbenchAtomicLoad(&tSrvCtx.iErrorCount));
		iExitCode = 2;
		goto cleanup;
	}

	if ( tCliCtx.iFirstSendNs > 0u && tCliCtx.iLastRecvNs > tCliCtx.iFirstSendNs ) {
		iSteadyNs = tCliCtx.iLastRecvNs - tCliCtx.iFirstSendNs;
	}

	qsort(tCliCtx.arrLatencyNs, tCliCtx.iRecv, sizeof(uint64_t), __benchEchoLatencyCmp);
	if ( tCliCtx.iRecv > 0u ) {
		fP50 = (double)tCliCtx.arrLatencyNs[(tCliCtx.iRecv - 1u) / 2u] / 1000.0;
		fP95 = (double)tCliCtx.arrLatencyNs[(uint32_t)((double)(tCliCtx.iRecv - 1u) * 0.95)] / 1000.0;
		fP99 = (double)tCliCtx.arrLatencyNs[(uint32_t)((double)(tCliCtx.iRecv - 1u) * 0.99)] / 1000.0;
	}

	xbenchPrintMetricU64("setup_elapsed_ns", iSetupElapsedNs);
	xbenchPrintMetricU64("total_elapsed_ns", iElapsedNs);
	xbenchPrintMetricDouble("messages_per_sec_total", xbenchSafeRate(tCliCtx.iRecv, iElapsedNs));
	xbenchPrintMetricDouble("bytes_per_sec_total", xbenchSafeRate((uint64_t)tCliCtx.iRecv * iMessageSize, iElapsedNs));
	xbenchPrintMetricU64("echo_steady_elapsed_ns", iSteadyNs);
	xbenchPrintMetricDouble("messages_per_sec_steady", xbenchSafeRate(tCliCtx.iRecv, iSteadyNs));
	xbenchPrintMetricDouble("bytes_per_sec_steady", xbenchSafeRate((uint64_t)tCliCtx.iRecv * iMessageSize, iSteadyNs));
	xbenchPrintMetricDouble("latency_p50_us", fP50);
	xbenchPrintMetricDouble("latency_p95_us", fP95);
	xbenchPrintMetricDouble("latency_p99_us", fP99);
	printf("client_open=%ld server_open=%ld recv=%u server_echo_bytes=%ld\n",
		xbenchAtomicLoad(&tCliCtx.iOpenCount),
		xbenchAtomicLoad(&tSrvCtx.iOpenCount),
		(unsigned)tCliCtx.iRecv,
		xbenchAtomicLoad(&tSrvCtx.iEchoBytes));
	printf("client_errors=%ld server_errors=%ld\n", xbenchAtomicLoad(&tCliCtx.iErrorCount), xbenchAtomicLoad(&tSrvCtx.iErrorCount));

cleanup:
	if ( pClient ) {
		xrtNetStreamClose(pClient, XNET_CLOSE_F_ABORT);
		xrtNetStreamDestroy(pClient);
	}
	if ( pServer ) xbenchStreamServerDestroy(pServer);
	if ( pClientEngine ) {
		xrtNetEngineStop(pClientEngine);
		xrtNetEngineDestroy(pClientEngine);
	}
	if ( pServerEngine ) {
		xrtNetEngineStop(pServerEngine);
		xrtNetEngineDestroy(pServerEngine);
	}
	free(tCliCtx.pPayload);
	free(tCliCtx.arrLatencyNs);
	xbenchNetUnit();
	return iExitCode;
}
