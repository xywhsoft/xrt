#include "../test.h"

#include <xrt/http_client_runtime.h>



/* 配置拒绝路径不允许异步发布完成回调。 */
static void testHttpRetryUnexpectedDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	(void)pCall;
	(void)pResult;
	(void)pData;
	testRequire(false, "invalid HTTP retry call was submitted");
}



/* 核对并清除一个稳定的高层客户端错误。 */
static void testHttpRetryConfigError(
	xhttpclienterror Code,
	cstr sMessage
)
{
	const xerror* pError = xrtGetError();

	testRequire(
		(pError != NULL) &&
		(strcmp(
			xrtErrorDomain(pError),
			"xrt.http.client"
		) == 0) &&
		(xrtErrorCode(pError) == (int64)Code),
		sMessage
	);
	xrtClearError();
}



/* 验证默认值、Client 配置边界和单次调用策略边界。 */
int main(void)
{
	xhttpclientconfig Config;
	xhttpcalloptions Options;
	xnetengineconfig EngineConfig;
	xnetengine* pEngine;
	xhttpclient* pClient;
	xhttprequest* pRequest;
	uint8 ConfigStorage[sizeof(xhttpretryconfig) + 2u];
	uint8 OptionsStorage[sizeof(xhttpretryoptions) + 2u];
	xhttpretryconfig RetryConfig;
	xhttpretryoptions RetryOptions;

	xrtHttpClientConfigInit(&Config);
	xrtHttpCallOptionsInit(&Options);
	testRequire(
		(Config.Retry.MaxRetries == 0) &&
		(Config.Retry.BaseDelay == XHTTP_RETRY_BASE_DEFAULT) &&
		(Config.Retry.MaxDelay == XHTTP_RETRY_DELAY_MAX_DEFAULT) &&
		(Config.Retry.Flags == (
			XHTTP_RETRY_STATUS |
			XHTTP_RETRY_TRANSPORT |
			XHTTP_RETRY_RESPECT_AFTER |
			XHTTP_RETRY_JITTER
		)) &&
		(Options.Retry.Mode == XHTTP_RETRY_DEFAULT) &&
		(Options.Retry.Flags == 0),
		"HTTP client retry defaults mismatch"
	);

	/* 两个初始化器都必须支持完整的未对齐存储并保护边界。 */
	memset(ConfigStorage, 0xA5, sizeof(ConfigStorage));
	xrtHttpRetryConfigInit(
		(xhttpretryconfig*)(ConfigStorage + 1u)
	);
	memcpy(
		&RetryConfig,
		ConfigStorage + 1u,
		sizeof(RetryConfig)
	);
	memset(OptionsStorage, 0x5A, sizeof(OptionsStorage));
	xrtHttpRetryOptionsInit(
		(xhttpretryoptions*)(OptionsStorage + 1u)
	);
	memcpy(
		&RetryOptions,
		OptionsStorage + 1u,
		sizeof(RetryOptions)
	);
	testRequire(
		(ConfigStorage[0] == 0xA5) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] == 0xA5) &&
		(OptionsStorage[0] == 0x5A) &&
		(OptionsStorage[sizeof(OptionsStorage) - 1u] == 0x5A) &&
		(RetryConfig.BaseDelay == XHTTP_RETRY_BASE_DEFAULT) &&
		(RetryConfig.MaxDelay ==
		 XHTTP_RETRY_DELAY_MAX_DEFAULT) &&
		(RetryConfig.MaxRetries == 0) &&
		(RetryOptions.Mode == XHTTP_RETRY_DEFAULT) &&
		(RetryOptions.Flags == 0),
		"HTTP retry unaligned initializer mismatch"
	);

	/* 地址回绕必须在任何写入前作为参数错误拒绝。 */
	xrtClearError();
	xrtHttpRetryConfigInit(
		(xhttpretryconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP retry wrapping config contract mismatch"
	);
	xrtClearError();
	xrtHttpRetryOptionsInit(
		(xhttpretryoptions*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP retry wrapping options contract mismatch"
	);
	xrtClearError();

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pEngine != NULL) && xrtNetEngineStart(pEngine),
		"HTTP retry config engine start failed"
	);

	Config.Retry.Flags |= UINT32_C(0x80000000);
	testRequire(
		xrtHttpClientCreate(pEngine, &Config) == NULL,
		"HTTP retry client accepted unknown flags"
	);
	testHttpRetryConfigError(
		XHTTP_CLIENT_ERROR_CONFIG,
		"HTTP retry unknown flag error mismatch"
	);
	xrtHttpClientConfigInit(&Config);
	Config.Retry.MaxDelay = 0;
	testRequire(
		xrtHttpClientCreate(pEngine, &Config) == NULL,
		"HTTP retry client accepted zero maximum delay"
	);
	testHttpRetryConfigError(
		XHTTP_CLIENT_ERROR_CONFIG,
		"HTTP retry zero maximum error mismatch"
	);
	xrtHttpClientConfigInit(&Config);
	Config.Retry.BaseDelay = Config.Retry.MaxDelay + 1u;
	testRequire(
		xrtHttpClientCreate(pEngine, &Config) == NULL,
		"HTTP retry client accepted inverted delay bounds"
	);
	testHttpRetryConfigError(
		XHTTP_CLIENT_ERROR_CONFIG,
		"HTTP retry delay bound error mismatch"
	);

	xrtHttpClientConfigInit(&Config);
	pClient = xrtHttpClientCreate(pEngine, &Config);
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://127.0.0.1:1/config")
	);
	testRequire(
		(pClient != NULL) && (pRequest != NULL),
		"HTTP retry call config fixture failed"
	);
	xrtHttpCallOptionsInit(&Options);
	Options.Retry.Mode = (xhttpretrymode)99;
	testRequire(
		xrtHttpClientDo(
			pClient,
			pRequest,
			&Options,
			testHttpRetryUnexpectedDone,
			NULL
		) == NULL,
		"HTTP retry call accepted an unknown mode"
	);
	testHttpRetryConfigError(
		XHTTP_CLIENT_ERROR_RETRY,
		"HTTP retry call mode error mismatch"
	);
	xrtHttpCallOptionsInit(&Options);
	Options.Retry.Flags = UINT32_C(0x80000000);
	testRequire(
		xrtHttpClientDo(
			pClient,
			pRequest,
			&Options,
			testHttpRetryUnexpectedDone,
			NULL
		) == NULL,
		"HTTP retry call accepted unknown flags"
	);
	testHttpRetryConfigError(
		XHTTP_CLIENT_ERROR_RETRY,
		"HTTP retry call flag error mismatch"
	);

	xrtHttpRequestDestroy(pRequest);
	xrtHttpClientDestroy(pClient);
	testRequire(
		xrtNetEngineDestroy(pEngine),
		"HTTP retry config engine destroy failed"
	);
	puts("[PASS] HTTP client retry config");
	return 0;
}
