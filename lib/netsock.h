



/*
	NetSock - 跨平台 Socket 基础操作 [Ver1.0]
	
	提供 Socket 创建/绑定/监听/接受/连接/收发/关闭/选项设置
	仅 IPv4，跨平台 (Windows / Linux)
*/

#if defined(_WIN32) || defined(_WIN64)
	#ifndef _SOCKLEN_T_DEFINED
		#define _SOCKLEN_T_DEFINED
		typedef int socklen_t;
	#endif
#endif



/* ============================== 地址操作 ============================== */

XXAPI void xrtNetAddrInit(xnetaddr* pAddr, const char* sIP, uint16 iPort)
{
	if ( !pAddr ) return;
	memset(pAddr, 0, sizeof(xnetaddr));
	pAddr->iPort = iPort;
	if ( sIP && sIP[0] ) {
		pAddr->iAddr = inet_addr(sIP);
		strncpy(pAddr->sAddr, sIP, 15);
		pAddr->sAddr[15] = '\0';
	} else {
		pAddr->iAddr = INADDR_ANY;
		strcpy(pAddr->sAddr, "0.0.0.0");
	}
}

XXAPI void xrtNetAddrFromSockAddr(xnetaddr* pAddr, struct sockaddr_in* pSA)
{
	if ( !pAddr || !pSA ) return;
	pAddr->iAddr = pSA->sin_addr.s_addr;
	pAddr->iPort = ntohs(pSA->sin_port);
	// inet_ntoa 返回静态缓冲区，需复制
	const char* sIP = inet_ntoa(pSA->sin_addr);
	if ( sIP ) {
		strncpy(pAddr->sAddr, sIP, 15);
		pAddr->sAddr[15] = '\0';
	}
}

XXAPI void xrtNetAddrToSockAddr(const xnetaddr* pAddr, struct sockaddr_in* pSA)
{
	if ( !pAddr || !pSA ) return;
	memset(pSA, 0, sizeof(struct sockaddr_in));
	pSA->sin_family = AF_INET;
	pSA->sin_addr.s_addr = pAddr->iAddr;
	pSA->sin_port = htons(pAddr->iPort);
}

XXAPI uint32 xrtNetIPFromStr(const char* sIP)
{
	if ( !sIP ) return 0;
	return (uint32)inet_addr(sIP);
}

XXAPI const char* xrtNetIPToStr(uint32 iIP)
{
	struct in_addr tAddr;
	tAddr.s_addr = iIP;
	// 使用 xrt 环形临时内存
	char* sResult = (char*)inet_ntoa(tAddr);
	if ( !sResult ) return "";
	str sCopy = xrtCopyStr(sResult, strlen(sResult));
	return (const char*)sCopy;
}



/* ============================== Socket 生命周期 ============================== */

XXAPI xnet_result xrtSockCreate(xnetconn* pConn, int iType)
{
	if ( !pConn ) return XRT_NET_ERROR;
	
	memset(pConn, 0, sizeof(xnetconn));
	pConn->hSocket = XSOCKET_INVALID;
	pConn->iType = iType;
	
	int iSockType = (iType == 0) ? SOCK_STREAM : SOCK_DGRAM;
	int iProtocol = (iType == 0) ? IPPROTO_TCP : IPPROTO_UDP;
	
	pConn->hSocket = socket(AF_INET, iSockType, iProtocol);
	if ( pConn->hSocket == XSOCKET_INVALID ) {
		return XRT_NET_ERROR;
	}
	
	return XRT_NET_OK;
}

XXAPI void xrtSockClose(xnetconn* pConn)
{
	if ( !pConn || pConn->hSocket == XSOCKET_INVALID ) return;
	
	#if defined(_WIN32) || defined(_WIN64)
		closesocket(pConn->hSocket);
	#else
		close(pConn->hSocket);
	#endif
	
	pConn->hSocket = XSOCKET_INVALID;
}

XXAPI xnet_result xrtSockSetNonBlock(xnetconn* pConn)
{
	if ( !pConn || pConn->hSocket == XSOCKET_INVALID ) return XRT_NET_ERROR;
	
	#if defined(_WIN32) || defined(_WIN64)
		u_long iMode = 1;
		if ( ioctlsocket(pConn->hSocket, FIONBIO, &iMode) != 0 ) {
			return XRT_NET_ERROR;
		}
	#else
		int iFlags = fcntl(pConn->hSocket, F_GETFL, 0);
		if ( iFlags == -1 ) return XRT_NET_ERROR;
		if ( fcntl(pConn->hSocket, F_SETFL, iFlags | O_NONBLOCK) == -1 ) {
			return XRT_NET_ERROR;
		}
	#endif
	
	return XRT_NET_OK;
}

XXAPI xnet_result xrtSockSetReuseAddr(xnetconn* pConn)
{
	if ( !pConn || pConn->hSocket == XSOCKET_INVALID ) return XRT_NET_ERROR;
	
	int iOpt = 1;
	#if defined(_WIN32) || defined(_WIN64)
		if ( setsockopt(pConn->hSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&iOpt, sizeof(iOpt)) != 0 ) {
			return XRT_NET_ERROR;
		}
	#else
		if ( setsockopt(pConn->hSocket, SOL_SOCKET, SO_REUSEADDR, &iOpt, sizeof(iOpt)) != 0 ) {
			return XRT_NET_ERROR;
		}
	#endif
	
	return XRT_NET_OK;
}

XXAPI xnet_result xrtSockSetTimeout(xnetconn* pConn, int iRecvMs, int iSendMs)
{
	if ( !pConn || pConn->hSocket == XSOCKET_INVALID ) return XRT_NET_ERROR;
	
	#if defined(_WIN32) || defined(_WIN64)
		DWORD iRecvTimeout = (DWORD)iRecvMs;
		DWORD iSendTimeout = (DWORD)iSendMs;
		if ( iRecvMs > 0 ) {
			if ( setsockopt(pConn->hSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&iRecvTimeout, sizeof(iRecvTimeout)) != 0 ) {
				return XRT_NET_ERROR;
			}
		}
		if ( iSendMs > 0 ) {
			if ( setsockopt(pConn->hSocket, SOL_SOCKET, SO_SNDTIMEO, (char*)&iSendTimeout, sizeof(iSendTimeout)) != 0 ) {
				return XRT_NET_ERROR;
			}
		}
	#else
		if ( iRecvMs > 0 ) {
			struct timeval tRecvTV;
			tRecvTV.tv_sec = iRecvMs / 1000;
			tRecvTV.tv_usec = (iRecvMs % 1000) * 1000;
			if ( setsockopt(pConn->hSocket, SOL_SOCKET, SO_RCVTIMEO, &tRecvTV, sizeof(tRecvTV)) != 0 ) {
				return XRT_NET_ERROR;
			}
		}
		if ( iSendMs > 0 ) {
			struct timeval tSendTV;
			tSendTV.tv_sec = iSendMs / 1000;
			tSendTV.tv_usec = (iSendMs % 1000) * 1000;
			if ( setsockopt(pConn->hSocket, SOL_SOCKET, SO_SNDTIMEO, &tSendTV, sizeof(tSendTV)) != 0 ) {
				return XRT_NET_ERROR;
			}
		}
	#endif
	
	return XRT_NET_OK;
}

XXAPI xnet_result xrtSockSetNoDelay(xnetconn* pConn)
{
	if ( !pConn || pConn->hSocket == XSOCKET_INVALID ) return XRT_NET_ERROR;
	
	int iOpt = 1;
	#if defined(_WIN32) || defined(_WIN64)
		if ( setsockopt(pConn->hSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&iOpt, sizeof(iOpt)) != 0 ) {
			return XRT_NET_ERROR;
		}
	#else
		if ( setsockopt(pConn->hSocket, IPPROTO_TCP, TCP_NODELAY, &iOpt, sizeof(iOpt)) != 0 ) {
			return XRT_NET_ERROR;
		}
	#endif
	
	return XRT_NET_OK;
}

XXAPI xnet_result xrtSockSetKeepAlive(xnetconn* pConn, int iIdleSec, int iIntervalSec, int iCount)
{
	if ( !pConn || pConn->hSocket == XSOCKET_INVALID ) return XRT_NET_ERROR;
	
	int iOpt = 1;
	#if defined(_WIN32) || defined(_WIN64)
		if ( setsockopt(pConn->hSocket, SOL_SOCKET, SO_KEEPALIVE, (char*)&iOpt, sizeof(iOpt)) != 0 ) {
			return XRT_NET_ERROR;
		}
		// Windows: 使用 tcp_keepalive 结构 (需要 WSAIoctl)
		// 简化版本只启用 keepalive
	#else
		if ( setsockopt(pConn->hSocket, SOL_SOCKET, SO_KEEPALIVE, &iOpt, sizeof(iOpt)) != 0 ) {
			return XRT_NET_ERROR;
		}
		if ( iIdleSec > 0 ) {
			setsockopt(pConn->hSocket, IPPROTO_TCP, TCP_KEEPIDLE, &iIdleSec, sizeof(iIdleSec));
		}
		if ( iIntervalSec > 0 ) {
			setsockopt(pConn->hSocket, IPPROTO_TCP, TCP_KEEPINTVL, &iIntervalSec, sizeof(iIntervalSec));
		}
		if ( iCount > 0 ) {
			setsockopt(pConn->hSocket, IPPROTO_TCP, TCP_KEEPCNT, &iCount, sizeof(iCount));
		}
	#endif
	
	return XRT_NET_OK;
}



/* ============================== 连接操作 ============================== */

XXAPI xnet_result xrtSockBind(xnetconn* pConn, const xnetaddr* pAddr)
{
	if ( !pConn || !pAddr || pConn->hSocket == XSOCKET_INVALID ) return XRT_NET_ERROR;
	
	struct sockaddr_in tSA;
	xrtNetAddrToSockAddr(pAddr, &tSA);
	
	if ( bind(pConn->hSocket, (struct sockaddr*)&tSA, sizeof(tSA)) != 0 ) {
		return XRT_NET_ERROR;
	}
	
	pConn->tLocalAddr = *pAddr;
	return XRT_NET_OK;
}

XXAPI xnet_result xrtSockListen(xnetconn* pConn, int iBacklog)
{
	if ( !pConn || pConn->hSocket == XSOCKET_INVALID || pConn->iType != 0 ) {
		return XRT_NET_ERROR;
	}
	
	if ( listen(pConn->hSocket, iBacklog) != 0 ) {
		return XRT_NET_ERROR;
	}
	
	return XRT_NET_OK;
}

XXAPI xnet_result xrtSockAccept(xnetconn* pServer, xnetconn* pClient)
{
	if ( !pServer || !pClient || pServer->hSocket == XSOCKET_INVALID ) {
		return XRT_NET_ERROR;
	}
	
	struct sockaddr_in tSA;
	socklen_t iSALen = sizeof(tSA);
	
	memset(pClient, 0, sizeof(xnetconn));
	pClient->hSocket = accept(pServer->hSocket, (struct sockaddr*)&tSA, &iSALen);
	
	if ( pClient->hSocket == XSOCKET_INVALID ) {
		#if defined(_WIN32) || defined(_WIN64)
			int iErr = WSAGetLastError();
			if ( iErr == WSAEWOULDBLOCK ) return XRT_NET_AGAIN;
		#else
			if ( errno == EAGAIN || errno == EWOULDBLOCK ) return XRT_NET_AGAIN;
		#endif
		return XRT_NET_ERROR;
	}
	
	xrtNetAddrFromSockAddr(&pClient->tRemoteAddr, &tSA);
	pClient->iType = pServer->iType;
	
	return XRT_NET_OK;
}

XXAPI xnet_result xrtSockConnect(xnetconn* pConn, const xnetaddr* pAddr)
{
	if ( !pConn || !pAddr || pConn->hSocket == XSOCKET_INVALID ) return XRT_NET_ERROR;
	
	struct sockaddr_in tSA;
	xrtNetAddrToSockAddr(pAddr, &tSA);
	
	int iResult = connect(pConn->hSocket, (struct sockaddr*)&tSA, sizeof(tSA));
	if ( iResult != 0 ) {
		#if defined(_WIN32) || defined(_WIN64)
			int iErr = WSAGetLastError();
			if ( iErr == WSAEWOULDBLOCK ) return XRT_NET_AGAIN;
			if ( iErr == WSAEISCONN ) return XRT_NET_OK;
		#else
			if ( errno == EINPROGRESS ) return XRT_NET_AGAIN;
			if ( errno == EISCONN ) return XRT_NET_OK;
		#endif
		return XRT_NET_ERROR;
	}
	
	pConn->tRemoteAddr = *pAddr;
	return XRT_NET_OK;
}



/* ============================== 数据收发 ============================== */

XXAPI xnet_result xrtSockSend(xnetconn* pConn, const char* pData, size_t iLen, size_t* pSent)
{
	if ( !pConn || !pData || iLen == 0 || pConn->hSocket == XSOCKET_INVALID ) {
		return XRT_NET_ERROR;
	}
	
	#if defined(_WIN32) || defined(_WIN64)
		int iResult = send(pConn->hSocket, pData, (int)iLen, 0);
	#else
		ssize_t iResult = send(pConn->hSocket, pData, iLen, MSG_NOSIGNAL);
	#endif
	
	if ( iResult < 0 ) {
		#if defined(_WIN32) || defined(_WIN64)
			int iErr = WSAGetLastError();
			if ( iErr == WSAEWOULDBLOCK ) return XRT_NET_AGAIN;
		#else
			if ( errno == EAGAIN || errno == EWOULDBLOCK ) return XRT_NET_AGAIN;
		#endif
		return XRT_NET_ERROR;
	}
	
	if ( pSent ) *pSent = (size_t)iResult;
	return XRT_NET_OK;
}

XXAPI xnet_result xrtSockRecv(xnetconn* pConn, char* pBuf, size_t iLen, size_t* pReceived)
{
	if ( !pConn || !pBuf || iLen == 0 || pConn->hSocket == XSOCKET_INVALID ) {
		return XRT_NET_ERROR;
	}
	
	#if defined(_WIN32) || defined(_WIN64)
		int iResult = recv(pConn->hSocket, pBuf, (int)iLen, 0);
	#else
		ssize_t iResult = recv(pConn->hSocket, pBuf, iLen, 0);
	#endif
	
	if ( iResult < 0 ) {
		#if defined(_WIN32) || defined(_WIN64)
			int iErr = WSAGetLastError();
			if ( iErr == WSAEWOULDBLOCK ) return XRT_NET_AGAIN;
		#else
			if ( errno == EAGAIN || errno == EWOULDBLOCK ) return XRT_NET_AGAIN;
		#endif
		return XRT_NET_ERROR;
	}
	
	if ( iResult == 0 ) return XRT_NET_CLOSED;
	
	if ( pReceived ) *pReceived = (size_t)iResult;
	return XRT_NET_OK;
}

XXAPI xnet_result xrtSockSendTo(xnetconn* pConn, const char* pData, size_t iLen, const xnetaddr* pAddr, size_t* pSent)
{
	if ( !pConn || !pData || iLen == 0 || !pAddr || pConn->hSocket == XSOCKET_INVALID ) {
		return XRT_NET_ERROR;
	}
	
	struct sockaddr_in tSA;
	xrtNetAddrToSockAddr(pAddr, &tSA);
	
	#if defined(_WIN32) || defined(_WIN64)
		int iResult = sendto(pConn->hSocket, pData, (int)iLen, 0, (struct sockaddr*)&tSA, sizeof(tSA));
	#else
		ssize_t iResult = sendto(pConn->hSocket, pData, iLen, 0, (struct sockaddr*)&tSA, sizeof(tSA));
	#endif
	
	if ( iResult < 0 ) {
		#if defined(_WIN32) || defined(_WIN64)
			int iErr = WSAGetLastError();
			if ( iErr == WSAEWOULDBLOCK ) return XRT_NET_AGAIN;
		#else
			if ( errno == EAGAIN || errno == EWOULDBLOCK ) return XRT_NET_AGAIN;
		#endif
		return XRT_NET_ERROR;
	}
	
	if ( pSent ) *pSent = (size_t)iResult;
	return XRT_NET_OK;
}

XXAPI xnet_result xrtSockRecvFrom(xnetconn* pConn, char* pBuf, size_t iLen, xnetaddr* pAddr, size_t* pReceived)
{
	if ( !pConn || !pBuf || iLen == 0 || pConn->hSocket == XSOCKET_INVALID ) {
		return XRT_NET_ERROR;
	}
	
	struct sockaddr_in tSA;
	socklen_t iSALen = sizeof(tSA);
	
	#if defined(_WIN32) || defined(_WIN64)
		int iResult = recvfrom(pConn->hSocket, pBuf, (int)iLen, 0, (struct sockaddr*)&tSA, &iSALen);
	#else
		ssize_t iResult = recvfrom(pConn->hSocket, pBuf, iLen, 0, (struct sockaddr*)&tSA, &iSALen);
	#endif
	
	if ( iResult < 0 ) {
		#if defined(_WIN32) || defined(_WIN64)
			int iErr = WSAGetLastError();
			if ( iErr == WSAEWOULDBLOCK ) return XRT_NET_AGAIN;
		#else
			if ( errno == EAGAIN || errno == EWOULDBLOCK ) return XRT_NET_AGAIN;
		#endif
		return XRT_NET_ERROR;
	}
	
	if ( pAddr ) {
		xrtNetAddrFromSockAddr(pAddr, &tSA);
	}
	
	if ( iResult == 0 ) return XRT_NET_CLOSED;
	
	if ( pReceived ) *pReceived = (size_t)iResult;
	return XRT_NET_OK;
}



/* ============================== DNS 解析 ============================== */

XXAPI xnet_result xrtNetResolve(const char* sHostname, xnetaddr* pAddr)
{
	if ( !sHostname || !pAddr ) return XRT_NET_ERROR;
	
	struct hostent* pHost = gethostbyname(sHostname);
	if ( !pHost || pHost->h_addrtype != AF_INET || !pHost->h_addr_list[0] ) {
		return XRT_NET_ERROR;
	}
	
	struct in_addr tInAddr;
	memcpy(&tInAddr, pHost->h_addr_list[0], sizeof(struct in_addr));
	
	pAddr->iAddr = tInAddr.s_addr;
	const char* sIP = inet_ntoa(tInAddr);
	if ( sIP ) {
		strncpy(pAddr->sAddr, sIP, 15);
		pAddr->sAddr[15] = '\0';
	}
	
	return XRT_NET_OK;
}



/* ============================== 网络缓冲区 ============================== */

XXAPI bool xrtNetBufInit(xnetbuf* pBuf, size_t iCapacity)
{
	if ( !pBuf ) return false;
	pBuf->pData = (char*)xrtMalloc(iCapacity);
	if ( !pBuf->pData ) return false;
	pBuf->iSize = 0;
	pBuf->iCapacity = iCapacity;
	return true;
}

XXAPI void xrtNetBufFree(xnetbuf* pBuf)
{
	if ( !pBuf ) return;
	if ( pBuf->pData ) {
		xrtFree(pBuf->pData);
		pBuf->pData = NULL;
	}
	pBuf->iSize = 0;
	pBuf->iCapacity = 0;
}

XXAPI bool xrtNetBufAppend(xnetbuf* pBuf, const char* pData, size_t iLen)
{
	if ( !pBuf || !pData || iLen == 0 ) return false;
	
	// 需要扩容
	if ( pBuf->iSize + iLen > pBuf->iCapacity ) {
		size_t iNewCap = pBuf->iCapacity * 2;
		if ( iNewCap < pBuf->iSize + iLen ) {
			iNewCap = pBuf->iSize + iLen;
		}
		char* pNew = (char*)xrtRealloc(pBuf->pData, iNewCap);
		if ( !pNew ) return false;
		pBuf->pData = pNew;
		pBuf->iCapacity = iNewCap;
	}
	
	memcpy(pBuf->pData + pBuf->iSize, pData, iLen);
	pBuf->iSize += iLen;
	return true;
}

XXAPI void xrtNetBufConsume(xnetbuf* pBuf, size_t iLen)
{
	if ( !pBuf || iLen == 0 ) return;
	if ( iLen >= pBuf->iSize ) {
		pBuf->iSize = 0;
	} else {
		memmove(pBuf->pData, pBuf->pData + iLen, pBuf->iSize - iLen);
		pBuf->iSize -= iLen;
	}
}

XXAPI void xrtNetBufClear(xnetbuf* pBuf)
{
	if ( !pBuf ) return;
	pBuf->iSize = 0;
}


