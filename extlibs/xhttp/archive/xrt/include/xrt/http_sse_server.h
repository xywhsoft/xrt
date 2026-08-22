#ifndef XRT_HTTP_SSE_SERVER_H
#define XRT_HTTP_SSE_SERVER_H

#include <xrt/http_body_stream.h>
#include <xrt/http_server.h>
#include <xrt/http_sse.h>



#if defined(XRT_FEATURE_HTTP_SSE_SERVER) && \
	(!defined(XRT_FEATURE_HTTP_SSE_HTTP) || \
	 !defined(XRT_FEATURE_HTTP_BODY_STREAM) || \
	 !defined(XRT_FEATURE_HTTP_SERVER_REPLY))
	#error "XRT HTTP SSE server requires SSE HTTP, body stream and server Reply support"
#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_HTTP_SSE_SERVER)

/*
	创建 200 SSE Reply 和独立生产端；成功后可继续修改 Reply 应用字段。
	配置会立即快照，输出句柄可以未对齐，但不得与非空配置重叠。
*/
XRT_API xhttpreply* xrtHttpSseReplyCreate(
	const xhttpbodystreamconfig* pConfig,
	xhttpbodystream** ppStream
);



/* 把 UTF-8 数据封装为一条默认 message 事件并原子提交。 */
XRT_API xhttpbodystreamresult xrtHttpSseSend(
	xhttpbodystream* pStream,
	xstrview Data
);



/* 单分配封装并提交完整事件；AGAIN 时不保留任何输入引用。 */
XRT_API xhttpbodystreamresult xrtHttpSseSendEvent(
	xhttpbodystream* pStream,
	const xhttpsseevent* pEvent
);



/* 单分配封装并提交一条或多条注释心跳。 */
XRT_API xhttpbodystreamresult xrtHttpSseSendComment(
	xhttpbodystream* pStream,
	xstrview Comment
);

#endif



XRT_EXTERN_C_END

#endif
