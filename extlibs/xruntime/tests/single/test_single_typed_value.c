#define XRUNTIME_IMPLEMENTATION
#include "../../single/xruntime.h"



/* 验证单头文件中的标量和当前容器转换入口。 */
int main(void)
{
	xvalue* pSource = xrtValueInt(73);
	xvalue* pResult;
	int16 iValue = 0;
	int32 iBool32 = -3;
	int64 iOutput = 0;
	bool bOutput = false;
	int iResult = 0;

	if ( (pSource == NULL) ||
		 !xrtValueToTyped(pSource, xrtTypeInt16(), &iValue, NULL) ||
		 (iValue != 73) ) {
		iResult = 1;
	}
	pResult = xrtValueFromTyped(xrtTypeInt16(), &iValue, NULL);
	if ( (pResult == NULL) || !xrtValueGetInt(pResult, &iOutput) ||
		 (iOutput != 73) ) {
		iResult = 2;
	}
	xrtValueRelease(pResult);
	pResult = xrtValueFromTyped(xrtTypeBool32(), &iBool32, NULL);
	if ( (pResult == NULL) || !xrtValueGetBool(pResult, &bOutput) ||
		 !bOutput ) {
		iResult = 8;
	}
	xrtValueRelease(pResult);
	pResult = NULL;
#if defined(XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE)
	{
		xvalue* pOwned = NULL;
		bool bOwned;

		bOwned = xrtValueToTyped(
			pSource, xrtTypeValue(), &pOwned, NULL
		);
		if ( bOwned ) {
			pResult = xrtValueFromTyped(
				xrtTypeValue(), &pOwned, NULL
			);
		}
		if ( !bOwned || (pResult == NULL) ||
			 !xrtValueGetInt(pResult, &iOutput) || (iOutput != 73) ) {
			iResult = 3;
		}
		xrtValueRelease(pResult);
		pResult = NULL;
		if ( bOwned ) {
			xrtTypeDropValue(xrtTypeValue(), &pOwned);
		}
	}
#endif
#if defined(XRUNTIME_FEATURE_TYPED_ARRAY_VALUE)
	{
		xvalue* pArrayValue = xrtValueArray();
		xtypedarray* pArray;

		if ( (pArrayValue == NULL) ||
			 !xrtValueArrayAppendNew(pArrayValue, xrtValueInt(9)) ||
			 ((pArray = xrtTypedArrayFromValue(
				pArrayValue, xrtTypeInt32(), NULL
			)) == NULL) || (xrtTypedArrayCount(pArray) != 1u) ) {
			iResult = 4;
			pArray = NULL;
		}
		xrtTypedArrayDestroy(pArray);
		xrtValueRelease(pArrayValue);
	}
#endif
#if defined(XRUNTIME_FEATURE_TYPED_LIST_VALUE)
	{
		xvalue* pListValue = xrtValueIntMap();
		xtypedlist* pList;

		if ( (pListValue == NULL) ||
			 !xrtValueIntMapSetNew(pListValue, -2, xrtValueInt(11)) ||
			 ((pList = xrtTypedListFromValue(
				pListValue, xrtTypeInt32(), NULL
			)) == NULL) || (xrtTypedListCount(pList) != 1u) ) {
			iResult = 5;
			pList = NULL;
		}
		xrtTypedListDestroy(pList);
		xrtValueRelease(pListValue);
	}
#endif
#if defined(XRUNTIME_FEATURE_TYPED_SET_VALUE)
	{
		xvalue* pSetValue = xrtValueSet();
		xtypedset* pSet;
		int32 iNeedle = 13;

		if ( (pSetValue == NULL) ||
			 !xrtValueSetAddNew(pSetValue, xrtValueInt(iNeedle)) ||
			 ((pSet = xrtTypedSetFromValue(
				pSetValue, xrtTypeInt32(), NULL
			)) == NULL) || !xrtTypedSetHas(pSet, &iNeedle) ) {
			iResult = 6;
			pSet = NULL;
		}
		xrtTypedSetDestroy(pSet);
		xrtValueRelease(pSetValue);
	}
#endif
#if defined(XRUNTIME_FEATURE_TYPED_DICT_VALUE)
	{
		xstrview Key = XRT_STR_INIT("item");
		xvalue* pDictValue = xrtValueObject();
		xtypeddict* pDict;

		if ( (pDictValue == NULL) ||
			 !xrtValueObjectSetNew(pDictValue, Key, xrtValueInt(17)) ||
			 ((pDict = xrtTypedDictFromValue(
				pDictValue, xrtTypeInt32(), NULL
			)) == NULL) || (xrtTypedDictCount(pDict) != 1u) ) {
			iResult = 7;
			pDict = NULL;
		}
		xrtTypedDictDestroy(pDict);
		xrtValueRelease(pDictValue);
	}
#endif
	xrtValueRelease(pSource);
	return iResult;
}
