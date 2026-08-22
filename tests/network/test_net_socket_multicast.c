#if defined(_WIN32) || defined(_WIN64)
	#if defined(__TINYC__)
		#include <winapi/winsock2.h>
	#else
		#include <winsock2.h>
	#endif
#else
	#include <sys/select.h>
#endif

#include "../test.h"



/* 为真实多播接收设置有限等待，避免环境异常时测试卡死。 */
static bool testSocketMulticastWaitRead(xnetsocket Socket)
{
	fd_set ReadSet;
	struct timeval Timeout;
	intptr_t iNative = xrtNetSocketNative(Socket);
	int iResult;

	FD_ZERO(&ReadSet);
	FD_SET((unsigned int)iNative, &ReadSet);
	Timeout.tv_sec = 3;
	Timeout.tv_usec = 0;
	#if defined(_WIN32) || defined(_WIN64)
		iResult = select(0, &ReadSet, NULL, NULL, &Timeout);
	#else
		iResult = select((int)iNative + 1,
			&ReadSet, NULL, NULL, &Timeout);
	#endif
	return iResult > 0;
}



/* IPv4 多播必须覆盖成员、选项与本机回环传输。 */
static void testSocketMulticastIPv4(void)
{
	static const char sPayload[] = "xrt-multicast";
	xnetsocket Receiver;
	xnetsocket Sender;
	xnetaddr BindAddress;
	xnetaddr Group;
	xnetaddr Interface;
	xnetaddr Remote;
	char sBuffer[32] = { 0 };
	size_t iSize = 0;

	Receiver = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM, 0);
	Sender = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM, 0);
	testRequire((Receiver != NULL) && (Sender != NULL),
		"opening IPv4 multicast sockets failed");
	testRequire(xrtNetAddrAny(&BindAddress,
		XNET_FAMILY_IPV4, 0) &&
		xrtNetSocketBind(Receiver, &BindAddress) &&
		xrtNetSocketLocal(Receiver, &BindAddress),
		"binding IPv4 multicast receiver failed");
	testRequire(xrtNetAddrParse(&Group,
		"239.255.42.99", BindAddress.Port) &&
		xrtNetAddrLoopback(&Interface, XNET_FAMILY_IPV4, 0),
		"building IPv4 multicast addresses failed");

	/* 在发送前先完成组成员和回环接口配置。 */
	testRequire(xrtNetSocketMulticastJoin(
		Receiver, &Group, &Interface),
		"joining IPv4 multicast group failed");
	testRequire(xrtNetSocketMulticastInterface(
		Sender, &Interface) &&
		xrtNetSocketMulticastLoop(Sender, true) &&
		xrtNetSocketMulticastHopLimit(Sender, 1),
		"configuring IPv4 multicast sender failed");
	testRequire((xrtNetSocketSendTo(Sender,
		sPayload, sizeof(sPayload) - 1u, &iSize, &Group) ==
		XNET_RESULT_OK) && (iSize == (sizeof(sPayload) - 1u)),
		"sending IPv4 multicast datagram failed");
	testRequire(testSocketMulticastWaitRead(Receiver),
		"IPv4 multicast datagram timed out");
	testRequire((xrtNetSocketRecvFrom(Receiver,
		sBuffer, sizeof(sBuffer), &iSize, &Remote) ==
		XNET_RESULT_OK) && (iSize == (sizeof(sPayload) - 1u)) &&
		(memcmp(sBuffer, sPayload, iSize) == 0) &&
		(Remote.Family == XNET_FAMILY_IPV4),
		"IPv4 multicast payload mismatch");

	/* 边界值和恢复默认接口都是稳定契约。 */
	testRequire(xrtNetSocketMulticastLoop(Sender, false) &&
		xrtNetSocketMulticastLoop(Sender, true),
		"IPv4 multicast loop boundary failed");
	testRequire(xrtNetSocketMulticastHopLimit(Sender, 0) &&
		xrtNetSocketMulticastHopLimit(Sender, 255),
		"IPv4 multicast hop boundary failed");
	testRequire(!xrtNetSocketMulticastHopLimit(Sender, -1) &&
		!xrtNetSocketMulticastHopLimit(Sender, 256),
		"IPv4 multicast accepted invalid hop limit");
	testRequire(xrtNetSocketMulticastInterface(Sender, NULL),
		"restoring default multicast interface failed");
	testRequire(xrtNetSocketMulticastLeave(
		Receiver, &Group, &Interface),
		"leaving IPv4 multicast group failed");

	testRequire(xrtNetSocketClose(Sender) &&
		xrtNetSocketClose(Receiver),
		"closing IPv4 multicast sockets failed");
}



/* 无效组地址、地址族和 Socket 类型必须在系统调用前被拒绝。 */
static void testSocketMulticastInvalid(void)
{
	xnetsocket Datagram;
	xnetsocket Stream;
	xnetaddr IPv4;
	xnetaddr IPv6;

	Datagram = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM, 0);
	Stream = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM, 0);
	testRequire((Datagram != NULL) && (Stream != NULL),
		"opening invalid multicast sockets failed");
	testRequire(xrtNetAddrLoopback(&IPv4,
		XNET_FAMILY_IPV4, 0) &&
		xrtNetAddrLoopback(&IPv6, XNET_FAMILY_IPV6, 0),
		"building invalid multicast addresses failed");

	testRequire(!xrtNetSocketMulticastJoin(
		Datagram, &IPv4, NULL),
		"multicast join accepted a unicast group");
	testRequire(!xrtNetSocketMulticastJoin(
		Datagram, &IPv6, NULL) &&
		!xrtNetSocketMulticastInterface(Datagram, &IPv6),
		"multicast accepted a mismatched address family");
	testRequire(!xrtNetSocketMulticastJoin(
		Stream, &IPv4, NULL) &&
		!xrtNetSocketMulticastLoop(Stream, true) &&
		!xrtNetSocketMulticastHopLimit(Stream, 1) &&
		!xrtNetSocketMulticastInterface(Stream, &IPv4),
		"stream socket accepted multicast configuration");

	testRequire(xrtNetSocketClose(Stream) &&
		xrtNetSocketClose(Datagram),
		"closing invalid multicast sockets failed");
}



/* 执行 Socket 多播契约回归。 */
int main(void)
{
	testSocketMulticastIPv4();
	testSocketMulticastInvalid();
	return 0;
}
