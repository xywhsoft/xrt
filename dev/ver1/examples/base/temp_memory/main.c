/*
 * XRT Example - Temporary Memory
 * XRT 范例 - 临时内存
 *
 * Description / 说明:
 *   EN: Demonstrates xrtTempMemory and xrtFreeTempMemory usage.
 *   CN: 演示 xrtTempMemory 与 xrtFreeTempMemory 的使用方式。
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"


str procMakeGreeting(int iIndex)
{
	str sBuf = (str)xrtTempMemory(64);

	if ( sBuf == NULL ) {
		return xCore.sNull;
	}

	sprintf(sBuf, "hello from temp slot #%d", iIndex);
	return sBuf;
}


str procBuildTempPath(str sBase, str sName)
{
	size_t iNeed = strlen(sBase) + strlen(sName) + 4;
	str sBuf = (str)xrtTempMemory(iNeed);

	if ( sBuf == NULL ) {
		return xCore.sNull;
	}

	sprintf(sBuf, "%s/%s", sBase, sName);
	return sBuf;
}


void procTestBasicUse(void)
{
	str arrText[5] = { 0 };
	int i = 0;

	printf("=== Basic Use ===\n");
	printf("=== 基本用法 ===\n");

	for ( i = 0; i < 5; i++ ) {
		arrText[i] = procMakeGreeting(i + 1);
		printf("[%d] %s (%p)\n", i, arrText[i], (void*)arrText[i]);
	}

	printf("\n");
}


void procTestWrapAround(void)
{
	ptr arrPtr[35] = { 0 };
	int i = 0;

	printf("=== Wrap Around ===\n");
	printf("=== 回绕行为 ===\n");

	for ( i = 0; i < 35; i++ ) {
		str sBuf = (str)xrtTempMemory(32);

		if ( sBuf == NULL ) {
			break;
		}

		sprintf(sBuf, "item-%d", i + 1);
		arrPtr[i] = sBuf;
	}

	printf("ptr[0]  = %p\n", arrPtr[0]);
	printf("ptr[31] = %p\n", arrPtr[31]);
	printf("ptr[32] = %p\n", arrPtr[32]);
	printf("ptr[34] = %p\n", arrPtr[34]);

	if ( arrPtr[0] == arrPtr[32] ) {
		printf("slot reuse observed after 32 allocations\n");
		printf("在 32 次分配后观察到槽位复用\n");
	} else {
		printf("pointer reuse is allocator-dependent, old temp pointers still should not be kept long-term\n");
		printf("指针是否复用取决于分配器实现，但旧的临时指针都不应长期保存\n");
	}

	printf("\n");
}


void procTestManualCleanup(void)
{
	str sBefore = NULL;
	str sAfter = NULL;

	printf("=== Manual Cleanup ===\n");
	printf("=== 手动清理 ===\n");

	sBefore = (str)xrtTempMemory(64);
	sprintf(sBefore, "temporary content before cleanup");
	printf("before cleanup: %s\n", sBefore);

	xrtFreeTempMemory();

	sAfter = (str)xrtTempMemory(64);
	sprintf(sAfter, "temporary content after cleanup");
	printf("after cleanup : %s\n\n", sAfter);
}


void procTestPathBuilding(void)
{
	str sPath1 = NULL;
	str sPath2 = NULL;
	str sPath3 = NULL;

	printf("=== Practical Case ===\n");
	printf("=== 实际场景 ===\n");

	sPath1 = procBuildTempPath("/tmp/demo", "a.txt");
	sPath2 = procBuildTempPath("/tmp/demo", "b.txt");
	sPath3 = procBuildTempPath("/tmp/demo", "c.txt");

	printf("path1 = %s\n", sPath1);
	printf("path2 = %s\n", sPath2);
	printf("path3 = %s\n\n", sPath3);
}


int main(void)
{
	xrtInit();

	printf("XRT Base - Temporary Memory Demo\n");
	printf("XRT 基础 - 临时内存演示\n");
	printf("================================\n\n");

	procTestBasicUse();
	procTestWrapAround();
	procTestManualCleanup();
	procTestPathBuilding();

	xrtFreeTempMemory();
	xrtUnit();
	return 0;
}
