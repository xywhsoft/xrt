#include "../test.h"

#include <xrt/http_auth.h>



/* 比较固定长度 Digest 计算结果。 */
static bool testHttpDigestComputeEqual(
	const char* sDigest,
	size_t iSize,
	cstr sExpected
)
{
	return (iSize == strlen(sExpected)) &&
		(memcmp(sDigest, sExpected, iSize) == 0);
}



/* 验证摘要分派、持久化 secret 和 userhash。 */
static void testHttpDigestComputePrimitives(void)
{
	char Digest[XHTTP_DIGEST_MAX_TEXT_SIZE];
	size_t iSize;

	testRequire(xrtHttpDigestHash(
		XHTTP_DIGEST_ALGORITHM_SHA256,
		"abc", 3u, Digest, sizeof(Digest), &iSize
	) && testHttpDigestComputeEqual(
		Digest, iSize,
		"ba7816bf8f01cfea414140de5dae2223"
		"b00361a396177a9cb410ff61f20015ad"
	), "HTTP Digest SHA-256 dispatch mismatch");
	testRequire(xrtHttpDigestHash(
		XHTTP_DIGEST_ALGORITHM_SHA512_256,
		"abc", 3u, Digest, sizeof(Digest), &iSize
	) && testHttpDigestComputeEqual(
		Digest, iSize,
		"53048e2681941ef99b2e29b76b4c7dab"
		"e4c2d0c634fc6d46e0e2f13107e7af23"
	), "HTTP Digest SHA-512/256 dispatch mismatch");
	testRequire(xrtHttpDigestSecret(
		XHTTP_DIGEST_ALGORITHM_SHA256_SESSION,
		XRT_STR_LITERAL("Mufasa"),
		XRT_STR_LITERAL("http-auth@example.org"),
		XRT_STR_LITERAL("Circle of Life"),
		Digest, sizeof(Digest), &iSize
	) && testHttpDigestComputeEqual(
		Digest, iSize,
		"7987c64c30e25f1b74be53f966b49b90"
		"f2808aa92faf9a00262392d7b4794232"
	), "HTTP Digest persistent secret mismatch");
	testRequire(xrtHttpDigestUserHash(
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XRT_STR_LITERAL("Mufasa"),
		XRT_STR_LITERAL("http-auth@example.org"),
		Digest, sizeof(Digest), &iSize
	) && testHttpDigestComputeEqual(
		Digest, iSize,
		"a947aad205e80e429958a387394944c6"
		"b496301e79f89d35a4cc23b6ee12b5b6"
	), "HTTP Digest userhash mismatch");
}



/* 验证 RFC 7616 SHA-256 官方请求向量及对应 rspauth。 */
static void testHttpDigestComputeRfcVector(void)
{
	xhttpdigestproof Proof = {
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XHTTP_DIGEST_QOP_AUTH,
		1u,
		XRT_STR_INIT(
			"7987C64C30E25F1B74BE53F966B49B90"
			"F2808AA92FAF9A00262392D7B4794232"
		),
		XRT_STR_INIT(
			"7ypf/xlj9XXwfDPEoM4URrv/xwf94BcCAzFZH4GiTo0v"
		),
		XRT_STR_INIT(
			"f2/wE4q74E6zIJEtWaHKaf5wv/H5QzzpXusqGemxURZJ"
		),
		XRT_STR_INIT("/dir/index.html"),
		{ NULL, 0 }
	};
	char Digest[XHTTP_DIGEST_MAX_TEXT_SIZE];
	size_t iSize;

	testRequire(xrtHttpDigestRequest(
		&Proof, XRT_STR_LITERAL("GET"),
		NULL, 0, &iSize
	) && (iSize == 64u),
		"HTTP Digest request query mismatch");
	testRequire(xrtHttpDigestRequest(
		&Proof, XRT_STR_LITERAL("GET"),
		Digest, sizeof(Digest), &iSize
	) && testHttpDigestComputeEqual(
		Digest, iSize,
		"753927fa0e85d155564e2e272a28d180"
		"2ca10daf4496794697cf8db5856cb6c1"
	), "HTTP Digest RFC request vector mismatch");
	testRequire(xrtHttpDigestRspAuth(
		&Proof, Digest, sizeof(Digest), &iSize
	) && testHttpDigestComputeEqual(
		Digest, iSize,
		"86d3b25618d41854ca5039a5d7e53ff6"
		"355d5134a9b1fb088a78ac3c462195a0"
	), "HTTP Digest rspauth vector mismatch");
}



/* 验证 session A1、auth-int 实体散列和双向证明。 */
static void testHttpDigestComputeVariants(void)
{
	char Entity[XHTTP_DIGEST_MAX_TEXT_SIZE];
	char Digest[XHTTP_DIGEST_MAX_TEXT_SIZE];
	size_t iSize;
	xhttpdigestproof Proof = {
		XHTTP_DIGEST_ALGORITHM_SHA256_SESSION,
		XHTTP_DIGEST_QOP_AUTH,
		1u,
		XRT_STR_INIT(
			"7987c64c30e25f1b74be53f966b49b90"
			"f2808aa92faf9a00262392d7b4794232"
		),
		XRT_STR_INIT(
			"7ypf/xlj9XXwfDPEoM4URrv/xwf94BcCAzFZH4GiTo0v"
		),
		XRT_STR_INIT(
			"f2/wE4q74E6zIJEtWaHKaf5wv/H5QzzpXusqGemxURZJ"
		),
		XRT_STR_INIT("/dir/index.html"),
		{ NULL, 0 }
	};

	testRequire(xrtHttpDigestRequest(
		&Proof, XRT_STR_LITERAL("GET"),
		Digest, sizeof(Digest), &iSize
	) && testHttpDigestComputeEqual(
		Digest, iSize,
		"2fd51b3a77ad75bad6afad6003e818d7"
		"67133c46d9e2749e7f5232ae1ea3efd7"
	), "HTTP Digest SHA-256-sess request mismatch");
	testRequire(xrtHttpDigestHash(
		XHTTP_DIGEST_ALGORITHM_SHA256,
		"Hello world", 11u,
		Entity, sizeof(Entity), &iSize
	), "HTTP Digest auth-int entity hash failed");
	Proof.Algorithm = XHTTP_DIGEST_ALGORITHM_SHA256;
	Proof.Qop = XHTTP_DIGEST_QOP_AUTH_INT;
	Proof.EntityHash = (xstrview){ Entity, iSize };
	testRequire(xrtHttpDigestRequest(
		&Proof, XRT_STR_LITERAL("GET"),
		Digest, sizeof(Digest), &iSize
	) && testHttpDigestComputeEqual(
		Digest, iSize,
		"be396cb5d80792ab9faef9f8fb4207fe"
		"7d2a4f3c114e24f0f71b18c06e436345"
	), "HTTP Digest auth-int request mismatch");
	testRequire(xrtHttpDigestRspAuth(
		&Proof, Digest, sizeof(Digest), &iSize
	) && testHttpDigestComputeEqual(
		Digest, iSize,
		"a4bfa83b44ed5574aedbb4d324844c9b"
		"ee5c6f539de8e75b604b41095023cf0a"
	), "HTTP Digest auth-int rspauth mismatch");
}



/* 验证语义拒绝、裁剪后端和纯比较谓词。 */
static void testHttpDigestComputeReject(void)
{
	xhttpdigestproof Proof = {
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XHTTP_DIGEST_QOP_AUTH,
		1u,
		XRT_STR_INIT(
			"7987c64c30e25f1b74be53f966b49b90"
			"f2808aa92faf9a00262392d7b4794232"
		),
		XRT_STR_INIT("nonce"),
		XRT_STR_INIT("client"),
		XRT_STR_INIT("/"),
		{ NULL, 0 }
	};
	char Digest[XHTTP_DIGEST_MAX_TEXT_SIZE];
	size_t iSize;

	testRequire(!xrtHttpDigestHash(
		XHTTP_DIGEST_ALGORITHM_UNKNOWN,
		"", 0, Digest, sizeof(Digest), &iSize
	) && (xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"HTTP Digest accepted unknown algorithm");
	xrtClearError();
	#if !defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_MD5)
	testRequire(!xrtHttpDigestHash(
		XHTTP_DIGEST_ALGORITHM_MD5,
		"", 0, Digest, sizeof(Digest), &iSize
	) && (xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED),
		"HTTP Digest SHA-2 closure accepted trimmed MD5");
	xrtClearError();
	#endif

	Proof.NonceCount = 0;
	testRequire(!xrtHttpDigestRequest(
		&Proof, XRT_STR_LITERAL("GET"),
		Digest, sizeof(Digest), &iSize
	), "HTTP Digest accepted zero nonce count");
	xrtClearError();
	Proof.NonceCount = 1u;
	Proof.Nonce = (xstrview){ NULL, 0 };
	testRequire(!xrtHttpDigestRequest(
		&Proof, XRT_STR_LITERAL("GET"),
		Digest, sizeof(Digest), &iSize
	), "HTTP Digest accepted an empty nonce");
	xrtClearError();
	Proof.Nonce = XRT_STR_LITERAL("nonce");
	Proof.Cnonce = (xstrview){ NULL, 0 };
	testRequire(!xrtHttpDigestRequest(
		&Proof, XRT_STR_LITERAL("GET"),
		Digest, sizeof(Digest), &iSize
	), "HTTP Digest accepted an empty client nonce");
	xrtClearError();
	Proof.Cnonce = XRT_STR_LITERAL("client");
	Proof.EntityHash = XRT_STR_LITERAL("00");
	testRequire(!xrtHttpDigestRequest(
		&Proof, XRT_STR_LITERAL("GET"),
		Digest, sizeof(Digest), &iSize
	), "HTTP Digest auth accepted an entity hash");
	xrtClearError();
	Proof.EntityHash = (xstrview){ NULL, 0 };
	Proof.Qop = XHTTP_DIGEST_QOP_AUTH_INT;
	testRequire(!xrtHttpDigestRequest(
		&Proof, XRT_STR_LITERAL("GET"),
		Digest, sizeof(Digest), &iSize
	), "HTTP Digest auth-int accepted a missing entity hash");
	xrtClearError();
	Proof.Qop = XHTTP_DIGEST_QOP_AUTH;
	testRequire(!xrtHttpDigestRequest(
		&Proof, (xstrview){ NULL, 0 },
		Digest, sizeof(Digest), &iSize
	), "HTTP Digest accepted an empty method");
	xrtClearError();

	testRequire(xrtHttpDigestEqual(
		XRT_STR_LITERAL("0123abcd"),
		XRT_STR_LITERAL("0123ABCD")
	) && !xrtHttpDigestEqual(
		XRT_STR_LITERAL("0123abcd"),
		XRT_STR_LITERAL("0123abce")
	) && !xrtHttpDigestEqual(
		XRT_STR_LITERAL("0123abcx"),
		XRT_STR_LITERAL("0123ABCX")
	) && (xrtGetError() == NULL),
		"HTTP Digest constant-time comparison contract mismatch");
}



/* 验证未对齐结构、短缓冲、别名与地址回绕。 */
static void testHttpDigestComputeMemoryContracts(void)
{
	uint8 ProofStorage[sizeof(xhttpdigestproof) + 2u];
	uint8 SizeStorage[sizeof(size_t) + 2u];
	xhttpdigestproof* pProof =
		(xhttpdigestproof*)(void*)(ProofStorage + 1u);
	size_t* pSize = (size_t*)(void*)(SizeStorage + 1u);
	xhttpdigestproof Proof = {
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XHTTP_DIGEST_QOP_AUTH,
		1u,
		XRT_STR_INIT(
			"7987c64c30e25f1b74be53f966b49b90"
			"f2808aa92faf9a00262392d7b4794232"
		),
		XRT_STR_INIT("nonce"),
		XRT_STR_INIT("client"),
		XRT_STR_INIT("/"),
		{ NULL, 0 }
	};
	char Digest[XHTTP_DIGEST_MAX_TEXT_SIZE];
	char Before[XHTTP_DIGEST_MAX_TEXT_SIZE];
	size_t iSize;
	size_t iRequired;

	memset(ProofStorage, 0xA5, sizeof(ProofStorage));
	memset(SizeStorage, 0xA5, sizeof(SizeStorage));
	memcpy(pProof, &Proof, sizeof(Proof));
	testRequire(xrtHttpDigestRequest(
		pProof, XRT_STR_LITERAL("GET"),
		Digest, sizeof(Digest), pSize
	) && (ProofStorage[0] == 0xA5) &&
		(ProofStorage[sizeof(ProofStorage) - 1u] == 0xA5) &&
		(SizeStorage[0] == 0xA5) &&
		(SizeStorage[sizeof(SizeStorage) - 1u] == 0xA5),
		"HTTP Digest rejected unaligned proof descriptors");
	memcpy(&iSize, pSize, sizeof(iSize));
	testRequire(iSize == 64u,
		"HTTP Digest unaligned output size mismatch");

	testRequire(xrtHttpDigestRequest(
		&Proof, XRT_STR_LITERAL("GET"),
		NULL, 0, &iRequired
	), "HTTP Digest short-output query failed");
	memset(Digest, 0x5A, sizeof(Digest));
	memcpy(Before, Digest, sizeof(Digest));
	testRequire(!xrtHttpDigestRequest(
		&Proof, XRT_STR_LITERAL("GET"),
		Digest, iRequired - 1u, &iSize
	) && (iSize == iRequired) &&
		(memcmp(Digest, Before, sizeof(Digest)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"HTTP Digest short output was not atomic");
	xrtClearError();

	testRequire(!xrtHttpDigestHash(
		XHTTP_DIGEST_ALGORITHM_SHA256,
		&iSize, sizeof(iSize),
		Digest, sizeof(Digest), &iSize
	), "HTTP Digest hash accepted a size alias");
	xrtClearError();
	testRequire(!xrtHttpDigestRequest(
		(xhttpdigestproof*)(uintptr_t)(UINTPTR_MAX - 1u),
		XRT_STR_LITERAL("GET"),
		Digest, sizeof(Digest), &iSize
	), "HTTP Digest accepted a wrapping proof");
	xrtClearError();
	Proof.Secret = (xstrview){
		(cstr)(uintptr_t)(UINTPTR_MAX - 1u), 64u
	};
	testRequire(!xrtHttpDigestRequest(
		&Proof, XRT_STR_LITERAL("GET"),
		Digest, sizeof(Digest), &iSize
	), "HTTP Digest accepted a wrapping secret");
	xrtClearError();
}



int main(void)
{
	testHttpDigestComputePrimitives();
	testHttpDigestComputeRfcVector();
	testHttpDigestComputeVariants();
	testHttpDigestComputeReject();
	testHttpDigestComputeMemoryContracts();
	puts("[PASS] HTTP Digest SHA-2 calculation");
	return 0;
}
