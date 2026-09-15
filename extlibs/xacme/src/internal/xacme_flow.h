#ifndef XACME_FLOW_H
#define XACME_FLOW_H

#include <xrt/core.h>
#include <xrt/error.h>

#include "xacme_csr.h"
#include "xacme_http.h"

#include <xrt/acme.h>
#include <xrt/acme_client.h>
#include <xrt/acme_store.h>
#include "xacme_jose.h"

struct xacmednsprovider;

/* 传播确认内置默认 resolver 组（任一可见即通过）。 */
#define XACME_FLOW_RESOLVER_MAX 4u
#define XACME_FLOW_PROPAGATE_TIMEOUT_MS 120000u

#if defined(XACME_FEATURE_ACME_FLOW)

/* flow 模块稳定错误码（错误域 "xrt.acme.flow"）。 */
typedef enum xacmeflowerror {
	XACME_FLOW_ERROR_ARGUMENT = 1,
	XACME_FLOW_ERROR_DIRECTORY,
	XACME_FLOW_ERROR_ACCOUNT,
	XACME_FLOW_ERROR_NONCE,
	XACME_FLOW_ERROR_ORDER,
	XACME_FLOW_ERROR_CHALLENGE,
	XACME_FLOW_ERROR_FINALIZE,
	XACME_FLOW_ERROR_CERTIFICATE,
	XACME_FLOW_ERROR_STORE,
	XACME_FLOW_ERROR_PROTOCOL
} xacmeflowerror;

/*
	ACME 客户端：账户密钥 + directory 端点 + nonce 槽。
	DirectoryUrl 任意 RFC 8555 CA；AccountKeyPem 为空时生成 ES256。
*/
typedef struct xacmeclient {
	xacmehttp Http;
	xacmees256key AccountKey;
	/* 宿主提供的证书密钥（EC/RSA）；为空则每次签发生成 ES256。 */
	xacmecertkey* pCertKey;
	char sDirectoryUrl[512];
	char sKid[512];
	char sNewNonce[512];
	char sNewAccount[512];
	char sNewOrder[512];
	char sRevokeCert[512];
	char sKeyChange[512];
	char sNonce[512];
	/* 传播确认 resolver（IP 字面量）与预算；空组走默认组。 */
	char sPropagateResolvers[XACME_FLOW_RESOLVER_MAX][64];
	size_t iPropagateResolverCount;
	uint32 uPropagateTimeoutMs;
	/* 单次签发的总预算（微秒；0 = 不限时）。Issue 入口打点，
	   轮询/传播/退避逐段检查剩余时间。 */
	uint64 uIssueTimeoutUs;
	uint64 IssueDeadline;
	bool bIssueDeadline;
} xacmeclient;

#endif

XRT_EXTERN_C_BEGIN

#if defined(XACME_FEATURE_ACME_FLOW)

/*
	初始化：建传输（pBorrowedEngine 为空则自建）、解析 directory、
	注册或复用账户（kid 来自 Location 头）。pAccount 携带 directory、
	账户密钥、EAB 与联系方式（借用视图，宿主保证存活至返回）。
	uTimeoutUs 为 0 时取传输默认（30 秒）。失败设置线程错误。
*/
bool xacmeClientInit(
	xacmeclient* pClient,
	struct xnetengine* pBorrowedEngine,
	cstr sCaPem,
	const xacmeaccountconfig* pAccount,
	uint64 uTimeoutUs
);

void xacmeClientUnit(xacmeclient* pClient);

/* 账户密钥的 PKCS#8 PEM 导出（xrtFree 释放），宿主可持久化后传入 Init 复用。 */
str xacmeClientAccountPem(const xacmeclient* pClient);

/*
	一次 dns-01 签发：域名可含通配符（*. 前缀）；产物含证书链与
	配对私钥（均 xrtFree）。bAlt 时若证书响应带 rel="alternate"
	备用链则优先采用（失败回退主链）。失败返回 false 并设置线程
	错误；provider 的 Add 在挑战触发前调用、Remove 在结束后尽力
	调用。
*/
bool xacmeClientIssue(
	xacmeclient* pClient,
	const xstrview* pDomains,
	size_t iDomainCount,
	const struct xacmednsprovider* pDns,
	xacmeissuegrant* pOut,
	bool bPreferAlternate
);

/*
	账户密钥滚动（RFC 8555 §7.3.5）：用 sNewKeyPem（PKCS#8/SEC1）
	替换当前账户密钥，kid 不变。要求 directory 提供 keyChange 端点。
	sStoreRoot 非空时成功后自动把新账户钥重存进该 store（按
	sDirectoryUrl 隔离），消除滚动后 Obtain 读旧钥开新账户的漂移。
*/
bool xacmeClientRollover(
	xacmeclient* pClient,
	cstr sNewKeyPem,
	cstr sStoreRoot
);

/*
	账户停用（RFC 8555 §7.3.6）：向账户 URL 提交 deactivated。
	停用后该账户及其订单永久不可用；幂等（已停用视为成功）。
*/
bool xacmeClientDeactivate(xacmeclient* pClient);

/*
	一站式续签（组合 store）：
	本地证书剩余寿命不少于 iRenewalDays 天时 *pbRenewed=false 并
	直接返回现有链与私钥；否则签发、落盘（key.pem + fullchain.pem
	+ CA 溯源）并返回新产物。pDomains[0] 同时是 store 的主域名键。
*/
bool xacmeClientIssueStored(
	xacmeclient* pClient,
	const xstrview* pDomains,
	size_t iDomainCount,
	const struct xacmednsprovider* pDns,
	cstr sStoreRoot,
	int iRenewalDays,
	xacmeissuegrant* pOut,
	bool* pbRenewed
);

/*
	吊销证书（RFC 8555 §7.6，账户钥签名）：sCertPem 为单张证书
	（取首个 PEM 块）；iReason 0-9（RFC 5280 CRLReason），<0 省略。
	已被吊销视为成功。要求 directory 提供 revokeCert 端点。
*/
bool xacmeClientRevoke(
	xacmeclient* pClient,
	cstr sCertPem,
	int iReason
);

#endif

XRT_EXTERN_C_END

#endif
