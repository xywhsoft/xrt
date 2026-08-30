#include "../internal/xrt_http.h"

#include <xrt/http_upgrade.h>

#include <stdio.h>



#if defined(XRT_FEATURE_HTTP1_HEAD)

#define XRT_HTTP1_DEFAULT_HEAD UINT32_C(65536)
#define XRT_HTTP1_DEFAULT_START_LINE UINT32_C(8192)
#define XRT_HTTP1_DEFAULT_FIELD_LINE UINT32_C(8192)
#define XRT_HTTP1_DEFAULT_FIELDS UINT32_C(100)



/* Header 语义在首遍校验时收集，第二遍只发布已经验证的借用视图。 */
typedef struct xrt_http1_semantics {
	bool HasLength;
	bool HasTransfer;
	bool FinalChunked;
	bool HasOtherTransfer;
	bool ConnectionClose;
	bool ConnectionKeepAlive;
	bool ConnectionUpgrade;
	bool HasUpgrade;
	uint32 ChunkedCount;
	uint64 Length;
} xrt_http1_semantics;



/* 清空定位信息，成功和数据不足都不残留上一次错误。 */
static void __xrtHttp1ErrorClear(xhttp1errorinfo* pInfo)
{
	if ( pInfo != NULL ) {
		memset(pInfo, 0, sizeof(*pInfo));
	}
}



/* 清空 Head 的解析结果，同时保留调用方提供的字段数组。 */
static void __xrtHttp1HeadReset(xhttp1head* pHead)
{
	xhttpfield* pFields;
	size_t iCapacity;

	if ( pHead == NULL ) {
		return;
	}
	pFields = pHead->Fields;
	iCapacity = pHead->FieldCapacity;
	memset(pHead, 0, sizeof(*pHead));
	pHead->Fields = pFields;
	pHead->FieldCapacity = iCapacity;
}



/* 设置 HTTP/1 协议错误，并同步发布精确字节与行位置。 */
xhttp1status __xrtHttp1Fail(
	xhttp1head* pHead,
	xhttp1errorinfo* pInfo,
	xhttp1error Code,
	size_t iOffset,
	size_t iLine,
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage
)
{
	char sData[96];
	xerrordesc Desc;
	xerror* pError;

	__xrtHttp1HeadReset(pHead);
	if ( pInfo != NULL ) {
		pInfo->Code = Code;
		pInfo->Offset = iOffset;
		pInfo->Line = iLine;
	}
	(void)snprintf(
		sData,
		sizeof(sData),
		"offset=%llu;line=%llu",
		(unsigned long long)iOffset,
		(unsigned long long)iLine
	);
	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Code = (int32)Code;
	Desc.Domain = "xrt.http1";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Data = sData;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
	return XHTTP1_ERROR;
}



/* 返回指定版本在线路上的固定文本。 */
cstr __xrtHttp1VersionText(xhttpversion Version)
{
	if ( Version == XHTTP_VERSION_1_0 ) {
		return "HTTP/1.0";
	}
	if ( Version == XHTTP_VERSION_1_1 ) {
		return "HTTP/1.1";
	}
	return NULL;
}



/* 严格解析固定八字节 HTTP/1 版本。 */
static bool __xrtHttp1VersionParse(
	xstrview Text,
	xhttpversion* pVersion
)
{
	if ( (Text.Size != 8) ||
		(memcmp(Text.Data, "HTTP/1.", 7) != 0) ) {
		return false;
	}
	if ( Text.Data[7] == '0' ) {
		*pVersion = XHTTP_VERSION_1_0;
		return true;
	}
	if ( Text.Data[7] == '1' ) {
		*pVersion = XHTTP_VERSION_1_1;
		return true;
	}
	return false;
}



/* 验证 request-target 不包含空白、控制字符或线路禁用的 fragment。 */
XRT_API bool xrtHttp1TargetValid(xstrview Target)
{
	size_t i;

	if ( !__xrtHttpViewValid(Target) || (Target.Size == 0) ) {
		return false;
	}
	for ( i = 0; i < Target.Size; i++ ) {
		unsigned char iByte = (unsigned char)Target.Data[i];

		if ( (iByte <= UINT8_C(0x20)) ||
			(iByte == UINT8_C(0x7F)) ||
			(iByte == (unsigned char)'#') ) {
			return false;
		}
	}
	return true;
}



/* 读取一行结尾；调用点只在已经定位完整 Header 后使用。 */
static size_t __xrtHttp1FindCrlf(
	cstr sData,
	size_t iStart,
	size_t iEnd
)
{
	size_t i;

	for ( i = iStart; (i + 1u) < iEnd; i++ ) {
		if ( (sData[i] == '\r') && (sData[i + 1u] == '\n') ) {
			return i;
		}
	}
	return XRT_NPOS;
}



/*
	增量定位空行，并在完整 Header 到达前尽早拒绝裸 LF、坏 CR 和超长行。
	这样恶意连接不需要占满整个 Header 限额才会被关闭。
*/
static xhttp1status __xrtHttp1LocateHead(
	xbytesview Input,
	const xhttp1limits* pLimits,
	size_t* pBytes,
	xhttp1head* pHead,
	xhttp1errorinfo* pInfo,
	cstr sOperation
)
{
	cstr sData = (cstr)Input.Data;
	size_t iLineStart = 0;
	size_t iLine = 1;
	size_t i = 0;

	while ( i < Input.Size ) {
		size_t iLimit = (iLine == 1) ?
			pLimits->MaxStartLine : pLimits->MaxFieldLine;

		if ( i >= pLimits->MaxHead ) {
			return __xrtHttp1Fail(
				pHead, pInfo, XHTTP1_ERROR_HEAD_TOO_LARGE,
				0, 0, XERR_RANGE, sOperation,
				"HTTP/1 Header exceeds its limit"
			);
		}

		if ( sData[i] == '\n' ) {
			return __xrtHttp1Fail(
				pHead, pInfo, XHTTP1_ERROR_LINE_END, i, iLine,
				XERR_PROTOCOL, sOperation,
				"HTTP/1 line contains a bare LF"
			);
		}
		if ( sData[i] == '\r' ) {
			size_t iLength;

			if ( (i + 1u) == Input.Size ) {
				break;
			}
			if ( sData[i + 1u] != '\n' ) {
				return __xrtHttp1Fail(
					pHead, pInfo, XHTTP1_ERROR_LINE_END, i, iLine,
					XERR_PROTOCOL, sOperation,
					"HTTP/1 carriage return is not followed by LF"
				);
			}
			iLength = i - iLineStart;
			if ( iLength > iLimit ) {
				return __xrtHttp1Fail(
					pHead, pInfo,
					(iLine == 1) ? XHTTP1_ERROR_START_LINE_TOO_LARGE :
						XHTTP1_ERROR_FIELD_LINE_TOO_LARGE,
					iLineStart, iLine, XERR_RANGE, sOperation,
					(iLine == 1) ?
						"HTTP/1 start line exceeds its limit" :
						"HTTP/1 field line exceeds its limit"
				);
			}
			if ( (iLine != 1) && (iLength == 0) ) {
				*pBytes = i + 2u;
				if ( *pBytes > pLimits->MaxHead ) {
					return __xrtHttp1Fail(
						pHead, pInfo, XHTTP1_ERROR_HEAD_TOO_LARGE,
						0, 0, XERR_RANGE, sOperation,
						"HTTP/1 Header exceeds its limit"
					);
				}
				return XHTTP1_READY;
			}
			i += 2u;
			iLineStart = i;
			iLine++;
			continue;
		}
		if ( (i - iLineStart) >= iLimit ) {
			return __xrtHttp1Fail(
				pHead, pInfo,
				(iLine == 1) ? XHTTP1_ERROR_START_LINE_TOO_LARGE :
					XHTTP1_ERROR_FIELD_LINE_TOO_LARGE,
				iLineStart, iLine, XERR_RANGE, sOperation,
				(iLine == 1) ?
					"HTTP/1 start line exceeds its limit" :
					"HTTP/1 field line exceeds its limit"
			);
		}
		i++;
	}
	if ( Input.Size > pLimits->MaxHead ) {
		return __xrtHttp1Fail(
			pHead, pInfo, XHTTP1_ERROR_HEAD_TOO_LARGE,
			0, 0, XERR_RANGE, sOperation,
			"HTTP/1 Header exceeds its limit"
		);
	}
	return XHTTP1_MORE;
}



/* 解析请求起始行，并保持方法和目标为输入中的借用视图。 */
static bool __xrtHttp1RequestLine(
	xstrview Line,
	xhttp1head* pHead,
	xhttp1errorinfo* pInfo,
	cstr sOperation
)
{
	cstr sFirst;
	cstr sSecond;
	xstrview Version;

	sFirst = (cstr)memchr(Line.Data, ' ', Line.Size);
	if ( sFirst == NULL ) {
		(void)__xrtHttp1Fail(
			pHead, pInfo, XHTTP1_ERROR_START_LINE, 0, 1,
			XERR_PROTOCOL, sOperation,
			"HTTP/1 request line is incomplete"
		);
		return false;
	}
	sSecond = (cstr)memchr(
		sFirst + 1,
		' ',
		Line.Size - (size_t)((sFirst + 1) - Line.Data)
	);
	if ( sSecond == NULL ) {
		(void)__xrtHttp1Fail(
			pHead, pInfo, XHTTP1_ERROR_START_LINE, 0, 1,
			XERR_PROTOCOL, sOperation,
			"HTTP/1 request line is incomplete"
		);
		return false;
	}
	pHead->Method.Data = Line.Data;
	pHead->Method.Size = (size_t)(sFirst - Line.Data);
	pHead->Target.Data = sFirst + 1;
	pHead->Target.Size = (size_t)(sSecond - (sFirst + 1));
	Version.Data = sSecond + 1;
	Version.Size = Line.Size - (size_t)(Version.Data - Line.Data);
	pHead->MethodCode = xrtHttpMethodParse(pHead->Method);
	if ( pHead->MethodCode == XHTTP_METHOD_INVALID ) {
		(void)__xrtHttp1Fail(
			pHead, pInfo, XHTTP1_ERROR_METHOD, 0, 1,
			XERR_PROTOCOL, sOperation,
			"HTTP/1 request method is invalid"
		);
		return false;
	}
	if ( !xrtHttp1TargetValid(pHead->Target) ) {
		(void)__xrtHttp1Fail(
			pHead, pInfo, XHTTP1_ERROR_TARGET,
			(size_t)(pHead->Target.Data - Line.Data), 1,
			XERR_PROTOCOL, sOperation,
			"HTTP/1 request target is invalid"
		);
		return false;
	}
	if ( !__xrtHttp1VersionParse(Version, &pHead->Version) ) {
		(void)__xrtHttp1Fail(
			pHead, pInfo, XHTTP1_ERROR_VERSION,
			(size_t)(Version.Data - Line.Data), 1,
			XERR_PROTOCOL, sOperation,
			"HTTP/1 request version is unsupported"
		);
		return false;
	}
	return true;
}



/* 解析响应起始行，状态码保持三位十进制，Reason 可以为空。 */
static bool __xrtHttp1ResponseLine(
	xstrview Line,
	xhttp1head* pHead,
	xhttp1errorinfo* pInfo,
	cstr sOperation
)
{
	xstrview Version;

	if ( (Line.Size < 13) || (Line.Data[8] != ' ') ) {
		(void)__xrtHttp1Fail(
			pHead, pInfo, XHTTP1_ERROR_START_LINE, 0, 1,
			XERR_PROTOCOL, sOperation,
			"HTTP/1 response line is incomplete"
		);
		return false;
	}
	Version.Data = Line.Data;
	Version.Size = 8;
	if ( !__xrtHttp1VersionParse(Version, &pHead->Version) ) {
		(void)__xrtHttp1Fail(
			pHead, pInfo, XHTTP1_ERROR_VERSION, 0, 1,
			XERR_PROTOCOL, sOperation,
			"HTTP/1 response version is unsupported"
		);
		return false;
	}
	if ( (Line.Data[9] < '1') || (Line.Data[9] > '9') ||
		(Line.Data[10] < '0') || (Line.Data[10] > '9') ||
		(Line.Data[11] < '0') || (Line.Data[11] > '9') ) {
		(void)__xrtHttp1Fail(
			pHead, pInfo, XHTTP1_ERROR_STATUS, 9, 1,
			XERR_PROTOCOL, sOperation,
			"HTTP/1 response status is invalid"
		);
		return false;
	}
	pHead->Status = (uint16)(
		((Line.Data[9] - '0') * 100) +
		((Line.Data[10] - '0') * 10) +
		(Line.Data[11] - '0')
	);
	if ( Line.Data[12] != ' ' ) {
		(void)__xrtHttp1Fail(
			pHead, pInfo, XHTTP1_ERROR_STATUS, 12, 1,
			XERR_PROTOCOL, sOperation,
			"HTTP/1 response status is not followed by SP"
		);
		return false;
	}
	pHead->Reason.Data = Line.Data + 13;
	pHead->Reason.Size = Line.Size - 13u;
	if ( !xrtHttpFieldValueValid(pHead->Reason) ) {
		(void)__xrtHttp1Fail(
			pHead, pInfo, XHTTP1_ERROR_REASON, 13, 1,
			XERR_PROTOCOL, sOperation,
			"HTTP/1 response reason contains a control byte"
		);
		return false;
	}
	return true;
}



/* 从当前位置读取一个非空 token，并把位置推进到 token 后。 */
static bool __xrtHttp1ReadToken(
	xstrview Text,
	size_t* pPosition,
	xstrview* pToken
)
{
	size_t iStart = *pPosition;

	while ( (*pPosition < Text.Size) &&
		__xrtHttpTokenByte(
			(unsigned char)Text.Data[*pPosition]
		) ) {
		(*pPosition)++;
	}
	pToken->Data = Text.Data + iStart;
	pToken->Size = *pPosition - iStart;
	return pToken->Size != 0;
}



/* 删除当前位置的可选横向空白。 */
static void __xrtHttp1SkipOws(xstrview Text, size_t* pPosition)
{
	while ( (*pPosition < Text.Size) &&
		((Text.Data[*pPosition] == ' ') ||
		 (Text.Data[*pPosition] == '\t')) ) {
		(*pPosition)++;
	}
}



/* 严格读取 Transfer-Encoding 参数中的 quoted-string。 */
static bool __xrtHttp1ReadQuoted(xstrview Text, size_t* pPosition)
{
	if ( (*pPosition >= Text.Size) ||
		(Text.Data[*pPosition] != '"') ) {
		return false;
	}
	(*pPosition)++;
	while ( *pPosition < Text.Size ) {
		unsigned char iByte = (unsigned char)Text.Data[*pPosition];

		(*pPosition)++;
		if ( iByte == (unsigned char)'"' ) {
			return true;
		}
		if ( iByte == (unsigned char)'\\' ) {
			if ( *pPosition >= Text.Size ) {
				return false;
			}
			iByte = (unsigned char)Text.Data[*pPosition];
			if ( !__xrtHttpQuotedPairByte(iByte) ) {
				return false;
			}
			(*pPosition)++;
			continue;
		}
		if ( !__xrtHttpQuotedTextByte(iByte) ) {
			return false;
		}
	}
	return false;
}



/* 严格迭代一个 Transfer-Encoding 字段值。 */
XRT_API xhttpnext xrtHttp1TransferCodingNext(
	xstrview Value,
	size_t* pOffset,
	xhttp1transfercoding* pCoding
)
{
	xhttp1transfercoding Coding;
	size_t iParameter = XRT_NPOS;
	size_t i;

	if ( !__xrtHttpViewValid(Value) ||
		(pOffset == NULL) || (pCoding == NULL) ||
		(*pOffset > Value.Size) ||
		__xrtRangesOverlap(
			Value.Data, Value.Size,
			pOffset, sizeof(*pOffset)
		) || __xrtRangesOverlap(
			Value.Data, Value.Size,
			pCoding, sizeof(*pCoding)
		) || __xrtRangesOverlap(
			pOffset, sizeof(*pOffset),
			pCoding, sizeof(*pCoding)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memset(&Coding, 0, sizeof(Coding));
	memset(pCoding, 0, sizeof(*pCoding));
	i = *pOffset;
	__xrtHttp1SkipOws(Value, &i);
	if ( i == Value.Size ) {
		*pOffset = i;
		return XHTTP_NEXT_END;
	}
	if ( !__xrtHttp1ReadToken(Value, &i, &Coding.Name) ) {
		__xrtErrorSetValue();
		return XHTTP_NEXT_ERROR;
	}
	__xrtHttp1SkipOws(Value, &i);
	while ( (i < Value.Size) && (Value.Data[i] == ';') ) {
		xstrview Name;
		xstrview Parameter;

		i++;
		if ( iParameter == XRT_NPOS ) {
			iParameter = i;
		}
		__xrtHttp1SkipOws(Value, &i);
		if ( !__xrtHttp1ReadToken(Value, &i, &Name) ) {
			__xrtErrorSetValue();
			return XHTTP_NEXT_ERROR;
		}
		__xrtHttp1SkipOws(Value, &i);
		if ( (i >= Value.Size) || (Value.Data[i] != '=') ) {
			__xrtErrorSetValue();
			return XHTTP_NEXT_ERROR;
		}
		i++;
		__xrtHttp1SkipOws(Value, &i);
		if ( (i < Value.Size) && (Value.Data[i] == '"') ) {
			if ( !__xrtHttp1ReadQuoted(Value, &i) ) {
				__xrtErrorSetValue();
				return XHTTP_NEXT_ERROR;
			}
		} else if ( !__xrtHttp1ReadToken(Value, &i, &Parameter) ) {
			__xrtErrorSetValue();
			return XHTTP_NEXT_ERROR;
		}
		__xrtHttp1SkipOws(Value, &i);
	}
	if ( iParameter != XRT_NPOS ) {
		Coding.Parameters.Data = Value.Data + iParameter;
		Coding.Parameters.Size = i - iParameter;
	}
	if ( i < Value.Size ) {
		size_t iNext;

		if ( Value.Data[i] != ',' ) {
			__xrtErrorSetValue();
			return XHTTP_NEXT_ERROR;
		}
		i++;
		iNext = i;
		__xrtHttp1SkipOws(Value, &iNext);
		if ( iNext == Value.Size ) {
			__xrtErrorSetValue();
			return XHTTP_NEXT_ERROR;
		}
	}
	*pOffset = i;
	*pCoding = Coding;
	return XHTTP_NEXT_ITEM;
}



/*
	解析完整 Transfer-Encoding 列表。
	Header 层保留扩展 coding，只发布分帧所需的稳定线路事实。
*/
static bool __xrtHttp1TransferEncoding(
	xstrview Value,
	xrt_http1_semantics* pSemantics
)
{
	xhttp1transfercoding Coding;
	xhttpnext Next;
	size_t i = 0;
	bool bAny = false;

	for ( ;; ) {
		Next = xrtHttp1TransferCodingNext(Value, &i, &Coding);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			return bAny;
		}
		bAny = true;
		pSemantics->HasTransfer = true;
		pSemantics->FinalChunked = xrtHttpTokenEqual(
			Coding.Name, XRT_STR_LITERAL("chunked")
		);
		if ( pSemantics->FinalChunked ) {
			pSemantics->ChunkedCount++;
			if ( Coding.Parameters.Data != NULL ) {
				return false;
			}
		} else {
			pSemantics->HasOtherTransfer = true;
		}
	}
}



/* 严格解析 Connection token 列表并收集标准连接选项。 */
static bool __xrtHttp1Connection(
	xstrview Value,
	xrt_http1_semantics* pSemantics
)
{
	xhttpnext Next;
	size_t i = 0;

	for ( ;; ) {
		xstrview Token;

		Next = xrtHttpTokenNext(Value, &i, &Token);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			return true;
		}
		if ( xrtHttpTokenEqual(Token, XRT_STR_LITERAL("close")) ) {
			pSemantics->ConnectionClose = true;
		} else if ( xrtHttpTokenEqual(
			Token, XRT_STR_LITERAL("keep-alive")
		) ) {
			pSemantics->ConnectionKeepAlive = true;
		} else if ( xrtHttpTokenEqual(
			Token, XRT_STR_LITERAL("upgrade")
		) ) {
			pSemantics->ConnectionUpgrade = true;
		}
	}
}



/* 校验全部字段并收集消息分帧与连接语义。 */
static bool __xrtHttp1FieldsValidate(
	cstr sData,
	size_t iStart,
	size_t iHeadBytes,
	const xhttp1limits* pLimits,
	xhttp1head* pHead,
	xrt_http1_semantics* pSemantics,
	xhttp1errorinfo* pInfo,
	cstr sOperation
)
{
	size_t iPosition = iStart;
	size_t iLine = 2;

	while ( iPosition < (iHeadBytes - 2u) ) {
		size_t iEnd = __xrtHttp1FindCrlf(
			sData, iPosition, iHeadBytes
		);
		xhttpfield Field;
		xrt_http_field_error FieldError;
		size_t iLineSize;

		if ( iEnd == XRT_NPOS ) {
			(void)__xrtHttp1Fail(
				pHead, pInfo, XHTTP1_ERROR_LINE_END,
				iPosition, iLine, XERR_PROTOCOL, sOperation,
				"HTTP/1 field line has no CRLF"
			);
			return false;
		}
		iLineSize = iEnd - iPosition;
		if ( iLineSize > pLimits->MaxFieldLine ) {
			(void)__xrtHttp1Fail(
				pHead, pInfo, XHTTP1_ERROR_FIELD_LINE_TOO_LARGE,
				iPosition, iLine, XERR_RANGE, sOperation,
				"HTTP/1 field line exceeds its limit"
			);
			return false;
		}
		FieldError = __xrtHttpFieldParse(
			(xstrview){ sData + iPosition, iLineSize },
			&Field
		);
		if ( FieldError == XRT_HTTP_FIELD_NAME ) {
			(void)__xrtHttp1Fail(
				pHead, pInfo, XHTTP1_ERROR_FIELD_NAME,
				iPosition, iLine, XERR_PROTOCOL, sOperation,
				"HTTP/1 field name, colon or obs-fold is invalid"
			);
			return false;
		}
		if ( FieldError == XRT_HTTP_FIELD_VALUE ) {
			(void)__xrtHttp1Fail(
				pHead, pInfo, XHTTP1_ERROR_FIELD_VALUE,
				(size_t)(Field.Value.Data - sData), iLine,
				XERR_PROTOCOL, sOperation,
				"HTTP/1 field value contains a control byte"
			);
			return false;
		}
		pHead->FieldCount++;
		if ( pHead->FieldCount > pLimits->MaxFields ) {
			(void)__xrtHttp1Fail(
				pHead, pInfo, XHTTP1_ERROR_TOO_MANY_FIELDS,
				iPosition, iLine, XERR_RANGE, sOperation,
				"HTTP/1 field count exceeds its limit"
			);
			return false;
		}
		if ( xrtHttpFieldNameEqual(
			Field.Name, XRT_STR_LITERAL("Content-Length")
		) ) {
			uint64 iLength;
			xrt_http_content_length Result =
				__xrtHttpContentLengthParse(
					Field.Value, &iLength
				);

			if ( Result == XRT_HTTP_CONTENT_LENGTH_INVALID ) {
				(void)__xrtHttp1Fail(
					pHead, pInfo, XHTTP1_ERROR_CONTENT_LENGTH,
					(size_t)(Field.Value.Data - sData), iLine,
					XERR_PROTOCOL, sOperation,
					"HTTP/1 Content-Length is invalid"
				);
				return false;
			}
			if ( (Result == XRT_HTTP_CONTENT_LENGTH_CONFLICT) ||
				(pSemantics->HasLength &&
				 (pSemantics->Length != iLength)) ) {
				(void)__xrtHttp1Fail(
					pHead, pInfo,
					XHTTP1_ERROR_CONFLICTING_CONTENT_LENGTH,
					(size_t)(Field.Value.Data - sData), iLine,
					XERR_PROTOCOL, sOperation,
					"HTTP/1 Content-Length values conflict"
				);
				return false;
			}
			pSemantics->Length = iLength;
			pSemantics->HasLength = true;
		} else if ( xrtHttpFieldNameEqual(
			Field.Name, XRT_STR_LITERAL("Transfer-Encoding")
		) ) {
			if ( !__xrtHttp1TransferEncoding(
				Field.Value, pSemantics
			) ) {
				(void)__xrtHttp1Fail(
					pHead, pInfo, XHTTP1_ERROR_TRANSFER_ENCODING,
					(size_t)(Field.Value.Data - sData), iLine,
					XERR_PROTOCOL, sOperation,
					"HTTP/1 Transfer-Encoding is invalid"
				);
				return false;
			}
		} else if ( xrtHttpFieldNameEqual(
			Field.Name, XRT_STR_LITERAL("Connection")
		) ) {
			if ( !__xrtHttp1Connection(Field.Value, pSemantics) ) {
				(void)__xrtHttp1Fail(
					pHead, pInfo, XHTTP1_ERROR_CONNECTION,
					(size_t)(Field.Value.Data - sData), iLine,
					XERR_PROTOCOL, sOperation,
					"HTTP/1 Connection field is invalid"
				);
				return false;
			}
		} else if ( xrtHttpFieldNameEqual(
			Field.Name, XRT_STR_LITERAL("Upgrade")
		) ) {
			size_t iProtocols;

			if ( !xrtHttpUpgradeCount(
				Field.Value, &iProtocols
			) ) {
				(void)__xrtHttp1Fail(
					pHead, pInfo, XHTTP1_ERROR_UPGRADE,
					(size_t)(Field.Value.Data - sData), iLine,
					XERR_PROTOCOL, sOperation,
					"HTTP/1 Upgrade field is invalid"
				);
				return false;
			}
			if ( iProtocols != 0 ) {
				pSemantics->HasUpgrade = true;
			}
		}
		iPosition = iEnd + 2u;
		iLine++;
	}
	return true;
}



/* 在全部字段已经验证后发布描述符，不再执行可能失败的操作。 */
static void __xrtHttp1FieldsFill(
	cstr sData,
	size_t iStart,
	size_t iHeadBytes,
	xhttpfield* pFields
)
{
	size_t iPosition = iStart;
	size_t iField = 0;

	while ( iPosition < (iHeadBytes - 2u) ) {
		size_t iEnd = __xrtHttp1FindCrlf(
			sData, iPosition, iHeadBytes
		);

		(void)__xrtHttpFieldParse(
			(xstrview){ sData + iPosition, iEnd - iPosition },
			&pFields[iField]
		);
		iField++;
		iPosition = iEnd + 2u;
	}
}



/* 解析请求或响应 Header 的共同入口。 */
static xhttp1status __xrtHttp1Parse(
	xbytesview Input,
	xhttpkind Kind,
	xhttp1head* pHead,
	const xhttp1limits* pInputLimits,
	xhttp1errorinfo* pInfo,
	cstr sOperation
)
{
	xhttp1limits Limits;
	xhttp1head Parsed;
	xrt_http1_semantics Semantics;
	size_t iHeadBytes = 0;
	size_t iStartEnd;
	xhttp1status Status;

	__xrtHttp1ErrorClear(pInfo);
	if ( (pHead == NULL) ||
		((Input.Data == NULL) && (Input.Size != 0)) ||
		((pHead != NULL) && (pHead->Fields == NULL) &&
		 (pHead->FieldCapacity != 0)) ) {
		return __xrtHttp1Fail(
			pHead, pInfo, XHTTP1_ERROR_ARGUMENT, 0, 0,
			XERR_ARGUMENT, sOperation,
			"HTTP/1 input, Head or field storage is invalid"
		);
	}
	if ( pInputLimits == NULL ) {
		xrtHttp1LimitsInit(&Limits);
	} else {
		if ( !__xrtRangeValid(pInputLimits, sizeof(Limits)) ) {
			return __xrtHttp1Fail(
				pHead, pInfo, XHTTP1_ERROR_ARGUMENT, 0, 0,
				XERR_ARGUMENT, sOperation,
				"HTTP/1 limits storage is invalid"
			);
		}
		memcpy(&Limits, pInputLimits, sizeof(Limits));
	}
	if ( (Limits.MaxHead < 4) ||
		(Limits.MaxStartLine == 0) ||
		(Limits.MaxFieldLine == 0) ) {
		return __xrtHttp1Fail(
			pHead, pInfo, XHTTP1_ERROR_ARGUMENT, 0, 0,
			XERR_ARGUMENT, sOperation,
			"HTTP/1 limits are invalid"
		);
	}
	__xrtHttp1HeadReset(pHead);
	Status = __xrtHttp1LocateHead(
		Input, &Limits, &iHeadBytes, pHead, pInfo, sOperation
	);
	if ( Status != XHTTP1_READY ) {
		return Status;
	}
	memset(&Parsed, 0, sizeof(Parsed));
	memset(&Semantics, 0, sizeof(Semantics));
	Parsed.Kind = Kind;
	Parsed.Bytes = iHeadBytes;
	Parsed.Fields = pHead->Fields;
	Parsed.FieldCapacity = pHead->FieldCapacity;
	iStartEnd = __xrtHttp1FindCrlf(
		(cstr)Input.Data, 0, iHeadBytes
	);
	if ( (iStartEnd == XRT_NPOS) ||
		(iStartEnd > Limits.MaxStartLine) ) {
		return __xrtHttp1Fail(
			pHead, pInfo,
			(iStartEnd == XRT_NPOS) ? XHTTP1_ERROR_START_LINE :
				XHTTP1_ERROR_START_LINE_TOO_LARGE,
			0, 1,
			(iStartEnd == XRT_NPOS) ? XERR_PROTOCOL : XERR_RANGE,
			sOperation,
			(iStartEnd == XRT_NPOS) ?
				"HTTP/1 start line has no CRLF" :
				"HTTP/1 start line exceeds its limit"
		);
	}
	if ( Kind == XHTTP_REQUEST ) {
		if ( !__xrtHttp1RequestLine(
			(xstrview){ (cstr)Input.Data, iStartEnd },
			&Parsed, pInfo, sOperation
		) ) {
			__xrtHttp1HeadReset(pHead);
			return XHTTP1_ERROR;
		}
	} else if ( !__xrtHttp1ResponseLine(
		(xstrview){ (cstr)Input.Data, iStartEnd },
		&Parsed, pInfo, sOperation
	) ) {
		__xrtHttp1HeadReset(pHead);
		return XHTTP1_ERROR;
	}
	if ( !__xrtHttp1FieldsValidate(
		(cstr)Input.Data,
		iStartEnd + 2u,
		iHeadBytes,
		&Limits,
		&Parsed,
		&Semantics,
		pInfo,
		sOperation
	) ) {
		__xrtHttp1HeadReset(pHead);
		return XHTTP1_ERROR;
	}
	if ( Semantics.HasTransfer &&
		((Parsed.Version != XHTTP_VERSION_1_1) ||
		 (Semantics.ChunkedCount > 1)) ) {
		return __xrtHttp1Fail(
			pHead, pInfo,
			XHTTP1_ERROR_UNSUPPORTED_TRANSFER_ENCODING,
			0, 0, XERR_UNSUPPORTED, sOperation,
			"HTTP/1 transfer coding is not supported"
		);
	}
	if ( Semantics.HasLength ) {
		Parsed.Flags |= (uint32)XHTTP1_CONTENT_LENGTH;
		Parsed.ContentLength = Semantics.Length;
	}
	if ( Semantics.HasTransfer ) {
		Parsed.Flags |= (uint32)XHTTP1_TRANSFER_ENCODING;
		if ( Semantics.FinalChunked ) {
			Parsed.Flags |= (uint32)XHTTP1_CHUNKED;
		}
		if ( Semantics.HasOtherTransfer ) {
			Parsed.Flags |= (uint32)XHTTP1_TRANSFER_OTHER;
		}
	}
	if ( Semantics.ConnectionClose ) {
		Parsed.Flags |= (uint32)XHTTP1_CONNECTION_CLOSE;
	} else if ( (Parsed.Version == XHTTP_VERSION_1_1) ||
		Semantics.ConnectionKeepAlive ) {
		Parsed.Flags |= (uint32)XHTTP1_KEEP_ALIVE;
	}
	if ( Semantics.ConnectionUpgrade && Semantics.HasUpgrade ) {
		Parsed.Flags |= (uint32)XHTTP1_UPGRADE;
	}
	*pHead = Parsed;
	if ( Parsed.FieldCount > Parsed.FieldCapacity ) {
		return XHTTP1_FIELDS;
	}
	__xrtHttp1FieldsFill(
		(cstr)Input.Data,
		iStartEnd + 2u,
		iHeadBytes,
		Parsed.Fields
	);
	return XHTTP1_READY;
}



/* 初始化适合公网输入的默认解析限额。 */
XRT_API void xrtHttp1LimitsInit(xhttp1limits* pLimits)
{
	const xhttp1limits Limits = {
		XRT_HTTP1_DEFAULT_HEAD,
		XRT_HTTP1_DEFAULT_START_LINE,
		XRT_HTTP1_DEFAULT_FIELD_LINE,
		XRT_HTTP1_DEFAULT_FIELDS
	};

	if ( !__xrtRangeValid(pLimits, sizeof(Limits)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memcpy(pLimits, &Limits, sizeof(Limits));
}



/* 初始化借用调用方字段数组的空 Head。 */
XRT_API void xrtHttp1HeadInit(
	xhttp1head* pHead,
	xhttpfield* pFields,
	size_t iCapacity
)
{
	if ( (pHead == NULL) ||
		((pFields == NULL) && (iCapacity != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(pHead, 0, sizeof(*pHead));
	pHead->Fields = pFields;
	pHead->FieldCapacity = iCapacity;
}



/* 严格增量解析 HTTP/1.0 或 HTTP/1.1 请求 Header。 */
XRT_API xhttp1status xrtHttp1RequestParse(
	xbytesview Input,
	xhttp1head* pHead,
	const xhttp1limits* pLimits,
	xhttp1errorinfo* pError
)
{
	return __xrtHttp1Parse(
		Input, XHTTP_REQUEST, pHead, pLimits,
		pError, "parse-http1-request"
	);
}



/* 严格增量解析 HTTP/1.0 或 HTTP/1.1 响应 Header。 */
XRT_API xhttp1status xrtHttp1ResponseParse(
	xbytesview Input,
	xhttp1head* pHead,
	const xhttp1limits* pLimits,
	xhttp1errorinfo* pError
)
{
	return __xrtHttp1Parse(
		Input, XHTTP_RESPONSE, pHead, pLimits,
		pError, "parse-http1-response"
	);
}



/* 返回第一个同名 Header，未找到返回空指针。 */
XRT_API const xhttpfield* xrtHttp1Field(
	const xhttp1head* pHead,
	xstrview Name
)
{
	size_t iPosition;

	if ( (pHead == NULL) ||
		((pHead->Fields == NULL) && (pHead->FieldCount != 0)) ||
		(pHead->FieldCount > pHead->FieldCapacity) ||
		!__xrtHttpViewValid(Name) ) {
		if ( (pHead != NULL) &&
			(pHead->FieldCount > pHead->FieldCapacity) ) {
			__xrtErrorSetInvalidState();
		} else {
			__xrtErrorSetInvalidArgument();
		}
		return NULL;
	}
	iPosition = xrtHttpFieldFind(
		pHead->Fields, pHead->FieldCount, Name, 0
	);
	return iPosition != XRT_NPOS ?
		&pHead->Fields[iPosition] : NULL;
}



#endif
