#include <stdio.h>

#include <xrt.h>



/* 应用取消回调只负责唤醒或标记对应工作。 */
static void stopWork(ptr pData)
{
	bool* pStopped = (bool*)pData;

	*pStopped = true;
}



/* 用父令牌一次取消一组子操作。 */
int main(void)
{
	xcancel* pRequest = xrtCancelCreate();
	xcancel* pOperation;
	xcancelwatch* pWatch;
	bool bStopped = false;

	if ( pRequest == NULL ) {
		return 1;
	}
	pOperation = xrtCancelChild(pRequest);
	if ( pOperation == NULL ) {
		xrtCancelDestroy(pRequest);
		return 1;
	}
	pWatch = xrtCancelWatch(pOperation, stopWork, &bStopped);
	if ( pWatch == NULL ) {
		xrtCancelDestroy(pOperation);
		xrtCancelDestroy(pRequest);
		return 1;
	}

	(void)xrtCancelRequest(pRequest);
	printf("operation stopped: %s\n", bStopped ? "yes" : "no");

	xrtCancelUnwatch(pWatch);
	xrtCancelDestroy(pOperation);
	xrtCancelDestroy(pRequest);
	return bStopped ? 0 : 1;
}
