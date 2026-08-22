#ifndef XRT_INTERNAL_WEBSOCKET_SERVER_ROUTER_H
#define XRT_INTERNAL_WEBSOCKET_SERVER_ROUTER_H

#include "xrt_websocket.h"

#include <xrt/memory.h>
#include <xrt/websocket_server_router.h>



#if defined(XWS_FEATURE_WEBSOCKET_SERVER_ROUTER)

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
