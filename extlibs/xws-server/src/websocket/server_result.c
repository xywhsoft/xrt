#include "../internal/xrt_websocket_server_future.h"

#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_FUTURE)




/* 记录连接建立结果的稳定错误上下文。 */
static void __xrtWsServerResultError(
	xerrkind Kind,
	xwsserverresulterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xwsServerErrorSetDetail(
		Kind,
		"xrt.websocket.open-result",
		(int32)Code,
		sOperation,
		sMessage,
		NULL
	);
}



/* 验证公开连接建立结果覆盖完整固定存储。 */
static bool __xrtWsServerResultValid(
	const xwsserverresult* pResult,
	cstr sOperation
)
{
	if ( xrtMemRangeValid(pResult, sizeof(*pResult)) ) {
		return true;
	}
	__xrtWsServerResultError(
		XERR_ARGUMENT,
		XWS_SERVER_RESULT_ERROR_ARGUMENT,
		sOperation,
		"WebSocket open result range is invalid"
	);
	return false;
}




/* 增加连接建立结果引用并返回原指针。 */
XWS_SERVER_API xwsserverresult* xrtWsServerResultRef(
	xwsserverresult* pResult
)
{
	if ( !__xrtWsServerResultValid(
		pResult,
		"retain-websocket-open-result"
	) ) {
		return NULL;
	}
	if ( xrtRefRetain(&pResult->References) < 0 ) {
		__xrtWsServerResultError(
			XERR_STATE,
			XWS_SERVER_RESULT_ERROR_STATE,
			"retain-websocket-open-result",
			"WebSocket open result cannot be retained"
		);
		return NULL;
	}
	return pResult;
}



/* 释放最后一个结果引用及其尚未取走的全部所有权。 */
XWS_SERVER_API void xrtWsServerResultDestroy(
	xwsserverresult* pResult
)
{
	xwsconn* pConnection;

	if ( pResult == NULL ) {
		return;
	}
	if ( !__xrtWsServerResultValid(
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
	__xwsServerOpenConnectionDestroy(pConnection);
	memset(pResult, 0, sizeof(*pResult));
	xrtFree(pResult);
}



/* 返回结果借用的 WebSocket Connection。 */
XWS_SERVER_API xwsconn* xrtWsServerResultConnection(
	const xwsserverresult* pResult
)
{
	if ( !__xrtWsServerResultValid(
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
XWS_SERVER_API xwsconn* xrtWsServerResultTakeConnection(
	xwsserverresult* pResult
)
{
	if ( !__xrtWsServerResultValid(
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





#endif
