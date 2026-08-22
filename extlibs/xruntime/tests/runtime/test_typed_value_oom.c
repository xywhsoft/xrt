#include "../test.h"



typedef struct testtypedvalueoom {
	bool Fail;
	size_t AllocCalls;
	size_t ReallocCalls;
} testtypedvalueoom;



/* 比较 OOM 测试记录中的整数。 */
static int testTypedValueOomCompare(
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



/* 散列 OOM 测试记录中的整数。 */
static uint64 testTypedValueOomHash(
	const void* pValue,
	const xrttype* pType
)
{
	int iValue;
	(void)pType;

	memcpy(&iValue, pValue, sizeof(iValue));
	return (uint64)(uint32)iValue;
}



/* 按开关分配转换测试内存。 */
static ptr testTypedValueOomAlloc(ptr pContext, size_t iSize)
{
	testtypedvalueoom* pState = (testtypedvalueoom*)pContext;

	pState->AllocCalls++;
	return pState->Fail ? NULL : malloc(iSize);
}



/* 按开关重分配转换测试内存。 */
static ptr testTypedValueOomRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	testtypedvalueoom* pState = (testtypedvalueoom*)pContext;

	pState->ReallocCalls++;
	return pState->Fail ? NULL : realloc(pMemory, iSize);
}



/* 释放转换测试内存。 */
static void testTypedValueOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 返回只由自定义转换器处理的平凡记录类型。 */
static xrttype testTypedValueOomType(void)
{
	static const xrttypeops Ops = {
		.Compare = testTypedValueOomCompare,
		.Hash = testTypedValueOomHash
	};
	xrttype Type = {
		.Id = 0,
		.Kind = XRT_TYPE_RECORD,
		.Flags = XRT_TYPE_FLAG_TRIVIAL_COPY | XRT_TYPE_FLAG_TRIVIAL_DROP |
			XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_RELOCATABLE |
			XRT_TYPE_FLAG_FINAL,
		.Name = XRT_STR_INIT("OomInt"),
		.AbiName = XRT_STR_INIT("tests.OomInt"),
		.Size = sizeof(int),
		.Align = TEST_ALIGNOF(int),
		.InstanceSize = sizeof(int),
		.InstanceAlign = TEST_ALIGNOF(int),
		.Ops = &Ops
	};

	Type.Id = xrtTypeId(Type.AbiName);
	return Type;
}



/* 在解码前强制申请大块内存，使后备分配器失败可确定复现。 */
static bool testTypedValueOomToTyped(
	const xvalue* pSource,
	const xrttype* pTargetType,
	ptr pTarget,
	ptr pContext
)
{
	ptr pProbe = xrtMalloc(8u * 1024u * 1024u);
	int64 iValue;
	int iResult;
	(void)pTargetType;
	(void)pContext;

	if ( pProbe == NULL ) {
		return false;
	}
	xrtFree(pProbe);
	if ( !xrtValueGetInt(pSource, &iValue) ) {
		return false;
	}
	iResult = (int)iValue;
	memcpy(pTarget, &iResult, sizeof(iResult));
	return true;
}



/* 在编码前强制申请大块内存，使后备分配器失败可确定复现。 */
static xvalue* testTypedValueOomFromTyped(
	const xrttype* pSourceType,
	const void* pSource,
	ptr pContext
)
{
	ptr pProbe = xrtMalloc(8u * 1024u * 1024u);
	int iValue;
	(void)pSourceType;
	(void)pContext;

	if ( pProbe == NULL ) {
		return NULL;
	}
	xrtFree(pProbe);
	memcpy(&iValue, pSource, sizeof(iValue));
	return xrtValueInt((int64)iValue);
}



#if defined(XRUNTIME_FEATURE_TYPED_SET_VALUE)

/* 完成解码后武装 OOM，使同一次 TakeValue 的编码阶段确定失败。 */
static bool testTypedValueOomArmAfterDecode(
	const xvalue* pSource,
	const xrttype* pTargetType,
	ptr pTarget,
	ptr pContext
)
{
	testtypedvalueoom* pState = (testtypedvalueoom*)pContext;
	int64 iValue;
	int iResult;
	(void)pTargetType;

	if ( !xrtValueGetInt(pSource, &iValue) ) {
		return false;
	}
	iResult = (int)iValue;
	memcpy(pTarget, &iResult, sizeof(iResult));
	pState->Fail = true;
	return true;
}

#endif



#if defined(XRUNTIME_FEATURE_TYPED_ARRAY_VALUE)

/* 验证类型数组桥接的双向 OOM 失败原子性。 */
static void testTypedArrayValueOom(
	testtypedvalueoom* pState,
	const xrttype* pType,
	const xvalueconverter* pConverter
)
{
	xrttype LargeType = *pType;
	xvalue* pSource = xrtValueArray();
	xtypedarray* pTyped = xrtTypedArrayCreate(pType);
	xtypedarray* pInline = xrtTypedArrayCreate(xrtTypeInt32());
	xtypedarray* pLarge;
	xvalue* pValue;
	int iItem = 11;
	xvalue* pItemValue = xrtValueInt(iItem + 1);
	size_t iAllocCalls;

	LargeType.Size = 128u;
	LargeType.Align = 1u;
	LargeType.InstanceSize = 128u;
	LargeType.InstanceAlign = 1u;
	pLarge = xrtTypedArrayCreate(&LargeType);

	testRequire(
		(pSource != NULL) && (pTyped != NULL) && (pInline != NULL) &&
		(pLarge != NULL) &&
		(pItemValue != NULL) &&
		xrtValueArrayAppendNew(pSource, xrtValueInt(iItem)) &&
		xrtTypedArrayPush(pTyped, &iItem) &&
		xrtTypedArrayReserve(pInline, 1u),
		"typed array OOM fixture failed"
	);
	iAllocCalls = pState->AllocCalls + pState->ReallocCalls;
	testRequire(
		xrtTypedArrayPushValue(pInline, pItemValue, NULL) &&
		((pState->AllocCalls + pState->ReallocCalls) == iAllocCalls),
		"typed array small Value scratch performed a heap allocation"
	);
	pState->Fail = true;
	xrtClearError();
	testRequire(
		xrtTypedArrayFromValue(pSource, pType, pConverter) == NULL &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtValueCount(pSource) == 1u),
		"typed array FromValue did not preserve OOM atomically"
	);
	xrtClearError();
	pValue = xrtTypedArrayToValue(pTyped, pConverter);
	testRequire(
		(pValue == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtTypedArrayCount(pTyped) == 1u),
		"typed array ToValue did not preserve OOM atomically"
	);
	xrtClearError();
	testRequire(
		!xrtTypedArrayPushValue(pTyped, pItemValue, pConverter) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtTypedArrayCount(pTyped) == 1u),
		"typed array single Value write did not preserve OOM atomically"
	);
	xrtClearError();
	testRequire(
		!xrtTypedArrayPushValue(pLarge, pItemValue, pConverter) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtTypedArrayCount(pLarge) == 0u),
		"typed array large Value scratch did not preserve OOM atomically"
	);
	xrtClearError();
	pValue = xrtTypedArrayTakeValue(pTyped, 0u, pConverter);
	testRequire(
		(pValue == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtTypedArrayCount(pTyped) == 1u),
		"typed array single Value take did not preserve OOM atomically"
	);
	pState->Fail = false;
	xrtValueRelease(pItemValue);
	xrtTypedArrayDestroy(pLarge);
	xrtTypedArrayDestroy(pInline);
	xrtTypedArrayDestroy(pTyped);
	xrtValueRelease(pSource);
}

#endif



#if defined(XRUNTIME_FEATURE_TYPED_LIST_VALUE)

/* 验证类型列表桥接的双向 OOM 失败原子性。 */
static void testTypedListValueOom(
	testtypedvalueoom* pState,
	const xrttype* pType,
	const xvalueconverter* pConverter
)
{
	xvalue* pSource = xrtValueIntMap();
	xtypedlist* pTyped = xrtTypedListCreate(pType);
	xvalue* pValue;
	int iItem = 13;
	xvalue* pItemValue = xrtValueInt(iItem + 1);

	testRequire(
		(pSource != NULL) && (pTyped != NULL) && (pItemValue != NULL) &&
		xrtValueIntMapSetNew(pSource, -7, xrtValueInt(iItem)) &&
		xrtTypedListSet(pTyped, -7, &iItem),
		"typed list OOM fixture failed"
	);
	pState->Fail = true;
	xrtClearError();
	testRequire(
		xrtTypedListFromValue(pSource, pType, pConverter) == NULL &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtValueCount(pSource) == 1u),
		"typed list FromValue did not preserve OOM atomically"
	);
	xrtClearError();
	pValue = xrtTypedListToValue(pTyped, pConverter);
	testRequire(
		(pValue == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtTypedListCount(pTyped) == 1u),
		"typed list ToValue did not preserve OOM atomically"
	);
	xrtClearError();
	testRequire(
		!xrtTypedListSetValue(pTyped, 4, pItemValue, pConverter) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtTypedListCount(pTyped) == 1u),
		"typed list single Value write did not preserve OOM atomically"
	);
	xrtClearError();
	pValue = xrtTypedListTakeValue(pTyped, -7, pConverter);
	testRequire(
		(pValue == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtTypedListCount(pTyped) == 1u),
		"typed list single Value take did not preserve OOM atomically"
	);
	pState->Fail = false;
	xrtValueRelease(pItemValue);
	xrtTypedListDestroy(pTyped);
	xrtValueRelease(pSource);
}

#endif



#if defined(XRUNTIME_FEATURE_TYPED_SET_VALUE)

/* 验证类型集合桥接的双向 OOM 失败原子性。 */
static void testTypedSetValueOom(
	testtypedvalueoom* pState,
	const xrttype* pType,
	const xvalueconverter* pConverter
)
{
	xvalue* pSource = xrtValueSet();
	xtypedset* pTyped = xrtTypedSetCreate(pType);
	xvalueconverter TakeConverter = {
		pState,
		testTypedValueOomArmAfterDecode,
		testTypedValueOomFromTyped
	};
	xvalue* pValue;
	int iItem = 17;
	xvalue* pItemValue = xrtValueInt(iItem + 1);
	xvalue* pNeedle = xrtValueInt(iItem);

	testRequire(
		(pSource != NULL) && (pTyped != NULL) &&
		(pItemValue != NULL) && (pNeedle != NULL) &&
		xrtValueSetAddNew(pSource, xrtValueInt(iItem)) &&
		xrtTypedSetAdd(pTyped, &iItem),
		"typed set OOM fixture failed"
	);
	pState->Fail = true;
	xrtClearError();
	testRequire(
		xrtTypedSetFromValue(pSource, pType, pConverter) == NULL &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtValueCount(pSource) == 1u),
		"typed set FromValue did not preserve OOM atomically"
	);
	xrtClearError();
	pValue = xrtTypedSetToValue(pTyped, pConverter);
	testRequire(
		(pValue == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtTypedSetCount(pTyped) == 1u),
		"typed set ToValue did not preserve OOM atomically"
	);
	xrtClearError();
	testRequire(
		!xrtTypedSetAddValue(pTyped, pItemValue, pConverter) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtTypedSetCount(pTyped) == 1u),
		"typed set single Value write did not preserve OOM atomically"
	);
	xrtClearError();
	pValue = xrtTypedSetTakeValue(pTyped, pItemValue, pConverter);
	testRequire(
		(pValue == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtTypedSetCount(pTyped) == 1u),
		"typed set single Value take did not preserve OOM atomically"
	);
	pState->Fail = false;
	xrtClearError();
	pValue = xrtTypedSetTakeValue(pTyped, pNeedle, &TakeConverter);
	testRequire(
		(pValue == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtTypedSetCount(pTyped) == 1u),
		"typed set canonical Value encode did not preserve OOM atomically"
	);
	pState->Fail = false;
	xrtValueRelease(pNeedle);
	xrtValueRelease(pItemValue);
	xrtTypedSetDestroy(pTyped);
	xrtValueRelease(pSource);
}

#endif



#if defined(XRUNTIME_FEATURE_TYPED_DICT_VALUE)

/* 验证类型字典桥接的双向 OOM 失败原子性。 */
static void testTypedDictValueOom(
	testtypedvalueoom* pState,
	const xrttype* pType,
	const xvalueconverter* pConverter
)
{
	xstrview Key = XRT_STR_INIT("item");
	xstrview Other = XRT_STR_INIT("other");
	xvalue* pSource = xrtValueObject();
	xtypeddict* pTyped = xrtTypedDictCreate(pType);
	xvalue* pValue;
	int iItem = 19;
	xvalue* pItemValue = xrtValueInt(iItem + 1);

	testRequire(
		(pSource != NULL) && (pTyped != NULL) && (pItemValue != NULL) &&
		xrtValueObjectSetNew(pSource, Key, xrtValueInt(iItem)) &&
		xrtTypedDictSet(pTyped, Key, &iItem),
		"typed dictionary OOM fixture failed"
	);
	pState->Fail = true;
	xrtClearError();
	testRequire(
		xrtTypedDictFromValue(pSource, pType, pConverter) == NULL &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtValueCount(pSource) == 1u),
		"typed dictionary FromValue did not preserve OOM atomically"
	);
	xrtClearError();
	pValue = xrtTypedDictToValue(pTyped, pConverter);
	testRequire(
		(pValue == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtTypedDictCount(pTyped) == 1u),
		"typed dictionary ToValue did not preserve OOM atomically"
	);
	xrtClearError();
	testRequire(
		!xrtTypedDictSetValue(
			pTyped, Other, pItemValue, pConverter
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtTypedDictCount(pTyped) == 1u),
		"typed dictionary single Value write did not preserve OOM atomically"
	);
	xrtClearError();
	pValue = xrtTypedDictTakeValue(pTyped, Key, pConverter);
	testRequire(
		(pValue == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtTypedDictCount(pTyped) == 1u),
		"typed dictionary single Value take did not preserve OOM atomically"
	);
	pState->Fail = false;
	xrtValueRelease(pItemValue);
	xrtTypedDictDestroy(pTyped);
	xrtValueRelease(pSource);
}

#endif



/* 验证转换器和容器桥接在 OOM 后不返回半成品。 */
int main(void)
{
	testtypedvalueoom State = { false, 0u, 0u };
	xallocator Allocator = {
		&State,
		testTypedValueOomAlloc,
		testTypedValueOomRealloc,
		testTypedValueOomFree
	};
	xrttype Type = testTypedValueOomType();
	xvalueconverter Converter = {
		NULL,
		testTypedValueOomToTyped,
		testTypedValueOomFromTyped
	};
	xvalue* pSource;
	xvalue* pResult;
	int iValue = 31;
	int iOutput = 0;

	testRequire(xrtSetAllocator(&Allocator), "typed Value OOM allocator install failed");
	pSource = xrtValueInt(31);
	testRequire(pSource != NULL, "typed Value OOM fixture failed");
	State.Fail = true;
	xrtClearError();
	testRequire(
		!xrtValueToTyped(pSource, &Type, &iOutput, &Converter) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"typed Value decoder did not preserve OOM"
	);
	xrtClearError();
	pResult = xrtValueFromTyped(&Type, &iValue, &Converter);
	testRequire(
		(pResult == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"typed Value encoder did not preserve OOM"
	);
	State.Fail = false;
	xrtValueRelease(pSource);

#if defined(XRUNTIME_FEATURE_TYPED_ARRAY_VALUE)
	testTypedArrayValueOom(&State, &Type, &Converter);
#endif
#if defined(XRUNTIME_FEATURE_TYPED_LIST_VALUE)
	testTypedListValueOom(&State, &Type, &Converter);
#endif
#if defined(XRUNTIME_FEATURE_TYPED_SET_VALUE)
	testTypedSetValueOom(&State, &Type, &Converter);
#endif
#if defined(XRUNTIME_FEATURE_TYPED_DICT_VALUE)
	testTypedDictValueOom(&State, &Type, &Converter);
#endif
	xrtClearError();
	printf("[PASS] typed Value bridge OOM\n");
	return 0;
}
