#include "../test.h"



/* 验证解析输入描述符会被快照，且结果支持未对齐存储。 */
static void testFormDataParseUnaligned(void)
{
	static const char Wire[] =
		"--x\r\n"
		"Content-Disposition: form-data; name=\"a\"\r\n"
		"Content-Type: text/plain\r\n"
		"\r\n"
		"value\r\n"
		"--x--\r\n";
	uint8 BoundaryStorage[sizeof(xmultipartboundary) + 2u];
	uint8 ConfigStorage[sizeof(xformdataconfig) + 2u];
	uint8 LimitsStorage[sizeof(xmultipartlimits) + 2u];
	uint8 ErrorStorage[sizeof(xmultiparterrorinfo) + 2u];
	xmultipartboundary Boundary;
	xformdataconfig Config;
	xmultipartlimits Limits;
	xmultiparterrorinfo Error;
	xformdatapart Part;
	xformdata* pForm;
	xbytesview Value;

	testRequire(xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("x"), &Boundary
	), "FormData parse boundary setup failed");
	xrtFormDataConfigInit(&Config);
	Config.InitialParts = 0;
	xrtMultipartLimitsInit(&Limits);
	memset(BoundaryStorage, 0xA5, sizeof(BoundaryStorage));
	memset(ConfigStorage, 0xA5, sizeof(ConfigStorage));
	memset(LimitsStorage, 0xA5, sizeof(LimitsStorage));
	memset(ErrorStorage, 0xA5, sizeof(ErrorStorage));
	memcpy(BoundaryStorage + 1u, &Boundary, sizeof(Boundary));
	memcpy(ConfigStorage + 1u, &Config, sizeof(Config));
	memcpy(LimitsStorage + 1u, &Limits, sizeof(Limits));
	pForm = xrtFormDataParse(
		(xbytesview){ (cbytes)Wire, sizeof(Wire) - 1u },
		(const xmultipartboundary*)(const void*)(BoundaryStorage + 1u),
		(const xformdataconfig*)(const void*)(ConfigStorage + 1u),
		(const xmultipartlimits*)(const void*)(LimitsStorage + 1u),
		(xmultiparterrorinfo*)(void*)(ErrorStorage + 1u)
	);
	memcpy(&Error, ErrorStorage + 1u, sizeof(Error));
	testRequire((pForm != NULL) &&
		(Error.Code == XMULTIPART_ERROR_NONE) &&
		(ErrorStorage[0] == 0xA5) &&
		(ErrorStorage[sizeof(ErrorStorage) - 1u] == 0xA5) &&
		xrtFormDataAt(pForm, 0u, &Part) &&
		xrtHttpBodyView(Part.Body, &Value) &&
		(Part.Name.Size == 1u) &&
		(memcmp(Part.Name.Data, "a", 1u) == 0) &&
		(Part.ContentType.Size == 10u) &&
		(memcmp(Part.ContentType.Data, "text/plain", 10u) == 0) &&
		(Value.Size == 5u) &&
		(memcmp(Value.Data, "value", 5u) == 0),
		"FormData parse did not support unaligned descriptors");
	xrtFormDataDestroy(pForm);
}



/* 验证空 FormData 仍统一执行正文和 delimiter 限额。 */
static void testFormDataParseEmptyLimits(void)
{
	static const char Wire[] = "preamble\r\n--x--\r\n";
	xmultipartboundary Boundary;
	xmultipartlimits Limits;
	xmultiparterrorinfo Error;
	xformdata* pForm;

	testRequire(xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("x"), &Boundary
	), "empty FormData parse boundary setup failed");
	xrtMultipartLimitsInit(&Limits);
	Limits.MaxDelimiterBytes = 6u;
	pForm = xrtFormDataParse(
		(xbytesview){ (cbytes)Wire, sizeof(Wire) - 1u },
		&Boundary, NULL, &Limits, &Error
	);
	testRequire((pForm == NULL) &&
		(Error.Code == XMULTIPART_ERROR_DELIMITER_BYTES_LIMIT) &&
		(Error.Offset == 10u) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "form.data") == 0) &&
		(xrtErrorCode(xrtGetError()) == XFORM_DATA_ERROR_MULTIPART),
		"empty FormData bypassed its delimiter limit");
	xrtClearError();

	Limits.MaxDelimiterBytes = 7u;
	pForm = xrtFormDataParse(
		(xbytesview){ (cbytes)Wire, sizeof(Wire) - 1u },
		&Boundary, NULL, &Limits, &Error
	);
	testRequire((pForm != NULL) &&
		(xrtFormDataCount(pForm) == 0u),
		"empty FormData rejected its exact delimiter limit");
	xrtFormDataDestroy(pForm);

	Limits.MaxBodyBytes = sizeof(Wire) - 2u;
	pForm = xrtFormDataParse(
		(xbytesview){ (cbytes)Wire, sizeof(Wire) - 1u },
		&Boundary, NULL, &Limits, &Error
	);
	testRequire((pForm == NULL) &&
		(Error.Code == XMULTIPART_ERROR_BODY_BYTES_LIMIT),
		"empty FormData bypassed its body limit");
	xrtClearError();
}



/* 验证回绕与重叠输入在任何输出发布前被拒绝。 */
static void testFormDataParseInvalidRanges(void)
{
	static const char Wire[] = "--x--\r\n";
	xmultipartboundary Boundary;
	xmultiparterrorinfo Error;
	xmultiparterrorinfo Snapshot;
	xformdata* pForm;

	testRequire(xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("x"), &Boundary
	), "invalid FormData parse boundary setup failed");
	Error.Code = XMULTIPART_ERROR_HEADER;
	Error.Offset = 77u;
	Snapshot = Error;
	pForm = xrtFormDataParse(
		(xbytesview){
			(cbytes)(uintptr_t)(UINTPTR_MAX - 1u), 4u
		},
		&Boundary, NULL, NULL, &Error
	);
	testRequire((pForm == NULL) &&
		(memcmp(&Error, &Snapshot, sizeof(Error)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"FormData parse accepted a wrapping body or changed output");
	xrtClearError();

	pForm = xrtFormDataParse(
		(xbytesview){ (cbytes)Wire, sizeof(Wire) - 1u },
		(const xmultipartboundary*)(uintptr_t)(UINTPTR_MAX - 1u),
		NULL, NULL, &Error
	);
	testRequire((pForm == NULL) &&
		(memcmp(&Error, &Snapshot, sizeof(Error)) == 0),
		"FormData parse accepted a wrapping boundary or changed output");
	xrtClearError();

	pForm = xrtFormDataParseContentType(
		(xstrview){
			(cstr)(uintptr_t)(UINTPTR_MAX - 1u), 4u
		},
		(xbytesview){ (cbytes)Wire, sizeof(Wire) - 1u },
		NULL, NULL, &Error
	);
	testRequire((pForm == NULL) &&
		(memcmp(&Error, &Snapshot, sizeof(Error)) == 0),
		"FormData parse accepted a wrapping Content-Type or changed output");
	xrtClearError();
}



/* 运行拥有型 FormData 解析器的独立边界测试。 */
int main(void)
{
	testFormDataParseUnaligned();
	testFormDataParseEmptyLimits();
	testFormDataParseInvalidRanges();
	printf("[PASS] form_data_parse\n");
	return 0;
}
