#include <stdio.h>
#include <xrt.h>



/* 协程任务在不阻塞原生线程的情况下等待后返回借用值。 */
static xtaskoutcome groupedCoroutine(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	if ( (xrtCoSleep(1000) == XWAIT_CANCELLED) ||
		xrtCancelRequested(pCancel) ) {
		return XTASK_CANCELLED;
	}
	pResult->Value = pData;
	return XTASK_SUCCESS;
}



/*
 * 范例：concurrency/task_group_coroutine —— 协程任务组
 * ----------------------------------------------------------------
 * 演示 API：
 *   协程任务原子纳入组 + 统一运行等待
 * 模块宏：XRT_MODULE_TASK（依赖 COROUTINE）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c $BS}
 *       examples/concurrency/task_group_coroutine/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   value = 47
 *
 * 与 task_group 同契约，工作形态换成协程——
 *   轻量并发（千级）场景的组管理。
 */


/* 把协程任务原子纳入组，再统一运行和等待。 */
int main(void)
{
	xcosched* pSched = xrtCoSchedCreate();
	xtaskgroup* pGroup = xrtTaskGroupCreate(NULL);
	xfuture* pFuture = NULL;
	int iValue = 47;
	int iResult = 1;

	if ( (pSched == NULL) || (pGroup == NULL) ) {
		goto cleanup;
	}
	pFuture = xrtTaskGroupCo(
		pGroup,
		pSched,
		groupedCoroutine,
		&iValue,
		NULL,
		0
	);
	if (
		(pFuture != NULL) && xrtCoSchedRun(pSched) &&
		(xrtTaskGroupWait(pGroup) == XWAIT_OK)
	) {
		printf("value = %d\n", *(int*)xrtFutureValue(pFuture));
		iResult = 0;
	}

cleanup:
	xrtFutureDestroy(pFuture);
	xrtTaskGroupDestroy(pGroup);
	(void)xrtCoSchedDestroy(pSched);
	(void)xrtCoThreadDetach();
	return iResult;
}
