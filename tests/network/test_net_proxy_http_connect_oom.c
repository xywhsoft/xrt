#include "../test.h"



typedef struct testhttpoom {
	size_t Calls;
	size_t FailAt;
	bool Hit;
} testhttpoom;



/* 在指定后端分配调用点失败，其余请求直接交给系统堆。 */
static ptr testHttpOomAlloc(ptr pContext, size_t iSize)
{
	testhttpoom* pState = (testhttpoom*)pContext;

	pState->Calls++;
	if ( pState->Calls == pState->FailAt ) {
		pState->Hit = true;
		return NULL;
	}
	return malloc(iSize);
}



/* 在指定后端重分配调用点失败，否则保持标准 realloc 语义。 */
static ptr testHttpOomRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	testhttpoom* pState = (testhttpoom*)pContext;
	ptr pResult;

	pState->Calls++;
	if ( pState->Calls == pState->FailAt ) {
		pState->Hit = true;
		return NULL;
	}
	pResult = realloc(pMemory, iSize);
	return pResult;
}



/* 释放由测试后端分配器取得的系统堆块。 */
static void testHttpOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 创建带 Basic 凭据的代理，覆盖握手临时认证存储。 */
static xnetproxy* testHttpOomProxy(void)
{
	static uint8 User[1536];
	static uint8 Password[1536];
	xnetproxyconfig Config;

	memset(User, 'u', sizeof(User));
	memset(Password, 'p', sizeof(Password));

	xrtNetProxyConfigInit(&Config);
	Config.Type = XNET_PROXY_HTTP_CONNECT;
	Config.Host = XRT_STR_LITERAL("proxy.test");
	Config.Port = 8080;
	Config.Auth = XNET_PROXY_AUTH_REQUIRED;
	Config.Username.Data = User;
	Config.Username.Size = sizeof(User);
	Config.Password.Data = Password;
	Config.Password.Size = sizeof(Password);
	return xrtNetProxyCreate(&Config);
}



/* 创建一轮标准 HTTP CONNECT 握手。 */
static xnetproxyhandshake* testHttpOomHandshake(xnetproxy* pProxy)
{
	static char Host[1024];
	xnetproxyhandshakeconfig Config;

	/*
		长目标主机使握手所有权、认证临时区和输出报文均走直接分配，
		避免把线程缓存的可选元数据失败误判为协议层忽略 OOM。
	*/
	memset(Host, 'h', sizeof(Host));
	xrtNetProxyHandshakeConfigInit(&Config);
	Config.Proxy = pProxy;
	Config.TargetHost.Data = Host;
	Config.TargetHost.Size = sizeof(Host);
	Config.TargetPort = 443;
	return xrtNetProxyHandshakeCreate(&Config);
}



/* 验证当前逻辑分配数量和字节数都回到预期快照。 */
static void testHttpOomSnapshot(
	const xmemdebugsnapshot* pExpected,
	cstr sMessage
)
{
	xmemdebugsnapshot Snapshot;

	xrtMemDebugSnapshot(&Snapshot);
	testRequire(
		(Snapshot.LiveCount == pExpected->LiveCount) &&
		(Snapshot.LiveBytes == pExpected->LiveBytes),
		sMessage
	);
}



/* 逐个拒绝握手创建链的后端分配，并验证所有逻辑分配都回滚到代理基线。 */
static void testHttpOomCreation(
	testhttpoom* pState,
	xnetproxy* pProxy
)
{
	xmemdebugsnapshot Baseline;
	bool bReachedSuccess = false;
	size_t iCovered = 0;

	xrtMemDebugSnapshot(&Baseline);
	for ( size_t i = 1; i <= 8; i++ ) {
		xnetproxyhandshake* pHandshake;

		pState->Hit = false;
		pState->FailAt = pState->Calls + i;
		pHandshake = testHttpOomHandshake(pProxy);
		pState->FailAt = SIZE_MAX;
		if ( pHandshake != NULL ) {
			testRequire(!pState->Hit,
				"HTTP CONNECT creation ignored injected OOM");
			xrtNetProxyHandshakeDestroy(pHandshake);
			xrtClearError();
			testHttpOomSnapshot(
				&Baseline,
				"HTTP CONNECT successful scan changed live allocations"
			);
			bReachedSuccess = true;
			break;
		}
		testRequire(pState->Hit,
			"HTTP CONNECT creation failed before the selected OOM point");
		testRequire(pHandshake == NULL,
			"HTTP CONNECT handshake survived injected OOM");
		testRequire(xrtErrorIs(xrtGetError(), XERR_MEMORY) != NULL,
			"HTTP CONNECT creation OOM lost memory cause");
		xrtClearError();
		testHttpOomSnapshot(
			&Baseline,
			"HTTP CONNECT creation OOM leaked a logical allocation"
		);
		iCovered++;
	}
	testRequire(iCovered >= 2,
		"HTTP CONNECT creation did not expose scratch and output OOM points");
	testRequire(bReachedSuccess,
		"HTTP CONNECT creation OOM scan did not reach the success boundary");
}



/* 分片响应拉直失败必须进入终态、保留内存原因并释放全部输入块。 */
static void testHttpOomPullup(
	testhttpoom* pState,
	xnetproxy* pProxy
)
{
	static const char Prefix[] = "HTTP/1.1 200 OK\r\nProxy-Agent: ";
	xmemdebugsnapshot Baseline;
	xnetproxyhandshake* pHandshake;
	xnetbuf Input;
	xnetspan Output;
	char Part1[1400];
	char Part2[1400];
	size_t iPrefix = sizeof(Prefix) - 1u;

	xrtMemDebugSnapshot(&Baseline);
	pHandshake = testHttpOomHandshake(pProxy);
	testRequire(pHandshake != NULL,
		"HTTP CONNECT pullup fixture handshake failed");
	testRequire(xrtNetProxyHandshakeOutput(pHandshake, &Output) &&
		(xrtNetProxyHandshakeSent(pHandshake, Output.Size) == Output.Size),
		"HTTP CONNECT pullup fixture output failed");
	memcpy(Part1, Prefix, iPrefix);
	memset(Part1 + iPrefix, 'a', sizeof(Part1) - iPrefix);
	memset(Part2, 'b', sizeof(Part2));
	memcpy(
		Part2 + sizeof(Part2) - 4u,
		"\r\n\r\n",
		4u
	);
	testRequire(xrtNetBufInit(&Input, NULL) &&
		xrtNetBufAppendBorrow(&Input, Part1, sizeof(Part1)) &&
		xrtNetBufAppendBorrow(&Input, Part2, sizeof(Part2)),
		"HTTP CONNECT pullup fixture input failed");
	pState->Hit = false;
	pState->FailAt = pState->Calls + 1u;
	testRequire(xrtNetProxyHandshakeStep(pHandshake, &Input) ==
		XNET_PROXY_HANDSHAKE_ERROR,
		"HTTP CONNECT pullup survived injected OOM");
	testRequire(pState->Hit,
		"HTTP CONNECT pullup did not reach the selected OOM point");
	testRequire(xrtErrorIs(
		xrtNetProxyHandshakeError(pHandshake), XERR_MEMORY
	) != NULL, "HTTP CONNECT pullup OOM lost memory cause");
	pState->FailAt = SIZE_MAX;
	xrtNetBufClear(&Input);
	xrtNetProxyHandshakeDestroy(pHandshake);
	xrtClearError();
	testHttpOomSnapshot(
		&Baseline,
		"HTTP CONNECT pullup OOM leaked a logical allocation"
	);
}



/* 故障解除后，同一代理配置必须仍能建立并完成新的握手。 */
static void testHttpOomRecovery(xnetproxy* pProxy)
{
	static const char Response[] = "HTTP/1.1 204 No Content\r\n\r\n";
	xnetproxyhandshake* pHandshake = testHttpOomHandshake(pProxy);
	xnetbuf Input;
	xnetspan Output;

	testRequire(pHandshake != NULL,
		"HTTP CONNECT recovery handshake creation failed");
	testRequire(xrtNetProxyHandshakeOutput(pHandshake, &Output) &&
		(xrtNetProxyHandshakeSent(pHandshake, Output.Size) == Output.Size),
		"HTTP CONNECT recovery output failed");
	testRequire(xrtNetBufInit(&Input, NULL) &&
		xrtNetBufAppendBorrow(&Input, Response, sizeof(Response) - 1u),
		"HTTP CONNECT recovery input failed");
	testRequire(xrtNetProxyHandshakeStep(pHandshake, &Input) ==
		XNET_PROXY_HANDSHAKE_READY,
		"HTTP CONNECT did not recover after injected OOM");
	xrtNetBufClear(&Input);
	xrtNetProxyHandshakeDestroy(pHandshake);
}



/* 执行 HTTP CONNECT 精确故障注入和泄漏回归。 */
int main(void)
{
	static testhttpoom State;
	xallocator Allocator;
	xmemdebugsnapshot Snapshot;
	xnetproxy* pProxy;

	memset(&State, 0, sizeof(State));
	State.FailAt = SIZE_MAX;
	Allocator.Context = &State;
	Allocator.Alloc = testHttpOomAlloc;
	Allocator.Realloc = testHttpOomRealloc;
	Allocator.Free = testHttpOomFree;
	testRequire(xrtSetAllocator(&Allocator),
		"HTTP CONNECT OOM allocator install failed");
	testRequire(xrtMemDebugEnabled(),
		"HTTP CONNECT OOM test requires memory debug");
	testRequire(xrtMemDebugReset(),
		"HTTP CONNECT OOM memory debug reset failed");
	pProxy = testHttpOomProxy();
	testRequire(pProxy != NULL,
		"HTTP CONNECT OOM proxy fixture failed");
	testHttpOomCreation(&State, pProxy);
	testHttpOomPullup(&State, pProxy);
	testHttpOomRecovery(pProxy);
	xrtNetProxyRelease(pProxy);
	xrtClearError();
	xrtMemDebugSnapshot(&Snapshot);
	testRequire(Snapshot.LiveCount == 0,
		"HTTP CONNECT OOM final logical allocation leak");
	testRequire(xrtMemDebugReset(),
		"HTTP CONNECT OOM final memory debug reset failed");
	printf("[PASS] net_proxy_http_connect_oom\n");
	return 0;
}
