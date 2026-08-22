#ifndef XRT_INTERNAL_WEBSOCKET_H
#define XRT_INTERNAL_WEBSOCKET_H

#include "xrt_internal.h"

#include <xrt/websocket.h>

#if defined(XRT_FEATURE_WEBSOCKET_EXTENSION)
	#include "xrt_http.h"
#endif



#if defined(XRT_FEATURE_WEBSOCKET_FRAME)

/* 设置 WebSocket 帧模块共享的结构化错误。 */
static inline void __xrtWsFrameError(
	xerrkind Kind,
	xwsframeerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.websocket.frame";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}

#endif



#if defined(XRT_FEATURE_WEBSOCKET_CLOSE)

/* 设置 WebSocket Close 负载层的结构化错误。 */
static inline void __xrtWsCloseError(
	xerrkind Kind,
	xwscloseerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.websocket.close";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}

#endif



#if defined(XRT_FEATURE_WEBSOCKET_MESSAGE)

/* 设置 WebSocket 消息状态机的结构化错误。 */
static inline void __xrtWsMessageError(
	xerrkind Kind,
	xwsmessageerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.websocket.message";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}

#endif



#if defined(XRT_FEATURE_WEBSOCKET_DEFLATE)

/* 设置 permessage-deflate 协商层的结构化错误。 */
static inline void __xrtWsDeflateError(
	xerrkind Kind,
	xwsdeflateerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.websocket.deflate";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 把扩展或 HTTP 参数错误包装为 permessage-deflate 错误。 */
static inline void __xrtWsDeflateWrap(
	xerrkind DefaultKind,
	xwsdeflateerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ?
		xrtErrorKind(pCause) : DefaultKind;
	Desc.Domain = "xrt.websocket.deflate";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}

#endif



#if defined(XRT_FEATURE_WEBSOCKET_HANDSHAKE)

/* 设置 WebSocket 握手模块共享的结构化错误。 */
static inline void __xrtWsHandshakeError(
	xerrkind Kind,
	xwshandshakeerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.websocket.handshake";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 把当前底层错误包装为稳定的 WebSocket 握手错误。 */
static inline void __xrtWsHandshakeWrap(
	xerrkind DefaultKind,
	xwshandshakeerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ?
		xrtErrorKind(pCause) : DefaultKind;
	Desc.Domain = "xrt.websocket.handshake";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}

#endif

#endif
