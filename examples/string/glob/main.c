#include <stdio.h>

#include <xrt.h>



/* 演示严格 UTF-8 文件名通配和 ASCII 大小写忽略。 */
int main(void)
{
	xstrview Name = XRT_STR_LITERAL("Report-你.TXT");
	xstrview Pattern = XRT_STR_LITERAL("report-?.[tT][xX][tT]");

	printf("%s\n", xrtStrGlob(Name, Pattern, XSTR_GLOB_CASE_ASCII) ?
		"matched" : "not matched");
	return 0;
}
