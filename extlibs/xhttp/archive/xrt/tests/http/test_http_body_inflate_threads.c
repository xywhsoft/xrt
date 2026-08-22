#include "../test.h"
#include "test_http_body_inflate_fixture.h"



#define TEST_HTTP_BODY_INFLATE_THREADS 16u
#define TEST_HTTP_BODY_INFLATE_OUTPUT 128u



/* 每个工作线程保存独立读取结果和共享只读正文引用。 */
typedef struct test_http_body_inflate_thread {
	xhttpbody* Body;
	uint8 Output[TEST_HTTP_BODY_INFLATE_OUTPUT];
	size_t OutputSize;
	bool Passed;
} test_http_body_inflate_thread;



/* 并发打开并完整读取一个独立解压数据流。 */
static int32 testHttpBodyInflateThreadRun(ptr pData)
{
	test_http_body_inflate_thread* pThread =
		(test_http_body_inflate_thread*)pData;
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
		(pThread->OutputSize ==
		 (sizeof(TestHttpBodyInflateParallelPlain) - 1u)) &&
		(memcmp(
			pThread->Output,
			TestHttpBodyInflateParallelPlain,
			pThread->OutputSize
		) == 0);
	return pThread->Passed ? 0 : 3;
}



/* 验证并发重放不共享解码器、来源游标或输出队列。 */
int main(void)
{
	test_http_body_inflate_thread
		Contexts[TEST_HTTP_BODY_INFLATE_THREADS];
	xthread* Threads[TEST_HTTP_BODY_INFLATE_THREADS];
	xhttpbodyinflateconfig Config;
	xhttpbody* pSource = xrtHttpBodyBorrow(
		(xbytesview){
			TestHttpBodyInflateParallelGzip,
			sizeof(TestHttpBodyInflateParallelGzip)
		}
	);
	xhttpbody* pBody;
	size_t i;

	xrtHttpBodyInflateConfigInit(&Config);
	Config.Inflate.Format = XINFLATE_GZIP;
	pBody = xrtHttpBodyInflate(pSource, &Config);
	testRequire((pSource != NULL) && (pBody != NULL),
		"threaded HTTP Inflate body setup failed");
	xrtHttpBodyDestroy(pSource);
	memset(Contexts, 0, sizeof(Contexts));
	for ( i = 0; i < TEST_HTTP_BODY_INFLATE_THREADS; i++ ) {
		Contexts[i].Body = pBody;
		Threads[i] = xrtThreadCreate(
			testHttpBodyInflateThreadRun,
			&Contexts[i],
			0
		);
		testRequire(Threads[i] != NULL,
			"HTTP Inflate replay thread creation failed");
	}
	for ( i = 0; i < TEST_HTTP_BODY_INFLATE_THREADS; i++ ) {
		testRequire((xrtThreadWait(Threads[i]) == XWAIT_OK) &&
			(xrtThreadExitCode(Threads[i]) == 0) &&
			Contexts[i].Passed,
			"HTTP Inflate concurrent replay failed");
		xrtThreadDestroy(Threads[i]);
	}
	xrtHttpBodyDestroy(pBody);
	printf("[PASS] threaded HTTP Inflate body\n");
	return 0;
}
