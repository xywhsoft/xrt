#include "../test.h"



typedef struct testtypedsetoom {
	bool Enabled;
	size_t Remaining;
} testtypedsetoom;



/* 按倒计数故障点分配类型集合测试内存。 */
static ptr testTypedSetOomAlloc(ptr pContext, size_t iSize)
{
	testtypedsetoom* pState = (testtypedsetoom*)pContext;

	if ( pState->Enabled ) {
		if ( pState->Remaining == 0u ) {
			return NULL;
		}
		pState->Remaining--;
	}
	return malloc(iSize);
}



/* 按倒计数故障点重分配类型集合测试内存。 */
static ptr testTypedSetOomRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	testtypedsetoom* pState = (testtypedsetoom*)pContext;

	if ( pState->Enabled ) {
		if ( pState->Remaining == 0u ) {
			return NULL;
		}
		pState->Remaining--;
	}
	return realloc(pMemory, iSize);
}



/* 释放类型集合测试内存。 */
static void testTypedSetOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 验证插入 OOM 不提交元素。 */
static void testTypedSetInsertOom(testtypedsetoom* pState)
{
	xtypedset Set;
	int64 iValue = 7;

	testRequire(
		xrtTypedSetInit(&Set, xrtTypeInt64()),
		"typed set OOM init failed"
	);
	pState->Enabled = true;
	pState->Remaining = 0u;
	xrtClearError();
	testRequire(
		!xrtTypedSetAdd(&Set, &iValue) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtTypedSetCount(&Set) == 0u),
		"typed set insertion OOM changed visible state"
	);
	pState->Enabled = false;
	xrtTypedSetUnit(&Set);
}



/* 扫描克隆的每个分配故障点并验证来源保持不变。 */
static void testTypedSetCloneOom(testtypedsetoom* pState)
{
	xtypedset Set;
	xtypedset* pClone;
	int64 iOne = 1;
	int64 iTwo = 2;
	bool bObservedFailure = false;
	bool bObservedSuccess = false;

	testRequire(
		xrtTypedSetInit(&Set, xrtTypeInt64()) &&
		xrtTypedSetAdd(&Set, &iOne) &&
		xrtTypedSetAdd(&Set, &iTwo),
		"typed set clone OOM fixture failed"
	);
	for ( size_t i = 0u; i < 16u; i++ ) {
		pState->Enabled = true;
		pState->Remaining = i;
		xrtClearError();
		pClone = xrtTypedSetClone(&Set);
		pState->Enabled = false;
		if ( pClone == NULL ) {
			bObservedFailure = true;
			testRequire(
				(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
				(xrtTypedSetCount(&Set) == 2u) &&
				xrtTypedSetHas(&Set, &iOne) &&
				xrtTypedSetHas(&Set, &iTwo),
				"typed set clone OOM changed its source"
			);
		} else {
			bObservedSuccess = true;
			testRequire(
				xrtTypedSetEquals(&Set, pClone),
				"typed set clone after OOM scan mismatched"
			);
			xrtTypedSetDestroy(pClone);
			break;
		}
	}
	testRequire(
		bObservedFailure && bObservedSuccess,
		"typed set clone OOM scan missed failure or success"
	);
	xrtTypedSetUnit(&Set);
}



/* 运行类型集合分配失败测试。 */
int main(void)
{
	testtypedsetoom State = { false, 0u };
	xallocator Allocator = {
		&State,
		testTypedSetOomAlloc,
		testTypedSetOomRealloc,
		testTypedSetOomFree
	};

	testRequire(
		xrtSetAllocator(&Allocator),
		"typed set OOM allocator install failed"
	);
	testTypedSetInsertOom(&State);
	testTypedSetCloneOom(&State);
	xrtClearError();
	printf("[PASS] typed set OOM\n");
	return 0;
}
