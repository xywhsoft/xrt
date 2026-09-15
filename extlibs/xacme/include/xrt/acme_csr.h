#ifndef XRT_ACME_CSR_H
#define XRT_ACME_CSR_H

/*
	xacme CSR/密钥序列化层的裁剪闭包契约：PKCS#10 组装与 PKCS#8/SEC1
	PEM 读写建立在 xrt 的 ASN.1/PEM/P-256 原语上，并复用 JOSE 密钥
	类型。CSR 对象为内部实现细节，宿主经 xrt/acme_client.h 使用；
	本头只固化依赖闭包，供构建器做负向裁剪门禁。
*/

#include <xrt/core.h>

#if defined(XACME_FEATURE_ACME_CSR)

#if !defined(XACME_FEATURE_ACME_JOSE) || \
	!defined(XRT_FEATURE_BUFFER) || \
	!defined(XRT_FEATURE_ASN1_DER) || \
	!defined(XRT_FEATURE_PEM) || \
	!defined(XRT_FEATURE_CODEC_BASE64) || \
	!defined(XRT_FEATURE_CRYPTO_P256) || \
	!defined(XRT_FEATURE_CRYPTO_ECDSA_P256_SIGN_DER) || \
	!defined(XRT_FEATURE_CRYPTO_RSA) || \
	!defined(XRT_FEATURE_CRYPTO_RSA_PRIVATE) || \
	!defined(XRT_FEATURE_CRYPTO_RSA_PKCS1) || \
	!defined(XRT_FEATURE_CRYPTO_RSA_PKCS1_SIGN)
	#error "XACME_FEATURE_ACME_CSR requires JOSE, DER, PEM and P-256 DER signing"
#endif

#endif

#endif
