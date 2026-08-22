#include "../test.h"

#include <xrt/http_digest.h>



/* 解析单成员摘要以供 SHA-2 校验测试使用。 */
static bool testHttpDigestSha2Parse(
	xstrview Value,
	xhttpdigest* pDigest
)
{
	xhttpdigestcursor Cursor;

	xrtHttpDigestCursorInit(&Cursor);
	return xrtHttpDigestNext(
		Value, &Cursor, pDigest
	) == XHTTP_NEXT_ITEM;
}



/* 验证 RFC 9530 附录 D 的 SHA-256 与 SHA-512 向量。 */
static void testHttpDigestSha2Vectors(void)
{
	static const char Input[] = "{\"hello\": \"world\"}";
	static const char Sha256[] =
		"sha-256=:X48E9qOokqqrvdts8nOJRJN3OWDUoyWxBf7kbu9DBPE=:";
	static const char Sha512[] =
		"sha-512=:WZDPaVn/7XgHaAy8pmojAkGWoRx2UFChF41A2svX+TaPm+"
		"AbwAgBWnrIiYllu7BNNyealdVLvRwEmTHWXvJwew==:";
	char arrOutput[128];
	size_t iSize;

	testRequire(
		xrtHttpDigestSha256Write(
			Input, sizeof(Input) - 1u,
			arrOutput, sizeof(arrOutput), &iSize
		) && (iSize == sizeof(Sha256) - 1u) &&
		(memcmp(arrOutput, Sha256, iSize) == 0),
		"HTTP SHA-256 digest vector mismatch"
	);
	testRequire(
		xrtHttpDigestSha512Write(
			Input, sizeof(Input) - 1u,
			arrOutput, sizeof(arrOutput), &iSize
		) && (iSize == sizeof(Sha512) - 1u) &&
		(memcmp(arrOutput, Sha512, iSize) == 0),
		"HTTP SHA-512 digest vector mismatch"
	);
}



/* 验证匹配、不匹配、不支持与错误结果相互独立。 */
static void testHttpDigestSha2Verify(void)
{
	static const char Input[] = "{\"hello\": \"world\"}";
	xhttpdigest Digest;

	testRequire(
		testHttpDigestSha2Parse(
			XRT_STR_LITERAL(
				"sha-256=:X48E9qOokqqrvdts8nOJRJN3OWDUoyWxBf7kbu9DBPE=:"
			), &Digest
		) && (xrtHttpDigestSha2Verify(
			&Digest, Input, sizeof(Input) - 1u
		) == XHTTP_DIGEST_MATCH_OK),
		"HTTP SHA-256 digest verification failed"
	);
	testRequire(
		xrtHttpDigestSha2Verify(
			&Digest, "changed", 7u
		) == XHTTP_DIGEST_MATCH_MISMATCH,
		"HTTP digest mismatch was not reported"
	);
	testRequire(
		testHttpDigestSha2Parse(
			XRT_STR_LITERAL("future=:AA==:"), &Digest
		) && (xrtHttpDigestSha2Verify(
			&Digest, Input, sizeof(Input) - 1u
		) == XHTTP_DIGEST_MATCH_UNSUPPORTED),
		"unknown HTTP digest algorithm was not preserved"
	);
	memset(&Digest, 0, sizeof(Digest));
	testRequire(
		xrtHttpDigestSha2Verify(
			&Digest, Input, sizeof(Input) - 1u
		) == XHTTP_DIGEST_MATCH_ERROR,
		"invalid HTTP digest descriptor was not rejected"
	);
}



/* 验证 SHA-2 便利层的内存、描述符和短缓冲契约。 */
static void testHttpDigestSha2Memory(void)
{
	union {
		uint64 Align;
		uint8 Bytes[sizeof(size_t) + 1u];
	} SizeStorage;
	union {
		uint64 Align;
		uint8 Bytes[sizeof(xhttpdigest) + 1u];
	} DigestStorage;
	size_t* pUnalignedSize = (size_t*)(SizeStorage.Bytes + 1u);
	xhttpdigest* pUnalignedDigest = (xhttpdigest*)(
		DigestStorage.Bytes + 1u
	);
	uint8 arrData[] = { 0xA5u, 'a', 'b', 'c' };
	char arrOutput[96];
	char arrBefore[96];
	char arrInPlace[96] = "abc";
	xhttpdigest Digest;
	xhttpdigest Invalid;
	size_t iSize;

	testRequire(
		xrtHttpDigestSha256Write(
			arrData + 1u, 3u,
			arrOutput, sizeof(arrOutput), pUnalignedSize
		),
		"HTTP SHA-2 writer rejected unaligned input or size"
	);
	memcpy(&iSize, pUnalignedSize, sizeof(iSize));
	testRequire(
		(iSize != 0) && xrtHttpDigestValid(
			(xstrview){ arrOutput, iSize }
		),
		"HTTP SHA-2 unaligned write result mismatch"
	);

	memset(arrOutput, 0xA5, sizeof(arrOutput));
	memcpy(arrBefore, arrOutput, sizeof(arrOutput));
	iSize = 77u;
	xrtClearError();
	testRequire(
		!xrtHttpDigestSha256Write(
			arrData + 1u, 3u, arrOutput, 4u, &iSize
		) && (iSize > 4u) &&
		(memcmp(arrOutput, arrBefore, sizeof(arrOutput)) == 0),
		"HTTP SHA-2 short-buffer failure was not atomic"
	);
	xrtClearError();

	iSize = 77u;
	testRequire(
		!xrtHttpDigestSha256Write(
			(const void*)(uintptr_t)(UINTPTR_MAX - 1u), 4u,
			arrOutput, sizeof(arrOutput), &iSize
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(iSize == 77u),
		"HTTP SHA-2 writer accepted wrapped input"
	);
	xrtClearError();
	testRequire(
		!xrtHttpDigestSha256Write(
			arrData, sizeof(arrData),
			(void*)(uintptr_t)(UINTPTR_MAX - 1u), 4u, &iSize
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP SHA-2 writer accepted wrapped output"
	);
	xrtClearError();
	testRequire(
		!xrtHttpDigestSha512Write(
			arrData, sizeof(arrData), arrOutput, sizeof(arrOutput),
			(size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP SHA-2 writer accepted wrapped length output"
	);
	xrtClearError();

	memset(arrOutput, 0xA5, sizeof(arrOutput));
	memcpy(arrBefore, arrOutput, sizeof(arrOutput));
	testRequire(
		!xrtHttpDigestSha256Write(
			arrData, sizeof(arrData),
			arrOutput, sizeof(arrOutput),
			(size_t*)(arrOutput + 1u)
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(memcmp(arrOutput, arrBefore, sizeof(arrOutput)) == 0),
		"HTTP SHA-2 writer accepted output/size overlap"
	);
	xrtClearError();
	testRequire(
		xrtHttpDigestSha256Write(
			arrInPlace, 3u,
			arrInPlace, sizeof(arrInPlace), &iSize
		) && xrtHttpDigestValid(
			(xstrview){ arrInPlace, iSize }
		),
		"HTTP SHA-2 writer rejected post-hash input reuse"
	);

	testRequire(
		testHttpDigestSha2Parse(
			XRT_STR_LITERAL(
				"sha-256=:ungWv48Bz+pBQUDeXa4iI7ADYaOWF3qctBD/YfIAFa0=:"
			), &Digest
		),
		"HTTP SHA-2 unaligned verifier setup failed"
	);
	memcpy(DigestStorage.Bytes + 1u, &Digest, sizeof(Digest));
	testRequire(
		xrtHttpDigestSha2Verify(
			pUnalignedDigest, arrData + 1u, 3u
		) == XHTTP_DIGEST_MATCH_OK,
		"HTTP SHA-2 verifier rejected unaligned descriptor"
	);

	Invalid = Digest;
	Invalid.Algorithm = XRT_STR_LITERAL("Bad");
	xrtClearError();
	testRequire(
		(xrtHttpDigestSha2Verify(
			&Invalid, arrData + 1u, 3u
		) == XHTTP_DIGEST_MATCH_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"HTTP SHA-2 verifier accepted malformed algorithm key"
	);
	Invalid = Digest;
	Invalid.Value.Number = 1;
	xrtClearError();
	testRequire(
		(xrtHttpDigestSha2Verify(
			&Invalid, arrData + 1u, 3u
		) == XHTTP_DIGEST_MATCH_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"HTTP SHA-2 verifier accepted incoherent digest value"
	);

	Invalid = Digest;
	Invalid.Algorithm = XRT_STR_LITERAL("future");
	Invalid.Value.Encoded = (xstrview){
		(cstr)(uintptr_t)(UINTPTR_MAX - 1u), 4u
	};
	xrtClearError();
	testRequire(
		(xrtHttpDigestSha2Verify(
			&Invalid, arrData + 1u, 3u
		) == XHTTP_DIGEST_MATCH_UNSUPPORTED) &&
		(xrtGetError() == NULL),
		"HTTP SHA-2 verifier decoded an unsupported algorithm"
	);

	xrtClearError();
	testRequire(
		(xrtHttpDigestSha2Verify(
			(const xhttpdigest*)(uintptr_t)(UINTPTR_MAX - 1u),
			arrData, sizeof(arrData)
		) == XHTTP_DIGEST_MATCH_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP SHA-2 verifier accepted wrapped descriptor"
	);
	xrtClearError();
	testRequire(
		(xrtHttpDigestSha2Verify(
			&Digest,
			(const void*)(uintptr_t)(UINTPTR_MAX - 1u), 4u
		) == XHTTP_DIGEST_MATCH_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP SHA-2 verifier accepted wrapped content"
	);
	xrtClearError();
}



/* 运行 RFC 9530 SHA-2 生成与验证测试。 */
int main(void)
{
	testHttpDigestSha2Vectors();
	testHttpDigestSha2Verify();
	testHttpDigestSha2Memory();
	printf("[PASS] http_digest_sha2\n");
	return 0;
}
