#include "../test.h"



#define TEST_HTTP_BODY_OOM_READERS 256u
#define TEST_HTTP_BODY_OOM_DATA_SIZE 2048u



/* 可调失败分配器验证多阶段创建失败不会泄漏。 */
typedef struct test_http_body_oom {
	size_t Calls;
	size_t FailAt;
	size_t Live;
} test_http_body_oom;



/* Reader 分配失败时，Close 会尝试发布一个需要被屏蔽的错误。 */
typedef struct test_http_body_oom_source {
	xerror* CleanupError;
	size_t Opens;
	size_t Closes;
} test_http_body_oom_source;



/* OOM 来源本身没有正文数据。 */
static xhttpbodystatus testHttpBodyOomSourceNext(
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



/* Close 尝试覆盖 Reader 分配失败留下的内存错误。 */
static void testHttpBodyOomSourceClose(ptr pContext)
{
	test_http_body_oom_source* pSource =
		(test_http_body_oom_source*)pContext;

	pSource->Closes++;
	xrtSetError(pSource->CleanupError);
}



/* 每次打开返回独立生命周期所需的操作表。 */
static bool testHttpBodyOomSourceOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	test_http_body_oom_source* pSource =
		(test_http_body_oom_source*)pFactory;

	pSource->Opens++;
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = testHttpBodyOomSourceNext;
	pOps->Close = testHttpBodyOomSourceClose;
	*ppReader = pSource;
	return true;
}



/* 在指定分配序号失败，其余分配记录存活块。 */
static ptr testHttpBodyOomAlloc(ptr pContext, size_t iSize)
{
	test_http_body_oom* pState =
		(test_http_body_oom*)pContext;
	ptr pMemory;

	pState->Calls++;
	if ( pState->Calls == pState->FailAt ) {
		return NULL;
	}
	pMemory = malloc(iSize);
	if ( pMemory != NULL ) {
		pState->Live++;
	}
	return pMemory;
}



/* OOM 测试不需要成功重分配。 */
static ptr testHttpBodyOomRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	(void)pContext;
	(void)pMemory;
	(void)iSize;
	return NULL;
}



/* 释放存活块并检查计数不会下溢。 */
static void testHttpBodyOomFree(ptr pContext, ptr pMemory)
{
	test_http_body_oom* pState =
		(test_http_body_oom*)pContext;

	if ( pMemory != NULL ) {
		testRequire(pState->Live != 0,
			"HTTP body OOM live counter underflow");
		pState->Live--;
		free(pMemory);
	}
}



/* 验证紧凑正文复制和 Reader 创建的 OOM 边界。 */
int main(void)
{
	static test_http_body_oom State = { 0 };
	static const xhttpbodyops SourceOps = {
		testHttpBodyOomSourceOpen,
		NULL
	};
	xallocator Allocator = {
		&State,
		testHttpBodyOomAlloc,
		testHttpBodyOomRealloc,
		testHttpBodyOomFree
	};
	xhttpbody* pBody;
	xhttpbodyreader* pReader;
	xerror* pCleanupError;
	xhttpbodyreader* Readers[TEST_HTTP_BODY_OOM_READERS];
	test_http_body_oom_source Source = { 0 };
	unsigned char Data[TEST_HTTP_BODY_OOM_DATA_SIZE];
	ptr pWarmup;
	size_t iBaseline;
	size_t iBacking;
	size_t iReaderCount = 0;
	size_t i;

	testRequire(xrtSetAllocator(&Allocator),
		"failed to install HTTP body OOM allocator");

	/* 预热线程缓存，使后续故障点只对应被测正文分配。 */
	pWarmup = xrtMalloc(1);
	testRequire(pWarmup != NULL, "HTTP body OOM cache warmup failed");
	xrtFree(pWarmup);
	testMemoryDebugDrain("HTTP body OOM warmup reset failed");
	iBaseline = State.Live;
	memset(Data, 0x5A, sizeof(Data));

	/* 大于池化上限的正文副本应直接命中第一个故障点。 */
	State.FailAt = State.Calls + 1u;
	testRequire(xrtHttpBodyCopy(
		(xbytesview){ Data, sizeof(Data) }
	) == NULL, "HTTP body copy survived first OOM");
	testMemoryDebugDrain("HTTP body first OOM reset failed");
	testRequire(State.Live == iBaseline,
		"HTTP body first OOM leaked memory");

	State.FailAt = 0;
	pBody = xrtHttpBodyCopy(
		(xbytesview){ Data, sizeof(Data) }
	);
	testRequire(pBody != NULL,
		"HTTP body OOM setup failed");
	iBacking = State.Live;
	State.FailAt = State.Calls + 1;

	/*
		Reader 可能复用 Body 尺寸类的空闲块。
		持有已有块，直到下一次 span 分配命中故障点。
	*/
	for ( ;; ) {
		pReader = xrtHttpBodyOpen(pBody);
		if ( pReader == NULL ) {
			break;
		}
		testRequire(
			iReaderCount < TEST_HTTP_BODY_OOM_READERS,
			"HTTP body Reader OOM did not reach a backing allocation"
		);
		Readers[iReaderCount++] = pReader;
	}
	testRequire(pReader == NULL,
		"HTTP body Reader survived OOM");
	testRequire(State.Live == iBacking,
		"HTTP body Reader OOM changed body ownership");
	for ( i = 0; i < iReaderCount; i++ ) {
		xrtHttpBodyReaderDestroy(Readers[i]);
	}
	xrtHttpBodyDestroy(pBody);
	testMemoryDebugDrain("HTTP body cleanup reset failed");
	testRequire(
		(iBacking != 0u) && (State.Live == (iBacking - 1u)),
		"HTTP body cleanup did not release direct payload"
	);

	/* Reader 分配 OOM 必须越过来源 Close，仍保留最初的内存错误。 */
	State.FailAt = 0;
	pCleanupError = xrtErrorCreate(
		XERR_STATE,
		"test.http.body.oom.cleanup",
		84,
		"cleanup error"
	);
	Source.CleanupError = pCleanupError;
	pBody = xrtHttpBodyCreate(
		&SourceOps,
		&Source,
		0,
		XHTTP_BODY_REPLAYABLE
	);
	testRequire((pCleanupError != NULL) && (pBody != NULL),
		"HTTP body custom OOM setup failed");
	xrtHttpBodyDestroy(pBody);
	xrtErrorFree(pCleanupError);
	memset(&Source, 0, sizeof(Source));
	iBaseline = State.Live;
	pCleanupError = xrtErrorCreate(
		XERR_STATE,
		"test.http.body.oom.cleanup",
		84,
		"cleanup error"
	);
	Source.CleanupError = pCleanupError;
	pBody = xrtHttpBodyCreate(
		&SourceOps,
		&Source,
		0,
		XHTTP_BODY_REPLAYABLE
	);
	testRequire((pCleanupError != NULL) && (pBody != NULL),
		"HTTP body custom OOM warm-up recovery failed");
	iReaderCount = 0;
	State.FailAt = State.Calls + 1u;
	for ( ;; ) {
		pReader = xrtHttpBodyOpen(pBody);
		if ( pReader == NULL ) {
			break;
		}
		testRequire(
			iReaderCount < TEST_HTTP_BODY_OOM_READERS,
			"HTTP body custom Reader OOM did not reach allocation"
		);
		Readers[iReaderCount++] = pReader;
	}
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(Source.Closes == 1),
		"HTTP body Close replaced Reader allocation OOM"
	);
	State.FailAt = 0;
	xrtClearError();
	for ( i = 0; i < iReaderCount; i++ ) {
		xrtHttpBodyReaderDestroy(Readers[i]);
	}
	testRequire(
		(xrtGetError() == NULL) &&
		(Source.Closes == (iReaderCount + 1u)),
		"HTTP body Reader cleanup leaked a callback error"
	);
	xrtHttpBodyDestroy(pBody);
	xrtErrorFree(pCleanupError);
	testMemoryDebugDrain("HTTP body custom OOM cleanup failed");
	testRequire(State.Live == iBaseline,
		"HTTP body custom OOM leaked storage");

	printf("[PASS] HTTP body OOM\n");
	return 0;
}
