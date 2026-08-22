#include "../test.h"

#include <xrt/http_auth.h>



/* 按字节比较借用文本与零结尾常量。 */
static bool testHttpBasicTextEqual(xstrview Text, cstr sExpected)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		(memcmp(Text.Data, sExpected, iSize) == 0);
}



/* 验证 RFC 7617 示例和空用户名、密码边界。 */
static void testHttpBasicRoundTrip(void)
{
	char Output[96];
	char Decoded[64];
	xhttpbasicauth Basic;
	size_t iSize;
	size_t iDecoded;

	testRequire(xrtHttpBasicWrite(
		XRT_STR_LITERAL("Aladdin"),
		XRT_STR_LITERAL("open sesame"),
		NULL, 0, &iSize
	) && (iSize == 34u),
		"HTTP Basic writer query mismatch");
	testRequire(xrtHttpBasicWrite(
		XRT_STR_LITERAL("Aladdin"),
		XRT_STR_LITERAL("open sesame"),
		Output, sizeof(Output), &iSize
	) && (iSize == 34u) &&
		(memcmp(
			Output,
			"Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==",
			34u
		) == 0),
		"HTTP Basic RFC vector mismatch");
	testRequire(xrtHttpBasicRead(
		(xstrview){ Output, iSize },
		NULL, 0, &iDecoded, &Basic
	) && (iDecoded == 19u) &&
		(Basic.User.Data == NULL) &&
		(Basic.Password.Data == NULL),
		"HTTP Basic reader query mismatch");
	testRequire(xrtHttpBasicRead(
		(xstrview){ Output, iSize },
		Decoded, sizeof(Decoded), &iDecoded, &Basic
	) && (iDecoded == 19u) &&
		testHttpBasicTextEqual(Basic.User, "Aladdin") &&
		testHttpBasicTextEqual(Basic.Password, "open sesame"),
		"HTTP Basic reader output mismatch");
	testRequire(xrtHttpBasicWrite(
		XRT_STR_LITERAL(""), XRT_STR_LITERAL(""),
		Output, sizeof(Output), &iSize
	) && (iSize == 10u) &&
		(memcmp(Output, "Basic Og==", 10u) == 0),
		"HTTP Basic empty credentials mismatch");
}



/* 验证 Basic 的控制字符、分隔符、格式和短缓冲错误。 */
static void testHttpBasicReject(void)
{
	char Output[64];
	char Before[64];
	xhttpbasicauth Basic;
	size_t iSize;

	testRequire(!xrtHttpBasicWrite(
		XRT_STR_LITERAL("bad:user"),
		XRT_STR_LITERAL("password"),
		NULL, 0, &iSize
	) && !xrtHttpBasicWrite(
		XRT_STR_LITERAL("user"),
		XRT_STR_LITERAL("bad\rpassword"),
		NULL, 0, &iSize
	), "HTTP Basic accepted forbidden input bytes");
	xrtClearError();
	memset(Output, 0xA5, sizeof(Output));
	memcpy(Before, Output, sizeof(Output));
	testRequire(!xrtHttpBasicWrite(
		XRT_STR_LITERAL("user"),
		XRT_STR_LITERAL("password"),
		Output, 4u, &iSize
	) && (memcmp(Output, Before, sizeof(Output)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"HTTP Basic short output was not atomic");
	xrtClearError();
	testRequire(!xrtHttpBasicRead(
		XRT_STR_LITERAL("Basic !!!"),
		Output, sizeof(Output), &iSize, &Basic
	) && !xrtHttpBasicRead(
		XRT_STR_LITERAL("Basic bm9jb2xvbg=="),
		Output, sizeof(Output), &iSize, &Basic
	) && !xrtHttpBasicRead(
		XRT_STR_LITERAL("Bearer dXNlcjpwYXNz"),
		Output, sizeof(Output), &iSize, &Basic
	), "HTTP Basic reader accepted malformed credentials");
	xrtClearError();
}



/* 验证 Basic challenge 统一转义 realm 并可声明 UTF-8。 */
static void testHttpBasicChallenge(void)
{
	char Output[96];
	char Decoded[32];
	xhttpauth Auth;
	xhttpbasicchallenge Challenge;
	xhttpparam Param;
	size_t iOffset = 0;
	size_t iSize;
	size_t iDecoded;

	testRequire(xrtHttpBasicChallengeWrite(
		XRT_STR_LITERAL("a\"b\\c"),
		true,
		Output,
		sizeof(Output),
		&iSize
	) && (iSize == 38u) &&
		(memcmp(
			Output,
			"Basic realm=\"a\\\"b\\\\c\", charset=\"UTF-8\"",
			38u
		) == 0) &&
		xrtHttpAuthParse(
			(xstrview){ Output, iSize }, &Auth
		) && (Auth.Kind == XHTTP_AUTH_PARAMS) &&
		(xrtHttpAuthParamNext(
			Auth.Data, &iOffset, &Param
		) == XHTTP_NEXT_ITEM),
		"HTTP Basic challenge mismatch");
	testRequire(xrtHttpBasicChallengeRead(
		(xstrview){ Output, iSize },
		NULL, 0, &iDecoded, &Challenge
	) && (iDecoded == 5u) && Challenge.Utf8 &&
		(Challenge.Realm.Data == NULL),
		"HTTP Basic challenge query mismatch");
	testRequire(xrtHttpBasicChallengeRead(
		(xstrview){ Output, iSize },
		Decoded, sizeof(Decoded), &iDecoded, &Challenge
	) && (iDecoded == 5u) && Challenge.Utf8 &&
		testHttpBasicTextEqual(Challenge.Realm, "a\"b\\c"),
		"HTTP Basic challenge round trip mismatch");
}



/* 验证参数重排、扩展参数、token 值和 RFC 7617 拒绝边界。 */
static void testHttpBasicChallengeInterop(void)
{
	static const xstrview Invalid[] = {
		XRT_STR_INIT("Basic charset=UTF-8"),
		XRT_STR_INIT("Basic realm=a, REALM=b"),
		XRT_STR_INIT("Basic realm=a, charset=UTF-8, CHARSET=utf-8"),
		XRT_STR_INIT("Basic realm=a, charset=latin1"),
		XRT_STR_INIT("Basic dXNlcjpwYXNz"),
		XRT_STR_INIT("Bearer realm=api")
	};
	xhttpbasicchallenge Challenge;
	char Decoded[32];
	size_t iSize;
	size_t i;

	testRequire(xrtHttpBasicChallengeRead(
		XRT_STR_LITERAL(
			"bAsIc extension=ok, CHARSET=utf-8, REALM=\"a\\\"b\""
		),
		Decoded, sizeof(Decoded), &iSize, &Challenge
	) && (iSize == 3u) && Challenge.Utf8 &&
		testHttpBasicTextEqual(Challenge.Realm, "a\"b"),
		"HTTP Basic challenge interoperability mismatch");
	for ( i = 0; i < (sizeof(Invalid) / sizeof(Invalid[0])); i++ ) {
		testRequire(!xrtHttpBasicChallengeRead(
			Invalid[i],
			Decoded,
			sizeof(Decoded),
			&iSize,
			&Challenge
		), "HTTP Basic challenge accepted malformed input");
		xrtClearError();
	}
}



/* 验证固定输出支持未对齐存储，并原子拒绝回绕和别名范围。 */
static void testHttpBasicMemoryContracts(void)
{
	uint8 SizeStorage[sizeof(size_t) + 2u];
	uint8 BasicStorage[sizeof(xhttpbasicauth) + 2u];
	uint8 ChallengeStorage[sizeof(xhttpbasicchallenge) + 2u];
	size_t* pSize = (size_t*)(void*)(SizeStorage + 1u);
	xhttpbasicauth* pBasic =
		(xhttpbasicauth*)(void*)(BasicStorage + 1u);
	xhttpbasicchallenge* pChallenge =
		(xhttpbasicchallenge*)(void*)(ChallengeStorage + 1u);
	char Output[96];
	char Decoded[32];
	char Before[96];
	xhttpbasicauth Basic;
	xhttpbasicchallenge Challenge;
	size_t iSize;

	memset(SizeStorage, 0xA5, sizeof(SizeStorage));
	memset(BasicStorage, 0xA5, sizeof(BasicStorage));
	memset(ChallengeStorage, 0xA5, sizeof(ChallengeStorage));
	testRequire(xrtHttpBasicWrite(
		XRT_STR_LITERAL("user"), XRT_STR_LITERAL("password"),
		Output, sizeof(Output), pSize
	), "HTTP Basic writer rejected unaligned size output");
	memcpy(&iSize, pSize, sizeof(iSize));
	testRequire((iSize == 26u) && xrtHttpBasicRead(
		(xstrview){ Output, iSize },
		Decoded, sizeof(Decoded), pSize, pBasic
	), "HTTP Basic reader rejected unaligned descriptors");
	memcpy(&iSize, pSize, sizeof(iSize));
	memcpy(&Basic, pBasic, sizeof(Basic));
	testRequire((iSize == 13u) &&
		testHttpBasicTextEqual(Basic.User, "user") &&
		testHttpBasicTextEqual(Basic.Password, "password"),
		"HTTP Basic reader published wrong unaligned result");
	testRequire(xrtHttpBasicChallengeWrite(
		XRT_STR_LITERAL("api"), true,
		Output, sizeof(Output), pSize
	), "HTTP Basic challenge rejected unaligned size output");
	memcpy(&iSize, pSize, sizeof(iSize));
	testRequire(xrtHttpBasicChallengeRead(
		(xstrview){ Output, iSize },
		Decoded, sizeof(Decoded), pSize, pChallenge
	), "HTTP Basic challenge rejected unaligned result");
	memcpy(&Challenge, pChallenge, sizeof(Challenge));
	testRequire(Challenge.Utf8 &&
		testHttpBasicTextEqual(Challenge.Realm, "api"),
		"HTTP Basic challenge published wrong unaligned result");
	testRequire(
		(SizeStorage[0] == 0xA5) &&
		(SizeStorage[sizeof(SizeStorage) - 1u] == 0xA5) &&
		(BasicStorage[0] == 0xA5) &&
		(BasicStorage[sizeof(BasicStorage) - 1u] == 0xA5) &&
		(ChallengeStorage[0] == 0xA5) &&
		(ChallengeStorage[sizeof(ChallengeStorage) - 1u] == 0xA5),
		"HTTP Basic wrote outside unaligned descriptor storage"
	);

	memset(Output, 0x5A, sizeof(Output));
	memcpy(Before, Output, sizeof(Output));
	memset(&Basic, 0xA5, sizeof(Basic));
	iSize = 71u;
	testRequire(!xrtHttpBasicRead(
		XRT_STR_LITERAL("Basic !!!"),
		Output, sizeof(Output), &iSize, &Basic
	) && (iSize == 71u) &&
		(Basic.User.Data == NULL) && (Basic.User.Size == 0u) &&
		(Basic.Password.Data == NULL) &&
		(Basic.Password.Size == 0u) &&
		(memcmp(Output, Before, sizeof(Output)) == 0),
		"HTTP Basic parse failure leaked partial output");
	xrtClearError();

	memset(Output, 0x5A, sizeof(Output));
	memcpy(Before, Output, sizeof(Output));
	memset(&Challenge, 0xA5, sizeof(Challenge));
	iSize = 71u;
	testRequire(!xrtHttpBasicChallengeRead(
		XRT_STR_LITERAL("Basic realm=api, charset=UTF-8"),
		Output, 2u, &iSize, &Challenge
	) && (iSize == 3u) && (Challenge.Realm.Data == NULL) &&
		!Challenge.Utf8 &&
		(memcmp(Output, Before, sizeof(Output)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"HTTP Basic challenge short read was not atomic");
	xrtClearError();

	memset(&Basic, 0xA5, sizeof(Basic));
	testRequire(!xrtHttpBasicRead(
		XRT_STR_LITERAL("Basic dXNlcjpwYXNzd29yZA=="),
		Output, 2u, &iSize, &Basic
	) && (iSize == 13u) &&
		(Basic.User.Data == NULL) &&
		(Basic.Password.Data == NULL) &&
		(memcmp(Output, Before, sizeof(Output)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"HTTP Basic short read was not atomic");
	xrtClearError();

	testRequire(!xrtHttpBasicWrite(
		XRT_STR_LITERAL("user"), XRT_STR_LITERAL("password"),
		(void*)(uintptr_t)(UINTPTR_MAX - 1u), 26u, &iSize
	), "HTTP Basic writer accepted wrapping output");
	xrtClearError();
	testRequire(!xrtHttpBasicWrite(
		XRT_STR_LITERAL("user"), XRT_STR_LITERAL("password"),
		Output, sizeof(Output),
		(size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
	), "HTTP Basic writer accepted wrapping size output");
	xrtClearError();
	testRequire(!xrtHttpBasicWrite(
		(xstrview){ (cstr)(uintptr_t)(UINTPTR_MAX - 1u), 4u },
		XRT_STR_LITERAL("password"), NULL, 0, &iSize
	), "HTTP Basic writer accepted wrapping user input");
	xrtClearError();

	testRequire(!xrtHttpBasicRead(
		XRT_STR_LITERAL("Basic dXNlcjpwYXNzd29yZA=="),
		(void*)(uintptr_t)(UINTPTR_MAX - 1u),
		13u, &iSize, &Basic
	), "HTTP Basic reader accepted wrapping output");
	xrtClearError();
	testRequire(!xrtHttpBasicRead(
		XRT_STR_LITERAL("Basic dXNlcjpwYXNzd29yZA=="),
		Decoded, sizeof(Decoded),
		(size_t*)(uintptr_t)(UINTPTR_MAX - 1u), &Basic
	), "HTTP Basic reader accepted wrapping size output");
	xrtClearError();
	testRequire(!xrtHttpBasicRead(
		XRT_STR_LITERAL("Basic dXNlcjpwYXNzd29yZA=="),
		Decoded, sizeof(Decoded), &iSize,
		(xhttpbasicauth*)(uintptr_t)(UINTPTR_MAX - 1u)
	), "HTTP Basic reader accepted wrapping result output");
	xrtClearError();
	testRequire(!xrtHttpBasicRead(
		(xstrview){ (cstr)(uintptr_t)(UINTPTR_MAX - 1u), 8u },
		Decoded, sizeof(Decoded), &iSize, &Basic
	), "HTTP Basic reader accepted wrapping input");
	xrtClearError();

	testRequire(!xrtHttpBasicChallengeWrite(
		XRT_STR_LITERAL("api"), false,
		(void*)(uintptr_t)(UINTPTR_MAX - 1u), 17u, &iSize
	), "HTTP Basic challenge accepted wrapping output");
	xrtClearError();
	testRequire(!xrtHttpBasicChallengeWrite(
		XRT_STR_LITERAL("api"), false,
		Output, sizeof(Output),
		(size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
	), "HTTP Basic challenge accepted wrapping size output");
	xrtClearError();
	testRequire(!xrtHttpBasicChallengeWrite(
		(xstrview){ (cstr)(uintptr_t)(UINTPTR_MAX - 1u), 4u },
		false, NULL, 0, &iSize
	), "HTTP Basic challenge accepted wrapping realm input");
	xrtClearError();
	testRequire(!xrtHttpBasicChallengeRead(
		XRT_STR_LITERAL("Basic realm=api"),
		Decoded, sizeof(Decoded), &iSize,
		(xhttpbasicchallenge*)(uintptr_t)(UINTPTR_MAX - 1u)
	), "HTTP Basic challenge reader accepted wrapping result");
	xrtClearError();
}



int main(void)
{
	testHttpBasicRoundTrip();
	testHttpBasicReject();
	testHttpBasicChallenge();
	testHttpBasicChallengeInterop();
	testHttpBasicMemoryContracts();
	puts("[PASS] HTTP Basic authentication");
	return 0;
}
