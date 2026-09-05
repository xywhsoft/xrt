#include <stdio.h>
#include <xrt.h>



/* 线程池任务返回输入整数的借用地址。 */
static xtaskoutcome groupedWork(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	if ( xrtCancelRequested(pCancel) ) {
		return XTASK_CANCELLED;
	}
	pResult->Value = pData;
	return XTASK_SUCCESS;
}



/*
 * 范例：concurrency/task_group_pool —— 线程池任务 × 任务组
 * ----------------------------------------------------------------
 * 演示 API：
 *   有界线程池任务原子纳入任务组
 *   统一提交、等待、按序取结果
 * 模块宏：XRT_MODULE_TASK
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c $BS}
 *       examples/concurrency/task_group_pool/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   value[0] = 11
 *   value[1] = 22
 *   value[2] = 33
 *
 * 池（执行资源）+ 组（结构化作用域）正交组合：
 *   三个池任务一次提交一个等待点，结果按下标对号。
 */


/* 用一个结构化任务组原子提交并等待多个有界线程池任务。 */
int main(void)
{
	xtaskpoolconfig tConfig = { 2, 16, 0 };
	xtaskpool* pPool = xrtTaskPoolCreate(&tConfig);
	xtaskgroup* pGroup = xrtTaskGroupCreate(NULL);
	xfuture* arrFuture[3] = { NULL, NULL, NULL };
	int arrValue[3] = { 11, 22, 33 };
	int iResult = 1;

	if ( (pPool == NULL) || (pGroup == NULL) ) {
		goto cleanup;
	}
	for ( size_t i = 0; i < 3; i++ ) {
		arrFuture[i] = xrtTaskGroupSubmitFor(
			pGroup,
			pPool,
			groupedWork,
			&arrValue[i],
			NULL,
			UINT64_C(2000000)
		);
		if ( arrFuture[i] == NULL ) {
			(void)xrtTaskGroupCancel(pGroup);
			goto cleanup;
		}
	}
	if ( xrtTaskGroupWait(pGroup) != XWAIT_OK ) {
		goto cleanup;
	}
	for ( size_t i = 0; i < 3; i++ ) {
		printf("value[%u] = %d\n", (unsigned)i,
			*(int*)xrtFutureValue(arrFuture[i]));
	}
	iResult = 0;

cleanup:
	for ( size_t i = 0; i < 3; i++ ) {
		xrtFutureDestroy(arrFuture[i]);
	}
	xrtTaskGroupDestroy(pGroup);
	(void)xrtTaskPoolDestroy(pPool);
	return iResult;
}
