#include "../internal/xrt_http.h"

#include <xrt/http_auth.h>



#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_VERIFY)

#define XRT_HTTP_DIGEST_VERIFY_VALID_FLAGS \
	((uint32)XHTTP_DIGEST_VERIFY_REQUIRE_USERHASH)



/* 工作副本允许公开描述符和两个协议对象位于未对齐存储。 */
typedef struct xrt_http_digest_verify_prepared {
	xhttpdigestverification Verification;
	xhttpdigestauth Auth;
	xhttpdigestchallenge Challenge;
} xrt_http_digest_verify_prepared;



/* 使用协议写出器的查询模式统一验证 challenge 与凭据结构。 */
static bool __xrtHttpDigestVerifyProtocol(
	const xhttpdigestauth* pAuth,
	const xhttpdigestchallenge* pChallenge
)
{
	size_t iSize;

	return xrtHttpDigestAuthWrite(
		pAuth, NULL, 0, &iSize
	) && xrtHttpDigestChallengeWrite(
		pChallenge, NULL, 0, &iSize
	);
}



/* 复制并验证固定描述符，策略错误与普通认证失败严格分离。 */
static bool __xrtHttpDigestVerifyPrepare(
	const xhttpdigestverification* pInput,
	xrt_http_digest_verify_prepared* pPrepared
)
{
	xhttpdigestverification Verification;
	xhttpdigestauth Auth;
	xhttpdigestchallenge Challenge;

	if ( !__xrtRangeValid(pInput, sizeof(*pInput)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(&Verification, pInput, sizeof(Verification));
	if ( !__xrtRangeValid(Verification.Auth, sizeof(Auth)) ||
		!__xrtRangeValid(Verification.Challenge, sizeof(Challenge)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(&Auth, Verification.Auth, sizeof(Auth));
	memcpy(&Challenge, Verification.Challenge, sizeof(Challenge));
	if ( !__xrtHttpViewValid(Verification.Secret) ||
		!__xrtHttpViewValid(Verification.Method) ||
		!__xrtHttpViewValid(Verification.RequestTarget) ||
		!__xrtHttpViewValid(Verification.EntityHash) ) {
		return false;
	}
	if ( ((Verification.Flags &
		~XRT_HTTP_DIGEST_VERIFY_VALID_FLAGS) != 0) ||
		(Verification.Method.Size == 0) ||
		(Verification.RequestTarget.Size == 0) ) {
		__xrtErrorSetValue();
		return false;
	}
	if ( !__xrtHttpDigestVerifyProtocol(&Auth, &Challenge) ) {
		return false;
	}
	if ( !xrtHttpDigestAlgorithmSupported(Challenge.Algorithm) ) {
		__xrtErrorSetUnsupported();
		return false;
	}
	if ( ((Verification.Flags &
		XHTTP_DIGEST_VERIFY_REQUIRE_USERHASH) != 0) &&
		(((Challenge.Flags &
		  XHTTP_DIGEST_CHALLENGE_HAS_USERHASH) == 0) ||
		 ((Challenge.Flags &
		  XHTTP_DIGEST_CHALLENGE_USERHASH) == 0)) ) {
		__xrtErrorSetValue();
		return false;
	}
	pPrepared->Verification = Verification;
	pPrepared->Auth = Auth;
	pPrepared->Challenge = Challenge;
	pPrepared->Verification.Auth = &pPrepared->Auth;
	pPrepared->Verification.Challenge = &pPrepared->Challenge;
	return true;
}



/* 判断凭据选择是否与服务器实际发布的 challenge 完全一致。 */
static bool __xrtHttpDigestVerifyChallenge(
	const xrt_http_digest_verify_prepared* pPrepared
)
{
	const xhttpdigestverification* pVerification =
		&pPrepared->Verification;
	const xhttpdigestauth* pAuth = &pPrepared->Auth;
	const xhttpdigestchallenge* pChallenge = &pPrepared->Challenge;
	bool bChallengeOpaque = (pChallenge->Flags &
		XHTTP_DIGEST_CHALLENGE_HAS_OPAQUE) != 0;
	bool bAuthOpaque = (pAuth->Flags &
		XHTTP_DIGEST_AUTH_HAS_OPAQUE) != 0;
	bool bUserHash = (pAuth->Flags &
		XHTTP_DIGEST_AUTH_USERHASH) != 0;
	bool bUserHashOffered =
		((pChallenge->Flags &
		  XHTTP_DIGEST_CHALLENGE_HAS_USERHASH) != 0) &&
		((pChallenge->Flags &
		  XHTTP_DIGEST_CHALLENGE_USERHASH) != 0);

	if ( (pAuth->Algorithm != pChallenge->Algorithm) ||
		!__xrtHttpViewEqual(pAuth->Realm, pChallenge->Realm) ||
		!__xrtHttpViewEqual(pAuth->Nonce, pChallenge->Nonce) ||
		!__xrtHttpViewEqual(
			pAuth->Uri, pVerification->RequestTarget
		) || (bChallengeOpaque != bAuthOpaque) ||
		(bChallengeOpaque && !__xrtHttpViewEqual(
			pAuth->Opaque, pChallenge->Opaque
		)) ) {
		return false;
	}
	if ( ((pChallenge->Flags &
		XHTTP_DIGEST_CHALLENGE_ALGORITHM_EXPLICIT) != 0) &&
		((pAuth->Flags &
		 XHTTP_DIGEST_AUTH_ALGORITHM_EXPLICIT) == 0) ) {
		return false;
	}
	if ( ((pAuth->Qop == XHTTP_DIGEST_QOP_AUTH) &&
		 ((pChallenge->Flags &
		   XHTTP_DIGEST_CHALLENGE_QOP_AUTH) == 0)) ||
		((pAuth->Qop == XHTTP_DIGEST_QOP_AUTH_INT) &&
		 ((pChallenge->Flags &
		   XHTTP_DIGEST_CHALLENGE_QOP_AUTH_INT) == 0)) ) {
		return false;
	}
	if ( (bUserHash && !bUserHashOffered) ||
		(((pVerification->Flags &
		   XHTTP_DIGEST_VERIFY_REQUIRE_USERHASH) != 0) &&
		 !bUserHash) ||
		(((pAuth->Flags &
		   XHTTP_DIGEST_AUTH_USERNAME_EXTENDED) != 0) &&
		 ((pChallenge->Flags &
		   XHTTP_DIGEST_CHALLENGE_UTF8) == 0)) ) {
		return false;
	}
	return true;
}



/* 重新计算客户端 request-digest，并用常量时间比较线路证明。 */
static xhttpdigestverifycheck __xrtHttpDigestProofVerifyPrepared(
	const xrt_http_digest_verify_prepared* pPrepared
)
{
	const xhttpdigestverification* pVerification =
		&pPrepared->Verification;
	const xhttpdigestauth* pAuth = &pPrepared->Auth;
	xhttpdigestproof Proof = {
		pAuth->Algorithm,
		pAuth->Qop,
		pAuth->NonceCount,
		pVerification->Secret,
		pAuth->Nonce,
		pAuth->Cnonce,
		pAuth->Uri,
		pVerification->EntityHash
	};
	char Expected[XHTTP_DIGEST_MAX_TEXT_SIZE];
	size_t iSize;
	xhttpdigestverifycheck Check;

	memset(Expected, 0, sizeof(Expected));
	if ( !__xrtHttpDigestVerifyChallenge(pPrepared) ) {
		return XHTTP_DIGEST_VERIFY_INVALID;
	}
	if ( !xrtHttpDigestRequest(
		&Proof,
		pVerification->Method,
		Expected,
		sizeof(Expected),
		&iSize
	) ) {
		xrtSecureZero(Expected, sizeof(Expected));
		return XHTTP_DIGEST_VERIFY_ERROR;
	}
	Check = xrtHttpDigestEqual(
		(xstrview){ Expected, iSize }, pAuth->Response
	) ? XHTTP_DIGEST_VERIFY_VALID : XHTTP_DIGEST_VERIFY_INVALID;
	xrtSecureZero(Expected, sizeof(Expected));
	return Check;
}



/* 验证可选签发时间输出不会覆盖任何仍需读取的输入。 */
static bool __xrtHttpDigestVerifyIssuedValid(
	const xhttpdigestverification* pInput,
	const xrt_http_digest_verify_prepared* pPrepared,
	xbytesview NonceKey,
	xbytesview NonceContext,
	int64* pIssuedSeconds
)
{
	const xhttpdigestverification* pVerification =
		&pPrepared->Verification;
	const xhttpdigestauth* pAuth = &pPrepared->Auth;
	const xhttpdigestchallenge* pChallenge = &pPrepared->Challenge;
	xstrview Views[18];

	if ( pIssuedSeconds == NULL ) {
		return true;
	}
	Views[0] = pVerification->Secret;
	Views[1] = pVerification->Method;
	Views[2] = pVerification->RequestTarget;
	Views[3] = pVerification->EntityHash;
	Views[4] = pAuth->Username;
	Views[5] = pAuth->UsernameLanguage;
	Views[6] = pAuth->Realm;
	Views[7] = pAuth->Nonce;
	Views[8] = pAuth->Uri;
	Views[9] = pAuth->Cnonce;
	Views[10] = pAuth->Response;
	Views[11] = pAuth->Opaque;
	Views[12] = pAuth->AlgorithmName;
	Views[13] = pChallenge->Realm;
	Views[14] = pChallenge->Domain;
	Views[15] = pChallenge->Nonce;
	Views[16] = pChallenge->Opaque;
	Views[17] = pChallenge->AlgorithmName;
	if ( !__xrtRangeValid(NonceKey.Data, NonceKey.Size) ||
		!__xrtRangeValid(NonceContext.Data, NonceContext.Size) ||
		!__xrtRangeValid(pIssuedSeconds, sizeof(*pIssuedSeconds)) ||
		__xrtRangesOverlap(
			pIssuedSeconds, sizeof(*pIssuedSeconds),
			pInput, sizeof(*pInput)
		) || __xrtRangesOverlap(
			pIssuedSeconds, sizeof(*pIssuedSeconds),
			pInput->Auth, sizeof(*pInput->Auth)
		) || __xrtRangesOverlap(
			pIssuedSeconds, sizeof(*pIssuedSeconds),
			pInput->Challenge, sizeof(*pInput->Challenge)
		) || __xrtRangesOverlap(
			pIssuedSeconds, sizeof(*pIssuedSeconds),
			NonceKey.Data, NonceKey.Size
		) || __xrtRangesOverlap(
			pIssuedSeconds, sizeof(*pIssuedSeconds),
			NonceContext.Data, NonceContext.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	for ( size_t i = 0; i < (sizeof(Views) / sizeof(Views[0])); i++ ) {
		if ( __xrtRangesOverlap(
			pIssuedSeconds, sizeof(*pIssuedSeconds),
			Views[i].Data, Views[i].Size
		) ) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
	}
	return true;
}



/* 验证 request-digest，nonce 生命周期由调用方单独处理。 */
XRT_API xhttpdigestverifycheck xrtHttpDigestProofVerify(
	const xhttpdigestverification* pVerification
)
{
	xrt_http_digest_verify_prepared Prepared;
	xhttpdigestverifycheck Check;

	memset(&Prepared, 0, sizeof(Prepared));
	if ( !__xrtHttpDigestVerifyPrepare(
		pVerification, &Prepared
	) ) {
		return XHTTP_DIGEST_VERIFY_ERROR;
	}
	Check = __xrtHttpDigestProofVerifyPrepared(&Prepared);
	xrtSecureZero(&Prepared, sizeof(Prepared));
	return Check;
}



/* 组合验证无状态 nonce 与 request-digest，并保持 stale 结果不可伪造。 */
XRT_API xhttpdigestverifycheck xrtHttpDigestVerify(
	const xhttpdigestverification* pVerification,
	xbytesview NonceKey,
	xbytesview NonceContext,
	int64 iNowSeconds,
	int64 iLifetimeSeconds,
	int64 iFutureSkewSeconds,
	int64* pIssuedSeconds
)
{
	xrt_http_digest_verify_prepared Prepared;
	xhttpdigestnoncecheck NonceCheck;
	xhttpdigestverifycheck ProofCheck;
	int64 iIssuedSeconds;

	memset(&Prepared, 0, sizeof(Prepared));
	if ( !__xrtHttpDigestVerifyPrepare(
		pVerification, &Prepared
	) || !__xrtHttpDigestVerifyIssuedValid(
		pVerification,
		&Prepared,
		NonceKey,
		NonceContext,
		pIssuedSeconds
	) ) {
		xrtSecureZero(&Prepared, sizeof(Prepared));
		return XHTTP_DIGEST_VERIFY_ERROR;
	}
	NonceCheck = xrtHttpDigestNonceVerify(
		Prepared.Auth.Nonce,
		NonceKey,
		NonceContext,
		iNowSeconds,
		iLifetimeSeconds,
		iFutureSkewSeconds,
		&iIssuedSeconds
	);
	if ( NonceCheck == XHTTP_DIGEST_NONCE_ERROR ) {
		xrtSecureZero(&Prepared, sizeof(Prepared));
		return XHTTP_DIGEST_VERIFY_ERROR;
	}
	if ( NonceCheck == XHTTP_DIGEST_NONCE_INVALID ) {
		xrtSecureZero(&Prepared, sizeof(Prepared));
		return XHTTP_DIGEST_VERIFY_INVALID;
	}
	ProofCheck = __xrtHttpDigestProofVerifyPrepared(&Prepared);
	xrtSecureZero(&Prepared, sizeof(Prepared));
	if ( ProofCheck != XHTTP_DIGEST_VERIFY_VALID ) {
		return ProofCheck;
	}
	if ( pIssuedSeconds != NULL ) {
		memcpy(
			pIssuedSeconds,
			&iIssuedSeconds,
			sizeof(iIssuedSeconds)
		);
	}
	return NonceCheck == XHTTP_DIGEST_NONCE_STALE ?
		XHTTP_DIGEST_VERIFY_STALE : XHTTP_DIGEST_VERIFY_VALID;
}



#undef XRT_HTTP_DIGEST_VERIFY_VALID_FLAGS

#endif
