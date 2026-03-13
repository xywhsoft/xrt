#ifndef XRT_XWS2_H
#define XRT_XWS2_H

#include <stdlib.h>

#include "xcodec_http1.h"
#include "xcodec_ws.h"
#include "xnet_stream.h"

#if !defined(_WIN32) && !defined(_WIN64)
	#include <sched.h>
#endif


/*
    XNet V2 - WebSocket Rebuild Skeleton

    Phase-3 scope in this header:
      - async WebSocket client and server wrappers on top of xnet_stream
      - HTTP upgrade handshake over plain TCP and builtin TLS
      - single-frame text/binary messaging plus ping/pong and close control

    Current limitations:
      - message fragmentation and continuation frames are rejected
      - extensions and permessage-deflate are not implemented
      - server subprotocol negotiation is fixed-string and optional
*/


/* ============================== Public model ============================== */

#define XWS2_URL_CAP              1024u
#define XWS2_HOST_CAP             256u
#define XWS2_PATH_CAP             1024u
#define XWS2_ORIGIN_CAP           256u
#define XWS2_PROTOCOL_CAP         128u
#define XWS2_CLOSE_REASON_CAP     123u

#define XWS2_CLOSE_NORMAL         1000u
#define XWS2_CLOSE_GOING_AWAY     1001u
#define XWS2_CLOSE_PROTOCOL       1002u
#define XWS2_CLOSE_UNSUPPORTED    1003u
#define XWS2_CLOSE_TOO_BIG        1009u
#define XWS2_CLOSE_INTERNAL       1011u

typedef struct xrt_ws2_client xws2client;
typedef struct xrt_ws2_server xws2server;
typedef struct xrt_ws2_conn   xws2conn;

typedef struct {
	char sURL[XWS2_URL_CAP];
	char sOrigin[XWS2_ORIGIN_CAP];
	char sProtocol[XWS2_PROTOCOL_CAP];
	uint32 iConnectTimeoutMs;
	uint32 iRecvLimit;
	bool bVerifyPeer;
} xws2clientconfig;

typedef struct {
	xnetaddr tBindAddr;
	uint32 iFlags;
	uint32 iBacklog;
	uint32 iRecvLimit;
	uint32 iAcceptPollMs;
	const xtlsconfig* pTlsConfig;
	char sProtocol[XWS2_PROTOCOL_CAP];
} xws2serverconfig;

typedef struct {
	void (*OnOpen)(ptr pOwner, xws2client* pClient);
	void (*OnText)(ptr pOwner, xws2client* pClient, const char* pData, size_t iLen);
	void (*OnBinary)(ptr pOwner, xws2client* pClient, const void* pData, size_t iLen);
	void (*OnClose)(ptr pOwner, xws2client* pClient, xnet_result iReason);
	void (*OnError)(ptr pOwner, xws2client* pClient, int iSysErr);
	void (*OnPing)(ptr pOwner, xws2client* pClient, const void* pData, size_t iLen);
	void (*OnPong)(ptr pOwner, xws2client* pClient, const void* pData, size_t iLen);
} xws2clientevents;

typedef struct {
	void (*OnOpen)(ptr pOwner, xws2server* pServer, xws2conn* pConn);
	void (*OnText)(ptr pOwner, xws2server* pServer, xws2conn* pConn, const char* pData, size_t iLen);
	void (*OnBinary)(ptr pOwner, xws2server* pServer, xws2conn* pConn, const void* pData, size_t iLen);
	void (*OnClose)(ptr pOwner, xws2server* pServer, xws2conn* pConn, xnet_result iReason);
	void (*OnError)(ptr pOwner, xws2server* pServer, xws2conn* pConn, int iSysErr);
	void (*OnPing)(ptr pOwner, xws2server* pServer, xws2conn* pConn, const void* pData, size_t iLen);
	void (*OnPong)(ptr pOwner, xws2server* pServer, xws2conn* pConn, const void* pData, size_t iLen);
} xws2serverevents;

typedef struct {
	bool bSecure;
	uint16 iPort;
	char sHost[XWS2_HOST_CAP];
	char sPath[XWS2_PATH_CAP];
} __xws2_url;

struct xrt_ws2_client {
	xnetengine* pEngine;
	xnetstream* pStream;
	xws2clientconfig tConfig;
	xws2clientevents tEvents;
	ptr pUserData;
	__xws2_url tURL;
	xtlsconfig tTlsCfg;
	char sKey[32];
	char sExpectedAccept[64];
	char* pMsgBuf;
	size_t iMsgLen;
	size_t iMsgCap;
	uint8 iMsgOpcode;
	volatile long iOpen;
	volatile long iClosePosted;
	int iLastSysErr;
};

struct xrt_ws2_conn {
	struct xrt_ws2_conn* pNext;
	volatile long iCleanupPosted;
	volatile long iOpen;
	volatile long iClosePosted;
	xws2server* pServer;
	xnetstream* pStream;
	char sProtocol[XWS2_PROTOCOL_CAP];
	char* pMsgBuf;
	size_t iMsgLen;
	size_t iMsgCap;
	uint8 iMsgOpcode;
	int iLastSysErr;
};

struct xrt_ws2_server {
	xnetengine* pEngine;
	xnetlistener* pListener;
	xws2serverconfig tConfig;
	xws2serverevents tEvents;
	ptr pUserData;
	volatile long iConnLock;
	volatile long bRunning;
	xws2conn* pConnHead;
#if defined(_WIN32) || defined(_WIN64)
	HANDLE hAcceptThread;
#else
	pthread_t hAcceptThread;
	bool bAcceptThreadStarted;
#endif
};


/* ============================== Legacy crypto hooks ============================== */

extern void xrtRandomBytes(uint8* pBuf, size_t iLen);
extern void xrtSHA1(const ptr pData, size_t iLen, uint8* pOut);


/* ============================== Local helpers ============================== */

#define __XWS2_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

typedef struct {
	xnetstream* pStream;
	char* pFrame;
	size_t iFrameLen;
} __xws2_close_task;

#define __XWS2_APPEND_OK       0
#define __XWS2_APPEND_TOO_BIG  1
#define __XWS2_APPEND_PROTOCOL 2
#define __XWS2_APPEND_INTERNAL 3

static char __xws2ToLower(char ch)
{
	if ( ch >= 'A' && ch <= 'Z' ) return (char)(ch + 32);
	return ch;
}

static bool __xws2StrEqNoCase(const char* sA, const char* sB)
{
	size_t i = 0;
	if ( !sA || !sB ) return false;
	while ( sA[i] && sB[i] ) {
		if ( __xws2ToLower(sA[i]) != __xws2ToLower(sB[i]) ) return false;
		i++;
	}
	return sA[i] == '\0' && sB[i] == '\0';
}

static bool __xws2ContainsTokenNoCase(const char* sText, const char* sToken)
{
	const char* p;
	size_t iTokenLen;
	if ( !sText || !sToken || sToken[0] == '\0' ) return false;
	iTokenLen = strlen(sToken);
	p = sText;
	while ( *p ) {
		const char* pStart;
		size_t iLen;
		while ( *p == ' ' || *p == '\t' || *p == ',' ) p++;
		pStart = p;
		while ( *p && *p != ',' ) p++;
		iLen = (size_t)(p - pStart);
		while ( iLen > 0 && (pStart[iLen - 1] == ' ' || pStart[iLen - 1] == '\t') ) iLen--;
		if ( iLen == iTokenLen ) {
			size_t i;
			bool bMatch = true;
			for ( i = 0; i < iLen; ++i ) {
				if ( __xws2ToLower(pStart[i]) != __xws2ToLower(sToken[i]) ) {
					bMatch = false;
					break;
				}
			}
			if ( bMatch ) return true;
		}
		if ( *p == ',' ) p++;
	}
	return false;
}

static void __xws2CopyToken(char* sDst, size_t iDstCap, const char* sSrc)
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

static long __xws2AtomicCompareExchange(volatile long* pValue, long iExchange, long iComparand)
{
	#if defined(_WIN32) || defined(_WIN64)
		return InterlockedCompareExchange((volatile LONG*)pValue, iExchange, iComparand);
	#else
		return __sync_val_compare_and_swap(pValue, iComparand, iExchange);
	#endif
}

static long __xws2AtomicLoad(volatile long* pValue)
{
	return __xws2AtomicCompareExchange(pValue, 0, 0);
}

static long __xws2AtomicLoadConst(const volatile long* pValue)
{
	return __xws2AtomicCompareExchange((volatile long*)pValue, 0, 0);
}

static void __xws2Sleep0(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(0);
	#else
		sched_yield();
	#endif
}

static void __xws2SleepMs(uint32 iDelayMs)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(iDelayMs);
	#else
		usleep((useconds_t)iDelayMs * 1000u);
	#endif
}

static void __xws2Lock(volatile long* pLock)
{
	if ( !pLock ) return;
	while ( __xws2AtomicCompareExchange(pLock, 1, 0) != 0 ) {
		__xws2Sleep0();
	}
}

static void __xws2Unlock(volatile long* pLock)
{
	if ( !pLock ) return;
	(void)__xws2AtomicCompareExchange(pLock, 0, 1);
}

static bool __xws2AppendBytes(char** ppBuf, size_t* pLen, size_t* pCap, const void* pData, size_t iDataLen)
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

static bool __xws2AppendText(char** ppBuf, size_t* pLen, size_t* pCap, const char* sText)
{
	return __xws2AppendBytes(ppBuf, pLen, pCap, sText, sText ? strlen(sText) : 0);
}

static const char* __xws2HttpStatusText(uint32 iStatusCode)
{
	switch ( iStatusCode ) {
		case 101: return "Switching Protocols";
		case 400: return "Bad Request";
		case 404: return "Not Found";
		case 426: return "Upgrade Required";
		case 500: return "Internal Server Error";
		default: return "OK";
	}
}

static bool __xws2Base64Encode(const uint8* pIn, size_t iLen, char* sOut, size_t iOutCap)
{
	static const char aTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	size_t iNeed = 4u * ((iLen + 2u) / 3u);
	size_t i = 0;
	size_t j = 0;
	if ( !pIn || !sOut || iOutCap <= iNeed ) return false;
	while ( i + 2u < iLen ) {
		uint32 iTri = ((uint32)pIn[i] << 16u) | ((uint32)pIn[i + 1u] << 8u) | (uint32)pIn[i + 2u];
		sOut[j++] = aTable[(iTri >> 18u) & 0x3Fu];
		sOut[j++] = aTable[(iTri >> 12u) & 0x3Fu];
		sOut[j++] = aTable[(iTri >> 6u) & 0x3Fu];
		sOut[j++] = aTable[iTri & 0x3Fu];
		i += 3u;
	}
	if ( i < iLen ) {
		uint32 iTri = (uint32)pIn[i] << 16u;
		sOut[j++] = aTable[(iTri >> 18u) & 0x3Fu];
		if ( i + 1u < iLen ) {
			iTri |= (uint32)pIn[i + 1u] << 8u;
			sOut[j++] = aTable[(iTri >> 12u) & 0x3Fu];
			sOut[j++] = aTable[(iTri >> 6u) & 0x3Fu];
			sOut[j++] = '=';
		} else {
			sOut[j++] = aTable[(iTri >> 12u) & 0x3Fu];
			sOut[j++] = '=';
			sOut[j++] = '=';
		}
	}
	sOut[j] = '\0';
	return true;
}

static bool __xws2GenerateKey(char* sKey, size_t iCap)
{
	uint8 aRandom[16];
	if ( !sKey || iCap < 25u ) return false;
	xrtRandomBytes(aRandom, sizeof(aRandom));
	return __xws2Base64Encode(aRandom, sizeof(aRandom), sKey, iCap);
}

static bool __xws2ComputeAccept(const char* sKey, char* sAccept, size_t iCap)
{
	char sCombined[128];
	uint8 aDigest[20];
	int iWritten;
	if ( !sKey || !sAccept || iCap < 29u ) return false;
	iWritten = snprintf(sCombined, sizeof(sCombined), "%s%s", sKey, __XWS2_GUID);
	if ( iWritten <= 0 || (size_t)iWritten >= sizeof(sCombined) ) return false;
	xrtSHA1(sCombined, (size_t)iWritten, aDigest);
	return __xws2Base64Encode(aDigest, sizeof(aDigest), sAccept, iCap);
}

static bool __xws2UrlParse(const char* sURL, __xws2_url* pOut)
{
	const char* p;
	const char* pHostStart;
	const char* pPathStart;
	size_t iHostLen;
	size_t iPathLen;
	if ( !sURL || !pOut ) return false;
	memset(pOut, 0, sizeof(__xws2_url));
	p = sURL;
	if ( strncmp(p, "wss://", 6) == 0 ) {
		pOut->bSecure = true;
		pOut->iPort = 443u;
		p += 6;
	} else if ( strncmp(p, "ws://", 5) == 0 ) {
		pOut->bSecure = false;
		pOut->iPort = 80u;
		p += 5;
	} else {
		return false;
	}
	pHostStart = p;
	if ( *p == '[' ) {
		pHostStart = ++p;
		while ( *p && *p != ']' ) p++;
		if ( *p != ']' ) return false;
		iHostLen = (size_t)(p - pHostStart);
		if ( iHostLen == 0 ) return false;
		if ( iHostLen >= sizeof(pOut->sHost) ) iHostLen = sizeof(pOut->sHost) - 1u;
		memcpy(pOut->sHost, pHostStart, iHostLen);
		pOut->sHost[iHostLen] = '\0';
		p++;
		if ( *p == ':' ) {
			pOut->iPort = (uint16)atoi(p + 1);
			while ( *p && *p != '/' && *p != '?' ) p++;
		}
	} else {
		while ( *p && *p != ':' && *p != '/' && *p != '?' ) p++;
		iHostLen = (size_t)(p - pHostStart);
		if ( iHostLen == 0 ) return false;
		if ( iHostLen >= sizeof(pOut->sHost) ) iHostLen = sizeof(pOut->sHost) - 1u;
		memcpy(pOut->sHost, pHostStart, iHostLen);
		pOut->sHost[iHostLen] = '\0';
		if ( *p == ':' ) {
			pOut->iPort = (uint16)atoi(p + 1);
			while ( *p && *p != '/' && *p != '?' ) p++;
		}
	}
	pPathStart = (*p == '/' || *p == '?') ? p : NULL;
	if ( pPathStart ) {
		iPathLen = strlen(pPathStart);
		if ( iPathLen >= sizeof(pOut->sPath) ) iPathLen = sizeof(pOut->sPath) - 1u;
		memcpy(pOut->sPath, pPathStart, iPathLen);
		pOut->sPath[iPathLen] = '\0';
	} else {
		strcpy(pOut->sPath, "/");
	}
	return pOut->sHost[0] != '\0' && pOut->iPort != 0u;
}

static void __xws2MakeHostHeader(const __xws2_url* pURL, char* sOut, size_t iOutCap)
{
	bool bDefaultPort;
	if ( !sOut || iOutCap == 0 ) return;
	sOut[0] = '\0';
	if ( !pURL ) return;
	bDefaultPort = (pURL->bSecure && pURL->iPort == 443u) || (!pURL->bSecure && pURL->iPort == 80u);
	if ( strchr(pURL->sHost, ':') != NULL ) {
		if ( bDefaultPort ) {
			(void)snprintf(sOut, iOutCap, "[%s]", pURL->sHost);
		} else {
			(void)snprintf(sOut, iOutCap, "[%s]:%u", pURL->sHost, (unsigned)pURL->iPort);
		}
	} else if ( bDefaultPort ) {
		(void)snprintf(sOut, iOutCap, "%s", pURL->sHost);
	} else {
		(void)snprintf(sOut, iOutCap, "%s:%u", pURL->sHost, (unsigned)pURL->iPort);
	}
}

static bool __xws2BuildClientHandshake(xws2client* pClient, char** ppOut, size_t* pOutLen)
{
	char sHost[384];
	char* pBuf = NULL;
	size_t iLen = 0;
	size_t iCap = 0;
	if ( !pClient || !ppOut || !pOutLen ) return false;
	__xws2MakeHostHeader(&pClient->tURL, sHost, sizeof(sHost));
	if ( !__xws2GenerateKey(pClient->sKey, sizeof(pClient->sKey)) ) return false;
	if ( !__xws2ComputeAccept(pClient->sKey, pClient->sExpectedAccept, sizeof(pClient->sExpectedAccept)) ) return false;
	if ( !__xws2AppendText(&pBuf, &iLen, &iCap, "GET ") ||
		!__xws2AppendText(&pBuf, &iLen, &iCap, pClient->tURL.sPath) ||
		!__xws2AppendText(&pBuf, &iLen, &iCap, " HTTP/1.1\r\nHost: ") ||
		!__xws2AppendText(&pBuf, &iLen, &iCap, sHost) ||
		!__xws2AppendText(&pBuf, &iLen, &iCap, "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: ") ||
		!__xws2AppendText(&pBuf, &iLen, &iCap, pClient->sKey) ||
		!__xws2AppendText(&pBuf, &iLen, &iCap, "\r\nSec-WebSocket-Version: 13\r\n") ) {
		XNET_FREE(pBuf);
		return false;
	}
	if ( pClient->tConfig.sProtocol[0] ) {
		if ( !__xws2AppendText(&pBuf, &iLen, &iCap, "Sec-WebSocket-Protocol: ") ||
			!__xws2AppendText(&pBuf, &iLen, &iCap, pClient->tConfig.sProtocol) ||
			!__xws2AppendText(&pBuf, &iLen, &iCap, "\r\n") ) {
			XNET_FREE(pBuf);
			return false;
		}
	}
	if ( pClient->tConfig.sOrigin[0] ) {
		if ( !__xws2AppendText(&pBuf, &iLen, &iCap, "Origin: ") ||
			!__xws2AppendText(&pBuf, &iLen, &iCap, pClient->tConfig.sOrigin) ||
			!__xws2AppendText(&pBuf, &iLen, &iCap, "\r\n") ) {
			XNET_FREE(pBuf);
			return false;
		}
	}
	if ( !__xws2AppendText(&pBuf, &iLen, &iCap, "\r\n") ) {
		XNET_FREE(pBuf);
		return false;
	}
	*ppOut = pBuf;
	*pOutLen = iLen;
	return true;
}

static bool __xws2BuildHttpResponseBytes(uint32 iStatusCode, const char* sBody, const char* sAccept, const char* sProtocol, char** ppOut, size_t* pOutLen)
{
	char sLine[256];
	char sLength[64];
	char* pBuf = NULL;
	size_t iLen = 0;
	size_t iCap = 0;
	size_t iBodyLen = sBody ? strlen(sBody) : 0u;
	if ( !ppOut || !pOutLen ) return false;
	(void)snprintf(sLine, sizeof(sLine), "HTTP/1.1 %u %s\r\n", (unsigned)iStatusCode, __xws2HttpStatusText(iStatusCode));
	if ( !__xws2AppendText(&pBuf, &iLen, &iCap, sLine) ) {
		XNET_FREE(pBuf);
		return false;
	}
	if ( iStatusCode == 101u ) {
		if ( !__xws2AppendText(&pBuf, &iLen, &iCap, "Upgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ") ||
			!__xws2AppendText(&pBuf, &iLen, &iCap, sAccept ? sAccept : "") ||
			!__xws2AppendText(&pBuf, &iLen, &iCap, "\r\n") ) {
			XNET_FREE(pBuf);
			return false;
		}
		if ( sProtocol && sProtocol[0] ) {
			if ( !__xws2AppendText(&pBuf, &iLen, &iCap, "Sec-WebSocket-Protocol: ") ||
				!__xws2AppendText(&pBuf, &iLen, &iCap, sProtocol) ||
				!__xws2AppendText(&pBuf, &iLen, &iCap, "\r\n") ) {
				XNET_FREE(pBuf);
				return false;
			}
		}
		if ( !__xws2AppendText(&pBuf, &iLen, &iCap, "\r\n") ) {
			XNET_FREE(pBuf);
			return false;
		}
	} else {
		(void)snprintf(sLength, sizeof(sLength), "%llu", (unsigned long long)iBodyLen);
		if ( !__xws2AppendText(&pBuf, &iLen, &iCap, "Connection: close\r\nContent-Type: text/plain\r\nContent-Length: ") ||
			!__xws2AppendText(&pBuf, &iLen, &iCap, sLength) ||
			!__xws2AppendText(&pBuf, &iLen, &iCap, "\r\n\r\n") ||
			!__xws2AppendBytes(&pBuf, &iLen, &iCap, sBody, iBodyLen) ) {
			XNET_FREE(pBuf);
			return false;
		}
	}
	*ppOut = pBuf;
	*pOutLen = iLen;
	return true;
}

static void __xws2MessageReset(char** ppBuf, size_t* pLen, size_t* pCap, uint8* pOpcode)
{
	if ( ppBuf && *ppBuf ) {
		XNET_FREE(*ppBuf);
		*ppBuf = NULL;
	}
	if ( pLen ) *pLen = 0u;
	if ( pCap ) *pCap = 0u;
	if ( pOpcode ) *pOpcode = 0u;
}

static int __xws2MessageAppend(char** ppBuf, size_t* pLen, size_t* pCap, uint8* pOpcode, uint8 iOpcode, const void* pPayload, size_t iPayloadLen, size_t iLimit)
{
	if ( !ppBuf || !pLen || !pCap || !pOpcode || (!pPayload && iPayloadLen != 0u) ) return __XWS2_APPEND_INTERNAL;
	if ( iLimit > 0u && *pLen + iPayloadLen > iLimit ) return __XWS2_APPEND_TOO_BIG;
	if ( iOpcode != 0u ) *pOpcode = iOpcode;
	if ( !__xws2AppendBytes(ppBuf, pLen, pCap, pPayload, iPayloadLen) ) return __XWS2_APPEND_INTERNAL;
	return __XWS2_APPEND_OK;
}

static size_t __xws2FrameSize(size_t iPayloadLen, bool bMask)
{
	size_t iSize = 2u + iPayloadLen;
	if ( iPayloadLen >= 126u && iPayloadLen <= 0xFFFFu ) iSize += 2u;
	else if ( iPayloadLen > 0xFFFFu ) iSize += 8u;
	if ( bMask ) iSize += 4u;
	return iSize;
}

static bool __xws2BuildFrameBytesEx(uint8 iOpcode, bool bFin, bool bMask, const void* pPayload, size_t iPayloadLen, char** ppOut, size_t* pOutLen)
{
	char* pBuf;
	size_t iNeed;
	size_t iPos = 0;
	uint8 aMask[4] = {0};
	const uint8* pBytes = (const uint8*)pPayload;
	if ( !ppOut || !pOutLen ) return false;
	iNeed = __xws2FrameSize(iPayloadLen, bMask);
	pBuf = (char*)XNET_ALLOC(iNeed);
	if ( !pBuf ) return false;
	((uint8*)pBuf)[iPos++] = (uint8)((bFin ? 0x80u : 0x00u) | (iOpcode & 0x0Fu));
	if ( iPayloadLen < 126u ) {
		((uint8*)pBuf)[iPos++] = (uint8)((bMask ? 0x80u : 0x00u) | (uint8)iPayloadLen);
	} else if ( iPayloadLen <= 0xFFFFu ) {
		((uint8*)pBuf)[iPos++] = (uint8)((bMask ? 0x80u : 0x00u) | 126u);
		((uint8*)pBuf)[iPos++] = (uint8)((iPayloadLen >> 8u) & 0xFFu);
		((uint8*)pBuf)[iPos++] = (uint8)(iPayloadLen & 0xFFu);
	} else {
		((uint8*)pBuf)[iPos++] = (uint8)((bMask ? 0x80u : 0x00u) | 127u);
		for ( int i = 7; i >= 0; --i ) {
			((uint8*)pBuf)[iPos++] = (uint8)((((uint64)iPayloadLen) >> (uint32)(i * 8)) & 0xFFu);
		}
	}
	if ( bMask ) {
		xrtRandomBytes(aMask, sizeof(aMask));
		memcpy(pBuf + iPos, aMask, sizeof(aMask));
		iPos += sizeof(aMask);
	}
	if ( iPayloadLen > 0 && pBytes ) {
		memcpy(pBuf + iPos, pBytes, iPayloadLen);
		if ( bMask ) {
			xrtCodecWsUnmask(pBuf + iPos, iPayloadLen, aMask, 0u);
		}
		iPos += iPayloadLen;
	}
	*ppOut = pBuf;
	*pOutLen = iPos;
	return true;
}

static bool __xws2BuildFrameBytes(uint8 iOpcode, bool bMask, const void* pPayload, size_t iPayloadLen, char** ppOut, size_t* pOutLen)
{
	return __xws2BuildFrameBytesEx(iOpcode, true, bMask, pPayload, iPayloadLen, ppOut, pOutLen);
}

static bool __xws2BuildClosePayload(uint16 iCode, const char* sReason, size_t iReasonLen, char** ppOut, size_t* pOutLen)
{
	char* pBuf;
	if ( !ppOut || !pOutLen ) return false;
	if ( iCode == 0 && (!sReason || iReasonLen == 0u) ) {
		*ppOut = NULL;
		*pOutLen = 0u;
		return true;
	}
	if ( iCode == 0u ) iCode = XWS2_CLOSE_NORMAL;
	if ( iReasonLen > XWS2_CLOSE_REASON_CAP ) iReasonLen = XWS2_CLOSE_REASON_CAP;
	pBuf = (char*)XNET_ALLOC(2u + iReasonLen);
	if ( !pBuf ) return false;
	pBuf[0] = (char)((iCode >> 8u) & 0xFFu);
	pBuf[1] = (char)(iCode & 0xFFu);
	if ( sReason && iReasonLen > 0 ) memcpy(pBuf + 2u, sReason, iReasonLen);
	*ppOut = pBuf;
	*pOutLen = 2u + iReasonLen;
	return true;
}

static bool __xws2StreamQueueBytesDirect(xnetstream* pStream, const void* pData, size_t iLen)
{
	if ( !pStream || !pData || iLen == 0u ) return false;
	if ( pStream->pTls ) {
		if ( !__xnetStreamAppendTlsPlainCopy(pStream, pData, iLen) ) return false;
	} else {
		if ( !__xnetStreamAppendSendCopy(pStream, pData, iLen) ) return false;
	}
	__xnetStreamKickWrite(pStream);
	return true;
}

static xnet_result __xws2StreamSendFrame(xnetstream* pStream, bool bMask, uint8 iOpcode, const void* pPayload, size_t iPayloadLen)
{
	char* pFrame = NULL;
	size_t iFrameLen = 0u;
	xnet_result iRet;
	if ( !pStream ) return XRT_NET_ERROR;
	if ( !__xws2BuildFrameBytesEx(iOpcode, true, bMask, pPayload, iPayloadLen, &pFrame, &iFrameLen) ) return XRT_NET_ERROR;
	iRet = xrtNetStreamSend(pStream, pFrame, iFrameLen);
	XNET_FREE(pFrame);
	return iRet;
}

static xnet_result __xws2StreamSendFrameEx(xnetstream* pStream, bool bFin, bool bMask, uint8 iOpcode, const void* pPayload, size_t iPayloadLen)
{
	char* pFrame = NULL;
	size_t iFrameLen = 0u;
	xnet_result iRet;
	if ( !pStream ) return XRT_NET_ERROR;
	if ( !__xws2BuildFrameBytesEx(iOpcode, bFin, bMask, pPayload, iPayloadLen, &pFrame, &iFrameLen) ) return XRT_NET_ERROR;
	iRet = xrtNetStreamSend(pStream, pFrame, iFrameLen);
	XNET_FREE(pFrame);
	return iRet;
}

static bool __xws2StreamQueueFrameDirectEx(xnetstream* pStream, bool bFin, bool bMask, uint8 iOpcode, const void* pPayload, size_t iPayloadLen)
{
	char* pFrame = NULL;
	size_t iFrameLen = 0u;
	bool bRet;
	if ( !pStream ) return false;
	if ( !__xws2BuildFrameBytesEx(iOpcode, bFin, bMask, pPayload, iPayloadLen, &pFrame, &iFrameLen) ) return false;
	bRet = __xws2StreamQueueBytesDirect(pStream, pFrame, iFrameLen);
	XNET_FREE(pFrame);
	return bRet;
}

static bool __xws2StreamQueueFrameDirect(xnetstream* pStream, bool bMask, uint8 iOpcode, const void* pPayload, size_t iPayloadLen)
{
	char* pFrame = NULL;
	size_t iFrameLen = 0u;
	bool bRet;
	if ( !pStream ) return false;
	if ( !__xws2BuildFrameBytesEx(iOpcode, true, bMask, pPayload, iPayloadLen, &pFrame, &iFrameLen) ) return false;
	bRet = __xws2StreamQueueBytesDirect(pStream, pFrame, iFrameLen);
	XNET_FREE(pFrame);
	return bRet;
}

static void __xws2CloseTask(xnetworker* pWorker, ptr pArg)
{
	__xws2_close_task* pTask = (__xws2_close_task*)pArg;
	(void)pWorker;
	if ( !pTask ) return;
	if ( pTask->pStream && pTask->pFrame && pTask->iFrameLen > 0u ) {
		(void)__xws2StreamQueueBytesDirect(pTask->pStream, pTask->pFrame, pTask->iFrameLen);
	}
	if ( pTask->pStream ) {
		xrtNetStreamClose(pTask->pStream, XNET_CLOSE_F_GRACEFUL);
	}
	XNET_FREE(pTask->pFrame);
	XNET_FREE(pTask);
}

static xnet_result __xws2PostClose(xnetstream* pStream, bool bMask, uint16 iCode, const char* sReason)
{
	__xws2_close_task* pTask;
	char* pPayload = NULL;
	char* pFrame = NULL;
	size_t iPayloadLen = 0u;
	size_t iFrameLen = 0u;
	size_t iReasonLen = sReason ? strlen(sReason) : 0u;
	if ( !pStream || !pStream->pEngine || !pStream->pWorker ) return XRT_NET_ERROR;
	if ( !__xws2BuildClosePayload(iCode, sReason, iReasonLen, &pPayload, &iPayloadLen) ) return XRT_NET_ERROR;
	if ( !__xws2BuildFrameBytes(XCODEC_WS_OPCODE_CLOSE, bMask, pPayload, iPayloadLen, &pFrame, &iFrameLen) ) {
		XNET_FREE(pPayload);
		return XRT_NET_ERROR;
	}
	XNET_FREE(pPayload);
	pTask = (__xws2_close_task*)XNET_ALLOC(sizeof(__xws2_close_task));
	if ( !pTask ) {
		XNET_FREE(pFrame);
		return XRT_NET_ERROR;
	}
	memset(pTask, 0, sizeof(__xws2_close_task));
	pTask->pStream = pStream;
	pTask->pFrame = pFrame;
	pTask->iFrameLen = iFrameLen;
	if ( xrtNetEnginePost(pStream->pEngine, pStream->pWorker->iId, __xws2CloseTask, pTask) != XRT_NET_OK ) {
		XNET_FREE(pFrame);
		XNET_FREE(pTask);
		return XRT_NET_ERROR;
	}
	return XRT_NET_OK;
}

static bool __xws2PeekPayloadCopy(xnetchain* pChain, const xcodecframe* pFrame, const xcodecwsframeinfo* pInfo, char** ppPayload, size_t* pPayloadLen)
{
	char* pBuf;
	size_t iNeed;
	if ( !ppPayload || !pPayloadLen || !pChain || !pFrame || !pInfo ) return false;
	iNeed = pFrame->iPayloadBytes;
	pBuf = (char*)XNET_ALLOC(iNeed + 1u);
	if ( !pBuf ) return false;
	if ( xrtCodecFramePeek(pChain, pFrame, pBuf, iNeed) != iNeed ) {
		XNET_FREE(pBuf);
		return false;
	}
	if ( (pInfo->iFlags & XCODEC_WS_F_MASKED) != 0u && iNeed > 0u ) {
		xrtCodecWsUnmask(pBuf, iNeed, pInfo->aMask, 0u);
	}
	pBuf[iNeed] = '\0';
	*ppPayload = pBuf;
	*pPayloadLen = iNeed;
	return true;
}

static void __xws2ClientEmitText(xws2client* pClient, const char* pData, size_t iLen)
{
	if ( pClient && pClient->tEvents.OnText ) {
		pClient->tEvents.OnText(pClient->pUserData, pClient, pData, iLen);
	}
}

static void __xws2ClientEmitBinary(xws2client* pClient, const void* pData, size_t iLen)
{
	if ( pClient && pClient->tEvents.OnBinary ) {
		pClient->tEvents.OnBinary(pClient->pUserData, pClient, pData, iLen);
	}
}

static void __xws2ServerEmitText(xws2conn* pConn, const char* pData, size_t iLen)
{
	xws2server* pServer = pConn ? pConn->pServer : NULL;
	if ( pServer && pServer->tEvents.OnText ) {
		pServer->tEvents.OnText(pServer->pUserData, pServer, pConn, pData, iLen);
	}
}

static void __xws2ServerEmitBinary(xws2conn* pConn, const void* pData, size_t iLen)
{
	xws2server* pServer = pConn ? pConn->pServer : NULL;
	if ( pServer && pServer->tEvents.OnBinary ) {
		pServer->tEvents.OnBinary(pServer->pUserData, pServer, pConn, pData, iLen);
	}
}

static int __xws2ClientConsumeDataFrame(xws2client* pClient, uint8 iOpcode, bool bFin, const char* pPayload, size_t iPayloadLen)
{
	size_t iLimit = pClient && pClient->tConfig.iRecvLimit > 0u ? (size_t)pClient->tConfig.iRecvLimit : 0u;
	int iAppend;
	if ( !pClient ) return __XWS2_APPEND_INTERNAL;
	if ( iOpcode == XCODEC_WS_OPCODE_CONT ) {
		if ( pClient->iMsgOpcode == 0u ) return __XWS2_APPEND_PROTOCOL;
		iAppend = __xws2MessageAppend(&pClient->pMsgBuf, &pClient->iMsgLen, &pClient->iMsgCap, &pClient->iMsgOpcode, 0u, pPayload, iPayloadLen, iLimit);
		if ( iAppend != __XWS2_APPEND_OK ) return iAppend;
		if ( bFin ) {
			if ( pClient->iMsgOpcode == XCODEC_WS_OPCODE_TEXT ) __xws2ClientEmitText(pClient, pClient->pMsgBuf ? pClient->pMsgBuf : "", pClient->iMsgLen);
			else if ( pClient->iMsgOpcode == XCODEC_WS_OPCODE_BINARY ) __xws2ClientEmitBinary(pClient, pClient->pMsgBuf, pClient->iMsgLen);
			else return __XWS2_APPEND_INTERNAL;
			__xws2MessageReset(&pClient->pMsgBuf, &pClient->iMsgLen, &pClient->iMsgCap, &pClient->iMsgOpcode);
		}
		return __XWS2_APPEND_OK;
	}
	if ( iOpcode != XCODEC_WS_OPCODE_TEXT && iOpcode != XCODEC_WS_OPCODE_BINARY ) return __XWS2_APPEND_PROTOCOL;
	if ( pClient->iMsgOpcode != 0u ) return __XWS2_APPEND_PROTOCOL;
	if ( bFin ) {
		if ( iOpcode == XCODEC_WS_OPCODE_TEXT ) __xws2ClientEmitText(pClient, pPayload, iPayloadLen);
		else __xws2ClientEmitBinary(pClient, pPayload, iPayloadLen);
		return __XWS2_APPEND_OK;
	}
	return __xws2MessageAppend(&pClient->pMsgBuf, &pClient->iMsgLen, &pClient->iMsgCap, &pClient->iMsgOpcode, iOpcode, pPayload, iPayloadLen, iLimit);
}

static int __xws2ServerConsumeDataFrame(xws2conn* pConn, uint8 iOpcode, bool bFin, const char* pPayload, size_t iPayloadLen)
{
	size_t iLimit = (pConn && pConn->pServer && pConn->pServer->tConfig.iRecvLimit > 0u) ? (size_t)pConn->pServer->tConfig.iRecvLimit : 0u;
	int iAppend;
	if ( !pConn ) return __XWS2_APPEND_INTERNAL;
	if ( iOpcode == XCODEC_WS_OPCODE_CONT ) {
		if ( pConn->iMsgOpcode == 0u ) return __XWS2_APPEND_PROTOCOL;
		iAppend = __xws2MessageAppend(&pConn->pMsgBuf, &pConn->iMsgLen, &pConn->iMsgCap, &pConn->iMsgOpcode, 0u, pPayload, iPayloadLen, iLimit);
		if ( iAppend != __XWS2_APPEND_OK ) return iAppend;
		if ( bFin ) {
			if ( pConn->iMsgOpcode == XCODEC_WS_OPCODE_TEXT ) __xws2ServerEmitText(pConn, pConn->pMsgBuf ? pConn->pMsgBuf : "", pConn->iMsgLen);
			else if ( pConn->iMsgOpcode == XCODEC_WS_OPCODE_BINARY ) __xws2ServerEmitBinary(pConn, pConn->pMsgBuf, pConn->iMsgLen);
			else return __XWS2_APPEND_INTERNAL;
			__xws2MessageReset(&pConn->pMsgBuf, &pConn->iMsgLen, &pConn->iMsgCap, &pConn->iMsgOpcode);
		}
		return __XWS2_APPEND_OK;
	}
	if ( iOpcode != XCODEC_WS_OPCODE_TEXT && iOpcode != XCODEC_WS_OPCODE_BINARY ) return __XWS2_APPEND_PROTOCOL;
	if ( pConn->iMsgOpcode != 0u ) return __XWS2_APPEND_PROTOCOL;
	if ( bFin ) {
		if ( iOpcode == XCODEC_WS_OPCODE_TEXT ) __xws2ServerEmitText(pConn, pPayload, iPayloadLen);
		else __xws2ServerEmitBinary(pConn, pPayload, iPayloadLen);
		return __XWS2_APPEND_OK;
	}
	return __xws2MessageAppend(&pConn->pMsgBuf, &pConn->iMsgLen, &pConn->iMsgCap, &pConn->iMsgOpcode, iOpcode, pPayload, iPayloadLen, iLimit);
}


/* ============================== Client helpers ============================== */

static void __xws2ClientEmitError(xws2client* pClient, int iSysErr)
{
	if ( !pClient ) return;
	pClient->iLastSysErr = iSysErr;
	if ( pClient->tEvents.OnError ) {
		pClient->tEvents.OnError(pClient->pUserData, pClient, iSysErr);
	}
}

static bool __xws2ClientValidateHandshake(xws2client* pClient, const xcodechttp1msg* pMsg)
{
	const char* sUpgrade;
	const char* sConnection;
	const char* sAccept;
	const char* sProtocol;
	if ( !pClient || !pMsg ) return false;
	if ( pMsg->iStatusCode != 101u ) return false;
	sUpgrade = xrtCodecHttp1GetHeader(pMsg, "Upgrade");
	sConnection = xrtCodecHttp1GetHeader(pMsg, "Connection");
	sAccept = xrtCodecHttp1GetHeader(pMsg, "Sec-WebSocket-Accept");
	if ( !sUpgrade || !__xws2StrEqNoCase(sUpgrade, "websocket") ) return false;
	if ( !sConnection || !__xws2ContainsTokenNoCase(sConnection, "Upgrade") ) return false;
	if ( !sAccept || !__xws2StrEqNoCase(sAccept, pClient->sExpectedAccept) ) return false;
	if ( pClient->tConfig.sProtocol[0] ) {
		sProtocol = xrtCodecHttp1GetHeader(pMsg, "Sec-WebSocket-Protocol");
		if ( !sProtocol || !__xws2StrEqNoCase(sProtocol, pClient->tConfig.sProtocol) ) return false;
	}
	return true;
}

static void __xws2ClientConsumeFrames(xws2client* pClient, xnetchain* pChain)
{
	while ( pClient && pChain ) {
		xcodecframe tFrame;
		xcodecwsframeinfo tInfo;
		xcodecstatus iParse = xrtCodecWsParseFrame(pChain, &tFrame, &tInfo);
		char* pPayload = NULL;
		size_t iPayloadLen = 0u;
		bool bFin;
		int iDataRet;
		uint16 iCloseCode = XWS2_CLOSE_NORMAL;
		if ( iParse == XCODEC_STATUS_NEED_MORE ) return;
		if ( iParse == XCODEC_STATUS_ERROR ) {
			__xws2ClientEmitError(pClient, -2);
			if ( pClient->pStream ) xrtNetStreamClose(pClient->pStream, XNET_CLOSE_F_ABORT);
			return;
		}
		if ( (tInfo.iFlags & XCODEC_WS_F_CONTROL) != 0u && tInfo.iPayloadLen > 125u ) {
			(void)__xws2PostClose(pClient->pStream, true, XWS2_CLOSE_PROTOCOL, "control too large");
			return;
		}
		if ( !__xws2PeekPayloadCopy(pChain, &tFrame, &tInfo, &pPayload, &iPayloadLen) ) {
			__xws2ClientEmitError(pClient, -3);
			xrtNetStreamClose(pClient->pStream, XNET_CLOSE_F_ABORT);
			return;
		}
		xrtCodecFrameConsume(pChain, &tFrame);
		bFin = (tInfo.iFlags & XCODEC_WS_F_FIN) != 0u;
		switch ( tInfo.iOpcode ) {
			case XCODEC_WS_OPCODE_TEXT:
			case XCODEC_WS_OPCODE_BINARY:
			case XCODEC_WS_OPCODE_CONT:
				iDataRet = __xws2ClientConsumeDataFrame(pClient, tInfo.iOpcode, bFin, pPayload, iPayloadLen);
				if ( iDataRet == __XWS2_APPEND_OK ) break;
				if ( iDataRet == __XWS2_APPEND_TOO_BIG ) {
					(void)__xws2PostClose(pClient->pStream, true, XWS2_CLOSE_TOO_BIG, "message too large");
				} else if ( iDataRet == __XWS2_APPEND_PROTOCOL ) {
					(void)__xws2PostClose(pClient->pStream, true, XWS2_CLOSE_PROTOCOL, "bad fragment sequence");
				} else {
					__xws2ClientEmitError(pClient, -7);
					xrtNetStreamClose(pClient->pStream, XNET_CLOSE_F_ABORT);
				}
				XNET_FREE(pPayload);
				return;
				break;
			case XCODEC_WS_OPCODE_PING:
				if ( pClient->pStream ) (void)__xws2StreamQueueFrameDirect(pClient->pStream, true, XCODEC_WS_OPCODE_PONG, pPayload, iPayloadLen);
				if ( pClient->tEvents.OnPing ) pClient->tEvents.OnPing(pClient->pUserData, pClient, pPayload, iPayloadLen);
				break;
			case XCODEC_WS_OPCODE_PONG:
				if ( pClient->tEvents.OnPong ) pClient->tEvents.OnPong(pClient->pUserData, pClient, pPayload, iPayloadLen);
				break;
			case XCODEC_WS_OPCODE_CLOSE:
				if ( iPayloadLen >= 2u ) {
					iCloseCode = (uint16)(((uint8)pPayload[0] << 8u) | (uint8)pPayload[1]);
				}
				if ( __xws2AtomicCompareExchange(&pClient->iClosePosted, 1, 0) == 0 ) {
					if ( pClient->pStream ) (void)__xws2PostClose(pClient->pStream, true, iCloseCode, NULL);
				}
				XNET_FREE(pPayload);
				return;
			default:
				(void)__xws2PostClose(pClient->pStream, true, XWS2_CLOSE_PROTOCOL, "bad opcode");
				XNET_FREE(pPayload);
				return;
		}
		XNET_FREE(pPayload);
	}
}

static void __xws2ClientStreamOnOpen(ptr pOwner, xnetstream* pStream)
{
	xws2client* pClient = (xws2client*)pOwner;
	char* pHandshake = NULL;
	size_t iHandshakeLen = 0u;
	if ( !pClient || !pStream ) return;
	if ( !__xws2BuildClientHandshake(pClient, &pHandshake, &iHandshakeLen) ) {
		__xws2ClientEmitError(pClient, -4);
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return;
	}
	if ( xrtNetStreamSend(pStream, pHandshake, iHandshakeLen) != XRT_NET_OK ) {
		__xws2ClientEmitError(pClient, -5);
		XNET_FREE(pHandshake);
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return;
	}
	XNET_FREE(pHandshake);
}

static void __xws2ClientStreamOnRecv(ptr pOwner, xnetstream* pStream, xnetchain* pChain)
{
	xws2client* pClient = (xws2client*)pOwner;
	if ( !pClient || !pStream || !pChain ) return;
	if ( __xws2AtomicLoad(&pClient->iOpen) == 0 ) {
		xcodecframe tFrame;
		xcodechttp1msg tMsg;
		xcodecstatus iParse = xrtCodecHttp1Parse(pChain, &tFrame, &tMsg);
		if ( iParse == XCODEC_STATUS_NEED_MORE ) return;
		if ( iParse == XCODEC_STATUS_ERROR || !__xws2ClientValidateHandshake(pClient, &tMsg) ) {
			__xws2ClientEmitError(pClient, -6);
			xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
			return;
		}
		xrtCodecFrameConsume(pChain, &tFrame);
		(void)__xws2AtomicCompareExchange(&pClient->iOpen, 1, 0);
		if ( pClient->tEvents.OnOpen ) {
			pClient->tEvents.OnOpen(pClient->pUserData, pClient);
		}
	}
	__xws2ClientConsumeFrames(pClient, pChain);
}

static void __xws2ClientStreamOnClose(ptr pOwner, xnetstream* pStream, xnet_result iReason)
{
	xws2client* pClient = (xws2client*)pOwner;
	(void)pStream;
	if ( pClient && pClient->tEvents.OnClose ) {
		pClient->tEvents.OnClose(pClient->pUserData, pClient, iReason);
	}
}

static void __xws2ClientStreamOnError(ptr pOwner, xnetstream* pStream, int iSysErr)
{
	xws2client* pClient = (xws2client*)pOwner;
	(void)pStream;
	__xws2ClientEmitError(pClient, iSysErr);
}

static const xnetstreamevents* __xws2ClientStreamEvents(void)
{
	static const xnetstreamevents tEvents = {
		__xws2ClientStreamOnOpen,
		__xws2ClientStreamOnRecv,
		NULL,
		__xws2ClientStreamOnClose,
		__xws2ClientStreamOnError,
		NULL,
		NULL
	};
	return &tEvents;
}


/* ============================== Server helpers ============================== */

static void __xws2ServerAddConn(xws2server* pServer, xws2conn* pConn)
{
	if ( !pServer || !pConn ) return;
	__xws2Lock(&pServer->iConnLock);
	pConn->pNext = pServer->pConnHead;
	pServer->pConnHead = pConn;
	__xws2Unlock(&pServer->iConnLock);
}

static void __xws2ServerRemoveConn(xws2server* pServer, xws2conn* pConn)
{
	xws2conn** ppNode;
	if ( !pServer || !pConn ) return;
	__xws2Lock(&pServer->iConnLock);
	ppNode = &pServer->pConnHead;
	while ( *ppNode ) {
		if ( *ppNode == pConn ) {
			*ppNode = pConn->pNext;
			break;
		}
		ppNode = &(*ppNode)->pNext;
	}
	__xws2Unlock(&pServer->iConnLock);
}

static xws2conn* __xws2ServerDetachAllConns(xws2server* pServer)
{
	xws2conn* pHead;
	if ( !pServer ) return NULL;
	__xws2Lock(&pServer->iConnLock);
	pHead = pServer->pConnHead;
	pServer->pConnHead = NULL;
	__xws2Unlock(&pServer->iConnLock);
	return pHead;
}

static void __xws2ConnCleanupTask(xnetworker* pWorker, ptr pArg)
{
	xws2conn* pConn = (xws2conn*)pArg;
	(void)pWorker;
	if ( !pConn ) return;
	if ( pConn->pServer ) {
		__xws2ServerRemoveConn(pConn->pServer, pConn);
		pConn->pServer = NULL;
	}
	if ( pConn->pStream ) {
		xrtNetStreamDestroy(pConn->pStream);
		pConn->pStream = NULL;
	}
	__xws2MessageReset(&pConn->pMsgBuf, &pConn->iMsgLen, &pConn->iMsgCap, &pConn->iMsgOpcode);
	XNET_FREE(pConn);
}

static void __xws2ConnPostCleanup(xws2conn* pConn)
{
	if ( !pConn ) return;
	if ( __xws2AtomicCompareExchange(&pConn->iCleanupPosted, 1, 0) != 0 ) return;
	if ( pConn->pServer && pConn->pServer->pEngine && pConn->pStream && pConn->pStream->pWorker ) {
		if ( xrtNetEnginePost(pConn->pServer->pEngine, pConn->pStream->pWorker->iId, __xws2ConnCleanupTask, pConn) == XRT_NET_OK ) {
			return;
		}
	}
	__xws2ConnCleanupTask(NULL, pConn);
}

static void __xws2ServerEmitError(xws2server* pServer, xws2conn* pConn, int iSysErr)
{
	if ( pConn ) pConn->iLastSysErr = iSysErr;
	if ( pServer && pServer->tEvents.OnError ) {
		pServer->tEvents.OnError(pServer->pUserData, pServer, pConn, iSysErr);
	}
}

static bool __xws2SendHttpReply(xnetstream* pStream, uint32 iStatusCode, const char* sBody, const char* sAccept, const char* sProtocol, bool bClose)
{
	char* pBytes = NULL;
	size_t iLen = 0u;
	if ( !pStream ) return false;
	if ( !__xws2BuildHttpResponseBytes(iStatusCode, sBody, sAccept, sProtocol, &pBytes, &iLen) ) return false;
	if ( !__xws2StreamQueueBytesDirect(pStream, pBytes, iLen) ) {
		XNET_FREE(pBytes);
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return false;
	}
	XNET_FREE(pBytes);
	if ( bClose ) xrtNetStreamClose(pStream, XNET_CLOSE_F_GRACEFUL);
	return true;
}

static bool __xws2ServerValidateRequest(const xcodechttp1msg* pMsg, const char** psKey)
{
	const char* sUpgrade;
	const char* sConnection;
	const char* sKey;
	const char* sVersion;
	if ( !pMsg || !psKey ) return false;
	if ( (pMsg->iFlags & XCODEC_HTTP1_F_REQUEST) == 0u ) return false;
	if ( strcmp(pMsg->sMethod, "GET") != 0 ) return false;
	sUpgrade = xrtCodecHttp1GetHeader(pMsg, "Upgrade");
	sConnection = xrtCodecHttp1GetHeader(pMsg, "Connection");
	sKey = xrtCodecHttp1GetHeader(pMsg, "Sec-WebSocket-Key");
	sVersion = xrtCodecHttp1GetHeader(pMsg, "Sec-WebSocket-Version");
	if ( !sUpgrade || !__xws2StrEqNoCase(sUpgrade, "websocket") ) return false;
	if ( !sConnection || !__xws2ContainsTokenNoCase(sConnection, "Upgrade") ) return false;
	if ( !sKey || sKey[0] == '\0' ) return false;
	if ( sVersion && strcmp(sVersion, "13") != 0 ) return false;
	*psKey = sKey;
	return true;
}

static void __xws2ServerConsumeFrames(xws2conn* pConn, xnetchain* pChain)
{
	xws2server* pServer = pConn ? pConn->pServer : NULL;
	while ( pConn && pServer && pChain ) {
		xcodecframe tFrame;
		xcodecwsframeinfo tInfo;
		xcodecstatus iParse = xrtCodecWsParseFrame(pChain, &tFrame, &tInfo);
		char* pPayload = NULL;
		size_t iPayloadLen = 0u;
		bool bFin;
		int iDataRet;
		uint16 iCloseCode = XWS2_CLOSE_NORMAL;
		if ( iParse == XCODEC_STATUS_NEED_MORE ) return;
		if ( iParse == XCODEC_STATUS_ERROR ) {
			__xws2ServerEmitError(pServer, pConn, -21);
			xrtNetStreamClose(pConn->pStream, XNET_CLOSE_F_ABORT);
			return;
		}
		if ( (tInfo.iFlags & XCODEC_WS_F_MASKED) == 0u ) {
			(void)__xws2PostClose(pConn->pStream, false, XWS2_CLOSE_PROTOCOL, "mask required");
			return;
		}
		if ( (tInfo.iFlags & XCODEC_WS_F_CONTROL) != 0u && tInfo.iPayloadLen > 125u ) {
			(void)__xws2PostClose(pConn->pStream, false, XWS2_CLOSE_PROTOCOL, "control too large");
			return;
		}
		if ( !__xws2PeekPayloadCopy(pChain, &tFrame, &tInfo, &pPayload, &iPayloadLen) ) {
			__xws2ServerEmitError(pServer, pConn, -22);
			xrtNetStreamClose(pConn->pStream, XNET_CLOSE_F_ABORT);
			return;
		}
		xrtCodecFrameConsume(pChain, &tFrame);
		bFin = (tInfo.iFlags & XCODEC_WS_F_FIN) != 0u;
		switch ( tInfo.iOpcode ) {
			case XCODEC_WS_OPCODE_TEXT:
			case XCODEC_WS_OPCODE_BINARY:
			case XCODEC_WS_OPCODE_CONT:
				iDataRet = __xws2ServerConsumeDataFrame(pConn, tInfo.iOpcode, bFin, pPayload, iPayloadLen);
				if ( iDataRet == __XWS2_APPEND_OK ) break;
				if ( iDataRet == __XWS2_APPEND_TOO_BIG ) {
					(void)__xws2PostClose(pConn->pStream, false, XWS2_CLOSE_TOO_BIG, "message too large");
				} else if ( iDataRet == __XWS2_APPEND_PROTOCOL ) {
					(void)__xws2PostClose(pConn->pStream, false, XWS2_CLOSE_PROTOCOL, "bad fragment sequence");
				} else {
					__xws2ServerEmitError(pServer, pConn, -23);
					xrtNetStreamClose(pConn->pStream, XNET_CLOSE_F_ABORT);
				}
				XNET_FREE(pPayload);
				return;
				break;
			case XCODEC_WS_OPCODE_PING:
				(void)__xws2StreamQueueFrameDirect(pConn->pStream, false, XCODEC_WS_OPCODE_PONG, pPayload, iPayloadLen);
				if ( pServer->tEvents.OnPing ) pServer->tEvents.OnPing(pServer->pUserData, pServer, pConn, pPayload, iPayloadLen);
				break;
			case XCODEC_WS_OPCODE_PONG:
				if ( pServer->tEvents.OnPong ) pServer->tEvents.OnPong(pServer->pUserData, pServer, pConn, pPayload, iPayloadLen);
				break;
			case XCODEC_WS_OPCODE_CLOSE:
				if ( iPayloadLen >= 2u ) {
					iCloseCode = (uint16)(((uint8)pPayload[0] << 8u) | (uint8)pPayload[1]);
				}
				if ( __xws2AtomicCompareExchange(&pConn->iClosePosted, 1, 0) == 0 ) {
					if ( pConn->pStream ) (void)__xws2PostClose(pConn->pStream, false, iCloseCode, NULL);
				}
				XNET_FREE(pPayload);
				return;
			default:
				(void)__xws2PostClose(pConn->pStream, false, XWS2_CLOSE_PROTOCOL, "bad opcode");
				XNET_FREE(pPayload);
				return;
		}
		XNET_FREE(pPayload);
	}
}

static bool __xws2ListenerOnAccept(ptr pOwner, xnetlistener* pListener, xnetstream* pStream)
{
	xws2server* pServer = (xws2server*)pOwner;
	xws2conn* pConn;
	(void)pListener;
	if ( !pServer || !pStream ) return false;
	pConn = (xws2conn*)xrtNetStreamGetUserData(pStream);
	if ( !pConn ) return false;
	pConn->pServer = pServer;
	pConn->pStream = pStream;
	__xws2ServerAddConn(pServer, pConn);
	return true;
}

static void __xws2ServerStreamOnOpen(ptr pOwner, xnetstream* pStream)
{
	(void)pOwner;
	(void)pStream;
}

static void __xws2ServerStreamOnRecv(ptr pOwner, xnetstream* pStream, xnetchain* pChain)
{
	xws2conn* pConn = (xws2conn*)pOwner;
	xws2server* pServer = pConn ? pConn->pServer : NULL;
	if ( !pConn || !pServer || !pStream || !pChain ) return;
	if ( __xws2AtomicLoad(&pConn->iOpen) == 0 ) {
		xcodecframe tFrame;
		xcodechttp1msg tMsg;
		xcodecstatus iParse = xrtCodecHttp1Parse(pChain, &tFrame, &tMsg);
		const char* sKey = NULL;
		char sAccept[64];
		const char* sClientProtocol;
		if ( iParse == XCODEC_STATUS_NEED_MORE ) return;
		if ( iParse == XCODEC_STATUS_ERROR || !__xws2ServerValidateRequest(&tMsg, &sKey) ) {
			__xws2ServerEmitError(pServer, pConn, -31);
			(void)__xws2SendHttpReply(pStream, 400u, "Bad Request", NULL, NULL, true);
			return;
		}
		if ( !__xws2ComputeAccept(sKey, sAccept, sizeof(sAccept)) ) {
			__xws2ServerEmitError(pServer, pConn, -32);
			xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
			return;
		}
		sClientProtocol = xrtCodecHttp1GetHeader(&tMsg, "Sec-WebSocket-Protocol");
		if ( pServer->tConfig.sProtocol[0] ) {
			if ( !sClientProtocol || !__xws2ContainsTokenNoCase(sClientProtocol, pServer->tConfig.sProtocol) ) {
				(void)__xws2SendHttpReply(pStream, 400u, "Protocol Required", NULL, NULL, true);
				return;
			}
			__xws2CopyToken(pConn->sProtocol, sizeof(pConn->sProtocol), pServer->tConfig.sProtocol);
		}
		if ( !__xws2SendHttpReply(pStream, 101u, NULL, sAccept, pConn->sProtocol, false) ) {
			__xws2ServerEmitError(pServer, pConn, -33);
			return;
		}
		xrtCodecFrameConsume(pChain, &tFrame);
		(void)__xws2AtomicCompareExchange(&pConn->iOpen, 1, 0);
		if ( pServer->tEvents.OnOpen ) {
			pServer->tEvents.OnOpen(pServer->pUserData, pServer, pConn);
		}
	}
	__xws2ServerConsumeFrames(pConn, pChain);
}

static void __xws2ServerStreamOnClose(ptr pOwner, xnetstream* pStream, xnet_result iReason)
{
	xws2conn* pConn = (xws2conn*)pOwner;
	xws2server* pServer = pConn ? pConn->pServer : NULL;
	(void)pStream;
	if ( pServer && pServer->tEvents.OnClose ) {
		pServer->tEvents.OnClose(pServer->pUserData, pServer, pConn, iReason);
	}
	__xws2ConnPostCleanup(pConn);
}

static void __xws2ServerStreamOnError(ptr pOwner, xnetstream* pStream, int iSysErr)
{
	xws2conn* pConn = (xws2conn*)pOwner;
	xws2server* pServer = pConn ? pConn->pServer : NULL;
	(void)pStream;
	__xws2ServerEmitError(pServer, pConn, iSysErr);
}

static const xnetlistenerevents* __xws2ListenerEvents(void)
{
	static const xnetlistenerevents tEvents = {
		__xws2ListenerOnAccept,
		NULL
	};
	return &tEvents;
}

static const xnetstreamevents* __xws2ServerStreamEvents(void)
{
	static const xnetstreamevents tEvents = {
		__xws2ServerStreamOnOpen,
		__xws2ServerStreamOnRecv,
		NULL,
		__xws2ServerStreamOnClose,
		__xws2ServerStreamOnError,
		NULL,
		NULL
	};
	return &tEvents;
}

#if defined(_WIN32) || defined(_WIN64)
static DWORD WINAPI __xws2AcceptThread(LPVOID pArg)
#else
static void* __xws2AcceptThread(void* pArg)
#endif
{
	xws2server* pServer = (xws2server*)pArg;
	while ( pServer && __xws2AtomicLoad(&pServer->bRunning) != 0 ) {
		xws2conn* pConn = NULL;
		if ( pServer->pListener && pServer->pListener->bRunning ) {
			pConn = (xws2conn*)XNET_ALLOC(sizeof(xws2conn));
			if ( pConn ) {
				memset(pConn, 0, sizeof(xws2conn));
				pConn->pServer = pServer;
				if ( !__xnetListenerTryAcceptOne(pServer->pListener, pConn) ) {
					XNET_FREE(pConn);
				}
			}
		}
		__xws2SleepMs(pServer && pServer->tConfig.iAcceptPollMs > 0 ? pServer->tConfig.iAcceptPollMs : 5u);
	}
	#if !defined(_WIN32) && !defined(_WIN64)
		return NULL;
	#endif
}


/* ============================== Public API ============================== */

static void xrtWs2ClientConfigInit(xws2clientconfig* pCfg)
{
	if ( !pCfg ) return;
	memset(pCfg, 0, sizeof(xws2clientconfig));
	pCfg->iConnectTimeoutMs = 5000u;
	pCfg->iRecvLimit = 1024u * 1024u;
	pCfg->bVerifyPeer = true;
}

static void xrtWs2ServerConfigInit(xws2serverconfig* pCfg)
{
	if ( !pCfg ) return;
	memset(pCfg, 0, sizeof(xws2serverconfig));
	pCfg->iBacklog = 128u;
	pCfg->iRecvLimit = 1024u * 1024u;
	pCfg->iAcceptPollMs = 5u;
}

static xws2client* xrtWs2ClientCreate(xnetengine* pEngine, const xws2clientconfig* pCfg, const xws2clientevents* pEvents, ptr pUserData)
{
	xws2client* pClient;
	if ( !pEngine ) return NULL;
	pClient = (xws2client*)XNET_ALLOC(sizeof(xws2client));
	if ( !pClient ) return NULL;
	memset(pClient, 0, sizeof(xws2client));
	pClient->pEngine = pEngine;
	if ( pCfg ) pClient->tConfig = *pCfg;
	else xrtWs2ClientConfigInit(&pClient->tConfig);
	if ( pEvents ) pClient->tEvents = *pEvents;
	pClient->pUserData = pUserData;
	return pClient;
}

static xnet_result xrtWs2ClientStart(xws2client* pClient)
{
	xnetconnectconfig tConnCfg;
	if ( !pClient || !pClient->pEngine || pClient->pStream ) return XRT_NET_ERROR;
	if ( !__xws2UrlParse(pClient->tConfig.sURL, &pClient->tURL) ) return XRT_NET_ERROR;
	pClient->pStream = xrtNetStreamCreate(pClient->pEngine, __xws2ClientStreamEvents(), pClient);
	if ( !pClient->pStream ) return XRT_NET_ERROR;
	xrtNetConnectConfigInit(&tConnCfg);
	tConnCfg.sHost = pClient->tURL.sHost;
	tConnCfg.iPort = pClient->tURL.iPort;
	tConnCfg.iConnectTimeoutMs = pClient->tConfig.iConnectTimeoutMs;
	tConnCfg.iRecvLimit = pClient->tConfig.iRecvLimit;
	if ( pClient->tURL.bSecure ) {
		memset(&pClient->tTlsCfg, 0, sizeof(pClient->tTlsCfg));
		pClient->tTlsCfg.sHostName = pClient->tURL.sHost;
		pClient->tTlsCfg.bVerifyPeer = pClient->tConfig.bVerifyPeer;
		tConnCfg.pTlsConfig = &pClient->tTlsCfg;
	}
	if ( xrtNetStreamConnect(pClient->pStream, &tConnCfg) != XRT_NET_OK ) {
		xrtNetStreamDestroy(pClient->pStream);
		pClient->pStream = NULL;
		return XRT_NET_ERROR;
	}
	return XRT_NET_OK;
}

static void xrtWs2ClientStop(xws2client* pClient)
{
	if ( !pClient ) return;
	if ( pClient->pStream ) {
		xrtNetStreamClose(pClient->pStream, XNET_CLOSE_F_ABORT);
		xrtNetStreamDestroy(pClient->pStream);
		pClient->pStream = NULL;
	}
	__xws2MessageReset(&pClient->pMsgBuf, &pClient->iMsgLen, &pClient->iMsgCap, &pClient->iMsgOpcode);
}

static void xrtWs2ClientDestroy(xws2client* pClient)
{
	if ( !pClient ) return;
	xrtWs2ClientStop(pClient);
	XNET_FREE(pClient);
}

static bool xrtWs2ClientIsOpen(const xws2client* pClient)
{
	return pClient && __xws2AtomicLoadConst(&pClient->iOpen) != 0 && pClient->pStream != NULL;
}

static xnet_result xrtWs2ClientSendText(xws2client* pClient, const char* sText, size_t iLen)
{
	if ( !pClient || !pClient->pStream || !xrtWs2ClientIsOpen(pClient) || !sText ) return XRT_NET_ERROR;
	return __xws2StreamSendFrame(pClient->pStream, true, XCODEC_WS_OPCODE_TEXT, sText, iLen);
}

static xnet_result xrtWs2ClientSendBinary(xws2client* pClient, const void* pData, size_t iLen)
{
	if ( !pClient || !pClient->pStream || !xrtWs2ClientIsOpen(pClient) || (!pData && iLen != 0u) ) return XRT_NET_ERROR;
	return __xws2StreamSendFrame(pClient->pStream, true, XCODEC_WS_OPCODE_BINARY, pData, iLen);
}

static xnet_result xrtWs2ClientPing(xws2client* pClient, const void* pData, size_t iLen)
{
	if ( !pClient || !pClient->pStream || !xrtWs2ClientIsOpen(pClient) || iLen > 125u ) return XRT_NET_ERROR;
	return __xws2StreamSendFrame(pClient->pStream, true, XCODEC_WS_OPCODE_PING, pData, iLen);
}

static xnet_result xrtWs2ClientClose(xws2client* pClient, uint16 iCode, const char* sReason)
{
	if ( !pClient || !pClient->pStream ) return XRT_NET_ERROR;
	if ( __xws2AtomicCompareExchange(&pClient->iClosePosted, 1, 0) != 0 ) return XRT_NET_OK;
	return __xws2PostClose(pClient->pStream, true, iCode, sReason);
}

static xws2server* xrtWs2ServerCreate(xnetengine* pEngine, const xws2serverconfig* pCfg, const xws2serverevents* pEvents, ptr pUserData)
{
	xws2server* pServer;
	if ( !pEngine ) return NULL;
	pServer = (xws2server*)XNET_ALLOC(sizeof(xws2server));
	if ( !pServer ) return NULL;
	memset(pServer, 0, sizeof(xws2server));
	pServer->pEngine = pEngine;
	if ( pCfg ) pServer->tConfig = *pCfg;
	else xrtWs2ServerConfigInit(&pServer->tConfig);
	if ( pEvents ) pServer->tEvents = *pEvents;
	pServer->pUserData = pUserData;
	return pServer;
}

static uint16 xrtWs2ServerBoundPort(const xws2server* pServer)
{
	return (pServer && pServer->pListener) ? pServer->pListener->tConfig.tBindAddr.iPort : 0u;
}

static xnet_result xrtWs2ServerStart(xws2server* pServer)
{
	xnetlistenconfig tListenCfg;
	if ( !pServer || !pServer->pEngine ) return XRT_NET_ERROR;
	if ( __xws2AtomicLoad(&pServer->bRunning) != 0 ) return XRT_NET_OK;
	xrtNetListenConfigInit(&tListenCfg);
	tListenCfg.tBindAddr = pServer->tConfig.tBindAddr;
	tListenCfg.iFlags = pServer->tConfig.iFlags;
	tListenCfg.iBacklog = pServer->tConfig.iBacklog;
	tListenCfg.iRecvLimit = pServer->tConfig.iRecvLimit;
	tListenCfg.pTlsConfig = pServer->tConfig.pTlsConfig;
	pServer->pListener = xrtNetListenerCreate(pServer->pEngine, &tListenCfg, __xws2ListenerEvents(), __xws2ServerStreamEvents(), pServer);
	if ( !pServer->pListener ) return XRT_NET_ERROR;
	if ( xrtNetListenerStart(pServer->pListener) != XRT_NET_OK ) {
		xrtNetListenerDestroy(pServer->pListener);
		pServer->pListener = NULL;
		return XRT_NET_ERROR;
	}
	(void)__xws2AtomicCompareExchange(&pServer->bRunning, 1, 0);
	#if defined(_WIN32) || defined(_WIN64)
		pServer->hAcceptThread = CreateThread(NULL, 0, __xws2AcceptThread, pServer, 0, NULL);
		if ( !pServer->hAcceptThread ) {
			(void)__xws2AtomicCompareExchange(&pServer->bRunning, 0, 1);
			xrtNetListenerStop(pServer->pListener);
			xrtNetListenerDestroy(pServer->pListener);
			pServer->pListener = NULL;
			return XRT_NET_ERROR;
		}
	#else
		if ( pthread_create(&pServer->hAcceptThread, NULL, __xws2AcceptThread, pServer) != 0 ) {
			(void)__xws2AtomicCompareExchange(&pServer->bRunning, 0, 1);
			xrtNetListenerStop(pServer->pListener);
			xrtNetListenerDestroy(pServer->pListener);
			pServer->pListener = NULL;
			return XRT_NET_ERROR;
		}
		pServer->bAcceptThreadStarted = true;
	#endif
	return XRT_NET_OK;
}

static void xrtWs2ServerStop(xws2server* pServer)
{
	xws2conn* pConn;
	if ( !pServer ) return;
	if ( __xws2AtomicCompareExchange(&pServer->bRunning, 0, 1) == 0 ) {
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
	pConn = __xws2ServerDetachAllConns(pServer);
	while ( pConn ) {
		xws2conn* pNext = pConn->pNext;
		pConn->pNext = NULL;
		(void)__xws2AtomicCompareExchange(&pConn->iCleanupPosted, 1, 0);
		if ( pConn->pStream ) {
			xrtNetStreamClose(pConn->pStream, XNET_CLOSE_F_ABORT);
			xrtNetStreamDestroy(pConn->pStream);
			pConn->pStream = NULL;
		}
		__xws2MessageReset(&pConn->pMsgBuf, &pConn->iMsgLen, &pConn->iMsgCap, &pConn->iMsgOpcode);
		XNET_FREE(pConn);
		pConn = pNext;
	}
}

static void xrtWs2ServerDestroy(xws2server* pServer)
{
	if ( !pServer ) return;
	xrtWs2ServerStop(pServer);
	XNET_FREE(pServer);
}

static bool xrtWs2ConnIsOpen(const xws2conn* pConn)
{
	return pConn && __xws2AtomicLoadConst(&pConn->iOpen) != 0 && pConn->pStream != NULL;
}

static xnet_result xrtWs2ConnSendText(xws2conn* pConn, const char* sText, size_t iLen)
{
	if ( !pConn || !pConn->pStream || !xrtWs2ConnIsOpen(pConn) || !sText ) return XRT_NET_ERROR;
	return __xws2StreamSendFrame(pConn->pStream, false, XCODEC_WS_OPCODE_TEXT, sText, iLen);
}

static xnet_result xrtWs2ConnSendBinary(xws2conn* pConn, const void* pData, size_t iLen)
{
	if ( !pConn || !pConn->pStream || !xrtWs2ConnIsOpen(pConn) || (!pData && iLen != 0u) ) return XRT_NET_ERROR;
	return __xws2StreamSendFrame(pConn->pStream, false, XCODEC_WS_OPCODE_BINARY, pData, iLen);
}

static xnet_result xrtWs2ConnPing(xws2conn* pConn, const void* pData, size_t iLen)
{
	if ( !pConn || !pConn->pStream || !xrtWs2ConnIsOpen(pConn) || iLen > 125u ) return XRT_NET_ERROR;
	return __xws2StreamSendFrame(pConn->pStream, false, XCODEC_WS_OPCODE_PING, pData, iLen);
}

static xnet_result xrtWs2ConnClose(xws2conn* pConn, uint16 iCode, const char* sReason)
{
	if ( !pConn || !pConn->pStream ) return XRT_NET_ERROR;
	if ( __xws2AtomicCompareExchange(&pConn->iClosePosted, 1, 0) != 0 ) return XRT_NET_OK;
	return __xws2PostClose(pConn->pStream, false, iCode, sReason);
}

#endif
