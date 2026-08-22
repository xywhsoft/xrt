#include "../test_allocator.h"

#include <xrt/http_auth.h>



/* 常规 Bearer challenge 查询、解析和写出不得依赖堆分配。 */
int main(void)
{
	xhttpbearerchallenge Input = {
		XHTTP_BEARER_HAS_REALM |
		XHTTP_BEARER_HAS_ERROR |
		XHTTP_BEARER_HAS_ERROR_URI,
		XRT_STR_INIT("api"),
		{ NULL, 0 },
		XRT_STR_INIT("invalid_token"),
		{ NULL, 0 },
		XRT_STR_INIT("https://example.com/help")
	};
	xhttpbearerchallenge Result;
	char Value[160];
	char Decoded[64];
	size_t iSize;
	size_t iDecoded;

	testRequire(testInstallFailAllocator(),
		"HTTP Bearer challenge failure allocator install failed");
	testRequire(xrtHttpBearerChallengeWrite(
		&Input, Value, sizeof(Value), &iSize
	), "HTTP Bearer challenge writer allocated");
	testRequire(xrtHttpBearerChallengeRead(
		(xstrview){ Value, iSize },
		NULL, 0, &iDecoded, &Result
	), "HTTP Bearer challenge query allocated");
	testRequire(xrtHttpBearerChallengeRead(
		(xstrview){ Value, iSize },
		Decoded, sizeof(Decoded), &iDecoded, &Result
	), "HTTP Bearer challenge reader allocated");
	puts("[PASS] HTTP Bearer challenge no allocation");
	return 0;
}
