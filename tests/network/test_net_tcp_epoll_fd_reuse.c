#include "../test.h"

#if defined(__linux__) && defined(XRT_FEATURE_NET_PORT_EPOLL)
	#include <xrt/detail/wait.h>
	#include <errno.h>
	#include <netinet/in.h>
	#include <poll.h>
	#include <sys/socket.h>
	#include <unistd.h>

typedef struct testcontext {
	xatomic32 Accepted;
	xatomic32 Closed;
	xatomic32 ListenerClosed;
	xatomic32 WorkerReady;
	int FirstFd;
	int LastFd;
	uint32 LastWorker;
} testcontext;

static const char Payload[262144];

static void testWait(xatomic32* pValue, uint32 iExpected, cstr sMessage)
{
	double iDeadline = __xrtWaitAfter(5000);

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		testRequire(!__xrtWaitExpired(iDeadline), sMessage);
		xrtThreadYield();
	}
}

static bool testAccept(xnetlistener* pListener, xnetstream* pStream, ptr pData)
{
	testcontext* pContext = (testcontext*)pData;
	xnetsocket Socket = xrtNetStreamSocket(pStream);

	(void)pListener;
	testRequire(xrtNetStreamSetData(pStream, pContext), "set stream test context");
	pContext->LastFd = (int)xrtNetSocketNative(Socket);
	pContext->LastWorker = xrtNetWorkerIndex(xrtNetStreamWorker(pStream));
	if ( xrtAtomic32Load(&pContext->Accepted, XMEMORY_RELAXED) == 0 ) {
		pContext->FirstFd = pContext->LastFd;
		testRequire(xrtNetSocketSet(Socket, XNET_OPTION_SEND_BUFFER, 4096),
			"shrink the stream send buffer");
		testRequire(xrtNetStreamSend(pStream, Payload, sizeof(Payload)) == XNET_RESULT_OK,
			"queue enough data to leave the stream write-blocked");
	}
	(void)xrtAtomic32FetchAdd(&pContext->Accepted, 1, XMEMORY_RELEASE);
	return true;
}

static void testRead(xnetstream* pStream, xnetbuf* pBuffer, ptr pData)
{
	testcontext* pContext = (testcontext*)pData;
	xnetstreamstats Stats;

	(void)pBuffer;
	testRequire(xrtAtomic32Load(&pContext->Accepted, XMEMORY_ACQUIRE) == 1,
		"only the first client sends data");
	testRequire(xrtNetStreamStats(pStream, &Stats) && Stats.QueuedBytes > 0,
		"unread response must still have a pending write");
	testRequire(xrtNetStreamAbort(pStream),
		"abort from the read callback while the write is still pending");
}

static void testEnd(xnetstream* pStream, ptr pData)
{
	(void)pData;
	testRequire(xrtNetStreamAbort(pStream), "retire the disconnected test stream");
}

static void testClose(xnetstream* pStream, xnetresult Result, const xerror* pError, ptr pData)
{
	testcontext* pContext = (testcontext*)pData;

	(void)Result;
	testRequire(pError == NULL, "a reused descriptor must not fail epoll registration");
	xrtNetStreamDestroy(pStream);
	(void)xrtAtomic32FetchAdd(&pContext->Closed, 1, XMEMORY_RELEASE);
}

static void testListenerClose(xnetlistener* pListener, ptr pData)
{
	testcontext* pContext = (testcontext*)pData;

	(void)pListener;
	xrtAtomic32Store(&pContext->ListenerClosed, 1, XMEMORY_RELEASE);
}

static void testWorkerReady(xnetworker* pWorker, ptr pData)
{
	testcontext* pContext = (testcontext*)pData;

	(void)pWorker;
	xrtAtomic32Store(&pContext->WorkerReady, 1, XMEMORY_RELEASE);
}

static void testWaitForWorker(xnetengine* pEngine, testcontext* pContext)
{
	xrtAtomic32Store(&pContext->WorkerReady, 0, XMEMORY_RELEASE);
	testRequire(xrtNetEnginePost(pEngine, pContext->LastWorker, testWorkerReady, pContext),
		"post a barrier after the stream's initial watch");
	testWait(&pContext->WorkerReady, 1, "wait for the worker barrier");
}

static int testConnect(uint16 iPort)
{
	struct sockaddr_in Address = {0};
	int iFd = socket(AF_INET, SOCK_STREAM, 0);
	int iReceive = 4096;

	testRequire(iFd >= 0, "make loopback client");
	testRequire(setsockopt(iFd, SOL_SOCKET, SO_RCVBUF, &iReceive, sizeof(iReceive)) == 0,
		"shrink the client receive buffer");
	Address.sin_family = AF_INET;
	Address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	Address.sin_port = htons(iPort);
	testRequire(connect(iFd, (const struct sockaddr*)&Address, sizeof(Address)) == 0,
		"connect loopback client");
	return iFd;
}

static void testReuse(uint32 iWorkers)
{
	testcontext Context;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents = {0};
	xnetstreamevents StreamEvents = {0};
	xnetaddr Address;

	xrtAtomic32Init(&Context.Accepted, 0);
	xrtAtomic32Init(&Context.Closed, 0);
	xrtAtomic32Init(&Context.ListenerClosed, 0);
	xrtAtomic32Init(&Context.WorkerReady, 0);
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = iWorkers;
	EngineConfig.Backend = XNET_PORT_EPOLL;
	xnetengine* pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(pEngine != NULL && xrtNetEngineStart(pEngine), "start epoll engine");
	xrtNetListenConfigInit(&ListenConfig);
	ListenConfig.Distribution = XNET_ACCEPT_ROUND_ROBIN;
	ListenConfig.Stream.WriteLimit = sizeof(Payload) * 2;
	ListenerEvents.Accept = testAccept;
	ListenerEvents.Close = testListenerClose;
	StreamEvents.Read = testRead;
	StreamEvents.End = testEnd;
	StreamEvents.Close = testClose;
	xnetlistener* pListener =
		xrtNetListen(pEngine, &ListenConfig, &ListenerEvents, &StreamEvents, &Context);
	testRequire(pListener != NULL && xrtNetListenerLocal(pListener, &Address),
		"listen on an ephemeral loopback port");

	int iFirst = testConnect(Address.Port);
	struct pollfd Incoming = { iFirst, POLLIN, 0 };
	testRequire(poll(&Incoming, 1, 5000) == 1 && (Incoming.revents & POLLIN),
		"wait for server data without draining the receive window");
	testWait(&Context.Accepted, 1, "wait for the first stream");
	testWaitForWorker(pEngine, &Context);
	/* Give loopback ACKs time to fill the deliberately small receive window. */
	(void)poll(NULL, 0, 20);
	struct pollfd Outgoing = { Context.FirstFd, POLLOUT, 0 };
	testRequire(poll(&Outgoing, 1, 0) == 0, "the first server socket must be write-blocked");
	testRequire(send(iFirst, "x", 1, 0) == 1, "make the first stream readable");
	testWait(&Context.Closed, 1, "wait for the first stream to abort");
	(void)close(iFirst);

	for ( uint32 i = 2; i <= iWorkers * 2 + 1; ++i ) {
		int iClient = testConnect(Address.Port);
		char cByte;

		testWait(&Context.Accepted, i, "wait for the next stream");
		testRequire(Context.LastFd == Context.FirstFd, "reuse the original server descriptor");
		testWaitForWorker(pEngine, &Context);
		testRequire(recv(iClient, &cByte, 1, MSG_DONTWAIT) < 0 && errno == EAGAIN,
			"the idle connection must stay open after registering its read watch");
		(void)close(iClient);
		testWait(&Context.Closed, i, "wait for the next stream to close");
	}

	testRequire(xrtNetListenerClose(pListener), "close the listener");
	testWait(&Context.ListenerClosed, 1, "wait for listener close");
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetEngineDestroy(pEngine), "retire the epoll engine");
}
#endif

int main(void)
{
	#if defined(__linux__) && defined(XRT_FEATURE_NET_PORT_EPOLL)
		testReuse(1);
		testReuse(64);
		printf("[PASS] TCP epoll descriptor reuse after partial readiness\n");
	#else
		testRequire(true, "epoll is unavailable on this platform");
	#endif
	return 0;
}
