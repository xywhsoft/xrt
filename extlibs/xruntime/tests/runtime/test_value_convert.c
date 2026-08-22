#include "../test.h"



/* 验证动态标量使用规范来源类型参与三种转换模式。 */
static void testValueConvertModes(void)
{
	xvalue* pInteger = xrtValueInt(42);
	xvalue* pBool = xrtValueBool(true);
	int64 iExact = 0;
	int32 iNarrow = 17;
	int32 iBool32 = 17;
	double fValue = -1.0;

	testRequire(pInteger != NULL, "Value conversion integer fixture failed");
	testRequire(
		xrtValueConvertTo(
			pInteger, xrtTypeInt64(), &iExact, XTYPE_CONVERT_EXACT
		) && (iExact == 42),
		"dynamic integer exact conversion failed"
	);
	xrtClearError();
	testRequire(
		!xrtValueConvertTo(
			pInteger, xrtTypeInt32(), &iNarrow, XTYPE_CONVERT_EXACT
		) && (iNarrow == 17) &&
		(xrtErrorCode(xrtGetError()) == XTYPE_CONVERT_ERROR_TYPE),
		"dynamic integer exact mode accepted a different width"
	);
	testRequire(
		xrtValueConvertTo(
			pInteger, xrtTypeInt32(), &iNarrow, XTYPE_CONVERT_EXPLICIT
		) && (iNarrow == 42),
		"dynamic integer explicit narrowing failed"
	);
	xrtClearError();
	testRequire(
		!xrtValueConvertTo(
			pInteger, xrtTypeFloat64(), &fValue, XTYPE_CONVERT_WIDEN
		) && (fValue == -1.0),
		"int64 to float64 was incorrectly treated as type-level widening"
	);
	testRequire(
		xrtValueConvertTo(
			pInteger, xrtTypeFloat64(), &fValue, XTYPE_CONVERT_EXPLICIT
		) && (fValue == 42.0),
		"dynamic integer explicit float conversion failed"
	);
	iNarrow = 0;
	testRequire(
		xrtValueConvertTo(
			pBool, xrtTypeInt32(), &iNarrow, XTYPE_CONVERT_WIDEN
		) && (iNarrow == 1),
		"dynamic boolean widening failed"
	);
	testRequire(
		xrtValueConvertTo(
			pBool, xrtTypeBool32(), &iBool32, XTYPE_CONVERT_WIDEN
		) && (iBool32 == 1),
		"dynamic boolean to bool32 widening failed"
	);
	xrtValueRelease(pBool);
	xrtValueRelease(pInteger);
}



/* 验证时间、指针、null 和浮点沿统一底座转换。 */
static void testValueConvertKinds(void)
{
	int iAnchor = 0;
	xvalue* pFloat = xrtValueFloat(-7.75);
	xvalue* pTime = xrtValueTime((xtime)1234567);
	xvalue* pPointer = xrtValuePointer(&iAnchor);
	int32 iInteger = 9;
	int64 iTime = 0;
	ptr pNull = &iAnchor;
	bool bPointer = false;

	testRequire(
		(pFloat != NULL) && (pTime != NULL) && (pPointer != NULL),
		"Value conversion kind fixtures failed"
	);
	testRequire(
		xrtValueConvertTo(
			pFloat, xrtTypeInt32(), &iInteger, XTYPE_CONVERT_EXPLICIT
		) && (iInteger == -7),
		"dynamic float explicit integer conversion failed"
	);
	testRequire(
		xrtValueConvertTo(
			pTime, xrtTypeInt64(), &iTime, XTYPE_CONVERT_EXPLICIT
		) && (iTime == 1234567),
		"dynamic time explicit integer conversion failed"
	);
	testRequire(
		xrtValueConvertTo(
			pPointer, xrtTypeBool(), &bPointer, XTYPE_CONVERT_EXPLICIT
		) && bPointer,
		"dynamic pointer truth conversion failed"
	);
	testRequire(
		xrtValueConvertTo(
			xrtValueNull(), xrtTypePointer(), &pNull, XTYPE_CONVERT_WIDEN
		) && (pNull == NULL),
		"dynamic null pointer widening failed"
	);
	xrtValueRelease(pPointer);
	xrtValueRelease(pTime);
	xrtValueRelease(pFloat);
}



/* 验证范围、参数与非标量失败均不修改目标。 */
static void testValueConvertFailure(void)
{
	xvalue* pLarge = xrtValueInt(128);
	xvalue* pText = xrtValueString(XRT_STR_LITERAL("42"));
	int8 iTarget = 11;

	testRequire((pLarge != NULL) && (pText != NULL),
		"Value conversion failure fixtures failed");
	xrtClearError();
	testRequire(
		!xrtValueConvertTo(
			pLarge, xrtTypeInt8(), &iTarget, XTYPE_CONVERT_EXPLICIT
		) && (iTarget == 11) &&
		(xrtErrorCode(xrtGetError()) == XTYPE_CONVERT_ERROR_RANGE),
		"dynamic integer range failure changed the target"
	);
	xrtClearError();
	testRequire(
		!xrtValueConvertTo(
			pLarge, xrtTypeInt8(), &iTarget, (xtypeconvertmode)99
		) && (iTarget == 11) &&
		(xrtErrorCode(xrtGetError()) == XTYPE_CONVERT_ERROR_MODE),
		"invalid dynamic conversion mode was not rejected"
	);
#if !defined(XRUNTIME_FEATURE_VALUE_CONVERT_STRING)
	xrtClearError();
	testRequire(
		!xrtValueConvertTo(
			pText, xrtTypeInt8(), &iTarget, XTYPE_CONVERT_EXPLICIT
		) && (iTarget == 11) &&
		(xrtErrorCode(xrtGetError()) == XTYPE_CONVERT_ERROR_TYPE),
		"core Value conversion accepted text without the text extension"
	);
#endif
	xrtValueRelease(pText);
	xrtValueRelease(pLarge);
}



/* 运行动态 Value 标量转换契约。 */
int main(void)
{
	testValueConvertModes();
	testValueConvertKinds();
	testValueConvertFailure();
	xrtClearError();
	printf("[PASS] Value runtime conversion\n");
	return 0;
}
