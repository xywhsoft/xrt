#define XRUNTIME_IMPLEMENTATION
#include "../../single/xruntime.h"



/* 验证单头文件中的 Future 类型描述与槽引用生命周期。 */
int main(void)
{
	const xrttype* pType = xrtTypeFuture();
	xfuture* pFuture = NULL;
	xfuture* pSlot = NULL;
	xpromise* pPromise = xrtPromiseCreate(&pFuture, NULL);
	int iAnswer = 42;
	int iResult = 0;

	if (
		(pPromise == NULL) || (pFuture == NULL) ||
		!xrtTypeValidate(pType) ||
		!xrtTypeInitValue(pType, &pSlot) ||
		!xrtTypeCopyValue(pType, &pSlot, &pFuture)
	) {
		iResult = 1;
	} else {
		xrtFutureDestroy(pFuture);
		pFuture = NULL;
		if (
			!xrtPromiseResolve(pPromise, &iAnswer) ||
			(xrtFutureValue(pSlot) != &iAnswer)
		) {
			iResult = 2;
		}
	}
	xrtFutureDestroy(pFuture);
	xrtTypeDropValue(pType, &pSlot);
	xrtPromiseDestroy(pPromise);
	return iResult;
}
