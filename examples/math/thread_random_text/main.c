#include <stdio.h>

#include <xrt.h>



/* 演示当前线程便捷随机文本。 */
int main(void)
{
	str sText = xrtRandStringFrom(XRT_STR_LITERAL("abcdef0123456789"), 24);

	if ( sText == NULL ) {
		return 1;
	}
	printf("%s\n", sText);
	xrtFree(sText);
	return 0;
}
