#include <stdio.h>

#include <xrt.h>



/* 示例任务计算整数平方，并把调用方提供的结果槽作为借用值返回。 */
typedef struct squaretask {
	int Input;
	int Output;
} squaretask;



/* 任务通过取消令牌观察协作停止，通过结果结构返回成功值。 */
static xtaskoutcome squareRun(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	squaretask* pTask = (squaretask*)pData;

	if ( xrtCancelRequested(pCancel) ) {
		return XTASK_CANCELLED;
	}
	pTask->Output = pTask->Input * pTask->Input;
	pResult->Value = &pTask->Output;
	return XTASK_SUCCESS;
}



/* 创建任务池、提交任务、读取 Future，并按所有权顺序释放对象。 */
int main(void)
{
	xtaskpoolconfig tConfig = { 2, 32, 0 };
	squaretask tTask = { 12, 0 };
	xtaskpool* pPool = xrtTaskPoolCreate(&tConfig);
	xfuture* pFuture;
	int iResult = 1;

	if ( pPool == NULL ) {
		return 1;
	}
	pFuture = xrtTaskSubmit(pPool, squareRun, &tTask, NULL);
	if (
		(pFuture != NULL) &&
		(xrtFutureWaitFor(pFuture, UINT64_C(2000000)) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_RESOLVED)
	) {
		printf("square = %d\n", *(int*)xrtFutureValue(pFuture));
		iResult = 0;
	}
	xrtFutureDestroy(pFuture);
	(void)xrtTaskPoolDestroy(pPool);
	return iResult;
}
