#include "../internal/xrt_tls_identity.h"



#if defined(XRT_FEATURE_TLS_IDENTITY_RSA)

static const uint8 __xrtTlsIdentityOidRsa[] = {
	0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x01
};

static const uint8 __xrtTlsIdentityOidRsaPss[] = {
	0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x0A
};



/* 内置 RSA 身份尾部保留完整私钥视图和随后深复制的 DER。 */
typedef struct __xrttlsidentityrsa {
	xrsaprivatekey Key;
	xx509signature PrivateRestriction;
	xx509signature PublicRestriction;
	bool PrivatePss;
	bool PrivateRestricted;
	bool PublicRestricted;
	size_t DerSize;
	uint8 Der[1];
} __xrttlsidentityrsa;



/* 读取一个非零规范无符号私钥整数。 */
static bool __xrtTlsIdentityRsaInteger(
	xdercursor* pCursor,
	xbytesview* pInteger
)
{
	xdervalue Value;

	return (xrtDerRead(pCursor, &Value) == XDER_VALUE) &&
		xrtDerUnsigned(&Value, pInteger) && (pInteger->Size != 0) &&
		!((pInteger->Size == 1u) && (pInteger->Data[0] == 0));
}



/* 严格解析传统双素数 PKCS#1 RSAPrivateKey。 */
static bool __xrtTlsIdentityRsaPkcs1(
	xbytesview Der,
	xrsaprivatekey* pKey
)
{
	xdercursor Root;
	xdercursor Fields;
	xdervalue Sequence;
	xdervalue Version;
	xbytesview Modulus;
	xbytesview Exponent;
	xbytesview PrivateExponent;
	xbytesview Prime1;
	xbytesview Prime2;
	xbytesview Exponent1;
	xbytesview Exponent2;
	xbytesview Coefficient;
	uint64 iVersion;
	xrsaprivatekey Key;

	if ( !xrtDerValidate(Der.Data, Der.Size) ||
		!xrtDerInit(&Root, Der.Data, Der.Size) ||
		(xrtDerRead(&Root, &Sequence) != XDER_VALUE) ||
		!xrtDerDone(&Root) || !xrtDerIs(
			&Sequence, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
		) || !xrtDerEnter(&Sequence, &Fields) ||
		(xrtDerRead(&Fields, &Version) != XDER_VALUE) ||
		!xrtDerUInt64(&Version, &iVersion) || (iVersion != 0) ||
		!__xrtTlsIdentityRsaInteger(&Fields, &Modulus) ||
		!__xrtTlsIdentityRsaInteger(&Fields, &Exponent) ||
		!__xrtTlsIdentityRsaInteger(&Fields, &PrivateExponent) ||
		!__xrtTlsIdentityRsaInteger(&Fields, &Prime1) ||
		!__xrtTlsIdentityRsaInteger(&Fields, &Prime2) ||
		!__xrtTlsIdentityRsaInteger(&Fields, &Exponent1) ||
		!__xrtTlsIdentityRsaInteger(&Fields, &Exponent2) ||
		!__xrtTlsIdentityRsaInteger(&Fields, &Coefficient) ||
		!xrtDerDone(&Fields) ) {
		return __xrtTlsIdentityCause(
			"parse-tls-rsa-key",
			"RSA private key is not a canonical two-prime PKCS#1 key"
		);
	}
	memset(&Key, 0, sizeof(Key));
	Key.Public.Modulus = Modulus.Data;
	Key.Public.ModulusSize = Modulus.Size;
	Key.Public.Exponent = Exponent.Data;
	Key.Public.ExponentSize = Exponent.Size;
	Key.PrivateExponent = PrivateExponent.Data;
	Key.PrivateExponentSize = PrivateExponent.Size;
	Key.Prime1 = Prime1.Data;
	Key.Prime1Size = Prime1.Size;
	Key.Prime2 = Prime2.Data;
	Key.Prime2Size = Prime2.Size;
	Key.Exponent1 = Exponent1.Data;
	Key.Exponent1Size = Exponent1.Size;
	Key.Exponent2 = Exponent2.Data;
	Key.Exponent2Size = Exponent2.Size;
	Key.Coefficient = Coefficient.Data;
	Key.CoefficientSize = Coefficient.Size;
	*pKey = Key;
	return true;
}



/* 读取 PKCS#8 AlgorithmIdentifier 并区分通用 RSA 与受限 PSS 密钥。 */
static bool __xrtTlsIdentityRsaAlgorithm(
	const xdervalue* pValue,
	bool* pPss,
	bool* pRestricted,
	xx509signature* pRestriction
)
{
	xx509algorithm Algorithm;
	xx509signature Restriction;
	xdercursor Cursor;
	xdervalue Parameter;
	xx509result Result;

	if ( !xrtX509AlgorithmParse(pValue->Raw, &Algorithm) ) {
		return __xrtTlsIdentityCause(
			"parse-tls-rsa-key",
			"PKCS#8 RSA algorithm identifier is malformed"
		);
	}
	if ( (Algorithm.Oid.Size == sizeof(__xrtTlsIdentityOidRsa)) &&
		(memcmp(
			Algorithm.Oid.Data, __xrtTlsIdentityOidRsa,
			sizeof(__xrtTlsIdentityOidRsa)
		) == 0) ) {
		if ( !Algorithm.HasParameters || !xrtDerInit(
			&Cursor, Algorithm.Parameters.Data, Algorithm.Parameters.Size
		) || (xrtDerRead(&Cursor, &Parameter) != XDER_VALUE) ||
			!xrtDerDone(&Cursor) || !xrtDerIs(
				&Parameter, XASN1_UNIVERSAL, (uint32)XASN1_NULL, false
			) || (Parameter.Value.Size != 0) ) {
			return __xrtTlsIdentityError(
				XERR_PROTOCOL, "parse-tls-rsa-key",
				"PKCS#8 rsaEncryption requires canonical NULL parameters"
			);
		}
		*pPss = false;
		*pRestricted = false;
		return true;
	}
	if ( (Algorithm.Oid.Size == sizeof(__xrtTlsIdentityOidRsaPss)) &&
		(memcmp(
			Algorithm.Oid.Data, __xrtTlsIdentityOidRsaPss,
			sizeof(__xrtTlsIdentityOidRsaPss)
		) == 0) ) {
		*pPss = true;
		*pRestricted = Algorithm.HasParameters;
		if ( !Algorithm.HasParameters ) {
			return true;
		}
		Result = xrtX509SignatureParse(&Algorithm, &Restriction);
		if ( (Result != X509_VALUE) ||
			(Restriction.Type != X509_SIGNATURE_RSA_PSS) ) {
			return __xrtTlsIdentityCause(
				"parse-tls-rsa-key",
				"PKCS#8 RSA-PSS restrictions are malformed"
			);
		}
		*pRestriction = Restriction;
		return true;
	}
	return __xrtTlsIdentityError(
		XERR_VALUE, "parse-tls-rsa-key",
		"PKCS#8 private key algorithm is not RSA"
	);
}



/* 严格解包 RFC 5208 PrivateKeyInfo，并复用同一 PKCS#1 解析器。 */
static bool __xrtTlsIdentityRsaPkcs8(
	xbytesview Der,
	xrsaprivatekey* pKey,
	bool* pPss,
	bool* pRestricted,
	xx509signature* pRestriction
)
{
	xdercursor Root;
	xdercursor Fields;
	xdervalue Sequence;
	xdervalue Version;
	xdervalue Algorithm;
	xdervalue PrivateKey;
	xbytesview PrivateDer;
	uint64 iVersion;

	if ( !xrtDerValidate(Der.Data, Der.Size) ||
		!xrtDerInit(&Root, Der.Data, Der.Size) ||
		(xrtDerRead(&Root, &Sequence) != XDER_VALUE) ||
		!xrtDerDone(&Root) || !xrtDerIs(
			&Sequence, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
		) || !xrtDerEnter(&Sequence, &Fields) ||
		(xrtDerRead(&Fields, &Version) != XDER_VALUE) ||
		!xrtDerUInt64(&Version, &iVersion) || (iVersion != 0) ||
		(xrtDerRead(&Fields, &Algorithm) != XDER_VALUE) ||
		(xrtDerRead(&Fields, &PrivateKey) != XDER_VALUE) ||
		!xrtDerOctets(&PrivateKey, &PrivateDer) ||
		!xrtDerDone(&Fields) ) {
		return __xrtTlsIdentityCause(
			"parse-tls-rsa-key",
			"RSA private key is not a canonical PKCS#8 PrivateKeyInfo"
		);
	}
	return __xrtTlsIdentityRsaAlgorithm(
		&Algorithm, pPss, pRestricted, pRestriction
	) &&
		__xrtTlsIdentityRsaPkcs1(PrivateDer, pKey);
}



/* 按顶层第二字段类型无歧义地区分 PKCS#1 与 PKCS#8。 */
static bool __xrtTlsIdentityRsaParse(
	xbytesview Der,
	xrsaprivatekey* pKey,
	bool* pPss,
	bool* pRestricted,
	xx509signature* pRestriction
)
{
	xdercursor Root;
	xdercursor Fields;
	xdervalue Sequence;
	xdervalue Version;
	xdervalue Second;

	if ( (Der.Data == NULL) || (Der.Size == 0) ||
		!xrtDerValidate(Der.Data, Der.Size) ||
		!xrtDerInit(&Root, Der.Data, Der.Size) ||
		(xrtDerRead(&Root, &Sequence) != XDER_VALUE) ||
		!xrtDerDone(&Root) || !xrtDerIs(
			&Sequence, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
		) || !xrtDerEnter(&Sequence, &Fields) ||
		(xrtDerRead(&Fields, &Version) != XDER_VALUE) ||
		(xrtDerRead(&Fields, &Second) != XDER_VALUE) ) {
		return __xrtTlsIdentityCause(
			"parse-tls-rsa-key", "RSA private key DER is malformed"
		);
	}
	if ( xrtDerIs(
		&Second, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
	) ) {
		return __xrtTlsIdentityRsaPkcs8(
			Der, pKey, pPss, pRestricted, pRestriction
		);
	}
	*pPss = false;
	*pRestricted = false;
	return __xrtTlsIdentityRsaPkcs1(Der, pKey);
}



/* 解析叶证书中可选的 RSA-PSS 公钥限制。 */
static bool __xrtTlsIdentityRsaPublicRestriction(
	const xtlsidentity* pIdentity,
	bool* pRestricted,
	xx509signature* pRestriction
)
{
	xx509pubkey PublicKey;
	xx509result Result;

	*pRestricted = false;
	if ( !xrtTlsIdentityPublicKey(pIdentity, &PublicKey) ) {
		return false;
	}
	if ( (PublicKey.Type != X509_KEY_RSA_PSS) ||
		!PublicKey.Algorithm.HasParameters ) {
		return true;
	}
	Result = xrtX509SignatureParse(
		&PublicKey.Algorithm, pRestriction
	);
	if ( (Result != X509_VALUE) ||
		(pRestriction->Type != X509_SIGNATURE_RSA_PSS) ) {
		return __xrtTlsIdentityCause(
			"create-tls-rsa-identity",
			"RSA-PSS certificate restrictions are malformed"
		);
	}
	*pRestricted = true;
	return true;
}



/* 精确核对证书公钥与私钥公开参数。 */
static bool __xrtTlsIdentityRsaMatch(
	const xtlsidentity* pIdentity,
	const xrsaprivatekey* pKey,
	bool bPss
)
{
	xx509pubkey PublicKey;

	if ( !xrtTlsIdentityPublicKey(pIdentity, &PublicKey) ) {
		return false;
	}
	if ( bPss && (xrtTlsIdentityType(pIdentity) != XTLS_IDENTITY_RSA_PSS) ) {
		return __xrtTlsIdentityError(
			XERR_VALUE, "create-tls-rsa-identity",
			"restricted RSA-PSS private key requires an RSA-PSS certificate"
		);
	}
	if ( (PublicKey.Modulus.Size != pKey->Public.ModulusSize) ||
		(PublicKey.Exponent.Size != pKey->Public.ExponentSize) ||
		!xrtConstTimeEqual(
			PublicKey.Modulus.Data, pKey->Public.Modulus,
			PublicKey.Modulus.Size
		) || !xrtConstTimeEqual(
			PublicKey.Exponent.Data, pKey->Public.Exponent,
			PublicKey.Exponent.Size
		) ) {
		return __xrtTlsIdentityError(
			XERR_VALUE, "create-tls-rsa-identity",
			"RSA private key does not match the leaf certificate"
		);
	}
	return true;
}



/* 把 TLS RSA-PSS 方案映射为 RFC 4055 限制比较所需参数。 */
static bool __xrtTlsIdentityRsaPssScheme(
	xtlssignature Signature,
	xx509signature* pScheme
)
{
	xx509signature Scheme;

	memset(&Scheme, 0, sizeof(Scheme));
	Scheme.Type = X509_SIGNATURE_RSA_PSS;
	Scheme.Trailer = 1u;
	if ( (Signature == XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256) ||
		(Signature == XTLS_SIGNATURE_RSA_PSS_PSS_SHA256) ) {
		Scheme.Hash = X509_HASH_SHA256;
		Scheme.MaskHash = X509_HASH_SHA256;
		Scheme.SaltSize = 32u;
	} else if ( (Signature == XTLS_SIGNATURE_RSA_PSS_RSAE_SHA384) ||
		(Signature == XTLS_SIGNATURE_RSA_PSS_PSS_SHA384) ) {
		Scheme.Hash = X509_HASH_SHA384;
		Scheme.MaskHash = X509_HASH_SHA384;
		Scheme.SaltSize = 48u;
	} else if ( (Signature == XTLS_SIGNATURE_RSA_PSS_RSAE_SHA512) ||
		(Signature == XTLS_SIGNATURE_RSA_PSS_PSS_SHA512) ) {
		Scheme.Hash = X509_HASH_SHA512;
		Scheme.MaskHash = X509_HASH_SHA512;
		Scheme.SaltSize = 64u;
	} else {
		return false;
	}
	*pScheme = Scheme;
	return true;
}



/* 判断一个 RSA-PSS 密钥限制是否允许具体 TLS PSS 方案。 */
static bool __xrtTlsIdentityRsaRestrictionAllows(
	const xx509signature* pRestriction,
	const xx509signature* pScheme
)
{
	return (pRestriction->Type == X509_SIGNATURE_RSA_PSS) &&
		(pRestriction->Hash == pScheme->Hash) &&
		(pRestriction->MaskHash == pScheme->MaskHash) &&
		(pRestriction->Trailer == pScheme->Trailer) &&
		(pRestriction->SaltSize <= pScheme->SaltSize);
}



/* 通过完整 CRT 私钥运算验证因子、指数、系数和公开参数。 */
static bool __xrtTlsIdentityRsaValidate(const xrsaprivatekey* pKey)
{
	uint8 Input[XRT_RSA_MODULUS_MAX_SIZE] = { 0 };
	uint8 Output[XRT_RSA_MODULUS_MAX_SIZE] = { 0 };
	bool bResult;

	if ( (pKey->Public.ModulusSize < XRT_RSA_MODULUS_MIN_SIZE) ||
		(pKey->Public.ModulusSize > XRT_RSA_MODULUS_MAX_SIZE) ) {
		return __xrtTlsIdentityError(
			XERR_RANGE, "create-tls-rsa-identity",
			"RSA identity modulus size is outside supported limits"
		);
	}
	Input[pKey->Public.ModulusSize - 1u] = 2u;
	bResult = xrtRsaPrivate(
		pKey, Input, pKey->Public.ModulusSize, Output
	);
	xrtSecureZero(Input, sizeof(Input));
	xrtSecureZero(Output, sizeof(Output));
	if ( !bResult ) {
		return __xrtTlsIdentityCause(
			"create-tls-rsa-identity",
			"RSA identity private parameters are inconsistent"
		);
	}
	return true;
}



/* 内置 RSA 后端支持与证书身份类型相符的全部已实现方案。 */
static bool __xrtTlsIdentityRsaSupports(
	const xtlsidentity* pIdentity,
	xtlsversion Version,
	xtlssignature Signature
)
{
	const __xrttlsidentityrsa* pRsa =
		(const __xrttlsidentityrsa*)__xrtTlsIdentityExtra(pIdentity);
	xx509signature Scheme;

	(void)Version;
	if ( (Signature == XTLS_SIGNATURE_RSA_PKCS1_SHA256) ||
		(Signature == XTLS_SIGNATURE_RSA_PKCS1_SHA384) ||
		(Signature == XTLS_SIGNATURE_RSA_PKCS1_SHA512) ) {
		return !pRsa->PrivatePss;
	}
	if ( !__xrtTlsIdentityRsaPssScheme(Signature, &Scheme) ) {
		return false;
	}
	if ( pRsa->PrivateRestricted &&
		!__xrtTlsIdentityRsaRestrictionAllows(
			&pRsa->PrivateRestriction, &Scheme
		) ) {
		return false;
	}
	return !pRsa->PublicRestricted ||
		__xrtTlsIdentityRsaRestrictionAllows(
			&pRsa->PublicRestriction, &Scheme
		);
}



/* 按 TLS 签名方案计算 SHA-256、SHA-384 或 SHA-512。 */
static bool __xrtTlsIdentityRsaHash(
	xtlssignature Signature,
	xbytesview Message,
	uint8* pHash,
	xcryptohash* pAlgorithm
)
{
	if ( (Signature == XTLS_SIGNATURE_RSA_PKCS1_SHA256) ||
		(Signature == XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256) ||
		(Signature == XTLS_SIGNATURE_RSA_PSS_PSS_SHA256) ) {
		*pAlgorithm = XCRYPTO_HASH_SHA256;
		return xrtSha256(Message.Data, Message.Size, pHash);
	}
	if ( (Signature == XTLS_SIGNATURE_RSA_PKCS1_SHA384) ||
		(Signature == XTLS_SIGNATURE_RSA_PSS_RSAE_SHA384) ||
		(Signature == XTLS_SIGNATURE_RSA_PSS_PSS_SHA384) ) {
		*pAlgorithm = XCRYPTO_HASH_SHA384;
		return xrtSha384(Message.Data, Message.Size, pHash);
	}
	if ( (Signature == XTLS_SIGNATURE_RSA_PKCS1_SHA512) ||
		(Signature == XTLS_SIGNATURE_RSA_PSS_RSAE_SHA512) ||
		(Signature == XTLS_SIGNATURE_RSA_PSS_PSS_SHA512) ) {
		*pAlgorithm = XCRYPTO_HASH_SHA512;
		return xrtSha512(Message.Data, Message.Size, pHash);
	}
	return __xrtTlsIdentityError(
		XERR_VALUE, "sign-tls-rsa-identity",
		"RSA identity signature scheme is unsupported"
	);
}



/* 使用临时签名保证内置 RSA 后端的失败原子性。 */
static bool __xrtTlsIdentityRsaSign(
	const xtlsidentity* pIdentity,
	xtlsversion Version,
	xtlssignature Signature,
	xbytesview Message,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	const __xrttlsidentityrsa* pRsa =
		(const __xrttlsidentityrsa*)__xrtTlsIdentityExtra(pIdentity);
	uint8 Hash[64] = { 0 };
	uint8 Signed[XRT_RSA_MODULUS_MAX_SIZE] = { 0 };
	xcryptohash Algorithm = XCRYPTO_HASH_SHA256;
	size_t iSize = pRsa->Key.Public.ModulusSize;
	bool bPss;
	bool bResult;

	(void)Version;
	if ( pOutput == NULL ) {
		*pSize = iSize;
		return true;
	}
	if ( iCapacity < iSize ) {
		return __xrtTlsIdentityError(
			XERR_RANGE, "sign-tls-rsa-identity",
			"RSA identity signature buffer is too small"
		);
	}
	if ( !__xrtTlsIdentityRsaHash(
		Signature, Message, Hash, &Algorithm
	) ) {
		return false;
	}
	bPss = (Signature == XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256) ||
		(Signature == XTLS_SIGNATURE_RSA_PSS_RSAE_SHA384) ||
		(Signature == XTLS_SIGNATURE_RSA_PSS_RSAE_SHA512) ||
		(Signature == XTLS_SIGNATURE_RSA_PSS_PSS_SHA256) ||
		(Signature == XTLS_SIGNATURE_RSA_PSS_PSS_SHA384) ||
		(Signature == XTLS_SIGNATURE_RSA_PSS_PSS_SHA512);
	if ( bPss ) {
		bResult = xrtRsaPssSign(
			&pRsa->Key, Algorithm, Algorithm, Hash, Signed
		);
	} else {
		bResult = xrtRsaPkcs1Sign(
			&pRsa->Key, Algorithm, Hash, Signed
		);
	}
	if ( bResult ) {
		memcpy(pOutput, Signed, iSize);
		*pSize = iSize;
	}
	xrtSecureZero(Hash, sizeof(Hash));
	xrtSecureZero(Signed, sizeof(Signed));
	return bResult;
}



/* 从 PKCS#1 或未加密 PKCS#8 DER 私钥创建 RSA 身份。 */
XRT_API xtlsidentity* xrtTlsIdentityRsa(
	const xbytesview* pCertificates,
	size_t iCertificateCount,
	xbytesview PrivateKey
)
{
	xtlsidentity* pIdentity;
	__xrttlsidentityrsa* pRsa;
	xrsaprivatekey Key;
	xtlsidentitytype Type;
	size_t iExtraSize;

	if ( (PrivateKey.Data == NULL) || (PrivateKey.Size == 0) ||
		(PrivateKey.Size > (SIZE_MAX - sizeof(__xrttlsidentityrsa) + 1u)) ) {
		(void)__xrtTlsIdentityError(
			XERR_ARGUMENT, "create-tls-rsa-identity",
			"RSA identity private key is empty or too large"
		);
		return NULL;
	}
	if ( (pCertificates == NULL) || (iCertificateCount == 0) ) {
		(void)__xrtTlsIdentityError(
			XERR_ARGUMENT, "create-tls-rsa-identity",
			"RSA identity certificate chain is empty"
		);
		return NULL;
	}
	{
		xx509cert Leaf;
		xx509pubkey PublicKey;

		if ( !xrtX509Parse(
			pCertificates[0].Data, pCertificates[0].Size, &Leaf
		) || !xrtX509PublicKey(&Leaf, &PublicKey) ) {
			(void)__xrtTlsIdentityCause(
				"create-tls-rsa-identity",
				"RSA identity leaf certificate parsing failed"
			);
			return NULL;
		}
		Type = PublicKey.Type == X509_KEY_RSA_PSS ?
			XTLS_IDENTITY_RSA_PSS : XTLS_IDENTITY_RSA;
		if ( (PublicKey.Type != X509_KEY_RSA) &&
			(PublicKey.Type != X509_KEY_RSA_PSS) ) {
			(void)__xrtTlsIdentityError(
				XERR_VALUE, "create-tls-rsa-identity",
				"RSA identity requires an RSA leaf certificate"
			);
			return NULL;
		}
	}
	iExtraSize = sizeof(__xrttlsidentityrsa) - 1u + PrivateKey.Size;
	pIdentity = __xrtTlsIdentityNew(
		pCertificates, iCertificateCount, Type, iExtraSize,
		__xrtTlsIdentityRsaSupports, __xrtTlsIdentityRsaSign,
		(ptr*)&pRsa
	);
	if ( pIdentity == NULL ) {
		return NULL;
	}
	pRsa->DerSize = PrivateKey.Size;
	memcpy(pRsa->Der, PrivateKey.Data, PrivateKey.Size);
	if ( !__xrtTlsIdentityRsaParse(
		(xbytesview) { pRsa->Der, pRsa->DerSize }, &Key,
		&pRsa->PrivatePss, &pRsa->PrivateRestricted,
		&pRsa->PrivateRestriction
	) ) {
		xrtTlsIdentityRelease(pIdentity);
		return NULL;
	}
	pRsa->Key = Key;
	if ( !__xrtTlsIdentityRsaPublicRestriction(
		pIdentity, &pRsa->PublicRestricted, &pRsa->PublicRestriction
	) || !__xrtTlsIdentityRsaMatch(
		pIdentity, &pRsa->Key, pRsa->PrivatePss
	) ||
		!__xrtTlsIdentityRsaValidate(&pRsa->Key) ) {
		xrtTlsIdentityRelease(pIdentity);
		return NULL;
	}
	if ( (Type == XTLS_IDENTITY_RSA_PSS) &&
		!__xrtTlsIdentityRsaSupports(
			pIdentity, XTLS_VERSION_13,
			XTLS_SIGNATURE_RSA_PSS_PSS_SHA256
		) && !__xrtTlsIdentityRsaSupports(
			pIdentity, XTLS_VERSION_13,
			XTLS_SIGNATURE_RSA_PSS_PSS_SHA384
		) && !__xrtTlsIdentityRsaSupports(
			pIdentity, XTLS_VERSION_13,
			XTLS_SIGNATURE_RSA_PSS_PSS_SHA512
		) ) {
		(void)__xrtTlsIdentityError(
			XERR_VALUE, "create-tls-rsa-identity",
			"RSA-PSS certificate and private-key restrictions have no supported TLS scheme"
		);
		xrtTlsIdentityRelease(pIdentity);
		return NULL;
	}
	return pIdentity;
}

#endif
