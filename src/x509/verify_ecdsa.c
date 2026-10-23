#include "../internal/xrt_x509.h"
#include "../internal/xrt_crypto_ecdsa.h"
#include "../internal/xrt_crypto_nist.h"



#if defined(XRT_FEATURE_X509_VERIFY_ECDSA)

/* 不分配诊断对象的 ECDSA 协议验证拒绝。 */
static xrt_x509_decision __xrtX509VerifyEcdsaReject(
	cstr sMessage
)
{
	return __xrtX509Decision(X509_CHECK_REJECTED, X509_ERROR_SIGNATURE, sMessage);
}



/* 使用公钥命名曲线选择严格 DER ECDSA 验签后端。 */
xrt_x509_decision __xrtX509VerifyEcdsa(
	const xx509signature* pScheme,
	xbytesview Content,
	xbytesview Signature,
	const xx509pubkey* pPublicKey
)
{
	uint8 Digest[XRT_X509_DIGEST_MAX_SIZE];
	size_t iDigestSize;
	xrt_crypto_check Result;
	xrt_x509_decision Decision;

	if ( (pPublicKey->Type != X509_KEY_EC) ||
		(pPublicKey->Key.Data == NULL) || (pPublicKey->Key.Size == 0) ) {
		return __xrtX509VerifyEcdsaReject(
			"ECDSA signature and public key types are incompatible");
	}
	if ( !__xrtX509Digest(
		pScheme->Hash, Content, Digest, &iDigestSize
	) ) {
		xrtSecureZero(Digest, sizeof(Digest));
		return __xrtX509CheckError();
	}
	if ( pPublicKey->Curve == X509_CURVE_P256 ) {
		if ( pPublicKey->Key.Size != XRT_P256_PUBLIC_SIZE ) {
			xrtSecureZero(Digest, sizeof(Digest));
			return __xrtX509VerifyEcdsaReject(
				"the P-256 public key point has the wrong size");
		}
		Result = __xrtEcdsaCheckDer(
			XRT_NIST_P256, Digest, iDigestSize, Signature.Data,
			Signature.Size, pPublicKey->Key.Data, 32u, 65u
		);
	} else if ( pPublicKey->Curve == X509_CURVE_P384 ) {
		if ( pPublicKey->Key.Size != XRT_P384_PUBLIC_SIZE ) {
			xrtSecureZero(Digest, sizeof(Digest));
			return __xrtX509VerifyEcdsaReject(
				"the P-384 public key point has the wrong size");
		}
		Result = __xrtEcdsaCheckDer(
			XRT_NIST_P384, Digest, iDigestSize, Signature.Data,
			Signature.Size, pPublicKey->Key.Data, 48u, 97u
		);
	} else {
		xrtSecureZero(Digest, sizeof(Digest));
		return __xrtX509Decision(X509_CHECK_UNSUPPORTED, X509_ERROR_SIGNATURE,
			"the EC public key curve has no enabled verification backend");
	}
	xrtSecureZero(Digest, sizeof(Digest));
	if ( Result == XRT_CRYPTO_CHECK_VALID ) {
		return __xrtX509CheckValid();
	}
	if ( Result == XRT_CRYPTO_CHECK_ERROR ) {
		return __xrtX509CheckError();
	}
	if ( Result == XRT_CRYPTO_CHECK_ARGUMENT ) {
		__xrtErrorSetInvalidArgument();
		return __xrtX509CheckError();
	}
	Decision = __xrtX509VerifyEcdsaReject("the ECDSA certificate signature is invalid");
	Decision.CryptoCode = XCRYPTO_ERROR_SIGNATURE;
	Decision.CryptoKind = XERR_PROTOCOL;
	return Decision;
}

#endif
