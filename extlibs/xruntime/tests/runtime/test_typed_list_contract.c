#include "../test.h"
#include "typed_callback_fixture.h"
#include "typed_value_fixture.h"



static xtypedlist* gTestTypedListCallbackTarget;



/* 从用户类型回调内执行无副作用查询。 */
static bool testTypedListCallbackProbe(void)
{
	(void)xrtTypedListCount(gTestTypedListCallbackTarget);
	return xrtErrorKind(xrtGetError()) == XERR_STATE;
}



/* 验证不可重定位拥有值、内部复制、失败原子替换和移动取出。 */
static void testTypedListLifecycle(void)
{
	xrttype Type = testTypedValueType();
	xtypedlist List;
	int iOwners = 2;
	testtypedvalue First = { 7, &iOwners };
	testtypedvalue Second = { 11, &iOwners };
	testtypedvalue Output = { 0, NULL };
	testtypedvalue* pFirst;

	Type.Flags &= ~XRT_TYPE_FLAG_RELOCATABLE;
	testRequire(xrtTypedListInit(&List, &Type), "owned typed list init failed");
	testRequire(
		xrtTypedListSet(&List, 1, &First) &&
		xrtTypedListSet(&List, 2, &Second) &&
		(iOwners == 4),
		"owned typed list copy count mismatch"
	);
	pFirst = (testtypedvalue*)xrtTypedListGet(&List, 1);
	testRequire(
		(pFirst != NULL) &&
		xrtTypedListSet(&List, 3, pFirst) &&
		(iOwners == 5),
		"typed list internal owned copy mismatch"
	);
	testRequire(
		xrtTypedListFind(&List, pFirst, NULL),
		"typed list rejected an exact active query slot"
	);
	xrtClearError();
	testRequire(
		!xrtTypedListFind(&List, (const bytes)pFirst + 1u, NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"typed list accepted a partial active query"
	);
	xrtClearError();
	testRequire(
		!xrtTypedListSet(&List, 4, (const bytes)pFirst + 1u) &&
		!xrtTypedListHas(&List, 4) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"typed list accepted a partial active copy source"
	);
	testTypedValueCopyFail(true);
	xrtClearError();
	testRequire(
		!xrtTypedListSet(&List, 2, &First) &&
		(((testtypedvalue*)xrtTypedListGet(&List, 2))->Value == 11) &&
		(xrtTypedListCount(&List) == 3u) &&
		(iOwners == 5),
		"typed list failed replacement changed visible state"
	);
	testRequire(
		(xrtErrorCause(xrtGetError()) != NULL) &&
		(strcmp(
			xrtErrorDomain(xrtErrorCause(xrtGetError())),
			"test.typed-value.copy"
		) == 0),
		"typed list copy failure cause mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtTypedListSet(&List, 4, &First) &&
		!xrtTypedListHas(&List, 4) &&
		(xrtTypedListCount(&List) == 3u) &&
		(iOwners == 5),
		"typed list failed insertion committed a value"
	);
	testTypedValueCopyFail(false);
	xrtClearError();
	testRequire(
		!xrtTypedListTake(&List, 2, xrtTypedListGet(&List, 1)) &&
		(strcmp(xrtErrorOperation(xrtGetError()), "take") == 0),
		"typed list take accepted an internal output"
	);
	testRequire(
		xrtTypedListTake(&List, 1, &Output) &&
		(Output.Value == 7) &&
		(iOwners == 5),
		"typed list owned take mismatch"
	);
	xrtTypedListUnit(&List);
	testRequire(iOwners == 3, "typed list unit leaked an owned value");
	xrtTypeDropValue(&Type, &First);
	xrtTypeDropValue(&Type, &Second);
	xrtTypeDropValue(&Type, &Output);
	testRequire(iOwners == 0, "typed list owned fixture cleanup mismatch");
}



/* 验证来源复制中途失败时合并目标和全部拥有关系保持不变。 */
static void testTypedListMergeFailure(void)
{
	xrttype Type = testTypedValueType();
	xtypedlist Target;
	xtypedlist Source;
	xtypedlist* pSnapshot;
	int iOwners = 3;
	testtypedvalue First = { 1, &iOwners };
	testtypedvalue Second = { 2, &iOwners };
	testtypedvalue Third = { 3, &iOwners };

	testRequire(
		xrtTypedListInit(&Target, &Type) &&
		xrtTypedListInit(&Source, &Type) &&
		xrtTypedListSet(&Target, 1, &First) &&
		xrtTypedListSet(&Target, 2, &Second) &&
		xrtTypedListSet(&Source, 2, &Third) &&
		xrtTypedListSet(&Source, 3, &Third),
		"typed list failed-merge fixture could not be built"
	);
	pSnapshot = xrtTypedListClone(&Target);
	testRequire(
		(pSnapshot != NULL) && (iOwners == 9),
		"typed list failed-merge snapshot mismatch"
	);
	testTypedValueCopyFailAfter(3u);
	xrtClearError();
	testRequire(
		!xrtTypedListMerge(&Target, &Source, true) &&
		xrtTypedListEquals(&Target, pSnapshot) &&
		(xrtTypedListCount(&Target) == 2u) &&
		(xrtTypedListCount(&Source) == 2u) &&
		(iOwners == 9),
		"typed list failed merge changed state or ownership"
	);
	testRequire(
		(xrtErrorCause(xrtGetError()) != NULL),
		"typed list failed merge lost its cause chain"
	);
	testTypedValueCopyFail(false);
	xrtTypedListDestroy(pSnapshot);
	xrtTypedListUnit(&Source);
	xrtTypedListUnit(&Target);
	xrtTypeDropValue(&Type, &First);
	xrtTypeDropValue(&Type, &Second);
	xrtTypeDropValue(&Type, &Third);
	testRequire(iOwners == 0, "typed list failed-merge fixture leaked values");
}



/* 验证事务提交即使版本碰撞，也会使提交前的迭代器失效。 */
static void testTypedListMergeInvalidatesIterator(void)
{
	xtypedlist Target;
	xtypedlist Source;
	xtypedlistiter Iterator;
	int64 iOldValue = 1;
	int64 iNewValue = 2;
	int64 iKey;

	testRequire(
		xrtTypedListInit(&Target, xrtTypeInt64()) &&
		xrtTypedListInit(&Source, xrtTypeInt64()) &&
		xrtTypedListSet(&Target, 7, &iOldValue) &&
		xrtTypedListSet(&Source, 7, &iNewValue) &&
		xrtTypedListIterBegin(&Target, &Iterator),
		"typed list merge iterator fixture could not be built"
	);
	testRequire(
		xrtTypedListMerge(&Target, &Source, true) &&
		(*(int64*)xrtTypedListGet(&Target, 7) == iNewValue),
		"typed list replacement merge failed"
	);
	xrtClearError();
	testRequire(
		(xrtTypedListIterNext(&Iterator, &iKey) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		(xrtErrorCode(xrtGetError()) == XTYPED_LIST_ERROR_STATE),
		"typed list merge did not invalidate an old iterator"
	);
	xrtTypedListIterEnd(&Iterator);
	xrtTypedListUnit(&Source);
	xrtTypedListUnit(&Target);
}



/* 验证所有用户类型回调都不能重入当前类型列表。 */
static void testTypedListCallbackReentry(void)
{
	xrttype Type = testTypedCallbackType();
	xtypedlist List;
	int iValue = 7;
	int iOutput = 0;
	int64 iKey;

	testRequire(
		xrtTypedListInit(&List, &Type),
		"typed list callback fixture init failed"
	);
	gTestTypedListCallbackTarget = &List;

	testTypedCallbackReset(
		TEST_TYPED_CALLBACK_INIT, testTypedListCallbackProbe
	);
	testRequire(
		xrtTypedListSet(&List, 7, &iValue) &&
		testTypedCallbackWasBlocked(),
		"typed list init callback reentry was allowed"
	);

	testTypedCallbackReset(
		TEST_TYPED_CALLBACK_COPY, testTypedListCallbackProbe
	);
	testRequire(
		xrtTypedListSet(&List, 7, &iValue) &&
		testTypedCallbackWasBlocked(),
		"typed list copy callback reentry was allowed"
	);

	testTypedCallbackReset(
		TEST_TYPED_CALLBACK_COMPARE, testTypedListCallbackProbe
	);
	testRequire(
		xrtTypedListFind(&List, &iValue, &iKey) &&
		(iKey == 7) &&
		testTypedCallbackWasBlocked(),
		"typed list compare callback reentry was allowed"
	);

	testTypedCallbackReset(
		TEST_TYPED_CALLBACK_TRACE, testTypedListCallbackProbe
	);
	testRequire(
		xrtTypedListInstanceOps()->Trace(
			&List, NULL, testTypedCallbackVisit, NULL
		) && testTypedCallbackWasBlocked(),
		"typed list trace callback reentry was allowed"
	);

	testTypedCallbackReset(
		TEST_TYPED_CALLBACK_MOVE, testTypedListCallbackProbe
	);
	testRequire(
		xrtTypedListTake(&List, 7, &iOutput) &&
		(iOutput == iValue) &&
		testTypedCallbackWasBlocked(),
		"typed list move callback reentry was allowed"
	);

	testTypedCallbackStop();
	testRequire(
		xrtTypedListSet(&List, 7, &iValue),
		"typed list drop callback fixture failed"
	);
	testTypedCallbackReset(
		TEST_TYPED_CALLBACK_DROP, testTypedListCallbackProbe
	);
	testRequire(
		xrtTypedListRemove(&List, 7) &&
		testTypedCallbackWasBlocked(),
		"typed list drop callback reentry was allowed"
	);

	testTypedCallbackStop();
	gTestTypedListCallbackTarget = NULL;
	xrtTypedListUnit(&List);
}



/* 验证元素能力、追加溢出和对象列表类型描述。 */
static void testTypedListRejection(void)
{
	const xrttype* arrArguments[] = { xrtTypeInt64() };
	xrttype NonCopyable = *xrtTypeInt64();
	xrttype ListType = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.typed-list.List")),
		.Kind = XRT_TYPE_LIST,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("List"),
		.AbiName = XRT_STR_INIT("tests.typed-list.List"),
		.Size = sizeof(ptr),
		.Align = TEST_ALIGNOF(ptr),
		.InstanceSize = sizeof(xtypedlist),
		.InstanceAlign = TEST_ALIGNOF(xtypedlist),
		.InstanceOps = xrtTypedListInstanceOps(),
		.ArgumentCount = 1u,
		.Arguments = arrArguments
	};
	xtypedlist List;
	int64 iValue = 1;

	NonCopyable.Flags &= ~(
		XRT_TYPE_FLAG_TRIVIAL_COPY | XRT_TYPE_FLAG_COPYABLE
	);
	testRequire(
		!xrtTypedListInit(&List, &NonCopyable),
		"noncopyable typed list item was accepted"
	);
	testRequire(
		xrtTypedListTypeValidate(&ListType),
		"typed list object descriptor was rejected"
	);
	ListType.Kind = XRT_TYPE_ARRAY;
	testRequire(
		!xrtTypedListTypeValidate(&ListType),
		"typed list accepted the wrong object kind"
	);
	testRequire(
		xrtTypedListInit(&List, xrtTypeInt64()) &&
		xrtTypedListSet(&List, INT64_MAX, &iValue),
		"typed list append overflow fixture failed"
	);
	xrtClearError();
	testRequire(
		!xrtTypedListAppend(&List, &iValue, NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		(xrtErrorCode(xrtGetError()) == XTYPED_LIST_ERROR_KEY),
		"typed list append overflow contract mismatch"
	);
	xrtTypedListUnit(&List);
}



/* 运行类型列表契约测试。 */
int main(void)
{
	testTypedListLifecycle();
	testTypedListMergeFailure();
	testTypedListMergeInvalidatesIterator();
	testTypedListCallbackReentry();
	testTypedListRejection();
	testTypedValueCopyFail(false);
	xrtClearError();
	printf("[PASS] typed list contract\n");
	return 0;
}
