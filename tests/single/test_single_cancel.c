#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件监听回调记录取消请求。 */
static void testSingleCancelCallback(ptr pData)
{
	int* pCount = (int*)pData;

	(*pCount)++;
}



/* 验证单头文件中的父子取消传播和监听生命周期。 */
int main(void)
{
	xcancel* pParent = xrtCancelCreate();
	xcancel* pChild = pParent != NULL ? xrtCancelChild(pParent) : NULL;
	xcancelwatch* pWatch = NULL;
	int iCount = 0;
	int iResult = 1;

	if ( pChild != NULL ) {
		pWatch = xrtCancelWatch(pChild, testSingleCancelCallback, &iCount);
	}
	if (
		(pWatch != NULL) && xrtCancelRequest(pParent) &&
		xrtCancelRequested(pChild) && xrtCancelTriggered(pWatch) &&
		(iCount == 1)
	) {
		iResult = 0;
	}
	xrtCancelUnwatch(pWatch);
	xrtCancelDestroy(pChild);
	xrtCancelDestroy(pParent);
	return iResult;
}
