#include "../test.h"

#if defined(_WIN32) || defined(_WIN64)
	#if defined(__TINYC__)
		#include <winapi/windows.h>
	#else
		#include <windows.h>
	#endif
#else
	#include <fcntl.h>
	#if defined(__linux__)
		#include <errno.h>
		#include <poll.h>
	#endif
#endif



/* 验证 XRT 拥有的原生 Socket 不会泄漏给后续创建的子进程。 */
static bool testSocketNoInherit(xnetsocket Socket)
{
	intptr_t iNative = xrtNetSocketNative(Socket);

	#if defined(_WIN32) || defined(_WIN64)
		DWORD iFlags = 0;

		return (iNative != (intptr_t)-1) &&
			(GetHandleInformation((HANDLE)(uintptr_t)iNative, &iFlags) != 0) &&
			((iFlags & HANDLE_FLAG_INHERIT) == 0);
	#else
		int iFlags;

		if ( iNative == (intptr_t)-1 ) {
			return false;
		}
		iFlags = fcntl((int)iNative, F_GETFD, 0);
		return (iFlags >= 0) && ((iFlags & FD_CLOEXEC) != 0);
	#endif
}



/* 循环完成阻塞流式发送，测试本身不假定一次系统调用完整写入。 */
static void testSocketSendFull(xnetsocket Socket,
	const void* pData, size_t iSize)
{
	const unsigned char* pBytes = (const unsigned char*)pData;
	size_t iOffset = 0;

	while ( iOffset < iSize ) {
		size_t iSent = 0;

		testRequire(xrtNetSocketSend(Socket, pBytes + iOffset,
			iSize - iOffset, &iSent) == XNET_RESULT_OK,
			"blocking TCP send failed");
		testRequire(iSent != 0, "blocking TCP send made no progress");
		iOffset += iSent;
	}
}



/* 循环完成阻塞流式接收，测试本身不假定一次系统调用完整读取。 */
static void testSocketRecvFull(xnetsocket Socket,
	void* pData, size_t iSize)
{
	unsigned char* pBytes = (unsigned char*)pData;
	size_t iOffset = 0;

	while ( iOffset < iSize ) {
		size_t iReceived = 0;

		testRequire(xrtNetSocketRecv(Socket, pBytes + iOffset,
			iSize - iOffset, &iReceived) == XNET_RESULT_OK,
			"blocking TCP receive failed");
		testRequire(iReceived != 0,
			"blocking TCP receive made no progress");
		iOffset += iReceived;
	}
}



/* 建立一个绑定到 IPv4 回环地址和动态端口的 Socket。 */
static xnetsocket testSocketBound(xnetsockettype Type, xnetaddr* pAddress)
{
	xnetsocket Socket;
	xnetaddr Address;

	Socket = xrtNetSocketOpen(XNET_FAMILY_IPV4, Type, 0);
	testRequire(Socket != NULL, "opening loopback socket failed");
	testRequire(xrtNetSocketSet(Socket,
		XNET_OPTION_REUSE_ADDRESS, 1), "setting reuse-address failed");
	testRequire(xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0),
		"building loopback address failed");
	testRequire(xrtNetSocketBind(Socket, &Address),
		"binding loopback socket failed");
	testRequire(xrtNetSocketLocal(Socket, &Address) &&
		(Address.Port != 0), "querying bound socket address failed");
	if ( pAddress != NULL ) {
		*pAddress = Address;
	}
	return Socket;
}



/* TCP 原语必须覆盖监听、连接、地址、向量 IO、短 IO 和半关闭。 */
static void testSocketTCP(void)
{
	xnetsocket Listener;
	xnetsocket Client;
	xnetsocket Accepted = NULL;
	xnetaddr ListenAddress;
	xnetaddr ClientLocal;
	xnetaddr ClientRemote;
	xnetaddr AcceptedLocal;
	xnetaddr AcceptedRemote;
	xnetspan SendVec[3];
	xnetwspan RecvVec[3];
	static const char sPayload[] = "vec-data";
	char aLeft[4] = { 0 };
	char aMiddle[2] = { 0 };
	char aRight[5] = { 0 };
	char sActual[8] = { 0 };
	char sReply[8] = { 0 };
	size_t iSize;
	int64 iOption;

	Listener = testSocketBound(XNET_SOCKET_STREAM, &ListenAddress);
	testRequire(testSocketNoInherit(Listener),
		"TCP listener socket is inheritable");
	testRequire(xrtNetSocketListen(Listener, 16),
		"starting TCP listener failed");

	Client = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM, 0);
	testRequire(Client != NULL, "opening TCP client failed");
	testRequire(testSocketNoInherit(Client),
		"TCP client socket is inheritable");
	testRequire(xrtNetSocketSet(Client, XNET_OPTION_NO_DELAY, 1) &&
		xrtNetSocketGet(Client, XNET_OPTION_NO_DELAY, &iOption) &&
		(iOption != 0), "TCP no-delay option mismatch");
	testRequire(xrtNetSocketConnect(Client, &ListenAddress) ==
		XNET_RESULT_OK, "blocking TCP connect failed");
	testRequire(xrtNetSocketAccept(Listener, &Accepted,
		&AcceptedRemote) == XNET_RESULT_OK,
		"blocking TCP accept failed");
	testRequire(Accepted != NULL, "accepted TCP socket is null");
	testRequire(testSocketNoInherit(Accepted),
		"accepted TCP socket is inheritable");

	testRequire(xrtNetSocketLocal(Client, &ClientLocal) &&
		xrtNetSocketRemote(Client, &ClientRemote) &&
		xrtNetSocketLocal(Accepted, &AcceptedLocal),
		"TCP endpoint query failed");
	testRequire(xrtNetAddrEqual(&ClientLocal, &AcceptedRemote) &&
		xrtNetAddrEqual(&ClientRemote, &AcceptedLocal) &&
		xrtNetAddrEqual(&ClientRemote, &ListenAddress),
		"TCP endpoint symmetry mismatch");
	testRequire((xrtNetSocketFamily(Client) == XNET_FAMILY_IPV4) &&
		(xrtNetSocketType(Client) == XNET_SOCKET_STREAM) &&
		(xrtNetSocketNative(Client) != (intptr_t)-1),
		"TCP socket identity mismatch");

	SendVec[0] = (xnetspan){ (cbytes)"vec", 3 };
	SendVec[1] = (xnetspan){ (cbytes)"-", 1 };
	SendVec[2] = (xnetspan){ (cbytes)"data", 4 };
	testRequire((xrtNetSocketSendVec(Client, SendVec, 3, &iSize) ==
		XNET_RESULT_OK) && (iSize > 0) && (iSize <= 8),
		"TCP vector send failed");
	testSocketSendFull(Client, sPayload + iSize, 8 - iSize);

	RecvVec[0] = (xnetwspan){ (bytes)aLeft, 3 };
	RecvVec[1] = (xnetwspan){ (bytes)aMiddle, 1 };
	RecvVec[2] = (xnetwspan){ (bytes)aRight, 4 };
	testRequire((xrtNetSocketRecvVec(Accepted, RecvVec, 3, &iSize) ==
		XNET_RESULT_OK) && (iSize > 0) && (iSize <= 8),
		"TCP vector receive failed");
	memcpy(sActual, aLeft, 3);
	memcpy(sActual + 3, aMiddle, 1);
	memcpy(sActual + 4, aRight, 4);
	testSocketRecvFull(Accepted, sActual + iSize, 8 - iSize);
	testRequire(memcmp(sActual, sPayload, 8) == 0,
		"TCP vector payload mismatch");

	testSocketSendFull(Accepted, "reply", 5);
	testSocketRecvFull(Client, sReply, 5);
	testRequire(memcmp(sReply, "reply", 5) == 0,
		"TCP scalar payload mismatch");

	testRequire(xrtNetSocketShutdown(Client, XNET_SHUTDOWN_WRITE),
		"TCP write shutdown failed");
	testRequire(xrtNetSocketRecv(Accepted,
		sReply, sizeof(sReply), &iSize) == XNET_RESULT_CLOSED,
		"TCP orderly EOF was not reported as closed");

	testRequire(xrtNetSocketClose(Accepted),
		"closing accepted TCP socket failed");
	testRequire(xrtNetSocketClose(Client),
		"closing TCP client failed");
	testRequire(xrtNetSocketClose(Listener),
		"closing TCP listener failed");
}



/* 循环接收指定数量的批量项，兼容单次系统调用和可移植阻塞回退。 */
static void testSocketRecvBatchItems(xnetsocket Socket,
	xnetdgramrecv* pItems, size_t iCount, cstr sMessage)
{
	size_t iReceived = 0;

	while ( iReceived < iCount ) {
		size_t iBatch = 0;
		xnetresult Result = xrtNetSocketRecvBatch(Socket,
			pItems + iReceived, iCount - iReceived, &iBatch);

		testRequire((Result == XNET_RESULT_OK) && (iBatch != 0), sMessage);
		iReceived += iBatch;
	}
}



/* 数据报元数据必须覆盖同步、批量、能力查询和关闭后的空结果。 */
static void testSocketUdpMeta(void)
{
	const uint32 iPacketInfo = XNET_DGRAM_META_DESTINATION |
		XNET_DGRAM_META_INTERFACE;
	xnetsocket Server;
	xnetsocket Client;
	xnetaddr ServerAddress;
	xnetaddr Remote;
	xnetdgrammeta Meta;
	xnetdgramrecv Item;
	uint32 iAvailable;
	uint32 iMetadata;
	char sData[8] = { 0 };
	size_t iSize = 0;

	Server = testSocketBound(XNET_SOCKET_DGRAM, &ServerAddress);
	Client = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_DGRAM, 0);
	testRequire((Server != NULL) && (Client != NULL),
		"UDP metadata socket setup failed");
	iAvailable = xrtNetSocketDgramMetaAvailable(Server);
	iMetadata = iAvailable & ~XNET_DGRAM_META_SEGMENT_SIZE;
	testRequire((iAvailable & iPacketInfo) == iPacketInfo,
		"IPv4 packet metadata is unavailable");
	testRequire((xrtNetSocketDgramMetaEnabled(Server) == 0) &&
		xrtNetSocketDgramMetaSet(Server, iMetadata) &&
		(xrtNetSocketDgramMetaEnabled(Server) == iMetadata),
		"enabling UDP metadata failed");

	testRequire((xrtNetSocketSendTo(
		Client,
		"meta",
		4,
		&iSize,
		&ServerAddress
	) == XNET_RESULT_OK) && (iSize == 4),
		"sending UDP metadata probe failed");
	testRequire((xrtNetSocketRecvMsg(
		Server,
		sData,
		sizeof(sData),
		&iSize,
		&Remote,
		&Meta
	) == XNET_RESULT_OK) && (iSize == 4) &&
		(memcmp(sData, "meta", 4) == 0),
		"receiving UDP metadata failed");
	testRequire(((Meta.Flags & iPacketInfo) == iPacketInfo) &&
		((Meta.Flags & XNET_DGRAM_META_TRUNCATED) == 0) &&
		(Meta.Destination.Family == XNET_FAMILY_IPV4) &&
		xrtNetAddrIsLoopback(&Meta.Destination) &&
		(Meta.Destination.Port == 0) && (Meta.Interface != 0),
		"UDP metadata fields mismatch");
	if ( (iMetadata & XNET_DGRAM_META_HOP_LIMIT) != 0 ) {
		testRequire(((Meta.Flags & XNET_DGRAM_META_HOP_LIMIT) != 0) &&
			(Meta.HopLimit > 0) && (Meta.HopLimit <= 255),
			"UDP hop-limit metadata mismatch");
	}
	if ( (iMetadata & XNET_DGRAM_META_TRAFFIC_CLASS) != 0 ) {
		testRequire(((Meta.Flags & XNET_DGRAM_META_TRAFFIC_CLASS) != 0) &&
			(Meta.TrafficClass >= 0) && (Meta.TrafficClass <= 255),
			"UDP traffic-class metadata mismatch");
	}

	memset(sData, 0, sizeof(sData));
	testRequire((xrtNetSocketSendTo(
		Client,
		"batch",
		5,
		&iSize,
		&ServerAddress
	) == XNET_RESULT_OK) && (iSize == 5),
		"sending UDP metadata batch probe failed");
	Item = (xnetdgramrecv){ .Data = sData, .Capacity = sizeof(sData) };
	testSocketRecvBatchItems(
		Server,
		&Item,
		1,
		"receiving UDP metadata batch failed"
	);
	testRequire((Item.Size == 5) && (memcmp(sData, "batch", 5) == 0) &&
		((Item.Meta.Flags & iMetadata) == iMetadata) &&
		xrtNetAddrIsLoopback(&Item.Meta.Destination) &&
		(Item.Meta.Interface != 0),
		"UDP metadata batch fields mismatch");

	testRequire(xrtNetSocketDgramMetaSet(Server, 0) &&
		(xrtNetSocketDgramMetaEnabled(Server) == 0),
		"disabling UDP metadata failed");
	testRequire((xrtNetSocketSendTo(
		Client,
		"plain",
		5,
		&iSize,
		&ServerAddress
	) == XNET_RESULT_OK) &&
		(xrtNetSocketRecvMsg(
			Server,
			sData,
			sizeof(sData),
			&iSize,
			&Remote,
			&Meta
		) == XNET_RESULT_OK) && (Meta.Flags == 0),
		"disabled UDP metadata was not empty");

	testRequire(xrtNetSocketClose(Client) && xrtNetSocketClose(Server),
		"closing UDP metadata sockets failed");
}



/* 逐数据报发送控制必须覆盖能力查询、连续/向量发送和连接式目标。 */
static void testSocketUdpControl(void)
{
	xnetsocket Server;
	xnetsocket Client;
	xnetaddr ServerAddress;
	xnetaddr ClientAddress;
	xnetaddr Remote;
	xnetdgramcontrol Control;
	xnetspan Spans[2];
	uint32 iAvailable;
	char sData[8] = { 0 };
	size_t iSize = 0;

	Server = testSocketBound(XNET_SOCKET_DGRAM, &ServerAddress);
	Client = testSocketBound(XNET_SOCKET_DGRAM, &ClientAddress);
	iAvailable = xrtNetSocketDgramControlAvailable(Client);
	testRequire((iAvailable & XNET_DGRAM_CONTROL_SOURCE) != 0,
		"IPv4 datagram source control is unavailable");

	memset(&Control, 0, sizeof(Control));
	Control.Flags = XNET_DGRAM_CONTROL_SOURCE;
	Control.Source = ClientAddress;
	Control.Source.Port = 0;
	testRequire((xrtNetSocketSendMsg(
		Client,
		"control",
		7,
		&iSize,
		&ServerAddress,
		&Control
	) == XNET_RESULT_OK) && (iSize == 7),
		"UDP controlled scalar send failed");
	testRequire((xrtNetSocketRecvFrom(
		Server,
		sData,
		sizeof(sData),
		&iSize,
		&Remote
	) == XNET_RESULT_OK) && (iSize == 7) &&
		(memcmp(sData, "control", 7) == 0) &&
		(Remote.Port == ClientAddress.Port) &&
		xrtNetAddrIsLoopback(&Remote),
		"UDP controlled scalar receive mismatch");

	testRequire(xrtNetSocketConnect(Client, &ServerAddress) ==
		XNET_RESULT_OK, "connecting controlled UDP failed");
	Spans[0] = (xnetspan){ (cbytes)"ve", 2 };
	Spans[1] = (xnetspan){ (cbytes)"c", 1 };
	testRequire((xrtNetSocketSendMsgVec(
		Client,
		Spans,
		2,
		&iSize,
		NULL,
		&Control
	) == XNET_RESULT_OK) && (iSize == 3),
		"connected UDP controlled vector send failed");
	testRequire((xrtNetSocketRecvFrom(
		Server,
		sData,
		sizeof(sData),
		&iSize,
		&Remote
	) == XNET_RESULT_OK) && (iSize == 3) &&
		(memcmp(sData, "vec", 3) == 0),
		"connected UDP controlled vector receive mismatch");

	testRequire(xrtNetSocketClose(Client) && xrtNetSocketClose(Server),
		"closing UDP control sockets failed");
}



/* PMTU 能力必须可查询、模式可往返，错误队列为空时不得伪造失败。 */
static void testSocketUdpPathMtu(void)
{
	xnetsocket Server;
	xnetsocket Client;
	xnetaddr ServerAddress;
	xnetdgramerror Error;
	uint32 iCapabilities;
	int64 iValue = -1;
	size_t iSize = 77;

	Server = testSocketBound(XNET_SOCKET_DGRAM, &ServerAddress);
	Client = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM,
		XNET_SOCKET_NONBLOCK
	);
	testRequire((Server != NULL) && (Client != NULL),
		"UDP PMTU socket setup failed");
	iCapabilities = xrtNetSocketDgramCapabilities(Client);
	if ( (iCapabilities & XNET_DGRAM_CAP_PATH_MTU_MODE) != 0 ) {
		testRequire(xrtNetSocketGet(
			Client,
			XNET_OPTION_PATH_MTU_MODE,
			&iValue
		) && (iValue == XNET_PMTU_SYSTEM),
			"default UDP PMTU mode mismatch");
		for ( int i = XNET_PMTU_DISCOVER;
			i <= XNET_PMTU_PROBE;
			i++ ) {
			testRequire(xrtNetSocketSet(
				Client,
				XNET_OPTION_PATH_MTU_MODE,
				i
			) && xrtNetSocketGet(
				Client,
				XNET_OPTION_PATH_MTU_MODE,
				&iValue
			) && (iValue == i), "UDP PMTU mode round-trip failed");
		}
		testRequire(xrtNetSocketSet(
			Client,
			XNET_OPTION_PATH_MTU_MODE,
			XNET_PMTU_SYSTEM
		), "restoring UDP PMTU mode failed");
	}

	testRequire(xrtNetSocketConnect(Client, &ServerAddress) ==
		XNET_RESULT_OK, "connecting UDP PMTU socket failed");
	if ( (iCapabilities & XNET_DGRAM_CAP_PATH_MTU_QUERY) != 0 ) {
		testRequire(xrtNetSocketGet(
			Client,
			XNET_OPTION_PATH_MTU,
			&iValue
		) && (iValue > 0), "connected UDP path MTU query failed");
	}
	if ( (iCapabilities & XNET_DGRAM_CAP_ERROR_QUEUE) != 0 ) {
		testRequire(xrtNetSocketSet(
			Client,
			XNET_OPTION_DGRAM_ERRORS,
			1
		) && xrtNetSocketGet(
			Client,
			XNET_OPTION_DGRAM_ERRORS,
			&iValue
		) && (iValue != 0), "enabling UDP error queue failed");
		memset(&Error, 0xA5, sizeof(Error));
		testRequire((xrtNetSocketDgramRecvError(
			Client,
			NULL,
			0,
			&iSize,
			&Error
		) == XNET_RESULT_AGAIN) && (iSize == 0) &&
			(Error.Flags == 0) && (Error.SystemCode == 0),
			"empty UDP error queue contract mismatch");
	}

	testRequire(xrtNetSocketClose(Client) && xrtNetSocketClose(Server),
		"closing UDP PMTU sockets failed");
}



/* GSO/GRO 必须保留聚合负载，并返回可用于拆包的原始分段大小。 */
static void testSocketUdpSegmentation(void)
{
	xnetsocket Server;
	xnetsocket Client;
	xnetaddr ServerAddress;
	xnetaddr Remote;
	xnetdgramcontrol Control;
	xnetdgrammeta Meta;
	uint32 iServerCapabilities;
	uint32 iClientCapabilities;
	char sData[8] = { 0 };
	char sPayload[8] = { 0 };
	size_t iSize = 0;
	size_t iTotal = 0;
	xnetresult Result;

	Server = testSocketBound(XNET_SOCKET_DGRAM, &ServerAddress);
	Client = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_DGRAM, 0);
	testRequire((Server != NULL) && (Client != NULL),
		"UDP segmentation socket setup failed");
	iServerCapabilities = xrtNetSocketDgramCapabilities(Server);
	iClientCapabilities = xrtNetSocketDgramCapabilities(Client);
	if ( ((iServerCapabilities & XNET_DGRAM_CAP_SEGMENT_RECEIVE) == 0) ||
		 ((iClientCapabilities & XNET_DGRAM_CAP_SEGMENT_SEND) == 0) ) {
		testRequire(xrtNetSocketClose(Client) && xrtNetSocketClose(Server),
			"closing unsupported UDP segmentation sockets failed");
		return;
	}
	testRequire(
		((xrtNetSocketDgramMetaAvailable(Server) &
		 XNET_DGRAM_META_SEGMENT_SIZE) != 0) &&
		((xrtNetSocketDgramControlAvailable(Client) &
		 XNET_DGRAM_CONTROL_SEGMENT_SIZE) != 0),
		"UDP segmentation capability surfaces disagree"
	);
	testRequire(xrtNetSocketDgramMetaSet(
		Server,
		XNET_DGRAM_META_SEGMENT_SIZE
	), "enabling UDP receive coalescing failed");

	memset(&Control, 0, sizeof(Control));
	Control.Flags = XNET_DGRAM_CONTROL_SEGMENT_SIZE;
	Control.SegmentSize = 3;
	testRequire((xrtNetSocketSendMsg(
		Client,
		"abcdefg",
		7,
		&iSize,
		&ServerAddress,
		&Control
	) == XNET_RESULT_OK) && (iSize == 7),
		"UDP segmented send failed");

	/* Windows 允许而不保证合并；Linux 通常一次返回完整聚合载荷。 */
	while ( iTotal < 7 ) {
		memset(sData, 0, sizeof(sData));
		Result = xrtNetSocketRecvMsg(
			Server,
			sData,
			sizeof(sData),
			&iSize,
			&Remote,
			&Meta
		);
		testRequire((Result == XNET_RESULT_OK) && (iSize != 0) &&
			(iSize <= (7 - iTotal)) && xrtNetAddrIsLoopback(&Remote),
			"UDP segmented receive failed");
		if ( (Meta.Flags & XNET_DGRAM_META_SEGMENT_SIZE) != 0 ) {
			testRequire(Meta.SegmentSize == 3,
				"UDP coalesced receive segment size mismatch");
		} else {
			testRequire(iSize <= 3,
				"UDP unmarked receive exceeded one segment");
		}
		memcpy(sPayload + iTotal, sData, iSize);
		iTotal += iSize;
	}
	testRequire(memcmp(sPayload, "abcdefg", 7) == 0,
		"UDP segmented payload mismatch");

	testRequire(xrtNetSocketClose(Client) && xrtNetSocketClose(Server),
		"closing UDP segmentation sockets failed");
}



#if defined(__linux__)

/* Linux 必须把回环 ICMP 反馈还原为带目标和原负载的数据报错误。 */
static void testSocketUdpErrorQueue(void)
{
	xnetsocket Reservation;
	xnetsocket Client;
	xnetaddr Target;
	xnetdgramerror Error;
	struct pollfd Poll;
	char sPayload[8] = { 0 };
	size_t iSize = 0;
	xnetresult Result;

	Reservation = testSocketBound(XNET_SOCKET_DGRAM, &Target);
	testRequire(xrtNetSocketClose(Reservation),
		"releasing UDP error target failed");
	Client = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM,
		XNET_SOCKET_NONBLOCK
	);
	testRequire((Client != NULL) && xrtNetSocketSet(
		Client,
		XNET_OPTION_DGRAM_ERRORS,
		1
	) && (xrtNetSocketConnect(Client, &Target) == XNET_RESULT_OK),
		"opening UDP error queue client failed");
	testRequire((xrtNetSocketSend(
		Client,
		"error",
		5,
		&iSize
	) == XNET_RESULT_OK) && (iSize == 5),
		"sending UDP error queue probe failed");

	Poll.fd = (int)xrtNetSocketNative(Client);
	Poll.events = POLLERR;
	Poll.revents = 0;
	testRequire((Poll.fd >= 0) && (poll(&Poll, 1, 1000) == 1) &&
		((Poll.revents & POLLERR) != 0),
		"UDP error queue did not become ready");
	Result = xrtNetSocketDgramRecvError(
		Client,
		sPayload,
		sizeof(sPayload),
		&iSize,
		&Error
	);
	testRequire((Result == XNET_RESULT_OK) && (iSize == 5) &&
		(memcmp(sPayload, "error", 5) == 0),
		"UDP error queue payload mismatch");
	testRequire((Error.SystemCode == ECONNREFUSED) &&
		(Error.Kind == XERR_IO) &&
		(Error.Origin == XNET_DGRAM_ERROR_ICMP) &&
		((Error.Flags & XNET_DGRAM_ERROR_REMOTE) != 0) &&
		xrtNetAddrEqual(&Error.Remote, &Target),
		"UDP error queue metadata mismatch");
	testRequire(xrtNetSocketClose(Client),
		"closing UDP error queue client failed");
}

#endif



/* UDP 原语必须保留报文边界、来源地址、向量 IO、批量 IO 和零长度数据报。 */
static void testSocketUDP(void)
{
	xnetsocket Server;
	xnetsocket Client;
	xnetaddr ServerAddress;
	xnetaddr Remote;
	xnetspan SendVec[2];
	xnetwspan RecvVec[2];
	xnetdgramsend SendBatch[3];
	xnetdgramrecv RecvBatch[3];
	char BatchData[3][12] = { { 0 } };
	char sData[16] = { 0 };
	char aLeft[4] = { 0 };
	char aRight[8] = { 0 };
	size_t iSize;
	int64 iOption;

	Server = testSocketBound(XNET_SOCKET_DGRAM, &ServerAddress);
	testRequire(testSocketNoInherit(Server),
		"UDP server socket is inheritable");
	Client = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM, 0);
	testRequire(Client != NULL, "opening UDP client failed");
	testRequire(testSocketNoInherit(Client),
		"UDP client socket is inheritable");
	testRequire(xrtNetSocketSet(Client, XNET_OPTION_BROADCAST, 1) &&
		xrtNetSocketGet(Client, XNET_OPTION_BROADCAST, &iOption) &&
		(iOption != 0), "UDP broadcast option mismatch");

	testRequire((xrtNetSocketSendTo(Client, "hello", 5,
		&iSize, &ServerAddress) == XNET_RESULT_OK) && (iSize == 5),
		"UDP scalar send failed");
	testRequire((xrtNetSocketRecvFrom(Server, sData, sizeof(sData),
		&iSize, &Remote) == XNET_RESULT_OK) && (iSize == 5) &&
		(memcmp(sData, "hello", 5) == 0), "UDP scalar receive failed");
	testRequire((Remote.Family == XNET_FAMILY_IPV4) &&
		(Remote.Port != 0), "UDP source address mismatch");

	SendVec[0] = (xnetspan){ (cbytes)"vec", 3 };
	SendVec[1] = (xnetspan){ (cbytes)"-udp", 4 };
	testRequire((xrtNetSocketSendToVec(Client, SendVec, 2,
		&iSize, &ServerAddress) == XNET_RESULT_OK) && (iSize == 7),
		"UDP vector send failed");
	RecvVec[0] = (xnetwspan){ (bytes)aLeft, 3 };
	RecvVec[1] = (xnetwspan){ (bytes)aRight, 4 };
	testRequire((xrtNetSocketRecvFromVec(Server, RecvVec, 2,
		&iSize, &Remote) == XNET_RESULT_OK) && (iSize == 7),
		"UDP vector receive failed");
	testRequire((memcmp(aLeft, "vec", 3) == 0) &&
		(memcmp(aRight, "-udp", 4) == 0),
		"UDP vector payload mismatch");
	testRequire((xrtNetSocketSendTo(Client, NULL, 0,
		&iSize, &ServerAddress) == XNET_RESULT_OK) && (iSize == 0),
		"UDP zero-length send failed");
	testRequire((xrtNetSocketRecvFrom(Server, NULL, 0,
		&iSize, &Remote) == XNET_RESULT_OK) && (iSize == 0),
		"UDP zero-length receive failed");

	/* 未连接批量收发必须保留输入顺序、逐项截断结果和零长度报文。 */
	SendBatch[0] = (xnetdgramsend){ &ServerAddress, "one", 3 };
	SendBatch[1] = (xnetdgramsend){ &ServerAddress, "truncate", 8 };
	SendBatch[2] = (xnetdgramsend){ &ServerAddress, NULL, 0 };
	testRequire((xrtNetSocketSendBatch(Client, SendBatch, 3,
		&iSize) == XNET_RESULT_OK) && (iSize == 3),
		"UDP unconnected batch send failed");
	RecvBatch[0] = (xnetdgramrecv){
		.Data = BatchData[0], .Capacity = 3
	};
	RecvBatch[1] = (xnetdgramrecv){
		.Data = BatchData[1], .Capacity = 4
	};
	RecvBatch[2] = (xnetdgramrecv){ .Data = NULL, .Capacity = 0 };
	testSocketRecvBatchItems(Server, RecvBatch, 3,
		"UDP unconnected batch receive failed");
	{
		/* UDP 不保证投递顺序（Darwin 环回即可乱序）。断言改为：
		   每项结果合法且不超过容量，"one" 完整出现在某一项。 */
		bool bWhole = false;
		size_t iItem;

		for ( iItem = 0; iItem < 3; iItem++ ) {
			size_t iCapacity = (iItem == 0) ? 3u :
				((iItem == 1) ? 4u : 0u);
			bool bOk = (RecvBatch[iItem].Result == XNET_RESULT_OK) ||
				(RecvBatch[iItem].Result == XNET_RESULT_TRUNCATED);

			testRequire(bOk && (RecvBatch[iItem].Size <= iCapacity),
				"UDP batch item result mismatch");
			if ( (RecvBatch[iItem].Size == 3) &&
				(memcmp(RecvBatch[iItem].Data, "one", 3) == 0) ) {
				bWhole = true;
			}
		}
		testRequire(bWhole, "UDP batch item result mismatch");
	}
	{
		/* Darwin 上零长度报文可能不携带源地址；正长度报文即使
		   因乱序落入较小缓冲而截断，也必须保留来源地址。 */
		size_t iItem;
		bool bAddressed = false;

		for ( iItem = 0; iItem < 3; iItem++ ) {
			bool bConsumed =
				(RecvBatch[iItem].Result == XNET_RESULT_OK) ||
				(RecvBatch[iItem].Result == XNET_RESULT_TRUNCATED);

			if ( bConsumed && (RecvBatch[iItem].Size > 0) ) {
				testRequire(
					(RecvBatch[iItem].Remote.Family ==
					 XNET_FAMILY_IPV4) &&
					(RecvBatch[iItem].Remote.Port == Remote.Port),
					"UDP batch source address mismatch"
				);
				bAddressed = true;
			}
		}
		testRequire(bAddressed, "UDP batch source address missing");
	}

	/* 连接式 UDP 也必须保留报文截断和零长度报文语义。 */
	testRequire((xrtNetSocketConnect(Client, &ServerAddress) ==
		XNET_RESULT_OK) && (xrtNetSocketConnect(Server, &Remote) ==
		XNET_RESULT_OK), "connecting UDP sockets failed");
	testRequire((xrtNetSocketSend(Client, "truncate", 8,
		&iSize) == XNET_RESULT_OK) && (iSize == 8),
		"connected UDP scalar send failed");
	memset(sData, 0, sizeof(sData));
	testRequire((xrtNetSocketRecv(Server, sData, 3,
		&iSize) == XNET_RESULT_TRUNCATED) && (iSize == 3) &&
		(memcmp(sData, "tru", 3) == 0),
		"connected UDP scalar truncation mismatch");

	SendVec[0] = (xnetspan){ (cbytes)"vector", 6 };
	SendVec[1] = (xnetspan){ (cbytes)"-cut", 4 };
	testRequire((xrtNetSocketSendVec(Client, SendVec, 2,
		&iSize) == XNET_RESULT_OK) && (iSize == 10),
		"connected UDP vector send failed");
	memset(aLeft, 0, sizeof(aLeft));
	memset(aRight, 0, sizeof(aRight));
	RecvVec[0] = (xnetwspan){ (bytes)aLeft, 2 };
	RecvVec[1] = (xnetwspan){ (bytes)aRight, 2 };
	testRequire((xrtNetSocketRecvVec(Server, RecvVec, 2,
		&iSize) == XNET_RESULT_TRUNCATED) && (iSize == 4) &&
		(memcmp(aLeft, "ve", 2) == 0) &&
		(memcmp(aRight, "ct", 2) == 0),
		"connected UDP vector truncation mismatch");

	testRequire((xrtNetSocketSend(Client, NULL, 0,
		&iSize) == XNET_RESULT_OK) && (iSize == 0),
		"connected UDP zero-length send failed");
	testRequire((xrtNetSocketRecv(Server, NULL, 0,
		&iSize) == XNET_RESULT_OK) && (iSize == 0),
		"connected UDP zero-length receive failed");

	/* 空远端批量项复用连接式 UDP 的固定 Peer。 */
	memset(BatchData, 0, sizeof(BatchData));
	SendBatch[0] = (xnetdgramsend){ NULL, "batch", 5 };
	SendBatch[1] = (xnetdgramsend){ NULL, NULL, 0 };
	testRequire((xrtNetSocketSendBatch(Client, SendBatch, 2,
		&iSize) == XNET_RESULT_OK) && (iSize == 2),
		"connected UDP batch send failed");
	RecvBatch[0] = (xnetdgramrecv){
		.Data = BatchData[0], .Capacity = sizeof(BatchData[0])
	};
	RecvBatch[1] = (xnetdgramrecv){ .Data = NULL, .Capacity = 0 };
	testSocketRecvBatchItems(Server, RecvBatch, 2,
		"connected UDP batch receive failed");
	testRequire((RecvBatch[0].Result == XNET_RESULT_OK) &&
		(RecvBatch[0].Size == 5) &&
		(memcmp(RecvBatch[0].Data, "batch", 5) == 0) &&
		(RecvBatch[1].Result == XNET_RESULT_OK) &&
		(RecvBatch[1].Size == 0), "connected UDP batch item mismatch");

	testRequire(xrtNetSocketClose(Client),
		"closing UDP client failed");
	testRequire(xrtNetSocketClose(Server),
		"closing UDP server failed");
}



/* 执行 Socket 原语层完整回归。 */
int main(void)
{
	testSocketTCP();
	testSocketUdpMeta();
	testSocketUdpControl();
	testSocketUdpPathMtu();
	testSocketUdpSegmentation();
	#if defined(__linux__)
		testSocketUdpErrorQueue();
	#endif
	testSocketUDP();
	return 0;
}
