#include "../test.h"

#include <xrt/http_auth.h>



/* 一组完整固定数据同时覆盖 challenge、证明和无状态 nonce。 */
typedef struct test_http_digest_verify_fixture {
	uint8 Key[XHTTP_DIGEST_NONCE_KEY_MIN];
	uint8 Salt[XHTTP_DIGEST_NONCE_SALT_SIZE];
	char Nonce[XHTTP_DIGEST_NONCE_TEXT_SIZE];
	char Secret[XHTTP_DIGEST_MAX_TEXT_SIZE];
	char Response[XHTTP_DIGEST_MAX_TEXT_SIZE];
	char EntityHash[XHTTP_DIGEST_MAX_TEXT_SIZE];
	char UserHash[XHTTP_DIGEST_MAX_TEXT_SIZE];
	size_t SecretSize;
	size_t ResponseSize;
	size_t EntityHashSize;
	size_t UserHashSize;
	xhttpdigestchallenge Challenge;
	xhttpdigestauth Auth;
	xhttpdigestverification Verification;
} test_http_digest_verify_fixture;



/* 为每个测试重新构建证明，避免测试之间共享可变状态。 */
static void testHttpDigestVerifyFixtureInit(
	test_http_digest_verify_fixture* pFixture
)
{
	xhttpdigestproof Proof;
	size_t iNonceSize;

	memset(pFixture, 0, sizeof(*pFixture));
	memset(pFixture->Key, 0x11, sizeof(pFixture->Key));
	memset(pFixture->Salt, 0x22, sizeof(pFixture->Salt));
	testRequire(xrtHttpDigestNonceWrite(
		(xbytesview){ pFixture->Key, sizeof(pFixture->Key) },
		XRT_BYTES_LITERAL("api@example.test"),
		INT64_C(1700000000),
		pFixture->Salt,
		pFixture->Nonce,
		sizeof(pFixture->Nonce),
		&iNonceSize
	), "HTTP Digest verify nonce fixture failed");
	testRequire(iNonceSize == sizeof(pFixture->Nonce),
		"HTTP Digest verify nonce size mismatch");
	testRequire(xrtHttpDigestSecret(
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XRT_STR_LITERAL("Mufasa"),
		XRT_STR_LITERAL("api@example.test"),
		XRT_STR_LITERAL("Circle Of Life"),
		pFixture->Secret,
		sizeof(pFixture->Secret),
		&pFixture->SecretSize
	), "HTTP Digest verify secret fixture failed");
	pFixture->Challenge = (xhttpdigestchallenge){
		XHTTP_DIGEST_CHALLENGE_HAS_OPAQUE |
		XHTTP_DIGEST_CHALLENGE_UTF8 |
		XHTTP_DIGEST_CHALLENGE_HAS_USERHASH |
		XHTTP_DIGEST_CHALLENGE_USERHASH |
		XHTTP_DIGEST_CHALLENGE_QOP_AUTH |
		XHTTP_DIGEST_CHALLENGE_QOP_AUTH_INT |
		XHTTP_DIGEST_CHALLENGE_ALGORITHM_EXPLICIT,
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XRT_STR_INIT("api@example.test"),
		{ NULL, 0 },
		{ pFixture->Nonce, sizeof(pFixture->Nonce) },
		XRT_STR_INIT("server-state"),
		{ NULL, 0 }
	};
	Proof = (xhttpdigestproof){
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XHTTP_DIGEST_QOP_AUTH,
		1u,
		{ pFixture->Secret, pFixture->SecretSize },
		{ pFixture->Nonce, sizeof(pFixture->Nonce) },
		XRT_STR_INIT("client-nonce"),
		XRT_STR_INIT("/private"),
		{ NULL, 0 }
	};
	testRequire(xrtHttpDigestRequest(
		&Proof,
		XRT_STR_LITERAL("GET"),
		pFixture->Response,
		sizeof(pFixture->Response),
		&pFixture->ResponseSize
	), "HTTP Digest verify response fixture failed");
	pFixture->Auth = (xhttpdigestauth){
		XHTTP_DIGEST_AUTH_HAS_OPAQUE |
		XHTTP_DIGEST_AUTH_ALGORITHM_EXPLICIT,
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XHTTP_DIGEST_QOP_AUTH,
		1u,
		XRT_STR_INIT("Mufasa"),
		{ NULL, 0 },
		XRT_STR_INIT("api@example.test"),
		{ pFixture->Nonce, sizeof(pFixture->Nonce) },
		XRT_STR_INIT("/private"),
		XRT_STR_INIT("client-nonce"),
		{ pFixture->Response, pFixture->ResponseSize },
		XRT_STR_INIT("server-state"),
		{ NULL, 0 }
	};
	pFixture->Verification = (xhttpdigestverification){
		0,
		&pFixture->Auth,
		&pFixture->Challenge,
		{ pFixture->Secret, pFixture->SecretSize },
		XRT_STR_INIT("GET"),
		XRT_STR_INIT("/private"),
		{ NULL, 0 }
	};
}



/* 验证纯证明、组合 nonce 和签发时间发布契约。 */
static void testHttpDigestVerifyValid(void)
{
	test_http_digest_verify_fixture Fixture;
	int64 iIssued = -1;

	testHttpDigestVerifyFixtureInit(&Fixture);
	testRequire(xrtHttpDigestProofVerify(
		&Fixture.Verification
	) == XHTTP_DIGEST_VERIFY_VALID,
		"HTTP Digest proof verifier rejected valid proof");
	testRequire(xrtHttpDigestVerify(
		&Fixture.Verification,
		(xbytesview){ Fixture.Key, sizeof(Fixture.Key) },
		XRT_BYTES_LITERAL("api@example.test"),
		INT64_C(1700000030),
		60,
		5,
		&iIssued
	) == XHTTP_DIGEST_VERIFY_VALID &&
		(iIssued == INT64_C(1700000000)),
		"HTTP Digest combined verifier rejected valid proof");
}



/* 所有 challenge 与请求绑定字段必须逐字节一致。 */
static void testHttpDigestVerifyBindings(void)
{
	test_http_digest_verify_fixture Fixture;
	xhttpdigestauth Auth;
	xhttpdigestchallenge Challenge;
	xhttpdigestverification Verification;

	testHttpDigestVerifyFixtureInit(&Fixture);
	Auth = Fixture.Auth;
	Challenge = Fixture.Challenge;
	Verification = Fixture.Verification;
	Verification.Auth = &Auth;
	Verification.Challenge = &Challenge;

	Auth.Uri = XRT_STR_LITERAL("/other");
	testRequire(xrtHttpDigestProofVerify(
		&Verification
	) == XHTTP_DIGEST_VERIFY_INVALID,
		"HTTP Digest verifier accepted mismatched credential URI");
	Auth = Fixture.Auth;
	Verification.RequestTarget = XRT_STR_LITERAL("/other");
	testRequire(xrtHttpDigestProofVerify(
		&Verification
	) == XHTTP_DIGEST_VERIFY_INVALID,
		"HTTP Digest verifier accepted mismatched request-target");
	Verification.RequestTarget = Fixture.Verification.RequestTarget;
	Challenge.Realm = XRT_STR_LITERAL("other-realm");
	testRequire(xrtHttpDigestProofVerify(
		&Verification
	) == XHTTP_DIGEST_VERIFY_INVALID,
		"HTTP Digest verifier accepted mismatched realm");
	Challenge = Fixture.Challenge;
	Challenge.Opaque = XRT_STR_LITERAL("other-state");
	testRequire(xrtHttpDigestProofVerify(
		&Verification
	) == XHTTP_DIGEST_VERIFY_INVALID,
		"HTTP Digest verifier accepted mismatched opaque");
	Challenge = Fixture.Challenge;
	Challenge.Flags &= ~XHTTP_DIGEST_CHALLENGE_QOP_AUTH;
	testRequire(xrtHttpDigestProofVerify(
		&Verification
	) == XHTTP_DIGEST_VERIFY_INVALID,
		"HTTP Digest verifier accepted qop not offered by server");
}



/* userhash 与 username* 必须受 challenge 能力和服务端策略约束。 */
static void testHttpDigestVerifyUsername(void)
{
	test_http_digest_verify_fixture Fixture;
	xhttpdigestauth Auth;
	xhttpdigestchallenge Challenge;
	xhttpdigestverification Verification;

	testHttpDigestVerifyFixtureInit(&Fixture);
	testRequire(xrtHttpDigestUserHash(
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XRT_STR_LITERAL("Mufasa"),
		XRT_STR_LITERAL("api@example.test"),
		Fixture.UserHash,
		sizeof(Fixture.UserHash),
		&Fixture.UserHashSize
	), "HTTP Digest verify userhash fixture failed");
	Auth = Fixture.Auth;
	Challenge = Fixture.Challenge;
	Verification = Fixture.Verification;
	Verification.Auth = &Auth;
	Verification.Challenge = &Challenge;
	Auth.Flags |= XHTTP_DIGEST_AUTH_HAS_USERHASH |
		XHTTP_DIGEST_AUTH_USERHASH;
	Auth.Username = (xstrview){
		Fixture.UserHash, Fixture.UserHashSize
	};
	testRequire(xrtHttpDigestProofVerify(
		&Verification
	) == XHTTP_DIGEST_VERIFY_VALID,
		"HTTP Digest verifier rejected advertised userhash");
	Challenge.Flags &= ~(XHTTP_DIGEST_CHALLENGE_HAS_USERHASH |
		XHTTP_DIGEST_CHALLENGE_USERHASH);
	testRequire(xrtHttpDigestProofVerify(
		&Verification
	) == XHTTP_DIGEST_VERIFY_INVALID,
		"HTTP Digest verifier accepted unadvertised userhash");

	Auth = Fixture.Auth;
	Challenge = Fixture.Challenge;
	Verification.Flags = XHTTP_DIGEST_VERIFY_REQUIRE_USERHASH;
	testRequire(xrtHttpDigestProofVerify(
		&Verification
	) == XHTTP_DIGEST_VERIFY_INVALID,
		"HTTP Digest verifier ignored required userhash policy");
	Verification.Flags = 0;
	Auth.Flags |= XHTTP_DIGEST_AUTH_USERNAME_EXTENDED;
	Challenge.Flags &= ~XHTTP_DIGEST_CHALLENGE_UTF8;
	testRequire(xrtHttpDigestProofVerify(
		&Verification
	) == XHTTP_DIGEST_VERIFY_INVALID,
		"HTTP Digest verifier accepted username* without UTF-8 offer");
}



/* auth-int 必须使用已经计算的实体摘要，且不缓存请求正文。 */
static void testHttpDigestVerifyAuthInt(void)
{
	test_http_digest_verify_fixture Fixture;
	xhttpdigestproof Proof;

	testHttpDigestVerifyFixtureInit(&Fixture);
	testRequire(xrtHttpDigestHash(
		XHTTP_DIGEST_ALGORITHM_SHA256,
		"payload",
		7u,
		Fixture.EntityHash,
		sizeof(Fixture.EntityHash),
		&Fixture.EntityHashSize
	), "HTTP Digest verify entity hash failed");
	Fixture.Auth.Qop = XHTTP_DIGEST_QOP_AUTH_INT;
	Fixture.Verification.EntityHash = (xstrview){
		Fixture.EntityHash, Fixture.EntityHashSize
	};
	Proof = (xhttpdigestproof){
		Fixture.Auth.Algorithm,
		Fixture.Auth.Qop,
		Fixture.Auth.NonceCount,
		Fixture.Verification.Secret,
		Fixture.Auth.Nonce,
		Fixture.Auth.Cnonce,
		Fixture.Auth.Uri,
		Fixture.Verification.EntityHash
	};
	testRequire(xrtHttpDigestRequest(
		&Proof,
		Fixture.Verification.Method,
		Fixture.Response,
		sizeof(Fixture.Response),
		&Fixture.ResponseSize
	), "HTTP Digest verify auth-int response failed");
	Fixture.Auth.Response = (xstrview){
		Fixture.Response, Fixture.ResponseSize
	};
	testRequire(xrtHttpDigestProofVerify(
		&Fixture.Verification
	) == XHTTP_DIGEST_VERIFY_VALID,
		"HTTP Digest verifier rejected valid auth-int proof");
	Fixture.Verification.EntityHash = (xstrview){ NULL, 0 };
	testRequire(xrtHttpDigestProofVerify(
		&Fixture.Verification
	) == XHTTP_DIGEST_VERIFY_ERROR,
		"HTTP Digest verifier accepted missing auth-int entity hash");
	xrtClearError();
}



/* stale 只能在 nonce 签名和客户端证明同时正确时发布。 */
static void testHttpDigestVerifyStale(void)
{
	test_http_digest_verify_fixture Fixture;
	int64 iIssued = -1;

	testHttpDigestVerifyFixtureInit(&Fixture);
	testRequire(xrtHttpDigestVerify(
		&Fixture.Verification,
		(xbytesview){ Fixture.Key, sizeof(Fixture.Key) },
		XRT_BYTES_LITERAL("api@example.test"),
		INT64_C(1700000061),
		60,
		5,
		&iIssued
	) == XHTTP_DIGEST_VERIFY_STALE &&
		(iIssued == INT64_C(1700000000)),
		"HTTP Digest verifier did not report valid stale proof");
	Fixture.Response[0] = Fixture.Response[0] == '0' ? '1' : '0';
	iIssued = -1;
	testRequire(xrtHttpDigestVerify(
		&Fixture.Verification,
		(xbytesview){ Fixture.Key, sizeof(Fixture.Key) },
		XRT_BYTES_LITERAL("api@example.test"),
		INT64_C(1700000061),
		60,
		5,
		&iIssued
	) == XHTTP_DIGEST_VERIFY_INVALID && (iIssued == -1),
		"HTTP Digest verifier disclosed stale for invalid proof");

	testHttpDigestVerifyFixtureInit(&Fixture);
	Fixture.Nonce[0] = Fixture.Nonce[0] == 'A' ? 'B' : 'A';
	iIssued = -1;
	testRequire(xrtHttpDigestVerify(
		&Fixture.Verification,
		(xbytesview){ Fixture.Key, sizeof(Fixture.Key) },
		XRT_BYTES_LITERAL("api@example.test"),
		INT64_C(1700000030),
		60,
		5,
		&iIssued
	) == XHTTP_DIGEST_VERIFY_INVALID && (iIssued == -1),
		"HTTP Digest verifier accepted invalid nonce signature");
}



/* 验证未对齐描述符、输出别名和无效策略的失败原子性。 */
static void testHttpDigestVerifyInvalid(void)
{
	test_http_digest_verify_fixture Fixture;
	uint8 VerificationStorage[sizeof(xhttpdigestverification) + 2u];
	uint8 IssuedStorage[sizeof(int64) + 2u];
	xhttpdigestverification* pVerification =
		(xhttpdigestverification*)(void*)(VerificationStorage + 1u);
	int64* pIssued = (int64*)(void*)(IssuedStorage + 1u);
	xhttpdigestchallenge Challenge;

	testHttpDigestVerifyFixtureInit(&Fixture);
	memset(VerificationStorage, 0xA5, sizeof(VerificationStorage));
	memcpy(pVerification, &Fixture.Verification, sizeof(*pVerification));
	memset(IssuedStorage, 0x5A, sizeof(IssuedStorage));
	testRequire(xrtHttpDigestVerify(
		pVerification,
		(xbytesview){ Fixture.Key, sizeof(Fixture.Key) },
		XRT_BYTES_LITERAL("api@example.test"),
		INT64_C(1700000030),
		60,
		5,
		pIssued
	) == XHTTP_DIGEST_VERIFY_VALID &&
		(VerificationStorage[0] == 0xA5u) &&
		(VerificationStorage[sizeof(VerificationStorage) - 1u] == 0xA5u) &&
		(IssuedStorage[0] == 0x5Au) &&
		(IssuedStorage[sizeof(IssuedStorage) - 1u] == 0x5Au),
		"HTTP Digest verifier rejected unaligned descriptors");

	testRequire(xrtHttpDigestVerify(
		&Fixture.Verification,
		(xbytesview){ Fixture.Key, sizeof(Fixture.Key) },
		XRT_BYTES_LITERAL("api@example.test"),
		INT64_C(1700000030),
		60,
		5,
		(int64*)(void*)Fixture.Key
	) == XHTTP_DIGEST_VERIFY_ERROR &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Digest verifier accepted aliased issued time");
	xrtClearError();
	Fixture.Verification.Flags = UINT32_C(0x80000000);
	testRequire(xrtHttpDigestProofVerify(
		&Fixture.Verification
	) == XHTTP_DIGEST_VERIFY_ERROR &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"HTTP Digest verifier accepted unknown flags");
	xrtClearError();
	Fixture.Verification.Flags = XHTTP_DIGEST_VERIFY_REQUIRE_USERHASH;
	Challenge = Fixture.Challenge;
	Challenge.Flags &= ~(XHTTP_DIGEST_CHALLENGE_HAS_USERHASH |
		XHTTP_DIGEST_CHALLENGE_USERHASH);
	Fixture.Verification.Challenge = &Challenge;
	testRequire(xrtHttpDigestProofVerify(
		&Fixture.Verification
	) == XHTTP_DIGEST_VERIFY_ERROR &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"HTTP Digest verifier accepted impossible userhash policy");
	xrtClearError();
	testRequire(xrtHttpDigestProofVerify(NULL) ==
		XHTTP_DIGEST_VERIFY_ERROR &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Digest verifier accepted NULL descriptor");
}



int main(void)
{
	testHttpDigestVerifyValid();
	testHttpDigestVerifyBindings();
	testHttpDigestVerifyUsername();
	testHttpDigestVerifyAuthInt();
	testHttpDigestVerifyStale();
	testHttpDigestVerifyInvalid();
	puts("[PASS] HTTP Digest verification");
	return 0;
}
