#include "../test.h"



/* 可切换分配器用于在已有数组上注入后续分配失败。 */
typedef struct testoomstate {
	bool Fail;
} testoomstate;



/* 大元素用于验证自引用插入不依赖临时副本。 */
typedef struct testoomlarge {
	unsigned char Data[2048];
} testoomlarge;



/* 在允许状态下转发到底层 C 分配器。 */
static ptr testOomAlloc(ptr pContext, size_t iSize)
{
	testoomstate* pState = (testoomstate*)pContext;

	return pState->Fail ? NULL : malloc(iSize);
}



/* 在允许状态下转发到底层 C 重分配器。 */
static ptr testOomRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	testoomstate* pState = (testoomstate*)pContext;

	return pState->Fail ? NULL : realloc(pMemory, iSize);
}



/* 释放测试分配器取得的内存。 */
static void testOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 验证扩容 OOM 不改变数组可见状态和原始数据。 */
int main(void)
{
	testoomstate tState = { false };
	xallocator tAllocator;
	xarray tArray;
	xarray tAligned;
	xarray tAlias;
	bytes pData;
	size_t iCount;
	size_t iCapacity;
	int pValues[] = { 10, 20, 30, 40 };
	testoomlarge pLarge[2];

	tAllocator.Context = &tState;
	tAllocator.Alloc = testOomAlloc;
	tAllocator.Realloc = testOomRealloc;
	tAllocator.Free = testOomFree;
	testRequire(xrtSetAllocator(&tAllocator), "failed to install toggle allocator");
	testRequire(xrtArrayInit(&tArray, sizeof(int)), "OOM array init failed");
	testRequire(xrtArrayAppend(&tArray, pValues, 4), "OOM array setup failed");
	pData = tArray.Data;
	iCount = tArray.Count;
	iCapacity = tArray.Capacity;

	tState.Fail = true;
	testRequire(!xrtArrayReserve(&tArray, 1024u * 1024u), "reserve should fail under OOM");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "array OOM error kind mismatch");
	testRequire(
		(tArray.Data == pData) &&
		(tArray.Count == iCount) &&
		(tArray.Capacity == iCapacity),
		"array OOM changed visible state"
	);
	for ( size_t i = 0; i < iCount; i++ ) {
		testRequire(*(int*)xrtArrayGet(&tArray, i) == pValues[i], "array OOM damaged data");
	}

	tState.Fail = false;
	xrtArrayUnit(&tArray);

	/* 过对齐扩容失败也必须保留原始地址、容量和内容。 */
	testRequire(xrtArrayInitAligned(&tAligned, 64, 64), "aligned OOM array init failed");
	testRequire(xrtArrayResize(&tAligned, 2), "aligned OOM array setup failed");
	memset(tAligned.Data, 0x5a, tAligned.Count * tAligned.ItemSize);
	pData = tAligned.Data;
	iCapacity = tAligned.Capacity;
	tState.Fail = true;
	testRequire(!xrtArrayReserve(&tAligned, 1024u * 1024u), "aligned reserve should fail under OOM");
	testRequire(
		(tAligned.Data == pData) &&
		(tAligned.Count == 2) &&
		(tAligned.Capacity == iCapacity) &&
		(tAligned.Data[0] == 0x5a),
		"aligned array OOM changed visible state"
	);
	tState.Fail = false;
	xrtArrayUnit(&tAligned);

	/* 已有容量足够时，自引用插入不应产生任何额外分配。 */
	memset(pLarge, 0x3c, sizeof(pLarge));
	testRequire(xrtArrayInit(&tAlias, sizeof(testoomlarge)), "alias OOM array init failed");
	testRequire(xrtArrayAppend(&tAlias, pLarge, 2), "alias OOM array setup failed");
	iCapacity = tAlias.Capacity;
	tState.Fail = true;
	testRequire(
		xrtArrayInsert(&tAlias, 1, xrtArrayGet(&tAlias, 0), 1),
		"self insert with spare capacity should not allocate"
	);
	testRequire(
		(tAlias.Count == 3) &&
		(tAlias.Capacity == iCapacity) &&
		(tAlias.Data[0] == 0x3c),
		"allocation-free self insert result mismatch"
	);

	/* 必须扩容时，失败仍应保留地址、数量、容量和内容。 */
	tState.Fail = false;
	testRequire(
		xrtArrayResize(&tAlias, tAlias.Capacity),
		"alias growth OOM setup failed"
	);
	pData = tAlias.Data;
	iCapacity = tAlias.Capacity;
	tState.Fail = true;
	testRequire(
		!xrtArrayInsert(&tAlias, 1, xrtArrayGet(&tAlias, 0), 1),
		"self insert should fail when required growth allocation fails"
	);
	testRequire(
		(tAlias.Data == pData) &&
		(tAlias.Count == iCapacity) &&
		(tAlias.Capacity == iCapacity) &&
		(tAlias.Data[0] == 0x3c),
		"growing self insert OOM changed visible state"
	);
	tState.Fail = false;
	xrtArrayUnit(&tAlias);
	printf("[PASS] array OOM\n");
	return 0;
}
