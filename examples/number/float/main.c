#include <stdio.h>

#include <xrt.h>



/* 演示 double 的最短往返写出、严格解析和分配便捷层。 */
int main(void)
{
	double fValue;
	str sText;

	if ( !xrtNumParse(
		XRT_STR_LITERAL(" -1_234.567_890e-2 "),
		(uint32)XNUMBER_PARSE_SPACE |
		(uint32)XNUMBER_PARSE_SEPARATOR,
		&fValue
	) ) {
		return 1;
	}
	sText = xrtNumString(fValue, 0);
	if ( sText == NULL ) {
		return 1;
	}
	printf("%s\n", sText);
	xrtFree(sText);
	return 0;
}
