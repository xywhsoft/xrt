#ifndef XACME_CSR_H
#define XACME_CSR_H

#include "xacme_jose.h"

#include <xrt/buffer.h>

#if defined(XACME_FEATURE_ACME_CSR)

/* csr 模块稳定错误码（错误域 "xrt.acme.csr"）。 */
typedef enum xacmecsrerror {
	XACME_CSR_ERROR_ARGUMENT = 1,
	XACME_CSR_ERROR_INTERNAL
} xacmecsrerror;

/* CSR 输入全部借用视图；Domains 为 SAN 列表（可含通配符）。 */
typedef struct xacmecsrconfig {
	xstrview CommonName;
	const xstrview* Domains;
	size_t DomainCount;
} xacmecsrconfig;

#endif

XRT_EXTERN_C_BEGIN

#if defined(XACME_FEATURE_ACME_CSR)

/* 生成 ES256 PKCS#10 CSR（DER）追加到 pOut；SAN 扩展经 extensionRequest。 */
bool xacmeCsrEc(
	const xacmees256key* pKey,
	const xacmecsrconfig* pConfig,
	xbuffer* pOut
);

/* PKCS#8 未加密 EC 私钥 PEM（"PRIVATE KEY"）；返回 xrtFree 释放文本。 */
str xacmeKeyPemWrite(const xacmees256key* pKey);

/* 解析 PKCS#8 或 SEC1 EC 私钥 PEM；公钥缺失时从私钥派生。 */
bool xacmeKeyPemRead(cstr sPem, size_t iSize, xacmees256key* pKey);

#endif

XRT_EXTERN_C_END

#endif
