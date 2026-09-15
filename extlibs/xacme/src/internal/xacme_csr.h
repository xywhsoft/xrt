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

/* 证书密钥算法：ES256 内部生成，或宿主提供的 RSA（PKCS#8/PKCS#1）。 */
typedef enum xacmecertkeykind {
	XACME_CERT_KEY_ES256 = 0,
	XACME_CERT_KEY_RSA
} xacmecertkeykind;

/*
	RSA 私钥的定长持有形态（大端正整数字节，无符号前导零已剥）。
	CRT 五参数齐备时签名走 CRT 快路径，否则要求完整私有指数。
*/
typedef struct xacmersakey {
	uint8 Modulus[1024];
	size_t ModulusSize;
	uint8 Exponent[16];
	size_t ExponentSize;
	uint8 PrivateExponent[1024];
	size_t PrivateExponentSize;
	uint8 Prime1[520];
	size_t Prime1Size;
	uint8 Prime2[520];
	size_t Prime2Size;
	uint8 Exponent1[520];
	size_t Exponent1Size;
	uint8 Exponent2[520];
	size_t Exponent2Size;
	uint8 Coefficient[520];
	size_t CoefficientSize;
} xacmersakey;

/* 证书密钥统一外壳；不用的分支内容未定义。 */
typedef struct xacmecertkey {
	xacmecertkeykind Kind;
	xacmees256key Ec;
	xacmersakey Rsa;
} xacmecertkey;

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

/*
	解析证书密钥 PEM（自动识别）：EC（PKCS#8/SEC1）或 RSA
	（PKCS#8 "PRIVATE KEY" / PKCS#1 "RSA PRIVATE KEY"）。
	输出敏感，宿主用 xacmeCertKeyUnit 擦除。
*/
bool xacmeCertKeyReadPem(cstr sPem, size_t iSize, xacmecertkey* pKey);

/* 序列化为未加密 PKCS#8 PEM（EC 或 RSA）；xrtFree 释放。 */
str xacmeCertKeyPemWrite(const xacmecertkey* pKey);

/* 擦除并清零（含 EC/RSA 两分支全量）。 */
void xacmeCertKeyUnit(xacmecertkey* pKey);

/* 按密钥算法生成 PKCS#10 CSR（DER）追加到 pOut。 */
bool xacmeCsrBuild(
	const xacmecertkey* pKey,
	const xacmecsrconfig* pConfig,
	xbuffer* pOut
);

#endif

XRT_EXTERN_C_END

#endif
