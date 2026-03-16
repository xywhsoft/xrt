#include "bench_stream_server.h"

typedef struct {
	volatile long iOpenCount;
	volatile long iCloseCount;
	volatile long iErrorCount;
	volatile long iRecvBytes;
	volatile long iRecvCalls;
	volatile uint64_t iResumeNs;
	volatile uint64_t iFirstRecvNs;
	int iRecvSockBuf;
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
	volatile uint64_t iFirstLowWaterNs;
	volatile uint64_t iFirstDrainNs;
	int iSendSockBuf;
} __bench_queue_client_ctx;

typedef struct {
	xnetstream* pStream;
	__bench_queue_client_ctx* pClientCtx;
	const void* pPayload;
	uint32_t iMessageSize;
	uint32_t iSendGoalMessages;
	uint32_t iHardSendLimit;
	uint32_t iHighWater;
	volatile long iDone;
	uint32_t iSentMessages;
	xnet_result iLastSendRes;
	bool bPressureObserved;
} __bench_queue_send_job;

static void __benchQueueSetSockBuf(xsocket hSocket, int iOptName, int iBytes)
{
	if ( !__xnetSocketIsValid(hSocket) || iBytes <= 0 ) return;
	#if defined(_WIN32) || defined(_WIN64)
		(void)setsockopt(hSocket, SOL_SOCKET, iOptName, (const char*)&iBytes, (int)sizeof(iBytes));
	#else
		(void)setsockopt(hSocket, SOL_SOCKET, iOptName, &iBytes, (socklen_t)sizeof(iBytes));
	#endif
}

static void __benchQueueServerOnOpen(ptr pOwner, xbenchstreamserver* pServer, xbenchstreamconn* pConn)
{
	__bench_queue_server_ctx* pCtx = (__bench_queue_server_ctx*)pOwner;
	(void)pServer;
	if ( !pCtx || !pConn || !pConn->pStream ) return;
	pCtx->pConn = pConn;
	__benchQueueSetSockBuf(pConn->pStream->hSocket, SO_RCVBUF, pCtx->iRecvSockBuf > 0 ? pCtx->iRecvSockBuf : 16384);
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
	xbenchAtomicInc(&pCtx->iRecvCalls);
	(void)xbenchAtomicSetOnceU64(&pCtx->iFirstRecvNs, xbenchNowNs());
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
	if ( pStream ) {
		__benchQueueSetSockBuf(pStream->hSocket, SO_SNDBUF, (pCtx && pCtx->iSendSockBuf > 0) ? pCtx->iSendSockBuf : 16384);
	}
	if ( pCtx ) xbenchAtomicInc(&pCtx->iOpenCount);
}

static void __benchQueueClientOnDrain(ptr pOwner, xnetstream* pStream)
{
	__bench_queue_client_ctx* pCtx = (__bench_queue_client_ctx*)pOwner;
	(void)pStream;
	if ( pCtx ) {
		(void)xbenchAtomicSetOnceU64(&pCtx->iFirstDrainNs, xbenchNowNs());
		xbenchAtomicInc(&pCtx->iDrainCount);
	}
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
	if ( pCtx ) {
		(void)xbenchAtomicSetOnceU64(&pCtx->iFirstLowWaterNs, xbenchNowNs());
		xbenchAtomicInc(&pCtx->iLowWaterCount);
	}
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

static void __benchQueueSendTask(xnetworker* pWorker, ptr pArg)
{
	__bench_queue_send_job* pJob = (__bench_queue_send_job*)pArg;
	xnetspan aVec[64];
	uint32_t iBatchCount = 0u;
	(void)pWorker;
	if ( !pJob || !pJob->pStream || !pJob->pPayload || !pJob->pClientCtx || pJob->iMessageSize == 0u ) {
		if ( pJob ) xbenchAtomicInc(&pJob->iDone);
		return;
	}

	iBatchCount = pJob->iHighWater / pJob->iMessageSize;
	if ( iBatchCount == 0u ) iBatchCount = 1u;
	if ( iBatchCount < 8u ) iBatchCount = 8u;
	if ( iBatchCount > (uint32_t)(sizeof(aVec) / sizeof(aVec[0]))) {
		iBatchCount = (uint32_t)(sizeof(aVec) / sizeof(aVec[0]));
	}
	for ( uint32_t i = 0; i < iBatchCount; ++i ) {
		aVec[i].pData = pJob->pPayload;
		aVec[i].iLen = pJob->iMessageSize;
	}

	pJob->iLastSendRes = XRT_NET_OK;
	for ( uint32_t i = 0; i < pJob->iHardSendLimit; ) {
		uint32_t iBatch = iBatchCount;
		uint32_t iPendingBytes;
		if ( iBatch > pJob->iHardSendLimit - i ) {
			iBatch = pJob->iHardSendLimit - i;
		}
		pJob->iLastSendRes = xrtNetStreamSendVec(pJob->pStream, aVec, iBatch);
		if ( pJob->iLastSendRes != XRT_NET_OK ) break;
		pJob->iSentMessages += iBatch;
		i += iBatch;
		iPendingBytes = (uint32_t)xrtNetStreamPendingSend(pJob->pStream);
		(void)xbenchAtomicMax(&pJob->pClientCtx->iPeakQueuedBytes, (long)iPendingBytes);
		if ( pJob->iSentMessages >= pJob->iSendGoalMessages &&
			(xbenchAtomicLoad(&pJob->pClientCtx->iHighWaterCount) > 0 ||
				xbenchAtomicLoad(&pJob->pClientCtx->iPeakQueuedBytes) >= (long)pJob->iHighWater ||
				iPendingBytes >= pJob->iHighWater) ) {
			pJob->bPressureObserved = true;
			break;
		}
	}

	xbenchAtomicInc(&pJob->iDone);
}

int main(int argc, char** argv)
{
	uint32_t iMessages = xbenchArgU32(argc, argv, 1, 10000u);
	uint32_t iMessageSize = xbenchArgU32(argc, argv, 2, 4096u);
	uint32_t iPauseMs = xbenchArgU32(argc, argv, 3, 500u);
	uint32_t iDrainTimeoutMs = xbenchArgU32(argc, argv, 4, 60000u);
	uint32_t iHighWater = xbenchEnvU32("XNET_BENCH_QUEUE_HIGH_WATER", 32768u);
	uint32_t iLowWater = xbenchEnvU32("XNET_BENCH_QUEUE_LOW_WATER", 8192u);
	uint32_t iSendSockBuf = xbenchEnvU32("XNET_BENCH_QUEUE_SNDBUF", 16384u);
	uint32_t iRecvSockBuf = xbenchEnvU32("XNET_BENCH_QUEUE_RCVBUF", 16384u);
	uint64_t iTargetBytes = (uint64_t)iMessages * (uint64_t)iMessageSize;
	uint64_t iRecvLimitBytes = 0u;
	uint64_t iExpectedRecvBytes = 0u;
	uint32_t iSendGoalMessages = iMessages;
	uint32_t iHardSendLimit = 0u;
	uint32_t iPressureGoalBytes = 0u;
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
	__bench_queue_send_job tSendJob;
	uint64_t iSendNs = 0u;
	uint64_t iDrainNs = 0u;
	uint64_t iPendingZeroNs = 0u;
	uint64_t iDrainDoneNs = 0u;
	uint64_t iResumeToFirstRecvNs = 0u;
	uint64_t iResumeToPendingZeroNs = 0u;
	uint64_t iPendingZeroToFullRecvNs = 0u;
	uint64_t iResumeToLowWaterNs = 0u;
	uint64_t iResumeToDrainNs = 0u;
	uint32_t iSentMessages = 0u;
	bool bClientOpened = false;
	bool bServerOpened = false;
	bool bPressureObserved = false;
	xnet_result iLastSendRes = XRT_NET_OK;
	int iExitCode = 0;
	uint64_t iObserveDeadlineNs = 0u;
	uint64_t iDrainDeadlineNs = 0u;

	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
	printf("xnet2 bench_send_queue_pressure\n");
	printf("messages=%u message_size=%u pause_ms=%u drain_timeout_ms=%u\n",
		(unsigned)iMessages,
		(unsigned)iMessageSize,
		(unsigned)iPauseMs,
		(unsigned)iDrainTimeoutMs);
	printf("high_water=%u low_water=%u sndbuf=%u rcvbuf=%u\n",
		(unsigned)iHighWater,
		(unsigned)iLowWater,
		(unsigned)iSendSockBuf,
		(unsigned)iRecvSockBuf);

	if ( iLowWater > iHighWater ) iLowWater = iHighWater;
	iPressureGoalBytes = iHighWater + iMessageSize;
	if ( iMessageSize > 0u ) {
		uint32_t iPressureMsgs = (iPressureGoalBytes + iMessageSize - 1u) / iMessageSize;
		if ( iSendGoalMessages < iPressureMsgs ) iSendGoalMessages = iPressureMsgs;
	}
	iHardSendLimit = iSendGoalMessages * 4u;
	if ( iHardSendLimit < iSendGoalMessages ) iHardSendLimit = iSendGoalMessages;
	iRecvLimitBytes = ((uint64_t)iHardSendLimit * (uint64_t)iMessageSize) + 4096u;

	if ( !xbenchNetInit() ) return 1;
	memset(&tSrvCtx, 0, sizeof(tSrvCtx));
	memset(&tCliCtx, 0, sizeof(tCliCtx));
	memset(&tSendJob, 0, sizeof(tSendJob));
	memset(&tSrvEvents, 0, sizeof(tSrvEvents));
	tSrvEvents.OnOpen = __benchQueueServerOnOpen;
	tSrvEvents.OnRecv = __benchQueueServerOnRecv;
	tSrvEvents.OnClose = __benchQueueServerOnClose;
	tSrvEvents.OnError = __benchQueueServerOnError;
	tSrvCtx.iRecvSockBuf = (int)iRecvSockBuf;
	tCliCtx.iSendSockBuf = (int)iSendSockBuf;

	pPayload = (char*)malloc(iMessageSize);
	if ( !pPayload ) goto cleanup;
	memset(pPayload, 'Q', iMessageSize);

	xrtNetEngineConfigInit(&tEngineCfg);
	tEngineCfg.iWorkerCount = 1u;
	tEngineCfg.iTimerTickMs = 1u;
	pServerEngine = xrtNetEngineCreate(&tEngineCfg);
	pClientEngine = xrtNetEngineCreate(&tEngineCfg);
	if ( !pServerEngine || !pClientEngine ) goto cleanup;
	if ( xrtNetEngineStart(pServerEngine) != XRT_NET_OK ) goto cleanup;
	if ( xrtNetEngineStart(pClientEngine) != XRT_NET_OK ) goto cleanup;

	xbenchStreamServerConfigInit(&tSrvCfg);
	(void)xrtNetAddrParse(&tSrvCfg.tListenCfg.tBindAddr, "127.0.0.1", 0);
	tSrvCfg.tListenCfg.iFlags |= XNET_LISTEN_F_REUSE_ADDR | XNET_LISTEN_F_NO_DELAY;
	tSrvCfg.tListenCfg.iRecvLimit = (iRecvLimitBytes >= (uint64_t)UINT32_MAX)
		? UINT32_MAX
		: (uint32_t)iRecvLimitBytes;
	pServer = xbenchStreamServerCreate(pServerEngine, &tSrvCfg, &tSrvEvents, &tSrvCtx);
	if ( !pServer ) goto cleanup;
	if ( xbenchStreamServerStart(pServer) != XRT_NET_OK ) goto cleanup;

	xrtNetConnectConfigInit(&tConnCfg);
	tConnCfg.sHost = "127.0.0.1";
	tConnCfg.iPort = xbenchStreamServerBoundPort(pServer);
	tConnCfg.iFlags |= XNET_CONNECT_F_NO_DELAY;
	tConnCfg.iHighWater = iHighWater;
	tConnCfg.iLowWater = iLowWater;
	iPressureGoalBytes = tConnCfg.iHighWater + iMessageSize;
	pClient = xrtNetStreamCreate(pClientEngine, __benchQueueClientEvents(), &tCliCtx);
	if ( !pClient ) goto cleanup;
	if ( xrtNetStreamConnect(pClient, &tConnCfg) != XRT_NET_OK ) goto cleanup;
	bClientOpened = xbenchWaitMin(&tCliCtx.iOpenCount, 1, 3000u);
	bServerOpened = xbenchWaitMin(&tSrvCtx.iOpenCount, 1, 3000u);
	if ( !bClientOpened || !bServerOpened ) {
		printf("open_wait_failed: client_open=%ld server_open=%ld client_errors=%ld server_errors=%ld\n",
			xbenchAtomicLoad(&tCliCtx.iOpenCount),
			xbenchAtomicLoad(&tSrvCtx.iOpenCount),
			xbenchAtomicLoad(&tCliCtx.iErrorCount),
			xbenchAtomicLoad(&tSrvCtx.iErrorCount));
		iExitCode = 2;
		goto cleanup;
	}

	tSendJob.pStream = pClient;
	tSendJob.pClientCtx = &tCliCtx;
	tSendJob.pPayload = pPayload;
	tSendJob.iMessageSize = iMessageSize;
	tSendJob.iSendGoalMessages = iSendGoalMessages;
	tSendJob.iHardSendLimit = iHardSendLimit;
	tSendJob.iHighWater = tConnCfg.iHighWater;
	xbenchTimerStart(&tSendTimer);
	if ( xrtNetEnginePost(pClientEngine, pClient->pWorker ? pClient->pWorker->iId : 0u, __benchQueueSendTask, &tSendJob) != XRT_NET_OK ) {
		printf("send_task_post_failed\n");
		iExitCode = 2;
		goto cleanup;
	}
	if ( !xbenchWaitMin(&tSendJob.iDone, 1, 10000u) ) {
		printf("send_task_timeout: sent=%u high_water=%ld peak_queue=%ld\n",
			(unsigned)tSendJob.iSentMessages,
			xbenchAtomicLoad(&tCliCtx.iHighWaterCount),
			xbenchAtomicLoad(&tCliCtx.iPeakQueuedBytes));
		iExitCode = 2;
		goto cleanup;
	}
	xbenchTimerStop(&tSendTimer);
	iSendNs = xbenchTimerElapsedNs(&tSendTimer);
	iSentMessages = tSendJob.iSentMessages;
	iLastSendRes = tSendJob.iLastSendRes;
	bPressureObserved = tSendJob.bPressureObserved;

	if ( iLastSendRes != XRT_NET_OK && !bPressureObserved ) {
		printf("send_failed_before_pressure: status=%d sent=%u hard_limit=%u pending=%u high_water=%ld peak_queue=%ld client_errors=%ld server_errors=%ld\n",
			(int)iLastSendRes,
			(unsigned)iSentMessages,
			(unsigned)iHardSendLimit,
			(unsigned)xrtNetStreamPendingSend(pClient),
			xbenchAtomicLoad(&tCliCtx.iHighWaterCount),
			xbenchAtomicLoad(&tCliCtx.iPeakQueuedBytes),
			xbenchAtomicLoad(&tCliCtx.iErrorCount),
			xbenchAtomicLoad(&tSrvCtx.iErrorCount));
		iExitCode = 2;
		goto cleanup;
	}

	iExpectedRecvBytes = (uint64_t)iSentMessages * (uint64_t)iMessageSize;

	iObserveDeadlineNs = xbenchDeadlineAfterMs(2000u);
	while ( !xbenchDeadlineReached(iObserveDeadlineNs) ) {
		(void)xbenchAtomicMax(&tCliCtx.iPeakQueuedBytes, (long)xrtNetStreamPendingSend(pClient));
		if ( xbenchAtomicLoad(&tCliCtx.iHighWaterCount) > 0 || xrtNetStreamPendingSend(pClient) > 0u ) break;
		if ( xbenchAtomicLoad(&tCliCtx.iErrorCount) > 0 || xbenchAtomicLoad(&tSrvCtx.iErrorCount) > 0 ) break;
		xbenchSleepMs(10u);
	}

	xbenchSleepMs(iPauseMs);
	if ( tSrvCtx.pConn && tSrvCtx.pConn->pStream ) {
		tCliCtx.iLowWaterCount = 0;
		tCliCtx.iDrainCount = 0;
		tCliCtx.iFirstLowWaterNs = 0u;
		tCliCtx.iFirstDrainNs = 0u;
		tSrvCtx.iResumeNs = xbenchNowNs();
		xrtNetStreamResumeRead(tSrvCtx.pConn->pStream);
	}

	xbenchTimerStart(&tDrainTimer);
	iDrainDeadlineNs = xbenchDeadlineAfterMs(iDrainTimeoutMs);
	while ( !xbenchDeadlineReached(iDrainDeadlineNs) ) {
		if ( xrtNetStreamPendingSend(pClient) == 0u && iPendingZeroNs == 0u ) {
			iPendingZeroNs = xbenchNowNs();
		}
		if ( xrtNetStreamPendingSend(pClient) == 0u &&
			(uint64_t)xbenchAtomicLoad(&tSrvCtx.iRecvBytes) >= iExpectedRecvBytes ) {
			iDrainDoneNs = xbenchNowNs();
			break;
		}
		xbenchSleepMs(1u);
	}
	xbenchTimerStop(&tDrainTimer);
	iDrainNs = xbenchTimerElapsedNs(&tDrainTimer);
	if ( xbenchAtomicLoadU64(&tSrvCtx.iResumeNs) > 0u && xbenchAtomicLoadU64(&tSrvCtx.iFirstRecvNs) >= xbenchAtomicLoadU64(&tSrvCtx.iResumeNs) ) {
		iResumeToFirstRecvNs = xbenchAtomicLoadU64(&tSrvCtx.iFirstRecvNs) - xbenchAtomicLoadU64(&tSrvCtx.iResumeNs);
	}
	if ( xbenchAtomicLoadU64(&tSrvCtx.iResumeNs) > 0u && iPendingZeroNs >= xbenchAtomicLoadU64(&tSrvCtx.iResumeNs) ) {
		iResumeToPendingZeroNs = iPendingZeroNs - xbenchAtomicLoadU64(&tSrvCtx.iResumeNs);
	}
	if ( iDrainDoneNs > 0u && iPendingZeroNs > 0u && iDrainDoneNs >= iPendingZeroNs ) {
		iPendingZeroToFullRecvNs = iDrainDoneNs - iPendingZeroNs;
	}
	if ( xbenchAtomicLoadU64(&tSrvCtx.iResumeNs) > 0u && xbenchAtomicLoadU64(&tCliCtx.iFirstLowWaterNs) >= xbenchAtomicLoadU64(&tSrvCtx.iResumeNs) ) {
		iResumeToLowWaterNs = xbenchAtomicLoadU64(&tCliCtx.iFirstLowWaterNs) - xbenchAtomicLoadU64(&tSrvCtx.iResumeNs);
	}
	if ( xbenchAtomicLoadU64(&tSrvCtx.iResumeNs) > 0u && xbenchAtomicLoadU64(&tCliCtx.iFirstDrainNs) >= xbenchAtomicLoadU64(&tSrvCtx.iResumeNs) ) {
		iResumeToDrainNs = xbenchAtomicLoadU64(&tCliCtx.iFirstDrainNs) - xbenchAtomicLoadU64(&tSrvCtx.iResumeNs);
	}

	if ( iSentMessages < iSendGoalMessages ) {
		printf("incomplete_enqueue: sent=%u expected_at_least=%u client_errors=%ld server_errors=%ld\n",
			(unsigned)iSentMessages,
			(unsigned)iSendGoalMessages,
			xbenchAtomicLoad(&tCliCtx.iErrorCount),
			xbenchAtomicLoad(&tSrvCtx.iErrorCount));
		iExitCode = 2;
		goto cleanup;
	}

	xbenchPrintMetricU64("enqueue_elapsed_ns", iSendNs);
	xbenchPrintMetricDouble("enqueue_messages_per_sec", xbenchSafeRate(iSentMessages, iSendNs));
	xbenchPrintMetricU64("drain_elapsed_ns", iDrainNs);
	xbenchPrintMetricU64("sent_messages", (uint64_t)iSentMessages);
	xbenchPrintMetricU64("expected_recv_bytes", iExpectedRecvBytes);
	xbenchPrintMetricU64("pressure_goal_bytes", (uint64_t)iPressureGoalBytes);
	xbenchPrintMetricU64("final_pending_send_bytes", (uint64_t)xrtNetStreamPendingSend(pClient));
	xbenchPrintMetricU64("server_recv_bytes", (uint64_t)xbenchAtomicLoad(&tSrvCtx.iRecvBytes));
	xbenchPrintMetricU64("server_recv_calls", (uint64_t)xbenchAtomicLoad(&tSrvCtx.iRecvCalls));
	xbenchPrintMetricU64("resume_to_first_recv_ns", iResumeToFirstRecvNs);
	xbenchPrintMetricU64("resume_to_low_water_ns", iResumeToLowWaterNs);
	xbenchPrintMetricU64("resume_to_drain_ns", iResumeToDrainNs);
	xbenchPrintMetricU64("resume_to_pending_zero_ns", iResumeToPendingZeroNs);
	xbenchPrintMetricU64("pending_zero_to_full_recv_ns", iPendingZeroToFullRecvNs);
	printf("high_water=%ld low_water=%ld drain=%ld peak_queue=%ld\n",
		xbenchAtomicLoad(&tCliCtx.iHighWaterCount),
		xbenchAtomicLoad(&tCliCtx.iLowWaterCount),
		xbenchAtomicLoad(&tCliCtx.iDrainCount),
		xbenchAtomicLoad(&tCliCtx.iPeakQueuedBytes));
	printf("client_errors=%ld server_errors=%ld\n", xbenchAtomicLoad(&tCliCtx.iErrorCount), xbenchAtomicLoad(&tSrvCtx.iErrorCount));
	if ( xbenchAtomicLoad(&tCliCtx.iErrorCount) != 0 || xbenchAtomicLoad(&tSrvCtx.iErrorCount) != 0 ||
		xrtNetStreamPendingSend(pClient) != 0u ||
		xbenchAtomicLoad(&tCliCtx.iPeakQueuedBytes) < (long)tConnCfg.iHighWater ||
		(uint64_t)xbenchAtomicLoad(&tSrvCtx.iRecvBytes) < iExpectedRecvBytes ) {
		printf("invalid_pressure_run: pending=%u high_water=%ld peak_queue=%ld goal=%u client_errors=%ld server_errors=%ld\n",
			(unsigned)xrtNetStreamPendingSend(pClient),
			xbenchAtomicLoad(&tCliCtx.iHighWaterCount),
			xbenchAtomicLoad(&tCliCtx.iPeakQueuedBytes),
			(unsigned)tConnCfg.iHighWater,
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
