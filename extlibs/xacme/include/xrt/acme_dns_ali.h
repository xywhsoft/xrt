#ifndef XRT_ACME_DNS_ALI_H
#define XRT_ACME_DNS_ALI_H

#include <xrt/core.h>
#include <xrt/error.h>

#include <xrt/acme_dns.h>

#if defined(XACME_FEATURE_DNS_ALI) && !defined(XACME_FEATURE_ACME_DNS)
	#error "XRT acme dns_ali requires XACME_FEATURE_ACME_DNS"
#endif



#if defined(XACME_FEATURE_DNS_ALI)

/*
	阿里云 DNS（alidns）provider，走 V3 签名（ACS3-HMAC-SHA256）。
	Endpoint 默认 alidns.aliyuncs.com；凭据与 Endpoint 均为借用视图，
	宿主保证存活至 Remove 完成。
*/
typedef struct xacmednaliconfig {
	cstr sAccessKeyId;
	cstr sAccessKeySecret;
	cstr sEndpoint;
	/* TXT 传播确认用的解析器；空则不自动确认。 */
	cstr sVerifyResolver;
} xacmednaliconfig;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XACME_FEATURE_DNS_ALI)

/* 全零初始化；AccessKeyId/Secret 必填。 */
XRT_API void xrtAcmeDnsAliConfigInit(xacmednaliconfig* pConfig);

/*
	构造阿里云 DNS provider。内部上下文由 xrtMalloc 分配，
	宿主用 xrtAcmeDnsAliProviderUnit 归还；凭据缺失返回 false。
*/
XRT_API bool xrtAcmeDnsAli(
	const xacmednaliconfig* pConfig,
	xacmednsprovider* pProvider
);

/* 释放构造时分配的内部上下文（Add 期间记录的 RecordId 列表等）。 */
XRT_API void xrtAcmeDnsAliProviderUnit(xacmednsprovider* pProvider);

#endif



XRT_EXTERN_C_END

#endif
