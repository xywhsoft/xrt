#include "../internal/xrt_http.h"



#if defined(XRT_FEATURE_HTTP1_MESSAGE)

/* 验证完整消息及其调用方描述符存储仍满足初始化契约。 */
static bool __xrtHttp1MessageValid(const xhttp1message* pMessage)
{
	return (pMessage != NULL) &&
		((pMessage->Head.Fields != NULL) ||
		 (pMessage->Head.FieldCapacity == 0)) &&
		((pMessage->Trailers != NULL) ||
		 (pMessage->TrailerCapacity == 0));
}



/* 保留描述符数组并清空上一条消息的全部借用视图与计数。 */
static void __xrtHttp1MessageReset(xhttp1message* pMessage)
{
	xhttpfield* pFields = pMessage->Head.Fields;
	size_t iFieldCapacity = pMessage->Head.FieldCapacity;
	xhttpfield* pTrailers = pMessage->Trailers;
	size_t iTrailerCapacity = pMessage->TrailerCapacity;

	memset(pMessage, 0, sizeof(*pMessage));
	xrtHttp1HeadInit(&pMessage->Head, pFields, iFieldCapacity);
	pMessage->Trailers = pTrailers;
	pMessage->TrailerCapacity = iTrailerCapacity;
}



/* 把最近一次 HTTP/1 全局错误同步到轻量解析位置信息。 */
static void __xrtHttp1MessageLastError(
	xhttp1errorinfo* pError,
	size_t iOffset
)
{
	int32 iCode;

	if ( pError == NULL ) {
		return;
	}
	memset(pError, 0, sizeof(*pError));
	iCode = xrtErrorCode(xrtGetError());
	pError->Code = (iCode > 0) ?
		(xhttp1error)iCode : XHTTP1_ERROR_ARGUMENT;
	pError->Offset = iOffset;
}



/* 把正文相对偏移转换成完整消息偏移，溢出时饱和到 SIZE_MAX。 */
static void __xrtHttp1MessageBodyOffset(
	xhttp1errorinfo* pError,
	size_t iHeadBytes
)
{
	if ( pError == NULL ) {
		return;
	}
	if ( pError->Offset > (SIZE_MAX - iHeadBytes) ) {
		pError->Offset = SIZE_MAX;
	} else {
		pError->Offset += iHeadBytes;
	}
}



/* 组合 Head 与 Body Reader，扫描连续输入中的第一条完整消息。 */
static xhttp1status __xrtHttp1MessageParse(
	xbytesview Input,
	bool bEnd,
	xhttpkind Kind,
	xstrview RequestMethod,
	xhttp1message* pMessage,
	const xhttp1limits* pHeadLimits,
	const xhttp1bodylimits* pBodyLimits,
	xhttp1errorinfo* pError
)
{
	xhttp1bodystatus BodyStatus;
	xhttp1status Status;
	xhttp1body Body;
	xbytesview Data;
	size_t iConsumed;
	size_t iPosition;

	if ( pError != NULL ) {
		memset(pError, 0, sizeof(*pError));
	}
	if ( !__xrtHttp1MessageValid(pMessage) ) {
		(void)__xrtHttp1Fail(
			NULL, pError, XHTTP1_ERROR_ARGUMENT, 0, 0,
			XERR_ARGUMENT, "parse-http1-message",
			"HTTP/1 Message or descriptor storage is invalid"
		);
		return XHTTP1_ERROR;
	}

	__xrtHttp1MessageReset(pMessage);
	Status = (Kind == XHTTP_REQUEST) ?
		xrtHttp1RequestParse(
			Input, &pMessage->Head, pHeadLimits, pError
		) :
		xrtHttp1ResponseParse(
			Input, &pMessage->Head, pHeadLimits, pError
		);
	if ( (Status == XHTTP1_MORE) && bEnd ) {
		(void)__xrtHttp1Fail(
			NULL, pError, XHTTP1_ERROR_HEAD_INCOMPLETE,
			Input.Size, 0, XERR_PROTOCOL,
			"parse-http1-message",
			"HTTP/1 Header ended before its final empty line"
		);
		__xrtHttp1MessageReset(pMessage);
		return XHTTP1_ERROR;
	}
	if ( Status != XHTTP1_READY ) {
		return Status;
	}

	if ( Kind == XHTTP_REQUEST ) {
		if ( !xrtHttp1RequestBodyPlan(
			&pMessage->Head, &pMessage->Plan
		) ) {
			__xrtHttp1MessageLastError(pError, 0);
			__xrtHttp1MessageReset(pMessage);
			return XHTTP1_ERROR;
		}
	} else if ( !xrtHttp1ResponseBodyPlan(
		&pMessage->Head, RequestMethod, &pMessage->Plan
	) ) {
		__xrtHttp1MessageLastError(pError, 0);
		__xrtHttp1MessageReset(pMessage);
		return XHTTP1_ERROR;
	}

	if ( !xrtHttp1BodyInit(
		&Body,
		&pMessage->Plan,
		pMessage->Trailers,
		pMessage->TrailerCapacity,
		pBodyLimits
	) ) {
		__xrtHttp1MessageLastError(pError, pMessage->Head.Bytes);
		__xrtHttp1MessageReset(pMessage);
		return XHTTP1_ERROR;
	}
	pMessage->Limits = Body.Limits;
	iPosition = pMessage->Head.Bytes;

	for ( ;; ) {
		BodyStatus = xrtHttp1BodyRead(
			&Body,
			(xbytesview){
				Input.Data + iPosition,
				Input.Size - iPosition
			},
			bEnd,
			&iConsumed,
			&Data,
			pError
		);
		iPosition += iConsumed;
		pMessage->TrailerCount = Body.TrailerCount;

		if ( BodyStatus == XHTTP1_BODY_DATA ) {
			if ( (Data.Size == 0) || (iConsumed == 0) ) {
				(void)__xrtHttp1Fail(
					NULL, pError, XHTTP1_ERROR_BODY_INCOMPLETE,
					iPosition, 0, XERR_STATE,
					"parse-http1-message",
					"HTTP/1 Body Reader made no progress"
				);
				__xrtHttp1MessageReset(pMessage);
				return XHTTP1_ERROR;
			}
			continue;
		}
		if ( BodyStatus == XHTTP1_BODY_MORE ) {
			return XHTTP1_MORE;
		}
		if ( BodyStatus == XHTTP1_BODY_FIELDS ) {
			return XHTTP1_FIELDS;
		}
		if ( BodyStatus == XHTTP1_BODY_ERROR ) {
			__xrtHttp1MessageBodyOffset(pError, pMessage->Head.Bytes);
			__xrtHttp1MessageReset(pMessage);
			return XHTTP1_ERROR;
		}
		break;
	}

	if ( Body.Received > (uint64)SIZE_MAX ) {
		(void)__xrtHttp1Fail(
			NULL, pError, XHTTP1_ERROR_BODY_TOO_LARGE,
			pMessage->Head.Bytes, 0, XERR_RANGE,
			"parse-http1-message",
			"HTTP/1 decoded Body does not fit in memory"
		);
		__xrtHttp1MessageReset(pMessage);
		return XHTTP1_ERROR;
	}
	pMessage->Wire = (xbytesview){ Input.Data, iPosition };
	pMessage->BodyBytes = (size_t)Body.Received;
	return XHTTP1_READY;
}



/* 判断完整消息已经成功扫描且全部借用范围仍自洽。 */
static bool __xrtHttp1MessageReady(const xhttp1message* pMessage)
{
	size_t iWireBody;

	if ( !__xrtHttp1MessageValid(pMessage) ||
		(pMessage->Wire.Data == NULL) ||
		(pMessage->Head.Bytes == 0) ||
		(pMessage->Wire.Size < pMessage->Head.Bytes) ) {
		return false;
	}
	iWireBody = pMessage->Wire.Size - pMessage->Head.Bytes;
	if ( (pMessage->Plan.Mode == XHTTP1_BODY_NONE) ||
		(pMessage->Plan.Mode == XHTTP1_BODY_TUNNEL) ) {
		return (pMessage->BodyBytes == 0) && (iWireBody == 0);
	}
	if ( (pMessage->Plan.Mode == XHTTP1_BODY_FIXED) ||
		(pMessage->Plan.Mode == XHTTP1_BODY_CLOSE) ) {
		return pMessage->BodyBytes == iWireBody;
	}
	if ( pMessage->Plan.Mode == XHTTP1_BODY_CHUNKED ) {
		return pMessage->BodyBytes <= iWireBody;
	}
	return false;
}



/* 初始化借用调用方描述符数组的空完整消息。 */
XRT_API void xrtHttp1MessageInit(
	xhttp1message* pMessage,
	xhttpfield* pFields,
	size_t iFieldCapacity,
	xhttpfield* pTrailers,
	size_t iTrailerCapacity
)
{
	if ( (pMessage == NULL) ||
		((pFields == NULL) && (iFieldCapacity != 0)) ||
		((pTrailers == NULL) && (iTrailerCapacity != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(pMessage, 0, sizeof(*pMessage));
	xrtHttp1HeadInit(&pMessage->Head, pFields, iFieldCapacity);
	pMessage->Trailers = pTrailers;
	pMessage->TrailerCapacity = iTrailerCapacity;
}



/* 扫描连续输入中的第一条完整 HTTP/1 请求。 */
XRT_API xhttp1status xrtHttp1RequestMessageParse(
	xbytesview Input,
	bool bEnd,
	xhttp1message* pMessage,
	const xhttp1limits* pHeadLimits,
	const xhttp1bodylimits* pBodyLimits,
	xhttp1errorinfo* pError
)
{
	return __xrtHttp1MessageParse(
		Input, bEnd, XHTTP_REQUEST, (xstrview){ NULL, 0 },
		pMessage, pHeadLimits, pBodyLimits, pError
	);
}



/* 扫描连续输入中的第一条完整 HTTP/1 响应。 */
XRT_API xhttp1status xrtHttp1ResponseMessageParse(
	xbytesview Input,
	bool bEnd,
	xstrview RequestMethod,
	xhttp1message* pMessage,
	const xhttp1limits* pHeadLimits,
	const xhttp1bodylimits* pBodyLimits,
	xhttp1errorinfo* pError
)
{
	return __xrtHttp1MessageParse(
		Input, bEnd, XHTTP_RESPONSE, RequestMethod,
		pMessage, pHeadLimits, pBodyLimits, pError
	);
}



/* 返回固定或关闭定界消息中无需重排的借用正文。 */
XRT_API xbytesview xrtHttp1MessageBodyView(const xhttp1message* pMessage)
{
	if ( !__xrtHttp1MessageReady(pMessage) ) {
		__xrtErrorSetInvalidArgument();
		return (xbytesview){ NULL, 0 };
	}
	if ( (pMessage->BodyBytes == 0) ||
		(pMessage->Plan.Mode == XHTTP1_BODY_CHUNKED) ) {
		return (xbytesview){ NULL, 0 };
	}
	return (xbytesview){
		pMessage->Wire.Data + pMessage->Head.Bytes,
		pMessage->BodyBytes
	};
}



/* 重新驱动同一 Body Plan，把正文原子复制为连续数据。 */
XRT_API bool xrtHttp1MessageBodyCopy(
	const xhttp1message* pMessage,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xstrview Parts[1];
	xhttp1bodystatus Status;
	xhttp1body Body;
	xbytesview Data;
	bytes pBytes = (bytes)pOutput;
	size_t iConsumed;
	size_t iCopied = 0;
	size_t iPosition;

	if ( pSize != NULL ) {
		*pSize = 0;
	}
	if ( !__xrtHttp1MessageReady(pMessage) || (pSize == NULL) ) {
		(void)__xrtHttp1Fail(
			NULL, NULL, XHTTP1_ERROR_ARGUMENT, 0, 0,
			XERR_ARGUMENT, "copy-http1-message-body",
			"HTTP/1 Message or output size is invalid"
		);
		return false;
	}
	Parts[0] = (xstrview){
		(cstr)pMessage->Wire.Data,
		pMessage->Wire.Size
	};
	if ( !__xrtHttp1WriteOutputValid(
		Parts, 1, NULL, 0,
		pOutput, iCapacity, pMessage->BodyBytes, pSize,
		"copy-http1-message-body"
	) ) {
		return false;
	}
	if ( pOutput == NULL ) {
		return true;
	}
	if ( pMessage->BodyBytes == 0 ) {
		return true;
	}
	if ( pMessage->Plan.Mode != XHTTP1_BODY_CHUNKED ) {
		memcpy(
			pBytes,
			pMessage->Wire.Data + pMessage->Head.Bytes,
			pMessage->BodyBytes
		);
		*pSize = pMessage->BodyBytes;
		return true;
	}
	if ( !xrtHttp1BodyInit(
		&Body,
		&pMessage->Plan,
		pMessage->Trailers,
		pMessage->TrailerCapacity,
		&pMessage->Limits
	) ) {
		return false;
	}
	iPosition = pMessage->Head.Bytes;

	for ( ;; ) {
		Status = xrtHttp1BodyRead(
			&Body,
			(xbytesview){
				pMessage->Wire.Data + iPosition,
				pMessage->Wire.Size - iPosition
			},
			true,
			&iConsumed,
			&Data,
			NULL
		);
		iPosition += iConsumed;
		if ( Status == XHTTP1_BODY_DATA ) {
			if ( (Data.Size > (pMessage->BodyBytes - iCopied)) ||
				(Data.Size == 0) || (iConsumed == 0) ) {
				break;
			}
			memcpy(pBytes + iCopied, Data.Data, Data.Size);
			iCopied += Data.Size;
			continue;
		}
		if ( Status == XHTTP1_BODY_DONE ) {
			if ( (iCopied == pMessage->BodyBytes) &&
				(iPosition == pMessage->Wire.Size) ) {
				*pSize = iCopied;
				return true;
			}
			break;
		}
		break;
	}

	(void)__xrtHttp1Fail(
		NULL, NULL, XHTTP1_ERROR_BODY_INCOMPLETE,
		iPosition, 0, XERR_STATE,
		"copy-http1-message-body",
		"HTTP/1 Message Body no longer matches its parsed wire data"
	);
	return false;
}

#endif
