#include "../test.h"



/* 可在 Reader 打开后切换的大块故障分配器。 */
typedef struct test_http_body_deflate_oom {
	size_t RejectAtLeast;
	size_t Denied;
} test_http_body_deflate_oom;



/* 拒绝达到动态阈值的底层申请。 */
static ptr testHttpBodyDeflateOomAlloc(
	ptr pContext,
	size_t iSize
)
{
	test_http_body_deflate_oom* pState =
		(test_http_body_deflate_oom*)pContext;

	if ( (pState->RejectAtLeast != 0) &&
		(iSize >= pState->RejectAtLeast) ) {
		pState->Denied++;
		return NULL;
	}
	return malloc(iSize);
}



/* 重分配使用同一动态阈值，并在失败时保留原块。 */
static ptr testHttpBodyDeflateOomRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_http_body_deflate_oom* pState =
		(test_http_body_deflate_oom*)pContext;

	if ( (pState->RejectAtLeast != 0) &&
		(iSize >= pState->RejectAtLeast) ) {
		pState->Denied++;
		return NULL;
	}
	return realloc(pMemory, iSize);
}



/* 归还故障窗口外取得的底层内存。 */
static void testHttpBodyDeflateOomFree(
	ptr pContext,
	ptr pMemory
)
{
	(void)pContext;
	free(pMemory);
}



/* 验证编码器状态和输出块 OOM 都不泄漏来源、队列或 Chunk。 */
int main(void)
{
	static uint8 Input[131072];
	test_http_body_deflate_oom State = { 0 };
	xallocator Allocator = {
		&State,
		testHttpBodyDeflateOomAlloc,
		testHttpBodyDeflateOomRealloc,
		testHttpBodyDeflateOomFree
	};
	xhttpbodydeflateconfig Config;
	xhttpbody* pSource;
	xhttpbody* pBody;
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	xhttpbodystatus Status;
	size_t i;
	size_t iDenied;
	uint32 iRandom = UINT32_C(0x12345678);

	testRequire(xrtSetAllocator(&Allocator),
		"HTTP Deflate body OOM allocator install failed");
	for ( i = 0; i < sizeof(Input); i++ ) {
		iRandom = (iRandom * UINT32_C(1664525)) +
			UINT32_C(1013904223);
		Input[i] = (uint8)(iRandom >> 24u);
	}
	pSource = xrtHttpBodyBorrow(
		(xbytesview){ Input, sizeof(Input) }
	);
	xrtHttpBodyDeflateConfigInit(&Config);
	Config.ReadSize = 65536;
	pBody = xrtHttpBodyDeflate(pSource, &Config);
	testRequire((pSource != NULL) && (pBody != NULL),
		"HTTP Deflate body OOM setup failed");

	/* 大状态分配失败不能消费或泄漏来源 Reader。 */
	State.RejectAtLeast = 1025;
	iDenied = State.Denied;
	pReader = xrtHttpBodyOpen(pBody);
	testRequire((pReader == NULL) &&
		(State.Denied > iDenied) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"HTTP Deflate encoder state OOM mismatch");
	xrtClearError();

	/* Reader 成功打开后，拒绝直接输出块并覆盖已排队 Header 的回收。 */
	State.RejectAtLeast = 0;
	pReader = xrtHttpBodyOpen(pBody);
	testRequire(pReader != NULL,
		"HTTP Deflate body OOM reader open failed");
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
		"HTTP Deflate output block OOM mismatch");
	State.RejectAtLeast = 0;
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	xrtHttpBodyDestroy(pSource);
	xrtClearError();
	testMemoryDebugDrain(
		"HTTP Deflate body OOM leaked a logical allocation"
	);

	printf("[PASS] HTTP Deflate body OOM\n");
	return 0;
}

