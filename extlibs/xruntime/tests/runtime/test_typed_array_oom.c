#include "../test.h"



typedef struct testtypedarrayoom {
	bool Fail;
} testtypedarrayoom;



/* 按开关分配类型数组测试内存。 */
static ptr testTypedArrayOomAlloc(ptr pContext, size_t iSize)
{
	testtypedarrayoom* pState = (testtypedarrayoom*)pContext;

	return pState->Fail ? NULL : malloc(iSize);
}



/* 按开关重分配类型数组测试内存。 */
static ptr testTypedArrayOomRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	testtypedarrayoom* pState = (testtypedarrayoom*)pContext;

	return pState->Fail ? NULL : realloc(pMemory, iSize);
}



/* 释放类型数组测试内存。 */
static void testTypedArrayOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 验证扩容 OOM 保留数组地址、数量、容量和元素。 */
int main(void)
{
	testtypedarrayoom State = { false };
	xallocator Allocator = {
		&State,
		testTypedArrayOomAlloc,
		testTypedArrayOomRealloc,
		testTypedArrayOomFree
	};
	xtypedarray Array;
	xtypedarray Other;
	xtypedarray* pConcat;
	bytes pData;
	size_t iCount;
	size_t iCapacity;
	size_t iConcatCount = 1024u * 1024u;
	int64 iValue = 37;

	testRequire(xrtSetAllocator(&Allocator), "typed array OOM allocator install failed");
	testRequire(
		xrtTypedArrayInit(&Array, xrtTypeInt64()) &&
		xrtTypedArrayInit(&Other, xrtTypeInt64()) &&
		xrtTypedArrayPush(&Array, &iValue) &&
		xrtTypedArrayPush(&Other, &iValue),
		"typed array OOM fixture failed"
	);
	pData = Array.Storage.Data;
	iCount = Array.Storage.Count;
	iCapacity = Array.Storage.Capacity;
	State.Fail = true;
	xrtClearError();
	testRequire(
		!xrtTypedArrayReserve(&Array, 1024u * 1024u),
		"typed array reserve survived forced OOM"
	);
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(Array.Storage.Data == pData) &&
		(Array.Storage.Count == iCount) &&
		(Array.Storage.Capacity == iCapacity) &&
		(*(const int64*)xrtTypedArrayConstGet(&Array, 0u) == 37),
		"typed array OOM changed visible state"
	);
	State.Fail = false;
	xrtClearError();
	testRequire(
		xrtTypedArrayResize(&Array, iConcatCount),
		"typed array concat OOM fixture resize failed"
	);
	State.Fail = true;
	xrtClearError();
	pConcat = xrtTypedArrayConcat(&Array, &Other);
	testRequire(
		pConcat == NULL,
		"typed array concat survived forced OOM"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"typed array concat did not preserve the OOM error"
	);
	testRequire(
		(xrtTypedArrayCount(&Array) == iConcatCount) &&
		(xrtTypedArrayCount(&Other) == 1u),
		"typed array concat OOM changed an input array"
	);
	State.Fail = false;
	xrtTypedArrayUnit(&Other);
	xrtTypedArrayUnit(&Array);
	xrtClearError();
	printf("[PASS] typed array OOM\n");
	return 0;
}
