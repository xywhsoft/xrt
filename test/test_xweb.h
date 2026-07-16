#ifndef XRT_TEST_XWEB_H
#define XRT_TEST_XWEB_H

typedef struct {
	volatile long iRouteCount;
	volatile long iMeCount;
	volatile long iPostCount;
	volatile long iStreamCount;
	volatile long iSlowWriteDone;
	volatile long iErrorCount;
	volatile long iBodyBeginCount;
	volatile long iBodyChunkCount;
	volatile long iBodyEndCount;
	volatile long iBodyAsyncCount;
	volatile long iUpgradeCount;
	volatile long iUpgradeRecvCount;
	volatile long iUpgradeCloseCount;
	volatile long iUpgradeErrorCount;
	xnetstream* pUpgradeStream;
	char aUpgradeData[64];
	char aStreamBody[512];
	size_t iStreamBodyLen;
} __test_xweb_ctx;

typedef struct {
	__test_xweb_ctx* pCtx;
	char aBody[640];
} __test_xweb_async_body_task;

typedef struct {
	const char* sText;
	volatile long* pFreeCount;
} __test_xweb_owned_route_ctx;

typedef struct {
	char aOrder[64];
	size_t iOrderLen;
	volatile long iRouteCount;
	volatile long iFreeCount;
	bool bCommittedImmutable;
} __test_xweb_middleware_ctx;


static void __Test_XWebMiddlewareMark(__test_xweb_middleware_ctx* pCtx, char ch)
{
	if ( !pCtx || pCtx->iOrderLen + 1u >= sizeof(pCtx->aOrder) ) { return; }
	pCtx->aOrder[pCtx->iOrderLen++] = ch;
	pCtx->aOrder[pCtx->iOrderLen] = '\0';
}


static void __Test_XWebMiddlewareReset(__test_xweb_middleware_ctx* pCtx)
{
	if ( !pCtx ) { return; }
	pCtx->iOrderLen = 0u;
	pCtx->aOrder[0] = '\0';
	pCtx->bCommittedImmutable = false;
}


static void __Test_XWebMiddlewareFree(ptr pUserData)
{
	__test_xweb_middleware_ctx* pCtx = (__test_xweb_middleware_ctx*)pUserData;
	if ( pCtx ) { __Test_XHttpdAtomicInc(&pCtx->iFreeCount); }
}


static xwebaction __Test_XWebMiddlewareOuter(xwebrequest* pReq, xwebresponse* pResp,
	xwebnext* pNext, ptr pUserData)
{
	__test_xweb_middleware_ctx* pCtx = (__test_xweb_middleware_ctx*)pUserData;
	const char* sPath = xrtWebRequestPath(pReq);
	xwebaction iAction;
	uint32 iStatusBefore;
	__Test_XWebMiddlewareMark(pCtx, 'A');
	(void)xrtWebResponseSetHeader(pResp, "X-Middleware-Pre", "outer");
	if ( strcmp(sPath, "/mw/short") == 0 ) {
		xrtWebResponseStatus(pResp, 403u, NULL);
		return xrtWebResponseText(pResp, "short", "text/plain; charset=utf-8")
			? XWEB_DONE : XWEB_ERROR;
	}
	if ( strcmp(sPath, "/mw/error") == 0 ) {
		(void)xrtWebResponseSetHeader(pResp, "X-Stale", "must-reset");
		(void)xrtWebResponseText(pResp, "stale-body", "text/plain; charset=utf-8");
		return XWEB_ERROR;
	}
	iAction = xrtWebNext(pNext);
	if ( strcmp(sPath, "/mw/twice") == 0 ) {
		(void)xrtWebNext(pNext);
	}
	__Test_XWebMiddlewareMark(pCtx, 'E');
	if ( xrtWebResponseCommitted(pResp) ) {
		iStatusBefore = xrtWebResponseStatusCode(pResp);
		xrtWebResponseStatus(pResp, 599u, "Must Not Change");
		pCtx->bCommittedImmutable =
			xrtWebResponseStatusCode(pResp) == iStatusBefore &&
			!xrtWebResponseSetHeader(pResp, "X-Late", "one") &&
			!xrtWebResponseAddHeader(pResp, "X-Late", "two") &&
			xrtWebResponseRemoveHeader(pResp, "Content-Type") == 0u &&
			!xrtWebResponseSetTrailer(pResp, "X-Late-Trailer", "one") &&
			!xrtWebResponseText(pResp, "late", "text/plain") &&
			!xrtWebResponseReserveBody(pResp, 32u) &&
			!xrtWebResponseAppendText(pResp, "late") &&
			!xrtWebResponseRedirect(pResp, "/late", 302u);
	}
	else {
		(void)xrtWebResponseSetHeader(pResp, "X-Middleware-Post", "outer");
	}
	(void)iAction;
	// 返回 NEXT 时，运行库保留本次 xrtWebNext 的下游结果。
	return XWEB_NEXT;
}


static xwebaction __Test_XWebMiddlewareAuto(xwebrequest* pReq, xwebresponse* pResp,
	xwebnext* pNext, ptr pUserData)
{
	__test_xweb_middleware_ctx* pCtx = (__test_xweb_middleware_ctx*)pUserData;
	(void)pReq;
	(void)pNext;
	__Test_XWebMiddlewareMark(pCtx, 'B');
	(void)xrtWebResponseSetHeader(pResp, "X-Middleware-Auto", "yes");
	return XWEB_NEXT;
}


static xwebaction __Test_XWebMiddlewareInner(xwebrequest* pReq, xwebresponse* pResp,
	xwebnext* pNext, ptr pUserData)
{
	__test_xweb_middleware_ctx* pCtx = (__test_xweb_middleware_ctx*)pUserData;
	xwebaction iAction;
	(void)pReq;
	__Test_XWebMiddlewareMark(pCtx, 'C');
	iAction = xrtWebNext(pNext);
	__Test_XWebMiddlewareMark(pCtx, 'D');
	if ( !xrtWebResponseCommitted(pResp) ) {
		(void)xrtWebResponseSetHeader(pResp, "X-Middleware-Inner", "yes");
	}
	(void)iAction;
	return XWEB_NEXT;
}


static xwebaction __Test_XWebMiddlewareRoute(xwebrequest* pReq, xwebresponse* pResp,
	ptr pUserData)
{
	__test_xweb_middleware_ctx* pCtx = (__test_xweb_middleware_ctx*)pUserData;
	const char* sPath = xrtWebRequestPath(pReq);
	__Test_XWebMiddlewareMark(pCtx, 'R');
	__Test_XHttpdAtomicInc(&pCtx->iRouteCount);
	if ( strcmp(sPath, "/mw/async") == 0 ) { return XWEB_ASYNC; }
	if ( strcmp(sPath, "/mw/committed") == 0 ) {
		xrtWebResponseStatus(pResp, 200u, NULL);
		(void)xrtWebResponseSetHeader(pResp, "Content-Type", "text/plain; charset=utf-8");
		if ( !xrtWebResponseStart(pResp) ||
			!xrtWebResponseWriteText(pResp, "committed") ||
			!xrtWebResponseEnd(pResp) ) {
			return XWEB_ERROR;
		}
		return XWEB_DONE;
	}
	xrtWebResponseStatus(pResp, 202u, NULL);
	return xrtWebResponseText(pResp, "middleware", "text/plain; charset=utf-8")
		? XWEB_DONE : XWEB_ERROR;
}


// 内部函数：xweb request body 流式 begin
static bool __Test_XWebBodyStreamBegin(xwebrequest* pReq, ptr pUserData)
{
	__test_xweb_ctx* pCtx = (__test_xweb_ctx*)pUserData;
	if ( !pCtx || !pReq ) { return false; }
	__Test_XHttpdAtomicInc(&pCtx->iBodyBeginCount);
	memset(pCtx->aStreamBody, 0, sizeof(pCtx->aStreamBody));
	pCtx->iStreamBodyLen = 0u;
	return strncmp(xrtWebRequestPath(pReq), "/stream-body/", 13u) == 0 ||
		strncmp(xrtWebRequestPath(pReq), "/stream-body-async/", 19u) == 0;
}


// 内部函数：xweb request body 流式 chunk
static bool __Test_XWebBodyStreamChunk(xwebrequest* pReq, const void* pData, size_t iLen, ptr pUserData)
{
	__test_xweb_ctx* pCtx = (__test_xweb_ctx*)pUserData;
	size_t iCopy;
	(void)pReq;
	if ( !pCtx || (!pData && iLen > 0u) ) { return false; }
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


// 内部函数：xweb request body 流式 end
static xwebaction __Test_XWebBodyStreamEnd(xwebrequest* pReq, xwebresponse* pResp, ptr pUserData)
{
	__test_xweb_ctx* pCtx = (__test_xweb_ctx*)pUserData;
	char sBody[640];
	char sName[64];
	bool bName;
	if ( !pCtx || !pReq || !pResp ) { return XWEB_ERROR; }
	__Test_XHttpdAtomicInc(&pCtx->iBodyEndCount);
	bName = xrtWebRequestParam(pReq, "name", sName, sizeof(sName), NULL);
	snprintf(sBody, sizeof(sBody), "stream:%s:%s", bName ? sName : "none", pCtx->aStreamBody);
	return xrtWebResponseText(pResp, sBody, "text/plain; charset=utf-8") ? XWEB_DONE : XWEB_ERROR;
}


// 内部函数：xweb 路由参数处理器
static int32 __Test_XWebBodyStreamAsyncProc(ptr pArg, xfuture_result* pOut)
{
	__test_xweb_async_body_task* pTask = (__test_xweb_async_body_task*)pArg;
	xhttpdresponse* pResp = NULL;
	if ( !pTask || !pOut ) {
		if ( pTask ) { xrtFree(pTask); }
		return XRT_NET_ERROR;
	}
	__Test_XHttpdSleepMs(25u);
	pResp = xrtHttpdResponseCreate();
	if ( !pResp ) {
		xrtFree(pTask);
		return XRT_NET_ERROR;
	}
	xrtHttpdResponseSetStatus(pResp, 207u, NULL);
	(void)xrtHttpdResponseSetHeader(pResp, "X-Async", "xweb-body-end");
	(void)xrtHttpdResponseSetBodyCopy(pResp, pTask->aBody, strlen(pTask->aBody), "text/plain; charset=utf-8");
	if ( pTask->pCtx ) { __Test_XHttpdAtomicInc(&pTask->pCtx->iBodyAsyncCount); }
	pOut->pValue = pResp;
	xrtFree(pTask);
	return XRT_NET_OK;
}


static xfuture* __Test_XWebBodyStreamEndAsync(xwebrequest* pReq, ptr pUserData)
{
	__test_xweb_ctx* pCtx = (__test_xweb_ctx*)pUserData;
	__test_xweb_async_body_task* pTask;
	char sName[64];
	bool bName;
	if ( !pCtx || !pReq || strncmp(xrtWebRequestPath(pReq), "/stream-body-async/", 19u) != 0 ) { return NULL; }
	__Test_XHttpdAtomicInc(&pCtx->iBodyEndCount);
	bName = xrtWebRequestParam(pReq, "name", sName, sizeof(sName), NULL);
	pTask = (__test_xweb_async_body_task*)xrtMalloc(sizeof(__test_xweb_async_body_task));
	if ( !pTask ) { return NULL; }
	memset(pTask, 0, sizeof(*pTask));
	pTask->pCtx = pCtx;
	snprintf(pTask->aBody, sizeof(pTask->aBody), "stream-async:%s:%s", bName ? sName : "none", pCtx->aStreamBody);
	return xTaskRunThread(__Test_XWebBodyStreamAsyncProc, pTask, 0);
}


static xwebaction __Test_XWebUserHandler(xwebrequest* pReq, xwebresponse* pResp, ptr pUserData)
{
	__test_xweb_ctx* pCtx = (__test_xweb_ctx*)pUserData;
	char sId[64];
	char sName[64];
	char sCookie[64];
	char sBody[256];
	char* sIdCopy;
	char* sNameCopy;
	char* sCookieCopy;
	char* sHeaderCopy;
	char* sHeaderNameCopy;
	char* sHeaderValueCopy;
	char* sParamNameCopy;
	char* sParamValueCopy;
	char* sQueryNameCopy;
	char* sQueryValueAtCopy;
	char* sCookieNameCopy;
	char* sCookieValueAtCopy;
	char* sTargetCopy;
	char* sPathCopy;
	char* sQueryCopy;
	char* sLocalAddr;
	char* sRemoteAddr;
	const char* sParamValueView = NULL;
	size_t iParamValueViewLen = 0u;
	size_t iHeaderCount;
	size_t iParamCount;
	size_t iQueryCount;
	size_t iCookieCount;
	xrtquerypair tQueryPair;
	xrtcookiepair tCookiePair;
	size_t iLen = 0u;
	bool bId;
	bool bName;
	bool bCookie;
	bool bParamView;
	bool bQueryPair;
	bool bCookiePair;
	if ( pCtx ) { __Test_XHttpdAtomicInc(&pCtx->iRouteCount); }
	bId = xrtWebRequestParam(pReq, "id", sId, sizeof(sId), &iLen);
	bName = xrtWebRequestQueryValue(pReq, "name", sName, sizeof(sName), NULL);
	bCookie = xrtWebRequestCookie(pReq, "sid", sCookie, sizeof(sCookie), NULL);
	sIdCopy = xrtWebRequestParamCopy(pReq, "id");
	sNameCopy = xrtWebRequestQueryValueCopy(pReq, "name");
	sCookieCopy = xrtWebRequestCookieCopy(pReq, "sid");
	sHeaderCopy = xrtWebRequestHeaderCopy(pReq, "Cookie");
	sHeaderNameCopy = xrtWebRequestHeaderNameCopyAt(pReq, 0u);
	sHeaderValueCopy = xrtWebRequestHeaderValueCopyAt(pReq, 0u);
	sParamNameCopy = xrtWebRequestParamNameCopyAt(pReq, 0u);
	sParamValueCopy = xrtWebRequestParamValueCopyAt(pReq, 0u);
	sQueryNameCopy = xrtWebRequestQueryNameCopyAt(pReq, 0u);
	sQueryValueAtCopy = xrtWebRequestQueryValueCopyAt(pReq, 0u);
	sCookieNameCopy = xrtWebRequestCookieNameCopyAt(pReq, 0u);
	sCookieValueAtCopy = xrtWebRequestCookieValueCopyAt(pReq, 0u);
	sTargetCopy = xrtWebRequestTargetCopy(pReq);
	sPathCopy = xrtWebRequestPathCopy(pReq);
	sQueryCopy = xrtWebRequestQueryCopy(pReq);
	sLocalAddr = xrtWebRequestLocalAddrCopy(pReq);
	sRemoteAddr = xrtWebRequestRemoteAddrCopy(pReq);
	iHeaderCount = xrtWebRequestHeaderCount(pReq);
	iParamCount = xrtWebRequestParamCount(pReq);
	iQueryCount = xrtWebRequestQueryCount(pReq);
	iCookieCount = xrtWebRequestCookieCount(pReq);
	bParamView = xrtWebRequestParamValueViewAt(pReq, 0u, &sParamValueView, &iParamValueViewLen);
	bQueryPair = xrtWebRequestQueryPairAt(pReq, 0u, &tQueryPair);
	bCookiePair = xrtWebRequestCookiePairAt(pReq, 0u, &tCookiePair);
	if ( strcmp(sIdCopy, "42") != 0 || strcmp(sNameCopy, "xlang") != 0 ||
	     strcmp(sCookieCopy, "abc") != 0 || strstr(sHeaderCopy, "sid=abc") == NULL ||
	     iHeaderCount < 2u || sHeaderNameCopy[0] == '\0' || sHeaderValueCopy[0] == '\0' ||
	     xrtWebRequestHeaderNameAt(pReq, iHeaderCount) != NULL ||
	     xrtWebRequestHeaderValueAt(pReq, iHeaderCount) != NULL ||
	     iParamCount != 1u || strcmp(sParamNameCopy, "id") != 0 || strcmp(sParamValueCopy, "42") != 0 ||
	     !bParamView || !sParamValueView || iParamValueViewLen != 2u ||
	     memcmp(sParamValueView, "42", 2u) != 0 ||
	     xrtWebRequestParamNameAt(pReq, iParamCount) != NULL ||
	     xrtWebRequestParamValueViewAt(pReq, iParamCount, NULL, NULL) ||
	     iQueryCount != 1u || strcmp(sQueryNameCopy, "name") != 0 || strcmp(sQueryValueAtCopy, "xlang") != 0 ||
	     !xrtWebRequestQueryHasValueAt(pReq, 0u) || xrtWebRequestQueryHasValueAt(pReq, 1u) ||
	     !bQueryPair || tQueryPair.tKey.iLen != 4u || strncmp(tQueryPair.tKey.sPtr, "name", 4u) != 0 ||
	     tQueryPair.tValue.iLen != 5u || strncmp(tQueryPair.tValue.sPtr, "xlang", 5u) != 0 ||
	     xrtWebRequestQueryPairAt(pReq, iQueryCount, &tQueryPair) ||
	     iCookieCount != 1u || strcmp(sCookieNameCopy, "sid") != 0 || strcmp(sCookieValueAtCopy, "abc") != 0 ||
	     !bCookiePair || tCookiePair.tName.iLen != 3u || strncmp(tCookiePair.tName.sPtr, "sid", 3u) != 0 ||
	     tCookiePair.tValue.iLen != 3u || strncmp(tCookiePair.tValue.sPtr, "abc", 3u) != 0 ||
	     xrtWebRequestCookiePairAt(pReq, iCookieCount, &tCookiePair) ||
	     strcmp(xrtWebRequestMethod(pReq), "GET") != 0 ||
	     xrtWebRequestMethodId(pReq) != XHTTPD_METHOD_GET ||
	     strcmp(sTargetCopy, "/users/42?name=xlang") != 0 ||
	     strcmp(sPathCopy, "/users/42") != 0 ||
	     strcmp(sQueryCopy, "name=xlang") != 0 ||
	     xrtWebRequestContentLength(pReq) != -1 ||
	     xrtWebRequestLocalPort(pReq) <= 0 ||
	     xrtWebRequestRemotePort(pReq) <= 0 ||
	     !xrtWebRequestConnectionOpen(pReq) ||
	     sLocalAddr[0] == '\0' ||
	     sRemoteAddr[0] == '\0' ) {
		bId = false;
	}
	snprintf(sBody, sizeof(sBody), "id=%s;name=%s;sid=%s",
		bId ? sId : "",
		bName ? sName : "",
		bCookie ? sCookie : "");
	xrtFree(sIdCopy);
	xrtFree(sNameCopy);
	xrtFree(sCookieCopy);
	xrtFree(sHeaderCopy);
	xrtFree(sHeaderNameCopy);
	xrtFree(sHeaderValueCopy);
	xrtFree(sParamNameCopy);
	xrtFree(sParamValueCopy);
	xrtFree(sQueryNameCopy);
	xrtFree(sQueryValueAtCopy);
	xrtFree(sCookieNameCopy);
	xrtFree(sCookieValueAtCopy);
	xrtFree(sTargetCopy);
	xrtFree(sPathCopy);
	xrtFree(sQueryCopy);
	xrtFree(sLocalAddr);
	xrtFree(sRemoteAddr);
	xrtWebResponseStatus(pResp, 201u, NULL);
	(void)xrtWebResponseSetHeader(pResp, "X-XWeb", "route");
	return xrtWebResponseText(pResp, sBody, "text/plain; charset=utf-8") ? XWEB_DONE : XWEB_ERROR;
}


// 内部函数：xweb 静态优先级处理器
static xwebaction __Test_XWebMeHandler(xwebrequest* pReq, xwebresponse* pResp, ptr pUserData)
{
	__test_xweb_ctx* pCtx = (__test_xweb_ctx*)pUserData;
	(void)pReq;
	if ( pCtx ) { __Test_XHttpdAtomicInc(&pCtx->iMeCount); }
	(void)xrtWebResponseSetHeader(pResp, "X-XWeb", "me");
	return xrtWebResponseText(pResp, "me", "text/plain; charset=utf-8") ? XWEB_DONE : XWEB_ERROR;
}


// 内部函数：xweb 普通响应 body 追加处理器
static xwebaction __Test_XWebAppendHandler(xwebrequest* pReq, xwebresponse* pResp, ptr pUserData)
{
	const void* pBody;
	size_t iBodyLen = 0u;
	(void)pReq;
	(void)pUserData;
	xrtWebResponseStatus(pResp, 200u, "Fine");
	(void)xrtWebResponseSetHeader(pResp, "Content-Type", "text/plain; charset=utf-8");
	(void)xrtWebResponseAddHeader(pResp, "X-Test", "one");
	(void)xrtWebResponseAddHeader(pResp, "X-Test", "two");
	(void)xrtWebResponseAddHeader(pResp, "X-Remove", "one");
	(void)xrtWebResponseAddHeader(pResp, "X-Remove", "two");
	if ( xrtWebResponseRemoveHeader(pResp, "X-Remove") != 2u ||
		xrtWebResponseHeader(pResp, "X-Remove") != NULL ) { return XWEB_ERROR; }
	if ( !xrtWebResponseReserveBody(pResp, 16u) ) { return XWEB_ERROR; }
	if ( !xrtWebResponseAppendText(pResp, "append") ) { return XWEB_ERROR; }
	if ( !xrtWebResponseAppendBody(pResp, "-", 1u) ) { return XWEB_ERROR; }
	if ( !xrtWebResponseAppendText(pResp, "body") ) { return XWEB_ERROR; }
	pBody = xrtWebResponseBodyView(pResp, &iBodyLen);
	if ( xrtWebResponseStatusCode(pResp) != 200u ||
	     strcmp(xrtWebResponseReason(pResp), "Fine") != 0 ||
	     xrtWebResponseHeaderCount(pResp) != 4u ||
	     strcmp(xrtWebResponseHeader(pResp, "X-Test"), "one") != 0 ||
	     strcmp(xrtWebResponseHeaderNameAt(pResp, 2u), "X-Test") != 0 ||
	     strcmp(xrtWebResponseHeaderValueAt(pResp, 3u), "two") != 0 ||
	     xrtWebResponseHeaderNameAt(pResp, 4u) != NULL ||
	     xrtWebResponseHeaderValueAt(pResp, 4u) != NULL ||
	     !pBody || iBodyLen != 11u || memcmp(pBody, "append-body", 11u) != 0 ||
	     xrtWebResponseCommitted(pResp) ) {
		return XWEB_ERROR;
	}
	return XWEB_DONE;
}


static xwebaction __Test_XWebTrailerHandler(xwebrequest* pReq, xwebresponse* pResp, ptr pUserData)
{
	(void)pReq;
	(void)pUserData;
	xrtWebResponseStatus(pResp, 200u, NULL);
	if ( !xrtWebResponseText(pResp, "xweb-trailer", "text/plain; charset=utf-8") ||
		!xrtWebResponseSetTrailer(pResp, "X-Web-End", "one") ||
		!xrtWebResponseAddTrailer(pResp, "X-Web-Meta", "two") ||
		xrtWebResponseTrailerCount(pResp) != 2u ||
		strcmp(xrtWebResponseTrailer(pResp, "X-Web-End"), "one") != 0 ||
		strcmp(xrtWebResponseTrailerNameAt(pResp, 1u), "X-Web-Meta") != 0 ||
		strcmp(xrtWebResponseTrailerValueAt(pResp, 1u), "two") != 0 ||
		xrtWebResponseTrailerNameAt(pResp, 2u) != NULL ||
		xrtWebResponseTrailerValueAt(pResp, 2u) != NULL ) {
		return XWEB_ERROR;
	}
	return XWEB_DONE;
}


static xwebaction __Test_XWebCompressionHandler(xwebrequest* pReq, xwebresponse* pResp, ptr pUserData)
{
	char aBody[2048];
	const char* sPath = xrtWebRequestPath(pReq);
	(void)pUserData;
	memset(aBody, 'C', sizeof(aBody));
	xrtWebResponseStatus(pResp, 200u, NULL);
	(void)xrtWebResponseSetHeader(pResp, "Content-Length", "2048");
	(void)xrtWebResponseSetHeader(pResp, "ETag", "\"strong-before-transform\"");
	(void)xrtWebResponseSetHeader(pResp, "Digest", "sha-256=stale");
	if ( sPath && strcmp(sPath, "/compress-no-transform") == 0 ) {
		(void)xrtWebResponseSetHeader(pResp, "Cache-Control", "private, no-transform");
	}
	return xrtWebResponseBody(pResp, aBody, sizeof(aBody), "text/plain; charset=utf-8")
		? XWEB_DONE : XWEB_ERROR;
}


static void __Test_XWebUpgradeOnRecv(ptr pOwner, xnetstream* pStream, xnetchain* pChain)
{
	__test_xweb_ctx* pCtx = (__test_xweb_ctx*)pOwner;
	size_t iBytes;
	size_t iUsed;
	size_t iCopy;
	if ( !pCtx || !pStream || !pChain ) return;
	iBytes = xrtNetChainBytes(pChain);
	iUsed = strlen(pCtx->aUpgradeData);
	iCopy = iBytes;
	if ( iCopy > sizeof(pCtx->aUpgradeData) - 1u - iUsed ) {
		iCopy = sizeof(pCtx->aUpgradeData) - 1u - iUsed;
	}
	if ( iCopy > 0u ) {
		(void)xrtNetChainPeek(pChain, pCtx->aUpgradeData + iUsed, iCopy);
		pCtx->aUpgradeData[iUsed + iCopy] = '\0';
	}
	xrtNetChainConsume(pChain, iBytes);
	if ( strstr(pCtx->aUpgradeData, "PING") &&
		__Test_XHttpdAtomicLoad(&pCtx->iUpgradeRecvCount) == 0 ) {
		if ( xrtNetStreamSend(pStream, "XWEB-UPGRADE-OK", 15u) == XRT_NET_OK ) {
			__Test_XHttpdAtomicInc(&pCtx->iUpgradeRecvCount);
		} else {
			__Test_XHttpdAtomicInc(&pCtx->iUpgradeErrorCount);
		}
	}
}


static void __Test_XWebUpgradeOnClose(ptr pOwner, xnetstream* pStream, xnet_result iReason)
{
	__test_xweb_ctx* pCtx = (__test_xweb_ctx*)pOwner;
	(void)pStream;
	(void)iReason;
	if ( pCtx ) __Test_XHttpdAtomicInc(&pCtx->iUpgradeCloseCount);
}


static void __Test_XWebUpgradeOnError(ptr pOwner, xnetstream* pStream, int iSysErr)
{
	__test_xweb_ctx* pCtx = (__test_xweb_ctx*)pOwner;
	(void)pStream;
	(void)iSysErr;
	if ( pCtx ) __Test_XHttpdAtomicInc(&pCtx->iUpgradeErrorCount);
}


static const xnetstreamevents __g_Test_XWebUpgradeEvents = {
	.OnRecv = __Test_XWebUpgradeOnRecv,
	.OnClose = __Test_XWebUpgradeOnClose,
	.OnError = __Test_XWebUpgradeOnError
};


static xwebaction __Test_XWebUpgradeHandler(xwebrequest* pReq, xwebresponse* pResp, ptr pUserData)
{
	__test_xweb_ctx* pCtx = (__test_xweb_ctx*)pUserData;
	xnetstream* pStream = NULL;
	(void)pReq;
	if ( !pCtx || !pResp ) return XWEB_ERROR;
	xrtWebResponseStatus(pResp, 101u, "Switching Protocols");
	if ( !xrtWebResponseSetHeader(pResp, "Connection", "Upgrade") ||
		!xrtWebResponseSetHeader(pResp, "Upgrade", "xweb-test") ||
		!xrtWebResponseUpgrade(pResp, &__g_Test_XWebUpgradeEvents, pCtx, &pStream) ||
		!xrtWebResponseCommitted(pResp) ) {
		return XWEB_ERROR;
	}
	pCtx->pUpgradeStream = pStream;
	__Test_XHttpdAtomicInc(&pCtx->iUpgradeCount);
	return XWEB_DONE;
}


// 验证高层 xweb 可直接使用 xhttpd 的分段文件发送能力。
static xwebaction __Test_XWebFileRangeHandler(xwebrequest* pReq, xwebresponse* pResp, ptr pUserData)
{
	const char* sPath = (const char*)pUserData;
	(void)pReq;
	xrtWebResponseStatus(pResp, 206u, NULL);
	(void)xrtWebResponseSetHeader(pResp, "Content-Type", "text/plain; charset=utf-8");
	return xrtWebResponseFileRange(pResp, sPath, 2u, 4u, 4096u) ? XWEB_DONE : XWEB_ERROR;
}


// 验证业务回调可以显式结束底层连接，且无需暴露可逃逸的连接裸指针。
static xwebaction __Test_XWebCloseHandler(xwebrequest* pReq, xwebresponse* pResp, ptr pUserData)
{
	(void)pResp;
	(void)pUserData;
	return xrtWebRequestConnectionOpen(pReq) && xrtWebRequestCloseConnection(pReq, false) ? XWEB_DONE : XWEB_ERROR;
}


// 内部函数：xweb POST 请求体处理器
static xwebaction __Test_XWebPostHandler(xwebrequest* pReq, xwebresponse* pResp, ptr pUserData)
{
	__test_xweb_ctx* pCtx = (__test_xweb_ctx*)pUserData;
	const char* pBody;
	size_t iBodyLen = 0u;
	char sBody[128];
	if ( pCtx ) { __Test_XHttpdAtomicInc(&pCtx->iPostCount); }
	if ( strcmp(xrtWebRequestPath(pReq), "/multipart") == 0 ) {
		char* sTitle = xrtWebRequestMultipartValueCopy(pReq, "title");
		char* sPart0Name = xrtWebRequestMultipartNameCopyAt(pReq, 0u);
		char* sPart0Text = xrtWebRequestMultipartBodyTextCopyAt(pReq, 0u);
		char* sPart1Name = xrtWebRequestMultipartNameCopyAt(pReq, 1u);
		char* sPart1File = xrtWebRequestMultipartFileNameCopyAt(pReq, 1u);
		char* sPart1Type = xrtWebRequestMultipartContentTypeCopyAt(pReq, 1u);
		const void* pPart1Data = NULL;
		size_t iPart1Len = 0u;
		xrtmultipartpartview tPart;
		bool bOk = xrtWebRequestIsMultipartForm(pReq) &&
			xrtWebRequestMultipartCount(pReq) == 2u &&
			xrtWebRequestMultipartPartAt(pReq, 0u, &tPart) &&
			(tPart.iFlags & XRT_MULTIPART_F_HAS_NAME) != 0u &&
			!xrtWebRequestMultipartIsFileAt(pReq, 0u) &&
			xrtWebRequestMultipartIsFileAt(pReq, 1u) &&
			xrtWebRequestMultipartBodyViewAt(pReq, 1u, &pPart1Data, &iPart1Len) &&
			xrtWebRequestMultipartBodySizeAt(pReq, 1u) == 9u &&
			iPart1Len == 9u && pPart1Data && memcmp(pPart1Data, "file-data", 9u) == 0 &&
			strcmp(sTitle, "hello multipart") == 0 &&
			strcmp(sPart0Name, "title") == 0 &&
			strcmp(sPart0Text, "hello multipart") == 0 &&
			strcmp(sPart1Name, "upload") == 0 &&
			strcmp(sPart1File, "note.txt") == 0 &&
			strcmp(sPart1Type, "text/plain") == 0 &&
			!xrtWebRequestMultipartPartAt(pReq, 2u, &tPart) &&
			xrtWebRequestMultipartBodySizeAt(pReq, 2u) == 0u;
		xrtFree(sTitle);
		xrtFree(sPart0Name);
		xrtFree(sPart0Text);
		xrtFree(sPart1Name);
		xrtFree(sPart1File);
		xrtFree(sPart1Type);
		return xrtWebResponseText(pResp, bOk ? "multipart-ok" : "multipart-bad", "text/plain; charset=utf-8") ? XWEB_DONE : XWEB_ERROR;
	}
	if ( strcmp(xrtWebRequestPath(pReq), "/form") == 0 ) {
		char* sName = xrtWebRequestFormValueCopy(pReq, "name");
		char* sCity = xrtWebRequestFormValueCopy(pReq, "city");
		char* sFormName = xrtWebRequestFormNameCopyAt(pReq, 0u);
		char* sFormValue = xrtWebRequestFormValueCopyAt(pReq, 0u);
		xrtquerypair tFormPair;
		bool bOk = xrtWebRequestIsFormUrlEncoded(pReq) &&
			xrtWebRequestFormCount(pReq) == 2u &&
			xrtWebRequestFormHasValueAt(pReq, 0u) &&
			xrtWebRequestFormPairAt(pReq, 0u, &tFormPair) &&
			tFormPair.tKey.iLen == 4u && strncmp(tFormPair.tKey.sPtr, "name", 4u) == 0 &&
			strcmp(sName, "alice bob") == 0 &&
			strcmp(sCity, "shanghai") == 0 &&
			strcmp(sFormName, "name") == 0 &&
			strcmp(sFormValue, "alice bob") == 0 &&
			!xrtWebRequestFormPairAt(pReq, 2u, &tFormPair) &&
			!xrtWebRequestFormHasValueAt(pReq, 2u);
		xrtFree(sName);
		xrtFree(sCity);
		xrtFree(sFormName);
		xrtFree(sFormValue);
		return xrtWebResponseText(pResp, bOk ? "form-ok" : "form-bad", "text/plain; charset=utf-8") ? XWEB_DONE : XWEB_ERROR;
	}
	pBody = (const char*)xrtWebRequestBody(pReq, &iBodyLen);
	if ( xrtWebRequestContentLength(pReq) != (int64)iBodyLen ) { return XWEB_ERROR; }
	if ( iBodyLen >= sizeof(sBody) ) { iBodyLen = sizeof(sBody) - 1u; }
	if ( pBody && iBodyLen > 0u ) { memcpy(sBody, pBody, iBodyLen); }
	sBody[iBodyLen] = '\0';
	return xrtWebResponseJsonText(pResp, sBody[0] ? sBody : "{}") ? XWEB_DONE : XWEB_ERROR;
}


// 内部函数：xweb 重定向处理器
static xwebaction __Test_XWebRedirectHandler(xwebrequest* pReq, xwebresponse* pResp, ptr pUserData)
{
	(void)pReq;
	(void)pUserData;
	return xrtWebResponseRedirect(pResp, "/users/me", 302u) ? XWEB_DONE : XWEB_ERROR;
}


// 内部函数：xweb Cookie 响应处理器
static xwebaction __Test_XWebCookieHandler(xwebrequest* pReq, xwebresponse* pResp, ptr pUserData)
{
	(void)pReq;
	(void)pUserData;
	if ( !xrtWebResponseCookie(pResp, "sid", "xyz", "/", 3600, XRT_SAME_SITE_LAX, XRT_SET_COOKIE_F_SECURE | XRT_SET_COOKIE_F_HTTP_ONLY) ) {
		return XWEB_ERROR;
	}
	return xrtWebResponseText(pResp, "cookie", "text/plain; charset=utf-8") ? XWEB_DONE : XWEB_ERROR;
}


// 内部函数：xweb 删除 Cookie 响应处理器
static xwebaction __Test_XWebDeleteCookieHandler(xwebrequest* pReq, xwebresponse* pResp, ptr pUserData)
{
	(void)pReq;
	(void)pUserData;
	if ( !xrtWebResponseDeleteCookie(pResp, "sid", "/") ) {
		return XWEB_ERROR;
	}
	return xrtWebResponseText(pResp, "delete-cookie", "text/plain; charset=utf-8") ? XWEB_DONE : XWEB_ERROR;
}


// 内部函数：xweb 流式响应处理器
static xwebaction __Test_XWebStreamHandler(xwebrequest* pReq, xwebresponse* pResp, ptr pUserData)
{
	__test_xweb_ctx* pCtx = (__test_xweb_ctx*)pUserData;
	const char* sTail = "ok";
	(void)pReq;
	if ( pCtx ) { __Test_XHttpdAtomicInc(&pCtx->iStreamCount); }
	xrtWebResponseStatus(pResp, 200u, NULL);
	(void)xrtWebResponseSetHeader(pResp, "Content-Type", "text/plain; charset=utf-8");
	if ( !xrtWebResponseStart(pResp) ) { return XWEB_ERROR; }
	if ( !xrtWebResponseWriteText(pResp, "stream-") ) { return XWEB_ERROR; }
	if ( !xrtWebResponseSend(pResp, sTail, strlen(sTail)) ) { return XWEB_ERROR; }
	return xrtWebResponseEnd(pResp) ? XWEB_DONE : XWEB_ERROR;
}


static xwebaction __Test_XWebSlowWriteHandler(xwebrequest* pReq, xwebresponse* pResp, ptr pUserData)
{
	__test_xweb_ctx* pCtx = (__test_xweb_ctx*)pUserData;
	static char aChunk[8192];
	static bool bChunkReady = false;
	(void)pReq;
	if ( pCtx ) { __Test_XHttpdAtomicInc(&pCtx->iStreamCount); }
	if ( !bChunkReady ) {
		memset(aChunk, 'W', sizeof(aChunk));
		bChunkReady = true;
	}
	xrtWebResponseStatus(pResp, 200u, NULL);
	(void)xrtWebResponseSetHeader(pResp, "Content-Type", "application/octet-stream");
	if ( !xrtWebResponseStart(pResp) ) { return XWEB_ERROR; }
	for ( uint32 i = 0u; i < 2048u; ++i ) {
		if ( !xrtWebResponseSend(pResp, aChunk, sizeof(aChunk)) ) {
			if ( pCtx ) { __Test_XHttpdAtomicInc(&pCtx->iSlowWriteDone); }
			return XWEB_ERROR;
		}
	}
	if ( pCtx ) { __Test_XHttpdAtomicInc(&pCtx->iSlowWriteDone); }
	return xrtWebResponseEnd(pResp) ? XWEB_DONE : XWEB_ERROR;
}


// 内部函数：xweb 错误处理器
static xwebaction __Test_XWebErrorHandler(xwebrequest* pReq, xwebresponse* pResp, uint32 iStatusCode, const char* sMessage, ptr pUserData)
{
	__test_xweb_ctx* pCtx = (__test_xweb_ctx*)pUserData;
	char sBody[128];
	(void)pReq;
	if ( pCtx ) { __Test_XHttpdAtomicInc(&pCtx->iErrorCount); }
	snprintf(sBody, sizeof(sBody), "error:%u:%s", (unsigned)iStatusCode, sMessage ? sMessage : "");
	xrtWebResponseStatus(pResp, iStatusCode, NULL);
	(void)xrtWebResponseSetHeader(pResp, "X-XWeb-Error", "custom");
	return xrtWebResponseText(pResp, sBody, "text/plain; charset=utf-8") ? XWEB_DONE : XWEB_ERROR;
}


// 内部函数：xweb app/vhost 文本处理器
static xwebaction __Test_XWebTextHandler(xwebrequest* pReq, xwebresponse* pResp, ptr pUserData)
{
	const char* sText = (const char*)pUserData;
	(void)pReq;
	return xrtWebResponseText(pResp, sText ? sText : "", "text/plain; charset=utf-8") ? XWEB_DONE : XWEB_ERROR;
}


// 内部函数：返回 TLS SNI，验证 xweb 可以读取握手层域名
static xwebaction __Test_XWebSNIHandler(xwebrequest* pReq, xwebresponse* pResp, ptr pUserData)
{
	char* sSNI;
	bool bOk;
	(void)pUserData;
	sSNI = xrtWebRequestTlsSNICopy(pReq);
	bOk = xrtWebResponseText(pResp, sSNI ? sSNI : "", "text/plain; charset=utf-8");
	xrtFree(sSNI);
	return bOk ? XWEB_DONE : XWEB_ERROR;
}


// 内部函数：xweb route owned userdata 处理器
static xwebaction __Test_XWebOwnedTextHandler(xwebrequest* pReq, xwebresponse* pResp, ptr pUserData)
{
	__test_xweb_owned_route_ctx* pCtx = (__test_xweb_owned_route_ctx*)pUserData;
	(void)pReq;
	return xrtWebResponseText(pResp, pCtx && pCtx->sText ? pCtx->sText : "", "text/plain; charset=utf-8") ? XWEB_DONE : XWEB_ERROR;
}


// 内部函数：释放 route owned userdata
static void __Test_XWebOwnedTextFree(ptr pUserData)
{
	__test_xweb_owned_route_ctx* pCtx = (__test_xweb_owned_route_ctx*)pUserData;
	if ( pCtx && pCtx->pFreeCount ) { __Test_XHttpdAtomicInc(pCtx->pFreeCount); }
	xrtFree(pCtx);
}


static xhttpresponse* __Test_XWebRequestHost(const char* sURL, const char* sMethod, const char* sBody, const char* sHost, xnet_result* pStatus);


// 内部函数：发送同步 HTTP 请求
static xhttpresponse* __Test_XWebRequest(const char* sURL, const char* sMethod, const char* sBody, xnet_result* pStatus)
{
	return __Test_XWebRequestHost(sURL, sMethod, sBody, NULL, pStatus);
}


static xhttpresponse* __Test_XWebRequestNoRedirect(const char* sURL, const char* sMethod, xnet_result* pStatus)
{
	xhttpclientconfig tConfig;
	xhttpclient* pClient;
	xhttprequest tReq;
	xhttpresponse* pResp = NULL;
	xrtHttpClientConfigInit(&tConfig);
	tConfig.iFlags = XHTTP_CLIENT_F_NONE;
	tConfig.iMaxRedirects = 0u;
	pClient = xrtHttpClientCreateEx(NULL, &tConfig);
	xrtHttpRequestInit(&tReq);
	(void)xrtHttpRequestSetURL(&tReq, sURL);
	if ( sMethod ) { (void)xrtHttpRequestSetMethod(&tReq, sMethod); }
	if ( pClient ) { pResp = xrtHttpClientExecuteSync(pClient, &tReq, pStatus); }
	else if ( pStatus ) { *pStatus = XRT_NET_ERROR; }
	xrtHttpRequestUnit(&tReq);
	if ( pClient ) { xrtHttpClientDestroy(pClient); }
	return pResp;
}


// 内部函数：发送带 Host 覆盖的同步 HTTP 请求
// 内部函数：发送带 Content-Type 的同步 HTTP 请求
static xhttpresponse* __Test_XWebRequestContentType(const char* sURL, const char* sMethod, const char* sBody, const char* sContentType, xnet_result* pStatus)
{
	xhttprequest tReq;
	xhttpresponse* pResp;
	xrtHttpRequestInit(&tReq);
	(void)xrtHttpRequestSetURL(&tReq, sURL);
	if ( sURL && strncmp(sURL, "https://", 8u) == 0 ) { xrtHttpRequestSetVerifyPeer(&tReq, false); }
	if ( sMethod ) { (void)xrtHttpRequestSetMethod(&tReq, sMethod); }
	(void)xrtHttpRequestSetHeader(&tReq, "Cookie", "sid=abc");
	if ( sBody ) { (void)xrtHttpRequestSetBodyCopy(&tReq, sBody, strlen(sBody), sContentType ? sContentType : "application/octet-stream"); }
	pResp = xrtHttpExecuteSync(NULL, &tReq, pStatus);
	xrtHttpRequestUnit(&tReq);
	return pResp;
}


static xhttpresponse* __Test_XWebRequestFormData(const char* sURL,
	const xhttpformdata* pForm, xnet_result* pStatus)
{
	xhttprequest tReq;
	xhttpresponse* pResp = NULL;
	xrtHttpRequestInit(&tReq);
	if ( xrtHttpRequestSetURL(&tReq, sURL) &&
		xrtHttpRequestSetMethod(&tReq, "POST") &&
		xrtHttpRequestSetFormData(&tReq, pForm) ) {
		pResp = xrtHttpExecuteSync(NULL, &tReq, pStatus);
	} else if ( pStatus ) { *pStatus = XRT_NET_ERROR; }
	xrtHttpRequestUnit(&tReq);
	return pResp;
}


static xhttpresponse* __Test_XWebRequestHost(const char* sURL, const char* sMethod, const char* sBody, const char* sHost, xnet_result* pStatus)
{
	xhttprequest tReq;
	xhttpresponse* pResp;
	xrtHttpRequestInit(&tReq);
	(void)xrtHttpRequestSetURL(&tReq, sURL);
	if ( sURL && strncmp(sURL, "https://", 8u) == 0 ) { xrtHttpRequestSetVerifyPeer(&tReq, false); }
	if ( sMethod ) { (void)xrtHttpRequestSetMethod(&tReq, sMethod); }
	(void)xrtHttpRequestSetHeader(&tReq, "Cookie", "sid=abc");
	if ( sHost ) { (void)xrtHttpRequestSetHeader(&tReq, "Host", sHost); }
	if ( sBody ) { (void)xrtHttpRequestSetBodyCopy(&tReq, sBody, strlen(sBody), "application/json"); }
	pResp = xrtHttpExecuteSync(NULL, &tReq, pStatus);
	xrtHttpRequestUnit(&tReq);
	return pResp;
}


// 内部函数：读取原始响应直到对端关闭，用于验证 HEAD 这类不携带正文的响应。
static xhttpresponse* __Test_XWebRequestHeader(const char* sURL, const char* sMethod, const char* sName, const char* sValue, xnet_result* pStatus)
{
	xhttprequest tReq;
	xhttpresponse* pResp;
	xrtHttpRequestInit(&tReq);
	(void)xrtHttpRequestSetURL(&tReq, sURL);
	if ( sURL && strncmp(sURL, "https://", 8u) == 0 ) { xrtHttpRequestSetVerifyPeer(&tReq, false); }
	if ( sMethod ) { (void)xrtHttpRequestSetMethod(&tReq, sMethod); }
	if ( sName && sValue ) { (void)xrtHttpRequestSetHeader(&tReq, sName, sValue); }
	pResp = xrtHttpExecuteSync(NULL, &tReq, pStatus);
	xrtHttpRequestUnit(&tReq);
	return pResp;
}


static bool __Test_XWebRecvRawClosed(xsocket hSocket, char* pBuf, size_t iCap, size_t* pOutLen, uint32 iTimeoutMs)
{
	size_t iLen = 0u;
	uint32 iElapsedMs = 0u;
	if ( pOutLen ) { *pOutLen = 0u; }
	if ( !pBuf || iCap < 2u || hSocket == XNET_SOCKET_INVALID ) { return false; }
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
			if ( iStep <= 0 ) { break; }
			iLen += (size_t)iStep;
			pBuf[iLen] = '\0';
			continue;
		}
		if ( iReady < 0 ) { break; }
		iElapsedMs += 50u;
	}
	if ( pOutLen ) { *pOutLen = iLen; }
	return iLen > 0u;
}


// XWEB 高层服务测试
static bool __Test_XWebWaitSocketClosed(xsocket hSocket, uint32 iTimeoutMs)
{
	uint32 iElapsedMs = 0u;
	if ( hSocket == XNET_SOCKET_INVALID ) { return false; }
	while ( iElapsedMs <= iTimeoutMs ) {
		fd_set tReadSet;
		struct timeval tTv;
		char ch;
		int iReady;
		FD_ZERO(&tReadSet);
		FD_SET(hSocket, &tReadSet);
		tTv.tv_sec = 0;
		tTv.tv_usec = 50000;
		iReady = select((int)(hSocket + 1), &tReadSet, NULL, NULL, &tTv);
		if ( iReady > 0 && FD_ISSET(hSocket, &tReadSet) ) {
			int iStep = recv(hSocket, &ch, 1, 0);
			return iStep == 0;
		}
		if ( iReady < 0 ) { return false; }
		iElapsedMs += 50u;
	}
	return false;
}


static xsocket __Test_XWebConnectSlowReader(uint16 iPort)
{
	xsocket hSocket;
	struct sockaddr_in tAddr;
	int iSockBuf = 4096;

	hSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if ( hSocket == XNET_SOCKET_INVALID ) { return XNET_SOCKET_INVALID; }
#if defined(_WIN32) || defined(_WIN64)
	(void)setsockopt(hSocket, SOL_SOCKET, SO_RCVBUF, (const char*)&iSockBuf, (int)sizeof(iSockBuf));
#else
	(void)setsockopt(hSocket, SOL_SOCKET, SO_RCVBUF, &iSockBuf, (socklen_t)sizeof(iSockBuf));
#endif
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


static bool __Test_XWebDrainUntilClosed(xsocket hSocket, uint32 iDelayMs, uint32 iTimeoutMs)
{
	char aBuf[4096];
	uint32 iElapsedMs = 0u;
	if ( hSocket == XNET_SOCKET_INVALID ) { return false; }
	if ( iDelayMs > 0u ) { xrtSleep(iDelayMs); }
	while ( iElapsedMs <= iTimeoutMs ) {
		fd_set tReadSet;
		struct timeval tTv;
		int iReady;
		FD_ZERO(&tReadSet);
		FD_SET(hSocket, &tReadSet);
		tTv.tv_sec = 0;
		tTv.tv_usec = 50000;
		iReady = select((int)(hSocket + 1), &tReadSet, NULL, NULL, &tTv);
		if ( iReady > 0 && FD_ISSET(hSocket, &tReadSet) ) {
			int iStep = recv(hSocket, aBuf, (int)sizeof(aBuf), 0);
			if ( iStep == 0 ) { return true; }
			if ( iStep < 0 ) { return !__xnetSocketWouldBlock(__xnetSocketLastErr()); }
			continue;
		}
		if ( iReady < 0 ) { return false; }
		iElapsedMs += 50u;
	}
	return false;
}


// 记录 XWeb 子断言失败，同时保留原有测试输出。
static int __Test_XWebTrackedPrintf(int* pFailCount, const char* sFormat, ...)
{
	char aBuf[4096];
	va_list tArgs;
	int iRet;

	va_start(tArgs, sFormat);
	iRet = vsnprintf(aBuf, sizeof(aBuf), sFormat, tArgs);
	va_end(tArgs);
	if ( pFailCount && strstr(aBuf, "FAIL") != NULL ) {
		(*pFailCount)++;
	}
	fputs(aBuf, stdout);
	return iRet;
}


int Test_XWeb(void)
{
	int iFailCount = 0;
#define printf(...) __Test_XWebTrackedPrintf(&iFailCount, __VA_ARGS__)
	printf("\n\n\n------------------------------------\n\n XWeb Server Test:\n\n");

	{
		xwebconfig tConfig;
		xwebstaticconfig tStaticConfig;
		xwebserver* pInvalidServer;
		xrtWebConfigInit(&tConfig);
		xrtWebStaticConfigInit(&tStaticConfig);
		printf("  XWeb versioned config contract : %s\n",
			tConfig.iSize == XWEB_CONFIG_V1_SIZE &&
			tConfig.iVersion == XWEB_CONFIG_VERSION ? "PASS" : "FAIL");
		printf("  XWeb versioned static config contract : %s\n",
			tStaticConfig.iSize == XWEB_STATIC_CONFIG_V1_SIZE &&
			tStaticConfig.iVersion == XWEB_STATIC_CONFIG_VERSION ? "PASS" : "FAIL");
		tConfig.iSize = XWEB_CONFIG_V1_SIZE - 1u;
		pInvalidServer = xrtWebServerCreate(NULL, &tConfig);
		printf("  XWeb rejects truncated config : %s\n",
			pInvalidServer == NULL ? "PASS" : "FAIL");
		if ( pInvalidServer ) { xrtWebServerDestroy(pInvalidServer); }
	}

	{
		xwebapp* pApp = xrtWebAppCreate();
		xwebrequest tReq;
		char sPattern[2048];
		char sPath[2048];
		size_t iPatternLen = 0u;
		size_t iPathLen = 0u;
		__xweb_route* pRoute;
		const char* sValue = NULL;
		size_t iValueLen = 0u;
		bool bBuilt = pApp != NULL;
		memset(&tReq, 0, sizeof(tReq));
		sPattern[0] = '\0';
		sPath[0] = '\0';
		for ( uint32 i = 0u; i < 40u && bBuilt; ++i ) {
			int iPatternPart = snprintf(sPattern + iPatternLen, sizeof(sPattern) - iPatternLen, "/{p%u}", (unsigned)i);
			int iPathPart = snprintf(sPath + iPathLen, sizeof(sPath) - iPathLen, "/v%u", (unsigned)i);
			bBuilt = iPatternPart > 0 && iPathPart > 0;
			iPatternLen += (size_t)iPatternPart;
			iPathLen += (size_t)iPathPart;
		}
		bBuilt = bBuilt && xrtWebAppGet(pApp, sPattern, __Test_XWebMeHandler, NULL) &&
			__xwebRequestParamsInit(&tReq, sPath);
		pRoute = bBuilt ? __xwebRouteMatch(pApp, XHTTPD_METHOD_GET, sPath, tReq.pParams, tReq.iParamCap, &tReq.iParamCount) : NULL;
		printf("  XWeb route supports more than 32 parameters : %s\n",
			pRoute && tReq.iParamCount == 40u && xrtWebRequestParamValueViewAt(&tReq, 39u, &sValue, &iValueLen) &&
			iValueLen == 3u && memcmp(sValue, "v39", 3u) == 0 ? "PASS" : "FAIL");
		printf("  XWeb method mask CONNECT and TRACE : %s\n",
			xrtWebMethodMask("CONNECT|TRACE") == (XWEB_METHOD_CONNECT | XWEB_METHOD_TRACE) ? "PASS" : "FAIL");
		__xwebRequestParamsUnit(&tReq);
		xrtWebAppRelease(pApp);
	}

	{
		xwebapp* pApp = xrtWebAppCreate();
		xwebrequest tReq;
		__xweb_route* pRoute = NULL;
		bool bReady;
		memset(&tReq, 0, sizeof(tReq));
		bReady = pApp != NULL && xrtWebAppGet(pApp, "/*", __Test_XWebMeHandler, NULL) &&
			__xwebRequestParamsInit(&tReq, "/");
		if ( bReady ) {
			pRoute = __xwebRouteMatch(pApp, XHTTPD_METHOD_GET, "/",
				tReq.pParams, tReq.iParamCap, &tReq.iParamCount);
		}
		printf("  XWeb tail wildcard matches root : %s\n", pRoute != NULL ? "PASS" : "FAIL");
		__xwebRequestParamsUnit(&tReq);
		xrtWebAppRelease(pApp);
	}

	{
		__test_xweb_middleware_ctx tMiddlewareCtx;
		xwebapp* pApp = NULL;
		xwebserver* pServer = NULL;
		xhttpresponse* pResp = NULL;
		xnet_result iStatus = XRT_NET_ERROR;
		char sURL[512];
		long iRouteBefore;
		bool bRegistered;

		memset(&tMiddlewareCtx, 0, sizeof(tMiddlewareCtx));
		pApp = xrtWebAppCreate();
		pServer = xrtWebServerCreateHostEx(NULL, "127.0.0.1", 0u,
			128u, 1024u * 1024u, 1024u * 1024u, 1u);
		bRegistered = pApp && pServer &&
			xrtWebAppUseEx(pApp, __Test_XWebMiddlewareOuter,
				&tMiddlewareCtx, __Test_XWebMiddlewareFree) &&
			xrtWebAppUse(pApp, __Test_XWebMiddlewareAuto, &tMiddlewareCtx) &&
			xrtWebAppGet(pApp, "/mw/{kind}", __Test_XWebMiddlewareRoute,
				&tMiddlewareCtx) &&
			xrtWebServerSetApp(pServer, pApp) &&
			xrtWebServerUse(pServer, __Test_XWebMiddlewareInner, &tMiddlewareCtx);
		printf("  XWeb middleware registration : %s\n", bRegistered ? "PASS" : "FAIL");
		printf("  XWeb middleware invalid registration : %s\n",
			pApp && !xrtWebAppUse(pApp, NULL, NULL) ? "PASS" : "FAIL");
		printf("  XWeb middleware server start : %s\n",
			pServer && xrtWebServerStart(pServer) == XRT_NET_OK ? "PASS" : "FAIL");
		if ( pApp ) {
			xrtWebAppRelease(pApp);
			pApp = NULL;
		}

		__Test_XWebMiddlewareReset(&tMiddlewareCtx);
		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/mw/ok",
			(unsigned)xrtWebServerPort(pServer));
		pResp = pServer ? __Test_XWebRequest(sURL, "GET", NULL, &iStatus) : NULL;
		printf("  XWeb middleware nested order : %s\n",
			pResp && strcmp(tMiddlewareCtx.aOrder, "ABCRDE") == 0 ? "PASS" : "FAIL");
		printf("  XWeb middleware response postprocess : %s\n",
			iStatus == XRT_NET_OK && pResp && pResp->iStatusCode == 202u &&
			pResp->pBody && strcmp(pResp->pBody, "middleware") == 0 &&
			xrtHttpResponseHeader(pResp, "X-Middleware-Pre") &&
			xrtHttpResponseHeader(pResp, "X-Middleware-Auto") &&
			xrtHttpResponseHeader(pResp, "X-Middleware-Inner") &&
			xrtHttpResponseHeader(pResp, "X-Middleware-Post") ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		pResp = NULL;

		__Test_XWebMiddlewareReset(&tMiddlewareCtx);
		iRouteBefore = __Test_XHttpdAtomicLoad(&tMiddlewareCtx.iRouteCount);
		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/mw/short",
			(unsigned)xrtWebServerPort(pServer));
		pResp = pServer ? __Test_XWebRequest(sURL, "GET", NULL, &iStatus) : NULL;
		printf("  XWeb middleware short circuit : %s\n",
			pResp && pResp->iStatusCode == 403u && pResp->pBody &&
			strcmp(pResp->pBody, "short") == 0 &&
			strcmp(tMiddlewareCtx.aOrder, "A") == 0 &&
			__Test_XHttpdAtomicLoad(&tMiddlewareCtx.iRouteCount) == iRouteBefore
				? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		pResp = NULL;

		__Test_XWebMiddlewareReset(&tMiddlewareCtx);
		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/mw/error",
			(unsigned)xrtWebServerPort(pServer));
		pResp = pServer ? __Test_XWebRequest(sURL, "GET", NULL, &iStatus) : NULL;
		printf("  XWeb middleware error reset : %s\n",
			pResp && pResp->iStatusCode == 500u && pResp->pBody &&
			strcmp(pResp->pBody, "Internal Server Error") == 0 &&
			!xrtHttpResponseHeader(pResp, "X-Stale") &&
			strcmp(tMiddlewareCtx.aOrder, "A") == 0 ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		pResp = NULL;

		__Test_XWebMiddlewareReset(&tMiddlewareCtx);
		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/mw/twice",
			(unsigned)xrtWebServerPort(pServer));
		pResp = pServer ? __Test_XWebRequest(sURL, "GET", NULL, &iStatus) : NULL;
		printf("  XWeb middleware next once contract : %s\n",
			pResp && pResp->iStatusCode == 500u &&
			strcmp(tMiddlewareCtx.aOrder, "ABCRDE") == 0 ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		pResp = NULL;

		__Test_XWebMiddlewareReset(&tMiddlewareCtx);
		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/mw/async",
			(unsigned)xrtWebServerPort(pServer));
		pResp = pServer ? __Test_XWebRequest(sURL, "GET", NULL, &iStatus) : NULL;
		printf("  XWeb middleware rejects unsafe async action : %s\n",
			pResp && pResp->iStatusCode == 500u &&
			strcmp(tMiddlewareCtx.aOrder, "ABCRDE") == 0 ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		pResp = NULL;

		__Test_XWebMiddlewareReset(&tMiddlewareCtx);
		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/missing",
			(unsigned)xrtWebServerPort(pServer));
		pResp = pServer ? __Test_XWebRequest(sURL, "GET", NULL, &iStatus) : NULL;
		printf("  XWeb middleware wraps not found : %s\n",
			pResp && pResp->iStatusCode == 404u &&
			strcmp(tMiddlewareCtx.aOrder, "ABCDE") == 0 &&
			xrtHttpResponseHeader(pResp, "X-Middleware-Inner") &&
			xrtHttpResponseHeader(pResp, "X-Middleware-Post") ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		pResp = NULL;

		__Test_XWebMiddlewareReset(&tMiddlewareCtx);
		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/mw/committed",
			(unsigned)xrtWebServerPort(pServer));
		pResp = pServer ? __Test_XWebRequest(sURL, "GET", NULL, &iStatus) : NULL;
		printf("  XWeb middleware committed response : %s\n",
			pResp && pResp->iStatusCode == 200u && pResp->pBody &&
			strcmp(pResp->pBody, "committed") == 0 &&
			strcmp(tMiddlewareCtx.aOrder, "ABCRDE") == 0 ? "PASS" : "FAIL");
		printf("  XWeb response immutable after commit : %s\n",
			tMiddlewareCtx.bCommittedImmutable ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }

		xrtHttpCloseIdleConnections(xrtNetSyncGetHiddenEngine());
		xrtNetSyncShutdownHiddenEngine();
		if ( pServer ) { xrtWebServerDestroy(pServer); }
		printf("  XWeb middleware user data released once : %s\n",
			__Test_XHttpdAtomicLoad(&tMiddlewareCtx.iFreeCount) == 1 ? "PASS" : "FAIL");
	}

	{
		__test_xweb_ctx tCtx;
		xnetengineconfig tEngineCfg;
		xwebconfig tWebCfg;
		xwebstaticconfig tStaticCfg;
		xnetengine* pEngine = NULL;
		xwebserver* pServer = NULL;
		xhttpresponse* pResp = NULL;
		xnet_result iStatus = XRT_NET_ERROR;
		char sURL[512];
		char* sStaticRoot = NULL;
		char* sStaticFile = NULL;
		char* sStaticHome = NULL;
		char sStaticETag[128];
		char sStaticIndexNames[32] = "home.txt";
		char sStaticCacheControl[32] = "max-age=30";

		sStaticETag[0] = '\0';
		memset(&tCtx, 0, sizeof(tCtx));
		xrtNetEngineConfigInit(&tEngineCfg);
		tEngineCfg.iWorkerCount = 1u;
		pEngine = xrtNetEngineCreate(&tEngineCfg);
		printf("  XWeb engine create : %s\n", pEngine != NULL ? "PASS" : "FAIL");
		printf("  XWeb engine start : %s\n", pEngine && xrtNetEngineStart(pEngine) == XRT_NET_OK ? "PASS" : "FAIL");

		xrtWebConfigInit(&tWebCfg);
		(void)xrtNetAddrParse(&tWebCfg.tBindAddr, "127.0.0.1", 0u);
		tWebCfg.iCompressionFlags = XWEB_COMPRESSION_F_GZIP | XWEB_COMPRESSION_F_DEFLATE;
		tWebCfg.iCompressionMinBytes = 32u;
		pServer = pEngine ? xrtWebServerCreate(pEngine, &tWebCfg) : NULL;
		printf("  XWeb server create : %s\n", pServer != NULL ? "PASS" : "FAIL");
		printf("  XWeb method mask get : %s\n", xrtWebMethodMask("get") == XWEB_METHOD_GET ? "PASS" : "FAIL");
		printf("  XWeb method mask combo : %s\n", xrtWebMethodMask("GET|POST, PATCH") == (XWEB_METHOD_GET | XWEB_METHOD_POST | XWEB_METHOD_PATCH) ? "PASS" : "FAIL");
		printf("  XWeb method mask any : %s\n", xrtWebMethodMask("*") == XWEB_METHOD_ANY ? "PASS" : "FAIL");
		printf("  XWeb method mask invalid : %s\n", xrtWebMethodMask("GET|BOGUS") == 0u ? "PASS" : "FAIL");

		printf("  XWeb route var register : %s\n", pServer && xrtWebServerGet(pServer, "/users/{id}", __Test_XWebUserHandler, &tCtx) ? "PASS" : "FAIL");
		printf("  XWeb route static register : %s\n", pServer && xrtWebServerGet(pServer, "/users/me", __Test_XWebMeHandler, &tCtx) ? "PASS" : "FAIL");
		printf("  XWeb route post register : %s\n", pServer && xrtWebServerPost(pServer, "/echo", __Test_XWebPostHandler, &tCtx) ? "PASS" : "FAIL");
		printf("  XWeb route form register : %s\n", pServer && xrtWebServerPost(pServer, "/form", __Test_XWebPostHandler, &tCtx) ? "PASS" : "FAIL");
		printf("  XWeb route multipart register : %s\n", pServer && xrtWebServerPost(pServer, "/multipart", __Test_XWebPostHandler, &tCtx) ? "PASS" : "FAIL");
		printf("  XWeb route any rollback : %s\n", pServer && !xrtWebServerAny(pServer, "/echo", __Test_XWebMeHandler, &tCtx) && xrtWebServerGet(pServer, "/echo", __Test_XWebMeHandler, &tCtx) ? "PASS" : "FAIL");
		printf("  XWeb route redirect register : %s\n", pServer && xrtWebServerGet(pServer, "/go", __Test_XWebRedirectHandler, NULL) ? "PASS" : "FAIL");
		printf("  XWeb route cookie register : %s\n", pServer && xrtWebServerGet(pServer, "/cookie", __Test_XWebCookieHandler, NULL) ? "PASS" : "FAIL");
		printf("  XWeb route delete cookie register : %s\n", pServer && xrtWebServerGet(pServer, "/delete-cookie", __Test_XWebDeleteCookieHandler, NULL) ? "PASS" : "FAIL");
		printf("  XWeb route stream register : %s\n", pServer && xrtWebServerGet(pServer, "/stream", __Test_XWebStreamHandler, &tCtx) ? "PASS" : "FAIL");
		printf("  XWeb route append body register : %s\n", pServer && xrtWebServerGet(pServer, "/append-body", __Test_XWebAppendHandler, NULL) ? "PASS" : "FAIL");
		printf("  XWeb route trailer register : %s\n", pServer && xrtWebServerGet(pServer, "/trailer", __Test_XWebTrailerHandler, NULL) ? "PASS" : "FAIL");
		printf("  XWeb route upgrade register : %s\n", pServer && xrtWebServerGet(pServer, "/upgrade", __Test_XWebUpgradeHandler, &tCtx) ? "PASS" : "FAIL");
		printf("  XWeb route compression register : %s\n", pServer && xrtWebServerGet(pServer, "/compress", __Test_XWebCompressionHandler, NULL) ? "PASS" : "FAIL");
		printf("  XWeb route no-transform register : %s\n", pServer && xrtWebServerGet(pServer, "/compress-no-transform", __Test_XWebCompressionHandler, NULL) ? "PASS" : "FAIL");
		printf("  XWeb body stream route register : %s\n", pServer && xrtWebServerStreamRoute(pServer, XWEB_METHOD_POST, "/stream-body/{name}", __Test_XWebBodyStreamBegin, __Test_XWebBodyStreamChunk, __Test_XWebBodyStreamEnd, &tCtx) ? "PASS" : "FAIL");
		printf("  XWeb async body stream route register : %s\n", pServer && xrtWebServerStreamRouteAsync(pServer, XWEB_METHOD_POST, "/stream-body-async/{name}", __Test_XWebBodyStreamBegin, __Test_XWebBodyStreamChunk, __Test_XWebBodyStreamEndAsync, &tCtx) ? "PASS" : "FAIL");
		printf("  XWeb route head register : %s\n", pServer && xrtWebServerRoute(pServer, XWEB_METHOD_HEAD, "/head", __Test_XWebMeHandler, &tCtx) ? "PASS" : "FAIL");
		printf("  XWeb error handler register : %s\n", pServer && xrtWebServerError(pServer, __Test_XWebErrorHandler, &tCtx, NULL) ? "PASS" : "FAIL");

		sStaticRoot = (char*)xrtPathJoin(2u, (str)"release/x64", (str)"xweb_static");
		sStaticFile = (char*)xrtPathJoin(2u, (str)sStaticRoot, (str)"index.txt");
		sStaticHome = (char*)xrtPathJoin(2u, (str)sStaticRoot, (str)"home.txt");
		(void)xrtDirCreateAll((str)sStaticRoot);
		(void)xrtFileWriteAll((str)sStaticFile, (str)"static-ok", 9u, XRT_CP_UTF8);
		(void)xrtFileWriteAll((str)sStaticHome, (str)"static-ex-ok", 12u, XRT_CP_UTF8);
		printf("  XWeb file range route register : %s\n", pServer && xrtWebServerGet(pServer, "/file-range", __Test_XWebFileRangeHandler, sStaticFile) ? "PASS" : "FAIL");
		printf("  XWeb close route register : %s\n", pServer && xrtWebServerGet(pServer, "/close", __Test_XWebCloseHandler, NULL) ? "PASS" : "FAIL");
		xrtWebStaticConfigInit(&tStaticCfg);
		tStaticCfg.sCacheControl = "max-age=60";
		printf("  XWeb static register : %s\n", pServer && xrtWebServerStatic(pServer, "/public", sStaticRoot, &tStaticCfg) ? "PASS" : "FAIL");
		printf("  XWeb static ex register : %s\n", pServer && xrtWebServerStaticEx(pServer, "/assets", sStaticRoot, sStaticIndexNames, sStaticCacheControl, 4096u, 0u) ? "PASS" : "FAIL");
		strcpy(sStaticIndexNames, "missing.txt");
		strcpy(sStaticCacheControl, "max-age=0");

		printf("  XWeb server start : %s\n", pServer && xrtWebServerStart(pServer) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  XWeb bound port : %s\n", pServer && xrtWebServerPort(pServer) > 0u ? "PASS" : "FAIL");

		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/users/42?name=xlang", (unsigned)xrtWebServerPort(pServer));
		pResp = pServer ? __Test_XWebRequest(sURL, "GET", NULL, &iStatus) : NULL;
		printf("  XWeb route response status : %s\n", iStatus == XRT_NET_OK && pResp && pResp->iStatusCode == 201u ? "PASS" : "FAIL");
		printf("  XWeb route response body : %s\n", pResp && pResp->pBody && strstr(pResp->pBody, "id=42;name=xlang;sid=abc") != NULL ? "PASS" : "FAIL");
		printf("  XWeb route response header : %s\n", pResp && xrtHttpResponseHeader(pResp, "X-XWeb") != NULL ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		pResp = NULL;

		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/file-range", (unsigned)xrtWebServerPort(pServer));
		pResp = pServer ? __Test_XWebRequest(sURL, "GET", NULL, &iStatus) : NULL;
		printf("  XWeb file range status : %s\n", iStatus == XRT_NET_OK && pResp && pResp->iStatusCode == 206u ? "PASS" : "FAIL");
		printf("  XWeb file range body : %s\n", pResp && pResp->iBodyLen == 4u && memcmp(pResp->pBody, "atic", 4u) == 0 ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		pResp = NULL;

		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/users/me", (unsigned)xrtWebServerPort(pServer));
		pResp = pServer ? __Test_XWebRequest(sURL, "GET", NULL, &iStatus) : NULL;
		printf("  XWeb static priority status : %s\n", iStatus == XRT_NET_OK && pResp && pResp->iStatusCode == 200u ? "PASS" : "FAIL");
		printf("  XWeb static priority body : %s\n", pResp && pResp->pBody && strcmp(pResp->pBody, "me") == 0 ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		pResp = NULL;

		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/append-body", (unsigned)xrtWebServerPort(pServer));
		pResp = pServer ? __Test_XWebRequest(sURL, "GET", NULL, &iStatus) : NULL;
		printf("  XWeb append body status : %s\n", iStatus == XRT_NET_OK && pResp && pResp->iStatusCode == 200u ? "PASS" : "FAIL");
		printf("  XWeb append body content : %s\n", pResp && pResp->pBody && strcmp(pResp->pBody, "append-body") == 0 ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		pResp = NULL;

		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/trailer", (unsigned)xrtWebServerPort(pServer));
		pResp = pServer ? __Test_XWebRequest(sURL, "GET", NULL, &iStatus) : NULL;
		printf("  XWeb response trailer status : %s\n", iStatus == XRT_NET_OK && pResp && pResp->iStatusCode == 200u ? "PASS" : "FAIL");
		printf("  XWeb response trailer body : %s\n", pResp && pResp->pBody && strcmp(pResp->pBody, "xweb-trailer") == 0 ? "PASS" : "FAIL");
		printf("  XWeb response trailer fields : %s\n",
			pResp && xrtHttpResponseTrailer(pResp, "X-Web-End") &&
			xrtHttpResponseTrailer(pResp, "X-Web-Meta") &&
			strcmp(xrtHttpResponseTrailer(pResp, "X-Web-End"), "one") == 0 &&
			strcmp(xrtHttpResponseTrailer(pResp, "X-Web-Meta"), "two") == 0 ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		pResp = NULL;

		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/compress", (unsigned)xrtWebServerPort(pServer));
		pResp = pServer ? __Test_XWebRequest(sURL, "GET", NULL, &iStatus) : NULL;
		printf("  XWeb gzip negotiation status : %s\n", iStatus == XRT_NET_OK && pResp && pResp->iStatusCode == 200u ? "PASS" : "FAIL");
		printf("  XWeb gzip decoded body : %s\n",
			pResp && pResp->iBodyLen == 2048u && pResp->pBody &&
			pResp->pBody[0] == 'C' && pResp->pBody[2047] == 'C' ? "PASS" : "FAIL");
		printf("  XWeb gzip response metadata : %s\n",
			pResp && xrtHttpResponseOriginalContentEncoding(pResp) &&
			strcmp(xrtHttpResponseOriginalContentEncoding(pResp), "gzip") == 0 &&
			xrtHttpResponseHeader(pResp, "Vary") &&
			xrtHttpHeaderContainsToken(xrtHttpResponseHeader(pResp, "Vary"), "Accept-Encoding") &&
			!xrtHttpResponseHeader(pResp, "ETag") && !xrtHttpResponseHeader(pResp, "Digest") ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		pResp = pServer ? __Test_XWebRequestHeader(sURL, "GET", "Accept-Encoding",
			"gzip;q=0.2, deflate;q=1", &iStatus) : NULL;
		printf("  XWeb deflate q-value preference : %s\n",
			pResp && pResp->iBodyLen == 2048u && xrtHttpResponseOriginalContentEncoding(pResp) &&
			strcmp(xrtHttpResponseOriginalContentEncoding(pResp), "deflate") == 0 ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		pResp = pServer ? __Test_XWebRequestHeader(sURL, "GET", "Accept-Encoding",
			"*;q=0.5, identity;q=0.1", &iStatus) : NULL;
		printf("  XWeb wildcard encoding negotiation : %s\n",
			pResp && xrtHttpResponseOriginalContentEncoding(pResp) &&
			strcmp(xrtHttpResponseOriginalContentEncoding(pResp), "gzip") == 0 ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		pResp = pServer ? __Test_XWebRequestHeader(sURL, "GET", "Accept-Encoding",
			"gzip;q=0.2, identity;q=1", &iStatus) : NULL;
		printf("  XWeb identity preference skips compression : %s\n",
			pResp && pResp->iBodyLen == 2048u && !xrtHttpResponseOriginalContentEncoding(pResp) &&
			xrtHttpResponseHeader(pResp, "Vary") ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		pResp = pServer ? __Test_XWebRequestHeader(sURL, "GET", "Accept-Encoding",
			"gzip;q=0, deflate;q=0, identity;q=0", &iStatus) : NULL;
		printf("  XWeb unacceptable encoding status : %s\n",
			iStatus == XRT_NET_OK && pResp && pResp->iStatusCode == 406u ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/compress-no-transform", (unsigned)xrtWebServerPort(pServer));
		pResp = pServer ? __Test_XWebRequest(sURL, "GET", NULL, &iStatus) : NULL;
		printf("  XWeb no-transform skips compression : %s\n",
			pResp && pResp->iBodyLen == 2048u && !xrtHttpResponseOriginalContentEncoding(pResp) &&
			xrtHttpResponseHeader(pResp, "ETag") && xrtHttpResponseHeader(pResp, "Digest") ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		pResp = NULL;

		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/echo", (unsigned)xrtWebServerPort(pServer));
		pResp = pServer ? __Test_XWebRequest(sURL, "POST", "{\"ok\":true}", &iStatus) : NULL;
		printf("  XWeb post status : %s\n", iStatus == XRT_NET_OK && pResp && pResp->iStatusCode == 200u ? "PASS" : "FAIL");
		printf("  XWeb post body : %s\n", pResp && pResp->pBody && strcmp(pResp->pBody, "{\"ok\":true}") == 0 ? "PASS" : "FAIL");
		printf("  XWeb post content-type : %s\n", pResp && xrtHttpResponseHeader(pResp, "Content-Type") != NULL && strstr(xrtHttpResponseHeader(pResp, "Content-Type"), "application/json") != NULL ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		pResp = NULL;

		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/form", (unsigned)xrtWebServerPort(pServer));
		pResp = pServer ? __Test_XWebRequestContentType(sURL, "POST", "name=alice+bob&city=shanghai", "application/x-www-form-urlencoded; charset=UTF-8", &iStatus) : NULL;
		printf("  XWeb form status : %s\n", iStatus == XRT_NET_OK && pResp && pResp->iStatusCode == 200u ? "PASS" : "FAIL");
		printf("  XWeb form body : %s\n", pResp && pResp->pBody && strcmp(pResp->pBody, "form-ok") == 0 ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		pResp = NULL;

		{
			const char* sMultipartBody =
				"--xlang\r\n"
				"Content-Disposition: form-data; name=\"title\"\r\n"
				"\r\n"
				"hello multipart\r\n"
				"--xlang\r\n"
				"Content-Disposition: form-data; name=\"upload\"; filename=\"note.txt\"\r\n"
				"Content-Type: text/plain\r\n"
				"\r\n"
				"file-data\r\n"
				"--xlang--\r\n";
			snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/multipart", (unsigned)xrtWebServerPort(pServer));
			pResp = pServer ? __Test_XWebRequestContentType(sURL, "POST", sMultipartBody, "multipart/form-data; boundary=xlang", &iStatus) : NULL;
			printf("  XWeb multipart status : %s\n", iStatus == XRT_NET_OK && pResp && pResp->iStatusCode == 200u ? "PASS" : "FAIL");
			printf("  XWeb multipart body : %s\n", pResp && pResp->pBody && strcmp(pResp->pBody, "multipart-ok") == 0 ? "PASS" : "FAIL");
			if ( pResp ) { xrtHttpResponseDestroy(pResp); }
			pResp = NULL;
		}

		{
			xhttpformdata* pForm = xrtHttpFormDataCreate();
			xhttpbody* pFileBody = xrtHttpBodyCreateBytesCopy("file-data", 9u);
			bool bBuilt = pForm && pFileBody &&
				xrtHttpFormDataAppendText(pForm, "title", "hello multipart") == XHTTP_SEMANTIC_OK &&
				xrtHttpFormDataAppendBody(pForm, "upload", pFileBody,
					"note.txt", "text/plain") == XHTTP_SEMANTIC_OK;
			xrtHttpBodyRelease(pFileBody);
			snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/multipart",
				(unsigned)xrtWebServerPort(pServer));
			pResp = (pServer && bBuilt) ? __Test_XWebRequestFormData(sURL, pForm, &iStatus) : NULL;
			printf("  XWeb FormData streaming status : %s\n",
				iStatus == XRT_NET_OK && pResp && pResp->iStatusCode == 200u ? "PASS" : "FAIL");
			printf("  XWeb FormData streaming body : %s\n",
				pResp && pResp->pBody && strcmp(pResp->pBody, "multipart-ok") == 0 ? "PASS" : "FAIL");
			if ( pResp ) { xrtHttpResponseDestroy(pResp); }
			pResp = NULL;
			xrtHttpFormDataDestroy(pForm);
		}

		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/public/index.txt", (unsigned)xrtWebServerPort(pServer));
		pResp = pServer ? __Test_XWebRequest(sURL, "GET", NULL, &iStatus) : NULL;
		printf("  XWeb static status : %s\n", iStatus == XRT_NET_OK && pResp && pResp->iStatusCode == 200u ? "PASS" : "FAIL");
		printf("  XWeb static body : %s\n", pResp && pResp->pBody && strcmp(pResp->pBody, "static-ok") == 0 ? "PASS" : "FAIL");
		printf("  XWeb static cache header : %s\n", pResp && xrtHttpResponseHeader(pResp, "Cache-Control") != NULL ? "PASS" : "FAIL");
		printf("  XWeb static etag header : %s\n", pResp && xrtHttpResponseHeader(pResp, "ETag") != NULL ? "PASS" : "FAIL");
		printf("  XWeb static last-modified header : %s\n", pResp && xrtHttpResponseHeader(pResp, "Last-Modified") != NULL ? "PASS" : "FAIL");
		printf("  XWeb static accept-ranges header : %s\n", pResp && xrtHttpResponseHeader(pResp, "Accept-Ranges") != NULL && strcmp(xrtHttpResponseHeader(pResp, "Accept-Ranges"), "bytes") == 0 ? "PASS" : "FAIL");
		if ( pResp && xrtHttpResponseHeader(pResp, "ETag") ) {
			snprintf(sStaticETag, sizeof(sStaticETag), "%s", xrtHttpResponseHeader(pResp, "ETag"));
		}
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		pResp = NULL;

		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/public/index.txt", (unsigned)xrtWebServerPort(pServer));
		pResp = pServer ? __Test_XWebRequestHeader(sURL, "GET", "Range", "bytes=0-5", &iStatus) : NULL;
		printf("  XWeb static range status : %s\n", iStatus == XRT_NET_OK && pResp && pResp->iStatusCode == 206u ? "PASS" : "FAIL");
		printf("  XWeb static range body : %s\n", pResp && pResp->pBody && pResp->iBodyLen == 6u && memcmp(pResp->pBody, "static", 6u) == 0 ? "PASS" : "FAIL");
		printf("  XWeb static range header : %s\n", pResp && xrtHttpResponseHeader(pResp, "Content-Range") != NULL && strcmp(xrtHttpResponseHeader(pResp, "Content-Range"), "bytes 0-5/9") == 0 ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		pResp = NULL;

		pResp = pServer ? __Test_XWebRequestHeader(sURL, "GET", "Range", "bytes=-2", &iStatus) : NULL;
		printf("  XWeb static suffix range status : %s\n", iStatus == XRT_NET_OK && pResp && pResp->iStatusCode == 206u ? "PASS" : "FAIL");
		printf("  XWeb static suffix range body : %s\n", pResp && pResp->pBody && pResp->iBodyLen == 2u && memcmp(pResp->pBody, "ok", 2u) == 0 ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		pResp = NULL;

		pResp = (pServer && sStaticETag[0]) ? __Test_XWebRequestHeader(sURL, "GET", "If-None-Match", sStaticETag, &iStatus) : NULL;
		printf("  XWeb static if-none-match status : %s\n", iStatus == XRT_NET_OK && pResp && pResp->iStatusCode == 304u ? "PASS" : "FAIL");
		printf("  XWeb static if-none-match empty body : %s\n", pResp && pResp->iBodyLen == 0u ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		pResp = NULL;

		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/assets/", (unsigned)xrtWebServerPort(pServer));
		pResp = pServer ? __Test_XWebRequest(sURL, "GET", NULL, &iStatus) : NULL;
		printf("  XWeb static ex status : %s\n", iStatus == XRT_NET_OK && pResp && pResp->iStatusCode == 200u ? "PASS" : "FAIL");
		printf("  XWeb static ex body : %s\n", pResp && pResp->pBody && strcmp(pResp->pBody, "static-ex-ok") == 0 ? "PASS" : "FAIL");
		printf("  XWeb static ex cache header : %s\n", pResp && xrtHttpResponseHeader(pResp, "Cache-Control") != NULL && strcmp(xrtHttpResponseHeader(pResp, "Cache-Control"), "max-age=30") == 0 ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		pResp = NULL;

		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/go", (unsigned)xrtWebServerPort(pServer));
		pResp = pServer ? __Test_XWebRequestNoRedirect(sURL, "GET", &iStatus) : NULL;
		printf("  XWeb redirect status : %s\n", iStatus == XRT_NET_OK && pResp && pResp->iStatusCode == 302u ? "PASS" : "FAIL");
		printf("  XWeb redirect location : %s\n", pResp && xrtHttpResponseHeader(pResp, "Location") != NULL && strcmp(xrtHttpResponseHeader(pResp, "Location"), "/users/me") == 0 ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		pResp = NULL;

		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/cookie", (unsigned)xrtWebServerPort(pServer));
		pResp = pServer ? __Test_XWebRequest(sURL, "GET", NULL, &iStatus) : NULL;
		printf("  XWeb cookie status : %s\n", iStatus == XRT_NET_OK && pResp && pResp->iStatusCode == 200u ? "PASS" : "FAIL");
		printf("  XWeb cookie header : %s\n", pResp && xrtHttpResponseHeader(pResp, "Set-Cookie") != NULL && strstr(xrtHttpResponseHeader(pResp, "Set-Cookie"), "sid=xyz") != NULL && strstr(xrtHttpResponseHeader(pResp, "Set-Cookie"), "Max-Age=3600") != NULL && strstr(xrtHttpResponseHeader(pResp, "Set-Cookie"), "SameSite=Lax") != NULL && strstr(xrtHttpResponseHeader(pResp, "Set-Cookie"), "Secure") != NULL && strstr(xrtHttpResponseHeader(pResp, "Set-Cookie"), "HttpOnly") != NULL ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		pResp = NULL;

		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/delete-cookie", (unsigned)xrtWebServerPort(pServer));
		pResp = pServer ? __Test_XWebRequest(sURL, "GET", NULL, &iStatus) : NULL;
		printf("  XWeb delete cookie status : %s\n", iStatus == XRT_NET_OK && pResp && pResp->iStatusCode == 200u ? "PASS" : "FAIL");
		printf("  XWeb delete cookie header : %s\n", pResp && xrtHttpResponseHeader(pResp, "Set-Cookie") != NULL && strstr(xrtHttpResponseHeader(pResp, "Set-Cookie"), "sid=") != NULL && strstr(xrtHttpResponseHeader(pResp, "Set-Cookie"), "Max-Age=0") != NULL ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		pResp = NULL;

		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/stream", (unsigned)xrtWebServerPort(pServer));
		pResp = pServer ? __Test_XWebRequest(sURL, "GET", NULL, &iStatus) : NULL;
		printf("  XWeb stream status : %s\n", iStatus == XRT_NET_OK && pResp && pResp->iStatusCode == 200u ? "PASS" : "FAIL");
		printf("  XWeb stream body : %s\n", pResp && pResp->pBody && strcmp(pResp->pBody, "stream-ok") == 0 ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }

		{
			xsocket hUpgrade = XNET_SOCKET_INVALID;
			char sUpgradeReq[512];
			char aUpgradeResp[2048];
			size_t iUpgradeRespLen = 0u;
			long iUpgradeBefore = __Test_XHttpdAtomicLoad(&tCtx.iUpgradeCount);
			long iRecvBefore = __Test_XHttpdAtomicLoad(&tCtx.iUpgradeRecvCount);
			long iCloseBefore = __Test_XHttpdAtomicLoad(&tCtx.iUpgradeCloseCount);
			snprintf(sUpgradeReq, sizeof(sUpgradeReq),
				"GET /upgrade HTTP/1.1\r\n"
				"Host: 127.0.0.1:%u\r\n"
				"Connection: Upgrade\r\n"
				"Upgrade: xweb-test\r\n\r\n"
				"PING",
				(unsigned)xrtWebServerPort(pServer));
			hUpgrade = pServer ? __Test_XHttpdConnectLoopback(xrtWebServerPort(pServer)) : XNET_SOCKET_INVALID;
			printf("  XWeb upgrade raw connect : %s\n", hUpgrade != XNET_SOCKET_INVALID ? "PASS" : "FAIL");
			printf("  XWeb upgrade request send : %s\n",
				hUpgrade != XNET_SOCKET_INVALID && __Test_XHttpdSendAll(hUpgrade,
					sUpgradeReq, strlen(sUpgradeReq)) ? "PASS" : "FAIL");
			printf("  XWeb upgrade response recv : %s\n",
				hUpgrade != XNET_SOCKET_INVALID && __Test_XHttpdRecvUntil(hUpgrade,
					aUpgradeResp, sizeof(aUpgradeResp), "XWEB-UPGRADE-OK",
					&iUpgradeRespLen, 3000u) ? "PASS" : "FAIL");
			printf("  XWeb upgrade response : %s\n",
				strstr(aUpgradeResp, "HTTP/1.1 101 Switching Protocols") &&
				strstr(aUpgradeResp, "Upgrade: xweb-test") ? "PASS" : "FAIL");
			printf("  XWeb upgrade committed callback : %s\n",
				__Test_XHttpdWaitMin(&tCtx.iUpgradeCount, iUpgradeBefore + 1, 1000u) ? "PASS" : "FAIL");
			printf("  XWeb upgrade buffered bytes : %s\n",
				__Test_XHttpdWaitMin(&tCtx.iUpgradeRecvCount, iRecvBefore + 1, 1000u) &&
				strcmp(tCtx.aUpgradeData, "PING") == 0 ? "PASS" : "FAIL");
			__Test_XHttpdCloseSocket(hUpgrade);
			printf("  XWeb upgrade close callback : %s\n",
				__Test_XHttpdWaitMin(&tCtx.iUpgradeCloseCount, iCloseBefore + 1, 2000u) ? "PASS" : "FAIL");
			printf("  XWeb upgrade error free : %s\n",
				__Test_XHttpdAtomicLoad(&tCtx.iUpgradeErrorCount) == 0 ? "PASS" : "FAIL");
			if ( tCtx.pUpgradeStream ) {
				xrtNetStreamDestroy(tCtx.pUpgradeStream);
				tCtx.pUpgradeStream = NULL;
			}
		}

		{
			xsocket hStream = XNET_SOCKET_INVALID;
			char sStreamHead[512];
			char aStreamResp[2048];
			size_t iStreamRespLen = 0u;
			long iChunkBefore = __Test_XHttpdAtomicLoad(&tCtx.iBodyChunkCount);
			snprintf(sStreamHead, sizeof(sStreamHead),
				"POST /stream-body/upload HTTP/1.1\r\n"
				"Host: 127.0.0.1:%u\r\n"
				"Connection: close\r\n"
				"Transfer-Encoding: chunked\r\n\r\n",
				(unsigned)xrtWebServerPort(pServer));
			hStream = pServer ? __Test_XHttpdConnectLoopback(xrtWebServerPort(pServer)) : XNET_SOCKET_INVALID;
			printf("  XWeb body stream raw connect : %s\n", hStream != XNET_SOCKET_INVALID ? "PASS" : "FAIL");
			printf("  XWeb body stream head send : %s\n", hStream != XNET_SOCKET_INVALID && __Test_XHttpdSendAll(hStream, sStreamHead, strlen(sStreamHead)) ? "PASS" : "FAIL");
			printf("  XWeb body stream chunk #1 send : %s\n", hStream != XNET_SOCKET_INVALID && __Test_XHttpdSendAll(hStream, "5\r\nhello\r\n", 10u) ? "PASS" : "FAIL");
			__Test_XHttpdSleepMs(60u);
			printf("  XWeb body stream chunk #2 send : %s\n", hStream != XNET_SOCKET_INVALID && __Test_XHttpdSendAll(hStream, "6\r\n-world\r\n0\r\n\r\n", 16u) ? "PASS" : "FAIL");
			printf("  XWeb body stream response recv : %s\n", hStream != XNET_SOCKET_INVALID && __Test_XHttpdRecvResponse(hStream, aStreamResp, sizeof(aStreamResp), &iStreamRespLen, 3000u) ? "PASS" : "FAIL");
			printf("  XWeb body stream status : %s\n", strstr(aStreamResp, "HTTP/1.1 200") != NULL ? "PASS" : "FAIL");
			printf("  XWeb body stream body : %s\n", strstr(aStreamResp, "\r\n\r\nstream:upload:hello-world") != NULL ? "PASS" : "FAIL");
			printf("  XWeb body stream callbacks : %s\n",
				__Test_XHttpdAtomicLoad(&tCtx.iBodyBeginCount) >= 1 &&
				__Test_XHttpdAtomicLoad(&tCtx.iBodyEndCount) >= 1 &&
				__Test_XHttpdAtomicLoad(&tCtx.iBodyChunkCount) >= iChunkBefore + 2 ? "PASS" : "FAIL");
			__Test_XHttpdCloseSocket(hStream);
		}

		{
			xsocket hStream = XNET_SOCKET_INVALID;
			char sStreamHead[512];
			char aStreamResp[2048];
			size_t iStreamRespLen = 0u;
			long iAsyncBefore = __Test_XHttpdAtomicLoad(&tCtx.iBodyAsyncCount);
			snprintf(sStreamHead, sizeof(sStreamHead),
				"POST /stream-body-async/upload HTTP/1.1\r\n"
				"Host: 127.0.0.1:%u\r\n"
				"Connection: close\r\n"
				"Transfer-Encoding: chunked\r\n\r\n",
				(unsigned)xrtWebServerPort(pServer));
			hStream = pServer ? __Test_XHttpdConnectLoopback(xrtWebServerPort(pServer)) : XNET_SOCKET_INVALID;
			printf("  XWeb async body stream raw connect : %s\n", hStream != XNET_SOCKET_INVALID ? "PASS" : "FAIL");
			printf("  XWeb async body stream head send : %s\n", hStream != XNET_SOCKET_INVALID && __Test_XHttpdSendAll(hStream, sStreamHead, strlen(sStreamHead)) ? "PASS" : "FAIL");
			printf("  XWeb async body stream chunk #1 send : %s\n", hStream != XNET_SOCKET_INVALID && __Test_XHttpdSendAll(hStream, "5\r\nalpha\r\n", 10u) ? "PASS" : "FAIL");
			__Test_XHttpdSleepMs(60u);
			printf("  XWeb async body stream chunk #2 send : %s\n", hStream != XNET_SOCKET_INVALID && __Test_XHttpdSendAll(hStream, "5\r\n-beta\r\n0\r\n\r\n", 15u) ? "PASS" : "FAIL");
			printf("  XWeb async body stream response recv : %s\n", hStream != XNET_SOCKET_INVALID && __Test_XHttpdRecvResponse(hStream, aStreamResp, sizeof(aStreamResp), &iStreamRespLen, 3000u) ? "PASS" : "FAIL");
			printf("  XWeb async body stream status : %s\n", strstr(aStreamResp, "HTTP/1.1 207") != NULL ? "PASS" : "FAIL");
			printf("  XWeb async body stream body : %s\n", strstr(aStreamResp, "\r\n\r\nstream-async:upload:alpha-beta") != NULL ? "PASS" : "FAIL");
			printf("  XWeb async body stream header : %s\n", strstr(aStreamResp, "\r\nX-Async: xweb-body-end\r\n") != NULL ? "PASS" : "FAIL");
			printf("  XWeb async body stream future count : %s\n", __Test_XHttpdWaitMin(&tCtx.iBodyAsyncCount, iAsyncBefore + 1, 1000u) ? "PASS" : "FAIL");
			__Test_XHttpdCloseSocket(hStream);
		}

		{
			xsocket hHead = XNET_SOCKET_INVALID;
			char sHeadReq[256];
			char aHeadResp[1024];
			size_t iHeadRespLen = 0u;
			bool bHeadRecv = false;
			snprintf(sHeadReq, sizeof(sHeadReq),
				"HEAD /head HTTP/1.1\r\n"
				"Host: 127.0.0.1:%u\r\n"
				"Connection: close\r\n\r\n",
				(unsigned)xrtWebServerPort(pServer));
			hHead = pServer ? __Test_XHttpdConnectLoopback(xrtWebServerPort(pServer)) : XNET_SOCKET_INVALID;
			printf("  XWeb head raw connect : %s\n", hHead != XNET_SOCKET_INVALID ? "PASS" : "FAIL");
			printf("  XWeb head send : %s\n", hHead != XNET_SOCKET_INVALID && __Test_XHttpdSendAll(hHead, sHeadReq, strlen(sHeadReq)) ? "PASS" : "FAIL");
			bHeadRecv = hHead != XNET_SOCKET_INVALID && __Test_XWebRecvRawClosed(hHead, aHeadResp, sizeof(aHeadResp), &iHeadRespLen, 2000u);
			printf("  XWeb head status : %s\n", bHeadRecv && strstr(aHeadResp, "HTTP/1.1 200") != NULL ? "PASS" : "FAIL");
			printf("  XWeb head content-length : %s\n", bHeadRecv && strstr(aHeadResp, "Content-Length: 2") != NULL ? "PASS" : "FAIL");
			printf("  XWeb head empty body : %s\n", bHeadRecv && strstr(aHeadResp, "\r\n\r\n") != NULL && strstr(aHeadResp, "\r\n\r\n")[4] == '\0' ? "PASS" : "FAIL");
			__Test_XHttpdCloseSocket(hHead);
		}

		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/missing", (unsigned)xrtWebServerPort(pServer));
		pResp = pServer ? __Test_XWebRequest(sURL, "GET", NULL, &iStatus) : NULL;
		printf("  XWeb custom error status : %s\n", iStatus == XRT_NET_OK && pResp && pResp->iStatusCode == 404u ? "PASS" : "FAIL");
		printf("  XWeb custom error body : %s\n", pResp && pResp->pBody && strcmp(pResp->pBody, "error:404:Not Found") == 0 ? "PASS" : "FAIL");
		printf("  XWeb custom error header : %s\n", pResp && xrtHttpResponseHeader(pResp, "X-XWeb-Error") != NULL ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		pResp = NULL;

		printf("  XWeb route callback count : %s\n", __Test_XHttpdWaitMin(&tCtx.iRouteCount, 1, 1000u) ? "PASS" : "FAIL");
		printf("  XWeb static callback count : %s\n", __Test_XHttpdWaitMin(&tCtx.iMeCount, 1, 1000u) ? "PASS" : "FAIL");
		printf("  XWeb post callback count : %s\n", __Test_XHttpdWaitMin(&tCtx.iPostCount, 1, 1000u) ? "PASS" : "FAIL");
		printf("  XWeb stream callback count : %s\n", __Test_XHttpdWaitMin(&tCtx.iStreamCount, 1, 1000u) ? "PASS" : "FAIL");
		printf("  XWeb error callback count : %s\n", __Test_XHttpdWaitMin(&tCtx.iErrorCount, 1, 1000u) ? "PASS" : "FAIL");
		printf("  XWeb request count stats : %s\n", pServer && xrtWebServerRequestCount(pServer) >= 7u ? "PASS" : "FAIL");
		printf("  XWeb connection count stats : %s\n", pServer && xrtWebServerConnectionCount(pServer) <= xrtWebServerRequestCount(pServer) ? "PASS" : "FAIL");

		{
			xsocket hRaw = XNET_SOCKET_INVALID;
			char sReq1[256];
			char aResp[2048];
			size_t iRespLen = 0u;
			bool bDrainNow;
			bool bRecv1 = false;
			snprintf(sReq1, sizeof(sReq1),
				"GET /users/me HTTP/1.1\r\n"
				"Host: 127.0.0.1:%u\r\n"
				"Connection: keep-alive\r\n\r\n",
				(unsigned)xrtWebServerPort(pServer));
			hRaw = pServer ? __Test_XHttpdConnectLoopback(xrtWebServerPort(pServer)) : XNET_SOCKET_INVALID;
			printf("  XWeb drain raw connect : %s\n", hRaw != XNET_SOCKET_INVALID ? "PASS" : "FAIL");
			printf("  XWeb drain keepalive send #1 : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdSendAll(hRaw, sReq1, strlen(sReq1)) ? "PASS" : "FAIL");
			bRecv1 = hRaw != XNET_SOCKET_INVALID && __Test_XHttpdRecvResponse(hRaw, aResp, sizeof(aResp), &iRespLen, 2000u);
			printf("  XWeb drain keepalive recv #1 : %s\n", bRecv1 ? "PASS" : "FAIL");
			printf("  XWeb drain keepalive header #1 : %s\n", bRecv1 && strstr(aResp, "Connection: keep-alive") != NULL ? "PASS" : "FAIL");
			bDrainNow = pServer && (xrtWebServerDrain(pServer, 0u) || xrtWebServerConnectionCount(pServer) > 0u);
			printf("  XWeb drain initiated : %s\n", bDrainNow ? "PASS" : "FAIL");
			__Test_XHttpdCloseSocket(hRaw);
			hRaw = XNET_SOCKET_INVALID;
			printf("  XWeb drain complete : %s\n", pServer && xrtWebServerDrain(pServer, 1000u) ? "PASS" : "FAIL");
			__Test_XHttpdCloseSocket(hRaw);
		}

		xrtHttpCloseIdleConnections(xrtNetSyncGetHiddenEngine());
		xrtNetSyncShutdownHiddenEngine();
		if ( pServer ) { xrtWebServerDestroy(pServer); }
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
		if ( sStaticFile ) { (void)xrtFileDelete((str)sStaticFile); }
		if ( sStaticHome ) { (void)xrtFileDelete((str)sStaticHome); }
		if ( sStaticRoot ) { (void)xrtDirDelete((str)sStaticRoot); }
		xrtFree(sStaticFile);
		xrtFree(sStaticHome);
		xrtFree(sStaticRoot);
	}

	{
		volatile long iRouteFreeCount = 0;
		xwebapp* pOwnedApp = xrtWebAppCreate();
		__test_xweb_owned_route_ctx* pOwnedCtx = (__test_xweb_owned_route_ctx*)xrtMalloc(sizeof(__test_xweb_owned_route_ctx));
		bool bRouteEx = false;
		if ( pOwnedCtx ) {
			pOwnedCtx->sText = "owned";
			pOwnedCtx->pFreeCount = &iRouteFreeCount;
		}
		bRouteEx = pOwnedApp && pOwnedCtx &&
			xrtWebAppRouteEx(pOwnedApp, XWEB_METHOD_GET, "/owned", __Test_XWebOwnedTextHandler, pOwnedCtx, __Test_XWebOwnedTextFree);
		printf("  XWeb app route ex register : %s\n", bRouteEx ? "PASS" : "FAIL");
		if ( !bRouteEx ) { xrtFree(pOwnedCtx); }
		xrtWebAppDestroy(pOwnedApp);
		printf("  XWeb app route ex free : %s\n", iRouteFreeCount == 1 ? "PASS" : "FAIL");
	}

	{
		xnetengineconfig tEngineCfg;
		xnetengine* pEngine = NULL;
		xwebserver* pServer = NULL;
		xhttpresponse* pResp = NULL;
		xnet_result iStatus = XRT_NET_ERROR;
		__test_xweb_ctx tLimitCtx;
		char sURL[512];

		memset(&tLimitCtx, 0, sizeof(tLimitCtx));
		xrtNetEngineConfigInit(&tEngineCfg);
		tEngineCfg.iWorkerCount = 1u;
		pEngine = xrtNetEngineCreate(&tEngineCfg);
		printf("  XWeb limit engine create : %s\n", pEngine != NULL ? "PASS" : "FAIL");
		printf("  XWeb limit engine start : %s\n", pEngine && xrtNetEngineStart(pEngine) == XRT_NET_OK ? "PASS" : "FAIL");

		pServer = pEngine ? xrtWebServerCreateHostEx(pEngine, "127.0.0.1", 0u, 64u, 1024u, 8u, 1u) : NULL;
		printf("  XWeb body limit server create : %s\n", pServer != NULL ? "PASS" : "FAIL");
		printf("  XWeb body limit route register : %s\n", pServer && xrtWebServerPost(pServer, "/limited", __Test_XWebPostHandler, &tLimitCtx) ? "PASS" : "FAIL");
		printf("  XWeb body limit server start : %s\n", pServer && xrtWebServerStart(pServer) == XRT_NET_OK ? "PASS" : "FAIL");

		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/limited", (unsigned)xrtWebServerPort(pServer));
		pResp = pServer ? __Test_XWebRequestContentType(sURL, "POST", "0123456789abcdef", "text/plain", &iStatus) : NULL;
		printf("  XWeb body limit status : %s\n", iStatus == XRT_NET_OK && pResp && pResp->iStatusCode == 413u ? "PASS" : "FAIL");
		printf("  XWeb body limit handler bypass : %s\n", tLimitCtx.iPostCount == 0 ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }

		xrtHttpCloseIdleConnections(xrtNetSyncGetHiddenEngine());
		xrtNetSyncShutdownHiddenEngine();
		if ( pServer ) { xrtWebServerDestroy(pServer); }
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tEngineCfg;
		xwebconfig tWebCfg;
		xnetengine* pEngine = NULL;
		xwebserver* pServer = NULL;
		xhttpresponse* pResp = NULL;
		xnet_result iStatus = XRT_NET_ERROR;
		__test_xweb_ctx tMultipartLimitCtx;
		const char* sBigPart =
			"--xlimit\r\n"
			"Content-Disposition: form-data; name=\"data\"\r\n"
			"\r\n"
			"abcde\r\n"
			"--xlimit--\r\n";
		const char* sManyParts =
			"--xlimit\r\n"
			"Content-Disposition: form-data; name=\"a\"\r\n"
			"\r\n"
			"1\r\n"
			"--xlimit\r\n"
			"Content-Disposition: form-data; name=\"b\"\r\n"
			"\r\n"
			"2\r\n"
			"--xlimit--\r\n";
		const char* sTruncatedPart =
			"--xlimit\r\n"
			"Content-Disposition: form-data; name=\"a\"\r\n"
			"\r\n"
			"1";
		const char* sMissingName =
			"--xlimit\r\n"
			"Content-Disposition: form-data\r\n"
			"\r\n"
			"1\r\n"
			"--xlimit--\r\n";
		const char* sDuplicateDisposition =
			"--xlimit\r\n"
			"Content-Disposition: form-data; name=\"a\"\r\n"
			"Content-Disposition: form-data; name=\"b\"\r\n"
			"\r\n"
			"1\r\n"
			"--xlimit--\r\n";
		const char* sInvalidPartHeader =
			"--xlimit\r\n"
			"Content-Disposition: form-data; name=\"a\"\r\n"
			"Invalid Header\r\n"
			"\r\n"
			"1\r\n"
			"--xlimit--\r\n";
		char sURL[512];

		memset(&tMultipartLimitCtx, 0, sizeof(tMultipartLimitCtx));
		xrtNetEngineConfigInit(&tEngineCfg);
		tEngineCfg.iWorkerCount = 1u;
		pEngine = xrtNetEngineCreate(&tEngineCfg);
		printf("  XWeb multipart limit engine create : %s\n", pEngine != NULL ? "PASS" : "FAIL");
		printf("  XWeb multipart limit engine start : %s\n", pEngine && xrtNetEngineStart(pEngine) == XRT_NET_OK ? "PASS" : "FAIL");

		xrtWebConfigInit(&tWebCfg);
		(void)xrtNetAddrParse(&tWebCfg.tBindAddr, "127.0.0.1", 0u);
		tWebCfg.iBodyLimit = 1024u;
		tWebCfg.iMultipartPartLimit = 1u;
		tWebCfg.iMultipartPartSizeLimit = 4u;
		pServer = pEngine ? xrtWebServerCreate(pEngine, &tWebCfg) : NULL;
		printf("  XWeb multipart limit server create : %s\n", pServer != NULL ? "PASS" : "FAIL");
		printf("  XWeb multipart limit route register : %s\n", pServer && xrtWebServerPost(pServer, "/multipart-limit", __Test_XWebPostHandler, &tMultipartLimitCtx) ? "PASS" : "FAIL");
		printf("  XWeb multipart limit server start : %s\n", pServer && xrtWebServerStart(pServer) == XRT_NET_OK ? "PASS" : "FAIL");

		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/multipart-limit", (unsigned)xrtWebServerPort(pServer));
		pResp = pServer ? __Test_XWebRequestContentType(sURL, "POST", sBigPart, "multipart/form-data; boundary=xlimit", &iStatus) : NULL;
		printf("  XWeb multipart part size limit status : %s\n", iStatus == XRT_NET_OK && pResp && pResp->iStatusCode == 413u ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		pResp = pServer ? __Test_XWebRequestContentType(sURL, "POST", sManyParts, "multipart/form-data; boundary=xlimit", &iStatus) : NULL;
		printf("  XWeb multipart part count limit status : %s\n", iStatus == XRT_NET_OK && pResp && pResp->iStatusCode == 413u ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		pResp = pServer ? __Test_XWebRequestContentType(sURL, "POST", sTruncatedPart, "multipart/form-data; boundary=xlimit", &iStatus) : NULL;
		printf("  XWeb multipart truncated body status : %s\n", iStatus == XRT_NET_OK && pResp && pResp->iStatusCode == 400u ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		pResp = pServer ? __Test_XWebRequestContentType(sURL, "POST", sMissingName, "multipart/form-data; boundary=xlimit", &iStatus) : NULL;
		printf("  XWeb multipart missing name status : %s\n", iStatus == XRT_NET_OK && pResp && pResp->iStatusCode == 400u ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		pResp = pServer ? __Test_XWebRequestContentType(sURL, "POST", sDuplicateDisposition, "multipart/form-data; boundary=xlimit", &iStatus) : NULL;
		printf("  XWeb multipart duplicate disposition status : %s\n", iStatus == XRT_NET_OK && pResp && pResp->iStatusCode == 400u ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		pResp = pServer ? __Test_XWebRequestContentType(sURL, "POST", sInvalidPartHeader, "multipart/form-data; boundary=xlimit", &iStatus) : NULL;
		printf("  XWeb multipart invalid part header status : %s\n", iStatus == XRT_NET_OK && pResp && pResp->iStatusCode == 400u ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		pResp = pServer ? __Test_XWebRequestContentType(sURL, "POST", sMissingName, "multipart/form-data", &iStatus) : NULL;
		printf("  XWeb multipart missing boundary status : %s\n", iStatus == XRT_NET_OK && pResp && pResp->iStatusCode == 400u ? "PASS" : "FAIL");
		printf("  XWeb multipart policy handler bypass : %s\n", tMultipartLimitCtx.iPostCount == 0 ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }

		xrtHttpCloseIdleConnections(xrtNetSyncGetHiddenEngine());
		xrtNetSyncShutdownHiddenEngine();
		if ( pServer ) { xrtWebServerDestroy(pServer); }
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tEngineCfg;
		xwebconfig tWebCfg;
		xnetengine* pEngine = NULL;
		xwebserver* pServer = NULL;
		__test_xweb_ctx tHeaderCtx;
		xsocket hRaw = XNET_SOCKET_INVALID;
		char sReq[512];
		char aResp[1024];
		size_t iRespLen = 0u;
		bool bRecv = false;

		memset(&tHeaderCtx, 0, sizeof(tHeaderCtx));
		xrtNetEngineConfigInit(&tEngineCfg);
		tEngineCfg.iWorkerCount = 1u;
		pEngine = xrtNetEngineCreate(&tEngineCfg);
		printf("  XWeb header limit engine create : %s\n", pEngine != NULL ? "PASS" : "FAIL");
		printf("  XWeb header limit engine start : %s\n", pEngine && xrtNetEngineStart(pEngine) == XRT_NET_OK ? "PASS" : "FAIL");

		xrtWebConfigInit(&tWebCfg);
		(void)xrtNetAddrParse(&tWebCfg.tBindAddr, "127.0.0.1", 0u);
		tWebCfg.iHeaderLimit = 2u;
		tWebCfg.iHeaderBytesLimit = 1024u;
		pServer = pEngine ? xrtWebServerCreate(pEngine, &tWebCfg) : NULL;
		printf("  XWeb header limit server create : %s\n", pServer != NULL ? "PASS" : "FAIL");
		printf("  XWeb header limit route register : %s\n", pServer && xrtWebServerGet(pServer, "/headers", __Test_XWebMeHandler, &tHeaderCtx) ? "PASS" : "FAIL");
		printf("  XWeb header limit server start : %s\n", pServer && xrtWebServerStart(pServer) == XRT_NET_OK ? "PASS" : "FAIL");

		snprintf(sReq, sizeof(sReq),
			"GET /headers HTTP/1.1\r\n"
			"Host: 127.0.0.1:%u\r\n"
			"X-A: 1\r\n"
			"X-B: 2\r\n"
			"Connection: close\r\n\r\n",
			(unsigned)xrtWebServerPort(pServer));
		hRaw = pServer ? __Test_XHttpdConnectLoopback(xrtWebServerPort(pServer)) : XNET_SOCKET_INVALID;
		printf("  XWeb header limit raw connect : %s\n", hRaw != XNET_SOCKET_INVALID ? "PASS" : "FAIL");
		printf("  XWeb header limit send : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdSendAll(hRaw, sReq, strlen(sReq)) ? "PASS" : "FAIL");
		bRecv = hRaw != XNET_SOCKET_INVALID && __Test_XWebRecvRawClosed(hRaw, aResp, sizeof(aResp), &iRespLen, 2000u);
		printf("  XWeb header limit status : %s\n", bRecv && strstr(aResp, "HTTP/1.1 431") != NULL ? "PASS" : "FAIL");
		printf("  XWeb header limit body : %s\n", bRecv && strstr(aResp, "Request Header Fields Too Large") != NULL ? "PASS" : "FAIL");
		printf("  XWeb header limit handler bypass : %s\n", tHeaderCtx.iMeCount == 0 ? "PASS" : "FAIL");
		__Test_XHttpdCloseSocket(hRaw);

		xrtHttpCloseIdleConnections(xrtNetSyncGetHiddenEngine());
		xrtNetSyncShutdownHiddenEngine();
		if ( pServer ) { xrtWebServerDestroy(pServer); }
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xnetengineconfig tEngineCfg;
		xwebconfig tWebCfg;
		xnetengine* pEngine = NULL;
		xwebserver* pServer = NULL;
		__test_xweb_ctx tTimeoutCtx;
		xsocket hRaw = XNET_SOCKET_INVALID;
		char sReq[512];
		char aResp[1024];
		size_t iRespLen = 0u;
		bool bRecv = false;
		bool bClosed = false;
		bool bWriteQueued = false;

		memset(&tTimeoutCtx, 0, sizeof(tTimeoutCtx));
		xrtNetEngineConfigInit(&tEngineCfg);
		tEngineCfg.iWorkerCount = 1u;
		pEngine = xrtNetEngineCreate(&tEngineCfg);
		printf("  XWeb timeout engine create : %s\n", pEngine != NULL ? "PASS" : "FAIL");
		printf("  XWeb timeout engine start : %s\n", pEngine && xrtNetEngineStart(pEngine) == XRT_NET_OK ? "PASS" : "FAIL");

		xrtWebConfigInit(&tWebCfg);
		(void)xrtNetAddrParse(&tWebCfg.tBindAddr, "127.0.0.1", 0u);
		tWebCfg.iBodyLimit = 1024u;
		tWebCfg.iHeaderBytesLimit = 1024u;
		tWebCfg.iHeaderTimeoutMs = 120u;
		tWebCfg.iBodyTimeoutMs = 120u;
		tWebCfg.iIdleTimeoutMs = 120u;
		tWebCfg.iWriteTimeoutMs = 120u;
		pServer = pEngine ? xrtWebServerCreate(pEngine, &tWebCfg) : NULL;
		printf("  XWeb timeout server create : %s\n", pServer != NULL ? "PASS" : "FAIL");
		printf("  XWeb timeout get route register : %s\n", pServer && xrtWebServerGet(pServer, "/idle", __Test_XWebMeHandler, &tTimeoutCtx) ? "PASS" : "FAIL");
		printf("  XWeb timeout post route register : %s\n", pServer && xrtWebServerPost(pServer, "/slow-body", __Test_XWebPostHandler, &tTimeoutCtx) ? "PASS" : "FAIL");
		printf("  XWeb timeout write route register : %s\n", pServer && xrtWebServerGet(pServer, "/slow-write", __Test_XWebSlowWriteHandler, &tTimeoutCtx) ? "PASS" : "FAIL");
		printf("  XWeb timeout server start : %s\n", pServer && xrtWebServerStart(pServer) == XRT_NET_OK ? "PASS" : "FAIL");

		hRaw = pServer ? __Test_XHttpdConnectLoopback(xrtWebServerPort(pServer)) : XNET_SOCKET_INVALID;
		printf("  XWeb header timeout raw connect : %s\n", hRaw != XNET_SOCKET_INVALID ? "PASS" : "FAIL");
		bRecv = hRaw != XNET_SOCKET_INVALID && __Test_XWebRecvRawClosed(hRaw, aResp, sizeof(aResp), &iRespLen, 1000u);
		printf("  XWeb header timeout status : %s\n", bRecv && strstr(aResp, "HTTP/1.1 408") != NULL ? "PASS" : "FAIL");
		printf("  XWeb header timeout body : %s\n", bRecv && strstr(aResp, "Request Timeout") != NULL ? "PASS" : "FAIL");
		__Test_XHttpdCloseSocket(hRaw);
		hRaw = XNET_SOCKET_INVALID;

		snprintf(sReq, sizeof(sReq),
			"POST /slow-body HTTP/1.1\r\n"
			"Host: 127.0.0.1:%u\r\n"
			"Content-Length: 8\r\n"
			"Connection: close\r\n\r\n"
			"1234",
			(unsigned)xrtWebServerPort(pServer));
		hRaw = pServer ? __Test_XHttpdConnectLoopback(xrtWebServerPort(pServer)) : XNET_SOCKET_INVALID;
		printf("  XWeb body timeout raw connect : %s\n", hRaw != XNET_SOCKET_INVALID ? "PASS" : "FAIL");
		printf("  XWeb body timeout partial send : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdSendAll(hRaw, sReq, strlen(sReq)) ? "PASS" : "FAIL");
		bRecv = hRaw != XNET_SOCKET_INVALID && __Test_XWebRecvRawClosed(hRaw, aResp, sizeof(aResp), &iRespLen, 1000u);
		printf("  XWeb body timeout status : %s\n", bRecv && strstr(aResp, "HTTP/1.1 408") != NULL ? "PASS" : "FAIL");
		printf("  XWeb body timeout handler bypass : %s\n", tTimeoutCtx.iPostCount == 0 ? "PASS" : "FAIL");
		__Test_XHttpdCloseSocket(hRaw);
		hRaw = XNET_SOCKET_INVALID;

		snprintf(sReq, sizeof(sReq),
			"GET /idle HTTP/1.1\r\n"
			"Host: 127.0.0.1:%u\r\n"
			"Connection: keep-alive\r\n\r\n",
			(unsigned)xrtWebServerPort(pServer));
		hRaw = pServer ? __Test_XHttpdConnectLoopback(xrtWebServerPort(pServer)) : XNET_SOCKET_INVALID;
		printf("  XWeb idle timeout raw connect : %s\n", hRaw != XNET_SOCKET_INVALID ? "PASS" : "FAIL");
		printf("  XWeb idle timeout request send : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdSendAll(hRaw, sReq, strlen(sReq)) ? "PASS" : "FAIL");
		bRecv = hRaw != XNET_SOCKET_INVALID && __Test_XHttpdRecvResponse(hRaw, aResp, sizeof(aResp), &iRespLen, 1000u);
		printf("  XWeb idle timeout first response : %s\n", bRecv && strstr(aResp, "HTTP/1.1 200") != NULL ? "PASS" : "FAIL");
		printf("  XWeb idle timeout keepalive header : %s\n", bRecv && strstr(aResp, "Connection: keep-alive") != NULL ? "PASS" : "FAIL");
		bClosed = hRaw != XNET_SOCKET_INVALID && __Test_XWebWaitSocketClosed(hRaw, 1000u);
		printf("  XWeb idle timeout close : %s\n", bClosed ? "PASS" : "FAIL");
		__Test_XHttpdCloseSocket(hRaw);
		hRaw = XNET_SOCKET_INVALID;

		snprintf(sReq, sizeof(sReq),
			"GET /slow-write HTTP/1.1\r\n"
			"Host: 127.0.0.1:%u\r\n"
			"Connection: keep-alive\r\n\r\n",
			(unsigned)xrtWebServerPort(pServer));
		hRaw = pServer ? __Test_XWebConnectSlowReader(xrtWebServerPort(pServer)) : XNET_SOCKET_INVALID;
		printf("  XWeb write timeout raw connect : %s\n", hRaw != XNET_SOCKET_INVALID ? "PASS" : "FAIL");
		printf("  XWeb write timeout request send : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdSendAll(hRaw, sReq, strlen(sReq)) ? "PASS" : "FAIL");
		bWriteQueued = hRaw != XNET_SOCKET_INVALID && __Test_XHttpdWaitMin(&tTimeoutCtx.iSlowWriteDone, 1, 5000u);
		printf("  XWeb write timeout response queued : %s\n", bWriteQueued ? "PASS" : "FAIL");
		bClosed = bWriteQueued && __Test_XWebDrainUntilClosed(hRaw, 400u, 2000u);
		printf("  XWeb write timeout close : %s\n", bClosed ? "PASS" : "FAIL");
		__Test_XHttpdCloseSocket(hRaw);

		xrtHttpCloseIdleConnections(xrtNetSyncGetHiddenEngine());
		xrtNetSyncShutdownHiddenEngine();
		if ( pServer ) { xrtWebServerDestroy(pServer); }
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	{
		xwebconfig tWebCfg;
		xwebserver* pServer = NULL;
		xhttpresponse* pResp = NULL;
		xnet_result iStatus = XRT_NET_ERROR;
		__test_xweb_ctx tLimitCtx;
		__test_xweb_ctx tHeaderCtx;
		xsocket hRaw = XNET_SOCKET_INVALID;
		char sURL[512];
		char sReq[512];
		char aResp[1024];
		size_t iRespLen = 0u;
		bool bRecv = false;

		memset(&tLimitCtx, 0, sizeof(tLimitCtx));
		pServer = xrtWebServerCreateHostFullEx(NULL, "127.0.0.1", 0u,
			64u, 1024u, 8u, 0u, 0u, 1u, 0u, 0u, 0u, 0u, 1000u, 0u);
		printf("  XWeb owned body server create : %s\n", pServer != NULL ? "PASS" : "FAIL");
		printf("  XWeb owned body route register : %s\n", pServer && xrtWebServerPost(pServer, "/limited", __Test_XWebPostHandler, &tLimitCtx) ? "PASS" : "FAIL");
		printf("  XWeb owned body server start : %s\n", pServer && xrtWebServerStart(pServer) == XRT_NET_OK ? "PASS" : "FAIL");
		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/limited", (unsigned)xrtWebServerPort(pServer));
		pResp = pServer ? __Test_XWebRequestContentType(sURL, "POST", "0123456789abcdef", "text/plain", &iStatus) : NULL;
		printf("  XWeb owned body limit status : %s\n", iStatus == XRT_NET_OK && pResp && pResp->iStatusCode == 413u ? "PASS" : "FAIL");
		printf("  XWeb owned body handler bypass : %s\n", tLimitCtx.iPostCount == 0 ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		xrtHttpCloseIdleConnections(xrtNetSyncGetHiddenEngine());
		xrtNetSyncShutdownHiddenEngine();
		if ( pServer ) { xrtWebServerDestroy(pServer); }

		memset(&tHeaderCtx, 0, sizeof(tHeaderCtx));
		xrtWebConfigInit(&tWebCfg);
		(void)xrtNetAddrParse(&tWebCfg.tBindAddr, "127.0.0.1", 0u);
		tWebCfg.iHeaderLimit = 2u;
		tWebCfg.iHeaderBytesLimit = 1024u;
		tWebCfg.iWorkerCount = 1u;
		pServer = xrtWebServerCreate(NULL, &tWebCfg);
		printf("  XWeb owned header server create : %s\n", pServer != NULL ? "PASS" : "FAIL");
		printf("  XWeb owned header route register : %s\n", pServer && xrtWebServerGet(pServer, "/headers", __Test_XWebMeHandler, &tHeaderCtx) ? "PASS" : "FAIL");
		printf("  XWeb owned header server start : %s\n", pServer && xrtWebServerStart(pServer) == XRT_NET_OK ? "PASS" : "FAIL");
		snprintf(sReq, sizeof(sReq),
			"GET /headers HTTP/1.1\r\n"
			"Host: 127.0.0.1:%u\r\n"
			"X-A: 1\r\n"
			"X-B: 2\r\n"
			"Connection: close\r\n\r\n",
			(unsigned)xrtWebServerPort(pServer));
		hRaw = pServer ? __Test_XHttpdConnectLoopback(xrtWebServerPort(pServer)) : XNET_SOCKET_INVALID;
		printf("  XWeb owned header raw connect : %s\n", hRaw != XNET_SOCKET_INVALID ? "PASS" : "FAIL");
		printf("  XWeb owned header send : %s\n", hRaw != XNET_SOCKET_INVALID && __Test_XHttpdSendAll(hRaw, sReq, strlen(sReq)) ? "PASS" : "FAIL");
		bRecv = hRaw != XNET_SOCKET_INVALID && __Test_XWebRecvRawClosed(hRaw, aResp, sizeof(aResp), &iRespLen, 2000u);
		printf("  XWeb owned header status : %s\n", bRecv && strstr(aResp, "HTTP/1.1 431") != NULL ? "PASS" : "FAIL");
		printf("  XWeb owned header body : %s\n", bRecv && strstr(aResp, "Request Header Fields Too Large") != NULL ? "PASS" : "FAIL");
		printf("  XWeb owned header handler bypass : %s\n", tHeaderCtx.iMeCount == 0 ? "PASS" : "FAIL");
		__Test_XHttpdCloseSocket(hRaw);
		if ( pServer ) { xrtWebServerDestroy(pServer); }
	}

	{
		xnetengineconfig tEngineCfg;
		xnetengine* pEngine = NULL;
		xwebserver* pServer = NULL;
		xwebapp* pApp1 = NULL;
		xwebapp* pApp2 = NULL;
		xwebapp* pHostApp = NULL;
		xhttpresponse* pResp = NULL;
		xnet_result iStatus = XRT_NET_ERROR;
		char sURL[512];
		uint16 iPort = 0u;

		xrtNetEngineConfigInit(&tEngineCfg);
		tEngineCfg.iWorkerCount = 1u;
		pEngine = xrtNetEngineCreate(&tEngineCfg);
		printf("  XWeb app engine create : %s\n", pEngine != NULL ? "PASS" : "FAIL");
		printf("  XWeb app engine start : %s\n", pEngine && xrtNetEngineStart(pEngine) == XRT_NET_OK ? "PASS" : "FAIL");

		pServer = pEngine ? xrtWebServerCreateHostEx(pEngine, "127.0.0.1", 0u, 128u, 1024u * 1024u, 1024u * 1024u, 1u) : NULL;
		pApp1 = xrtWebAppCreate();
		pApp2 = xrtWebAppCreate();
		pHostApp = xrtWebAppCreate();
		printf("  XWeb app create : %s\n", pServer && pApp1 && pApp2 && pHostApp ? "PASS" : "FAIL");

		printf("  XWeb app route register : %s\n",
			pApp1 && pApp2 && pHostApp &&
			xrtWebAppGet(pApp1, "/", __Test_XWebTextHandler, "app1") &&
			xrtWebAppGet(pApp2, "/", __Test_XWebTextHandler, "app2") &&
			xrtWebAppGet(pHostApp, "/", __Test_XWebTextHandler, "host")
				? "PASS" : "FAIL");
		printf("  XWeb server set app : %s\n", pServer && pApp1 && xrtWebServerSetApp(pServer, pApp1) ? "PASS" : "FAIL");
		printf("  XWeb server host app : %s\n", pServer && pHostApp && xrtWebServerHost(pServer, "x.local", pHostApp) ? "PASS" : "FAIL");

		printf("  XWeb app server start : %s\n", pServer && xrtWebServerStart(pServer) == XRT_NET_OK ? "PASS" : "FAIL");
		iPort = pServer ? xrtWebServerPort(pServer) : 0u;
		snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/", (unsigned)iPort);

		pResp = pServer ? __Test_XWebRequest(sURL, "GET", NULL, &iStatus) : NULL;
		printf("  XWeb default app response : %s\n", iStatus == XRT_NET_OK && pResp && pResp->pBody && strcmp(pResp->pBody, "app1") == 0 ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		pResp = NULL;

		pResp = pServer ? __Test_XWebRequestHost(sURL, "GET", NULL, "x.local", &iStatus) : NULL;
		printf("  XWeb vhost response : %s\n", iStatus == XRT_NET_OK && pResp && pResp->pBody && strcmp(pResp->pBody, "host") == 0 ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		pResp = NULL;

		printf("  XWeb reload app : %s\n", pServer && pApp2 && xrtWebServerReloadApp(pServer, pApp2) ? "PASS" : "FAIL");
		pResp = pServer ? __Test_XWebRequest(sURL, "GET", NULL, &iStatus) : NULL;
		printf("  XWeb reloaded app response : %s\n", iStatus == XRT_NET_OK && pResp && pResp->pBody && strcmp(pResp->pBody, "app2") == 0 ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		pResp = NULL;

		printf("  XWeb remove host app : %s\n", pServer && xrtWebServerRemoveHost(pServer, "x.local") ? "PASS" : "FAIL");
		pResp = pServer ? __Test_XWebRequestHost(sURL, "GET", NULL, "x.local", &iStatus) : NULL;
		printf("  XWeb removed host fallback : %s\n", iStatus == XRT_NET_OK && pResp && pResp->pBody && strcmp(pResp->pBody, "app2") == 0 ? "PASS" : "FAIL");
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }

		xrtHttpCloseIdleConnections(xrtNetSyncGetHiddenEngine());
		xrtNetSyncShutdownHiddenEngine();
		if ( pServer ) { xrtWebServerDestroy(pServer); }
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
		xrtWebAppDestroy(pApp1);
		xrtWebAppDestroy(pApp2);
		xrtWebAppDestroy(pHostApp);
	}

	{
		xtlsconfig tTlsCfg;
		xnetengineconfig tEngineCfg;
		xnetengine* pEngine = NULL;
		xwebserver* pServer = NULL;
		xhttpresponse* pResp = NULL;
		xnet_result iStatus = XRT_NET_ERROR;
		char sURL[512];
		bool bHasTlsFixtures;

		memset(&tTlsCfg, 0, sizeof(tTlsCfg));
		tTlsCfg.sCertFile = "dev/xnet2_tls_test_cert.pem";
		tTlsCfg.sKeyFile = "dev/xnet2_tls_test_key.pem";
		bHasTlsFixtures = xrtFileExists((str)tTlsCfg.sCertFile) && xrtFileExists((str)tTlsCfg.sKeyFile);
		printf("  XWeb TLS fixture files exist : %s\n", bHasTlsFixtures ? "PASS" : "SKIP");

		if ( bHasTlsFixtures ) {
			xrtNetEngineConfigInit(&tEngineCfg);
			tEngineCfg.iWorkerCount = 1u;
			pEngine = xrtNetEngineCreate(&tEngineCfg);
			printf("  XWeb TLS engine create : %s\n", pEngine != NULL ? "PASS" : "FAIL");
			printf("  XWeb TLS engine start : %s\n", pEngine && xrtNetEngineStart(pEngine) == XRT_NET_OK ? "PASS" : "FAIL");

			pServer = pEngine ? xrtWebServerCreateHostTlsFullEx(pEngine,
				tTlsCfg.sCertFile, tTlsCfg.sKeyFile, "127.0.0.1", 0u,
				128u, 1024u * 1024u, 1024u * 1024u, 100u, 64u * 1024u, 1u,
				10000u, 30000u, 30000u, 30000u, 1000u, 0u) : NULL;
			printf("  XWeb TLS server create : %s\n", pServer != NULL ? "PASS" : "FAIL");
			printf("  XWeb TLS route register : %s\n", pServer && xrtWebServerGet(pServer, "/secure", __Test_XWebTextHandler, "secure") ? "PASS" : "FAIL");
			printf("  XWeb TLS SNI route register : %s\n", pServer && xrtWebServerGet(pServer, "/sni", __Test_XWebSNIHandler, NULL) ? "PASS" : "FAIL");
			printf("  XWeb TLS SNI cert register : %s\n", pServer && xrtWebServerTlsHost(pServer, "127.0.0.1", tTlsCfg.sCertFile, tTlsCfg.sKeyFile) ? "PASS" : "FAIL");
			printf("  XWeb TLS server start : %s\n", pServer && xrtWebServerStart(pServer) == XRT_NET_OK ? "PASS" : "FAIL");

			snprintf(sURL, sizeof(sURL), "https://127.0.0.1:%u/secure", (unsigned)xrtWebServerPort(pServer));
			pResp = pServer ? __Test_XWebRequest(sURL, "GET", NULL, &iStatus) : NULL;
			printf("  XWeb TLS client status : %s\n", iStatus == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  XWeb TLS response status : %s\n", pResp && pResp->iStatusCode == 200u ? "PASS" : "FAIL");
			printf("  XWeb TLS response body : %s\n", pResp && pResp->pBody && strcmp(pResp->pBody, "secure") == 0 ? "PASS" : "FAIL");
			if ( pResp ) { xrtHttpResponseDestroy(pResp); }
			pResp = NULL;

			snprintf(sURL, sizeof(sURL), "https://127.0.0.1:%u/sni", (unsigned)xrtWebServerPort(pServer));
			pResp = pServer ? __Test_XWebRequest(sURL, "GET", NULL, &iStatus) : NULL;
			printf("  XWeb TLS SNI request status : %s\n", iStatus == XRT_NET_OK && pResp && pResp->iStatusCode == 200u ? "PASS" : "FAIL");
			printf("  XWeb TLS SNI request body : %s\n", pResp && pResp->pBody && strcmp(pResp->pBody, "127.0.0.1") == 0 ? "PASS" : "FAIL");
			printf("  XWeb TLS SNI cert remove : %s\n", pServer && xrtWebServerRemoveTlsHost(pServer, "127.0.0.1") ? "PASS" : "FAIL");
			if ( pResp ) { xrtHttpResponseDestroy(pResp); }

			xrtHttpCloseIdleConnections(xrtNetSyncGetHiddenEngine());
			xrtNetSyncShutdownHiddenEngine();
			if ( pServer ) { xrtWebServerDestroy(pServer); }
			if ( pEngine ) {
				xrtNetEngineStop(pEngine);
				xrtNetEngineDestroy(pEngine);
			}
		}
	}
#undef printf
	return iFailCount == 0 ? 0 : 1;
}

#endif
