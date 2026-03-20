#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#else
	#include <pthread.h>
	#include <unistd.h>
#endif

typedef struct {
	int iType;
	bool bRequireAuth;
	volatile long bRun;
	volatile long iAcceptCount;
	volatile long iTunnelCount;
	volatile long iAuthCount;
	volatile long iConnectCount;
	volatile long iLastConnectPort;
	xsocket hListen;
	uint16 iListenPort;
	char sUser[64];
	char sPass[64];
	char aLastConnectHost[256];
	#if defined(_WIN32) || defined(_WIN64)
		HANDLE hThread;
	#else
		pthread_t hThread;
		bool bThreadStarted;
	#endif
} __test_xnet_proxy_server;

typedef struct {
	volatile long bRun;
	volatile long iRecvCount;
	xsocket hListen;
	uint16 iListenPort;
	char aLastRecv[128];
	#if defined(_WIN32) || defined(_WIN64)
		HANDLE hThread;
	#else
		pthread_t hThread;
		bool bThreadStarted;
	#endif
} __test_xnet_proxy_echo_server;


static bool __Test_XNetProxySocketReadable(xsocket hSocket, uint32 iTimeoutMs)
{
	fd_set tReadSet;
	struct timeval tTv;
	int iRet;
	if ( !__xnetSocketIsValid(hSocket) ) return false;
	FD_ZERO(&tReadSet);
	FD_SET(hSocket, &tReadSet);
	tTv.tv_sec = (long)(iTimeoutMs / 1000u);
	tTv.tv_usec = (long)((iTimeoutMs % 1000u) * 1000u);
	iRet = select((int)(hSocket + 1), &tReadSet, NULL, NULL, &tTv);
	return iRet > 0 && FD_ISSET(hSocket, &tReadSet);
}

static bool __Test_XNetProxySendAll(xsocket hSocket, const void* pData, size_t iLen)
{
	const char* pBytes = (const char*)pData;
	size_t iSent = 0;
	while ( iSent < iLen ) {
		int iRet = send(hSocket, pBytes + iSent, (int)(iLen - iSent), 0);
		if ( iRet <= 0 ) return false;
		iSent += (size_t)iRet;
	}
	return true;
}

static bool __Test_XNetProxyRecvExact(xsocket hSocket, void* pBuf, size_t iLen, uint32 iTimeoutMs)
{
	char* pOut = (char*)pBuf;
	size_t iRead = 0;
	uint32 iLoops = (iTimeoutMs / 50u) + 1u;
	while ( iRead < iLen && iLoops-- > 0u ) {
		int iRet;
		if ( !__Test_XNetProxySocketReadable(hSocket, 50u) ) continue;
		iRet = recv(hSocket, pOut + iRead, (int)(iLen - iRead), 0);
		if ( iRet <= 0 ) return false;
		iRead += (size_t)iRet;
	}
	return iRead == iLen;
}

static bool __Test_XNetProxyRecvHttpHeader(xsocket hSocket, char* sBuf, size_t iBufCap, uint32 iTimeoutMs)
{
	size_t iLen = 0;
	uint32 iLoops = (iTimeoutMs / 50u) + 1u;
	if ( !sBuf || iBufCap < 5u ) return false;
	sBuf[0] = '\0';
	while ( iLoops-- > 0u && (iLen + 1u) < iBufCap ) {
		int iRet;
		if ( strstr(sBuf, "\r\n\r\n") != NULL ) return true;
		if ( !__Test_XNetProxySocketReadable(hSocket, 50u) ) continue;
		iRet = recv(hSocket, sBuf + iLen, (int)(iBufCap - iLen - 1u), 0);
		if ( iRet <= 0 ) return false;
		iLen += (size_t)iRet;
		sBuf[iLen] = '\0';
	}
	return strstr(sBuf, "\r\n\r\n") != NULL;
}

static bool __Test_XNetProxyCreateLoopbackListener(xsocket* phListen, uint16* piPort)
{
	xnetaddr tAddr;
	struct sockaddr_storage tStorage;
	struct sockaddr_storage tBound;
	socklen_t iAddrLen = 0;
	socklen_t iBoundLen = sizeof(tBound);
	xsocket hListen;
	int iOpt = 1;
	memset(&tAddr, 0, sizeof(tAddr));
	memset(&tStorage, 0, sizeof(tStorage));
	memset(&tBound, 0, sizeof(tBound));
	if ( phListen ) *phListen = XNET_SOCKET_INVALID;
	if ( piPort ) *piPort = 0;
	if ( xrtNetAddrParse(&tAddr, "127.0.0.1", 0) != XRT_NET_OK ) return false;
	if ( !__xnetAddrToSockAddr(&tAddr, &tStorage, &iAddrLen) ) return false;
	hListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if ( !__xnetSocketIsValid(hListen) ) return false;
	#if defined(_WIN32) || defined(_WIN64)
		setsockopt(hListen, SOL_SOCKET, SO_REUSEADDR, (const char*)&iOpt, sizeof(iOpt));
	#else
		setsockopt(hListen, SOL_SOCKET, SO_REUSEADDR, &iOpt, sizeof(iOpt));
	#endif
	if ( bind(hListen, (struct sockaddr*)&tStorage, iAddrLen) != 0 || listen(hListen, 8) != 0 ) {
		__xnetSocketCloseHandle(&hListen);
		return false;
	}
	if ( getsockname(hListen, (struct sockaddr*)&tBound, &iBoundLen) != 0 ) {
		__xnetSocketCloseHandle(&hListen);
		return false;
	}
	if ( phListen ) *phListen = hListen;
	if ( piPort ) *piPort = ntohs(((struct sockaddr_in*)&tBound)->sin_port);
	return true;
}

static xsocket __Test_XNetProxyConnectTarget(const char* sHost, uint16 iPort)
{
	xnetaddr tAddr;
	struct sockaddr_storage tStorage;
	socklen_t iAddrLen = 0;
	xsocket hSocket;
	memset(&tAddr, 0, sizeof(tAddr));
	memset(&tStorage, 0, sizeof(tStorage));
	if ( xrtNetAddrParse(&tAddr, sHost, iPort) != XRT_NET_OK ) return XNET_SOCKET_INVALID;
	if ( !__xnetAddrToSockAddr(&tAddr, &tStorage, &iAddrLen) ) return XNET_SOCKET_INVALID;
	hSocket = socket(tAddr.iFamily, SOCK_STREAM, IPPROTO_TCP);
	if ( !__xnetSocketIsValid(hSocket) ) return XNET_SOCKET_INVALID;
	if ( connect(hSocket, (struct sockaddr*)&tStorage, iAddrLen) != 0 ) {
		__xnetSocketCloseHandle(&hSocket);
		return XNET_SOCKET_INVALID;
	}
	return hSocket;
}

static void __Test_XNetProxyInitStreamEvents(xnetstreamevents* pEvents)
{
	memset(pEvents, 0, sizeof(xnetstreamevents));
	pEvents->OnOpen = __Test_XNet2_StreamOnOpen;
	pEvents->OnRecv = __Test_XNet2_StreamOnRecv;
	pEvents->OnClose = __Test_XNet2_StreamOnClose;
	pEvents->OnError = __Test_XNet2_StreamOnError;
}

static bool __Test_XNetProxySplitAuthority(const char* sAuthority, char* sHost, size_t iHostCap, uint16* piPort)
{
	const char* pColon = strrchr(sAuthority, ':');
	size_t iHostLen;
	long iPort;
	if ( !sAuthority || !pColon || !sHost || !piPort ) return false;
	iHostLen = (size_t)(pColon - sAuthority);
	if ( iHostLen == 0 || iHostLen >= iHostCap ) return false;
	memcpy(sHost, sAuthority, iHostLen);
	sHost[iHostLen] = '\0';
	iPort = strtol(pColon + 1, NULL, 10);
	if ( iPort <= 0 || iPort > 65535 ) return false;
	*piPort = (uint16)iPort;
	return true;
}

static bool __Test_XNetProxyHttpAuthOk(const char* sHeader, const char* sUser, const char* sPass)
{
	char aCred[256];
	char* pHeader;
	str sAuth;
	size_t iAuthLen;
	bool bOk;
	if ( (!sUser || !sUser[0]) && (!sPass || !sPass[0]) ) return true;
	snprintf(aCred, sizeof(aCred), "%s:%s", sUser ? sUser : "", sPass ? sPass : "");
	sAuth = xrtBase64Encode(aCred, strlen(aCred), NULL);
	if ( !sAuth || sAuth == xCore.sNull || !sAuth[0] ) {
		if ( sAuth && sAuth != xCore.sNull ) xrtFree(sAuth);
		return false;
	}
	pHeader = strstr((char*)sHeader, "Proxy-Authorization: Basic ");
	if ( !pHeader ) {
		xrtFree(sAuth);
		return false;
	}
	pHeader += strlen("Proxy-Authorization: Basic ");
	iAuthLen = strlen(sAuth);
	bOk = strncmp(pHeader, sAuth, iAuthLen) == 0 && pHeader[iAuthLen] == '\r';
	xrtFree(sAuth);
	return bOk;
}

static bool __Test_XNetProxyTunnel(__test_xnet_proxy_server* pServer, xsocket hClient, xsocket hUpstream)
{
	char aBuf[4096];
	while ( __Test_XNet2_StreamAtomicLoad(&pServer->bRun) != 0 ) {
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
			if ( !__Test_XNetProxySendAll(hUpstream, aBuf, (size_t)iRet) ) break;
		}
		if ( FD_ISSET(hUpstream, &tReadSet) ) {
			iRet = recv(hUpstream, aBuf, sizeof(aBuf), 0);
			if ( iRet <= 0 ) break;
			if ( !__Test_XNetProxySendAll(hClient, aBuf, (size_t)iRet) ) break;
		}
	}
	__xnetSocketCloseHandle(&hUpstream);
	__xnetSocketCloseHandle(&hClient);
	return true;
}

static bool __Test_XNetProxyHandleHttp(__test_xnet_proxy_server* pServer, xsocket hClient)
{
	char aHeader[2048];
	char aAuthority[384];
	char aHost[256];
	uint16 iPort = 0;
	char* pLineEnd;
	xsocket hUpstream;
	if ( !__Test_XNetProxyRecvHttpHeader(hClient, aHeader, sizeof(aHeader), 3000u) ) return false;
	pLineEnd = strstr(aHeader, "\r\n");
	if ( !pLineEnd ) return false;
	*pLineEnd = '\0';
	if ( sscanf(aHeader, "CONNECT %383s HTTP/1.1", aAuthority) != 1 ) return false;
	*pLineEnd = '\r';
	if ( !__Test_XNetProxySplitAuthority(aAuthority, aHost, sizeof(aHost), &iPort) ) return false;
	snprintf(pServer->aLastConnectHost, sizeof(pServer->aLastConnectHost), "%s", aHost);
	__Test_XNet2_StreamAtomicStore(&pServer->iLastConnectPort, (long)iPort);
	__Test_XNet2_StreamAtomicInc(&pServer->iConnectCount);
	if ( pServer->bRequireAuth ) {
		if ( !__Test_XNetProxyHttpAuthOk(aHeader, pServer->sUser, pServer->sPass) ) {
			(void)__Test_XNetProxySendAll(hClient, "HTTP/1.1 407 Proxy Authentication Required\r\nContent-Length: 0\r\n\r\n", 68u);
			return false;
		}
		__Test_XNet2_StreamAtomicInc(&pServer->iAuthCount);
	}
	hUpstream = __Test_XNetProxyConnectTarget(aHost, iPort);
	if ( !__xnetSocketIsValid(hUpstream) ) return false;
	if ( !__Test_XNetProxySendAll(hClient, "HTTP/1.1 200 Connection Established\r\n\r\n", 39u) ) {
		__xnetSocketCloseHandle(&hUpstream);
		return false;
	}
	__Test_XNet2_StreamAtomicInc(&pServer->iTunnelCount);
	return __Test_XNetProxyTunnel(pServer, hClient, hUpstream);
}

static bool __Test_XNetProxyHandleSocks5(__test_xnet_proxy_server* pServer, xsocket hClient)
{
	uint8 aHead[4];
	uint8 aMethods[16];
	uint8 aReply[10];
	char aHost[256];
	uint16 iPort;
	xsocket hUpstream;
	memset(aHead, 0, sizeof(aHead));
	memset(aMethods, 0, sizeof(aMethods));
	memset(aReply, 0, sizeof(aReply));
	memset(aHost, 0, sizeof(aHost));
	if ( !__Test_XNetProxyRecvExact(hClient, aHead, 2u, 3000u) ) return false;
	if ( aHead[0] != 0x05u || aHead[1] == 0u || aHead[1] > sizeof(aMethods) ) return false;
	if ( !__Test_XNetProxyRecvExact(hClient, aMethods, aHead[1], 3000u) ) return false;
	aReply[0] = 0x05u;
	aReply[1] = pServer->bRequireAuth ? 0x02u : 0x00u;
	if ( !__Test_XNetProxySendAll(hClient, aReply, 2u) ) return false;
	if ( pServer->bRequireAuth ) {
		uint8 iUserLen;
		uint8 iPassLen;
		char aUser[128];
		char aPass[128];
		memset(aUser, 0, sizeof(aUser));
		memset(aPass, 0, sizeof(aPass));
		if ( !__Test_XNetProxyRecvExact(hClient, aHead, 2u, 3000u) ) return false;
		iUserLen = aHead[1];
		if ( iUserLen >= sizeof(aUser) ) return false;
		if ( !__Test_XNetProxyRecvExact(hClient, aUser, iUserLen, 3000u) ) return false;
		if ( !__Test_XNetProxyRecvExact(hClient, &iPassLen, 1u, 3000u) ) return false;
		if ( iPassLen >= sizeof(aPass) ) return false;
		if ( !__Test_XNetProxyRecvExact(hClient, aPass, iPassLen, 3000u) ) return false;
		aReply[0] = 0x01u;
		aReply[1] = (strcmp(aUser, pServer->sUser) == 0 && strcmp(aPass, pServer->sPass) == 0) ? 0x00u : 0x01u;
		if ( !__Test_XNetProxySendAll(hClient, aReply, 2u) || aReply[1] != 0x00u ) return false;
		__Test_XNet2_StreamAtomicInc(&pServer->iAuthCount);
	}
	if ( !__Test_XNetProxyRecvExact(hClient, aHead, 4u, 3000u) ) return false;
	if ( aHead[0] != 0x05u || aHead[1] != 0x01u ) return false;
	if ( aHead[3] != 0x01u ) return false;
	if ( !__Test_XNetProxyRecvExact(hClient, aHead, 4u, 3000u) ) return false;
	snprintf(aHost, sizeof(aHost), "%u.%u.%u.%u", (unsigned)aHead[0], (unsigned)aHead[1], (unsigned)aHead[2], (unsigned)aHead[3]);
	if ( !__Test_XNetProxyRecvExact(hClient, aHead, 2u, 3000u) ) return false;
	iPort = (uint16)(((uint16)aHead[0] << 8) | (uint16)aHead[1]);
	snprintf(pServer->aLastConnectHost, sizeof(pServer->aLastConnectHost), "%s", aHost);
	__Test_XNet2_StreamAtomicStore(&pServer->iLastConnectPort, (long)iPort);
	__Test_XNet2_StreamAtomicInc(&pServer->iConnectCount);
	hUpstream = __Test_XNetProxyConnectTarget(aHost, iPort);
	if ( !__xnetSocketIsValid(hUpstream) ) return false;
	memset(aReply, 0, sizeof(aReply));
	aReply[0] = 0x05u;
	aReply[1] = 0x00u;
	aReply[3] = 0x01u;
	if ( !__Test_XNetProxySendAll(hClient, aReply, sizeof(aReply)) ) {
		__xnetSocketCloseHandle(&hUpstream);
		return false;
	}
	__Test_XNet2_StreamAtomicInc(&pServer->iTunnelCount);
	return __Test_XNetProxyTunnel(pServer, hClient, hUpstream);
}

#if defined(_WIN32) || defined(_WIN64)
static DWORD WINAPI __Test_XNetProxyServerThread(LPVOID pArg)
#else
static void* __Test_XNetProxyServerThread(void* pArg)
#endif
{
	__test_xnet_proxy_server* pServer = (__test_xnet_proxy_server*)pArg;
	while ( pServer && __Test_XNet2_StreamAtomicLoad(&pServer->bRun) != 0 ) {
		struct sockaddr_storage tStorage;
		socklen_t iAddrLen = sizeof(tStorage);
		xsocket hClient;
		if ( !__Test_XNetProxySocketReadable(pServer->hListen, 100u) ) continue;
		hClient = accept(pServer->hListen, (struct sockaddr*)&tStorage, &iAddrLen);
		if ( !__xnetSocketIsValid(hClient) ) continue;
		__Test_XNet2_StreamAtomicInc(&pServer->iAcceptCount);
		if ( pServer->iType == XNET_PROXY_HTTP_CONNECT ) (void)__Test_XNetProxyHandleHttp(pServer, hClient);
		else (void)__Test_XNetProxyHandleSocks5(pServer, hClient);
		if ( __xnetSocketIsValid(hClient) ) __xnetSocketCloseHandle(&hClient);
	}
	#if !defined(_WIN32) && !defined(_WIN64)
		return NULL;
	#endif
}

static bool __Test_XNetProxyServerStart(__test_xnet_proxy_server* pServer, int iType, bool bRequireAuth, const char* sUser, const char* sPass)
{
	memset(pServer, 0, sizeof(__test_xnet_proxy_server));
	pServer->iType = iType;
	pServer->bRequireAuth = bRequireAuth;
	snprintf(pServer->sUser, sizeof(pServer->sUser), "%s", sUser ? sUser : "");
	snprintf(pServer->sPass, sizeof(pServer->sPass), "%s", sPass ? sPass : "");
	if ( !__Test_XNetProxyCreateLoopbackListener(&pServer->hListen, &pServer->iListenPort) ) return false;
	__Test_XNet2_StreamAtomicStore(&pServer->bRun, 1);
	#if defined(_WIN32) || defined(_WIN64)
		pServer->hThread = CreateThread(NULL, 0, __Test_XNetProxyServerThread, pServer, 0, NULL);
		return pServer->hThread != NULL;
	#else
		if ( pthread_create(&pServer->hThread, NULL, __Test_XNetProxyServerThread, pServer) != 0 ) return false;
		pServer->bThreadStarted = true;
		return true;
	#endif
}

static void __Test_XNetProxyServerStop(__test_xnet_proxy_server* pServer)
{
	__Test_XNet2_StreamAtomicStore(&pServer->bRun, 0);
	#if defined(_WIN32) || defined(_WIN64)
		if ( pServer->hThread ) {
			WaitForSingleObject(pServer->hThread, INFINITE);
			CloseHandle(pServer->hThread);
		}
	#else
		if ( pServer->bThreadStarted ) pthread_join(pServer->hThread, NULL);
	#endif
	if ( __xnetSocketIsValid(pServer->hListen) ) __xnetSocketCloseHandle(&pServer->hListen);
}

#if defined(_WIN32) || defined(_WIN64)
static DWORD WINAPI __Test_XNetProxyEchoThread(LPVOID pArg)
#else
static void* __Test_XNetProxyEchoThread(void* pArg)
#endif
{
	__test_xnet_proxy_echo_server* pServer = (__test_xnet_proxy_echo_server*)pArg;
	char aBuf[512];
	while ( pServer && __Test_XNet2_StreamAtomicLoad(&pServer->bRun) != 0 ) {
		struct sockaddr_storage tStorage;
		socklen_t iAddrLen = sizeof(tStorage);
		xsocket hClient;
		if ( !__Test_XNetProxySocketReadable(pServer->hListen, 100u) ) continue;
		hClient = accept(pServer->hListen, (struct sockaddr*)&tStorage, &iAddrLen);
		if ( !__xnetSocketIsValid(hClient) ) continue;
		while ( __Test_XNet2_StreamAtomicLoad(&pServer->bRun) != 0 ) {
			int iRecv;
			size_t iCopy;
			if ( !__Test_XNetProxySocketReadable(hClient, 100u) ) continue;
			iRecv = recv(hClient, aBuf, sizeof(aBuf), 0);
			if ( iRecv <= 0 ) break;
			__Test_XNet2_StreamAtomicInc(&pServer->iRecvCount);
			iCopy = (size_t)iRecv < (sizeof(pServer->aLastRecv) - 1u) ? (size_t)iRecv : (sizeof(pServer->aLastRecv) - 1u);
			memcpy(pServer->aLastRecv, aBuf, iCopy);
			pServer->aLastRecv[iCopy] = '\0';
			if ( !__Test_XNetProxySendAll(hClient, aBuf, (size_t)iRecv) ) break;
		}
		__xnetSocketCloseHandle(&hClient);
	}
	#if !defined(_WIN32) && !defined(_WIN64)
		return NULL;
	#endif
}

static bool __Test_XNetProxyEchoStart(__test_xnet_proxy_echo_server* pServer)
{
	memset(pServer, 0, sizeof(__test_xnet_proxy_echo_server));
	if ( !__Test_XNetProxyCreateLoopbackListener(&pServer->hListen, &pServer->iListenPort) ) return false;
	__Test_XNet2_StreamAtomicStore(&pServer->bRun, 1);
	#if defined(_WIN32) || defined(_WIN64)
		pServer->hThread = CreateThread(NULL, 0, __Test_XNetProxyEchoThread, pServer, 0, NULL);
		return pServer->hThread != NULL;
	#else
		if ( pthread_create(&pServer->hThread, NULL, __Test_XNetProxyEchoThread, pServer) != 0 ) return false;
		pServer->bThreadStarted = true;
		return true;
	#endif
}

static void __Test_XNetProxyEchoStop(__test_xnet_proxy_echo_server* pServer)
{
	__Test_XNet2_StreamAtomicStore(&pServer->bRun, 0);
	#if defined(_WIN32) || defined(_WIN64)
		if ( pServer->hThread ) {
			WaitForSingleObject(pServer->hThread, INFINITE);
			CloseHandle(pServer->hThread);
		}
	#else
		if ( pServer->bThreadStarted ) pthread_join(pServer->hThread, NULL);
	#endif
	if ( __xnetSocketIsValid(pServer->hListen) ) __xnetSocketCloseHandle(&pServer->hListen);
}

static void __Test_XNetProxyRelease(xnetproxy** ppProxy)
{
	if ( ppProxy && *ppProxy ) {
		xrtNetProxyRelease(*ppProxy);
		*ppProxy = NULL;
	}
}


void Test_XNet_Proxy(void)
{
	printf("\n\n\n------------------------------------\n\n XNet Proxy Integration Test:\n\n");

	{
		__test_xnet_proxy_echo_server tEchoServer;
		__test_xnet_proxy_server tProxyServer;
		xnetengineconfig tEngineCfg;
		xnetstreamevents tEvents;
		xnetconnectconfig tConnCfg;
		xnetproxyconfig tProxyCfg;
		__test_xnet2_stream_stats tStats;
		xnetengine* pEngine = NULL;
		xnetstream* pStream = NULL;
		xnetproxy* pProxy = NULL;

		memset(&tStats, 0, sizeof(tStats));
		__Test_XNetProxyInitStreamEvents(&tEvents);
		printf("  HTTP CONNECT echo server start : %s\n", __Test_XNetProxyEchoStart(&tEchoServer) ? "PASS" : "FAIL");
		printf("  HTTP CONNECT proxy server start : %s\n", __Test_XNetProxyServerStart(&tProxyServer, XNET_PROXY_HTTP_CONNECT, false, NULL, NULL) ? "PASS" : "FAIL");
		xrtNetEngineConfigInit(&tEngineCfg);
		tEngineCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tEngineCfg);
		if ( pEngine ) (void)xrtNetEngineStart(pEngine);
		pStream = pEngine ? xrtNetStreamCreate(pEngine, &tEvents, &tStats) : NULL;
		xrtNetProxyConfigInit(&tProxyCfg);
		tProxyCfg.iType = XNET_PROXY_HTTP_CONNECT;
		snprintf(tProxyCfg.sHost, sizeof(tProxyCfg.sHost), "127.0.0.1");
		tProxyCfg.iPort = tProxyServer.iListenPort;
		pProxy = xrtNetProxyCreate(&tProxyCfg);
		xrtNetConnectConfigInit(&tConnCfg);
		tConnCfg.sHost = "127.0.0.1";
		tConnCfg.iPort = tEchoServer.iListenPort;
		tConnCfg.pProxy = pProxy;
		printf("  HTTP CONNECT stream connect : %s\n", pStream && pProxy && xrtNetStreamConnect(pStream, &tConnCfg) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  HTTP CONNECT pre-open send gated : %s\n", pStream && xrtNetStreamSend(pStream, "gate", 4u) == XRT_NET_AGAIN ? "PASS" : "FAIL");
		printf("  HTTP CONNECT open callback : %s\n", __Test_XNet2_StreamWaitMin(&tStats.iOpenCount, 1, 2000u) ? "PASS" : "FAIL");
		printf("  HTTP CONNECT remote addr keeps proxy peer : %s\n", pStream && pStream->tRemoteAddr.iPort == tProxyServer.iListenPort ? "PASS" : "FAIL");
		printf("  HTTP CONNECT send : %s\n", pStream && xrtNetStreamSend(pStream, "proxy-http", 10u) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  HTTP CONNECT recv : %s\n", __Test_XNet2_StreamWaitMin(&tStats.iRecvCount, 1, 2000u) ? "PASS" : "FAIL");
		printf("  HTTP CONNECT echoed text : %s\n", strcmp(tStats.aLastRecvText, "proxy-http") == 0 ? "PASS" : "FAIL");
		printf("  HTTP CONNECT tunnel target : %s\n", strcmp(tProxyServer.aLastConnectHost, "127.0.0.1") == 0 && __Test_XNet2_StreamAtomicLoad(&tProxyServer.iLastConnectPort) == (long)tEchoServer.iListenPort ? "PASS" : "FAIL");
		if ( pStream ) xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		(void)__Test_XNet2_StreamWaitMin(&tStats.iCloseCount, 1, 1000u);
		if ( pStream ) xrtNetStreamDestroy(pStream);
		__Test_XNetProxyRelease(&pProxy);
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
		__Test_XNetProxyServerStop(&tProxyServer);
		__Test_XNetProxyEchoStop(&tEchoServer);
	}

	{
		__test_xnet_proxy_echo_server tEchoServer;
		__test_xnet_proxy_server tProxyServer;
		xnetengineconfig tEngineCfg;
		xnetstreamevents tEvents;
		xnetconnectconfig tConnCfg;
		xnetproxyconfig tProxyCfg;
		__test_xnet2_stream_stats tStats;
		xnetengine* pEngine = NULL;
		xnetstream* pStream = NULL;
		xnetproxy* pProxy = NULL;

		memset(&tStats, 0, sizeof(tStats));
		__Test_XNetProxyInitStreamEvents(&tEvents);
		printf("  SOCKS5 auth echo server start : %s\n", __Test_XNetProxyEchoStart(&tEchoServer) ? "PASS" : "FAIL");
		printf("  SOCKS5 auth proxy start : %s\n", __Test_XNetProxyServerStart(&tProxyServer, XNET_PROXY_SOCKS5, true, "user", "pass") ? "PASS" : "FAIL");
		xrtNetEngineConfigInit(&tEngineCfg);
		tEngineCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tEngineCfg);
		if ( pEngine ) (void)xrtNetEngineStart(pEngine);
		pStream = pEngine ? xrtNetStreamCreate(pEngine, &tEvents, &tStats) : NULL;
		xrtNetProxyConfigInit(&tProxyCfg);
		tProxyCfg.iType = XNET_PROXY_SOCKS5;
		snprintf(tProxyCfg.sHost, sizeof(tProxyCfg.sHost), "127.0.0.1");
		tProxyCfg.iPort = tProxyServer.iListenPort;
		snprintf(tProxyCfg.sUser, sizeof(tProxyCfg.sUser), "user");
		snprintf(tProxyCfg.sPass, sizeof(tProxyCfg.sPass), "pass");
		pProxy = xrtNetProxyCreate(&tProxyCfg);
		xrtNetConnectConfigInit(&tConnCfg);
		tConnCfg.sHost = "127.0.0.1";
		tConnCfg.iPort = tEchoServer.iListenPort;
		tConnCfg.pProxy = pProxy;
		printf("  SOCKS5 auth stream connect : %s\n", pStream && pProxy && xrtNetStreamConnect(pStream, &tConnCfg) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  SOCKS5 auth open callback : %s\n", __Test_XNet2_StreamWaitMin(&tStats.iOpenCount, 1, 2000u) ? "PASS" : "FAIL");
		printf("  SOCKS5 auth counted : %s\n", __Test_XNet2_StreamAtomicLoad(&tProxyServer.iAuthCount) == 1 ? "PASS" : "FAIL");
		printf("  SOCKS5 auth send : %s\n", pStream && xrtNetStreamSend(pStream, "proxy-socks", 11u) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  SOCKS5 auth recv : %s\n", __Test_XNet2_StreamWaitMin(&tStats.iRecvCount, 1, 2000u) ? "PASS" : "FAIL");
		printf("  SOCKS5 auth echoed text : %s\n", strcmp(tStats.aLastRecvText, "proxy-socks") == 0 ? "PASS" : "FAIL");
		if ( pStream ) xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		(void)__Test_XNet2_StreamWaitMin(&tStats.iCloseCount, 1, 1000u);
		if ( pStream ) xrtNetStreamDestroy(pStream);
		__Test_XNetProxyRelease(&pProxy);
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
		__Test_XNetProxyServerStop(&tProxyServer);
		__Test_XNetProxyEchoStop(&tEchoServer);
	}

	{
		__test_xhttp_server tServer;
		__test_xnet_proxy_server tProxyServer;
		xnetengineconfig tEngineCfg;
		xnetengine* pClientEngine = NULL;
		xhttprequest tReq1;
		xhttprequest tReq2;
		xnetfuture* pFuture1 = NULL;
		xnetfuture* pFuture2 = NULL;
		xhttpresponse* pResp1 = NULL;
		xhttpresponse* pResp2 = NULL;
		xnetproxyconfig tProxyCfg;
		xnetproxy* pProxy = NULL;
		xnet_result iStatus1 = XRT_NET_ERROR;
		xnet_result iStatus2 = XRT_NET_ERROR;
		char sURL1[256];
		char sURL2[256];

		printf("  XHTTP proxy origin server start : %s\n", __Test_XHttpServerStart(&tServer, "HTTP/1.1 200 OK\r\nContent-Length: 5\r\nConnection: keep-alive\r\n\r\nproxy", NULL) ? "PASS" : "FAIL");
		printf("  XHTTP proxy server start : %s\n", __Test_XNetProxyServerStart(&tProxyServer, XNET_PROXY_HTTP_CONNECT, false, NULL, NULL) ? "PASS" : "FAIL");
		snprintf(sURL1, sizeof(sURL1), "http://127.0.0.1:%u/proxy-a", (unsigned)tServer.pListener->tConfig.tBindAddr.iPort);
		snprintf(sURL2, sizeof(sURL2), "http://127.0.0.1:%u/proxy-b", (unsigned)tServer.pListener->tConfig.tBindAddr.iPort);
		xrtNetEngineConfigInit(&tEngineCfg);
		tEngineCfg.iWorkerCount = 1;
		pClientEngine = xrtNetEngineCreate(&tEngineCfg);
		if ( pClientEngine ) (void)xrtNetEngineStart(pClientEngine);
		xrtNetProxyConfigInit(&tProxyCfg);
		tProxyCfg.iType = XNET_PROXY_HTTP_CONNECT;
		snprintf(tProxyCfg.sHost, sizeof(tProxyCfg.sHost), "127.0.0.1");
		tProxyCfg.iPort = tProxyServer.iListenPort;
		pProxy = xrtNetProxyCreate(&tProxyCfg);
		xrtHttpRequestInit(&tReq1);
		xrtHttpRequestInit(&tReq2);
		(void)xrtHttpRequestSetURL(&tReq1, sURL1);
		(void)xrtHttpRequestSetURL(&tReq2, sURL2);
		tReq1.pProxy = pProxy ? xrtNetProxyAddRef(pProxy) : NULL;
		tReq2.pProxy = pProxy ? xrtNetProxyAddRef(pProxy) : NULL;
		pFuture1 = xrtHttpExecuteAsync(pClientEngine, &tReq1);
		pFuture2 = NULL;
		printf("  XHTTP proxy future #1 wait : %s\n", pFuture1 && (iStatus1 = xrtNetFutureWait(pFuture1, 4000u)) == XRT_NET_OK ? "PASS" : "FAIL");
		pResp1 = (pFuture1 && iStatus1 == XRT_NET_OK) ? (xhttpresponse*)xrtNetFutureValue(pFuture1) : NULL;
		pFuture2 = xrtHttpExecuteAsync(pClientEngine, &tReq2);
		printf("  XHTTP proxy future #2 wait : %s\n", pFuture2 && (iStatus2 = xrtNetFutureWait(pFuture2, 4000u)) == XRT_NET_OK ? "PASS" : "FAIL");
		pResp2 = (pFuture2 && iStatus2 == XRT_NET_OK) ? (xhttpresponse*)xrtNetFutureValue(pFuture2) : NULL;
		printf("  XHTTP proxy responses ok : %s\n", pResp1 && pResp2 && memcmp(pResp1->pBody, "proxy", 5) == 0 && memcmp(pResp2->pBody, "proxy", 5) == 0 ? "PASS" : "FAIL");
		printf("  XHTTP proxy tunnel reused : %s\n", __Test_XNet2_StreamAtomicLoad(&tProxyServer.iAcceptCount) == 1 && __Test_XNet2_StreamAtomicLoad(&tProxyServer.iConnectCount) == 1 ? "PASS" : "FAIL");
		if ( pResp1 ) xrtHttpResponseDestroy(pResp1);
		if ( pResp2 ) xrtHttpResponseDestroy(pResp2);
		if ( pFuture1 ) xrtNetFutureDestroy(pFuture1);
		if ( pFuture2 ) xrtNetFutureDestroy(pFuture2);
		xrtHttpRequestUnit(&tReq1);
		xrtHttpRequestUnit(&tReq2);
		xrtHttpCloseIdleConnections(pClientEngine);
		__Test_XNetProxyRelease(&pProxy);
		if ( pClientEngine ) {
			xrtNetEngineStop(pClientEngine);
			xrtNetEngineDestroy(pClientEngine);
		}
		__Test_XHttpServerStop(&tServer);
		__Test_XNetProxyServerStop(&tProxyServer);
	}

	{
		__test_xws_server_ctx tSrvCtx;
		__test_xws_client_ctx tCliCtx;
		__test_xnet_proxy_server tProxyServer;
		xwsserverevents tSrvEvents;
		xwsclientevents tCliEvents;
		xwsserverconfig tSrvCfg;
		xwsclientconfig tCliCfg;
		xnetengineconfig tEngineCfg;
		xnetengine* pServerEngine = NULL;
		xnetengine* pClientEngine = NULL;
		xwsserver* pServer = NULL;
		xwsclient* pClient = NULL;
		xnetproxyconfig tProxyCfg;
		xnetproxy* pProxy = NULL;
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
		if ( pServerEngine ) (void)xrtNetEngineStart(pServerEngine);
		if ( pClientEngine ) (void)xrtNetEngineStart(pClientEngine);
		xrtWsServerConfigInit(&tSrvCfg);
		(void)xrtNetAddrParse(&tSrvCfg.tBindAddr, "127.0.0.1", 0);
		pServer = (pServerEngine && pClientEngine) ? xrtWsServerCreate(pServerEngine, &tSrvCfg, &tSrvEvents, &tSrvCtx) : NULL;
		printf("  XWS proxy server start : %s\n", pServer && xrtWsServerStart(pServer) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  XWS proxy tunnel server start : %s\n", __Test_XNetProxyServerStart(&tProxyServer, XNET_PROXY_HTTP_CONNECT, false, NULL, NULL) ? "PASS" : "FAIL");
		xrtNetProxyConfigInit(&tProxyCfg);
		tProxyCfg.iType = XNET_PROXY_HTTP_CONNECT;
		snprintf(tProxyCfg.sHost, sizeof(tProxyCfg.sHost), "127.0.0.1");
		tProxyCfg.iPort = tProxyServer.iListenPort;
		pProxy = xrtNetProxyCreate(&tProxyCfg);
		xrtWsClientConfigInit(&tCliCfg);
		snprintf(sURL, sizeof(sURL), "ws://127.0.0.1:%u/proxy-ws", (unsigned)xrtWsServerBoundPort(pServer));
		snprintf(tCliCfg.sURL, sizeof(tCliCfg.sURL), "%s", sURL);
		tCliCfg.pProxy = pProxy;
		pClient = pClientEngine ? xrtWsClientCreate(pClientEngine, &tCliCfg, &tCliEvents, &tCliCtx) : NULL;
		printf("  XWS proxy client start : %s\n", pClient && xrtWsClientStart(pClient) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  XWS proxy open callbacks : %s\n", __Test_XWsWaitMin(&tSrvCtx.iOpenCount, 1, 3000u) && __Test_XWsWaitMin(&tCliCtx.iOpenCount, 1, 3000u) ? "PASS" : "FAIL");
		printf("  XWS proxy send text : %s\n", pClient && xrtWsClientSendText(pClient, "proxy-ws", 8u) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  XWS proxy echoed text : %s\n", __Test_XWsWaitMin(&tCliCtx.iTextCount, 1, 3000u) && strcmp(tCliCtx.aLastText, "proxy-ws") == 0 ? "PASS" : "FAIL");
		if ( pClient ) {
			(void)xrtWsClientClose(pClient, XWS_CLOSE_NORMAL, "bye");
			(void)__Test_XWsWaitMin(&tCliCtx.iCloseCount, 1, 3000u);
			xrtWsClientDestroy(pClient);
		}
		__Test_XNetProxyRelease(&pProxy);
		if ( pServer ) xrtWsServerDestroy(pServer);
		if ( pClientEngine ) {
			xrtNetEngineStop(pClientEngine);
			xrtNetEngineDestroy(pClientEngine);
		}
		if ( pServerEngine ) {
			xrtNetEngineStop(pServerEngine);
			xrtNetEngineDestroy(pServerEngine);
		}
		__Test_XNetProxyServerStop(&tProxyServer);
	}

	{
		xtlsconfig tTlsCfg;
		__test_xhttp_server tServer;
		__test_xnet_proxy_server tProxyServer;
		xhttprequest tReq;
		xhttpresponse* pResp = NULL;
		xnet_result iStatus = XRT_NET_ERROR;
		xnetproxyconfig tProxyCfg;
		xnetproxy* pProxy = NULL;
		char sURL[256];

		memset(&tTlsCfg, 0, sizeof(tTlsCfg));
		tTlsCfg.sCertFile = "dev/xnet2_tls_test_cert.pem";
		tTlsCfg.sKeyFile = "dev/xnet2_tls_test_key.pem";
		if ( __Test_XHttpFileExists(tTlsCfg.sCertFile) && __Test_XHttpFileExists(tTlsCfg.sKeyFile) ) {
			printf("  XHTTP HTTPS via proxy origin start : %s\n", __Test_XHttpServerStart(&tServer, "HTTP/1.1 200 OK\r\nContent-Length: 6\r\nConnection: close\r\n\r\nsecure", &tTlsCfg) ? "PASS" : "FAIL");
			printf("  XHTTP HTTPS via proxy server start : %s\n", __Test_XNetProxyServerStart(&tProxyServer, XNET_PROXY_HTTP_CONNECT, false, NULL, NULL) ? "PASS" : "FAIL");
			xrtNetProxyConfigInit(&tProxyCfg);
			tProxyCfg.iType = XNET_PROXY_HTTP_CONNECT;
			snprintf(tProxyCfg.sHost, sizeof(tProxyCfg.sHost), "127.0.0.1");
			tProxyCfg.iPort = tProxyServer.iListenPort;
			pProxy = xrtNetProxyCreate(&tProxyCfg);
			xrtHttpRequestInit(&tReq);
			snprintf(sURL, sizeof(sURL), "https://127.0.0.1:%u/proxy-secure", (unsigned)tServer.pListener->tConfig.tBindAddr.iPort);
			(void)xrtHttpRequestSetURL(&tReq, sURL);
			xrtHttpRequestSetVerifyPeer(&tReq, false);
			tReq.pProxy = pProxy ? xrtNetProxyAddRef(pProxy) : NULL;
			pResp = xrtHttpExecuteSync(NULL, &tReq, &iStatus);
			printf("  XHTTP HTTPS via proxy response : %s\n", iStatus == XRT_NET_OK && pResp && memcmp(pResp->pBody, "secure", 6) == 0 ? "PASS" : "FAIL");
			if ( pResp ) xrtHttpResponseDestroy(pResp);
			xrtHttpRequestUnit(&tReq);
			xrtHttpCloseIdleConnections(xrtNetSyncGetHiddenEngine());
			xrtNetSyncShutdownHiddenEngine();
			__Test_XNetProxyRelease(&pProxy);
			__Test_XHttpServerStop(&tServer);
			__Test_XNetProxyServerStop(&tProxyServer);
		} else {
			printf("  XHTTP HTTPS via proxy fixture files missing : SKIP\n");
		}
	}
}
