#include <stdio.h>
#include <xruntime.h>



/* 展示 Future 类型槽在复制、完成和销毁过程中的所有权。 */
int main(void)
{
	const xrttype* pType = xrtTypeFuture();
	xfuture* pFuture = NULL;
	xfuture* pSlot = NULL;
	xpromise* pPromise = xrtPromiseCreate(&pFuture, NULL);
	int iAnswer = 42;

	if (
		(pPromise == NULL) || (pFuture == NULL) ||
		!xrtTypeInitValue(pType, &pSlot) ||
		!xrtTypeCopyValue(pType, &pSlot, &pFuture)
	) {
		xrtFutureDestroy(pFuture);
		xrtPromiseDestroy(pPromise);
		return 1;
	}
	xrtFutureDestroy(pFuture);
	pFuture = NULL;
	if (
		!xrtPromiseResolve(pPromise, &iAnswer) ||
		(xrtFutureValue(pSlot) != &iAnswer)
	) {
		xrtTypeDropValue(pType, &pSlot);
		xrtPromiseDestroy(pPromise);
		return 2;
	}

	printf(
		"type=%.*s value=%d\n",
		(int)pType->Name.Size,
		pType->Name.Data,
		*(int*)xrtFutureValue(pSlot)
	);
	xrtTypeDropValue(pType, &pSlot);
	xrtPromiseDestroy(pPromise);
	return 0;
}
