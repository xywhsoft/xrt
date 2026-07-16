#ifndef XRT_TEST_XNET2_CONVENIENCE_H
#define XRT_TEST_XNET2_CONVENIENCE_H

typedef struct {
	xnetlistener* pListener;
	xnetstream* pAccepted;
	uint8 aRecv[16];
	size_t iRecvLen;
	bool bAcceptOk;
	bool bGreetingSendOk;
	bool bGreetingDrainOk;
	bool bRecvOk;
	bool bSendOk;
	bool bDrainOk;
} __test_xnet2_conv_tcp_server;


static uint32 __Test_XNet2_ConvenienceTcpServerProc(ptr pArg)
{
	__test_xnet2_conv_tcp_server* pCase = (__test_xnet2_conv_tcp_server*)pArg;
	xnetstream* pStream = NULL;
	void* pBytes = NULL;
	size_t iLen = 0u;
	static const uint8 aReply[] = { 'o', 'k', 0u, '6' };
	static const uint8 aGreeting[] = { '2', '2', '0', ' ' };

	if ( !pCase || !pCase->pListener ) { return 0; }
	pStream = xrtNetTcpAccept(pCase->pListener, 2000);
	pCase->bAcceptOk = pStream != NULL;
	pCase->pAccepted = pStream;
	if ( pStream ) {
		pCase->bGreetingSendOk = xrtNetStreamSendBytes(pStream, aGreeting, sizeof(aGreeting));
		pCase->bGreetingDrainOk = xrtNetStreamDrain(pStream, 2000);
		pBytes = xrtNetStreamRecvBytes(pStream, sizeof(pCase->aRecv), 2000, &iLen);
		if ( pBytes && iLen <= sizeof(pCase->aRecv) ) {
			memcpy(pCase->aRecv, pBytes, iLen);
			pCase->iRecvLen = iLen;
			pCase->bRecvOk = true;
		}
		xrtFree(pBytes);
		pCase->bSendOk = xrtNetStreamSendBytes(pStream, aReply, sizeof(aReply));
		pCase->bDrainOk = xrtNetStreamDrain(pStream, 2000);
	}
	return 0;
}


typedef struct {
	xnetlistener* pListener;
	xnetstream* pAccepted;
	size_t iExpected;
	size_t iReceived;
	uint64 iChecksum;
} __test_xnet2_conv_close_server;


static uint32 __Test_XNet2_ConvenienceCloseServerProc(ptr pArg)
{
	__test_xnet2_conv_close_server* pCase = (__test_xnet2_conv_close_server*)pArg;

	if ( pCase == NULL || pCase->pListener == NULL ) { return 0; }
	pCase->pAccepted = xrtNetTcpAccept(pCase->pListener, 3000);
	while ( pCase->pAccepted != NULL && pCase->iReceived < pCase->iExpected ) {
		size_t iLen = 0;
		uint8* pBytes = (uint8*)xrtNetStreamRecvBytes(pCase->pAccepted, 8192u, 3000, &iLen);
		size_t i;

		if ( pBytes == NULL || iLen == 0 ) {
			xrtFree(pBytes);
			break;
		}
		for ( i = 0; i < iLen; ++i ) {
			pCase->iChecksum += pBytes[i];
		}
		pCase->iReceived += iLen;
		xrtFree(pBytes);
	}
	if ( pCase->pAccepted != NULL ) {
		xrtNetTcpStreamClose(pCase->pAccepted);
	}
	return 0;
}


static bool __Test_XNet2_ConvenienceCloseOrder(void)
{
	xnetlistener* pListener;
	xnetstream* pClient;
	xthread hThread;
	__test_xnet2_conv_close_server tServer;
	uint8* pPayload;
	uint64 iExpectedChecksum;
	uint16 iPort;
	size_t i;
	bool bSent;
	bool bOk;

	memset(&tServer, 0, sizeof(tServer));
	tServer.iExpected = 65536u;
	pPayload = (uint8*)xrtMalloc(tServer.iExpected);
	if ( pPayload == NULL ) { return false; }
	iExpectedChecksum = 0;
	for ( i = 0; i < tServer.iExpected; ++i ) {
		pPayload[i] = (uint8)(i & 0xffu);
		iExpectedChecksum += pPayload[i];
	}

	pListener = xrtNetTcpListen("127.0.0.1", 0u, 4u);
	iPort = xrtNetTcpListenerPort(pListener);
	tServer.pListener = pListener;
	hThread = pListener != NULL ? xrtThreadCreate(__Test_XNet2_ConvenienceCloseServerProc, &tServer, 0) : NULL;
	pClient = iPort != 0u ? xrtNetTcpConnect("127.0.0.1", iPort, 3000) : NULL;
	bSent = pClient != NULL && xrtNetStreamSendBytes(pClient, pPayload, tServer.iExpected);
	if ( pClient != NULL ) {
		xrtNetTcpStreamClose(pClient);
		xrtNetTcpStreamDestroy(pClient);
	}
	if ( hThread != NULL ) {
		xrtThreadWait(hThread);
		xrtThreadDestroy(hThread);
	}
	bOk = bSent && tServer.iReceived == tServer.iExpected && tServer.iChecksum == iExpectedChecksum;
	if ( tServer.pAccepted != NULL ) { xrtNetTcpStreamDestroy(tServer.pAccepted); }
	if ( pListener != NULL ) { xrtNetTcpListenerDestroy(pListener); }
	xrtFree(pPayload);
	xrtNetSyncShutdownHiddenEngine();
	return bOk;
}


static bool __Test_XNet2_ConvenienceAcceptTimeoutGap(void)
{
	xnetlistener* pListener = NULL;
	xnetstream* pTimedOut = NULL;
	xnetstream* pClient = NULL;
	xnetstream* pAccepted = NULL;
	uint16 iPort = 0u;
	bool bOk = false;

	xrtNetSyncShutdownHiddenEngine();
	pListener = xrtNetTcpListen("127.0.0.1", 0u, 4u);
	iPort = xrtNetTcpListenerPort(pListener);

	/* 先让 AcceptEx 挂起并使 waiter 超时，再让连接落入取消窗口。 */
	pTimedOut = pListener ? xrtNetTcpAccept(pListener, 20) : NULL;
	pClient = iPort != 0u ? xrtNetTcpConnect("127.0.0.1", iPort, 2000) : NULL;
	xrtSleep(50);
	pAccepted = pListener ? xrtNetTcpAccept(pListener, 1000) : NULL;
	bOk = pListener != NULL && pTimedOut == NULL && pClient != NULL && pAccepted != NULL;

	if ( pAccepted ) { xrtNetTcpStreamDestroy(pAccepted); }
	if ( pClient ) { xrtNetTcpStreamDestroy(pClient); }
	if ( pListener ) { xrtNetTcpListenerDestroy(pListener); }
	xrtNetSyncShutdownHiddenEngine();
	return bOk;
}


static bool __Test_XNet2_EngineLiveObjectGuard(void)
{
	xnetengineconfig tCfg;
	xnetengine* pEngine;
	xnetstream* pStream;
	bool bGuarded;
	xrtNetEngineConfigInit(&tCfg);
	tCfg.iWorkerCount = 1u;
	pEngine = xrtNetEngineCreate(&tCfg);
	if ( !pEngine || xrtNetEngineStart(pEngine) != XRT_NET_OK ) {
		if ( pEngine ) { xrtNetEngineDestroy(pEngine); }
		return false;
	}
	pStream = xrtNetStreamCreate(pEngine, NULL, NULL);
	xrtNetEngineDestroy(pEngine);
	bGuarded = pStream && xrtNetEngineGetLiveObjectCount(pEngine) == 1u;
	if ( pStream ) { xrtNetStreamDestroy(pStream); }
	bGuarded = bGuarded && xrtNetEngineGetLiveObjectCount(pEngine) == 0u;
	xrtNetEngineDestroy(pEngine);
	return bGuarded;
}


static bool __Test_XNet2_ConvenienceAsyncFacade(void)
{
	xnetlistener* pListener = NULL;
	xnetstream* pClient = NULL;
	xnetstream* pAccepted = NULL;
	xdgramsock* pUdpServer = NULL;
	xdgramsock* pUdpClient = NULL;
	xnetdgrampkt* pPacket = NULL;
	xfuture* pAccept = NULL;
	xfuture* pConnect = NULL;
	xfuture* pCancelConnect = NULL;
	xfuture* pSend = NULL;
	xfuture* pRecv = NULL;
	xfuture* pDrain = NULL;
	xfuture* pTimeout = NULL;
	xfuture* pUdpRecv = NULL;
	xfuture* pUdpSend = NULL;
	xfuture* pResolve4 = NULL;
	xfuture* pResolve6 = NULL;
	xfuture* pResolveAll = NULL;
	xnetaddr* pResolved4 = NULL;
	xnetaddr* pResolved6 = NULL;
	xnetaddrlist* pResolvedAll = NULL;
	xnetaddrlist* pSyncResolvedAll = NULL;
	xnetengine* pHiddenEngine = NULL;
	xnetbytes* pBytes = NULL;
	uint16 iPort;
	bool bOk = false;
	static const uint8 aPayload[] = { 'a', 0u, 's', 'y', 'n', 'c' };
	static const uint8 aUdpPayload[] = { 'u', 0u, 'a' };

	xrtNetSyncShutdownHiddenEngine();
	pListener = xrtNetTcpListen("127.0.0.1", 0u, 8u);
	pHiddenEngine = xrtNetSyncGetHiddenEngine();
	pResolve4 = pHiddenEngine ? xrtNetResolveFutureEx(pHiddenEngine, 0u,
		"localhost", 80u, AF_INET) : NULL;
	pResolve6 = pHiddenEngine ? xrtNetResolveFutureEx(pHiddenEngine, 0u,
		"localhost", 80u, AF_INET6) : NULL;
	pResolveAll = pHiddenEngine ? xrtNetResolveAllFuture(pHiddenEngine, 0u,
		"localhost", 80u, AF_UNSPEC) : NULL;
	pSyncResolvedAll = xrtNetResolveAll("localhost", 80u, AF_UNSPEC);
	if ( pResolve4 && xFutureWaitTimeout(pResolve4, 2000) ) {
		pResolved4 = (xnetaddr*)xFutureValue(pResolve4);
	}
	if ( pResolve6 && xFutureWaitTimeout(pResolve6, 2000) ) {
		pResolved6 = (xnetaddr*)xFutureValue(pResolve6);
	}
	if ( pResolveAll && xFutureWaitTimeout(pResolveAll, 2000) ) {
		pResolvedAll = (xnetaddrlist*)xFutureValue(pResolveAll);
	}
	iPort = xrtNetTcpListenerPort(pListener);
	pAccept = pListener ? xrtNetTcpAcceptAsync(pListener, 2000) : NULL;
	pConnect = iPort ? xrtNetTcpConnectAsync("localhost", iPort, 2000) : NULL;
	pCancelConnect = xrtNetTcpConnectAsync("192.0.2.1", 65000u, 5000);
	if ( pCancelConnect ) {
		(void)xFutureRequestCancel(pCancelConnect);
		(void)xFutureWaitTimeout(pCancelConnect, 1000);
	}
	if ( pConnect && xFutureWaitTimeout(pConnect, 3000) && xFutureStatus(pConnect) == XRT_NET_OK ) {
		xnetstream** ppClient = (xnetstream**)xFutureValue(pConnect);
		pClient = ppClient ? *ppClient : NULL;
	}
	if ( pAccept && xFutureWaitTimeout(pAccept, 3000) && xFutureStatus(pAccept) == XRT_NET_OK ) {
		xnetstream** ppAccepted = (xnetstream**)xFutureValue(pAccept);
		pAccepted = ppAccepted ? *ppAccepted : NULL;
	}
	if ( pAccepted ) { pRecv = xrtNetStreamRecvBytesAsync(pAccepted, 64u, 2000); }
	if ( pClient ) { pSend = xrtNetStreamSendBytesAsync(pClient, aPayload, sizeof(aPayload)); }
	if ( pSend ) { (void)xFutureWaitTimeout(pSend, 1000); }
	if ( pClient ) { pDrain = xrtNetStreamDrainAsync(pClient, 2000); }
	if ( pDrain ) { (void)xFutureWaitTimeout(pDrain, 3000); }
	if ( pRecv && xFutureWaitTimeout(pRecv, 3000) && xFutureStatus(pRecv) == XRT_NET_OK ) {
		pBytes = (xnetbytes*)xFutureValue(pRecv);
	}
	pTimeout = pListener ? xrtNetTcpAcceptAsync(pListener, 20) : NULL;
	if ( pTimeout ) { (void)xFutureWaitTimeout(pTimeout, 1000); }

	pUdpServer = xrtNetUdpBind("127.0.0.1", 0u);
	pUdpClient = xrtNetUdpBind("127.0.0.1", 0u);
	pUdpRecv = pUdpServer ? xrtNetUdpRecvAsync(pUdpServer, 2000) : NULL;
	pUdpSend = pUdpClient && pUdpServer ? xrtNetUdpSendBytesAsync(pUdpClient,
		"localhost", (uint16)xrtNetUdpLocalPort(pUdpServer), aUdpPayload, sizeof(aUdpPayload)) : NULL;
	if ( pUdpSend ) { (void)xFutureWaitTimeout(pUdpSend, 3000); }
	if ( pUdpRecv && xFutureWaitTimeout(pUdpRecv, 3000) && xFutureStatus(pUdpRecv) == XRT_NET_OK ) {
		xnetdgrampkt** ppPacket = (xnetdgrampkt**)xFutureValue(pUdpRecv);
		pPacket = ppPacket ? *ppPacket : NULL;
	}

	bOk = pClient != NULL && pAccepted != NULL && pBytes != NULL &&
		pResolved4 && pResolved4->iFamily == AF_INET &&
		pResolved6 && pResolved6->iFamily == AF_INET6 &&
		pResolvedAll && xrtNetAddrListCount(pResolvedAll) > 0u &&
		pSyncResolvedAll && xrtNetAddrListCount(pSyncResolvedAll) > 0u &&
		pBytes->iLen == sizeof(aPayload) && memcmp(pBytes->pData, aPayload, sizeof(aPayload)) == 0 &&
		pSend && xFutureStatus(pSend) == XRT_NET_OK && pDrain && xFutureStatus(pDrain) == XRT_NET_OK &&
		pTimeout && xFutureStatus(pTimeout) == XRT_NET_TIMEOUT &&
		pCancelConnect && xFutureStatus(pCancelConnect) == XRT_NET_CANCELLED &&
		pUdpSend && xFutureStatus(pUdpSend) == XRT_NET_OK && pPacket &&
		xrtNetDgramPacketBytes(pPacket) == sizeof(aUdpPayload);
	if ( !bOk ) {
		printf("  Async detail resolve4=%d resolve6=%d connect=%d cancel=%d accept=%d bytes=%u send=%d drain=%d timeout=%d udp_send=%d udp_recv=%d\n",
			pResolve4 ? (int)xFutureStatus(pResolve4) : 99,
			pResolve6 ? (int)xFutureStatus(pResolve6) : 99,
			pConnect ? (int)xFutureStatus(pConnect) : 99,
			pCancelConnect ? (int)xFutureStatus(pCancelConnect) : 99,
			pAccept ? (int)xFutureStatus(pAccept) : 99,
			(unsigned)(pBytes ? pBytes->iLen : 0u),
			pSend ? (int)xFutureStatus(pSend) : 99,
			pDrain ? (int)xFutureStatus(pDrain) : 99,
			pTimeout ? (int)xFutureStatus(pTimeout) : 99,
			pUdpSend ? (int)xFutureStatus(pUdpSend) : 99,
			pUdpRecv ? (int)xFutureStatus(pUdpRecv) : 99);
	}

	if ( pPacket ) xrtNetDgramPacketDestroy(pPacket);
	if ( pSyncResolvedAll ) xrtNetAddrListDestroy(pSyncResolvedAll);
	if ( pUdpSend ) xFutureRelease(pUdpSend);
	if ( pUdpRecv ) xFutureRelease(pUdpRecv);
	if ( pTimeout ) xFutureRelease(pTimeout);
	if ( pDrain ) xFutureRelease(pDrain);
	if ( pRecv ) xFutureRelease(pRecv);
	if ( pSend ) xFutureRelease(pSend);
	if ( pResolve6 ) xFutureRelease(pResolve6);
	if ( pResolve4 ) xFutureRelease(pResolve4);
	if ( pResolveAll ) xFutureRelease(pResolveAll);
	if ( pCancelConnect ) xFutureRelease(pCancelConnect);
	if ( pConnect ) xFutureRelease(pConnect);
	if ( pAccept ) xFutureRelease(pAccept);
	if ( pUdpClient ) xrtNetUdpDestroy(pUdpClient);
	if ( pUdpServer ) xrtNetUdpDestroy(pUdpServer);
	if ( pClient ) xrtNetTcpStreamDestroy(pClient);
	if ( pAccepted ) xrtNetTcpStreamDestroy(pAccepted);
	if ( pListener ) xrtNetTcpListenerDestroy(pListener);
	xrtNetSyncShutdownHiddenEngine();
	return bOk;
}


int Test_XNet2_Convenience(void)
{
	int iFailCount = 0;
	xnetlistenconfig tListenCfg;
	xnetconnectconfig tConnectCfg;
	xnetdgramconfig tDgramCfg;
	xnetlistener* pListener = NULL;
	xnetstream* pClient = NULL;
	xthread hServerThread = NULL;
	__test_xnet2_conv_tcp_server tServer;
	uint16 iPort = 0u;
	static const uint8 aTcpPayload[] = { 'x', 0u, 'l', '6' };
	static const uint8 aTcpReply[] = { 'o', 'k', 0u, '6' };
	static const uint8 aTcpGreeting[] = { '2', '2', '0', ' ' };
	void* pGreeting = NULL;
	size_t iGreetingLen = 0u;
	void* pReply = NULL;
	size_t iReplyLen = 0u;
	xdgramsock* pUdpServer = NULL;
	xdgramsock* pUdpClient = NULL;
	xnetdgrampkt* pPacket = NULL;
	void* pPacketBytes = NULL;
	size_t iPacketLen = 0u;
	bool bUdpSendOk = false;
	bool bCloseOrderOk = false;
	bool bAcceptTimeoutGapOk = false;
	bool bAsyncFacadeOk = false;
	bool bEngineLiveGuardOk = false;
	static const uint8 aUdpPayload[] = { 'u', 0u, 'd', 'p' };

	memset(&tServer, 0, sizeof(tServer));
	xrtNetSyncShutdownHiddenEngine();

	xrtNetListenConfigInit(&tListenCfg);
	(void)xrtNetAddrParse(&tListenCfg.tBindAddr, "127.0.0.1", 0u);
	tListenCfg.iFlags |= XNET_LISTEN_F_REUSE_ADDR | XNET_LISTEN_F_NO_DELAY;
	pListener = xrtNetTcpListenEx(&tListenCfg);
	iPort = xrtNetTcpListenerPort(pListener);
	tServer.pListener = pListener;
	if ( pListener ) {
		hServerThread = xrtThreadCreate(__Test_XNet2_ConvenienceTcpServerProc, &tServer, 0);
	}

	xrtNetConnectConfigInit(&tConnectCfg);
	tConnectCfg.sHost = "127.0.0.1";
	tConnectCfg.iPort = iPort;
	tConnectCfg.iFlags |= XNET_CONNECT_F_NO_DELAY;
	tConnectCfg.iConnectTimeoutMs = 2000u;
	pClient = iPort ? xrtNetTcpConnectEx(&tConnectCfg) : NULL;
	if ( pClient ) {
		pGreeting = xrtNetStreamRecvBytes(pClient, sizeof(aTcpGreeting), 2000, &iGreetingLen);
		(void)xrtNetStreamSendBytes(pClient, aTcpPayload, sizeof(aTcpPayload));
		pReply = xrtNetStreamRecvBytes(pClient, sizeof(aTcpReply), 2000, &iReplyLen);
	}
	if ( hServerThread ) {
		xrtThreadWait(hServerThread);
		xrtThreadDestroy(hServerThread);
	}

	printf("\n\n\n------------------------------------\n\n XNet2 Convenience Test:\n\n");
	printf("  TCP ListenEx creates listener : %s\n", pListener && iPort > 0u ? "PASS" : "FAIL");
	printf("  TCP ConnectEx creates stream : %s\n", pClient ? "PASS" : "FAIL");
	printf("  TCP accepted stream first send : %s\n",
		tServer.bGreetingSendOk && tServer.bGreetingDrainOk &&
		pGreeting && iGreetingLen == sizeof(aTcpGreeting) &&
		memcmp(pGreeting, aTcpGreeting, sizeof(aTcpGreeting)) == 0 ? "PASS" : "FAIL");
	printf("  TCP SendBytes/RecvBytes server payload : %s\n",
		tServer.bRecvOk && tServer.iRecvLen == sizeof(aTcpPayload) &&
		memcmp(tServer.aRecv, aTcpPayload, sizeof(aTcpPayload)) == 0 ? "PASS" : "FAIL");
	printf("  TCP SendBytes server send accepted : %s\n", tServer.bSendOk ? "PASS" : "FAIL");
	printf("  TCP SendBytes server drain : %s\n", tServer.bDrainOk ? "PASS" : "FAIL");
	printf("  TCP SendBytes/RecvBytes client reply length : %s\n",
		pReply && iReplyLen == sizeof(aTcpReply) ? "PASS" : "FAIL");
	printf("  TCP SendBytes/RecvBytes client reply : %s\n",
		pReply && iReplyLen == sizeof(aTcpReply) &&
		memcmp(pReply, aTcpReply, sizeof(aTcpReply)) == 0 ? "PASS" : "FAIL");

	if ( !(pListener && iPort > 0u) ) { ++iFailCount; }
	if ( !pClient ) { ++iFailCount; }
	if ( !(tServer.bGreetingSendOk && tServer.bGreetingDrainOk &&
		pGreeting && iGreetingLen == sizeof(aTcpGreeting) &&
		memcmp(pGreeting, aTcpGreeting, sizeof(aTcpGreeting)) == 0) ) { ++iFailCount; }
	if ( !(tServer.bRecvOk && tServer.iRecvLen == sizeof(aTcpPayload) && memcmp(tServer.aRecv, aTcpPayload, sizeof(aTcpPayload)) == 0) ) { ++iFailCount; }
	if ( !(pReply && iReplyLen == sizeof(aTcpReply) && memcmp(pReply, aTcpReply, sizeof(aTcpReply)) == 0) ) { ++iFailCount; }

	xrtFree(pGreeting);
	xrtFree(pReply);
	if ( pClient ) { xrtNetTcpStreamDestroy(pClient); }
	if ( tServer.pAccepted ) { xrtNetTcpStreamDestroy(tServer.pAccepted); }
	if ( pListener ) { xrtNetTcpListenerDestroy(pListener); }
	xrtNetSyncShutdownHiddenEngine();

	bCloseOrderOk = __Test_XNet2_ConvenienceCloseOrder();
	printf("  TCP send-close-destroy preserves queued bytes : %s\n", bCloseOrderOk ? "PASS" : "FAIL");
	if ( !bCloseOrderOk ) { ++iFailCount; }

	bAcceptTimeoutGapOk = __Test_XNet2_ConvenienceAcceptTimeoutGap();
	printf("  TCP accept preserves connection across timeout gap : %s\n", bAcceptTimeoutGapOk ? "PASS" : "FAIL");
	if ( !bAcceptTimeoutGapOk ) { ++iFailCount; }

	bAsyncFacadeOk = __Test_XNet2_ConvenienceAsyncFacade();
	printf("  Async facade uses engine futures, DNS, bytes and timeout : %s\n", bAsyncFacadeOk ? "PASS" : "FAIL");
	if ( !bAsyncFacadeOk ) { ++iFailCount; }
	bEngineLiveGuardOk = __Test_XNet2_EngineLiveObjectGuard();
	printf("  Engine destroy rejects live network objects : %s\n", bEngineLiveGuardOk ? "PASS" : "FAIL");
	if ( !bEngineLiveGuardOk ) { ++iFailCount; }

	xrtNetDgramConfigInit(&tDgramCfg);
	(void)xrtNetAddrParse(&tDgramCfg.tBindAddr, "127.0.0.1", 0u);
	pUdpServer = xrtNetUdpBindEx(&tDgramCfg);
	xrtNetDgramConfigInit(&tDgramCfg);
	(void)xrtNetAddrParse(&tDgramCfg.tBindAddr, "127.0.0.1", 0u);
	pUdpClient = xrtNetUdpBindEx(&tDgramCfg);
	if ( pUdpServer && pUdpClient ) {
		bUdpSendOk = xrtNetUdpSendBytes(pUdpClient, "127.0.0.1", (uint16)xrtNetUdpLocalPort(pUdpServer), aUdpPayload, sizeof(aUdpPayload));
		pPacket = xrtNetUdpRecv(pUdpServer, 2000);
		pPacketBytes = xrtNetDgramPacketBytesCopy(pPacket, &iPacketLen);
	}

	printf("  UDP BindEx creates sockets : %s\n", pUdpServer && pUdpClient ? "PASS" : "FAIL");
	printf("  UDP SendBytes posts datagram : %s\n", bUdpSendOk ? "PASS" : "FAIL");
	printf("  UDP Recv packet created : %s\n", pPacket ? "PASS" : "FAIL");
	printf("  UDP PacketBytesCopy length : %s\n",
		pPacketBytes && iPacketLen == sizeof(aUdpPayload) ? "PASS" : "FAIL");
	printf("  UDP SendBytes/PacketBytesCopy payload : %s\n",
		pPacketBytes && iPacketLen == sizeof(aUdpPayload) &&
		memcmp(pPacketBytes, aUdpPayload, sizeof(aUdpPayload)) == 0 ? "PASS" : "FAIL");

	if ( !(pUdpServer && pUdpClient) ) { ++iFailCount; }
	if ( !bUdpSendOk ) { ++iFailCount; }
	if ( !(pPacketBytes && iPacketLen == sizeof(aUdpPayload) && memcmp(pPacketBytes, aUdpPayload, sizeof(aUdpPayload)) == 0) ) { ++iFailCount; }

	xrtFree(pPacketBytes);
	if ( pPacket ) { xrtNetDgramPacketDestroy(pPacket); }
	if ( pUdpClient ) { xrtNetUdpDestroy(pUdpClient); }
	if ( pUdpServer ) { xrtNetUdpDestroy(pUdpServer); }
	xrtNetSyncShutdownHiddenEngine();

	return iFailCount == 0 ? 0 : 1;
}

#endif
