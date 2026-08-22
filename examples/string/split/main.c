#include <stdio.h>

#include <xrt.h>



/* 使用零分配迭代器逐项处理分隔文本。 */
int main(void)
{
	xstrsplit tSplit;
	xstrview Item;

	if ( !xrtStrSplitInit(&tSplit, XRT_STR_LITERAL("alpha,beta,,gamma"), XRT_STR_LITERAL(",")) ) {
		return 1;
	}
	while ( xrtStrSplitNext(&tSplit, &Item) ) {
		printf("%.*s\n", (int)Item.Size, Item.Data != NULL ? Item.Data : "");
	}
	return 0;
}
