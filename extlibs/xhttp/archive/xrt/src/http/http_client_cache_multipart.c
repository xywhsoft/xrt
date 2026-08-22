#include "../internal/xrt_http_client_runtime.h"

#include <xrt/multipart.h>



#if defined(XRT_FEATURE_HTTP_CLIENT_CACHE)

#define XRT_HTTP_CLIENT_CACHE_MULTIPART_HEADERS_MAX 64u
#define XRT_HTTP_CLIENT_CACHE_MULTIPART_HEADER_BYTES_MAX (64u * 1024u)



/* 判断两个借用文本是否逐字节相同。 */
static bool __xrtHttpClientCacheMultipartViewEqual(
	xstrview Left,
	xstrview Right
)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 保守比较两个 Part Content-Type，类型标记忽略大小写，参数保持原值。 */
static bool __xrtHttpClientCacheMultipartTypeEqual(
	const xmediatype* pLeft,
	const xmediatype* pRight
)
{
	xstrview LeftParameters =
		xrtHttpOwsTrim(pLeft->Parameters);
	xstrview RightParameters =
		xrtHttpOwsTrim(pRight->Parameters);

	return xrtHttpTokenEqual(
			pLeft->Type,
			pRight->Type
		) && xrtHttpTokenEqual(
			pLeft->Subtype,
			pRight->Subtype
		) && __xrtHttpClientCacheMultipartViewEqual(
			LeftParameters,
			RightParameters
		);
}



/* 发布不属于 multipart/byteranges 的无错误结果。 */
static __xrt_http_client_cache_multipart_decision
__xrtHttpClientCacheMultipartNone(
	__xrt_http_client_cache_multipart* pPlan
)
{
	memset(pPlan, 0, sizeof(*pPlan));
	pPlan->Decision =
		__XRT_HTTP_CLIENT_CACHE_MULTIPART_NONE;
	return pPlan->Decision;
}



/* 释放局部片段并发布带稳定原因的保守跳过结果。 */
static __xrt_http_client_cache_multipart_decision
__xrtHttpClientCacheMultipartSkip(
	xhttpcachepart* pParts,
	uint32 iReasons,
	xerrkind Kind,
	__xrt_http_client_cache_multipart* pPlan
)
{
	xrtFree(pParts);
	memset(pPlan, 0, sizeof(*pPlan));
	pPlan->Decision =
		__XRT_HTTP_CLIENT_CACHE_MULTIPART_SKIP;
	pPlan->Reasons = iReasons;
	if ( Kind == XERR_RANGE ) {
		__xrtErrorSetRange();
	} else {
		__xrtErrorSetValue();
	}
	return pPlan->Decision;
}



/* 严格读取 Part 字段块到固定上限的借用数组。 */
static bool __xrtHttpClientCacheMultipartFields(
	const xmultipartpart* pPart,
	xhttpfield Fields[
		XRT_HTTP_CLIENT_CACHE_MULTIPART_HEADERS_MAX
	],
	size_t* pCount
)
{
	xhttpfield Field;
	xhttpnext Next;
	size_t iOffset = 0;
	size_t iCount = 0;

	*pCount = 0;
	if ( (pPart->HeaderCount >
		  XRT_HTTP_CLIENT_CACHE_MULTIPART_HEADERS_MAX) ||
		(pPart->Headers.Size >
		 XRT_HTTP_CLIENT_CACHE_MULTIPART_HEADER_BYTES_MAX) ) {
		__xrtErrorSetRange();
		return false;
	}
	while ( (Next = xrtHttpFieldNext(
		pPart->Headers,
		&iOffset,
		&Field
	)) == XHTTP_NEXT_ITEM ) {
		if ( iCount >=
			XRT_HTTP_CLIENT_CACHE_MULTIPART_HEADERS_MAX ) {
			__xrtErrorSetRange();
			return false;
		}
		Fields[iCount++] = Field;
	}
	if ( (Next != XHTTP_NEXT_END) ||
		(iCount != pPart->HeaderCount) ) {
		if ( Next != XHTTP_NEXT_ERROR ) {
			__xrtErrorSetValue();
		}
		return false;
	}
	*pCount = iCount;
	return true;
}



/* 拒绝 Part 级传输编码，避免把编码载荷误当表示字节。 */
static bool __xrtHttpClientCacheMultipartEncoding(
	const xmultipartpart* pPart,
	const xhttpfield* pFields,
	size_t iCount
)
{
	size_t i;

	if ( (pPart->Flags &
		  XMULTIPART_PART_TRANSFER_ENCODING) != 0 ) {
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		if ( xrtHttpFieldNameEqual(
			pFields[i].Name,
			XRT_STR_LITERAL("Transfer-Encoding")
		) ) {
			return false;
		}
	}
	return true;
}



/* 验证可选 Part Content-Length 的全部值相同且匹配实际正文。 */
static bool __xrtHttpClientCacheMultipartLength(
	const xhttpfield* pFields,
	size_t iCount,
	size_t iBodySize
)
{
	uint64 iLength = 0;
	bool bPresent = false;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		uint64 iCurrent;

		if ( !xrtHttpFieldNameEqual(
			pFields[i].Name,
			XRT_STR_LITERAL("Content-Length")
		) ) {
			continue;
		}
		if ( !xrtHttpContentLengthParse(
			pFields[i].Value,
			&iCurrent
		) || (bPresent && (iCurrent != iLength)) ) {
			return false;
		}
		iLength = iCurrent;
		bPresent = true;
	}
	return !bPresent || (iLength == (uint64)iBodySize);
}



/* 返回 Part 原始 Content-Type 字段，通用解析器已经拒绝重复值。 */
static xstrview __xrtHttpClientCacheMultipartContentType(
	const xhttpfield* pFields,
	size_t iCount
)
{
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		if ( xrtHttpFieldNameEqual(
			pFields[i].Name,
			XRT_STR_LITERAL("Content-Type")
		) ) {
			return xrtHttpOwsTrim(pFields[i].Value);
		}
	}
	return (xstrview){ NULL, 0 };
}



/* 按表示偏移对借用片段执行稳定插入排序。 */
static void __xrtHttpClientCacheMultipartSort(
	xhttpcachepart* pParts,
	size_t iCount
)
{
	size_t i;

	for ( i = 1; i < iCount; i++ ) {
		xhttpcachepart Part = pParts[i];
		size_t j = i;

		while ( (j != 0) &&
			(pParts[j - 1u].Offset > Part.Offset) ) {
			pParts[j] = pParts[j - 1u];
			j--;
		}
		pParts[j] = Part;
	}
}



/* 合并重复或相交 Part；重叠字节不一致时拒绝整组表示。 */
static bool __xrtHttpClientCacheMultipartNormalize(
	xhttpcachepart* pParts,
	size_t iCount,
	size_t* pNormalized
)
{
	size_t iWrite = 0;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		xhttpcachepart Part = pParts[i];
		uint64 iPartLast;

		if ( (Part.Data.Size == 0) ||
			(((uint64)Part.Data.Size -
			  UINT64_C(1)) >
			 (UINT64_MAX - Part.Offset)) ) {
			__xrtErrorSetValue();
			return false;
		}
		iPartLast =
			Part.Offset +
			(uint64)Part.Data.Size -
			UINT64_C(1);
		if ( iWrite != 0 ) {
			xhttpcachepart* pPrevious =
				&pParts[iWrite - 1u];
			uint64 iPreviousLast =
				pPrevious->Offset +
				(uint64)pPrevious->Data.Size -
				UINT64_C(1);

			if ( Part.Offset <= iPreviousLast ) {
				uint64 iOverlapLast =
					iPartLast < iPreviousLast ?
						iPartLast :
						iPreviousLast;
				size_t iOverlap = (size_t)(
					(iOverlapLast -
					 Part.Offset) +
					UINT64_C(1)
				);
				size_t iPreviousOffset =
					(size_t)(
						Part.Offset -
						pPrevious->Offset
					);

				if ( memcmp(
					pPrevious->Data.Data +
						iPreviousOffset,
					Part.Data.Data,
					iOverlap
				) != 0 ) {
					__xrtErrorSetValue();
					return false;
				}
				if ( iPartLast <= iPreviousLast ) {
					continue;
				}
				Part.Offset =
					iPreviousLast +
					UINT64_C(1);
				Part.Data.Data += iOverlap;
				Part.Data.Size -= iOverlap;
			}
		}
		pParts[iWrite++] = Part;
	}
	*pNormalized = iWrite;
	return true;
}



/* 验证所有未知长度 Part 也落在后来确认的完整长度内。 */
static bool __xrtHttpClientCacheMultipartWithinLength(
	const xhttpcachepart* pParts,
	size_t iCount,
	uint64 iLength
)
{
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		if ( (pParts[i].Offset >= iLength) ||
			((uint64)pParts[i].Data.Size >
			 (iLength - pParts[i].Offset)) ) {
			return false;
		}
	}
	return true;
}



/* 判断规范片段是否无空洞覆盖已知完整表示。 */
static bool __xrtHttpClientCacheMultipartComplete(
	const xhttpcachepart* pParts,
	size_t iCount,
	uint64 iLength
)
{
	uint64 iNext = 0;
	size_t i;

	if ( (iLength == 0) || (iCount == 0) ) {
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		uint64 iSize =
			(uint64)pParts[i].Data.Size;

		if ( pParts[i].Offset != iNext ) {
			return false;
		}
		if ( iSize > (UINT64_MAX - iNext) ) {
			return false;
		}
		iNext += iSize;
	}
	return iNext == iLength;
}



/* 按实际 Part 数小步扩展描述数组，不按用户上限预留内存。 */
static bool __xrtHttpClientCacheMultipartReserve(
	xhttpcachepart** ppParts,
	size_t* pCapacity,
	size_t iRequired,
	size_t iMaxParts
)
{
	xhttpcachepart* pParts;
	size_t iCapacity;
	size_t iBytes;

	if ( iRequired <= *pCapacity ) {
		return true;
	}
	iCapacity = *pCapacity != 0 ?
		*pCapacity :
		(iMaxParts < 4u ? iMaxParts : 4u);
	while ( iCapacity < iRequired ) {
		if ( iCapacity > (iMaxParts / 2u) ) {
			iCapacity = iMaxParts;
		} else {
			iCapacity *= 2u;
		}
	}
	if ( (iCapacity < iRequired) ||
		(iCapacity >
		 (SIZE_MAX / sizeof(**ppParts))) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iBytes = iCapacity * sizeof(**ppParts);
	pParts = *ppParts == NULL ?
		(xhttpcachepart*)xrtMalloc(iBytes) :
		(xhttpcachepart*)xrtRealloc(*ppParts, iBytes);
	if ( pParts == NULL ) {
		return false;
	}
	*ppParts = pParts;
	*pCapacity = iCapacity;
	return true;
}



/* 释放源站 multipart 范围计划。 */
void __xrtHttpClientCacheMultipartUnit(
	__xrt_http_client_cache_multipart* pPlan
)
{
	if ( pPlan == NULL ) {
		return;
	}
	xrtFree(pPlan->Parts);
	memset(pPlan, 0, sizeof(*pPlan));
}



/* 把源站 multipart/byteranges 正文转换为规范缓存片段。 */
__xrt_http_client_cache_multipart_decision
__xrtHttpClientCacheMultipartPlan(
	const xhttpcachefragmentinput* pBase,
	xstrview ContentType,
	xbytesview Body,
	size_t iMaxParts,
	__xrt_http_client_cache_multipart* pPlan
)
{
	xhttpfield Fields[
		XRT_HTTP_CLIENT_CACHE_MULTIPART_HEADERS_MAX
	];
	xmultipartboundary Boundary;
	xmultiparterrorinfo MultipartError;
	xmultipartpart MultipartPart;
	xhttpcachefragmentinput Input;
	xhttpcachefragmentplan FragmentPlan;
	xhttpcachefragmentdecision FragmentDecision;
	xhttpcachepart* pParts = NULL;
	xmediatype OuterType;
	xmediatype SelectedType;
	xstrview SelectedText = { NULL, 0 };
	uint64 iLength = 0;
	size_t iOffset = 0;
	size_t iCount = 0;
	size_t iCapacity = 0;
	size_t iNormalized = 0;
	uint32 iFlags = 0;
	xhttpnext Next;

	if ( (pPlan == NULL) || (pBase == NULL) ||
		!__xrtHttpViewValid(ContentType) ||
		((Body.Data == NULL) && (Body.Size != 0)) ||
		(iMaxParts == 0) ||
		(pBase->Status != XHTTP_STATUS_PARTIAL_CONTENT) ||
		(pBase->RangeFields != NULL) ||
		(pBase->RangeFieldCount != 0) ||
		((pBase->Flags &
		  XHTTP_CACHE_FRAGMENT_MULTIPART_PART) != 0) ||
		__xrtRangesOverlap(
			pPlan,
			sizeof(*pPlan),
			pBase,
			sizeof(*pBase)
		) || __xrtRangesOverlap(
			pPlan,
			sizeof(*pPlan),
			ContentType.Data,
			ContentType.Size
		) || __xrtRangesOverlap(
			pPlan,
			sizeof(*pPlan),
			Body.Data,
			Body.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return __XRT_HTTP_CLIENT_CACHE_MULTIPART_ERROR;
	}
	if ( !xrtHttpMediaTypeParse(
		ContentType,
		&OuterType
	) ) {
		xrtClearError();
		return __xrtHttpClientCacheMultipartNone(pPlan);
	}
	if ( !xrtHttpTokenEqual(
			OuterType.Type,
			XRT_STR_LITERAL("multipart")
		) || !xrtHttpTokenEqual(
			OuterType.Subtype,
			XRT_STR_LITERAL("byteranges")
		) ) {
		return __xrtHttpClientCacheMultipartNone(pPlan);
	}
	if ( !xrtMultipartBoundaryFromContentType(
		ContentType,
		&Boundary
	) ) {
		return __xrtHttpClientCacheMultipartSkip(
			NULL,
			__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_BOUNDARY,
			XERR_VALUE,
			pPlan
		);
	}
	memset(&SelectedType, 0, sizeof(SelectedType));

	for ( ;; ) {
		size_t iFieldCount;
		xstrview PartType;

		Next = xrtMultipartNext(
			Body,
			&Boundary,
			&iOffset,
			&MultipartPart,
			&MultipartError
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return __xrtHttpClientCacheMultipartSkip(
				pParts,
				__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_BODY,
				XERR_VALUE,
				pPlan
			);
		}
		if ( Next == XHTTP_NEXT_END ) {
			break;
		}
		if ( iCount >= iMaxParts ) {
			return __xrtHttpClientCacheMultipartSkip(
				pParts,
				__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_PARTS,
				XERR_RANGE,
				pPlan
			);
		}
		if ( !__xrtHttpClientCacheMultipartReserve(
			&pParts,
			&iCapacity,
			iCount + 1u,
			iMaxParts
		) ) {
			xrtFree(pParts);
			return __XRT_HTTP_CLIENT_CACHE_MULTIPART_ERROR;
		}
		if ( !__xrtHttpClientCacheMultipartFields(
			&MultipartPart,
			Fields,
			&iFieldCount
		) ) {
			return __xrtHttpClientCacheMultipartSkip(
				pParts,
				__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_HEADERS,
				xrtErrorIs(xrtGetError(), XERR_RANGE) != NULL ?
					XERR_RANGE :
					XERR_VALUE,
				pPlan
			);
		}
		if ( !__xrtHttpClientCacheMultipartEncoding(
			&MultipartPart,
			Fields,
			iFieldCount
		) ) {
			return __xrtHttpClientCacheMultipartSkip(
				pParts,
				__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_ENCODING,
				XERR_VALUE,
				pPlan
			);
		}
		if ( !__xrtHttpClientCacheMultipartLength(
			Fields,
			iFieldCount,
			MultipartPart.Body.Size
		) ) {
			return __xrtHttpClientCacheMultipartSkip(
				pParts,
				__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_LENGTH,
				XERR_VALUE,
				pPlan
			);
		}

		Input = *pBase;
		Input.RangeFields = Fields;
		Input.RangeFieldCount = iFieldCount;
		Input.BodySize =
			(uint64)MultipartPart.Body.Size;
		Input.Flags |=
			XHTTP_CACHE_FRAGMENT_MULTIPART_PART |
			XHTTP_CACHE_FRAGMENT_BODY_COMPLETE;
		FragmentDecision = xrtHttpCacheFragmentPlan(
			&Input,
			&FragmentPlan
		);
		if ( FragmentDecision ==
			XHTTP_CACHE_FRAGMENT_ERROR ) {
			xrtFree(pParts);
			return __XRT_HTTP_CLIENT_CACHE_MULTIPART_ERROR;
		}
		if ( FragmentDecision !=
			XHTTP_CACHE_FRAGMENT_STORE ) {
			return __xrtHttpClientCacheMultipartSkip(
				pParts,
				__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_RANGE,
				XERR_VALUE,
				pPlan
			);
		}
		if ( (FragmentPlan.Fragment.Flags &
			  XHTTP_CACHE_FRAGMENT_HAS_LENGTH) != 0 ) {
			if ( ((iFlags &
				  __XRT_HTTP_CLIENT_CACHE_MULTIPART_HAS_LENGTH) != 0) &&
				(iLength !=
				 FragmentPlan.Fragment.Length) ) {
				return __xrtHttpClientCacheMultipartSkip(
					pParts,
					__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_LENGTH,
					XERR_VALUE,
					pPlan
				);
			}
			iLength = FragmentPlan.Fragment.Length;
			iFlags |=
				__XRT_HTTP_CLIENT_CACHE_MULTIPART_HAS_LENGTH;
		}

		PartType = __xrtHttpClientCacheMultipartContentType(
			Fields,
			iFieldCount
		);
		if ( PartType.Data != NULL ) {
			if ( (iFlags &
				  __XRT_HTTP_CLIENT_CACHE_MULTIPART_HAS_CONTENT_TYPE) == 0 ) {
				SelectedType = MultipartPart.ContentType;
				SelectedText = PartType;
				iFlags |=
					__XRT_HTTP_CLIENT_CACHE_MULTIPART_HAS_CONTENT_TYPE;
			} else if (
				!__xrtHttpClientCacheMultipartTypeEqual(
					&SelectedType,
					&MultipartPart.ContentType
				)
			) {
				return __xrtHttpClientCacheMultipartSkip(
					pParts,
					__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_CONTENT_TYPE,
					XERR_VALUE,
					pPlan
				);
			}
		}

		pParts[iCount].Offset =
			FragmentPlan.Fragment.Range.First;
		pParts[iCount].Data =
			MultipartPart.Body;
		iCount++;
	}
	if ( iCount == 0 ) {
		return __xrtHttpClientCacheMultipartSkip(
			pParts,
			__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_PARTS,
			XERR_VALUE,
			pPlan
		);
	}
	__xrtHttpClientCacheMultipartSort(
		pParts,
		iCount
	);
	if ( !__xrtHttpClientCacheMultipartNormalize(
		pParts,
		iCount,
		&iNormalized
	) ) {
		return __xrtHttpClientCacheMultipartSkip(
			pParts,
			__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_OVERLAP,
			XERR_VALUE,
			pPlan
		);
	}
	if ( ((iFlags &
		  __XRT_HTTP_CLIENT_CACHE_MULTIPART_HAS_LENGTH) != 0) &&
		!__xrtHttpClientCacheMultipartWithinLength(
			pParts,
			iNormalized,
			iLength
		) ) {
		return __xrtHttpClientCacheMultipartSkip(
			pParts,
			__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_LENGTH,
			XERR_VALUE,
			pPlan
		);
	}
	if ( ((iFlags &
		  __XRT_HTTP_CLIENT_CACHE_MULTIPART_HAS_LENGTH) != 0) &&
		__xrtHttpClientCacheMultipartComplete(
			pParts,
			iNormalized,
			iLength
		) ) {
		iFlags |=
			__XRT_HTTP_CLIENT_CACHE_MULTIPART_COMPLETE;
	}

	memset(pPlan, 0, sizeof(*pPlan));
	pPlan->Parts = pParts;
	pPlan->ContentType = SelectedText;
	pPlan->Length = iLength;
	pPlan->PartCount = iNormalized;
	pPlan->Decision =
		__XRT_HTTP_CLIENT_CACHE_MULTIPART_STORE;
	pPlan->Flags = iFlags;
	return pPlan->Decision;
}

#endif
