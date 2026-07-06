


typedef struct {
	int64 X;
	int64 Y;
} XrtTestPoint;

typedef struct {
	int Id;
} XrtTestResource;

static int __g_iXrtTestHandleDropCount = 0;
static int __g_iXrtTestResourceDropCount = 0;


// 测试用 record copy
static void __xrtTestPointCopy(ptr pDst, const ptr pSrc)
{
	memcpy(pDst, pSrc, sizeof(XrtTestPoint));
}


// 测试用 handle drop
static void __xrtTestHandleDrop(ptr pObj)
{
	ptr* ppHandle = (ptr*)pObj;
	if ( ppHandle && *ppHandle ) {
		__g_iXrtTestHandleDropCount++;
		*ppHandle = NULL;
	}
}


// 测试用资源 copy
static void __xrtTestResourceCopy(ptr pDst, const ptr pSrc)
{
	memcpy(pDst, pSrc, sizeof(XrtTestResource));
}


// 测试用资源 drop
static void __xrtTestResourceDrop(ptr pObj)
{
	XrtTestResource* pRes = (XrtTestResource*)pObj;
	if ( pRes != NULL && pRes->Id != 0 ) {
		__g_iXrtTestResourceDropCount++;
		pRes->Id = 0;
	}
}


static const xrt_type_ops __xrtTestPointOps = {
	.init = NULL,
	.copy = __xrtTestPointCopy,
	.move = NULL,
	.drop = NULL,
	.compare = NULL,
	.hash = NULL,
	.to_string = NULL,
	.box = NULL,
	.unbox = NULL
};

static const xrt_type_ops __xrtTestHandleOps = {
	.init = NULL,
	.copy = NULL,
	.move = NULL,
	.drop = __xrtTestHandleDrop,
	.compare = NULL,
	.hash = NULL,
	.to_string = NULL,
	.box = NULL,
	.unbox = NULL
};

static const xrt_type_ops __xrtTestResourceOps = {
	.init = NULL,
	.copy = __xrtTestResourceCopy,
	.move = NULL,
	.drop = __xrtTestResourceDrop,
	.compare = NULL,
	.hash = NULL,
	.to_string = NULL,
	.box = NULL,
	.unbox = NULL
};

static const xrt_func_sig __xrtTestPointToStringSig = {
	.TypeId = 0x1000100010001000ULL,
	.Name = "toString",
	.NameSize = 8,
	.ParamCount = 0,
	.Params = NULL,
	.ReturnCount = 0,
	.ReturnTypes = NULL,
	.Flags = 0
};

static const xrt_method_desc __xrtTestPointMethods[] = {
	{
		.Name = "toString",
		.NameSize = 8,
		.Sig = &__xrtTestPointToStringSig,
		.NativeEntry = NULL,
		.NativeAbi = XRT_NATIVE_ABI_XCALL,
		.XCallEntry = NULL
	}
};

static const xrt_method_table __xrtTestPointMethodTable = {
	.Count = 1,
	.Methods = __xrtTestPointMethods
};

static const xrt_type_desc __xrtTestPointType = {
	.TypeId = 10001,
	.Kind = XRT_TYPE_KIND_RECORD,
	.Name = "XrtTestPoint",
	.NameSize = 12,
	.AbiName = "XrtTestPoint",
	.AbiNameSize = 12,
	.Size = sizeof(XrtTestPoint),
	.Align = sizeof(int64),
	.Ops = &__xrtTestPointOps,
	.Methods = &__xrtTestPointMethodTable,
	.Extra = NULL
};

static const xrt_type_desc __xrtTestHandleType = {
	.TypeId = 10002,
	.Kind = XRT_TYPE_KIND_HANDLE,
	.Name = "XrtTestHandle",
	.NameSize = 13,
	.AbiName = "ptr",
	.AbiNameSize = 3,
	.Size = sizeof(ptr),
	.Align = sizeof(ptr),
	.Ops = &__xrtTestHandleOps,
	.Extra = NULL
};

static const xrt_type_desc __xrtTestIntSetType = {
	.TypeId = 10003,
	.Kind = XRT_TYPE_KIND_SET,
	.Name = "set<int>",
	.NameSize = 8,
	.AbiName = "xvalue",
	.AbiNameSize = 6,
	.Size = sizeof(xset_struct),
	.Align = sizeof(ptr),
	.Ops = NULL,
	.Extra = NULL
};

static const xrt_type_desc __xrtTestResourceType = {
	.TypeId = 10004,
	.Kind = XRT_TYPE_KIND_RECORD,
	.Name = "XrtTestResource",
	.NameSize = 15,
	.AbiName = "XrtTestResource",
	.AbiNameSize = 15,
	.Size = sizeof(XrtTestResource),
	.Align = sizeof(int),
	.Ops = &__xrtTestResourceOps,
	.Extra = NULL
};

static const xrt_type_desc* const __xrtTestPointArrayArgs[] = {
	&__xrtTestPointType
};

static const xrt_generic_type __xrtTestPointArrayGeneric = {
	.ArgCount = 1,
	.Args = __xrtTestPointArrayArgs
};

static const xrt_type_desc __xrtTestPointArrayType = {
	.TypeId = 10005,
	.Kind = XRT_TYPE_KIND_ARRAY,
	.Name = "array<XrtTestPoint>",
	.NameSize = 19,
	.AbiName = "xtarray",
	.AbiNameSize = 7,
	.Size = sizeof(xtarray),
	.Align = sizeof(ptr),
	.Ops = NULL,
	.Extra = &__xrtTestPointArrayGeneric
};


// 测试用 xcall：把所有位置参数相加
static bool __xrtTestSumXCall(xrt_call_frame* pFrame, xrt_call_result* pResult)
{
	uint32 i;
	int64 iSum = 0;
	if ( (pFrame == NULL) || (pResult == NULL) ) {
		return FALSE;
	}
	for ( i = 0; i < pFrame->Argc; i++ ) {
		iSum += xvoGetInt(pFrame->Argv[i]);
	}
	return xrtCallResultSetValue(pResult, 0, xvoCreateInt(iSum), TRUE);
}


// 测试用 xcall：读取位置参数和 kwargs，并写入多返回值
static bool __xrtTestKwMultiXCall(xrt_call_frame* pFrame, xrt_call_result* pResult)
{
	int64 iBase = 0;
	int64 iBonus = 0;
	uint32 iKwCount = 0;

	if ( (pFrame == NULL) || (pResult == NULL) ) {
		return FALSE;
	}
	if ( pFrame->Argc > 0 && pFrame->Argv != NULL ) {
		iBase = xvoGetInt(pFrame->Argv[0]);
	}
	if ( pFrame->Kwargs != NULL ) {
		iKwCount = xvoTableItemCount(pFrame->Kwargs);
		iBonus = xvoTableGetInt(pFrame->Kwargs, "bonus", 0);
	}

	return xrtCallResultSetValue(pResult, 0, xvoCreateInt(iBase + iBonus), TRUE) &&
	       xrtCallResultSetValue(pResult, 1, xvoCreateInt((int64)iKwCount), TRUE) &&
	       xrtCallResultSetValue(pResult, 4, xvoCreateInt(444), TRUE);
}


// Runtime Type / callable / typed value 基础测试
int Test_RuntimeType(xrtGlobalData* pCore)
{
	xrt_func_param arrParam[2];
	const xrt_type_desc* arrReturn[1];
	xrt_func_param arrKwParam[1];
	const xrt_type_desc* arrKwReturn[5];
	xrt_func_sig tSig;
	xrt_func_sig tKwSig;
	xrt_callable* pCallable;
	xrt_callable* pKwCallable;
	xrt_callable* pUnboxedCallable;
	xrt_callable* pCopiedCallable;
	xvalue arrArg[2];
	xvalue arrKwArg[1];
	xrt_call_frame tFrame;
	xrt_call_result tResult;
	xvalue vFunc;
	xvalue vKwFunc;
	xvalue vKwargs;
	xvalue vOverflow;
	xvalue vRecord;
	xvalue vRecordCopy;
	xvalue vHandle;
	xvalue vValueSet;
	xvalue vSetItem;
	xvalue vSetAt0;
	xvalue vSetAt1;
	xset pSet;
	xset pStringSet;
	xtarray pTypedArray;
	xtlist pTypedList;
	xtset pTypedSet;
	xtdict pTypedDict;
	XrtTestPoint tPoint;
	XrtTestPoint* pPoint;
	XrtTestResource tRes;
	XrtTestResource* pRes;
	int64* pSetAt0;
	int64* pSetAt1;
	int64* pTypedInt;
	int64 iSetVal;
	int64 iTypedVal;
	int64 iListKey;
	const char* sOne;
	const char* sTwo;
	const char* sDictKey;
	const char* sBoxedRaw;
	str sUnboxed;
	uint32 iDictKeyLen;
	const char** ppString;
	const xrt_method_desc* pMethod;
	xvalue vBoxed;
	int iHandleObj = 123;
	int64 iUnboxed;
	bool bTypedBool;
	str sTypeText;
	uint32 iTypeTextLen;
	bool bConverted;
	int64 iConverted;
	double fConverted;
	str sConverted;
	xvalue vConvert;

	(void)pCore;

	if ( !xrtTypeSame(xrtTypeInt(), xrtTypeInt()) ) {
		return 1;
	}
	if ( xrtTypeSame(xrtTypeInt(), xrtTypeFloat()) ) {
		return 2;
	}
	if ( !xrtTypeCanConvert(xrtTypeBool(), xrtTypeInt(), XRT_TYPE_CONVERT_SAFE_WIDEN) ) {
		return 115;
	}
	if ( xrtTypeCanConvert(xrtTypeInt(), xrtTypeFloat(), XRT_TYPE_CONVERT_SAFE_WIDEN) ) {
		return 116;
	}
	bConverted = TRUE;
	iConverted = 0;
	if ( !xrtTypeConvertValue(xrtTypeInt(), &iConverted, xrtTypeBool(), &bConverted, XRT_TYPE_CONVERT_SAFE_WIDEN) ||
	     iConverted != 1 ) {
		return 117;
	}
	iConverted = 42;
	fConverted = 0.0;
	if ( xrtTypeConvertValue(xrtTypeFloat(), &fConverted, xrtTypeInt(), &iConverted, XRT_TYPE_CONVERT_SAFE_WIDEN) ) {
		return 118;
	}
	if ( !xrtTypeConvertValue(xrtTypeFloat(), &fConverted, xrtTypeInt(), &iConverted, XRT_TYPE_CONVERT_EXPLICIT) ||
	     fConverted != 42.0 ) {
		return 119;
	}
	sOne = "123";
	iConverted = 0;
	if ( !xrtTypeConvertValue(xrtTypeInt(), &iConverted, xrtTypeString(), &sOne, XRT_TYPE_CONVERT_EXPLICIT) ||
	     iConverted != 123 ) {
		return 120;
	}
	sConverted = NULL;
	if ( !xrtTypeConvertValue(xrtTypeString(), &sConverted, xrtTypeInt(), &iConverted, XRT_TYPE_CONVERT_EXPLICIT) ||
	     sConverted == NULL || strcmp(sConverted, "123") != 0 ) {
		return 121;
	}
	xrtTypeDropValue(xrtTypeString(), &sConverted);
	vConvert = xvoCreateText("88", 0, FALSE);
	if ( vConvert == NULL ) {
		return 122;
	}
	iConverted = 0;
	if ( xrtValueConvertTo(vConvert, xrtTypeInt(), &iConverted, XRT_TYPE_CONVERT_SAFE_WIDEN) ) {
		xvoUnref(vConvert);
		return 123;
	}
	if ( !xrtValueConvertTo(vConvert, xrtTypeInt(), &iConverted, XRT_TYPE_CONVERT_EXPLICIT) || iConverted != 88 ) {
		xvoUnref(vConvert);
		return 124;
	}
	xvoUnref(vConvert);
	if ( xrtTypeGenericArgCount(&__xrtTestPointArrayType) != 1 ||
	     xrtTypeGenericArg(&__xrtTestPointArrayType, 0) != &__xrtTestPointType ||
	     xrtTypeGenericArg(&__xrtTestPointArrayType, 1) != NULL ) {
		return 68;
	}
	if ( xrtTypeMethods(&__xrtTestPointType) != &__xrtTestPointMethodTable ) {
		return 92;
	}
	pMethod = xrtTypeFindMethod(&__xrtTestPointType, "toString", 0);
	if ( pMethod == NULL || pMethod->Sig != &__xrtTestPointToStringSig ||
	     xrtTypeFindMethod(&__xrtTestPointType, "missing", 0) != NULL ) {
		return 93;
	}
	pMethod = xrtTypeFindMethod(xrtTypeInt(), "toString", 0);
	if ( pMethod == NULL || pMethod->Sig == NULL ||
	     pMethod->Sig->ReturnCount != 1 ||
	     pMethod->Sig->ReturnTypes == NULL ||
	     pMethod->Sig->ReturnTypes[0] != xrtTypeString() ) {
		return 98;
	}
	if ( xrtTypeFindMethod(xrtTypeBool(), "missing", 0) != NULL ) {
		return 99;
	}
	iSetVal = 42;
	sTypeText = xrtTypeToStringValue(xrtTypeInt(), &iSetVal, &iTypeTextLen);
	if ( sTypeText == NULL || strcmp((const char*)sTypeText, "42") != 0 || iTypeTextLen != 2 ) {
		return 100;
	}
	xrtFree(sTypeText);
	bTypedBool = TRUE;
	sTypeText = xrtTypeToStringValue(xrtTypeBool(), &bTypedBool, &iTypeTextLen);
	if ( sTypeText == NULL || strcmp((const char*)sTypeText, "true") != 0 || iTypeTextLen != 4 ) {
		return 101;
	}
	xrtFree(sTypeText);
	sOne = "typed";
	sTypeText = xrtTypeToStringValue(xrtTypeString(), &sOne, &iTypeTextLen);
	if ( sTypeText == NULL || strcmp((const char*)sTypeText, "typed") != 0 || iTypeTextLen != 5 ) {
		return 102;
	}
	xrtFree(sTypeText);
	vBoxed = xvoCreateInt(321);
	sTypeText = xvoToString(vBoxed, &iTypeTextLen);
	if ( sTypeText == NULL || strcmp((const char*)sTypeText, "321") != 0 || iTypeTextLen != 3 ) {
		return 103;
	}
	xrtFree(sTypeText);
	xvoUnref(vBoxed);
	vBoxed = xvoCreateText("boxed-text", 0, FALSE);
	sTypeText = xvoToString(vBoxed, &iTypeTextLen);
	if ( sTypeText == NULL || strcmp((const char*)sTypeText, "boxed-text") != 0 || iTypeTextLen != 10 ) {
		return 104;
	}
	xrtFree(sTypeText);
	xvoUnref(vBoxed);
	if ( xrtTypeInt()->Ops == NULL || xrtTypeInt()->Ops->box == NULL ||
	     xrtTypeInt()->Ops->unbox == NULL || xrtTypeInt()->Ops->to_string == NULL ) {
		return 77;
	}
	iSetVal = 1234;
	vBoxed = xrtTypeInt()->Ops->box(&iSetVal, xrtTypeInt());
	iUnboxed = 0;
	if ( vBoxed == NULL || !xrtTypeInt()->Ops->unbox(vBoxed, &iUnboxed, xrtTypeInt()) || iUnboxed != 1234 ) {
		return 78;
	}
	xvoUnref(vBoxed);
	if ( xrtTypeString()->Ops == NULL || xrtTypeString()->Ops->box == NULL ||
	     xrtTypeString()->Ops->unbox == NULL || xrtTypeString()->Ops->drop == NULL ) {
		return 79;
	}
	sBoxedRaw = "boxed";
	vBoxed = xrtTypeString()->Ops->box(&sBoxedRaw, xrtTypeString());
	sUnboxed = NULL;
	if ( vBoxed == NULL || !xrtTypeString()->Ops->unbox(vBoxed, &sUnboxed, xrtTypeString()) ||
	     sUnboxed == NULL || strcmp(sUnboxed, "boxed") != 0 ) {
		return 80;
	}
	xrtTypeString()->Ops->drop(&sUnboxed);
	xvoUnref(vBoxed);
	if ( sUnboxed != NULL ) {
		return 81;
	}

	memset(arrParam, 0, sizeof(arrParam));
	arrParam[0].Name = "a";
	arrParam[0].NameSize = 1;
	arrParam[0].Type = xrtTypeInt();
	arrParam[1].Name = "b";
	arrParam[1].NameSize = 1;
	arrParam[1].Type = xrtTypeInt();
	arrReturn[0] = xrtTypeInt();
	memset(&tSig, 0, sizeof(tSig));
	tSig.TypeId = 0x1122334455667788ULL;
	tSig.Name = "sum";
	tSig.NameSize = 3;
	tSig.ParamCount = 2;
	tSig.Params = arrParam;
	tSig.ReturnCount = 1;
	tSig.ReturnTypes = arrReturn;

	pCallable = xrtCallableCreate(&tSig, NULL, XRT_NATIVE_ABI_XCALL, __xrtTestSumXCall, NULL, NULL);
	if ( pCallable == NULL ) {
		return 3;
	}
	vFunc = xvoCreateCallable(pCallable, TRUE);
	if ( vFunc == NULL ) {
		xrtCallableUnref(pCallable);
		return 4;
	}
	if ( xrtCallableSig(pCallable) != &tSig || xrtCallableTypeId(pCallable) != 0x1122334455667788ULL ) {
		return 36;
	}
	if ( xvoCallableSig(vFunc) != &tSig || xrtFuncSigTypeId(xvoCallableSig(vFunc)) != 0x1122334455667788ULL ) {
		return 37;
	}
	if ( xrtTypeFunction()->Ops == NULL ||
	     xrtTypeFunction()->Ops->box == NULL ||
	     xrtTypeFunction()->Ops->unbox == NULL ||
	     xrtTypeFunction()->Ops->copy == NULL ||
	     xrtTypeFunction()->Ops->drop == NULL ) {
		return 111;
	}
	pUnboxedCallable = NULL;
	pCopiedCallable = NULL;
	if ( !xrtTypeFunction()->Ops->unbox(vFunc, &pUnboxedCallable, xrtTypeFunction()) ||
	     pUnboxedCallable == NULL ||
	     xrtCallableSig(pUnboxedCallable) != &tSig ) {
		return 112;
	}
	xrtTypeFunction()->Ops->copy(&pCopiedCallable, &pUnboxedCallable);
	if ( pCopiedCallable == NULL || xrtCallableSig(pCopiedCallable) != &tSig ) {
		return 113;
	}
	vBoxed = xrtTypeFunction()->Ops->box(&pCopiedCallable, xrtTypeFunction());
	if ( vBoxed == NULL || xvoCallableSig(vBoxed) != &tSig ) {
		return 114;
	}

	arrArg[0] = xvoCreateInt(7);
	arrArg[1] = xvoCreateInt(5);
	memset(&tFrame, 0, sizeof(tFrame));
	tFrame.Argc = 2;
	tFrame.Argv = arrArg;
	xrtCallResultInit(&tResult);
	if ( !xvoInvoke(vBoxed, &tFrame, &tResult) ) {
		return 5;
	}
	if ( tResult.Count != 1 || xvoGetInt(tResult.Values[0]) != 12 ) {
		return 6;
	}
	xrtCallResultUnit(&tResult);
	xvoUnref(vBoxed);
	xrtTypeFunction()->Ops->drop(&pCopiedCallable);
	xrtTypeFunction()->Ops->drop(&pUnboxedCallable);
	xvoUnref(arrArg[0]);
	xvoUnref(arrArg[1]);
	xvoUnref(vFunc);

	memset(arrKwParam, 0, sizeof(arrKwParam));
	arrKwParam[0].Name = "base";
	arrKwParam[0].NameSize = 4;
	arrKwParam[0].Type = xrtTypeInt();
	arrKwReturn[0] = xrtTypeInt();
	arrKwReturn[1] = xrtTypeInt();
	arrKwReturn[2] = xrtTypeInt();
	arrKwReturn[3] = xrtTypeInt();
	arrKwReturn[4] = xrtTypeInt();
	memset(&tKwSig, 0, sizeof(tKwSig));
	tKwSig.TypeId = 0x9988776655443322ULL;
	tKwSig.Name = "kw_multi";
	tKwSig.NameSize = 8;
	tKwSig.ParamCount = 1;
	tKwSig.Params = arrKwParam;
	tKwSig.ReturnCount = 5;
	tKwSig.ReturnTypes = arrKwReturn;
	pKwCallable = xrtCallableCreate(&tKwSig, NULL, XRT_NATIVE_ABI_XCALL, __xrtTestKwMultiXCall, NULL, NULL);
	if ( pKwCallable == NULL ) {
		return 105;
	}
	vKwFunc = xvoCreateCallable(pKwCallable, TRUE);
	if ( vKwFunc == NULL ) {
		xrtCallableUnref(pKwCallable);
		return 106;
	}
	vKwargs = xvoCreateTable();
	if ( vKwargs == NULL ) {
		xvoUnref(vKwFunc);
		return 107;
	}
	if ( !xvoTableSetInt(vKwargs, "bonus", 0, 8) || !xvoTableSetInt(vKwargs, "mode", 0, 1) ) {
		xvoUnref(vKwargs);
		xvoUnref(vKwFunc);
		return 108;
	}
	arrKwArg[0] = xvoCreateInt(34);
	memset(&tFrame, 0, sizeof(tFrame));
	tFrame.Argc = 1;
	tFrame.Argv = arrKwArg;
	tFrame.Kwargs = vKwargs;
	xrtCallResultInit(&tResult);
	if ( !xvoInvoke(vKwFunc, &tFrame, &tResult) ) {
		xvoUnref(arrKwArg[0]);
		xvoUnref(vKwargs);
		xvoUnref(vKwFunc);
		return 109;
	}
	vOverflow = tResult.Overflow;
	if ( tResult.Count != 5 ||
	     xvoGetInt(tResult.Values[0]) != 42 ||
	     xvoGetInt(tResult.Values[1]) != 2 ||
	     vOverflow == NULL ||
	     xvoArrayItemCount(vOverflow) != 1 ||
	     xvoArrayGetInt(vOverflow, 0) != 444 ) {
		xrtCallResultUnit(&tResult);
		xvoUnref(arrKwArg[0]);
		xvoUnref(vKwargs);
		xvoUnref(vKwFunc);
		return 110;
	}
	xrtCallResultUnit(&tResult);
	xvoUnref(arrKwArg[0]);
	xvoUnref(vKwargs);
	xvoUnref(vKwFunc);

	tPoint.X = 11;
	tPoint.Y = 22;
	vRecord = xvoCreateRecord(&__xrtTestPointType, &tPoint);
	if ( vRecord == NULL ) {
		return 7;
	}
	if ( !xrtTypeSame(xvoTypeDesc(vRecord), &__xrtTestPointType) ) {
		return 8;
	}
	pPoint = (XrtTestPoint*)xvoGetRecordData(vRecord);
	if ( pPoint == NULL || pPoint->X != 11 || pPoint->Y != 22 ) {
		return 9;
	}
	vRecordCopy = xvoCopy(vRecord);
	if ( vRecordCopy == NULL ) {
		return 10;
	}
	pPoint = (XrtTestPoint*)xvoGetRecordData(vRecordCopy);
	if ( pPoint == NULL || pPoint->X != 11 || pPoint->Y != 22 ) {
		return 11;
	}
	xvoUnref(vRecordCopy);
	xvoUnref(vRecord);

	__g_iXrtTestHandleDropCount = 0;
	vHandle = xvoCreateHandle(&__xrtTestHandleType, &iHandleObj, XRT_HANDLE_FLAG_OWNED);
	if ( vHandle == NULL ) {
		return 12;
	}
	if ( !xrtTypeSame(xvoTypeDesc(vHandle), &__xrtTestHandleType) ) {
		return 13;
	}
	if ( xvoGetHandleData(vHandle) != &iHandleObj ) {
		return 14;
	}
	xvoUnref(vHandle);
	if ( __g_iXrtTestHandleDropCount != 1 ) {
		return 15;
	}

	pSet = xrtSetCreate(xrtTypeInt(), XRT_OBJMODE_LOCAL);
	if ( pSet == NULL ) {
		return 16;
	}
	iSetVal = 100;
	if ( !xrtSetAdd(pSet, &iSetVal) ) {
		return 17;
	}
	iSetVal = 200;
	if ( !xrtSetAdd(pSet, &iSetVal) ) {
		return 18;
	}
	iSetVal = 100;
	if ( !xrtSetExists(pSet, &iSetVal) ) {
		return 19;
	}
	if ( xrtSetCount(pSet) != 2 ) {
		return 20;
	}
	pSetAt0 = (int64*)xrtSetItemAt(pSet, 0);
	pSetAt1 = (int64*)xrtSetItemAt(pSet, 1);
	if ( pSetAt0 == NULL || pSetAt1 == NULL ) {
		return 31;
	}
	if ( !((*pSetAt0 == 100 && *pSetAt1 == 200) || (*pSetAt0 == 200 && *pSetAt1 == 100)) ) {
		return 32;
	}
	if ( !xrtSetRemove(pSet, &iSetVal) ) {
		return 21;
	}
	if ( xrtSetExists(pSet, &iSetVal) ) {
		return 22;
	}
	if ( xrtSetCount(pSet) != 1 ) {
		return 23;
	}
	xrtSetDestroy(pSet);

	pStringSet = xrtSetCreate(xrtTypeString(), XRT_OBJMODE_LOCAL);
	if ( pStringSet == NULL ) {
		return 44;
	}
	sOne = "same";
	sTwo = "same";
	if ( !xrtSetAdd(pStringSet, &sOne) || !xrtSetAdd(pStringSet, &sTwo) ) {
		return 45;
	}
	if ( xrtSetCount(pStringSet) != 1 ) {
		return 46;
	}
	ppString = (const char**)xrtSetGet(pStringSet, &sTwo);
	if ( ppString == NULL || strcmp(*ppString, "same") != 0 ) {
		return 47;
	}
	xrtSetDestroy(pStringSet);

	pTypedArray = xrtTypedArrayCreate(&__xrtTestResourceType, XRT_OBJMODE_LOCAL);
	if ( pTypedArray == NULL ) {
		return 48;
	}
	__g_iXrtTestResourceDropCount = 0;
	tRes.Id = 1;
	if ( xrtTypedArrayAppend(pTypedArray, &tRes) == NULL ) {
		return 49;
	}
	tRes.Id = 2;
	if ( xrtTypedArrayAppend(pTypedArray, &tRes) == NULL ) {
		return 50;
	}
	tRes.Id = 3;
	if ( !xrtTypedArraySet(pTypedArray, 1, &tRes) ) {
		return 51;
	}
	if ( __g_iXrtTestResourceDropCount != 1 ) {
		return 52;
	}
	pRes = (XrtTestResource*)xrtTypedArrayGet(pTypedArray, 1);
	if ( pRes == NULL || pRes->Id != 3 || xrtTypedArrayCount(pTypedArray) != 2 ) {
		return 53;
	}
	if ( !xrtTypedArrayRemove(pTypedArray, 2, 1) ) {
		return 54;
	}
	if ( __g_iXrtTestResourceDropCount != 2 || xrtTypedArrayCount(pTypedArray) != 1 ) {
		return 55;
	}
	xrtTypedArrayDestroy(pTypedArray);
	if ( __g_iXrtTestResourceDropCount != 3 ) {
		return 56;
	}

	pTypedList = xrtTypedListCreate(xrtTypeInt(), XRT_OBJMODE_LOCAL);
	if ( pTypedList == NULL ) {
		return 57;
	}
	iTypedVal = 50;
	if ( xrtTypedListSet(pTypedList, 5, &iTypedVal) == NULL ) {
		return 58;
	}
	iTypedVal = 60;
	if ( xrtTypedListSet(pTypedList, 5, &iTypedVal) == NULL ) {
		return 59;
	}
	pTypedInt = (int64*)xrtTypedListGet(pTypedList, 5);
	if ( pTypedInt == NULL || *pTypedInt != 60 || xrtTypedListCount(pTypedList) != 1 ) {
		return 60;
	}
	iListKey = 0;
	pTypedInt = (int64*)xrtTypedListItemAt(pTypedList, 0, &iListKey);
	if ( pTypedInt == NULL || *pTypedInt != 60 || iListKey != 5 ) {
		return 94;
	}
	if ( !xrtTypedListRemove(pTypedList, 5) || xrtTypedListCount(pTypedList) != 0 ) {
		return 61;
	}
	xrtTypedListDestroy(pTypedList);

	pTypedArray = xrtTypedArrayCreate(xrtTypeInt(), XRT_OBJMODE_LOCAL);
	if ( pTypedArray == NULL ) {
		return 82;
	}
	vBoxed = xvoCreateInt(88);
	if ( vBoxed == NULL || !xrtTypedArrayAppendValue(pTypedArray, vBoxed) ) {
		return 83;
	}
	xvoUnref(vBoxed);
	pTypedInt = (int64*)xrtTypedArrayGet(pTypedArray, 1);
	if ( pTypedInt == NULL || *pTypedInt != 88 ) {
		return 84;
	}
	vBoxed = xvoCreateInt(99);
	if ( vBoxed == NULL || !xrtTypedArraySetValue(pTypedArray, 1, vBoxed) ) {
		return 85;
	}
	xvoUnref(vBoxed);
	pTypedInt = (int64*)xrtTypedArrayGet(pTypedArray, 1);
	if ( pTypedInt == NULL || *pTypedInt != 99 ) {
		return 86;
	}
	xrtTypedArrayDestroy(pTypedArray);

	pTypedSet = xrtTypedSetCreate(xrtTypeInt(), XRT_OBJMODE_LOCAL);
	if ( pTypedSet == NULL ) {
		return 89;
	}
	vBoxed = xvoCreateInt(77);
	if ( vBoxed == NULL || !xrtTypedSetAddValue(pTypedSet, vBoxed) ) {
		return 90;
	}
	xvoUnref(vBoxed);
	iTypedVal = 77;
	if ( !xrtTypedSetExists(pTypedSet, &iTypedVal) || xrtTypedSetCount(pTypedSet) != 1 ) {
		return 91;
	}
	xrtTypedSetDestroy(pTypedSet);

	pTypedSet = xrtTypedSetCreate(&__xrtTestResourceType, XRT_OBJMODE_LOCAL);
	if ( pTypedSet == NULL ) {
		return 69;
	}
	__g_iXrtTestResourceDropCount = 0;
	tRes.Id = 10;
	if ( !xrtTypedSetAdd(pTypedSet, &tRes) ) {
		return 70;
	}
	tRes.Id = 20;
	if ( !xrtTypedSetAdd(pTypedSet, &tRes) ) {
		return 71;
	}
	if ( !xrtTypedSetAdd(pTypedSet, &tRes) || xrtTypedSetCount(pTypedSet) != 2 ) {
		return 72;
	}
	pRes = (XrtTestResource*)xrtTypedSetGet(pTypedSet, &tRes);
	if ( pRes == NULL || pRes->Id != 20 || xrtTypedSetItemType(pTypedSet) != &__xrtTestResourceType ) {
		return 73;
	}
	if ( !xrtTypedSetRemove(pTypedSet, &tRes) || xrtTypedSetCount(pTypedSet) != 1 ) {
		return 74;
	}
	if ( __g_iXrtTestResourceDropCount != 1 ) {
		return 75;
	}
	xrtTypedSetDestroy(pTypedSet);
	if ( __g_iXrtTestResourceDropCount != 2 ) {
		return 76;
	}

	pTypedDict = xrtTypedDictCreate(xrtTypeString(), XRT_OBJMODE_LOCAL);
	if ( pTypedDict == NULL ) {
		return 62;
	}
	sOne = "one";
	if ( xrtTypedDictSet(pTypedDict, "a", 0, &sOne) == NULL ) {
		return 63;
	}
	sTwo = "two";
	if ( xrtTypedDictSet(pTypedDict, "b", 0, &sTwo) == NULL ) {
		return 64;
	}
	ppString = (const char**)xrtTypedDictGet(pTypedDict, "a", 0);
	if ( ppString == NULL || strcmp(*ppString, "one") != 0 ) {
		return 97;
	}
	ppString = (const char**)xrtTypedDictGet(pTypedDict, "b", 0);
	if ( ppString == NULL || strcmp(*ppString, "two") != 0 || xrtTypedDictCount(pTypedDict) != 2 ) {
		return 65;
	}
	vBoxed = xvoCreateText("three", 0, FALSE);
	if ( vBoxed == NULL || !xrtTypedDictSetValue(pTypedDict, "c", 0, vBoxed) ) {
		return 87;
	}
	xvoUnref(vBoxed);
	ppString = (const char**)xrtTypedDictGet(pTypedDict, "c", 0);
	if ( ppString == NULL || strcmp(*ppString, "three") != 0 || xrtTypedDictCount(pTypedDict) != 3 ) {
		return 88;
	}
	sDictKey = NULL;
	iDictKeyLen = 0;
	ppString = (const char**)xrtTypedDictItemAt(pTypedDict, 0, &sDictKey, &iDictKeyLen);
	if ( ppString == NULL || sDictKey == NULL || iDictKeyLen != 1 ) {
		return 95;
	}
	if ( (sDictKey[0] == 'a' && strcmp(*ppString, "one") != 0) ||
	     (sDictKey[0] == 'b' && strcmp(*ppString, "two") != 0) ||
	     (sDictKey[0] == 'c' && strcmp(*ppString, "three") != 0) ||
	     (sDictKey[0] != 'a' && sDictKey[0] != 'b' && sDictKey[0] != 'c') ) {
		return 96;
	}
	if ( !xrtTypedDictExists(pTypedDict, "a", 0) || !xrtTypedDictRemove(pTypedDict, "a", 0) ) {
		return 66;
	}
	if ( xrtTypedDictCount(pTypedDict) != 2 ) {
		return 67;
	}
	xrtTypedDictDestroy(pTypedDict);

	vValueSet = xvoCreateSet();
	if ( vValueSet == NULL ) {
		return 24;
	}
	if ( !xvoSetTypeDesc(vValueSet, &__xrtTestIntSetType) ) {
		return 38;
	}
	if ( !xrtTypeSame(xvoTypeDesc(vValueSet), &__xrtTestIntSetType) ) {
		return 39;
	}
	if ( xvoSetTypeDesc(vValueSet, &__xrtTestPointType) ) {
		return 40;
	}
	vSetItem = xvoCreateInt(300);
	if ( !xvoSetAddValue(vValueSet, vSetItem, TRUE) ) {
		return 25;
	}
	vSetItem = xvoCreateInt(400);
	if ( !xvoCollSetValue(vValueSet, vSetItem, TRUE) ) {
		return 26;
	}
	vSetItem = xvoCreateInt(300);
	if ( !xvoSetExistsValue(vValueSet, vSetItem) ) {
		return 27;
	}
	xvoUnref(vSetItem);
	if ( xvoSetItemCount(vValueSet) != 2 || xvoCollItemCount(vValueSet) != 2 ) {
		return 28;
	}
	vSetAt0 = xvoSetGetValueAt(vValueSet, 0);
	vSetAt1 = xvoSetGetValueAt(vValueSet, 1);
	if ( vSetAt0 == NULL || vSetAt1 == NULL ) {
		return 33;
	}
	if ( !((xvoGetInt(vSetAt0) == 300 && xvoGetInt(vSetAt1) == 400) ||
	       (xvoGetInt(vSetAt0) == 400 && xvoGetInt(vSetAt1) == 300)) ) {
		return 34;
	}
	vSetItem = xvoCreateInt(300);
	if ( !xvoCollRemove(vValueSet, vSetItem) ) {
		return 29;
	}
	xvoUnref(vSetItem);
	if ( xvoSetItemCount(vValueSet) != 1 ) {
		return 30;
	}
	vRecordCopy = xvoCopy(vValueSet);
	if ( vRecordCopy == NULL || xvoSetItemCount(vRecordCopy) != 1 ) {
		return 35;
	}
	if ( !xrtTypeSame(xvoTypeDesc(vRecordCopy), &__xrtTestIntSetType) ) {
		return 41;
	}
	xvoUnref(vRecordCopy);
	vRecordCopy = xvoDeepCopy(vValueSet);
	if ( vRecordCopy == NULL || xvoSetItemCount(vRecordCopy) != 1 ) {
		return 42;
	}
	if ( !xrtTypeSame(xvoTypeDesc(vRecordCopy), &__xrtTestIntSetType) ) {
		return 43;
	}
	xvoUnref(vRecordCopy);
	xvoUnref(vValueSet);

	printf("Runtime Type Test : PASS\n");
	return 0;
}
