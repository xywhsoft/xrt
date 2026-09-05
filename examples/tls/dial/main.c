#include "../common.h"



typedef struct example_tls_dial {
	char Request[1024];
	size_t RequestSize;
	size_t Sent;
	xtlsstream* Stream;
	xatomic32 Finished;
	xnetresult Result;
	bool Closing;
} example_tls_dial;



/* 输出回调借用的结构化错误。 */
static void exampleTlsDialError(cstr sPrefix, const xerror* pError)
{
	fprintf(
		stderr,
		"%s: %s\n",
		sPrefix,
		pError != NULL ? xrtErrorMessage(pError) : "unknown error"
	);
}



/* 发送尚未受理的请求前缀，背压解除后从精确偏移继续。 */
static void exampleTlsDialSend(xtlsstream* pStream, example_tls_dial* pExample)
{
	while ( pExample->Sent < pExample->RequestSize ) {
		size_t iWritten = 0;
		xtlsresult Result = xrtTlsStreamSend(
			pStream,
			pExample->Request + pExample->Sent,
			pExample->RequestSize - pExample->Sent,
			&iWritten
		);

		pExample->Sent += iWritten;
		if ( Result == XTLS_AGAIN ) {
			return;
		}
		if ( Result != XTLS_OK ) {
			(void)xrtTlsStreamAbort(pStream);
			return;
		}
	}

	/* 请求排队后认证关闭写侧，仍继续接收服务端响应。 */
	if ( !pExample->Closing ) {
		pExample->Closing = true;
		if ( !xrtTlsStreamClose(pStream) ) {
			(void)xrtTlsStreamAbort(pStream);
		}
	}
}



/* TLS READY 后开始发送最小 HTTP/1.1 请求。 */
static void exampleTlsDialOpen(xtlsstream* pStream, ptr pData)
{
	exampleTlsDialSend(pStream, (example_tls_dial*)pData);
}



/* TLS 与 TCP 两级发送预算恢复后继续未受理前缀。 */
static void exampleTlsDialWritable(xtlsstream* pStream, ptr pData)
{
	exampleTlsDialSend(pStream, (example_tls_dial*)pData);
}



/* 逐 Span 输出并精确消费借用明文，不复制整份响应。 */
static void exampleTlsDialRead(
	xtlsstream* pStream,
	const xnetbuf* pBuffer,
	ptr pData
)
{
	(void)pBuffer;
	(void)pData;

	while ( xrtTlsStreamAvailable(pStream) != 0 ) {
		const xnetbuf* pPlain = xrtTlsStreamBuffer(pStream);
		xnetspan Span;

		if ( (pPlain == NULL) || !xrtNetBufFront(pPlain, &Span) ) {
			(void)xrtTlsStreamAbort(pStream);
			return;
		}
		if ( fwrite(Span.Data, 1u, Span.Size, stdout) != Span.Size ) {
			(void)xrtTlsStreamAbort(pStream);
			return;
		}
		if ( !xrtTlsStreamConsume(pStream, Span.Size) ) {
			(void)xrtTlsStreamAbort(pStream);
			return;
		}
	}
}



/* 记录安全流唯一终态，主线程据此开始释放共享对象。 */
static void exampleTlsDialClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	example_tls_dial* pExample = (example_tls_dial*)pData;

	(void)pStream;
	pExample->Result = Result;
	if ( Result != XNET_RESULT_OK ) {
		exampleTlsDialError("TLS stream failed", pError);
	}
	xrtAtomic32Store(&pExample->Finished, 1, XMEMORY_RELEASE);
}



/* Dial 只在 TLS READY 后交付 Stream；失败不会发布 Stream 回调。 */
static void exampleTlsDialDone(
	xtlsdial* pDial,
	xnetresult Result,
	xtlsstream* pStream,
	const xerror* pError,
	ptr pData
)
{
	example_tls_dial* pExample = (example_tls_dial*)pData;

	(void)pDial;
	if ( Result == XNET_RESULT_OK ) {
		pExample->Stream = pStream;
		return;
	}
	pExample->Result = Result;
	exampleTlsDialError("TLS dial failed", pError);
	xrtAtomic32Store(&pExample->Finished, 1, XMEMORY_RELEASE);
}



/*
 * 范例：tls/dial —— HTTPS 客户端拨号：DNS → TCP → TLS 全链路
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtNetDialAddr / 系统信任库 + 主机名验证
 *   xrtTlsStreamAttach（客户端方向）   接管 TCP 流完成握手
 *   事件回调 + 原子终态               安全流生命周期编排
 * 模块宏：XRT_MODULE_TLS_STREAM（依赖 NET）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/tls/dial/main.c -lws2_32 -liphlpapi
 * 用法：
 *   dial <host> [port]
 * 预期输出（无参数时）：
 *   usage: dial <host> [port]
 *
 * "受管生命周期"的含义：DNS 解析、TCP 连接、TLS 握手
 *   每一步失败都会清理前面已建的资源（无句柄泄漏）；
 *   Finished 原子标志让主线程安全判断唯一终态。
 *   对外请求走 xhttp 更省事——本范例展示裸内核层。
 */


/* 使用系统信任库连接主机名，并展示受管 DNS、TCP 与 TLS 生命周期。 */
int main(int argc, char** argv)
{
	cstr sHost;
	unsigned long iPort = 443u;
	example_tls_dial Example = {0};
	xx509store* pStore = NULL;
	xtlsverifierconfig VerifierConfig;
	xtlsverifier* pVerifier = NULL;
	xtlsclientconfig TlsConfig;
	xtlsdialconfig DialConfig;
	xtlsstreamevents Events;
	xnetengineconfig EngineConfig;
	xnetengine* pEngine = NULL;
	xnetresolver* pResolver = NULL;
	xtlsdial* pDial = NULL;
	int iRequestSize;
	int iResult = 1;

	/* 构建门禁不隐式依赖外部网络，真实连接由调用方明确触发。 */
	if ( argc < 2 ) {
		printf("usage: dial <host> [port]\n");
		return 0;
	}
	sHost = argv[1];

	/* 可选端口必须完整落在 uint16 范围内。 */
	if ( argc >= 3 ) {
		char* pEnd = NULL;

		iPort = strtoul(argv[2], &pEnd, 10);
		if ( (pEnd == argv[2]) || (*pEnd != 0) ||
			(iPort == 0) || (iPort > 65535u) ) {
			fprintf(stderr, "invalid port\n");
			goto Cleanup;
		}
	}

	/* 请求内容固定有界，过长主机名在连接前失败。 */
	iRequestSize = snprintf(
		Example.Request,
		sizeof(Example.Request),
		"GET / HTTP/1.1\r\nHost: %s\r\n"
		"Connection: close\r\nUser-Agent: xrt-tls-dial\r\n\r\n",
		sHost
	);
	if ( (iRequestSize < 0) ||
		((size_t)iRequestSize >= sizeof(Example.Request)) ) {
		fprintf(stderr, "host name is too long\n");
		goto Cleanup;
	}
	Example.RequestSize = (size_t)iRequestSize;

	/* 验证器创建时复制系统信任快照，Store 随后即可释放。 */
	pStore = xrtX509StoreSystem();
	if ( pStore == NULL ) {
		exampleTlsDialError("failed to load system trust store", xrtGetError());
		goto Cleanup;
	}
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Store = pStore;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	if ( pVerifier == NULL ) {
		exampleTlsDialError("failed to create TLS verifier", xrtGetError());
		goto Cleanup;
	}
	xrtX509StoreFree(pStore);
	pStore = NULL;

	/* Engine 与 Resolver 分别拥有网络 Worker 和阻塞名称解析工作池。 */
	xrtNetEngineConfigInit(&EngineConfig);
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		exampleTlsDialError("failed to start network engine", xrtGetError());
		goto Cleanup;
	}
	pResolver = xrtNetResolverCreate(NULL);
	if ( pResolver == NULL ) {
		exampleTlsDialError("failed to create resolver", xrtGetError());
		goto Cleanup;
	}

	/* 主机名自动进入 SNI 和证书验证名称，总超时覆盖全部阶段。 */
	memset(&Events, 0, sizeof(Events));
	Events.Open = exampleTlsDialOpen;
	Events.Read = exampleTlsDialRead;
	Events.Writable = exampleTlsDialWritable;
	Events.Close = exampleTlsDialClose;
	xrtTlsClientConfigInit(&TlsConfig);
	TlsConfig.Verifier = pVerifier;
	xrtTlsDialConfigInit(&DialConfig);
	DialConfig.Timeout = 15000000u;
	pDial = xrtTlsDial(
		pEngine,
		pResolver,
		sHost,
		(uint16)iPort,
		&TlsConfig,
		&DialConfig,
		&Events,
		&Example,
		exampleTlsDialDone,
		&Example
	);
	if ( pDial == NULL ) {
		exampleTlsDialError("failed to submit TLS dial", xrtGetError());
		goto Cleanup;
	}

	/* Dial 总超时和 Stream 关闭超时保证等待最终收敛。 */
	while ( xrtAtomic32Load(
		&Example.Finished,
		XMEMORY_ACQUIRE
	) == 0 ) {
		xrtThreadYield();
	}
	iResult = Example.Result == XNET_RESULT_OK ? 0 : 1;

Cleanup:
	xrtTlsDialDestroy(pDial);
	xrtTlsStreamDestroy(Example.Stream);
	if ( (pResolver != NULL) && !xrtNetResolverDestroy(pResolver) ) {
		exampleTlsDialError("failed to destroy resolver", xrtGetError());
		iResult = 1;
	}
	if ( (pEngine != NULL) && !xrtNetEngineDestroy(pEngine) ) {
		exampleTlsDialError("failed to destroy network engine", xrtGetError());
		iResult = 1;
	}
	xrtTlsVerifierRelease(pVerifier);
	xrtX509StoreFree(pStore);
	return iResult;
}
