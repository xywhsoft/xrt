#include "../internal/xrt_websocket.h"



#if defined(XRT_FEATURE_WEBSOCKET_EXTENSION)

/* 判断借用视图是否满足空值一致性。 */
static bool __xrtWsExtensionViewValid(xstrview Text)
{
	return __xrtRangeValid(Text.Data, Text.Size);
}



/* 判断字节是否是扩展列表允许忽略的 OWS。 */
static bool __xrtWsExtensionOws(char iByte)
{
	return (iByte == ' ') || (iByte == '\t');
}



/* 查找当前扩展项在引号之外的结束逗号。 */
static size_t __xrtWsExtensionItemEnd(
	xstrview Extensions,
	size_t iBegin
)
{
	bool bQuoted = false;
	bool bEscape = false;
	size_t i;

	for ( i = iBegin; i < Extensions.Size; i++ ) {
		char iByte = Extensions.Data[i];

		if ( bQuoted ) {
			if ( bEscape ) {
				bEscape = false;
			} else if ( iByte == '\\' ) {
				bEscape = true;
			} else if ( iByte == '"' ) {
				bQuoted = false;
			}
		} else if ( iByte == '"' ) {
			bQuoted = true;
		} else if ( iByte == ',' ) {
			break;
		}
	}
	return i;
}



/* 验证非空参数段的完整语法和 WebSocket token 语义。 */
static bool __xrtWsExtensionParametersValid(
	const xwsextension* pExtension
)
{
	xhttpparam Param;
	xhttpnext Next;
	size_t iOffset = 0;
	size_t iCount = 0;

	for ( ;; ) {
		Next = xrtHttpParamNext(
			pExtension->Parameters,
			&iOffset,
			&Param
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			__xrtWsHandshakeWrap(
				XERR_PROTOCOL,
				XWS_HANDSHAKE_ERROR_EXTENSION,
				"extension-parameter",
				"invalid WebSocket extension parameter syntax"
			);
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			if ( iCount == 0 ) {
				__xrtWsHandshakeError(
					XERR_PROTOCOL,
					XWS_HANDSHAKE_ERROR_EXTENSION,
					"extension-parameter",
					"WebSocket extension parameter list is empty"
				);
				return false;
			}
			return true;
		}
		if ( ((Param.Flags & XHTTP_PARAM_HAS_VALUE) != 0) &&
			!xrtHttpParamTokenValid(&Param) ) {
			__xrtWsHandshakeError(
				XERR_PROTOCOL,
				XWS_HANDSHAKE_ERROR_EXTENSION,
				"extension-parameter",
				"WebSocket extension parameter value is not a token"
			);
			return false;
		}
		iCount++;
	}
}



/* 解析当前非空扩展项并验证它的全部参数。 */
static bool __xrtWsExtensionItemParse(
	xstrview Extensions,
	size_t iBegin,
	size_t iEnd,
	xwsextension* pExtension
)
{
	xwsextension Extension;
	size_t iName;
	size_t i;

	while ( (iEnd > iBegin) &&
		__xrtWsExtensionOws(Extensions.Data[iEnd - 1u]) ) {
		iEnd--;
	}
	iName = iBegin;
	i = iBegin;
	while ( (i < iEnd) &&
		__xrtHttpTokenByte((uint8)Extensions.Data[i]) ) {
		i++;
	}
	if ( i == iName ) {
		__xrtWsHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_EXTENSION,
			"extension",
			"WebSocket extension name is not a token"
		);
		return false;
	}
	Extension.Name.Data = Extensions.Data + iName;
	Extension.Name.Size = i - iName;
	while ( (i < iEnd) &&
		__xrtWsExtensionOws(Extensions.Data[i]) ) {
		i++;
	}
	Extension.Parameters.Data = NULL;
	Extension.Parameters.Size = 0;
	if ( i < iEnd ) {
		if ( Extensions.Data[i] != ';' ) {
			__xrtWsHandshakeError(
				XERR_PROTOCOL,
				XWS_HANDSHAKE_ERROR_EXTENSION,
				"extension",
				"WebSocket extension name is followed by invalid data"
			);
			return false;
		}
		Extension.Parameters.Data = Extensions.Data + i + 1u;
		Extension.Parameters.Size = iEnd - i - 1u;
		if ( !__xrtWsExtensionParametersValid(&Extension) ) {
			return false;
		}
	}
	*pExtension = Extension;
	return true;
}



/* 迭代严格的 WebSocket 扩展列表。 */
XRT_API xhttpnext xrtWsExtensionNext(
	xstrview Extensions,
	size_t* pOffset,
	xwsextension* pExtension
)
{
	xwsextension Extension;
	size_t iOriginal;
	size_t iNext;
	size_t iEnd;
	size_t i;

	if ( !__xrtWsExtensionViewValid(Extensions) ||
		!__xrtRangeValid(pOffset, sizeof(*pOffset)) ||
		!__xrtRangeValid(pExtension, sizeof(*pExtension)) ||
		__xrtRangesOverlap(
			pOffset, sizeof(*pOffset),
			Extensions.Data, Extensions.Size
		) || __xrtRangesOverlap(
			pExtension, sizeof(*pExtension),
			Extensions.Data, Extensions.Size
		) || __xrtRangesOverlap(
			pOffset, sizeof(*pOffset),
			pExtension, sizeof(*pExtension)
		) ) {
		__xrtWsHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"extension",
			"invalid WebSocket extension iterator arguments"
		);
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&iOriginal, pOffset, sizeof(iOriginal));
	if ( iOriginal > Extensions.Size ) {
		__xrtWsHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"extension",
			"WebSocket extension iterator offset is out of range"
		);
		return XHTTP_NEXT_ERROR;
	}
	i = iOriginal;

	/* HTTP #rule 允许逗号之间出现不计数的空成员。 */
	for ( ;; ) {
		while ( (i < Extensions.Size) &&
			__xrtWsExtensionOws(Extensions.Data[i]) ) {
			i++;
		}
		if ( i == Extensions.Size ) {
			if ( (iOriginal == 0) && (Extensions.Size != 0) ) {
				__xrtWsHandshakeError(
					XERR_PROTOCOL,
					XWS_HANDSHAKE_ERROR_EXTENSION,
					"extension",
					"WebSocket extension field contains no extension"
				);
				return XHTTP_NEXT_ERROR;
			}
			memcpy(pOffset, &i, sizeof(i));
			return XHTTP_NEXT_END;
		}
		if ( Extensions.Data[i] != ',' ) {
			break;
		}
		i++;
	}

	iEnd = __xrtWsExtensionItemEnd(Extensions, i);
	if ( !__xrtWsExtensionItemParse(
		Extensions, i, iEnd, &Extension
	) ) {
		return XHTTP_NEXT_ERROR;
	}
	iNext = (iEnd < Extensions.Size) ? (iEnd + 1u) : iEnd;
	memcpy(pExtension, &Extension, sizeof(Extension));
	memcpy(pOffset, &iNext, sizeof(iNext));
	return XHTTP_NEXT_ITEM;
}



/* 严格统计完整扩展列表。 */
XRT_API bool xrtWsExtensionCount(
	xstrview Extensions,
	size_t* pCount
)
{
	xwsextension Extension;
	xhttpnext Next;
	size_t iOffset = 0;
	size_t iCount = 0;

	if ( !__xrtWsExtensionViewValid(Extensions) ||
		!__xrtRangeValid(pCount, sizeof(*pCount)) ||
		__xrtRangesOverlap(
			pCount, sizeof(*pCount),
			Extensions.Data, Extensions.Size
		) ) {
		__xrtWsHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"extension-count",
			"invalid WebSocket extension count output"
		);
		return false;
	}
	for ( ;; ) {
		Next = xrtWsExtensionNext(
			Extensions, &iOffset, &Extension
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			memcpy(pCount, &iCount, sizeof(iCount));
			return true;
		}
		if ( iCount == SIZE_MAX ) {
			__xrtWsHandshakeError(
				XERR_RANGE,
				XWS_HANDSHAKE_ERROR_EXTENSION,
				"extension-count",
				"WebSocket extension count overflowed"
			);
			return false;
		}
		iCount++;
	}
}



/* 迭代扩展参数并验证 WebSocket 要求的 token 语义。 */
XRT_API xhttpnext xrtWsExtensionParamNext(
	const xwsextension* pExtension,
	size_t* pOffset,
	xhttpparam* pParam
)
{
	xwsextension Extension;
	xhttpparam Param;
	xhttpnext Next;
	size_t iOriginal;
	size_t iNext;

	if ( !__xrtRangeValid(pExtension, sizeof(*pExtension)) ||
		!__xrtRangeValid(pOffset, sizeof(*pOffset)) ||
		!__xrtRangeValid(pParam, sizeof(*pParam)) ||
		__xrtRangesOverlap(
			pOffset, sizeof(*pOffset),
			pExtension, sizeof(*pExtension)
		) || __xrtRangesOverlap(
			pParam, sizeof(*pParam),
			pExtension, sizeof(*pExtension)
		) || __xrtRangesOverlap(
			pOffset, sizeof(*pOffset),
			pParam, sizeof(*pParam)
		) ) {
		__xrtWsHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"extension-parameter",
			"invalid WebSocket extension parameter iterator arguments"
		);
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&Extension, pExtension, sizeof(Extension));
	if ( !__xrtWsExtensionViewValid(Extension.Name) ||
		!__xrtWsExtensionViewValid(Extension.Parameters) ||
		__xrtRangesOverlap(
			pOffset, sizeof(*pOffset),
			Extension.Name.Data, Extension.Name.Size
		) || __xrtRangesOverlap(
			pOffset, sizeof(*pOffset),
			Extension.Parameters.Data, Extension.Parameters.Size
		) || __xrtRangesOverlap(
			pParam, sizeof(*pParam),
			Extension.Name.Data, Extension.Name.Size
		) || __xrtRangesOverlap(
			pParam, sizeof(*pParam),
			Extension.Parameters.Data, Extension.Parameters.Size
		) ) {
		__xrtWsHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"extension-parameter",
			"invalid WebSocket extension parameter ranges"
		);
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&iOriginal, pOffset, sizeof(iOriginal));
	if ( iOriginal > Extension.Parameters.Size ) {
		__xrtWsHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"extension-parameter",
			"WebSocket extension parameter offset is out of range"
		);
		return XHTTP_NEXT_ERROR;
	}
	if ( !xrtHttpTokenValid(Extension.Name) ) {
		__xrtWsHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_EXTENSION,
			"extension-parameter",
			"WebSocket extension name is not a token"
		);
		return XHTTP_NEXT_ERROR;
	}
	iNext = iOriginal;
	Next = xrtHttpParamNext(
		Extension.Parameters,
		&iNext,
		&Param
	);
	if ( Next == XHTTP_NEXT_ERROR ) {
		__xrtWsHandshakeWrap(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_EXTENSION,
			"extension-parameter",
			"invalid WebSocket extension parameter syntax"
		);
		return XHTTP_NEXT_ERROR;
	}
	if ( (Next == XHTTP_NEXT_END) &&
		(iOriginal == 0) &&
		(Extension.Parameters.Size != 0) ) {
		__xrtWsHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_EXTENSION,
			"extension-parameter",
			"WebSocket extension parameter list is empty"
		);
		return XHTTP_NEXT_ERROR;
	}
	if ( (Next == XHTTP_NEXT_ITEM) &&
		((Param.Flags & XHTTP_PARAM_HAS_VALUE) != 0) &&
		!xrtHttpParamTokenValid(&Param) ) {
		__xrtWsHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_EXTENSION,
			"extension-parameter",
			"WebSocket extension parameter value is not a token"
		);
		return XHTTP_NEXT_ERROR;
	}
	if ( Next == XHTTP_NEXT_ITEM ) {
		memcpy(pParam, &Param, sizeof(Param));
	}
	memcpy(pOffset, &iNext, sizeof(iNext));
	return Next;
}



/* 写出一个已经验证的 WebSocket 扩展项。 */
XRT_API bool xrtWsExtensionWrite(
	xstrview Name,
	xstrview Parameters,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xwsextension Extension;
	uint8* pWrite = (uint8*)pOutput;
	size_t iCheckSize;
	size_t iSeparator = 0;
	size_t iRequired;

	if ( !__xrtRangeValid(pSize, sizeof(*pSize)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		!__xrtRangeValid(pOutput, iCapacity) ||
		!__xrtWsExtensionViewValid(Name) ||
		!__xrtWsExtensionViewValid(Parameters) ||
		__xrtRangesOverlap(
			pSize, sizeof(*pSize), Name.Data, Name.Size
		) || __xrtRangesOverlap(
			pSize, sizeof(*pSize),
			Parameters.Data, Parameters.Size
		) ) {
		__xrtWsHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"extension-write",
			"invalid WebSocket extension output arguments"
		);
		return false;
	}
	if ( !xrtHttpTokenValid(Name) ) {
		__xrtWsHandshakeError(
			XERR_VALUE,
			XWS_HANDSHAKE_ERROR_EXTENSION,
			"extension-write",
			"WebSocket extension name is not a token"
		);
		return false;
	}
	Extension.Name = Name;
	Extension.Parameters = Parameters;
	if ( (Parameters.Size != 0) &&
		!__xrtWsExtensionParametersValid(&Extension) ) {
		return false;
	}
	iRequired = Name.Size;
	if ( Parameters.Size != 0 ) {
		iSeparator = __xrtWsExtensionOws(
			Parameters.Data[0]
		) ? 1u : 2u;
		if ( (Parameters.Size > (SIZE_MAX - iSeparator)) ||
			(iRequired > (
				SIZE_MAX - Parameters.Size - iSeparator
			)) ) {
			__xrtWsHandshakeError(
				XERR_RANGE,
				XWS_HANDSHAKE_ERROR_OUTPUT,
				"extension-write",
				"WebSocket extension output size overflowed"
			);
			return false;
		}
		iRequired += Parameters.Size + iSeparator;
	}
	if ( pOutput == NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		return true;
	}
	iCheckSize = __xrtOutputCheckSize(iRequired, iCapacity);
	if ( !__xrtRangeValid(pOutput, iCheckSize) ||
		__xrtRangesOverlap(
			pSize, sizeof(*pSize), pOutput, iCheckSize
		) || __xrtRangesOverlap(
			Name.Data, Name.Size, pOutput, iCheckSize
		) || __xrtRangesOverlap(
			Parameters.Data, Parameters.Size,
			pOutput, iCheckSize
		) ) {
		__xrtWsHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"extension-write",
			"WebSocket extension output overlaps its inputs"
		);
		return false;
	}
	if ( iCapacity < iRequired ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		__xrtWsHandshakeError(
			XERR_RANGE,
			XWS_HANDSHAKE_ERROR_OUTPUT,
			"extension-write",
			"WebSocket extension output is too small"
		);
		return false;
	}
	memcpy(pWrite, Name.Data, Name.Size);
	if ( Parameters.Size != 0 ) {
		pWrite[Name.Size] = (uint8)';';
		if ( iSeparator == 2u ) {
			pWrite[Name.Size + 1u] = (uint8)' ';
		}
		memcpy(
			pWrite + Name.Size + iSeparator,
			Parameters.Data,
			Parameters.Size
		);
	}
	memcpy(pSize, &iRequired, sizeof(iRequired));
	return true;
}

#endif
