#include "../internal/xrt_http.h"

#include <xrt/http_cors_safelist.h>



#if defined(XRT_FEATURE_HTTP_CORS_SAFELIST)

/* 判断字段名是否与固定 ASCII 名称匹配。 */
static bool __xrtHttpCorsName(
	xstrview Name,
	cstr pExpected,
	size_t iSize
)
{
	return xrtHttpFieldNameEqual(
		Name, (xstrview){ pExpected, iSize }
	);
}



/* 判断 Accept 值是否避开 CORS 不安全字节。 */
static bool __xrtHttpCorsSafeBytes(xstrview Value)
{
	size_t i;

	for ( i = 0; i < Value.Size; i++ ) {
		if ( xrtHttpCorsRequestByteUnsafe(
			(uint8)Value.Data[i]
		) ) {
			return false;
		}
	}
	return true;
}



/* 判断语言值只包含 Fetch safelist 允许的字节。 */
static bool __xrtHttpCorsLanguageValue(xstrview Value)
{
	size_t i;

	for ( i = 0; i < Value.Size; i++ ) {
		uint8 iByte = (uint8)Value.Data[i];

		if ( ((iByte >= '0') && (iByte <= '9')) ||
			((iByte >= 'A') && (iByte <= 'Z')) ||
			((iByte >= 'a') && (iByte <= 'z')) ||
			(iByte == ' ') || (iByte == '*') ||
			(iByte == ',') || (iByte == '-') ||
			(iByte == '.') || (iByte == ';') ||
			(iByte == '=') ) {
			continue;
		}
		return false;
	}
	return true;
}



/* 跳过十进制数的前导零，并保留至少一位。 */
static xstrview __xrtHttpCorsDecimalNormalize(xstrview Text)
{
	size_t i = 0;

	while ( ((i + 1u) < Text.Size) &&
		(Text.Data[i] == '0') ) {
		i++;
	}
	return (xstrview){ Text.Data + i, Text.Size - i };
}



/* 判断一个 Range 值是带首位置的单字节区间。 */
static bool __xrtHttpCorsRangeValue(xstrview Value)
{
	xstrview Start;
	xstrview End;
	size_t iStart;
	size_t i;

	if ( (Value.Size < 8u) ||
		(memcmp(Value.Data, "bytes=", 6u) != 0) ) {
		return false;
	}
	i = 6u;
	iStart = i;
	while ( (i < Value.Size) &&
		(Value.Data[i] >= '0') &&
		(Value.Data[i] <= '9') ) {
		i++;
	}
	if ( (i == iStart) || (i >= Value.Size) ||
		(Value.Data[i] != '-') ) {
		return false;
	}
	Start = (xstrview){ Value.Data + iStart, i - iStart };
	i++;
	iStart = i;
	while ( (i < Value.Size) &&
		(Value.Data[i] >= '0') &&
		(Value.Data[i] <= '9') ) {
		i++;
	}
	if ( i != Value.Size ) {
		return false;
	}
	if ( iStart == i ) {
		return true;
	}
	End = (xstrview){ Value.Data + iStart, i - iStart };
	Start = __xrtHttpCorsDecimalNormalize(Start);
	End = __xrtHttpCorsDecimalNormalize(End);
	if ( Start.Size != End.Size ) {
		return Start.Size < End.Size;
	}
	return memcmp(Start.Data, End.Data, Start.Size) <= 0;
}



/* 判断 Content-Type essence 是否在 CORS safelist 中。 */
static bool __xrtHttpCorsContentType(xstrview Value)
{
	xmediatype Type;

	if ( !__xrtHttpCorsSafeBytes(Value) ||
		!__xrtMimeSniffTypeParse(Value, &Type) ) {
		return false;
	}
	return (xrtHttpTokenEqual(
		Type.Type, XRT_STR_LITERAL("application")
	) && xrtHttpTokenEqual(
		Type.Subtype,
		XRT_STR_LITERAL("x-www-form-urlencoded")
	)) || (xrtHttpTokenEqual(
		Type.Type, XRT_STR_LITERAL("multipart")
	) && xrtHttpTokenEqual(
		Type.Subtype, XRT_STR_LITERAL("form-data")
	)) || (xrtHttpTokenEqual(
		Type.Type, XRT_STR_LITERAL("text")
	) && xrtHttpTokenEqual(
		Type.Subtype, XRT_STR_LITERAL("plain")
	));
}



/* 判断 Fetch 定义的不安全请求字段字节。 */
XRT_API bool xrtHttpCorsRequestByteUnsafe(uint8 iByte)
{
	if ( (iByte < 0x20u) && (iByte != 0x09u) ) {
		return true;
	}
	return (iByte == 0x22u) || (iByte == 0x28u) ||
		(iByte == 0x29u) || (iByte == 0x3Au) ||
		(iByte == 0x3Cu) || (iByte == 0x3Eu) ||
		(iByte == 0x3Fu) || (iByte == 0x40u) ||
		(iByte == 0x5Bu) || (iByte == 0x5Cu) ||
		(iByte == 0x5Du) || (iByte == 0x7Bu) ||
		(iByte == 0x7Du) || (iByte == 0x7Fu);
}



/* 判断 CORS safelist 方法。 */
XRT_API bool xrtHttpCorsMethodSafelisted(xstrview Method)
{
	return xrtHttpMethodEqual(
		Method, XRT_STR_LITERAL("GET")
	) || xrtHttpMethodEqual(
		Method, XRT_STR_LITERAL("HEAD")
	) || xrtHttpMethodEqual(
		Method, XRT_STR_LITERAL("POST")
	);
}



/* 判断单个 CORS safelist 请求字段。 */
XRT_API bool xrtHttpCorsRequestHeaderSafelisted(
	xstrview Name,
	xstrview Value
)
{
	xstrview Trimmed;

	if ( !__xrtHttpViewValid(Name) ||
		!__xrtHttpViewValid(Value) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtHttpTokenValid(Name) || (Value.Size > 128u) ) {
		return false;
	}
	Trimmed = xrtHttpOwsTrim(Value);
	if ( Trimmed.Size != Value.Size ) {
		return false;
	}
	if ( __xrtHttpCorsName(Name, "Accept", 6u) ) {
		return __xrtHttpCorsSafeBytes(Value);
	}
	if ( __xrtHttpCorsName(Name, "Accept-Language", 15u) ||
		__xrtHttpCorsName(Name, "Content-Language", 16u) ) {
		return __xrtHttpCorsLanguageValue(Value);
	}
	if ( __xrtHttpCorsName(Name, "Content-Type", 12u) ) {
		return __xrtHttpCorsContentType(Value);
	}
	if ( __xrtHttpCorsName(Name, "Range", 5u) ) {
		return __xrtHttpCorsRangeValue(Value);
	}
	return false;
}



/* 判断完整字段数组和安全值总量。 */
XRT_API bool xrtHttpCorsRequestFieldsSafelisted(
	const xhttpfield* pFields,
	size_t iCount
)
{
	xhttpfield Field;
	size_t iTotal = 0;
	size_t i;

	if ( !__xrtHttpFieldArrayValid(pFields, iCount) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		size_t j;

		__xrtHttpFieldLoad(pFields, i, &Field);
		if ( !xrtHttpCorsRequestHeaderSafelisted(
			Field.Name, Field.Value
		) || (Field.Value.Size > (1024u - iTotal)) ) {
			return false;
		}

		/* 单值字段不能通过重复的安全值绕过完整值检查。 */
		if ( __xrtHttpCorsName(
			Field.Name, "Content-Type", 12u
		) || __xrtHttpCorsName(
			Field.Name, "Range", 5u
		) ) {
			xhttpfield Previous;

			for ( j = 0; j < i; j++ ) {
				__xrtHttpFieldLoad(pFields, j, &Previous);
				if ( xrtHttpFieldNameEqual(
					Previous.Name, Field.Name
				) ) {
					return false;
				}
			}
		}
		iTotal += Field.Value.Size;
	}
	return true;
}



/* 判断 CORS 非通配请求字段名。 */
XRT_API bool xrtHttpCorsRequestHeaderNonWildcard(xstrview Name)
{
	return xrtHttpTokenValid(Name) && __xrtHttpCorsName(
		Name, "Authorization", 13u
	);
}



/* 判断 CORS 默认可见响应字段。 */
XRT_API bool xrtHttpCorsResponseHeaderSafelisted(xstrview Name)
{
	if ( !xrtHttpTokenValid(Name) ) {
		return false;
	}
	return __xrtHttpCorsName(Name, "Cache-Control", 13u) ||
		__xrtHttpCorsName(Name, "Content-Language", 16u) ||
		__xrtHttpCorsName(Name, "Content-Length", 14u) ||
		__xrtHttpCorsName(Name, "Content-Type", 12u) ||
		__xrtHttpCorsName(Name, "Expires", 7u) ||
		__xrtHttpCorsName(Name, "Last-Modified", 13u) ||
		__xrtHttpCorsName(Name, "Pragma", 6u);
}



/* 判断 Fetch 始终隐藏的响应字段。 */
XRT_API bool xrtHttpCorsResponseHeaderForbidden(xstrview Name)
{
	if ( !xrtHttpTokenValid(Name) ) {
		return false;
	}
	return __xrtHttpCorsName(Name, "Set-Cookie", 10u) ||
		__xrtHttpCorsName(Name, "Set-Cookie2", 11u);
}



/* 结合 Expose-Headers 判断响应字段是否可见。 */
XRT_API xhttpnext xrtHttpCorsResponseHeaderExposed(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	bool bCredentials
)
{
	xhttpcorscursor Cursor;
	xstrview Exposed;
	xhttpnext Next;
	bool bFound = false;

	if ( !__xrtHttpFieldArrayValid(pFields, iCount) ||
		!xrtHttpTokenValid(Name) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	if ( xrtHttpCorsResponseHeaderForbidden(Name) ) {
		return XHTTP_NEXT_END;
	}
	if ( xrtHttpCorsResponseHeaderSafelisted(Name) ) {
		return XHTTP_NEXT_ITEM;
	}
	xrtHttpCorsCursorInit(&Cursor);
	while ( (Next = xrtHttpCorsExposeHeaderNext(
		pFields, iCount, &Cursor, &Exposed
	)) == XHTTP_NEXT_ITEM ) {
		if ( (!bCredentials && (Exposed.Size == 1u) &&
			(Exposed.Data[0] == '*')) ||
			xrtHttpFieldNameEqual(Exposed, Name) ) {
			bFound = true;
		}
	}
	if ( Next == XHTTP_NEXT_ERROR ) {
		return Next;
	}
	return bFound ? XHTTP_NEXT_ITEM : XHTTP_NEXT_END;
}

#endif
