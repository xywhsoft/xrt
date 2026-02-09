



/*
	NetTCP - TCP 服务器/客户端封装 [Ver1.0]
	
	基于 netsock.h (Socket 基础) + netpoll.h (IO 模型) + nettls.h (TLS 1.3)
	使用 xrt 线程库 (thread.h) 创建事件循环线程
	使用指针数组 (xparray) 管理动态客户端列表
	
	仅 IPv4，跨平台 (Windows / Linux)
*/



/* ============================== 内部客户端数据结构 ============================== */

typedef struct __xrt_tcp_client_slot {
	xnetconn tConn;
	xnetbuf tRecvBuf;
	xnetbuf tSendBuf;
	xtlsctx* pTlsCtx;
	bool bInUse;
} __xrt_tcp_client_slot;



/* ============================== TCP 服务器结构 ============================== */

struct xrt_tcp_server {
	xnetconn tListenConn;           // 监听 socket
	xnetpoller* pPoller;            // IO poller
	xnetconfig tConfig;             // 配置
	xnetevents tEvents;             // 事件回调
	xtlsconfig tTlsConfig;         // TLS 配置
	bool bTlsEnabled;              // 是否启用 TLS
	
	// 客户端管理
	__xrt_tcp_client_slot** arrClients;  // 客户端槽位数组
	int iMaxClients;               // 最大客户端数
	int iClientCount;              // 当前客户端数
	
	// 线程
	xthread pThread;               // 事件循环线程
	volatile bool bRunning;        // 运行标志
	
	// 用户数据
	ptr pUserData;
};



/* ============================== TCP 客户端结构 ============================== */

struct xrt_tcp_client {
	xnetconn tConn;                // 连接 socket
	xnetbuf tRecvBuf;              // 接收缓冲区
	xnetbuf tSendBuf;              // 发送缓冲区
	xnetconfig tConfig;            // 配置
	xnetevents tEvents;            // 事件回调
	xtlsconfig tTlsConfig;        // TLS 配置
	xtlsctx* pTlsCtx;             // TLS 上下文
	bool bTlsEnabled;             // 是否启用 TLS
	
	xnetaddr tServerAddr;          // 服务器地址
	
	// 线程
	xthread pThread;               // 事件循环线程
	volatile bool bRunning;        // 运行标志
	volatile bool bConnected;      // 连接状态
	
	// 用户数据
	ptr pUserData;
};



/* ============================== TCP 服务器 - 事件循环 ============================== */

// 服务器事件循环：轮询 accept + recv
static uint32 __xrt_tcp_server_loop(ptr pParam)
{
	xtcpserver* pServer = (xtcpserver*)pParam;
	char aBuf[8192];
	
	while ( pServer->bRunning ) {
		// 1. 尝试 accept 新连接
		xnetconn tNewConn;
		memset(&tNewConn, 0, sizeof(xnetconn));
		xnet_result iRes = xrtSockAccept(&pServer->tListenConn, &tNewConn);
		
		if ( iRes == XRT_NET_OK ) {
			// 找到空槽位
			int iSlot = -1;
			for ( int i = 0; i < pServer->iMaxClients; i++ ) {
				if ( !pServer->arrClients[i]->bInUse ) {
					iSlot = i;
					break;
				}
			}
			
			if ( iSlot >= 0 ) {
				__xrt_tcp_client_slot* pSlot = pServer->arrClients[iSlot];
				pSlot->tConn = tNewConn;
				pSlot->tConn.iId = iSlot;
				pSlot->tConn.iType = 0;  // TCP
				pSlot->bInUse = true;
				xrtNetBufClear(&pSlot->tRecvBuf);
				xrtNetBufClear(&pSlot->tSendBuf);
				
				// 设置非阻塞
				xrtSockSetNonBlock(&pSlot->tConn);
				
				pServer->iClientCount++;
				
				// TLS 握手（如果启用）
				if ( pServer->bTlsEnabled ) {
					pSlot->pTlsCtx = xrtTlsCreate(&pServer->tTlsConfig, true);
					if ( pSlot->pTlsCtx ) {
						pSlot->tConn.bTlsEnabled = true;
						pSlot->tConn.pTlsCtx = pSlot->pTlsCtx;
						// TODO: 服务端 TLS 握手循环
					}
				}
				
				// 触发 OnAccept 回调
				if ( pServer->tEvents.OnAccept ) {
					pServer->tEvents.OnAccept(pServer, &pSlot->tConn);
				}
			} else {
				// 没有空槽位，拒绝连接
				xrtSockClose(&tNewConn);
			}
		}
		
		// 2. 轮询所有客户端的 recv
		for ( int i = 0; i < pServer->iMaxClients; i++ ) {
			__xrt_tcp_client_slot* pSlot = pServer->arrClients[i];
			if ( !pSlot->bInUse ) continue;
			
			size_t iRecvd = 0;
			xnet_result iRecvRes;
			
			if ( pSlot->tConn.bTlsEnabled && pSlot->pTlsCtx ) {
				// TLS 读取
				// 先从 socket 读到 TLS 接收缓冲区
				char aTlsBuf[4096];
				size_t iTlsRecvd = 0;
				xnet_result iTlsRes = xrtSockRecv(&pSlot->tConn, aTlsBuf, sizeof(aTlsBuf), &iTlsRecvd);
				if ( iTlsRes == XRT_NET_OK && iTlsRecvd > 0 ) {
					xrtNetBufAppend(&pSlot->pTlsCtx->tRecvBuf, aTlsBuf, iTlsRecvd);
				}
				iRecvRes = xrtTlsRead(pSlot->pTlsCtx, aBuf, sizeof(aBuf), &iRecvd);
			} else {
				iRecvRes = xrtSockRecv(&pSlot->tConn, aBuf, sizeof(aBuf), &iRecvd);
			}
			
			if ( iRecvRes == XRT_NET_OK && iRecvd > 0 ) {
				if ( pServer->tEvents.OnRecv ) {
					pServer->tEvents.OnRecv(pServer, &pSlot->tConn, aBuf, iRecvd);
				}
			} else if ( iRecvRes == XRT_NET_CLOSED || iRecvRes == XRT_NET_ERROR ) {
				// 客户端断开
				if ( pServer->tEvents.OnClose ) {
					pServer->tEvents.OnClose(pServer, &pSlot->tConn);
				}
				if ( pSlot->pTlsCtx ) {
					xrtTlsDestroy(pSlot->pTlsCtx);
					pSlot->pTlsCtx = NULL;
				}
				xrtSockClose(&pSlot->tConn);
				pSlot->bInUse = false;
				pServer->iClientCount--;
			}
			// XRT_NET_AGAIN: 无数据可读，继续下一个
		}
		
		// 3. 短暂休眠避免忙等
		int iTimeout = pServer->tConfig.iPollTimeoutMs > 0 ? pServer->tConfig.iPollTimeoutMs : 10;
		#if defined(_WIN32) || defined(_WIN64)
			Sleep(iTimeout);
		#else
			usleep(iTimeout * 1000);
		#endif
	}
	
	return 0;
}



/* ============================== TCP 服务器 API ============================== */

XXAPI xtcpserver* xrtTcpServerCreate(const char* sIP, uint16 iPort,
	const xnetconfig* pConfig, const xnetevents* pEvents)
{
	xtcpserver* pServer = (xtcpserver*)xrtCalloc(1, sizeof(xtcpserver));
	if ( !pServer ) return NULL;
	
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
	
	// 分配客户端槽位数组
	pServer->arrClients = (__xrt_tcp_client_slot**)xrtCalloc(pServer->iMaxClients, sizeof(__xrt_tcp_client_slot*));
	if ( !pServer->arrClients ) {
		xrtFree(pServer);
		return NULL;
	}
	
	size_t iRecvSize = pServer->tConfig.iRecvBufSize > 0 ? pServer->tConfig.iRecvBufSize : 8192;
	size_t iSendSize = pServer->tConfig.iSendBufSize > 0 ? pServer->tConfig.iSendBufSize : 8192;
	
	for ( int i = 0; i < pServer->iMaxClients; i++ ) {
		pServer->arrClients[i] = (__xrt_tcp_client_slot*)xrtCalloc(1, sizeof(__xrt_tcp_client_slot));
		if ( !pServer->arrClients[i] ) {
			// 清理已分配
			for ( int j = 0; j < i; j++ ) {
				xrtNetBufFree(&pServer->arrClients[j]->tRecvBuf);
				xrtNetBufFree(&pServer->arrClients[j]->tSendBuf);
				xrtFree(pServer->arrClients[j]);
			}
			xrtFree(pServer->arrClients);
			xrtFree(pServer);
			return NULL;
		}
		xrtNetBufInit(&pServer->arrClients[i]->tRecvBuf, iRecvSize);
		xrtNetBufInit(&pServer->arrClients[i]->tSendBuf, iSendSize);
		pServer->arrClients[i]->bInUse = false;
		pServer->arrClients[i]->tConn.iId = i;
	}
	
	// 创建监听 socket
	memset(&pServer->tListenConn, 0, sizeof(xnetconn));
	if ( xrtSockCreate(&pServer->tListenConn, 0) != XRT_NET_OK ) {  // TCP
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
	
	return pServer;

error_cleanup:
	xrtSockClose(&pServer->tListenConn);
	for ( int i = 0; i < pServer->iMaxClients; i++ ) {
		if ( pServer->arrClients[i] ) {
			xrtNetBufFree(&pServer->arrClients[i]->tRecvBuf);
			xrtNetBufFree(&pServer->arrClients[i]->tSendBuf);
			xrtFree(pServer->arrClients[i]);
		}
	}
	xrtFree(pServer->arrClients);
	xrtFree(pServer);
	return NULL;
}

XXAPI void xrtTcpServerDestroy(xtcpserver* pServer)
{
	if ( !pServer ) return;
	
	xrtTcpServerStop(pServer);
	
	// 关闭所有客户端
	for ( int i = 0; i < pServer->iMaxClients; i++ ) {
		if ( pServer->arrClients[i] ) {
			if ( pServer->arrClients[i]->bInUse ) {
				xrtSockClose(&pServer->arrClients[i]->tConn);
			}
			if ( pServer->arrClients[i]->pTlsCtx ) {
				xrtTlsDestroy(pServer->arrClients[i]->pTlsCtx);
			}
			xrtNetBufFree(&pServer->arrClients[i]->tRecvBuf);
			xrtNetBufFree(&pServer->arrClients[i]->tSendBuf);
			xrtFree(pServer->arrClients[i]);
		}
	}
	xrtFree(pServer->arrClients);
	
	// 关闭监听 socket
	xrtSockClose(&pServer->tListenConn);
	
	xrtFree(pServer);
}

XXAPI xnet_result xrtTcpServerStart(xtcpserver* pServer)
{
	if ( !pServer ) return XRT_NET_ERROR;
	if ( pServer->bRunning ) return XRT_NET_OK;
	
	pServer->bRunning = true;
	pServer->pThread = xrtThreadCreate((ptr)__xrt_tcp_server_loop, pServer, 0);
	
	if ( !pServer->pThread ) {
		pServer->bRunning = false;
		return XRT_NET_ERROR;
	}
	
	return XRT_NET_OK;
}

XXAPI void xrtTcpServerStop(xtcpserver* pServer)
{
	if ( !pServer || !pServer->bRunning ) return;
	
	pServer->bRunning = false;
	
	if ( pServer->pThread ) {
		xrtThreadWait(pServer->pThread);
		xrtThreadDestroy(pServer->pThread);
		pServer->pThread = NULL;
	}
	
	// 关闭所有客户端连接
	for ( int i = 0; i < pServer->iMaxClients; i++ ) {
		if ( pServer->arrClients[i] && pServer->arrClients[i]->bInUse ) {
			if ( pServer->tEvents.OnClose ) {
				pServer->tEvents.OnClose(pServer, &pServer->arrClients[i]->tConn);
			}
			xrtSockClose(&pServer->arrClients[i]->tConn);
			pServer->arrClients[i]->bInUse = false;
		}
	}
	pServer->iClientCount = 0;
}

XXAPI xnet_result xrtTcpServerSend(xtcpserver* pServer, int iClientId,
	const char* pData, size_t iLen)
{
	if ( !pServer || !pData || iLen == 0 ) return XRT_NET_ERROR;
	if ( iClientId < 0 || iClientId >= pServer->iMaxClients ) return XRT_NET_ERROR;
	
	__xrt_tcp_client_slot* pSlot = pServer->arrClients[iClientId];
	if ( !pSlot || !pSlot->bInUse ) return XRT_NET_ERROR;
	
	if ( pSlot->tConn.bTlsEnabled && pSlot->pTlsCtx ) {
		size_t iWritten = 0;
		xnet_result iRes = xrtTlsWrite(pSlot->pTlsCtx, pData, iLen, &iWritten);
		if ( iRes != XRT_NET_OK ) return iRes;
		// 发送 TLS 缓冲区数据
		if ( pSlot->pTlsCtx->tSendBuf.iSize > 0 ) {
			size_t iSent = 0;
			xrtSockSend(&pSlot->tConn, pSlot->pTlsCtx->tSendBuf.pData,
				pSlot->pTlsCtx->tSendBuf.iSize, &iSent);
			if ( iSent > 0 ) xrtNetBufConsume(&pSlot->pTlsCtx->tSendBuf, iSent);
		}
		return XRT_NET_OK;
	} else {
		size_t iSent = 0;
		return xrtSockSend(&pSlot->tConn, pData, iLen, &iSent);
	}
}

XXAPI void xrtTcpServerDisconnect(xtcpserver* pServer, int iClientId)
{
	if ( !pServer ) return;
	if ( iClientId < 0 || iClientId >= pServer->iMaxClients ) return;
	
	__xrt_tcp_client_slot* pSlot = pServer->arrClients[iClientId];
	if ( !pSlot || !pSlot->bInUse ) return;
	
	if ( pServer->tEvents.OnClose ) {
		pServer->tEvents.OnClose(pServer, &pSlot->tConn);
	}
	
	if ( pSlot->pTlsCtx ) {
		xrtTlsClose(pSlot->pTlsCtx);
		xrtTlsDestroy(pSlot->pTlsCtx);
		pSlot->pTlsCtx = NULL;
	}
	
	xrtSockClose(&pSlot->tConn);
	pSlot->bInUse = false;
	pServer->iClientCount--;
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



/* ============================== TCP 客户端 - 事件循环 ============================== */

static uint32 __xrt_tcp_client_loop(ptr pParam)
{
	xtcpclient* pClient = (xtcpclient*)pParam;
	char aBuf[8192];
	
	while ( pClient->bRunning ) {
		size_t iRecvd = 0;
		xnet_result iRes;
		
		if ( pClient->bTlsEnabled && pClient->pTlsCtx ) {
			// TLS: 先从 socket 读数据到 TLS 缓冲区
			char aTlsBuf[4096];
			size_t iTlsRecvd = 0;
			xnet_result iTlsRes = xrtSockRecv(&pClient->tConn, aTlsBuf, sizeof(aTlsBuf), &iTlsRecvd);
			if ( iTlsRes == XRT_NET_OK && iTlsRecvd > 0 ) {
				xrtNetBufAppend(&pClient->pTlsCtx->tRecvBuf, aTlsBuf, iTlsRecvd);
			}
			iRes = xrtTlsRead(pClient->pTlsCtx, aBuf, sizeof(aBuf), &iRecvd);
		} else {
			iRes = xrtSockRecv(&pClient->tConn, aBuf, sizeof(aBuf), &iRecvd);
		}
		
		if ( iRes == XRT_NET_OK && iRecvd > 0 ) {
			if ( pClient->tEvents.OnRecv ) {
				pClient->tEvents.OnRecv(pClient, &pClient->tConn, aBuf, iRecvd);
			}
		} else if ( iRes == XRT_NET_CLOSED || iRes == XRT_NET_ERROR ) {
			pClient->bConnected = false;
			if ( pClient->tEvents.OnClose ) {
				pClient->tEvents.OnClose(pClient, &pClient->tConn);
			}
			break;
		}
		
		// 短暂休眠
		int iTimeout = pClient->tConfig.iPollTimeoutMs > 0 ? pClient->tConfig.iPollTimeoutMs : 10;
		#if defined(_WIN32) || defined(_WIN64)
			Sleep(iTimeout);
		#else
			usleep(iTimeout * 1000);
		#endif
	}
	
	return 0;
}



/* ============================== TCP 客户端 API ============================== */

XXAPI xtcpclient* xrtTcpClientCreate(const char* sIP, uint16 iPort,
	const xnetconfig* pConfig, const xnetevents* pEvents)
{
	xtcpclient* pClient = (xtcpclient*)xrtCalloc(1, sizeof(xtcpclient));
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
	
	// 保存服务器地址
	xrtNetAddrInit(&pClient->tServerAddr, sIP, iPort);
	
	// 初始化缓冲区
	size_t iRecvSize = pClient->tConfig.iRecvBufSize > 0 ? pClient->tConfig.iRecvBufSize : 8192;
	size_t iSendSize = pClient->tConfig.iSendBufSize > 0 ? pClient->tConfig.iSendBufSize : 8192;
	xrtNetBufInit(&pClient->tRecvBuf, iRecvSize);
	xrtNetBufInit(&pClient->tSendBuf, iSendSize);
	
	return pClient;
}

XXAPI void xrtTcpClientDestroy(xtcpclient* pClient)
{
	if ( !pClient ) return;
	
	xrtTcpClientDisconnect(pClient);
	
	xrtNetBufFree(&pClient->tRecvBuf);
	xrtNetBufFree(&pClient->tSendBuf);
	
	xrtFree(pClient);
}

XXAPI xnet_result xrtTcpClientConnect(xtcpclient* pClient)
{
	if ( !pClient ) return XRT_NET_ERROR;
	if ( pClient->bConnected ) return XRT_NET_OK;
	
	// 创建 socket
	memset(&pClient->tConn, 0, sizeof(xnetconn));
	if ( xrtSockCreate(&pClient->tConn, 0) != XRT_NET_OK ) {  // TCP
		return XRT_NET_ERROR;
	}
	
	// 阻塞连接
	xnet_result iRes = xrtSockConnect(&pClient->tConn, &pClient->tServerAddr);
	if ( iRes != XRT_NET_OK && iRes != XRT_NET_AGAIN ) {
		xrtSockClose(&pClient->tConn);
		return XRT_NET_ERROR;
	}
	
	// 如果是 AGAIN (非阻塞连接)，等待连接完成
	if ( iRes == XRT_NET_AGAIN ) {
		// 使用 select 等待连接完成
		fd_set tWriteSet;
		FD_ZERO(&tWriteSet);
		FD_SET(pClient->tConn.hSocket, &tWriteSet);
		struct timeval tTimeout;
		tTimeout.tv_sec = 5;
		tTimeout.tv_usec = 0;
		int iSelRes = select((int)pClient->tConn.hSocket + 1, NULL, &tWriteSet, NULL, &tTimeout);
		if ( iSelRes <= 0 ) {
			xrtSockClose(&pClient->tConn);
			return XRT_NET_TIMEOUT;
		}
	}
	
	// TLS 握手
	if ( pClient->bTlsEnabled ) {
		// 设置非阻塞，避免握手循环中 recv 阻塞
		xrtSockSetNonBlock(&pClient->tConn);
		
		pClient->pTlsCtx = xrtTlsCreate(&pClient->tTlsConfig, false);
		if ( !pClient->pTlsCtx ) {
			xrtSockClose(&pClient->tConn);
			return XRT_NET_ERROR;
		}
		
		// TLS 握手循环
		xnet_result iTlsRes;
		int iRetries = 0;
		while ( (iTlsRes = xrtTlsHandshake(pClient->pTlsCtx, &pClient->tConn)) == XRT_NET_AGAIN ) {
			// 发送待发数据
			if ( pClient->pTlsCtx->tSendBuf.iSize > 0 ) {
				size_t iSent = 0;
				xnet_result iSendRes = xrtSockSend(&pClient->tConn, pClient->pTlsCtx->tSendBuf.pData,
					pClient->pTlsCtx->tSendBuf.iSize, &iSent);
				#ifdef DEBUG_TRACE
					printf("    [TCP] TLS send: wanted=%d sent=%d res=%d\n",
						(int)pClient->pTlsCtx->tSendBuf.iSize, (int)iSent, (int)iSendRes);
				#endif
				if ( iSent > 0 ) xrtNetBufConsume(&pClient->pTlsCtx->tSendBuf, iSent);
			}
			
			iRetries++;
			if ( iRetries > 500 ) {
				#ifdef DEBUG_TRACE
					printf("    [TCP] TLS handshake timeout after %d retries\n", iRetries);
				#endif
				xrtTlsDestroy(pClient->pTlsCtx);
				pClient->pTlsCtx = NULL;
				xrtSockClose(&pClient->tConn);
				return XRT_NET_TIMEOUT;
			}
			
			#if defined(_WIN32) || defined(_WIN64)
				Sleep(10);
			#else
				usleep(10000);
			#endif
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
	
	// 设置非阻塞（连接后）
	xrtSockSetNonBlock(&pClient->tConn);
	
	pClient->bConnected = true;
	pClient->bRunning = true;
	
	// 启动接收线程
	pClient->pThread = xrtThreadCreate((ptr)__xrt_tcp_client_loop, pClient, 0);
	if ( !pClient->pThread ) {
		pClient->bRunning = false;
		pClient->bConnected = false;
		xrtSockClose(&pClient->tConn);
		return XRT_NET_ERROR;
	}
	
	// 触发连接回调
	if ( pClient->tEvents.OnConnect ) {
		pClient->tEvents.OnConnect(pClient, &pClient->tConn, true);
	}
	
	return XRT_NET_OK;
}

XXAPI void xrtTcpClientDisconnect(xtcpclient* pClient)
{
	if ( !pClient || !pClient->bRunning ) return;
	
	pClient->bRunning = false;
	pClient->bConnected = false;
	
	if ( pClient->pThread ) {
		xrtThreadWait(pClient->pThread);
		xrtThreadDestroy(pClient->pThread);
		pClient->pThread = NULL;
	}
	
	if ( pClient->pTlsCtx ) {
		xrtTlsClose(pClient->pTlsCtx);
		// 发送 close_notify
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
	
	if ( pClient->bTlsEnabled && pClient->pTlsCtx ) {
		size_t iWritten = 0;
		xnet_result iRes = xrtTlsWrite(pClient->pTlsCtx, pData, iLen, &iWritten);
		if ( iRes != XRT_NET_OK ) return iRes;
		// 发送 TLS 缓冲区
		if ( pClient->pTlsCtx->tSendBuf.iSize > 0 ) {
			size_t iSent = 0;
			xrtSockSend(&pClient->tConn, pClient->pTlsCtx->tSendBuf.pData,
				pClient->pTlsCtx->tSendBuf.iSize, &iSent);
			if ( iSent > 0 ) xrtNetBufConsume(&pClient->pTlsCtx->tSendBuf, iSent);
		}
		return XRT_NET_OK;
	} else {
		size_t iSent = 0;
		return xrtSockSend(&pClient->tConn, pData, iLen, &iSent);
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


