#include "../internal/xrt_internal.h"
#include "../internal/xrt_runtime_object.h"



#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT)

/* 设置运行时对象模块结构化错误。 */
static void __xrtRuntimeObjectError(
	xerrkind Kind,
	xobjecterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.object";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 为下层类型操作错误补充对象操作上下文。 */
static void __xrtRuntimeObjectWrap(
	xerrkind DefaultKind,
	xobjecterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : DefaultKind;
	Desc.Domain = "xrt.object";
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



/* 返回对象内已经按类型要求对齐的可写负载。 */
static ptr __xrtObjectPayload(xrtobject* pObject)
{
	return ((uint8*)pObject) + pObject->PayloadOffset;
}



/* 返回对象内已经按类型要求对齐的只读负载。 */
static const void* __xrtObjectConstPayload(const xrtobject* pObject)
{
	return ((const uint8*)pObject) + pObject->PayloadOffset;
}



/* 把对象强引用值初始化为空。 */
static bool __xrtObjectValueInit(ptr pValue, const xrttype* pType)
{
	xrtobject* pObject = NULL;
	(void)pType;

	memcpy(pValue, &pObject, sizeof(pObject));
	return true;
}



/* 先保留新对象，再失败原子地替换目标强引用值。 */
static bool __xrtObjectValueCopy(
	ptr pTarget,
	const void* pSource,
	const xrttype* pType
)
{
	xrtobject* pObject;
	xrtobject* pOldObject;
	(void)pType;

	memcpy(&pObject, pSource, sizeof(pObject));
	if ( (pObject != NULL) && (xrtObjectRef(pObject) == NULL) ) {
		return false;
	}
	memcpy(&pOldObject, pTarget, sizeof(pOldObject));
	memcpy(pTarget, &pObject, sizeof(pObject));
	xrtObjectUnref(pOldObject);
	return true;
}



/* 把源强引用移入目标，并释放目标原来拥有的对象。 */
static bool __xrtObjectValueMove(
	ptr pTarget,
	ptr pSource,
	const xrttype* pType
)
{
	xrtobject* pObject;
	xrtobject* pOldObject;
	xrtobject* pEmpty = NULL;
	(void)pType;

	memcpy(&pObject, pSource, sizeof(pObject));
	memcpy(&pOldObject, pTarget, sizeof(pOldObject));
	memcpy(pTarget, &pObject, sizeof(pObject));
	memcpy(pSource, &pEmpty, sizeof(pEmpty));
	xrtObjectUnref(pOldObject);
	return true;
}



/* 释放强引用值并先把槽位恢复为空。 */
static void __xrtObjectValueDrop(ptr pValue, const xrttype* pType)
{
	xrtobject* pObject;
	xrtobject* pEmpty = NULL;
	(void)pType;

	memcpy(&pObject, pValue, sizeof(pObject));
	memcpy(pValue, &pEmpty, sizeof(pEmpty));
	xrtObjectUnref(pObject);
}



/* 按进程内对象地址比较两个强引用值。 */
static int __xrtObjectValueCompare(
	const void* pLeft,
	const void* pRight,
	const xrttype* pType
)
{
	xrtobject* pLeftObject;
	xrtobject* pRightObject;
	uintptr_t iLeft;
	uintptr_t iRight;
	(void)pType;

	memcpy(&pLeftObject, pLeft, sizeof(pLeftObject));
	memcpy(&pRightObject, pRight, sizeof(pRightObject));
	iLeft = (uintptr_t)pLeftObject;
	iRight = (uintptr_t)pRightObject;
	return iLeft == iRight ? 0 : (iLeft < iRight ? -1 : 1);
}



/* 按进程内对象地址散列强引用值。 */
static uint64 __xrtObjectValueHash(
	const void* pValue,
	const xrttype* pType
)
{
	xrtobject* pObject;
	(void)pType;

	memcpy(&pObject, pValue, sizeof(pObject));
	return (uint64)(uintptr_t)pObject;
}



/* 枚举强引用槽位当前拥有的非空对象。 */
static bool __xrtObjectValueTrace(
	const void* pValue,
	const xrttype* pType,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	xrtobject* pObject;
	(void)pType;

	memcpy(&pObject, pValue, sizeof(pObject));
	return (pObject == NULL) || pVisit(pObject, pContext);
}



/* 对象强引用槽统一使用同一份不可变生命周期和追踪策略。 */
const xrttypeops __xrtObjectValueOperations = {
	.Init = __xrtObjectValueInit,
	.Copy = __xrtObjectValueCopy,
	.Move = __xrtObjectValueMove,
	.Drop = __xrtObjectValueDrop,
	.Clone = __xrtObjectValueCopy,
	.Compare = __xrtObjectValueCompare,
	.Hash = __xrtObjectValueHash,
	.Trace = __xrtObjectValueTrace
};



/* 返回对象强引用槽使用的统一值操作表。 */
XRT_API const xrttypeops* xrtObjectValueOps(void)
{
	return &__xrtObjectValueOperations;
}



#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH)

/* 原子取得对象图可见的对象状态。 */
static int32 __xrtObjectState(const xrtobject* pObject)
{
	return __xrtAtomicRefLoad(&pObject->State);
}



/* 从活动状态独占取得一次负载终结权。 */
bool __xrtObjectBeginFinalize(xrtobject* pObject)
{
	return __xrtAtomicRefCompareExchange(
		&pObject->State,
		XRT_OBJECT_STATE_FINALIZING,
		XRT_OBJECT_STATE_ACTIVE
	) == XRT_OBJECT_STATE_ACTIVE;
}



/* 在尚未销毁负载前撤销对象图批量终结。 */
void __xrtObjectCancelFinalize(xrtobject* pObject)
{
	if ( __xrtAtomicRefCompareExchange(
			&pObject->State,
			XRT_OBJECT_STATE_ACTIVE,
			XRT_OBJECT_STATE_FINALIZING
		) != XRT_OBJECT_STATE_FINALIZING ) {
		__xrtErrorSetInvalidState();
	}
}



/* 执行一次类型负载销毁；调用方必须已经独占终结权。 */
void __xrtObjectDropPayload(xrtobject* pObject)
{
	xrtTypeDropInstance(pObject->Type, __xrtObjectPayload(pObject));
}



/* 在负载销毁后把控制块发布为已终结状态。 */
void __xrtObjectEndFinalize(xrtobject* pObject)
{
	if ( __xrtAtomicRefCompareExchange(
			&pObject->State,
			XRT_OBJECT_STATE_FINALIZED,
			XRT_OBJECT_STATE_FINALIZING
		) != XRT_OBJECT_STATE_FINALIZING ) {
		__xrtErrorSetInvalidState();
	}
}

#endif



/* 释放一个控制块弱引用，最后一个弱引用负责回收整块内存。 */
static void __xrtObjectWeakRelease(xrtobject* pObject)
{
	int32 iReferences = xrtRefRelease(&pObject->WeakCount);

	if ( iReferences < 0 ) {
		__xrtErrorSetInvalidState();
		return;
	}
	if ( iReferences == 0 ) {
		xrtFree(pObject);
	}
}



/* 按类型声明的负载大小创建堆对象。 */
XRT_API xrtobject* xrtObjectCreate(const xrttype* pType)
{
	return xrtObjectCreateSized(
		pType,
		pType != NULL ? pType->InstanceSize : 0
	);
}



/* 创建带有可变尾随负载且满足类型对齐要求的对象。 */
XRT_API xrtobject* xrtObjectCreateSized(
	const xrttype* pType,
	size_t iSize
)
{
	xrtobject* pObject;
	size_t iPrefix = offsetof(xrtobject, Storage);
	size_t iPayload = iSize != 0 ? iSize : 1u;
	size_t iPadding;
	size_t iAllocation;
	uintptr_t iStorage;
	uintptr_t iMask;

	if ( !xrtTypeValidate(pType) ) {
		__xrtRuntimeObjectWrap(XERR_ARGUMENT, XOBJECT_ERROR_TYPE,
			"create", "the runtime object type is invalid");
		return NULL;
	}
	if ( (pType->Flags & XRT_TYPE_FLAG_REFERENCE) == 0 ) {
		__xrtRuntimeObjectError(XERR_TYPE, XOBJECT_ERROR_TYPE,
			"create", "the runtime object type is not a reference type");
		return NULL;
	}
	if ( iSize < pType->InstanceSize ) {
		__xrtRuntimeObjectError(XERR_RANGE, XOBJECT_ERROR_SIZE,
			"create", "the runtime object payload is smaller than its type");
		return NULL;
	}
	if ( (pType->InstanceAlign - 1u) > (SIZE_MAX - iPrefix) ) {
		__xrtRuntimeObjectError(XERR_RANGE, XOBJECT_ERROR_SIZE,
			"create", "the runtime object alignment overflows its allocation");
		return NULL;
	}
	iAllocation = iPrefix + (pType->InstanceAlign - 1u);
	if ( iPayload > (SIZE_MAX - iAllocation) ) {
		__xrtRuntimeObjectError(XERR_RANGE, XOBJECT_ERROR_SIZE,
			"create", "the runtime object payload overflows its allocation");
		return NULL;
	}
	iAllocation += iPayload;
	pObject = (xrtobject*)xrtCalloc(1u, iAllocation);
	if ( pObject == NULL ) {
		return NULL;
	}
	iStorage = (uintptr_t)(((uint8*)pObject) + iPrefix);
	iMask = (uintptr_t)(pType->InstanceAlign - 1u);
	iPadding = (iStorage & iMask) == 0 ?
		0u : pType->InstanceAlign - (size_t)(iStorage & iMask);
	pObject->StrongCount = 1;
	pObject->WeakCount = 1;
	pObject->Type = pType;
	pObject->Size = iSize;
	pObject->PayloadOffset = iPrefix + iPadding;
#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH)
	pObject->State = XRT_OBJECT_STATE_ACTIVE;
#endif
	if ( !xrtTypeInitInstance(pType, __xrtObjectPayload(pObject)) ) {
		xrtFree(pObject);
		__xrtRuntimeObjectWrap(XERR_STATE, XOBJECT_ERROR_INITIALIZE,
			"create", "the runtime object initializer failed");
		return NULL;
	}
	return pObject;
}



/* 增加一个已经存活对象的强引用。 */
XRT_API xrtobject* xrtObjectRef(xrtobject* pObject)
{
	if ( pObject == NULL ) {
		__xrtRuntimeObjectError(XERR_ARGUMENT, XOBJECT_ERROR_REFERENCE,
			"ref", "the runtime object is null");
		return NULL;
	}
#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH)
	if ( __xrtObjectState(pObject) != XRT_OBJECT_STATE_ACTIVE ) {
		__xrtRuntimeObjectError(XERR_STATE, XOBJECT_ERROR_REFERENCE,
			"ref", "the runtime object is being finalized");
		return NULL;
	}
#endif
	if ( xrtRefRetain(&pObject->StrongCount) < 0 ) {
		__xrtRuntimeObjectError(XERR_STATE, XOBJECT_ERROR_REFERENCE,
			"ref", "the runtime object reference cannot be retained");
		return NULL;
	}
#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH)
	if ( __xrtObjectState(pObject) != XRT_OBJECT_STATE_ACTIVE ) {
		(void)xrtRefRelease(&pObject->StrongCount);
		__xrtRuntimeObjectError(XERR_STATE, XOBJECT_ERROR_REFERENCE,
			"ref", "the runtime object began finalizing during retain");
		return NULL;
	}
#endif
	return pObject;
}



/* 释放一个对象强引用，并在最后一次释放时销毁负载。 */
XRT_API void xrtObjectUnref(xrtobject* pObject)
{
	int32 iReferences;

	if ( pObject == NULL ) {
		return;
	}
	iReferences = xrtRefRelease(&pObject->StrongCount);
	if ( iReferences < 0 ) {
		__xrtRuntimeObjectError(XERR_STATE, XOBJECT_ERROR_REFERENCE,
			"unref", "the runtime object reference cannot be released");
		return;
	}
	if ( iReferences != 0 ) {
		return;
	}
#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH)
	if ( __xrtObjectState(pObject) == XRT_OBJECT_STATE_FINALIZED ) {
		__xrtObjectWeakRelease(pObject);
		return;
	}
	if ( !__xrtObjectBeginFinalize(pObject) ) {
		return;
	}
	__xrtObjectGraphDetach(pObject);
	__xrtObjectDropPayload(pObject);
	__xrtObjectEndFinalize(pObject);
#else
	xrtTypeDropInstance(pObject->Type, __xrtObjectPayload(pObject));
#endif
	__xrtObjectWeakRelease(pObject);
}



/* 返回对象借用的运行时类型描述。 */
XRT_API const xrttype* xrtObjectType(const xrtobject* pObject)
{
	if ( pObject == NULL ) {
		__xrtRuntimeObjectError(XERR_ARGUMENT, XOBJECT_ERROR_REFERENCE,
			"type", "the runtime object is null");
		return NULL;
	}
	return pObject->Type;
}



/* 返回对象借用的可写负载。 */
XRT_API ptr xrtObjectData(xrtobject* pObject)
{
	if ( pObject == NULL ) {
		__xrtRuntimeObjectError(XERR_ARGUMENT, XOBJECT_ERROR_REFERENCE,
			"data", "the runtime object is null");
		return NULL;
	}
	return __xrtObjectPayload(pObject);
}



/* 返回对象借用的只读负载。 */
XRT_API const void* xrtObjectConstData(const xrtobject* pObject)
{
	if ( pObject == NULL ) {
		__xrtRuntimeObjectError(XERR_ARGUMENT, XOBJECT_ERROR_REFERENCE,
			"const-data", "the runtime object is null");
		return NULL;
	}
	return __xrtObjectConstPayload(pObject);
}



/* 返回对象创建时声明的真实负载长度。 */
XRT_API size_t xrtObjectSize(const xrtobject* pObject)
{
	if ( pObject == NULL ) {
		__xrtRuntimeObjectError(XERR_ARGUMENT, XOBJECT_ERROR_REFERENCE,
			"size", "the runtime object is null");
		return 0;
	}
	return pObject->Size;
}



/* 原子读取对象当前的瞬时强引用数量。 */
XRT_API size_t xrtObjectRefCount(const xrtobject* pObject)
{
	int32 iReferences;

	if ( pObject == NULL ) {
		__xrtRuntimeObjectError(XERR_ARGUMENT, XOBJECT_ERROR_REFERENCE,
			"ref-count", "the runtime object is null");
		return 0u;
	}
	iReferences = __xrtAtomicRefLoad(&pObject->StrongCount);
	return iReferences > 0 ? (size_t)iReferences : 0u;
}



/* 判断调用方持有的对象是否只有一个瞬时强引用。 */
XRT_API bool xrtObjectUnique(const xrtobject* pObject)
{
	if ( pObject == NULL ) {
		__xrtRuntimeObjectError(XERR_ARGUMENT, XOBJECT_ERROR_REFERENCE,
			"unique", "the runtime object is null");
		return false;
	}
	return __xrtAtomicRefLoad(&pObject->StrongCount) == 1;
}



/* 从可选的存活对象初始化一个空弱引用。 */
XRT_API bool xrtWeakInit(xrtweak* pWeak, xrtobject* pObject)
{
	if ( (pWeak == NULL) || (pWeak->Control != NULL) ) {
		__xrtRuntimeObjectError(XERR_ARGUMENT, XOBJECT_ERROR_WEAK,
			"weak-init", "the destination weak reference is invalid");
		return false;
	}
	if ( pObject == NULL ) {
		return true;
	}
	if ( xrtRefRetain(&pObject->WeakCount) < 0 ) {
		__xrtRuntimeObjectError(XERR_STATE, XOBJECT_ERROR_WEAK,
			"weak-init", "the object weak reference cannot be retained");
		return false;
	}
	pWeak->Control = pObject;
	return true;
}



/* 复制弱引用并替换目标，先保留新控制块以保证失败原子性。 */
XRT_API bool xrtWeakCopy(xrtweak* pTarget, const xrtweak* pSource)
{
	xrtobject* pObject;
	xrtobject* pOldObject;

	if ( (pTarget == NULL) || (pSource == NULL) ) {
		__xrtRuntimeObjectError(XERR_ARGUMENT, XOBJECT_ERROR_WEAK,
			"weak-copy", "the source or destination weak reference is null");
		return false;
	}
	if ( pTarget == pSource ) {
		return true;
	}
	pObject = (xrtobject*)pSource->Control;
	if ( (pObject != NULL) && (xrtRefRetain(&pObject->WeakCount) < 0) ) {
		__xrtRuntimeObjectError(XERR_STATE, XOBJECT_ERROR_WEAK,
			"weak-copy", "the object weak reference cannot be retained");
		return false;
	}
	pOldObject = (xrtobject*)pTarget->Control;
	pTarget->Control = pObject;
	if ( pOldObject != NULL ) {
		__xrtObjectWeakRelease(pOldObject);
	}
	return true;
}



/* 移动弱引用并替换目标，不增加源控制块引用。 */
XRT_API bool xrtWeakMove(xrtweak* pTarget, xrtweak* pSource)
{
	xrtobject* pOldObject;

	if ( (pTarget == NULL) || (pSource == NULL) ) {
		__xrtRuntimeObjectError(XERR_ARGUMENT, XOBJECT_ERROR_WEAK,
			"weak-move", "the source or destination weak reference is null");
		return false;
	}
	if ( pTarget == pSource ) {
		return true;
	}
	pOldObject = (xrtobject*)pTarget->Control;
	pTarget->Control = pSource->Control;
	pSource->Control = NULL;
	if ( pOldObject != NULL ) {
		__xrtObjectWeakRelease(pOldObject);
	}
	return true;
}



/* 用可选的存活对象替换弱引用，先保留新控制块再释放旧控制块。 */
XRT_API bool xrtWeakSet(xrtweak* pWeak, xrtobject* pObject)
{
	xrtobject* pOldObject;

	if ( pWeak == NULL ) {
		__xrtRuntimeObjectError(XERR_ARGUMENT, XOBJECT_ERROR_WEAK,
			"weak-set", "the destination weak reference is null");
		return false;
	}
	pOldObject = (xrtobject*)pWeak->Control;
	if ( pOldObject == pObject ) {
		return true;
	}
	if ( (pObject != NULL) && (xrtRefRetain(&pObject->WeakCount) < 0) ) {
		__xrtRuntimeObjectError(XERR_STATE, XOBJECT_ERROR_WEAK,
			"weak-set", "the object weak reference cannot be retained");
		return false;
	}
	pWeak->Control = pObject;
	if ( pOldObject != NULL ) {
		__xrtObjectWeakRelease(pOldObject);
	}
	return true;
}



/* 销毁弱引用值并使其恢复为空。 */
XRT_API void xrtWeakUnit(xrtweak* pWeak)
{
	xrtobject* pObject;

	if ( pWeak == NULL ) {
		return;
	}
	pObject = (xrtobject*)pWeak->Control;
	pWeak->Control = NULL;
	if ( pObject != NULL ) {
		__xrtObjectWeakRelease(pObject);
	}
}



/* 以原子方式读取强引用状态并返回瞬时过期结果。 */
XRT_API bool xrtWeakExpired(const xrtweak* pWeak)
{
	xrtobject* pObject;

	if ( pWeak == NULL ) {
		__xrtRuntimeObjectError(XERR_ARGUMENT, XOBJECT_ERROR_WEAK,
			"weak-expired", "the weak reference is null");
		return true;
	}
	pObject = (xrtobject*)pWeak->Control;
	return (pObject == NULL) ||
		(__xrtAtomicRefLoad(&pObject->StrongCount) <= 0)
#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH)
		|| (__xrtObjectState(pObject) != XRT_OBJECT_STATE_ACTIVE)
#endif
	;
}



/* 尝试把弱引用提升为新的强引用。 */
XRT_API xrtobject* xrtWeakLock(const xrtweak* pWeak)
{
	xrtobject* pObject;

	if ( pWeak == NULL ) {
		__xrtRuntimeObjectError(XERR_ARGUMENT, XOBJECT_ERROR_WEAK,
			"weak-lock", "the weak reference is null");
		return NULL;
	}
	pObject = (xrtobject*)pWeak->Control;
	if ( pObject == NULL ) {
		return NULL;
	}
#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH)
	if ( __xrtObjectState(pObject) != XRT_OBJECT_STATE_ACTIVE ) {
		return NULL;
	}
#endif
	if ( xrtRefRetain(&pObject->StrongCount) < 0 ) {
		return NULL;
	}
#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH)
	if ( __xrtObjectState(pObject) != XRT_OBJECT_STATE_ACTIVE ) {
		(void)xrtRefRelease(&pObject->StrongCount);
		return NULL;
	}
#endif
	return pObject;
}

#endif
