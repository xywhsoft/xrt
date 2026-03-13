#include "../lib/xws2.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#else
	#include <unistd.h>
#endif

typedef struct {
	volatile long iOpenCount;
	volatile long iTextCount;
	volatile long iBinaryCount;
	volatile long iPingCount;
	volatile long iPongCount;
	volatile long iCloseCount;
	volatile long iErrorCount;
	char aLastText[256];
	uint8 aLastBinary[256];
	size_t iLastBinaryLen;
	xws2conn* pLastConn;
} __test_xws2_server_ctx;

typedef struct {
	volatile long iOpenCount;
	volatile long iTextCount;
	volatile long iBinaryCount;
	volatile long iPingCount;
	volatile long iPongCount;
	volatile long iCloseCount;
	volatile long iErrorCount;
	char aLastText[256];
	uint8 aLastBinary[256];
	size_t iLastBinaryLen;
} __test_xws2_client_ctx;

static void __Test_XWs2SleepMs(uint32 iDelayMs)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(iDelayMs);
	#else
		usleep((useconds_t)iDelayMs * 1000u);
	#endif
}

static long __Test_XWs2AtomicInc(volatile long* pValue)
{
	#if defined(_WIN32) || defined(_WIN64)
		return InterlockedIncrement((volatile LONG*)pValue);
	#else
		return __sync_add_and_fetch(pValue, 1);
	#endif
}

static long __Test_XWs2AtomicLoad(volatile long* pValue)
{
	#if defined(_WIN32) || defined(_WIN64)
		return InterlockedCompareExchange((volatile LONG*)pValue, 0, 0);
	#else
		return __sync_add_and_fetch(pValue, 0);
	#endif
}

static bool __Test_XWs2WaitMin(volatile long* pValue, long iExpectMin, uint32 iTimeoutMs)
{
	uint32 iLoops = (iTimeoutMs / 10u) + 1u;
	for ( uint32 i = 0; i < iLoops; ++i ) {
		if ( __Test_XWs2AtomicLoad(pValue) >= iExpectMin ) return true;
		__Test_XWs2SleepMs(10u);
	}
	return __Test_XWs2AtomicLoad(pValue) >= iExpectMin;
}

static bool __Test_XWs2FileExists(const char* sPath)
{
	FILE* pFile = fopen(sPath, "rb");
	if ( !pFile ) return false;
	fclose(pFile);
	return true;
}

static void __Test_XWs2ServerOnOpen(ptr pOwner, xws2server* pServer, xws2conn* pConn)
{
	__test_xws2_server_ctx* pCtx = (__test_xws2_server_ctx*)pOwner;
	(void)pServer;
	if ( !pCtx ) return;
	pCtx->pLastConn = pConn;
	__Test_XWs2AtomicInc(&pCtx->iOpenCount);
}

static void __Test_XWs2ServerOnText(ptr pOwner, xws2server* pServer, xws2conn* pConn, const char* pData, size_t iLen)
{
	__test_xws2_server_ctx* pCtx = (__test_xws2_server_ctx*)pOwner;
	size_t iCopy;
	(void)pServer;
	if ( !pCtx || !pConn || !pData ) return;
	__Test_XWs2AtomicInc(&pCtx->iTextCount);
	iCopy = iLen < (sizeof(pCtx->aLastText) - 1u) ? iLen : (sizeof(pCtx->aLastText) - 1u);
	memcpy(pCtx->aLastText, pData, iCopy);
	pCtx->aLastText[iCopy] = '\0';
	(void)xrtWs2ConnSendText(pConn, pData, iLen);
}

static void __Test_XWs2ServerOnBinary(ptr pOwner, xws2server* pServer, xws2conn* pConn, const void* pData, size_t iLen)
{
	__test_xws2_server_ctx* pCtx = (__test_xws2_server_ctx*)pOwner;
	size_t iCopy;
	(void)pServer;
	if ( !pCtx || !pConn || (!pData && iLen != 0u) ) return;
	__Test_XWs2AtomicInc(&pCtx->iBinaryCount);
	iCopy = iLen < sizeof(pCtx->aLastBinary) ? iLen : sizeof(pCtx->aLastBinary);
	if ( iCopy > 0 ) memcpy(pCtx->aLastBinary, pData, iCopy);
	pCtx->iLastBinaryLen = iCopy;
	(void)xrtWs2ConnSendBinary(pConn, pData, iLen);
}

static void __Test_XWs2ServerOnClose(ptr pOwner, xws2server* pServer, xws2conn* pConn, xnet_result iReason)
{
	__test_xws2_server_ctx* pCtx = (__test_xws2_server_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	(void)iReason;
	if ( !pCtx ) return;
	__Test_XWs2AtomicInc(&pCtx->iCloseCount);
}

static void __Test_XWs2ServerOnError(ptr pOwner, xws2server* pServer, xws2conn* pConn, int iSysErr)
{
	__test_xws2_server_ctx* pCtx = (__test_xws2_server_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	(void)iSysErr;
	if ( !pCtx ) return;
	__Test_XWs2AtomicInc(&pCtx->iErrorCount);
}

static void __Test_XWs2ServerOnPing(ptr pOwner, xws2server* pServer, xws2conn* pConn, const void* pData, size_t iLen)
{
	__test_xws2_server_ctx* pCtx = (__test_xws2_server_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	(void)pData;
	(void)iLen;
	if ( !pCtx ) return;
	__Test_XWs2AtomicInc(&pCtx->iPingCount);
}

static void __Test_XWs2ServerOnPong(ptr pOwner, xws2server* pServer, xws2conn* pConn, const void* pData, size_t iLen)
{
	__test_xws2_server_ctx* pCtx = (__test_xws2_server_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	(void)pData;
	(void)iLen;
	if ( !pCtx ) return;
	__Test_XWs2AtomicInc(&pCtx->iPongCount);
}

static void __Test_XWs2ClientOnOpen(ptr pOwner, xws2client* pClient)
{
	__test_xws2_client_ctx* pCtx = (__test_xws2_client_ctx*)pOwner;
	(void)pClient;
	if ( !pCtx ) return;
	__Test_XWs2AtomicInc(&pCtx->iOpenCount);
}

static void __Test_XWs2ClientOnText(ptr pOwner, xws2client* pClient, const char* pData, size_t iLen)
{
	__test_xws2_client_ctx* pCtx = (__test_xws2_client_ctx*)pOwner;
	size_t iCopy;
	(void)pClient;
	if ( !pCtx || !pData ) return;
	__Test_XWs2AtomicInc(&pCtx->iTextCount);
	iCopy = iLen < (sizeof(pCtx->aLastText) - 1u) ? iLen : (sizeof(pCtx->aLastText) - 1u);
	memcpy(pCtx->aLastText, pData, iCopy);
	pCtx->aLastText[iCopy] = '\0';
}

static void __Test_XWs2ClientOnBinary(ptr pOwner, xws2client* pClient, const void* pData, size_t iLen)
{
	__test_xws2_client_ctx* pCtx = (__test_xws2_client_ctx*)pOwner;
	size_t iCopy;
	(void)pClient;
	if ( !pCtx || (!pData && iLen != 0u) ) return;
	__Test_XWs2AtomicInc(&pCtx->iBinaryCount);
	iCopy = iLen < sizeof(pCtx->aLastBinary) ? iLen : sizeof(pCtx->aLastBinary);
	if ( iCopy > 0 ) memcpy(pCtx->aLastBinary, pData, iCopy);
	pCtx->iLastBinaryLen = iCopy;
}

static void __Test_XWs2ClientOnClose(ptr pOwner, xws2client* pClient, xnet_result iReason)
{
	__test_xws2_client_ctx* pCtx = (__test_xws2_client_ctx*)pOwner;
	(void)pClient;
	(void)iReason;
	if ( !pCtx ) return;
	__Test_XWs2AtomicInc(&pCtx->iCloseCount);
}

static void __Test_XWs2ClientOnError(ptr pOwner, xws2client* pClient, int iSysErr)
{
	__test_xws2_client_ctx* pCtx = (__test_xws2_client_ctx*)pOwner;
	(void)pClient;
	(void)iSysErr;
	if ( !pCtx ) return;
	__Test_XWs2AtomicInc(&pCtx->iErrorCount);
}

static void __Test_XWs2ClientOnPing(ptr pOwner, xws2client* pClient, const void* pData, size_t iLen)
{
	__test_xws2_client_ctx* pCtx = (__test_xws2_client_ctx*)pOwner;
	(void)pClient;
	(void)pData;
	(void)iLen;
	if ( !pCtx ) return;
	__Test_XWs2AtomicInc(&pCtx->iPingCount);
}

static void __Test_XWs2ClientOnPong(ptr pOwner, xws2client* pClient, const void* pData, size_t iLen)
{
	__test_xws2_client_ctx* pCtx = (__test_xws2_client_ctx*)pOwner;
	(void)pClient;
	(void)pData;
	(void)iLen;
	if ( !pCtx ) return;
	__Test_XWs2AtomicInc(&pCtx->iPongCount);
}

void Test_XNet2_Ws2(void)
{
	printf("\n\n\n------------------------------------\n\n XNet2 WebSocket Skeleton Test:\n\n");

	{
		__test_xws2_server_ctx tSrvCtx;
		__test_xws2_client_ctx tCliCtx;
		xws2serverevents tSrvEvents;
		xws2clientevents tCliEvents;
		xws2serverconfig tSrvCfg;
		xws2clientconfig tCliCfg;
		xnetengineconfig tEngineCfg;
		xnetengine* pServerEngine = NULL;
		xnetengine* pClientEngine = NULL;
		xws2server* pServer = NULL;
		xws2client* pClient = NULL;
		uint8 aBinary[] = { 1u, 2u, 3u, 4u };
		char sURL[256];

		memset(&tSrvCtx, 0, sizeof(tSrvCtx));
		memset(&tCliCtx, 0, sizeof(tCliCtx));
		memset(&tSrvEvents, 0, sizeof(tSrvEvents));
		memset(&tCliEvents, 0, sizeof(tCliEvents));
		tSrvEvents.OnOpen = __Test_XWs2ServerOnOpen;
		tSrvEvents.OnText = __Test_XWs2ServerOnText;
		tSrvEvents.OnBinary = __Test_XWs2ServerOnBinary;
		tSrvEvents.OnClose = __Test_XWs2ServerOnClose;
		tSrvEvents.OnError = __Test_XWs2ServerOnError;
		tSrvEvents.OnPing = __Test_XWs2ServerOnPing;
		tSrvEvents.OnPong = __Test_XWs2ServerOnPong;
		tCliEvents.OnOpen = __Test_XWs2ClientOnOpen;
		tCliEvents.OnText = __Test_XWs2ClientOnText;
		tCliEvents.OnBinary = __Test_XWs2ClientOnBinary;
		tCliEvents.OnClose = __Test_XWs2ClientOnClose;
		tCliEvents.OnError = __Test_XWs2ClientOnError;
		tCliEvents.OnPing = __Test_XWs2ClientOnPing;
		tCliEvents.OnPong = __Test_XWs2ClientOnPong;

		xrtNetEngineConfigInit(&tEngineCfg);
		tEngineCfg.iWorkerCount = 1;
		pServerEngine = xrtNetEngineCreate(&tEngineCfg);
		pClientEngine = xrtNetEngineCreate(&tEngineCfg);
		printf("  WS2 plain server engine create : %s\n", pServerEngine != NULL ? "PASS" : "FAIL");
		printf("  WS2 plain client engine create : %s\n", pClientEngine != NULL ? "PASS" : "FAIL");
		if ( pServerEngine ) printf("  WS2 plain server engine start : %s\n", xrtNetEngineStart(pServerEngine) == XRT_NET_OK ? "PASS" : "FAIL");
		if ( pClientEngine ) printf("  WS2 plain client engine start : %s\n", xrtNetEngineStart(pClientEngine) == XRT_NET_OK ? "PASS" : "FAIL");

		xrtWs2ServerConfigInit(&tSrvCfg);
		(void)xrtNetAddrParse(&tSrvCfg.tBindAddr, "127.0.0.1", 0);
		pServer = (pServerEngine && pClientEngine) ? xrtWs2ServerCreate(pServerEngine, &tSrvCfg, &tSrvEvents, &tSrvCtx) : NULL;
		printf("  WS2 plain server create : %s\n", pServer != NULL ? "PASS" : "FAIL");
		printf("  WS2 plain server start : %s\n", pServer && xrtWs2ServerStart(pServer) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  WS2 plain bound port assigned : %s\n", pServer && xrtWs2ServerBoundPort(pServer) > 0 ? "PASS" : "FAIL");

		xrtWs2ClientConfigInit(&tCliCfg);
		snprintf(sURL, sizeof(sURL), "ws://127.0.0.1:%u/chat?mode=plain", (unsigned)xrtWs2ServerBoundPort(pServer));
		snprintf(tCliCfg.sURL, sizeof(tCliCfg.sURL), "%s", sURL);
		pClient = pClientEngine ? xrtWs2ClientCreate(pClientEngine, &tCliCfg, &tCliEvents, &tCliCtx) : NULL;
		printf("  WS2 plain client create : %s\n", pClient != NULL ? "PASS" : "FAIL");
		printf("  WS2 plain client start : %s\n", pClient && xrtWs2ClientStart(pClient) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  WS2 plain server open : %s\n", __Test_XWs2WaitMin(&tSrvCtx.iOpenCount, 1, 2000u) ? "PASS" : "FAIL");
		printf("  WS2 plain client open : %s\n", __Test_XWs2WaitMin(&tCliCtx.iOpenCount, 1, 2000u) ? "PASS" : "FAIL");
		printf("  WS2 plain client open state : %s\n", pClient && xrtWs2ClientIsOpen(pClient) ? "PASS" : "FAIL");
		printf("  WS2 plain server conn open state : %s\n", tSrvCtx.pLastConn && xrtWs2ConnIsOpen(tSrvCtx.pLastConn) ? "PASS" : "FAIL");

		printf("  WS2 plain send text : %s\n", pClient && xrtWs2ClientSendText(pClient, "hello-ws", 8u) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  WS2 plain server text callback : %s\n", __Test_XWs2WaitMin(&tSrvCtx.iTextCount, 1, 2000u) ? "PASS" : "FAIL");
		printf("  WS2 plain client text echo : %s\n", __Test_XWs2WaitMin(&tCliCtx.iTextCount, 1, 2000u) ? "PASS" : "FAIL");
		printf("  WS2 plain server saw text : %s\n", strcmp(tSrvCtx.aLastText, "hello-ws") == 0 ? "PASS" : "FAIL");
		printf("  WS2 plain client saw text : %s\n", strcmp(tCliCtx.aLastText, "hello-ws") == 0 ? "PASS" : "FAIL");

		printf("  WS2 plain send binary : %s\n", pClient && xrtWs2ClientSendBinary(pClient, aBinary, sizeof(aBinary)) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  WS2 plain server binary callback : %s\n", __Test_XWs2WaitMin(&tSrvCtx.iBinaryCount, 1, 2000u) ? "PASS" : "FAIL");
		printf("  WS2 plain client binary echo : %s\n", __Test_XWs2WaitMin(&tCliCtx.iBinaryCount, 1, 2000u) ? "PASS" : "FAIL");
		printf("  WS2 plain server saw binary : %s\n", tSrvCtx.iLastBinaryLen == sizeof(aBinary) && memcmp(tSrvCtx.aLastBinary, aBinary, sizeof(aBinary)) == 0 ? "PASS" : "FAIL");
		printf("  WS2 plain client saw binary : %s\n", tCliCtx.iLastBinaryLen == sizeof(aBinary) && memcmp(tCliCtx.aLastBinary, aBinary, sizeof(aBinary)) == 0 ? "PASS" : "FAIL");

		printf("  WS2 plain send ping : %s\n", pClient && xrtWs2ClientPing(pClient, "hb", 2u) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  WS2 plain server ping callback : %s\n", __Test_XWs2WaitMin(&tSrvCtx.iPingCount, 1, 2000u) ? "PASS" : "FAIL");
		printf("  WS2 plain client pong callback : %s\n", __Test_XWs2WaitMin(&tCliCtx.iPongCount, 1, 2000u) ? "PASS" : "FAIL");

		{
			long iSrvTextBefore = __Test_XWs2AtomicLoad(&tSrvCtx.iTextCount);
			long iSrvPingBefore = __Test_XWs2AtomicLoad(&tSrvCtx.iPingCount);
			long iCliPongBefore = __Test_XWs2AtomicLoad(&tCliCtx.iPongCount);
			long iCliTextBefore = __Test_XWs2AtomicLoad(&tCliCtx.iTextCount);
			printf("  WS2 plain fragmented client text frame1 : %s\n", pClient && __xws2StreamSendFrameEx(pClient->pStream, false, true, XCODEC_WS_OPCODE_TEXT, "frag-", 5u) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS2 plain fragmented client ping : %s\n", pClient && __xws2StreamSendFrameEx(pClient->pStream, true, true, XCODEC_WS_OPCODE_PING, "f1", 2u) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS2 plain fragmented client text cont : %s\n", pClient && __xws2StreamSendFrameEx(pClient->pStream, true, true, XCODEC_WS_OPCODE_CONT, "ws", 2u) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS2 plain fragmented server text callback : %s\n", __Test_XWs2WaitMin(&tSrvCtx.iTextCount, iSrvTextBefore + 1, 2000u) ? "PASS" : "FAIL");
			printf("  WS2 plain fragmented server ping callback : %s\n", __Test_XWs2WaitMin(&tSrvCtx.iPingCount, iSrvPingBefore + 1, 2000u) ? "PASS" : "FAIL");
			printf("  WS2 plain fragmented client pong callback : %s\n", __Test_XWs2WaitMin(&tCliCtx.iPongCount, iCliPongBefore + 1, 2000u) ? "PASS" : "FAIL");
			printf("  WS2 plain fragmented client text echo : %s\n", __Test_XWs2WaitMin(&tCliCtx.iTextCount, iCliTextBefore + 1, 2000u) ? "PASS" : "FAIL");
			printf("  WS2 plain fragmented text reassembled : %s\n", strcmp(tSrvCtx.aLastText, "frag-ws") == 0 && strcmp(tCliCtx.aLastText, "frag-ws") == 0 ? "PASS" : "FAIL");
		}

		{
			static const uint8 aFragBinA[] = { 9u, 8u };
			static const uint8 aFragBinB[] = { 7u, 6u, 5u };
			static const uint8 aFragBinAll[] = { 9u, 8u, 7u, 6u, 5u };
			long iCliBinaryBefore = __Test_XWs2AtomicLoad(&tCliCtx.iBinaryCount);
			long iCliPingBefore = __Test_XWs2AtomicLoad(&tCliCtx.iPingCount);
			long iSrvPongBefore = __Test_XWs2AtomicLoad(&tSrvCtx.iPongCount);
			printf("  WS2 plain fragmented server binary frame1 : %s\n", tSrvCtx.pLastConn && __xws2StreamSendFrameEx(tSrvCtx.pLastConn->pStream, false, false, XCODEC_WS_OPCODE_BINARY, aFragBinA, sizeof(aFragBinA)) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS2 plain fragmented server ping : %s\n", tSrvCtx.pLastConn && __xws2StreamSendFrameEx(tSrvCtx.pLastConn->pStream, true, false, XCODEC_WS_OPCODE_PING, "sv", 2u) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS2 plain fragmented server binary cont : %s\n", tSrvCtx.pLastConn && __xws2StreamSendFrameEx(tSrvCtx.pLastConn->pStream, true, false, XCODEC_WS_OPCODE_CONT, aFragBinB, sizeof(aFragBinB)) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS2 plain fragmented client ping callback : %s\n", __Test_XWs2WaitMin(&tCliCtx.iPingCount, iCliPingBefore + 1, 2000u) ? "PASS" : "FAIL");
			printf("  WS2 plain fragmented server pong callback : %s\n", __Test_XWs2WaitMin(&tSrvCtx.iPongCount, iSrvPongBefore + 1, 2000u) ? "PASS" : "FAIL");
			printf("  WS2 plain fragmented client binary callback : %s\n", __Test_XWs2WaitMin(&tCliCtx.iBinaryCount, iCliBinaryBefore + 1, 2000u) ? "PASS" : "FAIL");
			printf("  WS2 plain fragmented binary reassembled : %s\n", tCliCtx.iLastBinaryLen == sizeof(aFragBinAll) && memcmp(tCliCtx.aLastBinary, aFragBinAll, sizeof(aFragBinAll)) == 0 ? "PASS" : "FAIL");
		}

		printf("  WS2 plain close request : %s\n", pClient && xrtWs2ClientClose(pClient, XWS2_CLOSE_NORMAL, "bye") == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  WS2 plain client close callback : %s\n", __Test_XWs2WaitMin(&tCliCtx.iCloseCount, 1, 3000u) ? "PASS" : "FAIL");
		printf("  WS2 plain server close callback : %s\n", __Test_XWs2WaitMin(&tSrvCtx.iCloseCount, 1, 3000u) ? "PASS" : "FAIL");
		printf("  WS2 plain client error free : %s\n", __Test_XWs2AtomicLoad(&tCliCtx.iErrorCount) == 0 ? "PASS" : "FAIL");
		printf("  WS2 plain server error free : %s\n", __Test_XWs2AtomicLoad(&tSrvCtx.iErrorCount) == 0 ? "PASS" : "FAIL");

		if ( pClient ) xrtWs2ClientDestroy(pClient);
		if ( pServer ) xrtWs2ServerDestroy(pServer);
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
		__test_xws2_server_ctx tSrvCtx;
		__test_xws2_client_ctx tCliCtx;
		xws2serverevents tSrvEvents;
		xws2clientevents tCliEvents;
		xws2serverconfig tSrvCfg;
		xws2clientconfig tCliCfg;
		xnetengineconfig tEngineCfg;
		xnetengine* pServerEngine = NULL;
		xnetengine* pClientEngine = NULL;
		xws2server* pServer = NULL;
		xws2client* pClient = NULL;
		xtlsconfig tTlsCfg;
		char sURL[256];

		memset(&tSrvCtx, 0, sizeof(tSrvCtx));
		memset(&tCliCtx, 0, sizeof(tCliCtx));
		memset(&tSrvEvents, 0, sizeof(tSrvEvents));
		memset(&tCliEvents, 0, sizeof(tCliEvents));
		memset(&tTlsCfg, 0, sizeof(tTlsCfg));
		tSrvEvents.OnOpen = __Test_XWs2ServerOnOpen;
		tSrvEvents.OnText = __Test_XWs2ServerOnText;
		tSrvEvents.OnBinary = __Test_XWs2ServerOnBinary;
		tSrvEvents.OnClose = __Test_XWs2ServerOnClose;
		tSrvEvents.OnError = __Test_XWs2ServerOnError;
		tSrvEvents.OnPing = __Test_XWs2ServerOnPing;
		tSrvEvents.OnPong = __Test_XWs2ServerOnPong;
		tCliEvents.OnOpen = __Test_XWs2ClientOnOpen;
		tCliEvents.OnText = __Test_XWs2ClientOnText;
		tCliEvents.OnBinary = __Test_XWs2ClientOnBinary;
		tCliEvents.OnClose = __Test_XWs2ClientOnClose;
		tCliEvents.OnError = __Test_XWs2ClientOnError;
		tCliEvents.OnPing = __Test_XWs2ClientOnPing;
		tCliEvents.OnPong = __Test_XWs2ClientOnPong;
		tTlsCfg.sCertFile = "dev/xnet2_tls_test_cert.pem";
		tTlsCfg.sKeyFile = "dev/xnet2_tls_test_key.pem";

		xrtNetEngineConfigInit(&tEngineCfg);
		tEngineCfg.iWorkerCount = 1;
		pServerEngine = xrtNetEngineCreate(&tEngineCfg);
		pClientEngine = xrtNetEngineCreate(&tEngineCfg);
		printf("  WS2 TLS fixture files exist : %s\n", __Test_XWs2FileExists(tTlsCfg.sCertFile) && __Test_XWs2FileExists(tTlsCfg.sKeyFile) ? "PASS" : "FAIL");
		printf("  WS2 TLS server engine create : %s\n", pServerEngine != NULL ? "PASS" : "FAIL");
		printf("  WS2 TLS client engine create : %s\n", pClientEngine != NULL ? "PASS" : "FAIL");
		if ( pServerEngine ) printf("  WS2 TLS server engine start : %s\n", xrtNetEngineStart(pServerEngine) == XRT_NET_OK ? "PASS" : "FAIL");
		if ( pClientEngine ) printf("  WS2 TLS client engine start : %s\n", xrtNetEngineStart(pClientEngine) == XRT_NET_OK ? "PASS" : "FAIL");

		xrtWs2ServerConfigInit(&tSrvCfg);
		(void)xrtNetAddrParse(&tSrvCfg.tBindAddr, "127.0.0.1", 0);
		tSrvCfg.pTlsConfig = &tTlsCfg;
		pServer = (pServerEngine && pClientEngine) ? xrtWs2ServerCreate(pServerEngine, &tSrvCfg, &tSrvEvents, &tSrvCtx) : NULL;
		printf("  WS2 TLS server create : %s\n", pServer != NULL ? "PASS" : "FAIL");
		printf("  WS2 TLS server start : %s\n", pServer && xrtWs2ServerStart(pServer) == XRT_NET_OK ? "PASS" : "FAIL");

		xrtWs2ClientConfigInit(&tCliCfg);
		snprintf(sURL, sizeof(sURL), "wss://127.0.0.1:%u/secure", (unsigned)xrtWs2ServerBoundPort(pServer));
		snprintf(tCliCfg.sURL, sizeof(tCliCfg.sURL), "%s", sURL);
		tCliCfg.bVerifyPeer = false;
		pClient = pClientEngine ? xrtWs2ClientCreate(pClientEngine, &tCliCfg, &tCliEvents, &tCliCtx) : NULL;
		printf("  WS2 TLS client create : %s\n", pClient != NULL ? "PASS" : "FAIL");
		printf("  WS2 TLS client start : %s\n", pClient && xrtWs2ClientStart(pClient) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  WS2 TLS server open : %s\n", __Test_XWs2WaitMin(&tSrvCtx.iOpenCount, 1, 4000u) ? "PASS" : "FAIL");
		printf("  WS2 TLS client open : %s\n", __Test_XWs2WaitMin(&tCliCtx.iOpenCount, 1, 4000u) ? "PASS" : "FAIL");
		printf("  WS2 TLS send text : %s\n", pClient && xrtWs2ClientSendText(pClient, "secure-ws", 9u) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  WS2 TLS server text callback : %s\n", __Test_XWs2WaitMin(&tSrvCtx.iTextCount, 1, 4000u) ? "PASS" : "FAIL");
		printf("  WS2 TLS client text echo : %s\n", __Test_XWs2WaitMin(&tCliCtx.iTextCount, 1, 4000u) ? "PASS" : "FAIL");
		printf("  WS2 TLS echoed text : %s\n", strcmp(tCliCtx.aLastText, "secure-ws") == 0 ? "PASS" : "FAIL");

		{
			long iSrvTextBefore = __Test_XWs2AtomicLoad(&tSrvCtx.iTextCount);
			long iSrvPingBefore = __Test_XWs2AtomicLoad(&tSrvCtx.iPingCount);
			long iCliPongBefore = __Test_XWs2AtomicLoad(&tCliCtx.iPongCount);
			long iCliTextBefore = __Test_XWs2AtomicLoad(&tCliCtx.iTextCount);
			printf("  WS2 TLS fragmented client text frame1 : %s\n", pClient && __xws2StreamSendFrameEx(pClient->pStream, false, true, XCODEC_WS_OPCODE_TEXT, "tls-", 4u) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS2 TLS fragmented client ping : %s\n", pClient && __xws2StreamSendFrameEx(pClient->pStream, true, true, XCODEC_WS_OPCODE_PING, "ts", 2u) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS2 TLS fragmented client text cont : %s\n", pClient && __xws2StreamSendFrameEx(pClient->pStream, true, true, XCODEC_WS_OPCODE_CONT, "frag", 4u) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS2 TLS fragmented server text callback : %s\n", __Test_XWs2WaitMin(&tSrvCtx.iTextCount, iSrvTextBefore + 1, 4000u) ? "PASS" : "FAIL");
			printf("  WS2 TLS fragmented server ping callback : %s\n", __Test_XWs2WaitMin(&tSrvCtx.iPingCount, iSrvPingBefore + 1, 4000u) ? "PASS" : "FAIL");
			printf("  WS2 TLS fragmented client pong callback : %s\n", __Test_XWs2WaitMin(&tCliCtx.iPongCount, iCliPongBefore + 1, 4000u) ? "PASS" : "FAIL");
			printf("  WS2 TLS fragmented client text echo : %s\n", __Test_XWs2WaitMin(&tCliCtx.iTextCount, iCliTextBefore + 1, 4000u) ? "PASS" : "FAIL");
			printf("  WS2 TLS fragmented text reassembled : %s\n", strcmp(tSrvCtx.aLastText, "tls-frag") == 0 && strcmp(tCliCtx.aLastText, "tls-frag") == 0 ? "PASS" : "FAIL");
		}

		printf("  WS2 TLS close request : %s\n", pClient && xrtWs2ClientClose(pClient, XWS2_CLOSE_NORMAL, NULL) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  WS2 TLS client close callback : %s\n", __Test_XWs2WaitMin(&tCliCtx.iCloseCount, 1, 4000u) ? "PASS" : "FAIL");
		printf("  WS2 TLS server close callback : %s\n", __Test_XWs2WaitMin(&tSrvCtx.iCloseCount, 1, 4000u) ? "PASS" : "FAIL");
		printf("  WS2 TLS client error free : %s\n", __Test_XWs2AtomicLoad(&tCliCtx.iErrorCount) == 0 ? "PASS" : "FAIL");
		printf("  WS2 TLS server error free : %s\n", __Test_XWs2AtomicLoad(&tSrvCtx.iErrorCount) == 0 ? "PASS" : "FAIL");

		if ( pClient ) xrtWs2ClientDestroy(pClient);
		if ( pServer ) xrtWs2ServerDestroy(pServer);
		if ( pClientEngine ) {
			xrtNetEngineStop(pClientEngine);
			xrtNetEngineDestroy(pClientEngine);
		}
		if ( pServerEngine ) {
			xrtNetEngineStop(pServerEngine);
			xrtNetEngineDestroy(pServerEngine);
		}
	}
}
