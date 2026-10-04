#include "../test.h"
#include "typed_value_fixture.h"

static const xtypedarray* gBatchSource;
static const xtypedarray* gBatchTarget;
static size_t gBatchCallbacks;

/* 查询和销毁都必须被 BUSY 拒绝；保存原来的复制失败错误。 */
static void testTypedStackBatchCallbackProbe(void)
{
	if ( !gBatchSource ) return;
	xerror* saved = xrtTakeError();
	xrtTypedArrayUnit((xtypedarray*)gBatchSource);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "batch callback destroyed source");
	xrtClearError();
	if ( gBatchTarget ) {
		(void)xrtTypedArrayCount(gBatchTarget);
		testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "batch callback read target");
		xrtClearError();
	}
	gBatchCallbacks++;
	xrtSetErrorTake(saved);
}

static bool testTypedStackBatchCopy(ptr target, const void* source, const xrttype* type)
{
	testTypedStackBatchCallbackProbe();
	return testTypedValueCopy(target, source, type);
}

static void testTypedStackBatchDrop(ptr value, const xrttype* type)
{
	testTypedStackBatchCallbackProbe();
	testTypedValueDrop(value, type);
}



/* 验证旧版覆盖过的定宽标量不会因栈封装改变宽度。 */
static void testTypedStackWidths(void)
{
	xtypedstack Stack;
	int8 iSmall = -7;
	int8 iSmallOut = 0;
	uint16 iWide = 60000u;
	uint16 iWideOut = 0u;
	float fReal = 1.25f;
	float fRealOut = 0.0f;

	testRequire(
		xrtTypedStackInit(&Stack, xrtTypeInt8()) &&
		xrtTypedStackPush(&Stack, &iSmall) &&
		xrtTypedStackPop(&Stack, &iSmallOut) &&
		(iSmallOut == iSmall),
		"typed stack int8 width mismatch"
	);
	xrtTypedStackUnit(&Stack);
	testRequire(
		xrtTypedStackInit(&Stack, xrtTypeUInt16()) &&
		xrtTypedStackPush(&Stack, &iWide) &&
		xrtTypedStackPop(&Stack, &iWideOut) &&
		(iWideOut == iWide),
		"typed stack uint16 width mismatch"
	);
	xrtTypedStackUnit(&Stack);
	testRequire(
		xrtTypedStackInit(&Stack, xrtTypeFloat32()) &&
		xrtTypedStackPush(&Stack, &fReal) &&
		xrtTypedStackPop(&Stack, &fRealOut) &&
		(fRealOut == fReal),
		"typed stack float32 width mismatch"
	);
	xrtTypedStackUnit(&Stack);
}



/* 验证基础后进先出操作、自引用压入、克隆和丢弃式弹出。 */
static void testTypedStackOperations(void)
{
	xtypedstack Stack;
	xtypedstack* pClone;
	int64 iFirst = 11;
	int64 iSecond = 22;
	int64 iOutput = 0;
	const int64* pTop;

	testRequire(
		xrtTypedStackInit(&Stack, xrtTypeInt64()) &&
		xrtTypedStackReserve(&Stack, 2u) &&
		xrtTypedStackPush(&Stack, &iFirst) &&
		xrtTypedStackPush(&Stack, &iSecond),
		"typed stack fixture failed"
	);
	pTop = (const int64*)xrtTypedStackConstTop(&Stack);
	testRequire(
		(pTop != NULL) && (*pTop == iSecond) &&
		xrtTypedStackPush(&Stack, pTop) &&
		(xrtTypedStackCount(&Stack) == 3u) &&
		(*(const int64*)xrtTypedStackConstPeek(&Stack, 1u) == iSecond),
		"typed stack top, peek, or self push failed"
	);
	pClone = xrtTypedStackClone(&Stack);
	testRequire(
		(pClone != NULL) && xrtTypedStackEquals(&Stack, pClone),
		"typed stack clone failed"
	);
	testRequire(
		xrtTypedStackPop(&Stack, &iOutput) && (iOutput == iSecond) &&
		xrtTypedStackPop(&Stack, NULL) &&
		(xrtTypedStackCount(&Stack) == 1u) &&
		(*(const int64*)xrtTypedStackTop(&Stack) == iFirst),
		"typed stack pop failed"
	);
	testRequire(
		!xrtTypedStackEquals(&Stack, pClone) &&
		xrtTypedStackTrim(&Stack) &&
		(xrtTypedStackCapacity(&Stack) == 1u),
		"typed stack trim or equality failed"
	);
	xrtTypedStackClear(&Stack);
	testRequire(
		(xrtTypedStackCount(&Stack) == 0u) &&
		!xrtTypedStackPop(&Stack, NULL),
		"typed stack empty behavior mismatch"
	);
	xrtTypedStackDestroy(pClone);
	xrtTypedStackUnit(&Stack);
}



/* 对照逐次 Push/Pop 的所有小范围、空范围及 SIZE_MAX 上限。 */
static void testTypedStackBatches(void)
{
	for ( size_t n = 0u; n <= 8u; n++ ) {
		xtypedstack Stack;
		testRequire(xrtTypedStackInit(&Stack, xrtTypeInt64()), "batch init failed");
		for ( size_t i = 0u; i < n; i++ ) {
			int64 value = (int64)i + 11;
			testRequire(xrtTypedStackPush(&Stack, &value), "batch fixture failed");
		}
		for ( size_t depth = 0u; depth <= n; depth++ ) {
			for ( size_t limit = 0u; limit <= n + 2u; limit++ ) {
				xtypedarray* result = xrtTypedStackPeekBatch(&Stack, depth, limit);
				size_t count = limit < n - depth ? limit : n - depth;
				testRequire(result && xrtTypedArrayCount(result) == count &&
					xrtTypedStackCount(&Stack) == n, "peek batch count changed source");
				for ( size_t i = 0u; i < count; i++ ) {
					testRequire(*(const int64*)xrtTypedArrayConstGet(result, i) ==
						(int64)(n - depth - i - 1u) + 11, "peek batch order mismatch");
				}
				xrtTypedArrayDestroy(result);
			}
		}
		xrtClearError();
		testRequire(!xrtTypedStackPeekBatch(&Stack, SIZE_MAX, SIZE_MAX) &&
			xrtErrorKind(xrtGetError()) == XERR_RANGE, "batch depth overflow accepted");
		xrtClearError();
		xtypedarray* result = xrtTypedStackPopBatch(&Stack, SIZE_MAX);
		testRequire(result && xrtTypedArrayCount(result) == n &&
			xrtTypedStackCount(&Stack) == 0u, "pop batch count mismatch");
		for ( size_t i = 0u; i < n; i++ ) {
			testRequire(*(const int64*)xrtTypedArrayConstGet(result, i) ==
				(int64)(n - i - 1u) + 11, "pop batch order mismatch");
		}
		testRequire(xrtTypedStackPushBatch(&Stack, result) &&
			xrtTypedStackPushBatch(&Stack, &Stack) &&
			xrtTypedStackCount(&Stack) == n * 2u, "batch/self push failed");
		xrtTypedArrayDestroy(result);
		xrtTypedStackUnit(&Stack);
	}

	xtypedarray Array;
	testRequire(xrtTypedArrayInit(&Array, xrtTypeUInt8()), "slice init failed");
	for ( uint8 i = 0u; i < 6u; i++ ) {
		testRequire(xrtTypedArrayPush(&Array, &i), "slice fixture failed");
	}
	for ( size_t first = 0u; first <= 6u; first++ ) {
		for ( size_t count = 0u; count <= 6u - first; count++ ) {
			for ( unsigned int reverse = 0u; reverse < 2u; reverse++ ) {
				xtypedarray* slice = xrtTypedArraySlice(&Array, first, count, reverse != 0u);
				testRequire(slice && xrtTypedArrayCount(slice) == count, "slice count mismatch");
				for ( size_t i = 0u; i < count; i++ ) {
					testRequire(*(const uint8*)xrtTypedArrayConstGet(slice, i) ==
						first + (reverse ? count - i - 1u : i), "slice order mismatch");
				}
				xrtTypedArrayDestroy(slice);
			}
		}
	}
	testRequire(!xrtTypedArraySlice(&Array, 1u, SIZE_MAX, false) &&
		xrtErrorKind(xrtGetError()) == XERR_RANGE, "slice length overflow accepted");
	xrtClearError();
	xtypedarray* tail = xrtTypedArrayTakeTail(&Array, 2u, false);
	testRequire(tail && xrtTypedArrayCount(&Array) == 4u &&
		*(const uint8*)xrtTypedArrayConstGet(tail, 0u) == 4u &&
		*(const uint8*)xrtTypedArrayConstGet(tail, 1u) == 5u, "forward tail mismatch");
	xrtTypedArrayDestroy(tail);
	xrtTypedArrayUnit(&Array);
	xtypedstack wrong;
	testRequire(xrtTypedStackInit(&wrong, xrtTypeUInt64()) &&
		xrtTypedArrayInit(&Array, xrtTypeInt64()), "batch distinct identity init");
	xrtClearError();
	testRequire(!xrtTypedStackPushBatch(&wrong, &Array) &&
		xrtErrorKind(xrtGetError()) == XERR_TYPE, "batch accepted equal-layout different type");
	xrtClearError(); xrtTypedStackUnit(&wrong); xrtTypedArrayUnit(&Array);
}



/* 逐项复制失败不发布部分结果；PopBatch 必须移交而不是复制。 */
static void testTypedStackBatchOwnership(void)
{
	xrttype Type = testTypedValueType();
	xtypedstack Stack, Source;
	int owners = 1;
	testtypedvalue item = {7, &owners};
	testRequire(xrtTypedStackInit(&Stack, &Type) &&
		xrtTypedStackInit(&Source, &Type) && xrtTypedStackPush(&Stack, &item), "owned batch init");
	for ( int i = 0; i < 3; i++ ) {
		item.Value = i + 10;
		testRequire(xrtTypedStackPush(&Source, &item), "owned batch source");
	}
	for ( size_t i = 0u; i < 3u; i++ ) {
		testTypedValueCopyFailAfter(i);
		xrtClearError();
		testRequire(!xrtTypedStackPushBatch(&Stack, &Source) &&
			xrtErrorKind(xrtGetError()) == XERR_VALUE &&
			xrtTypedStackCount(&Stack) == 1u && xrtTypedStackCount(&Source) == 3u &&
			owners == 5, "batch push partial failure leaked or published");
		testTypedValueCopyFailAfter(i);
		xrtClearError();
		testRequire(!xrtTypedStackPeekBatch(&Source, 0u, 3u) &&
			xrtErrorKind(xrtGetError()) == XERR_VALUE && owners == 5 &&
			xrtTypedStackCount(&Source) == 3u, "batch peek failure leaked or changed source");
		testTypedValueCopyFailAfter(i); xrtClearError();
		testRequire(!xrtTypedStackPushBatch(&Source, &Source) &&
			xrtErrorKind(xrtGetError()) == XERR_VALUE && owners == 5 &&
			xrtTypedStackCount(&Source) == 3u &&
			((const testtypedvalue*)xrtTypedStackConstTop(&Source))->Value == 12,
			"self batch failure changed original source");
	}
	testTypedValueCopyFail(true);
	xrtClearError();
	xtypedarray* result = xrtTypedStackPopBatch(&Source, 2u);
	testRequire(result && !xrtGetError() && owners == 5 &&
		xrtTypedStackCount(&Source) == 1u &&
		((const testtypedvalue*)xrtTypedArrayConstGet(result, 0u))->Value == 12 &&
		((const testtypedvalue*)xrtTypedArrayConstGet(result, 1u))->Value == 11,
		"pop batch copied, lost ownership or reversed incorrectly");
	xrtTypedArrayDestroy(result);
	testRequire(owners == 3, "pop batch result did not drop exactly two owners");
	testTypedValueCopyFail(false);
	xrtTypedStackUnit(&Source);
	xrtTypedStackUnit(&Stack);
	xrtTypeDropValue(&Type, &item);
	testRequire(owners == 0, "batch final ownership imbalance");

	/* Element layout remains native and over-aligned, never a boxed vector. */
	xrttype Aligned = *xrtTypeUInt8();
	Aligned.Size = 64u; Aligned.Align = 64u;
	Aligned.Kind = XRT_TYPE_RECORD;
	Aligned.InstanceSize = 64u; Aligned.InstanceAlign = 64u;
	Aligned.Id = xrtTypeId(XRT_STR_LITERAL("tests.batch.aligned"));
	Aligned.AbiName = XRT_STR_LITERAL("tests.batch.aligned");
	Aligned.Ops = NULL;
	_Alignas(64) unsigned char block[64] = {17};
	testRequire(xrtTypedStackInit(&Stack, &Aligned) && xrtTypedStackPush(&Stack, block),
		"aligned batch init");
	result = xrtTypedStackPopBatch(&Stack, SIZE_MAX);
	testRequire(result && (uintptr_t)xrtTypedArrayConstData(result) % 64u == 0u &&
		memcmp(xrtTypedArrayConstData(result), block, sizeof(block)) == 0, "aligned batch layout");
	xrtTypedArrayDestroy(result);
	xrtTypedStackUnit(&Stack);

#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE_STRING)
	xstrview text = XRT_STR_INIT("a\0b");
	testRequire(xrtTypedStackInit(&Stack, xrtTypeStringView()) &&
		xrtTypedStackPush(&Stack, &text), "string batch init");
	result = xrtTypedStackPopBatch(&Stack, SIZE_MAX);
	const xstrview* out = result ? xrtTypedArrayConstGet(result, 0u) : NULL;
	testRequire(out && out->Size == 3u && memcmp(out->Data, text.Data, 3u) == 0,
		"string batch truncated embedded NUL");
	xrtTypedStackUnit(&Stack);
	testRequire(out->Size == 3u && out->Data[2] == 'b', "batch result borrowed dead source");
	xrtTypedArrayDestroy(result);
#endif
}



/* Copy 与失败回滚 Drop 都保护完整来源/目标，而不是只保护正在写的槽。 */
static void testTypedStackBatchCallbacks(void)
{
	xrttype Type = testTypedValueType();
	xrttypeops Ops = *Type.Ops;
	Ops.Copy = testTypedStackBatchCopy; Ops.Drop = testTypedStackBatchDrop;
	Type.Ops = &Ops;
	xtypedstack source, target;
	int owners = 1;
	testtypedvalue item = {9, &owners};
	testRequire(xrtTypedStackInit(&source, &Type) && xrtTypedStackInit(&target, &Type),
		"callback batch init");
	for ( unsigned int i = 0u; i < 3u; i++ )
		testRequire(xrtTypedStackPush(&source, &item), "callback batch fixture");
	gBatchSource = &source; gBatchTarget = &target; gBatchCallbacks = 0u;
	testTypedValueCopyFailAfter(1u);
	xrtClearError();
	testRequire(!xrtTypedStackPushBatch(&target, &source) &&
		xrtErrorKind(xrtGetError()) == XERR_VALUE && gBatchCallbacks >= 3u && owners == 4,
		"batch copy/rollback callback boundary failed");
	gBatchTarget = NULL; gBatchCallbacks = 0u;
	testTypedValueCopyFailAfter(1u); xrtClearError();
	testRequire(!xrtTypedStackPeekBatch(&source, 0u, SIZE_MAX) &&
		xrtErrorKind(xrtGetError()) == XERR_VALUE && gBatchCallbacks >= 3u && owners == 4,
		"batch peek copy/cleanup callback boundary failed");
	gBatchSource = NULL; testTypedValueCopyFail(false); xrtClearError();
	xrtTypedStackUnit(&source); xrtTypedStackUnit(&target);
	xrtTypeDropValue(&Type, &item);
	testRequire(owners == 0, "batch callback fixture ownership imbalance");
}



/* 运行类型栈常规与旧资产回归测试。 */
int main(void)
{
	testTypedStackWidths();
	testTypedStackOperations();
	testTypedStackBatches();
	testTypedStackBatchOwnership();
	testTypedStackBatchCallbacks();
	xrtClearError();
	printf("[PASS] typed stack\n");
	return 0;
}
