#include <stdio.h>
#include <xruntime.h>



/* 展示无损拓宽和带范围检查的显式转换。 */
int main(void)
{
	int16 iSource = -32000;
	int64 iWide = 0;
	double fSource = 127.75;
	int8 iNarrow = 0;
	bool bSource = true;
	int32 iBool32 = 0;

	if (
		!xrtTypeConvert(xrtTypeInt16(), &iSource,
			xrtTypeInt64(), &iWide, XTYPE_CONVERT_WIDEN) ||
		!xrtTypeConvert(xrtTypeFloat64(), &fSource,
			xrtTypeInt8(), &iNarrow, XTYPE_CONVERT_EXPLICIT) ||
		!xrtTypeConvert(xrtTypeBool(), &bSource,
			xrtTypeBool32(), &iBool32, XTYPE_CONVERT_WIDEN)
	) {
		return 1;
	}
	printf("wide=%lld narrow=%d bool32=%d\n",
		(long long)iWide, (int)iNarrow, (int)iBool32);
	return 0;
}
