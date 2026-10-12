#include "../test.h"



/* 比较借用文本与固定字面量。 */
static bool testText(xstrview Text, const char* sExpected)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		(memcmp(Text.Data, sExpected, iSize) == 0);
}



/* 构造测试使用的现代 SHA-256 challenge。 */
static xhttpdigestchallenge testChallenge(void)
{
	xhttpdigestchallenge Challenge = {
		XHTTP_DIGEST_CHALLENGE_HAS_OPAQUE |
		XHTTP_DIGEST_CHALLENGE_UTF8 |
		XHTTP_DIGEST_CHALLENGE_HAS_USERHASH |
		XHTTP_DIGEST_CHALLENGE_USERHASH |
		XHTTP_DIGEST_CHALLENGE_QOP_AUTH |
		XHTTP_DIGEST_CHALLENGE_QOP_AUTH_INT |
		XHTTP_DIGEST_CHALLENGE_ALGORITHM_EXPLICIT,
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XRT_STR_INIT("api"),
		{ NULL, 0 },
		XRT_STR_INIT("server-nonce"),
		XRT_STR_INIT("state"),
		{ NULL, 0 }
	};

	return Challenge;
}



/* 验证默认策略、局部策略和普通拒绝结果。 */
static void testChoose(void)
{
	xhttpdigestchallenge Challenge = testChallenge();
	xhttpdigestpolicy Policy;
	xhttpdigestchoice Choice;

	xrtHttpDigestPolicyInit(&Policy);
	testRequire(
		(Policy.Flags == 0) &&
		(Policy.Algorithms == XHTTP_DIGEST_ALGORITHMS_SHA2) &&
		(Policy.Qops == XHTTP_DIGEST_QOPS_ALL),
		"HTTP Digest default policy mismatch"
	);
	testRequire((xrtHttpDigestChallengeChoose(
		&Challenge, NULL, &Choice
	) == XHTTP_DIGEST_CHOOSE_ACCEPTED) &&
		(Choice.Algorithm == XHTTP_DIGEST_ALGORITHM_SHA256) &&
		(Choice.Qop == XHTTP_DIGEST_QOP_AUTH) && Choice.UserHash,
		"HTTP Digest default challenge choice mismatch");
	Policy.Flags = XHTTP_DIGEST_POLICY_PREFER_AUTH_INT;
	testRequire((xrtHttpDigestChallengeChoose(
		&Challenge, &Policy, &Choice
	) == XHTTP_DIGEST_CHOOSE_ACCEPTED) &&
		(Choice.Qop == XHTTP_DIGEST_QOP_AUTH_INT),
		"HTTP Digest auth-int preference mismatch");
	Policy.Qops = XHTTP_DIGEST_QOPS_AUTH;
	Challenge.Flags &= ~XHTTP_DIGEST_CHALLENGE_QOP_AUTH;
	testRequire(xrtHttpDigestChallengeChoose(
		&Challenge, &Policy, &Choice
	) == XHTTP_DIGEST_CHOOSE_REJECTED,
		"HTTP Digest unsupported qop was accepted");

	Challenge = testChallenge();
	Policy.Flags = XHTTP_DIGEST_POLICY_REQUIRE_USERHASH;
	Challenge.Flags &= ~(XHTTP_DIGEST_CHALLENGE_HAS_USERHASH |
		XHTTP_DIGEST_CHALLENGE_USERHASH);
	testRequire(xrtHttpDigestChallengeChoose(
		&Challenge, &Policy, &Choice
	) == XHTTP_DIGEST_CHOOSE_REJECTED,
		"HTTP Digest missing required userhash was accepted");
	Policy.Flags = XHTTP_DIGEST_POLICY_PLAIN_USERNAME |
		XHTTP_DIGEST_POLICY_REQUIRE_USERHASH;
	testRequire(xrtHttpDigestChallengeChoose(
		&Challenge, &Policy, &Choice
	) == XHTTP_DIGEST_CHOOSE_ERROR,
		"HTTP Digest contradictory policy was accepted");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"HTTP Digest contradictory policy error mismatch");

	Challenge = testChallenge();
	Challenge.Algorithm = XHTTP_DIGEST_ALGORITHM_MD5;
	Policy.Flags = 0;
	Policy.Algorithms = XHTTP_DIGEST_ALGORITHMS_SHA2;
	Policy.Qops = XHTTP_DIGEST_QOPS_ALL;
	testRequire(xrtHttpDigestChallengeChoose(
		&Challenge, &Policy, &Choice
	) == XHTTP_DIGEST_CHOOSE_REJECTED,
		"HTTP Digest default policy accepted MD5");
}



/* 构造客户端凭据并返回请求证明。 */
static xhttpdigestproof testClientAuth(
	xhttpdigestchallenge* pChallenge,
	xhttpdigestchoice* pChoice,
	char Secret[XHTTP_DIGEST_MAX_TEXT_SIZE],
	char Output[XHTTP_DIGEST_MAX_TEXT_SIZE * 2u],
	xhttpdigestauth* pAuth
)
{
	xhttpdigestclientauth Input;
	xhttpdigestproof Proof;
	char UserHash[XHTTP_DIGEST_MAX_TEXT_SIZE];
	char Expected[XHTTP_DIGEST_MAX_TEXT_SIZE];
	size_t iSecretSize;
	size_t iUserHashSize;
	size_t iExpectedSize;
	size_t iSize;

	testRequire(xrtHttpDigestSecret(
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XRT_STR_LITERAL("user"),
		XRT_STR_LITERAL("api"),
		XRT_STR_LITERAL("password"),
		Secret,
		XHTTP_DIGEST_MAX_TEXT_SIZE,
		&iSecretSize
	), "HTTP Digest client secret failed");
	testRequire(xrtHttpDigestChallengeChoose(
		pChallenge, NULL, pChoice
	) == XHTTP_DIGEST_CHOOSE_ACCEPTED,
		"HTTP Digest client choice failed");
	Input = (xhttpdigestclientauth){
		0,
		pChallenge,
		pChoice,
		XRT_STR_INIT("user"),
		{ NULL, 0 },
		{ Secret, iSecretSize },
		XRT_STR_INIT("GET"),
		XRT_STR_INIT("/private"),
		XRT_STR_INIT("client-nonce"),
		{ NULL, 0 },
		1u
	};
	testRequire(xrtHttpDigestClientAuth(
		&Input, NULL, 0, &iSize, pAuth
	) && (iSize == (XHTTP_DIGEST_MAX_TEXT_SIZE * 2u)) &&
		(pAuth->Username.Size == 0) &&
		(pAuth->Response.Size == 0),
		"HTTP Digest client auth query mismatch");
	testRequire(xrtHttpDigestClientAuth(
		&Input,
		Output,
		XHTTP_DIGEST_MAX_TEXT_SIZE * 2u,
		&iSize,
		pAuth
	) && (iSize == (XHTTP_DIGEST_MAX_TEXT_SIZE * 2u)) &&
		((pAuth->Flags & XHTTP_DIGEST_AUTH_USERHASH) != 0) &&
		testText(pAuth->Realm, "api") &&
		testText(pAuth->Nonce, "server-nonce") &&
		testText(pAuth->Opaque, "state") &&
		testText(pAuth->Uri, "/private"),
		"HTTP Digest client auth build mismatch");
	testRequire(xrtHttpDigestUserHash(
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XRT_STR_LITERAL("user"),
		XRT_STR_LITERAL("api"),
		UserHash,
		sizeof(UserHash),
		&iUserHashSize
	) && xrtHttpDigestEqual(
		pAuth->Username, (xstrview){ UserHash, iUserHashSize }
	), "HTTP Digest client userhash mismatch");
	Proof = (xhttpdigestproof){
		pChoice->Algorithm,
		pChoice->Qop,
		1u,
		{ Secret, iSecretSize },
		pChallenge->Nonce,
		XRT_STR_INIT("client-nonce"),
		XRT_STR_INIT("/private"),
		{ NULL, 0 }
	};
	testRequire(xrtHttpDigestRequest(
		&Proof,
		XRT_STR_LITERAL("GET"),
		Expected,
		sizeof(Expected),
		&iExpectedSize
	) && xrtHttpDigestEqual(
		pAuth->Response, (xstrview){ Expected, iExpectedSize }
	), "HTTP Digest client request proof mismatch");
	return Proof;
}



/* 验证凭据构建、短缓冲、扩展用户名和用户名边界。 */
static void testBuild(void)
{
	xhttpdigestchallenge Challenge = testChallenge();
	xhttpdigestchoice Choice;
	xhttpdigestauth Auth;
	xhttpdigestclientauth Input;
	xhttpdigestproof Proof;
	char Secret[XHTTP_DIGEST_MAX_TEXT_SIZE];
	char Output[XHTTP_DIGEST_MAX_TEXT_SIZE * 2u];
	size_t iSize;

	Proof = testClientAuth(
		&Challenge, &Choice, Secret, Output, &Auth
	);
	Input = (xhttpdigestclientauth){
		0,
		&Challenge,
		&Choice,
		XRT_STR_INIT("user"),
		{ NULL, 0 },
		Proof.Secret,
		XRT_STR_INIT("GET"),
		XRT_STR_INIT("/private"),
		XRT_STR_INIT("client-nonce"),
		{ NULL, 0 },
		1u
	};
	testRequire(!xrtHttpDigestClientAuth(
		&Input, Output, sizeof(Output) - 1u, &iSize, &Auth
	) && (iSize == sizeof(Output)),
		"HTTP Digest client short output was accepted");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"HTTP Digest client short output error mismatch");

	Challenge.Flags &= ~(XHTTP_DIGEST_CHALLENGE_HAS_USERHASH |
		XHTTP_DIGEST_CHALLENGE_USERHASH);
	Choice.UserHash = false;
	Input.Flags = XHTTP_DIGEST_CLIENT_USERNAME_EXTENDED;
	Input.Username = XRT_STR_LITERAL("user name");
	Input.UsernameLanguage = XRT_STR_LITERAL("en");
	testRequire(xrtHttpDigestClientAuth(
		&Input, Output, sizeof(Output), &iSize, &Auth
	) && (iSize == XHTTP_DIGEST_MAX_TEXT_SIZE) &&
		((Auth.Flags & XHTTP_DIGEST_AUTH_USERNAME_EXTENDED) != 0) &&
		testText(Auth.Username, "user name") &&
		testText(Auth.UsernameLanguage, "en"),
		"HTTP Digest extended username build mismatch");
	Input.Flags = 0;
	Input.Username = XRT_STR_LITERAL("bad:user");
	Input.UsernameLanguage = (xstrview){ NULL, 0 };
	testRequire(!xrtHttpDigestClientAuth(
		&Input, Output, sizeof(Output), &iSize, &Auth
	), "HTTP Digest client accepted colon username");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"HTTP Digest colon username error mismatch");
	xrtSecureZero(Secret, sizeof(Secret));
}



/* 验证 rspauth 成功、上下文不匹配和 auth-int 响应实体绑定。 */
static void testInfoVerify(void)
{
	xhttpdigestchallenge Challenge = testChallenge();
	xhttpdigestchoice Choice;
	xhttpdigestauth Auth;
	xhttpdigestproof Proof;
	xhttpdigestinfo Info;
	xhttpdigestinfoverification Verification;
	char Secret[XHTTP_DIGEST_MAX_TEXT_SIZE];
	char Output[XHTTP_DIGEST_MAX_TEXT_SIZE * 2u];
	char Response[XHTTP_DIGEST_MAX_TEXT_SIZE];
	char EntityHash[XHTTP_DIGEST_MAX_TEXT_SIZE];
	size_t iResponseSize;
	size_t iEntitySize;

	Proof = testClientAuth(
		&Challenge, &Choice, Secret, Output, &Auth
	);
	testRequire(xrtHttpDigestRspAuth(
		&Proof, Response, sizeof(Response), &iResponseSize
	), "HTTP Digest rspauth fixture failed");
	Info = (xhttpdigestinfo){
		XHTTP_DIGEST_INFO_HAS_NEXT_NONCE |
		XHTTP_DIGEST_INFO_HAS_RESPONSE,
		Proof.Algorithm,
		Proof.Qop,
		Proof.NonceCount,
		XRT_STR_INIT("next"),
		{ Response, iResponseSize },
		Proof.Cnonce
	};
	Verification = (xhttpdigestinfoverification){
		&Info,
		&Proof,
		{ NULL, 0 }
	};
	testRequire(xrtHttpDigestInfoVerify(
		&Verification
	) == XHTTP_DIGEST_INFO_VALID,
		"HTTP Digest rspauth verification mismatch");
	Info.NonceCount++;
	testRequire(xrtHttpDigestInfoVerify(
		&Verification
	) == XHTTP_DIGEST_INFO_INVALID,
		"HTTP Digest rspauth accepted wrong nonce count");
	Info.NonceCount--;
	Info.Flags = XHTTP_DIGEST_INFO_HAS_NEXT_NONCE;
	Info.Qop = XHTTP_DIGEST_QOP_NONE;
	Info.NonceCount = 0;
	Info.Response = (xstrview){ NULL, 0 };
	Info.Cnonce = (xstrview){ NULL, 0 };
	testRequire(xrtHttpDigestInfoVerify(
		&Verification
	) == XHTTP_DIGEST_INFO_INVALID,
		"HTTP Digest info without rspauth was trusted");

	testRequire(xrtHttpDigestHash(
		XHTTP_DIGEST_ALGORITHM_SHA256,
		"reply",
		5u,
		EntityHash,
		sizeof(EntityHash),
		&iEntitySize
	), "HTTP Digest response entity hash failed");
	Proof.Qop = XHTTP_DIGEST_QOP_AUTH_INT;
	Proof.EntityHash = (xstrview){ EntityHash, iEntitySize };
	testRequire(xrtHttpDigestRspAuth(
		&Proof, Response, sizeof(Response), &iResponseSize
	), "HTTP Digest auth-int rspauth fixture failed");
	Info = (xhttpdigestinfo){
		XHTTP_DIGEST_INFO_HAS_RESPONSE,
		Proof.Algorithm,
		Proof.Qop,
		Proof.NonceCount,
		{ NULL, 0 },
		{ Response, iResponseSize },
		Proof.Cnonce
	};
	Verification.ResponseEntityHash = (xstrview){
		EntityHash, iEntitySize
	};
	testRequire(xrtHttpDigestInfoVerify(
		&Verification
	) == XHTTP_DIGEST_INFO_VALID,
		"HTTP Digest auth-int rspauth verification mismatch");
	xrtSecureZero(Secret, sizeof(Secret));
}



/* 验证未对齐固定对象和危险输出别名。 */
static void testMemoryContract(void)
{
	static const char Secret[] =
		"00000000000000000000000000000000"
		"00000000000000000000000000000000";
	xhttpdigestchallenge Challenge = testChallenge();
	xhttpdigestpolicy Policy;
	xhttpdigestchoice Choice;
	xhttpdigestclientauth Input;
	xhttpdigestauth Auth;
	size_t iSize;
	uint8 Storage[sizeof(xhttpdigestchallenge) +
		sizeof(xhttpdigestpolicy) + sizeof(xhttpdigestchoice) + 3u];
	uint8 InputStorage[sizeof(xhttpdigestclientauth) + 1u];
	uint8 AuthStorage[sizeof(xhttpdigestauth) + 1u];
	uint8 SizeStorage[sizeof(size_t) + 1u];
	char Output[XHTTP_DIGEST_MAX_TEXT_SIZE * 2u];
	xhttpdigestchallenge* pChallenge =
		(xhttpdigestchallenge*)(void*)(Storage + 1u);
	xhttpdigestpolicy* pPolicy =
		(xhttpdigestpolicy*)(void*)(Storage + 1u + sizeof(Challenge));
	xhttpdigestchoice* pChoice = (xhttpdigestchoice*)(void*)(
		Storage + 1u + sizeof(Challenge) + sizeof(Policy)
	);
	xhttpdigestclientauth* pInput =
		(xhttpdigestclientauth*)(void*)(InputStorage + 1u);
	xhttpdigestauth* pAuth =
		(xhttpdigestauth*)(void*)(AuthStorage + 1u);
	size_t* pSize = (size_t*)(void*)(SizeStorage + 1u);

	xrtHttpDigestPolicyInit(&Policy);
	memcpy(pChallenge, &Challenge, sizeof(Challenge));
	memcpy(pPolicy, &Policy, sizeof(Policy));
	testRequire(xrtHttpDigestChallengeChoose(
		pChallenge, pPolicy, pChoice
	) == XHTTP_DIGEST_CHOOSE_ACCEPTED,
		"HTTP Digest unaligned choice failed");
	testRequire(xrtHttpDigestChallengeChoose(
		&Challenge,
		&Policy,
		(xhttpdigestchoice*)(void*)&Challenge
	) == XHTTP_DIGEST_CHOOSE_ERROR,
		"HTTP Digest choice accepted aliased output");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Digest choice alias error mismatch");

	Challenge = testChallenge();
	testRequire(xrtHttpDigestChallengeChoose(
		&Challenge, NULL, &Choice
	) == XHTTP_DIGEST_CHOOSE_ACCEPTED,
		"HTTP Digest memory fixture choice failed");
	Input = (xhttpdigestclientauth){
		0,
		&Challenge,
		&Choice,
		XRT_STR_INIT("user"),
		{ NULL, 0 },
		XRT_STR_INIT(Secret),
		XRT_STR_INIT("GET"),
		XRT_STR_INIT("/"),
		XRT_STR_INIT("client"),
		{ NULL, 0 },
		1u
	};
	memcpy(pInput, &Input, sizeof(Input));
	testRequire(xrtHttpDigestClientAuth(
		pInput, Output, sizeof(Output), pSize, pAuth
	), "HTTP Digest unaligned client auth failed");
	memcpy(&Auth, pAuth, sizeof(Auth));
	memcpy(&iSize, pSize, sizeof(iSize));
	testRequire((iSize == sizeof(Output)) &&
		(Auth.Response.Size == XHTTP_DIGEST_MAX_TEXT_SIZE),
		"HTTP Digest unaligned client auth result mismatch");

	Input.Username = (xstrview){
		(cstr)(uintptr_t)(UINTPTR_MAX - 1u), 4u
	};
	testRequire(!xrtHttpDigestClientAuth(
		&Input, Output, sizeof(Output), &iSize, &Auth
	), "HTTP Digest client accepted wrapped username range");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Digest wrapped username error mismatch");
}



/* 覆盖客户端协商、凭据构建和响应证明验证。 */
int main(void)
{
	testChoose();
	testBuild();
	testInfoVerify();
	testMemoryContract();
	puts("[PASS] HTTP Digest client protocol");
	return 0;
}
