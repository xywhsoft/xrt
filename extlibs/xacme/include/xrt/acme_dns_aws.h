#ifndef XRT_ACME_DNS_AWS_H
#define XRT_ACME_DNS_AWS_H

#include <xrt/core.h>
#include <xrt/error.h>

#include <xrt/acme_dns.h>

#if defined(XACME_FEATURE_DNS_AWS) && (\
	!defined(XACME_FEATURE_ACME_DNS) || \
	!defined(XACME_FEATURE_ACME_HTTP) || \
	!defined(XRT_FEATURE_CRYPTO_SHA256) || \
	!defined(XRT_FEATURE_CRYPTO_HMAC_SHA256) || \
	!defined(XRT_FEATURE_TIME) || \
	!defined(XRT_FEATURE_BUFFER))
	#error "XACME_FEATURE_DNS_AWS requires acme dns, acme http transport and signing primitives"
#endif

#if defined(XACME_FEATURE_DNS_AWS)

/*
	AWS Route53 provider（SigV4，XML API 2013-04-01）。
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

/* 构造 provider；内部上下文由 xrtMalloc 分配，宿主用 Unit 归还。
 * DNS 属主按 ASCII 小写登记、匹配和签名请求，TXT 摘要字节区分大小写。
 * 新属主/值从完整属主逐级查找唯一公有托管区；查询失败不回退到父区域。
 * 已拥有值的重复 Add 和 Remove 固定使用登记时的区域 ID；不重新选区。
 * 已存在的同值 TXT 不认领、不删除；本实例已确认写入的值可以重复 Add。
 * 完整 REST-XML 错误中的 400 InvalidChangeBatch、Throttling 和
 * PriorRequestNotComplete 最多尝试 4 次，500ms/1s/2s 退避并重新读取 TXT。
 * 限流耗尽保留 AGAIN/NETWORK；权限拒绝返回 PERMISSION/CREDENTIAL。
 * 错误 XML 畸形或含重复 Code/Error 不能证明拒绝，新增进入未知保护状态。
 * 新增请求发送后应答丢失、5xx 或畸形，返回 DNS_ERROR_UNCERTAIN，保留阻止重放的
 * 状态，后续同名同值 Add/Remove 均失败且不发请求；需外部核对并重建实例。
 * Route53 不提供逐值所有者/租约。相同 zone/name/value 的跨实例或跨进程
 * 使用，宿主必须协调完整 Add→验证→Remove 生命周期，包括外部 DNS 写者。 */
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
