#include <stdio.h>
#include <xruntime.h>



#if defined(XRUNTIME_FEATURE_TYPED_ARRAY_VALUE)

/* 展示动态稠密数组与类型数组的往返转换。 */
static bool exampleTypedArrayValue(size_t* pCount)
{
	xvalue* pSource = xrtValueArray();
	xvalue* pInput = xrtValueInt(9);
	xtypedarray* pTyped;
	xvalue* pSingle = NULL;
	xvalue* pResult = NULL;
	int64 iValue = 0;
	bool bSuccess;

	if ( (pSource == NULL) || (pInput == NULL) ||
		 !xrtValueArrayAppendNew(pSource, xrtValueInt(7)) ) {
		xrtValueRelease(pInput);
		xrtValueRelease(pSource);
		return false;
	}

	/* 整容器转换后仍可直接用动态值操作单个元素。 */
	pTyped = xrtTypedArrayFromValue(pSource, xrtTypeInt32(), NULL);
	if ( (pTyped != NULL) &&
		 xrtTypedArrayPushValue(pTyped, pInput, NULL) ) {
		pSingle = xrtTypedArrayGetValue(pTyped, 1u, NULL);
	}
	if ( (pSingle != NULL) &&
		 xrtValueGetInt(pSingle, &iValue) && (iValue == 9) ) {
		pResult = xrtTypedArrayToValue(pTyped, NULL);
	}
	bSuccess = (pResult != NULL);
	if ( bSuccess ) {
		*pCount += xrtValueCount(pResult);
	}
	xrtValueRelease(pResult);
	xrtValueRelease(pSingle);
	xrtTypedArrayDestroy(pTyped);
	xrtValueRelease(pInput);
	xrtValueRelease(pSource);
	return bSuccess;
}

#endif



#if defined(XRUNTIME_FEATURE_TYPED_LIST_VALUE)

/* 展示动态整数映射与稀疏类型列表的往返转换。 */
static bool exampleTypedListValue(size_t* pCount)
{
	xvalue* pSource = xrtValueIntMap();
	xvalue* pInput = xrtValueInt(23);
	xtypedlist* pTyped;
	xvalue* pSingle = NULL;
	xvalue* pResult = NULL;
	int64 iKey = 0;
	int64 iValue = 0;
	bool bSuccess;

	if ( (pSource == NULL) || (pInput == NULL) ||
		 !xrtValueIntMapSetNew(pSource, -3, xrtValueInt(11)) ) {
		xrtValueRelease(pInput);
		xrtValueRelease(pSource);
		return false;
	}

	/* AppendValue 返回实际整数键，省去手工临时类型值。 */
	pTyped = xrtTypedListFromValue(pSource, xrtTypeInt32(), NULL);
	if ( (pTyped != NULL) &&
		 xrtTypedListAppendValue(pTyped, pInput, &iKey, NULL) ) {
		pSingle = xrtTypedListGetValue(pTyped, iKey, NULL);
	}
	if ( (pSingle != NULL) &&
		 xrtValueGetInt(pSingle, &iValue) && (iValue == 23) ) {
		pResult = xrtTypedListToValue(pTyped, NULL);
	}
	bSuccess = (pResult != NULL);
	if ( bSuccess ) {
		*pCount += xrtValueCount(pResult);
	}
	xrtValueRelease(pResult);
	xrtValueRelease(pSingle);
	xrtTypedListDestroy(pTyped);
	xrtValueRelease(pInput);
	xrtValueRelease(pSource);
	return bSuccess;
}

#endif



#if defined(XRUNTIME_FEATURE_TYPED_SET_VALUE)

/* 展示动态集合与类型集合的往返转换。 */
static bool exampleTypedSetValue(size_t* pCount)
{
	xvalue* pSource = xrtValueSet();
	xvalue* pInput = xrtValueInt(29);
	xtypedset* pTyped;
	xvalue* pSingle = NULL;
	xvalue* pResult = NULL;
	int64 iValue = 0;
	bool bSuccess;

	if ( (pSource == NULL) || (pInput == NULL) ||
		 !xrtValueSetAddNew(pSource, xrtValueInt(13)) ) {
		xrtValueRelease(pInput);
		xrtValueRelease(pSource);
		return false;
	}

	/* GetValue 返回集合中规范元素的独立动态值。 */
	pTyped = xrtTypedSetFromValue(pSource, xrtTypeInt32(), NULL);
	if ( (pTyped != NULL) &&
		 xrtTypedSetAddValue(pTyped, pInput, NULL) ) {
		pSingle = xrtTypedSetGetValue(pTyped, pInput, NULL);
	}
	if ( (pSingle != NULL) &&
		 xrtValueGetInt(pSingle, &iValue) && (iValue == 29) ) {
		pResult = xrtTypedSetToValue(pTyped, NULL);
	}
	bSuccess = (pResult != NULL);
	if ( bSuccess ) {
		*pCount += xrtValueCount(pResult);
	}
	xrtValueRelease(pResult);
	xrtValueRelease(pSingle);
	xrtTypedSetDestroy(pTyped);
	xrtValueRelease(pInput);
	xrtValueRelease(pSource);
	return bSuccess;
}

#endif



#if defined(XRUNTIME_FEATURE_TYPED_DICT_VALUE)

/* 展示动态对象与类型字典的往返转换。 */
static bool exampleTypedDictValue(size_t* pCount)
{
	xstrview Key = XRT_STR_INIT("answer");
	xstrview ExtraKey = XRT_STR_INIT("extra");
	xvalue* pSource = xrtValueObject();
	xvalue* pInput = xrtValueInt(51);
	xtypeddict* pTyped;
	xvalue* pSingle = NULL;
	xvalue* pResult = NULL;
	int64 iValue = 0;
	bool bSuccess;

	if ( (pSource == NULL) || (pInput == NULL) ||
		 !xrtValueObjectSetNew(pSource, Key, xrtValueInt(42)) ) {
		xrtValueRelease(pInput);
		xrtValueRelease(pSource);
		return false;
	}

	/* 文本键显式携带长度，单元素桥接不会退化到 strlen。 */
	pTyped = xrtTypedDictFromValue(pSource, xrtTypeInt32(), NULL);
	if ( (pTyped != NULL) &&
		 xrtTypedDictSetValue(pTyped, ExtraKey, pInput, NULL) ) {
		pSingle = xrtTypedDictGetValue(pTyped, ExtraKey, NULL);
	}
	if ( (pSingle != NULL) &&
		 xrtValueGetInt(pSingle, &iValue) && (iValue == 51) ) {
		pResult = xrtTypedDictToValue(pTyped, NULL);
	}
	bSuccess = (pResult != NULL);
	if ( bSuccess ) {
		*pCount += xrtValueCount(pResult);
	}
	xrtValueRelease(pResult);
	xrtValueRelease(pSingle);
	xrtTypedDictDestroy(pTyped);
	xrtValueRelease(pInput);
	xrtValueRelease(pSource);
	return bSuccess;
}

#endif



/* 运行当前裁剪组合启用的容器桥接示例。 */
int main(void)
{
	size_t iCount = 0u;
	bool bSuccess = true;

#if defined(XRUNTIME_FEATURE_TYPED_ARRAY_VALUE)
	bSuccess = exampleTypedArrayValue(&iCount) && bSuccess;
#endif
#if defined(XRUNTIME_FEATURE_TYPED_LIST_VALUE)
	bSuccess = exampleTypedListValue(&iCount) && bSuccess;
#endif
#if defined(XRUNTIME_FEATURE_TYPED_SET_VALUE)
	bSuccess = exampleTypedSetValue(&iCount) && bSuccess;
#endif
#if defined(XRUNTIME_FEATURE_TYPED_DICT_VALUE)
	bSuccess = exampleTypedDictValue(&iCount) && bSuccess;
#endif
	printf("converted=%zu\n", iCount);
	return bSuccess ? 0 : 1;
}
