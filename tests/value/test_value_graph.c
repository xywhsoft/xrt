#include "../test.h"



/* 句柄图状态记录深拷贝和释放次数。 */
typedef struct testvaluegraphstate {
	int CloneCount;
	int DropCount;
} testvaluegraphstate;



/* 深拷贝句柄使用独立整数。 */
static bool testValueGraphHandleClone(ptr pHandle, ptr* pClone, ptr pUserData)
{
	testvaluegraphstate* pState = (testvaluegraphstate*)pUserData;
	int* pCopy = (int*)xrtMalloc(sizeof(int));

	if ( pCopy == NULL ) {
		return false;
	}
	*pCopy = *(int*)pHandle;
	*pClone = pCopy;
	pState->CloneCount++;
	return true;
}



/* 释放句柄整数。 */
static void testValueGraphHandleDrop(ptr pHandle, ptr pUserData)
{
	testvaluegraphstate* pState = (testvaluegraphstate*)pUserData;

	xrtFree(pHandle);
	pState->DropCount++;
}



/* 模拟违反错误契约的句柄克隆器，库必须补充确定的状态错误。 */
static bool testValueGraphHandleCloneWithoutError(
	ptr pHandle,
	ptr* pClone,
	ptr pUserData
)
{
	(void)pHandle;
	(void)pUserData;
	*pClone = NULL;
	return false;
}



/* 读取图内整数。 */
static int64 testValueGraphInt(const xvalue* pValue)
{
	int64 iValue = 0;

	testRequire(xrtValueGetInt(pValue, &iValue), "graph expected integer");
	return iValue;
}



/* 验证深拷贝与源图隔离并保留重复子值身份。 */
static void testValueGraphClone(void)
{
	xvalue* pRoot = xrtValueArray();
	xvalue* pChild = xrtValueObject();
	xvalue* pCopy;
	xvalue* pCopyChild;

	testRequire(xrtValueObjectSetNew(pChild, XRT_STR_LITERAL("value"), xrtValueInt(7)), "graph child fixture failed");
	testRequire(xrtValueArrayAppend(pRoot, pChild) && xrtValueArrayAppend(pRoot, pChild), "graph shared child fixture failed");
	pCopy = xrtValueDeepClone(pRoot);
	testRequire((pCopy != NULL) && xrtValueEqual(pRoot, pCopy), "graph deep clone mismatch");
	testRequire(xrtValueArrayGet(pCopy, 0) == xrtValueArrayGet(pCopy, 1), "graph clone lost shared child identity");
	pCopyChild = xrtValueArrayEdit(pCopy, 0);
	testRequire(xrtValueObjectSetNew(pCopyChild, XRT_STR_LITERAL("value"), xrtValueInt(8)), "graph clone mutation failed");
	testRequire(testValueGraphInt(xrtValueObjectGet(pChild, XRT_STR_LITERAL("value"))) == 7, "graph clone changed source");
	testRequire(!xrtValueEqual(pRoot, pCopy), "graph equality missed mutation");
	xrtValueRelease(pCopy);
	xrtValueRelease(pChild);
	xrtValueRelease(pRoot);
}



/* 验证对象相等不依赖插入顺序，数组相等保留顺序。 */
static void testValueGraphEqual(void)
{
	xvalue* pLeft = xrtValueObject();
	xvalue* pRight = xrtValueObject();
	xvalue* pArrayA = xrtValueArray();
	xvalue* pArrayB = xrtValueArray();

	testRequire(xrtValueObjectSetNew(pLeft, XRT_STR_LITERAL("a"), xrtValueInt(1)), "equal left a failed");
	testRequire(xrtValueObjectSetNew(pLeft, XRT_STR_LITERAL("b"), xrtValueInt(2)), "equal left b failed");
	testRequire(xrtValueObjectSetNew(pRight, XRT_STR_LITERAL("b"), xrtValueFloat(2.0)), "equal right b failed");
	testRequire(xrtValueObjectSetNew(pRight, XRT_STR_LITERAL("a"), xrtValueFloat(1.0)), "equal right a failed");
	testRequire(xrtValueEqual(pLeft, pRight), "object order-independent equality failed");
	testRequire(xrtValueArrayAppendNew(pArrayA, xrtValueInt(1)) && xrtValueArrayAppendNew(pArrayA, xrtValueInt(2)), "equal array A failed");
	testRequire(xrtValueArrayAppendNew(pArrayB, xrtValueInt(2)) && xrtValueArrayAppendNew(pArrayB, xrtValueInt(1)), "equal array B failed");
	testRequire(!xrtValueEqual(pArrayA, pArrayB), "array order equality mismatch");
	xrtValueRelease(pArrayB);
	xrtValueRelease(pArrayA);
	xrtValueRelease(pRight);
	xrtValueRelease(pLeft);
}



/* 验证句柄深拷贝执行显式策略。 */
static void testValueGraphHandle(void)
{
	static const xvaluehandleops tOps = {
		testValueGraphHandleClone,
		testValueGraphHandleDrop,
		NULL,
		NULL
	};
	testvaluegraphstate tState = { 0, 0 };
	ptr pHandle = xrtMalloc(sizeof(int));
	xvalue* pValue;
	xvalue* pCopy;
	ptr pRead;

	testRequire(pHandle != NULL, "graph handle allocation failed");
	*(int*)pHandle = 9;
	pValue = xrtValueHandleTake(&pHandle, &tOps, &tState);
	pCopy = xrtValueDeepClone(pValue);
	testRequire((pCopy != NULL) && (pCopy != pValue) && (tState.CloneCount == 1), "graph handle clone mismatch");
	testRequire(xrtValueGetHandle(pCopy, &pRead, NULL, NULL) && (*(int*)pRead == 9), "graph cloned handle value mismatch");
	xrtValueRelease(pCopy);
	xrtValueRelease(pValue);
	testRequire(tState.DropCount == 2, "graph handle drop mismatch");
}



/* 验证不可克隆句柄和无错误失败回调都形成明确错误。 */
static void testValueGraphHandleErrors(void)
{
	static const xvaluehandleops tUnsupportedOps = {
		NULL,
		testValueGraphHandleDrop,
		NULL,
		NULL
	};
	static const xvaluehandleops tBrokenOps = {
		testValueGraphHandleCloneWithoutError,
		testValueGraphHandleDrop,
		NULL,
		NULL
	};
	testvaluegraphstate tState = { 0, 0 };
	ptr pHandle = xrtMalloc(sizeof(int));
	xvalue* pValue;
	xvalue* pCopy;

	testRequire(pHandle != NULL, "unsupported handle fixture failed");
	pValue = xrtValueHandleTake(&pHandle, &tUnsupportedOps, &tState);
	xrtClearError();
	pCopy = xrtValueDeepClone(pValue);
	testRequire(
		(pCopy == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED),
		"unsupported handle clone error mismatch"
	);
	xrtValueRelease(pValue);

	pHandle = xrtMalloc(sizeof(int));
	testRequire(pHandle != NULL, "broken handle fixture failed");
	pValue = xrtValueHandleTake(&pHandle, &tBrokenOps, &tState);
	xrtClearError();
	pCopy = xrtValueDeepClone(pValue);
	testRequire(
		(pCopy == NULL) && (xrtErrorKind(xrtGetError()) == XERR_STATE),
		"broken handle clone error mismatch"
	);
	xrtValueRelease(pValue);
	testRequire(tState.DropCount == 2, "failed handle clone drop mismatch");
}



/* 验证调用前残留错误不会被图去重逻辑误判为循环。 */
static void testValueGraphStaleError(void)
{
	xvalue* pSource = xrtValueArray();
	xvalue* pCopy;

	testRequire(
		xrtValueArrayAppendNew(pSource, xrtValueInt(1)),
		"stale error graph fixture failed"
	);
	testRequire(
		xrtValueArrayAt(pSource, 9) == NULL,
		"stale error fixture should be out of range"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"stale error fixture mismatch"
	);
	pCopy = xrtValueDeepClone(pSource);
	testRequire(
		(pCopy != NULL) && xrtValueEqual(pSource, pCopy),
		"stale error broke deep clone"
	);
	xrtValueRelease(pCopy);
	xrtValueRelease(pSource);
}



/* 运行 Value 图回归。 */
int main(void)
{
	testValueGraphClone();
	testValueGraphEqual();
	testValueGraphHandle();
	testValueGraphHandleErrors();
	testValueGraphStaleError();
	printf("[PASS] value graph\n");
	return 0;
}
