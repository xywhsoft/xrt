#include "../test.h"

#include <xrt/http_auth.h>



/* 按字节比较 Digest 凭据中的借用文本。 */
static bool testHttpDigestAuthTextEqual(xstrview Text, cstr sExpected)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		(memcmp(Text.Data, sExpected, iSize) == 0);
}



/* 验证 RFC 7616 SHA-256 官方凭据向量的解析和规范往返。 */
static void testHttpDigestAuthRfcVector(void)
{
	static const char Value[] =
		"Digest username=\"Mufasa\", realm=\"http-auth@example.org\", "
		"uri=\"/dir/index.html\", algorithm=SHA-256, "
		"nonce=\"7ypf/xlj9XXwfDPEoM4URrv/xwf94BcCAzFZH4GiTo0v\", "
		"nc=00000001, "
		"cnonce=\"f2/wE4q74E6zIJEtWaHKaf5wv/H5QzzpXusqGemxURZJ\", "
		"qop=auth, "
		"response=\"753927fa0e85d155564e2e272a28d1802ca10daf4496794697cf8db5856cb6c1\", "
		"opaque=\"FQhe/qaU925kfnzjCev0ciny7QMkPqMAFRtzCUYo5tdS\"";
	xhttpdigestauth Digest;
	char Decoded[320];
	char Written[512];
	size_t iRequired;
	size_t iWritten;

	testRequire(xrtHttpDigestAuthRead(
		XRT_STR_LITERAL(Value), NULL, 0, &iRequired, &Digest
	) && (iRequired != 0) &&
		(Digest.Flags == (XHTTP_DIGEST_AUTH_HAS_OPAQUE |
		 XHTTP_DIGEST_AUTH_ALGORITHM_EXPLICIT)) &&
		(Digest.Algorithm == XHTTP_DIGEST_ALGORITHM_SHA256) &&
		(Digest.Qop == XHTTP_DIGEST_QOP_AUTH) &&
		(Digest.NonceCount == 1u) &&
		(Digest.Username.Data == NULL) &&
		testHttpDigestAuthTextEqual(Digest.AlgorithmName, "SHA-256"),
		"HTTP Digest RFC query mismatch");
	testRequire(xrtHttpDigestAuthRead(
		XRT_STR_LITERAL(Value),
		Decoded, sizeof(Decoded), &iRequired, &Digest
	) && testHttpDigestAuthTextEqual(Digest.Username, "Mufasa") &&
		testHttpDigestAuthTextEqual(
			Digest.Realm, "http-auth@example.org"
		) && testHttpDigestAuthTextEqual(
			Digest.Uri, "/dir/index.html"
		) && testHttpDigestAuthTextEqual(
			Digest.Response,
			"753927fa0e85d155564e2e272a28d1802ca10daf4496794697cf8db5856cb6c1"
		) && testHttpDigestAuthTextEqual(
			Digest.Opaque,
			"FQhe/qaU925kfnzjCev0ciny7QMkPqMAFRtzCUYo5tdS"
		), "HTTP Digest RFC parse mismatch");
	testRequire(xrtHttpDigestAuthWrite(
		&Digest, NULL, 0, &iWritten
	) && (iWritten == (sizeof(Value) - 1u)),
		"HTTP Digest RFC write query mismatch");
	testRequire(xrtHttpDigestAuthWrite(
		&Digest, Written, sizeof(Written), &iWritten
	) && (memcmp(Written, Value, iWritten) == 0),
		"HTTP Digest RFC canonical writer mismatch");
}



/* 验证 username*、语言标签、百分号解码和 userhash=false。 */
static void testHttpDigestAuthExtendedUsername(void)
{
	static const char Expected[] =
		"Digest username*=UTF-8'de'J%C3%A4s%C3%B8n%20Doe, "
		"realm=\"api@example.org\", uri=\"/doe.json\", "
		"algorithm=SHA-512-256, nonce=\"nonce\", nc=00000002, "
		"cnonce=\"client\", qop=auth, "
		"response=\"ae66e67d6b427bd3f120414a82e4acff38e8ecd9101d6c861229025f607a79dd\", "
		"opaque=\"opaque\", userhash=false";
	xhttpdigestauth Input = {
		XHTTP_DIGEST_AUTH_USERNAME_EXTENDED |
		XHTTP_DIGEST_AUTH_HAS_OPAQUE |
		XHTTP_DIGEST_AUTH_HAS_USERHASH |
		XHTTP_DIGEST_AUTH_ALGORITHM_EXPLICIT,
		XHTTP_DIGEST_ALGORITHM_SHA512_256,
		XHTTP_DIGEST_QOP_AUTH,
		2u,
		XRT_STR_INIT("J\xC3\xA4s\xC3\xB8n Doe"),
		XRT_STR_INIT("de"),
		XRT_STR_INIT("api@example.org"),
		XRT_STR_INIT("nonce"),
		XRT_STR_INIT("/doe.json"),
		XRT_STR_INIT("client"),
		XRT_STR_INIT(
			"AE66E67D6B427BD3F120414A82E4ACFF38E8ECD9101D6C861229025F607A79DD"
		),
		XRT_STR_INIT("opaque"),
		{ NULL, 0 }
	};
	xhttpdigestauth Digest;
	char Value[512];
	char Decoded[192];
	size_t iSize;
	size_t iDecoded;

	testRequire(xrtHttpDigestAuthWrite(
		&Input, Value, sizeof(Value), &iSize
	) && (iSize == (sizeof(Expected) - 1u)) &&
		(memcmp(Value, Expected, iSize) == 0),
		"HTTP Digest username* writer mismatch");
	testRequire(xrtHttpDigestAuthRead(
		(xstrview){ Value, iSize },
		Decoded, sizeof(Decoded), &iDecoded, &Digest
	) && ((Digest.Flags &
		 XHTTP_DIGEST_AUTH_USERNAME_EXTENDED) != 0) &&
		((Digest.Flags & XHTTP_DIGEST_AUTH_HAS_USERHASH) != 0) &&
		((Digest.Flags & XHTTP_DIGEST_AUTH_USERHASH) == 0) &&
		testHttpDigestAuthTextEqual(
			Digest.Username, "J\xC3\xA4s\xC3\xB8n Doe"
		) && testHttpDigestAuthTextEqual(
			Digest.UsernameLanguage, "de"
		), "HTTP Digest username* parse mismatch");
}



/* 验证用户哈希、默认 MD5 和未知扩展算法的表示能力。 */
static void testHttpDigestAuthAlgorithmInterop(void)
{
	xhttpdigestauth Digest;
	char Decoded[256];
	char Value[320];
	size_t iSize;

	testRequire(xrtHttpDigestAuthRead(
		XRT_STR_LITERAL(
			"Digest username=\"488869477bf257147b804c45308cd62ac4e25eb717b12b298c79e62dcea254ec\", "
			"realm=\"api@example.org\", uri=\"/doe.json\", "
			"algorithm=SHA-512-256, nonce=\"n\", nc=00000001, "
			"cnonce=\"c\", qop=auth, "
			"response=\"ae66e67d6b427bd3f120414a82e4acff38e8ecd9101d6c861229025f607a79dd\", "
			"userhash=true"
		),
		Decoded, sizeof(Decoded), &iSize, &Digest
	) && ((Digest.Flags & XHTTP_DIGEST_AUTH_USERHASH) != 0) &&
		(Digest.Username.Size == 64u),
		"HTTP Digest hashed username parse failed");
	testRequire(xrtHttpDigestAuthRead(
		XRT_STR_LITERAL(
			"Digest username=\"u\", realm=\"r\", uri=\"/\", "
			"nonce=\"n\", nc=00000001, cnonce=\"c\", qop=auth, "
			"response=\"0123456789abcdef0123456789abcdef\""
		),
		Decoded, sizeof(Decoded), &iSize, &Digest
	) && (Digest.Algorithm == XHTTP_DIGEST_ALGORITHM_MD5) &&
		((Digest.Flags &
		 XHTTP_DIGEST_AUTH_ALGORITHM_EXPLICIT) == 0),
		"HTTP Digest default MD5 metadata parse failed");

	Digest.Flags |= XHTTP_DIGEST_AUTH_ALGORITHM_EXPLICIT;
	Digest.Algorithm = XHTTP_DIGEST_ALGORITHM_UNKNOWN;
	Digest.AlgorithmName = XRT_STR_LITERAL("Fancy-32");
	Digest.Response = XRT_STR_LITERAL("01234567");
	testRequire(xrtHttpDigestAuthWrite(
		&Digest, Value, sizeof(Value), &iSize
	), "HTTP Digest unknown algorithm writer failed");
	testRequire(xrtHttpDigestAuthRead(
		(xstrview){ Value, iSize },
		Decoded, sizeof(Decoded), &iSize, &Digest
	) && (Digest.Algorithm == XHTTP_DIGEST_ALGORITHM_UNKNOWN) &&
		testHttpDigestAuthTextEqual(Digest.AlgorithmName, "Fancy-32"),
		"HTTP Digest unknown algorithm round trip failed");
}



/* 验证必填参数、线路形式和摘要长度边界。 */
static void testHttpDigestAuthReject(void)
{
	static const xstrview Invalid[] = {
		XRT_STR_INIT("Basic username=\"u\""),
		XRT_STR_INIT(
			"Digest realm=\"r\", uri=\"/\", nonce=\"n\", "
			"nc=00000001, cnonce=\"c\", qop=auth, "
			"response=\"0123456789abcdef0123456789abcdef\""
		),
		XRT_STR_INIT(
			"Digest username=\"u\", username*=UTF-8''u, realm=\"r\", "
			"uri=\"/\", nonce=\"n\", nc=00000001, cnonce=\"c\", "
			"qop=auth, response=\"0123456789abcdef0123456789abcdef\""
		),
		XRT_STR_INIT(
			"Digest username=u, realm=\"r\", uri=\"/\", nonce=\"n\", "
			"nc=00000001, cnonce=\"c\", qop=auth, "
			"response=\"0123456789abcdef0123456789abcdef\""
		),
		XRT_STR_INIT(
			"Digest username=\"u\", realm=\"r\", uri=\"/\", nonce=\"n\", "
			"nc=\"00000001\", cnonce=\"c\", qop=auth, "
			"response=\"0123456789abcdef0123456789abcdef\""
		),
		XRT_STR_INIT(
			"Digest username=\"u\", realm=\"r\", uri=\"/\", nonce=\"n\", "
			"nc=00000000, cnonce=\"c\", qop=auth, "
			"response=\"0123456789abcdef0123456789abcdef\""
		),
		XRT_STR_INIT(
			"Digest username=\"u\", realm=\"r\", uri=\"/\", nonce=\"n\", "
			"nc=00000001, cnonce=\"c\", qop=\"auth\", "
			"response=\"0123456789abcdef0123456789abcdef\""
		),
		XRT_STR_INIT(
			"Digest username=\"u\", realm=\"r\", uri=\"/\", nonce=\"n\", "
			"nc=00000001, cnonce=\"c\", qop=future, "
			"response=\"0123456789abcdef0123456789abcdef\""
		),
		XRT_STR_INIT(
			"Digest username=\"u\", realm=\"r\", uri=\"/\", nonce=\"n\", "
			"nc=00000001, cnonce=\"c\", qop=auth, response=0123"
		),
		XRT_STR_INIT(
			"Digest username=\"u\", realm=\"r\", uri=\"/\", nonce=\"n\", "
			"nc=00000001, cnonce=\"c\", qop=auth, response=\"xyz\""
		),
		XRT_STR_INIT(
			"Digest username=\"u\", realm=\"r\", uri=\"/\", "
			"algorithm=SHA-256, nonce=\"n\", nc=00000001, "
			"cnonce=\"c\", qop=auth, response=\"0123\""
		),
		XRT_STR_INIT(
			"Digest username*=ISO-8859-1''u, realm=\"r\", uri=\"/\", "
			"nonce=\"n\", nc=00000001, cnonce=\"c\", qop=auth, "
			"response=\"0123456789abcdef0123456789abcdef\""
		),
		XRT_STR_INIT(
			"Digest username*=UTF-8''u, realm=\"r\", uri=\"/\", "
			"nonce=\"n\", nc=00000001, cnonce=\"c\", qop=auth, "
			"response=\"0123456789abcdef0123456789abcdef\", userhash=true"
		),
		XRT_STR_INIT(
			"Digest username=\"abcd\", realm=\"r\", uri=\"/\", "
			"nonce=\"n\", nc=00000001, cnonce=\"c\", qop=auth, "
			"response=\"0123456789abcdef0123456789abcdef\", userhash=true"
		),
		XRT_STR_INIT(
			"Digest username=\"u\", realm=\"r\", uri=\"/\", "
			"algorithm=\"MD5\", nonce=\"n\", nc=00000001, "
			"cnonce=\"c\", qop=auth, "
			"response=\"0123456789abcdef0123456789abcdef\""
		)
	};
	xhttpdigestauth Digest;
	char Output[512];
	size_t iSize;

	for ( size_t i = 0; i < (sizeof(Invalid) / sizeof(Invalid[0])); i++ ) {
		testRequire(!xrtHttpDigestAuthRead(
			Invalid[i], Output, sizeof(Output), &iSize, &Digest
		), "HTTP Digest credentials accepted malformed input");
		xrtClearError();
	}
}



/* 验证未对齐描述符、短缓冲、别名和回绕范围。 */
static void testHttpDigestAuthMemoryContracts(void)
{
	uint8 InputStorage[sizeof(xhttpdigestauth) + 2u];
	uint8 ResultStorage[sizeof(xhttpdigestauth) + 2u];
	uint8 SizeStorage[sizeof(size_t) + 2u];
	xhttpdigestauth* pInput =
		(xhttpdigestauth*)(void*)(InputStorage + 1u);
	xhttpdigestauth* pResult =
		(xhttpdigestauth*)(void*)(ResultStorage + 1u);
	size_t* pSize = (size_t*)(void*)(SizeStorage + 1u);
	xhttpdigestauth Input = {
		0u,
		XHTTP_DIGEST_ALGORITHM_MD5,
		XHTTP_DIGEST_QOP_AUTH,
		1u,
		XRT_STR_INIT("u"),
		{ NULL, 0 },
		XRT_STR_INIT("r"),
		XRT_STR_INIT("n"),
		XRT_STR_INIT("/"),
		XRT_STR_INIT("c"),
		XRT_STR_INIT("0123456789abcdef0123456789abcdef"),
		{ NULL, 0 },
		{ NULL, 0 }
	};
	xhttpdigestauth Result;
	char Value[256];
	char Output[512];
	char Before[512];
	size_t iSize;
	size_t iRequired;

	memset(InputStorage, 0xA5, sizeof(InputStorage));
	memset(ResultStorage, 0xA5, sizeof(ResultStorage));
	memset(SizeStorage, 0xA5, sizeof(SizeStorage));
	memcpy(pInput, &Input, sizeof(Input));
	testRequire(xrtHttpDigestAuthWrite(
		pInput, Value, sizeof(Value), pSize
	), "HTTP Digest writer rejected unaligned input or size");
	memcpy(&iSize, pSize, sizeof(iSize));
	testRequire(xrtHttpDigestAuthRead(
		(xstrview){ Value, iSize },
		Output, sizeof(Output), pSize, pResult
	), "HTTP Digest reader rejected unaligned result");
	memcpy(&Result, pResult, sizeof(Result));
	testRequire(testHttpDigestAuthTextEqual(Result.Username, "u") &&
		(InputStorage[0] == 0xA5) &&
		(InputStorage[sizeof(InputStorage) - 1u] == 0xA5) &&
		(ResultStorage[0] == 0xA5) &&
		(ResultStorage[sizeof(ResultStorage) - 1u] == 0xA5) &&
		(SizeStorage[0] == 0xA5) &&
		(SizeStorage[sizeof(SizeStorage) - 1u] == 0xA5),
		"HTTP Digest unaligned contract mismatch");

	testRequire(xrtHttpDigestAuthRead(
		(xstrview){ Value, iSize },
		NULL, 0, &iRequired, &Result
	), "HTTP Digest short-read query failed");
	memset(Output, 0x5A, sizeof(Output));
	memcpy(Before, Output, sizeof(Output));
	testRequire(!xrtHttpDigestAuthRead(
		(xstrview){ Value, iSize },
		Output, iRequired - 1u, &iRequired, &Result
	) && (memcmp(Output, Before, sizeof(Output)) == 0) &&
		(Result.Flags == 0u) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"HTTP Digest short read was not atomic");
	xrtClearError();

	testRequire(xrtHttpDigestAuthWrite(
		&Input, NULL, 0, &iRequired
	), "HTTP Digest short-write query failed");
	memcpy(Before, Output, sizeof(Output));
	testRequire(!xrtHttpDigestAuthWrite(
		&Input, Output, iRequired - 1u, &iSize
	) && (iSize == iRequired) &&
		(memcmp(Output, Before, sizeof(Output)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"HTTP Digest short write was not atomic");
	xrtClearError();

	Input.Response = (xstrview){ (cstr)&iSize, 2u };
	testRequire(!xrtHttpDigestAuthWrite(
		&Input, NULL, 0, &iSize
	), "HTTP Digest writer accepted a size alias");
	xrtClearError();
	testRequire(!xrtHttpDigestAuthWrite(
		(xhttpdigestauth*)(uintptr_t)(UINTPTR_MAX - 1u),
		NULL, 0, &iSize
	), "HTTP Digest writer accepted wrapping input");
	xrtClearError();
	testRequire(!xrtHttpDigestAuthRead(
		(xstrview){ Value, iRequired },
		Output, sizeof(Output), &iSize,
		(xhttpdigestauth*)(uintptr_t)(UINTPTR_MAX - 1u)
	), "HTTP Digest reader accepted wrapping result");
	xrtClearError();
}



int main(void)
{
	testHttpDigestAuthRfcVector();
	testHttpDigestAuthExtendedUsername();
	testHttpDigestAuthAlgorithmInterop();
	testHttpDigestAuthReject();
	testHttpDigestAuthMemoryContracts();
	puts("[PASS] HTTP Digest credentials");
	return 0;
}
