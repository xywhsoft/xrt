/*
 * MIT License
 *
 * Copyright (c) 2025 xLeaves [xywhsoft] <xywhsoft@qq.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* 此文件由 tools/amalgamate.py 生成，请勿直接修改。 */
/* Supply XRT and selected extension dependencies before this header. */
#if !defined(XRT_CORE_H)
#error "xws_server requires XRT; include xrt.h or xrt_decl.h first"
#endif
#if defined(XWS_SERVER_IMPLEMENTATION) && defined(_MSC_VER) && \
	!defined(_CRT_SECURE_NO_WARNINGS)
	#define _CRT_SECURE_NO_WARNINGS
#endif
#if defined(XWS_SERVER_IMPLEMENTATION) && \
	!defined(_WIN32) && !defined(_WIN64)
	#if defined(__linux__) && !defined(_GNU_SOURCE)
		#define _GNU_SOURCE 1
	#endif
	#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
		#define _DARWIN_C_SOURCE 1
	#endif
	#if !defined(_POSIX_C_SOURCE)
		#define _POSIX_C_SOURCE 200809L
	#endif
	#if !defined(_FILE_OFFSET_BITS)
		#define _FILE_OFFSET_BITS 64
	#endif
#endif
#ifndef XWS_SERVER_SINGLE_HEADER_H
#define XWS_SERVER_SINGLE_HEADER_H
#define XWS_SERVER_SINGLE_HEADER 1


/* ========================================================================== */
/* feature selection: extlibs/xws-server/include/xws-server/features.h */
/* ========================================================================== */

/* 此文件由 tools/generate_extension_features.py 生成，请勿直接修改。 */
#ifndef XWS_SERVER_FEATURES_H
#define XWS_SERVER_FEATURES_H

/* websocket_server_deflate 及其直接依赖。 */
#if defined(XWS_SERVER_MODULE_ALL) || defined(XWS_SERVER_MODULE_WEBSOCKET_SERVER_DEFLATE)
#ifndef XWS_SERVER_FEATURE_WEBSOCKET_SERVER_DEFLATE
#define XWS_SERVER_FEATURE_WEBSOCKET_SERVER_DEFLATE
#endif
#ifndef XWS_SERVER_MODULE_WEBSOCKET_SERVER
#define XWS_SERVER_MODULE_WEBSOCKET_SERVER
#endif
#ifndef XWS_MODULE_WEBSOCKET_CONNECTION_DEFLATE
#define XWS_MODULE_WEBSOCKET_CONNECTION_DEFLATE
#endif
#ifndef XRT_MODULE_WEBSOCKET_DEFLATE
#define XRT_MODULE_WEBSOCKET_DEFLATE
#endif
#endif

/* websocket_server_future 及其直接依赖。 */
#if defined(XWS_SERVER_MODULE_ALL) || defined(XWS_SERVER_MODULE_WEBSOCKET_SERVER_FUTURE)
#ifndef XWS_SERVER_FEATURE_WEBSOCKET_SERVER_FUTURE
#define XWS_SERVER_FEATURE_WEBSOCKET_SERVER_FUTURE
#endif
#ifndef XWS_SERVER_MODULE_WEBSOCKET_SERVER
#define XWS_SERVER_MODULE_WEBSOCKET_SERVER
#endif
#ifndef XRT_MODULE_FUTURE_BRIDGE
#define XRT_MODULE_FUTURE_BRIDGE
#endif
#endif

/* websocket_server_tls 及其直接依赖。 */
#if defined(XWS_SERVER_MODULE_ALL) || defined(XWS_SERVER_MODULE_WEBSOCKET_SERVER_TLS)
#ifndef XWS_SERVER_FEATURE_WEBSOCKET_SERVER_TLS
#define XWS_SERVER_FEATURE_WEBSOCKET_SERVER_TLS
#endif
#ifndef XWS_SERVER_MODULE_WEBSOCKET_SERVER
#define XWS_SERVER_MODULE_WEBSOCKET_SERVER
#endif
#ifndef XWS_MODULE_WEBSOCKET_CONNECTION_TLS
#define XWS_MODULE_WEBSOCKET_CONNECTION_TLS
#endif
#ifndef XHTTP_SERVER_MODULE_HTTP_SERVER_TLS
#define XHTTP_SERVER_MODULE_HTTP_SERVER_TLS
#endif
#endif

/* websocket_server_router 及其直接依赖。 */
#if defined(XWS_SERVER_MODULE_ALL) || defined(XWS_SERVER_MODULE_WEBSOCKET_SERVER_ROUTER)
#ifndef XWS_SERVER_FEATURE_WEBSOCKET_SERVER_ROUTER
#define XWS_SERVER_FEATURE_WEBSOCKET_SERVER_ROUTER
#endif
#ifndef XWS_SERVER_MODULE_WEBSOCKET_SERVER
#define XWS_SERVER_MODULE_WEBSOCKET_SERVER
#endif
#ifndef XHTTP_SERVER_MODULE_HTTP_SERVER_ROUTER
#define XHTTP_SERVER_MODULE_HTTP_SERVER_ROUTER
#endif
#ifndef XHTTP_MODULE_HTTP_ORIGIN
#define XHTTP_MODULE_HTTP_ORIGIN
#endif
#endif

/* websocket_server 及其直接依赖。 */
#if defined(XWS_SERVER_MODULE_ALL) || defined(XWS_SERVER_MODULE_WEBSOCKET_SERVER)
#ifndef XWS_SERVER_FEATURE_WEBSOCKET_SERVER
#define XWS_SERVER_FEATURE_WEBSOCKET_SERVER
#endif
#ifndef XRT_MODULE_WEBSOCKET_HANDSHAKE
#define XRT_MODULE_WEBSOCKET_HANDSHAKE
#endif
#ifndef XWS_MODULE_WEBSOCKET_CONNECTION
#define XWS_MODULE_WEBSOCKET_CONNECTION
#endif
#ifndef XRT_MODULE_HTTP_UPGRADE
#define XRT_MODULE_HTTP_UPGRADE
#endif
#ifndef XHTTP_SERVER_MODULE_HTTP_SERVER_UPGRADE
#define XHTTP_SERVER_MODULE_HTTP_SERVER_UPGRADE
#endif
#endif

#endif /* XWS_SERVER_FEATURES_H */


/* ========================================================================== */
/* public: extlibs/xws-server/include/xws-server/export.h */
/* ========================================================================== */

#ifndef XWS_SERVER_EXPORT_H
#define XWS_SERVER_EXPORT_H


/* 独立动态库只导出本库接口，依赖由宿主共同链接。 */
#if defined(_WIN32) || defined(_WIN64)
 #if defined(XWS_SERVER_BUILD_SHARED)
  #define XWS_SERVER_API __declspec(dllexport)
 #elif defined(XWS_SERVER_USE_SHARED)
  #define XWS_SERVER_API __declspec(dllimport)
 #else
  #define XWS_SERVER_API
 #endif
#elif defined(__GNUC__) || defined(__clang__)
 #if defined(XWS_SERVER_BUILD_SHARED)
  #define XWS_SERVER_API __attribute__((visibility("default")))
 #else
  #define XWS_SERVER_API
 #endif
#else
 #define XWS_SERVER_API
#endif

#endif


/* ========================================================================== */
/* public: extlibs/xws-server/include/xrt/websocket_server.h */
/* ========================================================================== */

#ifndef XWS_SERVER_H_API
#define XWS_SERVER_H_API


#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER)
#endif




#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER) && \
	(!defined(XRT_FEATURE_WEBSOCKET_HANDSHAKE) || \
	 !defined(XWS_FEATURE_WEBSOCKET_CONNECTION) || \
	 !defined(XHTTP_FEATURE_HTTP_SERVER_UPGRADE))
	#error "XRT WebSocket server requires handshake, connection and HTTP Upgrade"
#endif


#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_TLS) && \
	(!defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER) || \
	 !defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS) || \
	 !defined(XHTTP_FEATURE_HTTP_SERVER_TLS))
	#error "XRT WebSocket TLS server requires server, TLS connection and HTTPS server"
#endif


#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_DEFLATE) && \
	(!defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER) || \
	 !defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE) || \
	 !defined(XRT_FEATURE_WEBSOCKET_DEFLATE))
	#error "XRT WebSocket Deflate server requires server, compressed connection and negotiation"
#endif




/*
	服务端压缩策略返回 true 表示接受并写出 Response。
	返回 false 且未设置线程错误表示主动放弃，带错误则终止握手。
	返回 true 时回调内部已恢复的错误会被丢弃，进入回调前的线程错误保持不变。
*/
typedef bool (*xwsdeflateacceptproc)(
	const xwsdeflate* pOffer,
	xwsdeflate* pResponse,
	ptr pData
);



#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER)

/*
	Accept 是末尾补零的响应值。
	Protocol 借用请求字段；空视图表示没有协商子协议。
*/
typedef struct xwsserverhandshake {
	char Accept[XWS_ACCEPT_CAPACITY];
	xstrview Protocol;
	xwsdeflate Deflate;
	char Extensions[XWS_DEFLATE_MAX_SIZE + 1u];
	bool DeflateEnabled;

} xwsserverhandshake;



/*
	Protocols 是按服务端支持顺序书写的候选列表，但选择保持客户端偏好顺序。
	Upgrade 调用返回后不再借用配置中的任何视图。
*/
typedef struct xwsserverconfig {
	xwsconnconfig Connection;
	xstrview Protocols;
	xwsdeflateacceptproc AcceptDeflate;
	ptr DeflateData;
	bool EnableDeflate;
	bool RequireDeflate;

} xwsserverconfig;



/*
	异步 Upgrade 终态只发布一次。
	成功时 Connection 调用方引用转移给回调；Error 只在回调期间借用。
*/
typedef void (*xwsupgradeproc)(
	xhttpconn* pHttp,
	xnetresult Result,
	xwsconn* pConnection,
	const xerror* pError,
	ptr pData
);

#endif






XRT_EXTERN_C_BEGIN



#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER)

/* 在完整、允许未对齐的存储中初始化服务端默认配置。 */
XWS_SERVER_API void xrtWsServerConfigInit(
	xwsserverconfig* pConfig
);



/* 从允许未对齐的完整快照无分配验证服务端配置。 */
XWS_SERVER_API bool xrtWsServerConfigValid(
	const xwsserverconfig* pConfig
);



/*
	严格验证一个完整 HTTP/1.1 WebSocket 请求并计算响应。
	配置和输出允许未对齐，输出不得覆盖请求或配置；
	重复子协议字段按一份合并列表验证，失败不修改 Handshake。
*/
XWS_SERVER_API bool xrtWsServerCheck(
	const xhttpserverrequest* pRequest,
	const xwsserverconfig* pConfig,
	xwsserverhandshake* pHandshake
);



/* 从完整、允许未对齐的已验证握手快照创建拥有型 101 Reply。 */
XWS_SERVER_API xhttpreply* xrtWsServerReply(
	const xwsserverhandshake* pHandshake
);



/*
	把 WebSocket 握手错误映射为无正文 400、405、426 或 500 最终响应。
	Error 为空或不属于握手域时按服务端内部错误处理。
*/
XWS_SERVER_API xnetresult xrtWsServerReject(
	xhttpconn* pHttp,
	const xerror* pError
);



/*
	提交已经通过 xrtWsServerCheck 的 WebSocket 握手。
	Headers 可以为空；非空时只追加普通响应字段，Upgrade、Connection、
	Sec-WebSocket-* 以及正文分帧字段由协议层独占，不能覆盖。
	Handshake 和 Headers 只在调用期间借用，配置、事件和子协议会在返回前快照。
	该入口适合鉴权后追加 Cookie、追踪标识或应用自定义 Header；同步失败不调用 Proc。
*/
XWS_SERVER_API xnetresult xrtWsUpgradeAccept(
	xhttpconn* pHttp,
	const xwsserverconfig* pConfig,
	const xwsserverhandshake* pHandshake,
	const xhttpheaders* pHeaders,
	const xwsconnevents* pEvents,
	ptr pEventData,
	xwsupgradeproc pProc,
	ptr pData
);



/*
	验证当前 HTTP 请求、提交 101 并接管为 WebSocket Connection。
	配置和事件表在分配前快照；调用前可以直接读取请求完成鉴权；
	EventData 只交给 Connection 事件，Data 只交给完成回调；
	同步失败不会调用 Proc。
*/
XWS_SERVER_API xnetresult xrtWsUpgrade(
	xhttpconn* pHttp,
	const xwsserverconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pEventData,
	xwsupgradeproc pProc,
	ptr pData
);

#endif






XRT_EXTERN_C_END

#endif


/* ========================================================================== */
/* public: extlibs/xws-server/include/xrt/websocket_server_router.h */
/* ========================================================================== */

#ifndef XRT_WEBSOCKET_SERVER_ROUTER_H
#define XRT_WEBSOCKET_SERVER_ROUTER_H




#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_ROUTER) && \
	(!defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER) || \
	 !defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER) || \
	 !defined(XHTTP_FEATURE_HTTP_ORIGIN))
	#error "XRT WebSocket server router requires WebSocket server, HTTP server router and HTTP Origin"
#endif



#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_ROUTER)

/* WebSocket 服务端路由错误区分参数、配置、分配、注册、响应和运行时状态。 */
typedef enum xwsserverroutererror {
	XWS_SERVER_ROUTER_ERROR_ARGUMENT = 1,
	XWS_SERVER_ROUTER_ERROR_CONFIG,
	XWS_SERVER_ROUTER_ERROR_MEMORY,
	XWS_SERVER_ROUTER_ERROR_ROUTE,
	XWS_SERVER_ROUTER_ERROR_AUTHORIZATION,
	XWS_SERVER_ROUTER_ERROR_RESPONSE,
	XWS_SERVER_ROUTER_ERROR_STATE
} xwsserverroutererror;



/*
	固定路由默认允许原生客户端省略 Origin，但浏览器提供时必须与请求同源。
	ANY 只适合已经在外层完成来源校验的端点。
*/
typedef enum xwsserveroriginpolicy {
	XWS_SERVER_ORIGIN_SAME_HOST_OR_ABSENT = 0,
	XWS_SERVER_ORIGIN_SAME_HOST,
	XWS_SERVER_ORIGIN_ANY
} xwsserveroriginpolicy;



/* Upgrade 成功后借用 Connection；需要跨回调保存时由用户显式增加引用。 */
typedef void (*xwsserverrouteopenproc)(
	xhttpconn* pHttp,
	xwsconn* pConnection,
	ptr pData
);



/*
	Origin 策略通过后调用业务授权；返回 false 由路由器统一回复 403。
	请求、路由参数和握手只在回调期间借用，回调不应修改线程错误。
*/
typedef bool (*xwsserverrouteauthorizeproc)(
	xhttpconn* pHttp,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	const xwsserverhandshake* pHandshake,
	ptr pData
);



/* 同步握手或异步 Upgrade 失败时借用稳定错误；该回调只用于观察。 */
typedef void (*xwsserverrouteerrorproc)(
	xhttpconn* pHttp,
	const xerror* pError,
	ptr pData
);



/* Router 和全部已升级连接释放后，对成功注册的 Data 调用一次。 */
typedef void (*xwsserverrouterreleaseproc)(ptr pData);



/* 固定路由复制服务端配置与事件，并为常见 WebSocket 端点托管生命周期。 */
typedef struct xwsserverrouteconfig {
	xwsserverconfig Server;
	xwsconnevents Events;
	xwsserveroriginpolicy Origin;
	xwsserverrouteauthorizeproc Authorize;
	xwsserverrouteopenproc Open;
	xwsserverrouteerrorproc Error;
	xwsserverrouterreleaseproc Release;
	ptr Data;
} xwsserverrouteconfig;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_ROUTER)

/*
	初始化默认 WebSocket 服务端配置、空连接事件和空生命周期回调。
	输出允许未对齐，但必须是完整且不回绕的固定结构地址范围。
*/
XWS_SERVER_API void xrtWsServerRouteConfigInit(
	xwsserverrouteconfig* pConfig
);



/*
	注册固定 WebSocket 路径；内部处理方法、Origin、授权、握手和 Upgrade。
	成功后 Router 接管一次 Release(Data)，失败时 Data 所有权保持不变。
	Pattern 和配置内借用视图必须完整且不回绕；固定配置允许未对齐。
	函数在注册前只读取配置一次，并深复制子协议列表；后续修改调用方配置
	不会改变已注册端点。
*/
XWS_SERVER_API bool xrtWsServerRoute(
	xhttpserverrouter* pRouter,
	xstrview Pattern,
	const xwsserverrouteconfig* pConfig
);

#endif



XRT_EXTERN_C_END

#endif


/* ========================================================================== */
/* public: extlibs/xws-server/include/xrt/websocket_server_future.h */
/* ========================================================================== */

#ifndef XWS_SERVER_FUTURE_H_API
#define XWS_SERVER_FUTURE_H_API


#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_FUTURE)
#endif





#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_FUTURE) && \
	(!defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER) || \
	 !defined(XRT_FEATURE_FUTURE) || \
	 !defined(XRT_FEATURE_FUTURE_BRIDGE))
	#error "XRT WebSocket server Future requires server, result and Future bridge support"
#endif




#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_FUTURE)

/*
	连接建立结果拥有 WebSocket Connection。
	客户端 Future 启用时，成功结果还拥有对应的 101 Response。
*/
typedef struct xwsserverresult xwsserverresult;



/* 连接建立结果错误码只描述结果对象本身，不依赖 HTTP 握手实现。 */
typedef enum xwsserverresulterror {
	XWS_SERVER_RESULT_ERROR_ARGUMENT = 1,
	XWS_SERVER_RESULT_ERROR_STATE
} xwsserverresulterror;



XRT_EXTERN_C_BEGIN



/*
	增加连接建立结果引用并返回原指针。
	空指针、无效对象范围或不可再增加的引用状态返回 NULL 并记录错误。
*/
XWS_SERVER_API xwsserverresult* xrtWsServerResultRef(
	xwsserverresult* pResult
);



/*
	释放结果引用。
	最后一个引用会中止尚未取走的 Connection，并销毁尚未取走的 Response。
	NULL 是空操作；非空但无效的对象范围会记录参数错误。
*/
XWS_SERVER_API void xrtWsServerResultDestroy(
	xwsserverresult* pResult
);



/*
	返回结果借用的 Connection；结果有效且 Connection 未被取走期间可用。
	空指针或无效对象范围返回 NULL 并记录参数错误。
*/
XWS_SERVER_API xwsconn* xrtWsServerResultConnection(
	const xwsserverresult* pResult
);



/*
	原子取走 Connection 所有权。
	调用方必须在使用结束后关闭或中止 Connection，并释放其调用方引用。
	同一结果的 Take 与借用访问不可并发。
	空指针或无效对象范围返回 NULL 并记录参数错误。
*/
XWS_SERVER_API xwsconn* xrtWsServerResultTakeConnection(
	xwsserverresult* pResult
);






#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_FUTURE)

/*
	验证当前请求、提交 101，并以 Future 交付拥有型连接建立结果。
	取消 Future 会异常关闭当前 HTTP Connection；此函数只应在 Request Worker 中调用。
	HTTP Connection、Config 和 Events 在分配桥接状态前验证完整对象范围。
	Connection 事件使用独立的 Data，不会看到 Future 内部桥接上下文。
*/
XWS_SERVER_API xfuture* xrtWsUpgradeAsync(
	xhttpconn* pHttp,
	const xwsserverconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pData
);

#endif



XRT_EXTERN_C_END

#endif


#endif


/* ========================================================================== */
/* public: extlibs/xws-server/include/xws-server.h */
/* ========================================================================== */

#ifndef XWS_SERVER_H
#define XWS_SERVER_H


#endif

#endif

#if defined(XWS_SERVER_IMPLEMENTATION) && !defined(XWS_SERVER_IMPLEMENTATION_ONCE)
#define XWS_SERVER_IMPLEMENTATION_ONCE 1


/* ========================================================================== */
/* internal: extlibs/xws-server/src/internal/xrt_websocket_server.h */
/* ========================================================================== */

#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER)
#ifndef XWS_SERVER_INTERNAL_HTTP_H
#define XWS_SERVER_INTERNAL_HTTP_H

#include <string.h>

/* 构建并安装扩展库自己的结构化错误。 */
static inline void __xwsServerErrorSetDetail(
	xerrkind Kind,
	cstr sDomain,
	int32 iCode,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = sDomain;
	Desc.Code = iCode;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 取走当前原因链，并用扩展库稳定域包装后重新安装。 */
static inline void __xwsServerErrorWrapDetail(
	xerrkind DefaultKind,
	cstr sDomain,
	int32 iCode,
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
	Desc.Domain = sDomain;
	Desc.Code = iCode;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}



#if defined(XRT_FEATURE_WEBSOCKET_HANDSHAKE)

/* 设置 WebSocket 握手层的结构化错误。 */
static inline void __xwsServerHandshakeError(
	xerrkind Kind,
	xwshandshakeerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xwsServerErrorSetDetail(
		Kind,
		"xrt.websocket.handshake",
		(int32)Code,
		sOperation,
		sMessage,
		NULL
	);
}



/* 将当前底层错误包装为稳定的 WebSocket 握手错误。 */
static inline void __xwsServerHandshakeWrap(
	xerrkind DefaultKind,
	xwshandshakeerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xwsServerErrorWrapDetail(
		DefaultKind,
		"xrt.websocket.handshake",
		(int32)Code,
		sOperation,
		sMessage
	);
}

#endif






#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER)

typedef struct __xws_server_fields {
	const xhttpfield* Data;
	size_t Count;
} __xws_server_fields;



/* 验证 HTTP 适配层公开入口使用的完整对象范围。 */
static inline bool __xwsServerHttpRangeCheck(
	const void* pObject,
	size_t iSize,
	cstr sOperation,
	cstr sMessage
)
{
	if ( xrtMemRangeValid(pObject, iSize) ) {
		return true;
	}
	__xwsServerHandshakeError(
		XERR_ARGUMENT,
		XWS_HANDSHAKE_ERROR_ARGUMENT,
		sOperation,
		sMessage
	);
	return false;
}



/* 验证不透明 HTTP 对象存在，不依赖扩展库之外的对象布局。 */
static inline bool __xwsServerHttpObjectCheck(
	const void* pObject,
	cstr sOperation,
	cstr sMessage
)
{
	if ( xrtMemRangeValid(pObject, sizeof(ptr)) &&
		(((uintptr_t)pObject & (sizeof(ptr) - 1u)) == 0u) ) {
		return true;
	}
	__xwsServerHandshakeError(
		XERR_ARGUMENT,
		XWS_HANDSHAKE_ERROR_ARGUMENT,
		sOperation,
		sMessage
	);
	return false;
}



/* 把可选事件表复制到对齐的独立快照。 */
static inline bool __xwsServerConnEventsSnapshot(
	xwsconnevents* pOutput,
	const xwsconnevents* pInput,
	cstr sOperation
)
{
	memset(pOutput, 0, sizeof(*pOutput));
	if ( pInput == NULL ) {
		return true;
	}
	if ( !__xwsServerHttpRangeCheck(
		pInput,
		sizeof(*pInput),
		sOperation,
		"WebSocket connection event range is invalid"
	) ) {
		return false;
	}
	memcpy(pOutput, pInput, sizeof(*pOutput));
	return true;
}



#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER)

/* 把可选服务端配置复制到对齐的默认值快照。 */
static inline bool __xwsServerServerConfigSnapshot(
	xwsserverconfig* pOutput,
	const xwsserverconfig* pInput,
	cstr sOperation
)
{
	xrtWsServerConfigInit(pOutput);
	if ( pInput == NULL ) {
		return true;
	}
	if ( !__xwsServerHttpRangeCheck(
		pInput,
		sizeof(*pInput),
		sOperation,
		"WebSocket server configuration range is invalid"
	) ) {
		return false;
	}
	memcpy(pOutput, pInput, sizeof(*pOutput));
	#if !defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_DEFLATE)
		if (pOutput->EnableDeflate || pOutput->RequireDeflate) {
			__xwsServerHandshakeError(XERR_VALUE, XWS_HANDSHAKE_ERROR_EXTENSION, sOperation, "WebSocket compression support is not selected");
			return false;
		}
	#endif
	return true;
}

#endif



/* 按大小写敏感规则比较两个协议文本视图。 */
static inline bool __xwsServerHttpTextEqual(
	xstrview Left,
	xstrview Right
)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 统计同名字段，不借出容器内部结构。 */
static inline size_t __xwsServerHttpFieldCount(
	const __xws_server_fields* pFields,
	xstrview Name
)
{
	return xrtHttpFieldCount(
		pFields->Data,
		pFields->Count,
		Name
	);
}



/* 返回唯一同名字段；缺失或重复都返回空指针。 */
static inline const xhttpfield* __xwsServerHttpFieldUnique(
	const __xws_server_fields* pFields,
	xstrview Name
)
{
	const xhttpfield* pFound = NULL;

	if ( xrtHttpFieldGetUnique(
		pFields->Data,
		pFields->Count,
		Name,
		&pFound
	) != XHTTP_NEXT_ITEM ) {
		return NULL;
	}
	return pFound;
}



/* 验证全部同名 token-list 字段并查找目标 token。 */
static inline bool __xwsServerHttpTokenHas(
	const __xws_server_fields* pFields,
	xstrview Name,
	xstrview Token,
	bool* pFound
)
{
	bool bPresent = false;
	bool bFound = false;

	for ( size_t i = 0; i < pFields->Count; i++ ) {
		const xhttpfield* pField = &pFields->Data[i];
		size_t iCount;

		if ( !xrtHttpFieldNameEqual(pField->Name, Name) ) {
			continue;
		}
		bPresent = true;
		if ( !xrtHttpTokenListCount(
			pField->Value,
			&iCount
		) || (iCount == 0) ) {
			return false;
		}
		if ( xrtHttpTokenListHas(
			pField->Value,
			Token
		) ) {
			bFound = true;
		}
	}
	*pFound = bPresent && bFound;
	return true;
}



/* 验证唯一字段只包含指定 token。 */
static inline bool __xwsServerHttpTokenExact(
	const __xws_server_fields* pFields,
	xstrview Name,
	xstrview Token
)
{
	const xhttpfield* pField =
		__xwsServerHttpFieldUnique(pFields, Name);
	size_t iCount;

	return (pField != NULL) &&
		xrtHttpTokenListCount(pField->Value, &iCount) &&
		(iCount == 1) &&
		xrtHttpTokenListHas(pField->Value, Token);
}



/* 验证 Upgrade 列表完整合法且至少包含一个无版本 websocket。 */
static inline bool __xwsServerHttpUpgradeHas(
	const __xws_server_fields* pFields
)
{
	xhttpupgradefieldcursor Cursor;
	xhttpupgradeitem Upgrade;
	xhttpnext Next;
	bool bFound = false;

	xrtHttpUpgradeFieldCursorInit(&Cursor);
	while ( (Next = xrtHttpUpgradeFieldNext(
		pFields->Data,
		pFields->Count,
		&Cursor,
		&Upgrade
	)) == XHTTP_NEXT_ITEM ) {
		if ( (Upgrade.Version.Size == 0) &&
			xrtHttpTokenEqual(
				Upgrade.Protocol,
				XRT_STR_LITERAL("websocket")
			) ) {
			bFound = true;
		}
	}
	return (Next == XHTTP_NEXT_END) && bFound;
}



/* 验证响应只选择唯一一个无版本 websocket 升级协议。 */



#if defined(XRT_FEATURE_WEBSOCKET_EXTENSION)

typedef bool (*__xws_server_extension_visit)(
	const xwsextension* pExtension,
	ptr pData
);



/* 把重复的扩展字段视为一份列表，并依次访问每个严格解析的扩展项。 */
static inline bool __xwsServerHttpExtensionsVisit(
	const __xws_server_fields* pFields,
	__xws_server_extension_visit pVisit,
	ptr pData
)
{
	for ( size_t i = 0; i < pFields->Count; i++ ) {
		const xhttpfield* pField = &pFields->Data[i];
		size_t iOffset = 0;

		if ( !xrtHttpFieldNameEqual(
				pField->Name,
				XRT_STR_LITERAL(
					"Sec-WebSocket-Extensions"
				)
			) ) {
			continue;
		}
		for ( ;; ) {
			xwsextension Extension;
			xhttpnext Next = xrtWsExtensionNext(
				pField->Value,
				&iOffset,
				&Extension
			);

			if ( Next == XHTTP_NEXT_ERROR ) {
				return false;
			}
			if ( Next == XHTTP_NEXT_END ) {
				break;
			}
			if ( !pVisit(&Extension, pData) ) {
				return false;
			}
		}
	}
	return true;
}

#endif

#endif

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xws-server/src/internal/xrt_websocket_server_router.h */
/* ========================================================================== */

#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_ROUTER)
#ifndef XRT_INTERNAL_WEBSOCKET_SERVER_ROUTER_H
#define XRT_INTERNAL_WEBSOCKET_SERVER_ROUTER_H





#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_ROUTER)

/* 路由快照由 Router 和全部已升级连接共同引用。 */
typedef struct xrt_ws_server_route {
	volatile int32 References;
	xwsserverrouteconfig Config;
	char Storage[];
} xrt_ws_server_route;



/* 每次 Upgrade 独立持有路由，并在异步完成与连接 Close 之间共享。 */
typedef struct xrt_ws_server_route_connection {
	volatile int32 References;
	xrt_ws_server_route* Route;
} xrt_ws_server_route_connection;



/* 所有 WebSocket 事件先恢复用户 Data，再转发给固定路由事件表。 */
extern const xwsconnevents __xrtWsServerRouteEvents;



/* 创建持有路由的单次 Upgrade 上下文。 */
xrt_ws_server_route_connection* __xrtWsServerRouteConnectionCreate(
	xrt_ws_server_route* pRoute
);



/* 释放 Upgrade 或 Connection 对单次上下文的一个引用。 */
void __xrtWsServerRouteConnectionRelease(
	xrt_ws_server_route_connection* pConnection
);



/* 释放 Router 或活动连接对固定路由快照的一个引用。 */
void __xrtWsServerRouteRelease(ptr pData);



/* 把底层 Upgrade 终态转为借用 Open 或 Error 回调。 */
void __xrtWsServerRouteUpgradeDone(
	xhttpconn* pHttp,
	xnetresult Result,
	xwsconn* pConnection,
	const xerror* pError,
	ptr pData
);

#endif

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xws-server/src/internal/xrt_websocket_server_future.h */
/* ========================================================================== */

#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_FUTURE)
#ifndef XWS_SERVER_INTERNAL_FUTURE_H
#define XWS_SERVER_INTERNAL_FUTURE_H





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
#endif


/* ========================================================================== */
/* source: extlibs/xws-server/src/websocket/server.c */
/* ========================================================================== */

#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER)



#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER)

typedef struct __xrt_ws_upgrade {
	xwsserverconfig Config;
	xwsconnevents Events;
	xwsupgradeproc Proc;
	ptr EventData;
	ptr Data;
	xstrview Protocol;
	char Storage[];
} __xrt_ws_upgrade;



/* 比较大小写敏感的子协议 token。 */
static bool __xrtWsServerProtocolEqual(
	xstrview Left,
	xstrview Right
)
{
	return __xwsServerHttpTextEqual(Left, Right);
}



/* 判断当前 token 是否已在更早字段或当前字段前缀出现。 */
static bool __xrtWsServerProtocolSeen(
	const xhttpserverrequest* pRequest,
	size_t iField,
	size_t iOrdinal,
	xstrview Protocol
)
{
	for ( size_t i = 0; i <= iField; i++ ) {
		const xhttpfield* pField =
			xrtHttpServerRequestHeaderAt(pRequest, i);
		size_t iOffset = 0;
		size_t iLimit = i == iField ?
			iOrdinal : SIZE_MAX;
		size_t iCurrent = 0;

		if ( (pField == NULL) ||
			!xrtHttpFieldNameEqual(
				pField->Name,
				XRT_STR_LITERAL(
					"Sec-WebSocket-Protocol"
				)
			) ) {
			continue;
		}
		while ( iCurrent < iLimit ) {
			xstrview Previous;
			xhttpnext Next = xrtWsProtocolNext(
				pField->Value,
				&iOffset,
				&Previous
			);

			if ( Next != XHTTP_NEXT_ITEM ) {
				break;
			}
			if ( __xrtWsServerProtocolEqual(
				Previous,
				Protocol
			) ) {
				return true;
			}
			iCurrent++;
		}
	}
	return false;
}



/* 验证重复子协议字段并按客户端出现顺序选择交集。 */
static bool __xrtWsServerProtocolSelect(
	const xhttpserverrequest* pRequest,
	xstrview Protocols,
	xstrview* pSelected
)
{
	size_t iCount = xrtHttpServerRequestHeaderCount(
		pRequest
	);
	xstrview Selected = { 0 };

	if ( !xrtWsProtocolsValid(Protocols) ) {
		__xwsServerHandshakeError(
			XERR_VALUE,
			XWS_HANDSHAKE_ERROR_PROTOCOL,
			"check-websocket-request",
			"WebSocket server protocol list is invalid"
		);
		return false;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		const xhttpfield* pField =
			xrtHttpServerRequestHeaderAt(pRequest, i);
		size_t iOffset = 0;
		size_t iOrdinal = 0;

		if ( (pField == NULL) ||
			!xrtHttpFieldNameEqual(
				pField->Name,
				XRT_STR_LITERAL(
					"Sec-WebSocket-Protocol"
				)
			) ) {
			continue;
		}
		if ( !xrtWsProtocolsValid(pField->Value) ) {
			__xwsServerHandshakeError(
				XERR_PROTOCOL,
				XWS_HANDSHAKE_ERROR_PROTOCOL,
				"check-websocket-request",
				"WebSocket request protocol field is invalid"
			);
			return false;
		}
		for ( ;; ) {
			xstrview Offered;
			xhttpnext Next = xrtWsProtocolNext(
				pField->Value,
				&iOffset,
				&Offered
			);

			if ( Next == XHTTP_NEXT_END ) {
				break;
			}
			if ( Next != XHTTP_NEXT_ITEM ) {
				return false;
			}
			if ( __xrtWsServerProtocolSeen(
				pRequest,
				i,
				iOrdinal,
				Offered
			) ) {
				__xwsServerHandshakeError(
					XERR_PROTOCOL,
					XWS_HANDSHAKE_ERROR_PROTOCOL,
					"check-websocket-request",
					"WebSocket request repeats a protocol"
				);
				return false;
			}
			if ( (Selected.Size == 0) &&
				xrtWsProtocolsHas(
					Protocols,
					Offered
				) ) {
				Selected = Offered;
			}
			iOrdinal++;
		}
	}
	*pSelected = Selected;
	return true;
}




#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_DEFLATE)

typedef struct __xrt_ws_server_deflate {
	xwsdeflate Offer;
	bool Found;
} __xrt_ws_server_deflate;



/* 找出唯一的 permessage-deflate offer；未知扩展由服务端主动忽略。 */
static bool __xrtWsServerDeflateVisit(
	const xwsextension* pExtension,
	ptr pData
)
{
	__xrt_ws_server_deflate* pDeflate =
		(__xrt_ws_server_deflate*)pData;

	if ( !xrtWsDeflateIs(pExtension) ) {
		return true;
	}
	if ( pDeflate->Found ) {
		__xwsServerHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_EXTENSION,
			"check-websocket-request",
			"WebSocket request repeats permessage-deflate"
		);
		return false;
	}
	if ( !xrtWsDeflateOfferParse(
		pExtension,
		&pDeflate->Offer
	) ) {
		__xwsServerHandshakeWrap(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_EXTENSION,
			"check-websocket-request",
			"WebSocket permessage-deflate offer is invalid"
		);
		return false;
	}
	pDeflate->Found = true;
	return true;
}



/* 运行可选策略，并区分无错误的主动放弃和带原因的策略失败。 */
static bool __xrtWsServerDeflateAccept(
	const xwsserverconfig* pConfig,
	const xwsdeflate* pOffer,
	xwsdeflate* pResponse,
	bool* pAccepted
)
{
	xerror* pPrevious;
	xerror* pCallbackError;
	bool bAccepted;

	if ( !xrtWsDeflateAccept(pOffer, pResponse) ) {
		__xwsServerHandshakeWrap(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_EXTENSION,
			"check-websocket-request",
			"WebSocket permessage-deflate offer cannot be accepted"
		);
		return false;
	}
	if ( pConfig->AcceptDeflate == NULL ) {
		*pAccepted = true;
		return true;
	}

	pPrevious = xrtTakeError();
	bAccepted = pConfig->AcceptDeflate(
		pOffer,
		pResponse,
		pConfig->DeflateData
	);
	pCallbackError = xrtTakeError();
	if ( !bAccepted && (pCallbackError != NULL) ) {
		xrtSetError(pCallbackError);
		__xwsServerHandshakeWrap(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_EXTENSION,
			"check-websocket-request",
			"WebSocket permessage-deflate policy failed"
		);
		xrtErrorFree(pCallbackError);
		xrtErrorFree(pPrevious);
		return false;
	}
	xrtErrorFree(pCallbackError);
	if ( pPrevious != NULL ) {
		xrtSetError(pPrevious);
	}
	xrtErrorFree(pPrevious);
	*pAccepted = bAccepted;
	return true;
}



/* 协商服务端压缩响应，并把规范字段值保存在握手快照中。 */
static bool __xrtWsServerDeflateSelect(
	const __xws_server_fields* pFields,
	const xwsserverconfig* pConfig,
	xwsserverhandshake* pHandshake
)
{
	__xrt_ws_server_deflate Deflate;
	xwsdeflate Response;
	size_t iSize = 0;
	bool bAccepted;

	if ( !pConfig->EnableDeflate ) {
		if ( pConfig->RequireDeflate ) {
			__xwsServerHandshakeError(
				XERR_VALUE,
				XWS_HANDSHAKE_ERROR_EXTENSION,
				"check-websocket-request",
				"Required WebSocket compression is not enabled"
			);
			return false;
		}
		return true;
	}
	memset(&Deflate, 0, sizeof(Deflate));
	if ( !__xwsServerHttpExtensionsVisit(
		pFields,
		__xrtWsServerDeflateVisit,
		&Deflate
	) ) {
		return false;
	}
	if ( !Deflate.Found ) {
		if ( pConfig->RequireDeflate ) {
			__xwsServerHandshakeError(
				XERR_PROTOCOL,
				XWS_HANDSHAKE_ERROR_EXTENSION,
				"check-websocket-request",
				"WebSocket request did not offer required compression"
			);
			return false;
		}
		return true;
	}
	if ( !__xrtWsServerDeflateAccept(
		pConfig,
		&Deflate.Offer,
		&Response,
		&bAccepted
	) ) {
		return false;
	}
	if ( !bAccepted ) {
		if ( pConfig->RequireDeflate ) {
			__xwsServerHandshakeError(
				XERR_PROTOCOL,
				XWS_HANDSHAKE_ERROR_EXTENSION,
				"check-websocket-request",
				"WebSocket compression policy declined a required offer"
			);
			return false;
		}
		return true;
	}
	if ( !xrtWsDeflateResponseCheck(
		&Deflate.Offer,
		&Response
	) || !xrtWsDeflateResponseWrite(
		&Response,
		pHandshake->Extensions,
		XWS_DEFLATE_MAX_SIZE,
		&iSize
	) ) {
		__xwsServerHandshakeWrap(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_EXTENSION,
			"check-websocket-request",
			"WebSocket compression policy produced an invalid response"
		);
		return false;
	}
	pHandshake->Extensions[iSize] = '\0';
	pHandshake->Deflate = Response;
	pHandshake->DeflateEnabled = true;
	return true;
}

#endif



/* 初始化服务端 Upgrade 默认配置。 */
XWS_SERVER_API void xrtWsServerConfigInit(
	xwsserverconfig* pConfig
)
{
	xwsserverconfig Config;

	if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
		__xwsServerHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"config-init-websocket-server",
			"WebSocket server configuration range is invalid"
		);
		return;
	}
	memset(&Config, 0, sizeof(Config));
	xrtWsConnConfigInit(&Config.Connection);
	Config.Connection.Role = XWS_ROLE_SERVER;
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 验证服务端固定配置，并以 Upgrade 实际采用的 Connection 参数完成预检。 */
XWS_SERVER_API bool xrtWsServerConfigValid(
	const xwsserverconfig* pConfig
)
{
	xwsserverconfig Config;
	xwsconnconfig Connection;

	if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
		return false;
	}
	memcpy(&Config, pConfig, sizeof(Config));
	if ( !xrtWsProtocolsValid(Config.Protocols) ) {
		return false;
	}
	#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_DEFLATE)
		if ( Config.RequireDeflate &&
			!Config.EnableDeflate ) {
			return false;
		}
	#else
		if (Config.EnableDeflate || Config.RequireDeflate) return false;
	#endif
	Connection = Config.Connection;
	Connection.Role = XWS_ROLE_SERVER;
	Connection.Protocol = (xstrview) { 0 };
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
		Connection.DeflateEnabled = false;
	#endif
	return xrtWsConnConfigValid(&Connection);
}



/* 严格验证服务端 HTTP/1.1 Upgrade 请求。 */
XWS_SERVER_API bool xrtWsServerCheck(
	const xhttpserverrequest* pRequest,
	const xwsserverconfig* pConfig,
	xwsserverhandshake* pHandshake
)
{
	__xws_server_fields Fields;
	xwsserverconfig Config;
	xwsserverhandshake Handshake;
	const xhttpfield* pKey;
	xhttpauthority Authority;
	bool bConnection;

	if ( !__xwsServerServerConfigSnapshot(
		&Config,
		pConfig,
		"check-websocket-request"
	) || !__xwsServerHttpObjectCheck(
		pRequest,
		"check-websocket-request",
		"WebSocket server request is null"
	) || !__xwsServerHttpRangeCheck(
		pHandshake,
		sizeof(Handshake),
		"check-websocket-request",
		"WebSocket server handshake output range is invalid"
	) ) {
		return false;
	}
	if ( ((Config.Protocols.Data == NULL) &&
		 (Config.Protocols.Size != 0)) ||
		(pHandshake == (const void*)pRequest) ||
		((pConfig != NULL) && xrtMemRangesOverlap(
			pHandshake,
			sizeof(Handshake),
			pConfig,
			sizeof(*pConfig)
		)) ) {
		__xwsServerHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"check-websocket-request",
			"WebSocket protocol list or handshake output is invalid"
		);
		return false;
	}
	memset(&Handshake, 0, sizeof(Handshake));
	Fields.Data = xrtHttpServerRequestHeaderData(pRequest);
	Fields.Count = xrtHttpServerRequestHeaderCount(
		pRequest
	);
	if ( !__xrtWsServerProtocolSelect(
		pRequest,
		Config.Protocols,
		&Handshake.Protocol
	) ) {
		return false;
	}
	if ( !__xwsServerHttpTextEqual(
		xrtHttpServerRequestMethod(pRequest),
		XRT_STR_LITERAL("GET")
	) ) {
		__xwsServerHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_METHOD,
			"check-websocket-request",
			"WebSocket Upgrade method is not GET"
		);
		return false;
	}
	if ( xrtHttpServerRequestVersion(pRequest) !=
		XHTTP_VERSION_1_1 ) {
		__xwsServerHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_VERSION,
			"check-websocket-request",
			"WebSocket Upgrade requires HTTP/1.1"
		);
		return false;
	}
	if ( __xwsServerHttpFieldCount(
		&Fields,
		XRT_STR_LITERAL("Host")
	) != 1 ) {
		__xwsServerHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_HOST,
			"check-websocket-request",
			"WebSocket Upgrade requires exactly one Host"
		);
		return false;
	}
	if ( !xrtHttpServerRequestAuthority(
		pRequest,
		&Authority
	) ) {
		__xwsServerHandshakeWrap(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_HOST,
			"check-websocket-request",
			"WebSocket Upgrade requires one valid Host"
		);
		return false;
	}
	if ( !__xwsServerHttpUpgradeHas(&Fields) ) {
		__xwsServerHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_UPGRADE,
			"check-websocket-request",
			"WebSocket Upgrade field is missing or ambiguous"
		);
		return false;
	}
	if ( !__xwsServerHttpTokenHas(
		&Fields,
		XRT_STR_LITERAL("Connection"),
		XRT_STR_LITERAL("Upgrade"),
		&bConnection
	) || !bConnection ) {
		__xwsServerHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_CONNECTION,
			"check-websocket-request",
			"WebSocket Connection field does not select Upgrade"
		);
		return false;
	}
	if ( !__xwsServerHttpTokenExact(
		&Fields,
		XRT_STR_LITERAL("Sec-WebSocket-Version"),
		XRT_STR_LITERAL("13")
	) ) {
		__xwsServerHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_VERSION,
			"check-websocket-request",
			"WebSocket version is missing, repeated or unsupported"
		);
		return false;
	}
	pKey = __xwsServerHttpFieldUnique(
		&Fields,
		XRT_STR_LITERAL("Sec-WebSocket-Key")
	);
	if ( (pKey == NULL) ||
		!xrtWsKeyValid(pKey->Value) ) {
		__xwsServerHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_KEY,
			"check-websocket-request",
			"WebSocket key is missing, repeated or invalid"
		);
		return false;
	}
	if ( (__xwsServerHttpFieldCount(
		&Fields,
		XRT_STR_LITERAL("Content-Length")
	) != 0) || (__xwsServerHttpFieldCount(
		&Fields,
		XRT_STR_LITERAL("Transfer-Encoding")
	) != 0) || (xrtHttpServerRequestBodyMode(
		pRequest
	) != XHTTP1_BODY_NONE) ||
		(xrtHttpServerRequestBodyBytes(pRequest) != 0) ) {
		__xwsServerHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_BODY,
			"check-websocket-request",
			"WebSocket Upgrade request contains an HTTP body"
		);
		return false;
	}
	#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_DEFLATE)
		if ( !__xrtWsServerDeflateSelect(
			&Fields,
			&Config,
			&Handshake
		) ) {
			return false;
		}
	#endif
	if ( !xrtWsAccept(
		pKey->Value,
		Handshake.Accept,
		sizeof(Handshake.Accept)
	) ) {
		return false;
	}
	memcpy(pHandshake, &Handshake, sizeof(Handshake));
	return true;
}



/* 从握手快照构造标准 101 Reply。 */
XWS_SERVER_API xhttpreply* xrtWsServerReply(
	const xwsserverhandshake* pHandshake
)
{
	xwsserverhandshake Handshake;
	xhttpreply* pReply;
	#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_DEFLATE)
		char sExtensions[XWS_DEFLATE_MAX_SIZE];
		size_t iExtensions = 0;
		bool bDeflateValid;
	#endif

	if ( !__xwsServerHttpRangeCheck(
		pHandshake,
		sizeof(Handshake),
		"reply-websocket-upgrade",
		"WebSocket server handshake snapshot range is invalid"
	) ) {
		return NULL;
	}
	memcpy(&Handshake, pHandshake, sizeof(Handshake));
	#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_DEFLATE)
		bDeflateValid = !Handshake.DeflateEnabled ||
			 (xrtWsDeflateResponseWrite(
				&Handshake.Deflate,
				sExtensions,
				sizeof(sExtensions),
				&iExtensions
			  ) &&
			  (Handshake.Extensions[iExtensions] == '\0') &&
			  (memcmp(
				Handshake.Extensions,
				sExtensions,
				iExtensions
			   ) == 0));
	#endif
	if ( (Handshake.Accept[XWS_ACCEPT_SIZE] != '\0') ||
		!xrtHttpFieldValueValid((xstrview) {
			Handshake.Accept,
			XWS_ACCEPT_SIZE
		}) ||
		((Handshake.Protocol.Data == NULL) &&
		 (Handshake.Protocol.Size != 0)) ||
		((Handshake.Protocol.Size != 0) &&
		 !xrtHttpTokenValid(Handshake.Protocol))
		#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_DEFLATE)
			|| !bDeflateValid
		#endif
		) {
		__xwsServerHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"reply-websocket-upgrade",
			"WebSocket server handshake snapshot is invalid"
		);
		return NULL;
	}
	pReply = xrtHttpReplyCreate(101);
	if ( pReply == NULL ) {
		__xwsServerHandshakeWrap(
			XERR_MEMORY,
			XWS_HANDSHAKE_ERROR_OUTPUT,
			"reply-websocket-upgrade",
			"WebSocket 101 Reply allocation failed"
		);
		return NULL;
	}
	if ( !xrtHttpReplySetHeader(
		pReply,
		XRT_STR_LITERAL("Upgrade"),
		XRT_STR_LITERAL("websocket")
	) || !xrtHttpReplySetHeader(
		pReply,
		XRT_STR_LITERAL("Connection"),
		XRT_STR_LITERAL("Upgrade")
	) || !xrtHttpReplySetHeader(
		pReply,
		XRT_STR_LITERAL("Sec-WebSocket-Accept"),
		(xstrview) {
			Handshake.Accept,
			XWS_ACCEPT_SIZE
		}
	) || ((Handshake.Protocol.Size != 0) &&
		!xrtHttpReplySetHeader(
			pReply,
			XRT_STR_LITERAL("Sec-WebSocket-Protocol"),
			Handshake.Protocol
		))
	#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_DEFLATE)
		|| (Handshake.DeflateEnabled &&
			!xrtHttpReplySetHeader(
				pReply,
				XRT_STR_LITERAL(
					"Sec-WebSocket-Extensions"
				),
				(xstrview) {
					Handshake.Extensions,
					iExtensions
				}
			))
	#endif
		) {
		xrtHttpReplyDestroy(pReply);
		__xwsServerHandshakeWrap(
			XERR_MEMORY,
			XWS_HANDSHAKE_ERROR_OUTPUT,
			"reply-websocket-upgrade",
			"WebSocket 101 Reply fields could not be stored"
		);
		return NULL;
	}
	return pReply;
}



/* 根据稳定握手域和错误类型选择不会泄露内部细节的 HTTP 状态。 */
static uint16 __xrtWsServerRejectStatus(
	const xerror* pError
)
{
	cstr sDomain;
	int32 iCode;

	if ( (pError == NULL) ||
		(xrtErrorKind(pError) != XERR_PROTOCOL) ) {
		return XHTTP_STATUS_INTERNAL_SERVER_ERROR;
	}
	sDomain = xrtErrorDomain(pError);
	if ( (sDomain == NULL) ||
		(strcmp(sDomain, "xrt.websocket.handshake") != 0) ) {
		return XHTTP_STATUS_INTERNAL_SERVER_ERROR;
	}
	iCode = xrtErrorCode(pError);
	if ( iCode == (int32)XWS_HANDSHAKE_ERROR_METHOD ) {
		return XHTTP_STATUS_METHOD_NOT_ALLOWED;
	}
	if ( iCode == (int32)XWS_HANDSHAKE_ERROR_VERSION ) {
		return XHTTP_STATUS_UPGRADE_REQUIRED;
	}
	return XHTTP_STATUS_BAD_REQUEST;
}



/* 构造并提交标准 WebSocket 握手拒绝响应。 */
XWS_SERVER_API xnetresult xrtWsServerReject(
	xhttpconn* pHttp,
	const xerror* pError
)
{
	uint16 iStatus;
	xhttpreply* pReply;
	xnetresult Result;
	bool bReady;

	if ( !__xwsServerHttpObjectCheck(
		pHttp,
		"reject-websocket-upgrade",
		"HTTP server connection is null"
	) ) {
		return XNET_RESULT_ERROR;
	}
	iStatus = __xrtWsServerRejectStatus(pError);
	pReply = xrtHttpReplyCreate(iStatus);
	if ( pReply == NULL ) {
		return XNET_RESULT_ERROR;
	}
	bReady = true;
	if ( iStatus == XHTTP_STATUS_METHOD_NOT_ALLOWED ) {
		bReady = xrtHttpReplySetHeader(
			pReply,
			XRT_STR_LITERAL("Allow"),
			XRT_STR_LITERAL("GET")
		);
	} else if ( iStatus == XHTTP_STATUS_UPGRADE_REQUIRED ) {
		bReady = xrtHttpReplySetHeader(
			pReply,
			XRT_STR_LITERAL("Upgrade"),
			XRT_STR_LITERAL("websocket")
		) && xrtHttpReplySetHeader(
			pReply,
			XRT_STR_LITERAL("Sec-WebSocket-Version"),
			XRT_STR_LITERAL("13")
		);
	}
	if ( bReady ) {
		Result = xrtHttpConnRespond(pHttp, pReply);
	} else {
		Result = XNET_RESULT_ERROR;
	}
	xrtHttpReplyDestroy(pReply);
	return Result;
}



/* 把 HTTP Upgrade 终态接管为共享 WebSocket Connection。 */
static void __xrtWsServerUpgradeDone(
	xhttpconn* pHttp,
	xnetresult Result,
	xhttpupgrade Upgrade,
	const xerror* pError,
	ptr pData
)
{
	__xrt_ws_upgrade* pUpgrade =
		(__xrt_ws_upgrade*)pData;
	xwsconn* pConnection = NULL;
	xerror* pOwnedError = NULL;

	if ( Result == XNET_RESULT_OK ) {
		pUpgrade->Config.Connection.Protocol =
			pUpgrade->Protocol;
		if ( Upgrade.Tcp != NULL ) {
			pConnection = xrtWsConnAttach(
				Upgrade.Tcp,
				&pUpgrade->Config.Connection,
				&pUpgrade->Events,
				pUpgrade->EventData
			);
			if ( pConnection != NULL ) {
				Upgrade.Tcp = NULL;
			}
		}
		#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_TLS)
			else if ( Upgrade.Tls != NULL ) {
				pConnection = xrtWsConnAttachTls(
					Upgrade.Tls,
					&pUpgrade->Config.Connection,
					&pUpgrade->Events,
					pUpgrade->EventData
				);
				if ( pConnection != NULL ) {
					Upgrade.Tls = NULL;
				}
			}
		#endif
		else {
			__xwsServerHandshakeError(
				XERR_STATE,
				XWS_HANDSHAKE_ERROR_UPGRADE,
				"attach-websocket-server",
				"HTTP Upgrade returned an unsupported transport"
			);
		}
		if ( pConnection == NULL ) {
			pOwnedError = xrtTakeError();
			xrtHttpUpgradeAbort(&Upgrade);
			Result = XNET_RESULT_ERROR;
			pError = pOwnedError;
		}
	}
	pUpgrade->Proc(
		pHttp,
		Result,
		pConnection,
		pError,
		pUpgrade->Data
	);
	xrtErrorFree(pOwnedError);
	xrtFree(pUpgrade);
}



/* 判断附加 Header 是否会覆盖 WebSocket 握手或 HTTP 分帧语义。 */
static bool __xrtWsServerHeaderReserved(xstrview Name)
{
	return xrtHttpFieldNameEqual(
		Name,
		XRT_STR_LITERAL("Connection")
	) || xrtHttpFieldNameEqual(
		Name,
		XRT_STR_LITERAL("Upgrade")
	) || xrtHttpFieldNameEqual(
		Name,
		XRT_STR_LITERAL("Sec-WebSocket-Accept")
	) || xrtHttpFieldNameEqual(
		Name,
		XRT_STR_LITERAL("Sec-WebSocket-Protocol")
	) || xrtHttpFieldNameEqual(
		Name,
		XRT_STR_LITERAL("Sec-WebSocket-Extensions")
	) || xrtHttpFieldNameEqual(
		Name,
		XRT_STR_LITERAL("Content-Length")
	) || xrtHttpFieldNameEqual(
		Name,
		XRT_STR_LITERAL("Transfer-Encoding")
	) || xrtHttpFieldNameEqual(
		Name,
		XRT_STR_LITERAL("Trailer")
	);
}



/* 把已经快照的握手、事件和 Reply 提交给 HTTP Upgrade 层。 */
static xnetresult __xrtWsServerUpgradeSubmit(
	xhttpconn* pHttp,
	const xwsserverconfig* pConfig,
	const xwsserverhandshake* pHandshake,
	const xhttpreply* pReply,
	const xwsconnevents* pEvents,
	ptr pEventData,
	xwsupgradeproc pProc,
	ptr pData
)
{
	__xrt_ws_upgrade* pUpgrade;
	xnetresult Result;
	size_t iAllocation;

	if ( pHandshake->Protocol.Size >
		(SIZE_MAX - sizeof(*pUpgrade) - 1u) ) {
		__xwsServerHandshakeError(
			XERR_RANGE,
			XWS_HANDSHAKE_ERROR_PROTOCOL,
			"accept-websocket-upgrade",
			"WebSocket selected protocol is too large"
		);
		return XNET_RESULT_ERROR;
	}
	iAllocation = sizeof(*pUpgrade) +
		pHandshake->Protocol.Size + 1u;
	pUpgrade = (__xrt_ws_upgrade*)xrtCalloc(
		1,
		iAllocation
	);
	if ( pUpgrade == NULL ) {
		__xwsServerHandshakeWrap(
			XERR_MEMORY,
			XWS_HANDSHAKE_ERROR_OUTPUT,
			"accept-websocket-upgrade",
			"WebSocket Upgrade context allocation failed"
		);
		return XNET_RESULT_ERROR;
	}
	pUpgrade->Config = *pConfig;
	pUpgrade->Config.Connection.Role = XWS_ROLE_SERVER;
	pUpgrade->Config.Connection.Protocol =
		(xstrview) { 0 };
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
		pUpgrade->Config.Connection.DeflateEnabled = false;
		#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_DEFLATE)
			pUpgrade->Config.Connection.DeflateEnabled =
				pHandshake->DeflateEnabled;
			if ( pHandshake->DeflateEnabled ) {
				pUpgrade->Config.Connection.Deflate =
					pHandshake->Deflate;
			}
		#endif
	#endif
	pUpgrade->Events = *pEvents;
	pUpgrade->Proc = pProc;
	pUpgrade->EventData = pEventData;
	pUpgrade->Data = pData;
	if ( pHandshake->Protocol.Size != 0 ) {
		memcpy(
			pUpgrade->Storage,
			pHandshake->Protocol.Data,
			pHandshake->Protocol.Size
		);
		pUpgrade->Protocol.Data =
			pUpgrade->Storage;
		pUpgrade->Protocol.Size =
			pHandshake->Protocol.Size;
	}
	Result = xrtHttpConnUpgrade(
		pHttp,
		pReply,
		__xrtWsServerUpgradeDone,
		pUpgrade
	);
	if ( Result != XNET_RESULT_OK ) {
		xrtFree(pUpgrade);
	}
	return Result;
}



/* 提交已经校验的握手，并在受保护字段之外追加应用 Header。 */
XWS_SERVER_API xnetresult xrtWsUpgradeAccept(
	xhttpconn* pHttp,
	const xwsserverconfig* pConfig,
	const xwsserverhandshake* pHandshake,
	const xhttpheaders* pHeaders,
	const xwsconnevents* pEvents,
	ptr pEventData,
	xwsupgradeproc pProc,
	ptr pData
)
{
	xwsserverconfig Config;
	xwsconnevents Events;
	xwsserverhandshake Handshake;
	xhttpheaders* pHeaderSnapshot = NULL;
	xhttpreply* pReply;
	xnetresult Result;
	size_t iHeaderCount;

	if ( !__xwsServerServerConfigSnapshot(
		&Config,
		pConfig,
		"accept-websocket-upgrade"
	) || !__xwsServerConnEventsSnapshot(
		&Events,
		pEvents,
		"accept-websocket-upgrade"
	) || !__xwsServerHttpObjectCheck(
		pHttp,
		"accept-websocket-upgrade",
		"HTTP server connection is null"
	) || !__xwsServerHttpRangeCheck(
		pHandshake,
		sizeof(Handshake),
		"accept-websocket-upgrade",
		"WebSocket server handshake snapshot range is invalid"
	) ) {
		return XNET_RESULT_ERROR;
	}
	if ( pProc == NULL ) {
		__xwsServerHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"accept-websocket-upgrade",
			"WebSocket Upgrade callback is null"
		);
		return XNET_RESULT_ERROR;
	}
	if ( !xrtWsServerConfigValid(&Config) ) {
		__xwsServerHandshakeError(
			XERR_VALUE,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"accept-websocket-upgrade",
			"WebSocket server configuration is invalid"
		);
		return XNET_RESULT_ERROR;
	}
	memcpy(&Handshake, pHandshake, sizeof(Handshake));
	if ( (Handshake.Protocol.Size != 0) &&
		!xrtWsProtocolsHas(
			Config.Protocols,
			Handshake.Protocol
		) ) {
		__xwsServerHandshakeError(
			XERR_VALUE,
			XWS_HANDSHAKE_ERROR_PROTOCOL,
			"accept-websocket-upgrade",
			"WebSocket selected protocol is not enabled by the server"
		);
		return XNET_RESULT_ERROR;
	}
	#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_DEFLATE)
		if ( Handshake.DeflateEnabled &&
			!Config.EnableDeflate ) {
			__xwsServerHandshakeError(
				XERR_VALUE,
				XWS_HANDSHAKE_ERROR_EXTENSION,
				"accept-websocket-upgrade",
				"WebSocket compression was not enabled by the server"
			);
			return XNET_RESULT_ERROR;
		}
	#endif
	pReply = xrtWsServerReply(&Handshake);
	if ( pReply == NULL ) {
		return XNET_RESULT_ERROR;
	}
	if ( pHeaders != NULL ) {
		pHeaderSnapshot = xrtHttpHeadersClone(pHeaders);
		if ( pHeaderSnapshot == NULL ) {
			xrtHttpReplyDestroy(pReply);
			__xwsServerHandshakeWrap(
				XERR_VALUE,
				XWS_HANDSHAKE_ERROR_OUTPUT,
				"accept-websocket-upgrade",
				"WebSocket application Header snapshot failed"
			);
			return XNET_RESULT_ERROR;
		}
	}
	iHeaderCount = xrtHttpHeadersCount(pHeaderSnapshot);
	for ( size_t i = 0; i < iHeaderCount; i++ ) {
		const xhttpfield* pField =
			xrtHttpHeadersAt(pHeaderSnapshot, i);

		if ( (pField == NULL) ||
			__xrtWsServerHeaderReserved(pField->Name) ) {
			__xwsServerHandshakeError(
				XERR_VALUE,
				XWS_HANDSHAKE_ERROR_OUTPUT,
				"accept-websocket-upgrade",
				"WebSocket application Header is invalid or reserved"
			);
			Result = XNET_RESULT_ERROR;
			goto Cleanup;
		}
		if ( !xrtHttpReplyAddHeader(
			pReply,
			pField->Name,
			pField->Value
		) ) {
			__xwsServerHandshakeWrap(
				XERR_VALUE,
				XWS_HANDSHAKE_ERROR_OUTPUT,
				"accept-websocket-upgrade",
				"WebSocket application Header could not be appended"
			);
			Result = XNET_RESULT_ERROR;
			goto Cleanup;
		}
	}
	Result = __xrtWsServerUpgradeSubmit(
		pHttp,
		&Config,
		&Handshake,
		pReply,
		&Events,
		pEventData,
		pProc,
		pData
	);

Cleanup:
	xrtHttpHeadersDestroy(pHeaderSnapshot);
	xrtHttpReplyDestroy(pReply);
	return Result;
}



/* 验证并提交常用服务端 WebSocket Upgrade。 */
XWS_SERVER_API xnetresult xrtWsUpgrade(
	xhttpconn* pHttp,
	const xwsserverconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pEventData,
	xwsupgradeproc pProc,
	ptr pData
)
{
	xwsserverconfig Config;
	xwsconnevents Events;
	xwsserverhandshake Handshake;
	const xhttpserverrequest* pRequest;

	if ( !__xwsServerServerConfigSnapshot(
		&Config,
		pConfig,
		"upgrade-websocket-server"
	) || !__xwsServerConnEventsSnapshot(
		&Events,
		pEvents,
		"upgrade-websocket-server"
	) || !__xwsServerHttpObjectCheck(
		pHttp,
		"upgrade-websocket-server",
		"HTTP server connection is null"
	) ) {
		return XNET_RESULT_ERROR;
	}
	if ( pProc == NULL ) {
		__xwsServerHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"upgrade-websocket-server",
			"WebSocket Upgrade callback is null"
		);
		return XNET_RESULT_ERROR;
	}
	if ( !xrtWsServerConfigValid(&Config) ) {
		__xwsServerHandshakeError(
			XERR_VALUE,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"upgrade-websocket-server",
			"WebSocket server configuration is invalid"
		);
		return XNET_RESULT_ERROR;
	}
	pRequest = xrtHttpConnRequest(pHttp);
	if ( !xrtWsServerCheck(
		pRequest,
		&Config,
		&Handshake
	) ) {
		return XNET_RESULT_ERROR;
	}
	return xrtWsUpgradeAccept(
		pHttp,
		&Config,
		&Handshake,
		NULL,
		&Events,
		pEventData,
		pProc,
		pData
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xws-server/src/websocket/server_router.c */
/* ========================================================================== */

#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_ROUTER)



#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_ROUTER)

/* 设置带稳定域、代码、操作和可选原因链的 WebSocket Router 错误。 */
static void __xrtWsServerRouterSetError(
	xerrkind Kind,
	xwsserverroutererror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Code = (int32)Code;
	Desc.SystemCode = pCause != NULL ?
		xrtErrorSystemCode(pCause) : 0;
	Desc.Domain = "xrt.websocket.server.router";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 验证固定公开对象占用完整且不回绕的地址区间。 */
static bool __xrtWsServerRouterRangeCheck(
	const void* pObject,
	size_t iSize,
	cstr sOperation,
	cstr sMessage
)
{
	if ( xrtMemRangeValid(pObject, iSize) ) {
		return true;
	}
	__xrtWsServerRouterSetError(
		XERR_ARGUMENT,
		XWS_SERVER_ROUTER_ERROR_ARGUMENT,
		sOperation,
		sMessage,
		NULL
	);
	return false;
}



/* 返回原因错误已有类型，缺失时使用调用点给出的保守类型。 */
static xerrkind __xrtWsServerRouterCauseKind(
	const xerror* pCause,
	xerrkind Default
)
{
	xerrkind Kind = xrtErrorKind(pCause);

	return Kind != XERR_NONE ? Kind : Default;
}



/* 比较大小写敏感且不要求零结尾的 HTTP 方法。 */
static bool __xrtWsServerRouterMethod(
	xstrview Method,
	cstr sExpected
)
{
	size_t iSize = strlen(sExpected);

	return (Method.Size == iSize) &&
		(memcmp(Method.Data, sExpected, iSize) == 0);
}



/* 向固定路由观察者发布一次同步握手或响应错误。 */
static void __xrtWsServerRouterReport(
	xrt_ws_server_route* pRoute,
	xhttpconn* pHttp,
	const xerror* pError
)
{
	if ( pRoute->Config.Error != NULL ) {
		pRoute->Config.Error(
			pHttp, pError, pRoute->Config.Data
		);
	}
}



/* 提交只允许 GET 的方法拒绝响应。 */
static bool __xrtWsServerRouterMethodReply(
	xhttpconn* pHttp,
	uint16 iStatus
)
{
	xhttpreply* pReply = xrtHttpReplyCreate(iStatus);
	xnetresult Result = XNET_RESULT_ERROR;

	if ( (pReply != NULL) &&
		xrtHttpReplySetHeader(
			pReply,
			XRT_STR_LITERAL("Allow"),
			XRT_STR_LITERAL("GET")
		) ) {
		Result = xrtHttpConnRespond(pHttp, pReply);
	}
	xrtHttpReplyDestroy(pReply);
	return Result == XNET_RESULT_OK;
}



/* 提交无正文的固定状态响应。 */
static bool __xrtWsServerRouterStatusReply(
	xhttpconn* pHttp,
	uint16 iStatus
)
{
	xhttpreply* pReply = xrtHttpReplyCreate(iStatus);
	xnetresult Result = XNET_RESULT_ERROR;

	if ( pReply != NULL ) {
		Result = xrtHttpConnRespond(pHttp, pReply);
	}
	xrtHttpReplyDestroy(pReply);
	return Result == XNET_RESULT_OK;
}



/* 包装响应构造或提交失败，通知观察者后异常关闭。 */
static void __xrtWsServerRouterResponseFailed(
	xrt_ws_server_route* pRoute,
	xhttpconn* pHttp,
	cstr sMessage
)
{
	xerror* pCause = xrtErrorRef(xrtGetError());
	xerror* pError;

	__xrtWsServerRouterSetError(
		__xrtWsServerRouterCauseKind(pCause, XERR_IO),
		XWS_SERVER_ROUTER_ERROR_RESPONSE,
		"reject-websocket-server-route",
		sMessage,
		pCause
	);
	xrtErrorFree(pCause);
	pError = xrtErrorRef(xrtGetError());
	__xrtWsServerRouterReport(pRoute, pHttp, pError);
	xrtErrorFree(pError);
	(void)xrtHttpConnAbort(pHttp);
}



/* 报告并提交标准握手拒绝；二次响应失败时异常关闭。 */
static void __xrtWsServerRouterReject(
	xrt_ws_server_route* pRoute,
	xhttpconn* pHttp,
	const xerror* pError
)
{
	__xrtWsServerRouterReport(pRoute, pHttp, pError);
	if ( xrtWsServerReject(
		pHttp, pError
	) != XNET_RESULT_OK ) {
		__xrtWsServerRouterResponseFailed(
			pRoute,
			pHttp,
			"WebSocket handshake rejection response failed"
		);
	}
}



/* 拒绝不符合 Origin 或业务授权策略的握手。 */
static void __xrtWsServerRouterAuthorizationReject(
	xrt_ws_server_route* pRoute,
	xhttpconn* pHttp,
	cstr sMessage
)
{
	xerror* pError;

	__xrtWsServerRouterSetError(
		XERR_PERMISSION,
		XWS_SERVER_ROUTER_ERROR_AUTHORIZATION,
		"authorize-websocket-server-route",
		sMessage,
		NULL
	);
	pError = xrtErrorRef(xrtGetError());
	__xrtWsServerRouterReport(pRoute, pHttp, pError);
	xrtErrorFree(pError);
	if ( !__xrtWsServerRouterStatusReply(
		pHttp, XHTTP_STATUS_FORBIDDEN
	) ) {
		__xrtWsServerRouterResponseFailed(
			pRoute,
			pHttp,
			"WebSocket authorization response failed"
		);
	}
}



/* 比较浏览器 Origin 与当前 HTTP 请求的 scheme、host 和有效端口。 */
static bool __xrtWsServerRouterOriginSame(
	xhttpconn* pHttp,
	const xhttpserverrequest* pRequest,
	const xhttporigin* pOrigin
)
{
	xhttpauthority Authority;
	xstrview Scheme = xrtHttpConnSecure(pHttp) ?
		XRT_STR_LITERAL("https") : XRT_STR_LITERAL("http");
	uint16 iDefault = xrtHttpConnSecure(pHttp) ? 443u : 80u;
	uint16 iOriginPort;
	uint16 iRequestPort;

	if ( ((pOrigin->Flags & XHTTP_ORIGIN_NULL) != 0) ||
		!xrtUrlSchemeIs(&pOrigin->Url, Scheme) ||
		!xrtHttpServerRequestAuthority(pRequest, &Authority) ||
		!xrtHttpHostEqual(pOrigin->Url.Host, Authority.Host) ||
		!xrtUrlPort(&pOrigin->Url, &iOriginPort) ||
		!xrtHttpAuthorityPort(
			&Authority, iDefault, &iRequestPort
		) ) {
		return false;
	}
	return iOriginPort == iRequestPort;
}



/* 执行固定路由 Origin 策略，ANY 保留给已在外层授权的组合。 */
static bool __xrtWsServerRouterOriginAllowed(
	xrt_ws_server_route* pRoute,
	xhttpconn* pHttp,
	const xhttpserverrequest* pRequest
)
{
	xhttporigin Origin;
	xhttpnext Next;

	if ( pRoute->Config.Origin == XWS_SERVER_ORIGIN_ANY ) {
		return true;
	}
	Next = xrtHttpOriginFields(
		xrtHttpServerRequestHeaderData(pRequest),
		xrtHttpServerRequestHeaderCount(pRequest),
		&Origin
	);
	if ( Next == XHTTP_NEXT_END ) {
		return pRoute->Config.Origin ==
			XWS_SERVER_ORIGIN_SAME_HOST_OR_ABSENT;
	}
	return (Next == XHTTP_NEXT_ITEM) &&
		__xrtWsServerRouterOriginSame(
			pHttp, pRequest, &Origin
		);
}



/* 判断请求是否声明或已经携带正文；WebSocket Upgrade 不接受这些请求。 */
static bool __xrtWsServerRouterHasBody(
	const xhttpserverrequest* pRequest
)
{
	return (xrtHttpServerRequestHeader(
		pRequest,
		XRT_STR_LITERAL("Content-Length")
	) != NULL) || (xrtHttpServerRequestHeader(
		pRequest,
		XRT_STR_LITERAL("Transfer-Encoding")
	) != NULL) || (xrtHttpServerRequestBodyMode(pRequest) !=
		XHTTP1_BODY_NONE) ||
		(xrtHttpServerRequestBodyBytes(pRequest) != 0);
}



/* 在 Header 阶段处理方法和正文门禁；无正文请求进入完整请求阶段。 */
static xhttpserverbodypolicy __xrtWsServerRouterHeaders(
	xhttpserver* pServer,
	xhttpconn* pHttp,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
)
{
	xrt_ws_server_route* pRoute =
		(xrt_ws_server_route*)pData;
	xstrview Method = xrtHttpServerRequestMethod(pRequest);
	xwsserverhandshake Handshake;
	xerror* pError;

	(void)pServer;
	(void)pParams;
	(void)iParamCount;
	if ( !__xrtWsServerRouterMethod(Method, "GET") ) {
		if ( !__xrtWsServerRouterMethodReply(
			pHttp, XHTTP_STATUS_METHOD_NOT_ALLOWED
		) ) {
			__xrtWsServerRouterResponseFailed(
				pRoute,
				pHttp,
				"WebSocket method rejection response failed"
			);
		}
		return XHTTP_SERVER_BODY_REJECT;
	}
	if ( __xrtWsServerRouterHasBody(pRequest) ) {
		(void)xrtWsServerCheck(
			pRequest,
			&pRoute->Config.Server,
			&Handshake
		);
		pError = xrtErrorRef(xrtGetError());
		__xrtWsServerRouterReject(pRoute, pHttp, pError);
		xrtErrorFree(pError);
		return XHTTP_SERVER_BODY_REJECT;
	}
	return XHTTP_SERVER_BODY_BUFFER;
}



/* 在完整请求阶段提交 Upgrade，确保未消费字节只属于下一层协议。 */
static void __xrtWsServerRouterRequest(
	xhttpserver* pServer,
	xhttpconn* pHttp,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
)
{
	xrt_ws_server_route* pRoute =
		(xrt_ws_server_route*)pData;
	xrt_ws_server_route_connection* pContext;
	xwsserverhandshake Handshake;
	xerror* pError;
	xnetresult Result;
	xerrkind Kind;
	xwsserverroutererror Code;

	(void)pServer;
	if ( !xrtWsServerCheck(
		pRequest, &pRoute->Config.Server, &Handshake
	) ) {
		pError = xrtErrorRef(xrtGetError());
		__xrtWsServerRouterReject(pRoute, pHttp, pError);
		xrtErrorFree(pError);
		return;
	}
	if ( !__xrtWsServerRouterOriginAllowed(
		pRoute, pHttp, pRequest
	) ) {
		__xrtWsServerRouterAuthorizationReject(
			pRoute,
			pHttp,
			"WebSocket request Origin is not allowed"
		);
		return;
	}
	if ( (pRoute->Config.Authorize != NULL) &&
		!pRoute->Config.Authorize(
			pHttp,
			pRequest,
			pParams,
			iParamCount,
			&Handshake,
			pRoute->Config.Data
		) ) {
		__xrtWsServerRouterAuthorizationReject(
			pRoute,
			pHttp,
			"WebSocket request was rejected by the authorization callback"
		);
		return;
	}
	pContext = __xrtWsServerRouteConnectionCreate(pRoute);
	if ( pContext == NULL ) {
		pError = xrtErrorRef(xrtGetError());
		Kind = __xrtWsServerRouterCauseKind(
			pError,
			XERR_MEMORY
		);
		Code = Kind == XERR_MEMORY ?
			XWS_SERVER_ROUTER_ERROR_MEMORY :
			XWS_SERVER_ROUTER_ERROR_STATE;
		__xrtWsServerRouterSetError(
			Kind,
			Code,
			"upgrade-websocket-server-route",
			"WebSocket route Upgrade context could not be retained",
			pError
		);
		xrtErrorFree(pError);
		pError = xrtErrorRef(xrtGetError());
		__xrtWsServerRouterReject(pRoute, pHttp, pError);
		xrtErrorFree(pError);
		return;
	}
	Result = xrtWsUpgradeAccept(
		pHttp,
		&pRoute->Config.Server,
		&Handshake,
		NULL,
		&__xrtWsServerRouteEvents,
		pContext,
		__xrtWsServerRouteUpgradeDone,
		pContext
	);
	if ( Result != XNET_RESULT_OK ) {
		pError = xrtErrorRef(xrtGetError());
		__xrtWsServerRouteConnectionRelease(pContext);
		__xrtWsServerRouterReject(pRoute, pHttp, pError);
		xrtErrorFree(pError);
	}
}



/* 初始化固定 WebSocket 路由的默认配置。 */
XWS_SERVER_API void xrtWsServerRouteConfigInit(
	xwsserverrouteconfig* pConfig
)
{
	xwsserverrouteconfig Config;

	if ( !__xrtWsServerRouterRangeCheck(
		pConfig,
		sizeof(Config),
		"config-init-websocket-server-route",
		"WebSocket server route configuration range is invalid"
	) ) {
		return;
	}
	memset(&Config, 0, sizeof(Config));
	xrtWsServerConfigInit(&Config.Server);
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 复制固定配置和协议列表，并把路由快照所有权移交给 Router。 */
XWS_SERVER_API bool xrtWsServerRoute(
	xhttpserverrouter* pRouter,
	xstrview Pattern,
	const xwsserverrouteconfig* pConfig
)
{
	xwsserverrouteconfig Config;
	xrt_ws_server_route* pRoute;
	xhttpserverrouteevents Events;
	xerror* pCause;
	xerrkind Kind;
	size_t iAllocation;

	if ( pRouter == NULL ) {
		__xrtWsServerRouterSetError(
			XERR_ARGUMENT,
			XWS_SERVER_ROUTER_ERROR_ARGUMENT,
			"register-websocket-server-route",
			"HTTP server Router is null",
			NULL
		);
		return false;
	}
	if ( !xrtMemRangeValid(Pattern.Data, Pattern.Size) ) {
		__xrtWsServerRouterSetError(
			XERR_ARGUMENT,
			XWS_SERVER_ROUTER_ERROR_ARGUMENT,
			"register-websocket-server-route",
			"WebSocket server route pattern range is invalid",
			NULL
		);
		return false;
	}
	xrtWsServerRouteConfigInit(&Config);
	if ( pConfig != NULL ) {
		if ( !__xrtWsServerRouterRangeCheck(
			pConfig,
			sizeof(Config),
			"register-websocket-server-route",
			"WebSocket server route configuration range is invalid"
		) ) {
			return false;
		}
		memcpy(&Config, pConfig, sizeof(Config));
	}
	if ( !xrtWsServerConfigValid(&Config.Server) ) {
		pCause = xrtErrorRef(xrtGetError());
		__xrtWsServerRouterSetError(
			XERR_VALUE,
			XWS_SERVER_ROUTER_ERROR_CONFIG,
			"register-websocket-server-route",
			"WebSocket server route configuration is invalid",
			pCause
		);
		xrtErrorFree(pCause);
		return false;
	}
	if ( (Config.Origin !=
		  XWS_SERVER_ORIGIN_SAME_HOST_OR_ABSENT) &&
		(Config.Origin != XWS_SERVER_ORIGIN_SAME_HOST) &&
		(Config.Origin != XWS_SERVER_ORIGIN_ANY) ) {
		__xrtWsServerRouterSetError(
			XERR_VALUE,
			XWS_SERVER_ROUTER_ERROR_CONFIG,
			"register-websocket-server-route",
			"WebSocket server route Origin policy is invalid",
			NULL
		);
		return false;
	}
	if ( Config.Server.Protocols.Size >
		(SIZE_MAX - sizeof(*pRoute) - 1u) ) {
		__xrtWsServerRouterSetError(
			XERR_RANGE,
			XWS_SERVER_ROUTER_ERROR_CONFIG,
			"register-websocket-server-route",
			"WebSocket server protocol list is too large",
			NULL
		);
		return false;
	}
	iAllocation = sizeof(*pRoute) +
		Config.Server.Protocols.Size + 1u;
	pRoute = (xrt_ws_server_route*)xrtCalloc(
		1, iAllocation
	);
	if ( pRoute == NULL ) {
		pCause = xrtErrorRef(xrtGetError());
		__xrtWsServerRouterSetError(
			XERR_MEMORY,
			XWS_SERVER_ROUTER_ERROR_MEMORY,
			"register-websocket-server-route",
			"WebSocket server route snapshot allocation failed",
			pCause
		);
		xrtErrorFree(pCause);
		return false;
	}
	pRoute->References = 1;
	pRoute->Config = Config;
	if ( Config.Server.Protocols.Size != 0 ) {
		memcpy(
			pRoute->Storage,
			Config.Server.Protocols.Data,
			Config.Server.Protocols.Size
		);
		pRoute->Config.Server.Protocols.Data =
			pRoute->Storage;
	}
	pRoute->Storage[Config.Server.Protocols.Size] = '\0';

	xrtHttpServerRouteEventsInit(&Events);
	Events.Headers = __xrtWsServerRouterHeaders;
	Events.Request = __xrtWsServerRouterRequest;
	Events.Release = __xrtWsServerRouteRelease;
	Events.Data = pRoute;
	if ( !xrtHttpServerRouteEvents(
		pRouter,
		XRT_STR_LITERAL("*"),
		Pattern,
		&Events
	) ) {
		pCause = xrtErrorRef(xrtGetError());
		Kind = __xrtWsServerRouterCauseKind(
			pCause, XERR_INTERNAL
		);
		xrtFree(pRoute);
		__xrtWsServerRouterSetError(
			Kind,
			XWS_SERVER_ROUTER_ERROR_ROUTE,
			"register-websocket-server-route",
			"HTTP server route registration failed",
			pCause
		);
		xrtErrorFree(pCause);
		return false;
	}
	return true;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xws-server/src/websocket/server_router_events.c */
/* ========================================================================== */

#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_ROUTER)



#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_ROUTER)

/* 增加活动 Upgrade 或 Connection 对固定路由快照的引用。 */
static bool __xrtWsServerRouteRef(
	xrt_ws_server_route* pRoute
)
{
	if ( xrtRefRetain(&pRoute->References) < 0 ) {
		__xwsServerErrorSetDetail(
			XERR_STATE,
			"xrt.websocket.server.router",
			(int32)XWS_SERVER_ROUTER_ERROR_STATE,
			"retain-websocket-server-route",
			"WebSocket server route reference cannot be retained",
			NULL
		);
		return false;
	}
	return true;
}



/* 释放 Router 或连接引用，并在最终引用上清理用户 Data。 */
void __xrtWsServerRouteRelease(ptr pData)
{
	xrt_ws_server_route* pRoute =
		(xrt_ws_server_route*)pData;

	if ( (pRoute == NULL) ||
		(xrtRefRelease(&pRoute->References) != 0) ) {
		return;
	}
	if ( pRoute->Config.Release != NULL ) {
		pRoute->Config.Release(pRoute->Config.Data);
	}
	xrtFree(pRoute);
}



/* 创建由 Upgrade 终态和成功连接共同管理的上下文。 */
xrt_ws_server_route_connection* __xrtWsServerRouteConnectionCreate(
	xrt_ws_server_route* pRoute
)
{
	xrt_ws_server_route_connection* pConnection;

	if ( pRoute == NULL ) {
		return NULL;
	}
	pConnection = (xrt_ws_server_route_connection*)xrtCalloc(
		1, sizeof(*pConnection)
	);
	if ( pConnection == NULL ) {
		return NULL;
	}
	pConnection->References = 1;
	pConnection->Route = pRoute;
	if ( !__xrtWsServerRouteRef(pRoute) ) {
		xrtFree(pConnection);
		return NULL;
	}
	return pConnection;
}



/* 释放单次连接上下文，并把最后一个引用归还给固定路由。 */
void __xrtWsServerRouteConnectionRelease(
	xrt_ws_server_route_connection* pConnection
)
{
	xrt_ws_server_route* pRoute;

	if ( (pConnection == NULL) ||
		(xrtRefRelease(&pConnection->References) != 0) ) {
		return;
	}
	pRoute = pConnection->Route;
	xrtFree(pConnection);
	__xrtWsServerRouteRelease(pRoute);
}



/* 转发逻辑消息开始事件。 */
static void __xrtWsServerRouteMessageBegin(
	xwsconn* pConnection,
	const xwsmessageinfo* pInfo,
	ptr pData
)
{
	xrt_ws_server_route_connection* pContext =
		(xrt_ws_server_route_connection*)pData;
	xrt_ws_server_route* pRoute = pContext->Route;

	if ( pRoute->Config.Events.MessageBegin != NULL ) {
		pRoute->Config.Events.MessageBegin(
			pConnection, pInfo, pRoute->Config.Data
		);
	}
}



/* 转发逻辑消息数据片段。 */
static void __xrtWsServerRouteMessageData(
	xwsconn* pConnection,
	xbytesview Data,
	ptr pData
)
{
	xrt_ws_server_route_connection* pContext =
		(xrt_ws_server_route_connection*)pData;
	xrt_ws_server_route* pRoute = pContext->Route;

	if ( pRoute->Config.Events.MessageData != NULL ) {
		pRoute->Config.Events.MessageData(
			pConnection, Data, pRoute->Config.Data
		);
	}
}



/* 转发逻辑消息结束事件。 */
static void __xrtWsServerRouteMessageEnd(
	xwsconn* pConnection,
	ptr pData
)
{
	xrt_ws_server_route_connection* pContext =
		(xrt_ws_server_route_connection*)pData;
	xrt_ws_server_route* pRoute = pContext->Route;

	if ( pRoute->Config.Events.MessageEnd != NULL ) {
		pRoute->Config.Events.MessageEnd(
			pConnection, pRoute->Config.Data
		);
	}
}



/* 转发 Ping 事件。 */
static void __xrtWsServerRoutePing(
	xwsconn* pConnection,
	xbytesview Payload,
	ptr pData
)
{
	xrt_ws_server_route_connection* pContext =
		(xrt_ws_server_route_connection*)pData;
	xrt_ws_server_route* pRoute = pContext->Route;

	if ( pRoute->Config.Events.Ping != NULL ) {
		pRoute->Config.Events.Ping(
			pConnection, Payload, pRoute->Config.Data
		);
	}
}



/* 转发 Pong 事件。 */
static void __xrtWsServerRoutePong(
	xwsconn* pConnection,
	xbytesview Payload,
	ptr pData
)
{
	xrt_ws_server_route_connection* pContext =
		(xrt_ws_server_route_connection*)pData;
	xrt_ws_server_route* pRoute = pContext->Route;

	if ( pRoute->Config.Events.Pong != NULL ) {
		pRoute->Config.Events.Pong(
			pConnection, Payload, pRoute->Config.Data
		);
	}
}



/* 转发高水位背压事件。 */
static void __xrtWsServerRouteBackpressure(
	xwsconn* pConnection,
	size_t iPending,
	ptr pData
)
{
	xrt_ws_server_route_connection* pContext =
		(xrt_ws_server_route_connection*)pData;
	xrt_ws_server_route* pRoute = pContext->Route;

	if ( pRoute->Config.Events.Backpressure != NULL ) {
		pRoute->Config.Events.Backpressure(
			pConnection, iPending, pRoute->Config.Data
		);
	}
}



/* 转发恢复可写事件。 */
static void __xrtWsServerRouteWritable(
	xwsconn* pConnection,
	size_t iPending,
	ptr pData
)
{
	xrt_ws_server_route_connection* pContext =
		(xrt_ws_server_route_connection*)pData;
	xrt_ws_server_route* pRoute = pContext->Route;

	if ( pRoute->Config.Events.Writable != NULL ) {
		pRoute->Config.Events.Writable(
			pConnection, iPending, pRoute->Config.Data
		);
	}
}



/* 转发发送队列排空事件。 */
static void __xrtWsServerRouteDrain(
	xwsconn* pConnection,
	ptr pData
)
{
	xrt_ws_server_route_connection* pContext =
		(xrt_ws_server_route_connection*)pData;
	xrt_ws_server_route* pRoute = pContext->Route;

	if ( pRoute->Config.Events.Drain != NULL ) {
		pRoute->Config.Events.Drain(
			pConnection, pRoute->Config.Data
		);
	}
}



/* 转发连接协议或传输错误。 */
static void __xrtWsServerRouteConnectionError(
	xwsconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	xrt_ws_server_route_connection* pContext =
		(xrt_ws_server_route_connection*)pData;
	xrt_ws_server_route* pRoute = pContext->Route;

	if ( pRoute->Config.Events.Error != NULL ) {
		pRoute->Config.Events.Error(
			pConnection, pError, pRoute->Config.Data
		);
	}
}



/* 转发唯一 Close，并在用户回调返回后释放连接持有的上下文。 */
static void __xrtWsServerRouteClose(
	xwsconn* pConnection,
	const xwsconnclose* pClose,
	ptr pData
)
{
	xrt_ws_server_route_connection* pContext =
		(xrt_ws_server_route_connection*)pData;
	xrt_ws_server_route* pRoute = pContext->Route;

	if ( pRoute->Config.Events.Close != NULL ) {
		pRoute->Config.Events.Close(
			pConnection, pClose, pRoute->Config.Data
		);
	}
	__xrtWsServerRouteConnectionRelease(pContext);
}



/* 把 Upgrade 终态转换为借用 Open 或 Error，并自动归还交付引用。 */
void __xrtWsServerRouteUpgradeDone(
	xhttpconn* pHttp,
	xnetresult Result,
	xwsconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	xrt_ws_server_route_connection* pContext =
		(xrt_ws_server_route_connection*)pData;
	xrt_ws_server_route* pRoute = pContext->Route;

	if ( (Result == XNET_RESULT_OK) &&
		(pConnection != NULL) ) {
		(void)xrtRefRetain(&pContext->References);
		if ( pRoute->Config.Open != NULL ) {
			pRoute->Config.Open(
				pHttp,
				pConnection,
				pRoute->Config.Data
			);
		}
		xrtWsConnDestroy(pConnection);
	} else if ( pRoute->Config.Error != NULL ) {
		pRoute->Config.Error(
			pHttp, pError, pRoute->Config.Data
		);
	}
	__xrtWsServerRouteConnectionRelease(pContext);
}



/* 固定适配表保证全部事件都恢复为调用方 Data。 */
const xwsconnevents __xrtWsServerRouteEvents = {
	.MessageBegin = __xrtWsServerRouteMessageBegin,
	.MessageData = __xrtWsServerRouteMessageData,
	.MessageEnd = __xrtWsServerRouteMessageEnd,
	.Ping = __xrtWsServerRoutePing,
	.Pong = __xrtWsServerRoutePong,
	.Backpressure = __xrtWsServerRouteBackpressure,
	.Writable = __xrtWsServerRouteWritable,
	.Drain = __xrtWsServerRouteDrain,
	.Error = __xrtWsServerRouteConnectionError,
	.Close = __xrtWsServerRouteClose
};

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xws-server/src/websocket/server_future.c */
/* ========================================================================== */

#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_FUTURE)



#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_FUTURE)

/*
	服务端桥在成功后原位转为 Future 拥有的结果。
	独立 HTTP 引用覆盖取消线程与 Upgrade 完成回调的竞态。
*/
typedef struct xrt_ws_server_future {
	xwsserverresult Result;
	xfuturebridge Bridge;
	xhttpconn* Http;
	xatomic32 Cancelled;
} xrt_ws_server_future;



/* Future 的拥有值析构只释放一个连接建立结果引用。 */
static void __xrtWsServerFutureFree(ptr pValue, ptr pData)
{
	(void)pData;
	xrtWsServerResultDestroy((xwsserverresult*)pValue);
}



/* 标记 Future 取消并异常关闭尚未完成的 HTTP Upgrade。 */
static void __xrtWsServerFutureCancel(ptr pData)
{
	xrt_ws_server_future* pContext =
		(xrt_ws_server_future*)pData;

	xrtAtomic32Store(
		&pContext->Cancelled,
		1,
		XMEMORY_RELEASE
	);
	(void)xrtHttpConnAbort(pContext->Http);
}



/* 接收 HTTP Upgrade 唯一终态并完成服务端 Future。 */
static void __xrtWsServerFutureDone(
	xhttpconn* pHttp,
	xnetresult Result,
	xwsconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	xrt_ws_server_future* pContext =
		(xrt_ws_server_future*)pData;
	xpromise* pPromise = xrtFutureBridgePromise(&pContext->Bridge);
	xhttpconn* pHeld = pContext->Http;
	xerror* pFallback = NULL;
	xerror* pFailure = NULL;
	bool bCancelled;
	bool bReady;
	bool bResolved = false;
	bool bSuccess;

	(void)pHttp;
	bReady = xrtFutureBridgeWait(&pContext->Bridge);
	xrtFutureBridgeUnwatch(&pContext->Bridge);
	bCancelled = xrtAtomic32Load(
		&pContext->Cancelled,
		XMEMORY_ACQUIRE
	) != 0;
	bSuccess = bReady && !bCancelled &&
		(Result == XNET_RESULT_OK) &&
		(pConnection != NULL);
	if ( !bReady ) {
		__xwsServerOpenConnectionDestroy(pConnection);
		xrtHttpConnDestroy(pHeld);
		xrtPromiseDestroy(pPromise);
		xrtFree(pContext);
		return;
	}
	if ( !bSuccess && !bCancelled &&
		(Result != XNET_RESULT_CANCELLED) ) {
		if ( pError == NULL ) {
			pFallback = __xwsServerOpenErrorCreate(
				XERR_INTERNAL,
				"complete-websocket-upgrade",
				Result == XNET_RESULT_OK ?
					"WebSocket Upgrade succeeded without a connection" :
					"WebSocket Upgrade failed without an error"
			);
			pError = pFallback != NULL ?
				pFallback : xrtGetError();
		}
		pFailure = xrtErrorRef(pError);
	}
	if ( bSuccess ) {
		xrtAtomicPtrStore(
			&pContext->Result.Connection,
			pConnection,
			XMEMORY_RELEASE
		);
	} else {
		__xwsServerOpenConnectionDestroy(pConnection);
		xrtFree(pContext);
	}
	xrtHttpConnDestroy(pHeld);
	if ( bSuccess ) {
		bResolved = xrtPromiseResolveOwned(
			pPromise,
			&pContext->Result,
			__xrtWsServerFutureFree,
			NULL
		);
		if ( !bResolved ) {
			xrtWsServerResultDestroy(&pContext->Result);
		}
	} else if ( bCancelled ||
		(Result == XNET_RESULT_CANCELLED) ) {
		(void)xrtPromiseCancel(pPromise);
	} else if ( pFailure != NULL ) {
		(void)xrtPromiseReject(pPromise, pFailure);
	} else {
		(void)xrtPromiseClose(pPromise);
	}
	xrtPromiseDestroy(pPromise);
	xrtErrorFree(pFailure);
	xrtErrorFree(pFallback);
}



/* 验证并提交服务端 WebSocket Upgrade Future。 */
XWS_SERVER_API xfuture* xrtWsUpgradeAsync(
	xhttpconn* pHttp,
	const xwsserverconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pData
)
{
	xrt_ws_server_future* pContext;
	xwsserverconfig Config;
	xwsconnevents Events;
	xfuture* pFuture;
	xerror* pError;

	if ( !__xwsServerServerConfigSnapshot(
		&Config,
		pConfig,
		"upgrade-websocket-async"
	) || !__xwsServerConnEventsSnapshot(
		&Events,
		pEvents,
		"upgrade-websocket-async"
	) || !__xwsServerHttpObjectCheck(
		pHttp,
		"upgrade-websocket-async",
		"HTTP server connection is null"
	) ) {
		return NULL;
	}
	pContext = (xrt_ws_server_future*)xrtCalloc(
		1,
		sizeof(*pContext)
	);
	if ( pContext == NULL ) {
		return NULL;
	}
	pContext->Result.References = 1;
	xrtAtomicPtrInit(&pContext->Result.Connection, NULL);
	xrtAtomic32Init(&pContext->Cancelled, 0);
	pContext->Http = xrtHttpConnRef(pHttp);
	if ( pContext->Http == NULL ) {
		xrtFree(pContext);
		return NULL;
	}
	pFuture = xrtFutureBridgeCreate(
		&pContext->Bridge,
		NULL
	);
	if ( pFuture == NULL ) {
		xrtHttpConnDestroy(pContext->Http);
		xrtFree(pContext);
		return NULL;
	}
	if ( xrtWsUpgrade(
		pHttp,
		&Config,
		&Events,
		pData,
		__xrtWsServerFutureDone,
		pContext
	) != XNET_RESULT_OK ) {
		xrtFutureDestroy(pFuture);
		xrtPromiseDestroy(xrtFutureBridgePromise(&pContext->Bridge));
		xrtHttpConnDestroy(pContext->Http);
		xrtFree(pContext);
		return NULL;
	}
	if ( !xrtFutureBridgeWatch(
		&pContext->Bridge,
		__xrtWsServerFutureCancel,
		pContext
	) ) {
		pError = xrtTakeError();
		xrtFutureBridgeFail(&pContext->Bridge);
		(void)xrtHttpConnAbort(pContext->Http);
		xrtFutureDestroy(pFuture);
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		return NULL;
	}
	xrtFutureBridgeReady(&pContext->Bridge);
	return pFuture;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xws-server/src/websocket/server_result.c */
/* ========================================================================== */

#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_FUTURE)

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
#endif

#endif
