/*
 * 范例：network/proxy_dial —— 真实代理隧道：托管 TCP + 完整回收
 * ----------------------------------------------------------------
 * 演示 API：
 *   代理拨号（SOCKS5 / HTTP CONNECT 可切）→ 托管隧道
 *   失败路径只借用回调期内的结构化错误
 *   原子终态等待 + 所有权全量回收
 * 模块宏：XRT_MODULE_PROXY（依赖 NET_TCP）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/proxy_dial/main.c -lws2_32 -liphlpapi
 * 用法：
 *   proxy_dial <proxy-host> <proxy-port> <target-host> <target-port>
 * 预期输出（无参数时）：
 *   usage: proxy_dial <proxy-host> ...
 *
 * 代理状态机（RESOLVING→CONNECTING→HANDSHAKE→CONNECTED）
 *   的失败区分：代理拒认 / TCP 失败 / DNS 失败——
 *   结构化错误的原因链让三者在日志里一眼可辨。
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xrt.h>



#if !defined(EXAMPLE_PROXY_HTTP_CONNECT)
	#define EXAMPLE_PROXY_HTTP_CONNECT 0
#endif



typedef struct exampleproxydial {
	xatomic32 Done;
	xatomic32 Closed;
	xnetresult Result;
	xnetstream* Stream;
} exampleproxydial;



/* 把十进制命令行参数解析为非零网络端口。 */
static bool exampleProxyDialPort(cstr sText, uint16* pPort)
{
	char* pEnd;
	unsigned long iValue;

	if ( (sText == NULL) || (sText[0] == 0) || (pPort == NULL) ) {
		return false;
	}
	iValue = strtoul(sText, &pEnd, 10);
	if ( (pEnd == sText) || (*pEnd != 0) ||
		(iValue == 0) || (iValue > UINT16_MAX) ) {
		return false;
	}
	*pPort = (uint16)iValue;
	return true;
}



/* 输出结构化错误及其原因链，便于区分代理、TCP 和 DNS 失败。 */
static void exampleProxyDialError(const xerror* pError)
{
	while ( pError != NULL ) {
		printf(
			"%s:%d %s: %s\n",
			xrtErrorDomain(pError),
			(int)xrtErrorCode(pError),
			xrtErrorOperation(pError),
			xrtErrorMessage(pError)
		);
		pError = xrtErrorCause(pError);
	}
}



/* 记录隧道 Stream 的关闭终态。 */
static void exampleProxyDialClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	exampleproxydial* pExample = (exampleproxydial*)pData;

	(void)pStream;
	if ( (Result != XNET_RESULT_OK) && (pError != NULL) ) {
		exampleProxyDialError(pError);
	}
	xrtAtomic32Store(&pExample->Closed, 1, XMEMORY_RELEASE);
}



/* 接管成功隧道，失败时只借用回调期内的结构化错误。 */
static void exampleProxyDialDone(
	xnetproxydial* pDial,
	xnetresult Result,
	xnetstream* pStream,
	const xerror* pError,
	ptr pData
)
{
	exampleproxydial* pExample = (exampleproxydial*)pData;

	(void)pDial;
	pExample->Result = Result;
	pExample->Stream = pStream;
	if ( pError != NULL ) {
		exampleProxyDialError(pError);
	}
	xrtAtomic32Store(&pExample->Done, 1, XMEMORY_RELEASE);
}



/* 等待原子终态，并在截止时间到达时返回失败。 */
static bool exampleProxyDialWait(
	const xatomic32* pValue,
	xdeadline Deadline
)
{
	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) == 0 ) {
		if ( xrtDeadlineExpired(Deadline) ) {
			return false;
		}
		xrtThreadYield();
	}
	return true;
}



/* 使用真实代理建立托管 TCP 隧道，并演示完整所有权回收。 */
int main(int argc, char** argv)
{
	exampleproxydial Example;
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetproxyconfig ProxyConfig;
	xnetproxydialconfig DialConfig;
	xnetstreamevents StreamEvents;
	xnetengine* pEngine = NULL;
	xnetresolver* pResolver = NULL;
	xnetproxy* pProxy = NULL;
	xnetproxydial* pDial = NULL;
	xdeadline Deadline;
	uint16 iProxyPort;
	uint16 iTargetPort;
	int iResult = 1;

	if ( argc != 5 ) {
		printf(
			"usage: proxy_dial <proxy-host> <proxy-port> "
			"<target-host> <target-port>\n"
		);
		return 0;
	}
	if ( !exampleProxyDialPort(argv[2], &iProxyPort) ||
		!exampleProxyDialPort(argv[4], &iTargetPort) ) {
		fprintf(stderr, "invalid proxy or target port\n");
		return 1;
	}
	memset(&Example, 0, sizeof(Example));
	memset(&StreamEvents, 0, sizeof(StreamEvents));
	StreamEvents.Close = exampleProxyDialClose;

	/* Engine 与 Resolver 由应用共享，不属于单次 Proxy Dial。 */
	xrtNetEngineConfigInit(&EngineConfig);
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		goto Cleanup;
	}
	xrtNetResolverConfigInit(&ResolverConfig);
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	if ( pResolver == NULL ) {
		goto Cleanup;
	}

	/* Proxy 是可跨线程共享的不可变配置，Dial 会持有自己的引用。 */
	xrtNetProxyConfigInit(&ProxyConfig);
	#if EXAMPLE_PROXY_HTTP_CONNECT
		ProxyConfig.Type = XNET_PROXY_HTTP_CONNECT;
	#endif
	ProxyConfig.Host.Data = argv[1];
	ProxyConfig.Host.Size = strlen(argv[1]);
	ProxyConfig.Port = iProxyPort;
	pProxy = xrtNetProxyCreate(&ProxyConfig);
	if ( pProxy == NULL ) {
		goto Cleanup;
	}
	xrtNetProxyDialConfigInit(&DialConfig);
	DialConfig.Timeout = 10000000u;
	pDial = xrtNetProxyDial(
		pEngine,
		pResolver,
		pProxy,
		argv[3],
		iTargetPort,
		&DialConfig,
		&StreamEvents,
		&Example,
		exampleProxyDialDone,
		&Example
	);
	if ( pDial == NULL ) {
		goto Cleanup;
	}

	/* 成功回调把 Stream 所有权交给调用方，Dial 可以独立释放。 */
	Deadline = xrtDeadlineAfter(15000000u);
	if ( !exampleProxyDialWait(&Example.Done, Deadline) ) {
		(void)xrtNetProxyDialCancel(pDial);
		goto Cleanup;
	}
	if ( (Example.Result != XNET_RESULT_OK) ||
		(Example.Stream == NULL) ) {
		goto Cleanup;
	}
	#if EXAMPLE_PROXY_HTTP_CONNECT
		printf("HTTP CONNECT tunnel is open\n");
	#else
		printf("SOCKS5 tunnel is open\n");
	#endif
	if ( !xrtNetStreamClose(Example.Stream) ||
		!exampleProxyDialWait(&Example.Closed, Deadline) ) {
		goto Cleanup;
	}
	iResult = 0;

Cleanup:
	/* 未完成操作先取消；终态 Stream、Dial、Proxy、Resolver 依次释放。 */
	if ( (pDial != NULL) &&
		(xrtAtomic32Load(&Example.Done, XMEMORY_ACQUIRE) == 0) ) {
		(void)xrtNetProxyDialCancel(pDial);
		Deadline = xrtDeadlineAfter(5000000u);
		(void)exampleProxyDialWait(&Example.Done, Deadline);
	}
	if ( (Example.Stream != NULL) &&
		(xrtNetStreamState(Example.Stream) != XNET_STREAM_CLOSED) ) {
		(void)xrtNetStreamAbort(Example.Stream);
		Deadline = xrtDeadlineAfter(5000000u);
		(void)exampleProxyDialWait(&Example.Closed, Deadline);
	}
	xrtNetStreamDestroy(Example.Stream);
	xrtNetProxyDialDestroy(pDial);
	xrtNetProxyRelease(pProxy);
	if ( (pResolver != NULL) && !xrtNetResolverDestroy(pResolver) ) {
		iResult = 1;
	}
	if ( pEngine != NULL ) {
		Deadline = xrtDeadlineAfter(5000000u);
		while ( !xrtNetEngineDestroy(pEngine) ) {
			xrtClearError();
			if ( xrtDeadlineExpired(Deadline) ) {
				iResult = 1;
				break;
			}
			xrtThreadYield();
		}
	}
	return iResult;
}
