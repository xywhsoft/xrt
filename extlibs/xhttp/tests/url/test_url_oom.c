#include "../test.h"



/* 可持续或按序号注入底层失败，用于覆盖分配型 URL Helper。 */
typedef struct test_url_oom {
	size_t Calls;
	size_t FailAt;
	size_t Live;
	bool FailAll;
} test_url_oom;



/* 在全拒绝模式或指定序号失败，其余请求交给 C 运行库。 */
static ptr testUrlOomAlloc(ptr pContext, size_t iSize)
{
	test_url_oom* pState = (test_url_oom*)pContext;
	ptr pMemory;

	pState->Calls++;
	if ( pState->FailAll || (pState->Calls == pState->FailAt) ) {
		return NULL;
	}
	pMemory = malloc(iSize);
	if ( pMemory != NULL ) {
		pState->Live++;
	}
	return pMemory;
}



/* 重分配使用相同的全拒绝模式和故障序号。 */
static ptr testUrlOomRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_url_oom* pState = (test_url_oom*)pContext;
	ptr pResult;

	pState->Calls++;
	if ( pState->FailAll || (pState->Calls == pState->FailAt) ) {
		return NULL;
	}
	pResult = realloc(pMemory, iSize);
	if ( (pResult != NULL) && (pMemory == NULL) ) {
		pState->Live++;
	}
	return pResult;
}



/* 释放成功的底层分配。 */
static void testUrlOomFree(ptr pContext, ptr pMemory)
{
	test_url_oom* pState = (test_url_oom*)pContext;

	if ( pMemory == NULL ) {
		return;
	}
	testRequire(pState->Live != 0, "URL OOM live counter underflow");
	pState->Live--;
	free(pMemory);
}



/* 排空调试隔离队列后，检查所有底层存储均已归还。 */
static void testUrlOomRequireReleased(
	test_url_oom* pState,
	cstr sMessage
)
{
	testMemoryDebugDrain("URL OOM memory debug reset failed");
	testRequire(pState->Live == 0, sMessage);
}



/* 首次构建 OOM 必须返回空结果，恢复后所有分配型路径仍可使用。 */
int main(void)
{
	static test_url_oom State = { 0, SIZE_MAX, 0, true };
	xallocator Allocator = {
		&State, testUrlOomAlloc, testUrlOomRealloc, testUrlOomFree
	};
	xurl Base;
	char LongPath[4096];
	str sOutput;
	size_t iCalls;
	size_t iSize = SIZE_MAX;

	testRequire(xrtSetAllocator(&Allocator),
		"URL OOM allocator install failed");
	testRequire(xrtUrlParse(
		XRT_STR_LITERAL("http://example.test/a/b?q"), &Base
	), "URL OOM fixture parse failed");
	sOutput = xrtUrlBuild(&Base, &iSize);
	testRequire((sOutput == NULL) && (iSize == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"URL build OOM result mismatch");
	State.FailAll = false;
	xrtClearError();
	testUrlOomRequireReleased(&State, "URL build OOM leaked storage");

	memset(LongPath, 'a', sizeof(LongPath));
	State.FailAt = State.Calls + 1u;
	iSize = SIZE_MAX;
	sOutput = xrtUrlPathNormalizeBuild(
		(xstrview){ LongPath, sizeof(LongPath) }, &iSize
	);
	testRequire((sOutput == NULL) && (iSize == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"URL normalize OOM result mismatch");
	xrtClearError();
	testUrlOomRequireReleased(&State, "URL normalize OOM leaked storage");

	State.FailAt = State.Calls + 1u;
	iSize = SIZE_MAX;
	sOutput = xrtUrlResolveBuild(
		&Base, (xstrview){ LongPath, sizeof(LongPath) }, &iSize
	);
	testRequire((sOutput == NULL) && (iSize == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"URL resolve final-allocation OOM result mismatch");
	xrtClearError();
	testUrlOomRequireReleased(
		&State,
		"URL resolve final-allocation OOM leaked storage"
	);

	State.FailAt = SIZE_MAX;
	iCalls = State.Calls;
	sOutput = xrtUrlResolveBuild(
		&Base, (xstrview){ LongPath, sizeof(LongPath) }, &iSize
	);
	testRequire((sOutput != NULL) &&
		(State.Calls == (iCalls + 1u)),
		"URL resolve build used intermediate allocations");
	xrtFree(sOutput);
	sOutput = xrtUrlBuild(&Base, &iSize);
	testRequire((sOutput != NULL) &&
		(iSize == strlen("http://example.test/a/b?q")),
		"URL build did not recover after OOM");
	xrtFree(sOutput);
	sOutput = xrtUrlPathNormalizeBuild(
		XRT_STR_LITERAL("/a/b/../c"), &iSize
	);
	testRequire((sOutput != NULL) && (iSize == strlen("/a/c")) &&
		(memcmp(sOutput, "/a/c", iSize + 1u) == 0),
		"URL normalize build recovery mismatch");
	xrtFree(sOutput);
	sOutput = xrtUrlResolveBuild(
		&Base, XRT_STR_LITERAL("../next"), &iSize
	);
	testRequire((sOutput != NULL) &&
		(iSize == strlen("http://example.test/next")) &&
		(memcmp(sOutput, "http://example.test/next", iSize + 1u) == 0),
		"URL resolve build recovery mismatch");
	xrtFree(sOutput);
	printf("[PASS] url_oom\n");
	return 0;
}
