#include "../test.h"

#include <float.h>
#include <math.h>



/* 验证最近一次错误属于类型转换层。 */
static void testConvertError(xtypeconverterror Code, xerrkind Kind)
{
	const xerror* pError = xrtGetError();

	testRequire(pError != NULL, "runtime conversion error is missing");
	testRequire(
		strcmp(xrtErrorDomain(pError), "xrt.type-convert") == 0,
		"runtime conversion error domain mismatch"
	);
	testRequire(xrtErrorCode(pError) == (int32)Code,
		"runtime conversion error code mismatch");
	testRequire(xrtErrorKind(pError) == Kind,
		"runtime conversion error kind mismatch");
}



/* 验证定宽标量的完整无损拓宽关系。 */
static void testConvertWidenRelations(void)
{
	testRequire(xrtTypeCanWiden(xrtTypeBool(), xrtTypeBool32()),
		"bool should widen to bool32");
	testRequire(xrtTypeCanWiden(xrtTypeBool32(), xrtTypeBool()),
		"bool32 should widen to bool");
	testRequire(xrtTypeCanWiden(xrtTypeBool(), xrtTypeInt8()),
		"bool should widen to int8");
	testRequire(xrtTypeCanWiden(xrtTypeBool(), xrtTypeUInt8()),
		"bool should widen to uint8");
	testRequire(xrtTypeCanWiden(xrtTypeBool(), xrtTypeFloat32()),
		"bool should widen to float32");
	testRequire(xrtTypeCanWiden(xrtTypeInt8(), xrtTypeInt64()),
		"int8 should widen to int64");
	testRequire(!xrtTypeCanWiden(xrtTypeInt16(), xrtTypeInt8()),
		"int16 should not widen to int8");
	testRequire(xrtTypeCanWiden(xrtTypeUInt8(), xrtTypeInt16()),
		"uint8 should widen to int16");
	testRequire(!xrtTypeCanWiden(xrtTypeUInt16(), xrtTypeInt16()),
		"uint16 should not widen to int16");
	testRequire(!xrtTypeCanWiden(xrtTypeInt8(), xrtTypeUInt64()),
		"signed values should not widen to unsigned values");
	testRequire(xrtTypeCanWiden(xrtTypeInt16(), xrtTypeFloat32()),
		"int16 should widen to float32");
	testRequire(!xrtTypeCanWiden(xrtTypeInt32(), xrtTypeFloat32()),
		"int32 should not widen to float32");
	testRequire(xrtTypeCanWiden(xrtTypeUInt32(), xrtTypeFloat64()),
		"uint32 should widen to float64");
	testRequire(!xrtTypeCanWiden(xrtTypeInt64(), xrtTypeFloat64()),
		"int64 should not widen to float64");
	testRequire(xrtTypeCanWiden(xrtTypeFloat32(), xrtTypeFloat64()),
		"float32 should widen to float64");
	testRequire(!xrtTypeCanWiden(xrtTypeFloat64(), xrtTypeFloat32()),
		"float64 should not widen to float32");
	testRequire(xrtTypeCanWiden(xrtTypeNull(), xrtTypePointer()),
		"null should widen to pointer");
}



/* 验证三种模式按 exact、widen、explicit 逐级包含。 */
static void testConvertModes(void)
{
	testRequire(xrtTypeCanConvert(xrtTypeInt32(), xrtTypeInt32(),
		XTYPE_CONVERT_EXACT), "exact mode rejected the same type");
	testRequire(!xrtTypeCanConvert(xrtTypeInt32(), xrtTypeInt64(),
		XTYPE_CONVERT_EXACT), "exact mode accepted different types");
	testRequire(xrtTypeCanConvert(xrtTypeInt32(), xrtTypeInt64(),
		XTYPE_CONVERT_WIDEN), "widen mode rejected a lossless direction");
	testRequire(!xrtTypeCanConvert(xrtTypeInt64(), xrtTypeInt32(),
		XTYPE_CONVERT_WIDEN), "widen mode accepted a narrowing direction");
	testRequire(xrtTypeCanConvert(xrtTypeInt64(), xrtTypeInt32(),
		XTYPE_CONVERT_EXPLICIT), "explicit mode rejected checked narrowing");
	testRequire(!xrtTypeCanConvert(xrtTypePointer(), xrtTypeInt64(),
		XTYPE_CONVERT_EXPLICIT), "explicit mode accepted pointer to integer");
}



/* 验证整数、布尔和类型标识转换。 */
static void testConvertIntegers(void)
{
	int8 iSmall = INT8_MIN;
	int64 iWide = 0;
	uint64 iUnsigned = UINT64_MAX;
	int64 iSignedTarget = 77;
	int64 iNegative = -1;
	uint64 iUnsignedTarget = 88u;
	bool bTrue = true;
	bool bBool32 = false;
	int32 iBool32 = -7;
	int32 iBool32Target = 77;
	uint8 iBoolValue = 0u;
	uint64 iTypeId = UINT64_C(42);
	int16 iTypeResult = 0;

	testRequire(xrtTypeConvert(xrtTypeInt8(), &iSmall,
		xrtTypeInt64(), &iWide, XTYPE_CONVERT_WIDEN) &&
		(iWide == INT8_MIN), "signed widening changed the value");
	testRequire(xrtTypeConvert(xrtTypeBool(), &bTrue,
		xrtTypeUInt8(), &iBoolValue, XTYPE_CONVERT_WIDEN) &&
		(iBoolValue == 1u), "boolean widening changed the value");
	testRequire(xrtTypeConvert(xrtTypeBool32(), &iBool32,
		xrtTypeBool(), &bBool32, XTYPE_CONVERT_WIDEN) && bBool32,
		"bool32 to bool widening failed");
	testRequire(xrtTypeConvert(xrtTypeBool(), &bTrue,
		xrtTypeBool32(), &iBool32Target, XTYPE_CONVERT_WIDEN) &&
		(iBool32Target == 1), "bool to bool32 widening was not normalized");
	testRequire(xrtTypeConvert(xrtTypeType(), &iTypeId,
		xrtTypeInt16(), &iTypeResult, XTYPE_CONVERT_EXPLICIT) &&
		(iTypeResult == 42), "type id conversion changed the value");

	xrtClearError();
	testRequire(!xrtTypeConvert(xrtTypeUInt64(), &iUnsigned,
		xrtTypeInt64(), &iSignedTarget, XTYPE_CONVERT_EXPLICIT) &&
		(iSignedTarget == 77), "uint64 overflow changed the signed target");
	testConvertError(XTYPE_CONVERT_ERROR_RANGE, XERR_RANGE);

	xrtClearError();
	testRequire(!xrtTypeConvert(xrtTypeInt64(), &iNegative,
		xrtTypeUInt64(), &iUnsignedTarget, XTYPE_CONVERT_EXPLICIT) &&
		(iUnsignedTarget == 88u), "negative conversion changed the unsigned target");
	testConvertError(XTYPE_CONVERT_ERROR_RANGE, XERR_RANGE);
}



/* 验证浮点截断、边界、特殊值和 float32 溢出。 */
static void testConvertFloats(void)
{
	double fValue = 127.9;
	int8 iResult = 0;
	double fBoundary = 128.0;
	double fMinimum = -128.0;
	double fNan = NAN;
	double fInfinity = INFINITY;
	double fHuge = DBL_MAX;
	double fFinite = FLT_MAX;
	int32 iInteger = 91;
	float fResult = 7.0f;
	float fNanResult = 0.0f;

	testRequire(xrtTypeConvert(xrtTypeFloat64(), &fValue,
		xrtTypeInt8(), &iResult, XTYPE_CONVERT_EXPLICIT) &&
		(iResult == 127), "float to integer did not truncate toward zero");
	testRequire(xrtTypeConvert(xrtTypeFloat64(), &fMinimum,
		xrtTypeInt8(), &iResult, XTYPE_CONVERT_EXPLICIT) &&
		(iResult == INT8_MIN), "minimum integer boundary was rejected");

	xrtClearError();
	iResult = 12;
	testRequire(!xrtTypeConvert(xrtTypeFloat64(), &fBoundary,
		xrtTypeInt8(), &iResult, XTYPE_CONVERT_EXPLICIT) &&
		(iResult == 12), "exclusive integer boundary changed the target");
	testConvertError(XTYPE_CONVERT_ERROR_RANGE, XERR_RANGE);

	xrtClearError();
	testRequire(!xrtTypeConvert(xrtTypeFloat64(), &fNan,
		xrtTypeInt32(), &iInteger, XTYPE_CONVERT_EXPLICIT) &&
		(iInteger == 91), "NaN to integer changed the target");
	testConvertError(XTYPE_CONVERT_ERROR_RANGE, XERR_RANGE);

	xrtClearError();
	testRequire(!xrtTypeConvert(xrtTypeFloat64(), &fInfinity,
		xrtTypeInt32(), &iInteger, XTYPE_CONVERT_EXPLICIT) &&
		(iInteger == 91), "infinity to integer changed the target");
	testConvertError(XTYPE_CONVERT_ERROR_RANGE, XERR_RANGE);

	xrtClearError();
	testRequire(!xrtTypeConvert(xrtTypeFloat64(), &fHuge,
		xrtTypeFloat32(), &fResult, XTYPE_CONVERT_EXPLICIT) &&
		(fResult == 7.0f), "finite float32 overflow changed the target");
	testConvertError(XTYPE_CONVERT_ERROR_RANGE, XERR_RANGE);

	testRequire(xrtTypeConvert(xrtTypeFloat64(), &fFinite,
		xrtTypeFloat32(), &fResult, XTYPE_CONVERT_EXPLICIT) &&
		(fResult == FLT_MAX), "FLT_MAX conversion failed");
	testRequire(xrtTypeConvert(xrtTypeFloat64(), &fNan,
		xrtTypeFloat32(), &fNanResult, XTYPE_CONVERT_EXPLICIT) &&
		isnan(fNanResult), "NaN conversion to float32 failed");
}



/* 验证时间、空值和指针转换。 */
static void testConvertSpecialScalars(void)
{
	xtime Time = INT64_C(123456789);
	int64 iTime = 0;
	uint64 iTooLarge = UINT64_MAX;
	xtime TimeTarget = 33;
	ptr pValue = (ptr)(uintptr_t)UINT64_C(1);
	ptr pNullTarget = pValue;
	bool bPointer = false;
	ptr pNull = NULL;
	bool bNullPointer = true;

	testRequire(xrtTypeConvert(xrtTypeTime(), &Time,
		xrtTypeInt64(), &iTime, XTYPE_CONVERT_EXPLICIT) &&
		(iTime == (int64)Time), "time to integer conversion failed");
	testRequire(xrtTypeConvert(xrtTypeInt64(), &iTime,
		xrtTypeTime(), &TimeTarget, XTYPE_CONVERT_EXPLICIT) &&
		(TimeTarget == Time), "integer to time conversion failed");
	testRequire(xrtTypeConvert(xrtTypeNull(), NULL,
		xrtTypePointer(), &pNullTarget, XTYPE_CONVERT_WIDEN) &&
		(pNullTarget == NULL), "null to pointer conversion failed");
	testRequire(xrtTypeConvert(xrtTypePointer(), &pValue,
		xrtTypeBool(), &bPointer, XTYPE_CONVERT_EXPLICIT) && bPointer,
		"non-null pointer truth conversion failed");
	testRequire(xrtTypeConvert(xrtTypePointer(), &pNull,
		xrtTypeBool(), &bNullPointer, XTYPE_CONVERT_EXPLICIT) &&
		!bNullPointer, "null pointer truth conversion failed");

	xrtClearError();
	testRequire(!xrtTypeConvert(xrtTypeUInt64(), &iTooLarge,
		xrtTypeTime(), &TimeTarget, XTYPE_CONVERT_EXPLICIT) &&
		(TimeTarget == Time), "time overflow changed the target");
	testConvertError(XTYPE_CONVERT_ERROR_RANGE, XERR_RANGE);
}



/* 验证参数、描述、重叠范围和陈旧错误隔离。 */
static void testConvertFailures(void)
{
	int32 iSource = 42;
	int32 iTarget = 77;
	xrttype Invalid = *xrtTypeInt32();
	unsigned char arrOverlap[16];
	unsigned char arrBefore[16];
	int64 iOverlapSource = INT64_C(123456);
	ptr pPointer = (ptr)(uintptr_t)UINT64_C(1);
	int64 iUnsupported = 99;
	xerror* pStale;

	xrtClearError();
	testRequire(!xrtTypeConvert(xrtTypeInt32(), &iSource,
		xrtTypeInt32(), &iTarget, (xtypeconvertmode)99) &&
		(iTarget == 77), "invalid mode changed the target");
	testConvertError(XTYPE_CONVERT_ERROR_MODE, XERR_ARGUMENT);

	Invalid.Id ^= UINT64_C(1);
	xrtClearError();
	testRequire(!xrtTypeConvert(&Invalid, &iSource,
		xrtTypeInt32(), &iTarget, XTYPE_CONVERT_EXACT) &&
		(iTarget == 77), "invalid descriptor changed the target");
	testConvertError(XTYPE_CONVERT_ERROR_TYPE, XERR_ARGUMENT);

	memset(arrOverlap, 0xA5, sizeof(arrOverlap));
	memcpy(arrOverlap, &iOverlapSource, sizeof(iOverlapSource));
	memcpy(arrBefore, arrOverlap, sizeof(arrOverlap));
	xrtClearError();
	testRequire(!xrtTypeConvert(xrtTypeInt64(), arrOverlap,
		xrtTypeInt32(), arrOverlap + 2u, XTYPE_CONVERT_EXPLICIT) &&
		(memcmp(arrOverlap, arrBefore, sizeof(arrOverlap)) == 0),
		"overlapping conversion changed storage");
	testConvertError(XTYPE_CONVERT_ERROR_ARGUMENT, XERR_ARGUMENT);

	pStale = xrtErrorCreate(XERR_IO, "test.stale", 91, "stale error");
	testRequire(pStale != NULL, "failed to create stale error fixture");
	xrtSetError(pStale);
	xrtErrorFree(pStale);
	testRequire(!xrtTypeConvert(xrtTypePointer(), &pPointer,
		xrtTypeInt64(), &iUnsupported, XTYPE_CONVERT_EXPLICIT) &&
		(iUnsupported == 99), "unsupported conversion changed the target");
	testConvertError(XTYPE_CONVERT_ERROR_TYPE, XERR_TYPE);
}



/* 验证同类型转换沿稳定复制操作执行。 */
static void testConvertExact(void)
{
	int32 iSource = INT32_C(-1234567);
	int32 iTarget = 0;

	testRequire(xrtTypeConvert(xrtTypeInt32(), &iSource,
		xrtTypeInt32(), &iTarget, XTYPE_CONVERT_EXACT) &&
		(iTarget == iSource), "exact conversion did not copy the value");
	testRequire(xrtTypeConvert(xrtTypeInt32(), &iSource,
		xrtTypeInt32(), &iSource, XTYPE_CONVERT_EXACT) &&
		(iSource == INT32_C(-1234567)), "exact self conversion changed the value");
}



/* 运行不依赖字符串模块的类型转换契约。 */
int main(void)
{
	testConvertWidenRelations();
	testConvertModes();
	testConvertIntegers();
	testConvertFloats();
	testConvertSpecialScalars();
	testConvertFailures();
	testConvertExact();
	xrtClearError();
	printf("[PASS] runtime conversion\n");
	return 0;
}
