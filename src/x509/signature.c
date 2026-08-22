#include "../internal/xrt_x509.h"



#if defined(XRT_FEATURE_X509_SIGNATURE)

static const uint8 __xrtX509OidSha1[] = {
	0x2B, 0x0E, 0x03, 0x02, 0x1A
};
static const uint8 __xrtX509OidSha224[] = {
	0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x04
};
static const uint8 __xrtX509OidSha256[] = {
	0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01
};
static const uint8 __xrtX509OidSha384[] = {
	0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02
};
static const uint8 __xrtX509OidSha512[] = {
	0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03
};
static const uint8 __xrtX509OidMgf1[] = {
	0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x08
};
static const uint8 __xrtX509OidRsaSha1[] = {
	0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x05
};
static const uint8 __xrtX509OidRsaSha224[] = {
	0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x0E
};
static const uint8 __xrtX509OidRsaSha256[] = {
	0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x0B
};
static const uint8 __xrtX509OidRsaSha384[] = {
	0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x0C
};
static const uint8 __xrtX509OidRsaSha512[] = {
	0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x0D
};
static const uint8 __xrtX509OidEcdsaSha1[] = {
	0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x01
};
static const uint8 __xrtX509OidEcdsaSha224[] = {
	0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x01
};
static const uint8 __xrtX509OidEcdsaSha256[] = {
	0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x02
};
static const uint8 __xrtX509OidEcdsaSha384[] = {
	0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x03
};
static const uint8 __xrtX509OidEcdsaSha512[] = {
	0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x04
};



/* 判断借用 OID 是否与静态内容八位组完全相同。 */
static bool __xrtX509SignatureOid(
	xbytesview Oid,
	const void* pExpected,
	size_t iExpectedSize
)
{
	return (Oid.Size == iExpectedSize) &&
		(memcmp(Oid.Data, pExpected, iExpectedSize) == 0);
}



/* 设置统一的签名算法格式错误并保留底层 DER 原因。 */
static xx509result __xrtX509SignatureError(
	cstr sMessage,
	const xerror* pCause
)
{
	__xrtX509Error(
		XERR_PROTOCOL, X509_ERROR_SIGNATURE_ALGORITHM,
		"x509-signature-parse", sMessage, SIZE_MAX, pCause
	);
	return X509_ERROR;
}



/* 把 RFC 4055 摘要 OID 映射为协议枚举。 */
static bool __xrtX509SignatureHash(
	xbytesview Oid,
	xx509hash* pHash
)
{
	if ( __xrtX509SignatureOid(
		Oid, __xrtX509OidSha1, sizeof(__xrtX509OidSha1)
	) ) {
		*pHash = X509_HASH_SHA1;
	} else if ( __xrtX509SignatureOid(
		Oid, __xrtX509OidSha224, sizeof(__xrtX509OidSha224)
	) ) {
		*pHash = X509_HASH_SHA224;
	} else if ( __xrtX509SignatureOid(
		Oid, __xrtX509OidSha256, sizeof(__xrtX509OidSha256)
	) ) {
		*pHash = X509_HASH_SHA256;
	} else if ( __xrtX509SignatureOid(
		Oid, __xrtX509OidSha384, sizeof(__xrtX509OidSha384)
	) ) {
		*pHash = X509_HASH_SHA384;
	} else if ( __xrtX509SignatureOid(
		Oid, __xrtX509OidSha512, sizeof(__xrtX509OidSha512)
	) ) {
		*pHash = X509_HASH_SHA512;
	} else {
		return false;
	}
	return true;
}



/* 返回完整参数 TLV 中唯一的 DER 值。 */
static bool __xrtX509SignatureParameter(
	xbytesview Der,
	xdervalue* pValue
)
{
	xdercursor Cursor;

	return xrtDerValidate(Der.Data, Der.Size) &&
		xrtDerInit(&Cursor, Der.Data, Der.Size) &&
		(xrtDerRead(&Cursor, pValue) == XDER_VALUE) && xrtDerDone(&Cursor);
}



/* RSA PKCS#1 摘要参数允许规范 NULL 或历史上广泛使用的缺省形式。 */
static bool __xrtX509SignatureNullOrAbsent(
	const xx509algorithm* pAlgorithm
)
{
	xdervalue Value;

	if ( !pAlgorithm->HasParameters ) {
		return true;
	}
	return __xrtX509SignatureParameter(pAlgorithm->Parameters, &Value) &&
		xrtDerIs(&Value, XASN1_UNIVERSAL, (uint32)XASN1_NULL, false) &&
		(Value.Value.Size == 0);
}



/* 从一个完整 HashAlgorithm AlgorithmIdentifier 读取受支持摘要。 */
static bool __xrtX509SignatureHashAlgorithm(
	xbytesview Der,
	xx509hash* pHash
)
{
	xx509algorithm Algorithm;

	return xrtX509AlgorithmParse(Der, &Algorithm) &&
		__xrtX509SignatureHash(Algorithm.Oid, pHash) &&
		__xrtX509SignatureNullOrAbsent(&Algorithm);
}



/* 从显式上下文字段中读取唯一内层 DER 值。 */
static bool __xrtX509SignatureExplicit(
	const xdervalue* pField,
	xdervalue* pValue
)
{
	xdercursor Cursor;

	return pField->Tag.Constructed && xrtDerEnter(pField, &Cursor) &&
		(xrtDerRead(&Cursor, pValue) == XDER_VALUE) && xrtDerDone(&Cursor);
}



/* 读取一个可装入 size_t 的非负显式 INTEGER。 */
static bool __xrtX509SignatureInteger(
	const xdervalue* pField,
	size_t* pInteger
)
{
	xdervalue Value;
	xbytesview Bytes;
	size_t iInteger = 0;

	if ( !__xrtX509SignatureExplicit(pField, &Value) ||
		!xrtDerUnsigned(&Value, &Bytes) || (Bytes.Size > sizeof(size_t)) ) {
		return false;
	}
	for ( size_t i = 0; i < Bytes.Size; i++ ) {
		iInteger = (iInteger << 8u) | (size_t)Bytes.Data[i];
	}
	*pInteger = iInteger;
	return true;
}



/* 解析一个显式 HashAlgorithm 字段。 */
static bool __xrtX509SignaturePssHash(
	const xdervalue* pField,
	xx509hash* pHash
)
{
	xdervalue Value;

	return __xrtX509SignatureExplicit(pField, &Value) &&
		__xrtX509SignatureHashAlgorithm(Value.Raw, pHash);
}



/* 解析一个显式 MGF1 AlgorithmIdentifier 及其内层摘要参数。 */
static bool __xrtX509SignaturePssMask(
	const xdervalue* pField,
	xx509hash* pHash
)
{
	xdervalue Value;
	xx509algorithm Algorithm;

	return __xrtX509SignatureExplicit(pField, &Value) &&
		xrtX509AlgorithmParse(Value.Raw, &Algorithm) &&
		__xrtX509SignatureOid(
			Algorithm.Oid, __xrtX509OidMgf1, sizeof(__xrtX509OidMgf1)
		) && Algorithm.HasParameters &&
		__xrtX509SignatureHashAlgorithm(Algorithm.Parameters, pHash);
}



/* 严格解析 RFC 4055 RSASSA-PSS-params，并接受规范要求兼容的显式默认值。 */
static bool __xrtX509SignaturePss(
	const xx509algorithm* pAlgorithm,
	xx509signature* pSignature
)
{
	xdervalue Sequence;
	xdercursor Fields;
	xdervalue Field;
	int iPrevious = -1;
	xx509signature Signature;

	memset(&Signature, 0, sizeof(Signature));
	Signature.Type = X509_SIGNATURE_RSA_PSS;
	Signature.Hash = X509_HASH_SHA1;
	Signature.MaskHash = X509_HASH_SHA1;
	Signature.SaltSize = 20u;
	Signature.Trailer = 1u;
	if ( !pAlgorithm->HasParameters ||
		!__xrtX509SignatureParameter(pAlgorithm->Parameters, &Sequence) ||
		!xrtDerIs(
			&Sequence, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
		) || !xrtDerEnter(&Sequence, &Fields) ) {
		return false;
	}
	while ( xrtDerRead(&Fields, &Field) == XDER_VALUE ) {
		size_t iInteger;
		int iTag;

		if ( (Field.Tag.Class != XASN1_CONTEXT) || !Field.Tag.Constructed ||
			(Field.Tag.Number > 3u) ) {
			return false;
		}
		iTag = (int)Field.Tag.Number;
		if ( iTag <= iPrevious ) {
			return false;
		}
		iPrevious = iTag;
		if ( iTag == 0 ) {
			if ( !__xrtX509SignaturePssHash(&Field, &Signature.Hash) ) {
				return false;
			}
		} else if ( iTag == 1 ) {
			if ( !__xrtX509SignaturePssMask(&Field, &Signature.MaskHash) ) {
				return false;
			}
		} else if ( iTag == 2 ) {
			if ( !__xrtX509SignatureInteger(&Field, &Signature.SaltSize) ) {
				return false;
			}
		} else if ( !__xrtX509SignatureInteger(&Field, &iInteger) ||
			(iInteger != 1u) ) {
			return false;
		}
	}
	if ( !xrtDerDone(&Fields) ) {
		return false;
	}
	*pSignature = Signature;
	return true;
}



/* 匹配固定 OID 列表并返回其中携带的摘要。 */
static bool __xrtX509SignatureScheme(
	xbytesview Oid,
	const uint8* const* pOids,
	const size_t* pSizes,
	xx509hash* pHash
)
{
	static const xx509hash Hashes[] = {
		X509_HASH_SHA1,
		X509_HASH_SHA224,
		X509_HASH_SHA256,
		X509_HASH_SHA384,
		X509_HASH_SHA512
	};

	for ( size_t i = 0; i < sizeof(Hashes) / sizeof(Hashes[0]); i++ ) {
		if ( __xrtX509SignatureOid(Oid, pOids[i], pSizes[i]) ) {
			*pHash = Hashes[i];
			return true;
		}
	}
	return false;
}



/* 解析已知证书签名算法并保持未知算法可由上层扩展。 */
XRT_API xx509result xrtX509SignatureParse(
	const xx509algorithm* pAlgorithm,
	xx509signature* pSignature
)
{
	static const uint8* const RsaOids[] = {
		__xrtX509OidRsaSha1,
		__xrtX509OidRsaSha224,
		__xrtX509OidRsaSha256,
		__xrtX509OidRsaSha384,
		__xrtX509OidRsaSha512
	};
	static const size_t RsaSizes[] = {
		sizeof(__xrtX509OidRsaSha1),
		sizeof(__xrtX509OidRsaSha224),
		sizeof(__xrtX509OidRsaSha256),
		sizeof(__xrtX509OidRsaSha384),
		sizeof(__xrtX509OidRsaSha512)
	};
	static const uint8* const EcdsaOids[] = {
		__xrtX509OidEcdsaSha1,
		__xrtX509OidEcdsaSha224,
		__xrtX509OidEcdsaSha256,
		__xrtX509OidEcdsaSha384,
		__xrtX509OidEcdsaSha512
	};
	static const size_t EcdsaSizes[] = {
		sizeof(__xrtX509OidEcdsaSha1),
		sizeof(__xrtX509OidEcdsaSha224),
		sizeof(__xrtX509OidEcdsaSha256),
		sizeof(__xrtX509OidEcdsaSha384),
		sizeof(__xrtX509OidEcdsaSha512)
	};
	xx509signature Signature;

	if ( (pAlgorithm == NULL) || (pSignature == NULL) ||
		(pAlgorithm->Oid.Data == NULL) || (pAlgorithm->Oid.Size == 0) ||
		(pAlgorithm->HasParameters ?
		 ((pAlgorithm->Parameters.Data == NULL) ||
		  (pAlgorithm->Parameters.Size == 0)) :
		 ((pAlgorithm->Parameters.Data != NULL) ||
		  (pAlgorithm->Parameters.Size != 0))) ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	memset(&Signature, 0, sizeof(Signature));
	if ( __xrtX509SignatureScheme(
		pAlgorithm->Oid, RsaOids, RsaSizes, &Signature.Hash
	) ) {
		if ( !__xrtX509SignatureNullOrAbsent(pAlgorithm) ) {
			return __xrtX509SignatureError(
				"RSA PKCS#1 signature parameters must be NULL or absent",
				NULL
			);
		}
		Signature.Type = X509_SIGNATURE_RSA_PKCS1;
	} else if ( __xrtX509SignatureOid(
		pAlgorithm->Oid, __xrtX509OidRsaPss, __xrtX509OidRsaPssSize
	) ) {
		if ( !__xrtX509SignaturePss(pAlgorithm, &Signature) ) {
			return __xrtX509SignatureError(
				"RSASSA-PSS parameters are missing or malformed", NULL
			);
		}
	} else if ( __xrtX509SignatureScheme(
		pAlgorithm->Oid, EcdsaOids, EcdsaSizes, &Signature.Hash
	) ) {
		if ( pAlgorithm->HasParameters ) {
			return __xrtX509SignatureError(
				"ECDSA signature parameters must be absent", NULL
			);
		}
		Signature.Type = X509_SIGNATURE_ECDSA;
	} else if ( __xrtX509SignatureOid(
		pAlgorithm->Oid, __xrtX509OidEd25519, __xrtX509OidEd25519Size
	) ) {
		if ( pAlgorithm->HasParameters ) {
			return __xrtX509SignatureError(
				"Ed25519 signature parameters must be absent", NULL
			);
		}
		Signature.Type = X509_SIGNATURE_ED25519;
	} else if ( __xrtX509SignatureOid(
		pAlgorithm->Oid, __xrtX509OidEd448, __xrtX509OidEd448Size
	) ) {
		if ( pAlgorithm->HasParameters ) {
			return __xrtX509SignatureError(
				"Ed448 signature parameters must be absent", NULL
			);
		}
		Signature.Type = X509_SIGNATURE_ED448;
	} else {
		return X509_DONE;
	}
	*pSignature = Signature;
	return X509_VALUE;
}

#endif
