#include "../test.h"



/* 立即返回，用于高频验证等待后的线程对象回收边界。 */
static int32 testThreadMemoryStatsImmediate(ptr pData)
{
	(void)pData;
	return 0;
}



/* 等待完成后立即销毁对象，验证内部运行引用已经先于终态发布。 */
int main(void)
{
	xmemstats Before;
	xmemstats After;
	xthread* pThread;

	xrtMemStatsEnable(true);
	xrtMemStatsGet(&Before);
	for ( size_t i = 0; i < 128; i++ ) {
		pThread = xrtThreadCreate(
			testThreadMemoryStatsImmediate,
			NULL,
			0
		);
		testRequire(pThread != NULL, "cleanup thread create failed");
		testRequire(
			xrtThreadWait(pThread) == XWAIT_OK,
			"cleanup thread wait failed"
		);
		xrtThreadDestroy(pThread);
	}
	xrtMemStatsGet(&After);
	testRequire(
		(After.BlockAllocCalls - Before.BlockAllocCalls) ==
		(After.BlockFreeCalls - Before.BlockFreeCalls),
		"thread wait returned before internal reference cleanup"
	);
	printf("[PASS] thread memory stats\n");
	return 0;
}
