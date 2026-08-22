#include "../internal/xrt_http_server.h"
#include "../internal/xrt_http.h"



#if defined(XRT_FEATURE_HTTP_SERVER_REQUEST)

/* 建立请求辅助层错误并保留借用原因链。 */
void __xrtHttpServerRequestSetError(
	xerrkind Kind,
	xhttpserverrequesterror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	__xrtErrorSetDetail(
		Kind,
		"xrt.http.server.request",
		(int32)Code,
		sOperation,
		sMessage,
		pCause
	);
}



/* 取得当前错误所有权，再用稳定请求错误域包裹并恢复失败回退。 */
void __xrtHttpServerRequestWrapError(
	xerrkind DefaultKind,
	xhttpserverrequesterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xrtErrorWrapDetail(
		DefaultKind,
		"xrt.http.server.request",
		(int32)Code,
		sOperation,
		sMessage
	);
}



/* 验证调用方输出不会破坏不透明请求拥有的任何内存。 */
bool __xrtHttpServerRequestOutputValid(
	const xhttpserverrequest* pRequest,
	const void* pOutput,
	size_t iSize
)
{
	if ( !__xrtRangeValid(pOutput, iSize) ) {
		return false;
	}
	if ( (pRequest == NULL) || (iSize == 0) ) {
		return true;
	}
	return !__xrtRangesOverlap(
		pOutput, iSize, pRequest, sizeof(*pRequest)
	) && !__xrtRangesOverlap(
		pOutput, iSize,
		pRequest->Method.Data, pRequest->Method.Size
	) && !__xrtRangesOverlap(
		pOutput, iSize,
		pRequest->Target.Data, pRequest->Target.Size
	) && !__xrtHttpFieldArrayOverlap(
		pRequest->Fields, pRequest->FieldCount,
		pOutput, iSize
	) && !__xrtRangesOverlap(
		pOutput, iSize,
		pRequest->Body, pRequest->BodySize
	) && !__xrtHttpFieldArrayOverlap(
		pRequest->Trailers, pRequest->TrailerCount,
		pOutput, iSize
	);
}



/* 表单辅助只接受完成且没有采用流式或丢弃策略的请求正文。 */
bool __xrtHttpServerRequestBufferedBody(
	const xhttpserverrequest* pRequest,
	xbytesview* pBody,
	cstr sOperation
)
{
	xbytesview Body;

	if ( (pRequest == NULL) ||
		!__xrtHttpServerRequestOutputValid(
			pRequest, pBody, sizeof(Body)
		) ) {
		__xrtHttpServerRequestSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_REQUEST_ERROR_ARGUMENT,
			sOperation,
			"HTTP server request or body output is invalid",
			NULL
		);
		return false;
	}
	if ( (pRequest->Flags &
		 XHTTP_SERVER_REQUEST_COMPLETE) == 0 ) {
		__xrtHttpServerRequestSetError(
			XERR_STATE,
			XHTTP_SERVER_REQUEST_ERROR_STATE,
			sOperation,
			"HTTP form parsing requires a complete request",
			NULL
		);
		return false;
	}
	if ( (pRequest->Flags &
		 (XHTTP_SERVER_REQUEST_STREAMED |
		  XHTTP_SERVER_REQUEST_DISCARDED)) != 0 ) {
		__xrtHttpServerRequestSetError(
			XERR_STATE,
			XHTTP_SERVER_REQUEST_ERROR_BODY,
			sOperation,
			"HTTP form parsing requires a buffered request body",
			NULL
		);
		return false;
	}
	Body = (xbytesview){
		pRequest->Body,
		pRequest->BodySize
	};
	memcpy(pBody, &Body, sizeof(Body));
	return true;
}



/* 安全累计紧凑请求块所需的字节数。 */
static bool __xrtHttpServerRequestSizeAdd(
	size_t* pTotal,
	size_t iValue
)
{
	if ( (pTotal == NULL) ||
		(*pTotal > (SIZE_MAX - iValue)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	*pTotal += iValue;
	return true;
}



/* 统计请求行和全部 Header 文本的精确拥有字节数。 */
static bool __xrtHttpServerRequestTextSize(
	const xhttp1head* pHead,
	size_t* pSize
)
{
	size_t iSize = 0;
	size_t i;

	if ( !__xrtHttpViewValid(pHead->Method) ||
		!__xrtHttpViewValid(pHead->Target) ||
		!__xrtHttpFieldArrayValid(
			pHead->Fields,
			pHead->FieldCount
		) ) {
		return false;
	}
	if ( !__xrtHttpServerRequestSizeAdd(
		&iSize, pHead->Method.Size
	) || !__xrtHttpServerRequestSizeAdd(
		&iSize, pHead->Target.Size
	) ) {
		return false;
	}
	for ( i = 0; i < pHead->FieldCount; i++ ) {
		if ( !__xrtHttpServerRequestSizeAdd(
			&iSize, pHead->Fields[i].Name.Size
		) || !__xrtHttpServerRequestSizeAdd(
			&iSize, pHead->Fields[i].Value.Size
		) ) {
			return false;
		}
	}
	*pSize = iSize;
	return true;
}



/* 依次复制文本并返回位于紧凑块内的借用视图。 */
static xstrview __xrtHttpServerRequestTextCopy(
	uint8** ppOutput,
	xstrview Text
)
{
	xstrview Copy = {
		(cstr)*ppOutput,
		Text.Size
	};

	if ( Text.Size != 0 ) {
		memcpy(*ppOutput, Text.Data, Text.Size);
		*ppOutput += Text.Size;
	}
	return Copy;
}



/* 从解析结果建立单块请求头快照。 */
xhttpserverrequest* __xrtHttpServerRequestCreate(
	const xhttp1head* pHead,
	const xhttp1bodyplan* pPlan,
	uint32 iFlags
)
{
	xhttpserverrequest* pRequest;
	xhttpfield* pFields;
	uint8* pText;
	size_t iFieldBytes;
	size_t iTextBytes;
	size_t iTotal;
	size_t i;

	if ( (pHead == NULL) || (pPlan == NULL) ||
		(pHead->Kind != XHTTP_REQUEST) ||
		((pHead->Fields == NULL) &&
		 (pHead->FieldCount != 0)) ||
		!__xrtHttpServerRequestTextSize(
			pHead, &iTextBytes
		) ) {
		if ( (pHead == NULL) || (pPlan == NULL) ) {
			__xrtErrorSetInvalidArgument();
		}
		return NULL;
	}
	if ( pHead->FieldCount >
		(SIZE_MAX / sizeof(xhttpfield)) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iFieldBytes = pHead->FieldCount * sizeof(xhttpfield);
	iTotal = sizeof(*pRequest);
	if ( !__xrtHttpServerRequestSizeAdd(
		&iTotal, iFieldBytes
	) || !__xrtHttpServerRequestSizeAdd(
		&iTotal, iTextBytes
	) ) {
		return NULL;
	}
	pRequest = (xhttpserverrequest*)xrtCalloc(1, iTotal);
	if ( pRequest == NULL ) {
		return NULL;
	}
	pRequest->RefCount = 1;
	pRequest->Version = pHead->Version;
	pRequest->Flags = iFlags;
	pRequest->Plan = *pPlan;
	pRequest->FieldCount = pHead->FieldCount;
	pFields = (xhttpfield*)(pRequest + 1);
	pText = (uint8*)pFields + iFieldBytes;
	pRequest->Fields = pFields;
	pRequest->Method = __xrtHttpServerRequestTextCopy(
		&pText, pHead->Method
	);
	pRequest->Target = __xrtHttpServerRequestTextCopy(
		&pText, pHead->Target
	);
	for ( i = 0; i < pHead->FieldCount; i++ ) {
		pFields[i].Name = __xrtHttpServerRequestTextCopy(
			&pText, pHead->Fields[i].Name
		);
		pFields[i].Value = __xrtHttpServerRequestTextCopy(
			&pText, pHead->Fields[i].Value
		);
	}
	return pRequest;
}



/* 按实际正文增长缓冲，不为无正文请求预留固定块。 */
static bool __xrtHttpServerRequestReserveBody(
	xhttpserverrequest* pRequest,
	size_t iRequired
)
{
	size_t iCapacity;
	uint8* pBody;

	if ( iRequired <= pRequest->BodyCapacity ) {
		return true;
	}
	iCapacity = pRequest->BodyCapacity;
	if ( iCapacity == 0 ) {
		iCapacity = iRequired;
	}
	while ( iCapacity < iRequired ) {
		size_t iNext = iCapacity > (SIZE_MAX / 2u) ?
			iRequired : (iCapacity * 2u);

		if ( iNext <= iCapacity ) {
			iCapacity = iRequired;
			break;
		}
		iCapacity = iNext;
	}
	pBody = (uint8*)xrtRealloc(
		pRequest->Body, iCapacity
	);
	if ( pBody == NULL ) {
		return false;
	}
	pRequest->Body = pBody;
	pRequest->BodyCapacity = iCapacity;
	return true;
}



/* 复制并累计一段缓冲正文。 */
bool __xrtHttpServerRequestAppendBody(
	xhttpserverrequest* pRequest,
	xbytesview Data
)
{
	size_t iRequired;

	if ( (pRequest == NULL) ||
		((Data.Data == NULL) && (Data.Size != 0)) ||
		((pRequest->Flags &
		  XHTTP_SERVER_REQUEST_STREAMED) != 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( Data.Size == 0 ) {
		return true;
	}
	if ( (pRequest->BodySize > (SIZE_MAX - Data.Size)) ||
		(pRequest->BodyBytes >
		 (UINT64_MAX - (uint64)Data.Size)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iRequired = pRequest->BodySize + Data.Size;
	if ( !__xrtHttpServerRequestReserveBody(
		pRequest, iRequired
	) ) {
		return false;
	}
	memcpy(
		pRequest->Body + pRequest->BodySize,
		Data.Data,
		Data.Size
	);
	pRequest->BodySize = iRequired;
	pRequest->BodyBytes += (uint64)Data.Size;
	return true;
}



/* 累计已经流式交付的正文。 */
bool __xrtHttpServerRequestDeliverBody(
	xhttpserverrequest* pRequest,
	uint64 iBytes
)
{
	if ( (pRequest == NULL) ||
		((pRequest->Flags &
		  XHTTP_SERVER_REQUEST_STREAMED) == 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pRequest->BodyBytes > (UINT64_MAX - iBytes) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	pRequest->BodyBytes += iBytes;
	return true;
}



/* 统计 Trailer 单块中的描述符与文本字节。 */
static bool __xrtHttpServerRequestTrailerSize(
	const xhttpfield* pTrailers,
	size_t iCount,
	size_t* pFieldBytes,
	size_t* pTextBytes
)
{
	size_t iText = 0;
	size_t i;

	if ( !__xrtHttpFieldArrayValid(pTrailers, iCount) ) {
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		if ( !__xrtHttpServerRequestSizeAdd(
			&iText, pTrailers[i].Name.Size
		) || !__xrtHttpServerRequestSizeAdd(
			&iText, pTrailers[i].Value.Size
		) ) {
			return false;
		}
	}
	*pFieldBytes = iCount * sizeof(xhttpfield);
	*pTextBytes = iText;
	return true;
}



/* 复制完整 Trailer 集合并在成功后替换旧集合。 */
bool __xrtHttpServerRequestSetTrailers(
	xhttpserverrequest* pRequest,
	const xhttpfield* pTrailers,
	size_t iCount
)
{
	xhttpfield* pFields;
	uint8* pText;
	ptr pBlock = NULL;
	size_t iFieldBytes;
	size_t iTextBytes;
	size_t iTotal;
	size_t i;

	if ( (pRequest == NULL) ||
		!__xrtHttpServerRequestTrailerSize(
			pTrailers,
			iCount,
			&iFieldBytes,
			&iTextBytes
		) ) {
		if ( pRequest == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	iTotal = iFieldBytes;
	if ( !__xrtHttpServerRequestSizeAdd(
		&iTotal, iTextBytes
	) ) {
		return false;
	}
	if ( iTotal != 0 ) {
		pBlock = xrtMalloc(iTotal);
		if ( pBlock == NULL ) {
			return false;
		}
	}
	pFields = (xhttpfield*)pBlock;
	pText = pBlock != NULL ?
		((uint8*)pBlock + iFieldBytes) : NULL;
	for ( i = 0; i < iCount; i++ ) {
		pFields[i].Name = __xrtHttpServerRequestTextCopy(
			&pText, pTrailers[i].Name
		);
		pFields[i].Value = __xrtHttpServerRequestTextCopy(
			&pText, pTrailers[i].Value
		);
	}
	xrtFree(pRequest->TrailerBlock);
	pRequest->TrailerBlock = pBlock;
	pRequest->Trailers = pFields;
	pRequest->TrailerCount = iCount;
	return true;
}



/* 增加只会由服务器状态机发布的稳定事实。 */
void __xrtHttpServerRequestSetFlags(
	xhttpserverrequest* pRequest,
	uint32 iFlags
)
{
	if ( pRequest != NULL ) {
		pRequest->Flags |= iFlags;
	}
}



/* 增加线程安全请求引用。 */
XRT_API xhttpserverrequest* xrtHttpServerRequestRef(
	xhttpserverrequest* pRequest
)
{
	if ( (pRequest == NULL) ||
		(xrtRefRetain(&pRequest->RefCount) < 0) ) {
		if ( pRequest == NULL ) {
			__xrtErrorSetInvalidArgument();
		} else {
			__xrtErrorSetInvalidState();
		}
		return NULL;
	}
	return pRequest;
}



/* 释放最后一个请求引用和按需分配的资产。 */
XRT_API void xrtHttpServerRequestDestroy(
	xhttpserverrequest* pRequest
)
{
	if ( (pRequest == NULL) ||
		(xrtRefRelease(&pRequest->RefCount) != 0) ) {
		return;
	}
	xrtFree(pRequest->TrailerBlock);
	xrtFree(pRequest->Body);
	memset(pRequest, 0, sizeof(*pRequest));
	xrtFree(pRequest);
}



/* 返回请求版本。 */
XRT_API xhttpversion xrtHttpServerRequestVersion(
	const xhttpserverrequest* pRequest
)
{
	return pRequest != NULL ? pRequest->Version : 0;
}



/* 返回借用请求方法。 */
XRT_API xstrview xrtHttpServerRequestMethod(
	const xhttpserverrequest* pRequest
)
{
	return pRequest != NULL ?
		pRequest->Method : (xstrview){ NULL, 0 };
}



/* 返回借用 request-target。 */
XRT_API xstrview xrtHttpServerRequestTarget(
	const xhttpserverrequest* pRequest
)
{
	return pRequest != NULL ?
		pRequest->Target : (xstrview){ NULL, 0 };
}



/* 解析请求快照拥有的 request-target。 */
XRT_API bool xrtHttpServerRequestParseTarget(
	const xhttpserverrequest* pRequest,
	xhttptarget* pTarget
)
{
	xhttptarget Target = { 0 };

	if ( !__xrtHttpServerRequestOutputValid(
		pRequest, pTarget, sizeof(Target)
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pTarget, &Target, sizeof(Target));
	if ( pRequest == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return xrtHttpTargetParse(
		pRequest->Method,
		pRequest->Target,
		pTarget
	);
}



/* 按 request-target 形式返回有效 authority。 */
XRT_API bool xrtHttpServerRequestAuthority(
	const xhttpserverrequest* pRequest,
	xurl* pAuthority
)
{
	const xhttpfield* pHost;
	xhttptarget Target;
	xurl Authority = { 0 };
	xstrview Host = { NULL, 0 };

	if ( !__xrtHttpServerRequestOutputValid(
		pRequest, pAuthority, sizeof(Authority)
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pAuthority, &Authority, sizeof(Authority));
	if ( !xrtHttpServerRequestParseTarget(
		pRequest, &Target
	) ) {
		return false;
	}
	pHost = xrtHttpServerRequestHeader(
		pRequest,
		XRT_STR_LITERAL("Host")
	);
	if ( pHost != NULL ) {
		Host = pHost->Value;
	}
	return xrtHttpTargetAuthority(
		&Target,
		Host,
		pAuthority
	);
}



/* 返回请求稳定标志。 */
XRT_API uint32 xrtHttpServerRequestFlags(
	const xhttpserverrequest* pRequest
)
{
	return pRequest != NULL ?
		pRequest->Flags : XHTTP_SERVER_REQUEST_NONE;
}



/* 返回由 Exchange 验证并冻结的 Trailer 接收能力。 */
XRT_API bool xrtHttpServerRequestAcceptsTrailers(
	const xhttpserverrequest* pRequest
)
{
	return (pRequest != NULL) &&
		((pRequest->Flags &
		  XHTTP_SERVER_REQUEST_ACCEPTS_TRAILERS) != 0);
}



/* 返回正文分帧模式。 */
XRT_API xhttp1bodymode xrtHttpServerRequestBodyMode(
	const xhttpserverrequest* pRequest
)
{
	return pRequest != NULL ?
		pRequest->Plan.Mode : XHTTP1_BODY_NONE;
}



/* 只对定长正文返回声明长度。 */
XRT_API uint64 xrtHttpServerRequestContentLength(
	const xhttpserverrequest* pRequest
)
{
	return (pRequest != NULL) &&
		(pRequest->Plan.Mode == XHTTP1_BODY_FIXED) ?
		pRequest->Plan.Length : 0;
}



/* 查找首个同名 Header。 */
XRT_API const xhttpfield* xrtHttpServerRequestHeader(
	const xhttpserverrequest* pRequest,
	xstrview Name
)
{
	return pRequest != NULL ?
		xrtHttpFieldGet(
			pRequest->Fields,
			pRequest->FieldCount,
			Name
		) : NULL;
}



/* 返回 Header 数量。 */
XRT_API size_t xrtHttpServerRequestHeaderCount(
	const xhttpserverrequest* pRequest
)
{
	return pRequest != NULL ? pRequest->FieldCount : 0;
}



/* 返回请求拥有的连续只读 Header 数组。 */
XRT_API const xhttpfield* xrtHttpServerRequestHeaderData(
	const xhttpserverrequest* pRequest
)
{
	return (pRequest != NULL) &&
		(pRequest->FieldCount != 0) ?
		pRequest->Fields : NULL;
}



/* 返回指定位置的 Header。 */
XRT_API const xhttpfield* xrtHttpServerRequestHeaderAt(
	const xhttpserverrequest* pRequest,
	size_t iIndex
)
{
	return (pRequest != NULL) &&
		(iIndex < pRequest->FieldCount) ?
		&pRequest->Fields[iIndex] : NULL;
}



/* 返回缓冲正文；流式请求不持有正文副本。 */
XRT_API xbytesview xrtHttpServerRequestBody(
	const xhttpserverrequest* pRequest
)
{
	if ( (pRequest == NULL) ||
		((pRequest->Flags &
		  XHTTP_SERVER_REQUEST_STREAMED) != 0) ) {
		return (xbytesview){ NULL, 0 };
	}
	return (xbytesview){
		pRequest->Body,
		pRequest->BodySize
	};
}



/* 返回已交付正文总字节数。 */
XRT_API uint64 xrtHttpServerRequestBodyBytes(
	const xhttpserverrequest* pRequest
)
{
	return pRequest != NULL ? pRequest->BodyBytes : 0;
}



/* 查找首个同名 Trailer。 */
XRT_API const xhttpfield* xrtHttpServerRequestTrailer(
	const xhttpserverrequest* pRequest,
	xstrview Name
)
{
	return pRequest != NULL ?
		xrtHttpFieldGet(
			pRequest->Trailers,
			pRequest->TrailerCount,
			Name
		) : NULL;
}



/* 返回 Trailer 数量。 */
XRT_API size_t xrtHttpServerRequestTrailerCount(
	const xhttpserverrequest* pRequest
)
{
	return pRequest != NULL ? pRequest->TrailerCount : 0;
}



/* 返回请求拥有的连续只读 Trailer 数组。 */
XRT_API const xhttpfield* xrtHttpServerRequestTrailerData(
	const xhttpserverrequest* pRequest
)
{
	return (pRequest != NULL) &&
		(pRequest->TrailerCount != 0) ?
		pRequest->Trailers : NULL;
}



/* 返回指定位置的 Trailer。 */
XRT_API const xhttpfield* xrtHttpServerRequestTrailerAt(
	const xhttpserverrequest* pRequest,
	size_t iIndex
)
{
	return (pRequest != NULL) &&
		(iIndex < pRequest->TrailerCount) ?
		&pRequest->Trailers[iIndex] : NULL;
}

#endif
