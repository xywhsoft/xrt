#include "../internal/xrt_internal.h"
#include <xrt/runtime_call.h>



#if defined(XRUNTIME_FEATURE_RUNTIME_CALL)

struct xrtcallable {
	volatile int32 RefCount;
	const xrtfunctionsig* Signature;
	xrtcallproc Entry;
	ptr Environment;
	xrtcalldrop DropEnvironment;
};



/* 设置动态调用模块结构化错误。 */
static void __xrtCallError(
	xerrkind Kind,
	xcallerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.call";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 为下层签名、值或入口错误补充动态调用上下文。 */
static void __xrtCallWrap(
	xerrkind DefaultKind,
	xcallerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : DefaultKind;
	Desc.Domain = "xrt.call";
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



/* 使用已经取出的入口错误建立调用错误并释放原因。 */
static void __xrtCallEntryError(xerror* pCause)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : XERR_STATE;
	Desc.Domain = "xrt.call";
	Desc.Code = XCALL_ERROR_ENTRY;
	Desc.Operation = "invoke";
	Desc.Message = "the callable entry failed";
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}



/* 恢复调用前由当前执行上下文持有的错误。 */
static void __xrtCallRestoreError(xerror* pError)
{
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 判断两个名称视图是否按完整字节相等。 */
static bool __xrtCallNameEqual(
	const xstrview* pLeft,
	const xstrview* pRight
)
{
	return (pLeft->Size == pRight->Size) &&
		((pLeft->Size == 0u) ||
		 (memcmp(pLeft->Data, pRight->Data, pLeft->Size) == 0));
}



/* 检查调用帧借用数组、值和关键字名称的基础结构。 */
static bool __xrtCallFrameShape(const xrtcallframe* pFrame)
{
	if ( pFrame == NULL ) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_FRAME,
			"frame-validate", "the call frame is null");
		return false;
	}
	if ( (pFrame->ArgumentCount != 0u) && (pFrame->Arguments == NULL) ) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_FRAME,
			"frame-validate", "the positional argument array is missing");
		return false;
	}
	if (
		(pFrame->KeywordCount != 0u) &&
		((pFrame->KeywordNames == NULL) || (pFrame->KeywordValues == NULL))
	) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_FRAME,
			"frame-validate", "the keyword argument arrays are missing");
		return false;
	}
	for ( size_t i = 0; i < pFrame->ArgumentCount; i++ ) {
		if ( pFrame->Arguments[i] == NULL ) {
			__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_FRAME,
				"frame-validate", "a positional argument is null");
			return false;
		}
	}
	for ( size_t i = 0; i < pFrame->KeywordCount; i++ ) {
		const xstrview* pName = &pFrame->KeywordNames[i];

		if (
			(pName->Data == NULL) ||
			(pName->Size == 0u) ||
			(pFrame->KeywordValues[i] == NULL)
		) {
			__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_FRAME,
				"frame-validate", "a keyword argument is invalid");
			return false;
		}
		for ( size_t j = 0; j < i; j++ ) {
			if ( __xrtCallNameEqual(pName, &pFrame->KeywordNames[j]) ) {
				__xrtCallError(XERR_EXISTS, XCALL_ERROR_FRAME,
					"frame-validate", "a keyword argument is duplicated");
				return false;
			}
		}
	}
	return true;
}



/* 返回关键字名称在调用帧中的位置，缺失时返回 SIZE_MAX。 */
static size_t __xrtCallFrameKeywordIndex(
	const xrtcallframe* pFrame,
	const xstrview* pName
)
{
	for ( size_t i = 0; i < pFrame->KeywordCount; i++ ) {
		if ( __xrtCallNameEqual(pName, &pFrame->KeywordNames[i]) ) {
			return i;
		}
	}
	return SIZE_MAX;
}



/* 检查调用帧结构和签名参数绑定。 */
XRT_API bool xrtCallFrameValidate(const xrtcallframe* pFrame)
{
	const xrtfunctionsig* pSignature;
	size_t iPositional = 0u;

	if ( !__xrtCallFrameShape(pFrame) ) {
		return false;
	}
	pSignature = pFrame->Signature;
	if ( pSignature == NULL ) {
		return true;
	}
	if ( !xrtFunctionSigValidate(pSignature) ) {
		__xrtCallWrap(XERR_ARGUMENT, XCALL_ERROR_SIGNATURE,
			"frame-validate", "the callable signature is invalid");
		return false;
	}
	for ( size_t i = 0; i < pSignature->ParamCount; i++ ) {
		const xrtparamdesc* pParam = &pSignature->Params[i];
		bool bPositional = false;
		bool bKeyword = false;

		if (
			((pParam->Flags & XRT_PARAM_FLAG_NAMED_ONLY) == 0u) &&
			(iPositional < pFrame->ArgumentCount)
		) {
			bPositional = true;
			iPositional++;
		}
		if ( pParam->Name.Size != 0u ) {
			bKeyword = __xrtCallFrameKeywordIndex(
				pFrame, &pParam->Name) != SIZE_MAX;
		}
		if ( bPositional && bKeyword ) {
			__xrtCallError(XERR_EXISTS, XCALL_ERROR_FRAME,
				"frame-validate", "a parameter was passed more than once");
			return false;
		}
		if (
			!bPositional &&
			!bKeyword &&
			((pParam->Flags & XRT_PARAM_FLAG_OPTIONAL) == 0u)
		) {
			__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_FRAME,
				"frame-validate", "a required parameter is missing");
			return false;
		}
	}
	if (
		(iPositional < pFrame->ArgumentCount) &&
		((pSignature->Flags & XRT_FUNCTION_FLAG_VARARGS) == 0u)
	) {
		__xrtCallError(XERR_RANGE, XCALL_ERROR_FRAME,
			"frame-validate", "too many positional arguments were passed");
		return false;
	}
	for ( size_t i = 0; i < pFrame->KeywordCount; i++ ) {
		bool bKnown = false;

		for ( size_t j = 0; j < pSignature->ParamCount; j++ ) {
			const xrtparamdesc* pParam = &pSignature->Params[j];

			if (
				(pParam->Name.Size != 0u) &&
				__xrtCallNameEqual(
					&pFrame->KeywordNames[i], &pParam->Name)
			) {
				bKnown = true;
				break;
			}
		}
		if (
			!bKnown &&
			((pSignature->Flags & XRT_FUNCTION_FLAG_KWARGS) == 0u)
		) {
			__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_FRAME,
				"frame-validate", "an unknown keyword argument was passed");
			return false;
		}
	}
	return true;
}



/* 返回调用帧中的原始位置参数。 */
XRT_API xvalue* xrtCallFrameArgument(
	const xrtcallframe* pFrame,
	size_t iIndex
)
{
	if (
		(pFrame == NULL) ||
		((pFrame->ArgumentCount != 0u) && (pFrame->Arguments == NULL))
	) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_FRAME,
			"frame-argument", "the positional argument array is invalid");
		return NULL;
	}
	if ( iIndex >= pFrame->ArgumentCount ) {
		__xrtCallError(XERR_RANGE, XCALL_ERROR_FRAME,
			"frame-argument", "the positional argument index is out of range");
		return NULL;
	}
	if ( pFrame->Arguments[iIndex] == NULL ) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_FRAME,
			"frame-argument", "the positional argument is null");
		return NULL;
	}
	return pFrame->Arguments[iIndex];
}



/* 返回调用帧中按完整名称匹配的关键字参数。 */
XRT_API xvalue* xrtCallFrameKeyword(
	const xrtcallframe* pFrame,
	xstrview Name
)
{
	if (
		(pFrame == NULL) ||
		((pFrame->KeywordCount != 0u) &&
		 ((pFrame->KeywordNames == NULL) || (pFrame->KeywordValues == NULL)))
	) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_FRAME,
			"frame-keyword", "the keyword argument arrays are invalid");
		return NULL;
	}
	if ( (Name.Data == NULL) || (Name.Size == 0u) ) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_FRAME,
			"frame-keyword", "the keyword name is empty or invalid");
		return NULL;
	}
	for ( size_t i = 0; i < pFrame->KeywordCount; i++ ) {
		if (
			(pFrame->KeywordNames[i].Data == NULL) ||
			(pFrame->KeywordNames[i].Size == 0u) ||
			(pFrame->KeywordValues[i] == NULL)
		) {
			__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_FRAME,
				"frame-keyword", "a keyword argument is invalid");
			return NULL;
		}
		if ( __xrtCallNameEqual(&Name, &pFrame->KeywordNames[i]) ) {
			return pFrame->KeywordValues[i];
		}
	}
	return NULL;
}



/* 按有效签名读取一个形参，不要求入口区分位置和关键字传递。 */
XRT_API xvalue* xrtCallFrameParameter(
	const xrtcallframe* pFrame,
	size_t iIndex
)
{
	const xrtfunctionsig* pSignature;
	const xrtparamdesc* pParam;
	xvalue* pPositional = NULL;
	xvalue* pKeyword = NULL;
	size_t iPosition = 0u;

	if ( pFrame == NULL ) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_FRAME,
			"frame-parameter", "the call frame is null");
		return NULL;
	}
	pSignature = pFrame->Signature;
	if (
		(pSignature == NULL) ||
		((pSignature->ParamCount != 0u) && (pSignature->Params == NULL))
	) {
		__xrtCallError(XERR_STATE, XCALL_ERROR_SIGNATURE,
			"frame-parameter", "the effective call signature is invalid");
		return NULL;
	}
	if ( iIndex >= pSignature->ParamCount ) {
		__xrtCallError(XERR_RANGE, XCALL_ERROR_FRAME,
			"frame-parameter", "the effective parameter index is out of range");
		return NULL;
	}
	pParam = &pSignature->Params[iIndex];
	for ( size_t i = 0; i <= iIndex; i++ ) {
		if (
			(pSignature->Params[i].Flags &
			 XRT_PARAM_FLAG_NAMED_ONLY) != 0u
		) {
			continue;
		}
		if ( (i == iIndex) && (iPosition < pFrame->ArgumentCount) ) {
			pPositional = xrtCallFrameArgument(pFrame, iPosition);
			if ( pPositional == NULL ) {
				return NULL;
			}
		}
		iPosition++;
	}
	if ( pParam->Name.Size != 0u ) {
		pKeyword = xrtCallFrameKeyword(pFrame, pParam->Name);
	}
	if ( (pPositional != NULL) && (pKeyword != NULL) ) {
		__xrtCallError(XERR_EXISTS, XCALL_ERROR_FRAME,
			"frame-parameter", "the parameter was passed more than once");
		return NULL;
	}
	if ( pPositional != NULL ) {
		return pPositional;
	}
	if ( pKeyword != NULL ) {
		return pKeyword;
	}
	if ( (pParam->Flags & XRT_PARAM_FLAG_OPTIONAL) != 0u ) {
		return NULL;
	}
	__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_FRAME,
		"frame-parameter", "the required parameter is missing");
	return NULL;
}



/* 返回结果下标对应的内部值槽。 */
static xvalue** __xrtCallResultSlot(xrtcallresult* pResult, size_t iIndex)
{
	return iIndex < XRT_CALL_RESULT_INLINE_COUNT
		? &pResult->Inline[iIndex]
		: &pResult->Overflow[iIndex - XRT_CALL_RESULT_INLINE_COUNT];
}



/* 返回结果下标对应的只读内部值槽。 */
static xvalue* const* __xrtCallResultConstSlot(
	const xrtcallresult* pResult,
	size_t iIndex
)
{
	return iIndex < XRT_CALL_RESULT_INLINE_COUNT
		? &pResult->Inline[iIndex]
		: &pResult->Overflow[iIndex - XRT_CALL_RESULT_INLINE_COUNT];
}



/* 检查结果计数和溢出存储是否自洽，不扫描已经持有的值。 */
static bool __xrtCallResultStorage(const xrtcallresult* pResult)
{
	size_t iCapacity;

	if ( pResult == NULL ) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_RESULT,
			"result-validate", "the call result is null");
		return false;
	}
	if (
		((pResult->OverflowCapacity == 0u) != (pResult->Overflow == NULL)) ||
		(pResult->OverflowCapacity > (SIZE_MAX - XRT_CALL_RESULT_INLINE_COUNT)) ||
		(pResult->OverflowCapacity > (SIZE_MAX / sizeof(xvalue*)))
	) {
		__xrtCallError(XERR_STATE, XCALL_ERROR_RESULT,
			"result-validate", "the call result storage is invalid");
		return false;
	}
	iCapacity = XRT_CALL_RESULT_INLINE_COUNT + pResult->OverflowCapacity;
	if ( pResult->Count > iCapacity ) {
		__xrtCallError(XERR_STATE, XCALL_ERROR_RESULT,
			"result-validate", "the call result count exceeds its storage");
		return false;
	}
	return true;
}



/* 在调用提交边界检查结果全部已持有值是否自洽。 */
static bool __xrtCallResultShape(const xrtcallresult* pResult)
{
	if ( !__xrtCallResultStorage(pResult) ) {
		return false;
	}
	for ( size_t i = 0; i < pResult->Count; i++ ) {
		if ( *__xrtCallResultConstSlot(pResult, i) == NULL ) {
			__xrtCallError(XERR_STATE, XCALL_ERROR_RESULT,
				"result-validate", "the call result contains an empty value slot");
			return false;
		}
	}
	return true;
}



/* 保证结果能够容纳指定总项数，增长失败时保持原状态。 */
static bool __xrtCallResultReserve(
	xrtcallresult* pResult,
	size_t iCount
)
{
	size_t iRequired;
	size_t iCapacity;
	xvalue** pOverflow;

	if ( iCount <= XRT_CALL_RESULT_INLINE_COUNT ) {
		return true;
	}
	iRequired = iCount - XRT_CALL_RESULT_INLINE_COUNT;
	if ( iRequired <= pResult->OverflowCapacity ) {
		return true;
	}
	iCapacity = pResult->OverflowCapacity != 0u
		? pResult->OverflowCapacity
		: XRT_CALL_RESULT_INLINE_COUNT;
	while ( iCapacity < iRequired ) {
		if ( iCapacity > (SIZE_MAX / 2u) ) {
			iCapacity = iRequired;
			break;
		}
		iCapacity *= 2u;
	}
	if ( iCapacity > (SIZE_MAX / sizeof(xvalue*)) ) {
		__xrtCallError(XERR_RANGE, XCALL_ERROR_RESULT,
			"result-reserve", "the call result capacity overflows memory size");
		return false;
	}
	pOverflow = (xvalue**)xrtRealloc(
		pResult->Overflow,
		iCapacity * sizeof(xvalue*)
	);
	if ( pOverflow == NULL ) {
		return false;
	}
	memset(
		pOverflow + pResult->OverflowCapacity,
		0,
		(iCapacity - pResult->OverflowCapacity) * sizeof(xvalue*)
	);
	pResult->Overflow = pOverflow;
	pResult->OverflowCapacity = iCapacity;
	return true;
}



/* 检查 Take 来源槽不会被结果结构或溢出存储覆盖。 */
static bool __xrtCallResultTakeSlotValid(
	const xrtcallresult* pResult,
	xvalue** pValue
)
{
	if ( pValue == NULL ) {
		return false;
	}
	if ( __xrtRangesOverlap(
		pValue, sizeof(*pValue), pResult, sizeof(*pResult))
	) {
		return false;
	}
	if (
		(pResult->Overflow != NULL) &&
		__xrtRangesOverlap(
			pValue,
			sizeof(*pValue),
			pResult->Overflow,
			pResult->OverflowCapacity * sizeof(xvalue*)
		)
	) {
		return false;
	}
	return true;
}



/* 初始化一个新的空调用结果。 */
XRT_API void xrtCallResultInit(xrtcallresult* pResult)
{
	if ( pResult != NULL ) {
		memset(pResult, 0, sizeof(*pResult));
	}
}



/* 释放结果持有的值并保留容量。 */
XRT_API void xrtCallResultClear(xrtcallresult* pResult)
{
	if ( pResult == NULL ) {
		return;
	}
	for ( size_t i = 0; i < pResult->Count; i++ ) {
		xvalue** pSlot = __xrtCallResultSlot(pResult, i);

		xrtValueRelease(*pSlot);
		*pSlot = NULL;
	}
	pResult->Count = 0u;
}



/* 释放结果持有的值和溢出存储。 */
XRT_API void xrtCallResultUnit(xrtcallresult* pResult)
{
	if ( pResult == NULL ) {
		return;
	}
	xrtCallResultClear(pResult);
	xrtFree(pResult->Overflow);
	memset(pResult, 0, sizeof(*pResult));
}



/* 返回调用结果数量。 */
XRT_API size_t xrtCallResultCount(const xrtcallresult* pResult)
{
	if ( !__xrtCallResultStorage(pResult) ) {
		return 0u;
	}
	return pResult->Count;
}



/* 返回指定下标借用的调用结果。 */
XRT_API xvalue* xrtCallResultGet(
	const xrtcallresult* pResult,
	size_t iIndex
)
{
	if ( !__xrtCallResultStorage(pResult) ) {
		return NULL;
	}
	if ( iIndex >= pResult->Count ) {
		__xrtCallError(XERR_RANGE, XCALL_ERROR_RESULT,
			"result-get", "the call result index is out of range");
		return NULL;
	}
	if ( *__xrtCallResultConstSlot(pResult, iIndex) == NULL ) {
		__xrtCallError(XERR_STATE, XCALL_ERROR_RESULT,
			"result-get", "the call result value slot is empty");
		return NULL;
	}
	return *__xrtCallResultConstSlot(pResult, iIndex);
}



/* 增加引用后替换或追加一个连续结果。 */
XRT_API bool xrtCallResultSet(
	xrtcallresult* pResult,
	size_t iIndex,
	const xvalue* pValue
)
{
	xvalue* pRetained;
	xvalue** pSlot;
	xvalue* pPrevious;

	if ( !__xrtCallResultStorage(pResult) ) {
		return false;
	}
	if (
		(pValue == NULL) ||
		(iIndex > pResult->Count) ||
		(iIndex == SIZE_MAX)
	) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_RESULT,
			"result-set", "the result value or index is invalid");
		return false;
	}
	pRetained = xrtValueRetain(pValue);
	if ( pRetained == NULL ) {
		__xrtCallWrap(XERR_STATE, XCALL_ERROR_RESULT,
			"result-set", "the result value cannot be retained");
		return false;
	}
	if ( !__xrtCallResultReserve(pResult, iIndex + 1u) ) {
		xrtValueRelease(pRetained);
		return false;
	}
	pSlot = __xrtCallResultSlot(pResult, iIndex);
	pPrevious = *pSlot;
	*pSlot = pRetained;
	if ( iIndex == pResult->Count ) {
		pResult->Count++;
	}
	xrtValueRelease(pPrevious);
	return true;
}



/* 移交引用后替换或追加一个连续结果。 */
XRT_API bool xrtCallResultSetTake(
	xrtcallresult* pResult,
	size_t iIndex,
	xvalue** pValue
)
{
	xvalue** pSlot;
	xvalue* pPrevious;

	if ( !__xrtCallResultStorage(pResult) ) {
		return false;
	}
	if (
		!__xrtCallResultTakeSlotValid(pResult, pValue) ||
		(*pValue == NULL) ||
		(iIndex > pResult->Count) ||
		(iIndex == SIZE_MAX)
	) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_RESULT,
			"result-set-take", "the result source or index is invalid");
		return false;
	}
	if ( !__xrtCallResultReserve(pResult, iIndex + 1u) ) {
		return false;
	}
	pSlot = __xrtCallResultSlot(pResult, iIndex);
	pPrevious = *pSlot;
	*pSlot = *pValue;
	*pValue = NULL;
	if ( iIndex == pResult->Count ) {
		pResult->Count++;
	}
	xrtValueRelease(pPrevious);
	return true;
}



/* 增加引用后追加一个结果。 */
XRT_API bool xrtCallResultPush(
	xrtcallresult* pResult,
	const xvalue* pValue
)
{
	if ( pResult == NULL ) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_RESULT,
			"result-push", "the call result is null");
		return false;
	}
	return xrtCallResultSet(pResult, pResult->Count, pValue);
}



/* 移交引用后追加一个结果。 */
XRT_API bool xrtCallResultPushTake(
	xrtcallresult* pResult,
	xvalue** pValue
)
{
	if ( pResult == NULL ) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_RESULT,
			"result-push-take", "the call result is null");
		return false;
	}
	return xrtCallResultSetTake(pResult, pResult->Count, pValue);
}



/* 把完整调用结果移动到已经初始化的目标。 */
XRT_API bool xrtCallResultMove(
	xrtcallresult* pTarget,
	xrtcallresult* pSource
)
{
	if ( pTarget == pSource ) {
		if ( pTarget == NULL ) {
			__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_RESULT,
				"result-move", "the call result is null");
			return false;
		}
		return __xrtCallResultShape(pTarget);
	}
	if (
		(pTarget == NULL) ||
		(pSource == NULL)
	) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_RESULT,
			"result-move", "the source or target call result is null");
		return false;
	}
	if ( __xrtRangesOverlap(
		pTarget, sizeof(*pTarget), pSource, sizeof(*pSource))
	) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_RESULT,
			"result-move", "the source and target call results overlap");
		return false;
	}
	if (
		!__xrtCallResultShape(pTarget) ||
		!__xrtCallResultShape(pSource)
	) {
		return false;
	}
	xrtCallResultUnit(pTarget);
	*pTarget = *pSource;
	memset(pSource, 0, sizeof(*pSource));
	return true;
}



/* 创建持有入口环境的不可变 callable。 */
XRT_API xrtcallable* xrtCallableCreate(
	const xrtfunctionsig* pSignature,
	xrtcallproc pEntry,
	ptr pEnvironment,
	xrtcalldrop pDropEnvironment
)
{
	xrtcallable* pCallable;

	if ( pEntry == NULL ) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_CALLABLE,
			"create", "the callable entry is null");
		return NULL;
	}
	if (
		(pSignature != NULL) &&
		!xrtFunctionSigValidate(pSignature)
	) {
		__xrtCallWrap(XERR_ARGUMENT, XCALL_ERROR_SIGNATURE,
			"create", "the callable signature is invalid");
		return NULL;
	}
	pCallable = (xrtcallable*)xrtMalloc(sizeof(xrtcallable));
	if ( pCallable == NULL ) {
		return NULL;
	}
	pCallable->RefCount = 1;
	pCallable->Signature = pSignature;
	pCallable->Entry = pEntry;
	pCallable->Environment = pEnvironment;
	pCallable->DropEnvironment = pDropEnvironment;
	return pCallable;
}



/* 增加一个已经存活 callable 的引用。 */
XRT_API xrtcallable* xrtCallableRef(xrtcallable* pCallable)
{
	if ( pCallable == NULL ) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_REFERENCE,
			"ref", "the callable is null");
		return NULL;
	}
	if ( xrtRefRetain(&pCallable->RefCount) < 0 ) {
		__xrtCallError(XERR_STATE, XCALL_ERROR_REFERENCE,
			"ref", "the callable reference cannot be retained");
		return NULL;
	}
	return pCallable;
}



/* 释放 callable 引用，最后一个引用负责释放环境。 */
XRT_API void xrtCallableUnref(xrtcallable* pCallable)
{
	int32 iReferences;

	if ( pCallable == NULL ) {
		return;
	}
	iReferences = xrtRefRelease(&pCallable->RefCount);
	if ( iReferences < 0 ) {
		__xrtCallError(XERR_STATE, XCALL_ERROR_REFERENCE,
			"unref", "the callable reference cannot be released");
		return;
	}
	if ( iReferences != 0 ) {
		return;
	}
	if ( pCallable->DropEnvironment != NULL ) {
		pCallable->DropEnvironment(pCallable->Environment);
	}
	xrtFree(pCallable);
}



/* 初始化一个拥有 callable 强引用的槽为空。 */
static bool __xrtTypeCallableInit(
	ptr pValue,
	const xrttype* pType
)
{
	xrtcallable* pEmpty = NULL;
	(void)pType;

	memcpy(pValue, &pEmpty, sizeof(pEmpty));
	return true;
}



/* 增加来源 callable 引用，成功后再替换目标槽。 */
static bool __xrtTypeCallableCopy(
	ptr pTarget,
	const void* pSource,
	const xrttype* pType
)
{
	xrtcallable* pSourceCallable;
	xrtcallable* pTargetCallable;
	xrtcallable* pReference = NULL;
	(void)pType;

	memcpy(&pSourceCallable, pSource, sizeof(pSourceCallable));
	if ( pSourceCallable != NULL ) {
		pReference = xrtCallableRef(pSourceCallable);
		if ( pReference == NULL ) {
			return false;
		}
	}
	memcpy(&pTargetCallable, pTarget, sizeof(pTargetCallable));
	memcpy(pTarget, &pReference, sizeof(pReference));
	xrtCallableUnref(pTargetCallable);
	return true;
}



/* 转移 callable 引用，清空来源并释放目标原引用。 */
static bool __xrtTypeCallableMove(
	ptr pTarget,
	ptr pSource,
	const xrttype* pType
)
{
	xrtcallable* pSourceCallable;
	xrtcallable* pTargetCallable;
	xrtcallable* pEmpty = NULL;
	(void)pType;

	memcpy(&pSourceCallable, pSource, sizeof(pSourceCallable));
	memcpy(&pTargetCallable, pTarget, sizeof(pTargetCallable));
	memcpy(pTarget, &pSourceCallable, sizeof(pSourceCallable));
	memcpy(pSource, &pEmpty, sizeof(pEmpty));
	xrtCallableUnref(pTargetCallable);
	return true;
}



/* 释放槽拥有的 callable 引用并恢复为空。 */
static void __xrtTypeCallableDrop(
	ptr pValue,
	const xrttype* pType
)
{
	xrtcallable* pCallable;
	xrtcallable* pEmpty = NULL;
	(void)pType;

	memcpy(&pCallable, pValue, sizeof(pCallable));
	memcpy(pValue, &pEmpty, sizeof(pEmpty));
	xrtCallableUnref(pCallable);
}



/* callable 槽按进程内对象身份比较。 */
static int __xrtTypeCallableCompare(
	const void* pLeft,
	const void* pRight,
	const xrttype* pType
)
{
	xrtcallable* pLeftCallable;
	xrtcallable* pRightCallable;
	uintptr_t iLeft;
	uintptr_t iRight;
	(void)pType;

	memcpy(&pLeftCallable, pLeft, sizeof(pLeftCallable));
	memcpy(&pRightCallable, pRight, sizeof(pRightCallable));
	iLeft = (uintptr_t)pLeftCallable;
	iRight = (uintptr_t)pRightCallable;
	return (iLeft > iRight) - (iLeft < iRight);
}



/* callable 槽按进程内对象身份散列。 */
static uint64 __xrtTypeCallableHash(
	const void* pValue,
	const xrttype* pType
)
{
	xrtcallable* pCallable;
	(void)pType;

	memcpy(&pCallable, pValue, sizeof(pCallable));
	return (uint64)(uintptr_t)pCallable;
}



static const xrttypeops __xrtTypeCallableOps = {
	.Init = __xrtTypeCallableInit,
	.Copy = __xrtTypeCallableCopy,
	.Move = __xrtTypeCallableMove,
	.Drop = __xrtTypeCallableDrop,
	.Clone = __xrtTypeCallableCopy,
	.Compare = __xrtTypeCallableCompare,
	.Hash = __xrtTypeCallableHash
};



static const xrttype __xrtTypeCallableDescriptor = {
	.Id = UINT64_C(0x2F78E864B00430C1),
	.Kind = XRT_TYPE_CALLABLE,
	.Flags = XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_REFERENCE |
		XRT_TYPE_FLAG_NULLABLE | XRT_TYPE_FLAG_FINAL |
		XRT_TYPE_FLAG_RELOCATABLE,
	.Name = XRT_STR_INIT("callable"),
	.AbiName = XRT_STR_INIT("xrt.callable"),
	.Size = sizeof(xrtcallable*),
	.Align = XRT_INTERNAL_ALIGNOF(xrtcallable*),
	.InstanceSize = 0u,
	.InstanceAlign = 1u,
	.Ops = &__xrtTypeCallableOps
};



/* 返回拥有型 callable 引用槽的稳定运行时类型。 */
XRT_API const xrttype* xrtTypeCallable(void)
{
	return &__xrtTypeCallableDescriptor;
}



/* 返回 callable 借用的签名。 */
XRT_API const xrtfunctionsig* xrtCallableSignature(
	const xrtcallable* pCallable
)
{
	if ( pCallable == NULL ) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_CALLABLE,
			"signature", "the callable is null");
		return NULL;
	}
	return pCallable->Signature;
}



/* 返回 callable 的稳定签名 ID。 */
XRT_API uint64 xrtCallableSignatureId(const xrtcallable* pCallable)
{
	const xrtfunctionsig* pSignature = xrtCallableSignature(pCallable);

	return pSignature != NULL ? xrtFunctionSigId(pSignature) : 0u;
}



/* 验证调用并以失败原子方式提交入口结果。 */
XRT_API bool xrtCallableInvoke(
	const xrtcallable* pCallable,
	const xrtcallframe* pFrame,
	xrtcallresult* pResult
)
{
	xrtcallframe EffectiveFrame;
	xrtcallresult Temporary = XRT_CALL_RESULT_INIT;
	const xrtfunctionsig* pEffectiveSignature;
	xerror* pPreviousError;
	xerror* pEntryError;
	bool bSuccess;

	if ( pCallable == NULL ) {
		__xrtCallError(XERR_ARGUMENT, XCALL_ERROR_CALLABLE,
			"invoke", "the callable is null");
		return false;
	}
	if ( !__xrtCallResultShape(pResult) ) {
		return false;
	}
	if ( pFrame != NULL ) {
		EffectiveFrame = *pFrame;
	} else {
		memset(&EffectiveFrame, 0, sizeof(EffectiveFrame));
	}
	if (
		(pCallable->Signature != NULL) &&
		(EffectiveFrame.Signature != NULL) &&
		(EffectiveFrame.Signature != pCallable->Signature)
	) {
		uint64 iFrameSignature;

		if ( !xrtFunctionSigValidate(EffectiveFrame.Signature) ) {
			__xrtCallWrap(XERR_ARGUMENT, XCALL_ERROR_SIGNATURE,
				"invoke", "the call frame signature is invalid");
			return false;
		}
		iFrameSignature = xrtFunctionSigId(EffectiveFrame.Signature);
		if ( iFrameSignature != xrtFunctionSigId(pCallable->Signature) ) {
			__xrtCallError(XERR_TYPE, XCALL_ERROR_SIGNATURE,
				"invoke", "the call frame signature does not match the callable");
			return false;
		}
	}
	if ( pCallable->Signature != NULL ) {
		EffectiveFrame.Signature = pCallable->Signature;
	}
	pEffectiveSignature = EffectiveFrame.Signature;
	if ( !xrtCallFrameValidate(&EffectiveFrame) ) {
		return false;
	}
	pPreviousError = xrtTakeError();
	bSuccess = pCallable->Entry(
		pCallable->Environment,
		&EffectiveFrame,
		&Temporary
	);
	pEntryError = xrtTakeError();
	if ( !bSuccess ) {
		xrtCallResultUnit(&Temporary);
		xrtErrorFree(pPreviousError);
		__xrtCallEntryError(pEntryError);
		return false;
	}
	if ( !__xrtCallResultShape(&Temporary) ) {
		xrtCallResultUnit(&Temporary);
		xrtErrorFree(pPreviousError);
		xrtErrorFree(pEntryError);
		return false;
	}
	if (
		(pEffectiveSignature != NULL) &&
		(Temporary.Count != pEffectiveSignature->ReturnCount)
	) {
		xrtCallResultUnit(&Temporary);
		xrtErrorFree(pPreviousError);
		xrtErrorFree(pEntryError);
		__xrtCallError(XERR_TYPE, XCALL_ERROR_RESULT,
			"invoke", "the callable returned an unexpected number of values");
		return false;
	}
	xrtErrorFree(pEntryError);
	if ( !xrtCallResultMove(pResult, &Temporary) ) {
		xrtCallResultUnit(&Temporary);
		xrtErrorFree(pPreviousError);
		return false;
	}
	__xrtCallRestoreError(pPreviousError);
	return true;
}

#endif
