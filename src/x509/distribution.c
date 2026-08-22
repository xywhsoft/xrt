#include "../internal/xrt_x509.h"



#if defined(XRT_FEATURE_X509_DISTRIBUTION)

/* 按 DER SET OF 排序规则比较两个完整 AttributeTypeAndValue。 */
static int __xrtX509DistributionDerCompare(
	xbytesview Left,
	xbytesview Right
)
{
	size_t iCommon = Left.Size < Right.Size ? Left.Size : Right.Size;
	int iOrder = memcmp(Left.Data, Right.Data, iCommon);

	if ( iOrder != 0 ) {
		return iOrder;
	}
	if ( Left.Size < Right.Size ) {
		return -1;
	}
	return Left.Size > Right.Size ? 1 : 0;
}



/* 验证隐式 RelativeDistinguishedName 内容及 DER SET OF 顺序。 */
static bool __xrtX509DistributionRelativeName(xbytesview Content)
{
	xdercursor Items;
	xdervalue Item;
	xbytesview Previous = { NULL, 0 };
	size_t iCount = 0;

	if ( !xrtDerInit(&Items, Content.Data, Content.Size) ) {
		return false;
	}
	while ( xrtDerRead(&Items, &Item) == XDER_VALUE ) {
		xdercursor Fields;
		xdervalue Type;
		xdervalue Value;
		xbytesview Oid;

		if ( !xrtDerIs(
			&Item, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
		) || !xrtDerEnter(&Item, &Fields) ||
			(xrtDerRead(&Fields, &Type) != XDER_VALUE) ||
			!xrtDerOid(&Type, &Oid) ||
			(xrtDerRead(&Fields, &Value) != XDER_VALUE) ||
			!xrtDerDone(&Fields) ) {
			return false;
		}
		if ( (iCount != 0) && (__xrtX509DistributionDerCompare(
			Previous, Item.Raw
		) > 0) ) {
			return false;
		}
		Previous = Item.Raw;
		iCount++;
	}
	return xrtDerDone(&Items) && (iCount != 0);
}



/* 解析 DistributionPoint 或 IDP 共用的显式 DistributionPointName 字段。 */
bool __xrtX509DistributionNameValue(
	const xdervalue* pField,
	xx509distributionname* pName,
	cstr sOperation
)
{
	xdercursor Wrapper;
	xdervalue Choice;
	xx509distributionname Name;
	const xerror* pCause = NULL;

	if ( (pField == NULL) || (pName == NULL) || (sOperation == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(&Name, 0, sizeof(Name));
	if ( !xrtDerIs(pField, XASN1_CONTEXT, 0u, true) ||
		!xrtDerInit(&Wrapper, pField->Value.Data, pField->Value.Size) ||
		(xrtDerRead(&Wrapper, &Choice) != XDER_VALUE) ||
		!xrtDerDone(&Wrapper) || (Choice.Tag.Class != XASN1_CONTEXT) ||
		(Choice.Tag.Number > 1u) || !Choice.Tag.Constructed ) {
		goto Invalid;
	}
	Name.Type = (xx509distributionnametype)Choice.Tag.Number;
	Name.Raw = Choice.Raw;
	Name.Value = Choice.Value;
	if ( Name.Type == X509_DISTRIBUTION_FULL_NAME ) {
		if ( !__xrtX509GeneralNamesContent(
			Choice.Value, &Name.FullNames, sOperation
		) ) {
			pCause = xrtGetError();
			goto Invalid;
		}
	} else if ( !__xrtX509DistributionRelativeName(Choice.Value) ) {
		goto Invalid;
	}
	*pName = Name;
	return true;

Invalid:
	__xrtX509Error(
		XERR_PROTOCOL, X509_ERROR_DISTRIBUTION_POINT, sOperation,
		"DistributionPointName has an invalid choice or relative name",
		SIZE_MAX, pCause
	);
	return false;
}



/* 解析上下文隐式 ReasonFlags，并转换为稳定的主机端位值。 */
bool __xrtX509ReasonFlagsValue(
	const xdervalue* pField,
	uint32 iTag,
	uint16* pReasons,
	cstr sOperation
)
{
	xbytesview Value;
	size_t iBits;
	uint8 iUnused;
	uint16 iReasons = 0;

	if ( (pField == NULL) || (pReasons == NULL) || (sOperation == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtDerIs(pField, XASN1_CONTEXT, iTag, false) ||
		(pField->Value.Size < 2u) ) {
		goto Invalid;
	}
	Value = pField->Value;
	iUnused = Value.Data[0];
	if ( iUnused > 7u ) {
		goto Invalid;
	}
	Value.Data++;
	Value.Size--;
	if ( (iUnused != 0) && ((Value.Data[Value.Size - 1u] &
		(uint8)((UINT16_C(1) << iUnused) - 1u)) != 0) ) {
		goto Invalid;
	}
	iBits = (Value.Size * 8u) - iUnused;
	if ( (iBits == 0) || (iBits > 9u) ||
		((Value.Data[Value.Size - 1u] &
		 (uint8)(UINT8_C(0x80) >> ((iBits - 1u) & 7u))) == 0) ) {
		goto Invalid;
	}
	for ( size_t i = 0; i < iBits; i++ ) {
		if ( (Value.Data[i >> 3u] &
			(uint8)(UINT8_C(0x80) >> (i & 7u))) != 0 ) {
			iReasons |= (uint16)(UINT16_C(1) << i);
		}
	}
	*pReasons = iReasons;
	return true;

Invalid:
	__xrtX509Error(
		XERR_PROTOCOL, X509_ERROR_DISTRIBUTION_POINT, sOperation,
		"ReasonFlags has an invalid tag, length or named-bit encoding",
		SIZE_MAX, NULL
	);
	return false;
}



/* 解析一项已经通过整体 DER 校验的 DistributionPoint。 */
static bool __xrtX509DistributionPointValue(
	const xdervalue* pValue,
	xx509distributionpoint* pPoint,
	cstr sOperation
)
{
	xdercursor Fields;
	xdervalue Field;
	xx509distributionpoint Point;
	uint32 iNext = 0;
	const xerror* pCause = NULL;

	memset(&Point, 0, sizeof(Point));
	if ( (pValue == NULL) || (pPoint == NULL) || !xrtDerIs(
		pValue, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
	) || !xrtDerEnter(pValue, &Fields) ) {
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
			(Field.Tag.Number > 2u) || (Field.Tag.Number < iNext) ) {
			goto Invalid;
		}
		iNext = Field.Tag.Number + 1u;
		if ( Field.Tag.Number == 0u ) {
			if ( !__xrtX509DistributionNameValue(
				&Field, &Point.Name, sOperation
			) ) {
				return false;
			}
			Point.HasName = true;
		} else if ( Field.Tag.Number == 1u ) {
			if ( !__xrtX509ReasonFlagsValue(
				&Field, 1u, &Point.Reasons, sOperation
			) ) {
				return false;
			}
			Point.HasReasons = true;
		} else {
			if ( !Field.Tag.Constructed || !__xrtX509GeneralNamesContent(
				Field.Value, &Point.Issuer, sOperation
			) ) {
				pCause = xrtGetError();
				goto Invalid;
			}
			Point.HasIssuer = true;
		}
	}
	if ( !Point.HasName && !Point.HasIssuer ) {
		goto Invalid;
	}
	Point.Raw = pValue->Raw;
	*pPoint = Point;
	return true;

Invalid:
	__xrtX509Error(
		XERR_PROTOCOL, X509_ERROR_DISTRIBUTION_POINT, sOperation,
		"DistributionPoint is empty, reasons-only, malformed or out of order",
		SIZE_MAX, pCause
	);
	return false;
}



/* 解析一项独立 DistributionPoint DER，成功结果借用输入。 */
XRT_API bool xrtX509DistributionPointParse(
	xbytesview Der,
	xx509distributionpoint* pPoint
)
{
	xdervalue Sequence;
	xx509distributionpoint Point;

	if ( (pPoint == NULL) || ((Der.Data == NULL) && (Der.Size != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtX509RootValue(
		Der, &Sequence, X509_ERROR_DISTRIBUTION_POINT,
		"x509-distribution-point",
		"DistributionPoint is not one canonical DER value"
	) || !__xrtX509DistributionPointValue(
		&Sequence, &Point, "x509-distribution-point"
	) ) {
		return false;
	}
	*pPoint = Point;
	return true;
}



/* 从完整 CRLDistributionPoints DER 初始化借用式游标。 */
XRT_API bool xrtX509DistributionInit(
	xbytesview Der,
	xx509distributioncursor* pCursor
)
{
	xdervalue Sequence;
	xx509distributioncursor Cursor;
	xdercursor Check;
	xdervalue PointValue;
	size_t iCount = 0;

	if ( (pCursor == NULL) || ((Der.Data == NULL) && (Der.Size != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtX509RootValue(
		Der, &Sequence, X509_ERROR_DISTRIBUTION_POINT,
		"x509-distribution-init",
		"CRLDistributionPoints is not one canonical DER sequence"
	) || !xrtDerIs(
		&Sequence, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
	) || !xrtDerEnter(&Sequence, &Cursor.Items) ) {
		return false;
	}
	Check = Cursor.Items;
	while ( xrtDerRead(&Check, &PointValue) == XDER_VALUE ) {
		xx509distributionpoint Point;

		if ( !__xrtX509DistributionPointValue(
			&PointValue, &Point, "x509-distribution-init"
		) ) {
			return false;
		}
		iCount++;
	}
	if ( !xrtDerDone(&Check) || (iCount == 0) ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_DISTRIBUTION_POINT,
			"x509-distribution-init",
			"CRLDistributionPoints must contain at least one valid point",
			SIZE_MAX, NULL
		);
		return false;
	}
	*pCursor = Cursor;
	return true;
}



/* 读取下一个 DistributionPoint；失败时游标和输出保持不变。 */
XRT_API xx509result xrtX509DistributionRead(
	xx509distributioncursor* pCursor,
	xx509distributionpoint* pPoint
)
{
	xx509distributioncursor Cursor;
	xdervalue Value;
	xderresult Result;
	xx509distributionpoint Point;

	if ( (pCursor == NULL) || (pPoint == NULL) ) {
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
	if ( !__xrtX509DistributionPointValue(
		&Value, &Point, "x509-distribution-read"
	) ) {
		return X509_ERROR;
	}
	*pCursor = Cursor;
	*pPoint = Point;
	return X509_VALUE;
}



/* 从证书扩展初始化指定类型的分发点列表。 */
static xx509result __xrtX509CertificateDistribution(
	const xx509cert* pCert,
	const uint8* pOid,
	size_t iOidSize,
	bool bRequireNonCritical,
	cstr sOperation,
	xx509distributioncursor* pCursor
)
{
	xx509ext Extension;
	xx509result Result;
	xx509distributioncursor Cursor;

	if ( (pCert == NULL) || (pCursor == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	Result = __xrtX509ExtensionFindValue(
		pCert->Extensions, pOid, iOidSize, &Extension, sOperation
	);
	if ( Result != X509_VALUE ) {
		return Result;
	}
	if ( bRequireNonCritical && Extension.Critical ) {
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_DISTRIBUTION_POINT, sOperation,
			"FreshestCRL must be non-critical", SIZE_MAX, NULL
		);
		return X509_ERROR;
	}
	if ( !xrtX509DistributionInit(Extension.Value, &Cursor) ) {
		return X509_ERROR;
	}
	*pCursor = Cursor;
	return X509_VALUE;
}



/* 初始化证书 CRLDistributionPoints；扩展不存在时返回 X509_DONE。 */
XRT_API xx509result xrtX509CrlPoints(
	const xx509cert* pCert,
	xx509distributioncursor* pCursor
)
{
	return __xrtX509CertificateDistribution(
		pCert, __xrtX509OidCrlDistribution,
		__xrtX509OidCrlDistributionSize, false,
		"x509-crl-points", pCursor
	);
}



/* 初始化证书 non-critical FreshestCRL；扩展不存在时返回 X509_DONE。 */
XRT_API xx509result xrtX509FreshestCrl(
	const xx509cert* pCert,
	xx509distributioncursor* pCursor
)
{
	return __xrtX509CertificateDistribution(
		pCert, __xrtX509OidFreshestCrl,
		__xrtX509OidFreshestCrlSize, true,
		"x509-freshest-crl", pCursor
	);
}

#endif
