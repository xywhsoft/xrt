#include "../test.h"



/* 契约夹具只跨线程保存不可变终态，不借用回调临时结果。 */
typedef struct test_http_client_contract {
	xatomic32 Completed;
	xnetresult Result;
	xhttpcallstate State;
	xhttpclienterror Error;
	bool EmptyResponse;
} test_http_client_contract;



/* 预取消若错误进入 Resolver，记录越界行为并返回空结果。 */
static xnetaddrlist* testHttpClientContractLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xatomic32* pLookups = (xatomic32*)pData;

	(void)sHost;
	(void)Family;
	(void)xrtAtomic32FetchAdd(
		pLookups,
		1,
		XMEMORY_ACQ_REL
	);
	return xrtNetAddrListCreate(NULL, 0);
}



/* 验证当前线程错误已经提升为指定的 HTTP Client 分类。 */
static void testHttpClientContractError(
	xhttpclienterror Code,
	bool bCause
)
{
	const xerror* pError = xrtGetError();

	testRequire(
		(pError != NULL) &&
		(strcmp(
			xrtErrorDomain(pError),
			"xrt.http.client"
		) == 0) &&
		(xrtErrorCode(pError) == (int32)Code) &&
		(!bCause || (xrtErrorCause(pError) != NULL)),
		"HTTP client public error was not promoted"
	);
	xrtClearError();
}



/* 预取消完成回调只复制终态，Call 和错误仍由调用方持有。 */
static void testHttpClientContractDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	test_http_client_contract* pState =
		(test_http_client_contract*)pData;

	testRequire(
		(pCall != NULL) &&
		(pResult != NULL) &&
		(pResult->Error != NULL),
		"HTTP client pre-cancel completion is incomplete"
	);
	pState->Result = pResult->Result;
	pState->State = pResult->Info.State;
	pState->Error = pResult->Info.Error;
	pState->EmptyResponse =
		(pResult->Response == NULL) &&
		(pResult->Tcp == NULL);
	xrtAtomic32Store(
		&pState->Completed,
		1,
		XMEMORY_RELEASE
	);
}



/* 同步拒绝路径若错误触发回调，只记录事实供主线程断言。 */
static void testHttpClientContractUnexpectedDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	xatomic32* pDone = (xatomic32*)pData;

	(void)pCall;
	(void)pResult;
	xrtAtomic32Store(pDone, 1, XMEMORY_RELEASE);
}



/* 在截止时间前等待预取消 Call 发布终态。 */
static void testHttpClientContractWait(
	const xatomic32* pCompleted
)
{
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(5000000));

	while ( xrtAtomic32Load(
		pCompleted,
		XMEMORY_ACQUIRE
	) == 0 ) {
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"HTTP client pre-cancel Call did not complete"
		);
		xrtThreadYield();
	}
}



/* 确认裁剪可选策略后，公开阶段码和错误码仍保留固定编号。 */
static void testHttpClientContractStableCodes(void)
{
	testRequire(
		(XHTTP_CALL_PHASE_QUEUED == 0) &&
		(XHTTP_CALL_PHASE_POOL == 3) &&
		(XHTTP_CALL_PHASE_CONNECT == 4) &&
		(XHTTP_CALL_PHASE_PROXY == 5) &&
		(XHTTP_CALL_PHASE_TLS == 6) &&
		(XHTTP_CALL_PHASE_REQUEST == 7) &&
		(XHTTP_CALL_PHASE_RESPONSE_HEADERS == 8) &&
		(XHTTP_CALL_PHASE_RESPONSE_BODY == 9),
		"HTTP client phase codes changed with trimming"
	);
	testRequire(
		(XHTTP_CLIENT_ERROR_NONE == 0) &&
		(XHTTP_CLIENT_ERROR_ARGUMENT == 1) &&
		(XHTTP_CLIENT_ERROR_RESPONSE == 5) &&
		(XHTTP_CLIENT_ERROR_POOL == 7) &&
		(XHTTP_CLIENT_ERROR_DIAL == 8) &&
		(XHTTP_CLIENT_ERROR_REDIRECT_DOWNGRADE == 20) &&
		(XHTTP_CLIENT_ERROR_COOKIE == 22) &&
		(XHTTP_CLIENT_ERROR_DECOMPRESSION == 23) &&
		(XHTTP_CLIENT_ERROR_INTERNAL == 24),
		"HTTP client error codes changed with trimming"
	);
	#if defined(XRT_FEATURE_HTTP_CLIENT_CACHE)
		testRequire(
			(XHTTP_CALL_PHASE_CACHE == 1) &&
			(XHTTP_CLIENT_ERROR_CACHE == 6),
			"HTTP client cache codes changed"
		);
	#endif
	#if defined(XRT_FEATURE_HTTP_CLIENT_RETRY)
		testRequire(
			(XHTTP_CALL_PHASE_RETRY == 2) &&
			(XHTTP_CLIENT_ERROR_RETRY == 21),
			"HTTP client retry codes changed"
		);
	#endif
}



/* 验证暂停控制拒绝空句柄，并发布统一的高层参数错误。 */
static void testHttpClientContractPauseArgument(void)
{
	testRequire(
		!xrtHttpCallPause(NULL),
		"HTTP client pause accepted a null Call"
	);
	testHttpClientContractError(
		XHTTP_CLIENT_ERROR_ARGUMENT,
		false
	);
	testRequire(
		!xrtHttpCallResume(NULL),
		"HTTP client resume accepted a null Call"
	);
	testHttpClientContractError(
		XHTTP_CLIENT_ERROR_ARGUMENT,
		false
	);
	testRequire(
		!xrtHttpCallPaused(NULL),
		"HTTP client pause query accepted a null Call"
	);
	testHttpClientContractError(
		XHTTP_CLIENT_ERROR_ARGUMENT,
		false
	);
	testRequire(
		xrtHttpCallWorker(NULL) == NULL,
		"HTTP client Worker query accepted a null Call"
	);
	testHttpClientContractError(
		XHTTP_CLIENT_ERROR_ARGUMENT,
		false
	);
	testRequire(
		xrtHttpCallRequestClone(NULL) == NULL,
		"HTTP client request clone accepted a null Call"
	);
	testHttpClientContractError(
		XHTTP_CLIENT_ERROR_ARGUMENT,
		false
	);
}



/* Client 创建必须立即拒绝其拥有的全部静态子配置。 */
static void testHttpClientContractConfig(xnetengine* pEngine)
{
	static uint8 Storage[sizeof(xhttpclientconfig) + 1u];
	xhttpclientconfig Config;
	xhttpclientconfig* pUnaligned =
		(xhttpclientconfig*)(Storage + 1u);
	xhttpclient* pClient;

	xrtHttpClientConfigInit(pUnaligned);
	memcpy(&Config, pUnaligned, sizeof(Config));
	testRequire(
		(Config.Timeout ==
		 XHTTP_CLIENT_TIMEOUT_DEFAULT) &&
		(Config.IdleTimeout ==
		 XHTTP_CLIENT_IDLE_TIMEOUT_DEFAULT),
		"HTTP client unaligned config init mismatch"
	);
	pClient = xrtHttpClientCreate(pEngine, pUnaligned);
	testRequire(
		pClient != NULL,
		"HTTP client rejected an unaligned config snapshot"
	);
	xrtHttpClientDestroy(pClient);

	xrtHttpClientConfigInit(
		(xhttpclientconfig*)(UINTPTR_MAX - 1u)
	);
	testHttpClientContractError(
		XHTTP_CLIENT_ERROR_ARGUMENT,
		false
	);
	pClient = xrtHttpClientCreate(
		pEngine,
		(const xhttpclientconfig*)(UINTPTR_MAX - 1u)
	);
	testRequire(
		pClient == NULL,
		"HTTP client accepted a wrapping config range"
	);
	testHttpClientContractError(
		XHTTP_CLIENT_ERROR_ARGUMENT,
		false
	);

	xrtHttpClientConfigInit(&Config);
	Config.Dial.MaxAttempts = 0;
	pClient = xrtHttpClientCreate(pEngine, &Config);
	testRequire(
		pClient == NULL,
		"HTTP client accepted an invalid Dial policy"
	);
	testHttpClientContractError(
		XHTTP_CLIENT_ERROR_CONFIG,
		true
	);

	xrtHttpClientConfigInit(&Config);
	Config.Call.WriteSize = 0;
	pClient = xrtHttpClientCreate(pEngine, &Config);
	testRequire(
		pClient == NULL,
		"HTTP client accepted an invalid transport policy"
	);
	testHttpClientContractError(
		XHTTP_CLIENT_ERROR_CONFIG,
		true
	);

	xrtHttpClientConfigInit(&Config);
	Config.Exchange.Head.MaxHead = 0;
	pClient = xrtHttpClientCreate(pEngine, &Config);
	testRequire(
		pClient == NULL,
		"HTTP client accepted an invalid response policy"
	);
	testHttpClientContractError(
		XHTTP_CLIENT_ERROR_CONFIG,
		true
	);

	xrtHttpClientConfigInit(&Config);
	Config.Resolver.Workers = 0;
	pClient = xrtHttpClientCreate(pEngine, &Config);
	testRequire(
		pClient == NULL,
		"HTTP client accepted an invalid Resolver policy"
	);
	testHttpClientContractError(
		XHTTP_CLIENT_ERROR_CONFIG,
		true
	);
}



/* 请求冻结或 HTTP/1 计划失败必须同步返回稳定高层错误。 */
static void testHttpClientContractSubmit(
	xhttpclient* pClient
)
{
	static const char Body[] = "abc";
	static uint8 Storage[sizeof(xhttpcalloptions) + 1u];
	xhttpcalloptions Options;
	xhttpcalloptions* pUnaligned =
		(xhttpcalloptions*)(Storage + 1u);
	xhttprequest* pRequest;
	xhttpcall* pCall;
	xatomic32 Done;

	xrtAtomic32Init(&Done, 0);
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("http://submit.test/resource")
	);
	testRequire(
		(pRequest != NULL) &&
		xrtHttpRequestSetBytes(
			pRequest,
			(xbytesview){
				(cbytes)Body,
				sizeof(Body) - 1u
			},
			(xstrview){ NULL, 0 }
		) &&
		xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("Content-Length"),
			XRT_STR_LITERAL("4")
		),
		"HTTP client invalid request fixture failed"
	);
	xrtHttpCallOptionsInit(pUnaligned);
	memcpy(&Options, pUnaligned, sizeof(Options));
	testRequire(
		(Options.Timeout == 0) &&
		(Options.IdleTimeout == 0) &&
		(Options.ResponseBodyLimit == 0),
		"HTTP client unaligned call options init mismatch"
	);
	pCall = xrtHttpClientDo(
		pClient,
		pRequest,
		pUnaligned,
		testHttpClientContractUnexpectedDone,
		&Done
	);
	testRequire(
		pCall == NULL,
		"HTTP client submitted an invalid HTTP/1 request"
	);
	testHttpClientContractError(
		XHTTP_CLIENT_ERROR_REQUEST,
		true
	);
	testRequire(
		xrtAtomic32Load(&Done, XMEMORY_ACQUIRE) == 0,
		"HTTP client called Done after synchronous rejection"
	);

	xrtHttpCallOptionsInit(
		(xhttpcalloptions*)(UINTPTR_MAX - 1u)
	);
	testHttpClientContractError(
		XHTTP_CLIENT_ERROR_ARGUMENT,
		false
	);
	pCall = xrtHttpClientDo(
		pClient,
		pRequest,
		(const xhttpcalloptions*)(UINTPTR_MAX - 1u),
		testHttpClientContractUnexpectedDone,
		&Done
	);
	testRequire(
		(pCall == NULL) &&
		(xrtAtomic32Load(&Done, XMEMORY_ACQUIRE) == 0),
		"HTTP client accepted a wrapping call options range"
	);
	testHttpClientContractError(
		XHTTP_CLIENT_ERROR_ARGUMENT,
		false
	);
	xrtHttpRequestDestroy(pRequest);
}



/*
	已经取消的令牌不得进入 DNS/TCP；
	Call 持有的内部 Client 引用必须允许 Owner 提前释放。
*/
static void testHttpClientContractPreCancel(
	xnetengine* pEngine,
	xhttpclient* pClient,
	const xatomic32* pLookups
)
{
	test_http_client_contract State;
	xhttpcalloptions Options;
	xhttprequest* pRequest;
	xhttpcall* pCall;
	xcancel* pCancel;
	const xerror* pError;

	memset(&State, 0, sizeof(State));
	xrtAtomic32Init(&State.Completed, 0);
	pCancel = xrtCancelCreate();
	testRequire(
		(pCancel != NULL) &&
		xrtCancelRequest(pCancel),
		"HTTP client pre-cancel token setup failed"
	);
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://cancel.test/prestart")
	);
	testRequire(
		pRequest != NULL,
		"HTTP client pre-cancel request setup failed"
	);
	xrtHttpCallOptionsInit(&Options);
	Options.Cancel = pCancel;
	Options.Timeout = XHTTP_CLIENT_TIMEOUT_NONE;
	Options.IdleTimeout = XHTTP_CLIENT_TIMEOUT_NONE;
	pCall = xrtHttpClientDo(
		pClient,
		pRequest,
		&Options,
		testHttpClientContractDone,
		&State
	);
	xrtHttpRequestDestroy(pRequest);
	testRequire(
		pCall != NULL,
		"HTTP client pre-cancel submission failed"
	);

	xrtHttpClientDestroy(pClient);
	testHttpClientContractWait(&State.Completed);
	pError = xrtHttpCallError(pCall);
	testRequire(
		(State.Result == XNET_RESULT_CANCELLED) &&
		(State.State == XHTTP_CALL_CANCELLED) &&
		(State.Error == XHTTP_CLIENT_ERROR_CANCELLED) &&
		State.EmptyResponse &&
		(xrtHttpCallState(pCall) == XHTTP_CALL_CANCELLED) &&
		(pError != NULL) &&
		(strcmp(
			xrtErrorDomain(pError),
			"xrt.http.client"
		) == 0) &&
		(xrtErrorCode(pError) ==
		 XHTTP_CLIENT_ERROR_CANCELLED) &&
		(xrtAtomic32Load(
			pLookups,
			XMEMORY_ACQUIRE
		) == 0),
		"HTTP client pre-cancel terminal contract mismatch"
	);
	testRequire(
		!xrtHttpCallPause(pCall) &&
		!xrtHttpCallResume(pCall) &&
		!xrtHttpCallPaused(pCall),
		"HTTP client terminal Call accepted pause control"
	);
	xrtHttpCallDestroy(pCall);
	xrtCancelDestroy(pCancel);
	testRequire(
		xrtNetEngineDestroy(pEngine),
		"HTTP client contract Engine destroy failed"
	);
}



/* 覆盖高层 Client 的配置、同步提交和预取消生命周期契约。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xhttpclientconfig ClientConfig;
	xnetengine* pEngine;
	xhttpclient* pClient;
	xatomic32 Lookups;

	testHttpClientContractStableCodes();
	testHttpClientContractPauseArgument();
	xrtAtomic32Init(&Lookups, 0);
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pEngine != NULL) &&
		xrtNetEngineStart(pEngine),
		"HTTP client contract Engine start failed"
	);
	testHttpClientContractConfig(pEngine);

	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Resolver.Lookup =
		testHttpClientContractLookup;
	ClientConfig.Resolver.LookupData = &Lookups;
	pClient = xrtHttpClientCreate(pEngine, &ClientConfig);
	testRequire(
		pClient != NULL,
		"HTTP client contract Client creation failed"
	);
	testHttpClientContractSubmit(pClient);
	testHttpClientContractPreCancel(
		pEngine,
		pClient,
		&Lookups
	);
	printf("[PASS] high-level HTTP client public contract\n");
	return 0;
}
