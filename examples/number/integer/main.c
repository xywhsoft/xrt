#include <stdio.h>

#include <xrt.h>



/* 演示严格整数解析和按基数输出。 */
int main(void)
{
	int64 iValue;
	str sDecimal;
	str sHex;

	if ( !xrtIntParse(XRT_STR_LITERAL(" -9_223_372_036_854_775_808 "),
		10, (uint32)XNUMBER_PARSE_SPACE |
		(uint32)XNUMBER_PARSE_SEPARATOR, &iValue) ) {
		return 1;
	}
	sDecimal = xrtIntString(iValue, 10, 0);
	sHex = xrtIntString(iValue, 16,
		(uint32)XNUMBER_PREFIX | (uint32)XNUMBER_UPPER);
	if ( (sDecimal == NULL) || (sHex == NULL) ) {
		xrtFree(sHex);
		xrtFree(sDecimal);
		return 1;
	}
	printf("%s\n%s\n", sDecimal, sHex);
	xrtFree(sHex);
	xrtFree(sDecimal);
	return 0;
}
