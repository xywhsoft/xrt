/*
 * XRT Example - WS Client Basic
 * XRT 范例 - XWS Client 基础
 *
 * Description / 说明:
 *   EN: Demonstrates starting a local WebSocket client, sending text/binary/ping frames, and receiving echoed callbacks from a local server.
 *   CN: 演示启动本地 WebSocket client，发送 text/binary/ping 帧，并从本地 server 接收回显回调。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/xws_client_basic.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/xws_client_basic -lm -lpthread
 */

#include "../../../xrt.c"
#include "../../example_wait.h"
#include <stdio.h>

typedef struct {
	volatile long iOpenCount;
	volatile long iTextCount;
	volatile long iBinaryCount;
	volatile long iPingCount;
	volatile long iPongCount;
	volatile long iCloseCount;
	char aLastText[256];
	uint8 aLastBinary[256];
	size_t iLastBinaryLen;
} ex_ws_basic_ctx;


static void procCopyBytes(uint8* pDst, size_t iCap, const void* pSrc, size_t iLen, size_t* pOutLen)
{
	size_t iCopy = 0u;

	if ( pOutLen ) *pOutLen = 0u;
	if ( !pDst || iCap == 0u || (!pSrc && iLen != 0u) ) return;
	iCopy = iLen;
	if ( iCopy > iCap ) iCopy = iCap;
	if ( iCopy > 0u ) memcpy(pDst, pSrc, iCopy);
	if ( pOutLen ) *pOutLen = iCopy;
}


static void procServerOnOpen(ptr pOwner, xwsserver* pServer, xwsconn* pConn)
{
	ex_ws_basic_ctx* pCtx = (ex_ws_basic_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	if ( !pCtx ) return;
	++pCtx->iOpenCount;
}


static void procServerOnText(ptr pOwner, xwsserver* pServer, xwsconn* pConn, const char* pData, size_t iLen)
{
	ex_ws_basic_ctx* pCtx = (ex_ws_basic_ctx*)pOwner;
	size_t iCopy = 0u;
	(void)pServer;
	if ( !pCtx || !pConn || !pData ) return;

	++pCtx->iTextCount;
	iCopy = iLen;
	if ( iCopy + 1u > sizeof(pCtx->aLastText) ) iCopy = sizeof(pCtx->aLastText) - 1u;
	memcpy(pCtx->aLastText, pData, iCopy);
	pCtx->aLastText[iCopy] = '\0';
	(void)xrtWsConnSendText(pConn, pData, iLen);
}


static void procServerOnBinary(ptr pOwner, xwsserver* pServer, xwsconn* pConn, const void* pData, size_t iLen)
{
	ex_ws_basic_ctx* pCtx = (ex_ws_basic_ctx*)pOwner;
	(void)pServer;
	if ( !pCtx || !pConn || (!pData && iLen != 0u) ) return;

	++pCtx->iBinaryCount;
	procCopyBytes(pCtx->aLastBinary, sizeof(pCtx->aLastBinary), pData, iLen, &pCtx->iLastBinaryLen);
	(void)xrtWsConnSendBinary(pConn, pData, iLen);
}


static void procServerOnClose(ptr pOwner, xwsserver* pServer, xwsconn* pConn, xnet_result iReason)
{
	ex_ws_basic_ctx* pCtx = (ex_ws_basic_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	(void)iReason;
	if ( !pCtx ) return;
	++pCtx->iCloseCount;
}


static void procServerOnPing(ptr pOwner, xwsserver* pServer, xwsconn* pConn, const void* pData, size_t iLen)
{
	ex_ws_basic_ctx* pCtx = (ex_ws_basic_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	(void)pData;
	(void)iLen;
	if ( !pCtx ) return;
	++pCtx->iPingCount;
}


static void procClientOnOpen(ptr pOwner, xwsclient* pClient)
{
	ex_ws_basic_ctx* pCtx = (ex_ws_basic_ctx*)pOwner;
	(void)pClient;
	if ( !pCtx ) return;
	++pCtx->iOpenCount;
}


static void procClientOnText(ptr pOwner, xwsclient* pClient, const char* pData, size_t iLen)
{
	ex_ws_basic_ctx* pCtx = (ex_ws_basic_ctx*)pOwner;
	size_t iCopy = 0u;
	(void)pClient;
	if ( !pCtx || !pData ) return;

	++pCtx->iTextCount;
	iCopy = iLen;
	if ( iCopy + 1u > sizeof(pCtx->aLastText) ) iCopy = sizeof(pCtx->aLastText) - 1u;
	memcpy(pCtx->aLastText, pData, iCopy);
	pCtx->aLastText[iCopy] = '\0';
}


static void procClientOnBinary(ptr pOwner, xwsclient* pClient, const void* pData, size_t iLen)
{
	ex_ws_basic_ctx* pCtx = (ex_ws_basic_ctx*)pOwner;
	(void)pClient;
	if ( !pCtx || (!pData && iLen != 0u) ) return;

	++pCtx->iBinaryCount;
	procCopyBytes(pCtx->aLastBinary, sizeof(pCtx->aLastBinary), pData, iLen, &pCtx->iLastBinaryLen);
}


static void procClientOnClose(ptr pOwner, xwsclient* pClient, xnet_result iReason)
{
	ex_ws_basic_ctx* pCtx = (ex_ws_basic_ctx*)pOwner;
	(void)pClient;
	(void)iReason;
	if ( !pCtx ) return;
	++pCtx->iCloseCount;
}


static void procClientOnPong(ptr pOwner, xwsclient* pClient, const void* pData, size_t iLen)
{
	ex_ws_basic_ctx* pCtx = (ex_ws_basic_ctx*)pOwner;
	(void)pClient;
	(void)pData;
	(void)iLen;
	if ( !pCtx ) return;
	++pCtx->iPongCount;
}


int main(void)
{
	ex_ws_basic_ctx tSrvCtx;
	ex_ws_basic_ctx tCliCtx;
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
	int iRet = 0;

	xrtInit();
	memset(&tSrvCtx, 0, sizeof(tSrvCtx));
	memset(&tCliCtx, 0, sizeof(tCliCtx));
	memset(&tSrvEvents, 0, sizeof(tSrvEvents));
	memset(&tCliEvents, 0, sizeof(tCliEvents));
	memset(&tSrvCfg, 0, sizeof(tSrvCfg));
	memset(&tCliCfg, 0, sizeof(tCliCfg));
	memset(&tEngineCfg, 0, sizeof(tEngineCfg));
	memset(sURL, 0, sizeof(sURL));

	tSrvEvents.OnOpen = procServerOnOpen;
	tSrvEvents.OnText = procServerOnText;
	tSrvEvents.OnBinary = procServerOnBinary;
	tSrvEvents.OnClose = procServerOnClose;
	tSrvEvents.OnPing = procServerOnPing;

	tCliEvents.OnOpen = procClientOnOpen;
	tCliEvents.OnText = procClientOnText;
	tCliEvents.OnBinary = procClientOnBinary;
	tCliEvents.OnClose = procClientOnClose;
	tCliEvents.OnPong = procClientOnPong;

	xrtNetEngineConfigInit(&tEngineCfg);
	tEngineCfg.iWorkerCount = 1u;
	pServerEngine = xrtNetEngineCreate(&tEngineCfg);
	pClientEngine = xrtNetEngineCreate(&tEngineCfg);
	if ( !pServerEngine || !pClientEngine ) {
		printf("engine = failed\n");
		iRet = 1;
		goto cleanup;
	}
	if ( xrtNetEngineStart(pServerEngine) != XRT_NET_OK || xrtNetEngineStart(pClientEngine) != XRT_NET_OK ) {
		printf("engine_start = failed\n");
		iRet = 1;
		goto cleanup;
	}

	xrtWsServerConfigInit(&tSrvCfg);
	(void)xrtNetAddrParse(&tSrvCfg.tBindAddr, "127.0.0.1", 0u);
	pServer = xrtWsServerCreate(pServerEngine, &tSrvCfg, &tSrvEvents, &tSrvCtx);
	if ( !pServer || xrtWsServerStart(pServer) != XRT_NET_OK ) {
		printf("server = failed\n");
		iRet = 1;
		goto cleanup;
	}

	xrtWsClientConfigInit(&tCliCfg);
	snprintf(sURL, sizeof(sURL), "ws://127.0.0.1:%u/basic", (unsigned)xrtWsServerBoundPort(pServer));
	snprintf(tCliCfg.sURL, sizeof(tCliCfg.sURL), "%s", sURL);
	pClient = xrtWsClientCreate(pClientEngine, &tCliCfg, &tCliEvents, &tCliCtx);
	if ( !pClient || xrtWsClientStart(pClient) != XRT_NET_OK ) {
		printf("client = failed\n");
		iRet = 1;
		goto cleanup;
	}

	if ( !exWaitLongMin(&tSrvCtx.iOpenCount, 1, 3000u) || !exWaitLongMin(&tCliCtx.iOpenCount, 1, 3000u) ) {
		printf("open = timeout\n");
		iRet = 1;
		goto cleanup;
	}
	if ( xrtWsClientSendText(pClient, "hello-ws", 8u) != XRT_NET_OK ) {
		printf("send_text = failed\n");
		iRet = 1;
		goto cleanup;
	}
	if ( !exWaitLongMin(&tSrvCtx.iTextCount, 1, 3000u) || !exWaitLongMin(&tCliCtx.iTextCount, 1, 3000u) ) {
		printf("text = timeout\n");
		iRet = 1;
		goto cleanup;
	}
	if ( xrtWsClientSendBinary(pClient, aBinary, sizeof(aBinary)) != XRT_NET_OK ) {
		printf("send_binary = failed\n");
		iRet = 1;
		goto cleanup;
	}
	if ( !exWaitLongMin(&tSrvCtx.iBinaryCount, 1, 3000u) || !exWaitLongMin(&tCliCtx.iBinaryCount, 1, 3000u) ) {
		printf("binary = timeout\n");
		iRet = 1;
		goto cleanup;
	}
	if ( xrtWsClientPing(pClient, "hb", 2u) != XRT_NET_OK ) {
		printf("send_ping = failed\n");
		iRet = 1;
		goto cleanup;
	}
	if ( !exWaitLongMin(&tSrvCtx.iPingCount, 1, 3000u) || !exWaitLongMin(&tCliCtx.iPongCount, 1, 3000u) ) {
		printf("ping_pong = timeout\n");
		iRet = 1;
		goto cleanup;
	}
	(void)xrtWsClientClose(pClient, XWS_CLOSE_NORMAL, "bye");
	(void)exWaitLongMin(&tCliCtx.iCloseCount, 1, 3000u);
	(void)exWaitLongMin(&tSrvCtx.iCloseCount, 1, 3000u);

	printf("url = %s\n", sURL);
	printf("server_open = %ld\n", tSrvCtx.iOpenCount);
	printf("client_open = %ld\n", tCliCtx.iOpenCount);
	printf("server_text = %s\n", tSrvCtx.aLastText);
	printf("client_text = %s\n", tCliCtx.aLastText);
	printf("server_binary_len = %u\n", (unsigned)tSrvCtx.iLastBinaryLen);
	printf("client_binary_len = %u\n", (unsigned)tCliCtx.iLastBinaryLen);
	printf("server_ping = %ld\n", tSrvCtx.iPingCount);
	printf("client_pong = %ld\n", tCliCtx.iPongCount);

cleanup:
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
	xrtUnit();
	return iRet;
}
