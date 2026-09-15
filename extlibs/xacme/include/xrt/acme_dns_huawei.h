#ifndef XRT_ACME_DNS_HUAWEI_H
#define XRT_ACME_DNS_HUAWEI_H

#include <xrt/core.h>
#include <xrt/error.h>

#include <xrt/acme_dns.h>

#if defined(XACME_FEATURE_DNS_HUAWEI) && \
	!defined(XACME_FEATURE_ACME_DNS) || \
	!defined(XACME_FEATURE_ACME_HTTP) || \
	!defined(XRT_FEATURE_JSON) || \
	!defined(XRT_FEATURE_CRYPTO_SHA256) || \
	!defined(XRT_FEATURE_CRYPTO_HMAC_SHA256) || \
	!defined(XRT_FEATURE_TIME) || \
	!defined(XRT_FEATURE_BUFFER)
	#error "XACME_FEATURE_DNS_HUAWEI requires acme dns, acme http transport and signing primitives"
#endif

#if defined(XACME_FEATURE_DNS_HUAWEI)

/*
	华为云 DNS provider（API v2，SDK-HMAC-SHA256 签名）。
	凭据为 AK/SK；Endpoint 默认 dns.myhuaweicloud.com。
	注意 recordset 的 name 带尾点、records 值需内嵌双引号。
*/
typedef struct xacmednshuaaweiconfig {
	cstr sAccessKey;
	cstr sSecretKey;
	cstr sEndpoint;
} xacmednshuaaweiconfig;

#endif

struct xnetengine;

XRT_EXTERN_C_BEGIN

#if defined(XACME_FEATURE_DNS_HUAWEI)

/* 全零初始化；AccessKey/SecretKey 必填。 */
XRT_API void xrtAcmeDnsHuaweiConfigInit(xacmednshuaaweiconfig* pConfig);

/* 构造 provider；内部上下文由 xrtMalloc 分配，宿主用 Unit 归还。 */
XRT_API bool xrtAcmeDnsHuawei(
	const xacmednshuaaweiconfig* pConfig,
	struct xnetengine* pBorrowedEngine,
	xacmednsprovider* pProvider
);

/* 释放构造时分配的内部上下文。 */
XRT_API void xrtAcmeDnsHuaweiProviderUnit(xacmednsprovider* pProvider);

#endif

XRT_EXTERN_C_END

#endif
