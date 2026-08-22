#include "../internal/xrt_memory.h"



#if defined(XRT_FEATURE_MEMORY_STATS)

#define XRT_MEM_STATS_STRIPE_COUNT 16u



/* 高频计数按线程哈希分散，避免所有线程争用同一缓存行。 */
typedef struct xrt_mem_stats_counter {
	uint64 MallocCalls;
	uint64 MallocBytes;
	uint64 CallocCalls;
	uint64 CallocBytes;
	uint64 ReallocCalls;
	uint64 ReallocBytes;
	uint64 MemDupCalls;
	uint64 MemDupBytes;
	uint64 FreeCalls;
	uint64 TempCalls;
	uint64 TempBytes;
	uint64 BlockAllocCalls;
	uint64 BlockAllocBytes;
	uint64 BlockFreeCalls;
	uint64 BlockFreeBytes;
	uint64 PooledAllocCalls;
	uint64 PooledAllocBytes;
	uint64 DirectAllocCalls;
	uint64 DirectAllocBytes;
	uint64 BackingAllocCalls;
	uint64 BackingAllocBytes;
	uint64 BackingReallocCalls;
	uint64 BackingReallocBytes;
	uint64 BackingFreeCalls;
} xrt_mem_stats_counter;



/* 每个统计槽拥有独立短锁和计数。 */
typedef struct xrt_mem_stats_stripe {
	xrt_spinlock Lock;
	xrt_mem_stats_counter Counter;
} xrt_mem_stats_stripe;



/* 尺寸类计数较冷，按尺寸类分别加锁以控制静态体积。 */
typedef struct xrt_mem_stats_state {
	volatile int32 InitState;
	volatile int32 Enabled;
	xrt_mem_stats_stripe Stripes[XRT_MEM_STATS_STRIPE_COUNT];
	xrt_spinlock ClassLocks[XRT_MEM_STATS_CLASS_COUNT];
	uint64 ClassCalls[XRT_MEM_STATS_CLASS_COUNT];
	uint64 ClassBytes[XRT_MEM_STATS_CLASS_COUNT];
} xrt_mem_stats_state;



static xrt_mem_stats_state __xrtMemStats;

#if !defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64)
static XRT_THREAD_LOCAL uint8 __xrtMemStatsThreadMarker;
#endif

#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64)
static pthread_once_t __xrtMemStatsOnce = PTHREAD_ONCE_INIT;
#endif



/* 初始化全部统计槽和尺寸类锁。 */
static void __xrtMemStatsInit(void)
{
	for ( uint32 i = 0; i < XRT_MEM_STATS_STRIPE_COUNT; i++ ) {
		__xrtSpinInit(&__xrtMemStats.Stripes[i].Lock);
	}
	for ( uint32 i = 0; i < XRT_MEM_STATS_CLASS_COUNT; i++ ) {
		__xrtSpinInit(&__xrtMemStats.ClassLocks[i]);
	}
}



/* 线程安全地完成内存统计初始化。 */
static void __xrtMemStatsEnsure(void)
{
	#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64)
		(void)pthread_once(&__xrtMemStatsOnce, __xrtMemStatsInit);
	#else
		int32 iState;

		#if defined(_MSC_VER)
			iState = (int32)_InterlockedCompareExchange((volatile long*)&__xrtMemStats.InitState, 1, 0);
		#elif defined(__TINYC__) && (defined(_WIN32) || defined(_WIN64))
			iState = (int32)InterlockedCompareExchange((volatile LONG*)&__xrtMemStats.InitState, 1, 0);
		#else
			iState = (int32)__sync_val_compare_and_swap(&__xrtMemStats.InitState, 0, 1);
		#endif

		if ( iState == 0 ) {
			__xrtMemStatsInit();
			#if defined(_MSC_VER)
				(void)_InterlockedExchange((volatile long*)&__xrtMemStats.InitState, 2);
			#elif defined(__TINYC__) && (defined(_WIN32) || defined(_WIN64))
				(void)InterlockedExchange((volatile LONG*)&__xrtMemStats.InitState, 2);
			#else
				(void)__sync_lock_test_and_set(&__xrtMemStats.InitState, 2);
			#endif
			return;
		}

		while ( __xrtAtomicRefLoad(&__xrtMemStats.InitState) != 2 ) {
			#if defined(_WIN32) || defined(_WIN64)
				(void)SwitchToThread();
			#else
				(void)sched_yield();
			#endif
		}
	#endif
}



/* 原子读取运行时统计开关。 */
static bool __xrtMemStatsEnabledLoad(void)
{
	#if defined(_MSC_VER)
		return _InterlockedCompareExchange((volatile long*)&__xrtMemStats.Enabled, 0, 0) != 0;
	#elif defined(__TINYC__) && (defined(_WIN32) || defined(_WIN64))
		return InterlockedCompareExchange((volatile LONG*)&__xrtMemStats.Enabled, 0, 0) != 0;
	#elif defined(__TINYC__)
		return __xrtMemStats.Enabled != 0;
	#else
		return __sync_val_compare_and_swap(&__xrtMemStats.Enabled, 0, 0) != 0;
	#endif
}



/* 仅在具备原子读取的平台执行关闭状态快速返回。 */
static bool __xrtMemStatsFastEnabled(void)
{
	#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64)
		return true;
	#else
		return __xrtMemStatsEnabledLoad();
	#endif
}



/* 原子写入运行时统计开关。 */
static void __xrtMemStatsEnabledStore(bool bEnabled)
{
	#if defined(_MSC_VER)
		(void)_InterlockedExchange((volatile long*)&__xrtMemStats.Enabled, bEnabled ? 1 : 0);
	#elif defined(__TINYC__) && (defined(_WIN32) || defined(_WIN64))
		(void)InterlockedExchange((volatile LONG*)&__xrtMemStats.Enabled, bEnabled ? 1 : 0);
	#elif defined(__TINYC__)
		__xrtMemStats.Enabled = bEnabled ? 1 : 0;
	#else
		(void)__sync_lock_test_and_set(&__xrtMemStats.Enabled, bEnabled ? 1 : 0);
	#endif
}



/* 选择当前线程稳定使用的统计槽。 */
static uint32 __xrtMemStatsStripeIndex(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		return ((uint32)GetCurrentThreadId()) & (XRT_MEM_STATS_STRIPE_COUNT - 1);
	#elif defined(__TINYC__)
		return ((uint32)(uintptr_t)pthread_self()) & (XRT_MEM_STATS_STRIPE_COUNT - 1);
	#else
		uintptr_t iAddress = (uintptr_t)&__xrtMemStatsThreadMarker;

		return (uint32)((iAddress >> 4) & (XRT_MEM_STATS_STRIPE_COUNT - 1));
	#endif
}



/* 按固定顺序锁住所有统计槽，形成快照线性化边界。 */
static void __xrtMemStatsLockAll(void)
{
	for ( uint32 i = 0; i < XRT_MEM_STATS_STRIPE_COUNT; i++ ) {
		__xrtSpinLock(&__xrtMemStats.Stripes[i].Lock);
	}
}



/* 按反序释放所有统计槽。 */
static void __xrtMemStatsUnlockAll(void)
{
	for ( uint32 i = XRT_MEM_STATS_STRIPE_COUNT; i != 0; i-- ) {
		__xrtSpinUnlock(&__xrtMemStats.Stripes[i - 1].Lock);
	}
}



/* 对长期运行计数执行无回绕的 uint64 饱和加法。 */
static void __xrtMemStatsAdd(uint64* pValue, uint64 iAdd)
{
	if ( *pValue > (UINT64_MAX - iAdd) ) {
		*pValue = UINT64_MAX;
	} else {
		*pValue += iAdd;
	}
}



/* 将一个槽的全部字段累加到公开快照。 */
static void __xrtMemStatsAccumulate(xmemstats* pStats, const xrt_mem_stats_counter* pCounter)
{
	__xrtMemStatsAdd(&pStats->MallocCalls, pCounter->MallocCalls);
	__xrtMemStatsAdd(&pStats->MallocBytes, pCounter->MallocBytes);
	__xrtMemStatsAdd(&pStats->CallocCalls, pCounter->CallocCalls);
	__xrtMemStatsAdd(&pStats->CallocBytes, pCounter->CallocBytes);
	__xrtMemStatsAdd(&pStats->ReallocCalls, pCounter->ReallocCalls);
	__xrtMemStatsAdd(&pStats->ReallocBytes, pCounter->ReallocBytes);
	__xrtMemStatsAdd(&pStats->MemDupCalls, pCounter->MemDupCalls);
	__xrtMemStatsAdd(&pStats->MemDupBytes, pCounter->MemDupBytes);
	__xrtMemStatsAdd(&pStats->FreeCalls, pCounter->FreeCalls);
	__xrtMemStatsAdd(&pStats->TempCalls, pCounter->TempCalls);
	__xrtMemStatsAdd(&pStats->TempBytes, pCounter->TempBytes);
	__xrtMemStatsAdd(&pStats->BlockAllocCalls, pCounter->BlockAllocCalls);
	__xrtMemStatsAdd(&pStats->BlockAllocBytes, pCounter->BlockAllocBytes);
	__xrtMemStatsAdd(&pStats->BlockFreeCalls, pCounter->BlockFreeCalls);
	__xrtMemStatsAdd(&pStats->BlockFreeBytes, pCounter->BlockFreeBytes);
	__xrtMemStatsAdd(&pStats->PooledAllocCalls, pCounter->PooledAllocCalls);
	__xrtMemStatsAdd(&pStats->PooledAllocBytes, pCounter->PooledAllocBytes);
	__xrtMemStatsAdd(&pStats->DirectAllocCalls, pCounter->DirectAllocCalls);
	__xrtMemStatsAdd(&pStats->DirectAllocBytes, pCounter->DirectAllocBytes);
	__xrtMemStatsAdd(&pStats->BackingAllocCalls, pCounter->BackingAllocCalls);
	__xrtMemStatsAdd(&pStats->BackingAllocBytes, pCounter->BackingAllocBytes);
	__xrtMemStatsAdd(&pStats->BackingReallocCalls, pCounter->BackingReallocCalls);
	__xrtMemStatsAdd(&pStats->BackingReallocBytes, pCounter->BackingReallocBytes);
	__xrtMemStatsAdd(&pStats->BackingFreeCalls, pCounter->BackingFreeCalls);
}



/* 记录一次公开内存 API 或底层分配器请求。 */
void __xrtMemStatsRecord(uint32 iOperation, size_t iSize)
{
	xrt_mem_stats_stripe* pStripe;

	__xrtMemStatsEnsure();
	if ( !__xrtMemStatsFastEnabled() ) {
		return;
	}
	pStripe = &__xrtMemStats.Stripes[__xrtMemStatsStripeIndex()];
	__xrtSpinLock(&pStripe->Lock);
	if ( __xrtMemStatsEnabledLoad() ) {
		switch ( iOperation ) {
			case XRT_MEM_STATS_OP_MALLOC:
				__xrtMemStatsAdd(&pStripe->Counter.MallocCalls, 1);
				__xrtMemStatsAdd(&pStripe->Counter.MallocBytes, (uint64)iSize);
				break;
			case XRT_MEM_STATS_OP_CALLOC:
				__xrtMemStatsAdd(&pStripe->Counter.CallocCalls, 1);
				__xrtMemStatsAdd(&pStripe->Counter.CallocBytes, (uint64)iSize);
				break;
			case XRT_MEM_STATS_OP_REALLOC:
				__xrtMemStatsAdd(&pStripe->Counter.ReallocCalls, 1);
				__xrtMemStatsAdd(&pStripe->Counter.ReallocBytes, (uint64)iSize);
				break;
			case XRT_MEM_STATS_OP_MEMDUP:
				__xrtMemStatsAdd(&pStripe->Counter.MemDupCalls, 1);
				__xrtMemStatsAdd(&pStripe->Counter.MemDupBytes, (uint64)iSize);
				break;
			case XRT_MEM_STATS_OP_FREE:
				__xrtMemStatsAdd(&pStripe->Counter.FreeCalls, 1);
				break;
			case XRT_MEM_STATS_OP_BACKING_ALLOC:
				__xrtMemStatsAdd(&pStripe->Counter.BackingAllocCalls, 1);
				__xrtMemStatsAdd(&pStripe->Counter.BackingAllocBytes, (uint64)iSize);
				break;
			case XRT_MEM_STATS_OP_BACKING_REALLOC:
				__xrtMemStatsAdd(&pStripe->Counter.BackingReallocCalls, 1);
				__xrtMemStatsAdd(&pStripe->Counter.BackingReallocBytes, (uint64)iSize);
				break;
			case XRT_MEM_STATS_OP_BACKING_FREE:
				__xrtMemStatsAdd(&pStripe->Counter.BackingFreeCalls, 1);
				break;
			default:
				break;
		}
	}
	__xrtSpinUnlock(&pStripe->Lock);
}



/* 记录全局堆取得一个池化或 backing 块。 */
void __xrtMemStatsBlockAlloc(size_t iSize, uint16 iClass, bool bPooled)
{
	xrt_mem_stats_stripe* pStripe;

	__xrtMemStatsEnsure();
	if ( !__xrtMemStatsFastEnabled() ) {
		return;
	}
	pStripe = &__xrtMemStats.Stripes[__xrtMemStatsStripeIndex()];
	__xrtSpinLock(&pStripe->Lock);
	if ( __xrtMemStatsEnabledLoad() ) {
		__xrtMemStatsAdd(&pStripe->Counter.BlockAllocCalls, 1);
		__xrtMemStatsAdd(&pStripe->Counter.BlockAllocBytes, (uint64)iSize);
		if ( bPooled ) {
			__xrtMemStatsAdd(&pStripe->Counter.PooledAllocCalls, 1);
			__xrtMemStatsAdd(&pStripe->Counter.PooledAllocBytes, (uint64)iSize);
			if ( iClass < XRT_MEM_STATS_CLASS_COUNT ) {
				__xrtSpinLock(&__xrtMemStats.ClassLocks[iClass]);
				__xrtMemStatsAdd(&__xrtMemStats.ClassCalls[iClass], 1);
				__xrtMemStatsAdd(&__xrtMemStats.ClassBytes[iClass], (uint64)iSize);
				__xrtSpinUnlock(&__xrtMemStats.ClassLocks[iClass]);
			}
		} else {
			__xrtMemStatsAdd(&pStripe->Counter.DirectAllocCalls, 1);
			__xrtMemStatsAdd(&pStripe->Counter.DirectAllocBytes, (uint64)iSize);
		}
	}
	__xrtSpinUnlock(&pStripe->Lock);
}



/* 记录全局堆释放一个逻辑块。 */
void __xrtMemStatsBlockFree(size_t iSize, uint16 iClass, bool bPooled)
{
	xrt_mem_stats_stripe* pStripe;

	(void)iClass;
	(void)bPooled;
	__xrtMemStatsEnsure();
	if ( !__xrtMemStatsFastEnabled() ) {
		return;
	}
	pStripe = &__xrtMemStats.Stripes[__xrtMemStatsStripeIndex()];
	__xrtSpinLock(&pStripe->Lock);
	if ( __xrtMemStatsEnabledLoad() ) {
		__xrtMemStatsAdd(&pStripe->Counter.BlockFreeCalls, 1);
		__xrtMemStatsAdd(&pStripe->Counter.BlockFreeBytes, (uint64)iSize);
	}
	__xrtSpinUnlock(&pStripe->Lock);
}



/* 记录一次临时内存请求。 */
void __xrtMemStatsTemp(size_t iSize)
{
	xrt_mem_stats_stripe* pStripe;

	__xrtMemStatsEnsure();
	if ( !__xrtMemStatsFastEnabled() ) {
		return;
	}
	pStripe = &__xrtMemStats.Stripes[__xrtMemStatsStripeIndex()];
	__xrtSpinLock(&pStripe->Lock);
	if ( __xrtMemStatsEnabledLoad() ) {
		__xrtMemStatsAdd(&pStripe->Counter.TempCalls, 1);
		__xrtMemStatsAdd(&pStripe->Counter.TempBytes, (uint64)iSize);
	}
	__xrtSpinUnlock(&pStripe->Lock);
}



/* 在线性化边界切换内存统计。 */
XRT_API void xrtMemStatsEnable(bool bEnable)
{
	__xrtMemStatsEnsure();
	__xrtMemStatsLockAll();
	__xrtMemStatsEnabledStore(bEnable);
	__xrtMemStatsUnlockAll();
}



/* 返回当前内存统计开关。 */
XRT_API bool xrtMemStatsEnabled(void)
{
	__xrtMemStatsEnsure();
	#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64)
		bool bEnabled;

		__xrtSpinLock(&__xrtMemStats.Stripes[0].Lock);
		bEnabled = __xrtMemStatsEnabledLoad();
		__xrtSpinUnlock(&__xrtMemStats.Stripes[0].Lock);
		return bEnabled;
	#else
		return __xrtMemStatsEnabledLoad();
	#endif
}



/* 在线性化边界清空所有统计计数。 */
XRT_API void xrtMemStatsReset(void)
{
	__xrtMemStatsEnsure();
	__xrtMemStatsLockAll();
	for ( uint32 i = 0; i < XRT_MEM_STATS_STRIPE_COUNT; i++ ) {
		memset(&__xrtMemStats.Stripes[i].Counter, 0, sizeof(xrt_mem_stats_counter));
	}
	memset(__xrtMemStats.ClassCalls, 0, sizeof(__xrtMemStats.ClassCalls));
	memset(__xrtMemStats.ClassBytes, 0, sizeof(__xrtMemStats.ClassBytes));
	__xrtMemStatsUnlockAll();
}



/* 获取字段相互一致的内存统计快照。 */
XRT_API void xrtMemStatsGet(xmemstats* pStats)
{
	if ( pStats == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}

	__xrtMemStatsEnsure();
	__xrtMemStatsLockAll();
	memset(pStats, 0, sizeof(xmemstats));
	pStats->Enabled = __xrtMemStatsEnabledLoad();
	pStats->ClassStep = XRT_MEM_STATS_CLASS_STEP;
	pStats->ClassCutoff = XRT_MEM_STATS_CLASS_CUTOFF;
	pStats->ClassCount = XRT_MEM_STATS_CLASS_COUNT;
	for ( uint32 i = 0; i < XRT_MEM_STATS_STRIPE_COUNT; i++ ) {
		__xrtMemStatsAccumulate(pStats, &__xrtMemStats.Stripes[i].Counter);
	}
	for ( uint32 i = 0; i < XRT_MEM_STATS_CLASS_COUNT; i++ ) {
		pStats->ClassCalls[i] = __xrtMemStats.ClassCalls[i];
		pStats->ClassBytes[i] = __xrtMemStats.ClassBytes[i];
	}
	__xrtMemStatsUnlockAll();
}

#endif
