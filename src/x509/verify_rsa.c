#include "../internal/xrt_x509.h"



#if defined(XRT_FEATURE_X509_VERIFY_RSA)

/* 用 X.509 错误包装 RSA 后端或参数策略失败。 */
static bool __xrtX509VerifyRsaError(
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



/* 验证 RFC 4055 中 RSA-PSS 公钥参数对具体签名的限制。 */
static bool __xrtX509VerifyRsaPssKey(
	const xx509signature* pScheme,
	const xx509pubkey* pPublicKey
)
{
	xx509signature Restriction;
	xx509result Result;
	const xerror* pCause;

	if ( pPublicKey->Type != X509_KEY_RSA_PSS ) {
		return true;
	}
	if ( !pPublicKey->Algorithm.HasParameters ) {
		return true;
	}
	Result = xrtX509SignatureParse(
		&pPublicKey->Algorithm, &Restriction
	);
	if ( Result != X509_VALUE ) {
		pCause = Result == X509_ERROR ? xrtGetError() : NULL;
		return __xrtX509VerifyRsaError(
			XERR_PROTOCOL,
			"the RSA-PSS public key restrictions are malformed",
			pCause
		);
	}
	if ( (Restriction.Type != X509_SIGNATURE_RSA_PSS) ||
		(Restriction.Hash != pScheme->Hash) ||
		(Restriction.MaskHash != pScheme->MaskHash) ||
		(Restriction.Trailer != pScheme->Trailer) ||
		(pScheme->SaltSize < Restriction.SaltSize) ) {
		return __xrtX509VerifyRsaError(
			XERR_PROTOCOL,
			"the signature violates RSA-PSS public key restrictions",
			NULL
		);
	}
	return true;
}



/* 使用 RSA-PKCS#1 或 RSA-PSS 后端验证证书协议签名。 */
bool __xrtX509VerifyRsa(
	const xx509signature* pScheme,
	xbytesview Content,
	xbytesview Signature,
	const xx509pubkey* pPublicKey
)
{
	xrsapublickey Key;
	xcryptohash Hash;
	xcryptohash MaskHash;
	uint8 Digest[XRT_X509_DIGEST_MAX_SIZE];
	size_t iDigestSize;
	const xerror* pCause;
	bool bResult;

	if ( (pPublicKey->Modulus.Data == NULL) ||
		(pPublicKey->Modulus.Size == 0) ||
		(pPublicKey->Exponent.Data == NULL) ||
		(pPublicKey->Exponent.Size == 0) ) {
		return __xrtX509VerifyRsaError(
			XERR_PROTOCOL, "the RSA public key view is incomplete", NULL
		);
	}
	if ( pScheme->Type == X509_SIGNATURE_RSA_PKCS1 ) {
		if ( pPublicKey->Type != X509_KEY_RSA ) {
			return __xrtX509VerifyRsaError(
				XERR_PROTOCOL,
				"RSA-PKCS#1 signatures require an unrestricted RSA key",
				NULL
			);
		}
	} else if ( (pPublicKey->Type != X509_KEY_RSA) &&
		(pPublicKey->Type != X509_KEY_RSA_PSS) ) {
		return __xrtX509VerifyRsaError(
			XERR_PROTOCOL,
			"RSA-PSS signature and public key types are incompatible",
			NULL
		);
	}
	if ( (pScheme->Type == X509_SIGNATURE_RSA_PSS) &&
		!__xrtX509VerifyRsaPssKey(pScheme, pPublicKey) ) {
		return false;
	}
	if ( !__xrtX509Digest(
		pScheme->Hash, Content, Digest, &iDigestSize
	) || !__xrtX509CryptoHash(pScheme->Hash, &Hash) ) {
		pCause = xrtGetError();
		xrtSecureZero(Digest, sizeof(Digest));
		return __xrtX509VerifyRsaError(
			XERR_UNSUPPORTED, "the RSA signature digest is unavailable", pCause
		);
	}
	(void)iDigestSize;
	Key.Modulus = pPublicKey->Modulus.Data;
	Key.ModulusSize = pPublicKey->Modulus.Size;
	Key.Exponent = pPublicKey->Exponent.Data;
	Key.ExponentSize = pPublicKey->Exponent.Size;
	if ( pScheme->Type == X509_SIGNATURE_RSA_PKCS1 ) {
		bResult = xrtRsaPkcs1Verify(
			&Key, Hash, Digest, Signature.Data, Signature.Size
		);
	} else if ( __xrtX509CryptoHash(pScheme->MaskHash, &MaskHash) ) {
		bResult = xrtRsaPssVerify(
			&Key, Hash, MaskHash, pScheme->SaltSize,
			Digest, Signature.Data, Signature.Size
		);
	} else {
		bResult = false;
	}
	if ( !bResult ) {
		pCause = xrtGetError();
		xrtSecureZero(Digest, sizeof(Digest));
		return __xrtX509VerifyRsaError(
			XERR_PROTOCOL, "the RSA certificate signature is invalid", pCause
		);
	}
	xrtSecureZero(Digest, sizeof(Digest));
	return true;
}

#endif
