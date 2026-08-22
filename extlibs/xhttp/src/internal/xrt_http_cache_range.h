#ifndef XRT_INTERNAL_HTTP_CACHE_RANGE_H
#define XRT_INTERNAL_HTTP_CACHE_RANGE_H

#include "xrt_http_cache_validate.h"

#include <xrt/http_cache_range.h>



#if defined(XHTTP_FEATURE_HTTP_CACHE_RANGE)

/* 判断闭区间是否合法。 */
bool __xrtHttpCacheRangeValid(
	const xhttpbyterange* pRange
);



/* 判断两个闭区间是否重叠或相邻。 */
bool __xrtHttpCacheRangeJoins(
	const xhttpbyterange* pLeft,
	const xhttpbyterange* pRight
);



#endif

#endif
