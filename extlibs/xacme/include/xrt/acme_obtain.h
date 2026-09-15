#ifndef XRT_ACME_OBTAIN_H
#define XRT_ACME_OBTAIN_H

#include <xrt/core.h>
#include <xrt/error.h>

#include <xrt/acme.h>
#include <xrt/acme_dns.h>

#if defined(XACME_FEATURE_ACME_OBTAIN) && \
	!defined(XACME_FEATURE_ACME_FLOW) || \
	!defined(XACME_FEATURE_ACME_STORE) || \
	!defined(XACME_FEATURE_ACME_CORE) || \
	!defined(XACME_FEATURE_ACME_HTTP) || \
	!defined(XACME_FEATURE_ACME_JOSE) || \
	!defined(XACME_FEATURE_ACME_CSR) || \
	!defined(XACME_FEATURE_ACME_DNS) || \
	!defined(XACME_FEATURE_DNS_ALI) || \
	!defined(XRT_FEATURE_JSON) || \
	!defined(XRT_FEATURE_FILE_WHOLE) || \
	!defined(XRT_FEATURE_X509_PARSE) || \
	!defined(XRT_FEATURE_CRYPTO_SHA256) || \
	!defined(XRT_FEATURE_PEM) || \
	!defined(XRT_FEATURE_CODEC_BASE64) || \
	!defined(XRT_FEATURE_TIME) || \
	!defined(XRT_FEATURE_BUFFER) || \
	!defined(XRT_FEATURE_DIR)
	#error "XACME_FEATURE_ACME_OBTAIN requires flow and store closures"
#endif

struct xnetengine;

#if defined(XACME_FEATURE_ACME_OBTAIN)

/*
	一站式配置：账户层 + 客户端层 + 续签层，全部借用视图，
	宿主保证存活至 Obtain 返回。sStoreRoot 必填；iRenewalDays
	为 0 时默认 30（剩余寿命不足该天数即续签）。
*/
typedef struct xacmeobtainconfig {
	const xacmeaccountconfig* pAccount;
	cstr sCaPem;
	struct xnetengine* pBorrowedEngine;
	uint64 uTimeoutUs;
	const cstr* sPropagateResolvers;
	size_t iPropagateResolverCount;
	uint32 uPropagateTimeoutMs;
	/* 单次签发总预算（微秒；0 = 不限时），透传给客户端。 */
	uint64 uIssueTimeoutUs;
	/* 宿主提供的证书私钥 PEM（可选，EC/RSA），透传给客户端。 */
	cstr sCertKeyPem;
	cstr sStoreRoot;
	int iRenewalDays;
} xacmeobtainconfig;

#endif

XRT_EXTERN_C_BEGIN

#if defined(XACME_FEATURE_ACME_OBTAIN)

/* 全零初始化；指针字段为空表示未设置。 */
XRT_API void xrtAcmeObtainConfigInit(xacmeobtainconfig* pConfig);

/*
	一次调用取得可用证书（链 + 配对私钥）：
	  1. store 无账户则注册并持久化（accounts/<ca16>/account.pem），
	     有则复用（同一 CA 稳定账户，不反复开户）；
	  2. 本地证书剩余寿命充足时直接返回（*pbRenewed=false）；
	  3. 不足则完整 dns-01 签发并落盘
	     （certs/<主域名>/{key.pem,fullchain.pem,meta_txt}）。
	pOut 两段文本均 xrtFree（或 xrtAcmeGrantUnit 统一释放）。
	失败返回 false 并设置线程错误。
*/
XRT_API bool xrtAcmeObtain(
	const xacmeobtainconfig* pConfig,
	const xstrview* pDomains,
	size_t iDomainCount,
	const xacmednsprovider* pDns,
	xacmeissuegrant* pOut,
	bool* pbRenewed
);

#endif

XRT_EXTERN_C_END

#endif
