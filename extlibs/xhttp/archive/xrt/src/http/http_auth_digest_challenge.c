#include "../internal/xrt_http.h"

#include <xrt/http_auth.h>



#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_CHALLENGE)

#define XRT_HTTP_DIGEST_CHALLENGE_REALM 0u
#define XRT_HTTP_DIGEST_CHALLENGE_DOMAIN 1u
#define XRT_HTTP_DIGEST_CHALLENGE_NONCE 2u
#define XRT_HTTP_DIGEST_CHALLENGE_OPAQUE 3u
#define XRT_HTTP_DIGEST_CHALLENGE_STALE 4u
#define XRT_HTTP_DIGEST_CHALLENGE_ALGORITHM 5u
#define XRT_HTTP_DIGEST_CHALLENGE_QOP 6u
#define XRT_HTTP_DIGEST_CHALLENGE_CHARSET 7u
#define XRT_HTTP_DIGEST_CHALLENGE_USERHASH 8u
#define XRT_HTTP_DIGEST_CHALLENGE_PARAMETERS 9u

#define XRT_HTTP_DIGEST_CHALLENGE_VALID_FLAGS \
	((uint32)XHTTP_DIGEST_CHALLENGE_HAS_DOMAIN | \
	 (uint32)XHTTP_DIGEST_CHALLENGE_HAS_OPAQUE | \
	 (uint32)XHTTP_DIGEST_CHALLENGE_HAS_STALE | \
	 (uint32)XHTTP_DIGEST_CHALLENGE_STALE | \
	 (uint32)XHTTP_DIGEST_CHALLENGE_UTF8 | \
	 (uint32)XHTTP_DIGEST_CHALLENGE_HAS_USERHASH | \
	 (uint32)XHTTP_DIGEST_CHALLENGE_USERHASH | \
	 (uint32)XHTTP_DIGEST_CHALLENGE_QOP_AUTH | \
	 (uint32)XHTTP_DIGEST_CHALLENGE_QOP_AUTH_INT | \
	 (uint32)XHTTP_DIGEST_CHALLENGE_ALGORITHM_EXPLICIT)



static const xstrview __xrtHttpDigestChallengeNames[
	XRT_HTTP_DIGEST_CHALLENGE_PARAMETERS
] = {
	XRT_STR_INIT("realm"),
	XRT_STR_INIT("domain"),
	XRT_STR_INIT("nonce"),
	XRT_STR_INIT("opaque"),
	XRT_STR_INIT("stale"),
	XRT_STR_INIT("algorithm"),
	XRT_STR_INIT("qop"),
	XRT_STR_INIT("charset"),
	XRT_STR_INIT("userhash")
};



/* 查找 Digest challenge 标准参数索引。 */
static size_t __xrtHttpDigestChallengeNameIndex(xstrview Name)
{
	for ( size_t i = 0;
		i < XRT_HTTP_DIGEST_CHALLENGE_PARAMETERS;
		i++ ) {
		if ( xrtHttpTokenEqual(
			Name, __xrtHttpDigestChallengeNames[i]
		) ) {
			return i;
		}
	}
	return XRT_HTTP_DIGEST_CHALLENGE_PARAMETERS;
}



/* 检查读取 API 的固定描述符和所有输入输出区间。 */
static bool __xrtHttpDigestChallengeReadValid(
	xstrview Value,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestchallenge* pChallenge
)
{
	if ( !__xrtHttpViewValid(Value) ||
		!__xrtRangeValid(pSize, sizeof(size_t)) ||
		!__xrtRangeValid(pChallenge, sizeof(*pChallenge)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		((pOutput != NULL) && !__xrtRangeValid(
			pOutput, iCapacity
		)) || __xrtRangesOverlap(
			pSize, sizeof(size_t), Value.Data, Value.Size
		) || __xrtRangesOverlap(
			pChallenge, sizeof(*pChallenge), Value.Data, Value.Size
		) || __xrtRangesOverlap(
			pSize, sizeof(size_t), pChallenge, sizeof(*pChallenge)
		) || ((pOutput != NULL) && (__xrtRangesOverlap(
			pOutput, iCapacity, Value.Data, Value.Size
		) || __xrtRangesOverlap(
			pOutput, iCapacity, pSize, sizeof(size_t)
		) || __xrtRangesOverlap(
			pOutput, iCapacity, pChallenge, sizeof(*pChallenge)
		))) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 查看参数的下一个语义字节而不推进游标。 */
static bool __xrtHttpDigestSemanticPeek(
	const xhttpparam* pParam,
	size_t iOffset,
	uint8* pByte
)
{
	return __xrtHttpParamSemanticNext(pParam, &iOffset, pByte);
}



/* 解析 quoted qop token-list，并保留当前实现认识的能力位。 */
static bool __xrtHttpDigestChallengeQop(
	const xhttpparam* pParam,
	uint32* pFlags
)
{
	size_t iOffset = 0;
	size_t iTokens = 0;
	uint8 iByte;

	if ( (pParam->Flags & XHTTP_PARAM_QUOTED) == 0 ) {
		return false;
	}
	while ( iOffset < pParam->Value.Size ) {
		size_t iLength = 0;
		bool bAuth = true;
		bool bAuthInt = true;

		while ( __xrtHttpDigestSemanticPeek(
			pParam, iOffset, &iByte
		) && ((iByte == (uint8)' ') ||
			(iByte == (uint8)'\t') ||
			(iByte == (uint8)',')) ) {
			(void)__xrtHttpParamSemanticNext(
				pParam, &iOffset, &iByte
			);
		}
		if ( iOffset == pParam->Value.Size ) {
			break;
		}
		while ( __xrtHttpDigestSemanticPeek(
			pParam, iOffset, &iByte
		) && __xrtHttpTokenByte(iByte) ) {
			static const char Auth[] = "auth";
			static const char AuthInt[] = "auth-int";

			(void)__xrtHttpParamSemanticNext(
				pParam, &iOffset, &iByte
			);
			bAuth = bAuth && (iLength < (sizeof(Auth) - 1u)) &&
				(__xrtHttpAsciiLower(iByte) ==
				 (uint8)Auth[iLength]);
			bAuthInt = bAuthInt &&
				(iLength < (sizeof(AuthInt) - 1u)) &&
				(__xrtHttpAsciiLower(iByte) ==
				 (uint8)AuthInt[iLength]);
			iLength++;
		}
		if ( iLength == 0 ) {
			return false;
		}
		if ( bAuth && (iLength == 4u) ) {
			*pFlags |= XHTTP_DIGEST_CHALLENGE_QOP_AUTH;
		}
		if ( bAuthInt && (iLength == 8u) ) {
			*pFlags |= XHTTP_DIGEST_CHALLENGE_QOP_AUTH_INT;
		}
		iTokens++;
		while ( __xrtHttpDigestSemanticPeek(
			pParam, iOffset, &iByte
		) && ((iByte == (uint8)' ') ||
			(iByte == (uint8)'\t')) ) {
			(void)__xrtHttpParamSemanticNext(
				pParam, &iOffset, &iByte
			);
		}
		if ( iOffset == pParam->Value.Size ) {
			break;
		}
		(void)__xrtHttpParamSemanticNext(
			pParam, &iOffset, &iByte
		);
		if ( iByte != (uint8)',' ) {
			return false;
		}
	}
	return iTokens != 0;
}



/* 解析固定 token 参数；quoted 形式在这些 RFC 参数中是不规范的。 */
static bool __xrtHttpDigestChallengeToken(
	const xhttpparam* pParam,
	xstrview Expected
)
{
	return ((pParam->Flags & XHTTP_PARAM_QUOTED) == 0) &&
		xrtHttpParamTokenEqual(pParam, Expected);
}



/* 解析并解码 RFC 7616 Digest challenge。 */
XRT_API bool xrtHttpDigestChallengeRead(
	xstrview Value,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestchallenge* pChallenge
)
{
	xhttpdigestchallenge Challenge = { 0 };
	xhttpparam Parameters[XRT_HTTP_DIGEST_CHALLENGE_PARAMETERS] = { 0 };
	size_t Sizes[XRT_HTTP_DIGEST_CHALLENGE_PARAMETERS] = { 0 };
	xhttpauth Auth;
	xhttpparam Param;
	xhttpnext Next;
	size_t iParamOffset = 0;
	size_t iRequired = 0;
	uint32 iSeen = 0;

	if ( !__xrtHttpDigestChallengeReadValid(
		Value, pOutput, iCapacity, pSize, pChallenge
	) ) {
		return false;
	}
	memcpy(pChallenge, &Challenge, sizeof(Challenge));
	if ( !xrtHttpAuthParse(Value, &Auth) ) {
		return false;
	}
	if ( !xrtHttpTokenEqual(
		Auth.Scheme, XRT_STR_LITERAL("Digest")
	) || (Auth.Kind != XHTTP_AUTH_PARAMS) ) {
		__xrtErrorSetValue();
		return false;
	}
	for ( ;; ) {
		size_t iIndex;
		uint32 iBit;

		Next = xrtHttpAuthParamNext(
			Auth.Data, &iParamOffset, &Param
		);
		if ( Next == XHTTP_NEXT_END ) {
			break;
		}
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		iIndex = __xrtHttpDigestChallengeNameIndex(Param.Name);
		if ( iIndex == XRT_HTTP_DIGEST_CHALLENGE_PARAMETERS ) {
			continue;
		}
		iBit = UINT32_C(1) << (uint32)iIndex;
		if ( (iSeen & iBit) != 0 ) {
			__xrtErrorSetValue();
			return false;
		}
		iSeen |= iBit;

		if ( iIndex <= XRT_HTTP_DIGEST_CHALLENGE_OPAQUE ) {
			if ( (Param.Flags & XHTTP_PARAM_QUOTED) == 0 ) {
				__xrtErrorSetValue();
				return false;
			}
			if ( !xrtHttpParamValueWrite(
				&Param, NULL, 0, &Sizes[iIndex]
			) ) {
				return false;
			}
			if ( !__xrtHttpSizeAdd(
				&iRequired, Sizes[iIndex]
			) ) {
				return false;
			}
			Parameters[iIndex] = Param;
			continue;
		}
		if ( iIndex == XRT_HTTP_DIGEST_CHALLENGE_STALE ) {
			Challenge.Flags |= XHTTP_DIGEST_CHALLENGE_HAS_STALE;
			if ( __xrtHttpDigestChallengeToken(
				&Param, XRT_STR_LITERAL("true")
			) ) {
				Challenge.Flags |= XHTTP_DIGEST_CHALLENGE_STALE;
			} else if ( !__xrtHttpDigestChallengeToken(
				&Param, XRT_STR_LITERAL("false")
			) ) {
				__xrtErrorSetValue();
				return false;
			}
			continue;
		}
		if ( iIndex == XRT_HTTP_DIGEST_CHALLENGE_ALGORITHM ) {
			if ( ((Param.Flags & XHTTP_PARAM_QUOTED) != 0) ||
				!xrtHttpParamTokenValid(&Param) ) {
				__xrtErrorSetValue();
				return false;
			}
			Challenge.Algorithm = xrtHttpDigestAlgorithmParse(
				Param.Value
			);
			Challenge.AlgorithmName =
				(Challenge.Algorithm == XHTTP_DIGEST_ALGORITHM_UNKNOWN) ?
				Param.Value : xrtHttpDigestAlgorithmName(
					Challenge.Algorithm
				);
			Challenge.Flags |=
				XHTTP_DIGEST_CHALLENGE_ALGORITHM_EXPLICIT;
			continue;
		}
		if ( iIndex == XRT_HTTP_DIGEST_CHALLENGE_QOP ) {
			if ( !__xrtHttpDigestChallengeQop(
				&Param, &Challenge.Flags
			) ) {
				__xrtErrorSetValue();
				return false;
			}
			continue;
		}
		if ( iIndex == XRT_HTTP_DIGEST_CHALLENGE_CHARSET ) {
			if ( !__xrtHttpDigestChallengeToken(
				&Param, XRT_STR_LITERAL("UTF-8")
			) ) {
				__xrtErrorSetValue();
				return false;
			}
			Challenge.Flags |= XHTTP_DIGEST_CHALLENGE_UTF8;
			continue;
		}
		Challenge.Flags |= XHTTP_DIGEST_CHALLENGE_HAS_USERHASH;
		if ( __xrtHttpDigestChallengeToken(
			&Param, XRT_STR_LITERAL("true")
		) ) {
			Challenge.Flags |= XHTTP_DIGEST_CHALLENGE_USERHASH;
		} else if ( !__xrtHttpDigestChallengeToken(
			&Param, XRT_STR_LITERAL("false")
		) ) {
			__xrtErrorSetValue();
			return false;
		}
	}
	if ( (iSeen & (UINT32_C(1) <<
		XRT_HTTP_DIGEST_CHALLENGE_REALM)) == 0 ||
		(iSeen & (UINT32_C(1) <<
		XRT_HTTP_DIGEST_CHALLENGE_NONCE)) == 0 ||
		(iSeen & (UINT32_C(1) <<
		XRT_HTTP_DIGEST_CHALLENGE_QOP)) == 0 ) {
		__xrtErrorSetValue();
		return false;
	}
	if ( (iSeen & (UINT32_C(1) <<
		XRT_HTTP_DIGEST_CHALLENGE_ALGORITHM)) == 0 ) {
		Challenge.Algorithm = XHTTP_DIGEST_ALGORITHM_MD5;
		Challenge.AlgorithmName = XRT_STR_LITERAL("MD5");
	}
	if ( (iSeen & (UINT32_C(1) <<
		XRT_HTTP_DIGEST_CHALLENGE_DOMAIN)) != 0 ) {
		Challenge.Flags |= XHTTP_DIGEST_CHALLENGE_HAS_DOMAIN;
	}
	if ( (iSeen & (UINT32_C(1) <<
		XRT_HTTP_DIGEST_CHALLENGE_OPAQUE)) != 0 ) {
		Challenge.Flags |= XHTTP_DIGEST_CHALLENGE_HAS_OPAQUE;
	}
	if ( pOutput == NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		memcpy(pChallenge, &Challenge, sizeof(Challenge));
		return true;
	}
	if ( !__xrtRangeValid(pOutput, iRequired) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iCapacity < iRequired ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		__xrtErrorSetRange();
		return false;
	}
	{
		size_t iOutput = 0;
		xstrview* pTargets[4] = {
			&Challenge.Realm,
			&Challenge.Domain,
			&Challenge.Nonce,
			&Challenge.Opaque
		};

		for ( size_t i = 0; i < 4u; i++ ) {
			size_t iWritten;

			if ( (iSeen & (UINT32_C(1) << (uint32)i)) == 0 ) {
				continue;
			}
			iWritten = __xrtHttpParamValueWriteUnchecked(
				&Parameters[i], (bytes)pOutput + iOutput
			);
			*pTargets[i] = (xstrview){
				(cstr)pOutput + iOutput, iWritten
			};
			iOutput += iWritten;
		}
	}
	memcpy(pSize, &iRequired, sizeof(iRequired));
	memcpy(pChallenge, &Challenge, sizeof(Challenge));
	return true;
}



/* 按固定顺序执行一次 challenge 测量或写出。 */
static bool __xrtHttpDigestChallengeWritePass(
	const xhttpdigestchallenge* pChallenge,
	xstrview Algorithm,
	xrt_http_param_writer* pWriter
)
{
	char Qop[14];
	size_t iQop = 0;

	if ( !__xrtHttpParamWriterBytes(pWriter, "Digest ", 7u) ||
		!__xrtHttpParamWriterQuoted(
			pWriter, XRT_STR_LITERAL("realm"),
			pChallenge->Realm, true
		) ) {
		return false;
	}
	if ( ((pChallenge->Flags &
		XHTTP_DIGEST_CHALLENGE_HAS_DOMAIN) != 0) &&
		!__xrtHttpParamWriterQuoted(
			pWriter, XRT_STR_LITERAL("domain"),
			pChallenge->Domain, false
		) ) {
		return false;
	}
	if ( !__xrtHttpParamWriterQuoted(
		pWriter, XRT_STR_LITERAL("nonce"),
		pChallenge->Nonce, false
	) ) {
		return false;
	}
	if ( ((pChallenge->Flags &
		XHTTP_DIGEST_CHALLENGE_HAS_OPAQUE) != 0) &&
		!__xrtHttpParamWriterQuoted(
			pWriter, XRT_STR_LITERAL("opaque"),
			pChallenge->Opaque, false
		) ) {
		return false;
	}
	if ( (pChallenge->Flags &
		XHTTP_DIGEST_CHALLENGE_HAS_STALE) != 0 ) {
		xstrview Stale = ((pChallenge->Flags &
			XHTTP_DIGEST_CHALLENGE_STALE) != 0) ?
			XRT_STR_LITERAL("true") : XRT_STR_LITERAL("false");

		if ( !__xrtHttpParamWriterToken(
			pWriter, XRT_STR_LITERAL("stale"), Stale, false
		) ) {
			return false;
		}
	}
	if ( ((pChallenge->Flags &
		XHTTP_DIGEST_CHALLENGE_ALGORITHM_EXPLICIT) != 0) &&
		!__xrtHttpParamWriterToken(
			pWriter, XRT_STR_LITERAL("algorithm"),
			Algorithm, false
		) ) {
		return false;
	}
	if ( (pChallenge->Flags &
		XHTTP_DIGEST_CHALLENGE_QOP_AUTH) != 0 ) {
		memcpy(Qop + iQop, "auth", 4u);
		iQop += 4u;
	}
	if ( (pChallenge->Flags &
		XHTTP_DIGEST_CHALLENGE_QOP_AUTH_INT) != 0 ) {
		if ( iQop != 0 ) {
			memcpy(Qop + iQop, ", auth-int", 10u);
			iQop += 10u;
		} else {
			memcpy(Qop + iQop, "auth-int", 8u);
			iQop += 8u;
		}
	}
	if ( !__xrtHttpParamWriterQuoted(
		pWriter, XRT_STR_LITERAL("qop"),
		(xstrview){ Qop, iQop }, false
	) ) {
		return false;
	}
	if ( ((pChallenge->Flags &
		XHTTP_DIGEST_CHALLENGE_UTF8) != 0) &&
		!__xrtHttpParamWriterToken(
			pWriter, XRT_STR_LITERAL("charset"),
			XRT_STR_LITERAL("UTF-8"), false
		) ) {
		return false;
	}
	if ( (pChallenge->Flags &
		XHTTP_DIGEST_CHALLENGE_HAS_USERHASH) != 0 ) {
		xstrview Userhash = ((pChallenge->Flags &
			XHTTP_DIGEST_CHALLENGE_USERHASH) != 0) ?
			XRT_STR_LITERAL("true") : XRT_STR_LITERAL("false");

		if ( !__xrtHttpParamWriterToken(
			pWriter, XRT_STR_LITERAL("userhash"),
			Userhash, false
		) ) {
			return false;
		}
	}
	return true;
}



/* 验证 challenge 描述符并选择算法线路名称。 */
static bool __xrtHttpDigestChallengeWriteValid(
	const xhttpdigestchallenge* pInput,
	xhttpdigestchallenge* pChallenge,
	xstrview* pAlgorithm,
	size_t* pSize
)
{
	xstrview Canonical;
	uint32 iFlags;

	if ( !__xrtRangeValid(pInput, sizeof(*pInput)) ||
		!__xrtRangeValid(pSize, sizeof(size_t)) ||
		__xrtRangesOverlap(
			pInput, sizeof(*pInput), pSize, sizeof(size_t)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pChallenge, pInput, sizeof(*pChallenge));
	iFlags = pChallenge->Flags;
	if ( ((iFlags & ~XRT_HTTP_DIGEST_CHALLENGE_VALID_FLAGS) != 0) ||
		((iFlags & (XHTTP_DIGEST_CHALLENGE_QOP_AUTH |
		 XHTTP_DIGEST_CHALLENGE_QOP_AUTH_INT)) == 0) ||
		(((iFlags & XHTTP_DIGEST_CHALLENGE_STALE) != 0) &&
		 ((iFlags & XHTTP_DIGEST_CHALLENGE_HAS_STALE) == 0)) ||
		(((iFlags & XHTTP_DIGEST_CHALLENGE_USERHASH) != 0) &&
		 ((iFlags & XHTTP_DIGEST_CHALLENGE_HAS_USERHASH) == 0)) ||
		!__xrtHttpViewValid(pChallenge->Realm) ||
		!__xrtHttpViewValid(pChallenge->Domain) ||
		!__xrtHttpViewValid(pChallenge->Nonce) ||
		!__xrtHttpViewValid(pChallenge->Opaque) ||
		!__xrtHttpViewValid(pChallenge->AlgorithmName) ||
		(((iFlags & XHTTP_DIGEST_CHALLENGE_HAS_DOMAIN) == 0) &&
		 (pChallenge->Domain.Size != 0)) ||
		(((iFlags & XHTTP_DIGEST_CHALLENGE_HAS_OPAQUE) == 0) &&
		 (pChallenge->Opaque.Size != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	{
		xstrview Views[5] = {
			pChallenge->Realm,
			pChallenge->Domain,
			pChallenge->Nonce,
			pChallenge->Opaque,
			pChallenge->AlgorithmName
		};

		for ( size_t i = 0; i < 5u; i++ ) {
			if ( __xrtRangesOverlap(
				pInput, sizeof(*pInput),
				Views[i].Data, Views[i].Size
			) || __xrtRangesOverlap(
				pSize, sizeof(size_t),
				Views[i].Data, Views[i].Size
			) ) {
				__xrtErrorSetInvalidArgument();
				return false;
			}
		}
	}
	Canonical = xrtHttpDigestAlgorithmName(pChallenge->Algorithm);
	if ( (pChallenge->Algorithm !=
		XHTTP_DIGEST_ALGORITHM_UNKNOWN) &&
		(Canonical.Size == 0) ) {
		__xrtErrorSetValue();
		return false;
	}
	if ( (iFlags &
		XHTTP_DIGEST_CHALLENGE_ALGORITHM_EXPLICIT) == 0 ) {
		if ( (pChallenge->Algorithm != XHTTP_DIGEST_ALGORITHM_MD5) ||
			((pChallenge->AlgorithmName.Size != 0) &&
			 !xrtHttpTokenEqual(
				pChallenge->AlgorithmName,
				XRT_STR_LITERAL("MD5")
			 )) ) {
			__xrtErrorSetValue();
			return false;
		}
		*pAlgorithm = XRT_STR_LITERAL("MD5");
		return true;
	}
	if ( pChallenge->Algorithm == XHTTP_DIGEST_ALGORITHM_UNKNOWN ) {
		if ( !xrtHttpTokenValid(pChallenge->AlgorithmName) ) {
			__xrtErrorSetValue();
			return false;
		}
		*pAlgorithm = pChallenge->AlgorithmName;
		return true;
	}
	if ( (pChallenge->AlgorithmName.Size != 0) &&
		!xrtHttpTokenEqual(
			pChallenge->AlgorithmName, Canonical
		) ) {
		__xrtErrorSetValue();
		return false;
	}
	*pAlgorithm = Canonical;
	return true;
}



/* 按规范顺序写出 Digest challenge。 */
XRT_API bool xrtHttpDigestChallengeWrite(
	const xhttpdigestchallenge* pInput,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xhttpdigestchallenge Challenge;
	xstrview Algorithm;
	xstrview Views[5];
	xrt_http_param_writer Writer = { 0 };

	if ( ((pOutput == NULL) && (iCapacity != 0)) ||
		!__xrtHttpDigestChallengeWriteValid(
			pInput, &Challenge, &Algorithm, pSize
		) || !__xrtHttpDigestChallengeWritePass(
			&Challenge, Algorithm, &Writer
		) ) {
		if ( (pOutput == NULL) && (iCapacity != 0) ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	Views[0] = Challenge.Realm;
	Views[1] = Challenge.Domain;
	Views[2] = Challenge.Nonce;
	Views[3] = Challenge.Opaque;
	Views[4] = Challenge.AlgorithmName;
	if ( pOutput == NULL ) {
		memcpy(pSize, &Writer.Size, sizeof(Writer.Size));
		return true;
	}
	if ( !__xrtRangeValid(pOutput, Writer.Size) ||
		__xrtRangesOverlap(
			pOutput, Writer.Size, pInput, sizeof(*pInput)
		) || __xrtRangesOverlap(
			pOutput, Writer.Size, pSize, sizeof(size_t)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	for ( size_t i = 0; i < 5u; i++ ) {
		if ( __xrtRangesOverlap(
			pOutput, Writer.Size, Views[i].Data, Views[i].Size
		) ) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
	}
	if ( iCapacity < Writer.Size ) {
		memcpy(pSize, &Writer.Size, sizeof(Writer.Size));
		__xrtErrorSetRange();
		return false;
	}
	Writer.Output = (bytes)pOutput;
	Writer.Size = 0;
	if ( !__xrtHttpDigestChallengeWritePass(
		&Challenge, Algorithm, &Writer
	) ) {
		return false;
	}
	memcpy(pSize, &Writer.Size, sizeof(Writer.Size));
	return true;
}



#undef XRT_HTTP_DIGEST_CHALLENGE_REALM
#undef XRT_HTTP_DIGEST_CHALLENGE_DOMAIN
#undef XRT_HTTP_DIGEST_CHALLENGE_NONCE
#undef XRT_HTTP_DIGEST_CHALLENGE_OPAQUE
#undef XRT_HTTP_DIGEST_CHALLENGE_STALE
#undef XRT_HTTP_DIGEST_CHALLENGE_ALGORITHM
#undef XRT_HTTP_DIGEST_CHALLENGE_QOP
#undef XRT_HTTP_DIGEST_CHALLENGE_CHARSET
#undef XRT_HTTP_DIGEST_CHALLENGE_USERHASH
#undef XRT_HTTP_DIGEST_CHALLENGE_PARAMETERS
#undef XRT_HTTP_DIGEST_CHALLENGE_VALID_FLAGS

#endif
