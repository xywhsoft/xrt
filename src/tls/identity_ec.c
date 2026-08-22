#include "../internal/xrt_tls_identity.h"



#if defined(XRT_FEATURE_TLS_IDENTITY_EC)

static const uint8 __xrtTlsIdentityOidEc[] = {
	0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01
};

static const uint8 __xrtTlsIdentityOidP256[] = {
	0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07
};

static const uint8 __xrtTlsIdentityOidP384[] = {
	0x2B, 0x81, 0x04, 0x00, 0x22
};



/* 判断借用 OID 是否与固定曲线 OID 完全相同。 */
static bool __xrtTlsIdentityEcOid(
	xbytesview Oid,
	const void* pExpected,
	size_t iExpectedSize
)
{
	return (Oid.Size == iExpectedSize) &&
		(memcmp(Oid.Data, pExpected, iExpectedSize) == 0);
}



/* 检查命名曲线 OID 与构造器要求的曲线一致。 */
static bool __xrtTlsIdentityEcCurve(
	const xdervalue* pValue,
	xx509curve Curve
)
{
	xbytesview Oid;

	if ( !xrtDerOid(pValue, &Oid) ) {
		return false;
	}
	if ( Curve == X509_CURVE_P256 ) {
		return __xrtTlsIdentityEcOid(
			Oid, __xrtTlsIdentityOidP256,
			sizeof(__xrtTlsIdentityOidP256)
		);
	}
	if ( Curve == X509_CURVE_P384 ) {
		return __xrtTlsIdentityEcOid(
			Oid, __xrtTlsIdentityOidP384,
			sizeof(__xrtTlsIdentityOidP384)
		);
	}
	return false;
}



/* 从显式上下文字段中读取唯一内层 DER 值。 */
static bool __xrtTlsIdentityEcExplicit(
	const xdervalue* pField,
	xdervalue* pValue
)
{
	xdercursor Cursor;

	return pField->Tag.Constructed && xrtDerEnter(pField, &Cursor) &&
		(xrtDerRead(&Cursor, pValue) == XDER_VALUE) && xrtDerDone(&Cursor);
}



/* 严格读取 RFC 5915 ECPrivateKey 及其可选曲线和公钥。 */
static bool __xrtTlsIdentityEcSec1(
	xbytesview Der,
	xx509curve Curve,
	void* pScalar,
	size_t iScalarSize,
	xbytesview* pEncodedPublic
)
{
	xdercursor Root;
	xdercursor Fields;
	xdervalue Sequence;
	xdervalue Version;
	xdervalue Private;
	xdervalue Field;
	xbytesview Scalar;
	xbytesview Public = { NULL, 0 };
	uint8 iUnused;
	uint64 iVersion;
	int iPrevious = -1;

	if ( !xrtDerValidate(Der.Data, Der.Size) ||
		!xrtDerInit(&Root, Der.Data, Der.Size) ||
		(xrtDerRead(&Root, &Sequence) != XDER_VALUE) ||
		!xrtDerDone(&Root) || !xrtDerIs(
			&Sequence, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
		) || !xrtDerEnter(&Sequence, &Fields) ||
		(xrtDerRead(&Fields, &Version) != XDER_VALUE) ||
		!xrtDerUInt64(&Version, &iVersion) || (iVersion != 1u) ||
		(xrtDerRead(&Fields, &Private) != XDER_VALUE) ||
		!xrtDerOctets(&Private, &Scalar) ||
		(Scalar.Size != iScalarSize) ) {
		return __xrtTlsIdentityCause(
			"parse-tls-ec-key",
			"EC private key is not a canonical SEC1 key"
		);
	}
	while ( xrtDerRead(&Fields, &Field) == XDER_VALUE ) {
		xdervalue Value;
		int iTag;

		if ( (Field.Tag.Class != XASN1_CONTEXT) ||
			!Field.Tag.Constructed || (Field.Tag.Number > 1u) ) {
			return __xrtTlsIdentityError(
				XERR_PROTOCOL, "parse-tls-ec-key",
				"SEC1 private key has an unsupported optional field"
			);
		}
		iTag = (int)Field.Tag.Number;
		if ( iTag <= iPrevious ) {
			return __xrtTlsIdentityError(
				XERR_PROTOCOL, "parse-tls-ec-key",
				"SEC1 private key optional fields are duplicated or unordered"
			);
		}
		iPrevious = iTag;
		if ( !__xrtTlsIdentityEcExplicit(&Field, &Value) ) {
			return __xrtTlsIdentityCause(
				"parse-tls-ec-key",
				"SEC1 private key optional field is malformed"
			);
		}
		if ( iTag == 0 ) {
			if ( !__xrtTlsIdentityEcCurve(&Value, Curve) ) {
				return __xrtTlsIdentityError(
					XERR_VALUE, "parse-tls-ec-key",
					"SEC1 private key named curve does not match the identity"
				);
			}
		} else if ( !xrtDerBitString(&Value, &Public, &iUnused) ||
			(iUnused != 0) ) {
			return __xrtTlsIdentityCause(
				"parse-tls-ec-key",
				"SEC1 private key public point is malformed"
			);
		}
	}
	if ( !xrtDerDone(&Fields) ) {
		return __xrtTlsIdentityCause(
			"parse-tls-ec-key", "SEC1 private key fields are malformed"
		);
	}
	memcpy(pScalar, Scalar.Data, iScalarSize);
	*pEncodedPublic = Public;
	return true;
}



/* 严格读取 RFC 5208 EC PrivateKeyInfo 并复用 SEC1 解析器。 */
static bool __xrtTlsIdentityEcPkcs8(
	xbytesview Der,
	xx509curve Curve,
	void* pScalar,
	size_t iScalarSize,
	xbytesview* pEncodedPublic
)
{
	xdercursor Root;
	xdercursor Fields;
	xdercursor Parameters;
	xdervalue Sequence;
	xdervalue Version;
	xdervalue AlgorithmValue;
	xdervalue CurveValue;
	xdervalue Private;
	xx509algorithm Algorithm;
	xbytesview PrivateDer;
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
		!__xrtTlsIdentityEcOid(
			Algorithm.Oid, __xrtTlsIdentityOidEc,
			sizeof(__xrtTlsIdentityOidEc)
		) || !Algorithm.HasParameters ||
		!xrtDerInit(
			&Parameters, Algorithm.Parameters.Data, Algorithm.Parameters.Size
		) || (xrtDerRead(&Parameters, &CurveValue) != XDER_VALUE) ||
		!xrtDerDone(&Parameters) ||
		!__xrtTlsIdentityEcCurve(&CurveValue, Curve) ||
		(xrtDerRead(&Fields, &Private) != XDER_VALUE) ||
		!xrtDerOctets(&Private, &PrivateDer) || !xrtDerDone(&Fields) ) {
		return __xrtTlsIdentityCause(
			"parse-tls-ec-key",
			"EC private key is not a canonical PKCS#8 key for the identity curve"
		);
	}
	return __xrtTlsIdentityEcSec1(
		PrivateDer, Curve, pScalar, iScalarSize, pEncodedPublic
	);
}



/* 按第二字段类型无歧义地区分 SEC1 与 PKCS#8。 */
bool __xrtTlsIdentityEcPrivate(
	xbytesview PrivateKey,
	xx509curve Curve,
	void* pScalar,
	size_t iScalarSize,
	xbytesview* pEncodedPublic
)
{
	xdercursor Root;
	xdercursor Fields;
	xdervalue Sequence;
	xdervalue Version;
	xdervalue Second;

	if ( (pScalar == NULL) || (pEncodedPublic == NULL) ||
		(PrivateKey.Data == NULL) || (PrivateKey.Size == 0) ||
		((Curve != X509_CURVE_P256) && (Curve != X509_CURVE_P384)) ||
		(((Curve == X509_CURVE_P256) && (iScalarSize != 32u)) ||
		 ((Curve == X509_CURVE_P384) && (iScalarSize != 48u))) ) {
		return __xrtTlsIdentityError(
			XERR_ARGUMENT, "parse-tls-ec-key",
			"EC private key input or curve is invalid"
		);
	}
	*pEncodedPublic = (xbytesview) { NULL, 0 };
	if ( PrivateKey.Size == iScalarSize ) {
		memcpy(pScalar, PrivateKey.Data, iScalarSize);
		return true;
	}
	if ( !xrtDerValidate(PrivateKey.Data, PrivateKey.Size) ||
		!xrtDerInit(&Root, PrivateKey.Data, PrivateKey.Size) ||
		(xrtDerRead(&Root, &Sequence) != XDER_VALUE) ||
		!xrtDerDone(&Root) || !xrtDerIs(
			&Sequence, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
		) || !xrtDerEnter(&Sequence, &Fields) ||
		(xrtDerRead(&Fields, &Version) != XDER_VALUE) ||
		(xrtDerRead(&Fields, &Second) != XDER_VALUE) ) {
		return __xrtTlsIdentityCause(
			"parse-tls-ec-key", "EC private key DER is malformed"
		);
	}
	if ( xrtDerIs(
		&Second, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
	) ) {
		return __xrtTlsIdentityEcPkcs8(
			PrivateKey, Curve, pScalar, iScalarSize, pEncodedPublic
		);
	}
	return __xrtTlsIdentityEcSec1(
		PrivateKey, Curve, pScalar, iScalarSize, pEncodedPublic
	);
}



/* TLS 1.2 把 ECDSA 线路值解释为摘要对，TLS 1.3 额外绑定曲线。 */
bool __xrtTlsIdentityEcSupports(
	xtlsidentitytype Type,
	xtlsversion Version,
	xtlssignature Signature
)
{
	if ( (Signature != XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256) &&
		(Signature != XTLS_SIGNATURE_ECDSA_SECP384R1_SHA384) &&
		(Signature != XTLS_SIGNATURE_ECDSA_SECP521R1_SHA512) ) {
		return false;
	}
	if ( Version == XTLS_VERSION_12 ) {
		return true;
	}
	return ((Type == XTLS_IDENTITY_ECDSA_P256) &&
		(Signature == XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256)) ||
		((Type == XTLS_IDENTITY_ECDSA_P384) &&
		 (Signature == XTLS_SIGNATURE_ECDSA_SECP384R1_SHA384)) ||
		((Type == XTLS_IDENTITY_ECDSA_P521) &&
		 (Signature == XTLS_SIGNATURE_ECDSA_SECP521R1_SHA512));
}



/* 按签名方案计算 ECDSA 摘要，并把算法显式交给 RFC 6979。 */
bool __xrtTlsIdentityEcHash(
	xtlssignature Signature,
	xbytesview Message,
	void* pHash,
	xcryptohash* pAlgorithm
)
{
	(void)Signature;
	if ( (pHash == NULL) || (pAlgorithm == NULL) ||
		((Message.Data == NULL) && (Message.Size != 0)) ) {
		return __xrtTlsIdentityError(
			XERR_ARGUMENT, "sign-tls-ec-identity",
			"ECDSA identity digest input is invalid"
		);
	}
	#if defined(XRT_FEATURE_CRYPTO_SHA256)
		if ( Signature == XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256 ) {
			*pAlgorithm = XCRYPTO_HASH_SHA256;
			return xrtSha256(Message.Data, Message.Size, pHash);
		}
	#endif

	#if defined(XRT_FEATURE_CRYPTO_SHA512)
		if ( Signature == XTLS_SIGNATURE_ECDSA_SECP384R1_SHA384 ) {
			*pAlgorithm = XCRYPTO_HASH_SHA384;
			return xrtSha384(Message.Data, Message.Size, pHash);
		}
		if ( Signature == XTLS_SIGNATURE_ECDSA_SECP521R1_SHA512 ) {
			*pAlgorithm = XCRYPTO_HASH_SHA512;
			return xrtSha512(Message.Data, Message.Size, pHash);
		}
	#endif

	return __xrtTlsIdentityError(
		XERR_UNSUPPORTED, "sign-tls-ec-identity",
		"ECDSA identity digest backend is unavailable"
	);
}

#endif
