#include "../internal/xrt_x509.h"



#if defined(XRT_FEATURE_X509_CRL_PROFILE)

const uint8 __xrtX509OidCrlNumber[] = { 0x55, 0x1D, 0x14 };
const size_t __xrtX509OidCrlNumberSize = sizeof(__xrtX509OidCrlNumber);
const uint8 __xrtX509OidCrlReason[] = { 0x55, 0x1D, 0x15 };
const size_t __xrtX509OidCrlReasonSize = sizeof(__xrtX509OidCrlReason);
const uint8 __xrtX509OidInvalidityDate[] = { 0x55, 0x1D, 0x18 };
const size_t __xrtX509OidInvalidityDateSize =
	sizeof(__xrtX509OidInvalidityDate);
const uint8 __xrtX509OidDeltaCrl[] = { 0x55, 0x1D, 0x1B };
const size_t __xrtX509OidDeltaCrlSize = sizeof(__xrtX509OidDeltaCrl);
const uint8 __xrtX509OidIssuingPoint[] = { 0x55, 0x1D, 0x1C };
const size_t __xrtX509OidIssuingPointSize =
	sizeof(__xrtX509OidIssuingPoint);
const uint8 __xrtX509OidCertificateIssuer[] = { 0x55, 0x1D, 0x1D };
const size_t __xrtX509OidCertificateIssuerSize =
	sizeof(__xrtX509OidCertificateIssuer);



/* 在已验证的 CRL 或条目扩展中查找并检查 critical 约束。 */
static xx509result __xrtX509CrlProfileExtension(
	xbytesview Extensions,
	const void* pOid,
	size_t iOidSize,
	bool bCritical,
	xx509error Code,
	cstr sOperation,
	cstr sCriticalMessage,
	xx509ext* pExtension
)
{
	xx509ext Extension;
	xx509result Result;

	if ( (pExtension == NULL) || (sCriticalMessage == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	Result = __xrtX509ExtensionFindValue(
		Extensions, pOid, iOidSize, &Extension, sOperation
	);
	if ( Result != X509_VALUE ) {
		return Result;
	}
	if ( Extension.Critical != bCritical ) {
		__xrtX509Error(
			XERR_PROTOCOL, Code, sOperation, sCriticalMessage, SIZE_MAX, NULL
		);
		return X509_ERROR;
	}
	*pExtension = Extension;
	return X509_VALUE;
}



/* 解析任意精度非负 CRL 编号正文。 */
XRT_API bool xrtX509CrlNumberParse(
	xbytesview Der,
	xbytesview* pNumber
)
{
	xdervalue Integer;
	xbytesview Number;
	const xerror* pCause;

	if ( (pNumber == NULL) || ((Der.Data == NULL) && (Der.Size != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtX509RootValue(
		Der, &Integer, X509_ERROR_CRL_NUMBER, "x509-crl-number",
		"CRL number is not one canonical non-negative DER INTEGER"
	) ) {
		return false;
	}
	if ( !xrtDerUnsigned(&Integer, &Number) ) {
		pCause = xrtGetError();
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_CRL_NUMBER, "x509-crl-number",
			"CRL number is not a non-negative DER INTEGER", SIZE_MAX, pCause
		);
		return false;
	}
	*pNumber = Number;
	return true;
}



/* 读取 CRLNumber。 */
XRT_API xx509result xrtX509CrlNumber(
	const xx509crl* pCrl,
	xbytesview* pNumber
)
{
	xx509ext Extension;
	xx509result Result;
	xbytesview Number;

	if ( (pCrl == NULL) || (pNumber == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	Result = __xrtX509CrlProfileExtension(
		pCrl->Extensions, __xrtX509OidCrlNumber,
		__xrtX509OidCrlNumberSize, false, X509_ERROR_CRL_NUMBER,
		"x509-crl-number", "CRLNumber must be non-critical", &Extension
	);
	if ( Result != X509_VALUE ) {
		return Result;
	}
	if ( !xrtX509CrlNumberParse(Extension.Value, &Number) ) {
		return X509_ERROR;
	}
	*pNumber = Number;
	return X509_VALUE;
}



/* 读取 DeltaCRLIndicator 的 BaseCRLNumber。 */
XRT_API xx509result xrtX509CrlDeltaBase(
	const xx509crl* pCrl,
	xbytesview* pNumber
)
{
	xx509ext Extension;
	xx509result Result;
	xbytesview Number;

	if ( (pCrl == NULL) || (pNumber == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	Result = __xrtX509CrlProfileExtension(
		pCrl->Extensions, __xrtX509OidDeltaCrl,
		__xrtX509OidDeltaCrlSize, true, X509_ERROR_CRL_NUMBER,
		"x509-crl-delta", "DeltaCRLIndicator must be critical", &Extension
	);
	if ( Result != X509_VALUE ) {
		return Result;
	}
	if ( !xrtX509CrlNumberParse(Extension.Value, &Number) ) {
		return X509_ERROR;
	}
	*pNumber = Number;
	return X509_VALUE;
}



/* 读取 CRL AuthorityKeyIdentifier。 */
XRT_API xx509result xrtX509CrlAuthorityKeyId(
	const xx509crl* pCrl,
	xx509authoritykeyid* pIdentifier
)
{
	xx509ext Extension;
	xx509result Result;
	xx509authoritykeyid Identifier;

	if ( (pCrl == NULL) || (pIdentifier == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	Result = __xrtX509CrlProfileExtension(
		pCrl->Extensions, __xrtX509OidAuthorityKeyId,
		__xrtX509OidAuthorityKeyIdSize, false, X509_ERROR_KEY_IDENTIFIER,
		"x509-crl-authority-key-id",
		"CRL AuthorityKeyIdentifier must be non-critical", &Extension
	);
	if ( Result != X509_VALUE ) {
		return Result;
	}
	if ( !xrtX509AuthorityKeyIdParse(Extension.Value, &Identifier) ) {
		return X509_ERROR;
	}
	*pIdentifier = Identifier;
	return X509_VALUE;
}



/* 解析隐式 DEFAULT FALSE；DER 中一旦出现就只能编码 TRUE。 */
static bool __xrtX509CrlProfileFlag(
	const xdervalue* pField,
	uint32 iTag,
	bool* pFlag
)
{
	if ( !xrtDerIs(pField, XASN1_CONTEXT, iTag, false) ||
		(pField->Value.Size != 1u) ||
		(pField->Value.Data[0] != UINT8_C(0xFF)) ) {
		return false;
	}
	*pFlag = true;
	return true;
}



/* 解析一项独立 IssuingDistributionPoint。 */
XRT_API bool xrtX509IssuingPointParse(
	xbytesview Der,
	xx509issuingpoint* pPoint
)
{
	xdervalue Sequence;
	xdercursor Fields;
	xdervalue Field;
	xx509issuingpoint Point;
	uint32 iNext = 0;
	size_t iCount = 0;
	const xerror* pCause = NULL;

	if ( (pPoint == NULL) || ((Der.Data == NULL) && (Der.Size != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(&Point, 0, sizeof(Point));
	if ( !__xrtX509RootValue(
		Der, &Sequence, X509_ERROR_CRL_ISSUING_POINT,
		"x509-crl-issuing-point",
		"IssuingDistributionPoint is not one canonical DER sequence"
	) ) {
		return false;
	}
	if ( !xrtDerIs(
		&Sequence, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
	) || !xrtDerEnter(&Sequence, &Fields) ) {
		goto Invalid;
	}
	while ( true ) {
		xderresult Result = xrtDerRead(&Fields, &Field);

		if ( Result == XDER_DONE ) {
			break;
		}
		if ( Result == XDER_ERROR ) {
			pCause = xrtGetError();
			goto Invalid;
		}
		if ( (Field.Tag.Class != XASN1_CONTEXT) ||
			(Field.Tag.Number > 5u) || (Field.Tag.Number < iNext) ) {
			goto Invalid;
		}
		iNext = Field.Tag.Number + 1u;
		if ( Field.Tag.Number == 0u ) {
			if ( !__xrtX509DistributionNameValue(
				&Field, &Point.DistributionPoint,
				"x509-crl-issuing-point"
			) ) {
				pCause = xrtGetError();
				goto Invalid;
			}
			Point.HasDistributionPoint = true;
		} else if ( Field.Tag.Number == 1u ) {
			if ( !__xrtX509CrlProfileFlag(
				&Field, 1u, &Point.OnlyUserCertificates
			) ) {
				goto Invalid;
			}
		} else if ( Field.Tag.Number == 2u ) {
			if ( !__xrtX509CrlProfileFlag(
				&Field, 2u, &Point.OnlyCaCertificates
			) ) {
				goto Invalid;
			}
		} else if ( Field.Tag.Number == 3u ) {
			if ( !__xrtX509ReasonFlagsValue(
				&Field, 3u, &Point.Reasons, "x509-crl-issuing-point"
			) ) {
				pCause = xrtGetError();
				goto Invalid;
			}
			Point.HasReasons = true;
		} else if ( Field.Tag.Number == 4u ) {
			if ( !__xrtX509CrlProfileFlag(
				&Field, 4u, &Point.Indirect
			) ) {
				goto Invalid;
			}
		} else if ( !__xrtX509CrlProfileFlag(
			&Field, 5u, &Point.OnlyAttributeCertificates
		) ) {
			goto Invalid;
		}
		iCount++;
	}
	if ( (iCount == 0) ||
		((size_t)Point.OnlyUserCertificates +
		 (size_t)Point.OnlyCaCertificates +
		 (size_t)Point.OnlyAttributeCertificates > 1u) ) {
		goto Invalid;
	}
	*pPoint = Point;
	return true;

Invalid:
	__xrtX509Error(
		XERR_PROTOCOL, X509_ERROR_CRL_ISSUING_POINT,
		"x509-crl-issuing-point",
		"IssuingDistributionPoint has invalid fields, defaults or scope",
		SIZE_MAX, pCause
	);
	return false;
}



/* 读取 CRL IssuingDistributionPoint。 */
XRT_API xx509result xrtX509CrlIssuingPoint(
	const xx509crl* pCrl,
	xx509issuingpoint* pPoint
)
{
	xx509ext Extension;
	xx509result Result;
	xx509issuingpoint Point;

	if ( (pCrl == NULL) || (pPoint == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	Result = __xrtX509CrlProfileExtension(
		pCrl->Extensions, __xrtX509OidIssuingPoint,
		__xrtX509OidIssuingPointSize, true,
		X509_ERROR_CRL_ISSUING_POINT, "x509-crl-issuing-point",
		"IssuingDistributionPoint must be critical", &Extension
	);
	if ( Result != X509_VALUE ) {
		return Result;
	}
	if ( !xrtX509IssuingPointParse(Extension.Value, &Point) ) {
		return X509_ERROR;
	}
	*pPoint = Point;
	return X509_VALUE;
}



/* 初始化完整 CRL 的 FreshestCRL，并执行 CRL 专属字段约束。 */
XRT_API xx509result xrtX509CrlFreshest(
	const xx509crl* pCrl,
	xx509distributioncursor* pCursor
)
{
	xx509ext Extension;
	xx509result Result;
	xx509distributioncursor Cursor;
	xx509distributioncursor Check;
	xx509distributionpoint Point;
	xbytesview BaseNumber;

	if ( (pCrl == NULL) || (pCursor == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	Result = __xrtX509CrlProfileExtension(
		pCrl->Extensions, __xrtX509OidFreshestCrl,
		__xrtX509OidFreshestCrlSize, false,
		X509_ERROR_DISTRIBUTION_POINT, "x509-crl-freshest",
		"CRL FreshestCRL must be non-critical", &Extension
	);
	if ( Result != X509_VALUE ) {
		return Result;
	}
	Result = xrtX509CrlDeltaBase(pCrl, &BaseNumber);
	if ( Result == X509_ERROR ) {
		return X509_ERROR;
	}
	if ( Result == X509_VALUE ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_DISTRIBUTION_POINT,
			"x509-crl-freshest",
			"FreshestCRL must not appear in a delta CRL", SIZE_MAX, NULL
		);
		return X509_ERROR;
	}
	if ( !xrtX509DistributionInit(Extension.Value, &Cursor) ) {
		return X509_ERROR;
	}
	Check = Cursor;
	while ( (Result = xrtX509DistributionRead(
		&Check, &Point
	)) == X509_VALUE ) {
		if ( !Point.HasName || Point.HasReasons || Point.HasIssuer ) {
			__xrtX509Error(
				XERR_PROTOCOL, X509_ERROR_DISTRIBUTION_POINT,
				"x509-crl-freshest",
				"CRL FreshestCRL points may contain only a distribution name",
				SIZE_MAX, NULL
			);
			return X509_ERROR;
		}
	}
	if ( Result == X509_ERROR ) {
		return X509_ERROR;
	}
	*pCursor = Cursor;
	return X509_VALUE;
}



/* 判断 CRLReason 协议值是否由 RFC 5280 定义。 */
static bool __xrtX509CrlReasonValid(uint8 iReason)
{
	return (iReason <= (uint8)X509_CRL_REASON_CERTIFICATE_HOLD) ||
		(iReason == (uint8)X509_CRL_REASON_REMOVE) ||
		(iReason == (uint8)X509_CRL_REASON_PRIVILEGE_WITHDRAWN) ||
		(iReason == (uint8)X509_CRL_REASON_AA_COMPROMISE);
}



/* 解析一项独立 CRLReason。 */
XRT_API bool xrtX509CrlReasonParse(
	xbytesview Der,
	xx509crlreason* pReason
)
{
	xdervalue Enumerated;
	xx509crlreason Reason;

	if ( (pReason == NULL) || ((Der.Data == NULL) && (Der.Size != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtX509RootValue(
		Der, &Enumerated, X509_ERROR_CRL_REASON, "x509-crl-reason",
		"CRLReason is not one canonical DER ENUMERATED"
	) ) {
		return false;
	}
	if ( !xrtDerIs(
		&Enumerated, XASN1_UNIVERSAL, (uint32)XASN1_ENUMERATED, false
	) || (Enumerated.Value.Size != 1u) ||
		!__xrtX509CrlReasonValid(Enumerated.Value.Data[0]) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_CRL_REASON, "x509-crl-reason",
			"CRLReason contains an undefined protocol value", SIZE_MAX, NULL
		);
		return false;
	}
	Reason = (xx509crlreason)Enumerated.Value.Data[0];
	*pReason = Reason;
	return true;
}



/* 读取条目 Reason Code。 */
XRT_API xx509result xrtX509CrlEntryReason(
	const xx509crlentry* pEntry,
	xx509crlreason* pReason
)
{
	xx509ext Extension;
	xx509result Result;
	xx509crlreason Reason;

	if ( (pEntry == NULL) || (pReason == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	Result = __xrtX509CrlProfileExtension(
		pEntry->Extensions, __xrtX509OidCrlReason,
		__xrtX509OidCrlReasonSize, false, X509_ERROR_CRL_REASON,
		"x509-crl-entry-reason", "Reason Code must be non-critical", &Extension
	);
	if ( Result != X509_VALUE ) {
		return Result;
	}
	if ( !xrtX509CrlReasonParse(Extension.Value, &Reason) ) {
		return X509_ERROR;
	}
	*pReason = Reason;
	return X509_VALUE;
}



/* 解析一项独立 InvalidityDate。 */
XRT_API bool xrtX509CrlInvalidityDateParse(
	xbytesview Der,
	xtime* pTime
)
{
	xdervalue Time;
	xtime iTime;
	const xerror* pCause;

	if ( (pTime == NULL) || ((Der.Data == NULL) && (Der.Size != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtX509RootValue(
		Der, &Time, X509_ERROR_CRL_INVALIDITY_DATE,
		"x509-crl-invalidity-date",
		"InvalidityDate is not one canonical DER GeneralizedTime"
	) ) {
		return false;
	}
	if ( !__xrtX509GeneralizedTimeValue(
		&Time, &iTime, Der.Data, Der.Size, "x509-crl-invalidity-date"
	) ) {
		pCause = xrtGetError();
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_CRL_INVALIDITY_DATE,
			"x509-crl-invalidity-date",
			"InvalidityDate is not a valid GeneralizedTime", SIZE_MAX, pCause
		);
		return false;
	}
	*pTime = iTime;
	return true;
}



/* 读取条目 Invalidity Date。 */
XRT_API xx509result xrtX509CrlEntryInvalidityDate(
	const xx509crlentry* pEntry,
	xtime* pTime
)
{
	xx509ext Extension;
	xx509result Result;
	xtime iTime;

	if ( (pEntry == NULL) || (pTime == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	Result = __xrtX509CrlProfileExtension(
		pEntry->Extensions, __xrtX509OidInvalidityDate,
		__xrtX509OidInvalidityDateSize, false,
		X509_ERROR_CRL_INVALIDITY_DATE, "x509-crl-entry-invalidity-date",
		"Invalidity Date must be non-critical", &Extension
	);
	if ( Result != X509_VALUE ) {
		return Result;
	}
	if ( !xrtX509CrlInvalidityDateParse(Extension.Value, &iTime) ) {
		return X509_ERROR;
	}
	*pTime = iTime;
	return X509_VALUE;
}



/* 读取条目 Certificate Issuer。 */
XRT_API xx509result xrtX509CrlEntryIssuer(
	const xx509crlentry* pEntry,
	xx509gencursor* pIssuer
)
{
	xx509ext Extension;
	xx509result Result;
	xx509gencursor Issuer;

	if ( (pEntry == NULL) || (pIssuer == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	Result = __xrtX509CrlProfileExtension(
		pEntry->Extensions, __xrtX509OidCertificateIssuer,
		__xrtX509OidCertificateIssuerSize, true,
		X509_ERROR_CRL_CERTIFICATE_ISSUER, "x509-crl-entry-issuer",
		"Certificate Issuer must be critical", &Extension
	);
	if ( Result != X509_VALUE ) {
		return Result;
	}
	if ( !xrtX509GeneralNameInit(Extension.Value, &Issuer) ) {
		return X509_ERROR;
	}
	*pIssuer = Issuer;
	return X509_VALUE;
}

#endif
