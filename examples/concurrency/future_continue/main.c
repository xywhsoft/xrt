#include <stdio.h>
#include <xrt.h>



/* 延续读取源结果，并通过输出 Promise 发布转换后的值。 */
static void addFive(
	const xfutureresult* pInput,
	xpromise* pOutput,
	ptr pData
)
{
	int* pValue = (int*)pData;

	*pValue = *(int*)pInput->Value + 5;
	(void)xrtPromiseResolve(pOutput, pValue);
}



/* 串联一个短成功延续并读取最终结果。 */
int main(void)
{
	xfuture* pSource;
	xfuture* pNext;
	xpromise* pPromise = xrtPromiseCreate(&pSource, NULL);
	int iSource = 100;
	int iResult = 0;

	if ( pPromise == NULL ) {
		return 1;
	}
	pNext = xrtFutureThen(pSource, addFive, &iResult);
	if ( pNext == NULL ) {
		xrtPromiseDestroy(pPromise);
		xrtFutureDestroy(pSource);
		return 2;
	}
	(void)xrtPromiseResolve(pPromise, &iSource);
	printf("result: %d\n", *(int*)xrtFutureValue(pNext));

	xrtFutureDestroy(pNext);
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pSource);
	return 0;
}
