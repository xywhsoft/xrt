#include <stdlib.h>
#include <string.h>

#include <xrt/http_auth.h>



#define XRT_HTTP_AUTH_FUZZ_INPUT_MAX ((size_t)1048576u)
#define XRT_HTTP_AUTH_FUZZ_COPY_MAX ((size_t)4096u)



/* 判断借用文本是否完整落在指定输入中。 */
static bool __xrtHttpAuthFuzzViewInside(
	xstrview Input,
	xstrview View
)
{
	uintptr_t iInput;
	uintptr_t iView;

	if ( View.Data == NULL ) {
		return View.Size == 0u;
	}
	if ( Input.Data == NULL ) {
		return false;
	}
	iInput = (uintptr_t)Input.Data;
	iView = (uintptr_t)View.Data;
	return (iView >= iInput) &&
		((iView - iInput) <= Input.Size) &&
		(View.Size <= (Input.Size - (iView - iInput)));
}



/* 检查解析结果的借用范围、类型和值内部参数游标。 */
static void __xrtHttpAuthFuzzAuth(
	xstrview Input,
	const xhttpauth* pAuth
)
{
	if ( !__xrtHttpAuthFuzzViewInside(Input, pAuth->Scheme) ||
		(pAuth->Scheme.Size == 0u) ||
		!xrtHttpTokenValid(pAuth->Scheme) ||
		!__xrtHttpAuthFuzzViewInside(Input, pAuth->Data) ||
		(pAuth->Kind < XHTTP_AUTH_NONE) ||
		(pAuth->Kind > XHTTP_AUTH_PARAMS) ) {
		abort();
	}
	if ( pAuth->Kind == XHTTP_AUTH_NONE ) {
		if ( pAuth->Data.Size != 0u ) {
			abort();
		}
		return;
	}
	if ( pAuth->Kind == XHTTP_AUTH_TOKEN68 ) {
		if ( !xrtHttpAuthToken68Valid(pAuth->Data) ) {
			abort();
		}
		return;
	}

	/* 参数列表必须前进到终点，且每个视图仍借用认证数据。 */
	{
		xhttpparam Param;
		size_t iOffset = 0u;
		size_t iGuard = 0u;

		for ( ;; ) {
			size_t iBefore = iOffset;
			xhttpnext Next = xrtHttpAuthParamNext(
				pAuth->Data,
				&iOffset,
				&Param
			);

			if ( Next == XHTTP_NEXT_END ) {
				break;
			}
			if ( (Next != XHTTP_NEXT_ITEM) ||
				(iOffset <= iBefore) ||
				(iOffset > pAuth->Data.Size) ||
				(++iGuard > (pAuth->Data.Size + 1u)) ||
				!__xrtHttpAuthFuzzViewInside(
					pAuth->Data,
					Param.Name
				) || !__xrtHttpAuthFuzzViewInside(
					pAuth->Data,
					Param.Value
				) || ((Param.Flags & XHTTP_PARAM_HAS_VALUE) == 0u) ) {
				abort();
			}
		}
	}
}



/* 把任意合法解析结果写回并再次严格解析。 */
static void __xrtHttpAuthFuzzRoundTrip(const xhttpauth* pAuth)
{
	char Output[XRT_HTTP_AUTH_FUZZ_COPY_MAX];
	xhttpauth Parsed;
	size_t iRequired;
	size_t iWritten;

	if ( !xrtHttpAuthWrite(
		pAuth->Scheme,
		pAuth->Data,
		NULL,
		0,
		&iRequired
	) ) {
		abort();
	}
	if ( iRequired > sizeof(Output) ) {
		return;
	}
	if ( !xrtHttpAuthWrite(
		pAuth->Scheme,
		pAuth->Data,
		Output,
		sizeof(Output),
		&iWritten
	) || (iWritten != iRequired) ||
		!xrtHttpAuthParse(
			(xstrview){ Output, iWritten },
			&Parsed
		) || (Parsed.Kind != pAuth->Kind) ||
		!xrtHttpTokenEqual(Parsed.Scheme, pAuth->Scheme) ||
		(Parsed.Data.Size != pAuth->Data.Size) ||
		((Parsed.Data.Size != 0u) &&
		 (memcmp(
			Parsed.Data.Data,
			pAuth->Data.Data,
			Parsed.Data.Size
		 ) != 0)) ) {
		abort();
	}
}



/* 迭代任意 challenge 列表并约束游标单调前进。 */
static void __xrtHttpAuthFuzzChallenges(xstrview Input)
{
	xhttpauth Auth;
	size_t iOffset = 0u;
	size_t iGuard = 0u;

	for ( ;; ) {
		size_t iBefore = iOffset;
		xhttpnext Next = xrtHttpChallengeNext(
			Input,
			&iOffset,
			&Auth
		);

		if ( Next != XHTTP_NEXT_ITEM ) {
			break;
		}
		if ( (iOffset <= iBefore) || (iOffset > Input.Size) ||
			(++iGuard > (Input.Size + 1u)) ) {
			abort();
		}
		__xrtHttpAuthFuzzAuth(Input, &Auth);
		__xrtHttpAuthFuzzRoundTrip(&Auth);
	}
	xrtClearError();
}



/* 严格解析单份凭据，并验证成功结果可往返。 */
static void __xrtHttpAuthFuzzCredentials(xstrview Input)
{
	xhttpauth Auth;

	if ( xrtHttpAuthParse(Input, &Auth) ) {
		__xrtHttpAuthFuzzAuth(Input, &Auth);
		__xrtHttpAuthFuzzRoundTrip(&Auth);
	}
	xrtClearError();
}



#if defined(XRT_FEATURE_HTTP_AUTH_BASIC)

/* 解码任意 Basic 凭据并约束明文视图和规范写回。 */
static void __xrtHttpAuthFuzzBasic(xstrview Input)
{
	unsigned char Plain[XRT_HTTP_AUTH_FUZZ_COPY_MAX];
	char Encoded[XRT_HTTP_AUTH_FUZZ_COPY_MAX];
	xhttpbasicauth Basic;
	size_t iRequired;
	size_t iWritten;
	size_t iEncoded;

	if ( !xrtHttpBasicRead(
		Input,
		NULL,
		0,
		&iRequired,
		&Basic
	) ) {
		xrtClearError();
		return;
	}
	if ( iRequired > sizeof(Plain) ) {
		return;
	}
	if ( !xrtHttpBasicRead(
		Input,
		Plain,
		sizeof(Plain),
		&iWritten,
		&Basic
	) || (iWritten != iRequired) ||
		!__xrtHttpAuthFuzzViewInside(
			(xstrview){ (cstr)Plain, iWritten },
			Basic.User
		) || !__xrtHttpAuthFuzzViewInside(
			(xstrview){ (cstr)Plain, iWritten },
			Basic.Password
		) || (Basic.User.Size + Basic.Password.Size + 1u != iWritten) ||
		(Plain[Basic.User.Size] != (unsigned char)':') ||
		!xrtHttpBasicWrite(
			Basic.User,
			Basic.Password,
			NULL,
			0,
			&iEncoded
		) || (iEncoded > sizeof(Encoded)) ||
		!xrtHttpBasicWrite(
			Basic.User,
			Basic.Password,
			Encoded,
			sizeof(Encoded),
			&iRequired
		) || (iRequired != iEncoded) ) {
		abort();
	}
	xrtSecureZero(Plain, sizeof(Plain));
	xrtSecureZero(Encoded, sizeof(Encoded));
	xrtClearError();
}



/* 解析任意 Basic challenge，并验证 realm 可规范写回。 */
static void __xrtHttpAuthFuzzBasicChallenge(xstrview Input)
{
	char Decoded[XRT_HTTP_AUTH_FUZZ_COPY_MAX];
	char Encoded[XRT_HTTP_AUTH_FUZZ_COPY_MAX];
	xhttpbasicchallenge Challenge;
	size_t iRequired;
	size_t iWritten;

	if ( !xrtHttpBasicChallengeRead(
		Input, NULL, 0, &iRequired, &Challenge
	) ) {
		xrtClearError();
		return;
	}
	if ( iRequired > sizeof(Decoded) ) {
		return;
	}
	if ( !xrtHttpBasicChallengeRead(
		Input,
		Decoded,
		sizeof(Decoded),
		&iWritten,
		&Challenge
	) || (iWritten != iRequired) ||
		!__xrtHttpAuthFuzzViewInside(
			(xstrview){ Decoded, iWritten }, Challenge.Realm
		) || !xrtHttpBasicChallengeWrite(
			Challenge.Realm,
			Challenge.Utf8,
			NULL,
			0,
			&iRequired
		) ) {
		abort();
	}
	if ( iRequired <= sizeof(Encoded) ) {
		if ( !xrtHttpBasicChallengeWrite(
			Challenge.Realm,
			Challenge.Utf8,
			Encoded,
			sizeof(Encoded),
			&iWritten
		) || (iWritten != iRequired) ) {
			abort();
		}
	}
	xrtClearError();
}

#endif



#if defined(XRT_FEATURE_HTTP_AUTH_BEARER)

/* 解析任意 Bearer 凭据并检查 token 始终借用输入。 */
static void __xrtHttpAuthFuzzBearer(xstrview Input)
{
	xstrview Token;

	if ( xrtHttpBearerRead(Input, &Token) &&
		(!__xrtHttpAuthFuzzViewInside(Input, Token) ||
		 !xrtHttpBearerTokenValid(Token)) ) {
		abort();
	}
	xrtClearError();
}

#endif



#if defined(XRT_FEATURE_HTTP_AUTH_BEARER_CHALLENGE)

/* 判断所有存在的标准 challenge 值都落在解码输出中。 */
static bool __xrtHttpAuthFuzzBearerChallengeViews(
	xstrview Output,
	const xhttpbearerchallenge* pChallenge
)
{
	if ( ((pChallenge->Flags & XHTTP_BEARER_HAS_REALM) != 0) &&
		!__xrtHttpAuthFuzzViewInside(Output, pChallenge->Realm) ) {
		return false;
	}
	if ( ((pChallenge->Flags & XHTTP_BEARER_HAS_SCOPE) != 0) &&
		!__xrtHttpAuthFuzzViewInside(Output, pChallenge->Scope) ) {
		return false;
	}
	if ( ((pChallenge->Flags & XHTTP_BEARER_HAS_ERROR) != 0) &&
		!__xrtHttpAuthFuzzViewInside(Output, pChallenge->Error) ) {
		return false;
	}
	if ( ((pChallenge->Flags &
		XHTTP_BEARER_HAS_ERROR_DESCRIPTION) != 0) &&
		!__xrtHttpAuthFuzzViewInside(
			Output, pChallenge->ErrorDescription
		) ) {
		return false;
	}
	if ( ((pChallenge->Flags & XHTTP_BEARER_HAS_ERROR_URI) != 0) &&
		!__xrtHttpAuthFuzzViewInside(Output, pChallenge->ErrorUri) ) {
		return false;
	}
	return true;
}



/* 解析任意 Bearer challenge，并验证标准参数可规范写回。 */
static void __xrtHttpAuthFuzzBearerChallenge(xstrview Input)
{
	char Decoded[XRT_HTTP_AUTH_FUZZ_COPY_MAX];
	char Encoded[XRT_HTTP_AUTH_FUZZ_COPY_MAX];
	xhttpbearerchallenge Challenge;
	size_t iRequired;
	size_t iWritten;

	if ( !xrtHttpBearerChallengeRead(
		Input, NULL, 0, &iRequired, &Challenge
	) ) {
		xrtClearError();
		return;
	}
	if ( iRequired > sizeof(Decoded) ) {
		return;
	}
	if ( !xrtHttpBearerChallengeRead(
		Input,
		Decoded,
		sizeof(Decoded),
		&iWritten,
		&Challenge
	) || (iWritten != iRequired) ||
		!__xrtHttpAuthFuzzBearerChallengeViews(
			(xstrview){ Decoded, iWritten }, &Challenge
		) ) {
		abort();
	}
	if ( Challenge.Flags == 0 ) {
		xrtClearError();
		return;
	}
	if ( !xrtHttpBearerChallengeWrite(
		&Challenge, NULL, 0, &iRequired
	) ) {
		abort();
	}
	if ( iRequired <= sizeof(Encoded) ) {
		if ( !xrtHttpBearerChallengeWrite(
			&Challenge,
			Encoded,
			sizeof(Encoded),
			&iWritten
		) || (iWritten != iRequired) ) {
			abort();
		}
	}
	xrtClearError();
}

#endif



#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_CHALLENGE)

/* 检查 Digest challenge 的解码视图与算法名借用范围。 */
static bool __xrtHttpAuthFuzzDigestChallengeViews(
	xstrview Input,
	xstrview Output,
	const xhttpdigestchallenge* pChallenge
)
{
	if ( !__xrtHttpAuthFuzzViewInside(Output, pChallenge->Realm) ||
		!__xrtHttpAuthFuzzViewInside(Output, pChallenge->Domain) ||
		!__xrtHttpAuthFuzzViewInside(Output, pChallenge->Nonce) ||
		!__xrtHttpAuthFuzzViewInside(Output, pChallenge->Opaque) ) {
		return false;
	}
	if ( pChallenge->Algorithm == XHTTP_DIGEST_ALGORITHM_UNKNOWN ) {
		return __xrtHttpAuthFuzzViewInside(
			Input, pChallenge->AlgorithmName
		) && xrtHttpTokenValid(pChallenge->AlgorithmName);
	}
	return xrtHttpTokenEqual(
		pChallenge->AlgorithmName,
		xrtHttpDigestAlgorithmName(pChallenge->Algorithm)
	);
}



/* 解析任意 Digest challenge，并验证结构化规范写回。 */
static void __xrtHttpAuthFuzzDigestChallenge(xstrview Input)
{
	char Decoded[XRT_HTTP_AUTH_FUZZ_COPY_MAX];
	char Encoded[XRT_HTTP_AUTH_FUZZ_COPY_MAX];
	char AgainOutput[XRT_HTTP_AUTH_FUZZ_COPY_MAX];
	xhttpdigestchallenge Challenge;
	xhttpdigestchallenge Again;
	size_t iRequired;
	size_t iWritten;
	size_t iAgain;

	if ( !xrtHttpDigestChallengeRead(
		Input, NULL, 0, &iRequired, &Challenge
	) ) {
		xrtClearError();
		return;
	}
	if ( iRequired > sizeof(Decoded) ) {
		return;
	}
	if ( !xrtHttpDigestChallengeRead(
		Input,
		Decoded,
		sizeof(Decoded),
		&iWritten,
		&Challenge
	) || (iWritten != iRequired) ||
		!__xrtHttpAuthFuzzDigestChallengeViews(
			Input,
			(xstrview){ Decoded, iWritten },
			&Challenge
		) || !xrtHttpDigestChallengeWrite(
			&Challenge, NULL, 0, &iRequired
		) ) {
		abort();
	}
	if ( iRequired <= sizeof(Encoded) ) {
		if ( !xrtHttpDigestChallengeWrite(
			&Challenge,
			Encoded,
			sizeof(Encoded),
			&iWritten
		) || (iWritten != iRequired) ||
			!xrtHttpDigestChallengeRead(
				(xstrview){ Encoded, iWritten },
				AgainOutput,
				sizeof(AgainOutput),
				&iAgain,
				&Again
			) ) {
			abort();
		}
	}
	xrtClearError();
}

#endif



#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_CREDENTIALS)

/* 检查 Digest 凭据的解码视图与未知算法名借用范围。 */
static bool __xrtHttpAuthFuzzDigestAuthViews(
	xstrview Input,
	xstrview Output,
	const xhttpdigestauth* pDigest
)
{
	if ( !__xrtHttpAuthFuzzViewInside(Output, pDigest->Username) ||
		!__xrtHttpAuthFuzzViewInside(
			Output, pDigest->UsernameLanguage
		) || !__xrtHttpAuthFuzzViewInside(Output, pDigest->Realm) ||
		!__xrtHttpAuthFuzzViewInside(Output, pDigest->Nonce) ||
		!__xrtHttpAuthFuzzViewInside(Output, pDigest->Uri) ||
		!__xrtHttpAuthFuzzViewInside(Output, pDigest->Cnonce) ||
		!__xrtHttpAuthFuzzViewInside(Output, pDigest->Response) ||
		!__xrtHttpAuthFuzzViewInside(Output, pDigest->Opaque) ) {
		return false;
	}
	if ( pDigest->Algorithm == XHTTP_DIGEST_ALGORITHM_UNKNOWN ) {
		return __xrtHttpAuthFuzzViewInside(
			Input, pDigest->AlgorithmName
		) && xrtHttpTokenValid(pDigest->AlgorithmName);
	}
	return xrtHttpTokenEqual(
		pDigest->AlgorithmName,
		xrtHttpDigestAlgorithmName(pDigest->Algorithm)
	);
}



/* 解析任意 Digest Authorization，并验证结构化规范写回。 */
static void __xrtHttpAuthFuzzDigestCredentials(xstrview Input)
{
	char Decoded[XRT_HTTP_AUTH_FUZZ_COPY_MAX];
	char Encoded[XRT_HTTP_AUTH_FUZZ_COPY_MAX];
	char AgainOutput[XRT_HTTP_AUTH_FUZZ_COPY_MAX];
	xhttpdigestauth Digest;
	xhttpdigestauth Again;
	size_t iRequired;
	size_t iWritten;
	size_t iAgain;

	if ( !xrtHttpDigestAuthRead(
		Input, NULL, 0, &iRequired, &Digest
	) ) {
		xrtClearError();
		return;
	}
	if ( iRequired > sizeof(Decoded) ) {
		return;
	}
	if ( !xrtHttpDigestAuthRead(
		Input,
		Decoded,
		sizeof(Decoded),
		&iWritten,
		&Digest
	) || (iWritten != iRequired) ||
		!__xrtHttpAuthFuzzDigestAuthViews(
			Input,
			(xstrview){ Decoded, iWritten },
			&Digest
		) || !xrtHttpDigestAuthWrite(
			&Digest, NULL, 0, &iRequired
		) ) {
		abort();
	}
	if ( iRequired <= sizeof(Encoded) ) {
		if ( !xrtHttpDigestAuthWrite(
			&Digest,
			Encoded,
			sizeof(Encoded),
			&iWritten
		) || (iWritten != iRequired) || !xrtHttpDigestAuthRead(
			(xstrview){ Encoded, iWritten },
			AgainOutput,
			sizeof(AgainOutput),
			&iAgain,
			&Again
		) ) {
			abort();
		}
	}
	xrtClearError();
}

#endif



#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_INFO)

/* 解析指定算法下的 Authentication-Info，并验证规范写回。 */
static void __xrtHttpAuthFuzzDigestInfoAlgorithm(
	xstrview Input,
	xhttpdigestalgorithm Algorithm
)
{
	char Decoded[XRT_HTTP_AUTH_FUZZ_COPY_MAX];
	char Encoded[XRT_HTTP_AUTH_FUZZ_COPY_MAX];
	char AgainOutput[XRT_HTTP_AUTH_FUZZ_COPY_MAX];
	xhttpdigestinfo Info;
	xhttpdigestinfo Again;
	size_t iRequired;
	size_t iWritten;
	size_t iAgain;

	if ( !xrtHttpDigestInfoRead(
		Input, Algorithm, NULL, 0, &iRequired, &Info
	) ) {
		xrtClearError();
		return;
	}
	if ( iRequired > sizeof(Decoded) ) {
		return;
	}
	if ( !xrtHttpDigestInfoRead(
		Input,
		Algorithm,
		Decoded,
		sizeof(Decoded),
		&iWritten,
		&Info
	) || (iWritten != iRequired) ||
		!__xrtHttpAuthFuzzViewInside(
			(xstrview){ Decoded, iWritten }, Info.NextNonce
		) || !__xrtHttpAuthFuzzViewInside(
			(xstrview){ Decoded, iWritten }, Info.Response
		) || !__xrtHttpAuthFuzzViewInside(
			(xstrview){ Decoded, iWritten }, Info.Cnonce
		) || !xrtHttpDigestInfoWrite(
			&Info, NULL, 0, &iRequired
		) ) {
		abort();
	}
	if ( iRequired <= sizeof(Encoded) ) {
		if ( !xrtHttpDigestInfoWrite(
			&Info,
			Encoded,
			sizeof(Encoded),
			&iWritten
		) || (iWritten != iRequired) || !xrtHttpDigestInfoRead(
			(xstrview){ Encoded, iWritten },
			Algorithm,
			AgainOutput,
			sizeof(AgainOutput),
			&iAgain,
			&Again
		) ) {
			abort();
		}
	}
	xrtClearError();
}



/* 分别覆盖 32 位与 64 位十六进制摘要语法。 */
static void __xrtHttpAuthFuzzDigestInfo(xstrview Input)
{
	__xrtHttpAuthFuzzDigestInfoAlgorithm(
		Input, XHTTP_DIGEST_ALGORITHM_MD5
	);
	__xrtHttpAuthFuzzDigestInfoAlgorithm(
		Input, XHTTP_DIGEST_ALGORITHM_SHA256
	);
}

#endif



/* 统一公开确定性回归和 libFuzzer 使用的认证协议入口。 */
int xrtHttpAuthFuzzerTestOneInput(const uint8* pData, size_t iSize)
{
	xstrview Input;

	if ( ((pData == NULL) && (iSize != 0u)) ||
		(iSize > XRT_HTTP_AUTH_FUZZ_INPUT_MAX) ) {
		return 0;
	}
	Input = (xstrview){ (cstr)pData, iSize };
	__xrtHttpAuthFuzzChallenges(Input);
	__xrtHttpAuthFuzzCredentials(Input);
	#if defined(XRT_FEATURE_HTTP_AUTH_BASIC)
		__xrtHttpAuthFuzzBasic(Input);
		if ( iSize <= XRT_HTTP_AUTH_FUZZ_COPY_MAX ) {
			__xrtHttpAuthFuzzBasicChallenge(Input);
		}
	#endif
	#if defined(XRT_FEATURE_HTTP_AUTH_BEARER)
		__xrtHttpAuthFuzzBearer(Input);
	#endif
	#if defined(XRT_FEATURE_HTTP_AUTH_BEARER_CHALLENGE)
		if ( iSize <= XRT_HTTP_AUTH_FUZZ_COPY_MAX ) {
			__xrtHttpAuthFuzzBearerChallenge(Input);
		}
	#endif
	#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_CHALLENGE)
		if ( iSize <= XRT_HTTP_AUTH_FUZZ_COPY_MAX ) {
			__xrtHttpAuthFuzzDigestChallenge(Input);
		}
	#endif
	#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_CREDENTIALS)
		if ( iSize <= XRT_HTTP_AUTH_FUZZ_COPY_MAX ) {
			__xrtHttpAuthFuzzDigestCredentials(Input);
		}
	#endif
	#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_INFO)
		if ( iSize <= XRT_HTTP_AUTH_FUZZ_COPY_MAX ) {
			__xrtHttpAuthFuzzDigestInfo(Input);
		}
	#endif
	return 0;
}



#if defined(XRT_HTTP_AUTH_FUZZ_LIBFUZZER)

/* 把独立认证入口适配为 Clang/libFuzzer 约定符号。 */
int LLVMFuzzerTestOneInput(const uint8* pData, size_t iSize)
{
	return xrtHttpAuthFuzzerTestOneInput(pData, iSize);
}

#endif
