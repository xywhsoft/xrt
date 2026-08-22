#include "../test.h"



/* 比较连续 URI 与不含 quoted-pair 的参数 URI 是否给出同一语法结论。 */
static void testUrlParamCompare(const char* pData, size_t iSize)
{
	xhttpparam Param;
	xurl Url;
	bool bDirect;
	bool bParam;

	memset(&Param, 0, sizeof(Param));
	Param.Value = (xstrview){ pData, iSize };
	Param.Flags = XHTTP_PARAM_HAS_VALUE | XHTTP_PARAM_QUOTED;
	bDirect = xrtUrlParse(Param.Value, &Url);
	xrtClearError();
	bParam = xrtUrlParamValid(&Param);
	xrtClearError();
	testRequire(bDirect == bParam,
		"URL parameter mutation disagreed with URL parser");
}



/* 对协议分隔符与组件字符执行确定性的单字节变异。 */
static void testUrlParamByteMutations(void)
{
	static const cstr Seeds[] = {
		"https://user@example.test:443/a%20b?q=?#f",
		"mailto:user@example.com",
		"//[2001:db8::1]:80/path",
		"../relative/path",
		"1bad:scheme",
		"http://a@b@c/"
	};
	static const char Alphabet[] =
		"abcXYZ019:/?#@%[]!$&'()*+,-.;=_~ ";
	char Buffer[160];
	size_t iLength;
	size_t iSeed;
	size_t iByte;
	size_t iReplacement;

	for ( iSeed = 0;
		iSeed < (sizeof(Seeds) / sizeof(Seeds[0]));
		iSeed++ ) {
		iLength = strlen(Seeds[iSeed]);
		testRequire(iLength < sizeof(Buffer),
			"URL parameter mutation seed is too long");
		for ( iByte = 0; iByte <= iLength; iByte++ ) {
			testUrlParamCompare(Seeds[iSeed], iByte);
		}
		for ( iByte = 0; iByte < iLength; iByte++ ) {
			for ( iReplacement = 0;
				iReplacement < (sizeof(Alphabet) - 1u);
				iReplacement++ ) {
				memcpy(Buffer, Seeds[iSeed], iLength);
				Buffer[iByte] = Alphabet[iReplacement];
				testUrlParamCompare(Buffer, iLength);
			}
		}
	}
}



/* quoted-pair 可以改变线路形式，但不能改变解码后的 URI 语义。 */
static void testUrlParamQuotedPairMutations(void)
{
	static const cstr Seeds[] = {
		"https://example.test/a?q=1#f",
		"mailto:user@example.com",
		"http://a@b@c/"
	};
	char Buffer[160];
	xhttpparam Param;
	xurl Url;
	size_t iLength;
	size_t iSeed;
	size_t iByte;
	bool bExpected;

	memset(&Param, 0, sizeof(Param));
	Param.Flags = XHTTP_PARAM_HAS_VALUE | XHTTP_PARAM_QUOTED;
	for ( iSeed = 0;
		iSeed < (sizeof(Seeds) / sizeof(Seeds[0]));
		iSeed++ ) {
		iLength = strlen(Seeds[iSeed]);
		bExpected = xrtUrlParse(
			(xstrview){ Seeds[iSeed], iLength }, &Url
		);
		xrtClearError();
		for ( iByte = 0; iByte < iLength; iByte++ ) {
			memcpy(Buffer, Seeds[iSeed], iByte);
			Buffer[iByte] = '\\';
			memcpy(
				Buffer + iByte + 1u,
				Seeds[iSeed] + iByte,
				iLength - iByte
			);
			Param.Value = (xstrview){ Buffer, iLength + 1u };
			testRequire(xrtUrlParamValid(&Param) == bExpected,
				"URL parameter quoted-pair changed URI semantics");
			xrtClearError();
		}
	}
}



/* 执行 URL 参数差分变异测试。 */
int main(void)
{
	testUrlParamByteMutations();
	testUrlParamQuotedPairMutations();
	printf("[PASS] url_param_mutation\n");
	return 0;
}
