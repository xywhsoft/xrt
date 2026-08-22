#include "../test.h"

#include <xrt/http_auth.h>



/* 按字节比较 Authentication-Info 中的借用文本。 */
static bool testHttpDigestInfoTextEqual(xstrview Text, cstr sExpected)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		(memcmp(Text.Data, sExpected, iSize) == 0);
}



/* 验证完整响应认证参数的查询、解析和规范写出。 */
static void testHttpDigestInfoRoundTrip(void)
{
	static const char Response[] =
		"0123456789ABCDEF0123456789ABCDEF"
		"0123456789ABCDEF0123456789ABCDEF";
	static const char Expected[] =
		"nextnonce=\"next\", qop=auth, "
		"rspauth=\"0123456789abcdef0123456789abcdef"
		"0123456789abcdef0123456789abcdef\", "
		"cnonce=\"client\", nc=00000002";
	xhttpdigestinfo Info;
	char Decoded[96];
	char Written[192];
	size_t iRequired;
	size_t iWritten;

	testRequire(xrtHttpDigestInfoRead(
		XRT_STR_LITERAL(
			"nc=00000002, extension=value, cnonce=\"client\", "
			"rspauth=\"0123456789ABCDEF0123456789ABCDEF"
			"0123456789ABCDEF0123456789ABCDEF\", "
			"qop=auth, nextnonce=\"next\""
		),
		XHTTP_DIGEST_ALGORITHM_SHA256,
		NULL, 0, &iRequired, &Info
	) && (iRequired == 74u) &&
		(Info.Flags == (XHTTP_DIGEST_INFO_HAS_NEXT_NONCE |
		 XHTTP_DIGEST_INFO_HAS_RESPONSE)) &&
		(Info.Algorithm == XHTTP_DIGEST_ALGORITHM_SHA256) &&
		(Info.Qop == XHTTP_DIGEST_QOP_AUTH) &&
		(Info.NonceCount == 2u) &&
		(Info.NextNonce.Data == NULL) &&
		(Info.Response.Data == NULL) &&
		(Info.Cnonce.Data == NULL),
		"HTTP Digest info query mismatch");
	testRequire(xrtHttpDigestInfoRead(
		XRT_STR_LITERAL(
			"nc=00000002, extension=value, cnonce=\"client\", "
			"rspauth=\"0123456789ABCDEF0123456789ABCDEF"
			"0123456789ABCDEF0123456789ABCDEF\", "
			"qop=auth, nextnonce=\"next\""
		),
		XHTTP_DIGEST_ALGORITHM_SHA256,
		Decoded, sizeof(Decoded), &iRequired, &Info
	) && testHttpDigestInfoTextEqual(Info.NextNonce, "next") &&
		testHttpDigestInfoTextEqual(Info.Response, Response) &&
		testHttpDigestInfoTextEqual(Info.Cnonce, "client"),
		"HTTP Digest info parse mismatch");
	testRequire(xrtHttpDigestInfoWrite(
		&Info, NULL, 0, &iWritten
	) && (iWritten == (sizeof(Expected) - 1u)),
		"HTTP Digest info write query mismatch");
	testRequire(xrtHttpDigestInfoWrite(
		&Info, Written, sizeof(Written), &iWritten
	) && (iWritten == (sizeof(Expected) - 1u)) &&
		(memcmp(Written, Expected, iWritten) == 0),
		"HTTP Digest info canonical write mismatch");
}



/* 验证独立 nextnonce、空参数集和未知扩展算法上下文。 */
static void testHttpDigestInfoOptional(void)
{
	xhttpdigestinfo Info;
	char Decoded[32];
	char Written[96];
	size_t iSize;

	testRequire(xrtHttpDigestInfoRead(
		XRT_STR_LITERAL("nextnonce=\"n\\\"ext\", extension=ok"),
		XHTTP_DIGEST_ALGORITHM_SHA512_256,
		Decoded, sizeof(Decoded), &iSize, &Info
	) && (Info.Flags == XHTTP_DIGEST_INFO_HAS_NEXT_NONCE) &&
		testHttpDigestInfoTextEqual(Info.NextNonce, "n\"ext"),
		"HTTP Digest nextnonce parse mismatch");
	testRequire(xrtHttpDigestInfoRead(
		XRT_STR_LITERAL("extension=ok"),
		XHTTP_DIGEST_ALGORITHM_UNKNOWN,
		NULL, 0, &iSize, &Info
	) && (iSize == 0u) && (Info.Flags == 0u),
		"HTTP Digest extension-only info failed");
	testRequire(xrtHttpDigestInfoWrite(
		&Info, Written, sizeof(Written), &iSize
	) && (iSize == 0u),
		"HTTP Digest empty info write failed");
	testRequire(xrtHttpDigestInfoRead(
		XRT_STR_LITERAL(
			"qop=auth-int, rspauth=\"ABCD\", "
			"cnonce=\"c\", nc=ffffffff"
		),
		XHTTP_DIGEST_ALGORITHM_UNKNOWN,
		Decoded, sizeof(Decoded), &iSize, &Info
	) && (Info.Flags == XHTTP_DIGEST_INFO_HAS_RESPONSE) &&
		(Info.Qop == XHTTP_DIGEST_QOP_AUTH_INT) &&
		(Info.NonceCount == UINT32_MAX),
		"HTTP Digest extension algorithm info failed");
}



/* 验证必需参数组、线路形式、十六进制和算法长度约束。 */
static void testHttpDigestInfoReject(void)
{
	static const xstrview Invalid[] = {
		XRT_STR_INIT("nextnonce=n"),
		XRT_STR_INIT("nextnonce=\"\""),
		XRT_STR_INIT("nextnonce=\"n\", nextnonce=\"m\""),
		XRT_STR_INIT("qop=auth"),
		XRT_STR_INIT("qop=auth, rspauth=\"0123\", cnonce=\"c\""),
		XRT_STR_INIT(
			"qop=auth, rspauth=\"0123\", cnonce=\"c\", nc=00000000"
		),
		XRT_STR_INIT(
			"qop=\"auth\", rspauth=\"0123\", "
			"cnonce=\"c\", nc=00000001"
		),
		XRT_STR_INIT(
			"qop=future, rspauth=\"0123\", "
			"cnonce=\"c\", nc=00000001"
		),
		XRT_STR_INIT(
			"qop=auth, rspauth=0123, cnonce=\"c\", nc=00000001"
		),
		XRT_STR_INIT(
			"qop=auth, rspauth=\"01xz\", cnonce=\"c\", nc=00000001"
		),
		XRT_STR_INIT(
			"qop=auth, rspauth=\"01\\23\", cnonce=\"c\", nc=00000001"
		),
		XRT_STR_INIT(
			"qop=auth, rspauth=\"0123\", cnonce=c, nc=00000001"
		),
		XRT_STR_INIT(
			"qop=auth, rspauth=\"0123\", cnonce=\"c\", nc=\"00000001\""
		),
		XRT_STR_INIT(
			"qop=auth, rspauth=\"012\", cnonce=\"c\", nc=00000001"
		)
	};
	xhttpdigestinfo Info;
	char Output[128];
	size_t iSize;

	for ( size_t i = 0; i < (sizeof(Invalid) / sizeof(Invalid[0])); i++ ) {
		testRequire(!xrtHttpDigestInfoRead(
			Invalid[i], XHTTP_DIGEST_ALGORITHM_UNKNOWN,
			Output, sizeof(Output), &iSize, &Info
		), "HTTP Digest info accepted malformed input");
		xrtClearError();
	}
	testRequire(!xrtHttpDigestInfoRead(
		XRT_STR_LITERAL(
			"qop=auth, rspauth=\"0123\", cnonce=\"c\", nc=00000001"
		),
		XHTTP_DIGEST_ALGORITHM_SHA256,
		Output, sizeof(Output), &iSize, &Info
	), "HTTP Digest info accepted wrong SHA-256 response size");
	xrtClearError();
	testRequire(!xrtHttpDigestInfoRead(
		XRT_STR_LITERAL("nextnonce=\"n\""),
		(xhttpdigestalgorithm)99,
		Output, sizeof(Output), &iSize, &Info
	) && (xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"HTTP Digest info accepted invalid algorithm enum");
	xrtClearError();

	Info = (xhttpdigestinfo){
		XHTTP_DIGEST_INFO_HAS_NEXT_NONCE,
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XHTTP_DIGEST_QOP_NONE,
		0u,
		XRT_STR_INIT(""),
		{ 0 },
		{ 0 }
	};
	testRequire(!xrtHttpDigestInfoWrite(
		&Info, Output, sizeof(Output), &iSize
	) && (xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"HTTP Digest info writer accepted empty nextnonce");
	xrtClearError();
}



/* 验证未对齐描述符、短缓冲、别名和地址回绕边界。 */
static void testHttpDigestInfoMemoryContracts(void)
{
	uint8 InputStorage[sizeof(xhttpdigestinfo) + 2u];
	uint8 ResultStorage[sizeof(xhttpdigestinfo) + 2u];
	uint8 SizeStorage[sizeof(size_t) + 2u];
	xhttpdigestinfo* pInput =
		(xhttpdigestinfo*)(void*)(InputStorage + 1u);
	xhttpdigestinfo* pResult =
		(xhttpdigestinfo*)(void*)(ResultStorage + 1u);
	size_t* pSize = (size_t*)(void*)(SizeStorage + 1u);
	xhttpdigestinfo Input = {
		XHTTP_DIGEST_INFO_HAS_NEXT_NONCE |
		XHTTP_DIGEST_INFO_HAS_RESPONSE,
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XHTTP_DIGEST_QOP_AUTH,
		1u,
		XRT_STR_INIT("next"),
		XRT_STR_INIT(
			"0123456789abcdef0123456789abcdef"
			"0123456789abcdef0123456789abcdef"
		),
		XRT_STR_INIT("client")
	};
	xhttpdigestinfo Result;
	char Value[256];
	char Decoded[96];
	char Before[256];
	size_t iSize;
	size_t iRequired;

	memset(InputStorage, 0xA5, sizeof(InputStorage));
	memset(ResultStorage, 0xA5, sizeof(ResultStorage));
	memset(SizeStorage, 0xA5, sizeof(SizeStorage));
	memcpy(pInput, &Input, sizeof(Input));
	testRequire(xrtHttpDigestInfoWrite(
		pInput, Value, sizeof(Value), pSize
	), "HTTP Digest info rejected unaligned write descriptors");
	memcpy(&iSize, pSize, sizeof(iSize));
	testRequire(xrtHttpDigestInfoRead(
		(xstrview){ Value, iSize }, XHTTP_DIGEST_ALGORITHM_SHA256,
		Decoded, sizeof(Decoded), pSize, pResult
	), "HTTP Digest info rejected unaligned read descriptors");
	memcpy(&Result, pResult, sizeof(Result));
	testRequire(testHttpDigestInfoTextEqual(Result.NextNonce, "next") &&
		(InputStorage[0] == 0xA5) &&
		(InputStorage[sizeof(InputStorage) - 1u] == 0xA5) &&
		(ResultStorage[0] == 0xA5) &&
		(ResultStorage[sizeof(ResultStorage) - 1u] == 0xA5) &&
		(SizeStorage[0] == 0xA5) &&
		(SizeStorage[sizeof(SizeStorage) - 1u] == 0xA5),
		"HTTP Digest info unaligned contract mismatch");

	testRequire(xrtHttpDigestInfoRead(
		(xstrview){ Value, iSize }, XHTTP_DIGEST_ALGORITHM_SHA256,
		NULL, 0, &iRequired, &Result
	), "HTTP Digest info short-read query failed");
	memset(Decoded, 0x5A, sizeof(Decoded));
	memcpy(Before, Decoded, sizeof(Decoded));
	testRequire(!xrtHttpDigestInfoRead(
		(xstrview){ Value, iSize }, XHTTP_DIGEST_ALGORITHM_SHA256,
		Decoded, iRequired - 1u, &iRequired, &Result
	) && (memcmp(Decoded, Before, sizeof(Decoded)) == 0) &&
		(Result.Flags == 0u) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"HTTP Digest info short read was not atomic");
	xrtClearError();

	testRequire(xrtHttpDigestInfoWrite(
		&Input, NULL, 0, &iRequired
	), "HTTP Digest info short-write query failed");
	memset(Value, 0x5A, sizeof(Value));
	memcpy(Before, Value, sizeof(Value));
	testRequire(!xrtHttpDigestInfoWrite(
		&Input, Value, iRequired - 1u, &iSize
	) && (iSize == iRequired) &&
		(memcmp(Value, Before, sizeof(Value)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"HTTP Digest info short write was not atomic");
	xrtClearError();

	Input.Response = (xstrview){ (cstr)&iSize, 64u };
	testRequire(!xrtHttpDigestInfoWrite(
		&Input, NULL, 0, &iSize
	), "HTTP Digest info writer accepted a size alias");
	xrtClearError();
	testRequire(!xrtHttpDigestInfoWrite(
		(xhttpdigestinfo*)(uintptr_t)(UINTPTR_MAX - 1u),
		NULL, 0, &iSize
	), "HTTP Digest info writer accepted wrapping input");
	xrtClearError();
	testRequire(!xrtHttpDigestInfoRead(
		(xstrview){ Value, iRequired }, XHTTP_DIGEST_ALGORITHM_SHA256,
		Decoded, sizeof(Decoded), &iSize,
		(xhttpdigestinfo*)(uintptr_t)(UINTPTR_MAX - 1u)
	), "HTTP Digest info reader accepted wrapping result");
	xrtClearError();
}



int main(void)
{
	testHttpDigestInfoRoundTrip();
	testHttpDigestInfoOptional();
	testHttpDigestInfoReject();
	testHttpDigestInfoMemoryContracts();
	puts("[PASS] HTTP Digest Authentication-Info");
	return 0;
}
