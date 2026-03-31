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
	volatile long iErrorCount;
	char aLastText[256];
	uint8 aLastBinary[256];
	size_t iLastBinaryLen;
	xwsconn* pLastConn;
} __test_xws_server_ctx;

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
} __test_xws_client_ctx;


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
	__Test_XWsAtomicInc(&pCtx->iTextCount);
	iCopy = iLen < (sizeof(pCtx->aLastText) - 1u) ? iLen : (sizeof(pCtx->aLastText) - 1u);
	memcpy(pCtx->aLastText, pData, iCopy);
	pCtx->aLastText[iCopy] = '\0';
	(void)xrtWsConnSendText(pConn, pData, iLen);
}


// 内部函数：__Test_XWsServerOnBinary
static void __Test_XWsServerOnBinary(ptr pOwner, xwsserver* pServer, xwsconn* pConn, const void* pData, size_t iLen)
{
	__test_xws_server_ctx* pCtx = (__test_xws_server_ctx*)pOwner;
	size_t iCopy;
	(void)pServer;
	if ( !pCtx || !pConn || (!pData && iLen != 0u) ) return;
	__Test_XWsAtomicInc(&pCtx->iBinaryCount);
	iCopy = iLen < sizeof(pCtx->aLastBinary) ? iLen : sizeof(pCtx->aLastBinary);
	if ( iCopy > 0 ) memcpy(pCtx->aLastBinary, pData, iCopy);
	pCtx->iLastBinaryLen = iCopy;
	(void)xrtWsConnSendBinary(pConn, pData, iLen);
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
	__Test_XWsAtomicInc(&pCtx->iTextCount);
	iCopy = iLen < (sizeof(pCtx->aLastText) - 1u) ? iLen : (sizeof(pCtx->aLastText) - 1u);
	memcpy(pCtx->aLastText, pData, iCopy);
	pCtx->aLastText[iCopy] = '\0';
}


// 内部函数：__Test_XWsClientOnBinary
static void __Test_XWsClientOnBinary(ptr pOwner, xwsclient* pClient, const void* pData, size_t iLen)
{
	__test_xws_client_ctx* pCtx = (__test_xws_client_ctx*)pOwner;
	size_t iCopy;
	(void)pClient;
	if ( !pCtx || (!pData && iLen != 0u) ) return;
	__Test_XWsAtomicInc(&pCtx->iBinaryCount);
	iCopy = iLen < sizeof(pCtx->aLastBinary) ? iLen : sizeof(pCtx->aLastBinary);
	if ( iCopy > 0 ) memcpy(pCtx->aLastBinary, pData, iCopy);
	pCtx->iLastBinaryLen = iCopy;
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
void Test_XNet_Ws(void)
{
	printf("\n\n\n------------------------------------\n\n XNet WebSocket Skeleton Test:\n\n");

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
		uint8 aBinary[] = { 1u, 2u, 3u, 4u };
		char sURL[256];

		memset(&tSrvCtx, 0, sizeof(tSrvCtx));
		memset(&tCliCtx, 0, sizeof(tCliCtx));
		memset(&tSrvEvents, 0, sizeof(tSrvEvents));
		memset(&tCliEvents, 0, sizeof(tCliEvents));
		tSrvEvents.OnOpen = __Test_XWsServerOnOpen;
		tSrvEvents.OnText = __Test_XWsServerOnText;
		tSrvEvents.OnBinary = __Test_XWsServerOnBinary;
		tSrvEvents.OnClose = __Test_XWsServerOnClose;
		tSrvEvents.OnError = __Test_XWsServerOnError;
		tSrvEvents.OnPing = __Test_XWsServerOnPing;
		tSrvEvents.OnPong = __Test_XWsServerOnPong;
		tCliEvents.OnOpen = __Test_XWsClientOnOpen;
		tCliEvents.OnText = __Test_XWsClientOnText;
		tCliEvents.OnBinary = __Test_XWsClientOnBinary;
		tCliEvents.OnClose = __Test_XWsClientOnClose;
		tCliEvents.OnError = __Test_XWsClientOnError;
		tCliEvents.OnPing = __Test_XWsClientOnPing;
		tCliEvents.OnPong = __Test_XWsClientOnPong;

		xrtNetEngineConfigInit(&tEngineCfg);
		tEngineCfg.iWorkerCount = 1;
		pServerEngine = xrtNetEngineCreate(&tEngineCfg);
		pClientEngine = xrtNetEngineCreate(&tEngineCfg);
		printf("  WS plain server engine create : %s\n", pServerEngine != NULL ? "PASS" : "FAIL");
		printf("  WS plain client engine create : %s\n", pClientEngine != NULL ? "PASS" : "FAIL");
		if ( pServerEngine ) printf("  WS plain server engine start : %s\n", xrtNetEngineStart(pServerEngine) == XRT_NET_OK ? "PASS" : "FAIL");
		if ( pClientEngine ) printf("  WS plain client engine start : %s\n", xrtNetEngineStart(pClientEngine) == XRT_NET_OK ? "PASS" : "FAIL");

		xrtWsServerConfigInit(&tSrvCfg);
		(void)xrtNetAddrParse(&tSrvCfg.tBindAddr, "127.0.0.1", 0);
		pServer = (pServerEngine && pClientEngine) ? xrtWsServerCreate(pServerEngine, &tSrvCfg, &tSrvEvents, &tSrvCtx) : NULL;
		printf("  WS plain server create : %s\n", pServer != NULL ? "PASS" : "FAIL");
		printf("  WS plain server start : %s\n", pServer && xrtWsServerStart(pServer) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  WS plain bound port assigned : %s\n", pServer && xrtWsServerBoundPort(pServer) > 0 ? "PASS" : "FAIL");

		xrtWsClientConfigInit(&tCliCfg);
		snprintf(sURL, sizeof(sURL), "ws://127.0.0.1:%u/chat?mode=plain", (unsigned)xrtWsServerBoundPort(pServer));
		snprintf(tCliCfg.sURL, sizeof(tCliCfg.sURL), "%s", sURL);
		pClient = pClientEngine ? xrtWsClientCreate(pClientEngine, &tCliCfg, &tCliEvents, &tCliCtx) : NULL;
		printf("  WS plain client create : %s\n", pClient != NULL ? "PASS" : "FAIL");
		printf("  WS plain client start : %s\n", pClient && xrtWsClientStart(pClient) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  WS plain server open : %s\n", __Test_XWsWaitMin(&tSrvCtx.iOpenCount, 1, 2000u) ? "PASS" : "FAIL");
		printf("  WS plain client open : %s\n", __Test_XWsWaitMin(&tCliCtx.iOpenCount, 1, 2000u) ? "PASS" : "FAIL");
		printf("  WS plain client open state : %s\n", pClient && xrtWsClientIsOpen(pClient) ? "PASS" : "FAIL");
		printf("  WS plain server conn open state : %s\n", tSrvCtx.pLastConn && xrtWsConnIsOpen(tSrvCtx.pLastConn) ? "PASS" : "FAIL");

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

		printf("  WS plain close request : %s\n", pClient && xrtWsClientClose(pClient, XWS_CLOSE_NORMAL, "bye") == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  WS plain client close callback : %s\n", __Test_XWsWaitMin(&tCliCtx.iCloseCount, 1, 3000u) ? "PASS" : "FAIL");
		printf("  WS plain server close callback : %s\n", __Test_XWsWaitMin(&tSrvCtx.iCloseCount, 1, 3000u) ? "PASS" : "FAIL");
		printf("  WS plain client error free : %s\n", __Test_XWsAtomicLoad(&tCliCtx.iErrorCount) == 0 ? "PASS" : "FAIL");
		printf("  WS plain server error free : %s\n", __Test_XWsAtomicLoad(&tSrvCtx.iErrorCount) == 0 ? "PASS" : "FAIL");
		if ( pClient ) {
			xrtWsClientDestroy(pClient);
			pClient = NULL;
		}

		{
			__test_xws_client_ctx tCliCtx2;
			long iSrvOpenBefore = __Test_XWsAtomicLoad(&tSrvCtx.iOpenCount);
			long iSrvCloseBefore = __Test_XWsAtomicLoad(&tSrvCtx.iCloseCount);
			long iSrvTextBefore = __Test_XWsAtomicLoad(&tSrvCtx.iTextCount);
			memset(&tCliCtx2, 0, sizeof(tCliCtx2));
			pClient = pClientEngine ? xrtWsClientCreate(pClientEngine, &tCliCfg, &tCliEvents, &tCliCtx2) : NULL;
			printf("  WS plain reaccept client create : %s\n", pClient != NULL ? "PASS" : "FAIL");
			printf("  WS plain reaccept client start : %s\n", pClient && xrtWsClientStart(pClient) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS plain reaccept server open : %s\n", __Test_XWsWaitMin(&tSrvCtx.iOpenCount, iSrvOpenBefore + 1, 2000u) ? "PASS" : "FAIL");
			printf("  WS plain reaccept client open : %s\n", __Test_XWsWaitMin(&tCliCtx2.iOpenCount, 1, 2000u) ? "PASS" : "FAIL");
			printf("  WS plain reaccept send text : %s\n", pClient && xrtWsClientSendText(pClient, "again-ws", 8u) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS plain reaccept server text callback : %s\n", __Test_XWsWaitMin(&tSrvCtx.iTextCount, iSrvTextBefore + 1, 2000u) ? "PASS" : "FAIL");
			printf("  WS plain reaccept client text echo : %s\n", __Test_XWsWaitMin(&tCliCtx2.iTextCount, 1, 2000u) ? "PASS" : "FAIL");
			printf("  WS plain reaccept echoed text : %s\n", strcmp(tCliCtx2.aLastText, "again-ws") == 0 && strcmp(tSrvCtx.aLastText, "again-ws") == 0 ? "PASS" : "FAIL");
			printf("  WS plain reaccept close request : %s\n", pClient && xrtWsClientClose(pClient, XWS_CLOSE_NORMAL, "bye2") == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS plain reaccept client close callback : %s\n", __Test_XWsWaitMin(&tCliCtx2.iCloseCount, 1, 3000u) ? "PASS" : "FAIL");
			printf("  WS plain reaccept server close callback : %s\n", __Test_XWsWaitMin(&tSrvCtx.iCloseCount, iSrvCloseBefore + 1, 3000u) ? "PASS" : "FAIL");
			printf("  WS plain reaccept client error free : %s\n", __Test_XWsAtomicLoad(&tCliCtx2.iErrorCount) == 0 ? "PASS" : "FAIL");
			printf("  WS plain reaccept server error free : %s\n", __Test_XWsAtomicLoad(&tSrvCtx.iErrorCount) == 0 ? "PASS" : "FAIL");
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
		memset(&tSrvEvents, 0, sizeof(tSrvEvents));
		memset(&tCliEvents, 0, sizeof(tCliEvents));
		memset(&tTlsCfg, 0, sizeof(tTlsCfg));
		tSrvEvents.OnOpen = __Test_XWsServerOnOpen;
		tSrvEvents.OnText = __Test_XWsServerOnText;
		tSrvEvents.OnBinary = __Test_XWsServerOnBinary;
		tSrvEvents.OnClose = __Test_XWsServerOnClose;
		tSrvEvents.OnError = __Test_XWsServerOnError;
		tSrvEvents.OnPing = __Test_XWsServerOnPing;
		tSrvEvents.OnPong = __Test_XWsServerOnPong;
		tCliEvents.OnOpen = __Test_XWsClientOnOpen;
		tCliEvents.OnText = __Test_XWsClientOnText;
		tCliEvents.OnBinary = __Test_XWsClientOnBinary;
		tCliEvents.OnClose = __Test_XWsClientOnClose;
		tCliEvents.OnError = __Test_XWsClientOnError;
		tCliEvents.OnPing = __Test_XWsClientOnPing;
		tCliEvents.OnPong = __Test_XWsClientOnPong;
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
			printf("  WS TLS send text : %s\n", pClient && xrtWsClientSendText(pClient, "secure-ws", 9u) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  WS TLS server text callback : %s\n", __Test_XWsWaitMin(&tSrvCtx.iTextCount, 1, 4000u) ? "PASS" : "FAIL");
			printf("  WS TLS client text echo : %s\n", __Test_XWsWaitMin(&tCliCtx.iTextCount, 1, 4000u) ? "PASS" : "FAIL");
			printf("  WS TLS echoed text : %s\n", strcmp(tCliCtx.aLastText, "secure-ws") == 0 ? "PASS" : "FAIL");

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
}
