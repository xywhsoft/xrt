#ifndef XRT_XWS_H
#define XRT_XWS_H

#ifndef XRT_NO_XWS

/*
	XRT mainline WebSocket layer on top of xnet.

	This header provides:
	  - async WebSocket client and server wrappers on top of xnet_stream
	  - HTTP upgrade handshake over plain TCP and builtin TLS
	  - text/binary messaging, ping/pong, close, and fragmented message reassembly

	Current limitations:
	  - extensions and permessage-deflate are not implemented
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
#define XWS_CLOSE_INVALID_DATA   1007u
#define XWS_CLOSE_TOO_BIG        1009u
#define XWS_CLOSE_INTERNAL       1011u

#define XWS_CLIENT_CONFIG_VERSION 3u
#define XWS_SERVER_CONFIG_VERSION 3u
#define XWS_CLIENT_EVENTS_VERSION 2u
#define XWS_SERVER_EVENTS_VERSION 2u

#define XWS_CLOSE_F_SENT             0x00000001u
#define XWS_CLOSE_F_RECEIVED         0x00000002u
#define XWS_CLOSE_F_CLEAN            0x00000004u
#define XWS_CLOSE_F_REMOTE_INITIATED 0x00000008u

#define XWS_F_PERMESSAGE_DEFLATE 0x00000001u
#define XWS_HANDSHAKE_ACCEPT 0u
#define XWS_HANDSHAKE_REJECT 1u
#define XWS_HANDSHAKE_ERROR  2u
#define XWS_HANDSHAKE_F_PERMESSAGE_DEFLATE 0x00000001u
#if !defined(XRT_NO_XDEFLATE) && !defined(XRT_NO_XINFLATE)
	#define XWS_HAS_PERMESSAGE_DEFLATE 1
#else
	#define XWS_HAS_PERMESSAGE_DEFLATE 0
#endif

typedef struct xrt_ws_client xwsclient;
typedef struct xrt_ws_server xwsserver;
typedef struct xrt_ws_conn   xwsconn;
typedef struct xrt_ws_writer xwswriter;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	uint32 iFlags;
	xnet_result iTransportResult;
	uint16 iLocalCode;
	uint16 iRemoteCode;
	const char* sRemoteReason;
	size_t iRemoteReasonLen;
} xwscloseinfo;

#define XWS_CLOSE_INFO_VERSION 1u
#define XWS_CLOSE_INFO_V1_SIZE ((uint32)sizeof(xwscloseinfo))

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	uint32 iStatusCode;
	uint32 iFlags;
	const char* sReason;
	const char* sProtocol;
	const void* pBody;
	size_t iBodyLen;
	xhttpheaders* pHeaders;
	ptr pConnectionData;
} xwshandshakeresponse;

#define XWS_HANDSHAKE_RESPONSE_VERSION 1u
#define XWS_HANDSHAKE_RESPONSE_V1_SIZE ((uint32)sizeof(xwshandshakeresponse))

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	char sURL[XWS_URL_CAP];
	char sOrigin[XWS_ORIGIN_CAP];
	char sProtocol[XWS_PROTOCOL_CAP];
	uint32 iConnectTimeoutMs;
	uint32 iRecvLimit;
	uint32 iHighWater;
	uint32 iLowWater;
	uint32 iMaxQueuedBytes;
	uint32 iWebSocketFlags;
	uint32 iCompressMinBytes;
	int32 iCompressionLevel;
	bool bVerifyPeer;
	xnetproxy* pProxy;
	char* pURLStorage;
	char* pOriginStorage;
	char* pProtocolStorage;
	xhttpheaders* pRequestHeadersStorage;
	uint32 iMaxFrameBytes;
	uint32 iHandshakeMaxBytes;
	uint32 iHandshakeTimeoutMs;
	uint32 iCloseTimeoutMs;
} xwsclientconfig;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	xnetaddr tBindAddr;
	uint32 iFlags;
	uint32 iBacklog;
	uint32 iRecvLimit;
	uint32 iHighWater;
	uint32 iLowWater;
	uint32 iMaxQueuedBytes;
	uint32 iWebSocketFlags;
	uint32 iCompressMinBytes;
	int32 iCompressionLevel;
	const xtlsconfig* pTlsConfig;
	char sProtocol[XWS_PROTOCOL_CAP];
	char* pProtocolStorage;
	char* pPathStorage;
	char* pOriginStorage;
	uint32 iMaxFrameBytes;
	uint32 iHandshakeMaxBytes;
	uint32 iHandshakeTimeoutMs;
	uint32 iCloseTimeoutMs;
} xwsserverconfig;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	void (*OnOpen)(ptr pOwner, xwsclient* pClient);
	void (*OnText)(ptr pOwner, xwsclient* pClient, const char* pData, size_t iLen);
	void (*OnBinary)(ptr pOwner, xwsclient* pClient, const void* pData, size_t iLen);
	void (*OnClose)(ptr pOwner, xwsclient* pClient, xnet_result iReason);
	void (*OnError)(ptr pOwner, xwsclient* pClient, int iSysErr);
	void (*OnPing)(ptr pOwner, xwsclient* pClient, const void* pData, size_t iLen);
	void (*OnPong)(ptr pOwner, xwsclient* pClient, const void* pData, size_t iLen);
	void (*OnBackpressure)(ptr pOwner, xwsclient* pClient, size_t iPendingBytes);
	void (*OnWritable)(ptr pOwner, xwsclient* pClient, size_t iPendingBytes);
	void (*OnDrain)(ptr pOwner, xwsclient* pClient);
	void (*OnCloseEx)(ptr pOwner, xwsclient* pClient, const xwscloseinfo* pInfo);
	bool (*OnHandshakeResponse)(ptr pOwner, xwsclient* pClient, const xcodechttp1msg* pResponse);
} xwsclientevents;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	void (*OnOpen)(ptr pOwner, xwsserver* pServer, xwsconn* pConn);
	void (*OnText)(ptr pOwner, xwsserver* pServer, xwsconn* pConn, const char* pData, size_t iLen);
	void (*OnBinary)(ptr pOwner, xwsserver* pServer, xwsconn* pConn, const void* pData, size_t iLen);
	void (*OnClose)(ptr pOwner, xwsserver* pServer, xwsconn* pConn, xnet_result iReason);
	void (*OnError)(ptr pOwner, xwsserver* pServer, xwsconn* pConn, int iSysErr);
	void (*OnPing)(ptr pOwner, xwsserver* pServer, xwsconn* pConn, const void* pData, size_t iLen);
	void (*OnPong)(ptr pOwner, xwsserver* pServer, xwsconn* pConn, const void* pData, size_t iLen);
	void (*OnBackpressure)(ptr pOwner, xwsserver* pServer, xwsconn* pConn, size_t iPendingBytes);
	void (*OnWritable)(ptr pOwner, xwsserver* pServer, xwsconn* pConn, size_t iPendingBytes);
	void (*OnDrain)(ptr pOwner, xwsserver* pServer, xwsconn* pConn);
	void (*OnCloseEx)(ptr pOwner, xwsserver* pServer, xwsconn* pConn, const xwscloseinfo* pInfo);
	uint32 (*OnHandshake)(ptr pOwner, xwsserver* pServer, xwsconn* pConn,
		const xcodechttp1msg* pRequest, xwshandshakeresponse* pResponse);
} xwsserverevents;

#define XWS_CLIENT_CONFIG_V1_SIZE ((uint32)(offsetof(xwsclientconfig, pProtocolStorage) + sizeof(((xwsclientconfig*)0)->pProtocolStorage)))
#define XWS_SERVER_CONFIG_V1_SIZE ((uint32)(offsetof(xwsserverconfig, pProtocolStorage) + sizeof(((xwsserverconfig*)0)->pProtocolStorage)))
#define XWS_CLIENT_EVENTS_V1_SIZE ((uint32)(offsetof(xwsclientevents, OnCloseEx) + sizeof(((xwsclientevents*)0)->OnCloseEx)))
#define XWS_SERVER_EVENTS_V1_SIZE ((uint32)(offsetof(xwsserverevents, OnCloseEx) + sizeof(((xwsserverevents*)0)->OnCloseEx)))
#define XWS_CLIENT_CONFIG_V2_SIZE ((uint32)(offsetof(xwsclientconfig, pRequestHeadersStorage) + sizeof(((xwsclientconfig*)0)->pRequestHeadersStorage)))
#define XWS_SERVER_CONFIG_V2_SIZE ((uint32)(offsetof(xwsserverconfig, pOriginStorage) + sizeof(((xwsserverconfig*)0)->pOriginStorage)))
#define XWS_CLIENT_CONFIG_V3_SIZE ((uint32)sizeof(xwsclientconfig))
#define XWS_SERVER_CONFIG_V3_SIZE ((uint32)sizeof(xwsserverconfig))
#define XWS_CLIENT_EVENTS_V2_SIZE ((uint32)sizeof(xwsclientevents))
#define XWS_SERVER_EVENTS_V2_SIZE ((uint32)sizeof(xwsserverevents))

#endif /* !XRT_BUILD_CORE */

#define __XWS_CONTROL_SEND_RESERVE_BYTES 512u
#define __XWS_MIN_SEND_BUDGET_BYTES      1024u
#define __XWS_MIN_HANDSHAKE_BYTES         256u
#define __XWS_TRANSPORT_RECV_SLACK_BYTES  65536u

typedef struct {
	bool bSecure;
	uint16 iPort;
	char* sHost;
	char* sPath;
} __xws_url;

typedef struct __xws_timer_ctx __xws_timer_ctx;
typedef struct __xws_open_waiter __xws_open_waiter;

#define __XWS_RUN_IDLE     0L
#define __XWS_RUN_OPENING  1L
#define __XWS_RUN_OPEN     2L
#define __XWS_RUN_CLOSED   3L

struct __xws_open_waiter {
	struct __xws_open_waiter* pNext;
	xnetfuture* pFuture;
};

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
	char* sProtocol;
	char* pMsgBuf;
	size_t iMsgLen;
	size_t iMsgCap;
	uint8 iMsgOpcode;
	bool bMsgCompressed;
	bool bPerMessageDeflate;
	volatile long iOpen;
	volatile long iClosePosted;
	volatile long iCloseNotified;
	volatile long iCloseFlags;
	volatile long iTimerLock;
	volatile long iOpenWaitLock;
	volatile long iRunState;
	volatile long iSendLock;
	volatile long iDataSendActive;
	volatile long iDestroyRequested;
	volatile int32 iRefCount;
	__xws_open_waiter* pOpenWaitHead;
	xnet_result iOpenResult;
	__xws_timer_ctx* pHandshakeTimer;
	__xws_timer_ctx* pCloseTimer;
	xnet_result iCloseTransportResult;
	uint16 iLocalCloseCode;
	uint16 iRemoteCloseCode;
	uint16 iRemoteCloseReasonLen;
	char sRemoteCloseReason[XWS_CLOSE_REASON_CAP + 1u];
	int iLastSysErr;
};

struct xrt_ws_conn {
	struct xrt_ws_conn* pNext;
	volatile long iCleanupPosted;
	volatile int32 iRefCount;
	volatile long iOpen;
	volatile long iClosePosted;
	volatile long iCloseNotified;
	volatile long iCloseFlags;
	volatile long iTimerLock;
	volatile long iSendLock;
	volatile long iDataSendActive;
	__xws_timer_ctx* pHandshakeTimer;
	__xws_timer_ctx* pCloseTimer;
	xnet_result iCloseTransportResult;
	uint16 iLocalCloseCode;
	uint16 iRemoteCloseCode;
	uint16 iRemoteCloseReasonLen;
	char sRemoteCloseReason[XWS_CLOSE_REASON_CAP + 1u];
	xwsserver* pServer;
	xnetstream* pStream;
	char* sProtocol;
	char* pMsgBuf;
	size_t iMsgLen;
	size_t iMsgCap;
	uint8 iMsgOpcode;
	bool bMsgCompressed;
	bool bPerMessageDeflate;
	ptr pConnectionData;
	int iLastSysErr;
};

struct xrt_ws_writer {
	xnetstream* pStream;
	union {
		xwsclient* pClient;
		xwsconn* pConn;
	} uOwner;
	uint8 iOpcode;
	uint8 iUtf8Need;
	uint8 iUtf8FirstMin;
	uint8 iUtf8FirstMax;
	bool bClient;
	bool bStarted;
	bool bFinished;
	bool bUtf8First;
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
#define __XWS_APPEND_INVALID_DATA 4

// 内部函数：__xwsToLower
static char __xwsToLower(char ch)
{
	if ( ch >= 'A' && ch <= 'Z' ) { return (char)(ch + 32); }
	return ch;
}


// 内部函数：字符串相等忽略大小写相关处理
static bool __xwsStrEqNoCase(const char* sA, const char* sB)
{
	size_t i = 0;
	if ( !sA || !sB ) { return false; }
	while ( sA[i] && sB[i] ) {
		if ( __xwsToLower(sA[i]) != __xwsToLower(sB[i]) ) { return false; }
		i++;
	}
	return sA[i] == '\0' && sB[i] == '\0';
}


// 内部函数：判断是否包含 Token 忽略大小写
static bool __xwsContainsTokenNoCase(const char* sText, const char* sToken)
{
	return xrtHttpHeaderContainsToken(sText, sToken);
}


static bool __xwsIsTchar(unsigned char ch)
{
	if ( (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ) { return true; }
	switch ( ch ) {
		case '!': case '#': case '$': case '%': case '&': case '\'': case '*': case '+': case '-':
		case '.': case '^': case '_': case '`': case '|': case '~': return true;
		default: return false;
	}
}


static bool __xwsValidToken(const char* sToken)
{
	const unsigned char* p = (const unsigned char*)sToken;
	if ( !p || !p[0] ) { return false; }
	while ( *p ) {
		if ( !__xwsIsTchar(*p++) ) { return false; }
	}
	return true;
}


static bool __xwsValidFieldValue(const char* sValue)
{
	const unsigned char* p = (const unsigned char*)sValue;
	if ( !p ) { return false; }
	while ( *p ) {
		if ( *p == '\r' || *p == '\n' || *p == 0x7fu || (*p < 0x20u && *p != '\t') ) { return false; }
		p++;
	}
	return true;
}


static bool __xwsValidUtf8(const void* pData, size_t iLen)
{
	if ( iLen == 0u ) { return true; }
	return xrtIsUTF8((str)(void*)pData, iLen);
}


static bool __xwsValidCloseCode(uint16 iCode)
{
	if ( iCode >= 3000u && iCode <= 4999u ) { return true; }
	if ( iCode < 1000u || iCode > 1014u ) { return false; }
	return iCode != 1004u && iCode != 1005u && iCode != 1006u;
}


static uint32 __xwsHeaderCount(const xcodechttp1msg* pMsg, const char* sName, const char** psFirst)
{
	uint32 iCount = 0u;
	const xcodechttp1header* pHeaders;
	if ( psFirst ) { *psFirst = NULL; }
	if ( !pMsg || !sName ) { return 0u; }
	pHeaders = pMsg->pHeaders ? pMsg->pHeaders : pMsg->arrHeaders;
	for ( uint32 i = 0u; i < pMsg->iHeaderCount; ++i ) {
		if ( __xwsStrEqNoCase(pHeaders[i].sName, sName) ) {
			if ( iCount == 0u && psFirst ) { *psFirst = pHeaders[i].sValue; }
			iCount++;
		}
	}
	return iCount;
}


#define __XWS_PMD_VALUE "permessage-deflate; client_no_context_takeover; server_no_context_takeover"

static bool __xwsSpanEqNoCase(const char* pText, size_t iLen, const char* sToken)
{
	size_t iTokenLen = sToken ? strlen(sToken) : 0u;
	if ( !pText || !sToken || iLen != iTokenLen ) { return false; }
	for ( size_t i = 0u; i < iLen; ++i ) {
		if ( __xwsToLower(pText[i]) != __xwsToLower(sToken[i]) ) { return false; }
	}
	return true;
}


static bool __xwsManagedHandshakeHeaderN(const char* sName, size_t iNameLen)
{
	return __xwsSpanEqNoCase(sName, iNameLen, "Host") ||
		__xwsSpanEqNoCase(sName, iNameLen, "Upgrade") ||
		__xwsSpanEqNoCase(sName, iNameLen, "Connection") ||
		__xwsSpanEqNoCase(sName, iNameLen, "Sec-WebSocket-Key") ||
		__xwsSpanEqNoCase(sName, iNameLen, "Sec-WebSocket-Version") ||
		__xwsSpanEqNoCase(sName, iNameLen, "Sec-WebSocket-Protocol") ||
		__xwsSpanEqNoCase(sName, iNameLen, "Sec-WebSocket-Extensions") ||
		__xwsSpanEqNoCase(sName, iNameLen, "Content-Length") ||
		__xwsSpanEqNoCase(sName, iNameLen, "Transfer-Encoding") ||
		__xwsSpanEqNoCase(sName, iNameLen, "Origin");
}


static void __xwsTrimSpan(const char** ppText, size_t* pLen)
{
	const char* pText;
	size_t iLen;
	if ( !ppText || !pLen || !*ppText ) { return; }
	pText = *ppText;
	iLen = *pLen;
	while ( iLen > 0u && (pText[0] == ' ' || pText[0] == '\t') ) { pText++; iLen--; }
	while ( iLen > 0u && (pText[iLen - 1u] == ' ' || pText[iLen - 1u] == '\t') ) { iLen--; }
	*ppText = pText;
	*pLen = iLen;
}


/* 1: exact supported PMD, 0: another extension/unsupported PMD, -1: malformed. */
static int __xwsParseExtensionItem(const char* pItem, size_t iLen, bool bStrictResponse)
{
	size_t iPos = 0u;
	size_t iNameStart;
	size_t iNameLen;
	bool bClientNoContext = false;
	bool bServerNoContext = false;
	bool bPmd;
	__xwsTrimSpan(&pItem, &iLen);
	if ( iLen == 0u ) { return -1; }
	iNameStart = iPos;
	while ( iPos < iLen && __xwsIsTchar((unsigned char)pItem[iPos]) ) { iPos++; }
	iNameLen = iPos - iNameStart;
	if ( iNameLen == 0u ) { return -1; }
	bPmd = __xwsSpanEqNoCase(pItem + iNameStart, iNameLen, "permessage-deflate");
	if ( !bPmd ) { return bStrictResponse ? -1 : 0; }
	while ( iPos < iLen ) {
		size_t iParamStart;
		size_t iParamLen;
		while ( iPos < iLen && (pItem[iPos] == ' ' || pItem[iPos] == '\t') ) { iPos++; }
		if ( iPos >= iLen || pItem[iPos++] != ';' ) { return bStrictResponse ? -1 : 0; }
		while ( iPos < iLen && (pItem[iPos] == ' ' || pItem[iPos] == '\t') ) { iPos++; }
		iParamStart = iPos;
		while ( iPos < iLen && __xwsIsTchar((unsigned char)pItem[iPos]) ) { iPos++; }
		iParamLen = iPos - iParamStart;
		if ( iParamLen == 0u ) { return -1; }
		while ( iPos < iLen && (pItem[iPos] == ' ' || pItem[iPos] == '\t') ) { iPos++; }
		if ( iPos < iLen && pItem[iPos] == '=' ) { return bStrictResponse ? -1 : 0; }
		if ( __xwsSpanEqNoCase(pItem + iParamStart, iParamLen, "client_no_context_takeover") ) {
			if ( bClientNoContext ) { return -1; }
			bClientNoContext = true;
		} else if ( __xwsSpanEqNoCase(pItem + iParamStart, iParamLen, "server_no_context_takeover") ) {
			if ( bServerNoContext ) { return -1; }
			bServerNoContext = true;
		} else {
			return bStrictResponse ? -1 : 0;
		}
	}
	return bClientNoContext && bServerNoContext ? 1 : (bStrictResponse ? -1 : 0);
}


static bool __xwsNegotiatePmd(const xcodechttp1msg* pMsg, bool bStrictResponse, bool* pNegotiated)
{
	const xcodechttp1header* pHeaders;
	uint32 iExactCount = 0u;
	if ( pNegotiated ) { *pNegotiated = false; }
	if ( !pMsg || !pNegotiated ) { return false; }
	pHeaders = pMsg->pHeaders ? pMsg->pHeaders : pMsg->arrHeaders;
	for ( uint32 i = 0u; i < pMsg->iHeaderCount; ++i ) {
		const char* pValue;
		size_t iValueLen;
		size_t iStart = 0u;
		bool bQuoted = false;
		bool bEscape = false;
		if ( !__xwsSpanEqNoCase(pHeaders[i].sName, pHeaders[i].iNameLen, "Sec-WebSocket-Extensions") ) { continue; }
		pValue = pHeaders[i].sValue;
		iValueLen = pHeaders[i].iValueLen;
		for ( size_t iPos = 0u; iPos <= iValueLen; ++iPos ) {
			char ch = iPos < iValueLen ? pValue[iPos] : ',';
			if ( bQuoted ) {
				if ( bEscape ) { bEscape = false; continue; }
				if ( ch == '\\' ) { bEscape = true; continue; }
				if ( ch == '"' ) { bQuoted = false; }
				continue;
			}
			if ( ch == '"' ) { bQuoted = true; continue; }
			if ( ch == ',' ) {
				int iItem = __xwsParseExtensionItem(pValue + iStart, iPos - iStart, bStrictResponse);
				if ( iItem < 0 ) { return false; }
				if ( iItem > 0 ) { iExactCount++; }
				iStart = iPos + 1u;
			}
		}
		if ( bQuoted || bEscape ) { return false; }
	}
	if ( bStrictResponse && iExactCount != 1u ) { return false; }
	*pNegotiated = iExactCount > 0u;
	return true;
}


static bool __xwsHeaderHasTokenNoCase(const xcodechttp1msg* pMsg, const char* sName, const char* sToken)
{
	const xcodechttp1header* pHeaders;
	if ( !pMsg || !sName || !sToken ) { return false; }
	pHeaders = pMsg->pHeaders ? pMsg->pHeaders : pMsg->arrHeaders;
	for ( uint32 i = 0u; i < pMsg->iHeaderCount; ++i ) {
		if ( __xwsStrEqNoCase(pHeaders[i].sName, sName) && __xwsContainsTokenNoCase(pHeaders[i].sValue, sToken) ) {
			return true;
		}
	}
	return false;
}


static bool __xwsProtocolListContains(const char* sList, const char* sProtocol)
{
	const char* p;
	size_t iProtocolLen;
	if ( !sList || !sProtocol || !__xwsValidToken(sProtocol) ) { return false; }
	p = sList;
	iProtocolLen = strlen(sProtocol);
	for ( ;; ) {
		const char* sBegin;
		const char* sEnd;
		while ( *p == ' ' || *p == '\t' ) { p++; }
		sBegin = p;
		while ( __xwsIsTchar((unsigned char)*p) ) { p++; }
		sEnd = p;
		while ( *p == ' ' || *p == '\t' ) { p++; }
		if ( sEnd == sBegin ) { return false; }
		if ( (size_t)(sEnd - sBegin) == iProtocolLen && memcmp(sBegin, sProtocol, iProtocolLen) == 0 ) { return true; }
		if ( !*p ) { return false; }
		if ( *p != ',' ) { return false; }
		p++;
		if ( !*p ) { return false; }
	}
}


static bool __xwsValidProtocolList(const char* sList)
{
	const char* p;
	if ( !sList || !sList[0] ) { return true; }
	p = sList;
	for ( ;; ) {
		bool bToken = false;
		while ( *p == ' ' || *p == '\t' ) { p++; }
		while ( __xwsIsTchar((unsigned char)*p) ) {
			bToken = true;
			p++;
		}
		while ( *p == ' ' || *p == '\t' ) { p++; }
		if ( !bToken ) { return false; }
		if ( !*p ) { return true; }
		if ( *p != ',' ) { return false; }
		p++;
		if ( !*p ) { return false; }
	}
}


static bool __xwsProtocolHeadersValid(const xcodechttp1msg* pMsg, bool* pHasProtocols)
{
	const xcodechttp1header* pHeaders;
	bool bFound = false;
	if ( pHasProtocols ) { *pHasProtocols = false; }
	if ( !pMsg ) { return false; }
	pHeaders = pMsg->pHeaders ? pMsg->pHeaders : pMsg->arrHeaders;
	for ( uint32 i = 0u; i < pMsg->iHeaderCount; ++i ) {
		if ( __xwsStrEqNoCase(pHeaders[i].sName, "Sec-WebSocket-Protocol") ) {
			bFound = true;
			if ( !__xwsValidProtocolList(pHeaders[i].sValue) ) { return false; }
		}
	}
	if ( pHasProtocols ) { *pHasProtocols = bFound; }
	return true;
}


static bool __xwsProtocolHeadersContain(const xcodechttp1msg* pMsg, const char* sProtocol)
{
	const xcodechttp1header* pHeaders;
	if ( !pMsg || !sProtocol || !__xwsValidToken(sProtocol) ) { return false; }
	pHeaders = pMsg->pHeaders ? pMsg->pHeaders : pMsg->arrHeaders;
	for ( uint32 i = 0u; i < pMsg->iHeaderCount; ++i ) {
		if ( __xwsStrEqNoCase(pHeaders[i].sName, "Sec-WebSocket-Protocol") &&
			__xwsProtocolListContains(pHeaders[i].sValue, sProtocol) ) { return true; }
	}
	return false;
}


static uint32 __xwsResolveHandshakeMaxBytes(uint32 iConfigured)
{
	return iConfigured > 0u ? iConfigured : XCODEC_HTTP1_DEFAULT_HEADER_BYTES;
}


static uint64 __xwsResolveFrameMaxBytes(uint32 iConfigured, uint32 iMessageLimit)
{
	if ( iConfigured > 0u ) { return iConfigured; }
	return iMessageLimit > 0u ? iMessageLimit : UINT64_MAX;
}


static uint32 __xwsResolveTransportRecvLimit(uint32 iFrameMaxBytes, uint32 iMessageLimit,
	uint32 iHandshakeMaxBytes)
{
	uint64 iFrameLimit = __xwsResolveFrameMaxBytes(iFrameMaxBytes, iMessageLimit);
	uint64 iNeed = __xwsResolveHandshakeMaxBytes(iHandshakeMaxBytes);
	if ( iFrameLimit == UINT64_MAX ) { return UINT32_MAX; }
	if ( iNeed < iFrameLimit ) { iNeed = iFrameLimit; }
	if ( iNeed > UINT32_MAX - __XWS_TRANSPORT_RECV_SLACK_BYTES ) { return UINT32_MAX; }
	return (uint32)(iNeed + __XWS_TRANSPORT_RECV_SLACK_BYTES);
}


static void __xwsResolveHandshakeLimits(uint32 iHandshakeMaxBytes, xcodechttp1limits* pLimits)
{
	size_t iLimit;
	if ( !pLimits ) { return; }
	xrtCodecHttp1LimitsInit(pLimits);
	iLimit = __xwsResolveHandshakeMaxBytes(iHandshakeMaxBytes);
	pLimits->iMaxHeaderBytes = iLimit;
	if ( pLimits->iMaxStartLineBytes > iLimit ) { pLimits->iMaxStartLineBytes = iLimit; }
	if ( pLimits->iMaxHeaderLineBytes > iLimit ) { pLimits->iMaxHeaderLineBytes = iLimit; }
}


static uint32 __xwsHandshakeErrorStatus(const xcodechttp1errorinfo* pError)
{
	if ( !pError ) { return 400u; }
	switch ( pError->eCode ) {
		case XCODEC_HTTP1_ERROR_HEADER_TOO_LARGE:
		case XCODEC_HTTP1_ERROR_START_LINE_TOO_LARGE:
		case XCODEC_HTTP1_ERROR_HEADER_LINE_TOO_LARGE:
		case XCODEC_HTTP1_ERROR_TOO_MANY_HEADERS:
			return 431u;
		default:
			return 400u;
	}
}


// 内部函数：复制 Token
static const char* __xwsClientConfigURLValue(const xwsclientconfig* pCfg)
{
	return pCfg ? (pCfg->pURLStorage ? pCfg->pURLStorage : pCfg->sURL) : NULL;
}


static const char* __xwsClientConfigOriginValue(const xwsclientconfig* pCfg)
{
	return pCfg ? (pCfg->pOriginStorage ? pCfg->pOriginStorage : pCfg->sOrigin) : NULL;
}


static const char* __xwsClientConfigProtocolValue(const xwsclientconfig* pCfg)
{
	return pCfg ? (pCfg->pProtocolStorage ? pCfg->pProtocolStorage : pCfg->sProtocol) : NULL;
}


static const char* __xwsServerConfigProtocolValue(const xwsserverconfig* pCfg)
{
	return pCfg ? (pCfg->pProtocolStorage ? pCfg->pProtocolStorage : pCfg->sProtocol) : NULL;
}


static const char* __xwsServerConfigPathValue(const xwsserverconfig* pCfg)
{
	return (pCfg && pCfg->pPathStorage) ? pCfg->pPathStorage : "";
}


static const char* __xwsServerConfigOriginValue(const xwsserverconfig* pCfg)
{
	return (pCfg && pCfg->pOriginStorage) ? pCfg->pOriginStorage : "";
}


static char* __xwsDupTextN(const char* sText, size_t iLen)
{
	char* pCopy;
	if ( (!sText && iLen != 0u) || iLen == SIZE_MAX ) { return NULL; }
	pCopy = (char*)XNET_ALLOC(iLen + 1u);
	if ( !pCopy ) { return NULL; }
	if ( iLen > 0u ) { memcpy(pCopy, sText, iLen); }
	pCopy[iLen] = '\0';
	return pCopy;
}


static char* __xwsDupText(const char* sText)
{
	return sText ? __xwsDupTextN(sText, strlen(__xrt_cstr(sText))) : NULL;
}


static bool __xwsSetConfigText(char* sInline, size_t iInlineCap, char** ppStorage, const char* sText)
{
	char* pCopy;
	size_t iLen;
	if ( !sInline || iInlineCap == 0u || !ppStorage || !sText ) { return false; }
	iLen = strlen(__xrt_cstr(sText));
	pCopy = __xwsDupTextN(sText, iLen);
	if ( !pCopy ) { return false; }
	if ( *ppStorage ) { XNET_FREE(*ppStorage); }
	*ppStorage = NULL;
	if ( iLen < iInlineCap ) {
		memcpy(sInline, pCopy, iLen + 1u);
		XNET_FREE(pCopy);
	} else {
		sInline[0] = '\0';
		*ppStorage = pCopy;
	}
	return true;
}


static void __xwsURLUnit(__xws_url* pURL)
{
	if ( !pURL ) { return; }
	if ( pURL->sHost ) { XNET_FREE(pURL->sHost); }
	if ( pURL->sPath ) { XNET_FREE(pURL->sPath); }
	memset(pURL, 0, sizeof(*pURL));
}


static bool __xwsParseURL(const char* sURL, __xws_url* pOut)
{
	xrturlview tView;
	__xws_url tNew;
	size_t iTargetLen;
	bool bPrefixSlash;
	if ( !sURL || !pOut || !xrtUrlParseView(sURL, &tView) ||
		(tView.iFlags & XRT_URL_F_ABSOLUTE) == 0u ||
		(tView.iFlags & (XRT_URL_F_HAS_USERINFO | XRT_URL_F_HAS_FRAGMENT)) != 0u ||
		!xrtUrlViewMatchesScheme2(&tView, "ws", "wss") || xrtStrViewIsEmpty(tView.tHost) ) { return false; }
	memset(&tNew, 0, sizeof(tNew));
	tNew.bSecure = xrtUrlViewIsScheme(&tView, "wss");
	tNew.iPort = tView.iPort;
	tNew.sHost = __xwsDupTextN(tView.tHost.sPtr, tView.tHost.iLen);
	if ( !tNew.sHost ) { return false; }
	bPrefixSlash = xrtStrViewIsEmpty(tView.tTarget) || tView.tTarget.sPtr[0] == '?';
	iTargetLen = xrtStrViewIsEmpty(tView.tTarget) ? 1u : tView.tTarget.iLen + (bPrefixSlash ? 1u : 0u);
	if ( iTargetLen < tView.tTarget.iLen || iTargetLen == SIZE_MAX ) {
		XNET_FREE(tNew.sHost);
		return false;
	}
	tNew.sPath = (char*)XNET_ALLOC(iTargetLen + 1u);
	if ( !tNew.sPath ) {
		XNET_FREE(tNew.sHost);
		return false;
	}
	if ( xrtStrViewIsEmpty(tView.tTarget) ) {
		tNew.sPath[0] = '/';
	} else {
		size_t iOffset = 0u;
		if ( bPrefixSlash ) { tNew.sPath[iOffset++] = '/'; }
		memcpy(tNew.sPath + iOffset, tView.tTarget.sPtr, tView.tTarget.iLen);
	}
	tNew.sPath[iTargetLen] = '\0';
	__xwsURLUnit(pOut);
	*pOut = tNew;
	return true;
}


static bool __xwsReplaceText(char** ppText, const char* sText)
{
	char* pCopy;
	if ( !ppText ) { return false; }
	pCopy = (sText && sText[0]) ? __xwsDupText(sText) : NULL;
	if ( sText && sText[0] && !pCopy ) { return false; }
	if ( *ppText ) { XNET_FREE(*ppText); }
	*ppText = pCopy;
	return true;
}


// 内部函数：__xwsAtomicCompareExchange
static long __xwsAtomicCompareExchange(volatile long* pValue, long iExchange, long iComparand)
{
	return __xnetAtomicCompareExchange32(pValue, iExchange, iComparand);
}


// 内部函数：__xwsAtomicLoad
static long __xwsAtomicLoad(volatile long* pValue)
{
	return __xnetAtomicLoad32(pValue);
}


// 内部函数：__xwsAtomicLoadConst
static long __xwsAtomicLoadConst(const volatile long* pValue)
{
	return __xnetAtomicLoad32(pValue);
}


static void __xwsAtomicOr(volatile long* pValue, long iFlags)
{
	long iOld;
	long iNew;
	if ( !pValue ) { return; }
	do {
		iOld = __xwsAtomicLoad(pValue);
		iNew = iOld | iFlags;
	} while ( __xwsAtomicCompareExchange(pValue, iNew, iOld) != iOld );
}


static void __xwsRecordLocalClose(volatile long* pFlags, uint16* pCode, uint16 iCode)
{
	if ( pCode ) { *pCode = iCode; }
	__xwsAtomicOr(pFlags, (long)XWS_CLOSE_F_SENT);
}


static void __xwsRecordRemoteClose(volatile long* pFlags, uint16* pCode, uint16* pReasonLen,
	char* sReason, const char* pPayload, size_t iPayloadLen, bool bRemoteInitiated)
{
	size_t iReasonLen = iPayloadLen >= 2u ? iPayloadLen - 2u : 0u;
	if ( pCode ) { *pCode = iPayloadLen >= 2u ? (uint16)(((uint8)pPayload[0] << 8u) | (uint8)pPayload[1]) : 0u; }
	if ( iReasonLen > XWS_CLOSE_REASON_CAP ) { iReasonLen = XWS_CLOSE_REASON_CAP; }
	if ( sReason ) {
		if ( iReasonLen > 0u ) { memcpy(sReason, pPayload + 2u, iReasonLen); }
		sReason[iReasonLen] = '\0';
	}
	if ( pReasonLen ) { *pReasonLen = (uint16)iReasonLen; }
	__xwsAtomicOr(pFlags, (long)(XWS_CLOSE_F_RECEIVED |
		(bRemoteInitiated ? XWS_CLOSE_F_REMOTE_INITIATED : 0u)));
}


static void __xwsFillCloseInfo(xwscloseinfo* pInfo, const volatile long* pFlags,
	xnet_result iTransportResult, uint16 iLocalCode, uint16 iRemoteCode,
	const char* sRemoteReason, uint16 iRemoteReasonLen)
{
	uint32 iFlags;
	if ( !pInfo ) { return; }
	memset(pInfo, 0, sizeof(*pInfo));
	iFlags = pFlags ? (uint32)__xwsAtomicLoadConst(pFlags) : 0u;
	if ( (iFlags & (XWS_CLOSE_F_SENT | XWS_CLOSE_F_RECEIVED)) ==
		(XWS_CLOSE_F_SENT | XWS_CLOSE_F_RECEIVED) ) { iFlags |= XWS_CLOSE_F_CLEAN; }
	pInfo->iSize = XWS_CLOSE_INFO_V1_SIZE;
	pInfo->iVersion = XWS_CLOSE_INFO_VERSION;
	pInfo->iFlags = iFlags;
	pInfo->iTransportResult = iTransportResult;
	pInfo->iLocalCode = iLocalCode;
	pInfo->iRemoteCode = iRemoteCode;
	pInfo->sRemoteReason = sRemoteReason ? sRemoteReason : "";
	pInfo->iRemoteReasonLen = (size_t)iRemoteReasonLen;
}


// 内部函数：休眠 0
static void __xwsSleep0(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(0);
	#else
		sched_yield();
	#endif
}


// 内部函数：锁定
static void __xwsLock(volatile long* pLock)
{
	if ( !pLock ) { return; }
	while ( __xwsAtomicCompareExchange(pLock, 1, 0) != 0 ) {
		__xwsSleep0();
	}
}


// 内部函数：解锁
static void __xwsUnlock(volatile long* pLock)
{
	if ( !pLock ) { return; }
	(void)__xnetAtomicExchange32(pLock, 0);
}


// 内部函数：__xwsAppendBytes
static bool __xwsAppendBytes(char** ppBuf, size_t* pLen, size_t* pCap, const void* pData, size_t iDataLen)
{
	size_t iNeedCap;
	size_t iTargetCap;
	char* pNewBuf;
	if ( !ppBuf || !pLen || !pCap || (!pData && iDataLen != 0) ) { return false; }
	if ( iDataLen == 0 ) { return true; }
	// 计算追加后的目标容量
	iTargetCap = *pLen + iDataLen + 1u;
	if ( iTargetCap < *pLen || iTargetCap < iDataLen ) { return false; }
	// 容量足够时直接追加
	if ( iTargetCap <= *pCap ) {
		memcpy(*ppBuf + *pLen, pData, iDataLen);
		*pLen += iDataLen;
		(*ppBuf)[*pLen] = '\0';
		return true;
	}
	// 容量不足时倍增扩容
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
static bool __xwsAppendText(char** ppBuf, size_t* pLen, size_t* pCap, const char* sText)
{
	return __xwsAppendBytes(ppBuf, pLen, pCap, sText, sText ? strlen(__xrt_cstr(sText)) : 0);
}


// 内部函数：HTTP 状态文本相关处理
static const char* __xwsHttpStatusText(uint32 iStatusCode)
{
	switch ( iStatusCode ) {
		case 101: return "Switching Protocols";
		case 400: return "Bad Request";
		case 401: return "Unauthorized";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 426: return "Upgrade Required";
		case 431: return "Request Header Fields Too Large";
		case 500: return "Internal Server Error";
		default: return "OK";
	}
}


// 内部函数：__xwsBase64Encode
static bool __xwsBase64Encode(const uint8* pIn, size_t iLen, char* sOut, size_t iOutCap)
{
	static const char aTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	size_t iNeed = 4u * ((iLen + 2u) / 3u);
	size_t i = 0;
	size_t j = 0;
	if ( !pIn || !sOut || iOutCap <= iNeed ) { return false; }
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
static int __xwsBase64Value(unsigned char ch)
{
	if ( ch >= 'A' && ch <= 'Z' ) { return (int)(ch - 'A'); }
	if ( ch >= 'a' && ch <= 'z' ) { return 26 + (int)(ch - 'a'); }
	if ( ch >= '0' && ch <= '9' ) { return 52 + (int)(ch - '0'); }
	if ( ch == '+' ) { return 62; }
	if ( ch == '/' ) { return 63; }
	return -1;
}


struct __xws_timer_ctx {
	volatile int32 iRefCount;
	volatile long iState;
	xnetengine* pEngine;
	xnetstream* pStream;
	uint64 iTimerId;
};


static void __xwsTimerRelease(__xws_timer_ctx* pTimer)
{
	if ( !pTimer || xrtAtomicRefRelease(&pTimer->iRefCount) != 0 ) { return; }
	if ( pTimer->pStream ) { __xnetStreamReleaseAsyncHold(pTimer->pStream); }
	XNET_FREE(pTimer);
}


static void __xwsTimerDiscard(ptr pArg)
{
	__xwsTimerRelease((__xws_timer_ctx*)pArg);
}


static void __xwsTimeoutTask(xnetworker* pWorker, ptr pArg)
{
	__xws_timer_ctx* pTimer = (__xws_timer_ctx*)pArg;
	xnetstream* pStream = pTimer ? pTimer->pStream : NULL;
	(void)pWorker;
	if ( pTimer && __xwsAtomicCompareExchange(&pTimer->iState, 2, 0) == 0 && pStream &&
		(pStream->iState & __XNET_STREAM_STATE_CLOSE_EMITTED) == 0u ) {
		pStream->bClosing = true;
		pStream->iFlags = XNET_CLOSE_F_ABORT;
		__xnetStreamClearRx(pStream);
		__xnetStreamClearSendQueue(pStream);
		__xnetStreamFinishClose(pStream, XRT_NET_TIMEOUT);
	}
	__xwsTimerRelease(pTimer);
}


static void __xwsCancelTimerSlot(volatile long* pLock, __xws_timer_ctx** ppSlot)
{
	__xws_timer_ctx* pTimer;
	if ( !pLock || !ppSlot ) { return; }
	__xwsLock(pLock);
	pTimer = *ppSlot;
	*ppSlot = NULL;
	__xwsUnlock(pLock);
	if ( !pTimer ) { return; }
	if ( __xwsAtomicCompareExchange(&pTimer->iState, 1, 0) == 0 &&
		pTimer->pEngine && pTimer->iTimerId != 0u ) {
		(void)xrtNetEngineCancelTimer(pTimer->pEngine, pTimer->iTimerId);
	}
	__xwsTimerRelease(pTimer);
}


static bool __xwsArmTimerSlot(xnetstream* pStream, uint32 iDelayMs,
	volatile long* pLock, __xws_timer_ctx** ppSlot)
{
	__xws_timer_ctx* pTimer;
	uint32 iAffinity;
	uint64 iTimerId = 0u;
	xnet_result iResult;
	bool bReleaseSlot;
	if ( iDelayMs == 0u ) { return true; }
	if ( !pStream || !pStream->pEngine || !pStream->pWorker || !pLock || !ppSlot ) { return false; }
	__xwsCancelTimerSlot(pLock, ppSlot);
	pTimer = (__xws_timer_ctx*)XNET_ALLOC(sizeof(*pTimer));
	if ( !pTimer ) { return false; }
	memset(pTimer, 0, sizeof(*pTimer));
	pTimer->iRefCount = 2;
	pTimer->pEngine = pStream->pEngine;
	pTimer->pStream = pStream;
	__xnetStreamAddAsyncHold(pStream);
	__xwsLock(pLock);
	*ppSlot = pTimer;
	__xwsUnlock(pLock);
	iAffinity = pStream->pWorker->iId;
	iResult = __xnetEngineScheduleTimerOwned(pStream->pEngine, iAffinity, iDelayMs,
		__xwsTimeoutTask, __xwsTimerDiscard, pTimer, &iTimerId);
	if ( iResult != XRT_NET_OK ) {
		bReleaseSlot = false;
		__xwsLock(pLock);
		if ( *ppSlot == pTimer ) { *ppSlot = NULL; bReleaseSlot = true; }
		__xwsUnlock(pLock);
		(void)__xwsAtomicCompareExchange(&pTimer->iState, 1, 0);
		if ( bReleaseSlot ) { __xwsTimerRelease(pTimer); }
		__xwsTimerRelease(pTimer);
		return false;
	}
	pTimer->iTimerId = iTimerId;
	if ( __xwsAtomicLoad(&pTimer->iState) != 0 ) {
		(void)xrtNetEngineCancelTimer(pTimer->pEngine, iTimerId);
	}
	return true;
}


static void __xwsClientCancelTimers(xwsclient* pClient)
{
	if ( !pClient ) { return; }
	__xwsCancelTimerSlot(&pClient->iTimerLock, &pClient->pHandshakeTimer);
	__xwsCancelTimerSlot(&pClient->iTimerLock, &pClient->pCloseTimer);
}


static void __xwsConnCancelTimers(xwsconn* pConn)
{
	if ( !pConn ) { return; }
	__xwsCancelTimerSlot(&pConn->iTimerLock, &pConn->pHandshakeTimer);
	__xwsCancelTimerSlot(&pConn->iTimerLock, &pConn->pCloseTimer);
}


static bool __xwsValidClientKey(const char* sKey)
{
	if ( !sKey || strlen(sKey) != 24u || sKey[22] != '=' || sKey[23] != '=' ) { return false; }
	for ( size_t i = 0u; i < 22u; ++i ) {
		if ( __xwsBase64Value((unsigned char)sKey[i]) < 0 ) { return false; }
	}
	return (__xwsBase64Value((unsigned char)sKey[21]) & 0x0f) == 0;
}


// 内部函数：__xwsGenerateKey
static bool __xwsGenerateKey(char* sKey, size_t iCap)
{
	uint8 aRandom[16];
	if ( !sKey || iCap < 25u ) { return false; }
	xrtRandomBytes(aRandom, sizeof(aRandom));
	return __xwsBase64Encode(aRandom, sizeof(aRandom), sKey, iCap);
}


// 内部函数：__xwsComputeAccept
static bool __xwsComputeAccept(const char* sKey, char* sAccept, size_t iCap)
{
	char sCombined[128];
	uint8 aDigest[20];
	int iWritten;
	if ( !sKey || !sAccept || iCap < 29u ) { return false; }
	iWritten = snprintf(sCombined, sizeof(sCombined), "%s%s", sKey, __XWS_GUID);
	if ( iWritten <= 0 || (size_t)iWritten >= sizeof(sCombined) ) { return false; }
	xrtSHA1(sCombined, (size_t)iWritten, aDigest);
	return __xwsBase64Encode(aDigest, sizeof(aDigest), sAccept, iCap);
}


static bool __xwsAppendHeaderCollection(char** ppBuf, size_t* pLen, size_t* pCap,
	const xhttpheaders* pHeaders)
{
	size_t iCount = xrtHttpHeadersCount(pHeaders);
	for ( size_t i = 0u; i < iCount; ++i ) {
		xrtheaderpair tHeader;
		if ( !xrtHttpHeadersAt(pHeaders, i, &tHeader) ||
			__xwsManagedHandshakeHeaderN(tHeader.tName.sPtr, tHeader.tName.iLen) ||
			!__xwsAppendBytes(ppBuf, pLen, pCap, tHeader.tName.sPtr, tHeader.tName.iLen) ||
			!__xwsAppendText(ppBuf, pLen, pCap, ": ") ||
			!__xwsAppendBytes(ppBuf, pLen, pCap, tHeader.tValue.sPtr, tHeader.tValue.iLen) ||
			!__xwsAppendText(ppBuf, pLen, pCap, "\r\n") ) { return false; }
	}
	return true;
}


// 内部函数：__xwsBuildClientHandshake
static bool __xwsBuildClientHandshake(xwsclient* pClient, char** ppOut, size_t* pOutLen)
{
	const char* sProtocol;
	const char* sOrigin;
	char* sHost = NULL;
	char* pBuf = NULL;
	size_t iLen = 0;
	size_t iCap = 0;
	size_t iHostCap;
	if ( !pClient || !ppOut || !pOutLen || !pClient->tURL.sHost || !pClient->tURL.sPath ) { return false; }
	sProtocol = __xwsClientConfigProtocolValue(&pClient->tConfig);
	sOrigin = __xwsClientConfigOriginValue(&pClient->tConfig);
	if ( !__xwsValidProtocolList(sProtocol) || !__xwsValidFieldValue(sOrigin) ) { return false; }
	// 构建 Host 头部值
	if ( strlen(pClient->tURL.sHost) > SIZE_MAX - 16u ) { return false; }
	iHostCap = strlen(pClient->tURL.sHost) + 16u;
	sHost = (char*)XNET_ALLOC(iHostCap);
	if ( !sHost || !xrtUrlMakeHostHeaderFixed(pClient->tURL.bSecure ? "wss" : "ws", pClient->tURL.sHost, pClient->tURL.iPort, sHost, iHostCap) ) {
		if ( sHost ) { XNET_FREE(sHost); }
		return false;
	}
	// 生成握手密钥和期望的 Accept 值
	if ( !__xwsGenerateKey(pClient->sKey, sizeof(pClient->sKey)) ||
		!__xwsComputeAccept(pClient->sKey, pClient->sExpectedAccept, sizeof(pClient->sExpectedAccept)) ) {
		XNET_FREE(sHost);
		return false;
	}
	// 构建基本请求行和必需头部
	if ( !__xwsAppendText(&pBuf, &iLen, &iCap, "GET ") ||
		!__xwsAppendText(&pBuf, &iLen, &iCap, pClient->tURL.sPath) ||
		!__xwsAppendText(&pBuf, &iLen, &iCap, " HTTP/1.1\r\nHost: ") ||
		!__xwsAppendText(&pBuf, &iLen, &iCap, sHost) ||
		!__xwsAppendText(&pBuf, &iLen, &iCap, "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: ") ||
		!__xwsAppendText(&pBuf, &iLen, &iCap, pClient->sKey) ||
		!__xwsAppendText(&pBuf, &iLen, &iCap, "\r\nSec-WebSocket-Version: 13\r\n") ) {
		XNET_FREE(sHost);
		XNET_FREE(pBuf);
		return false;
	}
	XNET_FREE(sHost);
	// 可选：追加 Sec-WebSocket-Protocol 头部
	if ( sProtocol[0] ) {
		if ( !__xwsAppendText(&pBuf, &iLen, &iCap, "Sec-WebSocket-Protocol: ") ||
			!__xwsAppendText(&pBuf, &iLen, &iCap, sProtocol) ||
			!__xwsAppendText(&pBuf, &iLen, &iCap, "\r\n") ) {
			XNET_FREE(pBuf);
			return false;
		}
	}
	// 可选：追加 Origin 头部
	if ( sOrigin[0] ) {
		if ( !__xwsAppendText(&pBuf, &iLen, &iCap, "Origin: ") ||
			!__xwsAppendText(&pBuf, &iLen, &iCap, sOrigin) ||
			!__xwsAppendText(&pBuf, &iLen, &iCap, "\r\n") ) {
			XNET_FREE(pBuf);
			return false;
		}
	}
	if ( (pClient->tConfig.iWebSocketFlags & XWS_F_PERMESSAGE_DEFLATE) != 0u ) {
		if ( !__xwsAppendText(&pBuf, &iLen, &iCap, "Sec-WebSocket-Extensions: " __XWS_PMD_VALUE "\r\n") ) {
			XNET_FREE(pBuf);
			return false;
		}
	}
	if ( pClient->tConfig.iSize >= XWS_CLIENT_CONFIG_V2_SIZE &&
		pClient->tConfig.pRequestHeadersStorage &&
		!__xwsAppendHeaderCollection(&pBuf, &iLen, &iCap, pClient->tConfig.pRequestHeadersStorage) ) {
		XNET_FREE(pBuf);
		return false;
	}
	// 追加空行结束头部
	if ( !__xwsAppendText(&pBuf, &iLen, &iCap, "\r\n") ) {
		XNET_FREE(pBuf);
		return false;
	}
	*ppOut = pBuf;
	*pOutLen = iLen;
	return true;
}


// 内部函数：__xwsBuildHttpResponseBytes
static bool __xwsBuildHttpResponseBytesEx(uint32 iStatusCode, const char* sReason,
	const void* pBody, size_t iBodyLen, const char* sAccept,
	const char* sProtocol, const char* sExtensions, const xhttpheaders* pHeaders,
	char** ppOut, size_t* pOutLen)
{
	char sLine[256];
	char sLength[64];
	char* pBuf = NULL;
	size_t iLen = 0;
	size_t iCap = 0;
	const char* sStatusReason = (sReason && sReason[0]) ? sReason : __xwsHttpStatusText(iStatusCode);
	if ( !ppOut || !pOutLen || (!pBody && iBodyLen != 0u) || !__xwsValidFieldValue(sStatusReason) ) { return false; }
	// 构建状态行
	(void)snprintf(sLine, sizeof(sLine), "HTTP/1.1 %u %s\r\n", (unsigned)iStatusCode, sStatusReason);
	if ( !__xwsAppendText(&pBuf, &iLen, &iCap, sLine) ) {
		XNET_FREE(pBuf);
		return false;
	}
	// 101 状态码表示协议切换，发送 WebSocket 升级响应
	if ( iStatusCode == 101u ) {
		if ( !__xwsAppendText(&pBuf, &iLen, &iCap, "Upgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ") ||
			!__xwsAppendText(&pBuf, &iLen, &iCap, sAccept ? sAccept : "") ||
			!__xwsAppendText(&pBuf, &iLen, &iCap, "\r\n") ) {
			XNET_FREE(pBuf);
			return false;
		}
		// 可选：追加子协议头部
		if ( sProtocol && sProtocol[0] ) {
			if ( !__xwsAppendText(&pBuf, &iLen, &iCap, "Sec-WebSocket-Protocol: ") ||
				!__xwsAppendText(&pBuf, &iLen, &iCap, sProtocol) ||
				!__xwsAppendText(&pBuf, &iLen, &iCap, "\r\n") ) {
				XNET_FREE(pBuf);
				return false;
			}
		}
		if ( sExtensions && sExtensions[0] ) {
			if ( !__xwsAppendText(&pBuf, &iLen, &iCap, "Sec-WebSocket-Extensions: ") ||
				!__xwsAppendText(&pBuf, &iLen, &iCap, sExtensions) ||
				!__xwsAppendText(&pBuf, &iLen, &iCap, "\r\n") ) {
				XNET_FREE(pBuf);
				return false;
			}
		}
		if ( pHeaders && !__xwsAppendHeaderCollection(&pBuf, &iLen, &iCap, pHeaders) ) {
			XNET_FREE(pBuf);
			return false;
		}
		if ( !__xwsAppendText(&pBuf, &iLen, &iCap, "\r\n") ) {
			XNET_FREE(pBuf);
			return false;
		}
	} else {
		// 非 101 状态码：返回普通 HTTP 错误响应
		(void)snprintf(sLength, sizeof(sLength), "%llu", (unsigned long long)iBodyLen);
		if ( !__xwsAppendText(&pBuf, &iLen, &iCap, "Connection: close\r\n") ||
			(iStatusCode == 426u && !__xwsAppendText(&pBuf, &iLen, &iCap, "Sec-WebSocket-Version: 13\r\n")) ||
			(pHeaders && !__xwsAppendHeaderCollection(&pBuf, &iLen, &iCap, pHeaders)) ||
			((!pHeaders || !xrtHttpHeadersContains(pHeaders, "Content-Type")) &&
				!__xwsAppendText(&pBuf, &iLen, &iCap, "Content-Type: text/plain\r\n")) ||
			!__xwsAppendText(&pBuf, &iLen, &iCap, "Content-Length: ") ||
			!__xwsAppendText(&pBuf, &iLen, &iCap, sLength) ||
			!__xwsAppendText(&pBuf, &iLen, &iCap, "\r\n\r\n") ||
			!__xwsAppendBytes(&pBuf, &iLen, &iCap, pBody, iBodyLen) ) {
			XNET_FREE(pBuf);
			return false;
		}
	}
	*ppOut = pBuf;
	*pOutLen = iLen;
	return true;
}


// 内部函数：重置消息
static void __xwsMessageReset(char** ppBuf, size_t* pLen, size_t* pCap, uint8* pOpcode)
{
	if ( ppBuf && *ppBuf ) {
		XNET_FREE(*ppBuf);
		*ppBuf = NULL;
	}
	if ( pLen ) { *pLen = 0u; }
	if ( pCap ) { *pCap = 0u; }
	if ( pOpcode ) { *pOpcode = 0u; }
}


// 内部函数：追加消息
static int __xwsMessageAppend(char** ppBuf, size_t* pLen, size_t* pCap, uint8* pOpcode, uint8 iOpcode, const void* pPayload, size_t iPayloadLen, size_t iLimit)
{
	if ( !ppBuf || !pLen || !pCap || !pOpcode || (!pPayload && iPayloadLen != 0u) ) { return __XWS_APPEND_INTERNAL; }
	if ( iPayloadLen > SIZE_MAX - *pLen ) { return __XWS_APPEND_TOO_BIG; }
	if ( iLimit > 0u && (*pLen > iLimit || iPayloadLen > iLimit - *pLen) ) { return __XWS_APPEND_TOO_BIG; }
	if ( iOpcode != 0u ) { *pOpcode = iOpcode; }
	if ( !__xwsAppendBytes(ppBuf, pLen, pCap, pPayload, iPayloadLen) ) { return __XWS_APPEND_INTERNAL; }
	return __XWS_APPEND_OK;
}


// 内部函数：__xwsFrameSize
static bool __xwsFrameSize(size_t iPayloadLen, bool bMask, size_t* pSize)
{
	size_t iExtra = 2u + (bMask ? 4u : 0u);
	if ( !pSize || (uint64)iPayloadLen > (UINT64_MAX >> 1u) ) { return false; }
	if ( iPayloadLen >= 126u && iPayloadLen <= 0xFFFFu ) { iExtra += 2u; }
	else if ( iPayloadLen > 0xFFFFu ) { iExtra += 8u; }
	if ( iPayloadLen > SIZE_MAX - iExtra ) { return false; }
	*pSize = iPayloadLen + iExtra;
	return true;
}


static bool __xwsBuildHttpResponseBytes(uint32 iStatusCode, const char* sBody, const char* sAccept,
	const char* sProtocol, const char* sExtensions, char** ppOut, size_t* pOutLen)
{
	return __xwsBuildHttpResponseBytesEx(iStatusCode, NULL, sBody,
		sBody ? strlen(sBody) : 0u, sAccept, sProtocol, sExtensions, NULL, ppOut, pOutLen);
}


// 内部函数：__xwsBuildFrameBytesEx
static bool __xwsBuildFrameBytesEx2(uint8 iOpcode, bool bFin, bool bMask, bool bRsv1,
	const void* pPayload, size_t iPayloadLen, char** ppOut, size_t* pOutLen)
{
	char* pBuf;
	size_t iNeed;
	size_t iPos = 0;
	uint8 aMask[4] = {0};
	const uint8* pBytes = (const uint8*)pPayload;
	if ( !ppOut || !pOutLen || (!pPayload && iPayloadLen != 0u) ) { return false; }
	if ( iOpcode != XCODEC_WS_OPCODE_CONT && iOpcode != XCODEC_WS_OPCODE_TEXT &&
		iOpcode != XCODEC_WS_OPCODE_BINARY && iOpcode != XCODEC_WS_OPCODE_CLOSE &&
		iOpcode != XCODEC_WS_OPCODE_PING && iOpcode != XCODEC_WS_OPCODE_PONG ) { return false; }
	if ( iOpcode >= 0x8u && (!bFin || iPayloadLen > 125u) ) { return false; }
	if ( bRsv1 && (iOpcode == XCODEC_WS_OPCODE_CONT || iOpcode >= 0x8u) ) { return false; }
	// 计算帧所需总大小并分配缓冲区
	if ( !__xwsFrameSize(iPayloadLen, bMask, &iNeed) ) { return false; }
	pBuf = (char*)XNET_ALLOC(iNeed);
	if ( !pBuf ) { return false; }
	// 第一个字节：FIN 位 + 操作码
	((uint8*)pBuf)[iPos++] = (uint8)((bFin ? 0x80u : 0x00u) |
		(bRsv1 ? XCODEC_WS_RSV1 : 0u) | (iOpcode & 0x0Fu));
	// 第二个字节：MASK 位 + 负载长度（7位、16位或64位编码）
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
	// 若需要掩码则生成随机掩码并写入头部
	if ( bMask ) {
		xrtRandomBytes(aMask, sizeof(aMask));
		memcpy(pBuf + iPos, aMask, sizeof(aMask));
		iPos += sizeof(aMask);
	}
	// 写入负载数据并按需应用掩码
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


static bool __xwsBuildFrameBytesEx(uint8 iOpcode, bool bFin, bool bMask,
	const void* pPayload, size_t iPayloadLen, char** ppOut, size_t* pOutLen)
{
	return __xwsBuildFrameBytesEx2(iOpcode, bFin, bMask, false, pPayload, iPayloadLen, ppOut, pOutLen);
}


// 内部函数：__xwsBuildFrameBytes
static bool __xwsBuildFrameBytes(uint8 iOpcode, bool bMask, const void* pPayload, size_t iPayloadLen, char** ppOut, size_t* pOutLen)
{
	return __xwsBuildFrameBytesEx(iOpcode, true, bMask, pPayload, iPayloadLen, ppOut, pOutLen);
}


// 内部函数：__xwsBuildClosePayload
static bool __xwsBuildClosePayload(uint16 iCode, const char* sReason, size_t iReasonLen, char** ppOut, size_t* pOutLen)
{
	char* pBuf;
	if ( !ppOut || !pOutLen ) { return false; }
	if ( iCode == 0 && (!sReason || iReasonLen == 0u) ) {
		*ppOut = NULL;
		*pOutLen = 0u;
		return true;
	}
	if ( iCode == 0u ) { iCode = XWS_CLOSE_NORMAL; }
	if ( !__xwsValidCloseCode(iCode) || iReasonLen > XWS_CLOSE_REASON_CAP ||
		(iReasonLen > 0u && !__xwsValidUtf8(sReason, iReasonLen)) ) { return false; }
	pBuf = (char*)XNET_ALLOC(2u + iReasonLen);
	if ( !pBuf ) { return false; }
	pBuf[0] = (char)((iCode >> 8u) & 0xFFu);
	pBuf[1] = (char)(iCode & 0xFFu);
	if ( sReason && iReasonLen > 0 ) { memcpy(pBuf + 2u, sReason, iReasonLen); }
	*ppOut = pBuf;
	*pOutLen = 2u + iReasonLen;
	return true;
}


static void __xwsOwnedBufferRelease(ptr pCtx, const void* pData, size_t iLen)
{
	(void)pCtx;
	(void)iLen;
	XNET_FREE((void*)pData);
}


static xnet_result __xwsStreamSendOwnedBytesEx(xnetstream* pStream, void* pData, size_t iLen,
	bool bControl)
{
	xnet_result iResult;
	xnetbufref tRef;
	if ( !pStream || !pData || iLen == 0u ) {
		XNET_FREE(pData);
		return XRT_NET_ERROR;
	}
	if ( iLen > UINT32_MAX ) {
		if ( bControl ) {
			XNET_FREE(pData);
			return XRT_NET_ERROR;
		}
		iResult = xrtNetStreamSend(pStream, pData, iLen);
		XNET_FREE(pData);
		return iResult;
	}
	memset(&tRef, 0, sizeof(tRef));
	tRef.pData = pData;
	tRef.iLen = (uint32)iLen;
	tRef.pfnRelease = __xwsOwnedBufferRelease;
	iResult = __xnetStreamSendRefEx(pStream, &tRef, bControl);
	if ( iResult != XRT_NET_OK ) { XNET_FREE(pData); }
	return iResult;
}


static xnet_result __xwsStreamSendOwnedBytes(xnetstream* pStream, void* pData, size_t iLen)
{
	return __xwsStreamSendOwnedBytesEx(pStream, pData, iLen, false);
}


static void __xwsConfigureStreamControlBudget(xnetstream* pStream)
{
	if ( pStream ) { pStream->iControlReserveBytes = __XWS_CONTROL_SEND_RESERVE_BYTES; }
}


// Consumes pData on both success and failure.
static bool __xwsStreamQueueOwnedBytesDirectEx(xnetstream* pStream, void* pData, size_t iLen,
	bool bControl)
{
	bool bQueued;
	xnetbufref tRef;
	if ( !pStream || !pData || iLen == 0u ) {
		XNET_FREE(pData);
		return false;
	}
	if ( pStream->pTls ) {
		bQueued = bControl ? __xnetStreamAppendTlsControlPlainCopy(pStream, pData, iLen) :
			__xnetStreamAppendTlsPlainCopy(pStream, pData, iLen);
		XNET_FREE(pData);
	} else if ( iLen > UINT32_MAX ) {
		bQueued = bControl ? __xnetStreamAppendControlSendCopy(pStream, pData, iLen) :
			__xnetStreamAppendSendCopy(pStream, pData, iLen);
		XNET_FREE(pData);
	} else {
		memset(&tRef, 0, sizeof(tRef));
		tRef.pData = pData;
		tRef.iLen = (uint32)iLen;
		tRef.pfnRelease = __xwsOwnedBufferRelease;
		bQueued = bControl ? __xnetStreamAppendControlSendRef(pStream, &tRef) :
			__xnetStreamAppendSendRef(pStream, &tRef);
		if ( !bQueued ) { XNET_FREE(pData); }
	}
	if ( !bQueued ) { return false; }
	__xnetStreamKickWrite(pStream);
	return true;
}


static bool __xwsStreamQueueOwnedBytesDirect(xnetstream* pStream, void* pData, size_t iLen)
{
	return __xwsStreamQueueOwnedBytesDirectEx(pStream, pData, iLen, false);
}


// 内部函数：__xwsStreamSendFrame
static xnet_result __xwsStreamSendFrame(xnetstream* pStream, bool bMask, uint8 iOpcode, const void* pPayload, size_t iPayloadLen)
{
	char* pFrame = NULL;
	size_t iFrameLen = 0u;
	xnet_result iRet;
	if ( !pStream ) { return XRT_NET_ERROR; }
	if ( !__xwsFrameSize(iPayloadLen, bMask, &iFrameLen) ) { return XRT_NET_ERROR; }
	if ( pStream->iMaxQueuedBytes > 0u &&
		iFrameLen > (size_t)(pStream->iMaxQueuedBytes -
			(pStream->iControlReserveBytes < pStream->iMaxQueuedBytes ?
			pStream->iControlReserveBytes : pStream->iMaxQueuedBytes)) ) { return XRT_NET_AGAIN; }
	if ( !__xwsBuildFrameBytesEx(iOpcode, true, bMask, pPayload, iPayloadLen, &pFrame, &iFrameLen) ) { return XRT_NET_ERROR; }
	iRet = __xwsStreamSendOwnedBytesEx(pStream, pFrame, iFrameLen,
		(iOpcode & 0x08u) != 0u);
	return iRet;
}


static xnet_result __xwsStreamSendMessage(xnetstream* pStream, bool bMask, uint8 iOpcode,
	const void* pPayload, size_t iPayloadLen, bool bPerMessageDeflate,
	uint32 iCompressMinBytes, int32 iCompressionLevel)
{
	#if XWS_HAS_PERMESSAGE_DEFLATE
		if ( bPerMessageDeflate && iPayloadLen >= (size_t)iCompressMinBytes && iPayloadLen > 0u ) {
			void* pCompressed = NULL;
			size_t iCompressedLen = 0u;
			if ( __xdeflatePmdMessage(pPayload, iPayloadLen, (int)iCompressionLevel,
				&pCompressed, &iCompressedLen) ) {
				if ( iCompressedLen < iPayloadLen ) {
					char* pFrame = NULL;
					size_t iFrameLen = 0u;
					xnet_result iRet;
					if ( !__xwsBuildFrameBytesEx2(iOpcode, true, bMask, true, pCompressed,
						iCompressedLen, &pFrame, &iFrameLen) ) {
						XNET_FREE(pCompressed);
						return XRT_NET_ERROR;
					}
					iRet = __xwsStreamSendOwnedBytes(pStream, pFrame, iFrameLen);
					XNET_FREE(pCompressed);
					return iRet;
				}
				XNET_FREE(pCompressed);
			}
		}
	#else
		(void)bPerMessageDeflate; (void)iCompressMinBytes; (void)iCompressionLevel;
	#endif
	return __xwsStreamSendFrame(pStream, bMask, iOpcode, pPayload, iPayloadLen);
}


// 内部函数：发送流 frame 扩展
static xnet_result UNUSED_ATTR __xwsStreamSendFrameEx(xnetstream* pStream, bool bFin, bool bMask, uint8 iOpcode, const void* pPayload, size_t iPayloadLen)
{
	char* pFrame = NULL;
	size_t iFrameLen = 0u;
	xnet_result iRet;
	if ( !pStream ) { return XRT_NET_ERROR; }
	if ( !__xwsFrameSize(iPayloadLen, bMask, &iFrameLen) ) { return XRT_NET_ERROR; }
	if ( pStream->iMaxQueuedBytes > 0u &&
		iFrameLen > (size_t)(pStream->iMaxQueuedBytes -
			(pStream->iControlReserveBytes < pStream->iMaxQueuedBytes ?
			pStream->iControlReserveBytes : pStream->iMaxQueuedBytes)) ) { return XRT_NET_AGAIN; }
	if ( !__xwsBuildFrameBytesEx(iOpcode, bFin, bMask, pPayload, iPayloadLen, &pFrame, &iFrameLen) ) { return XRT_NET_ERROR; }
	iRet = __xwsStreamSendOwnedBytesEx(pStream, pFrame, iFrameLen,
		(iOpcode & 0x08u) != 0u);
	return iRet;
}


// 内部函数：__xwsStreamQueueFrameDirectEx
static bool UNUSED_ATTR __xwsStreamQueueFrameDirectEx(xnetstream* pStream, bool bFin, bool bMask, uint8 iOpcode, const void* pPayload, size_t iPayloadLen)
{
	char* pFrame = NULL;
	size_t iFrameLen = 0u;
	bool bRet;
	if ( !pStream ) { return false; }
	if ( !__xwsBuildFrameBytesEx(iOpcode, bFin, bMask, pPayload, iPayloadLen, &pFrame, &iFrameLen) ) { return false; }
	bRet = __xwsStreamQueueOwnedBytesDirectEx(pStream, pFrame, iFrameLen,
		(iOpcode & 0x08u) != 0u);
	return bRet;
}


// 内部函数：__xwsStreamQueueFrameDirect
static bool __xwsStreamQueueFrameDirect(xnetstream* pStream, bool bMask, uint8 iOpcode, const void* pPayload, size_t iPayloadLen)
{
	char* pFrame = NULL;
	size_t iFrameLen = 0u;
	bool bRet;
	if ( !pStream ) { return false; }
	if ( !__xwsBuildFrameBytesEx(iOpcode, true, bMask, pPayload, iPayloadLen, &pFrame, &iFrameLen) ) { return false; }
	bRet = __xwsStreamQueueOwnedBytesDirectEx(pStream, pFrame, iFrameLen,
		(iOpcode & 0x08u) != 0u);
	return bRet;
}


// 内部函数：关闭任务
static void __xwsCloseTask(xnetworker* pWorker, ptr pArg)
{
	__xws_close_task* pTask = (__xws_close_task*)pArg;
	(void)pWorker;
	if ( !pTask ) { return; }
	if ( pTask->pStream && pTask->pFrame && pTask->iFrameLen > 0u ) {
		(void)__xwsStreamQueueOwnedBytesDirectEx(pTask->pStream, pTask->pFrame, pTask->iFrameLen, true);
		pTask->pFrame = NULL;
	}
	if ( pTask->pStream && pTask->bCloseStream ) {
		xrtNetStreamClose(pTask->pStream, XNET_CLOSE_F_GRACEFUL | XNET_CLOSE_F_WAIT_PEER);
	}
	XNET_FREE(pTask->pFrame);
	XNET_FREE(pTask);
}


// 内部函数：__xwsPostClose
static xnet_result __xwsPostClose(xnetstream* pStream, bool bMask, uint16 iCode, const char* sReason, bool bCloseStream)
{
	__xws_close_task* pTask;
	char* pPayload = NULL;
	char* pFrame = NULL;
	size_t iPayloadLen = 0u;
	size_t iFrameLen = 0u;
	size_t iReasonLen = sReason ? strlen(sReason) : 0u;
	if ( !pStream || !pStream->pEngine || !pStream->pWorker ) { return XRT_NET_ERROR; }
	// 构建 Close 帧负载（关闭码 + 原因）
	if ( !__xwsBuildClosePayload(iCode, sReason, iReasonLen, &pPayload, &iPayloadLen) ) { return XRT_NET_ERROR; }
	// 将负载封装为 WebSocket Close 帧
	if ( !__xwsBuildFrameBytes(XCODEC_WS_OPCODE_CLOSE, bMask, pPayload, iPayloadLen, &pFrame, &iFrameLen) ) {
		XNET_FREE(pPayload);
		return XRT_NET_ERROR;
	}
	XNET_FREE(pPayload);
	// 创建异步关闭任务并投递到工作线程
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


static xnet_result __xwsStartClose(xnetstream* pStream, bool bMask,
	volatile long* pClosePosted, volatile long* pCloseFlags, uint16* pLocalCode,
	uint16 iCode, const char* sReason, bool bCloseStream, bool* pStarted)
{
	xnet_result iResult;
	uint16 iActualCode = (iCode == 0u && sReason && sReason[0]) ? XWS_CLOSE_NORMAL : iCode;
	if ( pStarted ) { *pStarted = false; }
	if ( !pStream || !pClosePosted || !pCloseFlags || !pLocalCode ) { return XRT_NET_ERROR; }
	if ( __xwsAtomicCompareExchange(pClosePosted, 1, 0) != 0 ) { return XRT_NET_OK; }
	iResult = __xwsPostClose(pStream, bMask, iCode, sReason, bCloseStream);
	if ( iResult == XRT_NET_OK ) {
		__xwsRecordLocalClose(pCloseFlags, pLocalCode, iActualCode);
		if ( pStarted ) { *pStarted = true; }
	} else {
		(void)__xwsAtomicCompareExchange(pClosePosted, 0, 1);
	}
	return iResult;
}


static xnet_result __xwsClientStartClose(xwsclient* pClient, uint16 iCode,
	const char* sReason, bool bCloseStream)
{
	bool bStarted = false;
	xnet_result iResult;
	if ( !pClient ) { return XRT_NET_ERROR; }
	__xwsLock(&pClient->iSendLock);
	iResult = __xwsStartClose(pClient->pStream, true, &pClient->iClosePosted,
		&pClient->iCloseFlags, &pClient->iLocalCloseCode, iCode, sReason, bCloseStream, &bStarted);
	__xwsUnlock(&pClient->iSendLock);
	if ( iResult == XRT_NET_OK && bStarted && !__xwsArmTimerSlot(pClient->pStream,
		pClient->tConfig.iCloseTimeoutMs, &pClient->iTimerLock, &pClient->pCloseTimer) ) {
		xrtNetStreamClose(pClient->pStream, XNET_CLOSE_F_ABORT);
		return XRT_NET_ERROR;
	}
	return iResult;
}


static xnet_result __xwsConnStartClose(xwsconn* pConn, uint16 iCode,
	const char* sReason, bool bCloseStream)
{
	bool bStarted = false;
	xnet_result iResult;
	if ( !pConn ) { return XRT_NET_ERROR; }
	__xwsLock(&pConn->iSendLock);
	iResult = __xwsStartClose(pConn->pStream, false, &pConn->iClosePosted,
		&pConn->iCloseFlags, &pConn->iLocalCloseCode, iCode, sReason, bCloseStream, &bStarted);
	__xwsUnlock(&pConn->iSendLock);
	if ( iResult == XRT_NET_OK && bStarted && !__xwsArmTimerSlot(pConn->pStream,
		(pConn->pServer ? pConn->pServer->tConfig.iCloseTimeoutMs : 0u),
		&pConn->iTimerLock, &pConn->pCloseTimer) ) {
		xrtNetStreamClose(pConn->pStream, XNET_CLOSE_F_ABORT);
		return XRT_NET_ERROR;
	}
	return iResult;
}


// 内部函数：__xwsPeekPayloadCopy
static bool __xwsPeekPayloadCopy(xnetchain* pChain, const xcodecframe* pFrame, const xcodecwsframeinfo* pInfo, char** ppPayload, size_t* pPayloadLen)
{
	char* pBuf;
	size_t iNeed;
	if ( !ppPayload || !pPayloadLen || !pChain || !pFrame || !pInfo ) { return false; }
	iNeed = pFrame->iPayloadBytes;
	pBuf = (char*)XNET_ALLOC(iNeed + 1u);
	if ( !pBuf ) { return false; }
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


// 内部函数：__xwsClientEmitText
static void __xwsClientEmitText(xwsclient* pClient, const char* pData, size_t iLen)
{
	if ( pClient && pClient->tEvents.OnText ) {
		pClient->tEvents.OnText(pClient->pUserData, pClient, pData, iLen);
	}
}


// 内部函数：__xwsClientEmitBinary
static void __xwsClientEmitBinary(xwsclient* pClient, const void* pData, size_t iLen)
{
	if ( pClient && pClient->tEvents.OnBinary ) {
		pClient->tEvents.OnBinary(pClient->pUserData, pClient, pData, iLen);
	}
}


// 内部函数：__xwsServerEmitText
static void __xwsServerEmitText(xwsconn* pConn, const char* pData, size_t iLen)
{
	xwsserver* pServer = pConn ? pConn->pServer : NULL;
	if ( pServer && pServer->tEvents.OnText ) {
		pServer->tEvents.OnText(pServer->pUserData, pServer, pConn, pData, iLen);
	}
}


// 内部函数：__xwsServerEmitBinary
static void __xwsServerEmitBinary(xwsconn* pConn, const void* pData, size_t iLen)
{
	xwsserver* pServer = pConn ? pConn->pServer : NULL;
	if ( pServer && pServer->tEvents.OnBinary ) {
		pServer->tEvents.OnBinary(pServer->pUserData, pServer, pConn, pData, iLen);
	}
}


// 内部函数：__xwsClientConsumeDataFrame
static int __xwsClientConsumeDataFrame(xwsclient* pClient, uint8 iOpcode, bool bFin, const char* pPayload, size_t iPayloadLen)
{
	size_t iLimit = pClient && pClient->tConfig.iRecvLimit > 0u ? (size_t)pClient->tConfig.iRecvLimit : 0u;
	int iAppend;
	if ( !pClient ) { return __XWS_APPEND_INTERNAL; }
	// 续接帧：追加到已有的分片消息
	if ( iOpcode == XCODEC_WS_OPCODE_CONT ) {
		if ( pClient->iMsgOpcode == 0u ) { return __XWS_APPEND_PROTOCOL; }
		iAppend = __xwsMessageAppend(&pClient->pMsgBuf, &pClient->iMsgLen, &pClient->iMsgCap, &pClient->iMsgOpcode, 0u, pPayload, iPayloadLen, iLimit);
		if ( iAppend != __XWS_APPEND_OK ) { return iAppend; }
		// 最后一帧时触发回调并重置消息缓冲区
		if ( bFin ) {
			if ( pClient->iMsgOpcode == XCODEC_WS_OPCODE_TEXT ) {
				if ( !__xwsValidUtf8(pClient->pMsgBuf, pClient->iMsgLen) ) {
					__xwsMessageReset(&pClient->pMsgBuf, &pClient->iMsgLen, &pClient->iMsgCap, &pClient->iMsgOpcode);
					return __XWS_APPEND_INVALID_DATA;
				}
				__xwsClientEmitText(pClient, pClient->pMsgBuf ? pClient->pMsgBuf : "", pClient->iMsgLen);
			}
			else if ( pClient->iMsgOpcode == XCODEC_WS_OPCODE_BINARY ) { __xwsClientEmitBinary(pClient, pClient->pMsgBuf, pClient->iMsgLen); }
			else { return __XWS_APPEND_INTERNAL; }
			__xwsMessageReset(&pClient->pMsgBuf, &pClient->iMsgLen, &pClient->iMsgCap, &pClient->iMsgOpcode);
		}
		return __XWS_APPEND_OK;
	}
	// 新消息的第一帧必须是 text 或 binary
	if ( iOpcode != XCODEC_WS_OPCODE_TEXT && iOpcode != XCODEC_WS_OPCODE_BINARY ) { return __XWS_APPEND_PROTOCOL; }
	if ( pClient->iMsgOpcode != 0u ) { return __XWS_APPEND_PROTOCOL; }
	// 单帧完整消息直接触发回调
	if ( bFin ) {
		if ( iOpcode == XCODEC_WS_OPCODE_TEXT ) {
			if ( !__xwsValidUtf8(pPayload, iPayloadLen) ) { return __XWS_APPEND_INVALID_DATA; }
			__xwsClientEmitText(pClient, pPayload, iPayloadLen);
		}
		else { __xwsClientEmitBinary(pClient, pPayload, iPayloadLen); }
		return __XWS_APPEND_OK;
	}
	// 分片消息的首帧：缓冲数据等待后续帧
	return __xwsMessageAppend(&pClient->pMsgBuf, &pClient->iMsgLen, &pClient->iMsgCap, &pClient->iMsgOpcode, iOpcode, pPayload, iPayloadLen, iLimit);
}


// 内部函数：__xwsServerConsumeDataFrame
static int __xwsServerConsumeDataFrame(xwsconn* pConn, uint8 iOpcode, bool bFin, const char* pPayload, size_t iPayloadLen)
{
	size_t iLimit = (pConn && pConn->pServer && pConn->pServer->tConfig.iRecvLimit > 0u) ? (size_t)pConn->pServer->tConfig.iRecvLimit : 0u;
	int iAppend;
	if ( !pConn ) { return __XWS_APPEND_INTERNAL; }
	// 续接帧：追加到已有的分片消息
	if ( iOpcode == XCODEC_WS_OPCODE_CONT ) {
		if ( pConn->iMsgOpcode == 0u ) { return __XWS_APPEND_PROTOCOL; }
		iAppend = __xwsMessageAppend(&pConn->pMsgBuf, &pConn->iMsgLen, &pConn->iMsgCap, &pConn->iMsgOpcode, 0u, pPayload, iPayloadLen, iLimit);
		if ( iAppend != __XWS_APPEND_OK ) { return iAppend; }
		// 最后一帧时触发回调并重置消息缓冲区
		if ( bFin ) {
			if ( pConn->iMsgOpcode == XCODEC_WS_OPCODE_TEXT ) {
				if ( !__xwsValidUtf8(pConn->pMsgBuf, pConn->iMsgLen) ) {
					__xwsMessageReset(&pConn->pMsgBuf, &pConn->iMsgLen, &pConn->iMsgCap, &pConn->iMsgOpcode);
					return __XWS_APPEND_INVALID_DATA;
				}
				__xwsServerEmitText(pConn, pConn->pMsgBuf ? pConn->pMsgBuf : "", pConn->iMsgLen);
			}
			else if ( pConn->iMsgOpcode == XCODEC_WS_OPCODE_BINARY ) { __xwsServerEmitBinary(pConn, pConn->pMsgBuf, pConn->iMsgLen); }
			else { return __XWS_APPEND_INTERNAL; }
			__xwsMessageReset(&pConn->pMsgBuf, &pConn->iMsgLen, &pConn->iMsgCap, &pConn->iMsgOpcode);
		}
		return __XWS_APPEND_OK;
	}
	// 新消息的第一帧必须是 text 或 binary
	if ( iOpcode != XCODEC_WS_OPCODE_TEXT && iOpcode != XCODEC_WS_OPCODE_BINARY ) { return __XWS_APPEND_PROTOCOL; }
	if ( pConn->iMsgOpcode != 0u ) { return __XWS_APPEND_PROTOCOL; }
	// 单帧完整消息直接触发回调
	if ( bFin ) {
		if ( iOpcode == XCODEC_WS_OPCODE_TEXT ) {
			if ( !__xwsValidUtf8(pPayload, iPayloadLen) ) { return __XWS_APPEND_INVALID_DATA; }
			__xwsServerEmitText(pConn, pPayload, iPayloadLen);
		}
		else { __xwsServerEmitBinary(pConn, pPayload, iPayloadLen); }
		return __XWS_APPEND_OK;
	}
	// 分片消息的首帧：缓冲数据等待后续帧
	return __xwsMessageAppend(&pConn->pMsgBuf, &pConn->iMsgLen, &pConn->iMsgCap, &pConn->iMsgOpcode, iOpcode, pPayload, iPayloadLen, iLimit);
}


static int __xwsInflateMessage(const void* pData, size_t iLen, size_t iLimit, char** ppData, size_t* pOutLen)
{
	#if XWS_HAS_PERMESSAGE_DEFLATE
		__xinflate_result eResult = __xinflatePmdMessage(pData, iLen,
			iLimit > 0u ? (uint64)iLimit : UINT64_MAX, (void**)ppData, pOutLen);
		if ( eResult == __XINFLATE_OK ) { return __XWS_APPEND_OK; }
		return eResult == __XINFLATE_LIMIT ? __XWS_APPEND_TOO_BIG : __XWS_APPEND_INVALID_DATA;
	#else
		(void)pData; (void)iLen; (void)iLimit; (void)ppData; (void)pOutLen;
		return __XWS_APPEND_INTERNAL;
	#endif
}


static int __xwsClientConsumeDataFrameEx(xwsclient* pClient, uint8 iOpcode, bool bFin,
	bool bCompressed, const char* pPayload, size_t iPayloadLen)
{
	size_t iLimit = pClient && pClient->tConfig.iRecvLimit > 0u ? (size_t)pClient->tConfig.iRecvLimit : 0u;
	if ( !pClient ) { return __XWS_APPEND_INTERNAL; }
	if ( bCompressed ) {
		char* pInflated = NULL;
		size_t iInflatedLen = 0u;
		int iResult;
		if ( iOpcode != XCODEC_WS_OPCODE_TEXT && iOpcode != XCODEC_WS_OPCODE_BINARY ) { return __XWS_APPEND_PROTOCOL; }
		if ( pClient->iMsgOpcode != 0u ) { return __XWS_APPEND_PROTOCOL; }
		if ( !bFin ) {
			pClient->bMsgCompressed = true;
			return __xwsMessageAppend(&pClient->pMsgBuf, &pClient->iMsgLen, &pClient->iMsgCap,
				&pClient->iMsgOpcode, iOpcode, pPayload, iPayloadLen, iLimit);
		}
		iResult = __xwsInflateMessage(pPayload, iPayloadLen, iLimit, &pInflated, &iInflatedLen);
		if ( iResult == __XWS_APPEND_OK ) {
			iResult = __xwsClientConsumeDataFrame(pClient, iOpcode, true, pInflated, iInflatedLen);
		}
		XNET_FREE(pInflated);
		return iResult;
	}
	if ( iOpcode == XCODEC_WS_OPCODE_CONT && pClient->bMsgCompressed ) {
		char* pInflated = NULL;
		size_t iInflatedLen = 0u;
		uint8 iMessageOpcode;
		int iResult = __xwsMessageAppend(&pClient->pMsgBuf, &pClient->iMsgLen, &pClient->iMsgCap,
			&pClient->iMsgOpcode, 0u, pPayload, iPayloadLen, iLimit);
		if ( iResult != __XWS_APPEND_OK || !bFin ) { return iResult; }
		iMessageOpcode = pClient->iMsgOpcode;
		iResult = __xwsInflateMessage(pClient->pMsgBuf, pClient->iMsgLen, iLimit, &pInflated, &iInflatedLen);
		__xwsMessageReset(&pClient->pMsgBuf, &pClient->iMsgLen, &pClient->iMsgCap, &pClient->iMsgOpcode);
		pClient->bMsgCompressed = false;
		if ( iResult == __XWS_APPEND_OK ) {
			iResult = __xwsClientConsumeDataFrame(pClient, iMessageOpcode, true, pInflated, iInflatedLen);
		}
		XNET_FREE(pInflated);
		return iResult;
	}
	if ( pClient->bMsgCompressed ) { return __XWS_APPEND_PROTOCOL; }
	return __xwsClientConsumeDataFrame(pClient, iOpcode, bFin, pPayload, iPayloadLen);
}


static int __xwsServerConsumeDataFrameEx(xwsconn* pConn, uint8 iOpcode, bool bFin,
	bool bCompressed, const char* pPayload, size_t iPayloadLen)
{
	size_t iLimit = (pConn && pConn->pServer && pConn->pServer->tConfig.iRecvLimit > 0u)
		? (size_t)pConn->pServer->tConfig.iRecvLimit : 0u;
	if ( !pConn ) { return __XWS_APPEND_INTERNAL; }
	if ( bCompressed ) {
		char* pInflated = NULL;
		size_t iInflatedLen = 0u;
		int iResult;
		if ( iOpcode != XCODEC_WS_OPCODE_TEXT && iOpcode != XCODEC_WS_OPCODE_BINARY ) { return __XWS_APPEND_PROTOCOL; }
		if ( pConn->iMsgOpcode != 0u ) { return __XWS_APPEND_PROTOCOL; }
		if ( !bFin ) {
			pConn->bMsgCompressed = true;
			return __xwsMessageAppend(&pConn->pMsgBuf, &pConn->iMsgLen, &pConn->iMsgCap,
				&pConn->iMsgOpcode, iOpcode, pPayload, iPayloadLen, iLimit);
		}
		iResult = __xwsInflateMessage(pPayload, iPayloadLen, iLimit, &pInflated, &iInflatedLen);
		if ( iResult == __XWS_APPEND_OK ) {
			iResult = __xwsServerConsumeDataFrame(pConn, iOpcode, true, pInflated, iInflatedLen);
		}
		XNET_FREE(pInflated);
		return iResult;
	}
	if ( iOpcode == XCODEC_WS_OPCODE_CONT && pConn->bMsgCompressed ) {
		char* pInflated = NULL;
		size_t iInflatedLen = 0u;
		uint8 iMessageOpcode;
		int iResult = __xwsMessageAppend(&pConn->pMsgBuf, &pConn->iMsgLen, &pConn->iMsgCap,
			&pConn->iMsgOpcode, 0u, pPayload, iPayloadLen, iLimit);
		if ( iResult != __XWS_APPEND_OK || !bFin ) { return iResult; }
		iMessageOpcode = pConn->iMsgOpcode;
		iResult = __xwsInflateMessage(pConn->pMsgBuf, pConn->iMsgLen, iLimit, &pInflated, &iInflatedLen);
		__xwsMessageReset(&pConn->pMsgBuf, &pConn->iMsgLen, &pConn->iMsgCap, &pConn->iMsgOpcode);
		pConn->bMsgCompressed = false;
		if ( iResult == __XWS_APPEND_OK ) {
			iResult = __xwsServerConsumeDataFrame(pConn, iMessageOpcode, true, pInflated, iInflatedLen);
		}
		XNET_FREE(pInflated);
		return iResult;
	}
	if ( pConn->bMsgCompressed ) { return __XWS_APPEND_PROTOCOL; }
	return __xwsServerConsumeDataFrame(pConn, iOpcode, bFin, pPayload, iPayloadLen);
}


/* ============================== Client helpers ============================== */

static void __xwsClientEmitError(xwsclient* pClient, int iSysErr)
{
	if ( !pClient ) { return; }
	pClient->iLastSysErr = iSysErr;
	if ( pClient->tEvents.OnError ) {
		pClient->tEvents.OnError(pClient->pUserData, pClient, iSysErr);
	}
}


static void __xwsClientResolveOpenWaiterList(xwsclient* pClient,
	__xws_open_waiter* pWaiter, xnet_result iStatus)
{
	if ( !pClient ) { return; }
	while ( pWaiter ) {
		__xws_open_waiter* pNext = pWaiter->pNext;
		(void)__xnetFutureResolve(pWaiter->pFuture, iStatus,
			iStatus == XRT_NET_OK ? pClient : NULL);
		__xnetFutureReleaseAsyncHold(pWaiter->pFuture);
		XNET_FREE(pWaiter);
		pWaiter = pNext;
	}
}


static bool __xwsClientFinishOpening(xwsclient* pClient, long iState,
	xnet_result iStatus)
{
	__xws_open_waiter* pWaiter;
	bool bFinished = false;
	if ( !pClient ) { return false; }
	__xwsLock(&pClient->iOpenWaitLock);
	if ( __xwsAtomicLoad(&pClient->iRunState) == __XWS_RUN_OPENING ) {
		(void)__xnetAtomicExchange32(&pClient->iRunState, iState);
		pClient->iOpenResult = iStatus;
		pWaiter = pClient->pOpenWaitHead;
		pClient->pOpenWaitHead = NULL;
		bFinished = true;
	} else {
		pWaiter = NULL;
	}
	__xwsUnlock(&pClient->iOpenWaitLock);
	__xwsClientResolveOpenWaiterList(pClient, pWaiter, iStatus);
	return bFinished;
}


static void __xwsClientSetOpenFailure(xwsclient* pClient, xnet_result iStatus)
{
	if ( !pClient || iStatus == XRT_NET_OK ) { return; }
	(void)__xwsClientFinishOpening(pClient, __XWS_RUN_CLOSED, iStatus);
}


// 内部函数：__xwsClientEmitCloseOnce
static void __xwsClientEmitCloseOnce(xwsclient* pClient, xnet_result iReason)
{
	xwscloseinfo tInfo;
	if ( !pClient ) { return; }
	pClient->iCloseTransportResult = iReason;
	(void)__xwsAtomicCompareExchange(&pClient->iOpen, 0, 1);
	if ( __xwsClientFinishOpening(pClient, __XWS_RUN_CLOSED,
		iReason == XRT_NET_OK ? XRT_NET_CLOSED : iReason) ) {
	} else {
		(void)__xwsAtomicCompareExchange(&pClient->iRunState, __XWS_RUN_CLOSED,
			__XWS_RUN_OPEN);
	}
	if ( __xwsAtomicCompareExchange(&pClient->iCloseNotified, 1, 0) != 0 ) { return; }
	__xwsFillCloseInfo(&tInfo, &pClient->iCloseFlags, iReason, pClient->iLocalCloseCode,
		pClient->iRemoteCloseCode, pClient->sRemoteCloseReason, pClient->iRemoteCloseReasonLen);
	if ( pClient->tEvents.OnCloseEx ) {
		pClient->tEvents.OnCloseEx(pClient->pUserData, pClient, &tInfo);
	}
	if ( pClient->tEvents.OnClose ) {
		pClient->tEvents.OnClose(pClient->pUserData, pClient, iReason);
	}
}


// 内部函数：判断是否为 benign 流错误
static bool __xwsIsBenignStreamError(int iSysErr, xnetstream* pStream, volatile long* pClosePosted, volatile long* pCloseNotified)
{
	if ( pStream && pStream->bClosing ) { return true; }
	if ( pClosePosted && __xwsAtomicLoad(pClosePosted) != 0 ) { return true; }
	if ( pCloseNotified && __xwsAtomicLoad(pCloseNotified) != 0 ) { return true; }
	if ( iSysErr == -1 ) { return (pClosePosted && __xwsAtomicLoad(pClosePosted) != 0) || (pCloseNotified && __xwsAtomicLoad(pCloseNotified) != 0); }
	#if defined(_WIN32) || defined(_WIN64)
		return iSysErr == WSAECONNRESET || iSysErr == WSAECONNABORTED || iSysErr == WSAESHUTDOWN ||
			iSysErr == WSAENOTSOCK || iSysErr == WSA_OPERATION_ABORTED;
	#else
		return iSysErr == ECONNRESET || iSysErr == ECONNABORTED || iSysErr == EPIPE ||
			iSysErr == ESHUTDOWN || iSysErr == ENOTSOCK || iSysErr == EBADF;
	#endif
}


// 内部函数：__xwsClientValidateHandshake
static bool __xwsClientValidateHandshake(xwsclient* pClient, const xcodechttp1msg* pMsg)
{
	const char* sRequestedProtocol;
	const char* sUpgrade;
	const char* sAccept;
	const char* sProtocol = NULL;
	uint32 iProtocolCount;
	uint32 iExtensionCount;
	bool bPmd = false;
	if ( !pClient || !pMsg ) { return false; }
	sRequestedProtocol = __xwsClientConfigProtocolValue(&pClient->tConfig);
	if ( (pMsg->iFlags & XCODEC_HTTP1_F_RESPONSE) == 0u || pMsg->iStatusCode != 101u ||
		!__xwsStrEqNoCase(pMsg->sVersion, "HTTP/1.1") ) { return false; }
	if ( __xwsHeaderCount(pMsg, "Upgrade", &sUpgrade) != 1u ||
		__xwsHeaderCount(pMsg, "Sec-WebSocket-Accept", &sAccept) != 1u ) { return false; }
	if ( __xwsHeaderCount(pMsg, "Content-Length", NULL) != 0u ||
		__xwsHeaderCount(pMsg, "Transfer-Encoding", NULL) != 0u ) { return false; }
	if ( !sUpgrade || !__xwsStrEqNoCase(sUpgrade, "websocket") ) { return false; }
	if ( !__xwsHeaderHasTokenNoCase(pMsg, "Connection", "Upgrade") ) { return false; }
	if ( !sAccept || strcmp(sAccept, pClient->sExpectedAccept) != 0 ) { return false; }
	iProtocolCount = __xwsHeaderCount(pMsg, "Sec-WebSocket-Protocol", &sProtocol);
	if ( sRequestedProtocol[0] ) {
		if ( iProtocolCount != 1u || !__xwsValidToken(sProtocol) ||
			!__xwsProtocolListContains(sRequestedProtocol, sProtocol) ) { return false; }
	} else if ( iProtocolCount != 0u ) {
		return false;
	}
	iExtensionCount = __xwsHeaderCount(pMsg, "Sec-WebSocket-Extensions", NULL);
	if ( (pClient->tConfig.iWebSocketFlags & XWS_F_PERMESSAGE_DEFLATE) != 0u ) {
		if ( iExtensionCount > 0u && (!__xwsNegotiatePmd(pMsg, true, &bPmd) || !bPmd) ) { return false; }
	} else if ( iExtensionCount != 0u ) {
		return false;
	}
	pClient->bPerMessageDeflate = bPmd;
	return __xwsReplaceText(&pClient->sProtocol, sProtocol);
}


static uint16 __xwsValidateClosePayload(const char* pPayload, size_t iPayloadLen, uint16* pCode)
{
	uint16 iCode;
	if ( pCode ) { *pCode = 0u; }
	if ( iPayloadLen == 0u ) { return 0u; }
	if ( !pPayload || iPayloadLen == 1u ) { return XWS_CLOSE_PROTOCOL; }
	iCode = (uint16)(((uint8)pPayload[0] << 8u) | (uint8)pPayload[1]);
	if ( !__xwsValidCloseCode(iCode) ) { return XWS_CLOSE_PROTOCOL; }
	if ( !__xwsValidUtf8(pPayload + 2u, iPayloadLen - 2u) ) { return XWS_CLOSE_INVALID_DATA; }
	if ( pCode ) { *pCode = iCode; }
	return 0u;
}


static uint16 __xwsFrameParseErrorCloseCode(const xnetchain* pChain, uint64 iFrameLimit,
	uint8 iAllowedRsvBits)
{
	xcodecframe tFrame;
	xcodecwsframeinfo tInfo;
	if ( pChain && iFrameLimit != UINT64_MAX &&
		xrtCodecWsParseFrameEx2(pChain, &tFrame, &tInfo, UINT64_MAX,
			iAllowedRsvBits) != XCODEC_STATUS_ERROR ) {
		return XWS_CLOSE_TOO_BIG;
	}
	return XWS_CLOSE_PROTOCOL;
}


// 内部函数：__xwsClientConsumeFrames
static void __xwsClientConsumeFrames(xwsclient* pClient, xnetchain* pChain)
{
	while ( pClient && pChain ) {
		xcodecframe tFrame;
		xcodecwsframeinfo tInfo;
		uint64 iFrameLimit = __xwsResolveFrameMaxBytes(pClient->tConfig.iMaxFrameBytes,
			pClient->tConfig.iRecvLimit);
		xcodecstatus iParse = xrtCodecWsParseFrameEx2(pChain, &tFrame, &tInfo, iFrameLimit,
			pClient->bPerMessageDeflate ? XCODEC_WS_RSV1 : 0u);
		char* pPayload = NULL;
		size_t iPayloadLen = 0u;
		bool bFin;
		bool bCompressed;
		int iDataRet;
		uint16 iCloseCode = 0u;
		// 数据不足，等待更多数据
		if ( iParse == XCODEC_STATUS_NEED_MORE ) { return; }
		// 帧解析错误，关闭连接
		if ( iParse == XCODEC_STATUS_ERROR ) {
			uint16 iCloseCode = __xwsFrameParseErrorCloseCode(pChain, iFrameLimit,
				pClient->bPerMessageDeflate ? XCODEC_WS_RSV1 : 0u);
			(void)__xwsClientStartClose(pClient, iCloseCode,
				iCloseCode == XWS_CLOSE_TOO_BIG ? "frame too large" : "invalid frame", true);
			return;
		}
		if ( (tInfo.iFlags & XCODEC_WS_F_MASKED) != 0u ) {
			(void)__xwsClientStartClose(pClient, XWS_CLOSE_PROTOCOL, "masked server frame", true);
			return;
		}
		bCompressed = (tInfo.iFlags & XCODEC_WS_F_RSV1) != 0u;
		if ( bCompressed && ((tInfo.iFlags & XCODEC_WS_F_CONTROL) != 0u ||
			tInfo.iOpcode == XCODEC_WS_OPCODE_CONT) ) {
			(void)__xwsClientStartClose(pClient, XWS_CLOSE_PROTOCOL, "invalid rsv1", true);
			return;
		}
		// 控制帧负载不能超过 125 字节
		if ( (tInfo.iFlags & XCODEC_WS_F_CONTROL) != 0u && tInfo.iPayloadLen > 125u ) {
			(void)__xwsClientStartClose(pClient, XWS_CLOSE_PROTOCOL, "control too large", true);
			return;
		}
		// 提取并解掩码负载数据
		if ( !__xwsPeekPayloadCopy(pChain, &tFrame, &tInfo, &pPayload, &iPayloadLen) ) {
			__xwsClientEmitError(pClient, -3);
			xrtNetStreamClose(pClient->pStream, XNET_CLOSE_F_ABORT);
			return;
		}
		xrtCodecFrameConsume(pChain, &tFrame);
		bFin = (tInfo.iFlags & XCODEC_WS_F_FIN) != 0u;
		// 根据操作码分发处理
		switch ( tInfo.iOpcode ) {
			case XCODEC_WS_OPCODE_TEXT:
			case XCODEC_WS_OPCODE_BINARY:
			case XCODEC_WS_OPCODE_CONT:
				// 数据帧交给消息重组逻辑处理
				iDataRet = __xwsClientConsumeDataFrameEx(pClient, tInfo.iOpcode, bFin, bCompressed, pPayload, iPayloadLen);
				if ( iDataRet == __XWS_APPEND_OK ) { break; }
				if ( iDataRet == __XWS_APPEND_TOO_BIG ) {
					(void)__xwsClientStartClose(pClient, XWS_CLOSE_TOO_BIG, "message too large", true);
				} else if ( iDataRet == __XWS_APPEND_PROTOCOL ) {
					(void)__xwsClientStartClose(pClient, XWS_CLOSE_PROTOCOL, "bad fragment sequence", true);
				} else if ( iDataRet == __XWS_APPEND_INVALID_DATA ) {
					(void)__xwsClientStartClose(pClient, XWS_CLOSE_INVALID_DATA, "invalid message data", true);
				} else {
					__xwsClientEmitError(pClient, -7);
					xrtNetStreamClose(pClient->pStream, XNET_CLOSE_F_ABORT);
				}
				XNET_FREE(pPayload);
				return;
				break;
			case XCODEC_WS_OPCODE_PING:
				// 自动回复 Pong
				if ( pClient->pStream ) { (void)__xwsStreamQueueFrameDirect(pClient->pStream, true, XCODEC_WS_OPCODE_PONG, pPayload, iPayloadLen); }
				if ( pClient->tEvents.OnPing ) { pClient->tEvents.OnPing(pClient->pUserData, pClient, pPayload, iPayloadLen); }
				break;
			case XCODEC_WS_OPCODE_PONG:
				if ( pClient->tEvents.OnPong ) { pClient->tEvents.OnPong(pClient->pUserData, pClient, pPayload, iPayloadLen); }
				break;
			case XCODEC_WS_OPCODE_CLOSE:
				// 提取关闭码并通知关闭
				{
					uint16 iCloseError = __xwsValidateClosePayload(pPayload, iPayloadLen, &iCloseCode);
					if ( iCloseError != 0u ) {
						(void)__xwsClientStartClose(pClient, iCloseError,
							iCloseError == XWS_CLOSE_INVALID_DATA ? "invalid close reason" : "invalid close payload", true);
						XNET_FREE(pPayload);
						return;
					}
				}
				#if defined(XNET_DEBUG_CLOSE_DIAG)
					fprintf(stderr, "[CLOSE_DIAG][WS-CLIENT] recv close stream=%p code=%u posted=%ld open=%ld len=%zu\n",
						(void*)pClient->pStream,
						(unsigned)iCloseCode,
						(long)__xwsAtomicLoad(&pClient->iClosePosted),
						(long)__xwsAtomicLoad(&pClient->iOpen),
						iPayloadLen);
				#endif
				{
					bool bRemoteInitiated = __xwsAtomicLoad(&pClient->iClosePosted) == 0;
					__xwsRecordRemoteClose(&pClient->iCloseFlags, &pClient->iRemoteCloseCode,
						&pClient->iRemoteCloseReasonLen, pClient->sRemoteCloseReason,
						pPayload, iPayloadLen, bRemoteInitiated);
				}
				// 首次收到关闭帧则回复关闭帧，否则优雅关闭流
				if ( __xwsAtomicLoad(&pClient->iClosePosted) == 0 ) {
					if ( __xwsClientStartClose(pClient, iCloseCode, NULL, true) != XRT_NET_OK && pClient->pStream ) {
						xrtNetStreamClose(pClient->pStream, XNET_CLOSE_F_ABORT);
					}
				} else if ( pClient->pStream ) {
					xrtNetStreamClose(pClient->pStream, XNET_CLOSE_F_GRACEFUL);
				}
				XNET_FREE(pPayload);
				return;
			default:
				(void)__xwsClientStartClose(pClient, XWS_CLOSE_PROTOCOL, "bad opcode", true);
				XNET_FREE(pPayload);
				return;
		}
		XNET_FREE(pPayload);
	}
}


// 内部函数：__xwsClientStreamOnOpen
static void __xwsClientStreamOnOpen(ptr pOwner, xnetstream* pStream)
{
	xwsclient* pClient = (xwsclient*)pOwner;
	char* pHandshake = NULL;
	size_t iHandshakeLen = 0u;
	if ( !pClient || !pStream ) { return; }
	if ( !__xwsBuildClientHandshake(pClient, &pHandshake, &iHandshakeLen) ) {
		__xwsClientEmitError(pClient, -4);
		__xwsClientSetOpenFailure(pClient, XRT_NET_ERROR);
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return;
	}
	if ( xrtNetStreamSend(pStream, pHandshake, iHandshakeLen) != XRT_NET_OK ) {
		__xwsClientEmitError(pClient, -5);
		__xwsClientSetOpenFailure(pClient, XRT_NET_ERROR);
		XNET_FREE(pHandshake);
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return;
	}
	XNET_FREE(pHandshake);
	if ( !__xwsArmTimerSlot(pStream, pClient->tConfig.iHandshakeTimeoutMs,
		&pClient->iTimerLock, &pClient->pHandshakeTimer) ) {
		__xwsClientEmitError(pClient, -8);
		__xwsClientSetOpenFailure(pClient, XRT_NET_ERROR);
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
	}
}


// 内部函数：__xwsClientStreamOnRecv
static void __xwsClientStreamOnRecv(ptr pOwner, xnetstream* pStream, xnetchain* pChain)
{
	xwsclient* pClient = (xwsclient*)pOwner;
	if ( !pClient || !pStream || !pChain ) { return; }
	if ( __xwsAtomicLoad(&pClient->iOpen) == 0 ) {
		xcodecframe tFrame;
		xcodechttp1msg tMsg;
		xcodechttp1limits tLimits;
		xcodechttp1errorinfo tError;
		xcodecstatus iParse;
		__xwsResolveHandshakeLimits(pClient->tConfig.iHandshakeMaxBytes, &tLimits);
		iParse = xrtCodecHttp1ParseHeadEx(pChain, &tFrame, &tMsg, &tLimits, &tError);
		if ( iParse == XCODEC_STATUS_NEED_MORE ) { return; }
		__xwsCancelTimerSlot(&pClient->iTimerLock, &pClient->pHandshakeTimer);
		if ( iParse == XCODEC_STATUS_ERROR ||
			(pClient->tEvents.OnHandshakeResponse &&
				!pClient->tEvents.OnHandshakeResponse(pClient->pUserData, pClient, &tMsg)) ||
			!__xwsClientValidateHandshake(pClient, &tMsg) ) {
			xrtCodecHttp1MessageUnit(&tMsg);
			__xwsClientEmitError(pClient, -6);
			__xwsClientSetOpenFailure(pClient, XRT_NET_ERROR);
			xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
			return;
		}
		xrtCodecFrameConsume(pChain, &tFrame);
		xrtCodecHttp1MessageUnit(&tMsg);
		(void)__xwsAtomicCompareExchange(&pClient->iOpen, 1, 0);
		if ( !__xwsClientFinishOpening(pClient, __XWS_RUN_OPEN, XRT_NET_OK) ) {
			xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
			return;
		}
		if ( pClient->tEvents.OnOpen ) {
			pClient->tEvents.OnOpen(pClient->pUserData, pClient);
		}
	}
	__xwsClientConsumeFrames(pClient, pChain);
}


// 内部函数：__xwsClientStreamOnClose
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
	__xwsClientCancelTimers(pClient);
	__xwsClientEmitCloseOnce(pClient, iReason);
}


// 内部函数：__xwsClientStreamOnError
static void __xwsClientStreamOnError(ptr pOwner, xnetstream* pStream, int iSysErr)
{
	xwsclient* pClient = (xwsclient*)pOwner;
	(void)pStream;
	if ( pClient && __xwsIsBenignStreamError(iSysErr, pStream, &pClient->iClosePosted, &pClient->iCloseNotified) ) { return; }
	__xwsClientEmitError(pClient, iSysErr);
}


static void __xwsClientStreamOnDrain(ptr pOwner, xnetstream* pStream)
{
	xwsclient* pClient = (xwsclient*)pOwner;
	(void)pStream;
	if ( pClient && pClient->tEvents.OnDrain ) {
		pClient->tEvents.OnDrain(pClient->pUserData, pClient);
	}
}


static void __xwsClientStreamOnHighWater(ptr pOwner, xnetstream* pStream, uint32 iQueuedBytes)
{
	xwsclient* pClient = (xwsclient*)pOwner;
	size_t iPending = pStream ? xrtNetStreamPendingSend(pStream) : (size_t)iQueuedBytes;
	if ( pClient && pClient->tEvents.OnBackpressure ) {
		pClient->tEvents.OnBackpressure(pClient->pUserData, pClient, iPending);
	}
}


static void __xwsClientStreamOnLowWater(ptr pOwner, xnetstream* pStream, uint32 iQueuedBytes)
{
	xwsclient* pClient = (xwsclient*)pOwner;
	size_t iPending = pStream ? xrtNetStreamPendingSend(pStream) : (size_t)iQueuedBytes;
	if ( pClient && pClient->tEvents.OnWritable ) {
		pClient->tEvents.OnWritable(pClient->pUserData, pClient, iPending);
	}
}


// 内部函数：__xwsClientStreamEvents
static const xnetstreamevents* __xwsClientStreamEvents(void)
{
	static const xnetstreamevents tEvents = {
		__xwsClientStreamOnOpen,
		__xwsClientStreamOnRecv,
		__xwsClientStreamOnDrain,
		__xwsClientStreamOnClose,
		__xwsClientStreamOnError,
		__xwsClientStreamOnHighWater,
		__xwsClientStreamOnLowWater,
		NULL
	};
	return &tEvents;
}


/* ============================== Server helpers ============================== */

static void __xwsServerAddConn(xwsserver* pServer, xwsconn* pConn)
{
	if ( !pServer || !pConn ) { return; }
	__xwsLock(&pServer->iConnLock);
	pConn->pNext = pServer->pConnHead;
	pServer->pConnHead = pConn;
	__xwsUnlock(&pServer->iConnLock);
}


// 内部函数：__xwsServerRemoveConn
static void __xwsServerRemoveConn(xwsserver* pServer, xwsconn* pConn)
{
	xwsconn** ppNode;
	if ( !pServer || !pConn ) { return; }
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


// 内部函数：__xwsServerDetachAllConns
static xwsconn* __xwsServerDetachAllConns(xwsserver* pServer)
{
	xwsconn* pHead;
	if ( !pServer ) { return NULL; }
	__xwsLock(&pServer->iConnLock);
	pHead = pServer->pConnHead;
	pServer->pConnHead = NULL;
	__xwsUnlock(&pServer->iConnLock);
	return pHead;
}


static void __xwsConnFree(xwsconn* pConn)
{
	if ( !pConn ) { return; }
	__xwsConnCancelTimers(pConn);
	if ( pConn->sProtocol ) { XNET_FREE(pConn->sProtocol); pConn->sProtocol = NULL; }
	__xwsMessageReset(&pConn->pMsgBuf, &pConn->iMsgLen, &pConn->iMsgCap, &pConn->iMsgOpcode);
	XNET_FREE(pConn);
}


// 内部函数：__xwsConnCleanupTask
static void __xwsConnCleanupTask(xnetworker* pWorker, ptr pArg)
{
	xwsconn* pConn = (xwsconn*)pArg;
	(void)pWorker;
	if ( !pConn ) { return; }
	if ( pConn->pServer ) {
		__xwsServerRemoveConn(pConn->pServer, pConn);
		pConn->pServer = NULL;
	}
	if ( pConn->pStream ) {
		xrtNetStreamDestroy(pConn->pStream);
		pConn->pStream = NULL;
	}
	xrtWsConnRelease(pConn);
}


// 内部函数：__xwsConnPostCleanup
static void __xwsConnPostCleanup(xwsconn* pConn)
{
	if ( !pConn ) { return; }
	if ( __xwsAtomicCompareExchange(&pConn->iCleanupPosted, 1, 0) != 0 ) { return; }
	if ( pConn->pServer && pConn->pServer->pEngine && pConn->pStream && pConn->pStream->pWorker ) {
		if ( xrtNetEnginePost(pConn->pServer->pEngine, pConn->pStream->pWorker->iId, __xwsConnCleanupTask, pConn) == XRT_NET_OK ) {
			return;
		}
	}
	__xwsConnCleanupTask(NULL, pConn);
}


// 内部函数：__xwsServerEmitError
static void __xwsServerEmitError(xwsserver* pServer, xwsconn* pConn, int iSysErr)
{
	if ( pConn ) { pConn->iLastSysErr = iSysErr; }
	if ( pServer && pServer->tEvents.OnError ) {
		pServer->tEvents.OnError(pServer->pUserData, pServer, pConn, iSysErr);
	}
}


// 内部函数：__xwsServerEmitCloseOnce
static void __xwsServerEmitCloseOnce(xwsserver* pServer, xwsconn* pConn, xnet_result iReason)
{
	xwscloseinfo tInfo;
	if ( !pConn ) { return; }
	pConn->iCloseTransportResult = iReason;
	(void)__xwsAtomicCompareExchange(&pConn->iOpen, 0, 1);
	if ( __xwsAtomicCompareExchange(&pConn->iCloseNotified, 1, 0) != 0 ) { return; }
	__xwsFillCloseInfo(&tInfo, &pConn->iCloseFlags, iReason, pConn->iLocalCloseCode,
		pConn->iRemoteCloseCode, pConn->sRemoteCloseReason, pConn->iRemoteCloseReasonLen);
	if ( pServer && pServer->tEvents.OnCloseEx ) {
		pServer->tEvents.OnCloseEx(pServer->pUserData, pServer, pConn, &tInfo);
	}
	if ( pServer && pServer->tEvents.OnClose ) {
		pServer->tEvents.OnClose(pServer->pUserData, pServer, pConn, iReason);
	}
}


// 内部函数：__xwsSendHttpReply
static bool __xwsSendHttpReply(xnetstream* pStream, uint32 iStatusCode, const char* sBody,
	const char* sAccept, const char* sProtocol, const char* sExtensions, bool bClose)
{
	char* pBytes = NULL;
	size_t iLen = 0u;
	if ( !pStream ) { return false; }
	if ( !__xwsBuildHttpResponseBytes(iStatusCode, sBody, sAccept, sProtocol, sExtensions, &pBytes, &iLen) ) { return false; }
	if ( !__xwsStreamQueueOwnedBytesDirect(pStream, pBytes, iLen) ) {
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return false;
	}
	if ( bClose ) { xrtNetStreamClose(pStream, XNET_CLOSE_F_GRACEFUL); }
	return true;
}


static bool __xwsSendHttpReplyEx(xnetstream* pStream, uint32 iStatusCode, const char* sReason,
	const void* pBody, size_t iBodyLen, const char* sAccept, const char* sProtocol,
	const char* sExtensions, const xhttpheaders* pHeaders, bool bClose)
{
	char* pBytes = NULL;
	size_t iLen = 0u;
	if ( !pStream || !__xwsBuildHttpResponseBytesEx(iStatusCode, sReason, pBody, iBodyLen,
		sAccept, sProtocol, sExtensions, pHeaders, &pBytes, &iLen) ) { return false; }
	if ( !__xwsStreamQueueOwnedBytesDirect(pStream, pBytes, iLen) ) {
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return false;
	}
	if ( bClose ) { xrtNetStreamClose(pStream, XNET_CLOSE_F_GRACEFUL); }
	return true;
}


// 内部函数：__xwsServerValidateRequest
static uint32 __xwsServerValidateRequest(const xcodechttp1msg* pMsg, const char** psKey)
{
	const char* sUpgrade;
	const char* sKey;
	const char* sVersion;
	const char* sHost;
	xrturlview tAuthority;
	if ( psKey ) { *psKey = NULL; }
	if ( !pMsg || !psKey || (pMsg->iFlags & XCODEC_HTTP1_F_REQUEST) == 0u ) { return 400u; }
	if ( strcmp(pMsg->sMethod, "GET") != 0 || !__xwsStrEqNoCase(pMsg->sVersion, "HTTP/1.1") ) { return 400u; }
	if ( __xwsHeaderCount(pMsg, "Host", &sHost) != 1u || !sHost || !sHost[0] || strchr(sHost, ',') ||
		!xrtUrlParseAuthority(sHost, &tAuthority) || (tAuthority.iFlags & XRT_URL_F_HAS_USERINFO) != 0u ) { return 400u; }
	if ( __xwsHeaderCount(pMsg, "Upgrade", &sUpgrade) != 1u ||
		__xwsHeaderCount(pMsg, "Sec-WebSocket-Key", &sKey) != 1u ) { return 400u; }
	if ( __xwsHeaderCount(pMsg, "Sec-WebSocket-Version", &sVersion) != 1u ) { return 400u; }
	if ( __xwsHeaderCount(pMsg, "Content-Length", NULL) != 0u ||
		__xwsHeaderCount(pMsg, "Transfer-Encoding", NULL) != 0u ) { return 400u; }
	if ( !sUpgrade || !__xwsStrEqNoCase(sUpgrade, "websocket") ||
		!__xwsHeaderHasTokenNoCase(pMsg, "Connection", "Upgrade") ) { return 400u; }
	if ( !__xwsValidClientKey(sKey) ) { return 400u; }
	if ( strcmp(sVersion, "13") != 0 ) { return 426u; }
	*psKey = sKey;
	return 0u;
}


static uint32 __xwsServerConfiguredPolicyStatus(const xwsserver* pServer,
	const xcodechttp1msg* pMsg)
{
	const char* sExpectedPath = __xwsServerConfigPathValue(pServer ? &pServer->tConfig : NULL);
	const char* sExpectedOrigin = __xwsServerConfigOriginValue(pServer ? &pServer->tConfig : NULL);
	const char* sOrigin = NULL;
	uint32 iOriginCount;
	xrturlview tTarget;
	if ( !pServer || !pMsg || !xrtUrlParseTarget(pMsg->sTarget, &tTarget) ||
		(tTarget.iFlags & XRT_URL_F_HAS_FRAGMENT) != 0u ) { return 400u; }
	if ( sExpectedPath[0] &&
		(strlen(sExpectedPath) != tTarget.tPath.iLen ||
			memcmp(sExpectedPath, tTarget.tPath.sPtr, tTarget.tPath.iLen) != 0) ) { return 404u; }
	iOriginCount = __xwsHeaderCount(pMsg, "Origin", &sOrigin);
	if ( iOriginCount > 1u ) { return 400u; }
	if ( sExpectedOrigin[0] &&
		(iOriginCount != 1u || !sOrigin || strcmp(sExpectedOrigin, sOrigin) != 0) ) { return 403u; }
	return 0u;
}


static bool __xwsHandshakeHeadersValid(const xhttpheaders* pHeaders)
{
	size_t iCount;
	if ( !pHeaders ) { return true; }
	iCount = xrtHttpHeadersCount(pHeaders);
	for ( size_t i = 0u; i < iCount; ++i ) {
		xrtheaderpair tHeader;
		if ( !xrtHttpHeadersAt(pHeaders, i, &tHeader) ||
			__xwsManagedHandshakeHeaderN(tHeader.tName.sPtr, tHeader.tName.iLen) ) { return false; }
	}
	return true;
}


#define __XWS_HANDSHAKE_DECISION_ERROR  (-1)
#define __XWS_HANDSHAKE_DECISION_REJECT 0
#define __XWS_HANDSHAKE_DECISION_ACCEPT 1

static int __xwsServerPrepareHandshake(xwsserver* pServer, xwsconn* pConn,
	const xcodechttp1msg* pRequest, xwshandshakeresponse* pResponse)
{
	const char* sConfiguredProtocol;
	bool bPmd = false;
	uint32 iAction = XWS_HANDSHAKE_ACCEPT;
	if ( !pServer || !pConn || !pRequest || !pResponse ||
		!xrtWsHandshakeResponseInit(pResponse) ) { return __XWS_HANDSHAKE_DECISION_ERROR; }
	if ( !__xwsProtocolHeadersValid(pRequest, NULL) ) {
		pResponse->iStatusCode = 400u;
		pResponse->sReason = "Bad Protocol Header";
		return __XWS_HANDSHAKE_DECISION_REJECT;
	}
	sConfiguredProtocol = __xwsServerConfigProtocolValue(&pServer->tConfig);
	if ( sConfiguredProtocol[0] && __xwsProtocolHeadersContain(pRequest, sConfiguredProtocol) ) {
		pResponse->sProtocol = sConfiguredProtocol;
	} else if ( sConfiguredProtocol[0] && !pServer->tEvents.OnHandshake ) {
		pResponse->iStatusCode = 400u;
		pResponse->sReason = "Protocol Required";
		return __XWS_HANDSHAKE_DECISION_REJECT;
	}
	if ( (pServer->tConfig.iWebSocketFlags & XWS_F_PERMESSAGE_DEFLATE) != 0u ) {
		if ( !__xwsNegotiatePmd(pRequest, false, &bPmd) ) {
			pResponse->iStatusCode = 400u;
			pResponse->sReason = "Bad Extension Header";
			return __XWS_HANDSHAKE_DECISION_REJECT;
		}
		if ( bPmd ) { pResponse->iFlags |= XWS_HANDSHAKE_F_PERMESSAGE_DEFLATE; }
	}
	if ( pServer->tEvents.OnHandshake ) {
		iAction = pServer->tEvents.OnHandshake(pServer->pUserData, pServer, pConn, pRequest, pResponse);
	}
	if ( pResponse->iSize < XWS_HANDSHAKE_RESPONSE_V1_SIZE ||
		pResponse->iVersion != XWS_HANDSHAKE_RESPONSE_VERSION ||
		(pResponse->sReason && !__xwsValidFieldValue(pResponse->sReason)) ||
		(pResponse->iFlags & ~XWS_HANDSHAKE_F_PERMESSAGE_DEFLATE) != 0u ||
		!__xwsHandshakeHeadersValid(pResponse->pHeaders) ||
		(!pResponse->pBody && pResponse->iBodyLen != 0u) ) {
		return __XWS_HANDSHAKE_DECISION_ERROR;
	}
	if ( iAction == XWS_HANDSHAKE_ERROR ||
		(iAction != XWS_HANDSHAKE_ACCEPT && iAction != XWS_HANDSHAKE_REJECT) ) {
		return __XWS_HANDSHAKE_DECISION_ERROR;
	}
	if ( iAction == XWS_HANDSHAKE_REJECT ) {
		if ( pResponse->iStatusCode == 101u ) { pResponse->iStatusCode = 403u; }
		if ( pResponse->iStatusCode < 300u || pResponse->iStatusCode > 599u ) {
			return __XWS_HANDSHAKE_DECISION_ERROR;
		}
		return __XWS_HANDSHAKE_DECISION_REJECT;
	}
	if ( pResponse->iStatusCode != 101u || pResponse->iBodyLen != 0u ||
		((pResponse->iFlags & XWS_HANDSHAKE_F_PERMESSAGE_DEFLATE) != 0u && !bPmd) ||
		(pResponse->sProtocol && (!__xwsValidToken(pResponse->sProtocol) ||
			!__xwsProtocolHeadersContain(pRequest, pResponse->sProtocol))) ) {
		return __XWS_HANDSHAKE_DECISION_ERROR;
	}
	if ( !__xwsReplaceText(&pConn->sProtocol, pResponse->sProtocol) ) {
		return __XWS_HANDSHAKE_DECISION_ERROR;
	}
	pConn->bPerMessageDeflate =
		(pResponse->iFlags & XWS_HANDSHAKE_F_PERMESSAGE_DEFLATE) != 0u;
	pConn->pConnectionData = pResponse->pConnectionData;
	return __XWS_HANDSHAKE_DECISION_ACCEPT;
}


// 内部函数：__xwsServerConsumeFrames
static void __xwsServerConsumeFrames(xwsconn* pConn, xnetchain* pChain)
{
	xwsserver* pServer = pConn ? pConn->pServer : NULL;
	while ( pConn && pServer && pChain ) {
		xcodecframe tFrame;
		xcodecwsframeinfo tInfo;
		uint64 iFrameLimit = __xwsResolveFrameMaxBytes(pServer->tConfig.iMaxFrameBytes,
			pServer->tConfig.iRecvLimit);
		xcodecstatus iParse = xrtCodecWsParseFrameEx2(pChain, &tFrame, &tInfo, iFrameLimit,
			pConn->bPerMessageDeflate ? XCODEC_WS_RSV1 : 0u);
		char* pPayload = NULL;
		size_t iPayloadLen = 0u;
		bool bFin;
		bool bCompressed;
		int iDataRet;
		uint16 iCloseCode = 0u;
		// 数据不足，等待更多数据
		if ( iParse == XCODEC_STATUS_NEED_MORE ) { return; }
		// 帧解析错误，关闭连接
		if ( iParse == XCODEC_STATUS_ERROR ) {
			uint16 iCloseCode = __xwsFrameParseErrorCloseCode(pChain, iFrameLimit,
				pConn->bPerMessageDeflate ? XCODEC_WS_RSV1 : 0u);
			(void)__xwsConnStartClose(pConn, iCloseCode,
				iCloseCode == XWS_CLOSE_TOO_BIG ? "frame too large" : "invalid frame", true);
			return;
		}
		// 服务端要求客户端帧必须掩码
		if ( (tInfo.iFlags & XCODEC_WS_F_MASKED) == 0u ) {
			(void)__xwsConnStartClose(pConn, XWS_CLOSE_PROTOCOL, "mask required", true);
			return;
		}
		bCompressed = (tInfo.iFlags & XCODEC_WS_F_RSV1) != 0u;
		if ( bCompressed && ((tInfo.iFlags & XCODEC_WS_F_CONTROL) != 0u ||
			tInfo.iOpcode == XCODEC_WS_OPCODE_CONT) ) {
			(void)__xwsConnStartClose(pConn, XWS_CLOSE_PROTOCOL, "invalid rsv1", true);
			return;
		}
		// 控制帧负载不能超过 125 字节
		if ( (tInfo.iFlags & XCODEC_WS_F_CONTROL) != 0u && tInfo.iPayloadLen > 125u ) {
			(void)__xwsConnStartClose(pConn, XWS_CLOSE_PROTOCOL, "control too large", true);
			return;
		}
		// 提取并解掩码负载数据
		if ( !__xwsPeekPayloadCopy(pChain, &tFrame, &tInfo, &pPayload, &iPayloadLen) ) {
			__xwsServerEmitError(pServer, pConn, -22);
			xrtNetStreamClose(pConn->pStream, XNET_CLOSE_F_ABORT);
			return;
		}
		xrtCodecFrameConsume(pChain, &tFrame);
		bFin = (tInfo.iFlags & XCODEC_WS_F_FIN) != 0u;
		// 根据操作码分发处理
		switch ( tInfo.iOpcode ) {
			case XCODEC_WS_OPCODE_TEXT:
			case XCODEC_WS_OPCODE_BINARY:
			case XCODEC_WS_OPCODE_CONT:
				// 数据帧交给消息重组逻辑处理
				iDataRet = __xwsServerConsumeDataFrameEx(pConn, tInfo.iOpcode, bFin, bCompressed, pPayload, iPayloadLen);
				if ( iDataRet == __XWS_APPEND_OK ) { break; }
				if ( iDataRet == __XWS_APPEND_TOO_BIG ) {
					(void)__xwsConnStartClose(pConn, XWS_CLOSE_TOO_BIG, "message too large", true);
				} else if ( iDataRet == __XWS_APPEND_PROTOCOL ) {
					(void)__xwsConnStartClose(pConn, XWS_CLOSE_PROTOCOL, "bad fragment sequence", true);
				} else if ( iDataRet == __XWS_APPEND_INVALID_DATA ) {
					(void)__xwsConnStartClose(pConn, XWS_CLOSE_INVALID_DATA, "invalid message data", true);
				} else {
					__xwsServerEmitError(pServer, pConn, -23);
					xrtNetStreamClose(pConn->pStream, XNET_CLOSE_F_ABORT);
				}
				XNET_FREE(pPayload);
				return;
				break;
			case XCODEC_WS_OPCODE_PING:
				// 自动回复 Pong（服务端帧不掩码）
				(void)__xwsStreamQueueFrameDirect(pConn->pStream, false, XCODEC_WS_OPCODE_PONG, pPayload, iPayloadLen);
				if ( pServer->tEvents.OnPing ) { pServer->tEvents.OnPing(pServer->pUserData, pServer, pConn, pPayload, iPayloadLen); }
				break;
			case XCODEC_WS_OPCODE_PONG:
				if ( pServer->tEvents.OnPong ) { pServer->tEvents.OnPong(pServer->pUserData, pServer, pConn, pPayload, iPayloadLen); }
				break;
			case XCODEC_WS_OPCODE_CLOSE:
				// 提取关闭码并通知关闭
				{
					uint16 iCloseError = __xwsValidateClosePayload(pPayload, iPayloadLen, &iCloseCode);
					if ( iCloseError != 0u ) {
						(void)__xwsConnStartClose(pConn, iCloseError,
							iCloseError == XWS_CLOSE_INVALID_DATA ? "invalid close reason" : "invalid close payload", true);
						XNET_FREE(pPayload);
						return;
					}
				}
				#if defined(XNET_DEBUG_CLOSE_DIAG)
					fprintf(stderr, "[CLOSE_DIAG][WS-SERVER] recv close stream=%p code=%u posted=%ld open=%ld len=%zu\n",
						(void*)pConn->pStream,
						(unsigned)iCloseCode,
						(long)__xwsAtomicLoad(&pConn->iClosePosted),
						(long)__xwsAtomicLoad(&pConn->iOpen),
						iPayloadLen);
				#endif
				{
					bool bRemoteInitiated = __xwsAtomicLoad(&pConn->iClosePosted) == 0;
					__xwsRecordRemoteClose(&pConn->iCloseFlags, &pConn->iRemoteCloseCode,
						&pConn->iRemoteCloseReasonLen, pConn->sRemoteCloseReason,
						pPayload, iPayloadLen, bRemoteInitiated);
				}
				// 首次收到关闭帧则回复关闭帧，否则优雅关闭流
				if ( __xwsAtomicLoad(&pConn->iClosePosted) == 0 ) {
					if ( __xwsConnStartClose(pConn, iCloseCode, NULL, true) != XRT_NET_OK && pConn->pStream ) {
						xrtNetStreamClose(pConn->pStream, XNET_CLOSE_F_ABORT);
					}
				} else if ( pConn->pStream ) {
					xrtNetStreamClose(pConn->pStream, XNET_CLOSE_F_GRACEFUL);
				}
				XNET_FREE(pPayload);
				return;
			default:
				(void)__xwsConnStartClose(pConn, XWS_CLOSE_PROTOCOL, "bad opcode", true);
				XNET_FREE(pPayload);
				return;
		}
		XNET_FREE(pPayload);
	}
}


// 内部函数：__xwsListenerOnAccept
static bool __xwsListenerOnAccept(ptr pOwner, xnetlistener* pListener, xnetstream* pStream)
{
	xwsserver* pServer = (xwsserver*)pOwner;
	xwsconn* pConn;
	(void)pListener;
	if ( !pServer || !pStream ) { return false; }
	__xwsConfigureStreamControlBudget(pStream);
	pConn = (xwsconn*)xrtNetStreamGetUserData(pStream);
	if ( !pConn ) {
		pConn = (xwsconn*)XNET_ALLOC(sizeof(xwsconn));
		if ( !pConn ) { return false; }
		memset(pConn, 0, sizeof(xwsconn));
		pConn->iRefCount = 1;
		xrtNetStreamSetUserData(pStream, pConn);
	}
	pConn->pServer = pServer;
	pConn->pStream = pStream;
	if ( !__xwsArmTimerSlot(pStream, pServer->tConfig.iHandshakeTimeoutMs,
		&pConn->iTimerLock, &pConn->pHandshakeTimer) ) { return false; }
	__xwsServerAddConn(pServer, pConn);
	return true;
}


// 内部函数：__xwsServerStreamOnOpen
static void __xwsServerStreamOnOpen(ptr pOwner, xnetstream* pStream)
{
	(void)pOwner;
	(void)pStream;
}


// 内部函数：__xwsServerStreamOnRecv
static void __xwsServerStreamOnRecv(ptr pOwner, xnetstream* pStream, xnetchain* pChain)
{
	xwsconn* pConn = (xwsconn*)pOwner;
	xwsserver* pServer = pConn ? pConn->pServer : NULL;
	if ( !pConn || !pServer || !pStream || !pChain ) { return; }
	// 尚未完成握手时，先处理 HTTP 升级请求
	if ( __xwsAtomicLoad(&pConn->iOpen) == 0 ) {
		xcodecframe tFrame;
		xcodechttp1msg tMsg;
		xcodechttp1limits tLimits;
		xcodechttp1errorinfo tError;
		xcodecstatus iParse;
		const char* sKey = NULL;
		char sAccept[64];
		xwshandshakeresponse tResponse;
		uint32 iStatus;
		int iDecision;
		memset(&tResponse, 0, sizeof(tResponse));
		__xwsResolveHandshakeLimits(pServer->tConfig.iHandshakeMaxBytes, &tLimits);
		iParse = xrtCodecHttp1ParseHeadEx(pChain, &tFrame, &tMsg, &tLimits, &tError);
		if ( iParse == XCODEC_STATUS_NEED_MORE ) { return; }
		__xwsCancelTimerSlot(&pConn->iTimerLock, &pConn->pHandshakeTimer);
		iStatus = iParse == XCODEC_STATUS_ERROR ? __xwsHandshakeErrorStatus(&tError) :
			__xwsServerValidateRequest(&tMsg, &sKey);
		if ( iStatus == 0u ) { iStatus = __xwsServerConfiguredPolicyStatus(pServer, &tMsg); }
		if ( iStatus != 0u ) {
			xrtCodecHttp1MessageUnit(&tMsg);
			__xwsServerEmitError(pServer, pConn, -31);
			(void)__xwsSendHttpReply(pStream, iStatus,
				iStatus == 426u ? "Unsupported WebSocket Version" : __xwsHttpStatusText(iStatus), NULL, NULL, NULL, true);
			return;
		}
		// 计算服务端 Accept 值
		if ( !__xwsComputeAccept(sKey, sAccept, sizeof(sAccept)) ) {
			xrtCodecHttp1MessageUnit(&tMsg);
			__xwsServerEmitError(pServer, pConn, -32);
			xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
			return;
		}
		iDecision = __xwsServerPrepareHandshake(pServer, pConn, &tMsg, &tResponse);
		if ( iDecision == __XWS_HANDSHAKE_DECISION_ERROR ) {
			if ( tResponse.iVersion == XWS_HANDSHAKE_RESPONSE_VERSION ) {
				xrtWsHandshakeResponseUnit(&tResponse);
			}
			xrtCodecHttp1MessageUnit(&tMsg);
			__xwsServerEmitError(pServer, pConn, -34);
			(void)__xwsSendHttpReply(pStream, 500u, "Internal Server Error", NULL, NULL, NULL, true);
			return;
		}
		if ( iDecision == __XWS_HANDSHAKE_DECISION_REJECT ) {
			const void* pBody = tResponse.pBody ? tResponse.pBody :
				(const void*)__xwsHttpStatusText(tResponse.iStatusCode);
			size_t iBodyLen = tResponse.pBody ? tResponse.iBodyLen : strlen((const char*)pBody);
			bool bSent = __xwsSendHttpReplyEx(pStream, tResponse.iStatusCode, tResponse.sReason,
				pBody, iBodyLen, NULL, NULL, NULL, tResponse.pHeaders, true);
			xrtWsHandshakeResponseUnit(&tResponse);
			xrtCodecHttp1MessageUnit(&tMsg);
			if ( !bSent ) { __xwsServerEmitError(pServer, pConn, -34); }
			return;
		}
		if ( !__xwsSendHttpReplyEx(pStream, 101u, tResponse.sReason, NULL, 0u,
			sAccept, pConn->sProtocol,
			pConn->bPerMessageDeflate ? __XWS_PMD_VALUE : NULL,
			tResponse.pHeaders, false) ) {
			xrtWsHandshakeResponseUnit(&tResponse);
			xrtCodecHttp1MessageUnit(&tMsg);
			__xwsServerEmitError(pServer, pConn, -33);
			return;
		}
		xrtWsHandshakeResponseUnit(&tResponse);
		// 消费 HTTP 头部数据并标记连接已打开
		xrtCodecFrameConsume(pChain, &tFrame);
		xrtCodecHttp1MessageUnit(&tMsg);
		(void)__xwsAtomicCompareExchange(&pConn->iOpen, 1, 0);
		if ( pServer->tEvents.OnOpen ) {
			pServer->tEvents.OnOpen(pServer->pUserData, pServer, pConn);
		}
	}
	// 处理 WebSocket 数据帧
	__xwsServerConsumeFrames(pConn, pChain);
}


// 内部函数：__xwsServerStreamOnClose
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
	__xwsConnCancelTimers(pConn);
	__xwsServerEmitCloseOnce(pServer, pConn, iReason);
	__xwsConnPostCleanup(pConn);
}


// 内部函数：__xwsServerStreamOnError
static void __xwsServerStreamOnError(ptr pOwner, xnetstream* pStream, int iSysErr)
{
	xwsconn* pConn = (xwsconn*)pOwner;
	xwsserver* pServer = pConn ? pConn->pServer : NULL;
	if ( pConn && __xwsIsBenignStreamError(iSysErr, pStream, &pConn->iClosePosted, &pConn->iCloseNotified) ) { return; }
	__xwsServerEmitError(pServer, pConn, iSysErr);
}


static void __xwsServerStreamOnDrain(ptr pOwner, xnetstream* pStream)
{
	xwsconn* pConn = (xwsconn*)pOwner;
	xwsserver* pServer = pConn ? pConn->pServer : NULL;
	(void)pStream;
	if ( pServer && pServer->tEvents.OnDrain ) {
		pServer->tEvents.OnDrain(pServer->pUserData, pServer, pConn);
	}
}


static void __xwsServerStreamOnHighWater(ptr pOwner, xnetstream* pStream, uint32 iQueuedBytes)
{
	xwsconn* pConn = (xwsconn*)pOwner;
	xwsserver* pServer = pConn ? pConn->pServer : NULL;
	size_t iPending = pStream ? xrtNetStreamPendingSend(pStream) : (size_t)iQueuedBytes;
	if ( pServer && pServer->tEvents.OnBackpressure ) {
		pServer->tEvents.OnBackpressure(pServer->pUserData, pServer, pConn, iPending);
	}
}


static void __xwsServerStreamOnLowWater(ptr pOwner, xnetstream* pStream, uint32 iQueuedBytes)
{
	xwsconn* pConn = (xwsconn*)pOwner;
	xwsserver* pServer = pConn ? pConn->pServer : NULL;
	size_t iPending = pStream ? xrtNetStreamPendingSend(pStream) : (size_t)iQueuedBytes;
	if ( pServer && pServer->tEvents.OnWritable ) {
		pServer->tEvents.OnWritable(pServer->pUserData, pServer, pConn, iPending);
	}
}


// 内部函数：__xwsAcceptReady
static void __xwsAcceptReady(xnetlistener* pListener, xnet_result iStatus, xnetstream* pStream, ptr pCtx)
{
	xwsserver* pServer = (xwsserver*)pCtx;
	(void)pListener;
	(void)pStream;
	if ( !pServer || __xwsAtomicLoad(&pServer->bRunning) == 0 ) { return; }
	if ( pServer->pListener && pServer->pListener->bRunning ) {
		(void)__xnetListenerRegisterSyncAcceptWait(pServer->pListener, __xwsAcceptReady, NULL, pServer);
	}
	if ( iStatus != XRT_NET_OK && pServer->tEvents.OnError ) {
		pServer->tEvents.OnError(pServer->pUserData, pServer, NULL, -1);
	}
}


// 内部函数：__xwsArmAcceptTask
static void __xwsArmAcceptTask(xnetworker* pWorker, ptr pArg)
{
	xwsserver* pServer = (xwsserver*)pArg;
	(void)pWorker;
	if ( !pServer || !pServer->pListener || __xwsAtomicLoad(&pServer->bRunning) == 0 ) { return; }
	(void)__xnetListenerRegisterSyncAcceptWait(pServer->pListener, __xwsAcceptReady, NULL, pServer);
}


// 内部函数：__xwsListenerEvents
static const xnetlistenerevents* __xwsListenerEvents(void)
{
	static const xnetlistenerevents tEvents = {
		__xwsListenerOnAccept,
		NULL
	};
	return &tEvents;
}


// 内部函数：__xwsServerStreamEvents
static const xnetstreamevents* __xwsServerStreamEvents(void)
{
	static const xnetstreamevents tEvents = {
		__xwsServerStreamOnOpen,
		__xwsServerStreamOnRecv,
		__xwsServerStreamOnDrain,
		__xwsServerStreamOnClose,
		__xwsServerStreamOnError,
		__xwsServerStreamOnHighWater,
		__xwsServerStreamOnLowWater,
		NULL
	};
	return &tEvents;
}


#if !defined(XRT_NO_XHTTPD)
static bool __xwsHttpdRequestView(const xhttpdrequest* pRequest, xcodechttp1msg* pMessage,
	xcodechttp1header** ppAllocated)
{
	const xhttpdheader* pHeaders;
	xcodechttp1header* pOutHeaders;
	if ( ppAllocated ) { *ppAllocated = NULL; }
	if ( !pRequest || !pMessage || !ppAllocated ) { return false; }
	memset(pMessage, 0, sizeof(*pMessage));
	pMessage->iFlags = XCODEC_HTTP1_F_REQUEST;
	pMessage->iHeaderCount = pRequest->iHeaderCount;
	pMessage->iHeaderCap = pRequest->iHeaderCount;
	pMessage->iContentLength = pRequest->iContentLength;
	pMessage->sMethod = pRequest->sMethod ? pRequest->sMethod : "";
	pMessage->sTarget = pRequest->sTarget ? pRequest->sTarget : "";
	pMessage->sVersion = pRequest->sVersion ? pRequest->sVersion : "";
	pMessage->iMethodLen = strlen(pMessage->sMethod);
	pMessage->iTargetLen = strlen(pMessage->sTarget);
	pMessage->iVersionLen = strlen(pMessage->sVersion);
	if ( pRequest->iHeaderCount <= XCODEC_HTTP1_MAX_HEADERS ) {
		pOutHeaders = pMessage->arrHeaders;
	} else {
		#if SIZE_MAX < UINT32_MAX
			if ( pRequest->iHeaderCount > SIZE_MAX / sizeof(*pOutHeaders) ) { return false; }
		#endif
		pOutHeaders = (xcodechttp1header*)XNET_ALLOC(
			(size_t)pRequest->iHeaderCount * sizeof(*pOutHeaders));
		if ( !pOutHeaders ) { return false; }
		*ppAllocated = pOutHeaders;
	}
	pMessage->pHeaders = pOutHeaders;
	pHeaders = pRequest->pHeaders ? pRequest->pHeaders : pRequest->arrHeaders;
	for ( uint32 i = 0u; i < pRequest->iHeaderCount; ++i ) {
		pOutHeaders[i].sName = pHeaders[i].sName ? pHeaders[i].sName : "";
		pOutHeaders[i].sValue = pHeaders[i].sValue ? pHeaders[i].sValue : "";
		pOutHeaders[i].iNameLen = strlen(pOutHeaders[i].sName);
		pOutHeaders[i].iValueLen = strlen(pOutHeaders[i].sValue);
	}
	return true;
}


static bool __xwsHttpdAddResponseHeaders(xhttpdresponse* pResponse,
	const xhttpheaders* pHeaders)
{
	size_t iCount;
	if ( !pHeaders ) { return true; }
	iCount = xrtHttpHeadersCount(pHeaders);
	for ( size_t i = 0u; i < iCount; ++i ) {
		xrtheaderpair tHeader;
		if ( !xrtHttpHeadersAt(pHeaders, i, &tHeader) ||
			!xrtHttpdResponseAddHeader(pResponse, tHeader.tName.sPtr, tHeader.tValue.sPtr) ) {
			return false;
		}
	}
	return true;
}


static bool __xwsHttpdBuildHandshakeResponse(xhttpdresponse* pOutput,
	const xwshandshakeresponse* pHandshake, bool bAccepted, const char* sAccept,
	const char* sProtocol, bool bPerMessageDeflate)
{
	const void* pBody;
	size_t iBodyLen;
	if ( !pOutput || !pHandshake ) { return false; }
	xrtHttpdResponseInit(pOutput);
	if ( !xrtHttpdResponseSetStatusEx(pOutput, pHandshake->iStatusCode, pHandshake->sReason) ||
		!__xwsHttpdAddResponseHeaders(pOutput, pHandshake->pHeaders) ) { return false; }
	if ( bAccepted ) {
		return xrtHttpdResponseSetHeader(pOutput, "Upgrade", "websocket") &&
			xrtHttpdResponseSetHeader(pOutput, "Connection", "Upgrade") &&
			xrtHttpdResponseSetHeader(pOutput, "Sec-WebSocket-Accept", sAccept) &&
			(!sProtocol || !sProtocol[0] ||
				xrtHttpdResponseSetHeader(pOutput, "Sec-WebSocket-Protocol", sProtocol)) &&
			(!bPerMessageDeflate ||
				xrtHttpdResponseSetHeader(pOutput, "Sec-WebSocket-Extensions", __XWS_PMD_VALUE));
	}
	pBody = pHandshake->pBody ? pHandshake->pBody :
		(const void*)__xwsHttpStatusText(pHandshake->iStatusCode);
	iBodyLen = pHandshake->pBody ? pHandshake->iBodyLen : strlen((const char*)pBody);
	return xrtHttpdResponseSetHeader(pOutput, "Connection", "close") &&
		(pHandshake->iStatusCode != 426u ||
			xrtHttpdResponseSetHeader(pOutput, "Sec-WebSocket-Version", "13")) &&
		(xrtHttpdResponseHeader(pOutput, "Content-Type") != NULL ||
			xrtHttpdResponseSetHeader(pOutput, "Content-Type", "text/plain")) &&
		xrtHttpdResponseSetBodyCopy(pOutput, pBody, iBodyLen, NULL);
}


static xnet_result __xwsHttpdRespondHandshake(xhttpdconn* pHttpdConn,
	const xwshandshakeresponse* pHandshake)
{
	xhttpdresponse tOutput;
	xnet_result iResult;
	memset(&tOutput, 0, sizeof(tOutput));
	if ( !__xwsHttpdBuildHandshakeResponse(&tOutput, pHandshake, false, NULL, NULL, false) ) {
		xrtHttpdResponseUnit(&tOutput);
		return XRT_NET_ERROR;
	}
	iResult = xrtHttpdConnRespond(pHttpdConn, &tOutput);
	xrtHttpdResponseUnit(&tOutput);
	return iResult;
}
#endif

/* ============================== Public API ============================== */

static bool __xwsValidSendBudget(uint32 iHighWater, uint32 iLowWater, uint32 iMaxQueuedBytes)
{
	uint32 iDataBudget = iMaxQueuedBytes;
	if ( iMaxQueuedBytes > 0u && iMaxQueuedBytes < __XWS_MIN_SEND_BUDGET_BYTES ) { return false; }
	if ( iDataBudget > 0u ) { iDataBudget -= __XWS_CONTROL_SEND_RESERVE_BYTES; }
	if ( iHighWater > 0u && iDataBudget > 0u && iHighWater > iDataBudget ) { return false; }
	if ( iLowWater > 0u && iHighWater > 0u && iLowWater > iHighWater ) { return false; }
	if ( iLowWater > 0u && iDataBudget > 0u && iLowWater > iDataBudget ) { return false; }
	return true;
}


static bool __xwsValidReceiveLimits(uint32 iMessageLimit, uint32 iFrameLimit,
	uint32 iHandshakeLimit)
{
	uint64 iResolvedFrame = __xwsResolveFrameMaxBytes(iFrameLimit, iMessageLimit);
	uint32 iResolvedHandshake = __xwsResolveHandshakeMaxBytes(iHandshakeLimit);
	if ( iResolvedHandshake < __XWS_MIN_HANDSHAKE_BYTES ) { return false; }
	if ( iResolvedFrame != UINT64_MAX && iResolvedFrame > (uint64)UINT32_MAX - 14u ) { return false; }
	return true;
}


static bool __xwsValidFeatureConfig(uint32 iFlags, int32 iCompressionLevel)
{
	if ( (iFlags & ~XWS_F_PERMESSAGE_DEFLATE) != 0u || iCompressionLevel < -1 || iCompressionLevel > 10 ) {
		return false;
	}
	#if XWS_HAS_PERMESSAGE_DEFLATE == 0
		if ( (iFlags & XWS_F_PERMESSAGE_DEFLATE) != 0u ) { return false; }
	#endif
	return true;
}


static bool __xwsManagedHandshakeHeader(const char* sName)
{
	return sName && __xwsManagedHandshakeHeaderN(sName, strlen(sName));
}


static bool __xwsClientEventsCopy(xwsclientevents* pDst, const xwsclientevents* pSrc)
{
	size_t iCopy;
	if ( !pDst ) { return false; }
	xrtWsClientEventsInit(pDst);
	if ( !pSrc ) { return true; }
	if ( pSrc->iSize == 0u && pSrc->iVersion == 0u ) {
		memcpy(pDst, pSrc, sizeof(*pDst));
		pDst->iSize = XWS_CLIENT_EVENTS_V2_SIZE;
		pDst->iVersion = XWS_CLIENT_EVENTS_VERSION;
		return true;
	}
	if ( (pSrc->iVersion == 1u && pSrc->iSize < XWS_CLIENT_EVENTS_V1_SIZE) ||
		(pSrc->iVersion == 2u && pSrc->iSize < XWS_CLIENT_EVENTS_V2_SIZE) ||
		pSrc->iVersion == 0u || pSrc->iVersion > XWS_CLIENT_EVENTS_VERSION ) { return false; }
	iCopy = pSrc->iSize < sizeof(*pDst) ? (size_t)pSrc->iSize : sizeof(*pDst);
	memcpy(pDst, pSrc, iCopy);
	pDst->iSize = XWS_CLIENT_EVENTS_V2_SIZE;
	pDst->iVersion = XWS_CLIENT_EVENTS_VERSION;
	return true;
}


static bool __xwsServerEventsCopy(xwsserverevents* pDst, const xwsserverevents* pSrc)
{
	size_t iCopy;
	if ( !pDst ) { return false; }
	xrtWsServerEventsInit(pDst);
	if ( !pSrc ) { return true; }
	if ( pSrc->iSize == 0u && pSrc->iVersion == 0u ) {
		memcpy(pDst, pSrc, sizeof(*pDst));
		pDst->iSize = XWS_SERVER_EVENTS_V2_SIZE;
		pDst->iVersion = XWS_SERVER_EVENTS_VERSION;
		return true;
	}
	if ( (pSrc->iVersion == 1u && pSrc->iSize < XWS_SERVER_EVENTS_V1_SIZE) ||
		(pSrc->iVersion == 2u && pSrc->iSize < XWS_SERVER_EVENTS_V2_SIZE) ||
		pSrc->iVersion == 0u || pSrc->iVersion > XWS_SERVER_EVENTS_VERSION ) { return false; }
	iCopy = pSrc->iSize < sizeof(*pDst) ? (size_t)pSrc->iSize : sizeof(*pDst);
	memcpy(pDst, pSrc, iCopy);
	pDst->iSize = XWS_SERVER_EVENTS_V2_SIZE;
	pDst->iVersion = XWS_SERVER_EVENTS_VERSION;
	return true;
}

static bool __xwsClientConfigClone(xwsclientconfig* pDst, const xwsclientconfig* pSrc)
{
	if ( !pDst || !pSrc ) { return false; }
	xrtWsClientConfigInit(pDst);
	if ( (pSrc->iVersion == 1u && pSrc->iSize < XWS_CLIENT_CONFIG_V1_SIZE) ||
		(pSrc->iVersion == 2u && pSrc->iSize < XWS_CLIENT_CONFIG_V2_SIZE) ||
		(pSrc->iVersion == 3u && pSrc->iSize < XWS_CLIENT_CONFIG_V3_SIZE) ||
		pSrc->iVersion == 0u || pSrc->iVersion > XWS_CLIENT_CONFIG_VERSION ) { return false; }
	pDst->iConnectTimeoutMs = pSrc->iConnectTimeoutMs;
	pDst->iRecvLimit = pSrc->iRecvLimit;
	pDst->iHighWater = pSrc->iHighWater;
	pDst->iLowWater = pSrc->iLowWater;
	pDst->iMaxQueuedBytes = pSrc->iMaxQueuedBytes;
	pDst->iWebSocketFlags = pSrc->iWebSocketFlags;
	pDst->iCompressMinBytes = pSrc->iCompressMinBytes;
	pDst->iCompressionLevel = pSrc->iCompressionLevel;
	pDst->bVerifyPeer = pSrc->bVerifyPeer;
	pDst->pProxy = pSrc->pProxy;
	if ( !__xwsSetConfigText(pDst->sURL, sizeof(pDst->sURL), &pDst->pURLStorage, __xwsClientConfigURLValue(pSrc)) ||
		!__xwsSetConfigText(pDst->sOrigin, sizeof(pDst->sOrigin), &pDst->pOriginStorage, __xwsClientConfigOriginValue(pSrc)) ||
		!__xwsSetConfigText(pDst->sProtocol, sizeof(pDst->sProtocol), &pDst->pProtocolStorage, __xwsClientConfigProtocolValue(pSrc)) ) {
		xrtWsClientConfigUnit(pDst);
		return false;
	}
	if ( pSrc->iSize >= XWS_CLIENT_CONFIG_V2_SIZE && pSrc->pRequestHeadersStorage ) {
		pDst->pRequestHeadersStorage = xrtHttpHeadersClone(pSrc->pRequestHeadersStorage);
		if ( !pDst->pRequestHeadersStorage ) {
			xrtWsClientConfigUnit(pDst);
			return false;
		}
	}
	if ( pSrc->iSize >= XWS_CLIENT_CONFIG_V3_SIZE ) {
		pDst->iMaxFrameBytes = pSrc->iMaxFrameBytes;
		pDst->iHandshakeMaxBytes = pSrc->iHandshakeMaxBytes;
		pDst->iHandshakeTimeoutMs = pSrc->iHandshakeTimeoutMs;
		pDst->iCloseTimeoutMs = pSrc->iCloseTimeoutMs;
	}
	return true;
}


static bool __xwsServerConfigClone(xwsserverconfig* pDst, const xwsserverconfig* pSrc)
{
	if ( !pDst || !pSrc ) { return false; }
	xrtWsServerConfigInit(pDst);
	if ( (pSrc->iVersion == 1u && pSrc->iSize < XWS_SERVER_CONFIG_V1_SIZE) ||
		(pSrc->iVersion == 2u && pSrc->iSize < XWS_SERVER_CONFIG_V2_SIZE) ||
		(pSrc->iVersion == 3u && pSrc->iSize < XWS_SERVER_CONFIG_V3_SIZE) ||
		pSrc->iVersion == 0u || pSrc->iVersion > XWS_SERVER_CONFIG_VERSION ) { return false; }
	pDst->tBindAddr = pSrc->tBindAddr;
	pDst->iFlags = pSrc->iFlags;
	pDst->iBacklog = pSrc->iBacklog;
	pDst->iRecvLimit = pSrc->iRecvLimit;
	pDst->iHighWater = pSrc->iHighWater;
	pDst->iLowWater = pSrc->iLowWater;
	pDst->iMaxQueuedBytes = pSrc->iMaxQueuedBytes;
	pDst->iWebSocketFlags = pSrc->iWebSocketFlags;
	pDst->iCompressMinBytes = pSrc->iCompressMinBytes;
	pDst->iCompressionLevel = pSrc->iCompressionLevel;
	pDst->pTlsConfig = pSrc->pTlsConfig;
	if ( !__xwsSetConfigText(pDst->sProtocol, sizeof(pDst->sProtocol), &pDst->pProtocolStorage, __xwsServerConfigProtocolValue(pSrc)) ) {
		xrtWsServerConfigUnit(pDst);
		return false;
	}
	if ( pSrc->iSize >= XWS_SERVER_CONFIG_V2_SIZE ) {
		if ( pSrc->pPathStorage && !(pDst->pPathStorage = __xwsDupText(pSrc->pPathStorage)) ) {
			xrtWsServerConfigUnit(pDst);
			return false;
		}
		if ( pSrc->pOriginStorage && !(pDst->pOriginStorage = __xwsDupText(pSrc->pOriginStorage)) ) {
			xrtWsServerConfigUnit(pDst);
			return false;
		}
	}
	if ( pSrc->iSize >= XWS_SERVER_CONFIG_V3_SIZE ) {
		pDst->iMaxFrameBytes = pSrc->iMaxFrameBytes;
		pDst->iHandshakeMaxBytes = pSrc->iHandshakeMaxBytes;
		pDst->iHandshakeTimeoutMs = pSrc->iHandshakeTimeoutMs;
		pDst->iCloseTimeoutMs = pSrc->iCloseTimeoutMs;
	}
	return true;
}


XXAPI void xrtWsCloseInfoInit(xwscloseinfo* pInfo)
{
	if ( !pInfo ) { return; }
	memset(pInfo, 0, sizeof(*pInfo));
	pInfo->iSize = XWS_CLOSE_INFO_V1_SIZE;
	pInfo->iVersion = XWS_CLOSE_INFO_VERSION;
	pInfo->sRemoteReason = "";
}


XXAPI bool xrtWsHandshakeResponseInit(xwshandshakeresponse* pResponse)
{
	if ( !pResponse ) { return false; }
	memset(pResponse, 0, sizeof(*pResponse));
	pResponse->iSize = XWS_HANDSHAKE_RESPONSE_V1_SIZE;
	pResponse->iVersion = XWS_HANDSHAKE_RESPONSE_VERSION;
	pResponse->iStatusCode = 101u;
	pResponse->pHeaders = xrtHttpHeadersCreate();
	return pResponse->pHeaders != NULL;
}


XXAPI void xrtWsHandshakeResponseUnit(xwshandshakeresponse* pResponse)
{
	if ( !pResponse ) { return; }
	if ( pResponse->pHeaders ) { xrtHttpHeadersDestroy(pResponse->pHeaders); }
	memset(pResponse, 0, sizeof(*pResponse));
}

XXAPI void xrtWsClientConfigInit(xwsclientconfig* pCfg)
{
	if ( !pCfg ) { return; }
	memset(pCfg, 0, sizeof(xwsclientconfig));
	pCfg->iSize = XWS_CLIENT_CONFIG_V3_SIZE;
	pCfg->iVersion = XWS_CLIENT_CONFIG_VERSION;
	pCfg->iConnectTimeoutMs = 5000u;
	pCfg->iRecvLimit = 1024u * 1024u;
	pCfg->iHighWater = 262144u;
	pCfg->iLowWater = 65536u;
	pCfg->iMaxQueuedBytes = 1048576u;
	pCfg->iCompressMinBytes = 256u;
	pCfg->iCompressionLevel = 6;
	pCfg->iMaxFrameBytes = 1024u * 1024u;
	pCfg->iHandshakeMaxBytes = XCODEC_HTTP1_DEFAULT_HEADER_BYTES;
	pCfg->iHandshakeTimeoutMs = 10000u;
	pCfg->iCloseTimeoutMs = 5000u;
	pCfg->bVerifyPeer = true;
}


XXAPI void xrtWsClientEventsInit(xwsclientevents* pEvents)
{
	if ( !pEvents ) { return; }
	memset(pEvents, 0, sizeof(*pEvents));
	pEvents->iSize = XWS_CLIENT_EVENTS_V2_SIZE;
	pEvents->iVersion = XWS_CLIENT_EVENTS_VERSION;
}


XXAPI void xrtWsClientConfigUnit(xwsclientconfig* pCfg)
{
	if ( !pCfg ) { return; }
	if ( pCfg->pURLStorage ) { XNET_FREE(pCfg->pURLStorage); pCfg->pURLStorage = NULL; }
	if ( pCfg->pOriginStorage ) { XNET_FREE(pCfg->pOriginStorage); pCfg->pOriginStorage = NULL; }
	if ( pCfg->pProtocolStorage ) { XNET_FREE(pCfg->pProtocolStorage); pCfg->pProtocolStorage = NULL; }
	if ( pCfg->iSize >= XWS_CLIENT_CONFIG_V2_SIZE && pCfg->pRequestHeadersStorage ) {
		xrtHttpHeadersDestroy(pCfg->pRequestHeadersStorage);
		pCfg->pRequestHeadersStorage = NULL;
	}
}


XXAPI bool xrtWsClientConfigSetURL(xwsclientconfig* pCfg, const char* sURL)
{
	return pCfg && __xwsSetConfigText(pCfg->sURL, sizeof(pCfg->sURL), &pCfg->pURLStorage, sURL);
}


XXAPI bool xrtWsClientConfigSetOrigin(xwsclientconfig* pCfg, const char* sOrigin)
{
	if ( !pCfg || !__xwsValidFieldValue(sOrigin) ) { return false; }
	return __xwsSetConfigText(pCfg->sOrigin, sizeof(pCfg->sOrigin), &pCfg->pOriginStorage, sOrigin);
}


XXAPI bool xrtWsClientConfigSetProtocols(xwsclientconfig* pCfg, const char* sProtocols)
{
	if ( !pCfg || !__xwsValidProtocolList(sProtocols) ) { return false; }
	return __xwsSetConfigText(pCfg->sProtocol, sizeof(pCfg->sProtocol), &pCfg->pProtocolStorage, sProtocols);
}


XXAPI const char* xrtWsClientConfigURL(const xwsclientconfig* pCfg)
{
	return __xwsClientConfigURLValue(pCfg);
}


XXAPI const char* xrtWsClientConfigOrigin(const xwsclientconfig* pCfg)
{
	return __xwsClientConfigOriginValue(pCfg);
}


XXAPI const char* xrtWsClientConfigProtocols(const xwsclientconfig* pCfg)
{
	return __xwsClientConfigProtocolValue(pCfg);
}


XXAPI bool xrtWsClientConfigSetHeader(xwsclientconfig* pCfg, const char* sName, const char* sValue)
{
	if ( !pCfg || pCfg->iSize < XWS_CLIENT_CONFIG_V2_SIZE || !sName || !sValue ||
		__xwsManagedHandshakeHeader(sName) ) { return false; }
	if ( !pCfg->pRequestHeadersStorage ) {
		pCfg->pRequestHeadersStorage = xrtHttpHeadersCreate();
		if ( !pCfg->pRequestHeadersStorage ) { return false; }
	}
	return xrtHttpHeadersSet(pCfg->pRequestHeadersStorage, sName, sValue) == XHTTP_SEMANTIC_OK;
}


XXAPI bool xrtWsClientConfigAddHeader(xwsclientconfig* pCfg, const char* sName, const char* sValue)
{
	if ( !pCfg || pCfg->iSize < XWS_CLIENT_CONFIG_V2_SIZE || !sName || !sValue ||
		__xwsManagedHandshakeHeader(sName) ) { return false; }
	if ( !pCfg->pRequestHeadersStorage ) {
		pCfg->pRequestHeadersStorage = xrtHttpHeadersCreate();
		if ( !pCfg->pRequestHeadersStorage ) { return false; }
	}
	return xrtHttpHeadersAppend(pCfg->pRequestHeadersStorage, sName, sValue) == XHTTP_SEMANTIC_OK;
}


XXAPI size_t xrtWsClientConfigRemoveHeader(xwsclientconfig* pCfg, const char* sName)
{
	if ( !pCfg || pCfg->iSize < XWS_CLIENT_CONFIG_V2_SIZE || !pCfg->pRequestHeadersStorage || !sName ) { return 0u; }
	return xrtHttpHeadersRemove(pCfg->pRequestHeadersStorage, sName);
}


XXAPI const xhttpheaders* xrtWsClientConfigHeaders(const xwsclientconfig* pCfg)
{
	return (pCfg && pCfg->iSize >= XWS_CLIENT_CONFIG_V2_SIZE) ? pCfg->pRequestHeadersStorage : NULL;
}


// 初始化 WebSocket server 配置
XXAPI void xrtWsServerConfigInit(xwsserverconfig* pCfg)
{
	if ( !pCfg ) { return; }
	memset(pCfg, 0, sizeof(xwsserverconfig));
	pCfg->iSize = XWS_SERVER_CONFIG_V3_SIZE;
	pCfg->iVersion = XWS_SERVER_CONFIG_VERSION;
	pCfg->iBacklog = 128u;
	pCfg->iRecvLimit = 1024u * 1024u;
	pCfg->iHighWater = 262144u;
	pCfg->iLowWater = 65536u;
	pCfg->iMaxQueuedBytes = 1048576u;
	pCfg->iCompressMinBytes = 256u;
	pCfg->iCompressionLevel = 6;
	pCfg->iMaxFrameBytes = 1024u * 1024u;
	pCfg->iHandshakeMaxBytes = XCODEC_HTTP1_DEFAULT_HEADER_BYTES;
	pCfg->iHandshakeTimeoutMs = 10000u;
	pCfg->iCloseTimeoutMs = 5000u;
}


XXAPI void xrtWsServerEventsInit(xwsserverevents* pEvents)
{
	if ( !pEvents ) { return; }
	memset(pEvents, 0, sizeof(*pEvents));
	pEvents->iSize = XWS_SERVER_EVENTS_V2_SIZE;
	pEvents->iVersion = XWS_SERVER_EVENTS_VERSION;
}


XXAPI void xrtWsServerConfigUnit(xwsserverconfig* pCfg)
{
	if ( !pCfg ) { return; }
	if ( pCfg->pProtocolStorage ) { XNET_FREE(pCfg->pProtocolStorage); pCfg->pProtocolStorage = NULL; }
	if ( pCfg->iSize >= XWS_SERVER_CONFIG_V2_SIZE ) {
		if ( pCfg->pPathStorage ) { XNET_FREE(pCfg->pPathStorage); pCfg->pPathStorage = NULL; }
		if ( pCfg->pOriginStorage ) { XNET_FREE(pCfg->pOriginStorage); pCfg->pOriginStorage = NULL; }
	}
}


XXAPI bool xrtWsServerConfigSetProtocol(xwsserverconfig* pCfg, const char* sProtocol)
{
	if ( !pCfg || !sProtocol || (sProtocol[0] && !__xwsValidToken(sProtocol)) ) { return false; }
	return __xwsSetConfigText(pCfg->sProtocol, sizeof(pCfg->sProtocol), &pCfg->pProtocolStorage, sProtocol);
}


XXAPI const char* xrtWsServerConfigProtocol(const xwsserverconfig* pCfg)
{
	return __xwsServerConfigProtocolValue(pCfg);
}


XXAPI bool xrtWsServerConfigSetPath(xwsserverconfig* pCfg, const char* sPath)
{
	xrturlview tTarget;
	char* pCopy = NULL;
	if ( !pCfg || pCfg->iSize < XWS_SERVER_CONFIG_V2_SIZE || !sPath ) { return false; }
	if ( sPath[0] ) {
		if ( sPath[0] != '/' || !xrtUrlParseTarget(sPath, &tTarget) ||
			(tTarget.iFlags & (XRT_URL_F_HAS_QUERY | XRT_URL_F_HAS_FRAGMENT)) != 0u ||
			tTarget.tPath.iLen != strlen(sPath) ) { return false; }
		pCopy = __xwsDupText(sPath);
		if ( !pCopy ) { return false; }
	}
	if ( pCfg->pPathStorage ) { XNET_FREE(pCfg->pPathStorage); }
	pCfg->pPathStorage = pCopy;
	return true;
}


XXAPI bool xrtWsServerConfigSetOrigin(xwsserverconfig* pCfg, const char* sOrigin)
{
	char* pCopy = NULL;
	if ( !pCfg || pCfg->iSize < XWS_SERVER_CONFIG_V2_SIZE || !sOrigin || !__xwsValidFieldValue(sOrigin) ) { return false; }
	if ( sOrigin[0] ) {
		pCopy = __xwsDupText(sOrigin);
		if ( !pCopy ) { return false; }
	}
	if ( pCfg->pOriginStorage ) { XNET_FREE(pCfg->pOriginStorage); }
	pCfg->pOriginStorage = pCopy;
	return true;
}


XXAPI const char* xrtWsServerConfigPath(const xwsserverconfig* pCfg)
{
	return __xwsServerConfigPathValue(pCfg);
}


XXAPI const char* xrtWsServerConfigOrigin(const xwsserverconfig* pCfg)
{
	return __xwsServerConfigOriginValue(pCfg);
}


// xrtWsClientCreate 相关处理
XXAPI xwsclient* xrtWsClientCreate(xnetengine* pEngine, const xwsclientconfig* pCfg, const xwsclientevents* pEvents, ptr pUserData)
{
	xwsclient* pClient;
	if ( !pEngine ) { return NULL; }
	pClient = (xwsclient*)XNET_ALLOC(sizeof(xwsclient));
	if ( !pClient ) { return NULL; }
	memset(pClient, 0, sizeof(xwsclient));
	pClient->iRefCount = 1;
	pClient->pEngine = pEngine;
	if ( pCfg ) {
		if ( !__xwsClientConfigClone(&pClient->tConfig, pCfg) ) { XNET_FREE(pClient); return NULL; }
	}
	else { xrtWsClientConfigInit(&pClient->tConfig); }
	if ( !__xwsValidProtocolList(__xwsClientConfigProtocolValue(&pClient->tConfig)) ||
		!__xwsValidFieldValue(__xwsClientConfigOriginValue(&pClient->tConfig)) ||
		!__xwsValidSendBudget(pClient->tConfig.iHighWater, pClient->tConfig.iLowWater, pClient->tConfig.iMaxQueuedBytes) ||
		!__xwsValidReceiveLimits(pClient->tConfig.iRecvLimit, pClient->tConfig.iMaxFrameBytes,
			pClient->tConfig.iHandshakeMaxBytes) ||
		!__xwsValidFeatureConfig(pClient->tConfig.iWebSocketFlags, pClient->tConfig.iCompressionLevel) ||
		!__xwsClientEventsCopy(&pClient->tEvents, pEvents) ) {
		xrtWsClientConfigUnit(&pClient->tConfig);
		XNET_FREE(pClient);
		return NULL;
	}
	if ( pClient->tConfig.pProxy ) {
		pClient->tConfig.pProxy = xrtNetProxyAddRef(pClient->tConfig.pProxy);
	}
	pClient->pUserData = pUserData;
	pClient->iOpenResult = XRT_NET_ERROR;
	return pClient;
}


static void __xwsClientResetRunState(xwsclient* pClient)
{
	if ( !pClient ) { return; }
	__xwsClientCancelTimers(pClient);
	if ( pClient->sProtocol ) { XNET_FREE(pClient->sProtocol); pClient->sProtocol = NULL; }
	__xwsMessageReset(&pClient->pMsgBuf, &pClient->iMsgLen, &pClient->iMsgCap,
		&pClient->iMsgOpcode);
	pClient->bMsgCompressed = false;
	pClient->bPerMessageDeflate = false;
	pClient->iCloseTransportResult = XRT_NET_AGAIN;
	pClient->iLocalCloseCode = 0u;
	pClient->iRemoteCloseCode = 0u;
	pClient->iRemoteCloseReasonLen = 0u;
	pClient->sRemoteCloseReason[0] = '\0';
	pClient->iLastSysErr = 0;
	pClient->iOpenResult = XRT_NET_AGAIN;
	(void)__xnetAtomicExchange32(&pClient->iOpen, 0);
	(void)__xnetAtomicExchange32(&pClient->iClosePosted, 0);
	(void)__xnetAtomicExchange32(&pClient->iCloseNotified, 0);
	(void)__xnetAtomicExchange32(&pClient->iCloseFlags, 0);
	(void)__xnetAtomicExchange32(&pClient->iRunState, __XWS_RUN_OPENING);
}


// xrtWsClientStart 相关处理
XXAPI xnet_result xrtWsClientStart(xwsclient* pClient)
{
	xnetconnectconfig tConnCfg;
	if ( !pClient || !pClient->pEngine || pClient->pStream ||
		__xwsAtomicLoad(&pClient->iDataSendActive) != 0 ||
		__xwsAtomicLoad(&pClient->iDestroyRequested) != 0 ) { return XRT_NET_ERROR; }
	__xwsClientResetRunState(pClient);
	// 解析 WebSocket URL
	if ( !__xwsParseURL(__xwsClientConfigURLValue(&pClient->tConfig), &pClient->tURL) ) {
		__xwsClientSetOpenFailure(pClient, XRT_NET_ERROR);
		return XRT_NET_ERROR;
	}
	// 创建网络流
	pClient->pStream = xrtNetStreamCreate(pClient->pEngine, __xwsClientStreamEvents(), pClient);
	if ( !pClient->pStream ) {
		__xwsClientSetOpenFailure(pClient, XRT_NET_ERROR);
		return XRT_NET_ERROR;
	}
	__xwsConfigureStreamControlBudget(pClient->pStream);
	// 配置连接参数
	xrtNetConnectConfigInit(&tConnCfg);
	tConnCfg.sHost = pClient->tURL.sHost;
	tConnCfg.iPort = pClient->tURL.iPort;
	tConnCfg.iConnectTimeoutMs = pClient->tConfig.iConnectTimeoutMs;
	tConnCfg.iRecvLimit = __xwsResolveTransportRecvLimit(pClient->tConfig.iMaxFrameBytes,
		pClient->tConfig.iRecvLimit, pClient->tConfig.iHandshakeMaxBytes);
	if ( pClient->tConfig.iHighWater > 0u ) { tConnCfg.iHighWater = pClient->tConfig.iHighWater; }
	if ( pClient->tConfig.iLowWater > 0u ) { tConnCfg.iLowWater = pClient->tConfig.iLowWater; }
	if ( pClient->tConfig.iMaxQueuedBytes > 0u ) { tConnCfg.iMaxQueuedBytes = pClient->tConfig.iMaxQueuedBytes; }
	tConnCfg.pProxy = pClient->tConfig.pProxy;
	// 安全连接（wss）时配置 TLS
	if ( pClient->tURL.bSecure ) {
		memset(&pClient->tTlsCfg, 0, sizeof(pClient->tTlsCfg));
		pClient->tTlsCfg.sHostName = pClient->tURL.sHost;
		pClient->tTlsCfg.bVerifyPeer = pClient->tConfig.bVerifyPeer;
		tConnCfg.pTlsConfig = &pClient->tTlsCfg;
	}
	// 发起异步连接
	if ( xrtNetStreamConnect(pClient->pStream, &tConnCfg) != XRT_NET_OK ) {
		xrtNetStreamDestroy(pClient->pStream);
		pClient->pStream = NULL;
		__xwsClientSetOpenFailure(pClient, XRT_NET_ERROR);
		return XRT_NET_ERROR;
	}
	return XRT_NET_OK;
}


// xrtWsClientStop 相关处理
XXAPI void xrtWsClientStop(xwsclient* pClient)
{
	if ( !pClient ) { return; }
	__xwsClientCancelTimers(pClient);
	__xwsClientSetOpenFailure(pClient, XRT_NET_CANCELLED);
	if ( pClient->pStream ) {
		xrtNetStreamClose(pClient->pStream, XNET_CLOSE_F_ABORT);
		xrtNetStreamDestroy(pClient->pStream);
		pClient->pStream = NULL;
	}
	if ( pClient->sProtocol ) { XNET_FREE(pClient->sProtocol); pClient->sProtocol = NULL; }
	__xwsMessageReset(&pClient->pMsgBuf, &pClient->iMsgLen, &pClient->iMsgCap, &pClient->iMsgOpcode);
}


// xrtWsClientDestroy 相关处理
XXAPI void xrtWsClientDestroy(xwsclient* pClient)
{
	if ( !pClient ) { return; }
	if ( __xwsAtomicCompareExchange(&pClient->iDestroyRequested, 1, 0) != 0 ) { return; }
	xrtWsClientStop(pClient);
	if ( pClient->tConfig.pProxy ) {
		xrtNetProxyRelease(pClient->tConfig.pProxy);
		pClient->tConfig.pProxy = NULL;
	}
	xrtWsClientConfigUnit(&pClient->tConfig);
	__xwsURLUnit(&pClient->tURL);
	if ( xrtAtomicRefRelease(&pClient->iRefCount) == 0 ) { XNET_FREE(pClient); }
}


// xrtWsClientIsOpen 相关处理
XXAPI bool xrtWsClientIsOpen(const xwsclient* pClient)
{
	return pClient && __xwsAtomicLoadConst(&pClient->iOpen) != 0 && pClient->pStream != NULL;
}


XXAPI const char* xrtWsClientProtocol(const xwsclient* pClient)
{
	return (pClient && pClient->sProtocol) ? pClient->sProtocol : "";
}


XXAPI bool xrtWsClientCloseInfo(const xwsclient* pClient, xwscloseinfo* pInfo)
{
	if ( !pClient || !pInfo ) { return false; }
	__xwsFillCloseInfo(pInfo, &pClient->iCloseFlags, pClient->iCloseTransportResult,
		pClient->iLocalCloseCode, pClient->iRemoteCloseCode,
		pClient->sRemoteCloseReason, pClient->iRemoteCloseReasonLen);
	return true;
}


XXAPI bool xrtWsClientPerMessageDeflate(const xwsclient* pClient)
{
	return pClient && pClient->bPerMessageDeflate;
}


static bool __xwsWaitKindSupported(uint32 iWaitKind)
{
	return iWaitKind == XNET_STREAM_WAIT_WRITABLE || iWaitKind == XNET_STREAM_WAIT_DRAIN ||
		iWaitKind == XNET_STREAM_WAIT_CLOSE;
}


XXAPI size_t xrtWsClientPendingSend(const xwsclient* pClient)
{
	return (pClient && pClient->pStream) ? xrtNetStreamPendingSend(pClient->pStream) : 0u;
}


XXAPI xnetfuture* xrtWsClientOpenFuture(xwsclient* pClient)
{
	__xws_open_waiter* pWaiter;
	xnetfuture* pFuture;
	long iState;
	xnet_result iStatus;
	if ( !pClient ) { return NULL; }
	pFuture = xrtNetFutureCreate();
	if ( !pFuture ) { return NULL; }
	pWaiter = (__xws_open_waiter*)XNET_ALLOC(sizeof(__xws_open_waiter));
	if ( !pWaiter ) {
		xrtNetFutureDestroy(pFuture);
		return NULL;
	}
	memset(pWaiter, 0, sizeof(*pWaiter));
	pWaiter->pFuture = pFuture;
	__xwsLock(&pClient->iOpenWaitLock);
	iState = __xwsAtomicLoad(&pClient->iRunState);
	if ( iState == __XWS_RUN_OPENING ) {
		__xnetFutureAddAsyncHold(pFuture);
		pWaiter->pNext = pClient->pOpenWaitHead;
		pClient->pOpenWaitHead = pWaiter;
		__xwsUnlock(&pClient->iOpenWaitLock);
		return pFuture;
	}
	iStatus = iState == __XWS_RUN_OPEN ? XRT_NET_OK : pClient->iOpenResult;
	__xwsUnlock(&pClient->iOpenWaitLock);
	XNET_FREE(pWaiter);
	if ( iState == __XWS_RUN_IDLE ) {
		xrtNetFutureDestroy(pFuture);
		return NULL;
	}
	(void)__xnetFutureResolve(pFuture, iStatus,
		iStatus == XRT_NET_OK ? pClient : NULL);
	return pFuture;
}


XXAPI xnetfuture* xrtWsClientStartFuture(xwsclient* pClient)
{
	if ( !pClient || pClient->pStream ) { return NULL; }
	(void)xrtWsClientStart(pClient);
	return xrtWsClientOpenFuture(pClient);
}


static xnet_result __xwsClientWaitOpenCore(xwsclient* pClient, int iMode,
	int64 iDeadlineMs, uint32 iTimeoutMs, bool bCoroutine)
{
	xnetfuture* pFuture = xrtWsClientOpenFuture(pClient);
	xnet_result iStatus;
	if ( !pFuture ) { return XRT_NET_ERROR; }
	if ( bCoroutine ) {
		if ( iMode == 0 ) { iStatus = xrtNetFutureWaitCo(pFuture); }
		else if ( iMode == 1 ) { iStatus = xrtNetFutureWaitCoTimeout(pFuture, iTimeoutMs); }
		else { iStatus = xrtNetFutureWaitCoUntil(pFuture, iDeadlineMs); }
	} else {
		if ( iMode == 0 ) { iStatus = xrtNetFutureWait(pFuture, XNET_WAIT_INFINITE); }
		else if ( iMode == 1 ) { iStatus = xrtNetFutureWait(pFuture, iTimeoutMs); }
		else { iStatus = xrtNetFutureWaitUntil(pFuture, iDeadlineMs); }
	}
	__xnetFutureReleaseRefInternal(pFuture);
	return iStatus;
}


XXAPI xnet_result xrtWsClientWaitOpen(xwsclient* pClient)
{
	return __xwsClientWaitOpenCore(pClient, 0, 0, 0u, false);
}


XXAPI xnet_result xrtWsClientWaitOpenTimeout(xwsclient* pClient, uint32 iTimeoutMs)
{
	return __xwsClientWaitOpenCore(pClient, 1, 0, iTimeoutMs, false);
}


XXAPI xnet_result xrtWsClientWaitOpenUntil(xwsclient* pClient, int64 iDeadlineMs)
{
	return __xwsClientWaitOpenCore(pClient, 2, iDeadlineMs, 0u, false);
}


XXAPI xnet_result xrtWsClientWaitOpenCo(xwsclient* pClient)
{
	return __xwsClientWaitOpenCore(pClient, 0, 0, 0u, true);
}


XXAPI xnet_result xrtWsClientWaitOpenCoTimeout(xwsclient* pClient, uint32 iTimeoutMs)
{
	return __xwsClientWaitOpenCore(pClient, 1, 0, iTimeoutMs, true);
}


XXAPI xnet_result xrtWsClientWaitOpenCoUntil(xwsclient* pClient, int64 iDeadlineMs)
{
	return __xwsClientWaitOpenCore(pClient, 2, iDeadlineMs, 0u, true);
}


XXAPI xnetfuture* xrtWsClientWritableFuture(xwsclient* pClient)
{
	return (pClient && pClient->pStream) ? xrtNetStreamWritableFuture(pClient->pStream) : NULL;
}


XXAPI xnetfuture* xrtWsClientDrainFuture(xwsclient* pClient)
{
	return (pClient && pClient->pStream) ? xrtNetStreamDrainFuture(pClient->pStream) : NULL;
}


XXAPI xnetfuture* xrtWsClientCloseFuture(xwsclient* pClient)
{
	return (pClient && pClient->pStream) ? xrtNetStreamCloseFuture(pClient->pStream) : NULL;
}


XXAPI xnet_result xrtWsClientWaitEx(xwsclient* pClient, uint32 iWaitKind)
{
	if ( !pClient || !pClient->pStream || !__xwsWaitKindSupported(iWaitKind) ) { return XRT_NET_ERROR; }
	return xrtNetStreamWaitEx(pClient->pStream, iWaitKind);
}


XXAPI xnet_result xrtWsClientWaitTimeoutEx(xwsclient* pClient, uint32 iWaitKind, uint32 iTimeoutMs)
{
	if ( !pClient || !pClient->pStream || !__xwsWaitKindSupported(iWaitKind) ) { return XRT_NET_ERROR; }
	return xrtNetStreamWaitTimeoutEx(pClient->pStream, iWaitKind, iTimeoutMs);
}


XXAPI xnet_result xrtWsClientWaitUntilEx(xwsclient* pClient, uint32 iWaitKind, int64 iDeadlineMs)
{
	if ( !pClient || !pClient->pStream || !__xwsWaitKindSupported(iWaitKind) ) { return XRT_NET_ERROR; }
	return xrtNetStreamWaitUntilEx(pClient->pStream, iWaitKind, iDeadlineMs);
}


XXAPI xnet_result xrtWsClientWaitCoEx(xwsclient* pClient, uint32 iWaitKind)
{
	if ( !pClient || !pClient->pStream || !__xwsWaitKindSupported(iWaitKind) ) { return XRT_NET_ERROR; }
	return xrtNetStreamWaitCoEx(pClient->pStream, iWaitKind);
}


XXAPI xnet_result xrtWsClientWaitCoTimeoutEx(xwsclient* pClient, uint32 iWaitKind, uint32 iTimeoutMs)
{
	if ( !pClient || !pClient->pStream || !__xwsWaitKindSupported(iWaitKind) ) { return XRT_NET_ERROR; }
	return xrtNetStreamWaitCoTimeoutEx(pClient->pStream, iWaitKind, iTimeoutMs);
}


XXAPI xnet_result xrtWsClientWaitCoUntilEx(xwsclient* pClient, uint32 iWaitKind, int64 iDeadlineMs)
{
	if ( !pClient || !pClient->pStream || !__xwsWaitKindSupported(iWaitKind) ) { return XRT_NET_ERROR; }
	return xrtNetStreamWaitCoUntilEx(pClient->pStream, iWaitKind, iDeadlineMs);
}


// 发送 WebSocket client 文本
XXAPI xnet_result xrtWsClientSendText(xwsclient* pClient, const char* sText, size_t iLen)
{
	xnet_result iResult;
	if ( !pClient || !sText || !__xwsValidUtf8(sText, iLen) ) { return XRT_NET_ERROR; }
	if ( __xwsAtomicCompareExchange(&pClient->iDataSendActive, 1, 0) != 0 ) { return XRT_NET_AGAIN; }
	__xwsLock(&pClient->iSendLock);
	if ( !pClient->pStream || !xrtWsClientIsOpen(pClient) ||
		__xwsAtomicLoad(&pClient->iClosePosted) != 0 ) { iResult = XRT_NET_CLOSED; }
	else {
		iResult = __xwsStreamSendMessage(pClient->pStream, true, XCODEC_WS_OPCODE_TEXT, sText, iLen,
			pClient->bPerMessageDeflate, pClient->tConfig.iCompressMinBytes,
			pClient->tConfig.iCompressionLevel);
	}
	__xwsUnlock(&pClient->iSendLock);
	(void)__xnetAtomicExchange32(&pClient->iDataSendActive, 0);
	return iResult;
}


// xrtWsClientSendBinary 相关处理
XXAPI xnet_result xrtWsClientSendBinary(xwsclient* pClient, const void* pData, size_t iLen)
{
	xnet_result iResult;
	if ( !pClient || (!pData && iLen != 0u) ) { return XRT_NET_ERROR; }
	if ( __xwsAtomicCompareExchange(&pClient->iDataSendActive, 1, 0) != 0 ) { return XRT_NET_AGAIN; }
	__xwsLock(&pClient->iSendLock);
	if ( !pClient->pStream || !xrtWsClientIsOpen(pClient) ||
		__xwsAtomicLoad(&pClient->iClosePosted) != 0 ) { iResult = XRT_NET_CLOSED; }
	else {
		iResult = __xwsStreamSendMessage(pClient->pStream, true, XCODEC_WS_OPCODE_BINARY, pData, iLen,
			pClient->bPerMessageDeflate, pClient->tConfig.iCompressMinBytes,
			pClient->tConfig.iCompressionLevel);
	}
	__xwsUnlock(&pClient->iSendLock);
	(void)__xnetAtomicExchange32(&pClient->iDataSendActive, 0);
	return iResult;
}


// xrtWsClientPing 相关处理
XXAPI xnet_result xrtWsClientPing(xwsclient* pClient, const void* pData, size_t iLen)
{
	xnet_result iResult;
	if ( !pClient || !pClient->pStream || !xrtWsClientIsOpen(pClient) ||
		iLen > 125u || (!pData && iLen != 0u) ) { return XRT_NET_ERROR; }
	__xwsLock(&pClient->iSendLock);
	iResult = __xwsAtomicLoad(&pClient->iClosePosted) == 0 ?
		__xwsStreamSendFrame(pClient->pStream, true, XCODEC_WS_OPCODE_PING, pData, iLen) : XRT_NET_CLOSED;
	__xwsUnlock(&pClient->iSendLock);
	return iResult;
}


// xrtWsClientClose 相关处理
XXAPI xnet_result xrtWsClientClose(xwsclient* pClient, uint16 iCode, const char* sReason)
{
	if ( !pClient || !pClient->pStream ) { return XRT_NET_ERROR; }
	return __xwsClientStartClose(pClient, iCode, sReason, false);
}


// xrtWsServerCreate 相关处理
XXAPI xwsserver* xrtWsServerCreate(xnetengine* pEngine, const xwsserverconfig* pCfg, const xwsserverevents* pEvents, ptr pUserData)
{
	xwsserver* pServer;
	if ( !pEngine ) { return NULL; }
	pServer = (xwsserver*)XNET_ALLOC(sizeof(xwsserver));
	if ( !pServer ) { return NULL; }
	memset(pServer, 0, sizeof(xwsserver));
	pServer->pEngine = pEngine;
	if ( pCfg ) {
		if ( !__xwsServerConfigClone(&pServer->tConfig, pCfg) ) { XNET_FREE(pServer); return NULL; }
	}
	else { xrtWsServerConfigInit(&pServer->tConfig); }
	if ( __xwsServerConfigProtocolValue(&pServer->tConfig)[0] &&
		!__xwsValidToken(__xwsServerConfigProtocolValue(&pServer->tConfig)) ) {
		xrtWsServerConfigUnit(&pServer->tConfig);
		XNET_FREE(pServer);
		return NULL;
	}
	if ( !__xwsValidSendBudget(pServer->tConfig.iHighWater, pServer->tConfig.iLowWater, pServer->tConfig.iMaxQueuedBytes) ||
		!__xwsValidReceiveLimits(pServer->tConfig.iRecvLimit, pServer->tConfig.iMaxFrameBytes,
			pServer->tConfig.iHandshakeMaxBytes) ||
		!__xwsValidFeatureConfig(pServer->tConfig.iWebSocketFlags, pServer->tConfig.iCompressionLevel) ||
		!__xwsServerEventsCopy(&pServer->tEvents, pEvents) ) {
		xrtWsServerConfigUnit(&pServer->tConfig);
		XNET_FREE(pServer);
		return NULL;
	}
	pServer->pUserData = pUserData;
	return pServer;
}


#if !defined(XRT_NO_XHTTPD)
XXAPI xnet_result xrtWsServerUpgradeHttpd(xwsserver* pServer, xhttpdconn* pHttpdConn,
	const xhttpdrequest* pRequest, xwsconn** ppWsConn)
{
	xcodechttp1msg tMessage;
	xcodechttp1header* pAllocatedHeaders = NULL;
	xwshandshakeresponse tHandshake;
	xhttpdresponse tOutput;
	xwsconn* pWsConn = NULL;
	xnetstream* pStream = NULL;
	const char* sKey = NULL;
	char sAccept[64];
	uint32 iStatus;
	int iDecision;
	xnet_result iResult;
	if ( ppWsConn ) { *ppWsConn = NULL; }
	if ( !pServer || !pHttpdConn || !pRequest || !ppWsConn ||
		!__xwsHttpdRequestView(pRequest, &tMessage, &pAllocatedHeaders) ) {
		return XRT_NET_ERROR;
	}
	memset(&tHandshake, 0, sizeof(tHandshake));
	memset(&tOutput, 0, sizeof(tOutput));
	iStatus = __xwsServerValidateRequest(&tMessage, &sKey);
	if ( iStatus == 0u ) { iStatus = __xwsServerConfiguredPolicyStatus(pServer, &tMessage); }
	if ( iStatus != 0u ) {
		if ( !xrtWsHandshakeResponseInit(&tHandshake) ) {
			XNET_FREE(pAllocatedHeaders);
			return XRT_NET_ERROR;
		}
		tHandshake.iStatusCode = iStatus;
		tHandshake.sReason = iStatus == 426u ? "Unsupported WebSocket Version" :
			__xwsHttpStatusText(iStatus);
		iResult = __xwsHttpdRespondHandshake(pHttpdConn, &tHandshake);
		xrtWsHandshakeResponseUnit(&tHandshake);
		XNET_FREE(pAllocatedHeaders);
		return iResult;
	}
	if ( !__xwsComputeAccept(sKey, sAccept, sizeof(sAccept)) ) {
		XNET_FREE(pAllocatedHeaders);
		return XRT_NET_ERROR;
	}
	pWsConn = (xwsconn*)XNET_ALLOC(sizeof(*pWsConn));
	if ( !pWsConn ) {
		XNET_FREE(pAllocatedHeaders);
		return XRT_NET_ERROR;
	}
	memset(pWsConn, 0, sizeof(*pWsConn));
	pWsConn->iRefCount = 1;
	pWsConn->pServer = pServer;
	iDecision = __xwsServerPrepareHandshake(pServer, pWsConn, &tMessage, &tHandshake);
	XNET_FREE(pAllocatedHeaders);
	if ( iDecision == __XWS_HANDSHAKE_DECISION_ERROR ) {
		if ( tHandshake.iVersion == XWS_HANDSHAKE_RESPONSE_VERSION ) {
			xrtWsHandshakeResponseUnit(&tHandshake);
		}
		__xwsServerEmitError(pServer, pWsConn, -34);
		xrtWsConnRelease(pWsConn);
		memset(&tHandshake, 0, sizeof(tHandshake));
		if ( !xrtWsHandshakeResponseInit(&tHandshake) ) { return XRT_NET_ERROR; }
		tHandshake.iStatusCode = 500u;
		tHandshake.sReason = "Internal Server Error";
		iResult = __xwsHttpdRespondHandshake(pHttpdConn, &tHandshake);
		xrtWsHandshakeResponseUnit(&tHandshake);
		return iResult;
	}
	if ( iDecision == __XWS_HANDSHAKE_DECISION_REJECT ) {
		xrtWsConnRelease(pWsConn);
		iResult = __xwsHttpdRespondHandshake(pHttpdConn, &tHandshake);
		xrtWsHandshakeResponseUnit(&tHandshake);
		return iResult;
	}
	if ( !__xwsHttpdBuildHandshakeResponse(&tOutput, &tHandshake, true, sAccept,
		pWsConn->sProtocol, pWsConn->bPerMessageDeflate) ) {
		xrtHttpdResponseUnit(&tOutput);
		xrtWsHandshakeResponseUnit(&tHandshake);
		xrtWsConnRelease(pWsConn);
		return XRT_NET_ERROR;
	}
	iResult = xrtHttpdConnUpgrade(pHttpdConn, &tOutput, __xwsServerStreamEvents(),
		pWsConn, &pStream);
	xrtHttpdResponseUnit(&tOutput);
	xrtWsHandshakeResponseUnit(&tHandshake);
	if ( iResult != XRT_NET_OK || !pStream ) {
		xrtWsConnRelease(pWsConn);
		return iResult != XRT_NET_OK ? iResult : XRT_NET_ERROR;
	}
	pWsConn->pStream = pStream;
	if ( pServer->tConfig.iMaxQueuedBytes > 0u ) {
		pStream->iMaxQueuedBytes = pServer->tConfig.iMaxQueuedBytes;
	}
	pStream->iRecvLimit = __xwsResolveTransportRecvLimit(pServer->tConfig.iMaxFrameBytes,
		pServer->tConfig.iRecvLimit, pServer->tConfig.iHandshakeMaxBytes);
	__xwsConfigureStreamControlBudget(pStream);
	__xnetStreamApplyWatermark(pStream, pServer->tConfig.iHighWater,
		pServer->tConfig.iLowWater);
	__xwsServerAddConn(pServer, pWsConn);
	(void)__xwsAtomicCompareExchange(&pWsConn->iOpen, 1, 0);
	*ppWsConn = pWsConn;
	if ( pServer->tEvents.OnOpen ) {
		pServer->tEvents.OnOpen(pServer->pUserData, pServer, pWsConn);
	}
	return XRT_NET_OK;
}
#endif


// xrtWsServerBoundPort 相关处理
XXAPI uint16 xrtWsServerBoundPort(const xwsserver* pServer)
{
	return (pServer && pServer->pListener) ? pServer->pListener->tConfig.tBindAddr.iPort : 0u;
}


// xrtWsServerStart 相关处理
XXAPI xnet_result xrtWsServerStart(xwsserver* pServer)
{
	xnetlistenconfig tListenCfg;
	if ( !pServer || !pServer->pEngine ) { return XRT_NET_ERROR; }
	if ( __xwsAtomicLoad(&pServer->bRunning) != 0 ) { return XRT_NET_OK; }
	// 配置监听参数
	xrtNetListenConfigInit(&tListenCfg);
	tListenCfg.tBindAddr = pServer->tConfig.tBindAddr;
	tListenCfg.iFlags = pServer->tConfig.iFlags;
	tListenCfg.iBacklog = pServer->tConfig.iBacklog;
	tListenCfg.iRecvLimit = __xwsResolveTransportRecvLimit(pServer->tConfig.iMaxFrameBytes,
		pServer->tConfig.iRecvLimit, pServer->tConfig.iHandshakeMaxBytes);
	if ( pServer->tConfig.iHighWater > 0u ) { tListenCfg.iHighWater = pServer->tConfig.iHighWater; }
	if ( pServer->tConfig.iLowWater > 0u ) { tListenCfg.iLowWater = pServer->tConfig.iLowWater; }
	if ( pServer->tConfig.iMaxQueuedBytes > 0u ) { tListenCfg.iMaxQueuedBytes = pServer->tConfig.iMaxQueuedBytes; }
	tListenCfg.pTlsConfig = pServer->tConfig.pTlsConfig;
	// 创建监听器
	pServer->pListener = xrtNetListenerCreate(pServer->pEngine, &tListenCfg, __xwsListenerEvents(), __xwsServerStreamEvents(), pServer);
	if ( !pServer->pListener ) { return XRT_NET_ERROR; }
	// 启动监听
	if ( xrtNetListenerStart(pServer->pListener) != XRT_NET_OK ) {
		xrtNetListenerDestroy(pServer->pListener);
		pServer->pListener = NULL;
		return XRT_NET_ERROR;
	}
	(void)__xwsAtomicCompareExchange(&pServer->bRunning, 1, 0);
	// 投递异步接收任务到工作线程
	if ( xrtNetEnginePost(pServer->pEngine, pServer->pListener->pWorker->iId, __xwsArmAcceptTask, pServer) != XRT_NET_OK ) {
		(void)__xwsAtomicCompareExchange(&pServer->bRunning, 0, 1);
		xrtNetListenerStop(pServer->pListener);
		xrtNetListenerDestroy(pServer->pListener);
		pServer->pListener = NULL;
		return XRT_NET_ERROR;
	}
	return XRT_NET_OK;
}


// xrtWsServerStop 相关处理
XXAPI void xrtWsServerStop(xwsserver* pServer)
{
	xwsconn* pConn;
	if ( !pServer ) { return; }
	// 标记为已停止
	if ( __xwsAtomicCompareExchange(&pServer->bRunning, 0, 1) == 0 ) {
		/* already stopped */
	}
	// 停止并销毁监听器
	if ( pServer->pListener ) {
		xrtNetListenerStop(pServer->pListener);
		xrtNetListenerDestroy(pServer->pListener);
		pServer->pListener = NULL;
	}
	// 分离所有连接并逐一清理
	pConn = __xwsServerDetachAllConns(pServer);
	while ( pConn ) {
		xwsconn* pNext = pConn->pNext;
		pConn->pNext = NULL;
		// 跳过已投递清理任务的连接
		if ( __xwsAtomicLoad(&pConn->iCleanupPosted) != 0 ) {
			pConn->pServer = NULL;
			pConn = pNext;
			continue;
		}
		(void)__xwsAtomicCompareExchange(&pConn->iCleanupPosted, 1, 0);
		// 关闭并销毁流
		if ( pConn->pStream ) {
			xrtNetStreamClose(pConn->pStream, XNET_CLOSE_F_ABORT);
			xrtNetStreamDestroy(pConn->pStream);
			pConn->pStream = NULL;
		}
		pConn->pServer = NULL;
		xrtWsConnRelease(pConn);
		pConn = pNext;
	}
}


// xrtWsServerDestroy 相关处理
XXAPI void xrtWsServerDestroy(xwsserver* pServer)
{
	if ( !pServer ) { return; }
	xrtWsServerStop(pServer);
	xrtWsServerConfigUnit(&pServer->tConfig);
	XNET_FREE(pServer);
}


// xrtWsConnIsOpen 相关处理
XXAPI xwsconn* xrtWsConnRetain(xwsconn* pConn)
{
	return pConn && xrtAtomicRefRetain(&pConn->iRefCount) > 0 ? pConn : NULL;
}


XXAPI void xrtWsConnRelease(xwsconn* pConn)
{
	if ( pConn && xrtAtomicRefRelease(&pConn->iRefCount) == 0 ) { __xwsConnFree(pConn); }
}


XXAPI bool xrtWsConnIsOpen(const xwsconn* pConn)
{
	return pConn && __xwsAtomicLoadConst(&pConn->iOpen) != 0 && pConn->pStream != NULL;
}


XXAPI const char* xrtWsConnProtocol(const xwsconn* pConn)
{
	return (pConn && pConn->sProtocol) ? pConn->sProtocol : "";
}


XXAPI ptr xrtWsConnGetData(const xwsconn* pConn)
{
	return pConn ? pConn->pConnectionData : NULL;
}


XXAPI void xrtWsConnSetData(xwsconn* pConn, ptr pConnectionData)
{
	if ( pConn ) { pConn->pConnectionData = pConnectionData; }
}


XXAPI bool xrtWsConnCloseInfo(const xwsconn* pConn, xwscloseinfo* pInfo)
{
	if ( !pConn || !pInfo ) { return false; }
	__xwsFillCloseInfo(pInfo, &pConn->iCloseFlags, pConn->iCloseTransportResult,
		pConn->iLocalCloseCode, pConn->iRemoteCloseCode,
		pConn->sRemoteCloseReason, pConn->iRemoteCloseReasonLen);
	return true;
}


XXAPI bool xrtWsConnPerMessageDeflate(const xwsconn* pConn)
{
	return pConn && pConn->bPerMessageDeflate;
}


XXAPI size_t xrtWsConnPendingSend(const xwsconn* pConn)
{
	return (pConn && pConn->pStream) ? xrtNetStreamPendingSend(pConn->pStream) : 0u;
}


XXAPI xnetfuture* xrtWsConnWritableFuture(xwsconn* pConn)
{
	return (pConn && pConn->pStream) ? xrtNetStreamWritableFuture(pConn->pStream) : NULL;
}


XXAPI xnetfuture* xrtWsConnDrainFuture(xwsconn* pConn)
{
	return (pConn && pConn->pStream) ? xrtNetStreamDrainFuture(pConn->pStream) : NULL;
}


XXAPI xnetfuture* xrtWsConnCloseFuture(xwsconn* pConn)
{
	return (pConn && pConn->pStream) ? xrtNetStreamCloseFuture(pConn->pStream) : NULL;
}


XXAPI xnet_result xrtWsConnWaitEx(xwsconn* pConn, uint32 iWaitKind)
{
	if ( !pConn || !pConn->pStream || !__xwsWaitKindSupported(iWaitKind) ) { return XRT_NET_ERROR; }
	return xrtNetStreamWaitEx(pConn->pStream, iWaitKind);
}


XXAPI xnet_result xrtWsConnWaitTimeoutEx(xwsconn* pConn, uint32 iWaitKind, uint32 iTimeoutMs)
{
	if ( !pConn || !pConn->pStream || !__xwsWaitKindSupported(iWaitKind) ) { return XRT_NET_ERROR; }
	return xrtNetStreamWaitTimeoutEx(pConn->pStream, iWaitKind, iTimeoutMs);
}


XXAPI xnet_result xrtWsConnWaitUntilEx(xwsconn* pConn, uint32 iWaitKind, int64 iDeadlineMs)
{
	if ( !pConn || !pConn->pStream || !__xwsWaitKindSupported(iWaitKind) ) { return XRT_NET_ERROR; }
	return xrtNetStreamWaitUntilEx(pConn->pStream, iWaitKind, iDeadlineMs);
}


XXAPI xnet_result xrtWsConnWaitCoEx(xwsconn* pConn, uint32 iWaitKind)
{
	if ( !pConn || !pConn->pStream || !__xwsWaitKindSupported(iWaitKind) ) { return XRT_NET_ERROR; }
	return xrtNetStreamWaitCoEx(pConn->pStream, iWaitKind);
}


XXAPI xnet_result xrtWsConnWaitCoTimeoutEx(xwsconn* pConn, uint32 iWaitKind, uint32 iTimeoutMs)
{
	if ( !pConn || !pConn->pStream || !__xwsWaitKindSupported(iWaitKind) ) { return XRT_NET_ERROR; }
	return xrtNetStreamWaitCoTimeoutEx(pConn->pStream, iWaitKind, iTimeoutMs);
}


XXAPI xnet_result xrtWsConnWaitCoUntilEx(xwsconn* pConn, uint32 iWaitKind, int64 iDeadlineMs)
{
	if ( !pConn || !pConn->pStream || !__xwsWaitKindSupported(iWaitKind) ) { return XRT_NET_ERROR; }
	return xrtNetStreamWaitCoUntilEx(pConn->pStream, iWaitKind, iDeadlineMs);
}


// 发送 WebSocket conn 文本
XXAPI xnet_result xrtWsConnSendText(xwsconn* pConn, const char* sText, size_t iLen)
{
	xnet_result iResult;
	if ( !pConn || !sText || !__xwsValidUtf8(sText, iLen) ) { return XRT_NET_ERROR; }
	if ( __xwsAtomicCompareExchange(&pConn->iDataSendActive, 1, 0) != 0 ) { return XRT_NET_AGAIN; }
	__xwsLock(&pConn->iSendLock);
	if ( !pConn->pStream || !xrtWsConnIsOpen(pConn) ||
		__xwsAtomicLoad(&pConn->iClosePosted) != 0 ) { iResult = XRT_NET_CLOSED; }
	else {
		iResult = __xwsStreamSendMessage(pConn->pStream, false, XCODEC_WS_OPCODE_TEXT, sText, iLen,
			pConn->bPerMessageDeflate, pConn->pServer ? pConn->pServer->tConfig.iCompressMinBytes : 0u,
			pConn->pServer ? pConn->pServer->tConfig.iCompressionLevel : 6);
	}
	__xwsUnlock(&pConn->iSendLock);
	(void)__xnetAtomicExchange32(&pConn->iDataSendActive, 0);
	return iResult;
}


// xrtWsConnSendBinary 相关处理
XXAPI xnet_result xrtWsConnSendBinary(xwsconn* pConn, const void* pData, size_t iLen)
{
	xnet_result iResult;
	if ( !pConn || (!pData && iLen != 0u) ) { return XRT_NET_ERROR; }
	if ( __xwsAtomicCompareExchange(&pConn->iDataSendActive, 1, 0) != 0 ) { return XRT_NET_AGAIN; }
	__xwsLock(&pConn->iSendLock);
	if ( !pConn->pStream || !xrtWsConnIsOpen(pConn) ||
		__xwsAtomicLoad(&pConn->iClosePosted) != 0 ) { iResult = XRT_NET_CLOSED; }
	else {
		iResult = __xwsStreamSendMessage(pConn->pStream, false, XCODEC_WS_OPCODE_BINARY, pData, iLen,
			pConn->bPerMessageDeflate, pConn->pServer ? pConn->pServer->tConfig.iCompressMinBytes : 0u,
			pConn->pServer ? pConn->pServer->tConfig.iCompressionLevel : 6);
	}
	__xwsUnlock(&pConn->iSendLock);
	(void)__xnetAtomicExchange32(&pConn->iDataSendActive, 0);
	return iResult;
}


// xrtWsConnPing 相关处理
XXAPI xnet_result xrtWsConnPing(xwsconn* pConn, const void* pData, size_t iLen)
{
	xnet_result iResult;
	if ( !pConn || !pConn->pStream || !xrtWsConnIsOpen(pConn) || iLen > 125u ||
		(!pData && iLen != 0u) ) { return XRT_NET_ERROR; }
	__xwsLock(&pConn->iSendLock);
	iResult = __xwsAtomicLoad(&pConn->iClosePosted) == 0 ?
		__xwsStreamSendFrame(pConn->pStream, false, XCODEC_WS_OPCODE_PING, pData, iLen) : XRT_NET_CLOSED;
	__xwsUnlock(&pConn->iSendLock);
	return iResult;
}


// xrtWsConnClose 相关处理
XXAPI xnet_result xrtWsConnClose(xwsconn* pConn, uint16 iCode, const char* sReason)
{
	if ( !pConn || !pConn->pStream ) { return XRT_NET_ERROR; }
	return __xwsConnStartClose(pConn, iCode, sReason, false);
}


static bool __xwsWriterUtf8Update(xwswriter* pWriter, const void* pData, size_t iLen)
{
	const uint8* pBytes = (const uint8*)pData;
	if ( !pWriter || (!pData && iLen != 0u) ) { return false; }
	for ( size_t i = 0u; i < iLen; ++i ) {
		uint8 ch = pBytes[i];
		if ( pWriter->iUtf8Need != 0u ) {
			uint8 iMin = pWriter->bUtf8First ? pWriter->iUtf8FirstMin : 0x80u;
			uint8 iMax = pWriter->bUtf8First ? pWriter->iUtf8FirstMax : 0xBFu;
			if ( ch < iMin || ch > iMax ) { return false; }
			pWriter->bUtf8First = false;
			pWriter->iUtf8Need--;
			continue;
		}
		if ( ch <= 0x7Fu ) { continue; }
		pWriter->bUtf8First = true;
		pWriter->iUtf8FirstMin = 0x80u;
		pWriter->iUtf8FirstMax = 0xBFu;
		if ( ch >= 0xC2u && ch <= 0xDFu ) { pWriter->iUtf8Need = 1u; }
		else if ( ch == 0xE0u ) { pWriter->iUtf8Need = 2u; pWriter->iUtf8FirstMin = 0xA0u; }
		else if ( (ch >= 0xE1u && ch <= 0xECu) || (ch >= 0xEEu && ch <= 0xEFu) ) {
			pWriter->iUtf8Need = 2u;
		}
		else if ( ch == 0xEDu ) { pWriter->iUtf8Need = 2u; pWriter->iUtf8FirstMax = 0x9Fu; }
		else if ( ch == 0xF0u ) { pWriter->iUtf8Need = 3u; pWriter->iUtf8FirstMin = 0x90u; }
		else if ( ch >= 0xF1u && ch <= 0xF3u ) { pWriter->iUtf8Need = 3u; }
		else if ( ch == 0xF4u ) { pWriter->iUtf8Need = 3u; pWriter->iUtf8FirstMax = 0x8Fu; }
		else { return false; }
	}
	return true;
}


static xwswriter* __xwsWriterCreateClient(xwsclient* pClient, uint8 iOpcode)
{
	xwswriter* pWriter;
	if ( !pClient || (iOpcode != XCODEC_WS_OPCODE_TEXT && iOpcode != XCODEC_WS_OPCODE_BINARY) ||
		__xwsAtomicLoad(&pClient->iDestroyRequested) != 0 ) { return NULL; }
	pWriter = (xwswriter*)XNET_ALLOC(sizeof(xwswriter));
	if ( !pWriter ) { return NULL; }
	memset(pWriter, 0, sizeof(*pWriter));
	if ( __xwsAtomicCompareExchange(&pClient->iDataSendActive, 1, 0) != 0 ) {
		XNET_FREE(pWriter);
		return NULL;
	}
	if ( xrtAtomicRefRetain(&pClient->iRefCount) <= 0 ) {
		(void)__xnetAtomicExchange32(&pClient->iDataSendActive, 0);
		XNET_FREE(pWriter);
		return NULL;
	}
	__xwsLock(&pClient->iSendLock);
	if ( !pClient->pStream || !xrtWsClientIsOpen(pClient) ||
		__xwsAtomicLoad(&pClient->iClosePosted) != 0 ) {
		__xwsUnlock(&pClient->iSendLock);
		(void)__xnetAtomicExchange32(&pClient->iDataSendActive, 0);
		(void)xrtAtomicRefRelease(&pClient->iRefCount);
		XNET_FREE(pWriter);
		return NULL;
	}
	pWriter->pStream = pClient->pStream;
	__xnetStreamAddAsyncHold(pWriter->pStream);
	__xwsUnlock(&pClient->iSendLock);
	pWriter->uOwner.pClient = pClient;
	pWriter->iOpcode = iOpcode;
	pWriter->bClient = true;
	return pWriter;
}


static xwswriter* __xwsWriterCreateConn(xwsconn* pConn, uint8 iOpcode)
{
	xwswriter* pWriter;
	if ( !pConn || (iOpcode != XCODEC_WS_OPCODE_TEXT && iOpcode != XCODEC_WS_OPCODE_BINARY) ) { return NULL; }
	pWriter = (xwswriter*)XNET_ALLOC(sizeof(xwswriter));
	if ( !pWriter ) { return NULL; }
	memset(pWriter, 0, sizeof(*pWriter));
	if ( __xwsAtomicCompareExchange(&pConn->iDataSendActive, 1, 0) != 0 ) {
		XNET_FREE(pWriter);
		return NULL;
	}
	if ( !xrtWsConnRetain(pConn) ) {
		(void)__xnetAtomicExchange32(&pConn->iDataSendActive, 0);
		XNET_FREE(pWriter);
		return NULL;
	}
	__xwsLock(&pConn->iSendLock);
	if ( !pConn->pStream || !xrtWsConnIsOpen(pConn) || __xwsAtomicLoad(&pConn->iClosePosted) != 0 ) {
		__xwsUnlock(&pConn->iSendLock);
		(void)__xnetAtomicExchange32(&pConn->iDataSendActive, 0);
		xrtWsConnRelease(pConn);
		XNET_FREE(pWriter);
		return NULL;
	}
	pWriter->pStream = pConn->pStream;
	__xnetStreamAddAsyncHold(pWriter->pStream);
	__xwsUnlock(&pConn->iSendLock);
	pWriter->uOwner.pConn = pConn;
	pWriter->iOpcode = iOpcode;
	pWriter->bClient = false;
	return pWriter;
}


XXAPI xwswriter* xrtWsClientBeginText(xwsclient* pClient)
{
	return __xwsWriterCreateClient(pClient, XCODEC_WS_OPCODE_TEXT);
}


XXAPI xwswriter* xrtWsClientBeginBinary(xwsclient* pClient)
{
	return __xwsWriterCreateClient(pClient, XCODEC_WS_OPCODE_BINARY);
}


XXAPI xwswriter* xrtWsConnBeginText(xwsconn* pConn)
{
	return __xwsWriterCreateConn(pConn, XCODEC_WS_OPCODE_TEXT);
}


XXAPI xwswriter* xrtWsConnBeginBinary(xwsconn* pConn)
{
	return __xwsWriterCreateConn(pConn, XCODEC_WS_OPCODE_BINARY);
}


static xnet_result __xwsWriterSend(xwswriter* pWriter, const void* pData,
	size_t iLen, bool bFinal)
{
	xwswriter tNext;
	xnet_result iResult;
	volatile long* pSendLock;
	volatile long* pClosePosted;
	xnetstream* pCurrentStream;
	if ( !pWriter || pWriter->bFinished || (!pData && iLen != 0u) ) { return XRT_NET_ERROR; }
	tNext = *pWriter;
	if ( pWriter->iOpcode == XCODEC_WS_OPCODE_TEXT &&
		(!__xwsWriterUtf8Update(&tNext, pData, iLen) || (bFinal && tNext.iUtf8Need != 0u)) ) {
		return XRT_NET_ERROR;
	}
	if ( pWriter->bClient ) {
		xwsclient* pClient = pWriter->uOwner.pClient;
		if ( !pClient ) { return XRT_NET_CLOSED; }
		pSendLock = &pClient->iSendLock;
		pClosePosted = &pClient->iClosePosted;
		pCurrentStream = NULL;
	} else {
		xwsconn* pConn = pWriter->uOwner.pConn;
		if ( !pConn ) { return XRT_NET_CLOSED; }
		pSendLock = &pConn->iSendLock;
		pClosePosted = &pConn->iClosePosted;
		pCurrentStream = NULL;
	}
	__xwsLock(pSendLock);
	pCurrentStream = pWriter->bClient ? pWriter->uOwner.pClient->pStream :
		pWriter->uOwner.pConn->pStream;
	if ( pCurrentStream != pWriter->pStream || !pCurrentStream ||
		__xwsAtomicLoad(pClosePosted) != 0 ) {
		iResult = XRT_NET_CLOSED;
	} else {
		iResult = __xwsStreamSendFrameEx(pWriter->pStream, bFinal, pWriter->bClient,
			pWriter->bStarted ? XCODEC_WS_OPCODE_CONT : pWriter->iOpcode, pData, iLen);
	}
	__xwsUnlock(pSendLock);
	if ( iResult == XRT_NET_OK ) {
		pWriter->iUtf8Need = tNext.iUtf8Need;
		pWriter->iUtf8FirstMin = tNext.iUtf8FirstMin;
		pWriter->iUtf8FirstMax = tNext.iUtf8FirstMax;
		pWriter->bUtf8First = tNext.bUtf8First;
		pWriter->bStarted = true;
		if ( bFinal ) {
			pWriter->bFinished = true;
			if ( pWriter->bClient ) {
				(void)__xnetAtomicExchange32(&pWriter->uOwner.pClient->iDataSendActive, 0);
			} else {
				(void)__xnetAtomicExchange32(&pWriter->uOwner.pConn->iDataSendActive, 0);
			}
		}
	}
	return iResult;
}


XXAPI xnet_result xrtWsWriterWrite(xwswriter* pWriter, const void* pData, size_t iLen)
{
	return __xwsWriterSend(pWriter, pData, iLen, false);
}


XXAPI xnet_result xrtWsWriterFinish(xwswriter* pWriter, const void* pData, size_t iLen)
{
	return __xwsWriterSend(pWriter, pData, iLen, true);
}


XXAPI bool xrtWsWriterIsFinished(const xwswriter* pWriter)
{
	return pWriter && pWriter->bFinished;
}


XXAPI void xrtWsWriterDestroy(xwswriter* pWriter)
{
	if ( !pWriter ) { return; }
	if ( !pWriter->bFinished ) {
		if ( pWriter->bStarted ) {
			if ( pWriter->bClient && pWriter->uOwner.pClient &&
				pWriter->uOwner.pClient->pStream == pWriter->pStream ) {
				(void)__xwsClientStartClose(pWriter->uOwner.pClient, XWS_CLOSE_INTERNAL,
					"message writer abandoned", true);
			} else if ( !pWriter->bClient && pWriter->uOwner.pConn &&
				pWriter->uOwner.pConn->pStream == pWriter->pStream ) {
				(void)__xwsConnStartClose(pWriter->uOwner.pConn, XWS_CLOSE_INTERNAL,
					"message writer abandoned", true);
			}
		}
		if ( pWriter->bClient && pWriter->uOwner.pClient ) {
			(void)__xnetAtomicExchange32(&pWriter->uOwner.pClient->iDataSendActive, 0);
		} else if ( pWriter->uOwner.pConn ) {
			(void)__xnetAtomicExchange32(&pWriter->uOwner.pConn->iDataSendActive, 0);
		}
	}
	if ( pWriter->pStream ) { __xnetStreamReleaseAsyncHold(pWriter->pStream); }
	if ( pWriter->bClient && pWriter->uOwner.pClient ) {
		xwsclient* pClient = pWriter->uOwner.pClient;
		if ( xrtAtomicRefRelease(&pClient->iRefCount) == 0 ) { XNET_FREE(pClient); }
	} else if ( pWriter->uOwner.pConn ) {
		xrtWsConnRelease(pWriter->uOwner.pConn);
	}
	XNET_FREE(pWriter);
}


#endif /* XRT_NO_XWS */

#endif
