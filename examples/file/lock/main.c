#include <stdio.h>

#include <xrt.h>



/* 展示对整个文件使用非阻塞排他锁。 */
int main(void)
{
	static const char sPath[] = "xrt-file-lock-example.tmp";
	xfile File = xrtOpen(sPath, XFILE_READ | XFILE_WRITE |
		XFILE_CREATE);

	if ( File == NULL ) {
		return 1;
	}
	if ( !xrtFileLock(File, XFILE_LOCK_EXCLUSIVE, false) ) {
		(void)xrtClose(File);
		return 1;
	}
	printf("locked\n");
	if ( !xrtFileUnlock(File) || !xrtClose(File) ) {
		return 1;
	}
	return xrtFileDelete(sPath) ? 0 : 1;
}
