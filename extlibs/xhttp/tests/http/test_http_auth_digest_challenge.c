#include "../test.h"

#include <xrt/http_auth.h>



/* 按字节比较 Digest challenge 中的借用文本。 */
static bool testHttpDigestTextEqual(xstrview Text, cstr sExpected)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		(memcmp(Text.Data, sExpected, iSize) == 0);
}



/* 验证所有标准参数的规范写出、查询和解码往返。 */
static void testHttpDigestChallengeRoundTrip(void)
{
	static const char Expected[] =
		"Digest realm=\"api\\\"zone\", domain=\"/ /v2\", "
		"nonce=\"n\\\\x\", opaque=\"o\", stale=true, "
		"algorithm=SHA-512-256, qop=\"auth, auth-int\", "
		"charset=UTF-8, userhash=true";
	xhttpdigestchallenge Input = {
		XHTTP_DIGEST_CHALLENGE_HAS_DOMAIN |
		XHTTP_DIGEST_CHALLENGE_HAS_OPAQUE |
		XHTTP_DIGEST_CHALLENGE_HAS_STALE |
		XHTTP_DIGEST_CHALLENGE_STALE |
		XHTTP_DIGEST_CHALLENGE_UTF8 |
		XHTTP_DIGEST_CHALLENGE_HAS_USERHASH |
		XHTTP_DIGEST_CHALLENGE_USERHASH |
		XHTTP_DIGEST_CHALLENGE_QOP_AUTH |
		XHTTP_DIGEST_CHALLENGE_QOP_AUTH_INT |
		XHTTP_DIGEST_CHALLENGE_ALGORITHM_EXPLICIT,
		XHTTP_DIGEST_ALGORITHM_SHA512_256,
		XRT_STR_INIT("api\"zone"),
		XRT_STR_INIT("/ /v2"),
		XRT_STR_INIT("n\\x"),
		XRT_STR_INIT("o"),
		{ NULL, 0 }
	};
	xhttpdigestchallenge Challenge;
	char Value[256];
	char Decoded[64];
	size_t iSize;
	size_t iDecoded;

	testRequire(xrtHttpDigestChallengeWrite(
		&Input, NULL, 0, &iSize
	) && (iSize == (sizeof(Expected) - 1u)),
		"HTTP Digest challenge query mismatch");
	testRequire(xrtHttpDigestChallengeWrite(
		&Input, Value, sizeof(Value), &iSize
	) && (memcmp(Value, Expected, iSize) == 0),
		"HTTP Digest challenge writer mismatch");
	testRequire(xrtHttpDigestChallengeRead(
		(xstrview){ Value, iSize },
		NULL, 0, &iDecoded, &Challenge
	) && (iDecoded == 17u) &&
		(Challenge.Flags == Input.Flags) &&
		(Challenge.Algorithm == XHTTP_DIGEST_ALGORITHM_SHA512_256) &&
		(Challenge.Realm.Data == NULL) &&
		testHttpDigestTextEqual(
			Challenge.AlgorithmName, "SHA-512-256"
		), "HTTP Digest challenge read query mismatch");
	testRequire(xrtHttpDigestChallengeRead(
		(xstrview){ Value, iSize },
		Decoded, sizeof(Decoded), &iDecoded, &Challenge
	) && (iDecoded == 17u) &&
		testHttpDigestTextEqual(Challenge.Realm, "api\"zone") &&
		testHttpDigestTextEqual(Challenge.Domain, "/ /v2") &&
		testHttpDigestTextEqual(Challenge.Nonce, "n\\x") &&
		testHttpDigestTextEqual(Challenge.Opaque, "o"),
		"HTTP Digest challenge round trip mismatch");
}



/* 验证默认算法、扩展算法、未知 qop 和参数重排保持可扩展。 */
static void testHttpDigestChallengeInterop(void)
{
	xhttpdigestchallenge Challenge;
	char Decoded[128];
	size_t iSize;

	testRequire(xrtHttpDigestChallengeRead(
		XRT_STR_LITERAL(
			"Digest qop=\"auth\", nonce=\"n\", extension=ok, "
			"realm=\"api\""
		),
		Decoded, sizeof(Decoded), &iSize, &Challenge
	) && (Challenge.Algorithm == XHTTP_DIGEST_ALGORITHM_MD5) &&
		((Challenge.Flags &
		 XHTTP_DIGEST_CHALLENGE_ALGORITHM_EXPLICIT) == 0) &&
		((Challenge.Flags & XHTTP_DIGEST_CHALLENGE_QOP_AUTH) != 0),
		"HTTP Digest default algorithm interoperability failed");
	testRequire(xrtHttpDigestChallengeRead(
		XRT_STR_LITERAL(
			"Digest realm=\"\", nonce=\"\", "
			"qop=\"future, AUTH-INT\", algorithm=Fancy-512"
		),
		Decoded, sizeof(Decoded), &iSize, &Challenge
	) && (Challenge.Algorithm == XHTTP_DIGEST_ALGORITHM_UNKNOWN) &&
		((Challenge.Flags & XHTTP_DIGEST_CHALLENGE_QOP_AUTH) == 0) &&
		((Challenge.Flags & XHTTP_DIGEST_CHALLENGE_QOP_AUTH_INT) != 0) &&
		testHttpDigestTextEqual(Challenge.AlgorithmName, "Fancy-512"),
		"HTTP Digest extension algorithm interoperability failed");
	testRequire(xrtHttpDigestChallengeRead(
		XRT_STR_LITERAL(
			"Digest realm=\"a\\\"b\", nonce=\"n\\\\x\", "
			"qop=\", auth,, future,\""
		),
		Decoded, sizeof(Decoded), &iSize, &Challenge
	) && testHttpDigestTextEqual(Challenge.Realm, "a\"b") &&
		testHttpDigestTextEqual(Challenge.Nonce, "n\\x") &&
		((Challenge.Flags & XHTTP_DIGEST_CHALLENGE_QOP_AUTH) != 0),
		"HTTP Digest quoted-pair or list interoperability failed");
}



/* 验证必填参数、重复参数和 RFC 线路形式均被严格约束。 */
static void testHttpDigestChallengeReject(void)
{
	static const xstrview Invalid[] = {
		XRT_STR_INIT("Basic realm=\"a\""),
		XRT_STR_INIT("Digest token68"),
		XRT_STR_INIT("Digest nonce=\"n\", qop=\"auth\""),
		XRT_STR_INIT("Digest realm=\"a\", qop=\"auth\""),
		XRT_STR_INIT("Digest realm=\"a\", nonce=\"n\""),
		XRT_STR_INIT(
			"Digest realm=\"a\", REALM=\"b\", "
			"nonce=\"n\", qop=\"auth\""
		),
		XRT_STR_INIT("Digest realm=a, nonce=\"n\", qop=\"auth\""),
		XRT_STR_INIT("Digest realm=\"a\", nonce=n, qop=\"auth\""),
		XRT_STR_INIT("Digest realm=\"a\", nonce=\"n\", qop=auth"),
		XRT_STR_INIT(
			"Digest realm=\"a\", nonce=\"n\", qop=\"auth\", "
			"algorithm=\"SHA-256\""
		),
		XRT_STR_INIT(
			"Digest realm=\"a\", nonce=\"n\", qop=\"auth\", "
			"stale=\"true\""
		),
		XRT_STR_INIT(
			"Digest realm=\"a\", nonce=\"n\", qop=\"auth\", "
			"charset=Latin-1"
		),
		XRT_STR_INIT(
			"Digest realm=\"a\", nonce=\"n\", qop=\"auth\", "
			"userhash=yes"
		),
		XRT_STR_INIT(
			"Digest realm=\"a\", nonce=\"n\", qop=\"auth;auth-int\""
		),
		XRT_STR_INIT("Digest realm=\"a\", nonce=\"n\", qop=\"\"")
	};
	xhttpdigestchallenge Challenge;
	char Output[128];
	size_t iSize;

	for ( size_t i = 0; i < (sizeof(Invalid) / sizeof(Invalid[0])); i++ ) {
		testRequire(!xrtHttpDigestChallengeRead(
			Invalid[i], Output, sizeof(Output), &iSize, &Challenge
		), "HTTP Digest challenge accepted malformed input");
		testRequire(
			xrtErrorKind(xrtGetError()) == XERR_VALUE,
			"HTTP Digest malformed challenge error mismatch"
		);
		xrtClearError();
	}
}



/* 验证 writer 的语义约束和未知算法扩展出口。 */
static void testHttpDigestChallengeWriterValidation(void)
{
	xhttpdigestchallenge Challenge = {
		XHTTP_DIGEST_CHALLENGE_QOP_AUTH,
		XHTTP_DIGEST_ALGORITHM_MD5,
		XRT_STR_INIT("api"),
		{ NULL, 0 },
		XRT_STR_INIT("nonce"),
		{ NULL, 0 },
		{ NULL, 0 }
	};
	char Output[160];
	size_t iSize;

	testRequire(xrtHttpDigestChallengeWrite(
		&Challenge, Output, sizeof(Output), &iSize
	) && (memcmp(
		Output,
		"Digest realm=\"api\", nonce=\"nonce\", qop=\"auth\"",
		iSize
	) == 0), "HTTP Digest default writer mismatch");
	Challenge.Flags &= ~XHTTP_DIGEST_CHALLENGE_QOP_AUTH;
	testRequire(!xrtHttpDigestChallengeWrite(
		&Challenge, NULL, 0, &iSize
	) && (xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"HTTP Digest writer unknown qop error mismatch");
	xrtClearError();
	Challenge.Flags |= XHTTP_DIGEST_CHALLENGE_QOP_AUTH;
	Challenge.Flags |= XHTTP_DIGEST_CHALLENGE_ALGORITHM_EXPLICIT;
	Challenge.Algorithm = XHTTP_DIGEST_ALGORITHM_UNKNOWN;
	Challenge.AlgorithmName = XRT_STR_LITERAL("Fancy-512");
	testRequire(xrtHttpDigestChallengeWrite(
		&Challenge, Output, sizeof(Output), &iSize
	) && (iSize == 66u) && (memcmp(
		Output,
		"Digest realm=\"api\", nonce=\"nonce\", "
		"algorithm=Fancy-512, qop=\"auth\"",
		iSize
	) == 0), "HTTP Digest extension algorithm writer failed");

	Challenge.AlgorithmName = XRT_STR_LITERAL("bad value");
	testRequire(!xrtHttpDigestChallengeWrite(
		&Challenge, NULL, 0, &iSize
	), "HTTP Digest writer accepted invalid extension algorithm");
	xrtClearError();
	Challenge.Algorithm = XHTTP_DIGEST_ALGORITHM_SHA256;
	Challenge.AlgorithmName = XRT_STR_LITERAL("MD5");
	testRequire(!xrtHttpDigestChallengeWrite(
		&Challenge, NULL, 0, &iSize
	), "HTTP Digest writer accepted mismatched algorithm name");
	xrtClearError();
	Challenge.Flags = XHTTP_DIGEST_CHALLENGE_STALE |
		XHTTP_DIGEST_CHALLENGE_QOP_AUTH;
	Challenge.Algorithm = XHTTP_DIGEST_ALGORITHM_MD5;
	Challenge.AlgorithmName = (xstrview){ NULL, 0 };
	testRequire(!xrtHttpDigestChallengeWrite(
		&Challenge, NULL, 0, &iSize
	), "HTTP Digest writer accepted a stale value without presence");
	xrtClearError();
}



/* 验证未对齐描述符、短缓冲、别名和回绕范围的失败原子性。 */
static void testHttpDigestChallengeMemoryContracts(void)
{
	uint8 InputStorage[sizeof(xhttpdigestchallenge) + 2u];
	uint8 ResultStorage[sizeof(xhttpdigestchallenge) + 2u];
	uint8 SizeStorage[sizeof(size_t) + 2u];
	xhttpdigestchallenge* pInput =
		(xhttpdigestchallenge*)(void*)(InputStorage + 1u);
	xhttpdigestchallenge* pResult =
		(xhttpdigestchallenge*)(void*)(ResultStorage + 1u);
	size_t* pSize = (size_t*)(void*)(SizeStorage + 1u);
	xhttpdigestchallenge Input = {
		XHTTP_DIGEST_CHALLENGE_QOP_AUTH,
		XHTTP_DIGEST_ALGORITHM_MD5,
		XRT_STR_INIT("api"),
		{ NULL, 0 },
		XRT_STR_INIT("nonce"),
		{ NULL, 0 },
		{ NULL, 0 }
	};
	xhttpdigestchallenge Result;
	char Value[96];
	char Output[32];
	char Before[32];
	size_t iSize;
	xerror* pOld;

	memset(InputStorage, 0xA5, sizeof(InputStorage));
	memset(ResultStorage, 0xA5, sizeof(ResultStorage));
	memset(SizeStorage, 0xA5, sizeof(SizeStorage));
	memcpy(pInput, &Input, sizeof(Input));
	testRequire(xrtHttpDigestChallengeWrite(
		pInput, Value, sizeof(Value), pSize
	), "HTTP Digest challenge rejected unaligned input or size");
	memcpy(&iSize, pSize, sizeof(iSize));
	testRequire(xrtHttpDigestChallengeRead(
		(xstrview){ Value, iSize },
		Output, sizeof(Output), pSize, pResult
	), "HTTP Digest challenge rejected unaligned result");
	memcpy(&Result, pResult, sizeof(Result));
	testRequire(testHttpDigestTextEqual(Result.Realm, "api") &&
		(InputStorage[0] == 0xA5) &&
		(InputStorage[sizeof(InputStorage) - 1u] == 0xA5) &&
		(ResultStorage[0] == 0xA5) &&
		(ResultStorage[sizeof(ResultStorage) - 1u] == 0xA5) &&
		(SizeStorage[0] == 0xA5) &&
		(SizeStorage[sizeof(SizeStorage) - 1u] == 0xA5),
		"HTTP Digest challenge unaligned contract mismatch");

	memset(Output, 0x5A, sizeof(Output));
	memcpy(Before, Output, sizeof(Output));
	memset(&Result, 0xA5, sizeof(Result));
	iSize = 71u;
	testRequire(!xrtHttpDigestChallengeRead(
		XRT_STR_LITERAL(
			"Digest realm=\"api\", nonce=\"nonce\", qop=\"auth\""
		),
		Output, 7u, &iSize, &Result
	) && (iSize == 8u) && (Result.Flags == 0u) &&
		(memcmp(Output, Before, sizeof(Output)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"HTTP Digest challenge short read was not atomic");
	xrtClearError();

	iSize = 91u;
	memcpy(Before, Output, sizeof(Output));
	testRequire(!xrtHttpDigestChallengeWrite(
		&Input, Output, 8u, &iSize
	) && (iSize == 45u) &&
		(memcmp(Output, Before, sizeof(Output)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"HTTP Digest challenge short write was not atomic");
	xrtClearError();

	Input.Realm = (xstrview){ (cstr)&iSize, 1u };
	testRequire(!xrtHttpDigestChallengeWrite(
		&Input, NULL, 0, &iSize
	), "HTTP Digest writer accepted a size alias");
	xrtClearError();
	Input.Realm = XRT_STR_LITERAL("api");
	testRequire(!xrtHttpDigestChallengeWrite(
		(xhttpdigestchallenge*)(uintptr_t)(UINTPTR_MAX - 1u),
		NULL, 0, &iSize
	), "HTTP Digest writer accepted wrapping input");
	xrtClearError();
	testRequire(!xrtHttpDigestChallengeRead(
		XRT_STR_LITERAL(
			"Digest realm=\"api\", nonce=\"nonce\", qop=\"auth\""
		),
		Output, sizeof(Output), &iSize,
		(xhttpdigestchallenge*)(uintptr_t)(UINTPTR_MAX - 1u)
	), "HTTP Digest reader accepted wrapping result");
	xrtClearError();

	pOld = xrtErrorCreate(XERR_IO, "test.old", 17, "old error");
	testRequire(pOld != NULL, "HTTP Digest stale error setup failed");
	xrtSetError(pOld);
	xrtErrorFree(pOld);
	testRequire(!xrtHttpDigestChallengeRead(
		XRT_STR_LITERAL("Basic realm=\"api\""),
		Output, sizeof(Output), &iSize, &Result
	) && (xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"HTTP Digest scheme mismatch preserved a stale error");
	xrtClearError();
}



int main(void)
{
	testHttpDigestChallengeRoundTrip();
	testHttpDigestChallengeInterop();
	testHttpDigestChallengeReject();
	testHttpDigestChallengeWriterValidation();
	testHttpDigestChallengeMemoryContracts();
	puts("[PASS] HTTP Digest challenge");
	return 0;
}
