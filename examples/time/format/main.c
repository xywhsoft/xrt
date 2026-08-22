#include <xrt.h>

#include <stdio.h>



/* 展示调用方缓冲区和自动分配两种时间格式化路径。 */
int main(void)
{
	char arrText[64];
	xtime iTime;
	str sAllocated;

	if ( !xrtDateTime(2024, 2, 29, 23, 58, 57, 654321, &iTime) ) {
		return 1;
	}
	if ( xrtTimeWrite(arrText, sizeof(arrText), iTime, 8 * 3600,
		XRT_STR_LITERAL("%F %T.%f %:z")) == XRT_NPOS ) {
		return 1;
	}
	printf("buffer: %s\n", arrText);

	sAllocated = xrtTimeFormat(iTime, 0,
		XRT_STR_LITERAL("%A, %B %d, %Y"));
	if ( sAllocated == NULL ) {
		return 1;
	}
	printf("allocated: %s\n", sAllocated);
	xrtFree(sAllocated);
	return 0;
}
