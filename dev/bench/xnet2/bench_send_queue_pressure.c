#include "bench_stream_server.h"

typedef struct {
	volatile long iOpenCount;
	volatile long iCloseCount;
	volatile long iErrorCount;
	volatile long iRecvBytes;
	xbenchstreamconn* pConn;
} __bench_queue_server_ctx;

typedef struct {
	volatile long iOpenCount;
	volatile long iCloseCount;
	volatile long iErrorCount;
	volatile long iHighWaterCount;
	volatile long iLowWaterCount;
	volatile long iDrainCount;
	volatile long iPeakQueuedBytes;
} __bench_queue_client_ctx;

static void __benchQueueServerOnOpen(ptr pOwner, xbenchstreamserver* pServer, xbenchstreamconn* pConn)
{
	__bench_queue_server_ctx* pCtx = (__bench_queue_server_ctx*)pOwner;
	(void)pServer;
	if ( !pCtx || !pConn || !pConn->pStream ) return;
	pCtx->pConn = pConn;
	xbenchAtomicInc(&pCtx->iOpenCount);
	xrtNetStreamPauseRead(pConn->pStream);
}

static void __benchQueueServerOnRecv(ptr pOwner, xbenchstreamserver* pServer, xbenchstreamconn* pConn, xnetchain* pChain)
{
	__bench_queue_server_ctx* pCtx = (__bench_queue_server_ctx*)pOwner;
	size_t iBytes = xrtNetChainBytes(pChain);
	(void)pServer;
	(void)pConn;
	if ( !pCtx || !pChain ) return;
	xbenchAtomicAdd(&pCtx->iRecvBytes, (long)iBytes);
	xrtNetChainConsume(pChain, iBytes);
}

static void __benchQueueServerOnClose(ptr pOwner, xbenchstreamserver* pServer, xbenchstreamconn* pConn, xnet_result iReason)
{
	__bench_queue_server_ctx* pCtx = (__bench_queue_server_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	(void)iReason;
	if ( pCtx ) xbenchAtomicInc(&pCtx->iCloseCount);
}

static void __benchQueueServerOnError(ptr pOwner, xbenchstreamserver* pServer, xbenchstreamconn* pConn, int iSysErr)
{
	__bench_queue_server_ctx* pCtx = (__bench_queue_server_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	(void)iSysErr;
	if ( pCtx ) xbenchAtomicInc(&pCtx->iErrorCount);
}

static void __benchQueueClientOnOpen(ptr pOwner, xnetstream* pStream)
{
	__bench_queue_client_ctx* pCtx = (__bench_queue_client_ctx*)pOwner;
	(void)pStream;
	if ( pCtx ) xbenchAtomicInc(&pCtx->iOpenCount);
}

static void __benchQueueClientOnDrain(ptr pOwner, xnetstream* pStream)
{
	__bench_queue_client_ctx* pCtx = (__bench_queue_client_ctx*)pOwner;
	(void)pStream;
	if ( pCtx ) xbenchAtomicInc(&pCtx->iDrainCount);
}

static void __benchQueueClientOnClose(ptr pOwner, xnetstream* pStream, xnet_result iReason)
{
	__bench_queue_client_ctx* pCtx = (__bench_queue_client_ctx*)pOwner;
	(void)pStream;
	(void)iReason;
	if ( pCtx ) xbenchAtomicInc(&pCtx->iCloseCount);
}

static void __benchQueueClientOnError(ptr pOwner, xnetstream* pStream, int iSysErr)
{
	__bench_queue_client_ctx* pCtx = (__bench_queue_client_ctx*)pOwner;
	(void)pStream;
	(void)iSysErr;
	if ( pCtx ) xbenchAtomicInc(&pCtx->iErrorCount);
}

static void __benchQueueClientOnHighWater(ptr pOwner, xnetstream* pStream, uint32 iQueuedBytes)
{
	__bench_queue_client_ctx* pCtx = (__bench_queue_client_ctx*)pOwner;
	(void)pStream;
	if ( !pCtx ) return;
	xbenchAtomicInc(&pCtx->iHighWaterCount);
	(void)xbenchAtomicMax(&pCtx->iPeakQueuedBytes, (long)iQueuedBytes);
}

static void __benchQueueClientOnLowWater(ptr pOwner, xnetstream* pStream, uint32 iQueuedBytes)
{
	__bench_queue_client_ctx* pCtx = (__bench_queue_client_ctx*)pOwner;
	(void)pStream;
	(void)iQueuedBytes;
	if ( pCtx ) xbenchAtomicInc(&pCtx->iLowWaterCount);
}

static const xnetstreamevents* __benchQueueClientEvents(void)
{
	static const xnetstreamevents tEvents = {
		__benchQueueClientOnOpen,
		NULL,
		__benchQueueClientOnDrain,
		__benchQueueClientOnClose,
		__benchQueueClientOnError,
		__benchQueueClientOnHighWater,
		__benchQueueClientOnLowWater
	};
	return &tEvents;
}

int main(int argc, char** argv)
{
	uint32_t iMessages = xbenchArgU32(argc, argv, 1, 10000u);
	uint32_t iMessageSize = xbenchArgU32(argc, argv, 2, 4096u);
	uint32_t iPauseMs = xbenchArgU32(argc, argv, 3, 500u);
	uint64_t iTargetBytes = (uint64_t)iMessages * (uint64_t)iMessageSize;
	xnetengineconfig tEngineCfg;
	xbenchstreamserverconfig tSrvCfg;
	xbenchstreamserverevents tSrvEvents;
	__bench_queue_server_ctx tSrvCtx;
	__bench_queue_client_ctx tCliCtx;
	xbenchstreamserver* pServer = NULL;
	xnetengine* pServerEngine = NULL;
	xnetengine* pClientEngine = NULL;
	xnetstream* pClient = NULL;
	xnetconnectconfig tConnCfg;
	char* pPayload = NULL;
	xbenchtimer tSendTimer;
	xbenchtimer tDrainTimer;
	uint64_t iSendNs = 0u;
	uint64_t iDrainNs = 0u;
	uint32_t iSentMessages = 0u;
	int iExitCode = 0;

	printf("xnet2 bench_send_queue_pressure\n");
	printf("messages=%u message_size=%u pause_ms=%u\n", (unsigned)iMessages, (unsigned)iMessageSize, (unsigned)iPauseMs);

	if ( !xbenchNetInit() ) return 1;
	memset(&tSrvCtx, 0, sizeof(tSrvCtx));
	memset(&tCliCtx, 0, sizeof(tCliCtx));
	memset(&tSrvEvents, 0, sizeof(tSrvEvents));
	tSrvEvents.OnOpen = __benchQueueServerOnOpen;
	tSrvEvents.OnRecv = __benchQueueServerOnRecv;
	tSrvEvents.OnClose = __benchQueueServerOnClose;
	tSrvEvents.OnError = __benchQueueServerOnError;

	pPayload = (char*)malloc(iMessageSize);
	if ( !pPayload ) goto cleanup;
	memset(pPayload, 'Q', iMessageSize);

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
	tSrvCfg.tListenCfg.iRecvLimit = (iTargetBytes >= (uint64_t)UINT32_MAX - 4096u)
		? UINT32_MAX
		: (uint32_t)(iTargetBytes + 4096u);
	pServer = xbenchStreamServerCreate(pServerEngine, &tSrvCfg, &tSrvEvents, &tSrvCtx);
	if ( !pServer ) goto cleanup;
	if ( xbenchStreamServerStart(pServer) != XRT_NET_OK ) goto cleanup;

	xrtNetConnectConfigInit(&tConnCfg);
	tConnCfg.sHost = "127.0.0.1";
	tConnCfg.iPort = xbenchStreamServerBoundPort(pServer);
	tConnCfg.iFlags |= XNET_CONNECT_F_NO_DELAY;
	tConnCfg.iHighWater = 32768u;
	tConnCfg.iLowWater = 8192u;
	pClient = xrtNetStreamCreate(pClientEngine, __benchQueueClientEvents(), &tCliCtx);
	if ( !pClient ) goto cleanup;
	if ( xrtNetStreamConnect(pClient, &tConnCfg) != XRT_NET_OK ) goto cleanup;
	(void)xbenchWaitMin(&tCliCtx.iOpenCount, 1, 3000u);
	(void)xbenchWaitMin(&tSrvCtx.iOpenCount, 1, 3000u);

	xbenchTimerStart(&tSendTimer);
	for ( uint32_t i = 0; i < iMessages; ++i ) {
		if ( xrtNetStreamSend(pClient, pPayload, iMessageSize) != XRT_NET_OK ) break;
		iSentMessages++;
		(void)xbenchAtomicMax(&tCliCtx.iPeakQueuedBytes, (long)xrtNetStreamPendingSend(pClient));
	}
	xbenchTimerStop(&tSendTimer);
	iSendNs = xbenchTimerElapsedNs(&tSendTimer);

	for ( uint32_t i = 0; i < 200u; ++i ) {
		(void)xbenchAtomicMax(&tCliCtx.iPeakQueuedBytes, (long)xrtNetStreamPendingSend(pClient));
		if ( xbenchAtomicLoad(&tCliCtx.iHighWaterCount) > 0 || xrtNetStreamPendingSend(pClient) > 0u ) break;
		if ( xbenchAtomicLoad(&tCliCtx.iErrorCount) > 0 || xbenchAtomicLoad(&tSrvCtx.iErrorCount) > 0 ) break;
		xbenchSleepMs(10u);
	}

	xbenchSleepMs(iPauseMs);
	if ( tSrvCtx.pConn && tSrvCtx.pConn->pStream ) {
		xrtNetStreamResumeRead(tSrvCtx.pConn->pStream);
	}

	xbenchTimerStart(&tDrainTimer);
	for ( uint32_t i = 0; i < 2000u; ++i ) {
		if ( xrtNetStreamPendingSend(pClient) == 0u ) break;
		xbenchSleepMs(10u);
	}
	xbenchTimerStop(&tDrainTimer);
	iDrainNs = xbenchTimerElapsedNs(&tDrainTimer);

	if ( iSentMessages != iMessages ) {
		printf("incomplete_enqueue: sent=%u expected=%u client_errors=%ld server_errors=%ld\n",
			(unsigned)iSentMessages,
			(unsigned)iMessages,
			xbenchAtomicLoad(&tCliCtx.iErrorCount),
			xbenchAtomicLoad(&tSrvCtx.iErrorCount));
		iExitCode = 2;
		goto cleanup;
	}

	xbenchPrintMetricU64("enqueue_elapsed_ns", iSendNs);
	xbenchPrintMetricDouble("enqueue_messages_per_sec", xbenchSafeRate(iSentMessages, iSendNs));
	xbenchPrintMetricU64("drain_elapsed_ns", iDrainNs);
	xbenchPrintMetricU64("final_pending_send_bytes", (uint64_t)xrtNetStreamPendingSend(pClient));
	xbenchPrintMetricU64("server_recv_bytes", (uint64_t)xbenchAtomicLoad(&tSrvCtx.iRecvBytes));
	printf("high_water=%ld low_water=%ld drain=%ld peak_queue=%ld\n",
		xbenchAtomicLoad(&tCliCtx.iHighWaterCount),
		xbenchAtomicLoad(&tCliCtx.iLowWaterCount),
		xbenchAtomicLoad(&tCliCtx.iDrainCount),
		xbenchAtomicLoad(&tCliCtx.iPeakQueuedBytes));
	printf("client_errors=%ld server_errors=%ld\n", xbenchAtomicLoad(&tCliCtx.iErrorCount), xbenchAtomicLoad(&tSrvCtx.iErrorCount));
	if ( xbenchAtomicLoad(&tCliCtx.iErrorCount) != 0 || xbenchAtomicLoad(&tSrvCtx.iErrorCount) != 0 ||
		xrtNetStreamPendingSend(pClient) != 0u || xbenchAtomicLoad(&tCliCtx.iHighWaterCount) == 0 ) {
		printf("invalid_pressure_run: pending=%u high_water=%ld client_errors=%ld server_errors=%ld\n",
			(unsigned)xrtNetStreamPendingSend(pClient),
			xbenchAtomicLoad(&tCliCtx.iHighWaterCount),
			xbenchAtomicLoad(&tCliCtx.iErrorCount),
			xbenchAtomicLoad(&tSrvCtx.iErrorCount));
		iExitCode = 2;
	}

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
	free(pPayload);
	xbenchNetUnit();
	return iExitCode;
}
