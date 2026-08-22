#include "../internal/xrt_x509.h"

#include <stdio.h>



#if defined(XRT_FEATURE_X509_PARSE)

const uint8 __xrtX509OidRsa[] = {
	0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x01
};
const size_t __xrtX509OidRsaSize = sizeof(__xrtX509OidRsa);

const uint8 __xrtX509OidRsaPss[] = {
	0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x0A
};
const size_t __xrtX509OidRsaPssSize = sizeof(__xrtX509OidRsaPss);

const uint8 __xrtX509OidEc[] = {
	0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01
};
const size_t __xrtX509OidEcSize = sizeof(__xrtX509OidEc);

const uint8 __xrtX509OidP256[] = {
	0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07
};
const size_t __xrtX509OidP256Size = sizeof(__xrtX509OidP256);

const uint8 __xrtX509OidP384[] = {
	0x2B, 0x81, 0x04, 0x00, 0x22
};
const size_t __xrtX509OidP384Size = sizeof(__xrtX509OidP384);

const uint8 __xrtX509OidP521[] = {
	0x2B, 0x81, 0x04, 0x00, 0x23
};
const size_t __xrtX509OidP521Size = sizeof(__xrtX509OidP521);

const uint8 __xrtX509OidEd25519[] = { 0x2B, 0x65, 0x70 };
const size_t __xrtX509OidEd25519Size = sizeof(__xrtX509OidEd25519);

const uint8 __xrtX509OidEd448[] = { 0x2B, 0x65, 0x71 };
const size_t __xrtX509OidEd448Size = sizeof(__xrtX509OidEd448);

const uint8 __xrtX509OidX25519[] = { 0x2B, 0x65, 0x6E };
const size_t __xrtX509OidX25519Size = sizeof(__xrtX509OidX25519);

const uint8 __xrtX509OidX448[] = { 0x2B, 0x65, 0x6F };
const size_t __xrtX509OidX448Size = sizeof(__xrtX509OidX448);

const uint8 __xrtX509OidSubjectAltName[] = { 0x55, 0x1D, 0x11 };
const size_t __xrtX509OidSubjectAltNameSize =
	sizeof(__xrtX509OidSubjectAltName);

const uint8 __xrtX509OidIssuerAltName[] = { 0x55, 0x1D, 0x12 };
const size_t __xrtX509OidIssuerAltNameSize =
	sizeof(__xrtX509OidIssuerAltName);

const uint8 __xrtX509OidKeyUsage[] = { 0x55, 0x1D, 0x0F };
const size_t __xrtX509OidKeyUsageSize = sizeof(__xrtX509OidKeyUsage);

const uint8 __xrtX509OidBasicConstraints[] = { 0x55, 0x1D, 0x13 };
const size_t __xrtX509OidBasicConstraintsSize =
	sizeof(__xrtX509OidBasicConstraints);

const uint8 __xrtX509OidExtendedKeyUsage[] = { 0x55, 0x1D, 0x25 };
const size_t __xrtX509OidExtendedKeyUsageSize =
	sizeof(__xrtX509OidExtendedKeyUsage);

const uint8 __xrtX509OidSubjectKeyId[] = { 0x55, 0x1D, 0x0E };
const size_t __xrtX509OidSubjectKeyIdSize =
	sizeof(__xrtX509OidSubjectKeyId);

const uint8 __xrtX509OidAuthorityKeyId[] = { 0x55, 0x1D, 0x23 };
const size_t __xrtX509OidAuthorityKeyIdSize =
	sizeof(__xrtX509OidAuthorityKeyId);

const uint8 __xrtX509OidCrlDistribution[] = { 0x55, 0x1D, 0x1F };
const size_t __xrtX509OidCrlDistributionSize =
	sizeof(__xrtX509OidCrlDistribution);

const uint8 __xrtX509OidFreshestCrl[] = { 0x55, 0x1D, 0x2E };
const size_t __xrtX509OidFreshestCrlSize =
	sizeof(__xrtX509OidFreshestCrl);

const uint8 __xrtX509OidNameConstraints[] = { 0x55, 0x1D, 0x1E };
const size_t __xrtX509OidNameConstraintsSize =
	sizeof(__xrtX509OidNameConstraints);



/* 设置带 DER 偏移和可选原因链的 X.509 结构化错误。 */
void __xrtX509Error(
	xerrkind Kind,
	xx509error Code,
	cstr sOperation,
	cstr sMessage,
	size_t iOffset,
	const xerror* pCause
)
{
	char Data[64];
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.x509";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	if ( iOffset != SIZE_MAX ) {
		(void)snprintf(
			Data, sizeof(Data), "offset=%llu", (unsigned long long)iOffset
		);
		Desc.Data = Data;
	}
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 返回借用指针相对证书 DER 起点的安全偏移。 */
size_t __xrtX509Offset(
	const uint8* pBase,
	size_t iSize,
	const uint8* pValue
)
{
	uintptr_t iBase;
	uintptr_t iValue;

	if ( (pBase == NULL) || (pValue == NULL) ) {
		return SIZE_MAX;
	}
	iBase = (uintptr_t)pBase;
	iValue = (uintptr_t)pValue;
	if ( (iValue < iBase) || ((iValue - iBase) > iSize) ) {
		return SIZE_MAX;
	}
	return (size_t)(iValue - iBase);
}



/* 判断借用 OID 是否与一个静态内容八位组完全相同。 */
static bool __xrtX509OidEqual(
	xbytesview Oid,
	const void* pExpected,
	size_t iExpectedSize
)
{
	return (Oid.Size == iExpectedSize) &&
		(memcmp(Oid.Data, pExpected, iExpectedSize) == 0);
}



/* 验证固定长度 ASN.1 时间中的全部十进制位。 */
static bool __xrtX509TimeDigits(
	const uint8* pData,
	size_t iDigits
)
{
	for ( size_t i = 0; i < iDigits; i++ ) {
		if ( (pData[i] < (uint8)'0') || (pData[i] > (uint8)'9') ) {
			return false;
		}
	}
	return true;
}



/* 读取两个十进制字符。 */
static int __xrtX509TimePair(const uint8* pData)
{
	return ((int)(pData[0] - (uint8)'0') * 10) +
		(int)(pData[1] - (uint8)'0');
}



/* 严格解析 RFC 5280 时间字符，并兼容可无歧义解释的时间标签选择。 */
static bool __xrtX509TimeValueParse(
	const xdervalue* pValue,
	xtime* pTime,
	const uint8* pBase,
	size_t iSize,
	cstr sOperation
)
{
	const uint8* pData;
	size_t iLength;
	int64 iYear;
	int iMonth;
	int iDay;
	int iHour;
	int iMinute;
	int iSecond;
	const xerror* pCause;

	if ( (pValue == NULL) || (pTime == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pData = pValue->Value.Data;
	iLength = pValue->Value.Size;
	if ( xrtDerIs(
		pValue, XASN1_UNIVERSAL, (uint32)XASN1_UTC_TIME, false
	) ) {
		if ( (iLength != 13u) || (pData[12] != (uint8)'Z') ||
			!__xrtX509TimeDigits(pData, 12u) ) {
			__xrtX509Error(
				XERR_PROTOCOL, X509_ERROR_TIME, sOperation,
				"UTCTime must be exactly YYMMDDHHMMSSZ",
				__xrtX509Offset(pBase, iSize, pValue->Raw.Data), NULL
			);
			return false;
		}
		iYear = __xrtX509TimePair(pData);
		iYear += iYear >= 50 ? 1900 : 2000;
		pData += 2;
	} else if ( xrtDerIs(
		pValue, XASN1_UNIVERSAL,
		(uint32)XASN1_GENERALIZED_TIME, false
	) ) {
		if ( (iLength != 15u) || (pData[14] != (uint8)'Z') ||
			!__xrtX509TimeDigits(pData, 14u) ) {
			__xrtX509Error(
				XERR_PROTOCOL, X509_ERROR_TIME, sOperation,
				"GeneralizedTime must be exactly YYYYMMDDHHMMSSZ",
				__xrtX509Offset(pBase, iSize, pValue->Raw.Data), NULL
			);
			return false;
		}
		iYear = ((int64)__xrtX509TimePair(pData) * 100) +
			__xrtX509TimePair(pData + 2);
		pData += 4;
	} else {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_TIME, sOperation,
			"X.509 time uses an unsupported ASN.1 type",
			__xrtX509Offset(pBase, iSize, pValue->Raw.Data), NULL
		);
		return false;
	}
	iMonth = __xrtX509TimePair(pData);
	iDay = __xrtX509TimePair(pData + 2);
	iHour = __xrtX509TimePair(pData + 4);
	iMinute = __xrtX509TimePair(pData + 6);
	iSecond = __xrtX509TimePair(pData + 8);
	if ( iSecond > 59 ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_TIME, sOperation,
			"X.509 time contains an invalid second",
			__xrtX509Offset(pBase, iSize, pValue->Raw.Data), NULL
		);
		return false;
	}
	if ( !xrtDateTime(
		iYear, iMonth, iDay, iHour, iMinute, iSecond, 0, pTime
	) ) {
		pCause = xrtGetError();
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_TIME, sOperation,
			"X.509 time contains an invalid calendar value",
			__xrtX509Offset(pBase, iSize, pValue->Raw.Data), pCause
		);
		return false;
	}
	return true;
}



/* 解析证书有效期使用的 UTCTime 或 GeneralizedTime。 */
bool __xrtX509TimeValue(
	const xdervalue* pValue,
	xtime* pTime,
	const uint8* pBase,
	size_t iSize,
	cstr sOperation
)
{
	return __xrtX509TimeValueParse(
		pValue, pTime, pBase, iSize, sOperation
	);
}



/* 解析协议类型固定为 GeneralizedTime 的时间字段。 */
bool __xrtX509GeneralizedTimeValue(
	const xdervalue* pValue,
	xtime* pTime,
	const uint8* pBase,
	size_t iSize,
	cstr sOperation
)
{
	if ( !xrtDerIs(
		pValue, XASN1_UNIVERSAL, (uint32)XASN1_GENERALIZED_TIME, false
	) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_TIME, sOperation,
			"X.509 field must use GeneralizedTime",
			pValue != NULL ?
				__xrtX509Offset(pBase, iSize, pValue->Raw.Data) : SIZE_MAX,
			NULL
		);
		return false;
	}
	return __xrtX509TimeValueParse(
		pValue, pTime, pBase, iSize, sOperation
	);
}



/* 返回已经通过 DER 校验的序列号 INTEGER 完整内容。 */
bool __xrtX509SerialValue(
	const xdervalue* pValue,
	xbytesview* pSerial,
	const uint8* pBase,
	size_t iSize,
	cstr sOperation
)
{
	if ( (pValue == NULL) || (pSerial == NULL) || !xrtDerIs(
		pValue, XASN1_UNIVERSAL, (uint32)XASN1_INTEGER, false
	) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_SERIAL, sOperation,
			"serial number must be a canonical DER integer",
			pValue != NULL ?
				__xrtX509Offset(pBase, iSize, pValue->Raw.Data) : SIZE_MAX,
			NULL
		);
		return false;
	}
	*pSerial = pValue->Value;
	return true;
}



/* 解析已经通过 DER 校验的 AlgorithmIdentifier 值。 */
bool __xrtX509AlgorithmValue(
	const xdervalue* pValue,
	xx509algorithm* pAlgorithm,
	const uint8* pBase,
	size_t iSize,
	cstr sOperation
)
{
	xdercursor Cursor;
	xdervalue Field;
	xx509algorithm Algorithm;

	if ( (pValue == NULL) || (pAlgorithm == NULL) ||
		!xrtDerIs(
			pValue, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
		) || !xrtDerEnter(pValue, &Cursor) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_ALGORITHM, sOperation,
			"AlgorithmIdentifier must be a DER sequence",
			pValue != NULL ? __xrtX509Offset(pBase, iSize, pValue->Raw.Data) :
				SIZE_MAX,
			NULL
		);
		return false;
	}
	memset(&Algorithm, 0, sizeof(Algorithm));
	if ( (xrtDerRead(&Cursor, &Field) != XDER_VALUE) ||
		!xrtDerOid(&Field, &Algorithm.Oid) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_ALGORITHM, sOperation,
			"AlgorithmIdentifier is missing its object identifier",
			__xrtX509Offset(pBase, iSize, pValue->Raw.Data), NULL
		);
		return false;
	}
	if ( xrtDerRead(&Cursor, &Field) == XDER_VALUE ) {
		Algorithm.Parameters = Field.Raw;
		Algorithm.HasParameters = true;
	}
	if ( !xrtDerDone(&Cursor) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_ALGORITHM, sOperation,
			"AlgorithmIdentifier contains more than one parameter value",
			__xrtX509Offset(pBase, iSize, pValue->Raw.Data), NULL
		);
		return false;
	}
	Algorithm.Raw = pValue->Raw;
	*pAlgorithm = Algorithm;
	return true;
}



/* 严格解析一个独立 AlgorithmIdentifier DER 值。 */
XRT_API bool xrtX509AlgorithmParse(
	xbytesview Der,
	xx509algorithm* pAlgorithm
)
{
	xdercursor Cursor;
	xdervalue Value;
	const xerror* pCause;

	if ( (pAlgorithm == NULL) || ((Der.Data == NULL) && (Der.Size != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtDerValidate(Der.Data, Der.Size) ) {
		pCause = xrtGetError();
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_DER, "x509-algorithm",
			"AlgorithmIdentifier is not valid DER", SIZE_MAX, pCause
		);
		return false;
	}
	if ( !xrtDerInit(&Cursor, Der.Data, Der.Size) ||
		(xrtDerRead(&Cursor, &Value) != XDER_VALUE) ||
		!xrtDerDone(&Cursor) ) {
		return false;
	}
	return __xrtX509AlgorithmValue(
		&Value, pAlgorithm, Der.Data, Der.Size, "x509-algorithm"
	);
}



/* 严格解析时间内容，并兼容证书中可无歧义解释的时间标签选择。 */
XRT_API bool xrtX509TimeParse(
	xbytesview Der,
	xtime* pTime
)
{
	xdercursor Cursor;
	xdervalue Value;
	xtime iTime;
	const xerror* pCause;

	if ( (pTime == NULL) || ((Der.Data == NULL) && (Der.Size != 0)) ||
		(Der.Size == 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtDerValidate(Der.Data, Der.Size) ) {
		pCause = xrtGetError();
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_DER, "x509-time",
			"X.509 time is not one complete canonical DER value",
			SIZE_MAX, pCause
		);
		return false;
	}
	if ( !xrtDerInit(&Cursor, Der.Data, Der.Size) ||
		(xrtDerRead(&Cursor, &Value) != XDER_VALUE) ||
		!xrtDerDone(&Cursor) || !__xrtX509TimeValue(
			&Value, &iTime, Der.Data, Der.Size, "x509-time"
		) ) {
		return false;
	}
	*pTime = iTime;
	return true;
}



/* 解析一个 AttributeTypeAndValue，并发布借用属性视图。 */
static bool __xrtX509NameAttribute(
	const xdervalue* pValue,
	size_t iRdn,
	xx509nameattr* pAttribute,
	const uint8* pBase,
	size_t iSize,
	cstr sOperation
)
{
	xdercursor Cursor;
	xdervalue Oid;
	xdervalue Value;
	xx509nameattr Attribute;

	if ( !xrtDerIs(
		pValue, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
	) || !xrtDerEnter(pValue, &Cursor) ||
		(xrtDerRead(&Cursor, &Oid) != XDER_VALUE) ||
		!xrtDerOid(&Oid, &Attribute.Oid) ||
		(xrtDerRead(&Cursor, &Value) != XDER_VALUE) ||
		!xrtDerDone(&Cursor) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_NAME, sOperation,
			"Name attribute must contain exactly an OID and one value",
			__xrtX509Offset(pBase, iSize, pValue->Raw.Data), NULL
		);
		return false;
	}
	Attribute.Raw = pValue->Raw;
	Attribute.ValueTag = Value.Tag;
	Attribute.Value = Value.Value;
	Attribute.Rdn = iRdn;
	*pAttribute = Attribute;
	return true;
}



/* 验证已经通过 DER 校验的 Name 值。 */
bool __xrtX509NameValue(
	const xdervalue* pValue,
	bool bAllowEmpty,
	const uint8* pBase,
	size_t iSize,
	cstr sOperation
)
{
	xdercursor Rdns;
	xdervalue Rdn;
	size_t iRdn = 0;

	if ( (pValue == NULL) ||
		!xrtDerIs(
			pValue, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
		) || !xrtDerEnter(pValue, &Rdns) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_NAME, sOperation,
			"X.509 Name must be a DER sequence",
			pValue != NULL ? __xrtX509Offset(pBase, iSize, pValue->Raw.Data) :
				SIZE_MAX,
			NULL
		);
		return false;
	}
	while ( xrtDerRead(&Rdns, &Rdn) == XDER_VALUE ) {
		xdercursor Attributes;
		xdervalue Attribute;
		xx509nameattr Parsed;
		size_t iCount = 0;

		if ( !xrtDerIs(
			&Rdn, XASN1_UNIVERSAL, (uint32)XASN1_SET, true
		) || !xrtDerEnter(&Rdn, &Attributes) ) {
			__xrtX509Error(
				XERR_PROTOCOL, X509_ERROR_NAME, sOperation,
				"X.509 Name RDN must be a DER set",
				__xrtX509Offset(pBase, iSize, Rdn.Raw.Data), NULL
			);
			return false;
		}
		while ( xrtDerRead(&Attributes, &Attribute) == XDER_VALUE ) {
			if ( !__xrtX509NameAttribute(
				&Attribute, iRdn, &Parsed, pBase, iSize, sOperation
			) ) {
				return false;
			}
			iCount++;
		}
		if ( (iCount == 0) || !xrtDerDone(&Attributes) ) {
			__xrtX509Error(
				XERR_PROTOCOL, X509_ERROR_NAME, sOperation,
				"X.509 Name RDN must contain at least one valid attribute",
				__xrtX509Offset(pBase, iSize, Rdn.Raw.Data), NULL
			);
			return false;
		}
		iRdn++;
	}
	if ( !xrtDerDone(&Rdns) || (!bAllowEmpty && (iRdn == 0)) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_NAME, sOperation,
			"X.509 Name is malformed or unexpectedly empty",
			__xrtX509Offset(pBase, iSize, pValue->Raw.Data), NULL
		);
		return false;
	}
	return true;
}



/* 从完整 Name DER 初始化借用式属性游标。 */
XRT_API bool xrtX509NameInit(
	xbytesview Name,
	xx509namecursor* pCursor
)
{
	xdercursor Root;
	xdervalue Value;
	xx509namecursor Cursor;
	const xerror* pCause;

	if ( (pCursor == NULL) || ((Name.Data == NULL) && (Name.Size != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtDerValidate(Name.Data, Name.Size) ) {
		pCause = xrtGetError();
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_DER, "x509-name",
			"X.509 Name is not valid DER", SIZE_MAX, pCause
		);
		return false;
	}
	if ( !xrtDerInit(&Root, Name.Data, Name.Size) ||
		(xrtDerRead(&Root, &Value) != XDER_VALUE) ||
		!xrtDerDone(&Root) || !__xrtX509NameValue(
			&Value, true, Name.Data, Name.Size, "x509-name"
		) ) {
		return false;
	}
	memset(&Cursor, 0, sizeof(Cursor));
	if ( !xrtDerEnter(&Value, &Cursor.Rdns) ||
		!xrtDerInit(&Cursor.Attributes, NULL, 0) ) {
		return false;
	}
	*pCursor = Cursor;
	return true;
}



/* 读取下一个 Name 属性；失败时游标和输出保持不变。 */
XRT_API xx509result xrtX509NameRead(
	xx509namecursor* pCursor,
	xx509nameattr* pAttribute
)
{
	xx509namecursor Cursor;
	xx509nameattr Attribute;

	if ( (pCursor == NULL) || (pAttribute == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	Cursor = *pCursor;
	while ( true ) {
		xdervalue Value;
		xderresult Result;

		if ( Cursor.Active ) {
			Result = xrtDerRead(&Cursor.Attributes, &Value);
			if ( Result == XDER_VALUE ) {
				if ( !__xrtX509NameAttribute(
					&Value, Cursor.Rdn - 1u, &Attribute,
					Cursor.Rdns.Data, Cursor.Rdns.Size, "x509-name-read"
				) ) {
					return X509_ERROR;
				}
				*pCursor = Cursor;
				*pAttribute = Attribute;
				return X509_VALUE;
			}
			if ( Result == XDER_ERROR ) {
				return X509_ERROR;
			}
			Cursor.Active = false;
		}

		Result = xrtDerRead(&Cursor.Rdns, &Value);
		if ( Result == XDER_DONE ) {
			return X509_DONE;
		}
		if ( (Result == XDER_ERROR) || !xrtDerIs(
			&Value, XASN1_UNIVERSAL, (uint32)XASN1_SET, true
		) || !xrtDerEnter(&Value, &Cursor.Attributes) ) {
			__xrtX509Error(
				XERR_PROTOCOL, X509_ERROR_NAME, "x509-name-read",
				"X.509 Name cursor contains a malformed RDN", SIZE_MAX, NULL
			);
			return X509_ERROR;
		}
		Cursor.Rdn++;
		Cursor.Active = true;
	}
}



/* 查找第一个 OID 完全相同的 Name 属性。 */
XRT_API bool xrtX509NameFind(
	xbytesview Name,
	const void* pOid,
	size_t iOidSize,
	xx509nameattr* pAttribute
)
{
	xx509namecursor Cursor;
	xx509nameattr Attribute;

	if ( (pOid == NULL) || (iOidSize == 0) || (pAttribute == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtX509NameInit(Name, &Cursor) ) {
		return false;
	}
	while ( true ) {
		xx509result Result = xrtX509NameRead(&Cursor, &Attribute);

		if ( Result == X509_ERROR ) {
			return false;
		}
		if ( Result == X509_DONE ) {
			__xrtX509Error(
				XERR_NOT_FOUND, X509_ERROR_NOT_FOUND, "x509-name-find",
				"requested X.509 Name attribute was not found", SIZE_MAX, NULL
			);
			return false;
		}
		if ( __xrtX509OidEqual(Attribute.Oid, pOid, iOidSize) ) {
			*pAttribute = Attribute;
			return true;
		}
	}
}



/* 解析一个已经通过 DER 校验的扩展值。 */
bool __xrtX509ExtensionValue(
	const xdervalue* pValue,
	xx509ext* pExtension,
	const uint8* pBase,
	size_t iSize,
	cstr sOperation
)
{
	xdercursor Cursor;
	xdervalue Field;
	xx509ext Extension;
	const xerror* pCause;

	if ( (pValue == NULL) || (pExtension == NULL) ||
		!xrtDerIs(
			pValue, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
		) || !xrtDerEnter(pValue, &Cursor) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_EXTENSION, sOperation,
			"X.509 Extension must be a DER sequence",
			pValue != NULL ? __xrtX509Offset(pBase, iSize, pValue->Raw.Data) :
				SIZE_MAX,
			NULL
		);
		return false;
	}
	memset(&Extension, 0, sizeof(Extension));
	if ( (xrtDerRead(&Cursor, &Field) != XDER_VALUE) ||
		!xrtDerOid(&Field, &Extension.Oid) ||
		(xrtDerRead(&Cursor, &Field) != XDER_VALUE) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_EXTENSION, sOperation,
			"X.509 Extension is missing its OID or value",
			__xrtX509Offset(pBase, iSize, pValue->Raw.Data), NULL
		);
		return false;
	}
	if ( xrtDerIs(
		&Field, XASN1_UNIVERSAL, (uint32)XASN1_BOOLEAN, false
	) ) {
		if ( !xrtDerBoolean(&Field, &Extension.Critical) ||
			!Extension.Critical ||
			(xrtDerRead(&Cursor, &Field) != XDER_VALUE) ) {
			__xrtX509Error(
				XERR_PROTOCOL, X509_ERROR_EXTENSION, sOperation,
				"Extension critical DEFAULT must be omitted when false",
				__xrtX509Offset(pBase, iSize, pValue->Raw.Data), NULL
			);
			return false;
		}
	}
	if ( !xrtDerOctets(&Field, &Extension.Value) || !xrtDerDone(&Cursor) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_EXTENSION, sOperation,
			"X.509 Extension must end with exactly one OCTET STRING",
			__xrtX509Offset(pBase, iSize, pValue->Raw.Data), NULL
		);
		return false;
	}
	if ( !xrtDerValidate(Extension.Value.Data, Extension.Value.Size) ) {
		pCause = xrtGetError();
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_EXTENSION, sOperation,
			"X.509 extnValue does not contain one valid DER value",
			__xrtX509Offset(pBase, iSize, Extension.Value.Data), pCause
		);
		return false;
	}
	Extension.Raw = pValue->Raw;
	*pExtension = Extension;
	return true;
}



/* 只读取已校验扩展的 OID，用于避免重复正文验证。 */
static bool __xrtX509ExtensionOid(
	const xdervalue* pValue,
	xbytesview* pOid
)
{
	xdercursor Cursor;
	xdervalue Oid;

	return xrtDerIs(
		pValue, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
	) && xrtDerEnter(pValue, &Cursor) &&
		(xrtDerRead(&Cursor, &Oid) == XDER_VALUE) && xrtDerOid(&Oid, pOid);
}



/* 验证一项非空 Extensions SEQUENCE 及重复 OID。 */
bool __xrtX509ExtensionListValue(
	const xdervalue* pValue,
	xbytesview* pExtensions,
	const uint8* pBase,
	size_t iSize,
	cstr sOperation
)
{
	xdercursor Items;
	xdervalue Value;
	size_t iCount = 0;

	if ( (pValue == NULL) || (pExtensions == NULL) || !xrtDerIs(
		pValue, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
	) || !xrtDerEnter(pValue, &Items) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_EXTENSION, sOperation,
			"Extensions must be a DER sequence",
			pValue != NULL ?
				__xrtX509Offset(pBase, iSize, pValue->Raw.Data) : SIZE_MAX,
			NULL
		);
		return false;
	}
	while ( xrtDerRead(&Items, &Value) == XDER_VALUE ) {
		xx509ext Extension;
		xdercursor Previous;
		xdervalue PriorValue;

		if ( !__xrtX509ExtensionValue(
			&Value, &Extension, pBase, iSize, sOperation
		) ) {
			return false;
		}
		if ( !xrtDerEnter(pValue, &Previous) ) {
			return false;
		}
		while ( xrtDerRead(&Previous, &PriorValue) == XDER_VALUE ) {
			xbytesview PriorOid;

			if ( PriorValue.Raw.Data == Value.Raw.Data ) {
				break;
			}
			if ( !__xrtX509ExtensionOid(&PriorValue, &PriorOid) ) {
				return false;
			}
			if ( __xrtX509OidEqual(
				PriorOid, Extension.Oid.Data, Extension.Oid.Size
			) ) {
				__xrtX509Error(
					XERR_PROTOCOL, X509_ERROR_DUPLICATE_EXTENSION,
					sOperation, "Extensions repeat an object identifier",
					__xrtX509Offset(pBase, iSize, Value.Raw.Data), NULL
				);
				return false;
			}
		}
		iCount++;
	}
	if ( !xrtDerDone(&Items) || (iCount == 0) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_EXTENSION, sOperation,
			"Extensions must contain at least one Extension",
			__xrtX509Offset(pBase, iSize, pValue->Raw.Data), NULL
		);
		return false;
	}
	*pExtensions = pValue->Raw;
	return true;
}



/* 从已经由证书或 CRL 解析器验证过的扩展列表初始化游标。 */
bool __xrtX509ExtensionCursorInit(
	xbytesview Extensions,
	xx509extcursor* pCursor,
	cstr sOperation
)
{
	xdercursor Root;
	xdervalue Sequence;
	xx509extcursor Cursor;

	if ( (pCursor == NULL) || (sOperation == NULL) ||
		((Extensions.Data == NULL) && (Extensions.Size != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( Extensions.Size == 0 ) {
		if ( !xrtDerInit(&Cursor.Items, NULL, 0) ) {
			return false;
		}
		*pCursor = Cursor;
		return true;
	}
	if ( !xrtDerInit(&Root, Extensions.Data, Extensions.Size) ||
		(xrtDerRead(&Root, &Sequence) != XDER_VALUE) ||
		!xrtDerDone(&Root) || !xrtDerIs(
			&Sequence, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
		) || !xrtDerEnter(&Sequence, &Cursor.Items) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_EXTENSION, sOperation,
			"validated X.509 object contains an invalid Extensions view",
			SIZE_MAX, NULL
		);
		return false;
	}
	*pCursor = Cursor;
	return true;
}



/* 在已经由证书或 CRL 解析器验证过的扩展列表中单次查找。 */
xx509result __xrtX509ExtensionFindValue(
	xbytesview Extensions,
	const void* pOid,
	size_t iOidSize,
	xx509ext* pExtension,
	cstr sOperation
)
{
	xx509extcursor Cursor;
	xx509ext Extension;

	if ( (pOid == NULL) || (iOidSize == 0) || (pExtension == NULL) ||
		(sOperation == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	if ( !__xrtX509ExtensionCursorInit(
		Extensions, &Cursor, sOperation
	) ) {
		return X509_ERROR;
	}
	while ( true ) {
		xx509result Result = xrtX509ExtensionRead(&Cursor, &Extension);

		if ( Result != X509_VALUE ) {
			return Result;
		}
		if ( __xrtX509OidEqual(Extension.Oid, pOid, iOidSize) ) {
			*pExtension = Extension;
			return X509_VALUE;
		}
	}
}



/* 从一项完整 DER 中读取唯一顶层值。 */
bool __xrtX509RootValue(
	xbytesview Der,
	xdervalue* pValue,
	xx509error Code,
	cstr sOperation,
	cstr sMessage
)
{
	xdercursor Cursor;
	const xerror* pCause;

	if ( (pValue == NULL) || (sOperation == NULL) || (sMessage == NULL) ||
		((Der.Data == NULL) && (Der.Size != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtDerValidate(Der.Data, Der.Size) ) {
		pCause = xrtGetError();
		__xrtX509Error(
			XERR_PROTOCOL, Code, sOperation, sMessage, SIZE_MAX, pCause
		);
		return false;
	}
	if ( !xrtDerInit(&Cursor, Der.Data, Der.Size) ||
		(xrtDerRead(&Cursor, pValue) != XDER_VALUE) ||
		!xrtDerDone(&Cursor) ) {
		__xrtX509Error(
			XERR_PROTOCOL, Code, sOperation, sMessage, SIZE_MAX, NULL
		);
		return false;
	}
	return true;
}



/* 从一项完整 Extensions DER 初始化游标；空视图表示没有扩展。 */
XRT_API bool xrtX509ExtensionListInit(
	xbytesview Extensions,
	xx509extcursor* pCursor
)
{
	xdercursor Root;
	xdervalue Sequence;
	xbytesview Checked;
	xx509extcursor Cursor;
	const xerror* pCause;

	if ( (pCursor == NULL) ||
		((Extensions.Data == NULL) && (Extensions.Size != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( Extensions.Size == 0 ) {
		if ( !xrtDerInit(&Cursor.Items, NULL, 0) ) {
			return false;
		}
		*pCursor = Cursor;
		return true;
	}
	if ( !xrtDerValidate(Extensions.Data, Extensions.Size) ) {
		pCause = xrtGetError();
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_DER, "x509-extension-list",
			"Extensions is not one complete canonical DER value",
			SIZE_MAX, pCause
		);
		return false;
	}
	if ( !xrtDerInit(&Root, Extensions.Data, Extensions.Size) ||
		(xrtDerRead(&Root, &Sequence) != XDER_VALUE) ||
		!xrtDerDone(&Root) || !__xrtX509ExtensionListValue(
			&Sequence, &Checked, Extensions.Data, Extensions.Size,
			"x509-extension-list"
		) || !xrtDerEnter(&Sequence, &Cursor.Items) ) {
		return false;
	}
	*pCursor = Cursor;
	return true;
}



/* 从证书初始化扩展游标；没有扩展时初始化成功并立即结束。 */
XRT_API bool xrtX509ExtensionInit(
	const xx509cert* pCert,
	xx509extcursor* pCursor
)
{
	if ( pCert == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return xrtX509ExtensionListInit(pCert->Extensions, pCursor);
}



/* 读取下一个扩展；失败时游标和输出保持不变。 */
XRT_API xx509result xrtX509ExtensionRead(
	xx509extcursor* pCursor,
	xx509ext* pExtension
)
{
	xx509extcursor Cursor;
	xdervalue Value;
	xderresult Result;
	xx509ext Extension;

	if ( (pCursor == NULL) || (pExtension == NULL) ) {
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
	if ( !__xrtX509ExtensionValue(
		&Value, &Extension, Cursor.Items.Data, Cursor.Items.Size,
		"x509-extension-read"
	) ) {
		return X509_ERROR;
	}
	*pCursor = Cursor;
	*pExtension = Extension;
	return X509_VALUE;
}



/* 在一项完整 Extensions DER 中查找 OID 完全相同的扩展。 */
XRT_API bool xrtX509ExtensionListFind(
	xbytesview Extensions,
	const void* pOid,
	size_t iOidSize,
	xx509ext* pExtension
)
{
	xx509extcursor Cursor;
	xx509ext Extension;

	if ( (pOid == NULL) || (iOidSize == 0) || (pExtension == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtX509ExtensionListInit(Extensions, &Cursor) ) {
		return false;
	}
	while ( true ) {
		xx509result Result = xrtX509ExtensionRead(&Cursor, &Extension);

		if ( Result == X509_ERROR ) {
			return false;
		}
		if ( Result == X509_DONE ) {
			__xrtX509Error(
				XERR_NOT_FOUND, X509_ERROR_NOT_FOUND,
				"x509-extension-list-find",
				"requested X.509 extension was not found", SIZE_MAX, NULL
			);
			return false;
		}
		if ( __xrtX509OidEqual(Extension.Oid, pOid, iOidSize) ) {
			*pExtension = Extension;
			return true;
		}
	}
}



/* 查找第一个 OID 完全相同的证书扩展。 */
XRT_API bool xrtX509ExtensionFind(
	const xx509cert* pCert,
	const void* pOid,
	size_t iOidSize,
	xx509ext* pExtension
)
{
	if ( pCert == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return xrtX509ExtensionListFind(
		pCert->Extensions, pOid, iOidSize, pExtension
	);
}

#endif
