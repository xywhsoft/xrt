#include <stdio.h>
#include <xrt.h>



/* Future/Promise 基础示例直接完成并读取一个借用值。 */
int main(void)
{
	xfuture* pFuture;
	xpromise* pPromise;
	int iValue = 42;

	pPromise = xrtPromiseCreate(&pFuture, NULL);
	if ( (pPromise == NULL) || !xrtPromiseResolve(pPromise, &iValue) ) {
		return 1;
	}
	if ( xrtFutureWait(pFuture) != XWAIT_OK ) {
		return 2;
	}
	printf("future value: %d\n", *(int*)xrtFutureValue(pFuture));
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pFuture);
	return 0;
}
