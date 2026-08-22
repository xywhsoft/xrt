#include "../test.h"



/* 故障注入分配器记录限额和仍然活动的分配。 */
typedef struct testvalueoomstate {
	bool Fail;
} testvalueoomstate;



/* 在允许状态下转发到底层分配器。 */
static ptr testValueOomAlloc(ptr pContext, size_t iSize)
{
	testvalueoomstate* pState = (testvalueoomstate*)pContext;

	if ( pState->Fail ) {
		return NULL;
	}
	return malloc(iSize);
}



/* 在允许状态下执行保持原块的标准重分配。 */
static ptr testValueOomRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	testvalueoomstate* pState = (testvalueoomstate*)pContext;

	if ( pState->Fail ) {
		return NULL;
	}
	return realloc(pMemory, iSize);
}



/* 释放测试分配并更新活动计数。 */
static void testValueOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 释放 Value 接管的测试句柄。 */
static void testValueOomHandleDrop(ptr pHandle, ptr pUserData)
{
	(void)pUserData;
	xrtFree(pHandle);
}



/* 验证标量和三种 Take 在每个 OOM 阶段都保持来源与所有权。 */
int main(void)
{
	static const xvaluehandleops tHandleOps = {
		NULL,
		testValueOomHandleDrop,
		NULL,
		NULL
	};
	testvalueoomstate tState = { false };
	xallocator tAllocator;
	xvalue* arrHeld[4096];
	str sText;
	bytes pBytes;
	ptr pHandle;
	xvalue* pWarm;
	xvalue* pValue;
	size_t iHeld = 0;

	tAllocator.Context = &tState;
	tAllocator.Alloc = testValueOomAlloc;
	tAllocator.Realloc = testValueOomRealloc;
	tAllocator.Free = testValueOomFree;
	testRequire(
		xrtSetAllocator(&tAllocator),
		"failed to install value OOM allocator"
	);

	/* 首次 Value 分配没有可复用 span，必须直接传播底层 OOM。 */
	tState.Fail = true;
	xrtClearError();
	testRequire(xrtValueInt(1) == NULL, "value scalar allocation should fail");
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"value scalar OOM error mismatch"
	);

	/* 预热外壳尺寸类，使下一次 StringTake 精确失败在大块重分配。 */
	tState.Fail = false;
	pWarm = xrtValueInt(1);
	testRequire(pWarm != NULL, "value OOM shell warmup failed");
	xrtValueRelease(pWarm);
	sText = (str)xrtMalloc(2049);
	pBytes = (bytes)xrtMalloc(3);
	pHandle = xrtMalloc(sizeof(int));
	testRequire(
		(sText != NULL) && (pBytes != NULL) && (pHandle != NULL),
		"value OOM fixture allocation failed"
	);
	memset(sText, 'x', 2048);
	sText[2048] = '\0';

	tState.Fail = true;
	xrtClearError();
	testRequire(
		xrtValueString((xstrview){ sText, 2048 }) == NULL,
		"value string copy should fail"
	);
	testRequire(
		xrtValueStringTake(&sText, 2048) == NULL,
		"value string realloc should fail"
	);
	testRequire(
		(sText != NULL) && (sText[0] == 'x') &&
		(sText[2048] == '\0') &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"value string realloc OOM did not roll back"
	);

	/* 耗尽已有 Value 小块，后续 Take 必须失败在外壳分配。 */
	xrtClearError();
	while ( iHeld < (sizeof(arrHeld) / sizeof(arrHeld[0])) ) {
		pValue = xrtValueInt((int64)iHeld);
		if ( pValue == NULL ) {
			break;
		}
		arrHeld[iHeld++] = pValue;
	}
	testRequire(
		iHeld < (sizeof(arrHeld) / sizeof(arrHeld[0])),
		"value shell pool did not reach injected OOM"
	);
	testRequire(
		xrtValueBytesTake(&pBytes, 3) == NULL,
		"value bytes take should fail"
	);
	testRequire(
		pBytes != NULL,
		"value bytes OOM consumed source"
	);
	testRequire(
		xrtValueHandleTake(&pHandle, &tHandleOps, NULL) == NULL,
		"value handle take should fail"
	);
	testRequire(
		pHandle != NULL,
		"value handle OOM consumed source"
	);
	while ( iHeld != 0 ) {
		xrtValueRelease(arrHeld[--iHeld]);
	}

	tState.Fail = false;
	pValue = xrtValueStringTake(&sText, 2048);
	testRequire((pValue != NULL) && (sText == NULL), "value string take recovery failed");
	xrtValueRelease(pValue);
	pValue = xrtValueBytesTake(&pBytes, 3);
	testRequire((pValue != NULL) && (pBytes == NULL), "value bytes take recovery failed");
	xrtValueRelease(pValue);
	pValue = xrtValueHandleTake(&pHandle, &tHandleOps, NULL);
	testRequire((pValue != NULL) && (pHandle == NULL), "value handle take recovery failed");
	xrtValueRelease(pValue);

	printf("[PASS] value OOM\n");
	return 0;
}
