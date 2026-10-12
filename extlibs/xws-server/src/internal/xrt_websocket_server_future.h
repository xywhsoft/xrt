#ifndef XWS_SERVER_INTERNAL_FUTURE_H
#define XWS_SERVER_INTERNAL_FUTURE_H

#include "xrt_websocket_server.h"

#include <xrt/websocket_server_future.h>



/* 本库的握手结果用原子槽管理可取走的所有权。 */
#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_FUTURE)

struct xwsserverresult {
	volatile int32 References;
	xatomicptr Connection;
};



/* 中止并释放一个没有进入调用方所有权的 WebSocket Connection。 */
static inline void __xwsServerOpenConnectionDestroy(
	xwsconn* pConnection
)
{
	if ( pConnection == NULL ) {
		return;
	}
	(void)xrtWsConnAbort(pConnection);
	xrtWsConnDestroy(pConnection);
}



#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_FUTURE)

/* 创建 HTTP Future 桥接层自己的稳定握手错误。 */
static inline xerror* __xwsServerOpenErrorCreate(
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
