/*
 * XRT Example - WS Client Proxy
 * XRT 范例 - XWS Client 代理接入
 *
 * Description / 说明:
 *   EN: Demonstrates attaching a shared HTTP CONNECT proxy object to xws client configuration and establishing a proxied local WebSocket connection.
 *   CN: 演示把共享的 HTTP CONNECT 代理对象接入 xws client 配置，并建立经过代理的本地 WebSocket 连接。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/xws_client_proxy.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/xws_client_proxy -lm -lpthread
 */

#include "../../../xrt.c"
#include "../../example_wait.h"
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#else
	#include <pthread.h>
	#include <unistd.h>
#endif

typedef struct {
	volatile long bRun;
	volatile long iAcceptCount;
	volatile long iTunnelCount;
	volatile long iConnectCount;
	xsocket hListen;
	uint16 iListenPort;
	char aLastConnectHost[256];
	uint16 iLastConnectPort;
	#if defined(_WIN32) || defined(_WIN64)
		HANDLE hThread;
	#else
		pthread_t hThread;
		bool bThreadStarted;
	#endif
} ex_proxy_server;

typedef struct {
	volatile long iOpenCount;
	volatile long iTextCount;
	volatile long iCloseCount;
	char aLastText[256];
} ex_ws_proxy_ctx;


static bool procSocketReadable(xsocket hSocket, uint32 iTimeoutMs)
{
	fd_set tReadSet;
	struct timeval tTv;
	int iRet;

	if ( hSocket == XNET_SOCKET_INVALID ) return false;

	FD_ZERO(&tReadSet);
	FD_SET(hSocket, &tReadSet);
	tTv.tv_sec = (long)(iTimeoutMs / 1000u);
	tTv.tv_usec = (long)((iTimeoutMs % 1000u) * 1000u);
	iRet = select((int)(hSocket + 1), &tReadSet, NULL, NULL, &tTv);
	return iRet > 0 && FD_ISSET(hSocket, &tReadSet);
}


static void procCloseSocket(xsocket* phSocket)
{
	if ( !phSocket || *phSocket == XNET_SOCKET_INVALID ) return;
	#if defined(_WIN32) || defined(_WIN64)
		closesocket(*phSocket);
	#else
		close(*phSocket);
	#endif
	*phSocket = XNET_SOCKET_INVALID;
}


static bool procSendAll(xsocket hSocket, const void* pData, size_t iLen)
{
	const char* pBytes = (const char*)pData;
	size_t iSent = 0u;

	while ( iSent < iLen ) {
		int iStep = send(hSocket, pBytes + iSent, (int)(iLen - iSent), 0);
		if ( iStep <= 0 ) return false;
		iSent += (size_t)iStep;
	}

	return true;
}


static bool procRecvHeader(xsocket hSocket, char* sBuf, size_t iCap, uint32 iTimeoutMs)
{
	size_t iLen = 0u;
	uint32 iLoops = (iTimeoutMs / 50u) + 1u;

	if ( !sBuf || iCap < 5u ) return false;
	sBuf[0] = '\0';

	while ( iLoops-- > 0u && iLen + 1u < iCap ) {
		int iRet;

		if ( strstr(sBuf, "\r\n\r\n") != NULL ) return true;
		if ( !procSocketReadable(hSocket, 50u) ) continue;

		iRet = recv(hSocket, sBuf + iLen, (int)(iCap - iLen - 1u), 0);
		if ( iRet <= 0 ) return false;
		iLen += (size_t)iRet;
		sBuf[iLen] = '\0';
	}

	return strstr(sBuf, "\r\n\r\n") != NULL;
}


static bool procCreateLoopbackListener(xsocket* phListen, uint16* piPort)
{
	struct sockaddr_in tAddr;
	struct sockaddr_in tBound;
	socklen_t iBoundLen = sizeof(tBound);
	xsocket hListen;
	int iOpt = 1;

	if ( phListen ) *phListen = XNET_SOCKET_INVALID;
	if ( piPort ) *piPort = 0u;

	hListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if ( hListen == XNET_SOCKET_INVALID ) return false;

	#if defined(_WIN32) || defined(_WIN64)
		setsockopt(hListen, SOL_SOCKET, SO_REUSEADDR, (const char*)&iOpt, sizeof(iOpt));
	#else
		setsockopt(hListen, SOL_SOCKET, SO_REUSEADDR, &iOpt, sizeof(iOpt));
	#endif

	memset(&tAddr, 0, sizeof(tAddr));
	tAddr.sin_family = AF_INET;
	tAddr.sin_port = htons(0u);
	tAddr.sin_addr.s_addr = htonl(0x7F000001u);
	if ( bind(hListen, (struct sockaddr*)&tAddr, sizeof(tAddr)) != 0 || listen(hListen, 8) != 0 ) {
		procCloseSocket(&hListen);
		return false;
	}
	if ( getsockname(hListen, (struct sockaddr*)&tBound, &iBoundLen) != 0 ) {
		procCloseSocket(&hListen);
		return false;
	}

	if ( phListen ) *phListen = hListen;
	if ( piPort ) *piPort = ntohs(tBound.sin_port);
	return true;
}


static bool procSplitAuthority(const char* sAuthority, char* sHost, size_t iHostCap, uint16* piPort)
{
	const char* pColon = strrchr(sAuthority, ':');
	size_t iHostLen;
	long iPort;

	if ( !sAuthority || !pColon || !sHost || !piPort ) return false;

	iHostLen = (size_t)(pColon - sAuthority);
	if ( iHostLen == 0u || iHostLen + 1u > iHostCap ) return false;
	memcpy(sHost, sAuthority, iHostLen);
	sHost[iHostLen] = '\0';

	iPort = strtol(pColon + 1, NULL, 10);
	if ( iPort <= 0 || iPort > 65535 ) return false;
	*piPort = (uint16)iPort;
	return true;
}


static xsocket procConnectTarget(const char* sHost, uint16 iPort)
{
	struct sockaddr_in tAddr;
	xsocket hSocket;

	hSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if ( hSocket == XNET_SOCKET_INVALID ) return XNET_SOCKET_INVALID;

	memset(&tAddr, 0, sizeof(tAddr));
	tAddr.sin_family = AF_INET;
	tAddr.sin_port = htons(iPort);
	tAddr.sin_addr.s_addr = inet_addr(sHost);
	if ( connect(hSocket, (struct sockaddr*)&tAddr, sizeof(tAddr)) != 0 ) {
		procCloseSocket(&hSocket);
		return XNET_SOCKET_INVALID;
	}

	return hSocket;
}


static bool procTunnel(ex_proxy_server* pProxy, xsocket hClient, xsocket hUpstream)
{
	char aBuf[4096];

	while ( pProxy && pProxy->bRun != 0 ) {
		fd_set tReadSet;
		struct timeval tTv;
		int iMaxFd = (int)((hClient > hUpstream ? hClient : hUpstream) + 1);
		int iRet;

		FD_ZERO(&tReadSet);
		FD_SET(hClient, &tReadSet);
		FD_SET(hUpstream, &tReadSet);
		tTv.tv_sec = 0;
		tTv.tv_usec = 100000;
		iRet = select(iMaxFd, &tReadSet, NULL, NULL, &tTv);
		if ( iRet <= 0 ) continue;

		if ( FD_ISSET(hClient, &tReadSet) ) {
			iRet = recv(hClient, aBuf, sizeof(aBuf), 0);
			if ( iRet <= 0 ) break;
			if ( !procSendAll(hUpstream, aBuf, (size_t)iRet) ) break;
		}
		if ( FD_ISSET(hUpstream, &tReadSet) ) {
			iRet = recv(hUpstream, aBuf, sizeof(aBuf), 0);
			if ( iRet <= 0 ) break;
			if ( !procSendAll(hClient, aBuf, (size_t)iRet) ) break;
		}
	}

	procCloseSocket(&hUpstream);
	procCloseSocket(&hClient);
	return true;
}


static bool procHandleConnect(ex_proxy_server* pProxy, xsocket hClient)
{
	char aHeader[2048];
	char aAuthority[384];
	char aHost[256];
	char* pLineEnd;
	uint16 iPort = 0u;
	xsocket hUpstream = XNET_SOCKET_INVALID;

	memset(aHeader, 0, sizeof(aHeader));
	memset(aAuthority, 0, sizeof(aAuthority));
	memset(aHost, 0, sizeof(aHost));

	if ( !procRecvHeader(hClient, aHeader, sizeof(aHeader), 3000u) ) return false;
	pLineEnd = strstr(aHeader, "\r\n");
	if ( !pLineEnd ) return false;

	*pLineEnd = '\0';
	if ( sscanf(aHeader, "CONNECT %383s HTTP/1.1", aAuthority) != 1 ) return false;
	if ( !procSplitAuthority(aAuthority, aHost, sizeof(aHost), &iPort) ) return false;

	snprintf(pProxy->aLastConnectHost, sizeof(pProxy->aLastConnectHost), "%s", aHost);
	pProxy->iLastConnectPort = iPort;
	++pProxy->iConnectCount;

	hUpstream = procConnectTarget(aHost, iPort);
	if ( hUpstream == XNET_SOCKET_INVALID ) return false;
	if ( !procSendAll(hClient, "HTTP/1.1 200 Connection Established\r\n\r\n", 39u) ) {
		procCloseSocket(&hUpstream);
		return false;
	}

	++pProxy->iTunnelCount;
	return procTunnel(pProxy, hClient, hUpstream);
}


#if defined(_WIN32) || defined(_WIN64)
static DWORD WINAPI procProxyThread(LPVOID pArg)
#else
static void* procProxyThread(void* pArg)
#endif
{
	ex_proxy_server* pProxy = (ex_proxy_server*)pArg;

	while ( pProxy && pProxy->bRun != 0 ) {
		struct sockaddr_storage tStorage;
		socklen_t iAddrLen = sizeof(tStorage);
		xsocket hClient;

		if ( !procSocketReadable(pProxy->hListen, 100u) ) continue;
		hClient = accept(pProxy->hListen, (struct sockaddr*)&tStorage, &iAddrLen);
		if ( hClient == XNET_SOCKET_INVALID ) continue;

		++pProxy->iAcceptCount;
		(void)procHandleConnect(pProxy, hClient);
		procCloseSocket(&hClient);
	}

	#if defined(_WIN32) || defined(_WIN64)
		return 0;
	#else
		return NULL;
	#endif
}


static bool procProxyStart(ex_proxy_server* pProxy)
{
	memset(pProxy, 0, sizeof(*pProxy));
	if ( !procCreateLoopbackListener(&pProxy->hListen, &pProxy->iListenPort) ) return false;
	pProxy->bRun = 1;

	#if defined(_WIN32) || defined(_WIN64)
		pProxy->hThread = CreateThread(NULL, 0, procProxyThread, pProxy, 0, NULL);
		return pProxy->hThread != NULL;
	#else
		if ( pthread_create(&pProxy->hThread, NULL, procProxyThread, pProxy) != 0 ) return false;
		pProxy->bThreadStarted = true;
		return true;
	#endif
}


static void procProxyStop(ex_proxy_server* pProxy)
{
	if ( !pProxy ) return;
	pProxy->bRun = 0;
	procCloseSocket(&pProxy->hListen);

	#if defined(_WIN32) || defined(_WIN64)
		if ( pProxy->hThread ) {
			WaitForSingleObject(pProxy->hThread, INFINITE);
			CloseHandle(pProxy->hThread);
			pProxy->hThread = NULL;
		}
	#else
		if ( pProxy->bThreadStarted ) {
			pthread_join(pProxy->hThread, NULL);
			pProxy->bThreadStarted = false;
		}
	#endif
}


static void procServerOnOpen(ptr pOwner, xwsserver* pServer, xwsconn* pConn)
{
	ex_ws_proxy_ctx* pCtx = (ex_ws_proxy_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	if ( !pCtx ) return;
	++pCtx->iOpenCount;
}


static void procServerOnText(ptr pOwner, xwsserver* pServer, xwsconn* pConn, const char* pData, size_t iLen)
{
	ex_ws_proxy_ctx* pCtx = (ex_ws_proxy_ctx*)pOwner;
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


static void procServerOnClose(ptr pOwner, xwsserver* pServer, xwsconn* pConn, xnet_result iReason)
{
	ex_ws_proxy_ctx* pCtx = (ex_ws_proxy_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	(void)iReason;
	if ( !pCtx ) return;
	++pCtx->iCloseCount;
}


static void procClientOnOpen(ptr pOwner, xwsclient* pClient)
{
	ex_ws_proxy_ctx* pCtx = (ex_ws_proxy_ctx*)pOwner;
	(void)pClient;
	if ( !pCtx ) return;
	++pCtx->iOpenCount;
}


static void procClientOnText(ptr pOwner, xwsclient* pClient, const char* pData, size_t iLen)
{
	ex_ws_proxy_ctx* pCtx = (ex_ws_proxy_ctx*)pOwner;
	size_t iCopy = 0u;
	(void)pClient;
	if ( !pCtx || !pData ) return;

	++pCtx->iTextCount;
	iCopy = iLen;
	if ( iCopy + 1u > sizeof(pCtx->aLastText) ) iCopy = sizeof(pCtx->aLastText) - 1u;
	memcpy(pCtx->aLastText, pData, iCopy);
	pCtx->aLastText[iCopy] = '\0';
}


static void procClientOnClose(ptr pOwner, xwsclient* pClient, xnet_result iReason)
{
	ex_ws_proxy_ctx* pCtx = (ex_ws_proxy_ctx*)pOwner;
	(void)pClient;
	(void)iReason;
	if ( !pCtx ) return;
	++pCtx->iCloseCount;
}


int main(void)
{
	ex_proxy_server tProxy;
	ex_ws_proxy_ctx tSrvCtx;
	ex_ws_proxy_ctx tCliCtx;
	xwsserverevents tSrvEvents;
	xwsclientevents tCliEvents;
	xwsserverconfig tSrvCfg;
	xwsclientconfig tCliCfg;
	xnetproxyconfig tProxyCfg;
	xnetengineconfig tEngineCfg;
	xnetengine* pServerEngine = NULL;
	xnetengine* pClientEngine = NULL;
	xwsserver* pServer = NULL;
	xwsclient* pClient = NULL;
	xnetproxy* pProxy = NULL;
	char sURL[256];
	int iRet = 0;

	xrtInit();
	memset(&tProxy, 0, sizeof(tProxy));
	memset(&tSrvCtx, 0, sizeof(tSrvCtx));
	memset(&tCliCtx, 0, sizeof(tCliCtx));
	memset(&tSrvEvents, 0, sizeof(tSrvEvents));
	memset(&tCliEvents, 0, sizeof(tCliEvents));
	memset(&tSrvCfg, 0, sizeof(tSrvCfg));
	memset(&tCliCfg, 0, sizeof(tCliCfg));
	memset(&tProxyCfg, 0, sizeof(tProxyCfg));
	memset(&tEngineCfg, 0, sizeof(tEngineCfg));
	memset(sURL, 0, sizeof(sURL));

	if ( !procProxyStart(&tProxy) ) {
		printf("proxy = failed\n");
		iRet = 1;
		goto cleanup;
	}

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

	xrtNetProxyConfigInit(&tProxyCfg);
	tProxyCfg.iType = XNET_PROXY_HTTP_CONNECT;
	snprintf(tProxyCfg.sHost, sizeof(tProxyCfg.sHost), "127.0.0.1");
	tProxyCfg.iPort = tProxy.iListenPort;
	pProxy = xrtNetProxyCreate(&tProxyCfg);
	if ( !pProxy ) {
		printf("proxy_object = failed\n");
		iRet = 1;
		goto cleanup;
	}

	xrtWsClientConfigInit(&tCliCfg);
	snprintf(sURL, sizeof(sURL), "ws://127.0.0.1:%u/proxy-ws", (unsigned)xrtWsServerBoundPort(pServer));
	snprintf(tCliCfg.sURL, sizeof(tCliCfg.sURL), "%s", sURL);
	tCliCfg.pProxy = pProxy;
	pClient = xrtWsClientCreate(pClientEngine, &tCliCfg, &tCliEvents, &tCliCtx);
	xrtNetProxyRelease(pProxy);
	pProxy = NULL;
	if ( !pClient || xrtWsClientStart(pClient) != XRT_NET_OK ) {
		printf("client = failed\n");
		iRet = 1;
		goto cleanup;
	}

	if ( !exWaitLongMin(&tSrvCtx.iOpenCount, 1, 4000u) || !exWaitLongMin(&tCliCtx.iOpenCount, 1, 4000u) ) {
		printf("open = timeout\n");
		iRet = 1;
		goto cleanup;
	}
	if ( xrtWsClientSendText(pClient, "proxy-ws", 8u) != XRT_NET_OK ) {
		printf("send_text = failed\n");
		iRet = 1;
		goto cleanup;
	}
	if ( !exWaitLongMin(&tSrvCtx.iTextCount, 1, 4000u) || !exWaitLongMin(&tCliCtx.iTextCount, 1, 4000u) ) {
		printf("echo = timeout\n");
		iRet = 1;
		goto cleanup;
	}

	(void)xrtWsClientClose(pClient, XWS_CLOSE_NORMAL, "bye");
	(void)exWaitLongMin(&tCliCtx.iCloseCount, 1, 3000u);
	(void)exWaitLongMin(&tSrvCtx.iCloseCount, 1, 3000u);

	printf("url = %s\n", sURL);
	printf("proxy_port = %u\n", (unsigned)tProxy.iListenPort);
	printf("proxy_accept = %ld\n", tProxy.iAcceptCount);
	printf("proxy_connect = %ld\n", tProxy.iConnectCount);
	printf("proxy_tunnel = %ld\n", tProxy.iTunnelCount);
	printf("proxy_target = %s:%u\n", tProxy.aLastConnectHost, (unsigned)tProxy.iLastConnectPort);
	printf("server_text = %s\n", tSrvCtx.aLastText);
	printf("client_text = %s\n", tCliCtx.aLastText);

cleanup:
	if ( pProxy ) xrtNetProxyRelease(pProxy);
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
	procProxyStop(&tProxy);
	xrtUnit();
	return iRet;
}
