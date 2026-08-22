#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>
#include <string.h>



/* 单头文件必须完整提供受限目录根和根内文件访问。 */
int main(void)
{
	char sDirectory[96];
	xfileoptions Options;
	xfile File;
	xroot Parent;
	xroot Root;

	if ( snprintf(sDirectory, sizeof(sDirectory),
		".xrt-single-root-%lld", (long long)xrtNow()) <= 0 ) {
		return 1;
	}
	Parent = xrtRootOpen(".");
	if ( Parent == NULL ) {
		return 2;
	}
	if ( !xrtRootRemove(Parent, sDirectory) ) {
		xrtClearError();
	}
	if ( !xrtRootDirCreate(Parent, sDirectory, 0700u) ) {
		(void)xrtRootClose(Parent);
		return 3;
	}
	Root = xrtRootOpenIn(Parent, sDirectory);
	if ( Root == NULL ) {
		(void)xrtRootRemove(Parent, sDirectory);
		(void)xrtRootClose(Parent);
		return 4;
	}
	xrtFileOptionsInit(&Options);
	Options.Flags = XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE;
	File = xrtRootFileOpen(Root, "single.txt", &Options);
	if ( (File == NULL) || !xrtWriteFull(File, "single", 6u, NULL) ) {
		if ( File != NULL ) {
			(void)xrtClose(File);
		}
		(void)xrtRootClose(Root);
		(void)xrtRootRemove(Parent, sDirectory);
		(void)xrtRootClose(Parent);
		return 5;
	}
	if ( !xrtClose(File) || !xrtRootRemove(Root, "single.txt") ||
		 !xrtRootClose(Root) ||
		 !xrtRootRemove(Parent, sDirectory) ||
		 !xrtRootClose(Parent) ) {
		return 6;
	}
	puts("single file root ok");
	return 0;
}
