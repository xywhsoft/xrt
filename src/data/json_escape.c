#include "../internal/xrt_json.h"
#include "../internal/xrt_json_escape.h"



#if defined(XRT_FEATURE_JSON_ESCAPE)

/* 共享转义 Writer 保存调用方回调和精确输出长度。 */
typedef struct xjsonescapewriter {
	xjsonescapeemitproc Emit;
	ptr UserData;
	size_t Written;
} xjsonescapewriter;



/* 公共 Writer 桥接器保持回调和用户数据配对。 */
typedef struct xjsonquotebridge {
	xjsonwriteproc Write;
	ptr UserData;
} xjsonquotebridge;



/* 提交一段 token，并拒绝总长度溢出。 */
static xjsonescaperesult __xrtJsonEscapeEmit(
	xjsonescapewriter* pWriter,
	const void* pData,
	size_t iSize
)
{
	if ( iSize == 0 ) {
		return XJSON_ESCAPE_OK;
	}
	if ( pWriter->Written > (SIZE_MAX - iSize) ) {
		return XJSON_ESCAPE_OVERFLOW;
	}
	if ( !pWriter->Emit(pData, iSize, pWriter->UserData) ) {
		return XJSON_ESCAPE_OUTPUT;
	}
	pWriter->Written += iSize;
	return XJSON_ESCAPE_OK;
}



/* 写出四位大写 Unicode 转义。 */
static xjsonescaperesult __xrtJsonEscapeHex16(
	xjsonescapewriter* pWriter,
	uint32 iCode
)
{
	static const char sHex[] = "0123456789ABCDEF";
	char arrOutput[6];

	arrOutput[0] = '\\';
	arrOutput[1] = 'u';
	arrOutput[2] = sHex[(iCode >> 12) & UINT32_C(0x0F)];
	arrOutput[3] = sHex[(iCode >> 8) & UINT32_C(0x0F)];
	arrOutput[4] = sHex[(iCode >> 4) & UINT32_C(0x0F)];
	arrOutput[5] = sHex[iCode & UINT32_C(0x0F)];
	return __xrtJsonEscapeEmit(pWriter, arrOutput, sizeof(arrOutput));
}



/* 写出完整 JSON 字符串 token。 */
xjsonescaperesult __xrtJsonEscapeWrite(
	xstrview Text,
	uint32 iFlags,
	xjsonescapeemitproc pEmit,
	ptr pUserData,
	size_t* pWritten,
	size_t* pErrorOffset
)
{
	xjsonescapewriter Writer;
	xjsonescaperesult Result;
	size_t iOffset = 0;
	size_t iSegment = 0;
	size_t iError = 0;

	if ( pWritten != NULL ) {
		*pWritten = 0;
	}
	if ( pErrorOffset != NULL ) {
		*pErrorOffset = 0;
	}
	if (
		((Text.Data == NULL) && (Text.Size != 0)) ||
		(pEmit == NULL) ||
		!xrtUtf8Valid(Text, &iError)
	) {
		if ( pErrorOffset != NULL ) {
			*pErrorOffset = iError;
		}
		return XJSON_ESCAPE_INVALID;
	}
	Writer.Emit = pEmit;
	Writer.UserData = pUserData;
	Writer.Written = 0;
	Result = __xrtJsonEscapeEmit(&Writer, "\"", 1u);
	while ( (Result == XJSON_ESCAPE_OK) && (iOffset < Text.Size) ) {
		uint8 iByte = (uint8)Text.Data[iOffset];
		bool bEscape =
			(iByte < UINT8_C(0x20)) ||
			(iByte == (uint8)'\"') ||
			(iByte == (uint8)'\\') ||
			((iByte == (uint8)'/') &&
			 ((iFlags & XJSON_WRITE_ESCAPE_SLASH) != 0)) ||
			(((iByte == (uint8)'<') || (iByte == (uint8)'>') ||
			  (iByte == (uint8)'&')) &&
			 ((iFlags & XJSON_WRITE_ESCAPE_HTML) != 0)) ||
			((iByte >= UINT8_C(0x80)) &&
			 ((iFlags & XJSON_WRITE_ESCAPE_NON_ASCII) != 0));

		if ( !bEscape ) {
			iOffset++;
			continue;
		}
		if ( iOffset > iSegment ) {
			Result = __xrtJsonEscapeEmit(
				&Writer,
				Text.Data + iSegment,
				iOffset - iSegment
			);
			if ( Result != XJSON_ESCAPE_OK ) {
				break;
			}
		}
		if ( iByte >= UINT8_C(0x80) ) {
			uint32 iScalar;
			size_t iRead;

			if (
				xrtUtf8Decode(
					(xstrview){ Text.Data + iOffset, Text.Size - iOffset },
					&iScalar,
					&iRead
				) != XUTF_OK
			) {
				if ( pErrorOffset != NULL ) {
					*pErrorOffset = iOffset;
				}
				return XJSON_ESCAPE_INVALID;
			}
			if ( iScalar <= UINT32_C(0xFFFF) ) {
				Result = __xrtJsonEscapeHex16(&Writer, iScalar);
			} else {
				uint32 iPair = iScalar - UINT32_C(0x10000);

				Result = __xrtJsonEscapeHex16(
					&Writer,
					UINT32_C(0xD800) + (iPair >> 10)
				);
				if ( Result == XJSON_ESCAPE_OK ) {
					Result = __xrtJsonEscapeHex16(
						&Writer,
						UINT32_C(0xDC00) +
						(iPair & UINT32_C(0x3FF))
					);
				}
			}
			iOffset += iRead;
			iSegment = iOffset;
			continue;
		}
		if (
			(iByte == (uint8)'<') || (iByte == (uint8)'>') ||
			(iByte == (uint8)'&')
		) {
			Result = __xrtJsonEscapeHex16(&Writer, iByte);
		} else if ( iByte == (uint8)'\b' ) {
			Result = __xrtJsonEscapeEmit(&Writer, "\\b", 2u);
		} else if ( iByte == (uint8)'\f' ) {
			Result = __xrtJsonEscapeEmit(&Writer, "\\f", 2u);
		} else if ( iByte == (uint8)'\n' ) {
			Result = __xrtJsonEscapeEmit(&Writer, "\\n", 2u);
		} else if ( iByte == (uint8)'\r' ) {
			Result = __xrtJsonEscapeEmit(&Writer, "\\r", 2u);
		} else if ( iByte == (uint8)'\t' ) {
			Result = __xrtJsonEscapeEmit(&Writer, "\\t", 2u);
		} else if ( iByte < UINT8_C(0x20) ) {
			Result = __xrtJsonEscapeHex16(&Writer, iByte);
		} else {
			char arrEscape[2] = { '\\', (char)iByte };

			Result = __xrtJsonEscapeEmit(
				&Writer,
				arrEscape,
				sizeof(arrEscape)
			);
		}
		iOffset++;
		iSegment = iOffset;
	}
	if (
		(Result == XJSON_ESCAPE_OK) &&
		(Text.Size > iSegment)
	) {
		Result = __xrtJsonEscapeEmit(
			&Writer,
			Text.Data + iSegment,
			Text.Size - iSegment
		);
	}
	if ( Result == XJSON_ESCAPE_OK ) {
		Result = __xrtJsonEscapeEmit(&Writer, "\"", 1u);
	}
	if ( pWritten != NULL ) {
		*pWritten = Writer.Written;
	}
	return Result;
}



/* 把公共字节 Writer 适配到共享 quote 核心。 */
static bool __xrtJsonQuoteEmit(
	const void* pData,
	size_t iSize,
	ptr pUserData
)
{
	xjsonquotebridge* pBridge = (xjsonquotebridge*)pUserData;

	return pBridge->Write(
		(xbytesview){ (cbytes)pData, iSize },
		pBridge->UserData
	);
}



/* 严格校验 UTF-8 并流式写出 JSON 字符串 token。 */
XRT_API bool xrtJsonQuoteWrite(
	xstrview Text,
	uint32 iFlags,
	xjsonwriteproc pWrite,
	ptr pUserData,
	size_t* pWritten
)
{
	xjsonquotebridge Bridge;
	xjsonescaperesult Result;
	xerror* pPrevious;
	size_t iErrorOffset = 0;
	const uint32 iKnown =
		XJSON_WRITE_ESCAPE_SLASH |
		XJSON_WRITE_ESCAPE_HTML |
		XJSON_WRITE_ESCAPE_NON_ASCII;

	if ( pWritten != NULL ) {
		*pWritten = 0;
	}
	if ( (pWrite == NULL) || ((iFlags & ~iKnown) != 0) ) {
		__xrtJsonError(
			XERR_ARGUMENT,
			XJSON_ERROR_CONFIG,
			"quote",
			"invalid JSON quote configuration",
			NULL
		);
		return false;
	}
	Bridge.Write = pWrite;
	Bridge.UserData = pUserData;
	pPrevious = xrtErrorRef(xrtGetError());
	xrtClearError();
	Result = __xrtJsonEscapeWrite(
		Text,
		iFlags,
		__xrtJsonQuoteEmit,
		&Bridge,
		pWritten,
		&iErrorOffset
	);
	if ( Result == XJSON_ESCAPE_OK ) {
		__xrtErrorSetOwned(pPrevious);
		return true;
	}
	xrtErrorFree(pPrevious);
	if ( Result == XJSON_ESCAPE_INVALID ) {
		xjsonlocation Location = { iErrorOffset, 1u, iErrorOffset + 1u };

		__xrtJsonError(
			XERR_VALUE,
			XJSON_ERROR_UNSUPPORTED,
			"quote",
			"string is not valid UTF-8",
			&Location
		);
	} else if ( Result == XJSON_ESCAPE_OVERFLOW ) {
		__xrtErrorSetSizeOverflow();
	} else if ( xrtGetError() == NULL ) {
		__xrtJsonError(
			XERR_IO,
			XJSON_ERROR_OUTPUT,
			"quote",
			"JSON quote output callback failed",
			NULL
		);
	}
	return false;
}

#endif
