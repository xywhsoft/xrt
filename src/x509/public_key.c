#include "../internal/xrt_x509.h"



#if defined(XRT_FEATURE_X509_PARSE)

/* 判断借用 OID 是否与静态内容八位组完全相同。 */
static bool __xrtX509KeyOidEqual(
	xbytesview Oid,
	const void* pExpected,
	size_t iExpectedSize
)
{
	return (Oid.Size == iExpectedSize) &&
		(memcmp(Oid.Data, pExpected, iExpectedSize) == 0);
}



/* 读取 AlgorithmIdentifier 的完整参数 TLV。 */
static bool __xrtX509KeyParameter(
	const xx509algorithm* pAlgorithm,
	xdervalue* pValue
)
{
	xdercursor Cursor;

	return pAlgorithm->HasParameters &&
		xrtDerInit(
			&Cursor, pAlgorithm->Parameters.Data, pAlgorithm->Parameters.Size
		) && (xrtDerRead(&Cursor, pValue) == XDER_VALUE) &&
		xrtDerDone(&Cursor);
}



/* 判断 rsaEncryption 参数是否为规范 NULL。 */
static bool __xrtX509RsaParameters(const xx509algorithm* pAlgorithm)
{
	xdervalue Value;

	return __xrtX509KeyParameter(pAlgorithm, &Value) &&
		xrtDerIs(
			&Value, XASN1_UNIVERSAL, (uint32)XASN1_NULL, false
		) && (Value.Value.Size == 0);
}



/* 解析 RSA SubjectPublicKeyInfo 中的模数和指数。 */
static bool __xrtX509RsaPublicKey(
	xbytesview Key,
	xx509pubkey* pPublicKey,
	const uint8* pBase,
	size_t iSize,
	cstr sOperation
)
{
	xdercursor Root;
	xdercursor Fields;
	xdervalue Sequence;
	xdervalue Modulus;
	xdervalue Exponent;
	xbytesview ModulusBytes;
	xbytesview ExponentBytes;

	if ( !xrtDerValidate(Key.Data, Key.Size) ||
		!xrtDerInit(&Root, Key.Data, Key.Size) ||
		(xrtDerRead(&Root, &Sequence) != XDER_VALUE) ||
		!xrtDerDone(&Root) || !xrtDerIs(
			&Sequence, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
		) || !xrtDerEnter(&Sequence, &Fields) ||
		(xrtDerRead(&Fields, &Modulus) != XDER_VALUE) ||
		!xrtDerUnsigned(&Modulus, &ModulusBytes) ||
		(xrtDerRead(&Fields, &Exponent) != XDER_VALUE) ||
		!xrtDerUnsigned(&Exponent, &ExponentBytes) ||
		!xrtDerDone(&Fields) || (ModulusBytes.Size == 0) ||
		(ExponentBytes.Size == 0) ||
		((ModulusBytes.Size == 1u) && (ModulusBytes.Data[0] == 0)) ||
		((ExponentBytes.Size == 1u) && (ExponentBytes.Data[0] == 0)) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_PUBLIC_KEY, sOperation,
			"RSA subjectPublicKey must contain positive modulus and exponent",
			__xrtX509Offset(pBase, iSize, Key.Data), NULL
		);
		return false;
	}
	pPublicKey->Modulus = ModulusBytes;
	pPublicKey->Exponent = ExponentBytes;
	return true;
}



/* 解析 EC 命名曲线参数并验证已知曲线的未压缩点长度。 */
static bool __xrtX509EcPublicKey(
	const xx509algorithm* pAlgorithm,
	xbytesview Key,
	xx509pubkey* pPublicKey,
	const uint8* pBase,
	size_t iSize,
	cstr sOperation
)
{
	xdervalue Curve;
	xbytesview Oid;
	size_t iExpected = 0;

	if ( !__xrtX509KeyParameter(pAlgorithm, &Curve) ||
		!xrtDerOid(&Curve, &Oid) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_PUBLIC_KEY, sOperation,
			"EC SubjectPublicKeyInfo requires named-curve OID parameters",
			__xrtX509Offset(pBase, iSize, pAlgorithm->Raw.Data), NULL
		);
		return false;
	}
	if ( __xrtX509KeyOidEqual(Oid, __xrtX509OidP256, __xrtX509OidP256Size) ) {
		pPublicKey->Curve = X509_CURVE_P256;
		iExpected = 65u;
	} else if ( __xrtX509KeyOidEqual(
		Oid, __xrtX509OidP384, __xrtX509OidP384Size
	) ) {
		pPublicKey->Curve = X509_CURVE_P384;
		iExpected = 97u;
	} else if ( __xrtX509KeyOidEqual(
		Oid, __xrtX509OidP521, __xrtX509OidP521Size
	) ) {
		pPublicKey->Curve = X509_CURVE_P521;
		iExpected = 133u;
	}
	if ( (iExpected != 0) &&
		((Key.Size != iExpected) || (Key.Data[0] != UINT8_C(0x04))) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_PUBLIC_KEY, sOperation,
			"known NIST curve public key has an invalid point encoding",
			__xrtX509Offset(pBase, iSize, Key.Data), NULL
		);
		return false;
	}
	return true;
}



/* 验证 RFC 8410 公钥必须省略参数并使用固定字节数。 */
static bool __xrtX509OkpPublicKey(
	const xx509algorithm* pAlgorithm,
	xbytesview Key,
	size_t iExpected,
	const uint8* pBase,
	size_t iSize,
	cstr sOperation
)
{
	if ( pAlgorithm->HasParameters || (Key.Size != iExpected) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_PUBLIC_KEY, sOperation,
			"RFC 8410 public key has parameters or an invalid key size",
			__xrtX509Offset(pBase, iSize, pAlgorithm->Raw.Data), NULL
		);
		return false;
	}
	return true;
}



/* 验证并发布一个 SubjectPublicKeyInfo。 */
bool __xrtX509PublicKeyValue(
	const xdervalue* pValue,
	xx509pubkey* pPublicKey,
	const uint8* pBase,
	size_t iSize,
	cstr sOperation
)
{
	xdercursor Cursor;
	xdervalue AlgorithmValue;
	xdervalue KeyValue;
	xx509pubkey PublicKey;
	uint8 iUnused;

	if ( (pValue == NULL) || (pPublicKey == NULL) ||
		!xrtDerIs(
			pValue, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
		) || !xrtDerEnter(pValue, &Cursor) ||
		(xrtDerRead(&Cursor, &AlgorithmValue) != XDER_VALUE) ||
		(xrtDerRead(&Cursor, &KeyValue) != XDER_VALUE) ||
		!xrtDerDone(&Cursor) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_PUBLIC_KEY, sOperation,
			"SubjectPublicKeyInfo must contain exactly algorithm and key",
			pValue != NULL ? __xrtX509Offset(pBase, iSize, pValue->Raw.Data) :
				SIZE_MAX,
			NULL
		);
		return false;
	}
	memset(&PublicKey, 0, sizeof(PublicKey));
	if ( !__xrtX509AlgorithmValue(
		&AlgorithmValue, &PublicKey.Algorithm, pBase, iSize, sOperation
	) || !xrtDerBitString(&KeyValue, &PublicKey.Key, &iUnused) ||
		(iUnused != 0) || (PublicKey.Key.Size == 0) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_PUBLIC_KEY, sOperation,
			"subjectPublicKey must be a nonempty octet-aligned BIT STRING",
			__xrtX509Offset(pBase, iSize, KeyValue.Raw.Data), NULL
		);
		return false;
	}

	if ( __xrtX509KeyOidEqual(
		PublicKey.Algorithm.Oid, __xrtX509OidRsa, __xrtX509OidRsaSize
	) ) {
		PublicKey.Type = X509_KEY_RSA;
		if ( !__xrtX509RsaParameters(&PublicKey.Algorithm) ||
			!__xrtX509RsaPublicKey(
				PublicKey.Key, &PublicKey, pBase, iSize, sOperation
			) ) {
			return false;
		}
	} else if ( __xrtX509KeyOidEqual(
		PublicKey.Algorithm.Oid,
		__xrtX509OidRsaPss, __xrtX509OidRsaPssSize
	) ) {
		PublicKey.Type = X509_KEY_RSA_PSS;
		if ( !__xrtX509RsaPublicKey(
			PublicKey.Key, &PublicKey, pBase, iSize, sOperation
		) ) {
			return false;
		}
	} else if ( __xrtX509KeyOidEqual(
		PublicKey.Algorithm.Oid, __xrtX509OidEc, __xrtX509OidEcSize
	) ) {
		PublicKey.Type = X509_KEY_EC;
		if ( !__xrtX509EcPublicKey(
			&PublicKey.Algorithm, PublicKey.Key, &PublicKey,
			pBase, iSize, sOperation
		) ) {
			return false;
		}
	} else if ( __xrtX509KeyOidEqual(
		PublicKey.Algorithm.Oid,
		__xrtX509OidEd25519, __xrtX509OidEd25519Size
	) ) {
		PublicKey.Type = X509_KEY_ED25519;
		if ( !__xrtX509OkpPublicKey(
			&PublicKey.Algorithm, PublicKey.Key, 32u,
			pBase, iSize, sOperation
		) ) {
			return false;
		}
	} else if ( __xrtX509KeyOidEqual(
		PublicKey.Algorithm.Oid, __xrtX509OidEd448, __xrtX509OidEd448Size
	) ) {
		PublicKey.Type = X509_KEY_ED448;
		if ( !__xrtX509OkpPublicKey(
			&PublicKey.Algorithm, PublicKey.Key, 57u,
			pBase, iSize, sOperation
		) ) {
			return false;
		}
	} else if ( __xrtX509KeyOidEqual(
		PublicKey.Algorithm.Oid,
		__xrtX509OidX25519, __xrtX509OidX25519Size
	) ) {
		PublicKey.Type = X509_KEY_X25519;
		if ( !__xrtX509OkpPublicKey(
			&PublicKey.Algorithm, PublicKey.Key, 32u,
			pBase, iSize, sOperation
		) ) {
			return false;
		}
	} else if ( __xrtX509KeyOidEqual(
		PublicKey.Algorithm.Oid, __xrtX509OidX448, __xrtX509OidX448Size
	) ) {
		PublicKey.Type = X509_KEY_X448;
		if ( !__xrtX509OkpPublicKey(
			&PublicKey.Algorithm, PublicKey.Key, 56u,
			pBase, iSize, sOperation
		) ) {
			return false;
		}
	}
	*pPublicKey = PublicKey;
	return true;
}



/* 解析 SubjectPublicKeyInfo，并返回支持未来算法的通用借用视图。 */
XRT_API bool xrtX509PublicKey(
	const xx509cert* pCert,
	xx509pubkey* pPublicKey
)
{
	xdercursor Cursor;
	xdervalue Value;

	if ( (pCert == NULL) || (pPublicKey == NULL) ||
		(pCert->SubjectPublicKeyInfo.Data == NULL) ||
		(pCert->SubjectPublicKeyInfo.Size == 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtDerInit(
		&Cursor, pCert->SubjectPublicKeyInfo.Data,
		pCert->SubjectPublicKeyInfo.Size
	) || (xrtDerRead(&Cursor, &Value) != XDER_VALUE) ||
		!xrtDerDone(&Cursor) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_PUBLIC_KEY, "x509-public-key",
			"certificate SubjectPublicKeyInfo view is malformed",
			SIZE_MAX, NULL
		);
		return false;
	}
	return __xrtX509PublicKeyValue(
		&Value, pPublicKey,
		pCert->SubjectPublicKeyInfo.Data,
		pCert->SubjectPublicKeyInfo.Size,
		"x509-public-key"
	);
}

#endif
