#ifndef XRT_TEST_XNET2_CONVENIENCE_H
#define XRT_TEST_XNET2_CONVENIENCE_H

typedef struct {
	xnetlistener* pListener;
	xnetstream* pAccepted;
	uint8 aRecv[16];
	size_t iRecvLen;
	bool bAcceptOk;
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

	if ( !pCase || !pCase->pListener ) { return 0; }
	pStream = xrtNetTcpAccept(pCase->pListener, 2000);
	pCase->bAcceptOk = pStream != NULL;
	pCase->pAccepted = pStream;
	if ( pStream ) {
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
	void* pReply = NULL;
	size_t iReplyLen = 0u;
	xdgramsock* pUdpServer = NULL;
	xdgramsock* pUdpClient = NULL;
	xnetdgrampkt* pPacket = NULL;
	void* pPacketBytes = NULL;
	size_t iPacketLen = 0u;
	bool bUdpSendOk = false;
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
	if ( !(tServer.bRecvOk && tServer.iRecvLen == sizeof(aTcpPayload) && memcmp(tServer.aRecv, aTcpPayload, sizeof(aTcpPayload)) == 0) ) { ++iFailCount; }
	if ( !(pReply && iReplyLen == sizeof(aTcpReply) && memcmp(pReply, aTcpReply, sizeof(aTcpReply)) == 0) ) { ++iFailCount; }

	xrtFree(pReply);
	if ( pClient ) { xrtNetTcpStreamDestroy(pClient); }
	if ( tServer.pAccepted ) { xrtNetTcpStreamDestroy(tServer.pAccepted); }
	if ( pListener ) { xrtNetTcpListenerDestroy(pListener); }
	xrtNetSyncShutdownHiddenEngine();

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
