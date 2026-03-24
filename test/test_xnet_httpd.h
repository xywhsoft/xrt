#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#else
	#include <sys/time.h>
	#include <unistd.h>
#endif

typedef struct {
	volatile long iOpenCount;
	volatile long iRequestCount;
	volatile long iCloseCount;
	volatile long iErrorCount;
	char aLastMethod[32];
	char aLastPath[256];
	char aLastQuery[256];
	char aLastHost[256];
	char aLastBody[256];
} __test_xhttpd_ctx;

static void __Test_XHttpdSleepMs(uint32 iDelayMs)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(iDelayMs);
	#else
		usleep((useconds_t)iDelayMs * 1000u);
	#endif
}

static long __Test_XHttpdAtomicInc(volatile long* pValue)
{
	#if defined(_WIN32) || defined(_WIN64)
		return InterlockedIncrement((volatile LONG*)pValue);
	#else
		return __sync_add_and_fetch(pValue, 1);
	#endif
}

static long __Test_XHttpdAtomicLoad(volatile long* pValue)
{
	#if defined(_WIN32) || defined(_WIN64)
		return InterlockedCompareExchange((volatile LONG*)pValue, 0, 0);
	#else
		return __sync_add_and_fetch(pValue, 0);
	#endif
}

static bool __Test_XHttpdWaitMin(volatile long* pValue, long iExpectMin, uint32 iTimeoutMs)
{
	uint32 iLoops = (iTimeoutMs / 10u) + 1u;
	for ( uint32 i = 0; i < iLoops; ++i ) {
		if ( __Test_XHttpdAtomicLoad(pValue) >= iExpectMin ) return true;
		__Test_XHttpdSleepMs(10);
	}
	return __Test_XHttpdAtomicLoad(pValue) >= iExpectMin;
}

static bool __Test_XHttpdFileExists(const char* sPath)
{
	FILE* pFile = fopen(sPath, "rb");
	if ( !pFile ) return false;
	fclose(pFile);
	return true;
}

static void __Test_XHttpdCloseSocket(xsocket hSocket)
{
	if ( hSocket == XNET_SOCKET_INVALID ) return;
	#if defined(_WIN32) || defined(_WIN64)
		closesocket(hSocket);
	#else
		close(hSocket);
	#endif
}

static xsocket __Test_XHttpdConnectLoopback(uint16 iPort)
{
	xsocket hSocket;
	struct sockaddr_in tAddr;
	hSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if ( hSocket == XNET_SOCKET_INVALID ) return XNET_SOCKET_INVALID;
	memset(&tAddr, 0, sizeof(tAddr));
	tAddr.sin_family = AF_INET;
	tAddr.sin_port = htons(iPort);
	tAddr.sin_addr.s_addr = htonl(0x7F000001u);
	if ( connect(hSocket, (struct sockaddr*)&tAddr, sizeof(tAddr)) != 0 ) {
		__Test_XHttpdCloseSocket(hSocket);
		return XNET_SOCKET_INVALID;
	}
	return hSocket;
}

static bool __Test_XHttpdSendAll(xsocket hSocket, const char* pData, size_t iLen)
{
	size_t iSent = 0u;
	while ( iSent < iLen ) {
		int iStep = send(hSocket, pData + iSent, (int)(iLen - iSent), 0);
		if ( iStep <= 0 ) return false;
		iSent += (size_t)iStep;
	}
	return true;
}

static size_t __Test_XHttpdParseSizeDec(const char* sText)
{
	size_t iValue = 0u;

	if ( !sText ) return 0u;

	while ( *sText >= '0' && *sText <= '9' ) {
		iValue = (iValue * 10u) + (size_t)(*sText - '0');
		sText++;
	}

	return iValue;
}

static bool __Test_XHttpdTryParseResponse(const char* pBuf, size_t iLen, size_t* pTotalLen)
{
	size_t iHeaderLen = 0u;
	size_t iBodyLen = 0u;
	char aDigits[32];
	size_t iDigits = 0u;
	const char* pCl;
	if ( !pBuf || !pTotalLen ) return false;
	for ( size_t i = 0; i + 3u < iLen; ++i ) {
		if ( pBuf[i] == '\r' && pBuf[i + 1u] == '\n' && pBuf[i + 2u] == '\r' && pBuf[i + 3u] == '\n' ) {
			iHeaderLen = i + 4u;
			break;
		}
	}
	if ( iHeaderLen == 0u ) return false;
	pCl = strstr(pBuf, "Content-Length:");
	if ( pCl && (size_t)(pCl - pBuf) < iHeaderLen ) {
		pCl += strlen("Content-Length:");
		while ( *pCl == ' ' || *pCl == '\t' ) pCl++;
		while ( *pCl >= '0' && *pCl <= '9' && iDigits + 1u < sizeof(aDigits) ) {
			aDigits[iDigits++] = *pCl++;
		}
		aDigits[iDigits] = '\0';
		iBodyLen = iDigits > 0u ? __Test_XHttpdParseSizeDec(aDigits) : 0u;
	}
	if ( iLen < iHeaderLen + iBodyLen ) return false;
	*pTotalLen = iHeaderLen + iBodyLen;
	return true;
}

static bool __Test_XHttpdRecvResponse(xsocket hSocket, char* pBuf, size_t iCap, size_t* pOutLen, uint32 iTimeoutMs)
{
	size_t iLen = 0u;
	size_t iTotalLen = 0u;
	uint32 iElapsedMs = 0u;
	if ( pOutLen ) *pOutLen = 0u;
	if ( !pBuf || iCap < 2u || hSocket == XNET_SOCKET_INVALID ) return false;
	pBuf[0] = '\0';
	while ( iElapsedMs <= iTimeoutMs && iLen + 1u < iCap ) {
		fd_set tReadSet;
		struct timeval tTv;
		int iReady;
		FD_ZERO(&tReadSet);
		FD_SET(hSocket, &tReadSet);
		tTv.tv_sec = 0;
		tTv.tv_usec = 50000;
		iReady = select((int)(hSocket + 1), &tReadSet, NULL, NULL, &tTv);
		if ( iReady > 0 && FD_ISSET(hSocket, &tReadSet) ) {
			int iStep = recv(hSocket, pBuf + iLen, (int)(iCap - 1u - iLen), 0);
			if ( iStep <= 0 ) break;
			iLen += (size_t)iStep;
			pBuf[iLen] = '\0';
			if ( __Test_XHttpdTryParseResponse(pBuf, iLen, &iTotalLen) ) {
				if ( pOutLen ) *pOutLen = iTotalLen;
				pBuf[iTotalLen] = '\0';
				return true;
			}
			continue;
		}
		if ( iReady < 0 ) break;
		iElapsedMs += 50u;
	}
	if ( __Test_XHttpdTryParseResponse(pBuf, iLen, &iTotalLen) ) {
		if ( pOutLen ) *pOutLen = iTotalLen;
		pBuf[iTotalLen] = '\0';
		return true;
	}
	return false;
}

static void __Test_XHttpdOnOpen(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn)
{
	__test_xhttpd_ctx* pCtx = (__test_xhttpd_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	if ( !pCtx ) return;
	__Test_XHttpdAtomicInc(&pCtx->iOpenCount);
}

static bool __Test_XHttpdOnRequest(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq, xhttpdresponse* pResp)
{
	__test_xhttpd_ctx* pCtx = (__test_xhttpd_ctx*)pOwner;
	const char* sHost;
	(void)pServer;
	(void)pConn;
	if ( !pCtx || !pReq || !pResp ) return false;
	__Test_XHttpdAtomicInc(&pCtx->iRequestCount);
	__xhttpCopyToken(pCtx->aLastMethod, sizeof(pCtx->aLastMethod), pReq->sMethod);
	__xhttpCopyToken(pCtx->aLastPath, sizeof(pCtx->aLastPath), pReq->sPath);
	__xhttpCopyToken(pCtx->aLastQuery, sizeof(pCtx->aLastQuery), pReq->sQuery);
	sHost = xrtHttpdRequestHeader(pReq, "Host");
	if ( sHost ) __xhttpCopyToken(pCtx->aLastHost, sizeof(pCtx->aLastHost), sHost);
	memset(pCtx->aLastBody, 0, sizeof(pCtx->aLastBody));
	if ( pReq->pBody && pReq->iBodyLen > 0 ) {
		size_t iCopy = pReq->iBodyLen < (sizeof(pCtx->aLastBody) - 1u) ? pReq->iBodyLen : (sizeof(pCtx->aLastBody) - 1u);
		memcpy(pCtx->aLastBody, pReq->pBody, iCopy);
		pCtx->aLastBody[iCopy] = '\0';
	}
	if ( strcmp(pReq->sPath, "/echo") == 0 ) {
		xrtHttpdResponseSetStatus(pResp, 201u, NULL);
		(void)xrtHttpdResponseSetBodyCopy(pResp, pReq->pBody ? pReq->pBody : "", pReq->iBodyLen, "text/plain");
		(void)xrtHttpdResponseSetHeader(pResp, "X-Server", "xhttpd-test");
		return true;
	}
	if ( strcmp(pReq->sPath, "/secure") == 0 ) {
		xrtHttpdResponseSetStatus(pResp, 200u, NULL);
		(void)xrtHttpdResponseSetBodyCopy(pResp, "secure", 6, "text/plain");
		return true;
	}
	if ( strcmp(pReq->sPath, "/chunked") == 0 ) {
		xrtHttpdResponseSetStatus(pResp, 200u, NULL);
		(void)xrtHttpdResponseSetHeader(pResp, "Transfer-Encoding", "chunked");
		(void)xrtHttpdResponseSetBodyCopy(pResp, "chunk-reply", 11, "text/plain");
		return true;
	}
	return false;
}

static void __Test_XHttpdOnClose(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, xnet_result iReason)
{
	__test_xhttpd_ctx* pCtx = (__test_xhttpd_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	(void)iReason;
	if ( !pCtx ) return;
	__Test_XHttpdAtomicInc(&pCtx->iCloseCount);
}

static void __Test_XHttpdOnError(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, int iSysErr)
{
	__test_xhttpd_ctx* pCtx = (__test_xhttpd_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	(void)iSysErr;
	if ( !pCtx ) return;
	__Test_XHttpdAtomicInc(&pCtx->iErrorCount);
}

void Test_XNet_Httpd(void)
{
	printf("\n\n\n------------------------------------\n\n XNet HTTP Server Skeleton Test:\n\n");

	{
		__test_xhttpd_ctx tCtx;
		xhttpdevents tEvents;
		xhttpdconfig tSrvCfg;
		xnetengineconfig tEngineCfg;
		xnetengine* pServerEngine = NULL;
		xnetengine* pClientEngine = NULL;
		xhttpdserver* pServer = NULL;
		xhttprequest tReq;
		xhttpresponse* pResp = NULL;
		xnetfuture* pFuture = NULL;
		xnet_result iStatus = XRT_NET_ERROR;
		char sURL[256];

		memset(&tCtx, 0, sizeof(tCtx));
		memset(&tEvents, 0, sizeof(tEvents));
		tEvents.OnOpen = __Test_XHttpdOnOpen;
		tEvents.OnRequest = __Test_XHttpdOnRequest;
		tEvents.OnClose = __Test_XHttpdOnClose;
		tEvents.OnError = __Test_XHttpdOnError;

		xrtNetEngineConfigInit(&tEngineCfg);
		tEngineCfg.iWorkerCount = 1;
		pServerEngine = xrtNetEngineCreate(&tEngineCfg);
		pClientEngine = xrtNetEngineCreate(&tEngineCfg);
		printf("  HTTPD plain server engine create : %s\n", pServerEngine != NULL ? "PASS" : "FAIL");
		printf("  HTTPD plain client engine create : %s\n", pClientEngine != NULL ? "PASS" : "FAIL");
		if ( pServerEngine ) printf("  HTTPD plain server engine start : %s\n", xrtNetEngineStart(pServerEngine) == XRT_NET_OK ? "PASS" : "FAIL");
		if ( pClientEngine ) printf("  HTTPD plain client engine start : %s\n", xrtNetEngineStart(pClientEngine) == XRT_NET_OK ? "PASS" : "FAIL");

		xrtHttpdConfigInit(&tSrvCfg);
		(void)xrtNetAddrParse(&tSrvCfg.tBindAddr, "127.0.0.1", 0);
		pServer = (pServerEngine && pClientEngine) ? xrtHttpdCreate(pServerEngine, &tSrvCfg, &tEvents, &tCtx) : NULL;
		printf("  HTTPD plain server create : %s\n", pServer != NULL ? "PASS" : "FAIL");
		printf("  HTTPD plain server start : %s\n", pServer && xrtHttpdStart(pServer) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  HTTPD plain bound port assigned : %s\n", pServer && xrtHttpdBoundPort(pServer) > 0 ? "PASS" : "FAIL");

		xrtHttpRequestInit(&tReq);
		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/echo?mode=plain", (unsigned)xrtHttpdBoundPort(pServer));
		(void)xrtHttpRequestSetMethod(&tReq, "POST");
		(void)xrtHttpRequestSetURL(&tReq, sURL);
		(void)xrtHttpRequestSetBodyCopy(&tReq, "hello", 5, "text/plain");
		pFuture = (pClientEngine && pServer) ? xrtHttpExecuteAsync(pClientEngine, &tReq) : NULL;
		printf("  HTTPD plain client future create : %s\n", pFuture != NULL ? "PASS" : "FAIL");
		printf("  HTTPD plain client future wait : %s\n",
			pFuture && (iStatus = xrtNetFutureWait(pFuture, 3000u)) == XRT_NET_OK ? "PASS" : "FAIL");
		pResp = (pFuture && iStatus == XRT_NET_OK) ? (xhttpresponse*)xrtNetFutureValue(pFuture) : NULL;
		printf("  HTTPD plain response code : %s\n", pResp && pResp->iStatusCode == 201 ? "PASS" : "FAIL");
		printf("  HTTPD plain response body : %s\n", pResp && pResp->iBodyLen == 5 && memcmp(pResp->pBody, "hello", 5) == 0 ? "PASS" : "FAIL");
		printf("  HTTPD plain response header : %s\n",
			pResp && pResp->iHeaderCount > 0 && xrtHttpResponseHeader(pResp, "X-Server") != NULL ? "PASS" : "FAIL");
		printf("  HTTPD plain request callback : %s\n", __Test_XHttpdWaitMin(&tCtx.iRequestCount, 1, 1000u) ? "PASS" : "FAIL");
		printf("  HTTPD plain saw method : %s\n", strcmp(tCtx.aLastMethod, "POST") == 0 ? "PASS" : "FAIL");
		printf("  HTTPD plain saw path : %s\n", strcmp(tCtx.aLastPath, "/echo") == 0 ? "PASS" : "FAIL");
		printf("  HTTPD plain saw query : %s\n", strcmp(tCtx.aLastQuery, "mode=plain") == 0 ? "PASS" : "FAIL");
		printf("  HTTPD plain saw host : %s\n", strstr(tCtx.aLastHost, "127.0.0.1") != NULL ? "PASS" : "FAIL");
		printf("  HTTPD plain saw body : %s\n", strcmp(tCtx.aLastBody, "hello") == 0 ? "PASS" : "FAIL");
		printf("  HTTPD plain open callback : %s\n", __Test_XHttpdWaitMin(&tCtx.iOpenCount, 1, 1000u) ? "PASS" : "FAIL");

		{
			xhttprequest tChunkReq;
			xnetfuture* pChunkFuture = NULL;
			xhttpresponse* pChunkResp = NULL;
			xnet_result iChunkStatus = XRT_NET_ERROR;
			snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/echo?mode=chunked-req", (unsigned)xrtHttpdBoundPort(pServer));
			xrtHttpRequestInit(&tChunkReq);
			(void)xrtHttpRequestSetMethod(&tChunkReq, "POST");
			(void)xrtHttpRequestSetURL(&tChunkReq, sURL);
			(void)xrtHttpRequestSetHeader(&tChunkReq, "Transfer-Encoding", "chunked");
			(void)xrtHttpRequestSetBodyCopy(&tChunkReq, "chunk-in", 8, "text/plain");
			pChunkFuture = (pClientEngine && pServer) ? xrtHttpExecuteAsync(pClientEngine, &tChunkReq) : NULL;
			printf("  HTTPD chunked request future create : %s\n", pChunkFuture != NULL ? "PASS" : "FAIL");
			printf("  HTTPD chunked request future wait : %s\n",
				pChunkFuture && (iChunkStatus = xrtNetFutureWait(pChunkFuture, 3000u)) == XRT_NET_OK ? "PASS" : "FAIL");
			pChunkResp = (pChunkFuture && iChunkStatus == XRT_NET_OK) ? (xhttpresponse*)xrtNetFutureValue(pChunkFuture) : NULL;
			printf("  HTTPD chunked request response body : %s\n",
				pChunkResp && pChunkResp->iBodyLen == 8 && memcmp(pChunkResp->pBody, "chunk-in", 8) == 0 ? "PASS" : "FAIL");
			printf("  HTTPD chunked request server saw body : %s\n", strcmp(tCtx.aLastBody, "chunk-in") == 0 ? "PASS" : "FAIL");
			if ( pChunkResp ) xrtHttpResponseDestroy(pChunkResp);
			if ( pChunkFuture ) xrtNetFutureDestroy(pChunkFuture);
			xrtHttpRequestUnit(&tChunkReq);
		}

		{
			xhttprequest tChunkRespReq;
			xnetfuture* pChunkRespFuture = NULL;
			xhttpresponse* pChunkResp = NULL;
			xnet_result iChunkRespStatus = XRT_NET_ERROR;
			snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/chunked", (unsigned)xrtHttpdBoundPort(pServer));
			xrtHttpRequestInit(&tChunkRespReq);
			(void)xrtHttpRequestSetURL(&tChunkRespReq, sURL);
			pChunkRespFuture = (pClientEngine && pServer) ? xrtHttpExecuteAsync(pClientEngine, &tChunkRespReq) : NULL;
			printf("  HTTPD chunked response future create : %s\n", pChunkRespFuture != NULL ? "PASS" : "FAIL");
			printf("  HTTPD chunked response future wait : %s\n",
				pChunkRespFuture && (iChunkRespStatus = xrtNetFutureWait(pChunkRespFuture, 3000u)) == XRT_NET_OK ? "PASS" : "FAIL");
			pChunkResp = (pChunkRespFuture && iChunkRespStatus == XRT_NET_OK) ? (xhttpresponse*)xrtNetFutureValue(pChunkRespFuture) : NULL;
			printf("  HTTPD chunked response body : %s\n",
				pChunkResp && pChunkResp->iBodyLen == 11 && memcmp(pChunkResp->pBody, "chunk-reply", 11) == 0 ? "PASS" : "FAIL");
			printf("  HTTPD chunked response flag : %s\n",
				pChunkResp && (pChunkResp->iFlags & XHTTP_RESP_F_CHUNKED) != 0 &&
				xrtHttpResponseHeader(pChunkResp, "Transfer-Encoding") != NULL ? "PASS" : "FAIL");
			if ( pChunkResp ) xrtHttpResponseDestroy(pChunkResp);
			if ( pChunkRespFuture ) xrtNetFutureDestroy(pChunkRespFuture);
			xrtHttpRequestUnit(&tChunkRespReq);
		}

		xrtHttpCloseIdleConnections(pClientEngine);
		printf("  HTTPD plain close callback : %s\n", __Test_XHttpdWaitMin(&tCtx.iCloseCount, 1, 1000u) ? "PASS" : "FAIL");
		printf("  HTTPD plain path error free : %s\n", __Test_XHttpdAtomicLoad(&tCtx.iErrorCount) == 0 ? "PASS" : "FAIL");

		{
			xsocket hRaw = XNET_SOCKET_INVALID;
			char sReq[512];
			char aResp[2048];
			size_t iRespLen = 0u;
			long iOpenBefore = __Test_XHttpdAtomicLoad(&tCtx.iOpenCount);
			long iReqBefore = __Test_XHttpdAtomicLoad(&tCtx.iRequestCount);
			long iCloseBefore = __Test_XHttpdAtomicLoad(&tCtx.iCloseCount);
			hRaw = pServer ? __Test_XHttpdConnectLoopback(xrtHttpdBoundPort(pServer)) : XNET_SOCKET_INVALID;
			snprintf(sReq, sizeof(sReq),
				"POST /echo?mode=plain-reaccept HTTP/1.1\r\n"
				"Host: 127.0.0.1:%u\r\n"
				"Connection: close\r\n"
				"Content-Type: text/plain\r\n"
				"Content-Length: 5\r\n\r\n"
				"again",
				(unsigned)xrtHttpdBoundPort(pServer));
			printf("  HTTPD plain reaccept raw connect : %s\n", hRaw != XNET_SOCKET_INVALID ? "PASS" : "FAIL");
			printf("  HTTPD plain reaccept request send : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdSendAll(hRaw, sReq, strlen(sReq)) ? "PASS" : "FAIL");
			printf("  HTTPD plain reaccept response recv : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdRecvResponse(hRaw, aResp, sizeof(aResp), &iRespLen, 2000u) ? "PASS" : "FAIL");
			printf("  HTTPD plain reaccept response code : %s\n", strstr(aResp, "HTTP/1.1 201") != NULL ? "PASS" : "FAIL");
			printf("  HTTPD plain reaccept response body : %s\n", strstr(aResp, "\r\n\r\nagain") != NULL ? "PASS" : "FAIL");
			printf("  HTTPD plain reaccept request callback : %s\n", __Test_XHttpdWaitMin(&tCtx.iRequestCount, iReqBefore + 1, 1000u) ? "PASS" : "FAIL");
			printf("  HTTPD plain reaccept open callback : %s\n", __Test_XHttpdWaitMin(&tCtx.iOpenCount, iOpenBefore + 1, 1000u) ? "PASS" : "FAIL");
			printf("  HTTPD plain reaccept close callback : %s\n", __Test_XHttpdWaitMin(&tCtx.iCloseCount, iCloseBefore + 1, 2000u) ? "PASS" : "FAIL");
			printf("  HTTPD plain reaccept final query : %s\n", strcmp(tCtx.aLastQuery, "mode=plain-reaccept") == 0 && strcmp(tCtx.aLastBody, "again") == 0 ? "PASS" : "FAIL");
			__Test_XHttpdCloseSocket(hRaw);
		}

		{
			xsocket hRaw = XNET_SOCKET_INVALID;
			char sReq1[512];
			char sReq2[512];
			char aResp[2048];
			size_t iRespLen = 0u;
			long iOpenBefore = __Test_XHttpdAtomicLoad(&tCtx.iOpenCount);
			long iReqBefore = __Test_XHttpdAtomicLoad(&tCtx.iRequestCount);
			long iCloseBefore = __Test_XHttpdAtomicLoad(&tCtx.iCloseCount);
			hRaw = pServer ? __Test_XHttpdConnectLoopback(xrtHttpdBoundPort(pServer)) : XNET_SOCKET_INVALID;
			printf("  HTTPD plain keepalive raw connect : %s\n", hRaw != XNET_SOCKET_INVALID ? "PASS" : "FAIL");
			snprintf(sReq1, sizeof(sReq1),
				"POST /echo?mode=ka1 HTTP/1.1\r\n"
				"Host: 127.0.0.1:%u\r\n"
				"Connection: keep-alive\r\n"
				"Content-Type: text/plain\r\n"
				"Content-Length: 3\r\n\r\n"
				"one",
				(unsigned)xrtHttpdBoundPort(pServer));
			snprintf(sReq2, sizeof(sReq2),
				"POST /echo?mode=ka2 HTTP/1.1\r\n"
				"Host: 127.0.0.1:%u\r\n"
				"Connection: close\r\n"
				"Content-Type: text/plain\r\n"
				"Content-Length: 3\r\n\r\n"
				"two",
				(unsigned)xrtHttpdBoundPort(pServer));
			printf("  HTTPD plain keepalive request #1 send : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdSendAll(hRaw, sReq1, strlen(sReq1)) ? "PASS" : "FAIL");
			printf("  HTTPD plain keepalive response #1 recv : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdRecvResponse(hRaw, aResp, sizeof(aResp), &iRespLen, 2000u) ? "PASS" : "FAIL");
			printf("  HTTPD plain keepalive response #1 status : %s\n", strstr(aResp, "HTTP/1.1 201") != NULL ? "PASS" : "FAIL");
			printf("  HTTPD plain keepalive response #1 header : %s\n", strstr(aResp, "Connection: keep-alive") != NULL ? "PASS" : "FAIL");
			printf("  HTTPD plain keepalive response #1 body : %s\n", strstr(aResp, "\r\n\r\none") != NULL ? "PASS" : "FAIL");
			printf("  HTTPD plain keepalive request #2 send : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdSendAll(hRaw, sReq2, strlen(sReq2)) ? "PASS" : "FAIL");
			printf("  HTTPD plain keepalive response #2 recv : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdRecvResponse(hRaw, aResp, sizeof(aResp), &iRespLen, 2000u) ? "PASS" : "FAIL");
			printf("  HTTPD plain keepalive response #2 status : %s\n", strstr(aResp, "HTTP/1.1 201") != NULL ? "PASS" : "FAIL");
			printf("  HTTPD plain keepalive response #2 header : %s\n", strstr(aResp, "Connection: close") != NULL ? "PASS" : "FAIL");
			printf("  HTTPD plain keepalive response #2 body : %s\n", strstr(aResp, "\r\n\r\ntwo") != NULL ? "PASS" : "FAIL");
			printf("  HTTPD plain keepalive open once : %s\n", __Test_XHttpdWaitMin(&tCtx.iOpenCount, iOpenBefore + 1, 1000u) && __Test_XHttpdAtomicLoad(&tCtx.iOpenCount) == iOpenBefore + 1 ? "PASS" : "FAIL");
			printf("  HTTPD plain keepalive two requests : %s\n", __Test_XHttpdWaitMin(&tCtx.iRequestCount, iReqBefore + 2, 1000u) ? "PASS" : "FAIL");
			printf("  HTTPD plain keepalive close callback : %s\n", __Test_XHttpdWaitMin(&tCtx.iCloseCount, iCloseBefore + 1, 2000u) ? "PASS" : "FAIL");
			printf("  HTTPD plain keepalive final query : %s\n", strcmp(tCtx.aLastQuery, "mode=ka2") == 0 && strcmp(tCtx.aLastBody, "two") == 0 ? "PASS" : "FAIL");
			__Test_XHttpdCloseSocket(hRaw);
		}

		if ( pResp ) xrtHttpResponseDestroy(pResp);
		if ( pFuture ) xrtNetFutureDestroy(pFuture);
		xrtHttpRequestUnit(&tReq);
		if ( pServer ) xrtHttpdDestroy(pServer);
		if ( pClientEngine ) {
			xrtNetEngineStop(pClientEngine);
			xrtNetEngineDestroy(pClientEngine);
		}
		if ( pServerEngine ) {
			xrtNetEngineStop(pServerEngine);
			xrtNetEngineDestroy(pServerEngine);
		}
	}

	{
		__test_xhttpd_ctx tCtx;
		xhttpdevents tEvents;
		xhttpdconfig tSrvCfg;
		xnetengineconfig tEngineCfg;
		xnetengine* pServerEngine = NULL;
		xhttpdserver* pServer = NULL;
		xhttprequest tReq;
		xhttpresponse* pResp = NULL;
		xnet_result iStatus = XRT_NET_ERROR;
		char sURL[256];
		xtlsconfig tTlsCfg;
		bool bHasTlsFixtures;

		memset(&tCtx, 0, sizeof(tCtx));
		memset(&tEvents, 0, sizeof(tEvents));
		memset(&tTlsCfg, 0, sizeof(tTlsCfg));
		tEvents.OnOpen = __Test_XHttpdOnOpen;
		tEvents.OnRequest = __Test_XHttpdOnRequest;
		tEvents.OnClose = __Test_XHttpdOnClose;
		tEvents.OnError = __Test_XHttpdOnError;
		tTlsCfg.sCertFile = "dev/xnet2_tls_test_cert.pem";
		tTlsCfg.sKeyFile = "dev/xnet2_tls_test_key.pem";
		bHasTlsFixtures = __Test_XHttpdFileExists(tTlsCfg.sCertFile) && __Test_XHttpdFileExists(tTlsCfg.sKeyFile);

		if ( !bHasTlsFixtures ) {
			printf("  HTTPD TLS fixture files missing : SKIP\n");
		} else {
			printf("  HTTPD TLS fixture files exist : PASS\n");
			xrtNetEngineConfigInit(&tEngineCfg);
			tEngineCfg.iWorkerCount = 1;
			pServerEngine = xrtNetEngineCreate(&tEngineCfg);
			printf("  HTTPD TLS server engine create : %s\n", pServerEngine != NULL ? "PASS" : "FAIL");
			if ( pServerEngine ) printf("  HTTPD TLS server engine start : %s\n", xrtNetEngineStart(pServerEngine) == XRT_NET_OK ? "PASS" : "FAIL");

			xrtHttpdConfigInit(&tSrvCfg);
			(void)xrtNetAddrParse(&tSrvCfg.tBindAddr, "127.0.0.1", 0);
			tSrvCfg.pTlsConfig = &tTlsCfg;
			pServer = pServerEngine ? xrtHttpdCreate(pServerEngine, &tSrvCfg, &tEvents, &tCtx) : NULL;
			printf("  HTTPD TLS server create : %s\n", pServer != NULL ? "PASS" : "FAIL");
			printf("  HTTPD TLS server start : %s\n", pServer && xrtHttpdStart(pServer) == XRT_NET_OK ? "PASS" : "FAIL");

			xrtHttpRequestInit(&tReq);
			snprintf(sURL, sizeof(sURL), "https://127.0.0.1:%u/secure", (unsigned)xrtHttpdBoundPort(pServer));
			(void)xrtHttpRequestSetURL(&tReq, sURL);
			xrtHttpRequestSetVerifyPeer(&tReq, false);
			pResp = pServer ? xrtHttpExecuteSync(NULL, &tReq, &iStatus) : NULL;
			printf("  HTTPD TLS client status : %s\n", iStatus == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  HTTPD TLS response code : %s\n", pResp && pResp->iStatusCode == 200 ? "PASS" : "FAIL");
			printf("  HTTPD TLS response body : %s\n", pResp && pResp->iBodyLen == 6 && memcmp(pResp->pBody, "secure", 6) == 0 ? "PASS" : "FAIL");
			printf("  HTTPD TLS request callback : %s\n", __Test_XHttpdWaitMin(&tCtx.iRequestCount, 1, 2000u) ? "PASS" : "FAIL");
			printf("  HTTPD TLS saw path : %s\n", strcmp(tCtx.aLastPath, "/secure") == 0 ? "PASS" : "FAIL");
			xrtHttpCloseIdleConnections(xrtNetSyncGetHiddenEngine());
			printf("  HTTPD TLS close callback : %s\n", __Test_XHttpdWaitMin(&tCtx.iCloseCount, 1, 2000u) ? "PASS" : "FAIL");
			printf("  HTTPD TLS path error free : %s\n", __Test_XHttpdAtomicLoad(&tCtx.iErrorCount) == 0 ? "PASS" : "FAIL");
		}

		if ( pResp ) xrtHttpResponseDestroy(pResp);
		xrtHttpRequestUnit(&tReq);
		xrtNetSyncShutdownHiddenEngine();
		if ( pServer ) xrtHttpdDestroy(pServer);
		if ( pServerEngine ) {
			xrtNetEngineStop(pServerEngine);
			xrtNetEngineDestroy(pServerEngine);
		}
	}
}
