#include "../internal/xrt_http.h"

#include <xrt/http_auth.h>



#if defined(XRT_FEATURE_HTTP_AUTH_BEARER_CHALLENGE)

#define XRT_HTTP_BEARER_PARAMETER_COUNT 5u
#define XRT_HTTP_BEARER_VALID_FLAGS \
	((uint32)XHTTP_BEARER_HAS_REALM | \
	 (uint32)XHTTP_BEARER_HAS_SCOPE | \
	 (uint32)XHTTP_BEARER_HAS_ERROR | \
	 (uint32)XHTTP_BEARER_HAS_ERROR_DESCRIPTION | \
	 (uint32)XHTTP_BEARER_HAS_ERROR_URI)



static const xstrview __xrtHttpBearerNames[
	XRT_HTTP_BEARER_PARAMETER_COUNT
] = {
	XRT_STR_INIT("realm"),
	XRT_STR_INIT("scope"),
	XRT_STR_INIT("error"),
	XRT_STR_INIT("error_description"),
	XRT_STR_INIT("error_uri")
};



static const uint32 __xrtHttpBearerFlags[
	XRT_HTTP_BEARER_PARAMETER_COUNT
] = {
	XHTTP_BEARER_HAS_REALM,
	XHTTP_BEARER_HAS_SCOPE,
	XHTTP_BEARER_HAS_ERROR,
	XHTTP_BEARER_HAS_ERROR_DESCRIPTION,
	XHTTP_BEARER_HAS_ERROR_URI
};



typedef enum xrt_http_bearer_check {
	XRT_HTTP_BEARER_CHECK_ERROR = -1,
	XRT_HTTP_BEARER_CHECK_INVALID = 0,
	XRT_HTTP_BEARER_CHECK_VALID = 1
} xrt_http_bearer_check;



/* 返回 challenge 中与标准参数索引对应的值。 */
static xstrview __xrtHttpBearerValue(
	const xhttpbearerchallenge* pChallenge,
	size_t iIndex
)
{
	switch ( iIndex ) {
		case 0:
			return pChallenge->Realm;
		case 1:
			return pChallenge->Scope;
		case 2:
			return pChallenge->Error;
		case 3:
			return pChallenge->ErrorDescription;
		default:
			return pChallenge->ErrorUri;
	}
}



/* 设置 challenge 中与标准参数索引对应的借用值。 */
static void __xrtHttpBearerValueSet(
	xhttpbearerchallenge* pChallenge,
	size_t iIndex,
	xstrview Value
)
{
	switch ( iIndex ) {
		case 0:
			pChallenge->Realm = Value;
			break;
		case 1:
			pChallenge->Scope = Value;
			break;
		case 2:
			pChallenge->Error = Value;
			break;
		case 3:
			pChallenge->ErrorDescription = Value;
			break;
		default:
			pChallenge->ErrorUri = Value;
			break;
	}
}



/* 查找标准参数索引，未知扩展参数返回参数总数。 */
static size_t __xrtHttpBearerNameIndex(xstrview Name)
{
	size_t i;

	for ( i = 0; i < XRT_HTTP_BEARER_PARAMETER_COUNT; i++ ) {
		if ( xrtHttpTokenEqual(Name, __xrtHttpBearerNames[i]) ) {
			return i;
		}
	}
	return XRT_HTTP_BEARER_PARAMETER_COUNT;
}



/* 验证 RFC 6750 scope 的非空、单空格分隔和字符集合。 */
static bool __xrtHttpBearerScopeValid(const xhttpparam* pParam)
{
	size_t iOffset = 0;
	size_t iCount = 0;
	uint8 iByte;
	bool bSpace = true;

	while ( __xrtHttpParamSemanticNext(
		pParam, &iOffset, &iByte
	) ) {
		if ( iByte == (uint8)' ' ) {
			if ( bSpace ) {
				return false;
			}
			bSpace = true;
			continue;
		}
		if ( !((iByte == UINT8_C(0x21)) ||
			((iByte >= UINT8_C(0x23)) &&
			 (iByte <= UINT8_C(0x5B))) ||
			((iByte >= UINT8_C(0x5D)) &&
			 (iByte <= UINT8_C(0x7E)))) ) {
			return false;
		}
		bSpace = false;
		iCount++;
	}
	return (iCount != 0) && !bSpace;
}



/* 验证 error 和 error_description 的 RFC 6749 NQSCHAR 集合。 */
static bool __xrtHttpBearerErrorTextValid(
	const xhttpparam* pParam
)
{
	size_t iOffset = 0;
	size_t iCount = 0;
	uint8 iByte;

	while ( __xrtHttpParamSemanticNext(
		pParam, &iOffset, &iByte
	) ) {
		if ( !(((iByte >= UINT8_C(0x20)) &&
			  (iByte <= UINT8_C(0x21))) ||
			 ((iByte >= UINT8_C(0x23)) &&
			  (iByte <= UINT8_C(0x5B))) ||
			 ((iByte >= UINT8_C(0x5D)) &&
			  (iByte <= UINT8_C(0x7E)))) ) {
			return false;
		}
		iCount++;
	}
	return iCount != 0;
}



/* 验证 error_uri 的 NQCHAR 集合。 */
static bool __xrtHttpBearerUriCharactersValid(
	const xhttpparam* pParam
)
{
	size_t iOffset = 0;
	size_t iCount = 0;
	uint8 iByte;

	while ( __xrtHttpParamSemanticNext(
		pParam, &iOffset, &iByte
	) ) {
		if ( !((iByte == UINT8_C(0x21)) ||
			((iByte >= UINT8_C(0x23)) &&
			 (iByte <= UINT8_C(0x5B))) ||
			((iByte >= UINT8_C(0x5D)) &&
			 (iByte <= UINT8_C(0x7E)))) ) {
			return false;
		}
		iCount++;
	}
	return iCount != 0;
}



/* 严格验证 error_uri 是无 fragment 的绝对 URI。 */
static xrt_http_bearer_check __xrtHttpBearerUriValid(
	const xhttpparam* pParam,
	size_t iDecoded
)
{
	char Local[XRT_HTTP_AUTH_LOCAL_BYTES];
	str sValue = NULL;
	xstrview Value;
	xurl Url;
	size_t iWritten;
	bool bEscaped;
	bool bValid;

	if ( !__xrtHttpBearerUriCharactersValid(pParam) ) {
		return XRT_HTTP_BEARER_CHECK_INVALID;
	}
	bEscaped = ((pParam->Flags & XHTTP_PARAM_QUOTED) != 0) &&
		(memchr(
			pParam->Value.Data, '\\', pParam->Value.Size
		) != NULL);
	if ( !bEscaped ) {
		Value = pParam->Value;
	} else {
		sValue = iDecoded <= sizeof(Local) ? Local :
			(str)xrtMalloc(iDecoded);
		if ( sValue == NULL ) {
			return XRT_HTTP_BEARER_CHECK_ERROR;
		}
		iWritten = __xrtHttpParamValueWriteUnchecked(
			pParam, (bytes)sValue
		);
		Value = (xstrview){ sValue, iWritten };
	}
	bValid = xrtUrlParse(Value, &Url) &&
		((Url.Flags & XURL_HAS_SCHEME) != 0) &&
		((Url.Flags & XURL_HAS_FRAGMENT) == 0);
	if ( bEscaped && (sValue != Local) ) {
		xrtFree(sValue);
	}
	return bValid ? XRT_HTTP_BEARER_CHECK_VALID :
		XRT_HTTP_BEARER_CHECK_INVALID;
}



/* 验证一个已解析标准参数的 RFC 6750 语义。 */
static xrt_http_bearer_check __xrtHttpBearerParameterValid(
	size_t iIndex,
	const xhttpparam* pParam,
	size_t iDecoded
)
{
	if ( iIndex == 0 ) {
		return XRT_HTTP_BEARER_CHECK_VALID;
	}
	if ( iIndex == 1 ) {
		return __xrtHttpBearerScopeValid(pParam) ?
			XRT_HTTP_BEARER_CHECK_VALID :
			XRT_HTTP_BEARER_CHECK_INVALID;
	}
	if ( (iIndex == 2) || (iIndex == 3) ) {
		return __xrtHttpBearerErrorTextValid(pParam) ?
			XRT_HTTP_BEARER_CHECK_VALID :
			XRT_HTTP_BEARER_CHECK_INVALID;
	}
	return __xrtHttpBearerUriValid(pParam, iDecoded);
}



/* 安全累计 challenge 长度。 */
static bool __xrtHttpBearerSizeAdd(
	size_t* pSize,
	size_t iAdd
)
{
	if ( *pSize > (SIZE_MAX - iAdd) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	*pSize += iAdd;
	return true;
}



/* 校验读取函数的固定输出、输入和别名边界。 */
static bool __xrtHttpBearerReadOutputValid(
	xstrview Value,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpbearerchallenge* pChallenge
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



/* 解析、验证并解码 Bearer challenge 的五个标准参数。 */
XRT_API bool xrtHttpBearerChallengeRead(
	xstrview Value,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpbearerchallenge* pChallenge
)
{
	xhttpbearerchallenge Challenge = { 0 };
	xhttpparam Parameters[XRT_HTTP_BEARER_PARAMETER_COUNT] = { 0 };
	size_t Sizes[XRT_HTTP_BEARER_PARAMETER_COUNT] = { 0 };
	xhttpauth Auth;
	xhttpparam Param;
	xhttpnext Next;
	size_t iOffset = 0;
	size_t iRequired = 0;
	size_t iIndex;
	size_t iWritten;
	uint32 iFlag;
	xrt_http_bearer_check Check;

	if ( !__xrtHttpBearerReadOutputValid(
		Value, pOutput, iCapacity, pSize, pChallenge
	) ) {
		return false;
	}
	memcpy(pChallenge, &Challenge, sizeof(Challenge));
	if ( !xrtHttpAuthParse(Value, &Auth) ) {
		return false;
	}
	if ( !xrtHttpTokenEqual(
			Auth.Scheme, XRT_STR_LITERAL("Bearer")
		) || (Auth.Kind != XHTTP_AUTH_PARAMS) ) {
		__xrtErrorSetValue();
		return false;
	}
	for ( ;; ) {
		Next = xrtHttpAuthParamNext(
			Auth.Data, &iOffset, &Param
		);
		if ( Next == XHTTP_NEXT_END ) {
			break;
		}
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		iIndex = __xrtHttpBearerNameIndex(Param.Name);
		if ( iIndex == XRT_HTTP_BEARER_PARAMETER_COUNT ) {
			continue;
		}
		iFlag = __xrtHttpBearerFlags[iIndex];
		if ( (Challenge.Flags & iFlag) != 0 ) {
			__xrtErrorSetValue();
			return false;
		}
		if ( !xrtHttpParamValueWrite(
			&Param, NULL, 0, &Sizes[iIndex]
		) ) {
			return false;
		}
		Check = __xrtHttpBearerParameterValid(
			iIndex, &Param, Sizes[iIndex]
		);
		if ( Check == XRT_HTTP_BEARER_CHECK_ERROR ) {
			return false;
		}
		if ( Check == XRT_HTTP_BEARER_CHECK_INVALID ) {
			__xrtErrorSetValue();
			return false;
		}
		if ( !__xrtHttpBearerSizeAdd(
			&iRequired, Sizes[iIndex]
		) ) {
			return false;
		}
		Parameters[iIndex] = Param;
		Challenge.Flags |= iFlag;
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
	iOffset = 0;
	for ( iIndex = 0;
		iIndex < XRT_HTTP_BEARER_PARAMETER_COUNT;
		iIndex++ ) {
		if ( (Challenge.Flags &
			__xrtHttpBearerFlags[iIndex]) == 0 ) {
			continue;
		}
		iWritten = __xrtHttpParamValueWriteUnchecked(
			&Parameters[iIndex], (bytes)pOutput + iOffset
		);
		__xrtHttpBearerValueSet(
			&Challenge,
			iIndex,
			(xstrview){
				(cstr)pOutput + iOffset,
				iWritten
			}
		);
		iOffset += iWritten;
	}
	memcpy(pSize, &iOffset, sizeof(iOffset));
	memcpy(pChallenge, &Challenge, sizeof(Challenge));
	return true;
}



/* 验证写入配置、标准参数语义并计算线路长度。 */
static bool __xrtHttpBearerWriteMeasure(
	const xhttpbearerchallenge* pInput,
	xhttpbearerchallenge* pChallenge,
	size_t* pRequired,
	size_t* pSize
)
{
	xhttpparam Param;
	xrt_http_bearer_check Check;
	size_t iRequired = 7u;
	size_t iQuoted;
	size_t iIndex;
	size_t iCount = 0;
	xstrview Value;

	if ( !__xrtRangeValid(pInput, sizeof(*pInput)) ||
		!__xrtRangeValid(pSize, sizeof(size_t)) ||
		__xrtRangesOverlap(
			pInput, sizeof(*pInput), pSize, sizeof(size_t)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pChallenge, pInput, sizeof(*pChallenge));
	if ( ((pChallenge->Flags & ~XRT_HTTP_BEARER_VALID_FLAGS) != 0) ||
		(pChallenge->Flags == 0) ) {
		__xrtErrorSetValue();
		return false;
	}
	for ( iIndex = 0;
		iIndex < XRT_HTTP_BEARER_PARAMETER_COUNT;
		iIndex++ ) {
		Value = __xrtHttpBearerValue(pChallenge, iIndex);
		if ( !__xrtHttpViewValid(Value) ||
			__xrtRangesOverlap(
				pInput, sizeof(*pInput), Value.Data, Value.Size
			) || __xrtRangesOverlap(
				pSize, sizeof(size_t), Value.Data, Value.Size
			) ) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
		if ( (pChallenge->Flags &
			__xrtHttpBearerFlags[iIndex]) == 0 ) {
			if ( Value.Size != 0 ) {
				__xrtErrorSetInvalidArgument();
				return false;
			}
			continue;
		}
		Param = (xhttpparam){
			__xrtHttpBearerNames[iIndex],
			Value,
			XHTTP_PARAM_HAS_VALUE
		};
		Check = __xrtHttpBearerParameterValid(
			iIndex, &Param, Value.Size
		);
		if ( Check == XRT_HTTP_BEARER_CHECK_ERROR ) {
			return false;
		}
		if ( Check == XRT_HTTP_BEARER_CHECK_INVALID ) {
			__xrtErrorSetValue();
			return false;
		}
		if ( !xrtHttpQuotedWrite(
			Value, NULL, 0, &iQuoted
		) || !__xrtHttpBearerSizeAdd(
			&iRequired,
			(iCount == 0 ? 0u : 2u) +
				__xrtHttpBearerNames[iIndex].Size + 1u
		) || !__xrtHttpBearerSizeAdd(
			&iRequired, iQuoted
		) ) {
			return false;
		}
		iCount++;
	}
	*pRequired = iRequired;
	return true;
}



/* 按规范顺序写出 Bearer challenge 标准参数。 */
XRT_API bool xrtHttpBearerChallengeWrite(
	const xhttpbearerchallenge* pInput,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xhttpbearerchallenge Challenge;
	bytes pWrite = (bytes)pOutput;
	size_t iRequired;
	size_t iOffset = 0;
	size_t iIndex;
	size_t iCount = 0;
	xstrview Value;

	if ( ((pOutput == NULL) && (iCapacity != 0)) ||
		!__xrtHttpBearerWriteMeasure(
			pInput, &Challenge, &iRequired, pSize
		) ) {
		if ( (pOutput == NULL) && (iCapacity != 0) ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	if ( pOutput == NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		return true;
	}
	if ( !__xrtRangeValid(pOutput, iRequired) ||
		__xrtRangesOverlap(
			pOutput, iRequired, pInput, sizeof(*pInput)
		) || __xrtRangesOverlap(
			pOutput, iRequired, pSize, sizeof(size_t)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	for ( iIndex = 0;
		iIndex < XRT_HTTP_BEARER_PARAMETER_COUNT;
		iIndex++ ) {
		Value = __xrtHttpBearerValue(&Challenge, iIndex);
		if ( ((Challenge.Flags &
			__xrtHttpBearerFlags[iIndex]) != 0) &&
			__xrtRangesOverlap(
				pOutput, iRequired, Value.Data, Value.Size
			) ) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
	}
	if ( iCapacity < iRequired ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		__xrtErrorSetRange();
		return false;
	}
	memcpy(pWrite + iOffset, "Bearer ", 7u);
	iOffset += 7u;
	for ( iIndex = 0;
		iIndex < XRT_HTTP_BEARER_PARAMETER_COUNT;
		iIndex++ ) {
		if ( (Challenge.Flags &
			__xrtHttpBearerFlags[iIndex]) == 0 ) {
			continue;
		}
		if ( iCount != 0 ) {
			memcpy(pWrite + iOffset, ", ", 2u);
			iOffset += 2u;
		}
		memcpy(
			pWrite + iOffset,
			__xrtHttpBearerNames[iIndex].Data,
			__xrtHttpBearerNames[iIndex].Size
		);
		iOffset += __xrtHttpBearerNames[iIndex].Size;
		pWrite[iOffset++] = (uint8)'=';
		Value = __xrtHttpBearerValue(&Challenge, iIndex);
		iOffset += __xrtHttpQuotedWriteUnchecked(
			Value, pWrite + iOffset
		);
		iCount++;
	}
	memcpy(pSize, &iOffset, sizeof(iOffset));
	return true;
}

#endif
