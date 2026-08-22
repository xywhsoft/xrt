#include "../internal/xrt_internal.h"

#include <xrt/spin.h>



#if defined(XRT_FEATURE_SPIN)

#define XRT_SPIN_PAUSE_ROUNDS 64u
#define XRT_SPIN_YIELD_ROUNDS 16u



/* 验证自旋锁地址、对齐和初始化标记。 */
static bool __xrtSpinRequire(const xspinlock* pSpin)
{
	if ( !xrtMemRangeValid(pSpin, sizeof(*pSpin)) ||
		(((uintptr_t)pSpin & 3u) != 0u) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pSpin->Magic != XRT_SPIN_MAGIC ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return true;
}



/* 在持续竞争时主动让出处理器时间片。 */
static void __xrtSpinYield(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		(void)SwitchToThread();
	#else
		(void)sched_yield();
	#endif
}



/* 初始化调用方提供的自旋锁存储。 */
XRT_API bool xrtSpinInit(xspinlock* pSpin)
{
	if ( !xrtMemRangeValid(pSpin, sizeof(*pSpin)) ||
		(((uintptr_t)pSpin & 3u) != 0u) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	xrtAtomic32Init(&pSpin->State, 0u);
	pSpin->Magic = XRT_SPIN_MAGIC;
	return true;
}



/* 释放未被持有的自旋锁状态。 */
XRT_API bool xrtSpinUnit(xspinlock* pSpin)
{
	if ( !__xrtSpinRequire(pSpin) ) {
		return false;
	}
	if ( xrtAtomic32Load(&pSpin->State, XMEMORY_ACQUIRE) != 0u ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	pSpin->Magic = 0u;
	return true;
}



/* 创建动态自旋锁。 */
XRT_API xspinlock* xrtSpinCreate(void)
{
	xspinlock* pSpin = (xspinlock*)xrtMalloc(sizeof(*pSpin));

	if ( pSpin == NULL ) {
		return NULL;
	}
	if ( !xrtSpinInit(pSpin) ) {
		xrtFree(pSpin);
		return NULL;
	}
	return pSpin;
}



/* 销毁动态自旋锁。 */
XRT_API bool xrtSpinDestroy(xspinlock* pSpin)
{
	if ( pSpin == NULL ) {
		return true;
	}
	if ( !xrtSpinUnit(pSpin) ) {
		return false;
	}
	xrtFree(pSpin);
	return true;
}



/* 尝试进入短临界区。 */
XRT_API bool xrtSpinTryLock(xspinlock* pSpin)
{
	uint32 iExpected = 0u;

	if ( !__xrtSpinRequire(pSpin) ) {
		return false;
	}
	return xrtAtomic32CompareExchange(
		&pSpin->State,
		&iExpected,
		1u,
		XMEMORY_ACQUIRE,
		XMEMORY_RELAXED
	);
}



/* 先短暂自旋，再在长期竞争时让出处理器。 */
XRT_API bool xrtSpinLock(xspinlock* pSpin)
{
	uint32 iRounds = 0u;

	if ( !__xrtSpinRequire(pSpin) ) {
		return false;
	}
	for ( ;; ) {
		uint32 iExpected = 0u;

		if ( xrtAtomic32CompareExchange(
			&pSpin->State,
			&iExpected,
			1u,
			XMEMORY_ACQUIRE,
			XMEMORY_RELAXED
		) ) {
			return true;
		}
		for ( uint32 i = 0u; i < XRT_SPIN_PAUSE_ROUNDS; i++ ) {
			if ( xrtAtomic32Load(
				&pSpin->State,
				XMEMORY_RELAXED
			) == 0u ) {
				break;
			}
			xrtAtomicPause();
		}
		iRounds++;
		if ( iRounds >= XRT_SPIN_YIELD_ROUNDS ) {
			__xrtSpinYield();
			iRounds = 0u;
		}
	}
}



/* 以 Release 顺序离开短临界区。 */
XRT_API bool xrtSpinUnlock(xspinlock* pSpin)
{
	if ( !__xrtSpinRequire(pSpin) ) {
		return false;
	}
	if ( xrtAtomic32Load(&pSpin->State, XMEMORY_RELAXED) != 1u ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	xrtAtomic32Store(&pSpin->State, 0u, XMEMORY_RELEASE);
	return true;
}

#endif
