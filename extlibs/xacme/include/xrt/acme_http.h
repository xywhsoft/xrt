#ifndef XRT_ACME_HTTP_H
#define XRT_ACME_HTTP_H

/*
	xacme HTTPS 传输层的裁剪闭包契约：ACME 端点访问建立在 xrt 的
	自研 TLS/网络/HTTP 栈上（一次性连接，系统或自定义信任库）。
	传输对象为内部实现细节，宿主经 xrt/acme_client.h 使用；
	本头固化依赖闭包，并提供未交付对象的延后清理入口。
*/

#include <xrt/core.h>

#if defined(XACME_FEATURE_ACME_HTTP)

#if !defined(XRT_FEATURE_BUFFER) || \
	!defined(XRT_FEATURE_ATOMIC) || \
	!defined(XRT_FEATURE_HTTP) || \
	!defined(XRT_FEATURE_HTTP1_HEAD) || \
	!defined(XRT_FEATURE_HTTP1_BODY) || \
	!defined(XRT_FEATURE_HTTP1_NET) || \
	!defined(XRT_FEATURE_NET_ENGINE) || \
	!defined(XRT_FEATURE_NET_RESOLVER) || \
	!defined(XRT_FEATURE_NET_TCP) || \
	!defined(XRT_FEATURE_NET_TCP_DIAL) || \
	!defined(XRT_FEATURE_NET_TCP_DIAL_FUTURE) || \
	!defined(XRT_FEATURE_NET_TCP_FUTURE) || \
	!defined(XRT_FEATURE_TLS_STREAM) || \
	!defined(XRT_FEATURE_TLS_STREAM_DIAL) || \
	!defined(XRT_FEATURE_TLS_STREAM_DIAL_FUTURE) || \
	!defined(XRT_FEATURE_TLS_STREAM_FUTURE) || \
	!defined(XRT_FEATURE_TLS_CLIENT) || \
	!defined(XRT_FEATURE_TLS_CLIENT_VERIFY) || \
	!defined(XRT_FEATURE_TLS_VERIFY) || \
	!defined(XRT_FEATURE_TLS_NEGOTIATE) || \
	!defined(XRT_FEATURE_TLS_POLICY) || \
	!defined(XRT_FEATURE_TLS_CONTEXT) || \
	!defined(XRT_FEATURE_TLS_RECORD) || \
	!defined(XRT_FEATURE_TLS_RECORD_AES) || \
	!defined(XRT_FEATURE_TLS_SCHEDULE_SHA256) || \
	!defined(XRT_FEATURE_TLS_SCHEDULE_SHA384) || \
	!defined(XRT_FEATURE_TLS_KEY_EXCHANGE_X25519) || \
	!defined(XRT_FEATURE_TLS_KEY_EXCHANGE_P256) || \
	!defined(XRT_FEATURE_TLS_KEY_EXCHANGE_P384) || \
	!defined(XRT_FEATURE_X509_STORE) || \
	!defined(XRT_FEATURE_X509_STORE_SYSTEM) || \
	!defined(XRT_FEATURE_FUTURE) || \
	!defined(XRT_FEATURE_FUTURE_BRIDGE) || \
	!defined(XRT_FEATURE_CANCEL) || \
	!defined(XRT_FEATURE_THREAD) || \
	!defined(XRT_FEATURE_TIME)
	#error "XACME_FEATURE_ACME_HTTP requires the xrt TLS/net/HTTP stack"
#endif

/* 传输错误域 "xrt.acme.http" 的稳定代码。 */
typedef enum xacmehttperror {
	XACME_HTTP_ERROR_ARGUMENT = 1,
	XACME_HTTP_ERROR_URL,
	XACME_HTTP_ERROR_CONNECT,
	XACME_HTTP_ERROR_SEND,
	XACME_HTTP_ERROR_PROTOCOL,
	XACME_HTTP_ERROR_TIMEOUT,
	XACME_HTTP_ERROR_TLS,
	/* 请求已开始发送，服务端是否执行了写操作无法判定。 */
	XACME_HTTP_ERROR_UNCERTAIN
} xacmehttperror;

XRT_EXTERN_C_BEGIN

/*
	重试未交付的工厂/一站式临时对象的延后退休。队列跨线程可用，
	入列不分配内存；宿主须先停止新调用、等待在途调用结束，清理
	已交付实例，再于退出或卸载库前调用至 true。false 时保留库及
	相关运行环境，稍后重试；不启动后台清理线程。
	uTimeoutUs == 0 为一次非阻塞轮询；非零为本次等待预算，退休
	错误会提前结束。true 表示队列及其他清理调用正在处理的对象全部
	释放；false 表示仍有对象。piPending 可为空，否则返回未完成数量。
	保留调用前已有诊断；无旧诊断时报告退休错误或等待超时，非阻塞
	BUSY 不制造错误。返回值和数量是清理状态的依据。
	此入口仅处理未交付对象；已交付客户端/provider 由各自清理入口负责。
	有未完成对象时，新的私有引擎构造先尝试非阻塞清理，仍未完成
	则以 XERR_STATE 拒绝；借用引擎的构造不受此限制。
*/
XRT_API bool xrtAcmeCleanupPending(uint64 uTimeoutUs, size_t* piPending);

XRT_EXTERN_C_END

#endif

#endif
