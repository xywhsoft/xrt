#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#else
	#include <sys/time.h>
	#include <unistd.h>
#endif

#include "test_atomic_compat.h"

typedef struct {
	volatile long iOpenCount;
	volatile long iRequestCount;
	volatile long iCloseCount;
	volatile long iErrorCount;
	volatile long iAsyncFutureCount;
	volatile long iAsyncManualCount;
	volatile long iAsyncCoroutineCount;
	volatile long iBodyBeginCount;
	volatile long iBodyChunkCount;
	volatile long iBodyEndCount;
	volatile long iBodySawStreamingFlag;
	uint32 iLastMethod;
	char aLastMethod[32];
	char aLastPath[256];
	char aLastQuery[256];
	char aLastHost[256];
	char aLastBody[256];
	char aStreamBody[512];
	size_t iStreamBodyLen;
	#if !defined(XRT_NO_COROUTINE)
		xcosched* pSched;
	#endif
} __test_xhttpd_ctx;

typedef struct {
	__test_xhttpd_ctx* pCtx;
	char aBody[128];
	uint32 iStatusCode;
	uint32 iDelayMs;
	char aHeaderValue[32];
	bool bFail;
} __test_xhttpd_async_future_task;

typedef struct {
	__test_xhttpd_ctx* pCtx;
	xhttpdconn* pConn;
	char aBody[128];
	bool bClose;
} __test_xhttpd_async_manual_task;


// 内部函数：__Test_XHttpdSleepMs
static void __Test_XHttpdSleepMs(uint32 iDelayMs)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(iDelayMs);
	#else
		usleep((useconds_t)iDelayMs * 1000u);
	#endif
}


// 内部函数：__Test_XHttpdAtomicInc
static long __Test_XHttpdAtomicInc(volatile long* pValue)
{
	return __xrtTestAtomicAddFetchLong(pValue, 1);
}


// 内部函数：__Test_XHttpdAtomicLoad
static long __Test_XHttpdAtomicLoad(volatile long* pValue)
{
	return __xrtTestAtomicLoadLong(pValue);
}


// 内部函数：__Test_XHttpdWaitMin
static bool __Test_XHttpdWaitMin(volatile long* pValue, long iExpectMin, uint32 iTimeoutMs)
{
	uint32 iLoops = (iTimeoutMs / 10u) + 1u;
	for ( uint32 i = 0; i < iLoops; ++i ) {
		if ( __Test_XHttpdAtomicLoad(pValue) >= iExpectMin ) return true;
		__Test_XHttpdSleepMs(10);
	}
	return __Test_XHttpdAtomicLoad(pValue) >= iExpectMin;
}


// 内部函数：__Test_XHttpdFileExists
static bool __Test_XHttpdFileExists(const char* sPath)
{
	FILE* pFile = fopen(sPath, "rb");
	if ( !pFile ) return false;
	fclose(pFile);
	return true;
}


// 内部函数：__Test_XHttpdCloseSocket
static void __Test_XHttpdCloseSocket(xsocket hSocket)
{
	if ( hSocket == XNET_SOCKET_INVALID ) return;
	#if defined(_WIN32) || defined(_WIN64)
		closesocket(hSocket);
	#else
		close(hSocket);
	#endif
}


// 内部函数：__Test_XHttpdConnectLoopback
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


// 内部函数：__Test_XHttpdSendAll
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


// 内部函数：__Test_XHttpdParseSizeDec
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


// 内部函数：__Test_XHttpdTryParseResponse
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


// 内部函数：__Test_XHttpdRecvResponse
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


// 内部函数：__Test_XHttpdRecordRequest
static void __Test_XHttpdRecordRequest(__test_xhttpd_ctx* pCtx, const xhttpdrequest* pReq)
{
	const char* sHost;
	if ( !pCtx || !pReq ) return;
	__Test_XHttpdAtomicInc(&pCtx->iRequestCount);
	pCtx->iLastMethod = xrtHttpdRequestMethod(pReq);
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
}


// 内部函数：__Test_XHttpdOnOpen
static void __Test_XHttpdOnOpen(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn)
{
	__test_xhttpd_ctx* pCtx = (__test_xhttpd_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	if ( !pCtx ) return;
	__Test_XHttpdAtomicInc(&pCtx->iOpenCount);
}


// 内部函数：__Test_XHttpdOnRequest
static bool __Test_XHttpdOnRequest(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq, xhttpdresponse* pResp)
{
	__test_xhttpd_ctx* pCtx = (__test_xhttpd_ctx*)pOwner;
	(void)pServer;
	if ( !pCtx || !pReq || !pResp ) return false;
	__Test_XHttpdRecordRequest(pCtx, pReq);
	if ( strcmp(pReq->sPath, "/echo") == 0 ) {
		xrtHttpdResponseSetStatus(pResp, 201u, NULL);
		(void)xrtHttpdResponseSetBodyCopy(pResp, pReq->pBody ? pReq->pBody : "", pReq->iBodyLen, "text/plain");
		(void)xrtHttpdResponseSetHeader(pResp, "X-Server", "xhttpd-test");
		return true;
	}
	if ( strcmp(pReq->sPath, "/many-headers") == 0 ) {
		const char* sDyn = xrtHttpdRequestHeader(pReq, "X-Dyn-47");
		uint32 iHeaderCount = xrtHttpdRequestHeaderCount(pReq);
		const char* sLastName = iHeaderCount > 0u ? xrtHttpdRequestHeaderNameAt(pReq, iHeaderCount - 1u) : NULL;
		const char* sLastValue = iHeaderCount > 0u ? xrtHttpdRequestHeaderValueAt(pReq, iHeaderCount - 1u) : NULL;
		if ( iHeaderCount < 50u || !sLastName || !sLastValue ||
			strcmp(sLastName, "X-Dyn-47") != 0 || strcmp(sLastValue, "v47") != 0 ||
			xrtHttpdRequestHeaderNameAt(pReq, iHeaderCount) != NULL ||
			xrtHttpdRequestHeaderValueAt(pReq, iHeaderCount) != NULL ) {
			sDyn = NULL;
		}
		xrtHttpdResponseSetStatus(pResp, 200u, NULL);
		(void)xrtHttpdResponseSetBodyCopy(pResp, sDyn ? sDyn : "missing", sDyn ? strlen(sDyn) : 7u, "text/plain");
		return true;
	}
	if ( strcmp(pReq->sPath, "/secure") == 0 ) {
		xrtHttpdResponseSetStatus(pResp, 200u, NULL);
		(void)xrtHttpdResponseSetBodyCopy(pResp, "secure", 6, "text/plain");
		return true;
	}
	if ( strcmp(pReq->sPath, "/append-body") == 0 ) {
		xrtHttpdResponseSetStatus(pResp, 200u, NULL);
		if ( !xrtHttpdResponseSetBodyCopy(pResp, "alpha", 5u, "text/plain") ) { return false; }
		if ( !xrtHttpdResponseAppendBodyCopy(pResp, "-", 1u) ) { return false; }
		if ( !xrtHttpdResponseAppendBodyCopy(pResp, "beta", 4u) ) { return false; }
		return pResp->iBodyLen == 10u && pResp->iBodyCap >= pResp->iBodyLen;
	}
	if ( strcmp(pReq->sPath, "/chunked") == 0 ) {
		xrtHttpdResponseSetStatus(pResp, 200u, NULL);
		(void)xrtHttpdResponseSetHeader(pResp, "Transfer-Encoding", "chunked");
		(void)xrtHttpdResponseSetBodyCopy(pResp, "chunk-reply", 11, "text/plain");
		return true;
	}
	if ( strcmp(pReq->sPath, "/chunked-empty") == 0 ) {
		xrtHttpdResponseSetStatus(pResp, 200u, NULL);
		(void)xrtHttpdResponseSetHeader(pResp, "Transfer-Encoding", "chunked");
		return true;
	}
	if ( strcmp(pReq->sPath, "/stream-fixed") == 0 ) {
		xhttpdresponse tStreamResp;
		xrtHttpdResponseInit(&tStreamResp);
		xrtHttpdResponseSetStatus(&tStreamResp, 200u, NULL);
		(void)xrtHttpdResponseSetHeader(&tStreamResp, "Content-Type", "text/plain");
		(void)xrtHttpdResponseSetHeader(&tStreamResp, "Content-Length", "11");
		if ( xrtHttpdConnStart(pConn, &tStreamResp) == XRT_NET_OK ) {
			(void)xrtHttpdConnSend(pConn, "fixed-", 6u);
			(void)xrtHttpdConnSend(pConn, "reply", 5u);
			(void)xrtHttpdConnEnd(pConn);
		}
		xrtHttpdResponseUnit(&tStreamResp);
		return true;
	}
	return false;
}


// 内部函数：__Test_XHttpdRecordStreamRequest
static void __Test_XHttpdRecordStreamRequest(__test_xhttpd_ctx* pCtx, const xhttpdrequest* pReq)
{
	const char* sHost;
	if ( !pCtx || !pReq ) return;
	__Test_XHttpdAtomicInc(&pCtx->iRequestCount);
	pCtx->iLastMethod = xrtHttpdRequestMethod(pReq);
	__xhttpCopyToken(pCtx->aLastMethod, sizeof(pCtx->aLastMethod), pReq->sMethod);
	__xhttpCopyToken(pCtx->aLastPath, sizeof(pCtx->aLastPath), pReq->sPath);
	__xhttpCopyToken(pCtx->aLastQuery, sizeof(pCtx->aLastQuery), pReq->sQuery);
	sHost = xrtHttpdRequestHeader(pReq, "Host");
	if ( sHost ) __xhttpCopyToken(pCtx->aLastHost, sizeof(pCtx->aLastHost), sHost);
	memset(pCtx->aLastBody, 0, sizeof(pCtx->aLastBody));
	memset(pCtx->aStreamBody, 0, sizeof(pCtx->aStreamBody));
	pCtx->iStreamBodyLen = 0u;
	if ( (pReq->iFlags & XHTTPD_REQ_F_STREAMING) != 0u ) {
		__Test_XHttpdAtomicInc(&pCtx->iBodySawStreamingFlag);
	}
}


// 内部函数：__Test_XHttpdOnRequestBodyBegin
static bool __Test_XHttpdOnRequestBodyBegin(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq)
{
	__test_xhttpd_ctx* pCtx = (__test_xhttpd_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	if ( !pCtx || !pReq ) return false;
	__Test_XHttpdAtomicInc(&pCtx->iBodyBeginCount);
	__Test_XHttpdRecordStreamRequest(pCtx, pReq);
	return strcmp(pReq->sPath, "/stream-fixed-body") == 0 ||
		strcmp(pReq->sPath, "/stream-chunked-body") == 0 ||
		strcmp(pReq->sPath, "/stream-async-body") == 0 ||
		strcmp(pReq->sPath, "/stream-callback-fail") == 0 ||
		strcmp(pReq->sPath, "/stream-limit") == 0;
}


// 内部函数：__Test_XHttpdOnRequestBody
static bool __Test_XHttpdOnRequestBody(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq, const void* pData, size_t iLen)
{
	__test_xhttpd_ctx* pCtx = (__test_xhttpd_ctx*)pOwner;
	size_t iCopy;
	(void)pServer;
	(void)pConn;
	if ( !pCtx || (!pData && iLen > 0u) ) return false;
	if ( pReq && strcmp(pReq->sPath, "/stream-callback-fail") == 0 ) {
		__Test_XHttpdAtomicInc(&pCtx->iBodyChunkCount);
		return false;
	}
	__Test_XHttpdAtomicInc(&pCtx->iBodyChunkCount);
	iCopy = iLen;
	if ( iCopy > sizeof(pCtx->aStreamBody) - 1u - pCtx->iStreamBodyLen ) {
		iCopy = sizeof(pCtx->aStreamBody) - 1u - pCtx->iStreamBodyLen;
	}
	if ( iCopy > 0u ) {
		memcpy(pCtx->aStreamBody + pCtx->iStreamBodyLen, pData, iCopy);
		pCtx->iStreamBodyLen += iCopy;
		pCtx->aStreamBody[pCtx->iStreamBodyLen] = '\0';
	}
	return true;
}


// 内部函数：__Test_XHttpdOnRequestBodyEnd
static bool __Test_XHttpdOnRequestBodyEnd(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq, xhttpdresponse* pResp)
{
	__test_xhttpd_ctx* pCtx = (__test_xhttpd_ctx*)pOwner;
	size_t iCopy;
	(void)pServer;
	(void)pConn;
	if ( !pCtx || !pReq || !pResp ) return false;
	if ( strcmp(pReq->sPath, "/stream-fixed-body") != 0 &&
		strcmp(pReq->sPath, "/stream-chunked-body") != 0 &&
		strcmp(pReq->sPath, "/stream-limit") != 0 ) {
		return false;
	}
	__Test_XHttpdAtomicInc(&pCtx->iBodyEndCount);
	iCopy = pCtx->iStreamBodyLen;
	if ( iCopy > sizeof(pCtx->aLastBody) - 1u ) { iCopy = sizeof(pCtx->aLastBody) - 1u; }
	memcpy(pCtx->aLastBody, pCtx->aStreamBody, iCopy);
	pCtx->aLastBody[iCopy] = '\0';
	xrtHttpdResponseSetStatus(pResp, 200u, NULL);
	(void)xrtHttpdResponseSetBodyCopy(pResp, pCtx->aStreamBody, pCtx->iStreamBodyLen, "text/plain");
	(void)xrtHttpdResponseSetHeader(pResp, "X-Streamed", "yes");
	return true;
}


// 内部函数：__Test_XHttpdAsyncFutureProc
static int32 __Test_XHttpdAsyncFutureProc(ptr pArg, xfuture_result* pOut)
{
	__test_xhttpd_async_future_task* pTask = (__test_xhttpd_async_future_task*)pArg;
	xhttpdresponse* pResp = NULL;
	if ( !pTask || !pOut ) {
		if ( pTask ) XNET_FREE(pTask);
		return XRT_NET_ERROR;
	}
	__Test_XHttpdSleepMs(pTask->iDelayMs ? pTask->iDelayMs : 25u);
	if ( pTask->bFail ) {
		pOut->sError = (str)"async future failed";
		XNET_FREE(pTask);
		return XRT_NET_ERROR;
	}
	pResp = xrtHttpdResponseCreate();
	if ( !pResp ) {
		XNET_FREE(pTask);
		return XRT_NET_ERROR;
	}
	xrtHttpdResponseSetStatus(pResp, pTask->iStatusCode, NULL);
	(void)xrtHttpdResponseSetBodyCopy(pResp, pTask->aBody, strlen(pTask->aBody), "text/plain");
	if ( pTask->aHeaderValue[0] ) {
		(void)xrtHttpdResponseSetHeader(pResp, "X-Async", pTask->aHeaderValue);
	}
	pOut->pValue = pResp;
	__Test_XHttpdAtomicInc(&pTask->pCtx->iAsyncFutureCount);
	XNET_FREE(pTask);
	return XRT_NET_OK;
}


// 内部函数：__Test_XHttpdOnRequestAsyncFuture
static xfuture* __Test_XHttpdOnRequestAsyncFuture(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq)
{
	__test_xhttpd_ctx* pCtx = (__test_xhttpd_ctx*)pOwner;
	__test_xhttpd_async_future_task* pTask;
	(void)pServer;
	(void)pConn;
	if ( !pCtx || !pReq ) return NULL;
	__Test_XHttpdRecordRequest(pCtx, pReq);
	if ( strcmp(pReq->sPath, "/async/future") != 0 &&
		strcmp(pReq->sPath, "/async/fail") != 0 &&
		strcmp(pReq->sPath, "/async/slow") != 0 ) return NULL;
	pTask = (__test_xhttpd_async_future_task*)XNET_ALLOC(sizeof(__test_xhttpd_async_future_task));
	if ( !pTask ) return NULL;
	memset(pTask, 0, sizeof(*pTask));
	pTask->pCtx = pCtx;
	if ( strcmp(pReq->sPath, "/async/fail") == 0 ) {
		pTask->bFail = true;
	} else {
		snprintf(pTask->aBody, sizeof(pTask->aBody), "future:%s", pReq->pBody ? pReq->pBody : "");
		pTask->iStatusCode = 202u;
		pTask->iDelayMs = strcmp(pReq->sPath, "/async/slow") == 0 ? 200u : 25u;
		__xhttpCopyToken(pTask->aHeaderValue, sizeof(pTask->aHeaderValue), "thread");
	}
	return xTaskRunThread(__Test_XHttpdAsyncFutureProc, pTask, 0);
}


// 内部函数：__Test_XHttpdAsyncManualProc
static xfuture* __Test_XHttpdOnRequestBodyEndAsync(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq)
{
	__test_xhttpd_ctx* pCtx = (__test_xhttpd_ctx*)pOwner;
	__test_xhttpd_async_future_task* pTask;
	size_t iCopy;
	(void)pServer;
	(void)pConn;
	if ( !pCtx || !pReq || strcmp(pReq->sPath, "/stream-async-body") != 0 ) return NULL;
	__Test_XHttpdAtomicInc(&pCtx->iBodyEndCount);
	iCopy = pCtx->iStreamBodyLen;
	if ( iCopy > sizeof(pCtx->aLastBody) - 1u ) { iCopy = sizeof(pCtx->aLastBody) - 1u; }
	memcpy(pCtx->aLastBody, pCtx->aStreamBody, iCopy);
	pCtx->aLastBody[iCopy] = '\0';
	pTask = (__test_xhttpd_async_future_task*)XNET_ALLOC(sizeof(__test_xhttpd_async_future_task));
	if ( !pTask ) return NULL;
	memset(pTask, 0, sizeof(*pTask));
	pTask->pCtx = pCtx;
	pTask->iStatusCode = 203u;
	pTask->iDelayMs = 25u;
	__xhttpCopyToken(pTask->aHeaderValue, sizeof(pTask->aHeaderValue), "body-end");
	snprintf(pTask->aBody, sizeof(pTask->aBody), "async-body:%s", pCtx->aStreamBody);
	return xTaskRunThread(__Test_XHttpdAsyncFutureProc, pTask, 0);
}


static int32 __Test_XHttpdAsyncManualProc(ptr pArg, xfuture_result* pOut)
{
	__test_xhttpd_async_manual_task* pTask = (__test_xhttpd_async_manual_task*)pArg;
	xhttpdresponse* pResp = NULL;
	xnet_result iRet = XRT_NET_ERROR;
	(void)pOut;
	if ( !pTask ) return XRT_NET_ERROR;
	__Test_XHttpdSleepMs(25u);
	pResp = xrtHttpdResponseCreate();
	if ( pResp ) {
		xrtHttpdResponseSetStatus(pResp, 200u, NULL);
		(void)xrtHttpdResponseSetBodyCopy(pResp, pTask->aBody, strlen(pTask->aBody), "text/plain");
		if ( pTask->bClose ) (void)xrtHttpdResponseSetHeader(pResp, "Connection", "close");
		iRet = xrtHttpdConnRespond(pTask->pConn, pResp);
		if ( iRet == XRT_NET_OK ) __Test_XHttpdAtomicInc(&pTask->pCtx->iAsyncManualCount);
		xrtHttpdResponseDestroy(pResp);
	}
	XNET_FREE(pTask);
	return iRet;
}


// 内部函数：__Test_XHttpdOnRequestAsyncManual
static xfuture* __Test_XHttpdOnRequestAsyncManual(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq)
{
	__test_xhttpd_ctx* pCtx = (__test_xhttpd_ctx*)pOwner;
	__test_xhttpd_async_manual_task* pTask;
	(void)pServer;
	if ( !pCtx || !pConn || !pReq ) return NULL;
	__Test_XHttpdRecordRequest(pCtx, pReq);
	if ( strcmp(pReq->sPath, "/async/manual") != 0 ) return NULL;
	pTask = (__test_xhttpd_async_manual_task*)XNET_ALLOC(sizeof(__test_xhttpd_async_manual_task));
	if ( !pTask ) return NULL;
	memset(pTask, 0, sizeof(*pTask));
	pTask->pCtx = pCtx;
	pTask->pConn = pConn;
	snprintf(pTask->aBody, sizeof(pTask->aBody), "manual:%s", pReq->pBody ? pReq->pBody : "");
	pTask->bClose = (pReq->iFlags & XHTTPD_REQ_F_KEEPALIVE) == 0u;
	return xTaskRunThread(__Test_XHttpdAsyncManualProc, pTask, 0);
}

#if !defined(XRT_NO_COROUTINE)
// 内部函数：__Test_XHttpdAsyncCoProc
static int32 __Test_XHttpdAsyncCoProc(ptr pArg, xfuture_result* pOut)
{
	__test_xhttpd_ctx* pCtx = (__test_xhttpd_ctx*)pArg;
	xhttpdresponse* pResp = NULL;
	if ( !pCtx || !pOut ) return XRT_NET_ERROR;
	pResp = xrtHttpdResponseCreate();
	if ( !pResp ) return XRT_NET_ERROR;
	xrtHttpdResponseSetStatus(pResp, 206u, NULL);
	(void)xrtHttpdResponseSetBodyCopy(pResp, "co:done", 7, "text/plain");
	(void)xrtHttpdResponseSetHeader(pResp, "X-Async", "co");
	pOut->pValue = pResp;
	__Test_XHttpdAtomicInc(&pCtx->iAsyncCoroutineCount);
	return XRT_NET_OK;
}


// 内部函数：__Test_XHttpdOnRequestAsyncCo
static xfuture* __Test_XHttpdOnRequestAsyncCo(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq)
{
	__test_xhttpd_ctx* pCtx = (__test_xhttpd_ctx*)pOwner;
	xcosched* pSched = NULL;
	xfuture* pFuture = NULL;
	(void)pServer;
	(void)pConn;
	if ( !pCtx || !pReq ) return NULL;
	__Test_XHttpdRecordRequest(pCtx, pReq);
	if ( strcmp(pReq->sPath, "/async/co") != 0 ) return NULL;
	pSched = xrtCoSchedCreate();
	if ( !pSched ) return NULL;
	pFuture = xTaskRunCo(pSched, __Test_XHttpdAsyncCoProc, pCtx, 0);
	if ( pFuture ) xrtCoSchedRun(pSched);
	xrtCoSchedDestroy(pSched);
	return pFuture;
}
#endif

// 内部函数：__Test_XHttpdOnClose
static void __Test_XHttpdOnClose(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, xnet_result iReason)
{
	__test_xhttpd_ctx* pCtx = (__test_xhttpd_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	(void)iReason;
	if ( !pCtx ) return;
	__Test_XHttpdAtomicInc(&pCtx->iCloseCount);
}


// 内部函数：__Test_XHttpdOnError
static void __Test_XHttpdOnError(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, int iSysErr)
{
	__test_xhttpd_ctx* pCtx = (__test_xhttpd_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	(void)iSysErr;
	if ( !pCtx ) return;
	__Test_XHttpdAtomicInc(&pCtx->iErrorCount);
}


// XNETHTTP 服务端测试
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
		tSrvCfg.iBodyLimit = 8u;
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
		printf("  HTTPD plain response date header : %s\n",
			pResp && xrtHttpResponseHeader(pResp, "Date") != NULL ? "PASS" : "FAIL");
		{
			size_t iPlainBodyLen = 0u;
			const void* pPlainBody = xrtHttpResponseBody(pResp, &iPlainBodyLen);
			char* sPlainBody = xrtHttpResponseBodyTextCopy(pResp);
			printf("  HTTPD client accessor status : %s\n",
				xrtHttpResponseStatusCode(pResp) == 201u ? "PASS" : "FAIL");
			printf("  HTTPD client accessor reason : %s\n",
				xrtHttpResponseReason(pResp) != NULL ? "PASS" : "FAIL");
			printf("  HTTPD client accessor content-length : %s\n",
				xrtHttpResponseContentLength(pResp) == 5 ? "PASS" : "FAIL");
			printf("  HTTPD client accessor body : %s\n",
				pPlainBody && iPlainBodyLen == 5u && memcmp(pPlainBody, "hello", 5u) == 0 ? "PASS" : "FAIL");
			printf("  HTTPD client accessor body text copy : %s\n",
				sPlainBody && strcmp(sPlainBody, "hello") == 0 ? "PASS" : "FAIL");
			if ( sPlainBody ) { XNET_FREE(sPlainBody); }
		}
		printf("  HTTPD plain request callback : %s\n", __Test_XHttpdWaitMin(&tCtx.iRequestCount, 1, 1000u) ? "PASS" : "FAIL");
		printf("  HTTPD plain saw method : %s\n", strcmp(tCtx.aLastMethod, "POST") == 0 ? "PASS" : "FAIL");
		printf("  HTTPD plain saw method id : %s\n", tCtx.iLastMethod == XHTTPD_METHOD_POST ? "PASS" : "FAIL");
		printf("  HTTPD plain saw path : %s\n", strcmp(tCtx.aLastPath, "/echo") == 0 ? "PASS" : "FAIL");
		printf("  HTTPD plain saw query : %s\n", strcmp(tCtx.aLastQuery, "mode=plain") == 0 ? "PASS" : "FAIL");
		printf("  HTTPD plain saw host : %s\n", strstr(tCtx.aLastHost, "127.0.0.1") != NULL ? "PASS" : "FAIL");
		printf("  HTTPD plain saw body : %s\n", strcmp(tCtx.aLastBody, "hello") == 0 ? "PASS" : "FAIL");
		printf("  HTTPD plain open callback : %s\n", __Test_XHttpdWaitMin(&tCtx.iOpenCount, 1, 1000u) ? "PASS" : "FAIL");

		{
			xhttprequest* pHeapReq = xrtHttpRequestCreate();
			xhttpresponse* pHeapResp = NULL;
			xnet_result iHeapStatus = XRT_NET_ERROR;
			printf("  HTTPD client heap request create : %s\n", pHeapReq != NULL ? "PASS" : "FAIL");
			snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/secure", (unsigned)xrtHttpdBoundPort(pServer));
			if ( pHeapReq ) {
				(void)xrtHttpRequestSetURL(pHeapReq, sURL);
				pHeapResp = xrtHttpExecuteSync(pClientEngine, pHeapReq, &iHeapStatus);
			}
			printf("  HTTPD client heap request execute : %s\n",
				pHeapResp != NULL && iHeapStatus == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  HTTPD client heap response status : %s\n",
				xrtHttpResponseStatusCode(pHeapResp) == 200u ? "PASS" : "FAIL");
			printf("  HTTPD client heap response body : %s\n",
				pHeapResp && pHeapResp->iBodyLen == 6u && memcmp(pHeapResp->pBody, "secure", 6u) == 0 ? "PASS" : "FAIL");
			if ( pHeapResp ) { xrtHttpResponseDestroy(pHeapResp); }
			xrtHttpRequestDestroy(pHeapReq);
		}

		{
			xhttprequest tAppendReq;
			xnetfuture* pAppendFuture = NULL;
			xhttpresponse* pAppendResp = NULL;
			xnet_result iAppendStatus = XRT_NET_ERROR;
			xrtHttpRequestInit(&tAppendReq);
			snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/append-body", (unsigned)xrtHttpdBoundPort(pServer));
			(void)xrtHttpRequestSetURL(&tAppendReq, sURL);
			pAppendFuture = (pClientEngine && pServer) ? xrtHttpExecuteAsync(pClientEngine, &tAppendReq) : NULL;
			printf("  HTTPD append body response future create : %s\n", pAppendFuture != NULL ? "PASS" : "FAIL");
			printf("  HTTPD append body response future wait : %s\n",
				pAppendFuture && (iAppendStatus = xrtNetFutureWait(pAppendFuture, 3000u)) == XRT_NET_OK ? "PASS" : "FAIL");
			pAppendResp = (pAppendFuture && iAppendStatus == XRT_NET_OK) ? (xhttpresponse*)xrtNetFutureValue(pAppendFuture) : NULL;
			printf("  HTTPD append body response status : %s\n",
				pAppendResp && pAppendResp->iStatusCode == 200u ? "PASS" : "FAIL");
			printf("  HTTPD append body response body : %s\n",
				pAppendResp && pAppendResp->iBodyLen == 10u && memcmp(pAppendResp->pBody, "alpha-beta", 10u) == 0 ? "PASS" : "FAIL");
			if ( pAppendResp ) { xrtHttpResponseDestroy(pAppendResp); }
			if ( pAppendFuture ) { xrtNetFutureDestroy(pAppendFuture); }
			xrtHttpRequestUnit(&tAppendReq);
		}

		{
			xhttprequest tHeadReq;
			xnetfuture* pHeadFuture = NULL;
			xhttpresponse* pHeadResp = NULL;
			xnet_result iHeadStatus = XRT_NET_ERROR;
			xrtHttpRequestInit(&tHeadReq);
			snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/secure", (unsigned)xrtHttpdBoundPort(pServer));
			(void)xrtHttpRequestSetMethod(&tHeadReq, "HEAD");
			(void)xrtHttpRequestSetURL(&tHeadReq, sURL);
			pHeadFuture = (pClientEngine && pServer) ? xrtHttpExecuteAsync(pClientEngine, &tHeadReq) : NULL;
			printf("  HTTPD HEAD client future create : %s\n", pHeadFuture != NULL ? "PASS" : "FAIL");
			printf("  HTTPD HEAD client future wait : %s\n",
				pHeadFuture && (iHeadStatus = xrtNetFutureWait(pHeadFuture, 3000u)) == XRT_NET_OK ? "PASS" : "FAIL");
			pHeadResp = (pHeadFuture && iHeadStatus == XRT_NET_OK) ? (xhttpresponse*)xrtNetFutureValue(pHeadFuture) : NULL;
			printf("  HTTPD HEAD client status : %s\n", pHeadResp && pHeadResp->iStatusCode == 200u ? "PASS" : "FAIL");
			printf("  HTTPD HEAD client content-length : %s\n",
				xrtHttpResponseContentLength(pHeadResp) == 6 && xrtHttpResponseHeader(pHeadResp, "Content-Length") != NULL ? "PASS" : "FAIL");
			{
				char* sHeadBody = xrtHttpResponseBodyTextCopy(pHeadResp);
				printf("  HTTPD HEAD client empty body : %s\n",
					pHeadResp && sHeadBody != NULL && sHeadBody[0] == '\0' && pHeadResp->iBodyLen == 0u ? "PASS" : "FAIL");
				if ( sHeadBody ) { XNET_FREE(sHeadBody); }
			}
			if ( pHeadResp ) { xrtHttpResponseDestroy(pHeadResp); }
			if ( pHeadFuture ) { xrtNetFutureDestroy(pHeadFuture); }
			xrtHttpRequestUnit(&tHeadReq);
		}

		{
			xhttpdresponse tGuardResp;
			char* pHeaderBytes = NULL;
			size_t iHeaderBytesLen = 0u;
			bool bManyHeadersOk = true;
			xrtHttpdResponseInit(&tGuardResp);
			printf("  HTTPD response rejects CRLF header value : %s\n",
				!xrtHttpdResponseSetHeader(&tGuardResp, "X-Test", "ok\r\nX-Bad: yes") ? "PASS" : "FAIL");
			printf("  HTTPD response rejects CRLF header name : %s\n",
				!xrtHttpdResponseSetHeader(&tGuardResp, "X-Bad\r\nName", "ok") ? "PASS" : "FAIL");
			xrtHttpdResponseSetStatus(&tGuardResp, 200u, "OK\r\nX-Bad: yes");
			printf("  HTTPD response rejects CRLF reason : %s\n",
				tGuardResp.sReason[0] == '\0' ? "PASS" : "FAIL");
			xrtHttpdResponseUnit(&tGuardResp);
			xrtHttpdResponseInit(&tGuardResp);
			for ( uint32 i = 0u; i < 48u; ++i ) {
				char sName[32];
				char sValue[32];
				snprintf(sName, sizeof(sName), "X-Dyn-%u", (unsigned)i);
				snprintf(sValue, sizeof(sValue), "v%u", (unsigned)i);
				if ( !xrtHttpdResponseAddHeader(&tGuardResp, sName, sValue) ) {
					bManyHeadersOk = false;
					break;
				}
			}
			printf("  HTTPD response dynamic header append : %s\n",
				bManyHeadersOk && tGuardResp.iHeaderCount == 48u ? "PASS" : "FAIL");
			printf("  HTTPD response dynamic header lookup : %s\n",
				xrtHttpdResponseHeader(&tGuardResp, "X-Dyn-47") &&
				strcmp(xrtHttpdResponseHeader(&tGuardResp, "X-Dyn-47"), "v47") == 0 ? "PASS" : "FAIL");
			printf("  HTTPD response dynamic header serialize : %s\n",
				__xhttpdBuildResponseBytes(&tGuardResp, &pHeaderBytes, &iHeaderBytesLen) &&
				pHeaderBytes && strstr(pHeaderBytes, "X-Dyn-47: v47\r\n") != NULL ? "PASS" : "FAIL");
			if ( pHeaderBytes ) { XNET_FREE(pHeaderBytes); }
			xrtHttpdResponseUnit(&tGuardResp);
		}

		{
			xhttpdrequest tBodyReq;
			xhttpdresponse tBodyResp;
			size_t iRespCapAfterReserve;
			xrtHttpdRequestInit(&tBodyReq);
			xrtHttpdResponseInit(&tBodyResp);
			printf("  HTTPD request body reserve : %s\n",
				xrtHttpdRequestReserveBody(&tBodyReq, 4u) && tBodyReq.iBodyCap >= 4u ? "PASS" : "FAIL");
			printf("  HTTPD request body set copy : %s\n",
				xrtHttpdRequestSetBodyCopy(&tBodyReq, "req", 3u) &&
				tBodyReq.iBodyLen == 3u && tBodyReq.iBodyCap >= tBodyReq.iBodyLen &&
				memcmp(tBodyReq.pBody, "req", 3u) == 0 && tBodyReq.pBody[3] == '\0' ? "PASS" : "FAIL");
			printf("  HTTPD request body append copy : %s\n",
				xrtHttpdRequestAppendBodyCopy(&tBodyReq, "-body", 5u) &&
				tBodyReq.iBodyLen == 8u && tBodyReq.iBodyCap >= tBodyReq.iBodyLen &&
				memcmp(tBodyReq.pBody, "req-body", 8u) == 0 && tBodyReq.pBody[8] == '\0' ? "PASS" : "FAIL");
			printf("  HTTPD response body reserve : %s\n",
				xrtHttpdResponseReserveBody(&tBodyResp, 8u) && tBodyResp.iBodyCap >= 8u ? "PASS" : "FAIL");
			iRespCapAfterReserve = tBodyResp.iBodyCap;
			printf("  HTTPD response body set copy : %s\n",
				xrtHttpdResponseSetBodyCopy(&tBodyResp, "resp", 4u, "text/plain") &&
				tBodyResp.iBodyLen == 4u && tBodyResp.iBodyCap >= tBodyResp.iBodyLen &&
				tBodyResp.iBodyCap == iRespCapAfterReserve &&
				memcmp(tBodyResp.pBody, "resp", 4u) == 0 && tBodyResp.pBody[4] == '\0' ? "PASS" : "FAIL");
			printf("  HTTPD response body append copy : %s\n",
				xrtHttpdResponseAppendBodyCopy(&tBodyResp, "-body", 5u) &&
				tBodyResp.iBodyLen == 9u && tBodyResp.iBodyCap >= tBodyResp.iBodyLen &&
				memcmp(tBodyResp.pBody, "resp-body", 9u) == 0 && tBodyResp.pBody[9] == '\0' ? "PASS" : "FAIL");
			xrtHttpdRequestUnit(&tBodyReq);
			xrtHttpdResponseUnit(&tBodyResp);
		}

		{
			xsocket hManyHeaderSock = __Test_XHttpdConnectLoopback(xrtHttpdBoundPort(pServer));
			char sManyHeaderReq[4096];
			char sManyHeaderResp[1024];
			size_t iManyHeaderLen = 0u;
			size_t iManyHeaderRespLen = 0u;
			bool bManyHeaderBuild = hManyHeaderSock != XNET_SOCKET_INVALID;
			bool bManyHeaderSend = false;
			bool bManyHeaderRecv = false;
			bool bManyHeaderBody = false;
			if ( bManyHeaderBuild ) {
				int iWritten = snprintf(sManyHeaderReq, sizeof(sManyHeaderReq),
					"GET /many-headers HTTP/1.1\r\nHost: 127.0.0.1:%u\r\nConnection: close\r\n",
					(unsigned)xrtHttpdBoundPort(pServer));
				bManyHeaderBuild = iWritten > 0 && (size_t)iWritten < sizeof(sManyHeaderReq);
				if ( bManyHeaderBuild ) iManyHeaderLen = (size_t)iWritten;
			}
			for ( uint32 i = 0u; bManyHeaderBuild && i < 48u; ++i ) {
				int iWritten = snprintf(sManyHeaderReq + iManyHeaderLen, sizeof(sManyHeaderReq) - iManyHeaderLen,
					"X-Dyn-%u: v%u\r\n", (unsigned)i, (unsigned)i);
				bManyHeaderBuild = iWritten > 0 && (size_t)iWritten < sizeof(sManyHeaderReq) - iManyHeaderLen;
				if ( bManyHeaderBuild ) iManyHeaderLen += (size_t)iWritten;
			}
			if ( bManyHeaderBuild ) {
				int iWritten = snprintf(sManyHeaderReq + iManyHeaderLen, sizeof(sManyHeaderReq) - iManyHeaderLen, "\r\n");
				bManyHeaderBuild = iWritten > 0 && (size_t)iWritten < sizeof(sManyHeaderReq) - iManyHeaderLen;
				if ( bManyHeaderBuild ) iManyHeaderLen += (size_t)iWritten;
			}
			printf("  HTTPD request dynamic header build : %s\n", bManyHeaderBuild ? "PASS" : "FAIL");
			bManyHeaderSend = bManyHeaderBuild && __Test_XHttpdSendAll(hManyHeaderSock, sManyHeaderReq, iManyHeaderLen);
			printf("  HTTPD request dynamic header send : %s\n", bManyHeaderSend ? "PASS" : "FAIL");
			bManyHeaderRecv = bManyHeaderSend &&
				__Test_XHttpdRecvResponse(hManyHeaderSock, sManyHeaderResp, sizeof(sManyHeaderResp), &iManyHeaderRespLen, 3000u);
			printf("  HTTPD request dynamic header recv : %s\n", bManyHeaderRecv ? "PASS" : "FAIL");
			if ( bManyHeaderRecv ) {
				char* sBody = strstr(sManyHeaderResp, "\r\n\r\n");
				if ( sBody ) {
					sBody += 4;
					bManyHeaderBody = strncmp(sBody, "v47", 3u) == 0;
				}
			}
			printf("  HTTPD request dynamic header lookup : %s\n", bManyHeaderBody ? "PASS" : "FAIL");
			__Test_XHttpdCloseSocket(hManyHeaderSock);
		}

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

		{
			xhttprequest tChunkEmptyReq;
			xnetfuture* pChunkEmptyFuture = NULL;
			xhttpresponse* pChunkEmptyResp = NULL;
			xnet_result iChunkEmptyStatus = XRT_NET_ERROR;
			snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/chunked-empty", (unsigned)xrtHttpdBoundPort(pServer));
			xrtHttpRequestInit(&tChunkEmptyReq);
			(void)xrtHttpRequestSetURL(&tChunkEmptyReq, sURL);
			pChunkEmptyFuture = (pClientEngine && pServer) ? xrtHttpExecuteAsync(pClientEngine, &tChunkEmptyReq) : NULL;
			printf("  HTTPD empty chunked response future create : %s\n", pChunkEmptyFuture != NULL ? "PASS" : "FAIL");
			printf("  HTTPD empty chunked response future wait : %s\n",
				pChunkEmptyFuture && (iChunkEmptyStatus = xrtNetFutureWait(pChunkEmptyFuture, 3000u)) == XRT_NET_OK ? "PASS" : "FAIL");
			pChunkEmptyResp = (pChunkEmptyFuture && iChunkEmptyStatus == XRT_NET_OK) ? (xhttpresponse*)xrtNetFutureValue(pChunkEmptyFuture) : NULL;
			printf("  HTTPD empty chunked response body : %s\n",
				pChunkEmptyResp && pChunkEmptyResp->iBodyLen == 0u ? "PASS" : "FAIL");
			printf("  HTTPD empty chunked response flag : %s\n",
				pChunkEmptyResp && (pChunkEmptyResp->iFlags & XHTTP_RESP_F_CHUNKED) != 0 ? "PASS" : "FAIL");
			if ( pChunkEmptyResp ) xrtHttpResponseDestroy(pChunkEmptyResp);
			if ( pChunkEmptyFuture ) xrtNetFutureDestroy(pChunkEmptyFuture);
			xrtHttpRequestUnit(&tChunkEmptyReq);
		}

		{
			xhttprequest tStreamFixedReq;
			xnetfuture* pStreamFixedFuture = NULL;
			xhttpresponse* pStreamFixedResp = NULL;
			xnet_result iStreamFixedStatus = XRT_NET_ERROR;
			snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/stream-fixed", (unsigned)xrtHttpdBoundPort(pServer));
			xrtHttpRequestInit(&tStreamFixedReq);
			(void)xrtHttpRequestSetURL(&tStreamFixedReq, sURL);
			pStreamFixedFuture = (pClientEngine && pServer) ? xrtHttpExecuteAsync(pClientEngine, &tStreamFixedReq) : NULL;
			printf("  HTTPD fixed-length stream response future create : %s\n", pStreamFixedFuture != NULL ? "PASS" : "FAIL");
			printf("  HTTPD fixed-length stream response future wait : %s\n",
				pStreamFixedFuture && (iStreamFixedStatus = xrtNetFutureWait(pStreamFixedFuture, 3000u)) == XRT_NET_OK ? "PASS" : "FAIL");
			pStreamFixedResp = (pStreamFixedFuture && iStreamFixedStatus == XRT_NET_OK) ? (xhttpresponse*)xrtNetFutureValue(pStreamFixedFuture) : NULL;
			printf("  HTTPD fixed-length stream response body : %s\n",
				pStreamFixedResp && pStreamFixedResp->iBodyLen == 11 && memcmp(pStreamFixedResp->pBody, "fixed-reply", 11) == 0 ? "PASS" : "FAIL");
			printf("  HTTPD fixed-length stream response header : %s\n",
				pStreamFixedResp && (pStreamFixedResp->iFlags & XHTTP_RESP_F_CHUNKED) == 0 &&
				xrtHttpResponseHeader(pStreamFixedResp, "Transfer-Encoding") == NULL &&
				xrtHttpResponseHeader(pStreamFixedResp, "Content-Length") != NULL ? "PASS" : "FAIL");
			if ( pStreamFixedResp ) xrtHttpResponseDestroy(pStreamFixedResp);
			if ( pStreamFixedFuture ) xrtNetFutureDestroy(pStreamFixedFuture);
			xrtHttpRequestUnit(&tStreamFixedReq);
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

		{
			xsocket hRaw = XNET_SOCKET_INVALID;
			char sReq[512];
			char aResp[2048];
			size_t iRespLen = 0u;
			long iReqBefore = __Test_XHttpdAtomicLoad(&tCtx.iRequestCount);
			hRaw = pServer ? __Test_XHttpdConnectLoopback(xrtHttpdBoundPort(pServer)) : XNET_SOCKET_INVALID;
			snprintf(sReq, sizeof(sReq),
				"GET /echo?mode=bad-header HTTP/1.1\r\n"
				"Host: 127.0.0.1:%u\r\n"
				"Bad Header Without Colon\r\n"
				"Connection: close\r\n\r\n",
				(unsigned)xrtHttpdBoundPort(pServer));
			printf("  HTTPD bad header raw connect : %s\n", hRaw != XNET_SOCKET_INVALID ? "PASS" : "FAIL");
			printf("  HTTPD bad header request send : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdSendAll(hRaw, sReq, strlen(sReq)) ? "PASS" : "FAIL");
			printf("  HTTPD bad header response recv : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdRecvResponse(hRaw, aResp, sizeof(aResp), &iRespLen, 2000u) ? "PASS" : "FAIL");
			printf("  HTTPD bad header status : %s\n", strstr(aResp, "HTTP/1.1 400") != NULL ? "PASS" : "FAIL");
			printf("  HTTPD bad header skips callback : %s\n",
				__Test_XHttpdAtomicLoad(&tCtx.iRequestCount) == iReqBefore ? "PASS" : "FAIL");
			__Test_XHttpdCloseSocket(hRaw);
		}

		{
			xsocket hRaw = XNET_SOCKET_INVALID;
			char sReq[512];
			char aResp[2048];
			size_t iRespLen = 0u;
			long iReqBefore = __Test_XHttpdAtomicLoad(&tCtx.iRequestCount);
			hRaw = pServer ? __Test_XHttpdConnectLoopback(xrtHttpdBoundPort(pServer)) : XNET_SOCKET_INVALID;
			snprintf(sReq, sizeof(sReq),
				"POST /echo?mode=too-large HTTP/1.1\r\n"
				"Host: 127.0.0.1:%u\r\n"
				"Connection: close\r\n"
				"Content-Type: text/plain\r\n"
				"Content-Length: 9\r\n\r\n"
				"123456789",
				(unsigned)xrtHttpdBoundPort(pServer));
			printf("  HTTPD body limit raw connect : %s\n", hRaw != XNET_SOCKET_INVALID ? "PASS" : "FAIL");
			printf("  HTTPD body limit request send : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdSendAll(hRaw, sReq, strlen(sReq)) ? "PASS" : "FAIL");
			printf("  HTTPD body limit response recv : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdRecvResponse(hRaw, aResp, sizeof(aResp), &iRespLen, 2000u) ? "PASS" : "FAIL");
			printf("  HTTPD body limit status : %s\n", strstr(aResp, "HTTP/1.1 413") != NULL ? "PASS" : "FAIL");
			printf("  HTTPD body limit skips callback : %s\n",
				__Test_XHttpdAtomicLoad(&tCtx.iRequestCount) == iReqBefore ? "PASS" : "FAIL");
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
		xsocket hRaw = XNET_SOCKET_INVALID;
		char sReq[512];
		char aResp[2048];
		size_t iRespLen = 0u;

		memset(&tCtx, 0, sizeof(tCtx));
		memset(&tEvents, 0, sizeof(tEvents));
		tEvents.OnRequestBodyBegin = __Test_XHttpdOnRequestBodyBegin;
		tEvents.OnRequestBody = __Test_XHttpdOnRequestBody;
		tEvents.OnRequestBodyEnd = __Test_XHttpdOnRequestBodyEnd;
		tEvents.OnRequestBodyEndAsync = __Test_XHttpdOnRequestBodyEndAsync;
		tEvents.OnClose = __Test_XHttpdOnClose;
		tEvents.OnError = __Test_XHttpdOnError;

		xrtNetEngineConfigInit(&tEngineCfg);
		tEngineCfg.iWorkerCount = 1;
		pServerEngine = xrtNetEngineCreate(&tEngineCfg);
		printf("  HTTPD body stream failure engine create : %s\n", pServerEngine != NULL ? "PASS" : "FAIL");
		if ( pServerEngine ) printf("  HTTPD body stream failure engine start : %s\n", xrtNetEngineStart(pServerEngine) == XRT_NET_OK ? "PASS" : "FAIL");

		xrtHttpdConfigInit(&tSrvCfg);
		tSrvCfg.iBodyLimit = 1024u;
		(void)xrtNetAddrParse(&tSrvCfg.tBindAddr, "127.0.0.1", 0);
		pServer = pServerEngine ? xrtHttpdCreate(pServerEngine, &tSrvCfg, &tEvents, &tCtx) : NULL;
		printf("  HTTPD body stream failure server create : %s\n", pServer != NULL ? "PASS" : "FAIL");
		printf("  HTTPD body stream failure server start : %s\n", pServer && xrtHttpdStart(pServer) == XRT_NET_OK ? "PASS" : "FAIL");

		hRaw = pServer ? __Test_XHttpdConnectLoopback(xrtHttpdBoundPort(pServer)) : XNET_SOCKET_INVALID;
		snprintf(sReq, sizeof(sReq),
			"POST /stream-callback-fail HTTP/1.1\r\n"
			"Host: 127.0.0.1:%u\r\n"
			"Connection: close\r\n"
			"Transfer-Encoding: chunked\r\n\r\n"
			"4\r\nfail\r\n0\r\n\r\n",
			(unsigned)xrtHttpdBoundPort(pServer));
		printf("  HTTPD body stream callback fail raw connect : %s\n", hRaw != XNET_SOCKET_INVALID ? "PASS" : "FAIL");
		printf("  HTTPD body stream callback fail request send : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdSendAll(hRaw, sReq, strlen(sReq)) ? "PASS" : "FAIL");
		printf("  HTTPD body stream callback fail response recv : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdRecvResponse(hRaw, aResp, sizeof(aResp), &iRespLen, 3000u) ? "PASS" : "FAIL");
		printf("  HTTPD body stream callback fail status : %s\n", strstr(aResp, "HTTP/1.1 500") != NULL ? "PASS" : "FAIL");
		printf("  HTTPD body stream callback fail skips end : %s\n", __Test_XHttpdAtomicLoad(&tCtx.iBodyEndCount) == 0 ? "PASS" : "FAIL");
		__Test_XHttpdCloseSocket(hRaw);

		hRaw = pServer ? __Test_XHttpdConnectLoopback(xrtHttpdBoundPort(pServer)) : XNET_SOCKET_INVALID;
		memset(aResp, 0, sizeof(aResp));
		iRespLen = 0u;
		snprintf(sReq, sizeof(sReq),
			"POST /stream-chunked-body HTTP/1.1\r\n"
			"Host: 127.0.0.1:%u\r\n"
			"Connection: close\r\n"
			"Transfer-Encoding: chunked\r\n\r\n"
			"Z\r\nbad\r\n",
			(unsigned)xrtHttpdBoundPort(pServer));
		printf("  HTTPD body stream bad chunk raw connect : %s\n", hRaw != XNET_SOCKET_INVALID ? "PASS" : "FAIL");
		printf("  HTTPD body stream bad chunk request send : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdSendAll(hRaw, sReq, strlen(sReq)) ? "PASS" : "FAIL");
		printf("  HTTPD body stream bad chunk response recv : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdRecvResponse(hRaw, aResp, sizeof(aResp), &iRespLen, 3000u) ? "PASS" : "FAIL");
		printf("  HTTPD body stream bad chunk status : %s\n", strstr(aResp, "HTTP/1.1 400") != NULL ? "PASS" : "FAIL");
		__Test_XHttpdCloseSocket(hRaw);

		if ( pServer ) { xrtHttpdDestroy(pServer); }
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

		memset(&tCtx, 0, sizeof(tCtx));
		memset(&tEvents, 0, sizeof(tEvents));
		tEvents.OnOpen = __Test_XHttpdOnOpen;
		tEvents.OnRequestBodyBegin = __Test_XHttpdOnRequestBodyBegin;
		tEvents.OnRequestBody = __Test_XHttpdOnRequestBody;
		tEvents.OnRequestBodyEnd = __Test_XHttpdOnRequestBodyEnd;
		tEvents.OnRequestBodyEndAsync = __Test_XHttpdOnRequestBodyEndAsync;
		tEvents.OnClose = __Test_XHttpdOnClose;
		tEvents.OnError = __Test_XHttpdOnError;

		xrtNetEngineConfigInit(&tEngineCfg);
		tEngineCfg.iWorkerCount = 1;
		pServerEngine = xrtNetEngineCreate(&tEngineCfg);
		printf("  HTTPD body stream engine create : %s\n", pServerEngine != NULL ? "PASS" : "FAIL");
		if ( pServerEngine ) printf("  HTTPD body stream engine start : %s\n", xrtNetEngineStart(pServerEngine) == XRT_NET_OK ? "PASS" : "FAIL");

		xrtHttpdConfigInit(&tSrvCfg);
		tSrvCfg.iBodyLimit = 64u;
		(void)xrtNetAddrParse(&tSrvCfg.tBindAddr, "127.0.0.1", 0);
		pServer = pServerEngine ? xrtHttpdCreate(pServerEngine, &tSrvCfg, &tEvents, &tCtx) : NULL;
		printf("  HTTPD body stream server create : %s\n", pServer != NULL ? "PASS" : "FAIL");
		printf("  HTTPD body stream server start : %s\n", pServer && xrtHttpdStart(pServer) == XRT_NET_OK ? "PASS" : "FAIL");

		{
			xsocket hRaw = pServer ? __Test_XHttpdConnectLoopback(xrtHttpdBoundPort(pServer)) : XNET_SOCKET_INVALID;
			char sHead[512];
			char aResp[2048];
			size_t iRespLen = 0u;
			long iChunkBefore = __Test_XHttpdAtomicLoad(&tCtx.iBodyChunkCount);
			snprintf(sHead, sizeof(sHead),
				"POST /stream-fixed-body?kind=fixed HTTP/1.1\r\n"
				"Host: 127.0.0.1:%u\r\n"
				"Connection: close\r\n"
				"Content-Type: text/plain\r\n"
				"Content-Length: 10\r\n\r\n",
				(unsigned)xrtHttpdBoundPort(pServer));
			printf("  HTTPD fixed body stream raw connect : %s\n", hRaw != XNET_SOCKET_INVALID ? "PASS" : "FAIL");
			printf("  HTTPD fixed body stream head send : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdSendAll(hRaw, sHead, strlen(sHead)) ? "PASS" : "FAIL");
			printf("  HTTPD fixed body stream part #1 send : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdSendAll(hRaw, "alpha", 5u) ? "PASS" : "FAIL");
			__Test_XHttpdSleepMs(60u);
			printf("  HTTPD fixed body stream part #2 send : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdSendAll(hRaw, "-beta", 5u) ? "PASS" : "FAIL");
			printf("  HTTPD fixed body stream response recv : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdRecvResponse(hRaw, aResp, sizeof(aResp), &iRespLen, 3000u) ? "PASS" : "FAIL");
			printf("  HTTPD fixed body stream status : %s\n", strstr(aResp, "HTTP/1.1 200") != NULL ? "PASS" : "FAIL");
			printf("  HTTPD fixed body stream body : %s\n", strstr(aResp, "\r\n\r\nalpha-beta") != NULL ? "PASS" : "FAIL");
			printf("  HTTPD fixed body stream callbacks : %s\n",
				__Test_XHttpdAtomicLoad(&tCtx.iBodyBeginCount) >= 1 &&
				__Test_XHttpdAtomicLoad(&tCtx.iBodyEndCount) >= 1 &&
				__Test_XHttpdAtomicLoad(&tCtx.iBodyChunkCount) >= iChunkBefore + 2 ? "PASS" : "FAIL");
			printf("  HTTPD fixed body stream flag : %s\n", __Test_XHttpdAtomicLoad(&tCtx.iBodySawStreamingFlag) >= 1 ? "PASS" : "FAIL");
			printf("  HTTPD fixed body stream request meta : %s\n",
				strcmp(tCtx.aLastPath, "/stream-fixed-body") == 0 &&
				strcmp(tCtx.aLastQuery, "kind=fixed") == 0 &&
				strcmp(tCtx.aLastBody, "alpha-beta") == 0 ? "PASS" : "FAIL");
			__Test_XHttpdCloseSocket(hRaw);
		}

		{
			xsocket hRaw = pServer ? __Test_XHttpdConnectLoopback(xrtHttpdBoundPort(pServer)) : XNET_SOCKET_INVALID;
			char sHead[512];
			char aResp[2048];
			size_t iRespLen = 0u;
			long iEndBefore = __Test_XHttpdAtomicLoad(&tCtx.iBodyEndCount);
			snprintf(sHead, sizeof(sHead),
				"POST /stream-chunked-body HTTP/1.1\r\n"
				"Host: 127.0.0.1:%u\r\n"
				"Connection: close\r\n"
				"Transfer-Encoding: chunked\r\n\r\n",
				(unsigned)xrtHttpdBoundPort(pServer));
			printf("  HTTPD chunked body stream raw connect : %s\n", hRaw != XNET_SOCKET_INVALID ? "PASS" : "FAIL");
			printf("  HTTPD chunked body stream head send : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdSendAll(hRaw, sHead, strlen(sHead)) ? "PASS" : "FAIL");
			printf("  HTTPD chunked body stream chunk #1 send : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdSendAll(hRaw, "5\r\nhello\r\n", 10u) ? "PASS" : "FAIL");
			__Test_XHttpdSleepMs(60u);
			printf("  HTTPD chunked body stream chunk #2 send : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdSendAll(hRaw, "6\r\n-world\r\n0\r\n\r\n", 16u) ? "PASS" : "FAIL");
			printf("  HTTPD chunked body stream response recv : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdRecvResponse(hRaw, aResp, sizeof(aResp), &iRespLen, 3000u) ? "PASS" : "FAIL");
			printf("  HTTPD chunked body stream status : %s\n", strstr(aResp, "HTTP/1.1 200") != NULL ? "PASS" : "FAIL");
			printf("  HTTPD chunked body stream body : %s\n", strstr(aResp, "\r\n\r\nhello-world") != NULL ? "PASS" : "FAIL");
			printf("  HTTPD chunked body stream end callback : %s\n", __Test_XHttpdAtomicLoad(&tCtx.iBodyEndCount) >= iEndBefore + 1 ? "PASS" : "FAIL");
			__Test_XHttpdCloseSocket(hRaw);
		}

		{
			xsocket hRaw = pServer ? __Test_XHttpdConnectLoopback(xrtHttpdBoundPort(pServer)) : XNET_SOCKET_INVALID;
			char sHead[512];
			char aResp[2048];
			size_t iRespLen = 0u;
			long iAsyncBefore = __Test_XHttpdAtomicLoad(&tCtx.iAsyncFutureCount);
			snprintf(sHead, sizeof(sHead),
				"POST /stream-async-body HTTP/1.1\r\n"
				"Host: 127.0.0.1:%u\r\n"
				"Connection: close\r\n"
				"Content-Type: text/plain\r\n"
				"Content-Length: 11\r\n\r\n",
				(unsigned)xrtHttpdBoundPort(pServer));
			printf("  HTTPD async body stream raw connect : %s\n", hRaw != XNET_SOCKET_INVALID ? "PASS" : "FAIL");
			printf("  HTTPD async body stream head send : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdSendAll(hRaw, sHead, strlen(sHead)) ? "PASS" : "FAIL");
			printf("  HTTPD async body stream part #1 send : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdSendAll(hRaw, "gamma", 5u) ? "PASS" : "FAIL");
			__Test_XHttpdSleepMs(60u);
			printf("  HTTPD async body stream part #2 send : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdSendAll(hRaw, "-delta", 6u) ? "PASS" : "FAIL");
			printf("  HTTPD async body stream response recv : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdRecvResponse(hRaw, aResp, sizeof(aResp), &iRespLen, 3000u) ? "PASS" : "FAIL");
			printf("  HTTPD async body stream status : %s\n", strstr(aResp, "HTTP/1.1 203") != NULL ? "PASS" : "FAIL");
			printf("  HTTPD async body stream body : %s\n", strstr(aResp, "\r\n\r\nasync-body:gamma-delta") != NULL ? "PASS" : "FAIL");
			printf("  HTTPD async body stream header : %s\n", strstr(aResp, "\r\nX-Async: body-end\r\n") != NULL ? "PASS" : "FAIL");
			printf("  HTTPD async body stream future count : %s\n", __Test_XHttpdWaitMin(&tCtx.iAsyncFutureCount, iAsyncBefore + 1, 1000u) ? "PASS" : "FAIL");
			__Test_XHttpdCloseSocket(hRaw);
		}

		if ( pServer ) { xrtHttpdDestroy(pServer); }
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
		xsocket hRaw = XNET_SOCKET_INVALID;
		char sReq[512];
		char aResp[2048];
		size_t iRespLen = 0u;

		memset(&tCtx, 0, sizeof(tCtx));
		memset(&tEvents, 0, sizeof(tEvents));
		tEvents.OnRequestBodyBegin = __Test_XHttpdOnRequestBodyBegin;
		tEvents.OnRequestBody = __Test_XHttpdOnRequestBody;
		tEvents.OnRequestBodyEnd = __Test_XHttpdOnRequestBodyEnd;
		tEvents.OnClose = __Test_XHttpdOnClose;
		tEvents.OnError = __Test_XHttpdOnError;

		xrtNetEngineConfigInit(&tEngineCfg);
		tEngineCfg.iWorkerCount = 1;
		pServerEngine = xrtNetEngineCreate(&tEngineCfg);
		printf("  HTTPD body stream limit engine create : %s\n", pServerEngine != NULL ? "PASS" : "FAIL");
		if ( pServerEngine ) printf("  HTTPD body stream limit engine start : %s\n", xrtNetEngineStart(pServerEngine) == XRT_NET_OK ? "PASS" : "FAIL");

		xrtHttpdConfigInit(&tSrvCfg);
		tSrvCfg.iBodyLimit = 4u;
		(void)xrtNetAddrParse(&tSrvCfg.tBindAddr, "127.0.0.1", 0);
		pServer = pServerEngine ? xrtHttpdCreate(pServerEngine, &tSrvCfg, &tEvents, &tCtx) : NULL;
		printf("  HTTPD body stream limit server create : %s\n", pServer != NULL ? "PASS" : "FAIL");
		printf("  HTTPD body stream limit server start : %s\n", pServer && xrtHttpdStart(pServer) == XRT_NET_OK ? "PASS" : "FAIL");

		hRaw = pServer ? __Test_XHttpdConnectLoopback(xrtHttpdBoundPort(pServer)) : XNET_SOCKET_INVALID;
		snprintf(sReq, sizeof(sReq),
			"POST /stream-limit HTTP/1.1\r\n"
			"Host: 127.0.0.1:%u\r\n"
			"Connection: close\r\n"
			"Transfer-Encoding: chunked\r\n\r\n"
			"8\r\n12345678\r\n0\r\n\r\n",
			(unsigned)xrtHttpdBoundPort(pServer));
		printf("  HTTPD body stream limit raw connect : %s\n", hRaw != XNET_SOCKET_INVALID ? "PASS" : "FAIL");
		printf("  HTTPD body stream limit request send : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdSendAll(hRaw, sReq, strlen(sReq)) ? "PASS" : "FAIL");
		printf("  HTTPD body stream limit response recv : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdRecvResponse(hRaw, aResp, sizeof(aResp), &iRespLen, 3000u) ? "PASS" : "FAIL");
		printf("  HTTPD body stream limit status : %s\n", strstr(aResp, "HTTP/1.1 413") != NULL ? "PASS" : "FAIL");
		printf("  HTTPD body stream limit skips end : %s\n", __Test_XHttpdAtomicLoad(&tCtx.iBodyEndCount) == 0 ? "PASS" : "FAIL");
		__Test_XHttpdCloseSocket(hRaw);

		if ( pServer ) { xrtHttpdDestroy(pServer); }
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
		xsocket hRaw = XNET_SOCKET_INVALID;
		char sReq[512];

		memset(&tCtx, 0, sizeof(tCtx));
		memset(&tEvents, 0, sizeof(tEvents));
		tEvents.OnOpen = __Test_XHttpdOnOpen;
		tEvents.OnRequestAsync = __Test_XHttpdOnRequestAsyncFuture;
		tEvents.OnClose = __Test_XHttpdOnClose;
		tEvents.OnError = __Test_XHttpdOnError;

		xrtNetEngineConfigInit(&tEngineCfg);
		tEngineCfg.iWorkerCount = 1;
		pServerEngine = xrtNetEngineCreate(&tEngineCfg);
		printf("  HTTPD async disconnect server engine create : %s\n", pServerEngine != NULL ? "PASS" : "FAIL");
		if ( pServerEngine ) printf("  HTTPD async disconnect server engine start : %s\n", xrtNetEngineStart(pServerEngine) == XRT_NET_OK ? "PASS" : "FAIL");

		xrtHttpdConfigInit(&tSrvCfg);
		(void)xrtNetAddrParse(&tSrvCfg.tBindAddr, "127.0.0.1", 0);
		pServer = pServerEngine ? xrtHttpdCreate(pServerEngine, &tSrvCfg, &tEvents, &tCtx) : NULL;
		printf("  HTTPD async disconnect server create : %s\n", pServer != NULL ? "PASS" : "FAIL");
		printf("  HTTPD async disconnect server start : %s\n", pServer && xrtHttpdStart(pServer) == XRT_NET_OK ? "PASS" : "FAIL");

		hRaw = pServer ? __Test_XHttpdConnectLoopback(xrtHttpdBoundPort(pServer)) : XNET_SOCKET_INVALID;
		snprintf(sReq, sizeof(sReq),
			"GET /async/slow HTTP/1.1\r\n"
			"Host: 127.0.0.1:%u\r\n"
			"Connection: close\r\n\r\n",
			(unsigned)xrtHttpdBoundPort(pServer));
		printf("  HTTPD async disconnect raw connect : %s\n", hRaw != XNET_SOCKET_INVALID ? "PASS" : "FAIL");
		printf("  HTTPD async disconnect request send : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdSendAll(hRaw, sReq, strlen(sReq)) ? "PASS" : "FAIL");
		__Test_XHttpdCloseSocket(hRaw);
		hRaw = XNET_SOCKET_INVALID;
		printf("  HTTPD async disconnect request count : %s\n", __Test_XHttpdWaitMin(&tCtx.iRequestCount, 1, 1000u) ? "PASS" : "FAIL");
		printf("  HTTPD async disconnect future complete : %s\n", __Test_XHttpdWaitMin(&tCtx.iAsyncFutureCount, 1, 2000u) ? "PASS" : "FAIL");
		printf("  HTTPD async disconnect close callback : %s\n", __Test_XHttpdWaitMin(&tCtx.iCloseCount, 1, 2000u) ? "PASS" : "FAIL");
		printf("  HTTPD async disconnect benign error : %s\n", __Test_XHttpdAtomicLoad(&tCtx.iErrorCount) == 0 ? "PASS" : "FAIL");

		if ( pServer ) xrtHttpdDestroy(pServer);
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
		xsocket hRaw = XNET_SOCKET_INVALID;
		char sReq[512];

		memset(&tCtx, 0, sizeof(tCtx));
		memset(&tEvents, 0, sizeof(tEvents));
		tEvents.OnOpen = __Test_XHttpdOnOpen;
		tEvents.OnRequestAsync = __Test_XHttpdOnRequestAsyncFuture;
		tEvents.OnClose = __Test_XHttpdOnClose;
		tEvents.OnError = __Test_XHttpdOnError;

		xrtNetEngineConfigInit(&tEngineCfg);
		tEngineCfg.iWorkerCount = 1;
		pServerEngine = xrtNetEngineCreate(&tEngineCfg);
		printf("  HTTPD async stop-pending server engine create : %s\n", pServerEngine != NULL ? "PASS" : "FAIL");
		if ( pServerEngine ) printf("  HTTPD async stop-pending server engine start : %s\n", xrtNetEngineStart(pServerEngine) == XRT_NET_OK ? "PASS" : "FAIL");

		xrtHttpdConfigInit(&tSrvCfg);
		(void)xrtNetAddrParse(&tSrvCfg.tBindAddr, "127.0.0.1", 0);
		pServer = pServerEngine ? xrtHttpdCreate(pServerEngine, &tSrvCfg, &tEvents, &tCtx) : NULL;
		printf("  HTTPD async stop-pending server create : %s\n", pServer != NULL ? "PASS" : "FAIL");
		printf("  HTTPD async stop-pending server start : %s\n", pServer && xrtHttpdStart(pServer) == XRT_NET_OK ? "PASS" : "FAIL");

		hRaw = pServer ? __Test_XHttpdConnectLoopback(xrtHttpdBoundPort(pServer)) : XNET_SOCKET_INVALID;
		snprintf(sReq, sizeof(sReq),
			"GET /async/slow HTTP/1.1\r\n"
			"Host: 127.0.0.1:%u\r\n"
			"Connection: close\r\n\r\n",
			(unsigned)xrtHttpdBoundPort(pServer));
		printf("  HTTPD async stop-pending raw connect : %s\n", hRaw != XNET_SOCKET_INVALID ? "PASS" : "FAIL");
		printf("  HTTPD async stop-pending request send : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdSendAll(hRaw, sReq, strlen(sReq)) ? "PASS" : "FAIL");
		printf("  HTTPD async stop-pending request count : %s\n", __Test_XHttpdWaitMin(&tCtx.iRequestCount, 1, 1000u) ? "PASS" : "FAIL");
		if ( pServer ) {
			xrtHttpdDestroy(pServer);
			pServer = NULL;
		}
		printf("  HTTPD async stop-pending future complete : %s\n", __Test_XHttpdWaitMin(&tCtx.iAsyncFutureCount, 1, 2000u) ? "PASS" : "FAIL");
		printf("  HTTPD async stop-pending close callback : %s\n", __Test_XHttpdWaitMin(&tCtx.iCloseCount, 1, 2000u) ? "PASS" : "FAIL");
		printf("  HTTPD async stop-pending benign error : %s\n", __Test_XHttpdAtomicLoad(&tCtx.iErrorCount) == 0 ? "PASS" : "FAIL");

		__Test_XHttpdCloseSocket(hRaw);
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
		tEvents.OnRequestAsync = __Test_XHttpdOnRequestAsyncFuture;
		tEvents.OnClose = __Test_XHttpdOnClose;
		tEvents.OnError = __Test_XHttpdOnError;

		xrtNetEngineConfigInit(&tEngineCfg);
		tEngineCfg.iWorkerCount = 1;
		pServerEngine = xrtNetEngineCreate(&tEngineCfg);
		pClientEngine = xrtNetEngineCreate(&tEngineCfg);
		printf("  HTTPD async future server engine create : %s\n", pServerEngine != NULL ? "PASS" : "FAIL");
		printf("  HTTPD async future client engine create : %s\n", pClientEngine != NULL ? "PASS" : "FAIL");
		if ( pServerEngine ) printf("  HTTPD async future server engine start : %s\n", xrtNetEngineStart(pServerEngine) == XRT_NET_OK ? "PASS" : "FAIL");
		if ( pClientEngine ) printf("  HTTPD async future client engine start : %s\n", xrtNetEngineStart(pClientEngine) == XRT_NET_OK ? "PASS" : "FAIL");

		xrtHttpdConfigInit(&tSrvCfg);
		(void)xrtNetAddrParse(&tSrvCfg.tBindAddr, "127.0.0.1", 0);
		pServer = (pServerEngine && pClientEngine) ? xrtHttpdCreate(pServerEngine, &tSrvCfg, &tEvents, &tCtx) : NULL;
		printf("  HTTPD async future server create : %s\n", pServer != NULL ? "PASS" : "FAIL");
		printf("  HTTPD async future server start : %s\n", pServer && xrtHttpdStart(pServer) == XRT_NET_OK ? "PASS" : "FAIL");

		xrtHttpRequestInit(&tReq);
		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/async/future", (unsigned)xrtHttpdBoundPort(pServer));
		(void)xrtHttpRequestSetMethod(&tReq, "POST");
		(void)xrtHttpRequestSetURL(&tReq, sURL);
		(void)xrtHttpRequestSetBodyCopy(&tReq, "hello-async", 11, "text/plain");
		pFuture = (pClientEngine && pServer) ? xrtHttpExecuteAsync(pClientEngine, &tReq) : NULL;
		printf("  HTTPD async future create : %s\n", pFuture != NULL ? "PASS" : "FAIL");
		printf("  HTTPD async future wait : %s\n",
			pFuture && (iStatus = xrtNetFutureWait(pFuture, 3000u)) == XRT_NET_OK ? "PASS" : "FAIL");
		pResp = (pFuture && iStatus == XRT_NET_OK) ? (xhttpresponse*)xrtNetFutureValue(pFuture) : NULL;
		printf("  HTTPD async future code : %s\n", pResp && pResp->iStatusCode == 202 ? "PASS" : "FAIL");
		printf("  HTTPD async future body : %s\n",
			pResp && pResp->iBodyLen == 18 && memcmp(pResp->pBody, "future:hello-async", 18) == 0 ? "PASS" : "FAIL");
		printf("  HTTPD async future header : %s\n",
			pResp && xrtHttpResponseHeader(pResp, "X-Async") != NULL && strcmp(xrtHttpResponseHeader(pResp, "X-Async"), "thread") == 0 ? "PASS" : "FAIL");
		printf("  HTTPD async future callback count : %s\n", __Test_XHttpdWaitMin(&tCtx.iAsyncFutureCount, 1, 1000u) ? "PASS" : "FAIL");
		printf("  HTTPD async future request record : %s\n", strcmp(tCtx.aLastPath, "/async/future") == 0 && strcmp(tCtx.aLastBody, "hello-async") == 0 ? "PASS" : "FAIL");
		if ( pResp ) xrtHttpResponseDestroy(pResp);
		if ( pFuture ) xrtNetFutureDestroy(pFuture);
		pResp = NULL;
		pFuture = NULL;
		xrtHttpRequestUnit(&tReq);

		xrtHttpRequestInit(&tReq);
		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/async/fail", (unsigned)xrtHttpdBoundPort(pServer));
		(void)xrtHttpRequestSetURL(&tReq, sURL);
		pFuture = (pClientEngine && pServer) ? xrtHttpExecuteAsync(pClientEngine, &tReq) : NULL;
		printf("  HTTPD async fail create : %s\n", pFuture != NULL ? "PASS" : "FAIL");
		printf("  HTTPD async fail wait : %s\n",
			pFuture && (iStatus = xrtNetFutureWait(pFuture, 3000u)) == XRT_NET_OK ? "PASS" : "FAIL");
		pResp = (pFuture && iStatus == XRT_NET_OK) ? (xhttpresponse*)xrtNetFutureValue(pFuture) : NULL;
		printf("  HTTPD async fail mapped status : %s\n", pResp && pResp->iStatusCode == 500 ? "PASS" : "FAIL");
		printf("  HTTPD async fail mapped body : %s\n",
			pResp && strstr(pResp->pBody ? pResp->pBody : "", "Internal Server Error") != NULL ? "PASS" : "FAIL");
		printf("  HTTPD async fail request count : %s\n", __Test_XHttpdWaitMin(&tCtx.iRequestCount, 2, 1000u) ? "PASS" : "FAIL");
		xrtHttpCloseIdleConnections(pClientEngine);
		printf("  HTTPD async future close callback : %s\n", __Test_XHttpdWaitMin(&tCtx.iCloseCount, 1, 1000u) ? "PASS" : "FAIL");
		printf("  HTTPD async future path error free : %s\n", __Test_XHttpdAtomicLoad(&tCtx.iErrorCount) == 0 ? "PASS" : "FAIL");

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
		xsocket hRaw = XNET_SOCKET_INVALID;
		char sReq1[512];
		char sReq2[512];
		char aResp[2048];
		size_t iRespLen = 0u;
		long iOpenBefore;
		long iReqBefore;
		long iCloseBefore;

		memset(&tCtx, 0, sizeof(tCtx));
		memset(&tEvents, 0, sizeof(tEvents));
		tEvents.OnOpen = __Test_XHttpdOnOpen;
		tEvents.OnRequestAsync = __Test_XHttpdOnRequestAsyncManual;
		tEvents.OnClose = __Test_XHttpdOnClose;
		tEvents.OnError = __Test_XHttpdOnError;

		xrtNetEngineConfigInit(&tEngineCfg);
		tEngineCfg.iWorkerCount = 1;
		pServerEngine = xrtNetEngineCreate(&tEngineCfg);
		printf("  HTTPD async manual server engine create : %s\n", pServerEngine != NULL ? "PASS" : "FAIL");
		if ( pServerEngine ) printf("  HTTPD async manual server engine start : %s\n", xrtNetEngineStart(pServerEngine) == XRT_NET_OK ? "PASS" : "FAIL");

		xrtHttpdConfigInit(&tSrvCfg);
		(void)xrtNetAddrParse(&tSrvCfg.tBindAddr, "127.0.0.1", 0);
		pServer = pServerEngine ? xrtHttpdCreate(pServerEngine, &tSrvCfg, &tEvents, &tCtx) : NULL;
		printf("  HTTPD async manual server create : %s\n", pServer != NULL ? "PASS" : "FAIL");
		printf("  HTTPD async manual server start : %s\n", pServer && xrtHttpdStart(pServer) == XRT_NET_OK ? "PASS" : "FAIL");

		iOpenBefore = __Test_XHttpdAtomicLoad(&tCtx.iOpenCount);
		iReqBefore = __Test_XHttpdAtomicLoad(&tCtx.iRequestCount);
		iCloseBefore = __Test_XHttpdAtomicLoad(&tCtx.iCloseCount);
		hRaw = pServer ? __Test_XHttpdConnectLoopback(xrtHttpdBoundPort(pServer)) : XNET_SOCKET_INVALID;
		printf("  HTTPD async manual raw connect : %s\n", hRaw != XNET_SOCKET_INVALID ? "PASS" : "FAIL");
		snprintf(sReq1, sizeof(sReq1),
			"POST /async/manual HTTP/1.1\r\n"
			"Host: 127.0.0.1:%u\r\n"
			"Connection: keep-alive\r\n"
			"Content-Type: text/plain\r\n"
			"Content-Length: 3\r\n\r\n"
			"one",
			(unsigned)xrtHttpdBoundPort(pServer));
		snprintf(sReq2, sizeof(sReq2),
			"POST /async/manual HTTP/1.1\r\n"
			"Host: 127.0.0.1:%u\r\n"
			"Connection: close\r\n"
			"Content-Type: text/plain\r\n"
			"Content-Length: 3\r\n\r\n"
			"two",
			(unsigned)xrtHttpdBoundPort(pServer));
		printf("  HTTPD async manual request #1 send : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdSendAll(hRaw, sReq1, strlen(sReq1)) ? "PASS" : "FAIL");
		printf("  HTTPD async manual response #1 recv : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdRecvResponse(hRaw, aResp, sizeof(aResp), &iRespLen, 2000u) ? "PASS" : "FAIL");
		printf("  HTTPD async manual response #1 status : %s\n", strstr(aResp, "HTTP/1.1 200") != NULL ? "PASS" : "FAIL");
		printf("  HTTPD async manual response #1 header : %s\n", strstr(aResp, "Connection: keep-alive") != NULL ? "PASS" : "FAIL");
		printf("  HTTPD async manual response #1 body : %s\n", strstr(aResp, "\r\n\r\nmanual:one") != NULL ? "PASS" : "FAIL");
		printf("  HTTPD async manual request #2 send : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdSendAll(hRaw, sReq2, strlen(sReq2)) ? "PASS" : "FAIL");
		printf("  HTTPD async manual response #2 recv : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdRecvResponse(hRaw, aResp, sizeof(aResp), &iRespLen, 2000u) ? "PASS" : "FAIL");
		printf("  HTTPD async manual response #2 status : %s\n", strstr(aResp, "HTTP/1.1 200") != NULL ? "PASS" : "FAIL");
		printf("  HTTPD async manual response #2 header : %s\n", strstr(aResp, "Connection: close") != NULL ? "PASS" : "FAIL");
		printf("  HTTPD async manual response #2 body : %s\n", strstr(aResp, "\r\n\r\nmanual:two") != NULL ? "PASS" : "FAIL");
		printf("  HTTPD async manual open once : %s\n", __Test_XHttpdWaitMin(&tCtx.iOpenCount, iOpenBefore + 1, 1000u) && __Test_XHttpdAtomicLoad(&tCtx.iOpenCount) == iOpenBefore + 1 ? "PASS" : "FAIL");
		printf("  HTTPD async manual request count : %s\n", __Test_XHttpdWaitMin(&tCtx.iRequestCount, iReqBefore + 2, 1000u) ? "PASS" : "FAIL");
		printf("  HTTPD async manual response count : %s\n", __Test_XHttpdWaitMin(&tCtx.iAsyncManualCount, 2, 1000u) ? "PASS" : "FAIL");
		printf("  HTTPD async manual close callback : %s\n", __Test_XHttpdWaitMin(&tCtx.iCloseCount, iCloseBefore + 1, 2000u) ? "PASS" : "FAIL");
		printf("  HTTPD async manual final body : %s\n", strcmp(tCtx.aLastBody, "two") == 0 ? "PASS" : "FAIL");

		__Test_XHttpdCloseSocket(hRaw);
		if ( pServer ) xrtHttpdDestroy(pServer);
		if ( pServerEngine ) {
			xrtNetEngineStop(pServerEngine);
			xrtNetEngineDestroy(pServerEngine);
		}
	}

	#if !defined(XRT_NO_COROUTINE)
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
		tEvents.OnRequestAsync = __Test_XHttpdOnRequestAsyncCo;
		tEvents.OnClose = __Test_XHttpdOnClose;
		tEvents.OnError = __Test_XHttpdOnError;

		xrtNetEngineConfigInit(&tEngineCfg);
		tEngineCfg.iWorkerCount = 1;
		pServerEngine = xrtNetEngineCreate(&tEngineCfg);
		pClientEngine = xrtNetEngineCreate(&tEngineCfg);
		printf("  HTTPD async coroutine server engine create : %s\n", pServerEngine != NULL ? "PASS" : "FAIL");
		printf("  HTTPD async coroutine client engine create : %s\n", pClientEngine != NULL ? "PASS" : "FAIL");
		if ( pServerEngine ) printf("  HTTPD async coroutine server engine start : %s\n", xrtNetEngineStart(pServerEngine) == XRT_NET_OK ? "PASS" : "FAIL");
		if ( pClientEngine ) printf("  HTTPD async coroutine client engine start : %s\n", xrtNetEngineStart(pClientEngine) == XRT_NET_OK ? "PASS" : "FAIL");

		xrtHttpdConfigInit(&tSrvCfg);
		(void)xrtNetAddrParse(&tSrvCfg.tBindAddr, "127.0.0.1", 0);
		pServer = (pServerEngine && pClientEngine) ? xrtHttpdCreate(pServerEngine, &tSrvCfg, &tEvents, &tCtx) : NULL;
		printf("  HTTPD async coroutine server create : %s\n", pServer != NULL ? "PASS" : "FAIL");
		printf("  HTTPD async coroutine server start : %s\n", pServer && xrtHttpdStart(pServer) == XRT_NET_OK ? "PASS" : "FAIL");

		xrtHttpRequestInit(&tReq);
		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/async/co", (unsigned)xrtHttpdBoundPort(pServer));
		(void)xrtHttpRequestSetURL(&tReq, sURL);
		pFuture = (pClientEngine && pServer) ? xrtHttpExecuteAsync(pClientEngine, &tReq) : NULL;
		printf("  HTTPD async coroutine future create : %s\n", pFuture != NULL ? "PASS" : "FAIL");
		printf("  HTTPD async coroutine wait : %s\n",
			pFuture && (iStatus = xrtNetFutureWait(pFuture, 3000u)) == XRT_NET_OK ? "PASS" : "FAIL");
		pResp = (pFuture && iStatus == XRT_NET_OK) ? (xhttpresponse*)xrtNetFutureValue(pFuture) : NULL;
		printf("  HTTPD async coroutine code : %s\n", pResp && pResp->iStatusCode == 206 ? "PASS" : "FAIL");
		printf("  HTTPD async coroutine body : %s\n",
			pResp && pResp->iBodyLen == 7 && memcmp(pResp->pBody, "co:done", 7) == 0 ? "PASS" : "FAIL");
		printf("  HTTPD async coroutine header : %s\n",
			pResp && xrtHttpResponseHeader(pResp, "X-Async") != NULL && strcmp(xrtHttpResponseHeader(pResp, "X-Async"), "co") == 0 ? "PASS" : "FAIL");
		printf("  HTTPD async coroutine callback count : %s\n", __Test_XHttpdWaitMin(&tCtx.iAsyncCoroutineCount, 1, 1000u) ? "PASS" : "FAIL");
		xrtHttpCloseIdleConnections(pClientEngine);
		printf("  HTTPD async coroutine close callback : %s\n", __Test_XHttpdWaitMin(&tCtx.iCloseCount, 1, 1000u) ? "PASS" : "FAIL");

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
	#endif

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
