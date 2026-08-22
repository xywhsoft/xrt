#ifndef XRT_HTTP_SERVER_UPGRADE_H
#define XRT_HTTP_SERVER_UPGRADE_H

#include <xrt/http_server_runtime.h>



#if defined(XHTTP_FEATURE_HTTP_SERVER_UPGRADE) && \
	(!defined(XHTTP_FEATURE_HTTP_SERVER) || \
	 !defined(XHTTP_FEATURE_HTTP_SERVER_RESPONSE))
	#error "XRT HTTP server Upgrade requires HTTP server response support"
#endif



#if defined(XHTTP_FEATURE_HTTP_SERVER_UPGRADE)

typedef struct xtlsstream xtlsstream;



/*
	Upgrade 成功时恰好拥有一种传输。
	Buffered 是 HTTP 停止解释后已经留在传输明文缓冲中的字节数。
*/
typedef struct xhttpupgrade {
	xnetstream* Tcp;
	xtlsstream* Tls;
	size_t Buffered;
} xhttpupgrade;



/*
	受理成功后恰好调用一次，并且不会从提交函数调用栈重入。
	成功时 Upgrade 的传输所有权交给回调；失败时两种传输都为空。
	Error 只在回调期间借用，Connection 也只保证在回调期间有效。
	Server 的 Shutdown 与关闭 Future 等待该回调返回，但不等待新协议会话关闭。
*/
typedef void (*xhttpupgradeproc)(
	xhttpconn* pConnection,
	xnetresult Result,
	xhttpupgrade Upgrade,
	const xerror* pError,
	ptr pData
);



XRT_EXTERN_C_BEGIN



/*
	异常关闭并释放尚未交给新协议对象的 Upgrade 传输。
	结构可以未对齐，但必须覆盖完整且不回绕的范围；有效清理会先清空结构，且不改变已有错误。
*/
XRT_API void xrtHttpUpgradeAbort(xhttpupgrade* pUpgrade);



/*
	以下三个提交入口只接受已经完整解析的当前请求，通常应在 Request 回调中调用。
	Headers 或 Body 回调中的请求仍不完整，提交会以 XERR_STATE 同步失败，也不会
	安排完成回调。这样传输余量只包含 HTTP 请求之后的新协议字节。
*/



/*
	提交已经拥有的 HTTP/1 Tunnel 响应计划。
	非空 Response 无论成功失败都由函数接管；只接受 101 Upgrade 或 CONNECT
	Tunnel 终态。受理后 HTTP 停止拥有该连接，并通过 Proc 交付结果。
*/
XRT_API xnetresult xrtHttpConnUpgradeResponse(
	xhttpconn* pConnection,
	xhttp1serverresponse* pResponse,
	xhttpupgradeproc pProc,
	ptr pData
);



/*
	冻结 Reply 并提交 HTTP Upgrade。
	Reply 只在调用期间借用，通常是带 Connection、Upgrade 等字段的 101 响应。
*/
XRT_API xnetresult xrtHttpConnUpgrade(
	xhttpconn* pConnection,
	const xhttpreply* pReply,
	xhttpupgradeproc pProc,
	ptr pData
);



/*
	复制并原样发送完整 HTTP/1 切换响应，然后转移传输。
	该入口不解析 Response，调用方负责状态行、Header 和终止空行完全正确。
	Response 必须覆盖完整且不回绕的非空字节范围。
*/
XRT_API xnetresult xrtHttpConnUpgradeRaw(
	xhttpconn* pConnection,
	xbytesview Response,
	xhttpupgradeproc pProc,
	ptr pData
);



XRT_EXTERN_C_END

#endif

#endif
