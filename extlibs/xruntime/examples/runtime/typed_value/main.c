#include <stdio.h>
#include <xruntime.h>



/* 展示动态值与定宽运行时类型值之间的安全转换。 */
int main(void)
{
	xvalue* pSource = xrtValueInt(120);
	xvalue* pResult;
	int8 iSmall = 0;
	int64 iOutput = 0;

	if ( (pSource == NULL) ||
		 !xrtValueToTyped(pSource, xrtTypeInt8(), &iSmall, NULL) ) {
		xrtValueRelease(pSource);
		return 1;
	}
	pResult = xrtValueFromTyped(xrtTypeInt8(), &iSmall, NULL);
	if ( (pResult == NULL) || !xrtValueGetInt(pResult, &iOutput) ) {
		xrtValueRelease(pResult);
		xrtValueRelease(pSource);
		return 2;
	}
	printf("typed=%d dynamic=%lld\n", (int)iSmall, (long long)iOutput);
	xrtValueRelease(pResult);
	xrtValueRelease(pSource);
	return 0;
}
