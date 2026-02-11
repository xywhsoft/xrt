



/*
	NetLoop - 事件循环 [Ver1.0]
	
	统一管理 IO 轮询器，驱动 TCP/UDP 服务器/客户端
	通过 __xrt_conn_handler 实现事件路由:
	  xnetconn->pUserData -> __xrt_conn_handler -> pfnHandler(pOwner, ...)
	
	多个服务器可共享同一个事件循环 (共享底层 poller)
*/



/* ============================== 事件路由处理器 ============================== */

// 连接事件处理器 (每个 TCP/UDP 服务器/客户端 注册自己的处理器)
typedef struct {
	void (*pfnHandler)(ptr pOwner, xnetconn* pConn, int iEvent, const char* pData, size_t iLen);
	ptr pOwner;    // 指向 TCP/UDP server 或 client 宿主
} __xrt_conn_handler;



/* ============================== 事件循环结构 ============================== */

struct xrt_event_loop {
	xnetpoller* pPoller;
	volatile bool bRunning;
	int iPollTimeoutMs;         // 默认 100ms
};



/* ============================== 事件路由回调 ============================== */

// poller 全局回调: 将事件路由到具体的 TCP/UDP 处理函数
static void __xrt_eventloop_dispatch(xnetpoller* pPoller, xnetconn* pConn,
	int iEvent, const char* pData, size_t iLen)
{
	if ( !pConn ) return;
	__xrt_conn_handler* pHandler = (__xrt_conn_handler*)pConn->pUserData;
	if ( pHandler && pHandler->pfnHandler ) {
		pHandler->pfnHandler(pHandler->pOwner, pConn, iEvent, pData, iLen);
	}
}



/* ============================== 事件循环 API ============================== */

XXAPI xeventloop* xrtEventLoopCreate()
{
	xeventloop* pLoop = (xeventloop*)xrtCalloc(1, sizeof(xeventloop));
	if ( !pLoop ) return NULL;
	
	pLoop->pPoller = xrtPollCreate(NULL, __xrt_eventloop_dispatch, pLoop);
	if ( !pLoop->pPoller ) {
		xrtFree(pLoop);
		return NULL;
	}
	
	pLoop->bRunning = false;
	pLoop->iPollTimeoutMs = 100;
	
	return pLoop;
}

XXAPI void xrtEventLoopDestroy(xeventloop* pLoop)
{
	if ( !pLoop ) return;
	
	pLoop->bRunning = false;
	if ( pLoop->pPoller ) {
		xrtPollDestroy(pLoop->pPoller);
		pLoop->pPoller = NULL;
	}
	xrtFree(pLoop);
}

XXAPI void xrtEventLoopRun(xeventloop* pLoop)
{
	if ( !pLoop ) return;
	pLoop->bRunning = true;
	while ( pLoop->bRunning ) {
		xrtPollWait(pLoop->pPoller, pLoop->iPollTimeoutMs);
	}
}

XXAPI void xrtEventLoopStop(xeventloop* pLoop)
{
	if ( !pLoop ) return;
	pLoop->bRunning = false;
	xrtPollWakeup(pLoop->pPoller);
}

XXAPI xnet_result xrtEventLoopRunOnce(xeventloop* pLoop, int iTimeoutMs)
{
	if ( !pLoop ) return XRT_NET_ERROR;
	return xrtPollWait(pLoop->pPoller, iTimeoutMs);
}

XXAPI xnetpoller* xrtEventLoopGetPoller(xeventloop* pLoop)
{
	if ( !pLoop ) return NULL;
	return pLoop->pPoller;
}

