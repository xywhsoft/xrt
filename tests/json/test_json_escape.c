#include "../test.h"



/* 固定 quote Writer 保存分段输出并可在指定调用失败。 */
typedef struct testjsonescapeoutput {
	uint8 Data[256];
	size_t Size;
	size_t Calls;
	size_t FailAt;
} testjsonescapeoutput;



/* 消费 quote 分段输出。 */
static bool testJsonEscapeWrite(xbytesview Data, ptr pUserData)
{
	testjsonescapeoutput* pOutput = (testjsonescapeoutput*)pUserData;

	pOutput->Calls++;
	if ( pOutput->Calls == pOutput->FailAt ) {
		return false;
	}
	if ( Data.Size > (sizeof(pOutput->Data) - pOutput->Size) ) {
		return false;
	}
	memcpy(pOutput->Data + pOutput->Size, Data.Data, Data.Size);
	pOutput->Size += Data.Size;
	return true;
}



/* 验证默认和全部转义标志的规范输出。 */
static void testJsonEscapeFlags(void)
{
	static const uint8 arrInput[] = {
		'A', '/', '\n', '"', '<', '>', '&', UINT8_C(0xC3), UINT8_C(0xA9)
	};
	static const uint8 arrDefault[] = {
		'"', 'A', '/', '\\', 'n', '\\', '"', '<', '>', '&',
		UINT8_C(0xC3), UINT8_C(0xA9), '"'
	};
	static const char sEscaped[] =
		"\"A\\/\\n\\\"\\u003C\\u003E\\u0026\\u00E9\"";
	testjsonescapeoutput Output;
	size_t iWritten;

	memset(&Output, 0, sizeof(Output));
	testRequire(
		xrtJsonQuoteWrite(
			(xstrview){ (cstr)arrInput, sizeof(arrInput) },
			0,
			testJsonEscapeWrite,
			&Output,
			&iWritten
		) &&
		(iWritten == sizeof(arrDefault)) &&
		(Output.Size == sizeof(arrDefault)) &&
		(memcmp(Output.Data, arrDefault, sizeof(arrDefault)) == 0),
		"default JSON quote output mismatch"
	);
	memset(&Output, 0, sizeof(Output));
	testRequire(
		xrtJsonQuoteWrite(
			(xstrview){ (cstr)arrInput, sizeof(arrInput) },
			XJSON_WRITE_ESCAPE_SLASH |
			XJSON_WRITE_ESCAPE_HTML |
			XJSON_WRITE_ESCAPE_NON_ASCII,
			testJsonEscapeWrite,
			&Output,
			&iWritten
		) &&
		(Output.Size == (sizeof(sEscaped) - 1u)) &&
		(memcmp(Output.Data, sEscaped, sizeof(sEscaped) - 1u) == 0),
		"configured JSON quote output mismatch"
	);
}



/* 验证 UTF-8 错误位置和 Writer 失败时的精确已写长度。 */
static void testJsonEscapeErrors(void)
{
	const char arrInvalid[] = { (char)0xC3, '(' };
	testjsonescapeoutput Output;
	xjsonlocation Location;
	xerror* pPrevious;
	size_t iWritten = SIZE_MAX;

	memset(&Output, 0, sizeof(Output));
	testRequire(
		!xrtJsonQuoteWrite(
			(xstrview){ arrInvalid, sizeof(arrInvalid) },
			0,
			testJsonEscapeWrite,
			&Output,
			&iWritten
		) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "xrt.json") == 0) &&
		(xrtErrorCode(xrtGetError()) == XJSON_ERROR_UNSUPPORTED) &&
		xrtJsonErrorLocation(xrtGetError(), &Location) &&
		(Location.Offset == 0u) && (iWritten == 0u) &&
		(Output.Calls == 0u),
		"invalid UTF-8 JSON quote contract mismatch"
	);
	xrtClearError();

	memset(&Output, 0, sizeof(Output));
	Output.FailAt = 2u;
	testRequire(
		!xrtJsonQuoteWrite(
			XRT_STR_LITERAL("value"),
			0,
			testJsonEscapeWrite,
			&Output,
			&iWritten
		) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "xrt.json") == 0) &&
		(xrtErrorCode(xrtGetError()) == XJSON_ERROR_OUTPUT) &&
		(iWritten == 1u) && (Output.Size == 1u),
		"JSON quote Writer failure contract mismatch"
	);
	xrtClearError();

	memset(&Output, 0, sizeof(Output));
	pPrevious = xrtErrorCreate(XERR_VALUE, "previous", 1, "old");
	testRequire(pPrevious != NULL, "JSON quote previous error creation failed");
	xrtSetError(pPrevious);
	testRequire(
		xrtJsonQuoteWrite(
			XRT_STR_LITERAL("ok"),
			0,
			testJsonEscapeWrite,
			&Output,
			&iWritten
		) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "previous") == 0),
		"successful JSON quote changed previous error"
	);
	testRequire(
		!xrtJsonQuoteWrite(
			XRT_STR_LITERAL("bad flags"),
			XJSON_WRITE_PRETTY,
			testJsonEscapeWrite,
			&Output,
			NULL
		) &&
		(xrtErrorCode(xrtGetError()) == XJSON_ERROR_CONFIG),
		"JSON quote accepted unrelated writer flags"
	);
	xrtClearError();
	xrtErrorFree(pPrevious);
}



/* 执行流式 JSON quote 完整回归。 */
int main(void)
{
	testJsonEscapeFlags();
	testJsonEscapeErrors();
	printf("[PASS] JSON escape\n");
	return 0;
}
