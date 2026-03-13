#ifndef XRT_XHTTPD2_H
#define XRT_XHTTPD2_H

#include "xcodec_http1.h"
#include "xnet_stream.h"

#if !defined(_WIN32) && !defined(_WIN64)
	#include <sched.h>
#endif

/*
    XNet V2 - HTTP Server Rebuild Skeleton

    Phase-3 scope in this header:
      - HTTP server wrapper on top of xnet_stream
      - request and response materialization helpers
      - plain HTTP and builtin TLS loopback service path

    Current limitations:
      - serial keep-alive reuse only
      - client-side connection pooling is still deferred
      - chunked request and response bodies are whole-message only
      - static files, routing tables, and upgrade paths are deferred
*/

#define XHTTPD2_METHOD_CAP         16u
#define XHTTPD2_TARGET_CAP         256u
#define XHTTPD2_PATH_CAP           256u
#define XHTTPD2_QUERY_CAP          256u
#define XHTTPD2_VERSION_CAP        32u
#define XHTTPD2_REASON_CAP         128u
#define XHTTPD2_HEADER_NAME_CAP    64u
#define XHTTPD2_HEADER_VALUE_CAP   256u
#define XHTTPD2_MAX_HEADERS        32u

#define XHTTPD2_REQ_F_NONE         0x00000000u
#define XHTTPD2_REQ_F_KEEPALIVE    0x00000001u
#define XHTTPD2_REQ_F_CHUNKED      0x00000002u
#define XHTTPD2_REQ_F_UPGRADE      0x00000004u

#define XHTTPD2_RESP_F_NONE        0x00000000u
#define XHTTPD2_RESP_F_CLOSE       0x00000001u

typedef struct xrt_httpd2_server xhttpd2server;
typedef struct xrt_httpd2_conn xhttpd2conn;

typedef struct {
	char sName[XHTTPD2_HEADER_NAME_CAP];
	char sValue[XHTTPD2_HEADER_VALUE_CAP];
} xhttpd2header;

typedef struct {
	uint32 iFlags;
	uint32 iHeaderCount;
	int64_t iContentLength;
	char sMethod[XHTTPD2_METHOD_CAP];
	char sTarget[XHTTPD2_TARGET_CAP];
	char sPath[XHTTPD2_PATH_CAP];
	char sQuery[XHTTPD2_QUERY_CAP];
	char sVersion[XHTTPD2_VERSION_CAP];
	xhttpd2header arrHeaders[XHTTPD2_MAX_HEADERS];
	char* pBody;
	size_t iBodyLen;
} xhttpd2request;

typedef struct {
	uint32 iStatusCode;
	uint32 iFlags;
	uint32 iHeaderCount;
	char sReason[XHTTPD2_REASON_CAP];
	xhttpd2header arrHeaders[XHTTPD2_MAX_HEADERS];
	char* pBody;
	size_t iBodyLen;
} xhttpd2response;

typedef struct {
	xnetaddr tBindAddr;
	uint32 iFlags;
	uint32 iBacklog;
	uint32 iRecvLimit;
	uint32 iAcceptPollMs;
	const xtlsconfig* pTlsConfig;
} xhttpd2config;

typedef struct {
	void (*OnOpen)(ptr pOwner, xhttpd2server* pServer, xhttpd2conn* pConn);
	bool (*OnRequest)(ptr pOwner, xhttpd2server* pServer, xhttpd2conn* pConn, const xhttpd2request* pReq, xhttpd2response* pResp);
	void (*OnClose)(ptr pOwner, xhttpd2server* pServer, xhttpd2conn* pConn, xnet_result iReason);
	void (*OnError)(ptr pOwner, xhttpd2server* pServer, xhttpd2conn* pConn, int iSysErr);
} xhttpd2events;

struct xrt_httpd2_conn {
	struct xrt_httpd2_conn* pNext;
	volatile long iCleanupPosted;
	xhttpd2server* pServer;
	xnetstream* pStream;
	bool bResponseInFlight;
	bool bKeepAlive;
};

struct xrt_httpd2_server {
	xnetengine* pEngine;
	xnetlistener* pListener;
	xhttpd2config tConfig;
	xhttpd2events tEvents;
	ptr pUserData;
	volatile long iConnLock;
	volatile long bRunning;
	xhttpd2conn* pConnHead;
#if defined(_WIN32) || defined(_WIN64)
	HANDLE hAcceptThread;
#else
	pthread_t hAcceptThread;
	bool bAcceptThreadStarted;
#endif
};

static char __xhttpd2ToLower(char ch)
{
	if ( ch >= 'A' && ch <= 'Z' ) return (char)(ch + 32);
	return ch;
}

static bool __xhttpd2StrEqNoCase(const char* sA, const char* sB)
{
	size_t i = 0;
	if ( !sA || !sB ) return false;
	while ( sA[i] && sB[i] ) {
		if ( __xhttpd2ToLower(sA[i]) != __xhttpd2ToLower(sB[i]) ) return false;
		i++;
	}
	return sA[i] == '\0' && sB[i] == '\0';
}

static long __xhttpd2AtomicAdd(volatile long* pValue, long iDelta)
{
	return __xnetAtomicAddFetch32(pValue, iDelta);
}

static long __xhttpd2AtomicCompareExchange(volatile long* pValue, long iExchange, long iComparand)
{
	return __xnetAtomicCompareExchange32(pValue, iExchange, iComparand);
}

static long __xhttpd2AtomicLoad(volatile long* pValue)
{
	return __xnetAtomicLoad32(pValue);
}

static void __xhttpd2Sleep0(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(0);
	#else
		sched_yield();
	#endif
}

static void __xhttpd2Lock(volatile long* pLock)
{
	if ( !pLock ) return;
	while ( __xhttpd2AtomicCompareExchange(pLock, 1, 0) != 0 ) {
		__xhttpd2Sleep0();
	}
}

static void __xhttpd2Unlock(volatile long* pLock)
{
	if ( !pLock ) return;
	(void)__xnetAtomicExchange32(pLock, 0);
}

static void __xhttpd2CopyToken(char* sDst, size_t iDstCap, const char* sSrc)
{
	size_t iLen;
	if ( !sDst || iDstCap == 0 ) return;
	if ( !sSrc ) {
		sDst[0] = '\0';
		return;
	}
	iLen = strlen(sSrc);
	if ( iLen >= iDstCap ) iLen = iDstCap - 1u;
	memcpy(sDst, sSrc, iLen);
	sDst[iLen] = '\0';
}

static bool __xhttpd2AppendBytes(char** ppBuf, size_t* pLen, size_t* pCap, const void* pData, size_t iDataLen)
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

static bool __xhttpd2AppendText(char** ppBuf, size_t* pLen, size_t* pCap, const char* sText)
{
	return __xhttpd2AppendBytes(ppBuf, pLen, pCap, sText, sText ? strlen(sText) : 0);
}

static const char* __xhttpd2StatusText(uint32 iStatusCode)
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

static bool __xhttpd2ResponseHasHeader(const xhttpd2response* pResp, const char* sName)
{
	if ( !pResp || !sName ) return false;
	for ( uint32 i = 0; i < pResp->iHeaderCount; ++i ) {
		if ( __xhttpd2StrEqNoCase(pResp->arrHeaders[i].sName, sName) ) return true;
	}
	return false;
}

static bool __xhttpd2ContainsTokenNoCase(const char* sValue, const char* sToken)
{
	size_t iTokenLen;
	size_t i;
	if ( !sValue || !sToken || sToken[0] == '\0' ) return false;
	iTokenLen = strlen(sToken);
	while ( *sValue ) {
		while ( *sValue == ' ' || *sValue == '\t' || *sValue == ',' ) sValue++;
		for ( i = 0; i < iTokenLen; ++i ) {
			if ( sValue[i] == '\0' || __xhttpd2ToLower(sValue[i]) != __xhttpd2ToLower(sToken[i]) ) break;
		}
		if ( i == iTokenLen ) {
			char ch = sValue[iTokenLen];
			if ( ch == '\0' || ch == ',' || ch == ' ' || ch == '\t' ) return true;
		}
		while ( *sValue && *sValue != ',' ) sValue++;
	}
	return false;
}

static const char* xrtHttpd2RequestHeader(const xhttpd2request* pReq, const char* sName)
{
	if ( !pReq || !sName ) return NULL;
	for ( uint32 i = 0; i < pReq->iHeaderCount; ++i ) {
		if ( __xhttpd2StrEqNoCase(pReq->arrHeaders[i].sName, sName) ) return pReq->arrHeaders[i].sValue;
	}
	return NULL;
}

static const char* xrtHttpd2ResponseHeader(const xhttpd2response* pResp, const char* sName)
{
	if ( !pResp || !sName ) return NULL;
	for ( uint32 i = 0; i < pResp->iHeaderCount; ++i ) {
		if ( __xhttpd2StrEqNoCase(pResp->arrHeaders[i].sName, sName) ) return pResp->arrHeaders[i].sValue;
	}
	return NULL;
}

static void xrtHttpd2ConfigInit(xhttpd2config* pCfg)
{
	if ( !pCfg ) return;
	memset(pCfg, 0, sizeof(xhttpd2config));
	xrtNetAddrInitAny(&pCfg->tBindAddr, AF_INET, 0);
	pCfg->iBacklog = 128u;
	pCfg->iRecvLimit = 1024u * 1024u;
	pCfg->iAcceptPollMs = 5u;
}

static void xrtHttpd2RequestInit(xhttpd2request* pReq)
{
	if ( !pReq ) return;
	memset(pReq, 0, sizeof(xhttpd2request));
	pReq->iContentLength = -1;
}

static void xrtHttpd2RequestUnit(xhttpd2request* pReq)
{
	if ( !pReq ) return;
	if ( pReq->pBody ) {
		XNET_FREE(pReq->pBody);
		pReq->pBody = NULL;
	}
	pReq->iBodyLen = 0;
	memset(pReq, 0, sizeof(xhttpd2request));
}

static void xrtHttpd2ResponseInit(xhttpd2response* pResp)
{
	if ( !pResp ) return;
	memset(pResp, 0, sizeof(xhttpd2response));
	pResp->iStatusCode = 200u;
	pResp->iFlags = XHTTPD2_RESP_F_CLOSE;
}

static void xrtHttpd2ResponseUnit(xhttpd2response* pResp)
{
	if ( !pResp ) return;
	if ( pResp->pBody ) {
		XNET_FREE(pResp->pBody);
		pResp->pBody = NULL;
	}
	pResp->iBodyLen = 0;
	memset(pResp, 0, sizeof(xhttpd2response));
}

static void xrtHttpd2ResponseSetStatus(xhttpd2response* pResp, uint32 iStatusCode, const char* sReason)
{
	if ( !pResp ) return;
	pResp->iStatusCode = iStatusCode;
	__xhttpd2CopyToken(pResp->sReason, sizeof(pResp->sReason), sReason);
}

static bool xrtHttpd2ResponseSetHeader(xhttpd2response* pResp, const char* sName, const char* sValue)
{
	xhttpd2header* pHeader;
	if ( !pResp || !sName || !sValue ) return false;
	for ( uint32 i = 0; i < pResp->iHeaderCount; ++i ) {
		if ( __xhttpd2StrEqNoCase(pResp->arrHeaders[i].sName, sName) ) {
			__xhttpd2CopyToken(pResp->arrHeaders[i].sValue, sizeof(pResp->arrHeaders[i].sValue), sValue);
			return true;
		}
	}
	if ( pResp->iHeaderCount >= XHTTPD2_MAX_HEADERS ) return false;
	pHeader = &pResp->arrHeaders[pResp->iHeaderCount++];
	__xhttpd2CopyToken(pHeader->sName, sizeof(pHeader->sName), sName);
	__xhttpd2CopyToken(pHeader->sValue, sizeof(pHeader->sValue), sValue);
	return true;
}

static bool xrtHttpd2ResponseSetBodyCopy(xhttpd2response* pResp, const void* pData, size_t iLen, const char* sContentType)
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
		return xrtHttpd2ResponseSetHeader(pResp, "Content-Type", sContentType);
	}
	return true;
}

static bool __xhttpd2BuildRequest(const xcodecframe* pFrame, const xcodechttp1msg* pMsg, const xnetchain* pChain, xhttpd2request* pReq)
{
	const char* sQuery;
	size_t iPathLen;
	size_t iBodyBytes;
	if ( !pFrame || !pMsg || !pChain || !pReq ) return false;
	xrtHttpd2RequestInit(pReq);
	if ( pMsg->iFlags & XCODEC_HTTP1_F_KEEPALIVE ) pReq->iFlags |= XHTTPD2_REQ_F_KEEPALIVE;
	if ( pMsg->iFlags & XCODEC_HTTP1_F_CHUNKED ) pReq->iFlags |= XHTTPD2_REQ_F_CHUNKED;
	if ( pMsg->iFlags & XCODEC_HTTP1_F_UPGRADE ) pReq->iFlags |= XHTTPD2_REQ_F_UPGRADE;
	pReq->iContentLength = pMsg->iContentLength;
	__xhttpd2CopyToken(pReq->sMethod, sizeof(pReq->sMethod), pMsg->sMethod);
	__xhttpd2CopyToken(pReq->sTarget, sizeof(pReq->sTarget), pMsg->sTarget);
	__xhttpd2CopyToken(pReq->sVersion, sizeof(pReq->sVersion), pMsg->sVersion);
	sQuery = strchr(pReq->sTarget, '?');
	if ( sQuery ) {
		iPathLen = (size_t)(sQuery - pReq->sTarget);
		if ( iPathLen >= sizeof(pReq->sPath) ) iPathLen = sizeof(pReq->sPath) - 1u;
		memcpy(pReq->sPath, pReq->sTarget, iPathLen);
		pReq->sPath[iPathLen] = '\0';
		__xhttpd2CopyToken(pReq->sQuery, sizeof(pReq->sQuery), sQuery + 1);
	} else {
		__xhttpd2CopyToken(pReq->sPath, sizeof(pReq->sPath), pReq->sTarget);
	}
	pReq->iHeaderCount = pMsg->iHeaderCount < XHTTPD2_MAX_HEADERS ? pMsg->iHeaderCount : XHTTPD2_MAX_HEADERS;
	for ( uint32 i = 0; i < pReq->iHeaderCount; ++i ) {
		__xhttpd2CopyToken(pReq->arrHeaders[i].sName, sizeof(pReq->arrHeaders[i].sName), pMsg->arrHeaders[i].sName);
		__xhttpd2CopyToken(pReq->arrHeaders[i].sValue, sizeof(pReq->arrHeaders[i].sValue), pMsg->arrHeaders[i].sValue);
	}
	iBodyBytes = xrtCodecHttp1BodyBytes(pFrame);
	if ( (pMsg->iFlags & XCODEC_HTTP1_F_CHUNKED) != 0u ) pReq->iContentLength = (int64_t)iBodyBytes;
	if ( iBodyBytes > 0u ) {
		pReq->pBody = (char*)XNET_ALLOC(iBodyBytes + 1u);
		if ( !pReq->pBody ) {
			xrtHttpd2RequestUnit(pReq);
			return false;
		}
		pReq->iBodyLen = xrtCodecHttp1CopyBody(pChain, pFrame, pReq->pBody, iBodyBytes);
		pReq->pBody[pReq->iBodyLen] = '\0';
	}
	return true;
}

static bool __xhttpd2BuildResponseBytes(const xhttpd2response* pResp, char** ppOut, size_t* pOutLen)
{
	char* pBuf = NULL;
	size_t iLen = 0;
	size_t iCap = 0;
	char aLine[512];
	const char* sReason;
	bool bChunked;
	if ( !pResp || !ppOut || !pOutLen ) return false;
	sReason = pResp->sReason[0] ? pResp->sReason : __xhttpd2StatusText(pResp->iStatusCode);
	bChunked = __xhttpd2ContainsTokenNoCase(xrtHttpd2ResponseHeader(pResp, "Transfer-Encoding"), "chunked");
	snprintf(aLine, sizeof(aLine), "HTTP/1.1 %u %s\r\n", (unsigned)pResp->iStatusCode, sReason);
	if ( !__xhttpd2AppendText(&pBuf, &iLen, &iCap, aLine) ) goto fail;
	if ( !__xhttpd2ResponseHasHeader(pResp, "Connection") ) {
		if ( !__xhttpd2AppendText(&pBuf, &iLen, &iCap, "Connection: close\r\n") ) goto fail;
	}
	if ( !bChunked && !__xhttpd2ResponseHasHeader(pResp, "Content-Length") ) {
		snprintf(aLine, sizeof(aLine), "Content-Length: %llu\r\n", (unsigned long long)pResp->iBodyLen);
		if ( !__xhttpd2AppendText(&pBuf, &iLen, &iCap, aLine) ) goto fail;
	}
	for ( uint32 i = 0; i < pResp->iHeaderCount; ++i ) {
		if ( bChunked && __xhttpd2StrEqNoCase(pResp->arrHeaders[i].sName, "Content-Length") ) continue;
		snprintf(aLine, sizeof(aLine), "%s: %s\r\n", pResp->arrHeaders[i].sName, pResp->arrHeaders[i].sValue);
		if ( !__xhttpd2AppendText(&pBuf, &iLen, &iCap, aLine) ) goto fail;
	}
	if ( !__xhttpd2AppendText(&pBuf, &iLen, &iCap, "\r\n") ) goto fail;
	if ( bChunked ) {
		snprintf(aLine, sizeof(aLine), "%llX\r\n", (unsigned long long)pResp->iBodyLen);
		if ( !__xhttpd2AppendText(&pBuf, &iLen, &iCap, aLine) ) goto fail;
		if ( pResp->pBody && pResp->iBodyLen > 0 ) {
			if ( !__xhttpd2AppendBytes(&pBuf, &iLen, &iCap, pResp->pBody, pResp->iBodyLen) ) goto fail;
		}
		if ( !__xhttpd2AppendText(&pBuf, &iLen, &iCap, "\r\n0\r\n\r\n") ) goto fail;
	} else if ( pResp->pBody && pResp->iBodyLen > 0 ) {
		if ( !__xhttpd2AppendBytes(&pBuf, &iLen, &iCap, pResp->pBody, pResp->iBodyLen) ) goto fail;
	}
	*ppOut = pBuf;
	*pOutLen = iLen;
	return true;

fail:
	if ( pBuf ) XNET_FREE(pBuf);
	return false;
}

static void __xhttpd2ServerAddConn(xhttpd2server* pServer, xhttpd2conn* pConn)
{
	if ( !pServer || !pConn ) return;
	__xhttpd2Lock(&pServer->iConnLock);
	pConn->pNext = pServer->pConnHead;
	pServer->pConnHead = pConn;
	__xhttpd2Unlock(&pServer->iConnLock);
}

static void __xhttpd2ServerRemoveConn(xhttpd2server* pServer, xhttpd2conn* pConn)
{
	xhttpd2conn** ppNode;
	if ( !pServer || !pConn ) return;
	__xhttpd2Lock(&pServer->iConnLock);
	ppNode = &pServer->pConnHead;
	while ( *ppNode ) {
		if ( *ppNode == pConn ) {
			*ppNode = pConn->pNext;
			break;
		}
		ppNode = &(*ppNode)->pNext;
	}
	pConn->pNext = NULL;
	__xhttpd2Unlock(&pServer->iConnLock);
}

static xhttpd2conn* __xhttpd2ServerDetachAllConns(xhttpd2server* pServer)
{
	xhttpd2conn* pHead;
	if ( !pServer ) return NULL;
	__xhttpd2Lock(&pServer->iConnLock);
	pHead = pServer->pConnHead;
	pServer->pConnHead = NULL;
	__xhttpd2Unlock(&pServer->iConnLock);
	return pHead;
}

static void __xhttpd2ConnCleanupTask(xnetworker* pWorker, ptr pArg)
{
	xhttpd2conn* pConn = (xhttpd2conn*)pArg;
	(void)pWorker;
	if ( !pConn ) return;
	if ( pConn->pServer ) {
		__xhttpd2ServerRemoveConn(pConn->pServer, pConn);
		pConn->pServer = NULL;
	}
	if ( pConn->pStream ) {
		xrtNetStreamDestroy(pConn->pStream);
		pConn->pStream = NULL;
	}
	XNET_FREE(pConn);
}

static void __xhttpd2ConnPostCleanup(xhttpd2conn* pConn)
{
	if ( !pConn ) return;
	if ( __xhttpd2AtomicCompareExchange(&pConn->iCleanupPosted, 1, 0) != 0 ) return;
	if ( pConn->pServer && pConn->pServer->pEngine && pConn->pStream && pConn->pStream->pWorker ) {
		if ( xrtNetEnginePost(pConn->pServer->pEngine, pConn->pStream->pWorker->iId, __xhttpd2ConnCleanupTask, pConn) == XRT_NET_OK ) {
			return;
		}
	}
	__xhttpd2ConnCleanupTask(NULL, pConn);
}

static void __xhttpd2EmitServerError(xhttpd2server* pServer, xhttpd2conn* pConn, int iSysErr)
{
	if ( pServer && pServer->tEvents.OnError ) {
		pServer->tEvents.OnError(pServer->pUserData, pServer, pConn, iSysErr);
	}
}

static bool __xhttpd2SendResponseAndClose(xhttpd2conn* pConn, const xhttpd2response* pResp)
{
	char* pBytes = NULL;
	size_t iLen = 0;
	xnetstream* pStream;
	if ( !pConn || !pConn->pStream || !pResp ) return false;
	pStream = pConn->pStream;
	if ( !__xhttpd2BuildResponseBytes(pResp, &pBytes, &iLen) ) return false;
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
	if ( (pResp->iFlags & XHTTPD2_RESP_F_CLOSE) != 0 ) {
		xrtNetStreamClose(pStream, XNET_CLOSE_F_GRACEFUL);
	}
	return true;
}

static void __xhttpd2SendSimpleStatus(xhttpd2conn* pConn, uint32 iStatusCode, const char* sBody)
{
	xhttpd2response tResp;
	xrtHttpd2ResponseInit(&tResp);
	xrtHttpd2ResponseSetStatus(&tResp, iStatusCode, NULL);
	if ( sBody && sBody[0] ) {
		(void)xrtHttpd2ResponseSetBodyCopy(&tResp, sBody, strlen(sBody), "text/plain");
	}
	(void)__xhttpd2SendResponseAndClose(pConn, &tResp);
	xrtHttpd2ResponseUnit(&tResp);
}

static bool __xhttpd2ListenerOnAccept(ptr pOwner, xnetlistener* pListener, xnetstream* pStream)
{
	xhttpd2server* pServer = (xhttpd2server*)pOwner;
	xhttpd2conn* pConn;
	(void)pListener;
	if ( !pServer || !pStream ) return false;
	pConn = (xhttpd2conn*)xrtNetStreamGetUserData(pStream);
	if ( !pConn ) return false;
	pConn->pServer = pServer;
	pConn->pStream = pStream;
	__xhttpd2ServerAddConn(pServer, pConn);
	return true;
}

static void __xhttpd2StreamOnOpen(ptr pOwner, xnetstream* pStream)
{
	xhttpd2conn* pConn = (xhttpd2conn*)pOwner;
	xhttpd2server* pServer = pConn ? pConn->pServer : NULL;
	(void)pStream;
	if ( pServer && pServer->tEvents.OnOpen ) {
		pServer->tEvents.OnOpen(pServer->pUserData, pServer, pConn);
	}
}

static void __xhttpd2StreamOnRecv(ptr pOwner, xnetstream* pStream, xnetchain* pChain)
{
	xhttpd2conn* pConn = (xhttpd2conn*)pOwner;
	xhttpd2server* pServer = pConn ? pConn->pServer : NULL;
	xcodecframe tFrame;
	xcodechttp1msg tMsg;
	xcodecstatus iParse;
	xhttpd2request tReq;
	xhttpd2response tResp;
	bool bHandled = false;
	if ( !pConn || !pServer || !pStream || !pChain ) return;
	if ( __xhttpd2AtomicLoad(&pConn->iCleanupPosted) != 0 ) return;
	if ( pConn->bResponseInFlight ) return;
	iParse = xrtCodecHttp1Parse(pChain, &tFrame, &tMsg);
	if ( iParse == XCODEC_STATUS_NEED_MORE ) return;
	if ( iParse == XCODEC_STATUS_ERROR ) {
		__xhttpd2EmitServerError(pServer, pConn, -1);
		__xhttpd2SendSimpleStatus(pConn, 400u, "Bad Request");
		return;
	}
	if ( !__xhttpd2BuildRequest(&tFrame, &tMsg, pChain, &tReq) ) {
		__xhttpd2EmitServerError(pServer, pConn, -1);
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return;
	}
	xrtHttpd2ResponseInit(&tResp);
	if ( pServer->tEvents.OnRequest ) {
		bHandled = pServer->tEvents.OnRequest(pServer->pUserData, pServer, pConn, &tReq, &tResp);
	}
	if ( !bHandled ) {
		xrtHttpd2ResponseSetStatus(&tResp, 404u, NULL);
		(void)xrtHttpd2ResponseSetBodyCopy(&tResp, "Not Found", 9, "text/plain");
	}
	if ( (tReq.iFlags & XHTTPD2_REQ_F_KEEPALIVE) != 0 && !__xhttpd2ResponseHasHeader(&tResp, "Connection") ) {
		tResp.iFlags &= ~XHTTPD2_RESP_F_CLOSE;
		(void)xrtHttpd2ResponseSetHeader(&tResp, "Connection", "keep-alive");
	}
	xrtCodecFrameConsume(pChain, &tFrame);
	pConn->bKeepAlive = ((tReq.iFlags & XHTTPD2_REQ_F_KEEPALIVE) != 0) && ((tResp.iFlags & XHTTPD2_RESP_F_CLOSE) == 0u);
	pConn->bResponseInFlight = true;
	if ( !__xhttpd2SendResponseAndClose(pConn, &tResp) ) {
		__xhttpd2EmitServerError(pServer, pConn, -1);
	}
	xrtHttpd2RequestUnit(&tReq);
	xrtHttpd2ResponseUnit(&tResp);
}

static void __xhttpd2StreamOnDrain(ptr pOwner, xnetstream* pStream)
{
	xhttpd2conn* pConn = (xhttpd2conn*)pOwner;
	if ( !pConn || !pStream ) return;
	if ( __xhttpd2AtomicLoad(&pConn->iCleanupPosted) != 0 ) return;
	if ( !pConn->bResponseInFlight ) return;
	if ( !pConn->bKeepAlive || pStream->bClosing ) return;
	pConn->bResponseInFlight = false;
	if ( xrtNetChainBytes(&pStream->tRxChain) > 0u ) {
		__xhttpd2StreamOnRecv(pOwner, pStream, &pStream->tRxChain);
	}
}

static void __xhttpd2StreamOnClose(ptr pOwner, xnetstream* pStream, xnet_result iReason)
{
	xhttpd2conn* pConn = (xhttpd2conn*)pOwner;
	xhttpd2server* pServer = pConn ? pConn->pServer : NULL;
	(void)pStream;
	if ( pServer && pServer->tEvents.OnClose ) {
		pServer->tEvents.OnClose(pServer->pUserData, pServer, pConn, iReason);
	}
	__xhttpd2ConnPostCleanup(pConn);
}

static void __xhttpd2StreamOnError(ptr pOwner, xnetstream* pStream, int iSysErr)
{
	xhttpd2conn* pConn = (xhttpd2conn*)pOwner;
	xhttpd2server* pServer = pConn ? pConn->pServer : NULL;
	(void)pStream;
	__xhttpd2EmitServerError(pServer, pConn, iSysErr);
}

static const xnetlistenerevents* __xhttpd2ListenerEvents(void)
{
	static const xnetlistenerevents tEvents = {
		__xhttpd2ListenerOnAccept,
		NULL
	};
	return &tEvents;
}

static const xnetstreamevents* __xhttpd2StreamEvents(void)
{
	static const xnetstreamevents tEvents = {
		__xhttpd2StreamOnOpen,
		__xhttpd2StreamOnRecv,
		__xhttpd2StreamOnDrain,
		__xhttpd2StreamOnClose,
		__xhttpd2StreamOnError,
		NULL,
		NULL
	};
	return &tEvents;
}

#if defined(_WIN32) || defined(_WIN64)
static DWORD WINAPI __xhttpd2AcceptThread(LPVOID pArg)
#else
static void* __xhttpd2AcceptThread(void* pArg)
#endif
{
	xhttpd2server* pServer = (xhttpd2server*)pArg;
	while ( pServer && __xhttpd2AtomicLoad(&pServer->bRunning) != 0 ) {
		xhttpd2conn* pConn = NULL;
		if ( pServer->pListener && pServer->pListener->bRunning ) {
			pConn = (xhttpd2conn*)XNET_ALLOC(sizeof(xhttpd2conn));
			if ( pConn ) {
				xnetstream* pStream;
				memset(pConn, 0, sizeof(xhttpd2conn));
				pConn->pServer = pServer;
				pStream = __xnetListenerTryAcceptOne(pServer->pListener, pConn);
				if ( !pStream ) {
					XNET_FREE(pConn);
				}
			}
		}
		#if defined(_WIN32) || defined(_WIN64)
			Sleep(pServer && pServer->tConfig.iAcceptPollMs > 0 ? pServer->tConfig.iAcceptPollMs : 5u);
		#else
			usleep((useconds_t)((pServer && pServer->tConfig.iAcceptPollMs > 0 ? pServer->tConfig.iAcceptPollMs : 5u) * 1000u));
		#endif
	}
	#if !defined(_WIN32) && !defined(_WIN64)
		return NULL;
	#endif
}

static xhttpd2server* xrtHttpd2Create(xnetengine* pEngine, const xhttpd2config* pCfg, const xhttpd2events* pEvents, ptr pUserData)
{
	xhttpd2server* pServer;
	if ( !pEngine ) return NULL;
	pServer = (xhttpd2server*)XNET_ALLOC(sizeof(xhttpd2server));
	if ( !pServer ) return NULL;
	memset(pServer, 0, sizeof(xhttpd2server));
	pServer->pEngine = pEngine;
	if ( pCfg ) {
		pServer->tConfig = *pCfg;
	} else {
		xrtHttpd2ConfigInit(&pServer->tConfig);
	}
	if ( pEvents ) pServer->tEvents = *pEvents;
	pServer->pUserData = pUserData;
	return pServer;
}

static uint16 xrtHttpd2BoundPort(const xhttpd2server* pServer)
{
	return (pServer && pServer->pListener) ? pServer->pListener->tConfig.tBindAddr.iPort : 0u;
}

static xnet_result xrtHttpd2Start(xhttpd2server* pServer)
{
	xnetlistenconfig tListenCfg;
	if ( !pServer || !pServer->pEngine ) return XRT_NET_ERROR;
	if ( __xhttpd2AtomicLoad(&pServer->bRunning) != 0 ) return XRT_NET_OK;
	xrtNetListenConfigInit(&tListenCfg);
	tListenCfg.tBindAddr = pServer->tConfig.tBindAddr;
	tListenCfg.iFlags = pServer->tConfig.iFlags;
	tListenCfg.iBacklog = pServer->tConfig.iBacklog;
	tListenCfg.iRecvLimit = pServer->tConfig.iRecvLimit;
	tListenCfg.pTlsConfig = pServer->tConfig.pTlsConfig;
	pServer->pListener = xrtNetListenerCreate(pServer->pEngine, &tListenCfg, __xhttpd2ListenerEvents(), __xhttpd2StreamEvents(), pServer);
	if ( !pServer->pListener ) return XRT_NET_ERROR;
	if ( xrtNetListenerStart(pServer->pListener) != XRT_NET_OK ) {
		xrtNetListenerDestroy(pServer->pListener);
		pServer->pListener = NULL;
		return XRT_NET_ERROR;
	}
	(void)__xhttpd2AtomicCompareExchange(&pServer->bRunning, 1, 0);
	#if defined(_WIN32) || defined(_WIN64)
		pServer->hAcceptThread = CreateThread(NULL, 0, __xhttpd2AcceptThread, pServer, 0, NULL);
		if ( !pServer->hAcceptThread ) {
			(void)__xhttpd2AtomicCompareExchange(&pServer->bRunning, 0, 1);
			xrtNetListenerStop(pServer->pListener);
			xrtNetListenerDestroy(pServer->pListener);
			pServer->pListener = NULL;
			return XRT_NET_ERROR;
		}
	#else
		if ( pthread_create(&pServer->hAcceptThread, NULL, __xhttpd2AcceptThread, pServer) != 0 ) {
			(void)__xhttpd2AtomicCompareExchange(&pServer->bRunning, 0, 1);
			xrtNetListenerStop(pServer->pListener);
			xrtNetListenerDestroy(pServer->pListener);
			pServer->pListener = NULL;
			return XRT_NET_ERROR;
		}
		pServer->bAcceptThreadStarted = true;
	#endif
	return XRT_NET_OK;
}

static void xrtHttpd2Stop(xhttpd2server* pServer)
{
	xhttpd2conn* pConn;
	if ( !pServer ) return;
	if ( __xhttpd2AtomicCompareExchange(&pServer->bRunning, 0, 1) == 0 ) {
		/* already stopped */
	} else {
		#if defined(_WIN32) || defined(_WIN64)
			if ( pServer->hAcceptThread ) {
				WaitForSingleObject(pServer->hAcceptThread, INFINITE);
				CloseHandle(pServer->hAcceptThread);
				pServer->hAcceptThread = NULL;
			}
		#else
			if ( pServer->bAcceptThreadStarted ) {
				pthread_join(pServer->hAcceptThread, NULL);
				pServer->bAcceptThreadStarted = false;
			}
		#endif
	}
	if ( pServer->pListener ) {
		xrtNetListenerStop(pServer->pListener);
		xrtNetListenerDestroy(pServer->pListener);
		pServer->pListener = NULL;
	}
	pConn = __xhttpd2ServerDetachAllConns(pServer);
	while ( pConn ) {
		xhttpd2conn* pNext = pConn->pNext;
		pConn->pNext = NULL;
		(void)__xhttpd2AtomicCompareExchange(&pConn->iCleanupPosted, 1, 0);
		if ( pConn->pStream ) {
			xrtNetStreamClose(pConn->pStream, XNET_CLOSE_F_ABORT);
			xrtNetStreamDestroy(pConn->pStream);
			pConn->pStream = NULL;
		}
		XNET_FREE(pConn);
		pConn = pNext;
	}
}

static void xrtHttpd2Destroy(xhttpd2server* pServer)
{
	if ( !pServer ) return;
	xrtHttpd2Stop(pServer);
	XNET_FREE(pServer);
}

#endif
