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



/* 使用原生逃生接口等待非阻塞连接可写，事件后端将在后续模块替代此测试辅助。 */
static bool testSocketWaitWrite(xnetsocket Socket)
{
	fd_set WriteSet;
	fd_set ErrorSet;
	struct timeval Timeout;
	intptr_t iNative = xrtNetSocketNative(Socket);
	int iResult;

	FD_ZERO(&WriteSet);
	FD_ZERO(&ErrorSet);
	FD_SET((unsigned int)iNative, &WriteSet);
	FD_SET((unsigned int)iNative, &ErrorSet);
	Timeout.tv_sec = 3;
	Timeout.tv_usec = 0;
	#if defined(_WIN32) || defined(_WIN64)
		iResult = select(0, NULL, &WriteSet, &ErrorSet, &Timeout);
	#else
		iResult = select((int)iNative + 1,
			NULL, &WriteSet, &ErrorSet, &Timeout);
	#endif
	return iResult > 0;
}



/* 非阻塞 accept、recv 和 connect 必须把暂不可推进表达为 AGAIN。 */
int main(void)
{
	xnetsocket Listener;
	xnetsocket Client;
	xnetsocket Accepted = NULL;
	xnetsocket Datagram;
	xnetaddr Address;
	xnetaddr Remote;
	xnetresult Result;
	xnetspan Spans[65];
	char iByte;
	size_t iSize;
	size_t iAvailable;
	int64 iOption;

	Listener = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM, XNET_SOCKET_NONBLOCK);
	testRequire(Listener != NULL, "opening nonblocking listener failed");
	testRequire(xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0) &&
		xrtNetSocketBind(Listener, &Address) &&
		xrtNetSocketLocal(Listener, &Address) &&
		xrtNetSocketListen(Listener, 8),
		"starting nonblocking listener failed");
	testRequire(xrtNetSocketAccept(Listener,
		&Accepted, NULL) == XNET_RESULT_AGAIN,
		"empty nonblocking accept did not return AGAIN");
	testRequire(Accepted == NULL,
		"failed nonblocking accept modified output");

	Client = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM, XNET_SOCKET_NONBLOCK);
	testRequire(Client != NULL, "opening nonblocking client failed");
	Result = xrtNetSocketConnect(Client, &Address);
	testRequire((Result == XNET_RESULT_OK) ||
		(Result == XNET_RESULT_AGAIN), "nonblocking connect submit failed");
	if ( Result == XNET_RESULT_AGAIN ) {
		testRequire(testSocketWaitWrite(Client),
			"nonblocking connect did not become writable");
		testRequire(xrtNetSocketFinishConnect(Client) == XNET_RESULT_OK,
			"nonblocking connect completion failed");
	}

	for ( ;; ) {
		Result = xrtNetSocketAccept(Listener, &Accepted, &Remote);
		if ( Result != XNET_RESULT_AGAIN ) {
			break;
		}
	}
	testRequire((Result == XNET_RESULT_OK) && (Accepted != NULL),
		"nonblocking connected accept failed");
	testRequire(xrtNetSocketGet(Accepted,
		XNET_OPTION_NONBLOCK, &iOption) && (iOption != 0),
		"accepted socket did not inherit nonblocking contract");

	Datagram = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM, XNET_SOCKET_NONBLOCK);
	testRequire(Datagram != NULL, "opening nonblocking datagram failed");
	testRequire(xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0) &&
		xrtNetSocketBind(Datagram, &Address),
		"binding nonblocking datagram failed");
	testRequire(xrtNetSocketAvailable(Datagram, &iAvailable) &&
		(iAvailable == 0), "empty datagram available size mismatch");
	testRequire(xrtNetSocketRecvFrom(Datagram,
		&iByte, 1, &iSize, NULL) == XNET_RESULT_AGAIN,
		"empty nonblocking datagram receive did not return AGAIN");

	memset(Spans, 0, sizeof(Spans));
	testRequire(xrtNetSocketSendVec(Client,
		Spans, 65, &iSize) == XNET_RESULT_ERROR,
		"oversized socket vector was accepted");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorDomain(xrtGetError()) != NULL) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "xrt.net") == 0),
		"socket vector error domain mismatch");

	testRequire(xrtNetSocketClose(Datagram),
		"closing nonblocking datagram failed");
	testRequire(xrtNetSocketClose(Accepted),
		"closing nonblocking accepted socket failed");
	testRequire(xrtNetSocketClose(Client),
		"closing nonblocking client failed");
	testRequire(xrtNetSocketClose(Listener),
		"closing nonblocking listener failed");
	return 0;
}
