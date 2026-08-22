#include "../internal/xrt_x509.h"



#if defined(XRT_FEATURE_X509_VERIFY_ECDSA)

/* 用 X.509 错误包装 ECDSA 后端或曲线策略失败。 */
static bool __xrtX509VerifyEcdsaError(
	xerrkind Kind,
	cstr sMessage,
	const xerror* pCause
)
{
	__xrtX509Error(
		Kind, X509_ERROR_SIGNATURE,
		"x509-signature-verify", sMessage, SIZE_MAX, pCause
	);
	return false;
}



/* 使用公钥命名曲线选择严格 DER ECDSA 验签后端。 */
bool __xrtX509VerifyEcdsa(
	const xx509signature* pScheme,
	xbytesview Content,
	xbytesview Signature,
	const xx509pubkey* pPublicKey
)
{
	uint8 Digest[XRT_X509_DIGEST_MAX_SIZE];
	size_t iDigestSize;
	const xerror* pCause;
	bool bResult;

	if ( (pPublicKey->Type != X509_KEY_EC) ||
		(pPublicKey->Key.Data == NULL) || (pPublicKey->Key.Size == 0) ) {
		return __xrtX509VerifyEcdsaError(
			XERR_PROTOCOL,
			"ECDSA signature and public key types are incompatible",
			NULL
		);
	}
	if ( !__xrtX509Digest(
		pScheme->Hash, Content, Digest, &iDigestSize
	) ) {
		pCause = xrtGetError();
		xrtSecureZero(Digest, sizeof(Digest));
		return __xrtX509VerifyEcdsaError(
			XERR_UNSUPPORTED, "the ECDSA signature digest is unavailable", pCause
		);
	}
	if ( pPublicKey->Curve == X509_CURVE_P256 ) {
		if ( pPublicKey->Key.Size != XRT_P256_PUBLIC_SIZE ) {
			xrtSecureZero(Digest, sizeof(Digest));
			return __xrtX509VerifyEcdsaError(
				XERR_PROTOCOL,
				"the P-256 public key point has the wrong size",
				NULL
			);
		}
		bResult = xrtEcdsaP256VerifyDer(
			Digest, iDigestSize, Signature.Data,
			Signature.Size, pPublicKey->Key.Data
		);
	} else if ( pPublicKey->Curve == X509_CURVE_P384 ) {
		if ( pPublicKey->Key.Size != XRT_P384_PUBLIC_SIZE ) {
			xrtSecureZero(Digest, sizeof(Digest));
			return __xrtX509VerifyEcdsaError(
				XERR_PROTOCOL,
				"the P-384 public key point has the wrong size",
				NULL
			);
		}
		bResult = xrtEcdsaP384VerifyDer(
			Digest, iDigestSize, Signature.Data,
			Signature.Size, pPublicKey->Key.Data
		);
	} else {
		xrtSecureZero(Digest, sizeof(Digest));
		return __xrtX509VerifyEcdsaError(
			XERR_UNSUPPORTED,
			"the EC public key curve has no enabled verification backend",
			NULL
		);
	}
	if ( !bResult ) {
		pCause = xrtGetError();
		xrtSecureZero(Digest, sizeof(Digest));
		return __xrtX509VerifyEcdsaError(
			XERR_PROTOCOL, "the ECDSA certificate signature is invalid", pCause
		);
	}
	xrtSecureZero(Digest, sizeof(Digest));
	return true;
}

#endif
