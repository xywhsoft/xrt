#include "../test.h"



typedef struct testproxydialoom {
	xatomic32 Fail;
	xatomic32 FailAfterLookup;
	xatomic32 Done;
	xatomic32 BlockStart;
	xatomic32 BlockEntered;
	xatomic32 BlockRelease;
	xatomic64 Allocations;
	xatomic64 BlockThread;
	xnetresult Result;
	xerrkind ErrorKind;
} testproxydialoom;



typedef struct testproxydialsetup {
	testproxydialoom* Allocator;
	xnetengine* Engine;
	xnetresolver* Resolver;
	xnetproxy* Proxy;
	const xnetproxydialconfig* Config;
	xnetproxydial* Dial;
	xatomic32 Done;
} testproxydialsetup;



#define TEST_PROXY_DIAL_OOM_ADDRESS_COUNT	64u
#define TEST_PROXY_DIAL_OOM_HOST_SIZE		1024u



#if !defined(TEST_PROXY_DIAL_BACKEND)
	#define TEST_PROXY_DIAL_BACKEND XNET_PORT_SELECT
#endif



/* 正常阶段转发分配，故障阶段拒绝所有新的底层内存。 */
static ptr testProxyDialOomAlloc(ptr pData, size_t iSize)
{
	testproxydialoom* pContext = (testproxydialoom*)pData;
	uint64 iBlockThread = xrtAtomic64Load(
		&pContext->BlockThread,
		XMEMORY_ACQUIRE
	);

	(void)xrtAtomic64FetchAdd(
		&pContext->Allocations,
		1,
		XMEMORY_RELAXED
	);
	if ( (iBlockThread != 0) &&
		 (iBlockThread == xrtThreadCurrentId()) ) {
		xrtAtomic32Store(
			&pContext->BlockEntered,
			1,
			XMEMORY_RELEASE
		);
		while ( xrtAtomic32Load(
			&pContext->BlockRelease,
			XMEMORY_ACQUIRE
		) == 0 ) {
			xrtThreadYield();
		}
		xrtAtomic64Store(
			&pContext->BlockThread,
			0,
			XMEMORY_RELEASE
		);
	}
	return xrtAtomic32Load(&pContext->Fail, XMEMORY_ACQUIRE) != 0 ?
		NULL : malloc(iSize);
}



/* 正常阶段保持 realloc 语义，故障阶段拒绝扩容。 */
static ptr testProxyDialOomRealloc(
	ptr pData,
	ptr pMemory,
	size_t iSize
)
{
	testproxydialoom* pContext = (testproxydialoom*)pData;

	(void)xrtAtomic64FetchAdd(
		&pContext->Allocations,
		1,
		XMEMORY_RELAXED
	);
	return xrtAtomic32Load(&pContext->Fail, XMEMORY_ACQUIRE) != 0 ?
		NULL : realloc(pMemory, iSize);
}



/* 释放正常阶段已经取得的底层内存。 */
static void testProxyDialOomFree(ptr pData, ptr pMemory)
{
	(void)pData;
	free(pMemory);
}



/* 先建立完整地址结果，再关闭分配器以命中异步候选地址复制。 */
static xnetaddrlist* testProxyDialOomLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	testproxydialoom* pContext = (testproxydialoom*)pData;
	xnetaddr Addresses[TEST_PROXY_DIAL_OOM_ADDRESS_COUNT];
	xnetaddrlist* pList;

	(void)sHost;
	(void)Family;
	for ( size_t i = 0; i < TEST_PROXY_DIAL_OOM_ADDRESS_COUNT; i++ ) {
		if ( !xrtNetAddrLoopback(
			&Addresses[i],
			XNET_FAMILY_IPV4,
			0
		) ) {
			return NULL;
		}
		Addresses[i].Address[3] = (uint8)(i + 1u);
	}
	pList = xrtNetAddrListCreate(
		Addresses,
		TEST_PROXY_DIAL_OOM_ADDRESS_COUNT
	);
	if ( pList == NULL ) {
		fprintf(
			stderr,
			"proxy Dial OOM lookup fixture failed: kind=%d code=%d "
			"message=%s\n",
			(int)xrtErrorKind(xrtGetError()),
			(int)xrtErrorCode(xrtGetError()),
			xrtErrorMessage(xrtGetError())
		);
		return NULL;
	}
	if ( xrtAtomic32Exchange(
		&pContext->FailAfterLookup,
		0,
		XMEMORY_ACQ_REL
	) != 0 ) {
		xrtAtomic32Store(&pContext->Fail, 1, XMEMORY_RELEASE);
	}
	return pList;
}



/* OOM 终态必须保留错误，且不能把半成品 Stream 转移给调用方。 */
static void testProxyDialOomDone(
	xnetproxydial* pDial,
	xnetresult Result,
	xnetstream* pStream,
	const xerror* pError,
	ptr pData
)
{
	testproxydialoom* pContext = (testproxydialoom*)pData;

	(void)pDial;
	testRequire((pStream == NULL) && (pError != NULL),
		"proxy Dial OOM returned a stream or lost its error");
	pContext->Result = Result;
	pContext->ErrorKind = xrtErrorKind(pError);
	(void)xrtAtomic32FetchAdd(
		&pContext->Done,
		1,
		XMEMORY_RELEASE
	);
}



/* 初始化窗口测试只回收成功转移的 Stream，并发布唯一终态。 */
static void testProxyDialSetupDone(
	xnetproxydial* pDial,
	xnetresult Result,
	xnetstream* pStream,
	const xerror* pError,
	ptr pData
)
{
	testproxydialsetup* pContext = (testproxydialsetup*)pData;

	(void)pDial;
	(void)Result;
	(void)pError;
	xrtNetStreamDestroy(pStream);
	xrtAtomic32Store(&pContext->Done, 1, XMEMORY_RELEASE);
}



/* 在独立线程中进入 Proxy Dial，使分配器能冻结 Engine Hold 后的窗口。 */
static int32 testProxyDialSetupThread(ptr pData)
{
	testproxydialsetup* pContext = (testproxydialsetup*)pData;

	xrtAtomic64Store(
		&pContext->Allocator->BlockThread,
		xrtThreadCurrentId(),
		XMEMORY_RELEASE
	);
	while ( xrtAtomic32Load(
		&pContext->Allocator->BlockStart,
		XMEMORY_ACQUIRE
	) == 0 ) {
		xrtThreadYield();
	}
	pContext->Dial = xrtNetProxyDial(
		pContext->Engine,
		pContext->Resolver,
		pContext->Proxy,
		"origin.test",
		443,
		pContext->Config,
		NULL,
		NULL,
		testProxyDialSetupDone,
		pContext
	);
	return pContext->Dial != NULL ? 0 : -1;
}



/* 创建一个持有指定代理主机深拷贝的 SOCKS5 配置。 */
static xnetproxy* testProxyDialOomProxy(cstr sHost, size_t iSize)
{
	xnetproxyconfig Config;

	xrtNetProxyConfigInit(&Config);
	Config.Host.Data = sHost;
	Config.Host.Size = iSize;
	Config.Port = 1080;
	return xrtNetProxyCreate(&Config);
}



/* 仅在断言失败时展开完整原因链，避免跨架构诊断丢失根因。 */
static void testProxyDialOomPrintError(const xerror* pError)
{
	size_t iDepth = 0;

	while ( pError != NULL ) {
		fprintf(
			stderr,
			"proxy Dial OOM error[%u]: domain=%s code=%d kind=%d "
			"operation=%s message=%s data=%s\n",
			(unsigned)iDepth,
			xrtErrorDomain(pError),
			(int)xrtErrorCode(pError),
			(int)xrtErrorKind(pError),
			xrtErrorOperation(pError),
			xrtErrorMessage(pError),
			xrtErrorData(pError)
		);
		pError = xrtErrorCause(pError);
		iDepth++;
	}
}



/* 等待上一轮取消遗留的查询与回调全部离开 Resolver。 */
static void testProxyDialOomWaitResolverIdle(xnetresolver* pResolver)
{
	xdeadline Deadline = xrtDeadlineAfter(5000000u);
	xnetresolverstats Stats;

	for ( ;; ) {
		testRequire(
			xrtNetResolverStats(pResolver, &Stats),
			"proxy Dial OOM resolver statistics unavailable"
		);
		if ( (Stats.Outstanding == 0) &&
			(Stats.ActiveQueries == 0) &&
			(Stats.QueuedQueries == 0) &&
			(Stats.RunningQueries == 0) &&
			(Stats.ReadyCallbacks == 0) ) {
			return;
		}
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"proxy Dial OOM resolver did not become idle"
		);
		xrtThreadYield();
	}
}



/* 验证协议层与组合层的同步、异步 OOM 都能稳定终止并完整回收。 */
int main(void)
{
	testproxydialoom Context;
	xallocator Allocator;
	xnetproxyhandshakeconfig HandshakeConfig;
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetproxydialconfig DialConfig;
	xnetproxydialstats Stats;
	testproxydialsetup Setup;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetproxy* pProxy;
	xnetproxy* pLargeProxy;
	xnetproxyhandshake* pHandshake;
	xnetproxydial* pDial;
	xthread* pSetupThread;
	const xerror* pError;
	xdeadline Deadline;
	char sLargeHost[TEST_PROXY_DIAL_OOM_HOST_SIZE + 1u];

	memset(&Context, 0, sizeof(Context));
	Allocator.Context = &Context;
	Allocator.Alloc = testProxyDialOomAlloc;
	Allocator.Realloc = testProxyDialOomRealloc;
	Allocator.Free = testProxyDialOomFree;
	testRequire(xrtSetAllocator(&Allocator),
		"proxy Dial OOM allocator install failed");

	/* 先保留 Proxy，再拒绝握手对象或首个输出块的创建。 */
	pProxy = testProxyDialOomProxy("proxy.test", 10);
	testRequire(pProxy != NULL, "proxy Dial OOM proxy create failed");
	xrtNetProxyHandshakeConfigInit(&HandshakeConfig);
	HandshakeConfig.Proxy = pProxy;
	HandshakeConfig.TargetHost = XRT_STR_LITERAL("origin.test");
	HandshakeConfig.TargetPort = 443;
	xrtAtomic32Store(&Context.Fail, 1, XMEMORY_RELEASE);
	pHandshake = xrtNetProxyHandshakeCreate(&HandshakeConfig);
	testRequire(pHandshake == NULL,
		"proxy handshake unexpectedly survived OOM");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"proxy handshake OOM error mismatch");
	xrtAtomic32Store(&Context.Fail, 0, XMEMORY_RELEASE);
	xrtClearError();

	/* 后续两条组合路径共用一个异步解析器和网络 Engine。 */
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_PROXY_DIAL_BACKEND;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"proxy Dial OOM engine start failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 1;
	ResolverConfig.CacheEntries = 0;
	ResolverConfig.SuccessTTL = 0;
	ResolverConfig.FailureTTL = 0;
	ResolverConfig.Lookup = testProxyDialOomLookup;
	ResolverConfig.LookupData = &Context;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL,
		"proxy Dial OOM resolver create failed");
	xrtNetProxyDialConfigInit(&DialConfig);
	DialConfig.Transport.Family = XNET_FAMILY_IPV4;
	DialConfig.Transport.MaxAttempts = TEST_PROXY_DIAL_OOM_ADDRESS_COUNT;
	DialConfig.Transport.Timeout = 0;
	DialConfig.Transport.Stream.ReadSize = 64;
	DialConfig.Transport.Stream.ReadLimit = 1024;
	DialConfig.Timeout = 0;
	DialConfig.ReceiveLimit = 512;

	/* Engine Hold 必须覆盖组合对象返回前的分配与初始化窗口。 */
	memset(&Setup, 0, sizeof(Setup));
	Setup.Allocator = &Context;
	Setup.Engine = pEngine;
	Setup.Resolver = pResolver;
	Setup.Proxy = pProxy;
	Setup.Config = &DialConfig;
	pSetupThread = xrtThreadCreate(
		testProxyDialSetupThread,
		&Setup,
		0
	);
	testRequire(pSetupThread != NULL,
		"proxy Dial setup thread create failed");
	Deadline = xrtDeadlineAfter(5000000u);
	while ( xrtAtomic64Load(
		&Context.BlockThread,
		XMEMORY_ACQUIRE
	) == 0 ) {
		testRequire(!xrtDeadlineExpired(Deadline),
			"proxy Dial setup thread did not publish its id");
		xrtThreadYield();
	}
	xrtAtomic32Store(&Context.BlockStart, 1, XMEMORY_RELEASE);
	while ( xrtAtomic32Load(
		&Context.BlockEntered,
		XMEMORY_ACQUIRE
	) == 0 ) {
		testRequire(!xrtDeadlineExpired(Deadline),
			"proxy Dial setup allocation did not block");
		xrtThreadYield();
	}
	testRequire(!xrtNetEngineDestroy(pEngine),
		"engine destroy crossed an active proxy Dial setup");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		(xrtNetEngineState(pEngine) == XNET_ENGINE_RUNNING),
		"proxy Dial setup engine lifecycle error mismatch");
	xrtClearError();
	xrtAtomic32Store(&Context.BlockRelease, 1, XMEMORY_RELEASE);
	testRequire(xrtThreadWaitFor(
		pSetupThread,
		UINT64_C(5000000)
	) == XWAIT_OK, "proxy Dial setup thread timed out");
	testRequire((xrtThreadExitCode(pSetupThread) == 0) &&
		(Setup.Dial != NULL),
		"proxy Dial setup did not return a managed object");
	(void)xrtNetProxyDialCancel(Setup.Dial);
	Deadline = xrtDeadlineAfter(5000000u);
	while ( xrtAtomic32Load(&Setup.Done, XMEMORY_ACQUIRE) == 0 ) {
		testRequire(!xrtDeadlineExpired(Deadline),
			"proxy Dial setup cancellation did not complete");
		xrtThreadYield();
	}
	xrtNetProxyDialDestroy(Setup.Dial);
	xrtThreadDestroy(pSetupThread);
	testProxyDialOomWaitResolverIdle(pResolver);
	xrtAtomic32Store(&Context.Fail, 0, XMEMORY_RELEASE);
	xrtClearError();

	/* 1024 字节代理主机让 TCP Dial 深拷贝稳定越过小块缓存。 */
	memset(sLargeHost, 'p', TEST_PROXY_DIAL_OOM_HOST_SIZE);
	sLargeHost[TEST_PROXY_DIAL_OOM_HOST_SIZE] = 0;
	pLargeProxy = testProxyDialOomProxy(
		sLargeHost,
		TEST_PROXY_DIAL_OOM_HOST_SIZE
	);
	testRequire(pLargeProxy != NULL,
		"proxy Dial OOM large proxy create failed");
	xrtAtomic32Store(&Context.Fail, 1, XMEMORY_RELEASE);
	testRequire(xrtNetProxyDial(
		pEngine,
		pResolver,
		pLargeProxy,
		"origin.test",
		443,
		&DialConfig,
		NULL,
		NULL,
		testProxyDialOomDone,
		&Context
	) == NULL, "proxy Dial entry unexpectedly survived OOM");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"proxy Dial entry OOM error mismatch");
	xrtAtomic32Store(&Context.Fail, 0, XMEMORY_RELEASE);
	xrtClearError();
	xrtNetProxyRelease(pLargeProxy);

	/* 解析完成后失败必须经过唯一 Done，并保留底层内存错误原因。 */
	xrtAtomic32Store(
		&Context.FailAfterLookup,
		1,
		XMEMORY_RELEASE
	);
	pDial = xrtNetProxyDial(
		pEngine,
		pResolver,
		pProxy,
		"origin.test",
		443,
		&DialConfig,
		NULL,
		NULL,
		testProxyDialOomDone,
		&Context
	);
	testRequire(pDial != NULL,
		"proxy Dial asynchronous OOM setup failed");
	Deadline = xrtDeadlineAfter(5000000u);
	while ( xrtAtomic32Load(&Context.Done, XMEMORY_ACQUIRE) == 0 ) {
		if ( xrtDeadlineExpired(Deadline) ) {
			xnetresolverstats ResolverStats;
			xnetenginestats EngineStats;

			memset(&ResolverStats, 0, sizeof(ResolverStats));
			memset(&EngineStats, 0, sizeof(EngineStats));
			(void)xrtNetResolverStats(pResolver, &ResolverStats);
			(void)xrtNetEngineStats(pEngine, &EngineStats);
			(void)xrtNetProxyDialStats(pDial, &Stats);
			fprintf(
				stderr,
				"proxy Dial OOM timeout: proxy=%d transport=%d "
				"addresses=%u started=%u failed=%u active=%u "
				"resolver-outstanding=%u resolver-running=%u "
				"resolver-ready=%u engine-live=%llu\n",
				(int)Stats.State,
				(int)Stats.Transport.State,
				(unsigned)Stats.Transport.Addresses,
				(unsigned)Stats.Transport.AttemptsStarted,
				(unsigned)Stats.Transport.AttemptsFailed,
				(unsigned)Stats.Transport.ActiveAttempts,
				(unsigned)ResolverStats.Outstanding,
				(unsigned)ResolverStats.RunningQueries,
				(unsigned)ResolverStats.ReadyCallbacks,
				(unsigned long long)EngineStats.LiveObjects
			);
			testRequire(false,
				"proxy Dial OOM terminal callback timed out");
		}
		xrtThreadYield();
	}
	testRequire(xrtNetProxyDialStats(pDial, &Stats),
		"proxy Dial OOM statistics unavailable");
	pError = xrtNetProxyDialError(pDial);
	if ( !((Context.Result == XNET_RESULT_ERROR) &&
		((Context.ErrorKind == XERR_IO) ||
		 (Context.ErrorKind == XERR_MEMORY)) &&
		(pError != NULL) &&
		(xrtErrorIs(pError, XERR_MEMORY) != NULL) &&
		(Stats.State == XNET_PROXY_DIAL_FAILED) &&
		(Stats.Transport.State == XNET_DIAL_FAILED) &&
		(Stats.Transport.AttemptsFailed ==
		 Stats.Transport.AttemptsStarted) &&
		(Stats.Transport.ActiveAttempts == 0)) ) {
		fprintf(
			stderr,
			"proxy Dial OOM snapshot: result=%d callback-kind=%d "
			"domain=%s code=%d error-kind=%d cause-kind=%d "
			"has-memory=%d state=%d transport=%d "
			"started=%u failed=%u active=%u\n",
			(int)Context.Result,
			(int)Context.ErrorKind,
			pError != NULL ? xrtErrorDomain(pError) : "",
			pError != NULL ? (int)xrtErrorCode(pError) : 0,
			pError != NULL ? (int)xrtErrorKind(pError) : -1,
			xrtErrorCause(pError) != NULL ?
				(int)xrtErrorKind(xrtErrorCause(pError)) : -1,
			(pError != NULL) &&
				(xrtErrorIs(pError, XERR_MEMORY) != NULL),
			(int)Stats.State,
			(int)Stats.Transport.State,
			(unsigned)Stats.Transport.AttemptsStarted,
			(unsigned)Stats.Transport.AttemptsFailed,
			(unsigned)Stats.Transport.ActiveAttempts
		);
		testProxyDialOomPrintError(pError);
	}
	testRequire((Context.Result == XNET_RESULT_ERROR) &&
		((Context.ErrorKind == XERR_IO) ||
		 (Context.ErrorKind == XERR_MEMORY)) &&
		(pError != NULL) &&
		(xrtErrorIs(pError, XERR_MEMORY) != NULL) &&
		(Stats.State == XNET_PROXY_DIAL_FAILED) &&
		(Stats.Transport.State == XNET_DIAL_FAILED) &&
		(Stats.Transport.AttemptsFailed ==
		 Stats.Transport.AttemptsStarted) &&
		(Stats.Transport.ActiveAttempts == 0),
		"proxy Dial OOM terminal or statistics mismatch");

	/* 恢复分配后释放全部持有，Engine 不得残留 Timer、Dial 或 Stream。 */
	xrtAtomic32Store(&Context.Fail, 0, XMEMORY_RELEASE);
	xrtNetProxyDialDestroy(pDial);
	xrtNetProxyRelease(pProxy);
	testRequire(xrtNetResolverDestroy(pResolver),
		"proxy Dial OOM resolver destroy failed");
	Deadline = xrtDeadlineAfter(5000000u);
	while ( !xrtNetEngineDestroy(pEngine) ) {
		xrtClearError();
		testRequire(!xrtDeadlineExpired(Deadline),
			"proxy Dial OOM retained an internal engine resource");
		xrtThreadYield();
	}
	testRequire(xrtAtomic64Load(
		&Context.Allocations,
		XMEMORY_ACQUIRE
	) != 0, "proxy Dial OOM allocator was not exercised");
	printf("[PASS] managed proxy Dial OOM cleanup\n");
	return 0;
}
