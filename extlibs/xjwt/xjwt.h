/*
	xjwt —— JSON Web Token (RFC 7519) / JWS (RFC 7515) 签发与验证。

	构建在 xrt 核心密码原语（HMAC/RSA PKCS1/ECDSA P-256）之上。
	支持 HS256/384/512、RS256/384/512、ES256 三族算法；
	JWKS (RFC 7517) 公钥集解析与按 kid 自动选钥。

	用法示例（一步式）：
		char* token = xjwtHs256(claims, secret, 3600);
		xvalue* out  = xjwtVerify(token, secret, &check);

	xrt 模块闭包见 xjwt-xrt.h；实现是 xjwt.c 单一编译单元
	（把 xjwt.c 加入构建，或单 TU 场景直接 #include "xjwt.c"）。
*/
#ifndef XJWT_H
#define XJWT_H

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

void xjwtCheckInit(xjwtcheck* pCheck);

/* ------------------------------------------------------------------
 * 签发（一步式）
 * claims 是 xrt 的 xvalue JSON 对象；返回 token 字符串（xrtFree 释放）。
 * expireSeconds != 0 自动注入 exp；自动注入 iat。调用方保留 claims 所有权，
 * 签发成功或失败都不修改传入对象；自动字段仅存在于签发的 token 中。
 * 注意：本族不注入 iss/aud —— 验证侧 xjwtcheck 带 Issuer/Audience
 * 校验时请改用 xjwtSign（config 式可注入全部标准 claims）。
 * ------------------------------------------------------------------ */
char* xjwtHs256(const xvalue* claims, const char* secret, int expireSeconds);
char* xjwtHs384(const xvalue* claims, const char* secret, int expireSeconds);
char* xjwtHs512(const xvalue* claims, const char* secret, int expireSeconds);
char* xjwtRs256(const xvalue* claims, const char* privatePem, int expireSeconds);
char* xjwtRs384(const xvalue* claims, const char* privatePem, int expireSeconds);
char* xjwtRs512(const xvalue* claims, const char* privatePem, int expireSeconds);
char* xjwtEs256(const xvalue* claims, const char* privatePem, int expireSeconds);

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

void  xjwtConfigInit(xjwtconfig* pConfig);
/* config 指定的字段覆盖 token 中同名字段，不改变调用方的 claims。 */
char* xjwtSign(const xjwtconfig* pConfig, const xvalue* claims);

/* ------------------------------------------------------------------
 * 验证（一步式：验签 + exp/nbf/iss/aud 全检）
 * 通过返回 claims（xrtValueRelease 释放），失败返回 NULL 并设 xerror。
 * keyPem：HS 系列收密钥字符串，RS 系列收公钥 PEM，ES 收公钥 PEM。
 * check 传 NULL 等价于 xjwtCheckInit 后只查 exp。
 * ------------------------------------------------------------------ */
xvalue* xjwtVerify(const char* token, const char* keyPem, const xjwtcheck* check);

/* ------------------------------------------------------------------
 * 验证（公钥缓存式）：同一公钥反复验证时避免每次解析 PEM。
 * xjwtKeyParse 解析一次（RSA SPKI/PKCS#1 或 EC P-256 SPKI 公钥 PEM），
 * xjwtVerifyKey 用缓存的密钥验证，线程安全（缓存只读）。
 * ------------------------------------------------------------------ */
typedef struct xjwtkey xjwtkey;

xjwtkey* xjwtKeyParse(const char* publicPem);   /* 失败返回 NULL 并设 xerror；OOM 不回退算法 */
void     xjwtKeyFree(xjwtkey* key);
xvalue*  xjwtVerifyKey(const char* token, const xjwtkey* key, const xjwtcheck* check);

/* ------------------------------------------------------------------
 * 解码（不验签，调试/信任源）
 * xjwtDecode 返回 claims，xjwtDecodeHeader 返回 header，均由 xrtValueRelease 释放。
 * 对应 JSON 必须是对象；xjwtDecode 传 pAlg 时也要求 header 成功解码。
 * *pAlg 收 header 算法（未知为 INVALID），失败时置 INVALID。
 * *pKid 为堆拷贝，由调用方 xrtFree；缺失或失败为 NULL，显式空 kid 为 ""。
 * 传 pKid 时拒绝非字符串、≥256 字节或含 NUL 的 kid，不做前缀匹配。
 * ------------------------------------------------------------------ */
xvalue* xjwtDecode(const char* token, int* pAlg);
xvalue* xjwtDecodeHeader(const char* token, int* pAlg, const char** pKid);

/* ------------------------------------------------------------------
 * claims 独立校验（手工流程用）
 * ------------------------------------------------------------------ */
bool xjwtClaimsValid(const xvalue* claims, const xjwtcheck* check);

/* 取字符串 claim 到调用方缓冲（超长安全截断到 cap-1 并补零）。
 * claim 不存在或不是字符串返回 false。 */
bool xjwtClaimString(const xvalue* claims, const char* key,
                     char* pOut, size_t iCap);

/* ------------------------------------------------------------------
 * 错误便捷读取：返回当前执行上下文中 xjwt 域错误码
 * （XJWT_ERROR_*，见上），当前错误不属于 xjwt 时返回 0。
 * 语义：仅在 xjwt 调用失败后立即读取；成功调用不保证清除旧错误。
 * ------------------------------------------------------------------ */
int xjwtLastError(void);

/* ------------------------------------------------------------------
 * JWKS (RFC 7517)：最多 16 把密钥（超出整体解析失败）。
 * 不支持或畸形的单项密钥会跳过；分配失败使整组解析失败，不返回部分密钥集。
 * kid 严格匹配：token 带 kid 时须为字符串并精确命中带同一 kid 的条目；
 * 空字符串与缺失不同，含 NUL 不支持；JWK kid 须短于 128 字节。
 * token 无 kid 时取第一把可用密钥。
 * ------------------------------------------------------------------ */
typedef struct xjwtjwks xjwtjwks;

xjwtjwks* xjwtJwksParse(const char* json);
void      xjwtJwksFree(xjwtjwks* pJwks);
xvalue*   xjwtVerifyJwks(const char* token, const xjwtjwks* jwks, const xjwtcheck* check);

/* ------------------------------------------------------------------
 * 工具
 * ------------------------------------------------------------------ */
const char* xjwtAlgName(int alg);   /* "HS256" 等，未知返回 NULL */
int         xjwtAlgParse(const char* name);  /* "HS256" → XJWT_ALG_HS256 */

#ifdef __cplusplus
}
#endif

#endif
