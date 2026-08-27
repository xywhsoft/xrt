#include "../internal/xrt_runtime_value.h"



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_OBJECT) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_CALLABLE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_FUTURE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_WEAK) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TRACE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_ROOTS)

/* 设置运行时 Value 桥接模块结构化错误。 */
static void __xrtRuntimeValueError(
	xerrkind Kind,
	xruntimevalueerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.runtime-value";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 为下层 Value、对象、弱引用或 callable 错误补充桥接上下文。 */
static void __xrtRuntimeValueWrap(
	xerrkind DefaultKind,
	xruntimevalueerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : DefaultKind;
	Desc.Domain = "xrt.runtime-value";
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



/* 运行时身份句柄直接使用进程内稳定地址作为 Handle 哈希。 */
#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_OBJECT) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_CALLABLE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_FUTURE) || \
	defined(XRUNTIME_FEATURE_RUNTIME_VALUE_WEAK)

static uint64 __xrtRuntimeValueIdentityHash(ptr pHandle, ptr pUserData)
{
	(void)pUserData;
	return (uint64)(uintptr_t)pHandle;
}



/* 同一策略域内的运行时句柄按对象身份比较。 */
static bool __xrtRuntimeValueIdentityEqual(
	ptr pLeft,
	ptr pRight,
	ptr pUserData
)
{
	(void)pUserData;
	return pLeft == pRight;
}



/* 判断值是否属于指定的私有 Handle 策略。 */
static bool __xrtRuntimeValueIs(
	const xvalue* pValue,
	const xvaluehandleops* pExpected
)
{
	ptr pHandle;
	ptr pUserData;
	const xvaluehandleops* pOps;

	if ( xrtValueType(pValue) != XVALUE_HANDLE ) {
		return false;
	}
	return xrtValueGetHandle(
		pValue, &pHandle, &pOps, &pUserData) &&
		(pOps == pExpected) &&
		(pUserData == NULL);
}



/* 从指定私有 Handle 策略读取借用句柄并报告明确类型错误。 */
static bool __xrtRuntimeValueGet(
	const xvalue* pValue,
	const xvaluehandleops* pExpected,
	bool bNullable,
	xruntimevalueerror Code,
	cstr sOperation,
	ptr* pHandle
)
{
	ptr pValueHandle;
	ptr pUserData;
	const xvaluehandleops* pOps;

	if ( pHandle == NULL ) {
		__xrtRuntimeValueError(XERR_ARGUMENT, Code,
			sOperation, "the runtime Value output is null");
		return false;
	}
	if ( xrtValueType(pValue) != XVALUE_HANDLE ) {
		__xrtRuntimeValueError(XERR_TYPE, XRUNTIME_VALUE_ERROR_TYPE,
			sOperation, "the Value is not a runtime bridge handle");
		return false;
	}
	if ( !xrtValueGetHandle(
		pValue, &pValueHandle, &pOps, &pUserData) ) {
		__xrtRuntimeValueWrap(XERR_TYPE, XRUNTIME_VALUE_ERROR_TYPE,
			sOperation, "the runtime bridge handle cannot be read");
		return false;
	}
	if (
		(pOps != pExpected) ||
		(pUserData != NULL) ||
		(!bNullable && (pValueHandle == NULL))
	) {
		__xrtRuntimeValueError(XERR_TYPE, XRUNTIME_VALUE_ERROR_TYPE,
			sOperation, "the Value has another runtime handle type");
		return false;
	}
	*pHandle = pValueHandle;
	return true;
}

#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_OBJECT)

/* 克隆对象 Handle 时只增加对象引用，保留可变对象的稳定身份。 */
static bool __xrtRuntimeValueObjectClone(
	ptr pHandle,
	ptr* pClone,
	ptr pUserData
)
{
	xrtobject* pReference;

	(void)pUserData;
	if ( pClone == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pReference = xrtObjectRef((xrtobject*)pHandle);
	if ( pReference == NULL ) {
		return false;
	}
	*pClone = pReference;
	return true;
}



/* 释放 Value Handle 持有的运行时对象强引用。 */
static void __xrtRuntimeValueObjectDrop(ptr pHandle, ptr pUserData)
{
	(void)pUserData;
	xrtObjectUnref((xrtobject*)pHandle);
}



/* 对象具有可变身份，故意不提供伪深拷贝 Clone。 */
static const xvaluehandleops __xrtRuntimeValueObjectOps = {
	__xrtRuntimeValueObjectClone,
	__xrtRuntimeValueObjectDrop,
	__xrtRuntimeValueIdentityHash,
	__xrtRuntimeValueIdentityEqual
};



/* 增加对象引用并包装成 Value Handle。 */
XRT_API xvalue* xrtValueRuntimeObject(xrtobject* pObject)
{
	xrtobject* pReference;
	ptr pHandle;
	xvalue* pValue;

	if ( pObject == NULL ) {
		__xrtRuntimeValueError(XERR_ARGUMENT, XRUNTIME_VALUE_ERROR_OBJECT,
			"object", "the runtime object is null");
		return NULL;
	}
	pReference = xrtObjectRef(pObject);
	if ( pReference == NULL ) {
		__xrtRuntimeValueWrap(XERR_STATE, XRUNTIME_VALUE_ERROR_OBJECT,
			"object", "the runtime object cannot be retained");
		return NULL;
	}
	pHandle = pReference;
	pValue = xrtValueHandleTake(
		&pHandle, &__xrtRuntimeValueObjectOps, NULL);
	if ( pValue == NULL ) {
		xrtObjectUnref(pReference);
	}
	return pValue;
}



/* 把对象强引用移交给 Value Handle。 */
XRT_API xvalue* xrtValueRuntimeObjectTake(xrtobject** pObject)
{
	ptr pHandle;
	xvalue* pValue;

	if ( (pObject == NULL) || (*pObject == NULL) ) {
		__xrtRuntimeValueError(XERR_ARGUMENT, XRUNTIME_VALUE_ERROR_OWNERSHIP,
			"object-take", "the runtime object source is empty or invalid");
		return NULL;
	}
	pHandle = *pObject;
	pValue = xrtValueHandleTake(
		&pHandle, &__xrtRuntimeValueObjectOps, NULL);
	if ( pValue != NULL ) {
		*pObject = NULL;
	}
	return pValue;
}



/* 判断值是否由运行时对象桥接策略创建。 */
XRT_API bool xrtValueIsRuntimeObject(const xvalue* pValue)
{
	return __xrtRuntimeValueIs(pValue, &__xrtRuntimeValueObjectOps);
}



/* 返回 Value Handle 借用的运行时对象。 */
XRT_API xrtobject* xrtValueGetRuntimeObject(const xvalue* pValue)
{
	ptr pObject;

	if ( !__xrtRuntimeValueGet(
		pValue,
		&__xrtRuntimeValueObjectOps,
		false,
		XRUNTIME_VALUE_ERROR_OBJECT,
		"object-get",
		&pObject
	) ) {
		return NULL;
	}
	return (xrtobject*)pObject;
}

#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_CALLABLE)

/* 深克隆 callable Handle 时共享不可变 callable 身份。 */
static bool __xrtRuntimeValueCallableClone(
	ptr pHandle,
	ptr* pClone,
	ptr pUserData
)
{
	xrtcallable* pReference;

	(void)pUserData;
	if ( pClone == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pReference = xrtCallableRef((xrtcallable*)pHandle);
	if ( pReference == NULL ) {
		return false;
	}
	*pClone = pReference;
	return true;
}



/* 释放 Value Handle 持有的 callable 引用。 */
static void __xrtRuntimeValueCallableDrop(ptr pHandle, ptr pUserData)
{
	(void)pUserData;
	xrtCallableUnref((xrtcallable*)pHandle);
}



static const xvaluehandleops __xrtRuntimeValueCallableOps = {
	__xrtRuntimeValueCallableClone,
	__xrtRuntimeValueCallableDrop,
	__xrtRuntimeValueIdentityHash,
	__xrtRuntimeValueIdentityEqual
};



/* 增加 callable 引用并包装成 Value Handle。 */
XRT_API xvalue* xrtValueCallable(xrtcallable* pCallable)
{
	xrtcallable* pReference;
	ptr pHandle;
	xvalue* pValue;

	if ( pCallable == NULL ) {
		__xrtRuntimeValueError(XERR_ARGUMENT, XRUNTIME_VALUE_ERROR_CALLABLE,
			"callable", "the callable is null");
		return NULL;
	}
	pReference = xrtCallableRef(pCallable);
	if ( pReference == NULL ) {
		__xrtRuntimeValueWrap(XERR_STATE, XRUNTIME_VALUE_ERROR_CALLABLE,
			"callable", "the callable cannot be retained");
		return NULL;
	}
	pHandle = pReference;
	pValue = xrtValueHandleTake(
		&pHandle, &__xrtRuntimeValueCallableOps, NULL);
	if ( pValue == NULL ) {
		xrtCallableUnref(pReference);
	}
	return pValue;
}



/* 把 callable 引用移交给 Value Handle。 */
XRT_API xvalue* xrtValueCallableTake(xrtcallable** pCallable)
{
	ptr pHandle;
	xvalue* pValue;

	if ( (pCallable == NULL) || (*pCallable == NULL) ) {
		__xrtRuntimeValueError(XERR_ARGUMENT, XRUNTIME_VALUE_ERROR_OWNERSHIP,
			"callable-take", "the callable source is empty or invalid");
		return NULL;
	}
	pHandle = *pCallable;
	pValue = xrtValueHandleTake(
		&pHandle, &__xrtRuntimeValueCallableOps, NULL);
	if ( pValue != NULL ) {
		*pCallable = NULL;
	}
	return pValue;
}



/* 判断值是否由 callable 桥接策略创建。 */
XRT_API bool xrtValueIsCallable(const xvalue* pValue)
{
	return __xrtRuntimeValueIs(pValue, &__xrtRuntimeValueCallableOps);
}



/* 返回 Value Handle 借用的 callable。 */
XRT_API xrtcallable* xrtValueGetCallable(const xvalue* pValue)
{
	ptr pCallable;

	if ( !__xrtRuntimeValueGet(
		pValue,
		&__xrtRuntimeValueCallableOps,
		false,
		XRUNTIME_VALUE_ERROR_CALLABLE,
		"callable-get",
		&pCallable
	) ) {
		return NULL;
	}
	return (xrtcallable*)pCallable;
}



/* 返回 callable Value 借用的函数签名。 */
XRT_API const xrtfunctionsig* xrtValueCallableSignature(
	const xvalue* pValue
)
{
	xrtcallable* pCallable = xrtValueGetCallable(pValue);

	return pCallable != NULL ? xrtCallableSignature(pCallable) : NULL;
}



/* 调用 callable Value。 */
XRT_API bool xrtValueInvoke(
	const xvalue* pCallable,
	const xrtcallframe* pFrame,
	xrtcallresult* pResult
)
{
	xrtcallable* pTarget = xrtValueGetCallable(pCallable);

	return (pTarget != NULL) &&
		xrtCallableInvoke(pTarget, pFrame, pResult);
}



/* 初始化借用 callable 的同步进度桥。 */
XRT_API void xrtProgressCallInit(
	xrtprogresscall* pContext,
	xvalue* pCallback
)
{
	if ( pContext == NULL ) {
		return;
	}
	pContext->Callback =
		(pCallback != NULL) &&
		(xrtValueType(pCallback) != XVALUE_NULL) ?
			pCallback : NULL;
	pContext->InvokeFailed = false;
}



/* 构造三个整数参数并同步调用进度 callable。 */
XRT_API bool xrtProgressCallInvoke(
	const xrtprogress* pProgress,
	ptr pUserData
)
{
	xrtprogresscall* pContext = (xrtprogresscall*)pUserData;
	xrtcallframe Frame;
	xrtcallresult Result;
	xvalue* arrArguments[3] = { NULL, NULL, NULL };
	xvalue* pReturn;
	bool bContinue = false;

	if ( (pContext == NULL) || (pContext->Callback == NULL) ) {
		return true;
	}
	if ( pProgress == NULL ) {
		pContext->InvokeFailed = true;
		return false;
	}
	arrArguments[0] = xrtValueUInt(pProgress->iInputBytes);
	arrArguments[1] = xrtValueUInt(pProgress->iTotalInputBytes);
	arrArguments[2] = xrtValueUInt(pProgress->iOutputBytes);
	if ( (arrArguments[0] == NULL) ||
		 (arrArguments[1] == NULL) ||
		 (arrArguments[2] == NULL) ) {
		pContext->InvokeFailed = true;
		goto cleanup;
	}
	memset(&Frame, 0, sizeof(Frame));
	Frame.ArgumentCount = 3u;
	Frame.Arguments = arrArguments;
	xrtCallResultInit(&Result);
	if ( !xrtValueInvoke(pContext->Callback, &Frame, &Result) ) {
		pContext->InvokeFailed = true;
	} else if ( xrtCallResultCount(&Result) == 0u ) {
		pContext->InvokeFailed = true;
	} else {
		pReturn = xrtCallResultGet(&Result, 0u);
		if ( !xrtValueGetBool(pReturn, &bContinue) ) {
			pContext->InvokeFailed = true;
			bContinue = false;
		}
	}
	xrtCallResultUnit(&Result);

cleanup:
	xrtValueRelease(arrArguments[0]);
	xrtValueRelease(arrArguments[1]);
	xrtValueRelease(arrArguments[2]);
	return bContinue;
}

#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_FUTURE)

/* 深克隆 Future Handle 时增加消费端引用。 */
static bool __xrtRuntimeValueFutureClone(
	ptr pHandle,
	ptr* pClone,
	ptr pUserData
)
{
	xfuture* pReference;
	(void)pUserData;

	if ( pClone == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pReference = xrtFutureRef((xfuture*)pHandle);
	if ( pReference == NULL ) {
		return false;
	}
	*pClone = pReference;
	return true;
}



/* 释放 Value Handle 持有的 Future 引用。 */
static void __xrtRuntimeValueFutureDrop(ptr pHandle, ptr pUserData)
{
	(void)pUserData;
	xrtFutureDestroy((xfuture*)pHandle);
}



static const xvaluehandleops __xrtRuntimeValueFutureOps = {
	__xrtRuntimeValueFutureClone,
	__xrtRuntimeValueFutureDrop,
	__xrtRuntimeValueIdentityHash,
	__xrtRuntimeValueIdentityEqual
};



/* 增加 Future 引用并包装成 Value Handle。 */
XRT_API xvalue* xrtValueFuture(xfuture* pFuture)
{
	xfuture* pReference;
	ptr pHandle;
	xvalue* pValue;

	if ( pFuture == NULL ) {
		__xrtRuntimeValueError(XERR_ARGUMENT, XRUNTIME_VALUE_ERROR_FUTURE,
			"future", "the Future is null");
		return NULL;
	}
	pReference = xrtFutureRef(pFuture);
	if ( pReference == NULL ) {
		__xrtRuntimeValueWrap(XERR_STATE, XRUNTIME_VALUE_ERROR_FUTURE,
			"future", "the Future cannot be retained");
		return NULL;
	}
	pHandle = pReference;
	pValue = xrtValueHandleTake(
		&pHandle, &__xrtRuntimeValueFutureOps, NULL);
	if ( pValue == NULL ) {
		xrtFutureDestroy(pReference);
	}
	return pValue;
}



/* 把 Future 引用移交给 Value Handle。 */
XRT_API xvalue* xrtValueFutureTake(xfuture** pFuture)
{
	ptr pHandle;
	xvalue* pValue;

	if ( (pFuture == NULL) || (*pFuture == NULL) ) {
		__xrtRuntimeValueError(XERR_ARGUMENT, XRUNTIME_VALUE_ERROR_OWNERSHIP,
			"future-take", "the Future source is empty or invalid");
		return NULL;
	}
	pHandle = *pFuture;
	pValue = xrtValueHandleTake(
		&pHandle, &__xrtRuntimeValueFutureOps, NULL);
	if ( pValue != NULL ) {
		*pFuture = NULL;
	}
	return pValue;
}



/* 判断值是否由 Future 桥接策略创建。 */
XRT_API bool xrtValueIsFuture(const xvalue* pValue)
{
	return __xrtRuntimeValueIs(pValue, &__xrtRuntimeValueFutureOps);
}



/* 返回 Value Handle 借用的 Future。 */
XRT_API xfuture* xrtValueGetFuture(const xvalue* pValue)
{
	ptr pFuture;

	if ( !__xrtRuntimeValueGet(
		pValue,
		&__xrtRuntimeValueFutureOps,
		false,
		XRUNTIME_VALUE_ERROR_FUTURE,
		"future-get",
		&pFuture
	) ) {
		return NULL;
	}
	return (xfuture*)pFuture;
}

#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_WEAK)

/* 深克隆弱引用 Handle 时复制控制块引用，不增加对象强引用。 */
static bool __xrtRuntimeValueWeakClone(
	ptr pHandle,
	ptr* pClone,
	ptr pUserData
)
{
	xrtweak Source = { pHandle };
	xrtweak Target = { 0 };

	(void)pUserData;
	if ( pClone == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtWeakCopy(&Target, &Source) ) {
		return false;
	}
	*pClone = Target.Control;
	Target.Control = NULL;
	return true;
}



/* 释放 Value Handle 持有的弱控制块引用。 */
static void __xrtRuntimeValueWeakDrop(ptr pHandle, ptr pUserData)
{
	xrtweak Weak = { pHandle };

	(void)pUserData;
	xrtWeakUnit(&Weak);
}



static const xvaluehandleops __xrtRuntimeValueWeakOps = {
	__xrtRuntimeValueWeakClone,
	__xrtRuntimeValueWeakDrop,
	__xrtRuntimeValueIdentityHash,
	__xrtRuntimeValueIdentityEqual
};



/* 复制弱引用并包装成 Value Handle。 */
XRT_API xvalue* xrtValueWeak(const xrtweak* pWeak)
{
	xrtweak Copy = { 0 };
	ptr pHandle;
	xvalue* pValue;

	if ( pWeak == NULL ) {
		__xrtRuntimeValueError(XERR_ARGUMENT, XRUNTIME_VALUE_ERROR_WEAK,
			"weak", "the weak reference is null");
		return NULL;
	}
	if ( !xrtWeakCopy(&Copy, pWeak) ) {
		__xrtRuntimeValueWrap(XERR_STATE, XRUNTIME_VALUE_ERROR_WEAK,
			"weak", "the weak reference cannot be copied");
		return NULL;
	}
	pHandle = Copy.Control;
	pValue = xrtValueHandleTake(
		&pHandle, &__xrtRuntimeValueWeakOps, NULL);
	if ( pValue != NULL ) {
		Copy.Control = NULL;
	}
	xrtWeakUnit(&Copy);
	return pValue;
}



/* 把弱引用移交给 Value Handle。 */
XRT_API xvalue* xrtValueWeakTake(xrtweak* pWeak)
{
	ptr pHandle;
	xvalue* pValue;

	if ( pWeak == NULL ) {
		__xrtRuntimeValueError(XERR_ARGUMENT, XRUNTIME_VALUE_ERROR_OWNERSHIP,
			"weak-take", "the weak reference source is null");
		return NULL;
	}
	pHandle = pWeak->Control;
	pValue = xrtValueHandleTake(
		&pHandle, &__xrtRuntimeValueWeakOps, NULL);
	if ( pValue != NULL ) {
		pWeak->Control = NULL;
	}
	return pValue;
}



/* 判断值是否由弱引用桥接策略创建。 */
XRT_API bool xrtValueIsWeak(const xvalue* pValue)
{
	return __xrtRuntimeValueIs(pValue, &__xrtRuntimeValueWeakOps);
}



/* 把 Value 中的弱引用复制到目标。 */
XRT_API bool xrtValueGetWeak(
	const xvalue* pValue,
	xrtweak* pWeak
)
{
	ptr pControl;
	xrtweak Source;

	if ( pWeak == NULL ) {
		__xrtRuntimeValueError(XERR_ARGUMENT, XRUNTIME_VALUE_ERROR_WEAK,
			"weak-get", "the weak reference output is null");
		return false;
	}
	if ( !__xrtRuntimeValueGet(
		pValue,
		&__xrtRuntimeValueWeakOps,
		true,
		XRUNTIME_VALUE_ERROR_WEAK,
		"weak-get",
		&pControl
	) ) {
		return false;
	}
	Source.Control = pControl;
	if ( !xrtWeakCopy(pWeak, &Source) ) {
		__xrtRuntimeValueWrap(XERR_STATE, XRUNTIME_VALUE_ERROR_WEAK,
			"weak-get", "the weak reference cannot be copied");
		return false;
	}
	return true;
}



/* 查询 Value 中的弱引用是否已经过期。 */
XRT_API bool xrtValueWeakExpired(const xvalue* pValue)
{
	ptr pControl;
	xrtweak Weak;

	if ( !__xrtRuntimeValueGet(
		pValue,
		&__xrtRuntimeValueWeakOps,
		true,
		XRUNTIME_VALUE_ERROR_WEAK,
		"weak-expired",
		&pControl
	) ) {
		return true;
	}
	Weak.Control = pControl;
	return xrtWeakExpired(&Weak);
}



/* 从 Value 中的弱引用取得一个新的对象强引用。 */
XRT_API xrtobject* xrtValueWeakLock(const xvalue* pValue)
{
	ptr pControl;
	xrtweak Weak;

	if ( !__xrtRuntimeValueGet(
		pValue,
		&__xrtRuntimeValueWeakOps,
		true,
		XRUNTIME_VALUE_ERROR_WEAK,
		"weak-lock",
		&pControl
	) ) {
		return NULL;
	}
	Weak.Control = pControl;
	return xrtWeakLock(&Weak);
}

#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TRACE)

#define XRT_RUNTIME_VALUE_TRACE_INLINE 32u



/* 一次 Value 所有权图追踪使用栈内身份表，并只在大图时创建集合。 */
typedef struct xruntimevaluetrace {
	const void* Inline[XRT_RUNTIME_VALUE_TRACE_INLINE];
	size_t InlineCount;
	xset Overflow;
	bool OverflowReady;
	xrtobjectvisitor Visit;
	ptr Context;
} xruntimevaluetrace;



/* 记录一个 Value 外壳或容器 backing，重复身份不再次追踪。 */
static bool __xrtRuntimeValueTraceSeen(
	xruntimevaluetrace* pTrace,
	const void* pIdentity,
	bool* pSeen
)
{
	*pSeen = false;
	if ( pTrace->OverflowReady ) {
		if ( xrtSetHas(&pTrace->Overflow, &pIdentity) ) {
			*pSeen = true;
			return true;
		}
		return xrtSetAdd(&pTrace->Overflow, &pIdentity);
	}
	for ( size_t i = 0u; i < pTrace->InlineCount; i++ ) {
		if ( pTrace->Inline[i] == pIdentity ) {
			*pSeen = true;
			return true;
		}
	}
	if ( pTrace->InlineCount < XRT_RUNTIME_VALUE_TRACE_INLINE ) {
		pTrace->Inline[pTrace->InlineCount++] = pIdentity;
		return true;
	}
	if ( !xrtSetInit(&pTrace->Overflow, sizeof(pIdentity)) ) {
		return false;
	}
	if ( !xrtSetReserve(
			&pTrace->Overflow,
			XRT_RUNTIME_VALUE_TRACE_INLINE * 2u
		) ) {
		xrtSetUnit(&pTrace->Overflow);
		return false;
	}
	for ( size_t i = 0u; i < pTrace->InlineCount; i++ ) {
		const void* pInline = pTrace->Inline[i];

		if ( !xrtSetAdd(&pTrace->Overflow, &pInline) ) {
			xrtSetUnit(&pTrace->Overflow);
			return false;
		}
	}
	if ( !xrtSetAdd(&pTrace->Overflow, &pIdentity) ) {
		xrtSetUnit(&pTrace->Overflow);
		return false;
	}
	pTrace->OverflowReady = true;
	return true;
}



/* 递归枚举一个 Value 所有权图实际持有的对象强引用。 */
static bool __xrtRuntimeValueTraceGraph(
	const xvalue* pValue,
	xruntimevaluetrace* pTrace,
	uint32 iDepth
)
{
	xvalueiter Iterator;
	xvaluetype Type;
	const void* pIdentity;
	xvalue* pItem;
	bool bSeen;

	if ( iDepth >= XRT_VALUE_DEPTH_MAX ) {
		__xrtRuntimeValueError(XERR_RANGE, XRUNTIME_VALUE_ERROR_TRACE,
			"trace", "the Value graph exceeds the trace depth limit");
		return false;
	}
	Type = xrtValueType(pValue);
	if ( Type == XVALUE_INVALID ) {
		return false;
	}
	if ( xrtValueIsRuntimeObject(pValue) ) {
		if ( !__xrtRuntimeValueTraceSeen(
			pTrace, pValue, &bSeen
		) ) {
			return false;
		}
		if ( bSeen ) {
			return true;
		}
		return pTrace->Visit(
			xrtValueGetRuntimeObject(pValue), pTrace->Context);
	}
	if ( !__xrtValueContainerType(Type) ) {
		return true;
	}
	pIdentity = pValue->Data.Backing;
	if ( !__xrtRuntimeValueTraceSeen(
		pTrace, pIdentity, &bSeen
	) ) {
		return false;
	}
	if ( bSeen ) {
		return true;
	}
	if ( !xrtValueIterBegin(pValue, &Iterator) ) {
		return false;
	}
	while ( (pItem = xrtValueIterNext(&Iterator, NULL)) != NULL ) {
		if ( !__xrtRuntimeValueTraceGraph(
			pItem, pTrace, iDepth + 1u
		) ) {
			xrtValueIterEnd(&Iterator);
			return false;
		}
	}
	xrtValueIterEnd(&Iterator);
	return true;
}



/* 枚举 Value 图拥有的对象边，并在失败后释放大型图身份表。 */
XRT_API bool xrtValueTraceRuntimeObjects(
	const xvalue* pValue,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	xruntimevaluetrace Trace;
	xerror* pPrevious;
	xerror* pDiscard;
	bool bResult;

	if ( (pValue == NULL) || (pVisit == NULL) ) {
		__xrtRuntimeValueError(XERR_ARGUMENT, XRUNTIME_VALUE_ERROR_TRACE,
			"trace", "the Value or object visitor is null");
		return false;
	}
	memset(&Trace, 0, sizeof(Trace));
	Trace.Visit = pVisit;
	Trace.Context = pContext;

	/* 隔离调用前错误，避免静态错误对象复用地址时误判访问器状态。 */
	pPrevious = __xrtErrorSwapOwned(NULL);
	bResult = __xrtRuntimeValueTraceGraph(pValue, &Trace, 0u);
	if ( Trace.OverflowReady ) {
		xrtSetUnit(&Trace.Overflow);
	}
	if ( bResult ) {
		pDiscard = __xrtErrorSwapOwned(pPrevious);
		xrtErrorFree(pDiscard);
		return true;
	}

	/* 失败只保留本次遍历产生的错误，访问器未设置错误时补充统一错误。 */
	xrtErrorFree(pPrevious);
	if ( xrtGetError() == NULL ) {
		__xrtRuntimeValueError(XERR_STATE, XRUNTIME_VALUE_ERROR_TRACE,
			"trace", "the Value object graph visitor rejected an edge");
	}
	return false;
}

#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_ROOTS)

/* 一次对象图收集借用的外部 Value 根数组。 */
typedef struct xruntimevalueroots {
	const xvalue* const* Values;
	size_t Count;
} xruntimevalueroots;



/* 把全部外部 Value 所有权图中的对象引用报告给对象图收集器。 */
static bool __xrtRuntimeValueVisitRoots(
	xrtobjectvisitor pVisit,
	ptr pVisitContext,
	ptr pContext
)
{
	const xruntimevalueroots* pRoots =
		(const xruntimevalueroots*)pContext;

	for ( size_t i = 0u; i < pRoots->Count; i++ ) {
		if ( !xrtValueTraceRuntimeObjects(
			pRoots->Values[i], pVisit, pVisitContext
		) ) {
			__xrtRuntimeValueWrap(XERR_STATE, XRUNTIME_VALUE_ERROR_ROOTS,
				"collect-roots", "a Value root could not be traced");
			return false;
		}
	}
	return true;
}



/* 验证批量 Value 根视图，避免对象图快照完成后才报告参数错误。 */
static bool __xrtRuntimeValueRootsValid(
	const xvalue* const* pRoots,
	size_t iRootCount
)
{
	if ( (pRoots == NULL) && (iRootCount != 0u) ) {
		__xrtRuntimeValueError(XERR_ARGUMENT, XRUNTIME_VALUE_ERROR_ROOTS,
			"collect-roots", "the Value root array is null");
		return false;
	}
	for ( size_t i = 0u; i < iRootCount; i++ ) {
		if ( pRoots[i] == NULL ) {
			__xrtRuntimeValueError(XERR_ARGUMENT, XRUNTIME_VALUE_ERROR_ROOTS,
				"collect-roots", "the Value root array contains a null root");
			return false;
		}
	}
	return true;
}



/* 使用一个外部 Value 所有权图作为显式根执行对象图收集。 */
XRT_API bool xrtObjectGraphCollectValueRoot(
	xrtobjectgraph* pGraph,
	const xvalue* pRoot,
	xrtobjectgraphresult* pResult
)
{
	const xvalue* pRoots[1];

	if ( pRoot == NULL ) {
		__xrtRuntimeValueError(XERR_ARGUMENT, XRUNTIME_VALUE_ERROR_ROOTS,
			"collect-root", "the Value root is null");
		return false;
	}
	pRoots[0] = pRoot;
	return xrtObjectGraphCollectValueRoots(
		pGraph, pRoots, 1u, pResult);
}



/* 使用一组外部 Value 所有权图作为显式根执行对象图收集。 */
XRT_API bool xrtObjectGraphCollectValueRoots(
	xrtobjectgraph* pGraph,
	const xvalue* const* pRoots,
	size_t iRootCount,
	xrtobjectgraphresult* pResult
)
{
	xruntimevalueroots Roots;

	if ( !__xrtRuntimeValueRootsValid(pRoots, iRootCount) ) {
		return false;
	}
	if ( iRootCount == 0u ) {
		return xrtObjectGraphCollect(pGraph, pResult);
	}
	Roots.Values = pRoots;
	Roots.Count = iRootCount;
	return xrtObjectGraphCollectRoots(
		pGraph, __xrtRuntimeValueVisitRoots, &Roots, pResult);
}

#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE)

/* 初始化一个拥有 Value 引用的槽位为空值。 */
static bool __xrtTypeValueInit(ptr pValue, const xrttype* pType)
{
	xvalue* pNull = xrtValueNull();
	(void)pType;

	memcpy(pValue, &pNull, sizeof(pNull));
	return true;
}



/* 使用指定复制策略准备新引用，成功后再替换目标槽。 */
static bool __xrtTypeValueReplace(
	ptr pTarget,
	const void* pSource,
	xvalue* (*pDuplicate)(const xvalue*),
	cstr sOperation,
	cstr sFailure
)
{
	xvalue* pSourceValue;
	xvalue* pTargetValue;
	xvalue* pCopy;

	memcpy(&pSourceValue, pSource, sizeof(pSourceValue));
	if ( pSourceValue == NULL ) {
		__xrtRuntimeValueError(XERR_STATE, XRUNTIME_VALUE_ERROR_OWNERSHIP,
			sOperation, "the source Value slot is empty");
		return false;
	}
	pCopy = pDuplicate(pSourceValue);
	if ( pCopy == NULL ) {
		__xrtRuntimeValueWrap(XERR_STATE, XRUNTIME_VALUE_ERROR_OWNERSHIP,
			sOperation, sFailure);
		return false;
	}
	memcpy(&pTargetValue, pTarget, sizeof(pTargetValue));
	memcpy(pTarget, &pCopy, sizeof(pCopy));
	xrtValueRelease(pTargetValue);
	return true;
}



/* 复制 Value 所有权，容器只创建共享 backing 的独立 COW 外壳。 */
static bool __xrtTypeValueCopy(
	ptr pTarget,
	const void* pSource,
	const xrttype* pType
)
{
	(void)pType;
	return __xrtTypeValueReplace(
		pTarget,
		pSource,
		xrtValueClone,
		"type-copy",
		"the source Value could not be copied"
	);
}



/* 深克隆完整 Value 图，并保留图中的共享拓扑。 */
static bool __xrtTypeValueClone(
	ptr pTarget,
	const void* pSource,
	const xrttype* pType
)
{
	(void)pType;
	return __xrtTypeValueReplace(
		pTarget,
		pSource,
		xrtValueDeepClone,
		"type-clone",
		"the source Value graph could not be cloned"
	);
}



/* 移交 Value 图所有权，并把源槽恢复为有效空值。 */
static bool __xrtTypeValueMove(
	ptr pTarget,
	ptr pSource,
	const xrttype* pType
)
{
	xvalue* pSourceValue;
	xvalue* pTargetValue;
	xvalue* pNull = xrtValueNull();
	(void)pType;

	memcpy(&pSourceValue, pSource, sizeof(pSourceValue));
	if ( pSourceValue == NULL ) {
		pSourceValue = pNull;
	}
	memcpy(&pTargetValue, pTarget, sizeof(pTargetValue));
	memcpy(pTarget, &pSourceValue, sizeof(pSourceValue));
	memcpy(pSource, &pNull, sizeof(pNull));
	xrtValueRelease(pTargetValue);
	return true;
}



/* 释放槽位拥有的 Value 图引用。 */
static void __xrtTypeValueDrop(ptr pValue, const xrttype* pType)
{
	xvalue* pOwned;
	xvalue* pEmpty = NULL;
	(void)pType;

	memcpy(&pOwned, pValue, sizeof(pOwned));
	memcpy(pValue, &pEmpty, sizeof(pEmpty));
	xrtValueRelease(pOwned);
}



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TRACE)

/* 从一个 Value 槽追踪其完整所有权图中的对象强引用。 */
static bool __xrtTypeValueTrace(
	const void* pValue,
	const xrttype* pType,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	xvalue* pOwned;
	(void)pType;

	memcpy(&pOwned, pValue, sizeof(pOwned));
	if ( pOwned == NULL ) {
		return true;
	}
	return xrtValueTraceRuntimeObjects(pOwned, pVisit, pContext);
}

#endif



/* Value 槽支持 COW 复制、深克隆、移动和确定释放。 */
static const xrttypeops __xrtTypeValueOps = {
	.Init = __xrtTypeValueInit,
	.Copy = __xrtTypeValueCopy,
	.Move = __xrtTypeValueMove,
	.Drop = __xrtTypeValueDrop,
	.Clone = __xrtTypeValueClone,
#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TRACE)
	.Trace = __xrtTypeValueTrace
#endif
};



/* 进程期稳定描述同时供复合类型的静态泛型实参引用。 */
const xrttype __xrtTypeValueDescriptor = {
	.Id = UINT64_C(0xD382E6686762D482),
	.Kind = XRT_TYPE_HANDLE,
	.Flags = XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_REFERENCE |
		XRT_TYPE_FLAG_NULLABLE | XRT_TYPE_FLAG_FINAL |
		XRT_TYPE_FLAG_RELOCATABLE,
	.Name = XRT_STR_INIT("Value"),
	.AbiName = XRT_STR_INIT("xrt.Value"),
	.Size = sizeof(xvalue*),
	.Align = XRT_INTERNAL_ALIGNOF(xvalue*),
	.InstanceSize = 0u,
	.InstanceAlign = 1u,
	.Ops = &__xrtTypeValueOps
};



/* 返回 Value 所有权槽的稳定运行时类型。 */
XRT_API const xrttype* xrtTypeValue(void)
{
	return &__xrtTypeValueDescriptor;
}

#endif

#endif
