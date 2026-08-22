#include "../test.h"



typedef struct testconvertoom {
	bool Fail;
} testconvertoom;



typedef struct testconvertfixed {
	char Buffer[64];
	size_t Size;
} testconvertfixed;



/* 把内建格式化结果写入固定缓冲，不访问 XRT 分配器。 */
static bool testConvertFixedWrite(xstrview Text, ptr pContext)
{
	testconvertfixed* pWriter = (testconvertfixed*)pContext;

	if ( Text.Size > (sizeof(pWriter->Buffer) - pWriter->Size - 1u) ) {
		return false;
	}
	memcpy(pWriter->Buffer + pWriter->Size, Text.Data, Text.Size);
	pWriter->Size += Text.Size;
	pWriter->Buffer[pWriter->Size] = '\0';
	return true;
}



/* 按开关允许或拒绝文本转换分配。 */
static ptr testConvertOomAlloc(ptr pContext, size_t iSize)
{
	testconvertoom* pState = (testconvertoom*)pContext;

	return pState->Fail ? NULL : malloc(iSize);
}



/* 失败时保留原内存块。 */
static ptr testConvertOomRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	testconvertoom* pState = (testconvertoom*)pContext;

	return pState->Fail ? NULL : realloc(pMemory, iSize);
}



/* 释放测试分配器管理的内存。 */
static void testConvertOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 验证内建流式格式化在分配器完全拒绝请求时仍能成功。 */
static void testConvertFormatNoAllocation(testconvertoom* pState)
{
	testconvertfixed Writer;
	int64 iSource = INT64_MIN;
	bool bSuccess;

	memset(&Writer, 0, sizeof(Writer));
	pState->Fail = true;
	xrtClearError();
	bSuccess = xrtTypeFormat(
		xrtTypeInt64(), &iSource, testConvertFixedWrite, &Writer
	);
	pState->Fail = false;
	testRequire(bSuccess &&
		(strcmp(Writer.Buffer, "-9223372036854775808") == 0),
		"built-in streaming format allocated memory");
	testRequire(xrtGetError() == NULL,
		"allocation-free streaming format published an error");
}



/* 验证格式化 OOM 时拥有型字符串目标保持原值。 */
static void testConvertFormatOom(testconvertoom* pState)
{
	int64 iSource = INT64_MIN;
	str sTarget = xrtStrDup("target");
	str sOwned;
	str sBefore;

	testRequire(sTarget != NULL, "format OOM fixture allocation failed");
	sBefore = sTarget;
	pState->Fail = true;
	xrtClearError();
	testRequire(!xrtTypeConvert(xrtTypeInt64(), &iSource,
		xrtTypeString(), &sTarget, XTYPE_CONVERT_EXPLICIT),
		"integer formatting survived OOM");
	testRequire((sTarget == sBefore) && (strcmp(sTarget, "target") == 0),
		"formatting OOM changed the string target");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"formatting OOM error mismatch");
	xrtClearError();
	sOwned = xrtTypeToString(xrtTypeInt64(), &iSource);
	testRequire((sOwned == NULL) && (xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"owned string formatting OOM mismatch");

	pState->Fail = false;
	xrtClearError();
	testRequire(xrtTypeConvert(xrtTypeInt64(), &iSource,
		xrtTypeString(), &sTarget, XTYPE_CONVERT_EXPLICIT) &&
		(strcmp(sTarget, "-9223372036854775808") == 0),
		"formatting did not recover from OOM");
	xrtTypeDropValue(xrtTypeString(), &sTarget);
}



/* 验证同类型深复制 OOM 时来源和目标均保持原值。 */
static void testConvertCopyOom(testconvertoom* pState)
{
	char sLarge[2049];
	str sSource;
	str sTarget = xrtStrDup("target");
	str sBefore;

	memset(sLarge, 's', sizeof(sLarge) - 1u);
	sLarge[sizeof(sLarge) - 1u] = 0;
	sSource = xrtStrDup(sLarge);
	testRequire((sSource != NULL) && (sTarget != NULL),
		"copy OOM fixture allocation failed");
	sBefore = sTarget;
	pState->Fail = true;
	xrtClearError();
	testRequire(!xrtTypeConvert(xrtTypeString(), &sSource,
		xrtTypeString(), &sTarget, XTYPE_CONVERT_EXACT),
		"exact string copy survived OOM");
	testRequire(
		(sTarget == sBefore) && (strcmp(sTarget, "target") == 0) &&
		(strlen(sSource) == (sizeof(sLarge) - 1u)) &&
		(sSource[0] == 's'),
		"exact string copy OOM changed source or target"
	);
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"exact string copy OOM error mismatch");

	pState->Fail = false;
	xrtClearError();
	xrtTypeDropValue(xrtTypeString(), &sSource);
	xrtTypeDropValue(xrtTypeString(), &sTarget);
}



/* 验证成功的标量文本解析不需要堆分配。 */
static void testConvertParseNoAllocation(testconvertoom* pState)
{
	str sSource = "42";
	int32 iTarget = 0;

	pState->Fail = true;
	xrtClearError();
	testRequire(xrtTypeConvert(xrtTypeString(), &sSource,
		xrtTypeInt32(), &iTarget, XTYPE_CONVERT_EXPLICIT) &&
		(iTarget == 42), "successful integer text parsing allocated memory");
	pState->Fail = false;
	xrtClearError();
}



/* 运行文本转换的 OOM 和零分配解析门禁。 */
int main(void)
{
	testconvertoom State = { false };
	xallocator Allocator = {
		&State,
		testConvertOomAlloc,
		testConvertOomRealloc,
		testConvertOomFree
	};

	testRequire(xrtSetAllocator(&Allocator),
		"failed to install runtime conversion OOM allocator");
	testConvertFormatNoAllocation(&State);
	testConvertFormatOom(&State);
	testConvertCopyOom(&State);
	testConvertParseNoAllocation(&State);
	printf("[PASS] runtime string conversion OOM\n");
	return 0;
}
