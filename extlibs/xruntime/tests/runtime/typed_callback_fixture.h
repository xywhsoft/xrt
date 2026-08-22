#ifndef XRT_TEST_TYPED_CALLBACK_FIXTURE_H
#define XRT_TEST_TYPED_CALLBACK_FIXTURE_H



/* 类型回调类别用于逐项验证容器回调门禁。 */
typedef enum testtypedcallback {
	TEST_TYPED_CALLBACK_NONE = 0,
	TEST_TYPED_CALLBACK_INIT,
	TEST_TYPED_CALLBACK_COPY,
	TEST_TYPED_CALLBACK_MOVE,
	TEST_TYPED_CALLBACK_DROP,
	TEST_TYPED_CALLBACK_COMPARE,
	TEST_TYPED_CALLBACK_HASH,
	TEST_TYPED_CALLBACK_TRACE
} testtypedcallback;



/* 探针在类型回调中执行一次无副作用的同容器 API，并返回是否被拒绝。 */
typedef bool (*testtypedcallbackprobe)(void);



static testtypedcallback gTestTypedCallback;
static testtypedcallbackprobe gTestTypedCallbackProbe;
static bool gTestTypedCallbackTried;
static bool gTestTypedCallbackBlocked;



/* 在当前目标回调中执行一次容器探针。 */
static void testTypedCallbackRun(testtypedcallback Callback)
{
	if ( (gTestTypedCallback != Callback) || gTestTypedCallbackTried ) {
		return;
	}
	gTestTypedCallbackTried = true;
	xrtClearError();
	gTestTypedCallbackBlocked =
		(gTestTypedCallbackProbe != NULL) && gTestTypedCallbackProbe();
}



/* 初始化测试整数，并探测初始化回调重入。 */
static bool testTypedCallbackInit(ptr pValue, const xrttype* pType)
{
	(void)pType;
	testTypedCallbackRun(TEST_TYPED_CALLBACK_INIT);
	*(int*)pValue = 0;
	return true;
}



/* 复制测试整数，并探测复制回调重入。 */
static bool testTypedCallbackCopy(
	ptr pTarget,
	const void* pSource,
	const xrttype* pType
)
{
	(void)pType;
	testTypedCallbackRun(TEST_TYPED_CALLBACK_COPY);
	*(int*)pTarget = *(const int*)pSource;
	return true;
}



/* 移动测试整数，并探测移动回调重入。 */
static bool testTypedCallbackMove(
	ptr pTarget,
	ptr pSource,
	const xrttype* pType
)
{
	(void)pType;
	testTypedCallbackRun(TEST_TYPED_CALLBACK_MOVE);
	*(int*)pTarget = *(int*)pSource;
	*(int*)pSource = 0;
	return true;
}



/* 释放测试整数，并探测释放回调重入。 */
static void testTypedCallbackDrop(ptr pValue, const xrttype* pType)
{
	(void)pType;
	testTypedCallbackRun(TEST_TYPED_CALLBACK_DROP);
	*(int*)pValue = 0;
}



/* 比较测试整数，并探测比较回调重入。 */
static int testTypedCallbackCompare(
	const void* pLeft,
	const void* pRight,
	const xrttype* pType
)
{
	int iLeft = *(const int*)pLeft;
	int iRight = *(const int*)pRight;
	(void)pType;

	testTypedCallbackRun(TEST_TYPED_CALLBACK_COMPARE);
	return (iLeft > iRight) - (iLeft < iRight);
}



/* 散列测试整数，并探测散列回调重入。 */
static uint64 testTypedCallbackHash(
	const void* pValue,
	const xrttype* pType
)
{
	uint64 iValue = (uint64)(uint32)*(const int*)pValue;
	(void)pType;

	testTypedCallbackRun(TEST_TYPED_CALLBACK_HASH);
	return iValue * UINT64_C(11400714819323198485);
}



/* 追踪测试整数，并探测对象追踪回调重入。 */
static bool testTypedCallbackTrace(
	const void* pValue,
	const xrttype* pType,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	(void)pValue;
	(void)pType;
	(void)pVisit;
	(void)pContext;
	testTypedCallbackRun(TEST_TYPED_CALLBACK_TRACE);
	return true;
}



/* 构造覆盖容器生命周期、比较和散列路径的测试类型。 */
static xrttype testTypedCallbackType(void)
{
	static const xrttypeops Ops = {
		.Init = testTypedCallbackInit,
		.Copy = testTypedCallbackCopy,
		.Move = testTypedCallbackMove,
		.Drop = testTypedCallbackDrop,
		.Clone = testTypedCallbackCopy,
		.Compare = testTypedCallbackCompare,
		.Hash = testTypedCallbackHash,
		.Trace = testTypedCallbackTrace
	};
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.typed.Callback")),
		.Kind = XRT_TYPE_RECORD,
		.Flags = XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_RELOCATABLE,
		.Name = XRT_STR_INIT("Callback"),
		.AbiName = XRT_STR_INIT("tests.typed.Callback"),
		.Size = sizeof(int),
		.Align = TEST_ALIGNOF(int),
		.InstanceSize = sizeof(int),
		.InstanceAlign = TEST_ALIGNOF(int),
		.Ops = &Ops
	};

	return Type;
}



/* 对象追踪夹具不产生额外边，只验证类型回调调用路径。 */
static bool testTypedCallbackVisit(xrtobject* pObject, ptr pContext)
{
	(void)pObject;
	(void)pContext;
	return true;
}



/* 启动指定回调和容器探针。 */
static void testTypedCallbackReset(
	testtypedcallback Callback,
	testtypedcallbackprobe pProbe
)
{
	gTestTypedCallback = Callback;
	gTestTypedCallbackProbe = pProbe;
	gTestTypedCallbackTried = false;
	gTestTypedCallbackBlocked = false;
	xrtClearError();
}



/* 返回当前回调探针是否执行并被门禁拒绝。 */
static bool testTypedCallbackWasBlocked(void)
{
	return gTestTypedCallbackTried && gTestTypedCallbackBlocked;
}



/* 停止回调探针并清除借用函数。 */
static void testTypedCallbackStop(void)
{
	testTypedCallbackReset(TEST_TYPED_CALLBACK_NONE, NULL);
}



#endif
