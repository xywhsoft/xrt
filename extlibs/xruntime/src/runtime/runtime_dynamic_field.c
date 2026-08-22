#include "../internal/xrt_internal.h"
#include "../internal/xrt_runtime_object.h"
#include "../internal/xrt_runtime_value.h"
#include "../internal/xrt_typed_dict.h"
#include <xrt/runtime_field.h>



#if defined(XRUNTIME_FEATURE_RUNTIME_DYNAMIC_FIELD)

/* 动态字段的唯一泛型实参是拥有 xvalue 指针的运行时 Value 类型。 */
static const xrttype* const __xrtDynamicFieldArguments[] = {
	&__xrtTypeValueDescriptor
};



/* 动态字段表是可追踪引用对象，值语义统一复用对象强引用操作。 */
static const xrttype __xrtDynamicFieldTypeDescriptor = {
	.Id = UINT64_C(0x34E2328DABCB19D3),
	.Kind = XRT_TYPE_DICT,
	.Flags = XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_REFERENCE |
		XRT_TYPE_FLAG_NULLABLE | XRT_TYPE_FLAG_FINAL |
		XRT_TYPE_FLAG_RELOCATABLE,
	.Name = XRT_STR_INIT("DynamicFields"),
	.AbiName = XRT_STR_INIT("xrt.DynamicFields"),
	.Size = sizeof(xrtdynamicfields*),
	.Align = XRT_INTERNAL_ALIGNOF(xrtdynamicfields*),
	.InstanceSize = sizeof(xtypeddict),
	.InstanceAlign = XRT_INTERNAL_ALIGNOF(xtypeddict),
	.Ops = &__xrtObjectValueOperations,
	.InstanceOps = &__xrtTypedDictInstanceOperations,
	.ArgumentCount = 1u,
	.Arguments = __xrtDynamicFieldArguments
};



/* 设置动态字段模块结构化错误。 */
static void __xrtDynamicFieldError(
	xerrkind Kind,
	xdynamicfielderror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.dynamic-field";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 为下层对象、类型字典或 Value 错误补充动态字段上下文。 */
static void __xrtDynamicFieldWrap(
	xerrkind DefaultKind,
	xdynamicfielderror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : DefaultKind;
	Desc.Domain = "xrt.dynamic-field";
	Desc.Code = (int32)Code;
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



/* 验证对象的精确动态字段类型并返回可写字典载荷。 */
static xtypeddict* __xrtDynamicFieldDict(
	xrtdynamicfields* pFields,
	cstr sOperation
)
{
	if ( pFields == NULL ) {
		__xrtDynamicFieldError(XERR_ARGUMENT,
			XDYNAMIC_FIELD_ERROR_ARGUMENT, sOperation,
			"the dynamic field object is null");
		return NULL;
	}
	if ( xrtObjectType(pFields) != &__xrtDynamicFieldTypeDescriptor ) {
		__xrtDynamicFieldError(XERR_TYPE,
			XDYNAMIC_FIELD_ERROR_TYPE, sOperation,
			"the object is not an xrt.DynamicFields instance");
		return NULL;
	}
	return (xtypeddict*)xrtObjectData(pFields);
}



/* 验证对象的精确动态字段类型并返回只读字典载荷。 */
static const xtypeddict* __xrtDynamicFieldConstDict(
	const xrtdynamicfields* pFields,
	cstr sOperation
)
{
	if ( pFields == NULL ) {
		__xrtDynamicFieldError(XERR_ARGUMENT,
			XDYNAMIC_FIELD_ERROR_ARGUMENT, sOperation,
			"the dynamic field object is null");
		return NULL;
	}
	if ( xrtObjectType(pFields) != &__xrtDynamicFieldTypeDescriptor ) {
		__xrtDynamicFieldError(XERR_TYPE,
			XDYNAMIC_FIELD_ERROR_TYPE, sOperation,
			"the object is not an xrt.DynamicFields instance");
		return NULL;
	}
	return (const xtypeddict*)xrtObjectConstData(pFields);
}



/* 验证长度明确的字段名视图，允许空名称但拒绝悬空非空区间。 */
static bool __xrtDynamicFieldNameValid(
	xstrview Name,
	cstr sOperation
)
{
	if ( (Name.Data == NULL) && (Name.Size != 0u) ) {
		__xrtDynamicFieldError(XERR_ARGUMENT,
			XDYNAMIC_FIELD_ERROR_ARGUMENT, sOperation,
			"the dynamic field name view is invalid");
		return false;
	}
	return true;
}



/* 在清理临时对象期间保留原始失败。 */
static void __xrtDynamicFieldUnrefPreserveError(
	xrtdynamicfields* pFields
)
{
	xerror* pError = xrtTakeError();

	xrtObjectUnref(pFields);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 深复制一个 Value 图并失败原子地提交到字段字典。 */
static bool __xrtDynamicFieldSetClone(
	xtypeddict* pDict,
	xstrview Name,
	const xvalue* pValue,
	cstr sOperation,
	cstr sFailure
)
{
	xvalue* pCopy = xrtValueDeepClone(pValue);

	if ( pCopy == NULL ) {
		__xrtDynamicFieldWrap(XERR_MEMORY,
			XDYNAMIC_FIELD_ERROR_OPERATION, sOperation,
			"the dynamic field source could not be cloned");
		return false;
	}
	if ( !xrtTypedDictSetTake(pDict, Name, &pCopy) ) {
		xrtValueRelease(pCopy);
		__xrtDynamicFieldWrap(XERR_STATE,
			XDYNAMIC_FIELD_ERROR_OPERATION, sOperation, sFailure);
		return false;
	}
	xrtValueRelease(pCopy);
	return true;
}



/* 返回进程期稳定的动态字段运行时类型。 */
XRT_API const xrttype* xrtDynamicFieldsType(void)
{
	return &__xrtDynamicFieldTypeDescriptor;
}



/* 创建一个空动态字段对象。 */
XRT_API xrtdynamicfields* xrtDynamicFieldsCreate(void)
{
	xrtdynamicfields* pFields = xrtObjectCreate(
		&__xrtDynamicFieldTypeDescriptor
	);

	if ( pFields == NULL ) {
		__xrtDynamicFieldWrap(XERR_MEMORY,
			XDYNAMIC_FIELD_ERROR_OPERATION, "create",
			"the dynamic field object could not be created");
	}
	return pFields;
}



/* 保留一个动态字段对象。 */
XRT_API xrtdynamicfields* xrtDynamicFieldsRef(xrtdynamicfields* pFields)
{
	if ( __xrtDynamicFieldDict(pFields, "ref") == NULL ) {
		return NULL;
	}
	if ( xrtObjectRef(pFields) == NULL ) {
		__xrtDynamicFieldWrap(XERR_STATE,
			XDYNAMIC_FIELD_ERROR_STATE, "ref",
			"the dynamic field object could not be retained");
		return NULL;
	}
	return pFields;
}



/* 释放一个动态字段对象，允许空指针。 */
XRT_API void xrtDynamicFieldsUnref(xrtdynamicfields* pFields)
{
	if ( pFields == NULL ) {
		return;
	}
	if ( __xrtDynamicFieldDict(pFields, "unref") == NULL ) {
		return;
	}
	xrtObjectUnref(pFields);
}



/* 返回当前动态字段数量。 */
XRT_API size_t xrtDynamicFieldsCount(const xrtdynamicfields* pFields)
{
	const xtypeddict* pDict = __xrtDynamicFieldConstDict(pFields, "count");

	return pDict != NULL ? xrtTypedDictCount(pDict) : 0u;
}



/* 返回动态字段表再次扩容前的容量。 */
XRT_API size_t xrtDynamicFieldsCapacity(const xrtdynamicfields* pFields)
{
	const xtypeddict* pDict = __xrtDynamicFieldConstDict(pFields, "capacity");

	return pDict != NULL ? xrtTypedDictCapacity(pDict) : 0u;
}



/* 清空动态字段并保留存储供后续复用。 */
XRT_API bool xrtDynamicFieldsClear(xrtdynamicfields* pFields)
{
	xtypeddict* pDict = __xrtDynamicFieldDict(pFields, "clear");

	if ( (pDict != NULL) && xrtTypedDictClear(pDict) ) {
		return true;
	}
	if ( pDict != NULL ) {
		__xrtDynamicFieldWrap(XERR_STATE,
			XDYNAMIC_FIELD_ERROR_STATE, "clear",
			"the dynamic fields could not be cleared");
	}
	return false;
}



/* 预留指定数量的动态字段。 */
XRT_API bool xrtDynamicFieldsReserve(
	xrtdynamicfields* pFields,
	size_t iCapacity
)
{
	xtypeddict* pDict = __xrtDynamicFieldDict(pFields, "reserve");

	if ( (pDict != NULL) && xrtTypedDictReserve(pDict, iCapacity) ) {
		return true;
	}
	if ( pDict != NULL ) {
		__xrtDynamicFieldWrap(XERR_STATE,
			XDYNAMIC_FIELD_ERROR_OPERATION, "reserve",
			"the dynamic field capacity could not be reserved");
	}
	return false;
}



/* 释放动态字段表的多余容量。 */
XRT_API bool xrtDynamicFieldsTrim(xrtdynamicfields* pFields)
{
	xtypeddict* pDict = __xrtDynamicFieldDict(pFields, "trim");

	if ( (pDict != NULL) && xrtTypedDictTrim(pDict) ) {
		return true;
	}
	if ( pDict != NULL ) {
		__xrtDynamicFieldWrap(XERR_STATE,
			XDYNAMIC_FIELD_ERROR_OPERATION, "trim",
			"the dynamic fields could not be trimmed");
	}
	return false;
}



/* 判断指定名称是否存在。 */
XRT_API bool xrtDynamicFieldsHas(
	const xrtdynamicfields* pFields,
	xstrview Name
)
{
	const xtypeddict* pDict = __xrtDynamicFieldConstDict(pFields, "has");

	return (pDict != NULL) &&
		__xrtDynamicFieldNameValid(Name, "has") &&
		xrtTypedDictHas(pDict, Name);
}



/* 返回字段拥有的只读借用 Value。 */
XRT_API const xvalue* xrtDynamicFieldsGet(
	const xrtdynamicfields* pFields,
	xstrview Name
)
{
	const xtypeddict* pDict = __xrtDynamicFieldConstDict(pFields, "get");
	xvalue* const* ppValue;

	if ( pDict == NULL ) {
		return NULL;
	}
	if ( !__xrtDynamicFieldNameValid(Name, "get") ) {
		return NULL;
	}
	ppValue = (xvalue* const*)xrtTypedDictConstGet(pDict, Name);
	return ppValue != NULL ? *ppValue : NULL;
}



/* 保留并返回字段当前拥有的同一 Value。 */
XRT_API xvalue* xrtDynamicFieldsGetRef(
	const xrtdynamicfields* pFields,
	xstrview Name
)
{
	const xvalue* pValue = xrtDynamicFieldsGet(pFields, Name);

	return pValue != NULL ? xrtValueRetain(pValue) : NULL;
}



/* 深复制字段值，使调用方拥有独立的可变值图。 */
XRT_API xvalue* xrtDynamicFieldsCopy(
	const xrtdynamicfields* pFields,
	xstrview Name
)
{
	const xvalue* pValue = xrtDynamicFieldsGet(pFields, Name);
	xvalue* pCopy;

	if ( pValue == NULL ) {
		return NULL;
	}
	pCopy = xrtValueDeepClone(pValue);
	if ( pCopy == NULL ) {
		__xrtDynamicFieldWrap(XERR_MEMORY,
			XDYNAMIC_FIELD_ERROR_OPERATION, "copy",
			"the dynamic field value could not be copied");
	}
	return pCopy;
}



/* 返回与查询等价的内部规范字段名视图。 */
XRT_API bool xrtDynamicFieldsStoredName(
	const xrtdynamicfields* pFields,
	xstrview Name,
	xstrview* pStoredName
)
{
	const xtypeddict* pDict;

	if ( pStoredName == NULL ) {
		__xrtDynamicFieldError(XERR_ARGUMENT,
			XDYNAMIC_FIELD_ERROR_ARGUMENT, "stored-name",
			"the stored dynamic field name output is null");
		return false;
	}
	pStoredName->Data = NULL;
	pStoredName->Size = 0u;
	if ( !__xrtDynamicFieldNameValid(Name, "stored-name") ) {
		return false;
	}
	pDict = __xrtDynamicFieldConstDict(pFields, "stored-name");
	return (pDict != NULL) &&
		xrtTypedDictStoredKey(pDict, Name, pStoredName);
}



/* 深复制来源并失败原子地设置字段。 */
XRT_API bool xrtDynamicFieldsSet(
	xrtdynamicfields* pFields,
	xstrview Name,
	const xvalue* pValue
)
{
	xtypeddict* pDict;

	if ( (pValue == NULL) ||
		 !__xrtDynamicFieldNameValid(Name, "set") ) {
		if ( pValue != NULL ) {
			return false;
		}
		__xrtDynamicFieldError(XERR_ARGUMENT,
			XDYNAMIC_FIELD_ERROR_ARGUMENT, "set",
			"the dynamic field value is null");
		return false;
	}
	pDict = __xrtDynamicFieldDict(pFields, "set");
	return (pDict != NULL) && __xrtDynamicFieldSetClone(
		pDict,
		Name,
		pValue,
		"set",
		"the cloned dynamic field value could not be committed"
	);
}



/* 成功时把来源 Value 移交给字段表并清空调用方槽位。 */
XRT_API bool xrtDynamicFieldsSetTake(
	xrtdynamicfields* pFields,
	xstrview Name,
	xvalue** ppValue
)
{
	xtypeddict* pDict;

	if ( (ppValue == NULL) || (*ppValue == NULL) ) {
		__xrtDynamicFieldError(XERR_ARGUMENT,
			XDYNAMIC_FIELD_ERROR_ARGUMENT, "set-take",
			"the dynamic field source slot is null or empty");
		return false;
	}
	if ( !__xrtDynamicFieldNameValid(Name, "set-take") ) {
		return false;
	}
	pDict = __xrtDynamicFieldDict(pFields, "set-take");
	if ( pDict == NULL ) {
		return false;
	}
	if ( !__xrtDynamicFieldSetClone(
		pDict,
		Name,
		*ppValue,
		"set-take",
		"the isolated dynamic field value could not be committed"
	) ) {
		return false;
	}
	xrtValueRelease(*ppValue);
	*ppValue = NULL;
	return true;
}



/* 设置并无条件消费适合单行构造的临时 Value。 */
XRT_API bool xrtDynamicFieldsSetNew(
	xrtdynamicfields* pFields,
	xstrview Name,
	xvalue* pValue
)
{
	bool bResult = xrtDynamicFieldsSetTake(pFields, Name, &pValue);

	xrtValueRelease(pValue);
	return bResult;
}



/* 移交一个已经保留的同一 Value，并把类型槽恢复为空值。 */
static bool __xrtDynamicFieldsSetRefOwned(
	xtypeddict* pDict,
	xstrview Name,
	xvalue** ppValue,
	cstr sOperation
)
{
	xvalue* pMoved;

	if ( !xrtTypedDictSetTake(pDict, Name, ppValue) ) {
		__xrtDynamicFieldWrap(XERR_STATE,
			XDYNAMIC_FIELD_ERROR_OPERATION, sOperation,
			"the shared dynamic field value could not be committed");
		return false;
	}
	pMoved = *ppValue;
	*ppValue = NULL;
	xrtValueRelease(pMoved);
	return true;
}



/* 保留同一 Value 身份并失败原子地设置字段。 */
XRT_API bool xrtDynamicFieldsSetRef(
	xrtdynamicfields* pFields,
	xstrview Name,
	const xvalue* pValue
)
{
	xtypeddict* pDict;
	xvalue* pReference;

	if ( (pValue == NULL) ||
		 !__xrtDynamicFieldNameValid(Name, "set-ref") ) {
		if ( pValue == NULL ) {
			__xrtDynamicFieldError(XERR_ARGUMENT,
				XDYNAMIC_FIELD_ERROR_ARGUMENT, "set-ref",
				"the shared dynamic field value is null");
		}
		return false;
	}
	pDict = __xrtDynamicFieldDict(pFields, "set-ref");
	if ( pDict == NULL ) {
		return false;
	}
	pReference = xrtValueRetain(pValue);
	if ( pReference == NULL ) {
		__xrtDynamicFieldWrap(XERR_STATE,
			XDYNAMIC_FIELD_ERROR_OPERATION, "set-ref",
			"the shared dynamic field value could not be retained");
		return false;
	}
	if ( !__xrtDynamicFieldsSetRefOwned(
		pDict, Name, &pReference, "set-ref"
	) ) {
		xrtValueRelease(pReference);
		return false;
	}
	return true;
}



/* 成功时把来源 Value 的同一身份移交给字段表。 */
XRT_API bool xrtDynamicFieldsSetRefTake(
	xrtdynamicfields* pFields,
	xstrview Name,
	xvalue** ppValue
)
{
	xtypeddict* pDict;

	if ( (ppValue == NULL) || (*ppValue == NULL) ) {
		__xrtDynamicFieldError(XERR_ARGUMENT,
			XDYNAMIC_FIELD_ERROR_ARGUMENT, "set-ref-take",
			"the shared dynamic field source slot is null or empty");
		return false;
	}
	if ( !__xrtDynamicFieldNameValid(Name, "set-ref-take") ) {
		return false;
	}
	pDict = __xrtDynamicFieldDict(pFields, "set-ref-take");
	return (pDict != NULL) && __xrtDynamicFieldsSetRefOwned(
		pDict, Name, ppValue, "set-ref-take"
	);
}



/* 无论成功失败都消费共享 Value 临时值。 */
XRT_API bool xrtDynamicFieldsSetRefNew(
	xrtdynamicfields* pFields,
	xstrview Name,
	xvalue* pValue
)
{
	bool bResult = xrtDynamicFieldsSetRefTake(pFields, Name, &pValue);

	xrtValueRelease(pValue);
	return bResult;
}



/* 删除并释放指定字段。 */
XRT_API bool xrtDynamicFieldsRemove(
	xrtdynamicfields* pFields,
	xstrview Name
)
{
	xtypeddict* pDict = __xrtDynamicFieldDict(pFields, "remove");

	return (pDict != NULL) &&
		__xrtDynamicFieldNameValid(Name, "remove") &&
		xrtTypedDictRemove(pDict, Name);
}



/* 把指定字段值移交给调用方，字段缺失时返回空指针。 */
XRT_API xvalue* xrtDynamicFieldsTake(
	xrtdynamicfields* pFields,
	xstrview Name
)
{
	xtypeddict* pDict = __xrtDynamicFieldDict(pFields, "take");
	xvalue* pValue = xrtValueNull();

	if ( (pDict == NULL) ||
		 !__xrtDynamicFieldNameValid(Name, "take") ||
		 !xrtTypedDictTake(pDict, Name, &pValue) ) {
		xrtValueRelease(pValue);
		return NULL;
	}
	return pValue;
}



/* 启动动态字段外置迭代并保留字段对象。 */
static bool __xrtDynamicFieldsIterStart(
	xrtdynamicfields* pFields,
	xrtdynamicfielditer* pIterator,
	bool bReverse,
	cstr sOperation
)
{
	xtypeddict* pDict;

	if ( pIterator == NULL ) {
		__xrtDynamicFieldError(XERR_ARGUMENT,
			XDYNAMIC_FIELD_ERROR_ARGUMENT, sOperation,
			"the dynamic field iterator is null");
		return false;
	}
	memset(pIterator, 0, sizeof(*pIterator));
	pDict = __xrtDynamicFieldDict(pFields, sOperation);
	if ( (pDict == NULL) || (xrtObjectRef(pFields) == NULL) ) {
		return false;
	}
	if ( !(bReverse ?
		xrtTypedDictIterRBegin(pDict, &pIterator->Base) :
		xrtTypedDictIterBegin(pDict, &pIterator->Base)) ) {
		xrtObjectUnref(pFields);
		__xrtDynamicFieldWrap(XERR_STATE,
			XDYNAMIC_FIELD_ERROR_STATE, sOperation,
			"the dynamic field iteration could not start");
		return false;
	}
	pIterator->Fields = pFields;
	return true;
}



/* 启动按插入顺序的动态字段迭代。 */
XRT_API bool xrtDynamicFieldsIterBegin(
	xrtdynamicfields* pFields,
	xrtdynamicfielditer* pIterator
)
{
	return __xrtDynamicFieldsIterStart(
		pFields, pIterator, false, "iter-begin"
	);
}



/* 启动按插入顺序逆序的动态字段迭代。 */
XRT_API bool xrtDynamicFieldsIterRBegin(
	xrtdynamicfields* pFields,
	xrtdynamicfielditer* pIterator
)
{
	return __xrtDynamicFieldsIterStart(
		pFields, pIterator, true, "iter-rbegin"
	);
}



/* 返回下一字段的借用名称和值。 */
XRT_API const xvalue* xrtDynamicFieldsIterNext(
	xrtdynamicfielditer* pIterator,
	xstrview* pName
)
{
	xvalue** ppValue;

	if ( (pIterator == NULL) || (pIterator->Fields == NULL) ) {
		__xrtDynamicFieldError(
			pIterator == NULL ? XERR_ARGUMENT : XERR_STATE,
			pIterator == NULL ? XDYNAMIC_FIELD_ERROR_ARGUMENT :
				XDYNAMIC_FIELD_ERROR_STATE,
			"iter-next",
			"the dynamic field iterator is not active");
		return NULL;
	}
	ppValue = (xvalue**)xrtTypedDictIterNext(
		&pIterator->Base, pName
	);
	return ppValue != NULL ? *ppValue : NULL;
}



/* 结束迭代并释放字段对象保留。 */
XRT_API void xrtDynamicFieldsIterEnd(xrtdynamicfielditer* pIterator)
{
	xrtdynamicfields* pFields;

	if ( pIterator == NULL ) {
		return;
	}
	pFields = pIterator->Fields;
	xrtTypedDictIterEnd(&pIterator->Base);
	pIterator->Fields = NULL;
	xrtObjectUnref(pFields);
}



/* 返回递增且跳过外置迭代器保留零值的结构版本。 */
static uint64 __xrtDynamicFieldNextVersion(uint64 iVersion)
{
	iVersion++;
	return iVersion != 0u ? iVersion : 1u;
}



/* 按插入顺序把来源字段快照或深复制到独立工作字典。 */
static bool __xrtDynamicFieldMergeEntries(
	xtypeddict* pTarget,
	const xtypeddict* pSource,
	bool bReplace,
	bool bDeepClone
)
{
	xtypeddictiter Iterator = { 0 };
	xstrview Name;
	xvalue** ppValue;

	if ( !xrtTypedDictIterBegin(
		(xtypeddict*)pSource, &Iterator
	) ) {
		__xrtDynamicFieldWrap(XERR_STATE,
			XDYNAMIC_FIELD_ERROR_STATE, "merge",
			"the dynamic field source iteration could not start");
		return false;
	}
	while ( (ppValue = (xvalue**)xrtTypedDictIterNext(
		&Iterator, &Name
	)) != NULL ) {
		bool bStored;

		if ( !bReplace && xrtTypedDictHas(pTarget, Name) ) {
			continue;
		}
		if ( bDeepClone ) {
			bStored = __xrtDynamicFieldSetClone(
				pTarget,
				Name,
				*ppValue,
				"merge",
				"a cloned dynamic field value could not be committed"
			);
		} else {
			bStored = xrtTypedDictSet(pTarget, Name, ppValue);
		}
		if ( !bStored ) {
			if ( !bDeepClone ) {
				__xrtDynamicFieldWrap(XERR_STATE,
					XDYNAMIC_FIELD_ERROR_OPERATION, "merge",
					"an existing dynamic field value could not be snapshotted");
			}
			xrtTypedDictIterEnd(&Iterator);
			return false;
		}
	}
	if ( xrtGetError() != NULL ) {
		xrtTypedDictIterEnd(&Iterator);
		__xrtDynamicFieldWrap(XERR_STATE,
			XDYNAMIC_FIELD_ERROR_STATE, "merge",
			"the dynamic field source iteration failed");
		return false;
	}
	xrtTypedDictIterEnd(&Iterator);
	return true;
}



/* 事务合并两个动态字段对象。 */
XRT_API bool xrtDynamicFieldsMerge(
	xrtdynamicfields* pTarget,
	const xrtdynamicfields* pSource,
	bool bReplace
)
{
	xtypeddict* pTargetDict = __xrtDynamicFieldDict(pTarget, "merge");
	const xtypeddict* pSourceDict = __xrtDynamicFieldConstDict(
		pSource, "merge"
	);
	xtypeddict Work;
	xmap Previous;
	xerror* pPreviousError;
	xerror* pDiscard;
	size_t iCapacity;
	bool bReady = false;
	bool bSuccess = false;

	if ( (pTargetDict == NULL) || (pSourceDict == NULL) ) {
		return false;
	}
	if ( pTarget == pSource ) {
		return true;
	}
	if ( xrtTypedDictCount(pSourceDict) == 0u ) {
		return true;
	}
	if ( xrtTypedDictCount(pTargetDict) >
		 (SIZE_MAX - xrtTypedDictCount(pSourceDict)) ) {
		__xrtDynamicFieldError(XERR_RANGE,
			XDYNAMIC_FIELD_ERROR_OPERATION, "merge",
			"the merged dynamic field count overflows");
		return false;
	}
	iCapacity = xrtTypedDictCount(pTargetDict) +
		xrtTypedDictCount(pSourceDict);
	pPreviousError = __xrtErrorSwapOwned(NULL);
	memset(&Work, 0, sizeof(Work));
	if ( !xrtTypedDictInit(&Work, xrtTypeValue()) ) {
		__xrtDynamicFieldWrap(XERR_STATE,
			XDYNAMIC_FIELD_ERROR_OPERATION, "merge",
			"the dynamic field work dictionary could not be initialized");
		goto cleanup;
	}
	bReady = true;
	if ( !xrtTypedDictReserve(&Work, iCapacity) ) {
		__xrtDynamicFieldWrap(XERR_MEMORY,
			XDYNAMIC_FIELD_ERROR_OPERATION, "merge",
			"the dynamic field work dictionary could not be reserved");
		goto cleanup;
	}
	if ( !__xrtDynamicFieldMergeEntries(
		&Work, pTargetDict, true, false
	) || !__xrtDynamicFieldMergeEntries(
		&Work, pSourceDict, bReplace, true
	) ) {
		goto cleanup;
	}
	Previous = pTargetDict->Storage;
	pTargetDict->Storage = Work.Storage;
	Work.Storage = Previous;
	pTargetDict->Storage.Version = __xrtDynamicFieldNextVersion(
		Previous.Version
	);
	bSuccess = true;

cleanup:
	if ( bReady ) {
		xrtTypedDictUnit(&Work);
	}
	if ( bSuccess ) {
		pDiscard = __xrtErrorSwapOwned(pPreviousError);
		xrtErrorFree(pDiscard);
	} else {
		xrtErrorFree(pPreviousError);
		if ( xrtGetError() == NULL ) {
			__xrtDynamicFieldError(XERR_STATE,
				XDYNAMIC_FIELD_ERROR_OPERATION, "merge",
				"the dynamic fields could not be merged");
		}
	}
	return bSuccess;
}



/* 深复制一个动态字段对象和它拥有的 Value 图。 */
XRT_API xrtdynamicfields* xrtDynamicFieldsClone(
	const xrtdynamicfields* pFields
)
{
	xrtdynamicfields* pClone;

	if ( __xrtDynamicFieldConstDict(pFields, "clone") == NULL ) {
		return NULL;
	}
	pClone = xrtDynamicFieldsCreate();
	if ( pClone == NULL ) {
		return NULL;
	}
	if ( !xrtDynamicFieldsMerge(pClone, pFields, true) ) {
		__xrtDynamicFieldUnrefPreserveError(pClone);
		return NULL;
	}
	return pClone;
}



typedef enum xdynamicfieldcollectkind {
	XDYNAMIC_FIELD_COLLECT_KEYS,
	XDYNAMIC_FIELD_COLLECT_VALUES,
	XDYNAMIC_FIELD_COLLECT_ITEMS,
	XDYNAMIC_FIELD_COLLECT_OBJECT
} xdynamicfieldcollectkind;



/* 构造一个字段名 Value。 */
static xvalue* __xrtDynamicFieldNameValue(xstrview Name)
{
	return xrtValueString(Name);
}



/* 构造一个独立字段值。 */
static xvalue* __xrtDynamicFieldValueCopy(const xvalue* pValue)
{
	return xrtValueDeepClone(pValue);
}



/* 构造一个拥有独立名称和值的 [name, value] 二元项。 */
static xvalue* __xrtDynamicFieldPair(
	xstrview Name,
	const xvalue* pValue
)
{
	xvalue* pPair = xrtValueArray();
	xvalue* pName;
	xvalue* pCopy;

	if ( (pPair == NULL) || !xrtValueReserve(pPair, 2u) ) {
		xrtValueRelease(pPair);
		return NULL;
	}
	pName = __xrtDynamicFieldNameValue(Name);
	if ( (pName == NULL) ||
		 !xrtValueArrayAppendNew(pPair, pName) ) {
		xrtValueRelease(pPair);
		return NULL;
	}
	pCopy = __xrtDynamicFieldValueCopy(pValue);
	if ( (pCopy == NULL) ||
		 !xrtValueArrayAppendNew(pPair, pCopy) ) {
		xrtValueRelease(pPair);
		return NULL;
	}
	return pPair;
}



/* 构造语言绑定常用的名称、值或二元项数组。 */
static xvalue* __xrtDynamicFieldsCollect(
	const xrtdynamicfields* pFields,
	xdynamicfieldcollectkind Kind,
	cstr sOperation
)
{
	const xtypeddict* pDict;
	xrtdynamicfielditer Iterator = { 0 };
	xvalue* pResult = NULL;
	const xvalue* pValue;
	xstrview Name;
	size_t iCount;
	xerror* pPrevious;
	xerror* pFailure;
	xerror* pDiscard;
	bool bIterating = false;

	pDict = __xrtDynamicFieldConstDict(pFields, sOperation);
	if ( pDict == NULL ) {
		return NULL;
	}
	pPrevious = __xrtErrorSwapOwned(NULL);
	iCount = xrtTypedDictCount(pDict);
	pResult = Kind == XDYNAMIC_FIELD_COLLECT_OBJECT ?
		xrtValueObject() : xrtValueArray();
	if ( pResult == NULL ) {
		__xrtDynamicFieldWrap(XERR_MEMORY,
			XDYNAMIC_FIELD_ERROR_OPERATION, sOperation,
			"the dynamic field result could not be created");
		goto failure;
	}
	if ( !xrtValueReserve(pResult, iCount) ) {
		__xrtDynamicFieldWrap(XERR_MEMORY,
			XDYNAMIC_FIELD_ERROR_OPERATION, sOperation,
			"the dynamic field result could not be reserved");
		goto failure;
	}
	if ( !xrtDynamicFieldsIterBegin(
		(xrtdynamicfields*)pFields, &Iterator
	) ) {
		__xrtDynamicFieldWrap(XERR_STATE,
			XDYNAMIC_FIELD_ERROR_STATE, sOperation,
			"the dynamic field result iteration could not start");
		goto failure;
	}
	bIterating = true;
	while ( (pValue = xrtDynamicFieldsIterNext(
		&Iterator, &Name
	)) != NULL ) {
		xvalue* pItem;

		if ( Kind == XDYNAMIC_FIELD_COLLECT_KEYS ) {
			pItem = __xrtDynamicFieldNameValue(Name);
		} else if ( Kind == XDYNAMIC_FIELD_COLLECT_VALUES ) {
			pItem = __xrtDynamicFieldValueCopy(pValue);
		} else if ( Kind == XDYNAMIC_FIELD_COLLECT_ITEMS ) {
			pItem = __xrtDynamicFieldPair(Name, pValue);
		} else {
			pItem = __xrtDynamicFieldValueCopy(pValue);
		}
		if ( (pItem == NULL) || !(Kind == XDYNAMIC_FIELD_COLLECT_OBJECT ?
			xrtValueObjectSetNew(pResult, Name, pItem) :
			xrtValueArrayAppendNew(pResult, pItem)) ) {
			__xrtDynamicFieldWrap(XERR_MEMORY,
				XDYNAMIC_FIELD_ERROR_OPERATION, sOperation,
				"a dynamic field result item could not be created");
			goto failure;
		}
	}
	if ( xrtGetError() != NULL ) {
		__xrtDynamicFieldWrap(XERR_STATE,
			XDYNAMIC_FIELD_ERROR_STATE, sOperation,
			"the dynamic field result iteration failed");
		goto failure;
	}
	xrtDynamicFieldsIterEnd(&Iterator);
	pDiscard = __xrtErrorSwapOwned(pPrevious);
	xrtErrorFree(pDiscard);
	return pResult;

failure:
	pFailure = xrtTakeError();
	if ( bIterating ) {
		xrtDynamicFieldsIterEnd(&Iterator);
	}
	xrtValueRelease(pResult);
	if ( pFailure != NULL ) {
		xrtSetError(pFailure);
		xrtErrorFree(pFailure);
	}
	xrtErrorFree(pPrevious);
	if ( xrtGetError() == NULL ) {
		__xrtDynamicFieldError(XERR_STATE,
			XDYNAMIC_FIELD_ERROR_OPERATION, sOperation,
			"the dynamic field result could not be completed");
	}
	return NULL;
}



/* 返回按插入顺序排列的字段名数组。 */
XRT_API xvalue* xrtDynamicFieldsKeys(const xrtdynamicfields* pFields)
{
	return __xrtDynamicFieldsCollect(
		pFields, XDYNAMIC_FIELD_COLLECT_KEYS, "keys"
	);
}



/* 返回按插入顺序排列的独立字段值数组。 */
XRT_API xvalue* xrtDynamicFieldsValues(const xrtdynamicfields* pFields)
{
	return __xrtDynamicFieldsCollect(
		pFields, XDYNAMIC_FIELD_COLLECT_VALUES, "values"
	);
}



/* 返回按插入顺序排列的 [name, value] 二元项数组。 */
XRT_API xvalue* xrtDynamicFieldsItems(const xrtdynamicfields* pFields)
{
	return __xrtDynamicFieldsCollect(
		pFields, XDYNAMIC_FIELD_COLLECT_ITEMS, "items"
	);
}



/* 返回按插入顺序保存字段的独立 Value Object。 */
XRT_API xvalue* xrtDynamicFieldsToValue(const xrtdynamicfields* pFields)
{
	return __xrtDynamicFieldsCollect(
		pFields, XDYNAMIC_FIELD_COLLECT_OBJECT, "to-value"
	);
}



/* 从 Value Object 深复制名称和值并创建动态字段对象。 */
XRT_API xrtdynamicfields* xrtDynamicFieldsFromValue(const xvalue* pValue)
{
	xrtdynamicfields* pFields;
	size_t iCount;
	size_t i;

	if ( xrtValueType(pValue) != XVALUE_OBJECT ) {
		__xrtDynamicFieldError(XERR_TYPE,
			XDYNAMIC_FIELD_ERROR_TYPE, "from-value",
			"the dynamic field source is not a Value Object");
		return NULL;
	}
	pFields = xrtDynamicFieldsCreate();
	if ( pFields == NULL ) {
		return NULL;
	}
	iCount = xrtValueCount(pValue);
	if ( !xrtDynamicFieldsReserve(pFields, iCount) ) {
		__xrtDynamicFieldUnrefPreserveError(pFields);
		return NULL;
	}
	for ( i = 0u; i < iCount; ++i ) {
		xstrview Name;
		xvalue* pItem = xrtValueObjectAt(pValue, i, &Name);

		if ( (pItem == NULL) ||
			 !xrtDynamicFieldsSet(pFields, Name, pItem) ) {
			__xrtDynamicFieldUnrefPreserveError(pFields);
			return NULL;
		}
	}
	return pFields;
}

#endif
