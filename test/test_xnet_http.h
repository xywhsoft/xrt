#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#else
	#include <pthread.h>
	#include <unistd.h>
#endif

#include "test_atomic_compat.h"

typedef struct {
	xnetengine* pEngine;
	xnetlistener* pListener;
	xnetstream* pAccepted;
	volatile long bRun;
	volatile long iAcceptCount;
	volatile long iOpenCount;
	volatile long iRecvCount;
	volatile long iCloseCount;
	volatile long iErrorCount;
	char aLastMethod[32];
	char aLastTarget[256];
	char aLastHost[256];
	char aLastBody[256];
	char aResponse[2048];
	uint32 iResponseLen;
	char aResponseFollowup[2048];
	uint32 iResponseFollowupLen;
	uint32 iResponseFollowupDelayMs;
#if defined(_WIN32) || defined(_WIN64)
	HANDLE hThread;
#else
	pthread_t hThread;
	bool bThreadStarted;
#endif
} __test_xhttp_server;

typedef struct {
	__test_xhttp_server* pServer;
	xnetstream* pStream;
} __test_xhttp_delayed_send_ctx;

typedef struct {
	volatile long iHeadersCount;
	volatile long iBodyCount;
	uint32 iStatusCode;
	bool bSawStreamHeader;
	bool bCancelOnBody;
	char aBody[256];
	size_t iBodyLen;
} __test_xhttp_stream_ctx;

static void __Test_XHttpServerDelayedSendTask(xnetworker* pWorker, ptr pArg);
static long __Test_XHttpAtomicInc(volatile long* pValue);


static bool __Test_XHttpStreamOnHeaders(ptr pUserData, const xhttpresponse* pResponse)
{
	__test_xhttp_stream_ctx* pCtx = (__test_xhttp_stream_ctx*)pUserData;
	if ( !pCtx || !pResponse ) { return false; }
	__Test_XHttpAtomicInc(&pCtx->iHeadersCount);
	pCtx->iStatusCode = pResponse->iStatusCode;
	pCtx->bSawStreamHeader = xrtHttpResponseHeader(pResponse, "X-Stream") != NULL;
	return true;
}


static bool __Test_XHttpStreamOnBody(ptr pUserData, const void* pData, size_t iLen)
{
	__test_xhttp_stream_ctx* pCtx = (__test_xhttp_stream_ctx*)pUserData;
	size_t iCopy;
	if ( !pCtx || (!pData && iLen > 0u) ) { return false; }
	__Test_XHttpAtomicInc(&pCtx->iBodyCount);
	iCopy = iLen;
	if ( iCopy > sizeof(pCtx->aBody) - pCtx->iBodyLen ) { iCopy = sizeof(pCtx->aBody) - pCtx->iBodyLen; }
	if ( iCopy > 0u ) {
		memcpy(pCtx->aBody + pCtx->iBodyLen, pData, iCopy);
		pCtx->iBodyLen += iCopy;
	}
	return !pCtx->bCancelOnBody;
}


// 内部函数：__Test_XHttpSleepMs
static void __Test_XHttpSleepMs(uint32 iDelayMs)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(iDelayMs);
	#else
		usleep((useconds_t)iDelayMs * 1000u);
	#endif
}


// 内部函数：__Test_XHttpAtomicInc
static long __Test_XHttpAtomicInc(volatile long* pValue)
{
	return __xrtTestAtomicAddFetchLong(pValue, 1);
}


// 内部函数：__Test_XHttpAtomicLoad
static long __Test_XHttpAtomicLoad(volatile long* pValue)
{
	return __xrtTestAtomicLoadLong(pValue);
}


// 内部函数：__Test_XHttpAtomicStore
static void __Test_XHttpAtomicStore(volatile long* pValue, long iValue)
{
	__xrtTestAtomicStoreLong(pValue, iValue);
}


// 内部函数：__Test_XHttpWaitMin
static bool __Test_XHttpWaitMin(volatile long* pValue, long iExpectMin, uint32 iTimeoutMs)
{
	uint32 iLoops = (iTimeoutMs / 10u) + 1u;
	for ( uint32 i = 0; i < iLoops; ++i ) {
		if ( __Test_XHttpAtomicLoad(pValue) >= iExpectMin ) return true;
		__Test_XHttpSleepMs(10);
	}
	return __Test_XHttpAtomicLoad(pValue) >= iExpectMin;
}


// 内部函数：__Test_XHttpFileExists
static bool __Test_XHttpFileExists(const char* sPath)
{
	FILE* pFile = fopen(sPath, "rb");
	if ( !pFile ) return false;
	fclose(pFile);
	return true;
}


// 内部函数：__Test_XHttpServerOnAccept
static bool __Test_XHttpServerOnAccept(ptr pOwner, xnetlistener* pListener, xnetstream* pStream)
{
	__test_xhttp_server* pServer = (__test_xhttp_server*)pOwner;
	(void)pListener;
	if ( !pServer ) return false;
	pServer->pAccepted = pStream;
	__Test_XHttpAtomicInc(&pServer->iAcceptCount);
	return true;
}


// 内部函数：__Test_XHttpServerOnOpen
static void __Test_XHttpServerOnOpen(ptr pOwner, xnetstream* pStream)
{
	__test_xhttp_server* pServer = (__test_xhttp_server*)pOwner;
	(void)pStream;
	if ( !pServer ) return;
	__Test_XHttpAtomicInc(&pServer->iOpenCount);
}


// 内部函数：__Test_XHttpServerOnRecv
static void __Test_XHttpServerOnRecv(ptr pOwner, xnetstream* pStream, xnetchain* pChain)
{
	__test_xhttp_server* pServer = (__test_xhttp_server*)pOwner;
	xcodecframe tFrame;
	xcodechttp1msg tMsg;
	size_t iBodyBytes;
	if ( !pServer || !pStream || !pChain ) return;
	if ( xrtCodecHttp1Parse(pChain, &tFrame, &tMsg) != XCODEC_STATUS_FRAME ) return;
	__Test_XHttpAtomicInc(&pServer->iRecvCount);
	__xhttpCopyToken(pServer->aLastMethod, sizeof(pServer->aLastMethod), tMsg.sMethod);
	__xhttpCopyToken(pServer->aLastTarget, sizeof(pServer->aLastTarget), tMsg.sTarget);
	{
		const char* sHost = xrtCodecHttp1GetHeader(&tMsg, "Host");
		if ( sHost ) __xhttpCopyToken(pServer->aLastHost, sizeof(pServer->aLastHost), sHost);
	}
	memset(pServer->aLastBody, 0, sizeof(pServer->aLastBody));
	iBodyBytes = xrtCodecHttp1BodyBytes(&tFrame);
	if ( iBodyBytes > 0u ) {
		size_t iCopy = iBodyBytes < (sizeof(pServer->aLastBody) - 1u) ? iBodyBytes : (sizeof(pServer->aLastBody) - 1u);
		(void)xrtCodecHttp1CopyBody(pChain, &tFrame, pServer->aLastBody, iCopy);
	}
	xrtCodecFrameConsume(pChain, &tFrame);
	if ( pServer->iResponseLen > 0u ) {
		(void)xrtNetStreamSend(pStream, pServer->aResponse, pServer->iResponseLen);
	}
	if ( pServer->iResponseLen > 0u &&
		pServer->iResponseFollowupLen == 0u &&
		(strstr(pServer->aResponse, "\r\nConnection: close\r\n") != NULL ||
			strstr(pServer->aResponse, "\r\nConnection: close\r\n\r\n") != NULL ||
			strstr(pServer->aResponse, "\r\nconnection: close\r\n") != NULL ||
			strstr(pServer->aResponse, "\r\nconnection: close\r\n\r\n") != NULL) ) {
		(void)xrtNetStreamClose(pStream, 0);
	}
	if ( pServer->iResponseFollowupLen > 0u ) {
		if ( pServer->iResponseFollowupDelayMs == 0u ) {
			(void)xrtNetStreamSend(pStream, pServer->aResponseFollowup, pServer->iResponseFollowupLen);
		} else {
			__test_xhttp_delayed_send_ctx* pCtx =
				(__test_xhttp_delayed_send_ctx*)XNET_ALLOC(sizeof(__test_xhttp_delayed_send_ctx));
			if ( pCtx ) {
				memset(pCtx, 0, sizeof(__test_xhttp_delayed_send_ctx));
				pCtx->pServer = pServer;
				pCtx->pStream = pStream;
				__xnetStreamAddAsyncHold(pStream);
				if ( xrtNetEnginePostDelayed(
					pServer->pEngine,
					(pStream && pStream->pWorker) ? pStream->pWorker->iId : 0u,
					pServer->iResponseFollowupDelayMs,
					__Test_XHttpServerDelayedSendTask,
					pCtx) != XRT_NET_OK ) {
					__xnetStreamReleaseAsyncHold(pStream);
					XNET_FREE(pCtx);
				}
			}
		}
	}
}


// 内部函数：__Test_XHttpServerOnClose
static void __Test_XHttpServerOnClose(ptr pOwner, xnetstream* pStream, xnet_result iReason)
{
	__test_xhttp_server* pServer = (__test_xhttp_server*)pOwner;
	(void)pStream;
	(void)iReason;
	if ( !pServer ) return;
	__Test_XHttpAtomicInc(&pServer->iCloseCount);
}


// 内部函数：__Test_XHttpServerOnError
static void __Test_XHttpServerOnError(ptr pOwner, xnetstream* pStream, int iSysErr)
{
	__test_xhttp_server* pServer = (__test_xhttp_server*)pOwner;
	(void)pStream;
	(void)iSysErr;
	if ( !pServer ) return;
	__Test_XHttpAtomicInc(&pServer->iErrorCount);
}


// 内部函数：__Test_XHttpListenerEvents
static const xnetlistenerevents* __Test_XHttpListenerEvents(void)
{
	static const xnetlistenerevents tEvents = {
		__Test_XHttpServerOnAccept,
		NULL
	};
	return &tEvents;
}


// 内部函数：__Test_XHttpStreamEvents
static const xnetstreamevents* __Test_XHttpStreamEvents(void)
{
	static const xnetstreamevents tEvents = {
		__Test_XHttpServerOnOpen,
		__Test_XHttpServerOnRecv,
		NULL,
		__Test_XHttpServerOnClose,
		__Test_XHttpServerOnError,
		NULL,
		NULL,
		NULL
	};
	return &tEvents;
}

#if defined(_WIN32) || defined(_WIN64)
// 内部函数：__Test_XHttpAcceptThread
static DWORD WINAPI __Test_XHttpAcceptThread(LPVOID pArg)
#else
// 内部函数：__Test_XHttpAcceptThread
static void* __Test_XHttpAcceptThread(void* pArg)
#endif
{
	__test_xhttp_server* pServer = (__test_xhttp_server*)pArg;
	while ( pServer && __Test_XHttpAtomicLoad(&pServer->bRun) != 0 ) {
		if ( pServer->pListener && pServer->pListener->bRunning ) {
			(void)__xnetListenerTryAcceptOne(pServer->pListener, pServer);
		}
		__Test_XHttpSleepMs(5);
	}
	#if defined(_WIN32) || defined(_WIN64)
		return 0;
	#else
		return NULL;
	#endif
}


// 内部函数：__Test_XHttpServerDelayedSendTask
static void __Test_XHttpServerDelayedSendTask(xnetworker* pWorker, ptr pArg)
{
	__test_xhttp_delayed_send_ctx* pCtx = (__test_xhttp_delayed_send_ctx*)pArg;
	(void)pWorker;
	if ( pCtx && pCtx->pServer && pCtx->pStream && pCtx->pServer->iResponseFollowupLen > 0u ) {
		(void)xrtNetStreamSend(
			pCtx->pStream,
			pCtx->pServer->aResponseFollowup,
			pCtx->pServer->iResponseFollowupLen);
	}
	if ( pCtx && pCtx->pStream ) {
		__xnetStreamReleaseAsyncHold(pCtx->pStream);
	}
	if ( pCtx ) XNET_FREE(pCtx);
}


// 内部函数：__Test_XHttpServerStartEx
static bool __Test_XHttpServerStartEx(
	__test_xhttp_server* pServer,
	const char* sResponse,
	const char* sResponseFollowup,
	uint32 iResponseFollowupDelayMs,
	const xtlsconfig* pTlsCfg)
{
	xnetengineconfig tEngineCfg;
	xnetlistenconfig tListenCfg;
	if ( !pServer ) return false;
	memset(pServer, 0, sizeof(__test_xhttp_server));
	xrtNetEngineConfigInit(&tEngineCfg);
	tEngineCfg.iWorkerCount = 1;
	pServer->pEngine = xrtNetEngineCreate(&tEngineCfg);
	if ( !pServer->pEngine ) return false;
	if ( xrtNetEngineStart(pServer->pEngine) != XRT_NET_OK ) return false;
	xrtNetListenConfigInit(&tListenCfg);
	xrtNetAddrParse(&tListenCfg.tBindAddr, "127.0.0.1", 0);
	tListenCfg.pTlsConfig = pTlsCfg;
	pServer->pListener = xrtNetListenerCreate(pServer->pEngine, &tListenCfg,
		__Test_XHttpListenerEvents(), __Test_XHttpStreamEvents(), pServer);
	if ( !pServer->pListener ) return false;
	if ( xrtNetListenerStart(pServer->pListener) != XRT_NET_OK ) return false;
	__xhttpCopyToken(pServer->aResponse, sizeof(pServer->aResponse), sResponse ? sResponse : "");
	pServer->iResponseLen = (uint32)strlen(pServer->aResponse);
	__xhttpCopyToken(
		pServer->aResponseFollowup,
		sizeof(pServer->aResponseFollowup),
		sResponseFollowup ? sResponseFollowup : "");
	pServer->iResponseFollowupLen = (uint32)strlen(pServer->aResponseFollowup);
	pServer->iResponseFollowupDelayMs = iResponseFollowupDelayMs;
	__Test_XHttpAtomicStore(&pServer->bRun, 1);
	#if defined(_WIN32) || defined(_WIN64)
		pServer->hThread = CreateThread(NULL, 0, __Test_XHttpAcceptThread, pServer, 0, NULL);
		if ( !pServer->hThread ) return false;
	#else
		if ( pthread_create(&pServer->hThread, NULL, __Test_XHttpAcceptThread, pServer) != 0 ) return false;
		pServer->bThreadStarted = true;
	#endif
	return true;
}


// 内部函数：__Test_XHttpServerStart
static bool __Test_XHttpServerStart(__test_xhttp_server* pServer, const char* sResponse, const xtlsconfig* pTlsCfg)
{
	return __Test_XHttpServerStartEx(pServer, sResponse, NULL, 0u, pTlsCfg);
}


// 内部函数：__Test_XHttpServerStop
static void __Test_XHttpServerStop(__test_xhttp_server* pServer)
{
	if ( !pServer ) return;
	__Test_XHttpAtomicStore(&pServer->bRun, 0);
	#if defined(_WIN32) || defined(_WIN64)
		if ( pServer->hThread ) {
			WaitForSingleObject(pServer->hThread, INFINITE);
			CloseHandle(pServer->hThread);
			pServer->hThread = NULL;
		}
	#else
		if ( pServer->bThreadStarted ) {
			pthread_join(pServer->hThread, NULL);
			pServer->bThreadStarted = false;
		}
	#endif
	if ( pServer->pAccepted ) {
		if ( __Test_XHttpAtomicLoad(&pServer->iCloseCount) == 0 ) {
			xrtNetStreamClose(pServer->pAccepted, XNET_CLOSE_F_ABORT);
			__Test_XHttpSleepMs(20);
		}
		xrtNetStreamDestroy(pServer->pAccepted);
		pServer->pAccepted = NULL;
	}
	if ( pServer->pListener ) {
		xrtNetListenerStop(pServer->pListener);
		xrtNetListenerDestroy(pServer->pListener);
		pServer->pListener = NULL;
	}
	if ( pServer->pEngine ) {
		xrtNetEngineStop(pServer->pEngine);
		xrtNetEngineDestroy(pServer->pEngine);
		pServer->pEngine = NULL;
	}
}


// 内部函数：__Test_XHttpPrintRealHttpsResult
static void __Test_XHttpPrintRealHttpsResult(const char* sLabel, const char* sURL, bool bConnectionClose, bool bExpectOk)
{
	xhttprequest tReq;
	xhttpresponse* pResp = NULL;
	xnet_result iStatus = XRT_NET_ERROR;
	xrtHttpRequestInit(&tReq);
	(void)xrtHttpRequestSetURL(&tReq, sURL);
	xrtHttpRequestSetVerifyPeer(&tReq, false);
	xrtHttpRequestSetTimeout(&tReq, 15000u);
	xrtHttpRequestSetIdleTimeout(&tReq, 15000u);
	if ( bConnectionClose ) {
		(void)xrtHttpRequestSetHeader(&tReq, "Connection", "close");
	}
	pResp = xrtHttpExecuteSync(NULL, &tReq, &iStatus);
	printf("  HTTP real HTTPS %s : %s (net=%d http=%u len=%u)\n",
		sLabel,
		((iStatus == XRT_NET_OK && pResp && pResp->iStatusCode >= 200u && pResp->iStatusCode < 400u) == bExpectOk) ? "PASS" : "FAIL",
		(int)iStatus,
		pResp ? (unsigned)pResp->iStatusCode : 0u,
		pResp ? (unsigned)pResp->iBodyLen : 0u);
	if ( pResp ) xrtHttpResponseDestroy(pResp);
	xrtHttpRequestUnit(&tReq);
	xrtHttpCloseIdleConnections(xrtNetSyncGetHiddenEngine());
	xrtNetSyncShutdownHiddenEngine();
}


// XNETHTTP测试
void Test_XNet_Http(void)
{
	printf("\n\n\n------------------------------------\n\n XNet HTTP Client Skeleton Test:\n\n");

	{
		xhttprequest tReq;
		char* pBuilt = NULL;
		size_t iBuiltLen = 0;
		xrtHttpRequestInit(&tReq);
		(void)xrtHttpRequestSetMethod(&tReq, "POST");
		(void)xrtHttpRequestSetURL(&tReq, "http://127.0.0.1:8080/api");
		(void)xrtHttpRequestSetHeader(&tReq, "User-Agent", "xhttp-test");
		(void)xrtHttpRequestSetBodyCopy(&tReq, "hello", 5, "text/plain");
		printf("  HTTP build request bytes : %s\n", __xhttpBuildRequestBytes(&tReq, &pBuilt, &iBuiltLen) ? "PASS" : "FAIL");
		printf("  HTTP build has request line : %s\n", pBuilt && strstr(pBuilt, "POST /api HTTP/1.1\r\n") != NULL ? "PASS" : "FAIL");
		printf("  HTTP build has host header : %s\n", pBuilt && strstr(pBuilt, "Host: 127.0.0.1:8080\r\n") != NULL ? "PASS" : "FAIL");
		printf("  HTTP build has connection keep-alive : %s\n", pBuilt && strstr(pBuilt, "Connection: keep-alive\r\n") != NULL ? "PASS" : "FAIL");
		printf("  HTTP build has content length : %s\n", pBuilt && strstr(pBuilt, "Content-Length: 5\r\n") != NULL ? "PASS" : "FAIL");
		printf("  HTTP build has body : %s\n", pBuilt && iBuiltLen >= 5 && memcmp(pBuilt + iBuiltLen - 5, "hello", 5) == 0 ? "PASS" : "FAIL");
		if ( pBuilt ) XNET_FREE(pBuilt);
		xrtHttpRequestUnit(&tReq);
	}

	{
		xhttprequest tReq;
		char* pBuilt = NULL;
		size_t iBuiltLen = 0;
		xrtHttpRequestInit(&tReq);
		(void)xrtHttpRequestSetMethod(&tReq, "POST");
		(void)xrtHttpRequestSetURL(&tReq, "http://127.0.0.1:8080/chunked");
		(void)xrtHttpRequestSetHeader(&tReq, "Transfer-Encoding", "chunked");
		(void)xrtHttpRequestSetBodyCopy(&tReq, "hello", 5, "text/plain");
		printf("  HTTP build chunked request bytes : %s\n", __xhttpBuildRequestBytes(&tReq, &pBuilt, &iBuiltLen) ? "PASS" : "FAIL");
		printf("  HTTP build chunked transfer header : %s\n", pBuilt && strstr(pBuilt, "Transfer-Encoding: chunked\r\n") != NULL ? "PASS" : "FAIL");
		printf("  HTTP build chunked omits content length : %s\n", pBuilt && strstr(pBuilt, "Content-Length:") == NULL ? "PASS" : "FAIL");
		printf("  HTTP build chunked body framing : %s\n",
			pBuilt && strstr(pBuilt, "\r\n\r\n5\r\nhello\r\n0\r\n\r\n") != NULL ? "PASS" : "FAIL");
		if ( pBuilt ) XNET_FREE(pBuilt);
		xrtHttpRequestUnit(&tReq);
	}

	{
		xhttprequest tReq;
		char* pBuilt = NULL;
		size_t iBuiltLen = 0u;
		bool bAdded = true;
		xrtHttpRequestInit(&tReq);
		(void)xrtHttpRequestSetURL(&tReq, "http://127.0.0.1:8080/many-headers");
		for ( uint32 i = 0u; i < 48u; ++i ) {
			char sName[64];
			char sValue[64];
			snprintf(sName, sizeof(sName), "X-Dynamic-%u", (unsigned)i);
			snprintf(sValue, sizeof(sValue), "value-%u", (unsigned)i);
			bAdded = bAdded && xrtHttpRequestAddHeader(&tReq, sName, sValue);
		}
		(void)xrtHttpRequestAddHeader(&tReq, "X-Remove", "a");
		(void)xrtHttpRequestAddHeader(&tReq, "X-Remove", "b");
		printf("  HTTP dynamic request headers grow : %s\n", bAdded && tReq.iHeaderCount == 50u && tReq.iHeaderCap >= 50u ? "PASS" : "FAIL");
		printf("  HTTP dynamic request duplicate remove : %s\n", xrtHttpRequestRemoveHeader(&tReq, "x-remove") == 2u && tReq.iHeaderCount == 48u ? "PASS" : "FAIL");
		printf("  HTTP dynamic request serialize : %s\n", __xhttpBuildRequestBytes(&tReq, &pBuilt, &iBuiltLen) && pBuilt && strstr(pBuilt, "X-Dynamic-47: value-47\r\n") != NULL ? "PASS" : "FAIL");
		if ( pBuilt ) { XNET_FREE(pBuilt); }
		xrtHttpRequestUnit(&tReq);
	}

	{
		xhttprequest tReq;
		xhttpdiagnostics tDiagnostics;
		xnetfuture* pFuture;
		xnet_result iStatus;
		xrtHttpRequestInit(&tReq);
		printf("  HTTP diagnostics reject invalid URL input : %s\n", !xrtHttpRequestSetURL(&tReq, "not-an-http-url") ? "PASS" : "FAIL");
		xrtHttpRequestSetDiagnostics(&tReq, &tDiagnostics);
		pFuture = xrtHttpExecuteAsync(NULL, &tReq);
		iStatus = pFuture ? xrtNetFutureWait(pFuture, 1000u) : XRT_NET_ERROR;
		printf("  HTTP invalid URL diagnostics : %s\n",
			iStatus == XRT_NET_ERROR &&
			tDiagnostics.eError == XHTTP_ERROR_INVALID_URL &&
			tDiagnostics.ePhase == XHTTP_PHASE_PREPARE &&
			tDiagnostics.iCompletedMs >= tDiagnostics.iStartedMs ? "PASS" : "FAIL");
		printf("  HTTP diagnostic stable names : %s\n",
			strcmp(xrtHttpErrorCodeName(tDiagnostics.eError), "invalid_url") == 0 &&
			strcmp(xrtHttpPhaseName(tDiagnostics.ePhase), "prepare") == 0 ? "PASS" : "FAIL");
		if ( pFuture ) { xrtNetFutureDestroy(pFuture); }
		xrtHttpRequestUnit(&tReq);
		xrtNetSyncShutdownHiddenEngine();
	}

	{
		__test_xhttp_server tServer;
		xnetengineconfig tCfg;
		xnetengine* pClientEngine = NULL;
		xhttprequest tReq;
		xnetfuture* pFuture = NULL;
		xhttpresponse* pResp = NULL;
		xnet_result iStatus = XRT_NET_ERROR;
		char sURL[256];
		printf("  HTTP plain server start : %s\n", __Test_XHttpServerStart(&tServer, "HTTP/1.1 200 OK\r\nContent-Length: 5\r\nConnection: keep-alive\r\n\r\nworld", NULL) ? "PASS" : "FAIL");
		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/hello", (unsigned)tServer.pListener->tConfig.tBindAddr.iPort);
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pClientEngine = xrtNetEngineCreate(&tCfg);
		if ( pClientEngine ) (void)xrtNetEngineStart(pClientEngine);
		xrtHttpRequestInit(&tReq);
		(void)xrtHttpRequestSetURL(&tReq, sURL);
		(void)xrtHttpRequestSetHeader(&tReq, "User-Agent", "xhttp-async");
		xhttpdiagnostics tDiagnostics;
		xrtHttpRequestSetDiagnostics(&tReq, &tDiagnostics);
		pFuture = xrtHttpExecuteAsync(pClientEngine, &tReq);
		printf("  HTTP async future create : %s\n", pFuture != NULL ? "PASS" : "FAIL");
		printf("  HTTP async future wait : %s\n", pFuture && (iStatus = xrtNetFutureWait(pFuture, 3000)) == XRT_NET_OK ? "PASS" : "FAIL");
		pResp = (pFuture && iStatus == XRT_NET_OK) ? (xhttpresponse*)xrtNetFutureValue(pFuture) : NULL;
		printf("  HTTP async response status : %s\n", pResp && pResp->iStatusCode == 200 ? "PASS" : "FAIL");
		printf("  HTTP async response body : %s\n", pResp && pResp->iBodyLen == 5 && memcmp(pResp->pBody, "world", 5) == 0 ? "PASS" : "FAIL");
		printf("  HTTP async diagnostics success : %s\n",
			tDiagnostics.eError == XHTTP_ERROR_NONE &&
			tDiagnostics.ePhase == XHTTP_PHASE_COMPLETE &&
			tDiagnostics.eTransportStatus == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  HTTP async diagnostics timing : %s\n",
			tDiagnostics.iStartedMs > 0u &&
			tDiagnostics.iCompletedMs >= tDiagnostics.iStartedMs &&
			tDiagnostics.iFirstByteMs >= tDiagnostics.iRequestSentMs &&
			tDiagnostics.iResponseBodyBytes == 5u ? "PASS" : "FAIL");
		printf("  HTTP async response carries diagnostics : %s\n",
			pResp && xrtHttpResponseDiagnostics(pResp)->eTransportStatus == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  HTTP server accepted request : %s\n", __Test_XHttpWaitMin(&tServer.iAcceptCount, 1, 1000) ? "PASS" : "FAIL");
		printf("  HTTP server saw method : %s\n", strcmp(tServer.aLastMethod, "GET") == 0 ? "PASS" : "FAIL");
		printf("  HTTP server saw target : %s\n", strcmp(tServer.aLastTarget, "/hello") == 0 ? "PASS" : "FAIL");
		printf("  HTTP server saw host : %s\n", strstr(tServer.aLastHost, "127.0.0.1") != NULL ? "PASS" : "FAIL");
		printf("  HTTP plain path error free : %s\n", __Test_XHttpAtomicLoad(&tServer.iErrorCount) == 0 ? "PASS" : "FAIL");
		if ( pResp ) xrtHttpResponseDestroy(pResp);
		if ( pFuture ) xrtNetFutureDestroy(pFuture);
		xrtHttpRequestUnit(&tReq);
		xrtHttpCloseIdleConnections(pClientEngine);
		if ( pClientEngine ) {
			xrtNetEngineStop(pClientEngine);
			xrtNetEngineDestroy(pClientEngine);
		}
		__Test_XHttpServerStop(&tServer);
	}

	{
		__test_xhttp_server tServer;
		xnetengineconfig tCfg;
		xnetengine* pClientEngine = NULL;
		xhttprequest tReq;
		xnetfuture* pFuture = NULL;
		xnet_result iStatus = XRT_NET_ERROR;
		char sURL[256];
		printf("  HTTP explicit pending cancel server start : %s\n",
			__Test_XHttpServerStartEx(
				&tServer,
				NULL,
				"HTTP/1.1 200 OK\r\nContent-Length: 5\r\nConnection: close\r\n\r\nlater",
				250u,
				NULL) ? "PASS" : "FAIL");
		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/explicit-cancel", (unsigned)tServer.pListener->tConfig.tBindAddr.iPort);
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pClientEngine = xrtNetEngineCreate(&tCfg);
		if ( pClientEngine ) { (void)xrtNetEngineStart(pClientEngine); }
		xrtHttpRequestInit(&tReq);
		(void)xrtHttpRequestSetURL(&tReq, sURL);
		xhttpdiagnostics tCancelDiagnostics;
		xrtHttpRequestSetDiagnostics(&tReq, &tCancelDiagnostics);
		pFuture = xrtHttpExecuteAsync(pClientEngine, &tReq);
		printf("  HTTP explicit pending cancel reached server : %s\n", __Test_XHttpWaitMin(&tServer.iRecvCount, 1, 1000u) ? "PASS" : "FAIL");
		printf("  HTTP explicit pending cancel request : %s\n", pFuture && xrtHttpCancel(pFuture) ? "PASS" : "FAIL");
		printf("  HTTP explicit pending cancel status : %s\n", pFuture && (iStatus = xrtNetFutureWait(pFuture, 1000u)) == XRT_NET_CANCELLED ? "PASS" : "FAIL");
		printf("  HTTP explicit pending cancel diagnostics : %s\n",
			tCancelDiagnostics.eError == XHTTP_ERROR_CANCELLED &&
			tCancelDiagnostics.eTransportStatus == XRT_NET_CANCELLED ? "PASS" : "FAIL");
		__Test_XHttpSleepMs(300u);
		if ( pFuture ) { xrtNetFutureDestroy(pFuture); }
		xrtHttpRequestUnit(&tReq);
		if ( pClientEngine ) { xrtNetEngineStop(pClientEngine); xrtNetEngineDestroy(pClientEngine); }
		__Test_XHttpServerStop(&tServer);
	}

	{
		__test_xhttp_server tServer;
		xnetengineconfig tCfg;
		xnetengine* pClientEngine = NULL;
		xhttprequest tReq;
		xnetfuture* pFuture = NULL;
		xhttpresponse* pResp = NULL;
		xnet_result iStatus = XRT_NET_ERROR;
		char sURL[256];
		char sResponse[2048];
		size_t iLen = 0u;
		iLen += (size_t)snprintf(sResponse + iLen, sizeof(sResponse) - iLen, "HTTP/1.1 200 OK\r\n");
		for ( uint32 i = 0u; i < 48u && iLen < sizeof(sResponse); ++i ) {
			iLen += (size_t)snprintf(sResponse + iLen, sizeof(sResponse) - iLen, "X-Response-%u: value-%u\r\n", (unsigned)i, (unsigned)i);
		}
		(void)snprintf(sResponse + iLen, sizeof(sResponse) - iLen, "Content-Length: 2\r\nConnection: close\r\n\r\nok");
		printf("  HTTP dynamic response headers server start : %s\n", __Test_XHttpServerStart(&tServer, sResponse, NULL) ? "PASS" : "FAIL");
		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/many-response-headers", (unsigned)tServer.pListener->tConfig.tBindAddr.iPort);
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pClientEngine = xrtNetEngineCreate(&tCfg);
		if ( pClientEngine ) { (void)xrtNetEngineStart(pClientEngine); }
		xrtHttpRequestInit(&tReq);
		(void)xrtHttpRequestSetURL(&tReq, sURL);
		pFuture = xrtHttpExecuteAsync(pClientEngine, &tReq);
		printf("  HTTP dynamic response headers future wait : %s\n", pFuture && (iStatus = xrtNetFutureWait(pFuture, 3000u)) == XRT_NET_OK ? "PASS" : "FAIL");
		pResp = (pFuture && iStatus == XRT_NET_OK) ? (xhttpresponse*)xrtNetFutureValue(pFuture) : NULL;
		printf("  HTTP dynamic response headers preserved : %s\n", pResp && xrtHttpResponseHeaderCount(pResp) == 50u && strcmp(xrtHttpResponseHeader(pResp, "X-Response-47"), "value-47") == 0 ? "PASS" : "FAIL");
		printf("  HTTP dynamic response headers indexed : %s\n", pResp && xrtHttpResponseHeaderNameAt(pResp, 47u) && xrtHttpResponseHeaderValueAt(pResp, 47u) ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		if ( pFuture ) { xrtNetFutureDestroy(pFuture); }
		xrtHttpRequestUnit(&tReq);
		if ( pClientEngine ) { xrtNetEngineStop(pClientEngine); xrtNetEngineDestroy(pClientEngine); }
		__Test_XHttpServerStop(&tServer);
	}

	{
		__test_xhttp_server tServer;
		__test_xhttp_stream_ctx tStream;
		xhttpstreamcallbacks tCallbacks;
		xnetengineconfig tCfg;
		xnetengine* pClientEngine = NULL;
		xhttprequest tReq;
		xnetfuture* pFuture = NULL;
		xhttpresponse* pResp = NULL;
		xnet_result iStatus = XRT_NET_ERROR;
		char sURL[256];
		memset(&tStream, 0, sizeof(tStream));
		xrtHttpStreamCallbacksInit(&tCallbacks);
		tCallbacks.pUserData = &tStream;
		tCallbacks.OnHeaders = __Test_XHttpStreamOnHeaders;
		tCallbacks.OnBody = __Test_XHttpStreamOnBody;
		printf("  HTTP fixed stream server start : %s\n",
			__Test_XHttpServerStartEx(
				&tServer,
				"HTTP/1.1 200 OK\r\nContent-Length: 8\r\nX-Stream: fixed\r\nConnection: keep-alive\r\n\r\nabc",
				"defgh",
				80u,
				NULL) ? "PASS" : "FAIL");
		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/stream-fixed", (unsigned)tServer.pListener->tConfig.tBindAddr.iPort);
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pClientEngine = xrtNetEngineCreate(&tCfg);
		if ( pClientEngine ) { (void)xrtNetEngineStart(pClientEngine); }
		xrtHttpRequestInit(&tReq);
		(void)xrtHttpRequestSetURL(&tReq, sURL);
		pFuture = xrtHttpExecuteStreamAsync(pClientEngine, &tReq, &tCallbacks);
		printf("  HTTP fixed stream future create : %s\n", pFuture ? "PASS" : "FAIL");
		printf("  HTTP fixed stream future wait : %s\n", pFuture && (iStatus = xrtNetFutureWait(pFuture, 3000u)) == XRT_NET_OK ? "PASS" : "FAIL");
		pResp = (pFuture && iStatus == XRT_NET_OK) ? (xhttpresponse*)xrtNetFutureValue(pFuture) : NULL;
		printf("  HTTP fixed stream headers once : %s\n", __Test_XHttpAtomicLoad(&tStream.iHeadersCount) == 1 ? "PASS" : "FAIL");
		printf("  HTTP fixed stream header metadata : %s\n", tStream.iStatusCode == 200u && tStream.bSawStreamHeader ? "PASS" : "FAIL");
		printf("  HTTP fixed stream incremental chunks : %s\n", __Test_XHttpAtomicLoad(&tStream.iBodyCount) >= 2 ? "PASS" : "FAIL");
		printf("  HTTP fixed stream body : %s\n", tStream.iBodyLen == 8u && memcmp(tStream.aBody, "abcdefgh", 8u) == 0 ? "PASS" : "FAIL");
		printf("  HTTP fixed stream response metadata only : %s\n", pResp && pResp->pBody == NULL && pResp->iBodyLen == 8u ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		if ( pFuture ) { xrtNetFutureDestroy(pFuture); }
		xrtHttpRequestUnit(&tReq);
		xrtHttpCloseIdleConnections(pClientEngine);
		if ( pClientEngine ) { xrtNetEngineStop(pClientEngine); xrtNetEngineDestroy(pClientEngine); }
		__Test_XHttpServerStop(&tServer);
	}

	{
		__test_xhttp_server tServer;
		__test_xhttp_stream_ctx tStream;
		xhttpstreamcallbacks tCallbacks;
		xnetengineconfig tCfg;
		xnetengine* pClientEngine = NULL;
		xhttprequest tReq;
		xnetfuture* pFuture = NULL;
		xhttpresponse* pResp = NULL;
		xnet_result iStatus = XRT_NET_ERROR;
		char sURL[256];
		memset(&tStream, 0, sizeof(tStream));
		xrtHttpStreamCallbacksInit(&tCallbacks);
		tCallbacks.pUserData = &tStream;
		tCallbacks.OnHeaders = __Test_XHttpStreamOnHeaders;
		tCallbacks.OnBody = __Test_XHttpStreamOnBody;
		printf("  HTTP chunked stream server start : %s\n",
			__Test_XHttpServerStartEx(
				&tServer,
				"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nX-Stream: chunked\r\nConnection: keep-alive\r\n\r\n3\r\nabc\r\n5\r\nd",
				"efgh\r\n0\r\nX-Trailer: yes\r\n\r\n",
				80u,
				NULL) ? "PASS" : "FAIL");
		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/stream-chunked", (unsigned)tServer.pListener->tConfig.tBindAddr.iPort);
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pClientEngine = xrtNetEngineCreate(&tCfg);
		if ( pClientEngine ) { (void)xrtNetEngineStart(pClientEngine); }
		xrtHttpRequestInit(&tReq);
		(void)xrtHttpRequestSetURL(&tReq, sURL);
		pFuture = xrtHttpExecuteStreamAsync(pClientEngine, &tReq, &tCallbacks);
		printf("  HTTP chunked stream future wait : %s\n", pFuture && (iStatus = xrtNetFutureWait(pFuture, 3000u)) == XRT_NET_OK ? "PASS" : "FAIL");
		pResp = (pFuture && iStatus == XRT_NET_OK) ? (xhttpresponse*)xrtNetFutureValue(pFuture) : NULL;
		printf("  HTTP chunked stream body : %s\n", tStream.iBodyLen == 8u && memcmp(tStream.aBody, "abcdefgh", 8u) == 0 ? "PASS" : "FAIL");
		printf("  HTTP chunked stream decoded length : %s\n", pResp && pResp->iBodyLen == 8u && pResp->iContentLength == 8 && (pResp->iFlags & XHTTP_RESP_F_CHUNKED) != 0u ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		if ( pFuture ) { xrtNetFutureDestroy(pFuture); }
		xrtHttpRequestUnit(&tReq);
		xrtHttpCloseIdleConnections(pClientEngine);
		if ( pClientEngine ) { xrtNetEngineStop(pClientEngine); xrtNetEngineDestroy(pClientEngine); }
		__Test_XHttpServerStop(&tServer);
	}

	{
		__test_xhttp_server tServer;
		__test_xhttp_stream_ctx tStream;
		xhttpstreamcallbacks tCallbacks;
		xnetengineconfig tCfg;
		xnetengine* pClientEngine = NULL;
		xhttprequest tReq;
		xnetfuture* pFuture = NULL;
		xnet_result iStatus = XRT_NET_ERROR;
		char sURL[256];
		memset(&tStream, 0, sizeof(tStream));
		tStream.bCancelOnBody = true;
		xrtHttpStreamCallbacksInit(&tCallbacks);
		tCallbacks.pUserData = &tStream;
		tCallbacks.OnHeaders = __Test_XHttpStreamOnHeaders;
		tCallbacks.OnBody = __Test_XHttpStreamOnBody;
		printf("  HTTP stream callback cancel server start : %s\n",
			__Test_XHttpServerStartEx(
				&tServer,
				"HTTP/1.1 200 OK\r\nContent-Length: 8\r\nX-Stream: cancel\r\nConnection: close\r\n\r\nabc",
				"defgh",
				250u,
				NULL) ? "PASS" : "FAIL");
		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/stream-cancel", (unsigned)tServer.pListener->tConfig.tBindAddr.iPort);
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pClientEngine = xrtNetEngineCreate(&tCfg);
		if ( pClientEngine ) { (void)xrtNetEngineStart(pClientEngine); }
		xrtHttpRequestInit(&tReq);
		(void)xrtHttpRequestSetURL(&tReq, sURL);
		pFuture = xrtHttpExecuteStreamAsync(pClientEngine, &tReq, &tCallbacks);
		printf("  HTTP stream callback cancel status : %s\n", pFuture && (iStatus = xrtNetFutureWait(pFuture, 3000u)) == XRT_NET_CANCELLED ? "PASS" : "FAIL");
		printf("  HTTP stream callback cancel observed body : %s\n", tStream.iBodyLen > 0u && tStream.iBodyLen < 8u ? "PASS" : "FAIL");
		printf("  HTTP stream explicit cancel idempotent : %s\n", pFuture && xrtHttpCancel(pFuture) ? "PASS" : "FAIL");
		__Test_XHttpSleepMs(300u);
		if ( pFuture ) { xrtNetFutureDestroy(pFuture); }
		xrtHttpRequestUnit(&tReq);
		if ( pClientEngine ) { xrtNetEngineStop(pClientEngine); xrtNetEngineDestroy(pClientEngine); }
		__Test_XHttpServerStop(&tServer);
	}

	{
		__test_xhttp_server tServer;
		xnetengineconfig tCfg;
		xnetengine* pClientEngine = NULL;
		xhttprequest tReq;
		xnetfuture* pFuture = NULL;
		xhttpresponse* pResp = NULL;
		xnet_result iStatus = XRT_NET_ERROR;
		char sURL[256];
		printf("  HTTP close-delimited server start : %s\n", __Test_XHttpServerStart(&tServer, "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\nclose-body", NULL) ? "PASS" : "FAIL");
		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/close-delimited", (unsigned)tServer.pListener->tConfig.tBindAddr.iPort);
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pClientEngine = xrtNetEngineCreate(&tCfg);
		if ( pClientEngine ) (void)xrtNetEngineStart(pClientEngine);
		xrtHttpRequestInit(&tReq);
		(void)xrtHttpRequestSetURL(&tReq, sURL);
		(void)xrtHttpRequestSetHeader(&tReq, "Connection", "close");
		pFuture = xrtHttpExecuteAsync(pClientEngine, &tReq);
		printf("  HTTP close-delimited future create : %s\n", pFuture != NULL ? "PASS" : "FAIL");
		printf("  HTTP close-delimited future wait : %s\n", pFuture && (iStatus = xrtNetFutureWait(pFuture, 3000)) == XRT_NET_OK ? "PASS" : "FAIL");
		pResp = (pFuture && iStatus == XRT_NET_OK) ? (xhttpresponse*)xrtNetFutureValue(pFuture) : NULL;
		printf("  HTTP close-delimited response code : %s\n", pResp && pResp->iStatusCode == 200 ? "PASS" : "FAIL");
		printf("  HTTP close-delimited response body : %s\n", pResp && pResp->iBodyLen == 10 && memcmp(pResp->pBody, "close-body", 10) == 0 ? "PASS" : "FAIL");
		printf("  HTTP close-delimited no keepalive : %s\n", pResp && (pResp->iFlags & XHTTP_RESP_F_KEEPALIVE) == 0 ? "PASS" : "FAIL");
		if ( pResp ) xrtHttpResponseDestroy(pResp);
		if ( pFuture ) xrtNetFutureDestroy(pFuture);
		xrtHttpRequestUnit(&tReq);
		if ( pClientEngine ) {
			xrtNetEngineStop(pClientEngine);
			xrtNetEngineDestroy(pClientEngine);
		}
		__Test_XHttpServerStop(&tServer);
	}

	{
		__test_xhttp_server tServer;
		xnetengineconfig tCfg;
		xnetengine* pClientEngine = NULL;
		xhttprequest tReq1;
		xhttprequest tReq2;
		xnetfuture* pFuture1 = NULL;
		xnetfuture* pFuture2 = NULL;
		xhttpresponse* pResp1 = NULL;
		xhttpresponse* pResp2 = NULL;
		xnet_result iStatus1 = XRT_NET_ERROR;
		xnet_result iStatus2 = XRT_NET_ERROR;
		char sURL1[256];
		char sURL2[256];
		printf("  HTTP keepalive server start : %s\n", __Test_XHttpServerStart(&tServer, "HTTP/1.1 200 OK\r\nContent-Length: 4\r\nConnection: keep-alive\r\n\r\nkeep", NULL) ? "PASS" : "FAIL");
		snprintf(sURL1, sizeof(sURL1), "http://127.0.0.1:%u/reuse-a", (unsigned)tServer.pListener->tConfig.tBindAddr.iPort);
		snprintf(sURL2, sizeof(sURL2), "http://127.0.0.1:%u/reuse-b", (unsigned)tServer.pListener->tConfig.tBindAddr.iPort);
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pClientEngine = xrtNetEngineCreate(&tCfg);
		if ( pClientEngine ) (void)xrtNetEngineStart(pClientEngine);
		xrtHttpRequestInit(&tReq1);
		xrtHttpRequestInit(&tReq2);
		(void)xrtHttpRequestSetURL(&tReq1, sURL1);
		(void)xrtHttpRequestSetURL(&tReq2, sURL2);
		pFuture1 = xrtHttpExecuteAsync(pClientEngine, &tReq1);
		printf("  HTTP keepalive future #1 create : %s\n", pFuture1 != NULL ? "PASS" : "FAIL");
		printf("  HTTP keepalive future #1 wait : %s\n", pFuture1 && (iStatus1 = xrtNetFutureWait(pFuture1, 3000)) == XRT_NET_OK ? "PASS" : "FAIL");
		pResp1 = (pFuture1 && iStatus1 == XRT_NET_OK) ? (xhttpresponse*)xrtNetFutureValue(pFuture1) : NULL;
		printf("  HTTP keepalive response #1 : %s\n", pResp1 && pResp1->iBodyLen == 4 && memcmp(pResp1->pBody, "keep", 4) == 0 ? "PASS" : "FAIL");
		pFuture2 = xrtHttpExecuteAsync(pClientEngine, &tReq2);
		printf("  HTTP keepalive future #2 create : %s\n", pFuture2 != NULL ? "PASS" : "FAIL");
		printf("  HTTP keepalive future #2 wait : %s\n", pFuture2 && (iStatus2 = xrtNetFutureWait(pFuture2, 3000)) == XRT_NET_OK ? "PASS" : "FAIL");
		pResp2 = (pFuture2 && iStatus2 == XRT_NET_OK) ? (xhttpresponse*)xrtNetFutureValue(pFuture2) : NULL;
		printf("  HTTP keepalive response #2 : %s\n", pResp2 && pResp2->iBodyLen == 4 && memcmp(pResp2->pBody, "keep", 4) == 0 ? "PASS" : "FAIL");
		printf("  HTTP keepalive single accept : %s\n", __Test_XHttpAtomicLoad(&tServer.iAcceptCount) == 1 ? "PASS" : "FAIL");
		printf("  HTTP keepalive single open : %s\n", __Test_XHttpAtomicLoad(&tServer.iOpenCount) == 1 ? "PASS" : "FAIL");
		printf("  HTTP keepalive two requests : %s\n", __Test_XHttpAtomicLoad(&tServer.iRecvCount) == 2 ? "PASS" : "FAIL");
		printf("  HTTP keepalive second target : %s\n", strcmp(tServer.aLastTarget, "/reuse-b") == 0 ? "PASS" : "FAIL");
		printf("  HTTP keepalive server stays error free : %s\n", __Test_XHttpAtomicLoad(&tServer.iErrorCount) == 0 ? "PASS" : "FAIL");
		if ( pResp1 ) xrtHttpResponseDestroy(pResp1);
		if ( pResp2 ) xrtHttpResponseDestroy(pResp2);
		if ( pFuture1 ) xrtNetFutureDestroy(pFuture1);
		if ( pFuture2 ) xrtNetFutureDestroy(pFuture2);
		xrtHttpRequestUnit(&tReq1);
		xrtHttpRequestUnit(&tReq2);
		xrtHttpCloseIdleConnections(pClientEngine);
		if ( pClientEngine ) {
			xrtNetEngineStop(pClientEngine);
			xrtNetEngineDestroy(pClientEngine);
		}
		__Test_XHttpServerStop(&tServer);
	}

	{
		__test_xhttp_server tServer;
		xnetengineconfig tCfg;
		xnetengine* pClientEngine = NULL;
		xhttprequest tReq1;
		xhttprequest tReq2;
		xnetfuture* pFuture1 = NULL;
		xnetfuture* pFuture2 = NULL;
		xhttpresponse* pResp1 = NULL;
		xhttpresponse* pResp2 = NULL;
		xnet_result iStatus1 = XRT_NET_ERROR;
		xnet_result iStatus2 = XRT_NET_ERROR;
		char sURL1[256];
		char sURL2[256];
		printf("  HTTP chunked keepalive server start : %s\n",
			__Test_XHttpServerStart(&tServer,
				"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: keep-alive\r\n\r\n5\r\nchunk\r\n3\r\ned!\r\n0\r\n\r\n",
				NULL) ? "PASS" : "FAIL");
		snprintf(sURL1, sizeof(sURL1), "http://127.0.0.1:%u/chunk-a", (unsigned)tServer.pListener->tConfig.tBindAddr.iPort);
		snprintf(sURL2, sizeof(sURL2), "http://127.0.0.1:%u/chunk-b", (unsigned)tServer.pListener->tConfig.tBindAddr.iPort);
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pClientEngine = xrtNetEngineCreate(&tCfg);
		if ( pClientEngine ) (void)xrtNetEngineStart(pClientEngine);
		xrtHttpRequestInit(&tReq1);
		xrtHttpRequestInit(&tReq2);
		(void)xrtHttpRequestSetURL(&tReq1, sURL1);
		(void)xrtHttpRequestSetURL(&tReq2, sURL2);
		pFuture1 = xrtHttpExecuteAsync(pClientEngine, &tReq1);
		printf("  HTTP chunked future #1 create : %s\n", pFuture1 != NULL ? "PASS" : "FAIL");
		printf("  HTTP chunked future #1 wait : %s\n", pFuture1 && (iStatus1 = xrtNetFutureWait(pFuture1, 3000)) == XRT_NET_OK ? "PASS" : "FAIL");
		pResp1 = (pFuture1 && iStatus1 == XRT_NET_OK) ? (xhttpresponse*)xrtNetFutureValue(pFuture1) : NULL;
		printf("  HTTP chunked response #1 body : %s\n", pResp1 && pResp1->iBodyLen == 8 && memcmp(pResp1->pBody, "chunked!", 8) == 0 ? "PASS" : "FAIL");
		printf("  HTTP chunked response #1 flag : %s\n", pResp1 && (pResp1->iFlags & XHTTP_RESP_F_CHUNKED) != 0 ? "PASS" : "FAIL");
		pFuture2 = xrtHttpExecuteAsync(pClientEngine, &tReq2);
		printf("  HTTP chunked future #2 create : %s\n", pFuture2 != NULL ? "PASS" : "FAIL");
		printf("  HTTP chunked future #2 wait : %s\n", pFuture2 && (iStatus2 = xrtNetFutureWait(pFuture2, 3000)) == XRT_NET_OK ? "PASS" : "FAIL");
		pResp2 = (pFuture2 && iStatus2 == XRT_NET_OK) ? (xhttpresponse*)xrtNetFutureValue(pFuture2) : NULL;
		printf("  HTTP chunked response #2 body : %s\n", pResp2 && pResp2->iBodyLen == 8 && memcmp(pResp2->pBody, "chunked!", 8) == 0 ? "PASS" : "FAIL");
		printf("  HTTP chunked keepalive single accept : %s\n", __Test_XHttpAtomicLoad(&tServer.iAcceptCount) == 1 ? "PASS" : "FAIL");
		printf("  HTTP chunked keepalive two requests : %s\n", __Test_XHttpAtomicLoad(&tServer.iRecvCount) == 2 ? "PASS" : "FAIL");
		printf("  HTTP chunked keepalive second target : %s\n", strcmp(tServer.aLastTarget, "/chunk-b") == 0 ? "PASS" : "FAIL");
		if ( pResp1 ) xrtHttpResponseDestroy(pResp1);
		if ( pResp2 ) xrtHttpResponseDestroy(pResp2);
		if ( pFuture1 ) xrtNetFutureDestroy(pFuture1);
		if ( pFuture2 ) xrtNetFutureDestroy(pFuture2);
		xrtHttpRequestUnit(&tReq1);
		xrtHttpRequestUnit(&tReq2);
		xrtHttpCloseIdleConnections(pClientEngine);
		if ( pClientEngine ) {
			xrtNetEngineStop(pClientEngine);
			xrtNetEngineDestroy(pClientEngine);
		}
		__Test_XHttpServerStop(&tServer);
	}

	{
		__test_xhttp_server tServer;
		xnetengineconfig tCfg;
		xnetengine* pClientEngine = NULL;
		xhttprequest tReq;
		xnetfuture* pFuture = NULL;
		xhttpresponse* pResp = NULL;
		xnet_result iStatus = XRT_NET_ERROR;
		char sURL[256];
		printf("  HTTP chunked request server start : %s\n",
			__Test_XHttpServerStart(&tServer, "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok", NULL) ? "PASS" : "FAIL");
		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/ingest", (unsigned)tServer.pListener->tConfig.tBindAddr.iPort);
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pClientEngine = xrtNetEngineCreate(&tCfg);
		if ( pClientEngine ) (void)xrtNetEngineStart(pClientEngine);
		xrtHttpRequestInit(&tReq);
		(void)xrtHttpRequestSetMethod(&tReq, "POST");
		(void)xrtHttpRequestSetURL(&tReq, sURL);
		(void)xrtHttpRequestSetHeader(&tReq, "Transfer-Encoding", "chunked");
		(void)xrtHttpRequestSetBodyCopy(&tReq, "hello-chunk", 11, "text/plain");
		pFuture = xrtHttpExecuteAsync(pClientEngine, &tReq);
		printf("  HTTP chunked request future create : %s\n", pFuture != NULL ? "PASS" : "FAIL");
		printf("  HTTP chunked request future wait : %s\n", pFuture && (iStatus = xrtNetFutureWait(pFuture, 3000)) == XRT_NET_OK ? "PASS" : "FAIL");
		pResp = (pFuture && iStatus == XRT_NET_OK) ? (xhttpresponse*)xrtNetFutureValue(pFuture) : NULL;
		printf("  HTTP chunked request response code : %s\n", pResp && pResp->iStatusCode == 200 ? "PASS" : "FAIL");
		printf("  HTTP chunked request server saw body : %s\n", strcmp(tServer.aLastBody, "hello-chunk") == 0 ? "PASS" : "FAIL");
		if ( pResp ) xrtHttpResponseDestroy(pResp);
		if ( pFuture ) xrtNetFutureDestroy(pFuture);
		xrtHttpRequestUnit(&tReq);
		if ( pClientEngine ) {
			xrtNetEngineStop(pClientEngine);
			xrtNetEngineDestroy(pClientEngine);
		}
		__Test_XHttpServerStop(&tServer);
	}

	{
		__test_xhttp_server tServer;
		xnetengineconfig tCfg;
		xnetengine* pClientEngine = NULL;
		xhttprequest tReq;
		xnetfuture* pFuture = NULL;
		xhttpresponse* pResp = NULL;
		xnet_result iStatus = XRT_NET_ERROR;
		char sURL[256];
		printf("  HTTP idle reset server start : %s\n",
			__Test_XHttpServerStartEx(
				&tServer,
				"HTTP/1.1 200 OK\r\nContent-Length: 8\r\nConnection: close\r\n\r\nchu",
				"nked!",
				80u,
				NULL) ? "PASS" : "FAIL");
		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/idle-reset", (unsigned)tServer.pListener->tConfig.tBindAddr.iPort);
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pClientEngine = xrtNetEngineCreate(&tCfg);
		if ( pClientEngine ) (void)xrtNetEngineStart(pClientEngine);
		xrtHttpRequestInit(&tReq);
		(void)xrtHttpRequestSetURL(&tReq, sURL);
		xrtHttpRequestSetTimeout(&tReq, 1000u);
		xrtHttpRequestSetIdleTimeout(&tReq, 200u);
		pFuture = xrtHttpExecuteAsync(pClientEngine, &tReq);
		printf("  HTTP idle reset future create : %s\n", pFuture != NULL ? "PASS" : "FAIL");
		printf("  HTTP idle reset future wait : %s\n", pFuture && (iStatus = xrtNetFutureWait(pFuture, 1500u)) == XRT_NET_OK ? "PASS" : "FAIL");
		pResp = (pFuture && iStatus == XRT_NET_OK) ? (xhttpresponse*)xrtNetFutureValue(pFuture) : NULL;
		printf("  HTTP idle reset response body : %s\n", pResp && pResp->iBodyLen == 8 && memcmp(pResp->pBody, "chunked!", 8) == 0 ? "PASS" : "FAIL");
		printf("  HTTP idle reset request reached server : %s\n", __Test_XHttpWaitMin(&tServer.iRecvCount, 1, 1000u) ? "PASS" : "FAIL");
		if ( pResp ) xrtHttpResponseDestroy(pResp);
		if ( pFuture ) xrtNetFutureDestroy(pFuture);
		xrtHttpRequestUnit(&tReq);
		xrtHttpCloseIdleConnections(pClientEngine);
		if ( pClientEngine ) {
			xrtNetEngineStop(pClientEngine);
			xrtNetEngineDestroy(pClientEngine);
		}
		__Test_XHttpServerStop(&tServer);
	}

	{
		__test_xhttp_server tServer;
		xnetengineconfig tCfg;
		xnetengine* pClientEngine = NULL;
		xhttprequest tReq;
		xnetfuture* pFuture = NULL;
		xnet_result iStatus = XRT_NET_ERROR;
		char sURL[256];
		printf("  HTTP idle timeout server start : %s\n",
			__Test_XHttpServerStartEx(
				&tServer,
				"HTTP/1.1 200 OK\r\nContent-Length: 8\r\nConnection: close\r\n\r\ntime",
				"out!",
				250u,
				NULL) ? "PASS" : "FAIL");
		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/idle-timeout", (unsigned)tServer.pListener->tConfig.tBindAddr.iPort);
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pClientEngine = xrtNetEngineCreate(&tCfg);
		if ( pClientEngine ) (void)xrtNetEngineStart(pClientEngine);
		xrtHttpRequestInit(&tReq);
		(void)xrtHttpRequestSetURL(&tReq, sURL);
		xrtHttpRequestSetTimeout(&tReq, 1000u);
		xrtHttpRequestSetIdleTimeout(&tReq, 100u);
		xhttpdiagnostics tIdleDiagnostics;
		xrtHttpRequestSetDiagnostics(&tReq, &tIdleDiagnostics);
		pFuture = xrtHttpExecuteAsync(pClientEngine, &tReq);
		printf("  HTTP idle timeout future create : %s\n", pFuture != NULL ? "PASS" : "FAIL");
		printf("  HTTP idle timeout status : %s\n", pFuture && (iStatus = xrtNetFutureWait(pFuture, 1500u)) == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  HTTP idle timeout diagnostics : %s\n",
			tIdleDiagnostics.eError == XHTTP_ERROR_TIMEOUT_IDLE &&
			tIdleDiagnostics.eTransportStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  HTTP idle timeout request reached server : %s\n", __Test_XHttpWaitMin(&tServer.iRecvCount, 1, 1000u) ? "PASS" : "FAIL");
		__Test_XHttpSleepMs(320u);
		if ( pFuture ) xrtNetFutureDestroy(pFuture);
		xrtHttpRequestUnit(&tReq);
		xrtHttpCloseIdleConnections(pClientEngine);
		if ( pClientEngine ) {
			xrtNetEngineStop(pClientEngine);
			xrtNetEngineDestroy(pClientEngine);
		}
		__Test_XHttpServerStop(&tServer);
	}

	{
		__test_xhttp_server tServer;
		xnetengineconfig tCfg;
		xnetengine* pClientEngine = NULL;
		xhttprequest tReq;
		xnetfuture* pFuture = NULL;
		xnet_result iStatus = XRT_NET_ERROR;
		char sURL[256];
		printf("  HTTP total+idle timeout server start : %s\n",
			__Test_XHttpServerStartEx(
				&tServer,
				NULL,
				"HTTP/1.1 200 OK\r\nContent-Length: 5\r\nConnection: close\r\n\r\nlater",
				250u,
				NULL) ? "PASS" : "FAIL");
		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/total-timeout", (unsigned)tServer.pListener->tConfig.tBindAddr.iPort);
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pClientEngine = xrtNetEngineCreate(&tCfg);
		if ( pClientEngine ) (void)xrtNetEngineStart(pClientEngine);
		xrtHttpRequestInit(&tReq);
		(void)xrtHttpRequestSetURL(&tReq, sURL);
		xrtHttpRequestSetTimeout(&tReq, 100u);
		xrtHttpRequestSetIdleTimeout(&tReq, 400u);
		xhttpdiagnostics tTotalDiagnostics;
		xrtHttpRequestSetDiagnostics(&tReq, &tTotalDiagnostics);
		pFuture = xrtHttpExecuteAsync(pClientEngine, &tReq);
		printf("  HTTP total+idle future create : %s\n", pFuture != NULL ? "PASS" : "FAIL");
		printf("  HTTP total+idle total timeout wins : %s\n", pFuture && (iStatus = xrtNetFutureWait(pFuture, 1200u)) == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  HTTP total timeout diagnostics : %s\n",
			tTotalDiagnostics.eError == XHTTP_ERROR_TIMEOUT_TOTAL &&
			tTotalDiagnostics.eTransportStatus == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  HTTP total+idle request reached server : %s\n", __Test_XHttpWaitMin(&tServer.iRecvCount, 1, 1000u) ? "PASS" : "FAIL");
		__Test_XHttpSleepMs(320u);
		if ( pFuture ) xrtNetFutureDestroy(pFuture);
		xrtHttpRequestUnit(&tReq);
		xrtHttpCloseIdleConnections(pClientEngine);
		if ( pClientEngine ) {
			xrtNetEngineStop(pClientEngine);
			xrtNetEngineDestroy(pClientEngine);
		}
		__Test_XHttpServerStop(&tServer);
	}

	{
		__test_xhttp_server tServer;
		xnetengineconfig tCfg;
		xnetengine* pClientEngine = NULL;
		xhttpclient* pClient = NULL;
		xhttprequest tReq;
		xhttpresponse* pResp = NULL;
		xnet_result iStatus = XRT_NET_ERROR;
		char sURL[256];
		printf("  HTTP client-scoped pool server start : %s\n", __Test_XHttpServerStart(&tServer, "HTTP/1.1 200 OK\r\nContent-Length: 4\r\nConnection: keep-alive\r\n\r\npool", NULL) ? "PASS" : "FAIL");
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pClientEngine = xrtNetEngineCreate(&tCfg);
		if ( pClientEngine ) { (void)xrtNetEngineStart(pClientEngine); }
		pClient = xrtHttpClientCreate(pClientEngine);
		printf("  HTTP client-scoped pool create : %s\n", pClient ? "PASS" : "FAIL");
		xrtHttpRequestInit(&tReq);
		xhttpdiagnostics tPoolDiagnostics;
		xrtHttpRequestSetDiagnostics(&tReq, &tPoolDiagnostics);
		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/client-pool-a", (unsigned)tServer.pListener->tConfig.tBindAddr.iPort);
		(void)xrtHttpRequestSetURL(&tReq, sURL);
		pResp = xrtHttpClientExecuteSync(pClient, &tReq, &iStatus);
		printf("  HTTP client-scoped pool request #1 : %s\n", iStatus == XRT_NET_OK && pResp && pResp->iBodyLen == 4u ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); pResp = NULL; }
		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/client-pool-b", (unsigned)tServer.pListener->tConfig.tBindAddr.iPort);
		(void)xrtHttpRequestSetURL(&tReq, sURL);
		pResp = xrtHttpClientExecuteSync(pClient, &tReq, &iStatus);
		printf("  HTTP client-scoped pool request #2 : %s\n", iStatus == XRT_NET_OK && pResp && pResp->iBodyLen == 4u ? "PASS" : "FAIL");
		printf("  HTTP client-scoped pool reused connection : %s\n", __Test_XHttpAtomicLoad(&tServer.iAcceptCount) == 1 ? "PASS" : "FAIL");
		printf("  HTTP client-scoped diagnostics reused connection : %s\n", tPoolDiagnostics.bReusedConnection ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); pResp = NULL; }
		xrtHttpClientCloseIdle(pClient);
		__Test_XHttpSleepMs(50u);
		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/client-pool-c", (unsigned)tServer.pListener->tConfig.tBindAddr.iPort);
		(void)xrtHttpRequestSetURL(&tReq, sURL);
		pResp = xrtHttpClientExecuteSync(pClient, &tReq, &iStatus);
		printf("  HTTP client-scoped pool close idle reconnects : %s\n", iStatus == XRT_NET_OK && pResp && __Test_XHttpAtomicLoad(&tServer.iAcceptCount) == 2 ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		xrtHttpRequestUnit(&tReq);
		xrtHttpClientDestroy(pClient);
		__Test_XHttpSleepMs(50u);
		if ( pClientEngine ) { xrtNetEngineStop(pClientEngine); xrtNetEngineDestroy(pClientEngine); }
		__Test_XHttpServerStop(&tServer);
	}

	{
		__test_xhttp_server tServer;
		xhttprequest tReq;
		xhttpresponse* pResp = NULL;
		xnet_result iStatus = XRT_NET_ERROR;
		char sURL[256];
		xtlsconfig tTlsCfg;
		memset(&tTlsCfg, 0, sizeof(tTlsCfg));
		tTlsCfg.sCertFile = "dev/xnet2_tls_test_cert.pem";
		tTlsCfg.sKeyFile = "dev/xnet2_tls_test_key.pem";
		if ( __Test_XHttpFileExists(tTlsCfg.sCertFile) && __Test_XHttpFileExists(tTlsCfg.sKeyFile) ) {
			printf("  HTTP TLS fixture files exist : PASS\n");
			printf("  HTTP TLS server start : %s\n", __Test_XHttpServerStart(&tServer, "HTTP/1.1 200 OK\r\nContent-Length: 6\r\nConnection: keep-alive\r\n\r\nsecure", &tTlsCfg) ? "PASS" : "FAIL");
			snprintf(sURL, sizeof(sURL), "https://127.0.0.1:%u/secure", (unsigned)tServer.pListener->tConfig.tBindAddr.iPort);
			xrtHttpRequestInit(&tReq);
			(void)xrtHttpRequestSetURL(&tReq, sURL);
			xrtHttpRequestSetVerifyPeer(&tReq, false);
			pResp = xrtHttpExecuteSync(NULL, &tReq, &iStatus);
			printf("  HTTP sync status : %s\n", iStatus == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  HTTP sync HTTPS body : %s\n", pResp && pResp->iBodyLen == 6 && memcmp(pResp->pBody, "secure", 6) == 0 ? "PASS" : "FAIL");
			printf("  HTTP sync HTTPS code : %s\n", pResp && pResp->iStatusCode == 200 ? "PASS" : "FAIL");
			printf("  HTTP sync hidden engine path : %s\n", xrtNetSyncGetHiddenEngine() != NULL ? "PASS" : "FAIL");
			printf("  HTTP TLS server saw target : %s\n", __Test_XHttpWaitMin(&tServer.iRecvCount, 1, 2000) && strcmp(tServer.aLastTarget, "/secure") == 0 ? "PASS" : "FAIL");
			printf("  HTTP TLS path error free : %s\n", __Test_XHttpAtomicLoad(&tServer.iErrorCount) == 0 ? "PASS" : "FAIL");
			if ( pResp ) xrtHttpResponseDestroy(pResp);
			xrtHttpRequestUnit(&tReq);
			xrtHttpCloseIdleConnections(xrtNetSyncGetHiddenEngine());
			xrtNetSyncShutdownHiddenEngine();
			__Test_XHttpServerStop(&tServer);
		} else {
			printf("  HTTP TLS fixture files missing : SKIP\n");
		}
	}
}
