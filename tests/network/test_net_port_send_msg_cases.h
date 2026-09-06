#ifndef XRT_TEST_NET_PORT_SEND_MSG_CASES_H
#define XRT_TEST_NET_PORT_SEND_MSG_CASES_H

/* 两种完成式后端共享空控制、零 Flags 和显式控制的终态矩阵。 */
static void testNetPortSendMsgCases(xnetportbackend Backend)
{
	xnetportconfig Config;
	xnetport* pPort;
	xnetsocket Server;
	xnetsocket Client;
	xnetaddr ServerAddress;
	xnetaddr ClientAddress;
	xnetdgramcontrol Control;
	xnetspan Spans[2] = {
		{ (cbytes)"ab", 2u },
		{ (cbytes)"cde", 3u }
	};
	uint64 Id = 1;

	xrtNetPortConfigInit(&Config);
	Config.Backend = Backend;
	pPort = xrtNetPortCreate(&Config);
	Server = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM, XNET_SOCKET_NONBLOCK);
	Client = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM, XNET_SOCKET_NONBLOCK);
	testRequire((pPort != NULL) && (Server != NULL) && (Client != NULL),
		"SendMsg event matrix setup failed");
	testRequire(xrtNetAddrLoopback(&ServerAddress, XNET_FAMILY_IPV4, 0) &&
		xrtNetAddrLoopback(&ClientAddress, XNET_FAMILY_IPV4, 0) &&
		xrtNetSocketBind(Server, &ServerAddress) &&
		xrtNetSocketBind(Client, &ClientAddress) &&
		xrtNetSocketLocal(Server, &ServerAddress) &&
		xrtNetSocketLocal(Client, &ClientAddress) &&
		(xrtNetSocketConnect(Client, &ServerAddress) == XNET_RESULT_OK),
		"SendMsg event matrix bind/connect failed");

	for ( unsigned iControl = 0; iControl < 3; iControl++ ) {
		const xnetdgramcontrol* pControl = iControl == 0 ? NULL : &Control;

		memset(&Control, 0, sizeof(Control));
		if ( iControl == 2 ) {
			Control.Flags = XNET_DGRAM_CONTROL_SOURCE;
			Control.Source = ClientAddress;
			Control.Source.Port = 0;
		}
		for ( unsigned iRemote = 0; iRemote < 2; iRemote++ ) {
			const xnetaddr* pRemote = iRemote ? &ServerAddress : NULL;
			xnetporteventtype Type = iControl == 2 ? XNET_PORT_EVENT_SEND_MSG :
				(iRemote ? XNET_PORT_EVENT_SEND_TO : XNET_PORT_EVENT_SEND);

			for ( unsigned iVector = 0; iVector < 2; iVector++ ) {
				xnetportevent Events[2];
				const xnetportevent* pReceive = NULL;
				const xnetportevent* pSend = NULL;
				xdeadline Deadline = xrtDeadlineAfter(5000000u);
				size_t iCount = 0;
				char Data[5] = { 0 };
				bool bSent;

				testRequire(xrtNetPortRecvFrom(pPort, Server,
					Data, sizeof(Data), Id, Server),
					"SendMsg event matrix receive submit failed");
				bSent = iVector ? xrtNetPortSendMsgVec(pPort, Client,
					Spans, 2, pRemote, pControl, Id + 1u, Client) :
					xrtNetPortSendMsg(pPort, Client, "abcde", 5,
						pRemote, pControl, Id + 1u, Client);
				testRequire(bSent, "SendMsg event matrix send submit failed");
				while ( iCount < 2 ) {
					size_t iReady = 0;

					testRequire(xrtNetPortWait(pPort, Events + iCount,
						2u - iCount, Deadline, &iReady) == XNET_RESULT_OK,
						"SendMsg event matrix wait failed");
					testRequire(iReady != 0, "SendMsg event matrix timed out");
					iCount += iReady;
				}
				for ( size_t i = 0; i < iCount; i++ ) {
					if ( Events[i].Id == Id ) pReceive = &Events[i];
					if ( Events[i].Id == Id + 1u ) pSend = &Events[i];
				}
				testRequire((pReceive != NULL) &&
					(pReceive->Type == XNET_PORT_EVENT_RECV_FROM) &&
					(pReceive->Result == XNET_RESULT_OK) &&
					(pReceive->Bytes == sizeof(Data)) &&
					(pReceive->User == Server) &&
					(memcmp(Data, "abcde", sizeof(Data)) == 0),
					"SendMsg event matrix payload mismatch");
				testRequire((pSend != NULL) && (pSend->Type == Type) &&
					(pSend->Result == XNET_RESULT_OK) &&
					(pSend->Bytes == sizeof(Data)) && (pSend->User == Client),
					"SendMsg event matrix terminal type mismatch");
				if ( pRemote != NULL ) {
					testRequire(xrtNetAddrEqual(&pSend->Address, pRemote),
						"SendMsg event matrix destination mismatch");
				}
				Id += 2u;
			}
		}
	}
	testRequire(xrtNetPortDestroy(pPort) &&
		xrtNetSocketClose(Client) && xrtNetSocketClose(Server),
		"SendMsg event matrix cleanup failed");
}

#endif
