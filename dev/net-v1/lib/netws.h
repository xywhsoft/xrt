



/*
	NetWS - WebSocket 客户端/服务器封装 [Ver1.0]
	
	基于 nettcp.h (TCP) + nettls.h (TLS) + crypto.h (SHA-1, Base64)
	
	特性:
	  - 支持 WS/WSS 协议
	  - 客户端和服务器双模式
	  - 事件驱动架构
	  - 双模式 API: 简易版 (内部线程) + 高级版 (共享事件循环)
	  - 自动 Ping/Pong 心跳
	  - 分片消息重组
	
	仅 IPv4，跨平台 (Windows / Linux)
*/



/* ============================== 内部常量定义 ============================== */

#define __XRT_WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define __XRT_WS_MAX_HEADER_SIZE 4096
#define __XRT_WS_DEFAULT_MAX_MSG_SIZE (1024 * 1024)  // 1MB
#define __XRT_WS_DEFAULT_HANDSHAKE_TIMEOUT 10
#define __XRT_WS_KEY_LEN 16



/* ============================== WebSocket 帧结构 ============================== */

typedef struct {
	uint8 iOpcode;      // 操作码
	bool bFin;          // 是否最后一帧
	bool bMasked;       // 是否掩码
	uint8 aMask[4];     // 掩码键
	size_t iPayloadLen; // 载荷长度
	size_t iHeaderLen;  // 帧头长度
} __xrt_ws_frame;



/* ============================== WebSocket 客户端槽位结构 ============================== */

typedef struct {
	xnetconn* pConn;           // 底层连接
	int iClientId;             // 客户端 ID
	bool bInUse;               // 是否使用中
	bool bHandshakeDone;       // 握手是否完成
	xnetbuf tRecvBuf;          // 接收缓冲区
	xnetbuf tMsgBuf;           // 消息重组缓冲区
	uint8 iMsgOpcode;          // 消息操作码
	bool bFragmented;          // 是否正在接收分片消息
} __xrt_ws_client_slot;



/* ============================== WebSocket 客户端结构 ============================== */

struct xrt_ws_client {
	xtcpclient* pTcpClient;   // 底层 TCP 客户端
	
	xwsconfig tConfig;
	xwsevents tEvents;
	
	char sHost[256];          // 目标主机名
	uint16 iPort;             // 目标端口
	char sPath[512];          // 请求路径
	bool bSecure;             // 是否 WSS
	
	char sKey[32];            // Sec-WebSocket-Key (Base64)
	char sExpectedAccept[32]; // 期望的 Sec-WebSocket-Accept
	
	xnetbuf tRecvBuf;         // 接收缓冲区
	xnetbuf tMsgBuf;          // 消息重组缓冲区
	uint8 iMsgOpcode;         // 消息操作码
	bool bFragmented;         // 是否正在接收分片消息
	
	volatile bool bConnected;
	volatile bool bHandshakeDone;
	
	ptr pUserData;
};



/* ============================== WebSocket 服务器结构 ============================== */

struct xrt_ws_server {
	xtcpserver* pTcpServer;   // 底层 TCP 服务器
	
	xwsconfig tConfig;
	xwsevents tEvents;
	
	// 客户端槽位管理
	__xrt_ws_client_slot* arrSlots;
	int iMaxClients;
	int iClientCount;
	
	volatile bool bRunning;
	
	ptr pUserData;
};



/* ============================== 内部工具函数 ============================== */

// 生成随机 WebSocket Key
static void __xrt_ws_generate_key(char* sKey)
{
	uint8 aRandom[__XRT_WS_KEY_LEN];
	xrtRandomBytes(aRandom, __XRT_WS_KEY_LEN);
	str sEncoded = xrtBase64Encode(aRandom, __XRT_WS_KEY_LEN, NULL);
	if ( sEncoded && sEncoded != xCore.sNull ) {
		strcpy(sKey, sEncoded);
		xrtFree(sEncoded);
	}
}

// 计算 Sec-WebSocket-Accept
static void __xrt_ws_compute_accept(const char* sKey, char* sAccept)
{
	char sCombined[128];
	snprintf(sCombined, sizeof(sCombined), "%s%s", sKey, __XRT_WS_GUID);
	uint8 aSha1[20];
	xrtSHA1((uint8*)sCombined, strlen(sCombined), aSha1);
	str sEncoded = xrtBase64Encode(aSha1, 20, NULL);
	if ( sEncoded && sEncoded != xCore.sNull ) {
		strcpy(sAccept, sEncoded);
		xrtFree(sEncoded);
	}
}

// 掩码/解掩码数据
static void __xrt_ws_mask_data(uint8* pData, size_t iLen, const uint8* pMask)
{
	for ( size_t i = 0; i < iLen; i++ ) {
		pData[i] ^= pMask[i % 4];
	}
}

// 解析 WebSocket 帧头
// 返回: 0=需要更多数据, >0=帧头长度, -1=错误
static int __xrt_ws_parse_frame_header(const uint8* pData, size_t iLen, __xrt_ws_frame* pFrame)
{
	if ( iLen < 2 ) return 0;
	
	// 第一字节: FIN + RSV + Opcode
	pFrame->bFin = (pData[0] & 0x80) != 0;
	pFrame->iOpcode = pData[0] & 0x0F;
	
	// 第二字节: MASK + Payload Length
	pFrame->bMasked = (pData[1] & 0x80) != 0;
	uint8 iLen7 = pData[1] & 0x7F;
	
	size_t iHeaderLen = 2;
	
	if ( iLen7 == 126 ) {
		// 16-bit 长度
		if ( iLen < 4 ) return 0;
		pFrame->iPayloadLen = ((size_t)pData[2] << 8) | pData[3];
		iHeaderLen = 4;
	} else if ( iLen7 == 127 ) {
		// 64-bit 长度
		if ( iLen < 10 ) return 0;
		pFrame->iPayloadLen = 
			((size_t)pData[2] << 56) | ((size_t)pData[3] << 48) |
			((size_t)pData[4] << 40) | ((size_t)pData[5] << 32) |
			((size_t)pData[6] << 24) | ((size_t)pData[7] << 16) |
			((size_t)pData[8] << 8)  | pData[9];
		iHeaderLen = 10;
	} else {
		pFrame->iPayloadLen = iLen7;
	}
	
	// 掩码键
	if ( pFrame->bMasked ) {
		if ( iLen < iHeaderLen + 4 ) return 0;
		memcpy(pFrame->aMask, pData + iHeaderLen, 4);
		iHeaderLen += 4;
	}
	
	pFrame->iHeaderLen = iHeaderLen;
	return (int)iHeaderLen;
}

// 编码 WebSocket 帧
// 返回: 帧总长度
static size_t __xrt_ws_encode_frame(uint8* pOut, uint8 iOpcode, bool bFin, bool bMask,
	const uint8* pPayload, size_t iPayloadLen)
{
	size_t iIdx = 0;
	
	// 第一字节: FIN + Opcode
	pOut[iIdx++] = (bFin ? 0x80 : 0x00) | (iOpcode & 0x0F);
	
	// 第二字节 + 扩展长度
	uint8 iMaskBit = bMask ? 0x80 : 0x00;
	if ( iPayloadLen < 126 ) {
		pOut[iIdx++] = iMaskBit | (uint8)iPayloadLen;
	} else if ( iPayloadLen <= 0xFFFF ) {
		pOut[iIdx++] = iMaskBit | 126;
		pOut[iIdx++] = (uint8)(iPayloadLen >> 8);
		pOut[iIdx++] = (uint8)(iPayloadLen & 0xFF);
	} else {
		pOut[iIdx++] = iMaskBit | 127;
		pOut[iIdx++] = (uint8)(iPayloadLen >> 56);
		pOut[iIdx++] = (uint8)(iPayloadLen >> 48);
		pOut[iIdx++] = (uint8)(iPayloadLen >> 40);
		pOut[iIdx++] = (uint8)(iPayloadLen >> 32);
		pOut[iIdx++] = (uint8)(iPayloadLen >> 24);
		pOut[iIdx++] = (uint8)(iPayloadLen >> 16);
		pOut[iIdx++] = (uint8)(iPayloadLen >> 8);
		pOut[iIdx++] = (uint8)(iPayloadLen & 0xFF);
	}
	
	// 掩码键
	uint8 aMask[4] = {0};
	if ( bMask ) {
		xrtRandomBytes(aMask, 4);
		memcpy(pOut + iIdx, aMask, 4);
		iIdx += 4;
	}
	
	// 载荷数据
	if ( pPayload && iPayloadLen > 0 ) {
		memcpy(pOut + iIdx, pPayload, iPayloadLen);
		if ( bMask ) {
			__xrt_ws_mask_data(pOut + iIdx, iPayloadLen, aMask);
		}
		iIdx += iPayloadLen;
	}
	
	return iIdx;
}

// 计算编码帧需要的缓冲区大小
static size_t __xrt_ws_frame_size(size_t iPayloadLen, bool bMask)
{
	size_t iSize = 2;  // 基本头
	if ( iPayloadLen >= 126 && iPayloadLen <= 0xFFFF ) {
		iSize += 2;
	} else if ( iPayloadLen > 0xFFFF ) {
		iSize += 8;
	}
	if ( bMask ) {
		iSize += 4;
	}
	iSize += iPayloadLen;
	return iSize;
}



/* ============================== 内部 HTTP 头解析 ============================== */

// 从 HTTP 响应中提取指定头的值
static bool __xrt_ws_get_header_value(const char* sHeaders, const char* sName, char* sValue, size_t iMaxLen)
{
	if ( !sHeaders || !sName || !sValue ) return false;
	
	size_t iNameLen = strlen(sName);
	const char* p = sHeaders;
	
	while ( *p ) {
		// 跳过空白
		while ( *p == ' ' || *p == '\t' ) p++;
		
		// 检查是否匹配头名
		if ( strncasecmp(p, sName, iNameLen) == 0 && p[iNameLen] == ':' ) {
			p += iNameLen + 1;
			// 跳过冒号后的空白
			while ( *p == ' ' || *p == '\t' ) p++;
			// 提取值直到行尾
			size_t iIdx = 0;
			while ( *p && *p != '\r' && *p != '\n' && iIdx < iMaxLen - 1 ) {
				sValue[iIdx++] = *p++;
			}
			sValue[iIdx] = '\0';
			return true;
		}
		
		// 跳到下一行
		while ( *p && *p != '\n' ) p++;
		if ( *p == '\n' ) p++;
	}
	
	return false;
}

// 检查 HTTP 响应状态码
static int __xrt_ws_get_status_code(const char* sResponse)
{
	// HTTP/1.1 101 Switching Protocols
	if ( strncmp(sResponse, "HTTP/", 5) != 0 ) return -1;
	const char* p = sResponse + 5;
	// 跳过版本号
	while ( *p && *p != ' ' ) p++;
	if ( !*p ) return -1;
	// 跳过空格
	while ( *p == ' ' ) p++;
	// 解析状态码
	return atoi(p);
}



/* ============================== WebSocket 客户端 TCP 事件回调 ============================== */

// 前向声明
static void __xrt_ws_client_process_data(xwsclient* pClient, const char* pData, size_t iLen);

static void __xrt_ws_client_on_connect(ptr pOwner, xnetconn* pConn, bool bSuccess)
{
	xwsclient* pClient = (xwsclient*)xrtTcpClientGetUserData((xtcpclient*)pOwner);
	if ( !pClient ) return;
	
	if ( !bSuccess ) {
		pClient->bConnected = false;
		if ( pClient->tEvents.OnError ) {
			pClient->tEvents.OnError(pClient, pConn, -1);
		}
		return;
	}
	
	// TCP 连接成功，发送 WebSocket 握手请求
	char sRequest[1024];
	int iLen = snprintf(sRequest, sizeof(sRequest),
		"GET %s HTTP/1.1\r\n"
		"Host: %s:%d\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Key: %s\r\n"
		"Sec-WebSocket-Version: 13\r\n",
		pClient->sPath,
		pClient->sHost, pClient->iPort,
		pClient->sKey);
	
	// 可选头
	if ( pClient->tConfig.sProtocol && pClient->tConfig.sProtocol[0] ) {
		iLen += snprintf(sRequest + iLen, sizeof(sRequest) - iLen,
			"Sec-WebSocket-Protocol: %s\r\n", pClient->tConfig.sProtocol);
	}
	if ( pClient->tConfig.sOrigin && pClient->tConfig.sOrigin[0] ) {
		iLen += snprintf(sRequest + iLen, sizeof(sRequest) - iLen,
			"Origin: %s\r\n", pClient->tConfig.sOrigin);
	}
	
	// 结束头
	iLen += snprintf(sRequest + iLen, sizeof(sRequest) - iLen, "\r\n");
	
	// 计算期望的 Accept 值
	__xrt_ws_compute_accept(pClient->sKey, pClient->sExpectedAccept);
	
	// 发送握手请求
	xrtTcpClientSend(pClient->pTcpClient, sRequest, iLen);
}

static void __xrt_ws_client_on_recv(ptr pOwner, xnetconn* pConn, const char* pData, size_t iLen)
{
	xwsclient* pClient = (xwsclient*)xrtTcpClientGetUserData((xtcpclient*)pOwner);
	if ( !pClient ) return;
	
	__xrt_ws_client_process_data(pClient, pData, iLen);
}

static void __xrt_ws_client_on_close(ptr pOwner, xnetconn* pConn)
{
	xwsclient* pClient = (xwsclient*)xrtTcpClientGetUserData((xtcpclient*)pOwner);
	if ( !pClient ) return;
	
	pClient->bConnected = false;
	pClient->bHandshakeDone = false;
	
	if ( pClient->tEvents.OnClose ) {
		pClient->tEvents.OnClose(pClient, pConn, XRT_WS_CLOSE_ABNORMAL, "Connection closed");
	}
}

// 处理接收到的数据
static void __xrt_ws_client_process_data(xwsclient* pClient, const char* pData, size_t iLen)
{
	// 追加到接收缓冲区
	xrtNetBufAppend(&pClient->tRecvBuf, pData, iLen);
	
	if ( !pClient->bHandshakeDone ) {
		// 等待握手响应
		char* pBuf = (char*)pClient->tRecvBuf.pData;
		size_t iBufLen = pClient->tRecvBuf.iSize;
		
		// 查找 HTTP 响应结束标记
		char* pEnd = strstr(pBuf, "\r\n\r\n");
		if ( !pEnd ) return;  // 还没收到完整响应
		
		// 检查状态码
		int iStatus = __xrt_ws_get_status_code(pBuf);
		if ( iStatus != 101 ) {
			// 握手失败
			if ( pClient->tEvents.OnError ) {
				pClient->tEvents.OnError(pClient, NULL, iStatus);
			}
			xrtWsClientDisconnect(pClient);
			return;
		}
		
		// 验证 Sec-WebSocket-Accept
		char sAccept[64] = {0};
		if ( !__xrt_ws_get_header_value(pBuf, "Sec-WebSocket-Accept", sAccept, sizeof(sAccept)) ) {
			if ( pClient->tEvents.OnError ) {
				pClient->tEvents.OnError(pClient, NULL, -2);
			}
			xrtWsClientDisconnect(pClient);
			return;
		}
		
		if ( strcmp(sAccept, pClient->sExpectedAccept) != 0 ) {
			if ( pClient->tEvents.OnError ) {
				pClient->tEvents.OnError(pClient, NULL, -3);
			}
			xrtWsClientDisconnect(pClient);
			return;
		}
		
		// 握手成功
		pClient->bHandshakeDone = true;
		pClient->bConnected = true;
		
		// 消费 HTTP 响应
		size_t iHeaderLen = (size_t)(pEnd - pBuf) + 4;
		xrtNetBufConsume(&pClient->tRecvBuf, iHeaderLen);
		
		// 触发 OnOpen 回调
		if ( pClient->tEvents.OnOpen ) {
			pClient->tEvents.OnOpen(pClient, NULL);
		}
	}
	
	// 处理 WebSocket 帧
	while ( pClient->tRecvBuf.iSize > 0 ) {
		__xrt_ws_frame tFrame;
		int iHeaderLen = __xrt_ws_parse_frame_header(
			(uint8*)pClient->tRecvBuf.pData, pClient->tRecvBuf.iSize, &tFrame);
		
		if ( iHeaderLen <= 0 ) break;  // 需要更多数据
		
		size_t iFrameLen = tFrame.iHeaderLen + tFrame.iPayloadLen;
		if ( pClient->tRecvBuf.iSize < iFrameLen ) break;  // 需要更多数据
		
		// 获取载荷
		uint8* pPayload = (uint8*)pClient->tRecvBuf.pData + tFrame.iHeaderLen;
		
		// 解掩码 (服务器发送的数据通常不掩码)
		if ( tFrame.bMasked ) {
			__xrt_ws_mask_data(pPayload, tFrame.iPayloadLen, tFrame.aMask);
		}
		
		// 处理控制帧
		if ( tFrame.iOpcode >= 0x08 ) {
			switch ( tFrame.iOpcode ) {
				case XRT_WS_OP_CLOSE:
					{
						uint16 iCode = XRT_WS_CLOSE_NO_STATUS;
						const char* sReason = "";
						if ( tFrame.iPayloadLen >= 2 ) {
							iCode = ((uint16)pPayload[0] << 8) | pPayload[1];
							if ( tFrame.iPayloadLen > 2 ) {
								sReason = (const char*)(pPayload + 2);
							}
						}
						if ( pClient->tEvents.OnClose ) {
							pClient->tEvents.OnClose(pClient, NULL, iCode, sReason);
						}
						xrtNetBufConsume(&pClient->tRecvBuf, iFrameLen);
						xrtWsClientDisconnect(pClient);
						return;
					}
				case XRT_WS_OP_PING:
					// 自动回复 Pong
					xrtWsClientSendBinary(pClient, (char*)pPayload, tFrame.iPayloadLen);
					if ( pClient->tEvents.OnPing ) {
						pClient->tEvents.OnPing(pClient, NULL, (char*)pPayload, tFrame.iPayloadLen);
					}
					break;
				case XRT_WS_OP_PONG:
					if ( pClient->tEvents.OnPong ) {
						pClient->tEvents.OnPong(pClient, NULL, (char*)pPayload, tFrame.iPayloadLen);
					}
					break;
			}
		} else {
			// 数据帧
			if ( tFrame.iOpcode != XRT_WS_OP_CONTINUATION ) {
				// 新消息开始
				pClient->iMsgOpcode = tFrame.iOpcode;
				xrtNetBufClear(&pClient->tMsgBuf);
			}
			
			// 追加到消息缓冲区
			xrtNetBufAppend(&pClient->tMsgBuf, (char*)pPayload, tFrame.iPayloadLen);
			
			if ( tFrame.bFin ) {
				// 消息完成
				if ( pClient->tEvents.OnMessage ) {
					pClient->tEvents.OnMessage(pClient, NULL, pClient->iMsgOpcode,
						(char*)pClient->tMsgBuf.pData, pClient->tMsgBuf.iSize);
				}
				xrtNetBufClear(&pClient->tMsgBuf);
			} else {
				pClient->bFragmented = true;
			}
		}
		
		xrtNetBufConsume(&pClient->tRecvBuf, iFrameLen);
	}
}



/* ============================== WebSocket 客户端 API ============================== */

// 解析 WebSocket URL: ws://host:port/path 或 wss://host:port/path
static bool __xrt_ws_parse_url(const char* sURL, char* sHost, uint16* pPort, char* sPath, bool* pSecure)
{
	if ( !sURL ) return false;
	
	const char* p = sURL;
	
	// 解析协议
	if ( strncmp(p, "wss://", 6) == 0 ) {
		*pSecure = true;
		*pPort = 443;
		p += 6;
	} else if ( strncmp(p, "ws://", 5) == 0 ) {
		*pSecure = false;
		*pPort = 80;
		p += 5;
	} else {
		return false;
	}
	
	// 解析主机名和端口
	const char* pHostStart = p;
	const char* pPortStart = NULL;
	
	while ( *p && *p != '/' && *p != '?' ) {
		if ( *p == ':' ) {
			pPortStart = p + 1;
		}
		p++;
	}
	
	if ( pPortStart ) {
		size_t iHostLen = (size_t)(pPortStart - 1 - pHostStart);
		if ( iHostLen >= 256 ) iHostLen = 255;
		memcpy(sHost, pHostStart, iHostLen);
		sHost[iHostLen] = '\0';
		*pPort = (uint16)atoi(pPortStart);
	} else {
		size_t iHostLen = (size_t)(p - pHostStart);
		if ( iHostLen >= 256 ) iHostLen = 255;
		memcpy(sHost, pHostStart, iHostLen);
		sHost[iHostLen] = '\0';
	}
	
	// 解析路径
	if ( *p == '/' || *p == '?' ) {
		size_t iPathLen = strlen(p);
		if ( iPathLen >= 512 ) iPathLen = 511;
		memcpy(sPath, p, iPathLen);
		sPath[iPathLen] = '\0';
	} else {
		strcpy(sPath, "/");
	}
	
	return (sHost[0] != '\0');
}

// 内部初始化
static xwsclient* __xrt_ws_client_init(xeventloop* pLoop, const char* sURL,
	const xwsconfig* pConfig, const xwsevents* pEvents)
{
	xwsclient* pClient = (xwsclient*)xrtCalloc(1, sizeof(xwsclient));
	if ( !pClient ) return NULL;
	
	// 解析 URL
	if ( !__xrt_ws_parse_url(sURL, pClient->sHost, &pClient->iPort, pClient->sPath, &pClient->bSecure) ) {
		xrtFree(pClient);
		return NULL;
	}
	
	// 复制配置
	if ( pConfig ) {
		pClient->tConfig = *pConfig;
	}
	if ( !pClient->tConfig.sPath || !pClient->tConfig.sPath[0] ) {
		pClient->tConfig.sPath = pClient->sPath;
	}
	if ( pClient->tConfig.iMaxMessageSize <= 0 ) {
		pClient->tConfig.iMaxMessageSize = __XRT_WS_DEFAULT_MAX_MSG_SIZE;
	}
	if ( pClient->tConfig.iHandshakeTimeoutSec <= 0 ) {
		pClient->tConfig.iHandshakeTimeoutSec = __XRT_WS_DEFAULT_HANDSHAKE_TIMEOUT;
	}
	
	// 复制事件
	if ( pEvents ) {
		pClient->tEvents = *pEvents;
	}
	
	// 生成 WebSocket Key
	__xrt_ws_generate_key(pClient->sKey);
	
	// 创建底层 TCP 客户端
	xnetevents tTcpEvents = {0};
	tTcpEvents.OnConnect = __xrt_ws_client_on_connect;
	tTcpEvents.OnRecv = __xrt_ws_client_on_recv;
	tTcpEvents.OnClose = __xrt_ws_client_on_close;
	
	if ( pLoop ) {
		pClient->pTcpClient = xrtTcpClientCreateEx(pLoop, pClient->sHost, pClient->iPort, NULL, &tTcpEvents);
	} else {
		pClient->pTcpClient = xrtTcpClientCreate(pClient->sHost, pClient->iPort, NULL, &tTcpEvents);
	}
	
	if ( !pClient->pTcpClient ) {
		xrtFree(pClient);
		return NULL;
	}
	
	xrtTcpClientSetUserData(pClient->pTcpClient, pClient);
	
	// 启用 TLS (WSS)
	if ( pClient->bSecure ) {
		xtlsconfig tTlsConfig = {0};
		tTlsConfig.sHostName = pClient->sHost;
		tTlsConfig.bVerifyPeer = false;
		xrtTcpClientEnableTLS(pClient->pTcpClient, &tTlsConfig);
	}
	
	return pClient;
}

XXAPI xwsclient* xrtWsClientCreate(const char* sURL, const xwsconfig* pConfig, const xwsevents* pEvents)
{
	return __xrt_ws_client_init(NULL, sURL, pConfig, pEvents);
}

XXAPI xwsclient* xrtWsClientCreateEx(xeventloop* pLoop, const char* sURL,
	const xwsconfig* pConfig, const xwsevents* pEvents)
{
	return __xrt_ws_client_init(pLoop, sURL, pConfig, pEvents);
}

XXAPI void xrtWsClientDestroy(xwsclient* pClient)
{
	if ( !pClient ) return;
	
	if ( pClient->pTcpClient ) {
		xrtTcpClientDestroy(pClient->pTcpClient);
	}
	
	xrtNetBufFree(&pClient->tRecvBuf);
	xrtNetBufFree(&pClient->tMsgBuf);
	
	xrtFree(pClient);
}

XXAPI xnet_result xrtWsClientConnect(xwsclient* pClient)
{
	if ( !pClient || !pClient->pTcpClient ) return XRT_NET_ERROR;
	return xrtTcpClientConnect(pClient->pTcpClient);
}

XXAPI void xrtWsClientDisconnect(xwsclient* pClient)
{
	if ( !pClient ) return;
	
	pClient->bConnected = false;
	pClient->bHandshakeDone = false;
	
	if ( pClient->pTcpClient ) {
		xrtTcpClientDisconnect(pClient->pTcpClient);
	}
}

// 发送帧 (内部)
static xnet_result __xrt_ws_client_send_frame(xwsclient* pClient, uint8 iOpcode,
	const char* pData, size_t iLen)
{
	if ( !pClient || !pClient->bHandshakeDone ) return XRT_NET_ERROR;
	
	// 计算帧大小 (客户端必须掩码)
	size_t iFrameSize = __xrt_ws_frame_size(iLen, true);
	uint8* pFrame = (uint8*)xrtMalloc(iFrameSize);
	if ( !pFrame ) return XRT_NET_ERROR;
	
	// 编码帧
	size_t iActualSize = __xrt_ws_encode_frame(pFrame, iOpcode, true, true, (uint8*)pData, iLen);
	
	// 发送
	xnet_result iRes = xrtTcpClientSend(pClient->pTcpClient, (char*)pFrame, iActualSize);
	
	xrtFree(pFrame);
	return iRes;
}

XXAPI xnet_result xrtWsClientSendText(xwsclient* pClient, const char* sText, size_t iLen)
{
	return __xrt_ws_client_send_frame(pClient, XRT_WS_OP_TEXT, sText, iLen);
}

XXAPI xnet_result xrtWsClientSendBinary(xwsclient* pClient, const char* pData, size_t iLen)
{
	return __xrt_ws_client_send_frame(pClient, XRT_WS_OP_BINARY, pData, iLen);
}

XXAPI xnet_result xrtWsClientPing(xwsclient* pClient, const char* pData, size_t iLen)
{
	return __xrt_ws_client_send_frame(pClient, XRT_WS_OP_PING, pData, iLen);
}

XXAPI xnet_result xrtWsClientClose(xwsclient* pClient, uint16 iCode, const char* sReason)
{
	if ( !pClient || !pClient->bHandshakeDone ) return XRT_NET_ERROR;
	
	// 构造 Close 帧载荷
	size_t iReasonLen = sReason ? strlen(sReason) : 0;
	size_t iPayloadLen = 2 + iReasonLen;
	uint8* pPayload = (uint8*)xrtMalloc(iPayloadLen);
	if ( !pPayload ) return XRT_NET_ERROR;
	
	pPayload[0] = (uint8)(iCode >> 8);
	pPayload[1] = (uint8)(iCode & 0xFF);
	if ( iReasonLen > 0 ) {
		memcpy(pPayload + 2, sReason, iReasonLen);
	}
	
	xnet_result iRes = __xrt_ws_client_send_frame(pClient, XRT_WS_OP_CLOSE, (char*)pPayload, iPayloadLen);
	
	xrtFree(pPayload);
	return iRes;
}

XXAPI bool xrtWsClientIsConnected(xwsclient* pClient)
{
	return pClient && pClient->bConnected && pClient->bHandshakeDone;
}

XXAPI void xrtWsClientSetUserData(xwsclient* pClient, ptr pData)
{
	if ( pClient ) pClient->pUserData = pData;
}

XXAPI ptr xrtWsClientGetUserData(xwsclient* pClient)
{
	return pClient ? pClient->pUserData : NULL;
}



/* ============================== WebSocket 服务器 TCP 事件回调 ============================== */

// 前向声明
static void __xrt_ws_server_process_client_data(xwsserver* pServer, int iClientId,
	const char* pData, size_t iLen);

static void __xrt_ws_server_on_accept(ptr pOwner, xnetconn* pConn)
{
	xwsserver* pServer = (xwsserver*)xrtTcpServerGetUserData((xtcpserver*)pOwner);
	if ( !pServer ) return;
	
	// 查找空闲槽位
	int iSlot = -1;
	for ( int i = 0; i < pServer->iMaxClients; i++ ) {
		if ( !pServer->arrSlots[i].bInUse ) {
			iSlot = i;
			break;
		}
	}
	
	if ( iSlot < 0 ) {
		// 无空闲槽位，拒绝连接
		return;
	}
	
	__xrt_ws_client_slot* pSlot = &pServer->arrSlots[iSlot];
	memset(pSlot, 0, sizeof(__xrt_ws_client_slot));
	pSlot->bInUse = true;
	pSlot->iClientId = pConn->iId;
	pSlot->pConn = pConn;
	pServer->iClientCount++;
}

static void __xrt_ws_server_on_recv(ptr pOwner, xnetconn* pConn, const char* pData, size_t iLen)
{
	xwsserver* pServer = (xwsserver*)xrtTcpServerGetUserData((xtcpserver*)pOwner);
	if ( !pServer ) return;
	
	// 查找客户端槽位
	int iClientId = pConn->iId;
	if ( iClientId < 0 || iClientId >= pServer->iMaxClients ) return;
	
	__xrt_ws_server_process_client_data(pServer, iClientId, pData, iLen);
}

static void __xrt_ws_server_on_close(ptr pOwner, xnetconn* pConn)
{
	xwsserver* pServer = (xwsserver*)xrtTcpServerGetUserData((xtcpserver*)pOwner);
	if ( !pServer ) return;
	
	int iClientId = pConn->iId;
	if ( iClientId < 0 || iClientId >= pServer->iMaxClients ) return;
	
	__xrt_ws_client_slot* pSlot = &pServer->arrSlots[iClientId];
	if ( !pSlot->bInUse ) return;
	
	if ( pSlot->bHandshakeDone && pServer->tEvents.OnClose ) {
		pServer->tEvents.OnClose(pServer, pConn, XRT_WS_CLOSE_ABNORMAL, "Connection closed");
	}
	
	// 清理槽位
	xrtNetBufFree(&pSlot->tRecvBuf);
	xrtNetBufFree(&pSlot->tMsgBuf);
	pSlot->bInUse = false;
	pServer->iClientCount--;
}

// 处理客户端数据
static void __xrt_ws_server_process_client_data(xwsserver* pServer, int iClientId,
	const char* pData, size_t iLen)
{
	__xrt_ws_client_slot* pSlot = &pServer->arrSlots[iClientId];
	if ( !pSlot->bInUse ) return;
	
	// 追加到接收缓冲区
	xrtNetBufAppend(&pSlot->tRecvBuf, pData, iLen);
	
	if ( !pSlot->bHandshakeDone ) {
		// 处理 WebSocket 握手请求
		char* pBuf = (char*)pSlot->tRecvBuf.pData;
		size_t iBufLen = pSlot->tRecvBuf.iSize;
		
		// 查找 HTTP 请求结束标记
		char* pEnd = strstr(pBuf, "\r\n\r\n");
		if ( !pEnd ) return;
		
		// 验证是 WebSocket 升级请求
		char sUpgrade[32] = {0};
		char sConnection[64] = {0};
		char sKey[64] = {0};
		
		__xrt_ws_get_header_value(pBuf, "Upgrade", sUpgrade, sizeof(sUpgrade));
		__xrt_ws_get_header_value(pBuf, "Connection", sConnection, sizeof(sConnection));
		__xrt_ws_get_header_value(pBuf, "Sec-WebSocket-Key", sKey, sizeof(sKey));
		
		if ( strcasecmp(sUpgrade, "websocket") != 0 || !strstr(sConnection, "Upgrade") || !sKey[0] ) {
			// 无效请求
			xrtTcpServerDisconnect(pServer->pTcpServer, iClientId);
			return;
		}
		
		// 计算 Accept 密钥
		char sAccept[64];
		__xrt_ws_compute_accept(sKey, sAccept);
		
		// 发送握手响应
		char sResponse[512];
		int iRespLen = snprintf(sResponse, sizeof(sResponse),
			"HTTP/1.1 101 Switching Protocols\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Accept: %s\r\n"
			"\r\n",
			sAccept);
		
		xrtTcpServerSend(pServer->pTcpServer, iClientId, sResponse, iRespLen);
		
		// 握手完成
		pSlot->bHandshakeDone = true;
		
		// 消费 HTTP 请求
		size_t iHeaderLen = (size_t)(pEnd - pBuf) + 4;
		xrtNetBufConsume(&pSlot->tRecvBuf, iHeaderLen);
		
		// 触发 OnOpen 回调
		if ( pServer->tEvents.OnOpen ) {
			pServer->tEvents.OnOpen(pServer, pSlot->pConn);
		}
	}
	
	// 处理 WebSocket 帧
	while ( pSlot->tRecvBuf.iSize > 0 ) {
		__xrt_ws_frame tFrame;
		int iHeaderLen = __xrt_ws_parse_frame_header(
			(uint8*)pSlot->tRecvBuf.pData, pSlot->tRecvBuf.iSize, &tFrame);
		
		if ( iHeaderLen <= 0 ) break;
		
		size_t iFrameLen = tFrame.iHeaderLen + tFrame.iPayloadLen;
		if ( pSlot->tRecvBuf.iSize < iFrameLen ) break;
		
		// 获取载荷
		uint8* pPayload = (uint8*)pSlot->tRecvBuf.pData + tFrame.iHeaderLen;
		
		// 解掩码 (客户端发送的数据必须掩码)
		if ( tFrame.bMasked ) {
			__xrt_ws_mask_data(pPayload, tFrame.iPayloadLen, tFrame.aMask);
		}
		
		// 处理控制帧
		if ( tFrame.iOpcode >= 0x08 ) {
			switch ( tFrame.iOpcode ) {
				case XRT_WS_OP_CLOSE:
					{
						uint16 iCode = XRT_WS_CLOSE_NO_STATUS;
						const char* sReason = "";
						if ( tFrame.iPayloadLen >= 2 ) {
							iCode = ((uint16)pPayload[0] << 8) | pPayload[1];
							if ( tFrame.iPayloadLen > 2 ) {
								sReason = (const char*)(pPayload + 2);
							}
						}
						if ( pServer->tEvents.OnClose ) {
							pServer->tEvents.OnClose(pServer, pSlot->pConn, iCode, sReason);
						}
						// 发送 Close 响应
						xrtWsServerDisconnect(pServer, iClientId, iCode, sReason);
						return;
					}
				case XRT_WS_OP_PING:
					// 自动回复 Pong (服务器不掩码)
					{
						size_t iPongSize = __xrt_ws_frame_size(tFrame.iPayloadLen, false);
						uint8* pPong = (uint8*)xrtMalloc(iPongSize);
						if ( pPong ) {
							size_t iActual = __xrt_ws_encode_frame(pPong, XRT_WS_OP_PONG, true, false,
								pPayload, tFrame.iPayloadLen);
							xrtTcpServerSend(pServer->pTcpServer, iClientId, (char*)pPong, iActual);
							xrtFree(pPong);
						}
					}
					if ( pServer->tEvents.OnPing ) {
						pServer->tEvents.OnPing(pServer, pSlot->pConn, (char*)pPayload, tFrame.iPayloadLen);
					}
					break;
				case XRT_WS_OP_PONG:
					if ( pServer->tEvents.OnPong ) {
						pServer->tEvents.OnPong(pServer, pSlot->pConn, (char*)pPayload, tFrame.iPayloadLen);
					}
					break;
			}
		} else {
			// 数据帧
			if ( tFrame.iOpcode != XRT_WS_OP_CONTINUATION ) {
				pSlot->iMsgOpcode = tFrame.iOpcode;
				xrtNetBufClear(&pSlot->tMsgBuf);
			}
			
			xrtNetBufAppend(&pSlot->tMsgBuf, (char*)pPayload, tFrame.iPayloadLen);
			
			if ( tFrame.bFin ) {
				if ( pServer->tEvents.OnMessage ) {
					pServer->tEvents.OnMessage(pServer, pSlot->pConn, pSlot->iMsgOpcode,
						(char*)pSlot->tMsgBuf.pData, pSlot->tMsgBuf.iSize);
				}
				xrtNetBufClear(&pSlot->tMsgBuf);
				pSlot->bFragmented = false;
			} else {
				pSlot->bFragmented = true;
			}
		}
		
		xrtNetBufConsume(&pSlot->tRecvBuf, iFrameLen);
	}
}



/* ============================== WebSocket 服务器 API ============================== */

static xwsserver* __xrt_ws_server_init(xeventloop* pLoop, const char* sIP, uint16 iPort,
	const xwsconfig* pConfig, const xwsevents* pEvents)
{
	xwsserver* pServer = (xwsserver*)xrtCalloc(1, sizeof(xwsserver));
	if ( !pServer ) return NULL;
	
	// 复制配置
	if ( pConfig ) {
		pServer->tConfig = *pConfig;
	}
	if ( pServer->tConfig.iMaxMessageSize <= 0 ) {
		pServer->tConfig.iMaxMessageSize = __XRT_WS_DEFAULT_MAX_MSG_SIZE;
	}
	
	// 复制事件
	if ( pEvents ) {
		pServer->tEvents = *pEvents;
	}
	
	// 创建底层 TCP 服务器
	xnetevents tTcpEvents = {0};
	tTcpEvents.OnAccept = __xrt_ws_server_on_accept;
	tTcpEvents.OnRecv = __xrt_ws_server_on_recv;
	tTcpEvents.OnClose = __xrt_ws_server_on_close;
	
	xnetconfig tNetConfig = {0};
	tNetConfig.iRecvBufSize = 8192;
	tNetConfig.iSendBufSize = 8192;
	tNetConfig.iMaxClients = 256;
	
	if ( pLoop ) {
		pServer->pTcpServer = xrtTcpServerCreateEx(pLoop, sIP, iPort, &tNetConfig, &tTcpEvents);
	} else {
		pServer->pTcpServer = xrtTcpServerCreate(sIP, iPort, &tNetConfig, &tTcpEvents);
	}
	
	if ( !pServer->pTcpServer ) {
		xrtFree(pServer);
		return NULL;
	}
	
	xrtTcpServerSetUserData(pServer->pTcpServer, pServer);
	
	// 分配客户端槽位
	pServer->iMaxClients = tNetConfig.iMaxClients;
	pServer->arrSlots = (__xrt_ws_client_slot*)xrtCalloc(pServer->iMaxClients, sizeof(__xrt_ws_client_slot));
	if ( !pServer->arrSlots ) {
		xrtTcpServerDestroy(pServer->pTcpServer);
		xrtFree(pServer);
		return NULL;
	}
	
	return pServer;
}

XXAPI xwsserver* xrtWsServerCreate(const char* sIP, uint16 iPort,
	const xwsconfig* pConfig, const xwsevents* pEvents)
{
	return __xrt_ws_server_init(NULL, sIP, iPort, pConfig, pEvents);
}

XXAPI xwsserver* xrtWsServerCreateEx(xeventloop* pLoop, const char* sIP, uint16 iPort,
	const xwsconfig* pConfig, const xwsevents* pEvents)
{
	return __xrt_ws_server_init(pLoop, sIP, iPort, pConfig, pEvents);
}

XXAPI void xrtWsServerDestroy(xwsserver* pServer)
{
	if ( !pServer ) return;
	
	// 清理所有槽位
	if ( pServer->arrSlots ) {
		for ( int i = 0; i < pServer->iMaxClients; i++ ) {
			if ( pServer->arrSlots[i].bInUse ) {
				xrtNetBufFree(&pServer->arrSlots[i].tRecvBuf);
				xrtNetBufFree(&pServer->arrSlots[i].tMsgBuf);
			}
		}
		xrtFree(pServer->arrSlots);
	}
	
	if ( pServer->pTcpServer ) {
		xrtTcpServerDestroy(pServer->pTcpServer);
	}
	
	xrtFree(pServer);
}

XXAPI xnet_result xrtWsServerStart(xwsserver* pServer)
{
	if ( !pServer || !pServer->pTcpServer ) return XRT_NET_ERROR;
	pServer->bRunning = true;
	return xrtTcpServerStart(pServer->pTcpServer);
}

XXAPI void xrtWsServerStop(xwsserver* pServer)
{
	if ( !pServer ) return;
	pServer->bRunning = false;
	if ( pServer->pTcpServer ) {
		xrtTcpServerStop(pServer->pTcpServer);
	}
}

XXAPI xnet_result xrtWsServerEnableTLS(xwsserver* pServer, const xtlsconfig* pTlsConfig)
{
	if ( !pServer || !pServer->pTcpServer ) return XRT_NET_ERROR;
	return xrtTcpServerEnableTLS(pServer->pTcpServer, pTlsConfig);
}

// 发送帧 (内部)
static xnet_result __xrt_ws_server_send_frame(xwsserver* pServer, int iClientId,
	uint8 iOpcode, const char* pData, size_t iLen)
{
	if ( !pServer || iClientId < 0 || iClientId >= pServer->iMaxClients ) return XRT_NET_ERROR;
	
	__xrt_ws_client_slot* pSlot = &pServer->arrSlots[iClientId];
	if ( !pSlot->bInUse || !pSlot->bHandshakeDone ) return XRT_NET_ERROR;
	
	// 服务器不掩码
	size_t iFrameSize = __xrt_ws_frame_size(iLen, false);
	uint8* pFrame = (uint8*)xrtMalloc(iFrameSize);
	if ( !pFrame ) return XRT_NET_ERROR;
	
	size_t iActualSize = __xrt_ws_encode_frame(pFrame, iOpcode, true, false, (uint8*)pData, iLen);
	
	xnet_result iRes = xrtTcpServerSend(pServer->pTcpServer, iClientId, (char*)pFrame, iActualSize);
	
	xrtFree(pFrame);
	return iRes;
}

XXAPI xnet_result xrtWsServerSendText(xwsserver* pServer, int iClientId, const char* sText, size_t iLen)
{
	return __xrt_ws_server_send_frame(pServer, iClientId, XRT_WS_OP_TEXT, sText, iLen);
}

XXAPI xnet_result xrtWsServerSendBinary(xwsserver* pServer, int iClientId, const char* pData, size_t iLen)
{
	return __xrt_ws_server_send_frame(pServer, iClientId, XRT_WS_OP_BINARY, pData, iLen);
}

XXAPI xnet_result xrtWsServerPing(xwsserver* pServer, int iClientId, const char* pData, size_t iLen)
{
	return __xrt_ws_server_send_frame(pServer, iClientId, XRT_WS_OP_PING, pData, iLen);
}

XXAPI xnet_result xrtWsServerBroadcastText(xwsserver* pServer, const char* sText, size_t iLen)
{
	if ( !pServer ) return XRT_NET_ERROR;
	
	for ( int i = 0; i < pServer->iMaxClients; i++ ) {
		if ( pServer->arrSlots[i].bInUse && pServer->arrSlots[i].bHandshakeDone ) {
			xrtWsServerSendText(pServer, i, sText, iLen);
		}
	}
	return XRT_NET_OK;
}

XXAPI xnet_result xrtWsServerBroadcastBinary(xwsserver* pServer, const char* pData, size_t iLen)
{
	if ( !pServer ) return XRT_NET_ERROR;
	
	for ( int i = 0; i < pServer->iMaxClients; i++ ) {
		if ( pServer->arrSlots[i].bInUse && pServer->arrSlots[i].bHandshakeDone ) {
			xrtWsServerSendBinary(pServer, i, pData, iLen);
		}
	}
	return XRT_NET_OK;
}

XXAPI void xrtWsServerDisconnect(xwsserver* pServer, int iClientId, uint16 iCode, const char* sReason)
{
	if ( !pServer || iClientId < 0 || iClientId >= pServer->iMaxClients ) return;
	
	__xrt_ws_client_slot* pSlot = &pServer->arrSlots[iClientId];
	if ( !pSlot->bInUse ) return;
	
	if ( pSlot->bHandshakeDone ) {
		// 发送 Close 帧
		size_t iReasonLen = sReason ? strlen(sReason) : 0;
		size_t iPayloadLen = 2 + iReasonLen;
		uint8* pPayload = (uint8*)xrtMalloc(iPayloadLen);
		if ( pPayload ) {
			pPayload[0] = (uint8)(iCode >> 8);
			pPayload[1] = (uint8)(iCode & 0xFF);
			if ( iReasonLen > 0 ) {
				memcpy(pPayload + 2, sReason, iReasonLen);
			}
			__xrt_ws_server_send_frame(pServer, iClientId, XRT_WS_OP_CLOSE, (char*)pPayload, iPayloadLen);
			xrtFree(pPayload);
		}
	}
	
	// 断开底层 TCP 连接
	xrtTcpServerDisconnect(pServer->pTcpServer, iClientId);
}

XXAPI int xrtWsServerGetClientCount(xwsserver* pServer)
{
	return pServer ? pServer->iClientCount : 0;
}

XXAPI void xrtWsServerSetUserData(xwsserver* pServer, ptr pData)
{
	if ( pServer ) pServer->pUserData = pData;
}

XXAPI ptr xrtWsServerGetUserData(xwsserver* pServer)
{
	return pServer ? pServer->pUserData : NULL;
}


