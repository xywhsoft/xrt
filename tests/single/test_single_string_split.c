#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>



/* 验证单头文件中的字符串拆分模块。 */
int main(void)
{
	xstrlist* pList = xrtStrSplit(XRT_STR_LITERAL("a,b,"), XRT_STR_LITERAL(","));

	if ( (pList == NULL) || (pList->Count != 3) || (pList->Items[2].Size != 0) ) {
		xrtStrListFree(pList);
		return 1;
	}
	xrtStrListFree(pList);
	printf("[PASS] single-string-split\n");
	return 0;
}
