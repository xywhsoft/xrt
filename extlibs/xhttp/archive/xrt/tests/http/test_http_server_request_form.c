#include "http_server_request_fixture.h"



/* 验证 urlencoded 表单常用路径与正文所有权约束。 */
int main(void)
{
	static const char Form[] = "a=1&name=hello+world";
	xhttpserverrequest* pRequest =
		testHttpServerRequestFixtureCreate(
			"POST /submit HTTP/1.1\r\n"
			"Host: example.test\r\n"
			"Content-Type: application/x-www-form-urlencoded; charset=utf-8\r\n"
			"Content-Length: 20\r\n"
			"\r\n",
			XRT_BYTES_LITERAL(Form),
			XHTTP_SERVER_REQUEST_NONE
		);
	xqueryparams* pParams;
	xquerypair Pair;
	size_t iOffset = 0;
	xqueryparamsconfig Config;
	uint8 ConfigStorage[sizeof(xqueryparamsconfig) + 2u];
	uint8 OffsetStorage[sizeof(size_t) + 2u];
	xqueryparamsconfig* pUnalignedConfig =
		(xqueryparamsconfig*)(void*)(ConfigStorage + 1u);
	size_t* pUnalignedOffset =
		(size_t*)(void*)(OffsetStorage + 1u);

	pParams = xrtHttpServerRequestForm(
		pRequest,
		NULL,
		&iOffset
	);
	testRequire(
		(pParams != NULL) &&
		(iOffset == sizeof(Form) - 1u) &&
		xrtQueryParamsGet(
			pParams,
			XRT_STR_LITERAL("name"),
			&Pair
		) && testHttpServerRequestFixtureText(
			Pair.Value,
			"hello world"
		),
		"HTTP server request urlencoded form mismatch"
	);
	xrtQueryParamsDestroy(pParams);
	xrtQueryParamsConfigInit(&Config);
	memset(ConfigStorage, 0xA5, sizeof(ConfigStorage));
	memset(OffsetStorage, 0xA5, sizeof(OffsetStorage));
	memcpy(pUnalignedConfig, &Config, sizeof(Config));
	pParams = xrtHttpServerRequestForm(
		pRequest,
		pUnalignedConfig,
		pUnalignedOffset
	);
	memcpy(&iOffset, pUnalignedOffset, sizeof(iOffset));
	testRequire(
		(pParams != NULL) &&
		(iOffset == sizeof(Form) - 1u) &&
		(ConfigStorage[0] == UINT8_C(0xA5)) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] ==
		 UINT8_C(0xA5)) &&
		(OffsetStorage[0] == UINT8_C(0xA5)) &&
		(OffsetStorage[sizeof(OffsetStorage) - 1u] ==
		 UINT8_C(0xA5)),
		"HTTP server request rejected unaligned form descriptors"
	);
	xrtQueryParamsDestroy(pParams);
	xrtHttpServerRequestDestroy(pRequest);

	/* 尚未完成的请求不能提前解析为拥有型表单。 */
	pRequest = testHttpServerRequestFixtureCreate(
		"POST /submit HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Content-Type: application/x-www-form-urlencoded\r\n"
		"Content-Length: 3\r\n"
		"\r\n",
		XRT_BYTES_LITERAL("a=1"),
		XHTTP_SERVER_REQUEST_NONE
	);
	pRequest->Flags &= ~XHTTP_SERVER_REQUEST_COMPLETE;
	testRequire(
		xrtHttpServerRequestForm(
			pRequest,
			NULL,
			&iOffset
		) == NULL,
		"HTTP server request form accepted an incomplete request"
	);
	testHttpServerRequestFixtureError(
		XERR_STATE,
		XHTTP_SERVER_REQUEST_ERROR_STATE,
		"HTTP server request incomplete form error mismatch"
	);
	xrtHttpServerRequestDestroy(pRequest);

	/* 流式正文即使完整也不允许被拥有型表单辅助器读取。 */
	pRequest = testHttpServerRequestFixtureCreate(
		"POST /submit HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Content-Type: application/x-www-form-urlencoded\r\n"
		"Content-Length: 3\r\n"
		"\r\n",
		XRT_BYTES_LITERAL("a=1"),
		XHTTP_SERVER_REQUEST_STREAMED
	);
	testRequire(
		xrtHttpServerRequestForm(
			pRequest,
			NULL,
			&iOffset
		) == NULL,
		"HTTP server request form accepted a streamed body"
	);
	testHttpServerRequestFixtureError(
		XERR_STATE,
		XHTTP_SERVER_REQUEST_ERROR_BODY,
		"HTTP server request streamed form error mismatch"
	);
	xrtHttpServerRequestDestroy(pRequest);
	printf("[PASS] HTTP server request urlencoded form helper\n");
	return 0;
}
