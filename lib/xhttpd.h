#ifndef XRT_XHTTPD_H
#define XRT_XHTTPD_H

/*
	XRT mainline HTTP/1.1 server on top of xnet.

	This header provides:
	  - listener-driven HTTP server lifecycle on top of xnet_stream
	  - request and response materialization helpers
	  - plain HTTP and builtin TLS service paths
	  - serial keep-alive reuse on a single accepted connection

	Current limitations:
	  - chunked request and response bodies are whole-message oriented
	  - static files, routing tables, and generic upgrade dispatch are deferred
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

#define XHTTPD_REQ_F_NONE         0x00000000u
#define XHTTPD_REQ_F_KEEPALIVE    0x00000001u
#define XHTTPD_REQ_F_CHUNKED      0x00000002u
#define XHTTPD_REQ_F_UPGRADE      0x00000004u

#define XHTTPD_RESP_F_NONE        0x00000000u
#define XHTTPD_RESP_F_CLOSE       0x00000001u

typedef enum {
	XHTTPD_METHOD_UNKNOWN = 0,
	XHTTPD_METHOD_GET = 1,
	XHTTPD_METHOD_HEAD = 2,
	XHTTPD_METHOD_POST = 3,
	XHTTPD_METHOD_PUT = 4,
	XHTTPD_METHOD_DELETE = 5,
	XHTTPD_METHOD_PATCH = 6,
	XHTTPD_METHOD_OPTIONS = 7
} xhttpdmethod;

typedef struct xrt_httpd_server xhttpdserver;
typedef struct xrt_httpd_conn xhttpdconn;

typedef struct {
	char sName[XHTTPD_HEADER_NAME_CAP];
	char sValue[XHTTPD_HEADER_VALUE_CAP];
} xhttpdheader;

typedef struct {
	uint32 iFlags;
	uint32 iHeaderCount;
	uint32 iMethod;
	int64_t iContentLength;
	char sMethod[XHTTPD_METHOD_CAP];
	char sTarget[XHTTPD_TARGET_CAP];
	char sPath[XHTTPD_PATH_CAP];
	char sQuery[XHTTPD_QUERY_CAP];
	char sVersion[XHTTPD_VERSION_CAP];
	xhttpdheader arrHeaders[XHTTPD_MAX_HEADERS];
	char* pBody;
	size_t iBodyLen;
} xhttpdrequest;

typedef struct {
	uint32 iStatusCode;
	uint32 iFlags;
	uint32 iHeaderCount;
	char sReason[XHTTPD_REASON_CAP];
	xhttpdheader arrHeaders[XHTTPD_MAX_HEADERS];
	char* pBody;
	size_t iBodyLen;
} xhttpdresponse;

typedef struct {
	xnetaddr tBindAddr;
	uint32 iFlags;
	uint32 iBacklog;
	uint32 iRecvLimit;
	uint32 iBodyLimit;
	const xtlsconfig* pTlsConfig;
} xhttpdconfig;

typedef struct {
	void (*OnOpen)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn);
	bool (*OnRequest)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq, xhttpdresponse* pResp);
	xfuture* (*OnRequestAsync)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq);
	void (*OnClose)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, xnet_result iReason);
	void (*OnError)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, int iSysErr);
} xhttpdevents;
#endif

struct xrt_httpd_conn {
	struct xrt_httpd_conn* pNext;
	volatile long iCleanupPosted;
	volatile long iConnLock;
	volatile long iRefCount;
	xhttpdserver* pServer;
	xnetstream* pStream;
	xhttpdrequest* pRequest;
	bool bResponseInFlight;
	bool bResponseCommitted;
	bool bResponseDrained;
	bool bResponseStreaming;
	bool bResponseChunked;
	bool bAsyncPending;
	bool bKeepAlive;
	bool bContinueSent;
};

struct xrt_httpd_server {
	xnetengine* pEngine;
	xnetlistener* pListener;
	xhttpdconfig tConfig;
	xtlsconfig tTlsConfig;
	xhttpdevents tEvents;
	ptr pUserData;
	volatile long iConnLock;
	volatile long bRunning;
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
static void __xhttpdCopyToken(char* sDst, size_t iDstCap, const char* sSrc)
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
			break;
		case 6:
			if ( sMethod[0] == 'D' && sMethod[1] == 'E' && sMethod[2] == 'L' && sMethod[3] == 'E' && sMethod[4] == 'T' && sMethod[5] == 'E' ) { return XHTTPD_METHOD_DELETE; }
			break;
		case 7:
			if ( sMethod[0] == 'O' && sMethod[1] == 'P' && sMethod[2] == 'T' && sMethod[3] == 'I' && sMethod[4] == 'O' && sMethod[5] == 'N' && sMethod[6] == 'S' ) { return XHTTPD_METHOD_OPTIONS; }
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

static void __xhttpdStreamOnRecv(ptr pOwner, xnetstream* pStream, xnetchain* pChain);
static void __xhttpdEmitServerError(xhttpdserver* pServer, xhttpdconn* pConn, int iSysErr);


typedef struct {
	xhttpdconn* pConn;
} __xhttpd_conn_task;


// 内部函数：__xhttpdRequestCreate
static xhttpdrequest* __xhttpdRequestCreate(void)
{
	xhttpdrequest* pReq = (xhttpdrequest*)XNET_ALLOC(sizeof(xhttpdrequest));
	if ( !pReq ) { return NULL; }
	xrtHttpdRequestInit(pReq);
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


// 内部函数：__xhttpdConnRelease
static void __xhttpdConnRelease(xhttpdconn* pConn)
{
	if ( !pConn ) { return; }
	if ( __xhttpdAtomicAdd(&pConn->iRefCount, -1) != 0 ) { return; }
	__xhttpdRequestDestroy(pConn->pRequest);
	pConn->pRequest = NULL;
	XNET_FREE(pConn);
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
		case 408: return "Request Timeout";
		case 413: return "Payload Too Large";
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
		if ( __xhttpdStrEqNoCase(pResp->arrHeaders[i].sName, sName) ) { return true; }
	}
	return false;
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


// 获取 HTTP 服务端 request 头部
XXAPI const char* xrtHttpdRequestHeader(const xhttpdrequest* pReq, const char* sName)
{
	if ( !pReq || !sName ) { return NULL; }
	for ( uint32 i = 0; i < pReq->iHeaderCount; ++i ) {
		if ( __xhttpdStrEqNoCase(pReq->arrHeaders[i].sName, sName) ) { return pReq->arrHeaders[i].sValue; }
	}
	return NULL;
}


// 获取 HTTP 服务端 request 方法 ID
XXAPI uint32 xrtHttpdRequestMethod(const xhttpdrequest* pReq)
{
	return pReq ? pReq->iMethod : XHTTPD_METHOD_UNKNOWN;
}


// 获取 HTTP 服务端 response 头部
XXAPI const char* xrtHttpdResponseHeader(const xhttpdresponse* pResp, const char* sName)
{
	if ( !pResp || !sName ) { return NULL; }
	for ( uint32 i = 0; i < pResp->iHeaderCount; ++i ) {
		if ( __xhttpdStrEqNoCase(pResp->arrHeaders[i].sName, sName) ) { return pResp->arrHeaders[i].sValue; }
	}
	return NULL;
}


// 初始化 HTTP 服务端配置
XXAPI void xrtHttpdConfigInit(xhttpdconfig* pCfg)
{
	if ( !pCfg ) { return; }
	memset(pCfg, 0, sizeof(xhttpdconfig));
	xrtNetAddrInitAny(&pCfg->tBindAddr, AF_INET, 0);
	pCfg->iBacklog = 128u;
	pCfg->iRecvLimit = 1024u * 1024u;
	pCfg->iBodyLimit = pCfg->iRecvLimit;
}


// xrtHttpdRequestInit 相关处理
XXAPI void xrtHttpdRequestInit(xhttpdrequest* pReq)
{
	if ( !pReq ) { return; }
	memset(pReq, 0, sizeof(xhttpdrequest));
	pReq->iContentLength = -1;
}


// xrtHttpdRequestUnit 相关处理
XXAPI void xrtHttpdRequestUnit(xhttpdrequest* pReq)
{
	if ( !pReq ) { return; }
	if ( pReq->pBody ) {
		XNET_FREE(pReq->pBody);
		pReq->pBody = NULL;
	}
	pReq->iBodyLen = 0;
	memset(pReq, 0, sizeof(xhttpdrequest));
	pReq->iContentLength = -1;
}


// xrtHttpdResponseInit 相关处理
XXAPI void xrtHttpdResponseInit(xhttpdresponse* pResp)
{
	if ( !pResp ) { return; }
	memset(pResp, 0, sizeof(xhttpdresponse));
	pResp->iStatusCode = 200u;
	pResp->iFlags = XHTTPD_RESP_F_CLOSE;
}


// xrtHttpdResponseUnit 相关处理
XXAPI void xrtHttpdResponseUnit(xhttpdresponse* pResp)
{
	if ( !pResp ) { return; }
	if ( pResp->pBody ) {
		XNET_FREE(pResp->pBody);
		pResp->pBody = NULL;
	}
	pResp->iBodyLen = 0;
	memset(pResp, 0, sizeof(xhttpdresponse));
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
XXAPI void xrtHttpdResponseSetStatus(xhttpdresponse* pResp, uint32 iStatusCode, const char* sReason)
{
	if ( !pResp ) { return; }
	pResp->iStatusCode = iStatusCode;
	if ( !__xhttpdValidReasonPhrase(sReason) ) {
		pResp->sReason[0] = '\0';
		return;
	}
	__xhttpdCopyToken(pResp->sReason, sizeof(pResp->sReason), sReason);
}


// 设置 HTTP 服务端 response 头部
XXAPI bool xrtHttpdResponseSetHeader(xhttpdresponse* pResp, const char* sName, const char* sValue)
{
	xhttpdheader* pHeader;
	if ( !pResp || !sName || !sValue ) { return false; }
	if ( !__xhttpdValidHeaderName(sName) || !__xhttpdValidHeaderValue(sValue) ) { return false; }
	for ( uint32 i = 0; i < pResp->iHeaderCount; ++i ) {
		if ( __xhttpdStrEqNoCase(pResp->arrHeaders[i].sName, sName) ) {
			__xhttpdCopyToken(pResp->arrHeaders[i].sValue, sizeof(pResp->arrHeaders[i].sValue), sValue);
			return true;
		}
	}
	if ( pResp->iHeaderCount >= XHTTPD_MAX_HEADERS ) { return false; }
	pHeader = &pResp->arrHeaders[pResp->iHeaderCount++];
	__xhttpdCopyToken(pHeader->sName, sizeof(pHeader->sName), sName);
	__xhttpdCopyToken(pHeader->sValue, sizeof(pHeader->sValue), sValue);
	return true;
}


// xrtHttpdResponseSetBodyCopy 相关处理
XXAPI bool xrtHttpdResponseSetBodyCopy(xhttpdresponse* pResp, const void* pData, size_t iLen, const char* sContentType)
{
	char* pBodyCopy = NULL;
	if ( !pResp ) { return false; }
	if ( pResp->pBody ) {
		XNET_FREE(pResp->pBody);
		pResp->pBody = NULL;
	}
	pResp->iBodyLen = 0;
	if ( pData && iLen > 0 ) {
		if ( iLen == SIZE_MAX ) { return false; }
		pBodyCopy = (char*)XNET_ALLOC(iLen + 1u);
		if ( !pBodyCopy ) { return false; }
		memcpy(pBodyCopy, pData, iLen);
		pBodyCopy[iLen] = '\0';
	}
	pResp->pBody = pBodyCopy;
	pResp->iBodyLen = iLen;
	if ( sContentType && sContentType[0] ) {
		if ( !xrtHttpdResponseSetHeader(pResp, "Content-Type", sContentType) ) {
			if ( pResp->pBody ) { XNET_FREE(pResp->pBody); }
			pResp->pBody = NULL;
			pResp->iBodyLen = 0;
			return false;
		}
	}
	return true;
}


// 内部函数：应用轻量响应头部块
static bool __xhttpdResponseApplyHeaderBlock(xhttpdresponse* pResp, const char* sHeaders)
{
	xrtheaderpair tHeader;
	size_t iLen;
	size_t iOff = 0u;
	char sName[XHTTPD_HEADER_NAME_CAP];
	char sValue[XHTTPD_HEADER_VALUE_CAP];
	if ( !pResp ) { return false; }
	if ( !sHeaders || !sHeaders[0] ) { return true; }
	iLen = strlen(sHeaders);
	while ( xrtHttpHeaderNextLineN(sHeaders, iLen, &iOff, &tHeader) ) {
		if ( tHeader.tName.iLen == 0u || tHeader.tName.iLen >= sizeof(sName) ||
			tHeader.tValue.iLen >= sizeof(sValue) ) {
			return false;
		}
		memcpy(sName, tHeader.tName.sPtr, tHeader.tName.iLen);
		sName[tHeader.tName.iLen] = '\0';
		memcpy(sValue, tHeader.tValue.sPtr, tHeader.tValue.iLen);
		sValue[tHeader.tValue.iLen] = '\0';
		if ( __xhttpdResponseHeaderControlled(sName) ) { return false; }
		if ( !xrtHttpdResponseSetHeader(pResp, sName, sValue) ) { return false; }
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
	xrtHttpdResponseSetStatus(pResp, iStatusCode, sReason);
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
	if ( !pDst || !pSrc ) { return false; }
	memset(pDst, 0, sizeof(xhttpdresponse));
	*pDst = *pSrc;
	pDst->pBody = NULL;
	if ( pSrc->pBody && pSrc->iBodyLen > 0u ) {
		if ( pSrc->iBodyLen == SIZE_MAX ) {
			memset(pDst, 0, sizeof(xhttpdresponse));
			return false;
		}
		pDst->pBody = (char*)XNET_ALLOC(pSrc->iBodyLen + 1u);
		if ( !pDst->pBody ) {
			memset(pDst, 0, sizeof(xhttpdresponse));
			return false;
		}
		memcpy(pDst->pBody, pSrc->pBody, pSrc->iBodyLen);
		pDst->pBody[pSrc->iBodyLen] = '\0';
	}
	return true;
}


// 内部函数：__xhttpdRequestWantsKeepAlive
static bool __xhttpdRequestWantsKeepAlive(const xhttpdrequest* pReq)
{
	return pReq && (pReq->iFlags & XHTTPD_REQ_F_KEEPALIVE) != 0u;
}


// 内部函数：__xhttpdPrepareResponse
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


// 内部函数：__xhttpdBuildRequest
static bool __xhttpdBuildRequest(const xcodecframe* pFrame, const xcodechttp1msg* pMsg, const xnetchain* pChain, xhttpdrequest* pReq, size_t iBodyLimit)
{
	size_t iBodyBytes;
	xrturlview tTarget;
	// 参数校验并初始化请求结构体
	if ( !pFrame || !pMsg || !pChain || !pReq ) { return false; }
	xrtHttpdRequestInit(pReq);
	// 映射编解码器标志到请求标志
	if ( pMsg->iFlags & XCODEC_HTTP1_F_KEEPALIVE ) { pReq->iFlags |= XHTTPD_REQ_F_KEEPALIVE; }
	if ( pMsg->iFlags & XCODEC_HTTP1_F_CHUNKED ) { pReq->iFlags |= XHTTPD_REQ_F_CHUNKED; }
	if ( pMsg->iFlags & XCODEC_HTTP1_F_UPGRADE ) { pReq->iFlags |= XHTTPD_REQ_F_UPGRADE; }
	// 复制基本字段
	pReq->iContentLength = pMsg->iContentLength;
	__xhttpdCopyToken(pReq->sMethod, sizeof(pReq->sMethod), pMsg->sMethod);
	pReq->iMethod = __xhttpdParseMethodID(pReq->sMethod);
	__xhttpdCopyToken(pReq->sTarget, sizeof(pReq->sTarget), pMsg->sTarget);
	__xhttpdCopyToken(pReq->sVersion, sizeof(pReq->sVersion), pMsg->sVersion);
	// 解析请求目标为路径和查询字符串
	if ( !xrtUrlParseTarget(pMsg->sTarget, &tTarget) ) { return false; }
	if ( (tTarget.iFlags & XRT_URL_F_HAS_FRAGMENT) != 0u ) { return false; }
	if ( tTarget.tPath.iLen == 0u ) {
		__xhttpdCopyToken(pReq->sPath, sizeof(pReq->sPath), "/");
	} else {
		if ( !xrtUrlViewCopyPathTo(&tTarget, pReq->sPath, sizeof(pReq->sPath)) ) { return false; }
	}
	if ( (tTarget.iFlags & XRT_URL_F_HAS_QUERY) != 0u ) {
		if ( !xrtUrlViewCopyQueryTo(&tTarget, pReq->sQuery, sizeof(pReq->sQuery)) ) { return false; }
	}
	// 复制请求头部
	if ( pMsg->iHeaderCount > XHTTPD_MAX_HEADERS ) { return false; }
	pReq->iHeaderCount = pMsg->iHeaderCount;
	for ( uint32 i = 0; i < pReq->iHeaderCount; ++i ) {
		__xhttpdCopyToken(pReq->arrHeaders[i].sName, sizeof(pReq->arrHeaders[i].sName), pMsg->arrHeaders[i].sName);
		__xhttpdCopyToken(pReq->arrHeaders[i].sValue, sizeof(pReq->arrHeaders[i].sValue), pMsg->arrHeaders[i].sValue);
	}
	// 提取并复制请求正文
	iBodyBytes = xrtCodecHttp1BodyBytes(pFrame);
	if ( iBodyLimit > 0u && iBodyBytes > iBodyLimit ) { return false; }
	if ( (pMsg->iFlags & XCODEC_HTTP1_F_CHUNKED) != 0u ) { pReq->iContentLength = (int64_t)iBodyBytes; }
	if ( iBodyBytes > 0u ) {
		if ( iBodyBytes == SIZE_MAX ) {
			xrtHttpdRequestUnit(pReq);
			return false;
		}
		pReq->pBody = (char*)XNET_ALLOC(iBodyBytes + 1u);
		if ( !pReq->pBody ) {
			xrtHttpdRequestUnit(pReq);
			return false;
		}
		pReq->iBodyLen = xrtCodecHttp1CopyBody(pChain, pFrame, pReq->pBody, iBodyBytes);
		pReq->pBody[pReq->iBodyLen] = '\0';
	}
	return true;
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


// 内部函数：__xhttpdBuildResponseBytes
static bool __xhttpdBuildResponseBytes(const xhttpdresponse* pResp, char** ppOut, size_t* pOutLen)
{
	char* pBuf = NULL;
	size_t iLen = 0;
	size_t iCap = 0;
	char aLine[512];
	const char* sReason;
	bool bChunked;
	// 参数校验
	if ( !pResp || !ppOut || !pOutLen ) { return false; }
	// 确定原因短语和传输编码模式
	sReason = pResp->sReason[0] ? pResp->sReason : __xhttpdStatusText(pResp->iStatusCode);
	if ( !__xhttpdValidReasonPhrase(sReason) ) { goto fail; }
	bChunked = __xhttpdContainsTokenNoCase(xrtHttpdResponseHeader(pResp, "Transfer-Encoding"), "chunked");
	// 构建状态行
	snprintf(aLine, sizeof(aLine), "HTTP/1.1 %u %s\r\n", (unsigned)pResp->iStatusCode, sReason);
	if ( !__xhttpdAppendText(&pBuf, &iLen, &iCap, aLine) ) { goto fail; }
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
	if ( !bChunked && !__xhttpdResponseHasHeader(pResp, "Content-Length") ) {
		snprintf(aLine, sizeof(aLine), "Content-Length: %llu\r\n", (unsigned long long)pResp->iBodyLen);
		if ( !__xhttpdAppendText(&pBuf, &iLen, &iCap, aLine) ) { goto fail; }
	}
	// 追加用户自定义头部
	for ( uint32 i = 0; i < pResp->iHeaderCount; ++i ) {
		if ( bChunked && __xhttpdStrEqNoCase(pResp->arrHeaders[i].sName, "Content-Length") ) { continue; }
		if ( !__xhttpdValidHeaderName(pResp->arrHeaders[i].sName)
			|| !__xhttpdValidHeaderValue(pResp->arrHeaders[i].sValue) ) {
			goto fail;
		}
		snprintf(aLine, sizeof(aLine), "%s: %s\r\n", pResp->arrHeaders[i].sName, pResp->arrHeaders[i].sValue);
		if ( !__xhttpdAppendText(&pBuf, &iLen, &iCap, aLine) ) { goto fail; }
	}
	// 空行分隔头部和正文
	if ( !__xhttpdAppendText(&pBuf, &iLen, &iCap, "\r\n") ) { goto fail; }
	// 处理响应正文
	if ( bChunked ) {
		if ( pResp->pBody && pResp->iBodyLen > 0 ) {
			snprintf(aLine, sizeof(aLine), "%llX\r\n", (unsigned long long)pResp->iBodyLen);
			if ( !__xhttpdAppendText(&pBuf, &iLen, &iCap, aLine) ) { goto fail; }
			if ( !__xhttpdAppendBytes(&pBuf, &iLen, &iCap, pResp->pBody, pResp->iBodyLen) ) { goto fail; }
			if ( !__xhttpdAppendText(&pBuf, &iLen, &iCap, "\r\n") ) { goto fail; }
		}
		if ( !__xhttpdAppendText(&pBuf, &iLen, &iCap, "0\r\n\r\n") ) { goto fail; }
	} else if ( pResp->pBody && pResp->iBodyLen > 0 ) {
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
	char aLine[512];
	const char* sReason;
	bool bChunked;
	if ( !pResp || !ppOut || !pOutLen ) { return false; }
	sReason = pResp->sReason[0] ? pResp->sReason : __xhttpdStatusText(pResp->iStatusCode);
	if ( !__xhttpdValidReasonPhrase(sReason) ) { goto fail; }
	bChunked = __xhttpdContainsTokenNoCase(xrtHttpdResponseHeader(pResp, "Transfer-Encoding"), "chunked");
	snprintf(aLine, sizeof(aLine), "HTTP/1.1 %u %s\r\n", (unsigned)pResp->iStatusCode, sReason);
	if ( !__xhttpdAppendText(&pBuf, &iLen, &iCap, aLine) ) { goto fail; }
	if ( !__xhttpdResponseHasHeader(pResp, "Date") ) {
		if ( __xhttpdFormatDate(aLine, sizeof(aLine)) ) {
			if ( !__xhttpdAppendText(&pBuf, &iLen, &iCap, aLine) ) { goto fail; }
		}
	}
	if ( !__xhttpdResponseHasHeader(pResp, "Connection") ) {
		if ( !__xhttpdAppendText(&pBuf, &iLen, &iCap, "Connection: close\r\n") ) { goto fail; }
	}
	if ( !bChunked && !__xhttpdResponseHasHeader(pResp, "Content-Length") ) {
		snprintf(aLine, sizeof(aLine), "Content-Length: %llu\r\n", (unsigned long long)pResp->iBodyLen);
		if ( !__xhttpdAppendText(&pBuf, &iLen, &iCap, aLine) ) { goto fail; }
	}
	for ( uint32 i = 0; i < pResp->iHeaderCount; ++i ) {
		if ( bChunked && __xhttpdStrEqNoCase(pResp->arrHeaders[i].sName, "Content-Length") ) { continue; }
		if ( !__xhttpdValidHeaderName(pResp->arrHeaders[i].sName) ||
			!__xhttpdValidHeaderValue(pResp->arrHeaders[i].sValue) ) {
			goto fail;
		}
		snprintf(aLine, sizeof(aLine), "%s: %s\r\n", pResp->arrHeaders[i].sName, pResp->arrHeaders[i].sValue);
		if ( !__xhttpdAppendText(&pBuf, &iLen, &iCap, aLine) ) { goto fail; }
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
	pConn->bAsyncPending = false;
	pConn->bKeepAlive = false;
	pConn->bContinueSent = false;
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
	bool bKickRecv = false;

	if ( !pConn ) { return; }

	__xhttpdLock(&pConn->iConnLock);
	if ( pConn->pRequest && !pConn->bAsyncPending ) {
		if ( pConn->pStream == NULL ) {
			pReq = __xhttpdConnDetachRequestLocked(pConn);
		}
		else if ( pConn->bKeepAlive && pConn->bResponseCommitted && pConn->bResponseDrained && !pConn->pStream->bClosing ) {
			pReq = __xhttpdConnDetachRequestLocked(pConn);
			pStream = pConn->pStream;
			__xnetStreamAddAsyncHold(pStream);
			bKickRecv = xrtNetChainBytes(&pStream->tRxChain) > 0u;
		}
	}
	__xhttpdUnlock(&pConn->iConnLock);

	__xhttpdRequestDestroy(pReq);
	if ( bKickRecv ) { __xhttpdConnPostRecvKick(pConn, pStream); }
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
			else if ( bClose ) {
				xrtNetStreamClose(pStream, XNET_CLOSE_F_GRACEFUL);
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
	if ( !__xhttpdPrepareResponse(pReq, pResp, &tSend, &bKeepAlive) ) {
		__xhttpdUnlock(&pConn->iConnLock);
		return XRT_NET_ERROR;
	}
	// 序列化响应为原始字节
	if ( !__xhttpdBuildResponseBytes(&tSend, &pBytes, &iLen) ) {
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
		else if ( !bKeepAlive ) {
			xrtNetStreamClose(pStream, XNET_CLOSE_F_GRACEFUL);
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
	xrtHttpdResponseSetStatus(&tResp, iStatusCode, sReason);
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
	size_t iLen = 0u;
	bool bKeepAlive = false;
	bool bChunked;
	xnet_result iRet = XRT_NET_ERROR;
	if ( !pConn || !pResp ) { return XRT_NET_ERROR; }
	memset(&tBase, 0, sizeof(tBase));
	memset(&tSend, 0, sizeof(tSend));
	if ( !__xhttpdResponseCopy(&tBase, pResp) ) { goto end; }
	if ( tBase.pBody ) {
		XNET_FREE(tBase.pBody);
		tBase.pBody = NULL;
	}
	tBase.iBodyLen = 0u;
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
	if ( !__xhttpdPrepareResponse(pReq, &tBase, &tSend, &bKeepAlive) ) {
		__xhttpdUnlock(&pConn->iConnLock);
		goto end;
	}
	if ( !__xhttpdBuildResponseHeadBytes(&tSend, &pBytes, &iLen) ) {
		__xhttpdUnlock(&pConn->iConnLock);
		goto end;
	}
	pConn->bResponseCommitted = true;
	pConn->bResponseDrained = false;
	pConn->bResponseStreaming = true;
	pConn->bResponseChunked = bChunked;
	pConn->bKeepAlive = bKeepAlive;
	__xnetStreamAddAsyncHold(pStream);
	__xhttpdUnlock(&pConn->iConnLock);
	iRet = xrtNetStreamSend(pStream, pBytes, iLen);
	if ( iRet != XRT_NET_OK ) {
		__xhttpdLock(&pConn->iConnLock);
		pConn->bResponseStreaming = false;
		pConn->bResponseChunked = false;
		__xhttpdUnlock(&pConn->iConnLock);
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
	}
	__xnetStreamReleaseAsyncHold(pStream);
end:
	if ( pBytes ) { XNET_FREE(pBytes); }
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
		__xhttpdUnlock(&pConn->iConnLock);
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
	}
	__xnetStreamReleaseAsyncHold(pStream);
	return iRet;
}


// xrtHttpdConnEnd 相关处理
XXAPI xnet_result xrtHttpdConnEnd(xhttpdconn* pConn)
{
	xnetstream* pStream;
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
	__xnetStreamAddAsyncHold(pStream);
	__xhttpdUnlock(&pConn->iConnLock);
	iRet = bChunked ? xrtNetStreamSend(pStream, "0\r\n\r\n", 5u) : XRT_NET_OK;
	__xhttpdLock(&pConn->iConnLock);
	pConn->bResponseStreaming = false;
	pConn->bResponseChunked = false;
	__xhttpdUnlock(&pConn->iConnLock);
	if ( iRet != XRT_NET_OK ) {
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
	}
	else if ( !bKeepAlive ) {
		xrtNetStreamClose(pStream, XNET_CLOSE_F_GRACEFUL);
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
	(void)iChunkSize;
	if ( !pConn || !pResp || !sFilePath || !sFilePath[0] ) { return XRT_NET_ERROR; }
	iFileSize = xrtFileGetSize((str)sFilePath);
	objFile = xrtOpen((str)sFilePath, TRUE, XRT_CP_BINARY);
	if ( objFile == NULL ) { return XRT_NET_ERROR; }
	if ( iFileSize > 0u ) {
		pBuf = (char*)XNET_ALLOC(iFileSize);
		if ( pBuf == NULL ) {
			xrtClose(objFile);
			return XRT_NET_ERROR;
		}
		iRead = xrtGetBuffer(objFile, pBuf, iFileSize);
		if ( iRead != iFileSize ) {
			XNET_FREE(pBuf);
			xrtClose(objFile);
			return XRT_NET_ERROR;
		}
	}
	xrtClose(objFile);
	objFile = NULL;
	memset(&tFile, 0, sizeof(tFile));
	if ( !__xhttpdResponseCopy(&tFile, pResp) ) {
		XNET_FREE(pBuf);
		return XRT_NET_ERROR;
	}
	if ( !__xhttpdResponseHasHeader(&tFile, "Content-Length") ) {
		snprintf(sLength, sizeof(sLength), "%llu", (unsigned long long)iFileSize);
		if ( !xrtHttpdResponseSetHeader(&tFile, "Content-Length", sLength) ) {
			XNET_FREE(pBuf);
			xrtHttpdResponseUnit(&tFile);
			return XRT_NET_ERROR;
		}
	}
	if ( tFile.pBody ) { XNET_FREE(tFile.pBody); }
	tFile.pBody = pBuf;
	tFile.iBodyLen = iFileSize;
	pBuf = NULL;
	iRet = xrtHttpdConnRespond(pConn, &tFile);
	xrtHttpdResponseUnit(&tFile);
	return iRet;
}


// xrtHttpdConnClose 相关处理
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


// 内部函数：__xhttpdServerDetachAllConns
static xhttpdconn* __xhttpdServerDetachAllConns(xhttpdserver* pServer)
{
	xhttpdconn* pHead;
	if ( !pServer ) { return NULL; }
	__xhttpdLock(&pServer->iConnLock);
	pHead = pServer->pConnHead;
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
static bool __xhttpdMaybeSendContinue(xhttpdconn* pConn, xnetstream* pStream, xnetchain* pChain)
{
	static const char aHeadEnd[] = "\r\n\r\n";
	char* pHead;
	char* pEnd;
	size_t iAvail;
	bool bShouldSend = false;

	if ( !pConn || !pStream || !pChain ) { return false; }
	__xhttpdLock(&pConn->iConnLock);
	if ( pConn->bContinueSent || pConn->bResponseInFlight || __xhttpdAtomicLoad(&pConn->iCleanupPosted) != 0 ) {
		__xhttpdUnlock(&pConn->iConnLock);
		return false;
	}
	__xhttpdUnlock(&pConn->iConnLock);

	iAvail = xrtNetChainBytes(pChain);
	if ( iAvail == 0u || iAvail == SIZE_MAX ) { return false; }
	pHead = (char*)XNET_ALLOC(iAvail + 1u);
	if ( !pHead ) { return false; }
	if ( xrtNetChainPeek(pChain, pHead, iAvail) == iAvail ) {
		pHead[iAvail] = '\0';
		pEnd = strstr(pHead, aHeadEnd);
		if ( pEnd ) {
			pHead[(size_t)(pEnd - pHead) + sizeof(aHeadEnd) - 1u] = '\0';
		}
		bShouldSend = strstr(pHead, "\r\nExpect: 100-continue\r\n") != NULL
			|| strstr(pHead, "\r\nExpect: 100-Continue\r\n") != NULL
			|| strstr(pHead, "\r\nexpect: 100-continue\r\n") != NULL;
	}
	XNET_FREE(pHead);
	if ( !bShouldSend ) { return false; }

	__xhttpdLock(&pConn->iConnLock);
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
	return bShouldSend;
}


// 内部函数：获取状态正文
static const char* __xhttpdStatusBody(uint32 iStatusCode)
{
	switch ( iStatusCode ) {
		case 404: return "Not Found";
		case 408: return "Request Timeout";
		case 503: return "Service Unavailable";
		default: return "Internal Server Error";
	}
}


// 内部函数：__xhttpdAsyncRequestFinally
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
	if ( pServer && pServer->tEvents.OnOpen ) {
		pServer->tEvents.OnOpen(pServer->pUserData, pServer, pConn);
	}
}


// 内部函数：__xhttpdStreamOnRecv
static void __xhttpdStreamOnRecv(ptr pOwner, xnetstream* pStream, xnetchain* pChain)
{
	xhttpdconn* pConn = (xhttpdconn*)pOwner;
	xhttpdserver* pServer = pConn ? pConn->pServer : NULL;
	xcodecframe tFrame;
	xcodechttp1msg tMsg;
	xcodecstatus iParse;
	xhttpdrequest* pReq = NULL;
	xhttpdresponse tResp;
	xfuture* pAsync = NULL;
	bool bHandled = false;
	bool bResponseCommitted = false;
	// 参数校验与连接状态检查
	if ( !pConn || !pServer || !pStream || !pChain ) { return; }
	if ( __xhttpdAtomicLoad(&pConn->iCleanupPosted) != 0 ) { return; }
	// 检查是否有正在处理的响应
	__xhttpdLock(&pConn->iConnLock);
	if ( pConn->bResponseInFlight ) {
		__xhttpdUnlock(&pConn->iConnLock);
		return;
	}
	__xhttpdUnlock(&pConn->iConnLock);
	// 解析 HTTP/1.1 请求
	iParse = xrtCodecHttp1Parse(pChain, &tFrame, &tMsg);
	if ( iParse == XCODEC_STATUS_NEED_MORE ) {
		(void)__xhttpdMaybeSendContinue(pConn, pStream, pChain);
		return;
	}
	// 解析失败，返回 400 错误
	if ( iParse == XCODEC_STATUS_ERROR ) {
		__xhttpdEmitServerError(pServer, pConn, -1);
		__xhttpdSendSimpleStatus(pConn, 400u, "Bad Request");
		return;
	}
	if ( pServer->tConfig.iBodyLimit > 0u && xrtCodecHttp1BodyBytes(&tFrame) > pServer->tConfig.iBodyLimit ) {
		__xhttpdSendSimpleStatus(pConn, 413u, __xhttpdStatusBody(413u));
		xrtCodecFrameConsume(pChain, &tFrame);
		return;
	}
	// 创建并构建请求对象
	pReq = __xhttpdRequestCreate();
	if ( !pReq ) {
		__xhttpdEmitServerError(pServer, pConn, -1);
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return;
	}
	if ( !__xhttpdBuildRequest(&tFrame, &tMsg, pChain, pReq, pServer->tConfig.iBodyLimit) ) {
		__xhttpdRequestDestroy(pReq);
		__xhttpdEmitServerError(pServer, pConn, -1);
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return;
	}
	// 消费已解析的帧数据
	xrtCodecFrameConsume(pChain, &tFrame);

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
	if ( !pConn || !pStream ) { return; }
	if ( __xhttpdAtomicLoad(&pConn->iCleanupPosted) != 0 ) { return; }
	__xhttpdLock(&pConn->iConnLock);
	if ( !pConn->bResponseInFlight || !pConn->bResponseCommitted || !pConn->bKeepAlive || pStream->bClosing ) {
		__xhttpdUnlock(&pConn->iConnLock);
		return;
	}
	pConn->bResponseDrained = true;
	__xhttpdUnlock(&pConn->iConnLock);
	__xhttpdConnFinalizeMaybeAsync(pConn);
}


// 内部函数：__xhttpdStreamOnClose
static void __xhttpdStreamOnClose(ptr pOwner, xnetstream* pStream, xnet_result iReason)
{
	xhttpdconn* pConn = (xhttpdconn*)pOwner;
	xhttpdserver* pServer = pConn ? pConn->pServer : NULL;
	#if defined(XNET_DEBUG_CLOSE_DIAG)
		fprintf(stderr, "[CLOSE_DIAG][HTTPD] close conn=%p stream=%p reason=%d keepalive=%d inflight=%d\n",
			(void*)pConn,
			(void*)pStream,
			(int)iReason,
			pConn ? (pConn->bKeepAlive ? 1 : 0) : 0,
			pConn ? (pConn->bResponseInFlight ? 1 : 0) : 0);
	#endif
	(void)pStream;
	if ( pServer && pServer->tEvents.OnClose ) {
		pServer->tEvents.OnClose(pServer->pUserData, pServer, pConn, iReason);
	}
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
		NULL
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
	if ( !pEngine ) { return NULL; }
	pServer = (xhttpdserver*)XNET_ALLOC(sizeof(xhttpdserver));
	if ( !pServer ) { return NULL; }
	memset(pServer, 0, sizeof(xhttpdserver));
	pServer->pEngine = pEngine;
	if ( pCfg ) {
		pServer->tConfig = *pCfg;
	} else {
		xrtHttpdConfigInit(&pServer->tConfig);
	}
	pServer->tConfig.pTlsConfig = NULL;
	if ( pCfg && pCfg->pTlsConfig && !__xhttpdServerCopyTlsConfig(pServer, pCfg->pTlsConfig) ) {
		XNET_FREE(pServer);
		return NULL;
	}
	if ( pEvents ) { pServer->tEvents = *pEvents; }
	pServer->pUserData = pUserData;
	return pServer;
}


// 获取 HTTP 服务端 bound 端口
XXAPI uint16 xrtHttpdBoundPort(const xhttpdserver* pServer)
{
	return (pServer && pServer->pListener) ? pServer->pListener->tConfig.tBindAddr.iPort : 0u;
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


// 停止 HTTP 服务端
XXAPI void xrtHttpdStop(xhttpdserver* pServer)
{
	xhttpdconn* pConn;
	if ( !pServer ) { return; }
	if ( __xhttpdAtomicCompareExchange(&pServer->bRunning, 0, 1) != 1 ) { return; }
	if ( pServer->pListener ) {
		xrtNetListenerStop(pServer->pListener);
		xrtNetListenerDestroy(pServer->pListener);
		pServer->pListener = NULL;
	}
	pConn = __xhttpdServerDetachAllConns(pServer);
	while ( pConn ) {
		xhttpdconn* pNext = pConn->pNext;
		pConn->pNext = NULL;
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
		pConn = pNext;
	}
}


// 销毁 HTTP 服务端
XXAPI void xrtHttpdDestroy(xhttpdserver* pServer)
{
	if ( !pServer ) { return; }
	xrtHttpdStop(pServer);
	__xhttpdServerClearTlsConfig(pServer);
	XNET_FREE(pServer);
}

#endif
