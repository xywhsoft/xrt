#include "../lib/xhttp2.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#else
	#include <pthread.h>
	#include <unistd.h>
#endif

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
#if defined(_WIN32) || defined(_WIN64)
	HANDLE hThread;
#else
	pthread_t hThread;
	bool bThreadStarted;
#endif
} __test_xhttp2_server;

static void __Test_XHttp2SleepMs(uint32 iDelayMs)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(iDelayMs);
	#else
		usleep((useconds_t)iDelayMs * 1000u);
	#endif
}

static long __Test_XHttp2AtomicInc(volatile long* pValue)
{
	#if defined(_WIN32) || defined(_WIN64)
		return InterlockedIncrement((volatile LONG*)pValue);
	#else
		return __sync_add_and_fetch(pValue, 1);
	#endif
}

static long __Test_XHttp2AtomicLoad(volatile long* pValue)
{
	#if defined(_WIN32) || defined(_WIN64)
		return InterlockedCompareExchange((volatile LONG*)pValue, 0, 0);
	#else
		return __sync_add_and_fetch(pValue, 0);
	#endif
}

static void __Test_XHttp2AtomicStore(volatile long* pValue, long iValue)
{
	#if defined(_WIN32) || defined(_WIN64)
		InterlockedExchange((volatile LONG*)pValue, iValue);
	#else
		__sync_lock_test_and_set(pValue, iValue);
	#endif
}

static bool __Test_XHttp2WaitMin(volatile long* pValue, long iExpectMin, uint32 iTimeoutMs)
{
	uint32 iLoops = (iTimeoutMs / 10u) + 1u;
	for ( uint32 i = 0; i < iLoops; ++i ) {
		if ( __Test_XHttp2AtomicLoad(pValue) >= iExpectMin ) return true;
		__Test_XHttp2SleepMs(10);
	}
	return __Test_XHttp2AtomicLoad(pValue) >= iExpectMin;
}

static bool __Test_XHttp2FileExists(const char* sPath)
{
	FILE* pFile = fopen(sPath, "rb");
	if ( !pFile ) return false;
	fclose(pFile);
	return true;
}

static bool __Test_XHttp2ServerOnAccept(ptr pOwner, xnetlistener* pListener, xnetstream* pStream)
{
	__test_xhttp2_server* pServer = (__test_xhttp2_server*)pOwner;
	(void)pListener;
	if ( !pServer ) return false;
	pServer->pAccepted = pStream;
	__Test_XHttp2AtomicInc(&pServer->iAcceptCount);
	return true;
}

static void __Test_XHttp2ServerOnOpen(ptr pOwner, xnetstream* pStream)
{
	__test_xhttp2_server* pServer = (__test_xhttp2_server*)pOwner;
	(void)pStream;
	if ( !pServer ) return;
	__Test_XHttp2AtomicInc(&pServer->iOpenCount);
}

static void __Test_XHttp2ServerOnRecv(ptr pOwner, xnetstream* pStream, xnetchain* pChain)
{
	__test_xhttp2_server* pServer = (__test_xhttp2_server*)pOwner;
	xcodecframe tFrame;
	xcodechttp1msg tMsg;
	size_t iBodyBytes;
	if ( !pServer || !pStream || !pChain ) return;
	if ( xrtCodecHttp1Parse(pChain, &tFrame, &tMsg) != XCODEC_STATUS_FRAME ) return;
	__Test_XHttp2AtomicInc(&pServer->iRecvCount);
	__xhttp2CopyToken(pServer->aLastMethod, sizeof(pServer->aLastMethod), tMsg.sMethod);
	__xhttp2CopyToken(pServer->aLastTarget, sizeof(pServer->aLastTarget), tMsg.sTarget);
	{
		const char* sHost = xrtCodecHttp1GetHeader(&tMsg, "Host");
		if ( sHost ) __xhttp2CopyToken(pServer->aLastHost, sizeof(pServer->aLastHost), sHost);
	}
	memset(pServer->aLastBody, 0, sizeof(pServer->aLastBody));
	iBodyBytes = xrtCodecHttp1BodyBytes(&tFrame);
	if ( iBodyBytes > 0u ) {
		size_t iCopy = iBodyBytes < (sizeof(pServer->aLastBody) - 1u) ? iBodyBytes : (sizeof(pServer->aLastBody) - 1u);
		(void)xrtCodecHttp1CopyBody(pChain, &tFrame, pServer->aLastBody, iCopy);
	}
	xrtCodecFrameConsume(pChain, &tFrame);
	(void)xrtNetStreamSend(pStream, pServer->aResponse, pServer->iResponseLen);
}

static void __Test_XHttp2ServerOnClose(ptr pOwner, xnetstream* pStream, xnet_result iReason)
{
	__test_xhttp2_server* pServer = (__test_xhttp2_server*)pOwner;
	(void)pStream;
	(void)iReason;
	if ( !pServer ) return;
	__Test_XHttp2AtomicInc(&pServer->iCloseCount);
}

static void __Test_XHttp2ServerOnError(ptr pOwner, xnetstream* pStream, int iSysErr)
{
	__test_xhttp2_server* pServer = (__test_xhttp2_server*)pOwner;
	(void)pStream;
	(void)iSysErr;
	if ( !pServer ) return;
	__Test_XHttp2AtomicInc(&pServer->iErrorCount);
}

static const xnetlistenerevents* __Test_XHttp2ListenerEvents(void)
{
	static const xnetlistenerevents tEvents = {
		__Test_XHttp2ServerOnAccept,
		NULL
	};
	return &tEvents;
}

static const xnetstreamevents* __Test_XHttp2StreamEvents(void)
{
	static const xnetstreamevents tEvents = {
		__Test_XHttp2ServerOnOpen,
		__Test_XHttp2ServerOnRecv,
		NULL,
		__Test_XHttp2ServerOnClose,
		__Test_XHttp2ServerOnError,
		NULL,
		NULL
	};
	return &tEvents;
}

#if defined(_WIN32) || defined(_WIN64)
static DWORD WINAPI __Test_XHttp2AcceptThread(LPVOID pArg)
#else
static void* __Test_XHttp2AcceptThread(void* pArg)
#endif
{
	__test_xhttp2_server* pServer = (__test_xhttp2_server*)pArg;
	while ( pServer && __Test_XHttp2AtomicLoad(&pServer->bRun) != 0 ) {
		if ( pServer->pListener && pServer->pListener->bRunning ) {
			(void)__xnetListenerTryAcceptOne(pServer->pListener, pServer);
		}
		__Test_XHttp2SleepMs(5);
	}
	#if !defined(_WIN32) && !defined(_WIN64)
		return NULL;
	#endif
}

static bool __Test_XHttp2ServerStart(__test_xhttp2_server* pServer, const char* sResponse, const xtlsconfig* pTlsCfg)
{
	xnetengineconfig tEngineCfg;
	xnetlistenconfig tListenCfg;
	if ( !pServer || !sResponse ) return false;
	memset(pServer, 0, sizeof(__test_xhttp2_server));
	xrtNetEngineConfigInit(&tEngineCfg);
	tEngineCfg.iWorkerCount = 1;
	pServer->pEngine = xrtNetEngineCreate(&tEngineCfg);
	if ( !pServer->pEngine ) return false;
	if ( xrtNetEngineStart(pServer->pEngine) != XRT_NET_OK ) return false;
	xrtNetListenConfigInit(&tListenCfg);
	xrtNetAddrParse(&tListenCfg.tBindAddr, "127.0.0.1", 0);
	tListenCfg.pTlsConfig = pTlsCfg;
	pServer->pListener = xrtNetListenerCreate(pServer->pEngine, &tListenCfg,
		__Test_XHttp2ListenerEvents(), __Test_XHttp2StreamEvents(), pServer);
	if ( !pServer->pListener ) return false;
	if ( xrtNetListenerStart(pServer->pListener) != XRT_NET_OK ) return false;
	__xhttp2CopyToken(pServer->aResponse, sizeof(pServer->aResponse), sResponse);
	pServer->iResponseLen = (uint32)strlen(pServer->aResponse);
	__Test_XHttp2AtomicStore(&pServer->bRun, 1);
	#if defined(_WIN32) || defined(_WIN64)
		pServer->hThread = CreateThread(NULL, 0, __Test_XHttp2AcceptThread, pServer, 0, NULL);
		if ( !pServer->hThread ) return false;
	#else
		if ( pthread_create(&pServer->hThread, NULL, __Test_XHttp2AcceptThread, pServer) != 0 ) return false;
		pServer->bThreadStarted = true;
	#endif
	return true;
}

static void __Test_XHttp2ServerStop(__test_xhttp2_server* pServer)
{
	if ( !pServer ) return;
	__Test_XHttp2AtomicStore(&pServer->bRun, 0);
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
		if ( __Test_XHttp2AtomicLoad(&pServer->iCloseCount) == 0 ) {
			xrtNetStreamClose(pServer->pAccepted, XNET_CLOSE_F_ABORT);
			__Test_XHttp2SleepMs(20);
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

void Test_XNet2_Http2(void)
{
	printf("\n\n\n------------------------------------\n\n XNet2 HTTP Client Skeleton Test:\n\n");

	{
		xhttp2request tReq;
		char* pBuilt = NULL;
		size_t iBuiltLen = 0;
		xrtHttp2RequestInit(&tReq);
		(void)xrtHttp2RequestSetMethod(&tReq, "POST");
		(void)xrtHttp2RequestSetURL(&tReq, "http://127.0.0.1:8080/api");
		(void)xrtHttp2RequestSetHeader(&tReq, "User-Agent", "xhttp2-test");
		(void)xrtHttp2RequestSetBodyCopy(&tReq, "hello", 5, "text/plain");
		printf("  HTTP2 build request bytes : %s\n", __xhttp2BuildRequestBytes(&tReq, &pBuilt, &iBuiltLen) ? "PASS" : "FAIL");
		printf("  HTTP2 build has request line : %s\n", pBuilt && strstr(pBuilt, "POST /api HTTP/1.1\r\n") != NULL ? "PASS" : "FAIL");
		printf("  HTTP2 build has host header : %s\n", pBuilt && strstr(pBuilt, "Host: 127.0.0.1:8080\r\n") != NULL ? "PASS" : "FAIL");
		printf("  HTTP2 build has connection keep-alive : %s\n", pBuilt && strstr(pBuilt, "Connection: keep-alive\r\n") != NULL ? "PASS" : "FAIL");
		printf("  HTTP2 build has content length : %s\n", pBuilt && strstr(pBuilt, "Content-Length: 5\r\n") != NULL ? "PASS" : "FAIL");
		printf("  HTTP2 build has body : %s\n", pBuilt && iBuiltLen >= 5 && memcmp(pBuilt + iBuiltLen - 5, "hello", 5) == 0 ? "PASS" : "FAIL");
		if ( pBuilt ) XNET_FREE(pBuilt);
		xrtHttp2RequestUnit(&tReq);
	}

	{
		xhttp2request tReq;
		char* pBuilt = NULL;
		size_t iBuiltLen = 0;
		xrtHttp2RequestInit(&tReq);
		(void)xrtHttp2RequestSetMethod(&tReq, "POST");
		(void)xrtHttp2RequestSetURL(&tReq, "http://127.0.0.1:8080/chunked");
		(void)xrtHttp2RequestSetHeader(&tReq, "Transfer-Encoding", "chunked");
		(void)xrtHttp2RequestSetBodyCopy(&tReq, "hello", 5, "text/plain");
		printf("  HTTP2 build chunked request bytes : %s\n", __xhttp2BuildRequestBytes(&tReq, &pBuilt, &iBuiltLen) ? "PASS" : "FAIL");
		printf("  HTTP2 build chunked transfer header : %s\n", pBuilt && strstr(pBuilt, "Transfer-Encoding: chunked\r\n") != NULL ? "PASS" : "FAIL");
		printf("  HTTP2 build chunked omits content length : %s\n", pBuilt && strstr(pBuilt, "Content-Length:") == NULL ? "PASS" : "FAIL");
		printf("  HTTP2 build chunked body framing : %s\n",
			pBuilt && strstr(pBuilt, "\r\n\r\n5\r\nhello\r\n0\r\n\r\n") != NULL ? "PASS" : "FAIL");
		if ( pBuilt ) XNET_FREE(pBuilt);
		xrtHttp2RequestUnit(&tReq);
	}

	{
		__test_xhttp2_server tServer;
		xnetengineconfig tCfg;
		xnetengine* pClientEngine = NULL;
		xhttp2request tReq;
		xnetfuture* pFuture = NULL;
		xhttp2response* pResp = NULL;
		xnet_result iStatus = XRT_NET_ERROR;
		char sURL[256];
		printf("  HTTP2 plain server start : %s\n", __Test_XHttp2ServerStart(&tServer, "HTTP/1.1 200 OK\r\nContent-Length: 5\r\nConnection: keep-alive\r\n\r\nworld", NULL) ? "PASS" : "FAIL");
		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/hello", (unsigned)tServer.pListener->tConfig.tBindAddr.iPort);
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pClientEngine = xrtNetEngineCreate(&tCfg);
		if ( pClientEngine ) (void)xrtNetEngineStart(pClientEngine);
		xrtHttp2RequestInit(&tReq);
		(void)xrtHttp2RequestSetURL(&tReq, sURL);
		(void)xrtHttp2RequestSetHeader(&tReq, "User-Agent", "xhttp2-async");
		pFuture = xrtHttp2ExecuteAsync(pClientEngine, &tReq);
		printf("  HTTP2 async future create : %s\n", pFuture != NULL ? "PASS" : "FAIL");
		printf("  HTTP2 async future wait : %s\n", pFuture && (iStatus = xrtNetFutureWait(pFuture, 3000)) == XRT_NET_OK ? "PASS" : "FAIL");
		pResp = (pFuture && iStatus == XRT_NET_OK) ? (xhttp2response*)xrtNetFutureValue(pFuture) : NULL;
		printf("  HTTP2 async response status : %s\n", pResp && pResp->iStatusCode == 200 ? "PASS" : "FAIL");
		printf("  HTTP2 async response body : %s\n", pResp && pResp->iBodyLen == 5 && memcmp(pResp->pBody, "world", 5) == 0 ? "PASS" : "FAIL");
		printf("  HTTP2 server accepted request : %s\n", __Test_XHttp2WaitMin(&tServer.iAcceptCount, 1, 1000) ? "PASS" : "FAIL");
		printf("  HTTP2 server saw method : %s\n", strcmp(tServer.aLastMethod, "GET") == 0 ? "PASS" : "FAIL");
		printf("  HTTP2 server saw target : %s\n", strcmp(tServer.aLastTarget, "/hello") == 0 ? "PASS" : "FAIL");
		printf("  HTTP2 server saw host : %s\n", strstr(tServer.aLastHost, "127.0.0.1") != NULL ? "PASS" : "FAIL");
		printf("  HTTP2 plain path error free : %s\n", __Test_XHttp2AtomicLoad(&tServer.iErrorCount) == 0 ? "PASS" : "FAIL");
		if ( pResp ) xrtHttp2ResponseDestroy(pResp);
		if ( pFuture ) xrtNetFutureDestroy(pFuture);
		xrtHttp2RequestUnit(&tReq);
		xrtHttp2CloseIdleConnections(pClientEngine);
		if ( pClientEngine ) {
			xrtNetEngineStop(pClientEngine);
			xrtNetEngineDestroy(pClientEngine);
		}
		__Test_XHttp2ServerStop(&tServer);
	}

	{
		__test_xhttp2_server tServer;
		xnetengineconfig tCfg;
		xnetengine* pClientEngine = NULL;
		xhttp2request tReq1;
		xhttp2request tReq2;
		xnetfuture* pFuture1 = NULL;
		xnetfuture* pFuture2 = NULL;
		xhttp2response* pResp1 = NULL;
		xhttp2response* pResp2 = NULL;
		xnet_result iStatus1 = XRT_NET_ERROR;
		xnet_result iStatus2 = XRT_NET_ERROR;
		char sURL1[256];
		char sURL2[256];
		printf("  HTTP2 keepalive server start : %s\n", __Test_XHttp2ServerStart(&tServer, "HTTP/1.1 200 OK\r\nContent-Length: 4\r\nConnection: keep-alive\r\n\r\nkeep", NULL) ? "PASS" : "FAIL");
		snprintf(sURL1, sizeof(sURL1), "http://127.0.0.1:%u/reuse-a", (unsigned)tServer.pListener->tConfig.tBindAddr.iPort);
		snprintf(sURL2, sizeof(sURL2), "http://127.0.0.1:%u/reuse-b", (unsigned)tServer.pListener->tConfig.tBindAddr.iPort);
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pClientEngine = xrtNetEngineCreate(&tCfg);
		if ( pClientEngine ) (void)xrtNetEngineStart(pClientEngine);
		xrtHttp2RequestInit(&tReq1);
		xrtHttp2RequestInit(&tReq2);
		(void)xrtHttp2RequestSetURL(&tReq1, sURL1);
		(void)xrtHttp2RequestSetURL(&tReq2, sURL2);
		pFuture1 = xrtHttp2ExecuteAsync(pClientEngine, &tReq1);
		printf("  HTTP2 keepalive future #1 create : %s\n", pFuture1 != NULL ? "PASS" : "FAIL");
		printf("  HTTP2 keepalive future #1 wait : %s\n", pFuture1 && (iStatus1 = xrtNetFutureWait(pFuture1, 3000)) == XRT_NET_OK ? "PASS" : "FAIL");
		pResp1 = (pFuture1 && iStatus1 == XRT_NET_OK) ? (xhttp2response*)xrtNetFutureValue(pFuture1) : NULL;
		printf("  HTTP2 keepalive response #1 : %s\n", pResp1 && pResp1->iBodyLen == 4 && memcmp(pResp1->pBody, "keep", 4) == 0 ? "PASS" : "FAIL");
		pFuture2 = xrtHttp2ExecuteAsync(pClientEngine, &tReq2);
		printf("  HTTP2 keepalive future #2 create : %s\n", pFuture2 != NULL ? "PASS" : "FAIL");
		printf("  HTTP2 keepalive future #2 wait : %s\n", pFuture2 && (iStatus2 = xrtNetFutureWait(pFuture2, 3000)) == XRT_NET_OK ? "PASS" : "FAIL");
		pResp2 = (pFuture2 && iStatus2 == XRT_NET_OK) ? (xhttp2response*)xrtNetFutureValue(pFuture2) : NULL;
		printf("  HTTP2 keepalive response #2 : %s\n", pResp2 && pResp2->iBodyLen == 4 && memcmp(pResp2->pBody, "keep", 4) == 0 ? "PASS" : "FAIL");
		printf("  HTTP2 keepalive single accept : %s\n", __Test_XHttp2AtomicLoad(&tServer.iAcceptCount) == 1 ? "PASS" : "FAIL");
		printf("  HTTP2 keepalive single open : %s\n", __Test_XHttp2AtomicLoad(&tServer.iOpenCount) == 1 ? "PASS" : "FAIL");
		printf("  HTTP2 keepalive two requests : %s\n", __Test_XHttp2AtomicLoad(&tServer.iRecvCount) == 2 ? "PASS" : "FAIL");
		printf("  HTTP2 keepalive second target : %s\n", strcmp(tServer.aLastTarget, "/reuse-b") == 0 ? "PASS" : "FAIL");
		printf("  HTTP2 keepalive server stays error free : %s\n", __Test_XHttp2AtomicLoad(&tServer.iErrorCount) == 0 ? "PASS" : "FAIL");
		if ( pResp1 ) xrtHttp2ResponseDestroy(pResp1);
		if ( pResp2 ) xrtHttp2ResponseDestroy(pResp2);
		if ( pFuture1 ) xrtNetFutureDestroy(pFuture1);
		if ( pFuture2 ) xrtNetFutureDestroy(pFuture2);
		xrtHttp2RequestUnit(&tReq1);
		xrtHttp2RequestUnit(&tReq2);
		xrtHttp2CloseIdleConnections(pClientEngine);
		if ( pClientEngine ) {
			xrtNetEngineStop(pClientEngine);
			xrtNetEngineDestroy(pClientEngine);
		}
		__Test_XHttp2ServerStop(&tServer);
	}

	{
		__test_xhttp2_server tServer;
		xnetengineconfig tCfg;
		xnetengine* pClientEngine = NULL;
		xhttp2request tReq1;
		xhttp2request tReq2;
		xnetfuture* pFuture1 = NULL;
		xnetfuture* pFuture2 = NULL;
		xhttp2response* pResp1 = NULL;
		xhttp2response* pResp2 = NULL;
		xnet_result iStatus1 = XRT_NET_ERROR;
		xnet_result iStatus2 = XRT_NET_ERROR;
		char sURL1[256];
		char sURL2[256];
		printf("  HTTP2 chunked keepalive server start : %s\n",
			__Test_XHttp2ServerStart(&tServer,
				"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: keep-alive\r\n\r\n5\r\nchunk\r\n3\r\ned!\r\n0\r\n\r\n",
				NULL) ? "PASS" : "FAIL");
		snprintf(sURL1, sizeof(sURL1), "http://127.0.0.1:%u/chunk-a", (unsigned)tServer.pListener->tConfig.tBindAddr.iPort);
		snprintf(sURL2, sizeof(sURL2), "http://127.0.0.1:%u/chunk-b", (unsigned)tServer.pListener->tConfig.tBindAddr.iPort);
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pClientEngine = xrtNetEngineCreate(&tCfg);
		if ( pClientEngine ) (void)xrtNetEngineStart(pClientEngine);
		xrtHttp2RequestInit(&tReq1);
		xrtHttp2RequestInit(&tReq2);
		(void)xrtHttp2RequestSetURL(&tReq1, sURL1);
		(void)xrtHttp2RequestSetURL(&tReq2, sURL2);
		pFuture1 = xrtHttp2ExecuteAsync(pClientEngine, &tReq1);
		printf("  HTTP2 chunked future #1 create : %s\n", pFuture1 != NULL ? "PASS" : "FAIL");
		printf("  HTTP2 chunked future #1 wait : %s\n", pFuture1 && (iStatus1 = xrtNetFutureWait(pFuture1, 3000)) == XRT_NET_OK ? "PASS" : "FAIL");
		pResp1 = (pFuture1 && iStatus1 == XRT_NET_OK) ? (xhttp2response*)xrtNetFutureValue(pFuture1) : NULL;
		printf("  HTTP2 chunked response #1 body : %s\n", pResp1 && pResp1->iBodyLen == 8 && memcmp(pResp1->pBody, "chunked!", 8) == 0 ? "PASS" : "FAIL");
		printf("  HTTP2 chunked response #1 flag : %s\n", pResp1 && (pResp1->iFlags & XHTTP2_RESP_F_CHUNKED) != 0 ? "PASS" : "FAIL");
		pFuture2 = xrtHttp2ExecuteAsync(pClientEngine, &tReq2);
		printf("  HTTP2 chunked future #2 create : %s\n", pFuture2 != NULL ? "PASS" : "FAIL");
		printf("  HTTP2 chunked future #2 wait : %s\n", pFuture2 && (iStatus2 = xrtNetFutureWait(pFuture2, 3000)) == XRT_NET_OK ? "PASS" : "FAIL");
		pResp2 = (pFuture2 && iStatus2 == XRT_NET_OK) ? (xhttp2response*)xrtNetFutureValue(pFuture2) : NULL;
		printf("  HTTP2 chunked response #2 body : %s\n", pResp2 && pResp2->iBodyLen == 8 && memcmp(pResp2->pBody, "chunked!", 8) == 0 ? "PASS" : "FAIL");
		printf("  HTTP2 chunked keepalive single accept : %s\n", __Test_XHttp2AtomicLoad(&tServer.iAcceptCount) == 1 ? "PASS" : "FAIL");
		printf("  HTTP2 chunked keepalive two requests : %s\n", __Test_XHttp2AtomicLoad(&tServer.iRecvCount) == 2 ? "PASS" : "FAIL");
		printf("  HTTP2 chunked keepalive second target : %s\n", strcmp(tServer.aLastTarget, "/chunk-b") == 0 ? "PASS" : "FAIL");
		if ( pResp1 ) xrtHttp2ResponseDestroy(pResp1);
		if ( pResp2 ) xrtHttp2ResponseDestroy(pResp2);
		if ( pFuture1 ) xrtNetFutureDestroy(pFuture1);
		if ( pFuture2 ) xrtNetFutureDestroy(pFuture2);
		xrtHttp2RequestUnit(&tReq1);
		xrtHttp2RequestUnit(&tReq2);
		xrtHttp2CloseIdleConnections(pClientEngine);
		if ( pClientEngine ) {
			xrtNetEngineStop(pClientEngine);
			xrtNetEngineDestroy(pClientEngine);
		}
		__Test_XHttp2ServerStop(&tServer);
	}

	{
		__test_xhttp2_server tServer;
		xnetengineconfig tCfg;
		xnetengine* pClientEngine = NULL;
		xhttp2request tReq;
		xnetfuture* pFuture = NULL;
		xhttp2response* pResp = NULL;
		xnet_result iStatus = XRT_NET_ERROR;
		char sURL[256];
		printf("  HTTP2 chunked request server start : %s\n",
			__Test_XHttp2ServerStart(&tServer, "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok", NULL) ? "PASS" : "FAIL");
		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/ingest", (unsigned)tServer.pListener->tConfig.tBindAddr.iPort);
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pClientEngine = xrtNetEngineCreate(&tCfg);
		if ( pClientEngine ) (void)xrtNetEngineStart(pClientEngine);
		xrtHttp2RequestInit(&tReq);
		(void)xrtHttp2RequestSetMethod(&tReq, "POST");
		(void)xrtHttp2RequestSetURL(&tReq, sURL);
		(void)xrtHttp2RequestSetHeader(&tReq, "Transfer-Encoding", "chunked");
		(void)xrtHttp2RequestSetBodyCopy(&tReq, "hello-chunk", 11, "text/plain");
		pFuture = xrtHttp2ExecuteAsync(pClientEngine, &tReq);
		printf("  HTTP2 chunked request future create : %s\n", pFuture != NULL ? "PASS" : "FAIL");
		printf("  HTTP2 chunked request future wait : %s\n", pFuture && (iStatus = xrtNetFutureWait(pFuture, 3000)) == XRT_NET_OK ? "PASS" : "FAIL");
		pResp = (pFuture && iStatus == XRT_NET_OK) ? (xhttp2response*)xrtNetFutureValue(pFuture) : NULL;
		printf("  HTTP2 chunked request response code : %s\n", pResp && pResp->iStatusCode == 200 ? "PASS" : "FAIL");
		printf("  HTTP2 chunked request server saw body : %s\n", strcmp(tServer.aLastBody, "hello-chunk") == 0 ? "PASS" : "FAIL");
		if ( pResp ) xrtHttp2ResponseDestroy(pResp);
		if ( pFuture ) xrtNetFutureDestroy(pFuture);
		xrtHttp2RequestUnit(&tReq);
		if ( pClientEngine ) {
			xrtNetEngineStop(pClientEngine);
			xrtNetEngineDestroy(pClientEngine);
		}
		__Test_XHttp2ServerStop(&tServer);
	}

	{
		__test_xhttp2_server tServer;
		xhttp2request tReq;
		xhttp2response* pResp = NULL;
		xnet_result iStatus = XRT_NET_ERROR;
		char sURL[256];
		xtlsconfig tTlsCfg;
		memset(&tTlsCfg, 0, sizeof(tTlsCfg));
		tTlsCfg.sCertFile = "dev/xnet2_tls_test_cert.pem";
		tTlsCfg.sKeyFile = "dev/xnet2_tls_test_key.pem";
		printf("  HTTP2 TLS fixture files exist : %s\n", __Test_XHttp2FileExists(tTlsCfg.sCertFile) && __Test_XHttp2FileExists(tTlsCfg.sKeyFile) ? "PASS" : "FAIL");
		printf("  HTTP2 TLS server start : %s\n", __Test_XHttp2ServerStart(&tServer, "HTTP/1.1 200 OK\r\nContent-Length: 6\r\nConnection: keep-alive\r\n\r\nsecure", &tTlsCfg) ? "PASS" : "FAIL");
		snprintf(sURL, sizeof(sURL), "https://127.0.0.1:%u/secure", (unsigned)tServer.pListener->tConfig.tBindAddr.iPort);
		xrtHttp2RequestInit(&tReq);
		(void)xrtHttp2RequestSetURL(&tReq, sURL);
		xrtHttp2RequestSetVerifyPeer(&tReq, false);
		pResp = xrtHttp2ExecuteSync(NULL, &tReq, &iStatus);
		printf("  HTTP2 sync status : %s\n", iStatus == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  HTTP2 sync HTTPS body : %s\n", pResp && pResp->iBodyLen == 6 && memcmp(pResp->pBody, "secure", 6) == 0 ? "PASS" : "FAIL");
		printf("  HTTP2 sync HTTPS code : %s\n", pResp && pResp->iStatusCode == 200 ? "PASS" : "FAIL");
		printf("  HTTP2 sync hidden engine path : %s\n", xrtNetSyncGetHiddenEngine() != NULL ? "PASS" : "FAIL");
		printf("  HTTP2 TLS server saw target : %s\n", __Test_XHttp2WaitMin(&tServer.iRecvCount, 1, 2000) && strcmp(tServer.aLastTarget, "/secure") == 0 ? "PASS" : "FAIL");
		printf("  HTTP2 TLS path error free : %s\n", __Test_XHttp2AtomicLoad(&tServer.iErrorCount) == 0 ? "PASS" : "FAIL");
		if ( pResp ) xrtHttp2ResponseDestroy(pResp);
		xrtHttp2RequestUnit(&tReq);
		xrtHttp2CloseIdleConnections(xrtNetSyncGetHiddenEngine());
		xrtNetSyncShutdownHiddenEngine();
		__Test_XHttp2ServerStop(&tServer);
	}
}
