#ifndef XRT_ACME_CLIENT_H
#define XRT_ACME_CLIENT_H

#include <xrt/core.h>
#include <xrt/error.h>

#include <xrt/acme.h>
#include <xrt/acme_dns.h>

#if defined(XACME_FEATURE_ACME_FLOW) && !defined(XACME_FEATURE_ACME_CORE)
	#error "XRT acme client requires XACME_FEATURE_ACME_CORE"
#endif

#if defined(XACME_FEATURE_ACME_FLOW) && !defined(XACME_FEATURE_ACME_DNS)
	#error "XRT acme client requires XACME_FEATURE_ACME_DNS"
#endif

struct xnetengine;

#if defined(XACME_FEATURE_ACME_FLOW)

/*
	客户端配置：全部借用视图，宿主保证存活至 Create 返回。
	pAccount 必填；sPropagateResolvers 为空时使用内置默认组
	（223.5.5.5 / 119.29.29.29 / 8.8.8.8，任一可见即通过），
	uPropagateTimeoutMs 为 0 时默认 120 秒。
*/
typedef struct xacmeclientconfig {
	const xacmeaccountconfig* pAccount;
	cstr sCaPem;
	struct xnetengine* pBorrowedEngine;
	uint64 uTimeoutUs;
	const cstr* sPropagateResolvers;
	size_t iPropagateResolverCount;
	uint32 uPropagateTimeoutMs;
} xacmeclientconfig;

#endif

/* 不透明客户端；定义在内部头，宿主只经指针使用。 */

XRT_EXTERN_C_BEGIN

#if defined(XACME_FEATURE_ACME_FLOW)

/* 全零初始化；指针字段为空表示未设置。 */
XRT_API void xrtAcmeClientConfigInit(xacmeclientconfig* pConfig);

/*
	创建客户端：建传输、解析 directory、注册或复用账户（含
	EAB/contact）。失败返回 NULL 并设置线程错误。
*/
XRT_API struct xacmeclient* xrtAcmeClientCreate(
	const xacmeclientconfig* pConfig);

/* 销毁并释放；入参可为空。 */
XRT_API void xrtAcmeClientDestroy(struct xacmeclient* pClient);

/* 账户密钥 PKCS#8 PEM 导出（xrtFree 释放），宿主可持久化复用。 */
XRT_API str xrtAcmeClientAccountPem(const struct xacmeclient* pClient);

/*
	一次 dns-01 签发：域名可含通配符（*. 前缀）；产物含证书链与
	配对私钥（pOut 两段均 xrtFree，或经 xrtAcmeGrantUnit 统一释放）。
	provider 的 Add 在 TXT 铺设后、挑战触发前调用；传播确认通过后
	才触发挑战；Remove 在结束后尽力调用。
*/
XRT_API bool xrtAcmeClientIssue(
	struct xacmeclient* pClient,
	const xstrview* pDomains,
	size_t iDomainCount,
	const xacmednsprovider* pDns,
	xacmeissuegrant* pOut
);

#endif

#if defined(XACME_FEATURE_ACME_FLOW) && defined(XACME_FEATURE_ACME_STORE)

/*
	一站式续签（组合 store）：本地证书剩余寿命不少于 iRenewalDays
	天时 *pbRenewed=false 并直接返回现有链与私钥；否则签发、落盘
	（key.pem + fullchain.pem + CA 溯源）并返回新产物。
	pDomains[0] 同时是 store 的主域名键。
*/
XRT_API bool xrtAcmeClientIssueStored(
	struct xacmeclient* pClient,
	const xstrview* pDomains,
	size_t iDomainCount,
	const xacmednsprovider* pDns,
	cstr sStoreRoot,
	int iRenewalDays,
	xacmeissuegrant* pOut,
	bool* pbRenewed
);

#endif

XRT_EXTERN_C_END

#endif
