#include "../test.h"



/* 可在 Reader 打开后切换的大块故障分配器。 */
typedef struct test_http_body_inflate_oom {
	size_t RejectAtLeast;
	size_t Denied;
} test_http_body_inflate_oom;



/* 拒绝达到动态阈值的底层申请。 */
static ptr testHttpBodyInflateOomAlloc(
	ptr pContext,
	size_t iSize
)
{
	test_http_body_inflate_oom* pState =
		(test_http_body_inflate_oom*)pContext;

	if ( (pState->RejectAtLeast != 0) &&
		(iSize >= pState->RejectAtLeast) ) {
		pState->Denied++;
		return NULL;
	}
	return malloc(iSize);
}



/* 重分配使用同一动态阈值，并在失败时保留原块。 */
static ptr testHttpBodyInflateOomRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_http_body_inflate_oom* pState =
		(test_http_body_inflate_oom*)pContext;

	if ( (pState->RejectAtLeast != 0) &&
		(iSize >= pState->RejectAtLeast) ) {
		pState->Denied++;
		return NULL;
	}
	return realloc(pMemory, iSize);
}



/* 归还故障窗口外取得的底层内存。 */
static void testHttpBodyInflateOomFree(
	ptr pContext,
	ptr pMemory
)
{
	(void)pContext;
	free(pMemory);
}



/* 验证解码器窗口和输出块 OOM 都不泄漏来源、队列或 Chunk。 */
int main(void)
{
	static uint8 Input[131072];
	test_http_body_inflate_oom State = { 0 };
	xallocator Allocator = {
		&State,
		testHttpBodyInflateOomAlloc,
		testHttpBodyInflateOomRealloc,
		testHttpBodyInflateOomFree
	};
	xdeflateconfig DeflateConfig;
	xhttpbodyinflateconfig InflateConfig;
	bytes pEncoded;
	size_t iEncoded;
	xhttpbody* pSource;
	xhttpbody* pBody;
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	xhttpbodystatus Status;
	size_t i;
	size_t iDenied;

	testRequire(xrtSetAllocator(&Allocator),
		"HTTP Inflate body OOM allocator install failed");
	for ( i = 0; i < sizeof(Input); i++ ) {
		Input[i] = (uint8)('A' + (i % 7u));
	}
	xrtDeflateConfigInit(&DeflateConfig);
	DeflateConfig.Format = XDEFLATE_GZIP;
	pEncoded = xrtDeflateAll(
		(xbytesview){ Input, sizeof(Input) },
		&DeflateConfig,
		&iEncoded
	);
	testRequire(pEncoded != NULL,
		"HTTP Inflate body OOM fixture encoding failed");
	pSource = xrtHttpBodyBorrow(
		(xbytesview){ pEncoded, iEncoded }
	);
	xrtHttpBodyInflateConfigInit(&InflateConfig);
	InflateConfig.Inflate.Format = XINFLATE_GZIP;
	InflateConfig.ReadSize = iEncoded;
	pBody = xrtHttpBodyInflate(pSource, &InflateConfig);
	testRequire((pSource != NULL) &&
		(pBody != NULL),
		"HTTP Inflate body OOM setup failed");

	/* 算法窗口分配失败不能消费或泄漏来源 Reader。 */
	State.RejectAtLeast = 1025;
	iDenied = State.Denied;
	pReader = xrtHttpBodyOpen(pBody);
	testRequire((pReader == NULL) &&
		(State.Denied > iDenied) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"HTTP Inflate decoder state OOM mismatch");
	xrtClearError();

	/* Reader 成功打开后拒绝大输出块，并覆盖已排队块的回收。 */
	State.RejectAtLeast = 0;
	pReader = xrtHttpBodyOpen(pBody);
	testRequire(pReader != NULL,
		"HTTP Inflate body OOM reader open failed");
	State.RejectAtLeast = 1025;
	iDenied = State.Denied;
	for ( ;; ) {
		Status = xrtHttpBodyNext(
			pReader, 64, &Chunk
		);
		if ( Status != XHTTP_BODY_DATA ) {
			break;
		}
		xrtHttpBodyChunkRelease(&Chunk);
	}
	testRequire((Status == XHTTP_BODY_ERROR) &&
		(State.Denied > iDenied) &&
		(xrtHttpBodyReaderError(pReader) != NULL) &&
		(xrtErrorKind(
			xrtHttpBodyReaderError(pReader)
		) == XERR_MEMORY),
		"HTTP Inflate output block OOM mismatch");
	State.RejectAtLeast = 0;
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	xrtHttpBodyDestroy(pSource);
	xrtFree(pEncoded);
	xrtClearError();
	testMemoryDebugDrain(
		"HTTP Inflate body OOM leaked a logical allocation"
	);

	printf("[PASS] HTTP Inflate body OOM\n");
	return 0;
}

