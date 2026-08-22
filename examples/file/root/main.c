#include <stdio.h>
#include <string.h>

#include <xrt.h>



/* 使用目录能力安全处理不可信的相对文件名。 */
int main(int argc, char** argv)
{
	cstr sName = argc > 1 ? argv[1] : "message.txt";
	const cstr sDirectory = ".xrt-root-example-data";
	xfileoptions Options;
	xfile File;
	xroot Parent;
	xroot Root;

	Parent = xrtRootOpen(".");
	if ( Parent == NULL ) {
		fprintf(stderr, "%s\n", xrtErrorMessage(xrtGetError()));
		return 1;
	}
	if ( !xrtRootDirCreate(Parent, sDirectory, 0700u) ) {
		fprintf(stderr, "%s\n", xrtErrorMessage(xrtGetError()));
		(void)xrtRootClose(Parent);
		return 1;
	}
	Root = xrtRootOpenIn(Parent, sDirectory);
	if ( Root == NULL ) {
		fprintf(stderr, "%s\n", xrtErrorMessage(xrtGetError()));
		(void)xrtRootRemove(Parent, sDirectory);
		(void)xrtRootClose(Parent);
		return 1;
	}
	xrtFileOptionsInit(&Options);
	Options.Flags = XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE;
	File = xrtRootFileOpen(Root, sName, &Options);
	if ( File == NULL ) {
		fprintf(stderr, "%s\n", xrtErrorMessage(xrtGetError()));
		(void)xrtRootClose(Root);
		(void)xrtRootRemove(Parent, sDirectory);
		(void)xrtRootClose(Parent);
		return 1;
	}
	if ( !xrtWriteFull(File, "stored inside root\n", 19u, NULL) ||
		 !xrtClose(File) || !xrtRootRemove(Root, sName) ||
		 !xrtRootClose(Root) ||
		 !xrtRootRemove(Parent, sDirectory) ||
		 !xrtRootClose(Parent) ) {
		fprintf(stderr, "%s\n", xrtErrorMessage(xrtGetError()));
		return 1;
	}
	printf("stored and removed %s/%s\n", sDirectory, sName);
	return 0;
}
