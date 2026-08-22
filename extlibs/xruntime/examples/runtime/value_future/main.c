#include <stdio.h>
#include <xruntime.h>



/* 展示 Future 消费端引用移交给动态 Value。 */
int main(void)
{
	xfuture* pFuture = NULL;
	xpromise* pPromise = xrtPromiseCreate(&pFuture, NULL);
	xvalue* pValue = xrtValueFutureTake(&pFuture);
	int iAnswer = 42;
	int* pResult;
	int iExit = 0;

	if (
		(pPromise == NULL) || (pValue == NULL) ||
		!xrtPromiseResolve(pPromise, &iAnswer)
	) {
		iExit = 1;
	} else {
		pResult = (int*)xrtFutureValue(xrtValueGetFuture(pValue));
		if ( pResult == NULL ) {
			iExit = 1;
		} else {
			printf("answer=%d\n", *pResult);
		}
	}
	xrtValueRelease(pValue);
	xrtPromiseDestroy(pPromise);
	return iExit;
}
