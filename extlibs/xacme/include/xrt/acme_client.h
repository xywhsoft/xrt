#ifndef XRT_ACME_CLIENT_H
#define XRT_ACME_CLIENT_H

#include <xrt/core.h>
#include <xrt/error.h>

#include <xrt/acme.h>
#include <xrt/acme_dns.h>
#include <xrt/acme_http.h>
#if defined(XACME_FEATURE_ACME_FLOW) && (\
	!defined(XACME_FEATURE_ACME_CORE) || \
	!defined(XACME_FEATURE_ACME_DNS) || \
	!defined(XACME_FEATURE_ACME_HTTP) || \
	!defined(XACME_FEATURE_ACME_JOSE) || \
	!defined(XACME_FEATURE_ACME_CSR) || \
	!defined(XACME_FEATURE_DNS_ALI) || \
	!defined(XACME_FEATURE_ACME_STORE) || \
	!defined(XRT_FEATURE_JSON) || !defined(XRT_FEATURE_HTTP_TARGET) || \
	!defined(XRT_FEATURE_X509_PARSE) || !defined(XRT_FEATURE_X509_PROFILE) || \
	!defined(XRT_FEATURE_X509_NAME) || !defined(XRT_FEATURE_X509_VERIFY))
	#error "XACME_FEATURE_ACME_FLOW requires its core, DNS, HTTP target, JOSE, CSR, store, JSON and X.509 closures"
#endif

struct xnetengine;

/*
	线程安全契约：客户端与 provider 实例为单线程归属对象——同一
	实例的任意两个调用不得并发；跨线程使用需宿主外部串行化。
	不同实例的请求与账户状态独立，可并行使用；未交付对象的异常
	退休队列跨实例共享，经 xrtAcmeCleanupPending 同步。全部 API
	为同步阻塞调用。
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
	int64 uTimeoutMs;
	const cstr* sPropagateResolvers;
	size_t iPropagateResolverCount;
	uint32 uPropagateTimeoutMs;
	/*
		单次签发的总预算（毫秒；0 = 不限时）：覆盖订单/挑战/
		finalize/证书下载的全部轮询与退避，超限以 XERR_TIMEOUT
		失败。防病态 CA 把签发挂成小时级。
	*/
	int64 uIssueTimeoutMs;
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
	EAB/contact）。失败返回 NULL 并设置线程错误。失败构造的资源
	回滚独立留出至少 30 秒预算，不复用已耗尽的单次请求超时。
	仍未退休的未交付对象转移至待清理队列，保留原始构造错误；
	宿主在退出/卸载前用 xrtAcmeCleanupPending 确认全部释放。
	JSON/PEM/签名/传输/存储的内存失败保留原错误；底层传输失败
	保留其 kind/domain/code。宿主按 xrtErrorKind 分类，不假定
	所有错误来自 xrt.acme.flow。响应格式或状态无效为 XERR_PROTOCOL。
*/
XRT_API struct xacmeclient* xrtAcmeClientCreate(
	const xacmeclientconfig* pConfig);

/*
	清理资源而保留客户端外壳；空指针为 true，可重复调用。
	true 表示完成，随后可 Destroy；false 表示引擎尚未退休，保留
	客户端供稍后重试。无论结果如何，实例此后仅可 Cleanup/Destroy。
	保留调用前已有错误，因此用返回值判断清理结果，不能只看线程错误。
*/
XRT_API bool xrtAcmeClientCleanup(struct xacmeclient* pClient);

/* 销毁并释放；入参可为空。清理超时或失败时保留外壳供重试。
 * 需要确定释放结果时先调用 Cleanup；true 后再调用 Destroy。 */
XRT_API void xrtAcmeClientDestroy(struct xacmeclient* pClient);

/* 账户密钥 PKCS#8 PEM 导出（xrtFree 释放），宿主可持久化复用。 */
XRT_API str xrtAcmeClientAccountPem(const struct xacmeclient* pClient);

/*
	一次 dns-01 签发：域名可含通配符（*. 前缀）；产物含证书链与
	配对私钥（pOut 两段均 xrtFree，或经 xrtAcmeGrantUnit 统一释放）。
	provider 的 Add 用于铺设 TXT；随后传播确认，最后触发挑战。
	普通传播不可达或超时不阻断签发；传播分配失败立即返回，
	保留 MEMORY 诊断且不再查询其他 resolver 或触发挑战。
	Remove 在结束后尽力调用。
	最多 16 个不同域名；每段视图须为 1-511 字节且不含 NUL。
	这些限制在发订单前检查。失败不交付部分产物，清理 TXT 时
	保留签发的原始失败原因。轮询的解析或内存失败立即返回。
	证书密钥须不同于当前账户钥。下载仅接受 PEM 证书链（最多
	16 张、每张 DER 不超过 256 KiB），检查公钥、SAN、叶证书有效期
	及所提供链的发行者、CA 标志和签名；不代替部署的根信任策略。
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
	或校验失败自动回退主链，不视为错误。相对引用以主证书 URL
	解析；备用链须从同一张 DER 叶证书开始。
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
	端点；请求按 RFC 8555 内层新钥 JWK、外层旧钥 kid 签名。
	写入响应丢失时，使用 onlyReturnExisting 对账；未确认生效则返回
	false 并保留当前内存密钥。成功后客户端即刻使用新钥；若指定
	sStoreRoot 而重存失败，返回 false 但内存仍使用新钥，调用方可
	用同一 sNewKeyPem 重试对账与持久化。
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
