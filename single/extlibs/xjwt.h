/*
 * MIT License
 *
 * Copyright (c) 2025 xLeaves [xywhsoft] <xywhsoft@qq.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* 此文件由 tools/amalgamate.py 生成，请勿直接修改。 */
/* Supply XRT and selected extension dependencies before this header. */
#if !defined(XRT_CORE_H)
#error "xjwt requires XRT; include xrt.h or xrt_decl.h first"
#endif
#if defined(XJWT_IMPLEMENTATION) && defined(_MSC_VER) && \
	!defined(_CRT_SECURE_NO_WARNINGS)
	#define _CRT_SECURE_NO_WARNINGS
#endif
#if defined(XJWT_IMPLEMENTATION) && \
	!defined(_WIN32) && !defined(_WIN64)
	#if defined(__linux__) && !defined(_GNU_SOURCE)
		#define _GNU_SOURCE 1
	#endif
	#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
		#define _DARWIN_C_SOURCE 1
	#endif
	#if !defined(_POSIX_C_SOURCE)
		#define _POSIX_C_SOURCE 200809L
	#endif
	#if !defined(_FILE_OFFSET_BITS)
		#define _FILE_OFFSET_BITS 64
	#endif
#endif
#ifndef XJWT_SINGLE_HEADER_H
#define XJWT_SINGLE_HEADER_H
#define XJWT_SINGLE_HEADER 1


/* ========================================================================== */
/* feature selection: extlibs/xjwt/include/xjwt/features.h */
/* ========================================================================== */

/* 此文件由 tools/generate_extension_features.py 生成，请勿直接修改。 */
#ifndef XJWT_FEATURES_H
#define XJWT_FEATURES_H

/* xjwt 及其直接依赖。 */
#if defined(XJWT_MODULE_ALL) || defined(XJWT_MODULE_XJWT)
#ifndef XJWT_FEATURE_XJWT
#define XJWT_FEATURE_XJWT
#endif
#ifndef XRT_MODULE_JSON_READ
#define XRT_MODULE_JSON_READ
#endif
#ifndef XRT_MODULE_JSON_WRITE
#define XRT_MODULE_JSON_WRITE
#endif
#ifndef XRT_MODULE_VALUE
#define XRT_MODULE_VALUE
#endif
#ifndef XRT_MODULE_CODEC_BASE64
#define XRT_MODULE_CODEC_BASE64
#endif
#ifndef XRT_MODULE_CRYPTO_CORE
#define XRT_MODULE_CRYPTO_CORE
#endif
#ifndef XRT_MODULE_CRYPTO_SHA256
#define XRT_MODULE_CRYPTO_SHA256
#endif
#ifndef XRT_MODULE_CRYPTO_SHA512
#define XRT_MODULE_CRYPTO_SHA512
#endif
#ifndef XRT_MODULE_CRYPTO_HMAC_SHA256
#define XRT_MODULE_CRYPTO_HMAC_SHA256
#endif
#ifndef XRT_MODULE_CRYPTO_HMAC_SHA512
#define XRT_MODULE_CRYPTO_HMAC_SHA512
#endif
#ifndef XRT_MODULE_CRYPTO_RSA
#define XRT_MODULE_CRYPTO_RSA
#endif
#ifndef XRT_MODULE_CRYPTO_RSA_PRIVATE
#define XRT_MODULE_CRYPTO_RSA_PRIVATE
#endif
#ifndef XRT_MODULE_CRYPTO_RSA_PKCS1_SIGN
#define XRT_MODULE_CRYPTO_RSA_PKCS1_SIGN
#endif
#ifndef XRT_MODULE_CRYPTO_ECDSA_P256
#define XRT_MODULE_CRYPTO_ECDSA_P256
#endif
#ifndef XRT_MODULE_CRYPTO_ECDSA_P256_SIGN
#define XRT_MODULE_CRYPTO_ECDSA_P256_SIGN
#endif
#ifndef XRT_MODULE_PEM
#define XRT_MODULE_PEM
#endif
#ifndef XRT_MODULE_ASN1_DER
#define XRT_MODULE_ASN1_DER
#endif
#ifndef XRT_MODULE_TIME
#define XRT_MODULE_TIME
#endif
#ifndef XRT_MODULE_STRING
#define XRT_MODULE_STRING
#endif
#endif

#endif /* XJWT_FEATURES_H */


/* ========================================================================== */
/* public: extlibs/xjwt/include/xjwt/api.h */
/* ========================================================================== */

/*
	xjwt —— JSON Web Token (RFC 7519) / JWS (RFC 7515) 签发与验证。

	构建在 xrt 核心密码原语（HMAC/RSA PKCS1/ECDSA P-256）之上。
	支持 HS256/384/512、RS256/384/512、ES256 三族算法；
	JWKS (RFC 7517) 公钥集解析与按 kid 自动选钥。

	用法示例（一步式）：
		char* token = xjwtHs256(claims, secret, 3600);
		xvalue* out  = xjwtVerify(token, secret, &check);

	依赖闭包由 config/modules.json 声明；src/ 中各文件独立编译。
*/
#ifndef XJWT_API_H
#define XJWT_API_H


#if defined(XJWT_FEATURE_XJWT)

/* The selected product requires its complete declared dependency set. */
#if !defined(XRT_FEATURE_JSON_READ)
#error "xjwt requires json_read (XRT_FEATURE_JSON_READ)"
#endif
#if !defined(XRT_FEATURE_JSON_WRITE)
#error "xjwt requires json_write (XRT_FEATURE_JSON_WRITE)"
#endif
#if !defined(XRT_FEATURE_VALUE)
#error "xjwt requires value (XRT_FEATURE_VALUE)"
#endif
#if !defined(XRT_FEATURE_CODEC_BASE64)
#error "xjwt requires codec_base64 (XRT_FEATURE_CODEC_BASE64)"
#endif
#if !defined(XRT_FEATURE_CRYPTO_CORE)
#error "xjwt requires crypto_core (XRT_FEATURE_CRYPTO_CORE)"
#endif
#if !defined(XRT_FEATURE_CRYPTO_SHA256)
#error "xjwt requires crypto_sha256 (XRT_FEATURE_CRYPTO_SHA256)"
#endif
#if !defined(XRT_FEATURE_CRYPTO_SHA512)
#error "xjwt requires crypto_sha512 (XRT_FEATURE_CRYPTO_SHA512)"
#endif
#if !defined(XRT_FEATURE_CRYPTO_HMAC_SHA256)
#error "xjwt requires crypto_hmac_sha256 (XRT_FEATURE_CRYPTO_HMAC_SHA256)"
#endif
#if !defined(XRT_FEATURE_CRYPTO_HMAC_SHA512)
#error "xjwt requires crypto_hmac_sha512 (XRT_FEATURE_CRYPTO_HMAC_SHA512)"
#endif
#if !defined(XRT_FEATURE_CRYPTO_RSA)
#error "xjwt requires crypto_rsa (XRT_FEATURE_CRYPTO_RSA)"
#endif
#if !defined(XRT_FEATURE_CRYPTO_RSA_PRIVATE)
#error "xjwt requires crypto_rsa_private (XRT_FEATURE_CRYPTO_RSA_PRIVATE)"
#endif
#if !defined(XRT_FEATURE_CRYPTO_RSA_PKCS1_SIGN)
#error "xjwt requires crypto_rsa_pkcs1_sign (XRT_FEATURE_CRYPTO_RSA_PKCS1_SIGN)"
#endif
#if !defined(XRT_FEATURE_CRYPTO_ECDSA_P256)
#error "xjwt requires crypto_ecdsa_p256 (XRT_FEATURE_CRYPTO_ECDSA_P256)"
#endif
#if !defined(XRT_FEATURE_CRYPTO_ECDSA_P256_SIGN)
#error "xjwt requires crypto_ecdsa_p256_sign (XRT_FEATURE_CRYPTO_ECDSA_P256_SIGN)"
#endif
#if !defined(XRT_FEATURE_PEM)
#error "xjwt requires pem (XRT_FEATURE_PEM)"
#endif
#if !defined(XRT_FEATURE_ASN1_DER)
#error "xjwt requires asn1_der (XRT_FEATURE_ASN1_DER)"
#endif
#if !defined(XRT_FEATURE_TIME)
#error "xjwt requires time (XRT_FEATURE_TIME)"
#endif
#if !defined(XRT_FEATURE_STRING)
#error "xjwt requires string (XRT_FEATURE_STRING)"
#endif


#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* xrt 核心 JSON 值（前向声明，实现经 xrt_decl.h） */
struct xvalue;
typedef struct xvalue xvalue;

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------
 * 版本
 * ------------------------------------------------------------------ */
#define XJWT_VERSION_MAJOR 1
#define XJWT_VERSION_MINOR 0
#define XJWT_VERSION_PATCH 0

/* ------------------------------------------------------------------
 * 算法
 * ------------------------------------------------------------------ */
enum {
	XJWT_ALG_NONE    = 0,
	XJWT_ALG_HS256   = 1,
	XJWT_ALG_HS384   = 2,
	XJWT_ALG_HS512   = 3,
	XJWT_ALG_RS256   = 4,
	XJWT_ALG_RS384   = 5,
	XJWT_ALG_RS512   = 6,
	XJWT_ALG_ES256   = 7,
	XJWT_ALG_INVALID = -1,
};

/* ------------------------------------------------------------------
 * 错误码（xerror 域 "xrt.jwt"）
 * ------------------------------------------------------------------ */
enum {
	XJWT_ERROR_ARGUMENT      = 1,
	XJWT_ERROR_MALFORMED     = 2,
	XJWT_ERROR_ALG_MISMATCH  = 3,
	XJWT_ERROR_SIGNATURE     = 4,
	XJWT_ERROR_EXPIRED       = 5,
	XJWT_ERROR_NOT_YET       = 6,
	XJWT_ERROR_ISSUER        = 7,
	XJWT_ERROR_AUDIENCE      = 8,
	XJWT_ERROR_KEY_NOT_FOUND = 9,
	XJWT_ERROR_PARSE         = 10,
};

/* ------------------------------------------------------------------
 * claims 校验参数
 * exp/nbf 必须是整数（RFC 7519 虽允许小数 NumericDate，本库刻意
 * fail-closed：存在但非整数一律拒绝）。
 * ------------------------------------------------------------------ */
typedef struct xjwtcheck {
	const char* Issuer;        /* NULL = 跳过 iss 检查 */
	const char* Audience;      /* NULL = 跳过 aud 检查 */
	int64_t     NowOverride;   /* 0 = 当前时间（秒）；非零 = 测试注入 */
	int         ClockLeeway;   /* exp/nbf 容差秒数，默认 0；负数报 ARGUMENT */
} xjwtcheck;

/* 校验参数零值（默认只查 exp）。 */
XRT_API void xjwtCheckInit(xjwtcheck* pCheck);

/* ------------------------------------------------------------------
 * 签发（一步式）
 * claims 是 xrt 的 xvalue JSON 对象；返回 token 字符串（xrtFree 释放）。
 * expireSeconds != 0 自动注入 exp；自动注入 iat。调用方保留 claims 所有权，
 * 签发成功或失败都不修改传入对象；自动字段仅存在于签发的 token 中。
 * 注意：本族不注入 iss/aud —— 验证侧 xjwtcheck 带 Issuer/Audience
 * 校验时请改用 xjwtSign（config 式可注入全部标准 claims）。
 * ------------------------------------------------------------------ */
XRT_API char* xjwtHs256(const xvalue* claims, const char* secret, int expireSeconds);
/* HS384 一步式签发；自动注入 exp，返回 xrtFree 释放的 token。 */
XRT_API char* xjwtHs384(const xvalue* claims, const char* secret, int expireSeconds);
/* HS512 一步式签发；自动注入 exp，返回 xrtFree 释放的 token。 */
XRT_API char* xjwtHs512(const xvalue* claims, const char* secret, int expireSeconds);
/* RS256 一步式签发（RSA 私钥 PEM）；自动注入 exp。 */
XRT_API char* xjwtRs256(const xvalue* claims, const char* privatePem, int expireSeconds);
/* RS384 一步式签发（RSA 私钥 PEM）；自动注入 exp。 */
XRT_API char* xjwtRs384(const xvalue* claims, const char* privatePem, int expireSeconds);
/* RS512 一步式签发（RSA 私钥 PEM）；自动注入 exp。 */
XRT_API char* xjwtRs512(const xvalue* claims, const char* privatePem, int expireSeconds);
/* ES256 一步式签发（EC P-256 私钥 PEM）；自动注入 exp。 */
XRT_API char* xjwtEs256(const xvalue* claims, const char* privatePem, int expireSeconds);

/* ------------------------------------------------------------------
 * 签发（config 式，精调 header / kid / iss / aud）
 * ------------------------------------------------------------------ */
typedef struct xjwtconfig {
	int         Alg;            /* XJWT_ALG_* */
	const char* KeyPem;         /* HMAC 密钥（C 字符串，内嵌 '\0' 会被截断，
	                             * 不支持二进制密钥）或 RSA/ECDSA PEM */
	const char* KeyId;          /* 可选 kid；短于 256 字节，NULL 表示省略，"" 表示空标识 */
	const char* Issuer;         /* 可选自动注入 iss */
	const char* Audience;       /* 可选自动注入 aud */
	const char* Subject;        /* 可选自动注入 sub */
	int         ExpireSeconds;  /* 0 = 不注入 exp */
	const char* Jti;            /* 可选自动注入 jti */
} xjwtconfig;

/* 签发配置零值（可注入 iss/aud 与自定义 claims）。 */
XRT_API void  xjwtConfigInit(xjwtconfig* pConfig);
/* config 指定的字段覆盖 token 中同名字段，不改变调用方的 claims。 */
XRT_API char* xjwtSign(const xjwtconfig* pConfig, const xvalue* claims);

/* ------------------------------------------------------------------
 * 验证（一步式：验签 + exp/nbf/iss/aud 全检）
 * 通过返回 claims（xrtValueRelease 释放），失败返回 NULL 并设 xerror。
 * keyPem：HS 系列收密钥字符串，RS 系列收公钥 PEM，ES 收公钥 PEM。
 * check 传 NULL 等价于 xjwtCheckInit 后只查 exp。
 * ------------------------------------------------------------------ */
XRT_API xvalue* xjwtVerify(const char* token, const char* keyPem, const xjwtcheck* check);

/* ------------------------------------------------------------------
 * 验证（公钥缓存式）：同一公钥反复验证时避免每次解析 PEM。
 * xjwtKeyParse 解析一次（RSA SPKI/PKCS#1 或 EC P-256 SPKI 公钥 PEM），
 * xjwtVerifyKey 用缓存的密钥验证，线程安全（缓存只读）。
 * ------------------------------------------------------------------ */
typedef struct xjwtkey xjwtkey;

/* 解析公钥 PEM（RSA SPKI/PKCS#1 或 EC P-256）供反复验证。 */
XRT_API xjwtkey* xjwtKeyParse(const char* publicPem);   /* 失败返回 NULL 并设 xerror；OOM 不回退算法 */
XRT_API void     xjwtKeyFree(xjwtkey* key);
/* 用缓存公钥验证 token；返回 claims（xrtValueRelease 释放）。 */
XRT_API xvalue*  xjwtVerifyKey(const char* token, const xjwtkey* key, const xjwtcheck* check);

/* ------------------------------------------------------------------
 * 解码（不验签，调试/信任源）
 * xjwtDecode 返回 claims，xjwtDecodeHeader 返回 header，均由 xrtValueRelease 释放。
 * 对应 JSON 必须是对象；xjwtDecode 传 pAlg 时也要求 header 成功解码。
 * *pAlg 收 header 算法（未知为 INVALID），失败时置 INVALID。
 * *pKid 为堆拷贝，由调用方 xrtFree；缺失或失败为 NULL，显式空 kid 为 ""。
 * 传 pKid 时拒绝非字符串、≥256 字节或含 NUL 的 kid，不做前缀匹配。
 * ------------------------------------------------------------------ */
XRT_API xvalue* xjwtDecode(const char* token, int* pAlg);
/* 只解 header 不验签（取 alg/kid 供路由选择）。 */
XRT_API xvalue* xjwtDecodeHeader(const char* token, int* pAlg, const char** pKid);

/* ------------------------------------------------------------------
 * claims 独立校验（手工流程用）
 * ------------------------------------------------------------------ */
XRT_API bool xjwtClaimsValid(const xvalue* claims, const xjwtcheck* check);

/* 取字符串 claim 到调用方缓冲（超长安全截断到 cap-1 并补零）。
 * claim 不存在或不是字符串返回 false。 */
XRT_API bool xjwtClaimString(const xvalue* claims, const char* key,
                     char* pOut, size_t iCap);

/* ------------------------------------------------------------------
 * 错误便捷读取：返回当前执行上下文中 xjwt 域错误码
 * （XJWT_ERROR_*，见上），当前错误不属于 xjwt 时返回 0。
 * 语义：仅在 xjwt 调用失败后立即读取；成功调用不保证清除旧错误。
 * ------------------------------------------------------------------ */
XRT_API int xjwtLastError(void);

/* ------------------------------------------------------------------
 * JWKS (RFC 7517)：最多 16 把密钥（超出整体解析失败）。
 * 不支持或畸形的单项密钥会跳过；分配失败使整组解析失败，不返回部分密钥集。
 * kid 严格匹配：token 带 kid 时须为字符串并精确命中带同一 kid 的条目；
 * 空字符串与缺失不同，含 NUL 不支持；JWK kid 须短于 128 字节。
 * token 无 kid 时取第一把可用密钥。
 * ------------------------------------------------------------------ */
typedef struct xjwtjwks xjwtjwks;

/* 解析 JWKS 公钥集 JSON。 */
XRT_API xjwtjwks* xjwtJwksParse(const char* json);
/* 释放 JWKS 公钥集。 */
XRT_API void      xjwtJwksFree(xjwtjwks* pJwks);
/* 在 JWKS 集内按 kid 自动选钥验证 token。 */
XRT_API xvalue*   xjwtVerifyJwks(const char* token, const xjwtjwks* jwks, const xjwtcheck* check);

/* ------------------------------------------------------------------
 * 工具
 * ------------------------------------------------------------------ */
XRT_API const char* xjwtAlgName(int alg);   /* "HS256" 等，未知返回 NULL */
XRT_API int         xjwtAlgParse(const char* name);  /* "HS256" → XJWT_ALG_HS256 */

#ifdef __cplusplus
}
#endif

#endif /* selected xjwt */

#endif


/* ========================================================================== */
/* public: extlibs/xjwt/include/xjwt.h */
/* ========================================================================== */

#ifndef XJWT_H
#define XJWT_H


#endif

#endif

#if defined(XJWT_IMPLEMENTATION) && !defined(XJWT_IMPLEMENTATION_ONCE)
#define XJWT_IMPLEMENTATION_ONCE 1


/* ========================================================================== */
/* internal: extlibs/xjwt/src/internal/xjwt_internal.h */
/* ========================================================================== */

#if defined(XJWT_FEATURE_XJWT)
/* xjwt 内部头：跨分片共享的工具与数据结构。 */
#ifndef XJWT_INTERNAL_H
#define XJWT_INTERNAL_H


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
#endif


/* ========================================================================== */
/* source: extlibs/xjwt/src/jwt/xjwt_core.c */
/* ========================================================================== */

#if defined(XJWT_FEATURE_XJWT)
/* Base64URL 编解码 + token 组装/拆解 + 错误设置。 */

/* ------------------------------------------------------------------ */
/* Base64URL                                                           */
/* ------------------------------------------------------------------ */

static const char s_B64url_chars[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

char* xjwt__base64url_encode(const void* pData, size_t iSize)
{
	const unsigned char* p = (const unsigned char*)pData;
	if ( iSize > SIZE_MAX - 2 ||
		(iSize + 2) / 3 > (SIZE_MAX - 1) / 4 ) {
		xjwt__error(XJWT_ERROR_ARGUMENT, "base64url encode: input too large");
		return NULL;
	}
	size_t iOutSize = (iSize + 2) / 3 * 4;
	/* 去掉 padding：尾组 1 字节→2 字符，2 字节→3 字符 */
	if ( iSize % 3 == 1 ) iOutSize -= 2;
	else if ( iSize % 3 == 2 ) iOutSize -= 1;
	char* sOut = (char*)xrtMalloc(iOutSize + 1);
	if ( sOut == NULL ) return NULL;
	size_t j = 0;
	for ( size_t i = 0; i < iSize; i += 3 ) {
		uint32_t v = (uint32_t)p[i] << 16;
		if ( i + 1 < iSize ) v |= (uint32_t)p[i+1] << 8;
		if ( i + 2 < iSize ) v |= p[i+2];
		sOut[j++] = s_B64url_chars[(v >> 18) & 63];
		sOut[j++] = s_B64url_chars[(v >> 12) & 63];
		if ( i + 1 < iSize ) sOut[j++] = s_B64url_chars[(v >> 6) & 63];
		if ( i + 2 < iSize ) sOut[j++] = s_B64url_chars[v & 63];
	}
	sOut[j] = 0;
	return sOut;
}

static int b64url_val(char c)
{
	if ( c >= 'A' && c <= 'Z' ) return c - 'A';
	if ( c >= 'a' && c <= 'z' ) return c - 'a' + 26;
	if ( c >= '0' && c <= '9' ) return c - '0' + 52;
	if ( c == '-' ) return 62;
	if ( c == '_' ) return 63;
	return -1;
}

size_t xjwt__base64url_decode_size(const char* sText, size_t iTextSize)
{
	(void)sText;
	if ( iTextSize == 0 ) return 0;
	size_t rem = iTextSize % 4;
	if ( rem == 1 ) return 0;  /* 非法 */
	size_t full = iTextSize / 4 * 3;
	if ( rem == 2 ) return full + 1;
	if ( rem == 3 ) return full + 2;
	return full;
}

unsigned char* xjwt__base64url_decode(const char* sText, size_t iTextSize, size_t* pOutSize)
{
	size_t iOutSize = xjwt__base64url_decode_size(sText, iTextSize);
	if ( iOutSize == 0 && iTextSize > 0 ) {
		xjwt__error(XJWT_ERROR_MALFORMED, "base64url decode: invalid length");
		return NULL;
	}
	unsigned char* pOut = (unsigned char*)xrtMalloc(iOutSize > 0 ? iOutSize : 1);
	if ( pOut == NULL ) return NULL;
	size_t j = 0;
	for ( size_t i = 0; i < iTextSize; i += 4 ) {
		uint32_t v = 0;
		int n = 0;
		int iLast = 0;
		for ( int k = 0; k < 4 && i + k < iTextSize; k++ ) {
			int d = b64url_val(sText[i+k]);
			if ( d < 0 ) {
				xrtFree(pOut);
				xjwt__error(XJWT_ERROR_MALFORMED, "base64url decode: invalid character");
				return NULL;
			}
			v = (v << 6) | (uint32_t)d;
			iLast = d;
			n++;
		}
		/* RFC 4648 §3.5：末组未使用位必须为零，避免同一签名有多个字符串。 */
		if ( (n == 2 && (iLast & 15) != 0) ||
			(n == 3 && (iLast & 3) != 0) ) {
			xrtFree(pOut);
			xjwt__error(XJWT_ERROR_MALFORMED,
				"base64url decode: nonzero pad bits");
			return NULL;
		}
		v <<= (4 - n) * 6;
		if ( n >= 2 ) pOut[j++] = (unsigned char)(v >> 16);
		if ( n >= 3 ) pOut[j++] = (unsigned char)(v >> 8);
		if ( n >= 4 ) pOut[j++] = (unsigned char)v;
	}
	*pOutSize = j;
	return pOut;
}

/* ------------------------------------------------------------------ */
/* token 拆解 / 组装                                                    */
/* ------------------------------------------------------------------ */

bool xjwt__split(const char* sToken,
                 const char** pHead, size_t* pHeadSize,
                 const char** pClaims, size_t* pClaimsSize,
                 const char** pSig, size_t* pSigSize)
{
	if ( sToken == NULL ) return false;
	const char* p1 = strchr(sToken, '.');
	if ( p1 == NULL ) return false;
	const char* p2 = strchr(p1 + 1, '.');
	if ( p2 == NULL ) return false;
	/* RFC 7515：JWS 精确三段——signature 段内不允许再出现 '.' */
	if ( strchr(p2 + 1, '.') != NULL ) return false;
	*pHead = sToken;
	*pHeadSize = (size_t)(p1 - sToken);
	*pClaims = p1 + 1;
	*pClaimsSize = (size_t)(p2 - p1 - 1);
	*pSig = p2 + 1;
	*pSigSize = strlen(p2 + 1);
	return *pHeadSize > 0 && *pClaimsSize > 0;
}

char* xjwt__join(const char* sHeadJson, const char* sClaimsJson,
                 const void* pSig, size_t iSigSize)
{
	char* sHeadB64 = xjwt__base64url_encode(sHeadJson, strlen(sHeadJson));
	if ( sHeadB64 == NULL ) return NULL;
	char* sClaimsB64 = xjwt__base64url_encode(sClaimsJson, strlen(sClaimsJson));
	if ( sClaimsB64 == NULL ) { xrtFree(sHeadB64); return NULL; }
	char* sSigB64 = xjwt__base64url_encode(pSig, iSigSize);
	if ( sSigB64 == NULL ) { xrtFree(sHeadB64); xrtFree(sClaimsB64); return NULL; }

	size_t n = strlen(sHeadB64) + 1 + strlen(sClaimsB64) + 1 + strlen(sSigB64) + 1;
	char* sOut = (char*)xrtMalloc(n);
	if ( sOut != NULL ) {
		strcpy(sOut, sHeadB64);
		strcat(sOut, ".");
		strcat(sOut, sClaimsB64);
		strcat(sOut, ".");
		strcat(sOut, sSigB64);
	}
	xrtFree(sHeadB64); xrtFree(sClaimsB64); xrtFree(sSigB64);
	return sOut;
}

/* ------------------------------------------------------------------ */
/* 错误                                                                */
/* ------------------------------------------------------------------ */

void xjwt__error(int iCode, const char* sMessage)
{
	xrtSetErrorInfo(XERR_STATE, "xrt.jwt", iCode, sMessage);
}

bool xjwt__memory_error(void)
{
	const xerror* pError = xrtGetError();
	return pError != NULL && xrtErrorKind(pError) == XERR_MEMORY;
}

void xjwt__error_unless_memory(int iCode, const char* sMessage)
{
	if ( !xjwt__memory_error() )
		xjwt__error(iCode, sMessage);
}
#endif


/* ========================================================================== */
/* source: extlibs/xjwt/src/jwt/xjwt_sig.c */
/* ========================================================================== */

#if defined(XJWT_FEATURE_XJWT)
/* 签名与验签：按算法分派到 xrt HMAC/RSA PKCS1/ECDSA P-256。 */

/* ------------------------------------------------------------------ */
/* 算法 → 哈希枚举                                                      */
/* ------------------------------------------------------------------ */
static xcryptohash alg_hash(int alg)
{
	switch ( alg ) {
	case XJWT_ALG_HS256: case XJWT_ALG_RS256: case XJWT_ALG_ES256:
		return XCRYPTO_HASH_SHA256;
	case XJWT_ALG_HS384: case XJWT_ALG_RS384:
		return XCRYPTO_HASH_SHA384;
	case XJWT_ALG_HS512: case XJWT_ALG_RS512:
		return XCRYPTO_HASH_SHA512;
	}
	return (xcryptohash)0;
}

/* ------------------------------------------------------------------ */
/* HMAC 签名（HS256/384/512）                                           */
/* ------------------------------------------------------------------ */
static bool hmac_sign(int alg, const void* pData, size_t iSize,
                      const char* sSecret,
                      unsigned char* pOut, size_t iCapacity, size_t* pOutSize)
{
	size_t iKeySize = strlen(sSecret);
	size_t iMacSize = xrtCryptoHashSize(alg_hash(alg));
	if ( iCapacity < iMacSize ) {
		xjwt__error(XJWT_ERROR_ARGUMENT, "output buffer too small for HMAC");
		return false;
	}
	switch ( alg ) {
	case XJWT_ALG_HS256:
		if ( !xrtHmacSha256(sSecret, iKeySize, pData, iSize, pOut) ) return false;
		break;
	case XJWT_ALG_HS384:
		if ( !xrtHmacSha384(sSecret, iKeySize, pData, iSize, pOut) ) return false;
		break;
	case XJWT_ALG_HS512:
		if ( !xrtHmacSha512(sSecret, iKeySize, pData, iSize, pOut) ) return false;
		break;
	default: return false;
	}
	*pOutSize = iMacSize;
	return true;
}

/* ------------------------------------------------------------------ */
/* RSA PEM 解析（SPKI 或 PKCS#1 → xrt 公钥视图）                        */
/* ------------------------------------------------------------------ */
static bool rsa_public_der_parse(const void* pData, size_t iSize,
	bool bSpki, xbytesview* pModulus, xbytesview* pExponent)
{
	static const unsigned char aRsaOid[] = {
		0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01
	};
	xdercursor Outer, Body, Key;
	xdervalue Value;

	if ( !xrtDerValidate(pData, iSize) ||
		!xrtDerInit(&Outer, pData, iSize) ||
		xrtDerRead(&Outer, &Value) != XDER_VALUE ||
		!xrtDerIs(&Value, XASN1_UNIVERSAL, XASN1_SEQUENCE, true) ||
		!xrtDerDone(&Outer) || !xrtDerEnter(&Value, &Body) ) {
		return false;
	}
	if ( bSpki ) {
		xdercursor Algorithm;
		xbytesview EncodedKey;
		uint8 iUnused;

		if ( xrtDerRead(&Body, &Value) != XDER_VALUE ||
			!xrtDerIs(&Value, XASN1_UNIVERSAL, XASN1_SEQUENCE, true) ||
			!xrtDerEnter(&Value, &Algorithm) ||
			xrtDerRead(&Algorithm, &Value) != XDER_VALUE ||
			!xrtDerOidEqual(&Value, aRsaOid, sizeof(aRsaOid)) ||
			xrtDerRead(&Algorithm, &Value) != XDER_VALUE ||
			!xrtDerIs(&Value, XASN1_UNIVERSAL, XASN1_NULL, false) ||
			(Value.Value.Size != 0) || !xrtDerDone(&Algorithm) ||
			xrtDerRead(&Body, &Value) != XDER_VALUE ||
			!xrtDerBitString(&Value, &EncodedKey, &iUnused) ||
			(iUnused != 0) || !xrtDerDone(&Body) ||
			!xrtDerInit(&Key, EncodedKey.Data, EncodedKey.Size) ||
			xrtDerRead(&Key, &Value) != XDER_VALUE ||
			!xrtDerIs(&Value, XASN1_UNIVERSAL, XASN1_SEQUENCE, true) ||
			!xrtDerDone(&Key) || !xrtDerEnter(&Value, &Key) ) {
			return false;
		}
	} else {
		Key = Body;
	}
	return xrtDerRead(&Key, &Value) == XDER_VALUE &&
		xrtDerUnsigned(&Value, pModulus) &&
		xrtDerRead(&Key, &Value) == XDER_VALUE &&
		xrtDerUnsigned(&Value, pExponent) && xrtDerDone(&Key);
}

bool xjwt__rsa_public_parse(const char* sPem, xrsapublickey* pKey,
                            unsigned char** ppOwned)
{
	xpemblock tBlock;
	xbytesview Modulus, Exponent;
	xrsapublickey Parsed;
	unsigned char *pModulus = NULL, *pExponent = NULL, **ppArr = NULL;
	bool bSpki;
	size_t iPemSize;

	if ( (sPem == NULL) || (pKey == NULL) || (ppOwned == NULL) ) {
		xjwt__error(XJWT_ERROR_ARGUMENT, "RSA public key argument is null");
		return false;
	}
	iPemSize = strlen(sPem);
	bSpki = xrtPemFind(sPem, iPemSize, "PUBLIC KEY", &tBlock);
	if ( !bSpki &&
		!xrtPemFind(sPem, iPemSize, "RSA PUBLIC KEY", &tBlock) ) {
		xjwt__error(XJWT_ERROR_PARSE, "PEM public key not found");
		return false;
	}
	size_t iDerSize = 0;
	bytes pDer = xrtPemDecodeNew(&tBlock, &iDerSize);
	if ( pDer == NULL ) {
		xjwt__error_unless_memory(XJWT_ERROR_PARSE,
			"PEM decode failed");
		return false;
	}
	if ( !rsa_public_der_parse(pDer, iDerSize, bSpki,
		&Modulus, &Exponent) ) {
		goto invalid;
	}
	Parsed.Modulus = Modulus.Data;
	Parsed.ModulusSize = Modulus.Size;
	Parsed.Exponent = Exponent.Data;
	Parsed.ExponentSize = Exponent.Size;
	if ( !xjwt__rsa_jwa_key_valid(&Parsed) ) goto invalid;
	pModulus = (unsigned char*)xrtMalloc(Modulus.Size);
	pExponent = (unsigned char*)xrtMalloc(Exponent.Size);
	ppArr = (unsigned char**)xrtMalloc(2u * sizeof(*ppArr));
	if ( (pModulus == NULL) || (pExponent == NULL) || (ppArr == NULL) ) {
		goto cleanup;
	}
	memcpy(pModulus, Modulus.Data, Modulus.Size);
	memcpy(pExponent, Exponent.Data, Exponent.Size);
	ppArr[0] = pModulus;
	ppArr[1] = pExponent;
	pKey->Modulus = pModulus;
	pKey->ModulusSize = Modulus.Size;
	pKey->Exponent = pExponent;
	pKey->ExponentSize = Exponent.Size;
	*ppOwned = (unsigned char*)ppArr;
	xrtFree(pDer);
	return true;

invalid:
	xjwt__error(XJWT_ERROR_PARSE, "RSA public key DER parse failed");
cleanup:
	xrtFree(pModulus);
	xrtFree(pExponent);
	xrtFree(ppArr);
	xrtFree(pDer);
	return false;
}

void xjwt__rsa_public_free(xrsapublickey* pKey, unsigned char* pOwned)
{
	if ( pOwned != NULL ) {
		unsigned char** ppArr = (unsigned char**)pOwned;
		xrtFree(ppArr[0]);
		xrtFree(ppArr[1]);
		xrtFree(pOwned);
	}
	(void)pKey;
}

/* ------------------------------------------------------------------ */
/* RSA 签名                                                            */
/* ------------------------------------------------------------------ */
static bool rsa_sign(int alg, const void* pData, size_t iSize,
                     const char* sPrivatePem,
                     unsigned char* pOut, size_t iCapacity, size_t* pOutSize)
{
	return rsa_sign_impl(alg, pData, iSize, sPrivatePem, pOut, iCapacity, pOutSize);
}

/* ------------------------------------------------------------------ */
/* ECDSA P-256                                                         */
/* ------------------------------------------------------------------ */
/* ECDSA 公钥解析完整版在 xjwt_ext.c */

/* ------------------------------------------------------------------ */
/* 分派                                                                */
/* ------------------------------------------------------------------ */
bool xjwt__sign(int alg, const void* pData, size_t iSize,
                const char* sKeyPem, unsigned char* pOut,
                size_t iCapacity, size_t* pOutSize)
{
	switch ( alg ) {
	case XJWT_ALG_HS256: case XJWT_ALG_HS384: case XJWT_ALG_HS512:
		return hmac_sign(alg, pData, iSize, sKeyPem, pOut, iCapacity, pOutSize);
	case XJWT_ALG_RS256: case XJWT_ALG_RS384: case XJWT_ALG_RS512:
		return rsa_sign(alg, pData, iSize, sKeyPem, pOut, iCapacity, pOutSize);
	case XJWT_ALG_ES256:
		return es256_sign_impl(pData, iSize, sKeyPem, pOut, iCapacity, pOutSize);
	}
	xjwt__error(XJWT_ERROR_ALG_MISMATCH, "unsupported algorithm");
	return false;
}

bool xjwt__verify(int alg, const void* pData, size_t iSize,
                  const char* sKeyPem, const void* pSig, size_t iSigSize)
{
	/* 安全检查：拒绝 alg=none / alg 未知 */
	if ( alg <= XJWT_ALG_NONE || alg == XJWT_ALG_INVALID ) {
		xjwt__error(XJWT_ERROR_ALG_MISMATCH, "algorithm 'none' or invalid rejected");
		return false;
	}
	/* 安全检查：key 类型必须与算法族匹配（防算法混淆攻击）
	 * HS 族只能收 HMAC 密钥；RS 族与 ES 族只能收 PEM 公钥 */
	bool bIsPem = sKeyPem != NULL && strlen(sKeyPem) > 20 &&
	              strstr(sKeyPem, "-----BEGIN") != NULL;
	switch ( alg ) {
	case XJWT_ALG_HS256: case XJWT_ALG_HS384: case XJWT_ALG_HS512:
		if ( bIsPem ) {
			xjwt__error(XJWT_ERROR_ALG_MISMATCH,
				"HMAC algorithm with PEM key rejected (alg confusion)");
			return false;
		}
		break;
	case XJWT_ALG_RS256: case XJWT_ALG_RS384: case XJWT_ALG_RS512: case XJWT_ALG_ES256:
		if ( !bIsPem ) {
			xjwt__error(XJWT_ERROR_ALG_MISMATCH,
				"RSA/EC algorithm requires PEM key (alg confusion)");
			return false;
		}
		break;
	}
	xcryptohash iHash = alg_hash(alg);
	if ( iHash == 0 ) {
		xjwt__error(XJWT_ERROR_ALG_MISMATCH, "unsupported algorithm");
		return false;
	}

	switch ( alg ) {
	case XJWT_ALG_HS256: {
		unsigned char aMac[32];
		if ( !xrtHmacSha256(sKeyPem, strlen(sKeyPem), pData, iSize, aMac) ) {
			xjwt__error_unless_memory(XJWT_ERROR_SIGNATURE,
				"HS256 HMAC computation failed");
			return false;
		}
		if ( iSigSize != 32 ) { xjwt__error(XJWT_ERROR_SIGNATURE, "HS256 signature size mismatch"); return false; }
		if ( !xrtConstTimeEqual(aMac, pSig, 32) ) {
			xjwt__error(XJWT_ERROR_SIGNATURE, "HS256 signature mismatch");
			return false;
		}
		return true;
	}
	case XJWT_ALG_HS384: {
		unsigned char aMac[48];
		if ( !xrtHmacSha384(sKeyPem, strlen(sKeyPem), pData, iSize, aMac) ) {
			xjwt__error_unless_memory(XJWT_ERROR_SIGNATURE,
				"HS384 HMAC computation failed");
			return false;
		}
		if ( iSigSize != 48 ) { xjwt__error(XJWT_ERROR_SIGNATURE, "HS384 signature size mismatch"); return false; }
		if ( !xrtConstTimeEqual(aMac, pSig, 48) ) {
			xjwt__error(XJWT_ERROR_SIGNATURE, "HS384 signature mismatch");
			return false;
		}
		return true;
	}
	case XJWT_ALG_HS512: {
		unsigned char aMac[64];
		if ( !xrtHmacSha512(sKeyPem, strlen(sKeyPem), pData, iSize, aMac) ) {
			xjwt__error_unless_memory(XJWT_ERROR_SIGNATURE,
				"HS512 HMAC computation failed");
			return false;
		}
		if ( iSigSize != 64 ) { xjwt__error(XJWT_ERROR_SIGNATURE, "HS512 signature size mismatch"); return false; }
		if ( !xrtConstTimeEqual(aMac, pSig, 64) ) {
			xjwt__error(XJWT_ERROR_SIGNATURE, "HS512 signature mismatch");
			return false;
		}
		return true;
	}
	case XJWT_ALG_RS256: case XJWT_ALG_RS384: case XJWT_ALG_RS512: {
		xrsapublickey tKey;
		unsigned char* pOwned = NULL;
		if ( !xjwt__rsa_public_parse(sKeyPem, &tKey, &pOwned) )
			return false;
		bool ok = xjwt__verify_rsa_raw(pData, iSize, pSig, iSigSize, &tKey, alg);
		xjwt__rsa_public_free(&tKey, pOwned);
		if ( !ok ) xjwt__error_unless_memory(
			XJWT_ERROR_SIGNATURE, "RSA signature mismatch");
		return ok;
	}
	case XJWT_ALG_ES256: {
		bool ok = xjwt__verify_es256_full(pData, iSize, sKeyPem, pSig, iSigSize);
		if ( !ok ) xjwt__error_unless_memory(
			XJWT_ERROR_SIGNATURE, "ES256 signature mismatch");
		return ok;
	}
	}
	xjwt__error(XJWT_ERROR_ALG_MISMATCH, "unsupported algorithm");
	return false;
}

bool xjwt__verify_rsa_raw(const void* pData, size_t iSize,
                          const void* pSig, size_t iSigSize,
                          const xrsapublickey* pKey, int alg)
{
	xcryptohash iHash = alg_hash(alg);
	unsigned char aDigest[64];
	if ( !xjwt__rsa_jwa_key_valid(pKey) ) return false;
	switch ( iHash ) {
	case XCRYPTO_HASH_SHA256:
		if ( !xrtSha256(pData, iSize, aDigest) ) return false;
		break;
	case XCRYPTO_HASH_SHA384:
		if ( !xrtSha384(pData, iSize, aDigest) ) return false;
		break;
	case XCRYPTO_HASH_SHA512:
		if ( !xrtSha512(pData, iSize, aDigest) ) return false;
		break;
	default: return false;
	}
	return xrtRsaPkcs1Verify(pKey, iHash, aDigest, pSig, iSigSize);
}

bool xjwt__verify_es256_raw(const void* pData, size_t iSize,
                            const void* pSig, size_t iSigSize,
                            const unsigned char* pPublic65)
{
	if ( iSigSize != 64 ) return false;
	unsigned char aDigest[32];
	if ( !xrtSha256(pData, iSize, aDigest) ) return false;
	return xrtEcdsaP256Verify(aDigest, 32, pSig, pPublic65);
}
#endif


/* ========================================================================== */
/* source: extlibs/xjwt/src/jwt/xjwt_main.c */
/* ========================================================================== */

#if defined(XJWT_FEATURE_XJWT)
/* claims 校验（exp/nbf/iss/aud）+ 主入口（Sign/Verify/Decode）+ JWKS。 */

/* ------------------------------------------------------------------ */
/* claims 校验                                                         */
/* ------------------------------------------------------------------ */
void xjwtCheckInit(xjwtcheck* pCheck)
{
	if ( pCheck == NULL ) return;
	memset(pCheck, 0, sizeof(*pCheck));
}

int xjwtLastError(void)
{
	const xerror* pErr = xrtGetError();
	if ( pErr == NULL ) return 0;
	cstr sDomain = xrtErrorDomain(pErr);
	if ( sDomain == NULL || strcmp(sDomain, "xrt.jwt") != 0 ) return 0;
	return xrtErrorCode(pErr);
}

bool xjwtClaimsValid(const xvalue* claims, const xjwtcheck* check)
{
	const xvalue* p = (const xvalue*)claims;
	if ( p == NULL ) {
		xjwt__error(XJWT_ERROR_ARGUMENT, "claims is null");
		return false;
	}
	/* RFC 7519 §4：claims set 必须是 JSON 对象；
	 * 数组/标量会让 exp/nbf/iss/aud 检索全部落空（整组校验被绕过） */
	if ( !xrtValueIs(p, XVALUE_OBJECT) ) {
		xjwt__error(XJWT_ERROR_MALFORMED, "claims must be a JSON object");
		return false;
	}
	int64_t now = check && check->NowOverride != 0 ? check->NowOverride
		: (int64_t)xrtTimeUnix(xrtNow());
	int leeway = check ? check->ClockLeeway : 0;
	if ( leeway < 0 ) {
		xjwt__error(XJWT_ERROR_ARGUMENT, "clock leeway must be nonnegative");
		return false;
	}

	/* exp：本接口仅支持整数 NumericDate；存在但类型不支持一律拒绝，
	 * 防止字符串时间戳绕过过期检查 */
	{
		xvalue* pExp = xrtValueObjectGet(p, xrtStrView("exp"));
		if ( pExp != NULL ) {
			int64 exp;
			if ( !xrtValueIs(pExp, XVALUE_INT) || !xrtValueGetInt(pExp, &exp) ) {
				xjwt__error(XJWT_ERROR_EXPIRED, "exp must be an integer NumericDate");
				return false;
			}
			/* RFC 7519 §4.1.4：到期瞬间即失效。先比较顺序再求无符号
			 * 距离，跨越 INT64_MIN/MAX 也不会发生有符号溢出。 */
			if ( now >= exp && (uint64_t)now - (uint64_t)exp >= (uint64_t)leeway ) {
				xjwt__error(XJWT_ERROR_EXPIRED, "token expired");
				return false;
			}
		}
	}
	/* nbf：同 exp，类型错误拒绝 */
	{
		xvalue* pNbf = xrtValueObjectGet(p, xrtStrView("nbf"));
		if ( pNbf != NULL ) {
			int64 nbf;
			if ( !xrtValueIs(pNbf, XVALUE_INT) || !xrtValueGetInt(pNbf, &nbf) ) {
				xjwt__error(XJWT_ERROR_NOT_YET, "nbf must be an integer NumericDate");
				return false;
			}
			if ( nbf > now && (uint64_t)nbf - (uint64_t)now > (uint64_t)leeway ) {
				xjwt__error(XJWT_ERROR_NOT_YET, "token not yet valid");
				return false;
			}
		}
	}
	/* iss */
	if ( check != NULL && check->Issuer != NULL ) {
		xvalue* pIss = xrtValueObjectGet(p, xrtStrView("iss"));
		xstrview sv;
		if ( pIss == NULL || !xrtValueGetString(pIss, &sv) ||
		     sv.Size != strlen(check->Issuer) || !xrtConstTimeEqual(sv.Data, check->Issuer, sv.Size) ) {
			xjwt__error(XJWT_ERROR_ISSUER, "issuer mismatch");
			return false;
		}
	}
	/* aud：RFC 7519 §4.1.3 允许字符串或字符串数组，任一命中即通过 */
	if ( check != NULL && check->Audience != NULL ) {
		xvalue* pAud = xrtValueObjectGet(p, xrtStrView("aud"));
		xstrview sv;
		bool bMatch = false;
		if ( pAud != NULL && xrtValueGetString(pAud, &sv) ) {
			bMatch = sv.Size == strlen(check->Audience) &&
				xrtConstTimeEqual(sv.Data, check->Audience, sv.Size);
		} else if ( pAud != NULL && xrtValueIs(pAud, XVALUE_ARRAY) ) {
			size_t iExpect = strlen(check->Audience);
			size_t n = xrtValueCount(pAud);
			for ( size_t i = 0; i < n && !bMatch; i++ ) {
				xvalue* pElem = xrtValueArrayGet(pAud, (uint32)i);
				if ( pElem != NULL && xrtValueGetString(pElem, &sv) &&
				     sv.Size == iExpect &&
				     xrtConstTimeEqual(sv.Data, check->Audience, iExpect) )
					bMatch = true;
			}
		}
		if ( !bMatch ) {
			xjwt__error(XJWT_ERROR_AUDIENCE, "audience mismatch");
			return false;
		}
	}
	return true;
}

/* ------------------------------------------------------------------ */
/* 字符串 claim 便捷读取                                                 */
/* ------------------------------------------------------------------ */
bool xjwtClaimString(const xvalue* claims, const char* key,
                     char* pOut, size_t iCap)
{
	if ( claims == NULL || key == NULL || pOut == NULL || iCap == 0 )
		return false;
	xvalue* pVal = xrtValueObjectGet(claims, xrtStrView(key));
	xstrview sv;
	if ( pVal == NULL || !xrtValueGetString(pVal, &sv) )
		return false;
	size_t n = sv.Size < iCap - 1 ? sv.Size : iCap - 1;
	memcpy(pOut, sv.Data, n);
	pOut[n] = 0;
	return true;
}

/* ------------------------------------------------------------------ */
/* 主入口：签发                                                          */
/* ------------------------------------------------------------------ */
void xjwtConfigInit(xjwtconfig* pConfig)
{
	if ( pConfig == NULL ) return;
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->Alg = XJWT_ALG_HS256;
}

static bool xjwtSignSet(xvalue* pObject, const char* sKey, xvalue* pValue)
{
	if ( xrtValueObjectSetNew(pObject, xrtStrView(sKey), pValue) )
		return true;
	xjwt__error(XJWT_ERROR_PARSE, "JWT field allocation failed");
	return false;
}

char* xjwtSign(const xjwtconfig* pConfig, const xvalue* claims)
{
	if ( pConfig == NULL || claims == NULL || pConfig->KeyPem == NULL ) {
		xjwt__error(XJWT_ERROR_ARGUMENT, "xjwtSign: null argument");
		return NULL;
	}
	/* RFC 7519 §4：claims set 必须是 JSON 对象 */
	if ( !xrtValueIs(claims, XVALUE_OBJECT) ) {
		xjwt__error(XJWT_ERROR_MALFORMED, "claims must be a JSON object");
		return NULL;
	}
	/* 与 DecodeHeader 的验证侧上限保持一致：超长 kid 直接拒绝签发 */
	if ( pConfig->KeyId != NULL && strlen(pConfig->KeyId) >= 256 ) {
		xjwt__error(XJWT_ERROR_ARGUMENT,
			"KeyId must be shorter than 256 bytes");
		return NULL;
	}
	const char* sAlg = xjwtAlgName(pConfig->Alg);
	if ( sAlg == NULL ) {
		xjwt__error(XJWT_ERROR_ALG_MISMATCH, "unknown algorithm");
		return NULL;
	}
	/* 仅复制顶层对象：xrt 的 COW 外壳隔离字段替换，嵌套值只读序列化。 */
	xvalue* pClaims = xrtValueClone(claims);
	if ( pClaims == NULL ) {
		xjwt__error(XJWT_ERROR_PARSE, "claims clone failed");
		return NULL;
	}
	/* 自动注入标准 claims；成功或失败都不改变调用方对象。 */
	{
		int64_t now = (int64_t)xrtTimeUnix(xrtNow());
		if ( (pConfig->ExpireSeconds != 0 &&
			  !xjwtSignSet(pClaims, "exp", xrtValueInt(now + pConfig->ExpireSeconds))) ||
			 (pConfig->Issuer != NULL &&
			  !xjwtSignSet(pClaims, "iss", xrtValueString(xrtStrView(pConfig->Issuer)))) ||
			 (pConfig->Audience != NULL &&
			  !xjwtSignSet(pClaims, "aud", xrtValueString(xrtStrView(pConfig->Audience)))) ||
			 (pConfig->Subject != NULL &&
			  !xjwtSignSet(pClaims, "sub", xrtValueString(xrtStrView(pConfig->Subject)))) ||
			 (pConfig->Jti != NULL &&
			  !xjwtSignSet(pClaims, "jti", xrtValueString(xrtStrView(pConfig->Jti)))) ||
			 !xjwtSignSet(pClaims, "iat", xrtValueInt(now)) ) {
			xrtValueRelease(pClaims);
			return NULL;
		}
	}

	/* header 经 JSON 序列化构造：kid 自动转义且不受定长缓冲限制 */
	xvalue* pHead = xrtValueObject();
	if ( pHead == NULL ) {
		xrtValueRelease(pClaims);
		xjwt__error(XJWT_ERROR_PARSE, "header object alloc failed");
		return NULL;
	}
	if ( !xjwtSignSet(pHead, "alg", xrtValueString(xrtStrView(sAlg))) ||
		 !xjwtSignSet(pHead, "typ", xrtValueString(xrtStrView("JWT"))) ||
		 (pConfig->KeyId != NULL &&
		  !xjwtSignSet(pHead, "kid", xrtValueString(xrtStrView(pConfig->KeyId)))) ) {
		xrtValueRelease(pHead);
		xrtValueRelease(pClaims);
		return NULL;
	}
	char* sHeadJson = xrtJsonStringify(pHead, false, NULL);
	xrtValueRelease(pHead);
	if ( sHeadJson == NULL ) {
		xrtValueRelease(pClaims);
		xjwt__error(XJWT_ERROR_PARSE, "header stringify failed");
		return NULL;
	}

	/* claims JSON */
	char* sClaimsJson = xrtJsonStringify(pClaims, false, NULL);  /* compact */
	xrtValueRelease(pClaims);
	if ( sClaimsJson == NULL ) {
		xrtFree(sHeadJson);
		xjwt__error(XJWT_ERROR_PARSE, "claims stringify failed");
		return NULL;
	}

	/* signing input = header_b64 + "." + claims_b64 */
	char* sHeadB64 = xjwt__base64url_encode(sHeadJson, strlen(sHeadJson));
	if ( sHeadB64 == NULL ) { xrtFree(sHeadJson); xrtFree(sClaimsJson); return NULL; }
	char* sClaimsB64 = xjwt__base64url_encode(sClaimsJson, strlen(sClaimsJson));
	if ( sClaimsB64 == NULL ) { xrtFree(sHeadB64); xrtFree(sHeadJson); xrtFree(sClaimsJson); return NULL; }

	size_t n = strlen(sHeadB64) + 1 + strlen(sClaimsB64);
	char* sInput = (char*)xrtMalloc(n + 1);
	if ( sInput == NULL ) {
		xrtFree(sHeadB64); xrtFree(sClaimsB64);
		xrtFree(sHeadJson); xrtFree(sClaimsJson);
		return NULL;
	}
	strcpy(sInput, sHeadB64);
	strcat(sInput, ".");
	strcat(sInput, sClaimsB64);

	/* sign：缓冲覆盖 xrt 全模数上限（RSA-8192 = 1024 字节） */
	unsigned char aSig[XRT_RSA_MAX_MODULUS_SIZE];
	size_t iSigSize = 0;
	if ( !xjwt__sign(pConfig->Alg, sInput, n, pConfig->KeyPem,
	                 aSig, sizeof(aSig), &iSigSize) ) {
		xrtFree(sInput); xrtFree(sHeadB64); xrtFree(sClaimsB64);
		xrtFree(sHeadJson); xrtFree(sClaimsJson);
		return NULL;
	}

	/* join */
	char* sOut = xjwt__join(sHeadJson, sClaimsJson, aSig, iSigSize);
	xrtFree(sInput); xrtFree(sHeadB64); xrtFree(sClaimsB64);
	xrtFree(sHeadJson); xrtFree(sClaimsJson);
	return sOut;
}

/* 一步式便捷 */
char* xjwtHs256(const xvalue* claims, const char* secret, int expireSeconds)
{
	xjwtconfig cfg; xjwtConfigInit(&cfg);
	cfg.Alg = XJWT_ALG_HS256; cfg.KeyPem = secret; cfg.ExpireSeconds = expireSeconds;
	return xjwtSign(&cfg, claims);
}
char* xjwtHs384(const xvalue* claims, const char* secret, int expireSeconds)
{
	xjwtconfig cfg; xjwtConfigInit(&cfg);
	cfg.Alg = XJWT_ALG_HS384; cfg.KeyPem = secret; cfg.ExpireSeconds = expireSeconds;
	return xjwtSign(&cfg, claims);
}
char* xjwtHs512(const xvalue* claims, const char* secret, int expireSeconds)
{
	xjwtconfig cfg; xjwtConfigInit(&cfg);
	cfg.Alg = XJWT_ALG_HS512; cfg.KeyPem = secret; cfg.ExpireSeconds = expireSeconds;
	return xjwtSign(&cfg, claims);
}
char* xjwtRs256(const xvalue* claims, const char* privatePem, int expireSeconds)
{
	xjwtconfig cfg; xjwtConfigInit(&cfg);
	cfg.Alg = XJWT_ALG_RS256; cfg.KeyPem = privatePem; cfg.ExpireSeconds = expireSeconds;
	return xjwtSign(&cfg, claims);
}
char* xjwtRs384(const xvalue* claims, const char* privatePem, int expireSeconds)
{
	xjwtconfig cfg; xjwtConfigInit(&cfg);
	cfg.Alg = XJWT_ALG_RS384; cfg.KeyPem = privatePem; cfg.ExpireSeconds = expireSeconds;
	return xjwtSign(&cfg, claims);
}
char* xjwtRs512(const xvalue* claims, const char* privatePem, int expireSeconds)
{
	xjwtconfig cfg; xjwtConfigInit(&cfg);
	cfg.Alg = XJWT_ALG_RS512; cfg.KeyPem = privatePem; cfg.ExpireSeconds = expireSeconds;
	return xjwtSign(&cfg, claims);
}
char* xjwtEs256(const xvalue* claims, const char* privatePem, int expireSeconds)
{
	xjwtconfig cfg; xjwtConfigInit(&cfg);
	cfg.Alg = XJWT_ALG_ES256; cfg.KeyPem = privatePem; cfg.ExpireSeconds = expireSeconds;
	return xjwtSign(&cfg, claims);
}

/* ------------------------------------------------------------------ */
/* 主入口：验证                                                          */
/* ------------------------------------------------------------------ */

/* 三条验证路径共用前置：header(alg+kid) → claims 解码校验 → 重建签名输入 → 解码签名。
 * 成功返回 claims 并交出中间量（调用方 xrtFree）；失败返回 NULL。 */
xvalue* xjwt__verify_prepare(const char* sToken, const xjwtcheck* pCheck,
                             int* pAlg, const char** pKid,
                             char** psInput, size_t* pn,
                             unsigned char** ppSig, size_t* pSigSize)
{
	*pKid = NULL;

	/* header：alg + kid */
	xvalue* pHdr = xjwtDecodeHeader(sToken, pAlg, pKid);
	if ( pHdr == NULL ) return NULL;
	xrtValueRelease(pHdr);
	if ( *pAlg == XJWT_ALG_INVALID ) {
		xrtFree((void*)*pKid); *pKid = NULL;
		xjwt__error(XJWT_ERROR_ALG_MISMATCH, "unknown alg in header");
		return NULL;
	}

	/* claims 解码（不验签）+ 校验（exp/nbf/iss/aud） */
	xvalue* claims = xjwtDecode(sToken, NULL);
	if ( claims == NULL ) goto fail;
	if ( !xjwtClaimsValid(claims, pCheck) ) {
		xrtValueRelease((xvalue*)claims);
		goto fail;
	}

	/* 拆段并重建签名输入 */
	const char *pHead, *pClaimsB64, *pSigB64;
	size_t iHeadSize, iClaimsSize, iSigSize;
	if ( !xjwt__split(sToken, &pHead, &iHeadSize, &pClaimsB64, &iClaimsSize, &pSigB64, &iSigSize) ) {
		xrtValueRelease((xvalue*)claims);
		xjwt__error(XJWT_ERROR_MALFORMED, "token split failed");
		goto fail;
	}
	size_t n = iHeadSize + 1 + iClaimsSize;
	char* sInput = (char*)xrtMalloc(n + 1);
	if ( sInput == NULL ) {
		xrtValueRelease((xvalue*)claims);
		goto fail;
	}
	memcpy(sInput, pHead, iHeadSize);
	sInput[iHeadSize] = '.';
	memcpy(sInput + iHeadSize + 1, pClaimsB64, iClaimsSize);
	sInput[n] = 0;

	/* 解码签名 */
	size_t iSigBinSize = 0;
	unsigned char* pSigBin = xjwt__base64url_decode(pSigB64, iSigSize, &iSigBinSize);
	if ( pSigBin == NULL ) {
		xrtFree(sInput);
		xrtValueRelease((xvalue*)claims);
		goto fail;
	}

	*psInput = sInput; *pn = n; *ppSig = pSigBin; *pSigSize = iSigBinSize;
	return claims;

fail:
	xrtFree((void*)*pKid); *pKid = NULL;
	return NULL;
}

xvalue* xjwtVerify(const char* token, const char* keyPem, const xjwtcheck* check)
{
	if ( token == NULL || keyPem == NULL ) {
		xjwt__error(XJWT_ERROR_ARGUMENT, "xjwtVerify: null argument");
		return NULL;
	}

	int alg = XJWT_ALG_INVALID;
	const char* sKid = NULL;
	char* sInput = NULL;
	unsigned char* pSigBin = NULL;
	size_t n = 0, iSigBinSize = 0;
	xvalue* claims = xjwt__verify_prepare(token, check, &alg, &sKid,
	                                      &sInput, &n, &pSigBin, &iSigBinSize);
	if ( claims == NULL ) return NULL;
	xrtFree((void*)sKid);  /* 单钥 PEM 路径不选钥，kid 不参与 */

	bool ok = xjwt__verify(alg, sInput, n, keyPem, pSigBin, iSigBinSize);
	xrtFree(sInput); xrtFree(pSigBin);
	if ( !ok ) {
		xrtValueRelease((xvalue*)claims);
		return NULL;
	}
	return claims;
}

/* ------------------------------------------------------------------ */
/* 解码（不验签）                                                        */
/* ------------------------------------------------------------------ */
xvalue* xjwtDecode(const char* token, int* pAlg)
{
	if ( pAlg != NULL ) *pAlg = XJWT_ALG_INVALID;
	if ( token == NULL ) {
		xjwt__error(XJWT_ERROR_ARGUMENT, "token is null");
		return NULL;
	}
	const char *pHead, *pClaims, *pSig;
	size_t iHeadSize, iClaimsSize, iSigSize;
	if ( !xjwt__split(token, &pHead, &iHeadSize, &pClaims, &iClaimsSize, &pSig, &iSigSize) ) {
		xjwt__error(XJWT_ERROR_MALFORMED, "invalid token format");
		return NULL;
	}
	size_t n = 0;
	unsigned char* pJson = xjwt__base64url_decode(pClaims, iClaimsSize, &n);
	if ( pJson == NULL ) return NULL;
	xvalue* p = xrtJsonParse(xrtStrViewN((cstr)pJson, n));
	xrtFree(pJson);
	if ( p == NULL ) {
		xjwt__error_unless_memory(XJWT_ERROR_PARSE,
			"claims JSON parse failed");
		return NULL;
	}
	if ( !xrtValueIs(p, XVALUE_OBJECT) ) {
		xrtValueRelease(p);
		xjwt__error(XJWT_ERROR_PARSE, "claims must be a JSON object");
		return NULL;
	}
	if ( pAlg != NULL ) {
		/* 请求 header 输出时不能交付仅成功解析的 claims。 */
		xvalue* pHeader = xjwtDecodeHeader(token, pAlg, NULL);
		if ( pHeader == NULL ) {
			xrtValueRelease(p);
			return NULL;
		}
		xrtValueRelease(pHeader);
	}
	return p;
}

/* ------------------------------------------------------------------ */
/* 解码 header（不验签）：kid 为堆拷贝，调用方 xrtFree                    */
/* ------------------------------------------------------------------ */
xvalue* xjwtDecodeHeader(const char* token, int* pAlg, const char** pKid)
{
	if ( pAlg != NULL ) *pAlg = XJWT_ALG_INVALID;
	if ( pKid != NULL ) *pKid = NULL;
	if ( token == NULL ) {
		xjwt__error(XJWT_ERROR_ARGUMENT, "token is null");
		return NULL;
	}
	const char *pHead, *pClaims, *pSig;
	size_t iHeadSize, iClaimsSize, iSigSize;
	if ( !xjwt__split(token, &pHead, &iHeadSize, &pClaims, &iClaimsSize, &pSig, &iSigSize) ) {
		xjwt__error(XJWT_ERROR_MALFORMED, "invalid token format");
		return NULL;
	}
	size_t n = 0;
	unsigned char* pJson = xjwt__base64url_decode(pHead, iHeadSize, &n);
	if ( pJson == NULL ) return NULL;
	xvalue* p = xrtJsonParse(xrtStrViewN((cstr)pJson, n));
	xrtFree(pJson);
	if ( p == NULL ) {
		xjwt__error_unless_memory(XJWT_ERROR_PARSE,
			"header JSON parse failed");
		return NULL;
	}
	if ( !xrtValueIs(p, XVALUE_OBJECT) ) {
		xrtValueRelease(p);
		xjwt__error(XJWT_ERROR_PARSE, "header must be a JSON object");
		return NULL;
	}
	int alg = XJWT_ALG_INVALID;
	if ( pAlg != NULL ) {
		xstrview sv;
		if ( xrtValueGetString(xrtValueObjectGet(p, xrtStrView("alg")), &sv) ) {
			char a[16];
			if ( sv.Size < sizeof(a) && memchr(sv.Data, 0, sv.Size) == NULL ) {
				memcpy(a, sv.Data, sv.Size); a[sv.Size] = 0;
				alg = xjwtAlgParse(a);
			}
		}
	}
	if ( pKid != NULL ) {
		xstrview sv;
		xvalue* pKidVal = xrtValueObjectGet(p, xrtStrView("kid"));
		if ( pKidVal != NULL ) {
			if ( !xrtValueGetString(pKidVal, &sv) ) {
				xrtValueRelease(p);
				xjwt__error(XJWT_ERROR_PARSE,
					"kid must be a string");
				return NULL;
			}
			/* 超长 kid 视为"存在但不可用"：整条验证拒绝，
			 * 不允许退化成"无 kid 取第一把钥"绕过严格匹配 */
			if ( sv.Size >= 256 || memchr(sv.Data, 0, sv.Size) != NULL ) {
				xrtValueRelease(p);
				xjwt__error(XJWT_ERROR_PARSE, "kid too long or contains NUL");
				return NULL;
			}
			char* s = (char*)xrtMalloc(sv.Size + 1);
			if ( s == NULL ) {
				xrtValueRelease(p);
				return NULL;
			}
			memcpy(s, sv.Data, sv.Size); s[sv.Size] = 0;
			*pKid = s;
		}
	}
	if ( pAlg != NULL ) *pAlg = alg;
	return p;
}

/* ------------------------------------------------------------------ */
/* 算法名                                                              */
/* ------------------------------------------------------------------ */
const char* xjwtAlgName(int alg)
{
	switch ( alg ) {
	case XJWT_ALG_HS256: return "HS256";
	case XJWT_ALG_HS384: return "HS384";
	case XJWT_ALG_HS512: return "HS512";
	case XJWT_ALG_RS256: return "RS256";
	case XJWT_ALG_RS384: return "RS384";
	case XJWT_ALG_RS512: return "RS512";
	case XJWT_ALG_ES256: return "ES256";
	}
	return NULL;
}

int xjwtAlgParse(const char* name)
{
	if ( name == NULL ) return XJWT_ALG_INVALID;
	if ( strcmp(name, "HS256") == 0 ) return XJWT_ALG_HS256;
	if ( strcmp(name, "HS384") == 0 ) return XJWT_ALG_HS384;
	if ( strcmp(name, "HS512") == 0 ) return XJWT_ALG_HS512;
	if ( strcmp(name, "RS256") == 0 ) return XJWT_ALG_RS256;
	if ( strcmp(name, "RS384") == 0 ) return XJWT_ALG_RS384;
	if ( strcmp(name, "RS512") == 0 ) return XJWT_ALG_RS512;
	if ( strcmp(name, "ES256") == 0 ) return XJWT_ALG_ES256;
	return XJWT_ALG_INVALID;
}
#endif


/* ========================================================================== */
/* source: extlibs/xjwt/src/jwt/xjwt_ext.c */
/* ========================================================================== */

#if defined(XJWT_FEATURE_XJWT)
/* RSA 私钥 + EC 公钥/私钥 PEM 解析 + RS/ES 签发验签 + JWKS。 */

/* ------------------------------------------------------------------ */
/* Base64URL 解码（复用 xjwt_core.c 的实现，这里做 JWK 字段用）          */
/* ------------------------------------------------------------------ */
static unsigned char* b64url_dec(const char* s, size_t n, size_t* out)
{
	return xjwt__base64url_decode(s, n, out);
}

/* ------------------------------------------------------------------ */
/* RSA 私钥 PEM 解析（PKCS#8 或 PKCS#1 → xrsaprivatekey）              */
/* ------------------------------------------------------------------ */

void xjwt__rsa_owned_free(xjwt__rsa_owned* p)
{
	if ( p == NULL ) return;
	if ( p->pRaw ) {
		xrtSecureZero(p->pRaw, p->iRawSize);
		xrtFree(p->pRaw);
	}
	xrtFree(p);
}

static bool rsa_private_pkcs1_parse(const void* pData, size_t iSize,
	xbytesview aParts[8])
{
	xdercursor Outer, Fields;
	xdervalue Value;
	uint64 iVersion;

	if ( !xrtDerValidate(pData, iSize) ||
		!xrtDerInit(&Outer, pData, iSize) ||
		xrtDerRead(&Outer, &Value) != XDER_VALUE ||
		!xrtDerIs(&Value, XASN1_UNIVERSAL, XASN1_SEQUENCE, true) ||
		!xrtDerDone(&Outer) || !xrtDerEnter(&Value, &Fields) ||
		xrtDerRead(&Fields, &Value) != XDER_VALUE ||
		!xrtDerUInt64(&Value, &iVersion) || (iVersion != 0) ) {
		return false;
	}
	for ( size_t i = 0; i < 8u; i++ ) {
		if ( xrtDerRead(&Fields, &Value) != XDER_VALUE ||
			!xrtDerUnsigned(&Value, &aParts[i]) ) return false;
	}
	return xrtDerDone(&Fields);
}

/* RFC 5208/5958 [0] IMPLICIT SET OF Attribute: validate but ignore metadata. */
static bool pkcs8_attributes_parse(const xdervalue* pAttributeField)
{
	xdercursor Attributes;
	xdervalue Value;
	xbytesview Previous = { NULL, 0 };

	if ( !xrtDerEnter(pAttributeField, &Attributes) ) {
		return false;
	}
	while ( !xrtDerDone(&Attributes) ) {
		xdercursor Attribute, Values;
		xbytesview Oid, Current;
		int iOrder;
		if ( xrtDerRead(&Attributes, &Value) != XDER_VALUE ||
			!xrtDerIs(&Value, XASN1_UNIVERSAL, XASN1_SEQUENCE, true) )
			return false;
		Current = Value.Raw;
		if ( Previous.Data != NULL ) {
			size_t iCommon = Previous.Size < Current.Size ?
				Previous.Size : Current.Size;
			iOrder = memcmp(Previous.Data, Current.Data, iCommon);
			if ( (iOrder > 0) ||
				((iOrder == 0) && (Previous.Size > Current.Size)) )
				return false;
		}
		Previous = Current;
		if ( !xrtDerEnter(&Value, &Attribute) ||
			xrtDerRead(&Attribute, &Value) != XDER_VALUE ||
			!xrtDerOid(&Value, &Oid) ||
			xrtDerRead(&Attribute, &Value) != XDER_VALUE ||
			!xrtDerIs(&Value, XASN1_UNIVERSAL, XASN1_SET, true) ||
			!xrtDerDone(&Attribute) || !xrtDerEnter(&Value, &Values) ||
			xrtDerDone(&Values) ) return false;
		while ( !xrtDerDone(&Values) ) {
			if ( xrtDerRead(&Values, &Value) != XDER_VALUE ) return false;
		}
	}
	return true;
}

/* OneAsymmetricKey 的可选字段必须按标签排序，版本须与外层公钥一致。 */
static bool pkcs8_options_parse(xdercursor* pFields, uint64 iVersion,
	xbytesview* pPublic)
{
	xdervalue Value;
	bool bAttributes = false;
	bool bPublic = false;

	pPublic->Data = NULL;
	pPublic->Size = 0;
	while ( !xrtDerDone(pFields) ) {
		if ( xrtDerRead(pFields, &Value) != XDER_VALUE ||
			(Value.Tag.Class != XASN1_CONTEXT) ) return false;
		if ( (Value.Tag.Number == 0u) && Value.Tag.Constructed &&
			!bAttributes && !bPublic ) {
			if ( !pkcs8_attributes_parse(&Value) ) return false;
			bAttributes = true;
		} else if ( (Value.Tag.Number == 1u) &&
			!Value.Tag.Constructed && !bPublic &&
			(Value.Value.Size > 1u) && (Value.Value.Data[0] == 0u) ) {
			pPublic->Data = Value.Value.Data + 1u;
			pPublic->Size = Value.Value.Size - 1u;
			bPublic = true;
		} else {
			return false;
		}
	}
	return ((iVersion == 0u) && !bPublic) ||
		((iVersion == 1u) && bPublic);
}

/* 外层 RSA 公钥必须与 RSAPrivateKey 中的模数和指数完全一致。 */
static bool rsa_pkcs8_public_matches(xbytesview Encoded,
	const xbytesview aParts[8])
{
	xdercursor Outer, Fields;
	xdervalue Value;
	xbytesview Modulus, Exponent;

	return xrtDerValidate(Encoded.Data, Encoded.Size) &&
		xrtDerInit(&Outer, Encoded.Data, Encoded.Size) &&
		(xrtDerRead(&Outer, &Value) == XDER_VALUE) &&
		xrtDerIs(&Value, XASN1_UNIVERSAL, XASN1_SEQUENCE, true) &&
		xrtDerDone(&Outer) && xrtDerEnter(&Value, &Fields) &&
		(xrtDerRead(&Fields, &Value) == XDER_VALUE) &&
		xrtDerUnsigned(&Value, &Modulus) &&
		(xrtDerRead(&Fields, &Value) == XDER_VALUE) &&
		xrtDerUnsigned(&Value, &Exponent) && xrtDerDone(&Fields) &&
		(Modulus.Size == aParts[0].Size) &&
		(Exponent.Size == aParts[1].Size) &&
		(memcmp(Modulus.Data, aParts[0].Data, Modulus.Size) == 0) &&
		(memcmp(Exponent.Data, aParts[1].Data, Exponent.Size) == 0);
}

static bool rsa_private_der_parse(const void* pData, size_t iSize,
	bool bPkcs8, xbytesview aParts[8])
{
	static const unsigned char aRsaOid[] = {
		0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01
	};
	xdercursor Outer, Fields, Algorithm;
	xdervalue Value;
	xbytesview EncodedKey, Public;
	uint64 iVersion;

	if ( !bPkcs8 ) return rsa_private_pkcs1_parse(pData, iSize, aParts);
	if ( !xrtDerValidate(pData, iSize) ||
		!xrtDerInit(&Outer, pData, iSize) ||
		xrtDerRead(&Outer, &Value) != XDER_VALUE ||
		!xrtDerIs(&Value, XASN1_UNIVERSAL, XASN1_SEQUENCE, true) ||
		!xrtDerDone(&Outer) || !xrtDerEnter(&Value, &Fields) ||
		xrtDerRead(&Fields, &Value) != XDER_VALUE ||
		!xrtDerUInt64(&Value, &iVersion) || (iVersion > 1u) ||
		xrtDerRead(&Fields, &Value) != XDER_VALUE ||
		!xrtDerIs(&Value, XASN1_UNIVERSAL, XASN1_SEQUENCE, true) ||
		!xrtDerEnter(&Value, &Algorithm) ||
		xrtDerRead(&Algorithm, &Value) != XDER_VALUE ||
		!xrtDerOidEqual(&Value, aRsaOid, sizeof(aRsaOid)) ||
		xrtDerRead(&Algorithm, &Value) != XDER_VALUE ||
		!xrtDerIs(&Value, XASN1_UNIVERSAL, XASN1_NULL, false) ||
		(Value.Value.Size != 0) || !xrtDerDone(&Algorithm) ||
		xrtDerRead(&Fields, &Value) != XDER_VALUE ||
		!xrtDerOctets(&Value, &EncodedKey) ||
		!pkcs8_options_parse(&Fields, iVersion, &Public) ) {
		return false;
	}
	return rsa_private_pkcs1_parse(EncodedKey.Data, EncodedKey.Size, aParts) &&
		((Public.Data == NULL) ||
		 rsa_pkcs8_public_matches(Public, aParts));
}

bool xjwt__rsa_private_parse(const char* sPem, xrsaprivatekey* pKey,
                             xjwt__rsa_owned** ppOwned)
{
	xpemblock Block;
	xbytesview Parts[8];
	xrsaprivatekey Parsed;
	xjwt__rsa_owned* pOwn;
	size_t iPemSize, iDerSize = 0;
	bytes pDer;
	bool bPkcs8;

	if ( (sPem == NULL) || (pKey == NULL) || (ppOwned == NULL) ) {
		xjwt__error(XJWT_ERROR_ARGUMENT, "RSA private key argument is null");
		return false;
	}
	iPemSize = strlen(sPem);
	bPkcs8 = xrtPemFind(sPem, iPemSize, "PRIVATE KEY", &Block);
	if ( !bPkcs8 &&
		!xrtPemFind(sPem, iPemSize, "RSA PRIVATE KEY", &Block) ) {
		xjwt__error(XJWT_ERROR_PARSE, "PEM private key not found");
		return false;
	}
	pDer = xrtPemDecodeNew(&Block, &iDerSize);
	if ( pDer == NULL ) {
		xjwt__error_unless_memory(XJWT_ERROR_PARSE,
			"PEM decode failed");
		return false;
	}
	if ( !rsa_private_der_parse(pDer, iDerSize, bPkcs8, Parts) ) {
		xrtSecureZero(pDer, iDerSize);
		xrtFree(pDer);
		xjwt__error(XJWT_ERROR_PARSE, "RSA private key DER parse failed");
		return false;
	}
	pOwn = (xjwt__rsa_owned*)xrtMalloc(sizeof(*pOwn));
	if ( pOwn == NULL ) {
		xrtSecureZero(pDer, iDerSize);
		xrtFree(pDer);
		return false;
	}
	pOwn->pRaw = pDer;
	pOwn->iRawSize = iDerSize;
	Parsed.Public.Modulus       = Parts[0].Data; Parsed.Public.ModulusSize   = Parts[0].Size;
	Parsed.Public.Exponent      = Parts[1].Data; Parsed.Public.ExponentSize  = Parts[1].Size;
	Parsed.PrivateExponent      = Parts[2].Data; Parsed.PrivateExponentSize = Parts[2].Size;
	Parsed.Prime1               = Parts[3].Data; Parsed.Prime1Size          = Parts[3].Size;
	Parsed.Prime2               = Parts[4].Data; Parsed.Prime2Size          = Parts[4].Size;
	Parsed.Exponent1            = Parts[5].Data; Parsed.Exponent1Size       = Parts[5].Size;
	Parsed.Exponent2            = Parts[6].Data; Parsed.Exponent2Size       = Parts[6].Size;
	Parsed.Coefficient          = Parts[7].Data; Parsed.CoefficientSize     = Parts[7].Size;
	*pKey = Parsed;
	*ppOwned = pOwn;
	return true;
}

/* ------------------------------------------------------------------ */
/* EC P-256 公钥/私钥 PEM 解析                                          */
/* ------------------------------------------------------------------ */

/* EC P-256 PEM keys use id-ecPublicKey and prime256v1 namedCurve. */
static const unsigned char s_ecPublicKeyOid[] = {
	0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01
};
static const unsigned char s_p256Oid[] = {
	0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07
};

bool xjwt__ecdsa_public_parse(const char* sPem, unsigned char* pPublic65)
{
	xpemblock Block;
	xdercursor Outer, Fields, Algorithm;
	xdervalue Value;
	xbytesview Point;
	uint8 iUnused;
	size_t iDerSize = 0;
	bytes pDer;

	if ( (sPem == NULL) || (pPublic65 == NULL) ) {
		xjwt__error(XJWT_ERROR_ARGUMENT, "EC public key argument is null");
		return false;
	}
	if ( !xrtPemFind(sPem, strlen(sPem), "PUBLIC KEY", &Block) ) {
		xjwt__error(XJWT_ERROR_PARSE, "PEM public key not found");
		return false;
	}
	if ( Block.Body.Size > 4096u ) {
		xjwt__error(XJWT_ERROR_PARSE, "EC public key PEM is too large");
		return false;
	}
	pDer = xrtPemDecodeNew(&Block, &iDerSize);
	if ( pDer == NULL ) {
		xjwt__error_unless_memory(XJWT_ERROR_PARSE,
			"PEM decode failed");
		return false;
	}
	if ( !xrtDerValidate(pDer, iDerSize) ||
		!xrtDerInit(&Outer, pDer, iDerSize) ||
		xrtDerRead(&Outer, &Value) != XDER_VALUE ||
		!xrtDerIs(&Value, XASN1_UNIVERSAL, XASN1_SEQUENCE, true) ||
		!xrtDerDone(&Outer) || !xrtDerEnter(&Value, &Fields) ||
		xrtDerRead(&Fields, &Value) != XDER_VALUE ||
		!xrtDerIs(&Value, XASN1_UNIVERSAL, XASN1_SEQUENCE, true) ||
		!xrtDerEnter(&Value, &Algorithm) ||
		xrtDerRead(&Algorithm, &Value) != XDER_VALUE ||
		!xrtDerOidEqual(&Value, s_ecPublicKeyOid, sizeof(s_ecPublicKeyOid)) ||
		xrtDerRead(&Algorithm, &Value) != XDER_VALUE ||
		!xrtDerOidEqual(&Value, s_p256Oid, sizeof(s_p256Oid)) ||
		!xrtDerDone(&Algorithm) ||
		xrtDerRead(&Fields, &Value) != XDER_VALUE ||
		!xrtDerBitString(&Value, &Point, &iUnused) ||
		(iUnused != 0) || (Point.Size != 65u) ||
		(Point.Data[0] != 0x04u) || !xrtDerDone(&Fields) ||
		!xrtP256Valid(Point.Data) ) {
		xrtFree(pDer);
		xjwt__error(XJWT_ERROR_PARSE, "EC public key DER parse failed");
		return false;
	}
	memcpy(pPublic65, Point.Data, 65u);
	xrtFree(pDer);
	return true;
}

/* Parse SEC1 ECPrivateKey, including its optional explicit curve/public key. */
static bool ecdsa_sec1_parse(const void* pData, size_t iSize,
	xbytesview* pPrivate, xbytesview* pPublic)
{
	xdercursor Outer, Fields;
	xdervalue Value;
	uint64 iVersion;
	int iLastOptional = -1;

	pPublic->Data = NULL;
	pPublic->Size = 0;
	if ( !xrtDerValidate(pData, iSize) ||
		!xrtDerInit(&Outer, pData, iSize) ||
		xrtDerRead(&Outer, &Value) != XDER_VALUE ||
		!xrtDerIs(&Value, XASN1_UNIVERSAL, XASN1_SEQUENCE, true) ||
		!xrtDerDone(&Outer) || !xrtDerEnter(&Value, &Fields) ||
		xrtDerRead(&Fields, &Value) != XDER_VALUE ||
		!xrtDerUInt64(&Value, &iVersion) || (iVersion != 1u) ||
		xrtDerRead(&Fields, &Value) != XDER_VALUE ||
		!xrtDerOctets(&Value, pPrivate) || (pPrivate->Size != 32u) ) {
		return false;
	}
	while ( !xrtDerDone(&Fields) ) {
		xdercursor Optional;
		xbytesview Point;
		uint8 iUnused;
		if ( xrtDerRead(&Fields, &Value) != XDER_VALUE ||
			(Value.Tag.Class != XASN1_CONTEXT) ||
			!Value.Tag.Constructed || (Value.Tag.Number > 1u) ||
			((int)Value.Tag.Number <= iLastOptional) ||
			!xrtDerEnter(&Value, &Optional) ) return false;
		iLastOptional = (int)Value.Tag.Number;
		if ( xrtDerRead(&Optional, &Value) != XDER_VALUE ) return false;
		if ( iLastOptional == 0 ) {
			if ( !xrtDerOidEqual(&Value, s_p256Oid, sizeof(s_p256Oid)) )
				return false;
		} else {
			if ( !xrtDerBitString(&Value, &Point, &iUnused) ||
				(iUnused != 0) || (Point.Size != 65u) ||
				(Point.Data[0] != 0x04u) || !xrtP256Valid(Point.Data) )
				return false;
			*pPublic = Point;
		}
		if ( !xrtDerDone(&Optional) ) return false;
	}
	return true;
}

static bool ecdsa_private_der_parse(const void* pData, size_t iSize,
	bool bPkcs8, xbytesview* pPrivate, xbytesview* pPublic,
	xbytesview* pOuterPublic)
{
	xdercursor Outer, Fields, Algorithm;
	xdervalue Value;
	xbytesview EncodedKey;
	uint64 iVersion;

	pOuterPublic->Data = NULL;
	pOuterPublic->Size = 0;
	if ( !bPkcs8 ) return ecdsa_sec1_parse(pData, iSize, pPrivate, pPublic);
	if ( !xrtDerValidate(pData, iSize) ||
		!xrtDerInit(&Outer, pData, iSize) ||
		xrtDerRead(&Outer, &Value) != XDER_VALUE ||
		!xrtDerIs(&Value, XASN1_UNIVERSAL, XASN1_SEQUENCE, true) ||
		!xrtDerDone(&Outer) || !xrtDerEnter(&Value, &Fields) ||
		xrtDerRead(&Fields, &Value) != XDER_VALUE ||
		!xrtDerUInt64(&Value, &iVersion) || (iVersion > 1u) ||
		xrtDerRead(&Fields, &Value) != XDER_VALUE ||
		!xrtDerIs(&Value, XASN1_UNIVERSAL, XASN1_SEQUENCE, true) ||
		!xrtDerEnter(&Value, &Algorithm) ||
		xrtDerRead(&Algorithm, &Value) != XDER_VALUE ||
		!xrtDerOidEqual(&Value, s_ecPublicKeyOid, sizeof(s_ecPublicKeyOid)) ||
		xrtDerRead(&Algorithm, &Value) != XDER_VALUE ||
		!xrtDerOidEqual(&Value, s_p256Oid, sizeof(s_p256Oid)) ||
		!xrtDerDone(&Algorithm) ||
		xrtDerRead(&Fields, &Value) != XDER_VALUE ||
		!xrtDerOctets(&Value, &EncodedKey) ||
		!pkcs8_options_parse(&Fields, iVersion, pOuterPublic) ) {
		return false;
	}
	return ecdsa_sec1_parse(EncodedKey.Data, EncodedKey.Size,
		pPrivate, pPublic) &&
		((pOuterPublic->Data == NULL) ||
		 ((pOuterPublic->Size == 65u) &&
		  (pOuterPublic->Data[0] == 0x04u)));
}

bool xjwt__ecdsa_private_parse(const char* sPem, unsigned char* pPrivate32)
{
	xpemblock Block;
	xbytesview Private, Public, OuterPublic;
	unsigned char ExpectedPublic[65];
	size_t iDerSize = 0;
	bytes pDer;
	bool bPkcs8, bValid;

	if ( (sPem == NULL) || (pPrivate32 == NULL) ) {
		xjwt__error(XJWT_ERROR_ARGUMENT, "EC private key argument is null");
		return false;
	}
	bPkcs8 = xrtPemFind(sPem, strlen(sPem), "PRIVATE KEY", &Block);
	if ( !bPkcs8 &&
		!xrtPemFind(sPem, strlen(sPem), "EC PRIVATE KEY", &Block) ) {
		xjwt__error(XJWT_ERROR_PARSE, "PEM EC private key not found");
		return false;
	}
	if ( Block.Body.Size > 4096u ) {
		xjwt__error(XJWT_ERROR_PARSE, "EC private key PEM is too large");
		return false;
	}
	pDer = xrtPemDecodeNew(&Block, &iDerSize);
	if ( pDer == NULL ) {
		xjwt__error_unless_memory(XJWT_ERROR_PARSE,
			"PEM decode failed");
		return false;
	}
	bValid = ecdsa_private_der_parse(pDer, iDerSize, bPkcs8,
		&Private, &Public, &OuterPublic);
	if ( bValid ) bValid = xrtP256Public(Private.Data, ExpectedPublic);
	if ( bValid && Public.Data != NULL )
		bValid = xrtConstTimeEqual(ExpectedPublic, Public.Data, 65u);
	if ( bValid && OuterPublic.Data != NULL )
		bValid = xrtConstTimeEqual(ExpectedPublic, OuterPublic.Data, 65u);
	if ( bValid ) memcpy(pPrivate32, Private.Data, 32u);
	xrtSecureZero(ExpectedPublic, sizeof(ExpectedPublic));
	xrtSecureZero(pDer, iDerSize);
	xrtFree(pDer);
	if ( !bValid ) {
		xjwt__error(XJWT_ERROR_PARSE, "EC private key DER parse failed");
		return false;
	}
	return true;
}

/* ------------------------------------------------------------------ */
/* RS/ES 签发                                                           */
/* ------------------------------------------------------------------ */

static xcryptohash jwt_alg_hash(int alg)
{
	switch ( alg ) {
	case XJWT_ALG_HS256: case XJWT_ALG_RS256: case XJWT_ALG_ES256:
		return XCRYPTO_HASH_SHA256;
	case XJWT_ALG_HS384: case XJWT_ALG_RS384:
		return XCRYPTO_HASH_SHA384;
	case XJWT_ALG_HS512: case XJWT_ALG_RS512:
		return XCRYPTO_HASH_SHA512;
	}
	return (xcryptohash)0;
}

bool rsa_sign_impl(int alg, const void* pData, size_t iSize,
                   const char* sPrivatePem,
                   unsigned char* pOut, size_t iCapacity, size_t* pOutSize)
{
	xrsaprivatekey tKey;
	xjwt__rsa_owned* pOwn = NULL;
	if ( !xjwt__rsa_private_parse(sPrivatePem, &tKey, &pOwn) ) return false;
	if ( !xjwt__rsa_jwa_key_valid(&tKey.Public) ) {
		xjwt__error(XJWT_ERROR_ARGUMENT,
			"RSA signing key must be at least 2048 bits");
		xjwt__rsa_owned_free(pOwn);
		return false;
	}

	/* RSA 签名长度 = 模数长度；超出输出容量直接失败，不截断 */
	if ( tKey.Public.ModulusSize > iCapacity ) {
		xjwt__error(XJWT_ERROR_ARGUMENT,
			"RSA modulus exceeds signature output capacity");
		xjwt__rsa_owned_free(pOwn);
		return false;
	}

	xcryptohash iHash = jwt_alg_hash(alg);
	unsigned char aDigest[64];

	switch ( iHash ) {
	case XCRYPTO_HASH_SHA256:
		if ( !xrtSha256(pData, iSize, aDigest) ) goto fail;
		break;
	case XCRYPTO_HASH_SHA384:
		if ( !xrtSha384(pData, iSize, aDigest) ) goto fail;
		break;
	case XCRYPTO_HASH_SHA512:
		if ( !xrtSha512(pData, iSize, aDigest) ) goto fail;
		break;
	default:
		goto fail;
	}

	/* RSA 签名长度 = 模数长度 */
	size_t iSigSize = tKey.Public.ModulusSize;
	if ( !xrtRsaPkcs1Sign(&tKey, iHash, aDigest, pOut) ) goto fail;
	*pOutSize = iSigSize;

	xjwt__rsa_owned_free(pOwn);
	return true;

fail:
	xjwt__rsa_owned_free(pOwn);
	return false;
}

bool es256_sign_impl(const void* pData, size_t iSize,
                     const char* sPrivatePem,
                     unsigned char* pOut, size_t iCapacity, size_t* pOutSize)
{
	unsigned char aPrivate[32];
	unsigned char aDigest[32];
	bool bResult = false;
	if ( !xjwt__ecdsa_private_parse(sPrivatePem, aPrivate) ) return false;
	if ( !xrtSha256(pData, iSize, aDigest) ) goto cleanup;

	/* RFC 7518 §3.4：JWS ES256 是定宽 32 字节 R + 32 字节 S。 */
	if ( iCapacity < 64 ) {
		xjwt__error(XJWT_ERROR_ARGUMENT,
			"output buffer too small for ES256 signature");
		goto cleanup;
	}
	if ( !xrtEcdsaP256Sign(XCRYPTO_HASH_SHA256, aDigest, aPrivate, pOut) )
		goto cleanup;
	*pOutSize = 64u;
	bResult = true;

cleanup:
	xrtSecureZero(aPrivate, sizeof(aPrivate));
	xrtSecureZero(aDigest, sizeof(aDigest));
	return bResult;
}

/* ------------------------------------------------------------------ */
/* JWKS                                                                 */
/* ------------------------------------------------------------------ */


/* JWKS 密钥数上限：超出视为异常输入，整体解析失败
 * （静默截断会让密钥轮换期的新 kid 验不过，比报错更难排查） */
#define XJWT_JWKS_MAX_KEYS 16

xjwtjwks* xjwtJwksParse(const char* sJson)
{
	if ( sJson == NULL ) return NULL;
	xerror* pPrevious = xrtTakeError();
	xvalue* p = xrtJsonParse(xrtStrView(sJson));
	if ( p == NULL ) {
		if ( !xjwt__memory_error() )
			xjwt__error(XJWT_ERROR_PARSE, "JWKS JSON parse failed");
		xrtErrorFree(pPrevious);
		return NULL;
	}
	xvalue* pKeys = xrtValueObjectGet(p, xrtStrView("keys"));
	if ( pKeys == NULL || !xrtValueIs(pKeys, XVALUE_ARRAY) ) {
		xrtValueRelease(p);
		xrtErrorFree(pPrevious);
		xjwt__error(XJWT_ERROR_PARSE, "JWKS missing 'keys' array");
		return NULL;
	}
	size_t n = xrtValueCount(pKeys);
	if ( n > XJWT_JWKS_MAX_KEYS ) {
		xrtValueRelease(p);
		xrtErrorFree(pPrevious);
		xjwt__error(XJWT_ERROR_PARSE, "JWKS exceeds 16 keys");
		return NULL;
	}

	xjwtjwks* pJwks = (xjwtjwks*)xrtMalloc(sizeof(xjwtjwks));
	if ( pJwks == NULL ) {
		xrtValueRelease(p);
		xrtErrorFree(pPrevious);
		return NULL;
	}
	memset(pJwks, 0, sizeof(*pJwks));

	for ( size_t i = 0; i < n; i++ ) {
		xvalue* pKey = xrtValueArrayGet(pKeys, (uint32)i);
		if ( pKey == NULL ) continue;

		xstrview sv;
		int idx = pJwks->nKeys;
		memset(&pJwks->keys[idx], 0, sizeof(pJwks->keys[idx]));

		/* kid：超长（≥128）跳过该条目——静默截断会让严格匹配
		 * 永不命中且报错误导排障 */
		xvalue* pKid = xrtValueObjectGet(pKey, xrtStrView("kid"));
		if ( pKid != NULL ) {
			if ( !xrtValueGetString(pKid, &sv) ||
				sv.Size >= sizeof(pJwks->keys[idx].kid) ||
				memchr(sv.Data, 0, sv.Size) != NULL ) continue;
			memcpy(pJwks->keys[idx].kid, sv.Data, sv.Size);
			pJwks->keys[idx].kid[sv.Size] = 0;
			pJwks->keys[idx].HasKid = true;
		}
		/* kty */
		if ( !xrtValueGetString(xrtValueObjectGet(pKey, xrtStrView("kty")), &sv) ||
			sv.Size >= sizeof(pJwks->keys[idx].kty) ||
			memchr(sv.Data, 0, sv.Size) != NULL ) continue;
		memcpy(pJwks->keys[idx].kty, sv.Data, sv.Size);
		pJwks->keys[idx].kty[sv.Size] = 0;

		if ( strcmp(pJwks->keys[idx].kty, "RSA") == 0 ) {
			/* RSA: n + e；任一缺失/畸形则释放并清零槽位后跳过
			 * （残留指针既泄漏、又可能被后续条目沿用造成跨条目混淆） */
			size_t nSize = 0, eSize = 0;
			unsigned char* pn = NULL, *pe = NULL;
			if ( xrtValueGetString(xrtValueObjectGet(pKey, xrtStrView("n")), &sv) ) {
				pn = b64url_dec((const char*)sv.Data, sv.Size, &nSize);
				if ( pn == NULL && xjwt__memory_error() )
					goto memory_failure;
			}
			if ( xrtValueGetString(xrtValueObjectGet(pKey, xrtStrView("e")), &sv) ) {
				pe = b64url_dec((const char*)sv.Data, sv.Size, &eSize);
				if ( pe == NULL && xjwt__memory_error() ) {
					xrtFree(pn);
					goto memory_failure;
				}
			}
			if ( pn != NULL && pe != NULL ) {
				xrsapublickey Candidate;

				xjwt__int_trim(pn, &nSize);
				xjwt__int_trim(pe, &eSize);
				Candidate.Modulus = pn;
				Candidate.ModulusSize = nSize;
				Candidate.Exponent = pe;
				Candidate.ExponentSize = eSize;
				if ( xjwt__rsa_jwa_key_valid(&Candidate) ) {
					pJwks->keys[idx].pOwnedN = pn;
					pJwks->keys[idx].pOwnedE = pe;
					pJwks->keys[idx].rsa = Candidate;
					pJwks->nKeys++;
				} else {
					xrtFree(pn);
					xrtFree(pe);
				}
			} else {
				xrtFree(pn);
				xrtFree(pe);
			}
		} else if ( strcmp(pJwks->keys[idx].kty, "EC") == 0 ) {
			/* EC：仅支持 P-256，其他 crv（P-384/P-521）跳过该条目；
			 * 坐标必须恰为 32 字节，短坐标视为畸形 */
			if ( !xrtValueGetString(xrtValueObjectGet(pKey, xrtStrView("crv")), &sv) ||
				sv.Size != 5u || memcmp(sv.Data, "P-256", 5u) != 0 ) continue;

			size_t xSize = 0, ySize = 0;
			unsigned char* px = NULL, *py = NULL;
			if ( xrtValueGetString(xrtValueObjectGet(pKey, xrtStrView("x")), &sv) ) {
				px = b64url_dec((const char*)sv.Data, sv.Size, &xSize);
				if ( px == NULL && xjwt__memory_error() )
					goto memory_failure;
			}
			if ( xrtValueGetString(xrtValueObjectGet(pKey, xrtStrView("y")), &sv) ) {
				py = b64url_dec((const char*)sv.Data, sv.Size, &ySize);
				if ( py == NULL && xjwt__memory_error() ) {
					xrtFree(px);
					goto memory_failure;
				}
			}
			if ( px != NULL && py != NULL && xSize == 32 && ySize == 32 ) {
				pJwks->keys[idx].ec65[0] = 0x04;
				memcpy(pJwks->keys[idx].ec65 + 1, px, 32);
				memcpy(pJwks->keys[idx].ec65 + 33, py, 32);
				if ( xrtP256Valid(pJwks->keys[idx].ec65) )
					pJwks->nKeys++;
			}
			xrtFree(px); xrtFree(py);
		}
	}

	xrtValueRelease(p);
	xrtErrorFree(xrtTakeError());
	if ( pPrevious != NULL ) {
		xrtSetError(pPrevious);
		xrtErrorFree(pPrevious);
	}
	return pJwks;

memory_failure:
	{
		xerror* pFailure = xrtTakeError();
		xjwtJwksFree(pJwks);
		xrtValueRelease(p);
		xrtErrorFree(pPrevious);
		if ( pFailure != NULL ) {
			xrtSetError(pFailure);
			xrtErrorFree(pFailure);
		}
		return NULL;
	}
}

void xjwtJwksFree(xjwtjwks* pJwks)
{
	if ( pJwks == NULL ) return;
	for ( int i = 0; i < pJwks->nKeys; i++ ) {
		if ( pJwks->keys[i].pOwnedN ) xrtFree(pJwks->keys[i].pOwnedN);
		if ( pJwks->keys[i].pOwnedE ) xrtFree(pJwks->keys[i].pOwnedE);
	}
	xrtFree(pJwks);
}

/* 算法族判定：RS 族 true / ES 族 false，HS 或未知返回 -1 */
static int jwt_alg_rsa_family(int alg)
{
	switch ( alg ) {
	case XJWT_ALG_RS256: case XJWT_ALG_RS384: case XJWT_ALG_RS512:
		return 1;
	case XJWT_ALG_ES256:
		return 0;
	}
	return -1;
}

xvalue* xjwtVerifyJwks(const char* sToken, const xjwtjwks* pJwks,
                       const xjwtcheck* pCheck)
{
	if ( sToken == NULL || pJwks == NULL ) {
		xjwt__error(XJWT_ERROR_ARGUMENT, "xjwtVerifyJwks: null argument");
		return NULL;
	}

	/* 公共前置：header(alg+kid) → claims 校验 → 签名输入/签名解码 */
	int alg = XJWT_ALG_INVALID;
	const char* sKid = NULL;
	char* sInput = NULL;
	unsigned char* pSigBin = NULL;
	size_t n = 0, iSigBinSize = 0;
	xvalue* pClaims = xjwt__verify_prepare(sToken, pCheck, &alg, &sKid,
	                                       &sInput, &n, &pSigBin, &iSigBinSize);
	if ( pClaims == NULL ) return NULL;

	/* JWKS 只承载非对称公钥：显式拒绝 HS 族，不依赖签名格式不匹配兜底 */
	int iFamily = jwt_alg_rsa_family(alg);
	if ( iFamily < 0 ) {
		xrtFree((void*)sKid); xrtFree(sInput); xrtFree(pSigBin);
		xrtValueRelease(pClaims);
		xjwt__error(XJWT_ERROR_ALG_MISMATCH,
			"JWKS verification only accepts RS*/ES* algorithms");
		return NULL;
	}

	/* kid 严格匹配：token 带 kid 必须精确命中；
	 * token 无 kid 才回退取第一把可用密钥 */
	int iFound = -1;
	for ( int i = 0; i < pJwks->nKeys; i++ ) {
		if ( sKid == NULL ) { iFound = i; break; }
		if ( pJwks->keys[i].HasKid &&
			strcmp(sKid, pJwks->keys[i].kid) == 0 ) { iFound = i; break; }
	}
	xrtFree((void*)sKid);
	if ( iFound < 0 ) {
		xrtFree(sInput); xrtFree(pSigBin); xrtValueRelease(pClaims);
		xjwt__error(XJWT_ERROR_KEY_NOT_FOUND, "no matching kid in JWKS");
		return NULL;
	}

	/* 算法族必须与密钥类型匹配：RS* ↔ RSA、ES256 ↔ EC */
	bool bKeyRsa = strcmp(pJwks->keys[iFound].kty, "RSA") == 0;
	if ( ( iFamily == 1 ) != bKeyRsa ) {
		xrtFree(sInput); xrtFree(pSigBin); xrtValueRelease(pClaims);
		xjwt__error(XJWT_ERROR_ALG_MISMATCH,
			"token alg family does not match JWKS key type");
		return NULL;
	}

	bool ok;
	if ( bKeyRsa ) {
		ok = xjwt__verify_rsa_raw(sInput, n, pSigBin, iSigBinSize,
		                          &pJwks->keys[iFound].rsa, alg);
	} else {
		ok = xjwt__verify_es256_raw(sInput, n, pSigBin, iSigBinSize,
		                            pJwks->keys[iFound].ec65);
	}

	xrtFree(sInput); xrtFree(pSigBin);
	if ( !ok ) {
		xjwt__error_unless_memory(XJWT_ERROR_SIGNATURE,
			"JWKS signature mismatch");
		xrtValueRelease(pClaims);
		return NULL;
	}
	return pClaims;
}

/* ------------------------------------------------------------------ */
/* 公钥缓存（xjwtKeyParse / xjwtKeyFree / xjwtVerifyKey）                */
/* ------------------------------------------------------------------ */

struct xjwtkey {
	bool          bRsa;        /* true=RSA 公钥；false=EC P-256（65 字节点） */
	xrsapublickey Rsa;         /* bRsa 时有效（视图，指向 pOwnedArr 内部） */
	unsigned char* pOwnedArr;  /* xjwt__rsa_public_parse 的资源包 */
	unsigned char Ec65[65];    /* !bRsa 时有效 */
};

xjwtkey* xjwtKeyParse(const char* sPublicPem)
{
	if ( sPublicPem == NULL ) {
		xjwt__error(XJWT_ERROR_ARGUMENT, "xjwtKeyParse: null argument");
		return NULL;
	}
	xerror* pPrevious = xrtTakeError();
	xjwtkey* pKey = (xjwtkey*)xrtMalloc(sizeof(xjwtkey));
	if ( pKey == NULL ) {
		xrtErrorFree(pPrevious);
		return NULL;
	}
	memset(pKey, 0, sizeof(*pKey));

	/* 先按 RSA（SPKI / PKCS#1）解析，失败再按 EC P-256 SPKI */
	unsigned char* pOwned = NULL;
	xrsapublickey tRsa;
	if ( xjwt__rsa_public_parse(sPublicPem, &tRsa, &pOwned) ) {
		pKey->bRsa = true;
		pKey->Rsa = tRsa;
		pKey->pOwnedArr = pOwned;
		goto success;
	}
	/* 内存错误不代表算法不匹配，不能换解析器后吞掉根因。 */
	if ( xjwt__memory_error() ) goto failure;
	if ( xjwt__ecdsa_public_parse(sPublicPem, pKey->Ec65) ) {
		pKey->bRsa = false;
		goto success;
	}
failure:
	xrtFree(pKey);
	xrtErrorFree(pPrevious);
	xjwt__error_unless_memory(XJWT_ERROR_PARSE, "not an RSA/EC public key PEM");
	return NULL;

success:
	/* 丢弃内部算法探测的诊断，恢复调用前已有的错误。 */
	xrtErrorFree(xrtTakeError());
	if ( pPrevious != NULL ) {
		xrtSetError(pPrevious);
		xrtErrorFree(pPrevious);
	}
	return pKey;
}

void xjwtKeyFree(xjwtkey* pKey)
{
	if ( pKey == NULL ) return;
	if ( pKey->pOwnedArr != NULL )
		xjwt__rsa_public_free(&pKey->Rsa, pKey->pOwnedArr);
	xrtFree(pKey);
}

xvalue* xjwtVerifyKey(const char* sToken, const xjwtkey* pKey,
                      const xjwtcheck* pCheck)
{
	if ( sToken == NULL || pKey == NULL ) {
		xjwt__error(XJWT_ERROR_ARGUMENT, "xjwtVerifyKey: null argument");
		return NULL;
	}

	int alg = XJWT_ALG_INVALID;
	const char* sKid = NULL;
	char* sInput = NULL;
	unsigned char* pSigBin = NULL;
	size_t n = 0, iSigBinSize = 0;
	xvalue* pClaims = xjwt__verify_prepare(sToken, pCheck, &alg, &sKid,
	                                       &sInput, &n, &pSigBin, &iSigBinSize);
	if ( pClaims == NULL ) return NULL;
	xrtFree((void*)sKid);  /* 单钥路径不选钥，kid 不参与 */

	/* 缓存只承载公钥：拒绝 HS 族；算法族必须与密钥类型匹配 */
	int iFamily = jwt_alg_rsa_family(alg);
	if ( iFamily < 0 || ( iFamily == 1 ) != pKey->bRsa ) {
		xrtFree(sInput); xrtFree(pSigBin); xrtValueRelease(pClaims);
		xjwt__error(XJWT_ERROR_ALG_MISMATCH,
			"token alg family does not match cached key type");
		return NULL;
	}

	bool ok;
	if ( pKey->bRsa )
		ok = xjwt__verify_rsa_raw(sInput, n, pSigBin, iSigBinSize, &pKey->Rsa, alg);
	else
		ok = xjwt__verify_es256_raw(sInput, n, pSigBin, iSigBinSize, pKey->Ec65);

	xrtFree(sInput); xrtFree(pSigBin);
	if ( !ok ) {
		xjwt__error_unless_memory(XJWT_ERROR_SIGNATURE,
			"cached key signature mismatch");
		xrtValueRelease(pClaims);
		return NULL;
	}
	return pClaims;
}

/* ES256 验签（PEM 路径入口） */
bool xjwt__verify_es256_full(const void* pData, size_t iSize,
                             const char* sPublicPem,
                             const void* pSig, size_t iSigSize)
{
	unsigned char aPublic[65];
	if ( !xjwt__ecdsa_public_parse(sPublicPem, aPublic) ) return false;
	return xjwt__verify_es256_raw(pData, iSize, pSig, iSigSize, aPublic);
}
#endif

#endif
