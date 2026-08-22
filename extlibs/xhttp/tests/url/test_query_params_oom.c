#include "../test.h"



/* 可调失败点分配器扫描 QueryParams 的全部拥有型路径。 */
typedef struct test_query_params_allocator {
	size_t Calls;
	size_t FailAt;
	size_t Live;
} test_query_params_allocator;



/* 在目标分配序号失败，其余请求交给 C 运行库。 */
static ptr testQueryParamsAlloc(ptr pContext, size_t iSize)
{
	test_query_params_allocator* pState =
		(test_query_params_allocator*)pContext;
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



/* 重分配使用同一失败序号并保持失败时原块有效。 */
static ptr testQueryParamsRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_query_params_allocator* pState =
		(test_query_params_allocator*)pContext;
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



/* 释放成功分配的底层内存并维护存活计数。 */
static void testQueryParamsFree(ptr pContext, ptr pMemory)
{
	test_query_params_allocator* pState =
		(test_query_params_allocator*)pContext;

	if ( pMemory == NULL ) {
		return;
	}
	testRequire(pState->Live != 0,
		"query params OOM live counter underflow");
	pState->Live--;
	free(pMemory);
}



/* 验证多字段解析按容量增长分配，而不是为每个字段建立临时缓冲。 */
static void testQueryParamsParseAllocations(
	test_query_params_allocator* pState
)
{
	char Text[512];
	xqueryparams* pParams;
	size_t iBaseline = pState->Live;
	size_t iOffset = 0;
	size_t iError;
	size_t i;

	for ( i = 0; i < 32u; i++ ) {
		int iWritten = snprintf(
			Text + iOffset, sizeof(Text) - iOffset,
			"%sp%02u=%u", i != 0 ? "&" : "",
			(unsigned)i, (unsigned)i
		);

		testRequire((iWritten > 0) &&
			((size_t)iWritten < (sizeof(Text) - iOffset)),
			"query params allocation fixture overflow");
		iOffset += (size_t)iWritten;
	}
	pState->Calls = 0;
	pState->FailAt = 0;
	pParams = xrtQueryParamsParse(
		(xstrview){ Text, iOffset }, NULL, &iError
	);
	testRequire((pParams != NULL) && (iError == iOffset) &&
		(xrtQueryParamsCount(pParams) == 32u),
		"query params allocation parse failed");
	testRequire(pState->Calls <= 6u,
		"query params parse allocated per field");
	xrtQueryParamsDestroy(pParams);
	testMemoryDebugDrain(
		"query params allocation memory debug reset failed"
	);
	testRequire(pState->Live == iBaseline,
		"query params allocation test leaked storage");
}



/* 在一个失败点下验证创建、别名增长、解析提交、压缩、排序和构建。 */
static bool testQueryParamsOomAttempt(void)
{
	xqueryparams* pParams = xrtQueryParamsCreate(NULL);
	xqueryparams* pParsed = NULL;
	xqueryparams* pClone = NULL;
	xquerypair Pair;
	str sBuilt = NULL;
	size_t iError;
	size_t i;
	bool bComplete = false;

	if ( pParams == NULL ) {
		return false;
	}
	for ( i = 0; i < 48u; i++ ) {
		char Name[24];
		char Value[48];
		int iName = snprintf(Name, sizeof(Name), "p%u", (unsigned)i);
		int iValue = snprintf(
			Value, sizeof(Value), "value-%u-abcdefghijklmnopqrstuvwxyz",
			(unsigned)i
		);
		size_t iCount = xrtQueryParamsCount(pParams);
		size_t iBytes = xrtQueryParamsBytes(pParams);

		if ( !xrtQueryParamsAppend(
			pParams,
			(xstrview){ Name, (size_t)iName },
			(xstrview){ Value, (size_t)iValue }
		) ) {
			testRequire((xrtQueryParamsCount(pParams) == iCount) &&
				(xrtQueryParamsBytes(pParams) == iBytes),
				"query params append OOM changed visible state");
			goto done;
		}
	}
	if ( !xrtQueryParamsAt(pParams, 20u, &Pair) ) {
		goto done;
	}
	{
		size_t iCount = xrtQueryParamsCount(pParams);
		size_t iBytes = xrtQueryParamsBytes(pParams);

		if ( !xrtQueryParamsSetPair(pParams, Pair) ) {
			testRequire((xrtQueryParamsCount(pParams) == iCount) &&
				(xrtQueryParamsBytes(pParams) == iBytes),
				"query params alias Set OOM changed visible state");
			goto done;
		}
	}
	{
		size_t iCount = xrtQueryParamsCount(pParams);

		if ( !xrtQueryParamsParseAppend(
			pParams,
			XRT_STR_LITERAL("extra=one&extra=two&space=a+b"),
			&iError
		) ) {
			testRequire((xrtQueryParamsCount(pParams) == iCount) &&
				!xrtQueryParamsHas(
					pParams, XRT_STR_LITERAL("extra")
				), "query params parse OOM exposed partial append");
			goto done;
		}
	}
	for ( i = 0; i < 30u; i++ ) {
		char Name[24];
		int iName = snprintf(Name, sizeof(Name), "p%u", (unsigned)i);

		(void)xrtQueryParamsRemove(
			pParams, (xstrview){ Name, (size_t)iName }
		);
	}
	if ( !xrtQueryParamsCompact(pParams) ) {
		testRequire(xrtQueryParamsHas(
			pParams, XRT_STR_LITERAL("extra")
		), "query params compact OOM changed visible state");
		goto done;
	}
	if ( !xrtQueryParamsSort(pParams) ) {
		testRequire(xrtQueryParamsHas(
			pParams, XRT_STR_LITERAL("extra")
		), "query params sort OOM changed visible state");
		goto done;
	}
	pClone = xrtQueryParamsClone(pParams);
	if ( pClone == NULL ) {
		goto done;
	}
	sBuilt = xrtQueryParamsBuild(pClone, NULL);
	if ( sBuilt == NULL ) {
		goto done;
	}
	pParsed = xrtQueryParamsParse(
		(xstrview){ sBuilt, strlen(sBuilt) }, NULL, &iError
	);
	if ( pParsed == NULL ) {
		goto done;
	}
	bComplete = true;

done:
	xrtQueryParamsDestroy(pParsed);
	xrtFree(sBuilt);
	xrtQueryParamsDestroy(pClone);
	xrtQueryParamsDestroy(pParams);
	xrtClearError();
	return bComplete;
}



/* 扫描每个分配序号并要求失败路径、成功路径和底层释放都成立。 */
int main(void)
{
	test_query_params_allocator State = { 0, 0, 0 };
	xallocator Allocator = {
		&State,
		testQueryParamsAlloc,
		testQueryParamsRealloc,
		testQueryParamsFree
	};
	size_t iBaseline;
	size_t iFail;
	size_t iFailures = 0;
	bool bSuccess = false;

	testRequire(xrtSetAllocator(&Allocator),
		"query params OOM allocator install failed");
	testRequire(testQueryParamsOomAttempt(),
		"query params OOM warm-up failed");
	testMemoryDebugDrain(
		"query params OOM memory debug reset failed"
	);
	iBaseline = State.Live;
	testQueryParamsParseAllocations(&State);
	for ( iFail = 1; iFail <= 220u; iFail++ ) {
		State.Calls = 0;
		State.FailAt = iFail;
		if ( testQueryParamsOomAttempt() ) {
			bSuccess = true;
		} else {
			iFailures++;
		}
		testMemoryDebugDrain(
			"query params OOM memory debug reset failed"
		);
		testRequire(State.Live == iBaseline,
			"query params OOM attempt leaked storage");
	}
	testRequire((iFailures != 0) && bSuccess,
		"query params OOM sweep missed failure or success paths");
	printf("[PASS] query_params_oom\n");
	return 0;
}
