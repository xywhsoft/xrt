#include <stdio.h>
#include <xrt.h>



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
