/* xjwt 内部头：跨分片共享的工具与数据结构。 */
#ifndef XJWT_INTERNAL_H
#define XJWT_INTERNAL_H

#include <xjwt.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ------------------------------------------------------------------
 * DER 大整数：正数 INTEGER 高位置 1 时 DER 会补 0x00 前缀，
 * xrt RSA 大数运算要求剥离（就地 memmove，指针与释放不受影响）。
 * ------------------------------------------------------------------ */
static inline void xjwt__int_trim(unsigned char* p, size_t* pn)
{
	while ( *pn > 1 && p[0] == 0 ) {
		memmove(p, p + 1, *pn - 1);
		*pn -= 1;
	}
}

/* JWA RS256/384/512 要求 RSA 模数至少 2048 位。 */
static inline bool xjwt__rsa_jwa_key_valid(const xrsapublickey* pKey)
{
	const unsigned char* pModulus;
	const unsigned char* pExponent;

	if ( (pKey == NULL) || (pKey->Modulus == NULL) ||
		(pKey->Exponent == NULL) ||
		(pKey->ModulusSize < 256u) ||
		(pKey->ModulusSize > XRT_RSA_MAX_MODULUS_SIZE) ||
		(pKey->ExponentSize == 0) ||
		(pKey->ExponentSize > pKey->ModulusSize) ) {
		return false;
	}
	pModulus = (const unsigned char*)pKey->Modulus;
	pExponent = (const unsigned char*)pKey->Exponent;
	return (pModulus[0] != 0) &&
		((pKey->ModulusSize > 256u) || (pModulus[0] >= 0x80u)) &&
		((pModulus[pKey->ModulusSize - 1u] & 1u) != 0) &&
		(pExponent[0] != 0) &&
		((pExponent[pKey->ExponentSize - 1u] & 1u) != 0) &&
		((pKey->ExponentSize > 1u) || (pExponent[0] > 1u));
}

/* ------------------------------------------------------------------
 * Base64URL（RFC 4648 §5：无 padding，- 和 _ 替代 + 和 /）
 * ------------------------------------------------------------------ */

/* 编码：返回 xrtMalloc 字符串（xrtFree 释放）。 */
char* xjwt__base64url_encode(const void* pData, size_t iSize);

/* 解码：返回 xrtMalloc 缓冲（xrtFree 释放），*pOutSize 收长度。 */
unsigned char* xjwt__base64url_decode(const char* sText, size_t iTextSize, size_t* pOutSize);

/* 就地测量（不分配）。 */
size_t xjwt__base64url_decode_size(const char* sText, size_t iTextSize);

/* ------------------------------------------------------------------
 * token 组装 / 拆解
 * ------------------------------------------------------------------ */

/* 三段拆解：返回三段的 [ptr, size)；格式非法返回 false。 */
bool xjwt__split(const char* sToken,
                 const char** pHead, size_t* pHeadSize,
                 const char** pClaims, size_t* pClaimsSize,
                 const char** pSig, size_t* pSigSize);

/* 组装：header_json + claims_json + signature → "h.c.s"（xrtFree）。 */
char* xjwt__join(const char* sHeadJson, const char* sClaimsJson,
                 const void* pSig, size_t iSigSize);

/* ------------------------------------------------------------------
 * 错误设置（域 "xrt.jwt"）
 * ------------------------------------------------------------------ */
void xjwt__error(int iCode, const char* sMessage);
bool xjwt__memory_error(void);
void xjwt__error_unless_memory(int iCode, const char* sMessage);

/* ------------------------------------------------------------------
 * RSA / EC 密钥解析（PEM → xrt 内部表示）
 * ------------------------------------------------------------------ */

/* RSA 公钥：从 PEM（SPKI 或 PKCS#1）解析为 DER 再手动走。 */
bool xjwt__rsa_public_parse(const char* sPem, xrsapublickey* pKey,
                            unsigned char** ppOwned);  /* xjwt__rsa_public_free 释放 */

/* RSA 公钥资源释放（与 parse 配对）。 */
void xjwt__rsa_public_free(xrsapublickey* pKey, unsigned char* pOwned);

/* ECDSA P-256 公钥：从 PEM 解析 65 字节未压缩点。 */
bool xjwt__ecdsa_public_parse(const char* sPem, unsigned char* pPublic65);

typedef struct xjwt__rsa_owned {
	unsigned char* pRaw;        /* 私钥组件借用此 DER 存储 */
	size_t iRawSize;
} xjwt__rsa_owned;

struct xjwtjwks {
	int nKeys;
	struct {
		char kid[128];
		bool HasKid;      /* 显式空 kid 与未提供 kid 不同 */
		char kty[8];       /* "RSA" 或 "EC" */
		xrsapublickey rsa;
		unsigned char ec65[65];
		unsigned char* pOwnedN;   /* RSA n */
		unsigned char* pOwnedE;   /* RSA e */
	} keys[16];
};

void xjwt__rsa_owned_free(xjwt__rsa_owned* p);
bool xjwt__ecdsa_private_parse(const char* sPem, unsigned char* pPrivate32);

/* RSA 私钥：从 PEM（PKCS#8 或 PKCS#1）解析；owned 资源用 xjwt__rsa_owned_free 释放。 */
bool xjwt__rsa_private_parse(const char* sPem, xrsaprivatekey* pKey,
                             struct xjwt__rsa_owned** ppOwned);

/* ------------------------------------------------------------------
 * 签名 / 验签（按算法分派）
 * iCapacity 是 pOut 缓冲容量，签名超长直接失败（不截断）。
 * ------------------------------------------------------------------ */
bool xjwt__sign(int alg, const void* pSigningInput, size_t iSize,
                const char* sKeyPem, unsigned char* pOut,
                size_t iCapacity, size_t* pOutSize);
bool xjwt__verify(int alg, const void* pSigningInput, size_t iSize,
                  const char* sKeyPem, const void* pSig, size_t iSigSize);

/* RS/ES 签发完整实现（xjwt_ext.c）。 */
bool rsa_sign_impl(int alg, const void* pData, size_t iSize,
                   const char* sPrivatePem, unsigned char* pOut,
                   size_t iCapacity, size_t* pOutSize);
bool es256_sign_impl(const void* pData, size_t iSize,
                     const char* sPrivatePem, unsigned char* pOut,
                     size_t iCapacity, size_t* pOutSize);
bool xjwt__verify_es256_full(const void* pData, size_t iSize,
                             const char* sPublicPem,
                             const void* pSig, size_t iSigSize);

/* 三条验证路径（PEM/JWKS/缓存公钥）共用的前置步骤：
 * 解码 header(alg+kid) → claims 解码校验 → 重建签名输入 → 解码签名。
 * 成功返回 claims，并把中间量写给调用方（各自 xrtFree）；
 * *pKid 是堆拷贝，调用方负责 xrtFree。失败返回 NULL。 */
xvalue* xjwt__verify_prepare(const char* sToken, const xjwtcheck* pCheck,
                             int* pAlg, const char** pKid,
                             char** psInput, size_t* pn,
                             unsigned char** ppSig, size_t* pSigSize);

/* ECDSA P-256 公钥直接验签（JWKS 路径用）。 */
bool xjwt__verify_es256_raw(const void* pData, size_t iSize,
                            const void* pSig, size_t iSigSize,
                            const unsigned char* pPublic65);

/* RSA 公钥直接验签（JWKS 路径用）。 */
bool xjwt__verify_rsa_raw(const void* pData, size_t iSize,
                          const void* pSig, size_t iSigSize,
                          const xrsapublickey* pKey, int alg);

#endif
