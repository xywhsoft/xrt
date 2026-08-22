#ifndef XRT_INTERNAL_HTTP_SERVER_MUX_H
#define XRT_INTERNAL_HTTP_SERVER_MUX_H

#include "xrt_http_server_router.h"
#include "xrt_http_server_runtime.h"

#include <xrt/http_server_mux.h>



#if defined(XRT_FEATURE_HTTP_SERVER_MUX)

/* Host 表按 ASCII 不区分大小写排序，Entry 独立持有文本和 Router 引用。 */
typedef struct xrt_http_server_mux_entry {
	str Host;
	size_t Size;
	xhttpserverrouter* Router;
} xrt_http_server_mux_entry;



/* Mux 用读写锁保护热替换表，普通 Router 本身继续保持冻结只读。 */
struct xhttpservermux {
	volatile int32 References;
	xrwlock Lock;
	xhttpservermuxconfig Config;
	xrt_http_server_mux_entry* Entries;
	size_t Count;
	size_t Capacity;
	size_t HostBytes;
	xhttpserverrouter* Default;
};



/* 每个 Mux Connection 固定当前请求选择的 Router，直到下一请求或关闭。 */
typedef struct xrt_http_server_mux_connection {
	xhttpserverrouter* Router;
	xrt_http_server_route_match* Match;
} xrt_http_server_mux_connection;



/* Server 适配器独立持有 Mux，并保存调用方原始事件。 */
typedef struct xrt_http_server_mux_runtime {
	xhttpservermux* Mux;
	xhttpserverevents Events;
} xrt_http_server_mux_runtime;



/* 设置稳定 Mux 错误及可选原因链。 */
void __xrtHttpServerMuxSetError(
	xerrkind Kind,
	xhttpservermuxerror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
);



/* 包装当前线程错误并保留原始原因链。 */
void __xrtHttpServerMuxWrapError(
	xerrkind Default,
	xhttpservermuxerror Code,
	cstr sOperation,
	cstr sMessage
);



/* 使用已解析的非空 Host 选择并保留精确或默认 Router。 */
xhttpservermuxstatus __xrtHttpServerMuxSelect(
	xhttpservermux* pMux,
	xstrview Host,
	xhttpserverrouter** ppRouter
);



/* 建立供明文和 TLS Start 共用的 Server 事件适配器。 */
xrt_http_server_mux_runtime* __xrtHttpServerMuxRuntimeCreate(
	xhttpservermux* pMux,
	const xhttpserverevents* pEvents,
	xhttpserverevents* pOutput
);



/* 释放尚未交给 Server 或已经完成 Shutdown 的适配器。 */
void __xrtHttpServerMuxRuntimeDestroy(
	xrt_http_server_mux_runtime* pRuntime
);

#endif

#endif
