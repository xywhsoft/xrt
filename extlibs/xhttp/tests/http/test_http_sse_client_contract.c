#include "../test.h"



/* 契约夹具只记录不应发生的异步终态回调。 */
typedef struct test_http_sse_contract {
	xatomic32 Closed;
} test_http_sse_contract;



/* 验证当前线程错误属于稳定的 SSE Client 域和分类。 */
static void testHttpSseContractError(
	xhttpsseclienterror Code,
	xerrkind Kind
)
{
	const xerror* pError = xrtGetError();

	testRequire(
		(pError != NULL) &&
		(strcmp(
			xrtErrorDomain(pError),
			"xrt.http.sse.client"
		) == 0) &&
		(xrtErrorCode(pError) == (int32)Code) &&
		(xrtErrorKind(pError) == Kind),
		"SSE client public error classification mismatch"
	);
	xrtClearError();
}



/* 最小消息回调只允许构造器通过公开事件校验。 */
static bool testHttpSseContractMessage(
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



/* 同步拒绝路径不得发布异步 Close。 */
static void testHttpSseContractClose(
	xhttpsseclient* pClient,
	xhttpsseclosereason Reason,
	const xerror* pError,
	ptr pData
)
{
	test_http_sse_contract* pState =
		(test_http_sse_contract*)pData;

	(void)pClient;
	(void)Reason;
	(void)pError;
	xrtAtomic32Store(
		&pState->Closed,
		1,
		XMEMORY_RELEASE
	);
}



/* 占位 Header 回调用于验证 SSE 层拒绝占用内部 Exchange 回调。 */
static bool testHttpSseContractHeaders(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	ptr pData
)
{
	(void)pCall;
	(void)pResponse;
	(void)pData;
	return true;
}



/* 空参数必须返回明确哨兵，并发布 ARGUMENT 而不是下层通用错误。 */
static void testHttpSseContractArguments(void)
{
	xhttpsseclientinfo Info;

	xrtHttpSseClientConfigInit(NULL);
	testHttpSseContractError(
		XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
		XERR_ARGUMENT
	);
	testRequire(
		xrtHttpSseClientRef(NULL) == NULL,
		"SSE client retained a null session"
	);
	testHttpSseContractError(
		XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
		XERR_ARGUMENT
	);
	testRequire(
		!xrtHttpSseClientClose(NULL),
		"SSE client closed a null session"
	);
	testHttpSseContractError(
		XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
		XERR_ARGUMENT
	);
	testRequire(
		!xrtHttpSseClientPause(NULL),
		"SSE client paused a null session"
	);
	testHttpSseContractError(
		XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
		XERR_ARGUMENT
	);
	testRequire(
		!xrtHttpSseClientResume(NULL),
		"SSE client resumed a null session"
	);
	testHttpSseContractError(
		XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
		XERR_ARGUMENT
	);
	testRequire(
		xrtHttpSseClientState(NULL) ==
		XHTTP_SSE_CLIENT_CLOSED,
		"SSE client null state sentinel mismatch"
	);
	testHttpSseContractError(
		XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
		XERR_ARGUMENT
	);
	testRequire(
		!xrtHttpSseClientPaused(NULL),
		"SSE client null pause query mismatch"
	);
	testHttpSseContractError(
		XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
		XERR_ARGUMENT
	);
	testRequire(
		!xrtHttpSseClientInfo(NULL, &Info) &&
		!xrtHttpSseClientInfo(
			(const xhttpsseclient*)(uintptr_t)1,
			NULL
		),
		"SSE client accepted a null info argument"
	);
	testHttpSseContractError(
		XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
		XERR_ARGUMENT
	);
	testRequire(
		xrtHttpSseClientError(NULL) == NULL,
		"SSE client returned a null-session error"
	);
	testHttpSseContractError(
		XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
		XERR_ARGUMENT
	);
}



/* 默认配置必须天然有效，并固定长连接与 origin-form 策略。 */
static void testHttpSseContractDefaults(void)
{
	union {
		uint64 Align;
		uint8 Bytes[sizeof(xhttpsseclientconfig) + 2u];
	} Storage;
	xhttpsseclientconfig Config;
	xhttpsseclientconfig* pUnaligned =
		(xhttpsseclientconfig*)(void*)(Storage.Bytes + 1u);

	memset(Storage.Bytes, 0xA5, sizeof(Storage.Bytes));
	xrtHttpSseClientConfigInit(pUnaligned);
	memcpy(&Config, pUnaligned, sizeof(Config));
	testRequire(
		xrtHttpSseParserConfigValid(&Config.Parser) &&
		(Config.Http.Timeout ==
		 XHTTP_CLIENT_TIMEOUT_NONE) &&
		(Config.Http.IdleTimeout ==
		 XHTTP_CLIENT_TIMEOUT_NONE) &&
		(Config.Http.ResponseBodyLimit == UINT64_MAX) &&
		(Config.Http.Request.TargetForm ==
		 XHTTP1_TARGET_ORIGIN) &&
		(Config.Http.Redirect == XHTTP_REDIRECT_FOLLOW) &&
		(Config.MaxReconnects == SIZE_MAX) &&
		(Config.RetryMin == 100u) &&
		(Config.RetryMax == 300000u) &&
		(Storage.Bytes[0] == 0xA5u) &&
		(Storage.Bytes[sizeof(Storage.Bytes) - 1u] == 0xA5u),
		"SSE client default configuration mismatch"
	);
	xrtHttpSseClientConfigInit(
		(xhttpsseclientconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testHttpSseContractError(
		XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
		XERR_ARGUMENT
	);
}



/* 验证请求、事件、配置和预取消均在提交前同步拒绝。 */
static void testHttpSseContractConstruct(
	xhttpclient* pHttp
)
{
	union {
		uint64 Align;
		uint8 Bytes[sizeof(xhttpsseclientconfig) + 2u];
	} ConfigStorage;
	union {
		uint64 Align;
		uint8 Bytes[sizeof(xhttpsseclientevents) + 2u];
	} EventStorage;
	test_http_sse_contract State;
	xhttpsseclientconfig Config;
	xhttpsseclientevents Events;
	xhttprequest* pRequest;
	xhttpsseclient* pClient;
	xcancel* pCancel;
	const xhttpsseclientconfig* pUnalignedConfig =
		(const xhttpsseclientconfig*)(const void*)(
			ConfigStorage.Bytes + 1u
		);
	const xhttpsseclientevents* pUnalignedEvents =
		(const xhttpsseclientevents*)(const void*)(
			EventStorage.Bytes + 1u
		);

	memset(&State, 0, sizeof(State));
	xrtAtomic32Init(&State.Closed, 0);
	memset(&Events, 0, sizeof(Events));
	Events.Message = testHttpSseContractMessage;
	Events.Close = testHttpSseContractClose;
	Events.Data = &State;
	xrtHttpSseClientConfigInit(&Config);
	testRequire(
		xrtHttpSseConnect(
			NULL,
			XRT_STR_LITERAL("http://contract.test/events"),
			&Config,
			&Events
		) == NULL,
		"SSE client accepted a null HTTP runtime"
	);
	testHttpSseContractError(
		XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
		XERR_ARGUMENT
	);
	testRequire(
		xrtHttpSseConnect(
			pHttp,
			XRT_STR_LITERAL("not a URL"),
			&Config,
			&Events
		) == NULL,
		"SSE client accepted an invalid URL"
	);
	testHttpSseContractError(
		XHTTP_SSE_CLIENT_ERROR_REQUEST,
		XERR_VALUE
	);
	testRequire(
		xrtHttpSseConnectRequest(
			pHttp,
			NULL,
			&Config,
			&Events
		) == NULL,
		"SSE client accepted a null request"
	);
	testHttpSseContractError(
		XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
		XERR_ARGUMENT
	);
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("http://contract.test/events")
	);
	testRequire(
		(pRequest != NULL) &&
		(xrtHttpSseConnectRequest(
			pHttp,
			pRequest,
			&Config,
			&Events
		) == NULL),
		"SSE client accepted a non-GET request"
	);
	testHttpSseContractError(
		XHTTP_SSE_CLIENT_ERROR_REQUEST,
		XERR_VALUE
	);
	xrtHttpRequestDestroy(pRequest);
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://contract.test/events#fragment")
	);
	testRequire(
		(pRequest != NULL) &&
		(xrtHttpSseConnectRequest(
			pHttp,
			pRequest,
			&Config,
			&Events
		) == NULL),
		"SSE client accepted a URL fragment"
	);
	testHttpSseContractError(
		XHTTP_SSE_CLIENT_ERROR_REQUEST,
		XERR_VALUE
	);
	xrtHttpRequestDestroy(pRequest);

	Config.RetryMin = Config.RetryMax + 1u;
	testRequire(
		xrtHttpSseConnect(
			pHttp,
			XRT_STR_LITERAL("http://contract.test/events"),
			&Config,
			&Events
		) == NULL,
		"SSE client accepted an inverted retry range"
	);
	testHttpSseContractError(
		XHTTP_SSE_CLIENT_ERROR_CONFIG,
		XERR_VALUE
	);
	xrtHttpSseClientConfigInit(&Config);
	testRequire(
		xrtHttpSseConnect(
			pHttp,
			XRT_STR_LITERAL("http://contract.test/events"),
			(const xhttpsseclientconfig*)(uintptr_t)(UINTPTR_MAX - 1u),
			&Events
		) == NULL,
		"SSE client accepted a wrapping config range"
	);
	testHttpSseContractError(
		XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
		XERR_ARGUMENT
	);
	testRequire(
		xrtHttpSseConnect(
			pHttp,
			XRT_STR_LITERAL("http://contract.test/events"),
			&Config,
			(const xhttpsseclientevents*)(uintptr_t)(UINTPTR_MAX - 1u)
		) == NULL,
		"SSE client accepted a wrapping event range"
	);
	testHttpSseContractError(
		XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
		XERR_ARGUMENT
	);
	testRequire(
		xrtHttpSseConnect(
			pHttp,
			XRT_STR_LITERAL("http://contract.test/events"),
			&Config,
			NULL
		) == NULL,
		"SSE client accepted a null event table"
	);
	testHttpSseContractError(
		XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
		XERR_ARGUMENT
	);
	Config.Http.Events.Headers =
		testHttpSseContractHeaders;
	testRequire(
		xrtHttpSseConnect(
			pHttp,
			XRT_STR_LITERAL("http://contract.test/events"),
			&Config,
			&Events
		) == NULL,
		"SSE client accepted an occupied Exchange callback"
	);
	testHttpSseContractError(
		XHTTP_SSE_CLIENT_ERROR_CONFIG,
		XERR_VALUE
	);
	xrtHttpSseClientConfigInit(&Config);
	Events.Message = NULL;
	testRequire(
		xrtHttpSseConnect(
			pHttp,
			XRT_STR_LITERAL("http://contract.test/events"),
			&Config,
			&Events
		) == NULL,
		"SSE client accepted a missing Message callback"
	);
	testHttpSseContractError(
		XHTTP_SSE_CLIENT_ERROR_CONFIG,
		XERR_VALUE
	);
	Events.Message = testHttpSseContractMessage;
	pCancel = xrtCancelCreate();
	testRequire(
		(pCancel != NULL) &&
		xrtCancelRequest(pCancel),
		"SSE client pre-cancel fixture failed"
	);
	Config.Http.Cancel = pCancel;
	memset(ConfigStorage.Bytes, 0xA5, sizeof(ConfigStorage.Bytes));
	memset(EventStorage.Bytes, 0x5A, sizeof(EventStorage.Bytes));
	memcpy(
		ConfigStorage.Bytes + 1u,
		&Config,
		sizeof(Config)
	);
	memcpy(
		EventStorage.Bytes + 1u,
		&Events,
		sizeof(Events)
	);
	pClient = xrtHttpSseConnect(
		pHttp,
		XRT_STR_LITERAL("http://contract.test/events"),
		pUnalignedConfig,
		pUnalignedEvents
	);
	testRequire(
		(pClient == NULL) &&
		(ConfigStorage.Bytes[0] == 0xA5u) &&
		(ConfigStorage.Bytes[sizeof(ConfigStorage.Bytes) - 1u] == 0xA5u) &&
		(EventStorage.Bytes[0] == 0x5Au) &&
		(EventStorage.Bytes[sizeof(EventStorage.Bytes) - 1u] == 0x5Au),
		"SSE client submitted a pre-cancelled session"
	);
	testHttpSseContractError(
		XHTTP_SSE_CLIENT_ERROR_CANCELLED,
		XERR_CANCELLED
	);
	xrtCancelDestroy(pCancel);
	testRequire(
		xrtAtomic32Load(
			&State.Closed,
			XMEMORY_ACQUIRE
		) == 0,
		"SSE client published Close after synchronous rejection"
	);
}



/* 覆盖公开参数、默认值和同步构造失败契约。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xhttpclientconfig HttpConfig;
	xnetengine* pEngine;
	xhttpclient* pHttp;

	testHttpSseContractArguments();
	testHttpSseContractDefaults();
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pEngine != NULL) &&
		xrtNetEngineStart(pEngine),
		"SSE contract Engine start failed"
	);
	xrtHttpClientConfigInit(&HttpConfig);
	pHttp = xrtHttpClientCreate(
		pEngine, &HttpConfig
	);
	testRequire(
		pHttp != NULL,
		"SSE contract HTTP runtime creation failed"
	);
	testHttpSseContractConstruct(pHttp);
	xrtHttpClientDestroy(pHttp);
	testRequire(
		xrtNetEngineDestroy(pEngine),
		"SSE contract Engine destroy failed"
	);
	printf("[PASS] HTTP SSE client public contract\n");
	return 0;
}
