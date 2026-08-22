#include "../internal/xrt_http.h"

#include <xrt/http_encoding.h>



#if defined(XRT_FEATURE_HTTP_ENCODING)

#define XRT_HTTP_ACCEPT_ENCODING_FLAGS \
	((uint32)XHTTP_ACCEPT_ENCODING_PRESENT | \
	 (uint32)XHTTP_ACCEPT_ENCODING_GZIP | \
	 (uint32)XHTTP_ACCEPT_ENCODING_DEFLATE | \
	 (uint32)XHTTP_ACCEPT_ENCODING_IDENTITY | \
	 (uint32)XHTTP_ACCEPT_ENCODING_WILDCARD)

#define XRT_HTTP_CODING_FLAGS \
	((uint32)XHTTP_CODING_IDENTITY | \
	 (uint32)XHTTP_CODING_GZIP | \
	 (uint32)XHTTP_CODING_DEFLATE)



/* 验证可继续合并的公开协商状态。 */
XRT_API bool xrtHttpAcceptEncodingValid(
	const xhttpacceptencoding* pAccept
)
{
	return (pAccept != NULL) &&
		((pAccept->Flags &
		  ~XRT_HTTP_ACCEPT_ENCODING_FLAGS) == 0) &&
		(pAccept->Gzip <= XHTTP_QUALITY_MAX) &&
		(pAccept->Deflate <= XHTTP_QUALITY_MAX) &&
		(pAccept->Identity <= XHTTP_QUALITY_MAX) &&
		(pAccept->Wildcard <= XHTTP_QUALITY_MAX);
}



/* 用较高质量合并一个显式内置编码成员。 */
static void __xrtHttpAcceptEncodingSet(
	xhttpacceptencoding* pAccept,
	xstrview Coding,
	uint16 iQuality
)
{
	uint16* pValue = NULL;
	uint32 iFlag = 0;

	if ( xrtHttpTokenEqual(
		Coding, XRT_STR_LITERAL("gzip")
	) ) {
		pValue = &pAccept->Gzip;
		iFlag = XHTTP_ACCEPT_ENCODING_GZIP;
	} else if ( xrtHttpTokenEqual(
		Coding, XRT_STR_LITERAL("deflate")
	) ) {
		pValue = &pAccept->Deflate;
		iFlag = XHTTP_ACCEPT_ENCODING_DEFLATE;
	} else if ( xrtHttpTokenEqual(
		Coding, XRT_STR_LITERAL("identity")
	) ) {
		pValue = &pAccept->Identity;
		iFlag = XHTTP_ACCEPT_ENCODING_IDENTITY;
	} else if ( (Coding.Size == 1u) &&
		(Coding.Data[0] == '*') ) {
		pValue = &pAccept->Wildcard;
		iFlag = XHTTP_ACCEPT_ENCODING_WILDCARD;
	}
	if ( pValue == NULL ) {
		return;
	}
	if ( ((pAccept->Flags & iFlag) == 0) ||
		(iQuality > *pValue) ) {
		*pValue = iQuality;
	}
	pAccept->Flags |= iFlag;
}



/* 返回已验证状态中一个内置编码的有效质量。 */
static uint16 __xrtHttpAcceptEncodingQuality(
	const xhttpacceptencoding* pAccept,
	xhttpcoding Coding
)
{
	if ( (pAccept->Flags &
		  XHTTP_ACCEPT_ENCODING_PRESENT) == 0 ) {
		return XHTTP_QUALITY_MAX;
	}
	if ( Coding == XHTTP_CODING_GZIP ) {
		if ( (pAccept->Flags &
			  XHTTP_ACCEPT_ENCODING_GZIP) != 0 ) {
			return pAccept->Gzip;
		}
		return (pAccept->Flags &
			XHTTP_ACCEPT_ENCODING_WILDCARD) != 0 ?
				pAccept->Wildcard : 0;
	}
	if ( Coding == XHTTP_CODING_DEFLATE ) {
		if ( (pAccept->Flags &
			  XHTTP_ACCEPT_ENCODING_DEFLATE) != 0 ) {
			return pAccept->Deflate;
		}
		return (pAccept->Flags &
			XHTTP_ACCEPT_ENCODING_WILDCARD) != 0 ?
				pAccept->Wildcard : 0;
	}
	if ( (pAccept->Flags &
		  XHTTP_ACCEPT_ENCODING_IDENTITY) != 0 ) {
		return pAccept->Identity;
	}
	return ((pAccept->Flags &
			 XHTTP_ACCEPT_ENCODING_WILDCARD) != 0) &&
		(pAccept->Wildcard == 0) ? 0 : XHTTP_QUALITY_MAX;
}



/* 初始化 Header 缺失状态。 */
XRT_API void xrtHttpAcceptEncodingInit(
	xhttpacceptencoding* pAccept
)
{
	if ( pAccept == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(pAccept, 0, sizeof(*pAccept));
}



/* 失败原子地合并一个字段值。 */
XRT_API bool xrtHttpAcceptEncodingAdd(
	xhttpacceptencoding* pAccept,
	xstrview Value
)
{
	xhttpacceptencoding Accept;
	xhttpweightedtoken Item;
	xhttpnext Next;
	size_t iOffset = 0;

	if ( !xrtHttpAcceptEncodingValid(pAccept) ||
		!__xrtHttpViewValid(Value) ||
		__xrtRangesOverlap(
			pAccept, sizeof(*pAccept),
			Value.Data, Value.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Accept = *pAccept;
	Accept.Flags |= XHTTP_ACCEPT_ENCODING_PRESENT;
	for ( ;; ) {
		Next = xrtHttpWeightedTokenNext(
			Value, &iOffset, &Item
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			*pAccept = Accept;
			return true;
		}
		__xrtHttpAcceptEncodingSet(
			&Accept, Item.Token, Item.Quality
		);
	}
}



/* 扫描完整字段集合中的 Accept-Encoding。 */
XRT_API bool xrtHttpAcceptEncodingParse(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpacceptencoding* pAccept
)
{
	xhttpacceptencoding Accept;
	size_t i;

	if ( ((pFields == NULL) && (iCount != 0)) ||
		(pAccept == NULL) ||
		(iCount > (SIZE_MAX / sizeof(*pFields))) ||
		__xrtRangesOverlap(
			pFields, iCount * sizeof(*pFields),
			pAccept, sizeof(*pAccept)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	xrtHttpAcceptEncodingInit(&Accept);
	for ( i = 0; i < iCount; i++ ) {
		if ( xrtHttpFieldNameEqual(
			pFields[i].Name,
			XRT_STR_LITERAL("Accept-Encoding")
		) && !xrtHttpAcceptEncodingAdd(
			&Accept, pFields[i].Value
		) ) {
			return false;
		}
	}
	*pAccept = Accept;
	return true;
}



/* 查询一个内置编码的有效质量。 */
XRT_API uint16 xrtHttpAcceptEncodingQuality(
	const xhttpacceptencoding* pAccept,
	xhttpcoding Coding
)
{
	if ( !xrtHttpAcceptEncodingValid(pAccept) ||
		((Coding != XHTTP_CODING_IDENTITY) &&
		 (Coding != XHTTP_CODING_GZIP) &&
		 (Coding != XHTTP_CODING_DEFLATE)) ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	return __xrtHttpAcceptEncodingQuality(pAccept, Coding);
}



/* 选择质量最高且由调用方提供的内置表示。 */
XRT_API xhttpcoding xrtHttpAcceptEncodingSelect(
	const xhttpacceptencoding* pAccept,
	uint32 iAvailable,
	xhttpcoding Preferred
)
{
	static const xhttpcoding Order[] = {
		XHTTP_CODING_GZIP,
		XHTTP_CODING_DEFLATE,
		XHTTP_CODING_IDENTITY
	};
	xhttpcoding Selected = XHTTP_CODING_NONE;
	uint16 iSelected = 0;
	size_t i;

	if ( !xrtHttpAcceptEncodingValid(pAccept) ||
		((iAvailable & ~XRT_HTTP_CODING_FLAGS) != 0) ||
		((Preferred != XHTTP_CODING_NONE) &&
		 (Preferred != XHTTP_CODING_IDENTITY) &&
		 (Preferred != XHTTP_CODING_GZIP) &&
		 (Preferred != XHTTP_CODING_DEFLATE)) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_CODING_NONE;
	}
	if ( ((iAvailable & (uint32)Preferred) != 0) &&
		(Preferred != XHTTP_CODING_NONE) ) {
		iSelected = __xrtHttpAcceptEncodingQuality(
			pAccept, Preferred
		);
		if ( iSelected != 0 ) {
			Selected = Preferred;
		}
	}
	for ( i = 0; i < (sizeof(Order) / sizeof(Order[0])); i++ ) {
		uint16 iQuality;

		if ( (iAvailable & (uint32)Order[i]) == 0 ) {
			continue;
		}
		iQuality = __xrtHttpAcceptEncodingQuality(
			pAccept, Order[i]
		);
		if ( iQuality > iSelected ) {
			iSelected = iQuality;
			Selected = Order[i];
		}
	}
	return Selected;
}



/* 把内置编码映射为规范线缆 token。 */
XRT_API xstrview xrtHttpCodingName(xhttpcoding Coding)
{
	switch ( Coding ) {
		case XHTTP_CODING_IDENTITY:
			return XRT_STR_LITERAL("identity");

		case XHTTP_CODING_GZIP:
			return XRT_STR_LITERAL("gzip");

		case XHTTP_CODING_DEFLATE:
			return XRT_STR_LITERAL("deflate");

		default:
			return (xstrview){ NULL, 0 };
	}
}



/* 把内置内容编码 token 解析为统一枚举。 */
XRT_API xhttpcoding xrtHttpCodingParse(xstrview Token)
{
	if ( xrtHttpTokenEqual(
		Token,
		XRT_STR_LITERAL("identity")
	) ) {
		return XHTTP_CODING_IDENTITY;
	}
	if ( xrtHttpTokenEqual(
		Token,
		XRT_STR_LITERAL("gzip")
	) || xrtHttpTokenEqual(
		Token,
		XRT_STR_LITERAL("x-gzip")
	) ) {
		return XHTTP_CODING_GZIP;
	}
	if ( xrtHttpTokenEqual(
		Token,
		XRT_STR_LITERAL("deflate")
	) ) {
		return XHTTP_CODING_DEFLATE;
	}
	return XHTTP_CODING_NONE;
}

#endif
