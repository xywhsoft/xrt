#ifndef XRT_HTTP_TRAILER_H
#define XRT_HTTP_TRAILER_H

#include <xrt/http.h>



#if defined(XRT_FEATURE_HTTP_TRAILER) && \
	!defined(XRT_FEATURE_HTTP)
	#error "XRT HTTP Trailer support requires HTTP support"
#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_HTTP_TRAILER)

/* 判断字段名是否可作为通用 HTTP trailer 发送。 */
XRT_API bool xrtHttpTrailerNameValid(xstrview Name);



/* 完整验证实际 trailer section 的字段名称和值。 */
XRT_API bool xrtHttpTrailerSectionValid(
	const xhttpfield* pTrailers,
	size_t iCount
);



/* 完整验证重复 Trailer 字段行并统计其中声明的名称。 */
XRT_API bool xrtHttpTrailerCount(
	const xhttpfield* pFields,
	size_t iCount,
	size_t* pNameCount
);



/* 查找已声明的 trailer 字段名；返回 ITEM、END 或 ERROR。 */
XRT_API xhttpnext xrtHttpTrailerFind(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name
);



/*
	从实际 trailer 字段写出规范的 Trailer 声明值。
	同名字段按 ASCII 大小写不敏感规则去重，保留首次出现的名称与顺序。
	空输出可精确查询长度；输出不得与字段描述符或借用视图重叠。
*/
XRT_API bool xrtHttpTrailerNamesWrite(
	const xhttpfield* pTrailers,
	size_t iTrailerCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 构建零结尾的 Trailer 声明值，返回值由 xrtFree 释放。 */
XRT_API str xrtHttpTrailerNamesBuild(
	const xhttpfield* pTrailers,
	size_t iTrailerCount,
	size_t* pSize
);

#endif



XRT_EXTERN_C_END

#endif
