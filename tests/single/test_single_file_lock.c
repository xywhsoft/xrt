#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立接通整文件锁和区间锁。 */
int main(void)
{
	static const char sPath[] = "xrt-single-file-lock.tmp";
	xfile File = xrtOpen(sPath, XFILE_READ | XFILE_WRITE |
		XFILE_CREATE | XFILE_TRUNCATE);
	int iResult = 1;

	if ( (File != NULL) &&
		 xrtWriteFull(File, "lock", 4u, NULL) &&
		 xrtFileLockRange(File, XFILE_LOCK_SHARED, 0u, 2u, true) &&
		 xrtFileUnlockRange(File, 0u, 2u) &&
		 xrtFileLock(File, XFILE_LOCK_EXCLUSIVE, true) &&
		 xrtFileUnlock(File) && xrtClose(File) &&
		 xrtFileDelete(sPath) ) {
		iResult = 0;
	}
	return iResult;
}
