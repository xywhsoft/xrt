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
	char LongValue[512];
	char Decoded[64];
	char LongDecoded[384];
	size_t iSize;
	size_t iDecoded;
	size_t iOffset;

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

	/* 超过认证栈缓冲的 quoted-pair URI 也必须保持零分配。 */
	memcpy(LongValue, "Bearer error_uri=\"https:\\/\\/example.com\\/", 41u);
	iOffset = 41u;
	memset(LongValue + iOffset, 'a', 300u);
	iOffset += 300u;
	LongValue[iOffset++] = '"';
	testRequire(xrtHttpBearerChallengeRead(
		(xstrview){ LongValue, iOffset },
		LongDecoded, sizeof(LongDecoded), &iDecoded, &Result
	) && (iDecoded == 320u) &&
		(Result.ErrorUri.Size == 320u) &&
		(memcmp(
			Result.ErrorUri.Data, "https://example.com/", 20u
		) == 0),
		"long escaped Bearer error URI allocated"
	);
	puts("[PASS] HTTP Bearer challenge no allocation");
	return 0;
}
