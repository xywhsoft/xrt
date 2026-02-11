



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
	// 线程安全: 使用 inet_ntop 代替 inet_ntoa
	#if defined(_WIN32) || defined(_WIN64)
		InetNtopA(AF_INET, &pSA->sin_addr, pAddr->sAddr, sizeof(pAddr->sAddr));
	#else
		inet_ntop(AF_INET, &pSA->sin_addr, pAddr->sAddr, sizeof(pAddr->sAddr));
	#endif
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
	// 线程安全: 使用 inet_ntop + xrt 环形临时内存
	char aTmp[16];
	#if defined(_WIN32) || defined(_WIN64)
		InetNtopA(AF_INET, &tAddr, aTmp, sizeof(aTmp));
	#else
		inet_ntop(AF_INET, &tAddr, aTmp, sizeof(aTmp));
	#endif
	str sCopy = xrtCopyStr(aTmp, strlen(aTmp));
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
	
	#ifdef __TINYC__
		// TCC 不支持 getaddrinfo，使用 gethostbyname
		struct hostent* pHost = gethostbyname(sHostname);
		if ( !pHost || pHost->h_addrtype != AF_INET || !pHost->h_addr_list[0] ) {
			return XRT_NET_ERROR;
		}
		struct in_addr tInAddr;
		memcpy(&tInAddr, pHost->h_addr_list[0], sizeof(struct in_addr));
		pAddr->iAddr = tInAddr.s_addr;
		#if defined(_WIN32) || defined(_WIN64)
			InetNtopA(AF_INET, &tInAddr, pAddr->sAddr, sizeof(pAddr->sAddr));
		#else
			inet_ntop(AF_INET, &tInAddr, pAddr->sAddr, sizeof(pAddr->sAddr));
		#endif
	#else
		// 线程安全: 使用 getaddrinfo + inet_ntop
		struct addrinfo tHints, *pRes;
		memset(&tHints, 0, sizeof(tHints));
		tHints.ai_family = AF_INET;
		tHints.ai_socktype = SOCK_STREAM;
		if ( getaddrinfo(sHostname, NULL, &tHints, &pRes) != 0 ) {
			return XRT_NET_ERROR;
		}
		if ( !pRes ) return XRT_NET_ERROR;
		struct sockaddr_in* pSA = (struct sockaddr_in*)pRes->ai_addr;
		pAddr->iAddr = pSA->sin_addr.s_addr;
		#if defined(_WIN32) || defined(_WIN64)
			InetNtopA(AF_INET, &pSA->sin_addr, pAddr->sAddr, sizeof(pAddr->sAddr));
		#else
			inet_ntop(AF_INET, &pSA->sin_addr, pAddr->sAddr, sizeof(pAddr->sAddr));
		#endif
		freeaddrinfo(pRes);
	#endif
	
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



/* ============================== 环形网络缓冲区 ============================== */

// 向上对齐到 2 的幂
static inline size_t __xrt_ringbuf_next_pow2(size_t v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	#if defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__) || defined(__LP64__)
		v |= v >> 32;
	#endif
	v++;
	return v;
}

XXAPI bool xrtNetRingBufInit(xnetringbuf* pBuf, size_t iCapacity)
{
	if ( !pBuf || iCapacity == 0 ) return false;
	size_t iAligned = __xrt_ringbuf_next_pow2(iCapacity);
	if ( iAligned < 16 ) iAligned = 16;
	pBuf->pData = (char*)xrtMalloc(iAligned);
	if ( !pBuf->pData ) return false;
	pBuf->iCapacity = iAligned;
	pBuf->iMask = iAligned - 1;
	pBuf->iReadPos = 0;
	pBuf->iWritePos = 0;
	return true;
}

XXAPI void xrtNetRingBufFree(xnetringbuf* pBuf)
{
	if ( !pBuf ) return;
	if ( pBuf->pData ) {
		xrtFree(pBuf->pData);
		pBuf->pData = NULL;
	}
	pBuf->iCapacity = 0;
	pBuf->iMask = 0;
	pBuf->iReadPos = 0;
	pBuf->iWritePos = 0;
}

XXAPI size_t xrtNetRingBufReadable(const xnetringbuf* pBuf)
{
	if ( !pBuf ) return 0;
	return pBuf->iWritePos - pBuf->iReadPos;
}

XXAPI size_t xrtNetRingBufWritable(const xnetringbuf* pBuf)
{
	if ( !pBuf ) return 0;
	return pBuf->iCapacity - (pBuf->iWritePos - pBuf->iReadPos);
}

XXAPI size_t xrtNetRingBufWrite(xnetringbuf* pBuf, const char* pData, size_t iLen)
{
	if ( !pBuf || !pData || iLen == 0 ) return 0;
	size_t iAvail = xrtNetRingBufWritable(pBuf);
	if ( iLen > iAvail ) iLen = iAvail;
	if ( iLen == 0 ) return 0;
	
	size_t iPos = pBuf->iWritePos & pBuf->iMask;
	size_t iFirstChunk = pBuf->iCapacity - iPos;
	if ( iFirstChunk >= iLen ) {
		memcpy(pBuf->pData + iPos, pData, iLen);
	} else {
		memcpy(pBuf->pData + iPos, pData, iFirstChunk);
		memcpy(pBuf->pData, pData + iFirstChunk, iLen - iFirstChunk);
	}
	pBuf->iWritePos += iLen;
	return iLen;
}

XXAPI size_t xrtNetRingBufPeek(xnetringbuf* pBuf, char* pOut, size_t iLen)
{
	if ( !pBuf || !pOut || iLen == 0 ) return 0;
	size_t iReadable = xrtNetRingBufReadable(pBuf);
	if ( iLen > iReadable ) iLen = iReadable;
	if ( iLen == 0 ) return 0;
	
	size_t iPos = pBuf->iReadPos & pBuf->iMask;
	size_t iFirstChunk = pBuf->iCapacity - iPos;
	if ( iFirstChunk >= iLen ) {
		memcpy(pOut, pBuf->pData + iPos, iLen);
	} else {
		memcpy(pOut, pBuf->pData + iPos, iFirstChunk);
		memcpy(pOut + iFirstChunk, pBuf->pData, iLen - iFirstChunk);
	}
	return iLen;
}

XXAPI size_t xrtNetRingBufRead(xnetringbuf* pBuf, char* pOut, size_t iLen)
{
	size_t iRead = xrtNetRingBufPeek(pBuf, pOut, iLen);
	pBuf->iReadPos += iRead;
	return iRead;
}

XXAPI void xrtNetRingBufConsume(xnetringbuf* pBuf, size_t iLen)
{
	if ( !pBuf ) return;
	size_t iReadable = xrtNetRingBufReadable(pBuf);
	if ( iLen > iReadable ) iLen = iReadable;
	pBuf->iReadPos += iLen;
}


