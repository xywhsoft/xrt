#include "../test.h"



/* 记录 Value 类型复制测试中句柄的深克隆和释放次数。 */
typedef struct testruntimevaluetypecopystate {
	int CloneCount;
	int DropCount;
} testruntimevaluetypecopystate;



/* 为深克隆创建独立整数句柄。 */
static bool testRuntimeValueTypeCopyHandleClone(
	ptr pHandle,
	ptr* pClone,
	ptr pUserData
)
{
	testruntimevaluetypecopystate* pState =
		(testruntimevaluetypecopystate*)pUserData;
	int* pCopy = (int*)xrtMalloc(sizeof(int));

	if ( pCopy == NULL ) {
		return false;
	}
	*pCopy = *(int*)pHandle;
	*pClone = pCopy;
	pState->CloneCount++;
	return true;
}



/* 释放复制测试独占的整数句柄。 */
static void testRuntimeValueTypeCopyHandleDrop(ptr pHandle, ptr pUserData)
{
	testruntimevaluetypecopystate* pState =
		(testruntimevaluetypecopystate*)pUserData;

	xrtFree(pHandle);
	pState->DropCount++;
}



/* 按整数内容生成稳定句柄哈希。 */
static uint64 testRuntimeValueTypeCopyHandleHash(ptr pHandle, ptr pUserData)
{
	(void)pUserData;
	return (uint64)*(int*)pHandle;
}



/* 按整数内容比较两个测试句柄。 */
static bool testRuntimeValueTypeCopyHandleEqual(
	ptr pLeft,
	ptr pRight,
	ptr pUserData
)
{
	(void)pUserData;
	return *(int*)pLeft == *(int*)pRight;
}



/* 验证普通复制使用 COW，显式克隆才递归复制完整 Value 图。 */
int main(void)
{
	static const xvaluehandleops tHandleOps = {
		testRuntimeValueTypeCopyHandleClone,
		testRuntimeValueTypeCopyHandleDrop,
		testRuntimeValueTypeCopyHandleHash,
		testRuntimeValueTypeCopyHandleEqual
	};
	testruntimevaluetypecopystate State = { 0, 0 };
	const xrttype* pType = xrtTypeValue();
	ptr pData = xrtMalloc(sizeof(int));
	xvalue* pHandle;
	xvalue* pSource = xrtValueArray();
	xvalue* pCopy = xrtValueNull();
	xvalue* pClone = xrtValueNull();
	ptr pSourceData;
	ptr pCloneData;

	testRequire(
		(pData != NULL) && (pSource != NULL),
		"runtime Value type copy fixture allocation failed"
	);
	*(int*)pData = 37;
	pHandle = xrtValueHandleTake(&pData, &tHandleOps, &State);
	testRequire(
		(pHandle != NULL) &&
		xrtValueArrayAppend(pSource, pHandle) &&
		xrtValueArrayAppend(pSource, pHandle),
		"runtime Value type copy fixture failed"
	);
	testRequire(
		xrtTypeIsCopyable(pType) &&
		!xrtTypeIsComparable(pType) &&
		!xrtTypeIsHashable(pType),
		"runtime Value type capabilities are inconsistent"
	);
	testRequire(
		xrtTypeCopyValue(pType, &pCopy, &pSource) &&
		(pCopy != pSource) &&
		(xrtValueArrayGet(pCopy, 0u) == xrtValueArrayGet(pSource, 0u)) &&
		(State.CloneCount == 0),
		"runtime Value type copy did not preserve COW ownership"
	);
	testRequire(
		xrtValueArrayAppendNew(pCopy, xrtValueInt(41)) &&
		(xrtValueCount(pSource) == 2u) &&
		(xrtValueCount(pCopy) == 3u) &&
		(State.CloneCount == 0),
		"runtime Value type copy did not isolate a COW mutation"
	);
	testRequire(
		xrtTypeCloneValue(pType, &pClone, &pSource) &&
		(pClone != pSource) &&
		xrtValueEqual(pClone, pSource) &&
		(xrtValueArrayGet(pClone, 0u) == xrtValueArrayGet(pClone, 1u)) &&
		(xrtValueArrayGet(pClone, 0u) != xrtValueArrayGet(pSource, 0u)) &&
		(State.CloneCount == 1),
		"runtime Value type clone lost graph isolation or shared topology"
	);
	testRequire(
		xrtValueGetHandle(
			xrtValueArrayGet(pSource, 0u), &pSourceData, NULL, NULL
		) &&
		xrtValueGetHandle(
			xrtValueArrayGet(pClone, 0u), &pCloneData, NULL, NULL
		) &&
		(pSourceData != pCloneData) &&
		(*(int*)pSourceData == *(int*)pCloneData),
		"runtime Value type clone did not duplicate handle ownership"
	);

	xrtTypeDropValue(pType, &pClone);
	xrtTypeDropValue(pType, &pCopy);
	xrtValueRelease(pSource);
	xrtValueRelease(pHandle);
	testRequire(State.DropCount == 2,
		"runtime Value type copy leaked or double-dropped a handle");
	xrtClearError();
	printf("[PASS] runtime Value type copy and clone\n");
	return 0;
}
