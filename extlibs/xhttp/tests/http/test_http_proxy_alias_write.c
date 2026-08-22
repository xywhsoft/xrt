#include "../test.h"

#include <xrt/http_proxy_status.h>



/* 验证名称列表的规范百分号编码和解析闭环。 */
static void testProxyAliasWriteVectors(void)
{
	static const xstrview Aliases[] = {
		XRT_STR_INIT("tracker.example"),
		XRT_STR_INIT("comma,name.example"),
		XRT_STR_INIT("dot\\.label.example"),
		XRT_STR_INIT("backslash\\\\name.example")
	};
	static const char Expected[] =
		"tracker.example,comma%2Cname.example,"
		"dot%5C.label.example,backslash%5C%5Cname.example";
	char arrOutput[128];
	size_t iSize;

	testRequire(
		xrtHttpProxyAliasesWrite(
			Aliases, 4u, NULL, 0, &iSize
		) && (iSize == sizeof(Expected) - 1u) &&
		xrtHttpProxyAliasesWrite(
			Aliases, 4u, arrOutput, sizeof(arrOutput), &iSize
		) && (iSize == sizeof(Expected) - 1u) &&
		(memcmp(arrOutput, Expected, iSize) == 0) &&
		xrtHttpProxyAliasesValid(
			(xstrview){ arrOutput, iSize }
		),
		"proxy alias list writer vector mismatch"
	);
	testRequire(
		xrtHttpProxyAliasWrite(
			XRT_STR_LITERAL("comma,name"),
			arrOutput, sizeof(arrOutput), &iSize
		) && (iSize == 12u) &&
		(memcmp(arrOutput, "comma%2Cname", iSize) == 0),
		"single proxy alias writer mismatch"
	);
	testRequire(
		xrtHttpProxyAliasesWrite(
			NULL, 0, arrOutput, sizeof(arrOutput), &iSize
		) && (iSize == 0),
		"empty proxy alias writer mismatch"
	);
}



/* 验证分配型常用路径生成零结尾文本。 */
static void testProxyAliasBuild(void)
{
	static const xstrview Aliases[] = {
		XRT_STR_INIT("one.example"),
		XRT_STR_INIT("two.example")
	};
	char* sOutput;
	size_t iSize;

	sOutput = xrtHttpProxyAliasesBuild(
		Aliases, 2u, &iSize
	);
	testRequire(
		(sOutput != NULL) && (iSize == 23u) &&
		(strcmp(sOutput, "one.example,two.example") == 0),
		"proxy alias build mismatch"
	);
	xrtFree(sOutput);
}



/* 验证非法展示转义、容量和重叠失败保持输出原子。 */
static void testProxyAliasWriteFailure(void)
{
	static const xstrview Invalid[] = {
		XRT_STR_INIT(""),
		XRT_STR_INIT("bad\\x.example"),
		XRT_STR_INIT("bad\\")
	};
	char arrOutput[64];
	char arrBefore[64];
	size_t iSize;
	size_t i;

	memset(arrOutput, 0xA5, sizeof(arrOutput));
	memcpy(arrBefore, arrOutput, sizeof(arrOutput));
	for ( i = 0; i < sizeof(Invalid) / sizeof(Invalid[0]); i++ ) {
		iSize = 77u;
		testRequire(
			!xrtHttpProxyAliasWrite(
				Invalid[i], arrOutput,
				sizeof(arrOutput), &iSize
			) && (iSize == 77u) &&
			(memcmp(arrOutput, arrBefore, sizeof(arrOutput)) == 0),
			"invalid proxy alias was written"
		);
		xrtClearError();
	}
	iSize = 77u;
	testRequire(
		!xrtHttpProxyAliasWrite(
			XRT_STR_LITERAL("comma,name"), arrOutput, 4u, &iSize
		) && (iSize == 12u) &&
		(memcmp(arrOutput, arrBefore, sizeof(arrOutput)) == 0),
		"short proxy alias write was not atomic"
	);
	xrtClearError();
	memcpy(arrOutput, "comma,name", 10u);
	memcpy(arrBefore, arrOutput, sizeof(arrOutput));
	iSize = 77u;
	testRequire(
		!xrtHttpProxyAliasWrite(
			(xstrview){ arrOutput, 10u },
			arrOutput, sizeof(arrOutput), &iSize
		) && (iSize == 77u) &&
		(memcmp(arrOutput, arrBefore, sizeof(arrOutput)) == 0),
		"proxy alias writer accepted overlapping expansion"
	);
}



/* 运行 RFC 9532 next-hop-aliases 写出测试。 */
int main(void)
{
	testProxyAliasWriteVectors();
	testProxyAliasBuild();
	testProxyAliasWriteFailure();
	printf("[PASS] http_proxy_alias_write\n");
	return 0;
}
