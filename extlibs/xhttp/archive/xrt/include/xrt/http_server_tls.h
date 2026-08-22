#ifndef XRT_HTTP_SERVER_TLS_H
#define XRT_HTTP_SERVER_TLS_H

#include <xrt/http_server_runtime.h>
#include <xrt/tls_stream.h>



#if defined(XRT_FEATURE_HTTP_SERVER_TLS) && \
	(!defined(XRT_FEATURE_HTTP_SERVER) || \
	 !defined(XRT_FEATURE_TLS_STREAM))
	#error "XRT HTTP server TLS requires HTTP server and TLS Stream support"
#endif



#if defined(XRT_FEATURE_HTTP_SERVER_TLS)

/*
	Handshake 的 Context、Identity 和 Protocols 在启动时保存独立所有权。
	SelectContext 和 ResumeContext 由调用方保持到 Server 完全关闭。
*/
typedef struct xhttpservertlsconfig {
	xtlsserverconfig Handshake;
	xtlsstreamconfig Stream;
} xhttpservertlsconfig;



XRT_EXTERN_C_BEGIN



/*
	初始化 HTTP/1.1 ALPN、TLS 服务端和组合 Stream 默认策略。
	输出结构可以未对齐，但必须是完整且不回绕的可写范围。
*/
XRT_API void xrtHttpServerTlsConfigInit(
	xhttpservertlsconfig* pConfig
);



/*
	创建并立即启动 TLS HTTP/1 Server。
	启动时只读取一次 TLS 配置；固定结构与 ALPN 描述符均允许未对齐，
	但必须覆盖完整且不回绕的范围。ALPN 字节会被深复制，TLS 对象会被保留；
	选择器与恢复回调的上下文仍由调用方保持到 Server 完全关闭。
*/
XRT_API xhttpserver* xrtHttpServerStartTls(
	xnetengine* pEngine,
	const xhttpserverconfig* pConfig,
	const xhttpservertlsconfig* pTls,
	const xhttpserverevents* pEvents
);



/* 只在 Connection Worker 回调内返回借用 TLS Stream。 */
XRT_API xtlsstream* xrtHttpConnTls(xhttpconn* pConnection);



XRT_EXTERN_C_END

#endif

#endif
