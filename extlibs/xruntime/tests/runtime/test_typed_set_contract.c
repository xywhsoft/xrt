#include "../test.h"
#include "typed_callback_fixture.h"
#include "typed_value_fixture.h"



static xtypedset* gTestTypedSetCallbackTarget;



/* 从用户类型回调内执行无副作用查询。 */
static bool testTypedSetCallbackProbe(void)
{
	(void)xrtTypedSetCount(gTestTypedSetCallbackTarget);
	return xrtErrorKind(xrtGetError()) == XERR_STATE;
}



/* 验证不可重定位拥有值、内部复制、失败原子插入和移动取出。 */
static void testTypedSetLifecycle(void)
{
	xrttype Type = testTypedValueType();
	xtypedset Set;
	int iOwners = 2;
	testtypedvalue First = { 7, &iOwners };
	testtypedvalue Second = { 11, &iOwners };
	testtypedvalue Missing = { 19, NULL };
	testtypedvalue Output = { 0, NULL };
	const testtypedvalue* pFirst;

	Type.Flags &= ~XRT_TYPE_FLAG_RELOCATABLE;
	testRequire(xrtTypedSetInit(&Set, &Type), "owned typed set init failed");
	testRequire(
		xrtTypedSetAdd(&Set, &First) &&
		xrtTypedSetAdd(&Set, &Second) &&
		(iOwners == 4),
		"owned typed set copy count mismatch"
	);
	pFirst = (const testtypedvalue*)xrtTypedSetGet(&Set, &First);
	testRequire(
		(pFirst != NULL) && xrtTypedSetAdd(&Set, pFirst) &&
		(xrtTypedSetCount(&Set) == 2u) && (iOwners == 4),
		"typed set internal canonical source mismatch"
	);
	testTypedValueCopyFail(true);
	xrtClearError();
	testRequire(
		!xrtTypedSetAdd(&Set, &Missing) &&
		(xrtTypedSetCount(&Set) == 2u) &&
		!xrtTypedSetHas(&Set, &Missing) &&
		(iOwners == 4),
		"typed set failed insertion changed visible state"
	);
	testRequire(
		(xrtErrorCause(xrtGetError()) != NULL) &&
		(strcmp(
			xrtErrorDomain(xrtErrorCause(xrtGetError())),
			"test.typed-value.copy"
		) == 0),
		"typed set copy failure cause mismatch"
	);
	testTypedValueCopyFail(false);
	xrtClearError();
	testRequire(
		!xrtTypedSetAdd(&Set, (const bytes)pFirst + 1u) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"typed set accepted a partial internal source"
	);
	xrtClearError();
	testRequire(
		!xrtTypedSetTake(&Set, &First, (ptr)pFirst) &&
		(strcmp(xrtErrorOperation(xrtGetError()), "take") == 0),
		"typed set take accepted an internal output"
	);
	testRequire(
		xrtTypedSetTake(&Set, &First, &Output) &&
		(Output.Value == 7) &&
		(xrtTypedSetCount(&Set) == 1u) &&
		(iOwners == 4),
		"typed set owned take mismatch"
	);
	xrtTypedSetUnit(&Set);
	testRequire(iOwners == 3, "typed set unit leaked an owned value");
	xrtTypeDropValue(&Type, &First);
	xrtTypeDropValue(&Type, &Second);
	xrtTypeDropValue(&Type, &Output);
	testRequire(iOwners == 0, "typed set owned fixture cleanup mismatch");
}



/* 验证复制中途失败时事务合并保持目标和拥有关系不变。 */
static void testTypedSetMergeFailure(void)
{
	xrttype Type = testTypedValueType();
	xtypedset Target;
	xtypedset Source;
	xtypedset* pSnapshot;
	int iOwners = 3;
	testtypedvalue First = { 1, &iOwners };
	testtypedvalue Second = { 2, &iOwners };
	testtypedvalue Third = { 3, &iOwners };

	testRequire(
		xrtTypedSetInit(&Target, &Type) &&
		xrtTypedSetInit(&Source, &Type) &&
		xrtTypedSetAdd(&Target, &First) &&
		xrtTypedSetAdd(&Source, &Second) &&
		xrtTypedSetAdd(&Source, &Third),
		"typed set failed-merge fixture could not be built"
	);
	pSnapshot = xrtTypedSetClone(&Target);
	testRequire(
		(pSnapshot != NULL) && (iOwners == 7),
		"typed set failed-merge snapshot mismatch"
	);
	testTypedValueCopyFailAfter(1u);
	xrtClearError();
	testRequire(
		!xrtTypedSetMerge(&Target, &Source) &&
		xrtTypedSetEquals(&Target, pSnapshot) &&
		(xrtTypedSetCount(&Target) == 1u) &&
		(xrtTypedSetCount(&Source) == 2u) &&
		(iOwners == 7),
		"typed set failed merge changed state or ownership"
	);
	testRequire(
		xrtErrorCause(xrtGetError()) != NULL,
		"typed set failed merge lost its cause chain"
	);
	testTypedValueCopyFail(false);
	xrtTypedSetDestroy(pSnapshot);
	xrtTypedSetUnit(&Source);
	xrtTypedSetUnit(&Target);
	xrtTypeDropValue(&Type, &First);
	xrtTypeDropValue(&Type, &Second);
	xrtTypeDropValue(&Type, &Third);
	testRequire(iOwners == 0, "typed set failed-merge fixture leaked values");
}



/* 验证所有用户类型回调都不能重入当前类型集合。 */
static void testTypedSetCallbackReentry(void)
{
	xrttype Type = testTypedCallbackType();
	xtypedset Set;
	int iOne = 1;
	int iTwo = 2;
	int iThree = 3;
	int iOutput = 0;

	testRequire(
		xrtTypedSetInit(&Set, &Type),
		"typed set callback fixture init failed"
	);
	gTestTypedSetCallbackTarget = &Set;
	testTypedCallbackStop();
	testRequire(
		xrtTypedSetAdd(&Set, &iOne),
		"typed set callback base value failed"
	);

	testTypedCallbackReset(
		TEST_TYPED_CALLBACK_HASH, testTypedSetCallbackProbe
	);
	testRequire(
		xrtTypedSetHas(&Set, &iOne) &&
		testTypedCallbackWasBlocked(),
		"typed set hash callback reentry was allowed"
	);

	testTypedCallbackReset(
		TEST_TYPED_CALLBACK_COMPARE, testTypedSetCallbackProbe
	);
	testRequire(
		xrtTypedSetHas(&Set, &iOne) &&
		testTypedCallbackWasBlocked(),
		"typed set compare callback reentry was allowed"
	);

	testTypedCallbackReset(
		TEST_TYPED_CALLBACK_INIT, testTypedSetCallbackProbe
	);
	testRequire(
		xrtTypedSetAdd(&Set, &iTwo) &&
		testTypedCallbackWasBlocked(),
		"typed set init callback reentry was allowed"
	);

	testTypedCallbackReset(
		TEST_TYPED_CALLBACK_COPY, testTypedSetCallbackProbe
	);
	testRequire(
		xrtTypedSetAdd(&Set, &iThree) &&
		testTypedCallbackWasBlocked(),
		"typed set copy callback reentry was allowed"
	);

	testTypedCallbackReset(
		TEST_TYPED_CALLBACK_TRACE, testTypedSetCallbackProbe
	);
	testRequire(
		xrtTypedSetInstanceOps()->Trace(
			&Set, NULL, testTypedCallbackVisit, NULL
		) && testTypedCallbackWasBlocked(),
		"typed set trace callback reentry was allowed"
	);

	testTypedCallbackReset(
		TEST_TYPED_CALLBACK_MOVE, testTypedSetCallbackProbe
	);
	testRequire(
		xrtTypedSetTake(&Set, &iOne, &iOutput) &&
		(iOutput == iOne) &&
		testTypedCallbackWasBlocked(),
		"typed set move callback reentry was allowed"
	);

	testTypedCallbackReset(
		TEST_TYPED_CALLBACK_DROP, testTypedSetCallbackProbe
	);
	testRequire(
		xrtTypedSetRemove(&Set, &iTwo) &&
		testTypedCallbackWasBlocked(),
		"typed set drop callback reentry was allowed"
	);

	testTypedCallbackStop();
	gTestTypedSetCallbackTarget = NULL;
	xrtTypedSetUnit(&Set);
}



/* 验证元素能力、精确类型身份和对象集合描述。 */
static void testTypedSetRejection(void)
{
	xrttype Type = testTypedValueType();
	xrttype Alias = Type;
	xrttypeops MissingCompareOps = *Type.Ops;
	xrttypeops MissingHashOps = *Type.Ops;
	const xrttype* arrArguments[] = { &Type };
	xrttype SetType = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.typed-set.Set")),
		.Kind = XRT_TYPE_SET,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("Set"),
		.AbiName = XRT_STR_INIT("tests.typed-set.Set"),
		.Size = sizeof(ptr),
		.Align = TEST_ALIGNOF(ptr),
		.InstanceSize = sizeof(xtypedset),
		.InstanceAlign = TEST_ALIGNOF(xtypedset),
		.InstanceOps = xrtTypedSetInstanceOps(),
		.ArgumentCount = 1u,
		.Arguments = arrArguments
	};
	xtypedset Left;
	xtypedset Right;
	xtypedset Invalid;

	MissingCompareOps.Compare = NULL;
	Alias.Ops = &MissingCompareOps;
	testRequire(
		!xrtTypedSetInit(&Invalid, &Alias),
		"noncomparable typed set item was accepted"
	);
	Alias = Type;
	MissingHashOps.Hash = NULL;
	Alias.Ops = &MissingHashOps;
	testRequire(
		!xrtTypedSetInit(&Invalid, &Alias),
		"nonhashable typed set item was accepted"
	);
	testRequire(
		xrtTypedSetTypeValidate(&SetType),
		"typed set object descriptor was rejected"
	);
	SetType.Kind = XRT_TYPE_LIST;
	testRequire(
		!xrtTypedSetTypeValidate(&SetType),
		"typed set accepted the wrong object kind"
	);

	Alias = Type;
	testRequire(
		xrtTypedSetInit(&Left, &Type) &&
		xrtTypedSetInit(&Right, &Alias),
		"typed set exact-type fixture failed"
	);
	xrtClearError();
	testRequire(
		(xrtTypedSetUnion(&Left, &Right) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_TYPE),
		"typed set accepted distinct lifecycle descriptor addresses"
	);
	xrtTypedSetUnit(&Right);
	xrtTypedSetUnit(&Left);
}



/* 运行类型集合契约测试。 */
int main(void)
{
	testTypedSetLifecycle();
	testTypedSetMergeFailure();
	testTypedSetCallbackReentry();
	testTypedSetRejection();
	testTypedValueCopyFail(false);
	xrtClearError();
	printf("[PASS] typed set contract\n");
	return 0;
}
