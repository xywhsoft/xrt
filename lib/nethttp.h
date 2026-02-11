



/*
	NetHTTP - HTTP Client 封装 [Ver1.0]
	
	基于 netsock.h (Socket 基础) + nettls.h (TLS) + buffer.h (自增缓冲区)
	同步阻塞模型，支持 HTTP/1.1, HTTPS, 30x 重定向, Chunked 传输
	
	极简 API: xrtHttpGet / xrtHttpPost / xrtHttpGetFile / xrtHttpPostFile
	完整 API: xrtHttpReq* / xrtHttpResp* 系列
	
	仅 IPv4，跨平台 (Windows / Linux)
*/



/* ============================== URL 解析器 (公开 API) ============================== */



/* ============================== 内部线程安全的临时缓冲区 ============================== */

// 用于 xrtHttpRespHeader / xrtHttpRespCookie 返回值的线程局部缓冲区
static char __xrt_http_tmpbuf[4096];



XXAPI bool xrtUrlParse(str sURL, xurl pOut)
{
	if ( !sURL || !pOut ) return false;
	memset(pOut, 0, sizeof(xurl_struct));
	
	str p = sURL;
	
	// 解析协议
	if ( strncmp(p, "https://", 8) == 0 ) {
		pOut->bHttps = true;
		pOut->iPort = 443;
		p += 8;
	} else if ( strncmp(p, "http://", 7) == 0 ) {
		pOut->bHttps = false;
		pOut->iPort = 80;
		p += 7;
	} else {
		return false;
	}
	
	// 解析主机名和端口
	str pHostStart = p;
	str pHostEnd = NULL;
	str pPortStart = NULL;
	
	while ( *p && *p != '/' && *p != '?' ) {
		if ( *p == ':' ) {
			pPortStart = p + 1;
		}
		p++;
	}
	
	if ( pPortStart ) {
		// 有端口号
		size_t iHostLen = (size_t)(pPortStart - 1 - pHostStart);
		if ( iHostLen >= sizeof(pOut->sHost) ) iHostLen = sizeof(pOut->sHost) - 1;
		memcpy(pOut->sHost, pHostStart, iHostLen);
		pOut->sHost[iHostLen] = '\0';
		pOut->iPort = (uint16)atoi(pPortStart);
	} else {
		// 无端口号
		size_t iHostLen = (size_t)(p - pHostStart);
		if ( iHostLen >= sizeof(pOut->sHost) ) iHostLen = sizeof(pOut->sHost) - 1;
		memcpy(pOut->sHost, pHostStart, iHostLen);
		pOut->sHost[iHostLen] = '\0';
	}
	
	// 解析路径
	if ( *p == '/' || *p == '?' ) {
		size_t iPathLen = strlen(p);
		if ( iPathLen >= sizeof(pOut->sPath) ) iPathLen = sizeof(pOut->sPath) - 1;
		memcpy(pOut->sPath, p, iPathLen);
		pOut->sPath[iPathLen] = '\0';
	} else {
		pOut->sPath[0] = '/';
		pOut->sPath[1] = '\0';
	}
	
	return (pOut->sHost[0] != '\0');
}



// 内部辅助: URL 编码并追加到 xbuffer
static void __xrt_http_url_encode_append(str sSrc, xbuffer pBuf)
{
	str sEncoded = xrtUrlEncode(sSrc, 0);
	if ( sEncoded ) {
		xrtBufferAppend(pBuf, sEncoded, strlen(sEncoded), 0);
		xrtFree(sEncoded);
	}
}



/* ============================== 底层同步连接 ============================== */

// 内部连接上下文
typedef struct {
	xnetconn tConn;
	xtlsctx* pTlsCtx;
	bool bHttps;
	bool bConnected;
} __xrt_http_conn;

static bool __xrt_http_connect(__xrt_http_conn* pConn, const xurl pURL, int iTimeoutSec)
{
	memset(pConn, 0, sizeof(__xrt_http_conn));
	pConn->bHttps = pURL->bHttps;
	
	// DNS 解析
	xnetaddr tAddr;
	if ( xrtNetResolve(pURL->sHost, &tAddr) != XRT_NET_OK ) {
		#ifdef DEBUG_TRACE
			printf("    [HTTP] DNS resolve failed: %s\n", pURL->sHost);
		#endif
		return false;
	}
	tAddr.iPort = pURL->iPort;
	
	// 创建 Socket
	memset(&pConn->tConn, 0, sizeof(xnetconn));
	if ( xrtSockCreate(&pConn->tConn, 0) != XRT_NET_OK ) {
		#ifdef DEBUG_TRACE
			printf("    [HTTP] Socket create failed\n");
		#endif
		return false;
	}
	
	// 连接
	xnet_result iRes = xrtSockConnect(&pConn->tConn, &tAddr);
	if ( iRes != XRT_NET_OK && iRes != XRT_NET_AGAIN ) {
		xrtSockClose(&pConn->tConn);
		return false;
	}
	
	// AGAIN: 使用 select 等待
	if ( iRes == XRT_NET_AGAIN ) {
		fd_set tWriteSet;
		FD_ZERO(&tWriteSet);
		FD_SET(pConn->tConn.hSocket, &tWriteSet);
		struct timeval tTimeout;
		tTimeout.tv_sec = iTimeoutSec > 0 ? iTimeoutSec : 10;
		tTimeout.tv_usec = 0;
		int iSelRes = select((int)pConn->tConn.hSocket + 1, NULL, &tWriteSet, NULL, &tTimeout);
		if ( iSelRes <= 0 ) {
			xrtSockClose(&pConn->tConn);
			return false;
		}
	}
	
	// TLS 握手
	if ( pURL->bHttps ) {
		xrtSockSetNonBlock(&pConn->tConn);
		
		xtlsconfig tCfg;
		memset(&tCfg, 0, sizeof(xtlsconfig));
		tCfg.sHostName = pURL->sHost;
		tCfg.bVerifyPeer = false;  // 暂不验证证书
		
		pConn->pTlsCtx = xrtTlsCreate(&tCfg, false);
		if ( !pConn->pTlsCtx ) {
			xrtSockClose(&pConn->tConn);
			return false;
		}
		
		// TLS 握手循环 (select 等待 IO 就绪)
		xnet_result iTlsRes;
		int iRetries = 0;
		int iMaxRetries = (iTimeoutSec > 0 ? iTimeoutSec : 10) * 10;  // 100ms per retry
		while ( (iTlsRes = xrtTlsHandshake(pConn->pTlsCtx, &pConn->tConn)) == XRT_NET_AGAIN ) {
			// 发送待发数据
			if ( pConn->pTlsCtx->tSendBuf.iSize > 0 ) {
				size_t iSent = 0;
				xrtSockSend(&pConn->tConn, pConn->pTlsCtx->tSendBuf.pData,
					pConn->pTlsCtx->tSendBuf.iSize, &iSent);
				if ( iSent > 0 ) xrtNetBufConsume(&pConn->pTlsCtx->tSendBuf, iSent);
			}
			
			// select 等待 IO 就绪，替代 Sleep(10)
			fd_set tReadSet, tWriteSet;
			FD_ZERO(&tReadSet); FD_ZERO(&tWriteSet);
			FD_SET(pConn->tConn.hSocket, &tReadSet);
			if ( pConn->pTlsCtx->tSendBuf.iSize > 0 ) {
				FD_SET(pConn->tConn.hSocket, &tWriteSet);
			}
			struct timeval tSelTimeout = {0, 100000};  // 100ms
			select((int)pConn->tConn.hSocket + 1, &tReadSet,
				pConn->pTlsCtx->tSendBuf.iSize > 0 ? &tWriteSet : NULL, NULL, &tSelTimeout);
			
			iRetries++;
			if ( iRetries > iMaxRetries ) {
				#ifdef DEBUG_TRACE
					printf("    [HTTP] TLS handshake timeout after %d retries\n", iRetries);
				#endif
				xrtTlsDestroy(pConn->pTlsCtx);
				pConn->pTlsCtx = NULL;
				xrtSockClose(&pConn->tConn);
				return false;
			}
		}
		
		// 发送残余 TLS 数据
		if ( pConn->pTlsCtx->tSendBuf.iSize > 0 ) {
			size_t iSent = 0;
			xrtSockSend(&pConn->tConn, pConn->pTlsCtx->tSendBuf.pData,
				pConn->pTlsCtx->tSendBuf.iSize, &iSent);
			if ( iSent > 0 ) xrtNetBufConsume(&pConn->pTlsCtx->tSendBuf, iSent);
		}
		
		if ( iTlsRes != XRT_NET_OK ) {
			xrtTlsDestroy(pConn->pTlsCtx);
			pConn->pTlsCtx = NULL;
			xrtSockClose(&pConn->tConn);
			return false;
		}
		
		pConn->tConn.bTlsEnabled = true;
		pConn->tConn.pTlsCtx = pConn->pTlsCtx;
	}
	
	pConn->bConnected = true;
	return true;
}

// 发送数据 (自动处理 TLS)
static bool __xrt_http_send(__xrt_http_conn* pConn, str pData, size_t iLen)
{
	if ( !pConn->bConnected || iLen == 0 ) return false;
	
	if ( pConn->bHttps && pConn->pTlsCtx ) {
		size_t iWritten = 0;
		xnet_result iRes = xrtTlsWrite(pConn->pTlsCtx, pData, iLen, &iWritten);
		if ( iRes != XRT_NET_OK ) return false;
		// 发送 TLS 加密后的数据
		if ( pConn->pTlsCtx->tSendBuf.iSize > 0 ) {
			size_t iSent = 0;
			size_t iRemaining = pConn->pTlsCtx->tSendBuf.iSize;
			size_t iOffset = 0;
			while ( iRemaining > 0 ) {
				xrtSockSend(&pConn->tConn, pConn->pTlsCtx->tSendBuf.pData + iOffset,
					iRemaining, &iSent);
				if ( iSent <= 0 ) break;
				iOffset += iSent;
				iRemaining -= iSent;
			}
			xrtNetBufClear(&pConn->pTlsCtx->tSendBuf);
		}
		return true;
	} else {
		size_t iSent = 0;
		size_t iRemaining = iLen;
		size_t iOffset = 0;
		while ( iRemaining > 0 ) {
			xnet_result iRes = xrtSockSend(&pConn->tConn, pData + iOffset, iRemaining, &iSent);
			if ( iRes != XRT_NET_OK || iSent == 0 ) return false;
			iOffset += iSent;
			iRemaining -= iSent;
		}
		return true;
	}
}

// 接收数据 (自动处理 TLS, 使用 select 等待)
static int __xrt_http_recv(__xrt_http_conn* pConn, char* pBuf, size_t iBufSize, int iTimeoutSec)
{
	if ( !pConn->bConnected ) return -1;
	
	// 使用 select 等待可读
	fd_set tReadSet;
	FD_ZERO(&tReadSet);
	FD_SET(pConn->tConn.hSocket, &tReadSet);
	struct timeval tTimeout;
	tTimeout.tv_sec = iTimeoutSec > 0 ? iTimeoutSec : 10;
	tTimeout.tv_usec = 0;
	
	int iSelRes = select((int)pConn->tConn.hSocket + 1, &tReadSet, NULL, NULL, &tTimeout);
	if ( iSelRes <= 0 ) return -1;  // 超时或错误
	
	if ( pConn->bHttps && pConn->pTlsCtx ) {
		// 先从 socket 读原始数据到 TLS 缓冲区
		char aTlsBuf[8192];
		size_t iTlsRecvd = 0;
		xnet_result iTlsRes = xrtSockRecv(&pConn->tConn, aTlsBuf, sizeof(aTlsBuf), &iTlsRecvd);
		if ( iTlsRes == XRT_NET_OK && iTlsRecvd > 0 ) {
			xrtNetBufAppend(&pConn->pTlsCtx->tRecvBuf, aTlsBuf, iTlsRecvd);
		} else if ( iTlsRes == XRT_NET_CLOSED ) {
			return 0;  // 连接关闭
		}
		
		// TLS 解密
		size_t iRead = 0;
		xnet_result iRes = xrtTlsRead(pConn->pTlsCtx, pBuf, iBufSize, &iRead);
		if ( iRes == XRT_NET_OK && iRead > 0 ) return (int)iRead;
		if ( iRes == XRT_NET_CLOSED ) return 0;
		return -1;
	} else {
		size_t iRecvd = 0;
		xnet_result iRes = xrtSockRecv(&pConn->tConn, pBuf, iBufSize, &iRecvd);
		if ( iRes == XRT_NET_OK && iRecvd > 0 ) return (int)iRecvd;
		if ( iRes == XRT_NET_CLOSED ) return 0;
		return -1;
	}
}

// 关闭连接
static void __xrt_http_close(__xrt_http_conn* pConn)
{
	if ( !pConn ) return;
	
	if ( pConn->pTlsCtx ) {
		xrtTlsClose(pConn->pTlsCtx);
		// 发送 close_notify
		if ( pConn->pTlsCtx->tSendBuf.iSize > 0 ) {
			size_t iSent = 0;
			xrtSockSend(&pConn->tConn, pConn->pTlsCtx->tSendBuf.pData,
				pConn->pTlsCtx->tSendBuf.iSize, &iSent);
		}
		xrtTlsDestroy(pConn->pTlsCtx);
		pConn->pTlsCtx = NULL;
	}
	
	xrtSockClose(&pConn->tConn);
	pConn->bConnected = false;
}



/* ============================== HTTP 请求构建 ============================== */

static str __xrt_http_method_str(xhttp_method iMethod)
{
	switch ( iMethod ) {
		case XHTTP_GET:    return "GET";
		case XHTTP_POST:   return "POST";
		case XHTTP_PUT:    return "PUT";
		case XHTTP_DELETE: return "DELETE";
		case XHTTP_PATCH:  return "PATCH";
		case XHTTP_HEAD:   return "HEAD";
		default:           return "GET";
	}
}

// 构建 HTTP 请求文本到 xbuffer
static void __xrt_http_build_request(
	xhttp_method iMethod,
	const xurl pURL,
	str sHeaders,
	str pBody,
	size_t iBodyLen,
	str sContentType,
	xbuffer pBuf)
{
	char aTmp[4096];
	
	// 请求行
	int iLen = snprintf(aTmp, sizeof(aTmp), "%s %s HTTP/1.1\r\n",
		__xrt_http_method_str(iMethod), pURL->sPath);
	xrtBufferAppend(pBuf, aTmp, iLen, 0);
	
	// Host 头
	if ( (pURL->bHttps && pURL->iPort != 443) || (!pURL->bHttps && pURL->iPort != 80) ) {
		iLen = snprintf(aTmp, sizeof(aTmp), "Host: %s:%d\r\n", pURL->sHost, pURL->iPort);
	} else {
		iLen = snprintf(aTmp, sizeof(aTmp), "Host: %s\r\n", pURL->sHost);
	}
	xrtBufferAppend(pBuf, aTmp, iLen, 0);
	
	// 默认头
	xrtBufferAppend(pBuf, "User-Agent: xrt/1.0\r\n", 21, 0);
	xrtBufferAppend(pBuf, "Accept: */*\r\n", 13, 0);
	xrtBufferAppend(pBuf, "Connection: close\r\n", 19, 0);
	
	// Content-Type
	if ( pBody && iBodyLen > 0 ) {
		if ( sContentType && sContentType[0] ) {
			iLen = snprintf(aTmp, sizeof(aTmp), "Content-Type: %s\r\n", sContentType);
		} else {
			iLen = snprintf(aTmp, sizeof(aTmp), "Content-Type: application/x-www-form-urlencoded\r\n");
		}
		xrtBufferAppend(pBuf, aTmp, iLen, 0);
		
		// Content-Length
		iLen = snprintf(aTmp, sizeof(aTmp), "Content-Length: %u\r\n", (unsigned int)iBodyLen);
		xrtBufferAppend(pBuf, aTmp, iLen, 0);
	}
	
	// 用户自定义头
	if ( sHeaders && sHeaders[0] ) {
		size_t iHdrLen = strlen(sHeaders);
		xrtBufferAppend(pBuf, (ptr)sHeaders, (uint32)iHdrLen, 0);
		// 确保以 \r\n 结尾
		if ( iHdrLen < 2 || sHeaders[iHdrLen - 1] != '\n' ) {
			xrtBufferAppend(pBuf, "\r\n", 2, 0);
		}
	}
	
	// 空行结束头部
	xrtBufferAppend(pBuf, "\r\n", 2, 0);
	
	// 请求正文
	if ( pBody && iBodyLen > 0 ) {
		xrtBufferAppend(pBuf, (ptr)pBody, (uint32)iBodyLen, 0);
	}
}



/* ============================== HTTP 响应解析 ============================== */

// 创建响应对象
static xhttpresp __xrt_http_resp_create(void)
{
	xhttpresp pResp = (xhttpresp)xrtCalloc(1, sizeof(xhttpresp_struct));
	if ( pResp ) {
		xrtBufferInit(&pResp->tBody, 8192);
		xrtBufferInit(&pResp->tRawHeaders, 2048);
		pResp->iContentLength = (size_t)-1;
	}
	return pResp;
}

// 解析状态行: "HTTP/1.1 200 OK\r\n"
static bool __xrt_http_parse_status_line(str sLine, xhttpresp pResp)
{
	// HTTP/x.x
	if ( strncmp(sLine, "HTTP/", 5) != 0 ) return false;
	
	str p = sLine + 5;
	int i = 0;
	while ( *p && *p != ' ' && i < 15 ) {
		pResp->sVersion[i++] = *p++;
	}
	pResp->sVersion[i] = '\0';
	
	// 跳过空格
	while ( *p == ' ' ) p++;
	
	// 状态码
	pResp->iStatusCode = atoi(p);
	
	// 跳过状态码
	while ( *p >= '0' && *p <= '9' ) p++;
	while ( *p == ' ' ) p++;
	
	// 状态文本
	i = 0;
	while ( *p && *p != '\r' && *p != '\n' && i < 63 ) {
		pResp->sStatusText[i++] = *p++;
	}
	pResp->sStatusText[i] = '\0';
	
	return (pResp->iStatusCode > 0);
}

// 从原始头部中提取指定 Header 值 (不区分大小写)
static str __xrt_http_find_header(str sHeaders, str sName, str sBuf, size_t iBufSize)
{
	if ( !sHeaders || !sName ) return NULL;
	
	size_t iNameLen = strlen(sName);
	str p = sHeaders;
	
	while ( *p ) {
		// 不区分大小写比较
		bool bMatch = true;
		for ( size_t i = 0; i < iNameLen; i++ ) {
			char a = p[i], b = sName[i];
			if ( a >= 'A' && a <= 'Z' ) a += 32;
			if ( b >= 'A' && b <= 'Z' ) b += 32;
			if ( a != b ) { bMatch = false; break; }
		}
		
		if ( bMatch && p[iNameLen] == ':' ) {
			p += iNameLen + 1;
			while ( *p == ' ' || *p == '\t' ) p++;
			// 提取值直到 \r\n
			size_t i = 0;
			while ( *p && *p != '\r' && *p != '\n' && i < iBufSize - 1 ) {
				sBuf[i++] = *p++;
			}
			sBuf[i] = '\0';
			return sBuf;
		}
		
		// 跳到下一行
		while ( *p && *p != '\n' ) p++;
		if ( *p == '\n' ) p++;
	}
	
	return NULL;
}

// 解析 Chunked 编码数据到 pOut
static bool __xrt_http_decode_chunked(str pData, size_t iLen, xbuffer pOut)
{
	str p = pData;
	str pEnd = pData + iLen;
	
	while ( p < pEnd ) {
		// 解析 chunk 大小 (十六进制)
		size_t iChunkSize = 0;
		while ( p < pEnd && *p != '\r' && *p != '\n' ) {
			char c = *p;
			if ( c >= '0' && c <= '9' ) iChunkSize = iChunkSize * 16 + (c - '0');
			else if ( c >= 'a' && c <= 'f' ) iChunkSize = iChunkSize * 16 + (c - 'a' + 10);
			else if ( c >= 'A' && c <= 'F' ) iChunkSize = iChunkSize * 16 + (c - 'A' + 10);
			else break;  // chunk extension
			p++;
		}
		
		// 跳过 \r\n
		if ( p < pEnd && *p == '\r' ) p++;
		if ( p < pEnd && *p == '\n' ) p++;
		
		// chunk size 0 表示结束
		if ( iChunkSize == 0 ) break;
		
		// 复制 chunk 数据
		if ( p + iChunkSize <= pEnd ) {
			xrtBufferAppend(pOut, (ptr)p, (uint32)iChunkSize, 0);
			p += iChunkSize;
		} else {
			// 数据不完整
			xrtBufferAppend(pOut, (ptr)p, (uint32)(pEnd - p), 0);
			break;
		}
		
		// 跳过 chunk 后的 \r\n
		if ( p < pEnd && *p == '\r' ) p++;
		if ( p < pEnd && *p == '\n' ) p++;
	}
	
	return true;
}



/* ============================== Cookie 管理内部辅助 ============================== */

static bool __xrt_http_cookie_free_cb(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	str sVal = (str)xrtDictGetPtr((xdict)pArg, pKey->Key, pKey->KeyLen);
	if ( sVal ) xrtFree(sVal);
	return false;  // 继续遍历
}

static void __xrt_http_cookies_destroy(xdict pCookies)
{
	if ( !pCookies ) return;
	xrtDictWalk(pCookies, __xrt_http_cookie_free_cb, pCookies);
	xrtDictDestroy(pCookies);
}

// 遍历回调: 构建 Cookie 请求头
static bool __xrt_http_cookie_build_cb(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	xbuffer pBuf = (xbuffer)pArg;
	// pVal 指向字典内部的值槽, 其中存储的是 ptr (cookie value 字符串)
	str sCookieVal = *(str*)pVal;
	if ( !sCookieVal ) return false;
	
	if ( pBuf->Length > 8 ) {  // 已有 "Cookie: " 之后的内容
		xrtBufferAppend(pBuf, "; ", 2, 0);
	}
	xrtBufferAppend(pBuf, pKey->Key, pKey->KeyLen, 0);
	xrtBufferAppend(pBuf, "=", 1, 0);
	xrtBufferAppend(pBuf, sCookieVal, (uint32)strlen(sCookieVal), 0);
	return false;  // 继续遍历
}

// 构建 "Cookie: name1=val1; name2=val2\r\n" 字符串, 调用者负责 xrtFree
static str __xrt_http_build_cookie_header(xdict pCookies)
{
	if ( !pCookies ) return NULL;
	
	xbuffer_struct tBuf;
	xrtBufferInit(&tBuf, 512);
	xrtBufferAppend(&tBuf, "Cookie: ", 8, 0);
	
	xrtDictWalk(pCookies, __xrt_http_cookie_build_cb, &tBuf);
	
	if ( tBuf.Length <= 8 ) {
		// 没有任何 cookie
		xrtBufferUnit(&tBuf);
		return NULL;
	}
	
	xrtBufferAppend(&tBuf, "\r\n", 2, XBUF_ANSI);
	
	// 转移缓冲区所有权
	str sRet = tBuf.Buffer;
	// 不调用 xrtBufferUnit, 因为我们要保留 Buffer
	return sRet;
}

// 从响应头中提取所有 Set-Cookie 并存入 xdict
static void __xrt_http_save_cookies(xhttpresp pResp, xdict pCookies)
{
	if ( !pResp || !pResp->tRawHeaders.Buffer || !pCookies ) return;
	
	str p = pResp->tRawHeaders.Buffer;
	str sSetCookie = "set-cookie:";
	
	while ( *p ) {
		// 不区分大小写匹配 "set-cookie:"
		bool bMatch = true;
		for ( int i = 0; sSetCookie[i]; i++ ) {
			char a = p[i];
			if ( (a >= 'A') && (a <= 'Z') ) a += 32;
			if ( a != sSetCookie[i] ) { bMatch = false; break; }
		}
		
		if ( bMatch ) {
			p += 11;  // strlen("set-cookie:")
			while ( *p == ' ' || *p == '\t' ) p++;
			
			// 提取 name
			str pNameStart = p;
			while ( *p && *p != '=' && *p != '\r' && *p != '\n' ) p++;
			size_t iNameLen = (size_t)(p - pNameStart);
			
			if ( *p == '=' && iNameLen > 0 ) {
				p++;  // 跳过 '='
				// 提取 value (到 ';' 或行尾)
				str pValStart = p;
				while ( *p && *p != ';' && *p != '\r' && *p != '\n' ) p++;
				size_t iValLen = (size_t)(p - pValStart);
				
				// 释放旧值
				str sOldVal = (str)xrtDictGetPtr(pCookies, (ptr)pNameStart, (uint32)iNameLen);
				if ( sOldVal ) xrtFree(sOldVal);
				
				// 存储新值
				str sNewVal = xrtCopyStr((str)pValStart, (uint32)iValLen);
				xrtDictSetPtr(pCookies, (ptr)pNameStart, (uint32)iNameLen, sNewVal, NULL);
			}
		}
		
		// 跳到下一行
		while ( *p && *p != '\n' ) p++;
		if ( *p == '\n' ) p++;
	}
}



/* ============================== 核心 HTTP 执行引擎 ============================== */

static xhttpresp __xrt_http_execute(
	xhttp_method iMethod,
	str sURL,
	str sHeaders,
	str pBody,
	size_t iBodyLen,
	str sContentType,
	xhttp_proc procOnData,
	int iMaxRedirects,
	int iTimeoutSec,
	bool bVerifySSL,
	str sFilePath,
	xdict pCookies)
{
	if ( !sURL ) return NULL;
	
	// 解析 URL
	xurl_struct tURL;
	if ( !xrtUrlParse(sURL, &tURL) ) {
		#ifdef DEBUG_TRACE
			printf("    [HTTP] URL parse failed: %s\n", sURL);
		#endif
		return NULL;
	}
	
	#ifdef DEBUG_TRACE
		printf("    [HTTP] %s %s://%s:%d%s\n",
			__xrt_http_method_str(iMethod),
			tURL.bHttps ? "https" : "http",
			tURL.sHost, tURL.iPort, tURL.sPath);
	#endif
	
	// 建立连接
	__xrt_http_conn tConn;
	if ( !__xrt_http_connect(&tConn, &tURL, iTimeoutSec) ) {
		#ifdef DEBUG_TRACE
			printf("    [HTTP] Connect failed\n");
		#endif
		return NULL;
	}
	
	// Cookie 头构建
	str sCookieHdr = NULL;
	str sMergedHeaders = NULL;
	str sFinalHeaders = sHeaders;
	if ( pCookies ) {
		sCookieHdr = __xrt_http_build_cookie_header(pCookies);
		if ( sCookieHdr ) {
			if ( sHeaders ) {
				size_t iCookieLen = strlen(sCookieHdr);
				size_t iHeadersLen = strlen(sHeaders);
				sMergedHeaders = (str)xrtMalloc(iCookieLen + iHeadersLen + 1);
				if ( sMergedHeaders ) {
					memcpy(sMergedHeaders, sCookieHdr, iCookieLen);
					memcpy(sMergedHeaders + iCookieLen, sHeaders, iHeadersLen);
					sMergedHeaders[iCookieLen + iHeadersLen] = '\0';
					sFinalHeaders = sMergedHeaders;
				}
			} else {
				sFinalHeaders = sCookieHdr;
			}
		}
	}
	
	// 构建请求
	xbuffer_struct tReqBuf;
	xrtBufferInit(&tReqBuf, 4096);
	__xrt_http_build_request(iMethod, &tURL, sFinalHeaders, pBody, iBodyLen, sContentType, &tReqBuf);
	
	// 释放 Cookie 临时字符串
	if ( sCookieHdr ) xrtFree(sCookieHdr);
	if ( sMergedHeaders ) xrtFree(sMergedHeaders);
	
	// 发送请求
	bool bSendOK = __xrt_http_send(&tConn, tReqBuf.Buffer, tReqBuf.Length);
	xrtBufferUnit(&tReqBuf);
	
	if ( !bSendOK ) {
		#ifdef DEBUG_TRACE
			printf("    [HTTP] Send failed\n");
		#endif
		__xrt_http_close(&tConn);
		return NULL;
	}
	
	// 接收响应
	xbuffer_struct tRawBuf;
	xrtBufferInit(&tRawBuf, 16384);
	
	char aRecvBuf[8192];
	bool bHeadersDone = false;
	size_t iHeaderEnd = 0;
	
	// 创建响应对象
	xhttpresp pResp = __xrt_http_resp_create();
	if ( !pResp ) {
		__xrt_http_close(&tConn);
		return NULL;
	}
	
	// 是否使用 chunked 传输
	bool bChunked = false;
	size_t iTotalReceived = 0;
	
	// 文件句柄
	FILE* pFile = NULL;
	if ( sFilePath ) {
		pFile = fopen(sFilePath, "wb");
		if ( !pFile ) {
			xrtBufferUnit(&tRawBuf);
			xrtHttpRespFree(pResp);
			__xrt_http_close(&tConn);
			return NULL;
		}
	}
	
	// 接收循环
	while ( 1 ) {
		int iRecvd = __xrt_http_recv(&tConn, aRecvBuf, sizeof(aRecvBuf), iTimeoutSec);
		
		if ( iRecvd <= 0 ) {
			// 连接关闭或超时
			if ( !bHeadersDone ) {
				// 未收到完整头部
				if ( tRawBuf.Length == 0 ) {
					// 未收到任何数据
					xrtBufferUnit(&tRawBuf);
					if ( pFile ) fclose(pFile);
					xrtHttpRespFree(pResp);
					__xrt_http_close(&tConn);
					return NULL;
				}
			}
			break;
		}
		
		xrtBufferAppend(&tRawBuf, aRecvBuf, iRecvd, 0);
		
		// 检查是否收到完整头部
		if ( !bHeadersDone ) {
			// 搜索 \r\n\r\n
			for ( size_t i = 3; i < tRawBuf.Length; i++ ) {
				if ( tRawBuf.Buffer[i-3] == '\r' && tRawBuf.Buffer[i-2] == '\n' &&
					 tRawBuf.Buffer[i-1] == '\r' && tRawBuf.Buffer[i] == '\n' ) {
					iHeaderEnd = i + 1;  // 头部结束位置 (含 \r\n\r\n)
					bHeadersDone = true;
					
					// 保存原始头部
					xrtBufferAppend(&pResp->tRawHeaders, tRawBuf.Buffer, (uint32)iHeaderEnd, XBUF_ANSI);
					
					// 解析状态行
					__xrt_http_parse_status_line(tRawBuf.Buffer, pResp);
					
					// 解析关键头部
					char aTmpVal[256];
					
					// Content-Length
					if ( __xrt_http_find_header(pResp->tRawHeaders.Buffer, "Content-Length", aTmpVal, sizeof(aTmpVal)) ) {
						pResp->iContentLength = (size_t)atol(aTmpVal);
					}
					
					// Content-Type
					__xrt_http_find_header(pResp->tRawHeaders.Buffer, "Content-Type", pResp->sContentType, sizeof(pResp->sContentType));
					
					// Transfer-Encoding
					if ( __xrt_http_find_header(pResp->tRawHeaders.Buffer, "Transfer-Encoding", aTmpVal, sizeof(aTmpVal)) ) {
						// 检查是否为 chunked (不区分大小写)
						str pTE = aTmpVal;
						while ( *pTE == ' ' ) pTE++;
						if ( (pTE[0] == 'c' || pTE[0] == 'C') && (pTE[1] == 'h' || pTE[1] == 'H') ) {
							bChunked = true;
						}
					}
					
					// 把头部之后的数据作为 body 的开头
					size_t iBodyStart = iHeaderEnd;
					if ( iBodyStart < tRawBuf.Length ) {
						size_t iBodyPart = tRawBuf.Length - iBodyStart;
						if ( bChunked ) {
							// Chunked: 先暂存，最后统一解码
							xrtBufferAppend(&pResp->tBody, tRawBuf.Buffer + iBodyStart, (uint32)iBodyPart, 0);
						} else {
							if ( pFile ) {
								fwrite(tRawBuf.Buffer + iBodyStart, 1, iBodyPart, pFile);
							} else {
								xrtBufferAppend(&pResp->tBody, tRawBuf.Buffer + iBodyStart, (uint32)iBodyPart, 0);
							}
						}
						iTotalReceived += iBodyPart;
						
						// 回调
						if ( procOnData && !pFile ) {
							if ( !procOnData(&pResp->tBody, pResp->iContentLength != (size_t)-1 ? pResp->iContentLength : 0, iTotalReceived) ) {
								// 用户中止
								xrtBufferUnit(&tRawBuf);
								if ( pFile ) fclose(pFile);
								__xrt_http_close(&tConn);
								return pResp;
							}
						} else if ( procOnData && pFile ) {
							// 文件下载进度回调
							xbuffer_struct tProgressBuf;
							xrtBufferInit(&tProgressBuf, 0);
							if ( !procOnData(&tProgressBuf, pResp->iContentLength != (size_t)-1 ? pResp->iContentLength : 0, iTotalReceived) ) {
								xrtBufferUnit(&tProgressBuf);
								xrtBufferUnit(&tRawBuf);
								fclose(pFile);
								__xrt_http_close(&tConn);
								return pResp;
							}
							xrtBufferUnit(&tProgressBuf);
						}
					}
					
					break;
				}
			}
			
			if ( !bHeadersDone ) continue;
			
			// 检查是否已经收完 (非 chunked, Content-Length 已知)
			if ( !bChunked && pResp->iContentLength != (size_t)-1 ) {
				if ( iTotalReceived >= pResp->iContentLength ) break;
			}
			continue;
		}
		
		// 已过头部，直接处理 body 数据
		if ( bChunked ) {
			xrtBufferAppend(&pResp->tBody, aRecvBuf, iRecvd, 0);
		} else {
			if ( pFile ) {
				fwrite(aRecvBuf, 1, iRecvd, pFile);
			} else {
				xrtBufferAppend(&pResp->tBody, aRecvBuf, iRecvd, 0);
			}
		}
		iTotalReceived += iRecvd;
		
		// 回调
		if ( procOnData && !pFile ) {
			if ( !procOnData(&pResp->tBody, pResp->iContentLength != (size_t)-1 ? pResp->iContentLength : 0, iTotalReceived) ) {
				break;  // 用户中止
			}
		} else if ( procOnData && pFile ) {
			xbuffer_struct tProgressBuf;
			xrtBufferInit(&tProgressBuf, 0);
			if ( !procOnData(&tProgressBuf, pResp->iContentLength != (size_t)-1 ? pResp->iContentLength : 0, iTotalReceived) ) {
				xrtBufferUnit(&tProgressBuf);
				break;
			}
			xrtBufferUnit(&tProgressBuf);
		}
		
		// 检查是否已经收完
		if ( !bChunked && pResp->iContentLength != (size_t)-1 ) {
			if ( iTotalReceived >= pResp->iContentLength ) break;
		}
	}
	
	// 清理原始缓冲区
	xrtBufferUnit(&tRawBuf);
	
	// 关闭连接
	__xrt_http_close(&tConn);
	
	// 关闭文件
	if ( pFile ) {
		fclose(pFile);
		pFile = NULL;
	}
	
	// Chunked 解码
	if ( bChunked && pResp->tBody.Length > 0 ) {
		xbuffer_struct tDecoded;
		xrtBufferInit(&tDecoded, pResp->tBody.Length);
		__xrt_http_decode_chunked(pResp->tBody.Buffer, pResp->tBody.Length, &tDecoded);
		xrtBufferUnit(&pResp->tBody);
		pResp->tBody = tDecoded;
	}
	
	// 确保 body 以 \0 结尾 (方便字符串操作)
	if ( pResp->tBody.Buffer ) {
		xrtBufferAppend(&pResp->tBody, "", 0, XBUF_ANSI);
		if ( pResp->tBody.Length > 0 ) pResp->tBody.Length--;  // 不计入 \0 到长度中
	}
	
	// 保存响应中的 Set-Cookie 到 cookie 字典
	if ( pCookies && pResp ) {
		__xrt_http_save_cookies(pResp, pCookies);
	}
	
	// 处理重定向
	if ( iMaxRedirects > 0 && pResp->iStatusCode >= 300 && pResp->iStatusCode < 400 ) {
		char sLocation[2048];
		if ( __xrt_http_find_header(pResp->tRawHeaders.Buffer, "Location", sLocation, sizeof(sLocation)) ) {
			#ifdef DEBUG_TRACE
				printf("    [HTTP] Redirect %d -> %s\n", pResp->iStatusCode, sLocation);
			#endif
			
			// 判断是否为相对路径
			char sFullURL[4096];
			if ( sLocation[0] == '/' ) {
				// 相对路径，拼接完整 URL
				snprintf(sFullURL, sizeof(sFullURL), "%s://%s:%d%s",
					tURL.bHttps ? "https" : "http", tURL.sHost, tURL.iPort, sLocation);
			} else if ( strncmp(sLocation, "http://", 7) == 0 || strncmp(sLocation, "https://", 8) == 0 ) {
				// 绝对 URL
				snprintf(sFullURL, sizeof(sFullURL), "%s", sLocation);
			} else {
				// 不支持的重定向
				return pResp;
			}
			
			// 确定重定向方法
			// 307/308 保持原方法和 body; 301/302/303 转为 GET (不发 body)
			int iCode = pResp->iStatusCode;
			xhttp_method iRedirectMethod = iMethod;
			str pRedirectBody = pBody;
			size_t iRedirectBodyLen = iBodyLen;
			str sRedirectCT = sContentType;
			
			if ( iCode == 301 || iCode == 302 || iCode == 303 ) {
				iRedirectMethod = XHTTP_GET;
				pRedirectBody = NULL;
				iRedirectBodyLen = 0;
				sRedirectCT = NULL;
			}
			
			// 释放当前响应，递归请求
			xrtHttpRespFree(pResp);
			
			return __xrt_http_execute(
				iRedirectMethod, sFullURL, sHeaders,
				pRedirectBody, iRedirectBodyLen, sRedirectCT,
				procOnData, iMaxRedirects - 1, iTimeoutSec, bVerifySSL, sFilePath,
				pCookies);
		}
	}
	
	return pResp;
}



/* ============================== 极简 API 实现 ============================== */

XXAPI xhttpresp xrtHttpGet(str sURL, str sHeaders, xhttp_proc pProc)
{
	return __xrt_http_execute(XHTTP_GET, sURL, sHeaders, NULL, 0, NULL,
		pProc, 5, 10, true, NULL, NULL);
}

XXAPI xhttpresp xrtHttpPost(str sURL, str sBody, str sHeaders, xhttp_proc pProc)
{
	return __xrt_http_execute(XHTTP_POST, sURL, sHeaders,
		sBody, sBody ? strlen(sBody) : 0, NULL,
		pProc, 5, 10, true, NULL, NULL);
}

XXAPI bool xrtHttpGetFile(str sURL, str sFilePath, str sHeaders, xhttp_proc pProc)
{
	xhttpresp pResp = __xrt_http_execute(XHTTP_GET, sURL, sHeaders, NULL, 0, NULL,
		pProc, 5, 10, true, sFilePath, NULL);
	bool bOK = (pResp && pResp->iStatusCode >= 200 && pResp->iStatusCode < 300);
	xrtHttpRespFree(pResp);
	return bOK;
}

XXAPI bool xrtHttpPostFile(str sURL, str sBody,
	str sFilePath, str sHeaders, xhttp_proc pProc)
{
	xhttpresp pResp = __xrt_http_execute(XHTTP_POST, sURL, sHeaders,
		sBody, sBody ? strlen(sBody) : 0, NULL,
		pProc, 5, 10, true, sFilePath, NULL);
	bool bOK = (pResp && pResp->iStatusCode >= 200 && pResp->iStatusCode < 300);
	xrtHttpRespFree(pResp);
	return bOK;
}

// 释放响应对象
XXAPI void xrtHttpRespFree(xhttpresp pResp)
{
	if ( !pResp ) return;
	xrtBufferUnit(&pResp->tBody);
	xrtBufferUnit(&pResp->tRawHeaders);
	xrtFree(pResp);
}



/* ============================== 完整 API - 请求对象 ============================== */

XXAPI xhttpreq xrtHttpReqCreate(xhttp_method iMethod, str sURL)
{
	xhttpreq pReq = (xhttpreq)xrtCalloc(1, sizeof(xhttpreq_struct));
	if ( !pReq ) return NULL;
	
	pReq->iMethod = iMethod;
	if ( sURL ) {
		size_t iLen = strlen(sURL);
		if ( iLen >= sizeof(pReq->sURL) ) iLen = sizeof(pReq->sURL) - 1;
		memcpy(pReq->sURL, sURL, iLen);
		pReq->sURL[iLen] = '\0';
	}
	
	xrtBufferInit(&pReq->tHeaders, 512);
	xrtBufferInit(&pReq->tBody, 1024);
	xrtBufferInit(&pReq->tMultipart, 4096);
	
	pReq->iMaxRedirects = 5;
	pReq->iTimeoutSec = 10;
	pReq->bVerifySSL = true;
	pReq->bIsMultipart = false;
	
	return pReq;
}

XXAPI void xrtHttpReqFree(xhttpreq pReq)
{
	if ( !pReq ) return;
	xrtBufferUnit(&pReq->tHeaders);
	xrtBufferUnit(&pReq->tBody);
	xrtBufferUnit(&pReq->tMultipart);
	// 非持久化模式: 自动清理本地 cookie
	if ( pReq->pCookies && !pReq->bCookiePersist ) {
		__xrt_http_cookies_destroy(pReq->pCookies);
		pReq->pCookies = NULL;
	}
	xrtFree(pReq);
}

XXAPI void xrtHttpReqSetHeader(xhttpreq pReq, str sName, str sValue)
{
	if ( !pReq || !sName || !sValue ) return;
	char aBuf[4096];
	int iLen = snprintf(aBuf, sizeof(aBuf), "%s: %s\r\n", sName, sValue);
	xrtBufferAppend(&pReq->tHeaders, aBuf, iLen, 0);
}

XXAPI void xrtHttpReqSetBody(xhttpreq pReq, str pData, size_t iLen, str sContentType)
{
	if ( !pReq || !pData ) return;
	
	// 重置 body
	pReq->tBody.Length = 0;
	xrtBufferAppend(&pReq->tBody, (ptr)pData, (uint32)iLen, 0);
	
	// 设置 Content-Type
	if ( sContentType && sContentType[0] ) {
		xrtHttpReqSetHeader(pReq, "Content-Type", sContentType);
	}
}

XXAPI void xrtHttpReqAddField(xhttpreq pReq, str sName, str sValue)
{
	if ( !pReq || !sName || !sValue ) return;
	
	// URL 编码追加 key=value&
	if ( pReq->tBody.Length > 0 ) {
		xrtBufferAppend(&pReq->tBody, "&", 1, 0);
	}
	__xrt_http_url_encode_append(sName, &pReq->tBody);
	xrtBufferAppend(&pReq->tBody, "=", 1, 0);
	__xrt_http_url_encode_append(sValue, &pReq->tBody);
}



/* ============================== 完整 API - Multipart 表单 ============================== */

static void __xrt_http_multipart_ensure(xhttpreq pReq)
{
	if ( !pReq->bIsMultipart ) {
		pReq->bIsMultipart = true;
		// 生成随机 boundary
		snprintf(pReq->sBoundary, sizeof(pReq->sBoundary),
			"----XrtBoundary%08X%08X",
			(unsigned int)xrtNow(),
			(unsigned int)((size_t)pReq ^ 0xA5A5A5A5));
	}
}

XXAPI void xrtHttpReqAddFormField(xhttpreq pReq, str sName, str sValue)
{
	if ( !pReq || !sName || !sValue ) return;
	__xrt_http_multipart_ensure(pReq);
	
	char aBuf[4096];
	int iLen = snprintf(aBuf, sizeof(aBuf),
		"--%s\r\nContent-Disposition: form-data; name=\"%s\"\r\n\r\n%s\r\n",
		pReq->sBoundary, sName, sValue);
	xrtBufferAppend(&pReq->tMultipart, aBuf, iLen, 0);
}

XXAPI void xrtHttpReqAddFormFile(xhttpreq pReq, str sFieldName,
	str sFilePath, str sMimeType)
{
	if ( !pReq || !sFieldName || !sFilePath ) return;
	__xrt_http_multipart_ensure(pReq);
	
	// 提取文件名
	str sFileName = sFilePath;
	str p = sFilePath;
	while ( *p ) {
		if ( *p == '/' || *p == '\\' ) sFileName = p + 1;
		p++;
	}
	
	if ( !sMimeType || !sMimeType[0] ) sMimeType = "application/octet-stream";
	
	// 读取文件
	FILE* pFile = fopen(sFilePath, "rb");
	if ( !pFile ) return;
	
	fseek(pFile, 0, SEEK_END);
	long iFileSize = ftell(pFile);
	fseek(pFile, 0, SEEK_SET);
	
	str pFileData = (str)xrtMalloc(iFileSize);
	if ( !pFileData ) { fclose(pFile); return; }
	fread(pFileData, 1, iFileSize, pFile);
	fclose(pFile);
	
	// 写入 multipart 部分
	char aBuf[4096];
	int iLen = snprintf(aBuf, sizeof(aBuf),
		"--%s\r\nContent-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\n"
		"Content-Type: %s\r\n\r\n",
		pReq->sBoundary, sFieldName, sFileName, sMimeType);
	xrtBufferAppend(&pReq->tMultipart, aBuf, iLen, 0);
	xrtBufferAppend(&pReq->tMultipart, pFileData, (uint32)iFileSize, 0);
	xrtBufferAppend(&pReq->tMultipart, "\r\n", 2, 0);
	
	xrtFree(pFileData);
}

XXAPI void xrtHttpReqAddFormData(xhttpreq pReq, str sFieldName,
	str sFileName, str pData, size_t iLen, str sMimeType)
{
	if ( !pReq || !sFieldName || !pData ) return;
	__xrt_http_multipart_ensure(pReq);
	
	if ( !sFileName || !sFileName[0] ) sFileName = "data";
	if ( !sMimeType || !sMimeType[0] ) sMimeType = "application/octet-stream";
	
	char aBuf[4096];
	int iHdrLen = snprintf(aBuf, sizeof(aBuf),
		"--%s\r\nContent-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\n"
		"Content-Type: %s\r\n\r\n",
		pReq->sBoundary, sFieldName, sFileName, sMimeType);
	xrtBufferAppend(&pReq->tMultipart, aBuf, iHdrLen, 0);
	xrtBufferAppend(&pReq->tMultipart, (ptr)pData, (uint32)iLen, 0);
	xrtBufferAppend(&pReq->tMultipart, "\r\n", 2, 0);
}



/* ============================== 完整 API - 配置和执行 ============================== */

XXAPI void xrtHttpReqSetTimeout(xhttpreq pReq, int iTimeoutSec)
{
	if ( pReq ) pReq->iTimeoutSec = iTimeoutSec;
}

XXAPI void xrtHttpReqSetRedirect(xhttpreq pReq, int iMaxRedirects)
{
	if ( pReq ) pReq->iMaxRedirects = iMaxRedirects;
}

XXAPI void xrtHttpReqSetVerifySSL(xhttpreq pReq, bool bVerify)
{
	if ( pReq ) pReq->bVerifySSL = bVerify;
}

XXAPI void xrtHttpReqSetCallback(xhttpreq pReq, xhttp_proc pProc)
{
	if ( pReq ) pReq->procOnData = pProc;
}

XXAPI void xrtHttpReqSetUserData(xhttpreq pReq, ptr pData)
{
	if ( pReq ) pReq->pUserData = pData;
}

XXAPI xhttpresp xrtHttpReqExecute(xhttpreq pReq)
{
	if ( !pReq ) return NULL;
	
	// 准备 body 和 Content-Type
	str pBody = NULL;
	size_t iBodyLen = 0;
	str sContentType = NULL;
	char sMultipartCT[128];
	
	// 合并自定义头部为字符串
	str sHeaders = NULL;
	if ( pReq->tHeaders.Length > 0 ) {
		// 确保以 \0 结尾
		xrtBufferAppend(&pReq->tHeaders, "", 0, XBUF_ANSI);
		sHeaders = pReq->tHeaders.Buffer;
	}
	
	if ( pReq->bIsMultipart ) {
		// 添加 multipart 结束标记
		char aEnd[128];
		int iEndLen = snprintf(aEnd, sizeof(aEnd), "--%s--\r\n", pReq->sBoundary);
		xrtBufferAppend(&pReq->tMultipart, aEnd, iEndLen, 0);
		
		pBody = pReq->tMultipart.Buffer;
		iBodyLen = pReq->tMultipart.Length;
		snprintf(sMultipartCT, sizeof(sMultipartCT),
			"multipart/form-data; boundary=%s", pReq->sBoundary);
		sContentType = sMultipartCT;
	} else if ( pReq->tBody.Length > 0 ) {
		pBody = pReq->tBody.Buffer;
		iBodyLen = pReq->tBody.Length;
	}
	
	return __xrt_http_execute(
		pReq->iMethod, pReq->sURL, sHeaders,
		pBody, iBodyLen, sContentType,
		pReq->procOnData, pReq->iMaxRedirects, pReq->iTimeoutSec,
		pReq->bVerifySSL, NULL, pReq->pCookies);
}



/* ============================== Cookie 管理 API ============================== */

XXAPI void xrtHttpReqEnableCookies(xhttpreq pReq, bool bEnable)
{
	if ( !pReq ) return;
	if ( bEnable ) {
		if ( !pReq->pCookies ) {
			pReq->pCookies = xrtDictCreate(sizeof(ptr));
		}
		pReq->bCookiePersist = true;
	} else {
		if ( pReq->pCookies ) {
			__xrt_http_cookies_destroy(pReq->pCookies);
			pReq->pCookies = NULL;
		}
		pReq->bCookiePersist = false;
	}
}

XXAPI void xrtHttpReqSetCookie(xhttpreq pReq, str sName, str sValue)
{
	if ( !pReq || !sName || !sValue ) return;
	
	// 懒创建 xdict
	if ( !pReq->pCookies ) {
		pReq->pCookies = xrtDictCreate(sizeof(ptr));
		if ( !pReq->pCookies ) return;
	}
	
	uint32 iKeyLen = (uint32)strlen(sName);
	
	// 释放旧值
	str sOldVal = (str)xrtDictGetPtr(pReq->pCookies, (ptr)sName, iKeyLen);
	if ( sOldVal ) xrtFree(sOldVal);
	
	// 存储新值
	str sNewVal = xrtCopyStr((str)sValue, (uint32)strlen(sValue));
	xrtDictSetPtr(pReq->pCookies, (ptr)sName, iKeyLen, sNewVal, NULL);
}

XXAPI void xrtHttpReqRemoveCookie(xhttpreq pReq, str sName)
{
	if ( !pReq || !sName || !pReq->pCookies ) return;
	
	uint32 iKeyLen = (uint32)strlen(sName);
	
	// 释放值
	str sOldVal = (str)xrtDictGetPtr(pReq->pCookies, (ptr)sName, iKeyLen);
	if ( sOldVal ) xrtFree(sOldVal);
	
	xrtDictRemove(pReq->pCookies, (ptr)sName, iKeyLen);
}



/* ============================== 响应对象读取函数 ============================== */

XXAPI int xrtHttpRespCode(xhttpresp pResp)
{
	return pResp ? pResp->iStatusCode : 0;
}

XXAPI str xrtHttpRespBody(xhttpresp pResp)
{
	return (pResp && pResp->tBody.Buffer) ? pResp->tBody.Buffer : "";
}

XXAPI size_t xrtHttpRespBodyLen(xhttpresp pResp)
{
	return pResp ? pResp->tBody.Length : 0;
}

XXAPI str xrtHttpRespHeader(xhttpresp pResp, str sName)
{
	if ( !pResp || !pResp->tRawHeaders.Buffer || !sName ) return NULL;
	return __xrt_http_find_header(pResp->tRawHeaders.Buffer, sName,
		__xrt_http_tmpbuf, sizeof(__xrt_http_tmpbuf));
}

XXAPI str xrtHttpRespCookie(xhttpresp pResp, str sName)
{
	if ( !pResp || !pResp->tRawHeaders.Buffer || !sName ) return NULL;
	
	size_t iNameLen = strlen(sName);
	str p = pResp->tRawHeaders.Buffer;
	
	// 搜索所有 Set-Cookie 头
	while ( *p ) {
		// 不区分大小写匹配 "set-cookie:"
		bool bMatch = true;
		str sSetCookie = "set-cookie:";
		for ( int i = 0; sSetCookie[i]; i++ ) {
			char a = p[i];
			if ( a >= 'A' && a <= 'Z' ) a += 32;
			if ( a != sSetCookie[i] ) { bMatch = false; break; }
		}
		
		if ( bMatch ) {
			p += 11;  // strlen("set-cookie:")
			while ( *p == ' ' || *p == '\t' ) p++;
			
			// 检查 cookie 名称是否匹配: name=value
			if ( strncmp(p, sName, iNameLen) == 0 && p[iNameLen] == '=' ) {
				p += iNameLen + 1;
				size_t i = 0;
				while ( *p && *p != ';' && *p != '\r' && *p != '\n' && i < sizeof(__xrt_http_tmpbuf) - 1 ) {
					__xrt_http_tmpbuf[i++] = *p++;
				}
				__xrt_http_tmpbuf[i] = '\0';
				return __xrt_http_tmpbuf;
			}
		}
		
		// 跳到下一行
		while ( *p && *p != '\n' ) p++;
		if ( *p == '\n' ) p++;
	}
	
	return NULL;
}

XXAPI str xrtHttpRespContentType(xhttpresp pResp)
{
	return (pResp && pResp->sContentType[0]) ? pResp->sContentType : NULL;
}

