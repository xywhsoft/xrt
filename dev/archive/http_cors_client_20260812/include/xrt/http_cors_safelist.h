#ifndef XRT_HTTP_CORS_SAFELIST_H
#define XRT_HTTP_CORS_SAFELIST_H

#include <xrt/http_cors.h>
#include <xrt/mime.h>



#if defined(XRT_FEATURE_HTTP_CORS_SAFELIST) && \
	(!defined(XRT_FEATURE_HTTP_CORS) || \
	 !defined(XRT_FEATURE_MIME))
	#error "XRT HTTP CORS safelist requires CORS and MIME support"
#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_HTTP_CORS_SAFELIST)

/* 判断字节是否属于 Fetch 定义的 CORS-unsafe request-header byte。 */
XRT_API bool xrtHttpCorsRequestByteUnsafe(uint8 iByte);



/* 判断区分大小写的方法是否为 GET、HEAD 或 POST。 */
XRT_API bool xrtHttpCorsMethodSafelisted(xstrview Method);



/* 判断一个已经规范化的请求字段是否属于 CORS safelist。 */
XRT_API bool xrtHttpCorsRequestHeaderSafelisted(
	xstrview Name,
	xstrview Value
);



/*
	逐项判断 HTTP header-list 中的请求字段，并累计不超过 1024 字节的 value。
	Content-Type 与 Range 是单值字段；重复项即使各自安全也不属于 safelist。
*/
XRT_API bool xrtHttpCorsRequestFieldsSafelisted(
	const xhttpfield* pFields,
	size_t iCount
);



/* Authorization 在 Allow-Headers 中必须显式列出，不能由星号匹配。 */
XRT_API bool xrtHttpCorsRequestHeaderNonWildcard(xstrview Name);



/* 判断字段名是否属于七个默认可见的 CORS 响应字段。 */
XRT_API bool xrtHttpCorsResponseHeaderSafelisted(xstrview Name);



/* 判断响应字段名是否为始终不可暴露的 Set-Cookie 或 Set-Cookie2。 */
XRT_API bool xrtHttpCorsResponseHeaderForbidden(xstrview Name);



/*
	结合 Access-Control-Expose-Headers 判断响应字段是否可见。
	凭据模式下星号按普通字段名处理；返回 ITEM、END 或字段语法 ERROR。
*/
XRT_API xhttpnext xrtHttpCorsResponseHeaderExposed(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	bool bCredentials
);

#endif



XRT_EXTERN_C_END

#endif
