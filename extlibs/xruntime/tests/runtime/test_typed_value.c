#include "../test.h"

#include <limits.h>



/* 比较自定义记录中的整数。 */
static int testTypedValueCompare(
	const void* pLeft,
	const void* pRight,
	const xrttype* pType
)
{
	int iLeft;
	int iRight;
	(void)pType;

	memcpy(&iLeft, pLeft, sizeof(iLeft));
	memcpy(&iRight, pRight, sizeof(iRight));
	return (iLeft > iRight) - (iLeft < iRight);
}



/* 散列自定义记录中的整数。 */
static uint64 testTypedValueHash(
	const void* pValue,
	const xrttype* pType
)
{
	int iValue;
	(void)pType;

	memcpy(&iValue, pValue, sizeof(iValue));
	return (uint64)(uint32)iValue;
}



/* 返回用于验证用户转换器扩展路径的记录类型。 */
static xrttype testTypedValueType(void)
{
	static const xrttypeops Ops = {
		.Compare = testTypedValueCompare,
		.Hash = testTypedValueHash
	};
	xrttype Type = {
		.Id = 0,
		.Kind = XRT_TYPE_RECORD,
		.Flags = XRT_TYPE_FLAG_TRIVIAL_COPY | XRT_TYPE_FLAG_TRIVIAL_DROP |
			XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_RELOCATABLE |
			XRT_TYPE_FLAG_FINAL,
		.Name = XRT_STR_INIT("ConvertedInt"),
		.AbiName = XRT_STR_INIT("tests.ConvertedInt"),
		.Size = sizeof(int),
		.Align = TEST_ALIGNOF(int),
		.InstanceSize = sizeof(int),
		.InstanceAlign = TEST_ALIGNOF(int),
		.Ops = &Ops
	};

	Type.Id = xrtTypeId(Type.AbiName);
	return Type;
}



/* 把动态整数解码为自定义记录。 */
static bool testTypedValueToTyped(
	const xvalue* pSource,
	const xrttype* pTargetType,
	ptr pTarget,
	ptr pContext
)
{
	int64 iValue;
	int iResult;
	(void)pTargetType;
	(void)pContext;

	if ( !xrtValueGetInt(pSource, &iValue) ||
		 (iValue < INT_MIN) || (iValue > INT_MAX) ) {
		return false;
	}
	iResult = (int)iValue;
	memcpy(pTarget, &iResult, sizeof(iResult));
	return true;
}



/* 把自定义记录编码为动态整数。 */
static xvalue* testTypedValueFromTyped(
	const xrttype* pSourceType,
	const void* pSource,
	ptr pContext
)
{
	int iValue;
	(void)pSourceType;
	(void)pContext;

	memcpy(&iValue, pSource, sizeof(iValue));
	return xrtValueInt((int64)iValue);
}



/* 验证一个内建标量经过动态 Value 后保持等值。 */
static void testTypedValueRoundTrip(
	const xrttype* pType,
	const void* pSource
)
{
	union {
		long double Float;
		ptr Pointer;
		uint64 Integer;
		uint8 Data[16];
	} Target;
	xvalue* pValue;
	int iCompare = 1;

	memset(&Target, 0, sizeof(Target));
	pValue = xrtValueFromTyped(pType, pSource, NULL);
	testRequire(pValue != NULL, "typed scalar encoding failed");
	testRequire(
		xrtValueToTyped(pValue, pType, Target.Data, NULL),
		"typed scalar decoding failed"
	);
	testRequire(
		xrtTypeCompareValue(pType, pSource, Target.Data, &iCompare) &&
		(iCompare == 0),
		"typed scalar round trip changed the value"
	);
	xrtValueRelease(pValue);
}



#if defined(XRUNTIME_FEATURE_TYPED_ARRAY_VALUE) || \
	defined(XRUNTIME_FEATURE_TYPED_LIST_VALUE) || \
	defined(XRUNTIME_FEATURE_TYPED_SET_VALUE) || \
	defined(XRUNTIME_FEATURE_TYPED_DICT_VALUE)

/* 容器重入测试通过统一回调尝试修改当前来源容器。 */
typedef bool (*testtypedvaluemutate)(ptr pContainer, const void* pItem);



/* 容器重入测试状态保存当前来源和修改入口。 */
typedef struct testtypedvaluereentry {
	ptr Container;
	testtypedvaluemutate Mutate;
	bool Attempted;
} testtypedvaluereentry;



/* 动态来源快照测试通过统一入口修改正在转换的源容器。 */
typedef bool (*testtypedvaluesourcemutate)(xvalue* pSource);



/* 动态来源快照测试记录来源、修改入口和转换次数。 */
typedef struct testtypedvaluesnapshot {
	xvalue* Source;
	testtypedvaluesourcemutate Mutate;
	size_t Calls;
} testtypedvaluesnapshot;



/* 失败清理测试记录转换和销毁过程。 */
typedef struct testtypedvaluecleanup {
	size_t DecodeCount;
	size_t DropCount;
	bool PoisonDrop;
} testtypedvaluecleanup;



/* 在编码类型元素期间尝试重入来源容器。 */
static xvalue* testTypedValueReenter(
	const xrttype* pSourceType,
	const void* pSource,
	ptr pContext
)
{
	testtypedvaluereentry* pState = (testtypedvaluereentry*)pContext;
	(void)pSourceType;

	pState->Attempted = true;
	(void)pState->Mutate(pState->Container, pSource);
	return NULL;
}



/* 第一次解码时修改动态来源，并继续解码当前快照。 */
static bool testTypedValueSnapshotDecode(
	const xvalue* pSource,
	const xrttype* pTargetType,
	ptr pTarget,
	ptr pContext
)
{
	testtypedvaluesnapshot* pState = (testtypedvaluesnapshot*)pContext;

	pState->Calls++;
	if ( (pState->Calls == 1u) && !pState->Mutate(pState->Source) ) {
		return false;
	}
	return testTypedValueToTyped(pSource, pTargetType, pTarget, NULL);
}



/* 设置一个可在包装链中精确识别的测试错误。 */
static void testTypedValueSetError(
	xerrkind Kind,
	cstr sDomain,
	int32 iCode,
	cstr sOperation
)
{
	xerror* pError = xrtErrorCreate(Kind, sDomain, iCode, sOperation);

	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 在失败回收阶段注入次生错误，验证调用方保留原始失败。 */
static void testTypedValueCleanupDrop(ptr pValue, const xrttype* pType)
{
	testtypedvaluecleanup* pState =
		(testtypedvaluecleanup*)pType->Metadata;
	(void)pValue;

	pState->DropCount++;
	if ( pState->PoisonDrop ) {
		testTypedValueSetError(
			XERR_STATE, "tests.typed-value-cleanup", 902, "drop"
		);
	}
}



/* 返回带可观测销毁操作的失败清理测试类型。 */
static xrttype testTypedValueCleanupType(testtypedvaluecleanup* pState)
{
	static const xrttypeops Ops = {
		.Drop = testTypedValueCleanupDrop,
		.Compare = testTypedValueCompare,
		.Hash = testTypedValueHash
	};
	xrttype Type = {
		.Id = 0,
		.Kind = XRT_TYPE_RECORD,
		.Flags = XRT_TYPE_FLAG_TRIVIAL_COPY | XRT_TYPE_FLAG_COPYABLE |
			XRT_TYPE_FLAG_RELOCATABLE | XRT_TYPE_FLAG_FINAL,
		.Name = XRT_STR_INIT("CleanupInt"),
		.AbiName = XRT_STR_INIT("tests.CleanupInt"),
		.Size = sizeof(int),
		.Align = TEST_ALIGNOF(int),
		.InstanceSize = sizeof(int),
		.InstanceAlign = TEST_ALIGNOF(int),
		.Ops = &Ops,
		.Metadata = pState
	};

	Type.Id = xrtTypeId(Type.AbiName);
	return Type;
}



/* 第二个元素返回主错误，使后续销毁能够检验错误保留。 */
static bool testTypedValueCleanupDecode(
	const xvalue* pSource,
	const xrttype* pTargetType,
	ptr pTarget,
	ptr pContext
)
{
	testtypedvaluecleanup* pState = (testtypedvaluecleanup*)pContext;

	pState->DecodeCount++;
	if ( pState->DecodeCount == 2u ) {
		pState->PoisonDrop = true;
		testTypedValueSetError(
			XERR_MEMORY, "tests.typed-value-root", 901, "decode"
		);
		return false;
	}
	return testTypedValueToTyped(pSource, pTargetType, pTarget, NULL);
}



/* 检查失败清理后的错误链仍包含最初转换错误。 */
static bool testTypedValueCleanupErrorHeld(void)
{
	const xerror* pError = xrtGetError();

	return (pError != NULL) && (xrtErrorKind(pError) == XERR_MEMORY) &&
		(xrtErrorFind(pError, "tests.typed-value-root", 901) != NULL);
}

#endif



/* 验证内建安全标量和自定义转换器。 */
static void testTypedValueScalars(void)
{
	xrttype Type = testTypedValueType();
	xvalueconverter Converter = {
		NULL,
		testTypedValueToTyped,
		testTypedValueFromTyped
	};
	xvalue* pSource = xrtValueInt(127);
	xvalue* pResult;
	int8 iSmall = 0;
	int32 iBool32 = 0;
	int32 iBool32Source = -9;
	bool bOutput = false;
	int iCustom = 0;
	int64 iOutput = 0;
	bool bBool = true;
	int8 iInt8 = INT8_MIN;
	uint8 iUInt8 = UINT8_MAX;
	int16 iInt16 = INT16_MIN;
	uint16 iUInt16 = UINT16_MAX;
	int32 iInt32 = INT32_MIN;
	uint32 iUInt32 = UINT32_MAX;
	int64 iInt64 = INT64_MIN;
	uint64 iUInt64 = (uint64)INT64_MAX;
	float fFloat32 = 1.25f;
	double fFloat64 = -1.5;
	xtime Time = (xtime)123456789;
	ptr pPointer = (ptr)(uintptr_t)UINT32_C(0x1234);
	uint64 iType = UINT64_C(42);
	uint64 iTooLarge = UINT64_MAX;

	testRequire(pSource != NULL, "typed Value scalar fixture failed");
	testRequire(
		xrtValueToTyped(pSource, xrtTypeInt8(), &iSmall, NULL) &&
		(iSmall == 127),
		"dynamic integer to int8 conversion failed"
	);
	xrtValueRelease(pSource);
	pSource = xrtValueBool(true);
	testRequire(
		xrtValueToTyped(pSource, xrtTypeBool32(), &iBool32, NULL) &&
		(iBool32 == 1),
		"dynamic boolean to bool32 conversion failed"
	);
	pResult = xrtValueFromTyped(xrtTypeBool32(), &iBool32Source, NULL);
	testRequire(
		(pResult != NULL) && xrtValueGetBool(pResult, &bOutput) && bOutput,
		"bool32 to dynamic boolean conversion failed"
	);
	xrtValueRelease(pResult);
	xrtValueRelease(pSource);
	pSource = xrtValueInt(128);
	xrtClearError();
	testRequire(
		!xrtValueToTyped(pSource, xrtTypeInt8(), &iSmall, NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		(xrtErrorCode(xrtGetError()) == XTYPED_VALUE_ERROR_RANGE),
		"typed Value integer range rejection mismatch"
	);
	xrtValueRelease(pSource);
	pSource = xrtValueInt(41);
	testRequire(
		xrtValueToTyped(pSource, &Type, &iCustom, &Converter) &&
		(iCustom == 41),
		"custom dynamic value decoder failed"
	);
	pResult = xrtValueFromTyped(&Type, &iCustom, &Converter);
	testRequire(
		(pResult != NULL) && xrtValueGetInt(pResult, &iOutput) &&
		(iOutput == 41),
		"custom dynamic value encoder failed"
	);
	xrtValueRelease(pResult);
	xrtValueRelease(pSource);

	testTypedValueRoundTrip(xrtTypeBool(), &bBool);
	testTypedValueRoundTrip(xrtTypeBool32(), &iBool32Source);
	testTypedValueRoundTrip(xrtTypeInt8(), &iInt8);
	testTypedValueRoundTrip(xrtTypeUInt8(), &iUInt8);
	testTypedValueRoundTrip(xrtTypeInt16(), &iInt16);
	testTypedValueRoundTrip(xrtTypeUInt16(), &iUInt16);
	testTypedValueRoundTrip(xrtTypeInt32(), &iInt32);
	testTypedValueRoundTrip(xrtTypeUInt32(), &iUInt32);
	testTypedValueRoundTrip(xrtTypeInt64(), &iInt64);
	testTypedValueRoundTrip(xrtTypeUInt64(), &iUInt64);
	testTypedValueRoundTrip(xrtTypeFloat32(), &fFloat32);
	testTypedValueRoundTrip(xrtTypeFloat64(), &fFloat64);
	testTypedValueRoundTrip(xrtTypeTime(), &Time);
	testTypedValueRoundTrip(xrtTypePointer(), &pPointer);
	testTypedValueRoundTrip(xrtTypeType(), &iType);

	xrtClearError();
	pResult = xrtValueFromTyped(xrtTypeUInt64(), &iTooLarge, NULL);
	testRequire(
		(pResult == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		(xrtErrorCode(xrtGetError()) == XTYPED_VALUE_ERROR_RANGE),
		"uint64 outside the dynamic integer domain was accepted"
	);
}



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_CALLABLE)

/* callable 测试入口不产生返回值，只用于验证类型槽持有完整调用对象。 */
static bool testTypedValueCallableEntry(
	ptr pEnvironment,
	const xrtcallframe* pFrame,
	xrtcallresult* pResult
)
{
	(void)pEnvironment;
	(void)pFrame;
	(void)pResult;
	return true;
}



/* 记录最后一个 callable 强引用释放环境的时机。 */
static void testTypedValueCallableDrop(ptr pEnvironment)
{
	int* pDropCount = (int*)pEnvironment;

	(*pDropCount)++;
}



/* 验证 callable 在动态值和运行时类型槽之间往返时保持身份与所有权。 */
static void testTypedValueCallable(void)
{
	const xrttype* pType = xrtTypeCallable();
	xrtcallable* pCallable;
	xrtcallable* pTyped = NULL;
	xrtcallable* pNullTyped = (xrtcallable*)(uintptr_t)1u;
	xvalue* pValue;
	xvalue* pResult;
	xvalue* pNullResult;
	int iDropCount = 0;

	pCallable = xrtCallableCreate(
		NULL, testTypedValueCallableEntry, &iDropCount,
		testTypedValueCallableDrop
	);
	pValue = xrtValueCallable(pCallable);
	testRequire(
		(pCallable != NULL) && (pValue != NULL) &&
		xrtValueToTyped(pValue, pType, &pTyped, NULL) &&
		(pTyped == pCallable),
		"callable Value to typed slot conversion failed"
	);
	pResult = xrtValueFromTyped(pType, &pTyped, NULL);
	testRequire(
		(pResult != NULL) && xrtValueIsCallable(pResult) &&
		(xrtValueGetCallable(pResult) == pCallable),
		"typed callable slot to Value conversion failed"
	);
	xrtCallableUnref(pCallable);
	xrtValueRelease(pValue);
	xrtValueRelease(pResult);
	testRequire(iDropCount == 0,
		"typed callable slot did not preserve the callable lifetime");
	xrtTypeDropValue(pType, &pTyped);
	testRequire((pTyped == NULL) && (iDropCount == 1),
		"typed callable slot final drop mismatch");

	testRequire(
		xrtValueToTyped(xrtValueNull(), pType, &pNullTyped, NULL) &&
		(pNullTyped == NULL),
		"null Value to callable slot conversion failed"
	);
	pNullResult = xrtValueFromTyped(pType, &pNullTyped, NULL);
	testRequire(
		(pNullResult != NULL) &&
		(xrtValueType(pNullResult) == XVALUE_NULL),
		"null callable slot to Value conversion failed"
	);
	xrtValueRelease(pNullResult);
}

#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_FUTURE)

/* 验证 Future 在动态值和运行时类型槽之间往返时保持消费端生命周期。 */
static void testTypedValueFuture(void)
{
	const xrttype* pType = xrtTypeFuture();
	xfuture* pFuture = NULL;
	xfuture* pTyped = NULL;
	xfuture* pNullTyped = (xfuture*)(uintptr_t)1u;
	xpromise* pPromise = xrtPromiseCreate(&pFuture, NULL);
	xvalue* pValue;
	xvalue* pResult;
	xvalue* pNullResult;
	int iAnswer = 42;

	pValue = xrtValueFuture(pFuture);
	testRequire(
		(pPromise != NULL) && (pFuture != NULL) && (pValue != NULL) &&
		xrtValueToTyped(pValue, pType, &pTyped, NULL) &&
		(pTyped == pFuture),
		"Future Value to typed slot conversion failed"
	);
	pResult = xrtValueFromTyped(pType, &pTyped, NULL);
	testRequire(
		(pResult != NULL) && xrtValueIsFuture(pResult) &&
		(xrtValueGetFuture(pResult) == pFuture),
		"typed Future slot to Value conversion failed"
	);
	xrtFutureDestroy(pFuture);
	xrtValueRelease(pValue);
	xrtValueRelease(pResult);
	testRequire(
		xrtPromiseResolve(pPromise, &iAnswer) &&
		(xrtFutureState(pTyped) == XFUTURE_RESOLVED) &&
		(xrtFutureValue(pTyped) == &iAnswer),
		"typed Future slot did not preserve the consumer lifetime"
	);
	xrtTypeDropValue(pType, &pTyped);
	testRequire(pTyped == NULL, "typed Future slot final drop mismatch");
	xrtPromiseDestroy(pPromise);

	testRequire(
		xrtValueToTyped(xrtValueNull(), pType, &pNullTyped, NULL) &&
		(pNullTyped == NULL),
		"null Value to Future slot conversion failed"
	);
	pNullResult = xrtValueFromTyped(pType, &pNullTyped, NULL);
	testRequire(
		(pNullResult != NULL) &&
		(xrtValueType(pNullResult) == XVALUE_NULL),
		"null Future slot to Value conversion failed"
	);
	xrtValueRelease(pNullResult);
}

#endif



#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE)

/* 验证运行时 Value 类型槽使用深拷贝而不是共享可变 backing。 */
static void testTypedRuntimeValue(void)
{
	const xrttype* pType = xrtTypeValue();
	xvalue* pSource = xrtValueArray();
	xvalue* pOwned = NULL;
	xvalue* pResult;
	int64 iOwned = 0;
	int64 iResult = 0;

	testRequire(
		(pSource != NULL) &&
		xrtValueArrayAppendNew(pSource, xrtValueInt(5)) &&
		xrtValueToTyped(pSource, pType, &pOwned, NULL),
		"runtime Value typed slot conversion failed"
	);
	testRequire(
		xrtValueArraySetNew(pSource, 0u, xrtValueInt(9)) &&
		xrtValueGetInt(xrtValueArrayGet(pOwned, 0u), &iOwned) &&
		(iOwned == 5),
		"runtime Value typed slot shared the source backing"
	);
	pResult = xrtValueFromTyped(pType, &pOwned, NULL);
	testRequire(
		(pResult != NULL) && (pResult != pOwned) &&
		xrtValueGetInt(xrtValueArrayGet(pResult, 0u), &iResult) &&
		(iResult == 5),
		"runtime Value typed slot did not produce an independent graph"
	);
	xrtValueRelease(pResult);
	xrtTypeDropValue(pType, &pOwned);
	xrtValueRelease(pSource);

#if defined(XRUNTIME_FEATURE_TYPED_ARRAY_VALUE)
	{
		xvalue* pItems = xrtValueArray();
		xvalue* pNested = xrtValueArray();
		xtypedarray* pTyped;
		xvalue* pRoundTrip;
		xvalue* pStored;
		int64 iStored = 0;

		testRequire(
			(pItems != NULL) && (pNested != NULL) &&
			xrtValueArrayAppendNew(pNested, xrtValueInt(21)) &&
			xrtValueArrayAppendNew(pItems, pNested),
			"runtime Value typed array fixture failed"
		);
		pTyped = xrtTypedArrayFromValue(pItems, pType, NULL);
		testRequire(
			(pTyped != NULL) && (xrtTypedArrayCount(pTyped) == 1u),
			"runtime Value typed array conversion failed"
		);
		memcpy(&pStored, xrtTypedArrayConstGet(pTyped, 0u), sizeof(pStored));
		testRequire(
			xrtValueArraySetNew(
				xrtValueArrayGet(pItems, 0u), 0u, xrtValueInt(34)
			) && xrtValueGetInt(
				xrtValueArrayGet(pStored, 0u), &iStored
			) && (iStored == 21),
			"runtime Value typed array shared a nested source backing"
		);
		pRoundTrip = xrtTypedArrayToValue(pTyped, NULL);
		testRequire(
			(pRoundTrip != NULL) &&
			xrtValueGetInt(
				xrtValueArrayGet(xrtValueArrayGet(pRoundTrip, 0u), 0u),
				&iStored
			) && (iStored == 21),
			"runtime Value typed array round trip failed"
		);
		xrtValueRelease(pRoundTrip);
		xrtTypedArrayDestroy(pTyped);
		xrtValueRelease(pItems);
	}
#endif
}

#endif



#if defined(XRUNTIME_FEATURE_TYPED_ARRAY_VALUE)

/* 在数组解码期间追加一个不属于当前快照的新元素。 */
static bool testTypedArrayValueMutateSource(xvalue* pSource)
{
	return xrtValueArrayAppendNew(pSource, xrtValueInt(97));
}



/* 验证数组转换读取开始时的稳定动态快照。 */
static void testTypedArrayValueSnapshot(void)
{
	xvalue* pSource = xrtValueArray();
	testtypedvaluesnapshot State = {
		pSource,
		testTypedArrayValueMutateSource,
		0u
	};
	xvalueconverter Converter = {
		&State,
		testTypedValueSnapshotDecode,
		NULL
	};
	xrttype Type = testTypedValueType();
	xtypedarray* pArray;

	testRequire(
		(pSource != NULL) &&
		xrtValueArrayAppendNew(pSource, xrtValueInt(7)) &&
		xrtValueArrayAppendNew(pSource, xrtValueInt(11)),
		"typed array snapshot fixture failed"
	);
	pArray = xrtTypedArrayFromValue(pSource, &Type, &Converter);
	testRequire(
		(pArray != NULL) && (State.Calls == 2u) &&
		(xrtTypedArrayCount(pArray) == 2u) &&
		(xrtValueCount(pSource) == 3u),
		"typed array conversion did not hold a source snapshot"
	);
	xrtTypedArrayDestroy(pArray);
	xrtValueRelease(pSource);
}



/* 验证数组失败回收不会被元素销毁错误覆盖。 */
static void testTypedArrayValueCleanup(void)
{
	testtypedvaluecleanup State = { 0u, 0u, false };
	xvalueconverter Converter = {
		&State,
		testTypedValueCleanupDecode,
		NULL
	};
	xrttype Type = testTypedValueCleanupType(&State);
	xvalue* pSource = xrtValueArray();

	testRequire(
		(pSource != NULL) &&
		xrtValueArrayAppendNew(pSource, xrtValueInt(3)) &&
		xrtValueArrayAppendNew(pSource, xrtValueInt(5)),
		"typed array cleanup fixture failed"
	);
	xrtClearError();
	testRequire(
		(xrtTypedArrayFromValue(pSource, &Type, &Converter) == NULL) &&
		(State.DecodeCount == 2u) && (State.DropCount >= 3u) &&
		testTypedValueCleanupErrorHeld(),
		"typed array cleanup replaced the root conversion error"
	);
	xrtClearError();
	xrtValueRelease(pSource);
}



/* 尝试在类型数组编码回调中追加元素。 */
static bool testTypedArrayValueMutate(ptr pContainer, const void* pItem)
{
	return xrtTypedArrayPush((xtypedarray*)pContainer, pItem);
}



/* 验证类型数组桥接拒绝转换器重入。 */
static void testTypedArrayValueReentry(void)
{
	xrttype Type = testTypedValueType();
	xtypedarray* pArray = xrtTypedArrayCreate(&Type);
	testtypedvaluereentry State = {
		pArray,
		testTypedArrayValueMutate,
		false
	};
	xvalueconverter Converter = {
		&State,
		NULL,
		testTypedValueReenter
	};
	xvalue* pResult;
	int iValue = 3;

	testRequire(
		(pArray != NULL) && xrtTypedArrayPush(pArray, &iValue),
		"typed array reentry fixture failed"
	);
	xrtClearError();
	pResult = xrtTypedArrayToValue(pArray, &Converter);
	testRequire(
		(pResult == NULL) && State.Attempted &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		(xrtTypedArrayCount(pArray) == 1u),
		"typed array Value converter reentry was not rejected"
	);
	xrtTypedArrayDestroy(pArray);
}

/* 验证类型数组的单元素动态值便利操作。 */
static void testTypedArrayValueItems(void)
{
	xtypedarray* pArray = xrtTypedArrayCreate(xrtTypeInt32());
	xvalue* pOne = xrtValueInt(1);
	xvalue* pTwo = xrtValueInt(2);
	xvalue* pThree = xrtValueInt(3);
	xvalue* pFour = xrtValueInt(4);
	xvalue* pResult;
	int64 iValue = 0;

	testRequire(
		(pArray != NULL) && (pOne != NULL) && (pTwo != NULL) &&
		(pThree != NULL) && (pFour != NULL) &&
		xrtTypedArrayPushValue(pArray, pOne, NULL) &&
		xrtTypedArrayPushValue(pArray, pThree, NULL) &&
		xrtTypedArrayInsertValue(pArray, 1u, pTwo, NULL) &&
		xrtTypedArraySetValue(pArray, 0u, pFour, NULL),
		"typed array single Value mutation failed"
	);
	pResult = xrtTypedArrayGetValue(pArray, 1u, NULL);
	testRequire(
		(pResult != NULL) && xrtValueGetInt(pResult, &iValue) &&
		(iValue == 2) &&
		(xrtTypedArrayFindValue(pArray, pTwo, NULL) == 1u) &&
		xrtTypedArrayContainsValue(pArray, pThree, NULL),
		"typed array single Value query failed"
	);
	xrtValueRelease(pResult);
	pResult = xrtTypedArrayTakeValue(pArray, 1u, NULL);
	testRequire(
		(pResult != NULL) && xrtValueGetInt(pResult, &iValue) &&
		(iValue == 2) && (xrtTypedArrayCount(pArray) == 2u),
		"typed array single Value take failed"
	);
	xrtValueRelease(pResult);
	pResult = xrtTypedArrayPopValue(pArray, NULL);
	testRequire(
		(pResult != NULL) && xrtValueGetInt(pResult, &iValue) &&
		(iValue == 3) && (xrtTypedArrayCount(pArray) == 1u),
		"typed array single Value pop failed"
	);
	xrtValueRelease(pResult);
	xrtValueRelease(pFour);
	xrtValueRelease(pThree);
	xrtValueRelease(pTwo);
	xrtValueRelease(pOne);
	xrtTypedArrayDestroy(pArray);
}



/* 验证稠密动态数组与类型数组往返。 */
static void testTypedArrayValue(void)
{
	xvalue* pSource = xrtValueArray();
	xtypedarray* pArray;
	xvalue* pResult;
	int64 iValue = 0;

	testRequire(
		(pSource != NULL) &&
		xrtValueArrayAppendNew(pSource, xrtValueInt(7)) &&
		xrtValueArrayAppendNew(pSource, xrtValueInt(11)),
		"typed array Value fixture failed"
	);
	pArray = xrtTypedArrayFromValue(pSource, xrtTypeInt16(), NULL);
	testRequire(
		(pArray != NULL) && (xrtTypedArrayCount(pArray) == 2u) &&
		(*(const int16*)xrtTypedArrayConstGet(pArray, 1u) == 11),
		"dynamic array to typed array conversion failed"
	);
	pResult = xrtTypedArrayToValue(pArray, NULL);
	testRequire(
		(pResult != NULL) && (xrtValueCount(pResult) == 2u) &&
		xrtValueGetInt(xrtValueArrayGet(pResult, 0u), &iValue) &&
		(iValue == 7),
		"typed array to dynamic array conversion failed"
	);
	xrtValueRelease(pResult);
	xrtTypedArrayDestroy(pArray);
	xrtValueRelease(pSource);
	testTypedArrayValueSnapshot();
	testTypedArrayValueCleanup();
	testTypedArrayValueReentry();
	testTypedArrayValueItems();
}

#endif



#if defined(XRUNTIME_FEATURE_TYPED_LIST_VALUE)

/* 在稀疏映射解码期间写入一个不属于当前快照的新键。 */
static bool testTypedListValueMutateSource(xvalue* pSource)
{
	return xrtValueIntMapSetNew(pSource, 99, xrtValueInt(97));
}



/* 验证稀疏列表转换读取开始时的稳定动态快照。 */
static void testTypedListValueSnapshot(void)
{
	xvalue* pSource = xrtValueIntMap();
	testtypedvaluesnapshot State = {
		pSource,
		testTypedListValueMutateSource,
		0u
	};
	xvalueconverter Converter = {
		&State,
		testTypedValueSnapshotDecode,
		NULL
	};
	xrttype Type = testTypedValueType();
	xtypedlist* pList;

	testRequire(
		(pSource != NULL) &&
		xrtValueIntMapSetNew(pSource, -3, xrtValueInt(17)) &&
		xrtValueIntMapSetNew(pSource, 9, xrtValueInt(23)),
		"typed list snapshot fixture failed"
	);
	pList = xrtTypedListFromValue(pSource, &Type, &Converter);
	testRequire(
		(pList != NULL) && (State.Calls == 2u) &&
		(xrtTypedListCount(pList) == 2u) &&
		(xrtValueCount(pSource) == 3u),
		"typed list conversion did not hold a source snapshot"
	);
	xrtTypedListDestroy(pList);
	xrtValueRelease(pSource);
}



/* 验证稀疏列表失败回收不会被元素销毁错误覆盖。 */
static void testTypedListValueCleanup(void)
{
	testtypedvaluecleanup State = { 0u, 0u, false };
	xvalueconverter Converter = {
		&State,
		testTypedValueCleanupDecode,
		NULL
	};
	xrttype Type = testTypedValueCleanupType(&State);
	xvalue* pSource = xrtValueIntMap();

	testRequire(
		(pSource != NULL) &&
		xrtValueIntMapSetNew(pSource, -1, xrtValueInt(3)) &&
		xrtValueIntMapSetNew(pSource, 1, xrtValueInt(5)),
		"typed list cleanup fixture failed"
	);
	xrtClearError();
	testRequire(
		(xrtTypedListFromValue(pSource, &Type, &Converter) == NULL) &&
		(State.DecodeCount == 2u) && (State.DropCount >= 3u) &&
		testTypedValueCleanupErrorHeld(),
		"typed list cleanup replaced the root conversion error"
	);
	xrtClearError();
	xrtValueRelease(pSource);
}



/* 尝试在类型列表编码回调中写入新键。 */
static bool testTypedListValueMutate(ptr pContainer, const void* pItem)
{
	return xrtTypedListSet((xtypedlist*)pContainer, 99, pItem);
}



/* 验证类型列表桥接拒绝转换器重入。 */
static void testTypedListValueReentry(void)
{
	xrttype Type = testTypedValueType();
	xtypedlist* pList = xrtTypedListCreate(&Type);
	testtypedvaluereentry State = {
		pList,
		testTypedListValueMutate,
		false
	};
	xvalueconverter Converter = {
		&State,
		NULL,
		testTypedValueReenter
	};
	xvalue* pResult;
	int iValue = 5;

	testRequire(
		(pList != NULL) && xrtTypedListSet(pList, 1, &iValue),
		"typed list reentry fixture failed"
	);
	xrtClearError();
	pResult = xrtTypedListToValue(pList, &Converter);
	testRequire(
		(pResult == NULL) && State.Attempted &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		(xrtTypedListCount(pList) == 1u),
		"typed list Value converter reentry was not rejected"
	);
	xrtTypedListDestroy(pList);
}

/* 验证类型列表的单元素动态值便利操作。 */
static void testTypedListValueItems(void)
{
	xtypedlist* pList = xrtTypedListCreate(xrtTypeInt64());
	xvalue* pFirst = xrtValueInt(11);
	xvalue* pSecond = xrtValueInt(22);
	xvalue* pResult;
	int64 iKey = 0;
	int64 iValue = 0;

	testRequire(
		(pList != NULL) && (pFirst != NULL) && (pSecond != NULL) &&
		xrtTypedListSetValue(pList, -2, pFirst, NULL) &&
		xrtTypedListAppendValue(pList, pSecond, &iKey, NULL) &&
		(iKey == -1),
		"typed list single Value mutation failed"
	);
	pResult = xrtTypedListGetValue(pList, -1, NULL);
	testRequire(
		(pResult != NULL) && xrtValueGetInt(pResult, &iValue) &&
		(iValue == 22) &&
		xrtTypedListFindValue(pList, pSecond, &iKey, NULL) &&
		(iKey == -1) && xrtTypedListContainsValue(pList, pFirst, NULL),
		"typed list single Value query failed"
	);
	xrtValueRelease(pResult);
	pResult = xrtTypedListTakeValue(pList, -2, NULL);
	testRequire(
		(pResult != NULL) && xrtValueGetInt(pResult, &iValue) &&
		(iValue == 11) && (xrtTypedListCount(pList) == 1u),
		"typed list single Value take failed"
	);
	xrtValueRelease(pResult);
	xrtValueRelease(pSecond);
	xrtValueRelease(pFirst);
	xrtTypedListDestroy(pList);
}



/* 验证动态整数映射与稀疏类型列表往返。 */
static void testTypedListValue(void)
{
	xvalue* pSource = xrtValueIntMap();
	xtypedlist* pList;
	xvalue* pResult;
	int64 iValue = 0;

	testRequire(
		(pSource != NULL) &&
		xrtValueIntMapSetNew(pSource, -3, xrtValueInt(17)) &&
		xrtValueIntMapSetNew(pSource, 9, xrtValueInt(23)),
		"typed list Value fixture failed"
	);
	pList = xrtTypedListFromValue(pSource, xrtTypeInt32(), NULL);
	testRequire(
		(pList != NULL) && (xrtTypedListCount(pList) == 2u) &&
		(*(const int32*)xrtTypedListConstGet(pList, -3) == 17),
		"dynamic integer map to typed list conversion failed"
	);
	pResult = xrtTypedListToValue(pList, NULL);
	testRequire(
		(pResult != NULL) &&
		xrtValueGetInt(xrtValueIntMapGet(pResult, 9), &iValue) &&
		(iValue == 23),
		"typed list to dynamic integer map conversion failed"
	);
	xrtValueRelease(pResult);
	xrtTypedListDestroy(pList);
	xrtValueRelease(pSource);
	testTypedListValueSnapshot();
	testTypedListValueCleanup();
	testTypedListValueReentry();
	testTypedListValueItems();
}

#endif



#if defined(XRUNTIME_FEATURE_TYPED_SET_VALUE)

/* 在集合解码期间加入一个不属于当前快照的新元素。 */
static bool testTypedSetValueMutateSource(xvalue* pSource)
{
	return xrtValueSetAddNew(pSource, xrtValueInt(97));
}



/* 验证集合转换读取开始时的稳定动态快照。 */
static void testTypedSetValueSnapshot(void)
{
	xvalue* pSource = xrtValueSet();
	testtypedvaluesnapshot State = {
		pSource,
		testTypedSetValueMutateSource,
		0u
	};
	xvalueconverter Converter = {
		&State,
		testTypedValueSnapshotDecode,
		NULL
	};
	xrttype Type = testTypedValueType();
	xtypedset* pSet;

	testRequire(
		(pSource != NULL) &&
		xrtValueSetAddNew(pSource, xrtValueInt(5)) &&
		xrtValueSetAddNew(pSource, xrtValueInt(13)),
		"typed set snapshot fixture failed"
	);
	pSet = xrtTypedSetFromValue(pSource, &Type, &Converter);
	testRequire(
		(pSet != NULL) && (State.Calls == 2u) &&
		(xrtTypedSetCount(pSet) == 2u) &&
		(xrtValueCount(pSource) == 3u),
		"typed set conversion did not hold a source snapshot"
	);
	xrtTypedSetDestroy(pSet);
	xrtValueRelease(pSource);
}



/* 验证集合失败回收不会被元素销毁错误覆盖。 */
static void testTypedSetValueCleanup(void)
{
	testtypedvaluecleanup State = { 0u, 0u, false };
	xvalueconverter Converter = {
		&State,
		testTypedValueCleanupDecode,
		NULL
	};
	xrttype Type = testTypedValueCleanupType(&State);
	xvalue* pSource = xrtValueSet();

	testRequire(
		(pSource != NULL) &&
		xrtValueSetAddNew(pSource, xrtValueInt(3)) &&
		xrtValueSetAddNew(pSource, xrtValueInt(5)),
		"typed set cleanup fixture failed"
	);
	xrtClearError();
	testRequire(
		(xrtTypedSetFromValue(pSource, &Type, &Converter) == NULL) &&
		(State.DecodeCount == 2u) && (State.DropCount >= 3u) &&
		testTypedValueCleanupErrorHeld(),
		"typed set cleanup replaced the root conversion error"
	);
	xrtClearError();
	xrtValueRelease(pSource);
}



/* 尝试在类型集合编码回调中加入元素。 */
static bool testTypedSetValueMutate(ptr pContainer, const void* pItem)
{
	return xrtTypedSetAdd((xtypedset*)pContainer, pItem);
}



/* 验证类型集合桥接拒绝转换器重入。 */
static void testTypedSetValueReentry(void)
{
	xrttype Type = testTypedValueType();
	xtypedset* pSet = xrtTypedSetCreate(&Type);
	testtypedvaluereentry State = {
		pSet,
		testTypedSetValueMutate,
		false
	};
	xvalueconverter Converter = {
		&State,
		NULL,
		testTypedValueReenter
	};
	xvalue* pResult;
	int iValue = 7;

	testRequire(
		(pSet != NULL) && xrtTypedSetAdd(pSet, &iValue),
		"typed set reentry fixture failed"
	);
	xrtClearError();
	pResult = xrtTypedSetToValue(pSet, &Converter);
	testRequire(
		(pResult == NULL) && State.Attempted &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		(xrtTypedSetCount(pSet) == 1u),
		"typed set Value converter reentry was not rejected"
	);
	xrtTypedSetDestroy(pSet);
}

/* 验证类型集合的单元素动态值便利操作。 */
static void testTypedSetValueItems(void)
{
	xtypedset* pSet = xrtTypedSetCreate(xrtTypeInt32());
	xvalue* pFive = xrtValueInt(5);
	xvalue* pThirteen = xrtValueInt(13);
	xvalue* pResult;
	int64 iValue = 0;

	testRequire(
		(pSet != NULL) && (pFive != NULL) && (pThirteen != NULL) &&
		xrtTypedSetAddValue(pSet, pFive, NULL) &&
		xrtTypedSetAddValue(pSet, pThirteen, NULL) &&
		xrtTypedSetAddValue(pSet, pFive, NULL) &&
		(xrtTypedSetCount(pSet) == 2u) &&
		xrtTypedSetHasValue(pSet, pThirteen, NULL),
		"typed set single Value mutation failed"
	);
	pResult = xrtTypedSetGetValue(pSet, pThirteen, NULL);
	testRequire(
		(pResult != NULL) && xrtValueGetInt(pResult, &iValue) &&
		(iValue == 13),
		"typed set canonical Value query failed"
	);
	xrtValueRelease(pResult);
	pResult = xrtTypedSetTakeValue(pSet, pThirteen, NULL);
	testRequire(
		(pResult != NULL) && xrtValueGetInt(pResult, &iValue) &&
		(iValue == 13) && (xrtTypedSetCount(pSet) == 1u) &&
		xrtTypedSetRemoveValue(pSet, pFive, NULL) &&
		(xrtTypedSetCount(pSet) == 0u),
		"typed set single Value removal failed"
	);
	xrtValueRelease(pResult);
	xrtValueRelease(pThirteen);
	xrtValueRelease(pFive);
	xrtTypedSetDestroy(pSet);
}



/* 验证动态集合与类型集合往返。 */
static void testTypedSetValue(void)
{
	xvalue* pSource = xrtValueSet();
	xtypedset* pSet;
	xvalue* pResult;
	xvalue* pNeedle;
	int32 iNeedle = 13;

	testRequire(
		(pSource != NULL) &&
		xrtValueSetAddNew(pSource, xrtValueInt(5)) &&
		xrtValueSetAddNew(pSource, xrtValueInt(13)),
		"typed set Value fixture failed"
	);
	pSet = xrtTypedSetFromValue(pSource, xrtTypeInt32(), NULL);
	testRequire(
		(pSet != NULL) && (xrtTypedSetCount(pSet) == 2u) &&
		xrtTypedSetHas(pSet, &iNeedle),
		"dynamic set to typed set conversion failed"
	);
	pResult = xrtTypedSetToValue(pSet, NULL);
	pNeedle = xrtValueInt(13);
	testRequire(
		(pResult != NULL) && (xrtValueCount(pResult) == 2u) &&
		(pNeedle != NULL) && xrtValueSetHas(pResult, pNeedle),
		"typed set to dynamic set conversion failed"
	);
	xrtValueRelease(pNeedle);
	xrtValueRelease(pResult);
	xrtTypedSetDestroy(pSet);
	xrtValueRelease(pSource);
	testTypedSetValueSnapshot();
	testTypedSetValueCleanup();
	testTypedSetValueReentry();
	testTypedSetValueItems();
}

#endif



#if defined(XRUNTIME_FEATURE_TYPED_DICT_VALUE)

/* 在对象解码期间写入一个不属于当前快照的新键。 */
static bool testTypedDictValueMutateSource(xvalue* pSource)
{
	xstrview Key = XRT_STR_INIT("later");

	return xrtValueObjectSetNew(pSource, Key, xrtValueInt(97));
}



/* 验证字典转换读取开始时的稳定动态快照。 */
static void testTypedDictValueSnapshot(void)
{
	xstrview First = XRT_STR_INIT("first");
	xstrview Second = XRT_STR_INIT("second");
	xvalue* pSource = xrtValueObject();
	testtypedvaluesnapshot State = {
		pSource,
		testTypedDictValueMutateSource,
		0u
	};
	xvalueconverter Converter = {
		&State,
		testTypedValueSnapshotDecode,
		NULL
	};
	xrttype Type = testTypedValueType();
	xtypeddict* pDict;

	testRequire(
		(pSource != NULL) &&
		xrtValueObjectSetNew(pSource, First, xrtValueInt(29)) &&
		xrtValueObjectSetNew(pSource, Second, xrtValueInt(31)),
		"typed dictionary snapshot fixture failed"
	);
	pDict = xrtTypedDictFromValue(pSource, &Type, &Converter);
	testRequire(
		(pDict != NULL) && (State.Calls == 2u) &&
		(xrtTypedDictCount(pDict) == 2u) &&
		(xrtValueCount(pSource) == 3u),
		"typed dictionary conversion did not hold a source snapshot"
	);
	xrtTypedDictDestroy(pDict);
	xrtValueRelease(pSource);
}



/* 验证字典失败回收不会被元素销毁错误覆盖。 */
static void testTypedDictValueCleanup(void)
{
	xstrview First = XRT_STR_INIT("first");
	xstrview Second = XRT_STR_INIT("second");
	testtypedvaluecleanup State = { 0u, 0u, false };
	xvalueconverter Converter = {
		&State,
		testTypedValueCleanupDecode,
		NULL
	};
	xrttype Type = testTypedValueCleanupType(&State);
	xvalue* pSource = xrtValueObject();

	testRequire(
		(pSource != NULL) &&
		xrtValueObjectSetNew(pSource, First, xrtValueInt(3)) &&
		xrtValueObjectSetNew(pSource, Second, xrtValueInt(5)),
		"typed dictionary cleanup fixture failed"
	);
	xrtClearError();
	testRequire(
		(xrtTypedDictFromValue(pSource, &Type, &Converter) == NULL) &&
		(State.DecodeCount == 2u) && (State.DropCount >= 3u) &&
		testTypedValueCleanupErrorHeld(),
		"typed dictionary cleanup replaced the root conversion error"
	);
	xrtClearError();
	xrtValueRelease(pSource);
}



/* 尝试在类型字典编码回调中写入新键。 */
static bool testTypedDictValueMutate(ptr pContainer, const void* pItem)
{
	xstrview Key = XRT_STR_INIT("reentered");

	return xrtTypedDictSet((xtypeddict*)pContainer, Key, pItem);
}



/* 验证类型字典桥接拒绝转换器重入。 */
static void testTypedDictValueReentry(void)
{
	xrttype Type = testTypedValueType();
	xtypeddict* pDict = xrtTypedDictCreate(&Type);
	xstrview Key = XRT_STR_INIT("item");
	testtypedvaluereentry State = {
		pDict,
		testTypedDictValueMutate,
		false
	};
	xvalueconverter Converter = {
		&State,
		NULL,
		testTypedValueReenter
	};
	xvalue* pResult;
	int iValue = 9;

	testRequire(
		(pDict != NULL) &&
		xrtTypedDictSet(pDict, Key, &iValue),
		"typed dictionary reentry fixture failed"
	);
	xrtClearError();
	pResult = xrtTypedDictToValue(pDict, &Converter);
	testRequire(
		(pResult == NULL) && State.Attempted &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		(xrtTypedDictCount(pDict) == 1u),
		"typed dictionary Value converter reentry was not rejected"
	);
	xrtTypedDictDestroy(pDict);
}

/* 验证类型字典的单元素动态值便利操作和二进制文本键。 */
static void testTypedDictValueItems(void)
{
	static const char arrKey[] = { 'x', '\0', 'y' };
	xstrview Key = { arrKey, sizeof(arrKey) };
	xtypeddict* pDict = xrtTypedDictCreate(xrtTypeInt64());
	xvalue* pInput = xrtValueInt(41);
	xvalue* pResult;
	int64 iValue = 0;

	testRequire(
		(pDict != NULL) && (pInput != NULL) &&
		xrtTypedDictSetValue(pDict, Key, pInput, NULL),
		"typed dictionary single Value mutation failed"
	);
	pResult = xrtTypedDictGetValue(pDict, Key, NULL);
	testRequire(
		(pResult != NULL) && xrtValueGetInt(pResult, &iValue) &&
		(iValue == 41),
		"typed dictionary single Value query failed"
	);
	xrtValueRelease(pResult);
	pResult = xrtTypedDictTakeValue(pDict, Key, NULL);
	testRequire(
		(pResult != NULL) && xrtValueGetInt(pResult, &iValue) &&
		(iValue == 41) && (xrtTypedDictCount(pDict) == 0u),
		"typed dictionary single Value take failed"
	);
	xrtValueRelease(pResult);
	xrtValueRelease(pInput);
	xrtTypedDictDestroy(pDict);
}



/* 验证动态对象与类型字典往返，并保留带零字节的键。 */
static void testTypedDictValue(void)
{
	static const char arrKey[] = { 'a', '\0', 'b' };
	xstrview Key = { arrKey, sizeof(arrKey) };
	xvalue* pSource = xrtValueObject();
	xtypeddict* pDict;
	xvalue* pResult;
	int64 iValue = 0;

	testRequire(
		(pSource != NULL) &&
		xrtValueObjectSetNew(pSource, Key, xrtValueInt(29)),
		"typed dictionary Value fixture failed"
	);
	pDict = xrtTypedDictFromValue(pSource, xrtTypeInt64(), NULL);
	testRequire(
		(pDict != NULL) && (xrtTypedDictCount(pDict) == 1u) &&
		(*(const int64*)xrtTypedDictConstGet(pDict, Key) == 29),
		"dynamic object to typed dictionary conversion failed"
	);
	pResult = xrtTypedDictToValue(pDict, NULL);
	testRequire(
		(pResult != NULL) &&
		xrtValueGetInt(xrtValueObjectGet(pResult, Key), &iValue) &&
		(iValue == 29),
		"typed dictionary to dynamic object conversion failed"
	);
	xrtValueRelease(pResult);
	xrtTypedDictDestroy(pDict);
	xrtValueRelease(pSource);
	testTypedDictValueSnapshot();
	testTypedDictValueCleanup();
	testTypedDictValueReentry();
	testTypedDictValueItems();
}

#endif



/* 运行当前裁剪组合启用的动态值桥接测试。 */
int main(void)
{
	testTypedValueScalars();
#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_CALLABLE)
	testTypedValueCallable();
#endif
#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_FUTURE)
	testTypedValueFuture();
#endif
#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE)
	testTypedRuntimeValue();
#endif
#if defined(XRUNTIME_FEATURE_TYPED_ARRAY_VALUE)
	testTypedArrayValue();
#endif
#if defined(XRUNTIME_FEATURE_TYPED_LIST_VALUE)
	testTypedListValue();
#endif
#if defined(XRUNTIME_FEATURE_TYPED_SET_VALUE)
	testTypedSetValue();
#endif
#if defined(XRUNTIME_FEATURE_TYPED_DICT_VALUE)
	testTypedDictValue();
#endif
	xrtClearError();
	printf("[PASS] typed Value bridge\n");
	return 0;
}
