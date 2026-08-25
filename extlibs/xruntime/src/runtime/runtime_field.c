#include "../internal/xrt_runtime_type.h"
#include <xrt/runtime_field.h>



#if defined(XRUNTIME_FEATURE_RUNTIME_FIELD)

#define XRT_FIELD_FLAGS XRT_FIELD_FLAG_READONLY



/* 设置运行时字段模块结构化错误。 */
static void __xrtFieldError(
	xerrkind Kind,
	xfielderror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.field";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 为下层类型描述错误补充字段上下文并保留原始原因。 */
static void __xrtFieldWrap(
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : XERR_ARGUMENT;
	Desc.Domain = "xrt.field";
	Desc.Code = XFIELD_ERROR_DESCRIPTOR;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}



/* 检查字段表的借用数组形态。 */
static bool __xrtFieldTableShapeValid(const xrtfieldtable* pFields)
{
	return (pFields == NULL) ||
		(pFields->Count == 0u) ||
		(pFields->Fields != NULL);
}



/* 检查字段在声明类型中的边界、对齐和基础属性。 */
static bool __xrtFieldShapeValid(
	const xrttype* pOwner,
	const xrtfielddesc* pField
)
{
	size_t iMinimumOffset = pOwner->Base != NULL ?
		pOwner->Base->InstanceSize : 0u;

	if (
		!__xrtTypeViewValid(&pField->Name, false) ||
		(pField->Type == NULL) ||
		((pField->Flags & ~XRT_FIELD_FLAGS) != 0u) ||
		(pField->Offset < iMinimumOffset) ||
		(pField->Offset > pOwner->InstanceSize) ||
		(pField->Type->Align == 0u) ||
		((pField->Type->Align & (pField->Type->Align - 1u)) != 0u) ||
		(pOwner->InstanceAlign < pField->Type->Align) ||
		((pField->Offset % pField->Type->Align) != 0u)
	) {
		return false;
	}
	return pField->Type->Size <=
		(pOwner->InstanceSize - pField->Offset);
}



/* 判断两个非空存储区间是否重叠。 */
static bool __xrtFieldOverlaps(
	const xrtfielddesc* pLeft,
	const xrtfielddesc* pRight
)
{
	if ( (pLeft->Type->Size == 0u) || (pRight->Type->Size == 0u) ) {
		return false;
	}
	return (pLeft->Offset < (pRight->Offset + pRight->Type->Size)) &&
		(pRight->Offset < (pLeft->Offset + pLeft->Type->Size));
}



/* 验证一个声明类型的局部字段，不检查继承重名。 */
static bool __xrtFieldOwnerValidate(const xrttype* pOwner)
{
	const xrtfieldtable* pTable = pOwner->Fields;

	if ( !__xrtFieldTableShapeValid(pTable) ) {
		__xrtFieldError(XERR_ARGUMENT, XFIELD_ERROR_DESCRIPTOR,
			"validate", "a field table has no descriptor array");
		return false;
	}
	if ( pTable == NULL ) {
		return true;
	}
	for ( size_t i = 0; i < pTable->Count; i++ ) {
		const xrtfielddesc* pField = &pTable->Fields[i];

		if ( !__xrtFieldShapeValid(pOwner, pField) ) {
			__xrtFieldError(XERR_ARGUMENT, XFIELD_ERROR_DESCRIPTOR,
				"validate", "a field descriptor has an invalid name, layout, or flag");
			return false;
		}
		if ( !xrtTypeValidate(pField->Type) ) {
			__xrtFieldWrap("validate", "a field refers to an invalid runtime type");
			return false;
		}
		for ( size_t j = 0; j < i; j++ ) {
			const xrtfielddesc* pPrevious = &pTable->Fields[j];

			if ( __xrtTypeViewEqual(&pPrevious->Name, &pField->Name) ) {
				__xrtFieldError(XERR_EXISTS, XFIELD_ERROR_DESCRIPTOR,
					"validate", "field names must be unique within a type");
				return false;
			}
			if ( __xrtFieldOverlaps(pPrevious, pField) ) {
				__xrtFieldError(XERR_ARGUMENT, XFIELD_ERROR_DESCRIPTOR,
					"validate", "field storage ranges must not overlap");
				return false;
			}
		}
	}
	return true;
}



/* 在已经验证的基类链中查询字段名称。 */
static bool __xrtFieldBaseHasName(
	const xrttype* pBase,
	const xstrview* pName
)
{
	while ( pBase != NULL ) {
		const xrtfieldtable* pTable = pBase->Fields;

		if ( pTable != NULL ) {
			for ( size_t i = 0; i < pTable->Count; i++ ) {
				if ( __xrtTypeViewEqual(
					&pTable->Fields[i].Name, pName
				) ) {
					return true;
				}
			}
		}
		pBase = pBase->Base;
	}
	return false;
}



/* 返回继承链字段总数，并拒绝损坏的表或计数溢出。 */
static bool __xrtFieldCount(
	const xrttype* pType,
	size_t* pCount,
	cstr sOperation
)
{
	size_t iCount = 0u;
	uint32 iDepth = 0u;

	if ( (pType == NULL) || (pCount == NULL) ) {
		__xrtFieldError(XERR_ARGUMENT, XFIELD_ERROR_LOOKUP,
			sOperation, "the runtime type descriptor is null");
		return false;
	}
	while ( (pType != NULL) &&
			(iDepth < XRT_RUNTIME_TYPE_INHERITANCE_MAX) ) {
		const xrtfieldtable* pTable = pType->Fields;

		if ( !__xrtFieldTableShapeValid(pTable) ) {
			__xrtFieldError(XERR_ARGUMENT, XFIELD_ERROR_DESCRIPTOR,
				sOperation, "a field table has no descriptor array");
			return false;
		}
		if ( (pTable != NULL) &&
			 (pTable->Count > (SIZE_MAX - iCount)) ) {
			__xrtFieldError(XERR_RANGE, XFIELD_ERROR_DESCRIPTOR,
				sOperation, "the inherited field count overflows");
			return false;
		}
		if ( pTable != NULL ) {
			iCount += pTable->Count;
		}
		pType = pType->Base;
		iDepth++;
	}
	if ( pType != NULL ) {
		__xrtFieldError(XERR_ARGUMENT, XFIELD_ERROR_DESCRIPTOR,
			sOperation, "the field inheritance chain is cyclic or too deep");
		return false;
	}
	*pCount = iCount;
	return true;
}



/* 按准确描述符地址查找声明类型，不读取不受信任的字段内容。 */
static const xrttype* __xrtFieldOwner(
	const xrttype* pType,
	const xrtfielddesc* pField,
	cstr sOperation
)
{
	uint32 iDepth = 0u;

	if ( (pType == NULL) || (pField == NULL) ) {
		__xrtFieldError(XERR_ARGUMENT, XFIELD_ERROR_ACCESS,
			sOperation, "the runtime type or field descriptor is null");
		return NULL;
	}
	while ( (pType != NULL) &&
			(iDepth < XRT_RUNTIME_TYPE_INHERITANCE_MAX) ) {
		const xrtfieldtable* pTable = pType->Fields;

		if ( !__xrtFieldTableShapeValid(pTable) ) {
			__xrtFieldError(XERR_ARGUMENT, XFIELD_ERROR_DESCRIPTOR,
				sOperation, "a field table has no descriptor array");
			return NULL;
		}
		if ( pTable != NULL ) {
			for ( size_t i = 0; i < pTable->Count; i++ ) {
				if ( &pTable->Fields[i] == pField ) {
					return pType;
				}
			}
		}
		pType = pType->Base;
		iDepth++;
	}
	__xrtFieldError(XERR_ARGUMENT, XFIELD_ERROR_ACCESS,
		sOperation, pType != NULL ?
		"the field inheritance chain is cyclic or too deep" :
		"the field descriptor does not belong to the runtime type");
	return NULL;
}



/* 验证完整字段继承链，并禁止字段隐藏与基类负载重叠。 */
XRT_API bool xrtTypeFieldsValidate(const xrttype* pType)
{
	const xrttype* arrTypes[XRT_RUNTIME_TYPE_INHERITANCE_MAX];
	size_t iDepth = 0u;

	if ( !xrtTypeValidate(pType) ) {
		__xrtFieldWrap("validate", "the field owner type is invalid");
		return false;
	}
	if ( (pType->Kind != XRT_TYPE_CLASS) &&
		 (pType->Kind != XRT_TYPE_RECORD) ) {
		__xrtFieldError(XERR_ARGUMENT, XFIELD_ERROR_DESCRIPTOR,
			"validate", "only class and record types can declare fields");
		return false;
	}
	while ( pType != NULL ) {
		if ( iDepth >= XRT_RUNTIME_TYPE_INHERITANCE_MAX ) {
			__xrtFieldError(XERR_RANGE, XFIELD_ERROR_DESCRIPTOR,
				"validate", "the field inheritance chain exceeds the local depth limit");
			return false;
		}
		arrTypes[iDepth++] = pType;
		pType = pType->Base;
	}
	for ( size_t i = iDepth; i != 0u; i-- ) {
		const xrttype* pOwner = arrTypes[i - 1u];
		const xrtfieldtable* pTable = pOwner->Fields;

		if ( !__xrtFieldOwnerValidate(pOwner) ) {
			return false;
		}
		if ( (pTable == NULL) || (pOwner->Base == NULL) ) {
			continue;
		}
		for ( size_t j = 0; j < pTable->Count; j++ ) {
			if ( __xrtFieldBaseHasName(
				pOwner->Base, &pTable->Fields[j].Name
			) ) {
				__xrtFieldError(XERR_EXISTS, XFIELD_ERROR_DESCRIPTOR,
					"validate", "derived fields must not hide inherited fields");
				return false;
			}
		}
	}
	return true;
}



/* 返回继承链中的字段总数。 */
XRT_API size_t xrtTypeFieldCount(const xrttype* pType)
{
	size_t iCount;

	return __xrtFieldCount(pType, &iCount, "count") ? iCount : 0u;
}



/* 按基类优先顺序返回指定下标的字段。 */
XRT_API const xrtfielddesc* xrtTypeField(
	const xrttype* pType,
	size_t iIndex
)
{
	const xrttype* arrTypes[XRT_RUNTIME_TYPE_INHERITANCE_MAX];
	const xrttype* pCursor = pType;
	size_t iCount;
	size_t iDepth = 0u;

	if ( !__xrtFieldCount(pType, &iCount, "field") ) {
		return NULL;
	}
	if ( iIndex >= iCount ) {
		__xrtFieldError(XERR_RANGE, XFIELD_ERROR_LOOKUP,
			"field", "the field index is out of range");
		return NULL;
	}
	while ( pCursor != NULL ) {
		if ( iDepth >= XRT_RUNTIME_TYPE_INHERITANCE_MAX ) {
			__xrtFieldError(XERR_RANGE, XFIELD_ERROR_LOOKUP,
				"field", "the field inheritance chain exceeds the local depth limit");
			return NULL;
		}
		arrTypes[iDepth++] = pCursor;
		pCursor = pCursor->Base;
	}
	for ( size_t i = iDepth; i != 0u; i-- ) {
		const xrtfieldtable* pTable = arrTypes[i - 1u]->Fields;

		if ( pTable == NULL ) {
			continue;
		}
		if ( iIndex < pTable->Count ) {
			return &pTable->Fields[iIndex];
		}
		iIndex -= pTable->Count;
	}
	return NULL;
}



/* 从最具体类型开始按名称查询字段。 */
XRT_API const xrtfielddesc* xrtTypeFindField(
	const xrttype* pType,
	xstrview Name
)
{
	uint32 iDepth = 0u;

	if ( (pType == NULL) || !__xrtTypeViewValid(&Name, false) ) {
		__xrtFieldError(XERR_ARGUMENT, XFIELD_ERROR_LOOKUP,
			"find", "the runtime type or field name is invalid");
		return NULL;
	}
	while ( (pType != NULL) &&
			(iDepth < XRT_RUNTIME_TYPE_INHERITANCE_MAX) ) {
		const xrtfieldtable* pTable = pType->Fields;

		if ( !__xrtFieldTableShapeValid(pTable) ) {
			__xrtFieldError(XERR_ARGUMENT, XFIELD_ERROR_DESCRIPTOR,
				"find", "a field table has no descriptor array");
			return NULL;
		}
		if ( pTable != NULL ) {
			for ( size_t i = 0; i < pTable->Count; i++ ) {
				if ( __xrtTypeViewEqual(&pTable->Fields[i].Name, &Name) ) {
					return &pTable->Fields[i];
				}
			}
		}
		pType = pType->Base;
		iDepth++;
	}
	if ( pType != NULL ) {
		__xrtFieldError(XERR_ARGUMENT, XFIELD_ERROR_DESCRIPTOR,
			"find", "the field inheritance chain is cyclic or too deep");
	}
	return NULL;
}



/* 返回字段在继承链中的声明类型。 */
XRT_API const xrttype* xrtTypeFieldOwner(
	const xrttype* pType,
	const xrtfielddesc* pField
)
{
	return __xrtFieldOwner(pType, pField, "owner");
}



/* 检查字段归属和布局后返回实例内地址。 */
static const void* __xrtFieldData(
	const xrttype* pType,
	const xrtfielddesc* pField,
	const void* pInstance,
	cstr sOperation
)
{
	const xrttype* pOwner = __xrtFieldOwner(pType, pField, sOperation);

	if ( pOwner == NULL ) {
		return NULL;
	}
	if ( pInstance == NULL ) {
		__xrtFieldError(XERR_ARGUMENT, XFIELD_ERROR_ACCESS,
			sOperation, "the instance payload is null");
		return NULL;
	}
	if ( !__xrtFieldShapeValid(pOwner, pField) ) {
		__xrtFieldError(XERR_ARGUMENT, XFIELD_ERROR_DESCRIPTOR,
			sOperation, "the field layout is invalid");
		return NULL;
	}
	return (const uint8*)pInstance + pField->Offset;
}



/* 返回只读实例中的字段地址。 */
XRT_API const void* xrtFieldConstData(
	const xrttype* pType,
	const xrtfielddesc* pField,
	const void* pInstance
)
{
	return __xrtFieldData(pType, pField, pInstance, "const-data");
}



/* 返回可写实例中的字段地址；只读标志仍由上层策略解释。 */
XRT_API ptr xrtFieldData(
	const xrttype* pType,
	const xrtfielddesc* pField,
	ptr pInstance
)
{
	return (ptr)__xrtFieldData(pType, pField, pInstance, "data");
}

#endif
