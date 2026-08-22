#include "../internal/xrt_websocket_http_future.h"



#if defined(XWS_FEATURE_WEBSOCKET_HTTP_FUTURE)

/* 记录连接建立结果的稳定错误上下文。 */
static void __xrtWsOpenResultError(
	xerrkind Kind,
	xwsopenresulterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xwsErrorSetDetail(
		Kind,
		"xrt.websocket.open-result",
		(int32)Code,
		sOperation,
		sMessage,
		NULL
	);
}



/* 验证公开连接建立结果覆盖完整固定存储。 */
static bool __xrtWsOpenResultValid(
	const xwsopenresult* pResult,
	cstr sOperation
)
{
	if ( xrtMemRangeValid(pResult, sizeof(*pResult)) ) {
		return true;
	}
	__xrtWsOpenResultError(
		XERR_ARGUMENT,
		XWS_OPEN_RESULT_ERROR_ARGUMENT,
		sOperation,
		"WebSocket open result range is invalid"
	);
	return false;
}




/* 增加连接建立结果引用并返回原指针。 */
XRT_API xwsopenresult* xrtWsOpenResultRef(
	xwsopenresult* pResult
)
{
	if ( !__xrtWsOpenResultValid(
		pResult,
		"retain-websocket-open-result"
	) ) {
		return NULL;
	}
	if ( xrtRefRetain(&pResult->References) < 0 ) {
		__xrtWsOpenResultError(
			XERR_STATE,
			XWS_OPEN_RESULT_ERROR_STATE,
			"retain-websocket-open-result",
			"WebSocket open result cannot be retained"
		);
		return NULL;
	}
	return pResult;
}



/* 释放最后一个结果引用及其尚未取走的全部所有权。 */
XRT_API void xrtWsOpenResultDestroy(
	xwsopenresult* pResult
)
{
	xwsconn* pConnection;
	#if defined(XWS_FEATURE_WEBSOCKET_CLIENT_FUTURE)
		xhttpresponse* pResponse;
	#endif

	if ( pResult == NULL ) {
		return;
	}
	if ( !__xrtWsOpenResultValid(
		pResult,
		"destroy-websocket-open-result"
	) ) {
		return;
	}
	if ( xrtRefRelease(&pResult->References) != 0 ) {
		return;
	}
	pConnection = (xwsconn*)xrtAtomicPtrExchange(
		&pResult->Connection,
		NULL,
		XMEMORY_ACQ_REL
	);
	__xrtWsOpenConnectionDestroy(pConnection);
	#if defined(XWS_FEATURE_WEBSOCKET_CLIENT_FUTURE)
		pResponse = (xhttpresponse*)xrtAtomicPtrExchange(
			&pResult->Response,
			NULL,
			XMEMORY_ACQ_REL
		);
		xrtHttpResponseDestroy(pResponse);
	#endif
	memset(pResult, 0, sizeof(*pResult));
	xrtFree(pResult);
}



/* 返回结果借用的 WebSocket Connection。 */
XRT_API xwsconn* xrtWsOpenResultConnection(
	const xwsopenresult* pResult
)
{
	if ( !__xrtWsOpenResultValid(
		pResult,
		"get-websocket-open-connection"
	) ) {
		return NULL;
	}
	return (xwsconn*)xrtAtomicPtrLoad(
		&pResult->Connection,
		XMEMORY_ACQUIRE
	);
}



/* 原子取走结果拥有的 WebSocket Connection。 */
XRT_API xwsconn* xrtWsOpenResultTakeConnection(
	xwsopenresult* pResult
)
{
	if ( !__xrtWsOpenResultValid(
		pResult,
		"take-websocket-open-connection"
	) ) {
		return NULL;
	}
	return (xwsconn*)xrtAtomicPtrExchange(
		&pResult->Connection,
		NULL,
		XMEMORY_ACQ_REL
	);
}



#if defined(XWS_FEATURE_WEBSOCKET_CLIENT_FUTURE)

/* 返回结果借用的客户端 HTTP 101 Response。 */
XRT_API const xhttpresponse* xrtWsOpenResultResponse(
	const xwsopenresult* pResult
)
{
	if ( !__xrtWsOpenResultValid(
		pResult,
		"get-websocket-open-response"
	) ) {
		return NULL;
	}
	return (const xhttpresponse*)xrtAtomicPtrLoad(
		&pResult->Response,
		XMEMORY_ACQUIRE
	);
}



/* 原子取走结果拥有的客户端 HTTP 101 Response。 */
XRT_API xhttpresponse* xrtWsOpenResultTakeResponse(
	xwsopenresult* pResult
)
{
	if ( !__xrtWsOpenResultValid(
		pResult,
		"take-websocket-open-response"
	) ) {
		return NULL;
	}
	return (xhttpresponse*)xrtAtomicPtrExchange(
		&pResult->Response,
		NULL,
		XMEMORY_ACQ_REL
	);
}

#endif

#endif
