#ifndef XRT_XHTTP_H
#define XRT_XHTTP_H

/*
    XRT mainline HTTP/1.1 client on top of xnet.

    This header provides:
      - request and response objects
      - async HTTP/1.1 transactions over plain TCP or builtin TLS
      - sync facades that reuse the same async transport core
      - serial keep-alive connection reuse for the same origin

    Current limitations:
      - whole-body request/response handling is the primary path
      - chunked transfer is supported, but trailers remain metadata-only
      - streaming request/response bodies are still deferred
*/

#if !defined(XRT_BUILD_CORE)
#define XHTTP_METHOD_CAP         16u
#define XHTTP_URL_CAP            1024u
#define XHTTP_HOST_CAP           256u
#define XHTTP_PATH_CAP           1024u
#define XHTTP_HEADER_NAME_CAP    64u
#define XHTTP_HEADER_VALUE_CAP   256u
#define XHTTP_MAX_HEADERS        32u

#define XHTTP_RESP_F_NONE        0x00000000u
#define XHTTP_RESP_F_CHUNKED     0x00000001u
#define XHTTP_RESP_F_KEEPALIVE   0x00000002u
#define XHTTP_RESP_F_UPGRADE     0x00000004u

typedef struct {
	char sName[XHTTP_HEADER_NAME_CAP];
	char sValue[XHTTP_HEADER_VALUE_CAP];
} xhttpheader;

typedef struct {
	bool bHttps;
	uint16 iPort;
	char sHost[XHTTP_HOST_CAP];
	char sPath[XHTTP_PATH_CAP];
} xhttpurl;

typedef struct {
	char sMethod[XHTTP_METHOD_CAP];
	char sURL[XHTTP_URL_CAP];
	xhttpurl tURL;
	xhttpheader arrHeaders[XHTTP_MAX_HEADERS];
	uint32 iHeaderCount;
	char* pBody;
	size_t iBodyLen;
	uint32 iTimeoutMs;
	uint32 iIdleTimeoutMs;
	bool bVerifyPeer;
	xnetproxy* pProxy;
} xhttprequest;

typedef struct {
	uint32 iStatusCode;
	uint32 iFlags;
	uint32 iHeaderCount;
	int64_t iContentLength;
	char sVersion[XCODEC_HTTP1_TOKEN_CAP];
	char sReason[XCODEC_HTTP1_REASON_CAP];
	xhttpheader arrHeaders[XHTTP_MAX_HEADERS];
	char* pBody;
	size_t iBodyLen;
} xhttpresponse;
#endif

typedef struct {
	volatile long iRefCount;
	volatile long iCleanupPosted;
	volatile long iComplete;
	volatile long iIdleTimerGen;
	xnetengine* pEngine;
	struct __xhttp_conn* pConn;
	xnetstream* pStream;
	xnetfuture* pFuture;
	xhttprequest tReq;
	char* pSendBuf;
	size_t iSendLen;
	int iLastSysErr;
	xtlsconfig tTlsCfg;
} __xhttp_tx;

typedef struct {
	__xhttp_tx* pTx;
	long iGeneration;
} __xhttp_idle_timer_ctx;

typedef struct __xhttp_conn {
	struct __xhttp_conn* pNext;
	volatile long iCleanupPosted;
	xnetengine* pEngine;
	xnetstream* pStream;
	__xhttp_tx* pTx;
	char sHost[XHTTP_HOST_CAP];
	uint16 iPort;
	bool bHttps;
	bool bVerifyPeer;
	xnetproxy* pProxy;
	bool bOpen;
	bool bIdle;
	int iLastSysErr;
	xtlsconfig tTlsCfg;
} __xhttp_conn;

static volatile long __g_xhttpPoolLock = 0;
static __xhttp_conn* __g_xhttpIdleConnHead = NULL;


// 内部函数：__xhttpToLower
static char __xhttpToLower(char ch)
{
	if ( ch >= 'A' && ch <= 'Z' ) return (char)(ch + 32);
	return ch;
}


// 内部函数：字符串相等忽略大小写相关处理
static bool __xhttpStrEqNoCase(const char* sA, const char* sB)
{
	size_t i = 0;
	if ( !sA || !sB ) return false;
	while ( sA[i] && sB[i] ) {
		if ( __xhttpToLower(sA[i]) != __xhttpToLower(sB[i]) ) return false;
		i++;
	}
	return sA[i] == '\0' && sB[i] == '\0';
}


// 内部函数：__xhttpAtomicAdd
static long __xhttpAtomicAdd(volatile long* pValue, long iDelta)
{
	return __xnetAtomicAddFetch32(pValue, iDelta);
}


// 内部函数：__xhttpAtomicCompareExchange
static long __xhttpAtomicCompareExchange(volatile long* pValue, long iExchange, long iComparand)
{
	return __xnetAtomicCompareExchange32(pValue, iExchange, iComparand);
}


// 内部函数：__xhttpAtomicLoad
static long __xhttpAtomicLoad(volatile long* pValue)
{
	return __xnetAtomicLoad32(pValue);
}


// 内部函数：复制 Token
static void __xhttpCopyToken(char* sDst, size_t iDstCap, const char* sSrc)
{
	size_t iLen;
	if ( !sDst || iDstCap == 0 ) return;
	if ( !sSrc ) {
		sDst[0] = '\0';
		return;
	}
	iLen = strlen(__xrt_cstr(sSrc));
	if ( iLen >= iDstCap ) iLen = iDstCap - 1u;
	memcpy(sDst, sSrc, iLen);
	sDst[iLen] = '\0';
}


// 内部函数：__xhttpAppendBytes
static bool __xhttpAppendBytes(char** ppBuf, size_t* pLen, size_t* pCap, const void* pData, size_t iDataLen)
{
	size_t iNeedCap;
	char* pNewBuf;
	if ( !ppBuf || !pLen || !pCap || (!pData && iDataLen != 0) ) return false;
	if ( iDataLen == 0 ) return true;
	if ( *pLen + iDataLen + 1u <= *pCap ) {
		memcpy(*ppBuf + *pLen, pData, iDataLen);
		*pLen += iDataLen;
		(*ppBuf)[*pLen] = '\0';
		return true;
	}
	iNeedCap = (*pCap == 0) ? 256u : *pCap;
	while ( iNeedCap < (*pLen + iDataLen + 1u) ) iNeedCap *= 2u;
	pNewBuf = (char*)XNET_ALLOC(iNeedCap);
	if ( !pNewBuf ) return false;
	if ( *ppBuf && *pLen > 0 ) memcpy(pNewBuf, *ppBuf, *pLen);
	XNET_FREE(*ppBuf);
	*ppBuf = pNewBuf;
	*pCap = iNeedCap;
	memcpy(*ppBuf + *pLen, pData, iDataLen);
	*pLen += iDataLen;
	(*ppBuf)[*pLen] = '\0';
	return true;
}


// 内部函数：追加文本
static bool __xhttpAppendText(char** ppBuf, size_t* pLen, size_t* pCap, const char* sText)
{
	return __xhttpAppendBytes(ppBuf, pLen, pCap, sText, sText ? strlen(__xrt_cstr(sText)) : 0);
}


// 内部函数：__xhttpRequestHasHeader
static bool __xhttpRequestHasHeader(const xhttprequest* pReq, const char* sName)
{
	if ( !pReq || !sName ) return false;
	for ( uint32 i = 0; i < pReq->iHeaderCount; ++i ) {
		if ( __xhttpStrEqNoCase(pReq->arrHeaders[i].sName, sName) ) return true;
	}
	return false;
}


// 内部函数：获取 request 头部值
static const char* __xhttpRequestHeaderValue(const xhttprequest* pReq, const char* sName)
{
	if ( !pReq || !sName ) return NULL;
	for ( uint32 i = 0; i < pReq->iHeaderCount; ++i ) {
		if ( __xhttpStrEqNoCase(pReq->arrHeaders[i].sName, sName) ) return pReq->arrHeaders[i].sValue;
	}
	return NULL;
}


// 内部函数：判断是否存在状态 no 正文
static bool __xhttpStatusHasNoBody(uint32 iStatusCode)
{
	return (iStatusCode >= 100u && iStatusCode < 200u) || iStatusCode == 204u || iStatusCode == 304u;
}

static bool __xhttpRequestClone(xhttprequest* pDst, const xhttprequest* pSrc);
static void __xhttpRequestUnitInternal(xhttprequest* pReq);
static bool __xhttpBuildRequestBytes(const xhttprequest* pReq, char** ppOut, size_t* pOutLen);
static xhttpresponse* __xhttpBuildResponse(const xcodecframe* pFrame, const xcodechttp1msg* pMsg, const xnetchain* pChain);
static void __xhttpConnPostCleanup(__xhttp_conn* pConn);
static void __xhttpTxRefreshIdleTimeout(__xhttp_tx* pTx);
static uint32 __xhttpResolveConnectTimeoutMs(const xhttprequest* pReq);


// 内部函数：__xhttpPoolLockAcquire
static void __xhttpPoolLockAcquire(void)
{
	while ( __xnetAtomicCompareExchange32(&__g_xhttpPoolLock, 1, 0) != 0 ) {
		__xnetEngineSleepMs(1u);
	}
}


// 内部函数：__xhttpPoolLockRelease
static void __xhttpPoolLockRelease(void)
{
	(void)__xnetAtomicExchange32(&__g_xhttpPoolLock, 0);
}


// 内部函数：判断是否包含 Token 忽略大小写
static bool __xhttpContainsTokenNoCase(const char* sValue, const char* sToken)
{
	return xrtHttpHeaderContainsToken(sValue, sToken);
}


// 内部函数：构建主机头部
static bool __xhttpMakeHostHeader(const xhttprequest* pReq, char* sOut, size_t iOutCap)
{
	xrturlview tURL;
	if ( !pReq || !sOut || iOutCap == 0u ) return false;
	if ( pReq->sURL[0] != '\0' && xrtUrlParseView(pReq->sURL, &tURL) ) {
		return xrtUrlMakeHostHeader(&tURL, sOut, iOutCap);
	}
	if ( pReq->tURL.sHost[0] == '\0' ) return false;
	return xrtUrlMakeHostHeaderFixed(pReq->tURL.bHttps ? "https" : "http", pReq->tURL.sHost, pReq->tURL.iPort, sOut, iOutCap);
}


// 内部函数：__xhttpRequestWantsClose
static bool __xhttpRequestWantsClose(const xhttprequest* pReq)
{
	if ( !pReq ) return false;
	for ( uint32 i = 0; i < pReq->iHeaderCount; ++i ) {
		if ( __xhttpStrEqNoCase(pReq->arrHeaders[i].sName, "Connection") ) {
			return __xhttpContainsTokenNoCase(pReq->arrHeaders[i].sValue, "close");
		}
	}
	return false;
}


// 内部函数：__xhttpResponseWantsClose
static bool __xhttpResponseWantsClose(const xcodechttp1msg* pMsg)
{
	const char* sConn;
	if ( !pMsg ) return false;
	sConn = xrtCodecHttp1GetHeader(pMsg, "Connection");
	return sConn ? __xhttpContainsTokenNoCase(sConn, "close") : false;
}


// 内部函数：__xhttpConnMatchesReq
static bool __xhttpConnMatchesReq(const __xhttp_conn* pConn, xnetengine* pEngine, const xhttprequest* pReq)
{
	if ( !pConn || !pReq || !pEngine ) return false;
	if ( pConn->pEngine != pEngine ) return false;
	if ( pConn->iPort != pReq->tURL.iPort ) return false;
	if ( pConn->bHttps != pReq->tURL.bHttps ) return false;
	if ( pConn->bVerifyPeer != pReq->bVerifyPeer ) return false;
	if ( pConn->pProxy != pReq->pProxy ) return false;
	if ( strcmp(pConn->sHost, pReq->tURL.sHost) != 0 ) return false;
	if ( !pConn->bOpen || pConn->pStream == NULL || pConn->pTx != NULL ) return false;
	return true;
}


// 内部函数：__xhttpPoolTake
static __xhttp_conn* __xhttpPoolTake(xnetengine* pEngine, const xhttprequest* pReq)
{
	__xhttp_conn** ppConn;
	__xhttp_conn* pConn = NULL;
	if ( !pEngine || !pReq ) return NULL;
	__xhttpPoolLockAcquire();
	ppConn = &__g_xhttpIdleConnHead;
	while ( *ppConn ) {
		if ( __xhttpConnMatchesReq(*ppConn, pEngine, pReq) ) {
			pConn = *ppConn;
			*ppConn = pConn->pNext;
			pConn->pNext = NULL;
			pConn->bIdle = false;
			break;
		}
		ppConn = &(*ppConn)->pNext;
	}
	__xhttpPoolLockRelease();
	return pConn;
}


// 内部函数：__xhttpPoolPut
static bool __xhttpPoolPut(__xhttp_conn* pConn)
{
	if ( !pConn || !pConn->pStream || !pConn->bOpen || pConn->pTx != NULL ) return false;
	__xhttpPoolLockAcquire();
	pConn->pNext = __g_xhttpIdleConnHead;
	__g_xhttpIdleConnHead = pConn;
	pConn->bIdle = true;
	__xhttpPoolLockRelease();
	return true;
}


// 内部函数：删除内存池
static void __xhttpPoolRemove(__xhttp_conn* pConn)
{
	__xhttp_conn** ppCur;
	if ( !pConn ) return;
	__xhttpPoolLockAcquire();
	ppCur = &__g_xhttpIdleConnHead;
	while ( *ppCur ) {
		if ( *ppCur == pConn ) {
			*ppCur = pConn->pNext;
			pConn->pNext = NULL;
			pConn->bIdle = false;
			break;
		}
		ppCur = &(*ppCur)->pNext;
	}
	__xhttpPoolLockRelease();
}


// xrtHttpCloseIdleConnections 相关处理
XXAPI void xrtHttpCloseIdleConnections(xnetengine* pEngine)
{
	__xhttp_conn* pList = NULL;
	__xhttp_conn** ppTail = &pList;
	__xhttp_conn** ppCur;
	__xhttpPoolLockAcquire();
	ppCur = &__g_xhttpIdleConnHead;
	while ( *ppCur ) {
		__xhttp_conn* pConn = *ppCur;
		if ( pEngine != NULL && pConn->pEngine != pEngine ) {
			ppCur = &pConn->pNext;
			continue;
		}
		*ppCur = pConn->pNext;
		pConn->pNext = NULL;
		pConn->bIdle = false;
		*ppTail = pConn;
		ppTail = &pConn->pNext;
	}
	__xhttpPoolLockRelease();
	while ( pList ) {
		__xhttp_conn* pNext = pList->pNext;
		pList->pNext = NULL;
		if ( pList->pStream ) {
			xrtNetStreamClose(pList->pStream, XNET_CLOSE_F_ABORT);
		}
		__xhttpConnPostCleanup(pList);
		pList = pNext;
	}
}


// xrtHttpRequestInit 相关处理
XXAPI void xrtHttpRequestInit(xhttprequest* pReq)
{
	if ( !pReq ) return;
	memset(pReq, 0, sizeof(xhttprequest));
	strcpy(pReq->sMethod, "GET");
	pReq->iTimeoutMs = 30000u;
	pReq->iIdleTimeoutMs = 0u;
	pReq->bVerifyPeer = true;
}


// 内部函数：__xhttpRequestUnitInternal
static void __xhttpRequestUnitInternal(xhttprequest* pReq)
{
	if ( !pReq ) return;
	if ( pReq->pBody ) {
		XNET_FREE(pReq->pBody);
		pReq->pBody = NULL;
	}
	pReq->iBodyLen = 0;
	if ( pReq->pProxy ) {
		xrtNetProxyRelease(pReq->pProxy);
		pReq->pProxy = NULL;
	}
}


// xrtHttpRequestUnit 相关处理
XXAPI void xrtHttpRequestUnit(xhttprequest* pReq)
{
	if ( !pReq ) return;
	__xhttpRequestUnitInternal(pReq);
	memset(pReq, 0, sizeof(xhttprequest));
}


// xrtHttpRequestSetMethod 相关处理
XXAPI bool xrtHttpRequestSetMethod(xhttprequest* pReq, const char* sMethod)
{
	size_t iLen;
	if ( !pReq || !sMethod || !sMethod[0] ) return false;
	iLen = strlen(sMethod);
	if ( iLen >= sizeof(pReq->sMethod) ) iLen = sizeof(pReq->sMethod) - 1u;
	memcpy(pReq->sMethod, sMethod, iLen);
	pReq->sMethod[iLen] = '\0';
	return true;
}


// 设置 HTTP request URL
XXAPI bool xrtHttpRequestSetURL(xhttprequest* pReq, const char* sURL)
{
	size_t iLen;
	if ( !pReq || !sURL || !sURL[0] ) return false;
	if ( !xrtUrlParseFixedTo(sURL, "http", "https", &pReq->tURL.bHttps, pReq->tURL.sHost, sizeof(pReq->tURL.sHost), &pReq->tURL.iPort, pReq->tURL.sPath, sizeof(pReq->tURL.sPath)) ) return false;
	iLen = strlen(sURL);
	if ( iLen >= sizeof(pReq->sURL) ) iLen = sizeof(pReq->sURL) - 1u;
	memcpy(pReq->sURL, sURL, iLen);
	pReq->sURL[iLen] = '\0';
	return true;
}


// 设置 HTTP request 头部
XXAPI bool xrtHttpRequestSetHeader(xhttprequest* pReq, const char* sName, const char* sValue)
{
	xhttpheader* pHeader;
	if ( !pReq || !sName || !sValue ) return false;
	for ( uint32 i = 0; i < pReq->iHeaderCount; ++i ) {
		if ( __xhttpStrEqNoCase(pReq->arrHeaders[i].sName, sName) ) {
			__xhttpCopyToken(pReq->arrHeaders[i].sValue, sizeof(pReq->arrHeaders[i].sValue), sValue);
			return true;
		}
	}
	if ( pReq->iHeaderCount >= XHTTP_MAX_HEADERS ) return false;
	pHeader = &pReq->arrHeaders[pReq->iHeaderCount++];
	__xhttpCopyToken(pHeader->sName, sizeof(pHeader->sName), sName);
	__xhttpCopyToken(pHeader->sValue, sizeof(pHeader->sValue), sValue);
	return true;
}


// xrtHttpRequestSetBodyCopy 相关处理
XXAPI bool xrtHttpRequestSetBodyCopy(xhttprequest* pReq, const void* pData, size_t iLen, const char* sContentType)
{
	char* pBodyCopy = NULL;
	if ( !pReq ) return false;
	if ( pReq->pBody ) {
		XNET_FREE(pReq->pBody);
		pReq->pBody = NULL;
		pReq->iBodyLen = 0;
	}
	if ( pData && iLen > 0 ) {
		pBodyCopy = (char*)XNET_ALLOC(iLen);
		if ( !pBodyCopy ) return false;
		memcpy(pBodyCopy, pData, iLen);
		pReq->pBody = pBodyCopy;
		pReq->iBodyLen = iLen;
	}
	if ( sContentType && sContentType[0] ) {
		return xrtHttpRequestSetHeader(pReq, "Content-Type", sContentType);
	}
	return true;
}


// 设置 HTTP request 超时
XXAPI void xrtHttpRequestSetTimeout(xhttprequest* pReq, uint32 iTimeoutMs)
{
	if ( !pReq ) return;
	pReq->iTimeoutMs = iTimeoutMs;
}


// xrtHttpRequestSetIdleTimeout 相关处理
XXAPI void xrtHttpRequestSetIdleTimeout(xhttprequest* pReq, uint32 iTimeoutMs)
{
	if ( !pReq ) return;
	pReq->iIdleTimeoutMs = iTimeoutMs;
}


// xrtHttpRequestSetVerifyPeer 相关处理
XXAPI void xrtHttpRequestSetVerifyPeer(xhttprequest* pReq, bool bVerifyPeer)
{
	if ( !pReq ) return;
	pReq->bVerifyPeer = bVerifyPeer;
}


// xrtHttpResponseDestroy 相关处理
XXAPI void xrtHttpResponseDestroy(xhttpresponse* pResp)
{
	if ( !pResp ) return;
	if ( pResp->pBody ) {
		XNET_FREE(pResp->pBody);
		pResp->pBody = NULL;
	}
	XNET_FREE(pResp);
}


// 获取 HTTP response 头部
XXAPI const char* xrtHttpResponseHeader(const xhttpresponse* pResp, const char* sName)
{
	if ( !pResp || !sName ) return NULL;
	for ( uint32 i = 0; i < pResp->iHeaderCount; ++i ) {
		if ( __xhttpStrEqNoCase(pResp->arrHeaders[i].sName, sName) ) {
			return pResp->arrHeaders[i].sValue;
		}
	}
	return NULL;
}


// 内部函数：__xhttpRequestClone
static bool __xhttpRequestClone(xhttprequest* pDst, const xhttprequest* pSrc)
{
	if ( !pDst || !pSrc ) return false;
	xrtHttpRequestInit(pDst);
	memcpy(pDst->sMethod, pSrc->sMethod, sizeof(pDst->sMethod));
	memcpy(pDst->sURL, pSrc->sURL, sizeof(pDst->sURL));
	memcpy(&pDst->tURL, &pSrc->tURL, sizeof(pDst->tURL));
	memcpy(pDst->arrHeaders, pSrc->arrHeaders, sizeof(pDst->arrHeaders));
	pDst->iHeaderCount = pSrc->iHeaderCount;
	pDst->iTimeoutMs = pSrc->iTimeoutMs;
	pDst->iIdleTimeoutMs = pSrc->iIdleTimeoutMs;
	pDst->bVerifyPeer = pSrc->bVerifyPeer;
	pDst->pProxy = pSrc->pProxy ? xrtNetProxyAddRef(pSrc->pProxy) : NULL;
	if ( pSrc->pBody && pSrc->iBodyLen > 0 ) {
		pDst->pBody = (char*)XNET_ALLOC(pSrc->iBodyLen);
		if ( !pDst->pBody ) {
			__xhttpRequestUnitInternal(pDst);
			return false;
		}
		memcpy(pDst->pBody, pSrc->pBody, pSrc->iBodyLen);
		pDst->iBodyLen = pSrc->iBodyLen;
	}
	return true;
}


// 内部函数：__xhttpBuildRequestBytes
static bool __xhttpBuildRequestBytes(const xhttprequest* pReq, char** ppOut, size_t* pOutLen)
{
	char* pBuf = NULL;
	size_t iLen = 0;
	size_t iCap = 0;
	char aLine[512];
	bool bChunked;

	if ( !pReq || !ppOut || !pOutLen || pReq->tURL.sHost[0] == '\0' || pReq->sMethod[0] == '\0' ) return false;
	*ppOut = NULL;
	*pOutLen = 0;
	bChunked = __xhttpContainsTokenNoCase(__xhttpRequestHeaderValue(pReq, "Transfer-Encoding"), "chunked");

	if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, pReq->sMethod) ) goto fail;
	if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, " ") ) goto fail;
	if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, pReq->tURL.sPath[0] ? pReq->tURL.sPath : "/") ) goto fail;
	if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, " HTTP/1.1\r\n") ) goto fail;

	if ( !__xhttpRequestHasHeader(pReq, "Host") ) {
		char sHostHeader[384];
		if ( !__xhttpMakeHostHeader(pReq, sHostHeader, sizeof(sHostHeader)) ) goto fail;
		snprintf(aLine, sizeof(aLine), "Host: %s\r\n", sHostHeader);
		if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, aLine) ) goto fail;
	}
	if ( !__xhttpRequestHasHeader(pReq, "Connection") ) {
		if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, "Connection: keep-alive\r\n") ) goto fail;
	}
	if ( !bChunked && pReq->iBodyLen > 0 && !__xhttpRequestHasHeader(pReq, "Content-Length") ) {
		snprintf(aLine, sizeof(aLine), "Content-Length: %llu\r\n", (unsigned long long)pReq->iBodyLen);
		if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, aLine) ) goto fail;
	}
	for ( uint32 i = 0; i < pReq->iHeaderCount; ++i ) {
		if ( bChunked && __xhttpStrEqNoCase(pReq->arrHeaders[i].sName, "Content-Length") ) continue;
		snprintf(aLine, sizeof(aLine), "%s: %s\r\n", pReq->arrHeaders[i].sName, pReq->arrHeaders[i].sValue);
		if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, aLine) ) goto fail;
	}
	if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, "\r\n") ) goto fail;
	if ( bChunked ) {
		snprintf(aLine, sizeof(aLine), "%llX\r\n", (unsigned long long)pReq->iBodyLen);
		if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, aLine) ) goto fail;
		if ( pReq->pBody && pReq->iBodyLen > 0 ) {
			if ( !__xhttpAppendBytes(&pBuf, &iLen, &iCap, pReq->pBody, pReq->iBodyLen) ) goto fail;
		}
		if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, "\r\n0\r\n\r\n") ) goto fail;
	} else if ( pReq->pBody && pReq->iBodyLen > 0 ) {
		if ( !__xhttpAppendBytes(&pBuf, &iLen, &iCap, pReq->pBody, pReq->iBodyLen) ) goto fail;
	}

	*ppOut = pBuf;
	*pOutLen = iLen;
	return true;

fail:
	if ( pBuf ) XNET_FREE(pBuf);
	return false;
}


// 内部函数：__xhttpBuildResponse
static xhttpresponse* __xhttpBuildResponse(const xcodecframe* pFrame, const xcodechttp1msg* pMsg, const xnetchain* pChain)
{
	xhttpresponse* pResp;
	size_t iBodyBytes;
	if ( !pFrame || !pMsg || !pChain ) return NULL;
	pResp = (xhttpresponse*)XNET_ALLOC(sizeof(xhttpresponse));
	if ( !pResp ) return NULL;
	memset(pResp, 0, sizeof(xhttpresponse));
	pResp->iStatusCode = pMsg->iStatusCode;
	pResp->iContentLength = pMsg->iContentLength;
	__xhttpCopyToken(pResp->sVersion, sizeof(pResp->sVersion), pMsg->sVersion);
	__xhttpCopyToken(pResp->sReason, sizeof(pResp->sReason), pMsg->sReason);
	if ( pMsg->iFlags & XCODEC_HTTP1_F_CHUNKED ) pResp->iFlags |= XHTTP_RESP_F_CHUNKED;
	if ( pMsg->iFlags & XCODEC_HTTP1_F_KEEPALIVE ) pResp->iFlags |= XHTTP_RESP_F_KEEPALIVE;
	if ( pMsg->iFlags & XCODEC_HTTP1_F_UPGRADE ) pResp->iFlags |= XHTTP_RESP_F_UPGRADE;
	pResp->iHeaderCount = pMsg->iHeaderCount < XHTTP_MAX_HEADERS ? pMsg->iHeaderCount : XHTTP_MAX_HEADERS;
	for ( uint32 i = 0; i < pResp->iHeaderCount; ++i ) {
		__xhttpCopyToken(pResp->arrHeaders[i].sName, sizeof(pResp->arrHeaders[i].sName), pMsg->arrHeaders[i].sName);
		__xhttpCopyToken(pResp->arrHeaders[i].sValue, sizeof(pResp->arrHeaders[i].sValue), pMsg->arrHeaders[i].sValue);
	}
	iBodyBytes = xrtCodecHttp1BodyBytes(pFrame);
	if ( (pMsg->iFlags & XCODEC_HTTP1_F_CHUNKED) != 0u ) pResp->iContentLength = (int64_t)iBodyBytes;
	if ( iBodyBytes > 0u ) {
		pResp->pBody = (char*)XNET_ALLOC(iBodyBytes + 1u);
		if ( !pResp->pBody ) {
			xrtHttpResponseDestroy(pResp);
			return NULL;
		}
		pResp->iBodyLen = xrtCodecHttp1CopyBody(pChain, pFrame, pResp->pBody, iBodyBytes);
		pResp->pBody[pResp->iBodyLen] = '\0';
	}
	return pResp;
}


// 内部函数：__xhttpTxAddRef
static void __xhttpTxAddRef(__xhttp_tx* pTx)
{
	if ( !pTx ) return;
	(void)__xhttpAtomicAdd(&pTx->iRefCount, 1);
}


// 内部函数：__xhttpTxDiscardTransient
static void __xhttpTxDiscardTransient(__xhttp_tx* pTx)
{
	if ( !pTx ) return;
	if ( pTx->pSendBuf ) {
		XNET_FREE(pTx->pSendBuf);
		pTx->pSendBuf = NULL;
	}
	pTx->iSendLen = 0;
	__xhttpRequestUnitInternal(&pTx->tReq);
}


// 内部函数：__xhttpTxComplete
static bool __xhttpTxComplete(__xhttp_tx* pTx, xnet_result iStatus, xhttpresponse* pResp)
{
	if ( !pTx ) {
		if ( pResp ) xrtHttpResponseDestroy(pResp);
		return false;
	}
	if ( __xhttpAtomicCompareExchange(&pTx->iComplete, 1, 0) != 0 ) {
		if ( pResp ) xrtHttpResponseDestroy(pResp);
		return false;
	}
	if ( pTx->pFuture ) {
		(void)__xnetFutureResolve(pTx->pFuture, iStatus, pResp);
	} else if ( pResp ) {
		xrtHttpResponseDestroy(pResp);
	}
	return true;
}


// 内部函数：__xhttpTxRelease
static void __xhttpTxRelease(__xhttp_tx* pTx)
{
	if ( !pTx ) return;
	if ( __xhttpAtomicAdd(&pTx->iRefCount, -1) == 0 ) {
		__xhttpTxDiscardTransient(pTx);
		pTx->pConn = NULL;
		pTx->pStream = NULL;
		XNET_FREE(pTx);
	}
}


// 内部函数：__xhttpTxCleanupTask
static void __xhttpTxCleanupTask(xnetworker* pWorker, ptr pArg)
{
	__xhttp_tx* pTx = (__xhttp_tx*)pArg;
	(void)pWorker;
	if ( !pTx ) return;
	pTx->pConn = NULL;
	pTx->pStream = NULL;
	__xhttpTxDiscardTransient(pTx);
	__xhttpTxRelease(pTx);
}


// 内部函数：__xhttpTxPostCleanup
static void __xhttpTxPostCleanup(__xhttp_tx* pTx)
{
	uint32 iAffinity = 0u;
	if ( !pTx || !pTx->pEngine ) return;
	if ( __xhttpAtomicCompareExchange(&pTx->iCleanupPosted, 1, 0) != 0 ) return;
	if ( pTx->pStream && pTx->pStream->pWorker ) iAffinity = pTx->pStream->pWorker->iId;
	else if ( pTx->pConn && pTx->pConn->pStream && pTx->pConn->pStream->pWorker ) iAffinity = pTx->pConn->pStream->pWorker->iId;
	if ( xrtNetEnginePost(pTx->pEngine, iAffinity, __xhttpTxCleanupTask, pTx) != XRT_NET_OK ) {
		__xhttpTxCleanupTask(NULL, pTx);
	}
}


// 内部函数：__xhttpConnCleanupTask
static void __xhttpConnCleanupTask(xnetworker* pWorker, ptr pArg)
{
	__xhttp_conn* pConn = (__xhttp_conn*)pArg;
	(void)pWorker;
	if ( !pConn ) return;
	__xhttpPoolRemove(pConn);
	if ( pConn->pStream ) {
		xrtNetStreamDestroy(pConn->pStream);
		pConn->pStream = NULL;
	}
	if ( pConn->pProxy ) {
		xrtNetProxyRelease(pConn->pProxy);
		pConn->pProxy = NULL;
	}
	XNET_FREE(pConn);
}


// 内部函数：__xhttpConnPostCleanup
static void __xhttpConnPostCleanup(__xhttp_conn* pConn)
{
	if ( !pConn ) return;
	if ( __xhttpAtomicCompareExchange(&pConn->iCleanupPosted, 1, 0) != 0 ) return;
	__xhttpPoolRemove(pConn);
	if ( pConn->pEngine && pConn->pStream && pConn->pStream->pWorker ) {
		if ( xrtNetEnginePost(pConn->pEngine, pConn->pStream->pWorker->iId, __xhttpConnCleanupTask, pConn) == XRT_NET_OK ) {
			return;
		}
	}
	__xhttpConnCleanupTask(NULL, pConn);
}


// 内部函数：__xhttpResponseReusable
static bool __xhttpResponseReusable(const __xhttp_tx* pTx, const xcodechttp1msg* pMsg)
{
	if ( !pTx || !pMsg || !pTx->pConn ) return false;
	if ( (pMsg->iFlags & XCODEC_HTTP1_F_KEEPALIVE) == 0u ) return false;
	if ( (pMsg->iFlags & XCODEC_HTTP1_F_UPGRADE) != 0u ) return false;
	if ( __xhttpRequestWantsClose(&pTx->tReq) ) return false;
	if ( __xhttpResponseWantsClose(pMsg) ) return false;
	return pTx->pConn->pStream != NULL && pTx->pConn->bOpen;
}


// 内部函数：__xhttpTxAbortStream
static void __xhttpTxAbortStream(__xhttp_tx* pTx)
{
	if ( !pTx ) return;
	if ( pTx->pConn && pTx->pConn->pStream ) xrtNetStreamClose(pTx->pConn->pStream, XNET_CLOSE_F_ABORT);
	else if ( pTx->pStream ) xrtNetStreamClose(pTx->pStream, XNET_CLOSE_F_ABORT);
}


// 内部函数：__xhttpIdleTimeoutTask
static void __xhttpIdleTimeoutTask(xnetworker* pWorker, ptr pArg)
{
	__xhttp_idle_timer_ctx* pCtx = (__xhttp_idle_timer_ctx*)pArg;
	__xhttp_tx* pTx;
	(void)pWorker;
	if ( !pCtx ) return;
	pTx = pCtx->pTx;
	if ( pTx &&
		__xhttpAtomicLoad(&pTx->iComplete) == 0 &&
		__xhttpAtomicLoad(&pTx->iIdleTimerGen) == pCtx->iGeneration ) {
		(void)__xhttpTxComplete(pTx, XRT_NET_TIMEOUT, NULL);
		__xhttpTxAbortStream(pTx);
	}
	if ( pTx ) __xhttpTxRelease(pTx);
	XNET_FREE(pCtx);
}


// 内部函数：__xhttpTxRefreshIdleTimeout
static void __xhttpTxRefreshIdleTimeout(__xhttp_tx* pTx)
{
	__xhttp_idle_timer_ctx* pCtx;
	uint32 iTimeoutMs;
	long iGeneration;
	uint32 iAffinity = 0u;

	if ( !pTx || !pTx->pEngine ) return;
	iTimeoutMs = pTx->tReq.iIdleTimeoutMs;
	if ( iTimeoutMs == 0u ) return;

	iGeneration = __xhttpAtomicAdd(&pTx->iIdleTimerGen, 1);
	pCtx = (__xhttp_idle_timer_ctx*)XNET_ALLOC(sizeof(__xhttp_idle_timer_ctx));
	if ( !pCtx ) return;
	memset(pCtx, 0, sizeof(__xhttp_idle_timer_ctx));
	pCtx->pTx = pTx;
	pCtx->iGeneration = iGeneration;
	__xhttpTxAddRef(pTx);

	if ( pTx->pStream && pTx->pStream->pWorker ) iAffinity = pTx->pStream->pWorker->iId;
	else if ( pTx->pConn && pTx->pConn->pStream && pTx->pConn->pStream->pWorker ) iAffinity = pTx->pConn->pStream->pWorker->iId;

	if ( xrtNetEnginePostDelayed(pTx->pEngine, iAffinity, iTimeoutMs, __xhttpIdleTimeoutTask, pCtx) != XRT_NET_OK ) {
		__xhttpTxRelease(pTx);
		XNET_FREE(pCtx);
	}
}


// 内部函数：连接 resolve 超时毫秒
static uint32 __xhttpResolveConnectTimeoutMs(const xhttprequest* pReq)
{
	if ( !pReq ) return 0u;
	if ( pReq->iTimeoutMs > 0u ) return pReq->iTimeoutMs;
	return pReq->iIdleTimeoutMs;
}


// 内部函数：__xhttpConnSendActiveTx
static bool __xhttpConnSendActiveTx(__xhttp_conn* pConn)
{
	__xhttp_tx* pTx;
	if ( !pConn || !pConn->pStream ) return false;
	pTx = pConn->pTx;
	if ( !pTx || !pTx->pSendBuf || pTx->iSendLen == 0u ) return false;
	if ( xrtNetStreamSend(pConn->pStream, pTx->pSendBuf, pTx->iSendLen) != XRT_NET_OK ) {
		(void)__xhttpTxComplete(pTx, XRT_NET_ERROR, NULL);
		xrtNetStreamClose(pConn->pStream, XNET_CLOSE_F_ABORT);
		return false;
	}
	__xhttpTxRefreshIdleTimeout(pTx);
	return true;
}


// 内部函数：__xhttpTxTimeoutTask
static void __xhttpTxTimeoutTask(xnetworker* pWorker, ptr pArg)
{
	__xhttp_tx* pTx = (__xhttp_tx*)pArg;
	(void)pWorker;
	if ( !pTx ) return;
	if ( __xhttpAtomicLoad(&pTx->iComplete) == 0 ) {
		(void)__xhttpTxComplete(pTx, XRT_NET_TIMEOUT, NULL);
		__xhttpTxAbortStream(pTx);
	}
	__xhttpTxRelease(pTx);
}


// 内部函数：__xhttpClientOnOpen
static void __xhttpClientOnOpen(ptr pOwner, xnetstream* pStream)
{
	__xhttp_conn* pConn = (__xhttp_conn*)pOwner;
	if ( !pConn || !pStream ) return;
	pConn->bOpen = true;
	(void)__xhttpConnSendActiveTx(pConn);
}


// 内部函数：__xhttpClientOnRecv
static void __xhttpClientOnRecv(ptr pOwner, xnetstream* pStream, xnetchain* pChain)
{
	__xhttp_conn* pConn = (__xhttp_conn*)pOwner;
	__xhttp_tx* pTx = pConn ? pConn->pTx : NULL;
	xcodecframe tFrame;
	xcodechttp1msg tMsg;
	xcodecstatus iParse;
	bool bNoBodyExpected;
	bool bReusable;
	xhttpresponse* pResp;
	if ( !pTx || !pStream || !pChain ) return;
	__xhttpTxRefreshIdleTimeout(pTx);
	iParse = xrtCodecHttp1Parse(pChain, &tFrame, &tMsg);
	if ( iParse == XCODEC_STATUS_NEED_MORE ) return;
	if ( iParse == XCODEC_STATUS_ERROR ) {
		(void)__xhttpTxComplete(pTx, XRT_NET_ERROR, NULL);
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return;
	}
	bNoBodyExpected = __xhttpStatusHasNoBody(tMsg.iStatusCode) || __xhttpStrEqNoCase(pTx->tReq.sMethod, "HEAD");
	if ( (tMsg.iFlags & XCODEC_HTTP1_F_CHUNKED) == 0u && (tMsg.iContentLength < 0 && !bNoBodyExpected) ) {
		(void)__xhttpTxComplete(pTx, XRT_NET_ERROR, NULL);
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return;
	}
	pResp = __xhttpBuildResponse(&tFrame, &tMsg, pChain);
	if ( !pResp ) {
		(void)__xhttpTxComplete(pTx, XRT_NET_ERROR, NULL);
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return;
	}
	xrtCodecFrameConsume(pChain, &tFrame);
	bReusable = __xhttpResponseReusable(pTx, &tMsg);
	if ( bReusable && pConn ) {
		pConn->pTx = NULL;
		pTx->pConn = NULL;
		pTx->pStream = NULL;
		(void)__xhttpPoolPut(pConn);
		(void)__xhttpTxComplete(pTx, XRT_NET_OK, pResp);
		__xhttpTxPostCleanup(pTx);
		return;
	}
	(void)__xhttpTxComplete(pTx, XRT_NET_OK, pResp);
	xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
}


// 内部函数：__xhttpClientOnClose
static void __xhttpClientOnClose(ptr pOwner, xnetstream* pStream, xnet_result iReason)
{
	__xhttp_conn* pConn = (__xhttp_conn*)pOwner;
	__xhttp_tx* pTx = pConn ? pConn->pTx : NULL;
	(void)pStream;
	if ( !pConn ) return;
	pConn->bOpen = false;
	if ( pConn->bIdle ) __xhttpPoolRemove(pConn);
	pConn->bIdle = false;
	pConn->pStream = NULL;
	pConn->pTx = NULL;
	if ( pTx ) {
		pTx->pConn = NULL;
		pTx->pStream = NULL;
	}
	if ( pTx && __xhttpAtomicLoad(&pTx->iComplete) == 0 ) {
		(void)__xhttpTxComplete(pTx, iReason == XRT_NET_CLOSED ? XRT_NET_CLOSED : XRT_NET_ERROR, NULL);
	}
	if ( pTx ) __xhttpTxPostCleanup(pTx);
	__xhttpConnPostCleanup(pConn);
}


// 内部函数：__xhttpClientOnError
static void __xhttpClientOnError(ptr pOwner, xnetstream* pStream, int iSysErr)
{
	__xhttp_conn* pConn = (__xhttp_conn*)pOwner;
	__xhttp_tx* pTx = pConn ? pConn->pTx : NULL;
	(void)pStream;
	if ( pConn ) pConn->iLastSysErr = iSysErr;
	if ( pTx ) pTx->iLastSysErr = iSysErr;
}


// 内部函数：__xhttpClientEvents
static const xnetstreamevents* __xhttpClientEvents(void)
{
	static const xnetstreamevents tEvents = {
		__xhttpClientOnOpen,
		__xhttpClientOnRecv,
		NULL,
		__xhttpClientOnClose,
		__xhttpClientOnError,
		NULL,
		NULL
	};
	return &tEvents;
}


// xrtHttpExecuteAsync 相关处理
XXAPI xnetfuture* xrtHttpExecuteAsync(xnetengine* pEngine, const xhttprequest* pReq)
{
	__xhttp_tx* pTx;
	__xhttp_conn* pConn = NULL;
	xnetfuture* pFuture;
	xnetconnectconfig tConnCfg;
	xnetengine* pResolvedEngine;

	if ( !pReq ) return NULL;
	pResolvedEngine = __xnetSyncResolveEngine(pEngine);
	if ( !pResolvedEngine ) return NULL;
	pFuture = xrtNetFutureCreate();
	if ( !pFuture ) return NULL;

	pTx = (__xhttp_tx*)XNET_ALLOC(sizeof(__xhttp_tx));
	if ( !pTx ) {
		xrtNetFutureDestroy(pFuture);
		return NULL;
	}
	memset(pTx, 0, sizeof(__xhttp_tx));
	pTx->iRefCount = 1;
	pTx->pEngine = pResolvedEngine;
	pTx->pFuture = pFuture;
	if ( !__xhttpRequestClone(&pTx->tReq, pReq) ) {
		(void)__xnetFutureResolve(pFuture, XRT_NET_ERROR, NULL);
		__xhttpTxRelease(pTx);
		return pFuture;
	}
	if ( pTx->tReq.tURL.sHost[0] == '\0' ) {
	if ( pTx->tReq.sURL[0] == '\0' || !xrtUrlParseFixedTo(pTx->tReq.sURL, "http", "https", &pTx->tReq.tURL.bHttps, pTx->tReq.tURL.sHost, sizeof(pTx->tReq.tURL.sHost), &pTx->tReq.tURL.iPort, pTx->tReq.tURL.sPath, sizeof(pTx->tReq.tURL.sPath)) ) {
			(void)__xnetFutureResolve(pFuture, XRT_NET_ERROR, NULL);
			__xhttpTxRelease(pTx);
			return pFuture;
		}
	}
	if ( !__xhttpBuildRequestBytes(&pTx->tReq, &pTx->pSendBuf, &pTx->iSendLen) ) {
		(void)__xnetFutureResolve(pFuture, XRT_NET_ERROR, NULL);
		__xhttpTxRelease(pTx);
		return pFuture;
	}

	pConn = __xhttpPoolTake(pResolvedEngine, &pTx->tReq);
	if ( !pConn ) {
		pConn = (__xhttp_conn*)XNET_ALLOC(sizeof(__xhttp_conn));
		if ( !pConn ) {
			(void)__xnetFutureResolve(pFuture, XRT_NET_ERROR, NULL);
			__xhttpTxRelease(pTx);
			return pFuture;
		}
		memset(pConn, 0, sizeof(__xhttp_conn));
		pConn->pEngine = pResolvedEngine;
		__xhttpCopyToken(pConn->sHost, sizeof(pConn->sHost), pTx->tReq.tURL.sHost);
		pConn->iPort = pTx->tReq.tURL.iPort;
		pConn->bHttps = pTx->tReq.tURL.bHttps;
		pConn->bVerifyPeer = pTx->tReq.bVerifyPeer;
		pConn->pProxy = pTx->tReq.pProxy ? xrtNetProxyAddRef(pTx->tReq.pProxy) : NULL;
		pConn->pStream = xrtNetStreamCreate(pResolvedEngine, __xhttpClientEvents(), pConn);
		if ( !pConn->pStream ) {
			(void)__xnetFutureResolve(pFuture, XRT_NET_ERROR, NULL);
			if ( pConn->pProxy ) {
				xrtNetProxyRelease(pConn->pProxy);
				pConn->pProxy = NULL;
			}
			XNET_FREE(pConn);
			__xhttpTxRelease(pTx);
			return pFuture;
		}
	}

	pConn->pTx = pTx;
	pTx->pConn = pConn;
	pTx->pStream = pConn->pStream;

	if ( !pConn->bOpen ) {
		xrtNetConnectConfigInit(&tConnCfg);
		tConnCfg.sHost = pTx->tReq.tURL.sHost;
		tConnCfg.iPort = pTx->tReq.tURL.iPort;
		tConnCfg.iConnectTimeoutMs = __xhttpResolveConnectTimeoutMs(&pTx->tReq);
		tConnCfg.iRecvLimit = 1024u * 1024u;
		tConnCfg.pProxy = pConn->pProxy;
		if ( pTx->tReq.tURL.bHttps ) {
			memset(&pConn->tTlsCfg, 0, sizeof(pConn->tTlsCfg));
			pConn->tTlsCfg.sHostName = pTx->tReq.tURL.sHost;
			pConn->tTlsCfg.bVerifyPeer = pTx->tReq.bVerifyPeer;
			tConnCfg.pTlsConfig = &pConn->tTlsCfg;
		}
		if ( xrtNetStreamConnect(pConn->pStream, &tConnCfg) != XRT_NET_OK ) {
			(void)__xnetFutureResolve(pFuture, XRT_NET_ERROR, NULL);
			xrtNetStreamDestroy(pConn->pStream);
			pConn->pStream = NULL;
			pConn->pTx = NULL;
			pTx->pConn = NULL;
			pTx->pStream = NULL;
			if ( pConn->pProxy ) {
				xrtNetProxyRelease(pConn->pProxy);
				pConn->pProxy = NULL;
			}
			XNET_FREE(pConn);
			__xhttpTxRelease(pTx);
			return pFuture;
		}
	} else if ( !__xhttpConnSendActiveTx(pConn) ) {
		return pFuture;
	}

	if ( pTx->tReq.iTimeoutMs > 0 ) {
		__xhttpTxAddRef(pTx);
		if ( xrtNetEnginePostDelayed(pResolvedEngine, pTx->pStream && pTx->pStream->pWorker ? pTx->pStream->pWorker->iId : 0, pTx->tReq.iTimeoutMs, __xhttpTxTimeoutTask, pTx) != XRT_NET_OK ) {
			__xhttpTxRelease(pTx);
		}
	}
	return pFuture;
}


// xrtHttpExecuteSync 相关处理
XXAPI xhttpresponse* xrtHttpExecuteSync(xnetengine* pEngine, const xhttprequest* pReq, xnet_result* pStatus)
{
	xnetfuture* pFuture;
	xnet_result iStatus;
	xhttpresponse* pResp;
	if ( pStatus ) *pStatus = XRT_NET_ERROR;
	pFuture = xrtHttpExecuteAsync(pEngine, pReq);
	if ( !pFuture ) return NULL;
	iStatus = xrtNetFutureWait(pFuture, XNET_WAIT_INFINITE);
	pResp = (iStatus == XRT_NET_OK) ? (xhttpresponse*)xrtNetFutureValue(pFuture) : NULL;
	if ( pStatus ) *pStatus = iStatus;
	xrtNetFutureDestroy(pFuture);
	return pResp;
}

#endif
