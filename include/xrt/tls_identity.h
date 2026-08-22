#ifndef XRT_TLS_IDENTITY_H
#define XRT_TLS_IDENTITY_H

#include <xrt/tls.h>
#include <xrt/x509.h>



#if defined(XRT_FEATURE_TLS_IDENTITY) && \
	(!defined(XRT_FEATURE_TLS_NEGOTIATE) || \
	 !defined(XRT_FEATURE_X509_PARSE) || \
	 !defined(XRT_FEATURE_CRYPTO_CORE))
	#error "XRT_FEATURE_TLS_IDENTITY requires TLS negotiation, X.509 parsing and crypto core"
#endif

#if defined(XRT_FEATURE_TLS_IDENTITY_RSA) && \
	(!defined(XRT_FEATURE_TLS_IDENTITY) || \
	 !defined(XRT_FEATURE_X509_SIGNATURE) || \
	 !defined(XRT_FEATURE_CRYPTO_RSA_PRIVATE) || \
	 !defined(XRT_FEATURE_CRYPTO_RSA_PSS_SIGN) || \
	 !defined(XRT_FEATURE_CRYPTO_RSA_PKCS1_SIGN) || \
	 !defined(XRT_FEATURE_CRYPTO_SHA256) || \
	 !defined(XRT_FEATURE_CRYPTO_SHA512))
	#error "XRT_FEATURE_TLS_IDENTITY_RSA requires TLS identity, X.509 signature parsing and RSA signing backends"
#endif

#if defined(XRT_FEATURE_TLS_IDENTITY_EC) && \
	(!defined(XRT_FEATURE_TLS_IDENTITY) || \
	 !defined(XRT_FEATURE_ASN1_DER))
	#error "XRT_FEATURE_TLS_IDENTITY_EC requires TLS identity and strict DER"
#endif

#if defined(XRT_FEATURE_TLS_IDENTITY_P256) && \
	(!defined(XRT_FEATURE_TLS_IDENTITY_EC) || \
	 !defined(XRT_FEATURE_CRYPTO_P256) || \
	 !defined(XRT_FEATURE_CRYPTO_ECDSA_P256_SIGN_DER) || \
	 !defined(XRT_FEATURE_CRYPTO_HMAC_SHA256) || \
	 !defined(XRT_FEATURE_CRYPTO_HMAC_SHA512) || \
	 !defined(XRT_FEATURE_CRYPTO_SHA256) || \
	 !defined(XRT_FEATURE_CRYPTO_SHA512))
	#error "XRT_FEATURE_TLS_IDENTITY_P256 requires EC identity and complete SHA-2 ECDSA signing"
#endif

#if defined(XRT_FEATURE_TLS_IDENTITY_P384) && \
	(!defined(XRT_FEATURE_TLS_IDENTITY_EC) || \
	 !defined(XRT_FEATURE_CRYPTO_P384) || \
	 !defined(XRT_FEATURE_CRYPTO_ECDSA_P384_SIGN_DER) || \
	 !defined(XRT_FEATURE_CRYPTO_HMAC_SHA256) || \
	 !defined(XRT_FEATURE_CRYPTO_HMAC_SHA512) || \
	 !defined(XRT_FEATURE_CRYPTO_SHA256) || \
	 !defined(XRT_FEATURE_CRYPTO_SHA512))
	#error "XRT_FEATURE_TLS_IDENTITY_P384 requires EC identity and complete SHA-2 ECDSA signing"
#endif

#if defined(XRT_FEATURE_TLS_IDENTITY_ED25519) && \
	(!defined(XRT_FEATURE_TLS_IDENTITY) || \
	 !defined(XRT_FEATURE_CRYPTO_ED25519_SIGN))
	#error "XRT_FEATURE_TLS_IDENTITY_ED25519 requires TLS identity and Ed25519 signing"
#endif



#if defined(XRT_FEATURE_TLS_IDENTITY)

typedef struct xtlsidentity xtlsidentity;



/* 外部签名器可以进一步限制同一密钥类型实际支持的协议签名方案。 */
typedef bool (*xtlsidentitysupportsproc)(
	ptr pContext,
	xtlsversion Version,
	xtlssignature Signature
);



/*
	签名器接收完整 TLS 待签内容，内部负责协议方案要求的摘要与编码。
	pOutput 为空且容量为零时只查询所需长度；失败时不得修改输出和长度。
*/
typedef bool (*xtlsidentitysignproc)(
	ptr pContext,
	xtlsversion Version,
	xtlssignature Signature,
	xbytesview Message,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 身份释放过程只管理外部签名器上下文，不管理已经深复制的证书链。 */
typedef void (*xtlsidentityreleaseproc)(ptr pContext);



/*
	自定义身份配置借用输入证书和回调，仅在创建成功后接管 Context。
	创建后的身份是不可变快照，外部签名器必须允许并发调用。
*/
typedef struct xtlsidentityconfig {
	const xbytesview* Certificates;
	size_t CertificateCount;
	xtlsidentitytype Type;
	xtlsidentitysupportsproc Supports;
	xtlsidentitysignproc Sign;
	xtlsidentityreleaseproc Release;
	ptr Context;
} xtlsidentityconfig;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_TLS_IDENTITY)

/* 创建深复制证书链、接管外部签名上下文的自定义共享身份。 */
XRT_API xtlsidentity* xrtTlsIdentityCreate(
	const xtlsidentityconfig* pConfig
);



/* 增加共享身份引用；身份本体和证书视图均保持只读。 */
XRT_API xtlsidentity* xrtTlsIdentityRetain(
	const xtlsidentity* pIdentity
);



/* 释放身份，并在最后一个引用结束时释放签名器和清除内部存储。 */
XRT_API void xrtTlsIdentityRelease(xtlsidentity* pIdentity);



/* 返回叶证书对应的握手签名密钥类型。 */
XRT_API xtlsidentitytype xrtTlsIdentityType(
	const xtlsidentity* pIdentity
);



/* 返回按 TLS 发送顺序保存的证书数量，第一张证书是叶证书。 */
XRT_API size_t xrtTlsIdentityCertificateCount(
	const xtlsidentity* pIdentity
);



/* 借用指定位置的完整 DER 证书；视图随身份最后一个引用失效。 */
XRT_API bool xrtTlsIdentityCertificate(
	const xtlsidentity* pIdentity,
	size_t iIndex,
	xbytesview* pCertificate
);



/* 借用已经严格解析并与身份类型匹配的叶证书公钥。 */
XRT_API bool xrtTlsIdentityPublicKey(
	const xtlsidentity* pIdentity,
	xx509pubkey* pPublicKey
);



/* 判断身份后端是否支持指定 TLS 版本和签名方案。 */
XRT_API bool xrtTlsIdentityCanSign(
	const xtlsidentity* pIdentity,
	xtlsversion Version,
	xtlssignature Signature
);



/*
	签署完整 TLS 待签内容；空输出查询精确长度，容量不足时不调用签名器。
	外部签名器失败会保留为 TLS 身份错误的原因链。
*/
XRT_API bool xrtTlsIdentitySign(
	const xtlsidentity* pIdentity,
	xtlsversion Version,
	xtlssignature Signature,
	xbytesview Message,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);

#endif



#if defined(XRT_FEATURE_TLS_IDENTITY_RSA)

/*
	从 PKCS#1 或未加密 PKCS#8 DER 私钥创建 RSA 身份。
	构造过程核对叶证书公钥、完整指数和 CRT 五参数，不借用私钥输入。
*/
XRT_API xtlsidentity* xrtTlsIdentityRsa(
	const xbytesview* pCertificates,
	size_t iCertificateCount,
	xbytesview PrivateKey
);

#endif



#if defined(XRT_FEATURE_TLS_IDENTITY_P256)

/* 从原始标量、SEC1 或未加密 PKCS#8 私钥创建 P-256 身份。 */
XRT_API xtlsidentity* xrtTlsIdentityP256(
	const xbytesview* pCertificates,
	size_t iCertificateCount,
	xbytesview PrivateKey
);

#endif



#if defined(XRT_FEATURE_TLS_IDENTITY_P384)

/* 从原始标量、SEC1 或未加密 PKCS#8 私钥创建 P-384 身份。 */
XRT_API xtlsidentity* xrtTlsIdentityP384(
	const xbytesview* pCertificates,
	size_t iCertificateCount,
	xbytesview PrivateKey
);

#endif



#if defined(XRT_FEATURE_TLS_IDENTITY_ED25519)

/* 从 32 字节种子、DER OCTET 或未加密 PKCS#8 私钥创建 Ed25519 身份。 */
XRT_API xtlsidentity* xrtTlsIdentityEd25519(
	const xbytesview* pCertificates,
	size_t iCertificateCount,
	xbytesview PrivateKey
);

#endif



XRT_EXTERN_C_END

#endif
