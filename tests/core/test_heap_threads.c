#include "../test.h"
#include "../test_thread.h"



#define TEST_HEAP_THREAD_COUNT 4
#define TEST_HEAP_ITERATIONS 20000
#define TEST_HEAP_EXIT_ITERATIONS 96



/* 测试分配器统计仍由底层持有的原始分配。 */
typedef struct test_heap_allocator {
	size_t ActiveCount;
	#if defined(_WIN32) || defined(_WIN64)
		CRITICAL_SECTION Lock;
	#else
		pthread_mutex_t Lock;
	#endif
} test_heap_allocator;



/* 锁定测试分配器。 */
static void testHeapAllocatorLock(test_heap_allocator* pAllocator)
{
	#if defined(_WIN32) || defined(_WIN64)
		EnterCriticalSection(&pAllocator->Lock);
	#else
		(void)pthread_mutex_lock(&pAllocator->Lock);
	#endif
}



/* 解锁测试分配器。 */
static void testHeapAllocatorUnlock(test_heap_allocator* pAllocator)
{
	#if defined(_WIN32) || defined(_WIN64)
		LeaveCriticalSection(&pAllocator->Lock);
	#else
		(void)pthread_mutex_unlock(&pAllocator->Lock);
	#endif
}



/* 统计一次底层分配。 */
static ptr testHeapAlloc(ptr pContext, size_t iSize)
{
	test_heap_allocator* pAllocator = (test_heap_allocator*)pContext;
	ptr pMemory = malloc(iSize);

	if ( pMemory != NULL ) {
		testHeapAllocatorLock(pAllocator);
		pAllocator->ActiveCount++;
		testHeapAllocatorUnlock(pAllocator);
	}
	return pMemory;
}



/* 调整底层分配，不改变活动块数量。 */
static ptr testHeapRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	(void)pContext;
	return realloc(pMemory, iSize);
}



/* 统计并执行一次底层释放。 */
static void testHeapFree(ptr pContext, ptr pMemory)
{
	test_heap_allocator* pAllocator = (test_heap_allocator*)pContext;

	if ( pMemory != NULL ) {
		testHeapAllocatorLock(pAllocator);
		pAllocator->ActiveCount--;
		testHeapAllocatorUnlock(pAllocator);
	}
	free(pMemory);
}



/* 读取当前仍由底层持有的分配数量。 */
static size_t testHeapActiveCount(test_heap_allocator* pAllocator)
{
	size_t iCount;

	testHeapAllocatorLock(pAllocator);
	iCount = pAllocator->ActiveCount;
	testHeapAllocatorUnlock(pAllocator);
	return iCount;
}



/* 每个原生线程独立记录压力测试失败。 */
typedef struct test_heap_thread_context {
	int Index;
} test_heap_thread_context;



/* 反复跨越池化和大块边界，验证线程缓存与中央堆并发安全。 */
static int testHeapThreadRun(ptr pData)
{
	test_heap_thread_context* pContext = (test_heap_thread_context*)pData;

	for ( size_t i = 0; i < TEST_HEAP_ITERATIONS; i++ ) {
		size_t iSize = ((i * 37u) + ((size_t)pContext->Index * 17u)) % 2048u;
		size_t iNewSize = ((i * 53u) + 1u) % 3072u;
		unsigned char iValue = (unsigned char)(i + (size_t)pContext->Index);
		unsigned char* pMemory = (unsigned char*)xrtMalloc(iSize);

		if ( pMemory == NULL ) {
			fprintf(
				stderr,
				"[heap-threads] alloc size=%zu kind=%d message=%s\n",
				iSize,
				(int)xrtErrorKind(xrtGetError()),
				xrtErrorMessage(xrtGetError())
			);
			return 1;
		}
		if ( iSize != 0 ) {
			memset(pMemory, iValue, iSize);
		}
		pMemory = (unsigned char*)xrtRealloc(pMemory, iNewSize);
		if ( iNewSize == 0 ) {
			if ( pMemory != NULL ) {
				return 2;
			}
			continue;
		}
		if ( pMemory == NULL ) {
			return 3;
		}
		if ( (iSize != 0) && (pMemory[0] != iValue) ) {
			xrtFree(pMemory);
			return 4;
		}
		xrtFree(pMemory);
	}

	return 0;
}



/* 在线程中使用同一尺寸类并依靠线程退出析构归还缓存。 */
static int testHeapThreadExitRun(ptr pData)
{
	ptr pMemory;

	(void)pData;
	pMemory = xrtMalloc(32);
	if ( pMemory == NULL ) {
		return 1;
	}
	xrtFree(pMemory);
	return 0;
}



/* 启动多个未附加原生线程验证缓存、中央堆和退出析构。 */
int main(void)
{
	static test_heap_allocator AllocatorState;
	xallocator Allocator;
	test_heap_thread_context arrContext[TEST_HEAP_THREAD_COUNT];
	testthread arrThread[TEST_HEAP_THREAD_COUNT];
	size_t iExitBaseline = 0;

	memset(&AllocatorState, 0, sizeof(AllocatorState));
	#if defined(_WIN32) || defined(_WIN64)
		InitializeCriticalSection(&AllocatorState.Lock);
	#else
		testRequire(
			pthread_mutex_init(&AllocatorState.Lock, NULL) == 0,
			"allocator lock initialization failed"
		);
	#endif
	Allocator.Context = &AllocatorState;
	Allocator.Alloc = testHeapAlloc;
	Allocator.Realloc = testHeapRealloc;
	Allocator.Free = testHeapFree;
	testRequire(
		xrtSetAllocator(&Allocator),
		"heap thread allocator installation failed"
	);

	memset(arrContext, 0, sizeof(arrContext));
	for ( int i = 0; i < TEST_HEAP_THREAD_COUNT; i++ ) {
		arrContext[i].Index = i;
		arrThread[i].Proc = testHeapThreadRun;
		arrThread[i].Data = &arrContext[i];
	}
	testThreadsStart(arrThread, TEST_HEAP_THREAD_COUNT);
	testThreadsJoin(arrThread, TEST_HEAP_THREAD_COUNT);
	for ( int i = 0; i < TEST_HEAP_THREAD_COUNT; i++ ) {
		if ( arrThread[i].Result != 0 ) {
			fprintf(
				stderr,
				"[heap-threads] worker=%d result=%d\n",
				i,
				arrThread[i].Result
			);
		}
		testRequire(arrThread[i].Result == 0, "heap worker failed");
	}

	/* 顺序线程不应把线程缓存元数据或空闲块永久留在线程中。 */
	for ( int i = 0; i < TEST_HEAP_EXIT_ITERATIONS; i++ ) {
		testthread Thread;

		Thread.Proc = testHeapThreadExitRun;
		Thread.Data = NULL;
		testThreadsStart(&Thread, 1);
		testThreadsJoin(&Thread, 1);
		testRequire(Thread.Result == 0, "heap exit worker failed");
		if ( i == 0 ) {
			iExitBaseline = testHeapActiveCount(&AllocatorState);
		}
	}
	testRequire(
		testHeapActiveCount(&AllocatorState) == iExitBaseline,
		"thread exit retained heap cache allocations"
	);
	/* 分配器上下文按契约保留到进程退出，供主线程 TLS 析构使用。 */
	printf("[PASS] heap-threads\n");
	return 0;
}
