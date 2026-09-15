#ifndef XRT_ACME_CLIENT_H
#define XRT_ACME_CLIENT_H

#include <xrt/core.h>
#include <xrt/error.h>

#include <xrt/acme.h>
#include <xrt/acme_dns.h>
#include <xrt/acme_http.h>
#if defined(XACME_FEATURE_ACME_FLOW) && \
	!defined(XACME_FEATURE_ACME_CORE) || \
	!defined(XACME_FEATURE_ACME_DNS) || \
	!defined(XACME_FEATURE_ACME_HTTP) || \
	!defined(XACME_FEATURE_ACME_JOSE) || \
	!defined(XACME_FEATURE_ACME_CSR) || \
	!defined(XACME_FEATURE_DNS_ALI) || \
	!defined(XACME_FEATURE_ACME_STORE) || \
	!defined(XRT_FEATURE_JSON)
	#error "XACME_FEATURE_ACME_FLOW requires core, dns, http, jose, csr, dns_ali, store and json"
#endif

struct xnetengine;

/*
	线程安全契约：客户端与 provider 实例为单线程归属对象——同一
	实例的任意两个调用不得并发；跨线程使用需宿主外部串行化。
	不同实例（各自 Create 的客户端/provider）之间无共享状态，
	可并行使用。全部 API 为同步阻塞调用。
*/

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
	/*
		单次签发的总预算（微秒；0 = 不限时）：覆盖订单/挑战/
		finalize/证书下载的全部轮询与退避，超限以 XERR_TIMEOUT
		失败。防病态 CA 把签发挂成小时级。
	*/
	uint64 uIssueTimeoutUs;
	/*
		宿主提供的证书私钥 PEM（可选；EC P-256 或 RSA-2048+）：
		设置后每次签发复用同一证书密钥（含 RSA 证书场景）；
		为空则每次签发生成一次性 ES256。库不内置 RSA 密钥生成，
		RSA 密钥由宿主用 openssl 等工具预先生成。
	*/
	cstr sCertKeyPem;
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

/*
	Issue 的备用链变体：bPreferAlternate 时若证书响应的 Link 头带
	rel="alternate"（RFC 8555 §7.4.2），改用备用链下载；备用链获取
	失败自动回退主链，不视为错误。
*/
XRT_API bool xrtAcmeClientIssueEx(
	struct xacmeclient* pClient,
	const xstrview* pDomains,
	size_t iDomainCount,
	const xacmednsprovider* pDns,
	bool bPreferAlternate,
	xacmeissuegrant* pOut
);

/*
	吊销证书（RFC 8555 §7.6，账户钥签名）：sCertPem 为单张证书
	（取首个 PEM 块）；iReason 0-9（RFC 5280 CRLReason），<0 省略。
	已被吊销视为幂等成功。要求 directory 提供 revokeCert 端点。
*/
XRT_API bool xrtAcmeClientRevoke(
	struct xacmeclient* pClient,
	cstr sCertPem,
	int iReason
);

/*
	账户密钥滚动（RFC 8555 §7.3.5）：用 sNewKeyPem（PKCS#8/SEC1）
	替换当前账户密钥，账户 kid 不变。要求 directory 提供 keyChange
	端点；成功后客户端即刻使用新钥，宿主应经 xrtAcmeClientAccountPem
	重新持久化。
*/
XRT_API bool xrtAcmeClientRollover(
	struct xacmeclient* pClient,
	cstr sNewKeyPem,
	cstr sStoreRoot
);

/*
	账户停用（RFC 8555 §7.3.6）：停用后该账户及其订单永久不可用；
	幂等（已停用视为成功）。
*/
XRT_API bool xrtAcmeClientDeactivate(struct xacmeclient* pClient);

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
