#include "../internal/xrt_x509.h"



#if defined(XRT_FEATURE_X509_VERIFY)

/* 判断摘要枚举是否属于当前协议模型。 */
static bool __xrtX509VerifyHash(xx509hash Hash)
{
	return (Hash >= X509_HASH_SHA1) && (Hash <= X509_HASH_SHA512);
}



/* 验证公开签名描述内部字段与方案类型保持一致。 */
static bool __xrtX509VerifyScheme(const xx509signature* pScheme)
{
	if ( pScheme == NULL ) {
		return false;
	}
	switch ( pScheme->Type ) {
		case X509_SIGNATURE_RSA_PKCS1:
		case X509_SIGNATURE_ECDSA:
			return __xrtX509VerifyHash(pScheme->Hash) &&
				(pScheme->MaskHash == X509_HASH_NONE) &&
				(pScheme->SaltSize == 0) && (pScheme->Trailer == 0);
		case X509_SIGNATURE_RSA_PSS:
			return __xrtX509VerifyHash(pScheme->Hash) &&
				__xrtX509VerifyHash(pScheme->MaskHash) &&
				(pScheme->Trailer == 1u);
		case X509_SIGNATURE_ED25519:
		case X509_SIGNATURE_ED448:
			return (pScheme->Hash == X509_HASH_NONE) &&
				(pScheme->MaskHash == X509_HASH_NONE) &&
				(pScheme->SaltSize == 0) && (pScheme->Trailer == 0);
		default:
			return false;
	}
}



/* 设置当前裁剪构建缺少验签后端的明确错误。 */
static bool __xrtX509VerifyUnsupported(cstr sMessage)
{
	__xrtX509Error(
		XERR_UNSUPPORTED, X509_ERROR_SIGNATURE,
		"x509-signature-verify", sMessage, SIZE_MAX, NULL
	);
	return false;
}



/* 使用已解析方案和公钥验证一段原始内容的证书协议签名。 */
XRT_API bool xrtX509SignatureVerify(
	const xx509signature* pScheme,
	xbytesview Content,
	xbytesview Signature,
	const xx509pubkey* pPublicKey
)
{
	if ( !__xrtX509VerifyScheme(pScheme) || (pPublicKey == NULL) ||
		((Content.Data == NULL) && (Content.Size != 0)) ||
		(Signature.Data == NULL) || (Signature.Size == 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	switch ( pScheme->Type ) {
		case X509_SIGNATURE_RSA_PKCS1:
		case X509_SIGNATURE_RSA_PSS:
			#if defined(XRT_FEATURE_X509_VERIFY_RSA)
				return __xrtX509VerifyRsa(
					pScheme, Content, Signature, pPublicKey
				);
			#else
				return __xrtX509VerifyUnsupported(
					"the RSA verification backend is not enabled"
				);
			#endif
		case X509_SIGNATURE_ECDSA:
			#if defined(XRT_FEATURE_X509_VERIFY_ECDSA)
				return __xrtX509VerifyEcdsa(
					pScheme, Content, Signature, pPublicKey
				);
			#else
				return __xrtX509VerifyUnsupported(
					"the ECDSA verification backend is not enabled"
				);
			#endif
		case X509_SIGNATURE_ED25519:
			#if defined(XRT_FEATURE_X509_VERIFY_ED25519)
				return __xrtX509VerifyEd25519(
					pScheme, Content, Signature, pPublicKey
				);
			#else
				return __xrtX509VerifyUnsupported(
					"the Ed25519 verification backend is not enabled"
				);
			#endif
		case X509_SIGNATURE_ED448:
			return __xrtX509VerifyUnsupported(
				"the Ed448 verification backend is not available"
			);
		default:
			__xrtErrorSetInvalidArgument();
			return false;
	}
}



/* 使用已解析公钥验证一张证书的签名，不执行任何路径策略。 */
XRT_API bool xrtX509CertificateVerifyKey(
	const xx509cert* pCertificate,
	const xx509pubkey* pPublicKey
)
{
	xx509signature Scheme;
	xx509result Result;

	if ( (pCertificate == NULL) || (pPublicKey == NULL) ||
		(pCertificate->Tbs.Data == NULL) ||
		(pCertificate->Tbs.Size == 0) ||
		(pCertificate->Signature.Data == NULL) ||
		(pCertificate->Signature.Size == 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Result = xrtX509SignatureParse(
		&pCertificate->SignatureAlgorithm, &Scheme
	);
	if ( Result == X509_ERROR ) {
		return false;
	}
	if ( Result == X509_DONE ) {
		return __xrtX509VerifyUnsupported(
			"the certificate signature algorithm is not supported"
		);
	}
	return xrtX509SignatureVerify(
		&Scheme,
		pCertificate->Tbs,
		pCertificate->Signature,
		pPublicKey
	);
}



/* 只验证一张证书的签名，不执行名称、用途、有效期或路径策略。 */
XRT_API bool xrtX509CertificateVerify(
	const xx509cert* pCertificate,
	const xx509cert* pIssuer
)
{
	xx509pubkey PublicKey;

	if ( (pCertificate == NULL) || (pIssuer == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtX509PublicKey(pIssuer, &PublicKey) ) {
		return false;
	}
	return xrtX509CertificateVerifyKey(pCertificate, &PublicKey);
}

#endif
