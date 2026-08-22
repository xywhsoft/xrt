#ifndef XRT_INTERNAL_WEBSOCKET_HTTP_FUTURE_H
#define XRT_INTERNAL_WEBSOCKET_HTTP_FUTURE_H

#include "xrt_websocket_http.h"

#include <xrt/websocket_http_future.h>



#if defined(XRT_FEATURE_WEBSOCKET_HTTP_FUTURE)

/* 统一结果用原子槽管理可取走所有权，服务端裁剪时不携带客户端响应槽。 */
struct xwsopenresult {
	volatile int32 References;
	xatomicptr Connection;
	#if defined(XRT_FEATURE_WEBSOCKET_CLIENT_FUTURE)
		xatomicptr Response;
	#endif
};



/* 中止并释放一个没有进入调用方所有权的 WebSocket Connection。 */
static inline void __xrtWsOpenConnectionDestroy(
	xwsconn* pConnection
)
{
	if ( pConnection == NULL ) {
		return;
	}
	(void)xrtWsConnAbort(pConnection);
	xrtWsConnDestroy(pConnection);
}



#if defined(XRT_FEATURE_WEBSOCKET_CLIENT_FUTURE) || \
	defined(XRT_FEATURE_WEBSOCKET_SERVER_FUTURE)

/* 创建 HTTP Future 桥接层自己的稳定握手错误。 */
static inline xerror* __xrtWsOpenErrorCreate(
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.websocket.handshake";
	Desc.Code = XWS_HANDSHAKE_ERROR_UPGRADE;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	return xrtErrorBuild(&Desc);
}

#endif

#endif

#endif
