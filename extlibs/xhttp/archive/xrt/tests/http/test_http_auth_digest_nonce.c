#include "../test.h"

#include <xrt/http_auth.h>



static const uint8 testHttpDigestNonceKey[32] = {
	0x00u, 0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u,
	0x08u, 0x09u, 0x0Au, 0x0Bu, 0x0Cu, 0x0Du, 0x0Eu, 0x0Fu,
	0x10u, 0x11u, 0x12u, 0x13u, 0x14u, 0x15u, 0x16u, 0x17u,
	0x18u, 0x19u, 0x1Au, 0x1Bu, 0x1Cu, 0x1Du, 0x1Eu, 0x1Fu
};



static const uint8 testHttpDigestNonceSalt[16] = {
	0x00u, 0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u,
	0x08u, 0x09u, 0x0Au, 0x0Bu, 0x0Cu, 0x0Du, 0x0Eu, 0x0Fu
};



/* 验证独立 HMAC/Base64URL 向量和精确时间边界。 */
static void testHttpDigestNonceVector(void)
{
	static const char Expected[] =
		"AQAAAABlU_EAAAECAwQFBgcICQoLDA0O"
		"D6890fuzkgWMTSXpIb5Qq1GoN4f_sbE4ls8JpunSkaKV";
	xbytesview Key = {
		testHttpDigestNonceKey, sizeof(testHttpDigestNonceKey)
	};
	xbytesview Context = XRT_BYTES_LITERAL("api@example.test");
	char Nonce[XRT_HTTP_DIGEST_NONCE_TEXT_SIZE];
	int64 iIssued;
	size_t iSize;

	testRequire(xrtHttpDigestNonceWrite(
		Key, Context, INT64_C(1700000000),
		testHttpDigestNonceSalt,
		NULL, 0, &iSize
	) && (iSize == XRT_HTTP_DIGEST_NONCE_TEXT_SIZE),
		"HTTP Digest nonce query mismatch");
	testRequire(xrtHttpDigestNonceWrite(
		Key, Context, INT64_C(1700000000),
		testHttpDigestNonceSalt,
		Nonce, sizeof(Nonce), &iSize
	) && (iSize == (sizeof(Expected) - 1u)) &&
		(memcmp(Nonce, Expected, iSize) == 0),
		"HTTP Digest nonce vector mismatch");
	testRequire((xrtHttpDigestNonceVerify(
		(xstrview){ Nonce, iSize }, Key, Context,
		INT64_C(1700000060), 60, 5, &iIssued
	) == XHTTP_DIGEST_NONCE_VALID) &&
		(iIssued == INT64_C(1700000000)),
		"HTTP Digest nonce inclusive lifetime mismatch");
	testRequire(xrtHttpDigestNonceVerify(
		(xstrview){ Nonce, iSize }, Key, Context,
		INT64_C(1700000061), 60, 5, &iIssued
	) == XHTTP_DIGEST_NONCE_STALE,
		"HTTP Digest nonce stale boundary mismatch");
	testRequire(xrtHttpDigestNonceVerify(
		(xstrview){ Nonce, iSize }, Key, Context,
		INT64_C(1699999995), 60, 5, &iIssued
	) == XHTTP_DIGEST_NONCE_VALID,
		"HTTP Digest nonce future skew boundary mismatch");
	testRequire(xrtHttpDigestNonceVerify(
		(xstrview){ Nonce, iSize }, Key, Context,
		INT64_C(1699999994), 60, 5, &iIssued
	) == XHTTP_DIGEST_NONCE_INVALID,
		"HTTP Digest nonce accepted excessive future skew");
}



/* 验证线路、签名、密钥与上下文失败都只返回 INVALID。 */
static void testHttpDigestNonceInvalid(void)
{
	xbytesview Key = {
		testHttpDigestNonceKey, sizeof(testHttpDigestNonceKey)
	};
	xbytesview Context = XRT_BYTES_LITERAL("api@example.test");
	uint8 WrongKey[sizeof(testHttpDigestNonceKey)];
	char Nonce[XRT_HTTP_DIGEST_NONCE_TEXT_SIZE];
	int64 iIssued = INT64_C(123456789);
	size_t iSize;

	testRequire(xrtHttpDigestNonceWrite(
		Key, Context, INT64_C(1700000000),
		testHttpDigestNonceSalt,
		Nonce, sizeof(Nonce), &iSize
	), "HTTP Digest nonce invalid fixture failed");
	Nonce[sizeof(Nonce) - 1u] =
		Nonce[sizeof(Nonce) - 1u] == 'A' ? 'B' : 'A';
	testRequire((xrtHttpDigestNonceVerify(
		(xstrview){ Nonce, sizeof(Nonce) }, Key, Context,
		INT64_C(1700000000), 60, 0, &iIssued
	) == XHTTP_DIGEST_NONCE_INVALID) &&
		(iIssued == INT64_C(123456789)),
		"HTTP Digest nonce accepted a modified signature");
	Nonce[sizeof(Nonce) - 1u] = '=';
	testRequire(xrtHttpDigestNonceVerify(
		(xstrview){ Nonce, sizeof(Nonce) }, Key, Context,
		INT64_C(1700000000), 60, 0, NULL
	) == XHTTP_DIGEST_NONCE_INVALID,
		"HTTP Digest nonce accepted padding");
	testRequire(xrtHttpDigestNonceVerify(
		XRT_STR_LITERAL("short"), Key, Context,
		INT64_C(1700000000), 60, 0, NULL
	) == XHTTP_DIGEST_NONCE_INVALID,
		"HTTP Digest nonce accepted a short token");
	xrtClearError();

	testRequire(xrtHttpDigestNonceWrite(
		Key, Context, INT64_C(1700000000),
		testHttpDigestNonceSalt,
		Nonce, sizeof(Nonce), &iSize
	), "HTTP Digest nonce context fixture failed");
	testRequire(xrtHttpDigestNonceVerify(
		(xstrview){ Nonce, sizeof(Nonce) }, Key,
		XRT_BYTES_LITERAL("other"),
		INT64_C(1700000000), 60, 0, NULL
	) == XHTTP_DIGEST_NONCE_INVALID,
		"HTTP Digest nonce accepted the wrong context");
	memcpy(WrongKey, testHttpDigestNonceKey, sizeof(WrongKey));
	WrongKey[0] ^= 0x80u;
	testRequire(xrtHttpDigestNonceVerify(
		(xstrview){ Nonce, sizeof(Nonce) },
		(xbytesview){ WrongKey, sizeof(WrongKey) }, Context,
		INT64_C(1700000000), 60, 0, NULL
	) == XHTTP_DIGEST_NONCE_INVALID,
		"HTTP Digest nonce accepted the wrong key");
	testRequire(xrtGetError() == NULL,
		"HTTP Digest nonce invalid input changed the error slot");
}



/* 验证短缓冲、未对齐描述符、别名与参数错误。 */
static void testHttpDigestNonceMemory(void)
{
	xbytesview Key = {
		testHttpDigestNonceKey, sizeof(testHttpDigestNonceKey)
	};
	xbytesview Context = XRT_BYTES_LITERAL("api@example.test");
	uint8 SizeStorage[sizeof(size_t) + 2u];
	uint8 IssuedStorage[sizeof(int64) + 2u];
	uint8 Alias[96];
	size_t* pSize = (size_t*)(void*)(SizeStorage + 1u);
	int64* pIssued = (int64*)(void*)(IssuedStorage + 1u);
	char Nonce[XRT_HTTP_DIGEST_NONCE_TEXT_SIZE];
	char Before[XRT_HTTP_DIGEST_NONCE_TEXT_SIZE];
	size_t iSize;
	int64 iIssued;

	memset(SizeStorage, 0xA5, sizeof(SizeStorage));
	memset(IssuedStorage, 0xA5, sizeof(IssuedStorage));
	testRequire(xrtHttpDigestNonceWrite(
		Key, Context, INT64_C(1700000000),
		testHttpDigestNonceSalt,
		Nonce, sizeof(Nonce), pSize
	) && (SizeStorage[0] == 0xA5) &&
		(SizeStorage[sizeof(SizeStorage) - 1u] == 0xA5) &&
		(xrtHttpDigestNonceVerify(
			(xstrview){ Nonce, sizeof(Nonce) }, Key, Context,
			INT64_C(1700000000), 60, 0, pIssued
		 ) == XHTTP_DIGEST_NONCE_VALID) &&
		(IssuedStorage[0] == 0xA5) &&
		(IssuedStorage[sizeof(IssuedStorage) - 1u] == 0xA5),
		"HTTP Digest nonce rejected unaligned descriptors");
	memcpy(&iIssued, pIssued, sizeof(iIssued));
	testRequire(iIssued == INT64_C(1700000000),
		"HTTP Digest nonce unaligned issued time mismatch");

	memset(Nonce, 0x5A, sizeof(Nonce));
	memcpy(Before, Nonce, sizeof(Before));
	testRequire(!xrtHttpDigestNonceWrite(
		Key, Context, INT64_C(1700000000),
		testHttpDigestNonceSalt,
		Nonce, sizeof(Nonce) - 1u, &iSize
	) && (iSize == sizeof(Nonce)) &&
		(memcmp(Nonce, Before, sizeof(Nonce)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"HTTP Digest nonce short output was not atomic");
	xrtClearError();

	memcpy(Alias + 1u, testHttpDigestNonceSalt,
		sizeof(testHttpDigestNonceSalt));
	testRequire(!xrtHttpDigestNonceWrite(
		Key, Context, INT64_C(1700000000),
		Alias + 1u, Alias, sizeof(Nonce), &iSize
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Digest nonce accepted overlapping salt and output");
	xrtClearError();
	testRequire(!xrtHttpDigestNonceWrite(
		(xbytesview){ testHttpDigestNonceKey, 16u },
		Context, INT64_C(1700000000), testHttpDigestNonceSalt,
		Nonce, sizeof(Nonce), &iSize
	) && (xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"HTTP Digest nonce accepted a short HMAC key");
	xrtClearError();
	testRequire(!xrtHttpDigestNonceWrite(
		Key, Context, -1, testHttpDigestNonceSalt,
		Nonce, sizeof(Nonce), &iSize
	) && (xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"HTTP Digest nonce accepted a negative issue time");
	xrtClearError();
	testRequire(xrtHttpDigestNonceVerify(
		(xstrview){ Nonce, sizeof(Nonce) }, Key, Context,
		INT64_C(1700000000), 0, 0, NULL
	) == XHTTP_DIGEST_NONCE_ERROR,
		"HTTP Digest nonce accepted a zero lifetime");
	xrtClearError();
	testRequire(xrtHttpDigestNonceVerify(
		(xstrview){ (cstr)(uintptr_t)(UINTPTR_MAX - 1u), 76u },
		Key, Context, INT64_C(1700000000), 60, 0, NULL
	) == XHTTP_DIGEST_NONCE_ERROR,
		"HTTP Digest nonce accepted a wrapping token");
	xrtClearError();
	testRequire(xrtHttpDigestNonceVerify(
		(xstrview){ Nonce, sizeof(Nonce) }, Key, Context,
		INT64_C(1700000000), 60, 0,
		(int64*)(void*)Nonce
	) == XHTTP_DIGEST_NONCE_ERROR,
		"HTTP Digest nonce accepted an issued-time alias");
	xrtClearError();
}



int main(void)
{
	testHttpDigestNonceVector();
	testHttpDigestNonceInvalid();
	testHttpDigestNonceMemory();
	puts("[PASS] HTTP Digest stateless nonce");
	return 0;
}
