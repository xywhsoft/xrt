#include "../internal/xrt_http.h"

#include <xrt/http_sse.h>



#if defined(XHTTP_FEATURE_HTTP_SSE_HTTP)

/* SSE 使用的字段名称和值集中在协议适配层，避免运行时重复拼写。 */
static const xstrview __xrtHttpSseAcceptName = XRT_STR_INIT("Accept");
static const xstrview __xrtHttpSseContentTypeName =
	XRT_STR_INIT("Content-Type");
static const xstrview __xrtHttpSseLastEventIdName =
	XRT_STR_INIT("Last-Event-ID");
static const xstrview __xrtHttpSseMediaType =
	XRT_STR_INIT(XHTTP_SSE_MEDIA_TYPE);



/* 完成副本事务，把目标原有状态转交给副本统一释放。 */
static bool __xrtHttpSseHeadersCommit(
	xhttpheaders* pHeaders,
	xhttpheaders* pWork
)
{
	(void)xrtHttpHeadersSwap(pHeaders, pWork);
	xrtHttpHeadersDestroy(pWork);
	return true;
}



/* 判断媒体类型本体并保留调用线程原有错误。 */
XRT_API bool xrtHttpSseContentTypeValid(xstrview ContentType)
{
	xmediatype Type;
	xerror* pSaved = xrtTakeError();
	bool bValid;

	bValid = xrtHttpMediaTypeParse(ContentType, &Type) &&
		xrtHttpMediaTypeEqual(
			&Type,
			XRT_STR_LITERAL("text"),
			XRT_STR_LITERAL("event-stream")
		);
	xrtClearError();
	if ( pSaved != NULL ) {
		__xrtErrorSetOwned(pSaved);
	}
	return bValid;
}



/* 在副本中完整修改请求字段，任一验证或分配失败都不触碰原容器。 */
XRT_API bool xrtHttpSseRequestHeaders(
	xhttpheaders* pHeaders,
	xstrview LastEventId
)
{
	xhttpheaders* pWork;

	if ( (pHeaders == NULL) ||
		!__xrtHttpViewValid(LastEventId) ||
		((LastEventId.Size != 0) &&
		 !xrtHttpSseLastEventIdValid(LastEventId)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pWork = xrtHttpHeadersClone(pHeaders);
	if ( pWork == NULL ) {
		return false;
	}
	if ( !xrtHttpHeadersSet(
		pWork, __xrtHttpSseAcceptName, __xrtHttpSseMediaType
	) ) {
		xrtHttpHeadersDestroy(pWork);
		return false;
	}
	if ( LastEventId.Size == 0 ) {
		(void)xrtHttpHeadersRemove(pWork, __xrtHttpSseLastEventIdName);
	} else if ( !xrtHttpHeadersSet(
		pWork, __xrtHttpSseLastEventIdName, LastEventId
	) ) {
		xrtHttpHeadersDestroy(pWork);
		return false;
	}
	return __xrtHttpSseHeadersCommit(pHeaders, pWork);
}



/* 在副本中唯一设置响应媒体类型，不隐式添加缓存或连接策略。 */
XRT_API bool xrtHttpSseResponseHeaders(xhttpheaders* pHeaders)
{
	xhttpheaders* pWork;

	if ( pHeaders == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pWork = xrtHttpHeadersClone(pHeaders);
	if ( pWork == NULL ) {
		return false;
	}
	if ( !xrtHttpHeadersSet(
		pWork, __xrtHttpSseContentTypeName, __xrtHttpSseMediaType
	) ) {
		xrtHttpHeadersDestroy(pWork);
		return false;
	}
	return __xrtHttpSseHeadersCommit(pHeaders, pWork);
}



/* 分类 EventSource 响应；协议拒绝不是库错误，只有无效调用返回 ERROR。 */
XRT_API xhttpsseresponse xrtHttpSseResponseCheck(
	uint16 iStatus,
	const xhttpheaders* pHeaders
)
{
	const xhttpfield* pContentType;

	if ( (iStatus < 100) || (iStatus > 999) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_SSE_RESPONSE_ERROR;
	}
	if ( iStatus == 204 ) {
		return XHTTP_SSE_RESPONSE_STOP;
	}
	if ( iStatus != 200 ) {
		return XHTTP_SSE_RESPONSE_REJECT;
	}
	if ( (pHeaders == NULL) ||
		(xrtHttpHeadersCountName(
			pHeaders, __xrtHttpSseContentTypeName
		) != 1u) ) {
		return XHTTP_SSE_RESPONSE_REJECT;
	}
	pContentType = xrtHttpHeadersGet(
		pHeaders, __xrtHttpSseContentTypeName
	);
	if ( (pContentType == NULL) ||
		!xrtHttpSseContentTypeValid(pContentType->Value) ) {
		return XHTTP_SSE_RESPONSE_REJECT;
	}
	return XHTTP_SSE_RESPONSE_OPEN;
}

#endif
