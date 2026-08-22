#define XRUNTIME_IMPLEMENTATION
#include "../../single/xruntime.h"



/* 以两个借用分块格式化单头文件中的自定义记录。 */
static bool testSingleFormatValue(
	const void* pValue,
	const xrttype* pType,
	xrttypewriter pWrite,
	ptr pContext
)
{
	(void)pValue;
	(void)pType;
	return pWrite(XRT_STR_LITERAL("single="), pContext) &&
		pWrite(XRT_STR_LITERAL("7"), pContext);
}



/* 验证单头文件中的严格文本解析和拥有型字符串输出。 */
int main(void)
{
	static const xrttypeops Ops = {
		.Format = testSingleFormatValue
	};
	xrttype Type = {
		.Id = 0,
		.Kind = XRT_TYPE_RECORD,
		.Flags = XRT_TYPE_FLAG_TRIVIAL_COPY |
			XRT_TYPE_FLAG_TRIVIAL_DROP | XRT_TYPE_FLAG_COPYABLE |
			XRT_TYPE_FLAG_RELOCATABLE | XRT_TYPE_FLAG_FINAL,
		.Name = XRT_STR_INIT("SingleFormat"),
		.AbiName = XRT_STR_INIT("tests.single.Format"),
		.Size = sizeof(int),
		.Align = _Alignof(int),
		.InstanceSize = sizeof(int),
		.InstanceAlign = _Alignof(int),
		.Ops = &Ops
	};
	str sSource = "42";
	int32 iValue = 0;
	int32 iBool32 = 0;
	int iCustom = 7;
	str sBool = "true";
	str sOutput = NULL;
	str sCustom = NULL;
	int iResult = 0;

	Type.Id = xrtTypeId(Type.AbiName);
	sCustom = xrtTypeToString(&Type, &iCustom);
	if (
		!xrtTypeConvert(xrtTypeString(), &sSource,
			xrtTypeInt32(), &iValue, XTYPE_CONVERT_EXPLICIT) ||
		(iValue != 42) ||
		!xrtTypeConvert(xrtTypeString(), &sBool,
			xrtTypeBool32(), &iBool32, XTYPE_CONVERT_EXPLICIT) ||
		(iBool32 != 1) ||
		!xrtTypeConvert(xrtTypeInt32(), &iValue,
			xrtTypeString(), &sOutput, XTYPE_CONVERT_EXPLICIT) ||
		(sOutput == NULL) || (strcmp(sOutput, "42") != 0) ||
		(sCustom == NULL) || (strcmp(sCustom, "single=7") != 0)
	) {
		iResult = 1;
	}
	xrtTypeDropValue(xrtTypeString(), &sOutput);
	xrtFree(sCustom);
	return iResult;
}
