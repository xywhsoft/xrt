#include "../internal/xrt_x509.h"
#include "../internal/xrt_crypto_rsa_pkcs1.h"
#include "../internal/xrt_crypto_rsa_pss.h"



#if defined(XRT_FEATURE_X509_VERIFY_RSA)

/* 不分配诊断对象的 RSA 协议验证拒绝。 */
static xrt_x509_decision __xrtX509VerifyRsaReject(
	cstr sMessage
)
{
	return __xrtX509Decision(X509_CHECK_REJECTED, X509_ERROR_SIGNATURE, sMessage);
}



/* 验证 RFC 4055 中 RSA-PSS 公钥参数对具体签名的限制。 */
static xrt_x509_decision __xrtX509VerifyRsaPssKey(
	const xx509signature* pScheme,
	const xx509pubkey* pPublicKey
)
{
	xx509signature Restriction;
	xx509result Result;

	if ( pPublicKey->Type != X509_KEY_RSA_PSS ) {
		return __xrtX509CheckValid();
	}
	if ( !pPublicKey->Algorithm.HasParameters ) {
		return __xrtX509CheckValid();
	}
	Result = xrtX509SignatureParse(
		&pPublicKey->Algorithm, &Restriction
	);
	if ( Result == X509_ERROR ) {
		return __xrtX509CheckError();
	}
	if ( Result != X509_VALUE ) {
		return __xrtX509Decision(X509_CHECK_UNSUPPORTED, X509_ERROR_SIGNATURE,
			"the RSA-PSS public key restriction is not supported");
	}
	if ( (Restriction.Type != X509_SIGNATURE_RSA_PSS) ||
		(Restriction.Hash != pScheme->Hash) ||
		(Restriction.MaskHash != pScheme->MaskHash) ||
		(Restriction.Trailer != pScheme->Trailer) ||
		(pScheme->SaltSize < Restriction.SaltSize) ) {
		return __xrtX509VerifyRsaReject(
			"the signature violates RSA-PSS public key restrictions");
	}
	return __xrtX509CheckValid();
}



/* 使用 RSA-PKCS#1 或 RSA-PSS 后端验证证书协议签名。 */
xrt_x509_decision __xrtX509VerifyRsa(
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
	xrt_crypto_check Result;
	xrt_x509_decision Decision;

	if ( (pPublicKey->Modulus.Data == NULL) ||
		(pPublicKey->Modulus.Size == 0) ||
		(pPublicKey->Exponent.Data == NULL) ||
		(pPublicKey->Exponent.Size == 0) ) {
		return __xrtX509VerifyRsaReject("the RSA public key view is incomplete");
	}
	if ( pScheme->Type == X509_SIGNATURE_RSA_PKCS1 ) {
		if ( pPublicKey->Type != X509_KEY_RSA ) {
			return __xrtX509VerifyRsaReject(
				"RSA-PKCS#1 signatures require an unrestricted RSA key");
		}
	} else if ( (pPublicKey->Type != X509_KEY_RSA) &&
		(pPublicKey->Type != X509_KEY_RSA_PSS) ) {
		return __xrtX509VerifyRsaReject(
			"RSA-PSS signature and public key types are incompatible");
	}
	if ( pScheme->Type == X509_SIGNATURE_RSA_PSS ) {
		Decision = __xrtX509VerifyRsaPssKey(pScheme, pPublicKey);
		if ( Decision.Result != X509_CHECK_VALID ) {
			return Decision;
		}
	}
	if ( !__xrtX509Digest(
		pScheme->Hash, Content, Digest, &iDigestSize
	) || !__xrtX509CryptoHash(pScheme->Hash, &Hash) ) {
		xrtSecureZero(Digest, sizeof(Digest));
		return __xrtX509CheckError();
	}
	(void)iDigestSize;
	Key.Modulus = pPublicKey->Modulus.Data;
	Key.ModulusSize = pPublicKey->Modulus.Size;
	Key.Exponent = pPublicKey->Exponent.Data;
	Key.ExponentSize = pPublicKey->Exponent.Size;
	if ( pScheme->Type == X509_SIGNATURE_RSA_PKCS1 ) {
		Result = __xrtRsaPkcs1Check(
			&Key, Hash, Digest, Signature.Data, Signature.Size
		);
	} else if ( __xrtX509CryptoHash(pScheme->MaskHash, &MaskHash) ) {
		Result = __xrtRsaPssCheck(
			&Key, Hash, MaskHash, pScheme->SaltSize,
			Digest, Signature.Data, Signature.Size
		);
	} else {
		Result = XRT_CRYPTO_CHECK_ERROR;
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
	Decision = __xrtX509VerifyRsaReject("the RSA certificate signature is invalid");
	Decision.CryptoCode = Result == XRT_CRYPTO_CHECK_KEY ?
		XCRYPTO_ERROR_KEY : XCRYPTO_ERROR_SIGNATURE;
	Decision.CryptoKind = Result == XRT_CRYPTO_CHECK_KEY ? XERR_VALUE : XERR_PROTOCOL;
	return Decision;
}

#endif
