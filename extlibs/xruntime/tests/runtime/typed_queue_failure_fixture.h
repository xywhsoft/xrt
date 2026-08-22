#ifndef XRT_TEST_TYPED_QUEUE_FAILURE_FIXTURE_H
#define XRT_TEST_TYPED_QUEUE_FAILURE_FIXTURE_H



/* 可失败拥有值用于验证队列不会在类型回调失败后丢失元素。 */
typedef struct testtypedqueuevalue {
	int Value;
	int* Owners;
} testtypedqueuevalue;



static bool gTestTypedQueueCopyFails;
static bool gTestTypedQueueMoveFails;



/* 为失败夹具设置一个稳定值错误。 */
static void testTypedQueueValueError(cstr sOperation)
{
	xerror* pError = xrtErrorCreate(
		XERR_VALUE,
		"test.typed-queue.value",
		31,
		sOperation
	);

	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 初始化一个空的队列测试拥有值。 */
static bool testTypedQueueValueInit(ptr pValue, const xrttype* pType)
{
	testtypedqueuevalue* pItem = (testtypedqueuevalue*)pValue;
	(void)pType;

	pItem->Value = 0;
	pItem->Owners = NULL;
	return true;
}



/* 失败原子地复制并替换一个队列测试拥有值。 */
static bool testTypedQueueValueCopy(
	ptr pTarget,
	const void* pSource,
	const xrttype* pType
)
{
	testtypedqueuevalue* pTargetItem = (testtypedqueuevalue*)pTarget;
	const testtypedqueuevalue* pSourceItem =
		(const testtypedqueuevalue*)pSource;
	(void)pType;

	if ( gTestTypedQueueCopyFails ) {
		testTypedQueueValueError("copy rejected");
		return false;
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



/* 失败原子地移动并替换一个队列测试拥有值。 */
static bool testTypedQueueValueMove(
	ptr pTarget,
	ptr pSource,
	const xrttype* pType
)
{
	testtypedqueuevalue* pTargetItem = (testtypedqueuevalue*)pTarget;
	testtypedqueuevalue* pSourceItem = (testtypedqueuevalue*)pSource;
	(void)pType;

	if ( gTestTypedQueueMoveFails ) {
		testTypedQueueValueError("move rejected");
		return false;
	}
	if ( pTargetItem->Owners != NULL ) {
		(*pTargetItem->Owners)--;
	}
	*pTargetItem = *pSourceItem;
	pSourceItem->Value = 0;
	pSourceItem->Owners = NULL;
	return true;
}



/* 释放一个队列测试拥有值。 */
static void testTypedQueueValueDrop(ptr pValue, const xrttype* pType)
{
	testtypedqueuevalue* pItem = (testtypedqueuevalue*)pValue;
	(void)pType;

	if ( pItem->Owners != NULL ) {
		(*pItem->Owners)--;
	}
	pItem->Value = 0;
	pItem->Owners = NULL;
}



/* 构造具有可失败复制和移动操作的队列测试类型。 */
static xrttype testTypedQueueValueType(void)
{
	static const xrttypeops Ops = {
		.Init = testTypedQueueValueInit,
		.Copy = testTypedQueueValueCopy,
		.Move = testTypedQueueValueMove,
		.Drop = testTypedQueueValueDrop,
		.Clone = testTypedQueueValueCopy
	};
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.typed-queue.Value")),
		.Kind = XRT_TYPE_RECORD,
		.Flags = XRT_TYPE_FLAG_COPYABLE,
		.Name = XRT_STR_INIT("Value"),
		.AbiName = XRT_STR_INIT("tests.typed-queue.Value"),
		.Size = sizeof(testtypedqueuevalue),
		.Align = TEST_ALIGNOF(testtypedqueuevalue),
		.InstanceSize = sizeof(testtypedqueuevalue),
		.InstanceAlign = TEST_ALIGNOF(testtypedqueuevalue),
		.Ops = &Ops
	};

	return Type;
}



/* 设置后续复制和移动回调的故障模式。 */
static void testTypedQueueValueFailures(bool bCopy, bool bMove)
{
	gTestTypedQueueCopyFails = bCopy;
	gTestTypedQueueMoveFails = bMove;
	xrtClearError();
}

#endif
