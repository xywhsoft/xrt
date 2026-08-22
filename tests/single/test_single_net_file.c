#define XRT_MODULE_NET_FILE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头路径至少保留公开类型、标志和原生打开入口。 */
int main(void)
{
	xfileoptions Options;
	xfile File;

	xrtFileOptionsInit(&Options);
	Options.Flags = XFILE_READ | XFILE_WRITE |
		XFILE_CREATE | XFILE_TRUNCATE;
	File = xrtNetFileOpen("test_single_net_file.tmp", &Options);
	if ( (File == NULL) ||
		 ((xrtFileFlags(File) & XFILE_ASYNC) == 0u) ) {
		return 1;
	}
	if ( !xrtClose(File) ) {
		return 2;
	}
	return xrtFileDelete("test_single_net_file.tmp") ? 0 : 3;
}
