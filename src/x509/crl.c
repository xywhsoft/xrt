#include "../internal/xrt_x509.h"



#if defined(XRT_FEATURE_X509_CRL)

/* 判断字段是否使用 RFC 5280 允许的时间标签。 */
static bool __xrtX509CrlIsTime(const xdervalue* pValue)
{
	return xrtDerIs(
		pValue, XASN1_UNIVERSAL, (uint32)XASN1_UTC_TIME, false
	) || xrtDerIs(
		pValue, XASN1_UNIVERSAL, (uint32)XASN1_GENERALIZED_TIME, false
	);
}



/* 解析一项已经通过整体 DER 校验的 revokedCertificates 条目。 */
static bool __xrtX509CrlEntryValue(
	const xdervalue* pValue,
	xx509crlversion Version,
	xx509crlentry* pEntry,
	const uint8* pBase,
	size_t iSize,
	cstr sOperation
)
{
	xdercursor Fields;
	xdervalue Serial;
	xdervalue Time;
	xdervalue Extensions;
	xderresult Result;
	xx509crlentry Entry;

	if ( (pValue == NULL) || (pEntry == NULL) || !xrtDerIs(
		pValue, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
	) || !xrtDerEnter(pValue, &Fields) ||
		(xrtDerRead(&Fields, &Serial) != XDER_VALUE) ||
		(xrtDerRead(&Fields, &Time) != XDER_VALUE) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_CRL_ENTRY, sOperation,
			"CRL entry must contain a serial number and revocation time",
			pValue != NULL ?
				__xrtX509Offset(pBase, iSize, pValue->Raw.Data) : SIZE_MAX,
			NULL
		);
		return false;
	}
	memset(&Entry, 0, sizeof(Entry));
	if ( !__xrtX509SerialValue(
		&Serial, &Entry.Serial, pBase, iSize, sOperation
	) || !__xrtX509TimeValue(
		&Time, &Entry.RevokedAt, pBase, iSize, sOperation
	) ) {
		return false;
	}
	Result = xrtDerRead(&Fields, &Extensions);
	if ( Result == XDER_ERROR ) {
		return false;
	}
	if ( Result == XDER_VALUE ) {
		if ( Version != X509_CRL_VERSION_2 ) {
			__xrtX509Error(
				XERR_PROTOCOL, X509_ERROR_VERSION, sOperation,
				"CRL entry extensions require a v2 CRL",
				__xrtX509Offset(pBase, iSize, Extensions.Raw.Data), NULL
			);
			return false;
		}
		if ( !__xrtX509ExtensionListValue(
			&Extensions, &Entry.Extensions, pBase, iSize, sOperation
		) ) {
			return false;
		}
	}
	if ( !xrtDerDone(&Fields) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_CRL_ENTRY, sOperation,
			"CRL entry contains trailing fields",
			__xrtX509Offset(pBase, iSize, pValue->Raw.Data), NULL
		);
		return false;
	}
	Entry.Raw = pValue->Raw;
	*pEntry = Entry;
	return true;
}



/* 验证非空 revokedCertificates 序列中的全部条目。 */
static bool __xrtX509CrlRevokedValue(
	const xdervalue* pValue,
	xx509crlversion Version,
	xbytesview* pRevoked,
	const uint8* pBase,
	size_t iSize,
	cstr sOperation
)
{
	xdercursor Items;
	xdervalue Value;
	size_t iCount = 0;

	if ( (pValue == NULL) || (pRevoked == NULL) || !xrtDerIs(
		pValue, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
	) || !xrtDerEnter(pValue, &Items) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_CRL_ENTRY, sOperation,
			"revokedCertificates must be a DER sequence",
			pValue != NULL ?
				__xrtX509Offset(pBase, iSize, pValue->Raw.Data) : SIZE_MAX,
			NULL
		);
		return false;
	}
	while ( xrtDerRead(&Items, &Value) == XDER_VALUE ) {
		xx509crlentry Entry;

		if ( !__xrtX509CrlEntryValue(
			&Value, Version, &Entry, pBase, iSize, sOperation
		) ) {
			return false;
		}
		iCount++;
	}
	if ( !xrtDerDone(&Items) || (iCount == 0) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_CRL_ENTRY, sOperation,
			"an encoded revokedCertificates list must not be empty",
			__xrtX509Offset(pBase, iSize, pValue->Raw.Data), NULL
		);
		return false;
	}
	*pRevoked = pValue->Raw;
	return true;
}



/* 解析 v2 CRL 的显式扩展包装。 */
static bool __xrtX509CrlExtensions(
	const xdervalue* pValue,
	xx509crlversion Version,
	xbytesview* pExtensions,
	const uint8* pBase,
	size_t iSize
)
{
	xdercursor Explicit;
	xdervalue Sequence;

	if ( Version != X509_CRL_VERSION_2 ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_VERSION, "x509-crl-parse",
			"CRL extensions require a v2 CRL",
			__xrtX509Offset(pBase, iSize, pValue->Raw.Data), NULL
		);
		return false;
	}
	if ( !xrtDerIs(pValue, XASN1_CONTEXT, 0u, true) ||
		!xrtDerEnter(pValue, &Explicit) ||
		(xrtDerRead(&Explicit, &Sequence) != XDER_VALUE) ||
		!xrtDerDone(&Explicit) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_EXTENSION, "x509-crl-parse",
			"CRL extensions must explicitly wrap one Extensions sequence",
			__xrtX509Offset(pBase, iSize, pValue->Raw.Data), NULL
		);
		return false;
	}
	return __xrtX509ExtensionListValue(
		&Sequence, pExtensions, pBase, iSize, "x509-crl-parse"
	);
}



/* 解析 TBSCertList 的固定字段和三个可选尾字段。 */
static bool __xrtX509CrlTbs(
	const xdervalue* pValue,
	xx509crl* pCrl,
	const uint8* pBase,
	size_t iSize
)
{
	xdercursor Fields;
	xdervalue Field;
	xderresult Result;
	uint64 iVersion;

	if ( !xrtDerIs(
		pValue, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
	) || !xrtDerEnter(pValue, &Fields) ||
		(xrtDerRead(&Fields, &Field) != XDER_VALUE) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_CRL, "x509-crl-parse",
			"TBSCertList is not a nonempty DER sequence",
			__xrtX509Offset(pBase, iSize, pValue->Raw.Data), NULL
		);
		return false;
	}
	pCrl->Version = X509_CRL_VERSION_1;
	if ( xrtDerIs(
		&Field, XASN1_UNIVERSAL, (uint32)XASN1_INTEGER, false
	) ) {
		if ( !xrtDerUInt64(&Field, &iVersion) || (iVersion != 1u) ) {
			__xrtX509Error(
				XERR_PROTOCOL, X509_ERROR_VERSION, "x509-crl-parse",
				"an encoded CRL version must be v2",
				__xrtX509Offset(pBase, iSize, Field.Raw.Data), NULL
			);
			return false;
		}
		pCrl->Version = X509_CRL_VERSION_2;
		if ( xrtDerRead(&Fields, &Field) != XDER_VALUE ) {
			__xrtX509Error(
				XERR_PROTOCOL, X509_ERROR_CRL, "x509-crl-parse",
				"TBSCertList ends after its version", SIZE_MAX, NULL
			);
			return false;
		}
	}
	if ( !__xrtX509AlgorithmValue(
		&Field, &pCrl->TbsSignature, pBase, iSize, "x509-crl-parse"
	) || (xrtDerRead(&Fields, &Field) != XDER_VALUE) ||
		!__xrtX509NameValue(
			&Field, false, pBase, iSize, "x509-crl-parse"
		) ) {
		return false;
	}
	pCrl->Issuer = Field.Raw;
	if ( (xrtDerRead(&Fields, &Field) != XDER_VALUE) ||
		!__xrtX509TimeValue(
			&Field, &pCrl->ThisUpdate, pBase, iSize, "x509-crl-parse"
		) ) {
		return false;
	}

	Result = xrtDerRead(&Fields, &Field);
	if ( Result == XDER_ERROR ) {
		return false;
	}
	if ( (Result == XDER_VALUE) && __xrtX509CrlIsTime(&Field) ) {
		if ( !__xrtX509TimeValue(
			&Field, &pCrl->NextUpdate, pBase, iSize, "x509-crl-parse"
		) ) {
			return false;
		}
		pCrl->HasNextUpdate = true;
		if ( pCrl->NextUpdate < pCrl->ThisUpdate ) {
			__xrtX509Error(
				XERR_PROTOCOL, X509_ERROR_TIME, "x509-crl-parse",
				"CRL nextUpdate precedes thisUpdate",
				__xrtX509Offset(pBase, iSize, Field.Raw.Data), NULL
			);
			return false;
		}
		Result = xrtDerRead(&Fields, &Field);
		if ( Result == XDER_ERROR ) {
			return false;
		}
	}
	if ( (Result == XDER_VALUE) && xrtDerIs(
		&Field, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
	) ) {
		if ( !__xrtX509CrlRevokedValue(
			&Field, pCrl->Version, &pCrl->Revoked,
			pBase, iSize, "x509-crl-parse"
		) ) {
			return false;
		}
		Result = xrtDerRead(&Fields, &Field);
		if ( Result == XDER_ERROR ) {
			return false;
		}
	}
	if ( (Result == XDER_VALUE) && xrtDerIs(
		&Field, XASN1_CONTEXT, 0u, true
	) ) {
		if ( !__xrtX509CrlExtensions(
			&Field, pCrl->Version, &pCrl->Extensions, pBase, iSize
		) ) {
			return false;
		}
		Result = xrtDerRead(&Fields, &Field);
		if ( Result == XDER_ERROR ) {
			return false;
		}
	}
	if ( (Result != XDER_DONE) || !xrtDerDone(&Fields) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_CRL, "x509-crl-parse",
			"TBSCertList optional fields are unknown, out of order or trailing",
			Result == XDER_VALUE ?
				__xrtX509Offset(pBase, iSize, Field.Raw.Data) : SIZE_MAX,
			NULL
		);
		return false;
	}
	pCrl->Tbs = pValue->Raw;
	return true;
}



/* 严格解析一份完整 DER CRL；成功结果借用输入，失败时输出保持不变。 */
XRT_API bool xrtX509CrlParse(
	const void* pDer,
	size_t iSize,
	xx509crl* pCrl
)
{
	const uint8* pData = (const uint8*)pDer;
	xdercursor Root;
	xdercursor Fields;
	xdervalue Value;
	xdervalue Tbs;
	xdervalue Algorithm;
	xdervalue Signature;
	xx509crl Crl;
	uint8 iUnused;
	const xerror* pCause;

	if ( (pCrl == NULL) || ((pDer == NULL) && (iSize != 0)) ||
		(iSize == 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtDerValidate(pDer, iSize) ) {
		pCause = xrtGetError();
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_DER, "x509-crl-parse",
			"CRL is not one complete canonical DER value", SIZE_MAX, pCause
		);
		return false;
	}
	if ( !xrtDerInit(&Root, pDer, iSize) ||
		(xrtDerRead(&Root, &Value) != XDER_VALUE) || !xrtDerDone(&Root) ||
		!xrtDerIs(
			&Value, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
		) || !xrtDerEnter(&Value, &Fields) ||
		(xrtDerRead(&Fields, &Tbs) != XDER_VALUE) ||
		(xrtDerRead(&Fields, &Algorithm) != XDER_VALUE) ||
		(xrtDerRead(&Fields, &Signature) != XDER_VALUE) ||
		!xrtDerDone(&Fields) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_CRL, "x509-crl-parse",
			"CertificateList must contain exactly TBS, algorithm and signature",
			0, NULL
		);
		return false;
	}
	memset(&Crl, 0, sizeof(Crl));
	Crl.Raw = Value.Raw;
	if ( !__xrtX509CrlTbs(&Tbs, &Crl, pData, iSize) ||
		!__xrtX509AlgorithmValue(
			&Algorithm, &Crl.SignatureAlgorithm,
			pData, iSize, "x509-crl-parse"
		) ) {
		return false;
	}
	if ( (Crl.TbsSignature.Raw.Size != Crl.SignatureAlgorithm.Raw.Size) ||
		(memcmp(
			Crl.TbsSignature.Raw.Data, Crl.SignatureAlgorithm.Raw.Data,
			Crl.TbsSignature.Raw.Size
		) != 0) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_ALGORITHM, "x509-crl-parse",
			"TBS and outer CRL signature AlgorithmIdentifier differ",
			__xrtX509Offset(pData, iSize, Algorithm.Raw.Data), NULL
		);
		return false;
	}
	if ( !xrtDerBitString(&Signature, &Crl.Signature, &iUnused) ||
		(iUnused != 0) || (Crl.Signature.Size == 0) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_SIGNATURE, "x509-crl-parse",
			"CRL signature must be a nonempty octet-aligned BIT STRING",
			__xrtX509Offset(pData, iSize, Signature.Raw.Data), NULL
		);
		return false;
	}
	*pCrl = Crl;
	return true;
}



/* 判断绝对时间是否位于 CRL 发布窗口内。 */
XRT_API bool xrtX509CrlValidAt(const xx509crl* pCrl, xtime iTime)
{
	if ( pCrl == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return (iTime >= pCrl->ThisUpdate) &&
		(!pCrl->HasNextUpdate || (iTime <= pCrl->NextUpdate));
}



/* 初始化撤销条目游标；没有条目时初始化成功并立即结束。 */
XRT_API bool xrtX509CrlEntryInit(
	const xx509crl* pCrl,
	xx509crlcursor* pCursor
)
{
	xdercursor Root;
	xdervalue Sequence;
	xx509crlcursor Cursor;

	if ( (pCrl == NULL) || (pCursor == NULL) ||
		((pCrl->Revoked.Data == NULL) && (pCrl->Revoked.Size != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Cursor.Version = pCrl->Version;
	if ( pCrl->Revoked.Size == 0 ) {
		if ( !xrtDerInit(&Cursor.Items, NULL, 0) ) {
			return false;
		}
		*pCursor = Cursor;
		return true;
	}
	if ( !xrtDerInit(
		&Root, pCrl->Revoked.Data, pCrl->Revoked.Size
	) || (xrtDerRead(&Root, &Sequence) != XDER_VALUE) ||
		!xrtDerDone(&Root) || !xrtDerIs(
			&Sequence, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
		) || !xrtDerEnter(&Sequence, &Cursor.Items) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_CRL_ENTRY, "x509-crl-entry-init",
			"revokedCertificates view is not one DER sequence",
			SIZE_MAX, NULL
		);
		return false;
	}
	*pCursor = Cursor;
	return true;
}



/* 读取下一项撤销条目；失败时游标和输出保持不变。 */
XRT_API xx509result xrtX509CrlEntryRead(
	xx509crlcursor* pCursor,
	xx509crlentry* pEntry
)
{
	xx509crlcursor Cursor;
	xdervalue Value;
	xderresult Result;
	xx509crlentry Entry;

	if ( (pCursor == NULL) || (pEntry == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	Cursor = *pCursor;
	Result = xrtDerRead(&Cursor.Items, &Value);
	if ( Result == XDER_DONE ) {
		return X509_DONE;
	}
	if ( Result == XDER_ERROR ) {
		return X509_ERROR;
	}
	if ( !__xrtX509CrlEntryValue(
		&Value, Cursor.Version, &Entry,
		Cursor.Items.Data, Cursor.Items.Size, "x509-crl-entry-read"
	) ) {
		return X509_ERROR;
	}
	*pCursor = Cursor;
	*pEntry = Entry;
	return X509_VALUE;
}



/* 按规范 INTEGER 内容精确查找序列号。 */
XRT_API xx509result xrtX509CrlFind(
	const xx509crl* pCrl,
	xbytesview Serial,
	xx509crlentry* pEntry
)
{
	xx509crlcursor Cursor;
	xx509crlentry Entry;

	if ( (pCrl == NULL) || (Serial.Data == NULL) || (Serial.Size == 0) ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	if ( !xrtX509CrlEntryInit(pCrl, &Cursor) ) {
		return X509_ERROR;
	}
	while ( true ) {
		xx509result Result = xrtX509CrlEntryRead(&Cursor, &Entry);

		if ( Result != X509_VALUE ) {
			return Result;
		}
		if ( (Entry.Serial.Size == Serial.Size) &&
			(memcmp(Entry.Serial.Data, Serial.Data, Serial.Size) == 0) ) {
			if ( pEntry != NULL ) {
				*pEntry = Entry;
			}
			return X509_VALUE;
		}
	}
}



/* 判断 CRL 是否列出证书序列号。 */
XRT_API xx509result xrtX509CrlRevokes(
	const xx509crl* pCrl,
	const xx509cert* pCert,
	xx509crlentry* pEntry
)
{
	if ( pCert == NULL ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	return xrtX509CrlFind(pCrl, pCert->Serial, pEntry);
}

#endif
