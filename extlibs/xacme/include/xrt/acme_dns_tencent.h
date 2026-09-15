#ifndef XRT_ACME_DNS_TENCENT_H
#define XRT_ACME_DNS_TENCENT_H

#include <xrt/core.h>
#include <xrt/error.h>

#include <xrt/acme_dns.h>

#if defined(XACME_FEATURE_DNS_TENCENT) && \
	!defined(XACME_FEATURE_ACME_DNS) || \
	!defined(XACME_FEATURE_ACME_HTTP) || \
	!defined(XRT_FEATURE_JSON) || \
	!defined(XRT_FEATURE_CRYPTO_SHA256) || \
	!defined(XRT_FEATURE_CRYPTO_HMAC_SHA256) || \
	!defined(XRT_FEATURE_TIME) || \
	!defined(XRT_FEATURE_BUFFER)
	#error "XACME_FEATURE_DNS_TENCENT requires acme dns, acme http transport and signing primitives"
#endif

#if defined(XACME_FEATURE_DNS_TENCENT)

/*
	腾讯云 DNSPod provider（API 3.0，TC3-HMAC-SHA256 签名）。
	凭据为 SecretId/SecretKey；Endpoint 默认 dnspod.tencentcloudapi.com。
*/
typedef struct xacmednstencentconfig {
	cstr sSecretId;
	cstr sSecretKey;
	cstr sEndpoint;
} xacmednstencentconfig;

#endif

struct xnetengine;

XRT_EXTERN_C_BEGIN

#if defined(XACME_FEATURE_DNS_TENCENT)

/* 全零初始化；SecretId/SecretKey 必填。 */
XRT_API void xrtAcmeDnsTencentConfigInit(xacmednstencentconfig* pConfig);

/* 构造 provider；内部上下文由 xrtMalloc 分配，宿主用 Unit 归还。 */
XRT_API bool xrtAcmeDnsTencent(
	const xacmednstencentconfig* pConfig,
	struct xnetengine* pBorrowedEngine,
	xacmednsprovider* pProvider
);

/* 释放构造时分配的内部上下文。 */
XRT_API void xrtAcmeDnsTencentProviderUnit(xacmednsprovider* pProvider);

#endif

XRT_EXTERN_C_END

#endif
