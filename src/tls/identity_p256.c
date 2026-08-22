#include "../internal/xrt_tls_identity.h"



#if defined(XRT_FEATURE_TLS_IDENTITY_P256)

typedef struct __xrttlsidentityp256 {
	uint8 Private[XRT_P256_PRIVATE_SIZE];
} __xrttlsidentityp256;



/* P-256 身份遵守 TLS 1.2 摘要对与 TLS 1.3 曲线绑定语义。 */
static bool __xrtTlsIdentityP256Supports(
	const xtlsidentity* pIdentity,
	xtlsversion Version,
	xtlssignature Signature
)
{
	(void)pIdentity;
	return __xrtTlsIdentityEcSupports(
		XTLS_IDENTITY_ECDSA_P256, Version, Signature
	);
}



/* 对完整 TLS 待签内容执行线路摘要和确定性 low-S ECDSA。 */
static bool __xrtTlsIdentityP256Sign(
	const xtlsidentity* pIdentity,
	xtlsversion Version,
	xtlssignature Signature,
	xbytesview Message,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	const __xrttlsidentityp256* pEc =
		(const __xrttlsidentityp256*)__xrtTlsIdentityExtra(pIdentity);
	uint8 Hash[XRT_SHA512_SIZE];
	xcryptohash Algorithm = XCRYPTO_HASH_SHA256;
	bool bResult;

	(void)Version;
	if ( !__xrtTlsIdentityEcHash(
		Signature, Message, Hash, &Algorithm
	) ) {
		return false;
	}
	bResult = xrtEcdsaP256SignDer(
		Algorithm, Hash, pEc->Private, pOutput, iCapacity, pSize
	);
	xrtSecureZero(Hash, sizeof(Hash));
	return bResult;
}



/* 从原始标量、SEC1 或未加密 PKCS#8 私钥创建 P-256 身份。 */
XRT_API xtlsidentity* xrtTlsIdentityP256(
	const xbytesview* pCertificates,
	size_t iCertificateCount,
	xbytesview PrivateKey
)
{
	uint8 Scalar[XRT_P256_PRIVATE_SIZE] = { 0 };
	uint8 Derived[XRT_P256_PUBLIC_SIZE] = { 0 };
	xbytesview EncodedPublic;
	__xrttlsidentityp256* pEc;
	xtlsidentity* pIdentity = NULL;
	xx509pubkey PublicKey;

	if ( !__xrtTlsIdentityEcPrivate(
		PrivateKey, X509_CURVE_P256, Scalar, sizeof(Scalar), &EncodedPublic
	) || !xrtP256Public(Scalar, Derived) ) {
		(void)__xrtTlsIdentityCause(
			"create-tls-p256-identity",
			"P-256 private key parsing or public derivation failed"
		);
		goto Cleanup;
	}
	if ( (EncodedPublic.Data != NULL) &&
		((EncodedPublic.Size != sizeof(Derived)) || !xrtConstTimeEqual(
			EncodedPublic.Data, Derived, sizeof(Derived)
		)) ) {
		(void)__xrtTlsIdentityError(
			XERR_VALUE, "create-tls-p256-identity",
			"SEC1 public point does not match the private scalar"
		);
		goto Cleanup;
	}
	pIdentity = __xrtTlsIdentityNew(
		pCertificates, iCertificateCount, XTLS_IDENTITY_ECDSA_P256,
		sizeof(__xrttlsidentityp256), __xrtTlsIdentityP256Supports,
		__xrtTlsIdentityP256Sign, (ptr*)&pEc
	);
	if ( (pIdentity == NULL) || !xrtTlsIdentityPublicKey(
		pIdentity, &PublicKey
	) || (PublicKey.Key.Size != sizeof(Derived)) || !xrtConstTimeEqual(
		PublicKey.Key.Data, Derived, sizeof(Derived)
	) ) {
		if ( pIdentity != NULL ) {
			(void)__xrtTlsIdentityError(
				XERR_VALUE, "create-tls-p256-identity",
				"P-256 private key does not match the leaf certificate"
			);
			xrtTlsIdentityRelease(pIdentity);
			pIdentity = NULL;
		}
		goto Cleanup;
	}
	memcpy(pEc->Private, Scalar, sizeof(Scalar));

Cleanup:
	xrtSecureZero(Scalar, sizeof(Scalar));
	xrtSecureZero(Derived, sizeof(Derived));
	return pIdentity;
}

#endif
