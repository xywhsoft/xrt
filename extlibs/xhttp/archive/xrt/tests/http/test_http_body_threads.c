#include "../test.h"



/* 并发测试工厂只允许一个不可重放 Reader 被创建和关闭。 */
typedef struct test_http_body_thread_factory {
	size_t Opens;
	size_t Closes;
} test_http_body_thread_factory;



/* 每个线程保存共享正文和独立 Open 结果。 */
typedef struct test_http_body_thread {
	xhttpbody* Body;
	xhttpbodyreader* Reader;
} test_http_body_thread;



/* 未读取的测试 Reader 直接结束。 */
static xhttpbodystatus testHttpBodyThreadNext(
	ptr pContext,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	(void)pContext;
	(void)iMaxBytes;
	(void)pChunk;
	return XHTTP_BODY_EOF;
}



/* 记录唯一 Reader 的关闭。 */
static void testHttpBodyThreadClose(ptr pContext)
{
	test_http_body_thread_factory* pFactory =
		(test_http_body_thread_factory*)pContext;

	pFactory->Closes++;
}



/* 创建不可重放来源的唯一 Reader。 */
static bool testHttpBodyThreadOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	test_http_body_thread_factory* pState =
		(test_http_body_thread_factory*)pFactory;

	pState->Opens++;
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = testHttpBodyThreadNext;
	pOps->Close = testHttpBodyThreadClose;
	*ppReader = pState;
	return true;
}



/* 与其他工作线程竞争一次不可重放 Open。 */
static int32 testHttpBodyOpenThread(ptr pData)
{
	test_http_body_thread* pThread =
		(test_http_body_thread*)pData;

	pThread->Reader = xrtHttpBodyOpen(pThread->Body);
	return 0;
}



/* 验证不可重放 Open 门由原子状态保护且没有额外成功者。 */
int main(void)
{
	enum { TEST_HTTP_BODY_THREADS = 16 };
	xhttpbodyops Ops = {
		testHttpBodyThreadOpen,
		NULL
	};
	test_http_body_thread_factory Factory = { 0 };
	test_http_body_thread Contexts[TEST_HTTP_BODY_THREADS];
	xthread* Threads[TEST_HTTP_BODY_THREADS];
	xhttpbody* pBody;
	size_t iSuccess = 0;
	size_t i;

	pBody = xrtHttpBodyCreate(
		&Ops, &Factory, 0, XHTTP_BODY_NONE
	);
	testRequire(pBody != NULL,
		"threaded HTTP body create failed");
	for ( i = 0; i < TEST_HTTP_BODY_THREADS; i++ ) {
		Contexts[i].Body = pBody;
		Contexts[i].Reader = NULL;
		Threads[i] = xrtThreadCreate(
			testHttpBodyOpenThread, &Contexts[i], 0
		);
		testRequire(Threads[i] != NULL,
			"HTTP body Open worker create failed");
	}
	for ( i = 0; i < TEST_HTTP_BODY_THREADS; i++ ) {
		testRequire((xrtThreadWait(Threads[i]) == XWAIT_OK) &&
			(xrtThreadExitCode(Threads[i]) == 0),
			"HTTP body Open worker failed");
		xrtThreadDestroy(Threads[i]);
		if ( Contexts[i].Reader != NULL ) {
			iSuccess++;
			xrtHttpBodyReaderDestroy(Contexts[i].Reader);
		}
	}
	testRequire((iSuccess == 1) &&
		(Factory.Opens == 1) &&
		(Factory.Closes == 1),
		"non-replayable HTTP body admitted multiple Readers");
	xrtHttpBodyDestroy(pBody);
	printf("[PASS] HTTP body threads\n");
	return 0;
}
