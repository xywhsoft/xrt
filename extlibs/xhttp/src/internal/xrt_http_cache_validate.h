#ifndef XRT_INTERNAL_HTTP_CACHE_VALIDATE_H
#define XRT_INTERNAL_HTTP_CACHE_VALIDATE_H

#include "xrt_http_cache.h"
#include "xrt_http_semantics.h"

#include <xrt/http_cache_validate.h>



#if defined(XHTTP_FEATURE_HTTP_CACHE_VALIDATE)

/* 单个响应的验证元数据全部借用原 Header。 */
typedef struct xrt_http_cache_validator {
	xhttpetag ETag;
	xtime LastModified;
	xtime Date;
	uint64 ContentLength;
	bool ETagPresent;
	bool ETagValid;
	bool LastModifiedPresent;
	bool LastModifiedValid;
	bool LastModifiedStrong;
	bool DateValid;
	bool ContentLengthPresent;
	bool ContentLengthValid;
} xrt_http_cache_validator;



/* 验证借用字段数组的内存与 HTTP 名称和值语法。 */
bool __xrtHttpCacheFieldsValid(
	const xhttpfield* pFields,
	size_t iCount
);



/* 验证缓存条目数组及其中全部借用字段。 */
bool __xrtHttpCacheEntriesValid(
	const xhttpcacheentry* pEntries,
	size_t iCount
);



/* 判断一段内存是否覆盖条目数组或任一借用 Header。 */
bool __xrtHttpCacheEntriesOverlap(
	const xhttpcacheentry* pEntries,
	size_t iCount,
	const void* pMemory,
	size_t iSize
);



/* 从响应 Header 读取 ETag、日期和 Content-Length，不分配内存。 */
bool __xrtHttpCacheValidatorRead(
	const xhttpfield* pFields,
	size_t iCount,
	xrt_http_cache_validator* pValidator
);



/* 判断条目能否满足当前完整或 Range 请求。 */
bool __xrtHttpCacheEntryEligible(
	const xhttpcacheentry* pEntry,
	bool Range
);



/* 精确比较包含强弱标志的两个实体标签。 */
bool __xrtHttpCacheETagExact(
	const xhttpetag* pLeft,
	const xhttpetag* pRight
);



/* 按 HTTP 秒精度比较两个日期。 */
bool __xrtHttpCacheDateEqual(
	xtime iLeft,
	xtime iRight
);



/* 返回候选用于“最新响应”排序的 Date 或接收时间。 */
xtime __xrtHttpCacheEntryDate(
	const xhttpcacheentry* pEntry,
	const xrt_http_cache_validator* pValidator
);

#endif

#endif
