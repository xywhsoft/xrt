#include "http_server_request_fixture.h"



/* 验证 multipart/form-data 从请求正文一步解析为拥有型容器。 */
int main(void)
{
	static const char Body[] =
		"--AaB03x\r\n"
		"Content-Disposition: form-data; name=\"field\"\r\n"
		"\r\n"
		"value\r\n"
		"--AaB03x--\r\n";
	xhttpserverrequest* pRequest =
		testHttpServerRequestFixtureCreate(
			"POST /upload HTTP/1.1\r\n"
			"Host: example.test\r\n"
			"Content-Type: multipart/form-data; boundary=AaB03x\r\n"
			"Content-Length: 77\r\n"
			"\r\n",
			XRT_BYTES_LITERAL(Body),
			XHTTP_SERVER_REQUEST_NONE
		);
	xmultiparterrorinfo Error;
	xformdatapart Part;
	xformdata* pForm;
	xformdataconfig Config;
	xmultipartlimits Limits;
	uint8 ConfigStorage[sizeof(xformdataconfig) + 2u];
	uint8 LimitsStorage[sizeof(xmultipartlimits) + 2u];
	uint8 ErrorStorage[sizeof(xmultiparterrorinfo) + 2u];
	xformdataconfig* pUnalignedConfig =
		(xformdataconfig*)(void*)(ConfigStorage + 1u);
	xmultipartlimits* pUnalignedLimits =
		(xmultipartlimits*)(void*)(LimitsStorage + 1u);
	xmultiparterrorinfo* pUnalignedError =
		(xmultiparterrorinfo*)(void*)(ErrorStorage + 1u);

	memset(&Error, 0, sizeof(Error));
	pForm = xrtHttpServerRequestFormData(
		pRequest,
		NULL,
		NULL,
		&Error
	);
	testRequire(
		(pForm != NULL) &&
		(xrtFormDataCount(pForm) == 1) &&
		xrtFormDataGet(
			pForm,
			XRT_STR_LITERAL("field"),
			&Part
		) &&
		(Part.Length == 5) &&
		((Part.Flags & XFORM_DATA_PART_FILENAME) == 0),
		"HTTP server request multipart FormData mismatch"
	);
	xrtFormDataDestroy(pForm);
	xrtFormDataConfigInit(&Config);
	xrtMultipartLimitsInit(&Limits);
	memset(ConfigStorage, 0xA5, sizeof(ConfigStorage));
	memset(LimitsStorage, 0xA5, sizeof(LimitsStorage));
	memset(ErrorStorage, 0xA5, sizeof(ErrorStorage));
	memcpy(pUnalignedConfig, &Config, sizeof(Config));
	memcpy(pUnalignedLimits, &Limits, sizeof(Limits));
	pForm = xrtHttpServerRequestFormData(
		pRequest,
		pUnalignedConfig,
		pUnalignedLimits,
		pUnalignedError
	);
	memcpy(&Error, pUnalignedError, sizeof(Error));
	testRequire(
		(pForm != NULL) &&
		(xrtFormDataCount(pForm) == 1) &&
		(Error.Code == 0) &&
		(ConfigStorage[0] == UINT8_C(0xA5)) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] ==
		 UINT8_C(0xA5)) &&
		(LimitsStorage[0] == UINT8_C(0xA5)) &&
		(LimitsStorage[sizeof(LimitsStorage) - 1u] ==
		 UINT8_C(0xA5)) &&
		(ErrorStorage[0] == UINT8_C(0xA5)) &&
		(ErrorStorage[sizeof(ErrorStorage) - 1u] ==
		 UINT8_C(0xA5)),
		"HTTP server request rejected unaligned FormData descriptors"
	);
	xrtFormDataDestroy(pForm);
	xrtHttpServerRequestDestroy(pRequest);

	/* 错误媒体类型在进入 multipart 解析器前失败。 */
	pRequest = testHttpServerRequestFixtureCreate(
		"POST /upload HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: 0\r\n"
		"\r\n",
		(xbytesview){ NULL, 0 },
		XHTTP_SERVER_REQUEST_NONE
	);
	testRequire(
		xrtHttpServerRequestFormData(
			pRequest,
			NULL,
			NULL,
			&Error
		) == NULL,
		"HTTP server request FormData accepted text/plain"
	);
	testHttpServerRequestFixtureError(
		XERR_VALUE,
		XHTTP_SERVER_REQUEST_ERROR_CONTENT_TYPE,
		"HTTP server request FormData media error mismatch"
	);
	xrtHttpServerRequestDestroy(pRequest);
	printf("[PASS] HTTP server request multipart FormData helper\n");
	return 0;
}

