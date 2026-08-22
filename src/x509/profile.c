#include "../internal/xrt_x509.h"



#if defined(XRT_FEATURE_X509_PROFILE)

/* 在不产生 NOT_FOUND 错误的情况下查找一个扩展。 */
static xx509result __xrtX509ProfileExtension(
	const xx509cert* pCert,
	const void* pOid,
	size_t iOidSize,
	xx509ext* pExtension
)
{
	if ( (pCert == NULL) || (pOid == NULL) || (iOidSize == 0) ||
		(pExtension == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	return __xrtX509ExtensionFindValue(
		pCert->Extensions, pOid, iOidSize, pExtension, "x509-profile"
	);
}



/* 验证隐式 registeredID 的 OID 内容八位组。 */
static bool __xrtX509ProfileOidContent(
	const uint8* pData,
	size_t iSize
)
{
	bool bFirst = true;
	bool bOpen = false;

	if ( iSize == 0 ) {
		return false;
	}
	for ( size_t i = 0; i < iSize; i++ ) {
		if ( bFirst && (pData[i] == UINT8_C(0x80)) ) {
			return false;
		}
		bOpen = (pData[i] & UINT8_C(0x80)) != 0;
		bFirst = !bOpen;
	}
	return !bOpen;
}



/* 验证 IA5 GeneralName 内容不为空且只包含七位字符。 */
static bool __xrtX509ProfileIa5(xbytesview Value)
{
	if ( Value.Size == 0 ) {
		return false;
	}
	for ( size_t i = 0; i < Value.Size; i++ ) {
		if ( Value.Data[i] > UINT8_C(0x7F) ) {
			return false;
		}
	}
	return true;
}



/* 把 ASCII 字母转换为小写，其余字节保持不变。 */
static uint8 __xrtX509AsciiLower(uint8 iValue)
{
	if ( (iValue >= (uint8)'A') && (iValue <= (uint8)'Z') ) {
		return (uint8)(iValue + ((uint8)'a' - (uint8)'A'));
	}
	return iValue;
}



/* 按 ASCII 大小写不敏感规则比较两个等长字节视图。 */
bool __xrtX509AsciiEqual(xbytesview Left, xbytesview Right)
{
	if ( Left.Size != Right.Size ) {
		return false;
	}
	for ( size_t i = 0; i < Left.Size; i++ ) {
		if ( __xrtX509AsciiLower(Left.Data[i]) !=
			__xrtX509AsciiLower(Right.Data[i]) ) {
			return false;
		}
	}
	return true;
}



/* 校验 DNS 名称并返回去除一个根点后的长度和通配符状态。 */
bool __xrtX509DnsName(
	xbytesview Name,
	bool bPattern,
	size_t* pSize,
	bool* pWildcard
)
{
	size_t iSize = Name.Size;
	size_t iLabel = 0;
	size_t iLabelIndex = 0;
	bool bWildcard = false;

	if ( (Name.Data == NULL) || (pSize == NULL) ||
		(pWildcard == NULL) || (iSize == 0) ) {
		return false;
	}
	if ( Name.Data[iSize - 1u] == (uint8)'.' ) {
		iSize--;
	}
	if ( (iSize == 0) || (iSize > 253u) ) {
		return false;
	}
	for ( size_t i = 0; i <= iSize; i++ ) {
		uint8 iValue;

		if ( (i == iSize) || (Name.Data[i] == (uint8)'.') ) {
			size_t iLength = i - iLabel;

			if ( (iLength == 0) || (iLength > 63u) ) {
				return false;
			}
			if ( (iLength == 1u) && (Name.Data[iLabel] == (uint8)'*') ) {
				if ( !bPattern || bWildcard || (iLabelIndex != 0) ) {
					return false;
				}
				bWildcard = true;
			} else if ( (Name.Data[iLabel] == (uint8)'-') ||
				(Name.Data[i - 1u] == (uint8)'-') ) {
				return false;
			}
			iLabel = i + 1u;
			iLabelIndex++;
			continue;
		}
		iValue = Name.Data[i];
		if ( ((iValue >= (uint8)'a') && (iValue <= (uint8)'z')) ||
			((iValue >= (uint8)'A') && (iValue <= (uint8)'Z')) ||
			((iValue >= (uint8)'0') && (iValue <= (uint8)'9')) ||
			(iValue == (uint8)'-') ) {
			continue;
		}
		if ( (iValue != (uint8)'*') || !bPattern || (i != iLabel) ||
			(iLabelIndex != 0) || (((i + 1u) < iSize) &&
			 (Name.Data[i + 1u] != (uint8)'.')) ) {
			return false;
		}
	}
	if ( bWildcard && (iLabelIndex < 2u) ) {
		return false;
	}
	*pSize = iSize;
	*pWildcard = bWildcard;
	return true;
}



/* 解析普通或 NameConstraints 基点使用的 GeneralName。 */
bool __xrtX509GeneralNameValue(
	const xdervalue* pValue,
	bool bConstraint,
	xx509genname* pName,
	cstr sOperation
)
{
	xx509genname Name;
	bool bConstructed;
	const xerror* pCause = NULL;

	if ( (pValue->Tag.Class != XASN1_CONTEXT) ||
		(pValue->Tag.Number > 8u) ) {
		goto Invalid;
	}
	Name.Type = (xx509gennametype)pValue->Tag.Number;
	Name.Raw = pValue->Raw;
	Name.Value = pValue->Value;
	bConstructed = (Name.Type == X509_NAME_OTHER) ||
		(Name.Type == X509_NAME_X400) ||
		(Name.Type == X509_NAME_DIRECTORY) ||
		(Name.Type == X509_NAME_EDI);
	if ( pValue->Tag.Constructed != bConstructed ) {
		goto Invalid;
	}
	if ( (Name.Type == X509_NAME_EMAIL) ||
		(Name.Type == X509_NAME_DNS) || (Name.Type == X509_NAME_URI) ) {
		if ( !__xrtX509ProfileIa5(Name.Value) ) {
			goto Invalid;
		}
	} else if ( Name.Type == X509_NAME_IP ) {
		size_t iIpv4 = bConstraint ? 8u : 4u;
		size_t iIpv6 = bConstraint ? 32u : 16u;

		if ( (Name.Value.Size != iIpv4) && (Name.Value.Size != iIpv6) ) {
			goto Invalid;
		}
	} else if ( Name.Type == X509_NAME_REGISTERED_ID ) {
		if ( !__xrtX509ProfileOidContent(Name.Value.Data, Name.Value.Size) ) {
			goto Invalid;
		}
	} else if ( Name.Type == X509_NAME_DIRECTORY ) {
		xx509namecursor Cursor;

		if ( !xrtX509NameInit(Name.Value, &Cursor) ) {
			pCause = xrtGetError();
			goto Invalid;
		}
	} else if ( Name.Value.Size == 0 ) {
		goto Invalid;
	}
	*pName = Name;
	return true;

Invalid:
	__xrtX509Error(
		XERR_PROTOCOL, X509_ERROR_GENERAL_NAME, sOperation,
		"GeneralName has an invalid tag, form or content", SIZE_MAX, pCause
	);
	return false;
}



/* 从 GeneralNames 的 SEQUENCE 内容初始化并完整校验游标。 */
bool __xrtX509GeneralNamesContent(
	xbytesview Content,
	xx509gencursor* pCursor,
	cstr sOperation
)
{
	xx509gencursor Cursor;
	xdercursor Check;
	xdervalue Value;
	size_t iCount = 0;
	const xerror* pCause = NULL;

	if ( !xrtDerInit(&Cursor.Items, Content.Data, Content.Size) ) {
		pCause = xrtGetError();
		goto Invalid;
	}
	Check = Cursor.Items;
	while ( true ) {
		xderresult Result = xrtDerRead(&Check, &Value);
		xx509genname Name;

		if ( Result == XDER_DONE ) {
			break;
		}
		if ( Result == XDER_ERROR ) {
			pCause = xrtGetError();
			goto Invalid;
		}
		if ( !__xrtX509GeneralNameValue(
			&Value, false, &Name, sOperation
		) ) {
			return false;
		}
		iCount++;
	}
	if ( iCount == 0 ) {
		goto Invalid;
	}
	*pCursor = Cursor;
	return true;

Invalid:
	__xrtX509Error(
		XERR_PROTOCOL, X509_ERROR_GENERAL_NAME, sOperation,
		"GeneralNames must contain at least one valid name", SIZE_MAX, pCause
	);
	return false;
}



/* 从一项完整 GeneralNames DER 初始化游标。 */
XRT_API bool xrtX509GeneralNameInit(
	xbytesview Names,
	xx509gencursor* pCursor
)
{
	xdervalue Sequence;
	xx509gencursor Cursor;

	if ( (pCursor == NULL) || ((Names.Data == NULL) && (Names.Size != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtX509RootValue(
		Names, &Sequence, X509_ERROR_GENERAL_NAME, "x509-general-names",
		"GeneralNames is not one valid DER sequence"
	) ) {
		return false;
	}
	if ( !xrtDerIs(
		&Sequence, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
	) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_GENERAL_NAME,
			"x509-general-names",
			"GeneralNames is not one valid DER sequence", SIZE_MAX, NULL
		);
		return false;
	}
	if ( !__xrtX509GeneralNamesContent(
		Sequence.Value, &Cursor, "x509-general-names"
	) ) {
		return false;
	}
	*pCursor = Cursor;
	return true;
}



/* 读取下一个 GeneralName，并验证其上下文形式和基本内容。 */
XRT_API xx509result xrtX509GeneralNameRead(
	xx509gencursor* pCursor,
	xx509genname* pName
)
{
	xx509gencursor Cursor;
	xdervalue Value;
	xderresult Result;
	xx509genname Name;

	if ( (pCursor == NULL) || (pName == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	Cursor = *pCursor;
	Result = xrtDerRead(&Cursor.Items, &Value);
	if ( Result == XDER_DONE ) {
		return X509_DONE;
	}
	if ( Result == XDER_ERROR ) {
		const xerror* pCause = xrtGetError();

		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_GENERAL_NAME,
			"x509-general-name-read",
			"GeneralNames cursor contains malformed DER", SIZE_MAX, pCause
		);
		return X509_ERROR;
	}
	if ( !__xrtX509GeneralNameValue(
		&Value, false, &Name, "x509-general-name-read"
	) ) {
		return X509_ERROR;
	}
	*pCursor = Cursor;
	*pName = Name;
	return X509_VALUE;
}



/* 初始化证书的一项 GeneralNames 扩展。 */
static xx509result __xrtX509AltName(
	const xx509cert* pCert,
	const uint8* pOid,
	size_t iOidSize,
	xx509gencursor* pCursor
)
{
	xx509ext Extension;
	xx509result Result;
	xx509gencursor Cursor;

	if ( pCursor == NULL ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	Result = __xrtX509ProfileExtension(
		pCert, pOid, iOidSize, &Extension
	);
	if ( Result != X509_VALUE ) {
		return Result;
	}
	if ( !xrtX509GeneralNameInit(Extension.Value, &Cursor) ) {
		return X509_ERROR;
	}
	*pCursor = Cursor;
	return X509_VALUE;
}



/* 初始化 SubjectAltName；扩展不存在时返回 X509_DONE。 */
XRT_API xx509result xrtX509SubjectAltName(
	const xx509cert* pCert,
	xx509gencursor* pCursor
)
{
	return __xrtX509AltName(
		pCert, __xrtX509OidSubjectAltName,
		__xrtX509OidSubjectAltNameSize, pCursor
	);
}



/* 初始化 IssuerAltName；扩展不存在时返回 X509_DONE。 */
XRT_API xx509result xrtX509IssuerAltName(
	const xx509cert* pCert,
	xx509gencursor* pCursor
)
{
	return __xrtX509AltName(
		pCert, __xrtX509OidIssuerAltName,
		__xrtX509OidIssuerAltNameSize, pCursor
	);
}



/* 读取 KeyUsage；扩展不存在时返回 X509_DONE。 */
XRT_API xx509result xrtX509KeyUsage(
	const xx509cert* pCert,
	uint16* pUsage
)
{
	xx509ext Extension;
	xx509result Result;
	xdervalue BitString;
	xbytesview Bits;
	uint8 iUnused;
	uint16 iUsage = 0;
	const xerror* pCause;

	if ( pUsage == NULL ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	Result = __xrtX509ProfileExtension(
		pCert, __xrtX509OidKeyUsage, __xrtX509OidKeyUsageSize, &Extension
	);
	if ( Result != X509_VALUE ) {
		return Result;
	}
	if ( !__xrtX509RootValue(
		Extension.Value, &BitString, X509_ERROR_KEY_USAGE,
		"x509-key-usage", "KeyUsage is not one valid DER BIT STRING"
	) ) {
		return X509_ERROR;
	}
	if ( !xrtDerBitString(&BitString, &Bits, &iUnused) ) {
		pCause = xrtGetError();
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_KEY_USAGE, "x509-key-usage",
			"KeyUsage is not a DER BIT STRING", SIZE_MAX, pCause
		);
		return X509_ERROR;
	}
	if ( (Bits.Size == 0) || (Bits.Size > 2u) ||
		((Bits.Size == 2u) && ((Bits.Data[1] & UINT8_C(0x7F)) != 0)) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_KEY_USAGE, "x509-key-usage",
			"KeyUsage contains invalid bits", SIZE_MAX, NULL
		);
		return X509_ERROR;
	}
	for ( size_t i = 0; i < 9u; i++ ) {
		size_t iByte = i / 8u;
		uint8 iMask = (uint8)(UINT8_C(0x80) >> (i % 8u));

		if ( (iByte < Bits.Size) && ((Bits.Data[iByte] & iMask) != 0) ) {
			iUsage |= (uint16)(UINT16_C(1) << i);
		}
	}
	if ( (iUsage == 0) ||
		(((iUsage & (X509_USAGE_ENCIPHER_ONLY | X509_USAGE_DECIPHER_ONLY)) != 0) &&
		 ((iUsage & X509_USAGE_KEY_AGREEMENT) == 0)) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_KEY_USAGE, "x509-key-usage",
			"KeyUsage is empty or uses agreement-only bits without keyAgreement",
			SIZE_MAX, NULL
		);
		return X509_ERROR;
	}
	{
		size_t iHighest = 8u;
		size_t iExpectedBytes;
		uint8 iExpectedUnused;

		while ( (iUsage & (uint16)(UINT16_C(1) << iHighest)) == 0 ) {
			iHighest--;
		}
		iExpectedBytes = (iHighest / 8u) + 1u;
		iExpectedUnused = (uint8)(7u - (iHighest % 8u));
		if ( (Bits.Size != iExpectedBytes) ||
			(iUnused != iExpectedUnused) ) {
			__xrtX509Error(
				XERR_PROTOCOL, X509_ERROR_KEY_USAGE, "x509-key-usage",
				"KeyUsage named bits are not minimally encoded", SIZE_MAX, NULL
			);
			return X509_ERROR;
		}
	}
	*pUsage = iUsage;
	return X509_VALUE;
}



/* 读取 BasicConstraints；扩展不存在时返回 X509_DONE。 */
XRT_API xx509result xrtX509BasicConstraints(
	const xx509cert* pCert,
	xx509basicconstraints* pConstraints
)
{
	xx509ext Extension;
	xx509result Result;
	xdervalue Sequence;
	xdercursor Fields;
	xdervalue Field;
	xderresult FieldResult;
	xx509basicconstraints Constraints;
	uint64 iPathLimit;
	const xerror* pCause = NULL;

	if ( pConstraints == NULL ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	Result = __xrtX509ProfileExtension(
		pCert, __xrtX509OidBasicConstraints,
		__xrtX509OidBasicConstraintsSize, &Extension
	);
	if ( Result != X509_VALUE ) {
		return Result;
	}
	memset(&Constraints, 0, sizeof(Constraints));
	if ( !__xrtX509RootValue(
		Extension.Value, &Sequence, X509_ERROR_BASIC_CONSTRAINTS,
		"x509-basic-constraints",
		"BasicConstraints is not one valid DER sequence"
	) ) {
		return X509_ERROR;
	}
	if ( !xrtDerIs(
		&Sequence, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
	) || !xrtDerEnter(&Sequence, &Fields) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_BASIC_CONSTRAINTS,
			"x509-basic-constraints",
			"BasicConstraints is not one valid DER sequence", SIZE_MAX, NULL
		);
		return X509_ERROR;
	}
	FieldResult = xrtDerRead(&Fields, &Field);
	if ( FieldResult == XDER_ERROR ) {
		pCause = xrtGetError();
		goto Invalid;
	}
	if ( FieldResult == XDER_VALUE ) {
		if ( xrtDerIs(
			&Field, XASN1_UNIVERSAL, (uint32)XASN1_BOOLEAN, false
		) ) {
			if ( !xrtDerBoolean(&Field, &Constraints.CA) ) {
				pCause = xrtGetError();
				goto Invalid;
			}
			if ( !Constraints.CA ) {
				goto Invalid;
			}
			FieldResult = xrtDerRead(&Fields, &Field);
			if ( FieldResult == XDER_ERROR ) {
				pCause = xrtGetError();
				goto Invalid;
			}
			if ( FieldResult == XDER_DONE ) {
				*pConstraints = Constraints;
				return X509_VALUE;
			}
		}
		if ( !xrtDerUInt64(&Field, &iPathLimit) ) {
			pCause = xrtGetError();
			goto Invalid;
		}
		if ( iPathLimit > UINT32_MAX ) {
			goto Invalid;
		}
		Constraints.HasPathLimit = true;
		Constraints.PathLimit = (uint32)iPathLimit;
	}
	if ( !xrtDerDone(&Fields) || (Constraints.HasPathLimit && !Constraints.CA) ) {
		goto Invalid;
	}
	*pConstraints = Constraints;
	return X509_VALUE;

Invalid:
	__xrtX509Error(
		XERR_PROTOCOL, X509_ERROR_BASIC_CONSTRAINTS,
		"x509-basic-constraints",
		"BasicConstraints has invalid defaults, path limit or trailing fields",
		SIZE_MAX, pCause
	);
	return X509_ERROR;
}



/* 从一项完整 SEQUENCE OF OBJECT IDENTIFIER 初始化游标。 */
XRT_API bool xrtX509OidInit(
	xbytesview Oids,
	xx509oidcursor* pCursor
)
{
	xdervalue Sequence;
	xx509oidcursor Cursor;
	xdercursor Check;
	xdervalue Value;
	size_t iCount = 0;
	const xerror* pCause = NULL;

	if ( (pCursor == NULL) || ((Oids.Data == NULL) && (Oids.Size != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtX509RootValue(
		Oids, &Sequence, X509_ERROR_OID_LIST, "x509-oids",
		"OID list is not one valid DER sequence"
	) ) {
		return false;
	}
	if ( !xrtDerIs(
		&Sequence, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
	) || !xrtDerEnter(&Sequence, &Cursor.Items) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_OID_LIST, "x509-oids",
			"OID list is not one valid DER sequence", SIZE_MAX, NULL
		);
		return false;
	}
	Check = Cursor.Items;
	while ( xrtDerRead(&Check, &Value) == XDER_VALUE ) {
		xbytesview Oid;
		xdercursor Previous = Cursor.Items;
		xdervalue Prior;

		if ( !xrtDerOid(&Value, &Oid) ) {
			pCause = xrtGetError();
			goto Invalid;
		}
		while ( xrtDerRead(&Previous, &Prior) == XDER_VALUE ) {
			xbytesview PriorOid;

			if ( Prior.Raw.Data == Value.Raw.Data ) {
				break;
			}
			if ( !xrtDerOid(&Prior, &PriorOid) ) {
				pCause = xrtGetError();
				goto Invalid;
			}
			if ( (PriorOid.Size == Oid.Size) &&
				(memcmp(PriorOid.Data, Oid.Data, Oid.Size) == 0) ) {
				goto Invalid;
			}
		}
		iCount++;
	}
	if ( !xrtDerDone(&Check) || (iCount == 0) ) {
		goto Invalid;
	}
	*pCursor = Cursor;
	return true;

Invalid:
	__xrtX509Error(
		XERR_PROTOCOL, X509_ERROR_OID_LIST, "x509-oids",
		"OID list is empty, malformed or contains duplicates", SIZE_MAX, pCause
	);
	return false;
}



/* 读取下一个 OID 内容八位组。 */
XRT_API xx509result xrtX509OidRead(
	xx509oidcursor* pCursor,
	xbytesview* pOid
)
{
	xx509oidcursor Cursor;
	xdervalue Value;
	xderresult Result;
	xbytesview Oid;

	if ( (pCursor == NULL) || (pOid == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	Cursor = *pCursor;
	Result = xrtDerRead(&Cursor.Items, &Value);
	if ( Result == XDER_DONE ) {
		return X509_DONE;
	}
	if ( Result == XDER_ERROR ) {
		const xerror* pCause = xrtGetError();

		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_OID_LIST, "x509-oid-read",
			"OID cursor contains malformed DER", SIZE_MAX, pCause
		);
		return X509_ERROR;
	}
	if ( !xrtDerOid(&Value, &Oid) ) {
		const xerror* pCause = xrtGetError();

		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_OID_LIST, "x509-oid-read",
			"OID cursor item is not an OBJECT IDENTIFIER", SIZE_MAX, pCause
		);
		return X509_ERROR;
	}
	*pCursor = Cursor;
	*pOid = Oid;
	return X509_VALUE;
}



/* 初始化 ExtendedKeyUsage；扩展不存在时返回 X509_DONE。 */
XRT_API xx509result xrtX509ExtendedKeyUsage(
	const xx509cert* pCert,
	xx509oidcursor* pCursor
)
{
	xx509ext Extension;
	xx509result Result;
	xx509oidcursor Cursor;

	if ( pCursor == NULL ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	Result = __xrtX509ProfileExtension(
		pCert, __xrtX509OidExtendedKeyUsage,
		__xrtX509OidExtendedKeyUsageSize, &Extension
	);
	if ( Result != X509_VALUE ) {
		return Result;
	}
	if ( !xrtX509OidInit(Extension.Value, &Cursor) ) {
		const xerror* pCause = xrtGetError();

		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_EXTENDED_KEY_USAGE,
			"x509-extended-key-usage",
			"ExtendedKeyUsage contains an invalid OID list",
			SIZE_MAX, pCause
		);
		return X509_ERROR;
	}
	*pCursor = Cursor;
	return X509_VALUE;
}



/* 从独立 DER OCTET STRING 解析 SubjectKeyIdentifier。 */
XRT_API bool xrtX509SubjectKeyIdParse(
	xbytesview Der,
	xbytesview* pKeyId
)
{
	xdervalue Value;
	xbytesview KeyId;
	const xerror* pCause;

	if ( (pKeyId == NULL) || ((Der.Data == NULL) && (Der.Size != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtX509RootValue(
		Der, &Value, X509_ERROR_KEY_IDENTIFIER,
		"x509-subject-key-id",
		"SubjectKeyIdentifier is not one DER OCTET STRING"
	) ) {
		return false;
	}
	if ( !xrtDerOctets(&Value, &KeyId) ) {
		pCause = xrtGetError();
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_KEY_IDENTIFIER,
			"x509-subject-key-id",
			"SubjectKeyIdentifier is not an OCTET STRING",
			SIZE_MAX, pCause
		);
		return false;
	}
	if ( KeyId.Size == 0 ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_KEY_IDENTIFIER,
			"x509-subject-key-id",
			"SubjectKeyIdentifier must be a nonempty OCTET STRING",
			SIZE_MAX, NULL
		);
		return false;
	}
	*pKeyId = KeyId;
	return true;
}



/* 读取 SubjectKeyIdentifier；扩展不存在时返回 X509_DONE。 */
XRT_API xx509result xrtX509SubjectKeyId(
	const xx509cert* pCert,
	xbytesview* pKeyId
)
{
	xx509ext Extension;
	xx509result Result;
	xbytesview KeyId;

	if ( pKeyId == NULL ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	Result = __xrtX509ProfileExtension(
		pCert, __xrtX509OidSubjectKeyId,
		__xrtX509OidSubjectKeyIdSize, &Extension
	);
	if ( Result != X509_VALUE ) {
		return Result;
	}
	if ( Extension.Critical ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_KEY_IDENTIFIER,
			"x509-subject-key-id",
			"SubjectKeyIdentifier must be non-critical", SIZE_MAX, NULL
		);
		return X509_ERROR;
	}
	if ( !xrtX509SubjectKeyIdParse(Extension.Value, &KeyId) ) {
		return X509_ERROR;
	}
	*pKeyId = KeyId;
	return X509_VALUE;
}



/* 从独立 DER SEQUENCE 解析 AuthorityKeyIdentifier。 */
XRT_API bool xrtX509AuthorityKeyIdParse(
	xbytesview Der,
	xx509authoritykeyid* pIdentifier
)
{
	xdervalue Sequence;
	xdercursor Fields;
	xdervalue Field;
	xx509authoritykeyid Identifier;
	uint32 iNext = 0;
	const xerror* pCause = NULL;

	if ( (pIdentifier == NULL) || ((Der.Data == NULL) && (Der.Size != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(&Identifier, 0, sizeof(Identifier));
	if ( !__xrtX509RootValue(
		Der, &Sequence, X509_ERROR_KEY_IDENTIFIER,
		"x509-authority-key-id",
		"AuthorityKeyIdentifier is not one DER sequence"
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
			(Field.Tag.Number > 2u) || (Field.Tag.Number < iNext) ||
			(Field.Tag.Constructed != (Field.Tag.Number == 1u)) ) {
			goto Invalid;
		}
		iNext = Field.Tag.Number + 1u;
		if ( Field.Tag.Number == 0u ) {
			if ( Field.Value.Size == 0 ) {
				goto Invalid;
			}
			Identifier.HasKeyId = true;
			Identifier.KeyId = Field.Value;
		} else if ( Field.Tag.Number == 1u ) {
			if ( !__xrtX509GeneralNamesContent(
				Field.Value, &Identifier.Issuer,
				"x509-authority-key-id"
			) ) {
				pCause = xrtGetError();
				goto Invalid;
			}
			Identifier.HasIssuer = true;
		} else {
			xbytesview Serial = Field.Value;

			if ( (Serial.Size == 0) || ((Serial.Size > 1u) &&
				(((Serial.Data[0] == 0) &&
				  ((Serial.Data[1] & UINT8_C(0x80)) == 0)) ||
				 ((Serial.Data[0] == UINT8_C(0xFF)) &&
				  ((Serial.Data[1] & UINT8_C(0x80)) != 0)))) ) {
				goto Invalid;
			}
			Identifier.HasSerial = true;
			Identifier.Serial = Serial;
		}
	}
	if ( (!Identifier.HasKeyId && !Identifier.HasIssuer) ||
		(Identifier.HasIssuer != Identifier.HasSerial) ) {
		goto Invalid;
	}
	*pIdentifier = Identifier;
	return true;

Invalid:
	__xrtX509Error(
		XERR_PROTOCOL, X509_ERROR_KEY_IDENTIFIER,
		"x509-authority-key-id",
		"AuthorityKeyIdentifier has invalid fields, order or issuer/serial pair",
		SIZE_MAX, pCause
	);
	return false;
}



/* 读取 AuthorityKeyIdentifier；扩展不存在时返回 X509_DONE。 */
XRT_API xx509result xrtX509AuthorityKeyId(
	const xx509cert* pCert,
	xx509authoritykeyid* pIdentifier
)
{
	xx509ext Extension;
	xx509result Result;
	xx509authoritykeyid Identifier;

	if ( pIdentifier == NULL ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	Result = __xrtX509ProfileExtension(
		pCert, __xrtX509OidAuthorityKeyId,
		__xrtX509OidAuthorityKeyIdSize, &Extension
	);
	if ( Result != X509_VALUE ) {
		return Result;
	}
	if ( Extension.Critical ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_KEY_IDENTIFIER,
			"x509-authority-key-id",
			"AuthorityKeyIdentifier must be non-critical", SIZE_MAX, NULL
		);
		return X509_ERROR;
	}
	if ( !xrtX509AuthorityKeyIdParse(Extension.Value, &Identifier) ) {
		return X509_ERROR;
	}
	*pIdentifier = Identifier;
	return X509_VALUE;
}

#endif
