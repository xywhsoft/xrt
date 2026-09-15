#ifndef XRT_ACME_DNS_AWS_H
#define XRT_ACME_DNS_AWS_H

#include <xrt/core.h>
#include <xrt/error.h>

#include <xrt/acme_dns.h>

#if defined(XACME_FEATURE_DNS_AWS) && \
	!defined(XACME_FEATURE_ACME_DNS) || \
	!defined(XACME_FEATURE_ACME_HTTP) || \
	!defined(XRT_FEATURE_CRYPTO_SHA256) || \
	!defined(XRT_FEATURE_CRYPTO_HMAC_SHA256) || \
	!defined(XRT_FEATURE_TIME) || \
	!defined(XRT_FEATURE_BUFFER)
	#error "XACME_FEATURE_DNS_AWS requires acme dns, acme http transport and signing primitives"
#endif

#if defined(XACME_FEATURE_DNS_AWS)

/*
	AWS Route53 provider（SigV4，XML API 2013-03-01）。
	Region 可空（Route53 为全局服务，默认 us-east-1）；
	凭据建议为仅限 Route53 的 IAM 用户/角色。
*/
typedef struct xacmednsawsconfig {
	cstr sAccessKeyId;
	cstr sSecretAccessKey;
	cstr sRegion;
	cstr sEndpoint;
} xacmednsawsconfig;

#endif

struct xnetengine;

XRT_EXTERN_C_BEGIN

#if defined(XACME_FEATURE_DNS_AWS)

/* 全零初始化；AccessKeyId/SecretAccessKey 必填。 */
XRT_API void xrtAcmeDnsAwsConfigInit(xacmednsawsconfig* pConfig);

/* 构造 provider；内部上下文由 xrtMalloc 分配，宿主用 Unit 归还。 */
XRT_API bool xrtAcmeDnsAws(
	const xacmednsawsconfig* pConfig,
	struct xnetengine* pBorrowedEngine,
	xacmednsprovider* pProvider
);

/* 释放构造时分配的内部上下文。 */
XRT_API void xrtAcmeDnsAwsProviderUnit(xacmednsprovider* pProvider);

#endif

XRT_EXTERN_C_END

#endif
