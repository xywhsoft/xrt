/*
 * XRT Example - WS Server Basic
 * XRT 范例 - XWS Server 基础
 *
 * Description / 说明:
 *   EN: Demonstrates a local WebSocket server actively sending a welcome message on open and replying to client text messages.
 *   CN: 演示本地 WebSocket server 在连接打开时主动发送 welcome 消息，并对 client 文本消息作出回复。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/xws_server_basic.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/xws_server_basic -lm -lpthread
 */

#include "../../../xrt.c"
#include "../../example_wait.h"
#include <stdio.h>

typedef struct {
	volatile long iOpenCount;
	volatile long iTextCount;
	volatile long iCloseCount;
	char aLastText[256];
} ex_ws_server_ctx;

typedef struct {
	volatile long iOpenCount;
	volatile long iTextCount;
	volatile long iCloseCount;
	char aFirstText[256];
	char aLastText[256];
} ex_ws_client_ctx;


static void procServerOnOpen(ptr pOwner, xwsserver* pServer, xwsconn* pConn)
{
	ex_ws_server_ctx* pCtx = (ex_ws_server_ctx*)pOwner;
	(void)pServer;
	if ( !pCtx || !pConn ) return;
	++pCtx->iOpenCount;
	(void)xrtWsConnSendText(pConn, "welcome", 7u);
}


static void procServerOnText(ptr pOwner, xwsserver* pServer, xwsconn* pConn, const char* pData, size_t iLen)
{
	ex_ws_server_ctx* pCtx = (ex_ws_server_ctx*)pOwner;
	char sReply[256];
	size_t iCopy = 0u;
	(void)pServer;
	if ( !pCtx || !pConn || !pData ) return;

	++pCtx->iTextCount;
	iCopy = iLen;
	if ( iCopy + 1u > sizeof(pCtx->aLastText) ) iCopy = sizeof(pCtx->aLastText) - 1u;
	memcpy(pCtx->aLastText, pData, iCopy);
	pCtx->aLastText[iCopy] = '\0';

	snprintf(sReply, sizeof(sReply), "echo:%s", pCtx->aLastText);
	(void)xrtWsConnSendText(pConn, sReply, strlen(sReply));
}


static void procServerOnClose(ptr pOwner, xwsserver* pServer, xwsconn* pConn, xnet_result iReason)
{
	ex_ws_server_ctx* pCtx = (ex_ws_server_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	(void)iReason;
	if ( !pCtx ) return;
	++pCtx->iCloseCount;
}


static void procClientOnOpen(ptr pOwner, xwsclient* pClient)
{
	ex_ws_client_ctx* pCtx = (ex_ws_client_ctx*)pOwner;
	(void)pClient;
	if ( !pCtx ) return;
	++pCtx->iOpenCount;
}


static void procClientOnText(ptr pOwner, xwsclient* pClient, const char* pData, size_t iLen)
{
	ex_ws_client_ctx* pCtx = (ex_ws_client_ctx*)pOwner;
	size_t iCopy = 0u;
	(void)pClient;
	if ( !pCtx || !pData ) return;

	if ( pCtx->iTextCount == 0 ) {
		iCopy = iLen;
		if ( iCopy + 1u > sizeof(pCtx->aFirstText) ) iCopy = sizeof(pCtx->aFirstText) - 1u;
		memcpy(pCtx->aFirstText, pData, iCopy);
		pCtx->aFirstText[iCopy] = '\0';
	}

	++pCtx->iTextCount;
	iCopy = iLen;
	if ( iCopy + 1u > sizeof(pCtx->aLastText) ) iCopy = sizeof(pCtx->aLastText) - 1u;
	memcpy(pCtx->aLastText, pData, iCopy);
	pCtx->aLastText[iCopy] = '\0';
}


static void procClientOnClose(ptr pOwner, xwsclient* pClient, xnet_result iReason)
{
	ex_ws_client_ctx* pCtx = (ex_ws_client_ctx*)pOwner;
	(void)pClient;
	(void)iReason;
	if ( !pCtx ) return;
	++pCtx->iCloseCount;
}


int main(void)
{
	ex_ws_server_ctx tSrvCtx;
	ex_ws_client_ctx tCliCtx;
	xwsserverevents tSrvEvents;
	xwsclientevents tCliEvents;
	xwsserverconfig tSrvCfg;
	xwsclientconfig tCliCfg;
	xnetengineconfig tEngineCfg;
	xnetengine* pServerEngine = NULL;
	xnetengine* pClientEngine = NULL;
	xwsserver* pServer = NULL;
	xwsclient* pClient = NULL;
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
	tSrvEvents.OnClose = procServerOnClose;

	tCliEvents.OnOpen = procClientOnOpen;
	tCliEvents.OnText = procClientOnText;
	tCliEvents.OnClose = procClientOnClose;

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
	snprintf(sURL, sizeof(sURL), "ws://127.0.0.1:%u/server-basic", (unsigned)xrtWsServerBoundPort(pServer));
	snprintf(tCliCfg.sURL, sizeof(tCliCfg.sURL), "%s", sURL);
	pClient = xrtWsClientCreate(pClientEngine, &tCliCfg, &tCliEvents, &tCliCtx);
	if ( !pClient || xrtWsClientStart(pClient) != XRT_NET_OK ) {
		printf("client = failed\n");
		iRet = 1;
		goto cleanup;
	}

	if ( !exWaitLongMin(&tSrvCtx.iOpenCount, 1, 3000u) || !exWaitLongMin(&tCliCtx.iOpenCount, 1, 3000u) || !exWaitLongMin(&tCliCtx.iTextCount, 1, 3000u) ) {
		printf("open = timeout\n");
		iRet = 1;
		goto cleanup;
	}
	if ( xrtWsClientSendText(pClient, "hello-server", 12u) != XRT_NET_OK ) {
		printf("send_text = failed\n");
		iRet = 1;
		goto cleanup;
	}
	if ( !exWaitLongMin(&tSrvCtx.iTextCount, 1, 3000u) || !exWaitLongMin(&tCliCtx.iTextCount, 2, 3000u) ) {
		printf("reply = timeout\n");
		iRet = 1;
		goto cleanup;
	}

	(void)xrtWsClientClose(pClient, XWS_CLOSE_NORMAL, "bye");
	(void)exWaitLongMin(&tCliCtx.iCloseCount, 1, 3000u);
	(void)exWaitLongMin(&tSrvCtx.iCloseCount, 1, 3000u);

	printf("url = %s\n", sURL);
	printf("server_open = %ld\n", tSrvCtx.iOpenCount);
	printf("server_text = %ld\n", tSrvCtx.iTextCount);
	printf("server_last_text = %s\n", tSrvCtx.aLastText);
	printf("client_open = %ld\n", tCliCtx.iOpenCount);
	printf("client_first_text = %s\n", tCliCtx.aFirstText);
	printf("client_last_text = %s\n", tCliCtx.aLastText);

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
