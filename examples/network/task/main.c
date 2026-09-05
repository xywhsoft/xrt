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
 * 模块宏：XRT_MODULE_TASK_NET
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/task/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   worker=0
 *   value=42
 *
 * 任务与连接同 Worker：回调里访问本 Worker 的资源
 *   无需锁。返回值经 Future 交付（value=42）——
 *   网络层的轻量计算卸载入口。
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
	return xrtNetEngineDestroy(pEngine) ? 0 : 1;
}
