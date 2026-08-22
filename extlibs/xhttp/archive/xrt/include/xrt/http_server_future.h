#ifndef XRT_HTTP_SERVER_FUTURE_H
#define XRT_HTTP_SERVER_FUTURE_H

#include <xrt/future.h>
#include <xrt/http_server_runtime.h>



#if defined(XRT_FEATURE_HTTP_SERVER_FUTURE) && \
	(!defined(XRT_FEATURE_HTTP_SERVER) || \
	 !defined(XRT_FEATURE_FUTURE_CONTINUE))
	#error "XRT HTTP server Future support requires HTTP server and Future continuation support"
#endif



#if defined(XRT_FEATURE_HTTP_SERVER_FUTURE)

XRT_EXTERN_C_BEGIN



/*
	等待 Server 进入 CLOSED 终态。
	全部 HTTP Connection 的协议、传输所有权和内部运行时均已退出，Shutdown 与已经受理的
	Upgrade 交接回调也已返回；已经转交的新协议会话可以仍然存活。
	每次调用建立独立等待；取消 Future 只摘除本次等待，不会排空或中止 Server。
	Server 已经关闭时返回立即成功的 Future。
*/
XRT_API xfuture* xrtHttpServerWaitAsync(xhttpserver* pServer);



/*
	在 Connection Worker 上把当前请求的唯一最终响应绑定到 Future。

	Future 成功值必须是非空的借用 xhttpreply*。桥接层持有源 Future，并在所属
	Worker 上冻结 Reply；调用方或 Future 的值析构过程仍拥有 Reply。源 Future
	可以来自 Promise、任务池或协程，不需要 HTTP 专用任务类型。

	连接关闭、请求超时或其他最终响应先提交时会请求取消源 Future；取消是协作式
	通知，不要求生产者立即终止。已经投递的完成与手工响应由唯一最终响应门收敛。
	失败、超时和取消分别生成 500、504 和 503 兜底响应，并保留结构化原因。
*/
XRT_API bool xrtHttpConnRespondFuture(
	xhttpconn* pConnection,
	xfuture* pFuture
);



XRT_EXTERN_C_END

#endif

#endif
