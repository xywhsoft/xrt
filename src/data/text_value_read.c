#include "../internal/xrt_text_value.h"

#include <math.h>



#if defined(XRT_FEATURE_JSON_READ) || defined(XRT_FEATURE_XSON_READ)

/* 单次解析持有显式长度游标、资源预算和两个互不覆盖的反转义缓冲。 */
typedef struct xtextvalueparser {
	xstrview Text;
	xtextvaluereadconfig Config;
	size_t Offset;
	size_t Line;
	size_t Column;
	size_t Values;
	xbuffer NameBuffer;
	xbuffer StringBuffer;
	xtextvaluevisitproc Visitor;
	ptr VisitorData;
	xtextvalueerrorproc Error;
	ptr ErrorData;
	bool DecodeStrings;
	bool Stopped;
} xtextvalueparser;



/* 返回当前位置的稳定文本坐标。 */
static xtextvaluelocation __xrtTextValueParserLocation(const xtextvalueparser* pParser)
{
	xtextvaluelocation Location;

	Location.Offset = pParser->Offset;
	Location.Line = pParser->Line;
	Location.Column = pParser->Column;
	return Location;
}



/* 在解析当前位置设置 JSON 错误。 */
static void __xrtTextValueParserError(
	xtextvalueparser* pParser,
	xerrkind Kind,
	xtextvalueerror Code,
	cstr sMessage
)
{
	xtextvaluelocation Location = __xrtTextValueParserLocation(pParser);

	pParser->Error(Kind, Code, sMessage, &Location, pParser->ErrorData);
}



/* 在指定位置设置 JSON 错误。 */
static void __xrtTextValueLocationError(
	xtextvalueparser* pParser,
	xerrkind Kind,
	xtextvalueerror Code,
	cstr sMessage,
	const xtextvaluelocation* pLocation
)
{
	pParser->Error(Kind, Code, sMessage, pLocation, pParser->ErrorData);
}



/* 返回当前位置是否已经到达显式输入末尾。 */
static bool __xrtTextValueParserEnd(const xtextvalueparser* pParser)
{
	return pParser->Offset >= pParser->Text.Size;
}



/* 借用当前位置字节，调用方必须先检查输入未结束。 */
static uint8 __xrtTextValueParserPeek(const xtextvalueparser* pParser)
{
	return (uint8)pParser->Text.Data[pParser->Offset];
}



/* 消费一个字节并维护 CR、LF 与 CRLF 的一基行列。 */
static void __xrtTextValueParserAdvance(xtextvalueparser* pParser)
{
	uint8 iByte = __xrtTextValueParserPeek(pParser);
	bool bPreviousCr =
		(pParser->Offset > 0) &&
		((uint8)pParser->Text.Data[pParser->Offset - 1u] == (uint8)'\r');

	pParser->Offset++;
	if ( iByte == (uint8)'\r' ) {
		pParser->Line++;
		pParser->Column = 1;
	} else if ( iByte == (uint8)'\n' ) {
		if ( !bPreviousCr ) {
			pParser->Line++;
		}
		pParser->Column = 1;
	} else {
		pParser->Column++;
	}
}



/* 消费一段已知不含换行的字符串内容。 */
static void __xrtTextValueParserAdvanceText(
	xtextvalueparser* pParser,
	size_t iSize
)
{
	pParser->Offset += iSize;
	pParser->Column += iSize;
}



/* 返回 JSON 四种空白字节。 */
static bool __xrtTextValueSpace(uint8 iByte)
{
	return
		(iByte == (uint8)' ') ||
		(iByte == (uint8)'\t') ||
		(iByte == (uint8)'\r') ||
		(iByte == (uint8)'\n');
}



/* 跳过空白和显式允许的 C/C++ 风格注释。 */
static bool __xrtTextValueParserSpace(xtextvalueparser* pParser)
{
	for ( ;; ) {
		while (
			!__xrtTextValueParserEnd(pParser) &&
			__xrtTextValueSpace(__xrtTextValueParserPeek(pParser))
		) {
			__xrtTextValueParserAdvance(pParser);
		}
		if (
			((pParser->Config.Flags & XTEXT_VALUE_READ_COMMENTS) == 0) ||
			((pParser->Text.Size - pParser->Offset) < 2u) ||
			(__xrtTextValueParserPeek(pParser) != (uint8)'/')
		) {
			return true;
		}
		if ( (uint8)pParser->Text.Data[pParser->Offset + 1u] == (uint8)'/' ) {
			__xrtTextValueParserAdvance(pParser);
			__xrtTextValueParserAdvance(pParser);
			while (
				!__xrtTextValueParserEnd(pParser) &&
				(__xrtTextValueParserPeek(pParser) != (uint8)'\r') &&
				(__xrtTextValueParserPeek(pParser) != (uint8)'\n')
			) {
				__xrtTextValueParserAdvance(pParser);
			}
			continue;
		}
		if ( (uint8)pParser->Text.Data[pParser->Offset + 1u] == (uint8)'*' ) {
			__xrtTextValueParserAdvance(pParser);
			__xrtTextValueParserAdvance(pParser);
			while ( (pParser->Text.Size - pParser->Offset) >= 2u ) {
				if (
					(__xrtTextValueParserPeek(pParser) == (uint8)'*') &&
					((uint8)pParser->Text.Data[pParser->Offset + 1u] == (uint8)'/')
				) {
					__xrtTextValueParserAdvance(pParser);
					__xrtTextValueParserAdvance(pParser);
					break;
				}
				__xrtTextValueParserAdvance(pParser);
			}
			if (
				(pParser->Offset < 2u) ||
				((uint8)pParser->Text.Data[pParser->Offset - 2u] != (uint8)'*') ||
				((uint8)pParser->Text.Data[pParser->Offset - 1u] != (uint8)'/')
			) {
				__xrtTextValueParserError(
					pParser,
					XERR_PROTOCOL,
					XTEXT_VALUE_ERROR_SYNTAX,
					"unterminated JSON comment"
				);
				return false;
			}
			continue;
		}
		return true;
	}
}



/* 验证内部配置由格式适配器完整初始化。 */
static bool __xrtTextValueReadConfigValid(
	const xtextvaluereadconfig* pConfig
)
{
	uint32 iKnownFlags = XTEXT_VALUE_READ_COMMENTS | XTEXT_VALUE_READ_TRAILING_COMMA;

	return
		(pConfig != NULL) &&
		((pConfig->Dialect == XTEXT_VALUE_JSON) ||
		 (pConfig->Dialect == XTEXT_VALUE_XSON)) &&
		((pConfig->Flags & ~iKnownFlags) == 0) &&
		(pConfig->MaxDepth != 0) &&
		(pConfig->MaxDepth <= XRT_VALUE_DEPTH_MAX) &&
		(pConfig->MaxInputBytes != 0) &&
		(pConfig->MaxStringBytes != 0) &&
		(pConfig->MaxValues != 0) &&
		(pConfig->MaxContainerItems != 0);
}



/* 把一个十六进制 ASCII 字节转换为数值。 */
static int __xrtTextValueHex(uint8 iByte)
{
	if ( (iByte >= (uint8)'0') && (iByte <= (uint8)'9') ) {
		return (int)(iByte - (uint8)'0');
	}
	if ( (iByte >= (uint8)'a') && (iByte <= (uint8)'f') ) {
		return (int)(iByte - (uint8)'a') + 10;
	}
	if ( (iByte >= (uint8)'A') && (iByte <= (uint8)'F') ) {
		return (int)(iByte - (uint8)'A') + 10;
	}
	return -1;
}



/* 从当前位置读取四位 UTF-16 转义码元。 */
static bool __xrtTextValueParserHex16(xtextvalueparser* pParser, uint32* pCode)
{
	uint32 iCode = 0;

	if ( (pParser->Text.Size - pParser->Offset) < 4u ) {
		__xrtTextValueParserError(
			pParser,
			XERR_PROTOCOL,
			XTEXT_VALUE_ERROR_SYNTAX,
			"incomplete JSON Unicode escape"
		);
		return false;
	}
	for ( size_t i = 0; i < 4u; i++ ) {
		int iDigit = __xrtTextValueHex(
			(uint8)pParser->Text.Data[pParser->Offset + i]
		);

		if ( iDigit < 0 ) {
			__xrtTextValueParserError(
				pParser,
				XERR_PROTOCOL,
				XTEXT_VALUE_ERROR_SYNTAX,
				"invalid JSON Unicode escape"
			);
			return false;
		}
		iCode = (iCode << 4) | (uint32)iDigit;
	}
	__xrtTextValueParserAdvanceText(pParser, 4u);
	*pCode = iCode;
	return true;
}



/* 在字符串预算内增加解码字节数。 */
static bool __xrtTextValueStringCount(
	xtextvalueparser* pParser,
	size_t* pSize,
	size_t iAdd
)
{
	if (
		(iAdd > pParser->Config.MaxStringBytes) ||
		(*pSize > (pParser->Config.MaxStringBytes - iAdd))
	) {
		__xrtTextValueParserError(
			pParser,
			XERR_RANGE,
			XTEXT_VALUE_ERROR_LIMIT,
			"JSON string exceeds configured limit"
		);
		return false;
	}
	*pSize += iAdd;
	return true;
}



/* 在需要解码时向字符串临时缓冲追加字节。 */
static bool __xrtTextValueStringAppend(
	xbuffer* pBuffer,
	const void* pData,
	size_t iSize,
	bool bDecode
)
{
	if ( !bDecode || (iSize == 0) ) {
		return true;
	}
	return xrtBufferAppend(
		pBuffer,
		(xbytesview){ (cbytes)pData, iSize }
	);
}



/* 严格读取 JSON 字符串，按需返回借用输入或反转义缓冲视图。 */
static bool __xrtTextValueParserString(
	xtextvalueparser* pParser,
	xbuffer* pBuffer,
	bool bDecode,
	xstrview* pText
)
{
	size_t iStart;
	size_t iDecoded = 0;
	bool bEscaped = false;

	if (
		__xrtTextValueParserEnd(pParser) ||
		(__xrtTextValueParserPeek(pParser) != (uint8)'\"')
	) {
		__xrtTextValueParserError(
			pParser,
			XERR_PROTOCOL,
			XTEXT_VALUE_ERROR_SYNTAX,
			"expected JSON string"
		);
		return false;
	}
	xrtBufferClear(pBuffer);
	__xrtTextValueParserAdvance(pParser);
	iStart = pParser->Offset;

	for ( ;; ) {
		size_t iSegment = pParser->Offset;
		size_t iError = 0;

		while ( !__xrtTextValueParserEnd(pParser) ) {
			uint8 iByte = __xrtTextValueParserPeek(pParser);

			if ( (iByte == (uint8)'\"') || (iByte == (uint8)'\\') ||
				 (iByte < UINT8_C(0x20)) ) {
				break;
			}
			__xrtTextValueParserAdvanceText(pParser, 1u);
		}
		if ( pParser->Offset > iSegment ) {
			xstrview Segment = {
				pParser->Text.Data + iSegment,
				pParser->Offset - iSegment
			};

			if ( !xrtUtf8Valid(Segment, &iError) ) {
				xtextvaluelocation Location = __xrtTextValueParserLocation(pParser);

				Location.Offset = iSegment + iError;
				Location.Column -= Segment.Size - iError;
				__xrtTextValueLocationError(
					pParser,
					XERR_VALUE,
					XTEXT_VALUE_ERROR_SYNTAX,
					"string contains invalid UTF-8",
					&Location
				);
				return false;
			}
			if (
				!__xrtTextValueStringCount(pParser, &iDecoded, Segment.Size) ||
				(bEscaped && !__xrtTextValueStringAppend(
					pBuffer,
					Segment.Data,
					Segment.Size,
					bDecode
				))
			) {
				return false;
			}
		}
		if ( __xrtTextValueParserEnd(pParser) ) {
			__xrtTextValueParserError(
				pParser,
				XERR_PROTOCOL,
				XTEXT_VALUE_ERROR_SYNTAX,
				"unterminated JSON string"
			);
			return false;
		}
		if ( __xrtTextValueParserPeek(pParser) == (uint8)'\"' ) {
			__xrtTextValueParserAdvance(pParser);
			if ( bEscaped ) {
				pText->Data = (cstr)pBuffer->Data;
				pText->Size = pBuffer->Size;
			} else {
				pText->Data = pParser->Text.Data + iStart;
				pText->Size = iDecoded;
			}
			return true;
		}
		if ( __xrtTextValueParserPeek(pParser) < UINT8_C(0x20) ) {
			__xrtTextValueParserError(
				pParser,
				XERR_PROTOCOL,
				XTEXT_VALUE_ERROR_SYNTAX,
				"unescaped control byte in JSON string"
			);
			return false;
		}

		/* 首个转义出现时补入此前保持借用的全部原始内容。 */
		if ( !bEscaped ) {
			if ( !__xrtTextValueStringAppend(
				pBuffer,
				pParser->Text.Data + iStart,
				iDecoded,
				bDecode
			) ) {
				return false;
			}
			bEscaped = true;
		}
		__xrtTextValueParserAdvance(pParser);
		if ( __xrtTextValueParserEnd(pParser) ) {
			__xrtTextValueParserError(
				pParser,
				XERR_PROTOCOL,
				XTEXT_VALUE_ERROR_SYNTAX,
				"incomplete JSON escape"
			);
			return false;
		}

		/* 简单转义直接映射为单字节 UTF-8。 */
		{
			uint8 iEscape = __xrtTextValueParserPeek(pParser);
			uint8 iOutput = 0;

			if ( iEscape == (uint8)'u' ) {
				uint32 iScalar;
				char Output[4];
				size_t iOutputSize;

				__xrtTextValueParserAdvance(pParser);
				if ( !__xrtTextValueParserHex16(pParser, &iScalar) ) {
					return false;
				}
				if ( (iScalar >= UINT32_C(0xD800)) &&
					 (iScalar <= UINT32_C(0xDBFF)) ) {
					uint32 iLow;

					if (
						((pParser->Text.Size - pParser->Offset) < 6u) ||
						((uint8)pParser->Text.Data[pParser->Offset] != (uint8)'\\') ||
						((uint8)pParser->Text.Data[pParser->Offset + 1u] != (uint8)'u')
					) {
						__xrtTextValueParserError(
							pParser,
							XERR_PROTOCOL,
							XTEXT_VALUE_ERROR_SYNTAX,
							"JSON high surrogate requires a low surrogate"
						);
						return false;
					}
					__xrtTextValueParserAdvance(pParser);
					__xrtTextValueParserAdvance(pParser);
					if ( !__xrtTextValueParserHex16(pParser, &iLow) ) {
						return false;
					}
					if ( (iLow < UINT32_C(0xDC00)) ||
						 (iLow > UINT32_C(0xDFFF)) ) {
						__xrtTextValueParserError(
							pParser,
							XERR_PROTOCOL,
							XTEXT_VALUE_ERROR_SYNTAX,
							"invalid JSON low surrogate"
						);
						return false;
					}
					iScalar = UINT32_C(0x10000) +
						((iScalar - UINT32_C(0xD800)) << 10) +
						(iLow - UINT32_C(0xDC00));
				} else if ( (iScalar >= UINT32_C(0xDC00)) &&
							(iScalar <= UINT32_C(0xDFFF)) ) {
					__xrtTextValueParserError(
						pParser,
						XERR_PROTOCOL,
						XTEXT_VALUE_ERROR_SYNTAX,
						"JSON low surrogate has no high surrogate"
					);
					return false;
				}
				iOutputSize = xrtUtf8Encode(iScalar, Output);
				if (
					(iOutputSize == 0) ||
					!__xrtTextValueStringCount(pParser, &iDecoded, iOutputSize) ||
					!__xrtTextValueStringAppend(
						pBuffer,
						Output,
						iOutputSize,
						bDecode
					)
				) {
					return false;
				}
				continue;
			}
			if ( iEscape == (uint8)'\"' ) {
				iOutput = (uint8)'\"';
			} else if ( iEscape == (uint8)'\\' ) {
				iOutput = (uint8)'\\';
			} else if ( iEscape == (uint8)'/' ) {
				iOutput = (uint8)'/';
			} else if ( iEscape == (uint8)'b' ) {
				iOutput = (uint8)'\b';
			} else if ( iEscape == (uint8)'f' ) {
				iOutput = (uint8)'\f';
			} else if ( iEscape == (uint8)'n' ) {
				iOutput = (uint8)'\n';
			} else if ( iEscape == (uint8)'r' ) {
				iOutput = (uint8)'\r';
			} else if ( iEscape == (uint8)'t' ) {
				iOutput = (uint8)'\t';
			} else {
				__xrtTextValueParserError(
					pParser,
					XERR_PROTOCOL,
					XTEXT_VALUE_ERROR_SYNTAX,
					"invalid JSON escape"
				);
				return false;
			}
			__xrtTextValueParserAdvance(pParser);
			if (
				!__xrtTextValueStringCount(pParser, &iDecoded, 1u) ||
				!__xrtTextValueStringAppend(
					pBuffer,
					&iOutput,
					1u,
					bDecode
				)
			) {
				return false;
			}
		}
	}
}



/* 向访问器提交事件并记录正常停止状态。 */
static bool __xrtTextValueParserEmit(
	xtextvalueparser* pParser,
	const xtextvalueevent* pEvent
)
{
	const xerror* pPrevious;
	xerror* pHeld;
	xtextvaluevisitaction Action;

	if ( pParser->Visitor == NULL ) {
		return true;
	}
	pPrevious = xrtGetError();
	pHeld = xrtErrorRef(pPrevious);
	Action = pParser->Visitor(pEvent, pParser->VisitorData);
	if ( Action == XTEXT_VALUE_VISIT_NEXT ) {
		xrtErrorFree(pHeld);
		return true;
	}
	if ( Action == XTEXT_VALUE_VISIT_STOP ) {
		xrtErrorFree(pHeld);
		pParser->Stopped = true;
		return false;
	}
	if ( xrtGetError() == pPrevious ) {
		__xrtTextValueLocationError(
			pParser,
			XERR_STATE,
			XTEXT_VALUE_ERROR_STATE,
			"text value visitor reported failure",
			&pEvent->Location
		);
	}
	xrtErrorFree(pHeld);
	return false;
}



/* XSON 标签首字节只接受 ASCII 字母或下划线。 */
static bool __xrtTextValueIdentifierStart(uint8 iByte)
{
	return
		((iByte >= (uint8)'a') && (iByte <= (uint8)'z')) ||
		((iByte >= (uint8)'A') && (iByte <= (uint8)'Z')) ||
		(iByte == (uint8)'_');
}



/* 标签后续字节额外接受数字、点和连字符，便于建立命名空间。 */
static bool __xrtTextValueIdentifierByte(uint8 iByte)
{
	return
		__xrtTextValueIdentifierStart(iByte) ||
		((iByte >= (uint8)'0') && (iByte <= (uint8)'9')) ||
		(iByte == (uint8)'.') ||
		(iByte == (uint8)'-');
}



/* 判断当前位置是否是带完整 token 边界的固定字面量。 */
static bool __xrtTextValueParserMatches(
	const xtextvalueparser* pParser,
	cstr sLiteral,
	size_t iSize
)
{
	size_t iEnd = pParser->Offset + iSize;

	if (
		((pParser->Text.Size - pParser->Offset) < iSize) ||
		(memcmp(pParser->Text.Data + pParser->Offset, sLiteral, iSize) != 0)
	) {
		return false;
	}
	return
		(iEnd == pParser->Text.Size) ||
		!__xrtTextValueIdentifierByte((uint8)pParser->Text.Data[iEnd]);
}



/* 消费已经匹配并验证边界的固定字面量。 */
static void __xrtTextValueParserLiteral(
	xtextvalueparser* pParser,
	size_t iSize
)
{
	__xrtTextValueParserAdvanceText(pParser, iSize);
}



/* 扫描严格 JSON 数字 token 并解析为 int64 或 double。 */
static bool __xrtTextValueParserNumber(
	xtextvalueparser* pParser,
	xtextvalueevent* pEvent
)
{
	size_t iStart = pParser->Offset;
	bool bInteger = true;
	int64 iInteger = 0;
	double fValue = 0.0;

	if ( __xrtTextValueParserPeek(pParser) == (uint8)'-' ) {
		__xrtTextValueParserAdvanceText(pParser, 1u);
		if ( __xrtTextValueParserEnd(pParser) ) {
			goto format_error;
		}
	}
	if ( __xrtTextValueParserPeek(pParser) == (uint8)'0' ) {
		__xrtTextValueParserAdvanceText(pParser, 1u);
		if (
			!__xrtTextValueParserEnd(pParser) &&
			(__xrtTextValueParserPeek(pParser) >= (uint8)'0') &&
			(__xrtTextValueParserPeek(pParser) <= (uint8)'9')
		) {
			goto format_error;
		}
	} else if (
		(__xrtTextValueParserPeek(pParser) >= (uint8)'1') &&
		(__xrtTextValueParserPeek(pParser) <= (uint8)'9')
	) {
		do {
			__xrtTextValueParserAdvanceText(pParser, 1u);
		} while (
			!__xrtTextValueParserEnd(pParser) &&
			(__xrtTextValueParserPeek(pParser) >= (uint8)'0') &&
			(__xrtTextValueParserPeek(pParser) <= (uint8)'9')
		);
	} else {
		goto format_error;
	}
	if (
		!__xrtTextValueParserEnd(pParser) &&
		(__xrtTextValueParserPeek(pParser) == (uint8)'.')
	) {
		bInteger = false;
		__xrtTextValueParserAdvanceText(pParser, 1u);
		if (
			__xrtTextValueParserEnd(pParser) ||
			(__xrtTextValueParserPeek(pParser) < (uint8)'0') ||
			(__xrtTextValueParserPeek(pParser) > (uint8)'9')
		) {
			goto format_error;
		}
		do {
			__xrtTextValueParserAdvanceText(pParser, 1u);
		} while (
			!__xrtTextValueParserEnd(pParser) &&
			(__xrtTextValueParserPeek(pParser) >= (uint8)'0') &&
			(__xrtTextValueParserPeek(pParser) <= (uint8)'9')
		);
	}
	if (
		!__xrtTextValueParserEnd(pParser) &&
		((__xrtTextValueParserPeek(pParser) == (uint8)'e') ||
		 (__xrtTextValueParserPeek(pParser) == (uint8)'E'))
	) {
		bInteger = false;
		__xrtTextValueParserAdvanceText(pParser, 1u);
		if (
			!__xrtTextValueParserEnd(pParser) &&
			((__xrtTextValueParserPeek(pParser) == (uint8)'+') ||
			 (__xrtTextValueParserPeek(pParser) == (uint8)'-'))
		) {
			__xrtTextValueParserAdvanceText(pParser, 1u);
		}
		if (
			__xrtTextValueParserEnd(pParser) ||
			(__xrtTextValueParserPeek(pParser) < (uint8)'0') ||
			(__xrtTextValueParserPeek(pParser) > (uint8)'9')
		) {
			goto format_error;
		}
		do {
			__xrtTextValueParserAdvanceText(pParser, 1u);
		} while (
			!__xrtTextValueParserEnd(pParser) &&
			(__xrtTextValueParserPeek(pParser) >= (uint8)'0') &&
			(__xrtTextValueParserPeek(pParser) <= (uint8)'9')
		);
	}
	pEvent->Raw.Data = pParser->Text.Data + iStart;
	pEvent->Raw.Size = pParser->Offset - iStart;
	if ( bInteger && xrtIntParse(pEvent->Raw, 10u, 0, &iInteger) ) {
		pEvent->Type = XTEXT_VALUE_EVENT_INT;
		pEvent->Value.Integer = iInteger;
		return true;
	}
	if ( bInteger && !pParser->Config.BigIntegerFloat ) {
		xrtClearError();
		__xrtTextValueLocationError(
			pParser,
			XERR_RANGE,
			XTEXT_VALUE_ERROR_NUMBER,
			"integer does not fit int64",
			&pEvent->Location
		);
		return false;
	}
	xrtClearError();
	if ( !xrtNumParse(pEvent->Raw, 0, &fValue) || !isfinite(fValue) ) {
		xrtClearError();
		__xrtTextValueLocationError(
			pParser,
			XERR_RANGE,
			XTEXT_VALUE_ERROR_NUMBER,
			"number does not fit double",
			&pEvent->Location
		);
		return false;
	}
	pEvent->Type = XTEXT_VALUE_EVENT_FLOAT;
	pEvent->Value.Float = fValue;
	return true;

format_error:
	__xrtTextValueParserError(
		pParser,
		XERR_PROTOCOL,
		XTEXT_VALUE_ERROR_SYNTAX,
		"invalid JSON number"
	);
	return false;
}



/* 前置声明递归值解析入口，供容器解析器调用。 */
static bool __xrtTextValueParserValue(
	xtextvalueparser* pParser,
	size_t iDepth,
	const xvaluekey* pKey
);



/* 解析数组或集合，并直接发送开始、子项和结束事件。 */
static bool __xrtTextValueParserSequence(
	xtextvalueparser* pParser,
	xtextvalueevent* pEvent,
	xtextvalueeventtype BeginType,
	xtextvalueeventtype EndType,
	bool bIndexed
)
{
	size_t iItems = 0;
	bool bTrailingComma = false;

	pEvent->Type = BeginType;
	__xrtTextValueParserAdvance(pParser);
	if ( !__xrtTextValueParserEmit(pParser, pEvent) ||
		 !__xrtTextValueParserSpace(pParser) ) {
		return false;
	}
	if (
		!__xrtTextValueParserEnd(pParser) &&
		(__xrtTextValueParserPeek(pParser) == (uint8)']')
	) {
		xtextvalueevent EndEvent;

		memset(&EndEvent, 0, sizeof(EndEvent));
		EndEvent.Type = EndType;
		EndEvent.Depth = pEvent->Depth;
		EndEvent.Location = __xrtTextValueParserLocation(pParser);
		__xrtTextValueParserAdvance(pParser);
		return __xrtTextValueParserEmit(pParser, &EndEvent);
	}

	for ( ;; ) {
		xvaluekey Key;

		if ( iItems >= pParser->Config.MaxContainerItems ) {
			__xrtTextValueParserError(
				pParser,
				XERR_RANGE,
				XTEXT_VALUE_ERROR_LIMIT,
				"sequence exceeds configured item limit"
			);
			return false;
		}
		memset(&Key, 0, sizeof(Key));
		if ( bIndexed ) {
			Key.Type = XVALUE_KEY_INDEX;
			Key.Index = iItems;
		}
		if ( !__xrtTextValueParserValue(
			pParser,
			pEvent->Depth + 1u,
			&Key
		) ) {
			return false;
		}
		iItems++;
		if ( !__xrtTextValueParserSpace(pParser) || __xrtTextValueParserEnd(pParser) ) {
			break;
		}
		if ( __xrtTextValueParserPeek(pParser) == (uint8)']' ) {
			break;
		}
		if ( __xrtTextValueParserPeek(pParser) != (uint8)',' ) {
			break;
		}
		__xrtTextValueParserAdvance(pParser);
		bTrailingComma = true;
		if ( !__xrtTextValueParserSpace(pParser) ) {
			return false;
		}
		if (
			!__xrtTextValueParserEnd(pParser) &&
			(__xrtTextValueParserPeek(pParser) == (uint8)']')
		) {
			if (
				(pParser->Config.Flags & XTEXT_VALUE_READ_TRAILING_COMMA) == 0
			) {
				break;
			}
			break;
		}
		bTrailingComma = false;
	}
	if (
		__xrtTextValueParserEnd(pParser) ||
		(__xrtTextValueParserPeek(pParser) != (uint8)']')
	) {
		__xrtTextValueParserError(
			pParser,
			XERR_PROTOCOL,
			XTEXT_VALUE_ERROR_SYNTAX,
			"expected comma or end of sequence"
		);
		return false;
	}
	if (
		bTrailingComma &&
		((pParser->Config.Flags & XTEXT_VALUE_READ_TRAILING_COMMA) == 0)
	) {
		__xrtTextValueParserError(
			pParser,
			XERR_PROTOCOL,
			XTEXT_VALUE_ERROR_SYNTAX,
			"trailing comma is disabled"
		);
		return false;
	}
	{
		xtextvalueevent EndEvent;

		memset(&EndEvent, 0, sizeof(EndEvent));
		EndEvent.Type = EndType;
		EndEvent.Depth = pEvent->Depth;
		EndEvent.Location = __xrtTextValueParserLocation(pParser);
		__xrtTextValueParserAdvance(pParser);
		return __xrtTextValueParserEmit(pParser, &EndEvent);
	}
}



/* 解析字符串键对象或整数键映射。 */
static bool __xrtTextValueParserMapping(
	xtextvalueparser* pParser,
	xtextvalueevent* pEvent,
	xtextvalueeventtype BeginType,
	xtextvalueeventtype EndType,
	bool bIntegerKeys
)
{
	size_t iItems = 0;
	bool bTrailingComma = false;

	pEvent->Type = BeginType;
	__xrtTextValueParserAdvance(pParser);
	if ( !__xrtTextValueParserEmit(pParser, pEvent) ||
		 !__xrtTextValueParserSpace(pParser) ) {
		return false;
	}
	if (
		!__xrtTextValueParserEnd(pParser) &&
		(__xrtTextValueParserPeek(pParser) == (uint8)'}')
	) {
		xtextvalueevent EndEvent;

		memset(&EndEvent, 0, sizeof(EndEvent));
		EndEvent.Type = EndType;
		EndEvent.Depth = pEvent->Depth;
		EndEvent.Location = __xrtTextValueParserLocation(pParser);
		__xrtTextValueParserAdvance(pParser);
		return __xrtTextValueParserEmit(pParser, &EndEvent);
	}

	for ( ;; ) {
		xvaluekey Key;

		if ( iItems >= pParser->Config.MaxContainerItems ) {
			__xrtTextValueParserError(
				pParser,
				XERR_RANGE,
				XTEXT_VALUE_ERROR_LIMIT,
				"mapping exceeds configured item limit"
			);
			return false;
		}
		memset(&Key, 0, sizeof(Key));
		if ( bIntegerKeys ) {
			xtextvalueevent KeyEvent;

			memset(&KeyEvent, 0, sizeof(KeyEvent));
			KeyEvent.Location = __xrtTextValueParserLocation(pParser);
			if ( !__xrtTextValueParserNumber(pParser, &KeyEvent) ) {
				return false;
			}
			if ( KeyEvent.Type != XTEXT_VALUE_EVENT_INT ) {
				__xrtTextValueParserError(
					pParser,
					XERR_VALUE,
					XTEXT_VALUE_ERROR_NUMBER,
					"integer map key must fit int64"
				);
				return false;
			}
			Key.Type = XVALUE_KEY_INT;
			Key.Integer = KeyEvent.Value.Integer;
		} else {
			xstrview Name;

			if ( !__xrtTextValueParserString(
				pParser,
				&pParser->NameBuffer,
				pParser->DecodeStrings,
				&Name
			) ) {
				return false;
			}
			Key.Type = XVALUE_KEY_STRING;
			Key.String = Name;
		}
		if ( !__xrtTextValueParserSpace(pParser) ) {
			return false;
		}
		if (
			__xrtTextValueParserEnd(pParser) ||
			(__xrtTextValueParserPeek(pParser) != (uint8)':')
		) {
			__xrtTextValueParserError(
				pParser,
				XERR_PROTOCOL,
				XTEXT_VALUE_ERROR_SYNTAX,
				"expected colon after mapping key"
			);
			return false;
		}
		__xrtTextValueParserAdvance(pParser);
		if ( !__xrtTextValueParserSpace(pParser) ||
			 !__xrtTextValueParserValue(
				pParser,
				pEvent->Depth + 1u,
				&Key
			) ) {
			return false;
		}
		iItems++;
		if ( !__xrtTextValueParserSpace(pParser) || __xrtTextValueParserEnd(pParser) ) {
			break;
		}
		if ( __xrtTextValueParserPeek(pParser) == (uint8)'}' ) {
			break;
		}
		if ( __xrtTextValueParserPeek(pParser) != (uint8)',' ) {
			break;
		}
		__xrtTextValueParserAdvance(pParser);
		bTrailingComma = true;
		if ( !__xrtTextValueParserSpace(pParser) ) {
			return false;
		}
		if (
			!__xrtTextValueParserEnd(pParser) &&
			(__xrtTextValueParserPeek(pParser) == (uint8)'}')
		) {
			if (
				(pParser->Config.Flags & XTEXT_VALUE_READ_TRAILING_COMMA) == 0
			) {
				break;
			}
			break;
		}
		bTrailingComma = false;
	}
	if (
		__xrtTextValueParserEnd(pParser) ||
		(__xrtTextValueParserPeek(pParser) != (uint8)'}')
	) {
		__xrtTextValueParserError(
			pParser,
			XERR_PROTOCOL,
			XTEXT_VALUE_ERROR_SYNTAX,
			"expected comma or end of mapping"
		);
		return false;
	}
	if (
		bTrailingComma &&
		((pParser->Config.Flags & XTEXT_VALUE_READ_TRAILING_COMMA) == 0)
	) {
		__xrtTextValueParserError(
			pParser,
			XERR_PROTOCOL,
			XTEXT_VALUE_ERROR_SYNTAX,
			"trailing comma is disabled"
		);
		return false;
	}
	{
		xtextvalueevent EndEvent;

		memset(&EndEvent, 0, sizeof(EndEvent));
		EndEvent.Type = EndType;
		EndEvent.Depth = pEvent->Depth;
		EndEvent.Location = __xrtTextValueParserLocation(pParser);
		__xrtTextValueParserAdvance(pParser);
		return __xrtTextValueParserEmit(pParser, &EndEvent);
	}
}



/* 判断借用标识符是否等于固定 ASCII 名称。 */
static bool __xrtTextValueTagEqual(
	xstrview Tag,
	cstr sName,
	size_t iSize
)
{
	return
		(Tag.Size == iSize) &&
		(memcmp(Tag.Data, sName, iSize) == 0);
}



/* 解析 XSON 显式容器前缀或单字符串载荷标签。 */
static bool __xrtTextValueParserTagged(
	xtextvalueparser* pParser,
	xtextvalueevent* pEvent
)
{
	size_t iStart = pParser->Offset;
	xstrview Tag;

	do {
		__xrtTextValueParserAdvanceText(pParser, 1u);
	} while (
		!__xrtTextValueParserEnd(pParser) &&
		__xrtTextValueIdentifierByte(__xrtTextValueParserPeek(pParser))
	);
	Tag.Data = pParser->Text.Data + iStart;
	Tag.Size = pParser->Offset - iStart;
	if (
		!__xrtTextValueParserEnd(pParser) &&
		(__xrtTextValueParserPeek(pParser) == (uint8)'[') &&
		__xrtTextValueTagEqual(Tag, "set", 3u)
	) {
		if ( pEvent->Depth >= pParser->Config.MaxDepth ) {
			__xrtTextValueParserError(
				pParser,
				XERR_RANGE,
				XTEXT_VALUE_ERROR_LIMIT,
				"XSON nesting exceeds configured depth"
			);
			return false;
		}
		return __xrtTextValueParserSequence(
			pParser,
			pEvent,
			XTEXT_VALUE_EVENT_SET_BEGIN,
			XTEXT_VALUE_EVENT_SET_END,
			false
		);
	}
	if (
		!__xrtTextValueParserEnd(pParser) &&
		(__xrtTextValueParserPeek(pParser) == (uint8)'{') &&
		__xrtTextValueTagEqual(Tag, "intmap", 6u)
	) {
		if ( pEvent->Depth >= pParser->Config.MaxDepth ) {
			__xrtTextValueParserError(
				pParser,
				XERR_RANGE,
				XTEXT_VALUE_ERROR_LIMIT,
				"XSON nesting exceeds configured depth"
			);
			return false;
		}
		return __xrtTextValueParserMapping(
			pParser,
			pEvent,
			XTEXT_VALUE_EVENT_INT_MAP_BEGIN,
			XTEXT_VALUE_EVENT_INT_MAP_END,
			true
		);
	}
	if (
		__xrtTextValueParserEnd(pParser) ||
		(__xrtTextValueParserPeek(pParser) != (uint8)'(')
	) {
		__xrtTextValueParserError(
			pParser,
			XERR_PROTOCOL,
			XTEXT_VALUE_ERROR_SYNTAX,
			"expected XSON tag payload"
		);
		return false;
	}
	__xrtTextValueParserAdvance(pParser);
	if (
		!__xrtTextValueParserSpace(pParser) ||
		!__xrtTextValueParserString(
			pParser,
			&pParser->StringBuffer,
			pParser->DecodeStrings,
			&pEvent->Value.Tag.Payload
		) ||
		!__xrtTextValueParserSpace(pParser)
	) {
		return false;
	}
	if (
		__xrtTextValueParserEnd(pParser) ||
		(__xrtTextValueParserPeek(pParser) != (uint8)')')
	) {
		__xrtTextValueParserError(
			pParser,
			XERR_PROTOCOL,
			XTEXT_VALUE_ERROR_SYNTAX,
			"expected end of XSON tag payload"
		);
		return false;
	}
	__xrtTextValueParserAdvance(pParser);
	pEvent->Type = XTEXT_VALUE_EVENT_TAG;
	pEvent->Value.Tag.Name = Tag;
	return __xrtTextValueParserEmit(pParser, pEvent);
}



/* 解析一个值并确保所有事件使用一致的父容器定位。 */
static bool __xrtTextValueParserValue(
	xtextvalueparser* pParser,
	size_t iDepth,
	const xvaluekey* pKey
)
{
	xtextvalueevent Event;
	uint8 iByte;

	if ( pParser->Values >= pParser->Config.MaxValues ) {
		__xrtTextValueParserError(
			pParser,
			XERR_RANGE,
			XTEXT_VALUE_ERROR_LIMIT,
			"JSON value count exceeds configured limit"
		);
		return false;
	}
	if ( __xrtTextValueParserEnd(pParser) ) {
		__xrtTextValueParserError(
			pParser,
			XERR_PROTOCOL,
			XTEXT_VALUE_ERROR_SYNTAX,
			"expected JSON value"
		);
		return false;
	}
	pParser->Values++;
	memset(&Event, 0, sizeof(Event));
	Event.Location = __xrtTextValueParserLocation(pParser);
	Event.Depth = iDepth;
	if ( pKey != NULL ) {
		Event.Key = *pKey;
	}
	iByte = __xrtTextValueParserPeek(pParser);

	if ( (iByte == (uint8)'{') || (iByte == (uint8)'[') ) {
		if ( iDepth >= pParser->Config.MaxDepth ) {
			__xrtTextValueParserError(
				pParser,
				XERR_RANGE,
				XTEXT_VALUE_ERROR_LIMIT,
				"JSON nesting exceeds configured depth"
			);
			return false;
		}
		return iByte == (uint8)'{'
			? __xrtTextValueParserMapping(
				pParser,
				&Event,
				XTEXT_VALUE_EVENT_OBJECT_BEGIN,
				XTEXT_VALUE_EVENT_OBJECT_END,
				false
			)
			: __xrtTextValueParserSequence(
				pParser,
				&Event,
				XTEXT_VALUE_EVENT_ARRAY_BEGIN,
				XTEXT_VALUE_EVENT_ARRAY_END,
				true
			);
	}
	if ( iByte == (uint8)'\"' ) {
		Event.Type = XTEXT_VALUE_EVENT_STRING;
		if ( !__xrtTextValueParserString(
			pParser,
			&pParser->StringBuffer,
			pParser->DecodeStrings,
			&Event.Value.String
		) ) {
			return false;
		}
		return __xrtTextValueParserEmit(pParser, &Event);
	}
	if ( __xrtTextValueParserMatches(pParser, "null", 4u) ) {
		Event.Type = XTEXT_VALUE_EVENT_NULL;
		__xrtTextValueParserLiteral(pParser, 4u);
		return __xrtTextValueParserEmit(pParser, &Event);
	}
	if ( __xrtTextValueParserMatches(pParser, "true", 4u) ) {
		Event.Type = XTEXT_VALUE_EVENT_BOOL;
		Event.Value.Boolean = true;
		__xrtTextValueParserLiteral(pParser, 4u);
		return __xrtTextValueParserEmit(pParser, &Event);
	}
	if ( __xrtTextValueParserMatches(pParser, "false", 5u) ) {
		Event.Type = XTEXT_VALUE_EVENT_BOOL;
		Event.Value.Boolean = false;
		__xrtTextValueParserLiteral(pParser, 5u);
		return __xrtTextValueParserEmit(pParser, &Event);
	}
	if ( (iByte == (uint8)'-') ||
		 ((iByte >= (uint8)'0') && (iByte <= (uint8)'9')) ) {
		return __xrtTextValueParserNumber(pParser, &Event) &&
			__xrtTextValueParserEmit(pParser, &Event);
	}
	if (
		(pParser->Config.Dialect == XTEXT_VALUE_XSON) &&
		__xrtTextValueIdentifierStart(iByte)
	) {
		return __xrtTextValueParserTagged(pParser, &Event);
	}
	__xrtTextValueParserError(
		pParser,
		XERR_PROTOCOL,
		XTEXT_VALUE_ERROR_SYNTAX,
		"unexpected byte where a value was required"
	);
	return false;
}



/* 执行一次完整输入解析并统一管理临时缓冲。 */
xtextvaluevisitresult __xrtTextValueRead(
	xstrview Text,
	const xtextvaluereadconfig* pConfig,
	xtextvaluevisitproc pVisitor,
	ptr pVisitorData,
	xtextvalueerrorproc pError,
	ptr pErrorData,
	bool bDecodeStrings
)
{
	xtextvalueparser Parser;
	bool bParsed;

	if (
		!__xrtTextValueReadConfigValid(pConfig) ||
		((Text.Data == NULL) && (Text.Size != 0)) ||
		(pError == NULL)
	) {
		__xrtErrorSetInvalidArgument();
		return XTEXT_VALUE_VISIT_ERROR;
	}
	if ( Text.Size > pConfig->MaxInputBytes ) {
		pError(
			XERR_RANGE,
			XTEXT_VALUE_ERROR_LIMIT,
			"input exceeds configured limit",
			NULL,
			pErrorData
		);
		return XTEXT_VALUE_VISIT_ERROR;
	}
	memset(&Parser, 0, sizeof(Parser));
	Parser.Text = Text;
	Parser.Config = *pConfig;
	Parser.Line = 1u;
	Parser.Column = 1u;
	Parser.Visitor = pVisitor;
	Parser.VisitorData = pVisitorData;
	Parser.Error = pError;
	Parser.ErrorData = pErrorData;
	Parser.DecodeStrings = bDecodeStrings;
	if ( !xrtBufferInit(&Parser.NameBuffer) ||
		 !xrtBufferInit(&Parser.StringBuffer) ) {
		return XTEXT_VALUE_VISIT_ERROR;
	}
	bParsed = __xrtTextValueParserSpace(&Parser) &&
		__xrtTextValueParserValue(
			&Parser,
			0,
			NULL
		);
	if ( bParsed ) {
		bParsed = __xrtTextValueParserSpace(&Parser);
	}
	if ( bParsed && !__xrtTextValueParserEnd(&Parser) ) {
		__xrtTextValueParserError(
			&Parser,
			XERR_PROTOCOL,
			XTEXT_VALUE_ERROR_SYNTAX,
			"unexpected bytes after root value"
		);
		bParsed = false;
	}
	xrtBufferUnit(&Parser.StringBuffer);
	xrtBufferUnit(&Parser.NameBuffer);
	if ( Parser.Stopped ) {
		return XTEXT_VALUE_VISIT_STOPPED;
	}
	return bParsed ? XTEXT_VALUE_VISIT_DONE : XTEXT_VALUE_VISIT_ERROR;
}

#endif
