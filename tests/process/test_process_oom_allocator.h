#ifndef XRT_TEST_PROCESS_OOM_ALLOCATOR_H
#define XRT_TEST_PROCESS_OOM_ALLOCATOR_H

#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
	typedef volatile LONG testprocessoomatomic;
#else
	#if defined(__TINYC__)
		#include <pthread.h>
	#endif
	typedef volatile int32 testprocessoomatomic;
#endif



/* Process OOM 分配器允许测试在一次执行中关闭并恢复分配。 */
typedef struct testprocessoomallocator {
	#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64)
		pthread_mutex_t Lock;
	#endif
	testprocessoomatomic Fail;
	testprocessoomatomic Denied;
} testprocessoomallocator;



/* 以获取语义读取跨线程故障开关。 */
static bool testProcessOomFailLoad(
	testprocessoomallocator* pState
)
{
	#if defined(_WIN32) || defined(_WIN64)
		return InterlockedCompareExchange(
			(LONG volatile*)&pState->Fail,
			0,
			0
		) != 0;
	#elif defined(__TINYC__)
		bool bFail;

		(void)pthread_mutex_lock(&pState->Lock);
		bFail = pState->Fail != 0;
		(void)pthread_mutex_unlock(&pState->Lock);
		return bFail;
	#else
		return __atomic_load_n(&pState->Fail, __ATOMIC_ACQUIRE) != 0;
	#endif
}



/* 以发布语义切换跨线程故障开关。 */
static void testProcessOomFailStore(
	testprocessoomallocator* pState,
	bool bFail
)
{
	#if defined(_WIN32) || defined(_WIN64)
		(void)InterlockedExchange(&pState->Fail, bFail ? 1 : 0);
	#elif defined(__TINYC__)
		(void)pthread_mutex_lock(&pState->Lock);
		pState->Fail = bFail ? 1 : 0;
		(void)pthread_mutex_unlock(&pState->Lock);
	#else
		__atomic_store_n(&pState->Fail, bFail ? 1 : 0, __ATOMIC_RELEASE);
	#endif
}



/* 原子记录被拒绝的分配次数。 */
static void testProcessOomDeniedAdd(testprocessoomallocator* pState)
{
	#if defined(_WIN32) || defined(_WIN64)
		(void)InterlockedIncrement(&pState->Denied);
	#elif defined(__TINYC__)
		(void)pthread_mutex_lock(&pState->Lock);
		pState->Denied++;
		(void)pthread_mutex_unlock(&pState->Lock);
	#else
		(void)__atomic_add_fetch(&pState->Denied, 1, __ATOMIC_RELAXED);
	#endif
}



/* 原子读取被拒绝的分配次数。 */
static uint32 testProcessOomDeniedLoad(
	testprocessoomallocator* pState
)
{
	#if defined(_WIN32) || defined(_WIN64)
		return (uint32)InterlockedCompareExchange(
			(LONG volatile*)&pState->Denied,
			0,
			0
		);
	#elif defined(__TINYC__)
		uint32 iDenied;

		(void)pthread_mutex_lock(&pState->Lock);
		iDenied = (uint32)pState->Denied;
		(void)pthread_mutex_unlock(&pState->Lock);
		return iDenied;
	#else
		return (uint32)__atomic_load_n(&pState->Denied, __ATOMIC_RELAXED);
	#endif
}



/* 故障窗口内拒绝分配，其他请求交给 C 运行库。 */
static ptr testProcessOomAlloc(ptr pContext, size_t iSize)
{
	testprocessoomallocator* pState =
		(testprocessoomallocator*)pContext;

	if ( testProcessOomFailLoad(pState) ) {
		testProcessOomDeniedAdd(pState);
		return NULL;
	}
	return malloc(iSize);
}



/* 重分配与普通分配共享同一个可恢复故障窗口。 */
static ptr testProcessOomRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	testprocessoomallocator* pState =
		(testprocessoomallocator*)pContext;

	if ( testProcessOomFailLoad(pState) ) {
		testProcessOomDeniedAdd(pState);
		return NULL;
	}
	return realloc(pMemory, iSize);
}



/* 释放始终可用，使失败路径能够完整回滚。 */
static void testProcessOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 在第一次 XRT 分配前安装 Process 故障分配器。 */
static bool testProcessOomInstall(testprocessoomallocator* pState)
{
	xallocator Allocator;

	#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64)
		if ( pthread_mutex_init(&pState->Lock, NULL) != 0 ) {
			return false;
		}
	#endif
	testProcessOomFailStore(pState, false);
	#if defined(_WIN32) || defined(_WIN64)
		(void)InterlockedExchange(&pState->Denied, 0);
	#elif defined(__TINYC__)
		(void)pthread_mutex_lock(&pState->Lock);
		pState->Denied = 0;
		(void)pthread_mutex_unlock(&pState->Lock);
	#else
		__atomic_store_n(&pState->Denied, 0, __ATOMIC_RELAXED);
	#endif
	Allocator.Context = pState;
	Allocator.Alloc = testProcessOomAlloc;
	Allocator.Realloc = testProcessOomRealloc;
	Allocator.Free = testProcessOomFree;
	return xrtSetAllocator(&Allocator);
}

#endif
