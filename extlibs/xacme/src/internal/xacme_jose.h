#ifndef XACME_JOSE_H
#define XACME_JOSE_H

#include <xrt/core.h>
#include <xrt/crypto.h>

#if defined(XACME_FEATURE_ACME_JOSE)

/* jose 模块稳定错误码（错误域 "xrt.acme.jose"）。 */
typedef enum xacmejoseerror {
	XACME_JOSE_ERROR_ARGUMENT = 1,
	XACME_JOSE_ERROR_INTERNAL
} xacmejoseerror;

/* ES256 账户/签名密钥；Private 为 32 字节标量，Public 为 65 字节未压缩点。 */
typedef struct xacmees256key {
	uint8 Private[XRT_P256_PRIVATE_SIZE];
	uint8 Public[XRT_P256_PUBLIC_SIZE];
} xacmees256key;

/* JWS 保护头成员：Nonce/Url/Kid 均为借用视图；Kid 为空时嵌入完整 JWK。 */
typedef struct xacmejwsheader {
	xstrview Nonce;
	xstrview Url;
	xstrview Kid;
} xacmejwsheader;

/* base64url 无填充的 32 字节摘要文本长度（含末尾零共 44）。 */
#define XACME_JWK_THUMBPRINT_TEXT_SIZE 44

#endif

XRT_EXTERN_C_BEGIN

#if defined(XACME_FEATURE_ACME_JOSE)

/* 用操作系统安全随机源生成 P-256 密钥对。 */
bool xacmeEs256Generate(xacmees256key* pKey);

/* 从已有 32 字节私钥派生公钥；失败时保持 Private 之外的输出不变。 */
bool xacmeEs256FromPrivate(xacmees256key* pKey);

/* 规范化 EC JWK JSON（成员字典序：crv,kty,x,y）；返回 xrtFree 释放的文本。 */
str xacmeJwkEcJson(const xacmees256key* pKey);

/* RFC 7638 指纹：SHA-256(规范化 JWK JSON) 的 base64url 无填充文本。 */
bool xacmeJwkThumbprint(xstrview sCanonicalJson, char* sOut);

/* 等价于对 xacmeJwkEcJson 的输出取指纹。 */
bool xacmeJwkEcThumbprint(const xacmees256key* pKey, char* sOut);

/*
	ES256 JWS 紧凑序列化：base64url(header).base64url(payload).base64url(sig)。
	签名为 JOSE raw r||s（64 字节，非 DER）。payload 是调用方已备好的 JSON 字节。
	Url 必填（ACME 要求）；失败返回 NULL 并设置线程错误。
*/
str xacmeJwsEs256(
	const xacmees256key* pKey,
	const xacmejwsheader* pHeader,
	xstrview sPayload
);

/*
	RFC 8555 §7.3.4 External Account Binding 的内层 JWS：
	保护头 {"alg":"HS256","kid":<sKid>,"url":<sUrl>}，payload 为账户
	JWK JSON（借用视图），签名为 HMAC-SHA256(MAC) 裸 32 字节。
	输出扁平 JSON 序列化（xrtFree 释放），直接嵌入 newAccount 载荷。
*/
str xacmeJwsEabHs256(
	cstr sKid,
	cstr sUrl,
	xstrview sJwkJson,
	const uint8* pMac,
	size_t iMacSize
);

#endif

XRT_EXTERN_C_END

#endif
