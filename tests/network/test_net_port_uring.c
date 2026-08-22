#include "../test.h"
#include <errno.h>
#include <limits.h>



#if defined(__linux__)

/* 等待固定数量的无序终态，并由一份绝对截止时间约束整轮等待。 */
static void testUringWait(
	xnetport* pPort,
	xnetportevent* pEvents,
	size_t iExpected
)
{
	xdeadline Deadline = xrtDeadlineAfter(5000000u);
	size_t iCount = 0;

	while ( iCount < iExpected ) {
		size_t iReady = 0;

		testRequire(
			xrtNetPortWait(
				pPort,
				pEvents + iCount,
				iExpected - iCount,
				Deadline,
				&iReady
			) == XNET_RESULT_OK,
			"io_uring wait failed"
		);
		testRequire(iReady != 0, "io_uring wait made no progress");
		iCount += iReady;
	}
}



/* 按操作 ID 查找批量等待中无序到达的完成。 */
static const xnetportevent* testUringEvent(
	const xnetportevent* pEvents,
	size_t iCount,
	uint64 Id
)
{
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( pEvents[i].Id == Id ) {
			return &pEvents[i];
		}
	}
	return NULL;
}



/* 创建回环 TCP 监听 Socket，并返回内核选择的实际端口。 */
static xnetsocket testUringListener(xnetaddr* pAddress)
{
	xnetsocket Listener = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM,
		XNET_SOCKET_NONBLOCK
	);

	testRequire(Listener != NULL, "io_uring listener open failed");
	testRequire(
		xrtNetAddrLoopback(pAddress, XNET_FAMILY_IPV4, 0) &&
		xrtNetSocketBind(Listener, pAddress) &&
		xrtNetSocketListen(Listener, 16) &&
		xrtNetSocketLocal(Listener, pAddress),
		"io_uring listener setup failed"
	);
	return Listener;
}



/* accept、connect、向量收发、短读和 EOF 共享完成式单终态契约。 */
static void testUringTCP(void)
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
	xnetspan WriteSpans[2];
	char sReceived[5] = { 0 };
	char iProbe = 0;
	size_t iReceived = 0;
	uint64 iNextId = 5;

	xrtNetPortConfigInit(&Config);
	Config.Backend = XNET_PORT_URING;
	pPort = xrtNetPortCreate(&Config);
	Listener = testUringListener(&Address);
	Client = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM,
		XNET_SOCKET_NONBLOCK
	);
	testRequire(
		(pPort != NULL) && (Client != NULL),
		"io_uring TCP setup failed"
	);

	testRequire(
		xrtNetPortAccept(pPort, Listener, 1, Listener) &&
		xrtNetPortConnect(pPort, Client, &Address, 2, Client),
		"io_uring accept/connect submit failed"
	);
	testUringWait(pPort, Events, 2);
	pAccept = testUringEvent(Events, 2, 1);
	pConnect = testUringEvent(Events, 2, 2);
	testRequire(
		(pAccept != NULL) &&
		(pAccept->Type == XNET_PORT_EVENT_ACCEPT) &&
		(pAccept->Result == XNET_RESULT_OK) &&
		(pAccept->Accepted != NULL) &&
		(pAccept->Socket == Listener) &&
		(pAccept->User == Listener) &&
		xrtNetAddrIsLoopback(&pAccept->Address),
		"io_uring accept event mismatch"
	);
	testRequire(
		(pConnect != NULL) &&
		(pConnect->Type == XNET_PORT_EVENT_CONNECT) &&
		(pConnect->Result == XNET_RESULT_OK) &&
		(pConnect->Socket == Client) &&
		(pConnect->User == Client) &&
		xrtNetAddrEqual(&pConnect->Address, &Address),
		"io_uring connect event mismatch"
	);
	Server = pAccept->Accepted;

	/* POLL_ADD 可读探测不能提前消费流数据。 */
	testRequire(
		xrtNetPortReadProbe(pPort, Server, 101, Server) &&
		xrtNetPortSend(pPort, Client, "P", 1, 102, Client),
		"io_uring read probe submit failed"
	);
	testUringWait(pPort, Events, 2);
	testRequire(
		(testUringEvent(Events, 2, 101) != NULL) &&
		(testUringEvent(Events, 2, 101)->Type ==
			XNET_PORT_EVENT_READ_PROBE) &&
		(testUringEvent(Events, 2, 101)->Result == XNET_RESULT_OK) &&
		((testUringEvent(Events, 2, 101)->Flags &
			XNET_PORT_EVENT_READ) != 0) &&
		(testUringEvent(Events, 2, 101)->Bytes == 0),
		"io_uring read probe terminal mismatch"
	);
	testRequire(
		xrtNetPortRecv(pPort, Server, &iProbe, 1, 103, &iProbe),
		"io_uring receive after probe failed"
	);
	testUringWait(pPort, Events, 1);
	testRequire(
		(Events[0].Id == 103) &&
		(Events[0].Type == XNET_PORT_EVENT_RECV) &&
		(Events[0].Result == XNET_RESULT_OK) &&
		(Events[0].Bytes == 1) && (iProbe == 'P'),
		"io_uring read probe consumed stream data"
	);

	ReadSpans[0].Data = (bytes)sReceived;
	ReadSpans[0].Size = 2;
	ReadSpans[1].Data = (bytes)sReceived + 2;
	ReadSpans[1].Size = 3;
	WriteSpans[0].Data = (cbytes)"he";
	WriteSpans[0].Size = 2;
	WriteSpans[1].Data = (cbytes)"llo";
	WriteSpans[1].Size = 3;
	testRequire(
		xrtNetPortRecvVec(pPort, Server, ReadSpans, 2, 3, sReceived) &&
		xrtNetPortSendVec(pPort, Client, WriteSpans, 2, 4, WriteSpans),
		"io_uring TCP vector submit failed"
	);
	testUringWait(pPort, Events, 2);
	testRequire(
		(testUringEvent(Events, 2, 4) != NULL) &&
		(testUringEvent(Events, 2, 4)->Result == XNET_RESULT_OK) &&
		(testUringEvent(Events, 2, 4)->Bytes == sizeof(sReceived)),
		"io_uring TCP send completion mismatch"
	);
	testRequire(
		(testUringEvent(Events, 2, 3) != NULL) &&
		(testUringEvent(Events, 2, 3)->Result == XNET_RESULT_OK) &&
		(testUringEvent(Events, 2, 3)->Bytes != 0),
		"io_uring TCP receive completion mismatch"
	);
	iReceived = testUringEvent(Events, 2, 3)->Bytes;

	while ( iReceived < sizeof(sReceived) ) {
		testRequire(
			xrtNetPortRecv(
				pPort,
				Server,
				sReceived + iReceived,
				sizeof(sReceived) - iReceived,
				iNextId,
				NULL
			),
			"io_uring TCP remainder submit failed"
		);
		testUringWait(pPort, Events, 1);
		testRequire(
			(Events[0].Id == iNextId) &&
			(Events[0].Result == XNET_RESULT_OK) &&
			(Events[0].Bytes != 0),
			"io_uring TCP remainder completion mismatch"
		);
		iReceived += Events[0].Bytes;
		iNextId++;
	}
	testRequire(
		memcmp(sReceived, "hello", sizeof(sReceived)) == 0,
		"io_uring TCP payload mismatch"
	);

	testRequire(
		xrtNetSocketShutdown(Client, XNET_SHUTDOWN_WRITE) &&
		xrtNetPortRecv(
			pPort,
			Server,
			sReceived,
			sizeof(sReceived),
			iNextId,
			NULL
		),
		"io_uring TCP EOF submit failed"
	);
	testUringWait(pPort, Events, 1);
	testRequire(
		(Events[0].Id == iNextId) &&
		(Events[0].Result == XNET_RESULT_CLOSED) &&
		(Events[0].Bytes == 0) &&
		((Events[0].Flags & XNET_PORT_EVENT_EOF) != 0),
		"io_uring TCP EOF mismatch"
	);

	testRequire(
		xrtNetSocketClose(Server) &&
		xrtNetSocketClose(Client) &&
		xrtNetSocketClose(Listener) &&
		xrtNetPortDestroy(pPort),
		"io_uring TCP cleanup failed"
	);
}



/* UDP 覆盖向量、零长度、截断和已连接数据报，不依赖固定接收区。 */
static void testUringUDP(void)
{
	xnetportconfig Config;
	xnetportevent Events[2];
	const xnetportevent* pReceive;
	const xnetportevent* pSend;
	xnetport* pPort;
	xnetsocket Server;
	xnetsocket Client;
	xnetaddr ServerAddress;
	xnetaddr ClientAddress;
	xnetwspan ReadSpans[2];
	xnetspan WriteSpans[2];
	xnetdgramcontrol Control;
	char sData[5] = { 0 };
	char sSmall[2] = { 0 };

	xrtNetPortConfigInit(&Config);
	Config.Backend = XNET_PORT_URING;
	pPort = xrtNetPortCreate(&Config);
	Server = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM,
		XNET_SOCKET_NONBLOCK
	);
	Client = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM,
		XNET_SOCKET_NONBLOCK
	);
	testRequire(
		(pPort != NULL) && (Server != NULL) && (Client != NULL),
		"io_uring UDP setup failed"
	);
	testRequire(
		xrtNetAddrLoopback(&ServerAddress, XNET_FAMILY_IPV4, 0) &&
		xrtNetAddrLoopback(&ClientAddress, XNET_FAMILY_IPV4, 0) &&
		xrtNetSocketBind(Server, &ServerAddress) &&
		xrtNetSocketBind(Client, &ClientAddress) &&
		xrtNetSocketLocal(Server, &ServerAddress) &&
		xrtNetSocketLocal(Client, &ClientAddress),
		"io_uring UDP bind failed"
	);

	ReadSpans[0].Data = (bytes)sData;
	ReadSpans[0].Size = 2;
	ReadSpans[1].Data = (bytes)sData + 2;
	ReadSpans[1].Size = 3;
	WriteSpans[0].Data = (cbytes)"ab";
	WriteSpans[0].Size = 2;
	WriteSpans[1].Data = (cbytes)"cde";
	WriteSpans[1].Size = 3;
	testRequire(
		xrtNetPortRecvFromVec(pPort, Server, ReadSpans, 2, 11, Server) &&
		xrtNetPortSendToVec(
			pPort,
			Client,
			WriteSpans,
			2,
			&ServerAddress,
			12,
			Client
		),
		"io_uring UDP vector submit failed"
	);
	testUringWait(pPort, Events, 2);
	pReceive = testUringEvent(Events, 2, 11);
	testRequire(
		(pReceive != NULL) &&
		(pReceive->Result == XNET_RESULT_OK) &&
		(pReceive->Bytes == sizeof(sData)) &&
		(pReceive->User == Server) &&
		(memcmp(sData, "abcde", sizeof(sData)) == 0) &&
		(pReceive->Address.Port == ClientAddress.Port),
		"io_uring UDP vector receive mismatch"
	);
	testRequire(
		(testUringEvent(Events, 2, 12) != NULL) &&
		(testUringEvent(Events, 2, 12)->Result == XNET_RESULT_OK) &&
		(testUringEvent(Events, 2, 12)->Bytes == sizeof(sData)),
		"io_uring UDP vector send mismatch"
	);

	/* RECV_MSG 只在显式启用后提交，并把控制消息复制到稳定终态。 */
	testRequire(
		!xrtNetPortRecvMsg(pPort, Server, sData, sizeof(sData), 90, Server) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"io_uring accepted metadata receive before enabling it"
	);
	xrtClearError();
	testRequire(
		xrtNetSocketDgramMetaSet(
			Server,
			XNET_DGRAM_META_DESTINATION | XNET_DGRAM_META_INTERFACE
		) &&
		xrtNetPortRecvMsg(pPort, Server, sData, sizeof(sData), 90, Server) &&
		xrtNetPortSendTo(
			pPort,
			Client,
			"meta",
			4,
			&ServerAddress,
			91,
			Client
		),
		"io_uring UDP metadata submit failed"
	);
	testUringWait(pPort, Events, 2);
	pReceive = testUringEvent(Events, 2, 90);
	testRequire(
		(pReceive != NULL) &&
		(pReceive->Type == XNET_PORT_EVENT_RECV_MSG) &&
		(pReceive->Result == XNET_RESULT_OK) &&
		(pReceive->Bytes == 4) &&
		(memcmp(sData, "meta", 4) == 0) &&
		((pReceive->Meta.Flags &
			(XNET_DGRAM_META_DESTINATION | XNET_DGRAM_META_INTERFACE)) ==
			(XNET_DGRAM_META_DESTINATION | XNET_DGRAM_META_INTERFACE)) &&
		xrtNetAddrIsLoopback(&pReceive->Meta.Destination) &&
		(pReceive->Meta.Interface != 0),
		"io_uring UDP metadata event mismatch"
	);

	/* SEND_MSG 必须让内核持续借用操作内复制的控制值，直到 CQE 到达。 */
	testRequire(
		(xrtNetSocketDgramControlAvailable(Client) &
		 XNET_DGRAM_CONTROL_SOURCE) != 0,
		"io_uring UDP source control is unavailable"
	);
	memset(&Control, 0, sizeof(Control));
	Control.Flags = XNET_DGRAM_CONTROL_SOURCE;
	Control.Source = ClientAddress;
	Control.Source.Port = 0;
	testRequire(
		xrtNetPortRecvFrom(pPort, Server, sData, sizeof(sData), 92, Server) &&
		xrtNetPortSendMsg(
			pPort,
			Client,
			"ctrl",
			4,
			&ServerAddress,
			&Control,
			93,
			Client
		),
		"io_uring UDP controlled send submit failed"
	);
	testUringWait(pPort, Events, 2);
	pReceive = testUringEvent(Events, 2, 92);
	pSend = testUringEvent(Events, 2, 93);
	testRequire(
		(pReceive != NULL) &&
		(pReceive->Result == XNET_RESULT_OK) &&
		(pReceive->Bytes == 4) &&
		(memcmp(sData, "ctrl", 4) == 0) &&
		(pReceive->Address.Port == ClientAddress.Port),
		"io_uring UDP controlled receive mismatch"
	);
	testRequire(
		(pSend != NULL) &&
		(pSend->Type == XNET_PORT_EVENT_SEND_MSG) &&
		(pSend->Result == XNET_RESULT_OK) &&
		(pSend->Bytes == 4) &&
		xrtNetAddrEqual(&pSend->Address, &ServerAddress),
		"io_uring UDP controlled send event mismatch"
	);

	testRequire(
		xrtNetPortRecvFrom(pPort, Server, NULL, 0, 13, NULL) &&
		xrtNetPortSendTo(
			pPort,
			Client,
			NULL,
			0,
			&ServerAddress,
			14,
			NULL
		),
		"io_uring zero datagram submit failed"
	);
	testUringWait(pPort, Events, 2);
	testRequire(
		(testUringEvent(Events, 2, 13) != NULL) &&
		(testUringEvent(Events, 2, 13)->Result == XNET_RESULT_OK) &&
		(testUringEvent(Events, 2, 13)->Bytes == 0) &&
		((testUringEvent(Events, 2, 13)->Flags &
			XNET_PORT_EVENT_EOF) == 0),
		"io_uring zero datagram receive mismatch"
	);

	testRequire(
		xrtNetPortRecvFrom(
			pPort,
			Server,
			sSmall,
			sizeof(sSmall),
			15,
			NULL
		) &&
		xrtNetPortSendTo(
			pPort,
			Client,
			"large",
			5,
			&ServerAddress,
			16,
			NULL
		),
		"io_uring truncated datagram submit failed"
	);
	testUringWait(pPort, Events, 2);
	pReceive = testUringEvent(Events, 2, 15);
	testRequire(
		(pReceive != NULL) &&
		(pReceive->Result == XNET_RESULT_TRUNCATED) &&
		(pReceive->Bytes == sizeof(sSmall)) &&
		((pReceive->Flags & XNET_PORT_EVENT_ERROR) == 0) &&
		(memcmp(sSmall, "la", sizeof(sSmall)) == 0),
		"io_uring truncated datagram mismatch"
	);

	testRequire(
		(xrtNetSocketConnect(Server, &ClientAddress) == XNET_RESULT_OK) &&
		(xrtNetSocketConnect(Client, &ServerAddress) == XNET_RESULT_OK),
		"io_uring connected UDP setup failed"
	);
	testRequire(
		xrtNetPortRecv(pPort, Server, NULL, 0, 17, Server) &&
		xrtNetPortSend(pPort, Client, NULL, 0, 18, Client),
		"io_uring connected zero datagram submit failed"
	);
	testUringWait(pPort, Events, 2);
	testRequire(
		(testUringEvent(Events, 2, 17) != NULL) &&
		(testUringEvent(Events, 2, 17)->Result == XNET_RESULT_OK) &&
		(testUringEvent(Events, 2, 17)->Bytes == 0),
		"io_uring connected zero datagram mismatch"
	);
	memset(sSmall, 0, sizeof(sSmall));
	testRequire(
		xrtNetPortRecv(
			pPort,
			Server,
			sSmall,
			sizeof(sSmall),
			19,
			NULL
		) &&
		xrtNetPortSend(pPort, Client, "large", 5, 20, NULL),
		"io_uring connected truncation submit failed"
	);
	testUringWait(pPort, Events, 2);
	pReceive = testUringEvent(Events, 2, 19);
	testRequire(
		(pReceive != NULL) &&
		(pReceive->Result == XNET_RESULT_TRUNCATED) &&
		(pReceive->Bytes == sizeof(sSmall)) &&
		((pReceive->Flags & XNET_PORT_EVENT_ERROR) == 0) &&
		(memcmp(sSmall, "la", sizeof(sSmall)) == 0),
		"io_uring connected truncation mismatch"
	);
	testRequire(
		xrtNetSocketClose(Client) &&
		xrtNetSocketClose(Server) &&
		xrtNetPortDestroy(pPort),
		"io_uring UDP cleanup failed"
	);
}



/* 错误队列等待必须返回原数据报前缀、ICMP 错误和目标地址。 */
static void testUringDgramError(void)
{
	xnetportconfig Config;
	xnetportevent Event;
	xnetport* pPort;
	xnetsocket Reserved;
	xnetsocket Client;
	xnetaddr Target;
	char sPayload[8] = { 0 };
	size_t iSent = 0;
	size_t iCount = 0;

	xrtNetPortConfigInit(&Config);
	Config.Backend = XNET_PORT_URING;
	pPort = xrtNetPortCreate(&Config);
	Reserved = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM,
		XNET_SOCKET_NONBLOCK
	);
	Client = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM,
		XNET_SOCKET_NONBLOCK
	);
	testRequire(
		(pPort != NULL) && (Reserved != NULL) && (Client != NULL),
		"io_uring datagram error setup failed"
	);
	testRequire(
		xrtNetAddrLoopback(&Target, XNET_FAMILY_IPV4, 0) &&
		xrtNetSocketBind(Reserved, &Target) &&
		xrtNetSocketLocal(Reserved, &Target) &&
		xrtNetSocketClose(Reserved) &&
		xrtNetSocketSet(Client, XNET_OPTION_DGRAM_ERRORS, 1) &&
		xrtNetPortRecvError(
			pPort,
			Client,
			sPayload,
			sizeof(sPayload),
			94,
			Client
		) &&
		(xrtNetSocketSendTo(
			Client,
			"error",
			5,
			&iSent,
			&Target
		) == XNET_RESULT_OK) &&
		(iSent == 5),
		"io_uring datagram error submission failed"
	);
	testRequire(
		(xrtNetPortWait(
			pPort,
			&Event,
			1,
			xrtDeadlineAfter(5000000u),
			&iCount
		) == XNET_RESULT_OK) &&
		(iCount == 1),
		"io_uring datagram error wait failed"
	);
	testRequire(
		(Event.Type == XNET_PORT_EVENT_RECV_ERROR) &&
		(Event.Result == XNET_RESULT_OK) &&
		(Event.Id == 94) &&
		(Event.User == Client) &&
		(Event.Bytes == 5) &&
		(memcmp(sPayload, "error", 5) == 0) &&
		(Event.DgramError.SystemCode == ECONNREFUSED) &&
		(Event.DgramError.Kind == XERR_IO) &&
		(Event.DgramError.Origin == XNET_DGRAM_ERROR_ICMP) &&
		((Event.DgramError.Flags & XNET_DGRAM_ERROR_REMOTE) != 0) &&
		xrtNetAddrEqual(&Event.DgramError.Remote, &Target),
		"io_uring datagram error event mismatch"
	);
	testRequire(
		xrtNetSocketClose(Client) && xrtNetPortDestroy(pPort),
		"io_uring datagram error cleanup failed"
	);
}



/* ID 唯一性、硬操作上限和重复取消形成确定性背压。 */
static void testUringCancel(void)
{
	xnetportconfig Config;
	xnetportevent Events[2];
	xnetport* pPort;
	xnetsocket Listener;
	xnetaddr Address;

	xrtNetPortConfigInit(&Config);
	Config.Backend = XNET_PORT_URING;
	Config.OperationLimit = 2;
	pPort = xrtNetPortCreate(&Config);
	Listener = testUringListener(&Address);
	testRequire(pPort != NULL, "io_uring cancel setup failed");

	testRequire(
		xrtNetPortAccept(pPort, Listener, 21, NULL),
		"io_uring first cancellable accept failed"
	);
	testRequire(
		!xrtNetPortAccept(pPort, Listener, 21, NULL) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_EXISTS),
		"io_uring accepted duplicate operation id"
	);
	xrtClearError();
	testRequire(
		xrtNetPortAccept(pPort, Listener, 22, NULL),
		"io_uring second cancellable accept failed"
	);
	testRequire(
		!xrtNetPortAccept(pPort, Listener, 23, NULL) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_AGAIN),
		"io_uring operation limit was not enforced"
	);
	xrtClearError();

	testRequire(
		xrtNetPortCancel(pPort, 21) &&
		xrtNetPortCancel(pPort, 21) &&
		xrtNetPortCancel(pPort, 22),
		"io_uring cancellation request failed"
	);
	testUringWait(pPort, Events, 2);
	testRequire(
		(testUringEvent(Events, 2, 21) != NULL) &&
		(testUringEvent(Events, 2, 21)->Result ==
			XNET_RESULT_CANCELLED) &&
		(testUringEvent(Events, 2, 22) != NULL) &&
		(testUringEvent(Events, 2, 22)->Result ==
			XNET_RESULT_CANCELLED),
		"io_uring cancellation terminal mismatch"
	);
	testRequire(
		!xrtNetPortCancel(pPort, 99) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND),
		"io_uring missing cancellation id mismatch"
	);
	xrtClearError();

	testRequire(
		xrtNetSocketClose(Listener) &&
		xrtNetPortDestroy(pPort),
		"io_uring cancellation cleanup failed"
	);
}



/* 直接销毁端口必须同步撤销全部在途操作，返回后不再引用 Socket 或缓冲。 */
static void testUringDestroyPending(void)
{
	xnetportconfig Config;
	xnetport* pPort;
	xnetsocket Listener;
	xnetsocket Datagram;
	xnetaddr Address;
	char iByte = 0;

	xrtNetPortConfigInit(&Config);
	Config.Backend = XNET_PORT_URING;
	pPort = xrtNetPortCreate(&Config);
	Listener = testUringListener(&Address);
	Datagram = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM,
		XNET_SOCKET_NONBLOCK
	);
	testRequire(
		(pPort != NULL) && (Datagram != NULL) &&
		xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0) &&
		xrtNetSocketBind(Datagram, &Address),
		"io_uring pending destroy setup failed"
	);
	testRequire(
		xrtNetPortAccept(pPort, Listener, 25, NULL) &&
		xrtNetPortRecvFrom(pPort, Datagram, &iByte, 1, 26, &iByte),
		"io_uring pending destroy submit failed"
	);
	testRequire(
		xrtNetPortDestroy(pPort),
		"io_uring did not quiesce pending operations"
	);

	iByte = 1;
	testRequire(
		xrtNetSocketClose(Datagram) &&
		xrtNetSocketClose(Listener),
		"io_uring pending destroy socket cleanup failed"
	);
}



/* 公共层必须在进入内核前拒绝无法由稳定 UAPI 表达的超长 Span。 */
static void testUringInvalid(void)
{
	xnetportconfig Config;
	xnetport* pPort;
	xnetsocket Socket;
	xnetwspan ReadSpan;
	xnetspan WriteSpan;
	char iByte = 0;

	xrtNetPortConfigInit(&Config);
	Config.Backend = XNET_PORT_URING;
	Config.OperationLimit = 32769u;
	testRequire(
		(xrtNetPortCreate(&Config) == NULL) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"io_uring accepted an operation limit above 32768"
	);
	xrtClearError();

	xrtNetPortConfigInit(&Config);
	Config.Backend = XNET_PORT_URING;
	pPort = xrtNetPortCreate(&Config);
	Socket = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM,
		XNET_SOCKET_NONBLOCK
	);
	testRequire(
		(pPort != NULL) && (Socket != NULL),
		"io_uring invalid setup failed"
	);

	ReadSpan.Data = (bytes)&iByte;
	ReadSpan.Size = (size_t)INT_MAX + 1u;
	WriteSpan.Data = (cbytes)&iByte;
	WriteSpan.Size = (size_t)INT_MAX + 1u;
	testRequire(
		!xrtNetPortRecvVec(
			pPort,
			Socket,
			&ReadSpan,
			1,
			31,
			NULL
		) &&
		!xrtNetPortSendVec(
			pPort,
			Socket,
			&WriteSpan,
			1,
			32,
			NULL
		),
		"io_uring accepted an oversized completion span"
	);

	testRequire(
		xrtNetSocketClose(Socket) &&
		xrtNetPortDestroy(pPort),
		"io_uring invalid cleanup failed"
	);
}



/* 执行 Linux 完成式端口的身份、唤醒与协议边界回归。 */
int main(void)
{
	xnetportconfig Config;
	xnetportevent Event;
	xnetport* pPort;
	uint32 iCapabilities;
	size_t iCount = 0;

	xrtNetPortConfigInit(&Config);
	Config.Backend = XNET_PORT_URING;
	pPort = xrtNetPortCreate(&Config);
	testRequire(pPort != NULL, "creating io_uring port failed");
	iCapabilities = xrtNetPortCapabilities(pPort);
	testRequire(
		(xrtNetPortBackend(pPort) == XNET_PORT_URING) &&
		(strcmp(xrtNetPortName(pPort), "io_uring") == 0) &&
		((iCapabilities & XNET_PORT_CAP_COMPLETION) != 0) &&
		((iCapabilities & XNET_PORT_CAP_CANCEL) != 0) &&
		((iCapabilities & XNET_PORT_CAP_READ_PROBE) != 0) &&
		((iCapabilities & XNET_PORT_CAP_DGRAM_ERROR) != 0) &&
		((iCapabilities & XNET_PORT_CAP_READINESS) == 0),
		"io_uring identity or capabilities mismatch"
	);
	testRequire(
		xrtNetPortWake(pPort) &&
		xrtNetPortWake(pPort) &&
		xrtNetPortWake(pPort),
		"io_uring coalesced wake failed"
	);
	testRequire(
		(xrtNetPortWait(
			pPort,
			&Event,
			1,
			xrtDeadlineAfter(1000000u),
			&iCount
		) == XNET_RESULT_OK) &&
		(iCount == 1) &&
		(Event.Type == XNET_PORT_EVENT_WAKE),
		"io_uring wake event mismatch"
	);
	testRequire(
		!xrtNetPortWatch(pPort, NULL, 1, XNET_POLL_READ, NULL),
		"io_uring accepted readiness watch"
	);
	xrtClearError();
	testRequire(
		xrtNetPortDestroy(pPort),
		"destroying identity io_uring port failed"
	);

	testUringTCP();
	testUringUDP();
	testUringDgramError();
	testUringCancel();
	testUringDestroyPending();
	testUringInvalid();
	return 0;
}

#else

/* 非 Linux 平台必须明确拒绝 io_uring，不能静默降级。 */
int main(void)
{
	xnetportconfig Config;

	xrtNetPortConfigInit(&Config);
	Config.Backend = XNET_PORT_URING;
	testRequire(
		xrtNetPortCreate(&Config) == NULL,
		"non-Linux platform created an io_uring port"
	);
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_PORT_CREATE),
		"unavailable io_uring error mismatch"
	);
	return 0;
}

#endif
