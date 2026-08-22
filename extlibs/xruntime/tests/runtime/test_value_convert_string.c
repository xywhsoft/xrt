#include "../test.h"



/* 验证动态字符串按严格文本规则转换为标量。 */
static void testValueStringParse(void)
{
	xvalue* pInteger = xrtValueString(XRT_STR_LITERAL("32767"));
	xvalue* pBool = xrtValueString(XRT_STR_LITERAL("TrUe"));
	xvalue* pInvalid = xrtValueString(XRT_STR_LITERAL("12x"));
	int16 iValue = 0;
	bool bValue = false;

	testRequire(
		(pInteger != NULL) && (pBool != NULL) && (pInvalid != NULL),
		"Value string parse fixtures failed"
	);
	testRequire(
		xrtValueConvertTo(
			pInteger, xrtTypeInt16(), &iValue, XTYPE_CONVERT_EXPLICIT
		) && (iValue == 32767),
		"dynamic string integer parsing failed"
	);
	testRequire(
		xrtValueConvertTo(
			pBool, xrtTypeBool(), &bValue, XTYPE_CONVERT_EXPLICIT
		) && bValue,
		"dynamic string boolean parsing failed"
	);
	iValue = 19;
	xrtClearError();
	testRequire(
		!xrtValueConvertTo(
			pInvalid, xrtTypeInt16(), &iValue, XTYPE_CONVERT_EXPLICIT
		) && (iValue == 19) &&
		(xrtErrorCode(xrtGetError()) == XTYPE_CONVERT_ERROR_PARSE),
		"invalid dynamic string changed the numeric target"
	);
	iValue = 23;
	xrtClearError();
	testRequire(
		!xrtValueConvertTo(
			pInteger, xrtTypeInt16(), &iValue, XTYPE_CONVERT_WIDEN
		) && (iValue == 23),
		"dynamic string parsing ignored the explicit-mode requirement"
	);
	xrtValueRelease(pInvalid);
	xrtValueRelease(pBool);
	xrtValueRelease(pInteger);
}



/* 验证字符串精确复制和标量格式化使用统一拥有型字符串契约。 */
static void testValueStringFormat(void)
{
	xvalue* pText = xrtValueString(XRT_STR_LITERAL("copied"));
	xvalue* pInteger = xrtValueInt(INT64_MIN);
	str sTarget = xrtStrDup("old");

	testRequire(
		(pText != NULL) && (pInteger != NULL) && (sTarget != NULL),
		"Value string format fixtures failed"
	);
	testRequire(
		xrtValueConvertTo(
			pText, xrtTypeString(), &sTarget, XTYPE_CONVERT_EXACT
		) && (strcmp(sTarget, "copied") == 0),
		"dynamic string exact copy failed"
	);
	testRequire(
		xrtValueConvertTo(
			pInteger, xrtTypeString(), &sTarget, XTYPE_CONVERT_EXPLICIT
		) && (strcmp(sTarget, "-9223372036854775808") == 0),
		"dynamic integer string formatting failed"
	);
	xrtTypeDropValue(xrtTypeString(), &sTarget);
	xrtValueRelease(pInteger);
	xrtValueRelease(pText);
}



/* 验证内嵌零不能静默截断为拥有型 str 或文本标量。 */
static void testValueStringEmbeddedZero(void)
{
	static const char Data[] = { '4', '2', 0, '7' };
	xvalue* pText = xrtValueString((xstrview){ Data, sizeof(Data) });
	int32 iTarget = 31;

	testRequire(pText != NULL, "embedded-zero Value fixture failed");
	xrtClearError();
	testRequire(
		!xrtValueConvertTo(
			pText, xrtTypeInt32(), &iTarget, XTYPE_CONVERT_EXPLICIT
		) && (iTarget == 31) &&
		(xrtErrorCode(xrtGetError()) == XTYPE_CONVERT_ERROR_TYPE),
		"embedded-zero dynamic string was truncated or changed the target"
	);
	xrtValueRelease(pText);
}



/* 运行动态 Value 文本转换契约。 */
int main(void)
{
	testValueStringParse();
	testValueStringFormat();
	testValueStringEmbeddedZero();
	xrtClearError();
	printf("[PASS] Value string runtime conversion\n");
	return 0;
}
