#ifndef XRT_INTERNAL_HTTP_CACHE_H
#define XRT_INTERNAL_HTTP_CACHE_H

#include "xrt_http.h"



#if defined(XRT_FEATURE_HTTP_CACHE)

/*
	无错误副作用地解析 delta-seconds。
	Quoted 表示按已经通过 quoted-string 校验的内容处理 quoted-pair。
*/
bool __xrtHttpCacheDeltaParse(
	xstrview Text,
	bool Quoted,
	bool TrimOWS,
	uint64* pSeconds
);

#endif



#if defined(XRT_FEATURE_HTTP_CACHE_TIME)

/* 饱和累加两个缓存时间，保证溢出不会使条目变年轻。 */
uint64 __xrtHttpCacheTimeAdd(
	uint64 iLeft,
	uint64 iRight
);



/* 把线路秒数转换为内部微秒，溢出时保持饱和。 */
uint64 __xrtHttpCacheTimeSeconds(uint64 iSeconds);

#endif

#endif
