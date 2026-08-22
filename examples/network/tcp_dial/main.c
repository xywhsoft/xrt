#include <stdio.h>
#include <string.h>
#include <xrt.h>



typedef struct exampletcpdial {
	xnetstream* Client;
	xnetstream* Server;
	xatomic32 Accepted;
	xatomic32 Received;
	xatomic32 Closed;
} exampletcpdial;



/* 把示例主机名映射到本机，确保范例不依赖公网和系统 DNS。 */
static xnetaddrlist* exampleTcpDialLookup(
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



/* 接管服务端 Stream，并安装后续 Stream 回调需要的数据。 */
static bool exampleTcpDialAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	exampletcpdial* pContext = (exampletcpdial*)pData;

	(void)pListener;
	if ( !xrtNetStreamSetData(pStream, pContext) ) {
		return false;
	}
	pContext->Server = pStream;
	xrtAtomic32Store(&pContext->Accepted, 1, XMEMORY_RELEASE);
	return true;
}



/* 服务端原样回送，客户端验证完整响应。 */
static void exampleTcpDialRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	exampletcpdial* pContext = (exampletcpdial*)pData;
	char Data[32];
	size_t iSize = xrtNetBufRead(pBuffer, Data, sizeof(Data));

	if ( pStream == pContext->Server ) {
		(void)xrtNetStreamSend(pStream, Data, iSize);
	} else if ( (iSize == 10) && (memcmp(Data, "hello dial", 10) == 0) ) {
		xrtAtomic32Store(&pContext->Received, 1, XMEMORY_RELEASE);
	}
}



/* 记录两个公开 Stream 的正常关闭。 */
static void exampleTcpDialClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	exampletcpdial* pContext = (exampletcpdial*)pData;

	(void)pStream;
	if ( (Result == XNET_RESULT_OK) && (pError == NULL) ) {
		(void)xrtAtomic32FetchAdd(
			&pContext->Closed,
			1,
			XMEMORY_RELEASE
		);
	}
}



/* 等待示例原子条件，超时返回 false。 */
static bool exampleTcpDialWait(const xatomic32* pValue, uint32 iExpected)
{
	xdeadline iDeadline = xrtDeadlineAfter(3000000u);

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			return false;
		}
		xrtThreadYield();
	}
	return true;
}



/* 使用 Resolver、Dial Future 和普通 Stream 事件完成一次主机名连接。 */
int main(void)
{
	exampletcpdial Context;
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xnetstreamevents StreamEvents;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetlistener* pListener;
	xnetaddr Address;
	xfuture* pDial;

	memset(&Context, 0, sizeof(Context));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&StreamEvents, 0, sizeof(StreamEvents));
	ListenerEvents.Accept = exampleTcpDialAccept;
	StreamEvents.Read = exampleTcpDialRead;
	StreamEvents.Close = exampleTcpDialClose;

	/* 创建 Engine、Resolver 和本机服务端。 */
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		return 1;
	}
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Lookup = exampleTcpDialLookup;
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
		&StreamEvents,
		&Context
	);
	if ( (pListener == NULL) ||
		 !xrtNetListenerLocal(pListener, &Address) ) {
		return 3;
	}

	/* Future 持有成功 Stream，销毁 Future 前先增加独立引用。 */
	pDial = xrtNetDialAsync(
		pEngine,
		pResolver,
		"service.local",
		Address.Port,
		NULL,
		&StreamEvents,
		&Context
	);
	if ( (pDial == NULL) ||
		 (xrtFutureWaitFor(pDial, 3000000u) != XWAIT_OK) ||
		 (xrtFutureState(pDial) != XFUTURE_RESOLVED) ) {
		return 4;
	}
	Context.Client = xrtNetStreamRef(
		(xnetstream*)xrtFutureValue(pDial)
	);
	xrtFutureDestroy(pDial);
	if ( (Context.Client == NULL) ||
		 !exampleTcpDialWait(&Context.Accepted, 1) ) {
		return 5;
	}

	/* Stream 保持协议无关，成功连接后直接使用普通字节流 API。 */
	if ( xrtNetStreamSend(
		Context.Client,
		"hello dial",
		10
	) != XNET_RESULT_OK ) {
		return 6;
	}
	if ( !exampleTcpDialWait(&Context.Received, 1) ) {
		return 7;
	}
	printf("managed host connection received an echo\n");

	/* 对称关闭并释放所有调用方引用。 */
	if ( !xrtNetStreamClose(Context.Client) ||
		 !xrtNetStreamClose(Context.Server) ||
		 !exampleTcpDialWait(&Context.Closed, 2) ) {
		return 8;
	}
	(void)xrtNetListenerClose(pListener);
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtNetStreamDestroy(Context.Client);
	xrtNetStreamDestroy(Context.Server);
	xrtNetListenerDestroy(pListener);
	if ( !xrtNetResolverDestroy(pResolver) ) {
		return 9;
	}
	return xrtNetEngineDestroy(pEngine) ? 0 : 10;
}
