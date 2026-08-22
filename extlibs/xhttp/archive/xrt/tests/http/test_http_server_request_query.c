#include "http_server_request_fixture.h"



/* 验证 request-target 查询一步解码为拥有型有序容器。 */
int main(void)
{
	xhttpserverrequest* pRequest =
		testHttpServerRequestFixtureCreate(
			"GET /search?a=1&a=2+3&empty= HTTP/1.1\r\n"
			"Host: example.test\r\n"
			"\r\n",
			(xbytesview){ NULL, 0 },
			XHTTP_SERVER_REQUEST_NONE
		);
	xqueryparams* pParams;
	xquerypair Pair;
	size_t iOffset = 0;
	uint8 ConfigStorage[sizeof(xqueryparamsconfig) + 2u];
	uint8 OffsetStorage[sizeof(size_t) + 2u];
	xqueryparamsconfig* pUnalignedConfig =
		(xqueryparamsconfig*)(void*)(ConfigStorage + 1u);
	size_t* pUnalignedOffset =
		(size_t*)(void*)(OffsetStorage + 1u);
	xqueryparamsconfig Config;

	pParams = xrtHttpServerRequestQueryParams(
		pRequest,
		NULL,
		&iOffset
	);
	testRequire(
		(pParams != NULL) &&
		(iOffset == strlen("a=1&a=2+3&empty=")) &&
		(xrtQueryParamsCount(pParams) == 3) &&
		(xrtQueryParamsCountName(
			pParams,
			XRT_STR_LITERAL("a")
		 ) == 2) &&
		xrtQueryParamsGet(
			pParams,
			XRT_STR_LITERAL("a"),
			&Pair
		) && testHttpServerRequestFixtureText(
			Pair.Value,
			"1"
		),
		"HTTP server request QueryParams mismatch"
	);
	xrtQueryParamsDestroy(pParams);
	xrtQueryParamsConfigInit(&Config);
	memset(ConfigStorage, 0xA5, sizeof(ConfigStorage));
	memset(OffsetStorage, 0xA5, sizeof(OffsetStorage));
	memcpy(pUnalignedConfig, &Config, sizeof(Config));
	pParams = xrtHttpServerRequestQueryParams(
		pRequest,
		pUnalignedConfig,
		pUnalignedOffset
	);
	memcpy(&iOffset, pUnalignedOffset, sizeof(iOffset));
	testRequire(
		(pParams != NULL) &&
		(iOffset == strlen("a=1&a=2+3&empty=")) &&
		(ConfigStorage[0] == UINT8_C(0xA5)) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] ==
		 UINT8_C(0xA5)) &&
		(OffsetStorage[0] == UINT8_C(0xA5)) &&
		(OffsetStorage[sizeof(OffsetStorage) - 1u] ==
		 UINT8_C(0xA5)),
		"HTTP server request rejected unaligned query descriptors"
	);
	xrtQueryParamsDestroy(pParams);
	testRequire(
		xrtHttpServerRequestQueryParams(
			pRequest,
			NULL,
			(size_t*)(void*)pRequest->Target.Data
		) == NULL,
		"HTTP server query error output overwrote request target"
	);
	testHttpServerRequestFixtureError(
		XERR_ARGUMENT,
		XHTTP_SERVER_REQUEST_ERROR_ARGUMENT,
		"HTTP server query overlap error mismatch"
	);
	xrtHttpServerRequestDestroy(pRequest);

	/* 非法 percent 输入在 request-target 层失败并保留 URL 原因。 */
	pRequest = testHttpServerRequestFixtureCreate(
		"GET /search?bad=%ZZ HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"\r\n",
		(xbytesview){ NULL, 0 },
		XHTTP_SERVER_REQUEST_NONE
	);
	testRequire(
		xrtHttpServerRequestQueryParams(
			pRequest,
			NULL,
			&iOffset
		) == NULL,
		"HTTP server request accepted invalid query percent encoding"
	);
	testRequire(
		xrtErrorCause(xrtGetError()) != NULL,
		"HTTP server request query error lost its parser cause"
	);
	testHttpServerRequestFixtureError(
		XERR_VALUE,
		XHTTP_SERVER_REQUEST_ERROR_TARGET,
		"HTTP server request target error mismatch"
	);
	xrtHttpServerRequestDestroy(pRequest);

	/* 合法目标进入 QueryParams 后仍严格执行拥有型容器限额。 */
	pRequest = testHttpServerRequestFixtureCreate(
		"GET /search?a=1&b=2 HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"\r\n",
		(xbytesview){ NULL, 0 },
		XHTTP_SERVER_REQUEST_NONE
	);
	{
		xqueryparamsconfig Config;

		xrtQueryParamsConfigInit(&Config);
		Config.InitialPairs = 1;
		Config.MaxPairs = 1;
		testRequire(
			xrtHttpServerRequestQueryParams(
				pRequest,
				&Config,
				&iOffset
			) == NULL,
			"HTTP server request query ignored QueryParams limits"
		);
	}
	testRequire(
		xrtErrorCause(xrtGetError()) != NULL,
		"HTTP server request query limit lost its parser cause"
	);
	testHttpServerRequestFixtureError(
		XERR_RANGE,
		XHTTP_SERVER_REQUEST_ERROR_QUERY,
		"HTTP server request QueryParams limit error mismatch"
	);
	xrtHttpServerRequestDestroy(pRequest);
	printf("[PASS] HTTP server request QueryParams\n");
	return 0;
}
