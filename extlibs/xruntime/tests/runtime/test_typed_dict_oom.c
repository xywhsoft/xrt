#include "../test.h"



typedef struct testtypeddictoom {
	bool Enabled;
	size_t Remaining;
} testtypeddictoom;



/* 按倒计数故障点分配类型字典测试内存。 */
static ptr testTypedDictOomAlloc(ptr pContext, size_t iSize)
{
	testtypeddictoom* pState = (testtypeddictoom*)pContext;

	if ( pState->Enabled ) {
		if ( pState->Remaining == 0u ) {
			return NULL;
		}
		pState->Remaining--;
	}
	return malloc(iSize);
}



/* 按倒计数故障点重分配类型字典测试内存。 */
static ptr testTypedDictOomRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	testtypeddictoom* pState = (testtypeddictoom*)pContext;

	if ( pState->Enabled ) {
		if ( pState->Remaining == 0u ) {
			return NULL;
		}
		pState->Remaining--;
	}
	return realloc(pMemory, iSize);
}



/* 释放类型字典测试内存。 */
static void testTypedDictOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 验证插入 OOM 不提交键。 */
static void testTypedDictInsertOom(testtypeddictoom* pState)
{
	xtypeddict Dict;
	int64 iValue = 7;

	testRequire(
		xrtTypedDictInit(&Dict, xrtTypeInt64()),
		"typed dictionary OOM init failed"
	);
	pState->Enabled = true;
	pState->Remaining = 0u;
	xrtClearError();
	testRequire(
		!xrtTypedDictSet(&Dict, XRT_STR_LITERAL("value"), &iValue) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtTypedDictCount(&Dict) == 0u),
		"typed dictionary insertion OOM changed visible state"
	);
	xrtClearError();
	testRequire(
		!xrtTypedDictSetTake(
			&Dict, XRT_STR_LITERAL("moved"), &iValue
		) && (iValue == 7) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtTypedDictCount(&Dict) == 0u),
		"typed dictionary moved insertion OOM consumed its source"
	);
	pState->Enabled = false;
	xrtTypedDictUnit(&Dict);
}



/* 扫描克隆的每个分配故障点并验证来源保持不变。 */
static void testTypedDictCloneOom(testtypeddictoom* pState)
{
	xtypeddict Dict;
	xtypeddict* pClone;
	int64 iOne = 1;
	int64 iTwo = 2;
	bool bObservedFailure = false;
	bool bObservedSuccess = false;

	testRequire(
		xrtTypedDictInit(&Dict, xrtTypeInt64()) &&
		xrtTypedDictSet(&Dict, XRT_STR_LITERAL("one"), &iOne) &&
		xrtTypedDictSet(&Dict, XRT_STR_LITERAL("two"), &iTwo),
		"typed dictionary clone OOM fixture failed"
	);
	for ( size_t i = 0u; i < 20u; i++ ) {
		pState->Enabled = true;
		pState->Remaining = i;
		xrtClearError();
		pClone = xrtTypedDictClone(&Dict);
		pState->Enabled = false;
		if ( pClone == NULL ) {
			bObservedFailure = true;
			testRequire(
				(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
				(xrtTypedDictCount(&Dict) == 2u) &&
				xrtTypedDictHas(&Dict, XRT_STR_LITERAL("one")) &&
				xrtTypedDictHas(&Dict, XRT_STR_LITERAL("two")),
				"typed dictionary clone OOM changed its source"
			);
		} else {
			bObservedSuccess = true;
			testRequire(
				xrtTypedDictEquals(&Dict, pClone),
				"typed dictionary clone after OOM scan mismatched"
			);
			xrtTypedDictDestroy(pClone);
			break;
		}
	}
	testRequire(
		bObservedFailure && bObservedSuccess,
		"typed dictionary clone OOM scan missed failure or success"
	);
	xrtTypedDictUnit(&Dict);
}



/* 运行类型字典分配失败测试。 */
int main(void)
{
	testtypeddictoom State = { false, 0u };
	xallocator Allocator = {
		&State,
		testTypedDictOomAlloc,
		testTypedDictOomRealloc,
		testTypedDictOomFree
	};

	testRequire(
		xrtSetAllocator(&Allocator),
		"typed dictionary OOM allocator install failed"
	);
	testTypedDictInsertOom(&State);
	testTypedDictCloneOom(&State);
	xrtClearError();
	printf("[PASS] typed dictionary OOM\n");
	return 0;
}
