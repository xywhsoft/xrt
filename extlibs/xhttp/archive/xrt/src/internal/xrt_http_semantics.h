#ifndef XRT_INTERNAL_HTTP_SEMANTICS_H
#define XRT_INTERNAL_HTTP_SEMANTICS_H

#include "xrt_http.h"

#include <xrt/http_semantics.h>



#if defined(XRT_FEATURE_HTTP_ETAG)

/* 无错误副作用地验证实体标签正文。 */
bool __xrtHttpETagValid(const xhttpetag* pTag);



/* 无错误副作用地解析一个完整实体标签。 */
bool __xrtHttpETagParseValue(
	xstrview Text,
	xhttpetag* pTag
);

#endif



#if defined(XRT_FEATURE_HTTP_PRECONDITION)

/* 验证条件评估输入并快照当前表示，兼容未对齐的固定结构与字段数组。 */
bool __xrtHttpPreconditionInputRead(
	xstrview Method,
	const xhttpfield* pFields,
	size_t iFieldCount,
	const xhttprepresentation* pInput,
	xhttprepresentation* pCurrent
);

#endif



#if defined(XRT_FEATURE_HTTP_RANGE)

/* 无错误副作用地拆分一个完整 Range 字段值。 */
bool __xrtHttpRangeParseValue(
	xstrview Value,
	xstrview* pUnit,
	xstrview* pSet
);



/* 无错误副作用地解析一个完整 bytes Content-Range 字段值。 */
bool __xrtHttpContentRangeParseValue(
	xstrview Value,
	xhttpcontentrange* pRange
);



/* 无错误副作用地迭代已经验证视图的 byte-range-set。 */
xhttpnext __xrtHttpByteRangeNextValue(
	xstrview Set,
	size_t* pOffset,
	xhttprangespec* pSpec
);

#endif



#if defined(XRT_FEATURE_HTTP_RANGE_MULTIPART)

/* 验证并计算 canonical multipart/byteranges 正文长度。 */
bool __xrtHttpRangeMultipartLength(
	const xhttpbyterange* pRanges,
	size_t iRangeCount,
	uint64 iCompleteLength,
	xstrview ContentType,
	xstrview Boundary,
	uint64* pLength
);

#endif



#endif
