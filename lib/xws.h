#ifndef XRT_XWS_H
#define XRT_XWS_H

/*
    XRT mainline WebSocket layer on top of xnet.

    This header provides:
      - async WebSocket client and server wrappers on top of xnet_stream
      - HTTP upgrade handshake over plain TCP and builtin TLS
      - text/binary messaging, ping/pong, close, and fragmented message reassembly

    Current limitations:
      - extensions and permessage-deflate are not implemented
      - subprotocol negotiation is fixed-string and optional
*/


/* ============================== Public model ============================== */

#if !defined(XRT_BUILD_CORE)

#define XWS_URL_CAP              1024u
#define XWS_HOST_CAP             256u
#define XWS_PATH_CAP             1024u
#define XWS_ORIGIN_CAP           256u
#define XWS_PROTOCOL_CAP         128u
#define XWS_CLOSE_REASON_CAP     123u

#define XWS_CLOSE_NORMAL         1000u
#define XWS_CLOSE_GOING_AWAY     1001u
#define XWS_CLOSE_PROTOCOL       1002u
#define XWS_CLOSE_UNSUPPORTED    1003u
#define XWS_CLOSE_TOO_BIG        1009u
#define XWS_CLOSE_INTERNAL       1011u

typedef struct xrt_ws_client xwsclient;
typedef struct xrt_ws_server xwsserver;
typedef struct xrt_ws_conn   xwsconn;

typedef struct {
	char sURL[XWS_URL_CAP];
	char sOrigin[XWS_ORIGIN_CAP];
	char sProtocol[XWS_PROTOCOL_CAP];
	uint32 iConnectTimeoutMs;
	uint32 iRecvLimit;
	bool bVerifyPeer;
	xnetproxy* pProxy;
} xwsclientconfig;

typedef struct {
	xnetaddr tBindAddr;
	uint32 iFlags;
	uint32 iBacklog;
	uint32 iRecvLimit;
	const xtlsconfig* pTlsConfig;
	char sProtocol[XWS_PROTOCOL_CAP];
} xwsserverconfig;

typedef struct {
	void (*OnOpen)(ptr pOwner, xwsclient* pClient);
	void (*OnText)(ptr pOwner, xwsclient* pClient, const char* pData, size_t iLen);
	void (*OnBinary)(ptr pOwner, xwsclient* pClient, const void* pData, size_t iLen);
	void (*OnClose)(ptr pOwner, xwsclient* pClient, xnet_result iReason);
	void (*OnError)(ptr pOwner, xwsclient* pClient, int iSysErr);
	void (*OnPing)(ptr pOwner, xwsclient* pClient, const void* pData, size_t iLen);
	void (*OnPong)(ptr pOwner, xwsclient* pClient, const void* pData, size_t iLen);
} xwsclientevents;

typedef struct {
	void (*OnOpen)(ptr pOwner, xwsserver* pServer, xwsconn* pConn);
	void (*OnText)(ptr pOwner, xwsserver* pServer, xwsconn* pConn, const char* pData, size_t iLen);
	void (*OnBinary)(ptr pOwner, xwsserver* pServer, xwsconn* pConn, const void* pData, size_t iLen);
	void (*OnClose)(ptr pOwner, xwsserver* pServer, xwsconn* pConn, xnet_result iReason);
	void (*OnError)(ptr pOwner, xwsserver* pServer, xwsconn* pConn, int iSysErr);
	void (*OnPing)(ptr pOwner, xwsserver* pServer, xwsconn* pConn, const void* pData, size_t iLen);
	void (*OnPong)(ptr pOwner, xwsserver* pServer, xwsconn* pConn, const void* pData, size_t iLen);
} xwsserverevents;

#endif /* !XRT_BUILD_CORE */

typedef struct {
	bool bSecure;
	uint16 iPort;
	char sHost[XWS_HOST_CAP];
	char sPath[XWS_PATH_CAP];
} __xws_url;

struct xrt_ws_client {
	xnetengine* pEngine;
	xnetstream* pStream;
	xwsclientconfig tConfig;
	xwsclientevents tEvents;
	ptr pUserData;
	__xws_url tURL;
	xtlsconfig tTlsCfg;
	char sKey[32];
	char sExpectedAccept[64];
	char* pMsgBuf;
	size_t iMsgLen;
	size_t iMsgCap;
	uint8 iMsgOpcode;
	volatile long iOpen;
	volatile long iClosePosted;
	volatile long iCloseNotified;
	int iLastSysErr;
};

struct xrt_ws_conn {
	struct xrt_ws_conn* pNext;
	volatile long iCleanupPosted;
	volatile long iOpen;
	volatile long iClosePosted;
	volatile long iCloseNotified;
	xwsserver* pServer;
	xnetstream* pStream;
	char sProtocol[XWS_PROTOCOL_CAP];
	char* pMsgBuf;
	size_t iMsgLen;
	size_t iMsgCap;
	uint8 iMsgOpcode;
	int iLastSysErr;
};

struct xrt_ws_server {
	xnetengine* pEngine;
	xnetlistener* pListener;
	xwsserverconfig tConfig;
	xwsserverevents tEvents;
	ptr pUserData;
	volatile long iConnLock;
	volatile long bRunning;
	xwsconn* pConnHead;
};


/* ============================== WebSocket handshake crypto hooks ============================== */

extern void xrtRandomBytes(uint8* pBuf, size_t iLen);
extern void xrtSHA1(const ptr pData, size_t iLen, uint8* pOut);


/* ============================== Local helpers ============================== */

#define __XWS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

typedef struct {
	xnetstream* pStream;
	char* pFrame;
	size_t iFrameLen;
	bool bCloseStream;
} __xws_close_task;

#define __XWS_APPEND_OK       0
#define __XWS_APPEND_TOO_BIG  1
#define __XWS_APPEND_PROTOCOL 2
#define __XWS_APPEND_INTERNAL 3

static char __xwsToLower(char ch)
{
	if ( ch >= 'A' && ch <= 'Z' ) return (char)(ch + 32);
	return ch;
}

static bool __xwsStrEqNoCase(const char* sA, const char* sB)
{
	size_t i = 0;
	if ( !sA || !sB ) return false;
	while ( sA[i] && sB[i] ) {
		if ( __xwsToLower(sA[i]) != __xwsToLower(sB[i]) ) return false;
		i++;
	}
	return sA[i] == '\0' && sB[i] == '\0';
}

static bool __xwsContainsTokenNoCase(const char* sText, const char* sToken)
{
	return xrtHttpHeaderContainsToken(sText, sToken);
}

static void __xwsCopyToken(char* sDst, size_t iDstCap, const char* sSrc)
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

static long __xwsAtomicCompareExchange(volatile long* pValue, long iExchange, long iComparand)
{
	return __xnetAtomicCompareExchange32(pValue, iExchange, iComparand);
}

static long __xwsAtomicLoad(volatile long* pValue)
{
	return __xnetAtomicLoad32(pValue);
}

static long __xwsAtomicLoadConst(const volatile long* pValue)
{
	return __xnetAtomicLoad32(pValue);
}

static void __xwsSleep0(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(0);
	#else
		sched_yield();
	#endif
}

static void __xwsLock(volatile long* pLock)
{
	if ( !pLock ) return;
	while ( __xwsAtomicCompareExchange(pLock, 1, 0) != 0 ) {
		__xwsSleep0();
	}
}

static void __xwsUnlock(volatile long* pLock)
{
	if ( !pLock ) return;
	(void)__xnetAtomicExchange32(pLock, 0);
}

static bool __xwsAppendBytes(char** ppBuf, size_t* pLen, size_t* pCap, const void* pData, size_t iDataLen)
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

static bool __xwsAppendText(char** ppBuf, size_t* pLen, size_t* pCap, const char* sText)
{
	return __xwsAppendBytes(ppBuf, pLen, pCap, sText, sText ? strlen(__xrt_cstr(sText)) : 0);
}

static const char* __xwsHttpStatusText(uint32 iStatusCode)
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

static bool __xwsBase64Encode(const uint8* pIn, size_t iLen, char* sOut, size_t iOutCap)
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

static bool __xwsGenerateKey(char* sKey, size_t iCap)
{
	uint8 aRandom[16];
	if ( !sKey || iCap < 25u ) return false;
	xrtRandomBytes(aRandom, sizeof(aRandom));
	return __xwsBase64Encode(aRandom, sizeof(aRandom), sKey, iCap);
}

static bool __xwsComputeAccept(const char* sKey, char* sAccept, size_t iCap)
{
	char sCombined[128];
	uint8 aDigest[20];
	int iWritten;
	if ( !sKey || !sAccept || iCap < 29u ) return false;
	iWritten = snprintf(sCombined, sizeof(sCombined), "%s%s", sKey, __XWS_GUID);
	if ( iWritten <= 0 || (size_t)iWritten >= sizeof(sCombined) ) return false;
	xrtSHA1(sCombined, (size_t)iWritten, aDigest);
	return __xwsBase64Encode(aDigest, sizeof(aDigest), sAccept, iCap);
}

static bool __xwsBuildClientHandshake(xwsclient* pClient, char** ppOut, size_t* pOutLen)
{
	char sHost[384];
	char* pBuf = NULL;
	size_t iLen = 0;
	size_t iCap = 0;
	if ( !pClient || !ppOut || !pOutLen ) return false;
	if ( !xrtUrlMakeHostHeaderFixed(pClient->tURL.bSecure ? "wss" : "ws", pClient->tURL.sHost, pClient->tURL.iPort, sHost, sizeof(sHost)) ) return false;
	if ( !__xwsGenerateKey(pClient->sKey, sizeof(pClient->sKey)) ) return false;
	if ( !__xwsComputeAccept(pClient->sKey, pClient->sExpectedAccept, sizeof(pClient->sExpectedAccept)) ) return false;
	if ( !__xwsAppendText(&pBuf, &iLen, &iCap, "GET ") ||
		!__xwsAppendText(&pBuf, &iLen, &iCap, pClient->tURL.sPath) ||
		!__xwsAppendText(&pBuf, &iLen, &iCap, " HTTP/1.1\r\nHost: ") ||
		!__xwsAppendText(&pBuf, &iLen, &iCap, sHost) ||
		!__xwsAppendText(&pBuf, &iLen, &iCap, "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: ") ||
		!__xwsAppendText(&pBuf, &iLen, &iCap, pClient->sKey) ||
		!__xwsAppendText(&pBuf, &iLen, &iCap, "\r\nSec-WebSocket-Version: 13\r\n") ) {
		XNET_FREE(pBuf);
		return false;
	}
	if ( pClient->tConfig.sProtocol[0] ) {
		if ( !__xwsAppendText(&pBuf, &iLen, &iCap, "Sec-WebSocket-Protocol: ") ||
			!__xwsAppendText(&pBuf, &iLen, &iCap, pClient->tConfig.sProtocol) ||
			!__xwsAppendText(&pBuf, &iLen, &iCap, "\r\n") ) {
			XNET_FREE(pBuf);
			return false;
		}
	}
	if ( pClient->tConfig.sOrigin[0] ) {
		if ( !__xwsAppendText(&pBuf, &iLen, &iCap, "Origin: ") ||
			!__xwsAppendText(&pBuf, &iLen, &iCap, pClient->tConfig.sOrigin) ||
			!__xwsAppendText(&pBuf, &iLen, &iCap, "\r\n") ) {
			XNET_FREE(pBuf);
			return false;
		}
	}
	if ( !__xwsAppendText(&pBuf, &iLen, &iCap, "\r\n") ) {
		XNET_FREE(pBuf);
		return false;
	}
	*ppOut = pBuf;
	*pOutLen = iLen;
	return true;
}

static bool __xwsBuildHttpResponseBytes(uint32 iStatusCode, const char* sBody, const char* sAccept, const char* sProtocol, char** ppOut, size_t* pOutLen)
{
	char sLine[256];
	char sLength[64];
	char* pBuf = NULL;
	size_t iLen = 0;
	size_t iCap = 0;
	size_t iBodyLen = sBody ? strlen(sBody) : 0u;
	if ( !ppOut || !pOutLen ) return false;
	(void)snprintf(sLine, sizeof(sLine), "HTTP/1.1 %u %s\r\n", (unsigned)iStatusCode, __xwsHttpStatusText(iStatusCode));
	if ( !__xwsAppendText(&pBuf, &iLen, &iCap, sLine) ) {
		XNET_FREE(pBuf);
		return false;
	}
	if ( iStatusCode == 101u ) {
		if ( !__xwsAppendText(&pBuf, &iLen, &iCap, "Upgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ") ||
			!__xwsAppendText(&pBuf, &iLen, &iCap, sAccept ? sAccept : "") ||
			!__xwsAppendText(&pBuf, &iLen, &iCap, "\r\n") ) {
			XNET_FREE(pBuf);
			return false;
		}
		if ( sProtocol && sProtocol[0] ) {
			if ( !__xwsAppendText(&pBuf, &iLen, &iCap, "Sec-WebSocket-Protocol: ") ||
				!__xwsAppendText(&pBuf, &iLen, &iCap, sProtocol) ||
				!__xwsAppendText(&pBuf, &iLen, &iCap, "\r\n") ) {
				XNET_FREE(pBuf);
				return false;
			}
		}
		if ( !__xwsAppendText(&pBuf, &iLen, &iCap, "\r\n") ) {
			XNET_FREE(pBuf);
			return false;
		}
	} else {
		(void)snprintf(sLength, sizeof(sLength), "%llu", (unsigned long long)iBodyLen);
		if ( !__xwsAppendText(&pBuf, &iLen, &iCap, "Connection: close\r\nContent-Type: text/plain\r\nContent-Length: ") ||
			!__xwsAppendText(&pBuf, &iLen, &iCap, sLength) ||
			!__xwsAppendText(&pBuf, &iLen, &iCap, "\r\n\r\n") ||
			!__xwsAppendBytes(&pBuf, &iLen, &iCap, sBody, iBodyLen) ) {
			XNET_FREE(pBuf);
			return false;
		}
	}
	*ppOut = pBuf;
	*pOutLen = iLen;
	return true;
}

static void __xwsMessageReset(char** ppBuf, size_t* pLen, size_t* pCap, uint8* pOpcode)
{
	if ( ppBuf && *ppBuf ) {
		XNET_FREE(*ppBuf);
		*ppBuf = NULL;
	}
	if ( pLen ) *pLen = 0u;
	if ( pCap ) *pCap = 0u;
	if ( pOpcode ) *pOpcode = 0u;
}

static int __xwsMessageAppend(char** ppBuf, size_t* pLen, size_t* pCap, uint8* pOpcode, uint8 iOpcode, const void* pPayload, size_t iPayloadLen, size_t iLimit)
{
	if ( !ppBuf || !pLen || !pCap || !pOpcode || (!pPayload && iPayloadLen != 0u) ) return __XWS_APPEND_INTERNAL;
	if ( iLimit > 0u && *pLen + iPayloadLen > iLimit ) return __XWS_APPEND_TOO_BIG;
	if ( iOpcode != 0u ) *pOpcode = iOpcode;
	if ( !__xwsAppendBytes(ppBuf, pLen, pCap, pPayload, iPayloadLen) ) return __XWS_APPEND_INTERNAL;
	return __XWS_APPEND_OK;
}

static size_t __xwsFrameSize(size_t iPayloadLen, bool bMask)
{
	size_t iSize = 2u + iPayloadLen;
	if ( iPayloadLen >= 126u && iPayloadLen <= 0xFFFFu ) iSize += 2u;
	else if ( iPayloadLen > 0xFFFFu ) iSize += 8u;
	if ( bMask ) iSize += 4u;
	return iSize;
}

static bool __xwsBuildFrameBytesEx(uint8 iOpcode, bool bFin, bool bMask, const void* pPayload, size_t iPayloadLen, char** ppOut, size_t* pOutLen)
{
	char* pBuf;
	size_t iNeed;
	size_t iPos = 0;
	uint8 aMask[4] = {0};
	const uint8* pBytes = (const uint8*)pPayload;
	if ( !ppOut || !pOutLen ) return false;
	iNeed = __xwsFrameSize(iPayloadLen, bMask);
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

static bool __xwsBuildFrameBytes(uint8 iOpcode, bool bMask, const void* pPayload, size_t iPayloadLen, char** ppOut, size_t* pOutLen)
{
	return __xwsBuildFrameBytesEx(iOpcode, true, bMask, pPayload, iPayloadLen, ppOut, pOutLen);
}

static bool __xwsBuildClosePayload(uint16 iCode, const char* sReason, size_t iReasonLen, char** ppOut, size_t* pOutLen)
{
	char* pBuf;
	if ( !ppOut || !pOutLen ) return false;
	if ( iCode == 0 && (!sReason || iReasonLen == 0u) ) {
		*ppOut = NULL;
		*pOutLen = 0u;
		return true;
	}
	if ( iCode == 0u ) iCode = XWS_CLOSE_NORMAL;
	if ( iReasonLen > XWS_CLOSE_REASON_CAP ) iReasonLen = XWS_CLOSE_REASON_CAP;
	pBuf = (char*)XNET_ALLOC(2u + iReasonLen);
	if ( !pBuf ) return false;
	pBuf[0] = (char)((iCode >> 8u) & 0xFFu);
	pBuf[1] = (char)(iCode & 0xFFu);
	if ( sReason && iReasonLen > 0 ) memcpy(pBuf + 2u, sReason, iReasonLen);
	*ppOut = pBuf;
	*pOutLen = 2u + iReasonLen;
	return true;
}

static bool __xwsStreamQueueBytesDirect(xnetstream* pStream, const void* pData, size_t iLen)
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

static xnet_result __xwsStreamSendFrame(xnetstream* pStream, bool bMask, uint8 iOpcode, const void* pPayload, size_t iPayloadLen)
{
	char* pFrame = NULL;
	size_t iFrameLen = 0u;
	xnet_result iRet;
	if ( !pStream ) return XRT_NET_ERROR;
	if ( !__xwsBuildFrameBytesEx(iOpcode, true, bMask, pPayload, iPayloadLen, &pFrame, &iFrameLen) ) return XRT_NET_ERROR;
	iRet = xrtNetStreamSend(pStream, pFrame, iFrameLen);
	XNET_FREE(pFrame);
	return iRet;
}

static xnet_result UNUSED_ATTR __xwsStreamSendFrameEx(xnetstream* pStream, bool bFin, bool bMask, uint8 iOpcode, const void* pPayload, size_t iPayloadLen)
{
	char* pFrame = NULL;
	size_t iFrameLen = 0u;
	xnet_result iRet;
	if ( !pStream ) return XRT_NET_ERROR;
	if ( !__xwsBuildFrameBytesEx(iOpcode, bFin, bMask, pPayload, iPayloadLen, &pFrame, &iFrameLen) ) return XRT_NET_ERROR;
	iRet = xrtNetStreamSend(pStream, pFrame, iFrameLen);
	XNET_FREE(pFrame);
	return iRet;
}

static bool UNUSED_ATTR __xwsStreamQueueFrameDirectEx(xnetstream* pStream, bool bFin, bool bMask, uint8 iOpcode, const void* pPayload, size_t iPayloadLen)
{
	char* pFrame = NULL;
	size_t iFrameLen = 0u;
	bool bRet;
	if ( !pStream ) return false;
	if ( !__xwsBuildFrameBytesEx(iOpcode, bFin, bMask, pPayload, iPayloadLen, &pFrame, &iFrameLen) ) return false;
	bRet = __xwsStreamQueueBytesDirect(pStream, pFrame, iFrameLen);
	XNET_FREE(pFrame);
	return bRet;
}

static bool __xwsStreamQueueFrameDirect(xnetstream* pStream, bool bMask, uint8 iOpcode, const void* pPayload, size_t iPayloadLen)
{
	char* pFrame = NULL;
	size_t iFrameLen = 0u;
	bool bRet;
	if ( !pStream ) return false;
	if ( !__xwsBuildFrameBytesEx(iOpcode, true, bMask, pPayload, iPayloadLen, &pFrame, &iFrameLen) ) return false;
	bRet = __xwsStreamQueueBytesDirect(pStream, pFrame, iFrameLen);
	XNET_FREE(pFrame);
	return bRet;
}

static void __xwsCloseTask(xnetworker* pWorker, ptr pArg)
{
	__xws_close_task* pTask = (__xws_close_task*)pArg;
	(void)pWorker;
	if ( !pTask ) return;
	if ( pTask->pStream && pTask->pFrame && pTask->iFrameLen > 0u ) {
		(void)__xwsStreamQueueBytesDirect(pTask->pStream, pTask->pFrame, pTask->iFrameLen);
	}
	if ( pTask->pStream && pTask->bCloseStream ) {
		xrtNetStreamClose(pTask->pStream, XNET_CLOSE_F_GRACEFUL | XNET_CLOSE_F_WAIT_PEER);
	}
	XNET_FREE(pTask->pFrame);
	XNET_FREE(pTask);
}

static xnet_result __xwsPostClose(xnetstream* pStream, bool bMask, uint16 iCode, const char* sReason, bool bCloseStream)
{
	__xws_close_task* pTask;
	char* pPayload = NULL;
	char* pFrame = NULL;
	size_t iPayloadLen = 0u;
	size_t iFrameLen = 0u;
	size_t iReasonLen = sReason ? strlen(sReason) : 0u;
	if ( !pStream || !pStream->pEngine || !pStream->pWorker ) return XRT_NET_ERROR;
	if ( !__xwsBuildClosePayload(iCode, sReason, iReasonLen, &pPayload, &iPayloadLen) ) return XRT_NET_ERROR;
	if ( !__xwsBuildFrameBytes(XCODEC_WS_OPCODE_CLOSE, bMask, pPayload, iPayloadLen, &pFrame, &iFrameLen) ) {
		XNET_FREE(pPayload);
		return XRT_NET_ERROR;
	}
	XNET_FREE(pPayload);
	pTask = (__xws_close_task*)XNET_ALLOC(sizeof(__xws_close_task));
	if ( !pTask ) {
		XNET_FREE(pFrame);
		return XRT_NET_ERROR;
	}
	memset(pTask, 0, sizeof(__xws_close_task));
	pTask->pStream = pStream;
	pTask->pFrame = pFrame;
	pTask->iFrameLen = iFrameLen;
	pTask->bCloseStream = bCloseStream;
	if ( xrtNetEnginePost(pStream->pEngine, pStream->pWorker->iId, __xwsCloseTask, pTask) != XRT_NET_OK ) {
		XNET_FREE(pFrame);
		XNET_FREE(pTask);
		return XRT_NET_ERROR;
	}
	return XRT_NET_OK;
}

static bool __xwsPeekPayloadCopy(xnetchain* pChain, const xcodecframe* pFrame, const xcodecwsframeinfo* pInfo, char** ppPayload, size_t* pPayloadLen)
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

static void __xwsClientEmitText(xwsclient* pClient, const char* pData, size_t iLen)
{
	if ( pClient && pClient->tEvents.OnText ) {
		pClient->tEvents.OnText(pClient->pUserData, pClient, pData, iLen);
	}
}

static void __xwsClientEmitBinary(xwsclient* pClient, const void* pData, size_t iLen)
{
	if ( pClient && pClient->tEvents.OnBinary ) {
		pClient->tEvents.OnBinary(pClient->pUserData, pClient, pData, iLen);
	}
}

static void __xwsServerEmitText(xwsconn* pConn, const char* pData, size_t iLen)
{
	xwsserver* pServer = pConn ? pConn->pServer : NULL;
	if ( pServer && pServer->tEvents.OnText ) {
		pServer->tEvents.OnText(pServer->pUserData, pServer, pConn, pData, iLen);
	}
}

static void __xwsServerEmitBinary(xwsconn* pConn, const void* pData, size_t iLen)
{
	xwsserver* pServer = pConn ? pConn->pServer : NULL;
	if ( pServer && pServer->tEvents.OnBinary ) {
		pServer->tEvents.OnBinary(pServer->pUserData, pServer, pConn, pData, iLen);
	}
}

static int __xwsClientConsumeDataFrame(xwsclient* pClient, uint8 iOpcode, bool bFin, const char* pPayload, size_t iPayloadLen)
{
	size_t iLimit = pClient && pClient->tConfig.iRecvLimit > 0u ? (size_t)pClient->tConfig.iRecvLimit : 0u;
	int iAppend;
	if ( !pClient ) return __XWS_APPEND_INTERNAL;
	if ( iOpcode == XCODEC_WS_OPCODE_CONT ) {
		if ( pClient->iMsgOpcode == 0u ) return __XWS_APPEND_PROTOCOL;
		iAppend = __xwsMessageAppend(&pClient->pMsgBuf, &pClient->iMsgLen, &pClient->iMsgCap, &pClient->iMsgOpcode, 0u, pPayload, iPayloadLen, iLimit);
		if ( iAppend != __XWS_APPEND_OK ) return iAppend;
		if ( bFin ) {
			if ( pClient->iMsgOpcode == XCODEC_WS_OPCODE_TEXT ) __xwsClientEmitText(pClient, pClient->pMsgBuf ? pClient->pMsgBuf : "", pClient->iMsgLen);
			else if ( pClient->iMsgOpcode == XCODEC_WS_OPCODE_BINARY ) __xwsClientEmitBinary(pClient, pClient->pMsgBuf, pClient->iMsgLen);
			else return __XWS_APPEND_INTERNAL;
			__xwsMessageReset(&pClient->pMsgBuf, &pClient->iMsgLen, &pClient->iMsgCap, &pClient->iMsgOpcode);
		}
		return __XWS_APPEND_OK;
	}
	if ( iOpcode != XCODEC_WS_OPCODE_TEXT && iOpcode != XCODEC_WS_OPCODE_BINARY ) return __XWS_APPEND_PROTOCOL;
	if ( pClient->iMsgOpcode != 0u ) return __XWS_APPEND_PROTOCOL;
	if ( bFin ) {
		if ( iOpcode == XCODEC_WS_OPCODE_TEXT ) __xwsClientEmitText(pClient, pPayload, iPayloadLen);
		else __xwsClientEmitBinary(pClient, pPayload, iPayloadLen);
		return __XWS_APPEND_OK;
	}
	return __xwsMessageAppend(&pClient->pMsgBuf, &pClient->iMsgLen, &pClient->iMsgCap, &pClient->iMsgOpcode, iOpcode, pPayload, iPayloadLen, iLimit);
}

static int __xwsServerConsumeDataFrame(xwsconn* pConn, uint8 iOpcode, bool bFin, const char* pPayload, size_t iPayloadLen)
{
	size_t iLimit = (pConn && pConn->pServer && pConn->pServer->tConfig.iRecvLimit > 0u) ? (size_t)pConn->pServer->tConfig.iRecvLimit : 0u;
	int iAppend;
	if ( !pConn ) return __XWS_APPEND_INTERNAL;
	if ( iOpcode == XCODEC_WS_OPCODE_CONT ) {
		if ( pConn->iMsgOpcode == 0u ) return __XWS_APPEND_PROTOCOL;
		iAppend = __xwsMessageAppend(&pConn->pMsgBuf, &pConn->iMsgLen, &pConn->iMsgCap, &pConn->iMsgOpcode, 0u, pPayload, iPayloadLen, iLimit);
		if ( iAppend != __XWS_APPEND_OK ) return iAppend;
		if ( bFin ) {
			if ( pConn->iMsgOpcode == XCODEC_WS_OPCODE_TEXT ) __xwsServerEmitText(pConn, pConn->pMsgBuf ? pConn->pMsgBuf : "", pConn->iMsgLen);
			else if ( pConn->iMsgOpcode == XCODEC_WS_OPCODE_BINARY ) __xwsServerEmitBinary(pConn, pConn->pMsgBuf, pConn->iMsgLen);
			else return __XWS_APPEND_INTERNAL;
			__xwsMessageReset(&pConn->pMsgBuf, &pConn->iMsgLen, &pConn->iMsgCap, &pConn->iMsgOpcode);
		}
		return __XWS_APPEND_OK;
	}
	if ( iOpcode != XCODEC_WS_OPCODE_TEXT && iOpcode != XCODEC_WS_OPCODE_BINARY ) return __XWS_APPEND_PROTOCOL;
	if ( pConn->iMsgOpcode != 0u ) return __XWS_APPEND_PROTOCOL;
	if ( bFin ) {
		if ( iOpcode == XCODEC_WS_OPCODE_TEXT ) __xwsServerEmitText(pConn, pPayload, iPayloadLen);
		else __xwsServerEmitBinary(pConn, pPayload, iPayloadLen);
		return __XWS_APPEND_OK;
	}
	return __xwsMessageAppend(&pConn->pMsgBuf, &pConn->iMsgLen, &pConn->iMsgCap, &pConn->iMsgOpcode, iOpcode, pPayload, iPayloadLen, iLimit);
}


/* ============================== Client helpers ============================== */

static void __xwsClientEmitError(xwsclient* pClient, int iSysErr)
{
	if ( !pClient ) return;
	pClient->iLastSysErr = iSysErr;
	if ( pClient->tEvents.OnError ) {
		pClient->tEvents.OnError(pClient->pUserData, pClient, iSysErr);
	}
}

static void __xwsClientEmitCloseOnce(xwsclient* pClient, xnet_result iReason)
{
	if ( !pClient ) return;
	if ( __xwsAtomicCompareExchange(&pClient->iCloseNotified, 1, 0) != 0 ) return;
	if ( pClient->tEvents.OnClose ) {
		pClient->tEvents.OnClose(pClient->pUserData, pClient, iReason);
	}
}

static bool __xwsIsBenignStreamError(int iSysErr, xnetstream* pStream, volatile long* pClosePosted, volatile long* pCloseNotified)
{
	if ( pStream && pStream->bClosing ) return true;
	if ( pClosePosted && __xwsAtomicLoad(pClosePosted) != 0 ) return true;
	if ( pCloseNotified && __xwsAtomicLoad(pCloseNotified) != 0 ) return true;
	if ( iSysErr == -1 ) return (pClosePosted && __xwsAtomicLoad(pClosePosted) != 0) || (pCloseNotified && __xwsAtomicLoad(pCloseNotified) != 0);
	#if defined(_WIN32) || defined(_WIN64)
		return iSysErr == WSAECONNRESET || iSysErr == WSAECONNABORTED || iSysErr == WSAESHUTDOWN ||
			iSysErr == WSAENOTSOCK || iSysErr == WSA_OPERATION_ABORTED;
	#else
		return iSysErr == ECONNRESET || iSysErr == ECONNABORTED || iSysErr == EPIPE ||
			iSysErr == ESHUTDOWN || iSysErr == ENOTSOCK || iSysErr == EBADF;
	#endif
}

static bool __xwsClientValidateHandshake(xwsclient* pClient, const xcodechttp1msg* pMsg)
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
	if ( !sUpgrade || !__xwsStrEqNoCase(sUpgrade, "websocket") ) return false;
	if ( !sConnection || !__xwsContainsTokenNoCase(sConnection, "Upgrade") ) return false;
	if ( !sAccept || !__xwsStrEqNoCase(sAccept, pClient->sExpectedAccept) ) return false;
	if ( pClient->tConfig.sProtocol[0] ) {
		sProtocol = xrtCodecHttp1GetHeader(pMsg, "Sec-WebSocket-Protocol");
		if ( !sProtocol || !__xwsStrEqNoCase(sProtocol, pClient->tConfig.sProtocol) ) return false;
	}
	return true;
}

static void __xwsClientConsumeFrames(xwsclient* pClient, xnetchain* pChain)
{
	while ( pClient && pChain ) {
		xcodecframe tFrame;
		xcodecwsframeinfo tInfo;
		xcodecstatus iParse = xrtCodecWsParseFrame(pChain, &tFrame, &tInfo);
		char* pPayload = NULL;
		size_t iPayloadLen = 0u;
		bool bFin;
		int iDataRet;
		uint16 iCloseCode = XWS_CLOSE_NORMAL;
		if ( iParse == XCODEC_STATUS_NEED_MORE ) return;
		if ( iParse == XCODEC_STATUS_ERROR ) {
			__xwsClientEmitError(pClient, -2);
			if ( pClient->pStream ) xrtNetStreamClose(pClient->pStream, XNET_CLOSE_F_ABORT);
			return;
		}
		if ( (tInfo.iFlags & XCODEC_WS_F_CONTROL) != 0u && tInfo.iPayloadLen > 125u ) {
			(void)__xwsPostClose(pClient->pStream, true, XWS_CLOSE_PROTOCOL, "control too large", true);
			return;
		}
		if ( !__xwsPeekPayloadCopy(pChain, &tFrame, &tInfo, &pPayload, &iPayloadLen) ) {
			__xwsClientEmitError(pClient, -3);
			xrtNetStreamClose(pClient->pStream, XNET_CLOSE_F_ABORT);
			return;
		}
		xrtCodecFrameConsume(pChain, &tFrame);
		bFin = (tInfo.iFlags & XCODEC_WS_F_FIN) != 0u;
		switch ( tInfo.iOpcode ) {
			case XCODEC_WS_OPCODE_TEXT:
			case XCODEC_WS_OPCODE_BINARY:
			case XCODEC_WS_OPCODE_CONT:
				iDataRet = __xwsClientConsumeDataFrame(pClient, tInfo.iOpcode, bFin, pPayload, iPayloadLen);
				if ( iDataRet == __XWS_APPEND_OK ) break;
				if ( iDataRet == __XWS_APPEND_TOO_BIG ) {
					(void)__xwsPostClose(pClient->pStream, true, XWS_CLOSE_TOO_BIG, "message too large", true);
				} else if ( iDataRet == __XWS_APPEND_PROTOCOL ) {
					(void)__xwsPostClose(pClient->pStream, true, XWS_CLOSE_PROTOCOL, "bad fragment sequence", true);
				} else {
					__xwsClientEmitError(pClient, -7);
					xrtNetStreamClose(pClient->pStream, XNET_CLOSE_F_ABORT);
				}
				XNET_FREE(pPayload);
				return;
				break;
			case XCODEC_WS_OPCODE_PING:
				if ( pClient->pStream ) (void)__xwsStreamQueueFrameDirect(pClient->pStream, true, XCODEC_WS_OPCODE_PONG, pPayload, iPayloadLen);
				if ( pClient->tEvents.OnPing ) pClient->tEvents.OnPing(pClient->pUserData, pClient, pPayload, iPayloadLen);
				break;
			case XCODEC_WS_OPCODE_PONG:
				if ( pClient->tEvents.OnPong ) pClient->tEvents.OnPong(pClient->pUserData, pClient, pPayload, iPayloadLen);
				break;
			case XCODEC_WS_OPCODE_CLOSE:
				if ( iPayloadLen >= 2u ) {
					iCloseCode = (uint16)(((uint8)pPayload[0] << 8u) | (uint8)pPayload[1]);
				}
				#if defined(XNET_DEBUG_CLOSE_DIAG)
					fprintf(stderr, "[CLOSE_DIAG][WS-CLIENT] recv close stream=%p code=%u posted=%ld open=%ld len=%zu\n",
						(void*)pClient->pStream,
						(unsigned)iCloseCode,
						(long)__xwsAtomicLoad(&pClient->iClosePosted),
						(long)__xwsAtomicLoad(&pClient->iOpen),
						iPayloadLen);
				#endif
				__xwsClientEmitCloseOnce(pClient, XRT_NET_CLOSED);
				if ( __xwsAtomicCompareExchange(&pClient->iClosePosted, 1, 0) == 0 ) {
					if ( pClient->pStream ) (void)__xwsPostClose(pClient->pStream, true, iCloseCode, NULL, true);
				} else if ( pClient->pStream ) {
					xrtNetStreamClose(pClient->pStream, XNET_CLOSE_F_GRACEFUL);
				}
				XNET_FREE(pPayload);
				return;
			default:
				(void)__xwsPostClose(pClient->pStream, true, XWS_CLOSE_PROTOCOL, "bad opcode", true);
				XNET_FREE(pPayload);
				return;
		}
		XNET_FREE(pPayload);
	}
}

static void __xwsClientStreamOnOpen(ptr pOwner, xnetstream* pStream)
{
	xwsclient* pClient = (xwsclient*)pOwner;
	char* pHandshake = NULL;
	size_t iHandshakeLen = 0u;
	if ( !pClient || !pStream ) return;
	if ( !__xwsBuildClientHandshake(pClient, &pHandshake, &iHandshakeLen) ) {
		__xwsClientEmitError(pClient, -4);
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return;
	}
	if ( xrtNetStreamSend(pStream, pHandshake, iHandshakeLen) != XRT_NET_OK ) {
		__xwsClientEmitError(pClient, -5);
		XNET_FREE(pHandshake);
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return;
	}
	XNET_FREE(pHandshake);
}

static void __xwsClientStreamOnRecv(ptr pOwner, xnetstream* pStream, xnetchain* pChain)
{
	xwsclient* pClient = (xwsclient*)pOwner;
	if ( !pClient || !pStream || !pChain ) return;
	if ( __xwsAtomicLoad(&pClient->iOpen) == 0 ) {
		xcodecframe tFrame;
		xcodechttp1msg tMsg;
		xcodecstatus iParse = xrtCodecHttp1Parse(pChain, &tFrame, &tMsg);
		if ( iParse == XCODEC_STATUS_NEED_MORE ) return;
		if ( iParse == XCODEC_STATUS_ERROR || !__xwsClientValidateHandshake(pClient, &tMsg) ) {
			__xwsClientEmitError(pClient, -6);
			xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
			return;
		}
		xrtCodecFrameConsume(pChain, &tFrame);
		(void)__xwsAtomicCompareExchange(&pClient->iOpen, 1, 0);
		if ( pClient->tEvents.OnOpen ) {
			pClient->tEvents.OnOpen(pClient->pUserData, pClient);
		}
	}
	__xwsClientConsumeFrames(pClient, pChain);
}

static void __xwsClientStreamOnClose(ptr pOwner, xnetstream* pStream, xnet_result iReason)
{
	xwsclient* pClient = (xwsclient*)pOwner;
	#if defined(XNET_DEBUG_CLOSE_DIAG)
		fprintf(stderr, "[CLOSE_DIAG][WS-CLIENT] stream close stream=%p reason=%d posted=%ld notified=%ld\n",
			(void*)pStream,
			(int)iReason,
			pClient ? (long)__xwsAtomicLoad(&pClient->iClosePosted) : -1L,
			pClient ? (long)__xwsAtomicLoad(&pClient->iCloseNotified) : -1L);
	#endif
	(void)pStream;
	__xwsClientEmitCloseOnce(pClient, iReason);
}

static void __xwsClientStreamOnError(ptr pOwner, xnetstream* pStream, int iSysErr)
{
	xwsclient* pClient = (xwsclient*)pOwner;
	(void)pStream;
	if ( pClient && __xwsIsBenignStreamError(iSysErr, pStream, &pClient->iClosePosted, &pClient->iCloseNotified) ) return;
	__xwsClientEmitError(pClient, iSysErr);
}

static const xnetstreamevents* __xwsClientStreamEvents(void)
{
	static const xnetstreamevents tEvents = {
		__xwsClientStreamOnOpen,
		__xwsClientStreamOnRecv,
		NULL,
		__xwsClientStreamOnClose,
		__xwsClientStreamOnError,
		NULL,
		NULL
	};
	return &tEvents;
}


/* ============================== Server helpers ============================== */

static void __xwsServerAddConn(xwsserver* pServer, xwsconn* pConn)
{
	if ( !pServer || !pConn ) return;
	__xwsLock(&pServer->iConnLock);
	pConn->pNext = pServer->pConnHead;
	pServer->pConnHead = pConn;
	__xwsUnlock(&pServer->iConnLock);
}

static void __xwsServerRemoveConn(xwsserver* pServer, xwsconn* pConn)
{
	xwsconn** ppNode;
	if ( !pServer || !pConn ) return;
	__xwsLock(&pServer->iConnLock);
	ppNode = &pServer->pConnHead;
	while ( *ppNode ) {
		if ( *ppNode == pConn ) {
			*ppNode = pConn->pNext;
			break;
		}
		ppNode = &(*ppNode)->pNext;
	}
	__xwsUnlock(&pServer->iConnLock);
}

static xwsconn* __xwsServerDetachAllConns(xwsserver* pServer)
{
	xwsconn* pHead;
	if ( !pServer ) return NULL;
	__xwsLock(&pServer->iConnLock);
	pHead = pServer->pConnHead;
	pServer->pConnHead = NULL;
	__xwsUnlock(&pServer->iConnLock);
	return pHead;
}

static void __xwsConnCleanupTask(xnetworker* pWorker, ptr pArg)
{
	xwsconn* pConn = (xwsconn*)pArg;
	(void)pWorker;
	if ( !pConn ) return;
	if ( pConn->pServer ) {
		__xwsServerRemoveConn(pConn->pServer, pConn);
		pConn->pServer = NULL;
	}
	if ( pConn->pStream ) {
		xrtNetStreamDestroy(pConn->pStream);
		pConn->pStream = NULL;
	}
	__xwsMessageReset(&pConn->pMsgBuf, &pConn->iMsgLen, &pConn->iMsgCap, &pConn->iMsgOpcode);
	XNET_FREE(pConn);
}

static void __xwsConnPostCleanup(xwsconn* pConn)
{
	if ( !pConn ) return;
	if ( __xwsAtomicCompareExchange(&pConn->iCleanupPosted, 1, 0) != 0 ) return;
	if ( pConn->pServer && pConn->pServer->pEngine && pConn->pStream && pConn->pStream->pWorker ) {
		if ( xrtNetEnginePost(pConn->pServer->pEngine, pConn->pStream->pWorker->iId, __xwsConnCleanupTask, pConn) == XRT_NET_OK ) {
			return;
		}
	}
	__xwsConnCleanupTask(NULL, pConn);
}

static void __xwsServerEmitError(xwsserver* pServer, xwsconn* pConn, int iSysErr)
{
	if ( pConn ) pConn->iLastSysErr = iSysErr;
	if ( pServer && pServer->tEvents.OnError ) {
		pServer->tEvents.OnError(pServer->pUserData, pServer, pConn, iSysErr);
	}
}

static void __xwsServerEmitCloseOnce(xwsserver* pServer, xwsconn* pConn, xnet_result iReason)
{
	if ( !pConn ) return;
	if ( __xwsAtomicCompareExchange(&pConn->iCloseNotified, 1, 0) != 0 ) return;
	if ( pServer && pServer->tEvents.OnClose ) {
		pServer->tEvents.OnClose(pServer->pUserData, pServer, pConn, iReason);
	}
}

static bool __xwsSendHttpReply(xnetstream* pStream, uint32 iStatusCode, const char* sBody, const char* sAccept, const char* sProtocol, bool bClose)
{
	char* pBytes = NULL;
	size_t iLen = 0u;
	if ( !pStream ) return false;
	if ( !__xwsBuildHttpResponseBytes(iStatusCode, sBody, sAccept, sProtocol, &pBytes, &iLen) ) return false;
	if ( !__xwsStreamQueueBytesDirect(pStream, pBytes, iLen) ) {
		XNET_FREE(pBytes);
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return false;
	}
	XNET_FREE(pBytes);
	if ( bClose ) xrtNetStreamClose(pStream, XNET_CLOSE_F_GRACEFUL);
	return true;
}

static bool __xwsServerValidateRequest(const xcodechttp1msg* pMsg, const char** psKey)
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
	if ( !sUpgrade || !__xwsStrEqNoCase(sUpgrade, "websocket") ) return false;
	if ( !sConnection || !__xwsContainsTokenNoCase(sConnection, "Upgrade") ) return false;
	if ( !sKey || sKey[0] == '\0' ) return false;
	if ( sVersion && strcmp(sVersion, "13") != 0 ) return false;
	*psKey = sKey;
	return true;
}

static void __xwsServerConsumeFrames(xwsconn* pConn, xnetchain* pChain)
{
	xwsserver* pServer = pConn ? pConn->pServer : NULL;
	while ( pConn && pServer && pChain ) {
		xcodecframe tFrame;
		xcodecwsframeinfo tInfo;
		xcodecstatus iParse = xrtCodecWsParseFrame(pChain, &tFrame, &tInfo);
		char* pPayload = NULL;
		size_t iPayloadLen = 0u;
		bool bFin;
		int iDataRet;
		uint16 iCloseCode = XWS_CLOSE_NORMAL;
		if ( iParse == XCODEC_STATUS_NEED_MORE ) return;
		if ( iParse == XCODEC_STATUS_ERROR ) {
			__xwsServerEmitError(pServer, pConn, -21);
			xrtNetStreamClose(pConn->pStream, XNET_CLOSE_F_ABORT);
			return;
		}
		if ( (tInfo.iFlags & XCODEC_WS_F_MASKED) == 0u ) {
			(void)__xwsPostClose(pConn->pStream, false, XWS_CLOSE_PROTOCOL, "mask required", true);
			return;
		}
		if ( (tInfo.iFlags & XCODEC_WS_F_CONTROL) != 0u && tInfo.iPayloadLen > 125u ) {
			(void)__xwsPostClose(pConn->pStream, false, XWS_CLOSE_PROTOCOL, "control too large", true);
			return;
		}
		if ( !__xwsPeekPayloadCopy(pChain, &tFrame, &tInfo, &pPayload, &iPayloadLen) ) {
			__xwsServerEmitError(pServer, pConn, -22);
			xrtNetStreamClose(pConn->pStream, XNET_CLOSE_F_ABORT);
			return;
		}
		xrtCodecFrameConsume(pChain, &tFrame);
		bFin = (tInfo.iFlags & XCODEC_WS_F_FIN) != 0u;
		switch ( tInfo.iOpcode ) {
			case XCODEC_WS_OPCODE_TEXT:
			case XCODEC_WS_OPCODE_BINARY:
			case XCODEC_WS_OPCODE_CONT:
				iDataRet = __xwsServerConsumeDataFrame(pConn, tInfo.iOpcode, bFin, pPayload, iPayloadLen);
				if ( iDataRet == __XWS_APPEND_OK ) break;
				if ( iDataRet == __XWS_APPEND_TOO_BIG ) {
					(void)__xwsPostClose(pConn->pStream, false, XWS_CLOSE_TOO_BIG, "message too large", true);
				} else if ( iDataRet == __XWS_APPEND_PROTOCOL ) {
					(void)__xwsPostClose(pConn->pStream, false, XWS_CLOSE_PROTOCOL, "bad fragment sequence", true);
				} else {
					__xwsServerEmitError(pServer, pConn, -23);
					xrtNetStreamClose(pConn->pStream, XNET_CLOSE_F_ABORT);
				}
				XNET_FREE(pPayload);
				return;
				break;
			case XCODEC_WS_OPCODE_PING:
				(void)__xwsStreamQueueFrameDirect(pConn->pStream, false, XCODEC_WS_OPCODE_PONG, pPayload, iPayloadLen);
				if ( pServer->tEvents.OnPing ) pServer->tEvents.OnPing(pServer->pUserData, pServer, pConn, pPayload, iPayloadLen);
				break;
			case XCODEC_WS_OPCODE_PONG:
				if ( pServer->tEvents.OnPong ) pServer->tEvents.OnPong(pServer->pUserData, pServer, pConn, pPayload, iPayloadLen);
				break;
			case XCODEC_WS_OPCODE_CLOSE:
				if ( iPayloadLen >= 2u ) {
					iCloseCode = (uint16)(((uint8)pPayload[0] << 8u) | (uint8)pPayload[1]);
				}
				#if defined(XNET_DEBUG_CLOSE_DIAG)
					fprintf(stderr, "[CLOSE_DIAG][WS-SERVER] recv close stream=%p code=%u posted=%ld open=%ld len=%zu\n",
						(void*)pConn->pStream,
						(unsigned)iCloseCode,
						(long)__xwsAtomicLoad(&pConn->iClosePosted),
						(long)__xwsAtomicLoad(&pConn->iOpen),
						iPayloadLen);
				#endif
				__xwsServerEmitCloseOnce(pServer, pConn, XRT_NET_CLOSED);
				if ( __xwsAtomicCompareExchange(&pConn->iClosePosted, 1, 0) == 0 ) {
					if ( pConn->pStream ) (void)__xwsPostClose(pConn->pStream, false, iCloseCode, NULL, true);
				} else if ( pConn->pStream ) {
					xrtNetStreamClose(pConn->pStream, XNET_CLOSE_F_GRACEFUL);
				}
				XNET_FREE(pPayload);
				return;
			default:
				(void)__xwsPostClose(pConn->pStream, false, XWS_CLOSE_PROTOCOL, "bad opcode", true);
				XNET_FREE(pPayload);
				return;
		}
		XNET_FREE(pPayload);
	}
}

static bool __xwsListenerOnAccept(ptr pOwner, xnetlistener* pListener, xnetstream* pStream)
{
	xwsserver* pServer = (xwsserver*)pOwner;
	xwsconn* pConn;
	(void)pListener;
	if ( !pServer || !pStream ) return false;
	pConn = (xwsconn*)xrtNetStreamGetUserData(pStream);
	if ( !pConn ) {
		pConn = (xwsconn*)XNET_ALLOC(sizeof(xwsconn));
		if ( !pConn ) return false;
		memset(pConn, 0, sizeof(xwsconn));
		xrtNetStreamSetUserData(pStream, pConn);
	}
	pConn->pServer = pServer;
	pConn->pStream = pStream;
	__xwsServerAddConn(pServer, pConn);
	return true;
}

static void __xwsServerStreamOnOpen(ptr pOwner, xnetstream* pStream)
{
	(void)pOwner;
	(void)pStream;
}

static void __xwsServerStreamOnRecv(ptr pOwner, xnetstream* pStream, xnetchain* pChain)
{
	xwsconn* pConn = (xwsconn*)pOwner;
	xwsserver* pServer = pConn ? pConn->pServer : NULL;
	if ( !pConn || !pServer || !pStream || !pChain ) return;
	if ( __xwsAtomicLoad(&pConn->iOpen) == 0 ) {
		xcodecframe tFrame;
		xcodechttp1msg tMsg;
		xcodecstatus iParse = xrtCodecHttp1Parse(pChain, &tFrame, &tMsg);
		const char* sKey = NULL;
		char sAccept[64];
		const char* sClientProtocol;
		if ( iParse == XCODEC_STATUS_NEED_MORE ) return;
		if ( iParse == XCODEC_STATUS_ERROR || !__xwsServerValidateRequest(&tMsg, &sKey) ) {
			__xwsServerEmitError(pServer, pConn, -31);
			(void)__xwsSendHttpReply(pStream, 400u, "Bad Request", NULL, NULL, true);
			return;
		}
		if ( !__xwsComputeAccept(sKey, sAccept, sizeof(sAccept)) ) {
			__xwsServerEmitError(pServer, pConn, -32);
			xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
			return;
		}
		sClientProtocol = xrtCodecHttp1GetHeader(&tMsg, "Sec-WebSocket-Protocol");
		if ( pServer->tConfig.sProtocol[0] ) {
			if ( !sClientProtocol || !__xwsContainsTokenNoCase(sClientProtocol, pServer->tConfig.sProtocol) ) {
				(void)__xwsSendHttpReply(pStream, 400u, "Protocol Required", NULL, NULL, true);
				return;
			}
			__xwsCopyToken(pConn->sProtocol, sizeof(pConn->sProtocol), pServer->tConfig.sProtocol);
		}
		if ( !__xwsSendHttpReply(pStream, 101u, NULL, sAccept, pConn->sProtocol, false) ) {
			__xwsServerEmitError(pServer, pConn, -33);
			return;
		}
		xrtCodecFrameConsume(pChain, &tFrame);
		(void)__xwsAtomicCompareExchange(&pConn->iOpen, 1, 0);
		if ( pServer->tEvents.OnOpen ) {
			pServer->tEvents.OnOpen(pServer->pUserData, pServer, pConn);
		}
	}
	__xwsServerConsumeFrames(pConn, pChain);
}

static void __xwsServerStreamOnClose(ptr pOwner, xnetstream* pStream, xnet_result iReason)
{
	xwsconn* pConn = (xwsconn*)pOwner;
	xwsserver* pServer = pConn ? pConn->pServer : NULL;
	#if defined(XNET_DEBUG_CLOSE_DIAG)
		fprintf(stderr, "[CLOSE_DIAG][WS-SERVER] stream close stream=%p reason=%d posted=%ld notified=%ld\n",
			(void*)pStream,
			(int)iReason,
			pConn ? (long)__xwsAtomicLoad(&pConn->iClosePosted) : -1L,
			pConn ? (long)__xwsAtomicLoad(&pConn->iCloseNotified) : -1L);
	#endif
	(void)pStream;
	__xwsServerEmitCloseOnce(pServer, pConn, iReason);
	__xwsConnPostCleanup(pConn);
}

static void __xwsServerStreamOnError(ptr pOwner, xnetstream* pStream, int iSysErr)
{
	xwsconn* pConn = (xwsconn*)pOwner;
	xwsserver* pServer = pConn ? pConn->pServer : NULL;
	if ( pConn && __xwsIsBenignStreamError(iSysErr, pStream, &pConn->iClosePosted, &pConn->iCloseNotified) ) return;
	__xwsServerEmitError(pServer, pConn, iSysErr);
}

static void __xwsAcceptReady(xnetlistener* pListener, xnet_result iStatus, xnetstream* pStream, ptr pCtx)
{
	xwsserver* pServer = (xwsserver*)pCtx;
	(void)pListener;
	(void)pStream;
	if ( !pServer || __xwsAtomicLoad(&pServer->bRunning) == 0 ) return;
	if ( pServer->pListener && pServer->pListener->bRunning ) {
		(void)__xnetListenerRegisterSyncAcceptWait(pServer->pListener, __xwsAcceptReady, NULL, pServer);
	}
	if ( iStatus != XRT_NET_OK && pServer->tEvents.OnError ) {
		pServer->tEvents.OnError(pServer->pUserData, pServer, NULL, -1);
	}
}

static void __xwsArmAcceptTask(xnetworker* pWorker, ptr pArg)
{
	xwsserver* pServer = (xwsserver*)pArg;
	(void)pWorker;
	if ( !pServer || !pServer->pListener || __xwsAtomicLoad(&pServer->bRunning) == 0 ) return;
	(void)__xnetListenerRegisterSyncAcceptWait(pServer->pListener, __xwsAcceptReady, NULL, pServer);
}

static const xnetlistenerevents* __xwsListenerEvents(void)
{
	static const xnetlistenerevents tEvents = {
		__xwsListenerOnAccept,
		NULL
	};
	return &tEvents;
}

static const xnetstreamevents* __xwsServerStreamEvents(void)
{
	static const xnetstreamevents tEvents = {
		__xwsServerStreamOnOpen,
		__xwsServerStreamOnRecv,
		NULL,
		__xwsServerStreamOnClose,
		__xwsServerStreamOnError,
		NULL,
		NULL
	};
	return &tEvents;
}

/* ============================== Public API ============================== */

XXAPI void xrtWsClientConfigInit(xwsclientconfig* pCfg)
{
	if ( !pCfg ) return;
	memset(pCfg, 0, sizeof(xwsclientconfig));
	pCfg->iConnectTimeoutMs = 5000u;
	pCfg->iRecvLimit = 1024u * 1024u;
	pCfg->bVerifyPeer = true;
}

XXAPI void xrtWsServerConfigInit(xwsserverconfig* pCfg)
{
	if ( !pCfg ) return;
	memset(pCfg, 0, sizeof(xwsserverconfig));
	pCfg->iBacklog = 128u;
	pCfg->iRecvLimit = 1024u * 1024u;
}

XXAPI xwsclient* xrtWsClientCreate(xnetengine* pEngine, const xwsclientconfig* pCfg, const xwsclientevents* pEvents, ptr pUserData)
{
	xwsclient* pClient;
	if ( !pEngine ) return NULL;
	pClient = (xwsclient*)XNET_ALLOC(sizeof(xwsclient));
	if ( !pClient ) return NULL;
	memset(pClient, 0, sizeof(xwsclient));
	pClient->pEngine = pEngine;
	if ( pCfg ) pClient->tConfig = *pCfg;
	else xrtWsClientConfigInit(&pClient->tConfig);
	if ( pClient->tConfig.pProxy ) {
		pClient->tConfig.pProxy = xrtNetProxyAddRef(pClient->tConfig.pProxy);
	}
	if ( pEvents ) pClient->tEvents = *pEvents;
	pClient->pUserData = pUserData;
	return pClient;
}

XXAPI xnet_result xrtWsClientStart(xwsclient* pClient)
{
	xnetconnectconfig tConnCfg;
	if ( !pClient || !pClient->pEngine || pClient->pStream ) return XRT_NET_ERROR;
	if ( !xrtUrlParseFixedTo(pClient->tConfig.sURL, "ws", "wss", &pClient->tURL.bSecure, pClient->tURL.sHost, sizeof(pClient->tURL.sHost), &pClient->tURL.iPort, pClient->tURL.sPath, sizeof(pClient->tURL.sPath)) ) return XRT_NET_ERROR;
	pClient->pStream = xrtNetStreamCreate(pClient->pEngine, __xwsClientStreamEvents(), pClient);
	if ( !pClient->pStream ) return XRT_NET_ERROR;
	xrtNetConnectConfigInit(&tConnCfg);
	tConnCfg.sHost = pClient->tURL.sHost;
	tConnCfg.iPort = pClient->tURL.iPort;
	tConnCfg.iConnectTimeoutMs = pClient->tConfig.iConnectTimeoutMs;
	tConnCfg.iRecvLimit = pClient->tConfig.iRecvLimit;
	tConnCfg.pProxy = pClient->tConfig.pProxy;
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

XXAPI void xrtWsClientStop(xwsclient* pClient)
{
	if ( !pClient ) return;
	if ( pClient->pStream ) {
		xrtNetStreamClose(pClient->pStream, XNET_CLOSE_F_ABORT);
		xrtNetStreamDestroy(pClient->pStream);
		pClient->pStream = NULL;
	}
	__xwsMessageReset(&pClient->pMsgBuf, &pClient->iMsgLen, &pClient->iMsgCap, &pClient->iMsgOpcode);
}

XXAPI void xrtWsClientDestroy(xwsclient* pClient)
{
	if ( !pClient ) return;
	xrtWsClientStop(pClient);
	if ( pClient->tConfig.pProxy ) {
		xrtNetProxyRelease(pClient->tConfig.pProxy);
		pClient->tConfig.pProxy = NULL;
	}
	XNET_FREE(pClient);
}

XXAPI bool xrtWsClientIsOpen(const xwsclient* pClient)
{
	return pClient && __xwsAtomicLoadConst(&pClient->iOpen) != 0 && pClient->pStream != NULL;
}

XXAPI xnet_result xrtWsClientSendText(xwsclient* pClient, const char* sText, size_t iLen)
{
	if ( !pClient || !pClient->pStream || !xrtWsClientIsOpen(pClient) || !sText ) return XRT_NET_ERROR;
	return __xwsStreamSendFrame(pClient->pStream, true, XCODEC_WS_OPCODE_TEXT, sText, iLen);
}

XXAPI xnet_result xrtWsClientSendBinary(xwsclient* pClient, const void* pData, size_t iLen)
{
	if ( !pClient || !pClient->pStream || !xrtWsClientIsOpen(pClient) || (!pData && iLen != 0u) ) return XRT_NET_ERROR;
	return __xwsStreamSendFrame(pClient->pStream, true, XCODEC_WS_OPCODE_BINARY, pData, iLen);
}

XXAPI xnet_result xrtWsClientPing(xwsclient* pClient, const void* pData, size_t iLen)
{
	if ( !pClient || !pClient->pStream || !xrtWsClientIsOpen(pClient) || iLen > 125u ) return XRT_NET_ERROR;
	return __xwsStreamSendFrame(pClient->pStream, true, XCODEC_WS_OPCODE_PING, pData, iLen);
}

XXAPI xnet_result xrtWsClientClose(xwsclient* pClient, uint16 iCode, const char* sReason)
{
	if ( !pClient || !pClient->pStream ) return XRT_NET_ERROR;
	if ( __xwsAtomicCompareExchange(&pClient->iClosePosted, 1, 0) != 0 ) return XRT_NET_OK;
	return __xwsPostClose(pClient->pStream, true, iCode, sReason, false);
}

XXAPI xwsserver* xrtWsServerCreate(xnetengine* pEngine, const xwsserverconfig* pCfg, const xwsserverevents* pEvents, ptr pUserData)
{
	xwsserver* pServer;
	if ( !pEngine ) return NULL;
	pServer = (xwsserver*)XNET_ALLOC(sizeof(xwsserver));
	if ( !pServer ) return NULL;
	memset(pServer, 0, sizeof(xwsserver));
	pServer->pEngine = pEngine;
	if ( pCfg ) pServer->tConfig = *pCfg;
	else xrtWsServerConfigInit(&pServer->tConfig);
	if ( pEvents ) pServer->tEvents = *pEvents;
	pServer->pUserData = pUserData;
	return pServer;
}

XXAPI uint16 xrtWsServerBoundPort(const xwsserver* pServer)
{
	return (pServer && pServer->pListener) ? pServer->pListener->tConfig.tBindAddr.iPort : 0u;
}

XXAPI xnet_result xrtWsServerStart(xwsserver* pServer)
{
	xnetlistenconfig tListenCfg;
	if ( !pServer || !pServer->pEngine ) return XRT_NET_ERROR;
	if ( __xwsAtomicLoad(&pServer->bRunning) != 0 ) return XRT_NET_OK;
	xrtNetListenConfigInit(&tListenCfg);
	tListenCfg.tBindAddr = pServer->tConfig.tBindAddr;
	tListenCfg.iFlags = pServer->tConfig.iFlags;
	tListenCfg.iBacklog = pServer->tConfig.iBacklog;
	tListenCfg.iRecvLimit = pServer->tConfig.iRecvLimit;
	tListenCfg.pTlsConfig = pServer->tConfig.pTlsConfig;
	pServer->pListener = xrtNetListenerCreate(pServer->pEngine, &tListenCfg, __xwsListenerEvents(), __xwsServerStreamEvents(), pServer);
	if ( !pServer->pListener ) return XRT_NET_ERROR;
	if ( xrtNetListenerStart(pServer->pListener) != XRT_NET_OK ) {
		xrtNetListenerDestroy(pServer->pListener);
		pServer->pListener = NULL;
		return XRT_NET_ERROR;
	}
	(void)__xwsAtomicCompareExchange(&pServer->bRunning, 1, 0);
	if ( xrtNetEnginePost(pServer->pEngine, pServer->pListener->pWorker->iId, __xwsArmAcceptTask, pServer) != XRT_NET_OK ) {
		(void)__xwsAtomicCompareExchange(&pServer->bRunning, 0, 1);
		xrtNetListenerStop(pServer->pListener);
		xrtNetListenerDestroy(pServer->pListener);
		pServer->pListener = NULL;
		return XRT_NET_ERROR;
	}
	return XRT_NET_OK;
}

XXAPI void xrtWsServerStop(xwsserver* pServer)
{
	xwsconn* pConn;
	if ( !pServer ) return;
	if ( __xwsAtomicCompareExchange(&pServer->bRunning, 0, 1) == 0 ) {
		/* already stopped */
	}
	if ( pServer->pListener ) {
		xrtNetListenerStop(pServer->pListener);
		xrtNetListenerDestroy(pServer->pListener);
		pServer->pListener = NULL;
	}
	pConn = __xwsServerDetachAllConns(pServer);
	while ( pConn ) {
		xwsconn* pNext = pConn->pNext;
		pConn->pNext = NULL;
		if ( __xwsAtomicLoad(&pConn->iCleanupPosted) != 0 ) {
			pConn->pServer = NULL;
			pConn = pNext;
			continue;
		}
		(void)__xwsAtomicCompareExchange(&pConn->iCleanupPosted, 1, 0);
		if ( pConn->pStream ) {
			xrtNetStreamClose(pConn->pStream, XNET_CLOSE_F_ABORT);
			xrtNetStreamDestroy(pConn->pStream);
			pConn->pStream = NULL;
		}
		__xwsMessageReset(&pConn->pMsgBuf, &pConn->iMsgLen, &pConn->iMsgCap, &pConn->iMsgOpcode);
		XNET_FREE(pConn);
		pConn = pNext;
	}
}

XXAPI void xrtWsServerDestroy(xwsserver* pServer)
{
	if ( !pServer ) return;
	xrtWsServerStop(pServer);
	XNET_FREE(pServer);
}

XXAPI bool xrtWsConnIsOpen(const xwsconn* pConn)
{
	return pConn && __xwsAtomicLoadConst(&pConn->iOpen) != 0 && pConn->pStream != NULL;
}

XXAPI xnet_result xrtWsConnSendText(xwsconn* pConn, const char* sText, size_t iLen)
{
	if ( !pConn || !pConn->pStream || !xrtWsConnIsOpen(pConn) || !sText ) return XRT_NET_ERROR;
	return __xwsStreamSendFrame(pConn->pStream, false, XCODEC_WS_OPCODE_TEXT, sText, iLen);
}

XXAPI xnet_result xrtWsConnSendBinary(xwsconn* pConn, const void* pData, size_t iLen)
{
	if ( !pConn || !pConn->pStream || !xrtWsConnIsOpen(pConn) || (!pData && iLen != 0u) ) return XRT_NET_ERROR;
	return __xwsStreamSendFrame(pConn->pStream, false, XCODEC_WS_OPCODE_BINARY, pData, iLen);
}

XXAPI xnet_result xrtWsConnPing(xwsconn* pConn, const void* pData, size_t iLen)
{
	if ( !pConn || !pConn->pStream || !xrtWsConnIsOpen(pConn) || iLen > 125u ) return XRT_NET_ERROR;
	return __xwsStreamSendFrame(pConn->pStream, false, XCODEC_WS_OPCODE_PING, pData, iLen);
}

XXAPI xnet_result xrtWsConnClose(xwsconn* pConn, uint16 iCode, const char* sReason)
{
	if ( !pConn || !pConn->pStream ) return XRT_NET_ERROR;
	if ( __xwsAtomicCompareExchange(&pConn->iClosePosted, 1, 0) != 0 ) return XRT_NET_OK;
	return __xwsPostClose(pConn->pStream, false, iCode, sReason, false);
}

#endif
