#include "../internal/xrt_http.h"

#include <xrt/http_auth.h>



#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_INFO)

#define XRT_HTTP_DIGEST_INFO_NEXT_NONCE 0u
#define XRT_HTTP_DIGEST_INFO_QOP 1u
#define XRT_HTTP_DIGEST_INFO_RESPONSE 2u
#define XRT_HTTP_DIGEST_INFO_CNONCE 3u
#define XRT_HTTP_DIGEST_INFO_NC 4u
#define XRT_HTTP_DIGEST_INFO_PARAMETERS 5u

#define XRT_HTTP_DIGEST_INFO_RESPONSE_GROUP \
	((UINT32_C(1) << XRT_HTTP_DIGEST_INFO_QOP) | \
	 (UINT32_C(1) << XRT_HTTP_DIGEST_INFO_RESPONSE) | \
	 (UINT32_C(1) << XRT_HTTP_DIGEST_INFO_CNONCE) | \
	 (UINT32_C(1) << XRT_HTTP_DIGEST_INFO_NC))

#define XRT_HTTP_DIGEST_INFO_VALID_FLAGS \
	((uint32)XHTTP_DIGEST_INFO_HAS_NEXT_NONCE | \
	 (uint32)XHTTP_DIGEST_INFO_HAS_RESPONSE)



static const xstrview __xrtHttpDigestInfoNames[
	XRT_HTTP_DIGEST_INFO_PARAMETERS
] = {
	XRT_STR_INIT("nextnonce"),
	XRT_STR_INIT("qop"),
	XRT_STR_INIT("rspauth"),
	XRT_STR_INIT("cnonce"),
	XRT_STR_INIT("nc")
};



/* 查找 Authentication-Info 标准参数索引。 */
static size_t __xrtHttpDigestInfoNameIndex(xstrview Name)
{
	for ( size_t i = 0; i < XRT_HTTP_DIGEST_INFO_PARAMETERS; i++ ) {
		if ( xrtHttpTokenEqual(Name, __xrtHttpDigestInfoNames[i]) ) {
			return i;
		}
	}
	return XRT_HTTP_DIGEST_INFO_PARAMETERS;
}



/* 判断十六进制响应长度是否匹配请求算法。 */
static bool __xrtHttpDigestInfoResponseSizeValid(
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



/* 校验读取入口的固定描述符和内存区间。 */
static bool __xrtHttpDigestInfoReadValid(
	xstrview Value,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestinfo* pInfo
)
{
	if ( !__xrtHttpViewValid(Value) ||
		!__xrtRangeValid(pSize, sizeof(size_t)) ||
		!__xrtRangeValid(pInfo, sizeof(*pInfo)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		((pOutput != NULL) && !__xrtRangeValid(
			pOutput, iCapacity
		)) || __xrtRangesOverlap(
			pSize, sizeof(size_t), Value.Data, Value.Size
		) || __xrtRangesOverlap(
			pInfo, sizeof(*pInfo), Value.Data, Value.Size
		) || __xrtRangesOverlap(
			pSize, sizeof(size_t), pInfo, sizeof(*pInfo)
		) || ((pOutput != NULL) && (__xrtRangesOverlap(
			pOutput, iCapacity, Value.Data, Value.Size
		) || __xrtRangesOverlap(
			pOutput, iCapacity, pSize, sizeof(size_t)
		) || __xrtRangesOverlap(
			pOutput, iCapacity, pInfo, sizeof(*pInfo)
		))) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 严格解析 Digest Authentication-Info 参数列表。 */
XRT_API bool xrtHttpDigestInfoRead(
	xstrview Value,
	xhttpdigestalgorithm Algorithm,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestinfo* pInfo
)
{
	xhttpdigestinfo Info = { 0 };
	xhttpparam Parameters[XRT_HTTP_DIGEST_INFO_PARAMETERS] = { 0 };
	size_t Sizes[XRT_HTTP_DIGEST_INFO_PARAMETERS] = { 0 };
	xhttpparam Param;
	xhttpnext Next;
	size_t iParamOffset = 0;
	size_t iRequired = 0;
	uint32 iSeen = 0;

	if ( !__xrtHttpDigestInfoReadValid(
		Value, pOutput, iCapacity, pSize, pInfo
	) ) {
		return false;
	}
	memcpy(pInfo, &Info, sizeof(Info));
	if ( (Algorithm != XHTTP_DIGEST_ALGORITHM_UNKNOWN) &&
		(xrtHttpDigestSize(Algorithm) == 0) ) {
		__xrtErrorSetValue();
		return false;
	}
	Info.Algorithm = Algorithm;
	for ( ;; ) {
		size_t iIndex;
		uint32 iBit;

		Next = xrtHttpAuthParamNext(
			Value, &iParamOffset, &Param
		);
		if ( Next == XHTTP_NEXT_END ) {
			break;
		}
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		iIndex = __xrtHttpDigestInfoNameIndex(Param.Name);
		if ( iIndex == XRT_HTTP_DIGEST_INFO_PARAMETERS ) {
			continue;
		}
		iBit = UINT32_C(1) << (uint32)iIndex;
		if ( (iSeen & iBit) != 0 ) {
			__xrtErrorSetValue();
			return false;
		}
		iSeen |= iBit;

		if ( (iIndex == XRT_HTTP_DIGEST_INFO_NEXT_NONCE) ||
			(iIndex == XRT_HTTP_DIGEST_INFO_RESPONSE) ||
			(iIndex == XRT_HTTP_DIGEST_INFO_CNONCE) ) {
			if ( (Param.Flags & XHTTP_PARAM_QUOTED) == 0 ) {
				__xrtErrorSetValue();
				return false;
			}
			if ( iIndex == XRT_HTTP_DIGEST_INFO_RESPONSE ) {
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
			continue;
		}
		if ( iIndex == XRT_HTTP_DIGEST_INFO_QOP ) {
			if ( ((Param.Flags & XHTTP_PARAM_QUOTED) != 0) ||
				!xrtHttpParamTokenValid(&Param) ) {
				__xrtErrorSetValue();
				return false;
			}
			Info.Qop = xrtHttpDigestQopParse(Param.Value);
			if ( Info.Qop == XHTTP_DIGEST_QOP_NONE ) {
				__xrtErrorSetValue();
				return false;
			}
			continue;
		}
		if ( !__xrtHttpDigestNonceCountRead(
			&Param, &Info.NonceCount
		) || (Info.NonceCount == 0) ) {
			__xrtErrorSetValue();
			return false;
		}
	}
	if ( (iSeen & (UINT32_C(1) <<
		XRT_HTTP_DIGEST_INFO_NEXT_NONCE)) != 0 ) {
		if ( Sizes[XRT_HTTP_DIGEST_INFO_NEXT_NONCE] == 0 ) {
			__xrtErrorSetValue();
			return false;
		}
		Info.Flags |= XHTTP_DIGEST_INFO_HAS_NEXT_NONCE;
	}
	if ( (iSeen & XRT_HTTP_DIGEST_INFO_RESPONSE_GROUP) != 0 ) {
		if ( (iSeen & XRT_HTTP_DIGEST_INFO_RESPONSE_GROUP) !=
			XRT_HTTP_DIGEST_INFO_RESPONSE_GROUP ) {
			__xrtErrorSetValue();
			return false;
		}
		if ( !__xrtHttpDigestInfoResponseSizeValid(
			Algorithm, Sizes[XRT_HTTP_DIGEST_INFO_RESPONSE]
		) ) {
			__xrtErrorSetValue();
			return false;
		}
		Info.Flags |= XHTTP_DIGEST_INFO_HAS_RESPONSE;
	}
	if ( pOutput == NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		memcpy(pInfo, &Info, sizeof(Info));
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
		static const size_t Indices[] = {
			XRT_HTTP_DIGEST_INFO_NEXT_NONCE,
			XRT_HTTP_DIGEST_INFO_RESPONSE,
			XRT_HTTP_DIGEST_INFO_CNONCE
		};
		xstrview* Targets[] = {
			&Info.NextNonce,
			&Info.Response,
			&Info.Cnonce
		};
		size_t iOutput = 0;

		for ( size_t i = 0; i < 3u; i++ ) {
			size_t iIndex = Indices[i];
			size_t iWritten;

			if ( ((iIndex == XRT_HTTP_DIGEST_INFO_NEXT_NONCE) &&
				 ((Info.Flags &
				  XHTTP_DIGEST_INFO_HAS_NEXT_NONCE) == 0)) ||
				((iIndex != XRT_HTTP_DIGEST_INFO_NEXT_NONCE) &&
				 ((Info.Flags &
				  XHTTP_DIGEST_INFO_HAS_RESPONSE) == 0)) ) {
				continue;
			}
			iWritten = __xrtHttpParamValueWriteUnchecked(
				&Parameters[iIndex], (bytes)pOutput + iOutput
			);
			*Targets[i] = (xstrview){
				(cstr)pOutput + iOutput, iWritten
			};
			iOutput += iWritten;
		}
		memcpy(pSize, &iOutput, sizeof(iOutput));
	}
	memcpy(pInfo, &Info, sizeof(Info));
	return true;
}



/* 校验 Authentication-Info 写入描述符和上下文关系。 */
static bool __xrtHttpDigestInfoWriteValid(
	const xhttpdigestinfo* pInput,
	xhttpdigestinfo* pInfo,
	size_t* pSize
)
{
	xstrview Views[3];
	uint32 iFlags;

	if ( !__xrtRangeValid(pInput, sizeof(*pInput)) ||
		!__xrtRangeValid(pSize, sizeof(size_t)) ||
		__xrtRangesOverlap(
			pInput, sizeof(*pInput), pSize, sizeof(size_t)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pInfo, pInput, sizeof(*pInfo));
	iFlags = pInfo->Flags;
	Views[0] = pInfo->NextNonce;
	Views[1] = pInfo->Response;
	Views[2] = pInfo->Cnonce;
	for ( size_t i = 0; i < 3u; i++ ) {
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
	if ( ((iFlags & ~XRT_HTTP_DIGEST_INFO_VALID_FLAGS) != 0) ||
		((pInfo->Algorithm != XHTTP_DIGEST_ALGORITHM_UNKNOWN) &&
		 (xrtHttpDigestSize(pInfo->Algorithm) == 0)) ||
		(((iFlags & XHTTP_DIGEST_INFO_HAS_NEXT_NONCE) != 0) &&
		 (pInfo->NextNonce.Size == 0)) ||
		(((iFlags & XHTTP_DIGEST_INFO_HAS_NEXT_NONCE) == 0) &&
		 (pInfo->NextNonce.Size != 0)) ||
		(((iFlags & XHTTP_DIGEST_INFO_HAS_RESPONSE) == 0) &&
		((pInfo->Qop != XHTTP_DIGEST_QOP_NONE) ||
		 (pInfo->NonceCount != 0) ||
		 (pInfo->Response.Size != 0) ||
		 (pInfo->Cnonce.Size != 0))) ||
		(((iFlags & XHTTP_DIGEST_INFO_HAS_RESPONSE) != 0) &&
		(((pInfo->Qop != XHTTP_DIGEST_QOP_AUTH) &&
		  (pInfo->Qop != XHTTP_DIGEST_QOP_AUTH_INT)) ||
		 (pInfo->NonceCount == 0) ||
		 !__xrtHttpDigestHexViewValid(pInfo->Response) ||
		 !__xrtHttpDigestInfoResponseSizeValid(
			pInfo->Algorithm, pInfo->Response.Size
		 ))) ) {
		__xrtErrorSetValue();
		return false;
	}
	return true;
}



/* 按固定顺序执行一次 Authentication-Info 测量或写出。 */
static bool __xrtHttpDigestInfoWritePass(
	const xhttpdigestinfo* pInfo,
	xrt_http_param_writer* pWriter
)
{
	bool bFirst = true;

	if ( (pInfo->Flags & XHTTP_DIGEST_INFO_HAS_NEXT_NONCE) != 0 ) {
		if ( !__xrtHttpParamWriterQuoted(
			pWriter, XRT_STR_LITERAL("nextnonce"),
			pInfo->NextNonce, true
		) ) {
			return false;
		}
		bFirst = false;
	}
	if ( (pInfo->Flags & XHTTP_DIGEST_INFO_HAS_RESPONSE) != 0 ) {
		char NonceCount[8];
		xstrview Qop = xrtHttpDigestQopName(pInfo->Qop);

		__xrtHttpDigestNonceCountWrite(
			pInfo->NonceCount, NonceCount
		);
		if ( !__xrtHttpParamWriterToken(
			pWriter, XRT_STR_LITERAL("qop"), Qop, bFirst
		) || !__xrtHttpDigestWriterHex(
			pWriter, XRT_STR_LITERAL("rspauth"),
			pInfo->Response, false
		) || !__xrtHttpParamWriterQuoted(
			pWriter, XRT_STR_LITERAL("cnonce"),
			pInfo->Cnonce, false
		) || !__xrtHttpParamWriterToken(
			pWriter, XRT_STR_LITERAL("nc"),
			(xstrview){ NonceCount, sizeof(NonceCount) }, false
		) ) {
			return false;
		}
	}
	return true;
}



/* 写出 Digest Authentication-Info 参数列表。 */
XRT_API bool xrtHttpDigestInfoWrite(
	const xhttpdigestinfo* pInput,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xhttpdigestinfo Info;
	xstrview Views[3];
	xrt_http_param_writer Writer = { 0 };

	if ( ((pOutput == NULL) && (iCapacity != 0)) ||
		!__xrtHttpDigestInfoWriteValid(pInput, &Info, pSize) ||
		!__xrtHttpDigestInfoWritePass(&Info, &Writer) ) {
		if ( (pOutput == NULL) && (iCapacity != 0) ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	Views[0] = Info.NextNonce;
	Views[1] = Info.Response;
	Views[2] = Info.Cnonce;
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
	for ( size_t i = 0; i < 3u; i++ ) {
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
	if ( !__xrtHttpDigestInfoWritePass(&Info, &Writer) ) {
		return false;
	}
	memcpy(pSize, &Writer.Size, sizeof(Writer.Size));
	return true;
}



#undef XRT_HTTP_DIGEST_INFO_NEXT_NONCE
#undef XRT_HTTP_DIGEST_INFO_QOP
#undef XRT_HTTP_DIGEST_INFO_RESPONSE
#undef XRT_HTTP_DIGEST_INFO_CNONCE
#undef XRT_HTTP_DIGEST_INFO_NC
#undef XRT_HTTP_DIGEST_INFO_PARAMETERS
#undef XRT_HTTP_DIGEST_INFO_RESPONSE_GROUP
#undef XRT_HTTP_DIGEST_INFO_VALID_FLAGS

#endif
