#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立接通目录树复制、清空和递归删除。 */
int main(void)
{
	static const char sSource[] = "xrt-single-tree-source";
	static const char sTarget[] = "xrt-single-tree-target";
	static const char sFile[] = "xrt-single-tree-source/item.txt";
	static const char sCopied[] = "xrt-single-tree-target/item.txt";
	xfile File;
	bool bEmpty;
	int iResult = 1;

	if ( xrtDirExists(sSource) ) {
		(void)xrtDirRemoveAll(sSource);
	}
	if ( xrtDirExists(sTarget) ) {
		(void)xrtDirRemoveAll(sTarget);
	}
	xrtClearError();
	if ( !xrtDirCreate(sSource) ) {
		return 1;
	}
	File = xrtOpen(sFile, XFILE_WRITE | XFILE_CREATE | XFILE_EXCLUSIVE);
	if ( (File == NULL) || !xrtWriteFull(File, "tree", 4u, NULL) ||
		 !xrtClose(File) ) {
		return 1;
	}
	if ( xrtDirCopy(sSource, sTarget, false) && xrtFileExists(sCopied) &&
		xrtDirClean(sTarget) && xrtDirEmpty(sTarget, &bEmpty) && bEmpty &&
		xrtDirRemoveAll(sTarget) && xrtDirRemoveAll(sSource) ) {
		iResult = 0;
	}
	return iResult;
}
