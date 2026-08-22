#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件公开 Future 桥及其最小装配生命周期。 */
int main(void)
{
	xfuturebridge Bridge;
	xfuture* pFuture;
	xpromise* pPromise;
	int iResult = 1;

	pFuture = xrtFutureBridgeCreate(&Bridge, NULL);
	pPromise = xrtFutureBridgePromise(&Bridge);
	if ( (pFuture != NULL) && (pPromise != NULL) &&
		xrtFutureBridgeFail(&Bridge) &&
		!xrtFutureBridgeWait(&Bridge) ) {
		iResult = 0;
	}
	xrtFutureBridgeUnwatch(&Bridge);
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pFuture);
	return iResult;
}
