#include "../internal/xrt_http.h"

#include <xrt/http_digest.h>



#if defined(XHTTP_FEATURE_HTTP_DIGEST_WRITE)

/* 构造单成员 Dictionary 并交给通用 Structured writer。 */
static bool __xrtHttpDigestWrite(
	xstrview Algorithm,
	xhttpstructuredtype Type,
	int64 iNumber,
	xstrview Data,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xhttpstructureddictionaryentry Entry;

	memset(&Entry, 0, sizeof(Entry));
	Entry.Key = Algorithm;
	Entry.Member.Kind = XHTTP_STRUCTURED_MEMBER_ITEM;
	Entry.Member.Item.Bare.Type = Type;
	Entry.Member.Item.Bare.Number = iNumber;
	Entry.Member.Item.Bare.Data = Data;
	return xrtHttpStructuredDictionaryWrite(
		&Entry, 1u, pOutput, iCapacity, pSize
	);
}



/* 在偏好范围检查前验证全部公共内存参数。 */
static bool __xrtHttpDigestPreferenceArguments(
	xstrview Algorithm,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return __xrtHttpViewValid(Algorithm) &&
		__xrtRangeValid(pSize, sizeof(*pSize)) &&
		!__xrtRangesOverlap(
			Algorithm.Data, Algorithm.Size,
			pSize, sizeof(*pSize)
		) && ((pOutput == NULL) ? (iCapacity == 0) :
			(__xrtRangeValid(pOutput, iCapacity) &&
			 !__xrtRangesOverlap(
				pOutput, iCapacity, pSize, sizeof(*pSize)
			 ) && !__xrtRangesOverlap(
				pOutput, iCapacity,
				Algorithm.Data, Algorithm.Size
			 )));
}



/* 写出一个摘要成员。 */
XRT_API bool xrtHttpDigestWrite(
	xstrview Algorithm,
	xbytesview Digest,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return __xrtHttpDigestWrite(
		Algorithm, XHTTP_STRUCTURED_BYTES, 0,
		(xstrview){ (cstr)Digest.Data, Digest.Size },
		pOutput, iCapacity, pSize
	);
}



/* 写出一个摘要偏好成员。 */
XRT_API bool xrtHttpDigestPreferenceWrite(
	xstrview Algorithm,
	uint8 iWeight,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	if ( !__xrtHttpDigestPreferenceArguments(
		Algorithm, pOutput, iCapacity, pSize
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iWeight > 10u ) {
		__xrtErrorSetValue();
		return false;
	}
	return __xrtHttpDigestWrite(
		Algorithm, XHTTP_STRUCTURED_INTEGER,
		(int64)iWeight, (xstrview){ NULL, 0 },
		pOutput, iCapacity, pSize
	);
}

#endif
