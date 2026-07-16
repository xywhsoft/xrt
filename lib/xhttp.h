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
	  - chunked transfer and response trailers are fully parsed and exposed
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
#define XHTTP_MAX_TRAILERS       16u
#define XHTTP_MAX_INFORMATIONAL_RESPONSES 16u
#define XHTTP_CLIENT_CONFIG_VERSION 3u

#define XHTTP_CLIENT_F_NONE                         0x00000000u
#define XHTTP_CLIENT_F_FOLLOW_REDIRECTS             0x00000001u
#define XHTTP_CLIENT_F_REDIRECT_POST_TO_GET          0x00000002u
#define XHTTP_CLIENT_F_ALLOW_CROSS_ORIGIN_AUTH       0x00000004u
#define XHTTP_CLIENT_F_ALLOW_HTTPS_DOWNGRADE         0x00000008u
#define XHTTP_CLIENT_F_AUTO_DECOMPRESS                0x00000010u
#ifndef XRT_NO_XINFLATE
#define XHTTP_CLIENT_DEFAULT_FLAGS \
	(XHTTP_CLIENT_F_FOLLOW_REDIRECTS | XHTTP_CLIENT_F_REDIRECT_POST_TO_GET | \
	 XHTTP_CLIENT_F_AUTO_DECOMPRESS)
#else
#define XHTTP_CLIENT_DEFAULT_FLAGS \
	(XHTTP_CLIENT_F_FOLLOW_REDIRECTS | XHTTP_CLIENT_F_REDIRECT_POST_TO_GET)
#endif
#define XHTTP_CLIENT_DEFAULT_MAX_REDIRECTS 10u
#define XHTTP_CLIENT_DEFAULT_MAX_IDLE_CONNECTIONS 128u
#define XHTTP_CLIENT_DEFAULT_MAX_IDLE_CONNECTIONS_PER_ORIGIN 8u
#define XHTTP_CLIENT_DEFAULT_IDLE_CONNECTION_TIMEOUT_MS 90000u

/* Legacy source-compatibility constants; header strings are dynamically sized. */
#define XHTTP_HEADER_F_OWN_STRINGS 0x00000001u

#define XHTTP_RESP_F_NONE        0x00000000u
#define XHTTP_RESP_F_CHUNKED     0x00000001u
#define XHTTP_RESP_F_KEEPALIVE   0x00000002u
#define XHTTP_RESP_F_UPGRADE     0x00000004u
#define XHTTP_RESP_F_DECOMPRESSED 0x00000008u

typedef struct xrt_http_client xhttpclient;

typedef struct xrt_http_client_config {
	uint32 iSize;
	uint32 iVersion;
	uint32 iRecvLimit;
	uint32 iMaxInformationalResponses;
	xcodechttp1limits tHttp1Limits;
	xhttpcookiejar* pCookieJar;
	uint32 iFlags;
	uint32 iMaxRedirects;
	uint32 iMaxIdleConnections;
	uint32 iMaxIdleConnectionsPerOrigin;
	uint32 iIdleConnectionTimeoutMs;
} xhttpclientconfig;

#define XHTTP_CLIENT_CONFIG_V1_SIZE ((uint32)(offsetof(xhttpclientconfig, tHttp1Limits) + sizeof(xcodechttp1limits)))
#define XHTTP_CLIENT_CONFIG_V2_SIZE ((uint32)(offsetof(xhttpclientconfig, pCookieJar) + sizeof(((xhttpclientconfig*)0)->pCookieJar)))
#define XHTTP_CLIENT_CONFIG_V3_SIZE ((uint32)(offsetof(xhttpclientconfig, iIdleConnectionTimeoutMs) + sizeof(((xhttpclientconfig*)0)->iIdleConnectionTimeoutMs)))

#define XHTTP_CLIENT_STATS_VERSION 1u
typedef struct xrt_http_client_stats {
	uint32 iSize;
	uint32 iVersion;
	uint32 iActiveConnections;
	uint32 iIdleConnections;
	uint64 iRequestsStarted;
	uint64 iRequestsCompleted;
	uint64 iConnectionsOpened;
	uint64 iConnectionsReused;
	uint64 iConnectionsClosed;
	uint64 iRedirectsFollowed;
	uint64 iRequestBytes;
	uint64 iResponseBodyBytes;
} xhttpclientstats;

#define XHTTP_CLIENT_STATS_V1_SIZE ((uint32)sizeof(xhttpclientstats))

typedef struct xrt_http_header {
	const char* sName;
	const char* sValue;
	uint32 iFlags;
} xhttpheader;

typedef struct xrt_http_url {
	bool bHttps;
	uint16 iPort;
	char sHost[XHTTP_HOST_CAP];
	char sPath[XHTTP_PATH_CAP];
	char* pHostStorage;
	char* pPathStorage;
} xhttpurl;

typedef enum xrt_http_phase {
	XHTTP_PHASE_NONE = 0,
	XHTTP_PHASE_PREPARE,
	XHTTP_PHASE_CONNECT,
	XHTTP_PHASE_WRITE,
	XHTTP_PHASE_RESPONSE_HEADERS,
	XHTTP_PHASE_RESPONSE_BODY,
	XHTTP_PHASE_COMPLETE
} xhttp_phase;

typedef enum xrt_http_error_code {
	XHTTP_ERROR_NONE = 0,
	XHTTP_ERROR_INVALID_ARGUMENT,
	XHTTP_ERROR_OUT_OF_MEMORY,
	XHTTP_ERROR_INVALID_URL,
	XHTTP_ERROR_CONNECT,
	XHTTP_ERROR_WRITE,
	XHTTP_ERROR_PROTOCOL,
	XHTTP_ERROR_TOO_MANY_INFORMATIONAL_RESPONSES,
	XHTTP_ERROR_CALLBACK_ABORT,
	XHTTP_ERROR_TIMEOUT_TOTAL,
	XHTTP_ERROR_TIMEOUT_IDLE,
	XHTTP_ERROR_CANCELLED,
	XHTTP_ERROR_CONNECTION_CLOSED,
	XHTTP_ERROR_TRANSPORT,
	XHTTP_ERROR_BODY,
	XHTTP_ERROR_TOO_MANY_REDIRECTS,
	XHTTP_ERROR_REDIRECT_BODY_NOT_REPLAYABLE,
	XHTTP_ERROR_REDIRECT_DOWNGRADE,
	XHTTP_ERROR_INVALID_REDIRECT,
	XHTTP_ERROR_DECOMPRESSION
} xhttp_error_code;

/*
	Per-request diagnostic snapshot. Times are monotonic milliseconds and are
	intended for duration calculation only. A request-bound diagnostics object
	must outlive the async future and may be read after that future completes.
*/
typedef struct xrt_http_diagnostics {
	xhttp_error_code eError;
	xhttp_phase ePhase;
	xnet_result eTransportStatus;
	xcodechttp1error eProtocolError;
	size_t iProtocolOffset;
	uint32 iProtocolLine;
	int iSystemError;
	bool bReusedConnection;
	uint64 iStartedMs;
	uint64 iConnectedMs;
	uint64 iRequestSentMs;
	uint64 iFirstByteMs;
	uint64 iHeadersMs;
	uint64 iCompletedMs;
	uint64 iConnectDurationMs;
	uint64 iTimeToFirstByteMs;
	uint64 iTransferDurationMs;
	uint64 iTotalDurationMs;
	uint64 iRequestBytes;
	uint64 iResponseBodyBytes;
	uint32 iRedirectCount;
	uint64 iResponseWireBodyBytes;
} xhttpdiagnostics;

typedef struct xrt_http_request {
	char sMethod[XHTTP_METHOD_CAP];
	char sURL[XHTTP_URL_CAP];
	char* pMethodStorage;
	char* pURLStorage;
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
	/* Borrowed. When non-NULL it must outlive the async request. */
	xhttpdiagnostics* pDiagnostics;
	/* Retained body factory and parent context. Appended for source compatibility. */
	xhttpbody* pBodySource;
	xhttpcontext* pContext;
} xhttprequest;

typedef struct xrt_http_response {
	uint32 iStatusCode;
	uint32 iFlags;
	uint32 iHeaderCount;
	uint32 iHeaderCap;
	uint32 iTrailerCount;
	uint32 iTrailerCap;
	int64_t iContentLength;
	const char* sVersion;
	const char* sReason;
	xhttpheader arrHeaders[XHTTP_MAX_HEADERS];
	xhttpheader* pHeaders;
	xhttpheader arrTrailers[XHTTP_MAX_TRAILERS];
	xhttpheader* pTrailers;
	char* pStorage;
	char* pBody;
	size_t iBodyLen;
	size_t iBodyCap;
	xhttpdiagnostics tDiagnostics;
	char* sEffectiveURL;
	char* sOriginalContentEncoding;
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
	volatile long iActiveConnections;
	volatile long iIdleConnections;
	volatile int64 iRequestsStarted;
	volatile int64 iRequestsCompleted;
	volatile int64 iConnectionsOpened;
	volatile int64 iConnectionsReused;
	volatile int64 iConnectionsClosed;
	volatile int64 iRedirectsFollowed;
	volatile int64 iRequestBytes;
	volatile int64 iResponseBodyBytes;
	xnetengine* pEngine;
	struct __xhttp_conn* pIdleConnHead;
	xhttpclientconfig tConfig;
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
	size_t iSendOffset;
	xhttpbody* pUploadBody;
	xhttpbodyreader* pUploadReader;
	xhttpcontext* pContext;
	xnetcancelwatch* pCancelWatch;
	xnetfuture* pUploadWait;
	uint8* pUploadBuf;
	size_t iUploadBufCap;
	size_t iUploadPendingLen;
	int64_t iUploadContentLength;
	uint64 iUploadReadBytes;
	uint64 iRequestWireBytes;
	uint64 iResponseBodyBytesTotal;
	uint64 iEncodedBodyReceived;
	uint64 iBodyRemain;
	uint64 iChunkRemain;
	uint64 iBodyReceived;
	uint32 iStreamBodyMode;
	uint32 iStreamChunkState;
	uint32 iInformationalCount;
	uint32 iMaxInformationalResponses;
	uint32 iClientFlags;
	uint32 iMaxRedirects;
	uint32 iRedirectCount;
	xcodechttp1limits tHttp1Limits;
#ifndef XRT_NO_XINFLATE
	__xinflate* pResponseInflaters;
	uint32 iResponseInflaterCount;
	__xinflate_result eResponseInflateError;
#endif
	bool bStreaming;
	bool bStreamHeadersDelivered;
	bool bRedirectPending;
	bool bCookieHeaderFromJar;
	bool bUploadChunked;
	bool bUploadOpened;
	bool bUploadEof;
	bool bUploadWaiting;
	bool bUploadComplete;
	bool bUploadTerminalSent;
	int iLastSysErr;
	xhttpdiagnostics tDiagnostics;
	xtlsconfig tTlsCfg;
} __xhttp_tx;

typedef struct {
	__xhttp_tx* pTx;
} __xhttp_cancel_ctx;

typedef struct {
	__xhttp_tx* pTx;
	xnetfuture* pSource;
} __xhttp_upload_wait_ctx;

typedef struct {
	__xhttp_tx* pTx;
	long iGeneration;
} __xhttp_idle_timer_ctx;

typedef struct {
	xhttpclient* pClient;
	uint64 iConnectionId;
	long iGeneration;
} __xhttp_pool_timer_ctx;

#define __XHTTP_CONN_STATE_NONE   0
#define __XHTTP_CONN_STATE_ACTIVE 1
#define __XHTTP_CONN_STATE_IDLE   2

typedef struct __xhttp_conn {
	struct __xhttp_conn* pNext;
	volatile long iCleanupPosted;
	volatile long iPoolGeneration;
	volatile long iAccountingState;
	volatile long iOpenedCounted;
	volatile long iClosedCounted;
	uint64 iId;
	uint64 iIdleSinceMs;
	uint64 iPoolTimerId;
	xnetengine* pEngine;
	xhttpclient* pClient;
	xnetstream* pStream;
	__xhttp_tx* pTx;
	char* sHost;
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
static volatile int64 __g_xhttpNextConnectionId = 1;


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


static bool __xhttpStrEqViewNoCase(const char* sText, const char* sView, size_t iViewLen)
{
	size_t i;
	if ( !sText || (!sView && iViewLen > 0u) ) { return false; }
	for ( i = 0u; i < iViewLen; ++i ) {
		if ( sText[i] == '\0' || __xhttpToLower(sText[i]) != __xhttpToLower(sView[i]) ) { return false; }
	}
	return sText[i] == '\0';
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


static uint64 __xhttpNowMs(void)
{
	return __xnetSyncNowMs();
}


static void __xhttpDiagnosticsCompleteEarly(
	xhttpdiagnostics* pDiagnostics,
	xhttp_error_code eError,
	xhttp_phase ePhase,
	xnet_result eStatus)
{
	uint64 iNow;
	if ( !pDiagnostics ) { return; }
	iNow = __xhttpNowMs();
	if ( pDiagnostics->iStartedMs == 0u ) { pDiagnostics->iStartedMs = iNow; }
	pDiagnostics->eError = eError;
	pDiagnostics->ePhase = ePhase;
	pDiagnostics->eTransportStatus = eStatus;
	pDiagnostics->iCompletedMs = iNow;
	if ( iNow >= pDiagnostics->iStartedMs ) {
		pDiagnostics->iTotalDurationMs = iNow - pDiagnostics->iStartedMs;
	}
}


static void __xhttpDiagnosticsSetPhase(__xhttp_tx* pTx, xhttp_phase ePhase)
{
	if ( !pTx || __xhttpAtomicLoad(&pTx->iComplete) != 0 ) { return; }
	pTx->tDiagnostics.ePhase = ePhase;
}


static void __xhttpDiagnosticsSetError(__xhttp_tx* pTx, xhttp_error_code eError, xhttp_phase ePhase)
{
	if ( !pTx ) { return; }
	if ( pTx->tDiagnostics.eError == XHTTP_ERROR_NONE ) { pTx->tDiagnostics.eError = eError; }
	if ( ePhase != XHTTP_PHASE_NONE ) { pTx->tDiagnostics.ePhase = ePhase; }
}


// 内部函数：复制 Token
// 内部函数：__xhttpAppendBytes
#if defined(XRT_INTERNAL_TEST_ENV)
static void __xhttpCopyToken(char* sDst, size_t iDstCap, const char* sSrc)
{
	size_t iLen;
	if ( !sDst || iDstCap == 0u ) { return; }
	iLen = sSrc ? strlen(sSrc) : 0u;
	if ( iLen >= iDstCap ) { iLen = iDstCap - 1u; }
	if ( iLen > 0u ) { memcpy(sDst, sSrc, iLen); }
	sDst[iLen] = '\0';
}
#endif


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


static char* __xhttpStringCopyN(const char* sText, size_t iLen)
{
	char* sCopy;
	if ( !sText || iLen == SIZE_MAX ) { return NULL; }
	sCopy = (char*)XNET_ALLOC(iLen + 1u);
	if ( !sCopy ) { return NULL; }
	if ( iLen > 0u ) { memcpy(sCopy, sText, iLen); }
	sCopy[iLen] = '\0';
	return sCopy;
}


static const char* __xhttpRequestMethodText(const xhttprequest* pReq)
{
	return pReq ? (pReq->pMethodStorage ? pReq->pMethodStorage : pReq->sMethod) : NULL;
}


static const char* __xhttpRequestUrlText(const xhttprequest* pReq)
{
	return pReq ? (pReq->pURLStorage ? pReq->pURLStorage : pReq->sURL) : NULL;
}


static const char* __xhttpUrlHostText(const xhttpurl* pURL)
{
	return pURL ? (pURL->pHostStorage ? pURL->pHostStorage : pURL->sHost) : NULL;
}


static const char* __xhttpUrlPathText(const xhttpurl* pURL)
{
	return pURL ? (pURL->pPathStorage ? pURL->pPathStorage : pURL->sPath) : NULL;
}


static bool __xhttpStoreInlineText(char* sInline, size_t iInlineCap, char** ppStorage,
	const char* sText, size_t iLen)
{
	char* pNew = NULL;
	char* pOld;
	if ( !sInline || iInlineCap == 0u || !ppStorage || !sText || iLen == SIZE_MAX ) { return false; }
	if ( iLen >= iInlineCap ) {
		pNew = __xhttpStringCopyN(sText, iLen);
		if ( !pNew ) { return false; }
	} else {
		memmove(sInline, sText, iLen);
		sInline[iLen] = '\0';
	}
	pOld = *ppStorage;
	*ppStorage = pNew;
	if ( pNew ) { sInline[0] = '\0'; }
	if ( pOld ) { XNET_FREE(pOld); }
	return true;
}


static void __xhttpUrlUnit(xhttpurl* pURL)
{
	if ( !pURL ) { return; }
	if ( pURL->pHostStorage ) { XNET_FREE(pURL->pHostStorage); }
	if ( pURL->pPathStorage ) { XNET_FREE(pURL->pPathStorage); }
	memset(pURL, 0, sizeof(*pURL));
}


static bool __xhttpRequestStoreUrl(xhttprequest* pReq, const char* sURL)
{
	xrturlview tView;
	const char* sTarget;
	size_t iUrlLen;
	size_t iHostLen;
	size_t iTargetLen;
	char* pNewUrl = NULL;
	char* pNewHost = NULL;
	char* pNewPath = NULL;
	if ( !pReq || !sURL || !sURL[0] || !xrtUrlParseView(sURL, &tView) ||
		(tView.iFlags & XRT_URL_F_ABSOLUTE) == 0u ||
		!xrtUrlViewMatchesScheme2(&tView, "http", "https") ||
		xrtStrViewIsEmpty(tView.tHost) || (tView.iFlags & XRT_URL_F_HAS_USERINFO) != 0u ) { return false; }
	iUrlLen = strlen(sURL);
	iHostLen = tView.tHost.iLen;
	sTarget = !xrtStrViewIsEmpty(tView.tTarget) ? tView.tTarget.sPtr : "/";
	iTargetLen = !xrtStrViewIsEmpty(tView.tTarget) ? tView.tTarget.iLen : 1u;
	if ( iUrlLen >= sizeof(pReq->sURL) && !(pNewUrl = __xhttpStringCopyN(sURL, iUrlLen)) ) { goto fail; }
	if ( iHostLen >= sizeof(pReq->tURL.sHost) && !(pNewHost = __xhttpStringCopyN(tView.tHost.sPtr, iHostLen)) ) { goto fail; }
	if ( iTargetLen >= sizeof(pReq->tURL.sPath) && !(pNewPath = __xhttpStringCopyN(sTarget, iTargetLen)) ) { goto fail; }
	if ( !pNewUrl ) { memmove(pReq->sURL, sURL, iUrlLen); pReq->sURL[iUrlLen] = '\0'; }
	if ( !pNewHost ) { memmove(pReq->tURL.sHost, tView.tHost.sPtr, iHostLen); pReq->tURL.sHost[iHostLen] = '\0'; }
	if ( !pNewPath ) { memmove(pReq->tURL.sPath, sTarget, iTargetLen); pReq->tURL.sPath[iTargetLen] = '\0'; }
	if ( pReq->pURLStorage ) { XNET_FREE(pReq->pURLStorage); }
	if ( pReq->tURL.pHostStorage ) { XNET_FREE(pReq->tURL.pHostStorage); }
	if ( pReq->tURL.pPathStorage ) { XNET_FREE(pReq->tURL.pPathStorage); }
	pReq->pURLStorage = pNewUrl;
	pReq->tURL.pHostStorage = pNewHost;
	pReq->tURL.pPathStorage = pNewPath;
	if ( pNewUrl ) { pReq->sURL[0] = '\0'; }
	if ( pNewHost ) { pReq->tURL.sHost[0] = '\0'; }
	if ( pNewPath ) { pReq->tURL.sPath[0] = '\0'; }
	pReq->tURL.bHttps = xrtUrlViewIsScheme(&tView, "https");
	pReq->tURL.iPort = tView.iPort;
	return true;

fail:
	if ( pNewUrl ) { XNET_FREE(pNewUrl); }
	if ( pNewHost ) { XNET_FREE(pNewHost); }
	if ( pNewPath ) { XNET_FREE(pNewPath); }
	return false;
}


static void __xhttpHeaderUnit(xhttpheader* pHeader)
{
	if ( !pHeader ) { return; }
	if ( (pHeader->iFlags & XHTTP_HEADER_F_OWN_STRINGS) != 0u ) {
		if ( pHeader->sName ) { XNET_FREE((ptr)pHeader->sName); }
		if ( pHeader->sValue ) { XNET_FREE((ptr)pHeader->sValue); }
	}
	memset(pHeader, 0, sizeof(xhttpheader));
}


static bool __xhttpHeaderSetCopyN(xhttpheader* pHeader, const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen)
{
	char* sNameCopy;
	char* sValueCopy;
	if ( !pHeader || !sName || iNameLen == 0u || !sValue ) { return false; }
	sNameCopy = __xhttpStringCopyN(sName, iNameLen);
	if ( !sNameCopy ) { return false; }
	sValueCopy = __xhttpStringCopyN(sValue, iValueLen);
	if ( !sValueCopy ) {
		XNET_FREE(sNameCopy);
		return false;
	}
	__xhttpHeaderUnit(pHeader);
	pHeader->sName = sNameCopy;
	pHeader->sValue = sValueCopy;
	pHeader->iFlags = XHTTP_HEADER_F_OWN_STRINGS;
	return true;
}


static bool __xhttpHeaderSetCopy(xhttpheader* pHeader, const char* sName, const char* sValue)
{
	return sName && sValue
		? __xhttpHeaderSetCopyN(pHeader, sName, strlen(sName), sValue, strlen(sValue))
		: false;
}


static void __xhttpHeadersUnit(xhttpheader* pHeaders, uint32 iCount)
{
	if ( !pHeaders ) { return; }
	for ( uint32 i = 0u; i < iCount; ++i ) { __xhttpHeaderUnit(&pHeaders[i]); }
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


static void __xhttpDiagnosticsSetProtocolError(__xhttp_tx* pTx, const xcodechttp1errorinfo* pError, xhttp_phase ePhase)
{
	if ( !pTx ) { return; }
	__xhttpDiagnosticsSetError(pTx, XHTTP_ERROR_PROTOCOL, ePhase);
	if ( pError ) {
		pTx->tDiagnostics.eProtocolError = pError->eCode;
		pTx->tDiagnostics.iProtocolOffset = pError->iOffset;
		pTx->tDiagnostics.iProtocolLine = pError->iLine;
	}
}


static xhttpheader* __xhttpResponseTrailers(const xhttpresponse* pResp)
{
	if ( !pResp ) { return NULL; }
	return pResp->pTrailers ? pResp->pTrailers : (xhttpheader*)pResp->arrTrailers;
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


static bool __xhttpEnsureResponseTrailerCap(xhttpresponse* pResp, uint32 iNeed)
{
	xhttpheader* pNew;
	uint32 iNewCap;
	if ( !pResp ) { return false; }
	if ( pResp->pTrailers == NULL ) {
		pResp->pTrailers = pResp->arrTrailers;
		pResp->iTrailerCap = XHTTP_MAX_TRAILERS;
	}
	if ( iNeed <= pResp->iTrailerCap ) { return true; }
	iNewCap = pResp->iTrailerCap ? pResp->iTrailerCap : XHTTP_MAX_TRAILERS;
	while ( iNewCap < iNeed ) {
		if ( iNewCap > UINT32_MAX / 2u ) { return false; }
		iNewCap *= 2u;
	}
	pNew = (xhttpheader*)XNET_ALLOC(sizeof(xhttpheader) * iNewCap);
	if ( !pNew ) { return false; }
	memset(pNew, 0, sizeof(xhttpheader) * iNewCap);
	if ( pResp->iTrailerCount > 0u ) { memcpy(pNew, pResp->pTrailers, sizeof(xhttpheader) * pResp->iTrailerCount); }
	if ( pResp->pTrailers != pResp->arrTrailers ) { XNET_FREE(pResp->pTrailers); }
	pResp->pTrailers = pNew;
	pResp->iTrailerCap = iNewCap;
	return true;
}


static bool __xhttpValidFieldValue(const char* sValue)
{
	if ( !sValue ) { return false; }
	for ( const unsigned char* p = (const unsigned char*)sValue; *p; ++p ) {
		if ( *p == '\r' || *p == '\n' || *p == 0x7fu || (*p < 0x20u && *p != '\t') ) { return false; }
	}
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


static bool __xhttpIsRedirectStatus(uint32 iStatusCode)
{
	return iStatusCode == 301u || iStatusCode == 302u || iStatusCode == 303u ||
		iStatusCode == 307u || iStatusCode == 308u;
}


static uint32 __xhttpResponseHeaderNameCount(const xhttpresponse* pResp, const char* sName)
{
	xhttpheader* pHeaders;
	uint32 iCount = 0u;
	if ( !pResp || !sName ) { return 0u; }
	pHeaders = __xhttpResponseHeaders(pResp);
	for ( uint32 i = 0u; i < pResp->iHeaderCount; ++i ) {
		if ( pHeaders[i].sName && __xhttpStrEqNoCase(pHeaders[i].sName, sName) ) { iCount++; }
	}
	return iCount;
}


static uint32 __xhttpResponseRemoveHeader(xhttpresponse* pResp, const char* sName)
{
	xhttpheader* pHeaders;
	uint32 iWrite = 0u;
	uint32 iRemoved = 0u;
	if ( !pResp || !sName ) { return 0u; }
	pHeaders = __xhttpResponseHeaders(pResp);
	for ( uint32 iRead = 0u; iRead < pResp->iHeaderCount; ++iRead ) {
		if ( pHeaders[iRead].sName && __xhttpStrEqNoCase(pHeaders[iRead].sName, sName) ) {
			__xhttpHeaderUnit(&pHeaders[iRead]);
			iRemoved++;
			continue;
		}
		if ( iWrite != iRead ) {
			pHeaders[iWrite] = pHeaders[iRead];
			memset(&pHeaders[iRead], 0, sizeof(xhttpheader));
		}
		iWrite++;
	}
	pResp->iHeaderCount = iWrite;
	return iRemoved;
}


static bool __xhttpShouldAttemptRedirect(const __xhttp_tx* pTx, const xhttpresponse* pResp)
{
	return pTx && pResp &&
		(pTx->iClientFlags & XHTTP_CLIENT_F_FOLLOW_REDIRECTS) != 0u &&
		__xhttpIsRedirectStatus(pResp->iStatusCode) &&
		__xhttpResponseHeaderNameCount(pResp, "Location") > 0u;
}

static bool __xhttpRequestClone(xhttprequest* pDst, const xhttprequest* pSrc);
static void __xhttpRequestUnitInternal(xhttprequest* pReq);
#if defined(XRT_INTERNAL_TEST_ENV)
static bool __xhttpBuildRequestBytes(const xhttprequest* pReq, char** ppOut, size_t* pOutLen);
static bool __xhttpBuildRequestBytesEx(const xhttprequest* pReq, char** ppOut, size_t* pOutLen, xhttp_error_code* pError);
#endif
static bool __xhttpBuildRequestHeadEx(const xhttprequest* pReq, bool bHasBody,
	int64_t iBodyLength, char** ppOut, size_t* pOutLen, bool* pChunked, xhttp_error_code* pError);
static xhttpresponse* __xhttpBuildResponse(const xcodecframe* pFrame, const xcodechttp1msg* pMsg, const xnetchain* pChain);
static void __xhttpConnPostCleanup(__xhttp_conn* pConn);
static void __xhttpTxRefreshIdleTimeout(__xhttp_tx* pTx);
static uint32 __xhttpResolveConnectTimeoutMs(const xhttprequest* pReq);
static bool __xhttpPrepareTxContext(__xhttp_tx* pTx, xhttp_error_code* pError);
static bool __xhttpPrepareTxUpload(__xhttp_tx* pTx, xhttp_error_code* pError);
static bool __xhttpApplyClientCookies(__xhttp_tx* pTx, xhttp_error_code* pError);
static bool __xhttpTxStartHop(__xhttp_tx* pTx);
static bool __xhttpFollowRedirect(__xhttp_tx* pTx, const xhttpresponse* pResp);


// 内部函数：__xhttpPoolLockAcquire
static xhttpclient* __xhttpClientAddRef(xhttpclient* pClient)
{
	if ( pClient ) { (void)__xhttpAtomicAdd(&pClient->iRefCount, 1); }
	return pClient;
}


static void __xhttpClientRelease(xhttpclient* pClient)
{
	if ( !pClient ) { return; }
	if ( __xhttpAtomicAdd(&pClient->iRefCount, -1) == 0 ) {
		if ( pClient->tConfig.pCookieJar ) {
			xrtHttpCookieJarDestroy(pClient->tConfig.pCookieJar);
			pClient->tConfig.pCookieJar = NULL;
		}
		XNET_FREE(pClient);
	}
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
	const char* sURL;
	const char* sHost;
	if ( !pReq || !sOut || iOutCap == 0u ) { return false; }
	sURL = __xhttpRequestUrlText(pReq);
	if ( sURL && sURL[0] != '\0' && xrtUrlParseView(sURL, &tURL) ) {
		return xrtUrlMakeHostHeader(&tURL, sOut, iOutCap);
	}
	sHost = __xhttpUrlHostText(&pReq->tURL);
	if ( !sHost || sHost[0] == '\0' ) { return false; }
	return xrtUrlMakeHostHeaderFixed(pReq->tURL.bHttps ? "https" : "http", sHost, pReq->tURL.iPort, sOut, iOutCap);
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
// 内部函数：__xhttpConnMatchesReq
static void __xhttpClientAdd64(volatile int64* pCounter, uint64 iValue)
{
	if ( pCounter && iValue > 0u ) { (void)__xnetAtomicAddFetch64(pCounter, (int64)iValue); }
}


static void __xhttpConnAdjustCurrent(xhttpclient* pClient, long iState, long iDelta)
{
	if ( !pClient || iDelta == 0 ) { return; }
	if ( iState == __XHTTP_CONN_STATE_ACTIVE ) {
		(void)__xhttpAtomicAdd(&pClient->iActiveConnections, iDelta);
	} else if ( iState == __XHTTP_CONN_STATE_IDLE ) {
		(void)__xhttpAtomicAdd(&pClient->iIdleConnections, iDelta);
	}
}


static bool __xhttpConnTransition(__xhttp_conn* pConn, long iFrom, long iTo)
{
	if ( !pConn ) { return false; }
	if ( __xhttpAtomicCompareExchange(&pConn->iAccountingState, iTo, iFrom) != iFrom ) { return false; }
	__xhttpConnAdjustCurrent(pConn->pClient, iFrom, -1);
	__xhttpConnAdjustCurrent(pConn->pClient, iTo, 1);
	return true;
}


static void __xhttpConnTransitionToNone(__xhttp_conn* pConn)
{
	long iState;
	if ( !pConn ) { return; }
	for ( ;; ) {
		iState = __xhttpAtomicLoad(&pConn->iAccountingState);
		if ( iState == __XHTTP_CONN_STATE_NONE ) { return; }
		if ( __xhttpAtomicCompareExchange(&pConn->iAccountingState,
			__XHTTP_CONN_STATE_NONE, iState) == iState ) {
			__xhttpConnAdjustCurrent(pConn->pClient, iState, -1);
			return;
		}
	}
}


static uint32 __xhttpPoolMaxIdle(const xhttpclient* pClient)
{
	return pClient ? pClient->tConfig.iMaxIdleConnections : XHTTP_CLIENT_DEFAULT_MAX_IDLE_CONNECTIONS;
}


static uint32 __xhttpPoolMaxIdlePerOrigin(const xhttpclient* pClient)
{
	return pClient ? pClient->tConfig.iMaxIdleConnectionsPerOrigin :
		XHTTP_CLIENT_DEFAULT_MAX_IDLE_CONNECTIONS_PER_ORIGIN;
}


static uint32 __xhttpPoolIdleTimeoutMs(const xhttpclient* pClient)
{
	return pClient ? pClient->tConfig.iIdleConnectionTimeoutMs :
		XHTTP_CLIENT_DEFAULT_IDLE_CONNECTION_TIMEOUT_MS;
}


static bool __xhttpConnSameOrigin(const __xhttp_conn* pA, const __xhttp_conn* pB)
{
	return pA && pB && pA->pEngine == pB->pEngine && pA->iPort == pB->iPort &&
		pA->bHttps == pB->bHttps && pA->bVerifyPeer == pB->bVerifyPeer &&
		pA->pProxy == pB->pProxy && __xhttpStrEqNoCase(pA->sHost, pB->sHost);
}


static void __xhttpPoolCloseConn(__xhttp_conn* pConn)
{
	if ( !pConn ) { return; }
	if ( pConn->pStream ) { xrtNetStreamClose(pConn->pStream, XNET_CLOSE_F_ABORT); }
	else { __xhttpConnPostCleanup(pConn); }
}


static void __xhttpPoolTimerDiscard(ptr pArg)
{
	__xhttp_pool_timer_ctx* pCtx = (__xhttp_pool_timer_ctx*)pArg;
	if ( !pCtx ) { return; }
	if ( pCtx->pClient ) { __xhttpClientRelease(pCtx->pClient); }
	XNET_FREE(pCtx);
}


static void __xhttpPoolTimerTask(xnetworker* pWorker, ptr pArg)
{
	__xhttp_pool_timer_ctx* pCtx = (__xhttp_pool_timer_ctx*)pArg;
	__xhttp_conn** ppConn;
	__xhttp_conn* pExpired = NULL;
	(void)pWorker;
	if ( !pCtx ) { return; }
	__xhttpPoolLockAcquire(pCtx->pClient);
	ppConn = pCtx->pClient ? &pCtx->pClient->pIdleConnHead : &__g_xhttpIdleConnHead;
	while ( *ppConn ) {
		__xhttp_conn* pConn = *ppConn;
		if ( pConn->iId == pCtx->iConnectionId && pConn->bIdle &&
			__xhttpAtomicLoad(&pConn->iPoolGeneration) == pCtx->iGeneration ) {
			*ppConn = pConn->pNext;
			pConn->pNext = NULL;
			pConn->bIdle = false;
			pConn->iPoolTimerId = 0u;
			(void)__xhttpConnTransition(pConn, __XHTTP_CONN_STATE_IDLE, __XHTTP_CONN_STATE_NONE);
			pExpired = pConn;
			break;
		}
		ppConn = &pConn->pNext;
	}
	__xhttpPoolLockRelease(pCtx->pClient);
	if ( pExpired ) { __xhttpPoolCloseConn(pExpired); }
	__xhttpPoolTimerDiscard(pCtx);
}


static void __xhttpPoolCancelTimer(__xhttp_conn* pConn, uint64 iTimerId)
{
	if ( pConn && pConn->pEngine && iTimerId != 0u ) {
		(void)xrtNetEngineCancelTimer(pConn->pEngine, iTimerId);
	}
}


static bool __xhttpConnMatchesReq(const __xhttp_conn* pConn, xnetengine* pEngine, const xhttprequest* pReq)
{
	if ( !pConn || !pReq || !pEngine ) { return false; }
	if ( pConn->pEngine != pEngine ) { return false; }
	if ( pConn->iPort != pReq->tURL.iPort ) { return false; }
	if ( pConn->bHttps != pReq->tURL.bHttps ) { return false; }
	if ( pConn->bVerifyPeer != pReq->bVerifyPeer ) { return false; }
	if ( pConn->pProxy != pReq->pProxy ) { return false; }
	if ( !pConn->sHost || !__xhttpStrEqNoCase(pConn->sHost, __xhttpUrlHostText(&pReq->tURL)) ) { return false; }
	if ( !pConn->bOpen || pConn->pStream == NULL || pConn->pTx != NULL ) { return false; }
	return true;
}


// 内部函数：__xhttpPoolTake
static __xhttp_conn* __xhttpPoolTake(xhttpclient* pClient, xnetengine* pEngine, const xhttprequest* pReq)
{
	__xhttp_conn** ppConn;
	__xhttp_conn* pConn = NULL;
	__xhttp_conn* pExpired = NULL;
	uint32 iTimeoutMs;
	uint64 iNow;
	uint64 iTimerId = 0u;
	if ( !pEngine || !pReq ) { return NULL; }
	if ( pClient && __xhttpAtomicLoad(&pClient->iClosing) != 0 ) { return NULL; }
	iTimeoutMs = __xhttpPoolIdleTimeoutMs(pClient);
	iNow = __xhttpNowMs();
	__xhttpPoolLockAcquire(pClient);
	ppConn = pClient ? &pClient->pIdleConnHead : &__g_xhttpIdleConnHead;
	while ( *ppConn ) {
		__xhttp_conn* pCurrent = *ppConn;
		if ( iTimeoutMs > 0u && iNow >= pCurrent->iIdleSinceMs &&
			iNow - pCurrent->iIdleSinceMs >= (uint64)iTimeoutMs ) {
			*ppConn = pCurrent->pNext;
			pCurrent->pNext = pExpired;
			pExpired = pCurrent;
			pCurrent->bIdle = false;
			(void)__xhttpConnTransition(pCurrent, __XHTTP_CONN_STATE_IDLE, __XHTTP_CONN_STATE_NONE);
			continue;
		}
		if ( __xhttpConnMatchesReq(*ppConn, pEngine, pReq) ) {
			pConn = *ppConn;
			*ppConn = pConn->pNext;
			pConn->pNext = NULL;
			pConn->bIdle = false;
			iTimerId = pConn->iPoolTimerId;
			pConn->iPoolTimerId = 0u;
			if ( !__xhttpConnTransition(pConn, __XHTTP_CONN_STATE_IDLE, __XHTTP_CONN_STATE_ACTIVE) ) {
				pConn->pNext = pExpired;
				pExpired = pConn;
				pConn = NULL;
			}
			break;
		}
		ppConn = &(*ppConn)->pNext;
	}
	__xhttpPoolLockRelease(pClient);
	if ( pConn && pClient ) { __xhttpClientAdd64(&pClient->iConnectionsReused, 1u); }
	if ( iTimerId != 0u ) { (void)xrtNetEngineCancelTimer(pEngine, iTimerId); }
	while ( pExpired ) {
		__xhttp_conn* pNext = pExpired->pNext;
		uint64 iExpiredTimerId = pExpired->iPoolTimerId;
		pExpired->pNext = NULL;
		pExpired->iPoolTimerId = 0u;
		__xhttpPoolCancelTimer(pExpired, iExpiredTimerId);
		__xhttpPoolCloseConn(pExpired);
		pExpired = pNext;
	}
	return pConn;
}


// 内部函数：__xhttpPoolPut
static bool __xhttpPoolPut(__xhttp_conn* pConn)
{
	xhttpclient* pClient;
	__xhttp_conn** ppHead;
	__xhttp_conn** ppCur;
	__xhttp_conn** ppOldest = NULL;
	__xhttp_conn** ppOldestOrigin = NULL;
	__xhttp_conn** ppEvict = NULL;
	__xhttp_conn* pEvicted = NULL;
	__xhttp_pool_timer_ctx* pTimerCtx = NULL;
	uint32 iMaxIdle;
	uint32 iMaxPerOrigin;
	uint32 iTimeoutMs;
	uint32 iTotal = 0u;
	uint32 iOrigin = 0u;
	uint32 iAffinity = 0u;
	uint64 iTimerId = 0u;
	long iGeneration;
	if ( !pConn || !pConn->pStream || !pConn->bOpen || pConn->pTx != NULL ) { return false; }
	pClient = pConn->pClient;
	iMaxIdle = __xhttpPoolMaxIdle(pClient);
	iMaxPerOrigin = __xhttpPoolMaxIdlePerOrigin(pClient);
	iTimeoutMs = __xhttpPoolIdleTimeoutMs(pClient);
	if ( iMaxIdle == 0u || iMaxPerOrigin == 0u ) { return false; }
	if ( pClient && __xhttpAtomicLoad(&pClient->iClosing) != 0 ) { return false; }
	if ( iTimeoutMs > 0u ) {
		pTimerCtx = (__xhttp_pool_timer_ctx*)XNET_ALLOC(sizeof(*pTimerCtx));
		if ( !pTimerCtx ) { return false; }
		memset(pTimerCtx, 0, sizeof(*pTimerCtx));
		pTimerCtx->pClient = __xhttpClientAddRef(pClient);
	}
	__xhttpPoolLockAcquire(pClient);
	if ( pClient && __xhttpAtomicLoad(&pClient->iClosing) != 0 ) {
		__xhttpPoolLockRelease(pClient);
		__xhttpPoolTimerDiscard(pTimerCtx);
		return false;
	}
	ppHead = pClient ? &pClient->pIdleConnHead : &__g_xhttpIdleConnHead;
	iGeneration = __xhttpAtomicAdd(&pConn->iPoolGeneration, 1);
	if ( pTimerCtx ) {
		pTimerCtx->iConnectionId = pConn->iId;
		pTimerCtx->iGeneration = iGeneration;
		if ( pConn->pStream->pWorker ) { iAffinity = pConn->pStream->pWorker->iId; }
		if ( __xnetEngineScheduleTimerOwned(pConn->pEngine, iAffinity, iTimeoutMs,
			__xhttpPoolTimerTask, __xhttpPoolTimerDiscard, pTimerCtx, &iTimerId) != XRT_NET_OK ) {
			__xhttpPoolLockRelease(pClient);
			__xhttpPoolTimerDiscard(pTimerCtx);
			return false;
		}
	}
	if ( !__xhttpConnTransition(pConn, __XHTTP_CONN_STATE_ACTIVE, __XHTTP_CONN_STATE_IDLE) ) {
		__xhttpPoolLockRelease(pClient);
		if ( iTimerId != 0u ) { (void)xrtNetEngineCancelTimer(pConn->pEngine, iTimerId); }
		return false;
	}
	ppCur = ppHead;
	while ( *ppCur ) {
		__xhttp_conn* pCurrent = *ppCur;
		iTotal++;
		ppOldest = ppCur;
		if ( __xhttpConnSameOrigin(pCurrent, pConn) ) {
			iOrigin++;
			ppOldestOrigin = ppCur;
		}
		ppCur = &pCurrent->pNext;
	}
	if ( iOrigin >= iMaxPerOrigin ) { ppEvict = ppOldestOrigin; }
	else if ( iTotal >= iMaxIdle ) { ppEvict = ppOldest; }
	if ( ppEvict && *ppEvict ) {
		pEvicted = *ppEvict;
		*ppEvict = pEvicted->pNext;
		pEvicted->pNext = NULL;
		pEvicted->bIdle = false;
		(void)__xhttpConnTransition(pEvicted, __XHTTP_CONN_STATE_IDLE, __XHTTP_CONN_STATE_NONE);
	}
	pConn->pNext = *ppHead;
	*ppHead = pConn;
	pConn->bIdle = true;
	pConn->iIdleSinceMs = __xhttpNowMs();
	pConn->iPoolTimerId = iTimerId;
	__xhttpPoolLockRelease(pClient);
	if ( pEvicted ) {
		uint64 iEvictedTimerId = pEvicted->iPoolTimerId;
		pEvicted->iPoolTimerId = 0u;
		__xhttpPoolCancelTimer(pEvicted, iEvictedTimerId);
		__xhttpPoolCloseConn(pEvicted);
	}
	return true;
}


// 内部函数：删除内存池
static void __xhttpPoolRemove(__xhttp_conn* pConn)
{
	__xhttp_conn** ppCur;
	xhttpclient* pClient;
	uint64 iTimerId = 0u;
	if ( !pConn ) { return; }
	pClient = pConn->pClient;
	__xhttpPoolLockAcquire(pClient);
	ppCur = pClient ? &pClient->pIdleConnHead : &__g_xhttpIdleConnHead;
	while ( *ppCur ) {
		if ( *ppCur == pConn ) {
			*ppCur = pConn->pNext;
			pConn->pNext = NULL;
			pConn->bIdle = false;
			iTimerId = pConn->iPoolTimerId;
			pConn->iPoolTimerId = 0u;
			(void)__xhttpConnTransition(pConn, __XHTTP_CONN_STATE_IDLE, __XHTTP_CONN_STATE_NONE);
			break;
		}
		ppCur = &(*ppCur)->pNext;
	}
	__xhttpPoolLockRelease(pClient);
	__xhttpPoolCancelTimer(pConn, iTimerId);
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
		(void)__xhttpConnTransition(pConn, __XHTTP_CONN_STATE_IDLE, __XHTTP_CONN_STATE_NONE);
		*ppTail = pConn;
		ppTail = &pConn->pNext;
	}
	__xhttpPoolLockRelease(pClient);
	// 逐个关闭并清理收集到的连接
	while ( pList ) {
		__xhttp_conn* pNext = pList->pNext;
		uint64 iTimerId = pList->iPoolTimerId;
		pList->pNext = NULL;
		pList->iPoolTimerId = 0u;
		__xhttpPoolCancelTimer(pList, iTimerId);
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


XXAPI void xrtHttpDiagnosticsInit(xhttpdiagnostics* pDiagnostics)
{
	if ( !pDiagnostics ) { return; }
	memset(pDiagnostics, 0, sizeof(xhttpdiagnostics));
	pDiagnostics->eTransportStatus = XRT_NET_AGAIN;
}


XXAPI const char* xrtHttpErrorCodeName(xhttp_error_code eError)
{
	switch ( eError ) {
		case XHTTP_ERROR_NONE: return "none";
		case XHTTP_ERROR_INVALID_ARGUMENT: return "invalid_argument";
		case XHTTP_ERROR_OUT_OF_MEMORY: return "out_of_memory";
		case XHTTP_ERROR_INVALID_URL: return "invalid_url";
		case XHTTP_ERROR_CONNECT: return "connect";
		case XHTTP_ERROR_WRITE: return "write";
		case XHTTP_ERROR_PROTOCOL: return "protocol";
		case XHTTP_ERROR_TOO_MANY_INFORMATIONAL_RESPONSES: return "too_many_informational_responses";
		case XHTTP_ERROR_CALLBACK_ABORT: return "callback_abort";
		case XHTTP_ERROR_TIMEOUT_TOTAL: return "timeout_total";
		case XHTTP_ERROR_TIMEOUT_IDLE: return "timeout_idle";
		case XHTTP_ERROR_CANCELLED: return "cancelled";
		case XHTTP_ERROR_CONNECTION_CLOSED: return "connection_closed";
		case XHTTP_ERROR_TRANSPORT: return "transport";
		case XHTTP_ERROR_BODY: return "body";
		case XHTTP_ERROR_TOO_MANY_REDIRECTS: return "too_many_redirects";
		case XHTTP_ERROR_REDIRECT_BODY_NOT_REPLAYABLE: return "redirect_body_not_replayable";
		case XHTTP_ERROR_REDIRECT_DOWNGRADE: return "redirect_downgrade";
		case XHTTP_ERROR_INVALID_REDIRECT: return "invalid_redirect";
		case XHTTP_ERROR_DECOMPRESSION: return "decompression";
		default: return "unknown";
	}
}


XXAPI const char* xrtHttpPhaseName(xhttp_phase ePhase)
{
	switch ( ePhase ) {
		case XHTTP_PHASE_NONE: return "none";
		case XHTTP_PHASE_PREPARE: return "prepare";
		case XHTTP_PHASE_CONNECT: return "connect";
		case XHTTP_PHASE_WRITE: return "write";
		case XHTTP_PHASE_RESPONSE_HEADERS: return "response_headers";
		case XHTTP_PHASE_RESPONSE_BODY: return "response_body";
		case XHTTP_PHASE_COMPLETE: return "complete";
		default: return "unknown";
	}
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
	xhttpheader* pHeaders;
	if ( !pReq ) { return; }
	pHeaders = __xhttpRequestHeaders(pReq);
	__xhttpHeadersUnit(pHeaders, pReq->iHeaderCount);
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
	if ( pReq->pBodySource ) {
		xrtHttpBodyRelease(pReq->pBodySource);
		pReq->pBodySource = NULL;
	}
	if ( pReq->pContext ) {
		xrtHttpContextRelease(pReq->pContext);
		pReq->pContext = NULL;
	}
	if ( pReq->pMethodStorage ) { XNET_FREE(pReq->pMethodStorage); pReq->pMethodStorage = NULL; }
	if ( pReq->pURLStorage ) { XNET_FREE(pReq->pURLStorage); pReq->pURLStorage = NULL; }
	__xhttpUrlUnit(&pReq->tURL);
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
	if ( !xrtHttpIsTokenN(sMethod, iLen) ) { return false; }
	return __xhttpStoreInlineText(pReq->sMethod, sizeof(pReq->sMethod), &pReq->pMethodStorage, sMethod, iLen);
}


// 设置 HTTP request URL
XXAPI bool xrtHttpRequestSetURL(xhttprequest* pReq, const char* sURL)
{
	if ( !pReq || !sURL || !sURL[0] ) { return false; }
	return __xhttpRequestStoreUrl(pReq, sURL);
}


XXAPI const char* xrtHttpRequestMethod(const xhttprequest* pReq)
{
	return __xhttpRequestMethodText(pReq);
}


XXAPI const char* xrtHttpRequestURL(const xhttprequest* pReq)
{
	return __xhttpRequestUrlText(pReq);
}


XXAPI const char* xrtHttpRequestHost(const xhttprequest* pReq)
{
	return pReq ? __xhttpUrlHostText(&pReq->tURL) : NULL;
}


XXAPI const char* xrtHttpRequestTarget(const xhttprequest* pReq)
{
	return pReq ? __xhttpUrlPathText(&pReq->tURL) : NULL;
}


// 设置 HTTP request 头部
XXAPI bool xrtHttpRequestSetHeader(xhttprequest* pReq, const char* sName, const char* sValue)
{
	xhttpheader* pHeader;
	xhttpheader* pHeaders;
	if ( !pReq || !sName || !sName[0] || !sValue || !xrtHttpIsToken(sName) || !__xhttpValidFieldValue(sValue) ) { return false; }
	pHeaders = __xhttpRequestHeaders(pReq);
	for ( uint32 i = 0; i < pReq->iHeaderCount; ++i ) {
		if ( __xhttpStrEqNoCase(pHeaders[i].sName, sName) ) {
			return __xhttpHeaderSetCopy(&pHeaders[i], sName, sValue);
		}
	}
	if ( !__xhttpEnsureRequestHeaderCap(pReq, pReq->iHeaderCount + 1u) ) { return false; }
	pHeader = &pReq->pHeaders[pReq->iHeaderCount];
	if ( !__xhttpHeaderSetCopy(pHeader, sName, sValue) ) { return false; }
	pReq->iHeaderCount++;
	return true;
}


XXAPI bool xrtHttpRequestAddHeader(xhttprequest* pReq, const char* sName, const char* sValue)
{
	xhttpheader* pHeader;
	if ( !pReq || !sName || !sName[0] || !sValue || !xrtHttpIsToken(sName) || !__xhttpValidFieldValue(sValue) ) { return false; }
	if ( !__xhttpEnsureRequestHeaderCap(pReq, pReq->iHeaderCount + 1u) ) { return false; }
	pHeader = &pReq->pHeaders[pReq->iHeaderCount];
	if ( !__xhttpHeaderSetCopy(pHeader, sName, sValue) ) { return false; }
	pReq->iHeaderCount++;
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
			__xhttpHeaderUnit(&pReq->pHeaders[iRead]);
			iRemoved++;
			continue;
		}
		if ( iWrite != iRead ) {
			pReq->pHeaders[iWrite] = pReq->pHeaders[iRead];
			memset(&pReq->pHeaders[iRead], 0, sizeof(xhttpheader));
		}
		iWrite++;
	}
	if ( iWrite < pReq->iHeaderCount ) {
		memset(pReq->pHeaders + iWrite, 0, sizeof(xhttpheader) * (pReq->iHeaderCount - iWrite));
	}
	pReq->iHeaderCount = iWrite;
	return iRemoved;
}


XXAPI const char* xrtHttpRequestHeader(const xhttprequest* pReq, const char* sName)
{
	return __xhttpRequestHeaderValue(pReq, sName);
}


XXAPI uint32 xrtHttpRequestHeaderCount(const xhttprequest* pReq)
{
	return pReq ? pReq->iHeaderCount : 0u;
}


XXAPI const char* xrtHttpRequestHeaderNameAt(const xhttprequest* pReq, uint32 iIndex)
{
	xhttpheader* pHeaders = __xhttpRequestHeaders(pReq);
	return pHeaders && iIndex < pReq->iHeaderCount ? pHeaders[iIndex].sName : NULL;
}


XXAPI const char* xrtHttpRequestHeaderValueAt(const xhttprequest* pReq, uint32 iIndex)
{
	xhttpheader* pHeaders = __xhttpRequestHeaders(pReq);
	return pHeaders && iIndex < pReq->iHeaderCount ? pHeaders[iIndex].sValue : NULL;
}


// xrtHttpRequestSetBodyCopy 相关处理
XXAPI bool xrtHttpRequestSetBodyCopy(xhttprequest* pReq, const void* pData, size_t iLen, const char* sContentType)
{
	char* pBodyCopy = NULL;
	if ( !pReq || (!pData && iLen > 0u) ) { return false; }
	if ( pData && iLen > 0 ) {
		pBodyCopy = (char*)XNET_ALLOC(iLen);
		if ( !pBodyCopy ) { return false; }
		memcpy(pBodyCopy, pData, iLen);
	}
	if ( sContentType && sContentType[0] ) {
		if ( !xrtHttpRequestSetHeader(pReq, "Content-Type", sContentType) ) {
			XNET_FREE(pBodyCopy);
			return false;
		}
	}
	if ( pReq->pBodySource ) { xrtHttpBodyRelease(pReq->pBodySource); }
	if ( pReq->pBody ) { XNET_FREE(pReq->pBody); }
	pReq->pBodySource = NULL;
	pReq->pBody = pBodyCopy;
	pReq->iBodyLen = pBodyCopy ? iLen : 0u;
	return true;
}


XXAPI bool xrtHttpRequestSetAuthorization(xhttprequest* pReq, const char* sScheme, const char* sCredentials)
{
	char* sValue;
	size_t iSchemeLen;
	size_t iCredentialsLen;
	bool bSet;
	if ( !pReq || !sScheme || !xrtHttpIsToken(sScheme) || !sCredentials || !sCredentials[0] ||
		!__xhttpValidFieldValue(sCredentials) ) { return false; }
	iSchemeLen = strlen(sScheme);
	iCredentialsLen = strlen(sCredentials);
	if ( iSchemeLen > SIZE_MAX - iCredentialsLen - 2u ) { return false; }
	sValue = (char*)XNET_ALLOC(iSchemeLen + iCredentialsLen + 2u);
	if ( !sValue ) { return false; }
	memcpy(sValue, sScheme, iSchemeLen);
	sValue[iSchemeLen] = ' ';
	memcpy(sValue + iSchemeLen + 1u, sCredentials, iCredentialsLen);
	sValue[iSchemeLen + iCredentialsLen + 1u] = '\0';
	bSet = xrtHttpRequestSetHeader(pReq, "Authorization", sValue);
	XNET_FREE(sValue);
	return bSet;
}


XXAPI bool xrtHttpRequestSetBasicAuth(xhttprequest* pReq, const char* sUser, const char* sPassword)
{
	static const char sTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	uint8* pPlain;
	char* sEncoded;
	size_t iUserLen;
	size_t iPasswordLen;
	size_t iPlainLen;
	size_t iBlocks;
	size_t iEncodedLen;
	size_t iIn = 0u;
	size_t iOut = 0u;
	bool bSet;
	if ( !pReq || !sUser || !sPassword || strchr(sUser, ':') != NULL ) { return false; }
	iUserLen = strlen(sUser);
	iPasswordLen = strlen(sPassword);
	if ( iUserLen > SIZE_MAX - iPasswordLen - 1u ) { return false; }
	iPlainLen = iUserLen + iPasswordLen + 1u;
	if ( iPlainLen > SIZE_MAX - 2u ) { return false; }
	iBlocks = (iPlainLen + 2u) / 3u;
	if ( iBlocks > (SIZE_MAX - 1u) / 4u ) { return false; }
	iEncodedLen = iBlocks * 4u;
	pPlain = (uint8*)XNET_ALLOC(iPlainLen);
	sEncoded = (char*)XNET_ALLOC(iEncodedLen + 1u);
	if ( !pPlain || !sEncoded ) {
		if ( pPlain ) { XNET_FREE(pPlain); }
		if ( sEncoded ) { XNET_FREE(sEncoded); }
		return false;
	}
	if ( iUserLen > 0u ) { memcpy(pPlain, sUser, iUserLen); }
	pPlain[iUserLen] = ':';
	if ( iPasswordLen > 0u ) { memcpy(pPlain + iUserLen + 1u, sPassword, iPasswordLen); }
	while ( iIn < iPlainLen ) {
		size_t iRemaining = iPlainLen - iIn;
		uint32 iA = pPlain[iIn++];
		uint32 iB = iIn < iPlainLen ? pPlain[iIn++] : 0u;
		uint32 iC = iIn < iPlainLen ? pPlain[iIn++] : 0u;
		uint32 iTriple = (iA << 16u) | (iB << 8u) | iC;
		sEncoded[iOut++] = sTable[(iTriple >> 18u) & 63u];
		sEncoded[iOut++] = sTable[(iTriple >> 12u) & 63u];
		sEncoded[iOut++] = iRemaining > 1u ? sTable[(iTriple >> 6u) & 63u] : '=';
		sEncoded[iOut++] = iRemaining > 2u ? sTable[iTriple & 63u] : '=';
	}
	sEncoded[iOut] = '\0';
	bSet = xrtHttpRequestSetAuthorization(pReq, "Basic", sEncoded);
	{
		volatile uint8* pWipe = (volatile uint8*)pPlain;
		for ( size_t i = 0u; i < iPlainLen; ++i ) { pWipe[i] = 0u; }
	}
	XNET_FREE(pPlain);
	XNET_FREE(sEncoded);
	return bSet;
}


XXAPI bool xrtHttpRequestSetBearerAuth(xhttprequest* pReq, const char* sToken)
{
	return xrtHttpRequestSetAuthorization(pReq, "Bearer", sToken);
}


XXAPI void xrtHttpRequestClearAuthorization(xhttprequest* pReq)
{
	if ( pReq ) { (void)xrtHttpRequestRemoveHeader(pReq, "Authorization"); }
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


// 设置请求代理；请求对象持有独立引用，调用方可立即释放原代理
XXAPI void xrtHttpRequestSetProxy(xhttprequest* pReq, xnetproxy* pProxy)
{
	xnetproxy* pNewProxy;
	if ( !pReq || pReq->pProxy == pProxy ) { return; }
	pNewProxy = pProxy ? xrtNetProxyAddRef(pProxy) : NULL;
	if ( pReq->pProxy ) { xrtNetProxyRelease(pReq->pProxy); }
	pReq->pProxy = pNewProxy;
}


XXAPI void xrtHttpRequestSetDiagnostics(xhttprequest* pReq, xhttpdiagnostics* pDiagnostics)
{
	if ( !pReq ) { return; }
	pReq->pDiagnostics = pDiagnostics;
	if ( pDiagnostics ) { xrtHttpDiagnosticsInit(pDiagnostics); }
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
	xhttpheader* pHeaders;
	xhttpheader* pTrailers;
	if ( !pResp ) { return; }
	pHeaders = __xhttpResponseHeaders(pResp);
	__xhttpHeadersUnit(pHeaders, pResp->iHeaderCount);
	pTrailers = __xhttpResponseTrailers(pResp);
	__xhttpHeadersUnit(pTrailers, pResp->iTrailerCount);
	if ( pResp->pHeaders && pResp->pHeaders != pResp->arrHeaders ) {
		XNET_FREE(pResp->pHeaders);
		pResp->pHeaders = NULL;
	}
	if ( pResp->pTrailers && pResp->pTrailers != pResp->arrTrailers ) {
		XNET_FREE(pResp->pTrailers);
		pResp->pTrailers = NULL;
	}
	if ( pResp->pStorage ) { XNET_FREE(pResp->pStorage); pResp->pStorage = NULL; }
	if ( pResp->pBody ) {
		XNET_FREE(pResp->pBody);
		pResp->pBody = NULL;
	}
	if ( pResp->sEffectiveURL ) {
		XNET_FREE(pResp->sEffectiveURL);
		pResp->sEffectiveURL = NULL;
	}
	if ( pResp->sOriginalContentEncoding ) {
		XNET_FREE(pResp->sOriginalContentEncoding);
		pResp->sOriginalContentEncoding = NULL;
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


XXAPI uint32 xrtHttpResponseFlags(const xhttpresponse* pResp)
{
	return pResp ? pResp->iFlags : XHTTP_RESP_F_NONE;
}


XXAPI const char* xrtHttpResponseVersion(const xhttpresponse* pResp)
{
	return pResp ? pResp->sVersion : NULL;
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


XXAPI const char* xrtHttpResponseTrailer(const xhttpresponse* pResp, const char* sName)
{
	xhttpheader* pTrailers;
	if ( !pResp || !sName ) { return NULL; }
	pTrailers = __xhttpResponseTrailers(pResp);
	for ( uint32 i = 0u; i < pResp->iTrailerCount; ++i ) {
		if ( __xhttpStrEqNoCase(pTrailers[i].sName, sName) ) { return pTrailers[i].sValue; }
	}
	return NULL;
}


static uint32 __xhttpRequestHeaderNameCount(const xhttprequest* pReq, const char* sName)
{
	xhttpheader* pHeaders;
	uint32 iCount = 0u;
	if ( !pReq || !sName ) { return 0u; }
	pHeaders = __xhttpRequestHeaders(pReq);
	for ( uint32 i = 0u; i < pReq->iHeaderCount; ++i ) {
		if ( __xhttpStrEqNoCase(pHeaders[i].sName, sName) ) { iCount++; }
	}
	return iCount;
}


XXAPI uint32 xrtHttpResponseTrailerCount(const xhttpresponse* pResp)
{
	return pResp ? pResp->iTrailerCount : 0u;
}


XXAPI const char* xrtHttpResponseTrailerNameAt(const xhttpresponse* pResp, uint32 iIndex)
{
	xhttpheader* pTrailers = __xhttpResponseTrailers(pResp);
	return pTrailers && iIndex < pResp->iTrailerCount ? pTrailers[iIndex].sName : NULL;
}


XXAPI const char* xrtHttpResponseTrailerValueAt(const xhttpresponse* pResp, uint32 iIndex)
{
	xhttpheader* pTrailers = __xhttpResponseTrailers(pResp);
	return pTrailers && iIndex < pResp->iTrailerCount ? pTrailers[iIndex].sValue : NULL;
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


XXAPI const xhttpdiagnostics* xrtHttpResponseDiagnostics(const xhttpresponse* pResp)
{
	return pResp ? &pResp->tDiagnostics : NULL;
}


XXAPI const char* xrtHttpResponseURL(const xhttpresponse* pResp)
{
	return pResp ? pResp->sEffectiveURL : NULL;
}


XXAPI const char* xrtHttpResponseOriginalContentEncoding(const xhttpresponse* pResp)
{
	return pResp ? pResp->sOriginalContentEncoding : NULL;
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
	const char* sMethod;
	const char* sURL;
	if ( !pDst || !pSrc ) { return false; }
	xrtHttpRequestInit(pDst);
	sMethod = __xhttpRequestMethodText(pSrc);
	sURL = __xhttpRequestUrlText(pSrc);
	if ( !sMethod || !xrtHttpRequestSetMethod(pDst, sMethod) ) { goto fail; }
	if ( sURL && sURL[0] ) {
		if ( !xrtHttpRequestSetURL(pDst, sURL) ) { goto fail; }
	} else {
		const char* sHost = __xhttpUrlHostText(&pSrc->tURL);
		const char* sPath = __xhttpUrlPathText(&pSrc->tURL);
		if ( sHost && sHost[0] ) {
			if ( !sPath || !sPath[0] ||
				!__xhttpStoreInlineText(pDst->tURL.sHost, sizeof(pDst->tURL.sHost), &pDst->tURL.pHostStorage, sHost, strlen(sHost)) ||
				!__xhttpStoreInlineText(pDst->tURL.sPath, sizeof(pDst->tURL.sPath), &pDst->tURL.pPathStorage, sPath, strlen(sPath)) ) { goto fail; }
			pDst->tURL.bHttps = pSrc->tURL.bHttps;
			pDst->tURL.iPort = pSrc->tURL.iPort;
		}
	}
	pSrcHeaders = __xhttpRequestHeaders(pSrc);
	if ( !__xhttpEnsureRequestHeaderCap(pDst, pSrc->iHeaderCount) ) { goto fail; }
	for ( uint32 i = 0u; i < pSrc->iHeaderCount; ++i ) {
		if ( !__xhttpHeaderSetCopy(&pDst->pHeaders[i], pSrcHeaders[i].sName, pSrcHeaders[i].sValue) ) {
			pDst->iHeaderCount = i;
			goto fail;
		}
		pDst->iHeaderCount = i + 1u;
	}
	pDst->iTimeoutMs = pSrc->iTimeoutMs;
	pDst->iIdleTimeoutMs = pSrc->iIdleTimeoutMs;
	pDst->bVerifyPeer = pSrc->bVerifyPeer;
	pDst->pProxy = pSrc->pProxy ? xrtNetProxyAddRef(pSrc->pProxy) : NULL;
	pDst->pDiagnostics = pSrc->pDiagnostics;
	pDst->pBodySource = pSrc->pBodySource ? xrtHttpBodyRetain(pSrc->pBodySource) : NULL;
	if ( pSrc->pBodySource && !pDst->pBodySource ) { goto fail; }
	pDst->pContext = pSrc->pContext ? xrtHttpContextRetain(pSrc->pContext) : NULL;
	if ( pSrc->pContext && !pDst->pContext ) { goto fail; }
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

fail:
	__xhttpRequestUnitInternal(pDst);
	return false;
}


static bool __xhttpBuildRequestHeadEx(const xhttprequest* pReq, bool bHasBody,
	int64_t iBodyLength, char** ppOut, size_t* pOutLen, bool* pChunked, xhttp_error_code* pError)
{
	char* pBuf = NULL;
	xhttpheader* pHeaders;
	const char* sMethod;
	const char* sTarget;
	const char* sHostValue;
	const char* sTransferEncoding;
	const char* sContentLength;
	const char* sHostHeaderValue;
	size_t iLen = 0u;
	size_t iCap = 0u;
	char aLine[96];
	bool bUseChunked = false;
	bool bParsedLength = false;
	int64_t iDeclaredLength = -1;

	if ( pError ) { *pError = XHTTP_ERROR_INVALID_ARGUMENT; }
	if ( ppOut ) { *ppOut = NULL; }
	if ( pOutLen ) { *pOutLen = 0u; }
	if ( pChunked ) { *pChunked = false; }
	if ( !pReq || !ppOut || !pOutLen || !pChunked || iBodyLength < XHTTP_BODY_LENGTH_UNKNOWN ||
		(!bHasBody && iBodyLength == XHTTP_BODY_LENGTH_UNKNOWN) ) { return false; }
	sMethod = __xhttpRequestMethodText(pReq);
	sTarget = __xhttpUrlPathText(&pReq->tURL);
	sHostValue = __xhttpUrlHostText(&pReq->tURL);
	if ( !sMethod || !sMethod[0] || !xrtHttpIsToken(sMethod) || !sTarget || !sTarget[0] ||
		strchr(sTarget, '\r') || strchr(sTarget, '\n') || strchr(sTarget, ' ') ||
		!sHostValue || !sHostValue[0] ) { return false; }

	pHeaders = __xhttpRequestHeaders(pReq);
	for ( uint32 i = 0u; i < pReq->iHeaderCount; ++i ) {
		if ( !pHeaders[i].sName || !pHeaders[i].sValue || !xrtHttpIsToken(pHeaders[i].sName) ||
			!__xhttpValidFieldValue(pHeaders[i].sValue) ) { return false; }
	}
	if ( __xhttpRequestHeaderNameCount(pReq, "Host") > 1u ||
		__xhttpRequestHeaderNameCount(pReq, "Content-Length") > 1u ||
		__xhttpRequestHeaderNameCount(pReq, "Transfer-Encoding") > 1u ) { return false; }

	sTransferEncoding = __xhttpRequestHeaderValue(pReq, "Transfer-Encoding");
	sContentLength = __xhttpRequestHeaderValue(pReq, "Content-Length");
	sHostHeaderValue = __xhttpRequestHeaderValue(pReq, "Host");
	if ( sHostHeaderValue ) {
		xrturlview tAuthority;
		if ( !sHostHeaderValue[0] || strchr(sHostHeaderValue, ',') != NULL ||
			!xrtUrlParseAuthority(sHostHeaderValue, &tAuthority) ||
			(tAuthority.iFlags & XRT_URL_F_HAS_USERINFO) != 0u ) { return false; }
	}
	if ( sTransferEncoding ) {
		const char* sBegin = sTransferEncoding;
		const char* sEnd = sTransferEncoding + strlen(sTransferEncoding);
		while ( sBegin < sEnd && (*sBegin == ' ' || *sBegin == '\t') ) { sBegin++; }
		while ( sEnd > sBegin && (sEnd[-1] == ' ' || sEnd[-1] == '\t') ) { sEnd--; }
		if ( !__xcodecHttpTokenEqNoCase(sBegin, (size_t)(sEnd - sBegin), "chunked") ) { return false; }
		bUseChunked = true;
	}
	if ( sContentLength ) {
		if ( bUseChunked || iBodyLength == XHTTP_BODY_LENGTH_UNKNOWN ||
			!__xcodecHttpParseContentLength(sContentLength, &iDeclaredLength, &bParsedLength) || !bParsedLength ||
			(uint64)iDeclaredLength != (uint64)(bHasBody ? iBodyLength : 0) ) { return false; }
	}
	if ( bHasBody && iBodyLength == XHTTP_BODY_LENGTH_UNKNOWN ) { bUseChunked = true; }
	if ( __xhttpStrEqNoCase(sMethod, "TRACE") && bHasBody ) { return false; }
	if ( pError ) { *pError = XHTTP_ERROR_OUT_OF_MEMORY; }

	if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, sMethod) ||
		!__xhttpAppendText(&pBuf, &iLen, &iCap, " ") ||
		!__xhttpAppendText(&pBuf, &iLen, &iCap, sTarget) ||
		!__xhttpAppendText(&pBuf, &iLen, &iCap, " HTTP/1.1\r\n") ) { goto fail; }
	if ( !sHostHeaderValue ) {
		char* sHostHeader;
		size_t iHostLen = strlen(sHostValue);
		size_t iHostCap;
		if ( iHostLen > SIZE_MAX - 32u ) { if ( pError ) { *pError = XHTTP_ERROR_INVALID_ARGUMENT; } goto fail; }
		iHostCap = iHostLen + 32u;
		sHostHeader = (char*)XNET_ALLOC(iHostCap);
		if ( !sHostHeader ) { goto fail; }
		if ( !__xhttpMakeHostHeader(pReq, sHostHeader, iHostCap) ) {
			if ( pError ) { *pError = XHTTP_ERROR_INVALID_ARGUMENT; }
			XNET_FREE(sHostHeader);
			goto fail;
		}
		if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, "Host: ") ||
			!__xhttpAppendText(&pBuf, &iLen, &iCap, sHostHeader) ||
			!__xhttpAppendText(&pBuf, &iLen, &iCap, "\r\n") ) {
			XNET_FREE(sHostHeader);
			goto fail;
		}
		XNET_FREE(sHostHeader);
	}
	if ( !__xhttpRequestHasHeader(pReq, "Connection") &&
		!__xhttpAppendText(&pBuf, &iLen, &iCap, "Connection: keep-alive\r\n") ) { goto fail; }
	if ( bHasBody && !bUseChunked && !sContentLength ) {
		snprintf(aLine, sizeof(aLine), "Content-Length: %llu\r\n", (unsigned long long)iBodyLength);
		if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, aLine) ) { goto fail; }
	}
	if ( bUseChunked && !sTransferEncoding &&
		!__xhttpAppendText(&pBuf, &iLen, &iCap, "Transfer-Encoding: chunked\r\n") ) { goto fail; }
	for ( uint32 i = 0u; i < pReq->iHeaderCount; ++i ) {
		if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, pHeaders[i].sName) ||
			!__xhttpAppendText(&pBuf, &iLen, &iCap, ": ") ||
			!__xhttpAppendText(&pBuf, &iLen, &iCap, pHeaders[i].sValue) ||
			!__xhttpAppendText(&pBuf, &iLen, &iCap, "\r\n") ) { goto fail; }
	}
	if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, "\r\n") ) { goto fail; }
	*ppOut = pBuf;
	*pOutLen = iLen;
	*pChunked = bUseChunked;
	if ( pError ) { *pError = XHTTP_ERROR_NONE; }
	return true;

fail:
	if ( pBuf ) { XNET_FREE(pBuf); }
	return false;
}


// 内部函数：__xhttpBuildRequestBytes
#if defined(XRT_INTERNAL_TEST_ENV)
static bool __xhttpBuildRequestBytesEx(const xhttprequest* pReq, char** ppOut, size_t* pOutLen, xhttp_error_code* pError)
{
	char* pBuf = NULL;
	xhttpheader* pHeaders;
	const char* sMethod;
	const char* sTarget;
	const char* sHostValue;
	const char* sTransferEncoding;
	const char* sContentLength;
	const char* sHostHeaderValue;
	size_t iLen = 0;
	size_t iCap = 0;
	char aLine[512];
	bool bChunked;
	int64_t iDeclaredLength = -1;
	bool bParsedLength = false;

	// 参数校验并初始化输出
	if ( pError ) { *pError = XHTTP_ERROR_INVALID_ARGUMENT; }
	if ( !pReq || !ppOut || !pOutLen ) { return false; }
	sMethod = __xhttpRequestMethodText(pReq);
	sTarget = __xhttpUrlPathText(&pReq->tURL);
	sHostValue = __xhttpUrlHostText(&pReq->tURL);
	if ( !sMethod || !sMethod[0] || !sTarget || !sTarget[0] || !sHostValue || !sHostValue[0] ||
		(!pReq->pBody && pReq->iBodyLen > 0u) ) { return false; }
	*ppOut = NULL;
	*pOutLen = 0;
	pHeaders = __xhttpRequestHeaders(pReq);
	sTransferEncoding = __xhttpRequestHeaderValue(pReq, "Transfer-Encoding");
	sContentLength = __xhttpRequestHeaderValue(pReq, "Content-Length");
	sHostHeaderValue = __xhttpRequestHeaderValue(pReq, "Host");
	if ( sHostHeaderValue ) {
		xrturlview tAuthority;
		if ( !sHostHeaderValue[0] || strchr(sHostHeaderValue, ',') != NULL ||
			!xrtUrlParseAuthority(sHostHeaderValue, &tAuthority) ||
			(tAuthority.iFlags & XRT_URL_F_HAS_USERINFO) != 0u ) { return false; }
	}
	bChunked = false;
	if ( sTransferEncoding ) {
		const char* sBegin = sTransferEncoding;
		const char* sEnd = sTransferEncoding + strlen(sTransferEncoding);
		while ( sBegin < sEnd && (*sBegin == ' ' || *sBegin == '\t') ) { sBegin++; }
		while ( sEnd > sBegin && (sEnd[-1] == ' ' || sEnd[-1] == '\t') ) { sEnd--; }
		if ( !__xcodecHttpTokenEqNoCase(sBegin, (size_t)(sEnd - sBegin), "chunked") ) { return false; }
		bChunked = true;
	}
	if ( sContentLength ) {
		if ( bChunked || !__xcodecHttpParseContentLength(sContentLength, &iDeclaredLength, &bParsedLength) ||
			!bParsedLength || (uint64)iDeclaredLength != (uint64)pReq->iBodyLen ) { return false; }
	}
	if ( __xhttpStrEqNoCase(sMethod, "TRACE") && pReq->iBodyLen > 0u ) { return false; }
	if ( pError ) { *pError = XHTTP_ERROR_OUT_OF_MEMORY; }

	// 构建请求行：METHOD PATH HTTP/1.1
	if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, sMethod) ) { goto fail; }
	if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, " ") ) { goto fail; }
	if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, sTarget) ) { goto fail; }
	if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, " HTTP/1.1\r\n") ) { goto fail; }

	// 补充 Host 头部
	if ( !__xhttpRequestHasHeader(pReq, "Host") ) {
		char* sHostHeader;
		size_t iHostLen = strlen(sHostValue);
		size_t iHostCap;
		if ( iHostLen > SIZE_MAX - 32u ) {
			if ( pError ) { *pError = XHTTP_ERROR_INVALID_ARGUMENT; }
			goto fail;
		}
		iHostCap = iHostLen + 32u;
		sHostHeader = (char*)XNET_ALLOC(iHostCap);
		if ( !sHostHeader ) { goto fail; }
		if ( !__xhttpMakeHostHeader(pReq, sHostHeader, iHostCap) ) {
			if ( pError ) { *pError = XHTTP_ERROR_INVALID_ARGUMENT; }
			XNET_FREE(sHostHeader);
			goto fail;
		}
		if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, "Host: ") ||
			!__xhttpAppendText(&pBuf, &iLen, &iCap, sHostHeader) ||
			!__xhttpAppendText(&pBuf, &iLen, &iCap, "\r\n") ) {
			XNET_FREE(sHostHeader);
			goto fail;
		}
		XNET_FREE(sHostHeader);
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
		if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, pHeaders[i].sName) ||
			!__xhttpAppendText(&pBuf, &iLen, &iCap, ": ") ||
			!__xhttpAppendText(&pBuf, &iLen, &iCap, pHeaders[i].sValue) ||
			!__xhttpAppendText(&pBuf, &iLen, &iCap, "\r\n") ) { goto fail; }
	}
	// 空行分隔头部和正文
	if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, "\r\n") ) { goto fail; }
	// 处理请求正文
	if ( bChunked ) {
		if ( pReq->pBody && pReq->iBodyLen > 0u ) {
			snprintf(aLine, sizeof(aLine), "%llX\r\n", (unsigned long long)pReq->iBodyLen);
			if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, aLine) ) { goto fail; }
			if ( !__xhttpAppendBytes(&pBuf, &iLen, &iCap, pReq->pBody, pReq->iBodyLen) ) { goto fail; }
			if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, "\r\n") ) { goto fail; }
		}
		if ( !__xhttpAppendText(&pBuf, &iLen, &iCap, "0\r\n\r\n") ) { goto fail; }
	} else if ( pReq->pBody && pReq->iBodyLen > 0 ) {
		if ( !__xhttpAppendBytes(&pBuf, &iLen, &iCap, pReq->pBody, pReq->iBodyLen) ) { goto fail; }
	}

	*ppOut = pBuf;
	*pOutLen = iLen;
	if ( pError ) { *pError = XHTTP_ERROR_NONE; }
	return true;

fail:
	if ( pBuf ) { XNET_FREE(pBuf); }
	return false;
}


static bool __xhttpBuildRequestBytes(const xhttprequest* pReq, char** ppOut, size_t* pOutLen)
{
	return __xhttpBuildRequestBytesEx(pReq, ppOut, pOutLen, NULL);
}
#endif


// 内部函数：__xhttpBuildResponse
static xhttpresponse* __xhttpBuildResponse(const xcodecframe* pFrame, const xcodechttp1msg* pMsg, const xnetchain* pChain)
{
	xhttpresponse* pResp;
	size_t iBodyBytes;
	const xcodechttp1header* pMsgHeaders;
	const xcodechttp1header* pMsgTrailers;
	size_t iStorageBytes;
	size_t iStorageOff = 0u;
	// 参数校验
	if ( !pFrame || !pMsg || !pChain ) { return NULL; }
	// 分配响应结构体
	pResp = (xhttpresponse*)XNET_ALLOC(sizeof(xhttpresponse));
	if ( !pResp ) { return NULL; }
	memset(pResp, 0, sizeof(xhttpresponse));
	pResp->pHeaders = pResp->arrHeaders;
	pResp->iHeaderCap = XHTTP_MAX_HEADERS;
	pResp->pTrailers = pResp->arrTrailers;
	pResp->iTrailerCap = XHTTP_MAX_TRAILERS;
	// 复制状态码、内容长度、版本号、原因短语
	pResp->iStatusCode = pMsg->iStatusCode;
	pResp->iContentLength = pMsg->iContentLength;
	// 映射编解码器标志到响应标志
	if ( pMsg->iFlags & XCODEC_HTTP1_F_CHUNKED ) { pResp->iFlags |= XHTTP_RESP_F_CHUNKED; }
	if ( pMsg->iFlags & XCODEC_HTTP1_F_KEEPALIVE ) { pResp->iFlags |= XHTTP_RESP_F_KEEPALIVE; }
	if ( pMsg->iFlags & XCODEC_HTTP1_F_UPGRADE ) { pResp->iFlags |= XHTTP_RESP_F_UPGRADE; }
	// 复制响应头部
	pMsgHeaders = pMsg->pHeaders ? pMsg->pHeaders : pMsg->arrHeaders;
	pMsgTrailers = pMsg->pTrailers ? pMsg->pTrailers : pMsg->arrTrailers;
	if ( !__xhttpEnsureResponseHeaderCap(pResp, pMsg->iHeaderCount) ) {
		xrtHttpResponseDestroy(pResp);
		return NULL;
	}
	if ( !__xhttpEnsureResponseTrailerCap(pResp, pMsg->iTrailerCount) ) {
		xrtHttpResponseDestroy(pResp);
		return NULL;
	}
	iStorageBytes = pMsg->iVersionLen + 1u + pMsg->iReasonLen + 1u;
	for ( uint32 i = 0u; i < pMsg->iHeaderCount; ++i ) {
		if ( pMsgHeaders[i].iNameLen > SIZE_MAX - iStorageBytes - 1u ||
			pMsgHeaders[i].iValueLen > SIZE_MAX - iStorageBytes - pMsgHeaders[i].iNameLen - 2u ) {
			xrtHttpResponseDestroy(pResp);
			return NULL;
		}
		iStorageBytes += pMsgHeaders[i].iNameLen + pMsgHeaders[i].iValueLen + 2u;
	}
	for ( uint32 i = 0u; i < pMsg->iTrailerCount; ++i ) {
		if ( pMsgTrailers[i].iNameLen > SIZE_MAX - iStorageBytes - 1u ||
			pMsgTrailers[i].iValueLen > SIZE_MAX - iStorageBytes - pMsgTrailers[i].iNameLen - 2u ) {
			xrtHttpResponseDestroy(pResp);
			return NULL;
		}
		iStorageBytes += pMsgTrailers[i].iNameLen + pMsgTrailers[i].iValueLen + 2u;
	}
	pResp->pStorage = (char*)XNET_ALLOC(iStorageBytes);
	if ( !pResp->pStorage ) { xrtHttpResponseDestroy(pResp); return NULL; }
#define __XHTTP_COPY_META(dst_, src_, len_) do { \
	(dst_) = pResp->pStorage + iStorageOff; \
	if ( (len_) > 0u ) { memcpy(pResp->pStorage + iStorageOff, (src_), (len_)); } \
	pResp->pStorage[iStorageOff + (len_)] = '\0'; \
	iStorageOff += (len_) + 1u; \
} while ( 0 )
	__XHTTP_COPY_META(pResp->sVersion, pMsg->sVersion ? pMsg->sVersion : "", pMsg->iVersionLen);
	__XHTTP_COPY_META(pResp->sReason, pMsg->sReason ? pMsg->sReason : "", pMsg->iReasonLen);
	for ( uint32 i = 0u; i < pMsg->iHeaderCount; ++i ) {
		__XHTTP_COPY_META(pResp->pHeaders[i].sName, pMsgHeaders[i].sName, pMsgHeaders[i].iNameLen);
		__XHTTP_COPY_META(pResp->pHeaders[i].sValue, pMsgHeaders[i].sValue, pMsgHeaders[i].iValueLen);
		pResp->iHeaderCount++;
	}
	for ( uint32 i = 0u; i < pMsg->iTrailerCount; ++i ) {
		__XHTTP_COPY_META(pResp->pTrailers[i].sName, pMsgTrailers[i].sName, pMsgTrailers[i].iNameLen);
		__XHTTP_COPY_META(pResp->pTrailers[i].sValue, pMsgTrailers[i].sValue, pMsgTrailers[i].iValueLen);
		pResp->iTrailerCount++;
	}
#undef __XHTTP_COPY_META
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
		pResp->iBodyCap = iBodyBytes + 1u;
		pResp->pBody[pResp->iBodyLen] = '\0';
	}
	return pResp;
}


// HEAD responses are complete when headers are complete; the body is always empty.
static void __xhttpTxAddRef(__xhttp_tx* pTx)
{
	if ( !pTx ) { return; }
	(void)__xhttpAtomicAdd(&pTx->iRefCount, 1);
}


static void __xhttpTxStopCancelWatch(__xhttp_tx* pTx)
{
	xnetcancelwatch* pWatch;
	if ( !pTx ) { return; }
	pWatch = pTx->pCancelWatch;
	pTx->pCancelWatch = NULL;
	if ( pWatch ) { xrtNetCancelWatchDestroy(pWatch); }
}


static void __xhttpTxResetHop(__xhttp_tx* pTx)
{
	if ( !pTx ) { return; }
	(void)__xhttpAtomicAdd(&pTx->iIdleTimerGen, 1);
	if ( pTx->pUploadWait ) {
		(void)xFutureRequestCancel(pTx->pUploadWait);
		xFutureRelease(pTx->pUploadWait);
		pTx->pUploadWait = NULL;
	}
	if ( pTx->pUploadReader ) {
		xrtHttpBodyReaderClose(pTx->pUploadReader);
		pTx->pUploadReader = NULL;
	}
	if ( pTx->pUploadBody ) {
		xrtHttpBodyRelease(pTx->pUploadBody);
		pTx->pUploadBody = NULL;
	}
	if ( pTx->pUploadBuf ) {
		XNET_FREE(pTx->pUploadBuf);
		pTx->pUploadBuf = NULL;
	}
#ifndef XRT_NO_XINFLATE
	if ( pTx->pResponseInflaters ) {
		for ( uint32 i = 0u; i < pTx->iResponseInflaterCount; ++i ) {
			__xinflateUnit(&pTx->pResponseInflaters[i]);
		}
		XNET_FREE(pTx->pResponseInflaters);
		pTx->pResponseInflaters = NULL;
	}
	pTx->iResponseInflaterCount = 0u;
	pTx->eResponseInflateError = __XINFLATE_OK;
#endif
	if ( pTx->pStreamResp ) {
		xrtHttpResponseDestroy(pTx->pStreamResp);
		pTx->pStreamResp = NULL;
	}
	if ( pTx->pSendBuf ) {
		XNET_FREE(pTx->pSendBuf);
		pTx->pSendBuf = NULL;
	}
	pTx->iSendLen = 0u;
	pTx->iSendOffset = 0u;
	pTx->iUploadBufCap = 0u;
	pTx->iUploadPendingLen = 0u;
	pTx->iUploadContentLength = 0;
	pTx->iUploadReadBytes = 0u;
	pTx->iEncodedBodyReceived = 0u;
	pTx->iBodyRemain = 0u;
	pTx->iChunkRemain = 0u;
	pTx->iBodyReceived = 0u;
	pTx->iStreamBodyMode = __XHTTP_STREAM_BODY_NONE;
	pTx->iStreamChunkState = __XHTTP_STREAM_CHUNK_SIZE;
	pTx->iInformationalCount = 0u;
	pTx->bStreamHeadersDelivered = false;
	pTx->bRedirectPending = false;
	pTx->bUploadChunked = false;
	pTx->bUploadOpened = false;
	pTx->bUploadEof = false;
	pTx->bUploadWaiting = false;
	pTx->bUploadComplete = false;
	pTx->bUploadTerminalSent = false;
}


// 内部函数：__xhttpTxDiscardTransient
static void __xhttpTxDiscardTransient(__xhttp_tx* pTx)
{
	if ( !pTx ) { return; }
	__xhttpTxStopCancelWatch(pTx);
	if ( pTx->pContext ) { (void)xrtHttpContextCancel(pTx->pContext); }
	__xhttpTxResetHop(pTx);
	if ( pTx->pContext ) {
		xrtHttpContextRelease(pTx->pContext);
		pTx->pContext = NULL;
	}
	__xhttpRequestUnitInternal(&pTx->tReq);
	if ( pTx->pClient ) {
		__xhttpClientRelease(pTx->pClient);
		pTx->pClient = NULL;
	}
}


// 内部函数：__xhttpTxComplete
static bool __xhttpTxComplete(__xhttp_tx* pTx, xnet_result iStatus, xhttpresponse* pResp)
{
	uint64 iNow;
	const char* sError = NULL;
	if ( !pTx ) {
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		return false;
	}
	if ( iStatus == XRT_NET_OK && pResp && !pResp->sEffectiveURL ) {
		const char* sURL = __xhttpRequestUrlText(&pTx->tReq);
		pResp->sEffectiveURL = sURL ? __xhttpStringCopyN(sURL, strlen(sURL)) : NULL;
		if ( !pResp->sEffectiveURL ) {
			__xhttpDiagnosticsSetError(pTx, XHTTP_ERROR_OUT_OF_MEMORY, XHTTP_PHASE_COMPLETE);
			xrtHttpResponseDestroy(pResp);
			pResp = NULL;
			iStatus = XRT_NET_ERROR;
		}
	}
	if ( __xhttpAtomicCompareExchange(&pTx->iComplete, 1, 0) != 0 ) {
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		return false;
	}
	iNow = __xhttpNowMs();
	if ( pTx->tDiagnostics.eError == XHTTP_ERROR_NONE && iStatus != XRT_NET_OK ) {
		if ( iStatus == XRT_NET_TIMEOUT ) { pTx->tDiagnostics.eError = XHTTP_ERROR_TIMEOUT_TOTAL; }
		else if ( iStatus == XRT_NET_CANCELLED ) { pTx->tDiagnostics.eError = XHTTP_ERROR_CANCELLED; }
		else if ( iStatus == XRT_NET_CLOSED ) { pTx->tDiagnostics.eError = XHTTP_ERROR_CONNECTION_CLOSED; }
		else { pTx->tDiagnostics.eError = XHTTP_ERROR_TRANSPORT; }
	}
	if ( iStatus == XRT_NET_OK ) {
		pTx->tDiagnostics.eError = XHTTP_ERROR_NONE;
		pTx->tDiagnostics.ePhase = XHTTP_PHASE_COMPLETE;
	}
	pTx->tDiagnostics.eTransportStatus = iStatus;
	pTx->tDiagnostics.iSystemError = pTx->iLastSysErr;
	pTx->tDiagnostics.iCompletedMs = iNow;
	pTx->tDiagnostics.iRequestBytes = pTx->iRequestWireBytes;
	pTx->tDiagnostics.iResponseBodyBytes = pResp ? (uint64)pResp->iBodyLen : pTx->iBodyReceived;
	pTx->tDiagnostics.iRedirectCount = pTx->iRedirectCount;
	pTx->tDiagnostics.iResponseWireBodyBytes = pTx->iEncodedBodyReceived;
	if ( pTx->tDiagnostics.iStartedMs > 0u && iNow >= pTx->tDiagnostics.iStartedMs ) {
		pTx->tDiagnostics.iTotalDurationMs = iNow - pTx->tDiagnostics.iStartedMs;
	}
	if ( pTx->tDiagnostics.iConnectedMs >= pTx->tDiagnostics.iStartedMs && pTx->tDiagnostics.iStartedMs > 0u ) {
		pTx->tDiagnostics.iConnectDurationMs = pTx->tDiagnostics.iConnectedMs - pTx->tDiagnostics.iStartedMs;
	}
	if ( pTx->tDiagnostics.iFirstByteMs >= pTx->tDiagnostics.iStartedMs && pTx->tDiagnostics.iStartedMs > 0u ) {
		pTx->tDiagnostics.iTimeToFirstByteMs = pTx->tDiagnostics.iFirstByteMs - pTx->tDiagnostics.iStartedMs;
	}
	if ( pTx->tDiagnostics.iFirstByteMs > 0u && iNow >= pTx->tDiagnostics.iFirstByteMs ) {
		pTx->tDiagnostics.iTransferDurationMs = iNow - pTx->tDiagnostics.iFirstByteMs;
	}
	if ( pResp ) { memcpy(&pResp->tDiagnostics, &pTx->tDiagnostics, sizeof(xhttpdiagnostics)); }
	if ( pTx->tReq.pDiagnostics ) { memcpy(pTx->tReq.pDiagnostics, &pTx->tDiagnostics, sizeof(xhttpdiagnostics)); }
	if ( pTx->pClient ) {
		__xhttpClientAdd64(&pTx->pClient->iRequestsCompleted, 1u);
		__xhttpClientAdd64(&pTx->pClient->iRequestBytes, pTx->tDiagnostics.iRequestBytes);
		__xhttpClientAdd64(&pTx->pClient->iResponseBodyBytes, pTx->iResponseBodyBytesTotal);
	}
	if ( iStatus != XRT_NET_OK ) { sError = xrtHttpErrorCodeName(pTx->tDiagnostics.eError); }
	if ( pTx->pFuture ) {
		(void)__xnetFutureCompleteEx(pTx->pFuture, iStatus, pResp, sError, XFUTURE_RESULT_F_NONE);
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
	__xhttpConnTransitionToNone(pConn);
	if ( __xhttpAtomicLoad(&pConn->iOpenedCounted) != 0 &&
		__xhttpAtomicCompareExchange(&pConn->iClosedCounted, 1, 0) == 0 && pConn->pClient ) {
		__xhttpClientAdd64(&pConn->pClient->iConnectionsClosed, 1u);
	}
	if ( pConn->pStream ) {
		xrtNetStreamSetUserData(pConn->pStream, NULL);
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
	if ( pConn->sHost ) { XNET_FREE(pConn->sHost); pConn->sHost = NULL; }
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
// 内部函数：__xhttpCompleteCloseDelimitedResponse
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


XXAPI void xrtHttpClientConfigInit(xhttpclientconfig* pConfig)
{
	if ( !pConfig ) { return; }
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->iSize = XHTTP_CLIENT_CONFIG_V3_SIZE;
	pConfig->iVersion = XHTTP_CLIENT_CONFIG_VERSION;
	pConfig->iRecvLimit = 1024u * 1024u;
	pConfig->iMaxInformationalResponses = XHTTP_MAX_INFORMATIONAL_RESPONSES;
	pConfig->iFlags = XHTTP_CLIENT_DEFAULT_FLAGS;
	pConfig->iMaxRedirects = XHTTP_CLIENT_DEFAULT_MAX_REDIRECTS;
	pConfig->iMaxIdleConnections = XHTTP_CLIENT_DEFAULT_MAX_IDLE_CONNECTIONS;
	pConfig->iMaxIdleConnectionsPerOrigin = XHTTP_CLIENT_DEFAULT_MAX_IDLE_CONNECTIONS_PER_ORIGIN;
	pConfig->iIdleConnectionTimeoutMs = XHTTP_CLIENT_DEFAULT_IDLE_CONNECTION_TIMEOUT_MS;
	xrtCodecHttp1LimitsInit(&pConfig->tHttp1Limits);
}


XXAPI void xrtHttpClientStatsInit(xhttpclientstats* pStats)
{
	if ( !pStats ) { return; }
	memset(pStats, 0, sizeof(*pStats));
	pStats->iSize = XHTTP_CLIENT_STATS_V1_SIZE;
	pStats->iVersion = XHTTP_CLIENT_STATS_VERSION;
}


XXAPI bool xrtHttpClientGetStats(const xhttpclient* pClient, xhttpclientstats* pStats)
{
	long iActive;
	long iIdle;
	if ( !pClient || !pStats || pStats->iSize < XHTTP_CLIENT_STATS_V1_SIZE ||
		pStats->iVersion != XHTTP_CLIENT_STATS_VERSION ) { return false; }
	iActive = __xnetAtomicLoad32(&pClient->iActiveConnections);
	iIdle = __xnetAtomicLoad32(&pClient->iIdleConnections);
	pStats->iSize = XHTTP_CLIENT_STATS_V1_SIZE;
	pStats->iVersion = XHTTP_CLIENT_STATS_VERSION;
	pStats->iActiveConnections = iActive > 0 ? (uint32)iActive : 0u;
	pStats->iIdleConnections = iIdle > 0 ? (uint32)iIdle : 0u;
	pStats->iRequestsStarted = (uint64)__xnetAtomicLoad64(&pClient->iRequestsStarted);
	pStats->iRequestsCompleted = (uint64)__xnetAtomicLoad64(&pClient->iRequestsCompleted);
	pStats->iConnectionsOpened = (uint64)__xnetAtomicLoad64(&pClient->iConnectionsOpened);
	pStats->iConnectionsReused = (uint64)__xnetAtomicLoad64(&pClient->iConnectionsReused);
	pStats->iConnectionsClosed = (uint64)__xnetAtomicLoad64(&pClient->iConnectionsClosed);
	pStats->iRedirectsFollowed = (uint64)__xnetAtomicLoad64(&pClient->iRedirectsFollowed);
	pStats->iRequestBytes = (uint64)__xnetAtomicLoad64(&pClient->iRequestBytes);
	pStats->iResponseBodyBytes = (uint64)__xnetAtomicLoad64(&pClient->iResponseBodyBytes);
	return true;
}


XXAPI xhttpclient* xrtHttpClientCreateEx(xnetengine* pEngine, const xhttpclientconfig* pConfig)
{
	xhttpclient* pClient;
	xhttpclientconfig tConfig;
	size_t iCopy;
	xnetengine* pResolvedEngine = __xnetSyncResolveEngine(pEngine);
	if ( !pResolvedEngine ) { return NULL; }
	xrtHttpClientConfigInit(&tConfig);
	if ( pConfig ) {
		if ( pConfig->iSize < XHTTP_CLIENT_CONFIG_V1_SIZE ||
			pConfig->iVersion == 0u ||
			pConfig->iVersion > XHTTP_CLIENT_CONFIG_VERSION ||
			(pConfig->iVersion == 2u && pConfig->iSize < XHTTP_CLIENT_CONFIG_V2_SIZE) ||
			(pConfig->iVersion == 3u && pConfig->iSize < XHTTP_CLIENT_CONFIG_V3_SIZE) ||
			pConfig->iRecvLimit == 0u || pConfig->iMaxInformationalResponses == 0u ||
			pConfig->tHttp1Limits.iSize < XCODEC_HTTP1_LIMITS_V1_SIZE ) {
			return NULL;
		}
		iCopy = pConfig->iVersion == 1u ? XHTTP_CLIENT_CONFIG_V1_SIZE :
			(pConfig->iVersion == 2u ? XHTTP_CLIENT_CONFIG_V2_SIZE :
			(pConfig->iSize < sizeof(tConfig) ? pConfig->iSize : sizeof(tConfig)));
		memcpy(&tConfig, pConfig, iCopy);
		tConfig.iSize = XHTTP_CLIENT_CONFIG_V3_SIZE;
		tConfig.iVersion = XHTTP_CLIENT_CONFIG_VERSION;
	}
	if ( (tConfig.iFlags & ~(XHTTP_CLIENT_F_FOLLOW_REDIRECTS |
		XHTTP_CLIENT_F_REDIRECT_POST_TO_GET |
		XHTTP_CLIENT_F_ALLOW_CROSS_ORIGIN_AUTH |
		XHTTP_CLIENT_F_ALLOW_HTTPS_DOWNGRADE |
		XHTTP_CLIENT_F_AUTO_DECOMPRESS)) != 0u ||
		((tConfig.iFlags & XHTTP_CLIENT_F_FOLLOW_REDIRECTS) != 0u && tConfig.iMaxRedirects == 0u) ) {
		return NULL;
	}
#ifdef XRT_NO_XINFLATE
	if ( (tConfig.iFlags & XHTTP_CLIENT_F_AUTO_DECOMPRESS) != 0u ) { return NULL; }
#endif
	pClient = (xhttpclient*)XNET_ALLOC(sizeof(xhttpclient));
	if ( !pClient ) { return NULL; }
	memset(pClient, 0, sizeof(xhttpclient));
	pClient->iRefCount = 1;
	pClient->pEngine = pResolvedEngine;
	pClient->tConfig = tConfig;
	if ( tConfig.pCookieJar ) {
		pClient->tConfig.pCookieJar = xrtHttpCookieJarRetain(tConfig.pCookieJar);
		if ( !pClient->tConfig.pCookieJar ) { XNET_FREE(pClient); return NULL; }
	}
	return pClient;
}


XXAPI xhttpclient* xrtHttpClientCreate(xnetengine* pEngine)
{
	return xrtHttpClientCreateEx(pEngine, NULL);
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
	__xhttpTxStopCancelWatch(pTx);
	if ( pTx->pContext ) { (void)xrtHttpContextCancel(pTx->pContext); }
	__xhttpDiagnosticsSetError(pTx, XHTTP_ERROR_CANCELLED, pTx->tDiagnostics.ePhase);
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


XXAPI xhttpcookiejar* xrtHttpClientCookieJar(const xhttpclient* pClient)
{
	return pClient ? pClient->tConfig.pCookieJar : NULL;
}


static void __xhttpContextCancelled(ptr pArg)
{
	__xhttp_tx* pTx = (__xhttp_tx*)pArg;
	if ( !pTx ) { return; }
	__xhttpTxAddRef(pTx);
	if ( __xhttpAtomicLoad(&pTx->iComplete) == 0 ) {
		__xhttpDiagnosticsSetError(pTx, XHTTP_ERROR_CANCELLED, pTx->tDiagnostics.ePhase);
		if ( __xhttpTxComplete(pTx, XRT_NET_CANCELLED, NULL) ) { __xhttpTxAbortStream(pTx); }
	}
	__xhttpTxRelease(pTx);
}


static bool __xhttpTxInstallCancelWatch(__xhttp_tx* pTx)
{
	xnetcancel* pCancel;
	if ( !pTx || !pTx->pContext ) { return false; }
	pCancel = xrtHttpContextCancelToken(pTx->pContext);
	if ( !pCancel ) { return false; }
	pTx->pCancelWatch = xrtNetCancelWatchCreate(pCancel, __xhttpContextCancelled, pTx);
	if ( !pTx->pCancelWatch ) { return false; }
	if ( xrtNetCancelWatchIsTriggered(pTx->pCancelWatch) || xrtNetCancelIsRequested(pCancel) ) {
		__xhttpContextCancelled(pTx);
	}
	return true;
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
		__xhttpTxStopCancelWatch(pTx);
		if ( pTx->pContext ) { (void)xrtHttpContextCancel(pTx->pContext); }
		__xhttpDiagnosticsSetError(pTx, XHTTP_ERROR_TIMEOUT_IDLE, pTx->tDiagnostics.ePhase);
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


static bool __xhttpConnPumpUpload(__xhttp_conn* pConn);


static bool __xhttpUploadFail(__xhttp_tx* pTx, xhttp_error_code eError, xnet_result iStatus)
{
	if ( !pTx ) { return false; }
	__xhttpTxStopCancelWatch(pTx);
	if ( pTx->pContext ) { (void)xrtHttpContextCancel(pTx->pContext); }
	__xhttpDiagnosticsSetError(pTx, eError, XHTTP_PHASE_WRITE);
	(void)__xhttpTxComplete(pTx, iStatus, NULL);
	__xhttpTxAbortStream(pTx);
	return false;
}


static bool __xhttpUploadFailFromContext(__xhttp_tx* pTx)
{
	if ( pTx && pTx->pContext && xrtHttpContextIsCancelled(pTx->pContext) ) {
		return __xhttpUploadFail(pTx, XHTTP_ERROR_CANCELLED, XRT_NET_CANCELLED);
	}
	return __xhttpUploadFail(pTx, XHTTP_ERROR_TIMEOUT_TOTAL, XRT_NET_TIMEOUT);
}


static void __xhttpUploadWaitArgFree(ptr pArg)
{
	__xhttp_upload_wait_ctx* pCtx = (__xhttp_upload_wait_ctx*)pArg;
	if ( !pCtx ) { return; }
	if ( pCtx->pSource ) { xFutureRelease(pCtx->pSource); }
	if ( pCtx->pTx ) { __xhttpTxRelease(pCtx->pTx); }
	XNET_FREE(pCtx);
}


static void __xhttpUploadReady(const xfuture_result* pResult, ptr pArg)
{
	__xhttp_upload_wait_ctx* pCtx = (__xhttp_upload_wait_ctx*)pArg;
	__xhttp_tx* pTx = pCtx ? pCtx->pTx : NULL;
	bool bCurrent = false;
	if ( !pTx || !pCtx->pSource ) { return; }
	if ( pTx->pUploadWait == pCtx->pSource ) {
		xFutureRelease(pTx->pUploadWait);
		pTx->pUploadWait = NULL;
		pTx->bUploadWaiting = false;
		bCurrent = true;
	}
	if ( !bCurrent || __xhttpAtomicLoad(&pTx->iComplete) != 0 ) { return; }
	if ( !pResult || pResult->iStatus != XRT_NET_OK ) {
		if ( pResult && pResult->iStatus == XRT_NET_CANCELLED ) {
			(void)__xhttpUploadFail(pTx, XHTTP_ERROR_CANCELLED, XRT_NET_CANCELLED);
		} else {
			(void)__xhttpUploadFail(pTx, XHTTP_ERROR_BODY, XRT_NET_ERROR);
		}
		return;
	}
	if ( pTx->pConn ) { (void)__xhttpConnPumpUpload(pTx->pConn); }
}


static bool __xhttpUploadArmReadable(__xhttp_tx* pTx)
{
	xnetfuture* pReady;
	xfuture* pHook;
	__xhttp_upload_wait_ctx* pCtx;
	uint32 iAffinity = 0u;
	if ( !pTx || !pTx->pUploadReader || pTx->bUploadWaiting ) { return false; }
	pReady = xrtHttpBodyReaderWaitReadable(pTx->pUploadReader, pTx->pContext);
	if ( !pReady ) {
		return xrtHttpContextIsDone(pTx->pContext)
			? __xhttpUploadFailFromContext(pTx)
			: __xhttpUploadFail(pTx, XHTTP_ERROR_BODY, XRT_NET_ERROR);
	}
	pCtx = (__xhttp_upload_wait_ctx*)XNET_ALLOC(sizeof(*pCtx));
	if ( !pCtx ) {
		xFutureRelease(pReady);
		return __xhttpUploadFail(pTx, XHTTP_ERROR_OUT_OF_MEMORY, XRT_NET_ERROR);
	}
	memset(pCtx, 0, sizeof(*pCtx));
	pCtx->pTx = pTx;
	__xhttpTxAddRef(pTx);
	pCtx->pSource = xFutureAddRef(pReady);
	pTx->pUploadWait = xFutureAddRef(pReady);
	pTx->bUploadWaiting = true;
	if ( pTx->pStream && pTx->pStream->pWorker ) { iAffinity = pTx->pStream->pWorker->iId; }
	pHook = xFutureFinallyEngineEx(pReady, pTx->pEngine, iAffinity,
		__xhttpUploadReady, pCtx, __xhttpUploadWaitArgFree);
	xFutureRelease(pReady);
	if ( !pHook ) {
		xFutureRelease(pTx->pUploadWait);
		pTx->pUploadWait = NULL;
		pTx->bUploadWaiting = false;
		__xhttpUploadWaitArgFree(pCtx);
		return __xhttpUploadFail(pTx, XHTTP_ERROR_OUT_OF_MEMORY, XRT_NET_ERROR);
	}
	xFutureRelease(pHook);
	return true;
}


static bool __xhttpUploadOpen(__xhttp_tx* pTx)
{
	xhttp_semantic_result eOpen;
	if ( !pTx || pTx->bUploadOpened ) { return pTx != NULL; }
	pTx->bUploadOpened = true;
	if ( !pTx->pUploadBody ) {
		pTx->bUploadEof = true;
		return true;
	}
	eOpen = xrtHttpBodyOpen(pTx->pUploadBody, pTx->pContext, &pTx->pUploadReader);
	if ( eOpen == XHTTP_SEMANTIC_OK ) { return true; }
	if ( eOpen == XHTTP_SEMANTIC_CANCELLED ) { return __xhttpUploadFailFromContext(pTx); }
	return __xhttpUploadFail(pTx,
		eOpen == XHTTP_SEMANTIC_OUT_OF_MEMORY ? XHTTP_ERROR_OUT_OF_MEMORY : XHTTP_ERROR_BODY,
		XRT_NET_ERROR);
}


static size_t __xhttpUploadPayloadCapacity(const __xhttp_tx* pTx)
{
	size_t iCap = 16u * 1024u;
	size_t iMax;
	if ( !pTx || !pTx->pStream ) { return 0u; }
	iMax = pTx->pStream->iMaxQueuedBytes;
	if ( pTx->bUploadChunked ) {
		if ( iMax <= 32u ) { return 0u; }
		iMax -= 32u;
	}
	if ( iCap > iMax ) { iCap = iMax; }
	return iCap;
}


static bool __xhttpUploadFinish(__xhttp_tx* pTx)
{
	if ( !pTx ) { return false; }
	if ( pTx->pUploadReader ) {
		xrtHttpBodyReaderClose(pTx->pUploadReader);
		pTx->pUploadReader = NULL;
	}
	if ( pTx->pUploadBody ) {
		xrtHttpBodyRelease(pTx->pUploadBody);
		pTx->pUploadBody = NULL;
	}
	pTx->bUploadComplete = true;
	pTx->tDiagnostics.iRequestSentMs = __xhttpNowMs();
	__xhttpDiagnosticsSetPhase(pTx, XHTTP_PHASE_RESPONSE_HEADERS);
	__xhttpTxRefreshIdleTimeout(pTx);
	return true;
}


static bool __xhttpConnPumpUpload(__xhttp_conn* pConn)
{
	__xhttp_tx* pTx;
	xnetstream* pStream;
	xnet_result iSendStatus;
	if ( !pConn || !pConn->pStream || !(pTx = pConn->pTx) ) { return false; }
	pStream = pConn->pStream;
	if ( __xhttpAtomicLoad(&pTx->iComplete) != 0 ) { return false; }
	if ( pTx->bUploadComplete || pTx->bUploadWaiting ) { return true; }
	if ( xrtHttpContextIsDone(pTx->pContext) ) { return __xhttpUploadFailFromContext(pTx); }
	__xhttpDiagnosticsSetPhase(pTx, XHTTP_PHASE_WRITE);

	while ( pTx->iSendOffset < pTx->iSendLen ) {
		size_t iRemain = pTx->iSendLen - pTx->iSendOffset;
		size_t iPiece = iRemain > 16u * 1024u ? 16u * 1024u : iRemain;
		if ( pStream->iMaxQueuedBytes > 0u && iPiece > pStream->iMaxQueuedBytes ) {
			iPiece = pStream->iMaxQueuedBytes;
		}
		if ( iPiece == 0u ) { return __xhttpUploadFail(pTx, XHTTP_ERROR_WRITE, XRT_NET_ERROR); }
		iSendStatus = xrtNetStreamSend(pStream, pTx->pSendBuf + pTx->iSendOffset, iPiece);
		if ( iSendStatus == XRT_NET_AGAIN ) { return true; }
		if ( iSendStatus != XRT_NET_OK ) { return __xhttpUploadFail(pTx, XHTTP_ERROR_WRITE, iSendStatus); }
		pTx->iSendOffset += iPiece;
		pTx->iRequestWireBytes += iPiece;
		__xhttpTxRefreshIdleTimeout(pTx);
	}

	if ( !__xhttpUploadOpen(pTx) ) { return false; }
	for ( ;; ) {
		if ( xrtHttpContextIsDone(pTx->pContext) ) { return __xhttpUploadFailFromContext(pTx); }
		if ( pTx->iUploadPendingLen > 0u ) {
			if ( pTx->bUploadChunked ) {
				char aPrefix[32];
				xnetspan arrSpan[3];
				int iPrefixLen = snprintf(aPrefix, sizeof(aPrefix), "%llX\r\n",
					(unsigned long long)pTx->iUploadPendingLen);
				if ( iPrefixLen <= 0 || (size_t)iPrefixLen >= sizeof(aPrefix) ) {
					return __xhttpUploadFail(pTx, XHTTP_ERROR_BODY, XRT_NET_ERROR);
				}
				arrSpan[0].pData = aPrefix;
				arrSpan[0].iLen = (uint32)iPrefixLen;
				arrSpan[1].pData = pTx->pUploadBuf;
				arrSpan[1].iLen = (uint32)pTx->iUploadPendingLen;
				arrSpan[2].pData = "\r\n";
				arrSpan[2].iLen = 2u;
				iSendStatus = xrtNetStreamSendVec(pStream, arrSpan, 3u);
				if ( iSendStatus == XRT_NET_OK ) {
					pTx->iRequestWireBytes += (uint64)iPrefixLen + pTx->iUploadPendingLen + 2u;
				}
			} else {
				iSendStatus = xrtNetStreamSend(pStream, pTx->pUploadBuf, pTx->iUploadPendingLen);
				if ( iSendStatus == XRT_NET_OK ) { pTx->iRequestWireBytes += pTx->iUploadPendingLen; }
			}
			if ( iSendStatus == XRT_NET_AGAIN ) { return true; }
			if ( iSendStatus != XRT_NET_OK ) { return __xhttpUploadFail(pTx, XHTTP_ERROR_WRITE, iSendStatus); }
			pTx->iUploadPendingLen = 0u;
			__xhttpTxRefreshIdleTimeout(pTx);
			continue;
		}
		if ( pTx->bUploadEof ) {
			if ( pTx->bUploadChunked && !pTx->bUploadTerminalSent ) {
				iSendStatus = xrtNetStreamSend(pStream, "0\r\n\r\n", 5u);
				if ( iSendStatus == XRT_NET_AGAIN ) { return true; }
				if ( iSendStatus != XRT_NET_OK ) { return __xhttpUploadFail(pTx, XHTTP_ERROR_WRITE, iSendStatus); }
				pTx->bUploadTerminalSent = true;
				pTx->iRequestWireBytes += 5u;
				__xhttpTxRefreshIdleTimeout(pTx);
			}
			return __xhttpUploadFinish(pTx);
		}
		if ( !pTx->pUploadReader ) {
			pTx->bUploadEof = true;
			continue;
		}
		if ( !pTx->pUploadBuf ) {
			pTx->iUploadBufCap = __xhttpUploadPayloadCapacity(pTx);
			if ( pTx->iUploadBufCap == 0u ) {
				return __xhttpUploadFail(pTx, XHTTP_ERROR_WRITE, XRT_NET_ERROR);
			}
			pTx->pUploadBuf = (uint8*)XNET_ALLOC(pTx->iUploadBufCap);
			if ( !pTx->pUploadBuf ) {
				return __xhttpUploadFail(pTx, XHTTP_ERROR_OUT_OF_MEMORY, XRT_NET_ERROR);
			}
		}
		{
			size_t iReadCap = pTx->iUploadBufCap;
			size_t iRead = 0u;
			xhttp_body_read_result eRead;
			if ( pTx->iUploadContentLength >= 0 ) {
				uint64 iExpected = (uint64)pTx->iUploadContentLength;
				uint64 iRemain = pTx->iUploadReadBytes < iExpected ? iExpected - pTx->iUploadReadBytes : 1u;
				if ( iReadCap > iRemain ) { iReadCap = (size_t)iRemain; }
			}
			eRead = xrtHttpBodyReaderRead(pTx->pUploadReader, pTx->pUploadBuf,
				iReadCap, &iRead, pTx->pContext);
			if ( eRead == XHTTP_BODY_READ_DATA ) {
				if ( pTx->iUploadContentLength >= 0 &&
					(pTx->iUploadReadBytes > (uint64)pTx->iUploadContentLength ||
					(uint64)iRead > (uint64)pTx->iUploadContentLength - pTx->iUploadReadBytes) ) {
					return __xhttpUploadFail(pTx, XHTTP_ERROR_BODY, XRT_NET_ERROR);
				}
				pTx->iUploadReadBytes += iRead;
				pTx->iUploadPendingLen = iRead;
				continue;
			}
			if ( eRead == XHTTP_BODY_READ_EOF ) {
				if ( pTx->iUploadContentLength >= 0 &&
					pTx->iUploadReadBytes != (uint64)pTx->iUploadContentLength ) {
					return __xhttpUploadFail(pTx, XHTTP_ERROR_BODY, XRT_NET_ERROR);
				}
				pTx->bUploadEof = true;
				continue;
			}
			if ( eRead == XHTTP_BODY_READ_AGAIN ) { return __xhttpUploadArmReadable(pTx); }
			if ( eRead == XHTTP_BODY_READ_CANCELLED ) { return __xhttpUploadFailFromContext(pTx); }
			return __xhttpUploadFail(pTx, XHTTP_ERROR_BODY, XRT_NET_ERROR);
		}
	}
}


static void __xhttpUploadPumpTask(xnetworker* pWorker, ptr pArg)
{
	__xhttp_tx* pTx = (__xhttp_tx*)pArg;
	(void)pWorker;
	if ( pTx && pTx->pConn ) { (void)__xhttpConnPumpUpload(pTx->pConn); }
	if ( pTx ) { __xhttpTxRelease(pTx); }
}


// Start or resume the request writer on the stream's owning worker.
static bool __xhttpConnSendActiveTx(__xhttp_conn* pConn)
{
	__xhttp_tx* pTx;
	uint32 iAffinity;
	if ( !pConn || !pConn->pStream || !(pTx = pConn->pTx) || !pTx->pSendBuf || pTx->iSendLen == 0u ) {
		return false;
	}
	if ( pConn->pStream->pWorker && __xnetEngineIsCurrentWorker(pConn->pStream->pWorker) ) {
		return __xhttpConnPumpUpload(pConn);
	}
	iAffinity = pConn->pStream->pWorker ? pConn->pStream->pWorker->iId : 0u;
	__xhttpTxAddRef(pTx);
	if ( xrtNetEnginePost(pTx->pEngine, iAffinity, __xhttpUploadPumpTask, pTx) != XRT_NET_OK ) {
		__xhttpTxRelease(pTx);
		return __xhttpUploadFail(pTx, XHTTP_ERROR_WRITE, XRT_NET_ERROR);
	}
	return true;
}


static bool __xhttpScheduleTotalTimeout(__xhttp_tx* pTx);


// 内部函数：__xhttpTxTimeoutTask
static void __xhttpTxTimeoutTask(xnetworker* pWorker, ptr pArg)
{
	__xhttp_tx* pTx = (__xhttp_tx*)pArg;
	int64_t iDeadline;
	uint64 iNow;
	(void)pWorker;
	if ( !pTx ) { return; }
	if ( __xhttpAtomicLoad(&pTx->iComplete) != 0 ) { __xhttpTxRelease(pTx); return; }
	if ( pTx->pContext && xrtHttpContextIsCancelled(pTx->pContext) ) {
		__xhttpDiagnosticsSetError(pTx, XHTTP_ERROR_CANCELLED, pTx->tDiagnostics.ePhase);
		(void)__xhttpTxComplete(pTx, XRT_NET_CANCELLED, NULL);
		__xhttpTxAbortStream(pTx);
		__xhttpTxRelease(pTx);
		return;
	}
	iDeadline = pTx->pContext ? xrtHttpContextDeadline(pTx->pContext) : 0;
	iNow = __xhttpNowMs();
	if ( iDeadline > 0 && (uint64)iDeadline > iNow ) {
		(void)__xhttpScheduleTotalTimeout(pTx);
	} else {
		__xhttpTxStopCancelWatch(pTx);
		if ( pTx->pContext ) { (void)xrtHttpContextCancel(pTx->pContext); }
		__xhttpDiagnosticsSetError(pTx, XHTTP_ERROR_TIMEOUT_TOTAL, pTx->tDiagnostics.ePhase);
		(void)__xhttpTxComplete(pTx, XRT_NET_TIMEOUT, NULL);
		__xhttpTxAbortStream(pTx);
	}
	__xhttpTxRelease(pTx);
}


static bool __xhttpScheduleTotalTimeout(__xhttp_tx* pTx)
{
	int64_t iDeadline;
	uint64 iNow;
	uint64 iRemain;
	uint32 iDelay;
	uint32 iAffinity = 0u;
	if ( !pTx || !pTx->pContext ) { return false; }
	iDeadline = xrtHttpContextDeadline(pTx->pContext);
	if ( iDeadline <= 0 ) { return true; }
	iNow = __xhttpNowMs();
	iRemain = (uint64)iDeadline > iNow ? (uint64)iDeadline - iNow : 1u;
	iDelay = iRemain > UINT32_MAX ? UINT32_MAX : (uint32)iRemain;
	if ( iDelay == 0u ) { iDelay = 1u; }
	if ( pTx->pStream && pTx->pStream->pWorker ) { iAffinity = pTx->pStream->pWorker->iId; }
	__xhttpTxAddRef(pTx);
	if ( xrtNetEnginePostDelayed(pTx->pEngine, iAffinity, iDelay, __xhttpTxTimeoutTask, pTx) != XRT_NET_OK ) {
		__xhttpTxRelease(pTx);
		return false;
	}
	return true;
}


// 内部函数：__xhttpClientOnOpen
static void __xhttpClientOnOpen(ptr pOwner, xnetstream* pStream)
{
	__xhttp_conn* pConn = (__xhttp_conn*)pOwner;
	if ( !pConn || !pStream ) { return; }
	pConn->bOpen = true;
	if ( __xhttpAtomicCompareExchange(&pConn->iOpenedCounted, 1, 0) == 0 && pConn->pClient ) {
		__xhttpClientAdd64(&pConn->pClient->iConnectionsOpened, 1u);
	}
	if ( pConn->pTx ) {
		if ( pConn->pTx->tDiagnostics.iConnectedMs == 0u ) {
			pConn->pTx->tDiagnostics.iConnectedMs = __xhttpNowMs();
		}
		__xhttpDiagnosticsSetPhase(pConn->pTx, XHTTP_PHASE_WRITE);
	}
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


static void __xhttpStreamFail(__xhttp_tx* pTx, xnet_result iStatus)
{
	if ( !pTx ) { return; }
	if ( iStatus == XRT_NET_ERROR && pTx->tDiagnostics.eError == XHTTP_ERROR_NONE ) {
		__xhttpDiagnosticsSetError(pTx, XHTTP_ERROR_PROTOCOL, pTx->tDiagnostics.ePhase);
	}
	(void)__xhttpTxComplete(pTx, iStatus, NULL);
	__xhttpTxAbortStream(pTx);
}


static bool __xhttpStreamDeliverBody(__xhttp_tx* pTx, const void* pData, size_t iLen)
{
	xhttpresponse* pResp;
	if ( !pTx || (!pData && iLen > 0u) ) { return false; }
	if ( __xhttpAtomicLoad(&pTx->iComplete) != 0 ) { return false; }
	if ( iLen == 0u ) { return true; }
	if ( pTx->iBodyReceived > pTx->tHttp1Limits.iMaxBodyBytes ||
		(uint64)iLen > pTx->tHttp1Limits.iMaxBodyBytes - pTx->iBodyReceived ||
		pTx->iBodyReceived > (uint64)SIZE_MAX || (uint64)iLen > (uint64)SIZE_MAX - pTx->iBodyReceived ) {
		xcodechttp1errorinfo tError;
		memset(&tError, 0, sizeof(tError));
		tError.eCode = XCODEC_HTTP1_ERROR_BODY_TOO_LARGE;
		__xhttpDiagnosticsSetProtocolError(pTx, &tError, XHTTP_PHASE_RESPONSE_BODY);
		__xhttpStreamFail(pTx, XRT_NET_ERROR);
		return false;
	}
	if ( !pTx->bRedirectPending && pTx->tStreamCallbacks.OnBody &&
		!pTx->tStreamCallbacks.OnBody(pTx->tStreamCallbacks.pUserData, pData, iLen) ) {
		__xhttpDiagnosticsSetError(pTx, XHTTP_ERROR_CALLBACK_ABORT, XHTTP_PHASE_RESPONSE_BODY);
		__xhttpStreamFail(pTx, XRT_NET_CANCELLED);
		return false;
	}
	pResp = pTx->pStreamResp;
	if ( !pTx->bRedirectPending && !pTx->bStreaming && pResp &&
		!__xhttpAppendBytes(&pResp->pBody, &pResp->iBodyLen, &pResp->iBodyCap, pData, iLen) ) {
		__xhttpDiagnosticsSetError(pTx, XHTTP_ERROR_OUT_OF_MEMORY, XHTTP_PHASE_RESPONSE_BODY);
		__xhttpStreamFail(pTx, XRT_NET_ERROR);
		return false;
	}
	pTx->iBodyReceived += (uint64)iLen;
	if ( pTx->bStreaming && pResp ) { pResp->iBodyLen = (size_t)pTx->iBodyReceived; }
	return true;
}


#ifndef XRT_NO_XINFLATE
#define __XHTTP_MAX_CONTENT_CODINGS 4u

typedef struct {
	__xhttp_tx* pTx;
	uint32 iInflater;
} __xhttp_inflate_output_ctx;


static int __xhttpResponseEncodingPlan(const xhttpresponse* pResp,
	__xinflate_format aFormats[__XHTTP_MAX_CONTENT_CODINGS], uint32* pCount, size_t* pCombinedLen)
{
	xhttpheader* pHeaders;
	uint32 iCount = 0u;
	size_t iCombinedLen = 0u;
	bool bFound = false;
	if ( pCount ) { *pCount = 0u; }
	if ( pCombinedLen ) { *pCombinedLen = 0u; }
	if ( !pResp || !pCount || !pCombinedLen ) { return -1; }
	pHeaders = __xhttpResponseHeaders(pResp);
	for ( uint32 i = 0u; i < pResp->iHeaderCount; ++i ) {
		const char* sValue;
		const char* p;
		if ( !pHeaders[i].sName || !__xhttpStrEqNoCase(pHeaders[i].sName, "Content-Encoding") ) { continue; }
		sValue = pHeaders[i].sValue ? pHeaders[i].sValue : "";
		if ( bFound ) {
			if ( iCombinedLen > SIZE_MAX - 2u ) { return -1; }
			iCombinedLen += 2u;
		}
		if ( strlen(sValue) > SIZE_MAX - iCombinedLen ) { return -1; }
		iCombinedLen += strlen(sValue);
		bFound = true;
		p = sValue;
		for ( ;; ) {
			const char* sToken;
			size_t iTokenLen;
			while ( *p == ' ' || *p == '\t' ) { p++; }
			sToken = p;
			while ( *p && *p != ',' && *p != ' ' && *p != '\t' ) { p++; }
			iTokenLen = (size_t)(p - sToken);
			if ( iTokenLen == 0u || !xrtHttpIsTokenN(sToken, iTokenLen) ) { return 0; }
			while ( *p == ' ' || *p == '\t' ) { p++; }
			if ( *p && *p != ',' ) { return 0; }
			if ( !__xcodecHttpTokenEqNoCase(sToken, iTokenLen, "identity") ) {
				if ( iCount >= __XHTTP_MAX_CONTENT_CODINGS ) { return 0; }
				if ( __xcodecHttpTokenEqNoCase(sToken, iTokenLen, "gzip") ||
					__xcodecHttpTokenEqNoCase(sToken, iTokenLen, "x-gzip") ) {
					aFormats[iCount++] = __XINFLATE_FORMAT_GZIP;
				} else if ( __xcodecHttpTokenEqNoCase(sToken, iTokenLen, "deflate") ) {
					aFormats[iCount++] = __XINFLATE_FORMAT_DEFLATE_AUTO;
				} else {
					return 0;
				}
			}
			if ( !*p ) { break; }
			p++;
		}
	}
	if ( !bFound || iCount == 0u ) { return 0; }
	*pCount = iCount;
	*pCombinedLen = iCombinedLen;
	return 1;
}


static bool __xhttpResponseInflateOutput(ptr pUserData, const void* pData, size_t iLen)
{
	__xhttp_inflate_output_ctx* pCtx = (__xhttp_inflate_output_ctx*)pUserData;
	__xhttp_tx* pTx = pCtx ? pCtx->pTx : NULL;
	uint32 iNext = pCtx ? pCtx->iInflater + 1u : 0u;
	if ( !pTx ) { return false; }
	if ( iNext < pTx->iResponseInflaterCount ) {
		__xhttp_inflate_output_ctx tNextCtx;
		__xinflate_result eResult;
		if ( pTx->pResponseInflaters[iNext].bDone ) {
			pTx->eResponseInflateError = __XINFLATE_ERROR;
			return false;
		}
		tNextCtx.pTx = pTx;
		tNextCtx.iInflater = iNext;
		eResult = __xinflateWrite(&pTx->pResponseInflaters[iNext], pData, iLen, false,
			__xhttpResponseInflateOutput, &tNextCtx);
		if ( eResult != __XINFLATE_OK && eResult != __XINFLATE_DONE ) {
			pTx->eResponseInflateError = eResult;
			return false;
		}
		return true;
	}
	return __xhttpStreamDeliverBody(pTx, pData, iLen);
}


static bool __xhttpResponseInflateFail(__xhttp_tx* pTx, __xinflate_result eResult)
{
	(void)eResult;
	if ( !pTx || __xhttpAtomicLoad(&pTx->iComplete) != 0 ) { return false; }
	__xhttpDiagnosticsSetError(pTx, XHTTP_ERROR_DECOMPRESSION, XHTTP_PHASE_RESPONSE_BODY);
	__xhttpStreamFail(pTx, XRT_NET_ERROR);
	return false;
}


static int __xhttpResponseInflateSetup(__xhttp_tx* pTx, xhttpresponse* pResp)
{
	__xinflate_format aFormats[__XHTTP_MAX_CONTENT_CODINGS];
	__xinflate* pInflaters = NULL;
	xhttpheader* pHeaders;
	char* sOriginal = NULL;
	size_t iCombinedLen = 0u;
	size_t iOffset = 0u;
	uint32 iCount = 0u;
	int iPlan;
	if ( !pTx || !pResp ) { return -1; }
	iPlan = __xhttpResponseEncodingPlan(pResp, aFormats, &iCount, &iCombinedLen);
	if ( iPlan <= 0 ) { return iPlan; }
	if ( iCombinedLen == SIZE_MAX ) { return -1; }
	sOriginal = (char*)XNET_ALLOC(iCombinedLen + 1u);
	pInflaters = (__xinflate*)XNET_ALLOC(sizeof(__xinflate) * iCount);
	if ( !sOriginal || !pInflaters ) { goto fail; }
	memset(pInflaters, 0, sizeof(__xinflate) * iCount);
	pHeaders = __xhttpResponseHeaders(pResp);
	for ( uint32 i = 0u; i < pResp->iHeaderCount; ++i ) {
		size_t iValueLen;
		if ( !pHeaders[i].sName || !__xhttpStrEqNoCase(pHeaders[i].sName, "Content-Encoding") ) { continue; }
		if ( iOffset > 0u ) { sOriginal[iOffset++] = ','; sOriginal[iOffset++] = ' '; }
		iValueLen = strlen(pHeaders[i].sValue ? pHeaders[i].sValue : "");
		if ( iValueLen > 0u ) { memcpy(sOriginal + iOffset, pHeaders[i].sValue, iValueLen); }
		iOffset += iValueLen;
	}
	sOriginal[iOffset] = '\0';
	for ( uint32 i = 0u; i < iCount; ++i ) {
		if ( !__xinflateInit(&pInflaters[i], aFormats[iCount - 1u - i],
			pTx->tHttp1Limits.iMaxBodyBytes) ) {
			for ( uint32 j = 0u; j < i; ++j ) { __xinflateUnit(&pInflaters[j]); }
			goto fail;
		}
	}
	pTx->pResponseInflaters = pInflaters;
	pTx->iResponseInflaterCount = iCount;
	pTx->eResponseInflateError = __XINFLATE_OK;
	pResp->sOriginalContentEncoding = sOriginal;
	pResp->iFlags |= XHTTP_RESP_F_DECOMPRESSED;
	pResp->iContentLength = -1;
	(void)__xhttpResponseRemoveHeader(pResp, "Content-Encoding");
	(void)__xhttpResponseRemoveHeader(pResp, "Content-Length");
	return 1;

fail:
	if ( pInflaters ) { XNET_FREE(pInflaters); }
	if ( sOriginal ) { XNET_FREE(sOriginal); }
	return -1;
}


static bool __xhttpResponseInflateFeed(__xhttp_tx* pTx, const void* pData, size_t iLen)
{
	__xhttp_inflate_output_ctx tCtx;
	__xinflate_result eResult;
	if ( !pTx || !pTx->pResponseInflaters || pTx->iResponseInflaterCount == 0u ) { return false; }
	if ( pTx->pResponseInflaters[0].bDone ) {
		return __xhttpResponseInflateFail(pTx, __XINFLATE_ERROR);
	}
	tCtx.pTx = pTx;
	tCtx.iInflater = 0u;
	pTx->eResponseInflateError = __XINFLATE_OK;
	eResult = __xinflateWrite(&pTx->pResponseInflaters[0], pData, iLen, false,
		__xhttpResponseInflateOutput, &tCtx);
	if ( eResult == __XINFLATE_OK || eResult == __XINFLATE_DONE ) { return true; }
	if ( pTx->eResponseInflateError != __XINFLATE_OK ) { eResult = pTx->eResponseInflateError; }
	return __xhttpResponseInflateFail(pTx, eResult);
}


static bool __xhttpResponseInflateFinish(__xhttp_tx* pTx)
{
	if ( !pTx || !pTx->pResponseInflaters ) { return true; }
	for ( uint32 i = 0u; i < pTx->iResponseInflaterCount; ++i ) {
		__xhttp_inflate_output_ctx tCtx;
		__xinflate_result eResult;
		if ( pTx->pResponseInflaters[i].bDone ) { continue; }
		tCtx.pTx = pTx;
		tCtx.iInflater = i;
		pTx->eResponseInflateError = __XINFLATE_OK;
		eResult = __xinflateWrite(&pTx->pResponseInflaters[i], NULL, 0u, true,
			__xhttpResponseInflateOutput, &tCtx);
		if ( eResult != __XINFLATE_DONE ) {
			if ( pTx->eResponseInflateError != __XINFLATE_OK ) { eResult = pTx->eResponseInflateError; }
			return __xhttpResponseInflateFail(pTx, eResult);
		}
	}
	return true;
}
#endif


static bool __xhttpStreamEmitBody(__xhttp_tx* pTx, const void* pData, size_t iLen)
{
	if ( !pTx || (!pData && iLen > 0u) ) { return false; }
	if ( __xhttpAtomicLoad(&pTx->iComplete) != 0 ) { return false; }
	if ( iLen == 0u ) { return true; }
	if ( pTx->iEncodedBodyReceived > pTx->tHttp1Limits.iMaxBodyBytes ||
		(uint64)iLen > pTx->tHttp1Limits.iMaxBodyBytes - pTx->iEncodedBodyReceived ) {
		xcodechttp1errorinfo tError;
		memset(&tError, 0, sizeof(tError));
		tError.eCode = XCODEC_HTTP1_ERROR_BODY_TOO_LARGE;
		__xhttpDiagnosticsSetProtocolError(pTx, &tError, XHTTP_PHASE_RESPONSE_BODY);
		__xhttpStreamFail(pTx, XRT_NET_ERROR);
		return false;
	}
	pTx->iEncodedBodyReceived += (uint64)iLen;
	pTx->iResponseBodyBytesTotal += (uint64)iLen;
#ifndef XRT_NO_XINFLATE
	if ( pTx->pResponseInflaters ) { return __xhttpResponseInflateFeed(pTx, pData, iLen); }
#endif
	return __xhttpStreamDeliverBody(pTx, pData, iLen);
}


XXAPI bool xrtHttpRequestSetBody(xhttprequest* pReq, xhttpbody* pBody, const char* sContentType)
{
	xhttpbody* pRetained;
	if ( !pReq ) { return false; }
	pRetained = pBody ? xrtHttpBodyRetain(pBody) : NULL;
	if ( pBody && !pRetained ) { return false; }
	if ( sContentType && sContentType[0] &&
		!xrtHttpRequestSetHeader(pReq, "Content-Type", sContentType) ) {
		xrtHttpBodyRelease(pRetained);
		return false;
	}
	if ( pReq->pBody ) {
		XNET_FREE(pReq->pBody);
		pReq->pBody = NULL;
		pReq->iBodyLen = 0u;
	}
	if ( pReq->pBodySource ) { xrtHttpBodyRelease(pReq->pBodySource); }
	pReq->pBodySource = pRetained;
	return true;
}


XXAPI bool xrtHttpRequestSetFormData(xhttprequest* pReq, const xhttpformdata* pForm)
{
	char sContentType[128];
	size_t iContentTypeLen = 0u;
	xhttpbody* pBody = NULL;
	bool bSet;
	if ( !pReq || !pForm ||
		xrtHttpFormDataBuildBody(pForm, NULL, &pBody, sContentType,
			sizeof(sContentType), &iContentTypeLen) != XHTTP_SEMANTIC_OK || !pBody ) {
		return false;
	}
	bSet = xrtHttpRequestSetBody(pReq, pBody, sContentType);
	xrtHttpBodyRelease(pBody);
	return bSet;
}


XXAPI xhttpbody* xrtHttpRequestBody(const xhttprequest* pReq)
{
	return pReq ? pReq->pBodySource : NULL;
}


XXAPI void xrtHttpRequestSetContext(xhttprequest* pReq, xhttpcontext* pContext)
{
	xhttpcontext* pRetained;
	if ( !pReq || pReq->pContext == pContext ) { return; }
	pRetained = pContext ? xrtHttpContextRetain(pContext) : NULL;
	if ( pReq->pContext ) { xrtHttpContextRelease(pReq->pContext); }
	pReq->pContext = pRetained;
}


XXAPI xhttpcontext* xrtHttpRequestContext(const xhttprequest* pReq)
{
	return pReq ? pReq->pContext : NULL;
}


static bool __xhttpResponseCopyTrailers(xhttpresponse* pResp, const xcodechttp1msg* pMsg)
{
	if ( !pResp || !pMsg ) { return false; }
	if ( !__xhttpEnsureResponseTrailerCap(pResp, pResp->iTrailerCount + pMsg->iTrailerCount) ) { return false; }
	for ( uint32 i = 0u; i < pMsg->iTrailerCount; ++i ) {
		const xcodechttp1header* pSrc = &pMsg->pTrailers[i];
		xhttpheader* pDst = &pResp->pTrailers[pResp->iTrailerCount];
		if ( !__xhttpHeaderSetCopyN(pDst, pSrc->sName, pSrc->iNameLen, pSrc->sValue, pSrc->iValueLen) ) { return false; }
		pResp->iTrailerCount++;
	}
	return true;
}


static bool __xhttpStreamResponseReusable(const __xhttp_tx* pTx, const xhttpresponse* pResp)
{
	if ( !pTx || !pResp || !pTx->pConn ) { return false; }
	if ( !pTx->bUploadComplete ) { return false; }
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
#ifndef XRT_NO_XINFLATE
	if ( !__xhttpResponseInflateFinish(pTx) ) { return; }
#endif
	pConn = pTx->pConn;
	pResp = pTx->pStreamResp;
	pTx->pStreamResp = NULL;
	pResp->iBodyLen = (size_t)pTx->iBodyReceived;
	if ( (pResp->iFlags & (XHTTP_RESP_F_CHUNKED | XHTTP_RESP_F_DECOMPRESSED)) != 0u ) {
		pResp->iContentLength = (int64_t)pTx->iBodyReceived;
	}
	bReusable = __xhttpStreamResponseReusable(pTx, pResp) && (!pChain || xrtNetChainBytes(pChain) == 0u);
	if ( pTx->bRedirectPending ) {
		bool bFollowed;
		if ( pConn ) {
			pConn->pTx = NULL;
			pTx->pConn = NULL;
			pTx->pStream = NULL;
			if ( !bReusable || !__xhttpPoolPut(pConn) ) {
				xrtNetStreamClose(pConn->pStream, XNET_CLOSE_F_ABORT);
			}
		}
		bFollowed = __xhttpFollowRedirect(pTx, pResp);
		xrtHttpResponseDestroy(pResp);
		if ( !bFollowed && pTx->pConn == NULL ) { __xhttpTxPostCleanup(pTx); }
		return;
	}
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
				if ( xrtNetChainBytes(pChain) > pTx->tHttp1Limits.iMaxChunkLineBytes ) {
					xcodechttp1errorinfo tError = { XCODEC_HTTP1_ERROR_CHUNK_LINE_TOO_LARGE, 0u, 0u };
					__xhttpDiagnosticsSetProtocolError(pTx, &tError, XHTTP_PHASE_RESPONSE_BODY);
					__xhttpStreamFail(pTx, XRT_NET_ERROR);
					return -1;
				}
				return 0;
			}
			if ( iLineLen > pTx->tHttp1Limits.iMaxChunkLineBytes ) {
				xcodechttp1errorinfo tError = { XCODEC_HTTP1_ERROR_CHUNK_LINE_TOO_LARGE, 0u, 0u };
				__xhttpDiagnosticsSetProtocolError(pTx, &tError, XHTTP_PHASE_RESPONSE_BODY);
				__xhttpStreamFail(pTx, XRT_NET_ERROR);
				return -1;
			}
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
				{
					xcodechttp1errorinfo tError = { XCODEC_HTTP1_ERROR_INVALID_CHUNK_SIZE, 0u, 0u };
					__xhttpDiagnosticsSetProtocolError(pTx, &tError, XHTTP_PHASE_RESPONSE_BODY);
				}
				__xhttpStreamFail(pTx, XRT_NET_ERROR);
				return -1;
			}
			XNET_FREE(sLine);
			if ( pTx->iEncodedBodyReceived > pTx->tHttp1Limits.iMaxBodyBytes ||
				iChunkSize > pTx->tHttp1Limits.iMaxBodyBytes - pTx->iEncodedBodyReceived ) {
				xcodechttp1errorinfo tError = { XCODEC_HTTP1_ERROR_BODY_TOO_LARGE, 0u, 0u };
				__xhttpDiagnosticsSetProtocolError(pTx, &tError, XHTTP_PHASE_RESPONSE_BODY);
				__xhttpStreamFail(pTx, XRT_NET_ERROR);
				return -1;
			}
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
			xcodechttp1msg tTrailers;
			xcodechttp1errorinfo tError;
			xcodecstatus eStatus;
			size_t iConsumed = 0u;
			xrtCodecHttp1MessageInit(&tTrailers);
			eStatus = xrtCodecHttp1ParseTrailersEx(pChain, &iConsumed, &tTrailers, &pTx->tHttp1Limits, &tError);
			if ( eStatus == XCODEC_STATUS_NEED_MORE ) { xrtCodecHttp1MessageUnit(&tTrailers); return 0; }
			if ( eStatus != XCODEC_STATUS_FRAME ) {
				__xhttpDiagnosticsSetProtocolError(pTx, &tError, XHTTP_PHASE_RESPONSE_BODY);
				xrtCodecHttp1MessageUnit(&tTrailers);
				__xhttpStreamFail(pTx, XRT_NET_ERROR);
				return -1;
			}
			if ( !__xhttpResponseCopyTrailers(pTx->pStreamResp, &tTrailers) ) {
				xrtCodecHttp1MessageUnit(&tTrailers);
				__xhttpDiagnosticsSetError(pTx, XHTTP_ERROR_OUT_OF_MEMORY, XHTTP_PHASE_RESPONSE_BODY);
				__xhttpStreamFail(pTx, XRT_NET_ERROR);
				return -1;
			}
			xrtCodecHttp1MessageUnit(&tTrailers);
			xrtNetChainConsume(pChain, iConsumed);
			__xhttpStreamFinish(pTx, pStream, pChain);
			return 1;
		}
		else {
			__xhttpStreamFail(pTx, XRT_NET_ERROR);
			return -1;
		}
	}
}


static void __xhttpStoreResponseCookies(__xhttp_tx* pTx, const xhttpresponse* pResp)
{
	xhttpcookiejar* pJar;
	xhttpheader* pHeaders;
	const char* sURL;
	if ( !pTx || !pResp || !pTx->pClient || !(pJar = pTx->pClient->tConfig.pCookieJar) ) { return; }
	sURL = __xhttpRequestUrlText(&pTx->tReq);
	if ( !sURL || !sURL[0] ) { return; }
	pHeaders = __xhttpResponseHeaders(pResp);
	for ( uint32 i = 0u; i < pResp->iHeaderCount; ++i ) {
		if ( pHeaders[i].sName && pHeaders[i].sValue &&
			__xhttpStrEqNoCase(pHeaders[i].sName, "Set-Cookie") ) {
			/* A rejected response cookie never turns a valid HTTP response into a transport failure. */
			(void)xrtHttpCookieJarSetCookie(pJar, sURL, pHeaders[i].sValue, 0);
		}
	}
}


static int __xhttpStartStreamResponse(__xhttp_tx* pTx, xnetstream* pStream, xnetchain* pChain)
{
	xcodecframe tFrame;
	xcodechttp1msg tMsg;
	xcodechttp1errorinfo tError;
	xcodecstatus iParse;
	xhttpresponse* pResp;
	bool bNoBodyExpected;
	if ( !pTx || !pStream || !pChain ) { return -1; }
	iParse = xrtCodecHttp1ParseHeadEx(pChain, &tFrame, &tMsg, &pTx->tHttp1Limits, &tError);
	if ( iParse == XCODEC_STATUS_NEED_MORE ) { return 0; }
	if ( iParse == XCODEC_STATUS_ERROR ) {
		__xhttpDiagnosticsSetProtocolError(pTx, &tError, XHTTP_PHASE_RESPONSE_HEADERS);
		__xhttpStreamFail(pTx, XRT_NET_ERROR);
		return -1;
	}
	if ( tMsg.iStatusCode < 200u &&
		(tMsg.iContentLength >= 0 || (tMsg.iFlags & XCODEC_HTTP1_F_CHUNKED) != 0u) ) {
		xcodechttp1errorinfo tFramingError = { XCODEC_HTTP1_ERROR_INVALID_CONTENT_LENGTH, 0u, 0u };
		xrtCodecHttp1MessageUnit(&tMsg);
		__xhttpDiagnosticsSetProtocolError(pTx, &tFramingError, XHTTP_PHASE_RESPONSE_HEADERS);
		__xhttpStreamFail(pTx, XRT_NET_ERROR);
		return -1;
	}
	// Ignore bounded non-upgrade informational responses and continue with the final response.
	if ( tMsg.iStatusCode >= 100u && tMsg.iStatusCode < 200u && tMsg.iStatusCode != 101u ) {
		if ( ++pTx->iInformationalCount > pTx->iMaxInformationalResponses ) {
			xrtCodecHttp1MessageUnit(&tMsg);
			__xhttpDiagnosticsSetError(pTx, XHTTP_ERROR_TOO_MANY_INFORMATIONAL_RESPONSES, XHTTP_PHASE_RESPONSE_HEADERS);
			__xhttpStreamFail(pTx, XRT_NET_ERROR);
			return -1;
		}
		xrtNetChainConsume(pChain, tFrame.iHeaderBytes);
		xrtCodecHttp1MessageUnit(&tMsg);
		return 2;
	}
	pResp = __xhttpBuildResponse(&tFrame, &tMsg, pChain);
	if ( !pResp ) {
		xrtCodecHttp1MessageUnit(&tMsg);
		__xhttpDiagnosticsSetError(pTx, XHTTP_ERROR_OUT_OF_MEMORY, XHTTP_PHASE_RESPONSE_HEADERS);
		__xhttpStreamFail(pTx, XRT_NET_ERROR);
		return -1;
	}
	bNoBodyExpected = __xhttpStatusHasNoBody(tMsg.iStatusCode) || __xhttpStrEqNoCase(pTx->tReq.sMethod, "HEAD");
	if ( tMsg.iStatusCode == 204u &&
		(tMsg.iContentLength >= 0 || (tMsg.iFlags & XCODEC_HTTP1_F_CHUNKED) != 0u) ) {
		xcodechttp1errorinfo tFramingError = { XCODEC_HTTP1_ERROR_INVALID_CONTENT_LENGTH, 0u, 0u };
		xrtHttpResponseDestroy(pResp);
		xrtCodecHttp1MessageUnit(&tMsg);
		__xhttpDiagnosticsSetProtocolError(pTx, &tFramingError, XHTTP_PHASE_RESPONSE_HEADERS);
		__xhttpStreamFail(pTx, XRT_NET_ERROR);
		return -1;
	}
	if ( !bNoBodyExpected && tMsg.iContentLength > 0 &&
		(uint64)tMsg.iContentLength > pTx->tHttp1Limits.iMaxBodyBytes ) {
		xcodechttp1errorinfo tBodyError = { XCODEC_HTTP1_ERROR_BODY_TOO_LARGE, tFrame.iHeaderBytes, 0u };
		xrtHttpResponseDestroy(pResp);
		xrtCodecHttp1MessageUnit(&tMsg);
		__xhttpDiagnosticsSetProtocolError(pTx, &tBodyError, XHTTP_PHASE_RESPONSE_HEADERS);
		__xhttpStreamFail(pTx, XRT_NET_ERROR);
		return -1;
	}
	__xhttpStoreResponseCookies(pTx, pResp);
	pTx->pStreamResp = pResp;
	pTx->bRedirectPending = __xhttpShouldAttemptRedirect(pTx, pResp);
	pTx->bStreamHeadersDelivered = true;
	pTx->tDiagnostics.iHeadersMs = __xhttpNowMs();
	__xhttpDiagnosticsSetPhase(pTx, XHTTP_PHASE_RESPONSE_BODY);
	pTx->iBodyReceived = 0u;
	pTx->iEncodedBodyReceived = 0u;
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
#ifndef XRT_NO_XINFLATE
	if ( !pTx->bRedirectPending && pTx->iStreamBodyMode != __XHTTP_STREAM_BODY_NONE &&
		(pTx->iClientFlags & XHTTP_CLIENT_F_AUTO_DECOMPRESS) != 0u ) {
		int iInflateSetup = __xhttpResponseInflateSetup(pTx, pResp);
		if ( iInflateSetup < 0 ) {
			xrtCodecHttp1MessageUnit(&tMsg);
			__xhttpDiagnosticsSetError(pTx, XHTTP_ERROR_OUT_OF_MEMORY, XHTTP_PHASE_RESPONSE_HEADERS);
			__xhttpStreamFail(pTx, XRT_NET_ERROR);
			return -1;
		}
	}
#endif
	xrtNetChainConsume(pChain, tFrame.iHeaderBytes);
	xrtCodecHttp1MessageUnit(&tMsg);
	if ( !pTx->bRedirectPending && pTx->bStreaming && pTx->tStreamCallbacks.OnHeaders &&
		!pTx->tStreamCallbacks.OnHeaders(pTx->tStreamCallbacks.pUserData, pResp) ) {
		__xhttpDiagnosticsSetError(pTx, XHTTP_ERROR_CALLBACK_ABORT, XHTTP_PHASE_RESPONSE_HEADERS);
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
	// 参数校验
	if ( !pTx || !pStream || !pChain ) { return; }
	/* Do not enter user callbacks after cancellation completed on another thread. */
	if ( __xhttpAtomicLoad(&pTx->iComplete) != 0 ) { return; }
	if ( pTx->tDiagnostics.iFirstByteMs == 0u ) {
		pTx->tDiagnostics.iFirstByteMs = __xhttpNowMs();
		__xhttpDiagnosticsSetPhase(pTx, XHTTP_PHASE_RESPONSE_HEADERS);
	}
	// 刷新空闲超时定时器
	__xhttpTxRefreshIdleTimeout(pTx);
	// 解析 HTTP 响应
	if ( pTx->bStreamHeadersDelivered ) {
		(void)__xhttpContinueStreamBody(pTx, pStream, pChain);
		return;
	}
	for ( ;; ) {
		int iStreamResult = __xhttpStartStreamResponse(pTx, pStream, pChain);
		if ( iStreamResult != 2 ) { return; }
	}
}


// 内部函数：__xhttpClientOnClose
static void __xhttpClientOnClose(ptr pOwner, xnetstream* pStream, xnet_result iReason)
{
	__xhttp_conn* pConn = (__xhttp_conn*)pOwner;
	__xhttp_tx* pTx = pConn ? pConn->pTx : NULL;
	if ( !pConn ) { return; }
	/* Later completions may still reference the deferred stream; detach the connection owner first. */
	if ( pStream ) { xrtNetStreamSetUserData(pStream, NULL); }
	pConn->bOpen = false;
	if ( pConn->bIdle ) { __xhttpPoolRemove(pConn); }
	pConn->bIdle = false;
	pConn->pStream = NULL;
	pConn->pTx = NULL;
	if ( pTx ) {
		if ( pTx->bStreamHeadersDelivered &&
			pTx->iStreamBodyMode == __XHTTP_STREAM_BODY_CLOSE &&
			__xhttpAtomicLoad(&pTx->iComplete) == 0 && iReason == XRT_NET_CLOSED ) {
			if ( pStream && xrtNetChainBytes(&pStream->tRxChain) > 0u ) {
				(void)__xhttpContinueStreamBody(pTx, pStream, &pStream->tRxChain);
			}
			pTx->pConn = NULL;
			pTx->pStream = NULL;
			if ( pTx->pStreamResp ) {
				pTx->pStreamResp->iContentLength = (int64_t)pTx->iBodyReceived;
				__xhttpStreamFinish(pTx, NULL, NULL);
			} else if ( __xhttpAtomicLoad(&pTx->iComplete) == 0 ) {
				(void)__xhttpTxComplete(pTx, XRT_NET_ERROR, NULL);
			}
			if ( __xhttpAtomicLoad(&pTx->iComplete) != 0 && pTx->pConn == NULL ) {
				__xhttpTxPostCleanup(pTx);
			}
			__xhttpConnPostCleanup(pConn);
			return;
		}
		pTx->pConn = NULL;
		pTx->pStream = NULL;
	}
	if ( pTx && __xhttpAtomicLoad(&pTx->iComplete) == 0 ) {
		__xhttpDiagnosticsSetError(
			pTx,
			iReason == XRT_NET_CLOSED ? XHTTP_ERROR_CONNECTION_CLOSED : XHTTP_ERROR_TRANSPORT,
			pTx->tDiagnostics.ePhase);
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
	if ( pTx ) {
		pTx->iLastSysErr = iSysErr;
		__xhttpDiagnosticsSetError(
			pTx,
			pTx->tDiagnostics.ePhase == XHTTP_PHASE_CONNECT ? XHTTP_ERROR_CONNECT : XHTTP_ERROR_TRANSPORT,
			pTx->tDiagnostics.ePhase);
	}
}


static void __xhttpClientOnDrain(ptr pOwner, xnetstream* pStream)
{
	__xhttp_conn* pConn = (__xhttp_conn*)pOwner;
	(void)pStream;
	if ( pConn && pConn->pTx && __xhttpAtomicLoad(&pConn->pTx->iComplete) == 0 ) {
		(void)__xhttpConnPumpUpload(pConn);
	}
}


static void __xhttpClientOnLowWater(ptr pOwner, xnetstream* pStream, uint32 iQueuedBytes)
{
	(void)iQueuedBytes;
	__xhttpClientOnDrain(pOwner, pStream);
}


// 内部函数：__xhttpClientEvents
static const xnetstreamevents* __xhttpClientEvents(void)
{
	static const xnetstreamevents tEvents = {
		__xhttpClientOnOpen,
		__xhttpClientOnRecv,
		__xhttpClientOnDrain,
		__xhttpClientOnClose,
		__xhttpClientOnError,
		NULL,
		__xhttpClientOnLowWater,
		NULL
	};
	return &tEvents;
}


static bool __xhttpPrepareTxContext(__xhttp_tx* pTx, xhttp_error_code* pError)
{
	if ( pError ) { *pError = XHTTP_ERROR_INVALID_ARGUMENT; }
	if ( !pTx || pTx->pContext ) { return false; }
	pTx->pContext = pTx->tReq.iTimeoutMs > 0u
		? xrtHttpContextWithTimeout(pTx->tReq.pContext, pTx->tReq.iTimeoutMs)
		: xrtHttpContextWithCancel(pTx->tReq.pContext);
	if ( !pTx->pContext ) {
		if ( pError ) { *pError = XHTTP_ERROR_OUT_OF_MEMORY; }
		return false;
	}
	if ( xrtHttpContextIsDone(pTx->pContext) ) {
		if ( pError ) {
			*pError = xrtHttpContextIsCancelled(pTx->pContext)
				? XHTTP_ERROR_CANCELLED : XHTTP_ERROR_TIMEOUT_TOTAL;
		}
		return false;
	}
	if ( pError ) { *pError = XHTTP_ERROR_NONE; }
	return true;
}


static bool __xhttpPrepareTxUpload(__xhttp_tx* pTx, xhttp_error_code* pError)
{
	bool bHasBody;
	xhttpbody* pBody = NULL;
	int64_t iBodyLength = 0;
	if ( pError ) { *pError = XHTTP_ERROR_INVALID_ARGUMENT; }
	if ( !pTx || !pTx->pContext || pTx->pSendBuf || pTx->pUploadBody ||
		(pTx->tReq.pBodySource && pTx->tReq.pBody) ||
		(!pTx->tReq.pBody && pTx->tReq.iBodyLen > 0u) ) { return false; }
	if ( xrtHttpContextIsDone(pTx->pContext) ) {
		if ( pError ) {
			*pError = xrtHttpContextIsCancelled(pTx->pContext)
				? XHTTP_ERROR_CANCELLED : XHTTP_ERROR_TIMEOUT_TOTAL;
		}
		return false;
	}
#ifndef XRT_NO_XINFLATE
	if ( (pTx->iClientFlags & XHTTP_CLIENT_F_AUTO_DECOMPRESS) != 0u &&
		!__xhttpRequestHasHeader(&pTx->tReq, "Accept-Encoding") &&
		!xrtHttpRequestSetHeader(&pTx->tReq, "Accept-Encoding", "gzip, deflate") ) {
		if ( pError ) { *pError = XHTTP_ERROR_OUT_OF_MEMORY; }
		return false;
	}
#endif
	bHasBody = pTx->tReq.pBodySource != NULL || pTx->tReq.iBodyLen > 0u;
	if ( pTx->tReq.pBodySource ) {
		pBody = xrtHttpBodyRetain(pTx->tReq.pBodySource);
		if ( !pBody ) { if ( pError ) { *pError = XHTTP_ERROR_OUT_OF_MEMORY; } return false; }
		iBodyLength = xrtHttpBodyContentLength(pBody);
	} else if ( pTx->tReq.iBodyLen > 0u ) {
		if ( pTx->tReq.iBodyLen > (size_t)INT64_MAX ) { return false; }
		pBody = xrtHttpBodyCreateBytesRef(pTx->tReq.pBody, pTx->tReq.iBodyLen, NULL, NULL);
		if ( !pBody ) { if ( pError ) { *pError = XHTTP_ERROR_OUT_OF_MEMORY; } return false; }
		iBodyLength = (int64_t)pTx->tReq.iBodyLen;
	}
	pTx->pUploadBody = pBody;
	pTx->iUploadContentLength = iBodyLength;
	if ( !__xhttpBuildRequestHeadEx(&pTx->tReq, bHasBody, iBodyLength,
		&pTx->pSendBuf, &pTx->iSendLen, &pTx->bUploadChunked, pError) ) {
		return false;
	}
	return true;
}


static bool __xhttpApplyClientCookies(__xhttp_tx* pTx, xhttp_error_code* pError)
{
	xhttpcookiejar* pJar;
	const char* sURL;
	xhttp_semantic_result eResult;
	char aEmpty[1];
	char* sCookie = NULL;
	size_t iCookieLen = 0u;
	if ( !pTx || !pTx->pClient || !(pJar = pTx->pClient->tConfig.pCookieJar) ) { return true; }
	if ( __xhttpRequestHeaderValue(&pTx->tReq, "Cookie") != NULL ) { return true; }
	sURL = __xhttpRequestUrlText(&pTx->tReq);
	if ( !sURL || !sURL[0] ) { if ( pError ) { *pError = XHTTP_ERROR_INVALID_URL; } return false; }
	eResult = xrtHttpCookieJarBuildHeaderTo(pJar, sURL, 0, aEmpty, sizeof(aEmpty), &iCookieLen);
	if ( eResult == XHTTP_SEMANTIC_OK ) { return true; }
	if ( eResult != XHTTP_SEMANTIC_BUFFER_TOO_SMALL || iCookieLen == 0u || iCookieLen == SIZE_MAX ) {
		if ( pError ) {
			*pError = eResult == XHTTP_SEMANTIC_OUT_OF_MEMORY
				? XHTTP_ERROR_OUT_OF_MEMORY : XHTTP_ERROR_INVALID_ARGUMENT;
		}
		return false;
	}
	sCookie = (char*)XNET_ALLOC(iCookieLen + 1u);
	if ( !sCookie ) { if ( pError ) { *pError = XHTTP_ERROR_OUT_OF_MEMORY; } return false; }
	eResult = xrtHttpCookieJarBuildHeaderTo(pJar, sURL, 0, sCookie, iCookieLen + 1u, &iCookieLen);
	if ( eResult != XHTTP_SEMANTIC_OK || !xrtHttpRequestSetHeader(&pTx->tReq, "Cookie", sCookie) ) {
		XNET_FREE(sCookie);
		if ( pError ) {
			*pError = eResult == XHTTP_SEMANTIC_OUT_OF_MEMORY || eResult == XHTTP_SEMANTIC_OK
				? XHTTP_ERROR_OUT_OF_MEMORY : XHTTP_ERROR_INVALID_ARGUMENT;
		}
		return false;
	}
	XNET_FREE(sCookie);
	pTx->bCookieHeaderFromJar = true;
	return true;
}


// xrtHttpExecuteAsync 相关处理
static bool __xhttpTxStartHop(__xhttp_tx* pTx)
{
	__xhttp_conn* pConn;
	xnetconnectconfig tConnCfg;
	const char* sHost;
	if ( !pTx || !pTx->pEngine || !pTx->pSendBuf || pTx->iSendLen == 0u ||
		__xhttpAtomicLoad(&pTx->iComplete) != 0 ) { return false; }
	pConn = __xhttpPoolTake(pTx->pClient, pTx->pEngine, &pTx->tReq);
	if ( !pConn ) {
		pConn = (__xhttp_conn*)XNET_ALLOC(sizeof(*pConn));
		if ( !pConn ) {
			__xhttpDiagnosticsSetError(pTx, XHTTP_ERROR_OUT_OF_MEMORY, XHTTP_PHASE_CONNECT);
			(void)__xhttpTxComplete(pTx, XRT_NET_ERROR, NULL);
			return false;
		}
		memset(pConn, 0, sizeof(*pConn));
		pConn->iId = (uint64)__xnetAtomicAddFetch64(&__g_xhttpNextConnectionId, 1);
		pConn->pEngine = pTx->pEngine;
		pConn->pClient = __xhttpClientAddRef(pTx->pClient);
		(void)__xhttpConnTransition(pConn, __XHTTP_CONN_STATE_NONE, __XHTTP_CONN_STATE_ACTIVE);
		sHost = __xhttpUrlHostText(&pTx->tReq.tURL);
		pConn->sHost = sHost ? __xhttpStringCopyN(sHost, strlen(sHost)) : NULL;
		if ( !pConn->sHost ) {
			__xhttpDiagnosticsSetError(pTx, XHTTP_ERROR_OUT_OF_MEMORY, XHTTP_PHASE_CONNECT);
			(void)__xhttpTxComplete(pTx, XRT_NET_ERROR, NULL);
			__xhttpConnCleanupTask(NULL, pConn);
			return false;
		}
		pConn->iPort = pTx->tReq.tURL.iPort;
		pConn->bHttps = pTx->tReq.tURL.bHttps;
		pConn->bVerifyPeer = pTx->tReq.bVerifyPeer;
		pConn->pProxy = pTx->tReq.pProxy ? xrtNetProxyAddRef(pTx->tReq.pProxy) : NULL;
		pConn->pStream = xrtNetStreamCreate(pTx->pEngine, __xhttpClientEvents(), pConn);
		if ( !pConn->pStream ) {
			__xhttpDiagnosticsSetError(pTx, XHTTP_ERROR_OUT_OF_MEMORY, XHTTP_PHASE_CONNECT);
			(void)__xhttpTxComplete(pTx, XRT_NET_ERROR, NULL);
			__xhttpConnCleanupTask(NULL, pConn);
			return false;
		}
	} else {
		pTx->tDiagnostics.bReusedConnection = true;
		if ( pTx->tDiagnostics.iConnectedMs == 0u ) {
			pTx->tDiagnostics.iConnectedMs = __xhttpNowMs();
		}
	}

	pConn->pTx = pTx;
	pTx->pConn = pConn;
	pTx->pStream = pConn->pStream;
	if ( !pConn->bOpen ) {
		__xhttpDiagnosticsSetPhase(pTx, XHTTP_PHASE_CONNECT);
		xrtNetConnectConfigInit(&tConnCfg);
		tConnCfg.sHost = pConn->sHost;
		tConnCfg.iPort = pTx->tReq.tURL.iPort;
		tConnCfg.iConnectTimeoutMs = __xhttpResolveConnectTimeoutMs(&pTx->tReq);
		tConnCfg.iRecvLimit = pTx->pClient ? pTx->pClient->tConfig.iRecvLimit : 1024u * 1024u;
		tConnCfg.pProxy = pConn->pProxy;
		if ( pTx->tReq.tURL.bHttps ) {
			memset(&pConn->tTlsCfg, 0, sizeof(pConn->tTlsCfg));
			pConn->tTlsCfg.sHostName = pConn->sHost;
			pConn->tTlsCfg.bVerifyPeer = pTx->tReq.bVerifyPeer;
			tConnCfg.pTlsConfig = &pConn->tTlsCfg;
		}
		if ( xrtNetStreamConnect(pConn->pStream, &tConnCfg) != XRT_NET_OK ) {
			__xhttpDiagnosticsSetError(pTx, XHTTP_ERROR_CONNECT, XHTTP_PHASE_CONNECT);
			(void)__xhttpTxComplete(pTx, XRT_NET_ERROR, NULL);
			pConn->pTx = NULL;
			pTx->pConn = NULL;
			pTx->pStream = NULL;
			__xhttpConnCleanupTask(NULL, pConn);
			return false;
		}
		return true;
	}
	if ( __xhttpConnSendActiveTx(pConn) ) { return true; }
	if ( __xhttpAtomicLoad(&pTx->iComplete) == 0 ) {
		__xhttpDiagnosticsSetError(pTx, XHTTP_ERROR_WRITE, XHTTP_PHASE_WRITE);
		(void)__xhttpTxComplete(pTx, XRT_NET_ERROR, NULL);
		xrtNetStreamClose(pConn->pStream, XNET_CLOSE_F_ABORT);
	}
	return false;
}


static bool __xhttpRedirectHasReplayableBody(const xhttprequest* pReq)
{
	if ( !pReq ) { return false; }
	if ( pReq->pBodySource ) { return xrtHttpBodyIsReplayable(pReq->pBodySource); }
	return pReq->pBody != NULL || pReq->iBodyLen == 0u;
}


static void __xhttpRedirectDropBody(xhttprequest* pReq)
{
	if ( !pReq ) { return; }
	if ( pReq->pBodySource ) {
		xrtHttpBodyRelease(pReq->pBodySource);
		pReq->pBodySource = NULL;
	}
	if ( pReq->pBody ) {
		XNET_FREE(pReq->pBody);
		pReq->pBody = NULL;
	}
	pReq->iBodyLen = 0u;
	(void)xrtHttpRequestRemoveHeader(pReq, "Content-Length");
	(void)xrtHttpRequestRemoveHeader(pReq, "Transfer-Encoding");
	(void)xrtHttpRequestRemoveHeader(pReq, "Content-Type");
	(void)xrtHttpRequestRemoveHeader(pReq, "Trailer");
}


static bool __xhttpFollowRedirect(__xhttp_tx* pTx, const xhttpresponse* pResp)
{
	const char* sBase;
	const char* sLocation;
	const char* sMethod;
	const char* sOldHost;
	xrturlview tBase;
	xrturlview tNext;
	char* sResolved = NULL;
	size_t iBaseLen;
	size_t iLocationLen;
	size_t iResolvedLen = 0u;
	size_t iResolvedCap;
	xhttp_error_code eError = XHTTP_ERROR_INVALID_REDIRECT;
	bool bDropBody;
	bool bCrossOrigin;
	bool bOldHttps;
	uint16 iOldPort;

	if ( !pTx || !pResp || !__xhttpShouldAttemptRedirect(pTx, pResp) ) { return false; }
	if ( pTx->iRedirectCount >= pTx->iMaxRedirects ) {
		eError = XHTTP_ERROR_TOO_MANY_REDIRECTS;
		goto fail;
	}
	if ( __xhttpResponseHeaderNameCount(pResp, "Location") != 1u ) { goto fail; }
	sLocation = xrtHttpResponseHeader(pResp, "Location");
	sBase = __xhttpRequestUrlText(&pTx->tReq);
	if ( !sLocation || !sBase || !xrtUrlParseView(sBase, &tBase) ||
		(tBase.iFlags & XRT_URL_F_ABSOLUTE) == 0u ) { goto fail; }
	iBaseLen = strlen(sBase);
	iLocationLen = strlen(sLocation);
	if ( iBaseLen > SIZE_MAX - iLocationLen - 65u ) {
		eError = XHTTP_ERROR_OUT_OF_MEMORY;
		goto fail;
	}
	iResolvedCap = iBaseLen + iLocationLen + 65u;
	sResolved = (char*)XNET_ALLOC(iResolvedCap);
	if ( !sResolved ) {
		eError = XHTTP_ERROR_OUT_OF_MEMORY;
		goto fail;
	}
	if ( !xrtUrlResolveTo(&tBase, sLocation, iLocationLen, sResolved, iResolvedCap, &iResolvedLen) ||
		iResolvedLen == 0u || !xrtUrlParseViewN(sResolved, iResolvedLen, &tNext) ||
		(tNext.iFlags & XRT_URL_F_ABSOLUTE) == 0u ||
		!xrtUrlViewMatchesScheme2(&tNext, "http", "https") ||
		xrtStrViewIsEmpty(tNext.tHost) || (tNext.iFlags & XRT_URL_F_HAS_USERINFO) != 0u ) { goto fail; }
	bOldHttps = pTx->tReq.tURL.bHttps;
	iOldPort = pTx->tReq.tURL.iPort;
	sOldHost = __xhttpUrlHostText(&pTx->tReq.tURL);
	if ( bOldHttps && !xrtUrlViewIsScheme(&tNext, "https") &&
		(pTx->iClientFlags & XHTTP_CLIENT_F_ALLOW_HTTPS_DOWNGRADE) == 0u ) {
		eError = XHTTP_ERROR_REDIRECT_DOWNGRADE;
		goto fail;
	}
	bCrossOrigin = bOldHttps != xrtUrlViewIsScheme(&tNext, "https") ||
		iOldPort != tNext.iPort || !__xhttpStrEqViewNoCase(sOldHost, tNext.tHost.sPtr, tNext.tHost.iLen);
	sMethod = __xhttpRequestMethodText(&pTx->tReq);
	bDropBody = pResp->iStatusCode == 303u ||
		((pResp->iStatusCode == 301u || pResp->iStatusCode == 302u) &&
		__xhttpStrEqNoCase(sMethod, "POST") &&
		(pTx->iClientFlags & XHTTP_CLIENT_F_REDIRECT_POST_TO_GET) != 0u);
	if ( !bDropBody && !__xhttpRedirectHasReplayableBody(&pTx->tReq) ) {
		eError = XHTTP_ERROR_REDIRECT_BODY_NOT_REPLAYABLE;
		goto fail;
	}

	__xhttpTxResetHop(pTx);
	if ( pTx->bCookieHeaderFromJar ) {
		(void)xrtHttpRequestRemoveHeader(&pTx->tReq, "Cookie");
		pTx->bCookieHeaderFromJar = false;
	}
	if ( !xrtHttpRequestSetURL(&pTx->tReq, sResolved) ) { goto fail; }
	(void)xrtHttpRequestRemoveHeader(&pTx->tReq, "Host");
	if ( bCrossOrigin ) {
		(void)xrtHttpRequestRemoveHeader(&pTx->tReq, "Cookie");
		pTx->bCookieHeaderFromJar = false;
		if ( (pTx->iClientFlags & XHTTP_CLIENT_F_ALLOW_CROSS_ORIGIN_AUTH) == 0u ) {
			(void)xrtHttpRequestRemoveHeader(&pTx->tReq, "Authorization");
			(void)xrtHttpRequestRemoveHeader(&pTx->tReq, "Proxy-Authorization");
		}
	}
	if ( bDropBody ) {
		if ( pResp->iStatusCode != 303u || !__xhttpStrEqNoCase(sMethod, "HEAD") ) {
			if ( !xrtHttpRequestSetMethod(&pTx->tReq, "GET") ) {
				eError = XHTTP_ERROR_OUT_OF_MEMORY;
				goto fail;
			}
		}
		__xhttpRedirectDropBody(&pTx->tReq);
	}
	if ( !__xhttpApplyClientCookies(pTx, &eError) || !__xhttpPrepareTxUpload(pTx, &eError) ) { goto fail; }
	pTx->iRedirectCount++;
	pTx->tDiagnostics.iRedirectCount = pTx->iRedirectCount;
	if ( pTx->pClient ) { __xhttpClientAdd64(&pTx->pClient->iRedirectsFollowed, 1u); }
	XNET_FREE(sResolved);
	return __xhttpTxStartHop(pTx);

fail:
	if ( sResolved ) { XNET_FREE(sResolved); }
	__xhttpDiagnosticsSetError(pTx, eError, XHTTP_PHASE_PREPARE);
	(void)__xhttpTxComplete(pTx,
		eError == XHTTP_ERROR_CANCELLED ? XRT_NET_CANCELLED :
		(eError == XHTTP_ERROR_TIMEOUT_TOTAL ? XRT_NET_TIMEOUT : XRT_NET_ERROR), NULL);
	return false;
}


static xnetfuture* __xhttpExecuteAsyncEx(xhttpclient* pClient, xnetengine* pEngine, const xhttprequest* pReq, const xhttpstreamcallbacks* pCallbacks, bool bStreaming)
{
	__xhttp_tx* pTx;
	xnetfuture* pFuture;
	xnetengine* pResolvedEngine;
	xhttp_error_code eBuildError = XHTTP_ERROR_NONE;

	// 参数校验与引擎解析
	if ( !pReq ) { return NULL; }
	if ( pReq->pDiagnostics ) {
		xrtHttpDiagnosticsInit(pReq->pDiagnostics);
		pReq->pDiagnostics->ePhase = XHTTP_PHASE_PREPARE;
		pReq->pDiagnostics->iStartedMs = __xhttpNowMs();
	}
	if ( pClient && __xhttpAtomicLoad(&pClient->iClosing) != 0 ) {
		__xhttpDiagnosticsCompleteEarly(pReq->pDiagnostics, XHTTP_ERROR_INVALID_ARGUMENT, XHTTP_PHASE_PREPARE, XRT_NET_ERROR);
		return NULL;
	}
	pResolvedEngine = pClient ? pClient->pEngine : __xnetSyncResolveEngine(pEngine);
	if ( !pResolvedEngine ) {
		__xhttpDiagnosticsCompleteEarly(pReq->pDiagnostics, XHTTP_ERROR_TRANSPORT, XHTTP_PHASE_PREPARE, XRT_NET_ERROR);
		return NULL;
	}
	// 创建 Future 用于异步返回结果
	pFuture = xrtNetFutureCreate();
	if ( !pFuture ) {
		__xhttpDiagnosticsCompleteEarly(pReq->pDiagnostics, XHTTP_ERROR_OUT_OF_MEMORY, XHTTP_PHASE_PREPARE, XRT_NET_ERROR);
		return NULL;
	}

	// 分配并初始化事务对象
	pTx = (__xhttp_tx*)XNET_ALLOC(sizeof(__xhttp_tx));
	if ( !pTx ) {
		__xhttpDiagnosticsCompleteEarly(pReq->pDiagnostics, XHTTP_ERROR_OUT_OF_MEMORY, XHTTP_PHASE_PREPARE, XRT_NET_ERROR);
		xrtNetFutureDestroy(pFuture);
		return NULL;
	}
	memset(pTx, 0, sizeof(__xhttp_tx));
	pTx->iRefCount = 1;
	pTx->pEngine = pResolvedEngine;
	pTx->pFuture = pFuture;
	xrtHttpDiagnosticsInit(&pTx->tDiagnostics);
	pTx->tDiagnostics.ePhase = XHTTP_PHASE_PREPARE;
	pTx->tDiagnostics.iStartedMs = __xhttpNowMs();
	pTx->tReq.pDiagnostics = pReq->pDiagnostics;
	__xnetFutureAddAsyncHold(pFuture);
	pTx->pClient = __xhttpClientAddRef(pClient);
	if ( pTx->pClient ) { __xhttpClientAdd64(&pTx->pClient->iRequestsStarted, 1u); }
	pTx->bStreaming = bStreaming;
	pTx->iStreamBodyMode = __XHTTP_STREAM_BODY_NONE;
	pTx->iStreamChunkState = __XHTTP_STREAM_CHUNK_SIZE;
	pTx->iMaxInformationalResponses = pClient ? pClient->tConfig.iMaxInformationalResponses : XHTTP_MAX_INFORMATIONAL_RESPONSES;
	pTx->iClientFlags = pClient ? pClient->tConfig.iFlags : XHTTP_CLIENT_DEFAULT_FLAGS;
	pTx->iMaxRedirects = pClient ? pClient->tConfig.iMaxRedirects : XHTTP_CLIENT_DEFAULT_MAX_REDIRECTS;
	if ( pClient ) { pTx->tHttp1Limits = pClient->tConfig.tHttp1Limits; }
	else { xrtCodecHttp1LimitsInit(&pTx->tHttp1Limits); }
	if ( pCallbacks ) { memcpy(&pTx->tStreamCallbacks, pCallbacks, sizeof(xhttpstreamcallbacks)); }
	if ( !__xhttpFutureAttachCancel(pTx) ) {
		__xhttpDiagnosticsSetError(pTx, XHTTP_ERROR_OUT_OF_MEMORY, XHTTP_PHASE_PREPARE);
		(void)__xhttpTxComplete(pTx, XRT_NET_ERROR, NULL);
		__xhttpTxRelease(pTx);
		return pFuture;
	}
	// 克隆请求数据到事务中
	if ( !__xhttpRequestClone(&pTx->tReq, pReq) ) {
		__xhttpDiagnosticsSetError(pTx, XHTTP_ERROR_OUT_OF_MEMORY, XHTTP_PHASE_PREPARE);
		(void)__xhttpTxComplete(pTx, XRT_NET_ERROR, NULL);
		__xhttpTxRelease(pTx);
		return pFuture;
	}
	// 延迟解析 URL（若主机地址为空则从 sURL 重新解析）
	if ( !__xhttpUrlHostText(&pTx->tReq.tURL) || !__xhttpUrlHostText(&pTx->tReq.tURL)[0] ) {
		const char* sURL = __xhttpRequestUrlText(&pTx->tReq);
		if ( !sURL || !sURL[0] || !__xhttpRequestStoreUrl(&pTx->tReq, sURL) ) {
			__xhttpDiagnosticsSetError(pTx, XHTTP_ERROR_INVALID_URL, XHTTP_PHASE_PREPARE);
			(void)__xhttpTxComplete(pTx, XRT_NET_ERROR, NULL);
			__xhttpTxRelease(pTx);
			return pFuture;
		}
	}
	if ( !__xhttpPrepareTxContext(pTx, &eBuildError) ) {
		__xhttpDiagnosticsSetError(pTx,
			eBuildError != XHTTP_ERROR_NONE ? eBuildError : XHTTP_ERROR_OUT_OF_MEMORY,
			XHTTP_PHASE_PREPARE);
		(void)__xhttpTxComplete(pTx,
			eBuildError == XHTTP_ERROR_CANCELLED ? XRT_NET_CANCELLED :
			(eBuildError == XHTTP_ERROR_TIMEOUT_TOTAL ? XRT_NET_TIMEOUT : XRT_NET_ERROR), NULL);
		__xhttpTxRelease(pTx);
		return pFuture;
	}
	if ( !__xhttpApplyClientCookies(pTx, &eBuildError) ) {
		__xhttpDiagnosticsSetError(pTx,
			eBuildError != XHTTP_ERROR_NONE ? eBuildError : XHTTP_ERROR_OUT_OF_MEMORY,
			XHTTP_PHASE_PREPARE);
		(void)__xhttpTxComplete(pTx, XRT_NET_ERROR, NULL);
		__xhttpTxRelease(pTx);
		return pFuture;
	}
	// Build the request head and prepare a per-execution streaming body reader.
	if ( !__xhttpPrepareTxUpload(pTx, &eBuildError) ) {
		__xhttpDiagnosticsSetError(pTx, eBuildError != XHTTP_ERROR_NONE ? eBuildError : XHTTP_ERROR_OUT_OF_MEMORY, XHTTP_PHASE_PREPARE);
		(void)__xhttpTxComplete(pTx,
			eBuildError == XHTTP_ERROR_CANCELLED ? XRT_NET_CANCELLED :
			(eBuildError == XHTTP_ERROR_TIMEOUT_TOTAL ? XRT_NET_TIMEOUT : XRT_NET_ERROR), NULL);
		__xhttpTxRelease(pTx);
		return pFuture;
	}

	// 尝试从空闲连接池中获取可复用的连接
	if ( !__xhttpTxInstallCancelWatch(pTx) ) {
		__xhttpDiagnosticsSetError(pTx, XHTTP_ERROR_OUT_OF_MEMORY, XHTTP_PHASE_PREPARE);
		(void)__xhttpTxComplete(pTx, XRT_NET_ERROR, NULL);
		__xhttpTxRelease(pTx);
		return pFuture;
	}
	if ( __xhttpAtomicLoad(&pTx->iComplete) != 0 ) {
		__xhttpTxRelease(pTx);
		return pFuture;
	}
		// 空闲池中无可用连接，创建新连接
		// 创建网络流并绑定客户端事件

	// 绑定事务到连接

	// 连接未打开时发起连接请求
		// 配置 TLS 参数（HTTPS 连接）
	if ( !__xhttpScheduleTotalTimeout(pTx) ) {
		__xhttpDiagnosticsSetError(pTx, XHTTP_ERROR_TRANSPORT, XHTTP_PHASE_PREPARE);
		(void)__xhttpTxComplete(pTx, XRT_NET_ERROR, NULL);
		__xhttpTxRelease(pTx);
		return pFuture;
	}
	if ( !__xhttpTxStartHop(pTx) && pTx->pConn == NULL ) { __xhttpTxPostCleanup(pTx); }
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
