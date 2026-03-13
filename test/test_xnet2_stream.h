#include "../lib/xnet_stream.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#else
	#include <unistd.h>
#endif


typedef struct {
	volatile long iAcceptCount;
	bool bAccept;
} __test_xnet2_listener_stats;

typedef struct {
	volatile long iOpenCount;
	volatile long iHighWaterCount;
	volatile long iLowWaterCount;
	volatile long iDrainCount;
	volatile long iCloseCount;
	volatile long iReleaseCount;
	volatile long iRecvCount;
	volatile long iRecvTotalBytes;
	volatile long iLastRecvBytes;
	volatile long iLastRecvTextLen;
	volatile long iLastQueuedBytes;
	volatile long iLastCloseReason;
	volatile long iErrorCount;
	bool bEchoBack;
	char aLastRecvText[64];
} __test_xnet2_stream_stats;


static void __Test_XNet2_StreamSleepMs(uint32 iDelayMs)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(iDelayMs);
	#else
		usleep((useconds_t)iDelayMs * 1000u);
	#endif
}

static long __Test_XNet2_StreamAtomicInc(volatile long* pValue)
{
	#if defined(_WIN32) || defined(_WIN64)
		return InterlockedIncrement((volatile LONG*)pValue);
	#else
		return __sync_add_and_fetch(pValue, 1);
	#endif
}

static long __Test_XNet2_StreamAtomicAdd(volatile long* pValue, long iDelta)
{
	#if defined(_WIN32) || defined(_WIN64)
		return InterlockedAdd((volatile LONG*)pValue, iDelta);
	#else
		return __sync_add_and_fetch(pValue, iDelta);
	#endif
}

static long __Test_XNet2_StreamAtomicLoad(volatile long* pValue)
{
	#if defined(_WIN32) || defined(_WIN64)
		return InterlockedCompareExchange((volatile LONG*)pValue, 0, 0);
	#else
		return __sync_add_and_fetch(pValue, 0);
	#endif
}

static void __Test_XNet2_StreamAtomicStore(volatile long* pValue, long iValue)
{
	#if defined(_WIN32) || defined(_WIN64)
		InterlockedExchange((volatile LONG*)pValue, iValue);
	#else
		__sync_lock_test_and_set(pValue, iValue);
	#endif
}

static bool __Test_XNet2_StreamWaitValue(volatile long* pValue, long iExpect, uint32 iTimeoutMs)
{
	uint32 iLoops = (iTimeoutMs / 10u) + 1u;
	for ( uint32 i = 0; i < iLoops; ++i ) {
		if ( __Test_XNet2_StreamAtomicLoad(pValue) == iExpect ) return true;
		__Test_XNet2_StreamSleepMs(10);
	}
	return __Test_XNet2_StreamAtomicLoad(pValue) == iExpect;
}

static bool __Test_XNet2_StreamWaitMin(volatile long* pValue, long iExpectMin, uint32 iTimeoutMs)
{
	uint32 iLoops = (iTimeoutMs / 10u) + 1u;
	for ( uint32 i = 0; i < iLoops; ++i ) {
		if ( __Test_XNet2_StreamAtomicLoad(pValue) >= iExpectMin ) return true;
		__Test_XNet2_StreamSleepMs(10);
	}
	return __Test_XNet2_StreamAtomicLoad(pValue) >= iExpectMin;
}

static bool __Test_XNet2_StreamWaitPendingSend(xnetstream* pStream, size_t iExpect, uint32 iTimeoutMs)
{
	uint32 iLoops = (iTimeoutMs / 10u) + 1u;
	for ( uint32 i = 0; i < iLoops; ++i ) {
		if ( pStream && xrtNetStreamPendingSend(pStream) == iExpect ) return true;
		__Test_XNet2_StreamSleepMs(10);
	}
	return pStream && xrtNetStreamPendingSend(pStream) == iExpect;
}

static bool __Test_XNet2_StreamWaitRxBytes(xnetstream* pStream, size_t iExpect, uint32 iTimeoutMs)
{
	uint32 iLoops = (iTimeoutMs / 10u) + 1u;
	for ( uint32 i = 0; i < iLoops; ++i ) {
		if ( pStream && xrtNetChainBytes(&pStream->tRxChain) == iExpect ) return true;
		__Test_XNet2_StreamSleepMs(10);
	}
	return pStream && xrtNetChainBytes(&pStream->tRxChain) == iExpect;
}


static bool __Test_XNet2_ListenerOnAccept(ptr pOwner, xnetlistener* pListener, xnetstream* pStream)
{
	__test_xnet2_listener_stats* pStats = (__test_xnet2_listener_stats*)pOwner;
	(void)pListener;
	(void)pStream;
	if ( !pStats ) return true;
	__Test_XNet2_StreamAtomicInc(&pStats->iAcceptCount);
	return pStats->bAccept;
}

static void __Test_XNet2_StreamOnHighWater(ptr pOwner, xnetstream* pStream, uint32 iQueuedBytes)
{
	__test_xnet2_stream_stats* pStats = (__test_xnet2_stream_stats*)pOwner;
	(void)pStream;
	if ( !pStats ) return;
	__Test_XNet2_StreamAtomicInc(&pStats->iHighWaterCount);
	__Test_XNet2_StreamAtomicStore(&pStats->iLastQueuedBytes, (long)iQueuedBytes);
}

static void __Test_XNet2_StreamOnOpen(ptr pOwner, xnetstream* pStream)
{
	__test_xnet2_stream_stats* pStats = (__test_xnet2_stream_stats*)pOwner;
	(void)pStream;
	if ( !pStats ) return;
	__Test_XNet2_StreamAtomicInc(&pStats->iOpenCount);
}

static void __Test_XNet2_StreamOnLowWater(ptr pOwner, xnetstream* pStream, uint32 iQueuedBytes)
{
	__test_xnet2_stream_stats* pStats = (__test_xnet2_stream_stats*)pOwner;
	(void)pStream;
	if ( !pStats ) return;
	__Test_XNet2_StreamAtomicInc(&pStats->iLowWaterCount);
	__Test_XNet2_StreamAtomicStore(&pStats->iLastQueuedBytes, (long)iQueuedBytes);
}

static void __Test_XNet2_StreamOnDrain(ptr pOwner, xnetstream* pStream)
{
	__test_xnet2_stream_stats* pStats = (__test_xnet2_stream_stats*)pOwner;
	(void)pStream;
	if ( !pStats ) return;
	__Test_XNet2_StreamAtomicInc(&pStats->iDrainCount);
}

static void __Test_XNet2_StreamOnClose(ptr pOwner, xnetstream* pStream, xnet_result iReason)
{
	__test_xnet2_stream_stats* pStats = (__test_xnet2_stream_stats*)pOwner;
	(void)pStream;
	if ( !pStats ) return;
	__Test_XNet2_StreamAtomicInc(&pStats->iCloseCount);
	__Test_XNet2_StreamAtomicStore(&pStats->iLastCloseReason, (long)iReason);
}

static void __Test_XNet2_StreamOnRecv(ptr pOwner, xnetstream* pStream, xnetchain* pChain)
{
	__test_xnet2_stream_stats* pStats = (__test_xnet2_stream_stats*)pOwner;
	char aBuf[64];
	size_t iBytes = xrtNetChainBytes(pChain);
	if ( !pStats ) return;
	memset(aBuf, 0, sizeof(aBuf));
	__Test_XNet2_StreamAtomicInc(&pStats->iRecvCount);
	__Test_XNet2_StreamAtomicAdd(&pStats->iRecvTotalBytes, (long)iBytes);
	__Test_XNet2_StreamAtomicStore(&pStats->iLastRecvBytes, (long)iBytes);
	if ( iBytes > 0 ) {
		size_t iCopy = iBytes < (sizeof(aBuf) - 1u) ? iBytes : (sizeof(aBuf) - 1u);
		(void)xrtNetChainPeek(pChain, aBuf, iCopy);
		memset(pStats->aLastRecvText, 0, sizeof(pStats->aLastRecvText));
		memcpy(pStats->aLastRecvText, aBuf, iCopy);
		__Test_XNet2_StreamAtomicStore(&pStats->iLastRecvTextLen, (long)iCopy);
		if ( pStats->bEchoBack ) {
			(void)xrtNetStreamSend(pStream, aBuf, iCopy);
		}
	} else {
		pStats->aLastRecvText[0] = '\0';
		__Test_XNet2_StreamAtomicStore(&pStats->iLastRecvTextLen, 0);
	}
	xrtNetChainConsume(pChain, iBytes);
}

static void __Test_XNet2_StreamOnError(ptr pOwner, xnetstream* pStream, int iSysErr)
{
	__test_xnet2_stream_stats* pStats = (__test_xnet2_stream_stats*)pOwner;
	(void)pStream;
	(void)iSysErr;
	if ( !pStats ) return;
	__Test_XNet2_StreamAtomicInc(&pStats->iErrorCount);
}

static void __Test_XNet2_StreamRelease(ptr pCtx, const void* pData, size_t iLen)
{
	volatile long* pReleaseCount = (volatile long*)pCtx;
	(void)pData;
	(void)iLen;
	if ( !pReleaseCount ) return;
	__Test_XNet2_StreamAtomicInc(pReleaseCount);
}


void Test_XNet2_Stream(void)
{
	printf("\n\n\n------------------------------------\n\n XNet2 Stream Skeleton Test:\n\n");

	{
		xnetengineconfig tCfg;
		xnetlistenconfig tListenCfg;
		xnetstreamevents tEvents;
		xnetlistenerevents tListenerEvents;
		xnetspan arrWriteVec[8];
		xnetspan arrVec[2];
		xnetbufref tSendRef;
		xnetbufref tRecvRef;
		xnetengine* pEngine;
		xnetlistener* pListener;
		xnetstream* pClientStream;
		xnetstream* pAccepted1;
		xnetstream* pAccepted2;
		xnetstream* pAbortStream;
		__test_xnet2_listener_stats tListenerStats;
		__test_xnet2_stream_stats tClientStats;
		__test_xnet2_stream_stats tAcceptedStats1;
		__test_xnet2_stream_stats tAcceptedStats2;
		__test_xnet2_stream_stats tAbortStats;
		const char* sSendRef = "XYZ";
		const char* sRecvRef = "1234";
		uint32 iSpanCount = 0;

		memset(&tListenerStats, 0, sizeof(tListenerStats));
		memset(&tClientStats, 0, sizeof(tClientStats));
		memset(&tAcceptedStats1, 0, sizeof(tAcceptedStats1));
		memset(&tAcceptedStats2, 0, sizeof(tAcceptedStats2));
		memset(&tAbortStats, 0, sizeof(tAbortStats));
		memset(&tEvents, 0, sizeof(tEvents));
		memset(&tListenerEvents, 0, sizeof(tListenerEvents));

		tListenerStats.bAccept = true;
		tListenerEvents.OnAccept = __Test_XNet2_ListenerOnAccept;
		tEvents.OnOpen = __Test_XNet2_StreamOnOpen;
		tEvents.OnRecv = __Test_XNet2_StreamOnRecv;
		tEvents.OnDrain = __Test_XNet2_StreamOnDrain;
		tEvents.OnClose = __Test_XNet2_StreamOnClose;
		tEvents.OnError = __Test_XNet2_StreamOnError;
		tEvents.OnHighWater = __Test_XNet2_StreamOnHighWater;
		tEvents.OnLowWater = __Test_XNet2_StreamOnLowWater;

		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 2;
		tCfg.iDefaultHighWater = 8;
		tCfg.iDefaultLowWater = 4;
		pEngine = xrtNetEngineCreate(&tCfg);

		printf("  Engine create for stream test : %s\n", pEngine != NULL ? "PASS" : "FAIL");
		printf("  Engine start for stream test : %s\n",
			pEngine && xrtNetEngineStart(pEngine) == XRT_NET_OK ? "PASS" : "FAIL");

		xrtNetListenConfigInit(&tListenCfg);
		tListenCfg.iFlags |= XNET_LISTEN_F_REUSE_PORT;
		tListenCfg.iHighWater = 10;
		tListenCfg.iLowWater = 4;
		tListenCfg.iRecvLimit = 2048;
		pListener = pEngine ? xrtNetListenerCreate(pEngine, &tListenCfg, &tListenerEvents, &tEvents, &tListenerStats) : NULL;
		printf("  Listener create : %s\n", pListener != NULL ? "PASS" : "FAIL");
		printf("  Listener user data stored : %s\n", pListener && pListener->pUserData == &tListenerStats ? "PASS" : "FAIL");
		printf("  Listener port count follows reuse-port policy : %s\n", pListener && pListener->iPortCount == 2 ? "PASS" : "FAIL");
		printf("  Listener start : %s\n", pListener && xrtNetListenerStart(pListener) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Listener running : %s\n", pListener && pListener->bRunning ? "PASS" : "FAIL");

		pClientStream = pEngine ? xrtNetStreamCreate(pEngine, &tEvents, &tClientStats) : NULL;
		pAccepted1 = pListener ? __xnetListenerCreateAcceptedStream(pListener, &tAcceptedStats1) : NULL;
		pAccepted2 = pListener ? __xnetListenerCreateAcceptedStream(pListener, &tAcceptedStats2) : NULL;
		pAbortStream = pEngine ? xrtNetStreamCreate(pEngine, &tEvents, &tAbortStats) : NULL;

		printf("  Client stream create : %s\n", pClientStream != NULL ? "PASS" : "FAIL");
		printf("  Accepted stream #1 create : %s\n", pAccepted1 != NULL ? "PASS" : "FAIL");
		printf("  Accepted stream #2 create : %s\n", pAccepted2 != NULL ? "PASS" : "FAIL");
		printf("  Accept callback fires twice : %s\n", __Test_XNet2_StreamAtomicLoad(&tListenerStats.iAcceptCount) == 2 ? "PASS" : "FAIL");
		printf("  Accepted stream inherits listener : %s\n", pAccepted1 && pAccepted1->pListener == pListener ? "PASS" : "FAIL");
		printf("  Accepted stream inherits stream events : %s\n", pAccepted1 && pAccepted1->pEvents == &tEvents ? "PASS" : "FAIL");
		printf("  Accepted stream inherits recv limit : %s\n", pAccepted1 && pAccepted1->iRecvLimit == 2048 ? "PASS" : "FAIL");
		printf("  Accepted stream worker round-robin : %s\n",
			pAccepted1 && pAccepted2 && pAccepted1->pWorker && pAccepted2->pWorker &&
			pAccepted1->pWorker->iId != pAccepted2->pWorker->iId ? "PASS" : "FAIL");

		printf("  Client stream worker assigned : %s\n", pClientStream && pClientStream->pWorker != NULL ? "PASS" : "FAIL");
		printf("  Client stream user data getter : %s\n", pClientStream && xrtNetStreamGetUserData(pClientStream) == &tClientStats ? "PASS" : "FAIL");
		printf("  Client stream pending send == 0 : %s\n", pClientStream && xrtNetStreamPendingSend(pClientStream) == 0 ? "PASS" : "FAIL");
		printf("  Client stream connect placeholder error : %s\n", pClientStream && xrtNetStreamConnect(pClientStream, NULL) == XRT_NET_ERROR ? "PASS" : "FAIL");

		printf("  Direct send copy queues bytes : %s\n", pAccepted1 && xrtNetStreamSend(pAccepted1, "abc", 3) == XRT_NET_OK ? "PASS" : "FAIL");
		arrVec[0].pData = "de";
		arrVec[0].iLen = 2;
		arrVec[1].pData = "fgh";
		arrVec[1].iLen = 3;
		printf("  Direct send vec queues bytes : %s\n", pAccepted1 && xrtNetStreamSendVec(pAccepted1, arrVec, 2) == XRT_NET_OK ? "PASS" : "FAIL");

		memset(&tSendRef, 0, sizeof(tSendRef));
		tSendRef.pData = sSendRef;
		tSendRef.iLen = 3;
		tSendRef.pfnRelease = __Test_XNet2_StreamRelease;
		tSendRef.pReleaseCtx = (ptr)&tAcceptedStats1.iReleaseCount;
		printf("  Direct send ref queues bytes : %s\n", pAccepted1 && xrtNetStreamSendRef(pAccepted1, &tSendRef) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Pending send reflects direct queue : %s\n", pAccepted1 && xrtNetStreamPendingSend(pAccepted1) == 11 ? "PASS" : "FAIL");
		printf("  High-water callback fires once : %s\n", __Test_XNet2_StreamAtomicLoad(&tAcceptedStats1.iHighWaterCount) == 1 ? "PASS" : "FAIL");
		printf("  Gather write begins : %s\n", pAccepted1 && __xnetStreamTryBeginWrite(pAccepted1, arrWriteVec, 8, &iSpanCount) ? "PASS" : "FAIL");
		printf("  Gather write returns multiple spans : %s\n", iSpanCount >= 3 ? "PASS" : "FAIL");
		printf("  Write-posted flag set : %s\n", pAccepted1 && pAccepted1->tSendQ.bWritePosted ? "PASS" : "FAIL");

		xrtNetStreamClose(pAccepted1, XNET_CLOSE_F_GRACEFUL);
		printf("  Graceful close waits for queued send drain : %s\n", __Test_XNet2_StreamAtomicLoad(&tAcceptedStats1.iCloseCount) == 0 ? "PASS" : "FAIL");
		printf("  Send after graceful close rejected : %s\n", pAccepted1 && xrtNetStreamSend(pAccepted1, "!", 1) == XRT_NET_CLOSED ? "PASS" : "FAIL");
		printf("  Partial write completion consumes bytes : %s\n", pAccepted1 && __xnetStreamCompleteWrite(pAccepted1, 7) == 7 ? "PASS" : "FAIL");
		printf("  Low-water callback fires : %s\n", __Test_XNet2_StreamAtomicLoad(&tAcceptedStats1.iLowWaterCount) == 1 ? "PASS" : "FAIL");
		printf("  Ref release deferred before final drain : %s\n", __Test_XNet2_StreamAtomicLoad(&tAcceptedStats1.iReleaseCount) == 0 ? "PASS" : "FAIL");
		printf("  Final write completion drains queue : %s\n", pAccepted1 && __xnetStreamCompleteWrite(pAccepted1, 16) == 4 ? "PASS" : "FAIL");
		printf("  Drain callback fires once : %s\n", __Test_XNet2_StreamAtomicLoad(&tAcceptedStats1.iDrainCount) == 1 ? "PASS" : "FAIL");
		printf("  Close callback fires once : %s\n", __Test_XNet2_StreamAtomicLoad(&tAcceptedStats1.iCloseCount) == 1 ? "PASS" : "FAIL");
		printf("  Ref release fires once : %s\n", __Test_XNet2_StreamAtomicLoad(&tAcceptedStats1.iReleaseCount) == 1 ? "PASS" : "FAIL");
		printf("  Pending send returns to zero : %s\n", pAccepted1 && xrtNetStreamPendingSend(pAccepted1) == 0 ? "PASS" : "FAIL");

		printf("  Posted send copy dispatches to owner worker : %s\n", pAccepted2 && __xnetStreamPostSendCopy(pAccepted2, "hello", 5) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Posted send completes through port path : %s\n", __Test_XNet2_StreamWaitMin(&tAcceptedStats2.iDrainCount, 1, 200) ? "PASS" : "FAIL");
		printf("  Posted send pending send returns to zero : %s\n", __Test_XNet2_StreamWaitPendingSend(pAccepted2, 0, 200) ? "PASS" : "FAIL");
		iSpanCount = 0;
		printf("  Posted send leaves no manual gather work : %s\n", pAccepted2 && !__xnetStreamTryBeginWrite(pAccepted2, arrWriteVec, 8, &iSpanCount) ? "PASS" : "FAIL");
		printf("  Posted send write-posted flag clears : %s\n", pAccepted2 && !pAccepted2->tSendQ.bWritePosted ? "PASS" : "FAIL");

		if ( pAccepted2 ) {
			xrtNetStreamPauseRead(pAccepted2);
			printf("  Pause read flag set : %s\n", pAccepted2->bReadPaused ? "PASS" : "FAIL");
		}
		printf("  Posted recv copy dispatches to owner worker : %s\n", pAccepted2 && __xnetStreamPostRecvCopy(pAccepted2, "world", 5) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Paused recv buffers in rx chain : %s\n", __Test_XNet2_StreamWaitRxBytes(pAccepted2, 5, 200) ? "PASS" : "FAIL");
		printf("  Paused recv does not call callback : %s\n", __Test_XNet2_StreamAtomicLoad(&tAcceptedStats2.iRecvCount) == 0 ? "PASS" : "FAIL");
		if ( pAccepted2 ) {
			xrtNetStreamResumeRead(pAccepted2);
		}
		printf("  Resume read flag clears : %s\n", pAccepted2 && !pAccepted2->bReadPaused ? "PASS" : "FAIL");
		printf("  Resume dispatches buffered recv : %s\n", __Test_XNet2_StreamWaitMin(&tAcceptedStats2.iRecvCount, 1, 200) ? "PASS" : "FAIL");
		printf("  Recv callback sees full buffered bytes : %s\n", __Test_XNet2_StreamAtomicLoad(&tAcceptedStats2.iLastRecvBytes) == 5 ? "PASS" : "FAIL");
		printf("  Recv callback consumed chain : %s\n", __Test_XNet2_StreamWaitRxBytes(pAccepted2, 0, 200) ? "PASS" : "FAIL");

		memset(&tRecvRef, 0, sizeof(tRecvRef));
		tRecvRef.pData = sRecvRef;
		tRecvRef.iLen = 4;
		tRecvRef.pfnRelease = __Test_XNet2_StreamRelease;
		tRecvRef.pReleaseCtx = (ptr)&tAcceptedStats2.iReleaseCount;
		printf("  Posted recv ref dispatches to owner worker : %s\n", pAccepted2 && __xnetStreamPostRecvRef(pAccepted2, &tRecvRef) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Recv ref callback executes : %s\n", __Test_XNet2_StreamWaitMin(&tAcceptedStats2.iRecvCount, 2, 200) ? "PASS" : "FAIL");
		printf("  Recv ref release fires once : %s\n", __Test_XNet2_StreamWaitValue(&tAcceptedStats2.iReleaseCount, 1, 200) ? "PASS" : "FAIL");
		printf("  No recv overflow error on staged path : %s\n", __Test_XNet2_StreamAtomicLoad(&tAcceptedStats2.iErrorCount) == 0 ? "PASS" : "FAIL");

		xrtNetStreamSetUserData(pClientStream, NULL);
		printf("  Stream set user data : %s\n", pClientStream && xrtNetStreamGetUserData(pClientStream) == NULL ? "PASS" : "FAIL");

		if ( pAbortStream ) {
			printf("  Abort stream send copy : %s\n", xrtNetStreamSend(pAbortStream, "bye", 3) == XRT_NET_OK ? "PASS" : "FAIL");
			xrtNetStreamClose(pAbortStream, XNET_CLOSE_F_ABORT);
			printf("  Abort close callback fires : %s\n", __Test_XNet2_StreamAtomicLoad(&tAbortStats.iCloseCount) == 1 ? "PASS" : "FAIL");
			printf("  Abort close clears queue : %s\n", xrtNetStreamPendingSend(pAbortStream) == 0 ? "PASS" : "FAIL");
			printf("  Abort close rejects send : %s\n", xrtNetStreamSend(pAbortStream, "z", 1) == XRT_NET_CLOSED ? "PASS" : "FAIL");
		}

		if ( pListener ) {
			xrtNetListenerStop(pListener);
			printf("  Listener stop clears flag : %s\n", !pListener->bRunning ? "PASS" : "FAIL");
		}

		__Test_XNet2_StreamSleepMs(30);

		if ( pAbortStream ) xrtNetStreamDestroy(pAbortStream);
		if ( pAccepted2 ) xrtNetStreamDestroy(pAccepted2);
		if ( pAccepted1 ) xrtNetStreamDestroy(pAccepted1);
		if ( pClientStream ) xrtNetStreamDestroy(pClientStream);
		if ( pListener ) xrtNetListenerDestroy(pListener);
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetlistenconfig tListenCfg;
		xnetconnectconfig tConnectCfg;
		xnetstreamevents tEvents;
		xnetlistenerevents tListenerEvents;
		xnetengine* pEngine;
		xnetlistener* pListener;
		xnetstream* pClient;
		xnetstream* pAccepted;
		__test_xnet2_listener_stats tListenerStats;
		__test_xnet2_stream_stats tClientStats;
		__test_xnet2_stream_stats tAcceptedStats;

		memset(&tListenerStats, 0, sizeof(tListenerStats));
		memset(&tClientStats, 0, sizeof(tClientStats));
		memset(&tAcceptedStats, 0, sizeof(tAcceptedStats));
		memset(&tEvents, 0, sizeof(tEvents));
		memset(&tListenerEvents, 0, sizeof(tListenerEvents));

		tListenerStats.bAccept = true;
		tListenerEvents.OnAccept = __Test_XNet2_ListenerOnAccept;
		tEvents.OnOpen = __Test_XNet2_StreamOnOpen;
		tEvents.OnRecv = __Test_XNet2_StreamOnRecv;
		tEvents.OnDrain = __Test_XNet2_StreamOnDrain;
		tEvents.OnClose = __Test_XNet2_StreamOnClose;
		tEvents.OnError = __Test_XNet2_StreamOnError;

		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tCfg);
		printf("  Loopback engine create : %s\n", pEngine != NULL ? "PASS" : "FAIL");
		printf("  Loopback engine start : %s\n", pEngine && xrtNetEngineStart(pEngine) == XRT_NET_OK ? "PASS" : "FAIL");

		xrtNetListenConfigInit(&tListenCfg);
		xrtNetAddrInitAny(&tListenCfg.tBindAddr, AF_INET, 0);
		pListener = pEngine ? xrtNetListenerCreate(pEngine, &tListenCfg, &tListenerEvents, &tEvents, &tListenerStats) : NULL;
		printf("  Loopback listener create : %s\n", pListener != NULL ? "PASS" : "FAIL");
		printf("  Loopback listener start : %s\n", pListener && xrtNetListenerStart(pListener) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Loopback listener socket valid : %s\n", pListener && __xnetSocketIsValid(pListener->hSocket) ? "PASS" : "FAIL");
		printf("  Loopback listener bound port assigned : %s\n", pListener && pListener->tConfig.tBindAddr.iPort != 0 ? "PASS" : "FAIL");

		xrtNetConnectConfigInit(&tConnectCfg);
		tConnectCfg.sHost = "127.0.0.1";
		tConnectCfg.iPort = pListener ? pListener->tConfig.tBindAddr.iPort : 0;
		pClient = pEngine ? xrtNetStreamCreate(pEngine, &tEvents, &tClientStats) : NULL;
		printf("  Loopback client create : %s\n", pClient != NULL ? "PASS" : "FAIL");
		printf("  Loopback client connect : %s\n", pClient && xrtNetStreamConnect(pClient, &tConnectCfg) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Loopback client socket valid : %s\n", pClient && __xnetSocketIsValid(pClient->hSocket) ? "PASS" : "FAIL");

		pAccepted = NULL;
		for ( int iLoop = 0; iLoop < 50 && !pAccepted; ++iLoop ) {
			pAccepted = __xnetListenerTryAcceptOne(pListener, &tAcceptedStats);
			if ( !pAccepted ) __Test_XNet2_StreamSleepMs(10);
		}

		printf("  Loopback accept returns stream : %s\n", pAccepted != NULL ? "PASS" : "FAIL");
		printf("  Loopback accept callback fires : %s\n", __Test_XNet2_StreamAtomicLoad(&tListenerStats.iAcceptCount) >= 1 ? "PASS" : "FAIL");
		printf("  Loopback accepted socket valid : %s\n", pAccepted && __xnetSocketIsValid(pAccepted->hSocket) ? "PASS" : "FAIL");
		printf("  Loopback client open callback : %s\n", __Test_XNet2_StreamWaitMin(&tClientStats.iOpenCount, 1, 200) ? "PASS" : "FAIL");
		printf("  Loopback accepted open callback : %s\n", __Test_XNet2_StreamWaitMin(&tAcceptedStats.iOpenCount, 1, 200) ? "PASS" : "FAIL");
		printf("  Loopback client remote port matches listener : %s\n",
			pClient && pClient->tRemoteAddr.iPort == tConnectCfg.iPort ? "PASS" : "FAIL");
		printf("  Loopback accepted local port matches listener : %s\n",
			pAccepted && pAccepted->tLocalAddr.iPort == tConnectCfg.iPort ? "PASS" : "FAIL");
		printf("  Loopback client local port assigned : %s\n",
			pClient && pClient->tLocalAddr.iPort != 0 ? "PASS" : "FAIL");
		if ( pAccepted ) {
			tAcceptedStats.bEchoBack = true;
		}
		printf("  Loopback client send posts to owner worker : %s\n",
			pClient && xrtNetStreamSend(pClient, "ping", 4) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Loopback accepted recv auto-harvest : %s\n",
			__Test_XNet2_StreamWaitMin(&tAcceptedStats.iRecvCount, 1, 300) ? "PASS" : "FAIL");
		printf("  Loopback accepted sees ping : %s\n",
			strcmp(tAcceptedStats.aLastRecvText, "ping") == 0 ? "PASS" : "FAIL");
		printf("  Loopback accepted echo drains : %s\n",
			__Test_XNet2_StreamWaitMin(&tAcceptedStats.iDrainCount, 1, 300) ? "PASS" : "FAIL");
		printf("  Loopback client recv auto-harvest : %s\n",
			__Test_XNet2_StreamWaitMin(&tClientStats.iRecvCount, 1, 300) ? "PASS" : "FAIL");
		printf("  Loopback client sees echoed ping : %s\n",
			strcmp(tClientStats.aLastRecvText, "ping") == 0 ? "PASS" : "FAIL");
		printf("  Loopback echo path stays error free : %s\n",
			__Test_XNet2_StreamAtomicLoad(&tClientStats.iErrorCount) == 0 &&
			__Test_XNet2_StreamAtomicLoad(&tAcceptedStats.iErrorCount) == 0 ? "PASS" : "FAIL");

		if ( pClient ) {
			xrtNetStreamClose(pClient, XNET_CLOSE_F_ABORT);
		}
		if ( pAccepted ) {
			xrtNetStreamClose(pAccepted, XNET_CLOSE_F_ABORT);
		}
		printf("  Loopback client close callback : %s\n", __Test_XNet2_StreamWaitMin(&tClientStats.iCloseCount, 1, 200) ? "PASS" : "FAIL");
		printf("  Loopback accepted close callback : %s\n", __Test_XNet2_StreamWaitMin(&tAcceptedStats.iCloseCount, 1, 200) ? "PASS" : "FAIL");
		printf("  Loopback client socket closed : %s\n", pClient && !__xnetSocketIsValid(pClient->hSocket) ? "PASS" : "FAIL");
		printf("  Loopback accepted socket closed : %s\n", pAccepted && !__xnetSocketIsValid(pAccepted->hSocket) ? "PASS" : "FAIL");

		if ( pAccepted ) xrtNetStreamDestroy(pAccepted);
		if ( pClient ) xrtNetStreamDestroy(pClient);
		if ( pListener ) {
			xrtNetListenerStop(pListener);
			printf("  Loopback listener stop closes socket : %s\n", !__xnetSocketIsValid(pListener->hSocket) ? "PASS" : "FAIL");
			xrtNetListenerDestroy(pListener);
		}
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetlistenconfig tListenCfg;
		xnetstreamevents tEvents;
		xnetlistenerevents tListenerEvents;
		xnetengine* pEngine;
		xnetlistener* pListener;
		xnetstream* pAccepted;
		__test_xnet2_listener_stats tListenerStats;
		__test_xnet2_stream_stats tServerStats;
		xsocket hPeer = XNET_SOCKET_INVALID;
		struct sockaddr_storage tStorage;
		socklen_t iAddrLen = 0;
		xnetaddr tPeerAddr;
		char* pPayload = NULL;
		size_t iPayloadLen = 1024u * 1024u;
		size_t iReadTotal = 0;
		int iSockBuf = 4096;
		char aBuf[8192];

		memset(&tListenerStats, 0, sizeof(tListenerStats));
		memset(&tServerStats, 0, sizeof(tServerStats));
		memset(&tEvents, 0, sizeof(tEvents));
		memset(&tListenerEvents, 0, sizeof(tListenerEvents));
		memset(&tStorage, 0, sizeof(tStorage));
		memset(&tPeerAddr, 0, sizeof(tPeerAddr));

		tListenerStats.bAccept = true;
		tListenerEvents.OnAccept = __Test_XNet2_ListenerOnAccept;
		tEvents.OnOpen = __Test_XNet2_StreamOnOpen;
		tEvents.OnDrain = __Test_XNet2_StreamOnDrain;
		tEvents.OnClose = __Test_XNet2_StreamOnClose;
		tEvents.OnError = __Test_XNet2_StreamOnError;
		tEvents.OnHighWater = __Test_XNet2_StreamOnHighWater;
		tEvents.OnLowWater = __Test_XNet2_StreamOnLowWater;

		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tCfg);
		printf("  Slow-peer engine create : %s\n", pEngine != NULL ? "PASS" : "FAIL");
		printf("  Slow-peer engine start : %s\n", pEngine && xrtNetEngineStart(pEngine) == XRT_NET_OK ? "PASS" : "FAIL");

		xrtNetListenConfigInit(&tListenCfg);
		xrtNetAddrInitAny(&tListenCfg.tBindAddr, AF_INET, 0);
		tListenCfg.iHighWater = 32768;
		tListenCfg.iLowWater = 8192;
		pListener = pEngine ? xrtNetListenerCreate(pEngine, &tListenCfg, &tListenerEvents, &tEvents, &tListenerStats) : NULL;
		printf("  Slow-peer listener create : %s\n", pListener != NULL ? "PASS" : "FAIL");
		printf("  Slow-peer listener start : %s\n", pListener && xrtNetListenerStart(pListener) == XRT_NET_OK ? "PASS" : "FAIL");

		if ( pListener ) {
			(void)xrtNetAddrParse(&tPeerAddr, "127.0.0.1", pListener->tConfig.tBindAddr.iPort);
			(void)__xnetAddrToSockAddr(&tPeerAddr, &tStorage, &iAddrLen);
			hPeer = socket(pListener->tConfig.tBindAddr.iFamily, SOCK_STREAM, IPPROTO_TCP);
		}
		printf("  Slow-peer raw client socket create : %s\n", __xnetSocketIsValid(hPeer) ? "PASS" : "FAIL");
		printf("  Slow-peer raw client connect : %s\n",
			__xnetSocketIsValid(hPeer) && connect(hPeer, (struct sockaddr*)&tStorage, iAddrLen) == 0 ? "PASS" : "FAIL");

		pAccepted = NULL;
		for ( int iLoop = 0; iLoop < 50 && !pAccepted; ++iLoop ) {
			pAccepted = __xnetListenerTryAcceptOne(pListener, &tServerStats);
			if ( !pAccepted ) __Test_XNet2_StreamSleepMs(10);
		}

		printf("  Slow-peer accept returns stream : %s\n", pAccepted != NULL ? "PASS" : "FAIL");
		printf("  Slow-peer accepted open callback : %s\n", __Test_XNet2_StreamWaitMin(&tServerStats.iOpenCount, 1, 300) ? "PASS" : "FAIL");
		if ( __xnetSocketIsValid(hPeer) ) {
			#if defined(_WIN32) || defined(_WIN64)
				setsockopt(hPeer, SOL_SOCKET, SO_RCVBUF, (const char*)&iSockBuf, sizeof(iSockBuf));
			#else
				setsockopt(hPeer, SOL_SOCKET, SO_RCVBUF, &iSockBuf, sizeof(iSockBuf));
			#endif
		}
		if ( pAccepted && __xnetSocketIsValid(pAccepted->hSocket) ) {
			#if defined(_WIN32) || defined(_WIN64)
				setsockopt(pAccepted->hSocket, SOL_SOCKET, SO_SNDBUF, (const char*)&iSockBuf, sizeof(iSockBuf));
			#else
				setsockopt(pAccepted->hSocket, SOL_SOCKET, SO_SNDBUF, &iSockBuf, sizeof(iSockBuf));
			#endif
		}

		pPayload = (char*)XNET_ALLOC(iPayloadLen);
		if ( pPayload ) memset(pPayload, 'S', iPayloadLen);
		printf("  Slow-peer payload alloc : %s\n", pPayload != NULL ? "PASS" : "FAIL");
		printf("  Slow-peer server large send starts : %s\n",
			pAccepted && pPayload && xrtNetStreamSend(pAccepted, pPayload, iPayloadLen) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Slow-peer high-water or queued bytes observed : %s\n",
			__Test_XNet2_StreamWaitMin(&tServerStats.iHighWaterCount, 1, 500) ||
			(pAccepted && !__Test_XNet2_StreamWaitPendingSend(pAccepted, 0, 200)) ? "PASS" : "FAIL");

		if ( __xnetSocketIsValid(hPeer) ) {
			(void)__xnetSocketSetNonBlock(hPeer, true);
		}
		for ( uint32 iLoop = 0; iLoop < 300 && iReadTotal < iPayloadLen; ++iLoop ) {
			int iRecv = __xnetSocketIsValid(hPeer) ? recv(hPeer, aBuf, sizeof(aBuf), 0) : -1;
			if ( iRecv > 0 ) {
				iReadTotal += (size_t)iRecv;
				continue;
			}
			if ( iRecv < 0 && __xnetSocketWouldBlock(__xnetSocketLastErr()) ) {
				__Test_XNet2_StreamSleepMs(10);
				continue;
			}
			if ( iRecv == 0 ) {
				break;
			}
			__Test_XNet2_StreamSleepMs(10);
		}

		printf("  Slow-peer raw client drains payload : %s\n", iReadTotal == iPayloadLen ? "PASS" : "FAIL");
		printf("  Slow-peer drain callback fires : %s\n", __Test_XNet2_StreamWaitMin(&tServerStats.iDrainCount, 1, 1000) ? "PASS" : "FAIL");
		printf("  Slow-peer pending send returns zero : %s\n", pAccepted && __Test_XNet2_StreamWaitPendingSend(pAccepted, 0, 1000) ? "PASS" : "FAIL");
		printf("  Slow-peer path stays error free : %s\n", __Test_XNet2_StreamAtomicLoad(&tServerStats.iErrorCount) == 0 ? "PASS" : "FAIL");

		if ( __xnetSocketIsValid(hPeer) ) {
			__xnetSocketCloseHandle(&hPeer);
		}
		if ( pAccepted ) {
			xrtNetStreamClose(pAccepted, XNET_CLOSE_F_ABORT);
			xrtNetStreamDestroy(pAccepted);
		}
		if ( pPayload ) XNET_FREE(pPayload);
		if ( pListener ) {
			xrtNetListenerStop(pListener);
			xrtNetListenerDestroy(pListener);
		}
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}
}
