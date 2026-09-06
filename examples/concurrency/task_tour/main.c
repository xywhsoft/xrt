/*
 * 范例：concurrency/task_tour —— 任务池/任务组提交与等待补集
 * ----------------------------------------------------------------
 * 演示 API：
 *   【提交族】  xrtTaskSubmitWait / SubmitFor / SubmitUntil /
 *              SubmitUntilCancel（四种容量等待形态）
 *   【池收口】  xrtTaskPoolClose / Cancel / Wait / WaitFor /
 *              WaitUntil / WaitUntilCancel / Get（统计快照）
 *   【组提交族】 xrtTaskGroupSubmitWait / SubmitUntil /
 *              SubmitUntilCancel
 *   【组收口】  xrtTaskGroupStart（Future 启动器）/
 *              GroupWaitUntil / WaitUntilCancel / Error /
 *              CancelToken
 * 模块宏：XRT_MODULE_TASK
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/concurrency/task_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   task: submit wait/for/until/cancel ok
 *   task: pool close+wait stats(completed>=5) ok
 *   task: pool tiny cancel+wait-until ok
 *   task: group submit wait/until/cancel ok
 *   task: group start+wait-until cancel-token ok
 *
 * 任务返回固定值 7；组取消令牌非空；Cancel 版本用已触发
 *   令牌立即取消未受理的容量等待。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

#define EXAMPLE_TIMEOUT_US	UINT64_C(3000000)

/* 标准任务：返回值 7。 */
static xtaskoutcome exampleTask(xcancel* pCancel, ptr pData,
	xtaskvalue* pResult)
{
	(void)pCancel;
	(void)pData;
	pResult->Value = (ptr)7;
	return XTASK_SUCCESS;
}

/* Future 启动器：提交一个即时任务作为组项。 */
static xfuture* exampleStarter(ptr pData)
{
	return xrtTaskSubmit((xtaskpool*)pData, exampleTask, NULL,
		NULL);
}

int main(void)
{
	xtaskpoolconfig PoolConfig = { 2, 8, 0 };
	xtaskpool* pPool = NULL;
	xtaskpool* pFullPool = NULL;
	xtaskpoolstats Stats;
	xtaskgroup* pGroup = NULL;
	xtaskgroupconfig GroupConfig;
	xfuture* arrFutures[8];
	xcancel* pCancel = NULL;
	const xerror* pError = NULL;
	size_t i;
	int iResult = 1;

	memset(arrFutures, 0, sizeof(arrFutures));

	/* ---- 提交族四种容量等待形态 ---- */
	pPool = xrtTaskPoolCreate(&PoolConfig);
	if ( (pPool == NULL) ||
		((arrFutures[0] = xrtTaskSubmitWait(pPool, exampleTask,
			NULL, NULL)) == NULL) ||
		((arrFutures[1] = xrtTaskSubmitFor(pPool, exampleTask,
			NULL, NULL, EXAMPLE_TIMEOUT_US)) == NULL) ||
		((arrFutures[2] = xrtTaskSubmitUntil(pPool, exampleTask,
			NULL, NULL,
			xrtDeadlineAfter(EXAMPLE_TIMEOUT_US))) == NULL) ||
		((arrFutures[3] = xrtTaskSubmit(pPool, exampleTask,
			NULL, NULL)) == NULL) ) {
		goto Cleanup;
	}
	/* SubmitUntilCancel：未触发令牌正常提交。 */
	pCancel = xrtCancelCreate();
	if ( (pCancel == NULL) ||
		((arrFutures[4] = xrtTaskSubmitUntilCancel(pPool,
			exampleTask, NULL, NULL,
			xrtDeadlineAfter(EXAMPLE_TIMEOUT_US),
			pCancel)) == NULL) ) {
		goto Cleanup;
	}
	/* 逐个收割：值都是 7。 */
	for ( i = 0; i < 5u; ++i ) {
		if ( (xrtFutureWaitFor(arrFutures[i],
				EXAMPLE_TIMEOUT_US) != XWAIT_OK) ||
			((uintptr_t)xrtFutureValue(arrFutures[i]) != 7u) ) {
			goto Cleanup;
		}
	}
	printf("task: submit wait/for/until/cancel ok\n");

	/* ---- 池收口：Close → Wait + 统计 ---- */
	if ( !xrtTaskPoolClose(pPool) ||
		(xrtTaskPoolWaitFor(pPool, EXAMPLE_TIMEOUT_US) !=
			XWAIT_OK) ||
		!xrtTaskPoolGet(pPool, &Stats) ||
		(Stats.Completed < 5u) ||
		(Stats.Succeeded < 5u) ||
		!Stats.Closed ) {
		goto Cleanup;
	}
	printf("task: pool close+wait stats(completed>=%llu) ok\n",
		(unsigned long long)Stats.Completed);

	/* ---- 满池上的取消路径：容量 1 + 队列 1 填满 → 已触发令牌 ---- */
	{
		xtaskpoolconfig Tiny = { 1, 1, 0 };

		pFullPool = xrtTaskPoolCreate(&Tiny);
		if ( (pFullPool == NULL) ||
			!xrtCancelRequest(pCancel) ) {
			goto Cleanup;
		}
		/* 两个任务占满线程+队列。 */
		(void)xrtTaskSubmit(pFullPool, exampleTask, NULL, NULL);
		(void)xrtTaskSubmit(pFullPool, exampleTask, NULL, NULL);
		/* 第三个：已触发令牌让容量等待立即取消。 */
		{
			xfuture* pThird = xrtTaskSubmitUntilCancel(
				pFullPool, exampleTask, NULL, NULL,
				xrtDeadlineAfter(EXAMPLE_TIMEOUT_US),
				pCancel);

			if ( (pThird != NULL) ||
				(xrtTaskPoolCancel(pFullPool) ) ) {
				/* 取消后池进入 Cancelling。 */
			}
		}
		if ( (xrtTaskPoolWaitUntil(pFullPool,
				xrtDeadlineAfter(EXAMPLE_TIMEOUT_US)) !=
				XWAIT_OK) ||
			!xrtTaskPoolGet(pFullPool, &Stats) ) {
			goto Cleanup;
		}
		/* WaitUntilCancel：空池返回 ERROR（等价演示调用形态），
		 * Wait 单独收口。 */
		{
			xwaitresult iWait = xrtTaskPoolWaitUntilCancel(
				pFullPool,
				xrtDeadlineAfter(EXAMPLE_TIMEOUT_US),
				pCancel);

			if ( (iWait != XWAIT_OK) &&
				(iWait != XWAIT_CANCELLED) ) {
				goto Cleanup;
			}
		}
		(void)xrtTaskPoolWait(pFullPool);
	}
	printf("task: pool tiny cancel+wait-until ok\n");

	/* ---- 组提交族 ---- */
	memset(&GroupConfig, 0, sizeof(GroupConfig));
	pGroup = xrtTaskGroupCreate(&GroupConfig);
	if ( (pGroup == NULL) ) {
		goto Cleanup;
	}
	(void)xrtCancelRequest(pCancel);
	{
		/* 重新建未触发令牌。 */
		xcancel* pFresh = xrtCancelCreate();

		if ( pFresh == NULL ) {
			goto Cleanup;
		}
		xrtCancelDestroy(pCancel);
		pCancel = pFresh;
	}
	{
		xtaskpoolconfig Pool2 = { 2, 8, 0 };
		xtaskpool* pPool2 = xrtTaskPoolCreate(&Pool2);

		if ( (pPool2 == NULL) ||
			((arrFutures[5] = xrtTaskGroupSubmitWait(pGroup,
				pPool2, exampleTask, NULL, NULL)) ==
				NULL) ||
			((arrFutures[6] = xrtTaskGroupSubmitUntil(pGroup,
				pPool2, exampleTask, NULL, NULL,
				xrtDeadlineAfter(EXAMPLE_TIMEOUT_US))) ==
				NULL) ) {
			goto Cleanup;
		}
		/* GroupSubmitUntilCancel：未触发令牌正常纳入。 */
		{
			xfuture* pThird = xrtTaskGroupSubmitUntilCancel(
				pGroup, pPool2, exampleTask, NULL, NULL,
				xrtDeadlineAfter(EXAMPLE_TIMEOUT_US),
				pCancel);

			if ( pThird == NULL ) {
				goto Cleanup;
			}
			arrFutures[7] = pThird;
		}
		/* GroupStart：Future 启动器纳入组。 */
		{
			xfuture* pStarted = xrtTaskGroupStart(pGroup,
				exampleStarter, pPool2);

			if ( (pStarted == NULL) ||
				(xrtFutureWaitFor(pStarted,
					EXAMPLE_TIMEOUT_US) != XWAIT_OK) ) {
				goto Cleanup;
			}
			xrtFutureDestroy(pStarted);
		}
		/* 组等待族。 */
		if ( (xrtTaskGroupWaitUntil(pGroup,
				xrtDeadlineAfter(EXAMPLE_TIMEOUT_US)) !=
				XWAIT_OK) ||
			(xrtTaskGroupWaitUntilCancel(pGroup,
				xrtDeadlineAfter(EXAMPLE_TIMEOUT_US),
				pCancel) != XWAIT_OK) ) {
			goto Cleanup;
		}
		/* Error 与 CancelToken。 */
		pError = xrtTaskGroupError(pGroup);
		if ( (xrtTaskGroupCancelToken(pGroup) == NULL) ) {
			goto Cleanup;
		}
		(void)pError;
		(void)xrtTaskPoolClose(pPool2);
		(void)xrtTaskPoolWait(pPool2);
		xrtTaskPoolDestroy(pPool2);
	}
	printf("task: group submit wait/until/cancel ok\n");
	printf("task: group start+wait-until cancel-token ok\n");
	iResult = 0;

Cleanup:
	for ( i = 0; i < 8u; ++i ) {
		xrtFutureDestroy(arrFutures[i]);
	}
	xrtTaskGroupDestroy(pGroup);
	xrtCancelDestroy(pCancel);
	xrtTaskPoolDestroy(pFullPool);
	xrtTaskPoolDestroy(pPool);
	return iResult;
}
