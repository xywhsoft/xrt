#include "../internal/xrt_x509.h"



#if defined(XRT_FEATURE_X509_PARSE)

/* 判断一个字段是否是指定的上下文标签。 */
static bool __xrtX509Context(
	const xdervalue* pValue,
	uint32 iNumber,
	bool bConstructed
)
{
	return xrtDerIs(pValue, XASN1_CONTEXT, iNumber, bConstructed);
}



/* 解析可选显式版本并返回下一个 serialNumber 字段。 */
static bool __xrtX509Version(
	xdercursor* pFields,
	xdervalue* pField,
	xx509version* pVersion,
	const uint8* pBase,
	size_t iSize
)
{
	uint64 iVersion;

	*pVersion = X509_VERSION_1;
	if ( !__xrtX509Context(pField, 0u, true) ) {
		return true;
	}
	{
		xdercursor Explicit;
		xdervalue Integer;

		if ( !xrtDerEnter(pField, &Explicit) ||
			(xrtDerRead(&Explicit, &Integer) != XDER_VALUE) ||
			!xrtDerUInt64(&Integer, &iVersion) ||
			!xrtDerDone(&Explicit) || (iVersion == 0) || (iVersion > 2u) ) {
			__xrtX509Error(
				XERR_PROTOCOL, X509_ERROR_VERSION, "x509-parse",
				"explicit certificate version must be v2 or v3",
				__xrtX509Offset(pBase, iSize, pField->Raw.Data), NULL
			);
			return false;
		}
	}
	*pVersion = (xx509version)(iVersion + 1u);
	if ( xrtDerRead(pFields, pField) != XDER_VALUE ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_CERTIFICATE, "x509-parse",
			"TBSCertificate ends after its version", SIZE_MAX, NULL
		);
		return false;
	}
	return true;
}



/* 解析并验证证书 validity 序列。 */
static bool __xrtX509Validity(
	const xdervalue* pValue,
	xtime* pNotBefore,
	xtime* pNotAfter,
	const uint8* pBase,
	size_t iSize
)
{
	xdercursor Cursor;
	xdervalue Before;
	xdervalue After;

	if ( !xrtDerIs(
		pValue, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
	) || !xrtDerEnter(pValue, &Cursor) ||
		(xrtDerRead(&Cursor, &Before) != XDER_VALUE) ||
		(xrtDerRead(&Cursor, &After) != XDER_VALUE) ||
		!xrtDerDone(&Cursor) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_TIME, "x509-parse",
			"certificate validity must contain exactly two times",
			__xrtX509Offset(pBase, iSize, pValue->Raw.Data), NULL
		);
		return false;
	}
	if ( !__xrtX509TimeValue(
		&Before, pNotBefore, pBase, iSize, "x509-parse"
	) || !__xrtX509TimeValue(
		&After, pNotAfter, pBase, iSize, "x509-parse"
	) ) {
		return false;
	}
	if ( *pNotAfter < *pNotBefore ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_TIME, "x509-parse",
			"certificate notAfter precedes notBefore",
			__xrtX509Offset(pBase, iSize, pValue->Raw.Data), NULL
		);
		return false;
	}
	return true;
}



/* 验证隐式上下文 BIT STRING 并返回不含 unused-bit 字节的内容。 */
static bool __xrtX509UniqueId(
	const xdervalue* pValue,
	xbytesview* pId,
	uint8* pUnused,
	const uint8* pBase,
	size_t iSize
)
{
	const uint8* pData = pValue->Value.Data;
	size_t iLength = pValue->Value.Size;
	uint8 iUnused;

	if ( iLength == 0 ) {
		goto Invalid;
	}
	iUnused = pData[0];
	if ( (iUnused > 7u) || ((iLength == 1u) && (iUnused != 0)) ||
		((iUnused != 0) &&
		((pData[iLength - 1u] & ((UINT8_C(1) << iUnused) - 1u)) != 0)) ) {
		goto Invalid;
	}
	pId->Data = pData + 1u;
	pId->Size = iLength - 1u;
	*pUnused = iUnused;
	return true;

Invalid:
	__xrtX509Error(
		XERR_PROTOCOL, X509_ERROR_CERTIFICATE, "x509-parse",
		"certificate unique identifier is not a canonical BIT STRING",
		__xrtX509Offset(pBase, iSize, pValue->Raw.Data), NULL
	);
	return false;
}



/* 验证 Extensions 包装、重复 OID，并记录空 Subject 所需的 SAN。 */
static bool __xrtX509Extensions(
	const xdervalue* pValue,
	xbytesview* pExtensions,
	bool* pHasCriticalSan,
	const uint8* pBase,
	size_t iSize
)
{
	xdercursor Explicit;
	xdervalue Sequence;
	xdercursor Items;
	xdervalue Value;

	*pHasCriticalSan = false;
	if ( !__xrtX509Context(pValue, 3u, true) ||
		!xrtDerEnter(pValue, &Explicit) ||
		(xrtDerRead(&Explicit, &Sequence) != XDER_VALUE) ||
		!xrtDerDone(&Explicit) || !xrtDerIs(
			&Sequence, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
		) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_EXTENSION, "x509-parse",
			"extensions field must explicitly wrap one DER sequence",
			__xrtX509Offset(pBase, iSize, pValue->Raw.Data), NULL
		);
		return false;
	}
	if ( !__xrtX509ExtensionListValue(
		&Sequence, pExtensions, pBase, iSize, "x509-parse"
	) || !xrtDerEnter(&Sequence, &Items) ) {
		return false;
	}
	while ( xrtDerRead(&Items, &Value) == XDER_VALUE ) {
		xx509ext Extension;

		if ( !__xrtX509ExtensionValue(
			&Value, &Extension, pBase, iSize, "x509-parse"
		) ) {
			return false;
		}
		if ( (Extension.Oid.Size == __xrtX509OidSubjectAltNameSize) &&
			(memcmp(
				Extension.Oid.Data, __xrtX509OidSubjectAltName,
				__xrtX509OidSubjectAltNameSize
			) == 0) && Extension.Critical ) {
			xdercursor SanRoot;
			xdercursor Names;
			xdervalue San;

			if ( xrtDerInit(
				&SanRoot, Extension.Value.Data, Extension.Value.Size
			) && (xrtDerRead(&SanRoot, &San) == XDER_VALUE) &&
				xrtDerDone(&SanRoot) && xrtDerIs(
					&San, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
				) && xrtDerEnter(&San, &Names) && !xrtDerDone(&Names) ) {
				*pHasCriticalSan = true;
			}
		}
	}
	return xrtDerDone(&Items);
}



/* 解析 TBSCertificate 的全部固定和可选字段。 */
static bool __xrtX509Tbs(
	const xdervalue* pValue,
	xx509cert* pCert,
	const uint8* pBase,
	size_t iSize
)
{
	xdercursor Fields;
	xdervalue Field;
	xx509pubkey PublicKey;
	bool bSubjectEmpty;
	bool bHasCriticalSan = false;
	uint32 iLastOptional = 0;

	if ( !xrtDerIs(
		pValue, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
	) || !xrtDerEnter(pValue, &Fields) ||
		(xrtDerRead(&Fields, &Field) != XDER_VALUE) ||
		!__xrtX509Version(
			&Fields, &Field, &pCert->Version, pBase, iSize
		) || !__xrtX509SerialValue(
			&Field, &pCert->Serial, pBase, iSize, "x509-parse"
		) ) {
		return false;
	}

	if ( (xrtDerRead(&Fields, &Field) != XDER_VALUE) ||
		!__xrtX509AlgorithmValue(
			&Field, &pCert->TbsSignature, pBase, iSize, "x509-parse"
		) ) {
		return false;
	}
	if ( (xrtDerRead(&Fields, &Field) != XDER_VALUE) ||
		!__xrtX509NameValue(
			&Field, false, pBase, iSize, "x509-parse"
		) ) {
		return false;
	}
	pCert->Issuer = Field.Raw;

	if ( (xrtDerRead(&Fields, &Field) != XDER_VALUE) ||
		!__xrtX509Validity(
			&Field, &pCert->NotBefore, &pCert->NotAfter, pBase, iSize
		) ) {
		return false;
	}
	if ( (xrtDerRead(&Fields, &Field) != XDER_VALUE) ||
		!__xrtX509NameValue(
			&Field, true, pBase, iSize, "x509-parse"
		) ) {
		return false;
	}
	bSubjectEmpty = Field.Value.Size == 0;
	pCert->Subject = Field.Raw;

	if ( (xrtDerRead(&Fields, &Field) != XDER_VALUE) ||
		!__xrtX509PublicKeyValue(
			&Field, &PublicKey, pBase, iSize, "x509-parse"
		) ) {
		return false;
	}
	pCert->SubjectPublicKeyInfo = Field.Raw;

	while ( xrtDerRead(&Fields, &Field) == XDER_VALUE ) {
		if ( (Field.Tag.Class != XASN1_CONTEXT) ||
			(Field.Tag.Number < 1u) || (Field.Tag.Number > 3u) ||
			(Field.Tag.Number <= iLastOptional) ) {
			__xrtX509Error(
				XERR_PROTOCOL, X509_ERROR_CERTIFICATE, "x509-parse",
				"TBSCertificate optional fields are unknown or out of order",
				__xrtX509Offset(pBase, iSize, Field.Raw.Data), NULL
			);
			return false;
		}
		iLastOptional = Field.Tag.Number;
		if ( Field.Tag.Number == 1u ) {
			if ( Field.Tag.Constructed ||
				(pCert->Version == X509_VERSION_1) ) {
				__xrtX509Error(
					XERR_PROTOCOL, X509_ERROR_VERSION, "x509-parse",
					"issuerUniqueID requires v2 or v3 primitive encoding",
					__xrtX509Offset(pBase, iSize, Field.Raw.Data), NULL
				);
				return false;
			}
			if ( !__xrtX509UniqueId(
				&Field, &pCert->IssuerUniqueId,
				&pCert->IssuerUniqueIdUnusedBits, pBase, iSize
			) ) {
				return false;
			}
		} else if ( Field.Tag.Number == 2u ) {
			if ( Field.Tag.Constructed ||
				(pCert->Version == X509_VERSION_1) ) {
				__xrtX509Error(
					XERR_PROTOCOL, X509_ERROR_VERSION, "x509-parse",
					"subjectUniqueID requires v2 or v3 primitive encoding",
					__xrtX509Offset(pBase, iSize, Field.Raw.Data), NULL
				);
				return false;
			}
			if ( !__xrtX509UniqueId(
				&Field, &pCert->SubjectUniqueId,
				&pCert->SubjectUniqueIdUnusedBits, pBase, iSize
			) ) {
				return false;
			}
		} else {
			if ( !Field.Tag.Constructed ||
				(pCert->Version != X509_VERSION_3) ) {
				__xrtX509Error(
					XERR_PROTOCOL, X509_ERROR_VERSION, "x509-parse",
					"extensions require a v3 explicit encoding",
					__xrtX509Offset(pBase, iSize, Field.Raw.Data), NULL
				);
				return false;
			}
			if ( !__xrtX509Extensions(
				&Field, &pCert->Extensions, &bHasCriticalSan,
				pBase, iSize
			) ) {
				return false;
			}
		}
	}
	if ( !xrtDerDone(&Fields) ) {
		return false;
	}
	if ( bSubjectEmpty && !bHasCriticalSan ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_NAME, "x509-parse",
			"empty certificate subject requires a nonempty critical SAN",
			__xrtX509Offset(pBase, iSize, pCert->Subject.Data), NULL
		);
		return false;
	}
	pCert->Tbs = pValue->Raw;
	return true;
}



/* 严格解析一张完整 DER 证书；成功结果借用输入，失败时输出保持不变。 */
XRT_API bool xrtX509Parse(
	const void* pDer,
	size_t iSize,
	xx509cert* pCert
)
{
	const uint8* pData = (const uint8*)pDer;
	xdercursor Root;
	xdercursor CertificateFields;
	xdervalue CertificateValue;
	xdervalue Tbs;
	xdervalue Algorithm;
	xdervalue Signature;
	xx509cert Cert;
	uint8 iUnused;
	const xerror* pCause;

	if ( (pCert == NULL) || ((pDer == NULL) && (iSize != 0)) ||
		(iSize == 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtDerValidate(pDer, iSize) ) {
		pCause = xrtGetError();
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_DER, "x509-parse",
			"certificate is not one complete canonical DER value",
			SIZE_MAX, pCause
		);
		return false;
	}
	if ( !xrtDerInit(&Root, pDer, iSize) ||
		(xrtDerRead(&Root, &CertificateValue) != XDER_VALUE) ||
		!xrtDerDone(&Root) || !xrtDerIs(
			&CertificateValue, XASN1_UNIVERSAL,
			(uint32)XASN1_SEQUENCE, true
		) || !xrtDerEnter(&CertificateValue, &CertificateFields) ||
		(xrtDerRead(&CertificateFields, &Tbs) != XDER_VALUE) ||
		(xrtDerRead(&CertificateFields, &Algorithm) != XDER_VALUE) ||
		(xrtDerRead(&CertificateFields, &Signature) != XDER_VALUE) ||
		!xrtDerDone(&CertificateFields) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_CERTIFICATE, "x509-parse",
			"Certificate must contain exactly TBS, algorithm and signature",
			0, NULL
		);
		return false;
	}
	memset(&Cert, 0, sizeof(Cert));
	Cert.Raw = CertificateValue.Raw;
	if ( !__xrtX509Tbs(&Tbs, &Cert, pData, iSize) ||
		!__xrtX509AlgorithmValue(
			&Algorithm, &Cert.SignatureAlgorithm,
			pData, iSize, "x509-parse"
		) ) {
		return false;
	}
	if ( (Cert.TbsSignature.Raw.Size != Cert.SignatureAlgorithm.Raw.Size) ||
		(memcmp(
			Cert.TbsSignature.Raw.Data, Cert.SignatureAlgorithm.Raw.Data,
			Cert.TbsSignature.Raw.Size
		) != 0) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_ALGORITHM, "x509-parse",
			"TBS and outer signature AlgorithmIdentifier differ",
			__xrtX509Offset(pData, iSize, Algorithm.Raw.Data), NULL
		);
		return false;
	}
	if ( !xrtDerBitString(&Signature, &Cert.Signature, &iUnused) ||
		(iUnused != 0) || (Cert.Signature.Size == 0) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_SIGNATURE, "x509-parse",
			"certificate signature must be a nonempty octet-aligned BIT STRING",
			__xrtX509Offset(pData, iSize, Signature.Raw.Data), NULL
		);
		return false;
	}
	*pCert = Cert;
	return true;
}



/* 判断指定绝对时间是否位于证书闭区间内。 */
XRT_API bool xrtX509ValidAt(const xx509cert* pCert, xtime iTime)
{
	if ( pCert == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return (iTime >= pCert->NotBefore) && (iTime <= pCert->NotAfter);
}

#endif
