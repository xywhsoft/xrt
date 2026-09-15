#ifndef XRT_ACME_DNS_CF_H
#define XRT_ACME_DNS_CF_H

#include <xrt/core.h>
#include <xrt/error.h>

#include <xrt/acme_dns.h>

#if defined(XACME_FEATURE_DNS_CF) && \
	!defined(XACME_FEATURE_ACME_DNS) || \
	!defined(XACME_FEATURE_ACME_HTTP) || \
	!defined(XRT_FEATURE_JSON) || \
	!defined(XRT_FEATURE_BUFFER)
	#error "XACME_FEATURE_DNS_CF requires acme dns, acme http transport and signing primitives"
#endif

#if defined(XACME_FEATURE_DNS_CF)

/*
	Cloudflare DNS provider（API v4，Bearer API Token）。
	Token 建议只授予目标 zone 的 Zone.DNS Edit 权限；均为借用视图。
*/
typedef struct xacmednscfconfig {
	cstr sApiToken;
	cstr sEndpoint;
} xacmednscfconfig;

#endif

struct xnetengine;

XRT_EXTERN_C_BEGIN

#if defined(XACME_FEATURE_DNS_CF)

/* 全零初始化；ApiToken 必填。 */
XRT_API void xrtAcmeDnsCfConfigInit(xacmednscfconfig* pConfig);

/* 构造 provider；内部上下文由 xrtMalloc 分配，宿主用 Unit 归还。 */
XRT_API bool xrtAcmeDnsCf(
	const xacmednscfconfig* pConfig,
	struct xnetengine* pBorrowedEngine,
	xacmednsprovider* pProvider
);

/* 释放构造时分配的内部上下文。 */
XRT_API void xrtAcmeDnsCfProviderUnit(xacmednsprovider* pProvider);

#endif

XRT_EXTERN_C_END

#endif
