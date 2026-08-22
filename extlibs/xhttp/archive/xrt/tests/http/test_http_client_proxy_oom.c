#include "../test.h"



#define TEST_HTTP_PROXY_OOM_HOST_SIZE 1024u

#if defined(XRT_FEATURE_HTTP_CLIENT_POOL)
	#define TEST_HTTP_PROXY_OOM_EXPECTED_LOOKUPS 1u
#else
	#define TEST_HTTP_PROXY_OOM_EXPECTED_LOOKUPS 0u
#endif



/* 单点分配器与 Worker 屏障共同确定 Proxy Dial 构造失败位置。 */
typedef struct test_http_proxy_oom {
	xatomic32 Armed;
	xatomic32 Failed;
	xatomic32 Blocked;
	xatomic32 Release;
	xatomic32 Completed;
	xatomic32 Lookups;
	xatomic64 Allocations;
	xhttpcall* Call;
	bool ResultValid;
	#if defined(XRT_FEATURE_HTTP_CLIENT_POOL)
		xatomic32 WarmLookup;
		xatomic32 WarmLookupRelease;
		xatomic32 WarmCompleted;
		xhttpcall* WarmCall;
		bool WarmValid;
	#endif
} test_http_proxy_oom;



/* 武装后只拒绝第一份新内存，其余分配用于构造稳定错误链。 */
static bool testHttpProxyOomFail(
	test_http_proxy_oom* pState
)
{
	uint32 iExpected = 0;

	return xrtAtomic32Load(
		&pState->Armed,
		XMEMORY_ACQUIRE
	) && xrtAtomic32CompareExchange(
		&pState->Failed,
		&iExpected,
		1,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	);
}



/* 转发普通分配并在确定位置注入一次内存不足。 */
static ptr testHttpProxyOomAlloc(
	ptr pData,
	size_t iSize
)
{
	test_http_proxy_oom* pState =
		(test_http_proxy_oom*)pData;

	(void)xrtAtomic64FetchAdd(
		&pState->Allocations,
		1,
		XMEMORY_RELAXED
	);
	return testHttpProxyOomFail(pState) ?
		NULL : malloc(iSize);
}



/* 普通重分配保持 C 语义，并共享同一个单点故障门。 */
static ptr testHttpProxyOomRealloc(
	ptr pData,
	ptr pMemory,
	size_t iSize
)
{
	test_http_proxy_oom* pState =
		(test_http_proxy_oom*)pData;

	(void)xrtAtomic64FetchAdd(
		&pState->Allocations,
		1,
		XMEMORY_RELAXED
	);
	return testHttpProxyOomFail(pState) ?
		NULL : realloc(pMemory, iSize);
}



/* 释放测试分配器产生的底层内存。 */
static void testHttpProxyOomFree(
	ptr pData,
	ptr pMemory
)
{
	(void)pData;
	free(pMemory);
}



/* 预热唯一 Worker 的堆缓存，再等待主线程提交 Call 并武装故障点。 */
static void testHttpProxyOomBlock(
	xnetworker* pWorker,
	ptr pData
)
{
	test_http_proxy_oom* pState =
		(test_http_proxy_oom*)pData;
	ptr pWarmup;

	(void)pWorker;
	pWarmup = xrtMalloc(1u);
	testRequire(
		pWarmup != NULL,
		"HTTP proxy OOM Worker heap warm-up failed"
	);
	xrtFree(pWarmup);
	xrtAtomic32Store(
		&pState->Blocked,
		1,
		XMEMORY_RELEASE
	);
	while ( xrtAtomic32Load(
		&pState->Release,
		XMEMORY_ACQUIRE
	) == 0 ) {
		xrtThreadYield();
	}
}



/* 在截止时间内等待 Worker 或 Call 发布状态。 */
static void testHttpProxyOomWait(
	const xatomic32* pValue,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(5000000u);

	while ( xrtAtomic32Load(
		pValue,
		XMEMORY_ACQUIRE
	) == 0 ) {
		testRequire(
			!xrtDeadlineExpired(Deadline),
			sMessage
		);
		xrtThreadYield();
	}
}



/* 该路径不应到达 Resolver；发生调用说明 Proxy Dial 已越过构造故障。 */
static xnetaddrlist* testHttpProxyOomLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	test_http_proxy_oom* pState =
		(test_http_proxy_oom*)pData;
	#if defined(XRT_FEATURE_HTTP_CLIENT_POOL)
		uint32 iLookup;
	#endif

	(void)sHost;
	(void)Family;
	#if defined(XRT_FEATURE_HTTP_CLIENT_POOL)
		iLookup = xrtAtomic32FetchAdd(
			&pState->Lookups,
			1,
			XMEMORY_ACQ_REL
		);
		if ( iLookup == 0 ) {
			xrtAtomic32Store(
				&pState->WarmLookup,
				1,
				XMEMORY_RELEASE
			);
			while ( xrtAtomic32Load(
				&pState->WarmLookupRelease,
				XMEMORY_ACQUIRE
			) == 0 ) {
				xrtThreadYield();
			}
		}
	#else
		(void)xrtAtomic32FetchAdd(
			&pState->Lookups,
			1,
			XMEMORY_RELEASE
		);
	#endif
	return NULL;
}



#if defined(XRT_FEATURE_HTTP_CLIENT_POOL)

/* 预备 Call 只负责占住同一 Origin，结束时验证它仍按普通解析失败收口。 */
static void testHttpProxyOomWarmDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	test_http_proxy_oom* pState =
		(test_http_proxy_oom*)pData;

	pState->WarmValid =
		(pCall == pState->WarmCall) &&
		(pResult != NULL) &&
		(pResult->Result == XNET_RESULT_ERROR) &&
		(pResult->Response == NULL) &&
		(pResult->Tcp == NULL) &&
		(pResult->Error != NULL) &&
		!pResult->Upgraded &&
		(xrtHttpCallState(pCall) == XHTTP_CALL_FAILED);
	xrtAtomic32Store(
		&pState->WarmCompleted,
		1,
		XMEMORY_RELEASE
	);
}

#endif



/* 验证底层内存失败被高层代理错误完整包装且没有半成品传输。 */
static void testHttpProxyOomDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	test_http_proxy_oom* pState =
		(test_http_proxy_oom*)pData;

	pState->ResultValid =
		(pCall == pState->Call) &&
		(pResult != NULL) &&
		(pResult->Result == XNET_RESULT_ERROR) &&
		(pResult->Response == NULL) &&
		(pResult->Tcp == NULL) &&
		(pResult->Error != NULL) &&
		!pResult->Upgraded &&
		(strcmp(
			xrtErrorDomain(pResult->Error),
			"xrt.http.client"
		) == 0) &&
		(xrtErrorKind(pResult->Error) ==
		 XERR_MEMORY) &&
		(xrtErrorCode(pResult->Error) ==
		 XHTTP_CLIENT_ERROR_PROXY) &&
		(strcmp(
			xrtErrorOperation(pResult->Error),
			"dial-http-proxy"
		) == 0) &&
		(xrtErrorIs(
			pResult->Error,
			XERR_MEMORY
		) != NULL) &&
		(xrtHttpCallState(pCall) ==
		 XHTTP_CALL_FAILED);
	if ( !pState->ResultValid ) {
		fprintf(
			stderr,
			"[DIAG] HTTP proxy OOM result=%d response=%p tcp=%p "
			"error=%p state=%d domain=%s kind=%d code=%d operation=%s\n",
			pResult != NULL ? (int)pResult->Result : -1,
			pResult != NULL ? (void*)pResult->Response : NULL,
			pResult != NULL ? (void*)pResult->Tcp : NULL,
			pResult != NULL ? (void*)pResult->Error : NULL,
			(int)xrtHttpCallState(pCall),
			(pResult != NULL) && (pResult->Error != NULL) ?
				xrtErrorDomain(pResult->Error) : "",
			(pResult != NULL) && (pResult->Error != NULL) ?
				(int)xrtErrorKind(pResult->Error) : -1,
			(pResult != NULL) && (pResult->Error != NULL) ?
				(int)xrtErrorCode(pResult->Error) : -1,
			(pResult != NULL) && (pResult->Error != NULL) ?
				xrtErrorOperation(pResult->Error) : ""
		);
	}
	xrtAtomic32Store(
		&pState->Completed,
		1,
		XMEMORY_RELEASE
	);
}



/* 精确覆盖高层代理适配器在 Proxy Dial 构造 OOM 下的回滚。 */
int main(void)
{
	static test_http_proxy_oom State;
	xallocator Allocator;
	xnetengineconfig EngineConfig;
	xhttpclientconfig ClientConfig;
	xhttpcalloptions Options;
	xnetproxyconfig ProxyConfig;
	xnetenginestats EngineStats;
	xnetengine* pEngine;
	xhttpclient* pClient;
	xnetproxy* pProxy;
	xhttprequest* pRequest;
	char Url[TEST_HTTP_PROXY_OOM_HOST_SIZE + 16u];
	size_t iUrlSize;

	xrtAtomic32Init(&State.Armed, 0);
	xrtAtomic32Init(&State.Failed, 0);
	xrtAtomic32Init(&State.Blocked, 0);
	xrtAtomic32Init(&State.Release, 0);
	xrtAtomic32Init(&State.Completed, 0);
	xrtAtomic32Init(&State.Lookups, 0);
	xrtAtomic64Init(&State.Allocations, 0);
	#if defined(XRT_FEATURE_HTTP_CLIENT_POOL)
		xrtAtomic32Init(&State.WarmLookup, 0);
		xrtAtomic32Init(&State.WarmLookupRelease, 0);
		xrtAtomic32Init(&State.WarmCompleted, 0);
	#endif
	Allocator.Context = &State;
	Allocator.Alloc = testHttpProxyOomAlloc;
	Allocator.Realloc = testHttpProxyOomRealloc;
	Allocator.Free = testHttpProxyOomFree;
	testRequire(
		xrtSetAllocator(&Allocator),
		"HTTP proxy OOM allocator install failed"
	);

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pEngine != NULL) &&
		xrtNetEngineStart(pEngine),
		"HTTP proxy OOM engine start failed"
	);

	xrtNetProxyConfigInit(&ProxyConfig);
	ProxyConfig.Type = XNET_PROXY_HTTP_CONNECT;
	ProxyConfig.Host = XRT_STR_LITERAL("proxy.test");
	ProxyConfig.Port = 1080;
	pProxy = xrtNetProxyCreate(&ProxyConfig);
	testRequire(
		pProxy != NULL,
		"HTTP proxy OOM endpoint creation failed"
	);
	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Proxy = pProxy;
	ClientConfig.Resolver.Lookup =
		testHttpProxyOomLookup;
	ClientConfig.Resolver.LookupData = &State;
	pClient = xrtHttpClientCreate(
		pEngine,
		&ClientConfig
	);
	testRequire(
		pClient != NULL,
		"HTTP proxy OOM client creation failed"
	);
	xrtNetProxyRelease(pProxy);

	/*
		HTTP CONNECT 可承载统一主机上限长度的 authority。
		目标副本附加终止符后超过池化上限，保证命中直接分配故障。
	*/
	memcpy(Url, "http://", 7u);
	memset(
		Url + 7u,
		'a',
		TEST_HTTP_PROXY_OOM_HOST_SIZE
	);
	memcpy(
		Url + 7u + TEST_HTTP_PROXY_OOM_HOST_SIZE,
		"/oom",
		5u
	);
	iUrlSize = 7u + TEST_HTTP_PROXY_OOM_HOST_SIZE + 4u;
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		(xstrview){ Url, iUrlSize }
	);
	testRequire(
		pRequest != NULL,
		"HTTP proxy OOM request creation failed"
	);
	xrtHttpCallOptionsInit(&Options);
	Options.Timeout = XHTTP_CLIENT_TIMEOUT_NONE;
	Options.IdleTimeout = XHTTP_CLIENT_TIMEOUT_NONE;

	/*
		完整构建启用连接池时，先让预备 Call 占住同一 Origin。
		Resolver 屏障使 Origin 元数据稳定存活到目标 Call 进入 Proxy Dial。
	*/
	#if defined(XRT_FEATURE_HTTP_CLIENT_POOL)
		State.WarmCall = xrtHttpClientDo(
			pClient,
			pRequest,
			&Options,
			testHttpProxyOomWarmDone,
			&State
		);
		testRequire(
			State.WarmCall != NULL,
			"HTTP proxy OOM warm Call submission failed"
		);
		testHttpProxyOomWait(
			&State.WarmLookup,
			"HTTP proxy OOM warm Call did not reach resolver"
		);
	#endif

	testRequire(
		xrtNetEnginePost(
			pEngine,
			0,
			testHttpProxyOomBlock,
			&State
		),
		"HTTP proxy OOM blocker post failed"
	);
	testHttpProxyOomWait(
		&State.Blocked,
		"HTTP proxy OOM blocker did not start"
	);

	State.Call = xrtHttpClientDo(
		pClient,
		pRequest,
		&Options,
		testHttpProxyOomDone,
		&State
	);
	xrtHttpRequestDestroy(pRequest);
	testRequire(
		State.Call != NULL,
		"HTTP proxy OOM Call submission failed"
	);

	xrtAtomic32Store(
		&State.Armed,
		1,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&State.Release,
		1,
		XMEMORY_RELEASE
	);
	testHttpProxyOomWait(
		&State.Completed,
		"HTTP proxy OOM Call did not complete"
	);
	#if defined(XRT_FEATURE_HTTP_CLIENT_POOL)
		xrtAtomic32Store(
			&State.WarmLookupRelease,
			1,
			XMEMORY_RELEASE
		);
		testHttpProxyOomWait(
			&State.WarmCompleted,
			"HTTP proxy OOM warm Call did not complete"
		);
	#endif
	if (
		!State.ResultValid ||
		(xrtAtomic32Load(
			&State.Failed,
			XMEMORY_ACQUIRE
		) != 1) ||
		(xrtAtomic32Load(
			&State.Lookups,
			XMEMORY_ACQUIRE
		) != TEST_HTTP_PROXY_OOM_EXPECTED_LOOKUPS)
		#if defined(XRT_FEATURE_HTTP_CLIENT_POOL)
			|| !State.WarmValid
		#endif
	) {
		fprintf(
			stderr,
			"[DIAG] HTTP proxy OOM valid=%d failed=%u lookups=%u "
			"allocations=%llu\n",
			State.ResultValid ? 1 : 0,
			(unsigned)xrtAtomic32Load(
				&State.Failed,
				XMEMORY_ACQUIRE
			),
			(unsigned)xrtAtomic32Load(
				&State.Lookups,
				XMEMORY_ACQUIRE
			),
			(unsigned long long)xrtAtomic64Load(
				&State.Allocations,
				XMEMORY_ACQUIRE
			)
		);
	}
	testRequire(
		State.ResultValid &&
		(xrtAtomic32Load(
			&State.Failed,
			XMEMORY_ACQUIRE
		) == 1) &&
		(xrtAtomic32Load(
			&State.Lookups,
			XMEMORY_ACQUIRE
		) == TEST_HTTP_PROXY_OOM_EXPECTED_LOOKUPS) &&
		#if defined(XRT_FEATURE_HTTP_CLIENT_POOL)
			State.WarmValid &&
		#endif
		!xrtHttpCallCancel(State.Call),
		"HTTP proxy OOM terminal contract mismatch"
	);

	xrtHttpCallDestroy(State.Call);
	#if defined(XRT_FEATURE_HTTP_CLIENT_POOL)
		xrtHttpCallDestroy(State.WarmCall);
	#endif
	xrtHttpClientDestroy(pClient);
	xrtClearError();
	testRequire(
		xrtNetEngineStats(pEngine, &EngineStats) &&
		(EngineStats.LiveObjects == 0) &&
		xrtNetEngineDestroy(pEngine),
		"HTTP proxy OOM resources did not drain"
	);
	testRequire(
		xrtAtomic64Load(
			&State.Allocations,
			XMEMORY_ACQUIRE
		) != 0,
		"HTTP proxy OOM allocator was not exercised"
	);
	printf("[PASS] high-level HTTP proxy OOM\n");
	return 0;
}
