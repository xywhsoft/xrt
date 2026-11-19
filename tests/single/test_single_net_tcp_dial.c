#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
typedef SOCKET dial_native_socket;
typedef int dial_socklen;
#define DIAL_INVALID_SOCKET INVALID_SOCKET
#else
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int dial_native_socket;
typedef socklen_t dial_socklen;
#define DIAL_INVALID_SOCKET (-1)
#endif

static _Thread_local dial_native_socket TestHeldSocket = DIAL_INVALID_SOCKET;

static int testDialNativeConnect(dial_native_socket Socket,const struct sockaddr* Address,dial_socklen Length)
{
	if ( Address->sa_family != AF_INET6 ) {
		return connect(Socket, Address, Length);
	}
	TestHeldSocket = Socket;
#if defined(_WIN32) || defined(_WIN64)
	WSASetLastError(WSAEWOULDBLOCK);
#else
	errno = EINPROGRESS;
#endif
	return -1;
}

/* Keep the first native candidate pending while the select worker runs its
 * real fallback timer and opens the IPv4 connection. */
static int testDialNativeSelect(int Max,fd_set* Read,fd_set* Write,fd_set* Error,struct timeval* Timeout)
{
	if ( TestHeldSocket != DIAL_INVALID_SOCKET ) {
		if ( Read ) FD_CLR(TestHeldSocket, Read);
		if ( Write ) FD_CLR(TestHeldSocket, Write);
		if ( Error ) FD_CLR(TestHeldSocket, Error);
	}
	return select(Max, Read, Write, Error, Timeout);
}

static int testDialNativeClose(dial_native_socket Socket)
{
	if ( Socket == TestHeldSocket ) {
		TestHeldSocket = DIAL_INVALID_SOCKET;
	}
#if defined(_WIN32) || defined(_WIN64)
	return closesocket(Socket);
#else
	return close(Socket);
#endif
}

#define connect testDialNativeConnect
#define select testDialNativeSelect
#if defined(_WIN32) || defined(_WIN64)
#define closesocket testDialNativeClose
#else
#define close testDialNativeClose
#endif
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#undef connect
#undef select
#undef closesocket
#undef close
#include "net_engine_test.h"
#include <stdio.h>



typedef struct testsingledial {
	xnetstream* Client;
	xatomicptr Server;
	xatomic32 Done;
	xnetresult Result;
} testsingledial;



/* IPv6 is held pending by the native fixture. IPv4 has a real listener. */
static xnetaddrlist* testSingleDialLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Addresses[2];

	(void)sHost;
	(void)Family;
	(void)pData;
	if ( !xrtNetAddrLoopback(&Addresses[0], XNET_FAMILY_IPV6, 0) ||
		 !xrtNetAddrLoopback(&Addresses[1], XNET_FAMILY_IPV4, 0) ) {
		return NULL;
	}
	return xrtNetAddrListCreate(Addresses, 2);
}



/* 接管单头文件 Listener 接受的服务端 Stream。 */
static bool testSingleDialAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	testsingledial* pState = (testsingledial*)pData;

	(void)pListener;
	xrtAtomicPtrStore(&pState->Server, pStream, XMEMORY_RELEASE);
	return true;
}



/* 接管成功 Stream，并发布 Dial 唯一终态。 */
static void testSingleDialDone(
	xnetdial* pDial,
	xnetresult Result,
	xnetstream* pStream,
	const xerror* pError,
	ptr pData
)
{
	testsingledial* pState = (testsingledial*)pData;

	(void)pDial;
	if ( (Result == XNET_RESULT_OK) &&
		 (pStream != NULL) && (pError == NULL) ) {
		pState->Client = pStream;
	}
	pState->Result = Result;
	xrtAtomic32Store(&pState->Done, 1, XMEMORY_RELEASE);
}



/* 验证单头文件能够执行 Resolver、Dial、Accept 和完整关闭。 */
int main(void)
{
	testsingledial State;
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetlistener* pListener;
	xnetdial* pDial;
	xnetaddr Address;
	xnetdialconfig DialConfig;
	xnetdialstats Stats;
	xnetstream* pServer;
	double iDeadline;

	memset(&State, 0, sizeof(State));
	xrtAtomicPtrInit(&State.Server, NULL);
	xrtAtomic32Init(&State.Done, 0);
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	ListenerEvents.Accept = testSingleDialAccept;
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		return 1;
	}
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Lookup = testSingleDialLookup;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	if ( pResolver == NULL ) {
		return 2;
	}
	xrtNetListenConfigInit(&ListenConfig);
	(void)xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	);
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		&ListenerEvents,
		NULL,
		&State
	);
	if ( (pListener == NULL) ||
		 !xrtNetListenerLocal(pListener, &Address) ) {
		return 3;
	}
	xrtNetDialConfigInit(&DialConfig);
	DialConfig.FallbackDelay = 25;
	DialConfig.Timeout = 2000;
	pDial = xrtNetDial(
		pEngine,
		pResolver,
		"single.test",
		Address.Port,
		&DialConfig,
		NULL,
		NULL,
		testSingleDialDone,
		&State
	);
	if ( pDial == NULL ) {
		return 4;
	}
	iDeadline = __xrtWaitAfter(3000);
	while ( (xrtAtomic32Load(&State.Done, XMEMORY_ACQUIRE) == 0) ||
		 (State.Client && !xrtAtomicPtrLoad(&State.Server, XMEMORY_ACQUIRE)) ) {
		if ( __xrtWaitExpired(iDeadline) ) {
			return 5;
		}
		xrtThreadYield();
	}
	if ( !State.Client ) {
		fprintf(stderr, "fallback did not win while the first candidate was pending: result=%d\n",
			(int)State.Result);
		return 5;
	}
	if ( !xrtNetDialStats(pDial, &Stats) || !Stats.HasWinner ||
		 (Stats.WinnerIndex != 1) || (Stats.PeakAttempts != 2) ||
		 (Stats.AttemptsFailed != 0) || (Stats.ActiveAttempts != 0) ) {
		return 10;
	}
	pServer = xrtAtomicPtrLoad(&State.Server, XMEMORY_ACQUIRE);
	if ( !xrtNetStreamClose(State.Client) ||
		 !xrtNetStreamClose(pServer) ) {
		return 6;
	}
	while ( (xrtNetStreamState(State.Client) != XNET_STREAM_CLOSED) ||
		 (xrtNetStreamState(pServer) != XNET_STREAM_CLOSED) ) {
		if ( __xrtWaitExpired(iDeadline) ) {
			return 7;
		}
		xrtThreadYield();
	}
	(void)xrtNetListenerClose(pListener);
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtNetStreamDestroy(State.Client);
	xrtNetStreamDestroy(pServer);
	xrtNetDialDestroy(pDial);
	xrtNetListenerDestroy(pListener);
	if ( !xrtNetResolverDestroy(pResolver) ) {
		return 8;
	}
	if ( !testSingleNetEngineDestroy(pEngine) ) return 9;
	puts("[PASS] timed TCP fallback races a candidate while the first remains pending");
	return 0;
}
