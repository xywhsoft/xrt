#include <stdio.h>
#include <xruntime.h>



typedef struct examplepoint {
	int X;
	int Y;
} examplepoint;



/* 把格式化分块直接写到标准输出。 */
static bool exampleWriteStdout(xstrview Text, ptr pContext)
{
	(void)pContext;
	return fwrite(Text.Data, 1u, Text.Size, stdout) == Text.Size;
}



/* 使用借用分块格式化自定义点类型，不创建中间字符串。 */
static bool exampleFormatPoint(
	const void* pValue,
	const xrttype* pType,
	xrttypewriter pWrite,
	ptr pContext
)
{
	const examplepoint* pPoint = (const examplepoint*)pValue;
	char sX[32];
	char sY[32];
	size_t iXSize;
	size_t iYSize;

	(void)pType;
	if ( !xrtIntWrite(
		(int64)pPoint->X, 10u, sX, sizeof(sX), &iXSize, 0u
	) || !xrtIntWrite(
		(int64)pPoint->Y, 10u, sY, sizeof(sY), &iYSize, 0u
	) ) {
		return false;
	}
	return pWrite(XRT_STR_LITERAL("Point("), pContext) &&
		pWrite((xstrview){ sX, iXSize }, pContext) &&
		pWrite(XRT_STR_LITERAL(", "), pContext) &&
		pWrite((xstrview){ sY, iYSize }, pContext) &&
		pWrite(XRT_STR_LITERAL(")"), pContext);
}



/* 构造公开 Format 能力的自定义记录描述。 */
static xrttype examplePointType(void)
{
	static const xrttypeops Ops = {
		.Format = exampleFormatPoint
	};
	xrttype Type = {
		.Id = 0,
		.Kind = XRT_TYPE_RECORD,
		.Flags = XRT_TYPE_FLAG_TRIVIAL_COPY |
			XRT_TYPE_FLAG_TRIVIAL_DROP | XRT_TYPE_FLAG_COPYABLE |
			XRT_TYPE_FLAG_RELOCATABLE | XRT_TYPE_FLAG_FINAL,
		.Name = XRT_STR_INIT("Point"),
		.AbiName = XRT_STR_INIT("example.Point"),
		.Size = sizeof(examplepoint),
		.Align = _Alignof(examplepoint),
		.InstanceSize = sizeof(examplepoint),
		.InstanceAlign = _Alignof(examplepoint),
		.Ops = &Ops
	};

	Type.Id = xrtTypeId(Type.AbiName);
	return Type;
}



/* 展示严格解析、拥有型文本和自定义类型流式格式化。 */
int main(void)
{
	xrttype PointType = examplePointType();
	examplepoint Point = { 12, -3 };
	str sInput = "18446744073709551615";
	uint64 iValue = 0u;
	str sOutput = NULL;
	str sPoint = NULL;
	int iResult = 0;

	if (
		!xrtTypeConvert(xrtTypeString(), &sInput,
			xrtTypeUInt64(), &iValue, XTYPE_CONVERT_EXPLICIT) ||
		!xrtTypeConvert(xrtTypeUInt64(), &iValue,
			xrtTypeString(), &sOutput, XTYPE_CONVERT_EXPLICIT) ||
		((sPoint = xrtTypeToString(&PointType, &Point)) == NULL)
	) {
		iResult = 1;
	} else {
		printf("value=%llu text=%s\n",
			(unsigned long long)iValue, sOutput);
		printf("owned=%s\nstream=", sPoint);
		if ( !xrtTypeFormat(
			&PointType, &Point, exampleWriteStdout, NULL
		) ) {
			iResult = 1;
		}
		putchar('\n');
	}
	xrtTypeDropValue(xrtTypeString(), &sOutput);
	xrtFree(sPoint);
	return iResult;
}
