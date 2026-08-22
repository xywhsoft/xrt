#include "../test.h"

#include <xrt/http_origin.h>



/* 验证规范大小写、默认端口和 IPv6 写出。 */
static void testHttpOriginCanonicalWrite(void)
{
	xhttporigin Origins[2];
	xhttporigin EmptyPort;
	char sOutput[128];
	size_t iSize;

	testRequire(
		xrtHttpOriginParse(
			XRT_STR_LITERAL("HTTPS://Example.COM:443"),
			&Origins[0]
		) && xrtHttpOriginParse(
			XRT_STR_LITERAL("http://[2001:DB8::1]:8080"),
			&Origins[1]
		) && xrtHttpOriginListWrite(
			Origins, 2u, sOutput, sizeof(sOutput), &iSize
		) && (iSize == 45u) &&
		(memcmp(
			sOutput,
			"https://example.com http://[2001:db8::1]:8080",
			45u
		) == 0),
		"canonical Origin list writer mismatch"
	);
	testRequire(
		xrtHttpOriginParse(
			XRT_STR_LITERAL("https://Example.Test:"), &EmptyPort
		) && xrtHttpOriginWrite(
			&EmptyPort, sOutput, sizeof(sOutput), &iSize
		) && (iSize == 20u) &&
		(memcmp(sOutput, "https://example.test", iSize) == 0),
		"empty Origin port was not removed by canonical writer"
	);
}



/* 验证 null、列表生成约束和失败原子性。 */
static void testHttpOriginWriteBoundaries(void)
{
	xhttporigin Origins[2];
	char sOutput[32];
	char sBefore[32];
	size_t iSize;
	size_t* pAlias = (size_t*)(void*)sOutput;

	xrtHttpOriginNull(&Origins[0]);
	testRequire(
		xrtHttpOriginWrite(
			&Origins[0], sOutput, sizeof(sOutput), &iSize
		) && (iSize == 4u) &&
		(memcmp(sOutput, "null", 4u) == 0),
		"null Origin writer mismatch"
	);
	testRequire(
		xrtHttpOriginParse(
			XRT_STR_LITERAL("https://example.test"), &Origins[1]
		),
		"Origin setup failed"
	);
	Origins[1].Url.Path = XRT_STR_LITERAL("/resource");
	testRequire(
		!xrtHttpOriginWrite(
			&Origins[1], NULL, 0, &iSize
		),
		"Origin writer accepted a resource path"
	);
	xrtClearError();
	testRequire(
		xrtHttpOriginParse(
			XRT_STR_LITERAL("https://example.test"), &Origins[1]
		),
		"Origin setup restore failed"
	);
	memset(sOutput, 0xA5, sizeof(sOutput));
	memcpy(sBefore, sOutput, sizeof(sOutput));
	testRequire(
		!xrtHttpOriginListWrite(
			Origins, 2u, sOutput, sizeof(sOutput), &iSize
		) && (memcmp(sOutput, sBefore, sizeof(sOutput)) == 0),
		"null Origin was combined or modified output"
	);
	xrtClearError();
	memset(sOutput, 0x5A, sizeof(sOutput));
	memcpy(sBefore, sOutput, sizeof(sOutput));
	testRequire(
		!xrtHttpOriginWrite(
			&Origins[1], sOutput, 3u, &iSize
		) && (iSize == 20u) &&
		(memcmp(sOutput, sBefore, sizeof(sOutput)) == 0),
		"short Origin output was modified"
	);
	xrtClearError();
	testRequire(
		!xrtHttpOriginWrite(
			&Origins[1], sOutput, sizeof(sOutput), pAlias
		),
		"Origin writer accepted overlapping size output"
	);
	xrtClearError();
}



/* 验证同源的相邻项不会生成重复线路值。 */
static void testHttpOriginDuplicateWrite(void)
{
	xhttporigin Origins[2];
	size_t iSize;

	testRequire(
		xrtHttpOriginParse(
			XRT_STR_LITERAL("https://EXAMPLE.test:443"),
			&Origins[0]
		) && xrtHttpOriginParse(
			XRT_STR_LITERAL("https://example.test"),
			&Origins[1]
		),
		"duplicate Origin setup failed"
	);
	testRequire(
		!xrtHttpOriginListWrite(
			Origins, 2u, NULL, 0, &iSize
		),
		"adjacent same-origin values were generated"
	);
	xrtClearError();
}



/* 验证 Build 只返回一个零结尾拥有型结果。 */
static void testHttpOriginBuild(void)
{
	xhttporigin Origin;
	str sOutput;
	size_t iSize;

	testRequire(
		xrtHttpOriginParse(
			XRT_STR_LITERAL("HTTP://Build.Test:80"), &Origin
		),
		"Origin build setup failed"
	);
	sOutput = xrtHttpOriginBuild(&Origin, 1u, &iSize);
	testRequire(
		(sOutput != NULL) && (iSize == 17u) &&
		(strcmp(sOutput, "http://build.test") == 0),
		"Origin build mismatch"
	);
	xrtFree(sOutput);
}



int main(void)
{
	testHttpOriginCanonicalWrite();
	testHttpOriginWriteBoundaries();
	testHttpOriginDuplicateWrite();
	testHttpOriginBuild();
	printf("[PASS] http_origin_write\n");
	return 0;
}
