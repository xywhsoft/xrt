#include "../test.h"



/* 验证非安全字段名按 Fetch 规则排序、去重并转为小写。 */
static void testCorsClientHeaderNamesWrite(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("X-Zeta"), XRT_STR_INIT("1") },
		{ XRT_STR_INIT("Content-Type"), XRT_STR_INIT("application/json") },
		{ XRT_STR_INIT("x-alpha"), XRT_STR_INIT("2") },
		{ XRT_STR_INIT("X-ALPHA"), XRT_STR_INIT("3") }
	};
	static const char Expected[] = "content-type,x-alpha,x-zeta";
	char Output[sizeof(Expected) - 1u];
	size_t iSize;

	testRequire(xrtHttpCorsPreflightHeaderNamesWrite(
		Fields, 4u, NULL, 0, &iSize
	) && (iSize == sizeof(Expected) - 1u),
		"CORS preflight header-name size query mismatch");
	testRequire(xrtHttpCorsPreflightHeaderNamesWrite(
		Fields, 4u, Output, sizeof(Output), &iSize
	) && (iSize == sizeof(Output)) &&
		(memcmp(Output, Expected, sizeof(Output)) == 0),
		"CORS preflight header-name canonical write mismatch");
}



/* 验证长度输出支持未对齐存储，并拒绝覆盖输出缓冲。 */
static void testCorsClientWriterAlias(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("X-Trace"), XRT_STR_INIT("abc") }
	};
	uint8 SizeStorage[sizeof(size_t) + 1u];
	char Output[16];
	size_t iSize;

	testRequire(xrtHttpCorsPreflightHeaderNamesWrite(
		Fields,
		1u,
		NULL,
		0,
		(size_t*)(SizeStorage + 1u)
	), "CORS unaligned header-name size failed");
	memcpy(&iSize, SizeStorage + 1u, sizeof(iSize));
	testRequire(iSize == 7u,
		"CORS unaligned header-name size mismatch");
	testRequire(!xrtHttpCorsPreflightHeaderNamesWrite(
		Fields,
		1u,
		Output,
		sizeof(Output),
		(size_t*)Output
	), "CORS writer accepted overlapping size output");
	xrtClearError();
}



/* 验证预检字段行精确写出和短缓冲原子性。 */
static void testCorsClientPreflightWrite(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("X-Trace"), XRT_STR_INIT("abc") },
		{ XRT_STR_INIT("Content-Type"), XRT_STR_INIT("application/json") }
	};
	static const char Expected[] =
		"Origin: https://app.example\r\n"
		"Access-Control-Request-Method: PATCH\r\n"
		"Access-Control-Request-Headers: content-type,x-trace\r\n";
	xhttporigin Origin;
	char Output[sizeof(Expected) - 1u];
	char Short[sizeof(Expected) - 2u];
	size_t iSize;
	size_t i;

	testRequire(xrtHttpOriginParse(
		XRT_STR_LITERAL("https://app.example"), &Origin
	), "CORS preflight writer Origin parse failed");
	testRequire(xrtHttpCorsPreflightFieldsWrite(
		&Origin,
		XRT_STR_LITERAL("PATCH"),
		Fields,
		2u,
		NULL,
		0,
		&iSize
	) && (iSize == sizeof(Expected) - 1u),
		"CORS preflight field size query mismatch");
	testRequire(xrtHttpCorsPreflightFieldsWrite(
		&Origin,
		XRT_STR_LITERAL("PATCH"),
		Fields,
		2u,
		Output,
		sizeof(Output),
		&iSize
	) && (memcmp(Output, Expected, sizeof(Output)) == 0),
		"CORS preflight field write mismatch");
	memset(Short, 0x5A, sizeof(Short));
	testRequire(!xrtHttpCorsPreflightFieldsWrite(
		&Origin,
		XRT_STR_LITERAL("PATCH"),
		Fields,
		2u,
		Short,
		sizeof(Short),
		&iSize
	) && (iSize == sizeof(Expected) - 1u),
		"CORS preflight short buffer unexpectedly succeeded");
	for ( i = 0; i < sizeof(Short); i++ ) {
		testRequire(Short[i] == 0x5A,
			"CORS preflight short buffer was partially modified");
	}
	xrtClearError();
}



/* 执行 CORS 客户端写出测试。 */
int main(void)
{
	testCorsClientHeaderNamesWrite();
	testCorsClientWriterAlias();
	testCorsClientPreflightWrite();
	printf("[PASS] http_cors_client_write\n");
	return 0;
}
