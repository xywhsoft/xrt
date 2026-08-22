#include "../internal/xrt_http.h"

#include <xrt/http_auth.h>



#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_CLIENT)

#define XRT_HTTP_DIGEST_POLICY_VALID_FLAGS \
	((uint32)XHTTP_DIGEST_POLICY_PREFER_AUTH_INT | \
	 (uint32)XHTTP_DIGEST_POLICY_PLAIN_USERNAME | \
	 (uint32)XHTTP_DIGEST_POLICY_REQUIRE_USERHASH | \
	 (uint32)XHTTP_DIGEST_POLICY_REQUIRE_UTF8)

#define XRT_HTTP_DIGEST_CLIENT_AUTH_VALID_FLAGS \
	((uint32)XHTTP_DIGEST_CLIENT_USERNAME_EXTENDED)



/* 把算法枚举映射到客户端策略位。 */
static uint32 __xrtHttpDigestAlgorithmMask(
	xhttpdigestalgorithm Algorithm
)
{
	switch ( Algorithm ) {
		case XHTTP_DIGEST_ALGORITHM_MD5:
			return XHTTP_DIGEST_ALGORITHMS_MD5;
		case XHTTP_DIGEST_ALGORITHM_MD5_SESSION:
			return XHTTP_DIGEST_ALGORITHMS_MD5_SESSION;
		case XHTTP_DIGEST_ALGORITHM_SHA256:
			return XHTTP_DIGEST_ALGORITHMS_SHA256;
		case XHTTP_DIGEST_ALGORITHM_SHA256_SESSION:
			return XHTTP_DIGEST_ALGORITHMS_SHA256_SESSION;
		case XHTTP_DIGEST_ALGORITHM_SHA512_256:
			return XHTTP_DIGEST_ALGORITHMS_SHA512_256;
		case XHTTP_DIGEST_ALGORITHM_SHA512_256_SESSION:
			return XHTTP_DIGEST_ALGORITHMS_SHA512_256_SESSION;
		default:
			return 0;
	}
}



/* 把 qop 枚举映射到客户端策略位。 */
static uint32 __xrtHttpDigestQopMask(xhttpdigestqop Qop)
{
	if ( Qop == XHTTP_DIGEST_QOP_AUTH ) {
		return XHTTP_DIGEST_QOPS_AUTH;
	}
	if ( Qop == XHTTP_DIGEST_QOP_AUTH_INT ) {
		return XHTTP_DIGEST_QOPS_AUTH_INT;
	}
	return 0;
}



/* 使用协议写出器的查询模式验证 challenge 结构。 */
static bool __xrtHttpDigestClientChallengeValid(
	const xhttpdigestchallenge* pChallenge
)
{
	size_t iSize;

	return xrtHttpDigestChallengeWrite(
		pChallenge, NULL, 0, &iSize
	);
}



/* 验证协商输出不会覆盖 challenge、策略或其借用值。 */
static bool __xrtHttpDigestChoiceOutputValid(
	const xhttpdigestchallenge* pInput,
	const xhttpdigestchallenge* pChallenge,
	const xhttpdigestpolicy* pPolicy,
	xhttpdigestchoice* pChoice
)
{
	xstrview Views[5];

	if ( !__xrtRangeValid(pChoice, sizeof(*pChoice)) ||
		__xrtRangesOverlap(
			pChoice, sizeof(*pChoice), pInput, sizeof(*pInput)
		) || ((pPolicy != NULL) && (!__xrtRangeValid(
			pPolicy, sizeof(*pPolicy)
		) || __xrtRangesOverlap(
			pChoice, sizeof(*pChoice), pPolicy, sizeof(*pPolicy)
		))) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Views[0] = pChallenge->Realm;
	Views[1] = pChallenge->Domain;
	Views[2] = pChallenge->Nonce;
	Views[3] = pChallenge->Opaque;
	Views[4] = pChallenge->AlgorithmName;
	for ( size_t i = 0; i < (sizeof(Views) / sizeof(Views[0])); i++ ) {
		if ( __xrtRangesOverlap(
			pChoice, sizeof(*pChoice), Views[i].Data, Views[i].Size
		) ) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
	}
	return true;
}



/* 复制并校验本地协商策略。 */
static bool __xrtHttpDigestPolicyPrepare(
	const xhttpdigestpolicy* pInput,
	xhttpdigestpolicy* pPolicy
)
{
	if ( pInput == NULL ) {
		xrtHttpDigestPolicyInit(pPolicy);
		return true;
	}
	memcpy(pPolicy, pInput, sizeof(*pPolicy));
	if ( ((pPolicy->Flags &
		~XRT_HTTP_DIGEST_POLICY_VALID_FLAGS) != 0) ||
		((pPolicy->Algorithms &
		 ~XHTTP_DIGEST_ALGORITHMS_ALL) != 0) ||
		((pPolicy->Qops & ~XHTTP_DIGEST_QOPS_ALL) != 0) ||
		(pPolicy->Algorithms == 0) ||
		(pPolicy->Qops == 0) ||
		(((pPolicy->Flags &
		  XHTTP_DIGEST_POLICY_PLAIN_USERNAME) != 0) &&
		 ((pPolicy->Flags &
		  XHTTP_DIGEST_POLICY_REQUIRE_USERHASH) != 0)) ) {
		__xrtErrorSetValue();
		return false;
	}
	return true;
}



/* 初始化不启用遗留 MD5 的现代客户端策略。 */
XRT_API void xrtHttpDigestPolicyInit(xhttpdigestpolicy* pPolicy)
{
	xhttpdigestpolicy Policy = {
		0,
		XHTTP_DIGEST_ALGORITHMS_SHA2,
		XHTTP_DIGEST_QOPS_ALL
	};

	if ( !__xrtRangeValid(pPolicy, sizeof(*pPolicy)) ) {
		return;
	}
	memcpy(pPolicy, &Policy, sizeof(Policy));
}



/* 按服务器顺序之外的单项本地策略判断 challenge。 */
XRT_API xhttpdigestchoosecheck xrtHttpDigestChallengeChoose(
	const xhttpdigestchallenge* pInput,
	const xhttpdigestpolicy* pPolicyInput,
	xhttpdigestchoice* pChoice
)
{
	xhttpdigestchallenge Challenge;
	xhttpdigestpolicy Policy;
	xhttpdigestchoice Choice = { 0 };
	uint32 iOffered;
	uint32 iAllowed;
	bool bUserHash;

	if ( !__xrtRangeValid(pInput, sizeof(*pInput)) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_DIGEST_CHOOSE_ERROR;
	}
	memcpy(&Challenge, pInput, sizeof(Challenge));
	if ( !__xrtHttpDigestChoiceOutputValid(
		pInput, &Challenge, pPolicyInput, pChoice
	) ) {
		return XHTTP_DIGEST_CHOOSE_ERROR;
	}
	memcpy(pChoice, &Choice, sizeof(Choice));
	if ( !__xrtHttpDigestPolicyPrepare(
		pPolicyInput, &Policy
	) || !__xrtHttpDigestClientChallengeValid(&Challenge) ) {
		return XHTTP_DIGEST_CHOOSE_ERROR;
	}
	if ( ((__xrtHttpDigestAlgorithmMask(Challenge.Algorithm) &
		Policy.Algorithms) == 0) ||
		!xrtHttpDigestAlgorithmSupported(Challenge.Algorithm) ||
		(((Policy.Flags & XHTTP_DIGEST_POLICY_REQUIRE_UTF8) != 0) &&
		 ((Challenge.Flags & XHTTP_DIGEST_CHALLENGE_UTF8) == 0)) ) {
		return XHTTP_DIGEST_CHOOSE_REJECTED;
	}
	iOffered = 0;
	if ( (Challenge.Flags & XHTTP_DIGEST_CHALLENGE_QOP_AUTH) != 0 ) {
		iOffered |= XHTTP_DIGEST_QOPS_AUTH;
	}
	if ( (Challenge.Flags & XHTTP_DIGEST_CHALLENGE_QOP_AUTH_INT) != 0 ) {
		iOffered |= XHTTP_DIGEST_QOPS_AUTH_INT;
	}
	iAllowed = iOffered & Policy.Qops;
	if ( iAllowed == 0 ) {
		return XHTTP_DIGEST_CHOOSE_REJECTED;
	}
	bUserHash = ((Challenge.Flags &
		XHTTP_DIGEST_CHALLENGE_HAS_USERHASH) != 0) &&
		((Challenge.Flags &
		XHTTP_DIGEST_CHALLENGE_USERHASH) != 0);
	if ( ((Policy.Flags &
		XHTTP_DIGEST_POLICY_REQUIRE_USERHASH) != 0) && !bUserHash ) {
		return XHTTP_DIGEST_CHOOSE_REJECTED;
	}
	Choice.Algorithm = Challenge.Algorithm;
	if ( ((Policy.Flags &
		XHTTP_DIGEST_POLICY_PREFER_AUTH_INT) != 0) &&
		((iAllowed & XHTTP_DIGEST_QOPS_AUTH_INT) != 0) ) {
		Choice.Qop = XHTTP_DIGEST_QOP_AUTH_INT;
	} else if ( (iAllowed & XHTTP_DIGEST_QOPS_AUTH) != 0 ) {
		Choice.Qop = XHTTP_DIGEST_QOP_AUTH;
	} else {
		Choice.Qop = XHTTP_DIGEST_QOP_AUTH_INT;
	}
	Choice.UserHash = bUserHash && ((Policy.Flags &
		XHTTP_DIGEST_POLICY_PLAIN_USERNAME) == 0);
	memcpy(pChoice, &Choice, sizeof(Choice));
	return XHTTP_DIGEST_CHOOSE_ACCEPTED;
}



/* 工作副本允许公开描述符和协议对象位于未对齐存储。 */
typedef struct xrt_http_digest_client_prepared {
	xhttpdigestclientauth Input;
	xhttpdigestchallenge Challenge;
	xhttpdigestchoice Choice;
} xrt_http_digest_client_prepared;



/* 判断用户名是否含有会使 H(username:realm:password) 歧义的冒号。 */
static bool __xrtHttpDigestUsernameValid(xstrview Username)
{
	if ( Username.Size == 0 ) {
		return false;
	}
	for ( size_t i = 0; i < Username.Size; i++ ) {
		if ( Username.Data[i] == ':' ) {
			return false;
		}
	}
	return true;
}



/* 校验凭据输出描述符及其与全部输入区间的别名关系。 */
static bool __xrtHttpDigestClientOutputValid(
	const xhttpdigestclientauth* pInput,
	const xrt_http_digest_client_prepared* pPrepared,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestauth* pAuth
)
{
	const xhttpdigestclientauth* pClient = &pPrepared->Input;
	xstrview Views[12];

	if ( !__xrtRangeValid(pSize, sizeof(*pSize)) ||
		!__xrtRangeValid(pAuth, sizeof(*pAuth)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		((pOutput != NULL) && !__xrtRangeValid(
			pOutput, iCapacity
		)) || __xrtRangesOverlap(
			pSize, sizeof(*pSize), pAuth, sizeof(*pAuth)
		) || __xrtRangesOverlap(
			pSize, sizeof(*pSize), pInput, sizeof(*pInput)
		) || __xrtRangesOverlap(
			pAuth, sizeof(*pAuth), pInput, sizeof(*pInput)
		) || __xrtRangesOverlap(
			pSize, sizeof(*pSize), pClient->Challenge,
			sizeof(*pClient->Challenge)
		) || __xrtRangesOverlap(
			pAuth, sizeof(*pAuth), pClient->Challenge,
			sizeof(*pClient->Challenge)
		) || __xrtRangesOverlap(
			pSize, sizeof(*pSize), pClient->Choice,
			sizeof(*pClient->Choice)
		) || __xrtRangesOverlap(
			pAuth, sizeof(*pAuth), pClient->Choice,
			sizeof(*pClient->Choice)
		) || ((pOutput != NULL) && (__xrtRangesOverlap(
			pOutput, iCapacity, pSize, sizeof(*pSize)
		) || __xrtRangesOverlap(
			pOutput, iCapacity, pAuth, sizeof(*pAuth)
		) || __xrtRangesOverlap(
			pOutput, iCapacity, pInput, sizeof(*pInput)
		) || __xrtRangesOverlap(
			pOutput, iCapacity, pClient->Challenge,
			sizeof(*pClient->Challenge)
		) || __xrtRangesOverlap(
			pOutput, iCapacity, pClient->Choice,
			sizeof(*pClient->Choice)
		))) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Views[0] = pClient->Username;
	Views[1] = pClient->UsernameLanguage;
	Views[2] = pClient->Secret;
	Views[3] = pClient->Method;
	Views[4] = pClient->RequestTarget;
	Views[5] = pClient->Cnonce;
	Views[6] = pClient->EntityHash;
	Views[7] = pPrepared->Challenge.Realm;
	Views[8] = pPrepared->Challenge.Domain;
	Views[9] = pPrepared->Challenge.Nonce;
	Views[10] = pPrepared->Challenge.Opaque;
	Views[11] = pPrepared->Challenge.AlgorithmName;
	for ( size_t i = 0;
		i < (sizeof(Views) / sizeof(Views[0]));
		i++ ) {
		if ( !__xrtHttpViewValid(Views[i]) ||
			__xrtRangesOverlap(
				pSize, sizeof(*pSize), Views[i].Data, Views[i].Size
			) || __xrtRangesOverlap(
				pAuth, sizeof(*pAuth), Views[i].Data, Views[i].Size
			) || ((pOutput != NULL) && __xrtRangesOverlap(
				pOutput, iCapacity, Views[i].Data, Views[i].Size
			)) ) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
	}
	return true;
}



/* 复制并验证客户端凭据输入的固定结构和语义关系。 */
static bool __xrtHttpDigestClientPrepare(
	const xhttpdigestclientauth* pInput,
	xrt_http_digest_client_prepared* pPrepared
)
{
	const xhttpdigestclientauth* pClient;
	const xhttpdigestchallenge* pChallenge;
	const xhttpdigestchoice* pChoice;
	xstrview Views[7];
	uint32 iChoiceQop;
	size_t iDigest;

	if ( !__xrtRangeValid(pInput, sizeof(*pInput)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(&pPrepared->Input, pInput, sizeof(pPrepared->Input));
	if ( !__xrtRangeValid(
		pPrepared->Input.Challenge, sizeof(pPrepared->Challenge)
	) || !__xrtRangeValid(
		pPrepared->Input.Choice, sizeof(pPrepared->Choice)
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(
		&pPrepared->Challenge,
		pPrepared->Input.Challenge,
		sizeof(pPrepared->Challenge)
	);
	memcpy(
		&pPrepared->Choice,
		pPrepared->Input.Choice,
		sizeof(pPrepared->Choice)
	);
	pClient = &pPrepared->Input;
	pChallenge = &pPrepared->Challenge;
	pChoice = &pPrepared->Choice;
	Views[0] = pClient->Username;
	Views[1] = pClient->UsernameLanguage;
	Views[2] = pClient->Secret;
	Views[3] = pClient->Method;
	Views[4] = pClient->RequestTarget;
	Views[5] = pClient->Cnonce;
	Views[6] = pClient->EntityHash;
	for ( size_t i = 0; i < (sizeof(Views) / sizeof(Views[0])); i++ ) {
		if ( !__xrtHttpViewValid(Views[i]) ) {
			return false;
		}
	}
	if ( !__xrtHttpDigestClientChallengeValid(pChallenge) ||
		!xrtHttpDigestAlgorithmSupported(pChoice->Algorithm) ) {
		if ( xrtHttpDigestSize(pChoice->Algorithm) == 0 ) {
			__xrtErrorSetValue();
		} else if ( !xrtHttpDigestAlgorithmSupported(
			pChoice->Algorithm
		) ) {
			__xrtErrorSetUnsupported();
		}
		return false;
	}
	iChoiceQop = __xrtHttpDigestQopMask(pChoice->Qop);
	iDigest = xrtHttpDigestSize(pChoice->Algorithm) * 2u;
	if ( ((pClient->Flags &
		~XRT_HTTP_DIGEST_CLIENT_AUTH_VALID_FLAGS) != 0) ||
		(pChoice->Algorithm != pChallenge->Algorithm) ||
		(iChoiceQop == 0) ||
		((pChoice->Qop == XHTTP_DIGEST_QOP_AUTH) &&
		 ((pChallenge->Flags &
		  XHTTP_DIGEST_CHALLENGE_QOP_AUTH) == 0)) ||
		((pChoice->Qop == XHTTP_DIGEST_QOP_AUTH_INT) &&
		 ((pChallenge->Flags &
		  XHTTP_DIGEST_CHALLENGE_QOP_AUTH_INT) == 0)) ||
		(pChoice->UserHash && ((((pChallenge->Flags &
		  XHTTP_DIGEST_CHALLENGE_HAS_USERHASH) == 0) ||
		 ((pChallenge->Flags &
		  XHTTP_DIGEST_CHALLENGE_USERHASH) == 0)))) ||
		(((pClient->Flags &
		  XHTTP_DIGEST_CLIENT_USERNAME_EXTENDED) != 0) &&
		 (pChoice->UserHash || ((pChallenge->Flags &
		  XHTTP_DIGEST_CHALLENGE_UTF8) == 0))) ||
		(((pClient->Flags &
		  XHTTP_DIGEST_CLIENT_USERNAME_EXTENDED) == 0) &&
		 (pClient->UsernameLanguage.Size != 0)) ||
		!__xrtHttpDigestUsernameValid(pClient->Username) ||
		(pClient->Secret.Size != iDigest) ||
		!__xrtHttpDigestHexViewValid(pClient->Secret) ||
		(pClient->Method.Size == 0) ||
		(pClient->RequestTarget.Size == 0) ||
		(pClient->Cnonce.Size == 0) ||
		(pClient->NonceCount == 0) ||
		((pChoice->Qop == XHTTP_DIGEST_QOP_AUTH) &&
		 (pClient->EntityHash.Size != 0)) ||
		((pChoice->Qop == XHTTP_DIGEST_QOP_AUTH_INT) &&
		 ((pClient->EntityHash.Size != iDigest) ||
		  !__xrtHttpDigestHexViewValid(pClient->EntityHash))) ) {
		__xrtErrorSetValue();
		return false;
	}
	return true;
}



/* 从 challenge、选择结果和输入视图组装尚未计算摘要的凭据。 */
static void __xrtHttpDigestClientAuthInit(
	const xrt_http_digest_client_prepared* pPrepared,
	xhttpdigestauth* pAuth
)
{
	const xhttpdigestclientauth* pInput = &pPrepared->Input;
	const xhttpdigestchallenge* pChallenge = &pPrepared->Challenge;
	const xhttpdigestchoice* pChoice = &pPrepared->Choice;
	xhttpdigestauth Auth = { 0 };

	Auth.Algorithm = pChoice->Algorithm;
	Auth.Qop = pChoice->Qop;
	Auth.NonceCount = pInput->NonceCount;
	Auth.Realm = pChallenge->Realm;
	Auth.Nonce = pChallenge->Nonce;
	Auth.Uri = pInput->RequestTarget;
	Auth.Cnonce = pInput->Cnonce;
	if ( (pChallenge->Flags &
		XHTTP_DIGEST_CHALLENGE_ALGORITHM_EXPLICIT) != 0 ) {
		Auth.Flags |= XHTTP_DIGEST_AUTH_ALGORITHM_EXPLICIT;
	}
	if ( (pChallenge->Flags &
		XHTTP_DIGEST_CHALLENGE_HAS_OPAQUE) != 0 ) {
		Auth.Flags |= XHTTP_DIGEST_AUTH_HAS_OPAQUE;
		Auth.Opaque = pChallenge->Opaque;
	}
	if ( pChoice->UserHash ) {
		Auth.Flags |= XHTTP_DIGEST_AUTH_HAS_USERHASH |
			XHTTP_DIGEST_AUTH_USERHASH;
	} else {
		Auth.Username = pInput->Username;
		if ( (pInput->Flags &
			XHTTP_DIGEST_CLIENT_USERNAME_EXTENDED) != 0 ) {
			Auth.Flags |= XHTTP_DIGEST_AUTH_USERNAME_EXTENDED;
			Auth.UsernameLanguage = pInput->UsernameLanguage;
		}
	}
	*pAuth = Auth;
}



/* 计算用户名摘要和 request-digest，并发布最终凭据。 */
static bool __xrtHttpDigestClientAuthWrite(
	const xrt_http_digest_client_prepared* pPrepared,
	void* pOutput,
	size_t iRequired,
	xhttpdigestauth* pAuth
)
{
	const xhttpdigestclientauth* pInput = &pPrepared->Input;
	xhttpdigestauth Auth;
	xhttpdigestproof Proof;
	bytes pNext = (bytes)pOutput;
	size_t iWritten;
	size_t iDigest = xrtHttpDigestSize(
		pPrepared->Choice.Algorithm
	) * 2u;

	__xrtHttpDigestClientAuthInit(pPrepared, &Auth);
	if ( pPrepared->Choice.UserHash ) {
		if ( !xrtHttpDigestUserHash(
			pPrepared->Choice.Algorithm,
			pInput->Username,
			pPrepared->Challenge.Realm,
			pNext,
			iDigest,
			&iWritten
		) ) {
			goto fail;
		}
		Auth.Username = (xstrview){ (cstr)pNext, iWritten };
		pNext += iWritten;
	}
	Proof = (xhttpdigestproof){
		pPrepared->Choice.Algorithm,
		pPrepared->Choice.Qop,
		pInput->NonceCount,
		pInput->Secret,
		pPrepared->Challenge.Nonce,
		pInput->Cnonce,
		pInput->RequestTarget,
		pInput->EntityHash
	};
	if ( !xrtHttpDigestRequest(
		&Proof,
		pInput->Method,
		pNext,
		iDigest,
		&iWritten
	) ) {
		goto fail;
	}
	Auth.Response = (xstrview){ (cstr)pNext, iWritten };
	if ( !xrtHttpDigestAuthWrite(
		&Auth, NULL, 0, &iWritten
	) ) {
		goto fail;
	}
	memcpy(pAuth, &Auth, sizeof(Auth));
	return true;

fail:
	xrtSecureZero(pOutput, iRequired);
	return false;
}



/* 构造一份不保存密码、不分配内存的 Digest 客户端凭据。 */
XRT_API bool xrtHttpDigestClientAuth(
	const xhttpdigestclientauth* pInput,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestauth* pAuth
)
{
	xrt_http_digest_client_prepared Prepared;
	xhttpdigestauth Auth;
	size_t iDigest;
	size_t iRequired;

	memset(&Prepared, 0, sizeof(Prepared));
	memset(&Auth, 0, sizeof(Auth));
	if ( !__xrtHttpDigestClientPrepare(
		pInput, &Prepared
	) || !__xrtHttpDigestClientOutputValid(
		pInput, &Prepared, pOutput, iCapacity, pSize, pAuth
	) ) {
		xrtSecureZero(&Prepared, sizeof(Prepared));
		return false;
	}
	iDigest = xrtHttpDigestSize(Prepared.Choice.Algorithm) * 2u;
	iRequired = iDigest + (Prepared.Choice.UserHash ? iDigest : 0u);
	__xrtHttpDigestClientAuthInit(&Prepared, &Auth);
	memcpy(pAuth, &Auth, sizeof(Auth));
	if ( pOutput == NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		xrtSecureZero(&Prepared, sizeof(Prepared));
		return true;
	}
	if ( iCapacity < iRequired ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		__xrtErrorSetRange();
		xrtSecureZero(&Prepared, sizeof(Prepared));
		return false;
	}
	if ( !__xrtHttpDigestClientAuthWrite(
		&Prepared, pOutput, iRequired, &Auth
	) ) {
		xrtSecureZero(&Prepared, sizeof(Prepared));
		return false;
	}
	memcpy(pSize, &iRequired, sizeof(iRequired));
	memcpy(pAuth, &Auth, sizeof(Auth));
	xrtSecureZero(&Prepared, sizeof(Prepared));
	return true;
}



/* 复制并验证响应证明描述符和两个协议对象。 */
static bool __xrtHttpDigestInfoVerificationPrepare(
	const xhttpdigestinfoverification* pInput,
	xhttpdigestinfoverification* pVerification,
	xhttpdigestinfo* pInfo,
	xhttpdigestproof* pProof
)
{
	size_t iSize;

	if ( !__xrtRangeValid(pInput, sizeof(*pInput)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pVerification, pInput, sizeof(*pVerification));
	if ( !__xrtRangeValid(
		pVerification->Info, sizeof(*pInfo)
	) || !__xrtRangeValid(
		pVerification->Proof, sizeof(*pProof)
	) || !__xrtHttpViewValid(pVerification->ResponseEntityHash) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pInfo, pVerification->Info, sizeof(*pInfo));
	memcpy(pProof, pVerification->Proof, sizeof(*pProof));
	if ( !xrtHttpDigestInfoWrite(
		pInfo, NULL, 0, &iSize
	) ) {
		return false;
	}
	if ( pProof->Qop == XHTTP_DIGEST_QOP_AUTH_INT ) {
		pProof->EntityHash = pVerification->ResponseEntityHash;
	} else if ( pVerification->ResponseEntityHash.Size != 0 ) {
		__xrtErrorSetValue();
		return false;
	}
	return true;
}



/* 验证服务器响应证明，并把 nextnonce 的采用留给更高层状态机。 */
XRT_API xhttpdigestinfocheck xrtHttpDigestInfoVerify(
	const xhttpdigestinfoverification* pInput
)
{
	xhttpdigestinfoverification Verification;
	xhttpdigestinfo Info;
	xhttpdigestproof Proof;
	char Expected[XRT_HTTP_DIGEST_MAX_TEXT_SIZE];
	size_t iSize;
	xhttpdigestinfocheck Check;

	memset(&Verification, 0, sizeof(Verification));
	memset(&Info, 0, sizeof(Info));
	memset(&Proof, 0, sizeof(Proof));
	memset(Expected, 0, sizeof(Expected));
	if ( !__xrtHttpDigestInfoVerificationPrepare(
		pInput, &Verification, &Info, &Proof
	) || !xrtHttpDigestRspAuth(
		&Proof, Expected, sizeof(Expected), &iSize
	) ) {
		xrtSecureZero(&Verification, sizeof(Verification));
		xrtSecureZero(&Info, sizeof(Info));
		xrtSecureZero(&Proof, sizeof(Proof));
		xrtSecureZero(Expected, sizeof(Expected));
		return XHTTP_DIGEST_INFO_ERROR;
	}
	Check = ((Info.Flags & XHTTP_DIGEST_INFO_HAS_RESPONSE) != 0) &&
		(Info.Algorithm == Proof.Algorithm) &&
		(Info.Qop == Proof.Qop) &&
		(Info.NonceCount == Proof.NonceCount) &&
		__xrtHttpViewEqual(Info.Cnonce, Proof.Cnonce) &&
		xrtHttpDigestEqual(
			Info.Response, (xstrview){ Expected, iSize }
		) ? XHTTP_DIGEST_INFO_VALID : XHTTP_DIGEST_INFO_INVALID;
	xrtSecureZero(&Verification, sizeof(Verification));
	xrtSecureZero(&Info, sizeof(Info));
	xrtSecureZero(&Proof, sizeof(Proof));
	xrtSecureZero(Expected, sizeof(Expected));
	return Check;
}



#undef XRT_HTTP_DIGEST_POLICY_VALID_FLAGS
#undef XRT_HTTP_DIGEST_CLIENT_AUTH_VALID_FLAGS

#endif
