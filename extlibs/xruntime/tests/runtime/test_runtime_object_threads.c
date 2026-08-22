#include "../test.h"
#include "../test_thread.h"



#define TEST_WEAK_THREAD_COUNT 4
#define TEST_WEAK_ITERATIONS 50000



typedef struct testweakcontext {
	const xrtweak* Weak;
	int64 Expected;
	volatile int32* Ready;
	volatile int32* Start;
	volatile int32* Locked;
} testweakcontext;



/* 原子读取测试线程之间共享的 32 位计数。 */
static int32 testWeakAtomicLoad(const volatile int32* pValue)
{
	#if defined(_WIN32) || defined(_WIN64)
		return (int32)InterlockedCompareExchange(
			(volatile LONG*)pValue, 0, 0);
	#elif defined(__GNUC__) || defined(__clang__)
		return (int32)__sync_val_compare_and_swap(
			(volatile int32*)pValue, 0, 0);
	#else
		return *pValue;
	#endif
}



/* 原子增加测试线程之间共享的 32 位计数。 */
static void testWeakAtomicIncrement(volatile int32* pValue)
{
	#if defined(_WIN32) || defined(_WIN64)
		(void)InterlockedIncrement((volatile LONG*)pValue);
	#elif defined(__GNUC__) || defined(__clang__)
		(void)__sync_add_and_fetch(pValue, 1);
	#else
		(*pValue)++;
	#endif
}



/* 原子发布测试线程启动信号。 */
static void testWeakAtomicStart(volatile int32* pValue)
{
	#if defined(_WIN32) || defined(_WIN64)
		(void)InterlockedExchange((volatile LONG*)pValue, 1);
	#elif defined(__GNUC__) || defined(__clang__)
		(void)__sync_lock_test_and_set(pValue, 1);
	#else
		*pValue = 1;
	#endif
}



/* 并发复制、查询和提升同一控制块上的弱引用。 */
static int testWeakRun(ptr pData)
{
	testweakcontext* pContext = (testweakcontext*)pData;

	testWeakAtomicIncrement(pContext->Ready);
	while ( testWeakAtomicLoad(pContext->Start) == 0 ) {
	}
	for ( int i = 0; i < TEST_WEAK_ITERATIONS; i++ ) {
		xrtweak Local = { 0 };
		xrtobject* pObject;

		if ( !xrtWeakCopy(&Local, pContext->Weak) ) {
			return 1;
		}
		(void)xrtWeakExpired(&Local);
		pObject = xrtWeakLock(&Local);
		xrtWeakUnit(&Local);
		if ( pObject == NULL ) {
			continue;
		}
		testWeakAtomicIncrement(pContext->Locked);
		if ( *(const int64*)xrtObjectConstData(pObject) != pContext->Expected ) {
			xrtObjectUnref(pObject);
			return 2;
		}
		xrtObjectUnref(pObject);
	}
	return 0;
}



/* 验证弱引用计数与最后一个强引用释放之间的竞争。 */
int main(void)
{
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.runtime.Concurrent")),
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("Concurrent"),
		.AbiName = XRT_STR_INIT("tests.runtime.Concurrent"),
		.Size = sizeof(ptr),
		.Align = TEST_ALIGNOF(ptr),
		.InstanceSize = sizeof(int64),
		.InstanceAlign = TEST_ALIGNOF(int64)
	};
	xrtweak Weak = { 0 };
	xrtobject* pObject = xrtObjectCreate(&Type);
	testweakcontext arrContext[TEST_WEAK_THREAD_COUNT];
	testthread arrThreads[TEST_WEAK_THREAD_COUNT];
	volatile int32 iReady = 0;
	volatile int32 iStart = 0;
	volatile int32 iLocked = 0;

	testRequire(pObject != NULL, "concurrent object creation failed");
	*(int64*)xrtObjectData(pObject) = 99;
	testRequire(xrtWeakInit(&Weak, pObject), "concurrent weak init failed");
	for ( int i = 0; i < TEST_WEAK_THREAD_COUNT; i++ ) {
		arrContext[i].Weak = &Weak;
		arrContext[i].Expected = 99;
		arrContext[i].Ready = &iReady;
		arrContext[i].Start = &iStart;
		arrContext[i].Locked = &iLocked;
		arrThreads[i].Proc = testWeakRun;
		arrThreads[i].Data = &arrContext[i];
	}
	testThreadsStart(arrThreads, TEST_WEAK_THREAD_COUNT);
	while ( testWeakAtomicLoad(&iReady) != TEST_WEAK_THREAD_COUNT ) {
	}
	testWeakAtomicStart(&iStart);
	while ( testWeakAtomicLoad(&iLocked) < TEST_WEAK_THREAD_COUNT ) {
	}

	/* 工作者与最后一个所有者并发，Lock 只能返回完整对象或空。 */
	xrtObjectUnref(pObject);
	testThreadsJoin(arrThreads, TEST_WEAK_THREAD_COUNT);
	for ( int i = 0; i < TEST_WEAK_THREAD_COUNT; i++ ) {
		testRequire(arrThreads[i].Result == 0, "concurrent weak worker failed");
	}
	testRequire(xrtWeakExpired(&Weak), "concurrent weak did not expire");
	testRequire(xrtWeakLock(&Weak) == NULL, "expired weak became live");
	xrtWeakUnit(&Weak);
	printf("[PASS] runtime object threads\n");
	return 0;
}
