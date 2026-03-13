



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
#if defined(__TINYC__) && (defined(_WIN32) || defined(_WIN64))
	#define __XRT_HTTP_THREAD_LOCAL __declspec(thread)
#elif defined(__GNUC__)
	#define __XRT_HTTP_THREAD_LOCAL __thread
#elif defined(_WIN32) || defined(_WIN64)
	#define __XRT_HTTP_THREAD_LOCAL __declspec(thread)
#else
	#define __XRT_HTTP_THREAD_LOCAL __thread
#endif
static __XRT_HTTP_THREAD_LOCAL char __xrt_http_tmpbuf[4096];

typedef struct {
	char* sName;
	char* sValue;
	char* sDomain;
	char* sPath;
	time_t iExpiresAt;   // 0 = session cookie
	bool bSecure;
	bool bHostOnly;
} __xrt_http_cookie;

typedef struct {
	xbuffer pBuf;
	xurl pURL;
	time_t iNow;
} __xrt_http_cookie_build_ctx;



static char __xrt_http_tolower(char c)
{
	if ( c >= 'A' && c <= 'Z' ) return (char)(c + 32);
	return c;
}

static bool __xrt_http_str_ieq_n(const char* a, const char* b, size_t iLen)
{
	for ( size_t i = 0; i < iLen; i++ ) {
		if ( __xrt_http_tolower(a[i]) != __xrt_http_tolower(b[i]) ) return false;
	}
	return true;
}

static bool __xrt_http_str_ieq(const char* a, const char* b)
{
	size_t iLenA = a ? strlen(a) : 0;
	size_t iLenB = b ? strlen(b) : 0;
	if ( iLenA != iLenB ) return false;
	return __xrt_http_str_ieq_n(a, b, iLenA);
}

static char* __xrt_http_copy_lower(const char* sText)
{
	if ( !sText ) return NULL;
	size_t iLen = strlen(sText);
	char* sCopy = (char*)xrtMalloc(iLen + 1);
	if ( !sCopy ) return NULL;
	for ( size_t i = 0; i < iLen; i++ ) sCopy[i] = __xrt_http_tolower(sText[i]);
	sCopy[iLen] = '\0';
	return sCopy;
}

static char* __xrt_http_copy_trim(const char* sText, size_t iLen)
{
	while ( iLen > 0 && (*sText == ' ' || *sText == '\t') ) {
		sText++;
		iLen--;
	}
	while ( iLen > 0 && (sText[iLen - 1] == ' ' || sText[iLen - 1] == '\t') ) {
		iLen--;
	}
	return xrtCopyStr((str)sText, (uint32)iLen);
}

static void __xrt_http_cookie_free(__xrt_http_cookie* pCookie)
{
	if ( !pCookie ) return;
	if ( pCookie->sName ) xrtFree(pCookie->sName);
	if ( pCookie->sValue ) xrtFree(pCookie->sValue);
	if ( pCookie->sDomain ) xrtFree(pCookie->sDomain);
	if ( pCookie->sPath ) xrtFree(pCookie->sPath);
	xrtFree(pCookie);
}

static int __xrt_http_month_index(const char* sMonth)
{
	static const char* aMonths[] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
	for ( int i = 0; i < 12; i++ ) {
		if ( __xrt_http_str_ieq_n(sMonth, aMonths[i], 3) ) return i;
	}
	return -1;
}

static time_t __xrt_http_timegm(struct tm* pTM)
{
	#if defined(_WIN32) || defined(_WIN64)
		return _mkgmtime(pTM);
	#else
		return timegm(pTM);
	#endif
}

static bool __xrt_http_parse_http_date(const char* sText, time_t* pOut)
{
	char aWeekDay[8] = {0};
	char aMonth[8] = {0};
	char aTZ[8] = {0};
	int iDay = 0, iYear = 0, iHour = 0, iMin = 0, iSec = 0;
	struct tm tTM;
	int iMonth;

	if ( !sText || !pOut ) return false;
	if ( sscanf(sText, "%7[^,], %d %7s %d %d:%d:%d %7s",
		aWeekDay, &iDay, aMonth, &iYear, &iHour, &iMin, &iSec, aTZ) != 8 ) {
		return false;
	}

	iMonth = __xrt_http_month_index(aMonth);
	if ( iMonth < 0 ) return false;

	memset(&tTM, 0, sizeof(tTM));
	tTM.tm_mday = iDay;
	tTM.tm_mon = iMonth;
	tTM.tm_year = iYear - 1900;
	tTM.tm_hour = iHour;
	tTM.tm_min = iMin;
	tTM.tm_sec = iSec;
	tTM.tm_isdst = 0;

	*pOut = __xrt_http_timegm(&tTM);
	return (*pOut != (time_t)-1);
}

static char* __xrt_http_default_cookie_path(const char* sReqPath)
{
	size_t iLen = 0;
	const char* pQuery = NULL;
	const char* pLastSlash = NULL;

	if ( !sReqPath || sReqPath[0] != '/' ) return xrtCopyStr("/", 1);

	pQuery = strchr(sReqPath, '?');
	iLen = pQuery ? (size_t)(pQuery - sReqPath) : strlen(sReqPath);
	if ( iLen == 0 || sReqPath[0] != '/' ) return xrtCopyStr("/", 1);

	for ( size_t i = 0; i < iLen; i++ ) {
		if ( sReqPath[i] == '/' ) pLastSlash = sReqPath + i;
	}

	if ( !pLastSlash || pLastSlash == sReqPath ) return xrtCopyStr("/", 1);
	return xrtCopyStr((str)sReqPath, (uint32)(pLastSlash - sReqPath));
}

static bool __xrt_http_domain_match(const char* sHost, const __xrt_http_cookie* pCookie)
{
	size_t iHostLen;
	size_t iDomainLen;

	if ( !pCookie || !sHost ) return false;
	if ( !pCookie->sDomain || !pCookie->sDomain[0] ) return true;

	iHostLen = strlen(sHost);
	iDomainLen = strlen(pCookie->sDomain);

	if ( pCookie->bHostOnly ) {
		return __xrt_http_str_ieq(sHost, pCookie->sDomain);
	}

	if ( iHostLen < iDomainLen ) return false;
	if ( !__xrt_http_str_ieq_n(sHost + iHostLen - iDomainLen, pCookie->sDomain, iDomainLen) ) {
		return false;
	}
	if ( iHostLen == iDomainLen ) return true;
	return sHost[iHostLen - iDomainLen - 1] == '.';
}

static bool __xrt_http_path_match(const char* sReqPath, const char* sCookiePath)
{
	size_t iCookieLen;

	if ( !sReqPath || sReqPath[0] == '\0' ) sReqPath = "/";
	if ( !sCookiePath || sCookiePath[0] == '\0' ) return true;

	iCookieLen = strlen(sCookiePath);
	if ( iCookieLen == 1 && sCookiePath[0] == '/' ) return true;
	if ( strncmp(sReqPath, sCookiePath, iCookieLen) != 0 ) return false;
	if ( sReqPath[iCookieLen] == '\0' ) return true;
	if ( sCookiePath[iCookieLen - 1] == '/' ) return true;
	return sReqPath[iCookieLen] == '/';
}

static bool __xrt_http_header_exists(const char* sHeaders, const char* sName)
{
	size_t iNameLen;
	const char* p;

	if ( !sHeaders || !sName ) return false;

	iNameLen = strlen(sName);
	p = sHeaders;
	while ( *p ) {
		size_t i = 0;
		while ( p[i] && p[i] != ':' && p[i] != '\r' && p[i] != '\n' ) i++;
		if ( i == iNameLen && p[i] == ':' && __xrt_http_str_ieq_n(p, sName, iNameLen) ) {
			return true;
		}
		while ( *p && *p != '\n' ) p++;
		if ( *p == '\n' ) p++;
	}

	return false;
}

static bool __xrt_http_has_token_ci(const char* sValue, const char* sToken)
{
	size_t iTokenLen;
	const char* p;

	if ( !sValue || !sToken ) return false;
	iTokenLen = strlen(sToken);
	p = sValue;

	while ( *p ) {
		const char* pStart;
		size_t iLen;

		while ( *p == ' ' || *p == '\t' || *p == ',' ) p++;
		pStart = p;
		while ( *p && *p != ',' && *p != '\r' && *p != '\n' ) p++;
		iLen = (size_t)(p - pStart);
		while ( iLen > 0 && (pStart[iLen - 1] == ' ' || pStart[iLen - 1] == '\t') ) iLen--;
		if ( iLen == iTokenLen && __xrt_http_str_ieq_n(pStart, sToken, iTokenLen) ) {
			return true;
		}
		if ( *p == ',' ) p++;
	}

	return false;
}

static void __xrt_http_append_headers_normalized(xbuffer pBuf, const char* sHeaders)
{
	const char* p = sHeaders;
	bool bLastCRLF = false;

	if ( !pBuf || !sHeaders || !sHeaders[0] ) return;

	while ( *p ) {
		const char* pStart = p;
		while ( *p && *p != '\r' && *p != '\n' ) p++;
		if ( p > pStart ) {
			xrtBufferAppend(pBuf, (ptr)pStart, (uint32)(p - pStart), 0);
		}

		if ( *p == '\r' ) p++;
		if ( *p == '\n' ) p++;
		xrtBufferAppend(pBuf, "\r\n", 2, 0);
		bLastCRLF = true;
	}

	if ( !bLastCRLF ) {
		xrtBufferAppend(pBuf, "\r\n", 2, 0);
	}
}

static bool __xrt_http_buffer_ends_with(const xbuffer pBuf, str sSuffix)
{
	size_t iSuffixLen;
	if ( !pBuf || !pBuf->Buffer || !sSuffix ) return false;
	iSuffixLen = strlen(sSuffix);
	if ( pBuf->Length < iSuffixLen ) return false;
	return memcmp(pBuf->Buffer + pBuf->Length - iSuffixLen, sSuffix, iSuffixLen) == 0;
}

static void __xrt_http_format_url(char* sBuf, size_t iBufSize,
	bool bHttps, const char* sHost, uint16 iPort, const char* sPath)
{
	bool bDefaultPort = (bHttps && iPort == 443) || (!bHttps && iPort == 80);
	if ( !sPath || !sPath[0] ) sPath = "/";
	if ( bDefaultPort ) {
		snprintf(sBuf, iBufSize, "%s://%s%s", bHttps ? "https" : "http", sHost, sPath);
	} else {
		snprintf(sBuf, iBufSize, "%s://%s:%u%s", bHttps ? "https" : "http", sHost, iPort, sPath);
	}
}

static void __xrt_http_normalize_path(char* sPath)
{
	char aTmp[2048];
	char* aSegPos[256];
	int iSegCount = 0;
	char* pOut = aTmp;
	const char* p = sPath;

	if ( !sPath || !sPath[0] ) {
		strcpy(sPath, "/");
		return;
	}

	if ( *p != '/' ) *pOut++ = '/';

	while ( *p ) {
		const char* pSeg = p;
		size_t iSegLen;

		while ( *p == '/' ) p++;
		pSeg = p;
		while ( *p && *p != '/' ) p++;
		iSegLen = (size_t)(p - pSeg);

		if ( iSegLen == 0 ) break;
		if ( iSegLen == 1 && pSeg[0] == '.' ) continue;
		if ( iSegLen == 2 && pSeg[0] == '.' && pSeg[1] == '.' ) {
			if ( iSegCount > 0 ) {
				pOut = aSegPos[--iSegCount];
				*pOut = '\0';
			}
			continue;
		}

		if ( pOut == aTmp || pOut[-1] != '/' ) *pOut++ = '/';
		aSegPos[iSegCount++] = pOut - 1;
		memcpy(pOut, pSeg, iSegLen);
		pOut += iSegLen;
	}

	if ( pOut == aTmp ) *pOut++ = '/';
	*pOut = '\0';
	strcpy(sPath, aTmp);
}

static bool __xrt_http_resolve_redirect_url(char* sBuf, size_t iBufSize,
	xurl pBase, const char* sLocation)
{
	char aBasePath[2048];
	char aCombined[4096];
	char aPathPart[2048];
	const char* pQuery = NULL;

	if ( !sBuf || !pBase || !sLocation || !sLocation[0] ) return false;

	if ( strncmp(sLocation, "http://", 7) == 0 || strncmp(sLocation, "https://", 8) == 0 ) {
		snprintf(sBuf, iBufSize, "%s", sLocation);
		return true;
	}

	if ( strncmp(sLocation, "//", 2) == 0 ) {
		snprintf(sBuf, iBufSize, "%s:%s", pBase->bHttps ? "https" : "http", sLocation);
		return true;
	}

	memset(aBasePath, 0, sizeof(aBasePath));
	strncpy(aBasePath, pBase->sPath[0] ? pBase->sPath : "/", sizeof(aBasePath) - 1);
	pQuery = strchr(aBasePath, '?');
	if ( pQuery ) aBasePath[pQuery - aBasePath] = '\0';
	if ( aBasePath[0] == '\0' ) strcpy(aBasePath, "/");

	if ( sLocation[0] == '?' ) {
		snprintf(aCombined, sizeof(aCombined), "%s%s", aBasePath, sLocation);
		__xrt_http_format_url(sBuf, iBufSize, pBase->bHttps, pBase->sHost, pBase->iPort, aCombined);
		return true;
	}

	if ( sLocation[0] == '/' ) {
		snprintf(aCombined, sizeof(aCombined), "%s", sLocation);
	} else {
		char aBaseDir[2048];
		char* pLastSlash;

		strncpy(aBaseDir, aBasePath, sizeof(aBaseDir) - 1);
		aBaseDir[sizeof(aBaseDir) - 1] = '\0';
		pLastSlash = strrchr(aBaseDir, '/');
		if ( !pLastSlash ) {
			strcpy(aBaseDir, "/");
		} else if ( pLastSlash == aBaseDir ) {
			aBaseDir[1] = '\0';
		} else {
			pLastSlash[1] = '\0';
		}
		snprintf(aCombined, sizeof(aCombined), "%s%s", aBaseDir, sLocation);
	}

	pQuery = strpbrk(aCombined, "?#");
	if ( pQuery ) {
		size_t iPathLen = (size_t)(pQuery - aCombined);
		if ( iPathLen >= sizeof(aPathPart) ) iPathLen = sizeof(aPathPart) - 1;
		memcpy(aPathPart, aCombined, iPathLen);
		aPathPart[iPathLen] = '\0';
	} else {
		strncpy(aPathPart, aCombined, sizeof(aPathPart) - 1);
		aPathPart[sizeof(aPathPart) - 1] = '\0';
	}

	__xrt_http_normalize_path(aPathPart);

	if ( pQuery ) {
		snprintf(aCombined, sizeof(aCombined), "%s%s", aPathPart, pQuery);
	} else {
		snprintf(aCombined, sizeof(aCombined), "%s", aPathPart);
	}

	// HTTP 请求中不发送 fragment
	{
		char* pFragment = strchr(aCombined, '#');
		if ( pFragment ) *pFragment = '\0';
	}

	__xrt_http_format_url(sBuf, iBufSize, pBase->bHttps, pBase->sHost, pBase->iPort, aCombined);
	return true;
}



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

static bool __xrt_http_connect(__xrt_http_conn* pConn, const xurl pURL, int iTimeoutSec, bool bVerifySSL)
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
		tCfg.bVerifyPeer = bVerifySSL;
		
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
	bool bHasContentType = __xrt_http_header_exists(sHeaders, "Content-Type");
	bool bHasContentLength = __xrt_http_header_exists(sHeaders, "Content-Length");
	
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
		if ( !bHasContentType ) {
			if ( sContentType && sContentType[0] ) {
				iLen = snprintf(aTmp, sizeof(aTmp), "Content-Type: %s\r\n", sContentType);
			} else {
				iLen = snprintf(aTmp, sizeof(aTmp), "Content-Type: application/x-www-form-urlencoded\r\n");
			}
			xrtBufferAppend(pBuf, aTmp, iLen, 0);
		}
		
		// Content-Length
		if ( !bHasContentLength ) {
			iLen = snprintf(aTmp, sizeof(aTmp), "Content-Length: %u\r\n", (unsigned int)iBodyLen);
			xrtBufferAppend(pBuf, aTmp, iLen, 0);
		}
	}
	
	// 用户自定义头
	if ( sHeaders && sHeaders[0] ) {
		__xrt_http_append_headers_normalized(pBuf, sHeaders);
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
	const char* p = pData;
	const char* pEnd = pData + iLen;
	bool bSawLastChunk = false;
	
	while ( p < pEnd ) {
		size_t iChunkSize = 0;
		bool bHasDigit = false;
		
		while ( p < pEnd && *p != ';' && *p != '\r' && *p != '\n' ) {
			char c = *p;
			if ( c >= '0' && c <= '9' ) iChunkSize = iChunkSize * 16 + (size_t)(c - '0');
			else if ( c >= 'a' && c <= 'f' ) iChunkSize = iChunkSize * 16 + (size_t)(c - 'a' + 10);
			else if ( c >= 'A' && c <= 'F' ) iChunkSize = iChunkSize * 16 + (size_t)(c - 'A' + 10);
			else return false;
			bHasDigit = true;
			p++;
		}
		
		if ( !bHasDigit ) return false;
		
		while ( p < pEnd && *p != '\r' && *p != '\n' ) p++;  // 跳过 chunk-extension
		if ( p + 1 >= pEnd || p[0] != '\r' || p[1] != '\n' ) return false;
		p += 2;
		
		if ( iChunkSize == 0 ) {
			bSawLastChunk = true;
			while ( p < pEnd ) {
				const char* pLineStart = p;
				while ( p < pEnd && *p != '\r' && *p != '\n' ) p++;
				if ( p + 1 >= pEnd || p[0] != '\r' || p[1] != '\n' ) return false;
				if ( p == pLineStart ) {
					p += 2;
					return p == pEnd;
				}
				p += 2;
			}
			return false;
		}
		
		if ( (size_t)(pEnd - p) < iChunkSize + 2 ) return false;
		xrtBufferAppend(pOut, (ptr)p, (uint32)iChunkSize, 0);
		p += iChunkSize;
		if ( p[0] != '\r' || p[1] != '\n' ) return false;
		p += 2;
	}
	
	return bSawLastChunk;
}



/* ============================== Cookie 管理内部辅助 ============================== */

static bool __xrt_http_cookie_free_cb(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	(void)pVal;
	__xrt_http_cookie* pCookie = (__xrt_http_cookie*)xrtDictGetPtr((xdict)pArg, pKey->Key, pKey->KeyLen);
	if ( pCookie ) __xrt_http_cookie_free(pCookie);
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
	__xrt_http_cookie_build_ctx* pCtx = (__xrt_http_cookie_build_ctx*)pArg;
	__xrt_http_cookie* pCookie = pVal ? *((__xrt_http_cookie**)pVal) : NULL;
	(void)pKey;
	if ( !pCtx || !pCtx->pBuf || !pCtx->pURL || !pCookie || !pCookie->sValue ) return false;
	str sReqPath = pCtx->pURL->sPath[0] ? pCtx->pURL->sPath : "/";
	if ( pCookie->iExpiresAt != 0 && pCookie->iExpiresAt <= pCtx->iNow ) return false;
	if ( pCookie->bSecure && !pCtx->pURL->bHttps ) return false;
	if ( !__xrt_http_domain_match(pCtx->pURL->sHost, pCookie) ) return false;
	if ( !__xrt_http_path_match(sReqPath, pCookie->sPath) ) return false;
	
	if ( pCtx->pBuf->Length > 8 ) {  // 已有 "Cookie: " 之后的内容
		xrtBufferAppend(pCtx->pBuf, "; ", 2, 0);
	}
	xrtBufferAppend(pCtx->pBuf, pCookie->sName ? pCookie->sName : pKey->Key,
		pCookie->sName ? (uint32)strlen(pCookie->sName) : pKey->KeyLen, 0);
	xrtBufferAppend(pCtx->pBuf, "=", 1, 0);
	xrtBufferAppend(pCtx->pBuf, pCookie->sValue, (uint32)strlen(pCookie->sValue), 0);
	return false;  // 继续遍历
}

// 构建 "Cookie: name1=val1; name2=val2\r\n" 字符串, 调用者负责 xrtFree
static str __xrt_http_build_cookie_header(xdict pCookies, const xurl pURL)
{
	if ( !pCookies ) return NULL;
	
	xbuffer_struct tBuf;
	__xrt_http_cookie_build_ctx tCtx;
	xrtBufferInit(&tBuf, 512);
	xrtBufferAppend(&tBuf, "Cookie: ", 8, 0);
	
	tCtx.pBuf = &tBuf;
	tCtx.pURL = pURL;
	tCtx.iNow = time(NULL);
	xrtDictWalk(pCookies, __xrt_http_cookie_build_cb, &tCtx);
	
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
static void __xrt_http_save_cookies(xhttpresp pResp, xdict pCookies, const xurl pURL)
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
			__xrt_http_cookie* pCookie = NULL;
			char* sDomain = NULL;
			char* sPath = NULL;
			time_t iExpiresAt = 0;
			bool bSecure = false;
			bool bHostOnly = true;
			
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
				str sValue = xrtCopyStr((str)pValStart, (uint32)iValLen);
				
				pCookie = (__xrt_http_cookie*)xrtCalloc(1, sizeof(__xrt_http_cookie));
				if ( !pCookie || !sValue ) {
					if ( pCookie ) __xrt_http_cookie_free(pCookie);
					if ( sValue ) xrtFree(sValue);
					goto __next_cookie_line;
				}
				
				pCookie->sName = xrtCopyStr((str)pNameStart, (uint32)iNameLen);
				pCookie->sValue = sValue;
				pCookie->sDomain = __xrt_http_copy_lower(pURL ? pURL->sHost : "");
				pCookie->sPath = __xrt_http_default_cookie_path(pURL ? pURL->sPath : "/");
				pCookie->bSecure = false;
				pCookie->bHostOnly = true;
				pCookie->iExpiresAt = 0;
				
				while ( *p == ';' ) {
					str pAttrName;
					size_t iAttrNameLen;
					str pAttrVal = NULL;
					size_t iAttrValLen = 0;
					
					p++;
					while ( *p == ' ' || *p == '\t' ) p++;
					pAttrName = p;
					while ( *p && *p != '=' && *p != ';' && *p != '\r' && *p != '\n' ) p++;
					iAttrNameLen = (size_t)(p - pAttrName);
					if ( *p == '=' ) {
						p++;
						pAttrVal = p;
						while ( *p && *p != ';' && *p != '\r' && *p != '\n' ) p++;
						iAttrValLen = (size_t)(p - pAttrVal);
					}
					
					if ( iAttrNameLen == 6 && __xrt_http_str_ieq_n(pAttrName, "Domain", 6) && pAttrVal ) {
						char* sNewDomain = __xrt_http_copy_trim(pAttrVal, iAttrValLen);
						if ( sNewDomain ) {
							while ( sNewDomain[0] == '.' ) memmove(sNewDomain, sNewDomain + 1, strlen(sNewDomain));
							for ( size_t i = 0; sNewDomain[i]; i++ ) sNewDomain[i] = __xrt_http_tolower(sNewDomain[i]);
							if ( sNewDomain[0] ) {
								if ( pCookie->sDomain ) xrtFree(pCookie->sDomain);
								pCookie->sDomain = sNewDomain;
								pCookie->bHostOnly = false;
								sNewDomain = NULL;
							}
							if ( sNewDomain ) xrtFree(sNewDomain);
						}
					} else if ( iAttrNameLen == 4 && __xrt_http_str_ieq_n(pAttrName, "Path", 4) && pAttrVal ) {
						char* sNewPath = __xrt_http_copy_trim(pAttrVal, iAttrValLen);
						if ( sNewPath ) {
							if ( pCookie->sPath ) xrtFree(pCookie->sPath);
							pCookie->sPath = sNewPath;
						}
					} else if ( iAttrNameLen == 6 && __xrt_http_str_ieq_n(pAttrName, "Secure", 6) ) {
						pCookie->bSecure = true;
					} else if ( iAttrNameLen == 7 && __xrt_http_str_ieq_n(pAttrName, "Max-Age", 7) && pAttrVal ) {
						char* sAge = __xrt_http_copy_trim(pAttrVal, iAttrValLen);
						if ( sAge ) {
							long long iDelta = atoll(sAge);
							if ( iDelta <= 0 ) {
								pCookie->iExpiresAt = 1;
							} else {
								pCookie->iExpiresAt = time(NULL) + (time_t)iDelta;
							}
							xrtFree(sAge);
						}
					} else if ( iAttrNameLen == 7 && __xrt_http_str_ieq_n(pAttrName, "Expires", 7) && pAttrVal ) {
						char* sExpiry = __xrt_http_copy_trim(pAttrVal, iAttrValLen);
						if ( sExpiry ) {
							time_t iParsed = 0;
							if ( __xrt_http_parse_http_date(sExpiry, &iParsed) ) {
								pCookie->iExpiresAt = iParsed;
							}
							xrtFree(sExpiry);
						}
					}
				}
				
				{
					__xrt_http_cookie* pOldCookie = (__xrt_http_cookie*)xrtDictGetPtr(pCookies, (ptr)pNameStart, (uint32)iNameLen);
					bool bDeleteCookie = (pCookie->sValue[0] == '\0') ||
						(pCookie->iExpiresAt != 0 && pCookie->iExpiresAt <= time(NULL));
					
					if ( bDeleteCookie ) {
						if ( pOldCookie ) {
							__xrt_http_cookie_free(pOldCookie);
							xrtDictRemove(pCookies, (ptr)pNameStart, (uint32)iNameLen);
						}
						__xrt_http_cookie_free(pCookie);
						pCookie = NULL;
					} else {
						if ( pOldCookie ) __xrt_http_cookie_free(pOldCookie);
						xrtDictSetPtr(pCookies, (ptr)pNameStart, (uint32)iNameLen, pCookie, NULL);
						pCookie = NULL;
					}
				}
			}
		}
		
__next_cookie_line:
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
	if ( !__xrt_http_connect(&tConn, &tURL, iTimeoutSec, bVerifySSL) ) {
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
		sCookieHdr = __xrt_http_build_cookie_header(pCookies, &tURL);
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
			if ( !bHeadersDone || (!bChunked && pResp->iContentLength != (size_t)-1 &&
				iTotalReceived < pResp->iContentLength) ) {
				xrtBufferUnit(&tRawBuf);
				if ( pFile ) {
					fclose(pFile);
					remove(sFilePath);
				}
				xrtHttpRespFree(pResp);
				__xrt_http_close(&tConn);
				return NULL;
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
						bChunked = __xrt_http_has_token_ci(aTmpVal, "chunked");
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
								if ( fwrite(tRawBuf.Buffer + iBodyStart, 1, iBodyPart, pFile) != iBodyPart ) {
									xrtBufferUnit(&tRawBuf);
									fclose(pFile);
									remove(sFilePath);
									xrtHttpRespFree(pResp);
									__xrt_http_close(&tConn);
									return NULL;
								}
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
								remove(sFilePath);
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
				if ( fwrite(aRecvBuf, 1, iRecvd, pFile) != (size_t)iRecvd ) {
					xrtBufferUnit(&tRawBuf);
					fclose(pFile);
					remove(sFilePath);
					xrtHttpRespFree(pResp);
					__xrt_http_close(&tConn);
					return NULL;
				}
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
	
	// Chunked 解码
	if ( bChunked && pResp->tBody.Length > 0 ) {
		xbuffer_struct tDecoded;
		xrtBufferInit(&tDecoded, pResp->tBody.Length);
		if ( !__xrt_http_decode_chunked(pResp->tBody.Buffer, pResp->tBody.Length, &tDecoded) ) {
			xrtBufferUnit(&tDecoded);
			if ( pFile ) {
				fclose(pFile);
				remove(sFilePath);
			}
			xrtHttpRespFree(pResp);
			return NULL;
		}
		xrtBufferUnit(&pResp->tBody);
		pResp->tBody = tDecoded;
	}
	
	// 关闭文件 / 写出 chunked 正文
	if ( pFile ) {
		if ( bChunked && pResp->tBody.Length > 0 ) {
			if ( fwrite(pResp->tBody.Buffer, 1, pResp->tBody.Length, pFile) != pResp->tBody.Length ) {
				fclose(pFile);
				remove(sFilePath);
				xrtHttpRespFree(pResp);
				return NULL;
			}
		}
		fclose(pFile);
		pFile = NULL;
	}
	
	// 确保 body 以 \0 结尾 (方便字符串操作)
	if ( pResp->tBody.Buffer ) {
		xrtBufferAppend(&pResp->tBody, "", 0, XBUF_ANSI);
		if ( pResp->tBody.Length > 0 ) pResp->tBody.Length--;  // 不计入 \0 到长度中
	}
	
	// 保存响应中的 Set-Cookie 到 cookie 字典
	if ( pCookies && pResp ) {
		__xrt_http_save_cookies(pResp, pCookies, &tURL);
	}
	
	// 处理重定向
	if ( iMaxRedirects > 0 && pResp->iStatusCode >= 300 && pResp->iStatusCode < 400 ) {
		char sLocation[2048];
		if ( __xrt_http_find_header(pResp->tRawHeaders.Buffer, "Location", sLocation, sizeof(sLocation)) ) {
			#ifdef DEBUG_TRACE
				printf("    [HTTP] Redirect %d -> %s\n", pResp->iStatusCode, sLocation);
			#endif
			
			// 解析相对/绝对重定向 URL
			char sFullURL[4096];
			if ( !__xrt_http_resolve_redirect_url(sFullURL, sizeof(sFullURL), &tURL, sLocation) ) {
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
		if ( !__xrt_http_buffer_ends_with(&pReq->tMultipart, aEnd) ) {
			xrtBufferAppend(&pReq->tMultipart, aEnd, iEndLen, 0);
		}
		
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
	__xrt_http_cookie* pCookie;
	__xrt_http_cookie* pOldCookie;
	
	// 懒创建 xdict
	if ( !pReq->pCookies ) {
		pReq->pCookies = xrtDictCreate(sizeof(ptr));
		if ( !pReq->pCookies ) return;
	}
	
	uint32 iKeyLen = (uint32)strlen(sName);
	
	pCookie = (__xrt_http_cookie*)xrtCalloc(1, sizeof(__xrt_http_cookie));
	if ( !pCookie ) return;
	pCookie->sName = xrtCopyStr((str)sName, iKeyLen);
	pCookie->sValue = xrtCopyStr((str)sValue, (uint32)strlen(sValue));
	if ( !pCookie->sName || !pCookie->sValue ) {
		__xrt_http_cookie_free(pCookie);
		return;
	}
	
	pOldCookie = (__xrt_http_cookie*)xrtDictGetPtr(pReq->pCookies, (ptr)sName, iKeyLen);
	if ( pOldCookie ) __xrt_http_cookie_free(pOldCookie);
	xrtDictSetPtr(pReq->pCookies, (ptr)sName, iKeyLen, pCookie, NULL);
}

XXAPI void xrtHttpReqRemoveCookie(xhttpreq pReq, str sName)
{
	if ( !pReq || !sName || !pReq->pCookies ) return;
	
	uint32 iKeyLen = (uint32)strlen(sName);
	
	// 释放值
	__xrt_http_cookie* pOldCookie = (__xrt_http_cookie*)xrtDictGetPtr(pReq->pCookies, (ptr)sName, iKeyLen);
	if ( pOldCookie ) __xrt_http_cookie_free(pOldCookie);
	
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

