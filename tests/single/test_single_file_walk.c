#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立接通显式栈遍历与统计。 */
int main(void)
{
	static const char sRoot[] = "xrt-single-walk";
	static const char sFile[] = "xrt-single-walk/item.txt";
	xwalkstats Stats;
	xfile File;
	int iResult = 1;

	(void)xrtFileDelete(sFile);
	(void)xrtDirRemove(sRoot);
	xrtClearError();
	if ( !xrtDirCreate(sRoot) ) {
		return 1;
	}
	File = xrtOpen(sFile, XFILE_WRITE | XFILE_CREATE | XFILE_EXCLUSIVE);
	if ( (File == NULL) || !xrtWriteFull(File, "x", 1, NULL) ||
		 !xrtClose(File) ) {
		return 1;
	}
	if ( xrtFileWalk(sRoot, NULL, NULL, NULL, &Stats) &&
		(Stats.Items == 2u) && (Stats.Directories == 1u) &&
		(Stats.Files == 1u) && (Stats.Bytes == 1u) &&
		xrtFileDelete(sFile) && xrtDirRemove(sRoot) ) {
		iResult = 0;
	}
	return iResult;
}
