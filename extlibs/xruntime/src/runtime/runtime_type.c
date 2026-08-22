#include "../internal/xrt_runtime_type.h"



#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE)

#define XRT_TYPE_FLAGS (XRT_TYPE_FLAG_TRIVIAL_COPY | \
	XRT_TYPE_FLAG_TRIVIAL_DROP | XRT_TYPE_FLAG_COPYABLE | \
	XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE | XRT_TYPE_FLAG_FINAL | \
	XRT_TYPE_FLAG_RELOCATABLE)
#define XRT_PARAM_FLAGS (XRT_PARAM_FLAG_OPTIONAL | XRT_PARAM_FLAG_NAMED_ONLY)
#define XRT_FUNCTION_FLAGS (XRT_FUNCTION_FLAG_VARARGS | XRT_FUNCTION_FLAG_KWARGS)
#define XRT_METHOD_FLAGS (XRT_METHOD_FLAG_STATIC | \
	XRT_METHOD_FLAG_VIRTUAL | XRT_METHOD_FLAG_FINAL)



struct xrttyperegistry {
	xrt_spinlock Lock;
	size_t Count;
	size_t Capacity;
	const xrttype** Types;
};



struct xrtprotocolregistry {
	xrt_spinlock Lock;
	size_t Count;
	size_t Capacity;
	const xrtprotocolwitness** Witnesses;
};



/* 设置运行时类型模块结构化错误。 */
static void __xrtRuntimeTypeError(xerrkind Kind, xtypeerror Code,
	cstr sOperation, cstr sMessage)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.type";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 为类型操作回调的失败补充统一上下文，并保留其原始错误作为原因。 */
static void __xrtRuntimeTypeWrap(
	xerrkind DefaultKind,
	xtypeerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : DefaultKind;
	Desc.Domain = "xrt.type";
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



/* 规范标识使用稳定的 FNV-1a 64 位哈希，不依赖主机字节序。 */
static uint64 __xrtTypeHashBytes(uint64 iHash, const void* pData, size_t iSize)
{
	const uint8* pBytes = (const uint8*)pData;

	for ( size_t i = 0; i < iSize; i++ ) {
		iHash ^= pBytes[i];
		iHash *= UINT64_C(1099511628211);
	}
	return iHash;
}



/* 以固定小端字节顺序把一个 64 位整数加入稳定哈希。 */
static uint64 __xrtTypeHashU64(uint64 iHash, uint64 iValue)
{
	for ( uint32 i = 0; i < 8u; i++ ) {
		uint8 iByte = (uint8)(iValue >> (i * 8u));
		iHash = __xrtTypeHashBytes(iHash, &iByte, 1u);
	}
	return iHash;
}



/* ABI 对齐必须是非零的二次幂。 */
static bool __xrtTypeAlignValid(size_t iAlign)
{
	return (iAlign != 0) && ((iAlign & (iAlign - 1u)) == 0);
}



/* 检查内建标量类别能否由统一比较和散列路径安全解释。 */
static bool __xrtTypeScalarLayoutValid(const xrttype* pType)
{
	switch ( pType->Kind ) {
		case XRT_TYPE_NULL:
			return pType->Size == 0u;
		case XRT_TYPE_BOOL:
			return (pType->Size == sizeof(bool)) ||
				(pType->Size == sizeof(int32));
		case XRT_TYPE_SIGNED_INT:
		case XRT_TYPE_UNSIGNED_INT:
			return (pType->Size == 1u) || (pType->Size == 2u) ||
				(pType->Size == 4u) || (pType->Size == 8u);
		case XRT_TYPE_FLOAT:
			return (pType->Size == sizeof(float)) ||
				(pType->Size == sizeof(double));
		case XRT_TYPE_TIME:
			return pType->Size == sizeof(xtime);
		case XRT_TYPE_POINTER:
			return (pType->Size == sizeof(ptr)) &&
				(pType->Align == XRT_INTERNAL_ALIGNOF(ptr));
		case XRT_TYPE_TYPE:
			return pType->Size == sizeof(uint64);
		default:
			return true;
	}
}



/* 前置声明供签名中的类型引用执行规范身份检查。 */
static uint64 __xrtTypeComputedId(xstrview AbiName);



/* 只检查类型引用可安全读取且 ID 与规范 ABI 名一致。 */
static bool __xrtTypeIdentityValid(const xrttype* pType)
{
	return (pType != NULL) &&
		__xrtTypeViewValid(&pType->AbiName, false) &&
		(pType->Id == __xrtTypeComputedId(pType->AbiName));
}



/* 按参数、返回值和调用标志计算与函数显示名无关的签名 ID。 */
static uint64 __xrtFunctionSigComputedId(const xrtfunctionsig* pSignature)
{
	uint64 iHash = UINT64_C(14695981039346656037);

	iHash = __xrtTypeHashU64(iHash, (uint64)pSignature->ParamCount);
	for ( size_t i = 0; i < pSignature->ParamCount; i++ ) {
		const xrtparamdesc* pParam = &pSignature->Params[i];

		iHash = __xrtTypeHashU64(iHash, pParam->Type->Id);
		iHash = __xrtTypeHashU64(iHash, (uint64)pParam->Mode);
		iHash = __xrtTypeHashU64(iHash, (uint64)pParam->Flags);
		if ( pParam->Name.Size != 0u ) {
			iHash = __xrtTypeHashU64(iHash, (uint64)pParam->Name.Size);
			iHash = __xrtTypeHashBytes(
				iHash, pParam->Name.Data, pParam->Name.Size);
		}
	}
	iHash = __xrtTypeHashU64(iHash, (uint64)pSignature->ReturnCount);
	for ( size_t i = 0; i < pSignature->ReturnCount; i++ ) {
		iHash = __xrtTypeHashU64(iHash, pSignature->ReturnTypes[i]->Id);
	}
	iHash = __xrtTypeHashU64(iHash, (uint64)pSignature->Flags);
	return iHash != 0 ? iHash : UINT64_C(1);
}



/* 检查函数签名的视图、类型引用、标志和显式 ID。 */
static bool __xrtFunctionSigValidate(const xrtfunctionsig* pSignature)
{
	if ( pSignature == NULL ) {
		return false;
	}
	if (
		!__xrtTypeViewValid(&pSignature->Name, true) ||
		((pSignature->Flags & ~XRT_FUNCTION_FLAGS) != 0u) ||
		((pSignature->ParamCount != 0) && (pSignature->Params == NULL)) ||
		((pSignature->ReturnCount != 0) && (pSignature->ReturnTypes == NULL))
	) {
		return false;
	}
	for ( size_t i = 0; i < pSignature->ParamCount; i++ ) {
		const xrtparamdesc* pParam = &pSignature->Params[i];

		if (
			!__xrtTypeIdentityValid(pParam->Type) ||
			(pParam->Mode < XRT_PARAM_DEFAULT) ||
			(pParam->Mode > XRT_PARAM_BYREF) ||
			((pParam->Flags & ~XRT_PARAM_FLAGS) != 0u) ||
			!__xrtTypeViewValid(&pParam->Name,
				(pParam->Flags & XRT_PARAM_FLAG_NAMED_ONLY) == 0u)
		) {
			return false;
		}
		if ( pParam->Name.Size != 0u ) {
			for ( size_t j = 0; j < i; j++ ) {
				if ( __xrtTypeViewEqual(
					&pSignature->Params[j].Name, &pParam->Name
				) ) {
					return false;
				}
			}
		}
	}
	for ( size_t i = 0; i < pSignature->ReturnCount; i++ ) {
		if ( !__xrtTypeIdentityValid(pSignature->ReturnTypes[i]) ) {
			return false;
		}
	}
	return (pSignature->Id == 0) ||
		(pSignature->Id == __xrtFunctionSigComputedId(pSignature));
}



/* 检查方法表的名称、签名、入口、标志和重载唯一性。 */
static bool __xrtTypeMethodTableValidate(const xrtmethodtable* pMethods)
{
	if ( pMethods == NULL ) {
		return true;
	}
	if ( (pMethods->Count != 0) && (pMethods->Methods == NULL) ) {
		return false;
	}
	for ( size_t i = 0; i < pMethods->Count; i++ ) {
		const xrtmethoddesc* pMethod = &pMethods->Methods[i];

		if (
			!__xrtTypeViewValid(&pMethod->Name, false) ||
			!__xrtFunctionSigValidate(pMethod->Signature) ||
			(pMethod->Entry == NULL) ||
			((pMethod->Flags & ~XRT_METHOD_FLAGS) != 0u)
		) {
			return false;
		}
		for ( size_t j = 0; j < i; j++ ) {
			const xrtmethoddesc* pPrevious = &pMethods->Methods[j];

			if (
				__xrtTypeViewEqual(&pPrevious->Name, &pMethod->Name) &&
				(xrtFunctionSigId(pPrevious->Signature) ==
				 xrtFunctionSigId(pMethod->Signature))
			) {
				return false;
			}
		}
	}
	return true;
}



/* 在已经验证 ABI 名后计算非零规范类型 ID。 */
static uint64 __xrtTypeComputedId(xstrview AbiName)
{
	uint64 iHash = __xrtTypeHashBytes(
		UINT64_C(14695981039346656037), AbiName.Data, AbiName.Size);

	return iHash != 0u ? iHash : UINT64_C(1);
}



/* 按规范 ABI 名生成稳定类型 ID。 */
XRT_API uint64 xrtTypeId(xstrview AbiName)
{
	if ( !__xrtTypeViewValid(&AbiName, false) ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_DESCRIPTOR,
			"type-id", "the ABI type name is empty or invalid");
		return 0;
	}
	return __xrtTypeComputedId(AbiName);
}



/* 验证函数签名并返回显式或计算得到的稳定身份。 */
XRT_API uint64 xrtFunctionSigId(const xrtfunctionsig* pSignature)
{
	if ( !__xrtFunctionSigValidate(pSignature) ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_SIGNATURE,
			"signature-id", "the function signature is invalid");
		return 0;
	}
	return pSignature->Id != 0
		? pSignature->Id
		: __xrtFunctionSigComputedId(pSignature);
}



/* 检查函数签名的完整结构和稳定身份。 */
XRT_API bool xrtFunctionSigValidate(const xrtfunctionsig* pSignature)
{
	if ( !__xrtFunctionSigValidate(pSignature) ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_SIGNATURE,
			"signature-validate", "the function signature is invalid");
		return false;
	}
	return true;
}



/* 检查单个类型描述，不沿继承链递归。 */
static bool __xrtTypeShapeValidate(const xrttype* pType)
{
	const xrttypeops* pOps;

	if (
		(pType == NULL) ||
		(pType->Kind <= XRT_TYPE_INVALID) ||
		(pType->Kind > XRT_TYPE_WEAK) ||
		!__xrtTypeViewValid(&pType->Name, false) ||
		!__xrtTypeViewValid(&pType->AbiName, false) ||
		(pType->Id != __xrtTypeComputedId(pType->AbiName)) ||
		((pType->Flags & ~XRT_TYPE_FLAGS) != 0u) ||
		!__xrtTypeAlignValid(pType->Align) ||
		!__xrtTypeAlignValid(pType->InstanceAlign) ||
		!__xrtTypeScalarLayoutValid(pType) ||
		((pType->ArgumentCount != 0) && (pType->Arguments == NULL)) ||
		!__xrtTypeMethodTableValidate(pType->Methods)
	) {
		return false;
	}
	pOps = pType->Ops;
	if (
		((pType->Flags & XRT_TYPE_FLAG_TRIVIAL_COPY) != 0) &&
		((pType->Flags & XRT_TYPE_FLAG_COPYABLE) == 0)
	) {
		return false;
	}
	if (
		((pType->Flags & XRT_TYPE_FLAG_COPYABLE) != 0u) &&
		((pType->Flags & XRT_TYPE_FLAG_TRIVIAL_COPY) == 0u) &&
		((pOps == NULL) || (pOps->Copy == NULL))
	) {
		return false;
	}
	if (
		(pOps != NULL) &&
		((((pType->Flags & XRT_TYPE_FLAG_TRIVIAL_COPY) != 0u) &&
		  (pOps->Copy != NULL)) ||
		 (((pType->Flags & XRT_TYPE_FLAG_TRIVIAL_DROP) != 0u) &&
		  (pOps->Drop != NULL)) ||
		 (((pType->Flags & XRT_TYPE_FLAG_COPYABLE) == 0u) &&
		  (pOps->Copy != NULL)))
	) {
		return false;
	}
	if (
		((pType->Flags & XRT_TYPE_FLAG_REFERENCE) != 0) &&
		((pType->Size != sizeof(ptr)) || (pType->Align != XRT_INTERNAL_ALIGNOF(ptr)))
	) {
		return false;
	}
	for ( size_t i = 0; i < pType->ArgumentCount; i++ ) {
		if ( !__xrtTypeIdentityValid(pType->Arguments[i]) ) {
			return false;
		}
	}
	return true;
}



/* 检查类型描述和完整继承链的身份、布局、终结约束与环。 */
XRT_API bool xrtTypeValidate(const xrttype* pType)
{
	const xrttype* pBase = pType;

	if ( !__xrtTypeShapeValidate(pType) ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_DESCRIPTOR,
			"validate", "the runtime type descriptor is invalid");
		return false;
	}
	pBase = pType;
	for ( uint32 i = 0; i < XRT_RUNTIME_TYPE_INHERITANCE_MAX; i++ ) {
		pBase = pBase->Base;
		if ( pBase == NULL ) {
			return true;
		}
		if ( !__xrtTypeShapeValidate(pBase) ||
			 (pType->Kind != XRT_TYPE_CLASS) ||
			 (pBase->Kind != XRT_TYPE_CLASS) ||
			 ((pBase->Flags & XRT_TYPE_FLAG_FINAL) != 0u) ||
			 (pType->InstanceSize < pBase->InstanceSize) ||
			 (pType->InstanceAlign < pBase->InstanceAlign) ) {
			__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_DESCRIPTOR,
				"validate", "the runtime type inheritance chain is invalid");
			return false;
		}
	}
	__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_DESCRIPTOR,
		"validate", "the runtime type inheritance chain is cyclic or too deep");
	return false;
}



/* 比较两个有效类型引用是否具有相同规范身份。 */
XRT_API bool xrtTypeSame(const xrttype* pLeft, const xrttype* pRight)
{
	if ( pLeft == pRight ) {
		return __xrtTypeIdentityValid(pLeft);
	}
	if ( !__xrtTypeIdentityValid(pLeft) ||
		 !__xrtTypeIdentityValid(pRight) ||
		 (pLeft->Id != pRight->Id) ) {
		return false;
	}
	return __xrtTypeViewEqual(&pLeft->AbiName, &pRight->AbiName);
}



/* 沿有界类继承链判断类型关系。 */
XRT_API bool xrtTypeIsA(const xrttype* pType, const xrttype* pTarget)
{
	for ( uint32 i = 0;
		  (pType != NULL) && (i < XRT_RUNTIME_TYPE_INHERITANCE_MAX);
		  i++ ) {
		if ( xrtTypeSame(pType, pTarget) ) {
			return true;
		}
		pType = pType->Base;
	}
	return false;
}



/* 判断内建类型类别是否具有无需回调的比较和散列能力。 */
static bool __xrtTypeBuiltinComparable(const xrttype* pType)
{
	return __xrtTypeScalarLayoutValid(pType) &&
		((pType->Kind == XRT_TYPE_NULL) ||
		 (pType->Kind == XRT_TYPE_BOOL) ||
		 (pType->Kind == XRT_TYPE_SIGNED_INT) ||
		 (pType->Kind == XRT_TYPE_UNSIGNED_INT) ||
		 (pType->Kind == XRT_TYPE_FLOAT) ||
		 (pType->Kind == XRT_TYPE_TIME) ||
		 (pType->Kind == XRT_TYPE_POINTER) ||
		 (pType->Kind == XRT_TYPE_TYPE));
}



/* 查询值复制能力。 */
XRT_API bool xrtTypeIsCopyable(const xrttype* pType)
{
	return (pType != NULL) &&
		((pType->Flags & XRT_TYPE_FLAG_COPYABLE) != 0u);
}



/* 查询值是否允许容器按字节搬迁到另一地址。 */
XRT_API bool xrtTypeIsRelocatable(const xrttype* pType)
{
	return (pType != NULL) &&
		((pType->Flags & XRT_TYPE_FLAG_RELOCATABLE) != 0u);
}



/* 查询值比较能力。 */
XRT_API bool xrtTypeIsComparable(const xrttype* pType)
{
	return (pType != NULL) &&
		(((pType->Ops != NULL) && (pType->Ops->Compare != NULL)) ||
		 __xrtTypeBuiltinComparable(pType));
}



/* 查询值散列能力。 */
XRT_API bool xrtTypeIsHashable(const xrttype* pType)
{
	return (pType != NULL) &&
		(((pType->Ops != NULL) && (pType->Ops->Hash != NULL)) ||
		 __xrtTypeBuiltinComparable(pType));
}



/* 返回借用的泛型实参并检查下标范围。 */
XRT_API const xrttype* xrtTypeArgument(const xrttype* pType, size_t iIndex)
{
	if ( pType == NULL ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_DESCRIPTOR,
			"argument", "the runtime type descriptor is null");
		return NULL;
	}
	if ( iIndex >= pType->ArgumentCount ) {
		__xrtRuntimeTypeError(XERR_RANGE, XTYPE_ERROR_DESCRIPTOR,
			"argument", "the generic type argument index is out of range");
		return NULL;
	}
	return pType->Arguments[iIndex];
}



/* 沿当前类型和基类查找名称及可选签名匹配的方法。 */
XRT_API const xrtmethoddesc* xrtTypeFindMethod(
	const xrttype* pType,
	xstrview Name,
	uint64 iSignatureId
)
{
	if ( (pType == NULL) || !__xrtTypeViewValid(&Name, false) ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_DESCRIPTOR,
			"find-method", "the type or method name is invalid");
		return NULL;
	}
	for ( uint32 iBase = 0;
		  (pType != NULL) &&
			(iBase < XRT_RUNTIME_TYPE_INHERITANCE_MAX);
		  iBase++, pType = pType->Base ) {
		const xrtmethodtable* pMethods = pType->Methods;

		if ( pMethods == NULL ) {
			continue;
		}
		for ( size_t i = 0; i < pMethods->Count; i++ ) {
			const xrtmethoddesc* pMethod = &pMethods->Methods[i];

			if (
				__xrtTypeViewEqual(&pMethod->Name, &Name) &&
				((iSignatureId == 0) ||
				 (xrtFunctionSigId(pMethod->Signature) == iSignatureId))
			) {
				return pMethod;
			}
		}
	}
	return NULL;
}



/* 使用自定义初始化或零初始化建立一个有效值。 */
XRT_API bool xrtTypeInitValue(const xrttype* pType, ptr pValue)
{
	if ( (pType == NULL) || ((pValue == NULL) && (pType->Size != 0u)) ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_OPERATION,
			"init", "the type or destination value is invalid");
		return false;
	}
	if ( (pType->Ops != NULL) && (pType->Ops->Init != NULL) ) {
		return pType->Ops->Init(pValue, pType);
	}
	if ( pType->Size != 0u ) {
		memset(pValue, 0, pType->Size);
	}
	return true;
}



/* 按类型声明复制一个值，自复制保持原值。 */
XRT_API bool xrtTypeCopyValue(
	const xrttype* pType,
	ptr pTarget,
	const void* pSource
)
{
	if (
		(pType == NULL) ||
		((pTarget == NULL) && (pType->Size != 0u)) ||
		((pSource == NULL) && (pType->Size != 0u))
	) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_OPERATION,
			"copy", "the type, source, or destination value is invalid");
		return false;
	}
	if ( (pType->Flags & XRT_TYPE_FLAG_COPYABLE) == 0 ) {
		__xrtRuntimeTypeError(XERR_UNSUPPORTED, XTYPE_ERROR_OPERATION,
			"copy", "the runtime type is not copyable");
		return false;
	}
	if ( pTarget == pSource ) {
		return true;
	}
	if ( (pType->Ops != NULL) && (pType->Ops->Copy != NULL) ) {
		return pType->Ops->Copy(pTarget, pSource, pType);
	}
	if ( (pType->Flags & XRT_TYPE_FLAG_TRIVIAL_COPY) == 0 ) {
		__xrtRuntimeTypeError(XERR_UNSUPPORTED, XTYPE_ERROR_OPERATION,
			"copy", "the runtime type has no copy operation");
		return false;
	}
	if ( pType->Size != 0u ) {
		memmove(pTarget, pSource, pType->Size);
	}
	return true;
}



/* 按类型声明移动一个值并把源值置为空状态。 */
XRT_API bool xrtTypeMoveValue(
	const xrttype* pType,
	ptr pTarget,
	ptr pSource
)
{
	if (
		(pType == NULL) ||
		((pTarget == NULL) && (pType->Size != 0u)) ||
		((pSource == NULL) && (pType->Size != 0u))
	) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_OPERATION,
			"move", "the type, source, or destination value is invalid");
		return false;
	}
	if ( (pType->Ops != NULL) && (pType->Ops->Move != NULL) ) {
		if ( pTarget == pSource ) {
			return true;
		}
		return pType->Ops->Move(pTarget, pSource, pType);
	}
	if ( (pType->Flags & XRT_TYPE_FLAG_TRIVIAL_COPY) == 0 ) {
		__xrtRuntimeTypeError(XERR_UNSUPPORTED, XTYPE_ERROR_OPERATION,
			"move", "the runtime type has no move operation");
		return false;
	}
	if ( pTarget == pSource ) {
		return true;
	}
	if ( pType->Size != 0u ) {
		memmove(pTarget, pSource, pType->Size);
		memset(pSource, 0, pType->Size);
	}
	return true;
}



/* 执行可选销毁操作，平凡值不需要额外处理。 */
XRT_API void xrtTypeDropValue(const xrttype* pType, ptr pValue)
{
	if ( (pType == NULL) || ((pValue == NULL) && (pType->Size != 0u)) ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_OPERATION,
			"drop", "the type or value is invalid");
		return;
	}
	if ( (pType->Ops != NULL) && (pType->Ops->Drop != NULL) ) {
		pType->Ops->Drop(pValue, pType);
	}
}



/* 优先深克隆一个值，无克隆操作时使用复制契约。 */
XRT_API bool xrtTypeCloneValue(
	const xrttype* pType,
	ptr pTarget,
	const void* pSource
)
{
	if (
		(pType == NULL) ||
		((pTarget == NULL) && (pType->Size != 0u)) ||
		((pSource == NULL) && (pType->Size != 0u))
	) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_OPERATION,
			"clone", "the type, source, or destination value is invalid");
		return false;
	}
	if ( pTarget == pSource ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_OPERATION,
			"clone", "clone source and destination must be distinct");
		return false;
	}
	if ( (pType->Ops != NULL) && (pType->Ops->Clone != NULL) ) {
		return pType->Ops->Clone(pTarget, pSource, pType);
	}
	return xrtTypeCopyValue(pType, pTarget, pSource);
}



/* 通过类型操作枚举值直接拥有的强对象引用。 */
XRT_API bool xrtTypeTraceValue(
	const xrttype* pType,
	const void* pValue,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	if (
		(pType == NULL) ||
		((pValue == NULL) && (pType->Size != 0u)) ||
		(pVisit == NULL)
	) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_OPERATION,
			"trace", "the type, value, or object visitor is invalid");
		return false;
	}
	if ( (pType->Ops == NULL) || (pType->Ops->Trace == NULL) ) {
		return true;
	}
	if ( !pType->Ops->Trace(pValue, pType, pVisit, pContext) ) {
		__xrtRuntimeTypeWrap(XERR_STATE, XTYPE_ERROR_OPERATION,
			"trace", "the runtime type reference trace failed");
		return false;
	}
	return true;
}



/* 使用实例初始化器或零初始化建立引用对象负载。 */
XRT_API bool xrtTypeInitInstance(const xrttype* pType, ptr pInstance)
{
	if ( (pType == NULL) ||
		 ((pInstance == NULL) && (pType->InstanceSize != 0u)) ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_OPERATION,
			"instance-init", "the type or instance payload is invalid");
		return false;
	}
	if ( (pType->InstanceOps != NULL) &&
		 (pType->InstanceOps->Init != NULL) ) {
		return pType->InstanceOps->Init(pInstance, pType);
	}
	if ( pType->InstanceSize != 0u ) {
		memset(pInstance, 0, pType->InstanceSize);
	}
	return true;
}



/* 执行引用对象负载的可选销毁操作。 */
XRT_API void xrtTypeDropInstance(const xrttype* pType, ptr pInstance)
{
	if ( (pType == NULL) ||
		 ((pInstance == NULL) && (pType->InstanceSize != 0u)) ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_OPERATION,
			"instance-drop", "the type or instance payload is invalid");
		return;
	}
	if ( (pType->InstanceOps != NULL) &&
		 (pType->InstanceOps->Drop != NULL) ) {
		pType->InstanceOps->Drop(pInstance, pType);
	}
}



/* 枚举引用对象负载直接拥有的全部强对象引用。 */
XRT_API bool xrtTypeTraceInstance(
	const xrttype* pType,
	const void* pInstance,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	if (
		(pType == NULL) ||
		((pInstance == NULL) && (pType->InstanceSize != 0u)) ||
		(pVisit == NULL)
	) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_OPERATION,
			"instance-trace", "the type, instance, or object visitor is invalid");
		return false;
	}
	if ( (pType->InstanceOps == NULL) ||
		 (pType->InstanceOps->Trace == NULL) ) {
		return true;
	}
	if ( !pType->InstanceOps->Trace(
		pInstance, pType, pVisit, pContext
	) ) {
		__xrtRuntimeTypeWrap(XERR_STATE, XTYPE_ERROR_OPERATION,
			"instance-trace", "the runtime object instance trace failed");
		return false;
	}
	return true;
}



/* 把 float 位模式映射为统一零值且可直接比较的总序键。 */
static uint64 __xrtTypeFloat32Key(const void* pValue)
{
	uint32 iBits;

	memcpy(&iBits, pValue, sizeof(iBits));
	if ( (iBits & UINT32_C(0x7FFFFFFF)) == 0u ) {
		iBits = 0u;
	}
	return (iBits & UINT32_C(0x80000000)) != 0u ?
		(uint64)(~iBits & UINT32_MAX) :
		(uint64)(iBits ^ UINT32_C(0x80000000));
}



/* 把 double 位模式映射为统一零值且可直接比较的总序键。 */
static uint64 __xrtTypeFloat64Key(const void* pValue)
{
	uint64 iBits;

	memcpy(&iBits, pValue, sizeof(iBits));
	if ( (iBits & UINT64_C(0x7FFFFFFFFFFFFFFF)) == 0u ) {
		iBits = 0u;
	}
	return (iBits & UINT64_C(0x8000000000000000)) != 0u ?
		~iBits : (iBits ^ UINT64_C(0x8000000000000000));
}



/* 比较内建标量形态，未知类型返回不支持。 */
static bool __xrtTypeBuiltinCompare(const xrttype* pType,
	const void* pLeft, const void* pRight, int* pResult)
{
	int64 iLeftSigned;
	int64 iRightSigned;
	uint64 iLeft;
	uint64 iRight;

	switch ( pType->Kind ) {
		case XRT_TYPE_NULL:
			*pResult = 0;
			return true;
		case XRT_TYPE_BOOL: {
			bool bLeft;
			bool bRight;

			if ( !__xrtTypeReadBool(pLeft, pType->Size, &bLeft) ||
				 !__xrtTypeReadBool(pRight, pType->Size, &bRight) ) {
				return false;
			}
			*pResult = (int)bLeft - (int)bRight;
			return true;
		}
		case XRT_TYPE_SIGNED_INT:
		case XRT_TYPE_TIME:
			if ( !__xrtTypeReadSigned(pLeft, pType->Size, &iLeftSigned) ||
				 !__xrtTypeReadSigned(pRight, pType->Size, &iRightSigned) ) {
				return false;
			}
			*pResult = (iLeftSigned > iRightSigned) -
				(iLeftSigned < iRightSigned);
			return true;
		case XRT_TYPE_UNSIGNED_INT:
		case XRT_TYPE_TYPE:
			if ( !__xrtTypeReadUnsigned(pLeft, pType->Size, &iLeft) ||
				 !__xrtTypeReadUnsigned(pRight, pType->Size, &iRight) ) {
				return false;
			}
			break;
		case XRT_TYPE_FLOAT:
			if ( pType->Size == sizeof(float) ) {
				iLeft = __xrtTypeFloat32Key(pLeft);
				iRight = __xrtTypeFloat32Key(pRight);
			} else if ( pType->Size == sizeof(double) ) {
				iLeft = __xrtTypeFloat64Key(pLeft);
				iRight = __xrtTypeFloat64Key(pRight);
			} else {
				return false;
			}
			break;
		case XRT_TYPE_POINTER: {
			ptr pLeftValue;
			ptr pRightValue;

			if ( pType->Size != sizeof(ptr) ) {
				return false;
			}
			memcpy(&pLeftValue, pLeft, sizeof(pLeftValue));
			memcpy(&pRightValue, pRight, sizeof(pRightValue));
			iLeft = (uint64)(uintptr_t)pLeftValue;
			iRight = (uint64)(uintptr_t)pRightValue;
			break;
		}
		default:
			return false;
	}
	*pResult = (iLeft > iRight) - (iLeft < iRight);
	return true;
}



/* 散列内建标量形态，规则与内建比较的相等关系保持一致。 */
static bool __xrtTypeBuiltinHash(const xrttype* pType,
	const void* pValue, uint64* pHash)
{
	uint64 iValue;
	int64 iSigned;

	switch ( pType->Kind ) {
		case XRT_TYPE_NULL:
			iValue = 0u;
			break;
		case XRT_TYPE_BOOL: {
			bool bValue;

			if ( !__xrtTypeReadBool(pValue, pType->Size, &bValue) ) {
				return false;
			}
			iValue = bValue ? 1u : 0u;
			break;
		}
		case XRT_TYPE_SIGNED_INT:
		case XRT_TYPE_TIME:
			if ( !__xrtTypeReadSigned(pValue, pType->Size, &iSigned) ) {
				return false;
			}
			iValue = (uint64)iSigned;
			break;
		case XRT_TYPE_UNSIGNED_INT:
		case XRT_TYPE_TYPE:
			if ( !__xrtTypeReadUnsigned(pValue, pType->Size, &iValue) ) {
				return false;
			}
			break;
		case XRT_TYPE_FLOAT:
			if ( pType->Size == sizeof(float) ) {
				iValue = __xrtTypeFloat32Key(pValue);
			} else if ( pType->Size == sizeof(double) ) {
				iValue = __xrtTypeFloat64Key(pValue);
			} else {
				return false;
			}
			break;
		case XRT_TYPE_POINTER: {
			ptr pPointer;

			if ( pType->Size != sizeof(ptr) ) {
				return false;
			}
			memcpy(&pPointer, pValue, sizeof(pPointer));
			iValue = (uint64)(uintptr_t)pPointer;
			break;
		}
		default:
			return false;
	}
	*pHash = __xrtTypeHashU64(
		UINT64_C(14695981039346656037), iValue);
	return true;
}



/* 使用类型比较操作并在成功后提交比较结果。 */
XRT_API bool xrtTypeCompareValue(const xrttype* pType,
	const void* pLeft, const void* pRight, int* pResult)
{
	int iResult;

	if ( (pType == NULL) || (pResult == NULL) ||
		 ((pLeft == NULL) && (pType->Size != 0u)) ||
		 ((pRight == NULL) && (pType->Size != 0u)) ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_OPERATION,
			"compare", "the type, values, or result output is invalid");
		return false;
	}
	if ( (pType->Ops != NULL) && (pType->Ops->Compare != NULL) ) {
		iResult = pType->Ops->Compare(pLeft, pRight, pType);
	} else if ( !__xrtTypeBuiltinCompare(
		pType, pLeft, pRight, &iResult) ) {
		__xrtRuntimeTypeError(XERR_UNSUPPORTED, XTYPE_ERROR_OPERATION,
			"compare", "the runtime type has no comparison operation");
		return false;
	}
	*pResult = iResult;
	return true;
}



/* 使用类型散列操作并在成功后提交散列值。 */
XRT_API bool xrtTypeHashValue(const xrttype* pType,
	const void* pValue, uint64* pHash)
{
	uint64 iHash;

	if ( (pType == NULL) || (pHash == NULL) ||
		 ((pValue == NULL) && (pType->Size != 0u)) ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_OPERATION,
			"hash", "the type, value, or hash output is invalid");
		return false;
	}
	if ( (pType->Ops != NULL) && (pType->Ops->Hash != NULL) ) {
		iHash = pType->Ops->Hash(pValue, pType);
	} else if ( !__xrtTypeBuiltinHash(pType, pValue, &iHash) ) {
		__xrtRuntimeTypeError(XERR_UNSUPPORTED, XTYPE_ERROR_OPERATION,
			"hash", "the runtime type has no hash operation");
		return false;
	}
	*pHash = iHash;
	return true;
}



/* 在按类型 ID 排序的注册表中查找第一个不小于目标 ID 的位置。 */
static size_t __xrtTypeRegistryLowerBound(
	const xrttyperegistry* pRegistry, uint64 iTypeId)
{
	size_t iBegin = 0u;
	size_t iEnd = pRegistry->Count;

	while ( iBegin < iEnd ) {
		size_t iMiddle = iBegin + ((iEnd - iBegin) / 2u);

		if ( pRegistry->Types[iMiddle]->Id < iTypeId ) {
			iBegin = iMiddle + 1u;
		} else {
			iEnd = iMiddle;
		}
	}
	return iBegin;
}



/* 为类型注册表增长紧凑指针数组。 */
static bool __xrtTypeRegistryGrow(xrttyperegistry* pRegistry)
{
	const xrttype** pTypes;
	size_t iCapacity;

	if ( pRegistry->Count != pRegistry->Capacity ) {
		return true;
	}
	if ( pRegistry->Capacity > (SIZE_MAX / 2u) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iCapacity = pRegistry->Capacity != 0u ?
		pRegistry->Capacity * 2u : 16u;
	if ( iCapacity > (SIZE_MAX / sizeof(const xrttype*)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	pTypes = (const xrttype**)xrtRealloc(pRegistry->Types,
		iCapacity * sizeof(const xrttype*));
	if ( pTypes == NULL ) {
		return false;
	}
	pRegistry->Types = pTypes;
	pRegistry->Capacity = iCapacity;
	return true;
}



/* 创建空的线程安全类型注册表。 */
XRT_API xrttyperegistry* xrtTypeRegistryCreate(void)
{
	xrttyperegistry* pRegistry =
		(xrttyperegistry*)xrtCalloc(1u, sizeof(xrttyperegistry));

	if ( pRegistry != NULL ) {
		__xrtSpinInit(&pRegistry->Lock);
	}
	return pRegistry;
}



/* 销毁注册表自身，不销毁任何借用描述。 */
XRT_API void xrtTypeRegistryDestroy(xrttyperegistry* pRegistry)
{
	if ( pRegistry == NULL ) {
		return;
	}
	__xrtSpinUnit(&pRegistry->Lock);
	xrtFree(pRegistry->Types);
	xrtFree(pRegistry);
}



/* 注册唯一描述指针并维持数组按稳定 ID 排序。 */
XRT_API bool xrtTypeRegistryAdd(
	xrttyperegistry* pRegistry,
	const xrttype* pType
)
{
	size_t iIndex;

	if ( (pRegistry == NULL) || !xrtTypeValidate(pType) ) {
		if ( pRegistry == NULL ) {
			__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_REGISTRY,
				"registry-add", "the type registry is null");
		}
		return false;
	}
	__xrtSpinLock(&pRegistry->Lock);
	iIndex = __xrtTypeRegistryLowerBound(pRegistry, pType->Id);
	if ( (iIndex < pRegistry->Count) &&
		 (pRegistry->Types[iIndex]->Id == pType->Id) ) {
		if ( pRegistry->Types[iIndex] == pType ) {
			__xrtSpinUnlock(&pRegistry->Lock);
			return true;
		}
		__xrtSpinUnlock(&pRegistry->Lock);
		__xrtRuntimeTypeError(XERR_EXISTS, XTYPE_ERROR_REGISTRY,
			"registry-add", "the stable type identity already has a descriptor");
		return false;
	}
	if ( !__xrtTypeRegistryGrow(pRegistry) ) {
		__xrtSpinUnlock(&pRegistry->Lock);
		return false;
	}
	if ( iIndex < pRegistry->Count ) {
		memmove(&pRegistry->Types[iIndex + 1u],
			&pRegistry->Types[iIndex],
			(pRegistry->Count - iIndex) * sizeof(const xrttype*));
	}
	pRegistry->Types[iIndex] = pType;
	pRegistry->Count++;
	__xrtSpinUnlock(&pRegistry->Lock);
	return true;
}



/* 按注册时的准确描述指针移除类型；未注册是正常的 false 结果。 */
XRT_API bool xrtTypeRegistryRemove(
	xrttyperegistry* pRegistry,
	const xrttype* pType
)
{
	size_t iIndex;

	if ( (pRegistry == NULL) || (pType == NULL) || (pType->Id == 0u) ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_REGISTRY,
			"registry-remove", "the type registry or descriptor is invalid");
		return false;
	}
	__xrtSpinLock(&pRegistry->Lock);
	iIndex = __xrtTypeRegistryLowerBound(pRegistry, pType->Id);
	if ( (iIndex >= pRegistry->Count) ||
		 (pRegistry->Types[iIndex] != pType) ) {
		__xrtSpinUnlock(&pRegistry->Lock);
		return false;
	}
	pRegistry->Count--;
	if ( iIndex < pRegistry->Count ) {
		memmove(&pRegistry->Types[iIndex],
			&pRegistry->Types[iIndex + 1u],
			(pRegistry->Count - iIndex) * sizeof(const xrttype*));
	}
	__xrtSpinUnlock(&pRegistry->Lock);
	return true;
}



/* 在线程安全快照下返回已注册类型数量。 */
XRT_API size_t xrtTypeRegistryCount(const xrttyperegistry* pRegistry)
{
	size_t iCount;

	if ( pRegistry == NULL ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_REGISTRY,
			"registry-count", "the type registry is null");
		return 0;
	}
	__xrtSpinLock((xrt_spinlock*)&pRegistry->Lock);
	iCount = pRegistry->Count;
	__xrtSpinUnlock((xrt_spinlock*)&pRegistry->Lock);
	return iCount;
}



/* 在线程安全快照下按稳定类型 ID 顺序读取一个借用描述。 */
XRT_API const xrttype* xrtTypeRegistryAt(
	const xrttyperegistry* pRegistry,
	size_t iIndex
)
{
	const xrttype* pType;

	if ( pRegistry == NULL ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_REGISTRY,
			"registry-at", "the type registry is null");
		return NULL;
	}
	__xrtSpinLock((xrt_spinlock*)&pRegistry->Lock);
	if ( iIndex >= pRegistry->Count ) {
		__xrtSpinUnlock((xrt_spinlock*)&pRegistry->Lock);
		__xrtRuntimeTypeError(XERR_RANGE, XTYPE_ERROR_REGISTRY,
			"registry-at", "the type registry index is out of range");
		return NULL;
	}
	pType = pRegistry->Types[iIndex];
	__xrtSpinUnlock((xrt_spinlock*)&pRegistry->Lock);
	return pType;
}



/* 按稳定 ID 二分查询借用的类型描述。 */
XRT_API const xrttype* xrtTypeRegistryFindId(
	const xrttyperegistry* pRegistry,
	uint64 iTypeId
)
{
	const xrttype* pResult = NULL;
	size_t iIndex;

	if ( (pRegistry == NULL) || (iTypeId == 0) ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_REGISTRY,
			"registry-find-id", "the type registry or type ID is invalid");
		return NULL;
	}
	__xrtSpinLock((xrt_spinlock*)&pRegistry->Lock);
	iIndex = __xrtTypeRegistryLowerBound(pRegistry, iTypeId);
	if ( (iIndex < pRegistry->Count) &&
		 (pRegistry->Types[iIndex]->Id == iTypeId) ) {
		pResult = pRegistry->Types[iIndex];
	}
	__xrtSpinUnlock((xrt_spinlock*)&pRegistry->Lock);
	return pResult;
}



/* 由规范 ABI 名计算 ID 并二分查询借用描述。 */
XRT_API const xrttype* xrtTypeRegistryFindName(
	const xrttyperegistry* pRegistry,
	xstrview AbiName
)
{
	const xrttype* pResult = NULL;
	uint64 iTypeId;
	size_t iIndex;

	if ( (pRegistry == NULL) || !__xrtTypeViewValid(&AbiName, false) ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_REGISTRY,
			"registry-find-name", "the type registry or ABI name is invalid");
		return NULL;
	}
	iTypeId = __xrtTypeComputedId(AbiName);
	__xrtSpinLock((xrt_spinlock*)&pRegistry->Lock);
	iIndex = __xrtTypeRegistryLowerBound(pRegistry, iTypeId);
	if ( (iIndex < pRegistry->Count) &&
		 (pRegistry->Types[iIndex]->Id == iTypeId) &&
		 __xrtTypeViewEqual(&pRegistry->Types[iIndex]->AbiName, &AbiName) ) {
		pResult = pRegistry->Types[iIndex];
	}
	__xrtSpinUnlock((xrt_spinlock*)&pRegistry->Lock);
	return pResult;
}



/* 验证一个协议要求的名称和函数签名。 */
static bool __xrtProtocolRequirementValid(
	const xrtprotocolrequirement* pRequirement
)
{
	return (pRequirement != NULL) &&
		__xrtTypeViewValid(&pRequirement->Name, false) &&
		__xrtFunctionSigValidate(pRequirement->Signature);
}



/* 检查协议类型、要求数组以及每个重载身份的唯一性。 */
static bool __xrtProtocolValidate(const xrtprotocol* pProtocol)
{
	if (
		(pProtocol == NULL) ||
		!xrtTypeValidate(pProtocol->Type) ||
		(pProtocol->Type->Kind != XRT_TYPE_PROTOCOL) ||
		((pProtocol->RequirementCount != 0u) &&
		 (pProtocol->Requirements == NULL))
	) {
		return false;
	}
	for ( size_t i = 0; i < pProtocol->RequirementCount; i++ ) {
		const xrtprotocolrequirement* pRequirement =
			&pProtocol->Requirements[i];

		if ( !__xrtProtocolRequirementValid(pRequirement) ) {
			return false;
		}
		for ( size_t j = 0; j < i; j++ ) {
			const xrtprotocolrequirement* pPrevious =
				&pProtocol->Requirements[j];

			if (
				__xrtTypeViewEqual(&pPrevious->Name, &pRequirement->Name) &&
				(__xrtFunctionSigComputedId(pPrevious->Signature) ==
				 __xrtFunctionSigComputedId(pRequirement->Signature))
			) {
				return false;
			}
		}
	}
	return true;
}



/* 验证协议描述自身，不要求先构造具体类型见证。 */
XRT_API bool xrtProtocolValidate(const xrtprotocol* pProtocol)
{
	if ( !__xrtProtocolValidate(pProtocol) ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_PROTOCOL,
			"protocol-validate", "the protocol descriptor is invalid");
		return false;
	}
	return true;
}



/* 验证协议和具体类型，并逐项核对唯一见证入口。 */
XRT_API bool xrtProtocolWitnessValidate(
	const xrtprotocolwitness* pWitness
)
{
	const xrtprotocol* pProtocol;

	if (
		(pWitness == NULL) ||
		(pWitness->Protocol == NULL) ||
		(pWitness->ConcreteType == NULL) ||
		((pWitness->EntryCount != 0) && (pWitness->Entries == NULL))
	) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_PROTOCOL,
			"witness-validate", "the protocol witness structure is invalid");
		return false;
	}
	pProtocol = pWitness->Protocol;
	if (
		!__xrtProtocolValidate(pProtocol) ||
		!xrtTypeValidate(pWitness->ConcreteType) ||
		(pWitness->EntryCount != pProtocol->RequirementCount)
	) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_PROTOCOL,
			"witness-validate", "the protocol or witness shape is invalid");
		return false;
	}
	for ( size_t i = 0; i < pWitness->EntryCount; i++ ) {
		const xrtprotocolentry* pEntry = &pWitness->Entries[i];

		if (
			!__xrtTypeViewValid(&pEntry->Name, false) ||
			!__xrtFunctionSigValidate(pEntry->Signature) ||
			(pEntry->Entry == NULL)
		) {
			__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_PROTOCOL,
				"witness-validate", "a protocol witness entry is invalid");
			return false;
		}
	}
	for ( size_t i = 0; i < pProtocol->RequirementCount; i++ ) {
		const xrtprotocolrequirement* pRequirement =
			&pProtocol->Requirements[i];
		size_t iMatches = 0;

		for ( size_t j = 0; j < pWitness->EntryCount; j++ ) {
			const xrtprotocolentry* pEntry = &pWitness->Entries[j];

			if (
				__xrtTypeViewEqual(&pEntry->Name, &pRequirement->Name) &&
				(__xrtFunctionSigComputedId(pEntry->Signature) ==
				 __xrtFunctionSigComputedId(pRequirement->Signature))
			) {
				iMatches++;
			}
		}
		if ( iMatches != 1u ) {
			__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_PROTOCOL,
				"witness-validate", "each protocol requirement must have one witness entry");
			return false;
		}
	}
	return true;
}



/* 按名称和可选签名查询见证入口。 */
XRT_API const xrtprotocolentry* xrtProtocolWitnessFind(
	const xrtprotocolwitness* pWitness,
	xstrview Name,
	uint64 iSignatureId
)
{
	if (
		(pWitness == NULL) ||
		!__xrtTypeViewValid(&Name, false) ||
		((pWitness->EntryCount != 0) && (pWitness->Entries == NULL))
	) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_PROTOCOL,
			"witness-find", "the protocol witness or method name is invalid");
		return NULL;
	}
	for ( size_t i = 0; i < pWitness->EntryCount; i++ ) {
		const xrtprotocolentry* pEntry = &pWitness->Entries[i];

		if (
			!__xrtTypeViewValid(&pEntry->Name, false) ||
			!__xrtFunctionSigValidate(pEntry->Signature) ||
			(pEntry->Entry == NULL)
		) {
			__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_PROTOCOL,
				"witness-find", "a protocol witness entry is invalid");
			return NULL;
		}
		if (
			__xrtTypeViewEqual(&pEntry->Name, &Name) &&
			((iSignatureId == 0) ||
			 (__xrtFunctionSigComputedId(pEntry->Signature) == iSignatureId))
		) {
			return pEntry;
		}
	}
	return NULL;
}



/* 比较协议和具体类型 ID 组成的稳定见证键。 */
static int __xrtProtocolKeyCompare(const xrtprotocolwitness* pWitness,
	uint64 iProtocolTypeId, uint64 iConcreteTypeId)
{
	uint64 iProtocol = pWitness->Protocol->Type->Id;
	uint64 iConcrete = pWitness->ConcreteType->Id;

	if ( iProtocol != iProtocolTypeId ) {
		return iProtocol < iProtocolTypeId ? -1 : 1;
	}
	if ( iConcrete != iConcreteTypeId ) {
		return iConcrete < iConcreteTypeId ? -1 : 1;
	}
	return 0;
}



/* 在按稳定见证键排序的注册表中执行二分下界查询。 */
static size_t __xrtProtocolRegistryLowerBound(
	const xrtprotocolregistry* pRegistry,
	uint64 iProtocolTypeId, uint64 iConcreteTypeId)
{
	size_t iBegin = 0u;
	size_t iEnd = pRegistry->Count;

	while ( iBegin < iEnd ) {
		size_t iMiddle = iBegin + ((iEnd - iBegin) / 2u);
		int iCompare = __xrtProtocolKeyCompare(
			pRegistry->Witnesses[iMiddle], iProtocolTypeId, iConcreteTypeId);

		if ( iCompare < 0 ) {
			iBegin = iMiddle + 1u;
		} else {
			iEnd = iMiddle;
		}
	}
	return iBegin;
}



/* 为协议注册表增长紧凑见证指针数组。 */
static bool __xrtProtocolRegistryGrow(xrtprotocolregistry* pRegistry)
{
	const xrtprotocolwitness** pWitnesses;
	size_t iCapacity;

	if ( pRegistry->Count != pRegistry->Capacity ) {
		return true;
	}
	if ( pRegistry->Capacity > (SIZE_MAX / 2u) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iCapacity = pRegistry->Capacity != 0u ?
		pRegistry->Capacity * 2u : 16u;
	if ( iCapacity > (SIZE_MAX / sizeof(const xrtprotocolwitness*)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	pWitnesses = (const xrtprotocolwitness**)xrtRealloc(
		pRegistry->Witnesses,
		iCapacity * sizeof(const xrtprotocolwitness*));
	if ( pWitnesses == NULL ) {
		return false;
	}
	pRegistry->Witnesses = pWitnesses;
	pRegistry->Capacity = iCapacity;
	return true;
}



/* 创建空的线程安全协议见证注册表。 */
XRT_API xrtprotocolregistry* xrtProtocolRegistryCreate(void)
{
	xrtprotocolregistry* pRegistry =
		(xrtprotocolregistry*)xrtCalloc(1u, sizeof(xrtprotocolregistry));

	if ( pRegistry != NULL ) {
		__xrtSpinInit(&pRegistry->Lock);
	}
	return pRegistry;
}



/* 销毁协议注册表自身，不销毁任何借用见证。 */
XRT_API void xrtProtocolRegistryDestroy(xrtprotocolregistry* pRegistry)
{
	if ( pRegistry == NULL ) {
		return;
	}
	__xrtSpinUnit(&pRegistry->Lock);
	xrtFree(pRegistry->Witnesses);
	xrtFree(pRegistry);
}



/* 按协议和具体类型组成的键注册唯一见证指针。 */
XRT_API bool xrtProtocolRegistryAdd(
	xrtprotocolregistry* pRegistry,
	const xrtprotocolwitness* pWitness
)
{
	uint64 iProtocolTypeId;
	uint64 iConcreteTypeId;
	size_t iIndex;

	if ( (pRegistry == NULL) || !xrtProtocolWitnessValidate(pWitness) ) {
		if ( pRegistry == NULL ) {
			__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_REGISTRY,
				"protocol-add", "the protocol registry is null");
		}
		return false;
	}
	iProtocolTypeId = pWitness->Protocol->Type->Id;
	iConcreteTypeId = pWitness->ConcreteType->Id;
	__xrtSpinLock(&pRegistry->Lock);
	iIndex = __xrtProtocolRegistryLowerBound(
		pRegistry, iProtocolTypeId, iConcreteTypeId);
	if ( (iIndex < pRegistry->Count) &&
		 (__xrtProtocolKeyCompare(pRegistry->Witnesses[iIndex],
			iProtocolTypeId, iConcreteTypeId) == 0) ) {
		if ( pRegistry->Witnesses[iIndex] == pWitness ) {
			__xrtSpinUnlock(&pRegistry->Lock);
			return true;
		}
		__xrtSpinUnlock(&pRegistry->Lock);
		__xrtRuntimeTypeError(XERR_EXISTS, XTYPE_ERROR_REGISTRY,
			"protocol-add", "a witness for this protocol and type already exists");
		return false;
	}
	if ( !__xrtProtocolRegistryGrow(pRegistry) ) {
		__xrtSpinUnlock(&pRegistry->Lock);
		return false;
	}
	if ( iIndex < pRegistry->Count ) {
		memmove(&pRegistry->Witnesses[iIndex + 1u],
			&pRegistry->Witnesses[iIndex],
			(pRegistry->Count - iIndex) * sizeof(const xrtprotocolwitness*));
	}
	pRegistry->Witnesses[iIndex] = pWitness;
	pRegistry->Count++;
	__xrtSpinUnlock(&pRegistry->Lock);
	return true;
}



/* 按准确见证指针移除注册项。 */
XRT_API bool xrtProtocolRegistryRemove(
	xrtprotocolregistry* pRegistry,
	const xrtprotocolwitness* pWitness
)
{
	if ( (pRegistry == NULL) || !xrtProtocolWitnessValidate(pWitness) ) {
		if ( pRegistry == NULL ) {
			__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_REGISTRY,
				"protocol-remove", "the protocol registry is null");
		}
		return false;
	}
	__xrtSpinLock(&pRegistry->Lock);
	{
		size_t iIndex = __xrtProtocolRegistryLowerBound(pRegistry,
			pWitness->Protocol->Type->Id, pWitness->ConcreteType->Id);

		if ( (iIndex < pRegistry->Count) &&
			 (pRegistry->Witnesses[iIndex] == pWitness) ) {
			pRegistry->Count--;
			if ( iIndex < pRegistry->Count ) {
				memmove(&pRegistry->Witnesses[iIndex],
					&pRegistry->Witnesses[iIndex + 1u],
					(pRegistry->Count - iIndex) *
						sizeof(const xrtprotocolwitness*));
			}
			__xrtSpinUnlock(&pRegistry->Lock);
			return true;
		}
	}
	__xrtSpinUnlock(&pRegistry->Lock);
	return false;
}



/* 按协议和具体类型 ID 二分查询见证。 */
XRT_API const xrtprotocolwitness* xrtProtocolRegistryFind(
	const xrtprotocolregistry* pRegistry,
	uint64 iProtocolTypeId,
	uint64 iConcreteTypeId
)
{
	const xrtprotocolwitness* pResult = NULL;
	size_t iIndex;

	if (
		(pRegistry == NULL) ||
		(iProtocolTypeId == 0) ||
		(iConcreteTypeId == 0)
	) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_REGISTRY,
			"protocol-find", "the protocol registry or lookup IDs are invalid");
		return NULL;
	}
	__xrtSpinLock((xrt_spinlock*)&pRegistry->Lock);
	iIndex = __xrtProtocolRegistryLowerBound(
		pRegistry, iProtocolTypeId, iConcreteTypeId);
	if ( (iIndex < pRegistry->Count) &&
		 (__xrtProtocolKeyCompare(pRegistry->Witnesses[iIndex],
			iProtocolTypeId, iConcreteTypeId) == 0) ) {
		pResult = pRegistry->Witnesses[iIndex];
	}
	__xrtSpinUnlock((xrt_spinlock*)&pRegistry->Lock);
	return pResult;
}



/* 在线程安全快照下返回已注册见证数量。 */
XRT_API size_t xrtProtocolRegistryCount(
	const xrtprotocolregistry* pRegistry
)
{
	size_t iCount;

	if ( pRegistry == NULL ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_REGISTRY,
			"protocol-count", "the protocol registry is null");
		return 0;
	}
	__xrtSpinLock((xrt_spinlock*)&pRegistry->Lock);
	iCount = pRegistry->Count;
	__xrtSpinUnlock((xrt_spinlock*)&pRegistry->Lock);
	return iCount;
}



/* 在线程安全快照下按协议和具体类型 ID 顺序读取一个借用见证。 */
XRT_API const xrtprotocolwitness* xrtProtocolRegistryAt(
	const xrtprotocolregistry* pRegistry,
	size_t iIndex
)
{
	const xrtprotocolwitness* pWitness;

	if ( pRegistry == NULL ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_REGISTRY,
			"protocol-at", "the protocol registry is null");
		return NULL;
	}
	__xrtSpinLock((xrt_spinlock*)&pRegistry->Lock);
	if ( iIndex >= pRegistry->Count ) {
		__xrtSpinUnlock((xrt_spinlock*)&pRegistry->Lock);
		__xrtRuntimeTypeError(XERR_RANGE, XTYPE_ERROR_REGISTRY,
			"protocol-at", "the protocol registry index is out of range");
		return NULL;
	}
	pWitness = pRegistry->Witnesses[iIndex];
	__xrtSpinUnlock((xrt_spinlock*)&pRegistry->Lock);
	return pWitness;
}



/* 验证枚举类型以及所有变体名称、标签和负载身份。 */
XRT_API bool xrtEnumValidate(const xrtenum* pEnum)
{
	if (
		(pEnum == NULL) ||
		!xrtTypeValidate(pEnum->Type) ||
		(pEnum->Type->Kind != XRT_TYPE_ENUM) ||
		((pEnum->VariantCount != 0) && (pEnum->Variants == NULL))
	) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_ENUM,
			"enum-validate", "the enum descriptor is invalid");
		return false;
	}
	for ( size_t i = 0; i < pEnum->VariantCount; i++ ) {
		const xrtenumvariant* pVariant = &pEnum->Variants[i];

		if (
			!__xrtTypeViewValid(&pVariant->Name, false) ||
			((pVariant->PayloadType != NULL) &&
			 !__xrtTypeIdentityValid(pVariant->PayloadType))
		) {
			__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_ENUM,
				"enum-validate", "an enum variant is invalid");
			return false;
		}
		for ( size_t j = 0; j < i; j++ ) {
			if (
				(pEnum->Variants[j].Tag == pVariant->Tag) ||
				__xrtTypeViewEqual(
					&pEnum->Variants[j].Name,
					&pVariant->Name
				)
			) {
				__xrtRuntimeTypeError(XERR_EXISTS, XTYPE_ERROR_ENUM,
					"enum-validate", "enum variant names and tags must be unique");
				return false;
			}
		}
	}
	return true;
}



/* 按标签线性查询借用的枚举变体。 */
XRT_API const xrtenumvariant* xrtEnumFindTag(
	const xrtenum* pEnum,
	int64 iTag
)
{
	if ( (pEnum == NULL) ||
		 ((pEnum->VariantCount != 0u) && (pEnum->Variants == NULL)) ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_ENUM,
			"enum-find-tag", "the enum descriptor is invalid");
		return NULL;
	}
	for ( size_t i = 0; i < pEnum->VariantCount; i++ ) {
		if ( pEnum->Variants[i].Tag == iTag ) {
			return &pEnum->Variants[i];
		}
	}
	return NULL;
}



/* 按名称线性查询借用的枚举变体。 */
XRT_API const xrtenumvariant* xrtEnumFindName(
	const xrtenum* pEnum,
	xstrview Name
)
{
	if ( (pEnum == NULL) || !__xrtTypeViewValid(&Name, false) ||
		 ((pEnum->VariantCount != 0u) && (pEnum->Variants == NULL)) ) {
		__xrtRuntimeTypeError(XERR_ARGUMENT, XTYPE_ERROR_ENUM,
			"enum-find-name", "the enum descriptor or variant name is invalid");
		return NULL;
	}
	for ( size_t i = 0; i < pEnum->VariantCount; i++ ) {
		if ( __xrtTypeViewEqual(&pEnum->Variants[i].Name, &Name) ) {
			return &pEnum->Variants[i];
		}
	}
	return NULL;
}



#define XRT_BUILTIN_TYPE( \
	Function, IdValue, KindValue, FlagsValue, NameValue, CType \
) \
	XRT_API const xrttype* Function(void) \
	{ \
		static const xrttype Type = { \
			.Id = IdValue, \
			.Kind = KindValue, \
			.Flags = (FlagsValue) | XRT_TYPE_FLAG_RELOCATABLE, \
			.Name = XRT_STR_INIT(NameValue), \
			.AbiName = XRT_STR_INIT("xrt." NameValue), \
			.Size = sizeof(CType), \
			.Align = XRT_INTERNAL_ALIGNOF(CType), \
			.InstanceSize = sizeof(CType), \
			.InstanceAlign = XRT_INTERNAL_ALIGNOF(CType) \
		}; \
		return &Type; \
	}



/* 返回进程期稳定的 null 类型描述。 */
XRT_API const xrttype* xrtTypeNull(void)
{
	static const xrttype Type = {
		.Id = UINT64_C(0x6950D50F203E7DCC),
		.Kind = XRT_TYPE_NULL,
		.Flags = XRT_TYPE_FLAG_TRIVIAL_COPY | XRT_TYPE_FLAG_TRIVIAL_DROP |
			XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_FINAL |
			XRT_TYPE_FLAG_RELOCATABLE,
		.Name = XRT_STR_INIT("null"),
		.AbiName = XRT_STR_INIT("xrt.null"),
		.Size = 0u,
		.Align = 1u,
		.InstanceSize = 0u,
		.InstanceAlign = 1u
	};
	return &Type;
}



XRT_BUILTIN_TYPE(
	xrtTypeBool, UINT64_C(0x0DDD5573D01F8925),
	XRT_TYPE_BOOL,
	XRT_TYPE_FLAG_TRIVIAL_COPY | XRT_TYPE_FLAG_TRIVIAL_DROP |
		XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_FINAL,
	"bool", bool
)
XRT_BUILTIN_TYPE(
	xrtTypeBool32, UINT64_C(0x5200B3575DC757F0),
	XRT_TYPE_BOOL,
	XRT_TYPE_FLAG_TRIVIAL_COPY | XRT_TYPE_FLAG_TRIVIAL_DROP |
		XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_FINAL,
	"bool32", int32
)
XRT_BUILTIN_TYPE(
	xrtTypeInt8, UINT64_C(0x8C0A1E1A952DE77A),
	XRT_TYPE_SIGNED_INT,
	XRT_TYPE_FLAG_TRIVIAL_COPY | XRT_TYPE_FLAG_TRIVIAL_DROP |
		XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_FINAL,
	"int8", int8
)
XRT_BUILTIN_TYPE(
	xrtTypeUInt8, UINT64_C(0x6F71A731A529D6F3),
	XRT_TYPE_UNSIGNED_INT,
	XRT_TYPE_FLAG_TRIVIAL_COPY | XRT_TYPE_FLAG_TRIVIAL_DROP |
		XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_FINAL,
	"uint8", uint8
)
XRT_BUILTIN_TYPE(
	xrtTypeInt16, UINT64_C(0x2300E52B7CEC35F9),
	XRT_TYPE_SIGNED_INT,
	XRT_TYPE_FLAG_TRIVIAL_COPY | XRT_TYPE_FLAG_TRIVIAL_DROP |
		XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_FINAL,
	"int16", int16
)
XRT_BUILTIN_TYPE(
	xrtTypeUInt16, UINT64_C(0x880DEC5BA62C9A6A),
	XRT_TYPE_UNSIGNED_INT,
	XRT_TYPE_FLAG_TRIVIAL_COPY | XRT_TYPE_FLAG_TRIVIAL_DROP |
		XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_FINAL,
	"uint16", uint16
)
XRT_BUILTIN_TYPE(
	xrtTypeInt32, UINT64_C(0x22F9F92B7CE63947),
	XRT_TYPE_SIGNED_INT,
	XRT_TYPE_FLAG_TRIVIAL_COPY | XRT_TYPE_FLAG_TRIVIAL_DROP |
		XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_FINAL,
	"int32", int32
)
XRT_BUILTIN_TYPE(
	xrtTypeUInt32, UINT64_C(0x8814705BA631E664),
	XRT_TYPE_UNSIGNED_INT,
	XRT_TYPE_FLAG_TRIVIAL_COPY | XRT_TYPE_FLAG_TRIVIAL_DROP |
		XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_FINAL,
	"uint32", uint32
)
XRT_BUILTIN_TYPE(
	xrtTypeInt64, UINT64_C(0x22E8E12B7CD79D4C),
	XRT_TYPE_SIGNED_INT,
	XRT_TYPE_FLAG_TRIVIAL_COPY | XRT_TYPE_FLAG_TRIVIAL_DROP |
		XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_FINAL,
	"int64", int64
)
XRT_BUILTIN_TYPE(
	xrtTypeUInt64, UINT64_C(0x88256C5BA64052CB),
	XRT_TYPE_UNSIGNED_INT,
	XRT_TYPE_FLAG_TRIVIAL_COPY | XRT_TYPE_FLAG_TRIVIAL_DROP |
		XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_FINAL,
	"uint64", uint64
)
XRT_BUILTIN_TYPE(
	xrtTypeFloat32, UINT64_C(0x4686ECDDD67F49D8),
	XRT_TYPE_FLOAT,
	XRT_TYPE_FLAG_TRIVIAL_COPY | XRT_TYPE_FLAG_TRIVIAL_DROP |
		XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_FINAL,
	"float32", float
)
XRT_BUILTIN_TYPE(
	xrtTypeFloat64, UINT64_C(0x467CE0DDD676E0EF),
	XRT_TYPE_FLOAT,
	XRT_TYPE_FLAG_TRIVIAL_COPY | XRT_TYPE_FLAG_TRIVIAL_DROP |
		XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_FINAL,
	"float64", double
)
XRT_BUILTIN_TYPE(
	xrtTypeTime, UINT64_C(0x347415BB369EAF3C),
	XRT_TYPE_TIME,
	XRT_TYPE_FLAG_TRIVIAL_COPY | XRT_TYPE_FLAG_TRIVIAL_DROP |
		XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_FINAL,
	"time", xtime
)
XRT_BUILTIN_TYPE(
	xrtTypePointer, UINT64_C(0x55A641CF4B82EC82),
	XRT_TYPE_POINTER,
	XRT_TYPE_FLAG_TRIVIAL_COPY | XRT_TYPE_FLAG_TRIVIAL_DROP |
		XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_NULLABLE |
		XRT_TYPE_FLAG_FINAL,
	"pointer", ptr
)
XRT_BUILTIN_TYPE(
	xrtTypeType, UINT64_C(0xC150A9BB870BECF5),
	XRT_TYPE_TYPE,
	XRT_TYPE_FLAG_TRIVIAL_COPY | XRT_TYPE_FLAG_TRIVIAL_DROP |
		XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_FINAL,
	"type", uint64
)



#undef XRT_BUILTIN_TYPE

#endif
