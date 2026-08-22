#include "../test.h"



/* 可调失败点分配器用于扫描动态 Header 的所有分配边界。 */
typedef struct test_http_headers_allocator {
	size_t Calls;
	size_t FailAt;
	size_t Live;
} test_http_headers_allocator;



/* 在目标分配序号返回空指针，其余请求交给 C 运行库。 */
static ptr testHttpHeadersAlloc(ptr pContext, size_t iSize)
{
	test_http_headers_allocator* pState =
		(test_http_headers_allocator*)pContext;
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



/* 重分配失败保留原块，成功创建新块时更新存活计数。 */
static ptr testHttpHeadersRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_http_headers_allocator* pState =
		(test_http_headers_allocator*)pContext;
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



/* 释放成功分配的底层块并检查存活计数不下溢。 */
static void testHttpHeadersFree(ptr pContext, ptr pMemory)
{
	test_http_headers_allocator* pState =
		(test_http_headers_allocator*)pContext;

	if ( pMemory == NULL ) {
		return;
	}
	testRequire(pState->Live != 0,
		"HTTP Headers OOM live counter underflow");
	pState->Live--;
	free(pMemory);
}

/* 在一个失败点下执行扩容、别名 Set、压缩和 Clone，并验证失败原子性。 */
static bool testHttpHeadersOomAttempt(void)
{
	xhttpheaders* pHeaders = xrtHttpHeadersCreate(NULL);
	xhttpheaders* pClone = NULL;
	xhttpheaders* pTight = NULL;
	xhttpheadersconfig Config;
	const xhttpfield* pAlias;
	char Name[32];
	char Value[96];
	str sBuilt = NULL;
	size_t i;
	bool bComplete = false;

	if ( pHeaders == NULL ) {
		return false;
	}
	for ( i = 0; i < 40; i++ ) {
		size_t iCount = xrtHttpHeadersCount(pHeaders);
		size_t iBytes = xrtHttpHeadersBytes(pHeaders);
		int iName = snprintf(Name, sizeof(Name), "X-%u", (unsigned)i);
		int iValue = snprintf(Value, sizeof(Value),
			"value-%u-abcdefghijklmnopqrstuvwxyz", (unsigned)i);

		testRequire((iName > 0) && (iValue > 0),
			"HTTP Headers OOM fixture formatting failed");
		if ( !xrtHttpHeadersAdd(
			pHeaders,
			(xstrview){ Name, (size_t)iName },
			(xstrview){ Value, (size_t)iValue }
		) ) {
			testRequire((xrtHttpHeadersCount(pHeaders) == iCount) &&
				(xrtHttpHeadersBytes(pHeaders) == iBytes),
				"HTTP Headers Add OOM changed visible state");
			goto done;
		}
	}
	pAlias = xrtHttpHeadersAt(pHeaders, 20);
	{
		size_t iCount = xrtHttpHeadersCount(pHeaders);
		size_t iBytes = xrtHttpHeadersBytes(pHeaders);

		if ( !xrtHttpHeadersSet(
			pHeaders, XRT_STR_LITERAL("X-0"), pAlias->Value
		) ) {
			testRequire((xrtHttpHeadersCount(pHeaders) == iCount) &&
				(xrtHttpHeadersBytes(pHeaders) == iBytes),
				"HTTP Headers Set OOM changed visible state");
			goto done;
		}
	}
	for ( i = 1; i < 30; i++ ) {
		int iName = snprintf(Name, sizeof(Name), "X-%u", (unsigned)i);

		(void)xrtHttpHeadersRemove(
			pHeaders, (xstrview){ Name, (size_t)iName }
		);
	}
	{
		size_t iCount = xrtHttpHeadersCount(pHeaders);
		size_t iBytes = xrtHttpHeadersBytes(pHeaders);

		if ( !xrtHttpHeadersCompact(pHeaders) ) {
			testRequire((xrtHttpHeadersCount(pHeaders) == iCount) &&
				(xrtHttpHeadersBytes(pHeaders) == iBytes) &&
				(xrtHttpHeadersGet(
					pHeaders, XRT_STR_LITERAL("X-0")
				) != NULL),
				"HTTP Headers Compact OOM changed visible state");
			goto done;
		}
	}
	pClone = xrtHttpHeadersClone(pHeaders);
	if ( pClone == NULL ) {
		testRequire(xrtHttpHeadersGet(
			pHeaders, XRT_STR_LITERAL("X-0")
		) != NULL, "HTTP Headers Clone OOM changed source");
		goto done;
	}
	sBuilt = xrtHttpHeadersBuild(pHeaders, NULL);
	if ( sBuilt == NULL ) {
		goto done;
	}
	xrtFree(sBuilt);
	sBuilt = NULL;
	{
		size_t iCount = xrtHttpHeadersCount(pHeaders);

		if ( !xrtHttpHeadersAddBlock(
			pHeaders,
			XRT_STR_LITERAL("Extra: yes\r\n\r\n"),
			NULL
		) ) {
			testRequire((xrtHttpHeadersCount(pHeaders) == iCount) &&
				!xrtHttpHeadersHas(
					pHeaders, XRT_STR_LITERAL("Extra")
				), "HTTP Headers block OOM exposed partial append");
			goto done;
		}
	}

	/* 精确填满逻辑限额，覆盖 Set 最终态重排的独立 OOM 提交点。 */
	xrtHttpHeadersConfigInit(&Config);
	Config.InitialFields = 16;
	Config.InitialBytes = 1600;
	Config.MaxFields = 16;
	Config.MaxName = 4;
	Config.MaxValue = sizeof(Value);
	Config.MaxBytes = 1600;
	pTight = xrtHttpHeadersCreate(&Config);
	if ( pTight == NULL ) {
		goto done;
	}
	for ( i = 0; i < 16; i++ ) {
		int iName = snprintf(Name, sizeof(Name), "X-%02u", (unsigned)i);

		memset(Value, '0' + (int)(i % 10u), sizeof(Value));
		if ( !xrtHttpHeadersAdd(
			pTight,
			(xstrview){ Name, (size_t)iName },
			(xstrview){ Value, sizeof(Value) }
		) ) {
			goto done;
		}
	}
	memset(Value, 'z', sizeof(Value));
	if ( !xrtHttpHeadersSet(
		pTight,
		XRT_STR_LITERAL("X-00"),
		(xstrview){ Value, sizeof(Value) }
	) ) {
		const xhttpfield* pOriginal = xrtHttpHeadersAt(pTight, 0);

		testRequire((xrtHttpHeadersCount(pTight) == 16) &&
			(xrtHttpHeadersBytes(pTight) == Config.MaxBytes) &&
			(pOriginal != NULL) && (pOriginal->Value.Data[0] == '0'),
			"HTTP Headers Set repack OOM exposed partial state");
		goto done;
	}
	bComplete = true;

done:
	xrtFree(sBuilt);
	xrtHttpHeadersDestroy(pTight);
	xrtHttpHeadersDestroy(pClone);
	xrtHttpHeadersDestroy(pHeaders);
	xrtClearError();
	return bComplete;
}



/* 扫描每个分配序号，要求既命中失败路径也最终完整成功且不泄漏。 */
int main(void)
{
	test_http_headers_allocator State = { 0, 0, 0 };
	xallocator Allocator;
	size_t iBaseline;
	size_t iFail;
	size_t iFailures = 0;
	bool bSuccess = false;

	Allocator.Context = &State;
	Allocator.Alloc = testHttpHeadersAlloc;
	Allocator.Realloc = testHttpHeadersRealloc;
	Allocator.Free = testHttpHeadersFree;
	testRequire(xrtSetAllocator(&Allocator),
		"HTTP Headers OOM allocator install failed");

	/* 预热成功路径，使小对象堆的常驻 size-class span 进入稳定基线。 */
	testRequire(testHttpHeadersOomAttempt(),
		"HTTP Headers OOM warm-up failed");
	testMemoryDebugDrain(
		"HTTP Headers OOM memory debug reset failed"
	);
	iBaseline = State.Live;

	for ( iFail = 1; iFail <= 160; iFail++ ) {
		State.Calls = 0;
		State.FailAt = iFail;
		if ( testHttpHeadersOomAttempt() ) {
			bSuccess = true;
		} else {
			iFailures++;
		}
		testMemoryDebugDrain(
			"HTTP Headers OOM memory debug reset failed"
		);
		testRequire(State.Live == iBaseline,
			"HTTP Headers OOM attempt leaked storage");
	}
	testRequire((iFailures != 0) && bSuccess,
		"HTTP Headers OOM sweep missed failure or success paths");
	printf("[PASS] http_headers_oom\n");
	return 0;
}

