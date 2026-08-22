#include "../test.h"



/* 句柄测试状态记录生命周期、策略调用和回调重入保护。 */
typedef struct testvaluehandlestate {
	int CloneCount;
	int DropCount;
	int HashCount;
	xvalue* ReentryValue;
	xvalue* CallbackLeft;
	xvalue* CallbackRight;
	bool RetainBlocked;
	bool GetBlocked;
	bool ReleaseBlocked;
	bool HashBlocked;
	bool EqualLeftBlocked;
	bool EqualRightBlocked;
} testvaluehandlestate;



/* 克隆器复制一个整数句柄。 */
static bool testValueHandleClone(ptr pHandle, ptr* pClone, ptr pUserData)
{
	testvaluehandlestate* pState = (testvaluehandlestate*)pUserData;
	int* pCopy = (int*)xrtMalloc(sizeof(int));

	if ( pCopy == NULL ) {
		return false;
	}
	*pCopy = *(int*)pHandle;
	*pClone = pCopy;
	pState->CloneCount++;
	return true;
}



/* 释放器销毁整数句柄。 */
static void testValueHandleDrop(ptr pHandle, ptr pUserData)
{
	testvaluehandlestate* pState = (testvaluehandlestate*)pUserData;
	ptr pRead;

	if ( pState->ReentryValue != NULL ) {
		xrtClearError();
		pState->RetainBlocked =
			(xrtValueRetain(pState->ReentryValue) == NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_STATE);

		xrtClearError();
		pState->GetBlocked =
			!xrtValueGetHandle(
				pState->ReentryValue,
				&pRead,
				NULL,
				NULL
			) &&
			(xrtErrorKind(xrtGetError()) == XERR_STATE);

		xrtClearError();
		xrtValueRelease(pState->ReentryValue);
		pState->ReleaseBlocked =
			xrtErrorKind(xrtGetError()) == XERR_STATE;
	}

	xrtFree(pHandle);
	pState->DropCount++;
}



/* 哈希器按整数内容计算哈希。 */
static uint64 testValueHandleHash(ptr pHandle, ptr pUserData)
{
	testvaluehandlestate* pState = (testvaluehandlestate*)pUserData;
	xvalue* pClone;

	pState->HashCount++;
	if ( pState->CallbackLeft != NULL ) {
		xrtClearError();
		pClone = xrtValueClone(pState->CallbackLeft);
		pState->HashBlocked =
			(pClone == NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_STATE);
		xrtValueRelease(pClone);
	}
	return (uint64)*(int*)pHandle;
}



/* 相等器按整数内容比较句柄。 */
static bool testValueHandleEqual(ptr pLeft, ptr pRight, ptr pUserData)
{
	testvaluehandlestate* pState = (testvaluehandlestate*)pUserData;
	xvalue* pClone;

	if ( pState->CallbackLeft != NULL ) {
		xrtClearError();
		pClone = xrtValueClone(pState->CallbackLeft);
		pState->EqualLeftBlocked =
			(pClone == NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_STATE);
		xrtValueRelease(pClone);
	}
	if ( pState->CallbackRight != NULL ) {
		xrtClearError();
		pClone = xrtValueClone(pState->CallbackRight);
		pState->EqualRightBlocked =
			(pClone == NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_STATE);
		xrtValueRelease(pClone);
	}
	return *(int*)pLeft == *(int*)pRight;
}



/* 验证句柄哈希器与相等器必须成对提供且失败不消费来源。 */
static void testValueHandlePolicy(void)
{
	static const xvaluehandleops tHashOnly = {
		NULL,
		testValueHandleDrop,
		testValueHandleHash,
		NULL
	};
	static const xvaluehandleops tEqualOnly = {
		NULL,
		testValueHandleDrop,
		NULL,
		testValueHandleEqual
	};
	testvaluehandlestate tState = { 0 };
	ptr pHandle = xrtMalloc(sizeof(int));

	testRequire(pHandle != NULL, "handle policy allocation failed");
	xrtClearError();
	testRequire(
		xrtValueHandleTake(&pHandle, &tHashOnly, &tState) == NULL,
		"handle accepted hash-only policy"
	);
	testRequire(
		(pHandle != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"hash-only policy consumed source or reported wrong error"
	);

	xrtClearError();
	testRequire(
		xrtValueHandleTake(&pHandle, &tEqualOnly, &tState) == NULL,
		"handle accepted equal-only policy"
	);
	testRequire(
		(pHandle != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"equal-only policy consumed source or reported wrong error"
	);
	xrtFree(pHandle);
}



/* 验证最后释放回调不能重入读取、保留或再次释放同一值。 */
static void testValueHandleReentry(const xvaluehandleops* pOps)
{
	testvaluehandlestate tState = { 0 };
	ptr pHandle = xrtMalloc(sizeof(int));
	xvalue* pValue;

	testRequire(pHandle != NULL, "handle reentry allocation failed");
	pValue = xrtValueHandleTake(&pHandle, pOps, &tState);
	testRequire((pValue != NULL) && (pHandle == NULL), "handle reentry take failed");
	tState.ReentryValue = pValue;
	xrtClearError();
	xrtValueRelease(pValue);
	testRequire(
		tState.RetainBlocked &&
		tState.GetBlocked &&
		tState.ReleaseBlocked &&
		(tState.DropCount == 1),
		"handle drop callback reentry was not fully blocked"
	);
	xrtClearError();
}



/* 验证显式句柄所有权、共享身份和策略查询。 */
int main(void)
{
	static const xvaluehandleops tOps = {
		testValueHandleClone,
		testValueHandleDrop,
		testValueHandleHash,
		testValueHandleEqual
	};
	testvaluehandlestate tState = { 0 };
	ptr pHandle = xrtMalloc(sizeof(int));
	xvalue* pValue;
	xvalue* pShared;
	xvalue* pEqual;
	const xvaluehandleops* pReadOps;
	ptr pReadHandle;
	ptr pReadUser;
	ptr pOverlapped;
	uint64 iHash;

	testValueHandlePolicy();
	testRequire(pHandle != NULL, "handle allocation failed");
	*(int*)pHandle = 77;
	pValue = xrtValueHandleTake(&pHandle, &tOps, &tState);
	testRequire((pValue != NULL) && (pHandle == NULL), "handle take failed");
	testRequire(
		xrtValueGetHandle(pValue, &pReadHandle, &pReadOps, &pReadUser) &&
		(pReadHandle != NULL) && (*(int*)pReadHandle == 77) &&
		(pReadOps == &tOps) && (pReadUser == &tState),
		"handle getter mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtValueGetHandle(
			pValue,
			(ptr*)pReadHandle,
			NULL,
			NULL
		),
		"handle getter accepted owned handle output"
	);
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(*(int*)pReadHandle == 77),
		"handle getter alias changed owned handle"
	);
	xrtClearError();
	testRequire(
		!xrtValueGetHandle(
			pValue,
			&pOverlapped,
			(const xvaluehandleops**)&pOverlapped,
			NULL
		),
		"handle getter accepted overlapping outputs"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"handle getter output overlap error mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtValueHash(pValue, (uint64*)pReadHandle),
		"handle hash accepted owned output"
	);
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(*(int*)pReadHandle == 77),
		"handle hash alias changed owned handle"
	);
	tState.CallbackLeft = pValue;
	testRequire(
		xrtValueHash(pValue, &iHash) &&
		(tState.HashCount == 1) &&
		tState.HashBlocked,
		"handle hash callback escaped Value guard"
	);
	tState.CallbackLeft = NULL;
	pHandle = xrtMalloc(sizeof(int));
	testRequire(pHandle != NULL, "equal handle allocation failed");
	*(int*)pHandle = 77;
	pEqual = xrtValueHandleTake(&pHandle, &tOps, &tState);
	tState.CallbackLeft = pValue;
	tState.CallbackRight = pEqual;
	testRequire(
		(pEqual != NULL) &&
		(pHandle == NULL) &&
		xrtValueScalarEqual(pValue, pEqual) &&
		tState.EqualLeftBlocked &&
		tState.EqualRightBlocked,
		"handle equality callback escaped Value guards"
	);
	tState.CallbackLeft = NULL;
	tState.CallbackRight = NULL;
	pShared = xrtValueClone(pValue);
	testRequire((pShared == pValue) && (tState.CloneCount == 0), "shallow handle clone changed identity");
	xrtValueRelease(pShared);
	testRequire(tState.DropCount == 0, "shared handle dropped early");
	xrtValueRelease(pEqual);
	xrtValueRelease(pValue);
	testRequire(tState.DropCount == 2, "owned handle drop mismatch");
	testValueHandleReentry(&tOps);
	printf("[PASS] value handle\n");
	return 0;
}
