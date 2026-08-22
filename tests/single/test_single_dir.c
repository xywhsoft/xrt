#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立接通目录创建、迭代、空查询和删除。 */
int main(void)
{
	static const char sRoot[] = "xrt-single-dir";
	static const char sFile[] = "xrt-single-dir/item.txt";
	xdir Dir;
	xdirentry Entry;
	bool bEmpty;
	xfile File;
	int iResult = 1;

	(void)xrtFileDelete(sFile);
	(void)xrtDirRemove(sRoot);
	xrtClearError();
	if ( !xrtDirCreate(sRoot) || !xrtDirEmpty(sRoot, &bEmpty) || !bEmpty ) {
		return 1;
	}
	File = xrtOpen(sFile, XFILE_WRITE | XFILE_CREATE | XFILE_EXCLUSIVE);
	if ( File == NULL ) {
		return 1;
	}
	if ( !xrtClose(File) ) {
		return 1;
	}
	Dir = xrtDirOpen(sRoot, XDIR_STAT);
	if ( (Dir != NULL) && (xrtDirNext(Dir, &Entry) == XDIR_NEXT_ITEM) &&
		(Entry.Info.Type == XFILE_TYPE_FILE) && xrtDirClose(Dir) &&
		xrtFileDelete(sFile) && xrtDirRemove(sRoot) ) {
		iResult = 0;
	}
	return iResult;
}
