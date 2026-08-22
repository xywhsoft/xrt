#include "../test.h"

#include <math.h>



typedef struct testformatwriter {
	char Buffer[64];
	size_t Size;
	size_t WriteCount;
	size_t FailWrite;
} testformatwriter;



/* 把格式化分块写入固定缓冲，并按序号模拟无错误失败。 */
static bool testStringFormatWrite(xstrview Text, ptr pContext)
{
	testformatwriter* pWriter = (testformatwriter*)pContext;

	pWriter->WriteCount++;
	if ( (pWriter->FailWrite != 0u) &&
		 (pWriter->WriteCount == pWriter->FailWrite) ) {
		return false;
	}
	if ( Text.Size > (sizeof(pWriter->Buffer) - pWriter->Size - 1u) ) {
		return false;
	}
	memcpy(pWriter->Buffer + pWriter->Size, Text.Data, Text.Size);
	pWriter->Size += Text.Size;
	pWriter->Buffer[pWriter->Size] = '\0';
	return true;
}



/* 发布一个可由转换层保留为原因的自定义格式化错误。 */
static bool testStringFormatFailWithError(void)
{
	xerror* pError = xrtErrorCreate(
		XERR_IO, "test.format", 73, "custom formatter failure"
	);

	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
	return false;
}



/* 使用三个借用分块格式化测试记录，并提供两种失败路径。 */
static bool testStringFormatValue(
	const void* pValue,
	const xrttype* pType,
	xrttypewriter pWrite,
	ptr pContext
)
{
	int iValue;

	(void)pType;
	memcpy(&iValue, pValue, sizeof(iValue));
	if ( iValue == -1 ) {
		return false;
	}
	if ( iValue == -2 ) {
		return testStringFormatFailWithError();
	}
	return pWrite(XRT_STR_LITERAL("value"), pContext) &&
		pWrite(XRT_STR_LITERAL("="), pContext) &&
		pWrite(XRT_STR_LITERAL("17"), pContext);
}



/* 构造带有流式格式化能力的有效记录类型。 */
static xrttype testStringFormatType(void)
{
	static const xrttypeops Ops = {
		.Format = testStringFormatValue
	};
	xrttype Type = {
		.Id = 0,
		.Kind = XRT_TYPE_RECORD,
		.Flags = XRT_TYPE_FLAG_TRIVIAL_COPY |
			XRT_TYPE_FLAG_TRIVIAL_DROP | XRT_TYPE_FLAG_COPYABLE |
			XRT_TYPE_FLAG_RELOCATABLE | XRT_TYPE_FLAG_FINAL,
		.Name = XRT_STR_INIT("FormatValue"),
		.AbiName = XRT_STR_INIT("tests.runtime.FormatValue"),
		.Size = sizeof(int),
		.Align = TEST_ALIGNOF(int),
		.InstanceSize = sizeof(int),
		.InstanceAlign = TEST_ALIGNOF(int),
		.Ops = &Ops
	};

	Type.Id = xrtTypeId(Type.AbiName);
	return Type;
}



/* 验证文本转换失败发布稳定错误并保持指定目标文本。 */
static void testStringConvertError(
	xtypeconverterror Code,
	cstr sMessage
)
{
	const xerror* pError = xrtGetError();

	testRequire(pError != NULL, sMessage);
	testRequire(
		(strcmp(xrtErrorDomain(pError), "xrt.type-convert") == 0) &&
		(xrtErrorCode(pError) == (int32)Code),
		sMessage
	);
}



/* 验证文本扩展只绑定规范拥有型字符串。 */
static void testStringConvertRelations(void)
{
	const xrttype* pString = xrtTypeString();

	testRequire(xrtTypeCanConvert(pString, xrtTypeBool(),
		XTYPE_CONVERT_EXPLICIT), "string to bool relation is missing");
	testRequire(xrtTypeCanConvert(pString, xrtTypeInt8(),
		XTYPE_CONVERT_EXPLICIT), "string to signed relation is missing");
	testRequire(xrtTypeCanConvert(pString, xrtTypeUInt64(),
		XTYPE_CONVERT_EXPLICIT), "string to unsigned relation is missing");
	testRequire(xrtTypeCanConvert(pString, xrtTypeFloat64(),
		XTYPE_CONVERT_EXPLICIT), "string to float relation is missing");
	testRequire(xrtTypeCanConvert(pString, xrtTypeTime(),
		XTYPE_CONVERT_EXPLICIT), "string to time relation is missing");
	testRequire(xrtTypeCanConvert(xrtTypePointer(), pString,
		XTYPE_CONVERT_EXPLICIT), "pointer to string relation is missing");
	testRequire(!xrtTypeCanConvert(pString, xrtTypePointer(),
		XTYPE_CONVERT_EXPLICIT), "string to pointer relation should be unsupported");
	testRequire(!xrtTypeCanConvert(pString, xrtTypeInt64(),
		XTYPE_CONVERT_WIDEN), "string conversion should require explicit mode");
}



/* 验证严格布尔文本，不接受隐式真值或空白。 */
static void testStringConvertBool(void)
{
	str sTrue = "TRUE";
	str sFalse = "0";
	str sInvalid = "yes";
	str sSpace = " true";
	bool bValue = false;
	int32 iBool32 = 7;

	testRequire(xrtTypeConvert(xrtTypeString(), &sTrue,
		xrtTypeBool(), &bValue, XTYPE_CONVERT_EXPLICIT) && bValue,
		"strict true conversion failed");
	testRequire(xrtTypeConvert(xrtTypeString(), &sFalse,
		xrtTypeBool(), &bValue, XTYPE_CONVERT_EXPLICIT) && !bValue,
		"strict false conversion failed");
	testRequire(xrtTypeConvert(xrtTypeString(), &sTrue,
		xrtTypeBool32(), &iBool32, XTYPE_CONVERT_EXPLICIT) &&
		(iBool32 == 1), "strict bool32 conversion failed");

	bValue = true;
	xrtClearError();
	testRequire(!xrtTypeConvert(xrtTypeString(), &sInvalid,
		xrtTypeBool(), &bValue, XTYPE_CONVERT_EXPLICIT) && bValue,
		"invalid boolean text changed the target");
	testStringConvertError(XTYPE_CONVERT_ERROR_PARSE,
		"invalid boolean text error mismatch");

	xrtClearError();
	testRequire(!xrtTypeConvert(xrtTypeString(), &sSpace,
		xrtTypeBool(), &bValue, XTYPE_CONVERT_EXPLICIT) && bValue,
		"boolean conversion accepted leading space");
	testStringConvertError(XTYPE_CONVERT_ERROR_PARSE,
		"spaced boolean text error mismatch");
}



/* 验证整数文本完整解析和目标宽度范围检查。 */
static void testStringConvertIntegers(void)
{
	str sSigned = "-128";
	str sUnsigned = "18446744073709551615";
	str sOverflow = "128";
	str sNegative = "-1";
	str sPrefix = "0x10";
	int8 iSigned = 0;
	uint64 iUnsigned = 0u;
	int8 iSmall = 27;
	uint32 iTarget = 31u;

	testRequire(xrtTypeConvert(xrtTypeString(), &sSigned,
		xrtTypeInt8(), &iSigned, XTYPE_CONVERT_EXPLICIT) &&
		(iSigned == INT8_MIN), "minimum int8 text conversion failed");
	testRequire(xrtTypeConvert(xrtTypeString(), &sUnsigned,
		xrtTypeUInt64(), &iUnsigned, XTYPE_CONVERT_EXPLICIT) &&
		(iUnsigned == UINT64_MAX), "maximum uint64 text conversion failed");

	xrtClearError();
	testRequire(!xrtTypeConvert(xrtTypeString(), &sOverflow,
		xrtTypeInt8(), &iSmall, XTYPE_CONVERT_EXPLICIT) &&
		(iSmall == 27), "narrow integer overflow changed the target");
	testStringConvertError(XTYPE_CONVERT_ERROR_RANGE,
		"narrow integer overflow error mismatch");

	xrtClearError();
	testRequire(!xrtTypeConvert(xrtTypeString(), &sNegative,
		xrtTypeUInt32(), &iTarget, XTYPE_CONVERT_EXPLICIT) &&
		(iTarget == 31u), "negative unsigned text changed the target");
	testStringConvertError(XTYPE_CONVERT_ERROR_PARSE,
		"negative unsigned text error mismatch");

	xrtClearError();
	testRequire(!xrtTypeConvert(xrtTypeString(), &sPrefix,
		xrtTypeUInt32(), &iTarget, XTYPE_CONVERT_EXPLICIT) &&
		(iTarget == 31u), "prefixed integer text was accepted");
	testStringConvertError(XTYPE_CONVERT_ERROR_PARSE,
		"prefixed integer text error mismatch");
}



/* 验证浮点普通值和特殊值采用完整文本解析。 */
static void testStringConvertFloats(void)
{
	str sFinite = "1.25";
	str sInfinity = "-Infinity";
	str sNan = "NaN";
	double fFinite = 0.0;
	double fInfinity = 0.0;
	float fNan = 0.0f;

	testRequire(xrtTypeConvert(xrtTypeString(), &sFinite,
		xrtTypeFloat64(), &fFinite, XTYPE_CONVERT_EXPLICIT) &&
		(fFinite == 1.25), "finite floating-point text conversion failed");
	testRequire(xrtTypeConvert(xrtTypeString(), &sInfinity,
		xrtTypeFloat64(), &fInfinity, XTYPE_CONVERT_EXPLICIT) &&
		isinf(fInfinity) && (fInfinity < 0.0),
		"infinity text conversion failed");
	testRequire(xrtTypeConvert(xrtTypeString(), &sNan,
		xrtTypeFloat32(), &fNan, XTYPE_CONVERT_EXPLICIT) && isnan(fNan),
		"NaN text conversion failed");
}



/* 验证支持的时间文本可以规范化为 UTC RFC 3339。 */
static void testStringConvertTime(void)
{
	str sInput = "2024-01-02T03:04:05Z";
	xtime Time = 0;
	str sOutput = xrtStrDup("old");

	testRequire(sOutput != NULL, "time string fixture allocation failed");
	testRequire(xrtTypeConvert(xrtTypeString(), &sInput,
		xrtTypeTime(), &Time, XTYPE_CONVERT_EXPLICIT),
		"RFC 3339 text to time conversion failed");
	testRequire(xrtTypeConvert(xrtTypeTime(), &Time,
		xrtTypeString(), &sOutput, XTYPE_CONVERT_EXPLICIT) &&
		(strcmp(sOutput, "2024-01-02T03:04:05Z") == 0),
		"time to canonical RFC 3339 conversion failed");
	xrtTypeDropValue(xrtTypeString(), &sOutput);
}



/* 转换一个标量并验证目标字符串随后仍可统一销毁。 */
static void testStringFormatOne(
	const xrttype* pSourceType,
	const void* pSource,
	cstr sExpected
)
{
	str sTarget = xrtStrDup("previous");

	testRequire(sTarget != NULL, "string format fixture allocation failed");
	testRequire(xrtTypeConvert(pSourceType, pSource,
		xrtTypeString(), &sTarget, XTYPE_CONVERT_EXPLICIT),
		"scalar to string conversion failed");
	testRequire(strcmp(sTarget, sExpected) == 0,
		"scalar string representation mismatch");
	xrtTypeDropValue(xrtTypeString(), &sTarget);
}



/* 验证全部支持标量的稳定字符串表示。 */
static void testStringConvertFormat(void)
{
	bool bValue = true;
	int32 iBool32 = -7;
	int8 iInt8 = -8;
	uint8 iUInt8 = 8u;
	int16 iInt16 = -16;
	uint16 iUInt16 = 16u;
	int32 iInt32 = -32;
	uint32 iUInt32 = 32u;
	int64 iInt64 = INT64_MIN;
	uint64 iUInt64 = UINT64_MAX;
	uint64 iType = UINT64_C(42);
	float fFloat32 = 1.25f;
	double fFloat64 = 1.5;
	xtime Time = 0;
	ptr pValue = (ptr)(uintptr_t)UINT64_C(1);
	ptr pNull = NULL;

	testStringFormatOne(xrtTypeNull(), NULL, "null");
	testStringFormatOne(xrtTypeBool(), &bValue, "true");
	testStringFormatOne(xrtTypeBool32(), &iBool32, "true");
	testStringFormatOne(xrtTypeInt8(), &iInt8, "-8");
	testStringFormatOne(xrtTypeUInt8(), &iUInt8, "8");
	testStringFormatOne(xrtTypeInt16(), &iInt16, "-16");
	testStringFormatOne(xrtTypeUInt16(), &iUInt16, "16");
	testStringFormatOne(xrtTypeInt32(), &iInt32, "-32");
	testStringFormatOne(xrtTypeUInt32(), &iUInt32, "32");
	testStringFormatOne(xrtTypeInt64(), &iInt64,
		"-9223372036854775808");
	testStringFormatOne(xrtTypeUInt64(), &iUInt64,
		"18446744073709551615");
	testStringFormatOne(xrtTypeType(), &iType, "42");
	testStringFormatOne(xrtTypeFloat32(), &fFloat32, "1.25");
	testStringFormatOne(xrtTypeFloat64(), &fFloat64, "1.5");
	testStringFormatOne(xrtTypeTime(), &Time,
		"1970-01-01T00:00:00Z");
	testStringFormatOne(xrtTypePointer(), &pValue, "0x1");
	testStringFormatOne(xrtTypePointer(), &pNull, "null");
}



/* 验证同类型字符串转换执行深复制并安全替换旧目标。 */
static void testStringConvertExact(void)
{
	str sSource = xrtStrDup("source");
	str sTarget = xrtStrDup("target");

	testRequire((sSource != NULL) && (sTarget != NULL),
		"exact string conversion fixture allocation failed");
	testRequire(xrtTypeConvert(xrtTypeString(), &sSource,
		xrtTypeString(), &sTarget, XTYPE_CONVERT_EXACT) &&
		(sTarget != sSource) && (strcmp(sTarget, sSource) == 0),
		"exact string conversion did not deep copy");
	xrtTypeDropValue(xrtTypeString(), &sSource);
	xrtTypeDropValue(xrtTypeString(), &sTarget);
}



/* 验证流式格式化的参数、不支持类型和内建 writer 失败边界。 */
static void testStringFormatBoundaries(void)
{
	xrttype Plain = testStringFormatType();
	testformatwriter Writer;
	int iValue = 42;
	str sText;

	Plain.Name = XRT_STR_LITERAL("PlainValue");
	Plain.AbiName = XRT_STR_LITERAL("tests.runtime.PlainValue");
	Plain.Id = xrtTypeId(Plain.AbiName);
	Plain.Ops = NULL;

	memset(&Writer, 0, sizeof(Writer));
	xrtClearError();
	testRequire(!xrtTypeFormat(
		NULL, NULL, testStringFormatWrite, &Writer
	), "format accepted a null type");
	testStringConvertError(XTYPE_CONVERT_ERROR_ARGUMENT,
		"null format type error mismatch");

	xrtClearError();
	testRequire(!xrtTypeFormat(
		xrtTypeInt32(), NULL, testStringFormatWrite, &Writer
	), "format accepted a null non-empty value");
	testStringConvertError(XTYPE_CONVERT_ERROR_ARGUMENT,
		"null format value error mismatch");

	xrtClearError();
	testRequire(!xrtTypeFormat(
		xrtTypeInt32(), &iValue, NULL, &Writer
	), "format accepted a null writer");
	testStringConvertError(XTYPE_CONVERT_ERROR_ARGUMENT,
		"null format writer error mismatch");

	xrtClearError();
	testRequire(!xrtTypeFormat(
		&Plain, &iValue, testStringFormatWrite, &Writer
	), "format accepted a type without text capability");
	testStringConvertError(XTYPE_CONVERT_ERROR_TYPE,
		"unsupported format type error mismatch");

	xrtClearError();
	sText = xrtTypeToString(&Plain, &iValue);
	testRequire(sText == NULL,
		"owned format accepted a type without text capability");
	testStringConvertError(XTYPE_CONVERT_ERROR_TYPE,
		"unsupported owned format error mismatch");

	memset(&Writer, 0, sizeof(Writer));
	Writer.FailWrite = 1u;
	xrtClearError();
	testRequire(!xrtTypeFormat(
		xrtTypeInt32(), &iValue, testStringFormatWrite, &Writer
	) && (Writer.WriteCount == 1u) && (Writer.Size == 0u),
		"built-in formatter ignored writer failure");
	testStringConvertError(XTYPE_CONVERT_ERROR_OPERATION,
		"built-in writer failure error mismatch");
}



/* 验证自定义类型的流式、拥有型、转换和失败传播契约。 */
static void testStringConvertCustomFormat(void)
{
	xrttype Type = testStringFormatType();
	testformatwriter Writer;
	const xerror* pError;
	int iValue = 17;
	str sText;
	str sTarget = xrtStrDup("previous");
	str sBefore;

	testRequire((sTarget != NULL) && xrtTypeValidate(&Type),
		"custom format fixture is invalid");
	testRequire(xrtTypeCanConvert(&Type, xrtTypeString(),
		XTYPE_CONVERT_EXPLICIT), "custom format relation is missing");
	testRequire(!xrtTypeCanConvert(&Type, xrtTypeString(),
		XTYPE_CONVERT_WIDEN), "custom format should require explicit mode");

	memset(&Writer, 0, sizeof(Writer));
	testRequire(xrtTypeFormat(
		&Type, &iValue, testStringFormatWrite, &Writer
	) && (Writer.WriteCount == 3u) &&
		(strcmp(Writer.Buffer, "value=17") == 0),
		"custom streaming format failed");

	sText = xrtTypeToString(&Type, &iValue);
	testRequire((sText != NULL) && (strcmp(sText, "value=17") == 0),
		"custom owned string format failed");
	xrtFree(sText);

	testRequire(xrtTypeConvert(&Type, &iValue, xrtTypeString(),
		&sTarget, XTYPE_CONVERT_EXPLICIT) &&
		(strcmp(sTarget, "value=17") == 0),
		"custom type to string conversion failed");

	iValue = -1;
	sBefore = sTarget;
	xrtClearError();
	testRequire(!xrtTypeConvert(&Type, &iValue, xrtTypeString(),
		&sTarget, XTYPE_CONVERT_EXPLICIT) && (sTarget == sBefore) &&
		(strcmp(sTarget, "value=17") == 0),
		"silent custom format failure changed the target");
	testStringConvertError(XTYPE_CONVERT_ERROR_OPERATION,
		"silent custom format failure error mismatch");

	iValue = -2;
	memset(&Writer, 0, sizeof(Writer));
	xrtClearError();
	testRequire(!xrtTypeFormat(
		&Type, &iValue, testStringFormatWrite, &Writer
	), "custom format failure with error succeeded");
	pError = xrtGetError();
	testRequire((pError != NULL) && (xrtErrorKind(pError) == XERR_IO) &&
		(xrtErrorFind(pError, "test.format", 73) != NULL),
		"custom format cause was not preserved");
	testStringConvertError(XTYPE_CONVERT_ERROR_OPERATION,
		"custom format cause wrapper mismatch");

	iValue = 17;
	memset(&Writer, 0, sizeof(Writer));
	Writer.FailWrite = 2u;
	xrtClearError();
	testRequire(!xrtTypeFormat(
		&Type, &iValue, testStringFormatWrite, &Writer
	) && (Writer.WriteCount == 2u) &&
		(strcmp(Writer.Buffer, "value") == 0),
		"custom formatter ignored writer failure");
	testStringConvertError(XTYPE_CONVERT_ERROR_OPERATION,
		"writer failure error mismatch");

	xrtTypeDropValue(xrtTypeString(), &sTarget);
}



/* 运行可裁剪文本转换扩展的完整契约。 */
int main(void)
{
	testStringConvertRelations();
	testStringConvertBool();
	testStringConvertIntegers();
	testStringConvertFloats();
	testStringConvertTime();
	testStringConvertFormat();
	testStringConvertExact();
	testStringFormatBoundaries();
	testStringConvertCustomFormat();
	xrtClearError();
	printf("[PASS] runtime string conversion\n");
	return 0;
}
