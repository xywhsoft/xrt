#include "../test.h"
#include "../test_thread.h"



/* 线程回归记录生产线程和源完成延续的实际执行线程。 */
typedef struct testfuturecontinuethread {
	xpromise* Promise;
	uint64 ProducerId;
	uint64 ContinueId;
	int Value;
} testfuturecontinuethread;



/* 延续应在完成源 Promise 的同一线程同步执行。 */
static void testFutureContinueThreadProc(
	const xfutureresult* pInput,
	xpromise* pOutput,
	ptr pData
)
{
	testfuturecontinuethread* pContext =
		(testfuturecontinuethread*)pData;

	pContext->ContinueId = xrtThreadCurrentId();
	testRequire(xrtPromiseResolve(pOutput, pInput->Value),
		"threaded future continuation output resolve failed");
}



/* 工作线程写入源 Promise，并在返回前完成全部短延续。 */
static int testFutureContinueProducer(ptr pData)
{
	testfuturecontinuethread* pContext =
		(testfuturecontinuethread*)pData;

	pContext->ProducerId = xrtThreadCurrentId();
	return xrtPromiseResolve(pContext->Promise, &pContext->Value) ? 0 : 1;
}



/* 验证 source-context 契约不依赖线程队列或手工 pump。 */
int main(void)
{
	testfuturecontinuethread tContext = { 0 };
	testthread tProducer = { 0 };
	xfuture* pSource;
	xfuture* pNext;
	xpromise* pPromise = xrtPromiseCreate(&pSource, NULL);

	testRequire(pPromise != NULL,
		"threaded future continuation source create failed");
	tContext.Promise = pPromise;
	tContext.Value = 81;
	pNext = xrtFutureThen(
		pSource,
		testFutureContinueThreadProc,
		&tContext
	);
	testRequire(pNext != NULL,
		"threaded future continuation create failed");
	tProducer.Proc = testFutureContinueProducer;
	tProducer.Data = &tContext;
	testThreadsStart(&tProducer, 1);
	testThreadsJoin(&tProducer, 1);
	testRequire((tProducer.Result == 0) &&
		(tContext.ProducerId != 0) &&
		(tContext.ContinueId == tContext.ProducerId),
		"future continuation left source completion thread");
	testRequire(xrtFutureValue(pNext) == &tContext.Value,
		"threaded future continuation result mismatch");

	xrtFutureDestroy(pNext);
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pSource);
	printf("[PASS] future continuation threads\n");
	return 0;
}
