/*
 * XRT Example - String Split
 * XRT 范例 - 字符串分割
 *
 * Description / 说明:
 *   EN: Demonstrates xrtSplit with copy mode and in-place revise mode.
 *   CN: 演示 xrtSplit 的复制模式与原地修改模式。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/string_split.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/string_split -lm -lpthread
 *
 * Note / 注意:
 *   - The returned string array must be freed with xrtFree.
 *   - In revise mode the original buffer will be modified.
 */
#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"


static void PrintParts(str sTitle, str* arrParts, size_t iCount)
{
	size_t i;

	printf("%s (%zu)\n", sTitle, iCount);
	for ( i = 0; i < iCount; i++ ) {
		printf("  [%zu] %s\n", i, arrParts[i]);
	}
}



static void DemoCopyMode(void)
{
	str sSource = "red,green,blue,yellow";
	size_t iCount = 0;
	str* arrParts = xrtSplit(sSource, strlen(sSource), ",", 1, FALSE, &iCount);

	printf("Copy mode source: %s\n", sSource);
	PrintParts("Copy mode result", arrParts, iCount);

	xrtFree(arrParts);
}



static void DemoReviseMode(void)
{
	char sMutable[] = "alpha|beta|gamma|delta";
	size_t iCount = 0;
	str* arrParts = xrtSplit(sMutable, strlen(sMutable), "|", 1, TRUE, &iCount);

	printf("Revise mode original buffer becomes segments in place.\n");
	PrintParts("Revise mode result", arrParts, iCount);

	xrtFree(arrParts);
}



int main()
{
	xrtInit();

	printf("=== String Split ===\n");
	printf("=== 字符串分割 ===\n");

	DemoCopyMode();
	printf("\n");

	DemoReviseMode();

	xrtUnit();
	return 0;
}
