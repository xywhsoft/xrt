#include <stdio.h>
#include <xrt.h>



/* 在 Engine Worker 上生成一个轻量结果。 */
static xtaskoutcome buildValue(
	xnetworker* pWorker,
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	(void)pCancel;
	printf("worker=%u\n", xrtNetWorkerIndex(pWorker));
	pResult->Value = pData;
	return XTASK_SUCCESS;
}



/*
 * 范例：network/task —— 网络任务：Worker 上执行并取结果
 * ----------------------------------------------------------------
 * 演示 API：
 *   网络任务提交（亲和 Worker 执行）
 *   统一 Future 等待立即任务
 *   xrtTaskNetAfter / Until（延迟与截止时间提交）
 *   xrtTaskGroupNetUntil（延迟任务原子纳入任务组）
 * 模块宏：XRT_MODULE_TASK_NET
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/task/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   worker=0
 *   value=42
 *   worker=0
 *   after: value=42
 *   worker=0
 *   until: value=42
 *   worker=0
 *   group-until: done
 */


/* 演示立即网络任务与统一 Future 等待。 */
int main(void)
{
	xnetengine* pEngine = xrtNetEngineCreate(NULL);
	xfuture* pFuture;
	int iValue = 42;

	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		return 1;
	}
	pFuture = xrtTaskNet(
		pEngine,
		0,
		buildValue,
		&iValue,
		NULL
	);
	if ( (pFuture == NULL) ||
		(xrtFutureWaitFor(pFuture, 3000000u) != XWAIT_OK) ||
		(xrtFutureState(pFuture) != XFUTURE_RESOLVED) ) {
		xrtFutureDestroy(pFuture);
		(void)xrtNetEngineDestroy(pEngine);
		return 1;
	}
	printf("value=%d\n", *(int*)xrtFutureValue(pFuture));
	xrtFutureDestroy(pFuture);

	/* ---- After / Until：延迟与截止时间两种提交形态。 ---- */
	pFuture = xrtTaskNetAfter(pEngine, 0, buildValue, &iValue,
		NULL, 0u);
	if ( (pFuture == NULL) ||
		(xrtFutureWaitFor(pFuture, 3000000u) != XWAIT_OK) ) {
		xrtFutureDestroy(pFuture);
		(void)xrtNetEngineDestroy(pEngine);
		return 2;
	}
	printf("after: value=%d\n", *(int*)xrtFutureValue(pFuture));
	xrtFutureDestroy(pFuture);
	pFuture = xrtTaskNetUntil(pEngine, 0, buildValue, &iValue,
		NULL, xrtDeadlineAfter(0u));
	if ( (pFuture == NULL) ||
		(xrtFutureWaitFor(pFuture, 3000000u) != XWAIT_OK) ) {
		xrtFutureDestroy(pFuture);
		(void)xrtNetEngineDestroy(pEngine);
		return 3;
	}
	printf("until: value=%d\n", *(int*)xrtFutureValue(pFuture));
	xrtFutureDestroy(pFuture);

	/* ---- GroupNetUntil：延迟任务原子纳入任务组。 ---- */
	{
		xtaskgroup* pGroup;

		pGroup = xrtTaskGroupCreate(NULL);
		if ( (pGroup == NULL) ||
			(xrtTaskGroupNetUntil(pGroup, pEngine, 0,
				buildValue, &iValue, NULL,
				xrtDeadlineAfter(0u)) == NULL) ||
			!xrtTaskGroupClose(pGroup) ||
			(xrtTaskGroupWaitFor(pGroup, 3000000u) !=
				XWAIT_OK) ) {
			xrtTaskGroupDestroy(pGroup);
			(void)xrtNetEngineDestroy(pEngine);
			return 4;
		}
		printf("group-until: done\n");
		xrtTaskGroupDestroy(pGroup);
	}
	return xrtNetEngineDestroy(pEngine) ? 0 : 1;
}
