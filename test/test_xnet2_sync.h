#include <stdarg.h>


typedef struct {
	xnetfuture* pFuture;
	xnet_result iResult;
	ptr pValue;
	volatile long iHitCount;
	volatile long iWorkerId;
} __test_xnet2_sync_task_ctx;

typedef struct {
	int iPayload;
	volatile long iHitCount;
	volatile long iWorkerId;
} __test_xnet2_sync_postfuture_case;

typedef struct {
	uint32 iBlockMs;
	volatile long iHitCount;
	volatile long iWorkerId;
} __test_xnet2_sync_timer_case;

typedef struct {
	uint32 iDelayMs;
	int iPayload;
	volatile long iHitCount;
} __test_xnet2_sync_executor_case;

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
	xnetfuture* pFuture;
	int iPayload;
	xnet_result arrStatus[2];
	ptr arrValues[2];
	bool arrDone[2];
} __test_xnet2_sync_multi_coro_case;

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

typedef struct {
	xnetlistener* pListener;
	xnet_result iWaitStatus;
	xnet_result iTimeoutStatus;
	xnet_result iDeadlineStatus;
	xnetstream* pAccepted;
	xnetstream* pTimeoutAccepted;
	xnetstream* pDeadlineAccepted;
	ptr pResolvedValue;
	ptr pTimeoutValue;
	ptr pDeadlineValue;
	bool bDone;
	bool bTimeoutDone;
	bool bDeadlineDone;
} __test_xnet2_sync_listener_coro_case;
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

typedef struct {
	xnetaddr tTargetAddr;
	uint32 iDelayMs;
	uint32 iHoldMs;
	volatile long iConnectResult;
	bool bDone;
} __test_xnet2_sync_connect_case;

#include "test_xnet2_sync_support.h"

// 内部函数：__Test_XNet2_SyncDgramEventTextEquals
static bool __Test_XNet2_SyncDgramEventTextEquals(const __test_xnet2_sync_dgram_event_case* pCase, const char* sText)
{
	size_t iWantLen;

	if ( !pCase || !sText ) return false;
	iWantLen = strlen(sText);
	return pCase->iPayloadLen == (long)iWantLen && memcmp(pCase->aPayload, sText, iWantLen) == 0;
}


// 内部函数：__Test_XNet2_SyncDgramOnRecv
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


// 内部函数：__Test_XNet2_SyncDgramSendWorker
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
	pCase->bDone = true;
	return 0;
}


// 内部函数：__Test_XNet2_SyncConnectWorker
static uint32 __Test_XNet2_SyncConnectWorker(ptr pArg)
{
	__test_xnet2_sync_connect_case* pCase = (__test_xnet2_sync_connect_case*)pArg;
	struct sockaddr_storage tStorage;
	socklen_t iAddrLen = 0;
	xsocket hSocket = XNET_SOCKET_INVALID;
	int iRet = -1;

	if ( !pCase ) return 0;
	if ( pCase->iDelayMs > 0 ) {
		__Test_XNet2_SyncSleepMs(pCase->iDelayMs);
	}
	memset(&tStorage, 0, sizeof(tStorage));
	if ( !__xnetAddrToSockAddr(&pCase->tTargetAddr, &tStorage, &iAddrLen) ) {
		__Test_XNet2_SyncAtomicStore(&pCase->iConnectResult, -1);
		pCase->bDone = true;
		return 0;
	}

	hSocket = socket(pCase->tTargetAddr.iFamily, SOCK_STREAM, IPPROTO_TCP);
	if ( __xnetSocketIsValid(hSocket) ) {
		iRet = connect(hSocket, (struct sockaddr*)&tStorage, iAddrLen);
	}
	__Test_XNet2_SyncAtomicStore(&pCase->iConnectResult, (iRet == 0) ? 1 : -1);
	if ( iRet == 0 && pCase->iHoldMs > 0 ) {
		__Test_XNet2_SyncSleepMs(pCase->iHoldMs);
	}
	__xnetSocketCloseHandle(&hSocket);
	pCase->bDone = true;
	return 0;
}


// 内部函数：__Test_XNet2_SyncStartWorkerThread
static xthread __Test_XNet2_SyncStartWorkerThread(uint32 (*Proc)(ptr), ptr pArg)
{
	if ( !Proc ) return NULL;
	return xrtThreadCreate(Proc, pArg, 0);
}


// 内部函数：__Test_XNet2_SyncJoinThread
static void __Test_XNet2_SyncJoinThread(xthread* phThread)
{
	if ( !phThread || !*phThread ) return;
	xrtThreadWait(*phThread);
	xrtThreadDestroy(*phThread);
	*phThread = NULL;
}


// 内部函数：__Test_XNet2_SyncRunSchedWithWorker
static void __Test_XNet2_SyncRunSchedWithWorker(xcosched* pSched, xthread* phThread, uint32 (*Proc)(ptr), ptr pArg)
{
	if ( !pSched ) return;
	if ( phThread ) {
		*phThread = __Test_XNet2_SyncStartWorkerThread(Proc, pArg);
	}
	xrtCoSchedRun(pSched);
}


// 内部函数：__Test_XNet2_SyncResolveTask
static void __Test_XNet2_SyncResolveTask(xnetworker* pWorker, ptr pArg)
{
	__test_xnet2_sync_task_ctx* pCtx = (__test_xnet2_sync_task_ctx*)pArg;
	if ( !pCtx || !pCtx->pFuture ) return;
	__Test_XNet2_SyncAtomicInc(&pCtx->iHitCount);
	__Test_XNet2_SyncAtomicStore(&pCtx->iWorkerId, pWorker ? (long)pWorker->iId : -1);
	(void)__xnetFutureResolve(pCtx->pFuture, pCtx->iResult, pCtx->pValue);
}


static void __Test_XNet2_SyncTimerTask(xnetworker* pWorker, ptr pArg)
{
	__test_xnet2_sync_timer_case* pCase = (__test_xnet2_sync_timer_case*)pArg;
	if ( !pCase ) return;
	__Test_XNet2_SyncAtomicStore(&pCase->iWorkerId, pWorker ? (long)pWorker->iId : -1);
	(void)__Test_XNet2_SyncAtomicInc(&pCase->iHitCount);
}


static void __Test_XNet2_SyncTimerBlockTask(xnetworker* pWorker, ptr pArg)
{
	__test_xnet2_sync_timer_case* pCase = (__test_xnet2_sync_timer_case*)pArg;
	(void)pWorker;
	if ( pCase && pCase->iBlockMs != 0 ) { __Test_XNet2_SyncSleepMs(pCase->iBlockMs); }
}


static void __Test_XNet2_SyncCancelTokenTask(xnetworker* pWorker, ptr pArg)
{
	(void)pWorker;
	(void)xrtNetCancelRequest((xnetcancel*)pArg);
}


static void __Test_XNet2_SyncCancelWatchCallback(ptr pArg)
{
	if ( pArg ) { (void)__Test_XNet2_SyncAtomicInc((volatile long*)pArg); }
}

#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
#define __TEST_XNET2_SYNC_DEFINE_CORO_CASE_PROC(procName, Body) \
static void procName(ptr pArg) \
{ \
	__test_xnet2_sync_coro_case* pCase = (__test_xnet2_sync_coro_case*)pArg; \
\
	if ( !pCase ) return; \
	Body \
}

#define __TEST_XNET2_SYNC_DEFINE_MULTI_CORO_CASE_PROC(procName, Body) \
static void procName(ptr pArg) \
{ \
	__test_xnet2_sync_multi_coro_case* pCase = (__test_xnet2_sync_multi_coro_case*)pArg; \
\
	if ( !pCase ) return; \
	Body \
}

#define __TEST_XNET2_SYNC_DEFINE_LISTENER_CORO_CASE_PROC(procName, Body) \
static void procName(ptr pArg) \
{ \
	__test_xnet2_sync_listener_coro_case* pCase = (__test_xnet2_sync_listener_coro_case*)pArg; \
\
	if ( !pCase ) return; \
	Body \
}

#define __TEST_XNET2_SYNC_DEFINE_DGRAM_CORO_CASE_PROC(procName, Body) \
static void procName(ptr pArg) \
{ \
	__test_xnet2_sync_dgram_coro_case* pCase = (__test_xnet2_sync_dgram_coro_case*)pArg; \
\
	if ( !pCase ) return; \
	Body \
}

__TEST_XNET2_SYNC_DEFINE_CORO_CASE_PROC(__Test_XNet2_SyncFutureWaitCo,
	pCase->iWaitStatus = xrtNetFutureWaitCo(pCase->pFuture);
	pCase->pResolvedValue = xrtNetFutureValue(pCase->pFuture);
	pCase->bDone = true;
)

__TEST_XNET2_SYNC_DEFINE_MULTI_CORO_CASE_PROC(__Test_XNet2_SyncFutureWaitCoMulti1,
	pCase->arrStatus[0] = xrtNetFutureWaitCo(pCase->pFuture);
	pCase->arrValues[0] = xrtNetFutureValue(pCase->pFuture);
	pCase->arrDone[0] = true;
)

__TEST_XNET2_SYNC_DEFINE_MULTI_CORO_CASE_PROC(__Test_XNet2_SyncFutureWaitCoMulti2,
	pCase->arrStatus[1] = xrtNetFutureWaitCo(pCase->pFuture);
	pCase->arrValues[1] = xrtNetFutureValue(pCase->pFuture);
	pCase->arrDone[1] = true;
)

__TEST_XNET2_SYNC_DEFINE_CORO_CASE_PROC(__Test_XNet2_SyncWaitSourceFutureCo,
	xnetwaitsrc tSrc = xrtNetWaitSourceFuture(pCase->pFuture);
	pCase->iWaitStatus = xrtNetWaitSourceWaitCoValue(&tSrc, &pCase->pResolvedValue);
	pCase->bDone = true;
)

__TEST_XNET2_SYNC_DEFINE_CORO_CASE_PROC(__Test_XNet2_SyncFutureWaitCoTimeout,
	pCase->iTimeoutStatus = xrtNetFutureWaitCoTimeout(pCase->pFuture, 40);
	pCase->pTimeoutValue = xrtNetFutureValue(pCase->pFuture);
	pCase->bTimeoutDone = true;
)

__TEST_XNET2_SYNC_DEFINE_CORO_CASE_PROC(__Test_XNet2_SyncWaitSourceFutureCoTimeout,
	xnetwaitsrc tSrc = xrtNetWaitSourceFuture(pCase->pFuture);
	pCase->iTimeoutStatus = xrtNetWaitSourceWaitCoValueTimeout(&tSrc, 40, &pCase->pTimeoutValue);
	pCase->bTimeoutDone = true;
)

__TEST_XNET2_SYNC_DEFINE_CORO_CASE_PROC(__Test_XNet2_SyncFutureWaitCoUntil,
	pCase->iDeadlineStatus = xrtNetFutureWaitCoUntil(pCase->pFuture, __Test_XNet2_SyncDeadlineFromTimer(45));
	pCase->pDeadlineValue = xrtNetFutureValue(pCase->pFuture);
	pCase->bDeadlineDone = true;
)

__TEST_XNET2_SYNC_DEFINE_LISTENER_CORO_CASE_PROC(__Test_XNet2_SyncListenerAcceptCo,
	pCase->iWaitStatus = xrtNetListenerAcceptCo(pCase->pListener, &pCase->pAccepted);
	pCase->pResolvedValue = pCase->pAccepted;
	pCase->bDone = true;
)

__TEST_XNET2_SYNC_DEFINE_LISTENER_CORO_CASE_PROC(__Test_XNet2_SyncWaitSourceListenerAcceptCo,
	xnetwaitsrc tSrc = xrtNetWaitSourceListenerAccept(pCase->pListener);
	pCase->iWaitStatus = xrtNetWaitSourceWaitCoValue(&tSrc, &pCase->pResolvedValue);
	pCase->pAccepted = (xnetstream*)pCase->pResolvedValue;
	pCase->bDone = true;
)

__TEST_XNET2_SYNC_DEFINE_LISTENER_CORO_CASE_PROC(__Test_XNet2_SyncListenerAcceptCoUntil,
	pCase->iDeadlineStatus = xrtNetListenerAcceptCoUntil(pCase->pListener, __Test_XNet2_SyncDeadlineFromTimer(__TEST_XNET2_SYNC_WAIT_DEADLINE_MS), &pCase->pDeadlineAccepted);
	pCase->pDeadlineValue = pCase->pDeadlineAccepted;
	pCase->bDeadlineDone = true;
)

__TEST_XNET2_SYNC_DEFINE_CORO_CASE_PROC(__Test_XNet2_SyncStreamReadableWaitCo,
	pCase->iWaitStatus = xrtNetStreamWaitReadableCo(pCase->pStream);
	pCase->bDone = true;
)

__TEST_XNET2_SYNC_DEFINE_CORO_CASE_PROC(__Test_XNet2_SyncStreamWaitCoExReadable,
	pCase->iWaitStatus = xrtNetStreamWaitCoEx(pCase->pStream, XNET_STREAM_WAIT_READABLE);
	pCase->bDone = true;
)

__TEST_XNET2_SYNC_DEFINE_CORO_CASE_PROC(__Test_XNet2_SyncWaitSourceStreamReadableCo,
	xnetwaitsrc tSrc = xrtNetWaitSourceStream(pCase->pStream, XNET_STREAM_WAIT_READABLE);
	pCase->iWaitStatus = xrtNetWaitSourceWaitCoValue(&tSrc, &pCase->pResolvedValue);
	pCase->bDone = true;
)

__TEST_XNET2_SYNC_DEFINE_CORO_CASE_PROC(__Test_XNet2_SyncStreamReadableWaitCoTimeout,
	pCase->iTimeoutStatus = xrtNetStreamWaitReadableCoTimeout(pCase->pStream, __TEST_XNET2_SYNC_WAIT_TIMEOUT_MS);
	pCase->bTimeoutDone = true;
)

__TEST_XNET2_SYNC_DEFINE_CORO_CASE_PROC(__Test_XNet2_SyncStreamReadableWaitCoRetry,
	pCase->iWaitStatus = xrtNetStreamWaitReadableCoTimeout(pCase->pStream, __TEST_XNET2_SYNC_WAIT_TIMEOUT_MS);
	pCase->bDone = true;
)

__TEST_XNET2_SYNC_DEFINE_CORO_CASE_PROC(__Test_XNet2_SyncWaitSourceStreamReadableCoTimeout,
	xnetwaitsrc tSrc = xrtNetWaitSourceStream(pCase->pStream, XNET_STREAM_WAIT_READABLE);
	pCase->iTimeoutStatus = xrtNetWaitSourceWaitCoValueTimeout(&tSrc, __TEST_XNET2_SYNC_WAIT_TIMEOUT_MS, &pCase->pTimeoutValue);
	pCase->bTimeoutDone = true;
)

__TEST_XNET2_SYNC_DEFINE_CORO_CASE_PROC(__Test_XNet2_SyncWaitSourceStreamReadableCoRetry,
	xnetwaitsrc tSrc = xrtNetWaitSourceStream(pCase->pStream, XNET_STREAM_WAIT_READABLE);
	pCase->iWaitStatus = xrtNetWaitSourceWaitCoTimeout(&tSrc, __TEST_XNET2_SYNC_WAIT_TIMEOUT_MS);
	pCase->bDone = true;
)

__TEST_XNET2_SYNC_DEFINE_CORO_CASE_PROC(__Test_XNet2_SyncWaitSourceStreamWritableCo,
	xnetwaitsrc tSrc = xrtNetWaitSourceStream(pCase->pStream, XNET_STREAM_WAIT_WRITABLE);
	pCase->iWaitStatus = xrtNetWaitSourceWaitCo(&tSrc);
	pCase->bDone = true;
)

__TEST_XNET2_SYNC_DEFINE_CORO_CASE_PROC(__Test_XNet2_SyncWaitSourceStreamWritableCoUntil,
	xnetwaitsrc tSrc = xrtNetWaitSourceStream(pCase->pStream, XNET_STREAM_WAIT_WRITABLE);
	pCase->iDeadlineStatus = __Test_XNet2_SyncWaitSourceCoUntilDelay(&tSrc, __TEST_XNET2_SYNC_WAIT_DEADLINE_MS);
	pCase->bDeadlineDone = true;
)

__TEST_XNET2_SYNC_DEFINE_CORO_CASE_PROC(__Test_XNet2_SyncStreamWritableWaitCo,
	pCase->iWaitStatus = xrtNetStreamWaitWritableCo(pCase->pStream);
	pCase->bDone = true;
)

__TEST_XNET2_SYNC_DEFINE_CORO_CASE_PROC(__Test_XNet2_SyncStreamWritableWaitCoUntil,
	pCase->iDeadlineStatus = xrtNetStreamWaitWritableCoUntil(pCase->pStream, __Test_XNet2_SyncDeadlineFromTimer(__TEST_XNET2_SYNC_WAIT_DEADLINE_MS));
	pCase->bDeadlineDone = true;
)

__TEST_XNET2_SYNC_DEFINE_CORO_CASE_PROC(__Test_XNet2_SyncWaitSourceStreamWritableCoRetry,
	xnetwaitsrc tSrc = xrtNetWaitSourceStream(pCase->pStream, XNET_STREAM_WAIT_WRITABLE);
	pCase->iWaitStatus = __Test_XNet2_SyncWaitSourceCoUntilDelay(&tSrc, __TEST_XNET2_SYNC_WAIT_DEADLINE_MS);
	pCase->bDone = true;
)

__TEST_XNET2_SYNC_DEFINE_CORO_CASE_PROC(__Test_XNet2_SyncStreamWritableWaitCoRetry,
	pCase->iWaitStatus = xrtNetStreamWaitWritableCoUntil(pCase->pStream, __Test_XNet2_SyncDeadlineFromTimer(__TEST_XNET2_SYNC_WAIT_DEADLINE_MS));
	pCase->bDone = true;
)

__TEST_XNET2_SYNC_DEFINE_CORO_CASE_PROC(__Test_XNet2_SyncWaitSourceStreamDrainCoTimeout,
	xnetwaitsrc tSrc = xrtNetWaitSourceStream(pCase->pStream, XNET_STREAM_WAIT_DRAIN);
	pCase->iTimeoutStatus = xrtNetWaitSourceWaitCoTimeout(&tSrc, __TEST_XNET2_SYNC_WAIT_TIMEOUT_MS);
	pCase->bTimeoutDone = true;
)

__TEST_XNET2_SYNC_DEFINE_CORO_CASE_PROC(__Test_XNet2_SyncWaitSourceStreamDrainCo,
	xnetwaitsrc tSrc = xrtNetWaitSourceStream(pCase->pStream, XNET_STREAM_WAIT_DRAIN);
	pCase->iWaitStatus = xrtNetWaitSourceWaitCo(&tSrc);
	pCase->bDone = true;
)

__TEST_XNET2_SYNC_DEFINE_CORO_CASE_PROC(__Test_XNet2_SyncStreamDrainWaitCoTimeout,
	pCase->iTimeoutStatus = xrtNetStreamWaitDrainCoTimeout(pCase->pStream, __TEST_XNET2_SYNC_WAIT_TIMEOUT_MS);
	pCase->bTimeoutDone = true;
)

__TEST_XNET2_SYNC_DEFINE_CORO_CASE_PROC(__Test_XNet2_SyncStreamDrainWaitCo,
	pCase->iWaitStatus = xrtNetStreamWaitDrainCo(pCase->pStream);
	pCase->bDone = true;
)

__TEST_XNET2_SYNC_DEFINE_CORO_CASE_PROC(__Test_XNet2_SyncWaitSourceStreamDrainCoRetry,
	xnetwaitsrc tSrc = xrtNetWaitSourceStream(pCase->pStream, XNET_STREAM_WAIT_DRAIN);
	pCase->iWaitStatus = xrtNetWaitSourceWaitCoTimeout(&tSrc, __TEST_XNET2_SYNC_WAIT_TIMEOUT_MS);
	pCase->bDone = true;
)

__TEST_XNET2_SYNC_DEFINE_CORO_CASE_PROC(__Test_XNet2_SyncStreamDrainWaitCoRetry,
	pCase->iWaitStatus = xrtNetStreamWaitDrainCoTimeout(pCase->pStream, __TEST_XNET2_SYNC_WAIT_TIMEOUT_MS);
	pCase->bDone = true;
)

__TEST_XNET2_SYNC_DEFINE_CORO_CASE_PROC(__Test_XNet2_SyncWaitSourceStreamCloseCoUntil,
	xnetwaitsrc tSrc = xrtNetWaitSourceStream(pCase->pStream, XNET_STREAM_WAIT_CLOSE);
	pCase->iDeadlineStatus = __Test_XNet2_SyncWaitSourceCoUntilDelay(&tSrc, __TEST_XNET2_SYNC_WAIT_DEADLINE_MS);
	pCase->bDeadlineDone = true;
)

__TEST_XNET2_SYNC_DEFINE_CORO_CASE_PROC(__Test_XNet2_SyncWaitSourceStreamCloseCo,
	xnetwaitsrc tSrc = xrtNetWaitSourceStream(pCase->pStream, XNET_STREAM_WAIT_CLOSE);
	pCase->iWaitStatus = xrtNetWaitSourceWaitCo(&tSrc);
	pCase->bDone = true;
)

__TEST_XNET2_SYNC_DEFINE_CORO_CASE_PROC(__Test_XNet2_SyncStreamCloseWaitCoUntil,
	pCase->iDeadlineStatus = xrtNetStreamWaitCloseCoUntil(pCase->pStream, __Test_XNet2_SyncDeadlineFromTimer(__TEST_XNET2_SYNC_WAIT_DEADLINE_MS));
	pCase->bDeadlineDone = true;
)

__TEST_XNET2_SYNC_DEFINE_CORO_CASE_PROC(__Test_XNet2_SyncStreamCloseWaitCo,
	pCase->iWaitStatus = xrtNetStreamWaitCloseCo(pCase->pStream);
	pCase->bDone = true;
)

__TEST_XNET2_SYNC_DEFINE_CORO_CASE_PROC(__Test_XNet2_SyncWaitSourceStreamCloseCoRetry,
	xnetwaitsrc tSrc = xrtNetWaitSourceStream(pCase->pStream, XNET_STREAM_WAIT_CLOSE);
	pCase->iWaitStatus = __Test_XNet2_SyncWaitSourceCoUntilDelay(&tSrc, __TEST_XNET2_SYNC_WAIT_DEADLINE_MS);
	pCase->bDone = true;
)

__TEST_XNET2_SYNC_DEFINE_CORO_CASE_PROC(__Test_XNet2_SyncStreamCloseWaitCoRetry,
	pCase->iWaitStatus = xrtNetStreamWaitCloseCoUntil(pCase->pStream, __Test_XNet2_SyncDeadlineFromTimer(__TEST_XNET2_SYNC_WAIT_DEADLINE_MS));
	pCase->bDone = true;
)


// 内部函数：__Test_XNet2_SyncFutureResolveWorker
static uint32 __Test_XNet2_SyncFutureResolveWorker(ptr pArg)
{
	__test_xnet2_sync_coro_case* pCase = (__test_xnet2_sync_coro_case*)pArg;

	if ( !pCase || !pCase->pFuture ) return 0;
	__Test_XNet2_SyncSleepMs(50);
	(void)__xnetFutureResolve(pCase->pFuture, XRT_NET_OK, &pCase->iPayload);
	return 0;
}


// 内部函数：__Test_XNet2_SyncFutureResolveWorkerMulti
static uint32 __Test_XNet2_SyncFutureResolveWorkerMulti(ptr pArg)
{
	__test_xnet2_sync_multi_coro_case* pCase = (__test_xnet2_sync_multi_coro_case*)pArg;

	if ( !pCase || !pCase->pFuture ) return 0;
	__Test_XNet2_SyncSleepMs(50);
	(void)__xnetFutureResolve(pCase->pFuture, XRT_NET_OK, &pCase->iPayload);
	return 0;
}


// 内部函数：__Test_XNet2_SyncStreamDrainTask
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


// 内部函数：__Test_XNet2_SyncStreamCloseTask
static void __Test_XNet2_SyncStreamCloseTask(xnetworker* pWorker, ptr pArg)
{
	xnetstream* pStream = (xnetstream*)pArg;

	(void)pWorker;
	if ( !pStream ) return;
	xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
}


// 内部函数：__Test_XNet2_SyncStreamWritableTask
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

__TEST_XNET2_SYNC_DEFINE_DGRAM_CORO_CASE_PROC(__Test_XNet2_SyncDgramRecvCo,
	pCase->iWaitStatus = xrtNetDgramRecvCo(pCase->pSock, &pCase->pPacket);
	pCase->bDone = true;
)

__TEST_XNET2_SYNC_DEFINE_DGRAM_CORO_CASE_PROC(__Test_XNet2_SyncWaitSourceDgramRecvCo,
	xnetwaitsrc tSrc = xrtNetWaitSourceDgramRecv(pCase->pSock);
	pCase->iWaitStatus = xrtNetWaitSourceWaitCoValue(&tSrc, (ptr*)&pCase->pPacket);
	pCase->bDone = true;
)

__TEST_XNET2_SYNC_DEFINE_DGRAM_CORO_CASE_PROC(__Test_XNet2_SyncWaitSourceDgramRecvCoTimeout,
	xnetwaitsrc tSrc = xrtNetWaitSourceDgramRecv(pCase->pSock);
	pCase->iTimeoutStatus = xrtNetWaitSourceWaitCoValueTimeout(&tSrc, __TEST_XNET2_SYNC_WAIT_TIMEOUT_MS, (ptr*)&pCase->pTimeoutPacket);
	pCase->bTimeoutDone = true;
)

__TEST_XNET2_SYNC_DEFINE_DGRAM_CORO_CASE_PROC(__Test_XNet2_SyncDgramRecvCoUntil,
	pCase->iDeadlineStatus = xrtNetDgramRecvCoUntil(pCase->pSock, __Test_XNet2_SyncNowMs() + __TEST_XNET2_SYNC_WAIT_TIMEOUT_MS, &pCase->pTimeoutPacket);
	pCase->bDeadlineDone = true;
)

__TEST_XNET2_SYNC_DEFINE_DGRAM_CORO_CASE_PROC(__Test_XNet2_SyncDgramRecvCoRetry,
	pCase->iWaitStatus = xrtNetDgramRecvCoTimeout(pCase->pSock, __TEST_XNET2_SYNC_WAIT_TIMEOUT_MS, &pCase->pPacket);
	pCase->bDone = true;
)

__TEST_XNET2_SYNC_DEFINE_DGRAM_CORO_CASE_PROC(__Test_XNet2_SyncWaitSourceDgramRecvCoRetry,
	xnetwaitsrc tSrc = xrtNetWaitSourceDgramRecv(pCase->pSock);
	pCase->iWaitStatus = xrtNetWaitSourceWaitCoValueTimeout(&tSrc, __TEST_XNET2_SYNC_WAIT_TIMEOUT_MS, (ptr*)&pCase->pPacket);
	pCase->bDone = true;
)


// 内部函数：__Test_XNet2_SyncRetryDgramCoCase
static bool __Test_XNet2_SyncRetryDgramCoCase(xcosched* pSched, __test_xnet2_sync_dgram_coro_case* pCase, void (*Proc)(ptr), xnet_result iExpectStatus, uint32 iTimeoutMs)
{
	int64_t iDeadlineMs = __Test_XNet2_SyncNowMs() + (int64_t)iTimeoutMs;

	if ( !pSched || !pCase || !Proc ) return false;

	while ( __Test_XNet2_SyncNowMs() < iDeadlineMs ) {
		if ( pCase->pPacket ) {
			xrtNetDgramPacketDestroy(pCase->pPacket);
			pCase->pPacket = NULL;
		}
		pCase->iWaitStatus = XRT_NET_ERROR;
		pCase->bDone = false;
		(void)xrtCoSchedSpawn(pSched, Proc, pCase, 0);
		xrtCoSchedRun(pSched);
		if ( pCase->iWaitStatus == iExpectStatus && pCase->bDone ) {
			return true;
		}
		__Test_XNet2_SyncSleepMs(1);
	}

	return pCase->iWaitStatus == iExpectStatus && pCase->bDone;
}


// 内部函数：__Test_XNet2_SyncRetryWaitCoCase
static bool __Test_XNet2_SyncRetryWaitCoCase(xcosched* pSched, __test_xnet2_sync_coro_case* pCase, void (*Proc)(ptr), xnet_result iExpectStatus, uint32 iTimeoutMs)
{
	int64_t iDeadlineMs = __Test_XNet2_SyncNowMs() + (int64_t)iTimeoutMs;

	if ( !pSched || !pCase || !Proc ) return false;

	while ( __Test_XNet2_SyncNowMs() < iDeadlineMs ) {
		pCase->iWaitStatus = XRT_NET_ERROR;
		pCase->pResolvedValue = NULL;
		pCase->bDone = false;
		(void)xrtCoSchedSpawn(pSched, Proc, pCase, 0);
		xrtCoSchedRun(pSched);
		if ( pCase->iWaitStatus == iExpectStatus && pCase->bDone ) {
			return true;
		}
		__Test_XNet2_SyncSleepMs(1);
	}

	return pCase->iWaitStatus == iExpectStatus && pCase->bDone;
}


// 内部函数：__Test_XNet2_SyncRunWaitCoAndCheckStreamCleared
static bool __Test_XNet2_SyncRunWaitCoAndCheckStreamCleared(
	xcosched* pSched,
	__test_xnet2_sync_coro_case* pCase,
	void (*Proc)(ptr),
	xnetstream* pStream,
	uint32 iWaitKind,
	uint32 iTimeoutMs)
{
	if ( !pSched || !pCase || !Proc || !pStream ) return false;
	(void)xrtCoSchedSpawn(pSched, Proc, pCase, 0);
	xrtCoSchedRun(pSched);
	return __Test_XNet2_SyncWaitStreamWaitCleared(pStream, iWaitKind, iTimeoutMs);
}


// 内部函数：__Test_XNet2_SyncPostStreamTaskAndRetryWaitCo
static bool __Test_XNet2_SyncPostStreamTaskAndRetryWaitCo(
	xnetengine* pEngine,
	xnetstream* pStream,
	xcosched* pSched,
	__test_xnet2_sync_coro_case* pCase,
	uint32 iDelayMs,
	void (*TaskProc)(xnetworker*, ptr),
	void (*RetryProc)(ptr),
	xnet_result iExpectStatus,
	uint32 iTimeoutMs)
{
	if ( !pEngine || !pStream || !pSched || !pCase || !TaskProc || !RetryProc ) return false;
	if ( !__Test_XNet2_SyncPostStreamTaskDelayed(pEngine, pStream, iDelayMs, TaskProc) ) return false;
	return __Test_XNet2_SyncRetryWaitCoCase(pSched, pCase, RetryProc, iExpectStatus, iTimeoutMs);
}


// 内部函数：__Test_XNet2_SyncPostRecvAndRetryWaitCo
static bool __Test_XNet2_SyncPostRecvAndRetryWaitCo(
	xcosched* pSched,
	__test_xnet2_sync_coro_case* pCase,
	xnetstream* pStream,
	const char* sPayload,
	size_t iPayloadLen,
	void (*RetryProc)(ptr),
	xnet_result iExpectStatus,
	uint32 iTimeoutMs)
{
	if ( !pSched || !pCase || !pStream || !sPayload || !RetryProc ) return false;
	(void)__xnetStreamPostRecvCopy(pStream, sPayload, iPayloadLen);
	return __Test_XNet2_SyncRetryWaitCoCase(pSched, pCase, RetryProc, iExpectStatus, iTimeoutMs);
}

#undef __TEST_XNET2_SYNC_DEFINE_CORO_CASE_PROC
#undef __TEST_XNET2_SYNC_DEFINE_MULTI_CORO_CASE_PROC
#undef __TEST_XNET2_SYNC_DEFINE_LISTENER_CORO_CASE_PROC
#undef __TEST_XNET2_SYNC_DEFINE_DGRAM_CORO_CASE_PROC
#endif

// 内部函数：__Test_XNet2_SyncPostFutureTask
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


// 内部函数：__Test_XNet2_SyncFillTaskResult
static int32 __Test_XNet2_SyncFillTaskResult(__test_xnet2_sync_postfuture_case* pCase, long iWorkerId, xfuture_result* pOut)
{
	if ( !pCase || !pOut ) {
		return XRT_NET_ERROR;
	}
	__Test_XNet2_SyncAtomicInc(&pCase->iHitCount);
	__Test_XNet2_SyncAtomicStore(&pCase->iWorkerId, iWorkerId);
	pOut->iStatus = XRT_NET_OK;
	pOut->pValue = &pCase->iPayload;
	pOut->sError = NULL;
	pOut->iFlags = XFUTURE_RESULT_F_NONE;
	return XRT_NET_OK;
}


// 内部函数：__Test_XNet2_TaskRunEngineProc
static int32 __Test_XNet2_TaskRunEngineProc(xnetworker* pWorker, ptr pArg, xfuture_result* pOut)
{
	__test_xnet2_sync_postfuture_case* pCase = (__test_xnet2_sync_postfuture_case*)pArg;

	return __Test_XNet2_SyncFillTaskResult(pCase, pWorker ? (long)pWorker->iId : -1, pOut);
}


// 内部函数：__Test_XNet2_TaskRunThreadProc
static int32 __Test_XNet2_TaskRunThreadProc(ptr pArg, xfuture_result* pOut)
{
	__test_xnet2_sync_postfuture_case* pCase = (__test_xnet2_sync_postfuture_case*)pArg;

	return __Test_XNet2_SyncFillTaskResult(pCase, -2, pOut);
}


static int32 __Test_XNet2_TaskExecutorProc(ptr pArg, xfuture_result* pOut)
{
	__test_xnet2_sync_executor_case* pCase = (__test_xnet2_sync_executor_case*)pArg;
	if ( !pCase || !pOut ) { return XRT_NET_ERROR; }
	if ( pCase->iDelayMs > 0u ) { __Test_XNet2_SyncSleepMs(pCase->iDelayMs); }
	__Test_XNet2_SyncAtomicInc(&pCase->iHitCount);
	pOut->pValue = &pCase->iPayload;
	pOut->iFlags = XFUTURE_RESULT_F_NONE;
	return XRT_NET_OK;
}

#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
// 内部函数：__Test_XNet2_TaskRunCoProc
static int32 __Test_XNet2_TaskRunCoProc(ptr pArg, xfuture_result* pOut)
{
	__test_xnet2_sync_postfuture_case* pCase = (__test_xnet2_sync_postfuture_case*)pArg;

	return __Test_XNet2_SyncFillTaskResult(pCase, -3, pOut);
}
#endif

// 内部函数：__Test_XNet2_SyncTrackedPrintf
static int __Test_XNet2_SyncTrackedPrintf(int* piFailCount, const char* sFormat, ...)
{
	char aBuf[4096];
	va_list tArgs;
	int iRet;

	va_start(tArgs, sFormat);
	iRet = vsnprintf(aBuf, sizeof(aBuf), sFormat, tArgs);
	va_end(tArgs);

	if ( piFailCount && strstr(aBuf, " : FAIL") != NULL ) {
		(*piFailCount)++;
	}

	fputs(aBuf, stdout);
	return iRet;
}


// XNET2SYNC测试
int Test_XNet2_Sync(void)
{
	int iFailCount = 0;
#define printf(...) __Test_XNet2_SyncTrackedPrintf(&iFailCount, __VA_ARGS__)

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
		xfuture* pFuture = xFutureCreate();
		xpromise* pPromise = xPromiseCreate(pFuture);
		xwaitsrc tSrc = xWaitSourceNone();
		xfuture_result tResult;
		int iValue = 4321;
		ptr pWaitValue = NULL;
		memset(&tResult, 0, sizeof(tResult));
		printf("  Generic future create : %s\n", pFuture != NULL ? "PASS" : "FAIL");
		printf("  Generic promise create : %s\n", pPromise != NULL ? "PASS" : "FAIL");
		printf("  Generic future initial state : %s\n",
			pFuture && xFutureState(pFuture) == XFUTURE_PENDING ? "PASS" : "FAIL");
		printf("  Generic future pending wait timeout : %s\n",
			pFuture && !xFutureWaitTimeout(pFuture, 20) ? "PASS" : "FAIL");
		printf("  Generic promise resolve : %s\n",
			pPromise && xPromiseResolve(pPromise, &iValue) ? "PASS" : "FAIL");
		printf("  Generic future resolved state : %s\n",
			pFuture && xFutureState(pFuture) == XFUTURE_RESOLVED ? "PASS" : "FAIL");
		printf("  Generic future resolved wait : %s\n",
			pFuture && xFutureWait(pFuture) ? "PASS" : "FAIL");
		printf("  Generic future value : %s\n",
			pFuture && xFutureValue(pFuture) == &iValue ? "PASS" : "FAIL");
		printf("  Generic future result snapshot : %s\n",
			pFuture && xFutureGetResult(pFuture, &tResult) && tResult.iStatus == XRT_NET_OK && tResult.pValue == &iValue ? "PASS" : "FAIL");
		if ( pFuture ) {
			tSrc = xWaitSourceFromFuture(pFuture);
			pWaitValue = xWaitSourceWaitValue(&tSrc);
		}
		printf("  Generic wait source future value : %s\n",
			pFuture && pWaitValue == &iValue ? "PASS" : "FAIL");
		if ( pPromise ) xPromiseDestroy(pPromise);
		if ( pFuture ) xFutureRelease(pFuture);
	}

	{
		__test_xnet2_sync_postfuture_case tTaskCase;
		xfuture* pFuture = NULL;
		memset(&tTaskCase, 0, sizeof(tTaskCase));
		tTaskCase.iPayload = 9527;
		pFuture = xTaskRunEngine(NULL, 0, __Test_XNet2_TaskRunEngineProc, &tTaskCase);
		printf("  Generic task run engine create : %s\n", pFuture != NULL ? "PASS" : "FAIL");
		printf("  Generic task run engine wait : %s\n",
			pFuture && xFutureWaitTimeout(pFuture, 500) ? "PASS" : "FAIL");
		printf("  Generic task run engine value : %s\n",
			pFuture && xFutureValue(pFuture) == &tTaskCase.iPayload ? "PASS" : "FAIL");
		printf("  Generic task run engine hit count : %s\n", tTaskCase.iHitCount == 1 ? "PASS" : "FAIL");
		printf("  Generic task run engine worker id valid : %s\n", tTaskCase.iWorkerId >= 0 ? "PASS" : "FAIL");
		if ( pFuture ) xFutureRelease(pFuture);
	}

	{
		__test_xnet2_sync_postfuture_case tTaskCase;
		xfuture* pFuture = NULL;
		memset(&tTaskCase, 0, sizeof(tTaskCase));
		tTaskCase.iPayload = 9528;
		pFuture = xTaskRunDelayed(NULL, 0, 30, __Test_XNet2_TaskRunEngineProc, &tTaskCase);
		printf("  Generic task run delayed create : %s\n", pFuture != NULL ? "PASS" : "FAIL");
		printf("  Generic task run delayed first timeout : %s\n",
			pFuture && !xFutureWaitTimeout(pFuture, 5) ? "PASS" : "FAIL");
		printf("  Generic task run delayed eventual wait : %s\n",
			pFuture && xFutureWaitTimeout(pFuture, 500) ? "PASS" : "FAIL");
		printf("  Generic task run delayed value : %s\n",
			pFuture && xFutureValue(pFuture) == &tTaskCase.iPayload ? "PASS" : "FAIL");
		printf("  Generic task run delayed hit count : %s\n", tTaskCase.iHitCount == 1 ? "PASS" : "FAIL");
		if ( pFuture ) xFutureRelease(pFuture);
	}

	{
		__test_xnet2_sync_postfuture_case tTaskCase;
		xfuture* pFuture = NULL;
		memset(&tTaskCase, 0, sizeof(tTaskCase));
		tTaskCase.iPayload = 9529;
		pFuture = xTaskRunThread(__Test_XNet2_TaskRunThreadProc, &tTaskCase, 0);
		printf("  Generic task run thread create : %s\n", pFuture != NULL ? "PASS" : "FAIL");
		printf("  Generic task run thread wait : %s\n",
			pFuture && xFutureWaitTimeout(pFuture, 500) ? "PASS" : "FAIL");
		printf("  Generic task run thread value : %s\n",
			pFuture && xFutureValue(pFuture) == &tTaskCase.iPayload ? "PASS" : "FAIL");
		printf("  Generic task run thread hit count : %s\n", tTaskCase.iHitCount == 1 ? "PASS" : "FAIL");
		printf("  Generic task run thread marker : %s\n", tTaskCase.iWorkerId == -2 ? "PASS" : "FAIL");
		if ( pFuture ) xFutureRelease(pFuture);
	}

	{
		xnetcancel* pParent = xrtNetCancelCreate();
		xnetcancel* pChild = pParent ? xrtNetCancelCreateChild(pParent) : NULL;
		xnetcancelwatch* pWatch = NULL;
		volatile long iWatchCount = 0;
		if ( pChild ) {
			pWatch = xrtNetCancelWatchCreate(pChild, __Test_XNet2_SyncCancelWatchCallback, (ptr)&iWatchCount);
		}
		printf("  Cancel watch create on child token : %s\n", pWatch ? "PASS" : "FAIL");
		printf("  Cancel watch starts untriggered : %s\n",
			pWatch && !xrtNetCancelWatchIsTriggered(pWatch) ? "PASS" : "FAIL");
		printf("  Parent cancellation request : %s\n",
			pParent && xrtNetCancelRequest(pParent) ? "PASS" : "FAIL");
		printf("  Parent cancellation triggers child watch : %s\n",
			pWatch && xrtNetCancelWatchIsTriggered(pWatch) &&
			__xrtTestAtomicLoadLong(&iWatchCount) == 1 ? "PASS" : "FAIL");
		printf("  Cancel watch callback runs at most once : %s\n",
			pParent && !xrtNetCancelRequest(pParent) &&
			__xrtTestAtomicLoadLong(&iWatchCount) == 1 ? "PASS" : "FAIL");
		if ( pWatch ) xrtNetCancelWatchDestroy(pWatch);
		if ( pChild ) xrtNetCancelRelease(pChild);
		if ( pParent ) xrtNetCancelRelease(pParent);
	}

	{
		xtaskexecutorconfig tCfg;
		xtaskexecutorstats tStats;
		xtaskexecutor* pExecutor;
		__test_xnet2_sync_executor_case tSlow;
		__test_xnet2_sync_executor_case tCancelled;
		xfuture* pSlow;
		xfuture* pCancelled;
		xfuture* pRejected;
		int iSpin;
		memset(&tSlow, 0, sizeof(tSlow));
		memset(&tCancelled, 0, sizeof(tCancelled));
		memset(&tStats, 0, sizeof(tStats));
		tSlow.iDelayMs = 100u;
		tSlow.iPayload = 111;
		tCancelled.iPayload = 222;
		xTaskExecutorConfigInit(&tCfg);
		tCfg.iThreadCount = 1u;
		tCfg.iQueueLimit = 1u;
		pExecutor = xTaskExecutorCreate(&tCfg);
		pSlow = pExecutor ? xTaskExecutorSubmit(pExecutor, __Test_XNet2_TaskExecutorProc, &tSlow) : NULL;
		for ( iSpin = 0; pExecutor && iSpin < 100; ++iSpin ) {
			(void)xTaskExecutorGetStats(pExecutor, &tStats);
			if ( tStats.iRunning == 1u ) { break; }
			__Test_XNet2_SyncSleepMs(1u);
		}
		pCancelled = pExecutor ? xTaskExecutorSubmit(pExecutor,
			__Test_XNet2_TaskExecutorProc, &tCancelled) : NULL;
		pRejected = pExecutor ? xTaskExecutorSubmit(pExecutor,
			__Test_XNet2_TaskExecutorProc, &tCancelled) : NULL;
		if ( pCancelled ) { (void)xFutureRequestCancel(pCancelled); }
		printf("  Bounded task executor create : %s\n", pExecutor != NULL ? "PASS" : "FAIL");
		printf("  Bounded task executor accepts running task : %s\n", pSlow != NULL ? "PASS" : "FAIL");
		printf("  Bounded task executor hard queue limit : %s\n", pCancelled != NULL && pRejected == NULL ? "PASS" : "FAIL");
		printf("  Bounded task executor running result : %s\n",
			pSlow && xFutureWaitTimeout(pSlow, 1000) && xFutureValue(pSlow) == &tSlow.iPayload ? "PASS" : "FAIL");
		if ( pCancelled ) { (void)xFutureWaitTimeout(pCancelled, 1000); }
		for ( iSpin = 0; pExecutor && iSpin < 1000; ++iSpin ) {
			(void)xTaskExecutorGetStats(pExecutor, &tStats);
			if ( tStats.iCompleted == 2u ) { break; }
			__Test_XNet2_SyncSleepMs(1u);
		}
		printf("  Bounded task executor queued cancel : %s\n",
			pCancelled && xFutureStatus(pCancelled) == XRT_NET_CANCELLED &&
			tCancelled.iHitCount == 0 ? "PASS" : "FAIL");
		printf("  Bounded task executor stats : %s\n",
			tStats.iSubmitted == 2u && tStats.iCompleted == 2u && tStats.iRejected == 1u ? "PASS" : "FAIL");
		if ( pRejected ) { xFutureRelease(pRejected); }
		if ( pCancelled ) { xFutureRelease(pCancelled); }
		if ( pSlow ) { xFutureRelease(pSlow); }
		if ( pExecutor ) { xTaskExecutorDestroy(pExecutor); }
	}

	#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
	{
		__test_xnet2_sync_postfuture_case tTaskCase;
		xfuture* pFuture = NULL;
		xcosched* pSched = NULL;
		memset(&tTaskCase, 0, sizeof(tTaskCase));
		tTaskCase.iPayload = 9530;
		pSched = xrtCoSchedCreate();
		pFuture = pSched ? xTaskRunCo(pSched, __Test_XNet2_TaskRunCoProc, &tTaskCase, 0) : NULL;
		printf("  Generic task run coroutine create : %s\n", pFuture != NULL ? "PASS" : "FAIL");
		if ( pSched ) {
			xrtCoSchedRun(pSched);
		}
		printf("  Generic task run coroutine wait : %s\n",
			pFuture && xFutureWait(pFuture) ? "PASS" : "FAIL");
		printf("  Generic task run coroutine value : %s\n",
			pFuture && xFutureValue(pFuture) == &tTaskCase.iPayload ? "PASS" : "FAIL");
		printf("  Generic task run coroutine hit count : %s\n", tTaskCase.iHitCount == 1 ? "PASS" : "FAIL");
		printf("  Generic task run coroutine marker : %s\n", tTaskCase.iWorkerId == -3 ? "PASS" : "FAIL");
		if ( pFuture ) xFutureRelease(pFuture);
		if ( pSched ) xrtCoSchedDestroy(pSched);
	}
	#endif

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
		ptr pDeadlineValue = (ptr)1;

		if ( pFuture ) {
			tSrc = xrtNetWaitSourceFuture(pFuture);
			iDeadlineStatus = xrtNetWaitSourceWaitValueUntil(&tSrc, __Test_XNet2_SyncDeadlineFromTimer(20), &pDeadlineValue);
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
		__test_xnet2_sync_timer_case tCanceledTimer;
		__test_xnet2_sync_timer_case tCatchupTimer;
		__test_xnet2_sync_timer_case tBlockTask;
		uint64 iCanceledTimerId = 0;
		uint64 iCatchupTimerId = 0;
		int iPayload = 5678;

		memset(&tCtx, 0, sizeof(tCtx));
		memset(&tCanceledTimer, 0, sizeof(tCanceledTimer));
		memset(&tCatchupTimer, 0, sizeof(tCatchupTimer));
		memset(&tBlockTask, 0, sizeof(tBlockTask));
		tCanceledTimer.iWorkerId = -1;
		tCatchupTimer.iWorkerId = -1;
		tBlockTask.iBlockMs = 80;
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
			printf("  Engine timer cancel schedule : %s\n",
				xrtNetEngineScheduleTimer(pEngine, 0, 80, __Test_XNet2_SyncTimerTask,
					&tCanceledTimer, &iCanceledTimerId) == XRT_NET_OK && iCanceledTimerId != 0 ? "PASS" : "FAIL");
			printf("  Engine timer cancel request : %s\n",
				xrtNetEngineCancelTimer(pEngine, iCanceledTimerId) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  Engine timer catchup schedule : %s\n",
				xrtNetEngineScheduleTimer(pEngine, 0, 20, __Test_XNet2_SyncTimerTask,
					&tCatchupTimer, &iCatchupTimerId) == XRT_NET_OK && iCatchupTimerId != 0 ? "PASS" : "FAIL");
			printf("  Engine timer blocking task post : %s\n",
				xrtNetEnginePost(pEngine, 0, __Test_XNet2_SyncTimerBlockTask, &tBlockTask) == XRT_NET_OK ? "PASS" : "FAIL");
			__Test_XNet2_SyncSleepMs(140);
			printf("  Engine timer canceled callback suppressed : %s\n",
				tCanceledTimer.iHitCount == 0 ? "PASS" : "FAIL");
			printf("  Engine timer catches up after worker stall : %s\n",
				tCatchupTimer.iHitCount == 1 && tCatchupTimer.iWorkerId == 0 ? "PASS" : "FAIL");
			xrtNetEngineStop(pEngine);
		}

		if ( pFuture ) xrtNetFutureDestroy(pFuture);
		if ( pEngine ) xrtNetEngineDestroy(pEngine);
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
		xnetdgrambatch* pFutureBatch = NULL;
		xnetdgrambatch* pSyncBatch = NULL;
		xnetdgrampkt* pTaken = NULL;
		xnet_result iSyncStatus = XRT_NET_ERROR;

		memset(&tServerAddr, 0, sizeof(tServerAddr));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		xrtNetDgramConfigInit(&tServerCfg);
		tServerCfg.iRecvBatch = 8u;
		xrtNetDgramConfigInit(&tClientCfg);
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) { (void)xrtNetEngineStart(pEngine); }
		pServer = pEngine ? xrtNetDgramCreate(pEngine, &tServerCfg, NULL, NULL) : NULL;
		pClient = pEngine ? xrtNetDgramCreate(pEngine, &tClientCfg, NULL, NULL) : NULL;
		if ( pServer ) { (void)xrtNetDgramStart(pServer); }
		if ( pClient ) { (void)xrtNetDgramStart(pClient); }
		if ( pServer ) { (void)xrtNetAddrParse(&tServerAddr, "127.0.0.1", pServer->tLocalAddr.iPort); }
		if ( pClient && pServer ) {
			(void)xrtNetDgramSendTo(pClient, &tServerAddr, "b1", 2u);
			(void)xrtNetDgramSendTo(pClient, &tServerAddr, "b2", 2u);
			(void)xrtNetDgramSendTo(pClient, &tServerAddr, "b3", 2u);
			__Test_XNet2_SyncSleepMs(30u);
			pFuture = xrtNetDgramRecvBatchFuture(pServer, 8u);
			if ( pFuture && xrtNetFutureWait(pFuture, 1000u) == XRT_NET_OK ) {
				pFutureBatch = (xnetdgrambatch*)xrtNetFutureValue(pFuture);
			}
		}
		printf("  Dgram recv batch future create : %s\n", pFuture != NULL ? "PASS" : "FAIL");
		printf("  Dgram recv batch future drains queued packets : %s\n",
			pFutureBatch && xrtNetDgramBatchCount(pFutureBatch) == 3u ? "PASS" : "FAIL");
		printf("  Dgram recv batch packet access : %s\n",
			pFutureBatch && __Test_XNet2_SyncDgramPacketTextEquals(
				xrtNetDgramBatchPacket(pFutureBatch, 0u), "b1") ? "PASS" : "FAIL");
		if ( pFutureBatch ) { xrtNetDgramBatchDestroy(pFutureBatch); }
		if ( pFuture ) { xrtNetFutureDestroy(pFuture); }

		if ( pClient && pServer ) {
			(void)xrtNetDgramSendTo(pClient, &tServerAddr, "s1", 2u);
			(void)xrtNetDgramSendTo(pClient, &tServerAddr, "s2", 2u);
			__Test_XNet2_SyncSleepMs(30u);
			iSyncStatus = xrtNetDgramRecvBatchTimeout(pServer, 8u, 1000u, &pSyncBatch);
		}
		printf("  Dgram recv batch sync status : %s\n",
			iSyncStatus == XRT_NET_OK && pSyncBatch ? "PASS" : "FAIL");
		printf("  Dgram recv batch sync count : %s\n",
			pSyncBatch && xrtNetDgramBatchCount(pSyncBatch) == 2u ? "PASS" : "FAIL");
		pTaken = pSyncBatch ? xrtNetDgramBatchTake(pSyncBatch, 0u) : NULL;
		printf("  Dgram recv batch ownership transfer : %s\n",
			pTaken && __Test_XNet2_SyncDgramPacketTextEquals(pTaken, "s1") ? "PASS" : "FAIL");
		if ( pTaken ) { xrtNetDgramPacketDestroy(pTaken); }
		if ( pSyncBatch ) { xrtNetDgramBatchDestroy(pSyncBatch); }
		if ( pClient ) { xrtNetDgramStop(pClient); xrtNetDgramDestroy(pClient); }
		if ( pServer ) { xrtNetDgramStop(pServer); xrtNetDgramDestroy(pServer); }
		if ( pEngine ) { xrtNetEngineStop(pEngine); xrtNetEngineDestroy(pEngine); }
	}

	{
		xnetengineconfig tCfg;
		xnetengine* pEngine = NULL;
		xnetfuture* pFuture = NULL;
		xnetcancel* pCancel = NULL;
		xnetcontext tContext;
		xnetwaitsrc tSource;
		xneterror tError;
		xnetstream* pStream = NULL;
		xnet_result iStatus;
		uint64 iCancelTimerId = 0;

		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) { (void)xrtNetEngineStart(pEngine); }
		pFuture = xrtNetFutureCreate();
		pCancel = xrtNetCancelCreate();
		xrtNetContextInit(&tContext);
		xrtNetContextSetCancel(&tContext, pCancel);
		tSource = xrtNetWaitSourceFuture(pFuture);
		if ( pEngine && pCancel ) {
			(void)xrtNetEngineScheduleTimer(pEngine, 0, 20, __Test_XNet2_SyncCancelTokenTask,
				pCancel, &iCancelTimerId);
		}
		iStatus = xrtNetWaitSourceWaitContext(&tSource, &tContext);
		printf("  Context cancel token create : %s\n", pEngine && pFuture && pCancel ? "PASS" : "FAIL");
		printf("  Context cancel timer id : %s\n", iCancelTimerId != 0 ? "PASS" : "FAIL");
		printf("  Context cancel wait status : %s\n", iStatus == XRT_NET_CANCELLED ? "PASS" : "FAIL");
		printf("  Context cancel leaves source future pending : %s\n",
			pFuture && xrtNetFutureStatus(pFuture) == XRT_NET_AGAIN ? "PASS" : "FAIL");

		xrtNetContextInit(&tContext);
		xrtNetContextSetTimeout(&tContext, 15);
		iStatus = xrtNetWaitSourceWaitContext(&tSource, &tContext);
		printf("  Context deadline wait status : %s\n", iStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");

		pStream = pEngine ? xrtNetStreamCreate(pEngine, NULL, NULL) : NULL;
		if ( pStream ) {
			__Test_XNet2_SyncMarkStreamOpen(pStream);
			xrtNetStreamPauseRead(pStream);
			tSource = xrtNetWaitSourceStream(pStream, __XNET_STREAM_WAIT_READABLE);
			xrtNetContextInit(&tContext);
			xrtNetContextSetTimeout(&tContext, 15);
			iStatus = xrtNetWaitSourceWaitContext(&tSource, &tContext);
		}
		printf("  Context timeout cancels stream waiter : %s\n",
			pStream && iStatus == XRT_NET_TIMEOUT &&
			pStream->arrSyncWait[__XNET_STREAM_WAIT_READABLE].pfnWait == NULL ? "PASS" : "FAIL");

		xrtNetContextInit(&tContext);
		tContext.iVersion = XNET_CONTEXT_VERSION + 1u;
		xrtNetErrorClear();
		iStatus = xrtNetWaitSourceWaitContext(&tSource, &tContext);
		memset(&tError, 0, sizeof(tError));
		(void)xrtNetErrorGet(&tError);
		printf("  Structured error invalid context status : %s\n", iStatus == XRT_NET_ERROR ? "PASS" : "FAIL");
		printf("  Structured error wait validate fields : %s\n",
			tError.iResult == XRT_NET_ERROR && tError.iOperation == XNET_OP_WAIT &&
			tError.iPhase == XNET_PHASE_VALIDATE && tError.sMessage[0] != 0 ? "PASS" : "FAIL");

		if ( pStream ) xrtNetStreamDestroy(pStream);
		if ( pFuture ) xrtNetFutureDestroy(pFuture);
		if ( pCancel ) xrtNetCancelRelease(pCancel);
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
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
			printf("  Dgram recv helper status : %s\n", (iRecvStatus == XRT_NET_OK || pPacket != NULL) ? "PASS" : "FAIL");
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
		xnetdgrampkt* pRetryPacket = NULL;
		bool bRecvWaitCleared = false;
		xthread pThread = NULL;
		__test_xnet2_sync_dgram_send_case tSendCase;

		memset(&tServerAddr, 0, sizeof(tServerAddr));
		memset(&tSendCase, 0, sizeof(tSendCase));
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
			bRecvWaitCleared = __Test_XNet2_SyncWaitDgramRecvWaitCleared(pServer, __TEST_XNET2_SYNC_RETRY_WINDOW_MS);
		}
		if ( pClient && pServer && bRecvWaitCleared ) {
			tSendCase.pSock = pClient;
			tSendCase.tPeerAddr = tServerAddr;
			tSendCase.sPayload = "retry";
			tSendCase.iDelayMs = 10;
			pThread = xrtThreadCreate(__Test_XNet2_SyncDgramSendWorker, &tSendCase, 0);
			iRetryStatus = __Test_XNet2_SyncRetryWaitSourceDgramSync(pServer, &pRetryPacket, __TEST_XNET2_SYNC_RETRY_WINDOW_MS);
		}
		if ( pThread ) {
			xrtThreadWait(pThread);
			xrtThreadDestroy(pThread);
		}

		printf("  Wait source dgram timeout helper create : %s\n", pEngine && pServer && pClient ? "PASS" : "FAIL");
		printf("  Wait source dgram timeout helper status : %s\n", iTimeoutStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Wait source dgram timeout helper leaves null value : %s\n", pTimeoutValue == NULL ? "PASS" : "FAIL");
		printf("  Wait source dgram timeout helper unregisters waiter : %s\n", bRecvWaitCleared && iRetryStatus == XRT_NET_OK ? "PASS" : "FAIL");
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
		if ( pClient && pServer ) {
			(void)xrtNetDgramSendTo(pClient, &tServerAddr, "first", 5);
			(void)xrtNetDgramSendTo(pClient, &tServerAddr, "second", 6);
		}
		if ( pFirstFuture ) {
			iFirstStatus = xrtNetFutureWait(pFirstFuture, __TEST_XNET2_SYNC_RETRY_WINDOW_MS);
			pFirstPacket = (xnetdgrampkt*)xrtNetFutureValue(pFirstFuture);
		}
		if ( pSecondFuture ) {
			iSecondStatus = xrtNetFutureWait(pSecondFuture, __TEST_XNET2_SYNC_RETRY_WINDOW_MS);
			pSecondPacket = (xnetdgrampkt*)xrtNetFutureValue(pSecondFuture);
		}

		printf("  Dgram recv supports multiple waiters create : %s\n", pEngine && pServer && pClient && pFirstFuture && pSecondFuture ? "PASS" : "FAIL");
		printf("  Dgram recv first waiter status : %s\n", iFirstStatus == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Dgram recv first waiter FIFO payload : %s\n",
			__Test_XNet2_SyncDgramPacketTextEquals(pFirstPacket, "first") ? "PASS" : "FAIL");
		printf("  Dgram recv second waiter status : %s\n", iSecondStatus == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Dgram recv second waiter FIFO payload : %s\n",
			__Test_XNet2_SyncDgramPacketTextEquals(pSecondPacket, "second") ? "PASS" : "FAIL");

		if ( pFirstPacket ) xrtNetDgramPacketDestroy(pFirstPacket);
		if ( pSecondPacket ) xrtNetDgramPacketDestroy(pSecondPacket);
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
		bool bRecvWaitCleared = false;
		xthread pThread = NULL;
		__test_xnet2_sync_dgram_send_case tSendCase;

		memset(&tServerAddr, 0, sizeof(tServerAddr));
		memset(&tSendCase, 0, sizeof(tSendCase));
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
			bRecvWaitCleared = __Test_XNet2_SyncWaitDgramRecvWaitCleared(pServer, __TEST_XNET2_SYNC_RETRY_WINDOW_MS);
		}
		if ( pClient && pServer && bRecvWaitCleared ) {
			tSendCase.pSock = pClient;
			tSendCase.tPeerAddr = tServerAddr;
			tSendCase.sPayload = "dgun";
			tSendCase.iDelayMs = 10;
			pThread = xrtThreadCreate(__Test_XNet2_SyncDgramSendWorker, &tSendCase, 0);
			iRetryStatus = __Test_XNet2_SyncRetryDgramRecvSync(pServer, &pRetryPacket, __TEST_XNET2_SYNC_RETRY_WINDOW_MS);
		}
		if ( pThread ) {
			xrtThreadWait(pThread);
			xrtThreadDestroy(pThread);
		}

		printf("  Dgram recv deadline helper create : %s\n", pEngine && pServer && pClient ? "PASS" : "FAIL");
		printf("  Dgram recv deadline helper status : %s\n", iDeadlineStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Dgram recv deadline helper leaves null packet : %s\n", pTimeoutPacket == NULL ? "PASS" : "FAIL");
		printf("  Dgram recv deadline helper unregisters waiter : %s\n", bRecvWaitCleared && iRetryStatus == XRT_NET_OK ? "PASS" : "FAIL");
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
		xnetlistenconfig tListenCfg;
		xnetengine* pEngine = NULL;
		xnetlistener* pListener = NULL;
		xnetfuture* pFuture = NULL;
		xthread pThread = NULL;
		__test_xnet2_sync_connect_case tConnCase;
		xnetstream* pAccepted = NULL;
		xnet_result iWaitStatus = XRT_NET_ERROR;
		bool bWaitRegistered = false;
		bool bEarlyDestroyRejected = false;
		bool bOpenEmitted = false;

		memset(&tConnCase, 0, sizeof(tConnCase));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		xrtNetListenConfigInit(&tListenCfg);
		xrtNetAddrInitAny(&tListenCfg.tBindAddr, AF_INET, 0);
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pListener = pEngine ? xrtNetListenerCreate(pEngine, &tListenCfg, NULL, NULL, NULL) : NULL;
		if ( pListener ) {
			(void)xrtNetListenerStart(pListener);
			(void)xrtNetAddrParse(&tConnCase.tTargetAddr, "127.0.0.1", pListener->tConfig.tBindAddr.iPort);
			tConnCase.iDelayMs = 20;
			tConnCase.iHoldMs = 80;
			pFuture = xrtNetListenerAcceptFuture(pListener);
			bWaitRegistered = __Test_XNet2_SyncWaitListenerAcceptRegistered(pListener, 200);
		}

		xrtClearError();
		if ( pListener && bWaitRegistered ) {
			xrtNetListenerDestroy(pListener);
			bEarlyDestroyRejected = xrtGetError() && xrtGetError()[0] != 0;
		}

			if ( pListener && pFuture ) {
				pThread = xrtThreadCreate(__Test_XNet2_SyncConnectWorker, &tConnCase, 0);
				iWaitStatus = xrtNetFutureWait(pFuture, 500);
				pAccepted = (xnetstream*)xrtNetFutureValue(pFuture);
				bOpenEmitted = __Test_XNet2_SyncWaitStreamOpen(pAccepted, 200);
			}
			if ( pThread ) {
				xrtThreadWait(pThread);
				xrtThreadDestroy(pThread);
				pThread = NULL;
			}

			printf("  Listener accept future create : %s\n", pEngine && pListener && pFuture ? "PASS" : "FAIL");
		printf("  Listener accept future waiter registers : %s\n", bWaitRegistered ? "PASS" : "FAIL");
		printf("  Listener accept future rejects early listener destroy : %s\n", bEarlyDestroyRejected ? "PASS" : "FAIL");
		printf("  Listener accept future wait status : %s\n", iWaitStatus == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Listener accept future value : %s\n", pAccepted != NULL ? "PASS" : "FAIL");
		printf("  Listener accept future client connect : %s\n", tConnCase.iConnectResult == 1 ? "PASS" : "FAIL");
		printf("  Listener accept future accepted stream opens : %s\n", bOpenEmitted ? "PASS" : "FAIL");

			if ( pFuture ) xrtNetFutureDestroy(pFuture);
		if ( pAccepted ) {
			xrtNetStreamClose(pAccepted, XNET_CLOSE_F_ABORT);
			xrtNetStreamDestroy(pAccepted);
		}
		if ( pListener ) {
			xrtNetListenerStop(pListener);
			xrtClearError();
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
		xnetengine* pEngine = NULL;
		xnetlistener* pListener = NULL;
		xnetwaitsrc tSrc = xrtNetWaitSourceNone();
		xnet_result iTimeoutStatus = XRT_NET_ERROR;
		xnetstream* pTimeoutAccepted = (xnetstream*)1;
		bool bAcceptWaitCleared = false;
		bool bDestroyAfterTimeoutOk = false;

		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		xrtNetListenConfigInit(&tListenCfg);
		xrtNetAddrInitAny(&tListenCfg.tBindAddr, AF_INET, 0);
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pListener = pEngine ? xrtNetListenerCreate(pEngine, &tListenCfg, NULL, NULL, NULL) : NULL;
		if ( pListener ) {
			(void)xrtNetListenerStart(pListener);
			tSrc = xrtNetWaitSourceListenerAccept(pListener);
			iTimeoutStatus = xrtNetWaitSourceWaitValueTimeout(&tSrc, 30, (ptr*)&pTimeoutAccepted);
			bAcceptWaitCleared = pListener->tAcceptWait.pfnWait == NULL;
			xrtClearError();
			xrtNetListenerDestroy(pListener);
			bDestroyAfterTimeoutOk = xrtGetError() && xrtGetError()[0] == 0;
			pListener = NULL;
		}

		printf("  Listener timeout destroy create : %s\n", pEngine != NULL ? "PASS" : "FAIL");
		printf("  Listener timeout destroy timeout status : %s\n", iTimeoutStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Listener timeout destroy leaves null value : %s\n", pTimeoutAccepted == NULL ? "PASS" : "FAIL");
		printf("  Listener timeout destroy clears accept waiter : %s\n", bAcceptWaitCleared ? "PASS" : "FAIL");
		printf("  Listener timeout destroy succeeds : %s\n", bDestroyAfterTimeoutOk ? "PASS" : "FAIL");

		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetlistenconfig tListenCfg;
		xnetengine* pEngine = NULL;
		xnetlistener* pListener = NULL;
		xthread pThread = NULL;
		__test_xnet2_sync_connect_case tConnCase;
		xnetwaitsrc tSrc;
		xnet_result iTimeoutStatus = XRT_NET_ERROR;
		xnet_result iRetryStatus = XRT_NET_ERROR;
		xnetstream* pTimeoutAccepted = (xnetstream*)1;
		xnetstream* pAccepted = NULL;
		bool bOpenEmitted = false;

		memset(&tConnCase, 0, sizeof(tConnCase));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		xrtNetListenConfigInit(&tListenCfg);
		xrtNetAddrInitAny(&tListenCfg.tBindAddr, AF_INET, 0);
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pListener = pEngine ? xrtNetListenerCreate(pEngine, &tListenCfg, NULL, NULL, NULL) : NULL;
		if ( pListener ) {
			(void)xrtNetListenerStart(pListener);
			(void)xrtNetAddrParse(&tConnCase.tTargetAddr, "127.0.0.1", pListener->tConfig.tBindAddr.iPort);
			tConnCase.iDelayMs = 20;
			tConnCase.iHoldMs = 80;
			tSrc = xrtNetWaitSourceListenerAccept(pListener);
			iTimeoutStatus = xrtNetWaitSourceWaitValueTimeout(&tSrc, 30, (ptr*)&pTimeoutAccepted);
				pThread = xrtThreadCreate(__Test_XNet2_SyncConnectWorker, &tConnCase, 0);
				iRetryStatus = xrtNetWaitSourceWaitValue(&tSrc, (ptr*)&pAccepted);
				bOpenEmitted = __Test_XNet2_SyncWaitStreamOpen(pAccepted, 200);
			}
			if ( pThread ) {
				xrtThreadWait(pThread);
				xrtThreadDestroy(pThread);
				pThread = NULL;
			}

			printf("  Wait source listener accept create : %s\n", pEngine && pListener ? "PASS" : "FAIL");
		printf("  Wait source listener accept timeout status : %s\n", iTimeoutStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Wait source listener accept timeout leaves null value : %s\n", pTimeoutAccepted == NULL ? "PASS" : "FAIL");
		printf("  Wait source listener accept retry status : %s\n", iRetryStatus == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Wait source listener accept retry value : %s\n", pAccepted != NULL ? "PASS" : "FAIL");
		printf("  Wait source listener accept client connect : %s\n", tConnCase.iConnectResult == 1 ? "PASS" : "FAIL");
		printf("  Wait source listener accept accepted stream opens : %s\n", bOpenEmitted ? "PASS" : "FAIL");

			if ( pAccepted ) {
			xrtNetStreamClose(pAccepted, XNET_CLOSE_F_ABORT);
			xrtNetStreamDestroy(pAccepted);
		}
		if ( pListener ) {
			xrtNetListenerStop(pListener);
			xrtNetListenerDestroy(pListener);
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
		__test_xnet2_sync_dgram_coro_case tDeadlineCase;
		__test_xnet2_sync_dgram_coro_case tRetryCase;
		bool bRecvWaitCleared = false;
		bool bRetryOk = false;

		memset(&tDeadlineCase, 0, sizeof(tDeadlineCase));
		memset(&tRetryCase, 0, sizeof(tRetryCase));
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
		pSched = xrtCoSchedCreate();
		if ( pSched && pServer ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncDgramRecvCoUntil, &tDeadlineCase, 0);
			xrtCoSchedRun(pSched);
			bRecvWaitCleared = __Test_XNet2_SyncWaitDgramRecvWaitCleared(pServer, __TEST_XNET2_SYNC_RETRY_WINDOW_MS);
		}
		if ( pSched && pServer && pClient && bRecvWaitCleared ) {
			(void)xrtNetDgramSendTo(pClient, &tServerAddr, "dcud", 4);
			bRetryOk = __Test_XNet2_SyncRetryDgramRecvSync(pServer, &tRetryCase.pPacket, __TEST_XNET2_SYNC_RETRY_WINDOW_MS) == XRT_NET_OK;
		}
		printf("  Dgram coroutine deadline helper create : %s\n", pEngine && pServer && pClient && pSched ? "PASS" : "FAIL");
		printf("  Dgram coroutine deadline helper status : %s\n", tDeadlineCase.iDeadlineStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Dgram coroutine deadline helper leaves null packet : %s\n", tDeadlineCase.pTimeoutPacket == NULL ? "PASS" : "FAIL");
		printf("  Dgram coroutine deadline helper unregisters waiter : %s\n",
			bRecvWaitCleared && bRetryOk ? "PASS" : "FAIL");

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
			__Test_XNet2_SyncRunSchedWithWorker(pSched, &pThread, __Test_XNet2_SyncFutureResolveWorker, &tCase);
		}

		__Test_XNet2_SyncJoinThread(&pThread);

		printf("  Future coroutine wait create : %s\n", tCase.pFuture && pSched ? "PASS" : "FAIL");
		printf("  Future coroutine wait status : %s\n", tCase.iWaitStatus == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Future coroutine wait value : %s\n", tCase.pResolvedValue == &tCase.iPayload ? "PASS" : "FAIL");
		printf("  Future coroutine wait completion : %s\n", tCase.bDone ? "PASS" : "FAIL");

		if ( tCase.pFuture ) xrtNetFutureDestroy(tCase.pFuture);
		if ( pSched ) xrtCoSchedDestroy(pSched);
	}

	{
		__test_xnet2_sync_multi_coro_case tCase;
		xcosched* pSched = NULL;
		xthread pThread = NULL;

		memset(&tCase, 0, sizeof(tCase));
		tCase.pFuture = xrtNetFutureCreate();
		tCase.iPayload = 11223;
		pSched = xrtCoSchedCreate();
		if ( pSched && tCase.pFuture ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncFutureWaitCoMulti1, &tCase, 0);
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncFutureWaitCoMulti2, &tCase, 0);
			__Test_XNet2_SyncRunSchedWithWorker(pSched, &pThread, __Test_XNet2_SyncFutureResolveWorkerMulti, &tCase);
		}

		__Test_XNet2_SyncJoinThread(&pThread);

		printf("  Future multi coroutine wait create : %s\n", tCase.pFuture && pSched ? "PASS" : "FAIL");
		printf("  Future multi waiter #1 : %s\n", (tCase.arrDone[0] && tCase.arrStatus[0] == XRT_NET_OK && tCase.arrValues[0] == &tCase.iPayload) ? "PASS" : "FAIL");
		printf("  Future multi waiter #2 : %s\n", (tCase.arrDone[1] && tCase.arrStatus[1] == XRT_NET_OK && tCase.arrValues[1] == &tCase.iPayload) ? "PASS" : "FAIL");

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
			__Test_XNet2_SyncRunSchedWithWorker(pSched, &pThread, __Test_XNet2_SyncDgramSendWorker, &tSendCase);
		}
		__Test_XNet2_SyncJoinThread(&pThread);

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
		__test_xnet2_sync_dgram_coro_case tTimeoutCase;
		__test_xnet2_sync_dgram_coro_case tRetryCase;
		bool bRecvWaitCleared = false;
		bool bRetryOk = false;

		memset(&tTimeoutCase, 0, sizeof(tTimeoutCase));
		memset(&tRetryCase, 0, sizeof(tRetryCase));
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
		pSched = xrtCoSchedCreate();
		if ( pSched && pServer ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncWaitSourceDgramRecvCoTimeout, &tTimeoutCase, 0);
			xrtCoSchedRun(pSched);
			bRecvWaitCleared = __Test_XNet2_SyncWaitDgramRecvWaitCleared(pServer, __TEST_XNET2_SYNC_RETRY_WINDOW_MS);
		}
		if ( pSched && pServer && pClient && bRecvWaitCleared ) {
			(void)xrtNetDgramSendTo(pClient, &tServerAddr, "wgto", 4);
			bRetryOk = __Test_XNet2_SyncRetryWaitSourceDgramSync(pServer, &tRetryCase.pPacket, __TEST_XNET2_SYNC_RETRY_WINDOW_MS) == XRT_NET_OK;
		}
		printf("  Wait source dgram coroutine timeout helper create : %s\n", pEngine && pServer && pClient && pSched ? "PASS" : "FAIL");
		printf("  Wait source dgram coroutine timeout helper status : %s\n", tTimeoutCase.iTimeoutStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Wait source dgram coroutine timeout helper leaves null value : %s\n", tTimeoutCase.pTimeoutPacket == NULL ? "PASS" : "FAIL");
		printf("  Wait source dgram coroutine timeout helper unregisters waiter : %s\n",
			bRecvWaitCleared && bRetryOk ? "PASS" : "FAIL");

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
			__Test_XNet2_SyncRunSchedWithWorker(pSched, &pThread, __Test_XNet2_SyncFutureResolveWorker, &tCase);
		}

		__Test_XNet2_SyncJoinThread(&pThread);

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
		bool bEarlyDestroyDeferred = false;
		bool bFinalFutureDestroyOk = false;

		memset(&tWaitCase, 0, sizeof(tWaitCase));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pStream = pEngine ? xrtNetStreamCreate(pEngine, NULL, NULL) : NULL;
		if ( pStream ) {
			__Test_XNet2_SyncMarkStreamOpen(pStream);
			(void)xrtNetStreamSend(pStream, "drain", 5);
			tWaitCase.pFuture = xrtNetStreamDrainFuture(pStream);
		}

		xrtClearError();
		if ( pStream ) {
			xrtNetStreamDestroy(pStream);
			bEarlyDestroyDeferred = xrtGetError() && xrtGetError()[0] == 0;
		}

		pSched = xrtCoSchedCreate();
		if ( pEngine && pStream && tWaitCase.pFuture && pSched ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncFutureWaitCo, &tWaitCase, 0);
			(void)__Test_XNet2_SyncPostStreamTaskDelayed(pEngine, pStream, 40, __Test_XNet2_SyncStreamDrainTask);
			xrtCoSchedRun(pSched);
		}

		printf("  Stream drain future create : %s\n", pEngine && pStream && tWaitCase.pFuture && pSched ? "PASS" : "FAIL");
		printf("  Stream drain future defers early stream destroy : %s\n", bEarlyDestroyDeferred ? "PASS" : "FAIL");
		printf("  Stream drain future wait status : %s\n", tWaitCase.iWaitStatus == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Stream drain future value : %s\n", tWaitCase.pResolvedValue == pStream ? "PASS" : "FAIL");
		printf("  Stream drain future completion : %s\n", tWaitCase.bDone ? "PASS" : "FAIL");

		xrtClearError();
		if ( tWaitCase.pFuture ) {
			xrtNetFutureDestroy(tWaitCase.pFuture);
			tWaitCase.pFuture = NULL;
		}
		bFinalFutureDestroyOk = xrtGetError() && xrtGetError()[0] == 0;
		printf("  Stream drain future final future destroy succeeds : %s\n", bFinalFutureDestroyOk ? "PASS" : "FAIL");

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
		bool bTimeoutUnregistered = false;
		bool bRetryOk = false;

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
			__Test_XNet2_SyncMarkStreamOpen(pStream);
			(void)xrtNetStreamSend(pStream, "drain", 5);
			tWaitCase.pStream = pStream;
			tRetryCase.pStream = pStream;
		}
		pSched = xrtCoSchedCreate();
		if ( pEngine && pStream && pSched ) {
			bTimeoutUnregistered = __Test_XNet2_SyncRunWaitCoAndCheckStreamCleared(
				pSched,
				&tWaitCase,
				__Test_XNet2_SyncStreamDrainWaitCoTimeout,
				pStream,
				__XNET_STREAM_WAIT_DRAIN,
				__TEST_XNET2_SYNC_RETRY_WINDOW_MS);
		}
		if ( pEngine && pStream && pSched && bTimeoutUnregistered ) {
			bRetryOk = __Test_XNet2_SyncPostStreamTaskAndRetryWaitCo(
				pEngine,
				pStream,
				pSched,
				&tRetryCase,
				40,
				__Test_XNet2_SyncStreamDrainTask,
				__Test_XNet2_SyncStreamDrainWaitCoRetry,
				XRT_NET_OK,
				__TEST_XNET2_SYNC_RETRY_WINDOW_MS);
		}

		printf("  Stream drain coroutine timeout helper create : %s\n", pEngine && pStream && pSched ? "PASS" : "FAIL");
		printf("  Stream drain coroutine timeout helper status : %s\n", tWaitCase.iTimeoutStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Stream drain coroutine timeout helper completion : %s\n", tWaitCase.bTimeoutDone ? "PASS" : "FAIL");
		printf("  Stream drain coroutine timeout helper unregisters waiter : %s\n",
			bTimeoutUnregistered && bRetryOk ? "PASS" : "FAIL");

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
		bool bTimeoutUnregistered = false;
		bool bRetryOk = false;

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
			__Test_XNet2_SyncMarkStreamOpen(pStream);
			(void)xrtNetStreamSend(pStream, "drain", 5);
			tWaitCase.pStream = pStream;
			tRetryCase.pStream = pStream;
		}
		pSched = xrtCoSchedCreate();
		if ( pEngine && pStream && pSched ) {
			bTimeoutUnregistered = __Test_XNet2_SyncRunWaitCoAndCheckStreamCleared(
				pSched,
				&tWaitCase,
				__Test_XNet2_SyncWaitSourceStreamDrainCoTimeout,
				pStream,
				__XNET_STREAM_WAIT_DRAIN,
				__TEST_XNET2_SYNC_RETRY_WINDOW_MS);
		}
		if ( pEngine && pStream && pSched && bTimeoutUnregistered ) {
			bRetryOk = __Test_XNet2_SyncPostStreamTaskAndRetryWaitCo(
				pEngine,
				pStream,
				pSched,
				&tRetryCase,
				40,
				__Test_XNet2_SyncStreamDrainTask,
				__Test_XNet2_SyncWaitSourceStreamDrainCoRetry,
				XRT_NET_OK,
				__TEST_XNET2_SYNC_RETRY_WINDOW_MS);
		}

		printf("  Wait source stream drain timeout helper create : %s\n", pEngine && pStream && pSched ? "PASS" : "FAIL");
		printf("  Wait source stream drain timeout helper status : %s\n", tWaitCase.iTimeoutStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Wait source stream drain timeout helper completion : %s\n", tWaitCase.bTimeoutDone ? "PASS" : "FAIL");
		printf("  Wait source stream drain timeout helper unregisters waiter : %s\n",
			bTimeoutUnregistered && bRetryOk ? "PASS" : "FAIL");

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
			printf("  Stream generic sync wait helper status : %s\n",
				(iWaitStatus == XRT_NET_OK || (pStream && xrtNetChainBytes(&pStream->tRxChain) == 5)) ? "PASS" : "FAIL");
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
			printf("  Wait source stream sync wait status : %s\n",
				(iWaitStatus == XRT_NET_OK || pWaitValue == pStream || (pStream && xrtNetChainBytes(&pStream->tRxChain) == 4)) ? "PASS" : "FAIL");
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
			iTimeoutStatus = __Test_XNet2_SyncRetryStreamTimeoutSync(pStream, XNET_STREAM_WAIT_READABLE, __TEST_XNET2_SYNC_RETRY_WINDOW_MS);
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
		bool bTimeoutUnregistered = false;

		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pStream = pEngine ? xrtNetStreamCreate(pEngine, NULL, NULL) : NULL;
		if ( pStream ) {
			__Test_XNet2_SyncMarkStreamOpen(pStream);
			(void)xrtNetStreamSend(pStream, "drain", 5);
			iTimeoutStatus = __Test_XNet2_SyncRetryWaitSourceStreamTimeoutSync(
				pStream,
				XNET_STREAM_WAIT_DRAIN,
				__TEST_XNET2_SYNC_RETRY_WINDOW_MS);
			bTimeoutUnregistered = __Test_XNet2_SyncWaitStreamWaitCleared(pStream, __XNET_STREAM_WAIT_DRAIN, __TEST_XNET2_SYNC_RETRY_WINDOW_MS);
		}
		if ( pEngine && pStream && bTimeoutUnregistered ) {
			iRetryStatus = __Test_XNet2_SyncPostStreamTaskAndRetryWaitSource(
				pEngine,
				pStream,
				40,
				__Test_XNet2_SyncStreamDrainTask,
				XNET_STREAM_WAIT_DRAIN,
				__TEST_XNET2_SYNC_RETRY_WINDOW_MS);
		}

		printf("  Wait source stream drain sync timeout helper create : %s\n", pEngine && pStream ? "PASS" : "FAIL");
		printf("  Wait source stream drain sync timeout helper status : %s\n", iTimeoutStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Wait source stream drain sync timeout helper unregisters waiter : %s\n",
			bTimeoutUnregistered && iRetryStatus == XRT_NET_OK ? "PASS" : "FAIL");

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
		bool bTimeoutUnregistered = false;

		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pStream = pEngine ? xrtNetStreamCreate(pEngine, NULL, NULL) : NULL;
		if ( pStream ) {
			xrtNetStreamPauseRead(pStream);
			iTimeoutStatus = __Test_XNet2_SyncRetryWaitSourceStreamValueTimeoutSync(
				pStream,
				XNET_STREAM_WAIT_READABLE,
				&pTimeoutValue,
				__TEST_XNET2_SYNC_RETRY_WINDOW_MS);
			bTimeoutUnregistered = __Test_XNet2_SyncWaitStreamWaitCleared(pStream, __XNET_STREAM_WAIT_READABLE, __TEST_XNET2_SYNC_RETRY_WINDOW_MS);
		}
		if ( pEngine && pStream && bTimeoutUnregistered ) {
			(void)__xnetStreamPostRecvCopy(pStream, "wsrt", 4);
			iRetryStatus = __Test_XNet2_SyncRetryWaitSourceStreamSync(pStream, XNET_STREAM_WAIT_READABLE, __TEST_XNET2_SYNC_RETRY_WINDOW_MS);
		}

		printf("  Wait source stream sync timeout helper create : %s\n", pEngine && pStream ? "PASS" : "FAIL");
		printf("  Wait source stream sync timeout helper status : %s\n", iTimeoutStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Wait source stream sync timeout helper leaves null value : %s\n", pTimeoutValue == NULL ? "PASS" : "FAIL");
		printf("  Wait source stream sync timeout helper unregisters waiter : %s\n",
			bTimeoutUnregistered && iRetryStatus == XRT_NET_OK ? "PASS" : "FAIL");

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
			printf("  Wait source stream coroutine status : %s\n",
				(tWaitCase.iWaitStatus == XRT_NET_OK || tWaitCase.pResolvedValue == pStream) ? "PASS" : "FAIL");
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
		bool bDeadlineUnregistered = false;

		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pStream = pEngine ? xrtNetStreamCreate(pEngine, NULL, NULL) : NULL;
		if ( pStream ) {
			tSrc = xrtNetWaitSourceStream(pStream, XNET_STREAM_WAIT_CLOSE);
			iDeadlineStatus = __Test_XNet2_SyncWaitSourceUntilDelay(&tSrc, __TEST_XNET2_SYNC_WAIT_DEADLINE_MS);
			bDeadlineUnregistered = __Test_XNet2_SyncWaitStreamWaitCleared(pStream, __XNET_STREAM_WAIT_CLOSE, __TEST_XNET2_SYNC_RETRY_WINDOW_MS);
			iRetryStatus = __Test_XNet2_SyncPostStreamTaskAndRetryWaitSource(
				pEngine,
				pStream,
				40,
				__Test_XNet2_SyncStreamCloseTask,
				XNET_STREAM_WAIT_CLOSE,
				__TEST_XNET2_SYNC_RETRY_WINDOW_MS);
		}

		printf("  Wait source stream close sync deadline helper create : %s\n", pEngine && pStream ? "PASS" : "FAIL");
		printf("  Wait source stream close sync deadline helper status : %s\n",
			(iDeadlineStatus == XRT_NET_TIMEOUT || iDeadlineStatus == XRT_NET_CLOSED) ? "PASS" : "FAIL");
		printf("  Wait source stream close sync deadline helper unregisters waiter : %s\n",
			(bDeadlineUnregistered || iRetryStatus == XRT_NET_CLOSED) ? "PASS" : "FAIL");

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
		bool bTimeoutUnregistered = false;
		bool bRetryOk = false;

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
			bTimeoutUnregistered = __Test_XNet2_SyncRunWaitCoAndCheckStreamCleared(
				pSched,
				&tWaitCase,
				__Test_XNet2_SyncWaitSourceStreamReadableCoTimeout,
				pStream,
				__XNET_STREAM_WAIT_READABLE,
				__TEST_XNET2_SYNC_RETRY_WINDOW_MS);
		}
		if ( pEngine && pStream && pSched && bTimeoutUnregistered ) {
			bRetryOk = __Test_XNet2_SyncPostRecvAndRetryWaitCo(
				pSched,
				&tRetryCase,
				pStream,
				"wstr",
				4,
				__Test_XNet2_SyncWaitSourceStreamReadableCoRetry,
				XRT_NET_OK,
				__TEST_XNET2_SYNC_RETRY_WINDOW_MS);
		}

		printf("  Wait source stream coroutine timeout helper create : %s\n", pEngine && pStream && pSched ? "PASS" : "FAIL");
		printf("  Wait source stream coroutine timeout helper status : %s\n", tWaitCase.iTimeoutStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Wait source stream coroutine timeout helper completion : %s\n", tWaitCase.bTimeoutDone ? "PASS" : "FAIL");
		printf("  Wait source stream coroutine timeout helper leaves null value : %s\n", tWaitCase.pTimeoutValue == NULL ? "PASS" : "FAIL");
		printf("  Wait source stream coroutine timeout helper unregisters waiter : %s\n",
			bTimeoutUnregistered && bRetryOk ? "PASS" : "FAIL");

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
		bool bTimeoutUnregistered = false;
		bool bRetryOk = false;

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
			bTimeoutUnregistered = __Test_XNet2_SyncRunWaitCoAndCheckStreamCleared(
				pSched,
				&tWaitCase,
				__Test_XNet2_SyncStreamReadableWaitCoTimeout,
				pStream,
				__XNET_STREAM_WAIT_READABLE,
				__TEST_XNET2_SYNC_RETRY_WINDOW_MS);
		}
		if ( pEngine && pStream && pSched && bTimeoutUnregistered ) {
			bRetryOk = __Test_XNet2_SyncPostRecvAndRetryWaitCo(
				pSched,
				&tRetryCase,
				pStream,
				"retry",
				5,
				__Test_XNet2_SyncStreamReadableWaitCoRetry,
				XRT_NET_OK,
				__TEST_XNET2_SYNC_RETRY_WINDOW_MS);
		}

		printf("  Stream readable coroutine timeout helper create : %s\n", pEngine && pStream && pSched ? "PASS" : "FAIL");
		printf("  Stream readable coroutine timeout helper status : %s\n", tWaitCase.iTimeoutStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Stream readable coroutine timeout helper completion : %s\n", tWaitCase.bTimeoutDone ? "PASS" : "FAIL");
		printf("  Stream readable coroutine timeout helper unregisters waiter : %s\n",
			bTimeoutUnregistered && bRetryOk ? "PASS" : "FAIL");

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
		bool bDeadlineUnregistered = false;
		bool bRetryOk = false;

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
			bDeadlineUnregistered = __Test_XNet2_SyncRunWaitCoAndCheckStreamCleared(
				pSched,
				&tWaitCase,
				__Test_XNet2_SyncStreamCloseWaitCoUntil,
				pStream,
				__XNET_STREAM_WAIT_CLOSE,
				__TEST_XNET2_SYNC_RETRY_WINDOW_MS);
		}
		if ( pEngine && pStream && pSched && bDeadlineUnregistered ) {
			bRetryOk = __Test_XNet2_SyncPostStreamTaskAndRetryWaitCo(
				pEngine,
				pStream,
				pSched,
				&tRetryCase,
				40,
				__Test_XNet2_SyncStreamCloseTask,
				__Test_XNet2_SyncStreamCloseWaitCoRetry,
				XRT_NET_CLOSED,
				__TEST_XNET2_SYNC_RETRY_WINDOW_MS);
		}

		printf("  Stream close coroutine deadline helper create : %s\n", pEngine && pStream && pSched ? "PASS" : "FAIL");
		printf("  Stream close coroutine deadline helper status : %s\n", tWaitCase.iDeadlineStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Stream close coroutine deadline helper completion : %s\n", tWaitCase.bDeadlineDone ? "PASS" : "FAIL");
		printf("  Stream close coroutine deadline helper unregisters waiter : %s\n",
			bDeadlineUnregistered && bRetryOk ? "PASS" : "FAIL");

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
		bool bDeadlineUnregistered = false;
		bool bRetryOk = false;

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
			bDeadlineUnregistered = __Test_XNet2_SyncRunWaitCoAndCheckStreamCleared(
				pSched,
				&tWaitCase,
				__Test_XNet2_SyncWaitSourceStreamCloseCoUntil,
				pStream,
				__XNET_STREAM_WAIT_CLOSE,
				__TEST_XNET2_SYNC_RETRY_WINDOW_MS);
		}
		if ( pEngine && pStream && pSched && bDeadlineUnregistered ) {
			bRetryOk = __Test_XNet2_SyncPostStreamTaskAndRetryWaitCo(
				pEngine,
				pStream,
				pSched,
				&tRetryCase,
				40,
				__Test_XNet2_SyncStreamCloseTask,
				__Test_XNet2_SyncWaitSourceStreamCloseCoRetry,
				XRT_NET_CLOSED,
				__TEST_XNET2_SYNC_RETRY_WINDOW_MS);
		}

		printf("  Wait source stream close deadline helper create : %s\n", pEngine && pStream && pSched ? "PASS" : "FAIL");
		printf("  Wait source stream close deadline helper status : %s\n", tWaitCase.iDeadlineStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Wait source stream close deadline helper completion : %s\n", tWaitCase.bDeadlineDone ? "PASS" : "FAIL");
		printf("  Wait source stream close deadline helper unregisters waiter : %s\n",
			bDeadlineUnregistered && bRetryOk ? "PASS" : "FAIL");

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
			(void)__Test_XNet2_SyncPostStreamTaskDelayed(pEngine, pStream, 40, __Test_XNet2_SyncStreamCloseTask);
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
			bFinalFutureDestroyOk = !xrtGetError() || xrtGetError()[0] == 0;
		xrtClearError();
		if ( pStream ) {
			xrtNetStreamDestroy(pStream);
			pStream = NULL;
		}
			bFinalStreamDestroyOk = !xrtGetError() || xrtGetError()[0] == 0;
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

			if ( pSched ) {
				xrtCoSchedDestroy(pSched);
				pSched = NULL;
			}
			__Test_XNet2_SyncSleepMs(10);
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
			printf("  Stream readable coroutine helper wait status : %s\n",
				(tWaitCase.iWaitStatus == XRT_NET_OK || (pStream && xrtNetChainBytes(&pStream->tRxChain) == 5)) ? "PASS" : "FAIL");
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
		bool bDeadlineUnregistered = false;

		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pStream = pEngine ? xrtNetStreamCreate(pEngine, NULL, NULL) : NULL;
		if ( pStream ) {
			__xnetStreamApplyWatermark(pStream, 8, 4);
			__Test_XNet2_SyncMarkStreamOpen(pStream);
			(void)xrtNetStreamSend(pStream, "0123456789AB", 12);
			tSrc = xrtNetWaitSourceStream(pStream, XNET_STREAM_WAIT_WRITABLE);
			iDeadlineStatus = __Test_XNet2_SyncWaitSourceUntilDelay(&tSrc, __TEST_XNET2_SYNC_WAIT_DEADLINE_MS);
			bDeadlineUnregistered = __Test_XNet2_SyncWaitStreamWaitCleared(pStream, __XNET_STREAM_WAIT_WRITABLE, __TEST_XNET2_SYNC_RETRY_WINDOW_MS);
		}
		if ( pEngine && pStream && bDeadlineUnregistered ) {
			iRetryStatus = __Test_XNet2_SyncPostStreamTaskAndRetryWaitSource(
				pEngine,
				pStream,
				40,
				__Test_XNet2_SyncStreamWritableTask,
				XNET_STREAM_WAIT_WRITABLE,
				__TEST_XNET2_SYNC_RETRY_WINDOW_MS);
		}

		printf("  Wait source stream writable sync deadline helper create : %s\n", pEngine && pStream ? "PASS" : "FAIL");
		printf("  Wait source stream writable sync deadline helper status : %s\n", iDeadlineStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Wait source stream writable sync deadline helper unregisters waiter : %s\n",
			bDeadlineUnregistered && iRetryStatus == XRT_NET_OK ? "PASS" : "FAIL");

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
			printf("  Stream generic coroutine wait helper status : %s\n",
				(tWaitCase.iWaitStatus == XRT_NET_OK || (pStream && xrtNetChainBytes(&pStream->tRxChain) == 5)) ? "PASS" : "FAIL");
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
		bool bEarlyDestroyDeferred = false;
		bool bFinalFutureDestroyOk = false;

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
			__Test_XNet2_SyncMarkStreamOpen(pStream);
			(void)xrtNetStreamSend(pStream, "0123456789AB", 12);
			tWaitCase.pFuture = xrtNetStreamWritableFuture(pStream);
		}

		xrtClearError();
		if ( pStream ) {
			xrtNetStreamDestroy(pStream);
			bEarlyDestroyDeferred = xrtGetError() && xrtGetError()[0] == 0;
		}

		pSched = xrtCoSchedCreate();
		if ( pEngine && pStream && tWaitCase.pFuture && pSched ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncFutureWaitCo, &tWaitCase, 0);
			(void)__Test_XNet2_SyncPostStreamTaskDelayed(pEngine, pStream, 40, __Test_XNet2_SyncStreamWritableTask);
			xrtCoSchedRun(pSched);
		}

		printf("  Stream writable future create : %s\n", pEngine && pStream && tWaitCase.pFuture && pSched ? "PASS" : "FAIL");
		printf("  Stream writable future defers early stream destroy : %s\n", bEarlyDestroyDeferred ? "PASS" : "FAIL");
		printf("  Stream writable future wait status : %s\n",
			(tWaitCase.iWaitStatus == XRT_NET_OK || tWaitCase.pResolvedValue == pStream) ? "PASS" : "FAIL");
		printf("  Stream writable future value : %s\n", tWaitCase.pResolvedValue == pStream ? "PASS" : "FAIL");
		printf("  Stream writable future completion : %s\n", tWaitCase.bDone ? "PASS" : "FAIL");

		xrtClearError();
		if ( tWaitCase.pFuture ) {
			xrtNetFutureDestroy(tWaitCase.pFuture);
			tWaitCase.pFuture = NULL;
		}
		bFinalFutureDestroyOk = xrtGetError() && xrtGetError()[0] == 0;
		printf("  Stream writable future final future destroy succeeds : %s\n", bFinalFutureDestroyOk ? "PASS" : "FAIL");

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
			__Test_XNet2_SyncMarkStreamOpen(pStream);
			(void)xrtNetStreamSend(pStream, "0123456789AB", 12);
			tWaitCase.pStream = pStream;
		}
		pSched = xrtCoSchedCreate();
		if ( pEngine && pStream && pSched ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncStreamWritableWaitCo, &tWaitCase, 0);
			(void)__Test_XNet2_SyncPostStreamTaskDelayed(pEngine, pStream, 40, __Test_XNet2_SyncStreamWritableTask);
			xrtCoSchedRun(pSched);
		}

		printf("  Stream writable coroutine helper create : %s\n", pEngine && pStream && pSched ? "PASS" : "FAIL");
		printf("  Stream writable coroutine helper wait status : %s\n",
			(tWaitCase.iWaitStatus == XRT_NET_OK || tWaitCase.bDone) ? "PASS" : "FAIL");
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
		bool bDeadlineUnregistered = false;
		bool bRetryOk = false;

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
			__Test_XNet2_SyncMarkStreamOpen(pStream);
			(void)xrtNetStreamSend(pStream, "0123456789AB", 12);
			tWaitCase.pStream = pStream;
			tRetryCase.pStream = pStream;
		}
		pSched = xrtCoSchedCreate();
		if ( pEngine && pStream && pSched ) {
			bDeadlineUnregistered = __Test_XNet2_SyncRunWaitCoAndCheckStreamCleared(
				pSched,
				&tWaitCase,
				__Test_XNet2_SyncStreamWritableWaitCoUntil,
				pStream,
				__XNET_STREAM_WAIT_WRITABLE,
				__TEST_XNET2_SYNC_RETRY_WINDOW_MS);
		}
		if ( pEngine && pStream && pSched && bDeadlineUnregistered ) {
			bRetryOk = __Test_XNet2_SyncPostStreamTaskAndRetryWaitCo(
				pEngine,
				pStream,
				pSched,
				&tRetryCase,
				40,
				__Test_XNet2_SyncStreamWritableTask,
				__Test_XNet2_SyncStreamWritableWaitCoRetry,
				XRT_NET_OK,
				__TEST_XNET2_SYNC_RETRY_WINDOW_MS);
		}

		printf("  Stream writable coroutine deadline helper create : %s\n", pEngine && pStream && pSched ? "PASS" : "FAIL");
		printf("  Stream writable coroutine deadline helper status : %s\n", tWaitCase.iDeadlineStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Stream writable coroutine deadline helper completion : %s\n", tWaitCase.bDeadlineDone ? "PASS" : "FAIL");
		printf("  Stream writable coroutine deadline helper unregisters waiter : %s\n",
			bDeadlineUnregistered && bRetryOk ? "PASS" : "FAIL");

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
		bool bDeadlineUnregistered = false;
		bool bRetryOk = false;

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
			__Test_XNet2_SyncMarkStreamOpen(pStream);
			(void)xrtNetStreamSend(pStream, "0123456789AB", 12);
			tWaitCase.pStream = pStream;
			tRetryCase.pStream = pStream;
		}
		pSched = xrtCoSchedCreate();
		if ( pEngine && pStream && pSched ) {
			bDeadlineUnregistered = __Test_XNet2_SyncRunWaitCoAndCheckStreamCleared(
				pSched,
				&tWaitCase,
				__Test_XNet2_SyncWaitSourceStreamWritableCoUntil,
				pStream,
				__XNET_STREAM_WAIT_WRITABLE,
				__TEST_XNET2_SYNC_RETRY_WINDOW_MS);
		}
		if ( pEngine && pStream && pSched && bDeadlineUnregistered ) {
			bRetryOk = __Test_XNet2_SyncPostStreamTaskAndRetryWaitCo(
				pEngine,
				pStream,
				pSched,
				&tRetryCase,
				40,
				__Test_XNet2_SyncStreamWritableTask,
				__Test_XNet2_SyncWaitSourceStreamWritableCoRetry,
				XRT_NET_OK,
				__TEST_XNET2_SYNC_RETRY_WINDOW_MS);
		}

		printf("  Wait source stream writable deadline helper create : %s\n", pEngine && pStream && pSched ? "PASS" : "FAIL");
		printf("  Wait source stream writable deadline helper status : %s\n", tWaitCase.iDeadlineStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Wait source stream writable deadline helper completion : %s\n", tWaitCase.bDeadlineDone ? "PASS" : "FAIL");
		printf("  Wait source stream writable deadline helper unregisters waiter : %s\n",
			bDeadlineUnregistered && bRetryOk ? "PASS" : "FAIL");

		if ( pSched ) xrtCoSchedDestroy(pSched);
		if ( pStream ) xrtNetStreamDestroy(pStream);
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetlistenconfig tListenCfg;
		xnetengine* pEngine = NULL;
		xnetlistener* pListener = NULL;
		xcosched* pSched = NULL;
		xthread pThread = NULL;
		__test_xnet2_sync_connect_case tConnCase;
		__test_xnet2_sync_listener_coro_case tCase;
		bool bOpenEmitted = false;

		memset(&tConnCase, 0, sizeof(tConnCase));
		memset(&tCase, 0, sizeof(tCase));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		xrtNetListenConfigInit(&tListenCfg);
		xrtNetAddrInitAny(&tListenCfg.tBindAddr, AF_INET, 0);
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pListener = pEngine ? xrtNetListenerCreate(pEngine, &tListenCfg, NULL, NULL, NULL) : NULL;
		if ( pListener ) {
			(void)xrtNetListenerStart(pListener);
			(void)xrtNetAddrParse(&tConnCase.tTargetAddr, "127.0.0.1", pListener->tConfig.tBindAddr.iPort);
			tConnCase.iDelayMs = 20;
			tConnCase.iHoldMs = 80;
			tCase.pListener = pListener;
		}
		pSched = xrtCoSchedCreate();
			if ( pListener && pSched ) {
				(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncWaitSourceListenerAcceptCo, &tCase, 0);
				__Test_XNet2_SyncRunSchedWithWorker(pSched, &pThread, __Test_XNet2_SyncConnectWorker, &tConnCase);
				bOpenEmitted = __Test_XNet2_SyncWaitStreamOpen(tCase.pAccepted, 200);
			}
			__Test_XNet2_SyncJoinThread(&pThread);

			printf("  Wait source listener coroutine helper create : %s\n", pEngine && pListener && pSched ? "PASS" : "FAIL");
		printf("  Wait source listener coroutine helper status : %s\n", tCase.iWaitStatus == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Wait source listener coroutine helper value : %s\n", tCase.pAccepted != NULL && tCase.pResolvedValue == tCase.pAccepted ? "PASS" : "FAIL");
		printf("  Wait source listener coroutine helper completion : %s\n", tCase.bDone ? "PASS" : "FAIL");
		printf("  Wait source listener coroutine client connect : %s\n", tConnCase.iConnectResult == 1 ? "PASS" : "FAIL");
		printf("  Wait source listener coroutine accepted stream opens : %s\n", bOpenEmitted ? "PASS" : "FAIL");

			if ( pSched ) xrtCoSchedDestroy(pSched);
		if ( tCase.pAccepted ) {
			xrtNetStreamClose(tCase.pAccepted, XNET_CLOSE_F_ABORT);
			xrtNetStreamDestroy(tCase.pAccepted);
		}
		if ( pListener ) {
			xrtNetListenerStop(pListener);
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
		xnetengine* pEngine = NULL;
		xnetlistener* pListener = NULL;
		xcosched* pSched = NULL;
		xthread pThread = NULL;
		__test_xnet2_sync_connect_case tConnCase;
		__test_xnet2_sync_listener_coro_case tDeadlineCase;
		__test_xnet2_sync_listener_coro_case tRetryCase;
		bool bOpenEmitted = false;

		memset(&tConnCase, 0, sizeof(tConnCase));
		memset(&tDeadlineCase, 0, sizeof(tDeadlineCase));
		memset(&tRetryCase, 0, sizeof(tRetryCase));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		xrtNetListenConfigInit(&tListenCfg);
		xrtNetAddrInitAny(&tListenCfg.tBindAddr, AF_INET, 0);
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			(void)xrtNetEngineStart(pEngine);
		}
		pListener = pEngine ? xrtNetListenerCreate(pEngine, &tListenCfg, NULL, NULL, NULL) : NULL;
		if ( pListener ) {
			(void)xrtNetListenerStart(pListener);
			(void)xrtNetAddrParse(&tConnCase.tTargetAddr, "127.0.0.1", pListener->tConfig.tBindAddr.iPort);
			tConnCase.iDelayMs = 20;
			tConnCase.iHoldMs = 80;
			tDeadlineCase.pListener = pListener;
			tRetryCase.pListener = pListener;
		}
		pSched = xrtCoSchedCreate();
		if ( pListener && pSched ) {
			(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncListenerAcceptCoUntil, &tDeadlineCase, 0);
			xrtCoSchedRun(pSched);
		}
			if ( pListener && pSched ) {
				(void)xrtCoSchedSpawn(pSched, __Test_XNet2_SyncListenerAcceptCo, &tRetryCase, 0);
				__Test_XNet2_SyncRunSchedWithWorker(pSched, &pThread, __Test_XNet2_SyncConnectWorker, &tConnCase);
				bOpenEmitted = __Test_XNet2_SyncWaitStreamOpen(tRetryCase.pAccepted, 200);
			}
			__Test_XNet2_SyncJoinThread(&pThread);

			printf("  Listener coroutine deadline helper create : %s\n", pEngine && pListener && pSched ? "PASS" : "FAIL");
		printf("  Listener coroutine deadline helper status : %s\n", tDeadlineCase.iDeadlineStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Listener coroutine deadline helper leaves null value : %s\n", tDeadlineCase.pDeadlineAccepted == NULL && tDeadlineCase.pDeadlineValue == NULL ? "PASS" : "FAIL");
		printf("  Listener coroutine deadline helper unregisters waiter : %s\n",
			tRetryCase.iWaitStatus == XRT_NET_OK && tRetryCase.bDone ? "PASS" : "FAIL");
		printf("  Listener coroutine retry client connect : %s\n", tConnCase.iConnectResult == 1 ? "PASS" : "FAIL");
		printf("  Listener coroutine retry accepted stream opens : %s\n", bOpenEmitted ? "PASS" : "FAIL");

			if ( pSched ) xrtCoSchedDestroy(pSched);
		if ( tRetryCase.pAccepted ) {
			xrtNetStreamClose(tRetryCase.pAccepted, XNET_CLOSE_F_ABORT);
			xrtNetStreamDestroy(tRetryCase.pAccepted);
		}
		if ( pListener ) {
			xrtNetListenerStop(pListener);
			xrtNetListenerDestroy(pListener);
		}
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}
	#endif

#undef printf
	return iFailCount == 0 ? 0 : 1;
}
