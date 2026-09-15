#ifndef XRT_ACME_HTTP_H
#define XRT_ACME_HTTP_H

/*
	xacme HTTPS 传输层的裁剪闭包契约：ACME 端点访问建立在 xrt 的
	自研 TLS/网络/HTTP 栈上（一次性连接，系统或自定义信任库）。
	传输对象为内部实现细节，宿主经 xrt/acme_client.h 使用；
	本头只固化依赖闭包，供构建器做负向裁剪门禁。
*/

#include <xrt/core.h>

#if defined(XACME_FEATURE_ACME_HTTP)

#if !defined(XRT_FEATURE_BUFFER) || \
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

#endif

#endif
