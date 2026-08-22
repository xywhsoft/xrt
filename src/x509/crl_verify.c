#include "../internal/xrt_x509.h"



#if defined(XRT_FEATURE_X509_CRL_VERIFY)

/* 设置 CRL 使用未知签名算法时的明确错误。 */
static bool __xrtX509CrlVerifyUnsupported(void)
{
	__xrtX509Error(
		XERR_UNSUPPORTED, X509_ERROR_SIGNATURE, "x509-crl-verify",
		"the CRL signature algorithm is not supported", SIZE_MAX, NULL
	);
	return false;
}



/* 使用已解析公钥只验证 CRL 密码签名。 */
XRT_API bool xrtX509CrlVerifyKey(
	const xx509crl* pCrl,
	const xx509pubkey* pPublicKey
)
{
	xx509signature Scheme;
	xx509result Result;

	if ( (pCrl == NULL) || (pPublicKey == NULL) ||
		(pCrl->Tbs.Data == NULL) || (pCrl->Tbs.Size == 0) ||
		(pCrl->Signature.Data == NULL) || (pCrl->Signature.Size == 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Result = xrtX509SignatureParse(&pCrl->SignatureAlgorithm, &Scheme);
	if ( Result == X509_ERROR ) {
		return false;
	}
	if ( Result == X509_DONE ) {
		return __xrtX509CrlVerifyUnsupported();
	}
	return xrtX509SignatureVerify(
		&Scheme, pCrl->Tbs, pCrl->Signature, pPublicKey
	);
}



/* 使用证书公钥只验证 CRL 密码签名。 */
XRT_API bool xrtX509CrlVerify(
	const xx509crl* pCrl,
	const xx509cert* pIssuer
)
{
	xx509pubkey PublicKey;

	if ( (pCrl == NULL) || (pIssuer == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtX509PublicKey(pIssuer, &PublicKey) ) {
		return false;
	}
	return xrtX509CrlVerifyKey(pCrl, &PublicKey);
}

#endif
