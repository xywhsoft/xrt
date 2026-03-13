#include "../lib/xhttpd2.h"
#include "../lib/xhttp2.h"

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
} __test_xhttpd2_ctx;

static void __Test_XHttpd2SleepMs(uint32 iDelayMs)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(iDelayMs);
	#else
		usleep((useconds_t)iDelayMs * 1000u);
	#endif
}

static long __Test_XHttpd2AtomicInc(volatile long* pValue)
{
	#if defined(_WIN32) || defined(_WIN64)
		return InterlockedIncrement((volatile LONG*)pValue);
	#else
		return __sync_add_and_fetch(pValue, 1);
	#endif
}

static long __Test_XHttpd2AtomicLoad(volatile long* pValue)
{
	#if defined(_WIN32) || defined(_WIN64)
		return InterlockedCompareExchange((volatile LONG*)pValue, 0, 0);
	#else
		return __sync_add_and_fetch(pValue, 0);
	#endif
}

static bool __Test_XHttpd2WaitMin(volatile long* pValue, long iExpectMin, uint32 iTimeoutMs)
{
	uint32 iLoops = (iTimeoutMs / 10u) + 1u;
	for ( uint32 i = 0; i < iLoops; ++i ) {
		if ( __Test_XHttpd2AtomicLoad(pValue) >= iExpectMin ) return true;
		__Test_XHttpd2SleepMs(10);
	}
	return __Test_XHttpd2AtomicLoad(pValue) >= iExpectMin;
}

static bool __Test_XHttpd2FileExists(const char* sPath)
{
	FILE* pFile = fopen(sPath, "rb");
	if ( !pFile ) return false;
	fclose(pFile);
	return true;
}

static void __Test_XHttpd2CloseSocket(xsocket hSocket)
{
	if ( hSocket == XNET_SOCKET_INVALID ) return;
	#if defined(_WIN32) || defined(_WIN64)
		closesocket(hSocket);
	#else
		close(hSocket);
	#endif
}

static xsocket __Test_XHttpd2ConnectLoopback(uint16 iPort)
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
		__Test_XHttpd2CloseSocket(hSocket);
		return XNET_SOCKET_INVALID;
	}
	return hSocket;
}

static bool __Test_XHttpd2SendAll(xsocket hSocket, const char* pData, size_t iLen)
{
	size_t iSent = 0u;
	while ( iSent < iLen ) {
		int iStep = send(hSocket, pData + iSent, (int)(iLen - iSent), 0);
		if ( iStep <= 0 ) return false;
		iSent += (size_t)iStep;
	}
	return true;
}

static bool __Test_XHttpd2TryParseResponse(const char* pBuf, size_t iLen, size_t* pTotalLen)
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
		iBodyLen = iDigits > 0u ? (size_t)strtoull(aDigits, NULL, 10) : 0u;
	}
	if ( iLen < iHeaderLen + iBodyLen ) return false;
	*pTotalLen = iHeaderLen + iBodyLen;
	return true;
}

static bool __Test_XHttpd2RecvResponse(xsocket hSocket, char* pBuf, size_t iCap, size_t* pOutLen, uint32 iTimeoutMs)
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
			if ( __Test_XHttpd2TryParseResponse(pBuf, iLen, &iTotalLen) ) {
				if ( pOutLen ) *pOutLen = iTotalLen;
				pBuf[iTotalLen] = '\0';
				return true;
			}
			continue;
		}
		if ( iReady < 0 ) break;
		iElapsedMs += 50u;
	}
	if ( __Test_XHttpd2TryParseResponse(pBuf, iLen, &iTotalLen) ) {
		if ( pOutLen ) *pOutLen = iTotalLen;
		pBuf[iTotalLen] = '\0';
		return true;
	}
	return false;
}

static void __Test_XHttpd2OnOpen(ptr pOwner, xhttpd2server* pServer, xhttpd2conn* pConn)
{
	__test_xhttpd2_ctx* pCtx = (__test_xhttpd2_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	if ( !pCtx ) return;
	__Test_XHttpd2AtomicInc(&pCtx->iOpenCount);
}

static bool __Test_XHttpd2OnRequest(ptr pOwner, xhttpd2server* pServer, xhttpd2conn* pConn, const xhttpd2request* pReq, xhttpd2response* pResp)
{
	__test_xhttpd2_ctx* pCtx = (__test_xhttpd2_ctx*)pOwner;
	const char* sHost;
	(void)pServer;
	(void)pConn;
	if ( !pCtx || !pReq || !pResp ) return false;
	__Test_XHttpd2AtomicInc(&pCtx->iRequestCount);
	__xhttp2CopyToken(pCtx->aLastMethod, sizeof(pCtx->aLastMethod), pReq->sMethod);
	__xhttp2CopyToken(pCtx->aLastPath, sizeof(pCtx->aLastPath), pReq->sPath);
	__xhttp2CopyToken(pCtx->aLastQuery, sizeof(pCtx->aLastQuery), pReq->sQuery);
	sHost = xrtHttpd2RequestHeader(pReq, "Host");
	if ( sHost ) __xhttp2CopyToken(pCtx->aLastHost, sizeof(pCtx->aLastHost), sHost);
	memset(pCtx->aLastBody, 0, sizeof(pCtx->aLastBody));
	if ( pReq->pBody && pReq->iBodyLen > 0 ) {
		size_t iCopy = pReq->iBodyLen < (sizeof(pCtx->aLastBody) - 1u) ? pReq->iBodyLen : (sizeof(pCtx->aLastBody) - 1u);
		memcpy(pCtx->aLastBody, pReq->pBody, iCopy);
		pCtx->aLastBody[iCopy] = '\0';
	}
	if ( strcmp(pReq->sPath, "/echo") == 0 ) {
		xrtHttpd2ResponseSetStatus(pResp, 201u, NULL);
		(void)xrtHttpd2ResponseSetBodyCopy(pResp, pReq->pBody ? pReq->pBody : "", pReq->iBodyLen, "text/plain");
		(void)xrtHttpd2ResponseSetHeader(pResp, "X-Server", "xhttpd2-test");
		return true;
	}
	if ( strcmp(pReq->sPath, "/secure") == 0 ) {
		xrtHttpd2ResponseSetStatus(pResp, 200u, NULL);
		(void)xrtHttpd2ResponseSetBodyCopy(pResp, "secure", 6, "text/plain");
		return true;
	}
	if ( strcmp(pReq->sPath, "/chunked") == 0 ) {
		xrtHttpd2ResponseSetStatus(pResp, 200u, NULL);
		(void)xrtHttpd2ResponseSetHeader(pResp, "Transfer-Encoding", "chunked");
		(void)xrtHttpd2ResponseSetBodyCopy(pResp, "chunk-reply", 11, "text/plain");
		return true;
	}
	return false;
}

static void __Test_XHttpd2OnClose(ptr pOwner, xhttpd2server* pServer, xhttpd2conn* pConn, xnet_result iReason)
{
	__test_xhttpd2_ctx* pCtx = (__test_xhttpd2_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	(void)iReason;
	if ( !pCtx ) return;
	__Test_XHttpd2AtomicInc(&pCtx->iCloseCount);
}

static void __Test_XHttpd2OnError(ptr pOwner, xhttpd2server* pServer, xhttpd2conn* pConn, int iSysErr)
{
	__test_xhttpd2_ctx* pCtx = (__test_xhttpd2_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	(void)iSysErr;
	if ( !pCtx ) return;
	__Test_XHttpd2AtomicInc(&pCtx->iErrorCount);
}

void Test_XNet2_Httpd2(void)
{
	printf("\n\n\n------------------------------------\n\n XNet2 HTTP Server Skeleton Test:\n\n");

	{
		__test_xhttpd2_ctx tCtx;
		xhttpd2events tEvents;
		xhttpd2config tSrvCfg;
		xnetengineconfig tEngineCfg;
		xnetengine* pServerEngine = NULL;
		xnetengine* pClientEngine = NULL;
		xhttpd2server* pServer = NULL;
		xhttp2request tReq;
		xhttp2response* pResp = NULL;
		xnetfuture* pFuture = NULL;
		xnet_result iStatus = XRT_NET_ERROR;
		char sURL[256];

		memset(&tCtx, 0, sizeof(tCtx));
		memset(&tEvents, 0, sizeof(tEvents));
		tEvents.OnOpen = __Test_XHttpd2OnOpen;
		tEvents.OnRequest = __Test_XHttpd2OnRequest;
		tEvents.OnClose = __Test_XHttpd2OnClose;
		tEvents.OnError = __Test_XHttpd2OnError;

		xrtNetEngineConfigInit(&tEngineCfg);
		tEngineCfg.iWorkerCount = 1;
		pServerEngine = xrtNetEngineCreate(&tEngineCfg);
		pClientEngine = xrtNetEngineCreate(&tEngineCfg);
		printf("  HTTPD2 plain server engine create : %s\n", pServerEngine != NULL ? "PASS" : "FAIL");
		printf("  HTTPD2 plain client engine create : %s\n", pClientEngine != NULL ? "PASS" : "FAIL");
		if ( pServerEngine ) printf("  HTTPD2 plain server engine start : %s\n", xrtNetEngineStart(pServerEngine) == XRT_NET_OK ? "PASS" : "FAIL");
		if ( pClientEngine ) printf("  HTTPD2 plain client engine start : %s\n", xrtNetEngineStart(pClientEngine) == XRT_NET_OK ? "PASS" : "FAIL");

		xrtHttpd2ConfigInit(&tSrvCfg);
		(void)xrtNetAddrParse(&tSrvCfg.tBindAddr, "127.0.0.1", 0);
		pServer = (pServerEngine && pClientEngine) ? xrtHttpd2Create(pServerEngine, &tSrvCfg, &tEvents, &tCtx) : NULL;
		printf("  HTTPD2 plain server create : %s\n", pServer != NULL ? "PASS" : "FAIL");
		printf("  HTTPD2 plain server start : %s\n", pServer && xrtHttpd2Start(pServer) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  HTTPD2 plain bound port assigned : %s\n", pServer && xrtHttpd2BoundPort(pServer) > 0 ? "PASS" : "FAIL");

		xrtHttp2RequestInit(&tReq);
		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/echo?mode=plain", (unsigned)xrtHttpd2BoundPort(pServer));
		(void)xrtHttp2RequestSetMethod(&tReq, "POST");
		(void)xrtHttp2RequestSetURL(&tReq, sURL);
		(void)xrtHttp2RequestSetBodyCopy(&tReq, "hello", 5, "text/plain");
		pFuture = (pClientEngine && pServer) ? xrtHttp2ExecuteAsync(pClientEngine, &tReq) : NULL;
		printf("  HTTPD2 plain client future create : %s\n", pFuture != NULL ? "PASS" : "FAIL");
		printf("  HTTPD2 plain client future wait : %s\n",
			pFuture && (iStatus = xrtNetFutureWait(pFuture, 3000u)) == XRT_NET_OK ? "PASS" : "FAIL");
		pResp = (pFuture && iStatus == XRT_NET_OK) ? (xhttp2response*)xrtNetFutureValue(pFuture) : NULL;
		printf("  HTTPD2 plain response code : %s\n", pResp && pResp->iStatusCode == 201 ? "PASS" : "FAIL");
		printf("  HTTPD2 plain response body : %s\n", pResp && pResp->iBodyLen == 5 && memcmp(pResp->pBody, "hello", 5) == 0 ? "PASS" : "FAIL");
		printf("  HTTPD2 plain response header : %s\n",
			pResp && pResp->iHeaderCount > 0 && xrtHttp2ResponseHeader(pResp, "X-Server") != NULL ? "PASS" : "FAIL");
		printf("  HTTPD2 plain request callback : %s\n", __Test_XHttpd2WaitMin(&tCtx.iRequestCount, 1, 1000u) ? "PASS" : "FAIL");
		printf("  HTTPD2 plain saw method : %s\n", strcmp(tCtx.aLastMethod, "POST") == 0 ? "PASS" : "FAIL");
		printf("  HTTPD2 plain saw path : %s\n", strcmp(tCtx.aLastPath, "/echo") == 0 ? "PASS" : "FAIL");
		printf("  HTTPD2 plain saw query : %s\n", strcmp(tCtx.aLastQuery, "mode=plain") == 0 ? "PASS" : "FAIL");
		printf("  HTTPD2 plain saw host : %s\n", strstr(tCtx.aLastHost, "127.0.0.1") != NULL ? "PASS" : "FAIL");
		printf("  HTTPD2 plain saw body : %s\n", strcmp(tCtx.aLastBody, "hello") == 0 ? "PASS" : "FAIL");
		printf("  HTTPD2 plain open callback : %s\n", __Test_XHttpd2WaitMin(&tCtx.iOpenCount, 1, 1000u) ? "PASS" : "FAIL");

		{
			xhttp2request tChunkReq;
			xnetfuture* pChunkFuture = NULL;
			xhttp2response* pChunkResp = NULL;
			xnet_result iChunkStatus = XRT_NET_ERROR;
			snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/echo?mode=chunked-req", (unsigned)xrtHttpd2BoundPort(pServer));
			xrtHttp2RequestInit(&tChunkReq);
			(void)xrtHttp2RequestSetMethod(&tChunkReq, "POST");
			(void)xrtHttp2RequestSetURL(&tChunkReq, sURL);
			(void)xrtHttp2RequestSetHeader(&tChunkReq, "Transfer-Encoding", "chunked");
			(void)xrtHttp2RequestSetBodyCopy(&tChunkReq, "chunk-in", 8, "text/plain");
			pChunkFuture = (pClientEngine && pServer) ? xrtHttp2ExecuteAsync(pClientEngine, &tChunkReq) : NULL;
			printf("  HTTPD2 chunked request future create : %s\n", pChunkFuture != NULL ? "PASS" : "FAIL");
			printf("  HTTPD2 chunked request future wait : %s\n",
				pChunkFuture && (iChunkStatus = xrtNetFutureWait(pChunkFuture, 3000u)) == XRT_NET_OK ? "PASS" : "FAIL");
			pChunkResp = (pChunkFuture && iChunkStatus == XRT_NET_OK) ? (xhttp2response*)xrtNetFutureValue(pChunkFuture) : NULL;
			printf("  HTTPD2 chunked request response body : %s\n",
				pChunkResp && pChunkResp->iBodyLen == 8 && memcmp(pChunkResp->pBody, "chunk-in", 8) == 0 ? "PASS" : "FAIL");
			printf("  HTTPD2 chunked request server saw body : %s\n", strcmp(tCtx.aLastBody, "chunk-in") == 0 ? "PASS" : "FAIL");
			if ( pChunkResp ) xrtHttp2ResponseDestroy(pChunkResp);
			if ( pChunkFuture ) xrtNetFutureDestroy(pChunkFuture);
			xrtHttp2RequestUnit(&tChunkReq);
		}

		{
			xhttp2request tChunkRespReq;
			xnetfuture* pChunkRespFuture = NULL;
			xhttp2response* pChunkResp = NULL;
			xnet_result iChunkRespStatus = XRT_NET_ERROR;
			snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/chunked", (unsigned)xrtHttpd2BoundPort(pServer));
			xrtHttp2RequestInit(&tChunkRespReq);
			(void)xrtHttp2RequestSetURL(&tChunkRespReq, sURL);
			pChunkRespFuture = (pClientEngine && pServer) ? xrtHttp2ExecuteAsync(pClientEngine, &tChunkRespReq) : NULL;
			printf("  HTTPD2 chunked response future create : %s\n", pChunkRespFuture != NULL ? "PASS" : "FAIL");
			printf("  HTTPD2 chunked response future wait : %s\n",
				pChunkRespFuture && (iChunkRespStatus = xrtNetFutureWait(pChunkRespFuture, 3000u)) == XRT_NET_OK ? "PASS" : "FAIL");
			pChunkResp = (pChunkRespFuture && iChunkRespStatus == XRT_NET_OK) ? (xhttp2response*)xrtNetFutureValue(pChunkRespFuture) : NULL;
			printf("  HTTPD2 chunked response body : %s\n",
				pChunkResp && pChunkResp->iBodyLen == 11 && memcmp(pChunkResp->pBody, "chunk-reply", 11) == 0 ? "PASS" : "FAIL");
			printf("  HTTPD2 chunked response flag : %s\n",
				pChunkResp && (pChunkResp->iFlags & XHTTP2_RESP_F_CHUNKED) != 0 &&
				xrtHttp2ResponseHeader(pChunkResp, "Transfer-Encoding") != NULL ? "PASS" : "FAIL");
			if ( pChunkResp ) xrtHttp2ResponseDestroy(pChunkResp);
			if ( pChunkRespFuture ) xrtNetFutureDestroy(pChunkRespFuture);
			xrtHttp2RequestUnit(&tChunkRespReq);
		}

		xrtHttp2CloseIdleConnections(pClientEngine);
		printf("  HTTPD2 plain close callback : %s\n", __Test_XHttpd2WaitMin(&tCtx.iCloseCount, 1, 1000u) ? "PASS" : "FAIL");
		printf("  HTTPD2 plain path error free : %s\n", __Test_XHttpd2AtomicLoad(&tCtx.iErrorCount) == 0 ? "PASS" : "FAIL");

		{
			xsocket hRaw = XNET_SOCKET_INVALID;
			char sReq1[512];
			char sReq2[512];
			char aResp[2048];
			size_t iRespLen = 0u;
			long iOpenBefore = __Test_XHttpd2AtomicLoad(&tCtx.iOpenCount);
			long iReqBefore = __Test_XHttpd2AtomicLoad(&tCtx.iRequestCount);
			long iCloseBefore = __Test_XHttpd2AtomicLoad(&tCtx.iCloseCount);
			hRaw = pServer ? __Test_XHttpd2ConnectLoopback(xrtHttpd2BoundPort(pServer)) : XNET_SOCKET_INVALID;
			printf("  HTTPD2 plain keepalive raw connect : %s\n", hRaw != XNET_SOCKET_INVALID ? "PASS" : "FAIL");
			snprintf(sReq1, sizeof(sReq1),
				"POST /echo?mode=ka1 HTTP/1.1\r\n"
				"Host: 127.0.0.1:%u\r\n"
				"Connection: keep-alive\r\n"
				"Content-Type: text/plain\r\n"
				"Content-Length: 3\r\n\r\n"
				"one",
				(unsigned)xrtHttpd2BoundPort(pServer));
			snprintf(sReq2, sizeof(sReq2),
				"POST /echo?mode=ka2 HTTP/1.1\r\n"
				"Host: 127.0.0.1:%u\r\n"
				"Connection: close\r\n"
				"Content-Type: text/plain\r\n"
				"Content-Length: 3\r\n\r\n"
				"two",
				(unsigned)xrtHttpd2BoundPort(pServer));
			printf("  HTTPD2 plain keepalive request #1 send : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpd2SendAll(hRaw, sReq1, strlen(sReq1)) ? "PASS" : "FAIL");
			printf("  HTTPD2 plain keepalive response #1 recv : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpd2RecvResponse(hRaw, aResp, sizeof(aResp), &iRespLen, 2000u) ? "PASS" : "FAIL");
			printf("  HTTPD2 plain keepalive response #1 status : %s\n", strstr(aResp, "HTTP/1.1 201") != NULL ? "PASS" : "FAIL");
			printf("  HTTPD2 plain keepalive response #1 header : %s\n", strstr(aResp, "Connection: keep-alive") != NULL ? "PASS" : "FAIL");
			printf("  HTTPD2 plain keepalive response #1 body : %s\n", strstr(aResp, "\r\n\r\none") != NULL ? "PASS" : "FAIL");
			printf("  HTTPD2 plain keepalive request #2 send : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpd2SendAll(hRaw, sReq2, strlen(sReq2)) ? "PASS" : "FAIL");
			printf("  HTTPD2 plain keepalive response #2 recv : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpd2RecvResponse(hRaw, aResp, sizeof(aResp), &iRespLen, 2000u) ? "PASS" : "FAIL");
			printf("  HTTPD2 plain keepalive response #2 status : %s\n", strstr(aResp, "HTTP/1.1 201") != NULL ? "PASS" : "FAIL");
			printf("  HTTPD2 plain keepalive response #2 header : %s\n", strstr(aResp, "Connection: close") != NULL ? "PASS" : "FAIL");
			printf("  HTTPD2 plain keepalive response #2 body : %s\n", strstr(aResp, "\r\n\r\ntwo") != NULL ? "PASS" : "FAIL");
			printf("  HTTPD2 plain keepalive open once : %s\n", __Test_XHttpd2WaitMin(&tCtx.iOpenCount, iOpenBefore + 1, 1000u) && __Test_XHttpd2AtomicLoad(&tCtx.iOpenCount) == iOpenBefore + 1 ? "PASS" : "FAIL");
			printf("  HTTPD2 plain keepalive two requests : %s\n", __Test_XHttpd2WaitMin(&tCtx.iRequestCount, iReqBefore + 2, 1000u) ? "PASS" : "FAIL");
			printf("  HTTPD2 plain keepalive close callback : %s\n", __Test_XHttpd2WaitMin(&tCtx.iCloseCount, iCloseBefore + 1, 2000u) ? "PASS" : "FAIL");
			printf("  HTTPD2 plain keepalive final query : %s\n", strcmp(tCtx.aLastQuery, "mode=ka2") == 0 && strcmp(tCtx.aLastBody, "two") == 0 ? "PASS" : "FAIL");
			__Test_XHttpd2CloseSocket(hRaw);
		}

		if ( pResp ) xrtHttp2ResponseDestroy(pResp);
		if ( pFuture ) xrtNetFutureDestroy(pFuture);
		xrtHttp2RequestUnit(&tReq);
		if ( pServer ) xrtHttpd2Destroy(pServer);
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
		__test_xhttpd2_ctx tCtx;
		xhttpd2events tEvents;
		xhttpd2config tSrvCfg;
		xnetengineconfig tEngineCfg;
		xnetengine* pServerEngine = NULL;
		xhttpd2server* pServer = NULL;
		xhttp2request tReq;
		xhttp2response* pResp = NULL;
		xnet_result iStatus = XRT_NET_ERROR;
		char sURL[256];
		xtlsconfig tTlsCfg;

		memset(&tCtx, 0, sizeof(tCtx));
		memset(&tEvents, 0, sizeof(tEvents));
		memset(&tTlsCfg, 0, sizeof(tTlsCfg));
		tEvents.OnOpen = __Test_XHttpd2OnOpen;
		tEvents.OnRequest = __Test_XHttpd2OnRequest;
		tEvents.OnClose = __Test_XHttpd2OnClose;
		tEvents.OnError = __Test_XHttpd2OnError;
		tTlsCfg.sCertFile = "dev/xnet2_tls_test_cert.pem";
		tTlsCfg.sKeyFile = "dev/xnet2_tls_test_key.pem";

		xrtNetEngineConfigInit(&tEngineCfg);
		tEngineCfg.iWorkerCount = 1;
		pServerEngine = xrtNetEngineCreate(&tEngineCfg);
		printf("  HTTPD2 TLS fixture files exist : %s\n", __Test_XHttpd2FileExists(tTlsCfg.sCertFile) && __Test_XHttpd2FileExists(tTlsCfg.sKeyFile) ? "PASS" : "FAIL");
		printf("  HTTPD2 TLS server engine create : %s\n", pServerEngine != NULL ? "PASS" : "FAIL");
		if ( pServerEngine ) printf("  HTTPD2 TLS server engine start : %s\n", xrtNetEngineStart(pServerEngine) == XRT_NET_OK ? "PASS" : "FAIL");

		xrtHttpd2ConfigInit(&tSrvCfg);
		(void)xrtNetAddrParse(&tSrvCfg.tBindAddr, "127.0.0.1", 0);
		tSrvCfg.pTlsConfig = &tTlsCfg;
		pServer = pServerEngine ? xrtHttpd2Create(pServerEngine, &tSrvCfg, &tEvents, &tCtx) : NULL;
		printf("  HTTPD2 TLS server create : %s\n", pServer != NULL ? "PASS" : "FAIL");
		printf("  HTTPD2 TLS server start : %s\n", pServer && xrtHttpd2Start(pServer) == XRT_NET_OK ? "PASS" : "FAIL");

		xrtHttp2RequestInit(&tReq);
		snprintf(sURL, sizeof(sURL), "https://127.0.0.1:%u/secure", (unsigned)xrtHttpd2BoundPort(pServer));
		(void)xrtHttp2RequestSetURL(&tReq, sURL);
		xrtHttp2RequestSetVerifyPeer(&tReq, false);
		pResp = pServer ? xrtHttp2ExecuteSync(NULL, &tReq, &iStatus) : NULL;
		printf("  HTTPD2 TLS client status : %s\n", iStatus == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  HTTPD2 TLS response code : %s\n", pResp && pResp->iStatusCode == 200 ? "PASS" : "FAIL");
		printf("  HTTPD2 TLS response body : %s\n", pResp && pResp->iBodyLen == 6 && memcmp(pResp->pBody, "secure", 6) == 0 ? "PASS" : "FAIL");
		printf("  HTTPD2 TLS request callback : %s\n", __Test_XHttpd2WaitMin(&tCtx.iRequestCount, 1, 2000u) ? "PASS" : "FAIL");
		printf("  HTTPD2 TLS saw path : %s\n", strcmp(tCtx.aLastPath, "/secure") == 0 ? "PASS" : "FAIL");
		xrtHttp2CloseIdleConnections(xrtNetSyncGetHiddenEngine());
		printf("  HTTPD2 TLS close callback : %s\n", __Test_XHttpd2WaitMin(&tCtx.iCloseCount, 1, 2000u) ? "PASS" : "FAIL");
		printf("  HTTPD2 TLS path error free : %s\n", __Test_XHttpd2AtomicLoad(&tCtx.iErrorCount) == 0 ? "PASS" : "FAIL");

		if ( pResp ) xrtHttp2ResponseDestroy(pResp);
		xrtHttp2RequestUnit(&tReq);
		xrtNetSyncShutdownHiddenEngine();
		if ( pServer ) xrtHttpd2Destroy(pServer);
		if ( pServerEngine ) {
			xrtNetEngineStop(pServerEngine);
			xrtNetEngineDestroy(pServerEngine);
		}
	}
}
