#include "../test.h"
#include "typed_callback_fixture.h"
#include "typed_value_fixture.h"



static xtypedarray* gTestTypedArrayCallbackTarget;



/* 从用户类型回调内执行无副作用查询。 */
static bool testTypedArrayCallbackProbe(void)
{
	(void)xrtTypedArrayCount(gTestTypedArrayCallbackTarget);
	return xrtErrorKind(xrtGetError()) == XERR_STATE;
}



/* 验证非平凡元素所有权和复制失败原子性。 */
static void testArrayLifecycle(void)
{
	xrttype Type = testTypedValueType();
	xtypedarray Array;
	int iOwners = 2;
	testtypedvalue First = { 7, &iOwners };
	testtypedvalue Second = { 11, &iOwners };
	testtypedvalue Output = { 0, NULL };

	testRequire(xrtTypedArrayInit(&Array, &Type), "owned typed array init failed");
	testRequire(
		xrtTypedArrayPush(&Array, &First) &&
		xrtTypedArrayPush(&Array, &Second) &&
		(iOwners == 4),
		"owned typed array copy count mismatch"
	);
	testTypedValueCopyFail(true);
	xrtClearError();
	testRequire(
		!xrtTypedArrayPush(&Array, &First) &&
		(xrtTypedArrayCount(&Array) == 2u) &&
		(iOwners == 4),
		"typed array copy failure changed ownership or count"
	);
	testRequire(
		(xrtErrorCause(xrtGetError()) != NULL) &&
		(strcmp(
			xrtErrorDomain(xrtErrorCause(xrtGetError())),
			"test.typed-value.copy"
		) == 0),
		"typed array copy failure cause mismatch"
	);
	testTypedValueCopyFail(false);
	testRequire(
		xrtTypedArrayTake(&Array, 0u, &Output) &&
		(Output.Value == 7) &&
		(iOwners == 4),
		"typed array owned take mismatch"
	);
	xrtTypedArrayUnit(&Array);
	testRequire(iOwners == 3, "typed array unit leaked an owned item");
	xrtTypeDropValue(&Type, &First);
	xrtTypeDropValue(&Type, &Second);
	xrtTypeDropValue(&Type, &Output);
	testRequire(iOwners == 0, "owned fixture cleanup mismatch");
}



/* 验证所有用户类型回调都不能重入当前类型数组。 */
static void testTypedArrayCallbackReentry(void)
{
	xrttype Type = testTypedCallbackType();
	xtypedarray Array;
	xtypedarray Other;
	int iValue = 7;
	int iOutput = 0;

	testRequire(
		xrtTypedArrayInit(&Array, &Type) &&
		xrtTypedArrayInit(&Other, &Type),
		"typed array callback fixture init failed"
	);
	gTestTypedArrayCallbackTarget = &Array;

	testTypedCallbackReset(
		TEST_TYPED_CALLBACK_INIT, testTypedArrayCallbackProbe
	);
	testRequire(
		xrtTypedArrayPush(&Array, &iValue) &&
		testTypedCallbackWasBlocked(),
		"typed array init callback reentry was allowed"
	);

	testTypedCallbackReset(
		TEST_TYPED_CALLBACK_COPY, testTypedArrayCallbackProbe
	);
	testRequire(
		xrtTypedArraySet(&Array, 0u, &iValue) &&
		testTypedCallbackWasBlocked(),
		"typed array copy callback reentry was allowed"
	);

	testTypedCallbackReset(
		TEST_TYPED_CALLBACK_COMPARE, testTypedArrayCallbackProbe
	);
	testRequire(
		(xrtTypedArrayFind(&Array, &iValue) == 0u) &&
		testTypedCallbackWasBlocked(),
		"typed array compare callback reentry was allowed"
	);
	testTypedCallbackStop();
	testRequire(
		xrtTypedArrayPush(&Other, &iValue),
		"typed array equals fixture failed"
	);
	testTypedCallbackReset(
		TEST_TYPED_CALLBACK_COMPARE, testTypedArrayCallbackProbe
	);
	testRequire(
		xrtTypedArrayEquals(&Array, &Other) &&
		testTypedCallbackWasBlocked(),
		"typed array equals callback reentry was allowed"
	);

	testTypedCallbackReset(
		TEST_TYPED_CALLBACK_TRACE, testTypedArrayCallbackProbe
	);
	testRequire(
		xrtTypedArrayInstanceOps()->Trace(
			&Array, NULL, testTypedCallbackVisit, NULL
		) && testTypedCallbackWasBlocked(),
		"typed array trace callback reentry was allowed"
	);

	testTypedCallbackReset(
		TEST_TYPED_CALLBACK_MOVE, testTypedArrayCallbackProbe
	);
	testRequire(
		xrtTypedArrayTake(&Array, 0u, &iOutput) &&
		(iOutput == iValue) &&
		testTypedCallbackWasBlocked(),
		"typed array move callback reentry was allowed"
	);

	testTypedCallbackStop();
	testRequire(
		xrtTypedArrayPush(&Array, &iValue),
		"typed array drop callback fixture failed"
	);
	testTypedCallbackReset(
		TEST_TYPED_CALLBACK_DROP, testTypedArrayCallbackProbe
	);
	testRequire(
		xrtTypedArrayRemove(&Array, 0u, 1u) &&
		testTypedCallbackWasBlocked(),
		"typed array drop callback reentry was allowed"
	);

	testTypedCallbackStop();
	gTestTypedArrayCallbackTarget = NULL;
	xrtTypedArrayUnit(&Other);
	xrtTypedArrayUnit(&Array);
}



/* 验证元素类型能力和参数边界。 */
static void testArrayRejection(void)
{
	xtypedarray Array;
	xrttype NotCopyable = *xrtTypeInt64();
	xrttype NotRelocatable = *xrtTypeInt64();
	xrttype NotComparable = testTypedCallbackType();
	xrttypeops NotComparableOps = *NotComparable.Ops;
	xtypedarray Left;
	xtypedarray Right;
	int64 iValue = 1;

	NotCopyable.Flags &= ~XRT_TYPE_FLAG_COPYABLE;
	testRequire(
		!xrtTypedArrayInit(&Array, &NotCopyable),
		"noncopyable array item type was accepted"
	);
	NotRelocatable.Flags &= ~XRT_TYPE_FLAG_RELOCATABLE;
	testRequire(
		!xrtTypedArrayInit(&Array, &NotRelocatable),
		"nonrelocatable array item type was accepted"
	);
	testRequire(xrtTypedArrayInit(&Array, xrtTypeInt64()), "rejection fixture failed");
	xrtClearError();
	testRequire(
		!xrtTypedArrayInsert(&Array, 1u, &iValue),
		"out-of-range typed array insertion succeeded"
	);
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		(xrtErrorCode(xrtGetError()) == XTYPED_ARRAY_ERROR_RANGE),
		"typed array range error mismatch"
	);
	testRequire(xrtTypedArrayPush(&Array, &iValue), "pop fixture push failed");
	xrtClearError();
	testRequire(
		!xrtTypedArrayPop(&Array, NULL) &&
		(strcmp(xrtErrorOperation(xrtGetError()), "pop") == 0),
		"typed array pop error operation mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtTypedArrayTake(&Array, 0u, xrtTypedArrayGet(&Array, 0u)) &&
		(strcmp(xrtErrorOperation(xrtGetError()), "take") == 0),
		"typed array take accepted an overlapping output"
	);
	xrtTypedArrayUnit(&Array);
	NotComparableOps.Compare = NULL;
	NotComparable.Ops = &NotComparableOps;
	testRequire(
		xrtTypedArrayInit(&Left, &NotComparable) &&
		xrtTypedArrayInit(&Right, &NotComparable) &&
		xrtTypedArrayPush(&Left, &iValue) &&
		xrtTypedArrayPush(&Right, &iValue),
		"noncomparable typed array fixture failed"
	);
	xrtClearError();
	testRequire(
		!xrtTypedArrayEquals(&Left, &Right) &&
		(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED) &&
		(xrtErrorCode(xrtGetError()) == XTYPED_ARRAY_ERROR_TYPE),
		"noncomparable typed arrays were compared"
	);
	xrtTypedArrayUnit(&Right);
	xrtTypedArrayUnit(&Left);
}



/* 验证查询、复制和移动输出只能使用合法外部值或准确活动槽。 */
static void testArrayAliasBoundaries(void)
{
	xtypedarray Array;
	xtypedarray OveralignedArray;
	xrttype OveralignedType = testTypedCallbackType();
	int64 iFirst = 7;
	int64 iSecond = 11;
	const int64* pFirst;
	bytes pPartial;
	bytes pSpare;
	bytes pPadding;

	testRequire(
		xrtTypedArrayInit(&Array, xrtTypeInt64()) &&
		xrtTypedArrayReserve(&Array, 8u) &&
		xrtTypedArrayPush(&Array, &iFirst) &&
		xrtTypedArrayPush(&Array, &iSecond),
		"typed array alias fixture failed"
	);
	pFirst = (const int64*)xrtTypedArrayConstGet(&Array, 0u);
	pPartial = (bytes)pFirst + 1u;
	pSpare = Array.Storage.Data +
		(Array.Storage.Count * Array.Storage.ItemSize);
	testRequire(
		(pFirst != NULL) && (xrtTypedArrayFind(&Array, pFirst) == 0u),
		"typed array rejected an exact active query slot"
	);

	xrtClearError();
	testRequire(
		(xrtTypedArrayFind(&Array, pPartial) == SIZE_MAX) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"typed array accepted a partial active query"
	);
	xrtClearError();
	testRequire(
		!xrtTypedArrayPush(&Array, pPartial) &&
		(xrtTypedArrayCount(&Array) == 2u) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"typed array accepted a partial active copy source"
	);

	xrtClearError();
	testRequire(
		(xrtTypedArrayFind(&Array, pSpare) == SIZE_MAX) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"typed array accepted spare capacity as a query value"
	);
	xrtClearError();
	testRequire(
		!xrtTypedArrayTake(&Array, 0u, pSpare) &&
		(xrtTypedArrayCount(&Array) == 2u) &&
		(*(const int64*)xrtTypedArrayConstGet(&Array, 0u) == iFirst) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"typed array accepted spare capacity as a move output"
	);

	xrtTypedArrayUnit(&Array);

	OveralignedType.Size = 32u;
	OveralignedType.Align = 32u;
	OveralignedType.InstanceSize = 32u;
	OveralignedType.InstanceAlign = 32u;
	testRequire(
		xrtTypedArrayInit(&OveralignedArray, &OveralignedType) &&
		xrtTypedArrayReserve(&OveralignedArray, 2u),
		"overaligned typed array alias fixture failed"
	);
	pPadding = OveralignedArray.Storage.Data !=
		(bytes)OveralignedArray.Storage.Allocation ?
		(bytes)OveralignedArray.Storage.Allocation :
		OveralignedArray.Storage.Data +
			(OveralignedArray.Storage.Capacity *
			 OveralignedArray.Storage.ItemSize);
	xrtClearError();
	testRequire(
		(xrtTypedArrayFind(&OveralignedArray, pPadding) == SIZE_MAX) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"typed array accepted overaligned allocation padding as a value"
	);
	xrtTypedArrayUnit(&OveralignedArray);
}



/* 运行类型数组契约测试。 */
int main(void)
{
	testArrayLifecycle();
	testTypedArrayCallbackReentry();
	testArrayRejection();
	testArrayAliasBoundaries();
	xrtClearError();
	printf("[PASS] typed array contract\n");
	return 0;
}
