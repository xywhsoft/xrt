



/*
	NetTCP - TCP 服务器/客户端封装 [Ver2.0]
	
	基于 netsock.h (Socket) + netpoll.h (IO) + netloop.h (事件循环) + nettls.h (TLS)
	
	Ver2.0 变更:
	  - 事件驱动架构 (IOCP/io_uring 完成端口模型)
	  - 双模式 API: 简易版 (内部线程) + 高级版 (共享事件循环)
	  - 客户端槽位空闲栈 O(1) 分配/回收
	  - 环形接收缓冲区
	  - TLS 握手事件驱动化 (依赖 poller recv 事件)
	
	仅 IPv4，跨平台 (Windows / Linux)
*/



/* ============================== 内部客户端数据结构 ============================== */

typedef struct {
	xnetconn tConn;
	xnetringbuf tRecvRing;         // 环形接收缓冲区
	xtlsctx* pTlsCtx;
	__xrt_conn_handler tHandler;   // 事件路由
	bool bInUse;
	bool bTlsHandshaking;         // TLS 握手中
} __xrt_tcp_client_slot;



/* ============================== TCP 服务器结构 ============================== */

struct xrt_tcp_server {
	xeventloop* pLoop;            // 事件循环 (自有或借用)
	bool bOwnLoop;                // 是否拥有 event loop
	xthread pThread;              // 简易模式内部线程
	
	xnetconn tListenConn;
	__xrt_conn_handler tListenHandler;
	xnetconn tAcceptConn;          // 预分配 accept 连接缓冲区
	xnetconfig tConfig;
	xnetevents tEvents;
	xtlsconfig tTlsConfig;
	bool bTlsEnabled;
	
	// 客户端管理 (空闲索引栈)
	__xrt_tcp_client_slot* arrSlots;
	int iMaxClients;
	int iClientCount;
	int* arrFreeStack;             // 空闲索引栈
	int iFreeTop;                  // 栈顶 (-1=空)
	
	ptr pUserData;
	volatile bool bRunning;
};



/* ============================== TCP 客户端结构 ============================== */

struct xrt_tcp_client {
	xeventloop* pLoop;            // 事件循环 (自有或借用)
	bool bOwnLoop;
	xthread pThread;
	
	xnetconn tConn;
	__xrt_conn_handler tHandler;
	xnetconfig tConfig;
	xnetevents tEvents;
	xtlsconfig tTlsConfig;
	xtlsctx* pTlsCtx;
	bool bTlsEnabled;
	bool bTlsHandshaking;
	
	xnetaddr tServerAddr;
	
	volatile bool bRunning;
	volatile bool bConnected;
	
	ptr pUserData;
};



/* ============================== TCP 服务器 - 槽位管理 ============================== */

// 从空闲栈 pop 槽位 O(1)
static int __xrt_tcp_server_alloc_slot(xtcpserver* pServer)
{
	if ( pServer->iFreeTop < 0 ) return -1;
	int iSlot = pServer->arrFreeStack[pServer->iFreeTop];
	pServer->iFreeTop--;
	return iSlot;
}

// 归还槽位到空闲栈 O(1)
static void __xrt_tcp_server_free_slot(xtcpserver* pServer, int iSlot)
{
	pServer->iFreeTop++;
	pServer->arrFreeStack[pServer->iFreeTop] = iSlot;
}



/* ============================== TCP 服务器 - 事件处理 ============================== */

// Forward declarations
static void __xrt_tcp_server_on_event(ptr pOwner, xnetconn* pConn, int iEvent, const char* pData, size_t iLen);
static void __xrt_tcp_client_slot_on_event(ptr pOwner, xnetconn* pConn, int iEvent, const char* pData, size_t iLen);

// 客户端槽位 事件处理 (READ/WRITE/CLOSE)
static void __xrt_tcp_client_slot_on_event(ptr pOwner, xnetconn* pConn, int iEvent, const char* pData, size_t iLen)
{
	xtcpserver* pServer = (xtcpserver*)pOwner;
	int iSlot = pConn->iId;
	if ( iSlot < 0 || iSlot >= pServer->iMaxClients ) return;
	__xrt_tcp_client_slot* pSlot = &pServer->arrSlots[iSlot];
	if ( !pSlot->bInUse ) return;
	
	xnetpoller* pPoller = xrtEventLoopGetPoller(pServer->pLoop);
	
	if ( iEvent == XRT_POLL_READ ) {
		// TLS 握手进行中
		if ( pSlot->bTlsHandshaking && pSlot->pTlsCtx ) {
			// 将 poller 收到的数据送入 TLS 接收缓冲区
			xrtNetBufAppend(&pSlot->pTlsCtx->tRecvBuf, pData, iLen);
			xnet_result iRes = xrtTlsHandshake(pSlot->pTlsCtx, &pSlot->tConn);
			
			// 将 TLS 输出通过 poller 发送
			if ( pSlot->pTlsCtx->tSendBuf.iSize > 0 ) {
				xrtPollPostSend(pPoller, &pSlot->tConn,
					pSlot->pTlsCtx->tSendBuf.pData, pSlot->pTlsCtx->tSendBuf.iSize);
				xrtNetBufConsume(&pSlot->pTlsCtx->tSendBuf, pSlot->pTlsCtx->tSendBuf.iSize);
			}
			
			if ( iRes == XRT_NET_OK ) {
				// 握手完成
				pSlot->bTlsHandshaking = false;
				pSlot->tConn.bTlsEnabled = true;
				pSlot->tConn.pTlsCtx = pSlot->pTlsCtx;
			} else if ( iRes == XRT_NET_ERROR ) {
				// 握手失败，关闭连接
				goto close_slot;
			}
			// XRT_NET_AGAIN: 等待更多数据
			xrtPollPostRecv(pPoller, &pSlot->tConn);
			return;
		}
		
		// 正常数据
		if ( pSlot->tConn.bTlsEnabled && pSlot->pTlsCtx ) {
			// TLS: 将原始数据送入 TLS 解密
			xrtNetBufAppend(&pSlot->pTlsCtx->tRecvBuf, pData, iLen);
			char aBuf[8192];
			size_t iDecrypted = 0;
			while ( xrtTlsRead(pSlot->pTlsCtx, aBuf, sizeof(aBuf), &iDecrypted) == XRT_NET_OK && iDecrypted > 0 ) {
				if ( pServer->tEvents.OnRecv ) {
					pServer->tEvents.OnRecv(pServer, &pSlot->tConn, aBuf, iDecrypted);
				}
				iDecrypted = 0;
			}
		} else {
			// 明文数据
			if ( pServer->tEvents.OnRecv ) {
				pServer->tEvents.OnRecv(pServer, &pSlot->tConn, pData, iLen);
			}
		}
		
		// 投递下一次 recv
		xrtPollPostRecv(pPoller, &pSlot->tConn);
		return;
	}
	
	if ( iEvent == XRT_POLL_WRITE ) {
		if ( pServer->tEvents.OnSend ) {
			pServer->tEvents.OnSend(pServer, &pSlot->tConn, iLen);
		}
		return;
	}
	
	if ( iEvent == XRT_POLL_CLOSE || iEvent == XRT_POLL_ERROR ) {
close_slot:
		if ( pServer->tEvents.OnClose ) {
			pServer->tEvents.OnClose(pServer, &pSlot->tConn);
		}
		
		xrtPollRemove(pPoller, &pSlot->tConn);
		
		if ( pSlot->pTlsCtx ) {
			xrtTlsDestroy(pSlot->pTlsCtx);
			pSlot->pTlsCtx = NULL;
		}
		xrtSockClose(&pSlot->tConn);
		xrtNetRingBufFree(&pSlot->tRecvRing);
		pSlot->bInUse = false;
		pSlot->bTlsHandshaking = false;
		pServer->iClientCount--;
		__xrt_tcp_server_free_slot(pServer, iSlot);
		return;
	}
}

// 服务器监听 事件处理 (ACCEPT)
static void __xrt_tcp_server_on_event(ptr pOwner, xnetconn* pConn, int iEvent, const char* pData, size_t iLen)
{
	xtcpserver* pServer = (xtcpserver*)pOwner;
	
	if ( iEvent != XRT_POLL_ACCEPT ) return;
	
	xnetpoller* pPoller = xrtEventLoopGetPoller(pServer->pLoop);
	
	// 分配槽位
	int iSlot = __xrt_tcp_server_alloc_slot(pServer);
	if ( iSlot < 0 ) {
		// 无空闲槽位，拒绝连接
		xrtSockClose(pConn);
		// 继续投递 accept
		memset(&pServer->tAcceptConn, 0, sizeof(xnetconn));
		xrtPollPostAccept(pPoller, &pServer->tListenConn, &pServer->tAcceptConn);
		return;
	}
	
	__xrt_tcp_client_slot* pSlot = &pServer->arrSlots[iSlot];
	
	// 拷贝 accept 连接信息
	pSlot->tConn = *pConn;
	pSlot->tConn.iId = iSlot;
	pSlot->tConn.iType = 0;  // TCP
	pSlot->bInUse = true;
	pSlot->bTlsHandshaking = false;
	pSlot->pTlsCtx = NULL;
	
	// 设置事件处理器
	pSlot->tHandler.pfnHandler = __xrt_tcp_client_slot_on_event;
	pSlot->tHandler.pOwner = pServer;
	pSlot->tConn.pUserData = &pSlot->tHandler;
	
	// 初始化环形缓冲区
	size_t iRecvSize = pServer->tConfig.iRecvBufSize > 0 ? pServer->tConfig.iRecvBufSize : 8192;
	xrtNetRingBufInit(&pSlot->tRecvRing, iRecvSize);
	
	// 设置非阻塞
	xrtSockSetNonBlock(&pSlot->tConn);
	
	pServer->iClientCount++;
	
	// 注册到 poller
	xrtPollAdd(pPoller, &pSlot->tConn, XRT_POLL_READ);
	
	// Accept后设置TCP_NODELAY（如果配置启用）
	if ( pServer->tConfig.bNoDelay ) {
		xrtSockSetNoDelay(&pSlot->tConn);
	}
	
	// TLS 握手 (如果启用)
	if ( pServer->bTlsEnabled ) {
		pSlot->pTlsCtx = xrtTlsCreate(&pServer->tTlsConfig, true);
		if ( pSlot->pTlsCtx ) {
			pSlot->bTlsHandshaking = true;
		}
	}
	
	// 触发 OnAccept 回调
	if ( pServer->tEvents.OnAccept ) {
		pServer->tEvents.OnAccept(pServer, &pSlot->tConn);
	}
	
	// 投递 recv
	xrtPollPostRecv(pPoller, &pSlot->tConn);
	
	// 投递下一个 accept
	memset(&pServer->tAcceptConn, 0, sizeof(xnetconn));
	xrtPollPostAccept(pPoller, &pServer->tListenConn, &pServer->tAcceptConn);
}

// 服务器内部线程 (简易模式)
static uint32 __xrt_tcp_server_thread(ptr pParam)
{
	xtcpserver* pServer = (xtcpserver*)pParam;
	xrtEventLoopRun(pServer->pLoop);
	return 0;
}



/* ============================== TCP 服务器 API ============================== */

// 内部初始化 (Ex 和普通版共用)
static xtcpserver* __xrt_tcp_server_init(xeventloop* pLoop, const char* sIP, uint16 iPort,
	const xnetconfig* pConfig, const xnetevents* pEvents)
{
	xtcpserver* pServer = (xtcpserver*)xrtCalloc(1, sizeof(xtcpserver));
	if ( !pServer ) return NULL;
	
	pServer->pLoop = pLoop;
	
	// 配置
	if ( pConfig ) {
		pServer->tConfig = *pConfig;
	} else {
		pServer->tConfig.iRecvBufSize = 8192;
		pServer->tConfig.iSendBufSize = 8192;
		pServer->tConfig.iMaxClients = 256;
		pServer->tConfig.iPollTimeoutMs = 10;
	}
	
	if ( pEvents ) {
		pServer->tEvents = *pEvents;
	}
	
	pServer->iMaxClients = pServer->tConfig.iMaxClients > 0 ? pServer->tConfig.iMaxClients : 256;
	
	// 分配客户端槽位数组 (连续内存)
	pServer->arrSlots = (__xrt_tcp_client_slot*)xrtCalloc(pServer->iMaxClients, sizeof(__xrt_tcp_client_slot));
	if ( !pServer->arrSlots ) {
		xrtFree(pServer);
		return NULL;
	}
	
	// 分配空闲索引栈
	pServer->arrFreeStack = (int*)xrtMalloc(pServer->iMaxClients * sizeof(int));
	if ( !pServer->arrFreeStack ) {
		xrtFree(pServer->arrSlots);
		xrtFree(pServer);
		return NULL;
	}
	
	// 初始化空闲栈 (倒序压入, 使得 slot 0 先被分配)
	pServer->iFreeTop = pServer->iMaxClients - 1;
	for ( int i = 0; i < pServer->iMaxClients; i++ ) {
		pServer->arrFreeStack[i] = pServer->iMaxClients - 1 - i;
		pServer->arrSlots[i].bInUse = false;
		pServer->arrSlots[i].tConn.iId = i;
	}
	
	// 创建监听 socket
	memset(&pServer->tListenConn, 0, sizeof(xnetconn));
	if ( xrtSockCreate(&pServer->tListenConn, 0) != XRT_NET_OK ) {
		goto error_cleanup;
	}
	
	xrtSockSetReuseAddr(&pServer->tListenConn);
	xrtSockSetNonBlock(&pServer->tListenConn);
	
	xnetaddr tAddr;
	xrtNetAddrInit(&tAddr, sIP, iPort);
	
	if ( xrtSockBind(&pServer->tListenConn, &tAddr) != XRT_NET_OK ) {
		goto error_cleanup;
	}
	
	if ( xrtSockListen(&pServer->tListenConn, SOMAXCONN) != XRT_NET_OK ) {
		goto error_cleanup;
	}
	
	// 设置监听 handler
	pServer->tListenHandler.pfnHandler = __xrt_tcp_server_on_event;
	pServer->tListenHandler.pOwner = pServer;
	pServer->tListenConn.pUserData = &pServer->tListenHandler;
	
	// 注册监听 socket 到 poller
	xnetpoller* pPoller = xrtEventLoopGetPoller(pServer->pLoop);
	xrtPollAdd(pPoller, &pServer->tListenConn, XRT_POLL_ACCEPT);
	
	// 投递第一个异步 accept
	memset(&pServer->tAcceptConn, 0, sizeof(xnetconn));
	xrtPollPostAccept(pPoller, &pServer->tListenConn, &pServer->tAcceptConn);
	
	return pServer;

error_cleanup:
	xrtSockClose(&pServer->tListenConn);
	xrtFree(pServer->arrFreeStack);
	xrtFree(pServer->arrSlots);
	xrtFree(pServer);
	return NULL;
}

// 高级版: 绑定共享事件循环
XXAPI xtcpserver* xrtTcpServerCreateEx(xeventloop* pLoop, const char* sIP, uint16 iPort,
	const xnetconfig* pConfig, const xnetevents* pEvents)
{
	if ( !pLoop ) return NULL;
	
	xtcpserver* pServer = __xrt_tcp_server_init(pLoop, sIP, iPort, pConfig, pEvents);
	if ( !pServer ) return NULL;
	
	pServer->bOwnLoop = false;
	return pServer;
}

// 简易版: 内部创建事件循环 + 线程
XXAPI xtcpserver* xrtTcpServerCreate(const char* sIP, uint16 iPort,
	const xnetconfig* pConfig, const xnetevents* pEvents)
{
	xeventloop* pLoop = xrtEventLoopCreate();
	if ( !pLoop ) return NULL;
	
	xtcpserver* pServer = __xrt_tcp_server_init(pLoop, sIP, iPort, pConfig, pEvents);
	if ( !pServer ) {
		xrtEventLoopDestroy(pLoop);
		return NULL;
	}
	
	pServer->bOwnLoop = true;
	return pServer;
}

XXAPI void xrtTcpServerDestroy(xtcpserver* pServer)
{
	if ( !pServer ) return;
	
	xrtTcpServerStop(pServer);
	
	// 关闭所有客户端
	for ( int i = 0; i < pServer->iMaxClients; i++ ) {
		__xrt_tcp_client_slot* pSlot = &pServer->arrSlots[i];
		if ( pSlot->bInUse ) {
			if ( pSlot->pTlsCtx ) {
				xrtTlsDestroy(pSlot->pTlsCtx);
				pSlot->pTlsCtx = NULL;
			}
			xrtSockClose(&pSlot->tConn);
			xrtNetRingBufFree(&pSlot->tRecvRing);
		}
	}
	
	// 关闭监听 socket
	xrtSockClose(&pServer->tListenConn);
	
	// 释放资源
	xrtFree(pServer->arrFreeStack);
	xrtFree(pServer->arrSlots);
	
	if ( pServer->bOwnLoop && pServer->pLoop ) {
		xrtEventLoopDestroy(pServer->pLoop);
	}
	
	xrtFree(pServer);
}

XXAPI xnet_result xrtTcpServerStart(xtcpserver* pServer)
{
	if ( !pServer ) return XRT_NET_ERROR;
	if ( pServer->bRunning ) return XRT_NET_OK;
	
	pServer->bRunning = true;
	
	// 简易模式: 启动内部线程驱动事件循环
	if ( pServer->bOwnLoop ) {
		pServer->pThread = xrtThreadCreate((ptr)__xrt_tcp_server_thread, pServer, 0);
		if ( !pServer->pThread ) {
			pServer->bRunning = false;
			return XRT_NET_ERROR;
		}
	}
	// 高级模式: 事件循环由用户的 EventLoopRun 驱动
	
	return XRT_NET_OK;
}

XXAPI void xrtTcpServerStop(xtcpserver* pServer)
{
	if ( !pServer || !pServer->bRunning ) return;
	
	pServer->bRunning = false;
	
	if ( pServer->bOwnLoop && pServer->pLoop ) {
		xrtEventLoopStop(pServer->pLoop);
	}
	
	if ( pServer->pThread ) {
		xrtThreadWait(pServer->pThread);
		xrtThreadDestroy(pServer->pThread);
		pServer->pThread = NULL;
	}
	
	// 关闭所有客户端连接
	for ( int i = 0; i < pServer->iMaxClients; i++ ) {
		__xrt_tcp_client_slot* pSlot = &pServer->arrSlots[i];
		if ( pSlot->bInUse ) {
			// Stop前先PollRemove
			xnetpoller* pPoller = xrtEventLoopGetPoller(pServer->pLoop);
			xrtPollRemove(pPoller, &pSlot->tConn);
			
			// Graceful Shutdown
			#if defined(_WIN32) || defined(_WIN64)
				shutdown(pSlot->tConn.hSocket, SD_SEND);
			#else
				shutdown(pSlot->tConn.hSocket, SHUT_WR);
			#endif
			
			if ( pServer->tEvents.OnClose ) {
				pServer->tEvents.OnClose(pServer, &pSlot->tConn);
			}
			if ( pSlot->pTlsCtx ) {
				xrtTlsDestroy(pSlot->pTlsCtx);
				pSlot->pTlsCtx = NULL;
			}
			xrtSockClose(&pSlot->tConn);
			xrtNetRingBufFree(&pSlot->tRecvRing);
			pSlot->bInUse = false;
		}
	}
	pServer->iClientCount = 0;
	
	// 重建空闲栈
	pServer->iFreeTop = pServer->iMaxClients - 1;
	for ( int i = 0; i < pServer->iMaxClients; i++ ) {
		pServer->arrFreeStack[i] = pServer->iMaxClients - 1 - i;
	}
}

XXAPI xnet_result xrtTcpServerSend(xtcpserver* pServer, int iClientId,
	const char* pData, size_t iLen)
{
	if ( !pServer || !pData || iLen == 0 ) return XRT_NET_ERROR;
	if ( iClientId < 0 || iClientId >= pServer->iMaxClients ) return XRT_NET_ERROR;
	
	__xrt_tcp_client_slot* pSlot = &pServer->arrSlots[iClientId];
	if ( !pSlot->bInUse ) return XRT_NET_ERROR;
	
	xnetpoller* pPoller = xrtEventLoopGetPoller(pServer->pLoop);
	
	if ( pSlot->tConn.bTlsEnabled && pSlot->pTlsCtx ) {
		size_t iWritten = 0;
		xnet_result iRes = xrtTlsWrite(pSlot->pTlsCtx, pData, iLen, &iWritten);
		if ( iRes != XRT_NET_OK ) return iRes;
		// 通过 poller 发送加密数据
		if ( pSlot->pTlsCtx->tSendBuf.iSize > 0 ) {
			xrtPollPostSend(pPoller, &pSlot->tConn,
				pSlot->pTlsCtx->tSendBuf.pData, pSlot->pTlsCtx->tSendBuf.iSize);
			xrtNetBufConsume(&pSlot->pTlsCtx->tSendBuf, pSlot->pTlsCtx->tSendBuf.iSize);
		}
		return XRT_NET_OK;
	} else {
		return xrtPollPostSend(pPoller, &pSlot->tConn, pData, iLen);
	}
}

XXAPI void xrtTcpServerDisconnect(xtcpserver* pServer, int iClientId)
{
	if ( !pServer ) return;
	if ( iClientId < 0 || iClientId >= pServer->iMaxClients ) return;
	
	__xrt_tcp_client_slot* pSlot = &pServer->arrSlots[iClientId];
	if ( !pSlot->bInUse ) return;
	
	if ( pServer->tEvents.OnClose ) {
		pServer->tEvents.OnClose(pServer, &pSlot->tConn);
	}
	
	xnetpoller* pPoller = xrtEventLoopGetPoller(pServer->pLoop);
	xrtPollRemove(pPoller, &pSlot->tConn);
	
	if ( pSlot->pTlsCtx ) {
		xrtTlsClose(pSlot->pTlsCtx);
		xrtTlsDestroy(pSlot->pTlsCtx);
		pSlot->pTlsCtx = NULL;
	}
	
	xrtSockClose(&pSlot->tConn);
	xrtNetRingBufFree(&pSlot->tRecvRing);
	pSlot->bInUse = false;
	pSlot->bTlsHandshaking = false;
	pServer->iClientCount--;
	__xrt_tcp_server_free_slot(pServer, iClientId);
}

XXAPI xnet_result xrtTcpServerEnableTLS(xtcpserver* pServer, const xtlsconfig* pConfig)
{
	if ( !pServer || !pConfig ) return XRT_NET_ERROR;
	pServer->tTlsConfig = *pConfig;
	pServer->bTlsEnabled = true;
	return XRT_NET_OK;
}

XXAPI int xrtTcpServerGetClientCount(xtcpserver* pServer)
{
	return pServer ? pServer->iClientCount : 0;
}

XXAPI void xrtTcpServerSetUserData(xtcpserver* pServer, ptr pData)
{
	if ( pServer ) pServer->pUserData = pData;
}

XXAPI ptr xrtTcpServerGetUserData(xtcpserver* pServer)
{
	return pServer ? pServer->pUserData : NULL;
}



/* ============================== TCP 客户端 - 事件处理 ============================== */

// 客户端 事件处理 (READ/WRITE/CLOSE)
static void __xrt_tcp_client_on_event(ptr pOwner, xnetconn* pConn, int iEvent, const char* pData, size_t iLen)
{
	xtcpclient* pClient = (xtcpclient*)pOwner;
	if ( !pClient->bConnected && !pClient->bTlsHandshaking ) return;
	
	xnetpoller* pPoller = xrtEventLoopGetPoller(pClient->pLoop);
	
	if ( iEvent == XRT_POLL_READ ) {
		// TLS 握手进行中
		if ( pClient->bTlsHandshaking && pClient->pTlsCtx ) {
			xrtNetBufAppend(&pClient->pTlsCtx->tRecvBuf, pData, iLen);
			xnet_result iRes = xrtTlsHandshake(pClient->pTlsCtx, &pClient->tConn);
			
			if ( pClient->pTlsCtx->tSendBuf.iSize > 0 ) {
				xrtPollPostSend(pPoller, &pClient->tConn,
					pClient->pTlsCtx->tSendBuf.pData, pClient->pTlsCtx->tSendBuf.iSize);
				xrtNetBufConsume(&pClient->pTlsCtx->tSendBuf, pClient->pTlsCtx->tSendBuf.iSize);
			}
			
			if ( iRes == XRT_NET_OK ) {
				pClient->bTlsHandshaking = false;
				pClient->tConn.bTlsEnabled = true;
				pClient->tConn.pTlsCtx = pClient->pTlsCtx;
			} else if ( iRes == XRT_NET_ERROR ) {
				goto close_client;
			}
			xrtPollPostRecv(pPoller, &pClient->tConn);
			return;
		}
		
		// 正常数据
		if ( pClient->tConn.bTlsEnabled && pClient->pTlsCtx ) {
			xrtNetBufAppend(&pClient->pTlsCtx->tRecvBuf, pData, iLen);
			char aBuf[8192];
			size_t iDecrypted = 0;
			while ( xrtTlsRead(pClient->pTlsCtx, aBuf, sizeof(aBuf), &iDecrypted) == XRT_NET_OK && iDecrypted > 0 ) {
				if ( pClient->tEvents.OnRecv ) {
					pClient->tEvents.OnRecv(pClient, &pClient->tConn, aBuf, iDecrypted);
				}
				iDecrypted = 0;
			}
		} else {
			if ( pClient->tEvents.OnRecv ) {
				pClient->tEvents.OnRecv(pClient, &pClient->tConn, pData, iLen);
			}
		}
		
		xrtPollPostRecv(pPoller, &pClient->tConn);
		return;
	}
	
	if ( iEvent == XRT_POLL_WRITE ) {
		if ( pClient->tEvents.OnSend ) {
			pClient->tEvents.OnSend(pClient, &pClient->tConn, iLen);
		}
		return;
	}
	
	if ( iEvent == XRT_POLL_CLOSE || iEvent == XRT_POLL_ERROR ) {
close_client:
		pClient->bConnected = false;
		pClient->bTlsHandshaking = false;
		if ( pClient->tEvents.OnClose ) {
			pClient->tEvents.OnClose(pClient, &pClient->tConn);
		}
		return;
	}
}

// 客户端内部线程 (简易模式)
static uint32 __xrt_tcp_client_thread(ptr pParam)
{
	xtcpclient* pClient = (xtcpclient*)pParam;
	xrtEventLoopRun(pClient->pLoop);
	return 0;
}



/* ============================== TCP 客户端 API ============================== */

// 高级版: 绑定共享事件循环
XXAPI xtcpclient* xrtTcpClientCreateEx(xeventloop* pLoop, const char* sIP, uint16 iPort,
	const xnetconfig* pConfig, const xnetevents* pEvents)
{
	if ( !pLoop ) return NULL;
	
	xtcpclient* pClient = (xtcpclient*)xrtCalloc(1, sizeof(xtcpclient));
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
	
	xrtNetAddrInit(&pClient->tServerAddr, sIP, iPort);
	
	return pClient;
}

// 简易版: 内部创建事件循环 + 线程
XXAPI xtcpclient* xrtTcpClientCreate(const char* sIP, uint16 iPort,
	const xnetconfig* pConfig, const xnetevents* pEvents)
{
	xeventloop* pLoop = xrtEventLoopCreate();
	if ( !pLoop ) return NULL;
	
	xtcpclient* pClient = xrtTcpClientCreateEx(pLoop, sIP, iPort, pConfig, pEvents);
	if ( !pClient ) {
		xrtEventLoopDestroy(pLoop);
		return NULL;
	}
	
	pClient->bOwnLoop = true;
	return pClient;
}

XXAPI void xrtTcpClientDestroy(xtcpclient* pClient)
{
	if ( !pClient ) return;
	
	xrtTcpClientDisconnect(pClient);
	
	if ( pClient->bOwnLoop && pClient->pLoop ) {
		xrtEventLoopDestroy(pClient->pLoop);
		pClient->pLoop = NULL;
	}
	
	xrtFree(pClient);
}

XXAPI xnet_result xrtTcpClientConnect(xtcpclient* pClient)
{
	if ( !pClient ) return XRT_NET_ERROR;
	if ( pClient->bConnected ) return XRT_NET_OK;
	
	// 创建 socket
	memset(&pClient->tConn, 0, sizeof(xnetconn));
	if ( xrtSockCreate(&pClient->tConn, 0) != XRT_NET_OK ) {
		return XRT_NET_ERROR;
	}
	
	// 阻塞连接
	xnet_result iRes = xrtSockConnect(&pClient->tConn, &pClient->tServerAddr);
	if ( iRes != XRT_NET_OK && iRes != XRT_NET_AGAIN ) {
		xrtSockClose(&pClient->tConn);
		return XRT_NET_ERROR;
	}
	
	// 非阻塞 connect 等待
	if ( iRes == XRT_NET_AGAIN ) {
		fd_set tWriteSet;
		FD_ZERO(&tWriteSet);
		FD_SET(pClient->tConn.hSocket, &tWriteSet);
		struct timeval tTimeout;
		int iTimeoutMs = pClient->tConfig.iConnectTimeoutMs > 0 ? 
			pClient->tConfig.iConnectTimeoutMs : 5000;
		tTimeout.tv_sec = iTimeoutMs / 1000;
		tTimeout.tv_usec = (iTimeoutMs % 1000) * 1000;
		int iSelRes = select((int)pClient->tConn.hSocket + 1, NULL, &tWriteSet, NULL, &tTimeout);
		if ( iSelRes <= 0 ) {
			xrtSockClose(&pClient->tConn);
			return XRT_NET_TIMEOUT;
		}
	}
	
	// 设置非阻塞
	xrtSockSetNonBlock(&pClient->tConn);
	
	// TLS 握手 (同步)
	if ( pClient->bTlsEnabled ) {
		pClient->pTlsCtx = xrtTlsCreate(&pClient->tTlsConfig, false);
		if ( !pClient->pTlsCtx ) {
			xrtSockClose(&pClient->tConn);
			return XRT_NET_ERROR;
		}
		
		xnet_result iTlsRes;
		int iRetries = 0;
		int iMaxRetries = (pClient->tConfig.iConnectTimeoutMs > 0 ? 
			pClient->tConfig.iConnectTimeoutMs : 5000) / 100;  // 100ms per retry
		while ( (iTlsRes = xrtTlsHandshake(pClient->pTlsCtx, &pClient->tConn)) == XRT_NET_AGAIN ) {
			// 发送 TLS 输出
			if ( pClient->pTlsCtx->tSendBuf.iSize > 0 ) {
				size_t iSent = 0;
				xrtSockSend(&pClient->tConn, pClient->pTlsCtx->tSendBuf.pData,
					pClient->pTlsCtx->tSendBuf.iSize, &iSent);
				if ( iSent > 0 ) xrtNetBufConsume(&pClient->pTlsCtx->tSendBuf, iSent);
			}
			
			// select 等待事件驱动，替代 Sleep(10)
			fd_set tReadSet, tWriteSet;
			FD_ZERO(&tReadSet); FD_ZERO(&tWriteSet);
			FD_SET(pClient->tConn.hSocket, &tReadSet);
			if ( pClient->pTlsCtx->tSendBuf.iSize > 0 ) {
				FD_SET(pClient->tConn.hSocket, &tWriteSet);
			}
			struct timeval tSelTimeout = {0, 100000};  // 100ms
			select((int)pClient->tConn.hSocket + 1, &tReadSet, 
				pClient->pTlsCtx->tSendBuf.iSize > 0 ? &tWriteSet : NULL, NULL, &tSelTimeout);
			
			iRetries++;
			if ( iRetries > iMaxRetries ) {
				xrtTlsDestroy(pClient->pTlsCtx);
				pClient->pTlsCtx = NULL;
				xrtSockClose(&pClient->tConn);
				return XRT_NET_TIMEOUT;
			}
		}
		
		if ( iTlsRes != XRT_NET_OK ) {
			xrtTlsDestroy(pClient->pTlsCtx);
			pClient->pTlsCtx = NULL;
			xrtSockClose(&pClient->tConn);
			return XRT_NET_ERROR;
		}
		
		pClient->tConn.bTlsEnabled = true;
		pClient->tConn.pTlsCtx = pClient->pTlsCtx;
	}
	
	pClient->bConnected = true;
	pClient->bRunning = true;
	
	// 设置事件处理器
	pClient->tHandler.pfnHandler = __xrt_tcp_client_on_event;
	pClient->tHandler.pOwner = pClient;
	pClient->tConn.pUserData = &pClient->tHandler;
	
	// 注册到 poller
	xnetpoller* pPoller = xrtEventLoopGetPoller(pClient->pLoop);
	xrtPollAdd(pPoller, &pClient->tConn, XRT_POLL_READ);
	xrtPollPostRecv(pPoller, &pClient->tConn);
	
	// 触发连接回调
	if ( pClient->tEvents.OnConnect ) {
		pClient->tEvents.OnConnect(pClient, &pClient->tConn, true);
	}
	
	// 简易模式: 启动内部线程
	if ( pClient->bOwnLoop ) {
		pClient->pThread = xrtThreadCreate((ptr)__xrt_tcp_client_thread, pClient, 0);
		if ( !pClient->pThread ) {
			pClient->bRunning = false;
			pClient->bConnected = false;
			xrtSockClose(&pClient->tConn);
			return XRT_NET_ERROR;
		}
	}
	
	return XRT_NET_OK;
}

XXAPI void xrtTcpClientDisconnect(xtcpclient* pClient)
{
	if ( !pClient || !pClient->bRunning ) return;
	
	pClient->bRunning = false;
	pClient->bConnected = false;
	
	if ( pClient->bOwnLoop && pClient->pLoop ) {
		xrtEventLoopStop(pClient->pLoop);
	}
	
	if ( pClient->pThread ) {
		xrtThreadWait(pClient->pThread);
		xrtThreadDestroy(pClient->pThread);
		pClient->pThread = NULL;
	}
	
	if ( pClient->pTlsCtx ) {
		xrtTlsClose(pClient->pTlsCtx);
		if ( pClient->pTlsCtx->tSendBuf.iSize > 0 ) {
			size_t iSent = 0;
			xrtSockSend(&pClient->tConn, pClient->pTlsCtx->tSendBuf.pData,
				pClient->pTlsCtx->tSendBuf.iSize, &iSent);
		}
		xrtTlsDestroy(pClient->pTlsCtx);
		pClient->pTlsCtx = NULL;
	}
	
	xrtSockClose(&pClient->tConn);
}

XXAPI xnet_result xrtTcpClientSend(xtcpclient* pClient, const char* pData, size_t iLen)
{
	if ( !pClient || !pData || iLen == 0 ) return XRT_NET_ERROR;
	if ( !pClient->bConnected ) return XRT_NET_ERROR;
	
	xnetpoller* pPoller = xrtEventLoopGetPoller(pClient->pLoop);
	
	if ( pClient->bTlsEnabled && pClient->pTlsCtx ) {
		size_t iWritten = 0;
		xnet_result iRes = xrtTlsWrite(pClient->pTlsCtx, pData, iLen, &iWritten);
		if ( iRes != XRT_NET_OK ) return iRes;
		if ( pClient->pTlsCtx->tSendBuf.iSize > 0 ) {
			xrtPollPostSend(pPoller, &pClient->tConn,
				pClient->pTlsCtx->tSendBuf.pData, pClient->pTlsCtx->tSendBuf.iSize);
			xrtNetBufConsume(&pClient->pTlsCtx->tSendBuf, pClient->pTlsCtx->tSendBuf.iSize);
		}
		return XRT_NET_OK;
	} else {
		return xrtPollPostSend(pPoller, &pClient->tConn, pData, iLen);
	}
}

XXAPI xnet_result xrtTcpClientEnableTLS(xtcpclient* pClient, const xtlsconfig* pConfig)
{
	if ( !pClient || !pConfig ) return XRT_NET_ERROR;
	pClient->tTlsConfig = *pConfig;
	pClient->bTlsEnabled = true;
	return XRT_NET_OK;
}

XXAPI bool xrtTcpClientIsConnected(xtcpclient* pClient)
{
	return pClient ? pClient->bConnected : false;
}

XXAPI void xrtTcpClientSetUserData(xtcpclient* pClient, ptr pData)
{
	if ( pClient ) pClient->pUserData = pData;
}

XXAPI ptr xrtTcpClientGetUserData(xtcpclient* pClient)
{
	return pClient ? pClient->pUserData : NULL;
}

