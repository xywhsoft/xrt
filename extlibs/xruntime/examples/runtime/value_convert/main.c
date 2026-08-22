#include <stdio.h>
#include <xruntime.h>



/* 展示动态 Value 到定宽运行时类型的显式转换。 */
int main(void)
{
	xvalue* pSource = xrtValueInt(120);
	int8 iValue = 0;

	if ( (pSource == NULL) || !xrtValueConvertTo(
		pSource, xrtTypeInt8(), &iValue, XTYPE_CONVERT_EXPLICIT
	) ) {
		xrtValueRelease(pSource);
		return 1;
	}
	printf("value=%d\n", (int)iValue);
	xrtValueRelease(pSource);
	return 0;
}
