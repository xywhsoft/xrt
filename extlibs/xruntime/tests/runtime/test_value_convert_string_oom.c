#include "../test.h"



typedef struct testvalueconvertoom {
	bool Fail;
} testvalueconvertoom;



/* 按开关允许或拒绝 Value 文本转换分配。 */
static ptr testValueConvertOomAlloc(ptr pContext, size_t iSize)
{
	testvalueconvertoom* pState = (testvalueconvertoom*)pContext;

	return pState->Fail ? NULL : malloc(iSize);
}



/* 失败时保留原内存块。 */
static ptr testValueConvertOomRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	testvalueconvertoom* pState = (testvalueconvertoom*)pContext;

	return pState->Fail ? NULL : realloc(pMemory, iSize);
}



/* 释放测试分配器管理的内存。 */
static void testValueConvertOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 验证动态字符串精确复制 OOM 时目标保持原值。 */
static void testValueConvertStringOom(testvalueconvertoom* pState)
{
	char sLarge[2049];
	xvalue* pSource;
	str sTarget = xrtStrDup("target");
	str sBefore;

	memset(sLarge, 'v', sizeof(sLarge) - 1u);
	sLarge[sizeof(sLarge) - 1u] = 0;
	pSource = xrtValueString(
		(xstrview){ sLarge, sizeof(sLarge) - 1u }
	);
	testRequire((pSource != NULL) && (sTarget != NULL),
		"Value conversion OOM fixture allocation failed");
	sBefore = sTarget;
	pState->Fail = true;
	xrtClearError();
	testRequire(
		!xrtValueConvertTo(
			pSource, xrtTypeString(), &sTarget, XTYPE_CONVERT_EXACT
		),
		"dynamic string exact copy survived OOM"
	);
	testRequire(
		(sTarget == sBefore) && (strcmp(sTarget, "target") == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"dynamic string OOM changed the target or error kind"
	);
	pState->Fail = false;
	xrtClearError();
	xrtTypeDropValue(xrtTypeString(), &sTarget);
	xrtValueRelease(pSource);
}



/* 验证动态字符串解析不需要临时堆分配。 */
static void testValueConvertParseNoAllocation(testvalueconvertoom* pState)
{
	xvalue* pSource = xrtValueString(XRT_STR_LITERAL("42"));
	int32 iTarget = 0;

	testRequire(pSource != NULL, "Value parse allocation fixture failed");
	pState->Fail = true;
	xrtClearError();
	testRequire(
		xrtValueConvertTo(
			pSource, xrtTypeInt32(), &iTarget, XTYPE_CONVERT_EXPLICIT
		) && (iTarget == 42),
		"dynamic string parsing allocated temporary memory"
	);
	pState->Fail = false;
	xrtClearError();
	xrtValueRelease(pSource);
}



/* 运行 Value 文本转换的 OOM 与零分配门禁。 */
int main(void)
{
	testvalueconvertoom State = { false };
	xallocator Allocator = {
		&State,
		testValueConvertOomAlloc,
		testValueConvertOomRealloc,
		testValueConvertOomFree
	};

	testRequire(xrtSetAllocator(&Allocator),
		"failed to install Value conversion OOM allocator");
	testValueConvertStringOom(&State);
	testValueConvertParseNoAllocation(&State);
	printf("[PASS] Value string runtime conversion OOM\n");
	return 0;
}
