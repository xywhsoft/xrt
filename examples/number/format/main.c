#include <stdio.h>

#include <xrt.h>



/* 演示整数分组、进制前缀、Unicode 字符和精确浮点展示格式。 */
int main(void)
{
	str sInteger = xrtIntFormat(
		INT64_C(-123456789), XRT_STR_LITERAL(",d"));
	str sHex = xrtUIntFormat(
		UINT64_C(0xDEADBEEF), XRT_STR_LITERAL("#_X"));
	str sFloat = xrtNumFormat(
		1234567.895, XRT_STR_LITERAL(",.2f"));
	str sPercent = xrtNumFormat(
		0.125, XRT_STR_LITERAL(".1%"));
	str sCharacter = xrtIntFormat(
		20320, XRT_STR_LITERAL("c"));

	if ( (sInteger == NULL) || (sHex == NULL) ||
		 (sFloat == NULL) || (sPercent == NULL) ||
		 (sCharacter == NULL) ) {
		xrtFree(sInteger);
		xrtFree(sHex);
		xrtFree(sFloat);
		xrtFree(sPercent);
		xrtFree(sCharacter);
		return 1;
	}
	printf("%s\n%s\n%s\n%s\n%s\n",
		sInteger, sHex, sFloat, sPercent, sCharacter);
	xrtFree(sInteger);
	xrtFree(sHex);
	xrtFree(sFloat);
	xrtFree(sPercent);
	xrtFree(sCharacter);
	return 0;
}
