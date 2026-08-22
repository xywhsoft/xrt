#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件延续把输入整数转换成独立输出值。 */
static void testSingleFutureContinue(
	const xfutureresult* pInput,
	xpromise* pOutput,
	ptr pData
)
{
	int* pValue = (int*)pData;

	*pValue = *(int*)pInput->Value + 1;
	(void)xrtPromiseResolve(pOutput, pValue);
}



/* 验证 Future continuation 可独立裁剪并由单头文件完整实现。 */
int main(void)
{
	xfuture* pSource;
	xfuture* pNext;
	xpromise* pPromise = xrtPromiseCreate(&pSource, NULL);
	int iInput = 40;
	int iOutput = 0;
	int iResult = 1;

	if ( pPromise == NULL ) {
		return 1;
	}
	pNext = xrtFutureThen(
		pSource,
		testSingleFutureContinue,
		&iOutput
	);
	if ( (pNext != NULL) && xrtPromiseResolve(pPromise, &iInput) &&
		(xrtFutureValue(pNext) == &iOutput) && (iOutput == 41) ) {
		iResult = 0;
	}
	xrtFutureDestroy(pNext);
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pSource);
	return iResult;
}
