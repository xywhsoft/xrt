#include <stdio.h>

#include <xrt.h>



/* 演示零分配视图与独立结果的组合。 */
int main(void)
{
	xstrview Text = xrtStrTrim(XRT_STR_LITERAL("  alpha/beta  "));
	xstrview Name;
	char arrName[32];
	size_t iNameSize;
	str sResult;

	if ( !xrtStrCut(Text, XRT_STR_LITERAL("/"), &Name, NULL) ||
		 !xrtStrFilterTo(Name, XRT_STR_LITERAL("_-"), arrName,
			sizeof(arrName), &iNameSize) ) {
		return 1;
	}
	sResult = xrtStrConcat((xstrview){ arrName, iNameSize },
		XRT_STR_LITERAL(".txt"));
	if ( sResult == NULL ) {
		return 2;
	}
	printf("%s\n", sResult);
	xrtFree(sResult);
	return 0;
}
