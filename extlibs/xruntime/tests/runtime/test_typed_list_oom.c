#include "../test.h"



typedef struct testtypedlistoom {
	bool Fail;
} testtypedlistoom;



/* 按开关分配类型列表测试内存。 */
static ptr testTypedListOomAlloc(ptr pContext, size_t iSize)
{
	testtypedlistoom* pState = (testtypedlistoom*)pContext;

	return pState->Fail ? NULL : malloc(iSize);
}



/* 按开关重分配类型列表测试内存。 */
static ptr testTypedListOomRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	testtypedlistoom* pState = (testtypedlistoom*)pContext;

	return pState->Fail ? NULL : realloc(pMemory, iSize);
}



/* 释放类型列表测试内存。 */
static void testTypedListOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 验证节点页 OOM 不提交键且保留全部已有值。 */
int main(void)
{
	testtypedlistoom State = { false };
	xallocator Allocator = {
		&State,
		testTypedListOomAlloc,
		testTypedListOomRealloc,
		testTypedListOomFree
	};
	xtypedlist List;
	int64 iValue;

	testRequire(xrtSetAllocator(&Allocator), "typed list OOM allocator install failed");
	testRequire(
		xrtTypedListInit(&List, xrtTypeInt64()),
		"typed list OOM init failed"
	);
	State.Fail = true;
	xrtClearError();
	iValue = 7;
	testRequire(
		!xrtTypedListSet(&List, 0, &iValue) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtTypedListCount(&List) == 0u),
		"typed list first-node OOM changed visible state"
	);
	State.Fail = false;
	for ( int64 i = 0; i < (int64)XRT_POOL_PAGE_CAPACITY; i++ ) {
		iValue = i;
		testRequire(
			xrtTypedListSet(&List, i, &iValue),
			"typed list OOM page fill failed"
		);
	}
	State.Fail = true;
	xrtClearError();
	iValue = 99;
	testRequire(
		!xrtTypedListSet(
			&List, (int64)XRT_POOL_PAGE_CAPACITY, &iValue
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtTypedListCount(&List) == XRT_POOL_PAGE_CAPACITY) &&
		!xrtTypedListHas(&List, (int64)XRT_POOL_PAGE_CAPACITY) &&
		(*(int64*)xrtTypedListGet(&List, 0) == 0),
		"typed list new-page OOM changed keys or values"
	);
	State.Fail = false;
	xrtTypedListUnit(&List);
	xrtClearError();
	printf("[PASS] typed list OOM\n");
	return 0;
}
