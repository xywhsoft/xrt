



/*
	NetUDP - UDP 服务器/客户端封装 [Ver2.0]
	
	基于 netsock.h (Socket) + netpoll.h (IO) + netloop.h (事件循环)
	
	Ver2.0 变更:
	  - 事件驱动架构 (IOCP/io_uring)
	  - 双模式 API: 简易版 (内部线程) + 高级版 (共享事件循环)
	
	仅 IPv4，跨平台 (Windows / Linux)
	
	与 TCP 不同，UDP 无连接管理，通过 conn 的 tRemoteAddr 传递远端地址
*/



/* ============================== UDP 服务器结构 ============================== */

struct xrt_udp_server {
	xeventloop* pLoop;            // 事件循环 (自有或借用)
	bool bOwnLoop;
	xthread pThread;
	
	xnetconn tConn;                // UDP socket
	__xrt_conn_handler tHandler;
	xnetconfig tConfig;
	xnetevents tEvents;
	
	volatile bool bRunning;
	ptr pUserData;
};



/* ============================== UDP 客户端结构 ============================== */

struct xrt_udp_client {
	xeventloop* pLoop;            // 事件循环 (自有或借用)
	bool bOwnLoop;
	xthread pThread;
	
	xnetconn tConn;                // UDP socket
	__xrt_conn_handler tHandler;
	xnetconfig tConfig;
	xnetevents tEvents;
	
	volatile bool bRunning;
	ptr pUserData;
};



/* ============================== UDP 服务器 - 事件处理 ============================== */

// UDP 服务器事件处理
static void __xrt_udp_server_on_event(ptr pOwner, xnetconn* pConn, int iEvent, const char* pData, size_t iLen)
{
	xudpserver* pServer = (xudpserver*)pOwner;
	
	if ( iEvent == XRT_POLL_READ ) {
		// UDP 数据到达 - poller recv 提供数据，远端地址通过 recvfrom 获取
		// 注: 在 IOCP/io_uring 的当前实现中，UDP recv 使用标准 recv
		// 远端地址需要通过额外机制获取 (此处保持简单模式兼容)
		if ( pServer->tEvents.OnRecv ) {
			pServer->tEvents.OnRecv(pServer, &pServer->tConn, pData, iLen);
		}
		
		// 投递下一次 recv
		xrtPollPostRecv(xrtEventLoopGetPoller(pServer->pLoop), &pServer->tConn);
		return;
	}
	
	if ( iEvent == XRT_POLL_ERROR ) {
		if ( pServer->tEvents.OnError ) {
			pServer->tEvents.OnError(pServer, &pServer->tConn, -1);
		}
		return;
	}
}

// 服务器内部线程 (简易模式使用轮询接收, 更好的 UDP 远端地址支持)
static uint32 __xrt_udp_server_loop(ptr pParam)
{
	xudpserver* pServer = (xudpserver*)pParam;
	char aBuf[8192];
	
	while ( pServer->bRunning ) {
		xnetaddr tFromAddr;
		size_t iRecvd = 0;
		xnet_result iRes = xrtSockRecvFrom(&pServer->tConn, aBuf, sizeof(aBuf), &tFromAddr, &iRecvd);
		
		if ( iRes == XRT_NET_OK && iRecvd > 0 ) {
			pServer->tConn.tRemoteAddr = tFromAddr;
			if ( pServer->tEvents.OnRecv ) {
				pServer->tEvents.OnRecv(pServer, &pServer->tConn, aBuf, iRecvd);
			}
		} else if ( iRes == XRT_NET_AGAIN ) {
			int iTimeout = pServer->tConfig.iPollTimeoutMs > 0 ? pServer->tConfig.iPollTimeoutMs : 10;
			#if defined(_WIN32) || defined(_WIN64)
				Sleep(iTimeout);
			#else
				usleep(iTimeout * 1000);
			#endif
		} else if ( iRes == XRT_NET_CLOSED || iRes == XRT_NET_ERROR ) {
			if ( pServer->tEvents.OnError ) {
				pServer->tEvents.OnError(pServer, &pServer->tConn, (int)iRes);
			}
			break;
		}
	}
	
	return 0;
}



/* ============================== UDP 服务器 API ============================== */

// 高级版: 绑定共享事件循环
XXAPI xudpserver* xrtUdpServerCreateEx(xeventloop* pLoop, const char* sIP, uint16 iPort,
	const xnetconfig* pConfig, const xnetevents* pEvents)
{
	if ( !pLoop ) return NULL;
	
	xudpserver* pServer = (xudpserver*)xrtCalloc(1, sizeof(xudpserver));
	if ( !pServer ) return NULL;
	
	pServer->pLoop = pLoop;
	pServer->bOwnLoop = false;
	
	if ( pConfig ) {
		pServer->tConfig = *pConfig;
	} else {
		pServer->tConfig.iRecvBufSize = 8192;
		pServer->tConfig.iSendBufSize = 8192;
		pServer->tConfig.iMaxClients = 0;
		pServer->tConfig.iPollTimeoutMs = 10;
	}
	
	if ( pEvents ) {
		pServer->tEvents = *pEvents;
	}
	
	// 创建 UDP socket
	memset(&pServer->tConn, 0, sizeof(xnetconn));
	pServer->tConn.iType = 1;  // UDP
	
	if ( xrtSockCreate(&pServer->tConn, 1) != XRT_NET_OK ) {
		xrtFree(pServer);
		return NULL;
	}
	
	xrtSockSetReuseAddr(&pServer->tConn);
	xrtSockSetNonBlock(&pServer->tConn);
	
	xnetaddr tAddr;
	xrtNetAddrInit(&tAddr, sIP, iPort);
	
	if ( xrtSockBind(&pServer->tConn, &tAddr) != XRT_NET_OK ) {
		xrtSockClose(&pServer->tConn);
		xrtFree(pServer);
		return NULL;
	}
	
	// 设置事件处理器
	pServer->tHandler.pfnHandler = __xrt_udp_server_on_event;
	pServer->tHandler.pOwner = pServer;
	pServer->tConn.pUserData = &pServer->tHandler;
	
	// 注册到 poller
	xnetpoller* pPoller = xrtEventLoopGetPoller(pServer->pLoop);
	xrtPollAdd(pPoller, &pServer->tConn, XRT_POLL_READ);
	xrtPollPostRecv(pPoller, &pServer->tConn);
	
	return pServer;
}

// 简易版: 内部创建线程 (使用 recvfrom 轮询, 保留远端地址支持)
XXAPI xudpserver* xrtUdpServerCreate(const char* sIP, uint16 iPort,
	const xnetconfig* pConfig, const xnetevents* pEvents)
{
	xudpserver* pServer = (xudpserver*)xrtCalloc(1, sizeof(xudpserver));
	if ( !pServer ) return NULL;
	
	pServer->pLoop = NULL;  // 简易模式不使用事件循环
	pServer->bOwnLoop = true;
	
	if ( pConfig ) {
		pServer->tConfig = *pConfig;
	} else {
		pServer->tConfig.iRecvBufSize = 8192;
		pServer->tConfig.iSendBufSize = 8192;
		pServer->tConfig.iMaxClients = 0;
		pServer->tConfig.iPollTimeoutMs = 10;
	}
	
	if ( pEvents ) {
		pServer->tEvents = *pEvents;
	}
	
	// 创建 UDP socket
	memset(&pServer->tConn, 0, sizeof(xnetconn));
	pServer->tConn.iType = 1;
	
	if ( xrtSockCreate(&pServer->tConn, 1) != XRT_NET_OK ) {
		xrtFree(pServer);
		return NULL;
	}
	
	xrtSockSetReuseAddr(&pServer->tConn);
	xrtSockSetNonBlock(&pServer->tConn);
	
	xnetaddr tAddr;
	xrtNetAddrInit(&tAddr, sIP, iPort);
	
	if ( xrtSockBind(&pServer->tConn, &tAddr) != XRT_NET_OK ) {
		xrtSockClose(&pServer->tConn);
		xrtFree(pServer);
		return NULL;
	}
	
	return pServer;
}

XXAPI void xrtUdpServerDestroy(xudpserver* pServer)
{
	if ( !pServer ) return;
	
	xrtUdpServerStop(pServer);
	xrtSockClose(&pServer->tConn);
	xrtFree(pServer);
}

XXAPI xnet_result xrtUdpServerStart(xudpserver* pServer)
{
	if ( !pServer ) return XRT_NET_ERROR;
	if ( pServer->bRunning ) return XRT_NET_OK;
	
	pServer->bRunning = true;
	
	if ( pServer->bOwnLoop && !pServer->pLoop ) {
		// 简易模式: 使用 recvfrom 轮询线程
		pServer->pThread = xrtThreadCreate((ptr)__xrt_udp_server_loop, pServer, 0);
		if ( !pServer->pThread ) {
			pServer->bRunning = false;
			return XRT_NET_ERROR;
		}
	}
	// 高级模式: 事件由 EventLoopRun 驱动
	
	return XRT_NET_OK;
}

XXAPI void xrtUdpServerStop(xudpserver* pServer)
{
	if ( !pServer || !pServer->bRunning ) return;
	
	pServer->bRunning = false;
	
	if ( pServer->pThread ) {
		xrtThreadWait(pServer->pThread);
		xrtThreadDestroy(pServer->pThread);
		pServer->pThread = NULL;
	}
}

XXAPI xnet_result xrtUdpServerSendTo(xudpserver* pServer, const xnetaddr* pAddr,
	const char* pData, size_t iLen)
{
	if ( !pServer || !pAddr || !pData || iLen == 0 ) return XRT_NET_ERROR;
	
	size_t iSent = 0;
	return xrtSockSendTo(&pServer->tConn, pData, iLen, pAddr, &iSent);
}

XXAPI void xrtUdpServerSetUserData(xudpserver* pServer, ptr pData)
{
	if ( pServer ) pServer->pUserData = pData;
}

XXAPI ptr xrtUdpServerGetUserData(xudpserver* pServer)
{
	return pServer ? pServer->pUserData : NULL;
}



/* ============================== UDP 客户端 - 事件处理 ============================== */

static void __xrt_udp_client_on_event(ptr pOwner, xnetconn* pConn, int iEvent, const char* pData, size_t iLen)
{
	xudpclient* pClient = (xudpclient*)pOwner;
	
	if ( iEvent == XRT_POLL_READ ) {
		if ( pClient->tEvents.OnRecv ) {
			pClient->tEvents.OnRecv(pClient, &pClient->tConn, pData, iLen);
		}
		xrtPollPostRecv(xrtEventLoopGetPoller(pClient->pLoop), &pClient->tConn);
		return;
	}
	
	if ( iEvent == XRT_POLL_ERROR ) {
		if ( pClient->tEvents.OnError ) {
			pClient->tEvents.OnError(pClient, &pClient->tConn, -1);
		}
		return;
	}
}

// 简易模式轮询线程
static uint32 __xrt_udp_client_loop(ptr pParam)
{
	xudpclient* pClient = (xudpclient*)pParam;
	char aBuf[8192];
	
	while ( pClient->bRunning ) {
		xnetaddr tFromAddr;
		size_t iRecvd = 0;
		xnet_result iRes = xrtSockRecvFrom(&pClient->tConn, aBuf, sizeof(aBuf), &tFromAddr, &iRecvd);
		
		if ( iRes == XRT_NET_OK && iRecvd > 0 ) {
			pClient->tConn.tRemoteAddr = tFromAddr;
			if ( pClient->tEvents.OnRecv ) {
				pClient->tEvents.OnRecv(pClient, &pClient->tConn, aBuf, iRecvd);
			}
		} else if ( iRes == XRT_NET_AGAIN ) {
			int iTimeout = pClient->tConfig.iPollTimeoutMs > 0 ? pClient->tConfig.iPollTimeoutMs : 10;
			#if defined(_WIN32) || defined(_WIN64)
				Sleep(iTimeout);
			#else
				usleep(iTimeout * 1000);
			#endif
		} else if ( iRes == XRT_NET_CLOSED || iRes == XRT_NET_ERROR ) {
			if ( pClient->tEvents.OnError ) {
				pClient->tEvents.OnError(pClient, &pClient->tConn, (int)iRes);
			}
			break;
		}
	}
	
	return 0;
}



/* ============================== UDP 客户端 API ============================== */

// 高级版
XXAPI xudpclient* xrtUdpClientCreateEx(xeventloop* pLoop, const xnetconfig* pConfig, const xnetevents* pEvents)
{
	if ( !pLoop ) return NULL;
	
	xudpclient* pClient = (xudpclient*)xrtCalloc(1, sizeof(xudpclient));
	if ( !pClient ) return NULL;
	
	pClient->pLoop = pLoop;
	pClient->bOwnLoop = false;
	
	if ( pConfig ) {
		pClient->tConfig = *pConfig;
	} else {
		pClient->tConfig.iRecvBufSize = 8192;
		pClient->tConfig.iSendBufSize = 8192;
		pClient->tConfig.iMaxClients = 0;
		pClient->tConfig.iPollTimeoutMs = 10;
	}
	
	if ( pEvents ) {
		pClient->tEvents = *pEvents;
	}
	
	// 创建 UDP socket
	memset(&pClient->tConn, 0, sizeof(xnetconn));
	pClient->tConn.iType = 1;
	
	if ( xrtSockCreate(&pClient->tConn, 1) != XRT_NET_OK ) {
		xrtFree(pClient);
		return NULL;
	}
	
	xrtSockSetNonBlock(&pClient->tConn);
	
	// 设置事件处理器
	pClient->tHandler.pfnHandler = __xrt_udp_client_on_event;
	pClient->tHandler.pOwner = pClient;
	pClient->tConn.pUserData = &pClient->tHandler;
	
	// 注册到 poller
	xnetpoller* pPoller = xrtEventLoopGetPoller(pClient->pLoop);
	xrtPollAdd(pPoller, &pClient->tConn, XRT_POLL_READ);
	xrtPollPostRecv(pPoller, &pClient->tConn);
	
	return pClient;
}

// 简易版
XXAPI xudpclient* xrtUdpClientCreate(const xnetconfig* pConfig, const xnetevents* pEvents)
{
	xudpclient* pClient = (xudpclient*)xrtCalloc(1, sizeof(xudpclient));
	if ( !pClient ) return NULL;
	
	pClient->pLoop = NULL;
	pClient->bOwnLoop = true;
	
	if ( pConfig ) {
		pClient->tConfig = *pConfig;
	} else {
		pClient->tConfig.iRecvBufSize = 8192;
		pClient->tConfig.iSendBufSize = 8192;
		pClient->tConfig.iMaxClients = 0;
		pClient->tConfig.iPollTimeoutMs = 10;
	}
	
	if ( pEvents ) {
		pClient->tEvents = *pEvents;
	}
	
	// 创建 UDP socket
	memset(&pClient->tConn, 0, sizeof(xnetconn));
	pClient->tConn.iType = 1;
	
	if ( xrtSockCreate(&pClient->tConn, 1) != XRT_NET_OK ) {
		xrtFree(pClient);
		return NULL;
	}
	
	xrtSockSetNonBlock(&pClient->tConn);
	
	// 绑定到 0.0.0.0:0 让系统分配随机端口
	// 必须在 Start 之前绑定，否则 recvfrom 在 Windows 上返回 WSAEINVAL 导致轮询线程退出
	xnetaddr tBindAddr;
	xrtNetAddrInit(&tBindAddr, "0.0.0.0", 0);
	xrtSockBind(&pClient->tConn, &tBindAddr);
	
	return pClient;
}

XXAPI void xrtUdpClientDestroy(xudpclient* pClient)
{
	if ( !pClient ) return;
	
	xrtUdpClientStop(pClient);
	xrtSockClose(&pClient->tConn);
	xrtFree(pClient);
}

XXAPI xnet_result xrtUdpClientStart(xudpclient* pClient)
{
	if ( !pClient ) return XRT_NET_ERROR;
	if ( pClient->bRunning ) return XRT_NET_OK;
	
	pClient->bRunning = true;
	
	if ( pClient->bOwnLoop && !pClient->pLoop ) {
		// 简易模式: recvfrom 轮询线程
		pClient->pThread = xrtThreadCreate((ptr)__xrt_udp_client_loop, pClient, 0);
		if ( !pClient->pThread ) {
			pClient->bRunning = false;
			return XRT_NET_ERROR;
		}
	}
	
	return XRT_NET_OK;
}

XXAPI void xrtUdpClientStop(xudpclient* pClient)
{
	if ( !pClient || !pClient->bRunning ) return;
	
	pClient->bRunning = false;
	
	if ( pClient->pThread ) {
		xrtThreadWait(pClient->pThread);
		xrtThreadDestroy(pClient->pThread);
		pClient->pThread = NULL;
	}
}

XXAPI xnet_result xrtUdpClientSendTo(xudpclient* pClient, const xnetaddr* pAddr,
	const char* pData, size_t iLen)
{
	if ( !pClient || !pAddr || !pData || iLen == 0 ) return XRT_NET_ERROR;
	
	size_t iSent = 0;
	return xrtSockSendTo(&pClient->tConn, pData, iLen, pAddr, &iSent);
}

XXAPI void xrtUdpClientSetUserData(xudpclient* pClient, ptr pData)
{
	if ( pClient ) pClient->pUserData = pData;
}

XXAPI ptr xrtUdpClientGetUserData(xudpclient* pClient)
{
	return pClient ? pClient->pUserData : NULL;
}

