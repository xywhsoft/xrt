#include "../test.h"



/* 返回协议测试使用的第一个稳定结果。 */
static int testEntryA(void)
{
	return 1;
}



/* 返回协议测试使用的第二个稳定结果。 */
static int testEntryB(void)
{
	return 2;
}



typedef struct testtracevalue {
	xrtobject* First;
	xrtobject* Second;
} testtracevalue;



typedef struct testtracecontext {
	xrtobject* Expected[2];
	size_t Count;
} testtracecontext;



/* 验证追踪操作按每一个强引用槽位访问对象。 */
static bool testTraceVisit(xrtobject* pObject, ptr pContext)
{
	testtracecontext* pTrace = (testtracecontext*)pContext;

	if ( (pTrace->Count >= 2u) ||
		 (pObject != pTrace->Expected[pTrace->Count]) ) {
		return false;
	}
	pTrace->Count++;
	return true;
}



/* 枚举测试值直接持有的两个可选对象引用。 */
static bool testTraceValue(
	const void* pValue,
	const xrttype* pType,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	const testtracevalue* pTrace = (const testtracevalue*)pValue;
	(void)pType;

	if ( (pTrace->First != NULL) && !pVisit(pTrace->First, pContext) ) {
		return false;
	}
	if ( (pTrace->Second != NULL) && !pVisit(pTrace->Second, pContext) ) {
		return false;
	}
	return true;
}



/* 模拟携带下层原因的类型追踪失败。 */
static bool testTraceFail(
	const void* pValue,
	const xrttype* pType,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	xerror* pError;
	(void)pValue;
	(void)pType;
	(void)pVisit;
	(void)pContext;

	pError = xrtErrorCreate(XERR_VALUE, "test.type.trace", 23,
		"the test trace rejected the value");
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
	return false;
}



/* 模拟没有发布下层错误的类型追踪失败。 */
static bool testTraceFailSilent(
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
	return false;
}



/* 构造一个由当前测试栈帧持有的类描述。 */
static xrttype testClassType(
	cstr sName,
	cstr sAbiName,
	const xrttype* pBase,
	size_t iInstanceSize,
	const xrtmethodtable* pMethods
)
{
	xrttype Type;

	memset(&Type, 0, sizeof(Type));
	Type.Id = xrtTypeId((xstrview){ sAbiName, strlen(sAbiName) });
	Type.Kind = XRT_TYPE_CLASS;
	Type.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE;
	Type.Name = (xstrview){ sName, strlen(sName) };
	Type.AbiName = (xstrview){ sAbiName, strlen(sAbiName) };
	Type.Size = sizeof(ptr);
	Type.Align = TEST_ALIGNOF(ptr);
	Type.InstanceSize = iInstanceSize;
	Type.InstanceAlign = TEST_ALIGNOF(int64);
	Type.Base = pBase;
	Type.Methods = pMethods;
	return Type;
}



/* 验证最近一次错误属于运行时类型模块的指定操作。 */
static void testTypeError(xtypeerror Code, cstr sOperation)
{
	const xerror* pError = xrtGetError();

	testRequire(pError != NULL, "runtime type error is missing");
	testRequire(
		strcmp(xrtErrorDomain(pError), "xrt.type") == 0,
		"runtime type error domain mismatch"
	);
	testRequire(xrtErrorCode(pError) == (int32)Code, "runtime type error code mismatch");
	testRequire(
		strcmp(xrtErrorOperation(pError), sOperation) == 0,
		"runtime type error operation mismatch"
	);
}



/* 验证一个内建标量的严格排序和等值散列契约。 */
static void testScalarRelation(
	const xrttype* pType,
	const void* pLow,
	const void* pHigh
)
{
	int iCompare = 0;
	uint64 iFirstHash = 0u;
	uint64 iSecondHash = 0u;

	testRequire(
		xrtTypeCompareValue(pType, pLow, pHigh, &iCompare) &&
		(iCompare < 0),
		"scalar low-to-high ordering mismatch"
	);
	testRequire(
		xrtTypeCompareValue(pType, pHigh, pLow, &iCompare) &&
		(iCompare > 0),
		"scalar high-to-low ordering mismatch"
	);
	testRequire(
		xrtTypeCompareValue(pType, pLow, pLow, &iCompare) &&
		(iCompare == 0),
		"scalar equality mismatch"
	);
	testRequire(
		xrtTypeHashValue(pType, pLow, &iFirstHash) &&
		xrtTypeHashValue(pType, pLow, &iSecondHash) &&
		(iFirstHash == iSecondHash),
		"equal scalar values produced different hashes"
	);
}



/* 验证全部内建标量宽度共享一致的比较和散列能力。 */
static void testScalarOperations(void)
{
	bool bFalse = false;
	bool bTrue = true;
	int32 iBoolFalse = 0;
	int32 iBoolTrue = -9;
	int8 iInt8Low = INT8_MIN;
	int8 iInt8High = INT8_MAX;
	uint8 iUInt8Low = 0u;
	uint8 iUInt8High = UINT8_MAX;
	int16 iInt16Low = INT16_MIN;
	int16 iInt16High = INT16_MAX;
	uint16 iUInt16Low = 0u;
	uint16 iUInt16High = UINT16_MAX;
	int32 iInt32Low = INT32_MIN;
	int32 iInt32High = INT32_MAX;
	uint32 iUInt32Low = 0u;
	uint32 iUInt32High = UINT32_MAX;
	int64 iInt64Low = INT64_MIN;
	int64 iInt64High = INT64_MAX;
	uint64 iUInt64Low = 0u;
	uint64 iUInt64High = UINT64_MAX;
	float fFloat32Low = -1.25f;
	float fFloat32High = 1.25f;
	double fFloat64Low = -1.25;
	double fFloat64High = 1.25;
	xtime TimeLow = (xtime)-1;
	xtime TimeHigh = (xtime)1;
	ptr pPointerLow = (ptr)(uintptr_t)1u;
	ptr pPointerHigh = (ptr)(uintptr_t)2u;
	uint64 iTypeLow = UINT64_C(1);
	uint64 iTypeHigh = UINT64_C(2);

	testScalarRelation(xrtTypeBool(), &bFalse, &bTrue);
	testScalarRelation(xrtTypeBool32(), &iBoolFalse, &iBoolTrue);
	testScalarRelation(xrtTypeInt8(), &iInt8Low, &iInt8High);
	testScalarRelation(xrtTypeUInt8(), &iUInt8Low, &iUInt8High);
	testScalarRelation(xrtTypeInt16(), &iInt16Low, &iInt16High);
	testScalarRelation(xrtTypeUInt16(), &iUInt16Low, &iUInt16High);
	testScalarRelation(xrtTypeInt32(), &iInt32Low, &iInt32High);
	testScalarRelation(xrtTypeUInt32(), &iUInt32Low, &iUInt32High);
	testScalarRelation(xrtTypeInt64(), &iInt64Low, &iInt64High);
	testScalarRelation(xrtTypeUInt64(), &iUInt64Low, &iUInt64High);
	testScalarRelation(xrtTypeFloat32(), &fFloat32Low, &fFloat32High);
	testScalarRelation(xrtTypeFloat64(), &fFloat64Low, &fFloat64High);
	testScalarRelation(xrtTypeTime(), &TimeLow, &TimeHigh);
	testScalarRelation(xrtTypePointer(), &pPointerLow, &pPointerHigh);
	testScalarRelation(xrtTypeType(), &iTypeLow, &iTypeHigh);
}



/* 验证内建描述的身份、布局和继承终结属性。 */
static void testBuiltins(void)
{
	const xrttype* pTypes[] = {
		xrtTypeNull(), xrtTypeBool(), xrtTypeBool32(), xrtTypeInt8(),
		xrtTypeUInt8(), xrtTypeInt16(), xrtTypeUInt16(), xrtTypeInt32(),
		xrtTypeUInt32(), xrtTypeInt64(), xrtTypeUInt64(), xrtTypeFloat32(),
		xrtTypeFloat64(), xrtTypeTime(), xrtTypePointer(), xrtTypeType()
	};

	for ( size_t i = 0; i < sizeof(pTypes) / sizeof(pTypes[0]); i++ ) {
		testRequire(xrtTypeValidate(pTypes[i]), "builtin type validation failed");
		testRequire(
			pTypes[i]->Id == xrtTypeId(pTypes[i]->AbiName),
			"builtin type id mismatch"
		);
		testRequire(
			xrtTypeIsCopyable(pTypes[i]) &&
			xrtTypeIsRelocatable(pTypes[i]) &&
			xrtTypeIsComparable(pTypes[i]) &&
			xrtTypeIsHashable(pTypes[i]),
			"builtin type capability mismatch"
		);
	}
	testRequire(
		xrtTypeSame(xrtTypeInt64(), xrtTypeInt64()),
		"same builtin type mismatch"
	);
	testRequire(
		!xrtTypeSame(xrtTypeInt64(), xrtTypeUInt64()),
		"different builtin types compare equal"
	);
	testRequire(
		!xrtTypeSame(xrtTypeBool(), xrtTypeBool32()) &&
		(xrtTypeBool32()->Size == sizeof(int32)) &&
		(xrtTypeBool32()->Align == TEST_ALIGNOF(int32)),
		"bool32 ABI descriptor mismatch"
	);
}



/* 验证描述符、签名、方法和继承链的拒绝边界。 */
static void testDescriptorValidation(void)
{
	xrttype Base = testClassType(
		"Shape", "tests.runtime.Shape", NULL, sizeof(int64), NULL
	);
	xrttype Bad = Base;
	xrttype Derived;
	xrtparamdesc Param = {
		XRT_STR_INIT("value"), xrtTypeInt64(), XRT_PARAM_DEFAULT, 0u
	};
	xrtfunctionsig Signature = {
		.Name = XRT_STR_INIT("shape"),
		.ParamCount = 1u,
		.Params = &Param
	};
	xrtmethoddesc Method = {
		XRT_STR_INIT("shape"), &Signature,
		(ptr)(uintptr_t)&testEntryA, 0u
	};
	xrtmethodtable Methods = { 1u, &Method };

	Bad.Id ^= UINT64_C(1);
	xrtClearError();
	testRequire(!xrtTypeValidate(&Bad), "noncanonical type ID was accepted");
	testTypeError(XTYPE_ERROR_DESCRIPTOR, "validate");

	Bad = Base;
	Bad.Flags |= UINT32_C(0x80000000);
	testRequire(!xrtTypeValidate(&Bad), "unknown type flag was accepted");

	Bad = Base;
	Bad.Flags |= XRT_TYPE_FLAG_COPYABLE;
	testRequire(!xrtTypeValidate(&Bad), "copyable type without copy path was accepted");

	Bad = Base;
	Bad.Flags |= XRT_TYPE_FLAG_TRIVIAL_COPY;
	testRequire(!xrtTypeValidate(&Bad), "trivial copy without copyable flag was accepted");

	Bad = *xrtTypeInt64();
	Bad.Size = 3u;
	Bad.Align = 1u;
	testRequire(!xrtTypeValidate(&Bad), "unsupported scalar width was accepted");
	testRequire(
		!xrtTypeIsComparable(&Bad) && !xrtTypeIsHashable(&Bad),
		"malformed scalar reported comparison or hash capability"
	);

	Base.Flags |= XRT_TYPE_FLAG_FINAL;
	Derived = testClassType(
		"DerivedFinal", "tests.runtime.DerivedFinal",
		&Base, sizeof(int64) * 2u, NULL
	);
	testRequire(!xrtTypeValidate(&Derived), "derivation from final type was accepted");

	Base.Flags &= ~XRT_TYPE_FLAG_FINAL;
	Derived = testClassType(
		"DerivedSmall", "tests.runtime.DerivedSmall",
		&Base, sizeof(int32), NULL
	);
	testRequire(!xrtTypeValidate(&Derived), "undersized derived payload was accepted");

	Bad = Base;
	Bad.Base = &Bad;
	testRequire(!xrtTypeValidate(&Bad), "cyclic inheritance was accepted");

	Signature.Flags = UINT32_C(0x80000000);
	xrtClearError();
	testRequire(xrtFunctionSigId(&Signature) == 0u, "unknown signature flag was accepted");
	testTypeError(XTYPE_ERROR_SIGNATURE, "signature-id");

	Signature.Flags = 0u;
	Param.Flags = UINT32_C(0x80000000);
	testRequire(xrtFunctionSigId(&Signature) == 0u, "unknown parameter flag was accepted");

	Param.Flags = XRT_PARAM_FLAG_NAMED_ONLY;
	Param.Name.Data = NULL;
	Param.Name.Size = 0u;
	testRequire(xrtFunctionSigId(&Signature) == 0u, "unnamed named-only parameter was accepted");

	Param.Flags = 0u;
	Param.Name = XRT_STR_LITERAL("value");
	Method.Flags = UINT32_C(0x80000000);
	Bad = Base;
	Bad.Methods = &Methods;
	testRequire(!xrtTypeValidate(&Bad), "unknown method flag was accepted");
}



/* 验证内建生命周期、比较、散列和失败时输出不变。 */
static void testValueOperations(void)
{
	int64 iSource = 37;
	int64 iTarget = 9;
	int iCompare = 99;
	uint64 iHash = UINT64_C(99);
	double fPositiveZero = 0.0;
	double fNegativeZero = -0.0;
	int32 iBoolTrue = 1;
	int32 iBoolAlternate = -7;
	int32 iBoolFalse = 0;
	uint64 iPositiveHash;
	uint64 iNegativeHash;
	uint64 iBoolHash;
	uint64 iBoolAlternateHash;
	xrttype Unsupported = testClassType(
		"Unsupported", "tests.runtime.Unsupported", NULL,
		sizeof(int64), NULL
	);
	xrttype ValueOnly = *xrtTypePointer();

	ValueOnly.InstanceSize = 0u;
	ValueOnly.InstanceAlign = 1u;
	testRequire(
		xrtTypeValidate(&ValueOnly),
		"value-only type descriptor is invalid"
	);

	testRequire(xrtTypeInitValue(xrtTypeInt64(), &iTarget), "integer init failed");
	testRequire(iTarget == 0, "integer init did not clear value");
	testRequire(
		xrtTypeCopyValue(xrtTypeInt64(), &iTarget, &iSource) &&
		(iTarget == iSource),
		"integer copy failed"
	);
	testRequire(
		xrtTypeCopyValue(xrtTypeInt64(), &iSource, &iSource) &&
		(iSource == 37),
		"self copy modified value"
	);
	testRequire(
		xrtTypeMoveValue(xrtTypeInt64(), &iTarget, &iSource) &&
		(iTarget == 37) && (iSource == 0),
		"integer move failed"
	);
	iTarget = 41;
	testRequire(
		xrtTypeMoveValue(xrtTypeInt64(), &iTarget, &iTarget) &&
		(iTarget == 41),
		"self move modified value"
	);
	xrtClearError();
	testRequire(
		!xrtTypeCloneValue(xrtTypeInt64(), &iTarget, &iTarget) &&
		(iTarget == 41),
		"self clone was accepted"
	);
	testTypeError(XTYPE_ERROR_OPERATION, "clone");

	iSource = -8;
	iTarget = 12;
	testRequire(
		xrtTypeCompareValue(xrtTypeInt64(), &iSource, &iTarget, &iCompare) &&
		(iCompare < 0),
		"integer comparison failed"
	);
	testRequire(
		xrtTypeHashValue(xrtTypeInt64(), &iSource, &iHash),
		"integer hash failed"
	);
	testRequire(
		xrtTypeCompareValue(
			xrtTypeFloat64(), &fPositiveZero, &fNegativeZero, &iCompare
		) && (iCompare == 0),
		"floating signed zero comparison mismatch"
	);
	testRequire(
		xrtTypeHashValue(xrtTypeFloat64(), &fPositiveZero, &iPositiveHash) &&
		xrtTypeHashValue(xrtTypeFloat64(), &fNegativeZero, &iNegativeHash) &&
		(iPositiveHash == iNegativeHash),
		"floating signed zero hash mismatch"
	);
	testRequire(
		xrtTypeCompareValue(
			xrtTypeBool32(), &iBoolTrue, &iBoolAlternate, &iCompare
		) && (iCompare == 0),
		"bool32 did not normalize nonzero values"
	);
	testRequire(
		xrtTypeCompareValue(
			xrtTypeBool32(), &iBoolFalse, &iBoolTrue, &iCompare
		) && (iCompare < 0),
		"bool32 false ordering mismatch"
	);
	testRequire(
		xrtTypeHashValue(xrtTypeBool32(), &iBoolTrue, &iBoolHash) &&
		xrtTypeHashValue(
			xrtTypeBool32(), &iBoolAlternate, &iBoolAlternateHash
		) && (iBoolHash == iBoolAlternateHash),
		"bool32 equal values produced different hashes"
	);
	testRequire(
		xrtTypeCompareValue(xrtTypeNull(), NULL, NULL, &iCompare) &&
		(iCompare == 0) &&
		xrtTypeHashValue(xrtTypeNull(), NULL, &iHash),
		"null comparison or hash failed"
	);

	iCompare = 71;
	iHash = UINT64_C(71);
	xrtClearError();
	testRequire(
		!xrtTypeCompareValue(&ValueOnly, NULL, &iTarget, &iCompare) &&
		(iCompare == 71) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"comparison ignored the ABI value size"
	);
	xrtClearError();
	testRequire(
		!xrtTypeHashValue(&ValueOnly, NULL, &iHash) &&
		(iHash == UINT64_C(71)) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"hash ignored the ABI value size"
	);

	iCompare = 73;
	iHash = UINT64_C(73);
	xrtClearError();
	testRequire(
		!xrtTypeCompareValue(&Unsupported, &iSource, &iTarget, &iCompare) &&
		(iCompare == 73),
		"unsupported comparison changed output"
	);
	testTypeError(XTYPE_ERROR_OPERATION, "compare");
	xrtClearError();
	testRequire(
		!xrtTypeHashValue(&Unsupported, &iSource, &iHash) &&
		(iHash == UINT64_C(73)),
		"unsupported hash changed output"
	);
	testTypeError(XTYPE_ERROR_OPERATION, "hash");
}



/* 验证值存储和对象实例负载使用彼此独立的大小与操作表。 */
static void testValueInstanceBoundary(void)
{
	xrttype Type = testClassType(
		"Boundary", "tests.runtime.Boundary", NULL, 32u, NULL
	);
	uint8 arrValue[sizeof(ptr) + 8u];
	uint8 arrInstance[32u];

	memset(arrValue, 0xA5, sizeof(arrValue));
	memset(arrInstance, 0x5A, sizeof(arrInstance));
	testRequire(
		xrtTypeInitValue(&Type, arrValue),
		"reference value initialization failed"
	);
	for ( size_t i = 0; i < sizeof(ptr); i++ ) {
		testRequire(arrValue[i] == 0u, "reference value slot was not cleared");
	}
	for ( size_t i = sizeof(ptr); i < sizeof(arrValue); i++ ) {
		testRequire(
			arrValue[i] == UINT8_C(0xA5),
			"value initialization crossed the ABI value boundary"
		);
	}
	testRequire(
		xrtTypeInitInstance(&Type, arrInstance),
		"object instance initialization failed"
	);
	for ( size_t i = 0; i < sizeof(arrInstance); i++ ) {
		testRequire(arrInstance[i] == 0u, "instance payload was not cleared");
	}
}



/* 验证强引用追踪、空操作和失败原因链契约。 */
static void testValueTrace(void)
{
	static const xrttypeops TraceOps = {
		.Trace = testTraceValue
	};
	static const xrttypeops FailOps = {
		.Trace = testTraceFail
	};
	static const xrttypeops SilentOps = {
		.Trace = testTraceFailSilent
	};
	xrttype Type = testClassType(
		"Trace", "tests.runtime.Trace", NULL, sizeof(testtracevalue), NULL
	);
	xrttype Plain = testClassType(
		"Plain", "tests.runtime.Plain", NULL, sizeof(testtracevalue), NULL
	);
	testtracevalue Value = {
		(xrtobject*)(uintptr_t)UINT32_C(0x1000),
		(xrtobject*)(uintptr_t)UINT32_C(0x2000)
	};
	testtracecontext Context = {
		{
			(xrtobject*)(uintptr_t)UINT32_C(0x1000),
			(xrtobject*)(uintptr_t)UINT32_C(0x2000)
		},
		0u
	};
	const xerror* pError;

	Type.Kind = XRT_TYPE_RECORD;
	Type.Flags = 0u;
	Type.Size = sizeof(testtracevalue);
	Type.Align = TEST_ALIGNOF(testtracevalue);
	Plain.Kind = XRT_TYPE_RECORD;
	Plain.Flags = 0u;
	Plain.Size = sizeof(testtracevalue);
	Plain.Align = TEST_ALIGNOF(testtracevalue);
	Type.Ops = &TraceOps;
	testRequire(
		xrtTypeTraceValue(&Type, &Value, testTraceVisit, &Context) &&
		(Context.Count == 2u),
		"runtime type trace result mismatch"
	);
	Context.Count = 0u;
	testRequire(
		xrtTypeTraceValue(&Plain, &Value, testTraceVisit, &Context) &&
		(Context.Count == 0u),
		"runtime type empty trace was not a no-op"
	);

	xrtClearError();
	testRequire(
		!xrtTypeTraceValue(&Type, &Value, NULL, &Context),
		"runtime type trace accepted a null visitor"
	);
	testTypeError(XTYPE_ERROR_OPERATION, "trace");

	Type.Ops = &FailOps;
	xrtClearError();
	testRequire(
		!xrtTypeTraceValue(&Type, &Value, testTraceVisit, &Context),
		"runtime type trace callback failure was ignored"
	);
	pError = xrtGetError();
	testTypeError(XTYPE_ERROR_OPERATION, "trace");
	testRequire(
		(xrtErrorCause(pError) != NULL) &&
		(strcmp(xrtErrorDomain(xrtErrorCause(pError)), "test.type.trace") == 0),
		"runtime type trace cause mismatch"
	);

	Type.Ops = &SilentOps;
	xrtClearError();
	testRequire(
		!xrtTypeTraceValue(&Type, &Value, testTraceVisit, &Context),
		"silent runtime type trace failure was ignored"
	);
	testTypeError(XTYPE_ERROR_OPERATION, "trace");
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		(xrtErrorCause(xrtGetError()) == NULL),
		"silent runtime type trace failure fallback mismatch"
	);
}



/* 验证实例追踪独立使用 InstanceOps 且保留回调错误原因。 */
static void testInstanceTrace(void)
{
	static const xrtinstanceops TraceOps = {
		.Trace = testTraceValue
	};
	static const xrtinstanceops FailOps = {
		.Trace = testTraceFail
	};
	xrttype Type = testClassType(
		"InstanceTrace", "tests.runtime.InstanceTrace",
		NULL, sizeof(testtracevalue), NULL
	);
	testtracevalue Value = {
		(xrtobject*)(uintptr_t)UINT32_C(0x3000),
		(xrtobject*)(uintptr_t)UINT32_C(0x4000)
	};
	testtracecontext Context = {
		{
			(xrtobject*)(uintptr_t)UINT32_C(0x3000),
			(xrtobject*)(uintptr_t)UINT32_C(0x4000)
		},
		0u
	};
	const xerror* pError;

	Type.InstanceOps = &TraceOps;
	testRequire(
		xrtTypeTraceInstance(&Type, &Value, testTraceVisit, &Context) &&
		(Context.Count == 2u),
		"runtime instance trace result mismatch"
	);
	Type.InstanceOps = &FailOps;
	xrtClearError();
	testRequire(
		!xrtTypeTraceInstance(&Type, &Value, testTraceVisit, &Context),
		"runtime instance trace callback failure was ignored"
	);
	pError = xrtGetError();
	testTypeError(XTYPE_ERROR_OPERATION, "instance-trace");
	testRequire(
		(xrtErrorCause(pError) != NULL) &&
		(strcmp(xrtErrorDomain(xrtErrorCause(pError)), "test.type.trace") == 0),
		"runtime instance trace cause mismatch"
	);
}



/* 验证排序注册表的幂等注册、冲突、查询和移除语义。 */
static void testRegistryAndInheritance(void)
{
	xrttype Base = testClassType(
		"Base", "tests.runtime.Base", NULL, sizeof(int64), NULL
	);
	xrttype Derived = testClassType(
		"Derived", "tests.runtime.Derived", &Base, sizeof(int64) * 2u, NULL
	);
	const xrttype* arrArguments[] = { xrtTypeInt64() };
	xrttype Array = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.runtime.array<int64>")),
		.Kind = XRT_TYPE_ARRAY,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("array<int64>"),
		.AbiName = XRT_STR_INIT("tests.runtime.array<int64>"),
		.Size = sizeof(ptr),
		.Align = TEST_ALIGNOF(ptr),
		.InstanceSize = sizeof(ptr),
		.InstanceAlign = TEST_ALIGNOF(ptr),
		.ArgumentCount = 1,
		.Arguments = arrArguments
	};
	xrttyperegistry* pRegistry = xrtTypeRegistryCreate();
	xrttype Alias = Base;
	const xrttype* pFirst;
	const xrttype* pSecond;
	const xrttype* pThird;

	testRequire(
		(pRegistry != NULL) &&
		xrtTypeValidate(&Base) &&
		xrtTypeValidate(&Derived) &&
		xrtTypeValidate(&Array),
		"test type validation failed"
	);
	testRequire(xrtTypeIsA(&Derived, &Base), "derived type relation missing");
	testRequire(!xrtTypeIsA(&Base, &Derived), "base type relation reversed");
	testRequire(
		xrtTypeArgument(&Array, 0) == xrtTypeInt64(),
		"generic argument mismatch"
	);
	testRequire(
		xrtTypeRegistryAdd(pRegistry, &Base) &&
		xrtTypeRegistryAdd(pRegistry, &Derived) &&
		xrtTypeRegistryAdd(pRegistry, &Array) &&
		xrtTypeRegistryAdd(pRegistry, &Base),
		"type registry add failed"
	);
	testRequire(xrtTypeRegistryCount(pRegistry) == 3u, "registry count mismatch");
	pFirst = xrtTypeRegistryAt(pRegistry, 0u);
	pSecond = xrtTypeRegistryAt(pRegistry, 1u);
	pThird = xrtTypeRegistryAt(pRegistry, 2u);
	testRequire(
		(pFirst != NULL) &&
		(pSecond != NULL) &&
		(pThird != NULL) &&
		(pFirst->Id < pSecond->Id) &&
		(pSecond->Id < pThird->Id),
		"registry ordered enumeration mismatch"
	);
	xrtClearError();
	testRequire(
		xrtTypeRegistryAt(pRegistry, 3u) == NULL,
		"registry enumeration accepted an out-of-range index"
	);
	testTypeError(XTYPE_ERROR_REGISTRY, "registry-at");
	testRequire(
		xrtTypeRegistryFindId(pRegistry, Derived.Id) == &Derived,
		"registry id lookup mismatch"
	);
	testRequire(
		xrtTypeRegistryFindName(pRegistry, Array.AbiName) == &Array,
		"registry name lookup mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtTypeRegistryAdd(pRegistry, &Alias),
		"second descriptor for one stable type was accepted"
	);
	testTypeError(XTYPE_ERROR_REGISTRY, "registry-add");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_EXISTS, "registry collision kind mismatch");
	testRequire(
		xrtTypeRegistryRemove(pRegistry, &Derived) &&
		(xrtTypeRegistryFindId(pRegistry, Derived.Id) == NULL) &&
		(xrtTypeRegistryCount(pRegistry) == 2u),
		"registry remove failed"
	);
	xrtClearError();
	testRequire(
		!xrtTypeRegistryRemove(pRegistry, &Derived) &&
		(xrtGetError() == NULL),
		"missing registry removal reported an error"
	);
	xrtTypeRegistryDestroy(pRegistry);
	xrtClearError();
	testRequire(
		xrtTypeRegistryAt(NULL, 0u) == NULL,
		"registry enumeration accepted a null registry"
	);
	testTypeError(XTYPE_ERROR_REGISTRY, "registry-at");
}



/* 验证函数签名、方法继承和协议见证注册表。 */
static void testFunctionMethodsAndProtocol(void)
{
	xrtparamdesc arrParams[] = {
		{ XRT_STR_INIT("value"), xrtTypeInt64(), XRT_PARAM_DEFAULT, 0 }
	};
	const xrttype* arrReturns[] = { xrtTypeBool() };
	xrtfunctionsig Signature = {
		.Name = XRT_STR_INIT("contains"),
		.ParamCount = 1,
		.Params = arrParams,
		.ReturnCount = 1,
		.ReturnTypes = arrReturns
	};
	xrtfunctionsig Renamed = Signature;
	xrtparamdesc RenamedParam = arrParams[0];
	xrtfunctionsig ParamRenamed = Signature;
	xrtfunctionsig WithMetadata = Signature;
	xrtmethoddesc arrMethods[] = {
		{
			XRT_STR_INIT("contains"),
			&Signature,
			(ptr)(uintptr_t)&testEntryA,
			0
		}
	};
	xrtmethodtable Methods = {
		sizeof(arrMethods) / sizeof(arrMethods[0]),
		arrMethods
	};
	xrttype Concrete = testClassType(
		"Bag", "tests.runtime.Bag", NULL, sizeof(int64), &Methods
	);
	xrttype Concrete2 = testClassType(
		"Box", "tests.runtime.Box", NULL, sizeof(int64), &Methods
	);
	xrttype Derived = testClassType(
		"DerivedBag", "tests.runtime.DerivedBag",
		&Concrete, sizeof(int64) * 2u, NULL
	);
	xrttype ProtocolType = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.runtime.Container")),
		.Kind = XRT_TYPE_PROTOCOL,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("Container"),
		.AbiName = XRT_STR_INIT("tests.runtime.Container"),
		.Size = sizeof(ptr),
		.Align = TEST_ALIGNOF(ptr),
		.InstanceSize = sizeof(ptr),
		.InstanceAlign = TEST_ALIGNOF(ptr)
	};
	xrtprotocolrequirement arrRequirements[] = {
		{ XRT_STR_INIT("contains"), &Signature }
	};
	xrtprotocol Protocol = {
		&ProtocolType,
		sizeof(arrRequirements) / sizeof(arrRequirements[0]),
		arrRequirements
	};
	xrtprotocolrequirement arrDuplicateRequirements[] = {
		{ XRT_STR_INIT("contains"), &Signature },
		{ XRT_STR_INIT("contains"), &Signature }
	};
	xrtprotocol DuplicateProtocol = {
		&ProtocolType,
		sizeof(arrDuplicateRequirements) / sizeof(arrDuplicateRequirements[0]),
		arrDuplicateRequirements
	};
	xrtprotocolentry arrEntries[] = {
		{
			XRT_STR_INIT("contains"),
			&Signature,
			(ptr)(uintptr_t)&testEntryB
		}
	};
	xrtprotocolwitness Witness = {
		&Protocol,
		&Concrete,
		sizeof(arrEntries) / sizeof(arrEntries[0]),
		arrEntries
	};
	xrtprotocolwitness Witness2 = {
		&Protocol,
		&Concrete2,
		sizeof(arrEntries) / sizeof(arrEntries[0]),
		arrEntries
	};
	xrtprotocolwitness Alias = Witness;
	xrtprotocolentry BadEntry = arrEntries[0];
	xrtprotocolwitness BadWitness = Witness;
	xrtprotocolregistry* pRegistry;
	const xrtprotocolwitness* pFirstWitness;
	const xrtprotocolwitness* pSecondWitness;
	uint64 iSignatureId;

	Renamed.Name = XRT_STR_LITERAL("has");
	RenamedParam.Name = XRT_STR_LITERAL("item");
	ParamRenamed.Params = &RenamedParam;
	iSignatureId = xrtFunctionSigId(&Signature);
	testRequire(iSignatureId != 0, "function signature id missing");
	WithMetadata.UserTag = UINT64_C(0x584C414E47544553);
	WithMetadata.UserData = &Concrete;
	testRequire(
		xrtFunctionSigValidate(&WithMetadata) &&
		(iSignatureId == xrtFunctionSigId(&WithMetadata)),
		"function signature metadata polluted identity"
	);
	testRequire(
		iSignatureId == xrtFunctionSigId(&Renamed),
		"function name polluted signature identity"
	);
	testRequire(
		iSignatureId != xrtFunctionSigId(&ParamRenamed),
		"named positional parameter was omitted from signature identity"
	);
	arrParams[0].Flags = XRT_PARAM_FLAG_NAMED_ONLY;
	RenamedParam.Flags = XRT_PARAM_FLAG_NAMED_ONLY;
	testRequire(
		xrtFunctionSigId(&Signature) != xrtFunctionSigId(&ParamRenamed),
		"named-only parameter name was omitted from signature identity"
	);
	arrParams[0].Flags = 0u;
	RenamedParam.Flags = 0u;
	testRequire(
		xrtTypeFindMethod(
			&Concrete,
			XRT_STR_LITERAL("contains"),
			iSignatureId
		) == &arrMethods[0],
		"method lookup mismatch"
	);
	testRequire(
		xrtTypeFindMethod(
			&Derived,
			XRT_STR_LITERAL("contains"),
			iSignatureId
		) == &arrMethods[0],
		"inherited method lookup mismatch"
	);
	testRequire(
		xrtTypeFindMethod(
			&Derived,
			XRT_STR_LITERAL("missing"),
			0
		) == NULL,
		"missing inherited method was reported"
	);
	testRequire(
		xrtProtocolValidate(&Protocol),
		"protocol validation failed"
	);
	BadEntry.Name.Data = NULL;
	BadEntry.Name.Size = arrEntries[0].Name.Size;
	BadWitness.Entries = &BadEntry;
	xrtClearError();
	testRequire(
		!xrtProtocolWitnessValidate(&BadWitness),
		"malformed protocol witness entry was accepted"
	);
	testTypeError(XTYPE_ERROR_PROTOCOL, "witness-validate");
	xrtClearError();
	testRequire(
		xrtProtocolWitnessFind(
			&BadWitness,
			XRT_STR_LITERAL("contains"),
			iSignatureId
		) == NULL,
		"malformed protocol witness entry was searchable"
	);
	testTypeError(XTYPE_ERROR_PROTOCOL, "witness-find");
	xrtClearError();
	testRequire(
		!xrtProtocolValidate(&DuplicateProtocol),
		"duplicate protocol requirement was accepted"
	);
	testTypeError(XTYPE_ERROR_PROTOCOL, "protocol-validate");

	testRequire(
		xrtProtocolWitnessValidate(&Witness),
		"protocol witness validation failed"
	);
	testRequire(
		xrtProtocolWitnessFind(
			&Witness,
			XRT_STR_LITERAL("contains"),
			iSignatureId
		) == &arrEntries[0],
		"protocol witness lookup mismatch"
	);
	pRegistry = xrtProtocolRegistryCreate();
	testRequire(
		pRegistry != NULL &&
		xrtProtocolRegistryAdd(pRegistry, &Witness2) &&
		xrtProtocolRegistryAdd(pRegistry, &Witness) &&
		xrtProtocolRegistryAdd(pRegistry, &Witness),
		"protocol registry add failed"
	);
	testRequire(
		xrtProtocolRegistryCount(pRegistry) == 2u &&
		xrtProtocolRegistryFind(
			pRegistry,
			ProtocolType.Id,
			Concrete.Id
		) == &Witness,
		"protocol registry lookup mismatch"
	);
	pFirstWitness = xrtProtocolRegistryAt(pRegistry, 0u);
	pSecondWitness = xrtProtocolRegistryAt(pRegistry, 1u);
	testRequire(
		(pFirstWitness != NULL) &&
		(pSecondWitness != NULL) &&
		(pFirstWitness != pSecondWitness),
		"protocol registry ordered enumeration mismatch"
	);
	xrtClearError();
	testRequire(
		xrtProtocolRegistryAt(pRegistry, 2u) == NULL,
		"protocol registry enumeration accepted an out-of-range index"
	);
	testTypeError(XTYPE_ERROR_REGISTRY, "protocol-at");
	xrtClearError();
	testRequire(
		!xrtProtocolRegistryAdd(pRegistry, &Alias),
		"second witness for one protocol pair was accepted"
	);
	testTypeError(XTYPE_ERROR_REGISTRY, "protocol-add");
	testRequire(
		xrtProtocolRegistryRemove(pRegistry, &Witness) &&
		xrtProtocolRegistryRemove(pRegistry, &Witness2) &&
		xrtProtocolRegistryCount(pRegistry) == 0u,
		"protocol registry remove failed"
	);
	xrtClearError();
	testRequire(
		!xrtProtocolRegistryRemove(pRegistry, &Witness) &&
		(xrtGetError() == NULL),
		"missing protocol removal reported an error"
	);
	xrtProtocolRegistryDestroy(pRegistry);
	xrtClearError();
	testRequire(
		xrtProtocolRegistryAt(NULL, 0u) == NULL,
		"protocol registry enumeration accepted a null registry"
	);
	testTypeError(XTYPE_ERROR_REGISTRY, "protocol-at");
}



/* 验证枚举查询以及名称、标签和存储描述边界。 */
static void testEnum(void)
{
	xrttype EnumType = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.runtime.Result")),
		.Kind = XRT_TYPE_ENUM,
		.Flags = XRT_TYPE_FLAG_TRIVIAL_COPY |
			XRT_TYPE_FLAG_TRIVIAL_DROP | XRT_TYPE_FLAG_COPYABLE,
		.Name = XRT_STR_INIT("Result"),
		.AbiName = XRT_STR_INIT("tests.runtime.Result"),
		.Size = sizeof(int64) + sizeof(ptr),
		.Align = TEST_ALIGNOF(ptr),
		.InstanceSize = sizeof(int64) + sizeof(ptr),
		.InstanceAlign = TEST_ALIGNOF(ptr)
	};
	xrtenumvariant arrVariants[] = {
		{ XRT_STR_INIT("ok"), 0, xrtTypeInt64() },
		{ XRT_STR_INIT("error"), 1, xrtTypePointer() }
	};
	xrtenum Enum = {
		&EnumType,
		sizeof(arrVariants) / sizeof(arrVariants[0]),
		arrVariants
	};

	testRequire(xrtEnumValidate(&Enum), "enum validation failed");
	testRequire(
		xrtEnumFindTag(&Enum, 1) == &arrVariants[1],
		"enum tag lookup mismatch"
	);
	testRequire(
		xrtEnumFindName(&Enum, XRT_STR_LITERAL("ok")) == &arrVariants[0],
		"enum name lookup mismatch"
	);
	arrVariants[1].Tag = 0;
	xrtClearError();
	testRequire(!xrtEnumValidate(&Enum), "duplicate enum tag was accepted");
	testTypeError(XTYPE_ERROR_ENUM, "enum-validate");
	arrVariants[1].Tag = 1;
	arrVariants[1].Name = XRT_STR_LITERAL("ok");
	testRequire(!xrtEnumValidate(&Enum), "duplicate enum name was accepted");
	Enum.Variants = NULL;
	testRequire(!xrtEnumValidate(&Enum), "missing enum variants were accepted");
}



/* 运行运行时类型完整测试。 */
int main(void)
{
	testBuiltins();
	testScalarOperations();
	testDescriptorValidation();
	testValueOperations();
	testValueInstanceBoundary();
	testValueTrace();
	testInstanceTrace();
	testRegistryAndInheritance();
	testFunctionMethodsAndProtocol();
	testEnum();
	printf("[PASS] runtime type\n");
	return 0;
}
