#include <stdio.h>
#include <xrt.h>



/*
 * 范例：concurrency/task_group —— 任务组：统一等待与终态统计
 * ----------------------------------------------------------------
 * 演示 API：
 *   任务组提交多个独立任务
 *   统一等待 + 累计终态（成功/失败计数）
 * 模块宏：XRT_MODULE_TASK
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c $BS}
 *       examples/concurrency/task_group/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   completed: 2, succeeded: 2
 *
 * 结构化并发的最小形态：一组任务一个等待点，
 *   终态统计一次拿全（不用逐个 Future 等）。
 */


/* 用任务组统一等待多个独立 Future，并读取累计终态。 */
int main(void)
{
	xtaskgroup* pGroup = xrtTaskGroupCreate(NULL);
	xfuture* pFirst;
	xfuture* pSecond;
	xpromise* pFirstPromise = xrtPromiseCreate(&pFirst, NULL);
	xpromise* pSecondPromise = xrtPromiseCreate(&pSecond, NULL);
	xtaskgroupstats tStats;

	if ( (pGroup == NULL) || (pFirstPromise == NULL) ||
		(pSecondPromise == NULL) ) {
		return 1;
	}
	if ( !xrtTaskGroupAdd(pGroup, pFirst) ||
		!xrtTaskGroupAdd(pGroup, pSecond) ) {
		return 2;
	}
	(void)xrtPromiseResolve(pFirstPromise, NULL);
	(void)xrtPromiseResolve(pSecondPromise, NULL);
	if ( xrtTaskGroupWait(pGroup) != XWAIT_OK ) {
		return 3;
	}
	(void)xrtTaskGroupGet(pGroup, &tStats);
	printf(
		"completed: %llu, succeeded: %llu\n",
		(unsigned long long)tStats.Completed,
		(unsigned long long)tStats.Succeeded
	);

	xrtPromiseDestroy(pFirstPromise);
	xrtPromiseDestroy(pSecondPromise);
	xrtFutureDestroy(pFirst);
	xrtFutureDestroy(pSecond);
	xrtTaskGroupDestroy(pGroup);
	return 0;
}
