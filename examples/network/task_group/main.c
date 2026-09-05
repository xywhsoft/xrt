#include <stdio.h>
#include <xrt.h>



/* 在亲和 Worker 上返回一个借用整数。 */
static xtaskoutcome buildGroupValue(
	xnetworker* pWorker,
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	(void)pWorker;
	(void)pCancel;
	pResult->Value = pData;
	return XTASK_SUCCESS;
}



/*
 * 范例：network/task_group —— 任务组：结构化并发作用域
 * ----------------------------------------------------------------
 * 演示 API：
 *   网络任务组   立即/延迟任务共享一个作用域
 * 模块宏：XRT_MODULE_TASK_NET
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/task_group/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   values=7,11
 *
 * 结构化并发：组内任务全部完成后作用域才结束——
 *   取消沿组传播、结果统一收集（values 一次拿到），
 *   没有"泄漏的任务"这种悬空生命周期。
 */


/* 演示立即和延迟网络任务共享一个结构化作用域。 */
int main(void)
{
	xnetengine* pEngine = xrtNetEngineCreate(NULL);
	xtaskgroup* pGroup = xrtTaskGroupCreate(NULL);
	xfuture* pFirst = NULL;
	xfuture* pSecond = NULL;
	int iFirst = 7;
	int iSecond = 11;
	int iResult = 1;

	if ( (pEngine != NULL) && (pGroup != NULL) &&
		xrtNetEngineStart(pEngine) ) {
		pFirst = xrtTaskGroupNet(
			pGroup,
			pEngine,
			0,
			buildGroupValue,
			&iFirst,
			NULL
		);
		pSecond = xrtTaskGroupNetAfter(
			pGroup,
			pEngine,
			0,
			buildGroupValue,
			&iSecond,
			NULL,
			1000u
		);
	}
	if ( (pFirst != NULL) && (pSecond != NULL) &&
		(xrtTaskGroupWaitFor(pGroup, 3000000u) == XWAIT_OK) ) {
		printf(
			"values=%d,%d\n",
			*(int*)xrtFutureValue(pFirst),
			*(int*)xrtFutureValue(pSecond)
		);
		iResult = 0;
	}
	xrtFutureDestroy(pFirst);
	xrtFutureDestroy(pSecond);
	xrtTaskGroupDestroy(pGroup);
	if ( !xrtNetEngineDestroy(pEngine) ) {
		iResult = 1;
	}
	return iResult;
}
