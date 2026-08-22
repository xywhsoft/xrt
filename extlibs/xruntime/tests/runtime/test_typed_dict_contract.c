#include "../test.h"
#include "typed_callback_fixture.h"
#include "typed_value_fixture.h"



static xtypeddict* gTestTypedDictCallbackTarget;



/* 从用户类型回调内执行无副作用查询。 */
static bool testTypedDictCallbackProbe(void)
{
	(void)xrtTypedDictCount(gTestTypedDictCallbackTarget);
	return xrtErrorKind(xrtGetError()) == XERR_STATE;
}



/* 验证不可重定位拥有值、失败原子替换、内部复制和移动取出。 */
static void testTypedDictLifecycle(void)
{
	xrttype Type = testTypedValueType();
	xtypeddict Dict;
	int iOwners = 2;
	testtypedvalue First = { 7, &iOwners };
	testtypedvalue Second = { 11, &iOwners };
	testtypedvalue Output = { 0, NULL };
	testtypedvalue* pFirst;

	Type.Flags &= ~XRT_TYPE_FLAG_RELOCATABLE;
	testRequire(
		xrtTypedDictInit(&Dict, &Type) &&
		xrtTypedDictSet(&Dict, XRT_STR_LITERAL("first"), &First) &&
		xrtTypedDictSet(&Dict, XRT_STR_LITERAL("second"), &Second) &&
		(iOwners == 4),
		"owned typed dictionary setup mismatch"
	);
	pFirst = (testtypedvalue*)xrtTypedDictGet(
		&Dict, XRT_STR_LITERAL("first")
	);
	testRequire(
		(pFirst != NULL) &&
		xrtTypedDictSet(&Dict, XRT_STR_LITERAL("copy"), pFirst) &&
		(iOwners == 5),
		"typed dictionary internal owned copy mismatch"
	);
	testTypedValueCopyFail(true);
	xrtClearError();
	testRequire(
		!xrtTypedDictSet(&Dict, XRT_STR_LITERAL("second"), &First) &&
		(((testtypedvalue*)xrtTypedDictGet(
			&Dict, XRT_STR_LITERAL("second")
		))->Value == 11) &&
		(xrtTypedDictCount(&Dict) == 3u) &&
		(iOwners == 5),
		"typed dictionary failed replacement changed state"
	);
	testRequire(
		(xrtErrorCause(xrtGetError()) != NULL) &&
		(strcmp(
			xrtErrorDomain(xrtErrorCause(xrtGetError())),
			"test.typed-value.copy"
		) == 0),
		"typed dictionary replacement cause mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtTypedDictSet(&Dict, XRT_STR_LITERAL("failed"), &First) &&
		!xrtTypedDictHas(&Dict, XRT_STR_LITERAL("failed")) &&
		(iOwners == 5),
		"typed dictionary failed insertion committed a key"
	);
	testTypedValueCopyFail(false);
	xrtClearError();
	testRequire(
		!xrtTypedDictSet(
			&Dict, XRT_STR_LITERAL("bad"), (bytes)pFirst + 1u
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"typed dictionary accepted a partial internal source"
	);
	xrtClearError();
	testRequire(
		!xrtTypedDictTake(
			&Dict, XRT_STR_LITERAL("first"), pFirst
		) &&
		(strcmp(xrtErrorOperation(xrtGetError()), "take") == 0),
		"typed dictionary take accepted an internal output"
	);
	testRequire(
		xrtTypedDictTake(
			&Dict, XRT_STR_LITERAL("first"), &Output
		) &&
		(Output.Value == 7) &&
		(iOwners == 5),
		"typed dictionary owned take mismatch"
	);
	xrtTypedDictUnit(&Dict);
	testRequire(iOwners == 3, "typed dictionary unit leaked an owned value");
	xrtTypeDropValue(&Type, &First);
	xrtTypeDropValue(&Type, &Second);
	xrtTypeDropValue(&Type, &Output);
	testRequire(iOwners == 0, "typed dictionary owned fixture cleanup mismatch");
}



/* 验证复制中途失败时事务合并保持目标和拥有关系不变。 */
static void testTypedDictMergeFailure(void)
{
	xrttype Type = testTypedValueType();
	xtypeddict Target;
	xtypeddict Source;
	xtypeddict* pSnapshot;
	int iOwners = 3;
	testtypedvalue First = { 1, &iOwners };
	testtypedvalue Second = { 2, &iOwners };
	testtypedvalue Third = { 3, &iOwners };

	testRequire(
		xrtTypedDictInit(&Target, &Type) &&
		xrtTypedDictInit(&Source, &Type) &&
		xrtTypedDictSet(&Target, XRT_STR_LITERAL("one"), &First) &&
		xrtTypedDictSet(&Source, XRT_STR_LITERAL("one"), &Second) &&
		xrtTypedDictSet(&Source, XRT_STR_LITERAL("three"), &Third),
		"typed dictionary failed-merge fixture could not be built"
	);
	pSnapshot = xrtTypedDictClone(&Target);
	testRequire(
		(pSnapshot != NULL) && (iOwners == 7),
		"typed dictionary failed-merge snapshot mismatch"
	);
	testTypedValueCopyFailAfter(2u);
	xrtClearError();
	testRequire(
		!xrtTypedDictMerge(&Target, &Source, true) &&
		xrtTypedDictEquals(&Target, pSnapshot) &&
		(xrtTypedDictCount(&Target) == 1u) &&
		(xrtTypedDictCount(&Source) == 2u) &&
		(iOwners == 7),
		"typed dictionary failed merge changed state or ownership"
	);
	testRequire(
		xrtErrorCause(xrtGetError()) != NULL,
		"typed dictionary failed merge lost its cause chain"
	);
	testTypedValueCopyFail(false);
	xrtTypedDictDestroy(pSnapshot);
	xrtTypedDictUnit(&Source);
	xrtTypedDictUnit(&Target);
	xrtTypeDropValue(&Type, &First);
	xrtTypeDropValue(&Type, &Second);
	xrtTypeDropValue(&Type, &Third);
	testRequire(iOwners == 0, "typed dictionary failed-merge fixture leaked values");
}



/* 验证所有用户类型回调都不能重入当前类型字典。 */
static void testTypedDictCallbackReentry(void)
{
	xrttype Type = testTypedCallbackType();
	xtypeddict Dict;
	xtypeddict Other;
	int iZero = 0;
	int iOne = 1;
	int iTwo = 2;
	bool bNew;

	testRequire(
		xrtTypedDictInit(&Dict, &Type) &&
		xrtTypedDictInit(&Other, &Type),
		"typed dictionary callback fixture init failed"
	);
	gTestTypedDictCallbackTarget = &Dict;

	testTypedCallbackReset(
		TEST_TYPED_CALLBACK_INIT, testTypedDictCallbackProbe
	);
	testRequire(
		(xrtTypedDictGetOrAdd(
			&Dict, XRT_STR_LITERAL("init"), &bNew
		) != NULL) && bNew &&
		testTypedCallbackWasBlocked(),
		"typed dictionary init callback reentry was allowed"
	);

	testTypedCallbackReset(
		TEST_TYPED_CALLBACK_COPY, testTypedDictCallbackProbe
	);
	testRequire(
		xrtTypedDictSet(
			&Dict, XRT_STR_LITERAL("copy"), &iOne
		) && testTypedCallbackWasBlocked(),
		"typed dictionary copy callback reentry was allowed"
	);

	testTypedCallbackReset(
		TEST_TYPED_CALLBACK_MOVE, testTypedDictCallbackProbe
	);
	testRequire(
		xrtTypedDictSetTake(
			&Dict, XRT_STR_LITERAL("move"), &iTwo
		) && (iTwo == 0) && testTypedCallbackWasBlocked(),
		"typed dictionary move callback reentry was allowed"
	);

	testTypedCallbackStop();
	iTwo = 2;
	testRequire(
		xrtTypedDictSet(
			&Other, XRT_STR_LITERAL("init"), &iZero
		) &&
		xrtTypedDictSet(
			&Other, XRT_STR_LITERAL("copy"), &iOne
		) &&
		xrtTypedDictSet(
			&Other, XRT_STR_LITERAL("move"), &iTwo
		),
		"typed dictionary compare fixture failed"
	);
	testTypedCallbackReset(
		TEST_TYPED_CALLBACK_COMPARE, testTypedDictCallbackProbe
	);
	testRequire(
		xrtTypedDictEquals(&Dict, &Other) &&
		testTypedCallbackWasBlocked(),
		"typed dictionary compare callback reentry was allowed"
	);

	testTypedCallbackReset(
		TEST_TYPED_CALLBACK_TRACE, testTypedDictCallbackProbe
	);
	testRequire(
		xrtTypedDictInstanceOps()->Trace(
			&Dict, NULL, testTypedCallbackVisit, NULL
		) && testTypedCallbackWasBlocked(),
		"typed dictionary trace callback reentry was allowed"
	);

	testTypedCallbackReset(
		TEST_TYPED_CALLBACK_DROP, testTypedDictCallbackProbe
	);
	testRequire(
		xrtTypedDictRemove(&Dict, XRT_STR_LITERAL("copy")) &&
		testTypedCallbackWasBlocked(),
		"typed dictionary drop callback reentry was allowed"
	);

	testTypedCallbackStop();
	gTestTypedDictCallbackTarget = NULL;
	xrtTypedDictUnit(&Other);
	xrtTypedDictUnit(&Dict);
}



/* 验证元素能力、精确类型身份和对象字典描述。 */
static void testTypedDictRejection(void)
{
	xrttype Type = testTypedValueType();
	xrttype Alias = Type;
	xrttype NonCopyable = Type;
	xrttype NonComparable = Type;
	xrttypeops NonComparableOps = *Type.Ops;
	const xrttype* arrArguments[] = { &Type };
	xrttype DictType = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.typed-dict.Dict")),
		.Kind = XRT_TYPE_DICT,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("Dict"),
		.AbiName = XRT_STR_INIT("tests.typed-dict.Dict"),
		.Size = sizeof(ptr),
		.Align = TEST_ALIGNOF(ptr),
		.InstanceSize = sizeof(xtypeddict),
		.InstanceAlign = TEST_ALIGNOF(xtypeddict),
		.InstanceOps = xrtTypedDictInstanceOps(),
		.ArgumentCount = 1u,
		.Arguments = arrArguments
	};
	xtypeddict Left;
	xtypeddict Right;
	xtypeddict Invalid;

	NonCopyable.Flags &= ~XRT_TYPE_FLAG_COPYABLE;
	testRequire(
		!xrtTypedDictInit(&Invalid, &NonCopyable),
		"noncopyable typed dictionary item was accepted"
	);
	testRequire(
		xrtTypedDictTypeValidate(&DictType),
		"typed dictionary object descriptor was rejected"
	);
	DictType.Kind = XRT_TYPE_SET;
	testRequire(
		!xrtTypedDictTypeValidate(&DictType),
		"typed dictionary accepted the wrong object kind"
	);
	testRequire(
		xrtTypedDictInit(&Left, &Type) &&
		xrtTypedDictInit(&Right, &Alias),
		"typed dictionary exact-type fixture failed"
	);
	xrtClearError();
	testRequire(
		!xrtTypedDictMerge(&Left, &Right, true) &&
		(xrtErrorKind(xrtGetError()) == XERR_TYPE),
		"typed dictionary accepted distinct lifecycle descriptor addresses"
	);
	xrtTypedDictUnit(&Right);
	xrtTypedDictUnit(&Left);

	NonComparableOps.Compare = NULL;
	NonComparable.Ops = &NonComparableOps;
	testRequire(
		xrtTypedDictInit(&Left, &NonComparable) &&
		xrtTypedDictInit(&Right, &NonComparable),
		"noncomparable typed dictionary setup failed"
	);
	xrtClearError();
	testRequire(
		!xrtTypedDictEquals(&Left, &Right) &&
		(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED),
		"typed dictionary equality accepted noncomparable values"
	);
	xrtTypedDictUnit(&Right);
	xrtTypedDictUnit(&Left);
}



/* 运行类型字典契约测试。 */
int main(void)
{
	testTypedDictLifecycle();
	testTypedDictMergeFailure();
	testTypedDictCallbackReentry();
	testTypedDictRejection();
	testTypedValueCopyFail(false);
	xrtClearError();
	printf("[PASS] typed dictionary contract\n");
	return 0;
}
