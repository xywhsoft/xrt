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
