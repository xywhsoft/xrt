#include "../test.h"
#include "../test_thread.h"



/* 并发初始化上下文记录实际执行次数。 */
typedef struct testoncecontext {
	xonce Once;
	int Count;
} testoncecontext;



/* 成功初始化过程只能由一个线程执行。 */
static bool testOnceInitialize(ptr pData)
{
	testoncecontext* pContext = (testoncecontext*)pData;

	pContext->Count++;
	return true;
}



/* 每个外部线程并发进入同一个 Once。 */
static int testOnceWorker(ptr pData)
{
	testoncecontext* pContext = (testoncecontext*)pData;

	return xrtOnce(&pContext->Once, testOnceInitialize, pContext) ? 0 : 1;
}



/* 首次失败后第二次初始化必须能够重试。 */
static bool testOnceRetry(ptr pData)
{
	int* pCount = (int*)pData;

	(*pCount)++;
	return *pCount >= 2;
}



/* 递归上下文保存内层调用结果。 */
typedef struct testoncerecursive {
	xonce Once;
	bool InnerResult;
} testoncerecursive;



/* 同一线程递归进入相同 Once 必须立即失败。 */
static bool testOnceRecursive(ptr pData)
{
	testoncerecursive* pContext = (testoncerecursive*)pData;

	pContext->InnerResult = xrtOnce(&pContext->Once, testOnceRecursive, pContext);
	return pContext->InnerResult;
}



/* 递归初始化图只用一个 Once 契约表达，回边由初始化过程显式确认。 */
typedef struct testoncegraph {
	xonce First;
	xonce Second;
	int FirstCount;
	int SecondCount;
	bool BackEdgeSeen;
} testoncegraph;



static bool testOnceGraphFirst(ptr pData);



/* 第二个节点识别返回第一个节点的同线程回边，并把它视为已在构建。 */
static bool testOnceGraphSecond(ptr pData)
{
	testoncegraph* pContext = (testoncegraph*)pData;
	bool bResult;

	pContext->SecondCount++;
	xrtClearError();
	bResult = xrtOnce(&pContext->First, testOnceGraphFirst, pContext);
	if ( bResult || (xrtErrorKind(xrtGetError()) != XERR_STATE) ) {
		return false;
	}
	pContext->BackEdgeSeen = true;
	xrtClearError();
	return true;
}



/* 第一个节点继续初始化第二个节点，形成 First -> Second -> First 环。 */
static bool testOnceGraphFirst(ptr pData)
{
	testoncegraph* pContext = (testoncegraph*)pData;

	pContext->FirstCount++;
	return xrtOnce(&pContext->Second, testOnceGraphSecond, pContext);
}



/* 多个线程同时进入递归图时仍只有一个线程负责构建整张图。 */
static int testOnceGraphWorker(ptr pData)
{
	testoncegraph* pContext = (testoncegraph*)pData;

	return xrtOnce(&pContext->First, testOnceGraphFirst, pContext) ? 0 : 1;
}



/* 验证并发、失败重试、递归检测和参数边界。 */
int main(void)
{
	testoncecontext tContext = { XRT_ONCE_INIT, 0 };
	testoncerecursive tRecursive = { XRT_ONCE_INIT, true };
	testoncegraph tGraph = {
		XRT_ONCE_INIT,
		XRT_ONCE_INIT,
		0,
		0,
		false
	};
	xonce tRetry = XRT_ONCE_INIT;
	testthread arrThread[8];
	int iRetryCount = 0;

	memset(arrThread, 0, sizeof(arrThread));
	for ( size_t i = 0; i < 8; i++ ) {
		arrThread[i].Proc = testOnceWorker;
		arrThread[i].Data = &tContext;
	}
	testThreadsStart(arrThread, 8);
	testThreadsJoin(arrThread, 8);
	for ( size_t i = 0; i < 8; i++ ) {
		testRequire(arrThread[i].Result == 0, "concurrent once caller failed");
	}
	testRequire(tContext.Count == 1, "once initializer ran more than once");
	testRequire(
		xrtOnce(&tContext.Once, testOnceInitialize, &tContext),
		"completed once failed"
	);
	testRequire(tContext.Count == 1, "completed once reran initializer");

	testRequire(
		!xrtOnce(&tRetry, testOnceRetry, &iRetryCount),
		"failed once unexpectedly completed"
	);
	testRequire(
		xrtOnce(&tRetry, testOnceRetry, &iRetryCount),
		"failed once did not retry"
	);
	testRequire(iRetryCount == 2, "once retry count mismatch");

	testRequire(
		!xrtOnce(&tRecursive.Once, testOnceRecursive, &tRecursive),
		"recursive once unexpectedly succeeded"
	);
	testRequire(!tRecursive.InnerResult, "recursive once inner call succeeded");
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"recursive once error mismatch"
	);

	memset(arrThread, 0, sizeof(arrThread));
	for ( size_t i = 0; i < 8; i++ ) {
		arrThread[i].Proc = testOnceGraphWorker;
		arrThread[i].Data = &tGraph;
	}
	testThreadsStart(arrThread, 8);
	testThreadsJoin(arrThread, 8);
	for ( size_t i = 0; i < 8; i++ ) {
		testRequire(arrThread[i].Result == 0, "recursive once graph caller failed");
	}
	testRequire(
		(tGraph.FirstCount == 1) && (tGraph.SecondCount == 1),
		"recursive once graph initializer count mismatch"
	);
	testRequire(tGraph.BackEdgeSeen, "recursive once graph back edge was not detected");
	testRequire(
		xrtOnce(&tGraph.Second, testOnceGraphSecond, &tGraph),
		"completed recursive once graph failed"
	);
	testRequire(
		(tGraph.FirstCount == 1) && (tGraph.SecondCount == 1),
		"completed recursive once graph reran an initializer"
	);

	xrtClearError();
	testRequire(
		!xrtOnce(NULL, testOnceInitialize, &tContext),
		"null once unexpectedly succeeded"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"null once error mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtOnce(&tContext.Once, NULL, &tContext),
		"null once callback unexpectedly succeeded"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"null once callback error mismatch"
	);

	printf("[PASS] once\n");
	return 0;
}
