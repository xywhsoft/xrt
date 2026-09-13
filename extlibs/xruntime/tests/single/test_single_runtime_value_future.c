#define XRUNTIME_IMPLEMENTATION
#include "../../single/xruntime.h"
#include <assert.h>



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
	if (iResult == 0) {
		xrtownershipref slots[2] = {xrtValueOwnership(pValue), xrtPromiseOwnership(pPromise)};
		xrtownershipresult graph = {0}; bool live = true;
		assert(xrtOwnershipInspectReachable(slots, 1, slots, 2, &live, &graph, NULL, NULL));
		assert(graph.NodeCount == 3 && graph.EdgeCount == 4 && graph.ExternalRootCount == 0 && !live);
		assert(xrtValueRetain(pValue) == pValue);
		assert(xrtOwnershipInspectReachable(slots, 1, slots, 2, &live, &graph, NULL, NULL));
		assert(live && graph.ExternalRootCount == 1); xrtValueRelease(pValue);
	}
	xrtValueRelease(pValue);
	xrtPromiseDestroy(pPromise);
	return iResult;
}
