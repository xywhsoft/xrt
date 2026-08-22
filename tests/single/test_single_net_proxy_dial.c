#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



#if !defined(TEST_SINGLE_PROXY_HTTP_CONNECT)
	#define TEST_SINGLE_PROXY_HTTP_CONNECT 0
#endif



#if !defined(TEST_SINGLE_PROXY_DIAL_BACKEND)
	#define TEST_SINGLE_PROXY_DIAL_BACKEND XNET_PORT_SELECT
#endif



typedef enum testsingleproxystage {
	TEST_SINGLE_PROXY_GREETING = 0,
	TEST_SINGLE_PROXY_CONNECT,
	TEST_SINGLE_PROXY_TUNNEL
} testsingleproxystage;



typedef struct testsingleproxy {
	xatomicptr Client;
	xatomicptr Server;
	xatomic32 Done;
	xatomic32 Failed;
	xatomic32 Received;
	testsingleproxystage Stage;
	uint8 Reply[2];
	size_t ReplySize;
} testsingleproxy;



/* 把单头测试中的任意主机映射到当前 IPv4 回环 Listener。 */
static xnetaddrlist* testSingleProxyLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)sHost;
	(void)Family;
	(void)pData;
	if ( !xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0) ) {
		return NULL;
	}
	return xrtNetAddrListCreate(&Address, 1);
}



/* 接管单头代理服务端 Stream，并安装其状态机上下文。 */
static bool testSingleProxyAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	testsingleproxy* pState = (testsingleproxy*)pData;

	(void)pListener;
	if ( !xrtNetStreamSetData(pStream, pState) ) {
		return false;
	}
	xrtAtomicPtrStore(&pState->Server, pStream, XMEMORY_RELEASE);
	return true;
}



#if !TEST_SINGLE_PROXY_HTTP_CONNECT

/* 返回 SOCKS5 请求报文的完整长度，数据不足时返回零。 */
static size_t testSingleProxyRequestSize(const xnetbuf* pBuffer)
{
	uint8 Header[5];

	if ( (xrtNetBufSize(pBuffer) < 5) ||
		(xrtNetBufPeek(pBuffer, 0, Header, sizeof(Header)) !=
			sizeof(Header)) ) {
		return 0;
	}
	if ( Header[3] == 0x01 ) {
		return 10;
	}
	if ( Header[3] == 0x04 ) {
		return 22;
	}
	if ( Header[3] == 0x03 ) {
		return 7u + Header[4];
	}
	return SIZE_MAX;
}

#endif



/* 增量解析代理 CONNECT，隧道建立后原样回显应用字节。 */
static void testSingleProxyRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	#if TEST_SINGLE_PROXY_HTTP_CONNECT
		static const char Request[] =
			"CONNECT origin.single:443 HTTP/1.1\r\n"
			"Host: origin.single:443\r\n\r\n";
		static const char Connected[] =
			"HTTP/1.1 200 Connection Established\r\n\r\n";
	#else
	static const uint8 Method[] = { 0x05, 0x00 };
	static const uint8 Connected[] = {
		0x05, 0x00, 0x00, 0x01, 127, 0, 0, 1, 0x1F, 0x90
	};
	#endif
	testsingleproxy* pState = (testsingleproxy*)pData;

	for ( ;; ) {
		#if !TEST_SINGLE_PROXY_HTTP_CONNECT
		if ( pState->Stage == TEST_SINGLE_PROXY_GREETING ) {
			uint8 Header[2];
			size_t iSize;

			if ( (xrtNetBufSize(pBuffer) < 2) ||
				(xrtNetBufPeek(pBuffer, 0, Header, sizeof(Header)) !=
					sizeof(Header)) ) {
				return;
			}
			iSize = 2u + Header[1];
			if ( (Header[0] != 0x05) || (Header[1] == 0) ) {
				xrtAtomic32Store(&pState->Failed, 1, XMEMORY_RELEASE);
				(void)xrtNetStreamAbort(pStream);
				return;
			}
			if ( xrtNetBufSize(pBuffer) < iSize ) {
				return;
			}
			(void)xrtNetBufConsume(pBuffer, iSize);
			if ( xrtNetStreamSend(
				pStream,
				Method,
				sizeof(Method)
			) != XNET_RESULT_OK ) {
				xrtAtomic32Store(&pState->Failed, 1, XMEMORY_RELEASE);
				return;
			}
			pState->Stage = TEST_SINGLE_PROXY_CONNECT;
			continue;
		}
		#endif
		if ( pState->Stage == TEST_SINGLE_PROXY_CONNECT ) {
			#if TEST_SINGLE_PROXY_HTTP_CONNECT
				size_t iSize = sizeof(Request) - 1u;
				char Actual[sizeof(Request) - 1u];

				if ( xrtNetBufSize(pBuffer) < iSize ) {
					return;
				}
				if ( (xrtNetBufPeek(
					pBuffer, 0, Actual, iSize
				) != iSize) || (memcmp(
					Actual, Request, iSize
				) != 0) ) {
					xrtAtomic32Store(
						&pState->Failed, 1, XMEMORY_RELEASE
					);
					(void)xrtNetStreamAbort(pStream);
					return;
				}
			#else
			size_t iSize = testSingleProxyRequestSize(pBuffer);

			if ( iSize == 0 ) {
				return;
			}
			if ( iSize == SIZE_MAX ) {
				xrtAtomic32Store(&pState->Failed, 1, XMEMORY_RELEASE);
				(void)xrtNetStreamAbort(pStream);
				return;
			}
			if ( xrtNetBufSize(pBuffer) < iSize ) {
				return;
			}
			#endif
			(void)xrtNetBufConsume(pBuffer, iSize);
			if ( xrtNetStreamSend(
				pStream,
				Connected,
				sizeof(Connected) -
					(TEST_SINGLE_PROXY_HTTP_CONNECT ? 1u : 0u)
			) != XNET_RESULT_OK ) {
				xrtAtomic32Store(&pState->Failed, 1, XMEMORY_RELEASE);
				return;
			}
			pState->Stage = TEST_SINGLE_PROXY_TUNNEL;
			continue;
		}
		if ( xrtNetBufEmpty(pBuffer) ) {
			return;
		}
		if ( xrtNetStreamSendBuffer(pStream, pBuffer) != XNET_RESULT_OK ) {
			xrtAtomic32Store(&pState->Failed, 1, XMEMORY_RELEASE);
		}
		return;
	}
}



/* 收取隧道回显，并发布主线程可见的完成字节数。 */
static void testSingleProxyClientRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	testsingleproxy* pState = (testsingleproxy*)pData;
	size_t iSize = xrtNetBufSize(pBuffer);

	(void)pStream;
	if ( (iSize > (sizeof(pState->Reply) - pState->ReplySize)) ||
		(xrtNetBufRead(
			pBuffer,
			pState->Reply + pState->ReplySize,
			iSize
		) != iSize) ) {
		xrtAtomic32Store(&pState->Failed, 1, XMEMORY_RELEASE);
		return;
	}
	pState->ReplySize += iSize;
	xrtAtomic32Store(
		&pState->Received,
		(uint32)pState->ReplySize,
		XMEMORY_RELEASE
	);
}



/* 接管成功隧道，并立即发送一段应用数据验证剩余路径。 */
static void testSingleProxyDone(
	xnetproxydial* pDial,
	xnetresult Result,
	xnetstream* pStream,
	const xerror* pError,
	ptr pData
)
{
	testsingleproxy* pState = (testsingleproxy*)pData;

	(void)pDial;
	if ( (Result != XNET_RESULT_OK) ||
		(pStream == NULL) || (pError != NULL) ) {
		xrtAtomic32Store(&pState->Failed, 1, XMEMORY_RELEASE);
	} else {
		xrtAtomicPtrStore(&pState->Client, pStream, XMEMORY_RELEASE);
		if ( xrtNetStreamSend(pStream, "ok", 2) != XNET_RESULT_OK ) {
			xrtAtomic32Store(&pState->Failed, 1, XMEMORY_RELEASE);
		}
	}
	xrtAtomic32Store(&pState->Done, 1, XMEMORY_RELEASE);
}



/* 等待一个 Stream 进入关闭终态。 */
static bool testSingleProxyWaitClosed(
	const xnetstream* pStream,
	xdeadline Deadline
)
{
	while ( xrtNetStreamState(pStream) != XNET_STREAM_CLOSED ) {
		if ( xrtDeadlineExpired(Deadline) ) {
			return false;
		}
		xrtThreadYield();
	}
	return true;
}



/* 验证单个 xrt.h 完成 Resolver、TCP、代理 Dial、回显与完整回收。 */
int main(void)
{
	testsingleproxy State;
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xnetstreamevents ServerEvents;
	xnetstreamevents ClientEvents;
	xnetproxyconfig ProxyConfig;
	xnetproxydialconfig DialConfig;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetlistener* pListener;
	xnetproxy* pProxy;
	xnetproxydial* pDial;
	xnetstream* pClient;
	xnetstream* pServer;
	xnetaddr Address;
	xdeadline Deadline;

	memset(&State, 0, sizeof(State));
	xrtAtomicPtrInit(&State.Client, NULL);
	xrtAtomicPtrInit(&State.Server, NULL);
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&ServerEvents, 0, sizeof(ServerEvents));
	memset(&ClientEvents, 0, sizeof(ClientEvents));
	#if TEST_SINGLE_PROXY_HTTP_CONNECT
		State.Stage = TEST_SINGLE_PROXY_CONNECT;
	#endif
	ListenerEvents.Accept = testSingleProxyAccept;
	ServerEvents.Read = testSingleProxyRead;
	ClientEvents.Read = testSingleProxyClientRead;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_SINGLE_PROXY_DIAL_BACKEND;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		return 1;
	}
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Lookup = testSingleProxyLookup;
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
		&ServerEvents,
		&State
	);
	if ( (pListener == NULL) ||
		!xrtNetListenerLocal(pListener, &Address) ) {
		return 3;
	}
	xrtNetProxyConfigInit(&ProxyConfig);
	#if TEST_SINGLE_PROXY_HTTP_CONNECT
		ProxyConfig.Type = XNET_PROXY_HTTP_CONNECT;
	#endif
	ProxyConfig.Host = XRT_STR_LITERAL("proxy.single");
	ProxyConfig.Port = Address.Port;
	pProxy = xrtNetProxyCreate(&ProxyConfig);
	if ( pProxy == NULL ) {
		return 4;
	}
	xrtNetProxyDialConfigInit(&DialConfig);
	DialConfig.Transport.Stream.ReadSize = 64;
	DialConfig.Transport.Stream.WriteHighWater = 2;
	DialConfig.Transport.Stream.WriteLowWater = 1;
	DialConfig.Transport.Stream.WriteLimit = 3;
	DialConfig.Transport.Stream.ReadLimit = 512;
	DialConfig.ReceiveLimit = 256;
	pDial = xrtNetProxyDial(
		pEngine,
		pResolver,
		pProxy,
		"origin.single",
		443,
		&DialConfig,
		&ClientEvents,
		&State,
		testSingleProxyDone,
		&State
	);
	if ( pDial == NULL ) {
		return 5;
	}
	Deadline = xrtDeadlineAfter(5000000u);
	while ( (xrtAtomic32Load(&State.Done, XMEMORY_ACQUIRE) == 0) ||
		(xrtAtomic32Load(&State.Received, XMEMORY_ACQUIRE) < 2) ) {
		if ( xrtDeadlineExpired(Deadline) ||
			xrtAtomic32Load(&State.Failed, XMEMORY_ACQUIRE) ) {
			return 6;
		}
		xrtThreadYield();
	}
	pClient = (xnetstream*)xrtAtomicPtrLoad(
		&State.Client,
		XMEMORY_ACQUIRE
	);
	pServer = (xnetstream*)xrtAtomicPtrLoad(
		&State.Server,
		XMEMORY_ACQUIRE
	);
	if ( (pClient == NULL) || (pServer == NULL) ||
		(memcmp(State.Reply, "ok", 2) != 0) ||
		!xrtNetStreamClose(pClient) ||
		!xrtNetStreamClose(pServer) ||
		!testSingleProxyWaitClosed(pClient, Deadline) ||
		!testSingleProxyWaitClosed(pServer, Deadline) ) {
		return 7;
	}
	(void)xrtNetListenerClose(pListener);
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		if ( xrtDeadlineExpired(Deadline) ) {
			return 8;
		}
		xrtThreadYield();
	}
	xrtNetStreamDestroy(pClient);
	xrtNetStreamDestroy(pServer);
	xrtNetProxyDialDestroy(pDial);
	xrtNetProxyRelease(pProxy);
	xrtNetListenerDestroy(pListener);
	if ( !xrtNetResolverDestroy(pResolver) ) {
		return 9;
	}
	return xrtNetEngineDestroy(pEngine) ? 0 : 10;
}
