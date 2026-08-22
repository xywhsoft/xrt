#include "../test.h"
#include "../test_thread.h"



#define TEST_ERROR_THREAD_COUNT 4
#define TEST_ERROR_ITERATIONS 10000
#define TEST_ERROR_EXIT_ITERATIONS 96



/* 测试分配器统计仍由底层持有的原始分配。 */
typedef struct test_error_allocator {
	size_t ActiveCount;
	#if defined(_WIN32) || defined(_WIN64)
		CRITICAL_SECTION Lock;
	#else
		pthread_mutex_t Lock;
	#endif
} test_error_allocator;



/* 锁定测试分配器计数器。 */
static void testErrorAllocatorLock(test_error_allocator* pAllocator)
{
	#if defined(_WIN32) || defined(_WIN64)
		EnterCriticalSection(&pAllocator->Lock);
	#else
		(void)pthread_mutex_lock(&pAllocator->Lock);
	#endif
}



/* 解锁测试分配器计数器。 */
static void testErrorAllocatorUnlock(test_error_allocator* pAllocator)
{
	#if defined(_WIN32) || defined(_WIN64)
		LeaveCriticalSection(&pAllocator->Lock);
	#else
		(void)pthread_mutex_unlock(&pAllocator->Lock);
	#endif
}



/* 统计并执行一次底层分配。 */
static ptr testErrorAlloc(ptr pContext, size_t iSize)
{
	test_error_allocator* pAllocator = (test_error_allocator*)pContext;
	ptr pMemory = malloc(iSize);

	if ( pMemory != NULL ) {
		testErrorAllocatorLock(pAllocator);
		pAllocator->ActiveCount++;
		testErrorAllocatorUnlock(pAllocator);
	}
	return pMemory;
}



/* 执行一次底层重分配。 */
static ptr testErrorRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	(void)pContext;
	return realloc(pMemory, iSize);
}



/* 执行一次底层释放。 */
static void testErrorFree(ptr pContext, ptr pMemory)
{
	test_error_allocator* pAllocator = (test_error_allocator*)pContext;

	if ( pMemory != NULL ) {
		testErrorAllocatorLock(pAllocator);
		pAllocator->ActiveCount--;
		testErrorAllocatorUnlock(pAllocator);
	}
	free(pMemory);
}



/* 线程退出时故意不清除错误，用于验证 TLS 析构。 */
static int testErrorThreadExitRun(ptr pData)
{
	xerror* pError;

	(void)pData;
	pError = xrtErrorCreate(
		XERR_IO,
		"test.thread.exit",
		1,
		"thread exit error"
	);
	if ( pError == NULL ) {
		return 1;
	}
	xrtSetError(pError);
	xrtErrorFree(pError);
	return 0;
}



/* 读取当前仍由底层持有的原始分配数量。 */
static size_t testErrorAllocatorActiveCount(test_error_allocator* pAllocator)
{
	size_t iCount;

	testErrorAllocatorLock(pAllocator);
	iCount = pAllocator->ActiveCount;
	testErrorAllocatorUnlock(pAllocator);
	return iCount;
}



/* 每个线程使用不同代码验证当前错误互不污染。 */
typedef struct test_error_thread_context {
	int32 Code;
} test_error_thread_context;



/* 在线程错误槽中反复设置、读取和清除独立错误。 */
static int testErrorThreadRun(ptr pData)
{
	test_error_thread_context* pContext = (test_error_thread_context*)pData;
	xerror* pError;

	if ( xrtGetError() != NULL ) {
		return 1;
	}
	pError = xrtErrorCreate(XERR_IO, "test.thread", pContext->Code, "thread error");
	if ( pError == NULL ) {
		return 2;
	}
	xrtSetError(pError);
	xrtErrorFree(pError);
	for ( int i = 0; i < TEST_ERROR_ITERATIONS; i++ ) {
		if ( (xrtErrorCode(xrtGetError()) != pContext->Code) ||
			 (xrtErrorKind(xrtGetError()) != XERR_IO) ) {
			xrtClearError();
			return 3;
		}
	}
	xrtClearError();
	return xrtGetError() == NULL ? 0 : 4;
}



/* 验证原生线程与主线程各自拥有独立错误槽。 */
int main(void)
{
	static test_error_allocator AllocatorState;
	xallocator Allocator;
	test_error_thread_context arrContext[TEST_ERROR_THREAD_COUNT];
	testthread arrThread[TEST_ERROR_THREAD_COUNT];
	xerror* pMainError;
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
	Allocator.Alloc = testErrorAlloc;
	Allocator.Realloc = testErrorRealloc;
	Allocator.Free = testErrorFree;
	testRequire(
		xrtSetAllocator(&Allocator),
		"error thread allocator installation failed"
	);

	pMainError = xrtErrorCreate(
		XERR_STATE,
		"test.main",
		99,
		"main error"
	);
	testRequire(pMainError != NULL, "main error creation failed");
	xrtSetError(pMainError);
	xrtErrorFree(pMainError);
	for ( int i = 0; i < TEST_ERROR_THREAD_COUNT; i++ ) {
		arrContext[i].Code = i + 1;
		arrThread[i].Proc = testErrorThreadRun;
		arrThread[i].Data = &arrContext[i];
	}
	testThreadsStart(arrThread, TEST_ERROR_THREAD_COUNT);
	testThreadsJoin(arrThread, TEST_ERROR_THREAD_COUNT);
	for ( int i = 0; i < TEST_ERROR_THREAD_COUNT; i++ ) {
		testRequire(arrThread[i].Result == 0, "thread error slot was polluted");
	}
	testRequire(xrtErrorFind(xrtGetError(), "test.main", 99) != NULL,
		"worker thread changed main error");
	xrtClearError();

	/* 顺序复用同一尺寸类，错误对象和线程缓存都必须在退出时析构。 */
	for ( int i = 0; i < TEST_ERROR_EXIT_ITERATIONS; i++ ) {
		testthread Thread;

		Thread.Proc = testErrorThreadExitRun;
		Thread.Data = NULL;
		testThreadsStart(&Thread, 1);
		testThreadsJoin(&Thread, 1);
		testRequire(Thread.Result == 0, "thread exit error setup failed");
		if ( i == 0 ) {
			iExitBaseline = testErrorAllocatorActiveCount(&AllocatorState);
		}
	}
	testRequire(
		testErrorAllocatorActiveCount(&AllocatorState) == iExitBaseline,
		"thread exit retained dynamic error objects"
	);
	/* 分配器上下文按契约保留到进程退出，供主线程 TLS 析构使用。 */
	printf("[PASS] error-threads\n");
	return 0;
}
