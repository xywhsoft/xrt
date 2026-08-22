#define XRUNTIME_IMPLEMENTATION
#include "../../single/xruntime.h"



/* 验证单头文件中的 Future Value 桥接及所有权转移。 */
int main(void)
{
	xfuture* pFuture = NULL;
	xpromise* pPromise = xrtPromiseCreate(&pFuture, NULL);
	xvalue* pValue = xrtValueFutureTake(&pFuture);
	int iAnswer = 42;
	int iResult = 0;

	if (
		(pPromise == NULL) || (pValue == NULL) || (pFuture != NULL) ||
		!xrtPromiseResolve(pPromise, &iAnswer) ||
		(xrtFutureValue(xrtValueGetFuture(pValue)) != &iAnswer)
	) {
		iResult = 1;
	}
	xrtValueRelease(pValue);
	xrtPromiseDestroy(pPromise);
	return iResult;
}
