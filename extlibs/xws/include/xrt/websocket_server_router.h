#ifndef XRT_WEBSOCKET_SERVER_ROUTER_H
#define XRT_WEBSOCKET_SERVER_ROUTER_H

#include <xrt/http_server_router.h>
#include <xrt/websocket_http.h>



#if defined(XWS_FEATURE_WEBSOCKET_SERVER_ROUTER) && \
	(!defined(XWS_FEATURE_WEBSOCKET_SERVER) || \
	 !defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER))
	#error "XRT WebSocket server router requires WebSocket server and HTTP server router"
#endif



#if defined(XWS_FEATURE_WEBSOCKET_SERVER_ROUTER)

/* WebSocket 服务端路由错误区分参数、配置、分配、注册、响应和运行时状态。 */
typedef enum xwsserverroutererror {
	XWS_SERVER_ROUTER_ERROR_ARGUMENT = 1,
	XWS_SERVER_ROUTER_ERROR_CONFIG,
	XWS_SERVER_ROUTER_ERROR_MEMORY,
	XWS_SERVER_ROUTER_ERROR_ROUTE,
	XWS_SERVER_ROUTER_ERROR_RESPONSE,
	XWS_SERVER_ROUTER_ERROR_STATE
} xwsserverroutererror;



/* Upgrade 成功后借用 Connection；需要跨回调保存时由用户显式增加引用。 */
typedef void (*xwsserverrouteopenproc)(
	xhttpconn* pHttp,
	xwsconn* pConnection,
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
	注册固定 WebSocket 路径；内部处理 OPTIONS、方法拒绝、握手拒绝和 Upgrade。
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
