#include "../internal/xrt_http.h"

#include <xrt/http_forwarded.h>



#if defined(XHTTP_FEATURE_HTTP_FORWARDED)

#define XRT_HTTP_FORWARDED_NODE_BUFFER 64u



/* 游标状态区分尚未绑定和已经绑定的不可变输入。 */
typedef enum xrt_http_forwarded_cursor_state {
	XRT_HTTP_FORWARDED_CURSOR_INITIAL = 0,
	XRT_HTTP_FORWARDED_CURSOR_VALUE,
	XRT_HTTP_FORWARDED_CURSOR_FIELDS
} xrt_http_forwarded_cursor_state;



/* 判断字节是否属于 ALPHA。 */
static bool __xrtHttpForwardedAlpha(uint8 iByte)
{
	return ((iByte >= (uint8)'A') && (iByte <= (uint8)'Z')) ||
		((iByte >= (uint8)'a') && (iByte <= (uint8)'z'));
}



/* 判断字节是否属于 DIGIT。 */
static bool __xrtHttpForwardedDigit(uint8 iByte)
{
	return (iByte >= (uint8)'0') && (iByte <= (uint8)'9');
}



/* 判断字节是否可以出现在混淆节点或端口中。 */
static bool __xrtHttpForwardedObfuscatedByte(uint8 iByte)
{
	return __xrtHttpForwardedAlpha(iByte) ||
		__xrtHttpForwardedDigit(iByte) ||
		(iByte == (uint8)'.') ||
		(iByte == (uint8)'_') ||
		(iByte == (uint8)'-');
}



/* 验证节点端口是 1 到 5 位数字或非空混淆标识。 */
static bool __xrtHttpForwardedNodePortValid(xstrview Port)
{
	size_t i;

	if ( Port.Size == 0 ) {
		return false;
	}
	if ( Port.Data[0] == '_' ) {
		if ( Port.Size == 1u ) {
			return false;
		}
		for ( i = 1u; i < Port.Size; i++ ) {
			if ( !__xrtHttpForwardedObfuscatedByte(
				(uint8)Port.Data[i]
			) ) {
				return false;
			}
		}
		return true;
	}
	if ( Port.Size > 5u ) {
		return false;
	}
	for ( i = 0; i < Port.Size; i++ ) {
		if ( !__xrtHttpForwardedDigit((uint8)Port.Data[i]) ) {
			return false;
		}
	}
	return true;
}



/* 验证已经解码的混淆节点名称。 */
static bool __xrtHttpForwardedObfuscatedValid(xstrview Name)
{
	size_t i;

	if ( (Name.Size < 2u) || (Name.Data[0] != '_') ) {
		return false;
	}
	for ( i = 1u; i < Name.Size; i++ ) {
		if ( !__xrtHttpForwardedObfuscatedByte(
			(uint8)Name.Data[i]
		) ) {
			return false;
		}
	}
	return true;
}



/* 验证已经解码的 by 或 for 节点。 */
XRT_API bool xrtHttpForwardedNodeValid(xstrview Node)
{
	xstrview Name;
	xstrview Port = { NULL, 0 };
	cstr sColon;
	cstr sClose;

	if ( !__xrtHttpViewValid(Node) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( Node.Size == 0 ) {
		return false;
	}
	if ( Node.Data[0] == '[' ) {
		sClose = (cstr)memchr(Node.Data + 1u, ']', Node.Size - 1u);
		if ( (sClose == NULL) || (sClose == (Node.Data + 1u)) ) {
			return false;
		}
		Name = (xstrview){
			Node.Data + 1u,
			(size_t)(sClose - (Node.Data + 1u))
		};
		if ( !xrtHttpIpv6Valid(Name) ) {
			return false;
		}
		if ( (size_t)(sClose - Node.Data + 1u) == Node.Size ) {
			return true;
		}
		if ( sClose[1] != ':' ) {
			return false;
		}
		Port = (xstrview){
			sClose + 2u,
			Node.Size - (size_t)(sClose + 2u - Node.Data)
		};
		return __xrtHttpForwardedNodePortValid(Port);
	}
	sColon = (cstr)memchr(Node.Data, ':', Node.Size);
	if ( sColon == NULL ) {
		Name = Node;
	} else {
		Name = (xstrview){
			Node.Data, (size_t)(sColon - Node.Data)
		};
		Port = (xstrview){
			sColon + 1u,
			Node.Size - (size_t)(sColon + 1u - Node.Data)
		};
	}
	if ( !xrtHttpIpv4Valid(Name) &&
		!xrtHttpTokenEqual(Name, XRT_STR_LITERAL("unknown")) &&
		!__xrtHttpForwardedObfuscatedValid(Name) ) {
		return false;
	}
	return (sColon == NULL) ||
		__xrtHttpForwardedNodePortValid(Port);
}



/* 验证已经解码的 proto 参数是 URI scheme。 */
XRT_API bool xrtHttpForwardedProtoValid(xstrview Proto)
{
	size_t i;

	if ( !__xrtHttpViewValid(Proto) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (Proto.Size == 0) ||
		!__xrtHttpForwardedAlpha((uint8)Proto.Data[0]) ) {
		return false;
	}
	for ( i = 1u; i < Proto.Size; i++ ) {
		uint8 iByte = (uint8)Proto.Data[i];

		if ( !__xrtHttpForwardedAlpha(iByte) &&
			!__xrtHttpForwardedDigit(iByte) &&
			(iByte != (uint8)'+') &&
			(iByte != (uint8)'-') &&
			(iByte != (uint8)'.') ) {
			return false;
		}
	}
	return true;
}



/* 参数语义读取器在 quoted-string 上跳过 quoted-pair 的反斜线。 */
typedef struct xrt_http_forwarded_reader {
	const xhttpparam* Param;
	size_t Offset;
} xrt_http_forwarded_reader;



/* 读取下一个已经解码的参数字节。 */
static bool __xrtHttpForwardedRead(
	xrt_http_forwarded_reader* pReader,
	uint8* pByte
)
{
	return __xrtHttpParamSemanticNext(
		pReader->Param, &pReader->Offset, pByte
	);
}



/* 从当前位置严格读取非空节点端口。 */
static bool __xrtHttpForwardedNodePortRead(
	xrt_http_forwarded_reader* pReader
)
{
	uint8 iByte;
	size_t iCount = 0;
	bool bObfuscated;

	if ( !__xrtHttpForwardedRead(pReader, &iByte) ) {
		return false;
	}
	bObfuscated = iByte == (uint8)'_';
	if ( !bObfuscated && !__xrtHttpForwardedDigit(iByte) ) {
		return false;
	}
	while ( __xrtHttpForwardedRead(pReader, &iByte) ) {
		if ( bObfuscated ) {
			if ( !__xrtHttpForwardedObfuscatedByte(iByte) ) {
				return false;
			}
		} else if ( !__xrtHttpForwardedDigit(iByte) ) {
			return false;
		}
		iCount++;
	}
	return bObfuscated ? (iCount != 0) : (iCount < 5u);
}



/* 验证参数解码后的节点，并检查含端口或 IPv6 时必须使用引号。 */
static bool __xrtHttpForwardedNodeParamValid(
	const xhttpparam* pParam
)
{
	xrt_http_forwarded_reader Reader = { pParam, 0 };
	char sName[XRT_HTTP_FORWARDED_NODE_BUFFER];
	xstrview Name;
	uint8 iByte;
	size_t iSize = 0;
	bool bQuoted;

	bQuoted = (pParam->Flags & XHTTP_PARAM_QUOTED) != 0;
	if ( !__xrtHttpForwardedRead(&Reader, &iByte) ) {
		return false;
	}
	if ( iByte == (uint8)'[' ) {
		if ( !bQuoted ) {
			return false;
		}
		while ( __xrtHttpForwardedRead(&Reader, &iByte) &&
			(iByte != (uint8)']') ) {
			if ( iSize == sizeof(sName) ) {
				return false;
			}
			sName[iSize++] = (char)iByte;
		}
		Name = (xstrview){ sName, iSize };
		if ( (iByte != (uint8)']') ||
			!xrtHttpIpv6Valid(Name) ) {
			return false;
		}
		if ( !__xrtHttpForwardedRead(&Reader, &iByte) ) {
			return true;
		}
		return (iByte == (uint8)':') &&
			__xrtHttpForwardedNodePortRead(&Reader);
	}
	if ( iByte == (uint8)'_' ) {
		while ( __xrtHttpForwardedRead(&Reader, &iByte) ) {
			if ( iByte == (uint8)':' ) {
				return (iSize != 0) && bQuoted &&
					__xrtHttpForwardedNodePortRead(&Reader);
			}
			if ( !__xrtHttpForwardedObfuscatedByte(iByte) ) {
				return false;
			}
			iSize++;
		}
		return iSize != 0;
	}
	sName[iSize++] = (char)iByte;
	while ( __xrtHttpForwardedRead(&Reader, &iByte) ) {
		if ( iByte == (uint8)':' ) {
			break;
		}
		if ( iSize == sizeof(sName) ) {
			return false;
		}
		sName[iSize++] = (char)iByte;
	}
	Name = (xstrview){ sName, iSize };
	if ( !xrtHttpIpv4Valid(Name) &&
		!xrtHttpTokenEqual(Name, XRT_STR_LITERAL("unknown")) ) {
		return false;
	}
	if ( iByte != (uint8)':' ) {
		return true;
	}
	return bQuoted && __xrtHttpForwardedNodePortRead(&Reader);
}



/* 验证参数语义字节符合共享 HTTP Host authority 语法。 */
static bool __xrtHttpForwardedHostParamValid(
	const xhttpparam* pParam
)
{
	return xrtHttpParamHostValid(pParam);
}



/* 验证已经解码的 host 参数值。 */
XRT_API bool xrtHttpForwardedHostValid(xstrview Host)
{
	if ( !__xrtHttpViewValid(Host) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return xrtHttpHostValid(Host);
}



/* 验证参数解码后的协议名。 */
static bool __xrtHttpForwardedProtoParamValid(
	const xhttpparam* pParam
)
{
	size_t iOffset = 0;
	size_t iCount = 0;
	uint8 iByte;

	while ( __xrtHttpParamSemanticNext(
		pParam, &iOffset, &iByte
	) ) {
		if ( (iCount == 0) ?
			!__xrtHttpForwardedAlpha(iByte) :
			(!__xrtHttpForwardedAlpha(iByte) &&
			 !__xrtHttpForwardedDigit(iByte) &&
			 (iByte != (uint8)'+') &&
			 (iByte != (uint8)'-') &&
			 (iByte != (uint8)'.')) ) {
			return false;
		}
		iCount++;
	}
	return iCount != 0;
}



/* 判断标准参数名称。 */
static uint32 __xrtHttpForwardedKnown(xstrview Name)
{
	if ( xrtHttpTokenEqual(Name, XRT_STR_LITERAL("for")) ) {
		return XHTTP_FORWARDED_HAS_FOR;
	}
	if ( xrtHttpTokenEqual(Name, XRT_STR_LITERAL("by")) ) {
		return XHTTP_FORWARDED_HAS_BY;
	}
	if ( xrtHttpTokenEqual(Name, XRT_STR_LITERAL("host")) ) {
		return XHTTP_FORWARDED_HAS_HOST;
	}
	if ( xrtHttpTokenEqual(Name, XRT_STR_LITERAL("proto")) ) {
		return XHTTP_FORWARDED_HAS_PROTO;
	}
	return 0;
}



/* 检查当前偏移之前是否已有同名参数。 */
static bool __xrtHttpForwardedPairSeen(
	xstrview Element,
	size_t iBefore,
	xstrview Name
)
{
	xhttpparam Pair;
	size_t iOffset = 0;

	while ( iOffset < iBefore ) {
		if ( xrtHttpForwardedPairNext(
			Element, &iOffset, &Pair
		) != XHTTP_NEXT_ITEM ) {
			return false;
		}
		if ( xrtHttpTokenEqual(Pair.Name, Name) ) {
			return true;
		}
	}
	return false;
}



/* 严格读取一个 Forwarded 参数。 */
XRT_API xhttpnext xrtHttpForwardedPairNext(
	xstrview Element,
	size_t* pOffset,
	xhttpparam* pPair
)
{
	xhttpparam Pair;
	xhttpnext Next;
	size_t iOffset;
	size_t iOriginal;
	size_t iName;
	size_t iEnd;
	size_t iExpected;
	size_t i;

	if ( !__xrtHttpViewValid(Element) ||
		!__xrtRangeValid(pOffset, sizeof(iOffset)) ||
		!__xrtRangeValid(pPair, sizeof(Pair)) ||
		__xrtRangesOverlap(
			pOffset, sizeof(iOffset), pPair, sizeof(Pair)
		) || __xrtRangesOverlap(
			pOffset, sizeof(iOffset), Element.Data, Element.Size
		) || __xrtRangesOverlap(
			pPair, sizeof(Pair), Element.Data, Element.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&iOffset, pOffset, sizeof(iOffset));
	iOriginal = iOffset;
	memset(&Pair, 0, sizeof(Pair));
	Next = __xrtHttpNameValueNext(
		Element, &iOffset, &Pair, ';', true
	);
	if ( Next == XHTTP_NEXT_ERROR ) {
		return Next;
	}
	if ( (Next == XHTTP_NEXT_ITEM) &&
		((Pair.Flags & XHTTP_PARAM_HAS_VALUE) == 0) ) {
		__xrtErrorSetValue();
		return XHTTP_NEXT_ERROR;
	}
	if ( Next == XHTTP_NEXT_ITEM ) {
		iName = (size_t)(Pair.Name.Data - Element.Data);
		for ( i = iOriginal; i < iName; i++ ) {
			if ( Element.Data[i] != ';' ) {
				__xrtErrorSetValue();
				return XHTTP_NEXT_ERROR;
			}
		}
		if ( ((iName + Pair.Name.Size) >= Element.Size) ||
			(Element.Data[iName + Pair.Name.Size] != '=') ) {
			__xrtErrorSetValue();
			return XHTTP_NEXT_ERROR;
		}
		if ( (Pair.Flags & XHTTP_PARAM_QUOTED) != 0 ) {
			if ( ((iName + Pair.Name.Size + 1u) >= Element.Size) ||
				(Element.Data[iName + Pair.Name.Size + 1u] != '"') ||
				(Pair.Value.Data !=
				 (Element.Data + iName + Pair.Name.Size + 2u)) ) {
				__xrtErrorSetValue();
				return XHTTP_NEXT_ERROR;
			}
			iEnd = (size_t)(Pair.Value.Data - Element.Data) +
				Pair.Value.Size + 1u;
		} else {
			if ( Pair.Value.Data !=
				(Element.Data + iName + Pair.Name.Size + 1u) ) {
				__xrtErrorSetValue();
				return XHTTP_NEXT_ERROR;
			}
			iEnd = (size_t)(Pair.Value.Data - Element.Data) +
				Pair.Value.Size;
		}
		if ( iEnd > Element.Size ) {
			__xrtErrorSetValue();
			return XHTTP_NEXT_ERROR;
		}
		iExpected = iEnd;
		if ( iEnd < Element.Size ) {
			if ( Element.Data[iEnd] != ';' ) {
				__xrtErrorSetValue();
				return XHTTP_NEXT_ERROR;
			}
			iExpected++;
		}
		if ( iOffset != iExpected ) {
			__xrtErrorSetValue();
			return XHTTP_NEXT_ERROR;
		}
	}
	memcpy(pPair, &Pair, sizeof(Pair));
	memcpy(pOffset, &iOffset, sizeof(iOffset));
	return Next;
}



/* 严格解析一个代理元素。 */
XRT_API bool xrtHttpForwardedElementParse(
	xstrview Element,
	xhttpforwarded* pOutput
)
{
	xhttpforwarded Forwarded;
	xhttpparam Pair;
	xhttpnext Next;
	size_t iOffset = 0;
	size_t iBefore;
	uint32 iKnown;

	if ( !__xrtHttpViewValid(Element) ||
		!__xrtRangeValid(pOutput, sizeof(Forwarded)) ||
		__xrtRangesOverlap(
			pOutput, sizeof(Forwarded), Element.Data, Element.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(&Forwarded, 0, sizeof(Forwarded));
	Forwarded.Element = xrtHttpOwsTrim(Element);
	for ( ;; ) {
		iBefore = iOffset;
		Next = xrtHttpForwardedPairNext(
			Forwarded.Element, &iOffset, &Pair
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			break;
		}
		iKnown = __xrtHttpForwardedKnown(Pair.Name);
		if ( ((iKnown != 0) &&
			 ((Forwarded.Flags & iKnown) != 0)) ||
			((iKnown == 0) && __xrtHttpForwardedPairSeen(
			 Forwarded.Element, iBefore, Pair.Name
			)) ) {
			__xrtErrorSetValue();
			return false;
		}
		if ( iKnown == XHTTP_FORWARDED_HAS_FOR ) {
			if ( !__xrtHttpForwardedNodeParamValid(&Pair) ) {
				__xrtErrorSetValue();
				return false;
			}
			Forwarded.For = Pair;
		} else if ( iKnown == XHTTP_FORWARDED_HAS_BY ) {
			if ( !__xrtHttpForwardedNodeParamValid(&Pair) ) {
				__xrtErrorSetValue();
				return false;
			}
			Forwarded.By = Pair;
		} else if ( iKnown == XHTTP_FORWARDED_HAS_HOST ) {
			if ( !__xrtHttpForwardedHostParamValid(&Pair) ) {
				if ( xrtGetError() == NULL ) {
					__xrtErrorSetValue();
				}
				return false;
			}
			Forwarded.Host = Pair;
		} else if ( iKnown == XHTTP_FORWARDED_HAS_PROTO ) {
			if ( !__xrtHttpForwardedProtoParamValid(&Pair) ) {
				__xrtErrorSetValue();
				return false;
			}
			Forwarded.Proto = Pair;
		}
		Forwarded.Flags |= iKnown;
		if ( Forwarded.PairCount == SIZE_MAX ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		Forwarded.PairCount++;
	}
	memcpy(pOutput, &Forwarded, sizeof(Forwarded));
	return true;
}



/* 完整验证一个字段值并要求至少一个非空元素。 */
static bool __xrtHttpForwardedValueValidate(
	xstrview Value,
	size_t* pCount
)
{
	xhttpforwarded Forwarded;
	xstrview Element;
	xhttpnext Next;
	size_t iOffset = 0;
	size_t iNext;
	size_t iCount = 0;

	if ( !__xrtHttpViewValid(Value) ) {
		return false;
	}
	for ( ;; ) {
		Next = __xrtHttpQuotedListNext(
			Value, iOffset, &iNext, &Element
		);
		if ( Next == XHTTP_NEXT_END ) {
			break;
		}
		if ( !xrtHttpForwardedElementParse(
			Element, &Forwarded
		) ) {
			return false;
		}
		iOffset = iNext;
		if ( iCount == SIZE_MAX ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iCount++;
	}
	if ( iCount == 0 ) {
		__xrtErrorSetValue();
		return false;
	}
	*pCount = iCount;
	return true;
}



/* 初始化单字段游标。 */
XRT_API void xrtHttpForwardedCursorInit(
	xhttpforwardedcursor* pCursor
)
{
	xhttpforwardedcursor Cursor;

	if ( !__xrtRangeValid(pCursor, sizeof(Cursor)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(&Cursor, 0, sizeof(Cursor));
	memcpy(pCursor, &Cursor, sizeof(Cursor));
}



/* 初始化重复字段游标。 */
XRT_API void xrtHttpForwardedFieldCursorInit(
	xhttpforwardedfieldcursor* pCursor
)
{
	xhttpforwardedfieldcursor Cursor;

	if ( !__xrtRangeValid(pCursor, sizeof(Cursor)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(&Cursor, 0, sizeof(Cursor));
	memcpy(pCursor, &Cursor, sizeof(Cursor));
}



/* 验证完整 Forwarded 字段值。 */
XRT_API bool xrtHttpForwardedValid(xstrview Value)
{
	size_t iCount;

	if ( !__xrtHttpViewValid(Value) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtHttpForwardedValueValidate(Value, &iCount);
}



/* 完整验证并统计一个 Forwarded 字段值中的代理元素。 */
XRT_API bool xrtHttpForwardedCount(
	xstrview Value,
	size_t* pCount
)
{
	size_t iCount;

	if ( !__xrtHttpViewValid(Value) ||
		!__xrtRangeValid(pCount, sizeof(iCount)) ||
		__xrtRangesOverlap(
			Value.Data, Value.Size, pCount, sizeof(iCount)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpForwardedValueValidate(
		Value, &iCount
	) ) {
		return false;
	}
	memcpy(pCount, &iCount, sizeof(iCount));
	return true;
}



/* 判断单字段游标是否仍是初始化后的全零状态。 */
static bool __xrtHttpForwardedCursorInitial(
	const xhttpforwardedcursor* pCursor
)
{
	return (pCursor->Source == NULL) &&
		(pCursor->SourceSize == 0) &&
		(pCursor->Offset == 0) &&
		(pCursor->State ==
		 (uint8)XRT_HTTP_FORWARDED_CURSOR_INITIAL);
}



/* 验证单字段游标仍绑定同一个不可变字段值。 */
static bool __xrtHttpForwardedCursorValid(
	const xhttpforwardedcursor* pCursor,
	xstrview Value
)
{
	if ( pCursor->State ==
		(uint8)XRT_HTTP_FORWARDED_CURSOR_INITIAL ) {
		return __xrtHttpForwardedCursorInitial(pCursor);
	}
	return (pCursor->State ==
		 (uint8)XRT_HTTP_FORWARDED_CURSOR_VALUE) &&
		(pCursor->Source == (const void*)Value.Data) &&
		(pCursor->SourceSize == Value.Size) &&
		(pCursor->Offset <= Value.Size);
}



/* 迭代一个完整 Forwarded 字段值。 */
XRT_API xhttpnext xrtHttpForwardedNext(
	xstrview Value,
	xhttpforwardedcursor* pCursor,
	xhttpforwarded* pOutput
)
{
	xhttpforwardedcursor Cursor;
	xhttpforwarded Forwarded;
	xstrview Element;
	xhttpnext Next;
	size_t iNext;
	size_t iCount;

	if ( !__xrtHttpViewValid(Value) ||
		!__xrtRangeValid(pCursor, sizeof(Cursor)) ||
		!__xrtRangeValid(pOutput, sizeof(Forwarded)) ||
		__xrtRangesOverlap(
			pCursor, sizeof(Cursor), pOutput, sizeof(Forwarded)
		) || __xrtRangesOverlap(
			pCursor, sizeof(Cursor), Value.Data, Value.Size
		) || __xrtRangesOverlap(
			pOutput, sizeof(Forwarded), Value.Data, Value.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&Cursor, pCursor, sizeof(Cursor));
	if ( !__xrtHttpForwardedCursorValid(&Cursor, Value) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	if ( Cursor.State ==
		(uint8)XRT_HTTP_FORWARDED_CURSOR_INITIAL ) {
		if ( !__xrtHttpForwardedValueValidate(
			Value, &iCount
		) ) {
			return XHTTP_NEXT_ERROR;
		}
		Cursor.Source = Value.Data;
		Cursor.SourceSize = Value.Size;
		Cursor.State = (uint8)XRT_HTTP_FORWARDED_CURSOR_VALUE;
	}
	Next = __xrtHttpQuotedListNext(
		Value, Cursor.Offset, &iNext, &Element
	);
	memset(&Forwarded, 0, sizeof(Forwarded));
	if ( Next == XHTTP_NEXT_END ) {
		Cursor.Offset = iNext;
		memcpy(pOutput, &Forwarded, sizeof(Forwarded));
		memcpy(pCursor, &Cursor, sizeof(Cursor));
		return Next;
	}
	if ( !xrtHttpForwardedElementParse(
		Element, &Forwarded
	) ) {
		return XHTTP_NEXT_ERROR;
	}
	Cursor.Offset = iNext;
	memcpy(pOutput, &Forwarded, sizeof(Forwarded));
	memcpy(pCursor, &Cursor, sizeof(Cursor));
	return XHTTP_NEXT_ITEM;
}



/* 完整预校验所有重复 Forwarded 字段行。 */
static bool __xrtHttpForwardedFieldsValidate(
	const xhttpfield* pFields,
	size_t iCount,
	size_t* pElements
)
{
	xhttpfield Field;
	size_t iElements = 0;
	size_t iFieldElements;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		__xrtHttpFieldLoad(pFields, i, &Field);
		if ( xrtHttpFieldNameEqual(
			Field.Name, XRT_STR_LITERAL("Forwarded")
		) ) {
			if ( !__xrtHttpForwardedValueValidate(
				Field.Value, &iFieldElements
			) ) {
				return false;
			}
			if ( iFieldElements > (SIZE_MAX - iElements) ) {
				__xrtErrorSetSizeOverflow();
				return false;
			}
			iElements += iFieldElements;
		}
	}
	*pElements = iElements;
	return true;
}



/* 完整验证并统计全部 Forwarded 字段行。 */
XRT_API bool xrtHttpForwardedFieldCount(
	const xhttpfield* pFields,
	size_t iCount,
	size_t* pCount
)
{
	size_t iElements;

	if ( !__xrtHttpFieldArrayValid(pFields, iCount) ||
		!__xrtRangeValid(pCount, sizeof(iElements)) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount, pCount, sizeof(iElements)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpForwardedFieldsValidate(
		pFields, iCount, &iElements
	) ) {
		return false;
	}
	memcpy(pCount, &iElements, sizeof(iElements));
	return true;
}



/* 判断重复字段游标是否仍是初始化后的全零状态。 */
static bool __xrtHttpForwardedFieldCursorInitial(
	const xhttpforwardedfieldcursor* pCursor
)
{
	return (pCursor->Source == NULL) &&
		(pCursor->SourceSize == 0) &&
		(pCursor->Field == 0) &&
		(pCursor->Offset == 0) &&
		(pCursor->State ==
		 (uint8)XRT_HTTP_FORWARDED_CURSOR_INITIAL);
}



/* 验证重复字段游标仍绑定同一个不可变字段数组。 */
static bool __xrtHttpForwardedFieldCursorValid(
	const xhttpforwardedfieldcursor* pCursor,
	const xhttpfield* pFields,
	size_t iCount
)
{
	if ( pCursor->State ==
		(uint8)XRT_HTTP_FORWARDED_CURSOR_INITIAL ) {
		return __xrtHttpForwardedFieldCursorInitial(pCursor);
	}
	return (pCursor->State ==
		 (uint8)XRT_HTTP_FORWARDED_CURSOR_FIELDS) &&
		(pCursor->Source == (const void*)pFields) &&
		(pCursor->SourceSize == iCount) &&
		(pCursor->Field <= iCount) &&
		!((pCursor->Field == iCount) && (pCursor->Offset != 0));
}



/* 跨重复字段行迭代代理链路。 */
XRT_API xhttpnext xrtHttpForwardedFieldNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpforwardedfieldcursor* pCursor,
	xhttpforwarded* pOutput
)
{
	xhttpforwardedfieldcursor Cursor;
	xhttpforwarded Forwarded;
	xhttpfield Field;
	xstrview Element;
	xhttpnext Next;
	size_t iNext;
	size_t iElements;

	if ( !__xrtHttpFieldArrayValid(pFields, iCount) ||
		!__xrtRangeValid(pCursor, sizeof(Cursor)) ||
		!__xrtRangeValid(pOutput, sizeof(Forwarded)) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount, pCursor, sizeof(Cursor)
		) || __xrtHttpFieldArrayOverlap(
			pFields, iCount, pOutput, sizeof(Forwarded)
		) || __xrtRangesOverlap(
			pCursor, sizeof(Cursor), pOutput, sizeof(Forwarded)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&Cursor, pCursor, sizeof(Cursor));
	if ( !__xrtHttpForwardedFieldCursorValid(
		&Cursor, pFields, iCount
	) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	if ( Cursor.State ==
		(uint8)XRT_HTTP_FORWARDED_CURSOR_INITIAL ) {
		if ( !__xrtHttpForwardedFieldsValidate(
			pFields, iCount, &iElements
		) ) {
			return XHTTP_NEXT_ERROR;
		}
		Cursor.Source = pFields;
		Cursor.SourceSize = iCount;
		Cursor.State = (uint8)XRT_HTTP_FORWARDED_CURSOR_FIELDS;
	}
	while ( Cursor.Field < iCount ) {
		__xrtHttpFieldLoad(pFields, Cursor.Field, &Field);
		if ( !xrtHttpFieldNameEqual(
			Field.Name, XRT_STR_LITERAL("Forwarded")
		) ) {
			Cursor.Field++;
			Cursor.Offset = 0;
			continue;
		}
		Next = __xrtHttpQuotedListNext(
			Field.Value, Cursor.Offset, &iNext, &Element
		);
		if ( Next == XHTTP_NEXT_END ) {
			Cursor.Field++;
			Cursor.Offset = 0;
			continue;
		}
		if ( !xrtHttpForwardedElementParse(
			Element, &Forwarded
		) ) {
			return XHTTP_NEXT_ERROR;
		}
		Cursor.Offset = iNext;
		memcpy(pOutput, &Forwarded, sizeof(Forwarded));
		memcpy(pCursor, &Cursor, sizeof(Cursor));
		return XHTTP_NEXT_ITEM;
	}
	memset(&Forwarded, 0, sizeof(Forwarded));
	memcpy(pOutput, &Forwarded, sizeof(Forwarded));
	memcpy(pCursor, &Cursor, sizeof(Cursor));
	return XHTTP_NEXT_END;
}

#endif
