#include "../test.h"



/* 可调故障点分配器检查组合工厂、正文和 Reader 的回滚路径。 */
typedef struct test_http_body_compose_allocator {
	size_t Calls;
	size_t FailAt;
	size_t Live;
} test_http_body_compose_allocator;



/* 在指定分配序号失败，其余请求交给 C 运行库。 */
static ptr testHttpBodyComposeAlloc(ptr pContext, size_t iSize)
{
	test_http_body_compose_allocator* pState =
		(test_http_body_compose_allocator*)pContext;
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



/* 组合正文不依赖重分配，仍提供完整分配器契约。 */
static ptr testHttpBodyComposeRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_http_body_compose_allocator* pState =
		(test_http_body_compose_allocator*)pContext;
	ptr pResult;

	pState->Calls++;
	if ( pState->Calls == pState->FailAt ) {
		return NULL;
	}
	pResult = realloc(pMemory, iSize);
	if ( (pResult != NULL) && (pMemory == NULL) ) {
		pState->Live++;
	}
	return pResult;
}



/* 回收底层分配并维护存活计数。 */
static void testHttpBodyComposeFree(ptr pContext, ptr pMemory)
{
	test_http_body_compose_allocator* pState =
		(test_http_body_compose_allocator*)pContext;

	if ( pMemory == NULL ) {
		return;
	}
	testRequire(pState->Live != 0,
		"HTTP body compose OOM live counter underflow");
	pState->Live--;
	free(pMemory);
}



/* 在一个故障点下走完创建、引用、打开、分段读取和销毁。 */
static bool testHttpBodyComposeOomAttempt(void)
{
	static const uint8 LargeBytes[4096] = { 0 };
	xhttpbody* pFirst = NULL;
	xhttpbody* pSecond = NULL;
	xhttpbody* pBody = NULL;
	xhttpbodyreader* pReader = NULL;
	xhttpbodypiece Pieces[4];
	xhttpbodychunk Chunk;
	xhttpbodystatus Status;
	bool bComplete = false;

	pFirst = xrtHttpBodyBorrow(
		(xbytesview){ (cbytes)"first", 5 }
	);
	if ( pFirst == NULL ) {
		goto done;
	}
	pSecond = xrtHttpBodyBorrow(
		(xbytesview){ (cbytes)"second", 6 }
	);
	if ( pSecond == NULL ) {
		goto done;
	}
	Pieces[0] = xrtHttpBodyPieceBytes(
		(xbytesview){ LargeBytes, sizeof(LargeBytes) }
	);
	Pieces[1] = xrtHttpBodyPieceBody(pFirst);
	Pieces[2] = xrtHttpBodyPieceBytes(
		(xbytesview){ (cbytes)":", 1 }
	);
	Pieces[3] = xrtHttpBodyPieceBody(pSecond);
	pBody = xrtHttpBodyCompose(Pieces, 4);
	if ( pBody == NULL ) {
		goto done;
	}
	xrtHttpBodyDestroy(pFirst);
	pFirst = NULL;
	xrtHttpBodyDestroy(pSecond);
	pSecond = NULL;
	pReader = xrtHttpBodyOpen(pBody);
	if ( pReader == NULL ) {
		goto done;
	}
	for ( ;; ) {
		Status = xrtHttpBodyNext(
			pReader, sizeof(LargeBytes), &Chunk
		);
		if ( Status == XHTTP_BODY_EOF ) {
			break;
		}
		if ( Status != XHTTP_BODY_DATA ) {
			goto done;
		}
		xrtHttpBodyChunkRelease(&Chunk);
	}
	bComplete = true;

done:
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	xrtHttpBodyDestroy(pSecond);
	xrtHttpBodyDestroy(pFirst);
	xrtClearError();
	return bComplete;
}



/* 扫描每个分配序号，要求失败与成功路径都不泄漏。 */
int main(void)
{
	test_http_body_compose_allocator State = { 0, 0, 0 };
	xallocator Allocator = {
		&State,
		testHttpBodyComposeAlloc,
		testHttpBodyComposeRealloc,
		testHttpBodyComposeFree
	};
	size_t iBaseline;
	size_t iFail;
	size_t iFailures = 0;
	bool bSuccess = false;

	testRequire(xrtSetAllocator(&Allocator),
		"HTTP body compose OOM allocator install failed");
	testRequire(testHttpBodyComposeOomAttempt(),
		"HTTP body compose OOM warm-up failed");
	testMemoryDebugDrain(
		"HTTP body compose OOM warm-up reset failed"
	);
	iBaseline = State.Live;
	for ( iFail = 1; iFail <= 48u; iFail++ ) {
		State.Calls = 0;
		State.FailAt = iFail;
		if ( testHttpBodyComposeOomAttempt() ) {
			bSuccess = true;
		} else {
			iFailures++;
		}
		testMemoryDebugDrain(
			"HTTP body compose OOM reset failed"
		);
		testRequire(State.Live == iBaseline,
			"HTTP body compose OOM attempt leaked storage");
	}
	testRequire((iFailures != 0) && bSuccess,
		"HTTP body compose OOM missed failure or success paths");
	printf("[PASS] HTTP body compose OOM\n");
	return 0;
}

