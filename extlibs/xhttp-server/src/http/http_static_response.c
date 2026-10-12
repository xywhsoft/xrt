#include "../internal/xrt_http_static.h"



#if defined(XHTTP_FEATURE_HTTP_STATIC_RESPONSE)

/* 构建前测量保存全部派生状态，实际写出不再执行可变决策。 */
typedef struct xrt_http_static_response_measure {
	xhttpstaticresponseconfig Config;
	xstrview ContentType;
	xhttpcontentrange ContentRange;
	uint64 BodyLength;
	size_t Required;
	size_t ETagSize;
	size_t DateSize;
	size_t LengthSize;
	size_t RangeSize;
	size_t MultipartTypeSize;
	size_t FieldCount;
	bool Multipart;
	bool IncludeType;
	bool IncludeLength;
	bool IncludeRange;
	bool IncludeAcceptRanges;
	bool IncludeETag;
	bool IncludeLastModified;
	bool IncludeCacheControl;
	bool IncludeAllow;
} xrt_http_static_response_measure;



/* 安全累加工作区长度。 */
static bool __xrtHttpStaticResponseSizeAdd(
	size_t* pSize,
	size_t iAdd
)
{
	if ( *pSize > (SIZE_MAX - iAdd) ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	*pSize += iAdd;
	return true;
}



/* 初始化静态响应字段策略。 */
XHTTP_SERVER_API void xrtHttpStaticResponseConfigInit(
	xhttpstaticresponseconfig* pConfig
)
{
	xhttpstaticresponseconfig Config = {
		XRT_STR_LITERAL("application/octet-stream"),
		{ NULL, 0 },
		{ NULL, 0 }
	};

	if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return;
	}
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 验证计划中的已解析范围并重新核对有效负载总长度。 */
/* 快照可选的静态响应配置。 */
static bool __xrtHttpStaticResponseConfigRead(
	const xhttpstaticresponseconfig* pInput,
	xhttpstaticresponseconfig* pConfig
)
{
	*pConfig = (xhttpstaticresponseconfig){
		XRT_STR_LITERAL("application/octet-stream"),
		{ NULL, 0 },
		{ NULL, 0 }
	};
	if ( pInput == NULL ) {
		return true;
	}
	if ( !xrtMemRangeValid(pInput, sizeof(*pConfig)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	memcpy(pConfig, pInput, sizeof(*pConfig));
	return true;
}



static bool __xrtHttpStaticResponseRangesValid(
	const xhttpstaticplan* pPlan,
	const xhttpbyterange* pRanges
)
{
	xhttpbyterange Current;
	xhttpbyterange Previous;
	uint64 iSelected = 0;
	size_t i;

	if ( pPlan->RangeCount == 0 ) {
		return true;
	}
	if ( (pRanges == NULL) ||
		(pPlan->CompleteLength == 0) ) {
		return false;
	}
	for ( i = 0; i < pPlan->RangeCount; i++ ) {
		uint64 iLength;

		memcpy(
			&Current,
			((const uint8*)pRanges) +
				(i * sizeof(Current)),
			sizeof(Current)
		);
		if ( (Current.First > Current.Last) ||
			(Current.Last >= pPlan->CompleteLength) ||
			((i != 0) &&
			 (Previous.Last >= Current.First)) ) {
			return false;
		}
		iLength = (Current.Last - Current.First) +
			UINT64_C(1);
		if ( iSelected > (UINT64_MAX - iLength) ) {
			return false;
		}
		iSelected += iLength;
		Previous = Current;
	}
	return iSelected == pPlan->SelectedLength;
}



/* 验证静态计划自身可以安全生成响应。 */
static bool __xrtHttpStaticResponsePlanValid(
	const xhttpstaticplan* pPlan,
	const xhttpbyterange* pRanges
)
{
	if ( (pPlan == NULL) ||
		(pPlan->RangeCount >
		 (SIZE_MAX / sizeof(xhttpbyterange))) ) {
		return false;
	}
	switch ( pPlan->Status ) {
		case XHTTP_STATUS_OK:
			return (pPlan->RangeCount == 0) &&
				(pPlan->SelectedLength ==
				 pPlan->CompleteLength);

		case XHTTP_STATUS_PARTIAL_CONTENT:
			return pPlan->SendBody &&
				(pPlan->RangeCount != 0) &&
				(pPlan->SelectedLength != 0) &&
				__xrtHttpStaticResponseRangesValid(
					pPlan,
					pRanges
				);

		case XHTTP_STATUS_NOT_MODIFIED:
		case XHTTP_STATUS_METHOD_NOT_ALLOWED:
		case XHTTP_STATUS_PRECONDITION_FAILED:
		case XHTTP_STATUS_RANGE_NOT_SATISFIABLE:
			return !pPlan->SendBody &&
				(pPlan->RangeCount == 0) &&
				(pPlan->SelectedLength == 0);

		default:
			return false;
	}
}



/* 解析配置、状态和字段集合并计算精确工作区长度。 */
static bool __xrtHttpStaticResponseMeasure(
	const xhttpstaticplan* pPlan,
	const xhttpbyterange* pRanges,
	const xhttprepresentation* pCurrent,
	const xhttpstaticresponseconfig* pConfig,
	xrt_http_static_response_measure* pMeasure
)
{
	static const size_t iMultipartPrefix =
		sizeof("multipart/byteranges; boundary=") - 1u;
	xrt_http_static_response_measure Measure;
	xhttpbyterange Range;
	bool bMetadata;

	memset(&Measure, 0, sizeof(Measure));
	Measure.Config = *pConfig;
	if ( !pCurrent->Exists ||
		!__xhttpViewValid(Measure.Config.ContentType) ||
		!__xhttpViewValid(Measure.Config.CacheControl) ||
		!__xhttpViewValid(Measure.Config.Boundary) ||
		!__xrtHttpStaticResponsePlanValid(
			pPlan,
			pRanges
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	Measure.ContentType =
		Measure.Config.ContentType.Size != 0 ?
		Measure.Config.ContentType :
		XRT_STR_LITERAL("application/octet-stream");
	if ( !xrtHttpFieldValueValid(Measure.ContentType) ||
		((Measure.Config.CacheControl.Size != 0) &&
		 !xrtHttpFieldValueValid(
			Measure.Config.CacheControl
		 )) ||
		(pCurrent->HasETag &&
		 !xrtHttpBridgeETagValid(&pCurrent->ETag)) ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}

	Measure.Multipart =
		(pPlan->Status == XHTTP_STATUS_PARTIAL_CONTENT) &&
		(pPlan->RangeCount > 1u);
	if ( Measure.Multipart ) {
		if ( !xrtHttpBridgeRangeMultipartLength(
			pRanges,
			pPlan->RangeCount,
			pPlan->CompleteLength,
			Measure.ContentType,
			Measure.Config.Boundary,
			&Measure.BodyLength
		) ) {
			return false;
		}
		if ( !__xrtHttpStaticResponseSizeAdd(
			&Measure.MultipartTypeSize,
			iMultipartPrefix
		) || !__xrtHttpStaticResponseSizeAdd(
			&Measure.MultipartTypeSize,
			Measure.Config.Boundary.Size
		) ) {
			return false;
		}
	}

	Measure.IncludeType =
		(pPlan->Status == XHTTP_STATUS_OK) ||
		(pPlan->Status == XHTTP_STATUS_PARTIAL_CONTENT);
	Measure.IncludeLength =
		pPlan->Status != XHTTP_STATUS_NOT_MODIFIED;
	Measure.IncludeRange =
		((pPlan->Status == XHTTP_STATUS_PARTIAL_CONTENT) &&
		 (pPlan->RangeCount == 1u)) ||
		(pPlan->Status ==
		 XHTTP_STATUS_RANGE_NOT_SATISFIABLE);
	Measure.IncludeAcceptRanges =
		pPlan->AcceptRanges &&
		(pPlan->Status != XHTTP_STATUS_METHOD_NOT_ALLOWED);
	Measure.IncludeAllow =
		pPlan->Status == XHTTP_STATUS_METHOD_NOT_ALLOWED;
	bMetadata =
		pPlan->Status != XHTTP_STATUS_METHOD_NOT_ALLOWED;
	Measure.IncludeETag =
		bMetadata && pCurrent->HasETag;
	Measure.IncludeLastModified =
		bMetadata && pCurrent->HasLastModified;
	Measure.IncludeCacheControl =
		bMetadata &&
		(Measure.Config.CacheControl.Size != 0);

	if ( pPlan->Status == XHTTP_STATUS_OK ) {
		Measure.BodyLength = pPlan->CompleteLength;
	} else if ( pPlan->Status ==
		XHTTP_STATUS_PARTIAL_CONTENT &&
		!Measure.Multipart ) {
		Measure.BodyLength = pPlan->SelectedLength;
	}
	if ( Measure.IncludeRange ) {
		memset(
			&Measure.ContentRange,
			0,
			sizeof(Measure.ContentRange)
		);
		Measure.ContentRange.HasLength = true;
		Measure.ContentRange.Length =
			pPlan->CompleteLength;
		if ( pPlan->Status ==
			XHTTP_STATUS_PARTIAL_CONTENT ) {
			memcpy(&Range, pRanges, sizeof(Range));
			Measure.ContentRange.Satisfied = true;
			Measure.ContentRange.First = Range.First;
			Measure.ContentRange.Last = Range.Last;
		}
		if ( !xrtHttpContentRangeWrite(
			&Measure.ContentRange,
			NULL,
			0,
			&Measure.RangeSize
		) ) {
			return false;
		}
	}
	if ( Measure.IncludeETag &&
		!xrtHttpETagWrite(
			&pCurrent->ETag,
			NULL,
			0,
			&Measure.ETagSize
		) ) {
		return false;
	}
	if ( Measure.IncludeLastModified ) {
		Measure.DateSize = xrtTimeWriteHTTPDate(
			NULL,
			0,
			pCurrent->LastModified
		);
		if ( Measure.DateSize == XRT_NPOS ) {
			return false;
		}
	}
	if ( Measure.IncludeLength ) {
		Measure.LengthSize = __xhttpUInt64Size(
			Measure.BodyLength
		);
	}

	Measure.FieldCount =
		(size_t)Measure.IncludeType +
		(size_t)Measure.IncludeLength +
		(size_t)Measure.IncludeRange +
		(size_t)Measure.IncludeAcceptRanges +
		(size_t)Measure.IncludeETag +
		(size_t)Measure.IncludeLastModified +
		(size_t)Measure.IncludeCacheControl +
		(size_t)Measure.IncludeAllow;
	if ( Measure.FieldCount >
		XHTTP_STATIC_RESPONSE_MAX_FIELDS ) {
		xrtSetErrorKind(XERR_INTERNAL);
		return false;
	}
	if ( (Measure.Multipart &&
		 !__xrtHttpStaticResponseSizeAdd(
			&Measure.Required,
			Measure.MultipartTypeSize
		 )) ||
		!__xrtHttpStaticResponseSizeAdd(
			&Measure.Required,
			Measure.LengthSize
		) ||
		!__xrtHttpStaticResponseSizeAdd(
			&Measure.Required,
			Measure.RangeSize
		) ||
		!__xrtHttpStaticResponseSizeAdd(
			&Measure.Required,
			Measure.ETagSize
		) ||
		!__xrtHttpStaticResponseSizeAdd(
			&Measure.Required,
			Measure.DateSize
		) ) {
		return false;
	}
	*pMeasure = Measure;
	return true;
}



/* 判断一个输出区域是否覆盖静态响应的任一输入。 */
static bool __xrtHttpStaticResponseInputOverlap(
	const void* pOutput,
	size_t iOutputSize,
	const xhttpstaticplan* pPlanInput,
	const xhttpbyterange* pRanges,
	size_t iRangeBytes,
	const xhttprepresentation* pCurrentInput,
	const xhttpstaticresponseconfig* pConfig,
	const xhttprepresentation* pCurrent,
	const xrt_http_static_response_measure* pMeasure
)
{
	return xrtMemRangesOverlap(
			pOutput,
			iOutputSize,
			pPlanInput,
			sizeof(*pPlanInput)
		) || xrtMemRangesOverlap(
			pOutput, iOutputSize, pRanges, iRangeBytes
		) || xrtMemRangesOverlap(
			pOutput,
			iOutputSize,
			pCurrentInput,
			sizeof(*pCurrentInput)
		) || ((pConfig != NULL) && xrtMemRangesOverlap(
			pOutput, iOutputSize, pConfig, sizeof(*pConfig)
		)) || xrtMemRangesOverlap(
			pOutput,
			iOutputSize,
			pMeasure->Config.ContentType.Data,
			pMeasure->Config.ContentType.Size
		) || xrtMemRangesOverlap(
			pOutput,
			iOutputSize,
			pMeasure->Config.CacheControl.Data,
			pMeasure->Config.CacheControl.Size
		) || xrtMemRangesOverlap(
			pOutput,
			iOutputSize,
			pMeasure->Config.Boundary.Data,
			pMeasure->Config.Boundary.Size
		) || (pCurrent->HasETag && xrtMemRangesOverlap(
			pOutput,
			iOutputSize,
			pCurrent->ETag.Opaque.Data,
			pCurrent->ETag.Opaque.Size
		));
}



/* 验证长度、结构和工作区输出互不覆盖。 */
static bool __xrtHttpStaticResponseAliasesValid(
	const xhttpstaticplan* pPlanInput,
	const xhttpbyterange* pRanges,
	size_t iRangeBytes,
	const xhttprepresentation* pCurrentInput,
	const xhttpstaticresponseconfig* pConfig,
	const xhttprepresentation* pCurrent,
	const xrt_http_static_response_measure* pMeasure,
	void* pWorkspace,
	size_t* pSize,
	xhttpstaticresponse* pResponse
)
{
	if ( __xrtHttpStaticResponseInputOverlap(
			pSize,
			sizeof(*pSize),
			pPlanInput,
			pRanges,
			iRangeBytes,
			pCurrentInput,
			pConfig,
			pCurrent,
			pMeasure
		) || ((pResponse != NULL) &&
		 (__xrtHttpStaticResponseInputOverlap(
			pResponse,
			sizeof(*pResponse),
			pPlanInput,
			pRanges,
			iRangeBytes,
			pCurrentInput,
			pConfig,
			pCurrent,
			pMeasure
		  ) || xrtMemRangesOverlap(
			pResponse,
			sizeof(*pResponse),
			pSize,
			sizeof(*pSize)
		  ))) || ((pMeasure->Required != 0) &&
		 (__xrtHttpStaticResponseInputOverlap(
			pWorkspace,
			pMeasure->Required,
			pPlanInput,
			pRanges,
			iRangeBytes,
			pCurrentInput,
			pConfig,
			pCurrent,
			pMeasure
		  ) || xrtMemRangesOverlap(
			pWorkspace,
			pMeasure->Required,
			pSize,
			sizeof(*pSize)
		  ) || ((pResponse != NULL) &&
		   xrtMemRangesOverlap(
			pWorkspace,
			pMeasure->Required,
			pResponse,
			sizeof(*pResponse)
		   )))) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	return true;
}



/* 向响应追加一个借用字段。 */
static void __xrtHttpStaticResponseField(
	xhttpstaticresponse* pResponse,
	xstrview Name,
	xstrview Value
)
{
	pResponse->Fields[pResponse->FieldCount].Name = Name;
	pResponse->Fields[pResponse->FieldCount].Value = Value;
	pResponse->FieldCount++;
}



/* 在工作区中保留一个字段值视图。 */
static xstrview __xrtHttpStaticResponseReserve(
	char* sWorkspace,
	size_t* pOffset,
	size_t iSize
)
{
	xstrview Value = {
		sWorkspace + *pOffset,
		iSize
	};

	*pOffset += iSize;
	return Value;
}



/* 把测量结果写入工作区和独立字段描述符。 */
static bool __xrtHttpStaticResponseWrite(
	const xhttpstaticplan* pPlan,
	const xhttprepresentation* pCurrent,
	const xrt_http_static_response_measure* pMeasure,
	char* sWorkspace,
	xhttpstaticresponse* pResponse
)
{
	static const char sMultipartPrefix[] =
		"multipart/byteranges; boundary=";
	xhttpstaticresponse Response;
	xstrview Value;
	char sDate[30];
	size_t iValue;
	size_t iOffset = 0;

	memset(&Response, 0, sizeof(Response));
	Response.Status = pPlan->Status;
	Response.SendBody = pPlan->SendBody;
	Response.Multipart = pMeasure->Multipart;
	Response.BodyLength = pMeasure->BodyLength;
	Response.ContentType = pMeasure->ContentType;
	Response.Boundary = pMeasure->Config.Boundary;

	if ( pMeasure->IncludeType ) {
		if ( pMeasure->Multipart ) {
			Value = __xrtHttpStaticResponseReserve(
				sWorkspace,
				&iOffset,
				pMeasure->MultipartTypeSize
			);
			memcpy(
				(char*)Value.Data,
				sMultipartPrefix,
				sizeof(sMultipartPrefix) - 1u
			);
			memcpy(
				(char*)Value.Data +
					sizeof(sMultipartPrefix) - 1u,
				pMeasure->Config.Boundary.Data,
				pMeasure->Config.Boundary.Size
			);
		} else {
			Value = pMeasure->ContentType;
		}
		__xrtHttpStaticResponseField(
			&Response,
			XRT_STR_LITERAL("Content-Type"),
			Value
		);
	}
	if ( pMeasure->IncludeLength ) {
		Value = __xrtHttpStaticResponseReserve(
			sWorkspace,
			&iOffset,
			pMeasure->LengthSize
		);
		iValue = __xhttpUInt64Write(
			(char*)Value.Data,
			pMeasure->BodyLength
		);
		if ( iValue != Value.Size ) {
			xrtSetErrorKind(XERR_INTERNAL);
			return false;
		}
		__xrtHttpStaticResponseField(
			&Response,
			XRT_STR_LITERAL("Content-Length"),
			Value
		);
	}
	if ( pMeasure->IncludeRange ) {
		Value = __xrtHttpStaticResponseReserve(
			sWorkspace,
			&iOffset,
			pMeasure->RangeSize
		);
		if ( !xrtHttpContentRangeWrite(
			&pMeasure->ContentRange,
			(char*)Value.Data,
			Value.Size,
			&iValue
		) || (iValue != Value.Size) ) {
			return false;
		}
		__xrtHttpStaticResponseField(
			&Response,
			XRT_STR_LITERAL("Content-Range"),
			Value
		);
	}
	if ( pMeasure->IncludeAcceptRanges ) {
		__xrtHttpStaticResponseField(
			&Response,
			XRT_STR_LITERAL("Accept-Ranges"),
			XRT_STR_LITERAL("bytes")
		);
	}
	if ( pMeasure->IncludeETag ) {
		Value = __xrtHttpStaticResponseReserve(
			sWorkspace,
			&iOffset,
			pMeasure->ETagSize
		);
		if ( !xrtHttpETagWrite(
			&pCurrent->ETag,
			(char*)Value.Data,
			Value.Size,
			&iValue
		) || (iValue != Value.Size) ) {
			return false;
		}
		__xrtHttpStaticResponseField(
			&Response,
			XRT_STR_LITERAL("ETag"),
			Value
		);
	}
	if ( pMeasure->IncludeLastModified ) {
		Value = __xrtHttpStaticResponseReserve(
			sWorkspace,
			&iOffset,
			pMeasure->DateSize
		);
		iValue = xrtTimeWriteHTTPDate(
			sDate,
			sizeof(sDate),
			pCurrent->LastModified
		);
		if ( iValue != Value.Size ) {
			return false;
		}
		memcpy((char*)Value.Data, sDate, iValue);
		__xrtHttpStaticResponseField(
			&Response,
			XRT_STR_LITERAL("Last-Modified"),
			Value
		);
	}
	if ( pMeasure->IncludeCacheControl ) {
		__xrtHttpStaticResponseField(
			&Response,
			XRT_STR_LITERAL("Cache-Control"),
			pMeasure->Config.CacheControl
		);
	}
	if ( pMeasure->IncludeAllow ) {
		__xrtHttpStaticResponseField(
			&Response,
			XRT_STR_LITERAL("Allow"),
			XRT_STR_LITERAL("GET, HEAD")
		);
	}
	if ( (Response.FieldCount != pMeasure->FieldCount) ||
		(iOffset != pMeasure->Required) ) {
		xrtSetErrorKind(XERR_INTERNAL);
		return false;
	}
	memcpy(pResponse, &Response, sizeof(Response));
	return true;
}



/* 构建静态响应协议字段。 */
XHTTP_SERVER_API bool xrtHttpStaticResponseBuild(
	const xhttpstaticplan* pPlan,
	const xhttpbyterange* pRanges,
	const xhttprepresentation* pCurrent,
	const xhttpstaticresponseconfig* pConfig,
	void* pWorkspace,
	size_t iCapacity,
	size_t* pSize,
	xhttpstaticresponse* pResponse
)
{
	xhttpstaticresponseconfig Config;
	xhttpstaticplan Plan;
	xhttprepresentation Current;
	xrt_http_static_response_measure Measure;
	size_t iRangeBytes;
	size_t iRequired;

	if ( !xrtMemRangeValid(pSize, sizeof(iRequired)) ||
		!xrtMemRangeValid(pWorkspace, iCapacity) ||
		((pResponse == NULL) &&
		 ((pWorkspace != NULL) || (iCapacity != 0))) ||
		((pResponse != NULL) &&
		 !xrtMemRangeValid(pResponse, sizeof(*pResponse))) ||
		!xrtMemRangeValid(pPlan, sizeof(Plan)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	memcpy(&Plan, pPlan, sizeof(Plan));
	if ( Plan.RangeCount >
		(SIZE_MAX / sizeof(xhttpbyterange)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	iRangeBytes = Plan.RangeCount * sizeof(xhttpbyterange);
	if ( !xrtMemRangeValid(pRanges, iRangeBytes) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( !xrtHttpBridgePreconditionInputRead(
			XRT_STR_LITERAL("GET"),
			NULL,
			0,
			pCurrent,
			&Current
		) || !__xrtHttpStaticResponseConfigRead(
			pConfig,
			&Config
		) ) {
		return false;
	}
	if ( !__xrtHttpStaticResponseMeasure(
		&Plan,
		pRanges,
		&Current,
		&Config,
		&Measure
	) || !__xrtHttpStaticResponseAliasesValid(
		pPlan,
		pRanges,
		iRangeBytes,
		pCurrent,
		pConfig,
		&Current,
		&Measure,
		pWorkspace,
		pSize,
		pResponse
	) ) {
		return false;
	}
	if ( pResponse == NULL ) {
		iRequired = Measure.Required;
		memcpy(pSize, &iRequired, sizeof(iRequired));
		return true;
	}
	if ( iCapacity < Measure.Required ) {
		iRequired = Measure.Required;
		memcpy(pSize, &iRequired, sizeof(iRequired));
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	if ( (Measure.Required != 0) &&
		(pWorkspace == NULL) ) {
		iRequired = Measure.Required;
		memcpy(pSize, &iRequired, sizeof(iRequired));
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	if ( !__xrtHttpStaticResponseWrite(
		&Plan,
		&Current,
		&Measure,
		(char*)pWorkspace,
		pResponse
	) ) {
		return false;
	}
	iRequired = Measure.Required;
	memcpy(pSize, &iRequired, sizeof(iRequired));
	return true;
}

#endif
