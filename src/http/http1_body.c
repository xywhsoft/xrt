#include "../internal/xrt_http.h"



#if defined(XRT_FEATURE_HTTP1_BODY)

#define XRT_HTTP1_BODY_CHUNK_SIZE_START UINT32_C(1)
#define XRT_HTTP1_BODY_CHUNK_SIZE UINT32_C(2)
#define XRT_HTTP1_BODY_CHUNK_SIZE_BWS UINT32_C(3)
#define XRT_HTTP1_BODY_CHUNK_EXT_NAME_START UINT32_C(4)
#define XRT_HTTP1_BODY_CHUNK_EXT_NAME UINT32_C(5)
#define XRT_HTTP1_BODY_CHUNK_EXT_AFTER_NAME UINT32_C(6)
#define XRT_HTTP1_BODY_CHUNK_EXT_VALUE_START UINT32_C(7)
#define XRT_HTTP1_BODY_CHUNK_EXT_TOKEN UINT32_C(8)
#define XRT_HTTP1_BODY_CHUNK_EXT_QUOTED UINT32_C(9)
#define XRT_HTTP1_BODY_CHUNK_EXT_ESCAPE UINT32_C(10)
#define XRT_HTTP1_BODY_CHUNK_EXT_AFTER_VALUE UINT32_C(11)
#define XRT_HTTP1_BODY_CHUNK_EXT_VALUE_BWS UINT32_C(12)
#define XRT_HTTP1_BODY_CHUNK_LINE_LF UINT32_C(13)
#define XRT_HTTP1_BODY_CHUNK_DATA UINT32_C(14)
#define XRT_HTTP1_BODY_CHUNK_DATA_CR UINT32_C(15)
#define XRT_HTTP1_BODY_CHUNK_DATA_LF UINT32_C(16)
#define XRT_HTTP1_BODY_TRAILERS UINT32_C(17)
#define XRT_HTTP1_BODY_FIXED_DATA UINT32_C(18)
#define XRT_HTTP1_BODY_CLOSE_DATA UINT32_C(19)
#define XRT_HTTP1_BODY_COMPLETE UINT32_C(20)
#define XRT_HTTP1_BODY_FAILED UINT32_C(21)

#define XRT_HTTP1_DEFAULT_CHUNK_LINE UINT32_C(8192)
#define XRT_HTTP1_DEFAULT_TRAILER UINT32_C(16384)
#define XRT_HTTP1_DEFAULT_TRAILER_LINE UINT32_C(8192)
#define XRT_HTTP1_DEFAULT_TRAILERS UINT32_C(100)



/* 把 64 位线缆位置安全压缩到公开的 size_t 错误偏移。 */
static size_t __xrtHttp1BodyOffset(uint64 iOffset)
{
	if ( iOffset > (uint64)SIZE_MAX ) {
		return SIZE_MAX;
	}
	return (size_t)iOffset;
}



/* 清空一次解析调用的可选错误位置。 */
static void __xrtHttp1BodyErrorClear(xhttp1errorinfo* pError)
{
	if ( pError != NULL ) {
		memset(pError, 0, sizeof(*pError));
	}
}



/* 发布 Body 协议错误并把 Reader 固定在不可继续使用的终态。 */
static xhttp1bodystatus __xrtHttp1BodyFail(
	xhttp1body* pBody,
	xhttp1errorinfo* pError,
	xhttp1error Code,
	uint64 iOffset,
	size_t iLine,
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage
)
{
	if ( pBody != NULL ) {
		pBody->State = XRT_HTTP1_BODY_FAILED;
	}
	(void)__xrtHttp1Fail(
		NULL,
		pError,
		Code,
		__xrtHttp1BodyOffset(iOffset),
		iLine,
		Kind,
		sOperation,
		sMessage
	);
	return XHTTP1_BODY_ERROR;
}



/* 发布 Body Plan 错误；Plan 失败时不留下部分结论。 */
static bool __xrtHttp1BodyPlanFail(
	xhttp1bodyplan* pPlan,
	xhttp1error Code,
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage
)
{
	if ( pPlan != NULL ) {
		memset(pPlan, 0, sizeof(*pPlan));
	}
	(void)__xrtHttp1Fail(
		NULL, NULL, Code, 0, 0, Kind, sOperation, sMessage
	);
	return false;
}



/* 验证 Body Plan 输入是已经完成解析的指定方向 Header。 */
static bool __xrtHttp1BodyHeadValid(
	const xhttp1head* pHead,
	xhttpkind Kind
)
{
	return (pHead != NULL) &&
		(pHead->Kind == Kind) &&
		(pHead->Version >= XHTTP_VERSION_1_0) &&
		(pHead->Version <= XHTTP_VERSION_1_1) &&
		(pHead->Bytes != 0);
}



/* 复制正文限额，空指针使用默认值；零数量用于禁止对应输入。 */
static bool __xrtHttp1BodyLimitsResolve(
	const xhttp1bodylimits* pInput,
	xhttp1bodylimits* pLimits
)
{
	if ( pInput == NULL ) {
		xrtHttp1BodyLimitsInit(pLimits);
	} else {
		if ( !__xrtRangeValid(pInput, sizeof(*pLimits)) ) {
			return false;
		}
		memcpy(pLimits, pInput, sizeof(*pLimits));
	}
	return true;
}



/* 返回十六进制数字值，非十六进制字节返回负数。 */
static int __xrtHttp1Hex(unsigned char iByte)
{
	if ( (iByte >= (unsigned char)'0') &&
		(iByte <= (unsigned char)'9') ) {
		return (int)(iByte - (unsigned char)'0');
	}
	if ( (iByte >= (unsigned char)'a') &&
		(iByte <= (unsigned char)'f') ) {
		return (int)(iByte - (unsigned char)'a') + 10;
	}
	if ( (iByte >= (unsigned char)'A') &&
		(iByte <= (unsigned char)'F') ) {
		return (int)(iByte - (unsigned char)'A') + 10;
	}
	return -1;
}



/* chunk 行进入 CR 后统一等待 LF。 */
static bool __xrtHttp1ChunkLineEnd(
	xhttp1body* pBody,
	unsigned char iByte
)
{
	if ( iByte != (unsigned char)'\r' ) {
		return false;
	}
	pBody->State = XRT_HTTP1_BODY_CHUNK_LINE_LF;
	return true;
}



/*
	消费一个 chunk-size 或扩展语法字节。
	函数只在字节有效时推进状态，调用方随后统一增加线缆位置。
*/
static bool __xrtHttp1ChunkSyntax(
	xhttp1body* pBody,
	unsigned char iByte,
	xhttp1errorinfo* pError
)
{
	uint32 iState = pBody->State;
	int iHex;

	if ( iState == XRT_HTTP1_BODY_CHUNK_LINE_LF ) {
		if ( iByte != (unsigned char)'\n' ) {
			(void)__xrtHttp1BodyFail(
				pBody, pError, XHTTP1_ERROR_CHUNK_TERMINATOR,
				pBody->WireBytes, 0, XERR_PROTOCOL,
				"read-http1-body", "chunk line CR is not followed by LF"
			);
			return false;
		}
		if ( pBody->ChunkSize == 0 ) {
			pBody->State = XRT_HTTP1_BODY_TRAILERS;
		} else {
			if ( (pBody->Received > pBody->Limits.MaxBody) ||
				(pBody->ChunkSize >
				 (pBody->Limits.MaxBody - pBody->Received)) ) {
				(void)__xrtHttp1BodyFail(
					pBody, pError, XHTTP1_ERROR_BODY_TOO_LARGE,
					pBody->WireBytes, 0, XERR_RANGE,
					"read-http1-body", "chunked body exceeds its limit"
				);
				return false;
			}
			pBody->Remaining = pBody->ChunkSize;
			pBody->State = XRT_HTTP1_BODY_CHUNK_DATA;
		}
		return true;
	}

	if ( iByte != (unsigned char)'\r' ) {
		if ( pBody->ChunkLineBytes >= pBody->Limits.MaxChunkLine ) {
			(void)__xrtHttp1BodyFail(
				pBody, pError, XHTTP1_ERROR_CHUNK_LINE_TOO_LARGE,
				pBody->WireBytes, 0, XERR_RANGE,
				"read-http1-body", "chunk line exceeds its limit"
			);
			return false;
		}
		pBody->ChunkLineBytes++;
	}

	switch ( iState ) {
		case XRT_HTTP1_BODY_CHUNK_SIZE_START:
			iHex = __xrtHttp1Hex(iByte);
			if ( iHex < 0 ) {
				break;
			}
			pBody->ChunkSize = (uint64)iHex;
			pBody->State = XRT_HTTP1_BODY_CHUNK_SIZE;
			return true;

		case XRT_HTTP1_BODY_CHUNK_SIZE:
			iHex = __xrtHttp1Hex(iByte);
			if ( iHex >= 0 ) {
				if ( pBody->ChunkSize >
					((UINT64_MAX - (uint64)iHex) / UINT64_C(16)) ) {
					(void)__xrtHttp1BodyFail(
						pBody, pError, XHTTP1_ERROR_CHUNK_SIZE,
						pBody->WireBytes, 0, XERR_RANGE,
						"read-http1-body", "chunk size overflows uint64"
					);
					return false;
				}
				pBody->ChunkSize =
					(pBody->ChunkSize * UINT64_C(16)) + (uint64)iHex;
				return true;
			}
			if ( (iByte == (unsigned char)' ') ||
				(iByte == (unsigned char)'\t') ) {
				pBody->State = XRT_HTTP1_BODY_CHUNK_SIZE_BWS;
				return true;
			}
			if ( iByte == (unsigned char)';' ) {
				pBody->State = XRT_HTTP1_BODY_CHUNK_EXT_NAME_START;
				return true;
			}
			if ( __xrtHttp1ChunkLineEnd(pBody, iByte) ) {
				return true;
			}
			break;

		case XRT_HTTP1_BODY_CHUNK_SIZE_BWS:
			if ( (iByte == (unsigned char)' ') ||
				(iByte == (unsigned char)'\t') ) {
				return true;
			}
			if ( iByte == (unsigned char)';' ) {
				pBody->State = XRT_HTTP1_BODY_CHUNK_EXT_NAME_START;
				return true;
			}
			break;

		case XRT_HTTP1_BODY_CHUNK_EXT_NAME_START:
			if ( (iByte == (unsigned char)' ') ||
				(iByte == (unsigned char)'\t') ) {
				return true;
			}
			if ( __xrtHttpTokenByte(iByte) ) {
				pBody->State = XRT_HTTP1_BODY_CHUNK_EXT_NAME;
				return true;
			}
			break;

		case XRT_HTTP1_BODY_CHUNK_EXT_NAME:
			if ( __xrtHttpTokenByte(iByte) ) {
				return true;
			}
			if ( (iByte == (unsigned char)' ') ||
				(iByte == (unsigned char)'\t') ) {
				pBody->State = XRT_HTTP1_BODY_CHUNK_EXT_AFTER_NAME;
				return true;
			}
			if ( iByte == (unsigned char)'=' ) {
				pBody->State = XRT_HTTP1_BODY_CHUNK_EXT_VALUE_START;
				return true;
			}
			if ( iByte == (unsigned char)';' ) {
				pBody->State = XRT_HTTP1_BODY_CHUNK_EXT_NAME_START;
				return true;
			}
			if ( __xrtHttp1ChunkLineEnd(pBody, iByte) ) {
				return true;
			}
			break;

		case XRT_HTTP1_BODY_CHUNK_EXT_AFTER_NAME:
			if ( (iByte == (unsigned char)' ') ||
				(iByte == (unsigned char)'\t') ) {
				return true;
			}
			if ( iByte == (unsigned char)'=' ) {
				pBody->State = XRT_HTTP1_BODY_CHUNK_EXT_VALUE_START;
				return true;
			}
			if ( iByte == (unsigned char)';' ) {
				pBody->State = XRT_HTTP1_BODY_CHUNK_EXT_NAME_START;
				return true;
			}
			break;

		case XRT_HTTP1_BODY_CHUNK_EXT_VALUE_START:
			if ( (iByte == (unsigned char)' ') ||
				(iByte == (unsigned char)'\t') ) {
				return true;
			}
			if ( iByte == (unsigned char)'"' ) {
				pBody->State = XRT_HTTP1_BODY_CHUNK_EXT_QUOTED;
				return true;
			}
			if ( __xrtHttpTokenByte(iByte) ) {
				pBody->State = XRT_HTTP1_BODY_CHUNK_EXT_TOKEN;
				return true;
			}
			break;

		case XRT_HTTP1_BODY_CHUNK_EXT_TOKEN:
			if ( __xrtHttpTokenByte(iByte) ) {
				return true;
			}
			if ( (iByte == (unsigned char)' ') ||
				(iByte == (unsigned char)'\t') ) {
				pBody->State = XRT_HTTP1_BODY_CHUNK_EXT_VALUE_BWS;
				return true;
			}
			if ( iByte == (unsigned char)';' ) {
				pBody->State = XRT_HTTP1_BODY_CHUNK_EXT_NAME_START;
				return true;
			}
			if ( __xrtHttp1ChunkLineEnd(pBody, iByte) ) {
				return true;
			}
			break;

		case XRT_HTTP1_BODY_CHUNK_EXT_QUOTED:
			if ( iByte == (unsigned char)'"' ) {
				pBody->State = XRT_HTTP1_BODY_CHUNK_EXT_AFTER_VALUE;
				return true;
			}
			if ( iByte == (unsigned char)'\\' ) {
				pBody->State = XRT_HTTP1_BODY_CHUNK_EXT_ESCAPE;
				return true;
			}
			if ( __xrtHttpQuotedTextByte(iByte) ) {
				return true;
			}
			break;

		case XRT_HTTP1_BODY_CHUNK_EXT_ESCAPE:
			if ( __xrtHttpQuotedPairByte(iByte) ) {
				pBody->State = XRT_HTTP1_BODY_CHUNK_EXT_QUOTED;
				return true;
			}
			break;

		case XRT_HTTP1_BODY_CHUNK_EXT_AFTER_VALUE:
			if ( (iByte == (unsigned char)' ') ||
				(iByte == (unsigned char)'\t') ) {
				pBody->State = XRT_HTTP1_BODY_CHUNK_EXT_VALUE_BWS;
				return true;
			}
			if ( iByte == (unsigned char)';' ) {
				pBody->State = XRT_HTTP1_BODY_CHUNK_EXT_NAME_START;
				return true;
			}
			if ( __xrtHttp1ChunkLineEnd(pBody, iByte) ) {
				return true;
			}
			break;

		case XRT_HTTP1_BODY_CHUNK_EXT_VALUE_BWS:
			if ( (iByte == (unsigned char)' ') ||
				(iByte == (unsigned char)'\t') ) {
				return true;
			}
			if ( iByte == (unsigned char)';' ) {
				pBody->State = XRT_HTTP1_BODY_CHUNK_EXT_NAME_START;
				return true;
			}
			break;

		default:
			break;
	}

	(void)__xrtHttp1BodyFail(
		pBody,
		pError,
		(iState <= XRT_HTTP1_BODY_CHUNK_SIZE_BWS) ?
			XHTTP1_ERROR_CHUNK_SIZE : XHTTP1_ERROR_CHUNK_EXTENSION,
		pBody->WireBytes,
		0,
		XERR_PROTOCOL,
		"read-http1-body",
		(iState <= XRT_HTTP1_BODY_CHUNK_SIZE_BWS) ?
			"chunk size line is invalid" : "chunk extension is invalid"
	);
	return false;
}



/* 使用读取端状态机验证原样 chunk 扩展，避免发送端形成第二套语法。 */
bool __xrtHttp1ChunkExtensionsValid(xstrview Extensions)
{
	xhttp1body Body;
	size_t i;

	if ( !__xrtHttpViewValid(Extensions) ) {
		return false;
	}
	if ( Extensions.Size == 0 ) {
		return true;
	}
	if ( Extensions.Data[0] != ';' ) {
		return false;
	}
	memset(&Body, 0, sizeof(Body));
	Body.State = XRT_HTTP1_BODY_CHUNK_SIZE;
	Body.ChunkSize = 1;
	Body.Limits.MaxBody = UINT64_MAX;
	Body.Limits.MaxChunkLine = SIZE_MAX;
	for ( i = 0; i < Extensions.Size; i++ ) {
		if ( !__xrtHttp1ChunkSyntax(
			&Body, (unsigned char)Extensions.Data[i], NULL
		) ) {
			return false;
		}
		Body.WireBytes++;
	}
	if ( !__xrtHttp1ChunkSyntax(&Body, (unsigned char)'\r', NULL) ) {
		return false;
	}
	Body.WireBytes++;
	if ( !__xrtHttp1ChunkSyntax(&Body, (unsigned char)'\n', NULL) ) {
		return false;
	}
	return (Body.State == XRT_HTTP1_BODY_CHUNK_DATA) &&
		(Body.ChunkSize == 1);
}



/* 定位完整 trailer 区并在第一遍严格校验每一行。 */
static xhttp1status __xrtHttp1TrailersParse(
	xbytesview Input,
	xhttpfield* pFields,
	size_t iCapacity,
	const xhttp1bodylimits* pInputLimits,
	size_t* pBytes,
	size_t* pCount,
	xhttp1errorinfo* pError,
	uint64 iBaseOffset
)
{
	xhttp1bodylimits Limits = { 0 };
	cstr sData = (cstr)Input.Data;
	size_t iPosition = 0;
	size_t iLineStart = 0;
	size_t iLine = 1;
	size_t iCount = 0;
	size_t iTrailerBytes = 0;

	if ( pBytes != NULL ) {
		*pBytes = 0;
	}
	if ( pCount != NULL ) {
		*pCount = 0;
	}
	__xrtHttp1BodyErrorClear(pError);
	if ( ((Input.Data == NULL) && (Input.Size != 0)) ||
		((pFields == NULL) && (iCapacity != 0)) ||
		(pBytes == NULL) || (pCount == NULL) ||
		!__xrtHttp1BodyLimitsResolve(pInputLimits, &Limits) ) {
		(void)__xrtHttp1Fail(
			NULL, pError, XHTTP1_ERROR_ARGUMENT,
			__xrtHttp1BodyOffset(iBaseOffset), 0,
			XERR_ARGUMENT, "parse-http1-trailers",
			"trailer input, output or limits are invalid"
		);
		return XHTTP1_ERROR;
	}

	while ( iPosition < Input.Size ) {
		unsigned char iByte = (unsigned char)sData[iPosition];

		if ( iPosition >= Limits.MaxTrailer ) {
			(void)__xrtHttp1Fail(
				NULL, pError, XHTTP1_ERROR_TRAILER_TOO_LARGE,
				__xrtHttp1BodyOffset(iBaseOffset + iPosition),
				iLine, XERR_RANGE, "parse-http1-trailers",
				"trailer section exceeds its limit"
			);
			return XHTTP1_ERROR;
		}
		if ( iByte == (unsigned char)'\n' ) {
			(void)__xrtHttp1Fail(
				NULL, pError, XHTTP1_ERROR_LINE_END,
				__xrtHttp1BodyOffset(iBaseOffset + iPosition),
				iLine, XERR_PROTOCOL, "parse-http1-trailers",
				"trailer line contains a bare LF"
			);
			return XHTTP1_ERROR;
		}
		if ( iByte == (unsigned char)'\r' ) {
			size_t iLineSize;
			xhttpfield Field;
			xrt_http_field_error FieldError;

			if ( (iPosition + 1u) == Input.Size ) {
				break;
			}
			if ( sData[iPosition + 1u] != '\n' ) {
				(void)__xrtHttp1Fail(
					NULL, pError, XHTTP1_ERROR_LINE_END,
					__xrtHttp1BodyOffset(iBaseOffset + iPosition),
					iLine, XERR_PROTOCOL, "parse-http1-trailers",
					"trailer CR is not followed by LF"
				);
				return XHTTP1_ERROR;
			}
			iLineSize = iPosition - iLineStart;
			if ( iLineSize > Limits.MaxTrailerLine ) {
				(void)__xrtHttp1Fail(
					NULL, pError, XHTTP1_ERROR_TRAILER_LINE_TOO_LARGE,
					__xrtHttp1BodyOffset(iBaseOffset + iLineStart),
					iLine, XERR_RANGE, "parse-http1-trailers",
					"trailer field line exceeds its limit"
				);
				return XHTTP1_ERROR;
			}
			if ( iLineSize == 0 ) {
				iTrailerBytes = iPosition + 2u;
				break;
			}
			FieldError = __xrtHttpFieldParse(
				(xstrview){ sData + iLineStart, iLineSize },
				&Field
			);
			if ( FieldError != XRT_HTTP_FIELD_VALID ) {
				(void)__xrtHttp1Fail(
					NULL,
					pError,
					(FieldError == XRT_HTTP_FIELD_VALUE) ?
						XHTTP1_ERROR_FIELD_VALUE : XHTTP1_ERROR_FIELD_NAME,
					__xrtHttp1BodyOffset(iBaseOffset + iLineStart),
					iLine,
					XERR_PROTOCOL,
					"parse-http1-trailers",
					(FieldError == XRT_HTTP_FIELD_VALUE) ?
						"trailer field value is invalid" :
						"trailer field name, colon or obs-fold is invalid"
				);
				return XHTTP1_ERROR;
			}
			iCount++;
			if ( iCount > Limits.MaxTrailers ) {
				(void)__xrtHttp1Fail(
					NULL, pError, XHTTP1_ERROR_TOO_MANY_TRAILERS,
					__xrtHttp1BodyOffset(iBaseOffset + iLineStart),
					iLine, XERR_RANGE, "parse-http1-trailers",
					"trailer field count exceeds its limit"
				);
				return XHTTP1_ERROR;
			}
			iPosition += 2u;
			iLineStart = iPosition;
			iLine++;
			continue;
		}
		if ( (iPosition - iLineStart) >= Limits.MaxTrailerLine ) {
			(void)__xrtHttp1Fail(
				NULL, pError, XHTTP1_ERROR_TRAILER_LINE_TOO_LARGE,
				__xrtHttp1BodyOffset(iBaseOffset + iLineStart),
				iLine, XERR_RANGE, "parse-http1-trailers",
				"trailer field line exceeds its limit"
			);
			return XHTTP1_ERROR;
		}
		iPosition++;
	}

	if ( iTrailerBytes == 0 ) {
		if ( Input.Size >= Limits.MaxTrailer ) {
			(void)__xrtHttp1Fail(
				NULL, pError, XHTTP1_ERROR_TRAILER_TOO_LARGE,
				__xrtHttp1BodyOffset(iBaseOffset), 0,
				XERR_RANGE, "parse-http1-trailers",
				"trailer section exceeds its limit"
			);
			return XHTTP1_ERROR;
		}
		return XHTTP1_MORE;
	}
	*pBytes = iTrailerBytes;
	*pCount = iCount;
	if ( iCount > iCapacity ) {
		return XHTTP1_FIELDS;
	}

	/* 第二遍只发布已经完整校验的借用字段视图。 */
	iPosition = 0;
	iLineStart = 0;
	iCount = 0;
	while ( iPosition < (iTrailerBytes - 2u) ) {
		if ( (sData[iPosition] == '\r') &&
			(sData[iPosition + 1u] == '\n') ) {
			(void)__xrtHttpFieldParse(
				(xstrview){ sData + iLineStart, iPosition - iLineStart },
				&pFields[iCount]
			);
			iCount++;
			iPosition += 2u;
			iLineStart = iPosition;
			continue;
		}
		iPosition++;
	}
	return XHTTP1_READY;
}



/* 初始化无正文预设上限的流式 Body 限额。 */
XRT_API void xrtHttp1BodyLimitsInit(xhttp1bodylimits* pLimits)
{
	const xhttp1bodylimits Limits = {
		UINT64_MAX,
		XRT_HTTP1_DEFAULT_CHUNK_LINE,
		XRT_HTTP1_DEFAULT_TRAILER,
		XRT_HTTP1_DEFAULT_TRAILER_LINE,
		XRT_HTTP1_DEFAULT_TRAILERS
	};

	if ( !__xrtRangeValid(pLimits, sizeof(Limits)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memcpy(pLimits, &Limits, sizeof(Limits));
}



/* 按 RFC 9112 请求分帧优先级生成 Body Plan。 */
XRT_API bool xrtHttp1RequestBodyPlan(
	const xhttp1head* pHead,
	xhttp1bodyplan* pPlan
)
{
	if ( !__xrtHttp1BodyHeadValid(pHead, XHTTP_REQUEST) ||
		(pPlan == NULL) ) {
		return __xrtHttp1BodyPlanFail(
			pPlan, XHTTP1_ERROR_ARGUMENT, XERR_ARGUMENT,
			"plan-http1-request-body", "request Header or Body Plan is invalid"
		);
	}
	memset(pPlan, 0, sizeof(*pPlan));
	if ( ((pHead->Flags & (uint32)XHTTP1_TRANSFER_ENCODING) != 0) &&
		((pHead->Flags & (uint32)XHTTP1_CONTENT_LENGTH) != 0) ) {
		return __xrtHttp1BodyPlanFail(
			pPlan, XHTTP1_ERROR_TRANSFER_LENGTH, XERR_PROTOCOL,
			"plan-http1-request-body",
			"request contains both Transfer-Encoding and Content-Length"
		);
	}
	if ( (pHead->Flags & (uint32)XHTTP1_TRANSFER_ENCODING) != 0 ) {
		if ( (pHead->Flags & (uint32)XHTTP1_CHUNKED) == 0 ) {
			return __xrtHttp1BodyPlanFail(
				pPlan, XHTTP1_ERROR_REQUEST_TRANSFER_ENCODING,
				XERR_PROTOCOL, "plan-http1-request-body",
				"request transfer coding is not terminated by chunked"
			);
		}
		pPlan->Mode = XHTTP1_BODY_CHUNKED;
		return true;
	}
	if ( (pHead->Flags & (uint32)XHTTP1_CONTENT_LENGTH) != 0 ) {
		pPlan->Mode = XHTTP1_BODY_FIXED;
		pPlan->Length = pHead->ContentLength;
		return true;
	}
	pPlan->Mode = XHTTP1_BODY_NONE;
	return true;
}



/* 按请求方法与响应状态生成 RFC 9112 Body Plan。 */
XRT_API bool xrtHttp1ResponseBodyPlan(
	const xhttp1head* pHead,
	xstrview RequestMethod,
	xhttp1bodyplan* pPlan
)
{
	if ( !__xrtHttp1BodyHeadValid(pHead, XHTTP_RESPONSE) ||
		!xrtHttpTokenValid(RequestMethod) ||
		(pPlan == NULL) ) {
		return __xrtHttp1BodyPlanFail(
			pPlan, XHTTP1_ERROR_ARGUMENT, XERR_ARGUMENT,
			"plan-http1-response-body", "response Header, method or Body Plan is invalid"
		);
	}
	memset(pPlan, 0, sizeof(*pPlan));
	if ( pHead->Status == 101 ) {
		if ( (pHead->Flags & (uint32)XHTTP1_UPGRADE) == 0 ) {
			return __xrtHttp1BodyPlanFail(
				pPlan, XHTTP1_ERROR_UPGRADE, XERR_PROTOCOL,
				"plan-http1-response-body",
				"101 response does not contain a valid Upgrade handshake"
			);
		}
		pPlan->Mode = XHTTP1_BODY_TUNNEL;
		return true;
	}
	if ( xrtHttpMethodEqual(
		RequestMethod, XRT_STR_LITERAL("CONNECT")
	) &&
		(pHead->Status >= 200) && (pHead->Status < 300) ) {
		pPlan->Mode = XHTTP1_BODY_TUNNEL;
		return true;
	}
	if ( xrtHttpMethodEqual(
		RequestMethod, XRT_STR_LITERAL("HEAD")
	) ) {
		pPlan->Mode = XHTTP1_BODY_NONE;
		return true;
	}
	if ( (pHead->Status < 200) ||
		(pHead->Status == 204) ||
		(pHead->Status == 304) ) {
		pPlan->Mode = XHTTP1_BODY_NONE;
		return true;
	}
	if ( ((pHead->Flags & (uint32)XHTTP1_TRANSFER_ENCODING) != 0) &&
		((pHead->Flags & (uint32)XHTTP1_CONTENT_LENGTH) != 0) ) {
		return __xrtHttp1BodyPlanFail(
			pPlan, XHTTP1_ERROR_TRANSFER_LENGTH, XERR_PROTOCOL,
			"plan-http1-response-body",
			"response contains both Transfer-Encoding and Content-Length"
		);
	}
	if ( (pHead->Flags & (uint32)XHTTP1_TRANSFER_ENCODING) != 0 ) {
		pPlan->Mode = ((pHead->Flags & (uint32)XHTTP1_CHUNKED) != 0) ?
			XHTTP1_BODY_CHUNKED : XHTTP1_BODY_CLOSE;
		return true;
	}
	if ( (pHead->Flags & (uint32)XHTTP1_CONTENT_LENGTH) != 0 ) {
		pPlan->Mode = XHTTP1_BODY_FIXED;
		pPlan->Length = pHead->ContentLength;
		return true;
	}
	pPlan->Mode = XHTTP1_BODY_CLOSE;
	return true;
}



/* 初始化无分配 Body Reader。 */
XRT_API bool xrtHttp1BodyInit(
	xhttp1body* pBody,
	const xhttp1bodyplan* pPlan,
	xhttpfield* pTrailers,
	size_t iTrailerCapacity,
	const xhttp1bodylimits* pLimits
)
{
	xhttp1bodylimits Limits;

	if ( (pBody == NULL) || (pPlan == NULL) ||
		((pTrailers == NULL) && (iTrailerCapacity != 0)) ||
		(pPlan->Mode < XHTTP1_BODY_NONE) ||
		(pPlan->Mode > XHTTP1_BODY_TUNNEL) ||
		!__xrtHttp1BodyLimitsResolve(pLimits, &Limits) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pBody, 0, sizeof(*pBody));
	pBody->Mode = pPlan->Mode;
	pBody->Trailers = pTrailers;
	pBody->TrailerCapacity = iTrailerCapacity;
	pBody->Limits = Limits;
	if ( (pPlan->Mode == XHTTP1_BODY_FIXED) &&
		(pPlan->Length > Limits.MaxBody) ) {
		pBody->State = XRT_HTTP1_BODY_FAILED;
		(void)__xrtHttp1Fail(
			NULL, NULL, XHTTP1_ERROR_BODY_TOO_LARGE, 0, 0,
			XERR_RANGE, "init-http1-body", "fixed body exceeds its limit"
		);
		return false;
	}
	if ( pPlan->Mode == XHTTP1_BODY_FIXED ) {
		pBody->Remaining = pPlan->Length;
		pBody->State = (pPlan->Length == 0) ?
			XRT_HTTP1_BODY_COMPLETE : XRT_HTTP1_BODY_FIXED_DATA;
	} else if ( pPlan->Mode == XHTTP1_BODY_CHUNKED ) {
		pBody->State = XRT_HTTP1_BODY_CHUNK_SIZE_START;
	} else if ( pPlan->Mode == XHTTP1_BODY_CLOSE ) {
		pBody->State = XRT_HTTP1_BODY_CLOSE_DATA;
	} else {
		pBody->State = XRT_HTTP1_BODY_COMPLETE;
	}
	return true;
}



/* 在 trailer 描述符不足后替换存储，不改变已经解码的正文进度。 */
XRT_API bool xrtHttp1BodyTrailers(
	xhttp1body* pBody,
	xhttpfield* pTrailers,
	size_t iCapacity
)
{
	if ( (pBody == NULL) ||
		((pTrailers == NULL) && (iCapacity != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pBody->State != XRT_HTTP1_BODY_TRAILERS ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	pBody->Trailers = pTrailers;
	pBody->TrailerCapacity = iCapacity;
	pBody->TrailerCount = 0;
	return true;
}



/* 严格解析独立 trailer 区并返回借用字段。 */
XRT_API xhttp1status xrtHttp1TrailersParse(
	xbytesview Input,
	xhttpfield* pFields,
	size_t iCapacity,
	const xhttp1bodylimits* pLimits,
	size_t* pBytes,
	size_t* pCount,
	xhttp1errorinfo* pError
)
{
	return __xrtHttp1TrailersParse(
		Input, pFields, iCapacity, pLimits,
		pBytes, pCount, pError, 0
	);
}



/* 推进 fixed、chunked 或 close-delimited 正文状态机。 */
XRT_API xhttp1bodystatus xrtHttp1BodyRead(
	xhttp1body* pBody,
	xbytesview Input,
	bool bEnd,
	size_t* pConsumed,
	xbytesview* pData,
	xhttp1errorinfo* pError
)
{
	size_t iPosition = 0;

	if ( pConsumed != NULL ) {
		*pConsumed = 0;
	}
	if ( pData != NULL ) {
		memset(pData, 0, sizeof(*pData));
	}
	__xrtHttp1BodyErrorClear(pError);
	if ( (pBody == NULL) ||
		((Input.Data == NULL) && (Input.Size != 0)) ||
		(pConsumed == NULL) || (pData == NULL) ) {
		return __xrtHttp1BodyFail(
			pBody, pError, XHTTP1_ERROR_ARGUMENT, 0, 0,
			XERR_ARGUMENT, "read-http1-body",
			"Body Reader, input or output is invalid"
		);
	}
	if ( pBody->State == XRT_HTTP1_BODY_FAILED ) {
		__xrtErrorSetInvalidState();
		return XHTTP1_BODY_ERROR;
	}
	if ( pBody->State == XRT_HTTP1_BODY_COMPLETE ) {
		return XHTTP1_BODY_DONE;
	}

	if ( pBody->State == XRT_HTTP1_BODY_FIXED_DATA ) {
		size_t iTake = Input.Size;

		if ( (uint64)iTake > pBody->Remaining ) {
			iTake = (size_t)pBody->Remaining;
		}
		if ( iTake != 0 ) {
			pData->Data = Input.Data;
			pData->Size = iTake;
			pBody->Remaining -= (uint64)iTake;
			pBody->Received += (uint64)iTake;
			pBody->WireBytes += (uint64)iTake;
			*pConsumed = iTake;
			if ( pBody->Remaining == 0 ) {
				pBody->State = XRT_HTTP1_BODY_COMPLETE;
			}
			return XHTTP1_BODY_DATA;
		}
		if ( bEnd ) {
			return __xrtHttp1BodyFail(
				pBody, pError, XHTTP1_ERROR_BODY_INCOMPLETE,
				pBody->WireBytes, 0, XERR_PROTOCOL,
				"read-http1-body", "fixed body ended before Content-Length"
			);
		}
		return XHTTP1_BODY_MORE;
	}

	if ( pBody->State == XRT_HTTP1_BODY_CLOSE_DATA ) {
		if ( Input.Size != 0 ) {
			if ( (pBody->Received > pBody->Limits.MaxBody) ||
				((uint64)Input.Size >
				 (pBody->Limits.MaxBody - pBody->Received)) ) {
				return __xrtHttp1BodyFail(
					pBody, pError, XHTTP1_ERROR_BODY_TOO_LARGE,
					pBody->WireBytes, 0, XERR_RANGE,
					"read-http1-body", "close-delimited body exceeds its limit"
				);
			}
			*pData = Input;
			*pConsumed = Input.Size;
			pBody->Received += (uint64)Input.Size;
			pBody->WireBytes += (uint64)Input.Size;
			return XHTTP1_BODY_DATA;
		}
		if ( bEnd ) {
			pBody->State = XRT_HTTP1_BODY_COMPLETE;
			return XHTTP1_BODY_DONE;
		}
		return XHTTP1_BODY_MORE;
	}

	while ( iPosition < Input.Size ) {
		if ( pBody->State == XRT_HTTP1_BODY_CHUNK_DATA ) {
			size_t iTake = Input.Size - iPosition;

			if ( (uint64)iTake > pBody->Remaining ) {
				iTake = (size_t)pBody->Remaining;
			}
			pData->Data = Input.Data + iPosition;
			pData->Size = iTake;
			pBody->Remaining -= (uint64)iTake;
			pBody->Received += (uint64)iTake;
			pBody->WireBytes += (uint64)iTake;
			iPosition += iTake;
			*pConsumed = iPosition;
			if ( pBody->Remaining == 0 ) {
				pBody->State = XRT_HTTP1_BODY_CHUNK_DATA_CR;
			}
			return XHTTP1_BODY_DATA;
		}

		if ( pBody->State == XRT_HTTP1_BODY_CHUNK_DATA_CR ) {
			if ( Input.Data[iPosition] != (unsigned char)'\r' ) {
				*pConsumed = iPosition;
				return __xrtHttp1BodyFail(
					pBody, pError, XHTTP1_ERROR_CHUNK_TERMINATOR,
					pBody->WireBytes, 0, XERR_PROTOCOL,
					"read-http1-body", "chunk data is not followed by CRLF"
				);
			}
			pBody->State = XRT_HTTP1_BODY_CHUNK_DATA_LF;
			pBody->WireBytes++;
			iPosition++;
			continue;
		}

		if ( pBody->State == XRT_HTTP1_BODY_CHUNK_DATA_LF ) {
			if ( Input.Data[iPosition] != (unsigned char)'\n' ) {
				*pConsumed = iPosition;
				return __xrtHttp1BodyFail(
					pBody, pError, XHTTP1_ERROR_CHUNK_TERMINATOR,
					pBody->WireBytes, 0, XERR_PROTOCOL,
					"read-http1-body", "chunk data CR is not followed by LF"
				);
			}
			pBody->ChunkSize = 0;
			pBody->ChunkLineBytes = 0;
			pBody->State = XRT_HTTP1_BODY_CHUNK_SIZE_START;
			pBody->WireBytes++;
			iPosition++;
			continue;
		}

		if ( pBody->State == XRT_HTTP1_BODY_TRAILERS ) {
			xhttp1status Status;
			size_t iBytes = 0;
			size_t iCount = 0;

			Status = __xrtHttp1TrailersParse(
				(xbytesview){ Input.Data + iPosition, Input.Size - iPosition },
				pBody->Trailers,
				pBody->TrailerCapacity,
				&pBody->Limits,
				&iBytes,
				&iCount,
				pError,
				pBody->WireBytes
			);
			pBody->TrailerCount = iCount;
			if ( Status == XHTTP1_ERROR ) {
				*pConsumed = iPosition;
				pBody->State = XRT_HTTP1_BODY_FAILED;
				return XHTTP1_BODY_ERROR;
			}
			if ( Status == XHTTP1_FIELDS ) {
				*pConsumed = iPosition;
				return XHTTP1_BODY_FIELDS;
			}
			if ( Status == XHTTP1_MORE ) {
				*pConsumed = iPosition;
				if ( bEnd ) {
					return __xrtHttp1BodyFail(
						pBody, pError, XHTTP1_ERROR_BODY_INCOMPLETE,
						pBody->WireBytes, 0, XERR_PROTOCOL,
						"read-http1-body", "chunked body ended before trailers"
					);
				}
				return XHTTP1_BODY_MORE;
			}
			pBody->WireBytes += (uint64)iBytes;
			iPosition += iBytes;
			pBody->State = XRT_HTTP1_BODY_COMPLETE;
			*pConsumed = iPosition;
			return XHTTP1_BODY_DONE;
		}

		if ( !__xrtHttp1ChunkSyntax(
			pBody, Input.Data[iPosition], pError
		) ) {
			*pConsumed = iPosition;
			return XHTTP1_BODY_ERROR;
		}
		pBody->WireBytes++;
		iPosition++;
	}

	*pConsumed = iPosition;
	if ( bEnd ) {
		return __xrtHttp1BodyFail(
			pBody, pError, XHTTP1_ERROR_BODY_INCOMPLETE,
			pBody->WireBytes, 0, XERR_PROTOCOL,
			"read-http1-body", "chunked body ended before the last chunk"
		);
	}
	return XHTTP1_BODY_MORE;
}



/* 判断 Reader 是否已经完整消费正文与 trailer。 */
XRT_API bool xrtHttp1BodyDone(const xhttp1body* pBody)
{
	if ( pBody == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return pBody->State == XRT_HTTP1_BODY_COMPLETE;
}

#endif
