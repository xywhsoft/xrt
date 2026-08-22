#include "../internal/xrt_websocket_server_router.h"



#if defined(XRT_FEATURE_WEBSOCKET_SERVER_ROUTER)

/* 增加活动 Upgrade 或 Connection 对固定路由快照的引用。 */
static bool __xrtWsServerRouteRef(
	xrt_ws_server_route* pRoute
)
{
	if ( xrtRefRetain(&pRoute->References) < 0 ) {
		__xrtErrorSetDetail(
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
