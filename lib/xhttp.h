#ifndef XRT_XHTTP_H
#define XRT_XHTTP_H

/*
	XRT mainline HTTP/1.1 client on top of xnet.

	This header provides:
	  - request and response objects
	  - async HTTP/1.1 transactions over plain TCP or builtin TLS
	  - sync facades that reuse the same async transport core
	  - serial keep-alive connection reuse for the same origin
	  - incremental response headers and body delivery

	Current limitations:
	  - request bodies are currently buffered before transport
	  - chunked transfer is supported, but trailers remain metadata-only
	  - HTTP/2 is intentionally out of scope
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

typedef struct xrt_http_client xhttpclient;

typedef struct xrt_http_header {
	char sName[XHTTP_HEADER_NAME_CAP];
	char sValue[XHTTP_HEADER_VALUE_CAP];
} xhttpheader;

typedef struct xrt_http_url {
	bool bHttps;
	uint16 iPort;
	char sHost[XHTTP_HOST_CAP];
	char sPath[XHTTP_PATH_CAP];
} xhttpurl;

typedef struct xrt_http_request {
	char sMethod[XHTTP_METHOD_CAP];
	char sURL[XHTTP_URL_CAP];
	xhttpurl tURL;
	xhttpheader arrHeaders[XHTTP_MAX_HEADERS];
	xhttpheader* pHeaders;
	uint32 iHeaderCount;
	uint32 iHeaderCap;
	char* pBody;
	size_t iBodyLen;
	uint32 iTimeoutMs;
	uint32 iIdleTimeoutMs;
	bool bVerifyPeer;
	xnetproxy* pProxy;
} xhttprequest;

typedef struct xrt_http_response {
	uint32 iStatusCode;
	uint32 iFlags;
	uint32 iHeaderCount;
	uint32 iHeaderCap;
	int64_t iContentLength;
	char sVersion[XCODEC_HTTP1_TOKEN_CAP];
	char sReason[XCODEC_HTTP1_REASON_CAP];
	xhttpheader arrHeaders[XHTTP_MAX_HEADERS];
	xhttpheader* pHeaders;
	char* pBody;
	size_t iBodyLen;
} xhttpresponse;

typedef bool (*xhttpstreamheadersfn)(ptr pUserData, const xhttpresponse* pResponse);
typedef bool (*xhttpstreambodyfn)(ptr pUserData, const void* pData, size_t iLen);

typedef struct xrt_http_stream_callbacks {
	ptr pUserData;
	xhttpstreamheadersfn OnHeaders;
	xhttpstreambodyfn OnBody;
} xhttpstreamcallbacks;
#endif

#define __XHTTP_STREAM_BODY_NONE     0u
#define __XHTTP_STREAM_BODY_FIXED    1u
#define __XHTTP_STREAM_BODY_CHUNKED  2u
#define __XHTTP_STREAM_BODY_CLOSE    3u

#define __XHTTP_STREAM_CHUNK_SIZE       0u
#define __XHTTP_STREAM_CHUNK_DATA       1u
#define __XHTTP_STREAM_CHUNK_DATA_CRLF  2u
#define __XHTTP_STREAM_CHUNK_TRAILER    3u

struct __xhttp_conn;

struct xrt_http_client {
	volatile long iRefCount;
	volatile long iClosing;
	volatile long iPoolLock;
	xnetengine* pEngine;
	struct __xhttp_conn* pIdleConnHead;
};

typedef struct {
	volatile long iRefCount;
	volatile long iCleanupPosted;
	volatile long iComplete;
	volatile long iIdleTimerGen;
	xnetengine* pEngine;
	struct __xhttp_conn* pConn;
	xnetstream* pStream;
	xnetfuture* pFuture;
	xhttpclient* pClient;
	xhttprequest tReq;
	xhttpstreamcallbacks tStreamCallbacks;
	xhttpresponse* pStreamResp;
	char* pSendBuf;
	size_t iSendLen;
	uint64 iBodyRemain;
	uint64 iChunkRemain;
	uint64 iBodyReceived;
	uint32 iStreamBodyMode;
	uint32 iStreamChunkState;
	bool bStreaming;
	bool bStreamHeadersDelivered;
	int iLastSysErr;
	xtlsconfig tTlsCfg;
} __xhttp_tx;

typedef struct {
	__xhttp_tx* pTx;
} __xhttp_cancel_ctx;

typedef struct {
	__xhttp_tx* pTx;
	long iGeneration;
} __xhttp_idle_timer_ctx;

typedef struct __xhttp_conn {
	struct __xhttp_conn* pNext;
	volatile long iCleanupPosted;
	xnetengine* pEngine;
	xhttpclient* pClient;
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
	if ( ch >= 'A' && ch <= 'Z' ) { return (char)(ch + 32); }
	return ch;
}


// 内部函数：字符串相等忽略大小写相关处理
static bool __xhttpStrEqNoCase(const char* sA, const char* sB)
{
	size_t i = 0;
	if ( !sA || !sB ) { return false; }
	while ( sA[i] && sB[i] ) {
		if ( __xhttpToLower(sA[i]) != __xhttpToLower(sB[i]) ) { return false; }
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
	if ( !sDst || iDstCap == 0 ) { return; }
	if ( !sSrc ) {
		sDst[0] = '\0';
		return;
	}
	iLen = strlen(__xrt_cstr(sSrc));
	if ( iLen >= iDstCap ) { iLen = iDstCap - 1u; }
	memcpy(sDst, sSrc, iLen);
	sDst[iLen] = '\0';
}


// 内部函数：__xhttpAppendBytes
static bool __xhttpAppendBytes(char** ppBuf, size_t* pLen, size_t* pCap, const void* pData, size_t iDataLen)
{
	size_t iNeedCap;
	size_t iTargetCap;
	char* pNewBuf;
	// 参数校验
	if ( !ppBuf || !pLen || !pCap || (!pData && iDataLen != 0) ) { return false; }
	if ( iDataLen == 0 ) { return true; }
	// 计算目标容量，检查溢出
	iTargetCap = *pLen + iDataLen + 1u;
	if ( iTargetCap < *pLen || iTargetCap < iDataLen ) { return false; }
	// 容量足够，直接追加数据
	if ( iTargetCap <= *pCap ) {
		memcpy(*ppBuf + *pLen, pData, iDataLen);
		*pLen += iDataLen;
		(*ppBuf)[*pLen] = '\0';
		return true;
	}
	// 容量不足，按倍增策略扩容
	iNeedCap = (*pCap == 0) ? 256u : *pCap;
	while ( iNeedCap < iTargetCap ) {
		if ( iNeedCap > (SIZE_MAX >> 1) ) {
			iNeedCap = iTargetCap;
			break;
		}
		iNeedCap *= 2u;
	}
	if ( iNeedCap < iTargetCap ) { return false; }
	// 分配新缓冲区并拷贝旧数据
	pNewBuf = (char*)XNET_ALLOC(iNeedCap);
	if ( !pNewBuf ) { return false; }
	if ( *ppBuf && *pLen > 0 ) { memcpy(pNewBuf, *ppBuf, *pLen); }
	XNET_FREE(*ppBuf);
	*ppBuf = pNewBuf;
	*pCap = iNeedCap;
	// 追加新数据
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


static xhttpheader* __xhttpRequestHeaders(const xhttprequest* pReq)
{
	if ( !pReq ) { return NULL; }
	return pReq->pHeaders ? pReq->pHeaders : (xhttpheader*)pReq->arrHeaders;
}


static xhttpheader* __xhttpResponseHeaders(const xhttpresponse* pResp)
{
	if ( !pResp ) { return NULL; }
	return pResp->pHeaders ? pResp->pHeaders : (xhttpheader*)pResp->arrHeaders;
}


static bool __xhttpEnsureRequestHeaderCap(xhttprequest* pReq, uint32 iNeed)
{
	xhttpheader* pNew;
	uint32 iNewCap;
	if ( !pReq ) { return false; }
	if ( pReq->pHeaders == NULL ) {
		pReq->pHeaders = pReq->arrHeaders;
		pReq->iHeaderCap = XHTTP_MAX_HEADERS;
	}
	if ( iNeed <= pReq->iHeaderCap ) { return true; }
	iNewCap = pReq->iHeaderCap ? pReq->iHeaderCap : XHTTP_MAX_HEADERS;
	while ( iNewCap < iNeed ) {
		if ( iNewCap > UINT32_MAX / 2u ) { return false; }
		iNewCap *= 2u;
	}
	pNew = (xhttpheader*)XNET_ALLOC(sizeof(xhttpheader) * iNewCap);
	if ( !pNew ) { return false; }
	memset(pNew, 0, sizeof(xhttpheader) * iNewCap);
	if ( pReq->iHeaderCount > 0u ) { memcpy(pNew, pReq->pHeaders, sizeof(xhttpheader) * pReq->iHeaderCount); }
	if ( pReq->pHeaders != pReq->arrHeaders ) { XNET_FREE(pReq->pHeaders); }
	pReq->pHeaders = pNew;
	pReq->iHeaderCap = iNewCap;
	return true;
}


static bool __xhttpEnsureResponseHeaderCap(xhttpresponse* pResp, uint32 iNeed)
{
	xhttpheader* pNew;
	uint32 iNewCap;
	if ( !pResp ) { return false; }
	if ( pResp->pHeaders == NULL ) {
		pResp->pHeaders = pResp->arrHeaders;
		pResp->iHeaderCap = XHTTP_MAX_HEADERS;
	}
	if ( iNeed <= pResp->iHeaderCap ) { return true; }
	iNewCap = pResp->iHeaderCap ? pResp->iHeaderCap : XHTTP_MAX_HEADERS;
	while ( iNewCap < iNeed ) {
		if ( iNewCap > UINT32_MAX / 2u ) { return false; }
		iNewCap *= 2u;
	}
	pNew = (xhttpheader*)XNET_ALLOC(sizeof(xhttpheader) * iNewCap);
	if ( !pNew ) { return false; }
	memset(pNew, 0, sizeof(xhttpheader) * iNewCap);
	if ( pResp->iHeaderCount > 0u ) { memcpy(pNew, pResp->pHeaders, sizeof(xhttpheader) * pResp->iHeaderCount); }
	if ( pResp->pHeaders != pResp->arrHeaders ) { XNET_FREE(pResp->pHeaders); }
	pResp->pHeaders = pNew;
	pResp->iHeaderCap = iNewCap;
	return true;
}


// 内部函数：__xhttpRequestHasHeader
static bool __xhttpRequestHasHeader(const xhttprequest* pReq, const char* sName)
{
	xhttpheader* pHeaders;
	if ( !pReq || !sName ) { return false; }
	pHeaders = __xhttpRequestHeaders(pReq);
	for ( uint32 i = 0; i < pReq->iHeaderCount; ++i ) {
		if ( __xhttpStrEqNoCase(pHeaders[i].sName, sName) ) { return true; }
	}
	return false;
}


// 内部函数：获取 request 头部值
static const char* __xhttpRequestHeaderValue(const xhttprequest* pReq, const char* sName)
{
	xhttpheader* pHeaders;
	if ( !pReq || !sName ) { return NULL; }
	pHeaders = __xhttpRequestHeaders(pReq);
	for ( uint32 i = 0; i < pReq->iHeaderCount; ++i ) {
		if ( __xhttpStrEqNoCase(pHeaders[i].sName, sName) ) { return pHeaders[i].sValue; }
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
static xhttpresponse* __xhttpBuildHeadResponse(const xnetchain* pChain, xcodecframe* pFrame, xcodechttp1msg* pMsg, bool* pHadHead);
static void __xhttpConnPostCleanup(__xhttp_conn* pConn);
static void __xhttpTxRefreshIdleTimeout(__xhttp_tx* pTx);
static uint32 __xhttpResolveConnectTimeoutMs(const xhttprequest* pReq);


// 内部函数：__xhttpPoolLockAcquire
static xhttpclient* __xhttpClientAddRef(xhttpclient* pClient)
{
	if ( pClient ) { (void)__xhttpAtomicAdd(&pClient->iRefCount, 1); }
	return pClient;
}


static void __xhttpClientRelease(xhttpclient* pClient)
{
	if ( !pClient ) { return; }
	if ( __xhttpAtomicAdd(&pClient->iRefCount, -1) == 0 ) { XNET_FREE(pClient); }
}


static void __xhttpPoolLockAcquire(xhttpclient* pClient)
{
	volatile long* pLock = pClient ? &pClient->iPoolLock : &__g_xhttpPoolLock;
	while ( __xnetAtomicCompareExchange32(pLock, 1, 0) != 0 ) {
		__xnetEngineSleepMs(1u);
	}
}


// 内部函数：__xhttpPoolLockRelease
static void __xhttpPoolLockRelease(xhttpclient* pClient)
{
	volatile long* pLock = pClient ? &pClient->iPoolLock : &__g_xhttpPoolLock;
	(void)__xnetAtomicExchange32(pLock, 0);
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
	if ( !pReq || !sOut || iOutCap == 0u ) { return false; }
	if ( pReq->sURL[0] != '\0' && xrtUrlParseView(pReq->sURL, &tURL) ) {
		return xrtUrlMakeHostHeader(&tURL, sOut, iOutCap);
	}
	if ( pReq->tURL.sHost[0] == '\0' ) { return false; }
	return xrtUrlMakeHostHeaderFixed(pReq->tURL.bHttps ? "https" : "http", pReq->tURL.sHost, pReq->tURL.iPort, sOut, iOutCap);
}


// 内部函数：__xhttpRequestWantsClose
static bool __xhttpRequestWantsClose(const xhttprequest* pReq)
{
	xhttpheader* pHeaders;
	if ( !pReq ) { return false; }
	pHeaders = __xhttpRequestHeaders(pReq);
	for ( uint32 i = 0; i < pReq->iHeaderCount; ++i ) {
		if ( __xhttpStrEqNoCase(pHeaders[i].sName, "Connection") ) {
			return __xhttpContainsTokenNoCase(pHeaders[i].sValue, "close");
		}
	}
	return false;
}


// 内部函数：__xhttpResponseWantsClose
static bool __xhttpResponseWantsClose(const xcodechttp1msg* pMsg)
{
	const char* sConn;
	if ( !pMsg ) { return false; }
	sConn = xrtCodecHttp1GetHeader(pMsg, "Connection");
	return sConn ? __xhttpContainsTokenNoCase(sConn, "close") : false;
}


// 内部函数：__xhttpConnMatchesReq
static bool __xhttpConnMatchesReq(const __xhttp_conn* pConn, xnetengine* pEngine, const xhttprequest* pReq)
{
	if ( !pConn || !pReq || !pEngine ) { return false; }
	if ( pConn->pEngine != pEngine ) { return false; }
	if ( pConn->iPort != pReq->tURL.iPort ) { return false; }
	if ( pConn->bHttps != pReq->tURL.bHttps ) { return false; }
	if ( pConn->bVerifyPeer != pReq->bVerifyPeer ) { return false; }
	if ( pConn->pProxy != pReq->pProxy ) { return false; }
	if ( strcmp(pConn->sHost, pReq->tURL.sHost) != 0 ) { return false; }
	if ( !pConn->bOpen || pConn->pStream == NULL || pConn->pTx != NULL ) { return false; }
	return true;
}


// 内部函数：__xhttpPoolTake
static __xhttp_conn* __xhttpPoolTake(xhttpclient* pClient, xnetengine* pEngine, const xhttprequest* pReq)
{
	__xhttp_conn** ppConn;
	__xhttp_conn* pConn = NULL;
	if ( !pEngine || !pReq ) { return NULL; }
	__xhttpPoolLockAcquire(pClient);
	ppConn = pClient ? &pClient->pIdleConnHead : &__g_xhttpIdleConnHead;
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
	__xhttpPoolLockRelease(pClient);
	return pConn;
}


// 内部函数：__xhttpPoolPut
static bool __xhttpPoolPut(__xhttp_conn* pConn)
{
	xhttpclient* pClient;
	__xhttp_conn** ppHead;
	if ( !pConn || !pConn->pStream || !pConn->bOpen || pConn->pTx != NULL ) { return false; }
	pClient = pConn->pClient;
	if ( pClient && __xhttpAtomicLoad(&pClient->iClosing) != 0 ) { return false; }
	__xhttpPoolLockAcquire(pClient);
	if ( pClient && __xhttpAtomicLoad(&pClient->iClosing) != 0 ) {
		__xhttpPoolLockRelease(pClient);
		return false;
	}
	ppHead = pClient ? &pClient->pIdleConnHead : &__g_xhttpIdleConnHead;
	pConn->pNext = *ppHead;
	*ppHead = pConn;
	pConn->bIdle = true;
	__xhttpPoolLockRelease(pClient);
	return true;
}


// 内部函数：删除内存池
static void __xhttpPoolRemove(__xhttp_conn* pConn)
{
	__xhttp_conn** ppCur;
	xhttpclient* pClient;
	if ( !pConn ) { return; }
	pClient = pConn->pClient;
	__xhttpPoolLockAcquire(pClient);
	ppCur = pClient ? &pClient->pIdleConnHead : &__g_xhttpIdleConnHead;
	while ( *ppCur ) {
		if ( *ppCur == pConn ) {
			*ppCur = pConn->pNext;
			pConn->pNext = NULL;
			pConn->bIdle = false;
			break;
		}
		ppCur = &(*ppCur)->pNext;
	}
	__xhttpPoolLockRelease(pClient);
}


// xrtHttpCloseIdleConnections 相关处理
static void __xhttpCloseIdleConnections(xhttpclient* pClient, xnetengine* pEngine)
{
	__xhttp_conn* pList = NULL;
	__xhttp_conn** ppTail = &pList;
	__xhttp_conn** ppCur;
	// 获取连接池锁，收集匹配的空闲连接
	__xhttpPoolLockAcquire(pClient);
	ppCur = pClient ? &pClient->pIdleConnHead : &__g_xhttpIdleConnHead;
	while ( *ppCur ) {
		__xhttp_conn* pConn = *ppCur;
		// 跳过不属于指定引擎的连接
		if ( pEngine != NULL && pConn->pEngine != pEngine ) {
			ppCur = &pConn->pNext;
			continue;
		}
		// 从空闲池中摘除并追加到本地列表
		*ppCur = pConn->pNext;
		pConn->pNext = NULL;
		pConn->bIdle = false;
		*ppTail = pConn;
		ppTail = &pConn->pNext;
	}
	__xhttpPoolLockRelease(pClient);
	// 逐个关闭并清理收集到的连接
	while ( pList ) {
		__xhttp_conn* pNext = pList->pNext;
		pList->pNext = NULL;
		if ( pList->pStream ) {
			xrtNetStreamClose(pList->pStream, XNET_CLOSE_F_ABORT);
		} else {
			__xhttpConnPostCleanup(pList);
		}
		pList = pNext;
	}
}


// xrtHttpRequestInit 相关处理
XXAPI void xrtHttpRequestInit(xhttprequest* pReq)
{
	if ( !pReq ) { return; }
	memset(pReq, 0, sizeof(xhttprequest));
	pReq->pHeaders = pReq->arrHeaders;
	pReq->iHeaderCap = XHTTP_MAX_HEADERS;
	strcpy(pReq->sMethod, "GET");
	pReq->iTimeoutMs = 30000u;
	pReq->iIdleTimeoutMs = 0u;
	pReq->bVerifyPeer = true;
}


// xrtHttpRequestCreate 相关处理
XXAPI xhttprequest* xrtHttpRequestCreate(void)
{
	xhttprequest* pReq = (xhttprequest*)XNET_ALLOC(sizeof(xhttprequest));
	if ( !pReq ) { return NULL; }
	xrtHttpRequestInit(pReq);
	return pReq;
}


// 内部函数：__xhttpRequestUnitInternal
static void __xhttpRequestUnitInternal(xhttprequest* pReq)
{
	if ( !pReq ) { return; }
	if ( pReq->pHeaders && pReq->pHeaders != pReq->arrHeaders ) {
		XNET_FREE(pReq->pHeaders);
		pReq->pHeaders = pReq->arrHeaders;
	}
	pReq->iHeaderCount = 0u;
	pReq->iHeaderCap = XHTTP_MAX_HEADERS;
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
	if ( !pReq ) { return; }
	__xhttpRequestUnitInternal(pReq);
	memset(pReq, 0, sizeof(xhttprequest));
}


// xrtHttpRequestDestroy 相关处理
XXAPI void xrtHttpRequestDestroy(xhttprequest* pReq)
{
	if ( !pReq ) { return; }
	xrtHttpRequestUnit(pReq);
	XNET_FREE(pReq);
}


// xrtHttpRequestSetMethod 相关处理
XXAPI bool xrtHttpRequestSetMethod(xhttprequest* pReq, const char* sMethod)
{
	size_t iLen;
	if ( !pReq || !sMethod || !sMethod[0] ) { return false; }
	iLen = strlen(sMethod);
	if ( iLen >= sizeof(pReq->sMethod) ) { iLen = sizeof(pReq->sMethod) - 1u; }
	memcpy(pReq->sMethod, sMethod, iLen);
	pReq->sMethod[iLen] = '\0';
	return true;
}


// 设置 HTTP request URL
XXAPI bool xrtHttpRequestSetURL(xhttprequest* pReq, const char* sURL)
{
	size_t iLen;
	if ( !pReq || !sURL || !sURL[0] ) { return false; }
	if ( !xrtUrlParseFixedTo(sURL, "http", "https", &pReq->tURL.bHttps, pReq->tURL.sHost, sizeof(pReq->tURL.sHost), &pReq->tURL.iPort, pReq->tURL.sPath, sizeof(pReq->tURL.sPath)) ) { return false; }
	iLen = strlen(sURL);
	if ( iLen >= sizeof(pReq->sURL) ) { iLen = sizeof(pReq->sURL) - 1u; }
	memcpy(pReq->sURL, sURL, iLen);
	pReq->sURL[iLen] = '\0';
	return true;
}


// 设置 HTTP request 头部
XXAPI bool xrtHttpRequestSetHeader(xhttprequest* pReq, const char* sName, const char* sValue)
{
	xhttpheader* pHeader;
	xhttpheader* pHeaders;
	if ( !pReq || !sName || !sValue ) { return false; }
	pHeaders = __xhttpRequestHeaders(pReq);
	for ( uint32 i = 0; i < pReq->iHeaderCount; ++i ) {
		if ( __xhttpStrEqNoCase(pHeaders[i].sName, sName) ) {
			__xhttpCopyToken(pHeaders[i].sValue, sizeof(pHeaders[i].sValue), sValue);
			return true;
		}
	}
	if ( !__xhttpEnsureRequestHeaderCap(pReq, pReq->iHeaderCount + 1u) ) { return false; }
	pHeader = &pReq->pHeaders[pReq->iHeaderCount++];
	__xhttpCopyToken(pHeader->sName, sizeof(pHeader->sName), sName);
	__xhttpCopyToken(pHeader->sValue, sizeof(pHeader->sValue), sValue);
	return true;
}


XXAPI bool xrtHttpRequestAddHeader(xhttprequest* pReq, const char* sName, const char* sValue)
{
	xhttpheader* pHeader;
	if ( !pReq || !sName || !sName[0] || !sValue ) { return false; }
	if ( !__xhttpEnsureRequestHeaderCap(pReq, pReq->iHeaderCount + 1u) ) { return false; }
	pHeader = &pReq->pHeaders[pReq->iHeaderCount++];
	memset(pHeader, 0, sizeof(xhttpheader));
	__xhttpCopyToken(pHeader->sName, sizeof(pHeader->sName), sName);
	__xhttpCopyToken(pHeader->sValue, sizeof(pHeader->sValue), sValue);
	return true;
}


XXAPI uint32 xrtHttpRequestRemoveHeader(xhttprequest* pReq, const char* sName)
{
	uint32 iRead;
	uint32 iWrite = 0u;
	uint32 iRemoved = 0u;
	if ( !pReq || !sName ) { return 0u; }
	if ( !__xhttpEnsureRequestHeaderCap(pReq, pReq->iHeaderCount) ) { return 0u; }
	for ( iRead = 0u; iRead < pReq->iHeaderCount; ++iRead ) {
		if ( __xhttpStrEqNoCase(pReq->pHeaders[iRead].sName, sName) ) {
			iRemoved++;
			continue;
		}
		if ( iWrite != iRead ) { pReq->pHeaders[iWrite] = pReq->pHeaders[iRead]; }
		iWrite++;
	}
	if ( iWrite < pReq->iHeaderCount ) {
		memset(pReq->pHeaders + iWrite, 0, sizeof(xhttpheader) * (pReq->iHeaderCount - iWrite));
	}
	pReq->iHeaderCount = iWrite;
	return iRemoved;
}


// xrtHttpRequestSetBodyCopy 相关处理
XXAPI bool xrtHttpRequestSetBodyCopy(xhttprequest* pReq, const void* pData, size_t iLen, const char* sContentType)
{
	char* pBodyCopy = NULL;
	if ( !pReq ) { return false; }
	if ( pReq->pBody ) {
		XNET_FREE(pReq->pBody);
		pReq->pBody = NULL;
		pReq->iBodyLen = 0;
	}
	if ( pData && iLen > 0 ) {
		pBodyCopy = (char*)XNET_ALLOC(iLen);
		if ( !pBodyCopy ) { return false; }
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
	if ( !pReq ) { return; }
	pReq->iTimeoutMs = iTimeoutMs;
}


// xrtHttpRequestSetIdleTimeout 相关处理
XXAPI void xrtHttpRequestSetIdleTimeout(xhttprequest* pReq, uint32 iTimeoutMs)
{
	if ( !pReq ) { return; }
	pReq->iIdleTimeoutMs = iTimeoutMs;
}


// xrtHttpRequestSetVerifyPeer 相关处理
XXAPI void xrtHttpRequestSetVerifyPeer(xhttprequest* pReq, bool bVerifyPeer)
{
	if ( !pReq ) { return; }
	pReq->bVerifyPeer = bVerifyPeer;
}


// xrtHttpResponseDestroy 相关处理
XXAPI void xrtHttpResponseDestroy(xhttpresponse* pResp)
{
	if ( !pResp ) { return; }
	if ( pResp->pHeaders && pResp->pHeaders != pResp->arrHeaders ) {
		XNET_FREE(pResp->pHeaders);
		pResp->pHeaders = NULL;
	}
	if ( pResp->pBody ) {
		XNET_FREE(pResp->pBody);
		pResp->pBody = NULL;
	}
	XNET_FREE(pResp);
}


// 获取 HTTP response 头部
XXAPI const char* xrtHttpResponseHeader(const xhttpresponse* pResp, const char* sName)
{
	xhttpheader* pHeaders;
	if ( !pResp || !sName ) { return NULL; }
	pHeaders = __xhttpResponseHeaders(pResp);
	for ( uint32 i = 0; i < pResp->iHeaderCount; ++i ) {
		if ( __xhttpStrEqNoCase(pHeaders[i].sName, sName) ) {
			return pHeaders[i].sValue;
		}
	}
	return NULL;
}


XXAPI uint32 xrtHttpResponseHeaderCount(const xhttpresponse* pResp)
{
	return pResp ? pResp->iHeaderCount : 0u;
}


XXAPI const char* xrtHttpResponseHeaderNameAt(const xhttpresponse* pResp, uint32 iIndex)
{
	xhttpheader* pHeaders = __xhttpResponseHeaders(pResp);
	return pHeaders && iIndex < pResp->iHeaderCount ? pHeaders[iIndex].sName : NULL;
}


XXAPI const char* xrtHttpResponseHeaderValueAt(const xhttpresponse* pResp, uint32 iIndex)
{
	xhttpheader* pHeaders = __xhttpResponseHeaders(pResp);
	return pHeaders && iIndex < pResp->iHeaderCount ? pHeaders[iIndex].sValue : NULL;
}


// xrtHttpResponseStatusCode 相关处理
XXAPI uint32 xrtHttpResponseStatusCode(const xhttpresponse* pResp)
{
	return pResp ? pResp->iStatusCode : 0u;
}


// xrtHttpResponseReason 相关处理
XXAPI const char* xrtHttpResponseReason(const xhttpresponse* pResp)
{
	return pResp ? pResp->sReason : "";
}


// xrtHttpResponseContentLength 相关处理
XXAPI int64_t xrtHttpResponseContentLength(const xhttpresponse* pResp)
{
	return pResp ? pResp->iContentLength : -1;
}


// xrtHttpResponseBody 相关处理
XXAPI const void* xrtHttpResponseBody(const xhttpresponse* pResp, size_t* pLen)
{
	if ( pLen ) { *pLen = 0u; }
	if ( !pResp ) { return NULL; }
	if ( pLen ) { *pLen = pResp->iBodyLen; }
	return pResp->pBody;
}


// xrtHttpResponseBodyTextCopy 相关处理
XXAPI char* xrtHttpResponseBodyTextCopy(const xhttpresponse* pResp)
{
	char* pCopy;
	size_t iLen;
	const char* pBody;

	iLen = pResp ? pResp->iBodyLen : 0u;
	pBody = (pResp && pResp->pBody) ? pResp->pBody : "";
	if ( iLen == SIZE_MAX ) { return NULL; }
	pCopy = (char*)XNET_ALLOC(iLen + 1u);
	if ( !pCopy ) { return NULL; }
	if ( iLen > 0u ) { memcpy(pCopy, pBody, iLen); }
	pCopy[iLen] = '\0';
	return pCopy;
}


// Initialize callbacks for incremental response delivery.
XXAPI void xrtHttpStreamCallbacksInit(xhttpstreamcallbacks* pCallbacks)
{
	if ( !pCallbacks ) { return; }
	memset(pCallbacks, 0, sizeof(xhttpstreamcallbacks));
}


// 内部函数：__xhttpRequestClone
static bool __xhttpRequestClone(xhttprequest* pDst, const xhttprequest* pSrc)
{
	xhttpheader* pSrcHeaders;
	if ( !pDst || !pSrc ) { return false; }
	xrtHttpRequestInit(pDst);
	memcpy(pDst->sMethod, pSrc->sMethod, sizeof(pDst->sMethod));
	memcpy(pDst->sURL, pSrc->sURL, sizeof(pDst->sURL));
	memcpy(&pDst->tURL, &pSrc->tURL, sizeof(pDst->tURL));
	pSrcHeaders = __xhttpRequestHeaders(pSrc);
	if ( !__xhttpEnsureRequestHeaderCap(pDst, pSrc->iHeaderCount) ) { return false; }
	if ( pSrc->iHeaderCount > 0u ) {
		memcpy(pDst->pHeaders, pSrcHeaders, sizeof(xhttpheader) * pSrc->iHeaderCount);
	}
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
	xhttpheader* pHeaders;
	size_t iLen = 0;
	size_t iCap = 0;
	char aLine[512];
	bool bChunked;

	// 参数校验并初始化输出
	if ( !pReq || !ppOut || !pOutLen || pReq->tURL.sHost[0] == '\0' || pReq->sMethod[0] == '\0' ) { return false; }
	*ppOut = NULL;
	*pOutLen = 0;
	pHeaders = __xhttpRequestHeaders(pReq);
	bChunked = __xhttpContainsTokenNoCase(__xhttpRequestHeaderValue(pReq, "Transfer-Encoding"), "chunked");

	// 构建请求行：METHOD PATH HTTP/1.1
	if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, pReq->sMethod) ) { goto fail; }
	if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, " ") ) { goto fail; }
	if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, pReq->tURL.sPath[0] ? pReq->tURL.sPath : "/") ) { goto fail; }
	if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, " HTTP/1.1\r\n") ) { goto fail; }

	// 补充 Host 头部
	if ( !__xhttpRequestHasHeader(pReq, "Host") ) {
		char sHostHeader[384];
		if ( !__xhttpMakeHostHeader(pReq, sHostHeader, sizeof(sHostHeader)) ) { goto fail; }
		snprintf(aLine, sizeof(aLine), "Host: %s\r\n", sHostHeader);
		if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, aLine) ) { goto fail; }
	}
	// 补充 Connection 头部
	if ( !__xhttpRequestHasHeader(pReq, "Connection") ) {
		if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, "Connection: keep-alive\r\n") ) { goto fail; }
	}
	// 补充 Content-Length 头部
	if ( !bChunked && pReq->iBodyLen > 0 && !__xhttpRequestHasHeader(pReq, "Content-Length") ) {
		snprintf(aLine, sizeof(aLine), "Content-Length: %llu\r\n", (unsigned long long)pReq->iBodyLen);
		if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, aLine) ) { goto fail; }
	}
	// 追加用户自定义头部
	for ( uint32 i = 0; i < pReq->iHeaderCount; ++i ) {
		if ( bChunked && __xhttpStrEqNoCase(pHeaders[i].sName, "Content-Length") ) { continue; }
		if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, pHeaders[i].sName) ||
			!__xhttpAppendText(&pBuf, &iLen, &iCap, ": ") ||
			!__xhttpAppendText(&pBuf, &iLen, &iCap, pHeaders[i].sValue) ||
			!__xhttpAppendText(&pBuf, &iLen, &iCap, "\r\n") ) { goto fail; }
	}
	// 空行分隔头部和正文
	if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, "\r\n") ) { goto fail; }
	// 处理请求正文
	if ( bChunked ) {
		snprintf(aLine, sizeof(aLine), "%llX\r\n", (unsigned long long)pReq->iBodyLen);
		if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, aLine) ) { goto fail; }
		if ( pReq->pBody && pReq->iBodyLen > 0 ) {
			if ( !__xhttpAppendBytes(&pBuf, &iLen, &iCap, pReq->pBody, pReq->iBodyLen) ) { goto fail; }
		}
		if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, "\r\n0\r\n\r\n") ) { goto fail; }
	} else if ( pReq->pBody && pReq->iBodyLen > 0 ) {
		if ( !__xhttpAppendBytes(&pBuf, &iLen, &iCap, pReq->pBody, pReq->iBodyLen) ) { goto fail; }
	}

	*ppOut = pBuf;
	*pOutLen = iLen;
	return true;

fail:
	if ( pBuf ) { XNET_FREE(pBuf); }
	return false;
}


// 内部函数：__xhttpBuildResponse
static xhttpresponse* __xhttpBuildResponse(const xcodecframe* pFrame, const xcodechttp1msg* pMsg, const xnetchain* pChain)
{
	xhttpresponse* pResp;
	size_t iBodyBytes;
	const xcodechttp1header* pMsgHeaders;
	// 参数校验
	if ( !pFrame || !pMsg || !pChain ) { return NULL; }
	// 分配响应结构体
	pResp = (xhttpresponse*)XNET_ALLOC(sizeof(xhttpresponse));
	if ( !pResp ) { return NULL; }
	memset(pResp, 0, sizeof(xhttpresponse));
	pResp->pHeaders = pResp->arrHeaders;
	pResp->iHeaderCap = XHTTP_MAX_HEADERS;
	// 复制状态码、内容长度、版本号、原因短语
	pResp->iStatusCode = pMsg->iStatusCode;
	pResp->iContentLength = pMsg->iContentLength;
	__xhttpCopyToken(pResp->sVersion, sizeof(pResp->sVersion), pMsg->sVersion);
	__xhttpCopyToken(pResp->sReason, sizeof(pResp->sReason), pMsg->sReason);
	// 映射编解码器标志到响应标志
	if ( pMsg->iFlags & XCODEC_HTTP1_F_CHUNKED ) { pResp->iFlags |= XHTTP_RESP_F_CHUNKED; }
	if ( pMsg->iFlags & XCODEC_HTTP1_F_KEEPALIVE ) { pResp->iFlags |= XHTTP_RESP_F_KEEPALIVE; }
	if ( pMsg->iFlags & XCODEC_HTTP1_F_UPGRADE ) { pResp->iFlags |= XHTTP_RESP_F_UPGRADE; }
	// 复制响应头部
	pMsgHeaders = pMsg->pHeaders ? pMsg->pHeaders : pMsg->arrHeaders;
	if ( !__xhttpEnsureResponseHeaderCap(pResp, pMsg->iHeaderCount) ) {
		xrtHttpResponseDestroy(pResp);
		return NULL;
	}
	pResp->iHeaderCount = pMsg->iHeaderCount;
	for ( uint32 i = 0; i < pResp->iHeaderCount; ++i ) {
		__xhttpCopyToken(pResp->pHeaders[i].sName, sizeof(pResp->pHeaders[i].sName), pMsgHeaders[i].sName);
		__xhttpCopyToken(pResp->pHeaders[i].sValue, sizeof(pResp->pHeaders[i].sValue), pMsgHeaders[i].sValue);
	}
	// 提取并复制响应正文
	iBodyBytes = xrtCodecHttp1BodyBytes(pFrame);
	if ( (pMsg->iFlags & XCODEC_HTTP1_F_CHUNKED) != 0u ) { pResp->iContentLength = (int64_t)iBodyBytes; }
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


// HEAD responses are complete when headers are complete; the body is always empty.
static xhttpresponse* __xhttpBuildHeadResponse(const xnetchain* pChain, xcodecframe* pFrame, xcodechttp1msg* pMsg, bool* pHadHead)
{
	static const uint8 aHeadEnd[] = { '\r', '\n', '\r', '\n' };
	char* sHeadBuf = NULL;
	size_t iHeadEndPos;
	size_t iHeadBytes;
	char* sLineEnd;
	char* sCursor;
	char* sNextLine;
	xhttpresponse* pResp;
	if ( pHadHead ) { *pHadHead = false; }
	if ( !pChain || !pFrame || !pMsg ) { return NULL; }
	iHeadEndPos = __xcodecChainFindPattern(pChain, aHeadEnd, sizeof(aHeadEnd), 0);
	if ( iHeadEndPos == (size_t)-1 ) { return NULL; }
	if ( pHadHead ) { *pHadHead = true; }
	iHeadBytes = iHeadEndPos + sizeof(aHeadEnd);
	xrtCodecFrameInit(pFrame);
	xrtCodecHttp1MessageInit(pMsg);
	sHeadBuf = (char*)XNET_ALLOC(iHeadBytes + 1u);
	if ( !sHeadBuf ) { goto fail; }
	if ( __xcodecChainPeekAt(pChain, 0, sHeadBuf, iHeadBytes) != iHeadBytes ) { goto fail; }
	sHeadBuf[iHeadBytes] = '\0';
	sLineEnd = strstr(sHeadBuf, "\r\n");
	if ( !sLineEnd ) { goto fail; }
	*sLineEnd = '\0';
	{
		char* sVersion = sHeadBuf;
		char* sStatus = strchr(sVersion, ' ');
		char* sReason = NULL;
		char* pStatusEnd = NULL;
		long iStatusCode;
		if ( !sStatus ) { goto fail; }
		*sStatus++ = '\0';
		while ( *sStatus == ' ' ) { sStatus++; }
		sReason = strchr(sStatus, ' ');
		if ( sReason ) {
			*sReason++ = '\0';
			while ( *sReason == ' ' ) { sReason++; }
		}
		iStatusCode = strtol(sStatus, &pStatusEnd, 10);
		if ( pStatusEnd == sStatus || *pStatusEnd != '\0' || iStatusCode < 100 || iStatusCode > 999 ) { goto fail; }
		pMsg->iStatusCode = (uint32)iStatusCode;
		pMsg->iFlags |= XCODEC_HTTP1_F_RESPONSE;
		pFrame->iFlags |= XCODEC_FRAME_F_RESPONSE;
		__xcodecHttpCopyToken(pMsg->sVersion, sizeof(pMsg->sVersion), sVersion, strlen(sVersion));
		if ( sReason ) { __xcodecHttpCopyToken(pMsg->sReason, sizeof(pMsg->sReason), sReason, strlen(sReason)); }
	}
	sCursor = sLineEnd + 2;
	while ( sCursor < (sHeadBuf + iHeadBytes - 2u) ) {
		char* sColon;
		const char* sName;
		const char* sValue;
		size_t iNameLen;
		size_t iValueLen;
		if ( sCursor[0] == '\r' && sCursor[1] == '\n' ) { break; }
		sNextLine = strstr(sCursor, "\r\n");
		if ( !sNextLine ) { break; }
		*sNextLine = '\0';
		sColon = strchr(sCursor, ':');
		if ( !sColon ) { goto fail; }
		*sColon = '\0';
		sName = sCursor;
		sValue = sColon + 1;
		iNameLen = strlen(__xrt_cstr(sName));
		iValueLen = strlen(sValue);
		__xcodecHttpTrimView(&sName, &iNameLen);
		__xcodecHttpTrimView(&sValue, &iValueLen);
		if ( iNameLen == 0u || iNameLen >= XCODEC_HTTP1_TOKEN_CAP || iValueLen >= XCODEC_HTTP1_VALUE_CAP ) { goto fail; }
		if ( !__xcodecHttp1EnsureHeaderCap(pMsg, pMsg->iHeaderCount + 1u) ) { goto fail; }
		{
			xcodechttp1header* pHeader = &pMsg->pHeaders[pMsg->iHeaderCount++];
			__xcodecHttpCopyToken(pHeader->sName, sizeof(pHeader->sName), sName, iNameLen);
			__xcodecHttpCopyToken(pHeader->sValue, sizeof(pHeader->sValue), sValue, iValueLen);
			if ( __xcodecHttpStrEqNoCase(pHeader->sName, "Content-Length") ) {
				(void)__xcodecHttpParseInt64(pHeader->sValue, &pMsg->iContentLength);
			} else if ( __xcodecHttpStrEqNoCase(pHeader->sName, "Transfer-Encoding") ) {
				if ( __xcodecHttpContainsTokenNoCase(pHeader->sValue, "chunked") ) {
					pMsg->iFlags |= XCODEC_HTTP1_F_CHUNKED;
					pFrame->iFlags |= XCODEC_FRAME_F_CHUNKED;
				}
			} else if ( __xcodecHttpStrEqNoCase(pHeader->sName, "Connection") ) {
				if ( __xcodecHttpContainsTokenNoCase(pHeader->sValue, "upgrade") ) {
					pMsg->iFlags |= XCODEC_HTTP1_F_UPGRADE;
					pFrame->iFlags |= XCODEC_FRAME_F_UPGRADE;
				}
				if ( __xcodecHttpContainsTokenNoCase(pHeader->sValue, "close") ) {
					pMsg->iFlags &= ~XCODEC_HTTP1_F_KEEPALIVE;
					pFrame->iFlags &= ~XCODEC_FRAME_F_KEEPALIVE;
				} else if ( __xcodecHttpContainsTokenNoCase(pHeader->sValue, "keep-alive") ) {
					pMsg->iFlags |= XCODEC_HTTP1_F_KEEPALIVE;
					pFrame->iFlags |= XCODEC_FRAME_F_KEEPALIVE;
				}
			} else if ( __xcodecHttpStrEqNoCase(pHeader->sName, "Upgrade") && pHeader->sValue[0] != '\0' ) {
				pMsg->iFlags |= XCODEC_HTTP1_F_UPGRADE;
				pFrame->iFlags |= XCODEC_FRAME_F_UPGRADE;
			}
		}
		sCursor = sNextLine + 2;
	}
	if ( __xcodecHttpContainsTokenNoCase(pMsg->sVersion, "HTTP/1.1") &&
		((pFrame->iFlags & XCODEC_FRAME_F_KEEPALIVE) == 0) ) {
		const char* sConn = xrtCodecHttp1GetHeader(pMsg, "Connection");
		if ( !sConn || !__xcodecHttpContainsTokenNoCase(sConn, "close") ) {
			pMsg->iFlags |= XCODEC_HTTP1_F_KEEPALIVE;
			pFrame->iFlags |= XCODEC_FRAME_F_KEEPALIVE;
		}
	}
	pMsg->iHeadBytes = iHeadBytes;
	pFrame->iKind = XCODEC_KIND_HTTP1;
	pFrame->iHeaderBytes = iHeadBytes;
	pFrame->iPayloadOffset = iHeadBytes;
	pFrame->iPayloadBytes = 0u;
	pFrame->iFrameBytes = iHeadBytes;
	pResp = __xhttpBuildResponse(pFrame, pMsg, pChain);
	XNET_FREE(sHeadBuf);
	return pResp;

fail:
	if ( sHeadBuf ) { XNET_FREE(sHeadBuf); }
	xrtCodecHttp1MessageUnit(pMsg);
	return NULL;
}


static void __xhttpTxAddRef(__xhttp_tx* pTx)
{
	if ( !pTx ) { return; }
	(void)__xhttpAtomicAdd(&pTx->iRefCount, 1);
}


// 内部函数：__xhttpTxDiscardTransient
static void __xhttpTxDiscardTransient(__xhttp_tx* pTx)
{
	if ( !pTx ) { return; }
	if ( pTx->pStreamResp ) {
		xrtHttpResponseDestroy(pTx->pStreamResp);
		pTx->pStreamResp = NULL;
	}
	if ( pTx->pSendBuf ) {
		XNET_FREE(pTx->pSendBuf);
		pTx->pSendBuf = NULL;
	}
	pTx->iSendLen = 0;
	__xhttpRequestUnitInternal(&pTx->tReq);
	if ( pTx->pClient ) {
		__xhttpClientRelease(pTx->pClient);
		pTx->pClient = NULL;
	}
}


// 内部函数：__xhttpTxComplete
static bool __xhttpTxComplete(__xhttp_tx* pTx, xnet_result iStatus, xhttpresponse* pResp)
{
	if ( !pTx ) {
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		return false;
	}
	if ( __xhttpAtomicCompareExchange(&pTx->iComplete, 1, 0) != 0 ) {
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		return false;
	}
	if ( pTx->pFuture ) {
		(void)__xnetFutureResolve(pTx->pFuture, iStatus, pResp);
		__xnetFutureReleaseAsyncHold(pTx->pFuture);
	} else if ( pResp ) {
		xrtHttpResponseDestroy(pResp);
	}
	return true;
}


// 内部函数：__xhttpTxRelease
static void __xhttpTxRelease(__xhttp_tx* pTx)
{
	if ( !pTx ) { return; }
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
	if ( !pTx ) { return; }
	pTx->pConn = NULL;
	pTx->pStream = NULL;
	__xhttpTxDiscardTransient(pTx);
	__xhttpTxRelease(pTx);
}


// 内部函数：__xhttpTxPostCleanup
static void __xhttpTxPostCleanup(__xhttp_tx* pTx)
{
	uint32 iAffinity = 0u;
	if ( !pTx || !pTx->pEngine ) { return; }
	if ( __xhttpAtomicCompareExchange(&pTx->iCleanupPosted, 1, 0) != 0 ) { return; }
	if ( pTx->pStream && pTx->pStream->pWorker ) { iAffinity = pTx->pStream->pWorker->iId; }
	else if ( pTx->pConn && pTx->pConn->pStream && pTx->pConn->pStream->pWorker ) { iAffinity = pTx->pConn->pStream->pWorker->iId; }
	if ( xrtNetEnginePost(pTx->pEngine, iAffinity, __xhttpTxCleanupTask, pTx) != XRT_NET_OK ) {
		__xhttpTxCleanupTask(NULL, pTx);
	}
}


// 内部函数：__xhttpConnCleanupTask
static void __xhttpConnCleanupTask(xnetworker* pWorker, ptr pArg)
{
	__xhttp_conn* pConn = (__xhttp_conn*)pArg;
	(void)pWorker;
	if ( !pConn ) { return; }
	__xhttpPoolRemove(pConn);
	if ( pConn->pStream ) {
		xrtNetStreamDestroy(pConn->pStream);
		pConn->pStream = NULL;
	}
	if ( pConn->pProxy ) {
		xrtNetProxyRelease(pConn->pProxy);
		pConn->pProxy = NULL;
	}
	if ( pConn->pClient ) {
		__xhttpClientRelease(pConn->pClient);
		pConn->pClient = NULL;
	}
	XNET_FREE(pConn);
}


// 内部函数：__xhttpConnPostCleanup
static void __xhttpConnPostCleanup(__xhttp_conn* pConn)
{
	if ( !pConn ) { return; }
	if ( __xhttpAtomicCompareExchange(&pConn->iCleanupPosted, 1, 0) != 0 ) { return; }
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
	if ( !pTx || !pMsg || !pTx->pConn ) { return false; }
	if ( (pMsg->iFlags & XCODEC_HTTP1_F_KEEPALIVE) == 0u ) { return false; }
	if ( (pMsg->iFlags & XCODEC_HTTP1_F_UPGRADE) != 0u ) { return false; }
	if ( __xhttpRequestWantsClose(&pTx->tReq) ) { return false; }
	if ( __xhttpResponseWantsClose(pMsg) ) { return false; }
	return pTx->pConn->pStream != NULL && pTx->pConn->bOpen;
}


// 内部函数：__xhttpCompleteCloseDelimitedResponse
static bool __xhttpCompleteCloseDelimitedResponse(__xhttp_tx* pTx, xnetchain* pChain)
{
	xcodecframe tFrame;
	xcodechttp1msg tMsg;
	xcodecstatus iParse;
	size_t iChainBytes;
	xhttpresponse* pResp;
	bool bNoBodyExpected;
	if ( !pTx || !pChain ) { return false; }
	iParse = xrtCodecHttp1Parse(pChain, &tFrame, &tMsg);
	if ( iParse != XCODEC_STATUS_FRAME ) { return false; }
	if ( (tMsg.iFlags & XCODEC_HTTP1_F_CHUNKED) != 0u || tMsg.iContentLength >= 0 ) {
		xrtCodecHttp1MessageUnit(&tMsg);
		return false;
	}
	bNoBodyExpected = __xhttpStatusHasNoBody(tMsg.iStatusCode) || __xhttpStrEqNoCase(pTx->tReq.sMethod, "HEAD");
	if ( bNoBodyExpected ) {
		xrtCodecHttp1MessageUnit(&tMsg);
		return false;
	}
	iChainBytes = xrtNetChainBytes(pChain);
	if ( iChainBytes < tFrame.iHeaderBytes ) {
		xrtCodecHttp1MessageUnit(&tMsg);
		return false;
	}
	tFrame.iPayloadBytes = iChainBytes - tFrame.iHeaderBytes;
	tFrame.iFrameBytes = iChainBytes;
	tFrame.iFlags &= ~XCODEC_FRAME_F_KEEPALIVE;
	tMsg.iFlags &= ~XCODEC_HTTP1_F_KEEPALIVE;
	pResp = __xhttpBuildResponse(&tFrame, &tMsg, pChain);
	if ( !pResp ) {
		xrtCodecHttp1MessageUnit(&tMsg);
		(void)__xhttpTxComplete(pTx, XRT_NET_ERROR, NULL);
		return true;
	}
	(void)__xhttpTxComplete(pTx, XRT_NET_OK, pResp);
	xrtCodecFrameConsume(pChain, &tFrame);
	xrtCodecHttp1MessageUnit(&tMsg);
	return true;
}


// 内部函数：__xhttpTxAbortStream
static void __xhttpTxAbortStream(__xhttp_tx* pTx)
{
	if ( !pTx ) { return; }
	if ( pTx->pConn && pTx->pConn->pStream ) { xrtNetStreamClose(pTx->pConn->pStream, XNET_CLOSE_F_ABORT); }
	else if ( pTx->pStream ) { xrtNetStreamClose(pTx->pStream, XNET_CLOSE_F_ABORT); }
}


static void __xhttpFutureCancelCleanup(xnetfuture* pFuture)
{
	__xhttp_cancel_ctx* pCtx;
	if ( !pFuture ) { return; }
	__xnetFutureLock(pFuture);
	pCtx = (__xhttp_cancel_ctx*)pFuture->pPendingCtx;
	pFuture->pPendingCtx = NULL;
	pFuture->pfnPendingCancel = NULL;
	pFuture->pfnPendingCleanup = NULL;
	__xnetFutureUnlock(pFuture);
	if ( pCtx ) {
		if ( pCtx->pTx ) { __xhttpTxRelease(pCtx->pTx); }
		XNET_FREE(pCtx);
	}
}


XXAPI void xrtHttpCloseIdleConnections(xnetengine* pEngine)
{
	__xhttpCloseIdleConnections(NULL, pEngine);
}


XXAPI xhttpclient* xrtHttpClientCreate(xnetengine* pEngine)
{
	xhttpclient* pClient;
	xnetengine* pResolvedEngine = __xnetSyncResolveEngine(pEngine);
	if ( !pResolvedEngine ) { return NULL; }
	pClient = (xhttpclient*)XNET_ALLOC(sizeof(xhttpclient));
	if ( !pClient ) { return NULL; }
	memset(pClient, 0, sizeof(xhttpclient));
	pClient->iRefCount = 1;
	pClient->pEngine = pResolvedEngine;
	return pClient;
}


XXAPI void xrtHttpClientCloseIdle(xhttpclient* pClient)
{
	if ( !pClient ) { return; }
	__xhttpCloseIdleConnections(pClient, NULL);
}


XXAPI void xrtHttpClientDestroy(xhttpclient* pClient)
{
	if ( !pClient ) { return; }
	(void)__xnetAtomicExchange32(&pClient->iClosing, 1);
	__xhttpCloseIdleConnections(pClient, NULL);
	__xhttpClientRelease(pClient);
}


static bool __xhttpFutureCancelRequest(xnetfuture* pFuture)
{
	__xhttp_cancel_ctx* pCtx;
	__xhttp_tx* pTx = NULL;
	bool bCancelled = false;
	if ( !pFuture ) { return false; }
	__xnetFutureLock(pFuture);
	pCtx = (__xhttp_cancel_ctx*)pFuture->pPendingCtx;
	if ( pCtx ) { pTx = pCtx->pTx; }
	if ( pTx ) { __xhttpTxAddRef(pTx); }
	__xnetFutureUnlock(pFuture);
	if ( !pTx ) { return false; }
	if ( __xhttpTxComplete(pTx, XRT_NET_CANCELLED, NULL) ) {
		bCancelled = true;
		__xhttpTxAbortStream(pTx);
	} else {
		bCancelled = xrtNetFutureStatus(pFuture) == XRT_NET_CANCELLED;
	}
	__xhttpTxRelease(pTx);
	return bCancelled;
}


static bool __xhttpFutureAttachCancel(__xhttp_tx* pTx)
{
	__xhttp_cancel_ctx* pCtx;
	bool bAttached = false;
	if ( !pTx || !pTx->pFuture ) { return false; }
	pCtx = (__xhttp_cancel_ctx*)XNET_ALLOC(sizeof(__xhttp_cancel_ctx));
	if ( !pCtx ) { return false; }
	pCtx->pTx = pTx;
	__xhttpTxAddRef(pTx);
	__xnetFutureLock(pTx->pFuture);
	if ( !pTx->pFuture->bDone && pTx->pFuture->pPendingCtx == NULL &&
		pTx->pFuture->pfnPendingCancel == NULL && pTx->pFuture->pfnPendingCleanup == NULL ) {
		pTx->pFuture->pPendingCtx = pCtx;
		pTx->pFuture->pfnPendingCancel = __xhttpFutureCancelRequest;
		pTx->pFuture->pfnPendingCleanup = __xhttpFutureCancelCleanup;
		bAttached = true;
	}
	__xnetFutureUnlock(pTx->pFuture);
	if ( !bAttached ) {
		__xhttpTxRelease(pTx);
		XNET_FREE(pCtx);
	}
	return bAttached;
}


XXAPI bool xrtHttpCancel(xnetfuture* pFuture)
{
	__xnet_future_pending_cancel_fn pfnCancel = NULL;
	if ( !pFuture ) { return false; }
	__xnetFutureLock(pFuture);
	if ( !pFuture->bDone ) { pfnCancel = pFuture->pfnPendingCancel; }
	else {
		bool bCancelled = pFuture->iStatus == XRT_NET_CANCELLED;
		__xnetFutureUnlock(pFuture);
		return bCancelled;
	}
	__xnetFutureUnlock(pFuture);
	return pfnCancel ? pfnCancel(pFuture) : false;
}


// 内部函数：__xhttpIdleTimeoutTask
static void __xhttpIdleTimeoutTask(xnetworker* pWorker, ptr pArg)
{
	__xhttp_idle_timer_ctx* pCtx = (__xhttp_idle_timer_ctx*)pArg;
	__xhttp_tx* pTx;
	(void)pWorker;
	if ( !pCtx ) { return; }
	pTx = pCtx->pTx;
	if ( pTx &&
		__xhttpAtomicLoad(&pTx->iComplete) == 0 &&
		__xhttpAtomicLoad(&pTx->iIdleTimerGen) == pCtx->iGeneration ) {
		(void)__xhttpTxComplete(pTx, XRT_NET_TIMEOUT, NULL);
		__xhttpTxAbortStream(pTx);
	}
	if ( pTx ) { __xhttpTxRelease(pTx); }
	XNET_FREE(pCtx);
}


// 内部函数：__xhttpTxRefreshIdleTimeout
static void __xhttpTxRefreshIdleTimeout(__xhttp_tx* pTx)
{
	__xhttp_idle_timer_ctx* pCtx;
	uint32 iTimeoutMs;
	long iGeneration;
	uint32 iAffinity = 0u;

	// 参数校验和超时配置检查
	if ( !pTx || !pTx->pEngine ) { return; }
	iTimeoutMs = pTx->tReq.iIdleTimeoutMs;
	if ( iTimeoutMs == 0u ) { return; }

	// 递增代数以作废旧定时器
	iGeneration = __xhttpAtomicAdd(&pTx->iIdleTimerGen, 1);
	// 创建定时器上下文
	pCtx = (__xhttp_idle_timer_ctx*)XNET_ALLOC(sizeof(__xhttp_idle_timer_ctx));
	if ( !pCtx ) { return; }
	memset(pCtx, 0, sizeof(__xhttp_idle_timer_ctx));
	pCtx->pTx = pTx;
	pCtx->iGeneration = iGeneration;
	__xhttpTxAddRef(pTx);

	// 确定工作线程亲和性
	if ( pTx->pStream && pTx->pStream->pWorker ) { iAffinity = pTx->pStream->pWorker->iId; }
	else if ( pTx->pConn && pTx->pConn->pStream && pTx->pConn->pStream->pWorker ) { iAffinity = pTx->pConn->pStream->pWorker->iId; }

	// 投递延迟任务，失败时回滚引用
	if ( xrtNetEnginePostDelayed(pTx->pEngine, iAffinity, iTimeoutMs, __xhttpIdleTimeoutTask, pCtx) != XRT_NET_OK ) {
		__xhttpTxRelease(pTx);
		XNET_FREE(pCtx);
	}
}


// 内部函数：连接 resolve 超时毫秒
static uint32 __xhttpResolveConnectTimeoutMs(const xhttprequest* pReq)
{
	if ( !pReq ) { return 0u; }
	if ( pReq->iTimeoutMs > 0u ) { return pReq->iTimeoutMs; }
	return pReq->iIdleTimeoutMs;
}


// 内部函数：__xhttpConnSendActiveTx
static bool __xhttpConnSendActiveTx(__xhttp_conn* pConn)
{
	__xhttp_tx* pTx;
	if ( !pConn || !pConn->pStream ) { return false; }
	pTx = pConn->pTx;
	if ( !pTx || !pTx->pSendBuf || pTx->iSendLen == 0u ) { return false; }
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
	if ( !pTx ) { return; }
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
	if ( !pConn || !pStream ) { return; }
	pConn->bOpen = true;
	(void)__xhttpConnSendActiveTx(pConn);
}


static size_t __xhttpStreamFindCrlf(const xnetchain* pChain)
{
	size_t iPos = 0u;
	if ( !pChain ) { return (size_t)-1; }
	for ( ;; ) {
		char ch;
		iPos = xrtNetChainFindByte(pChain, '\r', iPos);
		if ( iPos == (size_t)-1 ) { return (size_t)-1; }
		if ( xrtNetChainPeekAt(pChain, iPos + 1u, &ch, 1u) == 1u && ch == '\n' ) { return iPos; }
		iPos++;
	}
}


static size_t __xhttpStreamFindHeaderEnd(const xnetchain* pChain)
{
	size_t iPos = 0u;
	if ( !pChain ) { return (size_t)-1; }
	for ( ;; ) {
		char aTail[3];
		iPos = xrtNetChainFindByte(pChain, '\r', iPos);
		if ( iPos == (size_t)-1 ) { return (size_t)-1; }
		if ( xrtNetChainPeekAt(pChain, iPos + 1u, aTail, sizeof(aTail)) == sizeof(aTail) &&
			aTail[0] == '\n' && aTail[1] == '\r' && aTail[2] == '\n' ) {
			return iPos;
		}
		iPos++;
	}
}


static void __xhttpStreamFail(__xhttp_tx* pTx, xnet_result iStatus)
{
	if ( !pTx ) { return; }
	(void)__xhttpTxComplete(pTx, iStatus, NULL);
	__xhttpTxAbortStream(pTx);
}


static bool __xhttpStreamEmitBody(__xhttp_tx* pTx, const void* pData, size_t iLen)
{
	if ( !pTx || (!pData && iLen > 0u) ) { return false; }
	if ( __xhttpAtomicLoad(&pTx->iComplete) != 0 ) { return false; }
	if ( iLen == 0u ) { return true; }
	if ( pTx->tStreamCallbacks.OnBody &&
		!pTx->tStreamCallbacks.OnBody(pTx->tStreamCallbacks.pUserData, pData, iLen) ) {
		__xhttpStreamFail(pTx, XRT_NET_CANCELLED);
		return false;
	}
	pTx->iBodyReceived += (uint64)iLen;
	if ( pTx->pStreamResp ) { pTx->pStreamResp->iBodyLen = (size_t)pTx->iBodyReceived; }
	return true;
}


static bool __xhttpStreamResponseReusable(const __xhttp_tx* pTx, const xhttpresponse* pResp)
{
	if ( !pTx || !pResp || !pTx->pConn ) { return false; }
	if ( (pResp->iFlags & XHTTP_RESP_F_KEEPALIVE) == 0u ) { return false; }
	if ( (pResp->iFlags & XHTTP_RESP_F_UPGRADE) != 0u ) { return false; }
	if ( __xhttpRequestWantsClose(&pTx->tReq) ) { return false; }
	return pTx->pConn->pStream != NULL && pTx->pConn->bOpen;
}


static void __xhttpStreamFinish(__xhttp_tx* pTx, xnetstream* pStream, xnetchain* pChain)
{
	__xhttp_conn* pConn;
	xhttpresponse* pResp;
	bool bReusable;
	if ( !pTx || !pTx->pStreamResp ) { return; }
	pConn = pTx->pConn;
	pResp = pTx->pStreamResp;
	pTx->pStreamResp = NULL;
	pResp->iBodyLen = (size_t)pTx->iBodyReceived;
	if ( (pResp->iFlags & XHTTP_RESP_F_CHUNKED) != 0u ) {
		pResp->iContentLength = (int64_t)pTx->iBodyReceived;
	}
	bReusable = __xhttpStreamResponseReusable(pTx, pResp) && (!pChain || xrtNetChainBytes(pChain) == 0u);
	if ( bReusable && pConn ) {
		pConn->pTx = NULL;
		if ( __xhttpPoolPut(pConn) ) {
			pTx->pConn = NULL;
			pTx->pStream = NULL;
			(void)__xhttpTxComplete(pTx, XRT_NET_OK, pResp);
			__xhttpTxPostCleanup(pTx);
			return;
		}
		pConn->pTx = pTx;
	}
	(void)__xhttpTxComplete(pTx, XRT_NET_OK, pResp);
	if ( pStream ) { xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT); }
}


// Returns -1 on terminal error/cancellation, 0 when more bytes are needed, and 1 on completion.
static int __xhttpContinueStreamBody(__xhttp_tx* pTx, xnetstream* pStream, xnetchain* pChain)
{
	if ( !pTx || !pStream || !pChain ) { return -1; }
	if ( pTx->iStreamBodyMode == __XHTTP_STREAM_BODY_CLOSE ) {
		for ( ;; ) {
			xnetspan arrSpans[8];
			uint32 iCount = xrtNetChainGetSpans(pChain, arrSpans, (uint32)(sizeof(arrSpans) / sizeof(arrSpans[0])));
			if ( iCount == 0u ) { return 0; }
			if ( !__xhttpStreamEmitBody(pTx, arrSpans[0].pData, arrSpans[0].iLen) ) { return -1; }
			xrtNetChainConsume(pChain, arrSpans[0].iLen);
		}
	}
	if ( pTx->iStreamBodyMode == __XHTTP_STREAM_BODY_FIXED ) {
		for ( ;; ) {
			xnetspan arrSpans[8];
			uint32 iCount;
			size_t iTake;
			if ( pTx->iBodyRemain == 0u ) {
				__xhttpStreamFinish(pTx, pStream, pChain);
				return 1;
			}
			iCount = xrtNetChainGetSpans(pChain, arrSpans, (uint32)(sizeof(arrSpans) / sizeof(arrSpans[0])));
			if ( iCount == 0u ) { return 0; }
			iTake = arrSpans[0].iLen;
			if ( (uint64)iTake > pTx->iBodyRemain ) { iTake = (size_t)pTx->iBodyRemain; }
			if ( !__xhttpStreamEmitBody(pTx, arrSpans[0].pData, iTake) ) { return -1; }
			xrtNetChainConsume(pChain, iTake);
			pTx->iBodyRemain -= (uint64)iTake;
		}
	}
	if ( pTx->iStreamBodyMode != __XHTTP_STREAM_BODY_CHUNKED ) { return -1; }
	for ( ;; ) {
		if ( pTx->iStreamChunkState == __XHTTP_STREAM_CHUNK_SIZE ) {
			size_t iLineLen = __xhttpStreamFindCrlf(pChain);
			char* sLine;
			uint64 iChunkSize = 0u;
			if ( iLineLen == (size_t)-1 ) {
				if ( xrtNetChainBytes(pChain) > 8192u ) { __xhttpStreamFail(pTx, XRT_NET_ERROR); return -1; }
				return 0;
			}
			if ( iLineLen > 8192u ) { __xhttpStreamFail(pTx, XRT_NET_ERROR); return -1; }
			sLine = (char*)XNET_ALLOC(iLineLen + 1u);
			if ( !sLine ) { __xhttpStreamFail(pTx, XRT_NET_ERROR); return -1; }
			if ( xrtNetChainPeekAt(pChain, 0u, sLine, iLineLen) != iLineLen ) {
				XNET_FREE(sLine);
				__xhttpStreamFail(pTx, XRT_NET_ERROR);
				return -1;
			}
			sLine[iLineLen] = '\0';
			if ( !__xcodecHttpParseHexU64(sLine, iLineLen, &iChunkSize) ) {
				XNET_FREE(sLine);
				__xhttpStreamFail(pTx, XRT_NET_ERROR);
				return -1;
			}
			XNET_FREE(sLine);
			xrtNetChainConsume(pChain, iLineLen + 2u);
			pTx->iChunkRemain = iChunkSize;
			pTx->iStreamChunkState = iChunkSize == 0u ? __XHTTP_STREAM_CHUNK_TRAILER : __XHTTP_STREAM_CHUNK_DATA;
		}
		else if ( pTx->iStreamChunkState == __XHTTP_STREAM_CHUNK_DATA ) {
			xnetspan arrSpans[8];
			uint32 iCount;
			size_t iTake;
			if ( pTx->iChunkRemain == 0u ) {
				pTx->iStreamChunkState = __XHTTP_STREAM_CHUNK_DATA_CRLF;
				continue;
			}
			iCount = xrtNetChainGetSpans(pChain, arrSpans, (uint32)(sizeof(arrSpans) / sizeof(arrSpans[0])));
			if ( iCount == 0u ) { return 0; }
			iTake = arrSpans[0].iLen;
			if ( (uint64)iTake > pTx->iChunkRemain ) { iTake = (size_t)pTx->iChunkRemain; }
			if ( !__xhttpStreamEmitBody(pTx, arrSpans[0].pData, iTake) ) { return -1; }
			xrtNetChainConsume(pChain, iTake);
			pTx->iChunkRemain -= (uint64)iTake;
			if ( pTx->iChunkRemain == 0u ) { pTx->iStreamChunkState = __XHTTP_STREAM_CHUNK_DATA_CRLF; }
		}
		else if ( pTx->iStreamChunkState == __XHTTP_STREAM_CHUNK_DATA_CRLF ) {
			char aCrlf[2];
			if ( xrtNetChainBytes(pChain) < 2u ) { return 0; }
			if ( xrtNetChainPeekAt(pChain, 0u, aCrlf, sizeof(aCrlf)) != sizeof(aCrlf) || aCrlf[0] != '\r' || aCrlf[1] != '\n' ) {
				__xhttpStreamFail(pTx, XRT_NET_ERROR);
				return -1;
			}
			xrtNetChainConsume(pChain, 2u);
			pTx->iStreamChunkState = __XHTTP_STREAM_CHUNK_SIZE;
		}
		else if ( pTx->iStreamChunkState == __XHTTP_STREAM_CHUNK_TRAILER ) {
			char aCrlf[2];
			size_t iTrailerEnd;
			if ( xrtNetChainBytes(pChain) < 2u ) { return 0; }
			if ( xrtNetChainPeekAt(pChain, 0u, aCrlf, sizeof(aCrlf)) == sizeof(aCrlf) && aCrlf[0] == '\r' && aCrlf[1] == '\n' ) {
				xrtNetChainConsume(pChain, 2u);
				__xhttpStreamFinish(pTx, pStream, pChain);
				return 1;
			}
			iTrailerEnd = __xhttpStreamFindHeaderEnd(pChain);
			if ( iTrailerEnd == (size_t)-1 ) {
				if ( xrtNetChainBytes(pChain) > 8192u ) { __xhttpStreamFail(pTx, XRT_NET_ERROR); return -1; }
				return 0;
			}
			xrtNetChainConsume(pChain, iTrailerEnd + 4u);
			__xhttpStreamFinish(pTx, pStream, pChain);
			return 1;
		}
		else {
			__xhttpStreamFail(pTx, XRT_NET_ERROR);
			return -1;
		}
	}
}


static int __xhttpStartStreamResponse(__xhttp_tx* pTx, xnetstream* pStream, xnetchain* pChain)
{
	xcodecframe tFrame;
	xcodechttp1msg tMsg;
	xcodecstatus iParse;
	xhttpresponse* pResp;
	bool bNoBodyExpected;
	if ( !pTx || !pStream || !pChain ) { return -1; }
	iParse = xrtCodecHttp1ParseHead(pChain, &tFrame, &tMsg);
	if ( iParse == XCODEC_STATUS_NEED_MORE ) { return 0; }
	if ( iParse == XCODEC_STATUS_ERROR ) { __xhttpStreamFail(pTx, XRT_NET_ERROR); return -1; }
	// Ignore non-upgrade informational responses and continue with the final response.
	if ( tMsg.iStatusCode >= 100u && tMsg.iStatusCode < 200u && tMsg.iStatusCode != 101u ) {
		xrtNetChainConsume(pChain, tFrame.iHeaderBytes);
		xrtCodecHttp1MessageUnit(&tMsg);
		return 2;
	}
	pResp = __xhttpBuildResponse(&tFrame, &tMsg, pChain);
	if ( !pResp ) {
		xrtCodecHttp1MessageUnit(&tMsg);
		__xhttpStreamFail(pTx, XRT_NET_ERROR);
		return -1;
	}
	bNoBodyExpected = __xhttpStatusHasNoBody(tMsg.iStatusCode) || __xhttpStrEqNoCase(pTx->tReq.sMethod, "HEAD");
	pTx->pStreamResp = pResp;
	pTx->bStreamHeadersDelivered = true;
	pTx->iBodyReceived = 0u;
	pTx->iStreamChunkState = __XHTTP_STREAM_CHUNK_SIZE;
	if ( bNoBodyExpected || tMsg.iContentLength == 0 ) {
		pTx->iStreamBodyMode = __XHTTP_STREAM_BODY_NONE;
	} else if ( (tMsg.iFlags & XCODEC_HTTP1_F_CHUNKED) != 0u ) {
		pTx->iStreamBodyMode = __XHTTP_STREAM_BODY_CHUNKED;
	} else if ( tMsg.iContentLength > 0 ) {
		pTx->iStreamBodyMode = __XHTTP_STREAM_BODY_FIXED;
		pTx->iBodyRemain = (uint64)tMsg.iContentLength;
	} else {
		pTx->iStreamBodyMode = __XHTTP_STREAM_BODY_CLOSE;
	}
	xrtNetChainConsume(pChain, tFrame.iHeaderBytes);
	xrtCodecHttp1MessageUnit(&tMsg);
	if ( pTx->tStreamCallbacks.OnHeaders &&
		!pTx->tStreamCallbacks.OnHeaders(pTx->tStreamCallbacks.pUserData, pResp) ) {
		__xhttpStreamFail(pTx, XRT_NET_CANCELLED);
		return -1;
	}
	if ( pTx->iStreamBodyMode == __XHTTP_STREAM_BODY_NONE ) {
		__xhttpStreamFinish(pTx, pStream, pChain);
		return 1;
	}
	return __xhttpContinueStreamBody(pTx, pStream, pChain);
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
	bool bHadHead;
	// 参数校验
	if ( !pTx || !pStream || !pChain ) { return; }
	/* Do not enter user callbacks after cancellation completed on another thread. */
	if ( __xhttpAtomicLoad(&pTx->iComplete) != 0 ) { return; }
	// 刷新空闲超时定时器
	__xhttpTxRefreshIdleTimeout(pTx);
	// 解析 HTTP 响应
	if ( pTx->bStreaming ) {
		if ( pTx->bStreamHeadersDelivered ) {
			(void)__xhttpContinueStreamBody(pTx, pStream, pChain);
			return;
		}
		for ( ;; ) {
			int iStreamResult = __xhttpStartStreamResponse(pTx, pStream, pChain);
			if ( iStreamResult != 2 ) { return; }
		}
	}
	iParse = xrtCodecHttp1Parse(pChain, &tFrame, &tMsg);
	if ( iParse == XCODEC_STATUS_NEED_MORE ) {
		if ( !__xhttpStrEqNoCase(pTx->tReq.sMethod, "HEAD") ) { return; }
		bHadHead = false;
		pResp = __xhttpBuildHeadResponse(pChain, &tFrame, &tMsg, &bHadHead);
		if ( !pResp ) {
			if ( bHadHead ) {
				(void)__xhttpTxComplete(pTx, XRT_NET_ERROR, NULL);
				xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
			}
			return;
		}
		bNoBodyExpected = true;
		xrtCodecFrameConsume(pChain, &tFrame);
		goto response_ready;
	}
	// 解析失败，终止事务
	if ( iParse == XCODEC_STATUS_ERROR ) {
		(void)__xhttpTxComplete(pTx, XRT_NET_ERROR, NULL);
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return;
	}
	// 检查响应正文期望情况
	bNoBodyExpected = __xhttpStatusHasNoBody(tMsg.iStatusCode) || __xhttpStrEqNoCase(pTx->tReq.sMethod, "HEAD");
	if ( (tMsg.iFlags & XCODEC_HTTP1_F_CHUNKED) == 0u && (tMsg.iContentLength < 0 && !bNoBodyExpected) ) {
		xrtCodecHttp1MessageUnit(&tMsg);
		return;
	}
	// 构建响应对象
	pResp = __xhttpBuildResponse(&tFrame, &tMsg, pChain);
	if ( !pResp ) {
		xrtCodecHttp1MessageUnit(&tMsg);
		(void)__xhttpTxComplete(pTx, XRT_NET_ERROR, NULL);
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return;
	}
	// 消费已解析的帧数据
	xrtCodecFrameConsume(pChain, &tFrame);
	// 判断连接是否可复用
response_ready:
	bReusable = __xhttpResponseReusable(pTx, &tMsg) && (!bNoBodyExpected || xrtNetChainBytes(pChain) == 0u);
	xrtCodecHttp1MessageUnit(&tMsg);
	if ( bReusable && pConn ) {
		// 可复用：归还连接到空闲池，完成事务
		pConn->pTx = NULL;
		if ( __xhttpPoolPut(pConn) ) {
			pTx->pConn = NULL;
			pTx->pStream = NULL;
			(void)__xhttpTxComplete(pTx, XRT_NET_OK, pResp);
			__xhttpTxPostCleanup(pTx);
			return;
		}
		pConn->pTx = pTx;
	}
	// 不可复用：完成事务并关闭连接
	(void)__xhttpTxComplete(pTx, XRT_NET_OK, pResp);
	xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
}


// 内部函数：__xhttpClientOnClose
static void __xhttpClientOnClose(ptr pOwner, xnetstream* pStream, xnet_result iReason)
{
	__xhttp_conn* pConn = (__xhttp_conn*)pOwner;
	__xhttp_tx* pTx = pConn ? pConn->pTx : NULL;
	if ( !pConn ) { return; }
	pConn->bOpen = false;
	if ( pConn->bIdle ) { __xhttpPoolRemove(pConn); }
	pConn->bIdle = false;
	pConn->pStream = NULL;
	pConn->pTx = NULL;
	if ( pTx ) {
		if ( pTx->bStreaming && pTx->bStreamHeadersDelivered &&
			pTx->iStreamBodyMode == __XHTTP_STREAM_BODY_CLOSE &&
			__xhttpAtomicLoad(&pTx->iComplete) == 0 && iReason == XRT_NET_CLOSED ) {
			xhttpresponse* pResp;
			if ( pStream && xrtNetChainBytes(&pStream->tRxChain) > 0u ) {
				(void)__xhttpContinueStreamBody(pTx, pStream, &pStream->tRxChain);
			}
			pResp = pTx->pStreamResp;
			pTx->pStreamResp = NULL;
			if ( pResp ) {
				pResp->iBodyLen = (size_t)pTx->iBodyReceived;
				pResp->iContentLength = (int64_t)pTx->iBodyReceived;
				(void)__xhttpTxComplete(pTx, XRT_NET_OK, pResp);
			} else if ( __xhttpAtomicLoad(&pTx->iComplete) == 0 ) {
				(void)__xhttpTxComplete(pTx, XRT_NET_ERROR, NULL);
			}
			pTx->pConn = NULL;
			pTx->pStream = NULL;
			__xhttpTxPostCleanup(pTx);
			__xhttpConnPostCleanup(pConn);
			return;
		}
		if ( __xhttpAtomicLoad(&pTx->iComplete) == 0 &&
			iReason == XRT_NET_CLOSED &&
			pStream &&
			xrtNetChainBytes(&pStream->tRxChain) > 0u &&
			__xhttpCompleteCloseDelimitedResponse(pTx, &pStream->tRxChain) ) {
			pTx->pConn = NULL;
			pTx->pStream = NULL;
			__xhttpTxPostCleanup(pTx);
			__xhttpConnPostCleanup(pConn);
			return;
		}
		pTx->pConn = NULL;
		pTx->pStream = NULL;
	}
	if ( pTx && __xhttpAtomicLoad(&pTx->iComplete) == 0 ) {
		(void)__xhttpTxComplete(pTx, iReason == XRT_NET_CLOSED ? XRT_NET_CLOSED : XRT_NET_ERROR, NULL);
	}
	if ( pTx ) { __xhttpTxPostCleanup(pTx); }
	__xhttpConnPostCleanup(pConn);
}


// 内部函数：__xhttpClientOnError
static void __xhttpClientOnError(ptr pOwner, xnetstream* pStream, int iSysErr)
{
	__xhttp_conn* pConn = (__xhttp_conn*)pOwner;
	__xhttp_tx* pTx = pConn ? pConn->pTx : NULL;
	(void)pStream;
	if ( pConn ) { pConn->iLastSysErr = iSysErr; }
	if ( pTx ) { pTx->iLastSysErr = iSysErr; }
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
		NULL,
		NULL
	};
	return &tEvents;
}


// xrtHttpExecuteAsync 相关处理
static xnetfuture* __xhttpExecuteAsyncEx(xhttpclient* pClient, xnetengine* pEngine, const xhttprequest* pReq, const xhttpstreamcallbacks* pCallbacks, bool bStreaming)
{
	__xhttp_tx* pTx;
	__xhttp_conn* pConn = NULL;
	xnetfuture* pFuture;
	xnetconnectconfig tConnCfg;
	xnetengine* pResolvedEngine;

	// 参数校验与引擎解析
	if ( !pReq ) { return NULL; }
	if ( pClient && __xhttpAtomicLoad(&pClient->iClosing) != 0 ) { return NULL; }
	pResolvedEngine = pClient ? pClient->pEngine : __xnetSyncResolveEngine(pEngine);
	if ( !pResolvedEngine ) { return NULL; }
	// 创建 Future 用于异步返回结果
	pFuture = xrtNetFutureCreate();
	if ( !pFuture ) { return NULL; }

	// 分配并初始化事务对象
	pTx = (__xhttp_tx*)XNET_ALLOC(sizeof(__xhttp_tx));
	if ( !pTx ) {
		xrtNetFutureDestroy(pFuture);
		return NULL;
	}
	memset(pTx, 0, sizeof(__xhttp_tx));
	pTx->iRefCount = 1;
	pTx->pEngine = pResolvedEngine;
	pTx->pFuture = pFuture;
	__xnetFutureAddAsyncHold(pFuture);
	pTx->pClient = __xhttpClientAddRef(pClient);
	pTx->bStreaming = bStreaming;
	pTx->iStreamBodyMode = __XHTTP_STREAM_BODY_NONE;
	pTx->iStreamChunkState = __XHTTP_STREAM_CHUNK_SIZE;
	if ( pCallbacks ) { memcpy(&pTx->tStreamCallbacks, pCallbacks, sizeof(xhttpstreamcallbacks)); }
	if ( !__xhttpFutureAttachCancel(pTx) ) {
		(void)__xhttpTxComplete(pTx, XRT_NET_ERROR, NULL);
		__xhttpTxRelease(pTx);
		return pFuture;
	}
	// 克隆请求数据到事务中
	if ( !__xhttpRequestClone(&pTx->tReq, pReq) ) {
		(void)__xhttpTxComplete(pTx, XRT_NET_ERROR, NULL);
		__xhttpTxRelease(pTx);
		return pFuture;
	}
	// 延迟解析 URL（若主机地址为空则从 sURL 重新解析）
	if ( pTx->tReq.tURL.sHost[0] == '\0' ) {
	if ( pTx->tReq.sURL[0] == '\0' || !xrtUrlParseFixedTo(pTx->tReq.sURL, "http", "https", &pTx->tReq.tURL.bHttps, pTx->tReq.tURL.sHost, sizeof(pTx->tReq.tURL.sHost), &pTx->tReq.tURL.iPort, pTx->tReq.tURL.sPath, sizeof(pTx->tReq.tURL.sPath)) ) {
			(void)__xhttpTxComplete(pTx, XRT_NET_ERROR, NULL);
			__xhttpTxRelease(pTx);
			return pFuture;
		}
	}
	// 序列化请求为原始字节
	if ( !__xhttpBuildRequestBytes(&pTx->tReq, &pTx->pSendBuf, &pTx->iSendLen) ) {
		(void)__xhttpTxComplete(pTx, XRT_NET_ERROR, NULL);
		__xhttpTxRelease(pTx);
		return pFuture;
	}

	// 尝试从空闲连接池中获取可复用的连接
	pConn = __xhttpPoolTake(pClient, pResolvedEngine, &pTx->tReq);
	if ( !pConn ) {
		// 空闲池中无可用连接，创建新连接
		pConn = (__xhttp_conn*)XNET_ALLOC(sizeof(__xhttp_conn));
		if ( !pConn ) {
			(void)__xhttpTxComplete(pTx, XRT_NET_ERROR, NULL);
			__xhttpTxRelease(pTx);
			return pFuture;
		}
		memset(pConn, 0, sizeof(__xhttp_conn));
		pConn->pEngine = pResolvedEngine;
		pConn->pClient = __xhttpClientAddRef(pClient);
		__xhttpCopyToken(pConn->sHost, sizeof(pConn->sHost), pTx->tReq.tURL.sHost);
		pConn->iPort = pTx->tReq.tURL.iPort;
		pConn->bHttps = pTx->tReq.tURL.bHttps;
		pConn->bVerifyPeer = pTx->tReq.bVerifyPeer;
		pConn->pProxy = pTx->tReq.pProxy ? xrtNetProxyAddRef(pTx->tReq.pProxy) : NULL;
		// 创建网络流并绑定客户端事件
		pConn->pStream = xrtNetStreamCreate(pResolvedEngine, __xhttpClientEvents(), pConn);
		if ( !pConn->pStream ) {
			(void)__xhttpTxComplete(pTx, XRT_NET_ERROR, NULL);
			if ( pConn->pProxy ) {
				xrtNetProxyRelease(pConn->pProxy);
				pConn->pProxy = NULL;
			}
			if ( pConn->pClient ) { __xhttpClientRelease(pConn->pClient); }
			XNET_FREE(pConn);
			__xhttpTxRelease(pTx);
			return pFuture;
		}
	}

	// 绑定事务到连接
	pConn->pTx = pTx;
	pTx->pConn = pConn;
	pTx->pStream = pConn->pStream;

	// 连接未打开时发起连接请求
	if ( !pConn->bOpen ) {
		xrtNetConnectConfigInit(&tConnCfg);
		tConnCfg.sHost = pTx->tReq.tURL.sHost;
		tConnCfg.iPort = pTx->tReq.tURL.iPort;
		tConnCfg.iConnectTimeoutMs = __xhttpResolveConnectTimeoutMs(&pTx->tReq);
		tConnCfg.iRecvLimit = 1024u * 1024u;
		tConnCfg.pProxy = pConn->pProxy;
		// 配置 TLS 参数（HTTPS 连接）
		if ( pTx->tReq.tURL.bHttps ) {
			memset(&pConn->tTlsCfg, 0, sizeof(pConn->tTlsCfg));
			pConn->tTlsCfg.sHostName = pTx->tReq.tURL.sHost;
			pConn->tTlsCfg.bVerifyPeer = pTx->tReq.bVerifyPeer;
			tConnCfg.pTlsConfig = &pConn->tTlsCfg;
		}
		if ( xrtNetStreamConnect(pConn->pStream, &tConnCfg) != XRT_NET_OK ) {
			(void)__xhttpTxComplete(pTx, XRT_NET_ERROR, NULL);
			xrtNetStreamDestroy(pConn->pStream);
			pConn->pStream = NULL;
			pConn->pTx = NULL;
			pTx->pConn = NULL;
			pTx->pStream = NULL;
			if ( pConn->pProxy ) {
				xrtNetProxyRelease(pConn->pProxy);
				pConn->pProxy = NULL;
			}
			if ( pConn->pClient ) { __xhttpClientRelease(pConn->pClient); }
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


XXAPI xnetfuture* xrtHttpExecuteAsync(xnetengine* pEngine, const xhttprequest* pReq)
{
	return __xhttpExecuteAsyncEx(NULL, pEngine, pReq, NULL, false);
}


XXAPI xnetfuture* xrtHttpExecuteStreamAsync(xnetengine* pEngine, const xhttprequest* pReq, const xhttpstreamcallbacks* pCallbacks)
{
	return __xhttpExecuteAsyncEx(NULL, pEngine, pReq, pCallbacks, true);
}


XXAPI xnetfuture* xrtHttpClientExecuteAsync(xhttpclient* pClient, const xhttprequest* pReq)
{
	return pClient ? __xhttpExecuteAsyncEx(pClient, NULL, pReq, NULL, false) : NULL;
}


XXAPI xnetfuture* xrtHttpClientExecuteStreamAsync(xhttpclient* pClient, const xhttprequest* pReq, const xhttpstreamcallbacks* pCallbacks)
{
	return pClient ? __xhttpExecuteAsyncEx(pClient, NULL, pReq, pCallbacks, true) : NULL;
}


// xrtHttpExecuteSync 相关处理
XXAPI xhttpresponse* xrtHttpExecuteSync(xnetengine* pEngine, const xhttprequest* pReq, xnet_result* pStatus)
{
	xnetfuture* pFuture;
	xnet_result iStatus;
	xhttpresponse* pResp;
	if ( pStatus ) { *pStatus = XRT_NET_ERROR; }
	pFuture = xrtHttpExecuteAsync(pEngine, pReq);
	if ( !pFuture ) { return NULL; }
	iStatus = xrtNetFutureWait(pFuture, XNET_WAIT_INFINITE);
	pResp = (iStatus == XRT_NET_OK) ? (xhttpresponse*)xrtNetFutureValue(pFuture) : NULL;
	if ( pStatus ) { *pStatus = iStatus; }
	xrtNetFutureDestroy(pFuture);
	return pResp;
}


XXAPI xhttpresponse* xrtHttpClientExecuteSync(xhttpclient* pClient, const xhttprequest* pReq, xnet_result* pStatus)
{
	xnetfuture* pFuture;
	xnet_result iStatus;
	xhttpresponse* pResp;
	if ( pStatus ) { *pStatus = XRT_NET_ERROR; }
	pFuture = xrtHttpClientExecuteAsync(pClient, pReq);
	if ( !pFuture ) { return NULL; }
	iStatus = xrtNetFutureWait(pFuture, XNET_WAIT_INFINITE);
	pResp = iStatus == XRT_NET_OK ? (xhttpresponse*)xrtNetFutureValue(pFuture) : NULL;
	if ( pStatus ) { *pStatus = iStatus; }
	xrtNetFutureDestroy(pFuture);
	return pResp;
}


// xrtHttpExecuteSyncDefault 相关处理：隐藏 xnet_result，便于脚本运行时窄 ABI 调用
XXAPI xhttpresponse* xrtHttpExecuteSyncDefault(const xhttprequest* pReq)
{
	return xrtHttpExecuteSync(NULL, pReq, NULL);
}

#endif
