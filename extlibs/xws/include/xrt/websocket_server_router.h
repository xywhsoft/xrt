#ifndef XRT_WEBSOCKET_SERVER_ROUTER_H
#define XRT_WEBSOCKET_SERVER_ROUTER_H

#include <xrt/http_origin.h>
#include <xrt/http_server_router.h>
#include <xrt/websocket_http.h>



#if defined(XWS_FEATURE_WEBSOCKET_SERVER_ROUTER) && \
	(!defined(XWS_FEATURE_WEBSOCKET_SERVER) || \
	 !defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER) || \
	 !defined(XHTTP_FEATURE_HTTP_ORIGIN))
	#error "XRT WebSocket server router requires WebSocket server, HTTP server router and HTTP Origin"
#endif



#if defined(XWS_FEATURE_WEBSOCKET_SERVER_ROUTER)

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



#if defined(XWS_FEATURE_WEBSOCKET_SERVER_ROUTER)

/*
	初始化默认 WebSocket 服务端配置、空连接事件和空生命周期回调。
	输出允许未对齐，但必须是完整且不回绕的固定结构地址范围。
*/
XRT_API void xrtWsServerRouteConfigInit(
	xwsserverrouteconfig* pConfig
);



/*
	注册固定 WebSocket 路径；内部处理方法、Origin、授权、握手和 Upgrade。
	成功后 Router 接管一次 Release(Data)，失败时 Data 所有权保持不变。
	Pattern 和配置内借用视图必须完整且不回绕；固定配置允许未对齐。
	函数在注册前只读取配置一次，并深复制子协议列表；后续修改调用方配置
	不会改变已注册端点。
*/
XRT_API bool xrtWsServerRoute(
	xhttpserverrouter* pRouter,
	xstrview Pattern,
	const xwsserverrouteconfig* pConfig
);

#endif



XRT_EXTERN_C_END

#endif
