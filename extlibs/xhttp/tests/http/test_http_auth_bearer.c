#include "../test.h"

#include <xrt/http_auth.h>



/* 按字节比较借用文本与零结尾常量。 */
static bool testHttpBearerTextEqual(xstrview Text, cstr sExpected)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		(memcmp(Text.Data, sExpected, iSize) == 0);
}



/* 验证 RFC 6750 b64token 往返且解析保持零分配视图。 */
static void testHttpBearerRoundTrip(void)
{
	char Output[64];
	xstrview Token;
	size_t iSize;

	testRequire(xrtHttpBearerWrite(
		XRT_STR_LITERAL("mF_9.B5f-4.1JqM"),
		Output, sizeof(Output), &iSize
	) && (iSize == 22u) &&
		xrtHttpBearerRead(
			(xstrview){ Output, iSize }, &Token
		) && testHttpBearerTextEqual(
			Token, "mF_9.B5f-4.1JqM"
		),
		"HTTP Bearer round trip mismatch");
}



/* 验证空白、非法填充和错误认证方案均被拒绝。 */
static void testHttpBearerReject(void)
{
	xstrview Token;

	testRequire(!xrtHttpBearerTokenValid(
		XRT_STR_LITERAL("bad token")
	) && !xrtHttpBearerTokenValid(
		XRT_STR_LITERAL("bad=tail")
	) && !xrtHttpBearerRead(
		XRT_STR_LITERAL("Basic dXNlcjpwYXNz"), &Token
	), "HTTP Bearer accepted malformed credentials");
	xrtClearError();
}



/* 验证 Bearer 固定输出支持未对齐存储并拒绝回绕和输入别名。 */
static void testHttpBearerMemoryContracts(void)
{
	uint8 SizeStorage[sizeof(size_t) + 2u];
	uint8 TokenStorage[sizeof(xstrview) + 2u];
	size_t* pSize = (size_t*)(void*)(SizeStorage + 1u);
	xstrview* pToken =
		(xstrview*)(void*)(TokenStorage + 1u);
	char Alias[sizeof(xstrview) + 16u];
	char Output[64];
	char Before[64];
	xstrview Token;
	const xerror* pBefore;
	size_t iSize;

	memset(SizeStorage, 0xA5, sizeof(SizeStorage));
	memset(TokenStorage, 0xA5, sizeof(TokenStorage));
	testRequire(xrtHttpBearerWrite(
		XRT_STR_LITERAL("opaque-token"),
		Output, sizeof(Output), pSize
	), "HTTP Bearer writer rejected unaligned size output");
	memcpy(&iSize, pSize, sizeof(iSize));
	testRequire((iSize == 19u) && xrtHttpBearerRead(
		(xstrview){ Output, iSize }, pToken
	), "HTTP Bearer reader rejected unaligned result output");
	memcpy(&Token, pToken, sizeof(Token));
	testRequire(testHttpBearerTextEqual(Token, "opaque-token") &&
		(SizeStorage[0] == 0xA5) &&
		(SizeStorage[sizeof(SizeStorage) - 1u] == 0xA5) &&
		(TokenStorage[0] == 0xA5) &&
		(TokenStorage[sizeof(TokenStorage) - 1u] == 0xA5),
		"HTTP Bearer unaligned output contract mismatch");
	memset(Output, 0x5A, sizeof(Output));
	memcpy(Before, Output, sizeof(Output));
	iSize = 71u;
	testRequire(!xrtHttpBearerWrite(
		XRT_STR_LITERAL("opaque-token"), Output, 4u, &iSize
	) && (iSize == 19u) &&
		(memcmp(Output, Before, sizeof(Output)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"HTTP Bearer short write was not atomic");
	xrtClearError();
	memset(&Token, 0xA5, sizeof(Token));
	testRequire(!xrtHttpBearerRead(
		XRT_STR_LITERAL("Basic abc"), &Token
	) && (Token.Data == NULL) && (Token.Size == 0u),
		"HTTP Bearer parse failure leaked a partial token");
	xrtClearError();
	testRequire(!xrtHttpBearerRead(
		XRT_STR_LITERAL("Bearer token"),
		(xstrview*)(uintptr_t)(UINTPTR_MAX - 1u)
	), "HTTP Bearer reader accepted wrapping result output");
	xrtClearError();
	testRequire(!xrtHttpBearerRead(
		(xstrview){ (cstr)(uintptr_t)(UINTPTR_MAX - 1u), 8u },
		&Token
	), "HTTP Bearer reader accepted wrapping input");
	xrtClearError();
	memset(Alias, 0, sizeof(Alias));
	memcpy(Alias, "Bearer token", 12u);
	testRequire(!xrtHttpBearerRead(
		(xstrview){ Alias, 12u },
		(xstrview*)(void*)Alias
	), "HTTP Bearer reader accepted an input alias");
	xrtClearError();

	testRequire(!xrtHttpBearerWrite(
		XRT_STR_LITERAL("opaque-token"),
		(void*)(uintptr_t)(UINTPTR_MAX - 1u), 19u, &iSize
	), "HTTP Bearer writer accepted wrapping output");
	xrtClearError();
	testRequire(!xrtHttpBearerWrite(
		XRT_STR_LITERAL("opaque-token"), Output, sizeof(Output),
		(size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
	), "HTTP Bearer writer accepted wrapping size output");
	xrtClearError();
	testRequire(!xrtHttpBearerWrite(
		(xstrview){ (cstr)(uintptr_t)(UINTPTR_MAX - 1u), 8u },
		NULL, 0, &iSize
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Bearer writer misclassified wrapping token input");
	xrtClearError();

	testRequire(!xrtHttpBearerWrite(
		XRT_STR_LITERAL("bad token"), NULL, 0, &iSize
	), "HTTP Bearer predicate preservation fixture failed");
	pBefore = xrtGetError();
	testRequire((pBefore != NULL) && !xrtHttpBearerTokenValid(
		(xstrview){ (cstr)(uintptr_t)(UINTPTR_MAX - 1u), 8u }
	) && (xrtGetError() == pBefore),
		"HTTP Bearer token predicate replaced the current error");
	xrtClearError();
}



int main(void)
{
	testHttpBearerRoundTrip();
	testHttpBearerReject();
	testHttpBearerMemoryContracts();
	puts("[PASS] HTTP Bearer authentication");
	return 0;
}
