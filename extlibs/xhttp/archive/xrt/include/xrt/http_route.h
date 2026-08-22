#ifndef XRT_HTTP_ROUTE_H
#define XRT_HTTP_ROUTE_H

#include <xrt/http.h>



#if defined(XRT_FEATURE_HTTP_ROUTE) && \
	!defined(XRT_FEATURE_HTTP)
	#error "XRT HTTP route support requires HTTP support"
#endif



#if defined(XRT_FEATURE_HTTP_ROUTE)

/* 路由匹配结果区分调用错误、正常未命中、完整命中和捕获存储不足。 */
typedef enum xhttproutestatus {
	XHTTP_ROUTE_ERROR = -1,
	XHTTP_ROUTE_MISS = 0,
	XHTTP_ROUTE_MATCH = 1,
	XHTTP_ROUTE_MORE = 2
} xhttproutestatus;



/* 路由参数名称借用 Pattern，原始值借用 Path，均不保证以零结尾。 */
typedef struct xhttprouteparam {
	xstrview Name;
	xstrview Value;
} xhttprouteparam;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_HTTP_ROUTE)

/*
	验证绝对路径模板并返回捕获数量。
	{name} 匹配一个非空路径段，{name...} 只能位于末尾并匹配剩余原始路径。
*/
XRT_API bool xrtHttpRouteValidate(
	xstrview Pattern,
	size_t* pParameters
);



/*
	按字节和路径段严格匹配模板；重复斜杠与尾斜杠均有意义。
	空 Params 可查询捕获数量；容量不足返回 MORE，且不写入任何 Param。
*/
XRT_API xhttproutestatus xrtHttpRouteMatch(
	xstrview Pattern,
	xstrview Path,
	xhttprouteparam* pParams,
	size_t iCapacity,
	size_t* pCount
);



/*
	按区分大小写的名称查找首个借用参数；未找到返回空指针且不设置错误。
	若输入描述符数组未对齐，返回地址也可能未对齐，调用方应以 memcpy 读取。
*/
XRT_API const xhttprouteparam* xrtHttpRouteParam(
	const xhttprouteparam* pParams,
	size_t iCount,
	xstrview Name
);

#endif



XRT_EXTERN_C_END

#endif
