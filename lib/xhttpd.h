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

typedef struct xrt_httpd_server xhttpdserver;
typedef struct xrt_httpd_conn xhttpdconn;

typedef struct {
	char sName[XHTTPD_HEADER_NAME_CAP];
	char sValue[XHTTPD_HEADER_VALUE_CAP];
} xhttpdheader;

typedef struct {
	uint32 iFlags;
	uint32 iHeaderCount;
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
	const xtlsconfig* pTlsConfig;
} xhttpdconfig;

typedef struct {
	void (*OnOpen)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn);
	bool (*OnRequest)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq, xhttpdresponse* pResp);
	void (*OnClose)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, xnet_result iReason);
	void (*OnError)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, int iSysErr);
} xhttpdevents;
#endif

struct xrt_httpd_conn {
	struct xrt_httpd_conn* pNext;
	volatile long iCleanupPosted;
	xhttpdserver* pServer;
	xnetstream* pStream;
	bool bResponseInFlight;
	bool bKeepAlive;
};

struct xrt_httpd_server {
	xnetengine* pEngine;
	xnetlistener* pListener;
	xhttpdconfig tConfig;
	xhttpdevents tEvents;
	ptr pUserData;
	volatile long iConnLock;
	volatile long bRunning;
	xhttpdconn* pConnHead;
};

static char __xhttpdToLower(char ch)
{
	if ( ch >= 'A' && ch <= 'Z' ) return (char)(ch + 32);
	return ch;
}

static bool __xhttpdStrEqNoCase(const char* sA, const char* sB)
{
	size_t i = 0;
	if ( !sA || !sB ) return false;
	while ( sA[i] && sB[i] ) {
		if ( __xhttpdToLower(sA[i]) != __xhttpdToLower(sB[i]) ) return false;
		i++;
	}
	return sA[i] == '\0' && sB[i] == '\0';
}

static long UNUSED_ATTR __xhttpdAtomicAdd(volatile long* pValue, long iDelta)
{
	return __xnetAtomicAddFetch32(pValue, iDelta);
}

static long __xhttpdAtomicCompareExchange(volatile long* pValue, long iExchange, long iComparand)
{
	return __xnetAtomicCompareExchange32(pValue, iExchange, iComparand);
}

static long __xhttpdAtomicLoad(volatile long* pValue)
{
	return __xnetAtomicLoad32(pValue);
}

static void __xhttpdSleep0(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(0);
	#else
		sched_yield();
	#endif
}

static void __xhttpdLock(volatile long* pLock)
{
	if ( !pLock ) return;
	while ( __xhttpdAtomicCompareExchange(pLock, 1, 0) != 0 ) {
		__xhttpdSleep0();
	}
}

static void __xhttpdUnlock(volatile long* pLock)
{
	if ( !pLock ) return;
	(void)__xnetAtomicExchange32(pLock, 0);
}

static void __xhttpdCopyToken(char* sDst, size_t iDstCap, const char* sSrc)
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

static bool __xhttpdAppendBytes(char** ppBuf, size_t* pLen, size_t* pCap, const void* pData, size_t iDataLen)
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

static bool __xhttpdAppendText(char** ppBuf, size_t* pLen, size_t* pCap, const char* sText)
{
	return __xhttpdAppendBytes(ppBuf, pLen, pCap, sText, sText ? strlen(__xrt_cstr(sText)) : 0);
}

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
		default: return "OK";
	}
}

static bool __xhttpdResponseHasHeader(const xhttpdresponse* pResp, const char* sName)
{
	if ( !pResp || !sName ) return false;
	for ( uint32 i = 0; i < pResp->iHeaderCount; ++i ) {
		if ( __xhttpdStrEqNoCase(pResp->arrHeaders[i].sName, sName) ) return true;
	}
	return false;
}

static bool __xhttpdContainsTokenNoCase(const char* sValue, const char* sToken)
{
	return xrtHttpHeaderContainsToken(sValue, sToken);
}

XXAPI const char* xrtHttpdRequestHeader(const xhttpdrequest* pReq, const char* sName)
{
	if ( !pReq || !sName ) return NULL;
	for ( uint32 i = 0; i < pReq->iHeaderCount; ++i ) {
		if ( __xhttpdStrEqNoCase(pReq->arrHeaders[i].sName, sName) ) return pReq->arrHeaders[i].sValue;
	}
	return NULL;
}

XXAPI const char* xrtHttpdResponseHeader(const xhttpdresponse* pResp, const char* sName)
{
	if ( !pResp || !sName ) return NULL;
	for ( uint32 i = 0; i < pResp->iHeaderCount; ++i ) {
		if ( __xhttpdStrEqNoCase(pResp->arrHeaders[i].sName, sName) ) return pResp->arrHeaders[i].sValue;
	}
	return NULL;
}

XXAPI void xrtHttpdConfigInit(xhttpdconfig* pCfg)
{
	if ( !pCfg ) return;
	memset(pCfg, 0, sizeof(xhttpdconfig));
	xrtNetAddrInitAny(&pCfg->tBindAddr, AF_INET, 0);
	pCfg->iBacklog = 128u;
	pCfg->iRecvLimit = 1024u * 1024u;
}

XXAPI void xrtHttpdRequestInit(xhttpdrequest* pReq)
{
	if ( !pReq ) return;
	memset(pReq, 0, sizeof(xhttpdrequest));
	pReq->iContentLength = -1;
}

XXAPI void xrtHttpdRequestUnit(xhttpdrequest* pReq)
{
	if ( !pReq ) return;
	if ( pReq->pBody ) {
		XNET_FREE(pReq->pBody);
		pReq->pBody = NULL;
	}
	pReq->iBodyLen = 0;
	memset(pReq, 0, sizeof(xhttpdrequest));
}

XXAPI void xrtHttpdResponseInit(xhttpdresponse* pResp)
{
	if ( !pResp ) return;
	memset(pResp, 0, sizeof(xhttpdresponse));
	pResp->iStatusCode = 200u;
	pResp->iFlags = XHTTPD_RESP_F_CLOSE;
}

XXAPI void xrtHttpdResponseUnit(xhttpdresponse* pResp)
{
	if ( !pResp ) return;
	if ( pResp->pBody ) {
		XNET_FREE(pResp->pBody);
		pResp->pBody = NULL;
	}
	pResp->iBodyLen = 0;
	memset(pResp, 0, sizeof(xhttpdresponse));
}

XXAPI void xrtHttpdResponseSetStatus(xhttpdresponse* pResp, uint32 iStatusCode, const char* sReason)
{
	if ( !pResp ) return;
	pResp->iStatusCode = iStatusCode;
	__xhttpdCopyToken(pResp->sReason, sizeof(pResp->sReason), sReason);
}

XXAPI bool xrtHttpdResponseSetHeader(xhttpdresponse* pResp, const char* sName, const char* sValue)
{
	xhttpdheader* pHeader;
	if ( !pResp || !sName || !sValue ) return false;
	for ( uint32 i = 0; i < pResp->iHeaderCount; ++i ) {
		if ( __xhttpdStrEqNoCase(pResp->arrHeaders[i].sName, sName) ) {
			__xhttpdCopyToken(pResp->arrHeaders[i].sValue, sizeof(pResp->arrHeaders[i].sValue), sValue);
			return true;
		}
	}
	if ( pResp->iHeaderCount >= XHTTPD_MAX_HEADERS ) return false;
	pHeader = &pResp->arrHeaders[pResp->iHeaderCount++];
	__xhttpdCopyToken(pHeader->sName, sizeof(pHeader->sName), sName);
	__xhttpdCopyToken(pHeader->sValue, sizeof(pHeader->sValue), sValue);
	return true;
}

XXAPI bool xrtHttpdResponseSetBodyCopy(xhttpdresponse* pResp, const void* pData, size_t iLen, const char* sContentType)
{
	char* pBodyCopy = NULL;
	if ( !pResp ) return false;
	if ( pResp->pBody ) {
		XNET_FREE(pResp->pBody);
		pResp->pBody = NULL;
	}
	pResp->iBodyLen = 0;
	if ( pData && iLen > 0 ) {
		pBodyCopy = (char*)XNET_ALLOC(iLen + 1u);
		if ( !pBodyCopy ) return false;
		memcpy(pBodyCopy, pData, iLen);
		pBodyCopy[iLen] = '\0';
	}
	pResp->pBody = pBodyCopy;
	pResp->iBodyLen = iLen;
	if ( sContentType && sContentType[0] ) {
		return xrtHttpdResponseSetHeader(pResp, "Content-Type", sContentType);
	}
	return true;
}

static bool __xhttpdBuildRequest(const xcodecframe* pFrame, const xcodechttp1msg* pMsg, const xnetchain* pChain, xhttpdrequest* pReq)
{
	size_t iBodyBytes;
	xrturlview tTarget;
	if ( !pFrame || !pMsg || !pChain || !pReq ) return false;
	xrtHttpdRequestInit(pReq);
	if ( pMsg->iFlags & XCODEC_HTTP1_F_KEEPALIVE ) pReq->iFlags |= XHTTPD_REQ_F_KEEPALIVE;
	if ( pMsg->iFlags & XCODEC_HTTP1_F_CHUNKED ) pReq->iFlags |= XHTTPD_REQ_F_CHUNKED;
	if ( pMsg->iFlags & XCODEC_HTTP1_F_UPGRADE ) pReq->iFlags |= XHTTPD_REQ_F_UPGRADE;
	pReq->iContentLength = pMsg->iContentLength;
	__xhttpdCopyToken(pReq->sMethod, sizeof(pReq->sMethod), pMsg->sMethod);
	__xhttpdCopyToken(pReq->sTarget, sizeof(pReq->sTarget), pMsg->sTarget);
	__xhttpdCopyToken(pReq->sVersion, sizeof(pReq->sVersion), pMsg->sVersion);
	if ( !xrtUrlParseTarget(pMsg->sTarget, &tTarget) ) return false;
	if ( (tTarget.iFlags & XRT_URL_F_HAS_FRAGMENT) != 0u ) return false;
	if ( tTarget.tPath.iLen == 0u ) {
		__xhttpdCopyToken(pReq->sPath, sizeof(pReq->sPath), "/");
	} else {
		if ( !xrtUrlViewCopyPathTo(&tTarget, pReq->sPath, sizeof(pReq->sPath)) ) return false;
	}
	if ( (tTarget.iFlags & XRT_URL_F_HAS_QUERY) != 0u ) {
		if ( !xrtUrlViewCopyQueryTo(&tTarget, pReq->sQuery, sizeof(pReq->sQuery)) ) return false;
	}
	pReq->iHeaderCount = pMsg->iHeaderCount < XHTTPD_MAX_HEADERS ? pMsg->iHeaderCount : XHTTPD_MAX_HEADERS;
	for ( uint32 i = 0; i < pReq->iHeaderCount; ++i ) {
		__xhttpdCopyToken(pReq->arrHeaders[i].sName, sizeof(pReq->arrHeaders[i].sName), pMsg->arrHeaders[i].sName);
		__xhttpdCopyToken(pReq->arrHeaders[i].sValue, sizeof(pReq->arrHeaders[i].sValue), pMsg->arrHeaders[i].sValue);
	}
	iBodyBytes = xrtCodecHttp1BodyBytes(pFrame);
	if ( (pMsg->iFlags & XCODEC_HTTP1_F_CHUNKED) != 0u ) pReq->iContentLength = (int64_t)iBodyBytes;
	if ( iBodyBytes > 0u ) {
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

static bool __xhttpdBuildResponseBytes(const xhttpdresponse* pResp, char** ppOut, size_t* pOutLen)
{
	char* pBuf = NULL;
	size_t iLen = 0;
	size_t iCap = 0;
	char aLine[512];
	const char* sReason;
	bool bChunked;
	if ( !pResp || !ppOut || !pOutLen ) return false;
	sReason = pResp->sReason[0] ? pResp->sReason : __xhttpdStatusText(pResp->iStatusCode);
	bChunked = __xhttpdContainsTokenNoCase(xrtHttpdResponseHeader(pResp, "Transfer-Encoding"), "chunked");
	snprintf(aLine, sizeof(aLine), "HTTP/1.1 %u %s\r\n", (unsigned)pResp->iStatusCode, sReason);
	if ( !__xhttpdAppendText(&pBuf, &iLen, &iCap, aLine) ) goto fail;
	if ( !__xhttpdResponseHasHeader(pResp, "Connection") ) {
		if ( !__xhttpdAppendText(&pBuf, &iLen, &iCap, "Connection: close\r\n") ) goto fail;
	}
	if ( !bChunked && !__xhttpdResponseHasHeader(pResp, "Content-Length") ) {
		snprintf(aLine, sizeof(aLine), "Content-Length: %llu\r\n", (unsigned long long)pResp->iBodyLen);
		if ( !__xhttpdAppendText(&pBuf, &iLen, &iCap, aLine) ) goto fail;
	}
	for ( uint32 i = 0; i < pResp->iHeaderCount; ++i ) {
		if ( bChunked && __xhttpdStrEqNoCase(pResp->arrHeaders[i].sName, "Content-Length") ) continue;
		snprintf(aLine, sizeof(aLine), "%s: %s\r\n", pResp->arrHeaders[i].sName, pResp->arrHeaders[i].sValue);
		if ( !__xhttpdAppendText(&pBuf, &iLen, &iCap, aLine) ) goto fail;
	}
	if ( !__xhttpdAppendText(&pBuf, &iLen, &iCap, "\r\n") ) goto fail;
	if ( bChunked ) {
		snprintf(aLine, sizeof(aLine), "%llX\r\n", (unsigned long long)pResp->iBodyLen);
		if ( !__xhttpdAppendText(&pBuf, &iLen, &iCap, aLine) ) goto fail;
		if ( pResp->pBody && pResp->iBodyLen > 0 ) {
			if ( !__xhttpdAppendBytes(&pBuf, &iLen, &iCap, pResp->pBody, pResp->iBodyLen) ) goto fail;
		}
		if ( !__xhttpdAppendText(&pBuf, &iLen, &iCap, "\r\n0\r\n\r\n") ) goto fail;
	} else if ( pResp->pBody && pResp->iBodyLen > 0 ) {
		if ( !__xhttpdAppendBytes(&pBuf, &iLen, &iCap, pResp->pBody, pResp->iBodyLen) ) goto fail;
	}
	*ppOut = pBuf;
	*pOutLen = iLen;
	return true;

fail:
	if ( pBuf ) XNET_FREE(pBuf);
	return false;
}

static void __xhttpdServerAddConn(xhttpdserver* pServer, xhttpdconn* pConn)
{
	if ( !pServer || !pConn ) return;
	__xhttpdLock(&pServer->iConnLock);
	pConn->pNext = pServer->pConnHead;
	pServer->pConnHead = pConn;
	__xhttpdUnlock(&pServer->iConnLock);
}

static void __xhttpdServerRemoveConn(xhttpdserver* pServer, xhttpdconn* pConn)
{
	xhttpdconn** ppNode;
	if ( !pServer || !pConn ) return;
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

static xhttpdconn* __xhttpdServerDetachAllConns(xhttpdserver* pServer)
{
	xhttpdconn* pHead;
	if ( !pServer ) return NULL;
	__xhttpdLock(&pServer->iConnLock);
	pHead = pServer->pConnHead;
	pServer->pConnHead = NULL;
	__xhttpdUnlock(&pServer->iConnLock);
	return pHead;
}

static void __xhttpdConnCleanupTask(xnetworker* pWorker, ptr pArg)
{
	xhttpdconn* pConn = (xhttpdconn*)pArg;
	(void)pWorker;
	if ( !pConn ) return;
	if ( pConn->pServer ) {
		__xhttpdServerRemoveConn(pConn->pServer, pConn);
		pConn->pServer = NULL;
	}
	if ( pConn->pStream ) {
		xrtNetStreamDestroy(pConn->pStream);
		pConn->pStream = NULL;
	}
	XNET_FREE(pConn);
}

static void __xhttpdConnPostCleanup(xhttpdconn* pConn)
{
	if ( !pConn ) return;
	if ( __xhttpdAtomicCompareExchange(&pConn->iCleanupPosted, 1, 0) != 0 ) return;
	if ( pConn->pServer && pConn->pServer->pEngine && pConn->pStream && pConn->pStream->pWorker ) {
		if ( xrtNetEnginePost(pConn->pServer->pEngine, pConn->pStream->pWorker->iId, __xhttpdConnCleanupTask, pConn) == XRT_NET_OK ) {
			return;
		}
	}
	__xhttpdConnCleanupTask(NULL, pConn);
}

static void __xhttpdEmitServerError(xhttpdserver* pServer, xhttpdconn* pConn, int iSysErr)
{
	if ( pServer && pServer->tEvents.OnError ) {
		pServer->tEvents.OnError(pServer->pUserData, pServer, pConn, iSysErr);
	}
}

static bool __xhttpdIsBenignStreamError(xhttpdconn* pConn, xnetstream* pStream, int iSysErr)
{
	if ( pConn && __xhttpdAtomicLoad(&pConn->iCleanupPosted) != 0 ) return true;
	if ( pStream && pStream->bClosing ) return true;
	if ( pConn && !pConn->bResponseInFlight && iSysErr == -1 ) return true;
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

static bool __xhttpdSendResponseAndClose(xhttpdconn* pConn, const xhttpdresponse* pResp)
{
	char* pBytes = NULL;
	size_t iLen = 0;
	xnetstream* pStream;
	if ( !pConn || !pConn->pStream || !pResp ) return false;
	pStream = pConn->pStream;
	if ( !__xhttpdBuildResponseBytes(pResp, &pBytes, &iLen) ) return false;
	if ( pStream->pTls ) {
		if ( !__xnetStreamAppendTlsPlainCopy(pStream, pBytes, iLen) ) {
			XNET_FREE(pBytes);
			xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
			return false;
		}
	} else {
		if ( !__xnetStreamAppendSendCopy(pStream, pBytes, iLen) ) {
			XNET_FREE(pBytes);
			xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
			return false;
		}
	}
	XNET_FREE(pBytes);
	__xnetStreamKickWrite(pStream);
	if ( (pResp->iFlags & XHTTPD_RESP_F_CLOSE) != 0 ) {
		xrtNetStreamClose(pStream, XNET_CLOSE_F_GRACEFUL);
	}
	return true;
}

static void __xhttpdSendSimpleStatus(xhttpdconn* pConn, uint32 iStatusCode, const char* sBody)
{
	xhttpdresponse tResp;
	xrtHttpdResponseInit(&tResp);
	xrtHttpdResponseSetStatus(&tResp, iStatusCode, NULL);
	if ( sBody && sBody[0] ) {
		(void)xrtHttpdResponseSetBodyCopy(&tResp, sBody, strlen(sBody), "text/plain");
	}
	(void)__xhttpdSendResponseAndClose(pConn, &tResp);
	xrtHttpdResponseUnit(&tResp);
}

static bool __xhttpdListenerOnAccept(ptr pOwner, xnetlistener* pListener, xnetstream* pStream)
{
	xhttpdserver* pServer = (xhttpdserver*)pOwner;
	xhttpdconn* pConn;
	(void)pListener;
	if ( !pServer || !pStream ) return false;
	pConn = (xhttpdconn*)xrtNetStreamGetUserData(pStream);
	if ( !pConn ) {
		pConn = (xhttpdconn*)XNET_ALLOC(sizeof(xhttpdconn));
		if ( !pConn ) return false;
		memset(pConn, 0, sizeof(xhttpdconn));
		xrtNetStreamSetUserData(pStream, pConn);
	}
	pConn->pServer = pServer;
	pConn->pStream = pStream;
	__xhttpdServerAddConn(pServer, pConn);
	return true;
}

static void __xhttpdStreamOnOpen(ptr pOwner, xnetstream* pStream)
{
	xhttpdconn* pConn = (xhttpdconn*)pOwner;
	xhttpdserver* pServer = pConn ? pConn->pServer : NULL;
	(void)pStream;
	if ( pServer && pServer->tEvents.OnOpen ) {
		pServer->tEvents.OnOpen(pServer->pUserData, pServer, pConn);
	}
}

static void __xhttpdStreamOnRecv(ptr pOwner, xnetstream* pStream, xnetchain* pChain)
{
	xhttpdconn* pConn = (xhttpdconn*)pOwner;
	xhttpdserver* pServer = pConn ? pConn->pServer : NULL;
	xcodecframe tFrame;
	xcodechttp1msg tMsg;
	xcodecstatus iParse;
	xhttpdrequest tReq;
	xhttpdresponse tResp;
	bool bHandled = false;
	if ( !pConn || !pServer || !pStream || !pChain ) return;
	if ( __xhttpdAtomicLoad(&pConn->iCleanupPosted) != 0 ) return;
	if ( pConn->bResponseInFlight ) return;
	iParse = xrtCodecHttp1Parse(pChain, &tFrame, &tMsg);
	if ( iParse == XCODEC_STATUS_NEED_MORE ) return;
	if ( iParse == XCODEC_STATUS_ERROR ) {
		__xhttpdEmitServerError(pServer, pConn, -1);
		__xhttpdSendSimpleStatus(pConn, 400u, "Bad Request");
		return;
	}
	if ( !__xhttpdBuildRequest(&tFrame, &tMsg, pChain, &tReq) ) {
		__xhttpdEmitServerError(pServer, pConn, -1);
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return;
	}
	xrtHttpdResponseInit(&tResp);
	if ( pServer->tEvents.OnRequest ) {
		bHandled = pServer->tEvents.OnRequest(pServer->pUserData, pServer, pConn, &tReq, &tResp);
	}
	if ( !bHandled ) {
		xrtHttpdResponseSetStatus(&tResp, 404u, NULL);
		(void)xrtHttpdResponseSetBodyCopy(&tResp, "Not Found", 9, "text/plain");
	}
	if ( (tReq.iFlags & XHTTPD_REQ_F_KEEPALIVE) != 0 && !__xhttpdResponseHasHeader(&tResp, "Connection") ) {
		tResp.iFlags &= ~XHTTPD_RESP_F_CLOSE;
		(void)xrtHttpdResponseSetHeader(&tResp, "Connection", "keep-alive");
	}
	xrtCodecFrameConsume(pChain, &tFrame);
	pConn->bKeepAlive = ((tReq.iFlags & XHTTPD_REQ_F_KEEPALIVE) != 0) && ((tResp.iFlags & XHTTPD_RESP_F_CLOSE) == 0u);
	pConn->bResponseInFlight = true;
	if ( !__xhttpdSendResponseAndClose(pConn, &tResp) ) {
		__xhttpdEmitServerError(pServer, pConn, -1);
	}
	xrtHttpdRequestUnit(&tReq);
	xrtHttpdResponseUnit(&tResp);
}

static void __xhttpdStreamOnDrain(ptr pOwner, xnetstream* pStream)
{
	xhttpdconn* pConn = (xhttpdconn*)pOwner;
	if ( !pConn || !pStream ) return;
	if ( __xhttpdAtomicLoad(&pConn->iCleanupPosted) != 0 ) return;
	if ( !pConn->bResponseInFlight ) return;
	if ( !pConn->bKeepAlive || pStream->bClosing ) return;
	pConn->bResponseInFlight = false;
	if ( xrtNetChainBytes(&pStream->tRxChain) > 0u ) {
		__xhttpdStreamOnRecv(pOwner, pStream, &pStream->tRxChain);
	}
}

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

static void __xhttpdStreamOnError(ptr pOwner, xnetstream* pStream, int iSysErr)
{
	xhttpdconn* pConn = (xhttpdconn*)pOwner;
	xhttpdserver* pServer = pConn ? pConn->pServer : NULL;
	if ( __xhttpdIsBenignStreamError(pConn, pStream, iSysErr) ) return;
	__xhttpdEmitServerError(pServer, pConn, iSysErr);
}

static void __xhttpdAcceptReady(xnetlistener* pListener, xnet_result iStatus, xnetstream* pStream, ptr pCtx)
{
	xhttpdserver* pServer = (xhttpdserver*)pCtx;
	(void)pListener;
	(void)pStream;
	if ( !pServer || __xhttpdAtomicLoad(&pServer->bRunning) == 0 ) return;
	if ( pServer->pListener && pServer->pListener->bRunning ) {
		(void)__xnetListenerRegisterSyncAcceptWait(pServer->pListener, __xhttpdAcceptReady, NULL, pServer);
	}
	if ( iStatus != XRT_NET_OK && pServer->tEvents.OnError ) {
		pServer->tEvents.OnError(pServer->pUserData, pServer, NULL, -1);
	}
}

static void __xhttpdArmAcceptTask(xnetworker* pWorker, ptr pArg)
{
	xhttpdserver* pServer = (xhttpdserver*)pArg;
	(void)pWorker;
	if ( !pServer || !pServer->pListener || __xhttpdAtomicLoad(&pServer->bRunning) == 0 ) return;
	(void)__xnetListenerRegisterSyncAcceptWait(pServer->pListener, __xhttpdAcceptReady, NULL, pServer);
}

static const xnetlistenerevents* __xhttpdListenerEvents(void)
{
	static const xnetlistenerevents tEvents = {
		__xhttpdListenerOnAccept,
		NULL
	};
	return &tEvents;
}

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

XXAPI xhttpdserver* xrtHttpdCreate(xnetengine* pEngine, const xhttpdconfig* pCfg, const xhttpdevents* pEvents, ptr pUserData)
{
	xhttpdserver* pServer;
	if ( !pEngine ) return NULL;
	pServer = (xhttpdserver*)XNET_ALLOC(sizeof(xhttpdserver));
	if ( !pServer ) return NULL;
	memset(pServer, 0, sizeof(xhttpdserver));
	pServer->pEngine = pEngine;
	if ( pCfg ) {
		pServer->tConfig = *pCfg;
	} else {
		xrtHttpdConfigInit(&pServer->tConfig);
	}
	if ( pEvents ) pServer->tEvents = *pEvents;
	pServer->pUserData = pUserData;
	return pServer;
}

XXAPI uint16 xrtHttpdBoundPort(const xhttpdserver* pServer)
{
	return (pServer && pServer->pListener) ? pServer->pListener->tConfig.tBindAddr.iPort : 0u;
}

XXAPI xnet_result xrtHttpdStart(xhttpdserver* pServer)
{
	xnetlistenconfig tListenCfg;
	if ( !pServer || !pServer->pEngine ) return XRT_NET_ERROR;
	if ( __xhttpdAtomicLoad(&pServer->bRunning) != 0 ) return XRT_NET_OK;
	xrtNetListenConfigInit(&tListenCfg);
	tListenCfg.tBindAddr = pServer->tConfig.tBindAddr;
	tListenCfg.iFlags = pServer->tConfig.iFlags;
	tListenCfg.iBacklog = pServer->tConfig.iBacklog;
	tListenCfg.iRecvLimit = pServer->tConfig.iRecvLimit;
	tListenCfg.pTlsConfig = pServer->tConfig.pTlsConfig;
	pServer->pListener = xrtNetListenerCreate(pServer->pEngine, &tListenCfg, __xhttpdListenerEvents(), __xhttpdStreamEvents(), pServer);
	if ( !pServer->pListener ) return XRT_NET_ERROR;
	if ( xrtNetListenerStart(pServer->pListener) != XRT_NET_OK ) {
		xrtNetListenerDestroy(pServer->pListener);
		pServer->pListener = NULL;
		return XRT_NET_ERROR;
	}
	(void)__xhttpdAtomicCompareExchange(&pServer->bRunning, 1, 0);
	if ( xrtNetEnginePost(pServer->pEngine, pServer->pListener->pWorker->iId, __xhttpdArmAcceptTask, pServer) != XRT_NET_OK ) {
		(void)__xhttpdAtomicCompareExchange(&pServer->bRunning, 0, 1);
		xrtNetListenerStop(pServer->pListener);
		xrtNetListenerDestroy(pServer->pListener);
		pServer->pListener = NULL;
		return XRT_NET_ERROR;
	}
	return XRT_NET_OK;
}

XXAPI void xrtHttpdStop(xhttpdserver* pServer)
{
	xhttpdconn* pConn;
	if ( !pServer ) return;
	if ( __xhttpdAtomicCompareExchange(&pServer->bRunning, 0, 1) == 0 ) {
		/* already stopped */
	}
	if ( pServer->pListener ) {
		xrtNetListenerStop(pServer->pListener);
		xrtNetListenerDestroy(pServer->pListener);
		pServer->pListener = NULL;
	}
	pConn = __xhttpdServerDetachAllConns(pServer);
	while ( pConn ) {
		xhttpdconn* pNext = pConn->pNext;
		pConn->pNext = NULL;
		(void)__xhttpdAtomicCompareExchange(&pConn->iCleanupPosted, 1, 0);
		if ( pConn->pStream ) {
			xrtNetStreamClose(pConn->pStream, XNET_CLOSE_F_ABORT);
			xrtNetStreamDestroy(pConn->pStream);
			pConn->pStream = NULL;
		}
		XNET_FREE(pConn);
		pConn = pNext;
	}
}

XXAPI void xrtHttpdDestroy(xhttpdserver* pServer)
{
	if ( !pServer ) return;
	xrtHttpdStop(pServer);
	XNET_FREE(pServer);
}

#endif
