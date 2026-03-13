


/*
	NetProxy - 代理协议支持 [Ver1.0]
	
	支持 SOCKS5 和 HTTP CONNECT 代理协议
	同步阻塞式 API，用于 TCP 客户端代理连接
	
	SOCKS5: RFC 1928 + RFC 1929 (用户名/密码认证)
	HTTP CONNECT: RFC 7231
*/



/* ============================== 内部工具函数 ============================== */

// 阻塞发送全部数据
static xnet_result __xrt_proxy_send_all(xnetconn* pConn, const char* pData, size_t iLen)
{
	size_t iSent = 0;
	while ( iSent < iLen ) {
		size_t iThisSent = 0;
		xnet_result iRes = xrtSockSend(pConn, pData + iSent, iLen - iSent, &iThisSent);
		if ( iRes != XRT_NET_OK ) {
			if ( iRes == XRT_NET_AGAIN ) continue;
			return iRes;
		}
		iSent += iThisSent;
	}
	return XRT_NET_OK;
}

// 阻塞接收指定长度数据 (带超时)
static xnet_result __xrt_proxy_recv_exact(xnetconn* pConn, char* pBuf, size_t iLen, int iTimeoutMs)
{
	size_t iReceived = 0;
	int iElapsed = 0;
	int iInterval = 50;  // 50ms 轮询间隔
	
	while ( iReceived < iLen ) {
		size_t iThisRecv = 0;
		xnet_result iRes = xrtSockRecv(pConn, pBuf + iReceived, iLen - iReceived, &iThisRecv);
		if ( iRes == XRT_NET_OK ) {
			iReceived += iThisRecv;
		} else if ( iRes == XRT_NET_AGAIN ) {
			// 等待数据
			#if defined(_WIN32) || defined(_WIN64)
				Sleep(iInterval);
			#else
				usleep(iInterval * 1000);
			#endif
			iElapsed += iInterval;
			if ( iTimeoutMs > 0 && iElapsed >= iTimeoutMs ) {
				return XRT_NET_TIMEOUT;
			}
		} else if ( iRes == XRT_NET_CLOSED ) {
			return XRT_NET_CLOSED;
		} else {
			return XRT_NET_ERROR;
		}
	}
	return XRT_NET_OK;
}

// 阻塞接收数据直到有数据 (带超时，返回实际接收长度)
static xnet_result __xrt_proxy_recv_some(xnetconn* pConn, char* pBuf, size_t iMaxLen, size_t* pReceived, int iTimeoutMs)
{
	int iElapsed = 0;
	int iInterval = 50;
	
	while ( 1 ) {
		size_t iThisRecv = 0;
		xnet_result iRes = xrtSockRecv(pConn, pBuf, iMaxLen, &iThisRecv);
		if ( iRes == XRT_NET_OK ) {
			if ( pReceived ) *pReceived = iThisRecv;
			return XRT_NET_OK;
		} else if ( iRes == XRT_NET_AGAIN ) {
			#if defined(_WIN32) || defined(_WIN64)
				Sleep(iInterval);
			#else
				usleep(iInterval * 1000);
			#endif
			iElapsed += iInterval;
			if ( iTimeoutMs > 0 && iElapsed >= iTimeoutMs ) {
				return XRT_NET_TIMEOUT;
			}
		} else {
			return iRes;
		}
	}
}



/* ============================== SOCKS5 协议实现 ============================== */

/*
	SOCKS5 握手流程:
	1. 客户端发送: VER(1) | NMETHODS(1) | METHODS(1-255)
	2. 服务端响应: VER(1) | METHOD(1)
	3. 如果需要认证 (METHOD=0x02):
	   - 客户端发送: VER(1) | ULEN(1) | UNAME(1-255) | PLEN(1) | PASSWD(1-255)
	   - 服务端响应: VER(1) | STATUS(1)
	4. 客户端发送 CONNECT 请求: VER(1) | CMD(1) | RSV(1) | ATYP(1) | DST.ADDR | DST.PORT(2)
	5. 服务端响应: VER(1) | REP(1) | RSV(1) | ATYP(1) | BND.ADDR | BND.PORT(2)
*/

XXAPI xnet_result xrtProxySocks5Connect(xnetconn* pConn, const char* sTargetHost, uint16 iTargetPort,
	const char* sUser, const char* sPass, int iTimeoutMs)
{
	if ( !pConn || !sTargetHost || iTargetPort == 0 ) return XRT_NET_ERROR;
	
	uint8 aBuf[512];
	xnet_result iRes;
	bool bNeedAuth = (sUser && sUser[0] && sPass && sPass[0]);
	
	// ========== Step 1: 发送认证方法协商 ==========
	// VER=0x05, NMETHODS=1或2, METHODS=0x00(无认证) [0x02(用户名/密码)]
	aBuf[0] = 0x05;  // SOCKS5
	if ( bNeedAuth ) {
		aBuf[1] = 0x02;  // 2种方法
		aBuf[2] = 0x00;  // 无认证
		aBuf[3] = 0x02;  // 用户名/密码
		iRes = __xrt_proxy_send_all(pConn, (char*)aBuf, 4);
	} else {
		aBuf[1] = 0x01;  // 1种方法
		aBuf[2] = 0x00;  // 无认证
		iRes = __xrt_proxy_send_all(pConn, (char*)aBuf, 3);
	}
	if ( iRes != XRT_NET_OK ) return iRes;
	
	// ========== Step 2: 接收服务端选择的认证方法 ==========
	iRes = __xrt_proxy_recv_exact(pConn, (char*)aBuf, 2, iTimeoutMs);
	if ( iRes != XRT_NET_OK ) return iRes;
	
	if ( aBuf[0] != 0x05 ) return XRT_NET_ERROR;  // 版本错误
	
	uint8 iMethod = aBuf[1];
	if ( iMethod == 0xFF ) return XRT_NET_ERROR;  // 无可接受的方法
	
	// ========== Step 3: 如果需要用户名/密码认证 ==========
	if ( iMethod == 0x02 ) {
		if ( !bNeedAuth ) return XRT_NET_ERROR;  // 服务端要求认证但未提供凭证
		
		size_t iUserLen = strlen(sUser);
		size_t iPassLen = strlen(sPass);
		if ( iUserLen > 255 || iPassLen > 255 ) return XRT_NET_ERROR;
		
		// VER=0x01, ULEN, UNAME, PLEN, PASSWD
		size_t iPos = 0;
		aBuf[iPos++] = 0x01;
		aBuf[iPos++] = (uint8)iUserLen;
		memcpy(aBuf + iPos, sUser, iUserLen);
		iPos += iUserLen;
		aBuf[iPos++] = (uint8)iPassLen;
		memcpy(aBuf + iPos, sPass, iPassLen);
		iPos += iPassLen;
		
		iRes = __xrt_proxy_send_all(pConn, (char*)aBuf, iPos);
		if ( iRes != XRT_NET_OK ) return iRes;
		
		// 接收认证结果
		iRes = __xrt_proxy_recv_exact(pConn, (char*)aBuf, 2, iTimeoutMs);
		if ( iRes != XRT_NET_OK ) return iRes;
		
		if ( aBuf[1] != 0x00 ) return XRT_NET_ERROR;  // 认证失败
	} else if ( iMethod != 0x00 ) {
		return XRT_NET_ERROR;  // 不支持的认证方法
	}
	
	// ========== Step 4: 发送 CONNECT 请求 (域名模式) ==========
	// VER=0x05, CMD=0x01(CONNECT), RSV=0x00, ATYP=0x03(域名), DST.ADDR, DST.PORT
	size_t iHostLen = strlen(sTargetHost);
	if ( iHostLen > 255 ) return XRT_NET_ERROR;
	
	size_t iPos = 0;
	aBuf[iPos++] = 0x05;              // VER
	aBuf[iPos++] = 0x01;              // CMD: CONNECT
	aBuf[iPos++] = 0x00;              // RSV
	aBuf[iPos++] = 0x03;              // ATYP: 域名
	aBuf[iPos++] = (uint8)iHostLen;   // 域名长度
	memcpy(aBuf + iPos, sTargetHost, iHostLen);
	iPos += iHostLen;
	aBuf[iPos++] = (uint8)(iTargetPort >> 8);    // 端口高字节
	aBuf[iPos++] = (uint8)(iTargetPort & 0xFF);  // 端口低字节
	
	iRes = __xrt_proxy_send_all(pConn, (char*)aBuf, iPos);
	if ( iRes != XRT_NET_OK ) return iRes;
	
	// ========== Step 5: 接收 CONNECT 响应 ==========
	// 先读取固定头部: VER, REP, RSV, ATYP (4字节)
	iRes = __xrt_proxy_recv_exact(pConn, (char*)aBuf, 4, iTimeoutMs);
	if ( iRes != XRT_NET_OK ) return iRes;
	
	if ( aBuf[0] != 0x05 ) return XRT_NET_ERROR;  // 版本错误
	if ( aBuf[1] != 0x00 ) return XRT_NET_ERROR;  // CONNECT 失败 (REP != 0)
	
	// 根据 ATYP 读取剩余数据
	uint8 iAtyp = aBuf[3];
	size_t iAddrLen = 0;
	if ( iAtyp == 0x01 ) {
		iAddrLen = 4;   // IPv4: 4字节
	} else if ( iAtyp == 0x03 ) {
		// 域名: 先读取长度
		iRes = __xrt_proxy_recv_exact(pConn, (char*)aBuf, 1, iTimeoutMs);
		if ( iRes != XRT_NET_OK ) return iRes;
		iAddrLen = aBuf[0];
	} else if ( iAtyp == 0x04 ) {
		iAddrLen = 16;  // IPv6: 16字节
	} else {
		return XRT_NET_ERROR;
	}
	
	// 读取地址 + 端口 (2字节)
	iRes = __xrt_proxy_recv_exact(pConn, (char*)aBuf, iAddrLen + 2, iTimeoutMs);
	if ( iRes != XRT_NET_OK ) return iRes;
	
	// SOCKS5 隧道建立成功
	return XRT_NET_OK;
}



/* ============================== HTTP CONNECT 协议实现 ============================== */

/*
	HTTP CONNECT 握手流程:
	1. 客户端发送: CONNECT host:port HTTP/1.1\r
Host: host:port\r
[Proxy-Authorization: Basic base64]\r
\r

	2. 服务端响应: HTTP/1.x 200 ...\r\n\r\n
*/

XXAPI xnet_result xrtProxyHttpConnect(xnetconn* pConn, const char* sTargetHost, uint16 iTargetPort,
	const char* sUser, const char* sPass, int iTimeoutMs)
{
	if ( !pConn || !sTargetHost || iTargetPort == 0 ) return XRT_NET_ERROR;
	
	char aBuf[1024];
	char aResp[1024];
	xnet_result iRes;
	bool bNeedAuth = (sUser && sUser[0] && sPass && sPass[0]);
	
	// ========== Step 1: 构造 CONNECT 请求 ==========
	size_t iLen;
	if ( bNeedAuth ) {
		// 构造 Basic 认证字符串: base64(user:pass)
		char aCredentials[256];
		snprintf(aCredentials, sizeof(aCredentials), "%s:%s", sUser, sPass);
		
		// Base64 编码
		char aBase64[512];
		size_t iCredLen = strlen(aCredentials);
		size_t iBase64Len = 0;
		
		// 简单的 Base64 编码
		static const char* sBase64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		size_t i;
		for ( i = 0; i + 2 < iCredLen; i += 3 ) {
			aBase64[iBase64Len++] = sBase64Chars[(aCredentials[i] >> 2) & 0x3F];
			aBase64[iBase64Len++] = sBase64Chars[((aCredentials[i] & 0x03) << 4) | ((aCredentials[i+1] >> 4) & 0x0F)];
			aBase64[iBase64Len++] = sBase64Chars[((aCredentials[i+1] & 0x0F) << 2) | ((aCredentials[i+2] >> 6) & 0x03)];
			aBase64[iBase64Len++] = sBase64Chars[aCredentials[i+2] & 0x3F];
		}
		if ( i < iCredLen ) {
			aBase64[iBase64Len++] = sBase64Chars[(aCredentials[i] >> 2) & 0x3F];
			if ( i + 1 < iCredLen ) {
				aBase64[iBase64Len++] = sBase64Chars[((aCredentials[i] & 0x03) << 4) | ((aCredentials[i+1] >> 4) & 0x0F)];
				aBase64[iBase64Len++] = sBase64Chars[((aCredentials[i+1] & 0x0F) << 2)];
				aBase64[iBase64Len++] = '=';
			} else {
				aBase64[iBase64Len++] = sBase64Chars[((aCredentials[i] & 0x03) << 4)];
				aBase64[iBase64Len++] = '=';
				aBase64[iBase64Len++] = '=';
			}
		}
		aBase64[iBase64Len] = '\0';
		
		iLen = snprintf(aBuf, sizeof(aBuf),
			"CONNECT %s:%u HTTP/1.1\r\n"
			"Host: %s:%u\r\n"
			"Proxy-Authorization: Basic %s\r\n"
			"Proxy-Connection: Keep-Alive\r\n"
			"\r\n",
			sTargetHost, iTargetPort,
			sTargetHost, iTargetPort,
			aBase64);
	} else {
		iLen = snprintf(aBuf, sizeof(aBuf),
			"CONNECT %s:%u HTTP/1.1\r\n"
			"Host: %s:%u\r\n"
			"Proxy-Connection: Keep-Alive\r\n"
			"\r\n",
			sTargetHost, iTargetPort,
			sTargetHost, iTargetPort);
	}
	
	// ========== Step 2: 发送 CONNECT 请求 ==========
	iRes = __xrt_proxy_send_all(pConn, aBuf, iLen);
	if ( iRes != XRT_NET_OK ) return iRes;
	
	// ========== Step 3: 接收响应 ==========
	// 读取 HTTP 响应直到 \r\n\r\n
	size_t iRespLen = 0;
	int iElapsed = 0;
	int iInterval = 50;
	
	while ( iRespLen < sizeof(aResp) - 1 ) {
		size_t iRecv = 0;
		iRes = xrtSockRecv(pConn, aResp + iRespLen, 1, &iRecv);
		if ( iRes == XRT_NET_OK && iRecv > 0 ) {
			iRespLen++;
			// 检查是否收到 \r\n\r\n
			if ( iRespLen >= 4 &&
				aResp[iRespLen-4] == '\r' && aResp[iRespLen-3] == '\n' &&
				aResp[iRespLen-2] == '\r' && aResp[iRespLen-1] == '\n' ) {
				break;
			}
		} else if ( iRes == XRT_NET_AGAIN ) {
			#if defined(_WIN32) || defined(_WIN64)
				Sleep(iInterval);
			#else
				usleep(iInterval * 1000);
			#endif
			iElapsed += iInterval;
			if ( iTimeoutMs > 0 && iElapsed >= iTimeoutMs ) {
				return XRT_NET_TIMEOUT;
			}
		} else {
			return iRes;
		}
	}
	aResp[iRespLen] = '\0';
	
	// ========== Step 4: 解析响应 ==========
	// 检查是否为 HTTP/1.x 200
	if ( iRespLen < 12 ) return XRT_NET_ERROR;
	if ( strncmp(aResp, "HTTP/1.", 7) != 0 ) return XRT_NET_ERROR;
	
	// 找状态码
	char* pStatus = strchr(aResp, ' ');
	if ( !pStatus ) return XRT_NET_ERROR;
	pStatus++;
	
	int iStatusCode = atoi(pStatus);
	if ( iStatusCode != 200 ) {
		// 407 = Proxy Authentication Required
		return XRT_NET_ERROR;
	}
	
	// HTTP CONNECT 隧道建立成功
	return XRT_NET_OK;
}



/* ============================== 通用代理连接 ============================== */

XXAPI xnet_result xrtProxyConnect(xnetconn* pConn, const xproxyconfig* pProxy,
	const char* sTargetHost, uint16 iTargetPort, int iTimeoutMs)
{
	if ( !pConn || !pProxy || !sTargetHost || iTargetPort == 0 ) return XRT_NET_ERROR;
	
	switch ( pProxy->iType ) {
		case XRT_PROXY_SOCKS5:
			return xrtProxySocks5Connect(pConn, sTargetHost, iTargetPort,
				pProxy->sUser[0] ? pProxy->sUser : NULL,
				pProxy->sPass[0] ? pProxy->sPass : NULL,
				iTimeoutMs);
		
		case XRT_PROXY_HTTP_CONNECT:
			return xrtProxyHttpConnect(pConn, sTargetHost, iTargetPort,
				pProxy->sUser[0] ? pProxy->sUser : NULL,
				pProxy->sPass[0] ? pProxy->sPass : NULL,
				iTimeoutMs);
		
		default:
			return XRT_NET_ERROR;
	}
}


