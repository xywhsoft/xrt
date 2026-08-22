#include "../test.h"



typedef struct teststringoom {
	bool Fail;
} teststringoom;



/* 按开关允许或拒绝运行时字符串分配。 */
static ptr testStringOomAlloc(ptr pContext, size_t iSize)
{
	teststringoom* pState = (teststringoom*)pContext;

	return pState->Fail ? NULL : malloc(iSize);
}



/* 失败时保留原内存块。 */
static ptr testStringOomRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	teststringoom* pState = (teststringoom*)pContext;

	return pState->Fail ? NULL : realloc(pMemory, iSize);
}



/* 释放测试分配器管理的内存。 */
static void testStringOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 验证字符串复制 OOM 时来源和目标均保持原值。 */
int main(void)
{
	teststringoom State = { false };
	xallocator Allocator = {
		&State,
		testStringOomAlloc,
		testStringOomRealloc,
		testStringOomFree
	};
	const xrttype* pType = xrtTypeString();
	char sLarge[2049];
	str sSource;
	str sTarget;
	str sTargetBefore;

	testRequire(xrtSetAllocator(&Allocator),
		"failed to install runtime string type OOM allocator");
	memset(sLarge, 's', sizeof(sLarge) - 1u);
	sLarge[sizeof(sLarge) - 1u] = 0;
	sSource = xrtStrDup(sLarge);
	sTarget = xrtStrDup("target");
	testRequire((sSource != NULL) && (sTarget != NULL),
		"runtime string type OOM fixture failed");
	sTargetBefore = sTarget;

	State.Fail = true;
	xrtClearError();
	testRequire(!xrtTypeCopyValue(pType, &sTarget, &sSource),
		"runtime string type copy survived OOM");
	testRequire(
		(sTarget == sTargetBefore) &&
		(strcmp(sTarget, "target") == 0) &&
		(strlen(sSource) == (sizeof(sLarge) - 1u)) &&
		(sSource[0] == 's'),
		"runtime string type OOM changed source or target"
	);
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"runtime string type OOM error mismatch"
	);

	State.Fail = false;
	xrtClearError();
	testRequire(
		xrtTypeCopyValue(pType, &sTarget, &sSource) &&
		(sTarget != sSource) &&
		(strcmp(sTarget, sSource) == 0),
		"runtime string type did not recover from OOM"
	);
	xrtTypeDropValue(pType, &sSource);
	xrtTypeDropValue(pType, &sTarget);
	xrtClearError();
	printf("[PASS] runtime string type OOM\n");
	return 0;
}
