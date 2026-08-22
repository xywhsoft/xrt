#include "../internal/xrt_http.h"

#include <xrt/http_auth.h>



#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_CREDENTIALS)

#define XRT_HTTP_DIGEST_AUTH_USERNAME 0u
#define XRT_HTTP_DIGEST_AUTH_USERNAME_EXT 1u
#define XRT_HTTP_DIGEST_AUTH_REALM 2u
#define XRT_HTTP_DIGEST_AUTH_NONCE 3u
#define XRT_HTTP_DIGEST_AUTH_URI 4u
#define XRT_HTTP_DIGEST_AUTH_RESPONSE 5u
#define XRT_HTTP_DIGEST_AUTH_ALGORITHM 6u
#define XRT_HTTP_DIGEST_AUTH_CNONCE 7u
#define XRT_HTTP_DIGEST_AUTH_OPAQUE 8u
#define XRT_HTTP_DIGEST_AUTH_QOP 9u
#define XRT_HTTP_DIGEST_AUTH_NC 10u
#define XRT_HTTP_DIGEST_AUTH_USERHASH 11u
#define XRT_HTTP_DIGEST_AUTH_PARAMETERS 12u

#define XRT_HTTP_DIGEST_AUTH_REQUIRED \
	((UINT32_C(1) << XRT_HTTP_DIGEST_AUTH_REALM) | \
	 (UINT32_C(1) << XRT_HTTP_DIGEST_AUTH_NONCE) | \
	 (UINT32_C(1) << XRT_HTTP_DIGEST_AUTH_URI) | \
	 (UINT32_C(1) << XRT_HTTP_DIGEST_AUTH_RESPONSE) | \
	 (UINT32_C(1) << XRT_HTTP_DIGEST_AUTH_CNONCE) | \
	 (UINT32_C(1) << XRT_HTTP_DIGEST_AUTH_QOP) | \
	 (UINT32_C(1) << XRT_HTTP_DIGEST_AUTH_NC))

#define XRT_HTTP_DIGEST_AUTH_VALID_FLAGS \
	((uint32)XHTTP_DIGEST_AUTH_USERNAME_EXTENDED | \
	 (uint32)XHTTP_DIGEST_AUTH_HAS_OPAQUE | \
	 (uint32)XHTTP_DIGEST_AUTH_HAS_USERHASH | \
	 (uint32)XHTTP_DIGEST_AUTH_USERHASH | \
	 (uint32)XHTTP_DIGEST_AUTH_ALGORITHM_EXPLICIT)



static const xstrview __xrtHttpDigestAuthNames[
	XRT_HTTP_DIGEST_AUTH_PARAMETERS
] = {
	XRT_STR_INIT("username"),
	XRT_STR_INIT("username*"),
	XRT_STR_INIT("realm"),
	XRT_STR_INIT("nonce"),
	XRT_STR_INIT("uri"),
	XRT_STR_INIT("response"),
	XRT_STR_INIT("algorithm"),
	XRT_STR_INIT("cnonce"),
	XRT_STR_INIT("opaque"),
	XRT_STR_INIT("qop"),
	XRT_STR_INIT("nc"),
	XRT_STR_INIT("userhash")
};



/* 查找 Digest 凭据标准参数索引。 */
static size_t __xrtHttpDigestAuthNameIndex(xstrview Name)
{
	for ( size_t i = 0; i < XRT_HTTP_DIGEST_AUTH_PARAMETERS; i++ ) {
		if ( xrtHttpTokenEqual(Name, __xrtHttpDigestAuthNames[i]) ) {
			return i;
		}
	}
	return XRT_HTTP_DIGEST_AUTH_PARAMETERS;
}



/* 校验读取入口的固定描述符和所有输入输出区间。 */
static bool __xrtHttpDigestAuthReadValid(
	xstrview Value,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestauth* pDigest
)
{
	if ( !__xrtHttpViewValid(Value) ||
		!__xrtRangeValid(pSize, sizeof(size_t)) ||
		!__xrtRangeValid(pDigest, sizeof(*pDigest)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		((pOutput != NULL) && !__xrtRangeValid(
			pOutput, iCapacity
		)) || __xrtRangesOverlap(
			pSize, sizeof(size_t), Value.Data, Value.Size
		) || __xrtRangesOverlap(
			pDigest, sizeof(*pDigest), Value.Data, Value.Size
		) || __xrtRangesOverlap(
			pSize, sizeof(size_t), pDigest, sizeof(*pDigest)
		) || ((pOutput != NULL) && (__xrtRangesOverlap(
			pOutput, iCapacity, Value.Data, Value.Size
		) || __xrtRangesOverlap(
			pOutput, iCapacity, pSize, sizeof(size_t)
		) || __xrtRangesOverlap(
			pOutput, iCapacity, pDigest, sizeof(*pDigest)
		))) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 判断一个已测量十六进制值是否符合当前算法的摘要长度。 */
static bool __xrtHttpDigestAuthHexSizeValid(
	xhttpdigestalgorithm Algorithm,
	size_t iSize
)
{
	size_t iDigest = xrtHttpDigestSize(Algorithm);

	if ( iDigest == 0 ) {
		return (iSize != 0) && ((iSize & 1u) == 0);
	}
	return iSize == (iDigest * 2u);
}



/* 解析 Digest 的固定 token 布尔参数。 */
static bool __xrtHttpDigestAuthBoolean(
	const xhttpparam* pParam,
	bool* pValue
)
{
	if ( ((pParam->Flags & XHTTP_PARAM_QUOTED) == 0) &&
		xrtHttpParamTokenEqual(pParam, XRT_STR_LITERAL("true")) ) {
		*pValue = true;
		return true;
	}
	if ( ((pParam->Flags & XHTTP_PARAM_QUOTED) == 0) &&
		xrtHttpParamTokenEqual(pParam, XRT_STR_LITERAL("false")) ) {
		*pValue = false;
		return true;
	}
	return false;
}



/* 解析并测量 username*，但把百分号解码推迟到输出提交阶段。 */
static bool __xrtHttpDigestAuthExtendedMeasure(
	const xhttpparam* pParam,
	xhttpextvalue* pExtended,
	size_t* pUsername,
	size_t* pLanguage
)
{
	if ( (pParam->Flags & XHTTP_PARAM_QUOTED) != 0 ) {
		__xrtErrorSetValue();
		return false;
	}
	if ( !xrtHttpExtValueParse(pParam->Value, pExtended) ) {
		return false;
	}
	if ( !xrtHttpTokenEqual(
		pExtended->Charset, XRT_STR_LITERAL("UTF-8")
	) ) {
		__xrtErrorSetValue();
		return false;
	}
	if ( !xrtHttpExtValueRead(
		pExtended, NULL, 0, pUsername
	) ) {
		return false;
	}
	*pLanguage = pExtended->Language.Size;
	return true;
}



/* 严格解析完整 RFC 7616 Digest 凭据。 */
XRT_API bool xrtHttpDigestAuthRead(
	xstrview Value,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestauth* pDigest
)
{
	xhttpdigestauth Digest = { 0 };
	xhttpparam Parameters[XRT_HTTP_DIGEST_AUTH_PARAMETERS] = { 0 };
	size_t Sizes[XRT_HTTP_DIGEST_AUTH_PARAMETERS] = { 0 };
	xhttpextvalue Extended = { 0 };
	xhttpauth Auth;
	xhttpparam Param;
	xhttpnext Next;
	size_t iParamOffset = 0;
	size_t iRequired = 0;
	uint32 iSeen = 0;
	bool bBoolean;

	if ( !__xrtHttpDigestAuthReadValid(
		Value, pOutput, iCapacity, pSize, pDigest
	) ) {
		return false;
	}
	memcpy(pDigest, &Digest, sizeof(Digest));
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
		iIndex = __xrtHttpDigestAuthNameIndex(Param.Name);
		if ( iIndex == XRT_HTTP_DIGEST_AUTH_PARAMETERS ) {
			continue;
		}
		iBit = UINT32_C(1) << (uint32)iIndex;
		if ( (iSeen & iBit) != 0 ) {
			__xrtErrorSetValue();
			return false;
		}
		if ( ((iIndex == XRT_HTTP_DIGEST_AUTH_USERNAME) &&
			 ((iSeen & (UINT32_C(1) <<
			  XRT_HTTP_DIGEST_AUTH_USERNAME_EXT)) != 0)) ||
			((iIndex == XRT_HTTP_DIGEST_AUTH_USERNAME_EXT) &&
			 ((iSeen & (UINT32_C(1) <<
			  XRT_HTTP_DIGEST_AUTH_USERNAME)) != 0)) ) {
			__xrtErrorSetValue();
			return false;
		}
		iSeen |= iBit;

		if ( iIndex == XRT_HTTP_DIGEST_AUTH_USERNAME_EXT ) {
			if ( !__xrtHttpDigestAuthExtendedMeasure(
				&Param,
				&Extended,
				&Sizes[iIndex],
				&Sizes[XRT_HTTP_DIGEST_AUTH_USERNAME]
			) || !__xrtHttpSizeAdd(
				&iRequired, Sizes[iIndex]
			) || !__xrtHttpSizeAdd(
				&iRequired,
				Sizes[XRT_HTTP_DIGEST_AUTH_USERNAME]
			) ) {
				return false;
			}
			Parameters[iIndex] = Param;
			Digest.Flags |= XHTTP_DIGEST_AUTH_USERNAME_EXTENDED;
			continue;
		}
		if ( (iIndex == XRT_HTTP_DIGEST_AUTH_USERNAME) ||
			(iIndex == XRT_HTTP_DIGEST_AUTH_REALM) ||
			(iIndex == XRT_HTTP_DIGEST_AUTH_NONCE) ||
			(iIndex == XRT_HTTP_DIGEST_AUTH_URI) ||
			(iIndex == XRT_HTTP_DIGEST_AUTH_RESPONSE) ||
			(iIndex == XRT_HTTP_DIGEST_AUTH_CNONCE) ||
			(iIndex == XRT_HTTP_DIGEST_AUTH_OPAQUE) ) {
			if ( (Param.Flags & XHTTP_PARAM_QUOTED) == 0 ) {
				__xrtErrorSetValue();
				return false;
			}
			if ( iIndex == XRT_HTTP_DIGEST_AUTH_RESPONSE ) {
				if ( !__xrtHttpDigestHexParamValid(
					&Param, &Sizes[iIndex]
				) ) {
					__xrtErrorSetValue();
					return false;
				}
			} else if ( !xrtHttpParamValueWrite(
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
			if ( iIndex == XRT_HTTP_DIGEST_AUTH_OPAQUE ) {
				Digest.Flags |= XHTTP_DIGEST_AUTH_HAS_OPAQUE;
			}
			continue;
		}
		if ( iIndex == XRT_HTTP_DIGEST_AUTH_ALGORITHM ) {
			if ( ((Param.Flags & XHTTP_PARAM_QUOTED) != 0) ||
				!xrtHttpParamTokenValid(&Param) ) {
				__xrtErrorSetValue();
				return false;
			}
			Digest.Algorithm = xrtHttpDigestAlgorithmParse(Param.Value);
			Digest.AlgorithmName =
				(Digest.Algorithm == XHTTP_DIGEST_ALGORITHM_UNKNOWN) ?
				Param.Value : xrtHttpDigestAlgorithmName(Digest.Algorithm);
			Digest.Flags |= XHTTP_DIGEST_AUTH_ALGORITHM_EXPLICIT;
			continue;
		}
		if ( iIndex == XRT_HTTP_DIGEST_AUTH_QOP ) {
			if ( ((Param.Flags & XHTTP_PARAM_QUOTED) != 0) ||
				!xrtHttpParamTokenValid(&Param) ) {
				__xrtErrorSetValue();
				return false;
			}
			Digest.Qop = xrtHttpDigestQopParse(Param.Value);
			if ( Digest.Qop == XHTTP_DIGEST_QOP_NONE ) {
				__xrtErrorSetValue();
				return false;
			}
			continue;
		}
		if ( iIndex == XRT_HTTP_DIGEST_AUTH_NC ) {
			if ( !__xrtHttpDigestNonceCountRead(
				&Param, &Digest.NonceCount
			) || (Digest.NonceCount == 0) ) {
				__xrtErrorSetValue();
				return false;
			}
			continue;
		}
		if ( !__xrtHttpDigestAuthBoolean(&Param, &bBoolean) ) {
			__xrtErrorSetValue();
			return false;
		}
		Digest.Flags |= XHTTP_DIGEST_AUTH_HAS_USERHASH;
		if ( bBoolean ) {
			Digest.Flags |= XHTTP_DIGEST_AUTH_USERHASH;
		}
	}
	if ( ((iSeen & XRT_HTTP_DIGEST_AUTH_REQUIRED) !=
		XRT_HTTP_DIGEST_AUTH_REQUIRED) ||
		((iSeen & ((UINT32_C(1) << XRT_HTTP_DIGEST_AUTH_USERNAME) |
		 (UINT32_C(1) << XRT_HTTP_DIGEST_AUTH_USERNAME_EXT))) == 0) ) {
		__xrtErrorSetValue();
		return false;
	}
	if ( (iSeen & (UINT32_C(1) <<
		XRT_HTTP_DIGEST_AUTH_ALGORITHM)) == 0 ) {
		Digest.Algorithm = XHTTP_DIGEST_ALGORITHM_MD5;
		Digest.AlgorithmName = XRT_STR_LITERAL("MD5");
	}
	if ( !__xrtHttpDigestAuthHexSizeValid(
		Digest.Algorithm,
		Sizes[XRT_HTTP_DIGEST_AUTH_RESPONSE]
	) ) {
		__xrtErrorSetValue();
		return false;
	}
	if ( (Digest.Flags & XHTTP_DIGEST_AUTH_USERHASH) != 0 ) {
		size_t iUsernameHash;

		if ( ((Digest.Flags &
			XHTTP_DIGEST_AUTH_USERNAME_EXTENDED) != 0) ||
			!__xrtHttpDigestHexParamValid(
				&Parameters[XRT_HTTP_DIGEST_AUTH_USERNAME],
				&iUsernameHash
			) || !__xrtHttpDigestAuthHexSizeValid(
				Digest.Algorithm, iUsernameHash
			) ) {
			__xrtErrorSetValue();
			return false;
		}
	}
	if ( pOutput == NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		memcpy(pDigest, &Digest, sizeof(Digest));
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
		size_t iWritten;

		if ( (Digest.Flags &
			XHTTP_DIGEST_AUTH_USERNAME_EXTENDED) != 0 ) {
			if ( !xrtHttpExtValueRead(
				&Extended,
				(bytes)pOutput + iOutput,
				iRequired - iOutput,
				&iWritten
			) ) {
				return false;
			}
			Digest.Username = (xstrview){
				(cstr)pOutput + iOutput, iWritten
			};
			iOutput += iWritten;
			Digest.UsernameLanguage = (xstrview){
				(cstr)pOutput + iOutput, Extended.Language.Size
			};
			if ( Extended.Language.Size != 0 ) {
				memcpy(
					(bytes)pOutput + iOutput,
					Extended.Language.Data,
					Extended.Language.Size
				);
			}
			iOutput += Extended.Language.Size;
		} else {
			iWritten = __xrtHttpParamValueWriteUnchecked(
				&Parameters[XRT_HTTP_DIGEST_AUTH_USERNAME],
				(bytes)pOutput + iOutput
			);
			Digest.Username = (xstrview){
				(cstr)pOutput + iOutput, iWritten
			};
			iOutput += iWritten;
		}
		{
			static const size_t Indices[] = {
				XRT_HTTP_DIGEST_AUTH_REALM,
				XRT_HTTP_DIGEST_AUTH_NONCE,
				XRT_HTTP_DIGEST_AUTH_URI,
				XRT_HTTP_DIGEST_AUTH_CNONCE,
				XRT_HTTP_DIGEST_AUTH_RESPONSE,
				XRT_HTTP_DIGEST_AUTH_OPAQUE
			};
			xstrview* Targets[] = {
				&Digest.Realm,
				&Digest.Nonce,
				&Digest.Uri,
				&Digest.Cnonce,
				&Digest.Response,
				&Digest.Opaque
			};

			for ( size_t i = 0; i < 6u; i++ ) {
				size_t iIndex = Indices[i];

				if ( (iIndex == XRT_HTTP_DIGEST_AUTH_OPAQUE) &&
					((Digest.Flags &
					 XHTTP_DIGEST_AUTH_HAS_OPAQUE) == 0) ) {
					continue;
				}
				iWritten = __xrtHttpParamValueWriteUnchecked(
					&Parameters[iIndex],
					(bytes)pOutput + iOutput
				);
				*Targets[i] = (xstrview){
					(cstr)pOutput + iOutput, iWritten
				};
				iOutput += iWritten;
			}
		}
		memcpy(pSize, &iOutput, sizeof(iOutput));
	}
	memcpy(pDigest, &Digest, sizeof(Digest));
	return true;
}



/* 检查写入描述符、借用视图和标志关系。 */
static bool __xrtHttpDigestAuthWriteValid(
	const xhttpdigestauth* pInput,
	xhttpdigestauth* pDigest,
	xstrview* pAlgorithm,
	size_t* pSize
)
{
	xstrview Canonical;
	xstrview Views[9];
	uint32 iFlags;

	if ( !__xrtRangeValid(pInput, sizeof(*pInput)) ||
		!__xrtRangeValid(pSize, sizeof(size_t)) ||
		__xrtRangesOverlap(
			pInput, sizeof(*pInput), pSize, sizeof(size_t)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pDigest, pInput, sizeof(*pDigest));
	iFlags = pDigest->Flags;
	Views[0] = pDigest->Username;
	Views[1] = pDigest->UsernameLanguage;
	Views[2] = pDigest->Realm;
	Views[3] = pDigest->Nonce;
	Views[4] = pDigest->Uri;
	Views[5] = pDigest->Cnonce;
	Views[6] = pDigest->Response;
	Views[7] = pDigest->Opaque;
	Views[8] = pDigest->AlgorithmName;
	if ( ((iFlags & ~XRT_HTTP_DIGEST_AUTH_VALID_FLAGS) != 0) ||
		(((iFlags & XHTTP_DIGEST_AUTH_USERHASH) != 0) &&
		 ((iFlags & XHTTP_DIGEST_AUTH_HAS_USERHASH) == 0)) ||
		(((iFlags & XHTTP_DIGEST_AUTH_USERNAME_EXTENDED) != 0) &&
		 ((iFlags & XHTTP_DIGEST_AUTH_USERHASH) != 0)) ||
		(((iFlags & XHTTP_DIGEST_AUTH_USERNAME_EXTENDED) == 0) &&
		 (pDigest->UsernameLanguage.Size != 0)) ||
		(((iFlags & XHTTP_DIGEST_AUTH_HAS_OPAQUE) == 0) &&
		 (pDigest->Opaque.Size != 0)) ||
		((pDigest->Qop != XHTTP_DIGEST_QOP_AUTH) &&
		 (pDigest->Qop != XHTTP_DIGEST_QOP_AUTH_INT)) ||
		(pDigest->NonceCount == 0) ) {
		__xrtErrorSetValue();
		return false;
	}
	for ( size_t i = 0; i < 9u; i++ ) {
		if ( !__xrtHttpViewValid(Views[i]) ||
			__xrtRangesOverlap(
				pInput, sizeof(*pInput), Views[i].Data, Views[i].Size
			) || __xrtRangesOverlap(
				pSize, sizeof(size_t), Views[i].Data, Views[i].Size
			) ) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
	}
	if ( !__xrtHttpDigestHexViewValid(pDigest->Response) ||
		!__xrtHttpDigestAuthHexSizeValid(
			pDigest->Algorithm, pDigest->Response.Size
		) || (((iFlags & XHTTP_DIGEST_AUTH_USERHASH) != 0) &&
		(!__xrtHttpDigestHexViewValid(pDigest->Username) ||
		 !__xrtHttpDigestAuthHexSizeValid(
			pDigest->Algorithm, pDigest->Username.Size
		 ))) ) {
		__xrtErrorSetValue();
		return false;
	}
	Canonical = xrtHttpDigestAlgorithmName(pDigest->Algorithm);
	if ( (pDigest->Algorithm != XHTTP_DIGEST_ALGORITHM_UNKNOWN) &&
		(Canonical.Size == 0) ) {
		__xrtErrorSetValue();
		return false;
	}
	if ( (iFlags & XHTTP_DIGEST_AUTH_ALGORITHM_EXPLICIT) == 0 ) {
		if ( (pDigest->Algorithm != XHTTP_DIGEST_ALGORITHM_MD5) ||
			((pDigest->AlgorithmName.Size != 0) &&
			 !xrtHttpTokenEqual(
				pDigest->AlgorithmName, XRT_STR_LITERAL("MD5")
			 )) ) {
			__xrtErrorSetValue();
			return false;
		}
		*pAlgorithm = XRT_STR_LITERAL("MD5");
		return true;
	}
	if ( pDigest->Algorithm == XHTTP_DIGEST_ALGORITHM_UNKNOWN ) {
		if ( !xrtHttpTokenValid(pDigest->AlgorithmName) ) {
			__xrtErrorSetValue();
			return false;
		}
		*pAlgorithm = pDigest->AlgorithmName;
		return true;
	}
	if ( (pDigest->AlgorithmName.Size != 0) &&
		!xrtHttpTokenEqual(pDigest->AlgorithmName, Canonical) ) {
		__xrtErrorSetValue();
		return false;
	}
	*pAlgorithm = Canonical;
	return true;
}



/* 写出 username* 的 UTF-8 RFC 8187 扩展值。 */
static bool __xrtHttpDigestAuthWriterExtended(
	xrt_http_param_writer* pWriter,
	const xhttpdigestauth* pDigest
)
{
	size_t iExtended;
	size_t iWritten;

	if ( !xrtHttpExtValueWrite(
		XRT_STR_LITERAL("UTF-8"),
		pDigest->UsernameLanguage,
		(xbytesview){
			(cbytes)pDigest->Username.Data,
			pDigest->Username.Size
		},
		NULL, 0, &iExtended
	) || !__xrtHttpParamWriterName(
		pWriter, XRT_STR_LITERAL("username*"), true
	) ) {
		return false;
	}
	if ( pWriter->Size > (SIZE_MAX - iExtended) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	if ( (pWriter->Output != NULL) && !xrtHttpExtValueWrite(
		XRT_STR_LITERAL("UTF-8"),
		pDigest->UsernameLanguage,
		(xbytesview){
			(cbytes)pDigest->Username.Data,
			pDigest->Username.Size
		},
		pWriter->Output + pWriter->Size,
		iExtended,
		&iWritten
	) ) {
		return false;
	}
	pWriter->Size += iExtended;
	return true;
}



/* 按固定顺序执行一次凭据测量或写出。 */
static bool __xrtHttpDigestAuthWritePass(
	const xhttpdigestauth* pDigest,
	xstrview Algorithm,
	xrt_http_param_writer* pWriter
)
{
	char NonceCount[8];
	xstrview Qop = xrtHttpDigestQopName(pDigest->Qop);

	__xrtHttpDigestNonceCountWrite(
		pDigest->NonceCount, NonceCount
	);
	if ( !__xrtHttpParamWriterBytes(pWriter, "Digest ", 7u) ) {
		return false;
	}
	if ( (pDigest->Flags &
		XHTTP_DIGEST_AUTH_USERNAME_EXTENDED) != 0 ) {
		if ( !__xrtHttpDigestAuthWriterExtended(pWriter, pDigest) ) {
			return false;
		}
	} else if ( ((pDigest->Flags &
		XHTTP_DIGEST_AUTH_USERHASH) != 0) ?
		!__xrtHttpDigestWriterHex(
			pWriter, XRT_STR_LITERAL("username"),
			pDigest->Username, true
		) : !__xrtHttpParamWriterQuoted(
			pWriter, XRT_STR_LITERAL("username"),
			pDigest->Username, true
		) ) {
		return false;
	}
	if ( !__xrtHttpParamWriterQuoted(
		pWriter, XRT_STR_LITERAL("realm"), pDigest->Realm, false
	) || !__xrtHttpParamWriterQuoted(
		pWriter, XRT_STR_LITERAL("uri"), pDigest->Uri, false
	) ) {
		return false;
	}
	if ( ((pDigest->Flags &
		XHTTP_DIGEST_AUTH_ALGORITHM_EXPLICIT) != 0) &&
		!__xrtHttpParamWriterToken(
			pWriter, XRT_STR_LITERAL("algorithm"), Algorithm, false
		) ) {
		return false;
	}
	if ( !__xrtHttpParamWriterQuoted(
		pWriter, XRT_STR_LITERAL("nonce"), pDigest->Nonce, false
	) || !__xrtHttpParamWriterToken(
		pWriter, XRT_STR_LITERAL("nc"),
		(xstrview){ NonceCount, sizeof(NonceCount) }, false
	) || !__xrtHttpParamWriterQuoted(
		pWriter, XRT_STR_LITERAL("cnonce"), pDigest->Cnonce, false
	) || !__xrtHttpParamWriterToken(
		pWriter, XRT_STR_LITERAL("qop"), Qop, false
	) || !__xrtHttpDigestWriterHex(
		pWriter, XRT_STR_LITERAL("response"),
		pDigest->Response, false
	) ) {
		return false;
	}
	if ( ((pDigest->Flags & XHTTP_DIGEST_AUTH_HAS_OPAQUE) != 0) &&
		!__xrtHttpParamWriterQuoted(
			pWriter, XRT_STR_LITERAL("opaque"),
			pDigest->Opaque, false
		) ) {
		return false;
	}
	if ( (pDigest->Flags &
		XHTTP_DIGEST_AUTH_HAS_USERHASH) != 0 ) {
		xstrview Userhash = ((pDigest->Flags &
			XHTTP_DIGEST_AUTH_USERHASH) != 0) ?
			XRT_STR_LITERAL("true") : XRT_STR_LITERAL("false");

		if ( !__xrtHttpParamWriterToken(
			pWriter, XRT_STR_LITERAL("userhash"), Userhash, false
		) ) {
			return false;
		}
	}
	return true;
}



/* 按规范顺序写出完整 Digest 凭据。 */
XRT_API bool xrtHttpDigestAuthWrite(
	const xhttpdigestauth* pInput,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xhttpdigestauth Digest;
	xstrview Algorithm;
	xstrview Views[9];
	xrt_http_param_writer Writer = { 0 };

	if ( ((pOutput == NULL) && (iCapacity != 0)) ||
		!__xrtHttpDigestAuthWriteValid(
			pInput, &Digest, &Algorithm, pSize
		) || !__xrtHttpDigestAuthWritePass(
			&Digest, Algorithm, &Writer
		) ) {
		if ( (pOutput == NULL) && (iCapacity != 0) ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	Views[0] = Digest.Username;
	Views[1] = Digest.UsernameLanguage;
	Views[2] = Digest.Realm;
	Views[3] = Digest.Nonce;
	Views[4] = Digest.Uri;
	Views[5] = Digest.Cnonce;
	Views[6] = Digest.Response;
	Views[7] = Digest.Opaque;
	Views[8] = Digest.AlgorithmName;
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
	for ( size_t i = 0; i < 9u; i++ ) {
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
	if ( !__xrtHttpDigestAuthWritePass(
		&Digest, Algorithm, &Writer
	) ) {
		return false;
	}
	memcpy(pSize, &Writer.Size, sizeof(Writer.Size));
	return true;
}



#undef XRT_HTTP_DIGEST_AUTH_USERNAME
#undef XRT_HTTP_DIGEST_AUTH_USERNAME_EXT
#undef XRT_HTTP_DIGEST_AUTH_REALM
#undef XRT_HTTP_DIGEST_AUTH_NONCE
#undef XRT_HTTP_DIGEST_AUTH_URI
#undef XRT_HTTP_DIGEST_AUTH_RESPONSE
#undef XRT_HTTP_DIGEST_AUTH_ALGORITHM
#undef XRT_HTTP_DIGEST_AUTH_CNONCE
#undef XRT_HTTP_DIGEST_AUTH_OPAQUE
#undef XRT_HTTP_DIGEST_AUTH_QOP
#undef XRT_HTTP_DIGEST_AUTH_NC
#undef XRT_HTTP_DIGEST_AUTH_USERHASH
#undef XRT_HTTP_DIGEST_AUTH_PARAMETERS
#undef XRT_HTTP_DIGEST_AUTH_REQUIRED
#undef XRT_HTTP_DIGEST_AUTH_VALID_FLAGS

#endif
