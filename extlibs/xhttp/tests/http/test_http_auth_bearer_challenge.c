#include "../test.h"

#include <xrt/http_auth.h>



/* 按字节比较 challenge 的借用文本和值。 */
static bool testHttpBearerChallengeTextEqual(
	xstrview Text,
	cstr sExpected
)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		(memcmp(Text.Data, sExpected, iSize) == 0);
}



/* 验证五个标准参数的规范写出、查询和解码往返。 */
static void testHttpBearerChallengeRoundTrip(void)
{
	xhttpbearerchallenge Input = {
		XHTTP_BEARER_HAS_REALM |
		XHTTP_BEARER_HAS_SCOPE |
		XHTTP_BEARER_HAS_ERROR |
		XHTTP_BEARER_HAS_ERROR_DESCRIPTION |
		XHTTP_BEARER_HAS_ERROR_URI,
		XRT_STR_INIT("api"),
		XRT_STR_INIT("read write"),
		XRT_STR_INIT("invalid_token"),
		XRT_STR_INIT("The access token expired"),
		XRT_STR_INIT("https://example.com/help")
	};
	xhttpbearerchallenge Challenge;
	char Value[256];
	char Decoded[128];
	size_t iSize;
	size_t iDecoded;

	testRequire(xrtHttpBearerChallengeWrite(
		&Input, NULL, 0, &iSize
	), "HTTP Bearer challenge query failed");
	testRequire(xrtHttpBearerChallengeWrite(
		&Input, Value, sizeof(Value), &iSize
	) && (memcmp(
		Value,
		"Bearer realm=\"api\", scope=\"read write\", "
		"error=\"invalid_token\", "
		"error_description=\"The access token expired\", "
		"error_uri=\"https://example.com/help\"",
		iSize
	) == 0), "HTTP Bearer challenge writer mismatch");
	testRequire(xrtHttpBearerChallengeRead(
		(xstrview){ Value, iSize },
		NULL, 0, &iDecoded, &Challenge
	) && (iDecoded == 74u) &&
		(Challenge.Flags == Input.Flags) &&
		(Challenge.Realm.Data == NULL) &&
		(Challenge.ErrorUri.Data == NULL),
		"HTTP Bearer challenge read query mismatch");
	testRequire(xrtHttpBearerChallengeRead(
		(xstrview){ Value, iSize },
		Decoded, sizeof(Decoded), &iDecoded, &Challenge
	) && (iDecoded == 74u) &&
		testHttpBearerChallengeTextEqual(Challenge.Realm, "api") &&
		testHttpBearerChallengeTextEqual(Challenge.Scope, "read write") &&
		testHttpBearerChallengeTextEqual(
			Challenge.Error, "invalid_token"
		) && testHttpBearerChallengeTextEqual(
			Challenge.ErrorDescription,
			"The access token expired"
		) && testHttpBearerChallengeTextEqual(
			Challenge.ErrorUri, "https://example.com/help"
		), "HTTP Bearer challenge round trip mismatch");
}



/* 验证扩展参数、参数顺序和合法 quoted-pair 均可互操作。 */
static void testHttpBearerChallengeInterop(void)
{
	xhttpbearerchallenge Challenge;
	char Decoded[96];
	size_t iSize;

	testRequire(xrtHttpBearerChallengeRead(
		XRT_STR_LITERAL(
			"Bearer extension=ok, error_uri=\"https:\\/\\/example.com\\/help\", "
			"realm=\"a\\\"b\""
		),
		Decoded, sizeof(Decoded), &iSize, &Challenge
	) && ((Challenge.Flags & XHTTP_BEARER_HAS_REALM) != 0) &&
		((Challenge.Flags & XHTTP_BEARER_HAS_ERROR_URI) != 0) &&
		testHttpBearerChallengeTextEqual(Challenge.Realm, "a\"b") &&
		testHttpBearerChallengeTextEqual(
			Challenge.ErrorUri, "https://example.com/help"
		), "HTTP Bearer challenge extension interoperability failed");
}



/* 验证重复参数、字符集合和 URI 边界。 */
static void testHttpBearerChallengeReject(void)
{
	static const xstrview Invalid[] = {
		XRT_STR_INIT("Bearer token"),
		XRT_STR_INIT("Bearer realm=\"a\", REALM=\"b\""),
		XRT_STR_INIT("Bearer scope=\" read\""),
		XRT_STR_INIT("Bearer scope=\"read  write\""),
		XRT_STR_INIT("Bearer error=\"bad\\\"value\""),
		XRT_STR_INIT("Bearer error_uri=\"relative/path\""),
		XRT_STR_INIT("Bearer error_uri=\"https://example.com/#part\"")
	};
	xhttpbearerchallenge Challenge;
	char Output[128];
	size_t iSize;
	size_t i;

	for ( i = 0; i < (sizeof(Invalid) / sizeof(Invalid[0])); i++ ) {
		testRequire(!xrtHttpBearerChallengeRead(
			Invalid[i], Output, sizeof(Output), &iSize, &Challenge
		), "HTTP Bearer challenge accepted malformed input");
		xrtClearError();
	}
	memset(&Challenge, 0, sizeof(Challenge));
	testRequire(!xrtHttpBearerChallengeWrite(
		&Challenge, NULL, 0, &iSize
	), "HTTP Bearer challenge writer accepted no parameters");
	xrtClearError();
}



/* 验证未对齐描述符、短缓冲和回绕范围保持失败原子性。 */
static void testHttpBearerChallengeMemoryContracts(void)
{
	uint8 InputStorage[sizeof(xhttpbearerchallenge) + 2u];
	uint8 ResultStorage[sizeof(xhttpbearerchallenge) + 2u];
	uint8 SizeStorage[sizeof(size_t) + 2u];
	xhttpbearerchallenge* pInput =
		(xhttpbearerchallenge*)(void*)(InputStorage + 1u);
	xhttpbearerchallenge* pResult =
		(xhttpbearerchallenge*)(void*)(ResultStorage + 1u);
	size_t* pSize = (size_t*)(void*)(SizeStorage + 1u);
	xhttpbearerchallenge Input = {
		XHTTP_BEARER_HAS_REALM,
		XRT_STR_INIT("api"),
		{ NULL, 0 }, { NULL, 0 }, { NULL, 0 }, { NULL, 0 }
	};
	xhttpbearerchallenge Result;
	char Value[64];
	char Output[32];
	char Before[32];
	size_t iSize;

	memset(InputStorage, 0xA5, sizeof(InputStorage));
	memset(ResultStorage, 0xA5, sizeof(ResultStorage));
	memset(SizeStorage, 0xA5, sizeof(SizeStorage));
	memcpy(pInput, &Input, sizeof(Input));
	testRequire(xrtHttpBearerChallengeWrite(
		pInput, Value, sizeof(Value), pSize
	), "HTTP Bearer challenge rejected unaligned input or size");
	memcpy(&iSize, pSize, sizeof(iSize));
	testRequire(xrtHttpBearerChallengeRead(
		(xstrview){ Value, iSize },
		Output, sizeof(Output), pSize, pResult
	), "HTTP Bearer challenge rejected unaligned result");
	memcpy(&Result, pResult, sizeof(Result));
	testRequire(testHttpBearerChallengeTextEqual(Result.Realm, "api") &&
		(InputStorage[0] == 0xA5) &&
		(InputStorage[sizeof(InputStorage) - 1u] == 0xA5) &&
		(ResultStorage[0] == 0xA5) &&
		(ResultStorage[sizeof(ResultStorage) - 1u] == 0xA5) &&
		(SizeStorage[0] == 0xA5) &&
		(SizeStorage[sizeof(SizeStorage) - 1u] == 0xA5),
		"HTTP Bearer challenge unaligned contract mismatch");

	memset(Output, 0x5A, sizeof(Output));
	memcpy(Before, Output, sizeof(Output));
	memset(&Result, 0xA5, sizeof(Result));
	iSize = 71u;
	testRequire(!xrtHttpBearerChallengeRead(
		XRT_STR_LITERAL("Bearer realm=\"api\""),
		Output, 2u, &iSize, &Result
	) && (iSize == 3u) && (Result.Flags == 0) &&
		(memcmp(Output, Before, sizeof(Output)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"HTTP Bearer challenge short read was not atomic");
	xrtClearError();
	testRequire(!xrtHttpBearerChallengeWrite(
		&Input, (void*)(uintptr_t)(UINTPTR_MAX - 1u),
		18u, &iSize
	), "HTTP Bearer challenge writer accepted wrapping output");
	xrtClearError();
	testRequire(!xrtHttpBearerChallengeWrite(
		(xhttpbearerchallenge*)(uintptr_t)(UINTPTR_MAX - 1u),
		NULL, 0, &iSize
	), "HTTP Bearer challenge writer accepted wrapping input");
	xrtClearError();
	testRequire(!xrtHttpBearerChallengeRead(
		XRT_STR_LITERAL("Bearer realm=\"api\""),
		Output, sizeof(Output), &iSize,
		(xhttpbearerchallenge*)(uintptr_t)(UINTPTR_MAX - 1u)
	), "HTTP Bearer challenge reader accepted wrapping result");
	xrtClearError();
}



int main(void)
{
	testHttpBearerChallengeRoundTrip();
	testHttpBearerChallengeInterop();
	testHttpBearerChallengeReject();
	testHttpBearerChallengeMemoryContracts();
	puts("[PASS] HTTP Bearer challenge");
	return 0;
}
