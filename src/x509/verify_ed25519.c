#include "../internal/xrt_x509.h"
#include "../internal/xrt_crypto_ed25519.h"



#if defined(XRT_FEATURE_X509_VERIFY_ED25519)

/* 使用纯 Ed25519 模式验证证书协议签名。 */
xrt_x509_decision __xrtX509VerifyEd25519(
	const xx509signature* pScheme,
	xbytesview Content,
	xbytesview Signature,
	const xx509pubkey* pPublicKey
)
{
	xrt_crypto_check Result;
	xrt_x509_decision Decision;

	(void)pScheme;
	if ( (pPublicKey->Type != X509_KEY_ED25519) ||
		(pPublicKey->Key.Data == NULL) ||
		(pPublicKey->Key.Size != XRT_ED25519_PUBLIC_SIZE) ||
		(Signature.Size != XRT_ED25519_SIGNATURE_SIZE) ) {
		return __xrtX509Decision(X509_CHECK_REJECTED, X509_ERROR_SIGNATURE,
			"the Ed25519 signature or public key view is invalid");
	}
	Result = __xrtEd25519Check(
		pPublicKey->Key.Data, XED25519_PURE, NULL, 0,
		Content.Data,
		Content.Size,
		Signature.Data
	);
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
	Decision = __xrtX509Decision(X509_CHECK_REJECTED, X509_ERROR_SIGNATURE,
		"the Ed25519 certificate signature is invalid");
	Decision.CryptoCode = Result == XRT_CRYPTO_CHECK_KEY ?
		XCRYPTO_ERROR_KEY : XCRYPTO_ERROR_SIGNATURE;
	Decision.CryptoKind = Result == XRT_CRYPTO_CHECK_KEY ? XERR_VALUE : XERR_PROTOCOL;
	return Decision;
}

#endif
