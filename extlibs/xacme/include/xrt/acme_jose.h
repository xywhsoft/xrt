#ifndef XRT_ACME_JOSE_H
#define XRT_ACME_JOSE_H

/*
	xacme JOSE 层的裁剪闭包契约：ES256 账户密钥与 JWS 组装建立在
	xrt 的 P-256 与 SHA-256 原语上。JOSE 对象为内部实现细节，
	宿主经 xrt/acme_client.h 使用；本头只固化依赖闭包，
	供构建器做负向裁剪门禁。
*/

#include <xrt/core.h>

#if defined(XACME_FEATURE_ACME_JOSE)

#if !defined(XRT_FEATURE_BUFFER) || \
	!defined(XRT_FEATURE_CODEC_BASE64) || \
	!defined(XRT_FEATURE_CRYPTO_SHA256) || \
	!defined(XRT_FEATURE_CRYPTO_P256_KEYPAIR) || \
	!defined(XRT_FEATURE_CRYPTO_ECDSA_P256_SIGN)
	#error "XACME_FEATURE_ACME_JOSE requires base64, SHA-256 and P-256 signing"
#endif

#endif

#endif
