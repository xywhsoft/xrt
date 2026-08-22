#include "../test.h"
#include "../test_thread.h"



#define TEST_REF_THREAD_COUNT 4
#define TEST_REF_ITERATIONS 50000



/* 所有工作线程共享同一个不会归零的引用计数。 */
typedef struct test_ref_context {
	volatile int32* Count;
} test_ref_context;



/* 反复成对增减，验证竞争下不会丢失更新或意外归零。 */
static int testRefRun(ptr pData)
{
	test_ref_context* pContext = (test_ref_context*)pData;

	for ( int i = 0; i < TEST_REF_ITERATIONS; i++ ) {
		if ( xrtRefRetain(pContext->Count) < 2 ) {
			return 1;
		}
		if ( xrtRefRelease(pContext->Count) < 1 ) {
			return 2;
		}
	}
	return 0;
}



/* 启动并发工作线程并验证最终引用计数保持不变。 */
int main(void)
{
	volatile int32 iCount = 1;
	test_ref_context arrContext[TEST_REF_THREAD_COUNT];
	testthread arrThread[TEST_REF_THREAD_COUNT];

	memset(arrContext, 0, sizeof(arrContext));
	for ( int i = 0; i < TEST_REF_THREAD_COUNT; i++ ) {
		arrContext[i].Count = &iCount;
		arrThread[i].Proc = testRefRun;
		arrThread[i].Data = &arrContext[i];
	}
	testThreadsStart(arrThread, TEST_REF_THREAD_COUNT);
	testThreadsJoin(arrThread, TEST_REF_THREAD_COUNT);
	for ( int i = 0; i < TEST_REF_THREAD_COUNT; i++ ) {
		testRequire(arrThread[i].Result == 0, "reference worker failed");
	}
	testRequire(iCount == 1, "concurrent reference count drifted");
	printf("[PASS] ref-threads\n");
	return 0;
}
