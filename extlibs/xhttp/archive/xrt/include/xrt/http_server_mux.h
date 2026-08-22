#ifndef XRT_HTTP_SERVER_MUX_H
#define XRT_HTTP_SERVER_MUX_H

#include <xrt/http_server_router.h>
#include <xrt/sync.h>



#if defined(XRT_FEATURE_HTTP_SERVER_MUX) && \
	(!defined(XRT_FEATURE_HTTP_SERVER_ROUTER) || \
	 !defined(XRT_FEATURE_RWLOCK))
	#error "XRT HTTP server mux requires server router and rwlock support"
#endif



#if defined(XRT_FEATURE_HTTP_SERVER_MUX)

typedef struct xhttpservermux xhttpservermux;



/* Mux 查找明确区分错误、未配置、Host 命中和默认站点命中。 */
typedef enum xhttpservermuxstatus {
	XHTTP_SERVER_MUX_ERROR = -1,
	XHTTP_SERVER_MUX_NOT_FOUND = 0,
	XHTTP_SERVER_MUX_HOST = 1,
	XHTTP_SERVER_MUX_DEFAULT = 2
} xhttpservermuxstatus;



/* Mux 错误稳定区分参数、Host、状态、限额、内存、锁和启动阶段。 */
typedef enum xhttpservermuxerror {
	XHTTP_SERVER_MUX_ERROR_ARGUMENT = 1,
	XHTTP_SERVER_MUX_ERROR_HOST,
	XHTTP_SERVER_MUX_ERROR_STATE,
	XHTTP_SERVER_MUX_ERROR_LIMIT,
	XHTTP_SERVER_MUX_ERROR_MEMORY,
	XHTTP_SERVER_MUX_ERROR_LOCK,
	XHTTP_SERVER_MUX_ERROR_CONTEXT,
	XHTTP_SERVER_MUX_ERROR_START,
	XHTTP_SERVER_MUX_ERROR_INTERNAL
} xhttpservermuxerror;



/* 显式限制虚拟主机数量与复制 Host 文本总量。 */
typedef struct xhttpservermuxconfig {
	size_t MaxHosts;
	size_t MaxHostBytes;
} xhttpservermuxconfig;



/* Mux 统计是加读锁取得的一致配置快照。 */
typedef struct xhttpservermuxstats {
	size_t Hosts;
	size_t HostBytes;
	bool HasDefault;
} xhttpservermuxstats;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_HTTP_SERVER_MUX)

/* 初始化最多 256 个 Host 和 64 KiB Host 文本的默认配置；存储可以未对齐。 */
XRT_API void xrtHttpServerMuxConfigInit(
	xhttpservermuxconfig* pConfig
);



/* 创建可并发查询并允许运行时替换 Router 的空 Mux；配置被立即快照。 */
XRT_API xhttpservermux* xrtHttpServerMuxCreate(
	const xhttpservermuxconfig* pConfig
);



/* 增加 Mux 引用；运行中的 Server 会独立持有一份引用。 */
XRT_API xhttpservermux* xrtHttpServerMuxRef(
	xhttpservermux* pMux
);



/* 释放 Mux 引用；最后一个引用释放全部 Router 与 Host 副本。 */
XRT_API void xrtHttpServerMuxDestroy(
	xhttpservermux* pMux
);



/* 设置或热替换默认站点；Router 必须已经冻结。 */
XRT_API bool xrtHttpServerMuxDefault(
	xhttpservermux* pMux,
	xhttpserverrouter* pRouter
);



/*
	设置或热替换一个精确 Host；比较忽略 ASCII 大小写和请求端口。
	Host 使用 HTTP Host 语法但不得包含端口，IPv6 字面量必须使用方括号。
*/
XRT_API bool xrtHttpServerMuxHost(
	xhttpservermux* pMux,
	xstrview Host,
	xhttpserverrouter* pRouter
);



/* 移除精确 Host；不存在是正常 NOT_FOUND，不设置错误。 */
XRT_API xhttpservermuxstatus xrtHttpServerMuxRemove(
	xhttpservermux* pMux,
	xstrview Host
);



/*
	按 Host 查询精确或默认 Router，并通过输出返回一份拥有型引用。
	Host 使用与注册相同的语法；NOT_FOUND 时输出保持为空。
	Router 输出可以位于完整但未对齐的存储中，但不得回绕或覆盖 Mux 与 Host。
*/
XRT_API xhttpservermuxstatus xrtHttpServerMuxMatch(
	xhttpservermux* pMux,
	xstrview Host,
	xhttpserverrouter** ppRouter
);



/*
	复制 Host 数量、文本字节数和默认站点状态的一致快照。
	输出可以未对齐，但不得回绕或覆盖 Mux 对象。
*/
XRT_API bool xrtHttpServerMuxStats(
	xhttpservermux* pMux,
	xhttpservermuxstats* pStats
);



/*
	启动按有效请求 Host 选择 Router 的明文 HTTP/1 Server。
	每个请求固定一次 Router 引用，热替换只影响之后开始的请求。
*/
XRT_API xhttpserver* xrtHttpServerMuxStart(
	xnetengine* pEngine,
	const xhttpserverconfig* pConfig,
	xhttpservermux* pMux,
	const xhttpserverevents* pEvents
);

#endif



XRT_EXTERN_C_END

#endif
