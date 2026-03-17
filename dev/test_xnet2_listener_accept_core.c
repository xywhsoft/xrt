#include "test_xnet_impl_env.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <winsock2.h>
#else
	#include <time.h>
	#include <unistd.h>
#endif

typedef struct {
	xnetaddr tTargetAddr;
	uint32 iDelayMs;
	uint32 iHoldMs;
	volatile long iConnectResult;
	bool bDone;
} __listener_accept_connect_case;

typedef struct {
	xnetlistener* pListener;
	xnet_result iStatus;
	xnet_result iTimeoutStatus;
	xnetstream* pAccepted;
	xnetstream* pRetryAccepted;
	bool bDone;
	bool bTimeoutDone;
} __listener_accept_coro_case;

static void __listener_accept_sleep_ms(uint32 iDelayMs)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(iDelayMs);
	#else
		usleep((useconds_t)iDelayMs * 1000u);
	#endif
}

static int64_t __listener_accept_now_ms(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		return (int64_t)GetTickCount64();
	#else
		struct timespec tNow;
		if ( clock_gettime(CLOCK_MONOTONIC, &tNow) != 0 ) return 0;
		return ((int64_t)tNow.tv_sec * 1000LL) + ((int64_t)tNow.tv_nsec / 1000000LL);
	#endif
}

static bool __listener_accept_wait_open(xnetstream* pStream, uint32 iTimeoutMs)
{
	int64_t iDeadlineMs = __listener_accept_now_ms() + (int64_t)iTimeoutMs;
	if ( !pStream ) return false;
	while ( __listener_accept_now_ms() < iDeadlineMs ) {
		if ( (pStream->iState & __XNET_STREAM_STATE_OPEN_EMITTED) != 0 ) {
			return true;
		}
		__listener_accept_sleep_ms(5);
	}
	return (pStream->iState & __XNET_STREAM_STATE_OPEN_EMITTED) != 0;
}

static bool __listener_accept_wait_registered(xnetlistener* pListener, uint32 iTimeoutMs)
{
	int64_t iDeadlineMs = __listener_accept_now_ms() + (int64_t)iTimeoutMs;
	if ( !pListener ) return false;
	while ( __listener_accept_now_ms() < iDeadlineMs ) {
		if ( pListener->tAcceptWait.pfnWait != NULL ) {
			return true;
		}
		__listener_accept_sleep_ms(5);
	}
	return pListener->tAcceptWait.pfnWait != NULL;
}

static bool __listener_accept_error_empty(void)
{
	const char* sErr = xrtGetError();
	return sErr == NULL || sErr[0] == 0;
}

static uint32 __listener_accept_connect_worker(ptr pArg)
{
	__listener_accept_connect_case* pCase = (__listener_accept_connect_case*)pArg;
	struct sockaddr_storage tStorage;
	socklen_t iAddrLen = 0;
	xsocket hSocket = XNET_SOCKET_INVALID;
	int iRet = -1;

	if ( !pCase ) return 0;
	if ( pCase->iDelayMs > 0 ) {
		__listener_accept_sleep_ms(pCase->iDelayMs);
	}
	memset(&tStorage, 0, sizeof(tStorage));
	if ( !__xnetAddrToSockAddr(&pCase->tTargetAddr, &tStorage, &iAddrLen) ) {
		pCase->iConnectResult = -1;
		pCase->bDone = true;
		return 0;
	}

	hSocket = socket(pCase->tTargetAddr.iFamily, SOCK_STREAM, IPPROTO_TCP);
	if ( __xnetSocketIsValid(hSocket) ) {
		iRet = connect(hSocket, (struct sockaddr*)&tStorage, iAddrLen);
	}
	pCase->iConnectResult = (iRet == 0) ? 1 : -1;
	if ( iRet == 0 && pCase->iHoldMs > 0 ) {
		__listener_accept_sleep_ms(pCase->iHoldMs);
	}
	__xnetSocketCloseHandle(&hSocket);
	pCase->bDone = true;
	return 0;
}

static void __listener_accept_waitsource_co(ptr pArg)
{
	__listener_accept_coro_case* pCase = (__listener_accept_coro_case*)pArg;
	xnetwaitsrc tSrc;
	if ( !pCase ) return;
	tSrc = xrtNetWaitSourceListenerAccept(pCase->pListener);
	pCase->iStatus = xrtNetWaitSourceWaitCoValue(&tSrc, (ptr*)&pCase->pAccepted);
	pCase->bDone = true;
}

static void __listener_accept_timeout_then_retry_co(ptr pArg)
{
	__listener_accept_coro_case* pCase = (__listener_accept_coro_case*)pArg;
	int64 iNowMs;
	if ( !pCase ) return;
	iNowMs = (int64)(xrtTimer() * 1000.0);
	pCase->iTimeoutStatus = xrtNetListenerAcceptCoUntil(pCase->pListener, iNowMs + 35, &pCase->pAccepted);
	pCase->bTimeoutDone = true;
	pCase->iStatus = xrtNetListenerAcceptCo(pCase->pListener, &pCase->pRetryAccepted);
	pCase->bDone = true;
}

static void __listener_accept_timeout_only_co(ptr pArg)
{
	__listener_accept_coro_case* pCase = (__listener_accept_coro_case*)pArg;
	int64 iNowMs;
	if ( !pCase ) return;
	iNowMs = (int64)(xrtTimer() * 1000.0);
	pCase->iTimeoutStatus = xrtNetListenerAcceptCoUntil(pCase->pListener, iNowMs + 35, &pCase->pAccepted);
	pCase->bTimeoutDone = true;
}

static void __listener_accept_retry_only_co(ptr pArg)
{
	__listener_accept_coro_case* pCase = (__listener_accept_coro_case*)pArg;
	if ( !pCase ) return;
	pCase->iStatus = xrtNetListenerAcceptCo(pCase->pListener, &pCase->pRetryAccepted);
	pCase->bDone = true;
}

int main(void)
{
	xrtGlobalData* pCore;
	bool bFutureOk = false;
	bool bWaitSrcOk = false;
	bool bCoOk = false;

	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
	#if defined(_WIN32) || defined(_WIN64)
		WSADATA tWSA;
		if ( WSAStartup(MAKEWORD(2, 2), &tWSA) != 0 ) {
			return 1;
		}
	#endif

	pCore = xrtInit();
	if ( !pCore ) {
		#if defined(_WIN32) || defined(_WIN64)
			WSACleanup();
		#endif
		return 1;
	}

	printf("Listener accept focused test:\n");

	{
		xnetengineconfig tCfg;
		xnetlistenconfig tListenCfg;
		xnetengine* pEngine = NULL;
		xnetlistener* pListener = NULL;
		xthread pThread = NULL;
		__listener_accept_connect_case tConnCase;
		xnetstream* pAccepted = NULL;
		xnetfuture* pFuture = NULL;
		xnet_result iFutureStatus = XRT_NET_ERROR;
		bool bWaitRegistered = false;
		bool bEarlyDestroyRejected = false;
		bool bOpenEmitted = false;

		memset(&tConnCase, 0, sizeof(tConnCase));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		xrtNetListenConfigInit(&tListenCfg);
		xrtNetAddrInitAny(&tListenCfg.tBindAddr, AF_INET, 0);
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) (void)xrtNetEngineStart(pEngine);
		pListener = pEngine ? xrtNetListenerCreate(pEngine, &tListenCfg, NULL, NULL, NULL) : NULL;
		if ( pListener ) {
			(void)xrtNetListenerStart(pListener);
			(void)xrtNetAddrParse(&tConnCase.tTargetAddr, "127.0.0.1", pListener->tConfig.tBindAddr.iPort);
			tConnCase.iDelayMs = 20;
			tConnCase.iHoldMs = 80;
			pFuture = xrtNetListenerAcceptFuture(pListener);
			bWaitRegistered = __listener_accept_wait_registered(pListener, 200);
		}

		xrtClearError();
		if ( pListener && bWaitRegistered ) {
			xrtNetListenerDestroy(pListener);
			bEarlyDestroyRejected = !__listener_accept_error_empty();
		}

		if ( pListener && pFuture ) {
			pThread = xrtThreadCreate(__listener_accept_connect_worker, &tConnCase, 0);
			if ( pFuture ) iFutureStatus = xrtNetFutureWait(pFuture, 400);
			pAccepted = pFuture ? (xnetstream*)xrtNetFutureValue(pFuture) : NULL;
			bOpenEmitted = __listener_accept_wait_open(pAccepted, 200);
		}

		if ( pThread ) {
			xrtThreadWait(pThread);
			xrtThreadDestroy(pThread);
		}

		printf("  future waiter registers before destroy gate: %s\n", bWaitRegistered ? "PASS" : "FAIL");
		printf("  future destroy protection: %s\n", bEarlyDestroyRejected ? "PASS" : "FAIL");
		printf("  future wait status: %s\n", iFutureStatus == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  future value: %s\n", pAccepted != NULL ? "PASS" : "FAIL");
		printf("  client connect: %s\n", tConnCase.iConnectResult == 1 ? "PASS" : "FAIL");
		printf("  accepted stream open: %s\n", bOpenEmitted ? "PASS" : "FAIL");

		bFutureOk = bWaitRegistered && bEarlyDestroyRejected && iFutureStatus == XRT_NET_OK &&
			pAccepted != NULL && tConnCase.iConnectResult == 1 && bOpenEmitted;

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
		xnetstream* pAccepted = (xnetstream*)1;
		xnet_result iTimeoutStatus = XRT_NET_ERROR;
		bool bAcceptHoldActive = false;
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
			iTimeoutStatus = xrtNetWaitSourceWaitValueTimeout(&tSrc, 35, (ptr*)&pAccepted);
			bAcceptHoldActive = __xnetAtomicLoad32(&pListener->iAsyncHoldCount) > 0 &&
				pListener->tAcceptWait.pfnWait == NULL;
			xrtClearError();
			xrtNetListenerDestroy(pListener);
			bDestroyAfterTimeoutOk = __listener_accept_error_empty();
			pListener = NULL;
		}

		printf("  timeout destroy status: %s\n", iTimeoutStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  timeout destroy leaves null value: %s\n", pAccepted == NULL ? "PASS" : "FAIL");
		printf("  timeout destroy sees outstanding accept hold: %s\n", bAcceptHoldActive ? "PASS" : "FAIL");
		printf("  timeout destroy succeeds while accept op is still deferred: %s\n", bDestroyAfterTimeoutOk ? "PASS" : "FAIL");

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
		xnetwaitsrc tSrc;
		xthread pThread = NULL;
		__listener_accept_connect_case tConnCase;
		xnetstream* pAccepted = NULL;
		xnetstream* pTimeoutAccepted = (xnetstream*)1;
		xnet_result iTimeoutStatus = XRT_NET_ERROR;
		xnet_result iRetryStatus = XRT_NET_ERROR;
		bool bOpenEmitted = false;

		memset(&tConnCase, 0, sizeof(tConnCase));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		xrtNetListenConfigInit(&tListenCfg);
		xrtNetAddrInitAny(&tListenCfg.tBindAddr, AF_INET, 0);
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) (void)xrtNetEngineStart(pEngine);
		pListener = pEngine ? xrtNetListenerCreate(pEngine, &tListenCfg, NULL, NULL, NULL) : NULL;
		if ( pListener ) {
			(void)xrtNetListenerStart(pListener);
			(void)xrtNetAddrParse(&tConnCase.tTargetAddr, "127.0.0.1", pListener->tConfig.tBindAddr.iPort);
			tConnCase.iDelayMs = 20;
			tConnCase.iHoldMs = 80;
			tSrc = xrtNetWaitSourceListenerAccept(pListener);
			iTimeoutStatus = xrtNetWaitSourceWaitValueTimeout(&tSrc, 35, (ptr*)&pTimeoutAccepted);
		}

		if ( pListener ) {
			pThread = xrtThreadCreate(__listener_accept_connect_worker, &tConnCase, 0);
			iRetryStatus = xrtNetWaitSourceWaitValue(&tSrc, (ptr*)&pAccepted);
			bOpenEmitted = __listener_accept_wait_open(pAccepted, 200);
		}

		if ( pThread ) {
			xrtThreadWait(pThread);
			xrtThreadDestroy(pThread);
		}

		printf("  wait source timeout status: %s\n", iTimeoutStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  wait source timeout null value: %s\n", pTimeoutAccepted == NULL ? "PASS" : "FAIL");
		printf("  wait source retry status: %s\n", iRetryStatus == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  wait source retry value: %s\n", pAccepted != NULL ? "PASS" : "FAIL");
		printf("  wait source client connect: %s\n", tConnCase.iConnectResult == 1 ? "PASS" : "FAIL");
		printf("  wait source accepted stream open: %s\n", bOpenEmitted ? "PASS" : "FAIL");

		bWaitSrcOk = iTimeoutStatus == XRT_NET_TIMEOUT && pTimeoutAccepted == NULL &&
			iRetryStatus == XRT_NET_OK && pAccepted != NULL && tConnCase.iConnectResult == 1 && bOpenEmitted;

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
		xnetlistenconfig tListenCfg;
		xnetengine* pEngine = NULL;
		xnetlistener* pListener = NULL;
		xcosched* pSched = NULL;
		xthread pThread = NULL;
		__listener_accept_connect_case tConnCase;
		__listener_accept_coro_case tTimeoutCase;
		__listener_accept_coro_case tRetryCase;
		bool bRetryOpen = false;

		memset(&tConnCase, 0, sizeof(tConnCase));
		memset(&tTimeoutCase, 0, sizeof(tTimeoutCase));
		memset(&tRetryCase, 0, sizeof(tRetryCase));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		xrtNetListenConfigInit(&tListenCfg);
		xrtNetAddrInitAny(&tListenCfg.tBindAddr, AF_INET, 0);
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) (void)xrtNetEngineStart(pEngine);
		pListener = pEngine ? xrtNetListenerCreate(pEngine, &tListenCfg, NULL, NULL, NULL) : NULL;
		if ( pListener ) {
			(void)xrtNetListenerStart(pListener);
			(void)xrtNetAddrParse(&tConnCase.tTargetAddr, "127.0.0.1", pListener->tConfig.tBindAddr.iPort);
			tConnCase.iDelayMs = 20;
			tConnCase.iHoldMs = 80;
			tTimeoutCase.pListener = pListener;
			tRetryCase.pListener = pListener;
		}

		pSched = xrtCoSchedCreate();
		if ( pSched && pListener ) {
			(void)xrtCoSchedSpawn(pSched, __listener_accept_timeout_only_co, &tTimeoutCase, 0);
			xrtCoSchedRun(pSched);
		}
		if ( pSched && pListener ) {
			(void)xrtCoSchedSpawn(pSched, __listener_accept_retry_only_co, &tRetryCase, 0);
			pThread = xrtThreadCreate(__listener_accept_connect_worker, &tConnCase, 0);
			xrtCoSchedRun(pSched);
			bRetryOpen = __listener_accept_wait_open(tRetryCase.pRetryAccepted, 200);
		}

		if ( pThread ) {
			xrtThreadWait(pThread);
			xrtThreadDestroy(pThread);
		}

		printf("  coroutine timeout status: %s\n", tTimeoutCase.iTimeoutStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  coroutine timeout null value: %s\n", tTimeoutCase.pAccepted == NULL ? "PASS" : "FAIL");
		printf("  coroutine retry status: %s\n", tRetryCase.iStatus == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  coroutine retry value: %s\n", tRetryCase.pRetryAccepted != NULL ? "PASS" : "FAIL");
		printf("  coroutine client connect: %s\n", tConnCase.iConnectResult == 1 ? "PASS" : "FAIL");
		printf("  coroutine retry open: %s\n", bRetryOpen ? "PASS" : "FAIL");

		bCoOk = tTimeoutCase.iTimeoutStatus == XRT_NET_TIMEOUT && tTimeoutCase.pAccepted == NULL &&
			tRetryCase.iStatus == XRT_NET_OK && tRetryCase.pRetryAccepted != NULL &&
			tConnCase.iConnectResult == 1 && bRetryOpen;

		if ( pSched ) xrtCoSchedDestroy(pSched);
		if ( tRetryCase.pRetryAccepted ) {
			xrtNetStreamClose(tRetryCase.pRetryAccepted, XNET_CLOSE_F_ABORT);
			xrtNetStreamDestroy(tRetryCase.pRetryAccepted);
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

	printf("  overall future path: %s\n", bFutureOk ? "PASS" : "FAIL");
	printf("  overall wait source path: %s\n", bWaitSrcOk ? "PASS" : "FAIL");
	printf("  overall coroutine path: %s\n", bCoOk ? "PASS" : "FAIL");

	xrtUnit();
	#if defined(_WIN32) || defined(_WIN64)
		WSACleanup();
	#endif
	return (bFutureOk && bWaitSrcOk && bCoOk) ? 0 : 2;
}
