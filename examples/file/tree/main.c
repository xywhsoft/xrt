#include <stdio.h>

#include <xrt.h>



/* 展示常用目录树复制和递归清理路径。 */
int main(void)
{
	static const char sSource[] = "xrt-tree-example-source";
	static const char sTarget[] = "xrt-tree-example-target";
	static const char sFile[] = "xrt-tree-example-source/item.txt";
	xfile File;
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
	if ( (File != NULL) && xrtWriteFull(File, "example", 7u, NULL) &&
		xrtClose(File) && xrtDirCopy(sSource, sTarget, false) ) {
		printf("copied %s to %s\n", sSource, sTarget);
		iResult = 0;
	}
	(void)xrtDirRemoveAll(sTarget);
	(void)xrtDirRemoveAll(sSource);
	return iResult;
}
