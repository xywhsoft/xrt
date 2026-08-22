#include <stdio.h>

#include <xrt.h>



/* 使用构建器在一次增长链中组装文本。 */
int main(void)
{
	xstrbuf tBuffer;
	str sResult;

	xrtStrBufInit(&tBuffer);
	if ( !xrtStrBufAppend(&tBuffer, XRT_STR_LITERAL("items=")) ||
		 !xrtStrBufAppendRepeat(&tBuffer, XRT_STR_LITERAL("ab"), 3) ) {
		xrtStrBufFree(&tBuffer);
		return 1;
	}
	sResult = xrtStrBufTake(&tBuffer);
	if ( sResult == NULL ) {
		return 2;
	}
	printf("%s\n", sResult);
	xrtFree(sResult);
	return 0;
}
