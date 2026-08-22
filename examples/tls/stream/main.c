#include "../common.h"



/* Echo 服务共享配置只在 Listener 和全部连接关闭后释放。 */
typedef struct example_tls_stream {
	xtlsserverconfig ServerConfig;
	xtlsstreamconfig StreamConfig;
	xtlsstreamevents StreamEvents;
	xatomic32 Connections;
	xatomic32 ListenerClosed;
} example_tls_stream;



/* 输出当前线程保存的结构化根错误。 */
static void exampleTlsStreamError(cstr sPrefix)
{
	const xerror* pError = xrtGetError();

	fprintf(
		stderr,
		"%s: %s\n",
		sPrefix,
		pError != NULL ? xrtErrorMessage(pError) : "unknown error"
	);
}



/* 从借用明文块逐 Span 回送；短写等待 Writable 后继续。 */
static void exampleTlsStreamEcho(xtlsstream* pStream)
{
	while ( xrtTlsStreamAvailable(pStream) != 0 ) {
		const xnetbuf* pBuffer = xrtTlsStreamBuffer(pStream);
		xnetspan Span;
		size_t iWritten = 0;
		xtlsresult Result;

		if ( (pBuffer == NULL) || !xrtNetBufFront(pBuffer, &Span) ) {
			(void)xrtTlsStreamAbort(pStream);
			return;
		}
		Result = xrtTlsStreamSend(
			pStream,
			Span.Data,
			Span.Size,
			&iWritten
		);
		if ( (iWritten != 0) &&
			!xrtTlsStreamConsume(pStream, iWritten) ) {
			(void)xrtTlsStreamAbort(pStream);
			return;
		}
		if ( Result == XTLS_AGAIN ) {
			return;
		}
		if ( Result != XTLS_OK ) {
			(void)xrtTlsStreamAbort(pStream);
			return;
		}
	}
}



/* TLS READY 后记录活动连接。 */
static void exampleTlsStreamOpen(xtlsstream* pStream, ptr pData)
{
	example_tls_stream* pExample = (example_tls_stream*)pData;
	xnetaddr Remote;
	char Address[128];

	(void)xrtAtomic32FetchAdd(
		&pExample->Connections,
		1,
		XMEMORY_RELEASE
	);
	if ( xrtNetStreamRemote(
		xrtTlsStreamTransport(pStream),
		&Remote
	) && xrtNetAddrText(
		&Remote,
		Address,
		sizeof(Address)
	) ) {
		printf("TLS client open: %s\n", Address);
	}
}



/* 新明文和写背压解除都进入同一个无重复 Echo 路径。 */
static void exampleTlsStreamRead(
	xtlsstream* pStream,
	const xnetbuf* pBuffer,
	ptr pData
)
{
	(void)pBuffer;
	(void)pData;
	exampleTlsStreamEcho(pStream);
}



/* 写入队列重新可用时继续尚未消费的借用明文。 */
static void exampleTlsStreamWritable(xtlsstream* pStream, ptr pData)
{
	(void)pData;
	exampleTlsStreamEcho(pStream);
}



/* 记录组合连接终态；失败根因只在回调期间打印。 */
static void exampleTlsStreamClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	example_tls_stream* pExample = (example_tls_stream*)pData;

	(void)pStream;
	if ( (Result != XNET_RESULT_OK) && (pError != NULL) ) {
		fprintf(
			stderr,
			"TLS client closed: %s\n",
			xrtErrorMessage(pError)
		);
	}
	(void)xrtAtomic32FetchSub(
		&pExample->Connections,
		1,
		XMEMORY_RELEASE
	);
}



/* 在 TCP Accept 回调内一步建立服务端 TLS 组合对象。 */
static bool exampleTlsStreamAccept(
	xnetlistener* pListener,
	xnetstream* pTransport,
	ptr pData
)
{
	example_tls_stream* pExample = (example_tls_stream*)pData;
	xtlsstream* pStream = NULL;
	bool bAccepted;

	(void)pListener;
	bAccepted = xrtTlsStreamAccept(
		pTransport,
		&pExample->ServerConfig,
		&pExample->StreamConfig,
		&pExample->StreamEvents,
		pExample,
		&pStream
	);
	if ( bAccepted ) {
		xrtTlsStreamDestroy(pStream);
	}
	return bAccepted;
}



/* 记录 Listener 唯一关闭回调。 */
static void exampleTlsStreamListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	example_tls_stream* pExample = (example_tls_stream*)pData;

	(void)pListener;
	xrtAtomic32Store(
		&pExample->ListenerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 运行一个可由 openssl s_client 访问的 TLS 1.3 Echo 服务。 */
int main(int argc, char** argv)
{
	static const xstrview Protocols[] = {
		XRT_STR_INIT("http/1.1")
	};
	example_tls_stream Example;
	uint8* pCertificateData = NULL;
	uint8* pPrivateData = NULL;
	size_t iCertificateSize = 0;
	size_t iPrivateSize = 0;
	xbytesview Certificate;
	xtlsidentity* pIdentity = NULL;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xnetengine* pEngine = NULL;
	xnetlistener* pListener = NULL;
	xnetaddr Address;
	unsigned long iPort = 8443u;
	int iResult = 1;

	if ( argc < 4 ) {
		printf(
			"usage: stream <rsa|p256|p384|ed25519> "
			"<certificate.der> <private.der> [port]\n"
		);
		return 0;
	}
	if ( argc >= 5 ) {
		char* pEnd = NULL;

		iPort = strtoul(argv[4], &pEnd, 10);
		if ( (pEnd == argv[4]) || (*pEnd != '\0') ||
			(iPort == 0) || (iPort > 65535u) ) {
			fprintf(stderr, "invalid port\n");
			goto Cleanup;
		}
	}
	if ( !exampleTlsReadFile(
		argv[2],
		&pCertificateData,
		&iCertificateSize
	) || !exampleTlsReadFile(
		argv[3],
		&pPrivateData,
		&iPrivateSize
	) ) {
		fprintf(stderr, "failed to read DER identity\n");
		goto Cleanup;
	}
	Certificate = (xbytesview) {
		pCertificateData,
		iCertificateSize
	};
	pIdentity = exampleTlsIdentity(
		argv[1],
		&Certificate,
		1u,
		(xbytesview) { pPrivateData, iPrivateSize }
	);
	if ( pIdentity == NULL ) {
		exampleTlsStreamError("failed to create TLS identity");
		goto Cleanup;
	}

	memset(&Example, 0, sizeof(Example));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	xrtTlsServerConfigInit(&Example.ServerConfig);
	Example.ServerConfig.Identity = pIdentity;
	Example.ServerConfig.Protocols = Protocols;
	Example.ServerConfig.ProtocolCount = 1u;
	Example.ServerConfig.RequireProtocol = true;
	xrtTlsStreamConfigInit(&Example.StreamConfig);
	Example.StreamEvents.Open = exampleTlsStreamOpen;
	Example.StreamEvents.Read = exampleTlsStreamRead;
	Example.StreamEvents.Writable = exampleTlsStreamWritable;
	Example.StreamEvents.Close = exampleTlsStreamClose;
	ListenerEvents.Accept = exampleTlsStreamAccept;
	ListenerEvents.Close = exampleTlsStreamListenerClose;

	xrtNetEngineConfigInit(&EngineConfig);
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		exampleTlsStreamError("failed to start network engine");
		goto Cleanup;
	}
	xrtNetListenConfigInit(&ListenConfig);
	if ( !xrtNetAddrAny(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		(uint16)iPort
	) ) {
		exampleTlsStreamError("failed to build listen address");
		goto Cleanup;
	}
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		&ListenerEvents,
		NULL,
		&Example
	);
	if ( (pListener == NULL) ||
		!xrtNetListenerLocal(pListener, &Address) ) {
		exampleTlsStreamError("failed to listen");
		goto Cleanup;
	}
	printf(
		"TLS echo listening on 0.0.0.0:%u; press Enter to stop\n",
		(unsigned)Address.Port
	);
	(void)getchar();
	if ( !xrtNetListenerClose(pListener) ) {
		exampleTlsStreamError("failed to close listener");
		goto Cleanup;
	}
	while ( (xrtAtomic32Load(
			&Example.ListenerClosed,
			XMEMORY_ACQUIRE
		) == 0) || (xrtAtomic32Load(
			&Example.Connections,
			XMEMORY_ACQUIRE
		) != 0) ) {
		xrtThreadYield();
	}
	iResult = 0;

Cleanup:
	xrtNetListenerDestroy(pListener);
	if ( (pEngine != NULL) && !xrtNetEngineDestroy(pEngine) ) {
		exampleTlsStreamError("failed to destroy network engine");
		iResult = 1;
	}
	xrtTlsIdentityRelease(pIdentity);
	free(pPrivateData);
	free(pCertificateData);
	return iResult;
}
