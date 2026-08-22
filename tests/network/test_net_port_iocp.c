#include "../test.h"
#include <limits.h>

#if defined(_WIN32) || defined(_WIN64)
	#if defined(__TINYC__)
		#include <winapi/windows.h>
	#else
		#include <windows.h>
	#endif
#endif



#if defined(_WIN32) || defined(_WIN64)

/* 验证 IOCP 创建的 Socket 句柄不会泄漏给后续子进程。 */
static bool testIOCPNoInherit(xnetsocket Socket)
{
	DWORD iFlags = 0;
	intptr_t iNative = xrtNetSocketNative(Socket);

	return (iNative != (intptr_t)-1) &&
		(GetHandleInformation((HANDLE)(uintptr_t)iNative, &iFlags) != 0) &&
		((iFlags & HANDLE_FLAG_INHERIT) == 0);
}



/* 提交失败时附带结构化错误，便于区分公共校验和平台拒绝。 */
static void testIOCPRequire(bool bResult, cstr sMessage)
{
	if ( !bResult && (xrtGetError() != NULL) ) {
		fprintf(stderr, "[ERROR] operation=%s message=%s system=%d\n",
			xrtErrorOperation(xrtGetError()),
			xrtErrorMessage(xrtGetError()),
			(int)xrtErrorSystemCode(xrtGetError()));
	}
	testRequire(bResult, sMessage);
}



/* 等待固定数量的终态事件，保留一次统一截止时间。 */
static void testIOCPWait(xnetport* pPort,
	xnetportevent* pEvents, size_t iExpected)
{
	xdeadline Deadline = xrtDeadlineAfter(5000000);
	size_t iCount = 0;

	while ( iCount < iExpected ) {
		size_t iReady = 0;
		xnetresult Result;

		Result = xrtNetPortWait(pPort, pEvents + iCount,
			iExpected - iCount, Deadline, &iReady);
		if ( Result != XNET_RESULT_OK ) {
			const xerror* pError = xrtGetError();

			fprintf(stderr,
				"[IOCP wait] result=%d ready=%zu received=%zu "
				"error=%d/%d/%d\n",
				(int)Result,
				iReady,
				iCount,
				pError != NULL ? (int)xrtErrorKind(pError) : 0,
				pError != NULL ? xrtErrorCode(pError) : 0,
				pError != NULL ? xrtErrorSystemCode(pError) : 0
			);
			for ( size_t i = 0; i < iCount; i++ ) {
				fprintf(stderr,
					"[IOCP event] id=%llu type=%d result=%d system=%d\n",
					(unsigned long long)pEvents[i].Id,
					(int)pEvents[i].Type,
					(int)pEvents[i].Result,
					(int)pEvents[i].SystemCode
				);
			}
		}
		testRequire(Result == XNET_RESULT_OK, "IOCP wait failed");
		testRequire(iReady != 0, "IOCP wait made no progress");
		iCount += iReady;
	}
}



/* 按操作 ID 查找无序到达的完成事件。 */
static const xnetportevent* testIOCPEvent(
	const xnetportevent* pEvents, size_t iCount, uint64 Id)
{
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( pEvents[i].Id == Id ) {
			return &pEvents[i];
		}
	}
	return NULL;
}



/* 创建一个回环 TCP 监听 Socket，并返回实际监听地址。 */
static xnetsocket testIOCPListener(xnetaddr* pAddress)
{
	xnetsocket Listener = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM, XNET_SOCKET_NONBLOCK);

	testRequire(Listener != NULL, "IOCP listener open failed");
	testRequire(xrtNetAddrLoopback(pAddress, XNET_FAMILY_IPV4, 0) &&
		xrtNetSocketBind(Listener, pAddress) &&
		xrtNetSocketListen(Listener, 16) &&
		xrtNetSocketLocal(Listener, pAddress),
		"IOCP listener setup failed");
	return Listener;
}



/* AcceptEx、ConnectEx、向量收发和流 EOF 必须保持同一套终态契约。 */
static void testIOCPTCP(void)
{
	xnetportconfig Config;
	xnetportevent Events[2];
	const xnetportevent* pAccept;
	const xnetportevent* pConnect;
	xnetport* pPort;
	xnetsocket Listener;
	xnetsocket Client;
	xnetsocket Server;
	xnetaddr Address;
	xnetwspan ReadSpans[2];
	char sReceived[5] = { 0 };
	const char sSent[] = "hello";
	char iProbe = 0;
	size_t iReceived;
	uint64 iNextId = 5;

	xrtNetPortConfigInit(&Config);
	Config.Backend = XNET_PORT_IOCP;
	pPort = xrtNetPortCreate(&Config);
	Listener = testIOCPListener(&Address);
	Client = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM, XNET_SOCKET_NONBLOCK);
	testRequire((pPort != NULL) && (Client != NULL),
		"IOCP TCP setup failed");

	testRequire(xrtNetPortAccept(pPort, Listener, 1, Listener) &&
		xrtNetPortConnect(pPort, Client, &Address, 2, Client),
		"IOCP accept/connect submit failed");
	testIOCPWait(pPort, Events, 2);
	pAccept = testIOCPEvent(Events, 2, 1);
	pConnect = testIOCPEvent(Events, 2, 2);
	testRequire((pAccept != NULL) &&
		(pAccept->Type == XNET_PORT_EVENT_ACCEPT) &&
		(pAccept->Result == XNET_RESULT_OK) &&
		(pAccept->Accepted != NULL) &&
		(pAccept->Socket == Listener) &&
		(pAccept->User == Listener) &&
		(pAccept->Address.Family == XNET_FAMILY_IPV4) &&
		testIOCPNoInherit(pAccept->Accepted),
		"IOCP accept event mismatch");
	testRequire((pConnect != NULL) &&
		(pConnect->Type == XNET_PORT_EVENT_CONNECT) &&
		(pConnect->Result == XNET_RESULT_OK) &&
		(pConnect->Socket == Client) &&
		(pConnect->User == Client) &&
		xrtNetAddrEqual(&pConnect->Address, &Address),
		"IOCP connect event mismatch");
	Server = pAccept->Accepted;

	/* 可读探测不占载荷缓冲，也不能消费或伪造流字节。 */
	testRequire(
		xrtNetPortReadProbe(pPort, Server, 101, Server) &&
		xrtNetPortSend(pPort, Client, "P", 1, 102, Client),
		"IOCP read probe submit failed"
	);
	testIOCPWait(pPort, Events, 2);
	testRequire(
		(testIOCPEvent(Events, 2, 101) != NULL) &&
		(testIOCPEvent(Events, 2, 101)->Type ==
			XNET_PORT_EVENT_READ_PROBE) &&
		(testIOCPEvent(Events, 2, 101)->Result == XNET_RESULT_OK) &&
		(testIOCPEvent(Events, 2, 101)->Bytes == 0) &&
		((testIOCPEvent(Events, 2, 101)->Flags &
			XNET_PORT_EVENT_EOF) == 0),
		"IOCP read probe terminal mismatch"
	);
	testRequire(
		xrtNetPortRecv(pPort, Server, &iProbe, 1, 103, &iProbe),
		"IOCP receive after probe failed"
	);
	testIOCPWait(pPort, Events, 1);
	testRequire(
		(Events[0].Id == 103) &&
		(Events[0].Type == XNET_PORT_EVENT_RECV) &&
		(Events[0].Result == XNET_RESULT_OK) &&
		(Events[0].Bytes == 1) && (iProbe == 'P'),
		"IOCP read probe consumed stream data"
	);

	ReadSpans[0].Data = (bytes)sReceived;
	ReadSpans[0].Size = 2;
	ReadSpans[1].Data = (bytes)sReceived + 2;
	ReadSpans[1].Size = 3;
	testRequire(xrtNetPortRecvVec(pPort, Server,
		ReadSpans, 2, 3, sReceived),
		"IOCP TCP receive submit failed");
	testIOCPRequire(xrtNetPortSend(pPort, Client,
		sSent, sizeof(sSent) - 1, 4, (ptr)sSent),
		"IOCP TCP send submit failed");
	testIOCPWait(pPort, Events, 2);
	testRequire((testIOCPEvent(Events, 2, 4) != NULL) &&
		(testIOCPEvent(Events, 2, 4)->Result == XNET_RESULT_OK) &&
		(testIOCPEvent(Events, 2, 4)->Bytes == (sizeof(sSent) - 1)),
		"IOCP TCP send completion mismatch");
	testRequire((testIOCPEvent(Events, 2, 3) != NULL) &&
		(testIOCPEvent(Events, 2, 3)->Result == XNET_RESULT_OK) &&
		(testIOCPEvent(Events, 2, 3)->Bytes != 0) &&
		(testIOCPEvent(Events, 2, 3)->Bytes <= sizeof(sReceived)),
		"IOCP TCP receive completion mismatch");
	iReceived = testIOCPEvent(Events, 2, 3)->Bytes;
	testRequire(memcmp(sReceived, sSent, iReceived) == 0,
		"IOCP vector receive data mismatch");

	/* TCP 允许短读，继续提交直到完整消费同一次发送。 */
	while ( iReceived < sizeof(sReceived) ) {
		testRequire(xrtNetPortRecv(pPort, Server,
			sReceived + iReceived, sizeof(sReceived) - iReceived,
			iNextId, NULL), "IOCP TCP remainder receive submit failed");
		testIOCPWait(pPort, Events, 1);
		testRequire((Events[0].Id == iNextId) &&
			(Events[0].Result == XNET_RESULT_OK) &&
			(Events[0].Bytes != 0),
			"IOCP TCP remainder receive mismatch");
		iReceived += Events[0].Bytes;
		iNextId++;
	}
	testRequire(memcmp(sReceived, sSent, sizeof(sReceived)) == 0,
		"IOCP TCP complete data mismatch");

	testRequire(xrtNetSocketShutdown(Client, XNET_SHUTDOWN_WRITE) &&
		xrtNetPortRecv(pPort, Server, sReceived,
			sizeof(sReceived), iNextId, NULL),
		"IOCP TCP EOF submit failed");
	testIOCPWait(pPort, Events, 1);
	testRequire((Events[0].Id == iNextId) &&
		(Events[0].Type == XNET_PORT_EVENT_RECV) &&
		(Events[0].Result == XNET_RESULT_CLOSED) &&
		(Events[0].Bytes == 0) &&
		((Events[0].Flags & XNET_PORT_EVENT_EOF) != 0),
		"IOCP TCP EOF completion mismatch");

	testRequire(xrtNetSocketClose(Server) &&
		xrtNetSocketClose(Client) &&
		xrtNetSocketClose(Listener) &&
		xrtNetPortDestroy(pPort), "IOCP TCP cleanup failed");
}



/* UDP 完成必须覆盖向量、零长度和截断，且不依赖后端固定缓冲。 */
static void testIOCPUDP(void)
{
	xnetportconfig Config;
	xnetportevent Events[2];
	const xnetportevent* pReceive;
	const xnetportevent* pSend;
	xnetport* pPort;
	xnetsocket Server;
	xnetsocket Client;
	xnetsocket ConnectedServer;
	xnetsocket ConnectedClient;
	xnetaddr ServerAddress;
	xnetaddr ClientAddress;
	xnetaddr ConnectedServerAddress;
	xnetaddr ConnectedClientAddress;
	xnetwspan ReadSpans[2];
	xnetspan WriteSpans[2];
	xnetdgramcontrol Control;
	char sData[5] = { 0 };
	char sSmall[2] = { 0 };

	xrtNetPortConfigInit(&Config);
	Config.Backend = XNET_PORT_IOCP;
	pPort = xrtNetPortCreate(&Config);
	Server = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM, XNET_SOCKET_NONBLOCK);
	Client = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM, XNET_SOCKET_NONBLOCK);
	testRequire((pPort != NULL) && (Server != NULL) && (Client != NULL),
		"IOCP UDP setup failed");
	testRequire(xrtNetAddrLoopback(&ServerAddress,
		XNET_FAMILY_IPV4, 0) &&
		xrtNetAddrLoopback(&ClientAddress,
			XNET_FAMILY_IPV4, 0) &&
		xrtNetSocketBind(Server, &ServerAddress) &&
		xrtNetSocketBind(Client, &ClientAddress) &&
		xrtNetSocketLocal(Server, &ServerAddress) &&
		xrtNetSocketLocal(Client, &ClientAddress),
		"IOCP UDP bind failed");

	ReadSpans[0].Data = (bytes)sData;
	ReadSpans[0].Size = 2;
	ReadSpans[1].Data = (bytes)sData + 2;
	ReadSpans[1].Size = 3;
	WriteSpans[0].Data = (cbytes)"ab";
	WriteSpans[0].Size = 2;
	WriteSpans[1].Data = (cbytes)"cde";
	WriteSpans[1].Size = 3;
	testRequire(xrtNetPortRecvFromVec(pPort, Server,
		ReadSpans, 2, 11, Server) &&
		xrtNetPortSendToVec(pPort, Client,
			WriteSpans, 2, &ServerAddress, 12, Client),
		"IOCP UDP vector submit failed");
	testIOCPWait(pPort, Events, 2);
	pReceive = testIOCPEvent(Events, 2, 11);
	pSend = testIOCPEvent(Events, 2, 12);
	testRequire((pReceive != NULL) &&
		(pReceive->Type == XNET_PORT_EVENT_RECV_FROM) &&
		(pReceive->Result == XNET_RESULT_OK) &&
		(pReceive->Bytes == sizeof(sData)) &&
		(pReceive->User == Server) &&
		(memcmp(sData, "abcde", sizeof(sData)) == 0),
		"IOCP UDP vector receive mismatch");
	testRequire((pSend != NULL) &&
		(pSend->Type == XNET_PORT_EVENT_SEND_TO) &&
		(pSend->Result == XNET_RESULT_OK) &&
		(pSend->Bytes == sizeof(sData)) &&
		(pSend->User == Client) &&
		xrtNetAddrEqual(&pSend->Address, &ServerAddress),
		"IOCP UDP vector send mismatch");
	testRequire(xrtNetSocketLocal(Client, &ClientAddress) &&
		(pReceive->Address.Family == ClientAddress.Family) &&
		(pReceive->Address.Port == ClientAddress.Port) &&
		xrtNetAddrIsLoopback(&pReceive->Address),
		"IOCP UDP remote address mismatch");

	/* WSARecvMsg 完成必须把目标地址和接口元数据写入同一个终态。 */
	testRequire(xrtNetSocketDgramMetaSet(
		Server,
		XNET_DGRAM_META_DESTINATION | XNET_DGRAM_META_INTERFACE
	), "enabling IOCP UDP metadata failed");
	testRequire(xrtNetPortRecvMsg(pPort, Server,
		sData, sizeof(sData), 90, Server) &&
		xrtNetPortSendTo(pPort, Client,
			"meta", 4, &ServerAddress, 91, Client),
		"IOCP UDP metadata submit failed");
	testIOCPWait(pPort, Events, 2);
	pReceive = testIOCPEvent(Events, 2, 90);
	testRequire((pReceive != NULL) &&
		(pReceive->Type == XNET_PORT_EVENT_RECV_MSG) &&
		(pReceive->Result == XNET_RESULT_OK) &&
		(pReceive->Bytes == 4) && (memcmp(sData, "meta", 4) == 0) &&
		((pReceive->Meta.Flags & (XNET_DGRAM_META_DESTINATION |
			XNET_DGRAM_META_INTERFACE)) ==
		 (XNET_DGRAM_META_DESTINATION | XNET_DGRAM_META_INTERFACE)) &&
		xrtNetAddrIsLoopback(&pReceive->Meta.Destination) &&
		(pReceive->Meta.Interface != 0),
		"IOCP UDP metadata event mismatch");

	/* WSASendMsg 必须复制逐包控制，并以独立 SEND_MSG 终态结束。 */
	testRequire((xrtNetSocketDgramControlAvailable(Client) &
		XNET_DGRAM_CONTROL_SOURCE) != 0,
		"IOCP UDP source control is unavailable");
	memset(&Control, 0, sizeof(Control));
	Control.Flags = XNET_DGRAM_CONTROL_SOURCE;
	Control.Source = ClientAddress;
	Control.Source.Port = 0;
	testRequire(xrtNetPortRecvFrom(
		pPort, Server, sData, sizeof(sData), 92, Server
	) && xrtNetPortSendMsg(
		pPort,
		Client,
		"ctrl",
		4,
		&ServerAddress,
		&Control,
		93,
		Client
	), "IOCP UDP controlled send submit failed");
	testIOCPWait(pPort, Events, 2);
	pReceive = testIOCPEvent(Events, 2, 92);
	pSend = testIOCPEvent(Events, 2, 93);
	testRequire((pReceive != NULL) &&
		(pReceive->Result == XNET_RESULT_OK) &&
		(pReceive->Bytes == 4) && (memcmp(sData, "ctrl", 4) == 0) &&
		(pReceive->Address.Port == ClientAddress.Port),
		"IOCP UDP controlled receive mismatch");
	testRequire((pSend != NULL) &&
		(pSend->Type == XNET_PORT_EVENT_SEND_MSG) &&
		(pSend->Result == XNET_RESULT_OK) &&
		(pSend->Bytes == 4) &&
		xrtNetAddrEqual(&pSend->Address, &ServerAddress),
		"IOCP UDP controlled send event mismatch");

	/* 零长度数据报是成功控制结果，不得误判为 EOF。 */
	testRequire(xrtNetPortRecvFrom(pPort, Server,
		NULL, 0, 13, NULL) &&
		xrtNetPortSendTo(pPort, Client,
			NULL, 0, &ServerAddress, 14, NULL),
		"IOCP zero datagram submit failed");
	testIOCPWait(pPort, Events, 2);
	testRequire((testIOCPEvent(Events, 2, 13) != NULL) &&
		(testIOCPEvent(Events, 2, 13)->Result == XNET_RESULT_OK) &&
		(testIOCPEvent(Events, 2, 13)->Bytes == 0) &&
		((testIOCPEvent(Events, 2, 13)->Flags &
			XNET_PORT_EVENT_EOF) == 0),
		"IOCP zero datagram receive mismatch");
	testRequire((testIOCPEvent(Events, 2, 14) != NULL) &&
		(testIOCPEvent(Events, 2, 14)->Result == XNET_RESULT_OK) &&
		(testIOCPEvent(Events, 2, 14)->Bytes == 0),
		"IOCP zero datagram send mismatch");

	/* 缓冲不足是 TRUNCATED，不是端口错误，且保留实际写入前缀。 */
	testRequire(xrtNetPortRecvFrom(pPort, Server,
		sSmall, sizeof(sSmall), 15, NULL) &&
		xrtNetPortSendTo(pPort, Client,
			"large", 5, &ServerAddress, 16, NULL),
		"IOCP truncated datagram submit failed");
	testIOCPWait(pPort, Events, 2);
	pReceive = testIOCPEvent(Events, 2, 15);
	testRequire((pReceive != NULL) &&
		(pReceive->Result == XNET_RESULT_TRUNCATED) &&
		(pReceive->Bytes == sizeof(sSmall)) &&
		((pReceive->Flags & XNET_PORT_EVENT_ERROR) == 0) &&
		(memcmp(sSmall, "la", sizeof(sSmall)) == 0),
		"IOCP truncated datagram mismatch");

	/* 通用 Recv/Send 必须服务已连接 UDP，不能被流套接字检查拒绝。 */
	ConnectedServer = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM,
		0
	);
	ConnectedClient = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM,
		0
	);
	testRequire((ConnectedServer != NULL) &&
		(ConnectedClient != NULL) &&
		xrtNetAddrLoopback(
			&ConnectedServerAddress,
			XNET_FAMILY_IPV4,
			0
		) &&
		xrtNetAddrLoopback(
			&ConnectedClientAddress,
			XNET_FAMILY_IPV4,
			0
		) &&
		xrtNetSocketBind(ConnectedServer, &ConnectedServerAddress) &&
		xrtNetSocketBind(ConnectedClient, &ConnectedClientAddress) &&
		xrtNetSocketLocal(ConnectedServer, &ConnectedServerAddress) &&
		xrtNetSocketLocal(ConnectedClient, &ConnectedClientAddress) &&
		(xrtNetSocketConnect(
			ConnectedClient,
			&ConnectedServerAddress
		) ==
		XNET_RESULT_OK) &&
		(xrtNetSocketConnect(
			ConnectedServer,
			&ConnectedClientAddress
		) ==
		XNET_RESULT_OK) &&
		xrtNetSocketSet(
			ConnectedClient,
			XNET_OPTION_NONBLOCK,
			1
		) &&
		xrtNetSocketSet(
			ConnectedServer,
			XNET_OPTION_NONBLOCK,
			1
		),
		"IOCP connected UDP setup failed");
	memset(sData, 0, sizeof(sData));
	testRequire(xrtNetPortRecv(pPort, ConnectedServer,
		sData, sizeof(sData), 17, ConnectedServer) &&
		xrtNetPortSend(pPort, ConnectedClient,
			"plain", sizeof(sData), 18, ConnectedClient),
		"IOCP connected UDP submit failed");
	testIOCPWait(pPort, Events, 2);
	pReceive = testIOCPEvent(Events, 2, 17);
	pSend = testIOCPEvent(Events, 2, 18);
	testRequire((pReceive != NULL) &&
		(pReceive->Type == XNET_PORT_EVENT_RECV) &&
		(pReceive->Result == XNET_RESULT_OK) &&
		(pReceive->Bytes == sizeof(sData)) &&
		((pReceive->Flags & XNET_PORT_EVENT_EOF) == 0) &&
		(memcmp(sData, "plain", sizeof(sData)) == 0),
		"IOCP connected UDP receive mismatch");
	testRequire((pSend != NULL) &&
		(pSend->Type == XNET_PORT_EVENT_SEND) &&
		(pSend->Result == XNET_RESULT_OK) &&
		(pSend->Bytes == sizeof(sData)),
		"IOCP connected UDP send mismatch");

	/* 已连接 UDP 的零长度完成不是流 EOF。 */
	testRequire(xrtNetPortRecv(pPort, ConnectedServer,
		NULL, 0, 19, ConnectedServer) &&
		xrtNetPortSend(pPort, ConnectedClient,
			NULL, 0, 20, ConnectedClient),
		"IOCP connected zero datagram submit failed");
	testIOCPWait(pPort, Events, 2);
	pReceive = testIOCPEvent(Events, 2, 19);
	pSend = testIOCPEvent(Events, 2, 20);
	testRequire((pReceive != NULL) &&
		(pReceive->Type == XNET_PORT_EVENT_RECV) &&
		(pReceive->Result == XNET_RESULT_OK) &&
		(pReceive->Bytes == 0) &&
		((pReceive->Flags & XNET_PORT_EVENT_EOF) == 0),
		"IOCP connected zero datagram receive mismatch");
	testRequire((pSend != NULL) &&
		(pSend->Type == XNET_PORT_EVENT_SEND) &&
		(pSend->Result == XNET_RESULT_OK) &&
		(pSend->Bytes == 0),
		"IOCP connected zero datagram send mismatch");

	/* 通用 Recv 也必须保留已连接 UDP 的截断语义。 */
	memset(sSmall, 0, sizeof(sSmall));
	testRequire(xrtNetPortRecv(pPort, ConnectedServer,
		sSmall, sizeof(sSmall), 21, NULL) &&
		xrtNetPortSend(pPort, ConnectedClient,
			"large", 5, 22, NULL),
		"IOCP connected truncation submit failed");
	testIOCPWait(pPort, Events, 2);
	pReceive = testIOCPEvent(Events, 2, 21);
	testRequire((pReceive != NULL) &&
		(pReceive->Type == XNET_PORT_EVENT_RECV) &&
		(pReceive->Result == XNET_RESULT_TRUNCATED) &&
		(pReceive->Bytes == sizeof(sSmall)) &&
		((pReceive->Flags & XNET_PORT_EVENT_ERROR) == 0) &&
		(memcmp(sSmall, "la", sizeof(sSmall)) == 0),
		"IOCP connected truncation mismatch");

	testRequire(xrtNetSocketClose(ConnectedClient) &&
		xrtNetSocketClose(ConnectedServer) &&
		xrtNetSocketClose(Client) &&
		xrtNetSocketClose(Server) &&
		xrtNetPortDestroy(pPort), "IOCP UDP cleanup failed");
}



/* 操作 ID、硬上限和取消必须形成确定性背压与单终态。 */
static void testIOCPCancel(void)
{
	xnetportconfig Config;
	xnetportevent Events[2];
	xnetport* pPort;
	xnetsocket Listener;
	xnetaddr Address;

	xrtNetPortConfigInit(&Config);
	Config.Backend = XNET_PORT_IOCP;
	Config.OperationLimit = 2;
	pPort = xrtNetPortCreate(&Config);
	Listener = testIOCPListener(&Address);
	testRequire(pPort != NULL, "IOCP cancellation setup failed");

	testRequire(xrtNetPortAccept(pPort, Listener, 21, NULL),
		"first cancellable accept failed");
	testRequire(!xrtNetPortAccept(pPort, Listener, 21, NULL) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_EXISTS) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_PORT_SUBMIT),
		"duplicate active operation id was accepted");
	xrtClearError();
	testRequire(xrtNetPortAccept(pPort, Listener, 22, NULL),
		"second cancellable accept failed");
	testRequire(!xrtNetPortAccept(pPort, Listener, 23, NULL) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_AGAIN),
		"IOCP operation limit was not enforced");
	xrtClearError();

	testRequire(xrtNetPortCancel(pPort, 21) &&
		xrtNetPortCancel(pPort, 22), "IOCP cancellation request failed");
	testIOCPWait(pPort, Events, 2);
	testRequire((testIOCPEvent(Events, 2, 21) != NULL) &&
		(testIOCPEvent(Events, 2, 21)->Result == XNET_RESULT_CANCELLED) &&
		(testIOCPEvent(Events, 2, 21)->Accepted == NULL) &&
		((testIOCPEvent(Events, 2, 21)->Flags &
			XNET_PORT_EVENT_ERROR) == 0),
		"first IOCP cancellation terminal mismatch");
	testRequire((testIOCPEvent(Events, 2, 22) != NULL) &&
		(testIOCPEvent(Events, 2, 22)->Result == XNET_RESULT_CANCELLED) &&
		(testIOCPEvent(Events, 2, 22)->Accepted == NULL),
		"second IOCP cancellation terminal mismatch");
	testRequire(!xrtNetPortCancel(pPort, 99) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_PORT_CANCEL),
		"missing IOCP cancellation id mismatch");
	xrtClearError();

	testRequire(xrtNetSocketClose(Listener) &&
		xrtNetPortDestroy(pPort), "IOCP cancellation cleanup failed");
}



/* 公共层必须在进入系统前拒绝类型、地址、Span 和标识错误。 */
static void testIOCPInvalid(void)
{
	xnetportconfig Config;
	xnetspan Spans[65];
	xnetport* pPort;
	xnetsocket Stream;
	xnetsocket Datagram;
	xnetaddr Address;
	xnetwspan ReadSpan;
	xnetspan WriteSpan;
	char iByte = 0;

	xrtNetPortConfigInit(&Config);
	Config.Backend = XNET_PORT_IOCP;
	pPort = xrtNetPortCreate(&Config);
	Stream = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM, XNET_SOCKET_NONBLOCK);
	Datagram = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM, XNET_SOCKET_NONBLOCK);
	testRequire((pPort != NULL) && (Stream != NULL) && (Datagram != NULL),
		"IOCP invalid setup failed");
	for ( size_t i = 0; i < 65; i++ ) {
		Spans[i].Data = (cbytes)&iByte;
		Spans[i].Size = 1;
	}
	ReadSpan.Data = (bytes)&iByte;
	ReadSpan.Size = (size_t)INT_MAX + 1u;
	WriteSpan.Data = (cbytes)&iByte;
	WriteSpan.Size = (size_t)INT_MAX + 1u;

	testRequire(!xrtNetPortAccept(pPort, Datagram, 1, NULL) &&
		!xrtNetPortAccept(pPort, Stream, 0, NULL) &&
		!xrtNetPortConnect(pPort, Stream, NULL, 2, NULL) &&
		!xrtNetPortRecv(pPort, Stream, NULL, 1, 3, NULL) &&
		!xrtNetPortRecv(pPort, Stream, &iByte, 0, 4, NULL) &&
		!xrtNetPortSend(pPort, Stream, NULL, 1, 5, NULL) &&
		!xrtNetPortSendVec(pPort, Stream, Spans, 65, 6, NULL) &&
		!xrtNetPortRecvVec(pPort, Stream, &ReadSpan, 1, 7, NULL) &&
		!xrtNetPortSendVec(pPort, Stream, &WriteSpan, 1, 8, NULL),
		"IOCP invalid completion submission was accepted");
	testRequire(!xrtNetPortRecvMsg(
		pPort,
		Datagram,
		&iByte,
		1,
		81,
		NULL
	), "IOCP recv-message accepted metadata-disabled socket");
	testRequire(
		((xrtNetPortCapabilities(pPort) &
		 XNET_PORT_CAP_DGRAM_ERROR) == 0) &&
		!xrtNetPortRecvError(pPort, Datagram, &iByte, 1, 82, NULL) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED),
		"IOCP accepted unsupported datagram error receive"
	);
	xrtClearError();
	testRequire(xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV6, 7) &&
		!xrtNetPortSendTo(pPort, Datagram,
			&iByte, 1, &Address, 9, NULL),
		"IOCP mismatched datagram family was accepted");
	testRequire(!xrtNetPortWatch(pPort, Datagram,
		10, XNET_POLL_READ, NULL) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_PORT_WATCH),
		"IOCP readiness watch was accepted");
	xrtClearError();

	testRequire(xrtNetSocketClose(Datagram) &&
		xrtNetSocketClose(Stream) &&
		xrtNetPortDestroy(pPort), "IOCP invalid cleanup failed");
}



/* Socket 的原生 IOCP 归属不可改绑，第二个端口必须确定性拒绝。 */
static void testIOCPOwner(void)
{
	xnetportconfig Config;
	xnetportevent Event;
	xnetport* pFirst;
	xnetport* pSecond;
	xnetsocket Socket;
	xnetaddr Address;
	char iByte = 0;

	xrtNetPortConfigInit(&Config);
	Config.Backend = XNET_PORT_IOCP;
	pFirst = xrtNetPortCreate(&Config);
	pSecond = xrtNetPortCreate(&Config);
	Socket = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM, XNET_SOCKET_NONBLOCK);
	testRequire((pFirst != NULL) && (pSecond != NULL) && (Socket != NULL),
		"IOCP owner setup failed");
	testRequire(xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0) &&
		xrtNetSocketBind(Socket, &Address), "IOCP owner bind failed");
	testRequire(xrtNetPortRecvFrom(pFirst, Socket,
		&iByte, 1, 31, NULL) && xrtNetPortCancel(pFirst, 31),
		"IOCP owner first submit failed");
	testIOCPWait(pFirst, &Event, 1);
	testRequire((Event.Id == 31) &&
		(Event.Result == XNET_RESULT_CANCELLED),
		"IOCP owner cancellation mismatch");
	testRequire(!xrtNetPortRecvFrom(pSecond, Socket,
		&iByte, 1, 32, NULL) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_EXISTS) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_PORT_SUBMIT),
		"Socket was rebound to a second IOCP");
	xrtClearError();

	testRequire(xrtNetSocketClose(Socket) &&
		xrtNetPortDestroy(pSecond) &&
		xrtNetPortDestroy(pFirst), "IOCP owner cleanup failed");
}



/* 已销毁端口的上下文地址即使被复用，旧 Socket 也不能伪装成新端口成员。 */
static void testIOCPOwnerAfterDestroy(void)
{
	xnetportconfig Config;
	xnetportevent Event;
	xnetport* pFirst;
	xnetport* pSecond;
	xnetsocket Socket;
	xnetaddr Address;
	char iByte = 0;

	xrtNetPortConfigInit(&Config);
	Config.Backend = XNET_PORT_IOCP;
	pFirst = xrtNetPortCreate(&Config);
	Socket = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM, XNET_SOCKET_NONBLOCK);
	testRequire((pFirst != NULL) && (Socket != NULL),
		"IOCP retired owner setup failed");
	testRequire(xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0) &&
		xrtNetSocketBind(Socket, &Address) &&
		xrtNetPortRecvFrom(pFirst, Socket, &iByte, 1, 51, NULL) &&
		xrtNetPortCancel(pFirst, 51),
		"IOCP retired owner first submit failed");
	testIOCPWait(pFirst, &Event, 1);
	testRequire((Event.Id == 51) &&
		(Event.Result == XNET_RESULT_CANCELLED),
		"IOCP retired owner cancellation mismatch");
	testRequire(xrtNetPortDestroy(pFirst),
		"IOCP retired owner first destroy failed");

	pSecond = xrtNetPortCreate(&Config);
	testRequire(pSecond != NULL,
		"IOCP retired owner second create failed");
	testRequire(!xrtNetPortRecvFrom(pSecond, Socket,
		&iByte, 1, 52, NULL) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_EXISTS) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_PORT_SUBMIT),
		"Socket escaped its retired IOCP owner");
	xrtClearError();

	testRequire(xrtNetSocketClose(Socket) &&
		xrtNetPortDestroy(pSecond),
		"IOCP retired owner cleanup failed");
}



/* 销毁必须同步排空在途 IO，返回后栈缓冲可以立即复用。 */
static void testIOCPDestroyActive(void)
{
	xnetportconfig Config;
	xnetport* pPort;
	xnetsocket Socket;
	xnetaddr Address;
	char Data[8] = { 0 };

	xrtNetPortConfigInit(&Config);
	Config.Backend = XNET_PORT_IOCP;
	pPort = xrtNetPortCreate(&Config);
	Socket = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM, XNET_SOCKET_NONBLOCK);
	testRequire((pPort != NULL) && (Socket != NULL),
		"IOCP active destroy setup failed");
	testRequire(xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0) &&
		xrtNetSocketBind(Socket, &Address),
		"IOCP active destroy bind failed");
	testRequire(xrtNetPortRecvFrom(pPort, Socket,
		Data, sizeof(Data), 41, NULL),
		"IOCP active destroy receive submit failed");
	testRequire(xrtNetPortDestroy(pPort),
		"IOCP active destroy did not quiesce");
	memset(Data, 0xA5, sizeof(Data));
	testRequire(xrtNetSocketClose(Socket),
		"IOCP active destroy socket close failed");
}



/* 执行 Windows 完成式网络端口完整基础回归。 */
int main(void)
{
	xnetportconfig Config;
	xnetportevent Event;
	xnetport* pPort;
	uint32 iCapabilities;
	size_t iCount = 0;

	xrtNetPortConfigInit(&Config);
	Config.Backend = XNET_PORT_IOCP;
	pPort = xrtNetPortCreate(&Config);
	testRequire(pPort != NULL, "creating IOCP port failed");
	iCapabilities = xrtNetPortCapabilities(pPort);
	testRequire((xrtNetPortBackend(pPort) == XNET_PORT_IOCP) &&
		(strcmp(xrtNetPortName(pPort), "iocp") == 0) &&
		((iCapabilities & XNET_PORT_CAP_COMPLETION) != 0) &&
		((iCapabilities & XNET_PORT_CAP_CANCEL) != 0) &&
		((iCapabilities & XNET_PORT_CAP_READ_PROBE) != 0) &&
		((iCapabilities & XNET_PORT_CAP_READINESS) == 0),
		"IOCP identity or capabilities mismatch");
	testRequire(xrtNetPortWake(pPort) &&
		xrtNetPortWake(pPort) && xrtNetPortWake(pPort),
		"IOCP coalesced wake failed");
	testRequire((xrtNetPortWait(pPort, &Event, 1,
		xrtDeadlineAfter(1000000), &iCount) == XNET_RESULT_OK) &&
		(iCount == 1) && (Event.Type == XNET_PORT_EVENT_WAKE),
		"IOCP wake event mismatch");
	testRequire(xrtNetPortDestroy(pPort),
		"destroying identity IOCP port failed");

	testIOCPTCP();
	testIOCPUDP();
	testIOCPCancel();
	testIOCPInvalid();
	testIOCPOwner();
	testIOCPOwnerAfterDestroy();
	testIOCPDestroyActive();
	return 0;
}

#else

/* 非 Windows 平台必须明确拒绝 IOCP，不能静默降级到其他后端。 */
int main(void)
{
	xnetportconfig Config;

	xrtNetPortConfigInit(&Config);
	Config.Backend = XNET_PORT_IOCP;
	testRequire(xrtNetPortCreate(&Config) == NULL,
		"non-Windows platform created an IOCP port");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_PORT_CREATE),
		"unavailable IOCP error mismatch");
	return 0;
}

#endif
