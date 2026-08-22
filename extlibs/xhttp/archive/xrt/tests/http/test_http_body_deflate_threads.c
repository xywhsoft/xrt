#include "../test.h"



#define TEST_HTTP_BODY_DEFLATE_THREADS 16u
#define TEST_HTTP_BODY_DEFLATE_OUTPUT 512u



/* 每个工作线程保存独立读取结果和共享只读预期值。 */
typedef struct test_http_body_deflate_thread {
	xhttpbody* Body;
	cbytes Expected;
	size_t ExpectedSize;
	uint8 Output[TEST_HTTP_BODY_DEFLATE_OUTPUT];
	size_t OutputSize;
	bool Passed;
} test_http_body_deflate_thread;



/* 并发打开并完整读取一个独立压缩数据流。 */
static int32 testHttpBodyDeflateThreadRun(ptr pData)
{
	test_http_body_deflate_thread* pThread =
		(test_http_body_deflate_thread*)pData;
	xhttpbodyreader* pReader = xrtHttpBodyOpen(pThread->Body);
	xhttpbodychunk Chunk;
	xhttpbodystatus Status;

	if ( pReader == NULL ) {
		return 1;
	}
	while ( (Status = xrtHttpBodyNext(
		pReader, 7, &Chunk
	)) == XHTTP_BODY_DATA ) {
		if ( Chunk.Size >
			(sizeof(pThread->Output) - pThread->OutputSize) ) {
			xrtHttpBodyChunkRelease(&Chunk);
			xrtHttpBodyReaderDestroy(pReader);
			return 2;
		}
		memcpy(
			pThread->Output + pThread->OutputSize,
			Chunk.Data,
			Chunk.Size
		);
		pThread->OutputSize += Chunk.Size;
		xrtHttpBodyChunkRelease(&Chunk);
	}
	xrtHttpBodyReaderDestroy(pReader);
	pThread->Passed = (Status == XHTTP_BODY_EOF) &&
		(pThread->OutputSize == pThread->ExpectedSize) &&
		(memcmp(
			pThread->Output,
			pThread->Expected,
			pThread->ExpectedSize
		) == 0);
	return pThread->Passed ? 0 : 3;
}



/* 验证并发重放不共享编码器、来源游标或输出队列。 */
int main(void)
{
	static const uint8 Input[] =
		"parallel replay parallel replay parallel replay";
	test_http_body_deflate_thread
		Contexts[TEST_HTTP_BODY_DEFLATE_THREADS];
	xthread* Threads[TEST_HTTP_BODY_DEFLATE_THREADS];
	xhttpbody* pSource = xrtHttpBodyBorrow(
		(xbytesview){ Input, sizeof(Input) - 1u }
	);
	xhttpbody* pBody = xrtHttpBodyDeflate(pSource, NULL);
	bytes pExpected;
	size_t iExpected;
	size_t i;

	testRequire((pSource != NULL) && (pBody != NULL),
		"threaded HTTP Deflate body setup failed");
	xrtHttpBodyDestroy(pSource);
	pExpected = xrtDeflateAll(
		(xbytesview){ Input, sizeof(Input) - 1u },
		NULL,
		&iExpected
	);
	testRequire((pExpected != NULL) &&
		(iExpected <= TEST_HTTP_BODY_DEFLATE_OUTPUT),
		"threaded HTTP Deflate expected output failed");
	memset(Contexts, 0, sizeof(Contexts));
	for ( i = 0; i < TEST_HTTP_BODY_DEFLATE_THREADS; i++ ) {
		Contexts[i].Body = pBody;
		Contexts[i].Expected = pExpected;
		Contexts[i].ExpectedSize = iExpected;
		Threads[i] = xrtThreadCreate(
			testHttpBodyDeflateThreadRun,
			&Contexts[i],
			0
		);
		testRequire(Threads[i] != NULL,
			"HTTP Deflate replay thread creation failed");
	}
	for ( i = 0; i < TEST_HTTP_BODY_DEFLATE_THREADS; i++ ) {
		testRequire((xrtThreadWait(Threads[i]) == XWAIT_OK) &&
			(xrtThreadExitCode(Threads[i]) == 0) &&
			Contexts[i].Passed,
			"HTTP Deflate concurrent replay failed");
		xrtThreadDestroy(Threads[i]);
	}
	xrtFree(pExpected);
	xrtHttpBodyDestroy(pBody);
	printf("[PASS] threaded HTTP Deflate body\n");
	return 0;
}
