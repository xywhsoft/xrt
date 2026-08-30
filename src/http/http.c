#include "../internal/xrt_http.h"



#if defined(XRT_FEATURE_HTTP)

/* 返回 uint64 十进制线路文本长度。 */
size_t __xrtHttpUInt64Size(uint64 iValue)
{
	size_t iSize = 1;

	while ( iValue >= UINT64_C(10) ) {
		iValue /= UINT64_C(10);
		iSize++;
	}
	return iSize;
}



/* 写入无符号十进制线路文本。 */
size_t __xrtHttpUInt64Write(char* sOutput, uint64 iValue)
{
	size_t iSize = __xrtHttpUInt64Size(iValue);
	size_t i = iSize;

	do {
		sOutput[--i] = (char)(
			'0' + (char)(iValue % UINT64_C(10))
		);
		iValue /= UINT64_C(10);
	} while ( iValue != 0 );
	return iSize;
}



/* 返回正式状态码对应的静态原因短语，未知状态保持可扩展。 */
XRT_API xstrview xrtHttpStatusText(uint16 iStatus)
{
	switch ( iStatus ) {
		case XHTTP_STATUS_CONTINUE:
			return XRT_STR_LITERAL("Continue");
		case XHTTP_STATUS_SWITCHING_PROTOCOLS:
			return XRT_STR_LITERAL("Switching Protocols");
		case XHTTP_STATUS_PROCESSING:
			return XRT_STR_LITERAL("Processing");
		case XHTTP_STATUS_EARLY_HINTS:
			return XRT_STR_LITERAL("Early Hints");

		case XHTTP_STATUS_OK:
			return XRT_STR_LITERAL("OK");
		case XHTTP_STATUS_CREATED:
			return XRT_STR_LITERAL("Created");
		case XHTTP_STATUS_ACCEPTED:
			return XRT_STR_LITERAL("Accepted");
		case XHTTP_STATUS_NON_AUTHORITATIVE_INFORMATION:
			return XRT_STR_LITERAL("Non-Authoritative Information");
		case XHTTP_STATUS_NO_CONTENT:
			return XRT_STR_LITERAL("No Content");
		case XHTTP_STATUS_RESET_CONTENT:
			return XRT_STR_LITERAL("Reset Content");
		case XHTTP_STATUS_PARTIAL_CONTENT:
			return XRT_STR_LITERAL("Partial Content");
		case XHTTP_STATUS_MULTI_STATUS:
			return XRT_STR_LITERAL("Multi-Status");
		case XHTTP_STATUS_ALREADY_REPORTED:
			return XRT_STR_LITERAL("Already Reported");
		case XHTTP_STATUS_IM_USED:
			return XRT_STR_LITERAL("IM Used");

		case XHTTP_STATUS_MULTIPLE_CHOICES:
			return XRT_STR_LITERAL("Multiple Choices");
		case XHTTP_STATUS_MOVED_PERMANENTLY:
			return XRT_STR_LITERAL("Moved Permanently");
		case XHTTP_STATUS_FOUND:
			return XRT_STR_LITERAL("Found");
		case XHTTP_STATUS_SEE_OTHER:
			return XRT_STR_LITERAL("See Other");
		case XHTTP_STATUS_NOT_MODIFIED:
			return XRT_STR_LITERAL("Not Modified");
		case XHTTP_STATUS_USE_PROXY:
			return XRT_STR_LITERAL("Use Proxy");
		case XHTTP_STATUS_TEMPORARY_REDIRECT:
			return XRT_STR_LITERAL("Temporary Redirect");
		case XHTTP_STATUS_PERMANENT_REDIRECT:
			return XRT_STR_LITERAL("Permanent Redirect");

		case XHTTP_STATUS_BAD_REQUEST:
			return XRT_STR_LITERAL("Bad Request");
		case XHTTP_STATUS_UNAUTHORIZED:
			return XRT_STR_LITERAL("Unauthorized");
		case XHTTP_STATUS_PAYMENT_REQUIRED:
			return XRT_STR_LITERAL("Payment Required");
		case XHTTP_STATUS_FORBIDDEN:
			return XRT_STR_LITERAL("Forbidden");
		case XHTTP_STATUS_NOT_FOUND:
			return XRT_STR_LITERAL("Not Found");
		case XHTTP_STATUS_METHOD_NOT_ALLOWED:
			return XRT_STR_LITERAL("Method Not Allowed");
		case XHTTP_STATUS_NOT_ACCEPTABLE:
			return XRT_STR_LITERAL("Not Acceptable");
		case XHTTP_STATUS_PROXY_AUTHENTICATION_REQUIRED:
			return XRT_STR_LITERAL("Proxy Authentication Required");
		case XHTTP_STATUS_REQUEST_TIMEOUT:
			return XRT_STR_LITERAL("Request Timeout");
		case XHTTP_STATUS_CONFLICT:
			return XRT_STR_LITERAL("Conflict");
		case XHTTP_STATUS_GONE:
			return XRT_STR_LITERAL("Gone");
		case XHTTP_STATUS_LENGTH_REQUIRED:
			return XRT_STR_LITERAL("Length Required");
		case XHTTP_STATUS_PRECONDITION_FAILED:
			return XRT_STR_LITERAL("Precondition Failed");
		case XHTTP_STATUS_CONTENT_TOO_LARGE:
			return XRT_STR_LITERAL("Content Too Large");
		case XHTTP_STATUS_URI_TOO_LONG:
			return XRT_STR_LITERAL("URI Too Long");
		case XHTTP_STATUS_UNSUPPORTED_MEDIA_TYPE:
			return XRT_STR_LITERAL("Unsupported Media Type");
		case XHTTP_STATUS_RANGE_NOT_SATISFIABLE:
			return XRT_STR_LITERAL("Range Not Satisfiable");
		case XHTTP_STATUS_EXPECTATION_FAILED:
			return XRT_STR_LITERAL("Expectation Failed");
		case XHTTP_STATUS_MISDIRECTED_REQUEST:
			return XRT_STR_LITERAL("Misdirected Request");
		case XHTTP_STATUS_UNPROCESSABLE_CONTENT:
			return XRT_STR_LITERAL("Unprocessable Content");
		case XHTTP_STATUS_LOCKED:
			return XRT_STR_LITERAL("Locked");
		case XHTTP_STATUS_FAILED_DEPENDENCY:
			return XRT_STR_LITERAL("Failed Dependency");
		case XHTTP_STATUS_TOO_EARLY:
			return XRT_STR_LITERAL("Too Early");
		case XHTTP_STATUS_UPGRADE_REQUIRED:
			return XRT_STR_LITERAL("Upgrade Required");
		case XHTTP_STATUS_PRECONDITION_REQUIRED:
			return XRT_STR_LITERAL("Precondition Required");
		case XHTTP_STATUS_TOO_MANY_REQUESTS:
			return XRT_STR_LITERAL("Too Many Requests");
		case XHTTP_STATUS_REQUEST_HEADER_FIELDS_TOO_LARGE:
			return XRT_STR_LITERAL("Request Header Fields Too Large");
		case XHTTP_STATUS_UNAVAILABLE_FOR_LEGAL_REASONS:
			return XRT_STR_LITERAL("Unavailable For Legal Reasons");

		case XHTTP_STATUS_INTERNAL_SERVER_ERROR:
			return XRT_STR_LITERAL("Internal Server Error");
		case XHTTP_STATUS_NOT_IMPLEMENTED:
			return XRT_STR_LITERAL("Not Implemented");
		case XHTTP_STATUS_BAD_GATEWAY:
			return XRT_STR_LITERAL("Bad Gateway");
		case XHTTP_STATUS_SERVICE_UNAVAILABLE:
			return XRT_STR_LITERAL("Service Unavailable");
		case XHTTP_STATUS_GATEWAY_TIMEOUT:
			return XRT_STR_LITERAL("Gateway Timeout");
		case XHTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED:
			return XRT_STR_LITERAL("HTTP Version Not Supported");
		case XHTTP_STATUS_VARIANT_ALSO_NEGOTIATES:
			return XRT_STR_LITERAL("Variant Also Negotiates");
		case XHTTP_STATUS_INSUFFICIENT_STORAGE:
			return XRT_STR_LITERAL("Insufficient Storage");
		case XHTTP_STATUS_LOOP_DETECTED:
			return XRT_STR_LITERAL("Loop Detected");
		case XHTTP_STATUS_NOT_EXTENDED:
			return XRT_STR_LITERAL("Not Extended");
		case XHTTP_STATUS_NETWORK_AUTHENTICATION_REQUIRED:
			return XRT_STR_LITERAL("Network Authentication Required");

		default:
			return (xstrview){ NULL, 0 };
	}
}



/* 验证借用字符串视图是一段不会发生地址回绕的连续内存。 */
bool __xrtHttpViewValid(xstrview Text)
{
	if ( !__xrtRangeValid(Text.Data, Text.Size) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 验证字段描述符数组及其借用视图的基础内存边界。 */
bool __xrtHttpFieldArrayValid(
	const xhttpfield* pFields,
	size_t iCount
)
{
	xhttpfield Field;
	size_t iBytes;
	size_t i;

	if ( ((pFields == NULL) && (iCount != 0)) ||
		(iCount > (SIZE_MAX / sizeof(*pFields))) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	iBytes = iCount * sizeof(*pFields);
	if ( !__xrtRangeValid(pFields, iBytes) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		__xrtHttpFieldLoad(pFields, i, &Field);
		if ( !__xrtHttpViewValid(Field.Name) ||
			!__xrtHttpViewValid(Field.Value) ) {
			return false;
		}
	}
	return true;
}



/* 从已经验证的字段数组复制一个描述符。 */
void __xrtHttpFieldLoad(
	const xhttpfield* pFields,
	size_t iIndex,
	xhttpfield* pField
)
{
	memcpy(
		pField,
		((const uint8*)pFields) + (iIndex * sizeof(*pFields)),
		sizeof(*pField)
	);
}



/* 判断一段内存是否覆盖字段描述符数组或任一借用文本。 */
bool __xrtHttpFieldArrayOverlap(
	const xhttpfield* pFields,
	size_t iCount,
	const void* pMemory,
	size_t iSize
)
{
	xhttpfield Field;
	size_t i;

	if ( __xrtRangesOverlap(
		pFields, iCount * sizeof(*pFields),
		pMemory, iSize
	) ) {
		return true;
	}
	for ( i = 0; i < iCount; i++ ) {
		__xrtHttpFieldLoad(pFields, i, &Field);
		if ( __xrtRangesOverlap(
			Field.Name.Data,
			Field.Name.Size,
			pMemory,
			iSize
		) || __xrtRangesOverlap(
			Field.Value.Data,
			Field.Value.Size,
			pMemory,
			iSize
		) ) {
			return true;
		}
	}
	return false;
}



/* 按字节精确比较两个借用字符串视图。 */
bool __xrtHttpViewEqual(
	xstrview Left,
	xstrview Right
)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 按 ASCII 规则把大写字母转换为小写。 */
unsigned char __xrtHttpAsciiLower(unsigned char iByte)
{
	if ( (iByte >= (unsigned char)'A') &&
		(iByte <= (unsigned char)'Z') ) {
		return (unsigned char)(iByte +
			((unsigned char)'a' - (unsigned char)'A'));
	}
	return iByte;
}



/* 判断一个字节是否属于 RFC tchar 集合。 */
bool __xrtHttpTokenByte(unsigned char iByte)
{
	if ( ((iByte >= (unsigned char)'0') &&
		(iByte <= (unsigned char)'9')) ||
		((iByte >= (unsigned char)'A') &&
		(iByte <= (unsigned char)'Z')) ||
		((iByte >= (unsigned char)'a') &&
		(iByte <= (unsigned char)'z')) ) {
		return true;
	}
	return (iByte == (unsigned char)'!') ||
		(iByte == (unsigned char)'#') ||
		(iByte == (unsigned char)'$') ||
		(iByte == (unsigned char)'%') ||
		(iByte == (unsigned char)'&') ||
		(iByte == (unsigned char)'\'') ||
		(iByte == (unsigned char)'*') ||
		(iByte == (unsigned char)'+') ||
		(iByte == (unsigned char)'-') ||
		(iByte == (unsigned char)'.') ||
		(iByte == (unsigned char)'^') ||
		(iByte == (unsigned char)'_') ||
		(iByte == (unsigned char)'`') ||
		(iByte == (unsigned char)'|') ||
		(iByte == (unsigned char)'~');
}



/* 判断字节是否属于 RFC quoted-string 的 qdtext 集合。 */
bool __xrtHttpQuotedTextByte(unsigned char iByte)
{
	return (iByte == (unsigned char)'\t') ||
		(iByte == (unsigned char)' ') ||
		(iByte == UINT8_C(0x21)) ||
		((iByte >= UINT8_C(0x23)) &&
		 (iByte <= UINT8_C(0x5B))) ||
		((iByte >= UINT8_C(0x5D)) &&
		 (iByte <= UINT8_C(0x7E))) ||
		(iByte >= UINT8_C(0x80));
}



/* 判断字节是否属于 RFC quoted-pair 的可转义集合。 */
bool __xrtHttpQuotedPairByte(unsigned char iByte)
{
	return (iByte == (unsigned char)'\t') ||
		((iByte >= UINT8_C(0x20)) &&
		 (iByte != UINT8_C(0x7F)));
}



/* 判断文本是否是非空 HTTP token。 */
XRT_API bool xrtHttpTokenValid(xstrview Text)
{
	size_t i;

	if ( !__xrtHttpViewValid(Text) || (Text.Size == 0) ) {
		return false;
	}
	for ( i = 0; i < Text.Size; i++ ) {
		if ( !__xrtHttpTokenByte((unsigned char)Text.Data[i]) ) {
			return false;
		}
	}
	return true;
}



/* 验证 Header 查询、删除和空 trailer 查询使用的非空字段名。 */
bool __xrtHttpLookupNameValid(xstrview Name)
{
	if ( !__xrtHttpViewValid(Name) ) {
		return false;
	}
	if ( !xrtHttpTokenValid(Name) ) {
		__xrtErrorSetValue();
		return false;
	}
	return true;
}



/* 按 ASCII 大小写不敏感规则比较两个 token。 */
XRT_API bool xrtHttpTokenEqual(xstrview Left, xstrview Right)
{
	size_t i;

	if ( !__xrtHttpViewValid(Left) ||
		!__xrtHttpViewValid(Right) ||
		(Left.Size != Right.Size) ) {
		return false;
	}
	for ( i = 0; i < Left.Size; i++ ) {
		if ( __xrtHttpAsciiLower((unsigned char)Left.Data[i]) !=
			__xrtHttpAsciiLower((unsigned char)Right.Data[i]) ) {
			return false;
		}
	}
	return true;
}



/* 按长度和字节直接分类常用方法，扩展方法只在未命中时扫描 token。 */
XRT_API xhttpmethod xrtHttpMethodParse(xstrview Method)
{
	cstr sData;
	size_t i;

	if ( !__xrtHttpViewValid(Method) || (Method.Size == 0) ) {
		return XHTTP_METHOD_INVALID;
	}
	sData = Method.Data;
	switch ( Method.Size ) {
	case 3:
		if ( (sData[0] == 'G') &&
			(sData[1] == 'E') &&
			(sData[2] == 'T') ) {
			return XHTTP_METHOD_GET;
		}
		if ( (sData[0] == 'P') &&
			(sData[1] == 'U') &&
			(sData[2] == 'T') ) {
			return XHTTP_METHOD_PUT;
		}
		break;
	case 4:
		if ( (sData[0] == 'P') &&
			(sData[1] == 'O') &&
			(sData[2] == 'S') &&
			(sData[3] == 'T') ) {
			return XHTTP_METHOD_POST;
		}
		if ( (sData[0] == 'H') &&
			(sData[1] == 'E') &&
			(sData[2] == 'A') &&
			(sData[3] == 'D') ) {
			return XHTTP_METHOD_HEAD;
		}
		break;
	case 5:
		if ( (sData[0] == 'P') &&
			(sData[1] == 'A') &&
			(sData[2] == 'T') &&
			(sData[3] == 'C') &&
			(sData[4] == 'H') ) {
			return XHTTP_METHOD_PATCH;
		}
		if ( (sData[0] == 'T') &&
			(sData[1] == 'R') &&
			(sData[2] == 'A') &&
			(sData[3] == 'C') &&
			(sData[4] == 'E') ) {
			return XHTTP_METHOD_TRACE;
		}
		break;
	case 6:
		if ( (sData[0] == 'D') &&
			(sData[1] == 'E') &&
			(sData[2] == 'L') &&
			(sData[3] == 'E') &&
			(sData[4] == 'T') &&
			(sData[5] == 'E') ) {
			return XHTTP_METHOD_DELETE;
		}
		break;
	case 7:
		if ( (sData[0] == 'C') &&
			(sData[1] == 'O') &&
			(sData[2] == 'N') &&
			(sData[3] == 'N') &&
			(sData[4] == 'E') &&
			(sData[5] == 'C') &&
			(sData[6] == 'T') ) {
			return XHTTP_METHOD_CONNECT;
		}
		if ( (sData[0] == 'O') &&
			(sData[1] == 'P') &&
			(sData[2] == 'T') &&
			(sData[3] == 'I') &&
			(sData[4] == 'O') &&
			(sData[5] == 'N') &&
			(sData[6] == 'S') ) {
			return XHTTP_METHOD_OPTIONS;
		}
		break;
	default:
		break;
	}

	for ( i = 0; i < Method.Size; i++ ) {
		if ( !__xrtHttpTokenByte((unsigned char)sData[i]) ) {
			return XHTTP_METHOD_INVALID;
		}
	}
	return XHTTP_METHOD_OTHER;
}



/* 按大小写敏感规则比较两个合法 HTTP 方法名。 */
XRT_API bool xrtHttpMethodEqual(
	xstrview Left,
	xstrview Right
)
{
	return xrtHttpTokenValid(Left) &&
		xrtHttpTokenValid(Right) &&
		__xrtHttpViewEqual(Left, Right);
}



/* 按 RFC 语义识别只读方法，供重试、Cookie 和缓存策略共同使用。 */
XRT_API bool xrtHttpMethodSafe(xstrview Method)
{
	switch ( xrtHttpMethodParse(Method) ) {
	case XHTTP_METHOD_GET:
	case XHTTP_METHOD_HEAD:
	case XHTTP_METHOD_OPTIONS:
	case XHTTP_METHOD_TRACE:
		return true;
	default:
		return false;
	}
}



/* 幂等方法包含全部安全方法，以及具有替换或删除语义的 PUT、DELETE。 */
XRT_API bool xrtHttpMethodIdempotent(xstrview Method)
{
	switch ( xrtHttpMethodParse(Method) ) {
	case XHTTP_METHOD_GET:
	case XHTTP_METHOD_HEAD:
	case XHTTP_METHOD_OPTIONS:
	case XHTTP_METHOD_TRACE:
	case XHTTP_METHOD_PUT:
	case XHTTP_METHOD_DELETE:
		return true;
	default:
		return false;
	}
}



/* 判断响应状态与大小写敏感的方法语义是否允许携带内容。 */
XRT_API bool xrtHttpResponseContentAllowed(
	xstrview Method,
	uint16 iStatus
)
{
	xhttpmethod MethodCode = xrtHttpMethodParse(Method);

	if ( (MethodCode == XHTTP_METHOD_INVALID) ||
		(iStatus < 100) ||
		(iStatus > 999) ) {
		return false;
	}
	if ( (MethodCode == XHTTP_METHOD_HEAD) || (iStatus < 200) ||
		(iStatus == 204) ||
		(iStatus == 205) ||
		(iStatus == 304) ) {
		return false;
	}
	if ( (iStatus >= 200) &&
		(iStatus < 300) &&
		(MethodCode == XHTTP_METHOD_CONNECT) ) {
		return false;
	}
	return true;
}



/* 删除文本两端的 OWS，不改变正文内部的空白。 */
XRT_API xstrview xrtHttpOwsTrim(xstrview Text)
{
	if ( !__xrtHttpViewValid(Text) ) {
		return (xstrview){ NULL, 0 };
	}
	while ( (Text.Size != 0) &&
		((Text.Data[0] == ' ') || (Text.Data[0] == '\t')) ) {
		Text.Data++;
		Text.Size--;
	}
	while ( (Text.Size != 0) &&
		((Text.Data[Text.Size - 1u] == ' ') ||
		 (Text.Data[Text.Size - 1u] == '\t')) ) {
		Text.Size--;
	}
	return Text;
}



/* 读取下一个非空列表成员，并让成员解析器负责验证 quoted-string。 */
xhttpnext __xrtHttpQuotedListNext(
	xstrview Value,
	size_t iOffset,
	size_t* pNext,
	xstrview* pElement
)
{
	xstrview Element;
	size_t iStart;
	size_t i;
	bool bQuoted;

	if ( iOffset > Value.Size ) {
		return XHTTP_NEXT_ERROR;
	}
	i = iOffset;
	for ( ;; ) {
		while ( (i < Value.Size) &&
			((Value.Data[i] == ' ') ||
			 (Value.Data[i] == '\t') ||
			 (Value.Data[i] == ',')) ) {
			i++;
		}
		if ( i == Value.Size ) {
			*pNext = i;
			*pElement = (xstrview){ NULL, 0 };
			return XHTTP_NEXT_END;
		}
		iStart = i;
		bQuoted = false;
		while ( i < Value.Size ) {
			if ( bQuoted && (Value.Data[i] == '\\') ) {
				i += ((i + 1u) < Value.Size) ? 2u : 1u;
				continue;
			}
			if ( Value.Data[i] == '"' ) {
				bQuoted = !bQuoted;
				i++;
				continue;
			}
			if ( !bQuoted && (Value.Data[i] == ',') ) {
				break;
			}
			i++;
		}
		Element = xrtHttpOwsTrim((xstrview){
			Value.Data + iStart, i - iStart
		});
		*pNext = (i < Value.Size) ? (i + 1u) : i;
		if ( Element.Size != 0 ) {
			*pElement = Element;
			return XHTTP_NEXT_ITEM;
		}
	}
}



/* 严格读取逗号分隔 token-list 的下一项。 */
XRT_API xhttpnext xrtHttpTokenNext(
	xstrview List,
	size_t* pOffset,
	xstrview* pToken
)
{
	size_t iStart;
	size_t i;

	if ( pToken != NULL ) {
		memset(pToken, 0, sizeof(*pToken));
	}
	if ( !__xrtHttpViewValid(List) ||
		(pOffset == NULL) || (pToken == NULL) ||
		(*pOffset > List.Size) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	i = *pOffset;
	while ( true ) {
		while ( (i < List.Size) &&
			((List.Data[i] == ' ') ||
			 (List.Data[i] == '\t')) ) {
			i++;
		}
		if ( i == List.Size ) {
			*pOffset = i;
			return XHTTP_NEXT_END;
		}
		if ( List.Data[i] != ',' ) {
			break;
		}
		i++;
	}
	iStart = i;
	while ( (i < List.Size) &&
		__xrtHttpTokenByte((unsigned char)List.Data[i]) ) {
		i++;
	}
	if ( i == iStart ) {
		__xrtErrorSetValue();
		return XHTTP_NEXT_ERROR;
	}
	pToken->Data = List.Data + iStart;
	pToken->Size = i - iStart;
	while ( (i < List.Size) &&
		((List.Data[i] == ' ') || (List.Data[i] == '\t')) ) {
		i++;
	}
	if ( i < List.Size ) {
		if ( List.Data[i] != ',' ) {
			memset(pToken, 0, sizeof(*pToken));
			__xrtErrorSetValue();
			return XHTTP_NEXT_ERROR;
		}
		i++;
	}
	*pOffset = i;
	return XHTTP_NEXT_ITEM;
}



/* 判断严格 token-list 是否包含指定 token。 */
XRT_API bool xrtHttpTokenListHas(xstrview List, xstrview Token)
{
	xhttpnext Next;
	xstrview Item;
	size_t iOffset = 0;
	bool bFound = false;

	if ( !xrtHttpTokenValid(Token) ) {
		return false;
	}
	for ( ;; ) {
		Next = xrtHttpTokenNext(List, &iOffset, &Item);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			return bFound;
		}
		if ( xrtHttpTokenEqual(Item, Token) ) {
			bFound = true;
		}
	}
}



/* 严格统计 token-list 条目。 */
XRT_API bool xrtHttpTokenListCount(xstrview List, size_t* pCount)
{
	xhttpnext Next;
	xstrview Item;
	size_t iOffset = 0;
	size_t iCount = 0;

	if ( pCount == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	*pCount = 0;
	for ( ;; ) {
		Next = xrtHttpTokenNext(List, &iOffset, &Item);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			*pCount = iCount;
			return true;
		}
		if ( iCount == SIZE_MAX ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iCount++;
	}
}



/* 验证 token 描述符数组并计算规范线路长度。 */
static bool __xrtHttpTokenListMeasure(
	const xstrview* pTokens,
	size_t iCount,
	size_t* pRequired
)
{
	xstrview Token;
	size_t iRequired = 0;
	size_t i;

	if ( (iCount > (SIZE_MAX / sizeof(*pTokens))) ||
		!__xrtRangeValid(
			pTokens, iCount * sizeof(*pTokens)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		memcpy(&Token, pTokens + i, sizeof(Token));
		if ( !__xrtHttpViewValid(Token) ) {
			return false;
		}
		if ( !xrtHttpTokenValid(Token) ) {
			__xrtErrorSetValue();
			return false;
		}
		if ( ((i != 0) &&
			 !__xrtHttpSizeAdd(&iRequired, 2u)) ||
			!__xrtHttpSizeAdd(&iRequired, Token.Size) ) {
			return false;
		}
	}
	*pRequired = iRequired;
	return true;
}



/* 判断一段内存是否覆盖 token 描述符或其借用文本。 */
static bool __xrtHttpTokenListOverlap(
	const xstrview* pTokens,
	size_t iCount,
	const void* pMemory,
	size_t iSize
)
{
	xstrview Token;
	size_t i;

	if ( __xrtRangesOverlap(
		pTokens, iCount * sizeof(*pTokens),
		pMemory, iSize
	) ) {
		return true;
	}
	for ( i = 0; i < iCount; i++ ) {
		memcpy(&Token, pTokens + i, sizeof(Token));
		if ( __xrtRangesOverlap(
			Token.Data, Token.Size, pMemory, iSize
		) ) {
			return true;
		}
	}
	return false;
}



/* 规范写出逗号空格分隔的 token-list。 */
XRT_API bool xrtHttpTokenListWrite(
	const xstrview* pTokens,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xstrview Token;
	bytes pBytes = (bytes)pOutput;
	size_t iRequired;
	size_t iOffset = 0;
	size_t i;

	if ( !__xrtRangeValid(pSize, sizeof(iRequired)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		((pOutput != NULL) &&
		 !__xrtRangeValid(pOutput, iCapacity)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpTokenListMeasure(
		pTokens, iCount, &iRequired
	) ) {
		return false;
	}
	if ( __xrtHttpTokenListOverlap(
		pTokens, iCount, pSize, sizeof(iRequired)
	) || ((pOutput != NULL) &&
		(__xrtHttpTokenListOverlap(
			pTokens, iCount, pOutput, iRequired
		 ) || __xrtRangesOverlap(
			pOutput, iRequired,
			pSize, sizeof(iRequired)
		 ))) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pSize, &iRequired, sizeof(iRequired));
	if ( pOutput == NULL ) {
		return true;
	}
	if ( iCapacity < iRequired ) {
		__xrtErrorSetRange();
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		memcpy(&Token, pTokens + i, sizeof(Token));
		if ( i != 0 ) {
			memcpy(pBytes + iOffset, ", ", 2u);
			iOffset += 2u;
		}
		memcpy(pBytes + iOffset, Token.Data, Token.Size);
		iOffset += Token.Size;
	}
	return true;
}



/* 构建零结尾 token-list。 */
XRT_API str xrtHttpTokenListBuild(
	const xstrview* pTokens,
	size_t iCount,
	size_t* pSize
)
{
	size_t iRequired;
	str sOutput;

	if ( (pSize != NULL) &&
		(!__xrtRangeValid(pSize, sizeof(*pSize)) ||
		 ((iCount <= (SIZE_MAX / sizeof(*pTokens))) &&
		  __xrtRangeValid(
			pTokens, iCount * sizeof(*pTokens)
		  ) && __xrtHttpTokenListOverlap(
			pTokens, iCount, pSize, sizeof(*pSize)
		  ))) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtHttpTokenListWrite(
		pTokens, iCount, NULL, 0, &iRequired
	) ) {
		return NULL;
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtHttpTokenListWrite(
		pTokens, iCount,
		sOutput, iRequired, &iRequired
	) ) {
		xrtFree(sOutput);
		return NULL;
	}
	sOutput[iRequired] = 0;
	if ( pSize != NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
	}
	return sOutput;
}



/* 验证同名字段 token 游标仍绑定原字段数组和字段名称。 */
static bool __xrtHttpFieldTokenCursorValid(
	const xhttpfieldtokencursor* pCursor,
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	bool bRequired
)
{
	if ( pCursor->Validated > 1u ) {
		return false;
	}
	if ( pCursor->Validated == 0 ) {
		return (pCursor->Source == NULL) &&
			(pCursor->Name.Data == NULL) &&
			(pCursor->Name.Size == 0) &&
			(pCursor->Count == 0) &&
			(pCursor->Field == 0) &&
			(pCursor->Offset == 0) &&
			(pCursor->Required == 0);
	}
	return (pCursor->Source == pFields) &&
		(pCursor->Count == iCount) &&
		__xrtHttpViewValid(pCursor->Name) &&
		xrtHttpTokenValid(pCursor->Name) &&
		xrtHttpFieldNameEqual(pCursor->Name, Name) &&
		(pCursor->Required == (bRequired ? 1u : 0u)) &&
		(pCursor->Field <= iCount) &&
		!((pCursor->Field == iCount) &&
		  (pCursor->Offset != 0));
}



/* 单遍验证重复字段值，并可同时统计或查找 token。 */
static bool __xrtHttpFieldTokenScan(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	const xstrview* pExpected,
	size_t* pFieldCount,
	size_t* pTokenCount,
	bool* pFound
)
{
	xhttpfield Field;
	xstrview Token;
	xhttpnext Next;
	size_t iTotal = 0;
	size_t iFields = 0;
	size_t i;
	bool bFound = false;

	for ( i = 0; i < iCount; i++ ) {
		size_t iOffset = 0;

		__xrtHttpFieldLoad(pFields, i, &Field);
		if ( !xrtHttpFieldNameEqual(Field.Name, Name) ) {
			continue;
		}
		iFields++;
		for ( ;; ) {
			Next = xrtHttpTokenNext(
				Field.Value, &iOffset, &Token
			);
			if ( Next == XHTTP_NEXT_ERROR ) {
				return false;
			}
			if ( Next == XHTTP_NEXT_END ) {
				break;
			}
			if ( iTotal == SIZE_MAX ) {
				__xrtErrorSetSizeOverflow();
				return false;
			}
			iTotal++;
			if ( (pExpected != NULL) &&
				xrtHttpTokenEqual(Token, *pExpected) ) {
				bFound = true;
			}
		}
	}
	if ( pFieldCount != NULL ) {
		*pFieldCount = iFields;
	}
	*pTokenCount = iTotal;
	if ( pFound != NULL ) {
		*pFound = bFound;
	}
	return true;
}



/* 初始化重复同名字段 token-list 游标。 */
XRT_API void xrtHttpFieldTokenCursorInit(
	xhttpfieldtokencursor* pCursor
)
{
	xhttpfieldtokencursor Cursor;

	if ( !__xrtRangeValid(pCursor, sizeof(Cursor)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(&Cursor, 0, sizeof(Cursor));
	memcpy(pCursor, &Cursor, sizeof(Cursor));
}



/* 跨重复同名字段读取可选或必需的 token-list 条目。 */
static xhttpnext __xrtHttpFieldTokenNextMode(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	bool bRequired,
	xhttpfieldtokencursor* pCursor,
	xstrview* pToken
)
{
	xhttpfieldtokencursor Cursor;
	xhttpfield Field;
	xstrview Token;
	xhttpnext Next;
	size_t iFields;
	size_t iIgnored;
	size_t iOffset;

	if ( !__xrtHttpFieldArrayValid(pFields, iCount) ||
		!xrtHttpTokenValid(Name) ||
		!__xrtRangeValid(pCursor, sizeof(Cursor)) ||
		!__xrtRangeValid(pToken, sizeof(Token)) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount, pCursor, sizeof(Cursor)
		) || __xrtHttpFieldArrayOverlap(
			pFields, iCount, pToken, sizeof(Token)
		) || __xrtRangesOverlap(
			Name.Data, Name.Size, pCursor, sizeof(Cursor)
		) || __xrtRangesOverlap(
			Name.Data, Name.Size, pToken, sizeof(Token)
		) || __xrtRangesOverlap(
			pCursor, sizeof(Cursor), pToken, sizeof(Token)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&Cursor, pCursor, sizeof(Cursor));
	memset(&Token, 0, sizeof(Token));
	memcpy(pToken, &Token, sizeof(Token));
	if ( !__xrtHttpFieldTokenCursorValid(
		&Cursor, pFields, iCount, Name, bRequired
	) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	if ( Cursor.Validated == 0 ) {
		if ( !__xrtHttpFieldTokenScan(
			pFields, iCount, Name,
			NULL, &iFields, &iIgnored, NULL
		) ) {
			return XHTTP_NEXT_ERROR;
		}
		if ( bRequired && (iFields != 0) &&
			(iIgnored == 0) ) {
			__xrtErrorSetValue();
			return XHTTP_NEXT_ERROR;
		}
		Cursor.Source = pFields;
		Cursor.Name = Name;
		Cursor.Count = iCount;
		Cursor.Required = bRequired ? 1u : 0u;
		Cursor.Validated = 1u;
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
		iOffset = Cursor.Offset;
		Next = xrtHttpTokenNext(
			Field.Value, &iOffset, &Token
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return Next;
		}
		if ( Next == XHTTP_NEXT_END ) {
			Cursor.Field++;
			Cursor.Offset = 0;
			continue;
		}
		Cursor.Offset = iOffset;
		memcpy(pToken, &Token, sizeof(Token));
		memcpy(pCursor, &Cursor, sizeof(Cursor));
		return XHTTP_NEXT_ITEM;
	}
	memcpy(pCursor, &Cursor, sizeof(Cursor));
	return XHTTP_NEXT_END;
}



/* 跨重复同名字段读取允许空列表的 token 条目。 */
XRT_API xhttpnext xrtHttpFieldTokenNext(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	xhttpfieldtokencursor* pCursor,
	xstrview* pToken
)
{
	return __xrtHttpFieldTokenNextMode(
		pFields, iCount, Name, false, pCursor, pToken
	);
}



/* 跨重复同名字段读取字段存在时至少包含一项的 token 列表。 */
xhttpnext __xrtHttpFieldTokenNextRequired(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	xhttpfieldtokencursor* pCursor,
	xstrview* pToken
)
{
	return __xrtHttpFieldTokenNextMode(
		pFields, iCount, Name, true, pCursor, pToken
	);
}



/* 完整验证并统计重复同名字段中的 token。 */
XRT_API bool xrtHttpFieldTokenCount(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	size_t* pTokenCount
)
{
	size_t iTotal = 0;

	if ( !__xrtHttpFieldArrayValid(pFields, iCount) ||
		!xrtHttpTokenValid(Name) ||
		!__xrtRangeValid(pTokenCount, sizeof(iTotal)) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount, pTokenCount, sizeof(iTotal)
		) || __xrtRangesOverlap(
			Name.Data, Name.Size,
			pTokenCount, sizeof(iTotal)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pTokenCount, &iTotal, sizeof(iTotal));
	if ( !__xrtHttpFieldTokenScan(
		pFields, iCount, Name,
		NULL, NULL, &iTotal, NULL
	) ) {
		return false;
	}
	memcpy(pTokenCount, &iTotal, sizeof(iTotal));
	return true;
}



/* 完整验证并单遍查找重复同名字段中的 token。 */
XRT_API xhttpnext xrtHttpFieldTokenFind(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	xstrview Token
)
{
	size_t iIgnored;
	bool bFound;

	if ( !__xrtHttpFieldArrayValid(pFields, iCount) ||
		!xrtHttpTokenValid(Name) ||
		!xrtHttpTokenValid(Token) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	if ( !__xrtHttpFieldTokenScan(
		pFields, iCount, Name,
		&Token, NULL, &iIgnored, &bFound
	) ) {
		return XHTTP_NEXT_ERROR;
	}
	return bFound ? XHTTP_NEXT_ITEM : XHTTP_NEXT_END;
}



/* 严格把 HTTP qvalue 转换为千分制定点值。 */
XRT_API bool xrtHttpQualityParse(
	xstrview Text,
	uint16* pQuality
)
{
	uint16 iQuality = 0;
	uint16 iScale = 100;
	size_t i;

	if ( !__xrtHttpViewValid(Text) ||
		!__xrtRangeValid(pQuality, sizeof(iQuality)) ||
		__xrtRangesOverlap(
			Text.Data, Text.Size,
			pQuality, sizeof(iQuality)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pQuality, &iQuality, sizeof(iQuality));
	Text = xrtHttpOwsTrim(Text);
	if ( (Text.Size == 0) ||
		((Text.Data[0] != '0') &&
		 (Text.Data[0] != '1')) ) {
		__xrtErrorSetValue();
		return false;
	}
	iQuality = Text.Data[0] == '1' ? 1000u : 0u;
	if ( Text.Size == 1u ) {
		memcpy(pQuality, &iQuality, sizeof(iQuality));
		return true;
	}
	if ( (Text.Data[1] != '.') || (Text.Size > 5u) ) {
		__xrtErrorSetValue();
		return false;
	}
	for ( i = 2u; i < Text.Size; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( (iByte < (unsigned char)'0') ||
			(iByte > (unsigned char)'9') ||
			((iQuality == 1000u) &&
			 (iByte != (unsigned char)'0')) ) {
			__xrtErrorSetValue();
			return false;
		}
		if ( iQuality != 1000u ) {
			iQuality = (uint16)(
				iQuality +
				(uint16)(iByte - (unsigned char)'0') *
					iScale
			);
		}
		iScale = (uint16)(iScale / 10u);
	}
	memcpy(pQuality, &iQuality, sizeof(iQuality));
	return true;
}



/* 迭代带可选 qvalue 的逗号分隔 token。 */
XRT_API xhttpnext xrtHttpWeightedTokenNext(
	xstrview List,
	size_t* pOffset,
	xhttpweightedtoken* pItem
)
{
	xhttpweightedtoken Item;
	size_t iOffset;
	size_t i;
	size_t iStart;
	size_t iQuality;

	memset(&Item, 0, sizeof(Item));
	if ( !__xrtHttpViewValid(List) ||
		!__xrtRangeValid(pOffset, sizeof(iOffset)) ||
		!__xrtRangeValid(pItem, sizeof(Item)) ||
		__xrtRangesOverlap(
			List.Data, List.Size,
			pOffset, sizeof(iOffset)
		) || __xrtRangesOverlap(
			List.Data, List.Size,
			pItem, sizeof(Item)
		) || __xrtRangesOverlap(
			pOffset, sizeof(iOffset),
			pItem, sizeof(Item)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&iOffset, pOffset, sizeof(iOffset));
	if ( iOffset > List.Size ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pItem, &Item, sizeof(Item));
	i = iOffset;
	while ( true ) {
		while ( (i < List.Size) &&
			((List.Data[i] == ' ') ||
			 (List.Data[i] == '\t')) ) {
			i++;
		}
		if ( i == List.Size ) {
			memcpy(pItem, &Item, sizeof(Item));
			memcpy(pOffset, &i, sizeof(i));
			return XHTTP_NEXT_END;
		}
		if ( List.Data[i] != ',' ) {
			break;
		}
		i++;
	}
	iStart = i;
	while ( (i < List.Size) &&
		__xrtHttpTokenByte(
			(unsigned char)List.Data[i]
		) ) {
		i++;
	}
	if ( i == iStart ) {
		__xrtErrorSetValue();
		return XHTTP_NEXT_ERROR;
	}
	Item.Token.Data = List.Data + iStart;
	Item.Token.Size = i - iStart;
	Item.Quality = 1000u;
	while ( (i < List.Size) &&
		((List.Data[i] == ' ') ||
		 (List.Data[i] == '\t')) ) {
		i++;
	}
	if ( (i < List.Size) && (List.Data[i] == ';') ) {
		i++;
		while ( (i < List.Size) &&
			((List.Data[i] == ' ') ||
			 (List.Data[i] == '\t')) ) {
			i++;
		}
		if ( ((List.Size - i) < 2u) ||
			((List.Data[i] != 'q') &&
			 (List.Data[i] != 'Q')) ||
			(List.Data[i + 1u] != '=') ) {
			__xrtErrorSetValue();
			return XHTTP_NEXT_ERROR;
		}
		i += 2u;
		iQuality = i;
		while ( (i < List.Size) &&
			(List.Data[i] != ' ') &&
			(List.Data[i] != '\t') &&
			(List.Data[i] != ',') ) {
			i++;
		}
		if ( !xrtHttpQualityParse(
			(xstrview){
				List.Data + iQuality,
				i - iQuality
			},
			&Item.Quality
		) ) {
			return XHTTP_NEXT_ERROR;
		}
		while ( (i < List.Size) &&
			((List.Data[i] == ' ') ||
			 (List.Data[i] == '\t')) ) {
			i++;
		}
	}
	if ( i < List.Size ) {
		if ( List.Data[i] != ',' ) {
			__xrtErrorSetValue();
			return XHTTP_NEXT_ERROR;
		}
		i++;
	}
	memcpy(pItem, &Item, sizeof(Item));
	memcpy(pOffset, &i, sizeof(i));
	return XHTTP_NEXT_ITEM;
}



/* 解析一个或多个逗号分隔且必须完全相同的 Content-Length。 */
xrt_http_content_length __xrtHttpContentLengthParse(
	xstrview Value,
	uint64* pLength
)
{
	uint64 iLength = 0;
	bool bHasLength = false;
	size_t i = 0;

	if ( !__xrtHttpViewValid(Value) || (pLength == NULL) ) {
		return XRT_HTTP_CONTENT_LENGTH_INVALID;
	}
	*pLength = 0;
	while ( i < Value.Size ) {
		uint64 iValue = 0;
		bool bDigit = false;

		while ( (i < Value.Size) &&
			((Value.Data[i] == ' ') || (Value.Data[i] == '\t')) ) {
			i++;
		}
		while ( (i < Value.Size) &&
			(Value.Data[i] >= '0') && (Value.Data[i] <= '9') ) {
			uint32 iDigit = (uint32)(Value.Data[i] - '0');

			if ( iValue > ((UINT64_MAX - iDigit) / UINT64_C(10)) ) {
				return XRT_HTTP_CONTENT_LENGTH_INVALID;
			}
			iValue = (iValue * UINT64_C(10)) + iDigit;
			bDigit = true;
			i++;
		}
		if ( !bDigit ) {
			return XRT_HTTP_CONTENT_LENGTH_INVALID;
		}
		while ( (i < Value.Size) &&
			((Value.Data[i] == ' ') || (Value.Data[i] == '\t')) ) {
			i++;
		}
		if ( bHasLength && (iLength != iValue) ) {
			return XRT_HTTP_CONTENT_LENGTH_CONFLICT;
		}
		iLength = iValue;
		bHasLength = true;
		if ( i == Value.Size ) {
			*pLength = iLength;
			return XRT_HTTP_CONTENT_LENGTH_VALID;
		}
		if ( Value.Data[i] != ',' ) {
			return XRT_HTTP_CONTENT_LENGTH_INVALID;
		}
		i++;
	}
	return XRT_HTTP_CONTENT_LENGTH_INVALID;
}



/* 对外解析 Content-Length，并把失败收敛为字段值错误。 */
XRT_API bool xrtHttpContentLengthParse(
	xstrview Value,
	uint64* pLength
)
{
	if ( pLength == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( __xrtHttpContentLengthParse(
		Value, pLength
	) != XRT_HTTP_CONTENT_LENGTH_VALID ) {
		*pLength = 0;
		__xrtErrorSetValue();
		return false;
	}
	return true;
}



/* 判断文本是否能安全作为 HTTP 字段值或 reason-phrase。 */
XRT_API bool xrtHttpFieldValueValid(xstrview Value)
{
	size_t i;

	if ( !__xrtHttpViewValid(Value) ) {
		return false;
	}
	for ( i = 0; i < Value.Size; i++ ) {
		unsigned char iByte = (unsigned char)Value.Data[i];

		if ( (iByte == (unsigned char)'\t') ||
			((iByte >= UINT8_C(0x20)) &&
			 (iByte != UINT8_C(0x7F))) ) {
			continue;
		}
		return false;
	}
	return true;
}



/* 解析一行字段，并为上层保留名称和值的精确失败分类。 */
xrt_http_field_error __xrtHttpFieldParse(
	xstrview Line,
	xhttpfield* pField
)
{
	cstr sColon;
	xhttpfield Field;

	if ( (pField == NULL) ||
		!__xrtHttpViewValid(Line) ||
		(Line.Size == 0) ||
		(Line.Data[0] == ' ') ||
		(Line.Data[0] == '\t') ) {
		return XRT_HTTP_FIELD_NAME;
	}
	sColon = (cstr)memchr(Line.Data, ':', Line.Size);
	if ( sColon == NULL ) {
		return XRT_HTTP_FIELD_NAME;
	}
	Field.Name.Data = Line.Data;
	Field.Name.Size = (size_t)(sColon - Line.Data);
	Field.Value.Data = sColon + 1;
	Field.Value.Size = Line.Size -
		(size_t)(Field.Value.Data - Line.Data);
	Field.Value = xrtHttpOwsTrim(Field.Value);
	if ( !xrtHttpTokenValid(Field.Name) ) {
		return XRT_HTTP_FIELD_NAME;
	}
	if ( !xrtHttpFieldValueValid(Field.Value) ) {
		return XRT_HTTP_FIELD_VALUE;
	}
	*pField = Field;
	return XRT_HTTP_FIELD_VALID;
}



/* 严格解析一行不含 CRLF 的 HTTP 字段。 */
XRT_API bool xrtHttpFieldParse(xstrview Line, xhttpfield* pField)
{
	xhttpfield Field = { 0 };

	if ( !__xrtRangeValid(pField, sizeof(Field)) ||
		!__xrtHttpViewValid(Line) ||
		__xrtRangesOverlap(
			pField, sizeof(Field), Line.Data, Line.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pField, &Field, sizeof(Field));
	if ( __xrtHttpFieldParse(Line, &Field) !=
		XRT_HTTP_FIELD_VALID ) {
		return false;
	}
	memcpy(pField, &Field, sizeof(Field));
	return true;
}



/* 严格读取字段块中的下一行，并拒绝裸 CR、裸 LF 和空字段行。 */
XRT_API xhttpnext xrtHttpFieldNext(
	xstrview Block,
	size_t* pOffset,
	xhttpfield* pField
)
{
	xhttpfield Field = { 0 };
	xstrview Line;
	size_t iOffset;
	size_t iEnd;
	size_t iNext;

	if ( !__xrtHttpViewValid(Block) ||
		!__xrtRangeValid(pOffset, sizeof(iOffset)) ||
		!__xrtRangeValid(pField, sizeof(Field)) ||
		__xrtRangesOverlap(
			pOffset, sizeof(iOffset), Block.Data, Block.Size
		) || __xrtRangesOverlap(
			pField, sizeof(Field), Block.Data, Block.Size
		) || __xrtRangesOverlap(
			pOffset, sizeof(iOffset), pField, sizeof(Field)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&iOffset, pOffset, sizeof(iOffset));
	if ( iOffset > Block.Size ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pField, &Field, sizeof(Field));
	if ( iOffset == Block.Size ) {
		return XHTTP_NEXT_END;
	}

	iEnd = iOffset;
	while ( iEnd < Block.Size ) {
		if ( Block.Data[iEnd] == '\n' ) {
			__xrtErrorSetValue();
			return XHTTP_NEXT_ERROR;
		}
		if ( Block.Data[iEnd] == '\r' ) {
			if ( (iEnd + 1u >= Block.Size) ||
				(Block.Data[iEnd + 1u] != '\n') ) {
				__xrtErrorSetValue();
				return XHTTP_NEXT_ERROR;
			}
			break;
		}
		iEnd++;
	}
	Line.Data = Block.Data + iOffset;
	Line.Size = iEnd - iOffset;
	if ( __xrtHttpFieldParse(Line, &Field) !=
		XRT_HTTP_FIELD_VALID ) {
		__xrtErrorSetValue();
		return XHTTP_NEXT_ERROR;
	}
	iNext = (iEnd == Block.Size) ? iEnd : (iEnd + 2u);
	memcpy(pField, &Field, sizeof(Field));
	memcpy(pOffset, &iNext, sizeof(iNext));
	return XHTTP_NEXT_ITEM;
}



/* 严格统计字段块中的全部字段。 */
XRT_API bool xrtHttpFieldBlockCount(
	xstrview Block,
	size_t* pCount
)
{
	xhttpfield Field;
	xhttpnext Next;
	size_t iOffset = 0;
	size_t iCount = 0;

	if ( !__xrtRangeValid(pCount, sizeof(iCount)) ||
		!__xrtHttpViewValid(Block) ||
		__xrtRangesOverlap(
			pCount, sizeof(iCount), Block.Data, Block.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pCount, &iCount, sizeof(iCount));
	for ( ;; ) {
		Next = xrtHttpFieldNext(Block, &iOffset, &Field);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			memcpy(pCount, &iCount, sizeof(iCount));
			return true;
		}
		if ( iCount == SIZE_MAX ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iCount++;
	}
}



/* 安全累加 HTTP 编解码所需的字节数。 */
bool __xrtHttpSizeAdd(
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



/* 完整校验字段数组并测量线路长度。 */
bool __xrtHttpFieldWriteMeasure(
	const xhttpfield* pFields,
	size_t iCount,
	bool bFinalLine,
	size_t* pRequired
)
{
	xhttpfield Field;
	size_t iRequired = bFinalLine ? 2u : 0u;
	size_t i;

	if ( !__xrtHttpFieldArrayValid(pFields, iCount) ) {
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		__xrtHttpFieldLoad(pFields, i, &Field);
		if ( !xrtHttpTokenValid(Field.Name) ||
			!xrtHttpFieldValueValid(Field.Value) ) {
			__xrtErrorSetValue();
			return false;
		}
		if ( !__xrtHttpSizeAdd(
			&iRequired, Field.Name.Size
		) || !__xrtHttpSizeAdd(
			&iRequired, 2u
		) || !__xrtHttpSizeAdd(
			&iRequired, Field.Value.Size
		) || !__xrtHttpSizeAdd(
			&iRequired, 2u
		) ) {
			return false;
		}
	}
	*pRequired = iRequired;
	return true;
}



/* 向已经确认容量的缓冲区写出字段数组。 */
size_t __xrtHttpFieldWriteUnchecked(
	const xhttpfield* pFields,
	size_t iCount,
	bool bFinalLine,
	bytes pOutput
)
{
	xhttpfield Field;
	size_t iOffset = 0;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		__xrtHttpFieldLoad(pFields, i, &Field);
		memcpy(
			pOutput + iOffset,
			Field.Name.Data,
			Field.Name.Size
		);
		iOffset += Field.Name.Size;
		memcpy(pOutput + iOffset, ": ", 2u);
		iOffset += 2u;
		if ( Field.Value.Size != 0 ) {
			memcpy(
				pOutput + iOffset,
				Field.Value.Data,
				Field.Value.Size
			);
		}
		iOffset += Field.Value.Size;
		memcpy(pOutput + iOffset, "\r\n", 2u);
		iOffset += 2u;
	}
	if ( bFinalLine ) {
		memcpy(pOutput + iOffset, "\r\n", 2u);
		iOffset += 2u;
	}
	return iOffset;
}



/* 统一执行字段写出的查询、别名、容量和失败原子契约。 */
static bool __xrtHttpFieldWriteList(
	const xhttpfield* pFields,
	size_t iCount,
	bool bFinalLine,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	size_t iCheckSize;
	size_t iRequired;
	size_t iWritten;

	if ( !__xrtRangeValid(pSize, sizeof(iRequired)) ||
		((pOutput == NULL) && (iCapacity != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpFieldWriteMeasure(
		pFields, iCount, bFinalLine, &iRequired
	) ) {
		return false;
	}
	if ( __xrtHttpFieldArrayOverlap(
		pFields, iCount, pSize, sizeof(*pSize)
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pOutput == NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		return true;
	}
	iCheckSize = __xrtOutputCheckSize(iRequired, iCapacity);
	if ( !__xrtRangeValid(pOutput, iCheckSize) ||
		__xrtRangesOverlap(
		pOutput, iCheckSize, pSize, sizeof(*pSize)
	) || __xrtHttpFieldArrayOverlap(
		pFields, iCount, pOutput, iCheckSize
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iCapacity < iRequired ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		__xrtErrorSetRange();
		return false;
	}
	iWritten = __xrtHttpFieldWriteUnchecked(
		pFields, iCount, bFinalLine, (bytes)pOutput
	);
	memcpy(pSize, &iWritten, sizeof(iWritten));
	return true;
}



/* 写出单个字段行及 CRLF。 */
XRT_API bool xrtHttpFieldWrite(
	const xhttpfield* pField,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return __xrtHttpFieldWriteList(
		pField, 1u, false,
		pOutput, iCapacity, pSize
	);
}



/* 写出字段数组及最终空行。 */
XRT_API bool xrtHttpFieldBlockWrite(
	const xhttpfield* pFields,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return __xrtHttpFieldWriteList(
		pFields, iCount, true,
		pOutput, iCapacity, pSize
	);
}



/* 按 ASCII 大小写不敏感规则比较字段名称。 */
XRT_API bool xrtHttpFieldNameEqual(xstrview Left, xstrview Right)
{
	return xrtHttpTokenEqual(Left, Right);
}



/* 从指定位置查找字段，未找到返回 XRT_NPOS。 */
XRT_API size_t xrtHttpFieldFind(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	size_t iStart
)
{
	xhttpfield Field;
	size_t i;

	if ( !__xrtHttpFieldArrayValid(pFields, iCount) ||
		!__xrtHttpViewValid(Name) ) {
		return XRT_NPOS;
	}
	for ( i = iStart; i < iCount; i++ ) {
		__xrtHttpFieldLoad(pFields, i, &Field);
		if ( xrtHttpFieldNameEqual(Field.Name, Name) ) {
			return i;
		}
	}
	return XRT_NPOS;
}



/* 返回第一个同名字段，未找到返回空指针。 */
XRT_API const xhttpfield* xrtHttpFieldGet(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name
)
{
	size_t iPosition = xrtHttpFieldFind(
		pFields, iCount, Name, 0
	);

	return (iPosition == XRT_NPOS) ? NULL : &pFields[iPosition];
}



/* 返回唯一同名字段，并把重复字段作为值错误报告。 */
XRT_API xhttpnext xrtHttpFieldGetUnique(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	const xhttpfield** ppField
)
{
	const xhttpfield* pFound = NULL;
	xhttpfield Field;
	size_t i;

	if ( !__xrtRangeValid(ppField, sizeof(pFound)) ||
		!__xrtHttpFieldArrayValid(pFields, iCount) ||
		!__xrtHttpViewValid(Name) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount, ppField, sizeof(pFound)
		) || __xrtRangesOverlap(
			Name.Data, Name.Size, ppField, sizeof(pFound)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(ppField, &pFound, sizeof(pFound));
	for ( i = 0; i < iCount; i++ ) {
		__xrtHttpFieldLoad(pFields, i, &Field);
		if ( !xrtHttpFieldNameEqual(
			Field.Name,
			Name
		) ) {
			continue;
		}
		if ( pFound != NULL ) {
			__xrtErrorSetValue();
			return XHTTP_NEXT_ERROR;
		}
		pFound = &pFields[i];
	}
	if ( pFound == NULL ) {
		return XHTTP_NEXT_END;
	}
	memcpy(ppField, &pFound, sizeof(pFound));
	return XHTTP_NEXT_ITEM;
}



/* 统计同名字段数量。 */
XRT_API size_t xrtHttpFieldCount(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name
)
{
	xhttpfield Field;
	size_t iFound = 0;
	size_t i;

	if ( !__xrtHttpFieldArrayValid(pFields, iCount) ||
		!__xrtHttpViewValid(Name) ) {
		return 0;
	}
	for ( i = 0; i < iCount; i++ ) {
		__xrtHttpFieldLoad(pFields, i, &Field);
		if ( xrtHttpFieldNameEqual(Field.Name, Name) ) {
			iFound++;
		}
	}
	return iFound;
}

#endif
