


// Runtime Type 基础类型描述
// 这一层只表达 XRT 可独立解释的运行时类型，不依赖任何 xlang 语法。

#define XRT_TYPE_NAME_LEN(s)	((uint32)(sizeof(s) - 1u))

static int __xrtTypeCompareBool(const ptr pA, const ptr pB)
{
	bool a = pA ? *(const bool*)pA : FALSE;
	bool b = pB ? *(const bool*)pB : FALSE;
	return (int)a - (int)b;
}


static uint64 __xrtTypeHashBool(const ptr pObj)
{
	bool v = pObj ? *(const bool*)pObj : FALSE;
	return v ? 0x9E3779B97F4A7C15ULL : 0xCBF29CE484222325ULL;
}


static xvalue __xrtTypeBoxBool(const ptr pObj, const xrt_type_desc* pType)
{
	(void)pType;
	return xvoCreateBool(pObj != NULL ? *(const bool*)pObj : FALSE);
}


static bool __xrtTypeUnboxBool(xvalue pVal, ptr pOut, const xrt_type_desc* pType)
{
	(void)pType;
	if ( pOut == NULL ) {
		return FALSE;
	}
	*(bool*)pOut = xvoGetBool(pVal);
	return TRUE;
}


// 复制 to_string 的返回值，并统一维护返回长度
static str __xrtTypeCopyStringResult(str sText, uint32 iSize, uint32* pSize)
{
	str sRet;

	if ( pSize != NULL ) {
		*pSize = 0;
	}
	if ( sText == NULL ) {
		return xCore.sNull;
	}
	if ( iSize == 0 ) {
		iSize = (uint32)strlen((const char*)sText);
	}
	if ( iSize == 0 ) {
		return xCore.sNull;
	}
	sRet = xrtCopyStr(sText, iSize);
	if ( sRet != xCore.sNull && pSize != NULL ) {
		*pSize = iSize;
	}
	return sRet;
}


static str __xrtTypeToStringBool(const ptr pObj, uint32* pSize)
{
	bool v = pObj ? *(const bool*)pObj : FALSE;
	return __xrtTypeCopyStringResult((str)(v ? "true" : "false"), 0, pSize);
}


static str __xrtTypeToStringInt64(const ptr pObj, uint32* pSize)
{
	char sBuffer[64];
	int iSize = xrtI64ToStr(pObj ? *(const int64*)pObj : 0, sBuffer);
	if ( iSize <= 0 ) {
		if ( pSize != NULL ) {
			*pSize = 0;
		}
		return xCore.sNull;
	}
	return __xrtTypeCopyStringResult((str)sBuffer, (uint32)iSize, pSize);
}


static str __xrtTypeToStringUInt32(const ptr pObj, uint32* pSize)
{
	int64 v = pObj ? (int64)*(const uint32*)pObj : 0;
	char sBuffer[64];
	int iSize = xrtI64ToStr(v, sBuffer);
	if ( iSize <= 0 ) {
		if ( pSize != NULL ) {
			*pSize = 0;
		}
		return xCore.sNull;
	}
	return __xrtTypeCopyStringResult((str)sBuffer, (uint32)iSize, pSize);
}


static str __xrtTypeToStringFloat64(const ptr pObj, uint32* pSize)
{
	char sBuffer[128];
	int iSize = xrtNumToStr(pObj ? *(const double*)pObj : 0.0, sBuffer);
	if ( iSize <= 0 ) {
		if ( pSize != NULL ) {
			*pSize = 0;
		}
		return xCore.sNull;
	}
	return __xrtTypeCopyStringResult((str)sBuffer, (uint32)iSize, pSize);
}


static int __xrtTypeCompareInt64(const ptr pA, const ptr pB)
{
	int64 a = pA ? *(const int64*)pA : 0;
	int64 b = pB ? *(const int64*)pB : 0;
	return (a > b) - (a < b);
}


static uint64 __xrtTypeHashInt64(const ptr pObj)
{
	int64 v = pObj ? *(const int64*)pObj : 0;
	return xrtHash64(&v, sizeof(v));
}


static xvalue __xrtTypeBoxInt64(const ptr pObj, const xrt_type_desc* pType)
{
	(void)pType;
	return xvoCreateInt(pObj != NULL ? *(const int64*)pObj : 0);
}


static bool __xrtTypeUnboxInt64(xvalue pVal, ptr pOut, const xrt_type_desc* pType)
{
	(void)pType;
	if ( pOut == NULL ) {
		return FALSE;
	}
	*(int64*)pOut = xvoGetInt(pVal);
	return TRUE;
}


static int __xrtTypeCompareUInt32(const ptr pA, const ptr pB)
{
	uint32 a = pA ? *(const uint32*)pA : 0;
	uint32 b = pB ? *(const uint32*)pB : 0;
	return (a > b) - (a < b);
}


static uint64 __xrtTypeHashUInt32(const ptr pObj)
{
	uint32 v = pObj ? *(const uint32*)pObj : 0;
	return xrtHash64(&v, sizeof(v));
}


static xvalue __xrtTypeBoxUInt32(const ptr pObj, const xrt_type_desc* pType)
{
	(void)pType;
	return xvoCreateInt(pObj != NULL ? (int64)*(const uint32*)pObj : 0);
}


static bool __xrtTypeUnboxUInt32(xvalue pVal, ptr pOut, const xrt_type_desc* pType)
{
	(void)pType;
	if ( pOut == NULL ) {
		return FALSE;
	}
	*(uint32*)pOut = (uint32)xvoGetInt(pVal);
	return TRUE;
}


static int __xrtTypeCompareFloat64(const ptr pA, const ptr pB)
{
	double a = pA ? *(const double*)pA : 0;
	double b = pB ? *(const double*)pB : 0;
	return (a > b) - (a < b);
}


static uint64 __xrtTypeHashFloat64(const ptr pObj)
{
	double v = pObj ? *(const double*)pObj : 0;
	return xrtHash64(&v, sizeof(v));
}


static xvalue __xrtTypeBoxFloat64(const ptr pObj, const xrt_type_desc* pType)
{
	(void)pType;
	return xvoCreateFloat(pObj != NULL ? *(const double*)pObj : 0.0);
}


static bool __xrtTypeUnboxFloat64(xvalue pVal, ptr pOut, const xrt_type_desc* pType)
{
	(void)pType;
	if ( pOut == NULL ) {
		return FALSE;
	}
	*(double*)pOut = xvoGetFloat(pVal);
	return TRUE;
}


static void __xrtTypeInitString(ptr pObj)
{
	if ( pObj != NULL ) {
		*(str*)pObj = xCore.sNull;
	}
}


static void __xrtTypeCopyString(ptr pDst, const ptr pSrc)
{
	str sSrc;
	if ( pDst == NULL ) {
		return;
	}
	sSrc = (pSrc != NULL) ? *(str const*)pSrc : NULL;
	if ( sSrc == NULL ) {
		sSrc = xCore.sNull;
	}
	*(str*)pDst = xrtCopyStr(sSrc, 0);
}


static void __xrtTypeDropString(ptr pObj)
{
	str sVal;
	if ( pObj == NULL ) {
		return;
	}
	sVal = *(str*)pObj;
	if ( sVal != NULL && sVal != xCore.sNull ) {
		xrtFree(sVal);
	}
	*(str*)pObj = NULL;
}


static int __xrtTypeCompareString(const ptr pA, const ptr pB)
{
	const char* a = (pA != NULL) ? *(const char* const*)pA : NULL;
	const char* b = (pB != NULL) ? *(const char* const*)pB : NULL;
	if ( a == b ) {
		return 0;
	}
	if ( a == NULL ) {
		return -1;
	}
	if ( b == NULL ) {
		return 1;
	}
	return strcmp(a, b);
}


static uint64 __xrtTypeHashString(const ptr pObj)
{
	const char* s = (pObj != NULL) ? *(const char* const*)pObj : NULL;
	if ( s == NULL ) {
		return 0;
	}
	return xrtHash64((ptr)s, strlen(s));
}


static xvalue __xrtTypeBoxString(const ptr pObj, const xrt_type_desc* pType)
{
	const char* s;
	(void)pType;
	s = (pObj != NULL) ? *(const char* const*)pObj : NULL;
	return xvoCreateText((ptr)s, 0, FALSE);
}


static str __xrtTypeToStringString(const ptr pObj, uint32* pSize)
{
	str sText = xCore.sNull;
	if ( pObj != NULL && *(str const*)pObj != NULL ) {
		sText = *(str const*)pObj;
	}
	return __xrtTypeCopyStringResult(sText, 0, pSize);
}


static bool __xrtTypeUnboxString(xvalue pVal, ptr pOut, const xrt_type_desc* pType)
{
	str sText;
	(void)pType;
	if ( pOut == NULL ) {
		return FALSE;
	}
	sText = xvoGetText(pVal);
	if ( sText == NULL ) {
		sText = xCore.sNull;
	}
	*(str*)pOut = xrtCopyStr(sText, 0);
	return TRUE;
}


static int __xrtTypeComparePtr(const ptr pA, const ptr pB)
{
	uintptr_t a = (uintptr_t)(pA ? *(const ptr*)pA : NULL);
	uintptr_t b = (uintptr_t)(pB ? *(const ptr*)pB : NULL);
	return (a > b) - (a < b);
}


static uint64 __xrtTypeHashPtr(const ptr pObj)
{
	ptr v = pObj ? *(const ptr*)pObj : NULL;
	return xrtHash64(&v, sizeof(v));
}


static xvalue __xrtTypeBoxTime(const ptr pObj, const xrt_type_desc* pType)
{
	(void)pType;
	return xvoCreateTime(pObj != NULL ? *(const xtime*)pObj : 0);
}


static bool __xrtTypeUnboxTime(xvalue pVal, ptr pOut, const xrt_type_desc* pType)
{
	(void)pType;
	if ( pOut == NULL ) {
		return FALSE;
	}
	*(xtime*)pOut = xvoGetTime(pVal);
	return TRUE;
}


static str __xrtTypeToStringTime(const ptr pObj, uint32* pSize)
{
	str sRet = xrtTimeToStr(pObj ? *(const xtime*)pObj : 0, XRT_TIME_FORMAT_DATETIME);
	if ( pSize != NULL ) {
		*pSize = (sRet != NULL && sRet != xCore.sNull) ? (uint32)strlen((const char*)sRet) : 0;
	}
	return sRet;
}


static xvalue __xrtTypeBoxPtr(const ptr pObj, const xrt_type_desc* pType)
{
	(void)pType;
	return xvoCreatePoint(pObj != NULL ? *(const ptr*)pObj : NULL);
}


static bool __xrtTypeUnboxPtr(xvalue pVal, ptr pOut, const xrt_type_desc* pType)
{
	(void)pType;
	if ( pOut == NULL ) {
		return FALSE;
	}
	*(ptr*)pOut = xvoGetPoint(pVal);
	return TRUE;
}


static str __xrtTypeToStringPtr(const ptr pObj, uint32* pSize)
{
	ptr v = pObj ? *(const ptr*)pObj : NULL;
	str sRet = xrtFormat("[point:%p]", v);
	if ( pSize != NULL ) {
		*pSize = (sRet != NULL && sRet != xCore.sNull) ? (uint32)strlen((const char*)sRet) : 0;
	}
	return sRet;
}


static void __xrtTypeInitFunction(ptr pObj)
{
	if ( pObj != NULL ) {
		*(xrt_callable**)pObj = NULL;
	}
}


static void __xrtTypeCopyFunction(ptr pDst, const ptr pSrc)
{
	xrt_callable* pCallable;
	if ( pDst == NULL ) {
		return;
	}
	pCallable = (pSrc != NULL) ? *(xrt_callable* const*)pSrc : NULL;
	*(xrt_callable**)pDst = pCallable;
	if ( pCallable != NULL ) {
		xrtCallableAddRef(pCallable);
	}
}


static void __xrtTypeDropFunction(ptr pObj)
{
	xrt_callable* pCallable;
	if ( pObj == NULL ) {
		return;
	}
	pCallable = *(xrt_callable**)pObj;
	if ( pCallable != NULL ) {
		xrtCallableUnref(pCallable);
		*(xrt_callable**)pObj = NULL;
	}
}


static int __xrtTypeCompareFunction(const ptr pA, const ptr pB)
{
	uintptr a = (uintptr)((pA != NULL) ? *(xrt_callable* const*)pA : NULL);
	uintptr b = (uintptr)((pB != NULL) ? *(xrt_callable* const*)pB : NULL);
	return (a > b) ? 1 : ((a < b) ? -1 : 0);
}


static uint64 __xrtTypeHashFunction(const ptr pObj)
{
	xrt_callable* pCallable = (pObj != NULL) ? *(xrt_callable* const*)pObj : NULL;
	return xrtHash64(&pCallable, sizeof(pCallable));
}


static str __xrtTypeToStringFunction(const ptr pObj, uint32* pSize)
{
	xrt_callable* pCallable = (pObj != NULL) ? *(xrt_callable* const*)pObj : NULL;
	str sRet = xrtFormat("[function:%p]", (ptr)pCallable);
	if ( pSize != NULL ) {
		*pSize = (sRet != NULL && sRet != xCore.sNull) ? (uint32)strlen((const char*)sRet) : 0;
	}
	return sRet;
}


static xvalue __xrtTypeBoxFunction(const ptr pObj, const xrt_type_desc* pType)
{
	xrt_callable* pCallable;
	(void)pType;
	pCallable = (pObj != NULL) ? *(xrt_callable* const*)pObj : NULL;
	if ( pCallable == NULL ) {
		return xvoCreateNull();
	}
	return xvoCreateCallable(pCallable, FALSE);
}


static bool __xrtTypeUnboxFunction(xvalue pVal, ptr pOut, const xrt_type_desc* pType)
{
	xrt_callable* pCallable;
	(void)pType;
	if ( pOut == NULL ) {
		return FALSE;
	}
	pCallable = xvoGetCallable(pVal);
	if ( pCallable == NULL ) {
		return FALSE;
	}
	*(xrt_callable**)pOut = pCallable;
	xrtCallableAddRef(pCallable);
	return TRUE;
}


#if !defined(XRT_NO_NETWORK)
static void __xrtTypeInitFuture(ptr pObj)
{
	if ( pObj != NULL ) {
		*(xfuture**)pObj = NULL;
	}
}


static void __xrtTypeCopyFuture(ptr pDst, const ptr pSrc)
{
	xfuture* pFuture;
	if ( pDst == NULL ) {
		return;
	}
	pFuture = (pSrc != NULL) ? *(xfuture* const*)pSrc : NULL;
	*(xfuture**)pDst = pFuture;
	if ( pFuture != NULL ) {
		xFutureAddRef(pFuture);
	}
}


static void __xrtTypeDropFuture(ptr pObj)
{
	xfuture* pFuture;
	if ( pObj == NULL ) {
		return;
	}
	pFuture = *(xfuture**)pObj;
	if ( pFuture != NULL ) {
		xFutureRelease(pFuture);
		*(xfuture**)pObj = NULL;
	}
}


static int __xrtTypeCompareFuture(const ptr pA, const ptr pB)
{
	uintptr a = (uintptr)((pA != NULL) ? *(xfuture* const*)pA : NULL);
	uintptr b = (uintptr)((pB != NULL) ? *(xfuture* const*)pB : NULL);
	return (a > b) ? 1 : ((a < b) ? -1 : 0);
}


static uint64 __xrtTypeHashFuture(const ptr pObj)
{
	xfuture* pFuture = (pObj != NULL) ? *(xfuture* const*)pObj : NULL;
	return xrtHash64(&pFuture, sizeof(pFuture));
}


static str __xrtTypeToStringFuture(const ptr pObj, uint32* pSize)
{
	xfuture* pFuture = (pObj != NULL) ? *(xfuture* const*)pObj : NULL;
	str sRet = xrtFormat("[future:%p]", (ptr)pFuture);
	if ( pSize != NULL ) {
		*pSize = (sRet != NULL && sRet != xCore.sNull) ? (uint32)strlen((const char*)sRet) : 0;
	}
	return sRet;
}


static xvalue __xrtTypeBoxFuture(const ptr pObj, const xrt_type_desc* pType)
{
	xfuture* pFuture;
	xvalue pVal;

	pFuture = (pObj != NULL) ? *(xfuture* const*)pObj : NULL;
	if ( pFuture == NULL ) {
		return xvoCreateNull();
	}

	xFutureAddRef(pFuture);
	pVal = xvoCreateHandle(pType, pFuture, XRT_HANDLE_FLAG_OWNED);
	if ( pVal == NULL ) {
		xFutureRelease(pFuture);
	}
	return pVal;
}


static bool __xrtTypeUnboxFuture(xvalue pVal, ptr pOut, const xrt_type_desc* pType)
{
	const xrt_type_desc* pValType;
	xfuture* pFuture;

	if ( pOut == NULL ) {
		return FALSE;
	}
	if ( pVal == NULL || xvoType(pVal) == XVO_DT_NULL ) {
		*(xfuture**)pOut = NULL;
		return TRUE;
	}

	pValType = xvoTypeDesc(pVal);
	if ( !xrtTypeSame(pValType, pType) ) {
		return FALSE;
	}
	pFuture = (xfuture*)xvoGetHandleData(pVal);
	if ( pFuture != NULL ) {
		xFutureAddRef(pFuture);
	}
	*(xfuture**)pOut = pFuture;
	return TRUE;
}
#endif


static const xrt_type_ops __xrtTypeBoolOps = {
	.init = NULL,
	.copy = NULL,
	.move = NULL,
	.drop = NULL,
	.compare = __xrtTypeCompareBool,
	.hash = __xrtTypeHashBool,
	.to_string = __xrtTypeToStringBool,
	.box = __xrtTypeBoxBool,
	.unbox = __xrtTypeUnboxBool
};

static const xrt_type_ops __xrtTypeInt64Ops = {
	.init = NULL,
	.copy = NULL,
	.move = NULL,
	.drop = NULL,
	.compare = __xrtTypeCompareInt64,
	.hash = __xrtTypeHashInt64,
	.to_string = __xrtTypeToStringInt64,
	.box = __xrtTypeBoxInt64,
	.unbox = __xrtTypeUnboxInt64
};

static const xrt_type_ops __xrtTypeUInt32Ops = {
	.init = NULL,
	.copy = NULL,
	.move = NULL,
	.drop = NULL,
	.compare = __xrtTypeCompareUInt32,
	.hash = __xrtTypeHashUInt32,
	.to_string = __xrtTypeToStringUInt32,
	.box = __xrtTypeBoxUInt32,
	.unbox = __xrtTypeUnboxUInt32
};

static const xrt_type_ops __xrtTypeFloat64Ops = {
	.init = NULL,
	.copy = NULL,
	.move = NULL,
	.drop = NULL,
	.compare = __xrtTypeCompareFloat64,
	.hash = __xrtTypeHashFloat64,
	.to_string = __xrtTypeToStringFloat64,
	.box = __xrtTypeBoxFloat64,
	.unbox = __xrtTypeUnboxFloat64
};

static const xrt_type_ops __xrtTypeStringOps = {
	.init = __xrtTypeInitString,
	.copy = __xrtTypeCopyString,
	.move = NULL,
	.drop = __xrtTypeDropString,
	.compare = __xrtTypeCompareString,
	.hash = __xrtTypeHashString,
	.to_string = __xrtTypeToStringString,
	.box = __xrtTypeBoxString,
	.unbox = __xrtTypeUnboxString
};

static const xrt_type_ops __xrtTypePtrOps = {
	.init = NULL,
	.copy = NULL,
	.move = NULL,
	.drop = NULL,
	.compare = __xrtTypeComparePtr,
	.hash = __xrtTypeHashPtr,
	.to_string = __xrtTypeToStringPtr,
	.box = __xrtTypeBoxPtr,
	.unbox = __xrtTypeUnboxPtr
};

static const xrt_type_ops __xrtTypeTimeOps = {
	.init = NULL,
	.copy = NULL,
	.move = NULL,
	.drop = NULL,
	.compare = __xrtTypeCompareInt64,
	.hash = __xrtTypeHashInt64,
	.to_string = __xrtTypeToStringTime,
	.box = __xrtTypeBoxTime,
	.unbox = __xrtTypeUnboxTime
};

static const xrt_type_ops __xrtTypeFunctionOps = {
	.init = __xrtTypeInitFunction,
	.copy = __xrtTypeCopyFunction,
	.move = NULL,
	.drop = __xrtTypeDropFunction,
	.compare = __xrtTypeCompareFunction,
	.hash = __xrtTypeHashFunction,
	.to_string = __xrtTypeToStringFunction,
	.box = __xrtTypeBoxFunction,
	.unbox = __xrtTypeUnboxFunction
};

#if !defined(XRT_NO_NETWORK)
static const xrt_type_ops __xrtTypeFutureOps = {
	.init = __xrtTypeInitFuture,
	.copy = __xrtTypeCopyFuture,
	.move = NULL,
	.drop = __xrtTypeDropFuture,
	.compare = __xrtTypeCompareFuture,
	.hash = __xrtTypeHashFuture,
	.to_string = __xrtTypeToStringFuture,
	.box = __xrtTypeBoxFuture,
	.unbox = __xrtTypeUnboxFuture
};
#endif

static const xrt_type_desc __xrtTypeString;

static const xrt_type_desc* const __xrtTypeToStringReturns[] = {
	&__xrtTypeString
};

static const xrt_func_sig __xrtTypeToStringSig = {
	.TypeId = 0,
	.Name = "toString",
	.NameSize = XRT_TYPE_NAME_LEN("toString"),
	.ParamCount = 0,
	.Params = NULL,
	.ReturnCount = 1,
	.ReturnTypes = __xrtTypeToStringReturns,
	.Flags = 0
};

static const xrt_method_desc __xrtTypeValueMethods[] = {
	{
		.Name = "toString",
		.NameSize = XRT_TYPE_NAME_LEN("toString"),
		.Sig = &__xrtTypeToStringSig,
		.NativeEntry = NULL,
		.NativeAbi = 0,
		.XCallEntry = NULL
	}
};

static const xrt_method_table __xrtTypeValueMethodTable = {
	.Count = 1,
	.Methods = __xrtTypeValueMethods
};

static const xrt_type_desc __xrtTypeNull = {
	.TypeId = XRT_TYPE_KIND_NULL,
	.Kind = XRT_TYPE_KIND_NULL,
	.Name = "null",
	.NameSize = XRT_TYPE_NAME_LEN("null"),
	.AbiName = "xvalue",
	.AbiNameSize = XRT_TYPE_NAME_LEN("xvalue"),
	.Size = 0,
	.Align = 0,
	.Ops = NULL,
	.Methods = &__xrtTypeValueMethodTable,
	.Extra = NULL
};

static const xrt_type_desc __xrtTypeBool = {
	.TypeId = XRT_TYPE_KIND_BOOL,
	.Kind = XRT_TYPE_KIND_BOOL,
	.Name = "bool",
	.NameSize = XRT_TYPE_NAME_LEN("bool"),
	.AbiName = "bool",
	.AbiNameSize = XRT_TYPE_NAME_LEN("bool"),
	.Size = sizeof(bool),
	.Align = sizeof(bool),
	.Ops = &__xrtTypeBoolOps,
	.Methods = &__xrtTypeValueMethodTable,
	.Extra = NULL
};

static const xrt_type_desc __xrtTypeInt = {
	.TypeId = XRT_TYPE_KIND_INT,
	.Kind = XRT_TYPE_KIND_INT,
	.Name = "int",
	.NameSize = XRT_TYPE_NAME_LEN("int"),
	.AbiName = "int64",
	.AbiNameSize = XRT_TYPE_NAME_LEN("int64"),
	.Size = sizeof(int64),
	.Align = sizeof(int64),
	.Ops = &__xrtTypeInt64Ops,
	.Methods = &__xrtTypeValueMethodTable,
	.Extra = NULL
};

static const xrt_type_desc __xrtTypeFloat = {
	.TypeId = XRT_TYPE_KIND_FLOAT,
	.Kind = XRT_TYPE_KIND_FLOAT,
	.Name = "float",
	.NameSize = XRT_TYPE_NAME_LEN("float"),
	.AbiName = "double",
	.AbiNameSize = XRT_TYPE_NAME_LEN("double"),
	.Size = sizeof(double),
	.Align = sizeof(double),
	.Ops = &__xrtTypeFloat64Ops,
	.Methods = &__xrtTypeValueMethodTable,
	.Extra = NULL
};

static const xrt_type_desc __xrtTypeString = {
	.TypeId = XRT_TYPE_KIND_STRING,
	.Kind = XRT_TYPE_KIND_STRING,
	.Name = "string",
	.NameSize = XRT_TYPE_NAME_LEN("string"),
	.AbiName = "str",
	.AbiNameSize = XRT_TYPE_NAME_LEN("str"),
	.Size = sizeof(str),
	.Align = sizeof(str),
	.Ops = &__xrtTypeStringOps,
	.Methods = &__xrtTypeValueMethodTable,
	.Extra = NULL
};

static const xrt_type_desc __xrtTypeTime = {
	.TypeId = XRT_TYPE_KIND_TIME,
	.Kind = XRT_TYPE_KIND_TIME,
	.Name = "time",
	.NameSize = XRT_TYPE_NAME_LEN("time"),
	.AbiName = "xtime",
	.AbiNameSize = XRT_TYPE_NAME_LEN("xtime"),
	.Size = sizeof(xtime),
	.Align = sizeof(xtime),
	.Ops = &__xrtTypeTimeOps,
	.Methods = &__xrtTypeValueMethodTable,
	.Extra = NULL
};

static const xrt_type_desc __xrtTypePoint = {
	.TypeId = XRT_TYPE_KIND_POINT,
	.Kind = XRT_TYPE_KIND_POINT,
	.Name = "point",
	.NameSize = XRT_TYPE_NAME_LEN("point"),
	.AbiName = "ptr",
	.AbiNameSize = XRT_TYPE_NAME_LEN("ptr"),
	.Size = sizeof(ptr),
	.Align = sizeof(ptr),
	.Ops = &__xrtTypePtrOps,
	.Methods = &__xrtTypeValueMethodTable,
	.Extra = NULL
};

static const xrt_type_desc __xrtTypeFunction = {
	.TypeId = XRT_TYPE_KIND_FUNCTION,
	.Kind = XRT_TYPE_KIND_FUNCTION,
	.Name = "function",
	.NameSize = XRT_TYPE_NAME_LEN("function"),
	.AbiName = "xrt_callable",
	.AbiNameSize = XRT_TYPE_NAME_LEN("xrt_callable"),
	.Size = sizeof(xrt_callable*),
	.Align = sizeof(ptr),
	.Ops = &__xrtTypeFunctionOps,
	.Methods = &__xrtTypeValueMethodTable,
	.Extra = NULL
};

#if !defined(XRT_NO_NETWORK)
static const xrt_type_desc __xrtTypeFuture = {
	.TypeId = XRT_TYPE_KIND_FUTURE,
	.Kind = XRT_TYPE_KIND_FUTURE,
	.Name = "future",
	.NameSize = XRT_TYPE_NAME_LEN("future"),
	.AbiName = "xfuture",
	.AbiNameSize = XRT_TYPE_NAME_LEN("xfuture"),
	.Size = sizeof(xfuture*),
	.Align = sizeof(ptr),
	.Ops = &__xrtTypeFutureOps,
	.Methods = &__xrtTypeValueMethodTable,
	.Extra = NULL
};
#endif

static const xrt_type_desc __xrtTypeArray = {
	.TypeId = XRT_TYPE_KIND_ARRAY,
	.Kind = XRT_TYPE_KIND_ARRAY,
	.Name = "array",
	.NameSize = XRT_TYPE_NAME_LEN("array"),
	.AbiName = "xvalue",
	.AbiNameSize = XRT_TYPE_NAME_LEN("xvalue"),
	.Size = sizeof(xvalue),
	.Align = sizeof(ptr),
	.Ops = NULL,
	.Extra = NULL
};

static const xrt_type_desc __xrtTypeList = {
	.TypeId = XRT_TYPE_KIND_LIST,
	.Kind = XRT_TYPE_KIND_LIST,
	.Name = "list",
	.NameSize = XRT_TYPE_NAME_LEN("list"),
	.AbiName = "xvalue",
	.AbiNameSize = XRT_TYPE_NAME_LEN("xvalue"),
	.Size = sizeof(xvalue),
	.Align = sizeof(ptr),
	.Ops = NULL,
	.Extra = NULL
};

static const xrt_type_desc __xrtTypeSet = {
	.TypeId = XRT_TYPE_KIND_SET,
	.Kind = XRT_TYPE_KIND_SET,
	.Name = "set",
	.NameSize = XRT_TYPE_NAME_LEN("set"),
	.AbiName = "xvalue",
	.AbiNameSize = XRT_TYPE_NAME_LEN("xvalue"),
	.Size = sizeof(xvalue),
	.Align = sizeof(ptr),
	.Ops = NULL,
	.Extra = NULL
};

static const xrt_type_desc __xrtTypeDict = {
	.TypeId = XRT_TYPE_KIND_DICT,
	.Kind = XRT_TYPE_KIND_DICT,
	.Name = "dict",
	.NameSize = XRT_TYPE_NAME_LEN("dict"),
	.AbiName = "xvalue",
	.AbiNameSize = XRT_TYPE_NAME_LEN("xvalue"),
	.Size = sizeof(xvalue),
	.Align = sizeof(ptr),
	.Ops = NULL,
	.Extra = NULL
};

static const xrt_type_desc __xrtTypeRecord = {
	.TypeId = XRT_TYPE_KIND_RECORD,
	.Kind = XRT_TYPE_KIND_RECORD,
	.Name = "record",
	.NameSize = XRT_TYPE_NAME_LEN("record"),
	.AbiName = "ptr",
	.AbiNameSize = XRT_TYPE_NAME_LEN("ptr"),
	.Size = 0,
	.Align = sizeof(ptr),
	.Ops = NULL,
	.Extra = NULL
};

static const xrt_type_desc __xrtTypeHandle = {
	.TypeId = XRT_TYPE_KIND_HANDLE,
	.Kind = XRT_TYPE_KIND_HANDLE,
	.Name = "handle",
	.NameSize = XRT_TYPE_NAME_LEN("handle"),
	.AbiName = "ptr",
	.AbiNameSize = XRT_TYPE_NAME_LEN("ptr"),
	.Size = sizeof(ptr),
	.Align = sizeof(ptr),
	.Ops = NULL,
	.Extra = NULL
};

static const xrt_type_desc __xrtTypeType = {
	.TypeId = XRT_TYPE_KIND_TYPE,
	.Kind = XRT_TYPE_KIND_TYPE,
	.Name = "type",
	.NameSize = XRT_TYPE_NAME_LEN("type"),
	.AbiName = "uint32",
	.AbiNameSize = XRT_TYPE_NAME_LEN("uint32"),
	.Size = sizeof(uint32),
	.Align = sizeof(uint32),
	.Ops = &__xrtTypeUInt32Ops,
	.Extra = NULL
};


XXAPI const xrt_type_desc* xrtTypeNull() { return &__xrtTypeNull; }
XXAPI const xrt_type_desc* xrtTypeBool() { return &__xrtTypeBool; }
XXAPI const xrt_type_desc* xrtTypeInt() { return &__xrtTypeInt; }
XXAPI const xrt_type_desc* xrtTypeFloat() { return &__xrtTypeFloat; }
XXAPI const xrt_type_desc* xrtTypeString() { return &__xrtTypeString; }
XXAPI const xrt_type_desc* xrtTypeTime() { return &__xrtTypeTime; }
XXAPI const xrt_type_desc* xrtTypePoint() { return &__xrtTypePoint; }
XXAPI const xrt_type_desc* xrtTypeFunction() { return &__xrtTypeFunction; }
#if !defined(XRT_NO_NETWORK)
XXAPI const xrt_type_desc* xrtTypeFuture() { return &__xrtTypeFuture; }
#endif
XXAPI const xrt_type_desc* xrtTypeArray() { return &__xrtTypeArray; }
XXAPI const xrt_type_desc* xrtTypeList() { return &__xrtTypeList; }
XXAPI const xrt_type_desc* xrtTypeSet() { return &__xrtTypeSet; }
XXAPI const xrt_type_desc* xrtTypeDict() { return &__xrtTypeDict; }
XXAPI const xrt_type_desc* xrtTypeRecord() { return &__xrtTypeRecord; }
XXAPI const xrt_type_desc* xrtTypeHandle() { return &__xrtTypeHandle; }
XXAPI const xrt_type_desc* xrtTypeType() { return &__xrtTypeType; }


// 判断两个类型描述是否表示同一种运行时类型
XXAPI bool xrtTypeSame(const xrt_type_desc* pA, const xrt_type_desc* pB)
{
	if ( pA == pB ) {
		return TRUE;
	}
	if ( (pA == NULL) || (pB == NULL) ) {
		return FALSE;
	}
	if ( (pA->TypeId != 0) && (pA->TypeId == pB->TypeId) ) {
		return TRUE;
	}
	if ( pA->NameSize != pB->NameSize ) {
		return FALSE;
	}
	if ( pA->Name == NULL || pB->Name == NULL ) {
		return FALSE;
	}
	return memcmp(pA->Name, pB->Name, pA->NameSize) == 0;
}


// 判断运行时类型是否等于目标类型，或由目标类型派生。
XXAPI bool xrtTypeIsA(const xrt_type_desc* pType, const xrt_type_desc* pTarget)
{
	const xrt_type_desc* pCurrent = pType;
	uint32 iDepth = 0;

	if ( pTarget == NULL ) {
		return FALSE;
	}
	while ( pCurrent != NULL && iDepth < 256u ) {
		if ( xrtTypeSame(pCurrent, pTarget) ) {
			return TRUE;
		}
		pCurrent = pCurrent->BaseType;
		iDepth++;
	}
	return FALSE;
}


// 读取类型名，便于 C 层诊断和调试
XXAPI const char* xrtTypeName(const xrt_type_desc* pType)
{
	if ( pType == NULL || pType->Name == NULL ) {
		return "unknown";
	}
	return pType->Name;
}


// 按类型描述把一个 C 对象转换成字符串；返回值遵循 XRT 字符串分配约定
XXAPI str xrtTypeToStringValue(const xrt_type_desc* pType, const ptr pObj, uint32* pSize)
{
	if ( pSize != NULL ) {
		*pSize = 0;
	}
	if ( pType == NULL || pType->Ops == NULL || pType->Ops->to_string == NULL ) {
		return xCore.sNull;
	}
	return pType->Ops->to_string(pObj, pSize);
}


// 获取泛型实参数量；非泛型类型返回 0
XXAPI uint32 xrtTypeGenericArgCount(const xrt_type_desc* pType)
{
	const xrt_generic_type* pGeneric;
	if ( pType == NULL || pType->Extra == NULL ) {
		return 0;
	}
	pGeneric = (const xrt_generic_type*)pType->Extra;
	return pGeneric->ArgCount;
}


// 获取泛型实参类型；越界或非泛型类型返回 NULL
XXAPI const xrt_type_desc* xrtTypeGenericArg(const xrt_type_desc* pType, uint32 iIndex)
{
	const xrt_generic_type* pGeneric;
	if ( pType == NULL || pType->Extra == NULL ) {
		return NULL;
	}
	pGeneric = (const xrt_generic_type*)pType->Extra;
	if ( iIndex >= pGeneric->ArgCount || pGeneric->Args == NULL ) {
		return NULL;
	}
	return pGeneric->Args[iIndex];
}


// 获取函数签名的稳定类型 id
XXAPI const xrt_method_table* xrtTypeMethods(const xrt_type_desc* pType)
{
	return pType != NULL ? pType->Methods : NULL;
}


XXAPI const xrt_method_desc* xrtTypeFindMethod(const xrt_type_desc* pType, const char* sName, uint32 iNameSize)
{
	const xrt_method_table* pTable;
	uint32 i;

	if ( pType == NULL || sName == NULL ) {
		return NULL;
	}
	if ( iNameSize == 0 ) {
		iNameSize = (uint32)strlen(sName);
	}
	pTable = xrtTypeMethods(pType);
	if ( pTable == NULL || pTable->Methods == NULL ) {
		return NULL;
	}
	for ( i = 0; i < pTable->Count; ++i ) {
		const xrt_method_desc* pMethod = &pTable->Methods[i];
		if ( pMethod->Name != NULL && pMethod->NameSize == iNameSize &&
		     memcmp(pMethod->Name, sName, iNameSize) == 0 ) {
			return pMethod;
		}
	}
	return NULL;
}


// 判断是否为 XRT 运行时基础类型
static bool __xrtTypeIsBasicScalar(const xrt_type_desc* pType)
{
	if ( pType == NULL ) {
		return FALSE;
	}
	return pType->Kind == XRT_TYPE_KIND_NULL ||
	       pType->Kind == XRT_TYPE_KIND_BOOL ||
	       pType->Kind == XRT_TYPE_KIND_INT ||
	       pType->Kind == XRT_TYPE_KIND_FLOAT ||
	       pType->Kind == XRT_TYPE_KIND_STRING ||
	       pType->Kind == XRT_TYPE_KIND_TIME ||
	       pType->Kind == XRT_TYPE_KIND_POINT;
}


static bool __xrtTypeCanExplicitConvert(const xrt_type_desc* pSrcType, const xrt_type_desc* pDstType)
{
	if ( pSrcType == NULL || pDstType == NULL ) {
		return FALSE;
	}
	if ( xrtTypeSame(pSrcType, pDstType) ) {
		return TRUE;
	}
	if ( pSrcType->Kind == XRT_TYPE_KIND_NULL ) {
		return __xrtTypeIsBasicScalar(pDstType);
	}
	if ( pDstType->Kind == XRT_TYPE_KIND_BOOL ) {
		return pSrcType->Kind == XRT_TYPE_KIND_BOOL ||
		       pSrcType->Kind == XRT_TYPE_KIND_INT ||
		       pSrcType->Kind == XRT_TYPE_KIND_FLOAT ||
		       pSrcType->Kind == XRT_TYPE_KIND_STRING ||
		       pSrcType->Kind == XRT_TYPE_KIND_TIME ||
		       pSrcType->Kind == XRT_TYPE_KIND_POINT;
	}
	if ( pDstType->Kind == XRT_TYPE_KIND_INT ) {
		return pSrcType->Kind == XRT_TYPE_KIND_BOOL ||
		       pSrcType->Kind == XRT_TYPE_KIND_INT ||
		       pSrcType->Kind == XRT_TYPE_KIND_FLOAT ||
		       pSrcType->Kind == XRT_TYPE_KIND_STRING ||
		       pSrcType->Kind == XRT_TYPE_KIND_TIME;
	}
	if ( pDstType->Kind == XRT_TYPE_KIND_FLOAT ) {
		return pSrcType->Kind == XRT_TYPE_KIND_BOOL ||
		       pSrcType->Kind == XRT_TYPE_KIND_INT ||
		       pSrcType->Kind == XRT_TYPE_KIND_FLOAT ||
		       pSrcType->Kind == XRT_TYPE_KIND_STRING ||
		       pSrcType->Kind == XRT_TYPE_KIND_TIME;
	}
	if ( pDstType->Kind == XRT_TYPE_KIND_STRING ) {
		return pSrcType->Kind == XRT_TYPE_KIND_BOOL ||
		       pSrcType->Kind == XRT_TYPE_KIND_INT ||
		       pSrcType->Kind == XRT_TYPE_KIND_FLOAT ||
		       pSrcType->Kind == XRT_TYPE_KIND_STRING ||
		       pSrcType->Kind == XRT_TYPE_KIND_TIME ||
		       pSrcType->Kind == XRT_TYPE_KIND_POINT;
	}
	if ( pDstType->Kind == XRT_TYPE_KIND_TIME ) {
		return pSrcType->Kind == XRT_TYPE_KIND_TIME ||
		       pSrcType->Kind == XRT_TYPE_KIND_STRING ||
		       pSrcType->Kind == XRT_TYPE_KIND_INT;
	}
	if ( pDstType->Kind == XRT_TYPE_KIND_POINT ) {
		return pSrcType->Kind == XRT_TYPE_KIND_POINT;
	}
	return FALSE;
}


static bool __xrtTypeReadBool(const xrt_type_desc* pType, const ptr pObj)
{
	const char* sText;
	if ( pType == NULL || pObj == NULL ) {
		return FALSE;
	}
	switch ( pType->Kind ) {
	case XRT_TYPE_KIND_BOOL:
		return *(const bool*)pObj;
	case XRT_TYPE_KIND_INT:
		return *(const int64*)pObj != 0;
	case XRT_TYPE_KIND_FLOAT:
		return *(const double*)pObj != 0.0;
	case XRT_TYPE_KIND_STRING:
		sText = *(const char* const*)pObj;
		return sText != NULL && sText[0] != '\0';
	case XRT_TYPE_KIND_TIME:
		return *(const xtime*)pObj != 0;
	case XRT_TYPE_KIND_POINT:
		return *(ptr const*)pObj != NULL;
	default:
		return FALSE;
	}
}


static int64 __xrtTypeReadInt(const xrt_type_desc* pType, const ptr pObj)
{
	const char* sText;
	if ( pType == NULL || pObj == NULL ) {
		return 0;
	}
	switch ( pType->Kind ) {
	case XRT_TYPE_KIND_BOOL:
		return *(const bool*)pObj ? 1 : 0;
	case XRT_TYPE_KIND_INT:
		return *(const int64*)pObj;
	case XRT_TYPE_KIND_FLOAT:
		return (int64)*(const double*)pObj;
	case XRT_TYPE_KIND_STRING:
		sText = *(const char* const*)pObj;
		return xrtStrToI64((str)sText);
	case XRT_TYPE_KIND_TIME:
		return (int64)*(const xtime*)pObj;
	default:
		return 0;
	}
}


static double __xrtTypeReadFloat(const xrt_type_desc* pType, const ptr pObj)
{
	const char* sText;
	if ( pType == NULL || pObj == NULL ) {
		return 0.0;
	}
	switch ( pType->Kind ) {
	case XRT_TYPE_KIND_BOOL:
		return *(const bool*)pObj ? 1.0 : 0.0;
	case XRT_TYPE_KIND_INT:
		return (double)*(const int64*)pObj;
	case XRT_TYPE_KIND_FLOAT:
		return *(const double*)pObj;
	case XRT_TYPE_KIND_STRING:
		sText = *(const char* const*)pObj;
		return xrtStrToNum((str)sText);
	case XRT_TYPE_KIND_TIME:
		return (double)*(const xtime*)pObj;
	default:
		return 0.0;
	}
}


static xtime __xrtTypeReadTime(const xrt_type_desc* pType, const ptr pObj)
{
	const char* sText;
	if ( pType == NULL || pObj == NULL ) {
		return 0;
	}
	switch ( pType->Kind ) {
	case XRT_TYPE_KIND_TIME:
		return *(const xtime*)pObj;
	case XRT_TYPE_KIND_STRING:
		sText = *(const char* const*)pObj;
		return xrtStrToTime((str)sText, sText != NULL ? strlen(sText) : 0);
	case XRT_TYPE_KIND_INT:
		return (xtime)*(const int64*)pObj;
	default:
		return 0;
	}
}


static ptr __xrtTypeReadPoint(const xrt_type_desc* pType, const ptr pObj)
{
	if ( pType == NULL || pObj == NULL || pType->Kind != XRT_TYPE_KIND_POINT ) {
		return NULL;
	}
	return *(ptr const*)pObj;
}


// 安全拓宽只描述运行时基础类型的无损方向。
// 更细的 int8/int16/float32 等静态宽度由语言编译器在类型系统层判断。
XXAPI bool xrtTypeCanWiden(const xrt_type_desc* pSrcType, const xrt_type_desc* pDstType)
{
	if ( pSrcType == NULL || pDstType == NULL ) {
		return FALSE;
	}
	if ( xrtTypeSame(pSrcType, pDstType) ) {
		return TRUE;
	}
	if ( pSrcType->Kind == XRT_TYPE_KIND_NULL && pDstType->Kind == XRT_TYPE_KIND_POINT ) {
		return TRUE;
	}
	if ( pSrcType->Kind == XRT_TYPE_KIND_BOOL &&
	     (pDstType->Kind == XRT_TYPE_KIND_INT || pDstType->Kind == XRT_TYPE_KIND_FLOAT) ) {
		return TRUE;
	}
	return FALSE;
}


XXAPI bool xrtTypeCanConvert(const xrt_type_desc* pSrcType, const xrt_type_desc* pDstType, uint32 iFlags)
{
	if ( pSrcType == NULL || pDstType == NULL ) {
		return FALSE;
	}
	if ( (iFlags & XRT_TYPE_CONVERT_EXACT) && xrtTypeSame(pSrcType, pDstType) ) {
		return TRUE;
	}
	if ( (iFlags & XRT_TYPE_CONVERT_SAFE_WIDEN) && xrtTypeCanWiden(pSrcType, pDstType) ) {
		return TRUE;
	}
	if ( (iFlags & XRT_TYPE_CONVERT_EXPLICIT) && __xrtTypeCanExplicitConvert(pSrcType, pDstType) ) {
		return TRUE;
	}
	return FALSE;
}


XXAPI bool xrtTypeConvertValue(const xrt_type_desc* pDstType, ptr pDst, const xrt_type_desc* pSrcType, const ptr pSrc, uint32 iFlags)
{
	str sText;

	if ( pDstType == NULL || pDst == NULL || pSrcType == NULL ) {
		return FALSE;
	}
	if ( !xrtTypeCanConvert(pSrcType, pDstType, iFlags) ) {
		return FALSE;
	}
	if ( xrtTypeSame(pSrcType, pDstType) ) {
		if ( pDstType->Ops != NULL && pDstType->Ops->copy != NULL ) {
			pDstType->Ops->copy(pDst, pSrc);
		} else if ( pDstType->Size > 0 && pSrc != NULL ) {
			memcpy(pDst, pSrc, pDstType->Size);
		}
		return TRUE;
	}
	switch ( pDstType->Kind ) {
	case XRT_TYPE_KIND_BOOL:
		*(bool*)pDst = __xrtTypeReadBool(pSrcType, pSrc);
		return TRUE;
	case XRT_TYPE_KIND_INT:
		*(int64*)pDst = __xrtTypeReadInt(pSrcType, pSrc);
		return TRUE;
	case XRT_TYPE_KIND_FLOAT:
		*(double*)pDst = __xrtTypeReadFloat(pSrcType, pSrc);
		return TRUE;
	case XRT_TYPE_KIND_STRING:
		if ( pSrcType->Kind == XRT_TYPE_KIND_NULL ) {
			*(str*)pDst = xCore.sNull;
			return TRUE;
		}
		sText = xrtTypeToStringValue(pSrcType, pSrc, NULL);
		*(str*)pDst = sText != NULL ? sText : xCore.sNull;
		return TRUE;
	case XRT_TYPE_KIND_TIME:
		*(xtime*)pDst = __xrtTypeReadTime(pSrcType, pSrc);
		return TRUE;
	case XRT_TYPE_KIND_POINT:
		*(ptr*)pDst = __xrtTypeReadPoint(pSrcType, pSrc);
		return TRUE;
	default:
		return FALSE;
	}
}


XXAPI bool xrtValueConvertTo(xvalue pVal, const xrt_type_desc* pDstType, ptr pDst, uint32 iFlags)
{
	const xrt_type_desc* pSrcType;
	bool bVal;
	int64 iVal;
	double fVal;
	str sVal;
	xtime tVal;
	ptr pPoint;

	if ( pDstType == NULL || pDst == NULL ) {
		return FALSE;
	}
	pSrcType = xvoTypeDesc(pVal);
	if ( !xrtTypeCanConvert(pSrcType, pDstType, iFlags) ) {
		return FALSE;
	}
	if ( xrtTypeSame(pSrcType, pDstType) ) {
		return xrtTypeUnboxValue(pDstType, pVal, pDst);
	}
	switch ( pSrcType != NULL ? pSrcType->Kind : XRT_TYPE_KIND_NULL ) {
	case XRT_TYPE_KIND_BOOL:
		bVal = xvoGetBool(pVal);
		return xrtTypeConvertValue(pDstType, pDst, pSrcType, &bVal, iFlags);
	case XRT_TYPE_KIND_INT:
		iVal = xvoGetInt(pVal);
		return xrtTypeConvertValue(pDstType, pDst, pSrcType, &iVal, iFlags);
	case XRT_TYPE_KIND_FLOAT:
		fVal = xvoGetFloat(pVal);
		return xrtTypeConvertValue(pDstType, pDst, pSrcType, &fVal, iFlags);
	case XRT_TYPE_KIND_STRING:
		sVal = xvoGetText(pVal);
		return xrtTypeConvertValue(pDstType, pDst, pSrcType, &sVal, iFlags);
	case XRT_TYPE_KIND_TIME:
		tVal = xvoGetTime(pVal);
		return xrtTypeConvertValue(pDstType, pDst, pSrcType, &tVal, iFlags);
	case XRT_TYPE_KIND_POINT:
		pPoint = xvoGetPoint(pVal);
		return xrtTypeConvertValue(pDstType, pDst, pSrcType, &pPoint, iFlags);
	case XRT_TYPE_KIND_NULL:
		return xrtTypeConvertValue(pDstType, pDst, pSrcType, NULL, iFlags);
	default:
		return FALSE;
	}
}


XXAPI xvalue xrtTypeBoxValue(const xrt_type_desc* pType, const ptr pObj)
{
	if ( pType == NULL || pType->Ops == NULL || pType->Ops->box == NULL ) {
		return NULL;
	}
	return pType->Ops->box(pObj, pType);
}


XXAPI bool xrtTypeUnboxValue(const xrt_type_desc* pType, xvalue pVal, ptr pOut)
{
	if ( pType == NULL || pType->Ops == NULL || pType->Ops->unbox == NULL || pOut == NULL ) {
		return FALSE;
	}
	return pType->Ops->unbox(pVal, pOut, pType);
}


XXAPI void xrtTypeDropValue(const xrt_type_desc* pType, ptr pObj)
{
	if ( pType == NULL || pType->Ops == NULL || pType->Ops->drop == NULL || pObj == NULL ) {
		return;
	}
	pType->Ops->drop(pObj);
}


// 按运行时类型描述在 C 对象和 xvalue 之间转换。
XXAPI uint64 xrtFuncSigTypeId(const xrt_func_sig* pSig)
{
	if ( pSig == NULL ) {
		return 0;
	}
	return pSig->TypeId;
}


// 初始化调用结果
XXAPI void xrtCallResultInit(xrt_call_result* pResult)
{
	if ( pResult ) {
		memset(pResult, 0, sizeof(*pResult));
	}
}


// 释放调用结果中持有的动态值引用
XXAPI void xrtCallResultUnit(xrt_call_result* pResult)
{
	uint32 i;
	if ( pResult == NULL ) {
		return;
	}
	for ( i = 0; i < XRT_CALL_RESULT_INLINE_COUNT; i++ ) {
		if ( pResult->Values[i] != NULL ) {
			xvoUnref(pResult->Values[i]);
			pResult->Values[i] = NULL;
		}
	}
	if ( pResult->Overflow != NULL ) {
		xvoUnref(pResult->Overflow);
		pResult->Overflow = NULL;
	}
	pResult->Count = 0;
}


// 写入调用结果；bColloc 为 TRUE 时转移 pVal 所有权
XXAPI bool xrtCallResultSetValue(xrt_call_result* pResult, uint32 iIndex, xvalue pVal, bool bColloc)
{
	if ( pResult == NULL || pVal == NULL ) {
		return FALSE;
	}
	if ( iIndex < XRT_CALL_RESULT_INLINE_COUNT ) {
		if ( pResult->Values[iIndex] != NULL ) {
			xvoUnref(pResult->Values[iIndex]);
		}
		pResult->Values[iIndex] = pVal;
		if ( !bColloc ) {
			xvoAddRef(pVal);
		}
	} else {
		uint32 iOverflowIndex = iIndex - XRT_CALL_RESULT_INLINE_COUNT;
		if ( pResult->Overflow == NULL ) {
			pResult->Overflow = xvoCreateArray();
			if ( pResult->Overflow == NULL ) {
				return FALSE;
			}
		}
		while ( xvoArrayItemCount(pResult->Overflow) <= iOverflowIndex ) {
			xvalue pNull = xvoCreateNull();
			if ( pNull == NULL ) {
				return FALSE;
			}
			if ( !xvoArrayAppendValue(pResult->Overflow, pNull, TRUE) ) {
				xvoUnref(pNull);
				return FALSE;
			}
		}
		if ( !xvoArraySetValue(pResult->Overflow, iOverflowIndex, pVal, bColloc) ) {
			return FALSE;
		}
	}
	if ( pResult->Count <= iIndex ) {
		pResult->Count = iIndex + 1;
	}
	return TRUE;
}


// 创建 callable 对象
XXAPI xrt_callable* xrtCallableCreate(const xrt_func_sig* pSig, ptr pNativeEntry, uint32 iNativeAbi, xrt_xcall_proc pXCallEntry, ptr pEnv, void (*pDropEnv)(ptr pEnv))
{
	xrt_callable* pCallable = xrtMalloc(sizeof(xrt_callable));
	if ( pCallable == NULL ) {
		return NULL;
	}
	pCallable->RefCount = 1;
	pCallable->Sig = pSig;
	pCallable->NativeEntry = pNativeEntry;
	pCallable->NativeAbi = iNativeAbi;
	pCallable->XCallEntry = pXCallEntry;
	pCallable->Env = pEnv;
	pCallable->DropEnv = pDropEnv;
	return pCallable;
}


// 获取 callable 的签名对象
XXAPI const xrt_func_sig* xrtCallableSig(const xrt_callable* pCallable)
{
	if ( pCallable == NULL ) {
		return NULL;
	}
	return pCallable->Sig;
}


// 获取 callable 对应的函数类型 id
XXAPI uint64 xrtCallableTypeId(const xrt_callable* pCallable)
{
	return xrtFuncSigTypeId(xrtCallableSig(pCallable));
}


// 增加 callable 引用
XXAPI void xrtCallableAddRef(xrt_callable* pCallable)
{
	if ( pCallable ) {
		pCallable->RefCount++;
	}
}


// 释放 callable 引用
XXAPI void xrtCallableUnref(xrt_callable* pCallable)
{
	if ( pCallable == NULL ) {
		return;
	}
	if ( pCallable->RefCount > 1 ) {
		pCallable->RefCount--;
		return;
	}
	if ( pCallable->DropEnv && pCallable->Env ) {
		pCallable->DropEnv(pCallable->Env);
	}
	xrtFree(pCallable);
}


// 通过 xcall 入口调用 callable
XXAPI bool xrtCallableInvoke(xrt_callable* pCallable, xrt_call_frame* pFrame, xrt_call_result* pResult)
{
	xrt_call_frame tFrame;
	if ( (pCallable == NULL) || (pCallable->XCallEntry == NULL) || (pResult == NULL) ) {
		return FALSE;
	}
	if ( pFrame ) {
		tFrame = *pFrame;
	} else {
		memset(&tFrame, 0, sizeof(tFrame));
	}
	if ( tFrame.Sig == NULL ) {
		tFrame.Sig = pCallable->Sig;
	}
	if ( tFrame.Env == NULL ) {
		tFrame.Env = pCallable->Env;
	}
	return pCallable->XCallEntry(&tFrame, pResult);
}

#undef XRT_TYPE_NAME_LEN
