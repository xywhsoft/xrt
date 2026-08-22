#include "../test.h"



/* 简单确定性生成器让边界变异在所有编译器上可复现。 */
static uint32 testCookieJarRandom(uint32* pState)
{
	*pState = (*pState * UINT32_C(1664525)) + UINT32_C(1013904223);
	return *pState;
}



/* 变异字段、URL 和上下文不得破坏 Jar 内部不变量。 */
int main(void)
{
	xcookiejarconfig Config;
	xcookiejar* pJar;
	xcookiestorecontext Store;
	xcookierequestcontext Request;
	char Field[192];
	char Output[4096];
	uint32 iState = UINT32_C(0xC001C0DE);
	size_t iRound;

	xrtCookieJarConfigInit(&Config);
	Config.MaxCookies = 64;
	Config.MaxCookiesPerDomain = 16;
	pJar = xrtCookieJarCreate(&Config);
	testRequire(pJar != NULL, "cookie jar mutation create failed");
	memset(&Store, 0, sizeof(Store));
	Store.Flags = XCOOKIE_STORE_HTTP_API | XCOOKIE_STORE_HAS_NOW |
		XCOOKIE_STORE_SAME_SITE;
	memset(&Request, 0, sizeof(Request));
	Request.Flags = XCOOKIE_REQUEST_HTTP_API |
		XCOOKIE_REQUEST_HAS_NOW | XCOOKIE_REQUEST_SAME_SITE;
	for ( iRound = 0; iRound < 6000u; iRound++ ) {
		uint32 iValue = testCookieJarRandom(&iState);
		size_t iLength = (size_t)(iValue % (uint32)sizeof(Field));
		size_t i;
		size_t iSize;

		for ( i = 0; i < iLength; i++ ) {
			Field[i] = (char)(testCookieJarRandom(&iState) & 0x7Fu);
		}
		Store.URL = ((iValue & 1u) == 0) ?
			XRT_STR_LITERAL("https://a.example.test/path/item") :
			XRT_STR_LITERAL("http://b.example.test/other");
		Store.PartitionKey = ((iValue & 2u) == 0) ?
			(xstrview){ NULL, 0 } : XRT_STR_LITERAL("top-site");
		Store.Now = (xtime)iRound * XRT_TIME_SECOND;
		(void)xrtCookieJarStore(
			pJar, &Store, (xstrview){ Field, iLength }, NULL
		);
		Request.URL = Store.URL;
		Request.PartitionKey = Store.PartitionKey;
		Request.Now = Store.Now;
		(void)xrtCookieJarWrite(
			pJar, &Request, Output, sizeof(Output), &iSize
		);
		testRequire(xrtCookieJarCount(pJar) <= Config.MaxCookies,
			"cookie jar mutation exceeded configured capacity");
	}
	xrtCookieJarRelease(pJar);
	printf("[PASS] cookie_jar_mutation rounds=6000\n");
	return 0;
}

