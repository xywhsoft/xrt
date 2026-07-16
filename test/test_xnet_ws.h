#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#else
	#include <unistd.h>
#endif

#include "test_atomic_compat.h"

typedef struct {
	volatile long iOpenCount;
	volatile long iTextCount;
	volatile long iBinaryCount;
	volatile long iPingCount;
	volatile long iPongCount;
	volatile long iCloseCount;
	volatile long iCloseExCount;
	volatile long iErrorCount;
	volatile long iBackpressureCount;
	volatile long iWritableCount;
	volatile long iDrainCount;
	volatile long iHandshakeCount;
	volatile long iHandshakeHeaderCount;
	volatile long iRejectNext;
	uint32 iCloseFlags;
	uint16 iLocalCloseCode;
	uint16 iRemoteCloseCode;
	char aRemoteCloseReason[XWS_CLOSE_REASON_CAP + 1u];
	char aLastText[256];
	uint8 aLastBinary[256];
	size_t iLastTextLen;
	size_t iLastBinaryLen;
	size_t iLastBinaryMessageLen;
	xwsconn* pLastConn;
} __test_xws_server_ctx;

typedef struct {
	volatile long iOpenCount;
	volatile long iTextCount;
	volatile long iBinaryCount;
	volatile long iPingCount;
	volatile long iPongCount;
	volatile long iCloseCount;
	volatile long iCloseExCount;
	volatile long iErrorCount;
	volatile long iBackpressureCount;
	volatile long iWritableCount;
	volatile long iDrainCount;
	volatile long iHandshakeResponseCount;
	volatile long iHandshakeHeaderCount;
	uint32 iHandshakeStatus;
	uint32 iCloseFlags;
	uint16 iLocalCloseCode;
	uint16 iRemoteCloseCode;
	char aRemoteCloseReason[XWS_CLOSE_REASON_CAP + 1u];
	char aLastText[256];
	uint8 aLastBinary[256];
	size_t iLastTextLen;
	size_t iLastBinaryLen;
	size_t iLastBinaryMessageLen;
} __test_xws_client_ctx;

typedef struct {
	xwsserver* pWsServer;
	volatile long iRequestCount;
	volatile long iAcceptedCount;
} __test_xws_httpd_ctx;

typedef struct {
	volatile long iAcceptCount;
	volatile long iRecvCount;
	volatile long iCloseCount;
	xnetstream* pStream;
} __test_xws_stall_ctx;


// 内部函数：__Test_XWsSleepMs
static void __Test_XWsSleepMs(uint32 iDelayMs)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(iDelayMs);
	#else
		usleep((useconds_t)iDelayMs * 1000u);
	#endif
}


// 内部函数：__Test_XWsAtomicInc
static long __Test_XWsAtomicInc(volatile long* pValue)
{
	return __xrtTestAtomicAddFetchLong(pValue, 1);
}


// 内部函数：__Test_XWsAtomicLoad
static long __Test_XWsAtomicLoad(volatile long* pValue)
{
	return __xrtTestAtomicLoadLong(pValue);
}


// 内部函数：__Test_XWsWaitMin
static bool __Test_XWsWaitMin(volatile long* pValue, long iExpectMin, uint32 iTimeoutMs)
{
	uint32 iLoops = (iTimeoutMs / 10u) + 1u;
	for ( uint32 i = 0; i < iLoops; ++i ) {
		if ( __Test_XWsAtomicLoad(pValue) >= iExpectMin ) return true;
		__Test_XWsSleepMs(10u);
	}
	return __Test_XWsAtomicLoad(pValue) >= iExpectMin;
}


// 内部函数：__Test_XWsFileExists
static bool __Test_XWsFileExists(const char* sPath)
{
	FILE* pFile = fopen(sPath, "rb");
	if ( !pFile ) return false;
	fclose(pFile);
	return true;
}


// 内部函数：__Test_XWsServerOnOpen
static void __Test_XWsServerOnOpen(ptr pOwner, xwsserver* pServer, xwsconn* pConn)
{
	__test_xws_server_ctx* pCtx = (__test_xws_server_ctx*)pOwner;
	(void)pServer;
	if ( !pCtx ) return;
	pCtx->pLastConn = pConn;
	__Test_XWsAtomicInc(&pCtx->iOpenCount);
}


// 内部函数：__Test_XWsServerOnText
static void __Test_XWsServerOnText(ptr pOwner, xwsserver* pServer, xwsconn* pConn, const char* pData, size_t iLen)
{
	__test_xws_server_ctx* pCtx = (__test_xws_server_ctx*)pOwner;
	size_t iCopy;
	(void)pServer;
	if ( !pCtx || !pConn || !pData ) return;
	iCopy = iLen < (sizeof(pCtx->aLastText) - 1u) ? iLen : (sizeof(pCtx->aLastText) - 1u);
	memcpy(pCtx->aLastText, pData, iCopy);
	pCtx->aLastText[iCopy] = '\0';
	pCtx->iLastTextLen = iLen;
	(void)xrtWsConnSendText(pConn, pData, iLen);
	__Test_XWsAtomicInc(&pCtx->iTextCount);
}


// 内部函数：__Test_XWsServerOnBinary
static void __Test_XWsServerOnBinary(ptr pOwner, xwsserver* pServer, xwsconn* pConn, const void* pData, size_t iLen)
{
	__test_xws_server_ctx* pCtx = (__test_xws_server_ctx*)pOwner;
	size_t iCopy;
	(void)pServer;
	if ( !pCtx || !pConn || (!pData && iLen != 0u) ) return;
	iCopy = iLen < sizeof(pCtx->aLastBinary) ? iLen : sizeof(pCtx->aLastBinary);
	if ( iCopy > 0 ) memcpy(pCtx->aLastBinary, pData, iCopy);
	pCtx->iLastBinaryLen = iCopy;
	pCtx->iLastBinaryMessageLen = iLen;
	(void)xrtWsConnSendBinary(pConn, pData, iLen);
	__Test_XWsAtomicInc(&pCtx->iBinaryCount);
}


// 内部函数：__Test_XWsServerOnClose
static void __Test_XWsServerOnClose(ptr pOwner, xwsserver* pServer, xwsconn* pConn, xnet_result iReason)
{
	__test_xws_server_ctx* pCtx = (__test_xws_server_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	(void)iReason;
	if ( !pCtx ) return;
	__Test_XWsAtomicInc(&pCtx->iCloseCount);
}


static bool __Test_XWsStallOnAccept(ptr pOwner, xnetlistener* pListener, xnetstream* pStream)
{
	__test_xws_stall_ctx* pCtx = (__test_xws_stall_ctx*)pOwner;
	(void)pListener;
	if ( !pCtx || !pStream ) { return false; }
	pCtx->pStream = pStream;
	xrtNetStreamSetUserData(pStream, pCtx);
	__Test_XWsAtomicInc(&pCtx->iAcceptCount);
	return true;
}


static void __Test_XWsStallOnRecv(ptr pOwner, xnetstream* pStream, xnetchain* pChain)
{
	__test_xws_stall_ctx* pCtx = (__test_xws_stall_ctx*)pOwner;
	(void)pStream;
	if ( pCtx ) { __Test_XWsAtomicInc(&pCtx->iRecvCount); }
	if ( pChain ) { xrtNetChainConsume(pChain, xrtNetChainBytes(pChain)); }
}


static void __Test_XWsStallOnClose(ptr pOwner, xnetstream* pStream, xnet_result iReason)
{
	__test_xws_stall_ctx* pCtx = (__test_xws_stall_ctx*)pOwner;
	(void)iReason;
	if ( pCtx ) {
		pCtx->pStream = NULL;
		__Test_XWsAtomicInc(&pCtx->iCloseCount);
	}
	xrtNetStreamDestroy(pStream);
}


static const xnetlistenerevents* __Test_XWsStallListenerEvents(void)
{
	static const xnetlistenerevents tEvents = {
		__Test_XWsStallOnAccept,
		NULL
	};
	return &tEvents;
}


static const xnetstreamevents* __Test_XWsStallStreamEvents(void)
{
	static const xnetstreamevents tEvents = {
		NULL,
		__Test_XWsStallOnRecv,
		NULL,
		__Test_XWsStallOnClose,
		NULL,
		NULL,
		NULL,
		NULL
	};
	return &tEvents;
}


static void __Test_XWsIgnoreCloseOnRecv(ptr pOwner, xnetstream* pStream, xnetchain* pChain)
{
	(void)pOwner;
	(void)pStream;
	if ( pChain ) { xrtNetChainConsume(pChain, xrtNetChainBytes(pChain)); }
}


static const xnetstreamevents* __Test_XWsIgnoreCloseStreamEvents(void)
{
	static const xnetstreamevents tEvents = {
		__xwsServerStreamOnOpen,
		__Test_XWsIgnoreCloseOnRecv,
		__xwsServerStreamOnDrain,
		__xwsServerStreamOnClose,
		__xwsServerStreamOnError,
		__xwsServerStreamOnHighWater,
		__xwsServerStreamOnLowWater,
		NULL
	};
	return &tEvents;
}


static uint32 __Test_XWsServerOnHandshake(ptr pOwner, xwsserver* pServer, xwsconn* pConn,
	const xcodechttp1msg* pRequest, xwshandshakeresponse* pResponse)
{
	static const char sDenied[] = "policy denied";
	__test_xws_server_ctx* pCtx = (__test_xws_server_ctx*)pOwner;
	const char* sHeader;
	bool bReject;
	(void)pServer;
	(void)pConn;
	if ( !pCtx || !pRequest || !pResponse || !pResponse->pHeaders ) { return XWS_HANDSHAKE_ERROR; }
	__Test_XWsAtomicInc(&pCtx->iHandshakeCount);
	sHeader = xrtCodecHttp1GetHeader(pRequest, "X-Test-Handshake");
	if ( sHeader && strcmp(sHeader, "policy-token") == 0 ) {
		__Test_XWsAtomicInc(&pCtx->iHandshakeHeaderCount);
	}
	bReject = __xrtTestAtomicCompareExchangeLong(&pCtx->iRejectNext, 0, 1) == 1;
	pResponse->sProtocol = "chat.v1";
	pResponse->pConnectionData = pCtx;
	if ( xrtHttpHeadersSet(pResponse->pHeaders, "X-WS-Policy",
		bReject ? "rejected" : "accepted") != XHTTP_SEMANTIC_OK ) {
		return XWS_HANDSHAKE_ERROR;
	}
	if ( bReject ) {
		pResponse->iStatusCode = 403u;
		pResponse->sReason = "Forbidden";
		pResponse->pBody = sDenied;
		pResponse->iBodyLen = sizeof(sDenied) - 1u;
		return XWS_HANDSHAKE_REJECT;
	}
	return XWS_HANDSHAKE_ACCEPT;
}


static xnet_result __Test_XWsSendFrameRaw(xnetstream* pStream, bool bFin, bool bMask,
	bool bRsv1, uint8 iOpcode, const void* pData, size_t iLen)
{
	char* pFrame = NULL;
	size_t iFrameLen = 0u;
	xnet_result iResult;
	if ( !pStream || !__xwsBuildFrameBytesEx2(iOpcode, bFin, bMask, bRsv1,
		pData, iLen, &pFrame, &iFrameLen) ) { return XRT_NET_ERROR; }
	iResult = xrtNetStreamSend(pStream, pFrame, iFrameLen);
	XNET_FREE(pFrame);
	return iResult;
}


static void __Test_XWsReleaseRef(ptr pCtx, const void* pData, size_t iLen)
{
	(void)iLen;
	if ( pCtx ) { __Test_XWsAtomicInc((volatile long*)pCtx); }
	XNET_FREE((void*)pData);
}


#if XWS_HAS_PERMESSAGE_DEFLATE
static bool __Test_XWsPmdRoundTrip(void)
{
	char aInput[1024];
	memset(aInput, 'P', sizeof(aInput));
	for ( int i = 0; i < 2; ++i ) {
		void* pCompressed = NULL;
		void* pInflated = NULL;
		size_t iCompressedLen = 0u;
		size_t iInflatedLen = 0u;
		bool bPass = __xdeflatePmdMessage(aInput, sizeof(aInput), 6,
			&pCompressed, &iCompressedLen) && iCompressedLen < sizeof(aInput) &&
			__xinflatePmdMessage(pCompressed, iCompressedLen, sizeof(aInput),
				&pInflated, &iInflatedLen) == __XINFLATE_OK &&
			iInflatedLen == sizeof(aInput) && memcmp(pInflated, aInput, sizeof(aInput)) == 0;
		XNET_FREE(pCompressed);
		XNET_FREE(pInflated);
		if ( !bPass ) { return false; }
	}
	return true;
}
#endif


static void __Test_XWsServerOnCloseEx(ptr pOwner, xwsserver* pServer, xwsconn* pConn, const xwscloseinfo* pInfo)
{
	__test_xws_server_ctx* pCtx = (__test_xws_server_ctx*)pOwner;
	size_t iCopy;
	(void)pServer;
	(void)pConn;
	if ( !pCtx || !pInfo ) return;
	pCtx->iCloseFlags = pInfo->iFlags;
	pCtx->iLocalCloseCode = pInfo->iLocalCode;
	pCtx->iRemoteCloseCode = pInfo->iRemoteCode;
	iCopy = pInfo->iRemoteReasonLen < XWS_CLOSE_REASON_CAP ? pInfo->iRemoteReasonLen : XWS_CLOSE_REASON_CAP;
	if ( iCopy > 0u ) memcpy(pCtx->aRemoteCloseReason, pInfo->sRemoteReason, iCopy);
	pCtx->aRemoteCloseReason[iCopy] = '\0';
	__Test_XWsAtomicInc(&pCtx->iCloseExCount);
}


static void __Test_XWsServerOnBackpressure(ptr pOwner, xwsserver* pServer, xwsconn* pConn, size_t iPendingBytes)
{
	__test_xws_server_ctx* pCtx = (__test_xws_server_ctx*)pOwner;
	(void)pServer; (void)pConn; (void)iPendingBytes;
	if ( pCtx ) __Test_XWsAtomicInc(&pCtx->iBackpressureCount);
}


static void __Test_XWsServerOnWritable(ptr pOwner, xwsserver* pServer, xwsconn* pConn, size_t iPendingBytes)
{
	__test_xws_server_ctx* pCtx = (__test_xws_server_ctx*)pOwner;
	(void)pServer; (void)pConn; (void)iPendingBytes;
	if ( pCtx ) __Test_XWsAtomicInc(&pCtx->iWritableCount);
}


static void __Test_XWsServerOnDrain(ptr pOwner, xwsserver* pServer, xwsconn* pConn)
{
	__test_xws_server_ctx* pCtx = (__test_xws_server_ctx*)pOwner;
	(void)pServer; (void)pConn;
	if ( pCtx ) __Test_XWsAtomicInc(&pCtx->iDrainCount);
}


// 内部函数：__Test_XWsServerOnError
static void __Test_XWsServerOnError(ptr pOwner, xwsserver* pServer, xwsconn* pConn, int iSysErr)
{
	__test_xws_server_ctx* pCtx = (__test_xws_server_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	(void)iSysErr;
	if ( !pCtx ) return;
	__Test_XWsAtomicInc(&pCtx->iErrorCount);
}


// 内部函数：__Test_XWsServerOnPing
static void __Test_XWsServerOnPing(ptr pOwner, xwsserver* pServer, xwsconn* pConn, const void* pData, size_t iLen)
{
	__test_xws_server_ctx* pCtx = (__test_xws_server_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	(void)pData;
	(void)iLen;
	if ( !pCtx ) return;
	__Test_XWsAtomicInc(&pCtx->iPingCount);
}


// 内部函数：__Test_XWsServerOnPong
static void __Test_XWsServerOnPong(ptr pOwner, xwsserver* pServer, xwsconn* pConn, const void* pData, size_t iLen)
{
	__test_xws_server_ctx* pCtx = (__test_xws_server_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	(void)pData;
	(void)iLen;
	if ( !pCtx ) return;
	__Test_XWsAtomicInc(&pCtx->iPongCount);
}


// 内部函数：__Test_XWsClientOnOpen
static void __Test_XWsClientOnOpen(ptr pOwner, xwsclient* pClient)
{
	__test_xws_client_ctx* pCtx = (__test_xws_client_ctx*)pOwner;
	(void)pClient;
	if ( !pCtx ) return;
	__Test_XWsAtomicInc(&pCtx->iOpenCount);
}


// 内部函数：__Test_XWsClientOnText
static void __Test_XWsClientOnText(ptr pOwner, xwsclient* pClient, const char* pData, size_t iLen)
{
	__test_xws_client_ctx* pCtx = (__test_xws_client_ctx*)pOwner;
	size_t iCopy;
	(void)pClient;
	if ( !pCtx || !pData ) return;
	iCopy = iLen < (sizeof(pCtx->aLastText) - 1u) ? iLen : (sizeof(pCtx->aLastText) - 1u);
	memcpy(pCtx->aLastText, pData, iCopy);
	pCtx->aLastText[iCopy] = '\0';
	pCtx->iLastTextLen = iLen;
	__Test_XWsAtomicInc(&pCtx->iTextCount);
}


// 内部函数：__Test_XWsClientOnBinary
static void __Test_XWsClientOnBinary(ptr pOwner, xwsclient* pClient, const void* pData, size_t iLen)
{
	__test_xws_client_ctx* pCtx = (__test_xws_client_ctx*)pOwner;
	size_t iCopy;
	(void)pClient;
	if ( !pCtx || (!pData && iLen != 0u) ) return;
	iCopy = iLen < sizeof(pCtx->aLastBinary) ? iLen : sizeof(pCtx->aLastBinary);
	if ( iCopy > 0 ) memcpy(pCtx->aLastBinary, pData, iCopy);
	pCtx->iLastBinaryLen = iCopy;
	pCtx->iLastBinaryMessageLen = iLen;
	__Test_XWsAtomicInc(&pCtx->iBinaryCount);
}


// 内部函数：__Test_XWsClientOnClose
static void __Test_XWsClientOnClose(ptr pOwner, xwsclient* pClient, xnet_result iReason)
{
	__test_xws_client_ctx* pCtx = (__test_xws_client_ctx*)pOwner;
	(void)pClient;
	(void)iReason;
	if ( !pCtx ) return;
	__Test_XWsAtomicInc(&pCtx->iCloseCount);
}


static bool __Test_XWsClientOnHandshakeResponse(ptr pOwner, xwsclient* pClient,
	const xcodechttp1msg* pResponse)
{
	__test_xws_client_ctx* pCtx = (__test_xws_client_ctx*)pOwner;
	const char* sHeader;
	(void)pClient;
	if ( !pCtx || !pResponse ) { return false; }
	pCtx->iHandshakeStatus = pResponse->iStatusCode;
	__Test_XWsAtomicInc(&pCtx->iHandshakeResponseCount);
	sHeader = xrtCodecHttp1GetHeader(pResponse, "X-WS-Policy");
	if ( sHeader && (strcmp(sHeader, "accepted") == 0 || strcmp(sHeader, "rejected") == 0) ) {
		__Test_XWsAtomicInc(&pCtx->iHandshakeHeaderCount);
	}
	return true;
}


static bool __Test_XWsHttpdOnRequest(ptr pOwner, xhttpdserver* pHttpd,
	xhttpdconn* pConn, const xhttpdrequest* pRequest, xhttpdresponse* pResponse)
{
	__test_xws_httpd_ctx* pCtx = (__test_xws_httpd_ctx*)pOwner;
	xwsconn* pWsConn = NULL;
	xnet_result iResult;
	(void)pHttpd;
	(void)pResponse;
	if ( !pCtx || !pCtx->pWsServer || !pConn || !pRequest ) { return false; }
	__Test_XWsAtomicInc(&pCtx->iRequestCount);
	iResult = xrtWsServerUpgradeHttpd(pCtx->pWsServer, pConn, pRequest, &pWsConn);
	if ( iResult == XRT_NET_OK && pWsConn ) { __Test_XWsAtomicInc(&pCtx->iAcceptedCount); }
	return iResult == XRT_NET_OK;
}


static void __Test_XWsClientOnCloseEx(ptr pOwner, xwsclient* pClient, const xwscloseinfo* pInfo)
{
	__test_xws_client_ctx* pCtx = (__test_xws_client_ctx*)pOwner;
	size_t iCopy;
	(void)pClient;
	if ( !pCtx || !pInfo ) return;
	pCtx->iCloseFlags = pInfo->iFlags;
	pCtx->iLocalCloseCode = pInfo->iLocalCode;
	pCtx->iRemoteCloseCode = pInfo->iRemoteCode;
	iCopy = pInfo->iRemoteReasonLen < XWS_CLOSE_REASON_CAP ? pInfo->iRemoteReasonLen : XWS_CLOSE_REASON_CAP;
	if ( iCopy > 0u ) memcpy(pCtx->aRemoteCloseReason, pInfo->sRemoteReason, iCopy);
	pCtx->aRemoteCloseReason[iCopy] = '\0';
	__Test_XWsAtomicInc(&pCtx->iCloseExCount);
}


static void __Test_XWsClientOnBackpressure(ptr pOwner, xwsclient* pClient, size_t iPendingBytes)
{
	__test_xws_client_ctx* pCtx = (__test_xws_client_ctx*)pOwner;
	(void)pClient; (void)iPendingBytes;
	if ( pCtx ) __Test_XWsAtomicInc(&pCtx->iBackpressureCount);
}


static void __Test_XWsClientOnWritable(ptr pOwner, xwsclient* pClient, size_t iPendingBytes)
{
	__test_xws_client_ctx* pCtx = (__test_xws_client_ctx*)pOwner;
	(void)pClient; (void)iPendingBytes;
	if ( pCtx ) __Test_XWsAtomicInc(&pCtx->iWritableCount);
}


static void __Test_XWsClientOnDrain(ptr pOwner, xwsclient* pClient)
{
	__test_xws_client_ctx* pCtx = (__test_xws_client_ctx*)pOwner;
	(void)pClient;
	if ( pCtx ) __Test_XWsAtomicInc(&pCtx->iDrainCount);
}


// 内部函数：__Test_XWsClientOnError
static void __Test_XWsClientOnError(ptr pOwner, xwsclient* pClient, int iSysErr)
{
	__test_xws_client_ctx* pCtx = (__test_xws_client_ctx*)pOwner;
	(void)pClient;
	(void)iSysErr;
	if ( !pCtx ) return;
	__Test_XWsAtomicInc(&pCtx->iErrorCount);
}


// 内部函数：__Test_XWsClientOnPing
static void __Test_XWsClientOnPing(ptr pOwner, xwsclient* pClient, const void* pData, size_t iLen)
{
	__test_xws_client_ctx* pCtx = (__test_xws_client_ctx*)pOwner;
	(void)pClient;
	(void)pData;
	(void)iLen;
	if ( !pCtx ) return;
	__Test_XWsAtomicInc(&pCtx->iPingCount);
}


// 内部函数：__Test_XWsClientOnPong
static void __Test_XWsClientOnPong(ptr pOwner, xwsclient* pClient, const void* pData, size_t iLen)
{
	__test_xws_client_ctx* pCtx = (__test_xws_client_ctx*)pOwner;
	(void)pClient;
	(void)pData;
	(void)iLen;
	if ( !pCtx ) return;
	__Test_XWsAtomicInc(&pCtx->iPongCount);
}


// XNETWebSocket测试
static int __Test_XWsTrackedPrintf(int* pFailCount, const char* sFormat, ...)
{
	char aBuf[4096];
	va_list tArgs;
	int iRet;
	va_start(tArgs, sFormat);
	iRet = vsnprintf(aBuf, sizeof(aBuf), sFormat, tArgs);
	va_end(tArgs);
	if ( pFailCount && strstr(aBuf, "FAIL") != NULL ) { (*pFailCount)++; }
	fputs(aBuf, stdout);
	return iRet;
}


int Test_XNet_Ws(void)
{
	int iFailCount = 0;
#define printf(...) __Test_XWsTrackedPrintf(&iFailCount, __VA_ARGS__)
	printf("\n\n\n------------------------------------\n\n XNet WebSocket Skeleton Test:\n\n");

	{
		xwsclientconfig tConfig;
		xwsclientconfig tClone;
		xwsserverconfig tServerConfig;
		xwsserverconfig tServerClone;
		xwsclientevents tClientEvents;
		xwsserverevents tServerEvents;
		xwshandshakeresponse tHandshakeResponse;
		xwsclient tClient;
		char sURL[6200];
		char sOrigin[600];
		char sProtocols[600];
		char sServerProtocol[300];
		char* pHandshake = NULL;
		size_t iHandshakeLen = 0u;
		size_t iOffset;
		bool bPass;
		memcpy(sURL, "ws://example.com/", 17u);
		memset(sURL + 17u, 'p', 6000u);
		sURL[6017u] = '\0';
		memcpy(sOrigin, "https://", 8u);
		memset(sOrigin + 8u, 'o', 560u);
		sOrigin[568u] = '\0';
		iOffset = 0u;
		for ( uint32 i = 0u; i < 40u; ++i ) {
			int iWritten = snprintf(sProtocols + iOffset, sizeof(sProtocols) - iOffset,
				"%sproto%u", i == 0u ? "" : ", ", (unsigned)i);
			iOffset += (size_t)iWritten;
		}
		memset(sServerProtocol, 's', sizeof(sServerProtocol) - 1u);
		sServerProtocol[sizeof(sServerProtocol) - 1u] = '\0';
		xrtWsClientConfigInit(&tConfig);
		xrtWsClientEventsInit(&tClientEvents);
		xrtWsServerEventsInit(&tServerEvents);
		printf("  WS public structure versions : %s\n",
			tConfig.iSize == XWS_CLIENT_CONFIG_V3_SIZE && tConfig.iVersion == XWS_CLIENT_CONFIG_VERSION &&
			tClientEvents.iSize == XWS_CLIENT_EVENTS_V2_SIZE && tClientEvents.iVersion == XWS_CLIENT_EVENTS_VERSION &&
			tServerEvents.iSize == XWS_SERVER_EVENTS_V2_SIZE && tServerEvents.iVersion == XWS_SERVER_EVENTS_VERSION &&
			XWS_CLIENT_CONFIG_V1_SIZE < XWS_CLIENT_CONFIG_V2_SIZE &&
			XWS_CLIENT_CONFIG_V2_SIZE < XWS_CLIENT_CONFIG_V3_SIZE &&
			XWS_SERVER_CONFIG_V1_SIZE < XWS_SERVER_CONFIG_V2_SIZE &&
			XWS_SERVER_CONFIG_V2_SIZE < XWS_SERVER_CONFIG_V3_SIZE &&
			XWS_CLIENT_EVENTS_V1_SIZE < XWS_CLIENT_EVENTS_V2_SIZE &&
			XWS_SERVER_EVENTS_V1_SIZE < XWS_SERVER_EVENTS_V2_SIZE ? "PASS" : "FAIL");
		{
			xwsclientconfig tLegacy;
			xwsclientconfig tUpgraded;
			xwsserverconfig tLegacyServer;
			xwsserverconfig tUpgradedServer;
			xrtWsClientConfigInit(&tLegacy);
			xrtWsServerConfigInit(&tLegacyServer);
			memset(&tUpgraded, 0, sizeof(tUpgraded));
			memset(&tUpgradedServer, 0, sizeof(tUpgradedServer));
			tLegacy.iSize = XWS_CLIENT_CONFIG_V2_SIZE;
			tLegacy.iVersion = 2u;
			tLegacyServer.iSize = XWS_SERVER_CONFIG_V2_SIZE;
			tLegacyServer.iVersion = 2u;
			bPass = __xwsClientConfigClone(&tUpgraded, &tLegacy) &&
				__xwsServerConfigClone(&tUpgradedServer, &tLegacyServer) &&
				tUpgraded.iVersion == XWS_CLIENT_CONFIG_VERSION &&
				tUpgraded.iMaxFrameBytes == 1024u * 1024u &&
				tUpgraded.iHandshakeMaxBytes == XCODEC_HTTP1_DEFAULT_HEADER_BYTES &&
				tUpgraded.iHandshakeTimeoutMs == 10000u && tUpgraded.iCloseTimeoutMs == 5000u &&
				tUpgradedServer.iVersion == XWS_SERVER_CONFIG_VERSION &&
				tUpgradedServer.iMaxFrameBytes == 1024u * 1024u &&
				tUpgradedServer.iHandshakeMaxBytes == XCODEC_HTTP1_DEFAULT_HEADER_BYTES &&
				tUpgradedServer.iHandshakeTimeoutMs == 10000u &&
				tUpgradedServer.iCloseTimeoutMs == 5000u;
			printf("  WS V2 config upgrades to V3 defaults : %s\n", bPass ? "PASS" : "FAIL");
			xrtWsClientConfigUnit(&tLegacy);
			xrtWsServerConfigUnit(&tLegacyServer);
			xrtWsClientConfigUnit(&tUpgraded);
			xrtWsServerConfigUnit(&tUpgradedServer);
		}
		bPass = xrtWsHandshakeResponseInit(&tHandshakeResponse) &&
			tHandshakeResponse.iSize == XWS_HANDSHAKE_RESPONSE_V1_SIZE &&
			tHandshakeResponse.iStatusCode == 101u && tHandshakeResponse.pHeaders != NULL;
		printf("  WS handshake response lifecycle : %s\n", bPass ? "PASS" : "FAIL");
		xrtWsHandshakeResponseUnit(&tHandshakeResponse);
		bPass = xrtWsClientConfigSetURL(&tConfig, sURL) &&
			xrtWsClientConfigSetOrigin(&tConfig, sOrigin) &&
			xrtWsClientConfigSetProtocols(&tConfig, sProtocols) &&
			xrtWsClientConfigSetHeader(&tConfig, "X-Test-Config", "one") &&
			xrtWsClientConfigAddHeader(&tConfig, "X-Test-Config", "two") &&
			strlen(xrtWsClientConfigURL(&tConfig)) == strlen(sURL) &&
			strlen(xrtWsClientConfigOrigin(&tConfig)) == strlen(sOrigin) &&
			strcmp(xrtWsClientConfigProtocols(&tConfig), sProtocols) == 0 &&
			xrtHttpHeadersCount(xrtWsClientConfigHeaders(&tConfig)) == 2u;
		printf("  WS dynamic URL Origin and protocol config : %s\n", bPass ? "PASS" : "FAIL");
		bPass = __xwsClientConfigClone(&tClone, &tConfig);
		xrtWsClientConfigUnit(&tConfig);
		memset(&tClient, 0, sizeof(tClient));
		if ( bPass ) {
			tClient.tConfig = tClone;
			memset(&tClone, 0, sizeof(tClone));
			bPass = __xwsParseURL(__xwsClientConfigURLValue(&tClient.tConfig), &tClient.tURL) &&
				__xwsBuildClientHandshake(&tClient, &pHandshake, &iHandshakeLen) && pHandshake &&
				strstr(pHandshake, sOrigin) != NULL && strstr(pHandshake, sProtocols) != NULL &&
				strstr(pHandshake, "X-Test-Config: one\r\n") != NULL &&
				strstr(pHandshake, "X-Test-Config: two\r\n") != NULL &&
				iHandshakeLen > strlen(sURL);
		}
		printf("  WS dynamic config deep clone and handshake : %s\n", bPass ? "PASS" : "FAIL");
		if ( pHandshake ) { XNET_FREE(pHandshake); }
		xrtWsClientConfigUnit(&tClient.tConfig);
		__xwsURLUnit(&tClient.tURL);
		xrtWsServerConfigInit(&tServerConfig);
		#if XWS_HAS_PERMESSAGE_DEFLATE
			printf("  WS PMD no-context repeated roundtrip : %s\n",
				__Test_XWsPmdRoundTrip() ? "PASS" : "FAIL");
		#endif
		printf("  WS send budget validation : %s\n",
			__xwsValidSendBudget(1024u, 256u, 2048u) &&
			!__xwsValidSendBudget(0u, 0u, 512u) &&
			!__xwsValidSendBudget(800u, 256u, 1024u) &&
			!__xwsValidSendBudget(4096u, 256u, 2048u) &&
			!__xwsValidSendBudget(1024u, 2048u, 4096u) ? "PASS" : "FAIL");
		{
			xnetstream tBudgetStream;
			memset(&tBudgetStream, 0, sizeof(tBudgetStream));
			tBudgetStream.iMaxQueuedBytes = 4096u;
			tBudgetStream.iControlReserveBytes = __XWS_CONTROL_SEND_RESERVE_BYTES;
			bPass = __xnetStreamTryReserveSend(&tBudgetStream, 3584u) &&
				!__xnetStreamTryReserveSend(&tBudgetStream, 1u) &&
				__xnetStreamTryReserveControlSend(&tBudgetStream, 139u) &&
				__xnetStreamTryReserveControlSend(&tBudgetStream, 373u) &&
				!__xnetStreamTryReserveControlSend(&tBudgetStream, 1u) &&
				__xnetStreamReservedSendBytes(&tBudgetStream) == 4096u;
			__xnetStreamReleaseSend(&tBudgetStream, 4096u);
			printf("  WS control frames retain reserved send budget : %s\n",
				bPass && __xnetStreamReservedSendBytes(&tBudgetStream) == 0u ? "PASS" : "FAIL");
		}
		{
			static const char sRepeatedProtocols[] =
				"GET /chat HTTP/1.1\r\n"
				"Host: example.test\r\n"
				"Upgrade: websocket\r\n"
				"Connection: Upgrade\r\n"
				"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
				"Sec-WebSocket-Version: 13\r\n"
				"Sec-WebSocket-Protocol: alpha\r\n"
				"Sec-WebSocket-Protocol: beta, chat.v1\r\n\r\n";
			xnetchain tChain;
			xcodecframe tFrame;
			xcodechttp1msg tMessage;
			bool bHasProtocols = false;
			memset(&tFrame, 0, sizeof(tFrame));
			memset(&tMessage, 0, sizeof(tMessage));
			xrtNetChainInit(&tChain);
			bPass = xrtNetChainAppendCopy(&tChain, sRepeatedProtocols,
				sizeof(sRepeatedProtocols) - 1u) &&
				xrtCodecHttp1ParseHead(&tChain, &tFrame, &tMessage) == XCODEC_STATUS_FRAME &&
				__xwsProtocolHeadersValid(&tMessage, &bHasProtocols) && bHasProtocols &&
				__xwsProtocolHeadersContain(&tMessage, "alpha") &&
				__xwsProtocolHeadersContain(&tMessage, "chat.v1") &&
				!__xwsProtocolHeadersContain(&tMessage, "chat");
			printf("  WS repeated protocol header fields : %s\n", bPass ? "PASS" : "FAIL");
			xrtCodecHttp1MessageUnit(&tMessage);
			xrtNetChainClear(&tChain);
		}
		{
			uint32 iSavedSize = tServerConfig.iSize;
			tServerConfig.iSize = XWS_SERVER_CONFIG_V1_SIZE - 1u;
			printf("  WS truncated config rejected : %s\n",
				!__xwsServerConfigClone(&tServerClone, &tServerConfig) ? "PASS" : "FAIL");
			tServerConfig.iSize = iSavedSize;
		}
		printf("  WS dynamic server protocol config : %s\n",
			xrtWsServerConfigSetProtocol(&tServerConfig, sServerProtocol) &&
			strcmp(xrtWsServerConfigProtocol(&tServerConfig), sServerProtocol) == 0 &&
			xrtWsServerConfigSetPath(&tServerConfig, "/chat") &&
			xrtWsServerConfigSetOrigin(&tServerConfig, "https://example.test") &&
			strcmp(xrtWsServerConfigPath(&tServerConfig), "/chat") == 0 &&
			strcmp(xrtWsServerConfigOrigin(&tServerConfig), "https://example.test") == 0 ? "PASS" : "FAIL");
		printf("  WS config rejects injection and invalid protocol : %s\n",
			!xrtWsClientConfigSetOrigin(&tConfig, "https://x\r\nInjected: yes") &&
			!xrtWsClientConfigSetProtocols(&tConfig, "chat,,bad") &&
			!xrtWsClientConfigSetHeader(&tConfig, "Host", "example.test") &&
			!xrtWsClientConfigSetHeader(&tConfig, "Sec-WebSocket-Key", "bad") &&
			!xrtWsServerConfigSetProtocol(&tServerConfig, "bad protocol") &&
			!xrtWsServerConfigSetPath(&tServerConfig, "/chat?bad=1") &&
			!xrtWsServerConfigSetOrigin(&tServerConfig, "https://x\r\nInjected: yes") ? "PASS" : "FAIL");
		xrtWsServerConfigUnit(&tServerConfig);
	}

	{
		__test_xws_server_ctx tSrvCtx;
		__test_xws_client_ctx tCliCtx;
		xwsserverevents tSrvEvents;
		xwsclientevents tCliEvents;
		xwsserverconfig tSrvCfg;
		xwsclientconfig tCliCfg;
		xnetengineconfig tEngineCfg;
		xnetengine* pServerEngine = NULL;
		xnetengine* pClientEngine = NULL;
		xwsserver* pServer = NULL;
		xwsclient* pClient = NULL;
		xwsconn* pRetainedConn = NULL;
		xnetfuture* pClientCloseFuture = NULL;
		xnetfuture* pConnCloseFuture = NULL;
		xnetfuture* pClientOpenFuture = NULL;
		uint8 aBinary[] = { 1u, 2u, 3u, 4u };
		char sURL[256];

		memset(&tSrvCtx, 0, sizeof(tSrvCtx));
		memset(&tCliCtx, 0, sizeof(tCliCtx));
		xrtWsServerEventsInit(&tSrvEvents);
		xrtWsClientEventsInit(&tCliEvents);
		tSrvEvents.OnOpen = __Test_XWsServerOnOpen;
		tSrvEvents.OnText = __Test_XWsServerOnText;
		tSrvEvents.OnBinary = __Test_XWsServerOnBinary;
		tSrvEvents.OnClose = __Test_XWsServerOnClose;
		tSrvEvents.OnError = __Test_XWsServerOnError;
		tSrvEvents.OnPing = __Test_XWsServerOnPing;
		tSrvEvents.OnPong = __Test_XWsServerOnPong;
		tSrvEvents.OnBackpressure = __Test_XWsServerOnBackpressure;
		tSrvEvents.OnWritable = __Test_XWsServerOnWritable;
		tSrvEvents.OnDrain = __Test_XWsServerOnDrain;
		tSrvEvents.OnCloseEx = __Test_XWsServerOnCloseEx;
		tSrvEvents.OnHandshake = __Test_XWsServerOnHandshake;
		tCliEvents.OnOpen = __Test_XWsClientOnOpen;
		tCliEvents.OnText = __Test_XWsClientOnText;
		tCliEvents.OnBinary = __Test_XWsClientOnBinary;
		tCliEvents.OnClose = __Test_XWsClientOnClose;
		tCliEvents.OnError = __Test_XWsClientOnError;
		tCliEvents.OnPing = __Test_XWsClientOnPing;
		tCliEvents.OnPong = __Test_XWsClientOnPong;
		tCliEvents.OnBackpressure = __Test_XWsClientOnBackpressure;
		tCliEvents.OnWritable = __Test_XWsClientOnWritable;
		tCliEvents.OnDrain = __Test_XWsClientOnDrain;
		tCliEvents.OnCloseEx = __Test_XWsClientOnCloseEx;
		tCliEvents.OnHandshakeResponse = __Test_XWsClientOnHandshakeResponse;

		xrtNetEngineConfigInit(&tEngineCfg);
		tEngineCfg.iWorkerCount = 1;
		pServerEngine = xrtNetEngineCreate(&tEngineCfg);
		pClientEngine = xrtNetEngineCreate(&tEngineCfg);
		printf("  WS plain server engine create : %s\n", pServerEngine != NULL ? "PASS" : "FAIL");
		printf("  WS plain client engine create : %s\n", pClientEngine != NULL ? "PASS" : "FAIL");
		if ( pServerEngine ) printf("  WS plain server engine start : %s\n", xrtNetEngineStart(pServerEngine) == XRT_NET_OK ? "PASS" : "FAIL");
		if ( pClientEngine ) printf("  WS plain client engine start : %s\n", xrtNetEngineStart(pClientEngine) == XRT_NET_OK ? "PASS" : "FAIL");

		xrtWsServerConfigInit(&tSrvCfg);
		tSrvCfg.iHighWater = 16u;
		tSrvCfg.iLowWater = 8u;
		tSrvCfg.iMaxQueuedBytes = 4096u;
		tSrvCfg.iRecvLimit = 2048u;
		tSrvCfg.iWebSocketFlags = XWS_F_PERMESSAGE_DEFLATE;
		tSrvCfg.iCompressMinBytes = 1u;
		(void)xrtNetAddrParse(&tSrvCfg.tBindAddr, "127.0.0.1", 0);
		(void)xrtWsServerConfigSetProtocol(&tSrvCfg, "chat.v1");
		(void)xrtWsServerConfigSetPath(&tSrvCfg, "/chat");
		(void)xrtWsServerConfigSetOrigin(&tSrvCfg, "https://example.test");
		pServer = (pServerEngine && pClientEngine) ? xrtWsServerCreate(pServerEngine, &tSrvCfg, &tSrvEvents, &tSrvCtx) : NULL;
		printf("  WS plain server create : %s\n", pServer != NULL ? "PASS" : "FAIL");
		printf("  WS plain server start : %s\n", pServer && xrtWsServerStart(pServer) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  WS plain bound port assigned : %s\n", pServer && xrtWsServerBoundPort(pServer) > 0 ? "PASS" : "FAIL");

		xrtWsClientConfigInit(&tCliCfg);
		tCliCfg.iHighWater = 16u;
		tCliCfg.iLowWater = 8u;
		tCliCfg.iMaxQueuedBytes = 4096u;
		tCliCfg.iRecvLimit = 2048u;
		tCliCfg.iWebSocketFlags = XWS_F_PERMESSAGE_DEFLATE;
		tCliCfg.iCompressMinBytes = 1u;
		snprintf(sURL, sizeof(sURL), "ws://127.0.0.1:%u/chat?mode=plain", (unsigned)xrtWsServerBoundPort(pServer));
		snprintf(tCliCfg.sURL, sizeof(tCliCfg.sURL), "%s", sURL);
		(void)xrtWsClientConfigSetProtocols(&tCliCfg, "other, chat.v1");
		(void)xrtWsClientConfigSetOrigin(&tCliCfg, "https://example.test");
		(void)xrtWsClientConfigSetHeader(&tCliCfg, "X-Test-Handshake", "policy-token");
		pClient = pClientEngine ? xrtWsClientCreate(pClientEngine, &tCliCfg, &tCliEvents, &tCliCtx) : NULL;
		printf("  WS plain client create : %s\n", pClient != NULL ? "PASS" : "FAIL");
		pClientOpenFuture = pClient ? xrtWsClientStartFuture(pClient) : NULL;
		printf("  WS plain client start future : %s\n", pClientOpenFuture != NULL ? "PASS" : "FAIL");
		printf("  WS open future resolves after Upgrade : %s\n",
			pClientOpenFuture && xrtNetFutureWait(pClientOpenFuture, 2000u) == XRT_NET_OK &&
			xrtNetFutureValue(pClientOpenFuture) == pClient ? "PASS" : "FAIL");
		if ( pClientOpenFuture ) { xrtNetFutureDestroy(pClientOpenFuture); pClientOpenFuture = NULL; }
		printf("  WS plain server open : %s\n", __Test_XWsWaitMin(&tSrvCtx.iOpenCount, 1, 2000u) ? "PASS" : "FAIL");
		printf("  WS plain client open : %s\n", __Test_XWsWaitMin(&tCliCtx.iOpenCount, 1, 2000u) ? "PASS" : "FAIL");
		printf("  WS plain client open state : %s\n", pClient && xrtWsClientIsOpen(pClient) ? "PASS" : "FAIL");
		printf("  WS open wait immediate after Upgrade : %s\n",
			pClient && xrtWsClientWaitOpenTimeout(pClient, 100u) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  WS plain server conn open state : %s\n", tSrvCtx.pLastConn && xrtWsConnIsOpen(tSrvCtx.pLastConn) ? "PASS" : "FAIL");
		printf("  WS handshake policy callbacks : %s\n",
			__Test_XWsAtomicLoad(&tSrvCtx.iHandshakeCount) == 1 &&
			__Test_XWsAtomicLoad(&tSrvCtx.iHandshakeHeaderCount) == 1 &&
			__Test_XWsAtomicLoad(&tCliCtx.iHandshakeResponseCount) == 1 &&
			__Test_XWsAtomicLoad(&tCliCtx.iHandshakeHeaderCount) == 1 &&
			tCliCtx.iHandshakeStatus == 101u ? "PASS" : "FAIL");
		printf("  WS negotiated protocol accessors : %s\n",
			pClient && tSrvCtx.pLastConn && strcmp(xrtWsClientProtocol(pClient), "chat.v1") == 0 &&
			strcmp(xrtWsConnProtocol(tSrvCtx.pLastConn), "chat.v1") == 0 ? "PASS" : "FAIL");
		printf("  WS handshake connection data : %s\n",
			tSrvCtx.pLastConn && xrtWsConnGetData(tSrvCtx.pLastConn) == &tSrvCtx ? "PASS" : "FAIL");
		printf("  WS PMD negotiated accessors : %s\n",
			pClient && tSrvCtx.pLastConn && xrtWsClientPerMessageDeflate(pClient) &&
			xrtWsConnPerMessageDeflate(tSrvCtx.pLastConn) ? "PASS" : "FAIL");
		pRetainedConn = tSrvCtx.pLastConn ? xrtWsConnRetain(tSrvCtx.pLastConn) : NULL;
		printf("  WS server connection retain : %s\n", pRetainedConn != NULL ? "PASS" : "FAIL");
		printf("  WS client send budget propagated : %s\n",
			pClient && pClient->pStream && pClient->pStream->iMaxQueuedBytes == 4096u &&
			pClient->pStream->tSendQ.iHighWater == 16u && pClient->pStream->tSendQ.iLowWater == 8u ? "PASS" : "FAIL");
		printf("  WS server send budget propagated : %s\n",
			tSrvCtx.pLastConn && tSrvCtx.pLastConn->pStream && tSrvCtx.pLastConn->pStream->iMaxQueuedBytes == 4096u &&
			tSrvCtx.pLastConn->pStream->tSendQ.iHighWater == 16u && tSrvCtx.pLastConn->pStream->tSendQ.iLowWater == 8u ? "PASS" : "FAIL");
		printf("  WS client backpressure callbacks : %s\n",
			__Test_XWsWaitMin(&tCliCtx.iBackpressureCount, 1, 2000u) &&
			__Test_XWsWaitMin(&tCliCtx.iWritableCount, 1, 2000u) &&
			__Test_XWsWaitMin(&tCliCtx.iDrainCount, 1, 2000u) ? "PASS" : "FAIL");
		printf("  WS server backpressure callbacks : %s\n",
			__Test_XWsWaitMin(&tSrvCtx.iBackpressureCount, 1, 2000u) &&
			__Test_XWsWaitMin(&tSrvCtx.iWritableCount, 1, 2000u) &&
			__Test_XWsWaitMin(&tSrvCtx.iDrainCount, 1, 2000u) ? "PASS" : "FAIL");
		{
			xnetfuture* pWritable = pClient ? xrtWsClientWritableFuture(pClient) : NULL;
			xnetfuture* pDrain = tSrvCtx.pLastConn ? xrtWsConnDrainFuture(tSrvCtx.pLastConn) : NULL;
			printf("  WS writable and drain futures : %s\n",
				pWritable && pDrain && xrtNetFutureWait(pWritable, 1000u) == XRT_NET_OK &&
				xrtNetFutureWait(pDrain, 1000u) == XRT_NET_OK ? "PASS" : "FAIL");
			if ( pWritable ) xrtNetFutureDestroy(pWritable);
			if ( pDrain ) xrtNetFutureDestroy(pDrain);
		}
		printf("  WS synchronous writable wait : %s\n",
			pClient && xrtWsClientWaitTimeoutEx(pClient, XNET_STREAM_WAIT_WRITABLE, 1000u) == XRT_NET_OK &&
			tSrvCtx.pLastConn && xrtWsConnWaitTimeoutEx(tSrvCtx.pLastConn, XNET_STREAM_WAIT_DRAIN, 1000u) == XRT_NET_OK ? "PASS" : "FAIL");

		printf("  WS plain send text : %s\n", pClient && xrtWsClientSendText(pClient, "hello-ws", 8u) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  WS plain server text callback : %s\n", __Test_XWsWaitMin(&tSrvCtx.iTextCount, 1, 2000u) ? "PASS" : "FAIL");
		printf("  WS plain client text echo : %s\n", __Test_XWsWaitMin(&tCliCtx.iTextCount, 1, 2000u) ? "PASS" : "FAIL");
		printf("  WS plain server saw text : %s\n", strcmp(tSrvCtx.aLastText, "hello-ws") == 0 ? "PASS" : "FAIL");
		printf("  WS plain client saw text : %s\n", strcmp(tCliCtx.aLastText, "hello-ws") == 0 ? "PASS" : "FAIL");

		printf("  WS plain send binary : %s\n", pClient && xrtWsClientSendBinary(pClient, aBinary, sizeof(aBinary)) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  WS plain server binary callback : %s\n", __Test_XWsWaitMin(&tSrvCtx.iBinaryCount, 1, 2000u) ? "PASS" : "FAIL");
		printf("  WS plain client binary echo : %s\n", __Test_XWsWaitMin(&tCliCtx.iBinaryCount, 1, 2000u) ? "PASS" : "FAIL");
		printf("  WS plain server saw binary : %s\n", tSrvCtx.iLastBinaryLen == sizeof(aBinary) && memcmp(tSrvCtx.aLastBinary, aBinary, sizeof(aBinary)) == 0 ? "PASS" : "FAIL");
		printf("  WS plain client saw binary : %s\n", tCliCtx.iLastBinaryLen == sizeof(aBinary) && memcmp(tCliCtx.aLastBinary, aBinary, sizeof(aBinary)) == 0 ? "PASS" : "FAIL");

		{
			char aLargeText[1024];
			uint8 aLargeBinary[1024];
			long iSrvTextBefore = __Test_XWsAtomicLoad(&tSrvCtx.iTextCount);
			long iCliTextBefore = __Test_XWsAtomicLoad(&tCliCtx.iTextCount);
			long iSrvBinaryBefore = __Test_XWsAtomicLoad(&tSrvCtx.iBinaryCount);
			long iCliBinaryBefore = __Test_XWsAtomicLoad(&tCliCtx.iBinaryCount);
			memset(aLargeText, 'T', sizeof(aLargeText));
			memset(aLargeBinary, 0x5Au, sizeof(aLargeBinary));
			printf("  WS PMD send repetitive text : %s\n",
				pClient && xrtWsClientSendText(pClient, aLargeText, sizeof(aLargeText)) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS PMD repetitive text roundtrip : %s\n",
				__Test_XWsWaitMin(&tSrvCtx.iTextCount, iSrvTextBefore + 1, 2000u) &&
				__Test_XWsWaitMin(&tCliCtx.iTextCount, iCliTextBefore + 1, 2000u) &&
				tSrvCtx.iLastTextLen == sizeof(aLargeText) && tCliCtx.iLastTextLen == sizeof(aLargeText) &&
				tSrvCtx.aLastText[0] == 'T' && tCliCtx.aLastText[0] == 'T' ? "PASS" : "FAIL");
			printf("  WS PMD send repetitive binary : %s\n",
				pClient && xrtWsClientSendBinary(pClient, aLargeBinary, sizeof(aLargeBinary)) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS PMD repetitive binary roundtrip : %s\n",
				__Test_XWsWaitMin(&tSrvCtx.iBinaryCount, iSrvBinaryBefore + 1, 2000u) &&
				__Test_XWsWaitMin(&tCliCtx.iBinaryCount, iCliBinaryBefore + 1, 2000u) &&
				tSrvCtx.iLastBinaryMessageLen == sizeof(aLargeBinary) &&
				tCliCtx.iLastBinaryMessageLen == sizeof(aLargeBinary) &&
				tSrvCtx.aLastBinary[0] == 0x5Au && tCliCtx.aLastBinary[0] == 0x5Au ? "PASS" : "FAIL");
		}

		printf("  WS plain send ping : %s\n", pClient && xrtWsClientPing(pClient, "hb", 2u) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  WS plain server ping callback : %s\n", __Test_XWsWaitMin(&tSrvCtx.iPingCount, 1, 2000u) ? "PASS" : "FAIL");
		printf("  WS plain client pong callback : %s\n", __Test_XWsWaitMin(&tCliCtx.iPongCount, 1, 2000u) ? "PASS" : "FAIL");

		{
			long iSrvTextBefore = __Test_XWsAtomicLoad(&tSrvCtx.iTextCount);
			long iSrvPingBefore = __Test_XWsAtomicLoad(&tSrvCtx.iPingCount);
			long iCliPongBefore = __Test_XWsAtomicLoad(&tCliCtx.iPongCount);
			long iCliTextBefore = __Test_XWsAtomicLoad(&tCliCtx.iTextCount);
			printf("  WS plain fragmented client text frame1 : %s\n", pClient && __xwsStreamSendFrameEx(pClient->pStream, false, true, XCODEC_WS_OPCODE_TEXT, "frag-", 5u) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS plain fragmented client ping : %s\n", pClient && __xwsStreamSendFrameEx(pClient->pStream, true, true, XCODEC_WS_OPCODE_PING, "f1", 2u) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS plain fragmented client text cont : %s\n", pClient && __xwsStreamSendFrameEx(pClient->pStream, true, true, XCODEC_WS_OPCODE_CONT, "ws", 2u) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS plain fragmented server text callback : %s\n", __Test_XWsWaitMin(&tSrvCtx.iTextCount, iSrvTextBefore + 1, 2000u) ? "PASS" : "FAIL");
			printf("  WS plain fragmented server ping callback : %s\n", __Test_XWsWaitMin(&tSrvCtx.iPingCount, iSrvPingBefore + 1, 2000u) ? "PASS" : "FAIL");
			printf("  WS plain fragmented client pong callback : %s\n", __Test_XWsWaitMin(&tCliCtx.iPongCount, iCliPongBefore + 1, 2000u) ? "PASS" : "FAIL");
			printf("  WS plain fragmented client text echo : %s\n", __Test_XWsWaitMin(&tCliCtx.iTextCount, iCliTextBefore + 1, 2000u) ? "PASS" : "FAIL");
			printf("  WS plain fragmented text reassembled : %s\n", strcmp(tSrvCtx.aLastText, "frag-ws") == 0 && strcmp(tCliCtx.aLastText, "frag-ws") == 0 ? "PASS" : "FAIL");
		}

		{
			static const uint8 aFragBinA[] = { 9u, 8u };
			static const uint8 aFragBinB[] = { 7u, 6u, 5u };
			static const uint8 aFragBinAll[] = { 9u, 8u, 7u, 6u, 5u };
			long iCliBinaryBefore = __Test_XWsAtomicLoad(&tCliCtx.iBinaryCount);
			long iCliPingBefore = __Test_XWsAtomicLoad(&tCliCtx.iPingCount);
			long iSrvPongBefore = __Test_XWsAtomicLoad(&tSrvCtx.iPongCount);
			printf("  WS plain fragmented server binary frame1 : %s\n", tSrvCtx.pLastConn && __xwsStreamSendFrameEx(tSrvCtx.pLastConn->pStream, false, false, XCODEC_WS_OPCODE_BINARY, aFragBinA, sizeof(aFragBinA)) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS plain fragmented server ping : %s\n", tSrvCtx.pLastConn && __xwsStreamSendFrameEx(tSrvCtx.pLastConn->pStream, true, false, XCODEC_WS_OPCODE_PING, "sv", 2u) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS plain fragmented server binary cont : %s\n", tSrvCtx.pLastConn && __xwsStreamSendFrameEx(tSrvCtx.pLastConn->pStream, true, false, XCODEC_WS_OPCODE_CONT, aFragBinB, sizeof(aFragBinB)) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS plain fragmented client ping callback : %s\n", __Test_XWsWaitMin(&tCliCtx.iPingCount, iCliPingBefore + 1, 2000u) ? "PASS" : "FAIL");
			printf("  WS plain fragmented server pong callback : %s\n", __Test_XWsWaitMin(&tSrvCtx.iPongCount, iSrvPongBefore + 1, 2000u) ? "PASS" : "FAIL");
			printf("  WS plain fragmented client binary callback : %s\n", __Test_XWsWaitMin(&tCliCtx.iBinaryCount, iCliBinaryBefore + 1, 2000u) ? "PASS" : "FAIL");
			printf("  WS plain fragmented binary reassembled : %s\n", tCliCtx.iLastBinaryLen == sizeof(aFragBinAll) && memcmp(tCliCtx.aLastBinary, aFragBinAll, sizeof(aFragBinAll)) == 0 ? "PASS" : "FAIL");
		}

		{
			static const uint8 aTextA[] = { 'w', 'r', '-', 0xE2u };
			static const uint8 aTextB[] = { 0x82u };
			static const uint8 aTextC[] = { 0xACu, '!' };
			static const uint8 aExpected[] = { 'w', 'r', '-', 0xE2u, 0x82u, 0xACu, '!' };
			xwswriter* pWriter;
			long iSrvTextBefore = __Test_XWsAtomicLoad(&tSrvCtx.iTextCount);
			long iCliTextBefore = __Test_XWsAtomicLoad(&tCliCtx.iTextCount);
			long iSrvPingBefore = __Test_XWsAtomicLoad(&tSrvCtx.iPingCount);
			(void)xrtWsClientWaitTimeoutEx(pClient, XNET_STREAM_WAIT_DRAIN, 1000u);
			pWriter = xrtWsClientBeginText(pClient);
			printf("  WS client streaming text writer begin : %s\n", pWriter ? "PASS" : "FAIL");
			printf("  WS writer excludes another data message : %s\n",
				pWriter && xrtWsClientSendBinary(pClient, "x", 1u) == XRT_NET_AGAIN ? "PASS" : "FAIL");
			printf("  WS writer AGAIN does not advance state : %s\n",
				pWriter && __xnetStreamTryReserveSend(pClient->pStream, 3584u) &&
				xrtWsWriterWrite(pWriter, aTextA, sizeof(aTextA)) == XRT_NET_AGAIN &&
				!pWriter->bStarted ? "PASS" : "FAIL");
			__xnetStreamReleaseSend(pClient->pStream, 3584u);
			printf("  WS writer retries same fragment : %s\n",
				pWriter && xrtWsWriterWrite(pWriter, aTextA, sizeof(aTextA)) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS control frame interleaves with writer : %s\n",
				xrtWsClientPing(pClient, "stream", 6u) == XRT_NET_OK &&
				xrtWsWriterWrite(pWriter, aTextB, sizeof(aTextB)) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS writer validates split UTF-8 and finishes : %s\n",
				xrtWsWriterFinish(pWriter, aTextC, sizeof(aTextC)) == XRT_NET_OK &&
				xrtWsWriterIsFinished(pWriter) ? "PASS" : "FAIL");
			xrtWsWriterDestroy(pWriter);
			printf("  WS streaming text roundtrip : %s\n",
				__Test_XWsWaitMin(&tSrvCtx.iTextCount, iSrvTextBefore + 1, 2000u) &&
				__Test_XWsWaitMin(&tCliCtx.iTextCount, iCliTextBefore + 1, 2000u) &&
				__Test_XWsWaitMin(&tSrvCtx.iPingCount, iSrvPingBefore + 1, 2000u) &&
				tSrvCtx.iLastTextLen == sizeof(aExpected) &&
				memcmp(tSrvCtx.aLastText, aExpected, sizeof(aExpected)) == 0 &&
				tCliCtx.iLastTextLen == sizeof(aExpected) &&
				memcmp(tCliCtx.aLastText, aExpected, sizeof(aExpected)) == 0 ? "PASS" : "FAIL");
		}

		{
			static const uint8 aBinaryA[] = { 0x10u, 0x20u };
			static const uint8 aBinaryB[] = { 0x30u, 0x40u, 0x50u };
			static const uint8 aExpected[] = { 0x10u, 0x20u, 0x30u, 0x40u, 0x50u };
			xwswriter* pWriter = tSrvCtx.pLastConn ? xrtWsConnBeginBinary(tSrvCtx.pLastConn) : NULL;
			long iCliBinaryBefore = __Test_XWsAtomicLoad(&tCliCtx.iBinaryCount);
			printf("  WS server streaming binary writer : %s\n",
				pWriter && xrtWsWriterWrite(pWriter, aBinaryA, sizeof(aBinaryA)) == XRT_NET_OK &&
				xrtWsConnSendText(tSrvCtx.pLastConn, "blocked", 7u) == XRT_NET_AGAIN &&
				xrtWsWriterFinish(pWriter, aBinaryB, sizeof(aBinaryB)) == XRT_NET_OK ? "PASS" : "FAIL");
			xrtWsWriterDestroy(pWriter);
			printf("  WS streaming binary delivered : %s\n",
				__Test_XWsWaitMin(&tCliCtx.iBinaryCount, iCliBinaryBefore + 1, 2000u) &&
				tCliCtx.iLastBinaryLen == sizeof(aExpected) &&
				memcmp(tCliCtx.aLastBinary, aExpected, sizeof(aExpected)) == 0 ? "PASS" : "FAIL");
		}

		#if XWS_HAS_PERMESSAGE_DEFLATE
		{
			char aFragmented[768];
			void* pCompressed = NULL;
			size_t iCompressedLen = 0u;
			long iSrvTextBefore = __Test_XWsAtomicLoad(&tSrvCtx.iTextCount);
			long iCliTextBefore = __Test_XWsAtomicLoad(&tCliCtx.iTextCount);
			long iSrvPingBefore = __Test_XWsAtomicLoad(&tSrvCtx.iPingCount);
			long iCliPongBefore = __Test_XWsAtomicLoad(&tCliCtx.iPongCount);
			memset(aFragmented, 'F', sizeof(aFragmented));
			printf("  WS PMD fragmented payload compress : %s\n",
				__xdeflatePmdMessage(aFragmented, sizeof(aFragmented), 6,
					&pCompressed, &iCompressedLen) && iCompressedLen >= 2u ? "PASS" : "FAIL");
			printf("  WS PMD fragmented first frame : %s\n",
				pClient && pCompressed && __Test_XWsSendFrameRaw(pClient->pStream, false, true, true,
					XCODEC_WS_OPCODE_TEXT, pCompressed, iCompressedLen / 2u) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS PMD fragmented interleaved ping : %s\n",
				pClient && xrtWsClientPing(pClient, "pmd", 3u) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS PMD fragmented continuation : %s\n",
				pClient && pCompressed && __Test_XWsSendFrameRaw(pClient->pStream, true, true, false,
					XCODEC_WS_OPCODE_CONT, (const uint8*)pCompressed + (iCompressedLen / 2u),
					iCompressedLen - (iCompressedLen / 2u)) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS PMD fragmented roundtrip : %s\n",
				__Test_XWsWaitMin(&tSrvCtx.iTextCount, iSrvTextBefore + 1, 2000u) &&
				__Test_XWsWaitMin(&tCliCtx.iTextCount, iCliTextBefore + 1, 2000u) &&
				__Test_XWsWaitMin(&tSrvCtx.iPingCount, iSrvPingBefore + 1, 2000u) &&
				__Test_XWsWaitMin(&tCliCtx.iPongCount, iCliPongBefore + 1, 2000u) &&
				tSrvCtx.iLastTextLen == sizeof(aFragmented) && tCliCtx.iLastTextLen == sizeof(aFragmented) &&
				tSrvCtx.aLastText[0] == 'F' && tCliCtx.aLastText[0] == 'F' ? "PASS" : "FAIL");
			XNET_FREE(pCompressed);
		}
		#endif

		pClientCloseFuture = pClient ? xrtWsClientCloseFuture(pClient) : NULL;
		pConnCloseFuture = tSrvCtx.pLastConn ? xrtWsConnCloseFuture(tSrvCtx.pLastConn) : NULL;
		printf("  WS plain close request : %s\n", pClient && xrtWsClientClose(pClient, XWS_CLOSE_NORMAL, "bye") == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  WS plain client close callback : %s\n", __Test_XWsWaitMin(&tCliCtx.iCloseCount, 1, 3000u) ? "PASS" : "FAIL");
		printf("  WS plain server close callback : %s\n", __Test_XWsWaitMin(&tSrvCtx.iCloseCount, 1, 3000u) ? "PASS" : "FAIL");
		printf("  WS close futures : %s\n",
			pClientCloseFuture && pConnCloseFuture && xrtNetFutureWait(pClientCloseFuture, 1000u) == XRT_NET_CLOSED &&
			xrtNetFutureWait(pConnCloseFuture, 1000u) == XRT_NET_CLOSED ? "PASS" : "FAIL");
		if ( pClientCloseFuture ) { xrtNetFutureDestroy(pClientCloseFuture); pClientCloseFuture = NULL; }
		if ( pConnCloseFuture ) { xrtNetFutureDestroy(pConnCloseFuture); pConnCloseFuture = NULL; }
		printf("  WS close metadata callbacks : %s\n",
			__Test_XWsWaitMin(&tCliCtx.iCloseExCount, 1, 1000u) && __Test_XWsWaitMin(&tSrvCtx.iCloseExCount, 1, 1000u) &&
			(tCliCtx.iCloseFlags & XWS_CLOSE_F_CLEAN) != 0u && (tSrvCtx.iCloseFlags & XWS_CLOSE_F_CLEAN) != 0u &&
			(tCliCtx.iCloseFlags & XWS_CLOSE_F_REMOTE_INITIATED) == 0u &&
			(tSrvCtx.iCloseFlags & XWS_CLOSE_F_REMOTE_INITIATED) != 0u &&
			tCliCtx.iLocalCloseCode == XWS_CLOSE_NORMAL && tCliCtx.iRemoteCloseCode == XWS_CLOSE_NORMAL &&
			tSrvCtx.iLocalCloseCode == XWS_CLOSE_NORMAL && tSrvCtx.iRemoteCloseCode == XWS_CLOSE_NORMAL &&
			strcmp(tSrvCtx.aRemoteCloseReason, "bye") == 0 ? "PASS" : "FAIL");
		{
			xwscloseinfo tClientInfo;
			xwscloseinfo tConnInfo;
			xrtWsCloseInfoInit(&tClientInfo);
			xrtWsCloseInfoInit(&tConnInfo);
			__Test_XWsSleepMs(50u);
			printf("  WS retained close metadata access : %s\n",
				pClient && xrtWsClientCloseInfo(pClient, &tClientInfo) && pRetainedConn &&
				xrtWsConnCloseInfo(pRetainedConn, &tConnInfo) &&
				(tClientInfo.iFlags & XWS_CLOSE_F_CLEAN) != 0u &&
				(tConnInfo.iFlags & XWS_CLOSE_F_CLEAN) != 0u &&
				strcmp(xrtWsConnProtocol(pRetainedConn), "chat.v1") == 0 ? "PASS" : "FAIL");
		}
		if ( pRetainedConn ) { xrtWsConnRelease(pRetainedConn); pRetainedConn = NULL; }
		printf("  WS plain client error free : %s\n", __Test_XWsAtomicLoad(&tCliCtx.iErrorCount) == 0 ? "PASS" : "FAIL");
		printf("  WS plain server error free : %s\n", __Test_XWsAtomicLoad(&tSrvCtx.iErrorCount) == 0 ? "PASS" : "FAIL");
		{
			long iServerOpenBefore = __Test_XWsAtomicLoad(&tSrvCtx.iOpenCount);
			long iServerCloseBefore = __Test_XWsAtomicLoad(&tSrvCtx.iCloseCount);
			long iClientOpenBefore = __Test_XWsAtomicLoad(&tCliCtx.iOpenCount);
			long iClientCloseBefore = __Test_XWsAtomicLoad(&tCliCtx.iCloseCount);
			xwscloseinfo tRestartInfo;
			xrtWsClientStop(pClient);
			pClientOpenFuture = xrtWsClientStartFuture(pClient);
			printf("  WS client object restart future : %s\n",
				pClientOpenFuture && xrtNetFutureWait(pClientOpenFuture, 2000u) == XRT_NET_OK &&
				__Test_XWsWaitMin(&tSrvCtx.iOpenCount, iServerOpenBefore + 1, 2000u) &&
				__Test_XWsWaitMin(&tCliCtx.iOpenCount, iClientOpenBefore + 1, 2000u) ? "PASS" : "FAIL");
			if ( pClientOpenFuture ) { xrtNetFutureDestroy(pClientOpenFuture); pClientOpenFuture = NULL; }
			xrtWsCloseInfoInit(&tRestartInfo);
			printf("  WS restart clears per-run close state : %s\n",
				xrtWsClientCloseInfo(pClient, &tRestartInfo) &&
				tRestartInfo.iTransportResult == XRT_NET_AGAIN && tRestartInfo.iFlags == 0u &&
				tRestartInfo.iLocalCode == 0u && tRestartInfo.iRemoteCode == 0u ? "PASS" : "FAIL");
			printf("  WS restarted client sends : %s\n",
				xrtWsClientSendText(pClient, "restart", 7u) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS restarted client closes cleanly : %s\n",
				xrtWsClientClose(pClient, XWS_CLOSE_NORMAL, "restart-done") == XRT_NET_OK &&
				__Test_XWsWaitMin(&tCliCtx.iCloseCount, iClientCloseBefore + 1, 3000u) &&
				__Test_XWsWaitMin(&tSrvCtx.iCloseCount, iServerCloseBefore + 1, 3000u) ? "PASS" : "FAIL");
		}
		{
			__test_xws_stall_ctx tRawCtx;
			xnetconnectconfig tRawCfg;
			xnetstream* pRawStream;
			long iServerCloseBefore = __Test_XWsAtomicLoad(&tSrvCtx.iCloseCount);
			memset(&tRawCtx, 0, sizeof(tRawCtx));
			pServer->tConfig.iHandshakeTimeoutMs = 50u;
			pRawStream = xrtNetStreamCreate(pClientEngine, __Test_XWsStallStreamEvents(), &tRawCtx);
			xrtNetConnectConfigInit(&tRawCfg);
			tRawCfg.sHost = "127.0.0.1";
			tRawCfg.iPort = xrtWsServerBoundPort(pServer);
			printf("  WS server handshake-timeout raw connect : %s\n",
				pRawStream && xrtNetStreamConnect(pRawStream, &tRawCfg) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS server aborts stalled Upgrade : %s\n",
				__Test_XWsWaitMin(&tSrvCtx.iCloseCount, iServerCloseBefore + 1, 2000u) &&
				__Test_XWsWaitMin(&tRawCtx.iCloseCount, 1, 2000u) ? "PASS" : "FAIL");
			pServer->tConfig.iHandshakeTimeoutMs = 10000u;
		}
		if ( pClient ) {
			xrtWsClientDestroy(pClient);
			pClient = NULL;
		}

		{
			__test_xws_client_ctx tCliCtx2;
			char aTooLarge[4096];
			long iSrvOpenBefore = __Test_XWsAtomicLoad(&tSrvCtx.iOpenCount);
			long iSrvCloseBefore = __Test_XWsAtomicLoad(&tSrvCtx.iCloseCount);
			long iSrvTextBefore = __Test_XWsAtomicLoad(&tSrvCtx.iTextCount);
			memset(&tCliCtx2, 0, sizeof(tCliCtx2));
			memset(aTooLarge, 'L', sizeof(aTooLarge));
			pClient = pClientEngine ? xrtWsClientCreate(pClientEngine, &tCliCfg, &tCliEvents, &tCliCtx2) : NULL;
			printf("  WS plain reaccept client create : %s\n", pClient != NULL ? "PASS" : "FAIL");
			printf("  WS plain reaccept client start : %s\n", pClient && xrtWsClientStart(pClient) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS plain reaccept server open : %s\n", __Test_XWsWaitMin(&tSrvCtx.iOpenCount, iSrvOpenBefore + 1, 2000u) ? "PASS" : "FAIL");
			printf("  WS plain reaccept client open : %s\n", __Test_XWsWaitMin(&tCliCtx2.iOpenCount, 1, 2000u) ? "PASS" : "FAIL");
			printf("  WS plain reaccept send text : %s\n", pClient && xrtWsClientSendText(pClient, "again-ws", 8u) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS plain reaccept server text callback : %s\n", __Test_XWsWaitMin(&tSrvCtx.iTextCount, iSrvTextBefore + 1, 2000u) ? "PASS" : "FAIL");
			printf("  WS plain reaccept client text echo : %s\n", __Test_XWsWaitMin(&tCliCtx2.iTextCount, 1, 2000u) ? "PASS" : "FAIL");
			printf("  WS plain reaccept echoed text : %s\n", strcmp(tCliCtx2.aLastText, "again-ws") == 0 && strcmp(tSrvCtx.aLastText, "again-ws") == 0 ? "PASS" : "FAIL");
			printf("  WS PMD decompressed limit send : %s\n",
				pClient && xrtWsClientSendText(pClient, aTooLarge, sizeof(aTooLarge)) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS plain reaccept client close callback : %s\n", __Test_XWsWaitMin(&tCliCtx2.iCloseCount, 1, 3000u) ? "PASS" : "FAIL");
			printf("  WS plain reaccept server close callback : %s\n", __Test_XWsWaitMin(&tSrvCtx.iCloseCount, iSrvCloseBefore + 1, 3000u) ? "PASS" : "FAIL");
			printf("  WS PMD decompressed limit close code : %s\n",
				__Test_XWsWaitMin(&tCliCtx2.iCloseExCount, 1, 1000u) &&
				tCliCtx2.iRemoteCloseCode == XWS_CLOSE_TOO_BIG &&
				__Test_XWsAtomicLoad(&tSrvCtx.iTextCount) == iSrvTextBefore + 1 ? "PASS" : "FAIL");
			printf("  WS plain reaccept client error free : %s\n", __Test_XWsAtomicLoad(&tCliCtx2.iErrorCount) == 0 ? "PASS" : "FAIL");
			printf("  WS plain reaccept server error free : %s\n", __Test_XWsAtomicLoad(&tSrvCtx.iErrorCount) == 0 ? "PASS" : "FAIL");
		}

		if ( pClient ) { xrtWsClientDestroy(pClient); pClient = NULL; }
		{
			__test_xws_client_ctx tFrameLimitCtx;
			uint8 aOversizedFrame[1536];
			long iServerOpenBefore = __Test_XWsAtomicLoad(&tSrvCtx.iOpenCount);
			long iServerCloseBefore = __Test_XWsAtomicLoad(&tSrvCtx.iCloseCount);
			memset(&tFrameLimitCtx, 0, sizeof(tFrameLimitCtx));
			for ( size_t i = 0u; i < sizeof(aOversizedFrame); ++i ) {
				aOversizedFrame[i] = (uint8)(i * 31u + 7u);
			}
			pServer->tConfig.iMaxFrameBytes = 1024u;
			pClient = pClientEngine ? xrtWsClientCreate(pClientEngine, &tCliCfg,
				&tCliEvents, &tFrameLimitCtx) : NULL;
			printf("  WS frame limit client start : %s\n",
				pClient && xrtWsClientStart(pClient) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS frame limit connection opens : %s\n",
				__Test_XWsWaitMin(&tSrvCtx.iOpenCount, iServerOpenBefore + 1, 2000u) &&
				__Test_XWsWaitMin(&tFrameLimitCtx.iOpenCount, 1, 2000u) ? "PASS" : "FAIL");
			printf("  WS oversized wire frame submitted : %s\n",
				pClient && __Test_XWsSendFrameRaw(pClient->pStream, true, true, false,
					XCODEC_WS_OPCODE_BINARY, aOversizedFrame, sizeof(aOversizedFrame)) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS oversized wire frame closes with 1009 : %s\n",
				__Test_XWsWaitMin(&tFrameLimitCtx.iCloseCount, 1, 3000u) &&
				__Test_XWsWaitMin(&tSrvCtx.iCloseCount, iServerCloseBefore + 1, 3000u) &&
				tFrameLimitCtx.iRemoteCloseCode == XWS_CLOSE_TOO_BIG ? "PASS" : "FAIL");
			pServer->tConfig.iMaxFrameBytes = 1024u * 1024u;
		}

		if ( pClient ) { xrtWsClientDestroy(pClient); pClient = NULL; }
		{
			__test_xws_client_ctx tRejectedCtx;
			xnetfuture* pRejectedOpen = NULL;
			long iSrvOpenBefore = __Test_XWsAtomicLoad(&tSrvCtx.iOpenCount);
			long iSrvHandshakeBefore = __Test_XWsAtomicLoad(&tSrvCtx.iHandshakeCount);
			memset(&tRejectedCtx, 0, sizeof(tRejectedCtx));
			__xrtTestAtomicStoreLong(&tSrvCtx.iRejectNext, 1);
			pClient = pClientEngine ? xrtWsClientCreate(pClientEngine, &tCliCfg, &tCliEvents, &tRejectedCtx) : NULL;
			printf("  WS policy rejection client create : %s\n", pClient != NULL ? "PASS" : "FAIL");
			pRejectedOpen = pClient ? xrtWsClientStartFuture(pClient) : NULL;
			printf("  WS policy rejection client start : %s\n",
				pRejectedOpen != NULL ? "PASS" : "FAIL");
			printf("  WS policy rejection open future : %s\n",
				pRejectedOpen && xrtNetFutureWait(pRejectedOpen, 2000u) == XRT_NET_ERROR ? "PASS" : "FAIL");
			if ( pRejectedOpen ) { xrtNetFutureDestroy(pRejectedOpen); }
			printf("  WS policy rejection response : %s\n",
				__Test_XWsWaitMin(&tRejectedCtx.iHandshakeResponseCount, 1, 2000u) &&
				tRejectedCtx.iHandshakeStatus == 403u &&
				__Test_XWsAtomicLoad(&tRejectedCtx.iHandshakeHeaderCount) == 1 &&
				__Test_XWsAtomicLoad(&tRejectedCtx.iOpenCount) == 0 &&
				__Test_XWsAtomicLoad(&tSrvCtx.iOpenCount) == iSrvOpenBefore &&
				__Test_XWsAtomicLoad(&tSrvCtx.iHandshakeCount) == iSrvHandshakeBefore + 1 ? "PASS" : "FAIL");
			printf("  WS policy rejection closes client : %s\n",
				__Test_XWsWaitMin(&tRejectedCtx.iCloseCount, 1, 2000u) &&
				__Test_XWsAtomicLoad(&tRejectedCtx.iErrorCount) == 1 ? "PASS" : "FAIL");
		}

		if ( pClient ) { xrtWsClientDestroy(pClient); pClient = NULL; }
		{
			__test_xws_client_ctx tCloseTimeoutCtx;
			xwscloseinfo tCloseInfo;
			long iServerOpenBefore = __Test_XWsAtomicLoad(&tSrvCtx.iOpenCount);
			long iServerCloseBefore = __Test_XWsAtomicLoad(&tSrvCtx.iCloseCount);
			memset(&tCloseTimeoutCtx, 0, sizeof(tCloseTimeoutCtx));
			tCliCfg.iCloseTimeoutMs = 50u;
			pClient = pClientEngine ? xrtWsClientCreate(pClientEngine, &tCliCfg,
				&tCliEvents, &tCloseTimeoutCtx) : NULL;
			printf("  WS close timeout client start : %s\n",
				pClient && xrtWsClientStart(pClient) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS close timeout connection opens : %s\n",
				__Test_XWsWaitMin(&tSrvCtx.iOpenCount, iServerOpenBefore + 1, 2000u) &&
				__Test_XWsWaitMin(&tCloseTimeoutCtx.iOpenCount, 1, 2000u) &&
				tSrvCtx.pLastConn && tSrvCtx.pLastConn->pStream ? "PASS" : "FAIL");
			if ( tSrvCtx.pLastConn && tSrvCtx.pLastConn->pStream ) {
				tSrvCtx.pLastConn->pStream->pEvents = __Test_XWsIgnoreCloseStreamEvents();
			}
			printf("  WS close timeout request accepted : %s\n",
				pClient && xrtWsClientClose(pClient, XWS_CLOSE_NORMAL, "timeout") == XRT_NET_OK ? "PASS" : "FAIL");
			xrtWsCloseInfoInit(&tCloseInfo);
			printf("  WS close timeout aborts unresponsive peer : %s\n",
				__Test_XWsWaitMin(&tCloseTimeoutCtx.iCloseCount, 1, 2000u) &&
				xrtWsClientCloseInfo(pClient, &tCloseInfo) &&
				tCloseInfo.iTransportResult == XRT_NET_TIMEOUT &&
				(tCloseInfo.iFlags & XWS_CLOSE_F_SENT) != 0u &&
				(tCloseInfo.iFlags & XWS_CLOSE_F_RECEIVED) == 0u ? "PASS" : "FAIL");
			printf("  WS close timeout releases server peer : %s\n",
				__Test_XWsWaitMin(&tSrvCtx.iCloseCount, iServerCloseBefore + 1, 2000u) ? "PASS" : "FAIL");
		}
		if ( pClient ) { xrtWsClientDestroy(pClient); pClient = NULL; }
		if ( pServer ) xrtWsServerDestroy(pServer);
		xrtWsClientConfigUnit(&tCliCfg);
		xrtWsServerConfigUnit(&tSrvCfg);
		if ( pClientEngine ) {
			xrtNetEngineStop(pClientEngine);
			xrtNetEngineDestroy(pClientEngine);
		}
		if ( pServerEngine ) {
			xrtNetEngineStop(pServerEngine);
			xrtNetEngineDestroy(pServerEngine);
		}
	}

	{
		__test_xws_stall_ctx tStallCtx;
		__test_xws_client_ctx tClientCtx;
		xnetengineconfig tEngineCfg;
		xnetlistenconfig tListenCfg;
		xwsclientconfig tClientCfg;
		xwsclientevents tClientEvents;
		xnetengine* pServerEngine = NULL;
		xnetengine* pClientEngine = NULL;
		xnetlistener* pListener = NULL;
		xwsclient* pClient = NULL;
		xnetfuture* pOpenFuture = NULL;
		xwscloseinfo tCloseInfo;
		char sURL[256];
		memset(&tStallCtx, 0, sizeof(tStallCtx));
		memset(&tClientCtx, 0, sizeof(tClientCtx));
		xrtNetEngineConfigInit(&tEngineCfg);
		tEngineCfg.iWorkerCount = 1u;
		pServerEngine = xrtNetEngineCreate(&tEngineCfg);
		pClientEngine = xrtNetEngineCreate(&tEngineCfg);
		printf("  WS timeout engines create : %s\n",
			pServerEngine && pClientEngine ? "PASS" : "FAIL");
		printf("  WS timeout engines start : %s\n",
			pServerEngine && pClientEngine && xrtNetEngineStart(pServerEngine) == XRT_NET_OK &&
			xrtNetEngineStart(pClientEngine) == XRT_NET_OK ? "PASS" : "FAIL");
		xrtNetListenConfigInit(&tListenCfg);
		(void)xrtNetAddrParse(&tListenCfg.tBindAddr, "127.0.0.1", 0u);
		pListener = pServerEngine ? xrtNetListenerCreate(pServerEngine, &tListenCfg,
			__Test_XWsStallListenerEvents(), __Test_XWsStallStreamEvents(), &tStallCtx) : NULL;
		printf("  WS timeout stall listener start : %s\n",
			pListener && xrtNetListenerStart(pListener) == XRT_NET_OK ? "PASS" : "FAIL");
		xrtWsClientConfigInit(&tClientCfg);
		tClientCfg.iHandshakeTimeoutMs = 50u;
		tClientCfg.iCloseTimeoutMs = 50u;
		snprintf(sURL, sizeof(sURL), "ws://127.0.0.1:%u/stall",
			pListener ? (unsigned)pListener->tConfig.tBindAddr.iPort : 0u);
		(void)xrtWsClientConfigSetURL(&tClientCfg, sURL);
		xrtWsClientEventsInit(&tClientEvents);
		tClientEvents.OnClose = __Test_XWsClientOnClose;
		tClientEvents.OnCloseEx = __Test_XWsClientOnCloseEx;
		tClientEvents.OnError = __Test_XWsClientOnError;
		pClient = pClientEngine ? xrtWsClientCreate(pClientEngine, &tClientCfg,
			&tClientEvents, &tClientCtx) : NULL;
		pOpenFuture = pClient ? xrtWsClientStartFuture(pClient) : NULL;
		printf("  WS handshake timeout client start : %s\n",
			pOpenFuture != NULL ? "PASS" : "FAIL");
		printf("  WS handshake timeout open future : %s\n",
			pOpenFuture && xrtNetFutureWait(pOpenFuture, 2000u) == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		if ( pOpenFuture ) { xrtNetFutureDestroy(pOpenFuture); pOpenFuture = NULL; }
		printf("  WS handshake timeout request received : %s\n",
			__Test_XWsWaitMin(&tStallCtx.iAcceptCount, 1, 2000u) &&
			__Test_XWsWaitMin(&tStallCtx.iRecvCount, 1, 2000u) ? "PASS" : "FAIL");
		xrtWsCloseInfoInit(&tCloseInfo);
		printf("  WS handshake timeout closes transport : %s\n",
			__Test_XWsWaitMin(&tClientCtx.iCloseCount, 1, 2000u) &&
			xrtWsClientCloseInfo(pClient, &tCloseInfo) &&
			tCloseInfo.iTransportResult == XRT_NET_TIMEOUT &&
			__Test_XWsAtomicLoad(&tClientCtx.iErrorCount) == 0 ? "PASS" : "FAIL");
		printf("  WS handshake timeout releases server stream : %s\n",
			__Test_XWsWaitMin(&tStallCtx.iCloseCount, 1, 2000u) ? "PASS" : "FAIL");
		if ( pClient ) { xrtWsClientDestroy(pClient); }
		xrtWsClientConfigUnit(&tClientCfg);
		if ( pListener ) { xrtNetListenerStop(pListener); xrtNetListenerDestroy(pListener); }
		if ( pClientEngine ) { xrtNetEngineStop(pClientEngine); xrtNetEngineDestroy(pClientEngine); }
		if ( pServerEngine ) { xrtNetEngineStop(pServerEngine); xrtNetEngineDestroy(pServerEngine); }
	}

	{
		__test_xws_server_ctx tWsServerCtx;
		__test_xws_client_ctx tWsClientCtx;
		__test_xws_httpd_ctx tHttpdCtx;
		xwsserverevents tWsServerEvents;
		xwsclientevents tWsClientEvents;
		xwsserverconfig tWsServerCfg;
		xwsclientconfig tWsClientCfg;
		xhttpdevents tHttpdEvents;
		xhttpdconfig tHttpdCfg;
		xnetengineconfig tEngineCfg;
		xnetengine* pServerEngine = NULL;
		xnetengine* pClientEngine = NULL;
		xhttpdserver* pHttpd = NULL;
		xwsserver* pWsServer = NULL;
		xwsclient* pWsClient = NULL;
		char sURL[256];

		memset(&tWsServerCtx, 0, sizeof(tWsServerCtx));
		memset(&tWsClientCtx, 0, sizeof(tWsClientCtx));
		memset(&tHttpdCtx, 0, sizeof(tHttpdCtx));
		xrtWsServerEventsInit(&tWsServerEvents);
		xrtWsClientEventsInit(&tWsClientEvents);
		xrtHttpdEventsInit(&tHttpdEvents);
		tWsServerEvents.OnOpen = __Test_XWsServerOnOpen;
		tWsServerEvents.OnText = __Test_XWsServerOnText;
		tWsServerEvents.OnClose = __Test_XWsServerOnClose;
		tWsServerEvents.OnError = __Test_XWsServerOnError;
		tWsServerEvents.OnCloseEx = __Test_XWsServerOnCloseEx;
		tWsServerEvents.OnHandshake = __Test_XWsServerOnHandshake;
		tWsClientEvents.OnOpen = __Test_XWsClientOnOpen;
		tWsClientEvents.OnText = __Test_XWsClientOnText;
		tWsClientEvents.OnClose = __Test_XWsClientOnClose;
		tWsClientEvents.OnError = __Test_XWsClientOnError;
		tWsClientEvents.OnCloseEx = __Test_XWsClientOnCloseEx;
		tWsClientEvents.OnHandshakeResponse = __Test_XWsClientOnHandshakeResponse;
		tHttpdEvents.OnRequest = __Test_XWsHttpdOnRequest;

		xrtNetEngineConfigInit(&tEngineCfg);
		tEngineCfg.iWorkerCount = 1u;
		pServerEngine = xrtNetEngineCreate(&tEngineCfg);
		pClientEngine = xrtNetEngineCreate(&tEngineCfg);
		printf("  WS HTTPD shared engines create : %s\n",
			pServerEngine && pClientEngine ? "PASS" : "FAIL");
		printf("  WS HTTPD shared engines start : %s\n",
			pServerEngine && pClientEngine && xrtNetEngineStart(pServerEngine) == XRT_NET_OK &&
			xrtNetEngineStart(pClientEngine) == XRT_NET_OK ? "PASS" : "FAIL");

		xrtWsServerConfigInit(&tWsServerCfg);
		(void)xrtWsServerConfigSetProtocol(&tWsServerCfg, "chat.v1");
		(void)xrtWsServerConfigSetPath(&tWsServerCfg, "/chat");
		(void)xrtWsServerConfigSetOrigin(&tWsServerCfg, "https://example.test");
		pWsServer = pServerEngine ? xrtWsServerCreate(pServerEngine, &tWsServerCfg,
			&tWsServerEvents, &tWsServerCtx) : NULL;
		tHttpdCtx.pWsServer = pWsServer;
		printf("  WS HTTPD protocol manager create : %s\n", pWsServer ? "PASS" : "FAIL");

		xrtHttpdConfigInit(&tHttpdCfg);
		(void)xrtNetAddrParse(&tHttpdCfg.tBindAddr, "127.0.0.1", 0u);
		pHttpd = pServerEngine ? xrtHttpdCreate(pServerEngine, &tHttpdCfg,
			&tHttpdEvents, &tHttpdCtx) : NULL;
		printf("  WS HTTPD server create : %s\n", pHttpd ? "PASS" : "FAIL");
		printf("  WS HTTPD server start : %s\n",
			pHttpd && xrtHttpdStart(pHttpd) == XRT_NET_OK ? "PASS" : "FAIL");

		xrtWsClientConfigInit(&tWsClientCfg);
		snprintf(sURL, sizeof(sURL), "ws://127.0.0.1:%u/chat?via=httpd",
			(unsigned)xrtHttpdBoundPort(pHttpd));
		(void)xrtWsClientConfigSetURL(&tWsClientCfg, sURL);
		(void)xrtWsClientConfigSetOrigin(&tWsClientCfg, "https://example.test");
		(void)xrtWsClientConfigSetProtocols(&tWsClientCfg, "other, chat.v1");
		(void)xrtWsClientConfigSetHeader(&tWsClientCfg, "X-Test-Handshake", "policy-token");
		pWsClient = pClientEngine ? xrtWsClientCreate(pClientEngine, &tWsClientCfg,
			&tWsClientEvents, &tWsClientCtx) : NULL;
		printf("  WS HTTPD client create : %s\n", pWsClient ? "PASS" : "FAIL");
		printf("  WS HTTPD client start : %s\n",
			pWsClient && xrtWsClientStart(pWsClient) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  WS HTTPD shared upgrade opens : %s\n",
			__Test_XWsWaitMin(&tWsServerCtx.iOpenCount, 1, 3000u) &&
			__Test_XWsWaitMin(&tWsClientCtx.iOpenCount, 1, 3000u) &&
			__Test_XWsAtomicLoad(&tHttpdCtx.iRequestCount) == 1 &&
			__Test_XWsAtomicLoad(&tHttpdCtx.iAcceptedCount) == 1 ? "PASS" : "FAIL");
		printf("  WS HTTPD shared upgrade metadata : %s\n",
			tWsServerCtx.pLastConn &&
			strcmp(xrtWsConnProtocol(tWsServerCtx.pLastConn), "chat.v1") == 0 &&
			xrtWsConnGetData(tWsServerCtx.pLastConn) == &tWsServerCtx &&
			__Test_XWsAtomicLoad(&tWsClientCtx.iHandshakeHeaderCount) == 1 ? "PASS" : "FAIL");
		printf("  WS HTTPD detaches HTTP connection : %s\n",
			pHttpd && xrtHttpdConnectionCount(pHttpd) == 0u ? "PASS" : "FAIL");
		printf("  WS HTTPD shared send text : %s\n",
			pWsClient && xrtWsClientSendText(pWsClient, "httpd-ws", 8u) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  WS HTTPD shared text roundtrip : %s\n",
			__Test_XWsWaitMin(&tWsServerCtx.iTextCount, 1, 2000u) &&
			__Test_XWsWaitMin(&tWsClientCtx.iTextCount, 1, 2000u) &&
			strcmp(tWsClientCtx.aLastText, "httpd-ws") == 0 ? "PASS" : "FAIL");
		printf("  WS HTTPD shared close request : %s\n",
			pWsClient && xrtWsClientClose(pWsClient, XWS_CLOSE_NORMAL, "done") == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  WS HTTPD shared clean close : %s\n",
			__Test_XWsWaitMin(&tWsClientCtx.iCloseCount, 1, 3000u) &&
			__Test_XWsWaitMin(&tWsServerCtx.iCloseCount, 1, 3000u) &&
			__Test_XWsAtomicLoad(&tWsClientCtx.iErrorCount) == 0 &&
			__Test_XWsAtomicLoad(&tWsServerCtx.iErrorCount) == 0 ? "PASS" : "FAIL");

		if ( pWsClient ) { xrtWsClientDestroy(pWsClient); }
		if ( pWsServer ) { xrtWsServerDestroy(pWsServer); }
		if ( pHttpd ) { xrtHttpdDestroy(pHttpd); }
		xrtWsClientConfigUnit(&tWsClientCfg);
		xrtWsServerConfigUnit(&tWsServerCfg);
		if ( pClientEngine ) { xrtNetEngineStop(pClientEngine); xrtNetEngineDestroy(pClientEngine); }
		if ( pServerEngine ) { xrtNetEngineStop(pServerEngine); xrtNetEngineDestroy(pServerEngine); }
	}

	{
		__test_xws_server_ctx tSrvCtx;
		__test_xws_client_ctx tCliCtx;
		xwsserverevents tSrvEvents;
		xwsclientevents tCliEvents;
		xwsserverconfig tSrvCfg;
		xwsclientconfig tCliCfg;
		xnetengineconfig tEngineCfg;
		xnetengine* pServerEngine = NULL;
		xnetengine* pClientEngine = NULL;
		xwsserver* pServer = NULL;
		xwsclient* pClient = NULL;
		xtlsconfig tTlsCfg;
		char sURL[256];

		memset(&tSrvCtx, 0, sizeof(tSrvCtx));
		memset(&tCliCtx, 0, sizeof(tCliCtx));
		xrtWsServerEventsInit(&tSrvEvents);
		xrtWsClientEventsInit(&tCliEvents);
		memset(&tTlsCfg, 0, sizeof(tTlsCfg));
		tSrvEvents.OnOpen = __Test_XWsServerOnOpen;
		tSrvEvents.OnText = __Test_XWsServerOnText;
		tSrvEvents.OnBinary = __Test_XWsServerOnBinary;
		tSrvEvents.OnClose = __Test_XWsServerOnClose;
		tSrvEvents.OnError = __Test_XWsServerOnError;
		tSrvEvents.OnPing = __Test_XWsServerOnPing;
		tSrvEvents.OnPong = __Test_XWsServerOnPong;
		tSrvEvents.OnCloseEx = __Test_XWsServerOnCloseEx;
		tCliEvents.OnOpen = __Test_XWsClientOnOpen;
		tCliEvents.OnText = __Test_XWsClientOnText;
		tCliEvents.OnBinary = __Test_XWsClientOnBinary;
		tCliEvents.OnClose = __Test_XWsClientOnClose;
		tCliEvents.OnError = __Test_XWsClientOnError;
		tCliEvents.OnPing = __Test_XWsClientOnPing;
		tCliEvents.OnPong = __Test_XWsClientOnPong;
		tCliEvents.OnCloseEx = __Test_XWsClientOnCloseEx;
		tTlsCfg.sCertFile = "dev/xnet2_tls_test_cert.pem";
		tTlsCfg.sKeyFile = "dev/xnet2_tls_test_key.pem";

		xrtNetEngineConfigInit(&tEngineCfg);
		tEngineCfg.iWorkerCount = 1;
		pServerEngine = xrtNetEngineCreate(&tEngineCfg);
		pClientEngine = xrtNetEngineCreate(&tEngineCfg);
		if ( __Test_XWsFileExists(tTlsCfg.sCertFile) && __Test_XWsFileExists(tTlsCfg.sKeyFile) ) {
			printf("  WS TLS fixture files exist : PASS\n");
			printf("  WS TLS server engine create : %s\n", pServerEngine != NULL ? "PASS" : "FAIL");
			printf("  WS TLS client engine create : %s\n", pClientEngine != NULL ? "PASS" : "FAIL");
			if ( pServerEngine ) printf("  WS TLS server engine start : %s\n", xrtNetEngineStart(pServerEngine) == XRT_NET_OK ? "PASS" : "FAIL");
			if ( pClientEngine ) printf("  WS TLS client engine start : %s\n", xrtNetEngineStart(pClientEngine) == XRT_NET_OK ? "PASS" : "FAIL");

			xrtWsServerConfigInit(&tSrvCfg);
			(void)xrtNetAddrParse(&tSrvCfg.tBindAddr, "127.0.0.1", 0);
			tSrvCfg.iWebSocketFlags = XWS_F_PERMESSAGE_DEFLATE;
			tSrvCfg.pTlsConfig = &tTlsCfg;
			pServer = (pServerEngine && pClientEngine) ? xrtWsServerCreate(pServerEngine, &tSrvCfg, &tSrvEvents, &tSrvCtx) : NULL;
			printf("  WS TLS server create : %s\n", pServer != NULL ? "PASS" : "FAIL");
			printf("  WS TLS server start : %s\n", pServer && xrtWsServerStart(pServer) == XRT_NET_OK ? "PASS" : "FAIL");

			xrtWsClientConfigInit(&tCliCfg);
			snprintf(sURL, sizeof(sURL), "wss://127.0.0.1:%u/secure", (unsigned)xrtWsServerBoundPort(pServer));
			snprintf(tCliCfg.sURL, sizeof(tCliCfg.sURL), "%s", sURL);
			tCliCfg.bVerifyPeer = false;
			pClient = pClientEngine ? xrtWsClientCreate(pClientEngine, &tCliCfg, &tCliEvents, &tCliCtx) : NULL;
			printf("  WS TLS client create : %s\n", pClient != NULL ? "PASS" : "FAIL");
			printf("  WS TLS client start : %s\n", pClient && xrtWsClientStart(pClient) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS TLS server open : %s\n", __Test_XWsWaitMin(&tSrvCtx.iOpenCount, 1, 4000u) ? "PASS" : "FAIL");
			printf("  WS TLS client open : %s\n", __Test_XWsWaitMin(&tCliCtx.iOpenCount, 1, 4000u) ? "PASS" : "FAIL");
			printf("  WS TLS PMD no-offer fallback : %s\n",
				pClient && tSrvCtx.pLastConn && !xrtWsClientPerMessageDeflate(pClient) &&
				!xrtWsConnPerMessageDeflate(tSrvCtx.pLastConn) ? "PASS" : "FAIL");
			printf("  WS TLS send text : %s\n", pClient && xrtWsClientSendText(pClient, "secure-ws", 9u) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS TLS server text callback : %s\n", __Test_XWsWaitMin(&tSrvCtx.iTextCount, 1, 4000u) ? "PASS" : "FAIL");
			printf("  WS TLS client text echo : %s\n", __Test_XWsWaitMin(&tCliCtx.iTextCount, 1, 4000u) ? "PASS" : "FAIL");
			printf("  WS TLS echoed text : %s\n", strcmp(tCliCtx.aLastText, "secure-ws") == 0 ? "PASS" : "FAIL");

			{
				volatile long iReleaseCount = 0;
				long iServerTextBefore = __Test_XWsAtomicLoad(&tSrvCtx.iTextCount);
				long iClientTextBefore = __Test_XWsAtomicLoad(&tCliCtx.iTextCount);
				char* pFrame = NULL;
				size_t iFrameLen = 0u;
				xnetbufref tRef;
				xnet_result iSendResult = XRT_NET_ERROR;
				memset(&tRef, 0, sizeof(tRef));
				if ( pClient && __xwsBuildFrameBytes(XCODEC_WS_OPCODE_TEXT, true,
					"tls-ref", 7u, &pFrame, &iFrameLen) && iFrameLen <= UINT32_MAX ) {
					tRef.pData = pFrame;
					tRef.iLen = (uint32)iFrameLen;
					tRef.pfnRelease = __Test_XWsReleaseRef;
					tRef.pReleaseCtx = (ptr)&iReleaseCount;
					iSendResult = xrtNetStreamSendRef(pClient->pStream, &tRef);
					if ( iSendResult != XRT_NET_OK ) { XNET_FREE(pFrame); }
				}
				printf("  WS TLS SendRef accepted : %s\n",
					iSendResult == XRT_NET_OK ? "PASS" : "FAIL");
				printf("  WS TLS SendRef releases exactly once : %s\n",
					__Test_XWsWaitMin(&iReleaseCount, 1, 4000u) &&
					__Test_XWsAtomicLoad(&iReleaseCount) == 1 ? "PASS" : "FAIL");
				printf("  WS TLS SendRef text roundtrip : %s\n",
					__Test_XWsWaitMin(&tSrvCtx.iTextCount, iServerTextBefore + 1, 4000u) &&
					__Test_XWsWaitMin(&tCliCtx.iTextCount, iClientTextBefore + 1, 4000u) &&
					strcmp(tCliCtx.aLastText, "tls-ref") == 0 ? "PASS" : "FAIL");
			}

			{
				long iSrvTextBefore = __Test_XWsAtomicLoad(&tSrvCtx.iTextCount);
				long iSrvPingBefore = __Test_XWsAtomicLoad(&tSrvCtx.iPingCount);
				long iCliPongBefore = __Test_XWsAtomicLoad(&tCliCtx.iPongCount);
				long iCliTextBefore = __Test_XWsAtomicLoad(&tCliCtx.iTextCount);
				printf("  WS TLS fragmented client text frame1 : %s\n", pClient && __xwsStreamSendFrameEx(pClient->pStream, false, true, XCODEC_WS_OPCODE_TEXT, "tls-", 4u) == XRT_NET_OK ? "PASS" : "FAIL");
				printf("  WS TLS fragmented client ping : %s\n", pClient && __xwsStreamSendFrameEx(pClient->pStream, true, true, XCODEC_WS_OPCODE_PING, "ts", 2u) == XRT_NET_OK ? "PASS" : "FAIL");
				printf("  WS TLS fragmented client text cont : %s\n", pClient && __xwsStreamSendFrameEx(pClient->pStream, true, true, XCODEC_WS_OPCODE_CONT, "frag", 4u) == XRT_NET_OK ? "PASS" : "FAIL");
				printf("  WS TLS fragmented server text callback : %s\n", __Test_XWsWaitMin(&tSrvCtx.iTextCount, iSrvTextBefore + 1, 4000u) ? "PASS" : "FAIL");
				printf("  WS TLS fragmented server ping callback : %s\n", __Test_XWsWaitMin(&tSrvCtx.iPingCount, iSrvPingBefore + 1, 4000u) ? "PASS" : "FAIL");
				printf("  WS TLS fragmented client pong callback : %s\n", __Test_XWsWaitMin(&tCliCtx.iPongCount, iCliPongBefore + 1, 4000u) ? "PASS" : "FAIL");
				printf("  WS TLS fragmented client text echo : %s\n", __Test_XWsWaitMin(&tCliCtx.iTextCount, iCliTextBefore + 1, 4000u) ? "PASS" : "FAIL");
				printf("  WS TLS fragmented text reassembled : %s\n", strcmp(tSrvCtx.aLastText, "tls-frag") == 0 && strcmp(tCliCtx.aLastText, "tls-frag") == 0 ? "PASS" : "FAIL");
			}

			printf("  WS TLS close request : %s\n", pClient && xrtWsClientClose(pClient, XWS_CLOSE_NORMAL, NULL) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS TLS client close callback : %s\n", __Test_XWsWaitMin(&tCliCtx.iCloseCount, 1, 4000u) ? "PASS" : "FAIL");
			printf("  WS TLS server close callback : %s\n", __Test_XWsWaitMin(&tSrvCtx.iCloseCount, 1, 4000u) ? "PASS" : "FAIL");
			printf("  WS TLS null-reason clean close metadata : %s\n",
				__Test_XWsWaitMin(&tCliCtx.iCloseExCount, 1, 1000u) && __Test_XWsWaitMin(&tSrvCtx.iCloseExCount, 1, 1000u) &&
				(tCliCtx.iCloseFlags & XWS_CLOSE_F_CLEAN) != 0u && (tSrvCtx.iCloseFlags & XWS_CLOSE_F_CLEAN) != 0u &&
				tSrvCtx.aRemoteCloseReason[0] == '\0' ? "PASS" : "FAIL");
			printf("  WS TLS client error free : %s\n", __Test_XWsAtomicLoad(&tCliCtx.iErrorCount) == 0 ? "PASS" : "FAIL");
			printf("  WS TLS server error free : %s\n", __Test_XWsAtomicLoad(&tSrvCtx.iErrorCount) == 0 ? "PASS" : "FAIL");
		} else {
			printf("  WS TLS fixture files missing : SKIP\n");
		}

		if ( pClient ) xrtWsClientDestroy(pClient);
		if ( pServer ) xrtWsServerDestroy(pServer);
		if ( pClientEngine ) {
			xrtNetEngineStop(pClientEngine);
			xrtNetEngineDestroy(pClientEngine);
		}
		if ( pServerEngine ) {
			xrtNetEngineStop(pServerEngine);
			xrtNetEngineDestroy(pServerEngine);
		}
	}
#undef printf
	return iFailCount == 0 ? 0 : 1;
}
