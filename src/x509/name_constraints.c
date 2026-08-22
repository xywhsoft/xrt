#include "../internal/xrt_x509.h"



#if defined(XRT_FEATURE_X509_NAME_CONSTRAINTS)

/* 设置 NameConstraints 层统一错误。 */
void __xrtX509NameConstraintsError(
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	__xrtX509Error(
		Kind, X509_ERROR_NAME_CONSTRAINTS, sOperation,
		sMessage, SIZE_MAX, pCause
	);
}



/* 解析上下文隐式的非负 BaseDistance，并返回去除符号零的视图。 */
static bool __xrtX509ConstraintDistance(
	const xdervalue* pValue,
	uint32 iTag,
	bool bDefault,
	xbytesview* pDistance
)
{
	xbytesview Distance;

	if ( !xrtDerIs(pValue, XASN1_CONTEXT, iTag, false) ||
		(pValue->Value.Size == 0) ||
		((pValue->Value.Data[0] & UINT8_C(0x80)) != 0) ||
		((pValue->Value.Size > 1u) &&
		 (pValue->Value.Data[0] == 0) &&
		 ((pValue->Value.Data[1] & UINT8_C(0x80)) == 0)) ) {
		return false;
	}
	Distance = pValue->Value;
	if ( Distance.Data[0] == 0 ) {
		Distance.Data++;
		Distance.Size--;
	}
	if ( bDefault && (Distance.Size == 0) ) {
		return false;
	}
	*pDistance = Distance;
	return true;
}



/* 解析一项已经通过整体 DER 校验的 GeneralSubtree。 */
static bool __xrtX509SubtreeValue(
	const xdervalue* pValue,
	xx509subtree* pSubtree,
	cstr sOperation
)
{
	xdercursor Fields;
	xdervalue Field;
	xx509subtree Subtree;
	uint32 iNext = 0;
	const xerror* pCause = NULL;

	memset(&Subtree, 0, sizeof(Subtree));
	if ( (pValue == NULL) || (pSubtree == NULL) || (sOperation == NULL) ||
		!xrtDerIs(
			pValue, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true
		) ) {
		goto Invalid;
	}
	if ( !xrtDerEnter(pValue, &Fields) ) {
		pCause = xrtGetError();
		goto Invalid;
	}
	{
		xderresult Result = xrtDerRead(&Fields, &Field);

		if ( Result != XDER_VALUE ) {
			pCause = Result == XDER_ERROR ? xrtGetError() : NULL;
			goto Invalid;
		}
	}
	if ( !__xrtX509GeneralNameValue(
			&Field, true, &Subtree.Base, sOperation
		) || !__xrtX509ConstraintBaseValid(&Subtree.Base, sOperation) ) {
		pCause = xrtGetError();
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
			(Field.Tag.Number > 1u) || (Field.Tag.Number < iNext) ) {
			goto Invalid;
		}
		iNext = Field.Tag.Number + 1u;
		if ( Field.Tag.Number == 0u ) {
			if ( !__xrtX509ConstraintDistance(
				&Field, 0u, true, &Subtree.Minimum
			) ) {
				goto Invalid;
			}
			Subtree.HasMinimum = true;
		} else {
			if ( !__xrtX509ConstraintDistance(
				&Field, 1u, false, &Subtree.Maximum
			) ) {
				goto Invalid;
			}
			Subtree.HasMaximum = true;
		}
	}
	Subtree.Raw = pValue->Raw;
	*pSubtree = Subtree;
	return true;

Invalid:
	__xrtX509NameConstraintsError(
		XERR_PROTOCOL, sOperation,
		"GeneralSubtree has an invalid base, distance or field order",
		pCause
	);
	return false;
}



/* 初始化并完整校验一项隐式 GeneralSubtrees 内容。 */
static bool __xrtX509SubtreesInit(
	xbytesview Content,
	xx509subtreecursor* pCursor,
	cstr sOperation
)
{
	xx509subtreecursor Cursor;
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
		xx509subtree Subtree;

		if ( Result == XDER_DONE ) {
			break;
		}
		if ( Result == XDER_ERROR ) {
			pCause = xrtGetError();
			goto Invalid;
		}
		if ( !__xrtX509SubtreeValue(
			&Value, &Subtree, sOperation
		) ) {
			return false;
		}
		iCount++;
	}
	if ( (iCount == 0) || !xrtDerDone(&Check) ) {
		goto Invalid;
	}
	*pCursor = Cursor;
	return true;

Invalid:
	__xrtX509NameConstraintsError(
		XERR_PROTOCOL, sOperation,
		"GeneralSubtrees must contain at least one canonical subtree",
		pCause
	);
	return false;
}



/* 解析一项已经通过整体 DER 校验的 NameConstraints。 */
static bool __xrtX509NameConstraintsValue(
	const xdervalue* pValue,
	xx509nameconstraints* pConstraints,
	cstr sOperation
)
{
	xdercursor Fields;
	xdervalue Field;
	xx509nameconstraints Constraints;
	uint32 iNext = 0;
	const xerror* pCause = NULL;

	memset(&Constraints, 0, sizeof(Constraints));
	if ( !xrtDerIs(
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
		if ( (Field.Tag.Class != XASN1_CONTEXT) || !Field.Tag.Constructed ||
			(Field.Tag.Number > 1u) || (Field.Tag.Number < iNext) ) {
			goto Invalid;
		}
		iNext = Field.Tag.Number + 1u;
		if ( Field.Tag.Number == 0u ) {
			if ( !__xrtX509SubtreesInit(
				Field.Value, &Constraints.Permitted, sOperation
			) ) {
				return false;
			}
			Constraints.HasPermitted = true;
		} else {
			if ( !__xrtX509SubtreesInit(
				Field.Value, &Constraints.Excluded, sOperation
			) ) {
				return false;
			}
			Constraints.HasExcluded = true;
		}
	}
	if ( !Constraints.HasPermitted && !Constraints.HasExcluded ) {
		goto Invalid;
	}
	*pConstraints = Constraints;
	return true;

Invalid:
	__xrtX509NameConstraintsError(
		XERR_PROTOCOL, sOperation,
		"NameConstraints is empty, malformed, duplicated or out of order",
		pCause
	);
	return false;
}



/* 解析一项独立 NameConstraints DER，成功结果借用输入。 */
XRT_API bool xrtX509NameConstraintsParse(
	xbytesview Der,
	xx509nameconstraints* pConstraints
)
{
	xdervalue Sequence;
	xx509nameconstraints Constraints;

	if ( (pConstraints == NULL) ||
		((Der.Data == NULL) && (Der.Size != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtX509RootValue(
		Der, &Sequence, X509_ERROR_NAME_CONSTRAINTS,
		"x509-name-constraints",
		"NameConstraints is not one canonical DER value"
	) || !__xrtX509NameConstraintsValue(
		&Sequence, &Constraints, "x509-name-constraints"
	) ) {
		return false;
	}
	*pConstraints = Constraints;
	return true;
}



/* 读取证书的 critical NameConstraints；扩展不存在时返回 X509_DONE。 */
XRT_API xx509result xrtX509NameConstraints(
	const xx509cert* pCertificate,
	xx509nameconstraints* pConstraints
)
{
	xx509ext Extension;
	xx509nameconstraints Constraints;
	xx509result Result;

	if ( (pCertificate == NULL) || (pConstraints == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	Result = __xrtX509ExtensionFindValue(
		pCertificate->Extensions, __xrtX509OidNameConstraints,
		__xrtX509OidNameConstraintsSize, &Extension,
		"x509-name-constraints"
	);
	if ( Result != X509_VALUE ) {
		return Result;
	}
	if ( !Extension.Critical ) {
		__xrtX509NameConstraintsError(
			XERR_PROTOCOL, "x509-name-constraints",
			"NameConstraints must be critical", NULL
		);
		return X509_ERROR;
	}
	if ( !xrtX509NameConstraintsParse(Extension.Value, &Constraints) ) {
		return X509_ERROR;
	}
	*pConstraints = Constraints;
	return X509_VALUE;
}



/* 读取下一项 GeneralSubtree；失败时游标和输出保持不变。 */
XRT_API xx509result xrtX509SubtreeRead(
	xx509subtreecursor* pCursor,
	xx509subtree* pSubtree
)
{
	xx509subtreecursor Cursor;
	xdervalue Value;
	xderresult Result;
	xx509subtree Subtree;

	if ( (pCursor == NULL) || (pSubtree == NULL) ) {
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
	if ( !__xrtX509SubtreeValue(
		&Value, &Subtree, "x509-subtree-read"
	) ) {
		return X509_ERROR;
	}
	*pCursor = Cursor;
	*pSubtree = Subtree;
	return X509_VALUE;
}

#endif
