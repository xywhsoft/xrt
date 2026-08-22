#include "../test.h"
#include "../test_thread.h"



#define TEST_XID_THREADS 8u
#define TEST_XID_PER_THREAD 2000u



/* 每个线程写入互不重叠的批量输出区间。 */
typedef struct testxidthreadcontext {
	xid* Values;
	size_t Count;
} testxidthreadcontext;



/* 并发调用同一个无全局可变状态的生成入口。 */
static int testXidThreadRun(ptr pData)
{
	testxidthreadcontext* pContext = (testxidthreadcontext*)pData;

	return xrtXidMakeMany(pContext->Values, pContext->Count) ? 0 : 1;
}



/* 为全局碰撞检查比较 XID。 */
static int testXidThreadSort(const void* pLeft, const void* pRight)
{
	return xrtXidCompare((const xid*)pLeft, (const xid*)pRight);
}



/* 验证安全随机与时间生成路径可并发执行且整批无碰撞。 */
int main(void)
{
	static xid arrValues[TEST_XID_THREADS * TEST_XID_PER_THREAD];
	testxidthreadcontext arrContexts[TEST_XID_THREADS];
	testthread arrThreads[TEST_XID_THREADS];

	for ( size_t i = 0; i < TEST_XID_THREADS; i++ ) {
		arrContexts[i].Values =
			arrValues + (i * TEST_XID_PER_THREAD);
		arrContexts[i].Count = TEST_XID_PER_THREAD;
		arrThreads[i].Proc = testXidThreadRun;
		arrThreads[i].Data = &arrContexts[i];
	}
	testThreadsStart(arrThreads, TEST_XID_THREADS);
	testThreadsJoin(arrThreads, TEST_XID_THREADS);
	for ( size_t i = 0; i < TEST_XID_THREADS; i++ ) {
		testRequire(
			arrThreads[i].Result == 0,
			"concurrent XID generation failed"
		);
	}
	qsort(
		arrValues,
		TEST_XID_THREADS * TEST_XID_PER_THREAD,
		sizeof(arrValues[0]),
		testXidThreadSort
	);
	for ( size_t i = 1;
		i < (TEST_XID_THREADS * TEST_XID_PER_THREAD);
		i++ ) {
		testRequire(
			xrtXidCompare(&arrValues[i - 1u], &arrValues[i]) < 0,
			"concurrent XID collision detected"
		);
	}
	printf("[PASS] XID threads\n");
	return 0;
}
