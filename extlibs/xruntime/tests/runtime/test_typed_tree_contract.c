#include "../test.h"
#include "typed_callback_fixture.h"
#include "typed_value_fixture.h"



static xtypedtree* gTestTypedTreeCallbackTarget;



/* 初始化一个空的测试拥有字符串。 */
static bool testTypedTreeStringInit(ptr pValue, const xrttype* pType)
{
	(void)pType;
	*(str*)pValue = NULL;
	return true;
}



/* 失败原子地深复制并替换一个测试拥有字符串。 */
static bool testTypedTreeStringCopy(
	ptr pTarget,
	const void* pSource,
	const xrttype* pType
)
{
	str* pTargetText = (str*)pTarget;
	cstr sSource = *(const cstr*)pSource;
	str sCopy = NULL;
	size_t iSize;
	(void)pType;

	if ( sSource != NULL ) {
		iSize = strlen(sSource);
		sCopy = (str)xrtMalloc(iSize + 1u);
		if ( sCopy == NULL ) {
			return false;
		}
		memcpy(sCopy, sSource, iSize + 1u);
	}
	xrtFree(*pTargetText);
	*pTargetText = sCopy;
	return true;
}



/* 移动并清空一个测试拥有字符串。 */
static bool testTypedTreeStringMove(
	ptr pTarget,
	ptr pSource,
	const xrttype* pType
)
{
	str* pTargetText = (str*)pTarget;
	str* pSourceText = (str*)pSource;
	(void)pType;

	xrtFree(*pTargetText);
	*pTargetText = *pSourceText;
	*pSourceText = NULL;
	return true;
}



/* 释放一个测试拥有字符串。 */
static void testTypedTreeStringDrop(ptr pValue, const xrttype* pType)
{
	(void)pType;
	xrtFree(*(str*)pValue);
	*(str*)pValue = NULL;
}



/* 按字节字典序比较两个可空测试字符串。 */
static int testTypedTreeStringCompare(
	const void* pLeft,
	const void* pRight,
	const xrttype* pType
)
{
	cstr sLeft = *(const cstr*)pLeft;
	cstr sRight = *(const cstr*)pRight;
	(void)pType;

	if ( (sLeft == NULL) || (sRight == NULL) ) {
		return (sLeft != NULL) - (sRight != NULL);
	}
	return strcmp(sLeft, sRight);
}



/* 构造用于继承旧版类型 AVL 字符串回归的拥有类型。 */
static xrttype testTypedTreeStringType(void)
{
	static const xrttypeops Ops = {
		.Init = testTypedTreeStringInit,
		.Copy = testTypedTreeStringCopy,
		.Move = testTypedTreeStringMove,
		.Drop = testTypedTreeStringDrop,
		.Clone = testTypedTreeStringCopy,
		.Compare = testTypedTreeStringCompare
	};
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.typed-tree.String")),
		.Kind = XRT_TYPE_STRING,
		.Flags = XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_RELOCATABLE,
		.Name = XRT_STR_INIT("String"),
		.AbiName = XRT_STR_INIT("tests.typed-tree.String"),
		.Size = sizeof(str),
		.Align = TEST_ALIGNOF(str),
		.InstanceSize = sizeof(str),
		.InstanceAlign = TEST_ALIGNOF(str),
		.Ops = &Ops
	};

	return Type;
}



/* 从用户类型回调内执行同一棵类型树的无副作用查询。 */
static bool testTypedTreeCallbackProbe(void)
{
	(void)xrtTypedTreeCount(gTestTypedTreeCallbackTarget);
	return xrtErrorKind(xrtGetError()) == XERR_STATE;
}



/* 验证旧版类型 AVL 的深复制字符串键值和自然升序行为。 */
static void testTypedTreeStringKeys(void)
{
	xrttype Type = testTypedTreeStringType();
	xtypedtree Tree;
	char arrMutableKey[] = "beta";
	str sMutableKey = arrMutableKey;
	str sAlphaKey = "alpha";
	str sBetaSearch = "beta";
	str sAlphaValue = "first";
	str sBetaValue = "second";
	const void* pStoredKey;
	const str* pStoredValue;

	testRequire(
		xrtTypedTreeInit(&Tree, &Type, &Type) &&
		xrtTypedTreeSet(&Tree, &sMutableKey, &sBetaValue) &&
		xrtTypedTreeSet(&Tree, &sAlphaKey, &sAlphaValue),
		"typed tree string fixture insertion failed"
	);
	memset(arrMutableKey, 'x', sizeof(arrMutableKey) - 1u);
	pStoredValue = (const str*)xrtTypedTreeConstGet(
		&Tree, &sBetaSearch
	);
	testRequire(
		(pStoredValue != NULL) &&
		(strcmp(*pStoredValue, "second") == 0),
		"typed tree did not own its string key or value"
	);
	pStoredValue = (const str*)xrtTypedTreeFirst(&Tree, &pStoredKey);
	testRequire(
		(pStoredValue != NULL) &&
		(strcmp(*(const cstr*)pStoredKey, "alpha") == 0) &&
		(strcmp(*pStoredValue, "first") == 0),
		"typed tree string key order mismatch"
	);
	xrtTypedTreeUnit(&Tree);
}



/* 验证键值非平凡所有权和替换失败原子性。 */
static void testTypedTreeLifecycle(void)
{
	xrttype Type = testTypedValueType();
	xtypedtree Tree;
	int iOwners = 2;
	testtypedvalue Key = { 7, &iOwners };
	testtypedvalue Value = { 11, &iOwners };
	testtypedvalue Replacement = { 19, &iOwners };
	const testtypedvalue* pStored;

	iOwners++;
	testRequire(
		xrtTypedTreeInit(&Tree, &Type, &Type) &&
		xrtTypedTreeSet(&Tree, &Key, &Value) &&
		(iOwners == 5),
		"typed tree owned insertion count mismatch"
	);
	testTypedValueCopyFail(true);
	xrtClearError();
	testRequire(
		!xrtTypedTreeSet(&Tree, &Key, &Replacement) &&
		(xrtTypedTreeCount(&Tree) == 1u),
		"typed tree failed replacement changed its count"
	);
	pStored = (const testtypedvalue*)xrtTypedTreeConstGet(&Tree, &Key);
	testRequire(
		(pStored != NULL) && (pStored->Value == 11) &&
		(iOwners == 5) &&
		(xrtErrorCause(xrtGetError()) != NULL),
		"typed tree failed replacement changed ownership or root error"
	);
	testTypedValueCopyFail(false);
	xrtTypedTreeUnit(&Tree);
	testRequire(iOwners == 3, "typed tree unit leaked an owned key or value");
	xrtTypeDropValue(&Type, &Key);
	xrtTypeDropValue(&Type, &Value);
	xrtTypeDropValue(&Type, &Replacement);
	testRequire(iOwners == 0, "typed tree lifecycle fixture cleanup mismatch");
}



/* 验证所有适用用户类型回调都不能重入当前类型树。 */
static void testTypedTreeCallbackReentry(void)
{
	xrttype Type = testTypedCallbackType();
	xtypedtree Tree;
	int iKey = 1;
	int iOtherKey = 2;
	int iValue = 10;
	int iMoved = 20;

	testRequire(
		xrtTypedTreeInit(&Tree, &Type, &Type),
		"typed tree callback fixture init failed"
	);
	gTestTypedTreeCallbackTarget = &Tree;
	testTypedCallbackReset(
		TEST_TYPED_CALLBACK_INIT, testTypedTreeCallbackProbe
	);
	testRequire(
		xrtTypedTreeSet(&Tree, &iKey, &iValue) &&
		testTypedCallbackWasBlocked(),
		"typed tree init callback reentry was allowed"
	);
	testTypedCallbackReset(
		TEST_TYPED_CALLBACK_COPY, testTypedTreeCallbackProbe
	);
	testRequire(
		xrtTypedTreeSet(&Tree, &iKey, &iValue) &&
		testTypedCallbackWasBlocked(),
		"typed tree copy callback reentry was allowed"
	);
	testTypedCallbackReset(
		TEST_TYPED_CALLBACK_COMPARE, testTypedTreeCallbackProbe
	);
	testRequire(
		xrtTypedTreeHas(&Tree, &iKey) &&
		testTypedCallbackWasBlocked(),
		"typed tree compare callback reentry was allowed"
	);
	testTypedCallbackReset(
		TEST_TYPED_CALLBACK_MOVE, testTypedTreeCallbackProbe
	);
	testRequire(
		xrtTypedTreeSetTake(&Tree, &iKey, &iMoved) &&
		(iMoved == 0) && testTypedCallbackWasBlocked(),
		"typed tree move callback reentry was allowed"
	);
	testTypedCallbackReset(
		TEST_TYPED_CALLBACK_TRACE, testTypedTreeCallbackProbe
	);
	testRequire(
		xrtTypedTreeInstanceOps()->Trace(
			&Tree, NULL, testTypedCallbackVisit, NULL
		) && testTypedCallbackWasBlocked(),
		"typed tree trace callback reentry was allowed"
	);
	testTypedCallbackStop();
	testRequire(
		xrtTypedTreeSet(&Tree, &iOtherKey, &iValue),
		"typed tree drop callback fixture insertion failed"
	);
	testTypedCallbackReset(
		TEST_TYPED_CALLBACK_DROP, testTypedTreeCallbackProbe
	);
	testRequire(
		xrtTypedTreeRemove(&Tree, &iOtherKey) &&
		testTypedCallbackWasBlocked(),
		"typed tree drop callback reentry was allowed"
	);
	testTypedCallbackStop();
	gTestTypedTreeCallbackTarget = NULL;
	xrtTypedTreeUnit(&Tree);
}



/* 验证事务合并在用户复制失败时保留目标树和所有权。 */
static void testTypedTreeMergeFailure(void)
{
	xrttype Type = testTypedValueType();
	xtypedtree Target;
	xtypedtree Source;
	testtypedvalue TargetKeyOne = { 1, NULL };
	testtypedvalue TargetKeyTwo = { 2, NULL };
	testtypedvalue SourceKeyTwo = { 2, NULL };
	testtypedvalue SourceKeyThree = { 3, NULL };
	testtypedvalue TargetValueOne = { 10, NULL };
	testtypedvalue TargetValueTwo = { 20, NULL };
	testtypedvalue SourceValueTwo = { 200, NULL };
	testtypedvalue SourceValueThree = { 300, NULL };

	testRequire(
		xrtTypedTreeInit(&Target, &Type, &Type) &&
		xrtTypedTreeInit(&Source, &Type, &Type) &&
		xrtTypedTreeSet(&Target, &TargetKeyOne, &TargetValueOne) &&
		xrtTypedTreeSet(&Target, &TargetKeyTwo, &TargetValueTwo) &&
		xrtTypedTreeSet(&Source, &SourceKeyTwo, &SourceValueTwo) &&
		xrtTypedTreeSet(&Source, &SourceKeyThree, &SourceValueThree),
		"typed tree merge failure fixture failed"
	);
	testTypedValueCopyFailAfter(4u);
	xrtClearError();
	testRequire(
		!xrtTypedTreeMerge(&Target, &Source, true) &&
		(xrtTypedTreeCount(&Target) == 2u) &&
		(((const testtypedvalue*)xrtTypedTreeConstGet(
			&Target, &TargetKeyTwo
		))->Value == 20) &&
		!xrtTypedTreeHas(&Target, &SourceKeyThree),
		"typed tree failed merge changed the target"
	);
	testRequire(
		(xrtErrorCause(xrtGetError()) != NULL),
		"typed tree failed merge lost its root cause"
	);
	testTypedValueCopyFail(false);
	xrtTypedTreeUnit(&Source);
	xrtTypedTreeUnit(&Target);
}



/* 验证能力、精确类型身份、内部边界和对象描述。 */
static void testTypedTreeRejection(void)
{
	xrttype Type = testTypedCallbackType();
	xrttype Alias = Type;
	xrttype MissingCompare = Type;
	xrttypeops MissingCompareOps = *Type.Ops;
	xrttype NonCopyable = Type;
	const xrttype* arrArguments[] = { &Type, &Type };
	xrttype TreeType = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.typed-tree.Tree")),
		.Kind = XRT_TYPE_DICT,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("Tree"),
		.AbiName = XRT_STR_INIT("tests.typed-tree.Tree"),
		.Size = sizeof(ptr),
		.Align = TEST_ALIGNOF(ptr),
		.InstanceSize = sizeof(xtypedtree),
		.InstanceAlign = TEST_ALIGNOF(xtypedtree),
		.InstanceOps = xrtTypedTreeInstanceOps(),
		.ArgumentCount = 2u,
		.Arguments = arrArguments
	};
	xtypedtree Left;
	xtypedtree Right;
	xtypedtree Invalid;
	int iKey = 1;
	int iValue = 10;
	int iOverlap = 1;
	const void* pStoredKey;
	ptr pStoredValue;

	MissingCompareOps.Compare = NULL;
	MissingCompare.Ops = &MissingCompareOps;
	testRequire(
		!xrtTypedTreeInit(&Invalid, &MissingCompare, &Type),
		"typed tree accepted a noncomparable key"
	);
	NonCopyable.Flags &= ~XRT_TYPE_FLAG_COPYABLE;
	testRequire(
		!xrtTypedTreeInit(&Invalid, &Type, &NonCopyable),
		"typed tree accepted a noncopyable value"
	);
	testRequire(
		xrtTypedTreeTypeValidate(&TreeType),
		"typed tree object descriptor was rejected"
	);
	TreeType.ArgumentCount = 1u;
	testRequire(
		!xrtTypedTreeTypeValidate(&TreeType),
		"typed tree accepted an incomplete generic descriptor"
	);
	testRequire(
		xrtTypedTreeInit(&Left, &Type, &Type) &&
		xrtTypedTreeInit(&Right, &Alias, &Type),
		"typed tree exact-type fixture failed"
	);
	xrtClearError();
	testRequire(
		!xrtTypedTreeMerge(&Left, &Right, true) &&
		(xrtErrorKind(xrtGetError()) == XERR_TYPE),
		"typed tree accepted distinct key descriptor addresses"
	);
	xrtTypedTreeUnit(&Right);
	testRequire(
		xrtTypedTreeSet(&Left, &iKey, &iValue),
		"typed tree internal boundary fixture failed"
	);
	xrtClearError();
	testRequire(
		!xrtTypedTreeSetTake(&Left, &iOverlap, &iOverlap) &&
		(iOverlap == 1) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"typed tree accepted overlapping move key and value"
	);
	xrtClearError();
	testRequire(
		!xrtTypedTreeTake(&Left, &iOverlap, &iOverlap) &&
		(iOverlap == 1) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"typed tree accepted overlapping take key and output"
	);
	testRequire(
		xrtTypedTreeHas(&Left, &iKey),
		"typed tree overlapping take changed the stored entry"
	);
	pStoredValue = xrtTypedTreeFirst(&Left, &pStoredKey);
	xrtClearError();
	testRequire(
		!xrtTypedTreeSet(
			&Left, (const bytes)pStoredKey + 1u, pStoredValue
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"typed tree accepted a partial internal key"
	);
	xrtClearError();
	testRequire(
		!xrtTypedTreeSetTake(&Left, pStoredKey, pStoredValue) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"typed tree accepted an internal movable value"
	);
	xrtTypedTreeUnit(&Left);
}



/* 运行类型树契约测试。 */
int main(void)
{
	testTypedTreeStringKeys();
	testTypedTreeLifecycle();
	testTypedTreeCallbackReentry();
	testTypedTreeMergeFailure();
	testTypedTreeRejection();
	xrtClearError();
	printf("[PASS] typed tree contract\n");
	return 0;
}
