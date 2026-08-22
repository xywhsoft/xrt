#include <stdio.h>
#include <string.h>
#include <xrt.h>



typedef struct exampleengine {
	xatomic32 Done;
} exampleengine;



/* 普通任务在亲和 Worker 上执行。 */
static void examplePost(xnetworker* pWorker, ptr pData)
{
	exampleengine* pState = (exampleengine*)pData;

	printf("worker %u handled a posted task\n", xrtNetWorkerIndex(pWorker));
	(void)xrtAtomic32FetchAdd(&pState->Done, 1, XMEMORY_RELEASE);
}



/* Timer 以明确终态回到同一个 Worker。 */
static void exampleTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	exampleengine* pState = (exampleengine*)pData;

	printf("worker %u completed timer %llu with result %d\n",
		xrtNetWorkerIndex(pWorker),
		(unsigned long long)Id,
		(int)Result);
	(void)xrtAtomic32FetchAdd(&pState->Done, 1, XMEMORY_RELEASE);
}



/* 建立两个 Worker，分别演示立即任务和延迟任务。 */
int main(void)
{
	xnetengineconfig Config;
	exampleengine State;
	xnetengine* pEngine;
	xdeadline iDeadline;

	memset(&State, 0, sizeof(State));
	xrtNetEngineConfigInit(&Config);
	Config.Workers = 2;
	pEngine = xrtNetEngineCreate(&Config);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		return 1;
	}
	if ( !xrtNetEnginePost(pEngine, 0, examplePost, &State) ||
		 (xrtNetEngineAfter(
			pEngine,
			1,
			100000u,
			exampleTimer,
			&State
		) == 0) ) {
		(void)xrtNetEngineDestroy(pEngine);
		return 2;
	}
	iDeadline = xrtDeadlineAfter(2000000u);
	while ( xrtAtomic32Load(&State.Done, XMEMORY_ACQUIRE) != 2 ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			(void)xrtNetEngineDestroy(pEngine);
			return 3;
		}
		xrtThreadYield();
	}
	return xrtNetEngineDestroy(pEngine) ? 0 : 4;
}
