#include "../internal/xrt_x509.h"



#if defined(XRT_FEATURE_X509_VERIFY_ED25519)

/* 使用纯 Ed25519 模式验证证书协议签名。 */
bool __xrtX509VerifyEd25519(
	const xx509signature* pScheme,
	xbytesview Content,
	xbytesview Signature,
	const xx509pubkey* pPublicKey
)
{
	const xerror* pCause;

	(void)pScheme;
	if ( (pPublicKey->Type != X509_KEY_ED25519) ||
		(pPublicKey->Key.Data == NULL) ||
		(pPublicKey->Key.Size != XRT_ED25519_PUBLIC_SIZE) ||
		(Signature.Size != XRT_ED25519_SIGNATURE_SIZE) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_SIGNATURE,
			"x509-signature-verify",
			"the Ed25519 signature or public key view is invalid",
			SIZE_MAX, NULL
		);
		return false;
	}
	if ( xrtEd25519Verify(
		pPublicKey->Key.Data,
		Content.Data,
		Content.Size,
		Signature.Data
	) ) {
		return true;
	}
	pCause = xrtGetError();
	__xrtX509Error(
		XERR_PROTOCOL, X509_ERROR_SIGNATURE,
		"x509-signature-verify",
		"the Ed25519 certificate signature is invalid",
		SIZE_MAX, pCause
	);
	return false;
}

#endif
