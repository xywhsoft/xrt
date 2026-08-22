#include "../test.h"



static int testRuntimeValueGraphChurnDrops = 0;



/* 记录每轮递归图根对象的最终释放。 */
static void testRuntimeValueGraphChurnDrop(ptr pValue, const xrttype* pType)
{
	(void)pValue;
	(void)pType;
	testRuntimeValueGraphChurnDrops++;
}



/* 递归函数取得父层所有权，并通过对象图模拟解释器作用域链。 */
static int64 testRuntimeValueGraphChurnFib(int iN, xvalue* pParent)
{
	xvalue* pFrame;
	xvalue* pNumber;
	xvalue* pBranch;
	int64 iLeft;
	int64 iRight;

	if ( iN < 2 ) {
		xrtValueRelease(pParent);
		return iN;
	}

	/* 当前帧保存参数和父帧，成功后完整接管父层引用。 */
	pFrame = xrtValueObject();
	pNumber = xrtValueInt(iN);
	if ( (pFrame == NULL) || (pNumber == NULL) ||
		 !xrtValueObjectSetTake(
			pFrame, XRT_STR_LITERAL("n"), &pNumber
		) ) {
		xrtValueRelease(pNumber);
		xrtValueRelease(pFrame);
		xrtValueRelease(pParent);
		return -1;
	}
	if ( !xrtValueObjectSetTake(
		pFrame, XRT_STR_LITERAL("parent"), &pParent
	) ) {
		xrtValueRelease(pFrame);
		xrtValueRelease(pParent);
		return -1;
	}

	/* 两个分支各取得一份帧引用，递归返回时分别消费。 */
	pBranch = xrtValueRetain(pFrame);
	if ( pBranch == NULL ) {
		xrtValueRelease(pFrame);
		return -1;
	}
	iLeft = testRuntimeValueGraphChurnFib(iN - 1, pBranch);
	if ( iLeft < 0 ) {
		xrtValueRelease(pFrame);
		return -1;
	}
	pBranch = xrtValueRetain(pFrame);
	if ( pBranch == NULL ) {
		xrtValueRelease(pFrame);
		return -1;
	}
	iRight = testRuntimeValueGraphChurnFib(iN - 2, pBranch);
	xrtValueRelease(pFrame);
	if ( iRight < 0 ) {
		return -1;
	}
	return iLeft + iRight;
}



/* 重复建立并释放完整递归图，验证结果及每轮根对象析构次数。 */
static bool testRuntimeValueGraphChurnRound(
	const xrttype* pType,
	int iDepth,
	int iRepeats,
	int64 iExpected
)
{
	for ( int i = 0; i < iRepeats; i++ ) {
		xrtobject* pObject = xrtObjectCreate(pType);
		xvalue* pParent;
		int iDropsBefore = testRuntimeValueGraphChurnDrops;
		int64 iResult;

		if ( pObject == NULL ) {
			return false;
		}
		pParent = xrtValueRuntimeObjectTake(&pObject);
		if ( pParent == NULL ) {
			xrtObjectUnref(pObject);
			return false;
		}
		iResult = testRuntimeValueGraphChurnFib(iDepth, pParent);
		if ( (iResult != iExpected) ||
			 (testRuntimeValueGraphChurnDrops != (iDropsBefore + 1)) ) {
			return false;
		}
	}
	return true;
}



/* 验证递归 Value 所有权图可持续建立和完整释放。 */
int main(void)
{
	static const xrtinstanceops tObjectOps = {
		.Drop = testRuntimeValueGraphChurnDrop
	};
	xrttype ObjectType = {
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("ValueGraphChurn"),
		.AbiName = XRT_STR_INIT("tests.runtime.ValueGraphChurn"),
		.Size = sizeof(ptr),
		.Align = TEST_ALIGNOF(ptr),
		.InstanceSize = sizeof(int64),
		.InstanceAlign = TEST_ALIGNOF(int64),
		.InstanceOps = &tObjectOps
	};

	ObjectType.Id = xrtTypeId(ObjectType.AbiName);
	testRequire(
		testRuntimeValueGraphChurnRound(&ObjectType, 16, 4, 987) &&
		testRuntimeValueGraphChurnRound(&ObjectType, 16, 4, 987) &&
		(testRuntimeValueGraphChurnDrops == 8),
		"runtime Value recursive graph leaked or corrupted ownership"
	);
	xrtClearError();
	printf("[PASS] runtime Value recursive graph churn\n");
	return 0;
}
