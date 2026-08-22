#ifndef XRT_XNET_PROXY_H
#define XRT_XNET_PROXY_H

/*
	XNet internal proxy handshake helpers.

	This header keeps protocol parsing isolated from stream lifecycle so the
	mainline transport can drive proxy negotiation as another pre-open stage.
*/


#define __XNET_PROXY_RECV_INITIAL     256u
#define __XNET_PROXY_RECV_MAX         8192u

#define __XNET_PROXY_ACTION_WAIT      0u
#define __XNET_PROXY_ACTION_SEND      1u
#define __XNET_PROXY_ACTION_READY     2u
#define __XNET_PROXY_ACTION_ERROR     3u

#define __XNET_PROXY_STAGE_INIT               0u
#define __XNET_PROXY_STAGE_SOCKS5_METHOD      1u
#define __XNET_PROXY_STAGE_SOCKS5_AUTH        2u
#define __XNET_PROXY_STAGE_SOCKS5_CONNECT     3u
#define __XNET_PROXY_STAGE_HTTP_CONNECT       4u
#define __XNET_PROXY_STAGE_READY              5u

typedef struct {
	int iType;
	uint16 iTargetPort;
	uint8 iStage;
	bool bTargetAddrValid;
	char sTargetHost[XNET_PROXY_HOST_CAP];
	xnetaddr tTargetAddr;
	char* pRecv;
	uint32 iRecvLen;
	uint32 iRecvCapacity;
} __xnet_proxy_state;


// 内部函数：__xnetProxyHasAuth
static bool __xnetProxyHasAuth(const xnetproxy* pProxy)
{
	if ( !pProxy ) { return false; }
	return pProxy->tConfig.sUser[0] != '\0' || pProxy->tConfig.sPass[0] != '\0';
}


// 内部函数：__xnetProxyConsumeRecv
static void __xnetProxyConsumeRecv(__xnet_proxy_state* pState, uint32 iLen)
{
	if ( !pState || iLen == 0 ) { return; }
	if ( iLen >= pState->iRecvLen ) {
		pState->iRecvLen = 0;
		return;
	}
	memmove(pState->pRecv, pState->pRecv + iLen, pState->iRecvLen - iLen);
	pState->iRecvLen -= iLen;
}


// 内部函数：__xnetProxyAppendRecv
static bool __xnetProxyAppendRecv(__xnet_proxy_state* pState, const void* pData, size_t iLen)
{
	if ( !pState || (!pData && iLen > 0) ) { return false; }
	if ( iLen == 0 ) { return true; }
	if ( iLen > (size_t)(__XNET_PROXY_RECV_MAX - pState->iRecvLen) ) { return false; }
	if ( (size_t)pState->iRecvLen + iLen > pState->iRecvCapacity ) {
		uint32 iCapacity = pState->iRecvCapacity
			? pState->iRecvCapacity : __XNET_PROXY_RECV_INITIAL;
		char* pNew;
		while ( (size_t)iCapacity < (size_t)pState->iRecvLen + iLen ) {
			iCapacity *= 2u;
			if ( iCapacity > __XNET_PROXY_RECV_MAX ) {
				iCapacity = __XNET_PROXY_RECV_MAX;
				break;
			}
		}
		pNew = (char*)XNET_ALLOC(iCapacity);
		if ( !pNew ) { return false; }
		if ( pState->iRecvLen > 0u ) { memcpy(pNew, pState->pRecv, pState->iRecvLen); }
		if ( pState->pRecv ) { XNET_FREE(pState->pRecv); }
		pState->pRecv = pNew;
		pState->iRecvCapacity = iCapacity;
	}
	memcpy(pState->pRecv + pState->iRecvLen, pData, iLen);
	pState->iRecvLen += (uint32)iLen;
	return true;
}


// 内部函数：格式化代理授权段
static bool __xnetProxyFormatAuthority(const char* sHost, uint16 iPort, char* sOut, size_t iOutCap)
{
	xnetaddr tAddr;
	int iWritten;
	if ( !sHost || !sHost[0] || !sOut || iOutCap == 0 ) { return false; }
	memset(&tAddr, 0, sizeof(tAddr));
	if ( xrtNetAddrParse(&tAddr, sHost, iPort) == XRT_NET_OK && tAddr.iFamily == AF_INET6 ) {
		iWritten = snprintf(sOut, iOutCap, "[%s]:%u", sHost, (unsigned)iPort);
	} else {
		iWritten = snprintf(sOut, iOutCap, "%s:%u", sHost, (unsigned)iPort);
	}
	return iWritten > 0 && (size_t)iWritten < iOutCap;
}


// 内部函数：__xnetProxyBuildSocks5Greeting
static bool __xnetProxyBuildSocks5Greeting(const xnetproxy* pProxy, char* pOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iLen = 0;
	if ( pOutLen ) { *pOutLen = 0; }
	if ( !pProxy || !pOut || iOutCap < 4u ) { return false; }
	pOut[iLen++] = 0x05;
	if ( __xnetProxyHasAuth(pProxy) ) {
		pOut[iLen++] = 0x02;
		pOut[iLen++] = 0x00;
		pOut[iLen++] = 0x02;
	} else {
		pOut[iLen++] = 0x01;
		pOut[iLen++] = 0x00;
	}
	if ( pOutLen ) { *pOutLen = iLen; }
	return true;
}


// 内部函数：__xnetProxyBuildSocks5Auth
static bool __xnetProxyBuildSocks5Auth(const xnetproxy* pProxy, char* pOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iUserLen;
	size_t iPassLen;
	size_t iPos = 0;
	if ( pOutLen ) { *pOutLen = 0; }
	if ( !pProxy || !pOut ) { return false; }
	iUserLen = strlen(pProxy->tConfig.sUser);
	iPassLen = strlen(pProxy->tConfig.sPass);
	if ( iUserLen > 255u || iPassLen > 255u ) { return false; }
	if ( iOutCap < (size_t)(3u + iUserLen + iPassLen) ) { return false; }
	pOut[iPos++] = 0x01;
	pOut[iPos++] = (uint8)iUserLen;
	if ( iUserLen > 0 ) {
		memcpy(pOut + iPos, pProxy->tConfig.sUser, iUserLen);
		iPos += iUserLen;
	}
	pOut[iPos++] = (uint8)iPassLen;
	if ( iPassLen > 0 ) {
		memcpy(pOut + iPos, pProxy->tConfig.sPass, iPassLen);
		iPos += iPassLen;
	}
	if ( pOutLen ) { *pOutLen = iPos; }
	return true;
}


// 内部函数：__xnetProxyBuildSocks5ConnectReq
static bool __xnetProxyBuildSocks5ConnectReq(const __xnet_proxy_state* pState, char* pOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iPos = 0;
	size_t iHostLen;
	if ( pOutLen ) { *pOutLen = 0; }
	if ( !pState || !pOut || !pState->sTargetHost[0] || pState->iTargetPort == 0 ) { return false; }

	// 写入 SOCKS5 请求头：版本号(0x05)、连接命令(0x01)、保留字段(0x00)
	pOut[iPos++] = 0x05;
	pOut[iPos++] = 0x01;
	pOut[iPos++] = 0x00;

	// 根据目标地址类型写入地址字段：IPv4(0x01)、IPv6(0x04)、域名(0x03)
	if ( pState->bTargetAddrValid && pState->tTargetAddr.iFamily == AF_INET ) {
		if ( iOutCap < iPos + 1u + 4u + 2u ) { return false; }
		pOut[iPos++] = 0x01;
		memcpy(pOut + iPos, pState->tTargetAddr.aAddr, 4u);
		iPos += 4u;
	} else if ( pState->bTargetAddrValid && pState->tTargetAddr.iFamily == AF_INET6 ) {
		if ( iOutCap < iPos + 1u + 16u + 2u ) { return false; }
		pOut[iPos++] = 0x04;
		memcpy(pOut + iPos, pState->tTargetAddr.aAddr, 16u);
		iPos += 16u;
	} else {
		iHostLen = strlen(pState->sTargetHost);
		if ( iHostLen == 0 || iHostLen > 255u ) { return false; }
		if ( iOutCap < iPos + 1u + 1u + iHostLen + 2u ) { return false; }
		pOut[iPos++] = 0x03;
		pOut[iPos++] = (uint8)iHostLen;
		memcpy(pOut + iPos, pState->sTargetHost, iHostLen);
		iPos += iHostLen;
	}

	// 写入目标端口（网络字节序，高位在前）
	pOut[iPos++] = (char)((pState->iTargetPort >> 8) & 0xFFu);
	pOut[iPos++] = (char)(pState->iTargetPort & 0xFFu);
	if ( pOutLen ) { *pOutLen = iPos; }
	return true;
}


// 内部函数：__xnetProxyBuildHttpConnectReq
static bool __xnetProxyBuildHttpConnectReq(const xnetproxy* pProxy, const __xnet_proxy_state* pState, char* pOut, size_t iOutCap, size_t* pOutLen)
{
	char sAuthority[384];
	int iWritten;
	if ( pOutLen ) { *pOutLen = 0; }
	if ( !pProxy || !pState || !pOut ) { return false; }
	// 格式化目标地址为 authority 字符串（IPv6 地址加方括号）
	if ( !__xnetProxyFormatAuthority(pState->sTargetHost, pState->iTargetPort, sAuthority, sizeof(sAuthority)) ) { return false; }

	// 根据是否配置了认证信息，构建带或不带 Proxy-Authorization 的 CONNECT 请求
	if ( __xnetProxyHasAuth(pProxy) ) {
		// 拼接 "用户名:密码" 并进行 Base64 编码
		char sCred[512];
		str sAuth;
		iWritten = snprintf(sCred, sizeof(sCred), "%s:%s", pProxy->tConfig.sUser, pProxy->tConfig.sPass);
		if ( iWritten < 0 || (size_t)iWritten >= sizeof(sCred) ) { return false; }
		sAuth = xrtBase64Encode(sCred, (size_t)iWritten, NULL);
		if ( !sAuth || sAuth == xCore.sNull || !sAuth[0] ) { return false; }
		iWritten = snprintf(pOut, iOutCap,
			"CONNECT %s HTTP/1.1\r\n"
			"Host: %s\r\n"
			"Proxy-Authorization: Basic %s\r\n"
			"Proxy-Connection: Keep-Alive\r\n"
			"\r\n",
			sAuthority,
			sAuthority,
			sAuth);
		xrtFree(sAuth);
	} else {
		iWritten = snprintf(pOut, iOutCap,
			"CONNECT %s HTTP/1.1\r\n"
			"Host: %s\r\n"
			"Proxy-Connection: Keep-Alive\r\n"
			"\r\n",
			sAuthority,
			sAuthority);
	}

	// 验证写入结果并返回输出长度
	if ( iWritten <= 0 || (size_t)iWritten >= iOutCap ) { return false; }
	if ( pOutLen ) { *pOutLen = (size_t)iWritten; }
	return true;
}


// 内部函数：结束代理 find HTTP 头部
static uint32 __xnetProxyFindHttpHeaderEnd(const char* pData, uint32 iLen)
{
	if ( !pData || iLen < 4u ) { return 0u; }
	for ( uint32 i = 0; i + 3u < iLen; ++i ) {
		if ( pData[i] == '\r' && pData[i + 1u] == '\n' &&
			pData[i + 2u] == '\r' && pData[i + 3u] == '\n' ) {
			return i + 4u;
		}
	}
	return 0u;
}


// 内部函数：创建代理状态
static __xnet_proxy_state* __xnetProxyStateCreate(const xnetproxy* pProxy, const char* sTargetHost, uint16 iTargetPort)
{
	__xnet_proxy_state* pState;
	if ( !pProxy || !sTargetHost || !sTargetHost[0] || iTargetPort == 0 ) { return NULL; }
	pState = (__xnet_proxy_state*)XNET_ALLOC(sizeof(__xnet_proxy_state));
	if ( !pState ) { return NULL; }
	memset(pState, 0, sizeof(__xnet_proxy_state));
	pState->iType = pProxy->tConfig.iType;
	pState->iTargetPort = iTargetPort;
	__xnetCopyFixedString(pState->sTargetHost, sizeof(pState->sTargetHost), sTargetHost);
	memset(&pState->tTargetAddr, 0, sizeof(pState->tTargetAddr));
	if ( xrtNetAddrParse(&pState->tTargetAddr, pState->sTargetHost, iTargetPort) == XRT_NET_OK ) {
		pState->bTargetAddrValid = true;
	}
	return pState;
}


// 内部函数：销毁代理状态
static void __xnetProxyStateDestroy(__xnet_proxy_state* pState)
{
	if ( !pState ) { return; }
	if ( pState->pRecv ) { XNET_FREE(pState->pRecv); }
	XNET_FREE(pState);
}


// 内部函数：开始代理状态
static uint32 __xnetProxyStateBegin(__xnet_proxy_state* pState, const xnetproxy* pProxy, char* pOut, size_t iOutCap, size_t* pOutLen)
{
	if ( pOutLen ) { *pOutLen = 0; }
	if ( !pState || !pProxy || !pOut ) { return __XNET_PROXY_ACTION_ERROR; }
	if ( pState->iStage != __XNET_PROXY_STAGE_INIT ) { return __XNET_PROXY_ACTION_ERROR; }

	if ( pProxy->tConfig.iType == XNET_PROXY_SOCKS5 ) {
		if ( !__xnetProxyBuildSocks5Greeting(pProxy, pOut, iOutCap, pOutLen) ) { return __XNET_PROXY_ACTION_ERROR; }
		pState->iStage = __XNET_PROXY_STAGE_SOCKS5_METHOD;
		return __XNET_PROXY_ACTION_SEND;
	}
	if ( pProxy->tConfig.iType == XNET_PROXY_HTTP_CONNECT ) {
		if ( !__xnetProxyBuildHttpConnectReq(pProxy, pState, pOut, iOutCap, pOutLen) ) { return __XNET_PROXY_ACTION_ERROR; }
		pState->iStage = __XNET_PROXY_STAGE_HTTP_CONNECT;
		return __XNET_PROXY_ACTION_SEND;
	}
	return __XNET_PROXY_ACTION_ERROR;
}


// 内部函数：__xnetProxyStateFeed
static uint32 __xnetProxyStateFeed(__xnet_proxy_state* pState, const xnetproxy* pProxy, const void* pData, size_t iLen, char* pOut, size_t iOutCap, size_t* pOutLen)
{
	if ( pOutLen ) { *pOutLen = 0; }
	if ( !pState || !pProxy || !__xnetProxyAppendRecv(pState, pData, iLen) ) { return __XNET_PROXY_ACTION_ERROR; }

	// 代理协议状态机循环，根据当前阶段处理接收到的数据
	for ( ;; ) {
		switch ( pState->iStage ) {
			// SOCKS5 方法选择阶段：解析服务端支持的认证方法
			case __XNET_PROXY_STAGE_SOCKS5_METHOD:
				if ( pState->iRecvLen < 2u ) { return __XNET_PROXY_ACTION_WAIT; }
				if ( (uint8)pState->pRecv[0] != 0x05 ) { return __XNET_PROXY_ACTION_ERROR; }
				if ( (uint8)pState->pRecv[1] == 0xFF ) { return __XNET_PROXY_ACTION_ERROR; }
				if ( (uint8)pState->pRecv[1] == 0x02 ) {
					if ( !__xnetProxyHasAuth(pProxy) || !__xnetProxyBuildSocks5Auth(pProxy, pOut, iOutCap, pOutLen) ) {
						return __XNET_PROXY_ACTION_ERROR;
					}
					__xnetProxyConsumeRecv(pState, 2u);
					pState->iStage = __XNET_PROXY_STAGE_SOCKS5_AUTH;
					return __XNET_PROXY_ACTION_SEND;
				}
				if ( (uint8)pState->pRecv[1] != 0x00 ) { return __XNET_PROXY_ACTION_ERROR; }
				if ( !__xnetProxyBuildSocks5ConnectReq(pState, pOut, iOutCap, pOutLen) ) { return __XNET_PROXY_ACTION_ERROR; }
				__xnetProxyConsumeRecv(pState, 2u);
				pState->iStage = __XNET_PROXY_STAGE_SOCKS5_CONNECT;
				return __XNET_PROXY_ACTION_SEND;

			// SOCKS5 认证阶段：验证用户名密码认证结果
			case __XNET_PROXY_STAGE_SOCKS5_AUTH:
				if ( pState->iRecvLen < 2u ) { return __XNET_PROXY_ACTION_WAIT; }
				if ( (uint8)pState->pRecv[0] != 0x01 || (uint8)pState->pRecv[1] != 0x00 ) { return __XNET_PROXY_ACTION_ERROR; }
				if ( !__xnetProxyBuildSocks5ConnectReq(pState, pOut, iOutCap, pOutLen) ) { return __XNET_PROXY_ACTION_ERROR; }
				__xnetProxyConsumeRecv(pState, 2u);
				pState->iStage = __XNET_PROXY_STAGE_SOCKS5_CONNECT;
				return __XNET_PROXY_ACTION_SEND;

			// SOCKS5 连接阶段：解析服务端连接响应，根据地址类型确定报文长度
			case __XNET_PROXY_STAGE_SOCKS5_CONNECT:
				{
					uint32 iNeed = 0;
					uint8 iAtyp;
					if ( pState->iRecvLen < 5u ) { return __XNET_PROXY_ACTION_WAIT; }
					// 验证 SOCKS5 响应头：版本号、结果码、保留字段
					if ( (uint8)pState->pRecv[0] != 0x05 || (uint8)pState->pRecv[1] != 0x00 || (uint8)pState->pRecv[2] != 0x00 ) {
						return __XNET_PROXY_ACTION_ERROR;
					}
					// 根据地址类型计算完整响应所需长度
					iAtyp = (uint8)pState->pRecv[3];
					if ( iAtyp == 0x01 ) {
						iNeed = 4u + 4u + 2u;
					} else if ( iAtyp == 0x04 ) {
						iNeed = 4u + 16u + 2u;
					} else if ( iAtyp == 0x03 ) {
						iNeed = 5u + (uint8)pState->pRecv[4] + 2u;
					} else {
						return __XNET_PROXY_ACTION_ERROR;
					}
					if ( pState->iRecvLen < iNeed ) { return __XNET_PROXY_ACTION_WAIT; }
					__xnetProxyConsumeRecv(pState, iNeed);
					pState->iStage = __XNET_PROXY_STAGE_READY;
					return __XNET_PROXY_ACTION_READY;
				}

			// HTTP CONNECT 阶段：解析 HTTP 代理响应，提取状态码判断连接是否成功
			case __XNET_PROXY_STAGE_HTTP_CONNECT:
				{
					// 查找 HTTP 头部结束标记（\r\n\r\n）
					uint32 iHeaderEnd = __xnetProxyFindHttpHeaderEnd(pState->pRecv, pState->iRecvLen);
					char chSaved;
					char* pStatus;
					int iStatusCode;
					if ( iHeaderEnd == 0u ) { return __XNET_PROXY_ACTION_WAIT; }
					// 临时截断以字符串方式解析 HTTP 头部
					chSaved = pState->pRecv[iHeaderEnd - 1u];
					pState->pRecv[iHeaderEnd - 1u] = '\0';
					// 验证 HTTP 响应行以 "HTTP/1." 开头
					if ( strncmp(pState->pRecv, "HTTP/1.", 7) != 0 ) {
						pState->pRecv[iHeaderEnd - 1u] = chSaved;
						return __XNET_PROXY_ACTION_ERROR;
					}
					// 提取状态码（跳过 "HTTP/1.x " 后的第一个空格）
					pStatus = strchr(pState->pRecv, ' ');
					if ( !pStatus ) {
						pState->pRecv[iHeaderEnd - 1u] = chSaved;
						return __XNET_PROXY_ACTION_ERROR;
					}
					// 解析状态码并恢复缓冲区
					{
						char* pEnd = NULL;
						long iStatusCodeLong = strtol(pStatus + 1, &pEnd, 10);
						if ( pEnd == pStatus + 1 || iStatusCodeLong < 100 || iStatusCodeLong > 999 ) {
							pState->pRecv[iHeaderEnd - 1u] = chSaved;
							return __XNET_PROXY_ACTION_ERROR;
						}
						iStatusCode = (int)iStatusCodeLong;
					}
					pState->pRecv[iHeaderEnd - 1u] = chSaved;
					// 仅状态码 200 表示代理隧道建立成功
					if ( iStatusCode != 200 ) { return __XNET_PROXY_ACTION_ERROR; }
					__xnetProxyConsumeRecv(pState, iHeaderEnd);
					pState->iStage = __XNET_PROXY_STAGE_READY;
					return __XNET_PROXY_ACTION_READY;
				}

			case __XNET_PROXY_STAGE_READY:
				return __XNET_PROXY_ACTION_READY;

			default:
				return __XNET_PROXY_ACTION_ERROR;
		}
	}
}

#endif
