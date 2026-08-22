#include "../test.h"



typedef struct teststringvalueoom {
	bool Fail;
} teststringvalueoom;



/* 按开关允许或拒绝字符串转换的底层分配。 */
static ptr testStringValueOomAlloc(ptr pContext, size_t iSize)
{
	teststringvalueoom* pState = (teststringvalueoom*)pContext;

	return pState->Fail ? NULL : malloc(iSize);
}



/* 失败时保留原内存块。 */
static ptr testStringValueOomRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	teststringvalueoom* pState = (teststringvalueoom*)pContext;

	return pState->Fail ? NULL : realloc(pMemory, iSize);
}



/* 释放测试分配器管理的内存。 */
static void testStringValueOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 验证字符串两个转换方向在 OOM 时均保持来源和输出边界。 */
int main(void)
{
	teststringvalueoom State = { false };
	xallocator Allocator = {
		&State,
		testStringValueOomAlloc,
		testStringValueOomRealloc,
		testStringValueOomFree
	};
	const xrttype* pType = xrtTypeString();
	char sLarge[2049];
	xvalue* pSource;
	xvalue* pEncoded;
	str sTyped = NULL;

	testRequire(xrtSetAllocator(&Allocator),
		"failed to install runtime string Value OOM allocator");
	memset(sLarge, 'v', sizeof(sLarge) - 1u);
	sLarge[sizeof(sLarge) - 1u] = 0;
	pSource = xrtValueString(
		xrtStrViewN(sLarge, sizeof(sLarge) - 1u)
	);
	testRequire(pSource != NULL,
		"runtime string Value OOM fixture failed");

	State.Fail = true;
	xrtClearError();
	testRequire(!xrtValueToTyped(pSource, pType, &sTyped, NULL),
		"runtime string Value decode survived OOM");
	testRequire(
		(sTyped == NULL) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"runtime string Value decode OOM state mismatch"
	);

	State.Fail = false;
	xrtClearError();
	testRequire(
		xrtValueToTyped(pSource, pType, &sTyped, NULL) &&
		(sTyped != NULL) &&
		(strlen(sTyped) == (sizeof(sLarge) - 1u)),
		"runtime string Value decode did not recover from OOM"
	);

	State.Fail = true;
	xrtClearError();
	pEncoded = xrtValueFromTyped(pType, &sTyped, NULL);
	testRequire(pEncoded == NULL,
		"runtime string Value encode survived OOM");
	testRequire(
		(strcmp(sTyped, sLarge) == 0) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"runtime string Value encode OOM changed the source"
	);

	State.Fail = false;
	xrtClearError();
	pEncoded = xrtValueFromTyped(pType, &sTyped, NULL);
	testRequire(pEncoded != NULL,
		"runtime string Value encode did not recover from OOM");
	xrtValueRelease(pEncoded);
	xrtTypeDropValue(pType, &sTyped);
	xrtValueRelease(pSource);
	xrtClearError();
	printf("[PASS] runtime string Value OOM\n");
	return 0;
}
