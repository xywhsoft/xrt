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
	xdir Dir;
	xdirentry Entry;
	xdirnext Next;
	bool bFound = false;
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
	if ( !xrtClose(File) ) {
		(void)xrtRootClose(Root);
		(void)xrtRootRemove(Parent, sDirectory);
		(void)xrtRootClose(Parent);
		return 6;
	}
	Dir = xrtRootDirOpen(Root, ".", XDIR_STAT);
	if ( Dir == NULL ) {
		(void)xrtRootRemove(Root, "single.txt");
		(void)xrtRootClose(Root);
		(void)xrtRootRemove(Parent, sDirectory);
		(void)xrtRootClose(Parent);
		return 7;
	}
	while ( (Next = xrtDirNext(Dir, &Entry)) == XDIR_NEXT_ITEM ) {
		if ( xrtStrEqual(Entry.Name, xrtStrView("single.txt")) &&
			 (Entry.Info.Type == XFILE_TYPE_FILE) ) bFound = true;
	}
	if ( (Next != XDIR_NEXT_END) || !xrtDirClose(Dir) || !bFound ||
		 !xrtRootRemove(Root, "single.txt") ||
		 !xrtRootClose(Root) ||
		 !xrtRootRemove(Parent, sDirectory) ||
		 !xrtRootClose(Parent) ) {
		return 8;
	}
	puts("single file root ok");
	return 0;
}
