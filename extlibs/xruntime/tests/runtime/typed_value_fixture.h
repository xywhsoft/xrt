#ifndef XRT_TEST_TYPED_VALUE_FIXTURE_H
#define XRT_TEST_TYPED_VALUE_FIXTURE_H



/* 共享拥有值用于验证类型容器的非平凡生命周期。 */
typedef struct testtypedvalue {
	int Value;
	int* Owners;
} testtypedvalue;



static bool gTestTypedValueCopyFails;
static size_t gTestTypedValueCopiesBeforeFailure = SIZE_MAX;



/* 初始化空的测试拥有值。 */
static bool testTypedValueInit(ptr pValue, const xrttype* pType)
{
	testtypedvalue* pItem = (testtypedvalue*)pValue;
	(void)pType;

	pItem->Value = 0;
	pItem->Owners = NULL;
	return true;
}



/* 失败原子地复制并替换一个测试拥有值。 */
static bool testTypedValueCopy(
	ptr pTarget,
	const void* pSource,
	const xrttype* pType
)
{
	testtypedvalue* pTargetItem = (testtypedvalue*)pTarget;
	const testtypedvalue* pSourceItem = (const testtypedvalue*)pSource;
	(void)pType;

	if ( gTestTypedValueCopyFails ||
		 (gTestTypedValueCopiesBeforeFailure == 0u) ) {
		xerror* pError = xrtErrorCreate(
			XERR_VALUE, "test.typed-value.copy", 17,
			"the test typed value copy was rejected"
		);

		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		return false;
	}
	if ( gTestTypedValueCopiesBeforeFailure != SIZE_MAX ) {
		gTestTypedValueCopiesBeforeFailure--;
	}
	if ( pSourceItem->Owners != NULL ) {
		(*pSourceItem->Owners)++;
	}
	if ( pTargetItem->Owners != NULL ) {
		(*pTargetItem->Owners)--;
	}
	*pTargetItem = *pSourceItem;
	return true;
}



/* 移动并替换一个测试拥有值。 */
static bool testTypedValueMove(
	ptr pTarget,
	ptr pSource,
	const xrttype* pType
)
{
	testtypedvalue* pTargetItem = (testtypedvalue*)pTarget;
	testtypedvalue* pSourceItem = (testtypedvalue*)pSource;
	(void)pType;

	if ( pTargetItem->Owners != NULL ) {
		(*pTargetItem->Owners)--;
	}
	*pTargetItem = *pSourceItem;
	pSourceItem->Value = 0;
	pSourceItem->Owners = NULL;
	return true;
}



/* 释放一个测试拥有值。 */
static void testTypedValueDrop(ptr pValue, const xrttype* pType)
{
	testtypedvalue* pItem = (testtypedvalue*)pValue;
	(void)pType;

	if ( pItem->Owners != NULL ) {
		(*pItem->Owners)--;
	}
	pItem->Value = 0;
	pItem->Owners = NULL;
}



/* 比较测试值内容。 */
static int testTypedValueCompare(
	const void* pLeft,
	const void* pRight,
	const xrttype* pType
)
{
	const testtypedvalue* pLeftItem = (const testtypedvalue*)pLeft;
	const testtypedvalue* pRightItem = (const testtypedvalue*)pRight;
	(void)pType;

	return pLeftItem->Value == pRightItem->Value ? 0 :
		(pLeftItem->Value < pRightItem->Value ? -1 : 1);
}



/* 按测试值内容生成稳定散列。 */
static uint64 testTypedValueHash(
	const void* pValue,
	const xrttype* pType
)
{
	const testtypedvalue* pItem = (const testtypedvalue*)pValue;
	(void)pType;

	return ((uint64)(uint32)pItem->Value * UINT64_C(11400714819323198485));
}



/* 构造共享测试拥有值的类型描述。 */
static xrttype testTypedValueType(void)
{
	static const xrttypeops Ops = {
		.Init = testTypedValueInit,
		.Copy = testTypedValueCopy,
		.Move = testTypedValueMove,
		.Drop = testTypedValueDrop,
		.Clone = testTypedValueCopy,
		.Compare = testTypedValueCompare,
		.Hash = testTypedValueHash
	};
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.typed.Value")),
		.Kind = XRT_TYPE_RECORD,
		.Flags = XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_RELOCATABLE,
		.Name = XRT_STR_INIT("Value"),
		.AbiName = XRT_STR_INIT("tests.typed.Value"),
		.Size = sizeof(testtypedvalue),
		.Align = TEST_ALIGNOF(testtypedvalue),
		.InstanceSize = sizeof(testtypedvalue),
		.InstanceAlign = TEST_ALIGNOF(testtypedvalue),
		.Ops = &Ops
	};

	return Type;
}



/* 控制后续测试值复制是否失败。 */
static void testTypedValueCopyFail(bool bFail)
{
	gTestTypedValueCopyFails = bFail;
	gTestTypedValueCopiesBeforeFailure = SIZE_MAX;
}



/* 在指定数量的成功复制后拒绝下一次测试值复制。 */
static inline void testTypedValueCopyFailAfter(size_t iSuccessfulCopies)
{
	gTestTypedValueCopyFails = false;
	gTestTypedValueCopiesBeforeFailure = iSuccessfulCopies;
}

#endif
