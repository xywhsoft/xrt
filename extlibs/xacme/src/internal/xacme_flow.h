#ifndef XACME_FLOW_H
#define XACME_FLOW_H

#include <xrt/core.h>
#include <xrt/error.h>

#include "xacme_csr.h"
#include "xacme_http.h"

#include <xrt/acme_store.h>
#include "xacme_jose.h"

struct xacmednsprovider;

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
	char sKid[512];
	char sNewNonce[512];
	char sNewAccount[512];
	char sNewOrder[512];
	char sNonce[512];
} xacmeclient;

#endif

XRT_EXTERN_C_BEGIN

#if defined(XACME_FEATURE_ACME_FLOW)

/*
	初始化：建传输（pBorrowedHttp 为空则自建）、解析 directory、
	注册或复用账户（kid 来自 Location 头）。失败设置线程错误。
*/
bool xacmeClientInit(
	xacmeclient* pClient,
	struct xnetengine* pBorrowedEngine,
	cstr sCaPem,
	cstr sDirectoryUrl,
	cstr sAccountKeyPem
);

void xacmeClientUnit(xacmeclient* pClient);

/* 账户密钥的 PKCS#8 PEM 导出（xrtFree 释放），宿主可持久化后传入 Init 复用。 */
str xacmeClientAccountPem(const xacmeclient* pClient);

/*
	一次 dns-01 签发：域名可含通配符（*. 前缀）；返回证书链 PEM
	（xrtFree 释放）。失败返回 NULL 并设置线程错误；provider 的
	Add 在挑战触发前调用、Remove 在结束后尽力调用。
*/
str xacmeClientIssue(
	xacmeclient* pClient,
	const xstrview* pDomains,
	size_t iDomainCount,
	const struct xacmednsprovider* pDns
);

/*
	一站式续签（组合 store）：
	本地证书剩余寿命不少于 iRenewalDays 天时 *pbRenewed=false 并
	直接返回现有链；否则签发、落盘（fullchain + CA 溯源）并返回
	新链。pDomains[0] 同时是 store 的主域名键。
*/
str xacmeClientIssueStored(
	xacmeclient* pClient,
	const xstrview* pDomains,
	size_t iDomainCount,
	const struct xacmednsprovider* pDns,
	cstr sStoreRoot,
	int iRenewalDays,
	bool* pbRenewed
);

#endif

XRT_EXTERN_C_END

#endif
