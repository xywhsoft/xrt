#include "../test.h"
#include "../fixtures/http_origin.h"



/* 同步构造 OOM 夹具只记录不应发生的异步 Close。 */
typedef struct test_http_sse_client_oom {
	xatomic32 Closed;
} test_http_sse_client_oom;



/* 暂停尾段 OOM 夹具保存唯一终态和消息计数。 */
typedef struct test_http_sse_tail_oom {
	xatomic32 Messages;
	xatomic32 Closed;
	xhttpsseclosereason Reason;
	xhttpsseclienterror Error;
	xerrkind Kind;
	bool HasCause;
	bool Triggered;
} test_http_sse_tail_oom;



/* 最小消息回调只允许同步构造扫描通过事件校验。 */
static bool testHttpSseClientOomMessage(
	xhttpsseclient* pClient,
	const xhttpssemessage* pMessage,
	ptr pData
)
{
	(void)pClient;
	(void)pMessage;
	(void)pData;
	return true;
}



/* 同步失败不得进入异步终态回调。 */
static void testHttpSseClientOomClose(
	xhttpsseclient* pClient,
	xhttpsseclosereason Reason,
	const xerror* pError,
	ptr pData
)
{
	test_http_sse_client_oom* pState =
		(test_http_sse_client_oom*)pData;

	(void)pClient;
	(void)Reason;
	(void)pError;
	xrtAtomic32Store(
		&pState->Closed,
		1,
		XMEMORY_RELEASE
	);
}



/* 在一个逻辑分配点注入故障，并完整销毁全部基线对象。 */
static bool testHttpSseClientOomConstructAttempt(
	size_t iFail
)
{
	test_http_sse_client_oom State;
	xnetengineconfig EngineConfig;
	xhttpclientconfig HttpConfig;
	xhttpsseclientconfig Config;
	xhttpsseclientevents Events;
	xnetengine* pEngine;
	xhttpclient* pHttp;
	xhttprequest* pRequest;
	xhttpsseclient* pClient;
	xcancel* pCancel;
	bool bTriggered;

	memset(&State, 0, sizeof(State));
	xrtAtomic32Init(&State.Closed, 0);
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pEngine != NULL) &&
		xrtNetEngineStart(pEngine),
		"SSE client OOM Engine start failed"
	);
	xrtHttpClientConfigInit(&HttpConfig);
	pHttp = xrtHttpClientCreate(
		pEngine, &HttpConfig
	);
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://oom.test/events")
	);
	pCancel = xrtCancelCreate();
	testRequire(
		(pHttp != NULL) &&
		(pRequest != NULL) &&
		(pCancel != NULL) &&
		xrtCancelRequest(pCancel),
		"SSE client OOM baseline creation failed"
	);
	xrtHttpSseClientConfigInit(&Config);
	Config.Http.Cancel = pCancel;
	memset(&Events, 0, sizeof(Events));
	Events.Message = testHttpSseClientOomMessage;
	Events.Close = testHttpSseClientOomClose;
	Events.Data = &State;
	testRequire(
		xrtMemDebugFailAfter((uint64)iFail),
		"SSE client OOM fault setup failed"
	);
	pClient = xrtHttpSseConnectRequest(
		pHttp,
		pRequest,
		&Config,
		&Events
	);
	bTriggered = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	testRequire(
		(pClient == NULL) &&
		(xrtAtomic32Load(
			&State.Closed,
			XMEMORY_ACQUIRE
		) == 0) &&
		(xrtGetError() != NULL),
		"SSE client OOM construction contract mismatch"
	);
	xrtClearError();
	xrtCancelDestroy(pCancel);
	xrtHttpRequestDestroy(pRequest);
	xrtHttpClientDestroy(pHttp);
	testRequire(
		xrtNetEngineDestroy(pEngine),
		"SSE client OOM Engine destroy failed"
	);
	testMemoryDebugDrain(
		"SSE client OOM construction leaked storage"
	);
	return !bTriggered;
}



/* 逐个覆盖会话、请求、HTTP 选项和取消监听的同步分配点。 */
static void testHttpSseClientOomConstruct(void)
{
	for ( size_t i = 0; i < 64u; i++ ) {
		if ( testHttpSseClientOomConstructAttempt(i) ) {
			testRequire(
				i >= 4u,
				"SSE client OOM scan missed construction allocations"
			);
			return;
		}
	}
	testRequire(
		false,
		"SSE client OOM construction scan did not converge"
	);
}



/* 为本地 origin 返回 IPv4 环回地址。 */
static xnetaddrlist* testHttpSseTailOomLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)pData;
	testRequire(
		strcmp(sHost, "tail-oom.test") == 0,
		"SSE tail OOM resolved an unexpected host"
	);
	if ( Family == XNET_FAMILY_IPV6 ) {
		return xrtNetAddrListCreate(NULL, 0);
	}
	testRequire(
		xrtNetAddrLoopback(
			&Address,
			XNET_FAMILY_IPV4,
			0
		),
		"SSE tail OOM loopback address failed"
	);
	return xrtNetAddrListCreate(&Address, 1);
}



/* 首条消息内暂停并拒绝下一次分配，精确命中尾段复制。 */
static bool testHttpSseTailOomMessage(
	xhttpsseclient* pClient,
	const xhttpssemessage* pMessage,
	ptr pData
)
{
	test_http_sse_tail_oom* pState =
		(test_http_sse_tail_oom*)pData;
	uint32 iOld = xrtAtomic32FetchAdd(
		&pState->Messages,
		1,
		XMEMORY_ACQ_REL
	);

	testRequire(
		(iOld == 0) &&
		(pMessage->Data.Size == 1u) &&
		(pMessage->Data.Data[0] == 'A') &&
		xrtMemDebugFailAfter(0) &&
		xrtHttpSseClientPause(pClient),
		"SSE tail OOM pause setup failed"
	);
	return true;
}



/* 复制尾段分配失败的唯一 INTERNAL 终态。 */
static void testHttpSseTailOomClose(
	xhttpsseclient* pClient,
	xhttpsseclosereason Reason,
	const xerror* pError,
	ptr pData
)
{
	test_http_sse_tail_oom* pState =
		(test_http_sse_tail_oom*)pData;

	(void)pClient;
	pState->Reason = Reason;
	if ( pError != NULL ) {
		pState->Error =
			(xhttpsseclienterror)xrtErrorCode(pError);
		pState->Kind = xrtErrorKind(pError);
		pState->HasCause = xrtErrorCause(pError) != NULL;
	}
	pState->Triggered = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	xrtAtomic32Store(
		&pState->Closed,
		1,
		XMEMORY_RELEASE
	);
}



/* 覆盖暂停后仅复制当前输入尾段的唯一动态分配失败。 */
static void testHttpSseClientTailOom(void)
{
	static const char Response[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: text/event-stream\r\n"
		"Content-Length: 18\r\n"
		"Connection: close\r\n\r\n"
		"data: A\n\n"
		"data: B\n\n";
	test_http_sse_tail_oom State;
	testhttporigin Origin;
	xnetengineconfig EngineConfig;
	xhttpclientconfig HttpConfig;
	xhttpsseclientconfig Config;
	xhttpsseclientevents Events;
	xnetengine* pEngine;
	xhttpclient* pHttp;
	xhttpsseclient* pClient;
	char Url[128];
	int iLength;

	memset(&State, 0, sizeof(State));
	xrtAtomic32Init(&State.Messages, 0);
	xrtAtomic32Init(&State.Closed, 0);
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pEngine != NULL) &&
		xrtNetEngineStart(pEngine),
		"SSE tail OOM Engine start failed"
	);
	testHttpOriginStart(
		&Origin,
		pEngine,
		Response,
		sizeof(Response) - 1u
	);
	testHttpOriginSplitResponse(
		&Origin,
		(sizeof(Response) - 1u) - 18u,
		UINT64_C(100000)
	);
	xrtHttpClientConfigInit(&HttpConfig);
	HttpConfig.Resolver.Lookup =
		testHttpSseTailOomLookup;
	#if defined(XRT_FEATURE_HTTP_CLIENT_POOL)
		HttpConfig.Pool.MaxIdle = 0;
	#endif
	pHttp = xrtHttpClientCreate(
		pEngine, &HttpConfig
	);
	testRequire(
		pHttp != NULL,
		"SSE tail OOM HTTP runtime creation failed"
	);
	iLength = snprintf(
		Url,
		sizeof(Url),
		"http://tail-oom.test:%u/events",
		(unsigned int)testHttpOriginPort(&Origin)
	);
	testRequire(
		(iLength > 0) &&
		((size_t)iLength < sizeof(Url)),
		"SSE tail OOM URL overflowed"
	);
	xrtHttpSseClientConfigInit(&Config);
	Config.MaxReconnects = 0;
	memset(&Events, 0, sizeof(Events));
	Events.Message = testHttpSseTailOomMessage;
	Events.Close = testHttpSseTailOomClose;
	Events.Data = &State;
	pClient = xrtHttpSseConnect(
		pHttp,
		(xstrview){ Url, (size_t)iLength },
		&Config,
		&Events
	);
	testRequire(
		pClient != NULL,
		"SSE tail OOM connection submission failed"
	);
	testHttpOriginWait(
		&State.Closed,
		1,
		"SSE tail OOM terminal state was not published"
	);
	testRequire(
		State.Triggered,
		"SSE tail OOM did not hit the injected allocation"
	);
	testRequire(
		xrtAtomic32Load(
			&State.Messages,
			XMEMORY_ACQUIRE
		) == 1u,
		"SSE tail OOM delivered an unexpected message count"
	);
	testRequire(
		State.Reason == XHTTP_SSE_CLOSE_INTERNAL,
		"SSE tail OOM close reason mismatch"
	);
	testRequire(
		State.Error == XHTTP_SSE_CLIENT_ERROR_INTERNAL,
		"SSE tail OOM error code mismatch"
	);
	testRequire(
		State.Kind == XERR_MEMORY,
		"SSE tail OOM error kind mismatch"
	);
	testRequire(
		State.HasCause,
		"SSE tail OOM did not preserve the allocation cause"
	);
	xrtClearError();
	xrtHttpSseClientDestroy(pClient);
	xrtHttpClientDestroy(pHttp);
	testHttpOriginStop(&Origin);
	testRequire(
		xrtNetEngineDestroy(pEngine),
		"SSE tail OOM Engine destroy failed"
	);
	testMemoryDebugDrain(
		"SSE tail OOM test leaked storage"
	);
}



/* 运行 SSE Client 同步构造和异步尾段的故障注入回归。 */
int main(void)
{
	testHttpSseClientOomConstruct();
	testHttpSseClientTailOom();
	printf("[PASS] HTTP SSE client OOM\n");
	return 0;
}
