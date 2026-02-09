



/*
	NetUDP - UDP 服务器/客户端封装 [Ver1.0]
	
	基于 netsock.h (Socket 基础)
	使用 xrt 线程库 (thread.h) 创建事件循环线程
	
	仅 IPv4，跨平台 (Windows / Linux)
	
	与 TCP 不同，UDP 的 OnRecv 回调提供远端地址信息 (OnRecvFrom)
*/



/* ============================== UDP 服务器结构 ============================== */

struct xrt_udp_server {
	xnetconn tConn;                // UDP socket
	xnetconfig tConfig;            // 配置
	xnetevents tEvents;            // 事件回调
	
	// 线程
	xthread pThread;               // 事件循环线程
	volatile bool bRunning;        // 运行标志
	
	// 用户数据
	ptr pUserData;
};



/* ============================== UDP 客户端结构 ============================== */

struct xrt_udp_client {
	xnetconn tConn;                // UDP socket
	xnetconfig tConfig;            // 配置
	xnetevents tEvents;            // 事件回调
	
	// 线程
	xthread pThread;               // 事件循环线程
	volatile bool bRunning;        // 运行标志
	
	// 用户数据
	ptr pUserData;
};



/* ============================== UDP 服务器 - 事件循环 ============================== */

static uint32 __xrt_udp_server_loop(ptr pParam)
{
	xudpserver* pServer = (xudpserver*)pParam;
	char aBuf[8192];
	
	while ( pServer->bRunning ) {
		xnetaddr tFromAddr;
		size_t iRecvd = 0;
		xnet_result iRes = xrtSockRecvFrom(&pServer->tConn, aBuf, sizeof(aBuf), &tFromAddr, &iRecvd);
		
		if ( iRes == XRT_NET_OK && iRecvd > 0 ) {
			// UDP OnRecv 传递远端地址（通过 conn 的 tRemoteAddr 字段）
			pServer->tConn.tRemoteAddr = tFromAddr;
			if ( pServer->tEvents.OnRecv ) {
				pServer->tEvents.OnRecv(pServer, &pServer->tConn, aBuf, iRecvd);
			}
		} else if ( iRes == XRT_NET_AGAIN ) {
			// 无数据可读，短暂休眠
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

XXAPI xudpserver* xrtUdpServerCreate(const char* sIP, uint16 iPort,
	const xnetconfig* pConfig, const xnetevents* pEvents)
{
	xudpserver* pServer = (xudpserver*)xrtCalloc(1, sizeof(xudpserver));
	if ( !pServer ) return NULL;
	
	// 配置
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
	
	if ( xrtSockCreate(&pServer->tConn, 1) != XRT_NET_OK ) {  // UDP
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
	pServer->pThread = xrtThreadCreate((ptr)__xrt_udp_server_loop, pServer, 0);
	
	if ( !pServer->pThread ) {
		pServer->bRunning = false;
		return XRT_NET_ERROR;
	}
	
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



/* ============================== UDP 客户端 - 事件循环 ============================== */

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

XXAPI xudpclient* xrtUdpClientCreate(const xnetconfig* pConfig, const xnetevents* pEvents)
{
	xudpclient* pClient = (xudpclient*)xrtCalloc(1, sizeof(xudpclient));
	if ( !pClient ) return NULL;
	
	// 配置
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
	pClient->tConn.iType = 1;  // UDP
	
	if ( xrtSockCreate(&pClient->tConn, 1) != XRT_NET_OK ) {  // UDP
		xrtFree(pClient);
		return NULL;
	}
	
	xrtSockSetNonBlock(&pClient->tConn);
	
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
	pClient->pThread = xrtThreadCreate((ptr)__xrt_udp_client_loop, pClient, 0);
	
	if ( !pClient->pThread ) {
		pClient->bRunning = false;
		return XRT_NET_ERROR;
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


