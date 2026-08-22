#include "../test.h"
#include "../test_thread.h"



/* 等待跨线程发布的信号。 */
static int testSemWaiter(ptr pData)
{
	xsem* pSem = (xsem*)pData;

	return xrtSemWaitFor(pSem, UINT64_C(2000000)) == XWAIT_OK ? 29 : 1;
}



/* 在较短期限内等待批量发布，用于验证发布数量。 */
static int testSemBatchWaiter(ptr pData)
{
	xsem* pSem = (xsem*)pData;
	xwaitresult Result = xrtSemWaitFor(pSem, UINT64_C(200000));

	return Result == XWAIT_OK ? 1 : (Result == XWAIT_TIMEOUT ? 0 : -1);
}



/* 验证计数上限、全有或全无发布、超时和跨线程唤醒。 */
int main(void)
{
	xsem tSem;
	xsem* pSem;
	testthread tThread;
	testthread arrBatch[3];
	int iBatchWakeCount = 0;

	memset(&tSem, 0, sizeof(tSem));
	testRequire(!xrtSemInit(&tSem, 0, 0), "zero-maximum semaphore init succeeded");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "zero maximum error mismatch");
	xrtClearError();
	testRequire(!xrtSemInit(&tSem, 3, 2), "oversized initial semaphore value succeeded");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "initial value error mismatch");
	xrtClearError();
	testRequire(xrtSemInit(&tSem, 0, 2), "in-place semaphore init failed");
	testRequire(xrtSemTryWait(&tSem) == XWAIT_TIMEOUT, "empty semaphore try-wait mismatch");
	testRequire(xrtSemPostMany(&tSem, 0), "zero-count semaphore post failed");
	testRequire(xrtSemTryWait(&tSem) == XWAIT_TIMEOUT, "zero-count post changed semaphore");
	testRequire(xrtSemPostMany(&tSem, 2), "semaphore multi-post failed");
	testRequire(xrtSemTryWait(&tSem) == XWAIT_OK, "semaphore first consume failed");
	testRequire(xrtSemTryWait(&tSem) == XWAIT_OK, "semaphore second consume failed");
	testRequire(xrtSemTryWait(&tSem) == XWAIT_TIMEOUT, "drained semaphore was signaled");
	testRequire(xrtSemUnit(&tSem), "in-place semaphore unit failed");

	pSem = xrtSemCreate(1, 2);
	testRequire(pSem != NULL, "owned semaphore create failed");
	testRequire(!xrtSemPostMany(pSem, 2), "overflow semaphore multi-post succeeded");
	testRequire(
		!xrtSemPostMany(pSem, UINT32_MAX),
		"oversized semaphore multi-post succeeded"
	);
	xrtClearError();
	testRequire(xrtSemTryWait(pSem) == XWAIT_OK, "overflow changed original count");
	testRequire(xrtSemTryWait(pSem) == XWAIT_TIMEOUT, "overflow partially posted count");
	memset(arrBatch, 0, sizeof(arrBatch));
	for ( size_t i = 0; i < 3; i++ ) {
		arrBatch[i].Proc = testSemBatchWaiter;
		arrBatch[i].Data = pSem;
	}
	testThreadsStart(arrBatch, 3);
	xrtSleep(20);
	testRequire(xrtSemPostMany(pSem, 2), "batch semaphore post failed");
	testThreadsJoin(arrBatch, 3);
	for ( size_t i = 0; i < 3; i++ ) {
		testRequire(arrBatch[i].Result >= 0, "batch semaphore waiter failed");
		iBatchWakeCount += arrBatch[i].Result;
	}
	testRequire(iBatchWakeCount == 2, "batch semaphore post released the wrong waiter count");
	memset(&tThread, 0, sizeof(tThread));
	tThread.Proc = testSemWaiter;
	tThread.Data = pSem;
	testThreadsStart(&tThread, 1);
	xrtSleep(20);
	testRequire(xrtSemPost(pSem), "cross-thread semaphore post failed");
	testThreadsJoin(&tThread, 1);
	testRequire(tThread.Result == 29, "cross-thread semaphore waiter failed");
	testRequire(xrtSemDestroy(pSem), "owned semaphore destroy failed");

	printf("[PASS] sem\n");
	return 0;
}
