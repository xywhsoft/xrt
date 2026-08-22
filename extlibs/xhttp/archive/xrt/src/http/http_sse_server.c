#include "../internal/xrt_http_body_stream.h"

#include <xrt/http_sse_server.h>



#if defined(XRT_FEATURE_HTTP_SSE_SERVER)

/* 直接把事件写入 Body Stream 已分配的节点载荷。 */
static bool __xrtHttpSseServerEventFill(
	void* pOutput,
	size_t iSize,
	ptr pData
)
{
	size_t iWritten;

	return xrtHttpSseEventWrite(
		(const xhttpsseevent*)pData,
		pOutput,
		iSize,
		&iWritten
	) && (iWritten == iSize);
}



/* 直接把注释写入 Body Stream 已分配的节点载荷。 */
static bool __xrtHttpSseServerCommentFill(
	void* pOutput,
	size_t iSize,
	ptr pData
)
{
	xstrview* pComment = (xstrview*)pData;
	size_t iWritten;

	return xrtHttpSseCommentWrite(
		*pComment,
		pOutput,
		iSize,
		&iWritten
	) && (iWritten == iSize);
}



/* 组合 Reply、规范 SSE Content-Type 与有界异步正文。 */
XRT_API xhttpreply* xrtHttpSseReplyCreate(
	const xhttpbodystreamconfig* pConfig,
	xhttpbodystream** ppStream
)
{
	xhttpbodystream* pStream = NULL;
	xhttpbodystream* pOutput = NULL;
	xhttpbody* pBody;
	xhttpreply* pReply;
	xhttpheaders* pHeaders;

	if ( !__xrtRangeValid(ppStream, sizeof(pOutput)) ||
		((pConfig != NULL) && !__xrtRangeValid(
			pConfig, sizeof(*pConfig)
		)) || ((pConfig != NULL) && __xrtRangesOverlap(
			ppStream,
			sizeof(pOutput),
			pConfig,
			sizeof(*pConfig)
		)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	memcpy(ppStream, &pOutput, sizeof(pOutput));
	pBody = xrtHttpBodyStreamCreate(pConfig, &pStream);
	if ( pBody == NULL ) {
		return NULL;
	}
	pReply = xrtHttpReplyCreate(200);
	if ( pReply == NULL ) {
		xrtHttpBodyStreamDestroy(pStream);
		xrtHttpBodyDestroy(pBody);
		return NULL;
	}
	pHeaders = xrtHttpReplyEditHeaders(pReply);
	if ( (pHeaders == NULL) ||
		!xrtHttpSseResponseHeaders(pHeaders) ||
		!xrtHttpReplySetBody(pReply, pBody) ) {
		xrtHttpReplyDestroy(pReply);
		xrtHttpBodyStreamDestroy(pStream);
		xrtHttpBodyDestroy(pBody);
		return NULL;
	}
	xrtHttpBodyDestroy(pBody);
	memcpy(ppStream, &pStream, sizeof(pStream));
	return pReply;
}



/* 封装最常用的只有 data 字段的默认事件。 */
XRT_API xhttpbodystreamresult xrtHttpSseSend(
	xhttpbodystream* pStream,
	xstrview Data
)
{
	xhttpsseevent Event;

	memset(&Event, 0, sizeof(Event));
	Event.Data = Data;
	Event.Flags = XHTTP_SSE_EVENT_DATA;
	return xrtHttpSseSendEvent(pStream, &Event);
}



/* 计量后在一个队列节点中完成事件编码和提交。 */
XRT_API xhttpbodystreamresult xrtHttpSseSendEvent(
	xhttpbodystream* pStream,
	const xhttpsseevent* pEvent
)
{
	size_t iSize;

	if ( !xrtHttpSseEventSize(pEvent, &iSize) ) {
		return XHTTP_BODY_STREAM_ERROR;
	}
	return __xrtHttpBodyStreamBuild(
		pStream,
		iSize,
		__xrtHttpSseServerEventFill,
		(ptr)pEvent
	);
}



/* 计量后在一个队列节点中完成注释编码和提交。 */
XRT_API xhttpbodystreamresult xrtHttpSseSendComment(
	xhttpbodystream* pStream,
	xstrview Comment
)
{
	size_t iSize;

	if ( !xrtHttpSseCommentSize(Comment, &iSize) ) {
		return XHTTP_BODY_STREAM_ERROR;
	}
	return __xrtHttpBodyStreamBuild(
		pStream,
		iSize,
		__xrtHttpSseServerCommentFill,
		&Comment
	);
}

#endif
