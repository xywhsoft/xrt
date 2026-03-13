#ifndef XRT_XHTTP2_H
#define XRT_XHTTP2_H

#include "xcodec_http1.h"
#include "xnet_stream.h"
#include "xnet_sync.h"

/*
    XNet V2 - HTTP Client Rebuild Skeleton

    Phase-3 scope in this header:
      - request and response objects
      - one-shot async HTTP/1.1 client transaction on xnet_stream
      - sync facade over the same async transport core

    Current limitations:
      - one request per connection, default Connection: keep-alive
      - whole-body chunked transfer is supported, but trailers are metadata-only
      - streaming bodies are still deferred to later protocol work
*/

#define XHTTP2_METHOD_CAP         16u
#define XHTTP2_URL_CAP            1024u
#define XHTTP2_HOST_CAP           256u
#define XHTTP2_PATH_CAP           1024u
#define XHTTP2_HEADER_NAME_CAP    64u
#define XHTTP2_HEADER_VALUE_CAP   256u
#define XHTTP2_MAX_HEADERS        32u

#define XHTTP2_RESP_F_NONE        0x00000000u
#define XHTTP2_RESP_F_CHUNKED     0x00000001u
#define XHTTP2_RESP_F_KEEPALIVE   0x00000002u
#define XHTTP2_RESP_F_UPGRADE     0x00000004u

typedef struct {
	char sName[XHTTP2_HEADER_NAME_CAP];
	char sValue[XHTTP2_HEADER_VALUE_CAP];
} xhttp2header;

typedef struct {
	bool bHttps;
	uint16 iPort;
	char sHost[XHTTP2_HOST_CAP];
	char sPath[XHTTP2_PATH_CAP];
} xhttp2url;

typedef struct {
	char sMethod[XHTTP2_METHOD_CAP];
	char sURL[XHTTP2_URL_CAP];
	xhttp2url tURL;
	xhttp2header arrHeaders[XHTTP2_MAX_HEADERS];
	uint32 iHeaderCount;
	char* pBody;
	size_t iBodyLen;
	uint32 iTimeoutMs;
	bool bVerifyPeer;
} xhttp2request;

typedef struct {
	uint32 iStatusCode;
	uint32 iFlags;
	uint32 iHeaderCount;
	int64_t iContentLength;
	char sVersion[XCODEC_HTTP1_TOKEN_CAP];
	char sReason[XCODEC_HTTP1_REASON_CAP];
	xhttp2header arrHeaders[XHTTP2_MAX_HEADERS];
	char* pBody;
	size_t iBodyLen;
} xhttp2response;

typedef struct {
	volatile long iRefCount;
	volatile long iCleanupPosted;
	volatile long iComplete;
	xnetengine* pEngine;
	struct __xhttp2_conn* pConn;
	xnetstream* pStream;
	xnetfuture* pFuture;
	xhttp2request tReq;
	char* pSendBuf;
	size_t iSendLen;
	int iLastSysErr;
	xtlsconfig tTlsCfg;
} __xhttp2_tx;

typedef struct __xhttp2_conn {
	struct __xhttp2_conn* pNext;
	volatile long iCleanupPosted;
	xnetengine* pEngine;
	xnetstream* pStream;
	__xhttp2_tx* pTx;
	char sHost[XHTTP2_HOST_CAP];
	uint16 iPort;
	bool bHttps;
	bool bVerifyPeer;
	bool bOpen;
	bool bIdle;
	int iLastSysErr;
	xtlsconfig tTlsCfg;
} __xhttp2_conn;

static volatile long __g_xhttp2PoolLock = 0;
static __xhttp2_conn* __g_xhttp2IdleConnHead = NULL;

static char __xhttp2ToLower(char ch)
{
	if ( ch >= 'A' && ch <= 'Z' ) return (char)(ch + 32);
	return ch;
}

static bool __xhttp2StrEqNoCase(const char* sA, const char* sB)
{
	size_t i = 0;
	if ( !sA || !sB ) return false;
	while ( sA[i] && sB[i] ) {
		if ( __xhttp2ToLower(sA[i]) != __xhttp2ToLower(sB[i]) ) return false;
		i++;
	}
	return sA[i] == '\0' && sB[i] == '\0';
}

static long __xhttp2AtomicAdd(volatile long* pValue, long iDelta)
{
	return __xnetAtomicAddFetch32(pValue, iDelta);
}

static long __xhttp2AtomicCompareExchange(volatile long* pValue, long iExchange, long iComparand)
{
	return __xnetAtomicCompareExchange32(pValue, iExchange, iComparand);
}

static long __xhttp2AtomicLoad(volatile long* pValue)
{
	return __xnetAtomicLoad32(pValue);
}

static void __xhttp2CopyToken(char* sDst, size_t iDstCap, const char* sSrc)
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

static bool __xhttp2AppendBytes(char** ppBuf, size_t* pLen, size_t* pCap, const void* pData, size_t iDataLen)
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

static bool __xhttp2AppendText(char** ppBuf, size_t* pLen, size_t* pCap, const char* sText)
{
	return __xhttp2AppendBytes(ppBuf, pLen, pCap, sText, sText ? strlen(sText) : 0);
}

static bool __xhttp2RequestHasHeader(const xhttp2request* pReq, const char* sName)
{
	if ( !pReq || !sName ) return false;
	for ( uint32 i = 0; i < pReq->iHeaderCount; ++i ) {
		if ( __xhttp2StrEqNoCase(pReq->arrHeaders[i].sName, sName) ) return true;
	}
	return false;
}

static const char* __xhttp2RequestHeaderValue(const xhttp2request* pReq, const char* sName)
{
	if ( !pReq || !sName ) return NULL;
	for ( uint32 i = 0; i < pReq->iHeaderCount; ++i ) {
		if ( __xhttp2StrEqNoCase(pReq->arrHeaders[i].sName, sName) ) return pReq->arrHeaders[i].sValue;
	}
	return NULL;
}

static bool __xhttp2StatusHasNoBody(uint32 iStatusCode)
{
	return (iStatusCode >= 100u && iStatusCode < 200u) || iStatusCode == 204u || iStatusCode == 304u;
}

static bool __xhttp2UrlParse(const char* sURL, xhttp2url* pOut)
{
	const char* p;
	const char* pHostStart;
	const char* pPortStart = NULL;
	size_t iHostLen;
	size_t iPathLen;
	if ( !sURL || !pOut ) return false;
	memset(pOut, 0, sizeof(xhttp2url));
	p = sURL;
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
	pHostStart = p;
	while ( *p && *p != '/' && *p != '?' ) {
		if ( *p == ':' ) pPortStart = p + 1;
		p++;
	}
	if ( pPortStart ) {
		iHostLen = (size_t)((pPortStart - 1) - pHostStart);
		if ( iHostLen == 0 ) return false;
		if ( iHostLen >= sizeof(pOut->sHost) ) iHostLen = sizeof(pOut->sHost) - 1u;
		memcpy(pOut->sHost, pHostStart, iHostLen);
		pOut->sHost[iHostLen] = '\0';
		pOut->iPort = (uint16)atoi(pPortStart);
	} else {
		iHostLen = (size_t)(p - pHostStart);
		if ( iHostLen == 0 ) return false;
		if ( iHostLen >= sizeof(pOut->sHost) ) iHostLen = sizeof(pOut->sHost) - 1u;
		memcpy(pOut->sHost, pHostStart, iHostLen);
		pOut->sHost[iHostLen] = '\0';
	}
	if ( *p == '/' || *p == '?' ) {
		iPathLen = strlen(p);
		if ( iPathLen >= sizeof(pOut->sPath) ) iPathLen = sizeof(pOut->sPath) - 1u;
		memcpy(pOut->sPath, p, iPathLen);
		pOut->sPath[iPathLen] = '\0';
	} else {
		strcpy(pOut->sPath, "/");
	}
	return pOut->sHost[0] != '\0';
}

static bool __xhttp2RequestClone(xhttp2request* pDst, const xhttp2request* pSrc);
static void __xhttp2RequestUnitInternal(xhttp2request* pReq);
static bool __xhttp2BuildRequestBytes(const xhttp2request* pReq, char** ppOut, size_t* pOutLen);
static xhttp2response* __xhttp2BuildResponse(const xcodecframe* pFrame, const xcodechttp1msg* pMsg, const xnetchain* pChain);
static void __xhttp2ConnPostCleanup(__xhttp2_conn* pConn);

static void __xhttp2PoolLockAcquire(void)
{
	while ( __sync_lock_test_and_set(&__g_xhttp2PoolLock, 1) != 0 ) {
		__xnetEngineSleepMs(1u);
	}
}

static void __xhttp2PoolLockRelease(void)
{
	__sync_lock_release(&__g_xhttp2PoolLock);
}

static bool __xhttp2ContainsTokenNoCase(const char* sValue, const char* sToken)
{
	size_t iTokenLen;
	size_t i;
	if ( !sValue || !sToken || sToken[0] == '\0' ) return false;
	iTokenLen = strlen(sToken);
	while ( *sValue ) {
		while ( *sValue == ' ' || *sValue == '\t' || *sValue == ',' ) sValue++;
		for ( i = 0; i < iTokenLen; ++i ) {
			if ( sValue[i] == '\0' || __xhttp2ToLower(sValue[i]) != __xhttp2ToLower(sToken[i]) ) break;
		}
		if ( i == iTokenLen ) {
			char ch = sValue[iTokenLen];
			if ( ch == '\0' || ch == ',' || ch == ' ' || ch == '\t' ) return true;
		}
		while ( *sValue && *sValue != ',' ) sValue++;
	}
	return false;
}

static bool __xhttp2RequestWantsClose(const xhttp2request* pReq)
{
	if ( !pReq ) return false;
	for ( uint32 i = 0; i < pReq->iHeaderCount; ++i ) {
		if ( __xhttp2StrEqNoCase(pReq->arrHeaders[i].sName, "Connection") ) {
			return __xhttp2ContainsTokenNoCase(pReq->arrHeaders[i].sValue, "close");
		}
	}
	return false;
}

static bool __xhttp2ResponseWantsClose(const xcodechttp1msg* pMsg)
{
	const char* sConn;
	if ( !pMsg ) return false;
	sConn = xrtCodecHttp1GetHeader(pMsg, "Connection");
	return sConn ? __xhttp2ContainsTokenNoCase(sConn, "close") : false;
}

static bool __xhttp2ConnMatchesReq(const __xhttp2_conn* pConn, xnetengine* pEngine, const xhttp2request* pReq)
{
	if ( !pConn || !pReq || !pEngine ) return false;
	if ( pConn->pEngine != pEngine ) return false;
	if ( pConn->iPort != pReq->tURL.iPort ) return false;
	if ( pConn->bHttps != pReq->tURL.bHttps ) return false;
	if ( pConn->bVerifyPeer != pReq->bVerifyPeer ) return false;
	if ( strcmp(pConn->sHost, pReq->tURL.sHost) != 0 ) return false;
	if ( !pConn->bOpen || pConn->pStream == NULL || pConn->pTx != NULL ) return false;
	return true;
}

static __xhttp2_conn* __xhttp2PoolTake(xnetengine* pEngine, const xhttp2request* pReq)
{
	__xhttp2_conn** ppConn;
	__xhttp2_conn* pConn = NULL;
	if ( !pEngine || !pReq ) return NULL;
	__xhttp2PoolLockAcquire();
	ppConn = &__g_xhttp2IdleConnHead;
	while ( *ppConn ) {
		if ( __xhttp2ConnMatchesReq(*ppConn, pEngine, pReq) ) {
			pConn = *ppConn;
			*ppConn = pConn->pNext;
			pConn->pNext = NULL;
			pConn->bIdle = false;
			break;
		}
		ppConn = &(*ppConn)->pNext;
	}
	__xhttp2PoolLockRelease();
	return pConn;
}

static bool __xhttp2PoolPut(__xhttp2_conn* pConn)
{
	if ( !pConn || !pConn->pStream || !pConn->bOpen || pConn->pTx != NULL ) return false;
	__xhttp2PoolLockAcquire();
	pConn->pNext = __g_xhttp2IdleConnHead;
	__g_xhttp2IdleConnHead = pConn;
	pConn->bIdle = true;
	__xhttp2PoolLockRelease();
	return true;
}

static void __xhttp2PoolRemove(__xhttp2_conn* pConn)
{
	__xhttp2_conn** ppCur;
	if ( !pConn ) return;
	__xhttp2PoolLockAcquire();
	ppCur = &__g_xhttp2IdleConnHead;
	while ( *ppCur ) {
		if ( *ppCur == pConn ) {
			*ppCur = pConn->pNext;
			pConn->pNext = NULL;
			pConn->bIdle = false;
			break;
		}
		ppCur = &(*ppCur)->pNext;
	}
	__xhttp2PoolLockRelease();
}

static void xrtHttp2CloseIdleConnections(xnetengine* pEngine)
{
	__xhttp2_conn* pList = NULL;
	__xhttp2_conn** ppTail = &pList;
	__xhttp2_conn** ppCur;
	__xhttp2PoolLockAcquire();
	ppCur = &__g_xhttp2IdleConnHead;
	while ( *ppCur ) {
		__xhttp2_conn* pConn = *ppCur;
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
	__xhttp2PoolLockRelease();
	while ( pList ) {
		__xhttp2_conn* pNext = pList->pNext;
		pList->pNext = NULL;
		if ( pList->pStream ) {
			xrtNetStreamClose(pList->pStream, XNET_CLOSE_F_ABORT);
		}
		__xhttp2ConnPostCleanup(pList);
		pList = pNext;
	}
}

static void xrtHttp2RequestInit(xhttp2request* pReq)
{
	if ( !pReq ) return;
	memset(pReq, 0, sizeof(xhttp2request));
	strcpy(pReq->sMethod, "GET");
	pReq->iTimeoutMs = 30000u;
	pReq->bVerifyPeer = true;
}

static void __xhttp2RequestUnitInternal(xhttp2request* pReq)
{
	if ( !pReq ) return;
	if ( pReq->pBody ) {
		XNET_FREE(pReq->pBody);
		pReq->pBody = NULL;
	}
	pReq->iBodyLen = 0;
}

static void xrtHttp2RequestUnit(xhttp2request* pReq)
{
	if ( !pReq ) return;
	__xhttp2RequestUnitInternal(pReq);
	memset(pReq, 0, sizeof(xhttp2request));
}

static bool xrtHttp2RequestSetMethod(xhttp2request* pReq, const char* sMethod)
{
	size_t iLen;
	if ( !pReq || !sMethod || !sMethod[0] ) return false;
	iLen = strlen(sMethod);
	if ( iLen >= sizeof(pReq->sMethod) ) iLen = sizeof(pReq->sMethod) - 1u;
	memcpy(pReq->sMethod, sMethod, iLen);
	pReq->sMethod[iLen] = '\0';
	return true;
}

static bool xrtHttp2RequestSetURL(xhttp2request* pReq, const char* sURL)
{
	size_t iLen;
	if ( !pReq || !sURL || !sURL[0] ) return false;
	if ( !__xhttp2UrlParse(sURL, &pReq->tURL) ) return false;
	iLen = strlen(sURL);
	if ( iLen >= sizeof(pReq->sURL) ) iLen = sizeof(pReq->sURL) - 1u;
	memcpy(pReq->sURL, sURL, iLen);
	pReq->sURL[iLen] = '\0';
	return true;
}

static bool xrtHttp2RequestSetHeader(xhttp2request* pReq, const char* sName, const char* sValue)
{
	xhttp2header* pHeader;
	if ( !pReq || !sName || !sValue ) return false;
	for ( uint32 i = 0; i < pReq->iHeaderCount; ++i ) {
		if ( __xhttp2StrEqNoCase(pReq->arrHeaders[i].sName, sName) ) {
			__xhttp2CopyToken(pReq->arrHeaders[i].sValue, sizeof(pReq->arrHeaders[i].sValue), sValue);
			return true;
		}
	}
	if ( pReq->iHeaderCount >= XHTTP2_MAX_HEADERS ) return false;
	pHeader = &pReq->arrHeaders[pReq->iHeaderCount++];
	__xhttp2CopyToken(pHeader->sName, sizeof(pHeader->sName), sName);
	__xhttp2CopyToken(pHeader->sValue, sizeof(pHeader->sValue), sValue);
	return true;
}

static bool xrtHttp2RequestSetBodyCopy(xhttp2request* pReq, const void* pData, size_t iLen, const char* sContentType)
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
		return xrtHttp2RequestSetHeader(pReq, "Content-Type", sContentType);
	}
	return true;
}

static void xrtHttp2RequestSetTimeout(xhttp2request* pReq, uint32 iTimeoutMs)
{
	if ( !pReq ) return;
	pReq->iTimeoutMs = iTimeoutMs;
}

static void xrtHttp2RequestSetVerifyPeer(xhttp2request* pReq, bool bVerifyPeer)
{
	if ( !pReq ) return;
	pReq->bVerifyPeer = bVerifyPeer;
}

static void xrtHttp2ResponseDestroy(xhttp2response* pResp)
{
	if ( !pResp ) return;
	if ( pResp->pBody ) {
		XNET_FREE(pResp->pBody);
		pResp->pBody = NULL;
	}
	XNET_FREE(pResp);
}

static const char* xrtHttp2ResponseHeader(const xhttp2response* pResp, const char* sName)
{
	if ( !pResp || !sName ) return NULL;
	for ( uint32 i = 0; i < pResp->iHeaderCount; ++i ) {
		if ( __xhttp2StrEqNoCase(pResp->arrHeaders[i].sName, sName) ) {
			return pResp->arrHeaders[i].sValue;
		}
	}
	return NULL;
}

static bool __xhttp2RequestClone(xhttp2request* pDst, const xhttp2request* pSrc)
{
	if ( !pDst || !pSrc ) return false;
	xrtHttp2RequestInit(pDst);
	memcpy(pDst->sMethod, pSrc->sMethod, sizeof(pDst->sMethod));
	memcpy(pDst->sURL, pSrc->sURL, sizeof(pDst->sURL));
	memcpy(&pDst->tURL, &pSrc->tURL, sizeof(pDst->tURL));
	memcpy(pDst->arrHeaders, pSrc->arrHeaders, sizeof(pDst->arrHeaders));
	pDst->iHeaderCount = pSrc->iHeaderCount;
	pDst->iTimeoutMs = pSrc->iTimeoutMs;
	pDst->bVerifyPeer = pSrc->bVerifyPeer;
	if ( pSrc->pBody && pSrc->iBodyLen > 0 ) {
		pDst->pBody = (char*)XNET_ALLOC(pSrc->iBodyLen);
		if ( !pDst->pBody ) {
			__xhttp2RequestUnitInternal(pDst);
			return false;
		}
		memcpy(pDst->pBody, pSrc->pBody, pSrc->iBodyLen);
		pDst->iBodyLen = pSrc->iBodyLen;
	}
	return true;
}

static bool __xhttp2BuildRequestBytes(const xhttp2request* pReq, char** ppOut, size_t* pOutLen)
{
	char* pBuf = NULL;
	size_t iLen = 0;
	size_t iCap = 0;
	char aLine[512];
	bool bDefaultPort;
	bool bChunked;

	if ( !pReq || !ppOut || !pOutLen || pReq->tURL.sHost[0] == '\0' || pReq->sMethod[0] == '\0' ) return false;
	*ppOut = NULL;
	*pOutLen = 0;
	bChunked = __xhttp2ContainsTokenNoCase(__xhttp2RequestHeaderValue(pReq, "Transfer-Encoding"), "chunked");

	snprintf(aLine, sizeof(aLine), "%s %s HTTP/1.1\r\n",
		pReq->sMethod,
		pReq->tURL.sPath[0] ? pReq->tURL.sPath : "/");
	if ( !__xhttp2AppendText(&pBuf, &iLen, &iCap, aLine) ) goto fail;

	bDefaultPort = (pReq->tURL.bHttps && pReq->tURL.iPort == 443u) || (!pReq->tURL.bHttps && pReq->tURL.iPort == 80u);
	if ( !__xhttp2RequestHasHeader(pReq, "Host") ) {
		if ( bDefaultPort ) {
			snprintf(aLine, sizeof(aLine), "Host: %s\r\n", pReq->tURL.sHost);
		} else {
			snprintf(aLine, sizeof(aLine), "Host: %s:%u\r\n", pReq->tURL.sHost, (unsigned)pReq->tURL.iPort);
		}
		if ( !__xhttp2AppendText(&pBuf, &iLen, &iCap, aLine) ) goto fail;
	}
	if ( !__xhttp2RequestHasHeader(pReq, "Connection") ) {
		if ( !__xhttp2AppendText(&pBuf, &iLen, &iCap, "Connection: keep-alive\r\n") ) goto fail;
	}
	if ( !bChunked && pReq->iBodyLen > 0 && !__xhttp2RequestHasHeader(pReq, "Content-Length") ) {
		snprintf(aLine, sizeof(aLine), "Content-Length: %llu\r\n", (unsigned long long)pReq->iBodyLen);
		if ( !__xhttp2AppendText(&pBuf, &iLen, &iCap, aLine) ) goto fail;
	}
	for ( uint32 i = 0; i < pReq->iHeaderCount; ++i ) {
		if ( bChunked && __xhttp2StrEqNoCase(pReq->arrHeaders[i].sName, "Content-Length") ) continue;
		snprintf(aLine, sizeof(aLine), "%s: %s\r\n", pReq->arrHeaders[i].sName, pReq->arrHeaders[i].sValue);
		if ( !__xhttp2AppendText(&pBuf, &iLen, &iCap, aLine) ) goto fail;
	}
	if ( !__xhttp2AppendText(&pBuf, &iLen, &iCap, "\r\n") ) goto fail;
	if ( bChunked ) {
		snprintf(aLine, sizeof(aLine), "%llX\r\n", (unsigned long long)pReq->iBodyLen);
		if ( !__xhttp2AppendText(&pBuf, &iLen, &iCap, aLine) ) goto fail;
		if ( pReq->pBody && pReq->iBodyLen > 0 ) {
			if ( !__xhttp2AppendBytes(&pBuf, &iLen, &iCap, pReq->pBody, pReq->iBodyLen) ) goto fail;
		}
		if ( !__xhttp2AppendText(&pBuf, &iLen, &iCap, "\r\n0\r\n\r\n") ) goto fail;
	} else if ( pReq->pBody && pReq->iBodyLen > 0 ) {
		if ( !__xhttp2AppendBytes(&pBuf, &iLen, &iCap, pReq->pBody, pReq->iBodyLen) ) goto fail;
	}

	*ppOut = pBuf;
	*pOutLen = iLen;
	return true;

fail:
	if ( pBuf ) XNET_FREE(pBuf);
	return false;
}

static xhttp2response* __xhttp2BuildResponse(const xcodecframe* pFrame, const xcodechttp1msg* pMsg, const xnetchain* pChain)
{
	xhttp2response* pResp;
	size_t iBodyBytes;
	if ( !pFrame || !pMsg || !pChain ) return NULL;
	pResp = (xhttp2response*)XNET_ALLOC(sizeof(xhttp2response));
	if ( !pResp ) return NULL;
	memset(pResp, 0, sizeof(xhttp2response));
	pResp->iStatusCode = pMsg->iStatusCode;
	pResp->iContentLength = pMsg->iContentLength;
	__xhttp2CopyToken(pResp->sVersion, sizeof(pResp->sVersion), pMsg->sVersion);
	__xhttp2CopyToken(pResp->sReason, sizeof(pResp->sReason), pMsg->sReason);
	if ( pMsg->iFlags & XCODEC_HTTP1_F_CHUNKED ) pResp->iFlags |= XHTTP2_RESP_F_CHUNKED;
	if ( pMsg->iFlags & XCODEC_HTTP1_F_KEEPALIVE ) pResp->iFlags |= XHTTP2_RESP_F_KEEPALIVE;
	if ( pMsg->iFlags & XCODEC_HTTP1_F_UPGRADE ) pResp->iFlags |= XHTTP2_RESP_F_UPGRADE;
	pResp->iHeaderCount = pMsg->iHeaderCount < XHTTP2_MAX_HEADERS ? pMsg->iHeaderCount : XHTTP2_MAX_HEADERS;
	for ( uint32 i = 0; i < pResp->iHeaderCount; ++i ) {
		__xhttp2CopyToken(pResp->arrHeaders[i].sName, sizeof(pResp->arrHeaders[i].sName), pMsg->arrHeaders[i].sName);
		__xhttp2CopyToken(pResp->arrHeaders[i].sValue, sizeof(pResp->arrHeaders[i].sValue), pMsg->arrHeaders[i].sValue);
	}
	iBodyBytes = xrtCodecHttp1BodyBytes(pFrame);
	if ( (pMsg->iFlags & XCODEC_HTTP1_F_CHUNKED) != 0u ) pResp->iContentLength = (int64_t)iBodyBytes;
	if ( iBodyBytes > 0u ) {
		pResp->pBody = (char*)XNET_ALLOC(iBodyBytes + 1u);
		if ( !pResp->pBody ) {
			xrtHttp2ResponseDestroy(pResp);
			return NULL;
		}
		pResp->iBodyLen = xrtCodecHttp1CopyBody(pChain, pFrame, pResp->pBody, iBodyBytes);
		pResp->pBody[pResp->iBodyLen] = '\0';
	}
	return pResp;
}

static void __xhttp2TxAddRef(__xhttp2_tx* pTx)
{
	if ( !pTx ) return;
	(void)__xhttp2AtomicAdd(&pTx->iRefCount, 1);
}

static void __xhttp2TxDiscardTransient(__xhttp2_tx* pTx)
{
	if ( !pTx ) return;
	if ( pTx->pSendBuf ) {
		XNET_FREE(pTx->pSendBuf);
		pTx->pSendBuf = NULL;
	}
	pTx->iSendLen = 0;
	__xhttp2RequestUnitInternal(&pTx->tReq);
}

static bool __xhttp2TxComplete(__xhttp2_tx* pTx, xnet_result iStatus, xhttp2response* pResp)
{
	if ( !pTx ) {
		if ( pResp ) xrtHttp2ResponseDestroy(pResp);
		return false;
	}
	if ( __xhttp2AtomicCompareExchange(&pTx->iComplete, 1, 0) != 0 ) {
		if ( pResp ) xrtHttp2ResponseDestroy(pResp);
		return false;
	}
	if ( pTx->pFuture ) {
		(void)__xnetFutureResolve(pTx->pFuture, iStatus, pResp);
	} else if ( pResp ) {
		xrtHttp2ResponseDestroy(pResp);
	}
	return true;
}

static void __xhttp2TxRelease(__xhttp2_tx* pTx)
{
	if ( !pTx ) return;
	if ( __xhttp2AtomicAdd(&pTx->iRefCount, -1) == 0 ) {
		__xhttp2TxDiscardTransient(pTx);
		pTx->pConn = NULL;
		pTx->pStream = NULL;
		XNET_FREE(pTx);
	}
}

static void __xhttp2TxCleanupTask(xnetworker* pWorker, ptr pArg)
{
	__xhttp2_tx* pTx = (__xhttp2_tx*)pArg;
	(void)pWorker;
	if ( !pTx ) return;
	pTx->pConn = NULL;
	pTx->pStream = NULL;
	__xhttp2TxDiscardTransient(pTx);
	__xhttp2TxRelease(pTx);
}

static void __xhttp2TxPostCleanup(__xhttp2_tx* pTx)
{
	uint32 iAffinity = 0u;
	if ( !pTx || !pTx->pEngine ) return;
	if ( __xhttp2AtomicCompareExchange(&pTx->iCleanupPosted, 1, 0) != 0 ) return;
	if ( pTx->pStream && pTx->pStream->pWorker ) iAffinity = pTx->pStream->pWorker->iId;
	else if ( pTx->pConn && pTx->pConn->pStream && pTx->pConn->pStream->pWorker ) iAffinity = pTx->pConn->pStream->pWorker->iId;
	if ( xrtNetEnginePost(pTx->pEngine, iAffinity, __xhttp2TxCleanupTask, pTx) != XRT_NET_OK ) {
		__xhttp2TxCleanupTask(NULL, pTx);
	}
}

static void __xhttp2ConnCleanupTask(xnetworker* pWorker, ptr pArg)
{
	__xhttp2_conn* pConn = (__xhttp2_conn*)pArg;
	(void)pWorker;
	if ( !pConn ) return;
	__xhttp2PoolRemove(pConn);
	if ( pConn->pStream ) {
		xrtNetStreamDestroy(pConn->pStream);
		pConn->pStream = NULL;
	}
	XNET_FREE(pConn);
}

static void __xhttp2ConnPostCleanup(__xhttp2_conn* pConn)
{
	if ( !pConn ) return;
	if ( __xhttp2AtomicCompareExchange(&pConn->iCleanupPosted, 1, 0) != 0 ) return;
	__xhttp2PoolRemove(pConn);
	if ( pConn->pEngine && pConn->pStream && pConn->pStream->pWorker ) {
		if ( xrtNetEnginePost(pConn->pEngine, pConn->pStream->pWorker->iId, __xhttp2ConnCleanupTask, pConn) == XRT_NET_OK ) {
			return;
		}
	}
	__xhttp2ConnCleanupTask(NULL, pConn);
}

static bool __xhttp2ResponseReusable(const __xhttp2_tx* pTx, const xcodechttp1msg* pMsg)
{
	if ( !pTx || !pMsg || !pTx->pConn ) return false;
	if ( (pMsg->iFlags & XCODEC_HTTP1_F_KEEPALIVE) == 0u ) return false;
	if ( (pMsg->iFlags & XCODEC_HTTP1_F_UPGRADE) != 0u ) return false;
	if ( __xhttp2RequestWantsClose(&pTx->tReq) ) return false;
	if ( __xhttp2ResponseWantsClose(pMsg) ) return false;
	return pTx->pConn->pStream != NULL && pTx->pConn->bOpen;
}

static bool __xhttp2ConnSendActiveTx(__xhttp2_conn* pConn)
{
	__xhttp2_tx* pTx;
	if ( !pConn || !pConn->pStream ) return false;
	pTx = pConn->pTx;
	if ( !pTx || !pTx->pSendBuf || pTx->iSendLen == 0u ) return false;
	if ( xrtNetStreamSend(pConn->pStream, pTx->pSendBuf, pTx->iSendLen) != XRT_NET_OK ) {
		(void)__xhttp2TxComplete(pTx, XRT_NET_ERROR, NULL);
		xrtNetStreamClose(pConn->pStream, XNET_CLOSE_F_ABORT);
		return false;
	}
	return true;
}

static void __xhttp2TxTimeoutTask(xnetworker* pWorker, ptr pArg)
{
	__xhttp2_tx* pTx = (__xhttp2_tx*)pArg;
	(void)pWorker;
	if ( !pTx ) return;
	if ( __xhttp2AtomicLoad(&pTx->iComplete) == 0 ) {
		(void)__xhttp2TxComplete(pTx, XRT_NET_TIMEOUT, NULL);
		if ( pTx->pConn && pTx->pConn->pStream ) xrtNetStreamClose(pTx->pConn->pStream, XNET_CLOSE_F_ABORT);
		else if ( pTx->pStream ) xrtNetStreamClose(pTx->pStream, XNET_CLOSE_F_ABORT);
	}
	__xhttp2TxRelease(pTx);
}

static void __xhttp2ClientOnOpen(ptr pOwner, xnetstream* pStream)
{
	__xhttp2_conn* pConn = (__xhttp2_conn*)pOwner;
	if ( !pConn || !pStream ) return;
	pConn->bOpen = true;
	(void)__xhttp2ConnSendActiveTx(pConn);
}

static void __xhttp2ClientOnRecv(ptr pOwner, xnetstream* pStream, xnetchain* pChain)
{
	__xhttp2_conn* pConn = (__xhttp2_conn*)pOwner;
	__xhttp2_tx* pTx = pConn ? pConn->pTx : NULL;
	xcodecframe tFrame;
	xcodechttp1msg tMsg;
	xcodecstatus iParse;
	bool bNoBodyExpected;
	bool bReusable;
	xhttp2response* pResp;
	if ( !pTx || !pStream || !pChain ) return;
	iParse = xrtCodecHttp1Parse(pChain, &tFrame, &tMsg);
	if ( iParse == XCODEC_STATUS_NEED_MORE ) return;
	if ( iParse == XCODEC_STATUS_ERROR ) {
		(void)__xhttp2TxComplete(pTx, XRT_NET_ERROR, NULL);
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return;
	}
	bNoBodyExpected = __xhttp2StatusHasNoBody(tMsg.iStatusCode) || __xhttp2StrEqNoCase(pTx->tReq.sMethod, "HEAD");
	if ( (tMsg.iFlags & XCODEC_HTTP1_F_CHUNKED) == 0u && (tMsg.iContentLength < 0 && !bNoBodyExpected) ) {
		(void)__xhttp2TxComplete(pTx, XRT_NET_ERROR, NULL);
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return;
	}
	pResp = __xhttp2BuildResponse(&tFrame, &tMsg, pChain);
	if ( !pResp ) {
		(void)__xhttp2TxComplete(pTx, XRT_NET_ERROR, NULL);
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return;
	}
	xrtCodecFrameConsume(pChain, &tFrame);
	bReusable = __xhttp2ResponseReusable(pTx, &tMsg);
	(void)__xhttp2TxComplete(pTx, XRT_NET_OK, pResp);
	if ( bReusable && pConn ) {
		pConn->pTx = NULL;
		pTx->pConn = NULL;
		pTx->pStream = NULL;
		(void)__xhttp2PoolPut(pConn);
		__xhttp2TxPostCleanup(pTx);
		return;
	}
	xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
}

static void __xhttp2ClientOnClose(ptr pOwner, xnetstream* pStream, xnet_result iReason)
{
	__xhttp2_conn* pConn = (__xhttp2_conn*)pOwner;
	__xhttp2_tx* pTx = pConn ? pConn->pTx : NULL;
	(void)pStream;
	if ( !pConn ) return;
	pConn->bOpen = false;
	if ( pConn->bIdle ) __xhttp2PoolRemove(pConn);
	pConn->bIdle = false;
	pConn->pStream = NULL;
	pConn->pTx = NULL;
	if ( pTx ) {
		pTx->pConn = NULL;
		pTx->pStream = NULL;
	}
	if ( pTx && __xhttp2AtomicLoad(&pTx->iComplete) == 0 ) {
		(void)__xhttp2TxComplete(pTx, iReason == XRT_NET_CLOSED ? XRT_NET_CLOSED : XRT_NET_ERROR, NULL);
	}
	if ( pTx ) __xhttp2TxPostCleanup(pTx);
	__xhttp2ConnPostCleanup(pConn);
}

static void __xhttp2ClientOnError(ptr pOwner, xnetstream* pStream, int iSysErr)
{
	__xhttp2_conn* pConn = (__xhttp2_conn*)pOwner;
	__xhttp2_tx* pTx = pConn ? pConn->pTx : NULL;
	(void)pStream;
	if ( pConn ) pConn->iLastSysErr = iSysErr;
	if ( pTx ) pTx->iLastSysErr = iSysErr;
}

static const xnetstreamevents* __xhttp2ClientEvents(void)
{
	static const xnetstreamevents tEvents = {
		__xhttp2ClientOnOpen,
		__xhttp2ClientOnRecv,
		NULL,
		__xhttp2ClientOnClose,
		__xhttp2ClientOnError,
		NULL,
		NULL
	};
	return &tEvents;
}

static xnetfuture* xrtHttp2ExecuteAsync(xnetengine* pEngine, const xhttp2request* pReq)
{
	__xhttp2_tx* pTx;
	__xhttp2_conn* pConn = NULL;
	xnetfuture* pFuture;
	xnetconnectconfig tConnCfg;
	xnetengine* pResolvedEngine;

	if ( !pReq ) return NULL;
	pResolvedEngine = __xnetSyncResolveEngine(pEngine);
	if ( !pResolvedEngine ) return NULL;
	pFuture = xrtNetFutureCreate();
	if ( !pFuture ) return NULL;

	pTx = (__xhttp2_tx*)XNET_ALLOC(sizeof(__xhttp2_tx));
	if ( !pTx ) {
		xrtNetFutureDestroy(pFuture);
		return NULL;
	}
	memset(pTx, 0, sizeof(__xhttp2_tx));
	pTx->iRefCount = 1;
	pTx->pEngine = pResolvedEngine;
	pTx->pFuture = pFuture;
	if ( !__xhttp2RequestClone(&pTx->tReq, pReq) ) {
		(void)__xnetFutureResolve(pFuture, XRT_NET_ERROR, NULL);
		__xhttp2TxRelease(pTx);
		return pFuture;
	}
	if ( pTx->tReq.tURL.sHost[0] == '\0' ) {
		if ( pTx->tReq.sURL[0] == '\0' || !__xhttp2UrlParse(pTx->tReq.sURL, &pTx->tReq.tURL) ) {
			(void)__xnetFutureResolve(pFuture, XRT_NET_ERROR, NULL);
			__xhttp2TxRelease(pTx);
			return pFuture;
		}
	}
	if ( !__xhttp2BuildRequestBytes(&pTx->tReq, &pTx->pSendBuf, &pTx->iSendLen) ) {
		(void)__xnetFutureResolve(pFuture, XRT_NET_ERROR, NULL);
		__xhttp2TxRelease(pTx);
		return pFuture;
	}

	pConn = __xhttp2PoolTake(pResolvedEngine, &pTx->tReq);
	if ( !pConn ) {
		pConn = (__xhttp2_conn*)XNET_ALLOC(sizeof(__xhttp2_conn));
		if ( !pConn ) {
			(void)__xnetFutureResolve(pFuture, XRT_NET_ERROR, NULL);
			__xhttp2TxRelease(pTx);
			return pFuture;
		}
		memset(pConn, 0, sizeof(__xhttp2_conn));
		pConn->pEngine = pResolvedEngine;
		__xhttp2CopyToken(pConn->sHost, sizeof(pConn->sHost), pTx->tReq.tURL.sHost);
		pConn->iPort = pTx->tReq.tURL.iPort;
		pConn->bHttps = pTx->tReq.tURL.bHttps;
		pConn->bVerifyPeer = pTx->tReq.bVerifyPeer;
		pConn->pStream = xrtNetStreamCreate(pResolvedEngine, __xhttp2ClientEvents(), pConn);
		if ( !pConn->pStream ) {
			(void)__xnetFutureResolve(pFuture, XRT_NET_ERROR, NULL);
			XNET_FREE(pConn);
			__xhttp2TxRelease(pTx);
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
		tConnCfg.iConnectTimeoutMs = pTx->tReq.iTimeoutMs;
		tConnCfg.iRecvLimit = 1024u * 1024u;
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
			XNET_FREE(pConn);
			__xhttp2TxRelease(pTx);
			return pFuture;
		}
	} else if ( !__xhttp2ConnSendActiveTx(pConn) ) {
		return pFuture;
	}

	if ( pTx->tReq.iTimeoutMs > 0 ) {
		__xhttp2TxAddRef(pTx);
		if ( xrtNetEnginePostDelayed(pResolvedEngine, pTx->pStream && pTx->pStream->pWorker ? pTx->pStream->pWorker->iId : 0, pTx->tReq.iTimeoutMs, __xhttp2TxTimeoutTask, pTx) != XRT_NET_OK ) {
			__xhttp2TxRelease(pTx);
		}
	}
	return pFuture;
}

static xhttp2response* xrtHttp2ExecuteSync(xnetengine* pEngine, const xhttp2request* pReq, xnet_result* pStatus)
{
	xnetfuture* pFuture;
	xnet_result iStatus;
	xhttp2response* pResp;
	if ( pStatus ) *pStatus = XRT_NET_ERROR;
	pFuture = xrtHttp2ExecuteAsync(pEngine, pReq);
	if ( !pFuture ) return NULL;
	iStatus = xrtNetFutureWait(pFuture, XNET_WAIT_INFINITE);
	pResp = (iStatus == XRT_NET_OK) ? (xhttp2response*)xrtNetFutureValue(pFuture) : NULL;
	if ( pStatus ) *pStatus = iStatus;
	xrtNetFutureDestroy(pFuture);
	return pResp;
}

#endif
