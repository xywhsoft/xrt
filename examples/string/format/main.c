#include <stdio.h>

#include <xrt.h>



/* 直接把格式化结果追加到已有构建器。 */
int main(void)
{
	xstrbuf tBuffer;
	str sResult;

	xrtStrBufInit(&tBuffer);
	if ( !xrtStrBufAppend(&tBuffer, XRT_STR_LITERAL("result: ")) ||
		 !xrtStrBufAppendFormat(&tBuffer, "%08X / %.2f", 255u, 3.5) ) {
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
