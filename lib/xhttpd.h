#ifndef XRT_XHTTPD_H
#define XRT_XHTTPD_H

/*
	XRT mainline HTTP/1.1 server on top of xnet.

	This header provides:
	  - listener-driven HTTP server lifecycle on top of xnet_stream
	  - request and response materialization helpers
	  - plain HTTP and builtin TLS service paths
	  - serial keep-alive reuse on a single accepted connection
	  - fixed-length and chunked request body streaming callbacks
	  - buffered, streaming, and file response helpers
*/

#if !defined(XRT_BUILD_CORE)
#define XHTTPD_METHOD_CAP         16u
#define XHTTPD_TARGET_CAP         256u
#define XHTTPD_PATH_CAP           256u
#define XHTTPD_QUERY_CAP          256u
#define XHTTPD_VERSION_CAP        32u
#define XHTTPD_REASON_CAP         128u
#define XHTTPD_HEADER_NAME_CAP    64u
#define XHTTPD_HEADER_VALUE_CAP   256u
#define XHTTPD_MAX_HEADERS        32u
#define XHTTPD_MAX_TRAILERS       16u

/* Legacy source-compatibility constants; header strings are dynamically sized. */
#define XHTTPD_HEADER_F_OWN_STRINGS 0x00000001u

#define XHTTPD_REQ_F_NONE         0x00000000u
#define XHTTPD_REQ_F_KEEPALIVE    0x00000001u
#define XHTTPD_REQ_F_CHUNKED      0x00000002u
#define XHTTPD_REQ_F_UPGRADE      0x00000004u
#define XHTTPD_REQ_F_STREAMING    0x00000008u

#define XHTTPD_RESP_F_NONE        0x00000000u
#define XHTTPD_RESP_F_CLOSE       0x00000001u

#define XHTTPD_CONN_TIMER_NONE    0u
#define XHTTPD_CONN_TIMER_HEADER  1u
#define XHTTPD_CONN_TIMER_BODY    2u
#define XHTTPD_CONN_TIMER_IDLE    3u
#define XHTTPD_CONN_TIMER_WRITE   4u

#define XHTTPD_BODY_MODE_NONE     0u
#define XHTTPD_BODY_MODE_FIXED    1u
#define XHTTPD_BODY_MODE_CHUNKED  2u

#define XHTTPD_CHUNK_STATE_SIZE       0u
#define XHTTPD_CHUNK_STATE_DATA       1u
#define XHTTPD_CHUNK_STATE_DATA_CRLF  2u
#define XHTTPD_CHUNK_STATE_TRAILER    3u

#ifndef XHTTPD_BODY_STREAM_OK
#define XHTTPD_BODY_STREAM_OK              0u
#define XHTTPD_BODY_STREAM_BAD_REQUEST     400u
#define XHTTPD_BODY_STREAM_TOO_LARGE       413u
#define XHTTPD_BODY_STREAM_INTERNAL_ERROR  500u
#endif

typedef enum {
	XHTTPD_METHOD_UNKNOWN = 0,
	XHTTPD_METHOD_GET = 1,
	XHTTPD_METHOD_HEAD = 2,
	XHTTPD_METHOD_POST = 3,
	XHTTPD_METHOD_PUT = 4,
	XHTTPD_METHOD_DELETE = 5,
	XHTTPD_METHOD_PATCH = 6,
	XHTTPD_METHOD_OPTIONS = 7,
	XHTTPD_METHOD_CONNECT = 8,
	XHTTPD_METHOD_TRACE = 9
} xhttpdmethod;

typedef struct xrt_httpd_server xhttpdserver;
typedef struct xrt_httpd_conn xhttpdconn;

typedef struct {
	const char* sName;
	const char* sValue;
	uint32 iFlags;
} xhttpdheader;

typedef struct {
	uint32 iFlags;
	uint32 iHeaderCount;
	uint32 iHeaderCap;
	uint32 iTrailerCount;
	uint32 iTrailerCap;
	uint32 iMethod;
	int64_t iContentLength;
	const char* sMethod;
	const char* sTarget;
	const char* sPath;
	const char* sQuery;
	const char* sVersion;
	xhttpdheader arrHeaders[XHTTPD_MAX_HEADERS];
	xhttpdheader* pHeaders;
	xhttpdheader arrTrailers[XHTTPD_MAX_TRAILERS];
	xhttpdheader* pTrailers;
	char* pStorage;
	char* pBody;
	size_t iBodyLen;
	size_t iBodyCap;
	/* Borrowed through xrtHttpdRequestContext(); retain before asynchronous use. */
	xhttpcontext* pContext;
} xhttpdrequest;

typedef struct {
	uint32 iStatusCode;
	uint32 iFlags;
	uint32 iHeaderCount;
	uint32 iHeaderCap;
	const char* sReason;
	char* pReasonStorage;
	xhttpdheader arrHeaders[XHTTPD_MAX_HEADERS];
	xhttpdheader* pHeaders;
	char* pBody;
	size_t iBodyLen;
	size_t iBodyCap;
	uint32 iTrailerCount;
	uint32 iTrailerCap;
	xhttpdheader arrTrailers[XHTTPD_MAX_TRAILERS];
	xhttpdheader* pTrailers;
} xhttpdresponse;

#define XHTTPD_CONFIG_VERSION 1u
#define XHTTPD_EVENTS_VERSION 1u

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	xnetaddr tBindAddr;
	uint32 iFlags;
	uint32 iBacklog;
	uint32 iRecvLimit;
	uint32 iBodyLimit;
	uint32 iHeaderLimit;
	uint32 iHeaderBytesLimit;
	uint32 iStartLineBytesLimit;
	uint32 iHeaderLineBytesLimit;
	uint32 iChunkLineBytesLimit;
	uint32 iTrailerLimit;
	uint32 iTrailerBytesLimit;
	uint32 iHeaderTimeoutMs;
	uint32 iBodyTimeoutMs;
	uint32 iIdleTimeoutMs;
	uint32 iWriteTimeoutMs;
	const xtlsconfig* pTlsConfig;
} xhttpdconfig;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	void (*OnOpen)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn);
	bool (*OnRequest)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq, xhttpdresponse* pResp);
	xfuture* (*OnRequestAsync)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq);
	bool (*OnRequestBodyBegin)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq);
	bool (*OnRequestBody)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq, const void* pData, size_t iLen);
	bool (*OnRequestBodyEnd)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq, xhttpdresponse* pResp);
	xfuture* (*OnRequestBodyEndAsync)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq);
	void (*OnClose)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, xnet_result iReason);
	void (*OnError)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, int iSysErr);
} xhttpdevents;

#define XHTTPD_CONFIG_V1_SIZE ((uint32)sizeof(xhttpdconfig))
#define XHTTPD_EVENTS_V1_SIZE ((uint32)sizeof(xhttpdevents))
#endif

#ifndef XHTTPD_REQ_F_STREAMING
	#define XHTTPD_REQ_F_STREAMING    0x00000008u
#endif
#ifndef XHTTPD_BODY_MODE_NONE
	#define XHTTPD_BODY_MODE_NONE     0u
	#define XHTTPD_BODY_MODE_FIXED    1u
	#define XHTTPD_BODY_MODE_CHUNKED  2u
#endif
#ifndef XHTTPD_CHUNK_STATE_SIZE
	#define XHTTPD_CHUNK_STATE_SIZE       0u
	#define XHTTPD_CHUNK_STATE_DATA       1u
	#define XHTTPD_CHUNK_STATE_DATA_CRLF  2u
	#define XHTTPD_CHUNK_STATE_TRAILER    3u
#endif
#ifndef XHTTPD_BODY_STREAM_OK
	#define XHTTPD_BODY_STREAM_OK              0u
	#define XHTTPD_BODY_STREAM_BAD_REQUEST     400u
	#define XHTTPD_BODY_STREAM_TOO_LARGE       413u
	#define XHTTPD_BODY_STREAM_INTERNAL_ERROR  500u
#endif

struct xrt_httpd_conn {
	struct xrt_httpd_conn* pNext;
	volatile long iCleanupPosted;
	volatile long iCloseNotified;
	volatile long iConnLock;
	volatile long iRefCount;
	volatile long iTimerState;
	xhttpdserver* pServer;
	xhttpdserver* pServerRef;
	xnetstream* pStream;
	xhttpdrequest* pRequest;
	uint32 iTimerKind;
	uint32 iTimerGen;
	bool bResponseInFlight;
	bool bResponseCommitted;
	bool bResponseDrained;
	bool bResponseStreaming;
	bool bResponseChunked;
	bool bRequestStreaming;
	bool bAsyncPending;
	bool bKeepAlive;
	bool bContinueSent;
	bool bUpgraded;
	char* pResponseEndBytes;
	size_t iResponseEndLen;
	uint32 iBodyMode;
	uint32 iChunkState;
	uint64 iBodyRemain;
	uint64 iBodyReceived;
	uint64 iChunkRemain;
};

struct xrt_httpd_server {
	volatile long iRefCount;
	xnetengine* pEngine;
	xnetlistener* pListener;
	xhttpdconfig tConfig;
	xtlsconfig tTlsConfig;
	xhttpdevents tEvents;
	ptr pUserData;
	volatile long iConnLock;
	volatile long bRunning;
	volatile long bDraining;
	xhttpdconn* pConnHead;
	bool bHasTlsConfig;
	char* sTlsCertFile;
	char* sTlsKeyFile;
	char* sTlsCaFile;
	char* sTlsCrlFile;
	char* sTlsHostName;
	void* pTlsCertData;
	void* pTlsKeyData;
	void* pTlsCaData;
	void* pTlsCrlData;
	xtlsresume* pTlsResume;
};

typedef struct {
	xhttpdconn* pConn;
	xfuture* pFuture;
} __xhttpd_async_ctx;

typedef struct {
	xhttpdconn* pConn;
	char* pBytes;
	size_t iLen;
	bool bClose;
} __xhttpd_send_task;

typedef struct {
	xhttpdconn* pConn;
	xnetstream* pStream;
	bool bHandoff;
} __xhttpd_upgrade_task;


// 内部函数：__xhttpdToLower
static char __xhttpdToLower(char ch)
{
	if ( ch >= 'A' && ch <= 'Z' ) { return (char)(ch + 32); }
	return ch;
}


// 内部函数：字符串相等忽略大小写相关处理
static bool __xhttpdStrEqNoCase(const char* sA, const char* sB)
{
	size_t i = 0;
	if ( !sA || !sB ) { return false; }
	while ( sA[i] && sB[i] ) {
		if ( __xhttpdToLower(sA[i]) != __xhttpdToLower(sB[i]) ) { return false; }
		i++;
	}
	return sA[i] == '\0' && sB[i] == '\0';
}


// 内部函数：__xhttpdAtomicAdd
static long UNUSED_ATTR __xhttpdAtomicAdd(volatile long* pValue, long iDelta)
{
	return __xnetAtomicAddFetch32(pValue, iDelta);
}


// 内部函数：__xhttpdAtomicCompareExchange
static long __xhttpdAtomicCompareExchange(volatile long* pValue, long iExchange, long iComparand)
{
	return __xnetAtomicCompareExchange32(pValue, iExchange, iComparand);
}


// 内部函数：__xhttpdAtomicLoad
static long __xhttpdAtomicLoad(volatile long* pValue)
{
	return __xnetAtomicLoad32(pValue);
}


// 内部函数：__xhttpdAtomicLoadConst
static long __xhttpdAtomicLoadConst(const volatile long* pValue)
{
	return __xnetAtomicLoad32((volatile long*)pValue);
}


// 内部函数：休眠 0
static void __xhttpdSleep0(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(0);
	#else
		sched_yield();
	#endif
}


// 内部函数：锁定
static void __xhttpdLock(volatile long* pLock)
{
	if ( !pLock ) {
		xrtSetError("xhttpd lock pointer is null.", FALSE);
		return;
	}
	while ( __xhttpdAtomicCompareExchange(pLock, 1, 0) != 0 ) {
		__xhttpdSleep0();
	}
}


// 内部函数：解锁
static void __xhttpdUnlock(volatile long* pLock)
{
	if ( !pLock ) {
		xrtSetError("xhttpd unlock pointer is null.", FALSE);
		return;
	}
	(void)__xnetAtomicExchange32(pLock, 0);
}


// 内部函数：复制 Token
// 内部函数：解析 HTTP 方法 ID
static uint32 __xhttpdParseMethodID(const char* sMethod)
{
	if ( !sMethod ) { return XHTTPD_METHOD_UNKNOWN; }
	switch ( strlen(sMethod) ) {
		case 3:
			if ( sMethod[0] == 'G' && sMethod[1] == 'E' && sMethod[2] == 'T' ) { return XHTTPD_METHOD_GET; }
			if ( sMethod[0] == 'P' && sMethod[1] == 'U' && sMethod[2] == 'T' ) { return XHTTPD_METHOD_PUT; }
			break;
		case 4:
			if ( sMethod[0] == 'H' && sMethod[1] == 'E' && sMethod[2] == 'A' && sMethod[3] == 'D' ) { return XHTTPD_METHOD_HEAD; }
			if ( sMethod[0] == 'P' && sMethod[1] == 'O' && sMethod[2] == 'S' && sMethod[3] == 'T' ) { return XHTTPD_METHOD_POST; }
			break;
		case 5:
			if ( sMethod[0] == 'P' && sMethod[1] == 'A' && sMethod[2] == 'T' && sMethod[3] == 'C' && sMethod[4] == 'H' ) { return XHTTPD_METHOD_PATCH; }
			if ( sMethod[0] == 'T' && sMethod[1] == 'R' && sMethod[2] == 'A' && sMethod[3] == 'C' && sMethod[4] == 'E' ) { return XHTTPD_METHOD_TRACE; }
			break;
		case 6:
			if ( sMethod[0] == 'D' && sMethod[1] == 'E' && sMethod[2] == 'L' && sMethod[3] == 'E' && sMethod[4] == 'T' && sMethod[5] == 'E' ) { return XHTTPD_METHOD_DELETE; }
			break;
		case 7:
			if ( sMethod[0] == 'O' && sMethod[1] == 'P' && sMethod[2] == 'T' && sMethod[3] == 'I' && sMethod[4] == 'O' && sMethod[5] == 'N' && sMethod[6] == 'S' ) { return XHTTPD_METHOD_OPTIONS; }
			if ( sMethod[0] == 'C' && sMethod[1] == 'O' && sMethod[2] == 'N' && sMethod[3] == 'N' && sMethod[4] == 'E' && sMethod[5] == 'C' && sMethod[6] == 'T' ) { return XHTTPD_METHOD_CONNECT; }
			break;
		default:
			break;
	}
	return XHTTPD_METHOD_UNKNOWN;
}


// 内部函数：复制内存
static void* __xhttpdDupBytes(const void* pData, size_t iLen)
{
	void* pCopy;
	if ( !pData || iLen == 0u ) { return NULL; }
	pCopy = XNET_ALLOC(iLen);
	if ( !pCopy ) { return NULL; }
	memcpy(pCopy, pData, iLen);
	return pCopy;
}


// 内部函数：复制字符串
static char* __xhttpdDupStr(const char* sText)
{
	size_t iLen;
	char* sCopy;
	if ( !sText ) { return NULL; }
	iLen = strlen(sText);
	if ( iLen == SIZE_MAX ) { return NULL; }
	sCopy = (char*)XNET_ALLOC(iLen + 1u);
	if ( !sCopy ) { return NULL; }
	memcpy(sCopy, sText, iLen + 1u);
	return sCopy;
}


// 内部函数：检查 HTTP 头名称
static bool __xhttpdValidHeaderName(const char* sName)
{
	const unsigned char* p;
	if ( !sName || !sName[0] ) { return false; }
	for ( p = (const unsigned char*)sName; *p; ++p ) {
		if ( *p <= 32u || *p >= 127u || *p == ':' ) { return false; }
	}
	return true;
}


// 内部函数：检查 HTTP 头值
static bool __xhttpdValidHeaderValue(const char* sValue)
{
	const unsigned char* p;
	if ( !sValue ) { return false; }
	for ( p = (const unsigned char*)sValue; *p; ++p ) {
		if ( *p == '\r' || *p == '\n' ) { return false; }
	}
	return true;
}


// 内部函数：检查 HTTP reason phrase
static bool __xhttpdValidReasonPhrase(const char* sReason)
{
	const unsigned char* p;
	if ( !sReason ) { return true; }
	for ( p = (const unsigned char*)sReason; *p; ++p ) {
		if ( *p == '\r' || *p == '\n' ) { return false; }
	}
	return true;
}


// 内部函数：__xhttpdAppendBytes
static bool __xhttpdAppendBytes(char** ppBuf, size_t* pLen, size_t* pCap, const void* pData, size_t iDataLen)
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
static bool __xhttpdAppendText(char** ppBuf, size_t* pLen, size_t* pCap, const char* sText)
{
	return __xhttpdAppendBytes(ppBuf, pLen, pCap, sText, sText ? strlen(__xrt_cstr(sText)) : 0);
}


static char* __xhttpdStringCopyN(const char* sText, size_t iLen)
{
	char* sCopy;
	if ( !sText || iLen == SIZE_MAX ) { return NULL; }
	sCopy = (char*)XNET_ALLOC(iLen + 1u);
	if ( !sCopy ) { return NULL; }
	if ( iLen > 0u ) { memcpy(sCopy, sText, iLen); }
	sCopy[iLen] = '\0';
	return sCopy;
}


static void __xhttpdHeaderUnit(xhttpdheader* pHeader)
{
	if ( !pHeader ) { return; }
	if ( (pHeader->iFlags & XHTTPD_HEADER_F_OWN_STRINGS) != 0u ) {
		if ( pHeader->sName ) { XNET_FREE((ptr)pHeader->sName); }
		if ( pHeader->sValue ) { XNET_FREE((ptr)pHeader->sValue); }
	}
	memset(pHeader, 0, sizeof(xhttpdheader));
}


static bool __xhttpdHeaderSetCopyN(xhttpdheader* pHeader, const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen)
{
	char* sNameCopy;
	char* sValueCopy;
	if ( !pHeader || !sName || iNameLen == 0u || !sValue ) { return false; }
	sNameCopy = __xhttpdStringCopyN(sName, iNameLen);
	if ( !sNameCopy ) { return false; }
	sValueCopy = __xhttpdStringCopyN(sValue, iValueLen);
	if ( !sValueCopy ) {
		XNET_FREE(sNameCopy);
		return false;
	}
	__xhttpdHeaderUnit(pHeader);
	pHeader->sName = sNameCopy;
	pHeader->sValue = sValueCopy;
	pHeader->iFlags = XHTTPD_HEADER_F_OWN_STRINGS;
	return true;
}


static bool __xhttpdHeaderSetCopy(xhttpdheader* pHeader, const char* sName, const char* sValue)
{
	return sName && sValue
		? __xhttpdHeaderSetCopyN(pHeader, sName, strlen(sName), sValue, strlen(sValue))
		: false;
}


static void __xhttpdHeadersUnit(xhttpdheader* pHeaders, uint32 iCount)
{
	if ( !pHeaders ) { return; }
	for ( uint32 i = 0u; i < iCount; ++i ) { __xhttpdHeaderUnit(&pHeaders[i]); }
}

static void __xhttpdStreamOnRecv(ptr pOwner, xnetstream* pStream, xnetchain* pChain);
static void __xhttpdEmitServerError(xhttpdserver* pServer, xhttpdconn* pConn, int iSysErr);
static void __xhttpdConnCancelTimerKind(xhttpdconn* pConn, uint32 iKind);
static void __xhttpdConnCancelTimer(xhttpdconn* pConn);
static void __xhttpdConnArmTimer(xhttpdconn* pConn, uint32 iKind, bool bRestart);
static void __xhttpdConnArmWriteTimer(xhttpdconn* pConn, bool bRestart);
static void __xhttpdStreamOnTimer(ptr pOwner, xnetstream* pStream);


typedef struct {
	xhttpdconn* pConn;
} __xhttpd_conn_task;


static void __xhttpdServerClearTlsConfig(xhttpdserver* pServer);


// 内部函数：增加 HTTP 服务端生命周期引用
static xhttpdserver* __xhttpdServerAddRef(xhttpdserver* pServer)
{
	if ( !pServer ) { return NULL; }
	(void)__xhttpdAtomicAdd(&pServer->iRefCount, 1);
	return pServer;
}


// 内部函数：释放 HTTP 服务端生命周期引用
static void __xhttpdServerRelease(xhttpdserver* pServer)
{
	if ( !pServer ) { return; }
	if ( __xhttpdAtomicAdd(&pServer->iRefCount, -1) != 0 ) { return; }
	__xhttpdServerClearTlsConfig(pServer);
	XNET_FREE(pServer);
}


// 内部函数：__xhttpdRequestCreate
static xhttpdrequest* __xhttpdRequestCreate(void)
{
	xhttpdrequest* pReq = (xhttpdrequest*)XNET_ALLOC(sizeof(xhttpdrequest));
	if ( !pReq ) { return NULL; }
	xrtHttpdRequestInit(pReq);
	pReq->pContext = xrtHttpContextCreate();
	if ( !pReq->pContext ) {
		xrtHttpdRequestUnit(pReq);
		XNET_FREE(pReq);
		return NULL;
	}
	return pReq;
}


// 内部函数：__xhttpdRequestDestroy
static void __xhttpdRequestDestroy(xhttpdrequest* pReq)
{
	if ( !pReq ) { return; }
	xrtHttpdRequestUnit(pReq);
	XNET_FREE(pReq);
}


// 内部函数：__xhttpdConnAddRef
static xhttpdconn* __xhttpdConnAddRef(xhttpdconn* pConn)
{
	if ( !pConn ) { return NULL; }
	(void)__xhttpdAtomicAdd(&pConn->iRefCount, 1);
	return pConn;
}


static void __xhttpdConnClearResponseEndLocked(xhttpdconn* pConn)
{
	if ( !pConn ) { return; }
	if ( pConn->pResponseEndBytes ) { XNET_FREE(pConn->pResponseEndBytes); }
	pConn->pResponseEndBytes = NULL;
	pConn->iResponseEndLen = 0u;
}


// 内部函数：__xhttpdConnRelease
static void __xhttpdConnRelease(xhttpdconn* pConn)
{
	xhttpdserver* pServerRef;
	if ( !pConn ) { return; }
	if ( __xhttpdAtomicAdd(&pConn->iRefCount, -1) != 0 ) { return; }
	pServerRef = pConn->pServerRef;
	pConn->pServerRef = NULL;
	__xhttpdRequestDestroy(pConn->pRequest);
	pConn->pRequest = NULL;
	__xhttpdConnClearResponseEndLocked(pConn);
	XNET_FREE(pConn);
	__xhttpdServerRelease(pServerRef);
}


// 内部函数：通知连接关闭；正常关闭与服务端强制停止只能通知一次
static void __xhttpdConnNotifyClose(xhttpdconn* pConn, xnet_result iReason)
{
	xhttpdserver* pServer;
	xhttpcontext* pContext = NULL;
	if ( !pConn ) { return; }
	if ( __xhttpdAtomicCompareExchange(&pConn->iCloseNotified, 1, 0) != 0 ) { return; }
	__xhttpdLock(&pConn->iConnLock);
	if ( pConn->pRequest && pConn->pRequest->pContext ) {
		pContext = xrtHttpContextRetain(pConn->pRequest->pContext);
	}
	__xhttpdUnlock(&pConn->iConnLock);
	if ( pContext ) {
		(void)xrtHttpContextCancel(pContext);
		xrtHttpContextRelease(pContext);
	}
	pServer = pConn->pServer;
	if ( pServer && pServer->tEvents.OnClose ) {
		pServer->tEvents.OnClose(pServer->pUserData, pServer, pConn, iReason);
	}
}


// 内部函数：状态文本相关处理
static const char* __xhttpdStatusText(uint32 iStatusCode)
{
	switch ( iStatusCode ) {
		case 200: return "OK";
		case 201: return "Created";
		case 204: return "No Content";
		case 400: return "Bad Request";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 406: return "Not Acceptable";
		case 408: return "Request Timeout";
		case 413: return "Payload Too Large";
		case 417: return "Expectation Failed";
		case 431: return "Request Header Fields Too Large";
		case 500: return "Internal Server Error";
		case 501: return "Not Implemented";
		case 503: return "Service Unavailable";
		default: return "Unknown Status";
	}
}


// 获取 HTTP 服务端默认状态文本
XXAPI const char* xrtHttpdStatusText(uint32 iStatusCode)
{
	return __xhttpdStatusText(iStatusCode);
}


// 内部函数：__xhttpdResponseHasHeader
static bool __xhttpdResponseHasHeader(const xhttpdresponse* pResp, const char* sName)
{
	if ( !pResp || !sName ) { return false; }
	for ( uint32 i = 0; i < pResp->iHeaderCount; ++i ) {
		if ( __xhttpdStrEqNoCase(pResp->pHeaders[i].sName, sName) ) { return true; }
	}
	return false;
}


// 内部函数：确保响应头表拥有足够容量，常见路径使用内联小数组，超过后再动态扩容。
static bool __xhttpdRequestEnsureHeaderCap(xhttpdrequest* pReq, uint32 iNeed)
{
	xhttpdheader* pNew;
	uint32 iNewCap;
	if ( !pReq ) { return false; }
	if ( pReq->pHeaders == NULL ) {
		pReq->pHeaders = pReq->arrHeaders;
		pReq->iHeaderCap = XHTTPD_MAX_HEADERS;
	}
	if ( iNeed <= pReq->iHeaderCap ) { return true; }
	iNewCap = pReq->iHeaderCap ? pReq->iHeaderCap : XHTTPD_MAX_HEADERS;
	while ( iNewCap < iNeed ) {
		if ( iNewCap > UINT32_MAX / 2u ) { return false; }
		iNewCap *= 2u;
	}
	pNew = (xhttpdheader*)XNET_ALLOC(sizeof(xhttpdheader) * iNewCap);
	if ( !pNew ) { return false; }
	memset(pNew, 0, sizeof(xhttpdheader) * iNewCap);
	if ( pReq->iHeaderCount > 0u ) {
		memcpy(pNew, pReq->pHeaders, sizeof(xhttpdheader) * pReq->iHeaderCount);
	}
	if ( pReq->pHeaders != pReq->arrHeaders ) { XNET_FREE(pReq->pHeaders); }
	pReq->pHeaders = pNew;
	pReq->iHeaderCap = iNewCap;
	return true;
}


static bool __xhttpdSizeAdd(size_t* pTotal, size_t iValue)
{
	if ( !pTotal || iValue > SIZE_MAX - *pTotal ) { return false; }
	*pTotal += iValue;
	return true;
}


static void __xhttpdResponseRemoveHeader(xhttpdresponse* pResp, const char* sName)
{
	uint32 i = 0u;
	if ( !pResp || !sName ) { return; }
	while ( i < pResp->iHeaderCount ) {
		if ( !__xhttpdStrEqNoCase(pResp->pHeaders[i].sName, sName) ) {
			i++;
			continue;
		}
		__xhttpdHeaderUnit(&pResp->pHeaders[i]);
		pResp->iHeaderCount--;
		if ( i < pResp->iHeaderCount ) {
			memmove(&pResp->pHeaders[i], &pResp->pHeaders[i + 1u],
				sizeof(xhttpdheader) * (pResp->iHeaderCount - i));
		}
		memset(&pResp->pHeaders[pResp->iHeaderCount], 0, sizeof(xhttpdheader));
	}
}


static bool __xhttpdStatusForbidsBody(uint32 iStatusCode)
{
	return (iStatusCode >= 100u && iStatusCode < 200u) || iStatusCode == 204u || iStatusCode == 304u;
}


static bool __xhttpdStatusForbidsFraming(uint32 iStatusCode)
{
	return (iStatusCode >= 100u && iStatusCode < 200u) || iStatusCode == 204u;
}


static bool __xhttpdRequestEnsureTrailerCap(xhttpdrequest* pReq, uint32 iNeed)
{
	xhttpdheader* pNew;
	uint32 iNewCap;
	if ( !pReq ) { return false; }
	if ( pReq->pTrailers == NULL ) {
		pReq->pTrailers = pReq->arrTrailers;
		pReq->iTrailerCap = XHTTPD_MAX_TRAILERS;
	}
	if ( iNeed <= pReq->iTrailerCap ) { return true; }
	iNewCap = pReq->iTrailerCap ? pReq->iTrailerCap : XHTTPD_MAX_TRAILERS;
	while ( iNewCap < iNeed ) {
		if ( iNewCap > UINT32_MAX / 2u ) { return false; }
		iNewCap *= 2u;
	}
	pNew = (xhttpdheader*)XNET_ALLOC(sizeof(xhttpdheader) * iNewCap);
	if ( !pNew ) { return false; }
	memset(pNew, 0, sizeof(xhttpdheader) * iNewCap);
	if ( pReq->iTrailerCount > 0u ) { memcpy(pNew, pReq->pTrailers, sizeof(xhttpdheader) * pReq->iTrailerCount); }
	if ( pReq->pTrailers != pReq->arrTrailers ) { XNET_FREE(pReq->pTrailers); }
	pReq->pTrailers = pNew;
	pReq->iTrailerCap = iNewCap;
	return true;
}


static bool __xhttpdResponseEnsureHeaderCap(xhttpdresponse* pResp, uint32 iNeed)
{
	xhttpdheader* pNew;
	uint32 iNewCap;
	if ( !pResp ) { return false; }
	if ( pResp->pHeaders == NULL ) {
		pResp->pHeaders = pResp->arrHeaders;
		pResp->iHeaderCap = XHTTPD_MAX_HEADERS;
	}
	if ( iNeed <= pResp->iHeaderCap ) { return true; }
	iNewCap = pResp->iHeaderCap ? pResp->iHeaderCap : XHTTPD_MAX_HEADERS;
	while ( iNewCap < iNeed ) {
		if ( iNewCap > UINT32_MAX / 2u ) { return false; }
		iNewCap *= 2u;
	}
	pNew = (xhttpdheader*)XNET_ALLOC(sizeof(xhttpdheader) * iNewCap);
	if ( !pNew ) { return false; }
	memset(pNew, 0, sizeof(xhttpdheader) * iNewCap);
	if ( pResp->iHeaderCount > 0u ) {
		memcpy(pNew, pResp->pHeaders, sizeof(xhttpdheader) * pResp->iHeaderCount);
	}
	if ( pResp->pHeaders != pResp->arrHeaders ) { XNET_FREE(pResp->pHeaders); }
	pResp->pHeaders = pNew;
	pResp->iHeaderCap = iNewCap;
	return true;
}


// 内部函数：确保 body 拥有足够容量，iBodyCap 表示不含末尾 NUL 的有效容量。
static bool __xhttpdBodyReserve(char** ppBody, size_t* pLen, size_t* pCap, size_t iNeed)
{
	char* pNew;
	size_t iNewCap;
	size_t iLen;
	if ( !ppBody || !pLen || !pCap ) { return false; }
	iLen = *pLen;
	if ( iLen > iNeed ) { iNeed = iLen; }
	if ( iNeed == 0u ) { return true; }
	if ( iNeed == SIZE_MAX ) { return false; }
	if ( *ppBody && *pCap >= iNeed ) { return true; }
	iNewCap = *pCap ? *pCap : 256u;
	while ( iNewCap < iNeed ) {
		if ( iNewCap > (SIZE_MAX - 1u) / 2u ) {
			iNewCap = iNeed;
			break;
		}
		iNewCap *= 2u;
	}
	if ( iNewCap < iNeed || iNewCap == SIZE_MAX ) { return false; }
	pNew = (char*)XNET_ALLOC(iNewCap + 1u);
	if ( !pNew ) { return false; }
	if ( *ppBody && iLen > 0u ) { memcpy(pNew, *ppBody, iLen); }
	if ( *ppBody && *pCap > 0u ) { XNET_FREE(*ppBody); }
	pNew[iLen] = '\0';
	*ppBody = pNew;
	*pCap = iNewCap;
	return true;
}


// 内部函数：释放 body 自持缓冲。
static void __xhttpdBodyRelease(char** ppBody, size_t* pLen, size_t* pCap)
{
	if ( !ppBody || !pLen || !pCap ) { return; }
	if ( *ppBody && *pCap > 0u ) { XNET_FREE(*ppBody); }
	*ppBody = NULL;
	*pLen = 0u;
	*pCap = 0u;
}


// 内部函数：复制设置 body。
static bool __xhttpdBodySetCopy(char** ppBody, size_t* pLen, size_t* pCap, const void* pData, size_t iLen)
{
	if ( !ppBody || !pLen || !pCap || (!pData && iLen > 0u) ) { return false; }
	if ( iLen == SIZE_MAX ) { return false; }
	if ( iLen == 0u ) {
		*pLen = 0u;
		if ( *ppBody ) { (*ppBody)[0] = '\0'; }
		return true;
	}
	*pLen = 0u;
	if ( !__xhttpdBodyReserve(ppBody, pLen, pCap, iLen) ) { return false; }
	memcpy(*ppBody, pData, iLen);
	(*ppBody)[iLen] = '\0';
	*pLen = iLen;
	return true;
}


// 内部函数：追加复制 body。
static bool __xhttpdBodyAppendCopy(char** ppBody, size_t* pLen, size_t* pCap, const void* pData, size_t iLen)
{
	size_t iOldLen;
	size_t iNeed;
	if ( !ppBody || !pLen || !pCap || (!pData && iLen > 0u) ) { return false; }
	if ( iLen == 0u ) { return true; }
	iOldLen = *pLen;
	if ( iOldLen > SIZE_MAX - iLen ) { return false; }
	iNeed = iOldLen + iLen;
	if ( !__xhttpdBodyReserve(ppBody, pLen, pCap, iNeed) ) { return false; }
	memcpy(*ppBody + iOldLen, pData, iLen);
	(*ppBody)[iNeed] = '\0';
	*pLen = iNeed;
	return true;
}


// 内部函数：判断是否包含 Token 忽略大小写
static bool __xhttpdContainsTokenNoCase(const char* sValue, const char* sToken)
{
	return xrtHttpHeaderContainsToken(sValue, sToken);
}


// 内部函数：判断是否为框架控制的响应头
static bool __xhttpdResponseHeaderControlled(const char* sName)
{
	return __xhttpdStrEqNoCase(sName, "Content-Length") ||
		__xhttpdStrEqNoCase(sName, "Transfer-Encoding") ||
		__xhttpdStrEqNoCase(sName, "Connection");
}


static bool __xhttpdForbiddenTrailer(const char* sName)
{
	return __xhttpdStrEqNoCase(sName, "Content-Length") ||
		__xhttpdStrEqNoCase(sName, "Transfer-Encoding") ||
		__xhttpdStrEqNoCase(sName, "Host") ||
		__xhttpdStrEqNoCase(sName, "Connection") ||
		__xhttpdStrEqNoCase(sName, "Upgrade") ||
		__xhttpdStrEqNoCase(sName, "Trailer");
}


// 获取 HTTP 服务端 request 头部
XXAPI const char* xrtHttpdRequestHeader(const xhttpdrequest* pReq, const char* sName)
{
	const xhttpdheader* pHeaders;
	if ( !pReq || !sName ) { return NULL; }
	pHeaders = pReq->pHeaders ? pReq->pHeaders : pReq->arrHeaders;
	for ( uint32 i = 0; i < pReq->iHeaderCount; ++i ) {
		if ( __xhttpdStrEqNoCase(pHeaders[i].sName, sName) ) { return pHeaders[i].sValue; }
	}
	return NULL;
}


// 获取 HTTP 服务端 request 方法 ID
// 获取 HTTP 服务端 request 头部数量
XXAPI uint32 xrtHttpdRequestHeaderCount(const xhttpdrequest* pReq)
{
	return pReq ? pReq->iHeaderCount : 0u;
}


// 按索引获取 HTTP 服务端 request 头部名称
XXAPI const char* xrtHttpdRequestHeaderNameAt(const xhttpdrequest* pReq, uint32 iIndex)
{
	const xhttpdheader* pHeaders;
	if ( !pReq || iIndex >= pReq->iHeaderCount ) { return NULL; }
	pHeaders = pReq->pHeaders ? pReq->pHeaders : pReq->arrHeaders;
	return pHeaders[iIndex].sName;
}


// 按索引获取 HTTP 服务端 request 头部值
XXAPI const char* xrtHttpdRequestHeaderValueAt(const xhttpdrequest* pReq, uint32 iIndex)
{
	const xhttpdheader* pHeaders;
	if ( !pReq || iIndex >= pReq->iHeaderCount ) { return NULL; }
	pHeaders = pReq->pHeaders ? pReq->pHeaders : pReq->arrHeaders;
	return pHeaders[iIndex].sValue;
}


XXAPI const char* xrtHttpdRequestTrailer(const xhttpdrequest* pReq, const char* sName)
{
	const xhttpdheader* pTrailers;
	if ( !pReq || !sName ) { return NULL; }
	pTrailers = pReq->pTrailers ? pReq->pTrailers : pReq->arrTrailers;
	for ( uint32 i = 0u; i < pReq->iTrailerCount; ++i ) {
		if ( __xhttpdStrEqNoCase(pTrailers[i].sName, sName) ) { return pTrailers[i].sValue; }
	}
	return NULL;
}


XXAPI uint32 xrtHttpdRequestTrailerCount(const xhttpdrequest* pReq)
{
	return pReq ? pReq->iTrailerCount : 0u;
}


XXAPI const char* xrtHttpdRequestTrailerNameAt(const xhttpdrequest* pReq, uint32 iIndex)
{
	const xhttpdheader* pTrailers;
	if ( !pReq || iIndex >= pReq->iTrailerCount ) { return NULL; }
	pTrailers = pReq->pTrailers ? pReq->pTrailers : pReq->arrTrailers;
	return pTrailers[iIndex].sName;
}


XXAPI const char* xrtHttpdRequestTrailerValueAt(const xhttpdrequest* pReq, uint32 iIndex)
{
	const xhttpdheader* pTrailers;
	if ( !pReq || iIndex >= pReq->iTrailerCount ) { return NULL; }
	pTrailers = pReq->pTrailers ? pReq->pTrailers : pReq->arrTrailers;
	return pTrailers[iIndex].sValue;
}


XXAPI xhttpcontext* xrtHttpdRequestContext(const xhttpdrequest* pReq)
{
	return pReq ? pReq->pContext : NULL;
}


XXAPI uint32 xrtHttpdRequestMethod(const xhttpdrequest* pReq)
{
	return pReq ? pReq->iMethod : XHTTPD_METHOD_UNKNOWN;
}


// 获取 HTTP 服务端 response 头部
XXAPI const char* xrtHttpdResponseHeader(const xhttpdresponse* pResp, const char* sName)
{
	const xhttpdheader* pHeaders;
	if ( !pResp || !sName ) { return NULL; }
	pHeaders = pResp->pHeaders ? pResp->pHeaders : pResp->arrHeaders;
	for ( uint32 i = 0; i < pResp->iHeaderCount; ++i ) {
		if ( __xhttpdStrEqNoCase(pHeaders[i].sName, sName) ) { return pHeaders[i].sValue; }
	}
	return NULL;
}


XXAPI uint32 xrtHttpdResponseHeaderCount(const xhttpdresponse* pResp)
{
	return pResp ? pResp->iHeaderCount : 0u;
}


XXAPI const char* xrtHttpdResponseHeaderNameAt(const xhttpdresponse* pResp, uint32 iIndex)
{
	const xhttpdheader* pHeaders;
	if ( !pResp || iIndex >= pResp->iHeaderCount ) { return NULL; }
	pHeaders = pResp->pHeaders ? pResp->pHeaders : pResp->arrHeaders;
	return pHeaders[iIndex].sName;
}


XXAPI const char* xrtHttpdResponseHeaderValueAt(const xhttpdresponse* pResp, uint32 iIndex)
{
	const xhttpdheader* pHeaders;
	if ( !pResp || iIndex >= pResp->iHeaderCount ) { return NULL; }
	pHeaders = pResp->pHeaders ? pResp->pHeaders : pResp->arrHeaders;
	return pHeaders[iIndex].sValue;
}


XXAPI const char* xrtHttpdResponseTrailer(const xhttpdresponse* pResp, const char* sName)
{
	const xhttpdheader* pTrailers;
	if ( !pResp || !sName ) { return NULL; }
	pTrailers = pResp->pTrailers ? pResp->pTrailers : pResp->arrTrailers;
	for ( uint32 i = 0u; i < pResp->iTrailerCount; ++i ) {
		if ( __xhttpdStrEqNoCase(pTrailers[i].sName, sName) ) { return pTrailers[i].sValue; }
	}
	return NULL;
}


XXAPI uint32 xrtHttpdResponseTrailerCount(const xhttpdresponse* pResp)
{
	return pResp ? pResp->iTrailerCount : 0u;
}


XXAPI const char* xrtHttpdResponseTrailerNameAt(const xhttpdresponse* pResp, uint32 iIndex)
{
	const xhttpdheader* pTrailers;
	if ( !pResp || iIndex >= pResp->iTrailerCount ) { return NULL; }
	pTrailers = pResp->pTrailers ? pResp->pTrailers : pResp->arrTrailers;
	return pTrailers[iIndex].sName;
}


XXAPI const char* xrtHttpdResponseTrailerValueAt(const xhttpdresponse* pResp, uint32 iIndex)
{
	const xhttpdheader* pTrailers;
	if ( !pResp || iIndex >= pResp->iTrailerCount ) { return NULL; }
	pTrailers = pResp->pTrailers ? pResp->pTrailers : pResp->arrTrailers;
	return pTrailers[iIndex].sValue;
}


// 初始化 HTTP 服务端配置
XXAPI void xrtHttpdConfigInit(xhttpdconfig* pCfg)
{
	if ( !pCfg ) { return; }
	memset(pCfg, 0, sizeof(xhttpdconfig));
	pCfg->iSize = XHTTPD_CONFIG_V1_SIZE;
	pCfg->iVersion = XHTTPD_CONFIG_VERSION;
	xrtNetAddrInitAny(&pCfg->tBindAddr, AF_INET, 0);
	pCfg->iBacklog = 128u;
	pCfg->iRecvLimit = 1024u * 1024u;
	pCfg->iBodyLimit = pCfg->iRecvLimit;
	pCfg->iHeaderLimit = 100u;
	pCfg->iHeaderBytesLimit = 64u * 1024u;
	pCfg->iStartLineBytesLimit = XCODEC_HTTP1_DEFAULT_START_LINE_BYTES;
	pCfg->iHeaderLineBytesLimit = XCODEC_HTTP1_DEFAULT_HEADER_LINE_BYTES;
	pCfg->iChunkLineBytesLimit = XCODEC_HTTP1_DEFAULT_CHUNK_LINE_BYTES;
	pCfg->iTrailerLimit = XCODEC_HTTP1_DEFAULT_TRAILER_COUNT;
	pCfg->iTrailerBytesLimit = XCODEC_HTTP1_DEFAULT_TRAILER_BYTES;
	pCfg->iHeaderTimeoutMs = 30000u;
	pCfg->iBodyTimeoutMs = 30000u;
	pCfg->iIdleTimeoutMs = 60000u;
	pCfg->iWriteTimeoutMs = 30000u;
}


XXAPI void xrtHttpdEventsInit(xhttpdevents* pEvents)
{
	if ( !pEvents ) { return; }
	memset(pEvents, 0, sizeof(*pEvents));
	pEvents->iSize = XHTTPD_EVENTS_V1_SIZE;
	pEvents->iVersion = XHTTPD_EVENTS_VERSION;
}


static bool __xhttpdConfigCopy(xhttpdconfig* pDst, const xhttpdconfig* pSrc)
{
	size_t iCopy;
	if ( !pDst ) { return false; }
	xrtHttpdConfigInit(pDst);
	if ( !pSrc ) { return true; }
	if ( pSrc->iVersion != XHTTPD_CONFIG_VERSION ||
		pSrc->iSize < XHTTPD_CONFIG_V1_SIZE ) {
		return false;
	}
	iCopy = pSrc->iSize < sizeof(*pDst) ? (size_t)pSrc->iSize : sizeof(*pDst);
	memcpy(pDst, pSrc, iCopy);
	pDst->iSize = XHTTPD_CONFIG_V1_SIZE;
	pDst->iVersion = XHTTPD_CONFIG_VERSION;
	return true;
}


static bool __xhttpdEventsCopy(xhttpdevents* pDst, const xhttpdevents* pSrc)
{
	size_t iCopy;
	if ( !pDst ) { return false; }
	xrtHttpdEventsInit(pDst);
	if ( !pSrc ) { return true; }
	/* Source compatibility for pre-versioned code that zero-initialized the event table. */
	if ( pSrc->iSize == 0u && pSrc->iVersion == 0u ) {
		memcpy(pDst, pSrc, sizeof(*pDst));
		pDst->iSize = XHTTPD_EVENTS_V1_SIZE;
		pDst->iVersion = XHTTPD_EVENTS_VERSION;
		return true;
	}
	if ( pSrc->iVersion != XHTTPD_EVENTS_VERSION ||
		pSrc->iSize < XHTTPD_EVENTS_V1_SIZE ) {
		return false;
	}
	iCopy = pSrc->iSize < sizeof(*pDst) ? (size_t)pSrc->iSize : sizeof(*pDst);
	memcpy(pDst, pSrc, iCopy);
	pDst->iSize = XHTTPD_EVENTS_V1_SIZE;
	pDst->iVersion = XHTTPD_EVENTS_VERSION;
	return true;
}


// xrtHttpdRequestInit 相关处理
XXAPI void xrtHttpdRequestInit(xhttpdrequest* pReq)
{
	if ( !pReq ) { return; }
	memset(pReq, 0, sizeof(xhttpdrequest));
	pReq->pHeaders = pReq->arrHeaders;
	pReq->iHeaderCap = XHTTPD_MAX_HEADERS;
	pReq->pTrailers = pReq->arrTrailers;
	pReq->iTrailerCap = XHTTPD_MAX_TRAILERS;
	pReq->sMethod = "";
	pReq->sTarget = "";
	pReq->sPath = "";
	pReq->sQuery = "";
	pReq->sVersion = "";
	pReq->iContentLength = -1;
}


// xrtHttpdRequestUnit 相关处理
XXAPI void xrtHttpdRequestUnit(xhttpdrequest* pReq)
{
	xhttpdheader* pHeaders;
	xhttpdheader* pTrailers;
	if ( !pReq ) { return; }
	__xhttpdBodyRelease(&pReq->pBody, &pReq->iBodyLen, &pReq->iBodyCap);
	pHeaders = pReq->pHeaders ? pReq->pHeaders : pReq->arrHeaders;
	__xhttpdHeadersUnit(pHeaders, pReq->iHeaderCount);
	pTrailers = pReq->pTrailers ? pReq->pTrailers : pReq->arrTrailers;
	__xhttpdHeadersUnit(pTrailers, pReq->iTrailerCount);
	if ( pReq->pHeaders && pReq->pHeaders != pReq->arrHeaders ) {
		XNET_FREE(pReq->pHeaders);
		pReq->pHeaders = NULL;
	}
	if ( pReq->pTrailers && pReq->pTrailers != pReq->arrTrailers ) { XNET_FREE(pReq->pTrailers); }
	if ( pReq->pStorage ) { XNET_FREE(pReq->pStorage); }
	if ( pReq->pContext ) {
		(void)xrtHttpContextCancel(pReq->pContext);
		xrtHttpContextRelease(pReq->pContext);
		pReq->pContext = NULL;
	}
	memset(pReq, 0, sizeof(xhttpdrequest));
	pReq->iContentLength = -1;
}


// 预留 HTTP 服务端 request body 缓冲容量。
XXAPI bool xrtHttpdRequestReserveBody(xhttpdrequest* pReq, size_t iCap)
{
	return pReq ? __xhttpdBodyReserve(&pReq->pBody, &pReq->iBodyLen, &pReq->iBodyCap, iCap) : false;
}


// 复制设置 HTTP 服务端 request body，主要供测试、代理和自定义解析路径使用。
XXAPI bool xrtHttpdRequestSetBodyCopy(xhttpdrequest* pReq, const void* pData, size_t iLen)
{
	return pReq ? __xhttpdBodySetCopy(&pReq->pBody, &pReq->iBodyLen, &pReq->iBodyCap, pData, iLen) : false;
}


// 追加 HTTP 服务端 request body。
XXAPI bool xrtHttpdRequestAppendBodyCopy(xhttpdrequest* pReq, const void* pData, size_t iLen)
{
	return pReq ? __xhttpdBodyAppendCopy(&pReq->pBody, &pReq->iBodyLen, &pReq->iBodyCap, pData, iLen) : false;
}


// xrtHttpdResponseInit 相关处理
XXAPI void xrtHttpdResponseInit(xhttpdresponse* pResp)
{
	if ( !pResp ) { return; }
	memset(pResp, 0, sizeof(xhttpdresponse));
	pResp->pHeaders = pResp->arrHeaders;
	pResp->iHeaderCap = XHTTPD_MAX_HEADERS;
	pResp->pTrailers = pResp->arrTrailers;
	pResp->iTrailerCap = XHTTPD_MAX_TRAILERS;
	pResp->sReason = "";
	pResp->iStatusCode = 200u;
	pResp->iFlags = XHTTPD_RESP_F_CLOSE;
}


// xrtHttpdResponseUnit 相关处理
XXAPI void xrtHttpdResponseUnit(xhttpdresponse* pResp)
{
	xhttpdheader* pHeaders;
	xhttpdheader* pTrailers;
	if ( !pResp ) { return; }
	__xhttpdBodyRelease(&pResp->pBody, &pResp->iBodyLen, &pResp->iBodyCap);
	pHeaders = pResp->pHeaders ? pResp->pHeaders : pResp->arrHeaders;
	__xhttpdHeadersUnit(pHeaders, pResp->iHeaderCount);
	if ( pResp->pHeaders && pResp->pHeaders != pResp->arrHeaders ) {
		XNET_FREE(pResp->pHeaders);
		pResp->pHeaders = NULL;
	}
	pTrailers = pResp->pTrailers ? pResp->pTrailers : pResp->arrTrailers;
	__xhttpdHeadersUnit(pTrailers, pResp->iTrailerCount);
	if ( pResp->pTrailers && pResp->pTrailers != pResp->arrTrailers ) {
		XNET_FREE(pResp->pTrailers);
		pResp->pTrailers = NULL;
	}
	if ( pResp->pReasonStorage ) { XNET_FREE(pResp->pReasonStorage); }
	memset(pResp, 0, sizeof(xhttpdresponse));
}


// 预留 HTTP 服务端 response body 缓冲容量。
XXAPI bool xrtHttpdResponseReserveBody(xhttpdresponse* pResp, size_t iCap)
{
	return pResp ? __xhttpdBodyReserve(&pResp->pBody, &pResp->iBodyLen, &pResp->iBodyCap, iCap) : false;
}


// xrtHttpdResponseCreate 相关处理
XXAPI xhttpdresponse* xrtHttpdResponseCreate(void)
{
	xhttpdresponse* pResp = (xhttpdresponse*)XNET_ALLOC(sizeof(xhttpdresponse));
	if ( !pResp ) { return NULL; }
	xrtHttpdResponseInit(pResp);
	return pResp;
}


// xrtHttpdResponseDestroy 相关处理
XXAPI void xrtHttpdResponseDestroy(xhttpdresponse* pResp)
{
	if ( !pResp ) { return; }
	xrtHttpdResponseUnit(pResp);
	XNET_FREE(pResp);
}


// 设置 HTTP 服务端 response 状态
XXAPI bool xrtHttpdResponseSetStatusEx(xhttpdresponse* pResp, uint32 iStatusCode, const char* sReason)
{
	char* sCopy = NULL;
	if ( !pResp || iStatusCode < 100u || iStatusCode > 999u || !__xhttpdValidReasonPhrase(sReason) ) { return false; }
	if ( sReason && sReason[0] ) {
		sCopy = __xhttpdStringCopyN(sReason, strlen(sReason));
		if ( !sCopy ) { return false; }
	}
	if ( pResp->pReasonStorage ) { XNET_FREE(pResp->pReasonStorage); }
	pResp->pReasonStorage = sCopy;
	pResp->sReason = sCopy ? sCopy : "";
	pResp->iStatusCode = iStatusCode;
	return true;
}


static bool __xhttpdResponseEnsureTrailerCap(xhttpdresponse* pResp, uint32 iNeed)
{
	xhttpdheader* pNew;
	uint32 iNewCap;
	if ( !pResp ) { return false; }
	if ( pResp->pTrailers == NULL ) {
		pResp->pTrailers = pResp->arrTrailers;
		pResp->iTrailerCap = XHTTPD_MAX_TRAILERS;
	}
	if ( iNeed <= pResp->iTrailerCap ) { return true; }
	iNewCap = pResp->iTrailerCap ? pResp->iTrailerCap : XHTTPD_MAX_TRAILERS;
	while ( iNewCap < iNeed ) {
		if ( iNewCap > UINT32_MAX / 2u ) { return false; }
		iNewCap *= 2u;
	}
	pNew = (xhttpdheader*)XNET_ALLOC(sizeof(xhttpdheader) * iNewCap);
	if ( !pNew ) { return false; }
	memset(pNew, 0, sizeof(xhttpdheader) * iNewCap);
	if ( pResp->iTrailerCount > 0u ) {
		memcpy(pNew, pResp->pTrailers, sizeof(xhttpdheader) * pResp->iTrailerCount);
	}
	if ( pResp->pTrailers != pResp->arrTrailers ) { XNET_FREE(pResp->pTrailers); }
	pResp->pTrailers = pNew;
	pResp->iTrailerCap = iNewCap;
	return true;
}


XXAPI void xrtHttpdResponseSetStatus(xhttpdresponse* pResp, uint32 iStatusCode, const char* sReason)
{
	(void)xrtHttpdResponseSetStatusEx(pResp, iStatusCode, sReason);
}


static bool __xhttpdResponseSetHeaderN(xhttpdresponse* pResp, const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen, bool bRejectControlled)
{
	xhttpdheader tHeader;
	if ( !pResp || !sName || iNameLen == 0u || !sValue ) { return false; }
	memset(&tHeader, 0, sizeof(tHeader));
	if ( !__xhttpdHeaderSetCopyN(&tHeader, sName, iNameLen, sValue, iValueLen) ) { return false; }
	if ( !__xhttpdValidHeaderName(tHeader.sName) || !__xhttpdValidHeaderValue(tHeader.sValue) ||
		(bRejectControlled && __xhttpdResponseHeaderControlled(tHeader.sName)) ) {
		__xhttpdHeaderUnit(&tHeader);
		return false;
	}
	if ( !__xhttpdResponseEnsureHeaderCap(pResp, pResp->iHeaderCount + 1u) ) {
		__xhttpdHeaderUnit(&tHeader);
		return false;
	}
	for ( uint32 i = 0; i < pResp->iHeaderCount; ++i ) {
		if ( __xhttpdStrEqNoCase(pResp->pHeaders[i].sName, tHeader.sName) ) {
			__xhttpdHeaderUnit(&pResp->pHeaders[i]);
			pResp->pHeaders[i] = tHeader;
			return true;
		}
	}
	pResp->pHeaders[pResp->iHeaderCount++] = tHeader;
	return true;
}


// 设置 HTTP 服务端 response 头部
XXAPI bool xrtHttpdResponseSetHeader(xhttpdresponse* pResp, const char* sName, const char* sValue)
{
	return sName && sValue
		? __xhttpdResponseSetHeaderN(pResp, sName, strlen(sName), sValue, strlen(sValue), false)
		: false;
}


// 追加 HTTP 服务端 response 头部。
// 与 SetHeader 不同，该函数允许相同名称出现多次，主要用于 Set-Cookie 等多值头。
XXAPI bool xrtHttpdResponseAddHeader(xhttpdresponse* pResp, const char* sName, const char* sValue)
{
	xhttpdheader* pHeader;
	if ( !pResp || !sName || !sValue ) { return false; }
	if ( !__xhttpdValidHeaderName(sName) || !__xhttpdValidHeaderValue(sValue) ) { return false; }
	if ( !__xhttpdResponseEnsureHeaderCap(pResp, pResp->iHeaderCount + 1u) ) { return false; }
	pHeader = &pResp->pHeaders[pResp->iHeaderCount];
	if ( !__xhttpdHeaderSetCopy(pHeader, sName, sValue) ) { return false; }
	pResp->iHeaderCount++;
	return true;
}


XXAPI uint32 xrtHttpdResponseRemoveHeader(xhttpdresponse* pResp, const char* sName)
{
	uint32 iBefore;
	if ( !pResp || !sName || !sName[0] ) { return 0u; }
	iBefore = pResp->iHeaderCount;
	__xhttpdResponseRemoveHeader(pResp, sName);
	return iBefore - pResp->iHeaderCount;
}


XXAPI bool xrtHttpdResponseSetTrailer(xhttpdresponse* pResp, const char* sName, const char* sValue)
{
	xhttpdheader tTrailer;
	if ( !pResp || !sName || !sValue || !__xhttpdValidHeaderName(sName) ||
		!__xhttpdValidHeaderValue(sValue) || __xhttpdForbiddenTrailer(sName) ) {
		return false;
	}
	memset(&tTrailer, 0, sizeof(tTrailer));
	if ( !__xhttpdHeaderSetCopy(&tTrailer, sName, sValue) ) { return false; }
	if ( !__xhttpdResponseEnsureTrailerCap(pResp, pResp->iTrailerCount + 1u) ) {
		__xhttpdHeaderUnit(&tTrailer);
		return false;
	}
	for ( uint32 i = 0u; i < pResp->iTrailerCount; ++i ) {
		if ( __xhttpdStrEqNoCase(pResp->pTrailers[i].sName, sName) ) {
			__xhttpdHeaderUnit(&pResp->pTrailers[i]);
			pResp->pTrailers[i] = tTrailer;
			return true;
		}
	}
	pResp->pTrailers[pResp->iTrailerCount++] = tTrailer;
	return true;
}


XXAPI bool xrtHttpdResponseAddTrailer(xhttpdresponse* pResp, const char* sName, const char* sValue)
{
	xhttpdheader* pTrailer;
	if ( !pResp || !sName || !sValue || !__xhttpdValidHeaderName(sName) ||
		!__xhttpdValidHeaderValue(sValue) || __xhttpdForbiddenTrailer(sName) ) {
		return false;
	}
	if ( !__xhttpdResponseEnsureTrailerCap(pResp, pResp->iTrailerCount + 1u) ) { return false; }
	pTrailer = &pResp->pTrailers[pResp->iTrailerCount];
	if ( !__xhttpdHeaderSetCopy(pTrailer, sName, sValue) ) { return false; }
	pResp->iTrailerCount++;
	return true;
}


// xrtHttpdResponseSetBodyCopy 相关处理
XXAPI bool xrtHttpdResponseSetBodyCopy(xhttpdresponse* pResp, const void* pData, size_t iLen, const char* sContentType)
{
	if ( !pResp ) { return false; }
	if ( !__xhttpdBodySetCopy(&pResp->pBody, &pResp->iBodyLen, &pResp->iBodyCap, pData, iLen) ) { return false; }
	if ( sContentType && sContentType[0] ) {
		if ( !xrtHttpdResponseSetHeader(pResp, "Content-Type", sContentType) ) {
			(void)__xhttpdBodySetCopy(&pResp->pBody, &pResp->iBodyLen, &pResp->iBodyCap, NULL, 0u);
			return false;
		}
	}
	return true;
}


// 追加 HTTP 服务端 response body。
XXAPI bool xrtHttpdResponseAppendBodyCopy(xhttpdresponse* pResp, const void* pData, size_t iLen)
{
	return pResp ? __xhttpdBodyAppendCopy(&pResp->pBody, &pResp->iBodyLen, &pResp->iBodyCap, pData, iLen) : false;
}


// 内部函数：应用轻量响应头部块
static bool __xhttpdResponseApplyHeaderBlock(xhttpdresponse* pResp, const char* sHeaders)
{
	xrtheaderpair tHeader;
	size_t iLen;
	size_t iOff = 0u;
	if ( !pResp ) { return false; }
	if ( !sHeaders || !sHeaders[0] ) { return true; }
	iLen = strlen(sHeaders);
	while ( xrtHttpHeaderNextLineN(sHeaders, iLen, &iOff, &tHeader) ) {
		if ( !__xhttpdResponseSetHeaderN(
			pResp,
			tHeader.tName.sPtr,
			tHeader.tName.iLen,
			tHeader.tValue.sPtr,
			tHeader.tValue.iLen,
			true) ) { return false; }
	}
	if ( iOff < iLen ) {
		if ( !(iOff + 1u == iLen && sHeaders[iOff] == '\n') &&
			!(iOff + 2u == iLen && sHeaders[iOff] == '\r' && sHeaders[iOff + 1u] == '\n') ) {
			return false;
		}
	}
	return true;
}


// 一次性填充 HTTP 服务端响应对象
XXAPI bool xrtHttpdResponseReply(xhttpdresponse* pResp, uint32 iStatusCode, const char* sReason, const char* sHeaders, const void* pBody, size_t iBodyLen)
{
	if ( !pResp || (!pBody && iBodyLen > 0u) ) { return false; }
	xrtHttpdResponseUnit(pResp);
	xrtHttpdResponseInit(pResp);
	if ( !xrtHttpdResponseSetStatusEx(pResp, iStatusCode, sReason) ) { return false; }
	if ( !__xhttpdResponseApplyHeaderBlock(pResp, sHeaders) ) {
		xrtHttpdResponseUnit(pResp);
		xrtHttpdResponseInit(pResp);
		return false;
	}
	if ( !xrtHttpdResponseSetBodyCopy(pResp, pBody, iBodyLen, NULL) ) {
		xrtHttpdResponseUnit(pResp);
		xrtHttpdResponseInit(pResp);
		return false;
	}
	return true;
}


// 内部函数：__xhttpdResponseCopy
static bool __xhttpdResponseCopy(xhttpdresponse* pDst, const xhttpdresponse* pSrc)
{
	const xhttpdheader* pSrcHeaders;
	const xhttpdheader* pSrcTrailers;
	if ( !pDst || !pSrc ) { return false; }
	xrtHttpdResponseInit(pDst);
	pDst->iFlags = pSrc->iFlags;
	if ( !xrtHttpdResponseSetStatusEx(pDst, pSrc->iStatusCode, pSrc->sReason) ) {
		xrtHttpdResponseUnit(pDst);
		return false;
	}
	if ( pSrc->iHeaderCount > 0u ) {
		pSrcHeaders = pSrc->pHeaders ? pSrc->pHeaders : pSrc->arrHeaders;
		if ( !__xhttpdResponseEnsureHeaderCap(pDst, pSrc->iHeaderCount) ) {
			xrtHttpdResponseUnit(pDst);
			return false;
		}
		for ( uint32 i = 0u; i < pSrc->iHeaderCount; ++i ) {
			if ( !__xhttpdHeaderSetCopy(&pDst->pHeaders[i], pSrcHeaders[i].sName, pSrcHeaders[i].sValue) ) {
				pDst->iHeaderCount = i;
				xrtHttpdResponseUnit(pDst);
				return false;
			}
			pDst->iHeaderCount = i + 1u;
		}
	}
	if ( pSrc->iTrailerCount > 0u ) {
		pSrcTrailers = pSrc->pTrailers ? pSrc->pTrailers : pSrc->arrTrailers;
		if ( !__xhttpdResponseEnsureTrailerCap(pDst, pSrc->iTrailerCount) ) {
			xrtHttpdResponseUnit(pDst);
			return false;
		}
		for ( uint32 i = 0u; i < pSrc->iTrailerCount; ++i ) {
			if ( !__xhttpdHeaderSetCopy(&pDst->pTrailers[i], pSrcTrailers[i].sName, pSrcTrailers[i].sValue) ) {
				pDst->iTrailerCount = i;
				xrtHttpdResponseUnit(pDst);
				return false;
			}
			pDst->iTrailerCount = i + 1u;
		}
	}
	if ( pSrc->pBody && pSrc->iBodyLen > 0u ) {
		if ( !xrtHttpdResponseSetBodyCopy(pDst, pSrc->pBody, pSrc->iBodyLen, NULL) ) {
			xrtHttpdResponseUnit(pDst);
			return false;
		}
	}
	return true;
}


// 内部函数：__xhttpdRequestWantsKeepAlive
static bool __xhttpdRequestWantsKeepAlive(const xhttpdrequest* pReq)
{
	return pReq && (pReq->iFlags & XHTTPD_REQ_F_KEEPALIVE) != 0u;
}


// 内部函数：__xhttpdPrepareResponse
// 内部函数：判断服务是否正在 drain
static bool __xhttpdServerIsDraining(const xhttpdserver* pServer)
{
	return pServer && __xhttpdAtomicLoadConst(&pServer->bDraining) != 0;
}


static bool __xhttpdPrepareResponse(const xhttpdrequest* pReq, const xhttpdresponse* pSrc, xhttpdresponse* pDst, bool* pKeepAlive)
{
	const char* sConn;
	bool bCloseToken;
	bool bKeepAliveHeader;
	bool bKeepAlive;

	// 初始化输出并深拷贝响应
	if ( pKeepAlive ) { *pKeepAlive = false; }
	if ( !pReq || !pSrc || !pDst ) { return false; }
	if ( !__xhttpdResponseCopy(pDst, pSrc) ) { return false; }
	if ( __xhttpdStatusForbidsFraming(pDst->iStatusCode) ) {
		__xhttpdResponseRemoveHeader(pDst, "Content-Length");
		__xhttpdResponseRemoveHeader(pDst, "Transfer-Encoding");
	}

	// 分析响应中的 Connection 头部
	sConn = xrtHttpdResponseHeader(pDst, "Connection");
	bCloseToken = __xhttpdContainsTokenNoCase(sConn, "close");
	bKeepAliveHeader = __xhttpdContainsTokenNoCase(sConn, "keep-alive");
	// 显式 keep-alive 且无 close 标记时清除关闭标志
	if ( bKeepAliveHeader && !bCloseToken ) {
		pDst->iFlags &= ~XHTTPD_RESP_F_CLOSE;
	}

	// 综合判断连接是否可保持存活
	bKeepAlive = __xhttpdRequestWantsKeepAlive(pReq) &&
		((pDst->iFlags & XHTTPD_RESP_F_CLOSE) == 0u) &&
		!bCloseToken;
	// 请求要求 keep-alive 但响应未显式设置 Connection 头部，自动补充
	if ( __xhttpdRequestWantsKeepAlive(pReq) && !bCloseToken && !sConn ) {
		pDst->iFlags &= ~XHTTPD_RESP_F_CLOSE;
		bKeepAlive = true;
		if ( !xrtHttpdResponseSetHeader(pDst, "Connection", "keep-alive") ) {
			xrtHttpdResponseUnit(pDst);
			return false;
		}
	}

	// 不可 keep-alive 时强制设置关闭标志
	if ( !bKeepAlive ) { pDst->iFlags |= XHTTPD_RESP_F_CLOSE; }
	if ( pKeepAlive ) { *pKeepAlive = bKeepAlive; }
	return true;
}


// 内部函数：__xhttpdBuildRequestEx
static bool __xhttpdBuildRequestEx(const xcodecframe* pFrame, const xcodechttp1msg* pMsg, const xnetchain* pChain, xhttpdrequest* pReq, size_t iBodyLimit, bool bCopyBody)
{
	size_t iBodyBytes;
	xrturlview tTarget;
	const xcodechttp1header* pMsgHeaders;
	const xcodechttp1header* pMsgTrailers;
	const char* sPath;
	size_t iPathLen;
	const char* sQuery = "";
	size_t iQueryLen = 0u;
	size_t iStorageBytes;
	size_t iStorageOff = 0u;
	// 参数校验并初始化请求结构体
	if ( !pFrame || !pMsg || !pChain || !pReq ) { return false; }
	// 映射编解码器标志到请求标志
	if ( pMsg->iFlags & XCODEC_HTTP1_F_KEEPALIVE ) { pReq->iFlags |= XHTTPD_REQ_F_KEEPALIVE; }
	if ( pMsg->iFlags & XCODEC_HTTP1_F_CHUNKED ) { pReq->iFlags |= XHTTPD_REQ_F_CHUNKED; }
	if ( pMsg->iFlags & XCODEC_HTTP1_F_UPGRADE ) { pReq->iFlags |= XHTTPD_REQ_F_UPGRADE; }
	// 复制基本字段
	pReq->iContentLength = pMsg->iContentLength;
	// 解析请求目标为路径和查询字符串
	if ( !xrtUrlParseTarget(pMsg->sTarget, &tTarget) ) { return false; }
	if ( (tTarget.iFlags & XRT_URL_F_HAS_FRAGMENT) != 0u ) { return false; }
	if ( tTarget.tPath.iLen == 0u ) {
		sPath = "/";
		iPathLen = 1u;
	} else {
		sPath = tTarget.tPath.sPtr;
		iPathLen = tTarget.tPath.iLen;
	}
	if ( (tTarget.iFlags & XRT_URL_F_HAS_QUERY) != 0u ) {
		sQuery = tTarget.tQuery.sPtr;
		iQueryLen = tTarget.tQuery.iLen;
	}
	// 复制请求头部
	if ( !__xhttpdRequestEnsureHeaderCap(pReq, pMsg->iHeaderCount) ) { goto fail; }
	if ( !__xhttpdRequestEnsureTrailerCap(pReq, pMsg->iTrailerCount) ) { goto fail; }
	pMsgHeaders = pMsg->pHeaders ? pMsg->pHeaders : pMsg->arrHeaders;
	pMsgTrailers = pMsg->pTrailers ? pMsg->pTrailers : pMsg->arrTrailers;
	iStorageBytes = 0u;
	if ( !__xhttpdSizeAdd(&iStorageBytes, pMsg->iMethodLen) ||
		!__xhttpdSizeAdd(&iStorageBytes, pMsg->iTargetLen) ||
		!__xhttpdSizeAdd(&iStorageBytes, iPathLen) ||
		!__xhttpdSizeAdd(&iStorageBytes, iQueryLen) ||
		!__xhttpdSizeAdd(&iStorageBytes, pMsg->iVersionLen) ||
		!__xhttpdSizeAdd(&iStorageBytes, 5u) ) { goto fail; }
	for ( uint32 i = 0u; i < pMsg->iHeaderCount; ++i ) {
		if ( !__xhttpdSizeAdd(&iStorageBytes, pMsgHeaders[i].iNameLen) ||
			!__xhttpdSizeAdd(&iStorageBytes, pMsgHeaders[i].iValueLen) ||
			!__xhttpdSizeAdd(&iStorageBytes, 2u) ) { goto fail; }
	}
	for ( uint32 i = 0u; i < pMsg->iTrailerCount; ++i ) {
		if ( !__xhttpdSizeAdd(&iStorageBytes, pMsgTrailers[i].iNameLen) ||
			!__xhttpdSizeAdd(&iStorageBytes, pMsgTrailers[i].iValueLen) ||
			!__xhttpdSizeAdd(&iStorageBytes, 2u) ) { goto fail; }
	}
	pReq->pStorage = (char*)XNET_ALLOC(iStorageBytes);
	if ( !pReq->pStorage ) { goto fail; }
#define __XHTTPD_COPY_REQUEST_META(dst_, src_, len_) do { \
	(dst_) = pReq->pStorage + iStorageOff; \
	if ( (len_) > 0u ) { memcpy(pReq->pStorage + iStorageOff, (src_), (len_)); } \
	pReq->pStorage[iStorageOff + (len_)] = '\0'; \
	iStorageOff += (len_) + 1u; \
} while ( 0 )
	__XHTTPD_COPY_REQUEST_META(pReq->sMethod, pMsg->sMethod, pMsg->iMethodLen);
	__XHTTPD_COPY_REQUEST_META(pReq->sTarget, pMsg->sTarget, pMsg->iTargetLen);
	__XHTTPD_COPY_REQUEST_META(pReq->sPath, sPath, iPathLen);
	__XHTTPD_COPY_REQUEST_META(pReq->sQuery, sQuery, iQueryLen);
	__XHTTPD_COPY_REQUEST_META(pReq->sVersion, pMsg->sVersion, pMsg->iVersionLen);
	pReq->iMethod = __xhttpdParseMethodID(pReq->sMethod);
	for ( uint32 i = 0u; i < pMsg->iHeaderCount; ++i ) {
		__XHTTPD_COPY_REQUEST_META(pReq->pHeaders[i].sName, pMsgHeaders[i].sName, pMsgHeaders[i].iNameLen);
		__XHTTPD_COPY_REQUEST_META(pReq->pHeaders[i].sValue, pMsgHeaders[i].sValue, pMsgHeaders[i].iValueLen);
		pReq->iHeaderCount++;
	}
	for ( uint32 i = 0u; i < pMsg->iTrailerCount; ++i ) {
		__XHTTPD_COPY_REQUEST_META(pReq->pTrailers[i].sName, pMsgTrailers[i].sName, pMsgTrailers[i].iNameLen);
		__XHTTPD_COPY_REQUEST_META(pReq->pTrailers[i].sValue, pMsgTrailers[i].sValue, pMsgTrailers[i].iValueLen);
		pReq->iTrailerCount++;
	}
#undef __XHTTPD_COPY_REQUEST_META
	if ( !bCopyBody ) { return true; }
	// 提取并复制请求正文
	iBodyBytes = xrtCodecHttp1BodyBytes(pFrame);
	if ( iBodyLimit > 0u && iBodyBytes > iBodyLimit ) { goto fail; }
	if ( (pMsg->iFlags & XCODEC_HTTP1_F_CHUNKED) != 0u ) { pReq->iContentLength = (int64_t)iBodyBytes; }
	if ( iBodyBytes > 0u ) {
		if ( !xrtHttpdRequestReserveBody(pReq, iBodyBytes) ) {
			xrtHttpdRequestUnit(pReq);
			return false;
		}
		pReq->iBodyLen = xrtCodecHttp1CopyBody(pChain, pFrame, pReq->pBody, iBodyBytes);
		pReq->pBody[pReq->iBodyLen] = '\0';
	}
	return true;

fail:
	xrtHttpdRequestUnit(pReq);
	return false;
}


static void __xhttpdResolveCodecLimits(const xhttpdserver* pServer, xcodechttp1limits* pLimits)
{
	if ( !pLimits ) { return; }
	xrtCodecHttp1LimitsInit(pLimits);
	if ( !pServer ) { return; }
	if ( pServer->tConfig.iStartLineBytesLimit > 0u ) { pLimits->iMaxStartLineBytes = pServer->tConfig.iStartLineBytesLimit; }
	if ( pServer->tConfig.iHeaderBytesLimit > 0u ) { pLimits->iMaxHeaderBytes = pServer->tConfig.iHeaderBytesLimit; }
	if ( pServer->tConfig.iHeaderLineBytesLimit > 0u ) { pLimits->iMaxHeaderLineBytes = pServer->tConfig.iHeaderLineBytesLimit; }
	if ( pServer->tConfig.iHeaderLimit > 0u ) { pLimits->iMaxHeaderCount = pServer->tConfig.iHeaderLimit; }
	if ( pServer->tConfig.iChunkLineBytesLimit > 0u ) { pLimits->iMaxChunkLineBytes = pServer->tConfig.iChunkLineBytesLimit; }
	if ( pServer->tConfig.iTrailerBytesLimit > 0u ) { pLimits->iMaxTrailerBytes = pServer->tConfig.iTrailerBytesLimit; }
	if ( pServer->tConfig.iTrailerLimit > 0u ) { pLimits->iMaxTrailerCount = pServer->tConfig.iTrailerLimit; }
	pLimits->iMaxBodyBytes = pServer->tConfig.iBodyLimit > 0u ? pServer->tConfig.iBodyLimit : UINT64_MAX;
}


static uint32 __xhttpdProtocolStatus(const xcodechttp1errorinfo* pError)
{
	if ( !pError ) { return 400u; }
	switch ( pError->eCode ) {
		case XCODEC_HTTP1_ERROR_HEADER_TOO_LARGE:
		case XCODEC_HTTP1_ERROR_START_LINE_TOO_LARGE:
		case XCODEC_HTTP1_ERROR_HEADER_LINE_TOO_LARGE:
		case XCODEC_HTTP1_ERROR_TOO_MANY_HEADERS:
		case XCODEC_HTTP1_ERROR_TRAILER_TOO_LARGE:
		case XCODEC_HTTP1_ERROR_TOO_MANY_TRAILERS:
			return 431u;
		case XCODEC_HTTP1_ERROR_BODY_TOO_LARGE:
			return 413u;
		default:
			return 400u;
	}
}


static bool __xhttpdValidateRequestContract(const xcodechttp1msg* pMsg)
{
	uint32 iHostCount = 0u;
	const char* sHost = NULL;
	xrturlview tAuthority;
	if ( !pMsg || (pMsg->iFlags & XCODEC_HTTP1_F_REQUEST) == 0u ) { return false; }
	for ( uint32 i = 0u; i < pMsg->iHeaderCount; ++i ) {
		if ( __xhttpdStrEqNoCase(pMsg->pHeaders[i].sName, "Host") ) {
			iHostCount++;
			sHost = pMsg->pHeaders[i].sValue;
		}
	}
	if ( iHostCount > 0u && (!sHost || !sHost[0] || strchr(sHost, ',') != NULL ||
		!xrtUrlParseAuthority(sHost, &tAuthority) ||
		(tAuthority.iFlags & XRT_URL_F_HAS_USERINFO) != 0u) ) { return false; }
	if ( __xhttpdStrEqNoCase(pMsg->sVersion, "HTTP/1.1") ) {
		return iHostCount == 1u;
	}
	return __xhttpdStrEqNoCase(pMsg->sVersion, "HTTP/1.0") && iHostCount <= 1u;
}


// 内部函数：__xhttpdBuildRequestHead
static bool __xhttpdBuildRequestHead(const xcodecframe* pFrame, const xcodechttp1msg* pMsg, const xnetchain* pChain, xhttpdrequest* pReq, size_t iBodyLimit)
{
	return __xhttpdBuildRequestEx(pFrame, pMsg, pChain, pReq, iBodyLimit, false);
}


// 内部函数：__xhttpdBuildRequest
static bool __xhttpdBuildRequest(const xcodecframe* pFrame, const xcodechttp1msg* pMsg, const xnetchain* pChain, xhttpdrequest* pReq, size_t iBodyLimit)
{
	return __xhttpdBuildRequestEx(pFrame, pMsg, pChain, pReq, iBodyLimit, true);
}


// 内部函数：格式化 HTTP Date 头
static bool __xhttpdFormatDate(char* sOut, size_t iOutCap)
{
	static const char* arrWeek[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
	static const char* arrMonth[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	time_t tNow;
	struct tm tGmt;

	if ( !sOut || iOutCap == 0u ) { return false; }
	tNow = time(NULL);
	#if defined(__TINYC__)
		{
			struct tm* pGmt = gmtime(&tNow);
			if ( !pGmt ) { return false; }
			tGmt = *pGmt;
		}
	#elif defined(_WIN32) || defined(_WIN64)
		if ( gmtime_s(&tGmt, &tNow) != 0 ) { return false; }
	#else
		if ( gmtime_r(&tNow, &tGmt) == NULL ) { return false; }
	#endif
	if ( tGmt.tm_wday < 0 || tGmt.tm_wday > 6 || tGmt.tm_mon < 0 || tGmt.tm_mon > 11 ) { return false; }
	return snprintf(sOut, iOutCap, "Date: %s, %02d %s %04d %02d:%02d:%02d GMT\r\n",
		arrWeek[tGmt.tm_wday],
		tGmt.tm_mday,
		arrMonth[tGmt.tm_mon],
		tGmt.tm_year + 1900,
		tGmt.tm_hour,
		tGmt.tm_min,
		tGmt.tm_sec) > 0;
}


static bool __xhttpdResponseTrailersValid(const xhttpdresponse* pResp)
{
	const xhttpdheader* pTrailers;
	const char* sTransferEncoding;
	if ( !pResp || pResp->iTrailerCount == 0u ) { return pResp != NULL; }
	if ( __xhttpdStatusForbidsBody(pResp->iStatusCode) ||
		__xhttpdResponseHasHeader(pResp, "Content-Length") ) {
		return false;
	}
	sTransferEncoding = xrtHttpdResponseHeader(pResp, "Transfer-Encoding");
	if ( sTransferEncoding && !__xhttpdContainsTokenNoCase(sTransferEncoding, "chunked") ) {
		return false;
	}
	pTrailers = pResp->pTrailers ? pResp->pTrailers : pResp->arrTrailers;
	for ( uint32 i = 0u; i < pResp->iTrailerCount; ++i ) {
		if ( !__xhttpdValidHeaderName(pTrailers[i].sName) ||
			!__xhttpdValidHeaderValue(pTrailers[i].sValue) ||
			__xhttpdForbiddenTrailer(pTrailers[i].sName) ) {
			return false;
		}
	}
	return true;
}


static bool __xhttpdAppendTrailerDeclaration(char** ppBuf, size_t* pLen, size_t* pCap,
	const xhttpdresponse* pResp)
{
	const xhttpdheader* pTrailers;
	uint32 iEmitted = 0u;
	if ( !pResp || pResp->iTrailerCount == 0u ) { return true; }
	pTrailers = pResp->pTrailers ? pResp->pTrailers : pResp->arrTrailers;
	if ( !__xhttpdAppendText(ppBuf, pLen, pCap, "Trailer: ") ) { return false; }
	for ( uint32 i = 0u; i < pResp->iTrailerCount; ++i ) {
		bool bDuplicate = false;
		for ( uint32 j = 0u; j < i; ++j ) {
			if ( __xhttpdStrEqNoCase(pTrailers[j].sName, pTrailers[i].sName) ) {
				bDuplicate = true;
				break;
			}
		}
		if ( bDuplicate ) { continue; }
		if ( iEmitted > 0u && !__xhttpdAppendText(ppBuf, pLen, pCap, ", ") ) { return false; }
		if ( !__xhttpdAppendText(ppBuf, pLen, pCap, pTrailers[i].sName) ) { return false; }
		iEmitted++;
	}
	return __xhttpdAppendText(ppBuf, pLen, pCap, "\r\n");
}


static bool __xhttpdAppendChunkEnd(char** ppBuf, size_t* pLen, size_t* pCap,
	const xhttpdresponse* pResp)
{
	const xhttpdheader* pTrailers;
	if ( !pResp || !__xhttpdAppendText(ppBuf, pLen, pCap, "0\r\n") ) { return false; }
	pTrailers = pResp->pTrailers ? pResp->pTrailers : pResp->arrTrailers;
	for ( uint32 i = 0u; i < pResp->iTrailerCount; ++i ) {
		if ( !__xhttpdAppendText(ppBuf, pLen, pCap, pTrailers[i].sName) ||
			!__xhttpdAppendText(ppBuf, pLen, pCap, ": ") ||
			!__xhttpdAppendText(ppBuf, pLen, pCap, pTrailers[i].sValue) ||
			!__xhttpdAppendText(ppBuf, pLen, pCap, "\r\n") ) {
			return false;
		}
	}
	return __xhttpdAppendText(ppBuf, pLen, pCap, "\r\n");
}


static bool __xhttpdBuildChunkEnd(const xhttpdresponse* pResp, char** ppOut, size_t* pOutLen)
{
	char* pBuf = NULL;
	size_t iLen = 0u;
	size_t iCap = 0u;
	if ( !pResp || !ppOut || !pOutLen || !__xhttpdResponseTrailersValid(pResp) ||
		!__xhttpdAppendChunkEnd(&pBuf, &iLen, &iCap, pResp) ) {
		if ( pBuf ) { XNET_FREE(pBuf); }
		return false;
	}
	*ppOut = pBuf;
	*pOutLen = iLen;
	return true;
}


// 内部函数：__xhttpdBuildResponseBytes
static bool __xhttpdBuildResponseBytes(const xhttpdresponse* pResp, char** ppOut, size_t* pOutLen)
{
	char* pBuf = NULL;
	size_t iLen = 0;
	size_t iCap = 0;
	char aLine[128];
	const char* sReason;
	const xhttpdheader* pHeaders;
	bool bChunked;
	bool bNoBody;
	bool bForbidFraming;
	bool bHasTrailers;
	// 参数校验
	if ( !pResp || !ppOut || !pOutLen ) { return false; }
	// 确定原因短语和传输编码模式
	if ( pResp->iStatusCode < 100u || pResp->iStatusCode > 999u ) { goto fail; }
	sReason = pResp->sReason && pResp->sReason[0] ? pResp->sReason : __xhttpdStatusText(pResp->iStatusCode);
	if ( !__xhttpdValidReasonPhrase(sReason) ) { goto fail; }
	bNoBody = __xhttpdStatusForbidsBody(pResp->iStatusCode);
	bForbidFraming = __xhttpdStatusForbidsFraming(pResp->iStatusCode);
	bHasTrailers = pResp->iTrailerCount > 0u;
	if ( !__xhttpdResponseTrailersValid(pResp) ) { goto fail; }
	bChunked = !bForbidFraming && (bHasTrailers ||
		__xhttpdContainsTokenNoCase(xrtHttpdResponseHeader(pResp, "Transfer-Encoding"), "chunked"));
	pHeaders = pResp->pHeaders ? pResp->pHeaders : pResp->arrHeaders;
	// 构建状态行
	snprintf(aLine, sizeof(aLine), "HTTP/1.1 %u ", (unsigned)pResp->iStatusCode);
	if ( !__xhttpdAppendText(&pBuf, &iLen, &iCap, aLine) ||
		!__xhttpdAppendText(&pBuf, &iLen, &iCap, sReason) ||
		!__xhttpdAppendText(&pBuf, &iLen, &iCap, "\r\n") ) { goto fail; }
	if ( !__xhttpdResponseHasHeader(pResp, "Date") ) {
		if ( __xhttpdFormatDate(aLine, sizeof(aLine)) ) {
			if ( !__xhttpdAppendText(&pBuf, &iLen, &iCap, aLine) ) { goto fail; }
		}
	}
	// 补充 Connection 头部
	if ( !__xhttpdResponseHasHeader(pResp, "Connection") ) {
		if ( !__xhttpdAppendText(&pBuf, &iLen, &iCap, "Connection: close\r\n") ) { goto fail; }
	}
	// 补充 Content-Length 头部
	if ( !bChunked && !bForbidFraming && !__xhttpdResponseHasHeader(pResp, "Content-Length") ) {
		snprintf(aLine, sizeof(aLine), "Content-Length: %llu\r\n", (unsigned long long)pResp->iBodyLen);
		if ( !__xhttpdAppendText(&pBuf, &iLen, &iCap, aLine) ) { goto fail; }
	}
	if ( bChunked && !__xhttpdResponseHasHeader(pResp, "Transfer-Encoding") &&
		!__xhttpdAppendText(&pBuf, &iLen, &iCap, "Transfer-Encoding: chunked\r\n") ) { goto fail; }
	if ( bHasTrailers && !__xhttpdAppendTrailerDeclaration(&pBuf, &iLen, &iCap, pResp) ) { goto fail; }
	// 追加用户自定义头部
	for ( uint32 i = 0; i < pResp->iHeaderCount; ++i ) {
		if ( bForbidFraming && (__xhttpdStrEqNoCase(pHeaders[i].sName, "Content-Length") ||
			__xhttpdStrEqNoCase(pHeaders[i].sName, "Transfer-Encoding")) ) { continue; }
		if ( bChunked && __xhttpdStrEqNoCase(pHeaders[i].sName, "Content-Length") ) { continue; }
		if ( bHasTrailers && __xhttpdStrEqNoCase(pHeaders[i].sName, "Trailer") ) { continue; }
		if ( !__xhttpdValidHeaderName(pHeaders[i].sName)
			|| !__xhttpdValidHeaderValue(pHeaders[i].sValue) ) {
			goto fail;
		}
		if ( !__xhttpdAppendText(&pBuf, &iLen, &iCap, pHeaders[i].sName) ||
			!__xhttpdAppendText(&pBuf, &iLen, &iCap, ": ") ||
			!__xhttpdAppendText(&pBuf, &iLen, &iCap, pHeaders[i].sValue) ||
			!__xhttpdAppendText(&pBuf, &iLen, &iCap, "\r\n") ) { goto fail; }
	}
	// 空行分隔头部和正文
	if ( !__xhttpdAppendText(&pBuf, &iLen, &iCap, "\r\n") ) { goto fail; }
	// 处理响应正文
	if ( !bNoBody && bChunked ) {
		if ( pResp->pBody && pResp->iBodyLen > 0 ) {
			snprintf(aLine, sizeof(aLine), "%llX\r\n", (unsigned long long)pResp->iBodyLen);
			if ( !__xhttpdAppendText(&pBuf, &iLen, &iCap, aLine) ) { goto fail; }
			if ( !__xhttpdAppendBytes(&pBuf, &iLen, &iCap, pResp->pBody, pResp->iBodyLen) ) { goto fail; }
			if ( !__xhttpdAppendText(&pBuf, &iLen, &iCap, "\r\n") ) { goto fail; }
		}
		if ( !__xhttpdAppendChunkEnd(&pBuf, &iLen, &iCap, pResp) ) { goto fail; }
	} else if ( !bNoBody && pResp->pBody && pResp->iBodyLen > 0 ) {
		if ( !__xhttpdAppendBytes(&pBuf, &iLen, &iCap, pResp->pBody, pResp->iBodyLen) ) { goto fail; }
	}
	*ppOut = pBuf;
	*pOutLen = iLen;
	return true;

fail:
	if ( pBuf ) { XNET_FREE(pBuf); }
	return false;
}


// 内部函数：__xhttpdBuildResponseHeadBytes
static bool __xhttpdBuildResponseHeadBytes(const xhttpdresponse* pResp, char** ppOut, size_t* pOutLen)
{
	char* pBuf = NULL;
	size_t iLen = 0;
	size_t iCap = 0;
	char aLine[128];
	const char* sReason;
	const xhttpdheader* pHeaders;
	bool bChunked;
	bool bForbidFraming;
	bool bHasTrailers;
	if ( !pResp || !ppOut || !pOutLen ) { return false; }
	if ( pResp->iStatusCode < 100u || pResp->iStatusCode > 999u ) { goto fail; }
	sReason = pResp->sReason && pResp->sReason[0] ? pResp->sReason : __xhttpdStatusText(pResp->iStatusCode);
	if ( !__xhttpdValidReasonPhrase(sReason) ) { goto fail; }
	bForbidFraming = __xhttpdStatusForbidsFraming(pResp->iStatusCode);
	bHasTrailers = pResp->iTrailerCount > 0u;
	if ( !__xhttpdResponseTrailersValid(pResp) ) { goto fail; }
	bChunked = !bForbidFraming && (bHasTrailers ||
		__xhttpdContainsTokenNoCase(xrtHttpdResponseHeader(pResp, "Transfer-Encoding"), "chunked"));
	pHeaders = pResp->pHeaders ? pResp->pHeaders : pResp->arrHeaders;
	snprintf(aLine, sizeof(aLine), "HTTP/1.1 %u ", (unsigned)pResp->iStatusCode);
	if ( !__xhttpdAppendText(&pBuf, &iLen, &iCap, aLine) ||
		!__xhttpdAppendText(&pBuf, &iLen, &iCap, sReason) ||
		!__xhttpdAppendText(&pBuf, &iLen, &iCap, "\r\n") ) { goto fail; }
	if ( !__xhttpdResponseHasHeader(pResp, "Date") ) {
		if ( __xhttpdFormatDate(aLine, sizeof(aLine)) ) {
			if ( !__xhttpdAppendText(&pBuf, &iLen, &iCap, aLine) ) { goto fail; }
		}
	}
	if ( !__xhttpdResponseHasHeader(pResp, "Connection") ) {
		if ( !__xhttpdAppendText(&pBuf, &iLen, &iCap, "Connection: close\r\n") ) { goto fail; }
	}
	if ( !bChunked && !bForbidFraming && !__xhttpdResponseHasHeader(pResp, "Content-Length") ) {
		snprintf(aLine, sizeof(aLine), "Content-Length: %llu\r\n", (unsigned long long)pResp->iBodyLen);
		if ( !__xhttpdAppendText(&pBuf, &iLen, &iCap, aLine) ) { goto fail; }
	}
	if ( bChunked && !__xhttpdResponseHasHeader(pResp, "Transfer-Encoding") &&
		!__xhttpdAppendText(&pBuf, &iLen, &iCap, "Transfer-Encoding: chunked\r\n") ) { goto fail; }
	if ( bHasTrailers && !__xhttpdAppendTrailerDeclaration(&pBuf, &iLen, &iCap, pResp) ) { goto fail; }
	for ( uint32 i = 0; i < pResp->iHeaderCount; ++i ) {
		if ( bForbidFraming && (__xhttpdStrEqNoCase(pHeaders[i].sName, "Content-Length") ||
			__xhttpdStrEqNoCase(pHeaders[i].sName, "Transfer-Encoding")) ) { continue; }
		if ( bChunked && __xhttpdStrEqNoCase(pHeaders[i].sName, "Content-Length") ) { continue; }
		if ( bHasTrailers && __xhttpdStrEqNoCase(pHeaders[i].sName, "Trailer") ) { continue; }
		if ( !__xhttpdValidHeaderName(pHeaders[i].sName) ||
			!__xhttpdValidHeaderValue(pHeaders[i].sValue) ) {
			goto fail;
		}
		if ( !__xhttpdAppendText(&pBuf, &iLen, &iCap, pHeaders[i].sName) ||
			!__xhttpdAppendText(&pBuf, &iLen, &iCap, ": ") ||
			!__xhttpdAppendText(&pBuf, &iLen, &iCap, pHeaders[i].sValue) ||
			!__xhttpdAppendText(&pBuf, &iLen, &iCap, "\r\n") ) { goto fail; }
	}
	if ( !__xhttpdAppendText(&pBuf, &iLen, &iCap, "\r\n") ) { goto fail; }
	*ppOut = pBuf;
	*pOutLen = iLen;
	return true;
fail:
	if ( pBuf ) { XNET_FREE(pBuf); }
	return false;
}


// 内部函数：__xhttpdConnDetachRequestLocked
static xhttpdrequest* __xhttpdConnDetachRequestLocked(xhttpdconn* pConn)
{
	xhttpdrequest* pReq;
	if ( !pConn ) { return NULL; }
	pReq = pConn->pRequest;
	pConn->pRequest = NULL;
	pConn->bResponseInFlight = false;
	pConn->bResponseCommitted = false;
	pConn->bResponseDrained = false;
	pConn->bResponseStreaming = false;
	pConn->bResponseChunked = false;
	__xhttpdConnClearResponseEndLocked(pConn);
	pConn->bRequestStreaming = false;
	pConn->bAsyncPending = false;
	pConn->bKeepAlive = false;
	pConn->bContinueSent = false;
	pConn->bUpgraded = false;
	pConn->iBodyMode = XHTTPD_BODY_MODE_NONE;
	pConn->iChunkState = XHTTPD_CHUNK_STATE_SIZE;
	pConn->iBodyRemain = 0u;
	pConn->iBodyReceived = 0u;
	pConn->iChunkRemain = 0u;
	return pReq;
}


// 内部函数：__xhttpdRecvKickTask
static void __xhttpdRecvKickTask(xnetworker* pWorker, ptr pArg)
{
	xhttpdconn* pConn = (xhttpdconn*)pArg;
	xnetstream* pStream = NULL;
	(void)pWorker;
	if ( !pConn ) { return; }
	__xhttpdLock(&pConn->iConnLock);
	if ( pConn->pStream && __xhttpdAtomicLoad(&pConn->iCleanupPosted) == 0 ) {
		pStream = pConn->pStream;
		__xnetStreamAddAsyncHold(pStream);
	}
	__xhttpdUnlock(&pConn->iConnLock);
	if ( pStream && xrtNetChainBytes(&pStream->tRxChain) > 0u ) {
		__xhttpdStreamOnRecv(pConn, pStream, &pStream->tRxChain);
	}
	if ( pStream ) { __xnetStreamReleaseAsyncHold(pStream); }
	__xhttpdConnRelease(pConn);
}


// 内部函数：__xhttpdConnPostRecvKick
static void __xhttpdConnPostRecvKick(xhttpdconn* pConn, xnetstream* pStream)
{
	if ( !pConn || !pStream || !pStream->pEngine || !pStream->pWorker ) { return; }
	if ( xrtNetEnginePost(pStream->pEngine, pStream->pWorker->iId, __xhttpdRecvKickTask, __xhttpdConnAddRef(pConn)) != XRT_NET_OK ) {
		__xhttpdConnRelease(pConn);
	}
}


// 内部函数：__xhttpdConnTryFinalizeRequest
static void __xhttpdConnTryFinalizeRequest(xhttpdconn* pConn)
{
	xhttpdrequest* pReq = NULL;
	xnetstream* pStream = NULL;
	xhttpdserver* pServer = NULL;
	bool bKickRecv = false;
	bool bCloseStream = false;
	bool bScheduleIdle = false;

	if ( !pConn ) { return; }

	__xhttpdLock(&pConn->iConnLock);
	pServer = pConn->pServer;
	if ( pConn->pRequest && !pConn->bAsyncPending ) {
		if ( pConn->pStream == NULL ) {
			pReq = __xhttpdConnDetachRequestLocked(pConn);
		}
		else if ( pConn->bKeepAlive && pConn->bResponseCommitted && pConn->bResponseDrained && !pConn->pStream->bClosing ) {
			pReq = __xhttpdConnDetachRequestLocked(pConn);
			pStream = pConn->pStream;
			__xnetStreamAddAsyncHold(pStream);
			if ( __xhttpdServerIsDraining(pServer) ) {
				bCloseStream = true;
			}
			else {
				bKickRecv = xrtNetChainBytes(&pStream->tRxChain) > 0u;
				bScheduleIdle = !bKickRecv;
			}
		}
	}
	__xhttpdUnlock(&pConn->iConnLock);

	__xhttpdRequestDestroy(pReq);
	if ( bCloseStream && pStream ) { xrtNetStreamClose(pStream, XNET_CLOSE_F_GRACEFUL); }
	else if ( bKickRecv ) { __xhttpdConnPostRecvKick(pConn, pStream); }
	else if ( bScheduleIdle ) { __xhttpdConnArmTimer(pConn, XHTTPD_CONN_TIMER_IDLE, true); }
	if ( pStream ) { __xnetStreamReleaseAsyncHold(pStream); }
}


// 内部函数：__xhttpdFinalizeTask
static void __xhttpdFinalizeTask(xnetworker* pWorker, ptr pArg)
{
	xhttpdconn* pConn = (xhttpdconn*)pArg;
	(void)pWorker;
	__xhttpdConnTryFinalizeRequest(pConn);
	__xhttpdConnRelease(pConn);
}


// 内部函数：__xhttpdConnFinalizeMaybeAsync
static void __xhttpdConnFinalizeMaybeAsync(xhttpdconn* pConn)
{
	xnetstream* pStream = NULL;
	if ( !pConn ) { return; }
	__xhttpdLock(&pConn->iConnLock);
	pStream = pConn->pStream;
	if ( pStream ) { __xnetStreamAddAsyncHold(pStream); }
	__xhttpdUnlock(&pConn->iConnLock);
	if ( pStream && pStream->pEngine && pStream->pWorker && !__xnetEngineIsCurrentWorker(pStream->pWorker) ) {
		if ( xrtNetEnginePost(pStream->pEngine, pStream->pWorker->iId, __xhttpdFinalizeTask, __xhttpdConnAddRef(pConn)) == XRT_NET_OK ) {
			__xnetStreamReleaseAsyncHold(pStream);
			return;
		}
		__xhttpdConnRelease(pConn);
	}
	if ( pStream ) { __xnetStreamReleaseAsyncHold(pStream); }
	__xhttpdConnTryFinalizeRequest(pConn);
}


// xrtHttpdConnIsOpen 相关处理
XXAPI bool xrtHttpdConnIsOpen(const xhttpdconn* pConn)
{
	bool bOpen = false;
	xhttpdconn* pMutable = (xhttpdconn*)pConn;
	if ( !pMutable ) { return false; }
	__xhttpdLock(&pMutable->iConnLock);
	bOpen = pMutable->pStream != NULL &&
		__xhttpdAtomicLoadConst(&pMutable->iCleanupPosted) == 0 &&
		!pMutable->pStream->bClosing;
	__xhttpdUnlock(&pMutable->iConnLock);
	return bOpen;
}


// 获取当前连接 TLS SNI，非 TLS 或无 SNI 返回 NULL
XXAPI const char* xrtHttpdConnTlsSNI(const xhttpdconn* pConn)
{
	const char* sRet = NULL;
	xnetstream* pStream = NULL;
	xhttpdconn* pMutable = (xhttpdconn*)pConn;
	if ( !pMutable ) { return NULL; }
	__xhttpdLock(&pMutable->iConnLock);
	pStream = pMutable->pStream;
	if ( pStream ) { __xnetStreamAddAsyncHold(pStream); }
	__xhttpdUnlock(&pMutable->iConnLock);
	if ( pStream ) {
		sRet = xrtNetStreamTlsSNI(pStream);
		__xnetStreamReleaseAsyncHold(pStream);
	}
	return sRet;
}


// 复制当前连接 TLS SNI，返回值需要使用 xrtFree 释放
XXAPI str xrtHttpdConnTlsSNICopy(const xhttpdconn* pConn)
{
	str sRet = xCore.sNull;
	const char* sSNI = NULL;
	xnetstream* pStream = NULL;
	xhttpdconn* pMutable = (xhttpdconn*)pConn;
	if ( !pMutable ) { return xCore.sNull; }
	__xhttpdLock(&pMutable->iConnLock);
	pStream = pMutable->pStream;
	if ( pStream ) { __xnetStreamAddAsyncHold(pStream); }
	__xhttpdUnlock(&pMutable->iConnLock);
	if ( pStream ) {
		sSNI = xrtNetStreamTlsSNI(pStream);
		sRet = (sSNI && sSNI[0]) ? xrtCopyStr((str)sSNI, 0) : xCore.sNull;
		__xnetStreamReleaseAsyncHold(pStream);
	}
	return sRet;
}


// 复制连接本地地址文本，返回值需要使用 xrtFree 释放
XXAPI str xrtHttpdConnLocalAddrText(const xhttpdconn* pConn)
{
	str sRet = xCore.sNull;
	xnetstream* pStream = NULL;
	xhttpdconn* pMutable = (xhttpdconn*)pConn;
	if ( !pMutable ) { return xCore.sNull; }
	__xhttpdLock(&pMutable->iConnLock);
	pStream = pMutable->pStream;
	if ( pStream ) { __xnetStreamAddAsyncHold(pStream); }
	__xhttpdUnlock(&pMutable->iConnLock);
	if ( pStream ) {
		sRet = xrtNetStreamLocalAddrText(pStream);
		__xnetStreamReleaseAsyncHold(pStream);
	}
	return sRet;
}


// 复制连接远端地址文本，返回值需要使用 xrtFree 释放
XXAPI str xrtHttpdConnRemoteAddrText(const xhttpdconn* pConn)
{
	str sRet = xCore.sNull;
	xnetstream* pStream = NULL;
	xhttpdconn* pMutable = (xhttpdconn*)pConn;
	if ( !pMutable ) { return xCore.sNull; }
	__xhttpdLock(&pMutable->iConnLock);
	pStream = pMutable->pStream;
	if ( pStream ) { __xnetStreamAddAsyncHold(pStream); }
	__xhttpdUnlock(&pMutable->iConnLock);
	if ( pStream ) {
		sRet = xrtNetStreamRemoteAddrText(pStream);
		__xnetStreamReleaseAsyncHold(pStream);
	}
	return sRet;
}


// 获取连接本地端口
XXAPI int xrtHttpdConnLocalPort(const xhttpdconn* pConn)
{
	int iRet = 0;
	xnetstream* pStream = NULL;
	xhttpdconn* pMutable = (xhttpdconn*)pConn;
	if ( !pMutable ) { return 0; }
	__xhttpdLock(&pMutable->iConnLock);
	pStream = pMutable->pStream;
	if ( pStream ) { __xnetStreamAddAsyncHold(pStream); }
	__xhttpdUnlock(&pMutable->iConnLock);
	if ( pStream ) {
		iRet = xrtNetStreamLocalPort(pStream);
		__xnetStreamReleaseAsyncHold(pStream);
	}
	return iRet;
}


// 获取连接远端端口
XXAPI int xrtHttpdConnRemotePort(const xhttpdconn* pConn)
{
	int iRet = 0;
	xnetstream* pStream = NULL;
	xhttpdconn* pMutable = (xhttpdconn*)pConn;
	if ( !pMutable ) { return 0; }
	__xhttpdLock(&pMutable->iConnLock);
	pStream = pMutable->pStream;
	if ( pStream ) { __xnetStreamAddAsyncHold(pStream); }
	__xhttpdUnlock(&pMutable->iConnLock);
	if ( pStream ) {
		iRet = xrtNetStreamRemotePort(pStream);
		__xnetStreamReleaseAsyncHold(pStream);
	}
	return iRet;
}


// 内部函数：发送任务进程
static void __xhttpdSendTaskProc(xnetworker* pWorker, ptr pArg)
{
	__xhttpd_send_task* pTask = (__xhttpd_send_task*)pArg;
	xhttpdconn* pConn;
	xhttpdserver* pServer = NULL;
	xnetstream* pStream = NULL;
	xnet_result iRet = XRT_NET_CLOSED;
	bool bClose = false;
	bool bCanSend = false;
	(void)pWorker;
	if ( !pTask ) { return; }
	pConn = pTask->pConn;
	if ( pConn ) {
		// 获取连接锁，读取服务和流信息
		__xhttpdLock(&pConn->iConnLock);
		pServer = pConn->pServer;
		pStream = pConn->pStream;
		// 流有效且未关闭时发送数据
		if ( pStream && !pStream->bClosing && __xhttpdAtomicLoad(&pConn->iCleanupPosted) == 0 ) {
			__xnetStreamAddAsyncHold(pStream);
			bClose = pTask->bClose;
			bCanSend = true;
		}
		__xhttpdUnlock(&pConn->iConnLock);
		if ( bCanSend ) {
			iRet = xrtNetStreamSend(pStream, pTask->pBytes, pTask->iLen);
			if ( iRet != XRT_NET_OK ) {
				xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
			}
			else {
				__xhttpdConnArmWriteTimer(pConn, true);
				if ( bClose ) {
					xrtNetStreamClose(pStream, XNET_CLOSE_F_GRACEFUL);
				}
			}
			__xnetStreamReleaseAsyncHold(pStream);
		}
		// 发送失败时触发错误回调
		if ( iRet != XRT_NET_OK ) { __xhttpdEmitServerError(pServer, pConn, -1); }
		// 尝试完成当前请求并复用连接
		__xhttpdConnTryFinalizeRequest(pConn);
		__xhttpdConnRelease(pConn);
	}
	// 释放任务资源
	XNET_FREE(pTask->pBytes);
	XNET_FREE(pTask);
}


// xrtHttpdConnRespond 相关处理
XXAPI xnet_result xrtHttpdConnRespond(xhttpdconn* pConn, const xhttpdresponse* pResp)
{
	xhttpdresponse tSend;
	xnetstream* pStream;
	xhttpdrequest* pReq;
	char* pBytes = NULL;
	size_t iLen = 0u;
	__xhttpd_send_task* pTask = NULL;
	bool bKeepAlive = false;
	xnet_result iRet;
	bool bAbortAfterUnlock = false;

	// 参数校验
	if ( !pConn || !pResp ) { return XRT_NET_ERROR; }
	memset(&tSend, 0, sizeof(tSend));

	// 获取连接锁，验证连接状态
	__xhttpdLock(&pConn->iConnLock);
	pStream = pConn->pStream;
	pReq = pConn->pRequest;
	if ( !pStream || pStream->bClosing || !pReq || pConn->bResponseCommitted || __xhttpdAtomicLoad(&pConn->iCleanupPosted) != 0 ) {
		__xhttpdUnlock(&pConn->iConnLock);
		return XRT_NET_CLOSED;
	}
	// 准备响应（确定 keep-alive 策略）
	if ( pReq->iMethod == XHTTPD_METHOD_HEAD && pResp->iTrailerCount > 0u ) {
		__xhttpdUnlock(&pConn->iConnLock);
		return XRT_NET_ERROR;
	}
	if ( !__xhttpdPrepareResponse(pReq, pResp, &tSend, &bKeepAlive) ) {
		__xhttpdUnlock(&pConn->iConnLock);
		return XRT_NET_ERROR;
	}
	if ( __xhttpdServerIsDraining(pConn->pServer) ) {
		bKeepAlive = false;
		tSend.iFlags |= XHTTPD_RESP_F_CLOSE;
		if ( !xrtHttpdResponseSetHeader(&tSend, "Connection", "close") ) {
			__xhttpdUnlock(&pConn->iConnLock);
			xrtHttpdResponseUnit(&tSend);
			return XRT_NET_ERROR;
		}
	}
	// HEAD 响应只发送头部，保留 Content-Length 语义但不发送 body。
	if ( pReq->iMethod == XHTTPD_METHOD_HEAD ) {
		if ( !__xhttpdBuildResponseHeadBytes(&tSend, &pBytes, &iLen) ) {
			__xhttpdUnlock(&pConn->iConnLock);
			xrtHttpdResponseUnit(&tSend);
			return XRT_NET_ERROR;
		}
	} else if ( !__xhttpdBuildResponseBytes(&tSend, &pBytes, &iLen) ) {
		__xhttpdUnlock(&pConn->iConnLock);
		xrtHttpdResponseUnit(&tSend);
		return XRT_NET_ERROR;
	}
	// 标记响应已提交
	pConn->bResponseCommitted = true;
	pConn->bResponseDrained = false;
	pConn->bKeepAlive = bKeepAlive;
	// 判断当前线程是否为流的工作线程
	if ( __xnetEngineIsCurrentWorker(pStream->pWorker) ) {
		// 直接在同一线程发送数据
		__xnetStreamAddAsyncHold(pStream);
		__xhttpdUnlock(&pConn->iConnLock);
		iRet = xrtNetStreamSend(pStream, pBytes, iLen);
		if ( iRet != XRT_NET_OK ) {
			xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		}
		else {
			__xhttpdConnArmWriteTimer(pConn, true);
			if ( !bKeepAlive ) {
				xrtNetStreamClose(pStream, XNET_CLOSE_F_GRACEFUL);
			}
		}
		__xnetStreamReleaseAsyncHold(pStream);
	}
	else {
		// 跨线程投递发送任务
		pTask = (__xhttpd_send_task*)XNET_ALLOC(sizeof(__xhttpd_send_task));
		if ( !pTask ) {
			iRet = XRT_NET_ERROR;
			__xnetStreamAddAsyncHold(pStream);
			bAbortAfterUnlock = true;
		}
		else {
			memset(pTask, 0, sizeof(*pTask));
			pTask->pConn = __xhttpdConnAddRef(pConn);
			pTask->pBytes = pBytes;
			pTask->iLen = iLen;
			pTask->bClose = !bKeepAlive;
			iRet = xrtNetEnginePost(pStream->pEngine, pStream->pWorker->iId, __xhttpdSendTaskProc, pTask);
			if ( iRet == XRT_NET_OK ) {
				pBytes = NULL;
			}
			else {
				__xhttpdConnRelease(pTask->pConn);
				XNET_FREE(pTask);
				pTask = NULL;
				__xnetStreamAddAsyncHold(pStream);
				bAbortAfterUnlock = true;
			}
		}
		__xhttpdUnlock(&pConn->iConnLock);
		if ( bAbortAfterUnlock ) {
			xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
			__xnetStreamReleaseAsyncHold(pStream);
		}
	}
	// 清理发送缓冲区
	XNET_FREE(pBytes);
	if ( iRet != XRT_NET_OK ) {
		xrtHttpdResponseUnit(&tSend);
		return iRet;
	}
	xrtHttpdResponseUnit(&tSend);
	// 尝试完成当前请求并复用连接
	__xhttpdConnFinalizeMaybeAsync(pConn);
	return XRT_NET_OK;
}


// xrtHttpdConnReply 相关处理
XXAPI xnet_result xrtHttpdConnReply(xhttpdconn* pConn, uint32 iStatusCode, const char* sReason, const char* sHeaders, const void* pBody, size_t iBodyLen)
{
	xhttpdresponse tResp;
	xnet_result iRet;
	if ( !pConn || (!pBody && iBodyLen > 0u) ) { return XRT_NET_ERROR; }
	xrtHttpdResponseInit(&tResp);
	if ( !xrtHttpdResponseSetStatusEx(&tResp, iStatusCode, sReason) ) {
		xrtHttpdResponseUnit(&tResp);
		return XRT_NET_ERROR;
	}
	if ( !__xhttpdResponseApplyHeaderBlock(&tResp, sHeaders) ) {
		xrtHttpdResponseUnit(&tResp);
		return XRT_NET_ERROR;
	}
	tResp.pBody = (char*)pBody;
	tResp.iBodyLen = iBodyLen;
	iRet = xrtHttpdConnRespond(pConn, &tResp);
	tResp.pBody = NULL;
	tResp.iBodyLen = 0u;
	xrtHttpdResponseUnit(&tResp);
	return iRet;
}


// xrtHttpdConnStart 相关处理
XXAPI xnet_result xrtHttpdConnStart(xhttpdconn* pConn, const xhttpdresponse* pResp)
{
	xhttpdresponse tBase;
	xhttpdresponse tSend;
	xnetstream* pStream;
	xhttpdrequest* pReq;
	char* pBytes = NULL;
	char* pEndBytes = NULL;
	size_t iLen = 0u;
	size_t iEndLen = 0u;
	bool bKeepAlive = false;
	bool bChunked;
	xnet_result iRet = XRT_NET_ERROR;
	if ( !pConn || !pResp ) { return XRT_NET_ERROR; }
	memset(&tBase, 0, sizeof(tBase));
	memset(&tSend, 0, sizeof(tSend));
	if ( !__xhttpdResponseCopy(&tBase, pResp) ) { goto end; }
	if ( __xhttpdStatusForbidsBody(tBase.iStatusCode) ) { goto end; }
	if ( tBase.pBody ) {
		__xhttpdBodyRelease(&tBase.pBody, &tBase.iBodyLen, &tBase.iBodyCap);
	}
	tBase.iBodyLen = 0u;
	if ( !__xhttpdResponseTrailersValid(&tBase) ) { goto end; }
	bChunked = __xhttpdContainsTokenNoCase(xrtHttpdResponseHeader(&tBase, "Transfer-Encoding"), "chunked");
	if ( !__xhttpdResponseHasHeader(&tBase, "Content-Length") &&
		!__xhttpdResponseHasHeader(&tBase, "Transfer-Encoding") ) {
		if ( !xrtHttpdResponseSetHeader(&tBase, "Transfer-Encoding", "chunked") ) { goto end; }
		bChunked = true;
	}
	__xhttpdLock(&pConn->iConnLock);
	pStream = pConn->pStream;
	pReq = pConn->pRequest;
	if ( !pStream || pStream->bClosing || !pReq || pConn->bResponseCommitted || __xhttpdAtomicLoad(&pConn->iCleanupPosted) != 0 ) {
		__xhttpdUnlock(&pConn->iConnLock);
		iRet = XRT_NET_CLOSED;
		goto end;
	}
	if ( pReq->iMethod == XHTTPD_METHOD_HEAD ) {
		__xhttpdUnlock(&pConn->iConnLock);
		goto end;
	}
	if ( !__xhttpdPrepareResponse(pReq, &tBase, &tSend, &bKeepAlive) ) {
		__xhttpdUnlock(&pConn->iConnLock);
		goto end;
	}
	if ( __xhttpdServerIsDraining(pConn->pServer) ) {
		bKeepAlive = false;
		tSend.iFlags |= XHTTPD_RESP_F_CLOSE;
		if ( !xrtHttpdResponseSetHeader(&tSend, "Connection", "close") ) {
			__xhttpdUnlock(&pConn->iConnLock);
			goto end;
		}
	}
	if ( !__xhttpdBuildResponseHeadBytes(&tSend, &pBytes, &iLen) ) {
		__xhttpdUnlock(&pConn->iConnLock);
		goto end;
	}
	if ( bChunked && !__xhttpdBuildChunkEnd(&tSend, &pEndBytes, &iEndLen) ) {
		__xhttpdUnlock(&pConn->iConnLock);
		goto end;
	}
	pConn->bResponseCommitted = true;
	pConn->bResponseDrained = false;
	pConn->bResponseStreaming = true;
	pConn->bResponseChunked = bChunked;
	pConn->bKeepAlive = bKeepAlive;
	__xhttpdConnClearResponseEndLocked(pConn);
	pConn->pResponseEndBytes = pEndBytes;
	pConn->iResponseEndLen = iEndLen;
	pEndBytes = NULL;
	iEndLen = 0u;
	__xnetStreamAddAsyncHold(pStream);
	__xhttpdUnlock(&pConn->iConnLock);
	iRet = xrtNetStreamSend(pStream, pBytes, iLen);
	if ( iRet != XRT_NET_OK ) {
		__xhttpdLock(&pConn->iConnLock);
		pConn->bResponseStreaming = false;
		pConn->bResponseChunked = false;
		__xhttpdConnClearResponseEndLocked(pConn);
		__xhttpdUnlock(&pConn->iConnLock);
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
	}
	else {
		__xhttpdConnArmWriteTimer(pConn, true);
	}
	__xnetStreamReleaseAsyncHold(pStream);
end:
	if ( pBytes ) { XNET_FREE(pBytes); }
	if ( pEndBytes ) { XNET_FREE(pEndBytes); }
	xrtHttpdResponseUnit(&tBase);
	xrtHttpdResponseUnit(&tSend);
	return iRet;
}


// xrtHttpdConnSend 相关处理
XXAPI xnet_result xrtHttpdConnSend(xhttpdconn* pConn, const void* pData, size_t iLen)
{
	xnetstream* pStream;
	char sChunkHead[32];
	xnet_result iRet;
	bool bChunked;
	if ( !pConn || (!pData && iLen > 0u) ) { return XRT_NET_ERROR; }
	if ( iLen == 0u ) { return XRT_NET_OK; }
	__xhttpdLock(&pConn->iConnLock);
	pStream = pConn->pStream;
	if ( !pStream || pStream->bClosing || !pConn->bResponseCommitted || !pConn->bResponseStreaming ||
		__xhttpdAtomicLoad(&pConn->iCleanupPosted) != 0 ) {
		__xhttpdUnlock(&pConn->iConnLock);
		return XRT_NET_CLOSED;
	}
	bChunked = pConn->bResponseChunked;
	__xnetStreamAddAsyncHold(pStream);
	__xhttpdUnlock(&pConn->iConnLock);
	if ( bChunked ) {
		snprintf(sChunkHead, sizeof(sChunkHead), "%llX\r\n", (unsigned long long)iLen);
		iRet = xrtNetStreamSend(pStream, sChunkHead, strlen(sChunkHead));
		if ( iRet == XRT_NET_OK ) { iRet = xrtNetStreamSend(pStream, pData, iLen); }
		if ( iRet == XRT_NET_OK ) { iRet = xrtNetStreamSend(pStream, "\r\n", 2u); }
	}
	else {
		iRet = xrtNetStreamSend(pStream, pData, iLen);
	}
	if ( iRet != XRT_NET_OK ) {
		__xhttpdLock(&pConn->iConnLock);
		pConn->bResponseStreaming = false;
		pConn->bResponseChunked = false;
		__xhttpdConnClearResponseEndLocked(pConn);
		__xhttpdUnlock(&pConn->iConnLock);
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
	}
	else {
		__xhttpdConnArmWriteTimer(pConn, true);
	}
	__xnetStreamReleaseAsyncHold(pStream);
	return iRet;
}


// xrtHttpdConnEnd 相关处理
XXAPI xnet_result xrtHttpdConnEnd(xhttpdconn* pConn)
{
	xnetstream* pStream;
	char* pEndBytes;
	size_t iEndLen;
	bool bKeepAlive;
	bool bChunked;
	xnet_result iRet;
	if ( !pConn ) { return XRT_NET_ERROR; }
	__xhttpdLock(&pConn->iConnLock);
	pStream = pConn->pStream;
	if ( !pStream || pStream->bClosing || !pConn->bResponseCommitted || !pConn->bResponseStreaming ||
		__xhttpdAtomicLoad(&pConn->iCleanupPosted) != 0 ) {
		__xhttpdUnlock(&pConn->iConnLock);
		return XRT_NET_CLOSED;
	}
	bKeepAlive = pConn->bKeepAlive;
	bChunked = pConn->bResponseChunked;
	pEndBytes = pConn->pResponseEndBytes;
	iEndLen = pConn->iResponseEndLen;
	pConn->pResponseEndBytes = NULL;
	pConn->iResponseEndLen = 0u;
	pConn->bResponseStreaming = false;
	pConn->bResponseChunked = false;
	__xnetStreamAddAsyncHold(pStream);
	__xhttpdUnlock(&pConn->iConnLock);
	iRet = bChunked && pEndBytes && iEndLen > 0u
		? xrtNetStreamSend(pStream, pEndBytes, iEndLen)
		: (bChunked ? XRT_NET_ERROR : XRT_NET_OK);
	if ( pEndBytes ) { XNET_FREE(pEndBytes); }
	if ( iRet != XRT_NET_OK ) {
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
	}
	else {
		__xhttpdConnArmWriteTimer(pConn, true);
		if ( !bKeepAlive ) {
			xrtNetStreamClose(pStream, XNET_CLOSE_F_GRACEFUL);
		}
	}
	__xnetStreamReleaseAsyncHold(pStream);
	if ( iRet == XRT_NET_OK ) { __xhttpdConnFinalizeMaybeAsync(pConn); }
	return iRet;
}


// xrtHttpdConnSendFile 相关处理
XXAPI xnet_result xrtHttpdConnSendFile(xhttpdconn* pConn, const xhttpdresponse* pResp, const char* sFilePath, size_t iChunkSize)
{
	xhttpdresponse tFile;
	xfile objFile = NULL;
	char* pBuf = NULL;
	char sLength[32];
	size_t iFileSize;
	size_t iRead;
	xnet_result iRet = XRT_NET_ERROR;
	if ( !pConn || !pResp || !sFilePath || !sFilePath[0] ) { return XRT_NET_ERROR; }
	if ( iChunkSize == 0u ) { iChunkSize = 64u * 1024u; }
	iFileSize = xrtFileGetSize((str)sFilePath);
	objFile = xrtOpen((str)sFilePath, TRUE, XRT_CP_BINARY);
	if ( objFile == NULL ) { return XRT_NET_ERROR; }
	memset(&tFile, 0, sizeof(tFile));
	if ( !__xhttpdResponseCopy(&tFile, pResp) ) {
		xrtClose(objFile);
		return XRT_NET_ERROR;
	}
	if ( !__xhttpdResponseHasHeader(&tFile, "Content-Length") ) {
		snprintf(sLength, sizeof(sLength), "%llu", (unsigned long long)iFileSize);
		if ( !xrtHttpdResponseSetHeader(&tFile, "Content-Length", sLength) ) {
			xrtClose(objFile);
			xrtHttpdResponseUnit(&tFile);
			return XRT_NET_ERROR;
		}
	}
	if ( iFileSize == 0u ) {
		iRet = xrtHttpdConnRespond(pConn, &tFile);
		xrtClose(objFile);
		xrtHttpdResponseUnit(&tFile);
		return iRet;
	}
	pBuf = (char*)XNET_ALLOC(iChunkSize);
	if ( pBuf == NULL ) {
		xrtClose(objFile);
		xrtHttpdResponseUnit(&tFile);
		return XRT_NET_ERROR;
	}
	iRet = xrtHttpdConnStart(pConn, &tFile);
	if ( iRet == XRT_NET_OK ) {
		while ( (iRead = xrtGetBuffer(objFile, pBuf, iChunkSize)) > 0u ) {
			iRet = xrtHttpdConnSend(pConn, pBuf, iRead);
			if ( iRet != XRT_NET_OK ) { break; }
		}
		if ( iRet == XRT_NET_OK ) { iRet = xrtHttpdConnEnd(pConn); }
	}
	XNET_FREE(pBuf);
	xrtClose(objFile);
	xrtHttpdResponseUnit(&tFile);
	return iRet;
}


// xrtHttpdConnClose 相关处理
// xrtHttpdConnSendFileRange 相关处理
XXAPI xnet_result xrtHttpdConnSendFileRange(xhttpdconn* pConn, const xhttpdresponse* pResp, const char* sFilePath, size_t iStart, size_t iLen, size_t iChunkSize)
{
	xhttpdresponse tFile;
	xfile objFile = NULL;
	char* pBuf = NULL;
	char sLength[32];
	size_t iFileSize;
	size_t iRemain;
	xnet_result iRet = XRT_NET_ERROR;
	if ( !pConn || !pResp || !sFilePath || !sFilePath[0] ) { return XRT_NET_ERROR; }
	if ( iChunkSize == 0u ) { iChunkSize = 64u * 1024u; }
	iFileSize = xrtFileGetSize((str)sFilePath);
	if ( iStart > iFileSize ) { return XRT_NET_ERROR; }
	if ( iLen > iFileSize - iStart ) { iLen = iFileSize - iStart; }
	objFile = xrtOpen((str)sFilePath, TRUE, XRT_CP_BINARY);
	if ( objFile == NULL ) { return XRT_NET_ERROR; }
	memset(&tFile, 0, sizeof(tFile));
	if ( !__xhttpdResponseCopy(&tFile, pResp) ) {
		xrtClose(objFile);
		return XRT_NET_ERROR;
	}
	if ( !__xhttpdResponseHasHeader(&tFile, "Content-Length") ) {
		snprintf(sLength, sizeof(sLength), "%llu", (unsigned long long)iLen);
		if ( !xrtHttpdResponseSetHeader(&tFile, "Content-Length", sLength) ) {
			xrtClose(objFile);
			xrtHttpdResponseUnit(&tFile);
			return XRT_NET_ERROR;
		}
	}
	if ( iLen == 0u ) {
		iRet = xrtHttpdConnRespond(pConn, &tFile);
		xrtClose(objFile);
		xrtHttpdResponseUnit(&tFile);
		return iRet;
	}
	if ( xrtSeek(objFile, (int64)iStart, XRT_SEEK_SET) != iStart ) {
		xrtClose(objFile);
		xrtHttpdResponseUnit(&tFile);
		return XRT_NET_ERROR;
	}
	pBuf = (char*)XNET_ALLOC(iChunkSize);
	if ( pBuf == NULL ) {
		xrtClose(objFile);
		xrtHttpdResponseUnit(&tFile);
		return XRT_NET_ERROR;
	}
	iRet = xrtHttpdConnStart(pConn, &tFile);
	if ( iRet == XRT_NET_OK ) {
		iRemain = iLen;
		while ( iRemain > 0u ) {
			size_t iWant = iRemain < iChunkSize ? iRemain : iChunkSize;
			size_t iRead = xrtGetBuffer(objFile, pBuf, iWant);
			if ( iRead == 0u ) { iRet = XRT_NET_ERROR; break; }
			iRet = xrtHttpdConnSend(pConn, pBuf, iRead);
			if ( iRet != XRT_NET_OK ) { break; }
			iRemain -= iRead;
		}
		if ( iRet == XRT_NET_OK ) { iRet = xrtHttpdConnEnd(pConn); }
	}
	XNET_FREE(pBuf);
	xrtClose(objFile);
	xrtHttpdResponseUnit(&tFile);
	return iRet;
}


XXAPI xnet_result xrtHttpdConnClose(xhttpdconn* pConn, uint32 iCloseFlags)
{
	xnetstream* pStream;
	if ( !pConn ) { return XRT_NET_ERROR; }
	__xhttpdLock(&pConn->iConnLock);
	pStream = pConn->pStream;
	if ( !pStream || __xhttpdAtomicLoad(&pConn->iCleanupPosted) != 0 ) {
		__xhttpdUnlock(&pConn->iConnLock);
		return XRT_NET_ERROR;
	}
	__xnetStreamAddAsyncHold(pStream);
	__xhttpdUnlock(&pConn->iConnLock);
	xrtNetStreamClose(pStream, iCloseFlags);
	__xnetStreamReleaseAsyncHold(pStream);
	return XRT_NET_OK;
}


// 内部函数：__xhttpdServerAddConn
static void __xhttpdServerAddConn(xhttpdserver* pServer, xhttpdconn* pConn)
{
	if ( !pServer || !pConn ) { return; }
	__xhttpdLock(&pServer->iConnLock);
	pConn->pNext = pServer->pConnHead;
	pServer->pConnHead = pConn;
	__xhttpdUnlock(&pServer->iConnLock);
}


// 内部函数：__xhttpdServerRemoveConn
static void __xhttpdServerRemoveConn(xhttpdserver* pServer, xhttpdconn* pConn)
{
	xhttpdconn** ppNode;
	if ( !pServer || !pConn ) { return; }
	__xhttpdLock(&pServer->iConnLock);
	ppNode = &pServer->pConnHead;
	while ( *ppNode ) {
		if ( *ppNode == pConn ) {
			*ppNode = pConn->pNext;
			break;
		}
		ppNode = &(*ppNode)->pNext;
	}
	pConn->pNext = NULL;
	__xhttpdUnlock(&pServer->iConnLock);
}


static void __xhttpdUpgradeTaskProc(xnetworker* pWorker, ptr pArg)
{
	__xhttpd_upgrade_task* pTask = (__xhttpd_upgrade_task*)pArg;
	xnetstream* pStream;
	(void)pWorker;
	if ( !pTask ) { return; }
	pStream = pTask->pStream;
	if ( pTask->bHandoff ) {
		__xhttpdConnTryFinalizeRequest(pTask->pConn);
		if ( pStream && !pStream->bClosing && xrtNetChainBytes(&pStream->tRxChain) > 0u &&
			pStream->pEvents && pStream->pEvents->OnRecv ) {
			pStream->pEvents->OnRecv(__xnetStreamOwner(pStream), pStream, &pStream->tRxChain);
		}
	}
	if ( pStream ) { __xnetStreamReleaseAsyncHold(pStream); }
	__xhttpdConnRelease(pTask->pConn);
	XNET_FREE(pTask);
}


XXAPI xnet_result xrtHttpdConnUpgrade(xhttpdconn* pConn, const xhttpdresponse* pResp,
	const xnetstreamevents* pEvents, ptr pUserData, xnetstream** ppStream)
{
	__xhttpd_upgrade_task* pTask = NULL;
	xhttpdserver* pServer;
	xhttpdrequest* pReq;
	xnetstream* pStream;
	char* pBytes = NULL;
	size_t iLen = 0u;
	xnet_result iRet;
	if ( ppStream ) { *ppStream = NULL; }
	if ( !pConn || !pResp || !ppStream || pResp->iStatusCode != 101u ||
		pResp->iBodyLen != 0u || pResp->iTrailerCount != 0u ||
		!xrtHttpdResponseHeader(pResp, "Upgrade") ||
		!__xhttpdContainsTokenNoCase(xrtHttpdResponseHeader(pResp, "Connection"), "Upgrade") ||
		__xhttpdResponseHasHeader(pResp, "Content-Length") ||
		__xhttpdResponseHasHeader(pResp, "Transfer-Encoding") ) {
		return XRT_NET_ERROR;
	}
	if ( !__xhttpdBuildResponseBytes(pResp, &pBytes, &iLen) ) { return XRT_NET_ERROR; }
	pTask = (__xhttpd_upgrade_task*)XNET_ALLOC(sizeof(*pTask));
	if ( !pTask ) {
		XNET_FREE(pBytes);
		return XRT_NET_ERROR;
	}
	memset(pTask, 0, sizeof(*pTask));
	__xhttpdLock(&pConn->iConnLock);
	pServer = pConn->pServer;
	pReq = pConn->pRequest;
	pStream = pConn->pStream;
	if ( !pServer || !pReq || !pStream || pStream->bClosing || pConn->bResponseCommitted ||
		pConn->bAsyncPending || pConn->bUpgraded ||
		__xhttpdAtomicLoad(&pConn->iCleanupPosted) != 0 ||
		(pReq->iFlags & XHTTPD_REQ_F_UPGRADE) == 0u ||
		pReq->iBodyLen != 0u || pReq->iContentLength > 0 ||
		!__xnetEngineIsCurrentWorker(pStream->pWorker) ) {
		__xhttpdUnlock(&pConn->iConnLock);
		XNET_FREE(pTask);
		XNET_FREE(pBytes);
		return XRT_NET_ERROR;
	}
	pTask->pConn = __xhttpdConnAddRef(pConn);
	pTask->pStream = pStream;
	__xnetStreamAddAsyncHold(pStream);
	__xhttpdUnlock(&pConn->iConnLock);
	if ( xrtNetEnginePost(pStream->pEngine, pStream->pWorker->iId,
		__xhttpdUpgradeTaskProc, pTask) != XRT_NET_OK ) {
		__xnetStreamReleaseAsyncHold(pStream);
		__xhttpdConnRelease(pTask->pConn);
		XNET_FREE(pTask);
		XNET_FREE(pBytes);
		return XRT_NET_ERROR;
	}
	iRet = xrtNetStreamSend(pStream, pBytes, iLen);
	XNET_FREE(pBytes);
	if ( iRet != XRT_NET_OK ) {
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return iRet;
	}
	__xhttpdLock(&pConn->iConnLock);
	if ( pConn->pStream != pStream || pStream->bClosing ||
		__xhttpdAtomicLoad(&pConn->iCleanupPosted) != 0 ) {
		__xhttpdUnlock(&pConn->iConnLock);
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return XRT_NET_CLOSED;
	}
	pConn->bResponseCommitted = true;
	pConn->bResponseDrained = true;
	pConn->bUpgraded = true;
	pConn->bKeepAlive = false;
	pConn->pStream = NULL;
	pStream->pEvents = pEvents;
	pStream->pUserData = pUserData;
	__xhttpdUnlock(&pConn->iConnLock);
	__xhttpdConnCancelTimer(pConn);
	__xhttpdServerRemoveConn(pServer, pConn);
	pTask->bHandoff = true;
	*ppStream = pStream;
	/* Drop the HTTP connection's base ownership; the posted handoff task holds the final reference. */
	__xhttpdConnRelease(pConn);
	return XRT_NET_OK;
}


// 内部函数：__xhttpdServerDetachAllConns
static xhttpdconn* __xhttpdServerDetachAllConns(xhttpdserver* pServer)
{
	xhttpdconn* pHead;
	xhttpdconn* pCur;
	if ( !pServer ) { return NULL; }
	__xhttpdLock(&pServer->iConnLock);
	pHead = pServer->pConnHead;
	for ( pCur = pHead; pCur; pCur = pCur->pNext ) {
		// stop 线程会在锁外遍历连接链表；这里临时持有引用，避免异步 cleanup 并发释放节点。
		__xhttpdConnAddRef(pCur);
	}
	pServer->pConnHead = NULL;
	__xhttpdUnlock(&pServer->iConnLock);
	return pHead;
}


// 内部函数：__xhttpdConnCleanupTask
static void __xhttpdConnCleanupTask(xnetworker* pWorker, ptr pArg)
{
	xhttpdconn* pConn = (xhttpdconn*)pArg;
	xnetstream* pStream = NULL;
	xhttpdrequest* pReq = NULL;
	(void)pWorker;
	if ( !pConn ) { return; }
	__xhttpdConnCancelTimer(pConn);
	__xhttpdLock(&pConn->iConnLock);
	if ( pConn->pServer ) {
		__xhttpdServerRemoveConn(pConn->pServer, pConn);
		pConn->pServer = NULL;
	}
	pStream = pConn->pStream;
	pConn->pStream = NULL;
	if ( !pConn->bAsyncPending ) {
		pReq = __xhttpdConnDetachRequestLocked(pConn);
	}
	__xhttpdUnlock(&pConn->iConnLock);
	if ( pStream ) { xrtNetStreamDestroy(pStream); }
	__xhttpdRequestDestroy(pReq);
	__xhttpdConnRelease(pConn);
}


// 内部函数：__xhttpdConnPostCleanup
static void __xhttpdConnPostCleanup(xhttpdconn* pConn)
{
	if ( !pConn ) { return; }
	if ( __xhttpdAtomicCompareExchange(&pConn->iCleanupPosted, 1, 0) != 0 ) { return; }
	if ( pConn->pServer && pConn->pServer->pEngine && pConn->pStream && pConn->pStream->pWorker ) {
		if ( xrtNetEnginePost(pConn->pServer->pEngine, pConn->pStream->pWorker->iId, __xhttpdConnCleanupTask, pConn) == XRT_NET_OK ) {
			return;
		}
	}
	__xhttpdConnCleanupTask(NULL, pConn);
}


// 内部函数：__xhttpdEmitServerError
static void __xhttpdEmitServerError(xhttpdserver* pServer, xhttpdconn* pConn, int iSysErr)
{
	if ( pServer && pServer->tEvents.OnError ) {
		pServer->tEvents.OnError(pServer->pUserData, pServer, pConn, iSysErr);
	}
}


// 内部函数：判断是否为 benign 流错误
static bool __xhttpdIsBenignStreamError(xhttpdconn* pConn, xnetstream* pStream, int iSysErr)
{
	if ( pConn && __xhttpdAtomicLoad(&pConn->iCleanupPosted) != 0 ) { return true; }
	if ( pStream && pStream->bClosing ) { return true; }
	if ( pConn && !pConn->bResponseInFlight && iSysErr == -1 ) { return true; }
	#if defined(_WIN32) || defined(_WIN64)
		if ( iSysErr == WSAECONNRESET || iSysErr == WSAECONNABORTED || iSysErr == WSAESHUTDOWN ||
			iSysErr == WSAENOTSOCK || iSysErr == WSA_OPERATION_ABORTED ) {
			return true;
		}
	#else
		if ( iSysErr == ECONNRESET || iSysErr == ECONNABORTED || iSysErr == EPIPE ||
			iSysErr == ESHUTDOWN || iSysErr == ENOTSOCK || iSysErr == EBADF ) {
			return true;
		}
	#endif
	return false;
}


// 内部函数：__xhttpdSendResponseAndClose
static bool __xhttpdSendResponseAndClose(xhttpdconn* pConn, const xhttpdresponse* pResp)
{
	char* pBytes = NULL;
	size_t iLen = 0u;
	xnetstream* pStream;
	xnet_result iRet;
	if ( !pConn || !pResp ) { return false; }
	__xhttpdLock(&pConn->iConnLock);
	pStream = pConn->pStream;
	if ( !pStream || __xhttpdAtomicLoad(&pConn->iCleanupPosted) != 0 ) {
		__xhttpdUnlock(&pConn->iConnLock);
		return false;
	}
	__xnetStreamAddAsyncHold(pStream);
	__xhttpdUnlock(&pConn->iConnLock);
	if ( !__xhttpdBuildResponseBytes(pResp, &pBytes, &iLen) ) {
		__xnetStreamReleaseAsyncHold(pStream);
		return false;
	}
	iRet = xrtNetStreamSend(pStream, pBytes, iLen);
	XNET_FREE(pBytes);
	if ( iRet != XRT_NET_OK ) {
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		__xnetStreamReleaseAsyncHold(pStream);
		return false;
	}
	xrtNetStreamClose(pStream, XNET_CLOSE_F_GRACEFUL);
	__xnetStreamReleaseAsyncHold(pStream);
	return true;
}


// 内部函数：__xhttpdSendSimpleStatus
static void __xhttpdSendSimpleStatus(xhttpdconn* pConn, uint32 iStatusCode, const char* sBody)
{
	xhttpdresponse tResp;
	bool bHasRequest = false;
	xrtHttpdResponseInit(&tResp);
	xrtHttpdResponseSetStatus(&tResp, iStatusCode, NULL);
	if ( sBody && sBody[0] ) {
		(void)xrtHttpdResponseSetBodyCopy(&tResp, sBody, strlen(sBody), "text/plain");
	}
	if ( pConn ) {
		__xhttpdLock(&pConn->iConnLock);
		bHasRequest = pConn->pRequest != NULL;
		__xhttpdUnlock(&pConn->iConnLock);
	}
	if ( bHasRequest ) { (void)xrtHttpdConnRespond(pConn, &tResp); }
	else { (void)__xhttpdSendResponseAndClose(pConn, &tResp); }
	xrtHttpdResponseUnit(&tResp);
}


// 内部函数：获取 Future 错误状态
static uint32 __xhttpdFutureErrorStatus(const xfuture_result* pResult)
{
	if ( !pResult ) { return 500u; }
	if ( pResult->iStatus == XRT_NET_TIMEOUT || (pResult->iFlags & XFUTURE_RESULT_F_TIMEOUT) != 0u ) { return 408u; }
	if ( pResult->iStatus == XRT_NET_CANCELLED || pResult->iStatus == XRT_NET_CLOSED ||
		(pResult->iFlags & (XFUTURE_RESULT_F_CANCELLED | XFUTURE_RESULT_F_CLOSED)) != 0u ) {
		return 503u;
	}
	return 500u;
}


// 内部函数：提前处理 Expect: 100-continue
static uint32 __xhttpdMaybeSendContinue(xhttpdconn* pConn, xnetstream* pStream, const xcodechttp1msg* pMsg)
{
	const char* sExpect = NULL;
	uint32 iExpectCount = 0u;
	bool bShouldSend = false;
	if ( !pConn || !pStream || !pMsg ) { return 417u; }
	for ( uint32 i = 0u; i < pMsg->iHeaderCount; ++i ) {
		if ( __xhttpdStrEqNoCase(pMsg->pHeaders[i].sName, "Expect") ) {
			iExpectCount++;
			sExpect = pMsg->pHeaders[i].sValue;
		}
	}
	if ( iExpectCount == 0u ) { return 0u; }
	if ( iExpectCount != 1u || !sExpect || !__xhttpdStrEqNoCase(sExpect, "100-continue") ||
		!__xhttpdStrEqNoCase(pMsg->sVersion, "HTTP/1.1") ) { return 417u; }
	if ( pMsg->iContentLength == 0 ||
		(pMsg->iContentLength < 0 && (pMsg->iFlags & XCODEC_HTTP1_F_CHUNKED) == 0u) ) { return 0u; }
	__xhttpdLock(&pConn->iConnLock);
	if ( pConn->bContinueSent || pConn->bResponseInFlight || __xhttpdAtomicLoad(&pConn->iCleanupPosted) != 0 ) {
		__xhttpdUnlock(&pConn->iConnLock);
		return 0u;
	}
	if ( !pConn->bContinueSent && !pConn->bResponseInFlight && pConn->pStream == pStream ) {
		pConn->bContinueSent = true;
		bShouldSend = true;
	} else {
		bShouldSend = false;
	}
	__xhttpdUnlock(&pConn->iConnLock);
	if ( bShouldSend ) {
		(void)xrtNetStreamSend(pStream, "HTTP/1.1 100 Continue\r\n\r\n", 25u);
	}
	return bShouldSend ? 100u : 0u;
}


// 内部函数：获取状态正文
static const char* __xhttpdStatusBody(uint32 iStatusCode)
{
	switch ( iStatusCode ) {
		case 404: return "Not Found";
		case 408: return "Request Timeout";
		case 413: return "Payload Too Large";
		case 417: return "Expectation Failed";
		case 431: return "Request Header Fields Too Large";
		case 503: return "Service Unavailable";
		default: return "Internal Server Error";
	}
}


// 内部函数：__xhttpdAsyncRequestFinally
static uint64 __xhttpdConnTimerId(const xnetstream* pStream)
{
	return pStream ? (uint64)(uintptr_t)pStream : 0u;
}


static uint32 __xhttpdConnTimerDelayMs(const xhttpdserver* pServer, uint32 iKind)
{
	if ( !pServer ) { return 0u; }
	switch ( iKind ) {
		case XHTTPD_CONN_TIMER_HEADER: return pServer->tConfig.iHeaderTimeoutMs;
		case XHTTPD_CONN_TIMER_BODY: return pServer->tConfig.iBodyTimeoutMs;
		case XHTTPD_CONN_TIMER_IDLE: return pServer->tConfig.iIdleTimeoutMs;
		case XHTTPD_CONN_TIMER_WRITE: return pServer->tConfig.iWriteTimeoutMs;
		default: return 0u;
	}
}


static void __xhttpdConnCancelTimerKind(xhttpdconn* pConn, uint32 iKind)
{
	xnetstream* pStream = NULL;
	bool bNeedCancel = false;
	if ( !pConn ) { return; }
	__xhttpdLock(&pConn->iConnLock);
	if ( pConn->iTimerState != 0 &&
		(iKind == XHTTPD_CONN_TIMER_NONE || pConn->iTimerKind == iKind) ) {
		pStream = pConn->pStream;
		pConn->iTimerState = 0;
		pConn->iTimerKind = XHTTPD_CONN_TIMER_NONE;
		pConn->iTimerGen++;
		if ( pStream && pStream->pWorker ) {
			__xnetStreamAddAsyncHold(pStream);
			bNeedCancel = true;
		}
	}
	__xhttpdUnlock(&pConn->iConnLock);
	if ( bNeedCancel ) {
		if ( xrtNetPortCancelTimer(&pStream->pWorker->tPort, __xhttpdConnTimerId(pStream)) == XRT_NET_OK ) {
			__xnetStreamReleaseAsyncHold(pStream);
		}
		__xnetStreamReleaseAsyncHold(pStream);
	}
}


static void __xhttpdConnCancelTimer(xhttpdconn* pConn)
{
	__xhttpdConnCancelTimerKind(pConn, XHTTPD_CONN_TIMER_NONE);
}


static void __xhttpdConnArmTimer(xhttpdconn* pConn, uint32 iKind, bool bRestart)
{
	xnetstream* pStream = NULL;
	xnetworker* pWorker = NULL;
	xhttpdserver* pServer;
	uint32 iDelayMs;
	uint32 iGen;
	if ( !pConn || iKind == XHTTPD_CONN_TIMER_NONE ) { return; }
	__xhttpdLock(&pConn->iConnLock);
	pServer = pConn->pServer;
	iDelayMs = __xhttpdConnTimerDelayMs(pServer, iKind);
	if ( iDelayMs == 0u || __xhttpdAtomicLoad(&pConn->iCleanupPosted) != 0 ||
		!pConn->pStream || pConn->pStream->bClosing || !pConn->pStream->pWorker ) {
		__xhttpdUnlock(&pConn->iConnLock);
		return;
	}
	if ( pConn->iTimerState != 0 && pConn->iTimerKind == iKind && !bRestart ) {
		__xhttpdUnlock(&pConn->iConnLock);
		return;
	}
	__xhttpdUnlock(&pConn->iConnLock);

	__xhttpdConnCancelTimer(pConn);

	__xhttpdLock(&pConn->iConnLock);
	pServer = pConn->pServer;
	iDelayMs = __xhttpdConnTimerDelayMs(pServer, iKind);
	if ( iDelayMs == 0u || __xhttpdAtomicLoad(&pConn->iCleanupPosted) != 0 ||
		!pConn->pStream || pConn->pStream->bClosing || !pConn->pStream->pWorker ) {
		__xhttpdUnlock(&pConn->iConnLock);
		return;
	}
	pStream = pConn->pStream;
	pWorker = pStream->pWorker;
	pConn->iTimerState = 1;
	pConn->iTimerKind = iKind;
	pConn->iTimerGen++;
	iGen = pConn->iTimerGen;
	__xnetStreamAddAsyncHold(pStream);
	__xhttpdUnlock(&pConn->iConnLock);

	if ( xrtNetPortArmTimer(&pWorker->tPort, __xhttpdConnTimerId(pStream), iDelayMs) != XRT_NET_OK ) {
		__xhttpdLock(&pConn->iConnLock);
		if ( pConn->iTimerState != 0 && pConn->iTimerGen == iGen ) {
			pConn->iTimerState = 0;
			pConn->iTimerKind = XHTTPD_CONN_TIMER_NONE;
			pConn->iTimerGen++;
		}
		__xhttpdUnlock(&pConn->iConnLock);
		__xnetStreamReleaseAsyncHold(pStream);
	}
}


static void __xhttpdConnArmWriteTimer(xhttpdconn* pConn, bool bRestart)
{
	xnetstream* pStream = NULL;
	bool bNeedTimer = false;
	if ( !pConn ) { return; }
	__xhttpdLock(&pConn->iConnLock);
	pStream = pConn->pStream;
	if ( pStream && !pStream->bClosing &&
		pConn->bResponseInFlight && pConn->bResponseCommitted && !pConn->bResponseDrained &&
		__xhttpdAtomicLoad(&pConn->iCleanupPosted) == 0 &&
		xrtNetStreamPendingSend(pStream) > 0u ) {
		bNeedTimer = true;
	}
	__xhttpdUnlock(&pConn->iConnLock);
	if ( bNeedTimer ) {
		__xhttpdConnArmTimer(pConn, XHTTPD_CONN_TIMER_WRITE, bRestart);
	}
	else {
		__xhttpdConnCancelTimerKind(pConn, XHTTPD_CONN_TIMER_WRITE);
	}
}


static void __xhttpdStreamOnTimer(ptr pOwner, xnetstream* pStream)
{
	xhttpdconn* pConn = (xhttpdconn*)pOwner;
	uint32 iKind;
	bool bValid = false;
	if ( !pConn || !pStream ) { return; }
	__xhttpdLock(&pConn->iConnLock);
	iKind = pConn->iTimerKind;
	if ( pConn->iTimerState != 0 && pConn->pStream == pStream &&
		__xhttpdAtomicLoad(&pConn->iCleanupPosted) == 0 && !pStream->bClosing ) {
		if ( iKind == XHTTPD_CONN_TIMER_WRITE ) {
			bValid = pConn->bResponseInFlight && pConn->bResponseCommitted &&
				!pConn->bResponseDrained && xrtNetStreamPendingSend(pStream) > 0u;
		}
		else {
			bValid = !pConn->bResponseInFlight;
		}
	}
	pConn->iTimerState = 0;
	pConn->iTimerKind = XHTTPD_CONN_TIMER_NONE;
	pConn->iTimerGen++;
	__xhttpdUnlock(&pConn->iConnLock);
	if ( !bValid ) { return; }
	if ( iKind == XHTTPD_CONN_TIMER_WRITE ) {
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
	}
	else if ( iKind == XHTTPD_CONN_TIMER_IDLE ) {
		xrtNetStreamClose(pStream, XNET_CLOSE_F_GRACEFUL);
	}
	else {
		__xhttpdSendSimpleStatus(pConn, 408u, __xhttpdStatusBody(408u));
	}
}


static void __xhttpdAsyncRequestFinally(const xfuture_result* pResult, ptr pArg)
{
	__xhttpd_async_ctx* pCtx = (__xhttpd_async_ctx*)pArg;
	xhttpdconn* pConn;
	xhttpdresponse* pResp = NULL;
	bool bCanRespond;
	bool bHasCommitted;

	if ( !pCtx ) { return; }
	pConn = pCtx->pConn;
	// 提取异步处理返回的响应
	if ( pResult && pResult->iStatus == XRT_NET_OK ) {
		pResp = (xhttpdresponse*)pResult->pValue;
	}

	// 更新连接状态，清除异步挂起标记
	__xhttpdLock(&pConn->iConnLock);
	pConn->bAsyncPending = false;
	bHasCommitted = pConn->bResponseCommitted;
	bCanRespond = (pConn->pStream != NULL) && !pConn->pStream->bClosing &&
		(__xhttpdAtomicLoad(&pConn->iCleanupPosted) == 0);
	__xhttpdUnlock(&pConn->iConnLock);

	// 在连接可用且响应未提交时发送结果
	if ( bCanRespond && !bHasCommitted ) {
		if ( pResp ) {
			// 正常响应
			(void)xrtHttpdConnRespond(pConn, pResp);
		}
		else if ( pResult && pResult->iStatus == XRT_NET_OK ) {
			// 异步成功但未返回响应，返回 404
			__xhttpdSendSimpleStatus(pConn, 404u, __xhttpdStatusBody(404u));
		}
		else {
			// 异步处理失败，返回对应错误状态码
			uint32 iStatusCode = __xhttpdFutureErrorStatus(pResult);
			__xhttpdSendSimpleStatus(pConn, iStatusCode, __xhttpdStatusBody(iStatusCode));
		}
	}

	// 释放资源和引用计数
	if ( pResp ) { xrtHttpdResponseDestroy(pResp); }
	__xhttpdConnFinalizeMaybeAsync(pConn);
	xFutureRelease(pCtx->pFuture);
	__xhttpdConnRelease(pConn);
	XNET_FREE(pCtx);
}


// 内部函数：__xhttpdAttachAsyncRequest
static bool __xhttpdAttachAsyncRequest(xhttpdconn* pConn, xfuture* pFuture)
{
	__xhttpd_async_ctx* pCtx;
	xfuture* pHook;

	if ( !pConn || !pFuture ) { return false; }
	pCtx = (__xhttpd_async_ctx*)XNET_ALLOC(sizeof(__xhttpd_async_ctx));
	if ( !pCtx ) { return false; }
	memset(pCtx, 0, sizeof(*pCtx));
	pCtx->pConn = __xhttpdConnAddRef(pConn);
	pCtx->pFuture = xFutureAddRef(pFuture);
	pHook = xFutureFinallyEngine(pFuture, NULL, 0, __xhttpdAsyncRequestFinally, pCtx);
	xFutureRelease(pFuture);
	if ( !pHook ) {
		xFutureRelease(pCtx->pFuture);
		__xhttpdConnRelease(pCtx->pConn);
		XNET_FREE(pCtx);
		return false;
	}
	xFutureRelease(pHook);
	return true;
}


// 内部函数：__xhttpdListenerOnAccept
static bool __xhttpdListenerOnAccept(ptr pOwner, xnetlistener* pListener, xnetstream* pStream)
{
	xhttpdserver* pServer = (xhttpdserver*)pOwner;
	xhttpdconn* pConn;
	(void)pListener;
	if ( !pServer || !pStream ) { return false; }
	pConn = (xhttpdconn*)xrtNetStreamGetUserData(pStream);
	if ( !pConn ) {
		pConn = (xhttpdconn*)XNET_ALLOC(sizeof(xhttpdconn));
		if ( !pConn ) { return false; }
		memset(pConn, 0, sizeof(xhttpdconn));
		pConn->iRefCount = 1;
		xrtNetStreamSetUserData(pStream, pConn);
	}
	pConn->pServer = pServer;
	pConn->pServerRef = __xhttpdServerAddRef(pServer);
	pConn->pStream = pStream;
	__xhttpdServerAddConn(pServer, pConn);
	return true;
}


// 内部函数：__xhttpdStreamOnOpen
static void __xhttpdStreamOnOpen(ptr pOwner, xnetstream* pStream)
{
	xhttpdconn* pConn = (xhttpdconn*)pOwner;
	xhttpdserver* pServer = pConn ? pConn->pServer : NULL;
	(void)pStream;
	__xhttpdConnArmTimer(pConn, XHTTPD_CONN_TIMER_HEADER, false);
	if ( pServer && pServer->tEvents.OnOpen ) {
		pServer->tEvents.OnOpen(pServer->pUserData, pServer, pConn);
	}
}


// 内部函数：判断是否启用 request body 流式回调
static bool __xhttpdHasRequestBodyStream(const xhttpdserver* pServer)
{
	return pServer &&
		pServer->tEvents.OnRequestBody != NULL &&
		(pServer->tEvents.OnRequestBodyEnd != NULL || pServer->tEvents.OnRequestBodyEndAsync != NULL);
}


// 内部函数：重置 request body 流式状态
static void __xhttpdResetBodyStreamLocked(xhttpdconn* pConn)
{
	if ( !pConn ) { return; }
	pConn->bRequestStreaming = false;
	pConn->iBodyMode = XHTTPD_BODY_MODE_NONE;
	pConn->iChunkState = XHTTPD_CHUNK_STATE_SIZE;
	pConn->iBodyRemain = 0u;
	pConn->iBodyReceived = 0u;
	pConn->iChunkRemain = 0u;
}


// 内部函数：解析 HTTP chunk size 行
// 内部函数：在链式缓冲中查找 CRLF
// 内部函数：在链式缓冲中查找 CRLFCRLF
// 内部函数：发送 request body 流式错误响应
static void __xhttpdBodyStreamFail(xhttpdconn* pConn, uint32 iStatusCode, const char* sBody)
{
	if ( !pConn ) { return; }
	__xhttpdLock(&pConn->iConnLock);
	__xhttpdResetBodyStreamLocked(pConn);
	__xhttpdUnlock(&pConn->iConnLock);
	__xhttpdConnCancelTimer(pConn);
	__xhttpdSendSimpleStatus(pConn, iStatusCode, sBody ? sBody : __xhttpdStatusBody(iStatusCode));
}


// 内部函数：投递 request body 数据块
static uint32 __xhttpdEmitRequestBodyChunk(xhttpdconn* pConn, const void* pData, size_t iLen)
{
	xhttpdserver* pServer;
	xhttpdrequest* pReq;
	if ( !pConn || (!pData && iLen > 0u) ) { return XHTTPD_BODY_STREAM_INTERNAL_ERROR; }
	if ( iLen == 0u ) { return XHTTPD_BODY_STREAM_OK; }
	__xhttpdLock(&pConn->iConnLock);
	pServer = pConn->pServer;
	pReq = pConn->pRequest;
	if ( !pServer || !pReq || !pConn->bRequestStreaming ) {
		__xhttpdUnlock(&pConn->iConnLock);
		return XHTTPD_BODY_STREAM_INTERNAL_ERROR;
	}
	if ( pServer->tConfig.iBodyLimit > 0u ) {
		uint64 iLimit = (uint64)pServer->tConfig.iBodyLimit;
		if ( (uint64)iLen > iLimit || pConn->iBodyReceived > iLimit - (uint64)iLen ) {
			__xhttpdUnlock(&pConn->iConnLock);
			return XHTTPD_BODY_STREAM_TOO_LARGE;
		}
	}
	pConn->iBodyReceived += (uint64)iLen;
	pReq->iBodyLen = (size_t)pConn->iBodyReceived;
	__xhttpdUnlock(&pConn->iConnLock);
	return pServer->tEvents.OnRequestBody(pServer->pUserData, pServer, pConn, pReq, pData, iLen) ?
		XHTTPD_BODY_STREAM_OK :
		XHTTPD_BODY_STREAM_INTERNAL_ERROR;
}


// 内部函数：完成 request body 流式处理并生成响应
static void __xhttpdCompleteRequestBodyStream(xhttpdconn* pConn)
{
	xhttpdserver* pServer;
	xhttpdrequest* pReq;
	xhttpdresponse tResp;
	xfuture* pAsync = NULL;
	bool bHandled = false;
	bool bResponseCommitted = false;
	if ( !pConn ) { return; }
	__xhttpdConnCancelTimer(pConn);
	xrtHttpdResponseInit(&tResp);
	__xhttpdLock(&pConn->iConnLock);
	pServer = pConn->pServer;
	pReq = pConn->pRequest;
	__xhttpdResetBodyStreamLocked(pConn);
	__xhttpdUnlock(&pConn->iConnLock);
	if ( pServer && pReq && pServer->tEvents.OnRequestBodyEndAsync ) {
		__xhttpdLock(&pConn->iConnLock);
		pConn->bAsyncPending = true;
		__xhttpdUnlock(&pConn->iConnLock);
		pAsync = pServer->tEvents.OnRequestBodyEndAsync(pServer->pUserData, pServer, pConn, pReq);
		if ( pAsync ) {
			if ( !__xhttpdAttachAsyncRequest(pConn, pAsync) ) {
				__xhttpdLock(&pConn->iConnLock);
				pConn->bAsyncPending = false;
				__xhttpdUnlock(&pConn->iConnLock);
				__xhttpdEmitServerError(pServer, pConn, -1);
				__xhttpdSendSimpleStatus(pConn, 500u, __xhttpdStatusBody(500u));
			}
			xrtHttpdResponseUnit(&tResp);
			return;
		}
		__xhttpdLock(&pConn->iConnLock);
		pConn->bAsyncPending = false;
		bResponseCommitted = pConn->bResponseCommitted;
		__xhttpdUnlock(&pConn->iConnLock);
		if ( bResponseCommitted ) {
			xrtHttpdResponseUnit(&tResp);
			__xhttpdConnFinalizeMaybeAsync(pConn);
			return;
		}
	}
	if ( pServer && pReq && pServer->tEvents.OnRequestBodyEnd ) {
		bHandled = pServer->tEvents.OnRequestBodyEnd(pServer->pUserData, pServer, pConn, pReq, &tResp);
	}
	__xhttpdLock(&pConn->iConnLock);
	bResponseCommitted = pConn->bResponseCommitted;
	__xhttpdUnlock(&pConn->iConnLock);
	if ( bResponseCommitted ) {
		xrtHttpdResponseUnit(&tResp);
		__xhttpdConnFinalizeMaybeAsync(pConn);
		return;
	}
	if ( !bHandled ) {
		xrtHttpdResponseSetStatus(&tResp, 404u, NULL);
		(void)xrtHttpdResponseSetBodyCopy(&tResp, "Not Found", 9u, "text/plain");
	}
	if ( xrtHttpdConnRespond(pConn, &tResp) != XRT_NET_OK ) {
		__xhttpdEmitServerError(pServer, pConn, -1);
	}
	xrtHttpdResponseUnit(&tResp);
}


// 内部函数：继续处理定长 request body
static uint32 __xhttpdContinueFixedBodyStream(xhttpdconn* pConn, xnetchain* pChain)
{
	xnetspan arrSpans[8];
	uint64 iRemain;
	if ( !pConn || !pChain ) { return XHTTPD_BODY_STREAM_INTERNAL_ERROR; }
	for ( ;; ) {
		uint32 iSpanCount;
		size_t iTake;
		uint32 iStatus;
		__xhttpdLock(&pConn->iConnLock);
		iRemain = pConn->iBodyRemain;
		__xhttpdUnlock(&pConn->iConnLock);
		if ( iRemain == 0u ) {
			__xhttpdCompleteRequestBodyStream(pConn);
			return XHTTPD_BODY_STREAM_OK;
		}
		iSpanCount = xrtNetChainGetSpans(pChain, arrSpans, (uint32)(sizeof(arrSpans) / sizeof(arrSpans[0])));
		if ( iSpanCount == 0u ) { return XHTTPD_BODY_STREAM_OK; }
		iTake = arrSpans[0].iLen;
		if ( (uint64)iTake > iRemain ) { iTake = (size_t)iRemain; }
		iStatus = __xhttpdEmitRequestBodyChunk(pConn, arrSpans[0].pData, iTake);
		if ( iStatus != XHTTPD_BODY_STREAM_OK ) { return iStatus; }
		xrtNetChainConsume(pChain, iTake);
		__xhttpdLock(&pConn->iConnLock);
		if ( pConn->iBodyRemain >= (uint64)iTake ) { pConn->iBodyRemain -= (uint64)iTake; }
		else { pConn->iBodyRemain = 0u; }
		__xhttpdUnlock(&pConn->iConnLock);
	}
}


// 内部函数：继续处理 chunked request body
static uint32 __xhttpdContinueChunkedBodyStream(xhttpdconn* pConn, xnetchain* pChain)
{
	xcodechttp1limits tLimits;
	xhttpdserver* pServer;
	if ( !pConn || !pChain ) { return XHTTPD_BODY_STREAM_INTERNAL_ERROR; }
	__xhttpdLock(&pConn->iConnLock);
	pServer = pConn->pServer;
	__xhttpdUnlock(&pConn->iConnLock);
	if ( !pServer ) { return XHTTPD_BODY_STREAM_INTERNAL_ERROR; }
	__xhttpdResolveCodecLimits(pServer, &tLimits);
	for ( ;; ) {
		uint32 iState;
		__xhttpdLock(&pConn->iConnLock);
		iState = pConn->iChunkState;
		__xhttpdUnlock(&pConn->iConnLock);
		if ( iState == XHTTPD_CHUNK_STATE_SIZE ) {
			size_t iDataOffset = 0u;
			uint64 iChunkSize = 0u;
			xcodechttp1errorinfo tError;
			xcodecstatus eParse = __xcodecHttpReadChunkHeaderEx(
				pChain, 0u, &tLimits, &iChunkSize, &iDataOffset, NULL, &tError);
			if ( eParse == XCODEC_STATUS_NEED_MORE ) { return XHTTPD_BODY_STREAM_OK; }
			if ( eParse != XCODEC_STATUS_FRAME ) {
				return tError.eCode == XCODEC_HTTP1_ERROR_OUT_OF_MEMORY ?
					XHTTPD_BODY_STREAM_INTERNAL_ERROR : XHTTPD_BODY_STREAM_BAD_REQUEST;
			}
			__xhttpdLock(&pConn->iConnLock);
			if ( tLimits.iMaxBodyBytes < pConn->iBodyReceived ||
				iChunkSize > tLimits.iMaxBodyBytes - pConn->iBodyReceived ) {
				__xhttpdUnlock(&pConn->iConnLock);
				return XHTTPD_BODY_STREAM_TOO_LARGE;
			}
			__xhttpdUnlock(&pConn->iConnLock);
			xrtNetChainConsume(pChain, iDataOffset);
			__xhttpdLock(&pConn->iConnLock);
			pConn->iChunkRemain = iChunkSize;
			pConn->iChunkState = (iChunkSize == 0u) ? XHTTPD_CHUNK_STATE_TRAILER : XHTTPD_CHUNK_STATE_DATA;
			__xhttpdUnlock(&pConn->iConnLock);
		}
		else if ( iState == XHTTPD_CHUNK_STATE_DATA ) {
			xnetspan arrSpans[8];
			uint32 iSpanCount;
			uint64 iRemain;
			size_t iTake;
			uint32 iStatus;
			__xhttpdLock(&pConn->iConnLock);
			iRemain = pConn->iChunkRemain;
			__xhttpdUnlock(&pConn->iConnLock);
			if ( iRemain == 0u ) {
				__xhttpdLock(&pConn->iConnLock);
				pConn->iChunkState = XHTTPD_CHUNK_STATE_DATA_CRLF;
				__xhttpdUnlock(&pConn->iConnLock);
				continue;
			}
			iSpanCount = xrtNetChainGetSpans(pChain, arrSpans, (uint32)(sizeof(arrSpans) / sizeof(arrSpans[0])));
			if ( iSpanCount == 0u ) { return XHTTPD_BODY_STREAM_OK; }
			iTake = arrSpans[0].iLen;
			if ( (uint64)iTake > iRemain ) { iTake = (size_t)iRemain; }
			iStatus = __xhttpdEmitRequestBodyChunk(pConn, arrSpans[0].pData, iTake);
			if ( iStatus != XHTTPD_BODY_STREAM_OK ) { return iStatus; }
			xrtNetChainConsume(pChain, iTake);
			__xhttpdLock(&pConn->iConnLock);
			if ( pConn->iChunkRemain >= (uint64)iTake ) { pConn->iChunkRemain -= (uint64)iTake; }
			else { pConn->iChunkRemain = 0u; }
			if ( pConn->iChunkRemain == 0u ) { pConn->iChunkState = XHTTPD_CHUNK_STATE_DATA_CRLF; }
			__xhttpdUnlock(&pConn->iConnLock);
		}
		else if ( iState == XHTTPD_CHUNK_STATE_DATA_CRLF ) {
			char aCrlf[2];
			if ( xrtNetChainBytes(pChain) < 2u ) { return XHTTPD_BODY_STREAM_OK; }
			if ( xrtNetChainPeekAt(pChain, 0u, aCrlf, sizeof(aCrlf)) != sizeof(aCrlf) ||
				aCrlf[0] != '\r' || aCrlf[1] != '\n' ) {
				return XHTTPD_BODY_STREAM_BAD_REQUEST;
			}
			xrtNetChainConsume(pChain, 2u);
			__xhttpdLock(&pConn->iConnLock);
			pConn->iChunkState = XHTTPD_CHUNK_STATE_SIZE;
			__xhttpdUnlock(&pConn->iConnLock);
		}
		else if ( iState == XHTTPD_CHUNK_STATE_TRAILER ) {
			xcodechttp1msg tTrailers;
			xcodechttp1errorinfo tError;
			xcodecstatus eParse;
			size_t iConsumed = 0u;
			xhttpdrequest* pReq;
			xrtCodecHttp1MessageInit(&tTrailers);
			eParse = xrtCodecHttp1ParseTrailersEx(pChain, &iConsumed, &tTrailers, &tLimits, &tError);
			if ( eParse == XCODEC_STATUS_NEED_MORE ) {
				xrtCodecHttp1MessageUnit(&tTrailers);
				return XHTTPD_BODY_STREAM_OK;
			}
			if ( eParse != XCODEC_STATUS_FRAME ) {
				uint32 iStatus = __xhttpdProtocolStatus(&tError);
				xrtCodecHttp1MessageUnit(&tTrailers);
				return tError.eCode == XCODEC_HTTP1_ERROR_OUT_OF_MEMORY ?
					XHTTPD_BODY_STREAM_INTERNAL_ERROR : iStatus;
			}
			__xhttpdLock(&pConn->iConnLock);
			pReq = pConn->pRequest;
			if ( !pReq || !__xhttpdRequestEnsureTrailerCap(pReq, tTrailers.iTrailerCount) ) {
				__xhttpdUnlock(&pConn->iConnLock);
				xrtCodecHttp1MessageUnit(&tTrailers);
				return XHTTPD_BODY_STREAM_INTERNAL_ERROR;
			}
			for ( uint32 i = 0u; i < tTrailers.iTrailerCount; ++i ) {
				const xcodechttp1header* pTrailer = xrtCodecHttp1TrailerAt(&tTrailers, i);
				if ( !pTrailer || !__xhttpdHeaderSetCopyN(&pReq->pTrailers[pReq->iTrailerCount],
					pTrailer->sName, pTrailer->iNameLen, pTrailer->sValue, pTrailer->iValueLen) ) {
					__xhttpdUnlock(&pConn->iConnLock);
					xrtCodecHttp1MessageUnit(&tTrailers);
					return XHTTPD_BODY_STREAM_INTERNAL_ERROR;
				}
				pReq->iTrailerCount++;
			}
			__xhttpdUnlock(&pConn->iConnLock);
			xrtCodecHttp1MessageUnit(&tTrailers);
			xrtNetChainConsume(pChain, iConsumed);
			__xhttpdCompleteRequestBodyStream(pConn);
			return XHTTPD_BODY_STREAM_OK;
		}
		else {
			return XHTTPD_BODY_STREAM_INTERNAL_ERROR;
		}
	}
}


// 内部函数：继续处理 request body 流式状态机
static uint32 __xhttpdContinueRequestBodyStream(xhttpdconn* pConn, xnetchain* pChain)
{
	uint32 iMode;
	if ( !pConn || !pChain ) { return XHTTPD_BODY_STREAM_INTERNAL_ERROR; }
	__xhttpdLock(&pConn->iConnLock);
	iMode = pConn->iBodyMode;
	__xhttpdUnlock(&pConn->iConnLock);
	if ( iMode == XHTTPD_BODY_MODE_FIXED ) { return __xhttpdContinueFixedBodyStream(pConn, pChain); }
	if ( iMode == XHTTPD_BODY_MODE_CHUNKED ) { return __xhttpdContinueChunkedBodyStream(pConn, pChain); }
	return XHTTPD_BODY_STREAM_INTERNAL_ERROR;
}


// 内部函数：根据已完成头部启动 request body 流式状态机
static bool __xhttpdTryStartRequestBodyStream(xhttpdconn* pConn, xnetstream* pStream, xnetchain* pChain)
{
	xhttpdserver* pServer = pConn ? pConn->pServer : NULL;
	xcodecframe tFrame;
	xcodechttp1msg tMsg;
	xcodechttp1limits tLimits;
	xcodechttp1errorinfo tError;
	xcodecstatus iParse;
	xhttpdrequest* pReq = NULL;
	bool bChunked;
	bool bHasBody;
	uint32 iStreamStatus;
	if ( !pConn || !pServer || !pStream || !pChain || !__xhttpdHasRequestBodyStream(pServer) ) { return false; }
	__xhttpdResolveCodecLimits(pServer, &tLimits);
	iParse = xrtCodecHttp1ParseHeadEx(pChain, &tFrame, &tMsg, &tLimits, &tError);
	if ( iParse == XCODEC_STATUS_NEED_MORE ) { return false; }
	if ( iParse == XCODEC_STATUS_ERROR ) {
		uint32 iStatus = __xhttpdProtocolStatus(&tError);
		__xhttpdConnCancelTimer(pConn);
		__xhttpdEmitServerError(pServer, pConn, -1);
		__xhttpdSendSimpleStatus(pConn, iStatus, __xhttpdStatusBody(iStatus));
		return true;
	}
	if ( !__xhttpdValidateRequestContract(&tMsg) ) {
		xrtCodecHttp1MessageUnit(&tMsg);
		__xhttpdConnCancelTimer(pConn);
		__xhttpdSendSimpleStatus(pConn, 400u, __xhttpdStatusBody(400u));
		return true;
	}
	bChunked = (tMsg.iFlags & XCODEC_HTTP1_F_CHUNKED) != 0u;
	bHasBody = bChunked || tMsg.iContentLength > 0;
	if ( !bHasBody ) {
		xrtCodecHttp1MessageUnit(&tMsg);
		return false;
	}
	if ( (pServer->tConfig.iHeaderLimit > 0u && tMsg.iHeaderCount > pServer->tConfig.iHeaderLimit) ||
		(pServer->tConfig.iHeaderBytesLimit > 0u && tFrame.iHeaderBytes > pServer->tConfig.iHeaderBytesLimit) ) {
		xrtCodecHttp1MessageUnit(&tMsg);
		__xhttpdConnCancelTimer(pConn);
		__xhttpdSendSimpleStatus(pConn, 431u, __xhttpdStatusBody(431u));
		return true;
	}
	if ( !bChunked && pServer->tConfig.iBodyLimit > 0u &&
		tMsg.iContentLength > (int64_t)pServer->tConfig.iBodyLimit ) {
		xrtCodecHttp1MessageUnit(&tMsg);
		__xhttpdConnCancelTimer(pConn);
		__xhttpdSendSimpleStatus(pConn, 413u, __xhttpdStatusBody(413u));
		return true;
	}
	pReq = __xhttpdRequestCreate();
	if ( !pReq ) {
		xrtCodecHttp1MessageUnit(&tMsg);
		__xhttpdEmitServerError(pServer, pConn, -1);
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return true;
	}
	if ( !__xhttpdBuildRequestHead(&tFrame, &tMsg, pChain, pReq, pServer->tConfig.iBodyLimit) ) {
		xrtCodecHttp1MessageUnit(&tMsg);
		__xhttpdRequestDestroy(pReq);
		__xhttpdEmitServerError(pServer, pConn, -1);
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return true;
	}
	pReq->iFlags |= XHTTPD_REQ_F_STREAMING;
	if ( pServer->tEvents.OnRequestBodyBegin &&
		!pServer->tEvents.OnRequestBodyBegin(pServer->pUserData, pServer, pConn, pReq) ) {
		xrtCodecHttp1MessageUnit(&tMsg);
		__xhttpdRequestDestroy(pReq);
		return false;
	}
	xrtNetChainConsume(pChain, tFrame.iHeaderBytes);
	xrtCodecHttp1MessageUnit(&tMsg);

	__xhttpdLock(&pConn->iConnLock);
	if ( __xhttpdAtomicLoad(&pConn->iCleanupPosted) != 0 || pConn->pRequest != NULL || pConn->bResponseInFlight ) {
		__xhttpdUnlock(&pConn->iConnLock);
		__xhttpdRequestDestroy(pReq);
		return true;
	}
	pConn->pRequest = pReq;
	pConn->bResponseInFlight = true;
	pConn->bResponseCommitted = false;
	pConn->bResponseDrained = false;
	pConn->bResponseStreaming = false;
	pConn->bResponseChunked = false;
	pConn->bRequestStreaming = true;
	pConn->bAsyncPending = false;
	pConn->bKeepAlive = false;
	pConn->iBodyMode = bChunked ? XHTTPD_BODY_MODE_CHUNKED : XHTTPD_BODY_MODE_FIXED;
	pConn->iChunkState = XHTTPD_CHUNK_STATE_SIZE;
	pConn->iBodyRemain = bChunked ? 0u : (uint64)pReq->iContentLength;
	pConn->iBodyReceived = 0u;
	pConn->iChunkRemain = 0u;
	__xhttpdUnlock(&pConn->iConnLock);

	__xhttpdConnArmTimer(pConn, XHTTPD_CONN_TIMER_BODY, true);
	iStreamStatus = __xhttpdContinueRequestBodyStream(pConn, pChain);
	if ( iStreamStatus != XHTTPD_BODY_STREAM_OK ) {
		__xhttpdBodyStreamFail(pConn, iStreamStatus, __xhttpdStatusBody(iStreamStatus));
	}
	return true;
}


// 内部函数：__xhttpdStreamOnRecv
static void __xhttpdStreamOnRecv(ptr pOwner, xnetstream* pStream, xnetchain* pChain)
{
	xhttpdconn* pConn = (xhttpdconn*)pOwner;
	xhttpdserver* pServer = pConn ? pConn->pServer : NULL;
	xcodecframe tFrame;
	xcodechttp1msg tMsg;
	xcodechttp1limits tLimits;
	xcodechttp1errorinfo tError;
	xcodecstatus iParse;
	xhttpdrequest* pReq = NULL;
	xhttpdresponse tResp;
	xfuture* pAsync = NULL;
	bool bHandled = false;
	bool bResponseCommitted = false;
	bool bRequestStreaming = false;
	// 参数校验与连接状态检查
	if ( !pConn || !pServer || !pStream || !pChain ) { return; }
	if ( __xhttpdAtomicLoad(&pConn->iCleanupPosted) != 0 ) { return; }
	// 检查是否有正在处理的响应
	__xhttpdLock(&pConn->iConnLock);
	bRequestStreaming = pConn->bRequestStreaming;
	if ( pConn->bResponseInFlight && !bRequestStreaming ) {
		__xhttpdUnlock(&pConn->iConnLock);
		return;
	}
	__xhttpdUnlock(&pConn->iConnLock);
	if ( bRequestStreaming ) {
		uint32 iStreamStatus = __xhttpdContinueRequestBodyStream(pConn, pChain);
		if ( iStreamStatus != XHTTPD_BODY_STREAM_OK ) {
			__xhttpdBodyStreamFail(pConn, iStreamStatus, __xhttpdStatusBody(iStreamStatus));
		}
		return;
	}
	// 解析 HTTP/1.1 请求
	__xhttpdConnCancelTimerKind(pConn, XHTTPD_CONN_TIMER_IDLE);
	if ( __xhttpdTryStartRequestBodyStream(pConn, pStream, pChain) ) { return; }
	__xhttpdResolveCodecLimits(pServer, &tLimits);
	iParse = xrtCodecHttp1ParseEx(pChain, &tFrame, &tMsg, &tLimits, &tError);
	if ( iParse == XCODEC_STATUS_NEED_MORE ) {
		xcodecframe tHeadFrame;
		xcodechttp1msg tHeadMsg;
		xcodechttp1errorinfo tHeadError;
		xcodecstatus iHeadParse = xrtCodecHttp1ParseHeadEx(pChain, &tHeadFrame, &tHeadMsg, &tLimits, &tHeadError);
		if ( iHeadParse == XCODEC_STATUS_ERROR ) {
			uint32 iStatus = __xhttpdProtocolStatus(&tHeadError);
			__xhttpdConnCancelTimer(pConn);
			__xhttpdSendSimpleStatus(pConn, iStatus, __xhttpdStatusBody(iStatus));
			return;
		}
		if ( iHeadParse == XCODEC_STATUS_FRAME ) {
			uint32 iExpectStatus;
			if ( !__xhttpdValidateRequestContract(&tHeadMsg) ) {
				xrtCodecHttp1MessageUnit(&tHeadMsg);
				__xhttpdConnCancelTimer(pConn);
				__xhttpdSendSimpleStatus(pConn, 400u, __xhttpdStatusBody(400u));
				return;
			}
			iExpectStatus = __xhttpdMaybeSendContinue(pConn, pStream, &tHeadMsg);
			if ( iExpectStatus == 417u ) {
				xrtCodecHttp1MessageUnit(&tHeadMsg);
				__xhttpdConnCancelTimer(pConn);
				__xhttpdSendSimpleStatus(pConn, 417u, __xhttpdStatusBody(417u));
				return;
			}
			__xhttpdConnArmTimer(pConn, XHTTPD_CONN_TIMER_BODY, false);
			xrtCodecHttp1MessageUnit(&tHeadMsg);
		}
		else {
			__xhttpdConnArmTimer(pConn, XHTTPD_CONN_TIMER_HEADER, false);
		}
		return;
	}
	// 解析失败，返回 400 错误
	if ( iParse == XCODEC_STATUS_ERROR ) {
		uint32 iStatus = __xhttpdProtocolStatus(&tError);
		__xhttpdConnCancelTimer(pConn);
		__xhttpdEmitServerError(pServer, pConn, -1);
		__xhttpdSendSimpleStatus(pConn, iStatus, __xhttpdStatusBody(iStatus));
		return;
	}
	if ( !__xhttpdValidateRequestContract(&tMsg) ) {
		xrtCodecHttp1MessageUnit(&tMsg);
		__xhttpdConnCancelTimer(pConn);
		__xhttpdSendSimpleStatus(pConn, 400u, __xhttpdStatusBody(400u));
		return;
	}
	if ( pServer->tConfig.iBodyLimit > 0u && xrtCodecHttp1BodyBytes(&tFrame) > pServer->tConfig.iBodyLimit ) {
		__xhttpdConnCancelTimer(pConn);
		__xhttpdSendSimpleStatus(pConn, 413u, __xhttpdStatusBody(413u));
		xrtCodecFrameConsume(pChain, &tFrame);
		xrtCodecHttp1MessageUnit(&tMsg);
		return;
	}
	// 创建并构建请求对象
	if ( (pServer->tConfig.iHeaderLimit > 0u && tMsg.iHeaderCount > pServer->tConfig.iHeaderLimit) ||
		(pServer->tConfig.iHeaderBytesLimit > 0u && tFrame.iHeaderBytes > pServer->tConfig.iHeaderBytesLimit) ) {
		__xhttpdConnCancelTimer(pConn);
		__xhttpdSendSimpleStatus(pConn, 431u, __xhttpdStatusBody(431u));
		xrtCodecFrameConsume(pChain, &tFrame);
		xrtCodecHttp1MessageUnit(&tMsg);
		return;
	}
	__xhttpdConnCancelTimer(pConn);
	pReq = __xhttpdRequestCreate();
	if ( !pReq ) {
		xrtCodecHttp1MessageUnit(&tMsg);
		__xhttpdEmitServerError(pServer, pConn, -1);
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return;
	}
	if ( !__xhttpdBuildRequest(&tFrame, &tMsg, pChain, pReq, pServer->tConfig.iBodyLimit) ) {
		xrtCodecHttp1MessageUnit(&tMsg);
		__xhttpdRequestDestroy(pReq);
		__xhttpdEmitServerError(pServer, pConn, -1);
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return;
	}
	// 消费已解析的帧数据
	xrtCodecFrameConsume(pChain, &tFrame);
	xrtCodecHttp1MessageUnit(&tMsg);

	// 将请求绑定到连接，标记响应进行中
	__xhttpdLock(&pConn->iConnLock);
	if ( __xhttpdAtomicLoad(&pConn->iCleanupPosted) != 0 || pConn->pRequest != NULL || pConn->bResponseInFlight ) {
		__xhttpdUnlock(&pConn->iConnLock);
		__xhttpdRequestDestroy(pReq);
		return;
	}
	pConn->pRequest = pReq;
	pConn->bResponseInFlight = true;
	pConn->bResponseCommitted = false;
	pConn->bResponseDrained = false;
	pConn->bResponseStreaming = false;
	pConn->bResponseChunked = false;
	pConn->bAsyncPending = false;
	pConn->bKeepAlive = false;
	__xhttpdUnlock(&pConn->iConnLock);

	// 优先尝试异步请求处理
	if ( pServer->tEvents.OnRequestAsync ) {
		__xhttpdLock(&pConn->iConnLock);
		pConn->bAsyncPending = true;
		__xhttpdUnlock(&pConn->iConnLock);
		pAsync = pServer->tEvents.OnRequestAsync(pServer->pUserData, pServer, pConn, pReq);
		if ( pAsync ) {
			// 附加异步完成回调
			if ( !__xhttpdAttachAsyncRequest(pConn, pAsync) ) {
				__xhttpdLock(&pConn->iConnLock);
				pConn->bAsyncPending = false;
				__xhttpdUnlock(&pConn->iConnLock);
				__xhttpdEmitServerError(pServer, pConn, -1);
				__xhttpdSendSimpleStatus(pConn, 500u, __xhttpdStatusBody(500u));
			}
			return;
		}
		// 异步处理器返回 NULL，回退到同步处理
		__xhttpdLock(&pConn->iConnLock);
		pConn->bAsyncPending = false;
		bResponseCommitted = pConn->bResponseCommitted;
		__xhttpdUnlock(&pConn->iConnLock);
		if ( bResponseCommitted ) {
			__xhttpdConnFinalizeMaybeAsync(pConn);
			return;
		}
	}

	// 同步请求处理
	xrtHttpdResponseInit(&tResp);
	if ( pServer->tEvents.OnRequest ) {
		bHandled = pServer->tEvents.OnRequest(pServer->pUserData, pServer, pConn, pReq, &tResp);
	}
	__xhttpdLock(&pConn->iConnLock);
	bResponseCommitted = pConn->bResponseCommitted;
	__xhttpdUnlock(&pConn->iConnLock);
	if ( bResponseCommitted ) {
		xrtHttpdResponseUnit(&tResp);
		__xhttpdConnFinalizeMaybeAsync(pConn);
		return;
	}
	// 未处理时返回 404
	if ( !bHandled ) {
		xrtHttpdResponseSetStatus(&tResp, 404u, NULL);
		(void)xrtHttpdResponseSetBodyCopy(&tResp, "Not Found", 9, "text/plain");
	}
	// 发送响应
	if ( xrtHttpdConnRespond(pConn, &tResp) != XRT_NET_OK ) {
		__xhttpdEmitServerError(pServer, pConn, -1);
	}
	xrtHttpdResponseUnit(&tResp);
}


// 内部函数：__xhttpdStreamOnDrain
static void __xhttpdStreamOnDrain(ptr pOwner, xnetstream* pStream)
{
	xhttpdconn* pConn = (xhttpdconn*)pOwner;
	bool bShouldFinalize = false;
	if ( !pConn || !pStream ) { return; }
	if ( __xhttpdAtomicLoad(&pConn->iCleanupPosted) != 0 ) { return; }
	__xhttpdLock(&pConn->iConnLock);
	if ( pConn->bResponseInFlight && pConn->bResponseCommitted ) {
		pConn->bResponseDrained = true;
		if ( pConn->bKeepAlive && !pStream->bClosing ) {
			bShouldFinalize = true;
		}
	}
	else {
		__xhttpdUnlock(&pConn->iConnLock);
		return;
	}
	__xhttpdUnlock(&pConn->iConnLock);
	__xhttpdConnCancelTimerKind(pConn, XHTTPD_CONN_TIMER_WRITE);
	if ( bShouldFinalize ) {
		__xhttpdConnFinalizeMaybeAsync(pConn);
	}
}


// 内部函数：__xhttpdStreamOnClose
static void __xhttpdStreamOnClose(ptr pOwner, xnetstream* pStream, xnet_result iReason)
{
	xhttpdconn* pConn = (xhttpdconn*)pOwner;
	#if defined(XNET_DEBUG_CLOSE_DIAG)
		fprintf(stderr, "[CLOSE_DIAG][HTTPD] close conn=%p stream=%p reason=%d keepalive=%d inflight=%d\n",
			(void*)pConn,
			(void*)pStream,
			(int)iReason,
			pConn ? (pConn->bKeepAlive ? 1 : 0) : 0,
			pConn ? (pConn->bResponseInFlight ? 1 : 0) : 0);
	#endif
	(void)pStream;
	__xhttpdConnNotifyClose(pConn, iReason);
	__xhttpdConnPostCleanup(pConn);
}


// 内部函数：处理流错误回调
static void __xhttpdStreamOnError(ptr pOwner, xnetstream* pStream, int iSysErr)
{
	xhttpdconn* pConn = (xhttpdconn*)pOwner;
	xhttpdserver* pServer = pConn ? pConn->pServer : NULL;
	if ( __xhttpdIsBenignStreamError(pConn, pStream, iSysErr) ) { return; }
	__xhttpdEmitServerError(pServer, pConn, iSysErr);
}


// 内部函数：__xhttpdAcceptReady
static void __xhttpdAcceptReady(xnetlistener* pListener, xnet_result iStatus, xnetstream* pStream, ptr pCtx)
{
	xhttpdserver* pServer = (xhttpdserver*)pCtx;
	(void)pListener;
	(void)pStream;
	if ( !pServer || __xhttpdAtomicLoad(&pServer->bRunning) == 0 ) { return; }
	if ( pServer->pListener && pServer->pListener->bRunning ) {
		(void)__xnetListenerRegisterSyncAcceptWait(pServer->pListener, __xhttpdAcceptReady, NULL, pServer);
	}
	if ( iStatus != XRT_NET_OK && pServer->tEvents.OnError ) {
		pServer->tEvents.OnError(pServer->pUserData, pServer, NULL, -1);
	}
}


// 内部函数：__xhttpdArmAcceptTask
static void __xhttpdArmAcceptTask(xnetworker* pWorker, ptr pArg)
{
	xhttpdserver* pServer = (xhttpdserver*)pArg;
	(void)pWorker;
	if ( !pServer || !pServer->pListener || __xhttpdAtomicLoad(&pServer->bRunning) == 0 ) { return; }
	(void)__xnetListenerRegisterSyncAcceptWait(pServer->pListener, __xhttpdAcceptReady, NULL, pServer);
}


// 内部函数：__xhttpdListenerEvents
static const xnetlistenerevents* __xhttpdListenerEvents(void)
{
	static const xnetlistenerevents tEvents = {
		__xhttpdListenerOnAccept,
		NULL
	};
	return &tEvents;
}


// 内部函数：__xhttpdStreamEvents
static const xnetstreamevents* __xhttpdStreamEvents(void)
{
	static const xnetstreamevents tEvents = {
		__xhttpdStreamOnOpen,
		__xhttpdStreamOnRecv,
		__xhttpdStreamOnDrain,
		__xhttpdStreamOnClose,
		__xhttpdStreamOnError,
		NULL,
		NULL,
		__xhttpdStreamOnTimer
	};
	return &tEvents;
}


// 内部函数：释放 HTTPD TLS 配置副本
static void __xhttpdServerClearTlsConfig(xhttpdserver* pServer)
{
	if ( !pServer ) { return; }
	if ( pServer->sTlsCertFile ) { XNET_FREE(pServer->sTlsCertFile); }
	if ( pServer->sTlsKeyFile ) { XNET_FREE(pServer->sTlsKeyFile); }
	if ( pServer->sTlsCaFile ) { XNET_FREE(pServer->sTlsCaFile); }
	if ( pServer->sTlsCrlFile ) { XNET_FREE(pServer->sTlsCrlFile); }
	if ( pServer->sTlsHostName ) { XNET_FREE(pServer->sTlsHostName); }
	if ( pServer->pTlsCertData ) { XNET_FREE(pServer->pTlsCertData); }
	if ( pServer->pTlsKeyData ) { XNET_FREE(pServer->pTlsKeyData); }
	if ( pServer->pTlsCaData ) { XNET_FREE(pServer->pTlsCaData); }
	if ( pServer->pTlsCrlData ) { XNET_FREE(pServer->pTlsCrlData); }
	if ( pServer->pTlsResume ) {
		memset(pServer->pTlsResume, 0, sizeof(*pServer->pTlsResume));
		XNET_FREE(pServer->pTlsResume);
	}
	pServer->sTlsCertFile = NULL;
	pServer->sTlsKeyFile = NULL;
	pServer->sTlsCaFile = NULL;
	pServer->sTlsCrlFile = NULL;
	pServer->sTlsHostName = NULL;
	pServer->pTlsCertData = NULL;
	pServer->pTlsKeyData = NULL;
	pServer->pTlsCaData = NULL;
	pServer->pTlsCrlData = NULL;
	pServer->pTlsResume = NULL;
	memset(&pServer->tTlsConfig, 0, sizeof(pServer->tTlsConfig));
	pServer->bHasTlsConfig = false;
	if ( pServer->tConfig.pTlsConfig == &pServer->tTlsConfig ) { pServer->tConfig.pTlsConfig = NULL; }
}


// 内部函数：复制 HTTPD TLS 配置
static bool __xhttpdServerCopyTlsConfig(xhttpdserver* pServer, const xtlsconfig* pSrc)
{
	if ( !pServer || !pSrc ) { return true; }
	pServer->tTlsConfig = *pSrc;
	pServer->tTlsConfig.iDataLock = 0;
	if ( pSrc->sCertFile && !(pServer->sTlsCertFile = __xhttpdDupStr(pSrc->sCertFile)) ) { goto fail; }
	if ( pSrc->sKeyFile && !(pServer->sTlsKeyFile = __xhttpdDupStr(pSrc->sKeyFile)) ) { goto fail; }
	if ( pSrc->sCaFile && !(pServer->sTlsCaFile = __xhttpdDupStr(pSrc->sCaFile)) ) { goto fail; }
	if ( pSrc->sCrlFile && !(pServer->sTlsCrlFile = __xhttpdDupStr(pSrc->sCrlFile)) ) { goto fail; }
	if ( pSrc->sHostName && !(pServer->sTlsHostName = __xhttpdDupStr(pSrc->sHostName)) ) { goto fail; }
	if ( pSrc->pCertData && pSrc->iCertDataLen > 0u && !(pServer->pTlsCertData = __xhttpdDupBytes(pSrc->pCertData, pSrc->iCertDataLen)) ) { goto fail; }
	if ( pSrc->pKeyData && pSrc->iKeyDataLen > 0u && !(pServer->pTlsKeyData = __xhttpdDupBytes(pSrc->pKeyData, pSrc->iKeyDataLen)) ) { goto fail; }
	if ( pSrc->pCaData && pSrc->iCaDataLen > 0u && !(pServer->pTlsCaData = __xhttpdDupBytes(pSrc->pCaData, pSrc->iCaDataLen)) ) { goto fail; }
	if ( pSrc->pCrlData && pSrc->iCrlDataLen > 0u && !(pServer->pTlsCrlData = __xhttpdDupBytes(pSrc->pCrlData, pSrc->iCrlDataLen)) ) { goto fail; }
	if ( pSrc->pResume ) {
		pServer->pTlsResume = (xtlsresume*)XNET_ALLOC(sizeof(*pServer->pTlsResume));
		if ( !pServer->pTlsResume ) { goto fail; }
		memcpy(pServer->pTlsResume, pSrc->pResume, sizeof(*pServer->pTlsResume));
	}
	pServer->tTlsConfig.sCertFile = pServer->sTlsCertFile;
	pServer->tTlsConfig.sKeyFile = pServer->sTlsKeyFile;
	pServer->tTlsConfig.sCaFile = pServer->sTlsCaFile;
	pServer->tTlsConfig.sCrlFile = pServer->sTlsCrlFile;
	pServer->tTlsConfig.sHostName = pServer->sTlsHostName;
	pServer->tTlsConfig.pCertData = pServer->pTlsCertData;
	pServer->tTlsConfig.pKeyData = pServer->pTlsKeyData;
	pServer->tTlsConfig.pCaData = pServer->pTlsCaData;
	pServer->tTlsConfig.pCrlData = pServer->pTlsCrlData;
	pServer->tTlsConfig.pResume = pServer->pTlsResume;
	pServer->tConfig.pTlsConfig = &pServer->tTlsConfig;
	pServer->bHasTlsConfig = true;
	return true;

fail:
	__xhttpdServerClearTlsConfig(pServer);
	return false;
}


// 创建 HTTP 服务端
XXAPI xhttpdserver* xrtHttpdCreate(xnetengine* pEngine, const xhttpdconfig* pCfg, const xhttpdevents* pEvents, ptr pUserData)
{
	xhttpdserver* pServer;
	const xtlsconfig* pTlsConfig;
	if ( !pEngine ) { return NULL; }
	pServer = (xhttpdserver*)XNET_ALLOC(sizeof(xhttpdserver));
	if ( !pServer ) { return NULL; }
	memset(pServer, 0, sizeof(xhttpdserver));
	pServer->iRefCount = 1;
	pServer->pEngine = pEngine;
	if ( !__xhttpdConfigCopy(&pServer->tConfig, pCfg) ||
		!__xhttpdEventsCopy(&pServer->tEvents, pEvents) ) {
		XNET_FREE(pServer);
		return NULL;
	}
	pTlsConfig = pServer->tConfig.pTlsConfig;
	pServer->tConfig.pTlsConfig = NULL;
	if ( pTlsConfig && !__xhttpdServerCopyTlsConfig(pServer, pTlsConfig) ) {
		XNET_FREE(pServer);
		return NULL;
	}
	pServer->pUserData = pUserData;
	return pServer;
}


// 获取 HTTP 服务端 bound 端口
XXAPI uint16 xrtHttpdBoundPort(const xhttpdserver* pServer)
{
	return (pServer && pServer->pListener) ? pServer->pListener->tConfig.tBindAddr.iPort : 0u;
}


// 获取当前连接数量
XXAPI uint32 xrtHttpdConnectionCount(xhttpdserver* pServer)
{
	uint32 iCount = 0u;
	xhttpdconn* pCur;
	if ( !pServer ) { return 0u; }
	__xhttpdLock(&pServer->iConnLock);
	for ( pCur = pServer->pConnHead; pCur; pCur = pCur->pNext ) {
		iCount++;
	}
	__xhttpdUnlock(&pServer->iConnLock);
	return iCount;
}


// 启动 HTTP 服务端
XXAPI xnet_result xrtHttpdStart(xhttpdserver* pServer)
{
	xnetlistenconfig tListenCfg;
	// 参数校验与重复启动检查
	if ( !pServer || !pServer->pEngine ) { return XRT_NET_ERROR; }
	if ( __xhttpdAtomicLoad(&pServer->bRunning) != 0 ) { return XRT_NET_OK; }
	// 初始化监听配置
	xrtNetListenConfigInit(&tListenCfg);
	tListenCfg.tBindAddr = pServer->tConfig.tBindAddr;
	tListenCfg.iFlags = pServer->tConfig.iFlags;
	tListenCfg.iBacklog = pServer->tConfig.iBacklog;
	tListenCfg.iRecvLimit = pServer->tConfig.iRecvLimit;
	tListenCfg.pTlsConfig = pServer->tConfig.pTlsConfig;
	// 创建监听器
	pServer->pListener = xrtNetListenerCreate(pServer->pEngine, &tListenCfg, __xhttpdListenerEvents(), __xhttpdStreamEvents(), pServer);
	if ( !pServer->pListener ) { return XRT_NET_ERROR; }
	// 启动监听器
	if ( xrtNetListenerStart(pServer->pListener) != XRT_NET_OK ) {
		xrtNetListenerDestroy(pServer->pListener);
		pServer->pListener = NULL;
		return XRT_NET_ERROR;
	}
	// 标记服务为运行状态
	(void)__xhttpdAtomicCompareExchange(&pServer->bDraining, 0, 1);
	(void)__xhttpdAtomicCompareExchange(&pServer->bRunning, 1, 0);
	// 投递接受连接任务到监听器工作线程
	if ( xrtNetEnginePost(pServer->pEngine, pServer->pListener->pWorker->iId, __xhttpdArmAcceptTask, pServer) != XRT_NET_OK ) {
		(void)__xhttpdAtomicCompareExchange(&pServer->bRunning, 0, 1);
		xrtNetListenerStop(pServer->pListener);
		xrtNetListenerDestroy(pServer->pListener);
		pServer->pListener = NULL;
		return XRT_NET_ERROR;
	}
	return XRT_NET_OK;
}


// 内部函数：停止 HTTP 监听器
static void __xhttpdServerStopListener(xhttpdserver* pServer)
{
    if ( !pServer ) { return; }
    if ( pServer->pListener ) {
        xrtNetListenerStop(pServer->pListener);
        xrtNetListenerDestroy(pServer->pListener);
        pServer->pListener = NULL;
    }
}


// 内部函数：强制关闭并清理所有 HTTP 连接
static void __xhttpdServerAbortAllConns(xhttpdserver* pServer)
{
	xhttpdconn* pConn;
	if ( !pServer ) { return; }
	pConn = __xhttpdServerDetachAllConns(pServer);
    while ( pConn ) {
        xhttpdconn* pNext = pConn->pNext;
		bool bUpgraded;
        pConn->pNext = NULL;
		__xhttpdLock(&pConn->iConnLock);
		bUpgraded = pConn->bUpgraded;
		__xhttpdUnlock(&pConn->iConnLock);
		if ( bUpgraded ) {
			__xhttpdConnRelease(pConn);
			pConn = pNext;
			continue;
		}
		__xhttpdConnNotifyClose(pConn, XRT_NET_CANCELLED);
        if ( __xhttpdAtomicCompareExchange(&pConn->iCleanupPosted, 1, 0) == 0 ) {
            xnetstream* pStream;
            __xhttpdLock(&pConn->iConnLock);
            pStream = pConn->pStream;
            if ( pStream ) { __xnetStreamAddAsyncHold(pStream); }
            __xhttpdUnlock(&pConn->iConnLock);
            if ( pStream ) {
                xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
                __xnetStreamReleaseAsyncHold(pStream);
            }
            __xhttpdConnCleanupTask(NULL, pConn);
        }
        else {
            __xhttpdLock(&pConn->iConnLock);
            if ( pConn->pServer == pServer ) {
                pConn->pServer = NULL;
            }
            __xhttpdUnlock(&pConn->iConnLock);
        }
        __xhttpdConnRelease(pConn);
        pConn = pNext;
	}
}


// 内部函数：优雅关闭空闲 keep-alive 连接
static void __xhttpdServerCloseIdleConns(xhttpdserver* pServer)
{
	xhttpdconn** arrConns;
	xhttpdconn* pCur;
	uint32 iCap;
	uint32 iCount = 0u;
	uint32 i;
	if ( !pServer ) { return; }
	iCap = xrtHttpdConnectionCount(pServer);
	if ( iCap == 0u ) { return; }
	arrConns = (xhttpdconn**)XNET_ALLOC(sizeof(xhttpdconn*) * iCap);
	if ( !arrConns ) { return; }
	__xhttpdLock(&pServer->iConnLock);
	for ( pCur = pServer->pConnHead; pCur && iCount < iCap; pCur = pCur->pNext ) {
		arrConns[iCount++] = __xhttpdConnAddRef(pCur);
	}
	__xhttpdUnlock(&pServer->iConnLock);
	for ( i = 0u; i < iCount; ++i ) {
		xnetstream* pStream = NULL;
		xhttpdconn* pConn = arrConns[i];
		__xhttpdLock(&pConn->iConnLock);
		if ( pConn->pStream && !pConn->pStream->bClosing &&
			!pConn->bResponseInFlight &&
			__xhttpdAtomicLoad(&pConn->iCleanupPosted) == 0 ) {
			pStream = pConn->pStream;
			__xnetStreamAddAsyncHold(pStream);
		}
		__xhttpdUnlock(&pConn->iConnLock);
		if ( pStream ) {
			xrtNetStreamClose(pStream, XNET_CLOSE_F_GRACEFUL);
			__xnetStreamReleaseAsyncHold(pStream);
		}
		__xhttpdConnRelease(pConn);
	}
	XNET_FREE(arrConns);
}


// 优雅 drain：停止监听，等待已有连接自然结束
XXAPI bool xrtHttpdDrain(xhttpdserver* pServer, uint32 iTimeoutMs)
{
    double fStart;
    bool bInfinite;
    if ( !pServer ) { return true; }
	(void)__xhttpdAtomicCompareExchange(&pServer->bRunning, 0, 1);
	(void)__xhttpdAtomicCompareExchange(&pServer->bDraining, 1, 0);
	__xhttpdServerStopListener(pServer);
	__xhttpdServerCloseIdleConns(pServer);
	if ( xrtHttpdConnectionCount(pServer) == 0u ) { return true; }
    if ( iTimeoutMs == 0u ) { return false; }
    bInfinite = iTimeoutMs == UINT32_MAX;
    fStart = xrtTimer();
    while ( xrtHttpdConnectionCount(pServer) > 0u ) {
        if ( !bInfinite && (uint32)((xrtTimer() - fStart) * 1000.0) >= iTimeoutMs ) {
            return false;
        }
        xrtSleep(10u);
    }
    return true;
}


// 停止 HTTP 服务端：停止监听并强制关闭所有连接
XXAPI void xrtHttpdStop(xhttpdserver* pServer)
{
    if ( !pServer ) { return; }
    (void)__xhttpdAtomicCompareExchange(&pServer->bRunning, 0, 1);
    (void)__xhttpdAtomicCompareExchange(&pServer->bDraining, 1, 0);
    __xhttpdServerStopListener(pServer);
    __xhttpdServerAbortAllConns(pServer);
}


// 销毁 HTTP 服务端
XXAPI void xrtHttpdDestroy(xhttpdserver* pServer)
{
	if ( !pServer ) { return; }
	xrtHttpdStop(pServer);
	__xhttpdServerRelease(pServer);
}

#endif
