#include "../lib/xnet_sync.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#else
	#include <time.h>
	#include <unistd.h>
#endif


typedef struct {
	xnetfuture* pFuture;
	xnet_result iResult;
	ptr pValue;
	volatile long iHitCount;
	volatile long iWorkerId;
} __test_xnet2_sync_task_ctx;

#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
typedef struct {
	xnetfuture* pFuture;
	xnetstream* pStream;
	int iPayload;
	xnet_result iWaitStatus;
	xnet_result iTimeoutStatus;
	xnet_result iDeadlineStatus;
	ptr pResolvedValue;
	ptr pTimeoutValue;
	ptr pDeadlineValue;
	bool bDone;
	bool bTimeoutDone;
	bool bDeadlineDone;
} __test_xnet2_sync_coro_case;

typedef struct {
	int iPayload;
	volatile long iHitCount;
	volatile long iWorkerId;
} __test_xnet2_sync_postfuture_case;

typedef struct {
	xdgramsock* pSock;
	xnet_result iWaitStatus;
	xnet_result iTimeoutStatus;
	xnet_result iDeadlineStatus;
	xnetdgrampkt* pPacket;
	xnetdgrampkt* pTimeoutPacket;
	bool bDone;
	bool bTimeoutDone;
	bool bDeadlineDone;
} __test_xnet2_sync_dgram_coro_case;
#endif

typedef struct {
	xdgramsock* pSock;
	xnetaddr tPeerAddr;
	const char* sPayload;
	uint32 iDelayMs;
	xnet_result iSendStatus;
	bool bDone;
} __test_xnet2_sync_dgram_send_case;

typedef struct {
	volatile long iHitCount;
	volatile long iPayloadLen;
	volatile long iFromPort;
	char aPayload[64];
} __test_xnet2_sync_dgram_event_case;


static void __Test_XNet2_SyncSleepMs(uint32 iDelayMs)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(iDelayMs);
	#else
		usleep((useconds_t)iDelayMs * 1000u);
	#endif
}

static long __Test_XNet2_SyncAtomicInc(volatile long* pValue)
{
	#if defined(_WIN32) || defined(_WIN64)
		return InterlockedIncrement((volatile LONG*)pValue);
	#else
		return __sync_add_and_fetch(pValue, 1);
	#endif
}

static void __Test_XNet2_SyncAtomicStore(volatile long* pValue, long iValue)
{
	#if defined(_WIN32) || defined(_WIN64)
		InterlockedExchange((volatile LONG*)pValue, iValue);
	#else
		__sync_lock_test_and_set(pValue, iValue);
	#endif
}

static int64_t __Test_XNet2_SyncNowMs(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		return (int64_t)GetTickCount64();
	#else
		struct timespec tNow;
		if ( clock_gettime(CLOCK_MONOTONIC, &tNow) != 0 ) return 0;
		return ((int64_t)tNow.tv_sec * 1000LL) + ((int64_t)tNow.tv_nsec / 1000000LL);
	#endif
}

static bool __Test_XNet2_SyncDgramPacketTextEquals(const xnetdgrampkt* pPacket, const char* sText)
{
	char aBuf[128];
	size_t iWantLen;
	size_t iCopyLen;

	if ( !pPacket || !sText ) return false;
	memset(aBuf, 0, sizeof(aBuf));
	iWantLen = strlen(sText);
	iCopyLen = xrtNetDgramPacketPeek(pPacket, aBuf, sizeof(aBuf) - 1u);
	return iCopyLen == iWantLen && memcmp(aBuf, sText, iWantLen) == 0;
}

static bool __Test_XNet2_SyncDgramEventTextEquals(const __test_xnet2_sync_dgram_event_case* pCase, const char* sText)
{
	size_t iWantLen;

	if ( !pCase || !sText ) return false;
	iWantLen = strlen(sText);
	return pCase->iPayloadLen == (long)iWantLen && memcmp(pCase->aPayload, sText, iWantLen) == 0;
}

static void __Test_XNet2_SyncDgramOnRecv(ptr pOwner, xdgramsock* pSock, const xnetaddr* pFrom, xnetchain* pChain)
{
	__test_xnet2_sync_dgram_event_case* pCase = (__test_xnet2_sync_dgram_event_case*)pOwner;
	size_t iCopyLen = 0;

	(void)pSock;
	if ( !pCase || !pFrom || !pChain ) return;
	memset(pCase->aPayload, 0, sizeof(pCase->aPayload));
	iCopyLen = xrtNetChainPeek(pChain, pCase->aPayload, sizeof(pCase->aPayload) - 1u);
	__Test_XNet2_SyncAtomicStore(&pCase->iPayloadLen, (long)iCopyLen);
	__Test_XNet2_SyncAtomicStore(&pCase->iFromPort, (long)pFrom->iPort);
	(void)__Test_XNet2_SyncAtomicInc(&pCase->iHitCount);
}

static uint32 __Test_XNet2_SyncDgramSendWorker(ptr pArg)
{
	__test_xnet2_sync_dgram_send_case* pCase = (__test_xnet2_sync_dgram_send_case*)pArg;
	size_t iLen = 0;

	if ( !pCase || !pCase->pSock || !pCase->sPayload ) return 0;
	if ( pCase->iDelayMs > 0 ) {
		__Test_XNet2_SyncSleepMs(pCase->iDelayMs);
	}
	iLen = strlen(pCase->sPayload);
	pCase->iSendStatus = xrtNetDgramSendTo(pCase->pSock, &pCase->tPeerAddr, pCase->sPayload, iLen);
	pCase->bDone = TRUE;
	return 0;
}

static void __Test_XNet2_SyncResolveTask(xnetworker* pWorker, ptr pArg)
{
	__test_xnet2_sync_task_ctx* pCtx = (__test_xnet2_sync_task_ctx*)pArg;
	if ( !pCtx || !pCtx->pFuture ) return;
	__Test_XNet2_SyncAtomicInc(&pCtx->iHitCount);
	__Test_XNet2_SyncAtomicStore(&pCtx->iWorkerId, pWorker ? (long)pWorker->iId : -1);
	(void)__xnetFutureResolve(pCtx->pFuture, pCtx->iResult, pCtx->pValue);
}

#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
static void __Test_XNet2_SyncFutureWaitCo(ptr pArg)
{
	__test_xnet2_sync_coro_case* pCase = (__test_xnet2_sync_coro_case*)pArg;

	if ( !pCase ) return;
	pCase->iWaitStatus = xrtNetFutureWaitCo(pCase->pFuture);
	pCase->pResolvedValue = xrtNetFutureValue(pCase->pFuture);
	pCase->bDone = TRUE;
}

static void __Test_XNet2_SyncWaitSourceFutureCo(ptr pArg)
{
	__test_xnet2_sync_coro_case* pCase = (__test_xnet2_sync_coro_case*)pArg;
	xnetwaitsrc tSrc;

	if ( !pCase ) return;
	tSrc = xrtNetWaitSourceFuture(pCase->pFuture);
	pCase->iWaitStatus = xrtNetWaitSourceWaitCoValue(&tSrc, &pCase->pResolvedValue);
	pCase->bDone = TRUE;
}

static void __Test_XNet2_SyncFutureWaitCoTimeout(ptr pArg)
{
	__test_xnet2_sync_coro_case* pCase = (__test_xnet2_sync_coro_case*)pArg;

	if ( !pCase ) return;
	pCase->iTimeoutStatus = xrtNetFutureWaitCoTimeout(pCase->pFuture, 40);
	pCase->pTimeoutValue = xrtNetFutureValue(pCase->pFuture);
	pCase->bTimeoutDone = TRUE;
}

static void __Test_XNet2_SyncWaitSourceFutureCoTimeout(ptr pArg)
{
	__test_xnet2_sync_coro_case* pCase = (__test_xnet2_sync_coro_case*)pArg;
	xnetwaitsrc tSrc;

	if ( !pCase ) return;
	tSrc = xrtNetWaitSourceFuture(pCase->pFuture);
	pCase->iTimeoutStatus = xrtNetWaitSourceWaitCoValueTimeout(&tSrc, 40, &pCase->pTimeoutValue);
	pCase->bTimeoutDone = TRUE;
}

static void __Test_XNet2_SyncFutureWaitCoUntil(ptr pArg)
{
	__test_xnet2_sync_coro_case* pCase = (__test_xnet2_sync_coro_case*)pArg;
	int64 iNowMs = 0;

	if ( !pCase ) return;
	iNowMs = (int64)(xrtTimer() * 1000.0);
	pCase->iDeadlineStatus = xrtNetFutureWaitCoUntil(pCase->pFuture, iNowMs + 45);
	pCase->pDeadlineValue = xrtNetFutureValue(pCase->pFuture);
	pCase->bDeadlineDone = TRUE;
}

static void __Test_XNet2_SyncStreamReadableWaitCo(ptr pArg)
{
	__test_xnet2_sync_coro_case* pCase = (__test_xnet2_sync_coro_case*)pArg;

	if ( !pCase ) return;
	pCase->iWaitStatus = xrtNetStreamWaitReadableCo(pCase->pStream);
	pCase->bDone = TRUE;
}

static void __Test_XNet2_SyncStreamWaitCoExReadable(ptr pArg)
{
	__test_xnet2_sync_coro_case* pCase = (__test_xnet2_sync_coro_case*)pArg;

	if ( !pCase ) return;
	pCase->iWaitStatus = xrtNetStreamWaitCoEx(pCase->pStream, XNET_STREAM_WAIT_READABLE);
	pCase->bDone = TRUE;
}

static void __Test_XNet2_SyncWaitSourceStreamReadableCo(ptr pArg)
{
	__test_xnet2_sync_coro_case* pCase = (__test_xnet2_sync_coro_case*)pArg;
	xnetwaitsrc tSrc;

	if ( !pCase ) return;
	tSrc = xrtNetWaitSourceStream(pCase->pStream, XNET_STREAM_WAIT_READABLE);
	pCase->iWaitStatus = xrtNetWaitSourceWaitCoValue(&tSrc, &pCase->pResolvedValue);
	pCase->bDone = TRUE;
}

static void __Test_XNet2_SyncStreamReadableWaitCoTimeout(ptr pArg)
{
	__test_xnet2_sync_coro_case* pCase = (__test_xnet2_sync_coro_case*)pArg;

	if ( !pCase ) return;
	pCase->iTimeoutStatus = xrtNetStreamWaitReadableCoTimeout(pCase->pStream, 30);
	pCase->bTimeoutDone = TRUE;
}

static void __Test_XNet2_SyncWaitSourceStreamReadableCoTimeout(ptr pArg)
{
	__test_xnet2_sync_coro_case* pCase = (__test_xnet2_sync_coro_case*)pArg;
	xnetwaitsrc tSrc;

	if ( !pCase ) return;
	tSrc = xrtNetWaitSourceStream(pCase->pStream, XNET_STREAM_WAIT_READABLE);
	pCase->iTimeoutStatus = xrtNetWaitSourceWaitCoValueTimeout(&tSrc, 30, &pCase->pTimeoutValue);
	pCase->bTimeoutDone = TRUE;
}

static void __Test_XNet2_SyncWaitSourceStreamWritableCo(ptr pArg)
{
	__test_xnet2_sync_coro_case* pCase = (__test_xnet2_sync_coro_case*)pArg;
	xnetwaitsrc tSrc;

	if ( !pCase ) return;
	tSrc = xrtNetWaitSourceStream(pCase->pStream, XNET_STREAM_WAIT_WRITABLE);
	pCase->iWaitStatus = xrtNetWaitSourceWaitCo(&tSrc);
	pCase->bDone = TRUE;
}

static void __Test_XNet2_SyncWaitSourceStreamWritableCoUntil(ptr pArg)
{
	__test_xnet2_sync_coro_case* pCase = (__test_xnet2_sync_coro_case*)pArg;
	xnetwaitsrc tSrc;
	int64 iNowMs = 0;

	if ( !pCase ) return;
	tSrc = xrtNetWaitSourceStream(pCase->pStream, XNET_STREAM_WAIT_WRITABLE);
	iNowMs = (int64)(xrtTimer() * 1000.0);
	pCase->iDeadlineStatus = xrtNetWaitSourceWaitCoUntil(&tSrc, iNowMs + 35);
	pCase->bDeadlineDone = TRUE;
}

static void __Test_XNet2_SyncStreamWritableWaitCo(ptr pArg)
{
	__test_xnet2_sync_coro_case* pCase = (__test_xnet2_sync_coro_case*)pArg;

	if ( !pCase ) return;
	pCase->iWaitStatus = xrtNetStreamWaitWritableCo(pCase->pStream);
	pCase->bDone = TRUE;
}

static void __Test_XNet2_SyncStreamWritableWaitCoUntil(ptr pArg)
{
	__test_xnet2_sync_coro_case* pCase = (__test_xnet2_sync_coro_case*)pArg;
	int64 iNowMs = 0;

	if ( !pCase ) return;
	iNowMs = (int64)(xrtTimer() * 1000.0);
	pCase->iDeadlineStatus = xrtNetStreamWaitWritableCoUntil(pCase->pStream, iNowMs + 35);
	pCase->bDeadlineDone = TRUE;
}

static void __Test_XNet2_SyncWaitSourceStreamDrainCoTimeout(ptr pArg)
{
	__test_xnet2_sync_coro_case* pCase = (__test_xnet2_sync_coro_case*)pArg;
	xnetwaitsrc tSrc;

	if ( !pCase ) return;
	tSrc = xrtNetWaitSourceStream(pCase->pStream, XNET_STREAM_WAIT_DRAIN);
	pCase->iTimeoutStatus = xrtNetWaitSourceWaitCoTimeout(&tSrc, 30);
	pCase->bTimeoutDone = TRUE;
}

static void __Test_XNet2_SyncWaitSourceStreamDrainCo(ptr pArg)
{
	__test_xnet2_sync_coro_case* pCase = (__test_xnet2_sync_coro_case*)pArg;
	xnetwaitsrc tSrc;

	if ( !pCase ) return;
	tSrc = xrtNetWaitSourceStream(pCase->pStream, XNET_STREAM_WAIT_DRAIN);
	pCase->iWaitStatus = xrtNetWaitSourceWaitCo(&tSrc);
	pCase->bDone = TRUE;
}

static void __Test_XNet2_SyncStreamDrainWaitCoTimeout(ptr pArg)
{
	__test_xnet2_sync_coro_case* pCase = (__test_xnet2_sync_coro_case*)pArg;

	if ( !pCase ) return;
	pCase->iTimeoutStatus = xrtNetStreamWaitDrainCoTimeout(pCase->pStream, 30);
	pCase->bTimeoutDone = TRUE;
}

static void __Test_XNet2_SyncStreamDrainWaitCo(ptr pArg)
{
	__test_xnet2_sync_coro_case* pCase = (__test_xnet2_sync_coro_case*)pArg;

	if ( !pCase ) return;
	pCase->iWaitStatus = xrtNetStreamWaitDrainCo(pCase->pStream);
	pCase->bDone = TRUE;
}

static void __Test_XNet2_SyncWaitSourceStreamCloseCoUntil(ptr pArg)
{
	__test_xnet2_sync_coro_case* pCase = (__test_xnet2_sync_coro_case*)pArg;
	xnetwaitsrc tSrc;
	int64 iNowMs = 0;

	if ( !pCase ) return;
	tSrc = xrtNetWaitSourceStream(pCase->pStream, XNET_STREAM_WAIT_CLOSE);
	iNowMs = (int64)(xrtTimer() * 1000.0);
	pCase->iDeadlineStatus = xrtNetWaitSourceWaitCoUntil(&tSrc, iNowMs + 35);
	pCase->bDeadlineDone = TRUE;
}

static void __Test_XNet2_SyncWaitSourceStreamCloseCo(ptr pArg)
{
	__test_xnet2_sync_coro_case* pCase = (__test_xnet2_sync_coro_case*)pArg;
	xnetwaitsrc tSrc;

	if ( !pCase ) return;
	tSrc = xrtNetWaitSourceStream(pCase->pStream, XNET_STREAM_WAIT_CLOSE);
	pCase->iWaitStatus = xrtNetWaitSourceWaitCo(&tSrc);
	pCase->bDone = TRUE;
}

static void __Test_XNet2_SyncStreamCloseWaitCoUntil(ptr pArg)
{
	__test_xnet2_sync_coro_case* pCase = (__test_xnet2_sync_coro_case*)pArg;
	int64 iNowMs = 0;

	if ( !pCase ) return;
	iNowMs = (int64)(xrtTimer() * 1000.0);
	pCase->iDeadlineStatus = xrtNetStreamWaitCloseCoUntil(pCase->pStream, iNowMs + 35);
	pCase->bDeadlineDone = TRUE;
}

static void __Test_XNet2_SyncStreamCloseWaitCo(ptr pArg)
{
	__test_xnet2_sync_coro_case* pCase = (__test_xnet2_sync_coro_case*)pArg;

	if ( !pCase ) return;
	pCase->iWaitStatus = xrtNetStreamWaitCloseCo(pCase->pStream);
	pCase->bDone = TRUE;
}

static uint32 __Test_XNet2_SyncFutureResolveWorker(ptr pArg)
{
	__test_xnet2_sync_coro_case* pCase = (__test_xnet2_sync_coro_case*)pArg;

	if ( !pCase || !pCase->pFuture ) return 0;
	__Test_XNet2_SyncSleepMs(50);
	(void)__xnetFutureResolve(pCase->pFuture, XRT_NET_OK, &pCase->iPayload);
	return 0;
}

static xnet_result __Test_XNet2_SyncPostFutureTask(xnetworker* pWorker, ptr pArg, ptr* ppValue)
{
	__test_xnet2_sync_postfuture_case* pCase = (__test_xnet2_sync_postfuture_case*)pArg;

	if ( !pCase ) {
		return XRT_NET_ERROR;
	}

	__Test_XNet2_SyncAtomicInc(&pCase->iHitCount);
	__Test_XNet2_SyncAtomicStore(&pCase->iWorkerId, pWorker ? (long)pWorker->iId : -1);
	if ( ppValue ) {
		*ppValue = &pCase->iPayload;
	}
	return XRT_NET_OK;
}

static void __Test_XNet2_SyncStreamDrainTask(xnetworker* pWorker, ptr pArg)
{
	xnetstream* pStream = (xnetstream*)pArg;
	size_t iPending = 0;

	(void)pWorker;
	if ( !pStream ) return;
	iPending = xrtNetStreamPendingSend(pStream);
	if ( iPending > 0 ) {
		(void)__xnetStreamCompleteWrite(pStream, iPending);
	}
}

static void __Test_XNet2_SyncStreamCloseTask(xnetworker* pWorker, ptr pArg)
{
	xnetstream* pStream = (xnetstream*)pArg;

	(void)pWorker;
	if ( !pStream ) return;
	xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
}

static void __Test_XNet2_SyncStreamWritableTask(xnetworker* pWorker, ptr pArg)
{
	xnetstream* pStream = (xnetstream*)pArg;
	size_t iPending = 0;

	(void)pWorker;
	if ( !pStream ) return;
	iPending = xrtNetStreamPendingSend(pStream);
	if ( iPending > 8 ) iPending = 8;
	if ( iPending > 0 ) {
		(void)__xnetStreamCompleteWrite(pStream, iPending);
	}
}

static void __Test_XNet2_SyncDgramRecvCo(ptr pArg)
{
	__test_xnet2_sync_dgram_coro_case* pCase = (__test_xnet2_sync_dgram_coro_case*)pArg;

	if ( !pCase ) return;
	pCase->iWaitStatus = xrtNetDgramRecvCo(pCase->pSock, &pCase->pPacket);
	pCase->bDone = TRUE;
}

static void __Test_XNet2_SyncWaitSourceDgramRecvCo(ptr pArg)
{
	__test_xnet2_sync_dgram_coro_case* pCase = (__test_xnet2_sync_dgram_coro_case*)pArg;
	xnetwaitsrc tSrc;

	if ( !pCase ) return;
	tSrc = xrtNetWaitSourceDgramRecv(pCase->pSock);
	pCase->iWaitStatus = xrtNetWaitSourceWaitCoValue(&tSrc, (ptr*)&pCase->pPacket);
	pCase->bDone = TRUE;
}

static void __Test_XNet2_SyncWaitSourceDgramRecvCoTimeout(ptr pArg)
{
	__test_xnet2_sync_dgram_coro_case* pCase = (__test_xnet2_sync_dgram_coro_case*)pArg;
	xnetwaitsrc tSrc;

	if ( !pCase ) return;
	tSrc = xrtNetWaitSourceDgramRecv(pCase->pSock);
	pCase->iTimeoutStatus = xrtNetWaitSourceWaitCoValueTimeout(&tSrc, 30, (ptr*)&pCase->pTimeoutPacket);
	pCase->bTimeoutDone = TRUE;
}

static void __Test_XNet2_SyncDgramRecvCoUntil(ptr pArg)
{
	__test_xnet2_sync_dgram_coro_case* pCase = (__test_xnet2_sync_dgram_coro_case*)pArg;

	if ( !pCase ) return;
	pCase->iDeadlineStatus = xrtNetDgramRecvCoUntil(pCase->pSock, __Test_XNet2_SyncNowMs() + 30, &pCase->pTimeoutPacket);
	pCase->bDeadlineDone = TRUE;
}
#endif


void Test_XNet2_Sync(void)
{
	printf("\n\n\n------------------------------------\n\n XNet2 Sync Skeleton Test:\n\n");

	{
		xnetfuture* pFuture = xrtNetFutureCreate();
		int iValue = 1234;
		xnetwaitsrc tSrc = xrtNetWaitSourceNone();
		ptr pWaitValue = NULL;
		printf("  Future create : %s\n", pFuture != NULL ? "PASS" : "FAIL");
		printf("  Future pending status : %s\n",
			pFuture && xrtNetFutureStatus(pFuture) == XRT_NET_AGAIN ? "PASS" : "FAIL");
		printf("  Future wait timeout : %s\n",
			pFuture && xrtNetFutureWait(pFuture, 20) == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Future value pending : %s\n",
			pFuture && xrtNetFutureValue(pFuture) == NULL ? "PASS" : "FAIL");
		printf("  Future first resolve : %s\n",
			pFuture && __xnetFutureResolve(pFuture, XRT_NET_OK, &iValue) ? "PASS" : "FAIL");
		printf("  Future second resolve ignored : %s\n",
			pFuture && !__xnetFutureResolve(pFuture, XRT_NET_ERROR, NULL) ? "PASS" : "FAIL");
		printf("  Future wait after resolve : %s\n",
			pFuture && xrtNetFutureWait(pFuture, 0) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Future final status : %s\n",
			pFuture && xrtNetFutureStatus(pFuture) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Future value after resolve : %s\n",
			pFuture && xrtNetFutureValue(pFuture) == &iValue ? "PASS" : "FAIL");
		if ( pFuture ) {
			tSrc = xrtNetWaitSourceFuture(pFuture);
		}
		printf("  Wait source future sync wait : %s\n",
			pFuture && xrtNetWaitSourceWait(&tSrc) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Wait source future sync value wait : %s\n",
			pFuture && xrtNetWaitSourceWaitValue(&tSrc, &pWaitValue) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Wait source future sync value matches : %s\n",
			pFuture && pWaitValue == &iValue ? "PASS" : "FAIL");
		if ( pFuture ) {
			__xnetFutureReset(pFuture);
			printf("  Future reset to pending : %s\n",
				xrtNetFutureStatus(pFuture) == XRT_NET_AGAIN ? "PASS" : "FAIL");
			xrtNetFutureDestroy(pFuture);
		}
	}

	{
		xnetwaitsrc tInvalidSrc = xrtNetWaitSourceNone();
		xnet_result iInvalidWait = XRT_NET_OK;

		iInvalidWait = xrtNetWaitSourceWait(&tInvalidSrc);
		printf("  Wait source invalid kind rejected : %s\n",
			iInvalidWait == XRT_NET_ERROR ? "PASS" : "FAIL");
	}

	{
		xnetfuture* pFuture = xrtNetFutureCreate();
		xnetwaitsrc tSrc = xrtNetWaitSourceNone();
		xnet_result iTimeoutStatus = XRT_NET_ERROR;
		ptr pTimeoutValue = (ptr)1;

		if ( pFuture ) {
			tSrc = xrtNetWaitSourceFuture(pFuture);
			iTimeoutStatus = xrtNetWaitSourceWaitValueTimeout(&tSrc, 20, &pTimeoutValue);
		}

		printf("  Wait source future sync timeout create : %s\n", pFuture ? "PASS" : "FAIL");
		printf("  Wait source future sync timeout status : %s\n", iTimeoutStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Wait source future sync timeout leaves pending future : %s\n",
			pFuture && xrtNetFutureStatus(pFuture) == XRT_NET_AGAIN ? "PASS" : "FAIL");
		printf("  Wait source future sync timeout leaves null value : %s\n", pTimeoutValue == NULL ? "PASS" : "FAIL");

		if ( pFuture ) xrtNetFutureDestroy(pFuture);
	}

	{
		xnetfuture* pFuture = xrtNetFutureCreate();
		xnetwaitsrc tSrc = xrtNetWaitSourceNone();
		xnet_result iDeadlineStatus = XRT_NET_ERROR;
		int64_t iNowMs = 0;
		ptr pDeadlineValue = (ptr)1;

		if ( pFuture ) {
			tSrc = xrtNetWaitSourceFuture(pFuture);
			iNowMs = __xnetSyncNowMs();
			iDeadlineStatus = xrtNetWaitSourceWaitValueUntil(&tSrc, iNowMs + 20, &pDeadlineValue);
		}

		printf("  Wait source future sync deadline create : %s\n", pFuture ? "PASS" : "FAIL");
		printf("  Wait source future sync deadline status : %s\n", iDeadlineStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Wait source future sync deadline leaves pending future : %s\n",
			pFuture && xrtNetFutureStatus(pFuture) == XRT_NET_AGAIN ? "PASS" : "FAIL");
		printf("  Wait source future sync deadline leaves null value : %s\n", pDeadlineValue == NULL ? "PASS" : "FAIL");

		if ( pFuture ) xrtNetFutureDestroy(pFuture);
	}

	{
		xnetengineconfig tCfg;
		xnetengine* pEngine;
		xnetfuture* pFuture;
		__test_xnet2_sync_task_ctx tCtx;
		int iPayload = 5678;

		memset(&tCtx, 0, sizeof(tCtx));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;

		pEngine = xrtNetEngineCreate(&tCfg);
		pFuture = xrtNetFutureCreate();
		tCtx.pFuture = pFuture;
		tCtx.iResult = XRT_NET_OK;
		tCtx.pValue = &iPayload;
		printf("  Sync engine create : %s\n", pEngine != NULL ? "PASS" : "FAIL");
		printf("  Sync future create : %s\n", pFuture != NULL ? "PASS" : "FAIL");

		if ( pEngine && pFuture ) {
			printf("  Sync engine start : %s\n", xrtNetEngineStart(pEngine) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  Delayed resolve post : %s\n",
				xrtNetEnginePostDelayed(pEngine, 0, 50, __Test_XNet2_SyncResolveTask, &tCtx) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  Delayed future timeout first : %s\n",
				xrtNetFutureWait(pFuture, 5) == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
			printf("  Delayed future still pending : %s\n",
				xrtNetFutureStatus(pFuture) == XRT_NET_AGAIN ? "PASS" : "FAIL");
			printf("  Delayed future eventual resolve : %s\n",
				xrtNetFutureWait(pFuture, 250) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  Delayed future value : %s\n",
				xrtNetFutureValue(pFuture) == &iPayload ? "PASS" : "FAIL");
			printf("  Delayed resolve hit count : %s\n", tCtx.iHitCount == 1 ? "PASS" : "FAIL");
			printf("  Delayed resolve worker id : %s\n", tCtx.iWorkerId == 0 ? "PASS" : "FAIL");
			xrtNetEngineStop(pEngine);
		}

		if ( pFuture ) xrtNetFutureDestroy(pFuture);
		if ( pEngine ) xrtNetEngineDestroy(pEngine);
	}

	{
		xnetengineconfig tCfg;
		xnetengine* pCustom;
		xnetengine* pHidden1;
		xnetengine* pHidden2;
		xnetfuture* pFuture;
		__test_xnet2_sync_task_ctx tCtx;
		int iPayload = 9012;

		memset(&tCtx, 0, sizeof(tCtx));
		xrtNetSyncShutdownHiddenEngine();
		pHidden1 = xrtNetSyncGetHiddenEngine();
		pHidden2 = xrtNetSyncGetHiddenEngine();

		printf("  Hidden engine create : %s\n", pHidden1 != NULL ? "PASS" : "FAIL");
		printf("  Hidden engine running : %s\n", pHidden1 && pHidden1->bRunning ? "PASS" : "FAIL");
		printf("  Hidden engine singleton : %s\n", pHidden1 && pHidden1 == pHidden2 ? "PASS" : "FAIL");
		printf("  Hidden engine one worker policy : %s\n",
			pHidden1 && xrtNetEngineGetWorkerCount(pHidden1) == 1 ? "PASS" : "FAIL");
		printf("  Hidden engine resolver returns hidden : %s\n",
			__xnetSyncResolveEngine(NULL) == pHidden1 ? "PASS" : "FAIL");

		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 2;
		pCustom = xrtNetEngineCreate(&tCfg);
		if ( pCustom ) {
			(void)xrtNetEngineStart(pCustom);
		}
		printf("  Custom engine resolver preserves explicit engine : %s\n",
			pCustom && __xnetSyncResolveEngine(pCustom) == pCustom ? "PASS" : "FAIL");

		pFuture = xrtNetFutureCreate();
		tCtx.pFuture = pFuture;
		tCtx.iResult = XRT_NET_OK;
		tCtx.pValue = &iPayload;
		printf("  Hidden engine post resolve : %s\n",
			pHidden1 && pFuture &&
			xrtNetEnginePost(pHidden1, 0, __Test_XNet2_SyncResolveTask, &tCtx) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Hidden engine future wait : %s\n",
			pFuture && xrtNetFutureWait(pFuture, 250) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Hidden engine future value : %s\n",
			pFuture && xrtNetFutureValue(pFuture) == &iPayload ? "PASS" : "FAIL");

		if ( pFuture ) xrtNetFutureDestroy(pFuture);
		if ( pCustom ) {
			xrtNetEngineStop(pCustom);
			xrtNetEngineDestroy(pCustom);
		}
		xrtNetSyncShutdownHiddenEngine();
		__Test_XNet2_SyncSleepMs(10);
		printf("  Hidden engine shutdown and recreate : %s\n",
			xrtNetSyncGetHiddenEngine() != NULL ? "PASS" : "FAIL");
		xrtNetSyncShutdownHiddenEngine();
	}

	{
		xnetengineconfig tCfg;
		xnetdgramconfig tServerCfg;
		xnetdgramconfig tClientCfg;
		xnetengine* pEngine = NULL;
		xdgramsock* pServer = NULL;
		xdgramsock* pClient = NULL;
		xnetaddr tServerAddr;
		xnetdgramevents tEvents;
		__test_xnet2_sync_dgram_event_case tEventCase;
		xnet_result iConflictStatus = XRT_NET_ERROR;
		xnetdgrampkt* pPacket = (xnetdgrampkt*)1;

		memset(&tServerAddr, 0, sizeof(tServerAddr));
		memset(&tEvents, 0, sizeof(tEvents));
		memset(&tEventCase, 0, sizeof(tEventCase));
		tEvents.OnRecv = __Test_XNet2_SyncDgramOnRecv;
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		xrtNetDgramConfigInit(&tServerCfg);
		xrtNetDgramConfigInit(&tClientCfg);
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pServer = pEngine ? xrtNetDgramCreate(pEngine, &tServerCfg, &tEvents, &tEventCase) : NULL;
		pClient = pEngine ? xrtNetDgramCreate(pEngine, &tClientCfg, NULL, NULL) : NULL;
		if ( pServer ) (void)xrtNetDgramStart(pServer);
		if ( pClient ) (void)xrtNetDgramStart(pClient);
		if ( pServer ) {
			(void)xrtNetAddrParse(&tServerAddr, "127.0.0.1", pServer->tLocalAddr.iPort);
			iConflictStatus = xrtNetDgramRecvTimeout(pServer, 120, &pPacket);
		}
		if ( pClient && pServer ) {
			(void)xrtNetDgramSendTo(pClient, &tServerAddr, "cbok", 4);
		}
		for ( int i = 0; i < 40 && tEventCase.iHitCount == 0; ++i ) {
			__Test_XNet2_SyncSleepMs(10);
		}

		printf("  Dgram recv rejects OnRecv conflict create : %s\n", pEngine && pServer && pClient ? "PASS" : "FAIL");
		printf("  Dgram recv rejects OnRecv conflict status : %s\n", iConflictStatus == XRT_NET_ERROR ? "PASS" : "FAIL");
		printf("  Dgram recv rejects OnRecv conflict leaves null packet : %s\n", pPacket == NULL ? "PASS" : "FAIL");
		printf("  Dgram OnRecv callback still fires : %s\n", tEventCase.iHitCount > 0 ? "PASS" : "FAIL");
		printf("  Dgram OnRecv callback payload : %s\n",
			__Test_XNet2_SyncDgramEventTextEquals(&tEventCase, "cbok") ? "PASS" : "FAIL");
		printf("  Dgram OnRecv callback source port : %s\n",
			tEventCase.iFromPort == (long)pClient->tLocalAddr.iPort ? "PASS" : "FAIL");

		if ( pClient ) {
			xrtNetDgramStop(pClient);
			xrtNetDgramDestroy(pClient);
		}
		if ( pServer ) {
			xrtNetDgramStop(pServer);
			xrtNetDgramDestroy(pServer);
		}
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetdgramconfig tServerCfg;
		xnetdgramconfig tClientCfg;
		xnetengine* pEngine = NULL;
		xdgramsock* pServer = NULL;
		xdgramsock* pClient = NULL;
		xnetaddr tServerAddr;
		xnet_result iRecvStatus = XRT_NET_ERROR;
		xnetdgrampkt* pPacket = NULL;

		memset(&tServerAddr, 0, sizeof(tServerAddr));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		xrtNetDgramConfigInit(&tServerCfg);
		xrtNetDgramConfigInit(&tClientCfg);
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pServer = pEngine ? xrtNetDgramCreate(pEngine, &tServerCfg, NULL, NULL) : NULL;
		pClient = pEngine ? xrtNetDgramCreate(pEngine, &tClientCfg, NULL, NULL) : NULL;
		if ( pServer ) (void)xrtNetDgramStart(pServer);
		if ( pClient ) (void)xrtNetDgramStart(pClient);
		if ( pServer ) (void)xrtNetAddrParse(&tServerAddr, "127.0.0.1", pServer->tLocalAddr.iPort);
		if ( pClient && pServer ) {
			(void)xrtNetDgramSendTo(pClient, &tServerAddr, "dgok", 4);
			iRecvStatus = xrtNetDgramRecv(pServer, &pPacket);
		}

		printf("  Dgram recv helper create : %s\n", pEngine && pServer && pClient ? "PASS" : "FAIL");
		printf("  Dgram recv helper status : %s\n", iRecvStatus == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Dgram recv helper packet created : %s\n", pPacket != NULL ? "PASS" : "FAIL");
		printf("  Dgram recv helper packet source port : %s\n",
			pPacket && xrtNetDgramPacketFrom(pPacket) && xrtNetDgramPacketFrom(pPacket)->iPort == pClient->tLocalAddr.iPort ? "PASS" : "FAIL");
		printf("  Dgram recv helper payload : %s\n",
			__Test_XNet2_SyncDgramPacketTextEquals(pPacket, "dgok") ? "PASS" : "FAIL");

		if ( pPacket ) xrtNetDgramPacketDestroy(pPacket);
		if ( pClient ) {
			xrtNetDgramStop(pClient);
			xrtNetDgramDestroy(pClient);
		}
		if ( pServer ) {
			xrtNetDgramStop(pServer);
			xrtNetDgramDestroy(pServer);
		}
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetdgramconfig tServerCfg;
		xnetdgramconfig tClientCfg;
		xnetengine* pEngine = NULL;
		xdgramsock* pServer = NULL;
		xdgramsock* pClient = NULL;
		xnetaddr tServerAddr;
		xnetwaitsrc tSrc = xrtNetWaitSourceNone();
		xnet_result iTimeoutStatus = XRT_NET_ERROR;
		xnet_result iRetryStatus = XRT_NET_ERROR;
		ptr pTimeoutValue = (ptr)1;
		ptr pRetryValue = NULL;
		xnetdgrampkt* pRetryPacket = NULL;

		memset(&tServerAddr, 0, sizeof(tServerAddr));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		xrtNetDgramConfigInit(&tServerCfg);
		xrtNetDgramConfigInit(&tClientCfg);
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pServer = pEngine ? xrtNetDgramCreate(pEngine, &tServerCfg, NULL, NULL) : NULL;
		pClient = pEngine ? xrtNetDgramCreate(pEngine, &tClientCfg, NULL, NULL) : NULL;
		if ( pServer ) (void)xrtNetDgramStart(pServer);
		if ( pClient ) (void)xrtNetDgramStart(pClient);
		if ( pServer ) {
			(void)xrtNetAddrParse(&tServerAddr, "127.0.0.1", pServer->tLocalAddr.iPort);
			tSrc = xrtNetWaitSourceDgramRecv(pServer);
			iTimeoutStatus = xrtNetWaitSourceWaitValueTimeout(&tSrc, 30, &pTimeoutValue);
		}
		if ( pClient && pServer ) {
			(void)xrtNetDgramSendTo(pClient, &tServerAddr, "retry", 5);
			iRetryStatus = xrtNetWaitSourceWaitValue(&tSrc, &pRetryValue);
			pRetryPacket = (xnetdgrampkt*)pRetryValue;
		}

		printf("  Wait source dgram timeout helper create : %s\n", pEngine && pServer && pClient ? "PASS" : "FAIL");
		printf("  Wait source dgram timeout helper status : %s\n", iTimeoutStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Wait source dgram timeout helper leaves null value : %s\n", pTimeoutValue == NULL ? "PASS" : "FAIL");
		printf("  Wait source dgram timeout helper unregisters waiter : %s\n", iRetryStatus == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Wait source dgram retry payload : %s\n",
			__Test_XNet2_SyncDgramPacketTextEquals(pRetryPacket, "retry") ? "PASS" : "FAIL");

		if ( pRetryPacket ) xrtNetDgramPacketDestroy(pRetryPacket);
		if ( pClient ) {
			xrtNetDgramStop(pClient);
			xrtNetDgramDestroy(pClient);
		}
		if ( pServer ) {
			xrtNetDgramStop(pServer);
			xrtNetDgramDestroy(pServer);
		}
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetdgramconfig tServerCfg;
		xnetdgramconfig tClientCfg;
		xnetengine* pEngine = NULL;
		xdgramsock* pServer = NULL;
		xdgramsock* pClient = NULL;
		xnetaddr tServerAddr;
		xnetfuture* pFuture = NULL;
		xnet_result iWaitStatus = XRT_NET_ERROR;
		xnetdgrampkt* pPacket = NULL;
		bool bDestroyRejected = false;

		memset(&tServerAddr, 0, sizeof(tServerAddr));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		xrtNetDgramConfigInit(&tServerCfg);
		xrtNetDgramConfigInit(&tClientCfg);
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pServer = pEngine ? xrtNetDgramCreate(pEngine, &tServerCfg, NULL, NULL) : NULL;
		pClient = pEngine ? xrtNetDgramCreate(pEngine, &tClientCfg, NULL, NULL) : NULL;
		if ( pServer ) (void)xrtNetDgramStart(pServer);
		if ( pClient ) (void)xrtNetDgramStart(pClient);
		if ( pServer ) (void)xrtNetAddrParse(&tServerAddr, "127.0.0.1", pServer->tLocalAddr.iPort);
		if ( pServer ) {
			pFuture = xrtNetDgramRecvFuture(pServer);
		}

		if ( pServer ) {
			xrtNetDgramDestroy(pServer);
			bDestroyRejected = pFuture && xrtNetFutureStatus(pFuture) == XRT_NET_AGAIN && pServer->bRunning;
		}
		if ( pClient && pServer ) {
			(void)xrtNetDgramSendTo(pClient, &tServerAddr, "hold", 4);
		}
		if ( pFuture ) {
			iWaitStatus = xrtNetFutureWait(pFuture, 300);
			pPacket = (xnetdgrampkt*)xrtNetFutureValue(pFuture);
		}

		printf("  Dgram recv future create : %s\n", pEngine && pServer && pClient && pFuture ? "PASS" : "FAIL");
		printf("  Dgram recv future rejects early destroy : %s\n",
			bDestroyRejected ? "PASS" : "FAIL");
		printf("  Dgram recv future wait status : %s\n", iWaitStatus == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Dgram recv future packet survives rejected destroy : %s\n",
			__Test_XNet2_SyncDgramPacketTextEquals(pPacket, "hold") ? "PASS" : "FAIL");

		if ( pPacket ) xrtNetDgramPacketDestroy(pPacket);
		if ( pFuture ) xrtNetFutureDestroy(pFuture);
		if ( pClient ) {
			xrtNetDgramStop(pClient);
			xrtNetDgramDestroy(pClient);
		}
		if ( pServer ) {
			xrtNetDgramStop(pServer);
			xrtNetDgramDestroy(pServer);
		}
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetdgramconfig tServerCfg;
		xnetdgramconfig tClientCfg;
		xnetengine* pEngine = NULL;
		xdgramsock* pServer = NULL;
		xdgramsock* pClient = NULL;
		xnetaddr tServerAddr;
		xnetfuture* pFirstFuture = NULL;
		xnetfuture* pSecondFuture = NULL;
		xnet_result iSecondStatus = XRT_NET_ERROR;
		xnet_result iFirstStatus = XRT_NET_ERROR;
		xnetdgrampkt* pFirstPacket = NULL;
		xnetdgrampkt* pSecondPacket = NULL;

		memset(&tServerAddr, 0, sizeof(tServerAddr));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		xrtNetDgramConfigInit(&tServerCfg);
		xrtNetDgramConfigInit(&tClientCfg);
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pServer = pEngine ? xrtNetDgramCreate(pEngine, &tServerCfg, NULL, NULL) : NULL;
		pClient = pEngine ? xrtNetDgramCreate(pEngine, &tClientCfg, NULL, NULL) : NULL;
		if ( pServer ) (void)xrtNetDgramStart(pServer);
		if ( pClient ) (void)xrtNetDgramStart(pClient);
		if ( pServer ) (void)xrtNetAddrParse(&tServerAddr, "127.0.0.1", pServer->tLocalAddr.iPort);
		if ( pServer ) {
			pFirstFuture = xrtNetDgramRecvFuture(pServer);
			pSecondFuture = xrtNetDgramRecvFuture(pServer);
		}
		if ( pSecondFuture ) {
			iSecondStatus = xrtNetFutureWait(pSecondFuture, 200);
			pSecondPacket = (xnetdgrampkt*)xrtNetFutureValue(pSecondFuture);
		}
		if ( pClient && pServer ) {
			(void)xrtNetDgramSendTo(pClient, &tServerAddr, "once", 4);
		}
		if ( pFirstFuture ) {
			iFirstStatus = xrtNetFutureWait(pFirstFuture, 300);
			pFirstPacket = (xnetdgrampkt*)xrtNetFutureValue(pFirstFuture);
		}

		printf("  Dgram recv rejects second waiter create : %s\n", pEngine && pServer && pClient && pFirstFuture && pSecondFuture ? "PASS" : "FAIL");
		printf("  Dgram recv rejects second waiter status : %s\n", iSecondStatus == XRT_NET_ERROR ? "PASS" : "FAIL");
		printf("  Dgram recv rejects second waiter packet : %s\n", pSecondPacket == NULL ? "PASS" : "FAIL");
		printf("  Dgram recv first waiter still succeeds : %s\n", iFirstStatus == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Dgram recv first waiter payload : %s\n",
			__Test_XNet2_SyncDgramPacketTextEquals(pFirstPacket, "once") ? "PASS" : "FAIL");

		if ( pFirstPacket ) xrtNetDgramPacketDestroy(pFirstPacket);
		if ( pSecondFuture ) xrtNetFutureDestroy(pSecondFuture);
		if ( pFirstFuture ) xrtNetFutureDestroy(pFirstFuture);
		if ( pClient ) {
			xrtNetDgramStop(pClient);
			xrtNetDgramDestroy(pClient);
		}
		if ( pServer ) {
			xrtNetDgramStop(pServer);
			xrtNetDgramDestroy(pServer);
		}
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetdgramconfig tServerCfg;
		xnetdgramconfig tClientCfg;
		xnetengine* pEngine = NULL;
		xdgramsock* pServer = NULL;
		xdgramsock* pClient = NULL;
		xnetaddr tServerAddr;
		xnet_result iDeadlineStatus = XRT_NET_ERROR;
		xnet_result iRetryStatus = XRT_NET_ERROR;
		xnetdgrampkt* pTimeoutPacket = (xnetdgrampkt*)1;
		xnetdgrampkt* pRetryPacket = NULL;

		memset(&tServerAddr, 0, sizeof(tServerAddr));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		xrtNetDgramConfigInit(&tServerCfg);
		xrtNetDgramConfigInit(&tClientCfg);
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pServer = pEngine ? xrtNetDgramCreate(pEngine, &tServerCfg, NULL, NULL) : NULL;
		pClient = pEngine ? xrtNetDgramCreate(pEngine, &tClientCfg, NULL, NULL) : NULL;
		if ( pServer ) (void)xrtNetDgramStart(pServer);
		if ( pClient ) (void)xrtNetDgramStart(pClient);
		if ( pServer ) {
			(void)xrtNetAddrParse(&tServerAddr, "127.0.0.1", pServer->tLocalAddr.iPort);
			iDeadlineStatus = xrtNetDgramRecvUntil(pServer, __Test_XNet2_SyncNowMs() + 30, &pTimeoutPacket);
		}
		if ( pClient && pServer ) {
			(void)xrtNetDgramSendTo(pClient, &tServerAddr, "dgun", 4);
			iRetryStatus = xrtNetDgramRecvUntil(pServer, __Test_XNet2_SyncNowMs() + 300, &pRetryPacket);
		}

		printf("  Dgram recv deadline helper create : %s\n", pEngine && pServer && pClient ? "PASS" : "FAIL");
		printf("  Dgram recv deadline helper status : %s\n", iDeadlineStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Dgram recv deadline helper leaves null packet : %s\n", pTimeoutPacket == NULL ? "PASS" : "FAIL");
		printf("  Dgram recv deadline helper unregisters waiter : %s\n", iRetryStatus == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Dgram recv deadline helper retry payload : %s\n",
			__Test_XNet2_SyncDgramPacketTextEquals(pRetryPacket, "dgun") ? "PASS" : "FAIL");

		if ( pRetryPacket ) xrtNetDgramPacketDestroy(pRetryPacket);
		if ( pClient ) {
			xrtNetDgramStop(pClient);
			xrtNetDgramDestroy(pClient);
		}
		if ( pServer ) {
			xrtNetDgramStop(pServer);
			xrtNetDgramDestroy(pServer);
		}
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
	{
		xnetengineconfig tCfg;
		xnetdgramconfig tServerCfg;
		xnetdgramconfig tClientCfg;
		xnetengine* pEngine = NULL;
		xdgramsock* pServer = NULL;
		xdgramsock* pClient = NULL;
		xnetaddr tServerAddr;
		xcosched* pSched = NULL;
		xthread pThread = NULL;
		__test_xnet2_sync_dgram_coro_case tDeadlineCase;
		__test_xnet2_sync_dgram_coro_case tRetryCase;
		__test_xnet2_sync_dgram_send_case tSendCase;

		memset(&tDeadlineCase, 0, sizeof(tDeadlineCase));
		memset(&tRetryCase, 0, sizeof(tRetryCase));
		memset(&tSendCase, 0, sizeof(tSendCase));
		memset(&tServerAddr, 0, sizeof(tServerAddr));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		xrtNetDgramConfigInit(&tServerCfg);
		xrtNetDgramConfigInit(&tClientCfg);
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pServer = pEngine ? xrtNetDgramCreate(pEngine, &tServerCfg, NULL, NULL) : NULL;
		pClient = pEngine ? xrtNetDgramCreate(pEngine, &tClientCfg, NULL, NULL) : NULL;
		if ( pServer ) (void)xrtNetDgramStart(pServer);
		if ( pClient ) (void)xrtNetDgramStart(pClient);
		if ( pServer ) (void)xrtNetAddrParse(&tServerAddr, "127.0.0.1", pServer->tLocalAddr.iPort);
		tDeadlineCase.pSock = pServer;
		tRetryCase.pSock = pServer;
		tSendCase.pSock = pClient;
		tSendCase.tPeerAddr = tServerAddr;
		tSendCase.sPayload = "dcud";
		tSendCase.iDelayMs = 40;
		pSched = xrtCoSchedCreate();
		if ( pSched && pServer ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncDgramRecvCoUntil, &tDeadlineCase, 0);
			xrtCoSchedRun(pSched);
		}
		if ( pSched && pServer && pClient ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncDgramRecvCo, &tRetryCase, 0);
			pThread = xrtThreadCreate(__Test_XNet2_SyncDgramSendWorker, &tSendCase, 0);
			xrtCoSchedRun(pSched);
		}
		if ( pThread ) {
			xrtThreadWait(pThread);
			xrtThreadDestroy(pThread);
		}

		printf("  Dgram coroutine deadline helper create : %s\n", pEngine && pServer && pClient && pSched ? "PASS" : "FAIL");
		printf("  Dgram coroutine deadline helper status : %s\n", tDeadlineCase.iDeadlineStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Dgram coroutine deadline helper leaves null packet : %s\n", tDeadlineCase.pTimeoutPacket == NULL ? "PASS" : "FAIL");
		printf("  Dgram coroutine deadline helper unregisters waiter : %s\n",
			tRetryCase.iWaitStatus == XRT_NET_OK && tRetryCase.bDone ? "PASS" : "FAIL");

		if ( tRetryCase.pPacket ) xrtNetDgramPacketDestroy(tRetryCase.pPacket);
		if ( pSched ) xrtCoSchedDestroy(pSched);
		if ( pClient ) {
			xrtNetDgramStop(pClient);
			xrtNetDgramDestroy(pClient);
		}
		if ( pServer ) {
			xrtNetDgramStop(pServer);
			xrtNetDgramDestroy(pServer);
		}
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		__test_xnet2_sync_coro_case tCase;
		xcosched* pSched = NULL;
		xthread pThread = NULL;

		memset(&tCase, 0, sizeof(tCase));
		tCase.pFuture = xrtNetFutureCreate();
		tCase.iPayload = 24680;
		pSched = xrtCoSchedCreate();
		if ( pSched && tCase.pFuture ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncFutureWaitCo, &tCase, 0);
			pThread = xrtThreadCreate(__Test_XNet2_SyncFutureResolveWorker, &tCase, 0);
			xrtCoSchedRun(pSched);
		}

		if ( pThread ) {
			xrtThreadWait(pThread);
			xrtThreadDestroy(pThread);
		}

		printf("  Future coroutine wait create : %s\n", tCase.pFuture && pSched ? "PASS" : "FAIL");
		printf("  Future coroutine wait status : %s\n", tCase.iWaitStatus == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Future coroutine wait value : %s\n", tCase.pResolvedValue == &tCase.iPayload ? "PASS" : "FAIL");
		printf("  Future coroutine wait completion : %s\n", tCase.bDone ? "PASS" : "FAIL");

		if ( tCase.pFuture ) xrtNetFutureDestroy(tCase.pFuture);
		if ( pSched ) xrtCoSchedDestroy(pSched);
	}

	{
		xnetengineconfig tCfg;
		xnetdgramconfig tServerCfg;
		xnetdgramconfig tClientCfg;
		xnetengine* pEngine = NULL;
		xdgramsock* pServer = NULL;
		xdgramsock* pClient = NULL;
		xnetaddr tServerAddr;
		xcosched* pSched = NULL;
		xthread pThread = NULL;
		__test_xnet2_sync_dgram_coro_case tCase;
		__test_xnet2_sync_dgram_send_case tSendCase;

		memset(&tCase, 0, sizeof(tCase));
		memset(&tSendCase, 0, sizeof(tSendCase));
		memset(&tServerAddr, 0, sizeof(tServerAddr));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		xrtNetDgramConfigInit(&tServerCfg);
		xrtNetDgramConfigInit(&tClientCfg);
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pServer = pEngine ? xrtNetDgramCreate(pEngine, &tServerCfg, NULL, NULL) : NULL;
		pClient = pEngine ? xrtNetDgramCreate(pEngine, &tClientCfg, NULL, NULL) : NULL;
		if ( pServer ) (void)xrtNetDgramStart(pServer);
		if ( pClient ) (void)xrtNetDgramStart(pClient);
		if ( pServer ) (void)xrtNetAddrParse(&tServerAddr, "127.0.0.1", pServer->tLocalAddr.iPort);
		tCase.pSock = pServer;
		tSendCase.pSock = pClient;
		tSendCase.tPeerAddr = tServerAddr;
		tSendCase.sPayload = "codat";
		tSendCase.iDelayMs = 40;
		pSched = xrtCoSchedCreate();
		if ( pSched && pServer && pClient ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncDgramRecvCo, &tCase, 0);
			pThread = xrtThreadCreate(__Test_XNet2_SyncDgramSendWorker, &tSendCase, 0);
			xrtCoSchedRun(pSched);
		}
		if ( pThread ) {
			xrtThreadWait(pThread);
			xrtThreadDestroy(pThread);
		}

		printf("  Dgram coroutine recv helper create : %s\n", pEngine && pServer && pClient && pSched ? "PASS" : "FAIL");
		printf("  Dgram coroutine recv helper status : %s\n", tCase.iWaitStatus == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Dgram coroutine recv helper completion : %s\n", tCase.bDone ? "PASS" : "FAIL");
		printf("  Dgram coroutine recv helper payload : %s\n",
			__Test_XNet2_SyncDgramPacketTextEquals(tCase.pPacket, "codat") ? "PASS" : "FAIL");
		printf("  Dgram coroutine recv helper source port : %s\n",
			tCase.pPacket && xrtNetDgramPacketFrom(tCase.pPacket) &&
			xrtNetDgramPacketFrom(tCase.pPacket)->iPort == pClient->tLocalAddr.iPort ? "PASS" : "FAIL");
		printf("  Dgram coroutine recv helper sender ran : %s\n",
			tSendCase.bDone && tSendCase.iSendStatus == XRT_NET_OK ? "PASS" : "FAIL");

		if ( tCase.pPacket ) xrtNetDgramPacketDestroy(tCase.pPacket);
		if ( pSched ) xrtCoSchedDestroy(pSched);
		if ( pClient ) {
			xrtNetDgramStop(pClient);
			xrtNetDgramDestroy(pClient);
		}
		if ( pServer ) {
			xrtNetDgramStop(pServer);
			xrtNetDgramDestroy(pServer);
		}
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetdgramconfig tServerCfg;
		xnetdgramconfig tClientCfg;
		xnetengine* pEngine = NULL;
		xdgramsock* pServer = NULL;
		xdgramsock* pClient = NULL;
		xnetaddr tServerAddr;
		xcosched* pSched = NULL;
		xthread pThread = NULL;
		__test_xnet2_sync_dgram_coro_case tCase;
		__test_xnet2_sync_dgram_send_case tSendCase;

		memset(&tCase, 0, sizeof(tCase));
		memset(&tSendCase, 0, sizeof(tSendCase));
		memset(&tServerAddr, 0, sizeof(tServerAddr));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		xrtNetDgramConfigInit(&tServerCfg);
		xrtNetDgramConfigInit(&tClientCfg);
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pServer = pEngine ? xrtNetDgramCreate(pEngine, &tServerCfg, NULL, NULL) : NULL;
		pClient = pEngine ? xrtNetDgramCreate(pEngine, &tClientCfg, NULL, NULL) : NULL;
		if ( pServer ) (void)xrtNetDgramStart(pServer);
		if ( pClient ) (void)xrtNetDgramStart(pClient);
		if ( pServer ) (void)xrtNetAddrParse(&tServerAddr, "127.0.0.1", pServer->tLocalAddr.iPort);
		tCase.pSock = pServer;
		tSendCase.pSock = pClient;
		tSendCase.tPeerAddr = tServerAddr;
		tSendCase.sPayload = "wgco";
		tSendCase.iDelayMs = 40;
		pSched = xrtCoSchedCreate();
		if ( pSched && pServer && pClient ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncWaitSourceDgramRecvCo, &tCase, 0);
			pThread = xrtThreadCreate(__Test_XNet2_SyncDgramSendWorker, &tSendCase, 0);
			xrtCoSchedRun(pSched);
		}
		if ( pThread ) {
			xrtThreadWait(pThread);
			xrtThreadDestroy(pThread);
		}

		printf("  Wait source dgram coroutine helper create : %s\n", pEngine && pServer && pClient && pSched ? "PASS" : "FAIL");
		printf("  Wait source dgram coroutine helper status : %s\n", tCase.iWaitStatus == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Wait source dgram coroutine helper completion : %s\n", tCase.bDone ? "PASS" : "FAIL");
		printf("  Wait source dgram coroutine helper payload : %s\n",
			__Test_XNet2_SyncDgramPacketTextEquals(tCase.pPacket, "wgco") ? "PASS" : "FAIL");

		if ( tCase.pPacket ) xrtNetDgramPacketDestroy(tCase.pPacket);
		if ( pSched ) xrtCoSchedDestroy(pSched);
		if ( pClient ) {
			xrtNetDgramStop(pClient);
			xrtNetDgramDestroy(pClient);
		}
		if ( pServer ) {
			xrtNetDgramStop(pServer);
			xrtNetDgramDestroy(pServer);
		}
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetdgramconfig tServerCfg;
		xnetdgramconfig tClientCfg;
		xnetengine* pEngine = NULL;
		xdgramsock* pServer = NULL;
		xdgramsock* pClient = NULL;
		xnetaddr tServerAddr;
		xcosched* pSched = NULL;
		xthread pThread = NULL;
		__test_xnet2_sync_dgram_coro_case tTimeoutCase;
		__test_xnet2_sync_dgram_coro_case tRetryCase;
		__test_xnet2_sync_dgram_send_case tSendCase;

		memset(&tTimeoutCase, 0, sizeof(tTimeoutCase));
		memset(&tRetryCase, 0, sizeof(tRetryCase));
		memset(&tSendCase, 0, sizeof(tSendCase));
		memset(&tServerAddr, 0, sizeof(tServerAddr));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		xrtNetDgramConfigInit(&tServerCfg);
		xrtNetDgramConfigInit(&tClientCfg);
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pServer = pEngine ? xrtNetDgramCreate(pEngine, &tServerCfg, NULL, NULL) : NULL;
		pClient = pEngine ? xrtNetDgramCreate(pEngine, &tClientCfg, NULL, NULL) : NULL;
		if ( pServer ) (void)xrtNetDgramStart(pServer);
		if ( pClient ) (void)xrtNetDgramStart(pClient);
		if ( pServer ) (void)xrtNetAddrParse(&tServerAddr, "127.0.0.1", pServer->tLocalAddr.iPort);
		tTimeoutCase.pSock = pServer;
		tRetryCase.pSock = pServer;
		tSendCase.pSock = pClient;
		tSendCase.tPeerAddr = tServerAddr;
		tSendCase.sPayload = "wgto";
		tSendCase.iDelayMs = 40;
		pSched = xrtCoSchedCreate();
		if ( pSched && pServer ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncWaitSourceDgramRecvCoTimeout, &tTimeoutCase, 0);
			xrtCoSchedRun(pSched);
		}
		if ( pSched && pServer && pClient ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncWaitSourceDgramRecvCo, &tRetryCase, 0);
			pThread = xrtThreadCreate(__Test_XNet2_SyncDgramSendWorker, &tSendCase, 0);
			xrtCoSchedRun(pSched);
		}
		if ( pThread ) {
			xrtThreadWait(pThread);
			xrtThreadDestroy(pThread);
		}

		printf("  Wait source dgram coroutine timeout helper create : %s\n", pEngine && pServer && pClient && pSched ? "PASS" : "FAIL");
		printf("  Wait source dgram coroutine timeout helper status : %s\n", tTimeoutCase.iTimeoutStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Wait source dgram coroutine timeout helper leaves null value : %s\n", tTimeoutCase.pTimeoutPacket == NULL ? "PASS" : "FAIL");
		printf("  Wait source dgram coroutine timeout helper unregisters waiter : %s\n",
			tRetryCase.iWaitStatus == XRT_NET_OK && tRetryCase.bDone ? "PASS" : "FAIL");

		if ( tRetryCase.pPacket ) xrtNetDgramPacketDestroy(tRetryCase.pPacket);
		if ( pSched ) xrtCoSchedDestroy(pSched);
		if ( pClient ) {
			xrtNetDgramStop(pClient);
			xrtNetDgramDestroy(pClient);
		}
		if ( pServer ) {
			xrtNetDgramStop(pServer);
			xrtNetDgramDestroy(pServer);
		}
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		__test_xnet2_sync_coro_case tCase;
		xcosched* pSched = NULL;
		xthread pThread = NULL;

		memset(&tCase, 0, sizeof(tCase));
		tCase.pFuture = xrtNetFutureCreate();
		tCase.iPayload = 13524;
		pSched = xrtCoSchedCreate();
		if ( pSched && tCase.pFuture ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncWaitSourceFutureCo, &tCase, 0);
			pThread = xrtThreadCreate(__Test_XNet2_SyncFutureResolveWorker, &tCase, 0);
			xrtCoSchedRun(pSched);
		}

		if ( pThread ) {
			xrtThreadWait(pThread);
			xrtThreadDestroy(pThread);
		}

		printf("  Wait source future coroutine create : %s\n", tCase.pFuture && pSched ? "PASS" : "FAIL");
		printf("  Wait source future coroutine status : %s\n", tCase.iWaitStatus == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Wait source future coroutine value : %s\n", tCase.pResolvedValue == &tCase.iPayload ? "PASS" : "FAIL");
		printf("  Wait source future coroutine completion : %s\n", tCase.bDone ? "PASS" : "FAIL");

		if ( tCase.pFuture ) xrtNetFutureDestroy(tCase.pFuture);
		if ( pSched ) xrtCoSchedDestroy(pSched);
	}

	{
		__test_xnet2_sync_coro_case tCase;
		xcosched* pSched = NULL;

		memset(&tCase, 0, sizeof(tCase));
		tCase.pFuture = xrtNetFutureCreate();
		pSched = xrtCoSchedCreate();
		if ( pSched && tCase.pFuture ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncFutureWaitCoTimeout, &tCase, 0);
			xrtCoSchedRun(pSched);
		}

		printf("  Future coroutine timeout create : %s\n", tCase.pFuture && pSched ? "PASS" : "FAIL");
		printf("  Future coroutine timeout status : %s\n", tCase.iTimeoutStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Future coroutine timeout completion : %s\n", tCase.bTimeoutDone ? "PASS" : "FAIL");
		printf("  Future coroutine timeout leaves pending future : %s\n",
			tCase.pFuture && xrtNetFutureStatus(tCase.pFuture) == XRT_NET_AGAIN ? "PASS" : "FAIL");
		printf("  Future coroutine timeout leaves null value : %s\n", tCase.pTimeoutValue == NULL ? "PASS" : "FAIL");

		if ( tCase.pFuture ) xrtNetFutureDestroy(tCase.pFuture);
		if ( pSched ) xrtCoSchedDestroy(pSched);
	}

	{
		__test_xnet2_sync_coro_case tCase;
		xcosched* pSched = NULL;

		memset(&tCase, 0, sizeof(tCase));
		tCase.pFuture = xrtNetFutureCreate();
		pSched = xrtCoSchedCreate();
		if ( pSched && tCase.pFuture ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncWaitSourceFutureCoTimeout, &tCase, 0);
			xrtCoSchedRun(pSched);
		}

		printf("  Wait source future coroutine timeout create : %s\n", tCase.pFuture && pSched ? "PASS" : "FAIL");
		printf("  Wait source future coroutine timeout status : %s\n", tCase.iTimeoutStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Wait source future coroutine timeout completion : %s\n", tCase.bTimeoutDone ? "PASS" : "FAIL");
		printf("  Wait source future coroutine timeout leaves pending future : %s\n",
			tCase.pFuture && xrtNetFutureStatus(tCase.pFuture) == XRT_NET_AGAIN ? "PASS" : "FAIL");
		printf("  Wait source future coroutine timeout leaves null value : %s\n", tCase.pTimeoutValue == NULL ? "PASS" : "FAIL");

		if ( tCase.pFuture ) xrtNetFutureDestroy(tCase.pFuture);
		if ( pSched ) xrtCoSchedDestroy(pSched);
	}

	{
		__test_xnet2_sync_coro_case tCase;
		xcosched* pSched = NULL;

		memset(&tCase, 0, sizeof(tCase));
		tCase.pFuture = xrtNetFutureCreate();
		pSched = xrtCoSchedCreate();
		if ( pSched && tCase.pFuture ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncFutureWaitCoUntil, &tCase, 0);
			xrtCoSchedRun(pSched);
		}

		printf("  Future coroutine deadline create : %s\n", tCase.pFuture && pSched ? "PASS" : "FAIL");
		printf("  Future coroutine deadline status : %s\n", tCase.iDeadlineStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Future coroutine deadline completion : %s\n", tCase.bDeadlineDone ? "PASS" : "FAIL");
		printf("  Future coroutine deadline leaves pending future : %s\n",
			tCase.pFuture && xrtNetFutureStatus(tCase.pFuture) == XRT_NET_AGAIN ? "PASS" : "FAIL");
		printf("  Future coroutine deadline leaves null value : %s\n", tCase.pDeadlineValue == NULL ? "PASS" : "FAIL");

		if ( tCase.pFuture ) xrtNetFutureDestroy(tCase.pFuture);
		if ( pSched ) xrtCoSchedDestroy(pSched);
	}

	{
		xnetengineconfig tCfg;
		xnetengine* pEngine = NULL;
		__test_xnet2_sync_postfuture_case tTaskCase;
		__test_xnet2_sync_coro_case tWaitCase;
		xcosched* pSched = NULL;

		memset(&tTaskCase, 0, sizeof(tTaskCase));
		memset(&tWaitCase, 0, sizeof(tWaitCase));
		tTaskCase.iPayload = 13579;
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 2;
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		tWaitCase.pFuture = xrtNetEnginePostFuture(pEngine, 1, __Test_XNet2_SyncPostFutureTask, &tTaskCase);
		pSched = xrtCoSchedCreate();
		if ( pSched && tWaitCase.pFuture ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncFutureWaitCo, &tWaitCase, 0);
			xrtCoSchedRun(pSched);
		}

		printf("  Post future create : %s\n", pEngine && tWaitCase.pFuture && pSched ? "PASS" : "FAIL");
		printf("  Post future wait status : %s\n", tWaitCase.iWaitStatus == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Post future wait value : %s\n", tWaitCase.pResolvedValue == &tTaskCase.iPayload ? "PASS" : "FAIL");
		printf("  Post future completion : %s\n", tWaitCase.bDone ? "PASS" : "FAIL");
		printf("  Post future task hit count : %s\n", tTaskCase.iHitCount == 1 ? "PASS" : "FAIL");
		printf("  Post future worker id valid : %s\n", tTaskCase.iWorkerId >= 0 ? "PASS" : "FAIL");

		if ( tWaitCase.pFuture ) xrtNetFutureDestroy(tWaitCase.pFuture);
		if ( pSched ) xrtCoSchedDestroy(pSched);
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		__test_xnet2_sync_postfuture_case tTaskCase;
		__test_xnet2_sync_coro_case tWaitCase;
		xcosched* pSched = NULL;

		memset(&tTaskCase, 0, sizeof(tTaskCase));
		memset(&tWaitCase, 0, sizeof(tWaitCase));
		tTaskCase.iPayload = 97531;
		tWaitCase.pFuture = xrtNetEnginePostDelayedFuture(NULL, 0, 40, __Test_XNet2_SyncPostFutureTask, &tTaskCase);
		pSched = xrtCoSchedCreate();
		if ( pSched && tWaitCase.pFuture ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncFutureWaitCo, &tWaitCase, 0);
			xrtCoSchedRun(pSched);
		}

		printf("  Delayed post future create : %s\n", tWaitCase.pFuture && pSched ? "PASS" : "FAIL");
		printf("  Delayed post future wait status : %s\n", tWaitCase.iWaitStatus == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Delayed post future wait value : %s\n", tWaitCase.pResolvedValue == &tTaskCase.iPayload ? "PASS" : "FAIL");
		printf("  Delayed post future completion : %s\n", tWaitCase.bDone ? "PASS" : "FAIL");
		printf("  Delayed post future task hit count : %s\n", tTaskCase.iHitCount == 1 ? "PASS" : "FAIL");

		if ( tWaitCase.pFuture ) xrtNetFutureDestroy(tWaitCase.pFuture);
		if ( pSched ) xrtCoSchedDestroy(pSched);
		xrtNetSyncShutdownHiddenEngine();
	}

	{
		__test_xnet2_sync_postfuture_case tTaskCase;
		xnetfuture* pFuture = NULL;
		const char* sEarlyDestroyError = NULL;
		const char* sFinalDestroyError = NULL;

		memset(&tTaskCase, 0, sizeof(tTaskCase));
		tTaskCase.iPayload = 86420;
		pFuture = xrtNetEnginePostDelayedFuture(NULL, 0, 80, __Test_XNet2_SyncPostFutureTask, &tTaskCase);

		xrtClearError();
		if ( pFuture ) {
			xrtNetFutureDestroy(pFuture);
			sEarlyDestroyError = xrtGetError();
		}
		printf("  Posted future early destroy rejected : %s\n",
			pFuture && sEarlyDestroyError && sEarlyDestroyError[0] != 0 ? "PASS" : "FAIL");
		printf("  Posted future survives early destroy : %s\n",
			pFuture && xrtNetFutureStatus(pFuture) == XRT_NET_AGAIN ? "PASS" : "FAIL");
		printf("  Posted future completes after rejected destroy : %s\n",
			pFuture && xrtNetFutureWait(pFuture, 250) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Posted future value after rejected destroy : %s\n",
			pFuture && xrtNetFutureValue(pFuture) == &tTaskCase.iPayload ? "PASS" : "FAIL");
		printf("  Posted future task hit count after rejected destroy : %s\n",
			tTaskCase.iHitCount == 1 ? "PASS" : "FAIL");

		xrtClearError();
		if ( pFuture ) {
			xrtNetFutureDestroy(pFuture);
			pFuture = NULL;
		}
		sFinalDestroyError = xrtGetError();
		printf("  Posted future final destroy succeeds : %s\n",
			sFinalDestroyError && sFinalDestroyError[0] == 0 ? "PASS" : "FAIL");
		xrtNetSyncShutdownHiddenEngine();
	}

	{
		xnetengineconfig tCfg;
		xnetengine* pEngine = NULL;
		xnetstream* pStream = NULL;
		xcosched* pSched = NULL;
		__test_xnet2_sync_coro_case tWaitCase;
		bool bEarlyDestroyRejected = false;
		bool bFinalFutureDestroyOk = false;
		bool bFinalStreamDestroyOk = false;

		memset(&tWaitCase, 0, sizeof(tWaitCase));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pStream = pEngine ? xrtNetStreamCreate(pEngine, NULL, NULL) : NULL;
		if ( pStream ) {
			(void)xrtNetStreamSend(pStream, "drain", 5);
			tWaitCase.pFuture = xrtNetStreamDrainFuture(pStream);
		}

		xrtClearError();
		if ( pStream ) {
			xrtNetStreamDestroy(pStream);
			bEarlyDestroyRejected = xrtGetError() && xrtGetError()[0] != 0;
		}

		pSched = xrtCoSchedCreate();
		if ( pEngine && pStream && tWaitCase.pFuture && pSched ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncFutureWaitCo, &tWaitCase, 0);
			(void)xrtNetEnginePostDelayed(pEngine, pStream->pWorker ? pStream->pWorker->iId : 0, 40, __Test_XNet2_SyncStreamDrainTask, pStream);
			xrtCoSchedRun(pSched);
		}

		printf("  Stream drain future create : %s\n", pEngine && pStream && tWaitCase.pFuture && pSched ? "PASS" : "FAIL");
		printf("  Stream drain future rejects early stream destroy : %s\n", bEarlyDestroyRejected ? "PASS" : "FAIL");
		printf("  Stream drain future wait status : %s\n", tWaitCase.iWaitStatus == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Stream drain future value : %s\n", tWaitCase.pResolvedValue == pStream ? "PASS" : "FAIL");
		printf("  Stream drain future completion : %s\n", tWaitCase.bDone ? "PASS" : "FAIL");
		printf("  Stream drain future empties send queue : %s\n", pStream && xrtNetStreamPendingSend(pStream) == 0 ? "PASS" : "FAIL");

		xrtClearError();
		if ( tWaitCase.pFuture ) {
			xrtNetFutureDestroy(tWaitCase.pFuture);
			tWaitCase.pFuture = NULL;
		}
		bFinalFutureDestroyOk = xrtGetError() && xrtGetError()[0] == 0;
		xrtClearError();
		if ( pStream ) {
			xrtNetStreamDestroy(pStream);
			pStream = NULL;
		}
		bFinalStreamDestroyOk = xrtGetError() && xrtGetError()[0] == 0;
		printf("  Stream drain future final future destroy succeeds : %s\n", bFinalFutureDestroyOk ? "PASS" : "FAIL");
		printf("  Stream drain future final stream destroy succeeds : %s\n", bFinalStreamDestroyOk ? "PASS" : "FAIL");

		if ( pSched ) xrtCoSchedDestroy(pSched);
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetengine* pEngine = NULL;
		xnetstream* pStream = NULL;
		xcosched* pSched = NULL;
		__test_xnet2_sync_coro_case tWaitCase;
		__test_xnet2_sync_coro_case tRetryCase;

		memset(&tWaitCase, 0, sizeof(tWaitCase));
		memset(&tRetryCase, 0, sizeof(tRetryCase));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pStream = pEngine ? xrtNetStreamCreate(pEngine, NULL, NULL) : NULL;
		if ( pStream ) {
			(void)xrtNetStreamSend(pStream, "drain", 5);
			tWaitCase.pStream = pStream;
			tRetryCase.pStream = pStream;
		}
		pSched = xrtCoSchedCreate();
		if ( pEngine && pStream && pSched ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncStreamDrainWaitCoTimeout, &tWaitCase, 0);
			xrtCoSchedRun(pSched);
		}
		if ( pEngine && pStream && pSched ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncStreamDrainWaitCo, &tRetryCase, 0);
			(void)xrtNetEnginePostDelayed(
				pEngine,
				pStream->pWorker ? pStream->pWorker->iId : 0,
				40,
				__Test_XNet2_SyncStreamDrainTask,
				pStream);
			xrtCoSchedRun(pSched);
		}

		printf("  Stream drain coroutine timeout helper create : %s\n", pEngine && pStream && pSched ? "PASS" : "FAIL");
		printf("  Stream drain coroutine timeout helper status : %s\n", tWaitCase.iTimeoutStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Stream drain coroutine timeout helper completion : %s\n", tWaitCase.bTimeoutDone ? "PASS" : "FAIL");
		printf("  Stream drain coroutine timeout helper unregisters waiter : %s\n",
			tRetryCase.iWaitStatus == XRT_NET_OK && tRetryCase.bDone ? "PASS" : "FAIL");

		if ( pSched ) xrtCoSchedDestroy(pSched);
		if ( pStream ) xrtNetStreamDestroy(pStream);
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetengine* pEngine = NULL;
		xnetstream* pStream = NULL;
		xcosched* pSched = NULL;
		__test_xnet2_sync_coro_case tWaitCase;
		__test_xnet2_sync_coro_case tRetryCase;

		memset(&tWaitCase, 0, sizeof(tWaitCase));
		memset(&tRetryCase, 0, sizeof(tRetryCase));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pStream = pEngine ? xrtNetStreamCreate(pEngine, NULL, NULL) : NULL;
		if ( pStream ) {
			(void)xrtNetStreamSend(pStream, "drain", 5);
			tWaitCase.pStream = pStream;
			tRetryCase.pStream = pStream;
		}
		pSched = xrtCoSchedCreate();
		if ( pEngine && pStream && pSched ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncWaitSourceStreamDrainCoTimeout, &tWaitCase, 0);
			xrtCoSchedRun(pSched);
		}
		if ( pEngine && pStream && pSched ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncWaitSourceStreamDrainCo, &tRetryCase, 0);
			(void)xrtNetEnginePostDelayed(
				pEngine,
				pStream->pWorker ? pStream->pWorker->iId : 0,
				40,
				__Test_XNet2_SyncStreamDrainTask,
				pStream);
			xrtCoSchedRun(pSched);
		}

		printf("  Wait source stream drain timeout helper create : %s\n", pEngine && pStream && pSched ? "PASS" : "FAIL");
		printf("  Wait source stream drain timeout helper status : %s\n", tWaitCase.iTimeoutStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Wait source stream drain timeout helper completion : %s\n", tWaitCase.bTimeoutDone ? "PASS" : "FAIL");
		printf("  Wait source stream drain timeout helper unregisters waiter : %s\n",
			tRetryCase.iWaitStatus == XRT_NET_OK && tRetryCase.bDone ? "PASS" : "FAIL");

		if ( pSched ) xrtCoSchedDestroy(pSched);
		if ( pStream ) xrtNetStreamDestroy(pStream);
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetengine* pEngine = NULL;
		xnetstream* pStream = NULL;
		xnet_result iWaitStatus = XRT_NET_ERROR;

		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pStream = pEngine ? xrtNetStreamCreate(pEngine, NULL, NULL) : NULL;
		if ( pStream ) {
			xrtNetStreamPauseRead(pStream);
			(void)__xnetStreamPostRecvCopy(pStream, "sread", 5);
			iWaitStatus = xrtNetStreamWaitEx(pStream, XNET_STREAM_WAIT_READABLE);
		}

		printf("  Stream generic sync wait helper create : %s\n", pEngine && pStream ? "PASS" : "FAIL");
		printf("  Stream generic sync wait helper status : %s\n", iWaitStatus == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Stream generic sync wait helper buffers recv bytes : %s\n",
			pStream && xrtNetChainBytes(&pStream->tRxChain) == 5 ? "PASS" : "FAIL");

		if ( pStream ) xrtNetStreamDestroy(pStream);
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetengine* pEngine = NULL;
		xnetstream* pStream = NULL;
		xnet_result iWaitStatus = XRT_NET_ERROR;
		xnetwaitsrc tSrc = xrtNetWaitSourceNone();
		ptr pWaitValue = NULL;

		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pStream = pEngine ? xrtNetStreamCreate(pEngine, NULL, NULL) : NULL;
		if ( pStream ) {
			xrtNetStreamPauseRead(pStream);
			(void)__xnetStreamPostRecvCopy(pStream, "wsrc", 4);
			tSrc = xrtNetWaitSourceStream(pStream, XNET_STREAM_WAIT_READABLE);
			iWaitStatus = xrtNetWaitSourceWaitValue(&tSrc, &pWaitValue);
		}

		printf("  Wait source stream sync wait create : %s\n", pEngine && pStream ? "PASS" : "FAIL");
		printf("  Wait source stream sync wait status : %s\n", iWaitStatus == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Wait source stream sync wait buffers recv bytes : %s\n",
			pStream && xrtNetChainBytes(&pStream->tRxChain) == 4 ? "PASS" : "FAIL");
		printf("  Wait source stream sync wait value matches : %s\n", pWaitValue == pStream ? "PASS" : "FAIL");

		if ( pStream ) xrtNetStreamDestroy(pStream);
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetengine* pEngine = NULL;
		xnetstream* pStream = NULL;
		xnet_result iTimeoutStatus = XRT_NET_ERROR;
		xnet_result iRetryStatus = XRT_NET_ERROR;

		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pStream = pEngine ? xrtNetStreamCreate(pEngine, NULL, NULL) : NULL;
		if ( pStream ) {
			xrtNetStreamPauseRead(pStream);
			iTimeoutStatus = xrtNetStreamWaitTimeoutEx(pStream, XNET_STREAM_WAIT_READABLE, 30);
			(void)__xnetStreamPostRecvCopy(pStream, "retry", 5);
			iRetryStatus = xrtNetStreamWaitEx(pStream, XNET_STREAM_WAIT_READABLE);
		}

		printf("  Stream generic sync timeout helper create : %s\n", pEngine && pStream ? "PASS" : "FAIL");
		printf("  Stream generic sync timeout helper status : %s\n", iTimeoutStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Stream generic sync timeout helper unregisters waiter : %s\n", iRetryStatus == XRT_NET_OK ? "PASS" : "FAIL");

		if ( pStream ) xrtNetStreamDestroy(pStream);
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetengine* pEngine = NULL;
		xnetstream* pStream = NULL;
		xnet_result iTimeoutStatus = XRT_NET_ERROR;
		xnet_result iRetryStatus = XRT_NET_ERROR;
		xnetwaitsrc tSrc = xrtNetWaitSourceNone();
		ptr pTimeoutValue = (ptr)1;

		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pStream = pEngine ? xrtNetStreamCreate(pEngine, NULL, NULL) : NULL;
		if ( pStream ) {
			(void)xrtNetStreamSend(pStream, "drain", 5);
			tSrc = xrtNetWaitSourceStream(pStream, XNET_STREAM_WAIT_DRAIN);
			iTimeoutStatus = xrtNetWaitSourceWaitTimeout(&tSrc, 30);
			(void)xrtNetEnginePostDelayed(
				pEngine,
				pStream->pWorker ? pStream->pWorker->iId : 0,
				40,
				__Test_XNet2_SyncStreamDrainTask,
				pStream);
			iRetryStatus = xrtNetWaitSourceWait(&tSrc);
		}

		printf("  Wait source stream drain sync timeout helper create : %s\n", pEngine && pStream ? "PASS" : "FAIL");
		printf("  Wait source stream drain sync timeout helper status : %s\n", iTimeoutStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Wait source stream drain sync timeout helper unregisters waiter : %s\n", iRetryStatus == XRT_NET_OK ? "PASS" : "FAIL");

		if ( pStream ) xrtNetStreamDestroy(pStream);
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetengine* pEngine = NULL;
		xnetstream* pStream = NULL;
		xnet_result iTimeoutStatus = XRT_NET_ERROR;
		xnet_result iRetryStatus = XRT_NET_ERROR;
		xnetwaitsrc tSrc = xrtNetWaitSourceNone();
		ptr pTimeoutValue = (ptr)1;

		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pStream = pEngine ? xrtNetStreamCreate(pEngine, NULL, NULL) : NULL;
		if ( pStream ) {
			xrtNetStreamPauseRead(pStream);
			tSrc = xrtNetWaitSourceStream(pStream, XNET_STREAM_WAIT_READABLE);
			iTimeoutStatus = xrtNetWaitSourceWaitValueTimeout(&tSrc, 30, &pTimeoutValue);
			(void)__xnetStreamPostRecvCopy(pStream, "wsrt", 4);
			iRetryStatus = xrtNetWaitSourceWait(&tSrc);
		}

		printf("  Wait source stream sync timeout helper create : %s\n", pEngine && pStream ? "PASS" : "FAIL");
		printf("  Wait source stream sync timeout helper status : %s\n", iTimeoutStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Wait source stream sync timeout helper leaves null value : %s\n", pTimeoutValue == NULL ? "PASS" : "FAIL");
		printf("  Wait source stream sync timeout helper unregisters waiter : %s\n", iRetryStatus == XRT_NET_OK ? "PASS" : "FAIL");

		if ( pStream ) xrtNetStreamDestroy(pStream);
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetengine* pEngine = NULL;
		xnetstream* pStream = NULL;
		xcosched* pSched = NULL;
		__test_xnet2_sync_coro_case tWaitCase;

		memset(&tWaitCase, 0, sizeof(tWaitCase));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pStream = pEngine ? xrtNetStreamCreate(pEngine, NULL, NULL) : NULL;
		if ( pStream ) {
			xrtNetStreamPauseRead(pStream);
			tWaitCase.pStream = pStream;
		}
		pSched = xrtCoSchedCreate();
		if ( pEngine && pStream && pSched ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncWaitSourceStreamReadableCo, &tWaitCase, 0);
			(void)__xnetStreamPostRecvCopy(pStream, "wsco", 4);
			xrtCoSchedRun(pSched);
		}

		printf("  Wait source stream coroutine create : %s\n", pEngine && pStream && pSched ? "PASS" : "FAIL");
		printf("  Wait source stream coroutine status : %s\n", tWaitCase.iWaitStatus == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Wait source stream coroutine value matches : %s\n", tWaitCase.pResolvedValue == pStream ? "PASS" : "FAIL");
		printf("  Wait source stream coroutine completion : %s\n", tWaitCase.bDone ? "PASS" : "FAIL");

		if ( pSched ) xrtCoSchedDestroy(pSched);
		if ( pStream ) xrtNetStreamDestroy(pStream);
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetengine* pEngine = NULL;
		xnetstream* pStream = NULL;
		xnet_result iDeadlineStatus = XRT_NET_ERROR;
		xnet_result iRetryStatus = XRT_NET_ERROR;
		xnetwaitsrc tSrc = xrtNetWaitSourceNone();
		int64 iNowMs = 0;

		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pStream = pEngine ? xrtNetStreamCreate(pEngine, NULL, NULL) : NULL;
		if ( pStream ) {
			tSrc = xrtNetWaitSourceStream(pStream, XNET_STREAM_WAIT_CLOSE);
			iNowMs = (int64)(xrtTimer() * 1000.0);
			iDeadlineStatus = xrtNetWaitSourceWaitUntil(&tSrc, iNowMs + 35);
			(void)xrtNetEnginePostDelayed(
				pEngine,
				pStream->pWorker ? pStream->pWorker->iId : 0,
				40,
				__Test_XNet2_SyncStreamCloseTask,
				pStream);
			iRetryStatus = xrtNetWaitSourceWait(&tSrc);
		}

		printf("  Wait source stream close sync deadline helper create : %s\n", pEngine && pStream ? "PASS" : "FAIL");
		printf("  Wait source stream close sync deadline helper status : %s\n", iDeadlineStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Wait source stream close sync deadline helper unregisters waiter : %s\n", iRetryStatus == XRT_NET_CLOSED ? "PASS" : "FAIL");

		if ( pStream ) xrtNetStreamDestroy(pStream);
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetengine* pEngine = NULL;
		xnetstream* pStream = NULL;
		xcosched* pSched = NULL;
		__test_xnet2_sync_coro_case tWaitCase;
		__test_xnet2_sync_coro_case tRetryCase;

		memset(&tWaitCase, 0, sizeof(tWaitCase));
		memset(&tRetryCase, 0, sizeof(tRetryCase));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pStream = pEngine ? xrtNetStreamCreate(pEngine, NULL, NULL) : NULL;
		if ( pStream ) {
			xrtNetStreamPauseRead(pStream);
			tWaitCase.pStream = pStream;
			tRetryCase.pStream = pStream;
		}
		pSched = xrtCoSchedCreate();
		if ( pEngine && pStream && pSched ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncWaitSourceStreamReadableCoTimeout, &tWaitCase, 0);
			xrtCoSchedRun(pSched);
		}
		if ( pEngine && pStream && pSched ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncWaitSourceStreamReadableCo, &tRetryCase, 0);
			(void)__xnetStreamPostRecvCopy(pStream, "wstr", 4);
			xrtCoSchedRun(pSched);
		}

		printf("  Wait source stream coroutine timeout helper create : %s\n", pEngine && pStream && pSched ? "PASS" : "FAIL");
		printf("  Wait source stream coroutine timeout helper status : %s\n", tWaitCase.iTimeoutStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Wait source stream coroutine timeout helper completion : %s\n", tWaitCase.bTimeoutDone ? "PASS" : "FAIL");
		printf("  Wait source stream coroutine timeout helper leaves null value : %s\n", tWaitCase.pTimeoutValue == NULL ? "PASS" : "FAIL");
		printf("  Wait source stream coroutine timeout helper unregisters waiter : %s\n",
			tRetryCase.iWaitStatus == XRT_NET_OK && tRetryCase.bDone ? "PASS" : "FAIL");

		if ( pSched ) xrtCoSchedDestroy(pSched);
		if ( pStream ) xrtNetStreamDestroy(pStream);
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetengine* pEngine = NULL;
		xnetstream* pStream = NULL;
		xcosched* pSched = NULL;
		__test_xnet2_sync_coro_case tWaitCase;
		__test_xnet2_sync_coro_case tRetryCase;

		memset(&tWaitCase, 0, sizeof(tWaitCase));
		memset(&tRetryCase, 0, sizeof(tRetryCase));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pStream = pEngine ? xrtNetStreamCreate(pEngine, NULL, NULL) : NULL;
		if ( pStream ) {
			xrtNetStreamPauseRead(pStream);
			tWaitCase.pStream = pStream;
			tRetryCase.pStream = pStream;
		}
		pSched = xrtCoSchedCreate();
		if ( pEngine && pStream && pSched ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncStreamReadableWaitCoTimeout, &tWaitCase, 0);
			xrtCoSchedRun(pSched);
		}
		if ( pEngine && pStream && pSched ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncStreamReadableWaitCo, &tRetryCase, 0);
			(void)__xnetStreamPostRecvCopy(pStream, "retry", 5);
			xrtCoSchedRun(pSched);
		}

		printf("  Stream readable coroutine timeout helper create : %s\n", pEngine && pStream && pSched ? "PASS" : "FAIL");
		printf("  Stream readable coroutine timeout helper status : %s\n", tWaitCase.iTimeoutStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Stream readable coroutine timeout helper completion : %s\n", tWaitCase.bTimeoutDone ? "PASS" : "FAIL");
		printf("  Stream readable coroutine timeout helper unregisters waiter : %s\n",
			tRetryCase.iWaitStatus == XRT_NET_OK && tRetryCase.bDone ? "PASS" : "FAIL");

		if ( pSched ) xrtCoSchedDestroy(pSched);
		if ( pStream ) xrtNetStreamDestroy(pStream);
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetengine* pEngine = NULL;
		xnetstream* pStream = NULL;
		xcosched* pSched = NULL;
		__test_xnet2_sync_coro_case tWaitCase;
		__test_xnet2_sync_coro_case tRetryCase;

		memset(&tWaitCase, 0, sizeof(tWaitCase));
		memset(&tRetryCase, 0, sizeof(tRetryCase));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pStream = pEngine ? xrtNetStreamCreate(pEngine, NULL, NULL) : NULL;
		if ( pStream ) {
			tWaitCase.pStream = pStream;
			tRetryCase.pStream = pStream;
		}
		pSched = xrtCoSchedCreate();
		if ( pEngine && pStream && pSched ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncStreamCloseWaitCoUntil, &tWaitCase, 0);
			xrtCoSchedRun(pSched);
		}
		if ( pEngine && pStream && pSched ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncStreamCloseWaitCo, &tRetryCase, 0);
			(void)xrtNetEnginePostDelayed(
				pEngine,
				pStream->pWorker ? pStream->pWorker->iId : 0,
				40,
				__Test_XNet2_SyncStreamCloseTask,
				pStream);
			xrtCoSchedRun(pSched);
		}

		printf("  Stream close coroutine deadline helper create : %s\n", pEngine && pStream && pSched ? "PASS" : "FAIL");
		printf("  Stream close coroutine deadline helper status : %s\n", tWaitCase.iDeadlineStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Stream close coroutine deadline helper completion : %s\n", tWaitCase.bDeadlineDone ? "PASS" : "FAIL");
		printf("  Stream close coroutine deadline helper unregisters waiter : %s\n",
			tRetryCase.iWaitStatus == XRT_NET_CLOSED && tRetryCase.bDone ? "PASS" : "FAIL");

		if ( pSched ) xrtCoSchedDestroy(pSched);
		if ( pStream ) xrtNetStreamDestroy(pStream);
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetengine* pEngine = NULL;
		xnetstream* pStream = NULL;
		xcosched* pSched = NULL;
		__test_xnet2_sync_coro_case tWaitCase;
		__test_xnet2_sync_coro_case tRetryCase;

		memset(&tWaitCase, 0, sizeof(tWaitCase));
		memset(&tRetryCase, 0, sizeof(tRetryCase));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pStream = pEngine ? xrtNetStreamCreate(pEngine, NULL, NULL) : NULL;
		if ( pStream ) {
			tWaitCase.pStream = pStream;
			tRetryCase.pStream = pStream;
		}
		pSched = xrtCoSchedCreate();
		if ( pEngine && pStream && pSched ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncWaitSourceStreamCloseCoUntil, &tWaitCase, 0);
			xrtCoSchedRun(pSched);
		}
		if ( pEngine && pStream && pSched ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncWaitSourceStreamCloseCo, &tRetryCase, 0);
			(void)xrtNetEnginePostDelayed(
				pEngine,
				pStream->pWorker ? pStream->pWorker->iId : 0,
				40,
				__Test_XNet2_SyncStreamCloseTask,
				pStream);
			xrtCoSchedRun(pSched);
		}

		printf("  Wait source stream close deadline helper create : %s\n", pEngine && pStream && pSched ? "PASS" : "FAIL");
		printf("  Wait source stream close deadline helper status : %s\n", tWaitCase.iDeadlineStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Wait source stream close deadline helper completion : %s\n", tWaitCase.bDeadlineDone ? "PASS" : "FAIL");
		printf("  Wait source stream close deadline helper unregisters waiter : %s\n",
			tRetryCase.iWaitStatus == XRT_NET_CLOSED && tRetryCase.bDone ? "PASS" : "FAIL");

		if ( pSched ) xrtCoSchedDestroy(pSched);
		if ( pStream ) xrtNetStreamDestroy(pStream);
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetengine* pEngine = NULL;
		xnetstream* pStream = NULL;
		xcosched* pSched = NULL;
		__test_xnet2_sync_coro_case tWaitCase;
		bool bFinalFutureDestroyOk = false;
		bool bFinalStreamDestroyOk = false;

		memset(&tWaitCase, 0, sizeof(tWaitCase));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pStream = pEngine ? xrtNetStreamCreate(pEngine, NULL, NULL) : NULL;
		tWaitCase.pFuture = pStream ? xrtNetStreamCloseFuture(pStream) : NULL;
		pSched = xrtCoSchedCreate();
		if ( pEngine && pStream && tWaitCase.pFuture && pSched ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncFutureWaitCo, &tWaitCase, 0);
			(void)xrtNetEnginePostDelayed(pEngine, pStream->pWorker ? pStream->pWorker->iId : 0, 40, __Test_XNet2_SyncStreamCloseTask, pStream);
			xrtCoSchedRun(pSched);
		}

		printf("  Stream close future create : %s\n", pEngine && pStream && tWaitCase.pFuture && pSched ? "PASS" : "FAIL");
		printf("  Stream close future wait status : %s\n", tWaitCase.iWaitStatus == XRT_NET_CLOSED ? "PASS" : "FAIL");
		printf("  Stream close future value : %s\n", tWaitCase.pResolvedValue == pStream ? "PASS" : "FAIL");
		printf("  Stream close future completion : %s\n", tWaitCase.bDone ? "PASS" : "FAIL");

		xrtClearError();
		if ( tWaitCase.pFuture ) {
			xrtNetFutureDestroy(tWaitCase.pFuture);
			tWaitCase.pFuture = NULL;
		}
		bFinalFutureDestroyOk = xrtGetError() && xrtGetError()[0] == 0;
		xrtClearError();
		if ( pStream ) {
			xrtNetStreamDestroy(pStream);
			pStream = NULL;
		}
		bFinalStreamDestroyOk = xrtGetError() && xrtGetError()[0] == 0;
		printf("  Stream close future final future destroy succeeds : %s\n", bFinalFutureDestroyOk ? "PASS" : "FAIL");
		printf("  Stream close future final stream destroy succeeds : %s\n", bFinalStreamDestroyOk ? "PASS" : "FAIL");

		if ( pSched ) xrtCoSchedDestroy(pSched);
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetengine* pEngine = NULL;
		xnetstream* pStream = NULL;
		xcosched* pSched = NULL;
		__test_xnet2_sync_coro_case tWaitCase;
		const char* sReadableError = NULL;
		bool bFinalFutureDestroyOk = false;
		bool bFinalStreamDestroyOk = false;

		memset(&tWaitCase, 0, sizeof(tWaitCase));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pStream = pEngine ? xrtNetStreamCreate(pEngine, NULL, NULL) : NULL;
		if ( pStream ) {
			xrtNetStreamPauseRead(pStream);
			tWaitCase.pFuture = xrtNetStreamReadableFuture(pStream);
		}
		pSched = xrtCoSchedCreate();
		if ( pEngine && pStream && tWaitCase.pFuture && pSched ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncFutureWaitCo, &tWaitCase, 0);
			(void)__xnetStreamPostRecvCopy(pStream, "ready", 5);
			xrtCoSchedRun(pSched);
		}

		printf("  Stream readable future create : %s\n", pEngine && pStream && tWaitCase.pFuture && pSched ? "PASS" : "FAIL");
		printf("  Stream readable future wait status : %s\n", tWaitCase.iWaitStatus == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Stream readable future value : %s\n", tWaitCase.pResolvedValue == pStream ? "PASS" : "FAIL");
		printf("  Stream readable future completion : %s\n", tWaitCase.bDone ? "PASS" : "FAIL");
		printf("  Stream readable future buffers recv bytes : %s\n",
			pStream && xrtNetChainBytes(&pStream->tRxChain) == 5 ? "PASS" : "FAIL");

		xrtClearError();
		if ( tWaitCase.pFuture ) {
			xrtNetFutureDestroy(tWaitCase.pFuture);
			tWaitCase.pFuture = NULL;
		}
		bFinalFutureDestroyOk = xrtGetError() && xrtGetError()[0] == 0;
		xrtClearError();
		if ( pStream ) {
			xrtNetStreamDestroy(pStream);
			pStream = NULL;
		}
		bFinalStreamDestroyOk = xrtGetError() && xrtGetError()[0] == 0;
		printf("  Stream readable future final future destroy succeeds : %s\n", bFinalFutureDestroyOk ? "PASS" : "FAIL");
		printf("  Stream readable future final stream destroy succeeds : %s\n", bFinalStreamDestroyOk ? "PASS" : "FAIL");

		if ( pSched ) xrtCoSchedDestroy(pSched);
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}

		xrtNetEngineConfigInit(&tCfg);
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pStream = pEngine ? xrtNetStreamCreate(pEngine, NULL, NULL) : NULL;
		xrtClearError();
		tWaitCase.pFuture = pStream ? xrtNetStreamReadableFuture(pStream) : NULL;
		sReadableError = xrtGetError();
		printf("  Stream readable future rejects unpaused read : %s\n",
			pStream && tWaitCase.pFuture == NULL && sReadableError && sReadableError[0] != 0 ? "PASS" : "FAIL");

		if ( pStream ) xrtNetStreamDestroy(pStream);
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetengine* pEngine = NULL;
		xnetstream* pStream = NULL;
		xnetfuture* pFuture = NULL;
		const char* sInvalidKindError = NULL;

		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pStream = pEngine ? xrtNetStreamCreate(pEngine, NULL, NULL) : NULL;
		xrtClearError();
		pFuture = pStream ? xrtNetStreamFutureEx(pStream, 99u) : NULL;
		sInvalidKindError = xrtGetError();
		printf("  Stream generic future rejects invalid kind : %s\n",
			pStream && pFuture == NULL && sInvalidKindError && sInvalidKindError[0] != 0 ? "PASS" : "FAIL");

		if ( pFuture ) xrtNetFutureDestroy(pFuture);
		if ( pStream ) xrtNetStreamDestroy(pStream);
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetengine* pEngine = NULL;
		xnetstream* pStream = NULL;
		xcosched* pSched = NULL;
		__test_xnet2_sync_coro_case tWaitCase;

		memset(&tWaitCase, 0, sizeof(tWaitCase));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pStream = pEngine ? xrtNetStreamCreate(pEngine, NULL, NULL) : NULL;
		if ( pStream ) {
			xrtNetStreamPauseRead(pStream);
			tWaitCase.pStream = pStream;
		}
		pSched = xrtCoSchedCreate();
		if ( pEngine && pStream && pSched ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncStreamReadableWaitCo, &tWaitCase, 0);
			(void)__xnetStreamPostRecvCopy(pStream, "rwait", 5);
			xrtCoSchedRun(pSched);
		}

		printf("  Stream readable coroutine helper create : %s\n", pEngine && pStream && pSched ? "PASS" : "FAIL");
		printf("  Stream readable coroutine helper wait status : %s\n", tWaitCase.iWaitStatus == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Stream readable coroutine helper completion : %s\n", tWaitCase.bDone ? "PASS" : "FAIL");
		printf("  Stream readable coroutine helper buffers recv bytes : %s\n",
			pStream && xrtNetChainBytes(&pStream->tRxChain) == 5 ? "PASS" : "FAIL");

		if ( pSched ) xrtCoSchedDestroy(pSched);
		if ( pStream ) xrtNetStreamDestroy(pStream);
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetengine* pEngine = NULL;
		xnetstream* pStream = NULL;
		xnet_result iDeadlineStatus = XRT_NET_ERROR;
		xnet_result iRetryStatus = XRT_NET_ERROR;
		xnetwaitsrc tSrc = xrtNetWaitSourceNone();
		int64 iNowMs = 0;

		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pStream = pEngine ? xrtNetStreamCreate(pEngine, NULL, NULL) : NULL;
		if ( pStream ) {
			__xnetStreamApplyWatermark(pStream, 8, 4);
			(void)xrtNetStreamSend(pStream, "0123456789AB", 12);
			tSrc = xrtNetWaitSourceStream(pStream, XNET_STREAM_WAIT_WRITABLE);
			iNowMs = (int64)(xrtTimer() * 1000.0);
			iDeadlineStatus = xrtNetWaitSourceWaitUntil(&tSrc, iNowMs + 35);
			(void)xrtNetEnginePostDelayed(
				pEngine,
				pStream->pWorker ? pStream->pWorker->iId : 0,
				40,
				__Test_XNet2_SyncStreamWritableTask,
				pStream);
			iRetryStatus = xrtNetWaitSourceWait(&tSrc);
		}

		printf("  Wait source stream writable sync deadline helper create : %s\n", pEngine && pStream ? "PASS" : "FAIL");
		printf("  Wait source stream writable sync deadline helper status : %s\n", iDeadlineStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Wait source stream writable sync deadline helper unregisters waiter : %s\n", iRetryStatus == XRT_NET_OK ? "PASS" : "FAIL");

		if ( pStream ) xrtNetStreamDestroy(pStream);
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetengine* pEngine = NULL;
		xnetstream* pStream = NULL;
		xcosched* pSched = NULL;
		__test_xnet2_sync_coro_case tWaitCase;

		memset(&tWaitCase, 0, sizeof(tWaitCase));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pStream = pEngine ? xrtNetStreamCreate(pEngine, NULL, NULL) : NULL;
		if ( pStream ) {
			xrtNetStreamPauseRead(pStream);
			tWaitCase.pStream = pStream;
		}
		pSched = xrtCoSchedCreate();
		if ( pEngine && pStream && pSched ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncStreamWaitCoExReadable, &tWaitCase, 0);
			(void)__xnetStreamPostRecvCopy(pStream, "gread", 5);
			xrtCoSchedRun(pSched);
		}

		printf("  Stream generic coroutine wait helper create : %s\n", pEngine && pStream && pSched ? "PASS" : "FAIL");
		printf("  Stream generic coroutine wait helper status : %s\n", tWaitCase.iWaitStatus == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Stream generic coroutine wait helper completion : %s\n", tWaitCase.bDone ? "PASS" : "FAIL");

		if ( pSched ) xrtCoSchedDestroy(pSched);
		if ( pStream ) xrtNetStreamDestroy(pStream);
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetengine* pEngine = NULL;
		xnetstream* pStream = NULL;
		xcosched* pSched = NULL;
		__test_xnet2_sync_coro_case tWaitCase;
		bool bEarlyDestroyRejected = false;
		bool bFinalFutureDestroyOk = false;
		bool bFinalStreamDestroyOk = false;

		memset(&tWaitCase, 0, sizeof(tWaitCase));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pStream = pEngine ? xrtNetStreamCreate(pEngine, NULL, NULL) : NULL;
		if ( pStream ) {
			__xnetStreamApplyWatermark(pStream, 8, 4);
			(void)xrtNetStreamSend(pStream, "0123456789AB", 12);
			tWaitCase.pFuture = xrtNetStreamWritableFuture(pStream);
		}

		xrtClearError();
		if ( pStream ) {
			xrtNetStreamDestroy(pStream);
			bEarlyDestroyRejected = xrtGetError() && xrtGetError()[0] != 0;
		}

		pSched = xrtCoSchedCreate();
		if ( pEngine && pStream && tWaitCase.pFuture && pSched ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncFutureWaitCo, &tWaitCase, 0);
			(void)xrtNetEnginePostDelayed(
				pEngine,
				pStream->pWorker ? pStream->pWorker->iId : 0,
				40,
				__Test_XNet2_SyncStreamWritableTask,
				pStream);
			xrtCoSchedRun(pSched);
		}

		printf("  Stream writable future create : %s\n", pEngine && pStream && tWaitCase.pFuture && pSched ? "PASS" : "FAIL");
		printf("  Stream writable future rejects early stream destroy : %s\n", bEarlyDestroyRejected ? "PASS" : "FAIL");
		printf("  Stream writable future wait status : %s\n", tWaitCase.iWaitStatus == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Stream writable future value : %s\n", tWaitCase.pResolvedValue == pStream ? "PASS" : "FAIL");
		printf("  Stream writable future completion : %s\n", tWaitCase.bDone ? "PASS" : "FAIL");
		printf("  Stream writable future relieves pressure to low water or below : %s\n",
			pStream && xrtNetStreamPendingSend(pStream) <= 4 && !pStream->tSendQ.bHighWaterHit ? "PASS" : "FAIL");

		xrtClearError();
		if ( tWaitCase.pFuture ) {
			xrtNetFutureDestroy(tWaitCase.pFuture);
			tWaitCase.pFuture = NULL;
		}
		bFinalFutureDestroyOk = xrtGetError() && xrtGetError()[0] == 0;
		xrtClearError();
		if ( pStream ) {
			xrtNetStreamDestroy(pStream);
			pStream = NULL;
		}
		bFinalStreamDestroyOk = xrtGetError() && xrtGetError()[0] == 0;
		printf("  Stream writable future final future destroy succeeds : %s\n", bFinalFutureDestroyOk ? "PASS" : "FAIL");
		printf("  Stream writable future final stream destroy succeeds : %s\n", bFinalStreamDestroyOk ? "PASS" : "FAIL");

		if ( pSched ) xrtCoSchedDestroy(pSched);
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetengine* pEngine = NULL;
		xnetstream* pStream = NULL;
		xcosched* pSched = NULL;
		__test_xnet2_sync_coro_case tWaitCase;

		memset(&tWaitCase, 0, sizeof(tWaitCase));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pStream = pEngine ? xrtNetStreamCreate(pEngine, NULL, NULL) : NULL;
		if ( pStream ) {
			__xnetStreamApplyWatermark(pStream, 8, 4);
			(void)xrtNetStreamSend(pStream, "0123456789AB", 12);
			tWaitCase.pStream = pStream;
		}
		pSched = xrtCoSchedCreate();
		if ( pEngine && pStream && pSched ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncStreamWritableWaitCo, &tWaitCase, 0);
			(void)xrtNetEnginePostDelayed(
				pEngine,
				pStream->pWorker ? pStream->pWorker->iId : 0,
				40,
				__Test_XNet2_SyncStreamWritableTask,
				pStream);
			xrtCoSchedRun(pSched);
		}

		printf("  Stream writable coroutine helper create : %s\n", pEngine && pStream && pSched ? "PASS" : "FAIL");
		printf("  Stream writable coroutine helper wait status : %s\n", tWaitCase.iWaitStatus == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Stream writable coroutine helper completion : %s\n", tWaitCase.bDone ? "PASS" : "FAIL");
		printf("  Stream writable coroutine helper relieves pressure : %s\n",
			pStream && xrtNetStreamPendingSend(pStream) <= 4 && !pStream->tSendQ.bHighWaterHit ? "PASS" : "FAIL");

		if ( pSched ) xrtCoSchedDestroy(pSched);
		if ( pStream ) xrtNetStreamDestroy(pStream);
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetengine* pEngine = NULL;
		xnetstream* pStream = NULL;
		xcosched* pSched = NULL;
		__test_xnet2_sync_coro_case tWaitCase;
		__test_xnet2_sync_coro_case tRetryCase;

		memset(&tWaitCase, 0, sizeof(tWaitCase));
		memset(&tRetryCase, 0, sizeof(tRetryCase));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pStream = pEngine ? xrtNetStreamCreate(pEngine, NULL, NULL) : NULL;
		if ( pStream ) {
			__xnetStreamApplyWatermark(pStream, 8, 4);
			(void)xrtNetStreamSend(pStream, "0123456789AB", 12);
			tWaitCase.pStream = pStream;
			tRetryCase.pStream = pStream;
		}
		pSched = xrtCoSchedCreate();
		if ( pEngine && pStream && pSched ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncStreamWritableWaitCoUntil, &tWaitCase, 0);
			xrtCoSchedRun(pSched);
		}
		if ( pEngine && pStream && pSched ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncStreamWritableWaitCo, &tRetryCase, 0);
			(void)xrtNetEnginePostDelayed(
				pEngine,
				pStream->pWorker ? pStream->pWorker->iId : 0,
				40,
				__Test_XNet2_SyncStreamWritableTask,
				pStream);
			xrtCoSchedRun(pSched);
		}

		printf("  Stream writable coroutine deadline helper create : %s\n", pEngine && pStream && pSched ? "PASS" : "FAIL");
		printf("  Stream writable coroutine deadline helper status : %s\n", tWaitCase.iDeadlineStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Stream writable coroutine deadline helper completion : %s\n", tWaitCase.bDeadlineDone ? "PASS" : "FAIL");
		printf("  Stream writable coroutine deadline helper unregisters waiter : %s\n",
			tRetryCase.iWaitStatus == XRT_NET_OK && tRetryCase.bDone ? "PASS" : "FAIL");

		if ( pSched ) xrtCoSchedDestroy(pSched);
		if ( pStream ) xrtNetStreamDestroy(pStream);
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetengine* pEngine = NULL;
		xnetstream* pStream = NULL;
		xcosched* pSched = NULL;
		__test_xnet2_sync_coro_case tWaitCase;
		__test_xnet2_sync_coro_case tRetryCase;

		memset(&tWaitCase, 0, sizeof(tWaitCase));
		memset(&tRetryCase, 0, sizeof(tRetryCase));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pStream = pEngine ? xrtNetStreamCreate(pEngine, NULL, NULL) : NULL;
		if ( pStream ) {
			__xnetStreamApplyWatermark(pStream, 8, 4);
			(void)xrtNetStreamSend(pStream, "0123456789AB", 12);
			tWaitCase.pStream = pStream;
			tRetryCase.pStream = pStream;
		}
		pSched = xrtCoSchedCreate();
		if ( pEngine && pStream && pSched ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncWaitSourceStreamWritableCoUntil, &tWaitCase, 0);
			xrtCoSchedRun(pSched);
		}
		if ( pEngine && pStream && pSched ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncWaitSourceStreamWritableCo, &tRetryCase, 0);
			(void)xrtNetEnginePostDelayed(
				pEngine,
				pStream->pWorker ? pStream->pWorker->iId : 0,
				40,
				__Test_XNet2_SyncStreamWritableTask,
				pStream);
			xrtCoSchedRun(pSched);
		}

		printf("  Wait source stream writable deadline helper create : %s\n", pEngine && pStream && pSched ? "PASS" : "FAIL");
		printf("  Wait source stream writable deadline helper status : %s\n", tWaitCase.iDeadlineStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Wait source stream writable deadline helper completion : %s\n", tWaitCase.bDeadlineDone ? "PASS" : "FAIL");
		printf("  Wait source stream writable deadline helper unregisters waiter : %s\n",
			tRetryCase.iWaitStatus == XRT_NET_OK && tRetryCase.bDone ? "PASS" : "FAIL");

		if ( pSched ) xrtCoSchedDestroy(pSched);
		if ( pStream ) xrtNetStreamDestroy(pStream);
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}
	#endif
}
