#include "../internal/xrt_http.h"

#include <xrt/http_structured.h>



#if defined(XRT_FEATURE_HTTP_STRUCTURED)

#define XRT_HTTP_STRUCTURED_INTEGER_MAX INT64_C(999999999999999)



/* 线路字段游标绑定 List 或 Dictionary 操作。 */
typedef enum xrt_http_structured_field_state {
	XRT_HTTP_STRUCTURED_FIELD_INITIAL = 0,
	XRT_HTTP_STRUCTURED_FIELD_LIST,
	XRT_HTTP_STRUCTURED_FIELD_DICTIONARY
} xrt_http_structured_field_state;



/* 有序 Map 游标绑定单字段值或重复字段组合。 */
typedef enum xrt_http_structured_map_state {
	XRT_HTTP_STRUCTURED_MAP_INITIAL = 0,
	XRT_HTTP_STRUCTURED_MAP_VALUE,
	XRT_HTTP_STRUCTURED_MAP_FIELDS
} xrt_http_structured_map_state;



/* 判断字节是否是 ASCII 字母。 */
static bool __xrtHttpStructuredAlpha(unsigned char iByte)
{
	return ((iByte >= (unsigned char)'A') &&
		(iByte <= (unsigned char)'Z')) ||
		((iByte >= (unsigned char)'a') &&
		 (iByte <= (unsigned char)'z'));
}



/* 判断字节是否是小写 ASCII 字母。 */
static bool __xrtHttpStructuredLower(unsigned char iByte)
{
	return (iByte >= (unsigned char)'a') &&
		(iByte <= (unsigned char)'z');
}



/* 判断字节是否是 ASCII 数字。 */
static bool __xrtHttpStructuredDigit(unsigned char iByte)
{
	return (iByte >= (unsigned char)'0') &&
		(iByte <= (unsigned char)'9');
}



/* 判断两个 key 是否按字节完全相同。 */
static bool __xrtHttpStructuredKeyEqual(xstrview Left, xstrview Right)
{
	return __xrtHttpViewEqual(Left, Right);
}



/* 跳过 Structured Fields 顶层允许的 SP。 */
static size_t __xrtHttpStructuredSkipSp(xstrview Value, size_t iOffset)
{
	while ( (iOffset < Value.Size) &&
		(Value.Data[iOffset] == ' ') ) {
		iOffset++;
	}
	return iOffset;
}



/* 跳过 List 与 Dictionary 分隔位置允许的 OWS。 */
static size_t __xrtHttpStructuredSkipOws(xstrview Value, size_t iOffset)
{
	while ( (iOffset < Value.Size) &&
		((Value.Data[iOffset] == ' ') ||
		 (Value.Data[iOffset] == '\t')) ) {
		iOffset++;
	}
	return iOffset;
}



/* 从十六进制小写字符读取一个半字节。 */
static uint8 __xrtHttpStructuredHexLower(unsigned char iByte)
{
	if ( (iByte >= (unsigned char)'0') &&
		(iByte <= (unsigned char)'9') ) {
		return (uint8)(iByte - (unsigned char)'0');
	}
	return (uint8)(iByte - (unsigned char)'a' + 10u);
}



/* 验证 Base64 内容并返回解码长度，允许 RFC 9651 要求的可选填充。 */
static bool __xrtHttpStructuredBytesMeasure(
	xstrview Encoded,
	size_t* pSize
)
{
	xbase64config Config;

	Config.Alphabet = NULL;
	Config.Flags = (uint32)XBASE64_OPTIONAL_PADDING;
	return xrtBase64Decode(
		Encoded.Data, Encoded.Size, NULL, 0, pSize, &Config
	);
}



/* 测量并严格校验 String 的反斜杠转义。 */
static bool __xrtHttpStructuredStringMeasure(
	xstrview Encoded,
	size_t* pSize
)
{
	size_t iOutput = 0;
	size_t i;

	for ( i = 0; i < Encoded.Size; i++ ) {
		unsigned char iByte = (unsigned char)Encoded.Data[i];

		if ( (iByte < 0x20u) || (iByte > 0x7Eu) ) {
			return false;
		}
		if ( iByte == (unsigned char)'\\' ) {
			if ( ++i >= Encoded.Size ) {
				return false;
			}
			iByte = (unsigned char)Encoded.Data[i];
			if ( (iByte != (unsigned char)'\\') &&
				(iByte != (unsigned char)'"') ) {
				return false;
			}
		} else if ( iByte == (unsigned char)'"' ) {
			return false;
		}
		if ( iOutput == SIZE_MAX ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iOutput++;
	}
	*pSize = iOutput;
	return true;
}



/* 测量 Display String，逐字节校验解码后的 UTF-8。 */
static bool __xrtHttpStructuredDisplayMeasure(
	xstrview Encoded,
	size_t* pSize
)
{
	xutf8state State;
	size_t iOutput = 0;
	size_t i = 0;

	xrtUtf8StateInit(&State);
	while ( i < Encoded.Size ) {
		unsigned char iByte = (unsigned char)Encoded.Data[i++];
		char sByte;
		xutfstatus Status;

		if ( (iByte < 0x20u) || (iByte > 0x7Eu) ||
			(iByte == (unsigned char)'"') ) {
			return false;
		}
		if ( iByte == (unsigned char)'%' ) {
			unsigned char iHigh;
			unsigned char iLow;

			if ( (Encoded.Size - i) < 2u ) {
				return false;
			}
			iHigh = (unsigned char)Encoded.Data[i];
			iLow = (unsigned char)Encoded.Data[i + 1u];
			if ( !(__xrtHttpStructuredDigit(iHigh) ||
				 ((iHigh >= (unsigned char)'a') &&
				  (iHigh <= (unsigned char)'f'))) ||
				!(__xrtHttpStructuredDigit(iLow) ||
				 ((iLow >= (unsigned char)'a') &&
				  (iLow <= (unsigned char)'f'))) ) {
				return false;
			}
			iByte = (unsigned char)(
				(__xrtHttpStructuredHexLower(iHigh) << 4u) |
				__xrtHttpStructuredHexLower(iLow)
			);
			i += 2u;
		}
		sByte = (char)iByte;
		Status = xrtUtf8StateFeed(
			&State, (xstrview){ &sByte, 1u }, false
		);
		if ( (Status != XUTF_OK) && (Status != XUTF_MORE) ) {
			return false;
		}
		if ( iOutput == SIZE_MAX ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iOutput++;
	}
	if ( xrtUtf8StateFeed(
		&State, (xstrview){ NULL, 0 }, true
	) != XUTF_OK ) {
		return false;
	}
	*pSize = iOutput;
	return true;
}



/* 读取 Structured Fields key，不读取后续分隔符。 */
static bool __xrtHttpStructuredKeyParse(
	xstrview Value,
	size_t* pOffset,
	xstrview* pKey
)
{
	size_t iStart = *pOffset;
	size_t i = iStart;

	if ( (i >= Value.Size) ||
		(!__xrtHttpStructuredLower(
			(unsigned char)Value.Data[i]
		) && (Value.Data[i] != '*')) ) {
		return false;
	}
	i++;
	while ( i < Value.Size ) {
		unsigned char iByte = (unsigned char)Value.Data[i];

		if ( !__xrtHttpStructuredLower(iByte) &&
			!__xrtHttpStructuredDigit(iByte) &&
			(iByte != (unsigned char)'_') &&
			(iByte != (unsigned char)'-') &&
			(iByte != (unsigned char)'.') &&
			(iByte != (unsigned char)'*') ) {
			break;
		}
		i++;
	}
	*pKey = (xstrview){ Value.Data + iStart, i - iStart };
	*pOffset = i;
	return true;
}



/* 严格读取 Integer 或 Decimal，并把 Decimal 转成千分之一。 */
static bool __xrtHttpStructuredNumberParse(
	xstrview Value,
	size_t* pOffset,
	xhttpstructuredbare* pBare
)
{
	size_t i = *pOffset;
	size_t iIntegerStart;
	size_t iIntegerDigits;
	size_t iFractionStart = 0;
	size_t iFractionDigits = 0;
	uint64 iInteger = 0;
	uint64 iFraction = 0;
	bool bNegative = false;
	bool bDecimal = false;
	size_t j;

	if ( (i < Value.Size) && (Value.Data[i] == '-') ) {
		bNegative = true;
		i++;
	}
	iIntegerStart = i;
	while ( (i < Value.Size) && __xrtHttpStructuredDigit(
		(unsigned char)Value.Data[i]
	) ) {
		i++;
	}
	iIntegerDigits = i - iIntegerStart;
	if ( iIntegerDigits == 0 ) {
		return false;
	}
	if ( (i < Value.Size) && (Value.Data[i] == '.') ) {
		bDecimal = true;
		i++;
		iFractionStart = i;
		while ( (i < Value.Size) && __xrtHttpStructuredDigit(
			(unsigned char)Value.Data[i]
		) ) {
			i++;
		}
		iFractionDigits = i - iFractionStart;
		if ( (iIntegerDigits > 12u) ||
			(iFractionDigits == 0) ||
			(iFractionDigits > 3u) ) {
			return false;
		}
	} else if ( iIntegerDigits > 15u ) {
		return false;
	}

	for ( j = 0; j < iIntegerDigits; j++ ) {
		iInteger = (iInteger * 10u) + (uint64)(
			Value.Data[iIntegerStart + j] - '0'
		);
	}
	if ( !bDecimal ) {
		if ( iInteger > (uint64)XRT_HTTP_STRUCTURED_INTEGER_MAX ) {
			return false;
		}
		pBare->Type = XHTTP_STRUCTURED_INTEGER;
		pBare->Number = bNegative ? -(int64)iInteger : (int64)iInteger;
	} else {
		for ( j = 0; j < iFractionDigits; j++ ) {
			iFraction = (iFraction * 10u) + (uint64)(
				Value.Data[iFractionStart + j] - '0'
			);
		}
		while ( j++ < 3u ) {
			iFraction *= 10u;
		}
		iInteger = (iInteger * 1000u) + iFraction;
		pBare->Type = XHTTP_STRUCTURED_DECIMAL;
		pBare->Number = bNegative ? -(int64)iInteger : (int64)iInteger;
	}
	*pOffset = i;
	return true;
}



/* 读取 String 并保留不含引号的线路转义内容。 */
static bool __xrtHttpStructuredStringParse(
	xstrview Value,
	size_t* pOffset,
	xhttpstructuredbare* pBare
)
{
	size_t i = *pOffset;
	size_t iStart;

	if ( (i >= Value.Size) || (Value.Data[i] != '"') ) {
		return false;
	}
	iStart = ++i;
	while ( i < Value.Size ) {
		unsigned char iByte = (unsigned char)Value.Data[i++];

		if ( iByte == (unsigned char)'"' ) {
			pBare->Type = XHTTP_STRUCTURED_STRING;
			pBare->Encoded = (xstrview){
				Value.Data + iStart, (i - 1u) - iStart
			};
			*pOffset = i;
			return true;
		}
		if ( (iByte < 0x20u) || (iByte > 0x7Eu) ) {
			return false;
		}
		if ( iByte == (unsigned char)'\\' ) {
			if ( i >= Value.Size ) {
				return false;
			}
			iByte = (unsigned char)Value.Data[i++];
			if ( (iByte != (unsigned char)'\\') &&
				(iByte != (unsigned char)'"') ) {
				return false;
			}
		}
	}
	return false;
}



/* 读取 token 并保留线路切片。 */
static bool __xrtHttpStructuredTokenParse(
	xstrview Value,
	size_t* pOffset,
	xhttpstructuredbare* pBare
)
{
	size_t iStart = *pOffset;
	size_t i = iStart;

	if ( (i >= Value.Size) ||
		(!__xrtHttpStructuredAlpha(
			(unsigned char)Value.Data[i]
		) && (Value.Data[i] != '*')) ) {
		return false;
	}
	i++;
	while ( i < Value.Size ) {
		unsigned char iByte = (unsigned char)Value.Data[i];

		if ( !__xrtHttpTokenByte(iByte) &&
			(iByte != (unsigned char)':') &&
			(iByte != (unsigned char)'/') ) {
			break;
		}
		i++;
	}
	pBare->Type = XHTTP_STRUCTURED_TOKEN;
	pBare->Encoded = (xstrview){ Value.Data + iStart, i - iStart };
	*pOffset = i;
	return true;
}



/* 读取并严格验证 Byte Sequence 的 Base64 内容。 */
static bool __xrtHttpStructuredBytesParse(
	xstrview Value,
	size_t* pOffset,
	xhttpstructuredbare* pBare
)
{
	size_t i = *pOffset;
	size_t iStart;
	size_t iDecoded;

	if ( (i >= Value.Size) || (Value.Data[i] != ':') ) {
		return false;
	}
	iStart = ++i;
	while ( (i < Value.Size) && (Value.Data[i] != ':') ) {
		i++;
	}
	if ( i == Value.Size ) {
		return false;
	}
	pBare->Encoded = (xstrview){ Value.Data + iStart, i - iStart };
	if ( !__xrtHttpStructuredBytesMeasure(
		pBare->Encoded, &iDecoded
	) ) {
		xrtClearError();
		return false;
	}
	pBare->Type = XHTTP_STRUCTURED_BYTES;
	*pOffset = i + 1u;
	return true;
}



/* 读取 Boolean 的固定两字节线路形式。 */
static bool __xrtHttpStructuredBooleanParse(
	xstrview Value,
	size_t* pOffset,
	xhttpstructuredbare* pBare
)
{
	size_t i = *pOffset;

	if ( ((Value.Size - i) < 2u) || (Value.Data[i] != '?') ||
		((Value.Data[i + 1u] != '0') &&
		 (Value.Data[i + 1u] != '1')) ) {
		return false;
	}
	pBare->Type = XHTTP_STRUCTURED_BOOLEAN;
	pBare->Number = Value.Data[i + 1u] == '1' ? 1 : 0;
	*pOffset = i + 2u;
	return true;
}



/* 读取 Date，并复用 Integer 的数值边界。 */
static bool __xrtHttpStructuredDateParse(
	xstrview Value,
	size_t* pOffset,
	xhttpstructuredbare* pBare
)
{
	xhttpstructuredbare Bare;
	size_t i = *pOffset;

	memset(&Bare, 0, sizeof(Bare));
	if ( (i >= Value.Size) || (Value.Data[i] != '@') ) {
		return false;
	}
	i++;
	if ( !__xrtHttpStructuredNumberParse(
		Value, &i, &Bare
	) || (Bare.Type != XHTTP_STRUCTURED_INTEGER) ) {
		return false;
	}
	Bare.Type = XHTTP_STRUCTURED_DATE;
	*pBare = Bare;
	*pOffset = i;
	return true;
}



/* 读取 Display String 并校验百分号文本与解码后的 UTF-8。 */
static bool __xrtHttpStructuredDisplayParse(
	xstrview Value,
	size_t* pOffset,
	xhttpstructuredbare* pBare
)
{
	size_t i = *pOffset;
	size_t iStart;
	size_t iDecoded;

	if ( ((Value.Size - i) < 2u) ||
		(Value.Data[i] != '%') ||
		(Value.Data[i + 1u] != '"') ) {
		return false;
	}
	i += 2u;
	iStart = i;
	while ( i < Value.Size ) {
		unsigned char iByte = (unsigned char)Value.Data[i];

		if ( iByte == (unsigned char)'"' ) {
			pBare->Encoded = (xstrview){
				Value.Data + iStart, i - iStart
			};
			if ( !__xrtHttpStructuredDisplayMeasure(
				pBare->Encoded, &iDecoded
			) ) {
				return false;
			}
			pBare->Type = XHTTP_STRUCTURED_DISPLAY;
			*pOffset = i + 1u;
			return true;
		}
		if ( (iByte < 0x20u) || (iByte > 0x7Eu) ) {
			return false;
		}
		if ( iByte == (unsigned char)'%' ) {
			if ( (Value.Size - i) < 3u ) {
				return false;
			}
			i += 3u;
		} else {
			i++;
		}
	}
	return false;
}



/* 按首字节分派一个裸值。 */
static bool __xrtHttpStructuredBareParse(
	xstrview Value,
	size_t* pOffset,
	xhttpstructuredbare* pBare
)
{
	xhttpstructuredbare Bare;
	size_t i = *pOffset;
	bool bResult;

	memset(&Bare, 0, sizeof(Bare));
	if ( i >= Value.Size ) {
		return false;
	}
	if ( (Value.Data[i] == '-') || __xrtHttpStructuredDigit(
		(unsigned char)Value.Data[i]
	) ) {
		bResult = __xrtHttpStructuredNumberParse(Value, &i, &Bare);
	} else if ( Value.Data[i] == '"' ) {
		bResult = __xrtHttpStructuredStringParse(Value, &i, &Bare);
	} else if ( __xrtHttpStructuredAlpha(
		(unsigned char)Value.Data[i]
	) || (Value.Data[i] == '*') ) {
		bResult = __xrtHttpStructuredTokenParse(Value, &i, &Bare);
	} else if ( Value.Data[i] == ':' ) {
		bResult = __xrtHttpStructuredBytesParse(Value, &i, &Bare);
	} else if ( Value.Data[i] == '?' ) {
		bResult = __xrtHttpStructuredBooleanParse(Value, &i, &Bare);
	} else if ( Value.Data[i] == '@' ) {
		bResult = __xrtHttpStructuredDateParse(Value, &i, &Bare);
	} else if ( Value.Data[i] == '%' ) {
		bResult = __xrtHttpStructuredDisplayParse(Value, &i, &Bare);
	} else {
		bResult = false;
	}
	if ( !bResult ) {
		return false;
	}
	*pOffset = i;
	*pBare = Bare;
	return true;
}



/* 解析一个参数，并把省略值发布为 Boolean true。 */
static bool __xrtHttpStructuredParameterParse(
	xstrview Parameters,
	size_t* pOffset,
	xhttpstructuredparameter* pParameter
)
{
	xhttpstructuredparameter Parameter;
	size_t i = *pOffset;

	memset(&Parameter, 0, sizeof(Parameter));
	if ( (i >= Parameters.Size) || (Parameters.Data[i] != ';') ) {
		return false;
	}
	i = __xrtHttpStructuredSkipSp(Parameters, i + 1u);
	if ( !__xrtHttpStructuredKeyParse(
		Parameters, &i, &Parameter.Key
	) ) {
		return false;
	}
	Parameter.Value.Type = XHTTP_STRUCTURED_BOOLEAN;
	Parameter.Value.Number = 1;
	if ( (i < Parameters.Size) && (Parameters.Data[i] == '=') ) {
		i++;
		if ( !__xrtHttpStructuredBareParse(
			Parameters, &i, &Parameter.Value
		) ) {
			return false;
		}
	}
	*pOffset = i;
	*pParameter = Parameter;
	return true;
}



/* 解析当前位置起的全部参数，并返回原始借用区。 */
static bool __xrtHttpStructuredParametersParse(
	xstrview Value,
	size_t* pOffset,
	xstrview* pParameters
)
{
	xhttpstructuredparameter Parameter;
	size_t iStart = *pOffset;
	size_t i = iStart;

	while ( (i < Value.Size) && (Value.Data[i] == ';') ) {
		if ( !__xrtHttpStructuredParameterParse(
			Value, &i, &Parameter
		) ) {
			return false;
		}
	}
	*pParameters = (xstrview){
		iStart < Value.Size ? Value.Data + iStart : NULL,
		i - iStart
	};
	*pOffset = i;
	return true;
}



/* 解析 Item，但不处理顶层空白或后续分隔符。 */
static bool __xrtHttpStructuredItemParseAt(
	xstrview Value,
	size_t* pOffset,
	xhttpstructureditem* pItem
)
{
	xhttpstructureditem Item;
	size_t i = *pOffset;

	memset(&Item, 0, sizeof(Item));
	if ( !__xrtHttpStructuredBareParse(
		Value, &i, &Item.Bare
	) || !__xrtHttpStructuredParametersParse(
		Value, &i, &Item.Parameters
	) ) {
		return false;
	}
	*pOffset = i;
	*pItem = Item;
	return true;
}



/* 验证括号内的完整 Inner List 并返回右括号位置。 */
static bool __xrtHttpStructuredInnerParseAt(
	xstrview Value,
	size_t* pOffset,
	xhttpstructuredmember* pMember
)
{
	xhttpstructuredmember Member;
	xhttpstructureditem Item;
	size_t i = *pOffset;
	size_t iStart;
	size_t iEnd;

	memset(&Member, 0, sizeof(Member));
	if ( (i >= Value.Size) || (Value.Data[i] != '(') ) {
		return false;
	}
	iStart = ++i;
	for ( ;; ) {
		i = __xrtHttpStructuredSkipSp(Value, i);
		if ( i >= Value.Size ) {
			return false;
		}
		if ( Value.Data[i] == ')' ) {
			iEnd = i++;
			break;
		}
		if ( !__xrtHttpStructuredItemParseAt(
			Value, &i, &Item
		) ) {
			return false;
		}
		if ( (i >= Value.Size) ||
			((Value.Data[i] != ' ') &&
			 (Value.Data[i] != ')')) ) {
			return false;
		}
	}
	if ( !__xrtHttpStructuredParametersParse(
		Value, &i, &Member.Parameters
	) ) {
		return false;
	}
	Member.Kind = XHTTP_STRUCTURED_MEMBER_INNER_LIST;
	Member.Inner = (xstrview){ Value.Data + iStart, iEnd - iStart };
	*pOffset = i;
	*pMember = Member;
	return true;
}



/* 解析 List 或 Dictionary 共用的 Item / Inner List 成员。 */
static bool __xrtHttpStructuredMemberParseAt(
	xstrview Value,
	size_t* pOffset,
	xhttpstructuredmember* pMember
)
{
	xhttpstructuredmember Member;
	xhttpstructureditem Item;
	size_t i = *pOffset;

	memset(&Member, 0, sizeof(Member));
	if ( (i < Value.Size) && (Value.Data[i] == '(') ) {
		return __xrtHttpStructuredInnerParseAt(
			Value, pOffset, pMember
		);
	}
	if ( !__xrtHttpStructuredItemParseAt(Value, &i, &Item) ) {
		return false;
	}
	Member.Kind = XHTTP_STRUCTURED_MEMBER_ITEM;
	Member.Bare = Item.Bare;
	Member.Parameters = Item.Parameters;
	*pOffset = i;
	*pMember = Member;
	return true;
}



/* 解析 Dictionary 线路成员，包括省略 Boolean true 的形式。 */
static bool __xrtHttpStructuredDictionaryParseAt(
	xstrview Value,
	size_t* pOffset,
	xhttpstructureddictionarymember* pOutput
)
{
	xhttpstructureddictionarymember Output;
	size_t i = *pOffset;

	memset(&Output, 0, sizeof(Output));
	if ( !__xrtHttpStructuredKeyParse(Value, &i, &Output.Key) ) {
		return false;
	}
	if ( (i < Value.Size) && (Value.Data[i] == '=') ) {
		i++;
		if ( !__xrtHttpStructuredMemberParseAt(
			Value, &i, &Output.Member
		) ) {
			return false;
		}
	} else {
		Output.Member.Kind = XHTTP_STRUCTURED_MEMBER_ITEM;
		Output.Member.Bare.Type = XHTTP_STRUCTURED_BOOLEAN;
		Output.Member.Bare.Number = 1;
		if ( !__xrtHttpStructuredParametersParse(
			Value, &i, &Output.Member.Parameters
		) ) {
			return false;
		}
	}
	*pOffset = i;
	*pOutput = Output;
	return true;
}



/* 完成一个顶层成员后的逗号和 OWS 处理。 */
static bool __xrtHttpStructuredMemberFinish(
	xstrview Value,
	size_t* pOffset
)
{
	size_t i = __xrtHttpStructuredSkipOws(Value, *pOffset);

	if ( i == Value.Size ) {
		*pOffset = i;
		return true;
	}
	if ( Value.Data[i] != ',' ) {
		return false;
	}
	i = __xrtHttpStructuredSkipOws(Value, i + 1u);
	if ( i == Value.Size ) {
		return false;
	}
	*pOffset = i;
	return true;
}



/* 验证公共借用值、游标和输出互不覆盖。 */
static bool __xrtHttpStructuredNextArguments(
	xstrview Value,
	const size_t* pOffset,
	const void* pOutput,
	size_t iOutputSize
)
{
	return __xrtHttpViewValid(Value) &&
		__xrtRangeValid(pOffset, sizeof(*pOffset)) &&
		__xrtRangeValid(pOutput, iOutputSize) &&
		!__xrtRangesOverlap(
			Value.Data, Value.Size, pOffset, sizeof(*pOffset)
		) && !__xrtRangesOverlap(
			Value.Data, Value.Size, pOutput, iOutputSize
		) && !__xrtRangesOverlap(
			pOffset, sizeof(*pOffset), pOutput, iOutputSize
		);
}



/* 线性验证已经去除括号的完整 Inner List 内容。 */
static bool __xrtHttpStructuredInnerComplete(xstrview Inner)
{
	xhttpstructureditem Item;
	size_t iOffset = 0;

	for ( ;; ) {
		iOffset = __xrtHttpStructuredSkipSp(Inner, iOffset);
		if ( iOffset == Inner.Size ) {
			return true;
		}
		if ( !__xrtHttpStructuredItemParseAt(
			Inner, &iOffset, &Item
		) || ((iOffset < Inner.Size) &&
			(Inner.Data[iOffset] != ' ')) ) {
			return false;
		}
	}
}



/* 线性验证完整 Structured List，不通过公开迭代器递归。 */
static bool __xrtHttpStructuredListComplete(xstrview Value)
{
	xhttpstructuredmember Member;
	size_t iOffset = __xrtHttpStructuredSkipSp(Value, 0);

	while ( iOffset < Value.Size ) {
		if ( !__xrtHttpStructuredMemberParseAt(
			Value, &iOffset, &Member
		) || !__xrtHttpStructuredMemberFinish(
			Value, &iOffset
		) ) {
			return false;
		}
	}
	return true;
}



/* 线性验证完整 Structured Dictionary，不构建抽象 map。 */
static bool __xrtHttpStructuredDictionaryComplete(xstrview Value)
{
	xhttpstructureddictionarymember Member;
	size_t iOffset = __xrtHttpStructuredSkipSp(Value, 0);

	while ( iOffset < Value.Size ) {
		if ( !__xrtHttpStructuredDictionaryParseAt(
			Value, &iOffset, &Member
		) || !__xrtHttpStructuredMemberFinish(
			Value, &iOffset
		) ) {
			return false;
		}
	}
	return true;
}



/* 验证 Structured Fields key 的小写 ASCII 语法。 */
XRT_API bool xrtHttpStructuredKeyValid(xstrview Key)
{
	xstrview Parsed;
	size_t iOffset = 0;

	if ( !__xrtHttpViewValid(Key) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtHttpStructuredKeyParse(
		Key, &iOffset, &Parsed
	) && (iOffset == Key.Size);
}



/* 验证 Structured Fields token 语法。 */
XRT_API bool xrtHttpStructuredTokenValid(xstrview Token)
{
	xhttpstructuredbare Bare;
	size_t iOffset = 0;

	if ( !__xrtHttpViewValid(Token) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(&Bare, 0, sizeof(Bare));
	return __xrtHttpStructuredTokenParse(
		Token, &iOffset, &Bare
	) && (iOffset == Token.Size);
}



/* 从当前位置解析一个裸值。 */
XRT_API xhttpnext xrtHttpStructuredBareNext(
	xstrview Value,
	size_t* pOffset,
	xhttpstructuredbare* pBare
)
{
	xhttpstructuredbare Bare;
	size_t iOffset;

	memset(&Bare, 0, sizeof(Bare));
	if ( !__xrtHttpStructuredNextArguments(
		Value, pOffset, pBare, sizeof(Bare)
	) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&iOffset, pOffset, sizeof(iOffset));
	if ( iOffset > Value.Size ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	if ( iOffset == Value.Size ) {
		memcpy(pBare, &Bare, sizeof(Bare));
		return XHTTP_NEXT_END;
	}
	if ( !__xrtHttpStructuredBareParse(
		Value, &iOffset, &Bare
	) ) {
		__xrtErrorSetValue();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pBare, &Bare, sizeof(Bare));
	memcpy(pOffset, &iOffset, sizeof(iOffset));
	return XHTTP_NEXT_ITEM;
}



/* 严格解析完整顶层 Item。 */
XRT_API bool xrtHttpStructuredItemParse(
	xstrview Value,
	xhttpstructureditem* pItem
)
{
	xhttpstructureditem Item;
	size_t iOffset;

	memset(&Item, 0, sizeof(Item));
	if ( !__xrtHttpViewValid(Value) ||
		!__xrtRangeValid(pItem, sizeof(Item)) ||
		__xrtRangesOverlap(
			Value.Data, Value.Size, pItem, sizeof(Item)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	iOffset = __xrtHttpStructuredSkipSp(Value, 0);
	if ( !__xrtHttpStructuredItemParseAt(
		Value, &iOffset, &Item
	) || (__xrtHttpStructuredSkipSp(
		Value, iOffset
	) != Value.Size) ) {
		__xrtErrorSetValue();
		return false;
	}
	memcpy(pItem, &Item, sizeof(Item));
	return true;
}



/* 迭代原始参数区。 */
XRT_API xhttpnext xrtHttpStructuredParameterNext(
	xstrview Parameters,
	size_t* pOffset,
	xhttpstructuredparameter* pParameter
)
{
	xhttpstructuredparameter Parameter;
	size_t iOffset;

	memset(&Parameter, 0, sizeof(Parameter));
	if ( !__xrtHttpStructuredNextArguments(
		Parameters, pOffset, pParameter, sizeof(Parameter)
	) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&iOffset, pOffset, sizeof(iOffset));
	if ( iOffset > Parameters.Size ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	if ( iOffset == Parameters.Size ) {
		memcpy(pParameter, &Parameter, sizeof(Parameter));
		return XHTTP_NEXT_END;
	}
	if ( !__xrtHttpStructuredParameterParse(
		Parameters, &iOffset, &Parameter
	) ) {
		__xrtErrorSetValue();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pParameter, &Parameter, sizeof(Parameter));
	memcpy(pOffset, &iOffset, sizeof(iOffset));
	return XHTTP_NEXT_ITEM;
}



/* 判断当前参数 key 在前缀中是否已经出现。 */
static bool __xrtHttpStructuredParameterSeen(
	xstrview Parameters,
	size_t iBefore,
	xstrview Key
)
{
	xhttpstructuredparameter Parameter;
	size_t iOffset = 0;

	while ( iOffset < iBefore ) {
		if ( !__xrtHttpStructuredParameterParse(
			Parameters, &iOffset, &Parameter
		) ) {
			return false;
		}
		if ( __xrtHttpStructuredKeyEqual(Parameter.Key, Key) ) {
			return true;
		}
	}
	return false;
}



/* 完整校验参数区并返回抽象有序 map 的成员数。 */
static bool __xrtHttpStructuredParametersCount(
	xstrview Parameters,
	size_t* pCount
)
{
	xhttpstructuredparameter Parameter;
	size_t iCount = 0;
	size_t iOffset = 0;
	size_t iBefore;

	while ( iOffset < Parameters.Size ) {
		iBefore = iOffset;
		if ( !__xrtHttpStructuredParameterParse(
			Parameters, &iOffset, &Parameter
		) ) {
			return false;
		}
		if ( !__xrtHttpStructuredParameterSeen(
			Parameters, iBefore, Parameter.Key
		) ) {
			if ( iCount == SIZE_MAX ) {
				__xrtErrorSetSizeOverflow();
				return false;
			}
			iCount++;
		}
	}
	*pCount = iCount;
	return true;
}



/* 在完整参数区中读取 key 的最后一次线路值。 */
static bool __xrtHttpStructuredParameterLast(
	xstrview Parameters,
	xstrview Key,
	xhttpstructuredparameter* pOutput
)
{
	xhttpstructuredparameter Parameter;
	size_t iOffset = 0;
	bool bFound = false;

	while ( iOffset < Parameters.Size ) {
		if ( !__xrtHttpStructuredParameterParse(
			Parameters, &iOffset, &Parameter
		) ) {
			return false;
		}
		if ( __xrtHttpStructuredKeyEqual(Parameter.Key, Key) ) {
			*pOutput = Parameter;
			bFound = true;
		}
	}
	return bFound;
}



/* 返回去重后的参数数量。 */
XRT_API size_t xrtHttpStructuredParameterCount(xstrview Parameters)
{
	size_t iCount;

	if ( !__xrtHttpViewValid(Parameters) ) {
		__xrtErrorSetInvalidArgument();
		return XRT_NPOS;
	}
	if ( !__xrtHttpStructuredParametersCount(
		Parameters, &iCount
	) ) {
		if ( xrtGetError() == NULL ) {
			__xrtErrorSetValue();
		}
		return XRT_NPOS;
	}
	return iCount;
}



/* 按首次出现顺序读取参数，重复 key 取最后值。 */
XRT_API xhttpnext xrtHttpStructuredParameterAt(
	xstrview Parameters,
	size_t iIndex,
	xhttpstructuredparameter* pParameter
)
{
	xhttpstructuredparameter Parameter;
	xhttpstructuredparameter Last;
	size_t iOffset = 0;
	size_t iBefore;
	size_t iUnique = 0;
	size_t iCount;

	memset(&Parameter, 0, sizeof(Parameter));
	if ( !__xrtHttpViewValid(Parameters) ||
		!__xrtRangeValid(pParameter, sizeof(Parameter)) ||
		__xrtRangesOverlap(
			Parameters.Data, Parameters.Size,
			pParameter, sizeof(Parameter)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	if ( !__xrtHttpStructuredParametersCount(
		Parameters, &iCount
	) ) {
		if ( xrtGetError() == NULL ) {
			__xrtErrorSetValue();
		}
		return XHTTP_NEXT_ERROR;
	}
	if ( iIndex >= iCount ) {
		memcpy(pParameter, &Parameter, sizeof(Parameter));
		return XHTTP_NEXT_END;
	}
	while ( iOffset < Parameters.Size ) {
		iBefore = iOffset;
		(void)__xrtHttpStructuredParameterParse(
			Parameters, &iOffset, &Parameter
		);
		if ( __xrtHttpStructuredParameterSeen(
			Parameters, iBefore, Parameter.Key
		) ) {
			continue;
		}
		if ( iUnique++ != iIndex ) {
			continue;
		}
		(void)__xrtHttpStructuredParameterLast(
			Parameters, Parameter.Key, &Last
		);
		memcpy(pParameter, &Last, sizeof(Last));
		return XHTTP_NEXT_ITEM;
	}
	__xrtErrorSetValue();
	return XHTTP_NEXT_ERROR;
}



/* 按 key 读取最后一次参数值。 */
XRT_API xhttpnext xrtHttpStructuredParameterFind(
	xstrview Parameters,
	xstrview Key,
	xhttpstructuredparameter* pParameter
)
{
	xhttpstructuredparameter Parameter;
	xstrview Parsed;
	size_t iOffset = 0;
	size_t iCount;

	memset(&Parameter, 0, sizeof(Parameter));
	if ( !__xrtHttpViewValid(Parameters) ||
		!__xrtHttpViewValid(Key) ||
		!__xrtRangeValid(pParameter, sizeof(Parameter)) ||
		__xrtRangesOverlap(
			Parameters.Data, Parameters.Size,
			pParameter, sizeof(Parameter)
		) || __xrtRangesOverlap(
			Key.Data, Key.Size, pParameter, sizeof(Parameter)
		) || !__xrtHttpStructuredKeyParse(
			Key, &iOffset, &Parsed
		) || (iOffset != Key.Size) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	if ( !__xrtHttpStructuredParametersCount(
		Parameters, &iCount
	) ) {
		if ( xrtGetError() == NULL ) {
			__xrtErrorSetValue();
		}
		return XHTTP_NEXT_ERROR;
	}
	if ( !__xrtHttpStructuredParameterLast(
		Parameters, Key, &Parameter
	) ) {
		memset(&Parameter, 0, sizeof(Parameter));
		memcpy(pParameter, &Parameter, sizeof(Parameter));
		return XHTTP_NEXT_END;
	}
	memcpy(pParameter, &Parameter, sizeof(Parameter));
	return XHTTP_NEXT_ITEM;
}



/* 迭代已经取出括号的 Inner List。 */
XRT_API xhttpnext xrtHttpStructuredInnerNext(
	xstrview Inner,
	size_t* pOffset,
	xhttpstructureditem* pItem
)
{
	xhttpstructureditem Item;
	size_t iOffset;
	size_t iNext;

	memset(&Item, 0, sizeof(Item));
	if ( !__xrtHttpStructuredNextArguments(
		Inner, pOffset, pItem, sizeof(Item)
	) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&iOffset, pOffset, sizeof(iOffset));
	if ( iOffset > Inner.Size ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	if ( (iOffset == 0) &&
		!__xrtHttpStructuredInnerComplete(Inner) ) {
		__xrtErrorSetValue();
		return XHTTP_NEXT_ERROR;
	}
	iOffset = __xrtHttpStructuredSkipSp(Inner, iOffset);
	if ( iOffset == Inner.Size ) {
		memcpy(pItem, &Item, sizeof(Item));
		memcpy(pOffset, &iOffset, sizeof(iOffset));
		return XHTTP_NEXT_END;
	}
	iNext = iOffset;
	if ( !__xrtHttpStructuredItemParseAt(
		Inner, &iNext, &Item
	) || ((iNext < Inner.Size) &&
		(Inner.Data[iNext] != ' ')) ) {
		__xrtErrorSetValue();
		return XHTTP_NEXT_ERROR;
	}
	iNext = __xrtHttpStructuredSkipSp(Inner, iNext);
	memcpy(pItem, &Item, sizeof(Item));
	memcpy(pOffset, &iNext, sizeof(iNext));
	return XHTTP_NEXT_ITEM;
}



/* 严格验证完整 Inner List 内容。 */
XRT_API bool xrtHttpStructuredInnerValid(xstrview Inner)
{
	if ( !__xrtHttpViewValid(Inner) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpStructuredInnerComplete(Inner) ) {
		__xrtErrorSetValue();
		return false;
	}
	return true;
}



/* 迭代完整 Structured List。 */
XRT_API xhttpnext xrtHttpStructuredListNext(
	xstrview Value,
	size_t* pOffset,
	xhttpstructuredmember* pMember
)
{
	xhttpstructuredmember Member;
	size_t iOffset;
	size_t iNext;

	memset(&Member, 0, sizeof(Member));
	if ( !__xrtHttpStructuredNextArguments(
		Value, pOffset, pMember, sizeof(Member)
	) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&iOffset, pOffset, sizeof(iOffset));
	if ( iOffset > Value.Size ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	if ( (iOffset == 0) &&
		!__xrtHttpStructuredListComplete(Value) ) {
		__xrtErrorSetValue();
		return XHTTP_NEXT_ERROR;
	}
	if ( iOffset == 0 ) {
		iOffset = __xrtHttpStructuredSkipSp(Value, iOffset);
	}
	if ( iOffset == Value.Size ) {
		memcpy(pMember, &Member, sizeof(Member));
		memcpy(pOffset, &iOffset, sizeof(iOffset));
		return XHTTP_NEXT_END;
	}
	iNext = iOffset;
	if ( !__xrtHttpStructuredMemberParseAt(
		Value, &iNext, &Member
	) || !__xrtHttpStructuredMemberFinish(
		Value, &iNext
	) ) {
		__xrtErrorSetValue();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pMember, &Member, sizeof(Member));
	memcpy(pOffset, &iNext, sizeof(iNext));
	return XHTTP_NEXT_ITEM;
}



/* 严格验证完整 Structured List。 */
XRT_API bool xrtHttpStructuredListValid(xstrview Value)
{
	if ( !__xrtHttpViewValid(Value) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpStructuredListComplete(Value) ) {
		__xrtErrorSetValue();
		return false;
	}
	return true;
}



/* 迭代完整 Structured Dictionary。 */
XRT_API xhttpnext xrtHttpStructuredDictionaryNext(
	xstrview Value,
	size_t* pOffset,
	xhttpstructureddictionarymember* pMember
)
{
	xhttpstructureddictionarymember Member;
	size_t iOffset;
	size_t iNext;

	memset(&Member, 0, sizeof(Member));
	if ( !__xrtHttpStructuredNextArguments(
		Value, pOffset, pMember, sizeof(Member)
	) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&iOffset, pOffset, sizeof(iOffset));
	if ( iOffset > Value.Size ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	if ( (iOffset == 0) &&
		!__xrtHttpStructuredDictionaryComplete(Value) ) {
		__xrtErrorSetValue();
		return XHTTP_NEXT_ERROR;
	}
	if ( iOffset == 0 ) {
		iOffset = __xrtHttpStructuredSkipSp(Value, iOffset);
	}
	if ( iOffset == Value.Size ) {
		memcpy(pMember, &Member, sizeof(Member));
		memcpy(pOffset, &iOffset, sizeof(iOffset));
		return XHTTP_NEXT_END;
	}
	iNext = iOffset;
	if ( !__xrtHttpStructuredDictionaryParseAt(
		Value, &iNext, &Member
	) || !__xrtHttpStructuredMemberFinish(
		Value, &iNext
	) ) {
		__xrtErrorSetValue();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pMember, &Member, sizeof(Member));
	memcpy(pOffset, &iNext, sizeof(iNext));
	return XHTTP_NEXT_ITEM;
}



/* 严格验证完整 Structured Dictionary。 */
XRT_API bool xrtHttpStructuredDictionaryValid(xstrview Value)
{
	if ( !__xrtHttpViewValid(Value) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpStructuredDictionaryComplete(Value) ) {
		__xrtErrorSetValue();
		return false;
	}
	return true;
}



/* 在已经完整验证的 Dictionary 上读取下一个线路成员。 */
static xhttpnext __xrtHttpStructuredDictionaryUnchecked(
	xstrview Value,
	size_t* pOffset,
	xhttpstructureddictionarymember* pMember
)
{
	size_t iOffset = *pOffset;

	if ( iOffset == 0 ) {
		iOffset = __xrtHttpStructuredSkipSp(Value, iOffset);
	}
	if ( iOffset == Value.Size ) {
		*pOffset = iOffset;
		return XHTTP_NEXT_END;
	}
	if ( !__xrtHttpStructuredDictionaryParseAt(
		Value, &iOffset, pMember
	) || !__xrtHttpStructuredMemberFinish(
		Value, &iOffset
	) ) {
		__xrtErrorSetValue();
		return XHTTP_NEXT_ERROR;
	}
	*pOffset = iOffset;
	return XHTTP_NEXT_ITEM;
}



/* 判断当前 Dictionary key 在前缀中是否已经出现。 */
static bool __xrtHttpStructuredDictionarySeen(
	xstrview Value,
	size_t iBefore,
	xstrview Key
)
{
	xhttpstructureddictionarymember Member;
	size_t iOffset = __xrtHttpStructuredSkipSp(Value, 0);

	while ( iOffset < iBefore ) {
		if ( !__xrtHttpStructuredDictionaryParseAt(
			Value, &iOffset, &Member
		) || !__xrtHttpStructuredMemberFinish(
			Value, &iOffset
		) ) {
			return false;
		}
		if ( __xrtHttpStructuredKeyEqual(Member.Key, Key) ) {
			return true;
		}
	}
	return false;
}



/* 在完整 Dictionary 中读取 key 的最后一次线路值。 */
static bool __xrtHttpStructuredDictionaryLast(
	xstrview Value,
	xstrview Key,
	xhttpstructureddictionarymember* pOutput
)
{
	xhttpstructureddictionarymember Member;
	size_t iOffset = __xrtHttpStructuredSkipSp(Value, 0);
	bool bFound = false;

	while ( iOffset < Value.Size ) {
		if ( !__xrtHttpStructuredDictionaryParseAt(
			Value, &iOffset, &Member
		) || !__xrtHttpStructuredMemberFinish(
			Value, &iOffset
		) ) {
			return false;
		}
		if ( __xrtHttpStructuredKeyEqual(Member.Key, Key) ) {
			*pOutput = Member;
			bFound = true;
		}
	}
	return bFound;
}



/* 判断有序 Map 游标是否是初始化后的全零状态。 */
static bool __xrtHttpStructuredMapCursorInitial(
	const xhttpstructuredmapcursor* pCursor
)
{
	return (pCursor->Source == NULL) &&
		(pCursor->Name == NULL) &&
		(pCursor->SourceSize == 0) &&
		(pCursor->NameSize == 0) &&
		(pCursor->Field == 0) &&
		(pCursor->Offset == 0) &&
		(pCursor->Order == 0) &&
		(pCursor->State ==
		 (uint8)XRT_HTTP_STRUCTURED_MAP_INITIAL);
}



/* 验证有序 Map 游标仍绑定同一个不可变 Dictionary 值。 */
static bool __xrtHttpStructuredMapValueCursorValid(
	const xhttpstructuredmapcursor* pCursor,
	xstrview Value
)
{
	if ( pCursor->State ==
		(uint8)XRT_HTTP_STRUCTURED_MAP_INITIAL ) {
		return __xrtHttpStructuredMapCursorInitial(pCursor);
	}
	return (pCursor->State ==
		(uint8)XRT_HTTP_STRUCTURED_MAP_VALUE) &&
		(pCursor->Source == (const void*)Value.Data) &&
		(pCursor->Name == NULL) &&
		(pCursor->SourceSize == Value.Size) &&
		(pCursor->NameSize == 0) &&
		(pCursor->Field == 0) &&
		(pCursor->Offset <= Value.Size);
}



/* 初始化 Dictionary 有序 Map 游标。 */
XRT_API void xrtHttpStructuredMapCursorInit(
	xhttpstructuredmapcursor* pCursor
)
{
	xhttpstructuredmapcursor Cursor;

	if ( !__xrtRangeValid(pCursor, sizeof(Cursor)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(&Cursor, 0, sizeof(Cursor));
	memcpy(pCursor, &Cursor, sizeof(Cursor));
}



/* 按抽象有序 Map 语义迭代完整 Dictionary。 */
XRT_API xhttpnext xrtHttpStructuredDictionaryMapNext(
	xstrview Value,
	xhttpstructuredmapcursor* pCursor,
	xhttpstructureddictionarymember* pMember
)
{
	xhttpstructureddictionarymember Member;
	xhttpstructureddictionarymember Last;
	xhttpstructuredmapcursor Cursor;
	xhttpnext Next;
	size_t iBefore;

	memset(&Member, 0, sizeof(Member));
	if ( !__xrtHttpViewValid(Value) ||
		!__xrtRangeValid(pCursor, sizeof(Cursor)) ||
		!__xrtRangeValid(pMember, sizeof(Member)) ||
		__xrtRangesOverlap(
			Value.Data, Value.Size, pCursor, sizeof(Cursor)
		) || __xrtRangesOverlap(
			Value.Data, Value.Size, pMember, sizeof(Member)
		) || __xrtRangesOverlap(
			pCursor, sizeof(Cursor), pMember, sizeof(Member)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&Cursor, pCursor, sizeof(Cursor));
	if ( !__xrtHttpStructuredMapValueCursorValid(
		&Cursor, Value
	) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	if ( Cursor.State ==
		(uint8)XRT_HTTP_STRUCTURED_MAP_INITIAL ) {
		if ( !xrtHttpStructuredDictionaryValid(Value) ) {
			return XHTTP_NEXT_ERROR;
		}
		Cursor.Source = Value.Data;
		Cursor.SourceSize = Value.Size;
		Cursor.State = (uint8)XRT_HTTP_STRUCTURED_MAP_VALUE;
	}
	for ( ;; ) {
		iBefore = Cursor.Offset;
		Next = __xrtHttpStructuredDictionaryUnchecked(
			Value, &Cursor.Offset, &Member
		);
		if ( Next == XHTTP_NEXT_END ) {
			memset(&Member, 0, sizeof(Member));
			memcpy(pMember, &Member, sizeof(Member));
			memcpy(pCursor, &Cursor, sizeof(Cursor));
			return Next;
		}
		if ( Next == XHTTP_NEXT_ERROR ) {
			return Next;
		}
		if ( __xrtHttpStructuredDictionarySeen(
			Value, iBefore, Member.Key
		) ) {
			continue;
		}
		if ( !__xrtHttpStructuredDictionaryLast(
			Value, Member.Key, &Last
		) ) {
			__xrtErrorSetValue();
			return XHTTP_NEXT_ERROR;
		}
		memcpy(pMember, &Last, sizeof(Last));
		memcpy(pCursor, &Cursor, sizeof(Cursor));
		return XHTTP_NEXT_ITEM;
	}
}



/* 返回去重后的 Dictionary 成员数量。 */
XRT_API size_t xrtHttpStructuredDictionaryCount(xstrview Value)
{
	xhttpstructuredmapcursor Cursor;
	xhttpstructureddictionarymember Member;
	xhttpnext Next;
	size_t iCount;

	if ( !__xrtHttpViewValid(Value) ) {
		__xrtErrorSetInvalidArgument();
		return XRT_NPOS;
	}
	iCount = 0;
	xrtHttpStructuredMapCursorInit(&Cursor);
	while ( (Next = xrtHttpStructuredDictionaryMapNext(
		Value, &Cursor, &Member
	)) == XHTTP_NEXT_ITEM ) {
		if ( iCount == SIZE_MAX ) {
			__xrtErrorSetSizeOverflow();
			return XRT_NPOS;
		}
		iCount++;
	}
	if ( Next == XHTTP_NEXT_ERROR ) {
		return XRT_NPOS;
	}
	return iCount;
}



/* 按首次出现顺序读取 Dictionary，重复 key 取最后值。 */
XRT_API xhttpnext xrtHttpStructuredDictionaryAt(
	xstrview Value,
	size_t iIndex,
	xhttpstructureddictionarymember* pMember
)
{
	xhttpstructuredmapcursor Cursor;
	xhttpstructureddictionarymember Member;
	size_t iUnique = 0;
	xhttpnext Next;

	memset(&Member, 0, sizeof(Member));
	if ( !__xrtHttpViewValid(Value) ||
		!__xrtRangeValid(pMember, sizeof(Member)) ||
		__xrtRangesOverlap(
			Value.Data, Value.Size, pMember, sizeof(Member)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	xrtHttpStructuredMapCursorInit(&Cursor);
	while ( (Next = xrtHttpStructuredDictionaryMapNext(
		Value, &Cursor, &Member
	)) == XHTTP_NEXT_ITEM ) {
		if ( iUnique++ == iIndex ) {
			memcpy(pMember, &Member, sizeof(Member));
			return Next;
		}
	}
	if ( Next == XHTTP_NEXT_END ) {
		memset(&Member, 0, sizeof(Member));
		memcpy(pMember, &Member, sizeof(Member));
	}
	return Next;
}



/* 按 key 读取 Dictionary 中最后一次成员值。 */
XRT_API xhttpnext xrtHttpStructuredDictionaryFind(
	xstrview Value,
	xstrview Key,
	xhttpstructureddictionarymember* pMember
)
{
	xhttpstructureddictionarymember Member;
	xstrview Parsed;
	size_t iOffset = 0;

	memset(&Member, 0, sizeof(Member));
	if ( !__xrtHttpViewValid(Value) ||
		!__xrtHttpViewValid(Key) ||
		!__xrtRangeValid(pMember, sizeof(Member)) ||
		__xrtRangesOverlap(
			Value.Data, Value.Size, pMember, sizeof(Member)
		) || __xrtRangesOverlap(
			Key.Data, Key.Size, pMember, sizeof(Member)
		) || !__xrtHttpStructuredKeyParse(
			Key, &iOffset, &Parsed
		) || (iOffset != Key.Size) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	if ( !xrtHttpStructuredDictionaryValid(Value) ) {
		return XHTTP_NEXT_ERROR;
	}
	if ( !__xrtHttpStructuredDictionaryLast(
		Value, Key, &Member
	) ) {
		memset(&Member, 0, sizeof(Member));
		memcpy(pMember, &Member, sizeof(Member));
		return XHTTP_NEXT_END;
	}
	memcpy(pMember, &Member, sizeof(Member));
	return XHTTP_NEXT_ITEM;
}



/* 初始化重复字段行游标。 */
XRT_API void xrtHttpStructuredFieldCursorInit(
	xhttpstructuredfieldcursor* pCursor
)
{
	xhttpstructuredfieldcursor Cursor;

	if ( !__xrtRangeValid(pCursor, sizeof(Cursor)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(&Cursor, 0, sizeof(Cursor));
	memcpy(pCursor, &Cursor, sizeof(Cursor));
}



/* 验证重复字段数组可按 RFC 的逗号组合语义处理。 */
static bool __xrtHttpStructuredFieldsValidate(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	bool bDictionary,
	size_t* pMatches
)
{
	xhttpfield Field;
	size_t iMatches = 0;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		__xrtHttpFieldLoad(pFields, i, &Field);
		if ( !xrtHttpFieldNameEqual(Field.Name, Name) ) {
			continue;
		}
		if ( bDictionary ?
			!xrtHttpStructuredDictionaryValid(Field.Value) :
			!xrtHttpStructuredListValid(Field.Value) ) {
			return false;
		}
		if ( iMatches == SIZE_MAX ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iMatches++;
	}
	if ( iMatches > 1u ) {
		for ( i = 0; i < iCount; i++ ) {
			__xrtHttpFieldLoad(pFields, i, &Field);
			if ( xrtHttpFieldNameEqual(Field.Name, Name) &&
				(__xrtHttpStructuredSkipSp(
					Field.Value, 0
				) == Field.Value.Size) ) {
				__xrtErrorSetValue();
				return false;
			}
		}
	}
	*pMatches = iMatches;
	return true;
}



/* 判断重复字段游标是否是初始化后的全零状态。 */
static bool __xrtHttpStructuredFieldCursorInitial(
	const xhttpstructuredfieldcursor* pCursor
)
{
	return (pCursor->Source == NULL) &&
		(pCursor->Name == NULL) &&
		(pCursor->SourceSize == 0) &&
		(pCursor->NameSize == 0) &&
		(pCursor->Field == 0) &&
		(pCursor->Offset == 0) &&
		(pCursor->State ==
		 (uint8)XRT_HTTP_STRUCTURED_FIELD_INITIAL);
}



/* 验证重复字段游标仍绑定同一字段数组、字段名和顶层类型。 */
static bool __xrtHttpStructuredFieldCursorValid(
	const xhttpstructuredfieldcursor* pCursor,
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	uint8 iState
)
{
	if ( pCursor->State ==
		(uint8)XRT_HTTP_STRUCTURED_FIELD_INITIAL ) {
		return __xrtHttpStructuredFieldCursorInitial(pCursor);
	}
	return (pCursor->State == iState) &&
		(pCursor->Source == (const void*)pFields) &&
		(pCursor->SourceSize == iCount) &&
		(pCursor->NameSize == Name.Size) &&
		__xrtRangeValid(pCursor->Name, pCursor->NameSize) &&
		xrtHttpFieldNameEqual(
			(xstrview){ (cstr)pCursor->Name, pCursor->NameSize },
			Name
		) &&
		(pCursor->Field <= iCount) &&
		!((pCursor->Field == iCount) &&
		  (pCursor->Offset != 0));
}



/* 验证重复字段迭代器的公共内存参数。 */
static bool __xrtHttpStructuredFieldArguments(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	uint8 iState,
	const xhttpstructuredfieldcursor* pCursor,
	const void* pOutput,
	size_t iOutputSize,
	xhttpstructuredfieldcursor* pLoaded
)
{
	if ( !__xrtHttpFieldArrayValid(pFields, iCount) ||
		!__xrtHttpViewValid(Name) || (Name.Size == 0) ||
		!xrtHttpTokenValid(Name) ||
		!__xrtRangeValid(pCursor, sizeof(*pCursor)) ||
		!__xrtRangeValid(pOutput, iOutputSize) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount, pCursor, sizeof(*pCursor)
		) || __xrtHttpFieldArrayOverlap(
			pFields, iCount, pOutput, iOutputSize
		) || __xrtRangesOverlap(
			Name.Data, Name.Size, pCursor, sizeof(*pCursor)
		) || __xrtRangesOverlap(
			Name.Data, Name.Size, pOutput, iOutputSize
		) || __xrtRangesOverlap(
			pCursor, sizeof(*pCursor), pOutput, iOutputSize
		) ) {
		return false;
	}
	memcpy(pLoaded, pCursor, sizeof(*pLoaded));
	return __xrtHttpStructuredFieldCursorValid(
		pLoaded, pFields, iCount, Name, iState
	);
}



/* 跨重复字段行迭代 Structured List。 */
XRT_API xhttpnext xrtHttpStructuredListFieldNext(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	xhttpstructuredfieldcursor* pCursor,
	xhttpstructuredmember* pMember
)
{
	xhttpstructuredfieldcursor Cursor;
	xhttpstructuredmember Member;
	xhttpfield Field;
	xhttpnext Next;
	size_t iMatches;

	memset(&Member, 0, sizeof(Member));
	if ( !__xrtHttpStructuredFieldArguments(
		pFields, iCount, Name,
		(uint8)XRT_HTTP_STRUCTURED_FIELD_LIST,
		pCursor, pMember,
		sizeof(Member), &Cursor
	) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	if ( Cursor.State ==
		(uint8)XRT_HTTP_STRUCTURED_FIELD_INITIAL ) {
		if ( !__xrtHttpStructuredFieldsValidate(
			pFields, iCount, Name, false, &iMatches
		) ) {
			return XHTTP_NEXT_ERROR;
		}
		Cursor.Source = pFields;
		Cursor.Name = Name.Data;
		Cursor.SourceSize = iCount;
		Cursor.NameSize = Name.Size;
		Cursor.State =
			(uint8)XRT_HTTP_STRUCTURED_FIELD_LIST;
	}
	while ( Cursor.Field < iCount ) {
		__xrtHttpFieldLoad(pFields, Cursor.Field, &Field);
		if ( !xrtHttpFieldNameEqual(Field.Name, Name) ) {
			if ( Cursor.Offset != 0 ) {
				__xrtErrorSetInvalidArgument();
				return XHTTP_NEXT_ERROR;
			}
			Cursor.Field++;
			continue;
		}
		Next = xrtHttpStructuredListNext(
			Field.Value, &Cursor.Offset, &Member
		);
		if ( Next == XHTTP_NEXT_ITEM ) {
			memcpy(pMember, &Member, sizeof(Member));
			memcpy(pCursor, &Cursor, sizeof(Cursor));
			return Next;
		}
		if ( Next == XHTTP_NEXT_ERROR ) {
			return Next;
		}
		Cursor.Field++;
		Cursor.Offset = 0;
	}
	Cursor.Offset = 0;
	memset(&Member, 0, sizeof(Member));
	memcpy(pMember, &Member, sizeof(Member));
	memcpy(pCursor, &Cursor, sizeof(Cursor));
	return XHTTP_NEXT_END;
}



/* 跨重复字段行迭代 Structured Dictionary。 */
XRT_API xhttpnext xrtHttpStructuredDictionaryFieldNext(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	xhttpstructuredfieldcursor* pCursor,
	xhttpstructureddictionarymember* pMember
)
{
	xhttpstructuredfieldcursor Cursor;
	xhttpstructureddictionarymember Member;
	xhttpfield Field;
	xhttpnext Next;
	size_t iMatches;

	memset(&Member, 0, sizeof(Member));
	if ( !__xrtHttpStructuredFieldArguments(
		pFields, iCount, Name,
		(uint8)XRT_HTTP_STRUCTURED_FIELD_DICTIONARY,
		pCursor, pMember,
		sizeof(Member), &Cursor
	) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	if ( Cursor.State ==
		(uint8)XRT_HTTP_STRUCTURED_FIELD_INITIAL ) {
		if ( !__xrtHttpStructuredFieldsValidate(
			pFields, iCount, Name, true, &iMatches
		) ) {
			return XHTTP_NEXT_ERROR;
		}
		Cursor.Source = pFields;
		Cursor.Name = Name.Data;
		Cursor.SourceSize = iCount;
		Cursor.NameSize = Name.Size;
		Cursor.State =
			(uint8)XRT_HTTP_STRUCTURED_FIELD_DICTIONARY;
	}
	while ( Cursor.Field < iCount ) {
		__xrtHttpFieldLoad(pFields, Cursor.Field, &Field);
		if ( !xrtHttpFieldNameEqual(Field.Name, Name) ) {
			if ( Cursor.Offset != 0 ) {
				__xrtErrorSetInvalidArgument();
				return XHTTP_NEXT_ERROR;
			}
			Cursor.Field++;
			continue;
		}
		Next = xrtHttpStructuredDictionaryNext(
			Field.Value, &Cursor.Offset, &Member
		);
		if ( Next == XHTTP_NEXT_ITEM ) {
			memcpy(pMember, &Member, sizeof(Member));
			memcpy(pCursor, &Cursor, sizeof(Cursor));
			return Next;
		}
		if ( Next == XHTTP_NEXT_ERROR ) {
			return Next;
		}
		Cursor.Field++;
		Cursor.Offset = 0;
	}
	Cursor.Offset = 0;
	memset(&Member, 0, sizeof(Member));
	memcpy(pMember, &Member, sizeof(Member));
	memcpy(pCursor, &Cursor, sizeof(Cursor));
	return XHTTP_NEXT_END;
}



/* 在已经完整验证的字段数组上迭代 Dictionary。 */
static xhttpnext __xrtHttpStructuredDictionaryFieldUnchecked(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	xhttpstructuredfieldcursor* pCursor,
	xhttpstructureddictionarymember* pMember
)
{
	xhttpfield Field;
	xhttpnext Next;

	while ( pCursor->Field < iCount ) {
		__xrtHttpFieldLoad(pFields, pCursor->Field, &Field);
		if ( !xrtHttpFieldNameEqual(Field.Name, Name) ) {
			pCursor->Field++;
			continue;
		}
		Next = __xrtHttpStructuredDictionaryUnchecked(
			Field.Value, &pCursor->Offset, pMember
		);
		if ( Next == XHTTP_NEXT_ITEM ) {
			return Next;
		}
		if ( Next == XHTTP_NEXT_ERROR ) {
			return Next;
		}
		pCursor->Field++;
		pCursor->Offset = 0;
	}
	return XHTTP_NEXT_END;
}



/* 验证跨字段 Dictionary 查询的公共输入和可选输出。 */
static bool __xrtHttpStructuredDictionaryFieldArguments(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	const void* pOutput,
	size_t iOutputSize
)
{
	return __xrtHttpFieldArrayValid(pFields, iCount) &&
		__xrtHttpViewValid(Name) && (Name.Size != 0) &&
		xrtHttpTokenValid(Name) &&
		((iOutputSize == 0) ||
		 (__xrtRangeValid(pOutput, iOutputSize) &&
		  !__xrtHttpFieldArrayOverlap(
			pFields, iCount, pOutput, iOutputSize
		  ) && !__xrtRangesOverlap(
			Name.Data, Name.Size, pOutput, iOutputSize
		  )));
}



/* 判断线路序号之前是否已经出现 Dictionary key。 */
static bool __xrtHttpStructuredDictionaryFieldSeen(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	size_t iBefore,
	xstrview Key
)
{
	xhttpstructuredfieldcursor Cursor;
	xhttpstructureddictionarymember Member;
	size_t iOrder = 0;

	memset(&Cursor, 0, sizeof(Cursor));
	while ( iOrder < iBefore ) {
		if ( __xrtHttpStructuredDictionaryFieldUnchecked(
			pFields, iCount, Name, &Cursor, &Member
		) != XHTTP_NEXT_ITEM ) {
			return false;
		}
		if ( __xrtHttpStructuredKeyEqual(Member.Key, Key) ) {
			return true;
		}
		iOrder++;
	}
	return false;
}



/* 读取跨字段 Dictionary key 的最后一次值。 */
static bool __xrtHttpStructuredDictionaryFieldLast(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	xstrview Key,
	xhttpstructureddictionarymember* pOutput
)
{
	xhttpstructuredfieldcursor Cursor;
	xhttpstructureddictionarymember Member;
	bool bFound = false;

	memset(&Cursor, 0, sizeof(Cursor));
	while ( __xrtHttpStructuredDictionaryFieldUnchecked(
		pFields, iCount, Name, &Cursor, &Member
	) == XHTTP_NEXT_ITEM ) {
		if ( __xrtHttpStructuredKeyEqual(Member.Key, Key) ) {
			*pOutput = Member;
			bFound = true;
		}
	}
	return bFound;
}



/* 验证有序 Map 游标仍绑定同一重复字段组合。 */
static bool __xrtHttpStructuredMapFieldsCursorValid(
	const xhttpstructuredmapcursor* pCursor,
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name
)
{
	if ( pCursor->State ==
		(uint8)XRT_HTTP_STRUCTURED_MAP_INITIAL ) {
		return __xrtHttpStructuredMapCursorInitial(pCursor);
	}
	return (pCursor->State ==
		(uint8)XRT_HTTP_STRUCTURED_MAP_FIELDS) &&
		(pCursor->Source == (const void*)pFields) &&
		(pCursor->SourceSize == iCount) &&
		(pCursor->NameSize == Name.Size) &&
		__xrtRangeValid(pCursor->Name, pCursor->NameSize) &&
		xrtHttpFieldNameEqual(
			(xstrview){ (cstr)pCursor->Name, pCursor->NameSize },
			Name
		) &&
		(pCursor->Field <= iCount) &&
		!((pCursor->Field == iCount) &&
		  (pCursor->Offset != 0));
}



/* 跨重复字段行按抽象有序 Map 语义迭代 Dictionary。 */
XRT_API xhttpnext xrtHttpStructuredDictionaryMapFieldNext(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	xhttpstructuredmapcursor* pCursor,
	xhttpstructureddictionarymember* pMember
)
{
	xhttpstructuredfieldcursor Raw;
	xhttpstructureddictionarymember Member;
	xhttpstructureddictionarymember Last;
	xhttpstructuredmapcursor Cursor;
	xhttpnext Next;
	size_t iBefore;
	size_t iMatches;

	memset(&Member, 0, sizeof(Member));
	if ( !__xrtHttpStructuredDictionaryFieldArguments(
		pFields, iCount, Name, pMember, sizeof(Member)
	) || !__xrtRangeValid(pCursor, sizeof(Cursor)) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount, pCursor, sizeof(Cursor)
		) || __xrtRangesOverlap(
			Name.Data, Name.Size, pCursor, sizeof(Cursor)
		) || __xrtRangesOverlap(
			pCursor, sizeof(Cursor), pMember, sizeof(Member)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&Cursor, pCursor, sizeof(Cursor));
	if ( !__xrtHttpStructuredMapFieldsCursorValid(
		&Cursor, pFields, iCount, Name
	) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	if ( Cursor.State ==
		(uint8)XRT_HTTP_STRUCTURED_MAP_INITIAL ) {
		if ( !__xrtHttpStructuredFieldsValidate(
			pFields, iCount, Name, true, &iMatches
		) ) {
			return XHTTP_NEXT_ERROR;
		}
		Cursor.Source = pFields;
		Cursor.Name = Name.Data;
		Cursor.SourceSize = iCount;
		Cursor.NameSize = Name.Size;
		Cursor.State = (uint8)XRT_HTTP_STRUCTURED_MAP_FIELDS;
	}
	for ( ;; ) {
		memset(&Raw, 0, sizeof(Raw));
		Raw.Field = Cursor.Field;
		Raw.Offset = Cursor.Offset;
		iBefore = Cursor.Order;
		Next = __xrtHttpStructuredDictionaryFieldUnchecked(
			pFields, iCount, Name, &Raw, &Member
		);
		Cursor.Field = Raw.Field;
		Cursor.Offset = Raw.Offset;
		if ( Next == XHTTP_NEXT_END ) {
			Cursor.Offset = 0;
			memset(&Member, 0, sizeof(Member));
			memcpy(pMember, &Member, sizeof(Member));
			memcpy(pCursor, &Cursor, sizeof(Cursor));
			return Next;
		}
		if ( Next == XHTTP_NEXT_ERROR ) {
			return Next;
		}
		if ( Cursor.Order == SIZE_MAX ) {
			__xrtErrorSetSizeOverflow();
			return XHTTP_NEXT_ERROR;
		}
		Cursor.Order++;
		if ( __xrtHttpStructuredDictionaryFieldSeen(
			pFields, iCount, Name, iBefore, Member.Key
		) ) {
			continue;
		}
		if ( !__xrtHttpStructuredDictionaryFieldLast(
			pFields, iCount, Name, Member.Key, &Last
		) ) {
			__xrtErrorSetValue();
			return XHTTP_NEXT_ERROR;
		}
		memcpy(pMember, &Last, sizeof(Last));
		memcpy(pCursor, &Cursor, sizeof(Cursor));
		return XHTTP_NEXT_ITEM;
	}
}



/* 返回重复字段行逻辑组合后的 Dictionary 成员数。 */
XRT_API size_t xrtHttpStructuredDictionaryFieldCount(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name
)
{
	xhttpstructuredmapcursor Cursor;
	xhttpstructureddictionarymember Member;
	xhttpnext Next;
	size_t iResult;

	if ( !__xrtHttpStructuredDictionaryFieldArguments(
		pFields, iCount, Name, NULL, 0
	) ) {
		__xrtErrorSetInvalidArgument();
		return XRT_NPOS;
	}
	iResult = 0;
	xrtHttpStructuredMapCursorInit(&Cursor);
	while ( (Next = xrtHttpStructuredDictionaryMapFieldNext(
		pFields, iCount, Name, &Cursor, &Member
	)) == XHTTP_NEXT_ITEM ) {
		if ( iResult == SIZE_MAX ) {
			__xrtErrorSetSizeOverflow();
			return XRT_NPOS;
		}
		iResult++;
	}
	if ( Next == XHTTP_NEXT_ERROR ) {
		return XRT_NPOS;
	}
	return iResult;
}



/* 按首次出现顺序读取跨字段 Dictionary。 */
XRT_API xhttpnext xrtHttpStructuredDictionaryFieldAt(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	size_t iIndex,
	xhttpstructureddictionarymember* pMember
)
{
	xhttpstructuredmapcursor Cursor;
	xhttpstructureddictionarymember Member;
	size_t iUnique = 0;
	xhttpnext Next;

	memset(&Member, 0, sizeof(Member));
	if ( !__xrtHttpStructuredDictionaryFieldArguments(
		pFields, iCount, Name, pMember, sizeof(Member)
	) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	xrtHttpStructuredMapCursorInit(&Cursor);
	while ( (Next = xrtHttpStructuredDictionaryMapFieldNext(
		pFields, iCount, Name, &Cursor, &Member
	)) == XHTTP_NEXT_ITEM ) {
		if ( iUnique++ == iIndex ) {
			memcpy(pMember, &Member, sizeof(Member));
			return Next;
		}
	}
	if ( Next == XHTTP_NEXT_END ) {
		memset(&Member, 0, sizeof(Member));
		memcpy(pMember, &Member, sizeof(Member));
	}
	return Next;
}



/* 按 key 读取跨字段 Dictionary 中最后一次成员值。 */
XRT_API xhttpnext xrtHttpStructuredDictionaryFieldFind(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	xstrview Key,
	xhttpstructureddictionarymember* pMember
)
{
	xhttpstructureddictionarymember Member;
	xstrview Parsed;
	size_t iKeyOffset = 0;
	size_t iMatches;

	memset(&Member, 0, sizeof(Member));
	if ( !__xrtHttpStructuredDictionaryFieldArguments(
		pFields, iCount, Name, pMember, sizeof(Member)
	) || !__xrtHttpViewValid(Key) ||
		__xrtRangesOverlap(
			Key.Data, Key.Size, pMember, sizeof(Member)
		) || !__xrtHttpStructuredKeyParse(
			Key, &iKeyOffset, &Parsed
		) || (iKeyOffset != Key.Size) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	if ( !__xrtHttpStructuredFieldsValidate(
		pFields, iCount, Name, true, &iMatches
	) ) {
		return XHTTP_NEXT_ERROR;
	}
	if ( !__xrtHttpStructuredDictionaryFieldLast(
		pFields, iCount, Name, Key, &Member
	) ) {
		memset(&Member, 0, sizeof(Member));
		memcpy(pMember, &Member, sizeof(Member));
		return XHTTP_NEXT_END;
	}
	memcpy(pMember, &Member, sizeof(Member));
	return XHTTP_NEXT_ITEM;
}



/* 解析唯一同名字段行中的 Structured Item。 */
XRT_API xhttpnext xrtHttpStructuredItemField(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	xhttpstructureditem* pItem
)
{
	xhttpstructureditem Item;
	xhttpfield Field;
	size_t iFound = XRT_NPOS;
	size_t i;

	memset(&Item, 0, sizeof(Item));
	if ( !__xrtHttpFieldArrayValid(pFields, iCount) ||
		!__xrtHttpViewValid(Name) || (Name.Size == 0) ||
		!xrtHttpTokenValid(Name) ||
		!__xrtRangeValid(pItem, sizeof(Item)) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount, pItem, sizeof(Item)
		) || __xrtRangesOverlap(
			Name.Data, Name.Size, pItem, sizeof(Item)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	for ( i = 0; i < iCount; i++ ) {
		__xrtHttpFieldLoad(pFields, i, &Field);
		if ( !xrtHttpFieldNameEqual(Field.Name, Name) ) {
			continue;
		}
		if ( iFound != XRT_NPOS ) {
			__xrtErrorSetValue();
			return XHTTP_NEXT_ERROR;
		}
		iFound = i;
	}
	if ( iFound == XRT_NPOS ) {
		memcpy(pItem, &Item, sizeof(Item));
		return XHTTP_NEXT_END;
	}
	__xrtHttpFieldLoad(pFields, iFound, &Field);
	if ( !xrtHttpStructuredItemParse(Field.Value, &Item) ) {
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pItem, &Item, sizeof(Item));
	return XHTTP_NEXT_ITEM;
}



/* 验证解码函数的描述符、目标和长度输出。 */
static bool __xrtHttpStructuredDecodeArguments(
	const xhttpstructuredbare* pBare,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpstructuredbare* pLoaded
)
{
	if ( !__xrtRangeValid(pBare, sizeof(*pBare)) ||
		!__xrtRangeValid(pSize, sizeof(*pSize)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		((pOutput != NULL) &&
		 !__xrtRangeValid(pOutput, iCapacity)) ||
		__xrtRangesOverlap(
			pBare, sizeof(*pBare), pSize, sizeof(*pSize)
		) || ((pOutput != NULL) && __xrtRangesOverlap(
			pBare, sizeof(*pBare), pOutput, iCapacity
		)) || ((pOutput != NULL) && __xrtRangesOverlap(
			pSize, sizeof(*pSize), pOutput, iCapacity
		)) ) {
		return false;
	}
	memcpy(pLoaded, pBare, sizeof(*pLoaded));
	if ( !__xrtHttpViewValid(pLoaded->Encoded) ||
		__xrtRangesOverlap(
			pLoaded->Encoded.Data, pLoaded->Encoded.Size,
			pSize, sizeof(*pSize)
		) ) {
		return false;
	}
	return true;
}



/* 验证解码容量以及允许的同址原地收缩。 */
static bool __xrtHttpStructuredDecodeOutput(
	xstrview Encoded,
	void* pOutput,
	size_t iCapacity,
	size_t iRequired,
	size_t* pSize
)
{
	if ( pOutput == NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		return true;
	}
	if ( iCapacity < iRequired ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		__xrtErrorSetRange();
		return false;
	}
	if ( __xrtRangesOverlap(
		pOutput, iRequired, Encoded.Data, Encoded.Size
	) && (pOutput != (const void*)Encoded.Data) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 解码 String 的反斜杠转义。 */
XRT_API bool xrtHttpStructuredStringDecode(
	const xhttpstructuredbare* pBare,
	char* sOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xhttpstructuredbare Bare;
	size_t iRequired;
	size_t iInput;
	size_t iOutput;

	if ( !__xrtHttpStructuredDecodeArguments(
		pBare, sOutput, iCapacity, pSize, &Bare
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (Bare.Type != XHTTP_STRUCTURED_STRING) ||
		(Bare.Number != 0) ||
		!__xrtHttpStructuredStringMeasure(
			Bare.Encoded, &iRequired
		) ) {
		if ( xrtGetError() == NULL ) {
			__xrtErrorSetValue();
		}
		return false;
	}
	if ( !__xrtHttpStructuredDecodeOutput(
		Bare.Encoded, sOutput, iCapacity, iRequired, pSize
	) || (sOutput == NULL) ) {
		return sOutput == NULL;
	}
	iInput = 0;
	iOutput = 0;
	while ( iInput < Bare.Encoded.Size ) {
		if ( Bare.Encoded.Data[iInput] == '\\' ) {
			iInput++;
		}
		sOutput[iOutput++] = Bare.Encoded.Data[iInput++];
	}
	memcpy(pSize, &iRequired, sizeof(iRequired));
	return true;
}



/* 解码 Byte Sequence。 */
XRT_API bool xrtHttpStructuredBytesDecode(
	const xhttpstructuredbare* pBare,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xhttpstructuredbare Bare;
	xbase64config Config;
	size_t iRequired;

	if ( !__xrtHttpStructuredDecodeArguments(
		pBare, pOutput, iCapacity, pSize, &Bare
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (Bare.Type != XHTTP_STRUCTURED_BYTES) ||
		(Bare.Number != 0) ) {
		__xrtErrorSetValue();
		return false;
	}
	Config.Alphabet = NULL;
	Config.Flags = (uint32)XBASE64_OPTIONAL_PADDING;
	if ( !xrtBase64Decode(
		Bare.Encoded.Data, Bare.Encoded.Size,
		NULL, 0, &iRequired, &Config
	) ) {
		return false;
	}
	if ( !__xrtHttpStructuredDecodeOutput(
		Bare.Encoded, pOutput, iCapacity, iRequired, pSize
	) || (pOutput == NULL) ) {
		return pOutput == NULL;
	}
	if ( !xrtBase64Decode(
		Bare.Encoded.Data, Bare.Encoded.Size,
		pOutput, iCapacity, &iRequired, &Config
	) ) {
		return false;
	}
	memcpy(pSize, &iRequired, sizeof(iRequired));
	return true;
}



/* 解码 Display String 的百分号字节并保留严格 UTF-8。 */
XRT_API bool xrtHttpStructuredDisplayDecode(
	const xhttpstructuredbare* pBare,
	char* sOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xhttpstructuredbare Bare;
	size_t iRequired;
	size_t iInput;
	size_t iOutput;

	if ( !__xrtHttpStructuredDecodeArguments(
		pBare, sOutput, iCapacity, pSize, &Bare
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (Bare.Type != XHTTP_STRUCTURED_DISPLAY) ||
		(Bare.Number != 0) ||
		!__xrtHttpStructuredDisplayMeasure(
			Bare.Encoded, &iRequired
		) ) {
		if ( xrtGetError() == NULL ) {
			__xrtErrorSetValue();
		}
		return false;
	}
	if ( !__xrtHttpStructuredDecodeOutput(
		Bare.Encoded, sOutput, iCapacity, iRequired, pSize
	) || (sOutput == NULL) ) {
		return sOutput == NULL;
	}
	iInput = 0;
	iOutput = 0;
	while ( iInput < Bare.Encoded.Size ) {
		unsigned char iByte = (unsigned char)Bare.Encoded.Data[iInput++];

		if ( iByte == (unsigned char)'%' ) {
			iByte = (unsigned char)(
				(__xrtHttpStructuredHexLower(
					(unsigned char)Bare.Encoded.Data[iInput]
				) << 4u) |
				__xrtHttpStructuredHexLower(
					(unsigned char)Bare.Encoded.Data[iInput + 1u]
				)
			);
			iInput += 2u;
		}
		sOutput[iOutput++] = (char)iByte;
	}
	memcpy(pSize, &iRequired, sizeof(iRequired));
	return true;
}

#endif
