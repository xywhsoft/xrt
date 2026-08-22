#ifndef XRT_HTTP_ROUTER_H
#define XRT_HTTP_ROUTER_H

#include <xrt/http_route.h>



#if defined(XHTTP_FEATURE_HTTP_ROUTER) && \
	!defined(XHTTP_FEATURE_HTTP_ROUTE)
	#error "XRT HTTP router support requires HTTP route support"
#endif



#if defined(XHTTP_FEATURE_HTTP_ROUTER)

/* Router 是注册阶段可变、冻结后只读并发的拥有型路由索引。 */
typedef struct xhttprouter xhttprouter;



/* Router 结果明确区分调用错误、404、405、完整命中和捕获容量不足。 */
typedef enum xhttprouterstatus {
	XHTTP_ROUTER_ERROR = -1,
	XHTTP_ROUTER_NOT_FOUND = 0,
	XHTTP_ROUTER_MATCH = 1,
	XHTTP_ROUTER_MORE = 2,
	XHTTP_ROUTER_METHOD_NOT_ALLOWED = 3
} xhttprouterstatus;



/* HEAD 使用同一路径的 GET 路由时记录显式回退事实。 */
#define XHTTP_ROUTER_HEAD_FALLBACK UINT32_C(0x00000001)

/* 命中注册方法为星号的任意方法路由。 */
#define XHTTP_ROUTER_ANY_METHOD UINT32_C(0x00000002)



/* 显式限额约束注册规模；全部字节包含模板、方法和静态边副本。 */
typedef struct xhttprouterconfig {
	size_t MaxRoutes;
	size_t MaxNodes;
	size_t MaxBytes;
} xhttprouterconfig;



/* 命中结果借用冻结 Router 中的方法和模板，Value 保留注册时的借用指针。 */
typedef struct xhttproutermatch {
	uint32 Flags;
	xstrview Method;
	xstrview Pattern;
	ptr Value;
} xhttproutermatch;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_ROUTER)

/* 初始化最多 4096 条路由、16384 个节点和 4 MiB 文本的默认配置；存储可以未对齐。 */
XRT_API void xrtHttpRouterConfigInit(xhttprouterconfig* pConfig);



/* 创建空 Router；配置可以未对齐，并在返回前被按值持有。 */
XRT_API xhttprouter* xrtHttpRouterCreate(
	const xhttprouterconfig* pConfig
);



/* 销毁 Router 自身资源；注册 Value 始终由调用方持有。 */
XRT_API void xrtHttpRouterDestroy(xhttprouter* pRouter);



/*
	复制并注册一个方法与模板；Method 为 "*" 时匹配任意合法请求方法。
	同一结构路径不能重复注册同一方法，参数名可以按方法分别定义。
*/
XRT_API bool xrtHttpRouterAdd(
	xhttprouter* pRouter,
	xstrview Method,
	xstrview Pattern,
	ptr pValue
);



/*
	冻结静态边并允许无锁并发匹配；重复调用是安全的。
	冻结失败时 Router 仍保持可注册状态，成功后不能继续 Add。
*/
XRT_API bool xrtHttpRouterFreeze(xhttprouter* pRouter);



/* 判断 Router 是否已经进入不可变匹配阶段。 */
XRT_API bool xrtHttpRouterFrozen(const xhttprouter* pRouter);



/* 返回当前注册路由数量。 */
XRT_API size_t xrtHttpRouterCount(const xhttprouter* pRouter);



/* 返回当前预编译节点数量，空 Router 仍包含根节点。 */
XRT_API size_t xrtHttpRouterNodes(const xhttprouter* pRouter);



/* 返回当前实际持有的方法、模板和静态边文本字节数。 */
XRT_API size_t xrtHttpRouterBytes(const xhttprouter* pRouter);



/*
	在冻结 Router 中按静态、参数、尾参数优先级匹配方法与原始路径。
	容量不足返回 MORE，Match 和 Count 有效，但不会写入任何 Param。
	固定输出和捕获数组支持未对齐存储，参数错误不修改任何输出。
	所有输出及 Method、Path 不得相互重叠。
*/
XRT_API xhttprouterstatus xrtHttpRouterMatch(
	const xhttprouter* pRouter,
	xstrview Method,
	xstrview Path,
	xhttprouteparam* pParams,
	size_t iCapacity,
	size_t* pCount,
	xhttproutermatch* pMatch
);



/*
	列出至少一条匹配结构路径允许的唯一注册方法，并为 GET 合成 HEAD。
	容量不足返回 MORE 和精确需求，且不写入任何 Method；结果均借用 Router。
	方法数组和 Count 支持未对齐存储，参数错误不修改任何输出。
*/
XRT_API xhttprouterstatus xrtHttpRouterMethods(
	const xhttprouter* pRouter,
	xstrview Path,
	xstrview* pMethods,
	size_t iCapacity,
	size_t* pCount
);

#endif



XRT_EXTERN_C_END

#endif
