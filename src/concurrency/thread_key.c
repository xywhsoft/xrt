#include "../internal/xrt_internal.h"

#if !defined(_WIN32) && !defined(_WIN64)
	#include <pthread.h>
#endif



#if defined(XRT_FEATURE_THREAD_KEY)

/* 动态键由调用方和每个非空线程槽共同持有。 */
struct xthreadkey {
	xthreadkeyproc Destroy;
	volatile int32 References;
	volatile int32 Closed;
	#if defined(_WIN32) || defined(_WIN64)
		DWORD Index;
	#else
		pthread_key_t Key;
	#endif
};



/* 返回键是否已经进入逻辑关闭状态。 */
static bool __xrtThreadKeyClosed(const xthreadkey* pKey)
{
	return __xrtAtomicRefLoad(&pKey->Closed) != 0;
}



/* 每个非空值槽同时接入当前原生线程的清理链。 */
typedef struct xthreadkeyslot {
	struct xthreadkeyslot* Previous;
	struct xthreadkeyslot* Next;
	struct xthreadkeystate* State;
	xthreadkey* Key;
	ptr Value;
} xthreadkeyslot;



/* 线程状态只追踪当前线程尚未移交或析构的非空值。 */
typedef struct xthreadkeystate {
	xthreadkeyslot* Head;
} xthreadkeystate;



/* 析构过程反复安装值时，采用与 POSIX 线程键一致的有限清理轮次。 */
#define XRT_THREAD_KEY_CLEAR_PASSES 4u



/* 写入线程键平台错误。 */
static void __xrtThreadKeySetSystemError(cstr sOperation, int iCode, cstr sMessage)
{
	xerrordesc tDesc;
	xerror* pError;

	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.Kind = __xrtSystemErrorKind(iCode);
	tDesc.Code = 1;
	tDesc.SystemCode = iCode;
	tDesc.Domain = "xrt.thread_key";
	tDesc.Operation = sOperation;
	tDesc.Message = sMessage;
	pError = xrtErrorBuild(&tDesc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



#if defined(_WIN32) || defined(_WIN64)

static DWORD __xrtThreadKeyStateIndex = TLS_OUT_OF_INDEXES;
static volatile LONG __xrtThreadKeyStateInit;



/* 创建保存每线程清理链的 Windows TLS 索引。 */
static bool __xrtThreadKeyStateEnsure(void)
{
	LONG iState = InterlockedCompareExchange(&__xrtThreadKeyStateInit, 1, 0);

	if ( iState == 0 ) {
		__xrtThreadKeyStateIndex = TlsAlloc();
		InterlockedExchange(
			&__xrtThreadKeyStateInit,
			__xrtThreadKeyStateIndex != TLS_OUT_OF_INDEXES ? 2 : 3
		);
		return __xrtThreadKeyStateIndex != TLS_OUT_OF_INDEXES;
	}
	while ( (iState = InterlockedCompareExchange(
		&__xrtThreadKeyStateInit, 0, 0)) == 1 ) {
		Sleep(0);
	}
	return iState == 2;
}



/* 返回当前原生线程的值清理链。 */
static xthreadkeystate* __xrtThreadKeyStateGet(void)
{
	if ( __xrtThreadKeyStateInit != 2 ) {
		return NULL;
	}
	return (xthreadkeystate*)TlsGetValue(__xrtThreadKeyStateIndex);
}



/* 替换当前原生线程的值清理链。 */
static bool __xrtThreadKeyStateSet(xthreadkeystate* pState)
{
	if ( !__xrtThreadKeyStateEnsure() ||
		 !TlsSetValue(__xrtThreadKeyStateIndex, pState) ) {
		__xrtThreadKeySetSystemError(
			"state",
			(int)GetLastError(),
			"thread-local state update failed"
		);
		return false;
	}
	return true;
}



/* 返回当前原生线程的指定键值槽。 */
static xthreadkeyslot* __xrtThreadKeySlot(const xthreadkey* pKey)
{
	return (xthreadkeyslot*)TlsGetValue(pKey->Index);
}



/* 替换当前原生线程的指定键值槽。 */
static bool __xrtThreadKeySlotSet(xthreadkey* pKey, xthreadkeyslot* pSlot)
{
	if ( !TlsSetValue(pKey->Index, pSlot) ) {
		__xrtThreadKeySetSystemError(
			"set",
			(int)GetLastError(),
			"thread-local value update failed"
		);
		return false;
	}
	return true;
}

#else

static pthread_key_t __xrtThreadKeyStateKey;
static pthread_once_t __xrtThreadKeyStateOnce = PTHREAD_ONCE_INIT;
static int __xrtThreadKeyStateError;
static bool __xrtThreadKeyStateReady;



/* 释放线程退出时传入的一轮线程键状态。 */
static void __xrtThreadKeyStateExit(void* pData);



/* 创建保存每线程清理链的 POSIX pthread key。 */
static void __xrtThreadKeyStateInit(void)
{
	__xrtThreadKeyStateError = pthread_key_create(
		&__xrtThreadKeyStateKey,
		__xrtThreadKeyStateExit
	);
	__xrtThreadKeyStateReady = __xrtThreadKeyStateError == 0;
}



/* 确认 POSIX 每线程状态键已经创建。 */
static bool __xrtThreadKeyStateEnsure(void)
{
	int iResult = pthread_once(&__xrtThreadKeyStateOnce, __xrtThreadKeyStateInit);

	if ( iResult == 0 ) {
		iResult = __xrtThreadKeyStateError;
	}
	if ( iResult != 0 ) {
		__xrtThreadKeySetSystemError(
			"state",
			iResult,
			"thread-local state creation failed"
		);
		return false;
	}
	return true;
}



/* 返回当前原生线程的值清理链。 */
static xthreadkeystate* __xrtThreadKeyStateGet(void)
{
	if ( !__xrtThreadKeyStateReady ) {
		return NULL;
	}
	return (xthreadkeystate*)pthread_getspecific(__xrtThreadKeyStateKey);
}



/* 替换当前原生线程的值清理链。 */
static bool __xrtThreadKeyStateSet(xthreadkeystate* pState)
{
	int iResult;

	if ( !__xrtThreadKeyStateEnsure() ) {
		return false;
	}
	iResult = pthread_setspecific(__xrtThreadKeyStateKey, pState);
	if ( iResult != 0 ) {
		__xrtThreadKeySetSystemError(
			"state",
			iResult,
			"thread-local state update failed"
		);
		return false;
	}
	return true;
}



/* 返回当前原生线程的指定键值槽。 */
static xthreadkeyslot* __xrtThreadKeySlot(const xthreadkey* pKey)
{
	return (xthreadkeyslot*)pthread_getspecific(pKey->Key);
}



/* 替换当前原生线程的指定键值槽。 */
static bool __xrtThreadKeySlotSet(xthreadkey* pKey, xthreadkeyslot* pSlot)
{
	int iResult = pthread_setspecific(pKey->Key, pSlot);

	if ( iResult != 0 ) {
		__xrtThreadKeySetSystemError(
			"set",
			iResult,
			"thread-local value update failed"
		);
		return false;
	}
	return true;
}

#endif



/* 删除不再被任何调用方或线程槽引用的平台键。 */
static bool __xrtThreadKeyRelease(xthreadkey* pKey)
{
	int32 iReferences = xrtRefRelease(&pKey->References);
	bool bResult = true;

	if ( iReferences != 0 ) {
		return iReferences > 0;
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( !TlsFree(pKey->Index) ) {
			__xrtThreadKeySetSystemError(
				"destroy",
				(int)GetLastError(),
				"thread-local key destruction failed"
			);
			bResult = false;
		}
	#else
		{
			int iResult = pthread_key_delete(pKey->Key);

			if ( iResult != 0 ) {
				__xrtThreadKeySetSystemError(
					"destroy",
					iResult,
					"thread-local key destruction failed"
				);
				bResult = false;
			}
		}
	#endif
	xrtFree(pKey);
	return bResult;
}



/* 从当前线程清理链移除一个值槽。 */
static void __xrtThreadKeySlotUnlink(xthreadkeyslot* pSlot)
{
	xthreadkeystate* pState = pSlot->State;

	if ( pSlot->Previous != NULL ) {
		pSlot->Previous->Next = pSlot->Next;
	} else {
		pState->Head = pSlot->Next;
	}
	if ( pSlot->Next != NULL ) {
		pSlot->Next->Previous = pSlot->Previous;
	}
	pSlot->Previous = NULL;
	pSlot->Next = NULL;
	pSlot->State = NULL;
}



/* 当前线程不再保存值时释放空清理链。 */
static void __xrtThreadKeyStateDropEmpty(xthreadkeystate* pState)
{
	if ( (pState == NULL) || (pState->Head != NULL) ||
		 (__xrtThreadKeyStateGet() != pState) ) {
		return;
	}
	if ( __xrtThreadKeyStateSet(NULL) ) {
		xrtFree(pState);
	}
}



/* 取得当前清理链，必要时创建一个空状态。 */
static xthreadkeystate* __xrtThreadKeyStateRequire(bool* pCreated)
{
	xthreadkeystate* pState = __xrtThreadKeyStateGet();

	*pCreated = false;
	if ( pState != NULL ) {
		return pState;
	}
	pState = (xthreadkeystate*)xrtCalloc(1, sizeof(xthreadkeystate));
	if ( pState == NULL ) {
		return NULL;
	}
	if ( !__xrtThreadKeyStateSet(pState) ) {
		xrtFree(pState);
		return NULL;
	}
	*pCreated = true;
	return pState;
}



/* 分离并析构一轮当前线程拥有的全部值。 */
static void __xrtThreadKeyStateClear(xthreadkeystate* pState)
{
	xthreadkeyslot* pSlot;

	if ( pState == NULL ) {
		return;
	}
	if ( __xrtThreadKeyStateGet() == pState ) {
		(void)__xrtThreadKeyStateSet(NULL);
	}
	pSlot = pState->Head;
	pState->Head = NULL;
	while ( pSlot != NULL ) {
		xthreadkeyslot* pNext = pSlot->Next;
		xthreadkey* pKey = pSlot->Key;
		xthreadkeyproc pDestroy = pKey->Destroy;
		ptr pValue = pSlot->Value;

		(void)__xrtThreadKeySlotSet(pKey, NULL);
		xrtFree(pSlot);
		if ( (pDestroy != NULL) && (pValue != NULL) ) {
			pDestroy(pValue);
		}
		(void)__xrtThreadKeyRelease(pKey);
		pSlot = pNext;
	}
	xrtFree(pState);
}



#if !defined(_WIN32) && !defined(_WIN64)
/* POSIX 在线程退出时自动执行一轮值清理。 */
static void __xrtThreadKeyStateExit(void* pData)
{
	__xrtThreadKeyStateClear((xthreadkeystate*)pData);
}
#endif



/* 创建一个动态原生线程局部键。 */
XRT_API xthreadkey* xrtThreadKeyCreate(xthreadkeyproc pDestroy)
{
	xthreadkey* pKey = (xthreadkey*)xrtMalloc(sizeof(xthreadkey));

	if ( pKey == NULL ) {
		return NULL;
	}
	pKey->Destroy = pDestroy;
	pKey->References = 1;
	pKey->Closed = 0;
	#if defined(_WIN32) || defined(_WIN64)
		pKey->Index = TlsAlloc();
		if ( pKey->Index == TLS_OUT_OF_INDEXES ) {
			int iCode = (int)GetLastError();

			xrtFree(pKey);
			__xrtThreadKeySetSystemError(
				"create",
				iCode,
				"thread-local key creation failed"
			);
			return NULL;
		}
	#else
		{
			int iResult = pthread_key_create(&pKey->Key, NULL);

			if ( iResult != 0 ) {
				xrtFree(pKey);
				__xrtThreadKeySetSystemError(
					"create",
					iResult,
					"thread-local key creation failed"
				);
				return NULL;
			}
		}
	#endif
	return pKey;
}



/* 关闭动态键，并延迟到最后一个线程槽退出后释放平台资源。 */
XRT_API bool xrtThreadKeyDestroy(xthreadkey* pKey)
{
	xthreadkeyslot* pSlot;
	xthreadkeyproc pDestroy;
	ptr pValue;

	if ( pKey == NULL ) {
		return true;
	}
	if ( __xrtThreadKeyClosed(pKey) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	pSlot = __xrtThreadKeySlot(pKey);
	pDestroy = pKey->Destroy;
	pValue = pSlot != NULL ? pSlot->Value : NULL;
	if ( pSlot != NULL ) {
		xthreadkeystate* pState = pSlot->State;

		if ( !__xrtThreadKeySlotSet(pKey, NULL) ) {
			return false;
		}
		(void)__xrtAtomicRefCompareExchange(&pKey->Closed, 1, 0);
		__xrtThreadKeySlotUnlink(pSlot);
		xrtFree(pSlot);
		__xrtThreadKeyStateDropEmpty(pState);
	} else {
		(void)__xrtAtomicRefCompareExchange(&pKey->Closed, 1, 0);
	}
	if ( (pDestroy != NULL) && (pValue != NULL) ) {
		pDestroy(pValue);
	}
	if ( pSlot != NULL ) {
		(void)__xrtThreadKeyRelease(pKey);
	}
	return __xrtThreadKeyRelease(pKey);
}



/* 返回当前原生线程保存的借用值。 */
XRT_API ptr xrtThreadKeyGet(const xthreadkey* pKey)
{
	xthreadkeyslot* pSlot;

	if ( pKey == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( __xrtThreadKeyClosed(pKey) ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	pSlot = __xrtThreadKeySlot(pKey);
	return pSlot != NULL ? pSlot->Value : NULL;
}



/* 转移新值所有权，并在替换完成后析构旧值。 */
XRT_API bool xrtThreadKeySet(xthreadkey* pKey, ptr pValue)
{
	xthreadkeyslot* pSlot;
	ptr pOldValue;

	if ( pKey == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( __xrtThreadKeyClosed(pKey) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	pSlot = __xrtThreadKeySlot(pKey);
	if ( pSlot != NULL ) {
		xthreadkeystate* pState = pSlot->State;

		if ( pSlot->Value == pValue ) {
			return true;
		}
		pOldValue = pSlot->Value;
		if ( pValue == NULL ) {
			if ( !__xrtThreadKeySlotSet(pKey, NULL) ) {
				return false;
			}
			__xrtThreadKeySlotUnlink(pSlot);
			xrtFree(pSlot);
			__xrtThreadKeyStateDropEmpty(pState);
			(void)__xrtThreadKeyRelease(pKey);
		} else {
			pSlot->Value = pValue;
		}
		if ( (pKey->Destroy != NULL) && (pOldValue != NULL) ) {
			pKey->Destroy(pOldValue);
		}
		return true;
	}
	if ( pValue == NULL ) {
		return true;
	}
	{
		xthreadkeystate* pState;
		bool bCreated;

		pState = __xrtThreadKeyStateRequire(&bCreated);
		if ( pState == NULL ) {
			return false;
		}
		pSlot = (xthreadkeyslot*)xrtCalloc(1, sizeof(xthreadkeyslot));
		if ( pSlot == NULL ) {
			if ( bCreated ) {
				__xrtThreadKeyStateDropEmpty(pState);
			}
			return false;
		}
		pSlot->State = pState;
		pSlot->Key = pKey;
		pSlot->Value = pValue;
		if ( xrtRefRetain(&pKey->References) < 0 ) {
			xrtFree(pSlot);
			__xrtThreadKeyStateDropEmpty(pState);
			__xrtErrorSetInvalidState();
			return false;
		}
		pSlot->Next = pState->Head;
		if ( pState->Head != NULL ) {
			pState->Head->Previous = pSlot;
		}
		pState->Head = pSlot;
		if ( !__xrtThreadKeySlotSet(pKey, pSlot) ) {
			__xrtThreadKeySlotUnlink(pSlot);
			xrtFree(pSlot);
			__xrtThreadKeyStateDropEmpty(pState);
			(void)__xrtThreadKeyRelease(pKey);
			return false;
		}
	}
	return true;
}



/* 取走当前原生线程的值而不执行析构。 */
XRT_API ptr xrtThreadKeyTake(xthreadkey* pKey)
{
	xthreadkeyslot* pSlot;
	xthreadkeystate* pState;
	ptr pValue;

	if ( pKey == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( __xrtThreadKeyClosed(pKey) ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	pSlot = __xrtThreadKeySlot(pKey);
	if ( pSlot == NULL ) {
		return NULL;
	}
	if ( !__xrtThreadKeySlotSet(pKey, NULL) ) {
		return NULL;
	}
	pState = pSlot->State;
	pValue = pSlot->Value;
	__xrtThreadKeySlotUnlink(pSlot);
	xrtFree(pSlot);
	__xrtThreadKeyStateDropEmpty(pState);
	(void)__xrtThreadKeyRelease(pKey);
	return pValue;
}



/* 清理当前原生线程仍由全部动态键拥有的值。 */
XRT_API bool xrtThreadKeysClear(void)
{
	uint32 iPass;

	for ( iPass = 0; iPass < XRT_THREAD_KEY_CLEAR_PASSES; iPass++ ) {
		xthreadkeystate* pState = __xrtThreadKeyStateGet();

		if ( pState == NULL ) {
			return true;
		}
		__xrtThreadKeyStateClear(pState);
	}
	if ( __xrtThreadKeyStateGet() != NULL ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return true;
}

#endif
