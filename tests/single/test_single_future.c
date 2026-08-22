#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件包含可裁剪的 Future/Promise 核心。 */
int main(void)
{
	xfuture* pFuture;
	xpromise* pPromise;
	int iValue = 5;
	int iResult = 1;

	pPromise = xrtPromiseCreate(&pFuture, NULL);
	if ( (pPromise != NULL) && xrtPromiseResolve(pPromise, &iValue) &&
		(xrtFutureWait(pFuture) == XWAIT_OK) &&
		(xrtFutureValue(pFuture) == &iValue) ) {
		iResult = 0;
	}
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pFuture);
	return iResult;
}
