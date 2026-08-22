#include "../test.h"



/* 记录自定义底层分配器的调用次数。 */
typedef struct test_allocator_state {
	size_t AllocCount;
	size_t ReallocCount;
	size_t FreeCount;
} test_allocator_state;



/* 测试分配器的分配回调。 */
static ptr testAlloc(ptr pContext, size_t iSize)
{
	test_allocator_state* pState = (test_allocator_state*)pContext;

	pState->AllocCount++;
	return malloc(iSize);
}



/* 测试分配器的重分配回调。 */
static ptr testRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	test_allocator_state* pState = (test_allocator_state*)pContext;

	pState->ReallocCount++;
	return realloc(pMemory, iSize);
}



/* 测试分配器的释放回调。 */
static void testFree(ptr pContext, ptr pMemory)
{
	test_allocator_state* pState = (test_allocator_state*)pContext;

	pState->FreeCount++;
	free(pMemory);
}



/* 验证分配边界、所有权和分配器冻结契约。 */
int main(void)
{
	test_allocator_state tState;
	xallocator tAllocator;
	unsigned char* pMemory;
	unsigned char* pCopy;
	const unsigned char arrSource[] = { 1, 2, 3, 4 };
	const void* pLast = (const void*)(uintptr_t)(UINTPTR_MAX - 1u);

	testRequire(xrtMemRangeValid(NULL, 0u), "empty memory range must be valid");
	testRequire(!xrtMemRangeValid(NULL, 1u), "non-empty null range must be invalid");
	testRequire(
		xrtMemRangeValid(arrSource, sizeof(arrSource)),
		"ordinary memory range must be valid"
	);
	testRequire(xrtMemRangeValid(pLast, 1u), "last non-wrapping range must be valid");
	testRequire(!xrtMemRangeValid(pLast, 2u), "wrapping memory range must be invalid");
	testRequire(
		xrtMemRangesOverlap(arrSource, 3u, arrSource + 2u, 2u),
		"intersecting memory ranges must overlap"
	);
	testRequire(
		!xrtMemRangesOverlap(arrSource, 2u, arrSource + 2u, 2u),
		"adjacent memory ranges must not overlap"
	);
	testRequire(
		!xrtMemRangesOverlap(arrSource, 0u, arrSource, 1u),
		"empty memory range must not overlap"
	);

	memset(&tState, 0, sizeof(tState));
	tAllocator.Context = &tState;
	tAllocator.Alloc = testAlloc;
	tAllocator.Realloc = testRealloc;
	tAllocator.Free = testFree;
	testRequire(xrtSetAllocator(&tAllocator), "custom allocator should install before first allocation");

	pMemory = (unsigned char*)xrtCalloc(1, 2048);
	testRequire(pMemory != NULL, "calloc failed");
	for ( size_t i = 0; i < 2048; i++ ) {
		testRequire(pMemory[i] == 0, "calloc did not clear memory");
	}
	pMemory[0] = 0x5A;
	pMemory = (unsigned char*)xrtRealloc(pMemory, 4096);
	testRequire((pMemory != NULL) && (pMemory[0] == 0x5A), "realloc did not preserve memory");
	testRequire(xrtRealloc(pMemory, 0) == NULL, "zero-size realloc must release memory");

	pCopy = (unsigned char*)xrtMemDup(arrSource, sizeof(arrSource));
	testRequire(pCopy != NULL, "memory duplicate failed");
	testRequire(memcmp(pCopy, arrSource, sizeof(arrSource)) == 0, "memory duplicate mismatch");
	xrtFree(pCopy);

	testRequire(xrtCalloc(SIZE_MAX, 2) == NULL, "overflowing calloc must fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE, "overflow must report range error");
	xrtClearError();
	testRequire(!xrtSetAllocator(&tAllocator), "allocator replacement after allocation must fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "late allocator replacement must report state error");

	#if defined(XRT_FEATURE_MEMORY_DEBUG)
		xrtClearError();
		testRequire(xrtMemDebugReset(), "debug quarantine reset failed");
		testRequire(tState.ReallocCount == 0, "debug realloc must preserve the old block in quarantine");
	#else
		testRequire(tState.ReallocCount == 1, "custom allocator realloc count mismatch");
	#endif
	testRequire(tState.AllocCount >= 2, "custom allocator allocation count mismatch");
	testRequire(tState.FreeCount >= 1, "custom allocator free count mismatch");
	xrtClearError();
	printf("[PASS] memory\n");
	return 0;
}
