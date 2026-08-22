#include "../internal/xrt_internal.h"

#if !defined(_WIN32) && !defined(_WIN64)
	#include <time.h>
#endif



#if defined(XRT_FEATURE_ONCE)

#define XRT_ONCE_PENDING 0
#define XRT_ONCE_RUNNING 1
#define XRT_ONCE_COMPLETE 2



/* Once 的内部布局保留初始化状态和递归检测所需的线程标识。 */
typedef struct xrt_once_impl {
	volatile int32 State;
	uint32 Reserved;
	volatile uint64 Owner;
} xrt_once_impl;



typedef char xrt_once_storage_check[
	(sizeof(xrt_once_impl) <= XRT_ONCE_STORAGE_SIZE) ? 1 : -1
];



#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64)
static pthread_mutex_t __xrtOnceAtomicLock = PTHREAD_MUTEX_INITIALIZER;
#endif



/* 原子读取 Once 状态。 */
static int32 __xrtOnceLoad(const volatile int32* pState)
{
	#if defined(_WIN32) || defined(_WIN64)
		return (int32)InterlockedCompareExchange((volatile LONG*)pState, 0, 0);
	#elif defined(__TINYC__)
		int32 iState;

		(void)pthread_mutex_lock(&__xrtOnceAtomicLock);
		iState = *pState;
		(void)pthread_mutex_unlock(&__xrtOnceAtomicLock);
		return iState;
	#else
		return (int32)__sync_val_compare_and_swap((volatile int32*)pState, 0, 0);
	#endif
}



/* 原子读取正在执行初始化的线程标识。 */
static uint64 __xrtOnceOwnerLoad(const volatile uint64* pOwner)
{
	#if defined(_WIN32) || defined(_WIN64)
		return (uint64)InterlockedCompareExchange64(
			(volatile LONG64*)pOwner,
			0,
			0
		);
	#elif defined(__TINYC__)
		uint64 iOwner;

		(void)pthread_mutex_lock(&__xrtOnceAtomicLock);
		iOwner = *pOwner;
		(void)pthread_mutex_unlock(&__xrtOnceAtomicLock);
		return iOwner;
	#else
		return (uint64)__sync_val_compare_and_swap(
			(volatile uint64*)pOwner,
			0,
			0
		);
	#endif
}



/* 原子发布或清除正在执行初始化的线程标识。 */
static void __xrtOnceOwnerStore(volatile uint64* pOwner, uint64 iOwner)
{
	#if defined(_WIN32) || defined(_WIN64)
		(void)InterlockedExchange64((volatile LONG64*)pOwner, (LONG64)iOwner);
	#elif defined(__TINYC__)
		(void)pthread_mutex_lock(&__xrtOnceAtomicLock);
		*pOwner = iOwner;
		(void)pthread_mutex_unlock(&__xrtOnceAtomicLock);
	#else
		(void)__sync_lock_test_and_set(pOwner, iOwner);
	#endif
}



/* 尝试把 Once 从待处理状态切换为运行状态。 */
static bool __xrtOnceStart(volatile int32* pState)
{
	#if defined(_WIN32) || defined(_WIN64)
		return InterlockedCompareExchange(
			(volatile LONG*)pState,
			XRT_ONCE_RUNNING,
			XRT_ONCE_PENDING
		) == XRT_ONCE_PENDING;
	#elif defined(__TINYC__)
		bool bStarted;

		(void)pthread_mutex_lock(&__xrtOnceAtomicLock);
		bStarted = *pState == XRT_ONCE_PENDING;
		if ( bStarted ) {
			*pState = XRT_ONCE_RUNNING;
		}
		(void)pthread_mutex_unlock(&__xrtOnceAtomicLock);
		return bStarted;
	#else
		return __sync_val_compare_and_swap(
			pState,
			XRT_ONCE_PENDING,
			XRT_ONCE_RUNNING
		) == XRT_ONCE_PENDING;
	#endif
}



/* 发布初始化的完成或失败状态。 */
static void __xrtOnceFinish(volatile int32* pState, int32 iState)
{
	#if defined(_WIN32) || defined(_WIN64)
		(void)InterlockedExchange((volatile LONG*)pState, (LONG)iState);
	#elif defined(__TINYC__)
		(void)pthread_mutex_lock(&__xrtOnceAtomicLock);
		*pState = iState;
		(void)pthread_mutex_unlock(&__xrtOnceAtomicLock);
	#else
		__sync_synchronize();
		(void)__sync_lock_test_and_set(pState, iState);
	#endif
}



/* 初始化短暂竞争时让出处理器，长时间竞争时降低空转负载。 */
static void __xrtOnceWait(uint32 iAttempt)
{
	#if defined(_WIN32) || defined(_WIN64)
		if ( iAttempt < 64u ) {
			(void)SwitchToThread();
		} else {
			Sleep(1);
		}
	#else
		if ( iAttempt < 64u ) {
			(void)sched_yield();
		} else {
			struct timespec tDelay = { 0, 1000000 };

			(void)nanosleep(&tDelay, NULL);
		}
	#endif
}



/* 并发执行一次可失败初始化。 */
XRT_API bool xrtOnce(xonce* pOnce, xonceproc pProc, ptr pData)
{
	xrt_once_impl* pImpl;
	uint64 iCurrent;
	uint32 iAttempt = 0;

	if ( (pOnce == NULL) || (pProc == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pImpl = (xrt_once_impl*)pOnce;
	iCurrent = __xrtCurrentThreadId();
	for ( ;; ) {
		int32 iState = __xrtOnceLoad(&pImpl->State);

		if ( iState == XRT_ONCE_COMPLETE ) {
			return true;
		}
		if ( iState == XRT_ONCE_PENDING ) {
			if ( __xrtOnceStart(&pImpl->State) ) {
				bool bResult;

				__xrtOnceOwnerStore(&pImpl->Owner, iCurrent);
				bResult = pProc(pData);
				__xrtOnceOwnerStore(&pImpl->Owner, 0);
				__xrtOnceFinish(
					&pImpl->State,
					bResult ? XRT_ONCE_COMPLETE : XRT_ONCE_PENDING
				);
				return bResult;
			}
			iAttempt++;
			continue;
		}
		if (
			(iState == XRT_ONCE_RUNNING) &&
			(__xrtOnceOwnerLoad(&pImpl->Owner) == iCurrent)
		) {
			__xrtErrorSetInvalidState();
			return false;
		}
		__xrtOnceWait(iAttempt);
		if ( iAttempt != UINT32_MAX ) {
			iAttempt++;
		}
	}
}

#endif
