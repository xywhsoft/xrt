#include "../internal/xrt_tls_identity.h"



#if defined(XRT_FEATURE_TLS_IDENTITY_ED25519)

static const uint8 __xrtTlsIdentityOidEd25519[] = {
	0x2B, 0x65, 0x70
};



typedef struct __xrttlsidentityed25519 {
	xed25519key Key;
} __xrttlsidentityed25519;



/* 读取一个完整 DER OCTET STRING。 */
static bool __xrtTlsIdentityEdOctets(
	xbytesview Der,
	xbytesview* pOctets
)
{
	xdercursor Cursor;
	xdervalue Value;

	return xrtDerValidate(Der.Data, Der.Size) &&
		xrtDerInit(&Cursor, Der.Data, Der.Size) &&
		(xrtDerRead(&Cursor, &Value) == XDER_VALUE) && xrtDerDone(&Cursor) &&
		xrtDerOctets(&Value, pOctets);
}



/* 严格读取 RFC 8410 Ed25519 PrivateKeyInfo 的嵌套种子。 */
static bool __xrtTlsIdentityEdPkcs8(
	xbytesview Der,
	uint8* pSeed
)
{
	xdercursor Root;
	xdercursor Fields;
	xdervalue Sequence;
	xdervalue Version;
	xdervalue AlgorithmValue;
	xdervalue Private;
	xx509algorithm Algorithm;
	xbytesview PrivateDer;
	xbytesview Seed;
	uint64 iVersion;

	if ( !xrtDerValidate(Der.Data, Der.Size) ||
		!xrtDerInit(&Root, Der.Data, Der.Size) ||
		(xrtDerRead(&Root, &Sequence) != XDER_VALUE) ||
		!xrtDerDone(&Root) || !xrtDerIs(
			&Sequence, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
		) || !xrtDerEnter(&Sequence, &Fields) ||
		(xrtDerRead(&Fields, &Version) != XDER_VALUE) ||
		!xrtDerUInt64(&Version, &iVersion) || (iVersion != 0u) ||
		(xrtDerRead(&Fields, &AlgorithmValue) != XDER_VALUE) ||
		!xrtX509AlgorithmParse(AlgorithmValue.Raw, &Algorithm) ||
		(Algorithm.Oid.Size != sizeof(__xrtTlsIdentityOidEd25519)) ||
		(memcmp(
			Algorithm.Oid.Data, __xrtTlsIdentityOidEd25519,
			sizeof(__xrtTlsIdentityOidEd25519)
		) != 0) || Algorithm.HasParameters ||
		(xrtDerRead(&Fields, &Private) != XDER_VALUE) ||
		!xrtDerOctets(&Private, &PrivateDer) || !xrtDerDone(&Fields) ||
		!__xrtTlsIdentityEdOctets(PrivateDer, &Seed) ||
		(Seed.Size != XRT_ED25519_SEED_SIZE) ) {
		return __xrtTlsIdentityCause(
			"parse-tls-ed25519-key",
			"Ed25519 private key is not a canonical RFC 8410 PKCS#8 key"
		);
	}
	memcpy(pSeed, Seed.Data, XRT_ED25519_SEED_SIZE);
	return true;
}



/* 保留旧版原始种子与 OCTET 输入，同时严格识别标准 PKCS#8。 */
static bool __xrtTlsIdentityEdPrivate(
	xbytesview PrivateKey,
	uint8* pSeed
)
{
	xdercursor Root;
	xdervalue Value;
	xbytesview Seed;

	if ( (PrivateKey.Data == NULL) || (PrivateKey.Size == 0) ||
		(pSeed == NULL) ) {
		return __xrtTlsIdentityError(
			XERR_ARGUMENT, "parse-tls-ed25519-key",
			"Ed25519 private key input is empty"
		);
	}
	if ( PrivateKey.Size == XRT_ED25519_SEED_SIZE ) {
		memcpy(pSeed, PrivateKey.Data, XRT_ED25519_SEED_SIZE);
		return true;
	}
	if ( !xrtDerValidate(PrivateKey.Data, PrivateKey.Size) ||
		!xrtDerInit(&Root, PrivateKey.Data, PrivateKey.Size) ||
		(xrtDerRead(&Root, &Value) != XDER_VALUE) ||
		!xrtDerDone(&Root) ) {
		return __xrtTlsIdentityCause(
			"parse-tls-ed25519-key", "Ed25519 private key DER is malformed"
		);
	}
	if ( xrtDerIs(
		&Value, XASN1_UNIVERSAL, (uint32)XASN1_OCTET_STRING, false
	) ) {
		if ( !xrtDerOctets(&Value, &Seed) ) {
			return __xrtTlsIdentityCause(
				"parse-tls-ed25519-key",
				"Ed25519 private key OCTET STRING is malformed"
			);
		}
		if ( Seed.Size == XRT_ED25519_SEED_SIZE ) {
			memcpy(pSeed, Seed.Data, XRT_ED25519_SEED_SIZE);
			return true;
		}
		{
			xbytesview Nested;

			if ( __xrtTlsIdentityEdOctets(Seed, &Nested) &&
				(Nested.Size == XRT_ED25519_SEED_SIZE) ) {
				memcpy(
					pSeed, Nested.Data, XRT_ED25519_SEED_SIZE
				);
				return true;
			}
		}
		return __xrtTlsIdentityError(
			XERR_RANGE, "parse-tls-ed25519-key",
			"Ed25519 seed OCTET STRING must directly or indirectly contain 32 bytes"
		);
	}
	if ( !xrtDerIs(
		&Value, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
	) ) {
		return __xrtTlsIdentityError(
			XERR_PROTOCOL, "parse-tls-ed25519-key",
			"Ed25519 private key DER type is unsupported"
		);
	}
	return __xrtTlsIdentityEdPkcs8(PrivateKey, pSeed);
}



/* Ed25519 身份只接受协议定义的纯 Ed25519 签名方案。 */
static bool __xrtTlsIdentityEdSupports(
	const xtlsidentity* pIdentity,
	xtlsversion Version,
	xtlssignature Signature
)
{
	(void)pIdentity;
	(void)Version;
	return Signature == XTLS_SIGNATURE_ED25519;
}



/* 使用展开密钥原子生成固定 64 字节 Ed25519 签名。 */
static bool __xrtTlsIdentityEdSign(
	const xtlsidentity* pIdentity,
	xtlsversion Version,
	xtlssignature Signature,
	xbytesview Message,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	const __xrttlsidentityed25519* pEd =
		(const __xrttlsidentityed25519*)__xrtTlsIdentityExtra(pIdentity);
	uint8 Signed[XRT_ED25519_SIGNATURE_SIZE] = { 0 };
	bool bResult;

	(void)Version;
	(void)Signature;
	if ( pOutput == NULL ) {
		*pSize = sizeof(Signed);
		return true;
	}
	if ( iCapacity < sizeof(Signed) ) {
		return __xrtTlsIdentityError(
			XERR_RANGE, "sign-tls-ed25519-identity",
			"Ed25519 signature buffer is too small"
		);
	}
	bResult = xrtEd25519SignKey(
		&pEd->Key, Message.Data, Message.Size, Signed
	);
	if ( bResult ) {
		memcpy(pOutput, Signed, sizeof(Signed));
		*pSize = sizeof(Signed);
	}
	xrtSecureZero(Signed, sizeof(Signed));
	return bResult;
}



/* 从 32 字节种子、DER OCTET 或未加密 PKCS#8 私钥创建 Ed25519 身份。 */
XRT_API xtlsidentity* xrtTlsIdentityEd25519(
	const xbytesview* pCertificates,
	size_t iCertificateCount,
	xbytesview PrivateKey
)
{
	uint8 Seed[XRT_ED25519_SEED_SIZE] = { 0 };
	xed25519key Key;
	__xrttlsidentityed25519* pEd;
	xtlsidentity* pIdentity = NULL;
	xx509pubkey PublicKey;

	memset(&Key, 0, sizeof(Key));
	if ( !__xrtTlsIdentityEdPrivate(PrivateKey, Seed) ||
		!xrtEd25519KeyInit(&Key, Seed) ) {
		(void)__xrtTlsIdentityCause(
			"create-tls-ed25519-identity",
			"Ed25519 private key parsing or expansion failed"
		);
		goto Cleanup;
	}
	pIdentity = __xrtTlsIdentityNew(
		pCertificates, iCertificateCount, XTLS_IDENTITY_ED25519,
		sizeof(__xrttlsidentityed25519), __xrtTlsIdentityEdSupports,
		__xrtTlsIdentityEdSign, (ptr*)&pEd
	);
	if ( (pIdentity == NULL) || !xrtTlsIdentityPublicKey(
		pIdentity, &PublicKey
	) || (PublicKey.Key.Size != sizeof(Key.Public)) || !xrtConstTimeEqual(
		PublicKey.Key.Data, Key.Public, sizeof(Key.Public)
	) ) {
		if ( pIdentity != NULL ) {
			(void)__xrtTlsIdentityError(
				XERR_VALUE, "create-tls-ed25519-identity",
				"Ed25519 private key does not match the leaf certificate"
			);
			xrtTlsIdentityRelease(pIdentity);
			pIdentity = NULL;
		}
		goto Cleanup;
	}
	pEd->Key = Key;

Cleanup:
	xrtSecureZero(Seed, sizeof(Seed));
	xrtEd25519KeyClear(&Key);
	return pIdentity;
}

#endif
