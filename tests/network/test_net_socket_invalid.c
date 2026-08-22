#include "../test.h"



/* 验证最近一次失败属于指定的稳定网络错误代码。 */
static void testSocketError(xneterror Code, cstr sMessage)
{
	const xerror* pError = xrtGetError();

	testRequire((pError != NULL) &&
		(xrtErrorDomain(pError) != NULL) &&
		(strcmp(xrtErrorDomain(pError), "xrt.net") == 0) &&
		(xrtErrorCode(pError) == (int32)Code), sMessage);
}



/* 无效构造参数必须在创建平台句柄前被拒绝。 */
static void testSocketInvalidOpen(void)
{
	testRequire(xrtNetSocketOpen(XNET_FAMILY_UNSPEC,
		XNET_SOCKET_STREAM, 0) == NULL,
		"unspecified socket family was accepted");
	testRequire(xrtNetSocketOpen(XNET_FAMILY_IPV4,
		(xnetsockettype)99, 0) == NULL,
		"invalid socket type was accepted");
	testRequire(xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM, 0x80000000u) == NULL,
		"unknown socket flag was accepted");
}



/* 类型专用操作必须由 XRT 一致拒绝，不能依赖平台偶然行为。 */
static void testSocketInvalidType(void)
{
	xnetsocket Stream;
	xnetsocket Datagram;
	xnetsocket Accepted = (xnetsocket)(uintptr_t)1;
	xnetaddr Address;
	int64 iValue = 77;
	size_t iSize;

	Stream = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM, 0);
	Datagram = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM, 0);
	testRequire((Stream != NULL) && (Datagram != NULL),
		"invalid-type socket setup failed");

	xrtClearError();
	testRequire(!xrtNetSocketSet(Datagram,
		XNET_OPTION_NO_DELAY, 1), "UDP no-delay was accepted");
	testSocketError(XNET_ERROR_SOCKET_OPTION,
		"UDP no-delay error mismatch");
	testRequire(!xrtNetSocketGet(Datagram,
		XNET_OPTION_LINGER, &iValue) && (iValue == 77),
		"UDP linger query modified output or succeeded");
	testRequire(!xrtNetSocketSet(Stream,
		XNET_OPTION_BROADCAST, 1), "TCP broadcast was accepted");
	testRequire(!xrtNetSocketListen(Datagram, 8),
		"datagram listen was accepted");
	testRequire((xrtNetSocketAccept(Datagram,
		&Accepted, NULL) == XNET_RESULT_ERROR) && (Accepted == NULL),
		"datagram accept output contract mismatch");

	testRequire(xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 1),
		"invalid-type address setup failed");
	testRequire(xrtNetSocketRecvFrom(Stream, NULL, 0,
		&iSize, NULL) == XNET_RESULT_ERROR,
		"stream recv-from was accepted");
	testRequire(xrtNetSocketSendTo(Stream, NULL, 0,
		&iSize, &Address) == XNET_RESULT_ERROR,
		"stream send-to was accepted");

	testRequire(xrtNetSocketClose(Datagram) &&
		xrtNetSocketClose(Stream), "invalid-type socket close failed");
}



/* 参数、输出原子性和连接完成状态必须在系统调用前得到验证。 */
static void testSocketInvalidArguments(void)
{
	xnetsocket Socket;
	xnetsocket Datagram;
	xnetsocket Receiver;
	xnetaddr Address;
	xnetaddr ReceiverAddress;
	xnetspan ReadSpan;
	xnetwspan WriteSpan;
	xnetdgramrecv RecvBatch;
	xnetdgramsend SendBatch[2];
	xnetdgramcontrol Control;
	xnetdgramerror DgramError;
	char sSegments[XNET_DGRAM_BATCH_MAX + 1u];
	size_t iSize = 91;
	int64 iValue = 123;
	char iByte = 0;

	Socket = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM, XNET_SOCKET_NONBLOCK);
	Datagram = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM, XNET_SOCKET_NONBLOCK);
	testRequire((Socket != NULL) && (Datagram != NULL),
		"invalid-argument socket setup failed");
	testRequire((xrtNetSocketDgramMetaAvailable(Socket) == 0) &&
		!xrtNetSocketDgramMetaSet(Socket, XNET_DGRAM_META_DESTINATION),
		"stream accepted datagram metadata");
	testRequire(xrtNetSocketDgramControlAvailable(Socket) == 0,
		"stream reported datagram send control");
	testRequire(xrtNetSocketDgramCapabilities(Socket) == 0,
		"stream reported advanced datagram capabilities");
	iSize = 91;
	memset(&DgramError, 0xA5, sizeof(DgramError));
	testRequire((xrtNetSocketDgramRecvError(
		Socket,
		&iByte,
		1,
		&iSize,
		&DgramError
	) == XNET_RESULT_ERROR) && (iSize == 0) &&
		(DgramError.Flags == 0), "stream datagram error queue was accepted");
	testRequire(!xrtNetSocketSet(
		Socket,
		XNET_OPTION_DGRAM_ERRORS,
		1
	), "stream datagram error option was accepted");
	testRequire(!xrtNetSocketDgramMetaSet(
		Datagram,
		XNET_DGRAM_META_TRUNCATED
	), "socket accepted a metadata result flag as configuration");
	iSize = 91;
	testRequire((xrtNetSocketRecvMsg(
		Datagram,
		&iByte,
		1,
		&iSize,
		NULL,
		NULL
	) == XNET_RESULT_ERROR) && (iSize == 0),
		"recv-message accepted a null metadata output");
	testRequire(xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 7),
		"datagram control address setup failed");
	memset(&Control, 0, sizeof(Control));
	Control.Flags = 0x80000000u;
	iSize = 91;
	testRequire((xrtNetSocketSendMsg(
		Datagram, &iByte, 1, &iSize, &Address, &Control
	) == XNET_RESULT_ERROR) && (iSize == 0),
		"unknown datagram control flag was accepted");
	Control.Flags = XNET_DGRAM_CONTROL_SOURCE;
	Control.Source = Address;
	testRequire(xrtNetSocketSendMsg(
		Datagram, &iByte, 1, &iSize, &Address, &Control
	) == XNET_RESULT_ERROR,
		"nonzero datagram source port was accepted");
	testRequire(xrtNetAddrAny(
		&Control.Source,
		XNET_FAMILY_IPV4,
		0
	), "unspecified datagram source setup failed");
	testRequire(xrtNetSocketSendMsg(
		Datagram, &iByte, 1, &iSize, &Address, &Control
	) == XNET_RESULT_ERROR,
		"unspecified datagram source was accepted");
	Control.Flags = XNET_DGRAM_CONTROL_HOP_LIMIT;
	Control.HopLimit = 256;
	testRequire(xrtNetSocketSendMsg(
		Datagram, &iByte, 1, &iSize, &Address, &Control
	) == XNET_RESULT_ERROR,
		"out-of-range datagram hop limit was accepted");
	memset(&Control, 0, sizeof(Control));
	Control.Flags = XNET_DGRAM_CONTROL_SEGMENT_SIZE;
	testRequire(xrtNetSocketSendMsg(
		Datagram, &iByte, 1, &iSize, &Address, &Control
	) == XNET_RESULT_ERROR,
		"zero datagram segment size was accepted");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"zero datagram segment size error mismatch");
	Control.SegmentSize = 65536u;
	testRequire(xrtNetSocketSendMsg(
		Datagram, &iByte, 1, &iSize, &Address, &Control
	) == XNET_RESULT_ERROR,
		"oversized datagram segment size was accepted");
	Control.SegmentSize = 1;
	testRequire(xrtNetSocketSendMsg(
		Datagram,
		sSegments,
		sizeof(sSegments),
		&iSize,
		&Address,
		&Control
	) == XNET_RESULT_ERROR,
		"excessive datagram segment count was accepted");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"excessive datagram segment count error mismatch");
	testRequire(xrtNetSocketFinishConnect(Socket) == XNET_RESULT_ERROR,
		"fresh socket reported a finished connection");
	testSocketError(XNET_ERROR_SOCKET_CONNECT,
		"fresh finish-connect error mismatch");

	testRequire(xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV6, 0),
		"mismatched address setup failed");
	testRequire(!xrtNetSocketBind(Socket, &Address),
		"mismatched bind family was accepted");
	testSocketError(XNET_ERROR_SOCKET_BIND,
		"mismatched bind error mismatch");

	testRequire(xrtNetSocketRecv(Socket, NULL, 1,
		&iSize) == XNET_RESULT_ERROR && (iSize == 0),
		"invalid recv buffer output mismatch");
	testRequire(xrtNetSocketSend(Socket, NULL, 1,
		&iSize) == XNET_RESULT_ERROR && (iSize == 0),
		"invalid send buffer output mismatch");
	testRequire(xrtNetSocketRecv(Socket, &iByte, 1,
		NULL) == XNET_RESULT_ERROR, "null recv count was accepted");
	testRequire(xrtNetSocketSend(Socket, &iByte, 1,
		NULL) == XNET_RESULT_ERROR, "null send count was accepted");

	ReadSpan = (xnetspan){ NULL, 1 };
	WriteSpan = (xnetwspan){ NULL, 1 };
	testRequire(xrtNetSocketSendVec(Socket, &ReadSpan, 1,
		&iSize) == XNET_RESULT_ERROR && (iSize == 0),
		"invalid send vector output mismatch");
	testSocketError(XNET_ERROR_SOCKET_WRITE,
		"invalid send vector error mismatch");
	testRequire(xrtNetSocketRecvVec(Socket, &WriteSpan, 1,
		&iSize) == XNET_RESULT_ERROR && (iSize == 0),
		"invalid recv vector output mismatch");
	testSocketError(XNET_ERROR_SOCKET_READ,
		"invalid recv vector error mismatch");

	/* 批量原语必须统一清零计数，并在任何系统调用前拒绝整批无效输入。 */
	iSize = 91;
	testRequire((xrtNetSocketRecvBatch(Datagram,
		NULL, 0, &iSize) == XNET_RESULT_OK) && (iSize == 0),
		"empty receive batch contract mismatch");
	iSize = 91;
	testRequire((xrtNetSocketSendBatch(Datagram,
		NULL, 0, &iSize) == XNET_RESULT_OK) && (iSize == 0),
		"empty send batch contract mismatch");
	testRequire(xrtNetSocketRecvBatch(Datagram,
		NULL, 0, NULL) == XNET_RESULT_ERROR,
		"null receive batch count was accepted");
	testRequire(xrtNetSocketSendBatch(Datagram,
		NULL, 0, NULL) == XNET_RESULT_ERROR,
		"null send batch count was accepted");

	RecvBatch = (xnetdgramrecv){ .Data = &iByte, .Capacity = 1 };
	iSize = 91;
	testRequire((xrtNetSocketRecvBatch(Datagram, &RecvBatch,
		XNET_DGRAM_BATCH_MAX + 1, &iSize) == XNET_RESULT_ERROR) &&
		(iSize == 0), "oversized receive batch was accepted");
	SendBatch[0] = (xnetdgramsend){ NULL, &iByte, 1 };
	iSize = 91;
	testRequire((xrtNetSocketSendBatch(Datagram, SendBatch,
		XNET_DGRAM_BATCH_MAX + 1, &iSize) == XNET_RESULT_ERROR) &&
		(iSize == 0), "oversized send batch was accepted");

	RecvBatch = (xnetdgramrecv){ .Data = NULL, .Capacity = 1 };
	iSize = 91;
	testRequire((xrtNetSocketRecvBatch(Datagram,
		&RecvBatch, 1, &iSize) == XNET_RESULT_ERROR) && (iSize == 0),
		"invalid receive batch buffer was accepted");
	SendBatch[0] = (xnetdgramsend){ NULL, NULL, 1 };
	iSize = 91;
	testRequire((xrtNetSocketSendBatch(Datagram,
		SendBatch, 1, &iSize) == XNET_RESULT_ERROR) && (iSize == 0),
		"invalid send batch buffer was accepted");

	RecvBatch = (xnetdgramrecv){ .Data = &iByte, .Capacity = 1 };
	iSize = 91;
	testRequire((xrtNetSocketRecvBatch(Socket,
		&RecvBatch, 1, &iSize) == XNET_RESULT_ERROR) && (iSize == 0),
		"stream receive batch was accepted");
	SendBatch[0] = (xnetdgramsend){ NULL, &iByte, 1 };
	iSize = 91;
	testRequire((xrtNetSocketSendBatch(Socket,
		SendBatch, 1, &iSize) == XNET_RESULT_ERROR) && (iSize == 0),
		"stream send batch was accepted");

	/* 后项地址无效时，前项也不能提前发送。 */
	Receiver = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM, XNET_SOCKET_NONBLOCK);
	testRequire((Receiver != NULL) &&
		xrtNetAddrLoopback(&ReceiverAddress, XNET_FAMILY_IPV4, 0) &&
		xrtNetSocketBind(Receiver, &ReceiverAddress) &&
		xrtNetSocketLocal(Receiver, &ReceiverAddress),
		"batch atomic-validation receiver setup failed");
	SendBatch[0] = (xnetdgramsend){ &ReceiverAddress, &iByte, 1 };
	SendBatch[1] = (xnetdgramsend){ &Address, &iByte, 1 };
	iSize = 91;
	testRequire((xrtNetSocketSendBatch(Datagram,
		SendBatch, 2, &iSize) == XNET_RESULT_ERROR) && (iSize == 0),
		"invalid batch address caused a partial send");
	iSize = 91;
	testRequire(xrtNetSocketAvailable(Receiver, &iSize) && (iSize == 0),
		"invalid batch address delivered the valid prefix");
	testRequire(xrtNetSocketClose(Receiver),
		"batch atomic-validation receiver close failed");

	testRequire(!xrtNetSocketGet(Socket,
		(xnetoption)99, &iValue) && (iValue == 123),
		"unknown option modified output or succeeded");
	testRequire(!xrtNetSocketSet(
		Datagram,
		XNET_OPTION_PATH_MTU_MODE,
		99
	), "invalid path MTU mode was accepted");
	testRequire(!xrtNetSocketSet(
		Datagram,
		XNET_OPTION_PATH_MTU,
		1500
	), "read-only path MTU was set");
	iValue = 123;
	testRequire(!xrtNetSocketGet(
		Datagram,
		XNET_OPTION_PATH_MTU,
		&iValue
	) && (iValue == 123),
		"unconnected path MTU query modified output or succeeded");
	iSize = 91;
	testRequire((xrtNetSocketDgramRecvError(
		Datagram,
		&iByte,
		1,
		&iSize,
		NULL
	) == XNET_RESULT_ERROR) && (iSize == 0),
		"null datagram error output was accepted");
	testRequire(xrtNetSocketDgramRecvError(
		Datagram,
		&iByte,
		1,
		NULL,
		&DgramError
	) == XNET_RESULT_ERROR,
		"null datagram error byte count was accepted");
	testRequire(!xrtNetSocketSet(Socket,
		XNET_OPTION_ERROR, 0), "read-only socket error was set");
	testRequire(!xrtNetSocketAvailable(Socket, NULL),
		"null available output was accepted");
	testRequire(!xrtNetSocketLocal(Socket, NULL),
		"null local-address output was accepted");
	testRequire(!xrtNetSocketShutdown(Socket, (xnetshutdown)99),
		"invalid shutdown direction was accepted");

	testRequire(xrtNetSocketClose(Datagram) && xrtNetSocketClose(Socket),
		"invalid-argument socket close failed");
}



/* 执行 Socket 负向契约回归。 */
int main(void)
{
	testSocketInvalidOpen();
	testSocketInvalidType();
	testSocketInvalidArguments();
	return 0;
}
